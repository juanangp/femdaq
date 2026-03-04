
#include "FEMDAQARCFEM.h"
#include "ARCPacket.h"
#include "FEMINOSPacket.h"

#include <filesystem>
#include <future>

std::atomic<bool> FEMDAQARCFEM::stopReceiver(false);
std::atomic<bool> FEMDAQARCFEM::stopEventBuilder(false);

FEMDAQARCFEM::Registrar::Registrar() {
  FEMDAQ::RegisterType("ARC", [](RunConfig &cfg) {
    return std::make_unique<FEMDAQARCFEM>(cfg);
  });

  FEMDAQ::RegisterType("FEMINOS", [](RunConfig &cfg) {
    return std::make_unique<FEMDAQARCFEM>(cfg);
  });
}

FEMDAQARCFEM::Registrar FEMDAQARCFEM::registrar_;

FEMDAQARCFEM::FEMDAQARCFEM(RunConfig &rC) : FEMDAQ(rC) {

  if (runConfig.electronics == "ARC") {
    packetAPI.isMFrame = &ARCPacket::isMFrame;
    packetAPI.isDataFrame = &ARCPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &ARCPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &ARCPacket::TryExtractNextEvent;
    packetAPI.DataPacket_Print = &ARCPacket::DataPacket_Print;
  } else if (runConfig.electronics == "FEMINOS") {
    packetAPI.isMFrame = &FEMINOSPacket::isMFrame;
    packetAPI.isDataFrame = &FEMINOSPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &FEMINOSPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &FEMINOSPacket::TryExtractNextEvent;
    packetAPI.DataPacket_Print = &FEMINOSPacket::DataPacket_Print;
  } else {
    throw std::runtime_error("Unknown electronics type in config!");
  }

  std::vector<std::thread> threads;

  for (auto &FEM : FEMArray) {
    receiverThreads.emplace_back(&FEMDAQARCFEM::FEMReceiverThread, this,
                                 std::ref(FEM));
  }
}

FEMDAQARCFEM::~FEMDAQARCFEM() {

  stopReceiver = true;
  for (auto &t : receiverThreads) {
    if (t.joinable())
      t.join();
  }
}

void FEMDAQARCFEM::SendCommand(const char *cmd, bool wait) {
  std::vector<std::future<void>> futures;

  for (auto &FEM : FEMArray) {
    if (FEM.active) {
      futures.push_back(
          std::async(std::launch::async, [this, cmd, &FEM, wait]() {
            this->SendCommand(cmd, FEM, wait);
          }));
    }
  }

  try {
    for (auto &f : futures) {
      f.get();
    }
  } catch (const std::runtime_error &e) {
    stopReceiver = true;
    std::cout << "[CRITICAL] Command failed: " << e.what() << std::endl;
    throw;
  }
}

void FEMDAQARCFEM::waitForCmd(FEMProxy &FEM) {

  auto start = std::chrono::steady_clock::now();

  while (FEM.cmd_rcv < FEM.cmd_sent) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto now = std::chrono::steady_clock::now();
    // 10 seconds timeout
    if (std::chrono::duration_cast<std::chrono::seconds>(now - start).count() >
        10) {
      throw std::runtime_error("Timeout waiting for FEM " +
                               std::to_string(FEM.femID) + " response");
    }
    if (stopReceiver)
      break;
  }
}

void FEMDAQARCFEM::FEMReceiverThread(FEMProxy &FEM) {

  uint16_t buf_rcv[8192 / sizeof(uint16_t)];
  const size_t offset = (runConfig.electronics == "FEMINOS") ? 1 : 0;

  while (!stopReceiver) {

    int length = recvfrom(FEM.client, buf_rcv, 8192, 0,
                          (struct sockaddr *)&FEM.remote, &FEM.remote_size);

    if (length < 0) {
      if (stopReceiver)
        break;
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        continue;
      } else {
        std::string error = "FEM" + std::to_string(FEM.femID) +
                            " recvfrom failed: " + std::string(strerror(errno));
        throw std::runtime_error(error);
      }
    }

    if (length <= 6)
      continue;
    const size_t size = length / sizeof(uint16_t);

    // --- Lógica de Procesamiento ---
    if (packetAPI.isDataFrame(&buf_rcv[1])) {
      // const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
      FEM.mutex_mem.lock();
      FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size - offset]);
      const size_t bufferSize = FEM.buffer.size();
      FEM.mutex_mem.unlock();
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug) {
        // packetAPI.DataPacket_Print(&buf_rcv[1], size-1);
        std::cout << "FEM " << FEM.femID << " Packet buffered with size "
                  << (int)size - 1 << " queue size: " << bufferSize
                  << std::endl;
      }
    } else if (packetAPI.isMFrame(&buf_rcv[1])) {
      FEM.cmd_rcv++;
      PrintMonitoring(&buf_rcv[1], size - 1, FEM);
    } else {
      const short errorCode = buf_rcv[2];
      if (runConfig.verboseLevel > RunConfig::Verbosity::Info || errorCode) {
        if (errorCode)
          std::cout << "---------------------ERROR----------------"
                    << std::endl;
        else
          std::cout << "FEM " << FEM.femID << " DEBUG PACKET REPLY"
                    << std::endl;

        packetAPI.DataPacket_Print(&buf_rcv[1], size - 1, stdout);
      }
      FEM.cmd_rcv++;
    }
  }
}

void FEMDAQARCFEM::startDAQ(const std::vector<std::string> &flags) {

  stopEventBuilder = false;
  stopRun = false;
  storedEvents = 0;
  runStartTime = getCurrentTime();
  eventBuilderThread = std::thread(&FEMDAQARCFEM::EventBuilder, this);
  std::this_thread::sleep_for(std::chrono::seconds(1));
  char daq_cmd[40];
  // First daq request, do not add sequence number
  const uint32_t reqCmd = 0xFF;
  sprintf(daq_cmd, "daq 0x%06x F\n", reqCmd);

  while (!abrt && !stopRun) {
    SendCommand(daq_cmd, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

void FEMDAQARCFEM::stopDAQ() {

  SendCommand("daq 0x000000 F\n", false);

  SendCommand("sca enable 0");
  runEndTime = getCurrentTime();
  std::this_thread::sleep_for(std::chrono::seconds(1));

  stopEventBuilder = true;
  if (eventBuilderThread.joinable())
    eventBuilderThread.join();
}

void FEMDAQARCFEM::PrintMonitoring(uint16_t *buff, const uint16_t &size,
                                   FEMProxy &FEM) {

  if (fileRoot) {
    char *ptr;
    size_t size_mem;
    FILE *mem_fp = open_memstream(&ptr, &size_mem);

    fprintf(mem_fp, "FEM %u MONI PACKET\n", FEM.femID);
    auto tmstmp = GetTimeStampFromUnixTime(getCurrentTime());
    fprintf(mem_fp, "TIME %s\n", tmstmp);
    packetAPI.DataPacket_Print(buff, size, mem_fp);

    fclose(mem_fp);

    std::fwrite(ptr, 1, size_mem, stdout);
    FEM.monitoringLog.append(ptr, size_mem);

    if (runConfig.verboseLevel >= RunConfig::Verbosity::Info) {
      std::fwrite(ptr, 1, size_mem, stdout);
      std::fflush(stdout);
    }
    free(ptr);
  } else if (runConfig.verboseLevel >= RunConfig::Verbosity::Info) {
    packetAPI.DataPacket_Print(buff, size, stdout);
  }
}

void FEMDAQARCFEM::EventBuilder() {

  SignalEvent sEvent;
  double lastTimeSaved = runStartTime;
  uint32_t prevEvCount = 0;
  double prevEventTime = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0x0;
  uint64_t fileSize = 0;

  int fileIndex = 1;
  const std::string baseName = MakeBaseFileName();
  std::string fileName = MakeFileName(baseName, fileIndex);

  if (!runConfig.readOnly) {
    OpenRootFile(fileName, sEvent, getCurrentTime());
  }

  bool newEvent = true;
  bool emptyBuffer = true;
  int tC = 0;

  while (!(emptyBuffer && stopEventBuilder)) {
    emptyBuffer = true;
    newEvent = true;

    for (auto &FEM : FEMArray) {
      std::deque<uint16_t> eventBuffer;
      FEM.mutex_mem.lock();
      emptyBuffer &= FEM.buffer.empty();
      if (!FEM.buffer.empty()) {
        if (FEM.pendingEvent) { // Wait till we reach end of event for all the
                                // ARC
          FEM.pendingEvent = !packetAPI.TryExtractNextEvent(
              FEM.buffer, FEM.bufferIndex, eventBuffer);
        }
      }
      FEM.mutex_mem.unlock();
      if (!FEM.pendingEvent)
        packetAPI.ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
      newEvent &= !FEM.pendingEvent; // Check if the event is pending
    }

    if (newEvent) { // Save Event if closed
      sEvent.eventID = ev_count;
      sEvent.timestamp = (double)ts * 2.E-8 + runStartTime;
      UpdateRun(sEvent.timestamp, prevEventTime, storedEvents, prevEvCount);

      if (fileRoot) {
        FillTree(sEvent.timestamp, lastTimeSaved);

        if (storedEvents % 100 == 0) {
          // std::cout<<"File size "<<std::filesystem::file_size(fileName)<<"
          // "<<runConfig.fileSize<<std::endl;
          fileSize = std::filesystem::file_size(fileName);
        }

        if (fileSize >= runConfig.fileSize) {

          CloseRootFile(sEvent.timestamp);

          fileIndex++;
          fileSize = 0;

          // Create new writer + new model (must be recreated!)
          fileName = MakeFileName(baseName, fileIndex);

          std::cout << "New file " << fileName << " " << std::endl;

          OpenRootFile(fileName, sEvent, sEvent.timestamp);
        }
      }

      for (auto &FEM : FEMArray)
        FEM.pendingEvent = true;

      ++storedEvents;
      sEvent.Clear();
    }

    if (stopEventBuilder) {
      for (auto &FEM : FEMArray)
        if (tC % 1000 == 0) {
          if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
            std::cout << "FEM " << FEM.femID << " Buffer size "
                      << FEM.buffer.size() << std::endl;
        }
      tC++;
      if (tC >= 1000)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  for (auto &FEM : FEMArray)
    if (FEM.buffer.size() > 0)
      std::cout << "FEM " << FEM.femID << " Buffer size left "
                << FEM.buffer.size() << std::endl;

  if (fileRoot) {
    CloseRootFile(runEndTime);
  }

  std::cout << "End of event builder " << storedEvents << " events acquired"
            << std::endl;
}


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
    packetAPI.ConfigPacket_Print = &ARCPacket::ConfigPacket_Print;
  } else if (runConfig.electronics == "FEMINOS") {
    packetAPI.isMFrame = &FEMINOSPacket::isMFrame;
    packetAPI.isDataFrame = &FEMINOSPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &FEMINOSPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &FEMINOSPacket::TryExtractNextEvent;
    packetAPI.DataPacket_Print = &FEMINOSPacket::DataPacket_Print;
    packetAPI.ConfigPacket_Print = &FEMINOSPacket::ConfigPacket_Print;
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

void FEMDAQARCFEM::SendDAQCmdThread(FEMProxy &FEM) {

  char daq_cmd[40];
  FEM.tmpBuffer.clear();
  FEM.daq_credit = 0;
  const uint32_t reqFrames = 272 * 20;
  uint8_t seq = 0x00;
  const uint32_t frameThr = 272;
  // First daq request, do not add sequence number
  sprintf(daq_cmd, "daq 0x%06x F\n", reqFrames);
  SendCommand(daq_cmd, FEM, false);

  while (!abrt && !stopRun) {
    if (FEM.daq_credit >= frameThr) {
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
        std::cout << "FEM " << FEM.femID << " Daq frames credits "
                  << FEM.daq_credit << " left " << reqFrames - FEM.daq_credit
                  << std::endl;
      sprintf(daq_cmd, "daq 0x%06x F 0x%02x\n", frameThr, seq);
      SendCommand(daq_cmd, FEM, false);
      FEM.daq_credit -= frameThr;
      seq++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  SendCommand("daq 0x000000 F\n", FEM, false);
}

void FEMDAQARCFEM::SendCommand(const char *cmd) {
  std::vector<std::future<void>> futures;

  for (auto &FEM : FEMArray) {
    if (FEM.active) {
      futures.push_back(std::async(std::launch::async, [this, cmd, &FEM]() {
        this->SendCommand(cmd, FEM);
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

void FEMDAQARCFEM::SendCommand(const char *cmd, FEMProxy &FEM, bool wait) {

  const int e =
      sendto(FEM.client, cmd, strlen(cmd), 0, (struct sockaddr *)&(FEM.target),
             sizeof(struct sockaddr));
  if (e == -1) {
    std::string error = "sendto failed: " + std::string(strerror(errno));
    throw std::runtime_error(error);
  }

  if (!wait)
    return;

  if (FEM.logFile != nullptr &&
      runConfig.verboseLevel >= RunConfig::Verbosity::Info) {
    fprintf(FEM.logFile, ">> FEM %u Cmd sent %s\n", FEM.femID, cmd);
  }

  FEM.cmd_sent++;
  waitForCmd(FEM);
}

void FEMDAQARCFEM::waitForCmd(FEMProxy &FEM) {

  auto start = std::chrono::steady_clock::now();

  while (FEM.cmd_rcv < FEM.cmd_sent) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));

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

    const size_t size = length / sizeof(uint16_t);

    // if (length <= 6){
    //   if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
    //     packetAPI.DataPacket_Print(&buf_rcv[1], size - 1, stdout);
    //   continue;
    // }

    if (packetAPI.isDataFrame(&buf_rcv[1])) {
      // const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
      FEM.daq_credit++;
      FEM.mutex_mem.lock();
      FEM.tmpBuffer.insert(FEM.tmpBuffer.end(), &buf_rcv[1],
                           &buf_rcv[size - offset]);
      FEM.mutex_mem.unlock();
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug) {
        PrintMonitoring(&buf_rcv[1], size - 1, FEM);
        std::cout << "FEM " << FEM.femID << " Packet buffered with size "
                  << (int)size - offset << std::endl;
      }
    } else {
      PrintMonitoring(&buf_rcv[1], size - 1, FEM);
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
  UpdateRunThread = std::thread(&FEMDAQ::UpdateThread, this);

  std::cout << GetTimeStampFromUnixTime(runStartTime)
            << " Starting data taking " << fileNameRoot << " " << std::endl;

  std::vector<std::future<void>> futures;

  for (auto &FEM : FEMArray) {
    if (FEM.active) {
      futures.push_back(std::async(
          std::launch::async, [this, &FEM]() { this->SendDAQCmdThread(FEM); }));
    }
  }

  try {
    for (auto &f : futures) {
      f.get();
    }
  } catch (const std::runtime_error &e) {
    stopReceiver = true;
    std::cout << "[CRITICAL] DAQ command failed: " << e.what() << std::endl;
    throw;
  }
}

void FEMDAQARCFEM::stopDAQ() {

  runEndTime = getCurrentTime();

  if (UpdateRunThread.joinable())
    UpdateRunThread.join();

  stopEventBuilder = true;
  if (eventBuilderThread.joinable())
    eventBuilderThread.join();
}

void FEMDAQARCFEM::PrintMonitoring(uint16_t *buff, const uint16_t &size,
                                   FEMProxy &FEM) {
  char *ptr = nullptr;
  size_t size_mem = 0;

  FILE *mem_fp = open_memstream(&ptr, &size_mem);
  if (!mem_fp)
    return;

  std::string tmstmp = GetTimeStampFromUnixTime(getCurrentTime());
  if (packetAPI.isMFrame(&buff[0])) {
    fprintf(mem_fp, "--- FEM %u MONI PACKET | %s ---\n", FEM.femID,
            tmstmp.c_str());
    packetAPI.DataPacket_Print(buff, size, mem_fp);
    fprintf(mem_fp, "\n");
  } else if (packetAPI.isDataFrame(&buff[0])) {
    fprintf(mem_fp, "--- FEM %u DATA PACKET | %s ---\n", FEM.femID,
            tmstmp.c_str());
    packetAPI.DataPacket_Print(buff, size, mem_fp);
    fprintf(mem_fp, "\n");
  } else {
    const short errorCode = buff[1];
    if (errorCode)
      fprintf(mem_fp, "-------- FEM %u ERROR--------", FEM.femID);
    if (runConfig.verboseLevel > RunConfig::Verbosity::Info) {
      fprintf(mem_fp, "--- FEM %u CONFIG PACKET | %s ---\n", FEM.femID,
              tmstmp.c_str());
      packetAPI.DataPacket_Print(buff, size, mem_fp);
    } else {
      packetAPI.ConfigPacket_Print(buff, size, mem_fp);
    }
    fprintf(mem_fp, "\n");
  }

  fclose(mem_fp);

  if (ptr) {
    if (FEM.logFile) {
      std::fwrite(ptr, 1, size_mem, FEM.logFile);
      std::fflush(FEM.logFile);
    }

    if (runConfig.verboseLevel >= RunConfig::Verbosity::Info) {
      std::fwrite(ptr, 1, size_mem, stdout);
    }

    free(ptr);
  }
}

void FEMDAQARCFEM::EventBuilder() {

  double lastTimeSaved = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0x0;

  if (fileRoot)
    WriteRunStartTime(runStartTime);

  bool newEvent = true;
  bool emptyBuffer = true;
  int tC = 0;
  std::vector<uint16_t> eventBuffer;
  eventBuffer.reserve(72 * 4 * 600);

  // Initialize FEMs
  for (auto &FEM : FEMArray) {
    FEM.bufferIndex = 0;
    FEM.pendingEvent = true;
    FEM.buffer.clear();
  }

  while (!(emptyBuffer && stopEventBuilder)) {
    emptyBuffer = true;
    newEvent = true;

    for (auto &FEM : FEMArray) {
      FEM.mutex_mem.lock();
      if (!FEM.tmpBuffer.empty()) {
        // Data is moved from tmp buffer to avoid lock
        FEM.buffer.insert(FEM.buffer.end(),
                          std::make_move_iterator(FEM.tmpBuffer.begin()),
                          std::make_move_iterator(FEM.tmpBuffer.end()));
        FEM.tmpBuffer.clear();
      }
      FEM.mutex_mem.unlock();

      emptyBuffer &= FEM.buffer.empty();

      if (!FEM.buffer.empty() && FEM.pendingEvent) {
        FEM.pendingEvent = !packetAPI.TryExtractNextEvent(
            FEM.buffer, FEM.bufferIndex, eventBuffer);
      }

      if (!FEM.pendingEvent)
        packetAPI.ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
      newEvent &= !FEM.pendingEvent; // Check if the event is pending
      eventBuffer.clear();
    }

    if (newEvent) { // Save Event if closed
      sEvent.eventID = ev_count;
      sEvent.timestamp = (double)ts * 1.E-8 + runStartTime;

      if (storedEvents == 0) {
        std::cout << "Start time: " << GetTimeStampFromUnixTime(runStartTime)
                  << " Last ev tS: "
                  << GetTimeStampFromUnixTime(sEvent.timestamp) << " "
                  << sEvent.timestamp - runStartTime << std::endl;
      }

      if (fileRoot && !sEvent.signalsID.empty()) {
        FillTree(sEvent.timestamp, lastTimeSaved);

        if (storedEvents % 100 == 0) {
          CheckFileSize(sEvent.timestamp);
        }
      }

      for (auto &FEM : FEMArray)
        FEM.pendingEvent = true;

      if (!sEvent.signalsID.empty()) {
        ++storedEvents;
        sEvent.Clear();
      }
    }

    if (stopEventBuilder) {
      for (auto &FEM : FEMArray)
        if (tC % 100 == 0) {
          if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
            std::cout << "FEM " << FEM.femID << " Buffer size "
                      << FEM.buffer.size() << std::endl;
        }
      tC++;
      if (tC >= 500)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  std::cout << "End of event builder " << storedEvents << " events acquired in "
            << runEndTime - runStartTime << " s"
            << " Avg rate: " << storedEvents / (runEndTime - runStartTime)
            << " Hz" << std::endl;

  std::cout << "End time: " << GetTimeStampFromUnixTime(runEndTime)
            << " Last evtS: " << GetTimeStampFromUnixTime(sEvent.timestamp)
            << " " << runEndTime - sEvent.timestamp << std::endl;

  for (auto &FEM : FEMArray)
    if (!FEM.buffer.empty()) {
      std::cout << "FEM " << FEM.femID << " Buffer size left "
                << FEM.buffer.size() << std::endl;
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug) {
        if (FEM.logFile)
          fprintf(FEM.logFile, "------DEBUG BUFFER FRAMES LEFT-------\n");
        PrintMonitoring(&FEM.buffer[0], FEM.buffer.size(), FEM);
      }
    }

  if (fileRoot) {
    if (sEvent.timestamp > runEndTime)
      runEndTime = sEvent.timestamp;

    WriteRunEndTime(runEndTime);
  }
}

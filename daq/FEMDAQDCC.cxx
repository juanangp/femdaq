
#include "FEMDAQDCC.h"

#include <filesystem>
#include <sstream>

FEMDAQDCC::Registrar::Registrar() {
  FEMDAQ::RegisterType(
      "DCC", [](RunConfig &cfg) { return std::make_unique<FEMDAQDCC>(cfg); });
}

FEMDAQDCC::Registrar FEMDAQDCC::registrar_;

FEMDAQDCC::FEMDAQDCC(RunConfig &rC) : FEMDAQ(rC) {}

FEMDAQDCC::~FEMDAQDCC() {}

void FEMDAQDCC::Pedestals(const std::vector<std::string> &flags) {

  // Default values
  int nTriggers = 100;
  int mean = 250;
  double stdDev = 5.0;

  //// Flags processing (e.g. "nTriggers=100","mean=250", "stdDev=5.0")
  for (size_t i = 0; i < flags.size(); ++i) {
    const std::string &arg = flags[i];

    if (arg.find("nTriggers=") == 0) {
      nTriggers = std::stoi(arg.substr(10));
    } else if (arg.find("mean=") == 0) {
      mean = std::stoi(arg.substr(5));
    } else if (arg.find("stdDev=") == 0) {
      stdDev = std::stod(arg.substr(7));
    } else {
      std::cout << "Unsupported flag " << arg << " doing nothing!!!"
                << std::endl;
      std::cout << "Supported flags are: nTriggers=xxx, mean=yyy, stdDev=z.z"
                << std::endl;
    }
  }

  std::cout << "Starting pedestal run: nTriggers=" << nTriggers
            << " mean=" << mean << " stdDev=" << stdDev << std::endl;

  auto &FEM = FEMArray.front();
  const auto &fecs = runConfig.fems.front().fecs;
  char cmd[200];

  // Pedestal acquisition
  for (int i = 0; i < nTriggers; i++) {
    SendCommand("isobus 0x6C", FEM); // SCA start
    SendCommand("isobus 0x1C", FEM); // SCA stop
    waitForTrigger();
    for (auto fecID : fecs) {
      for (int a = 0; a < 4; a++) {
        sprintf(cmd, "hped acc %d %d %d:%d", fecID, a, 3, 78);
        SendCommand(cmd, FEM);
      }
    }
  }

  SendCommand("fem 0");

  for (auto fecID : fecs) {
    for (int a = 0; a < 4; a++) {
      sprintf(cmd, "hped getsummary %d %d %d:%d", fecID, a, 3, 78);
      SendCommand(cmd, FEM, 1); // Get summary
      sprintf(cmd, "hped centermean %d %d %d:%d %d", fecID, a, 3, 78, mean);
      SendCommand(cmd, FEM); // Set mean
      sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fecID, a, 3, 78, mean,
              stdDev);
      SendCommand(cmd, FEM); // Set thr
    }
  }
}

void FEMDAQDCC::startDAQ(const std::vector<std::string> &flags) {

  int mode = 0; // Mode
  bool internal = false;

  for (const auto &flag : flags) {
    if (flag == "internal")
      internal = true; // Internal (random) trigger, otherwise external
                       // (default)
    else if (flag == "ZS")
      mode = 1; // Zero suppression
    else
      std::cout
          << "Unknown flag " << flag
          << " supported flags are internal (trigger) or ZS (Zero suppression)"
          << std::endl;
  }

  runStartTime = getCurrentTime();
  stopRun = false;
  storedEvents = 0;

  stopEventBuilder = false;
  eventBuilderThread = std::thread(&FEMDAQDCC::EventBuilder, this);
  UpdateRunThread = std::thread(&FEMDAQ::UpdateThread, this);

  auto &FEM = FEMArray.front();
  FEM.tmpBuffer.clear();
  const auto &fecs = runConfig.fems.front().fecs;
  char cmd[200];

  std::cout << GetTimeStampFromUnixTime(runStartTime)
            << " Starting data taking ";
  if (fileRoot)
    std::cout << fileNameRoot;
  std::cout << std::endl;

  auto rS = std::chrono::high_resolution_clock::now();

  while (!stopRun) {
    // SendCommand("fem 0");

    SendCommand("isobus 0x64", FEM); // SCA start
    if (internal)
      SendCommand("isobus 0x14", FEM);
    waitForTrigger();
    if (abrt)
      break;
    // Perform data acquisition phase, compress, accept size
    auto now = std::chrono::high_resolution_clock::now();
    for (auto fecID : fecs) {
      for (int a = 0; a < 4; a++) {
        sprintf(cmd, "areq %d %d %d %d %d", mode, fecID, a, 3, 78);
        SendCommand(cmd, FEM, 0);
      }
    }

    auto elapsed =
        std::chrono::duration_cast<std::chrono::nanoseconds>(now - rS);

    uint64_t ns10 = static_cast<uint64_t>(elapsed.count() / 10) &
                    0xFFFFFFFFFFFFULL; // 48 bits mask

    uint16_t t1 = static_cast<uint16_t>((ns10 >> 32) & 0xFFFF); // Bits 47-32
    uint16_t t2 = static_cast<uint16_t>((ns10 >> 16) & 0xFFFF); // Bits 31-16
    uint16_t t3 = static_cast<uint16_t>(ns10 & 0xFFFF);         // Bits 15-0

    FEM.mutex_mem.lock();
    FEM.tmpBuffer.emplace_back(0xFFFF); // Insert word to mark EOE
    FEM.tmpBuffer.emplace_back(t1);     // Insert timeStamp 48 bits
    FEM.tmpBuffer.emplace_back(t2);
    FEM.tmpBuffer.emplace_back(t3);
    FEM.mutex_mem.unlock();
  }
}

void FEMDAQDCC::EventBuilder() {

  auto &FEM = FEMArray.front();

  double lastTimeSaved = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0x0;

  if (fileRoot)
    WriteRunStartTime(runStartTime);

  bool newEvent = false;
  bool emptyBuffer = true;
  int tC = 0;

  std::vector<uint16_t> eventBuffer;
  eventBuffer.reserve(6 * 72 * 4 * 600);

  FEM.bufferIndex = 0;
  FEM.buffer.clear();

  std::cout << "Start of event builder" << std::endl;

  while (!(emptyBuffer && stopEventBuilder)) {

    FEM.mutex_mem.lock();
    if (!FEM.tmpBuffer.empty()) {
      FEM.buffer.insert(FEM.buffer.end(),
                        std::make_move_iterator(FEM.tmpBuffer.begin()),
                        std::make_move_iterator(FEM.tmpBuffer.end()));
      FEM.tmpBuffer.clear();
    }
    FEM.mutex_mem.unlock();

    emptyBuffer = FEM.buffer.empty();

    if (!emptyBuffer) {
      newEvent = DCCPacket::TryExtractNextEvent(FEM.buffer, FEM.bufferIndex,
                                                eventBuffer);
    }

    if (newEvent) {
      DCCPacket::ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
      eventBuffer.clear();
      sEvent.eventID = ev_count;
      sEvent.timestamp = (double)ts * 1.E-8 + runStartTime;

      // Do not fill empty events
      if (fileRoot && !sEvent.signalsID.empty()) {
        FillTree(sEvent.timestamp, lastTimeSaved);

        if (storedEvents % 100 == 0) {
          CheckFileSize(sEvent.timestamp);
        }
      }

      newEvent = false;
      eventBuffer.clear();

      if (!sEvent.signalsID.empty()) {
        ++storedEvents;
        sEvent.Clear();
      }
    }

    if (stopEventBuilder) {
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

  std::cout << GetTimeStampFromUnixTime(runStartTime)
            << " End of event builder: " << storedEvents
            << " events stored and " << ev_count << " events triggered"
            << std::endl;

  if (!FEM.buffer.empty()) {
    std::cout << "FEM " << FEM.femID << " Buffer size left "
              << FEM.buffer.size() << std::endl;
    if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug) {
      fprintf(FEM.logFile, "------DEBUG BUFFER FRAMES LEFT-------\n");
      fprintf(stdout, "------DEBUG BUFFER FRAMES LEFT-------\n");
      for (const auto &word : FEM.buffer) {
        fprintf(FEM.logFile, "%08X\n", word);
        fprintf(stdout, "%08X\n", word);
      }
    }
  }

  if (fileRoot) {
    WriteRunEndTime(runEndTime);
  }
}

void FEMDAQDCC::SendCommand(const char *cmd) {

  auto &FEM = FEMArray.front();

  if (std::strstr(cmd, "asic") &&
      (std::strstr(cmd, "gain") || std::strstr(cmd, "time"))) {
    uint32_t asicID = 0, gain = 0, time = 0;
    if (std::sscanf(cmd, "asic %u gain 0x%x time 0x%x", &asicID, &gain,
                    &time) == 3) {
      uint8_t regValue = ((time & 0x0F) << 3) | ((gain & 0x03) << 1);
      char finalCmd[64];
      std::snprintf(finalCmd, sizeof(finalCmd), "asic %u write 1 0x%X", asicID,
                    regValue);
      SendCommand(finalCmd, FEM);
    } else {
      std::string error = "Invalid cmd sintax: " + std::string(cmd) +
                          " proper sintax is asic 0 gain 0x1 time 0x3";
      throw std::runtime_error(error);
    }
  } else {
    SendCommand(cmd, FEM);
  }
}

bool FEMDAQDCC::SendCommand(const char *cmd, FEMProxy &FEM, int pckType) {
  // pckType = -1 ASCII, >=0 Binary

  const int e =
      sendto(FEM.client, cmd, strlen(cmd), 0, (struct sockaddr *)&(FEM.target),
             sizeof(struct sockaddr));
  if (e == -1) {
    std::string error = "sendto failed: " + std::string(strerror(errno));
    throw std::runtime_error(error);
  }

  const bool logCmd = strncmp(cmd, "wait", 4) != 0 &&
                      strncmp(cmd, "areq", 4) != 0 &&
                      strncmp(cmd, "isobus", 6) != 0;

  if (FEM.logFile != nullptr &&
      runConfig.verboseLevel >= RunConfig::Verbosity::Info) {
    if (logCmd)
      fprintf(FEM.logFile, ">> FEM %u Cmd sent %s\n", FEM.femID, cmd);
  }

  // wait for incoming messages
  bool done = false;
  int length = 0;
  size_t pckCnt = 1;
  uint16_t buf_rcv[8192 / sizeof(uint16_t)];
  uint8_t *buf_ual;

  while (!done) {
    int cnt = 0;
    auto startTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::duration<int>>(
        std::chrono::steady_clock::now() - startTime);

    do {
      length = recvfrom(FEM.client, buf_rcv, 8192, 0,
                        (struct sockaddr *)&FEM.remote, &FEM.remote_size);

      if (length < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
          if (cnt % 1000 == 0) {
            duration = std::chrono::duration_cast<std::chrono::duration<int>>(
                std::chrono::steady_clock::now() - startTime);
            // if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
            // fprintf(stderr, "socket() failed: %s\n", strerror(errno));
          }

        } else {
          if (abrt)
            return false;
          std::string error =
              "recvfrom failed: " + std::string(strerror(errno));
          throw std::runtime_error(error);
        }
      }
      if (abrt)
        return false;
      cnt++;
    } while (length < 0 && duration.count() < 10);

    if (duration.count() >= 10 || length < 0) {
      std::string error = "TIMEOUT: No reply after " +
                          std::to_string(duration.count()) + " seconds";
      throw std::runtime_error(error);
    }

    // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit
    // boundary, so skip these first two bytes
    int index = 0;
    if ((buf_rcv[0] == 0)) {
      length -= 2;
      index = 1;
    }

    buf_ual = reinterpret_cast<uint8_t *>(&buf_rcv[1]);

    // ERROR ASCII packet
    if (*buf_ual == '-') {
      if (strncmp(cmd, "wait", 4) == 0)
        return true;
      *(buf_ual + length) = '\0';
      fprintf(stdout, "--------ERROR packet---------: %s\n", buf_ual);
      if (FEM.logFile)
        fprintf(FEM.logFile, "--------ERROR packet---------: %s\n", buf_ual);
      return false;
    }

    if (pckType >= 0) { // Data packet

      DCCPacket::DataPacket *data_pkt = (DCCPacket::DataPacket *)buf_ual;

      if (GET_TYPE(ntohs(data_pkt->hdr)) == RESP_TYPE_ADC_DATA) {
        const size_t pckSize = (ntohs(data_pkt->size)) / sizeof(uint16_t);
        FEM.mutex_mem.lock();
        FEM.tmpBuffer.insert(FEM.tmpBuffer.end(), &buf_rcv[index],
                             &buf_rcv[index + pckSize]);
        FEM.mutex_mem.unlock();

        if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
          PrintMonitoring(data_pkt);
      } else {
        PrintMonitoring(data_pkt);
      }

      // Check End Of Event
      if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV ||
          GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ ||
          (pckType > 0 && pckCnt >= pckType)) {
        done = true;
      }

    } else { // ASCII Packet
      if (logCmd) {
        *(buf_ual + length) = '\0';
        if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
          fprintf(stdout, "dcc().rep(): %s", buf_ual);
        if (FEM.logFile)
          fprintf(FEM.logFile, ">>> dcc().rep(): %s", buf_ual);
      }
      done = true;
    }
    pckCnt++;
  }
  return false;
}

void FEMDAQDCC::waitForTrigger() { // Wait till trigger is acquired
  auto &FEM = FEMArray.front();

  bool retry = false;
  do {
    retry =
        SendCommand("wait 1000000", FEM); // Wait for the event to be acquired
  } while (retry && !abrt); // Infinite loop till aborted or wait succeed
}

void FEMDAQDCC::PrintMonitoring(DCCPacket::DataPacket *pck) {
  char *ptr = nullptr;
  size_t size_mem = 0;

  auto &FEM = FEMArray.front();

  FILE *mem_fp = open_memstream(&ptr, &size_mem);
  if (!mem_fp)
    return;

  std::string tmstmp = GetTimeStampFromUnixTime(getCurrentTime());
  fprintf(mem_fp, "--- FEM %u MONI PACKET | %s ---\n", FEM.femID,
          tmstmp.c_str());

  DataPacket_Print(pck, mem_fp);
  fprintf(mem_fp, "\n");

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

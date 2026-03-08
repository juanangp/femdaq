
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
        SendCommand(cmd);
      }
    }
  }

  SendCommand("fem 0");

  for (auto fecID : fecs) {
    for (int a = 0; a < 4; a++) {
      sprintf(cmd, "hped getsummary %d %d %d:%d", fecID, a, 3, 78);
      SendCommand(cmd, FEM, DCCPacket::packetType::BINARY, 1,
                  DCCPacket::packetDataType::PEDESTAL); // Get summary
      sprintf(cmd, "hped centermean %d %d %d:%d %d", fecID, a, 3, 78, mean);
      SendCommand(cmd); // Set mean
      sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fecID, a, 3, 78, mean,
              stdDev);
      SendCommand(cmd); // Set thr
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

  sEvent.Clear();
  runStartTime = getCurrentTime();
  double lastTimeSaved = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0;

  if (fileRoot)
    WriteRunStartTime(runStartTime);

  stopRun = false;
  storedEvents = 0;
  int eventID = 0;

  UpdateRunThread = std::thread(&FEMDAQ::UpdateThread, this);

  auto &FEM = FEMArray.front();
  const auto &fecs = runConfig.fems.front().fecs;
  char cmd[200];

  while (!stopRun) {
    // SendCommand("fem 0");

    SendCommand("isobus 0x64", FEM); // SCA start
    if (internal)
      SendCommand("isobus 0x14", FEM);
    sEvent.Clear();
    sEvent.eventID = eventID;
    waitForTrigger();
    if (abrt)
      break;
    // Perform data acquisition phase, compress, accept size
    sEvent.timestamp = getCurrentTime();
    for (auto fecID : fecs) {
      for (int a = 0; a < 4; a++) {
        sprintf(cmd, "areq %d %d %d %d %d", mode, fecID, a, 3, 78);
        SendCommand(cmd, FEM, DCCPacket::packetType::BINARY, 0,
                    DCCPacket::packetDataType::EVENT);
      }
    }

    eventID++;

    if (sEvent.signalsID.empty())
      continue;

    if (fileRoot) {
      FillTree(sEvent.timestamp, lastTimeSaved);

      if (storedEvents % 100 == 0) {
        CheckFileSize(sEvent.timestamp);
      }
    }

    ++storedEvents;
    sEvent.Clear();
  }

  if (fileRoot) {
    WriteRunEndTime(getCurrentTime());
  }

  std::cout << "End of DAQ " << storedEvents << " events acquired" << std::endl;
}

void FEMDAQDCC::stopDAQ() {

  if (UpdateRunThread.joinable())
    UpdateRunThread.join();
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

DCCPacket::packetReply
FEMDAQDCC::SendCommand(const char *cmd, FEMProxy &FEM,
                       DCCPacket::packetType pckType, size_t nPackets,
                       DCCPacket::packetDataType dataType) {

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
  uint8_t buf_rcv[8192];
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
            return DCCPacket::packetReply::ERROR;
          std::string error =
              "recvfrom failed: " + std::string(strerror(errno));
          throw std::runtime_error(error);
        }
      }
      if (abrt)
        return DCCPacket::packetReply::ERROR;
      cnt++;
    } while (length < 0 && duration.count() < 10);

    if (duration.count() >= 10 || length < 0) {
      std::cout << "No reply after " << duration.count()
                << " seconds, missing packets are expected" << std::endl;
      return DCCPacket::packetReply::ERROR;
    }

    // if the first 2 bytes are null, UDP datagram is aligned on next 32-bit
    // boundary, so skip these first two bytes
    if ((buf_rcv[0] == 0) && (buf_rcv[1] == 0)) {
      buf_ual = &buf_rcv[2];
      length -= 2;
    } else {
      buf_ual = &buf_rcv[0];
    }

    if ((*buf_ual == '-') && strncmp(cmd, "wait", 4) == 0) {
      return DCCPacket::packetReply::RETRY;
    } else if ((*buf_ual == '-')) { // ERROR ASCII packet
      *(buf_ual + length) = '\0';
      fprintf(stdout, "--------ERROR packet---------: %s\n", buf_ual);
      if (FEM.logFile)
        fprintf(FEM.logFile, "--------ERROR packet---------: %s\n", buf_ual);
      return DCCPacket::packetReply::ERROR;
    }

    // show packet if desired
    if (runConfig.verboseLevel > RunConfig::Verbosity::Info &&
        pckType == DCCPacket::packetType::BINARY) {
      if (FEM.logFile)
        fprintf(FEM.logFile, ">>> dcc().rep(): %d bytes of data \n", length);
      DCCPacket::DataPacket *data_pk = (DCCPacket::DataPacket *)buf_ual;
      PrintMonitoring(data_pk);
    } else if (pckType != DCCPacket::packetType::BINARY) {
      if (logCmd) {
        *(buf_ual + length) = '\0';
        if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
          fprintf(stdout, "dcc().rep(): %s", buf_ual);
        if (FEM.logFile)
          fprintf(FEM.logFile, ">>> dcc().rep(): %s", buf_ual);
      }
    }

    if (pckType == DCCPacket::packetType::BINARY) { // DAQ packet

      DCCPacket::DataPacket *data_pkt = (DCCPacket::DataPacket *)buf_ual;

      if (dataType == DCCPacket::packetDataType::EVENT) {
        saveEvent(buf_ual, length);
      } else {
        PrintMonitoring(data_pkt);
      }

      // Check End Of Event
      if (GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EOEV ||
          GET_FRAME_TY_V2(ntohs(data_pkt->dcchdr)) & FRAME_FLAG_EORQ ||
          (nPackets > 0 && pckCnt >= nPackets)) {
        done = true;
      }

    } else {
      done = true; // ASCII Packet, check response?
    }

    pckCnt++;
  }
  return DCCPacket::packetReply::OK;
}

void FEMDAQDCC::waitForTrigger() { // Wait till trigger is acquired
  auto &FEM = FEMArray.front();

  DCCPacket::packetReply reply;
  do {
    reply =
        SendCommand("wait 1000000", FEM); // Wait for the event to be acquired
  } while (reply == DCCPacket::packetReply::RETRY &&
           !abrt); // Infinite loop till aborted or wait succeed
}

void FEMDAQDCC::saveEvent(unsigned char *buf, int size) {
  // If data supplied, copy to temporary buffer
  if (size <= 0)
    return;

  DCCPacket::DataPacket *dp = (DCCPacket::DataPacket *)buf;

  // Check if packet has ADC data
  if (GET_TYPE(ntohs(dp->hdr)) != RESP_TYPE_ADC_DATA)
    return;

  const unsigned int scnt = ntohs(dp->scnt);
  if ((scnt <= 8) && (ntohs(dp->samp[0]) == 0) && (ntohs(dp->samp[1]) == 0))
    return; // empty data
  if ((scnt <= 12) &&
      ((ntohs(dp->samp[0]) == 0x11ff) || (ntohs(dp->samp[1]) == 0x11ff)))
    return; // Data starting at 511 bin

  unsigned short fec, asic, channel;
  const unsigned short arg1 = GET_RB_ARG1(ntohs(dp->args));
  const unsigned short arg2 = GET_RB_ARG2(ntohs(dp->args));

  int physChannel =
      DCCPacket::Arg12ToFecAsicChannel(arg1, arg2, fec, asic, channel);

  if (physChannel < 0)
    return;

  if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
    std::cout << "FEC " << fec << " asic " << asic << " channel " << channel
              << " physChann " << physChannel << "\n";

  // bool compress = GET_RB_COMPRESS(ntohs(dp->args) );
  std::vector<short> sData(512, 0);
  short *sDataPtr = sData.data();

  unsigned short timeBin = 0;

  for (unsigned int i = 0; i < scnt && i < 511; i++) {
    short data = ntohs(dp->samp[i]);
    if ((data & 0xFE00) == 0x1000) {
      timeBin = GET_CELL_INDEX(data);
    } else if ((data & 0xF000) == 0) { // Check fastest method
      if (timeBin < 512)
        sDataPtr[timeBin] = std::move(data);
      // if (timeBin < 512) sDataPtr[timeBin] = data;
      // if (timeBin < 512) memcpy(&sData[timeBin],&data,sizeof(short));
      // if (timeBin < 512) std::copy(&sData[timeBin],&sData[timeBin], &data);
      timeBin++;
    }
  }

  sEvent.AddSignal(physChannel, sData);
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

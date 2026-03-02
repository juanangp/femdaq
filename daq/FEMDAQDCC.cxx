
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

void FEMDAQDCC::Pedestals() { // TODO make configurable via flags

  auto &FEM = FEMArray.front();
  const auto &fecs = runConfig.fems.front().fecs;
  char cmd[200];

  // Pedestal acquisition
  for (int i = 0; i < 100; i++) {
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
      sprintf(cmd, "hped centermean %d %d %d:%d %d", fecID, a, 3, 78, 250);
      SendCommand(cmd); // Set mean
      sprintf(cmd, "hped setthr %d %d %d:%d %d %.1f", fecID, a, 3, 78, 250,
              5.0);
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
  uint32_t prevEvCount = 0;
  double prevEventTime = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0;
  uint64_t fileSize = 0;

  int fileIndex = 1;
  const std::string baseName = MakeBaseFileName();
  std::string fileName = MakeFileName(baseName, fileIndex);

  if (!runConfig.readOnly) {
    OpenRootFile(fileName, sEvent, runStartTime);
  }

  stopRun = false;
  storedEvents = 0;
  int eventID = 0;
  auto &FEM = FEMArray.front();
  const auto &fecs = runConfig.fems.front().fecs;
  char cmd[200];

  while (!abrt && !stopRun) {
    // SendCommand("fem 0");

    SendCommand("isobus 0x6C", FEM); // SCA start
    if (internal)
      SendCommand("isobus 0x1C", FEM);
    sEvent.Clear();
    sEvent.eventID = eventID;
    waitForTrigger();
    // Perform data acquisition phase, compress, accept size
    sEvent.timestamp = getCurrentTime();
    for (auto fecID : fecs) {
      for (int a = 0; a < 4; a++) {
        sprintf(cmd, "areq %d %d %d %d %d", mode, fecID, a, 3, 78);
        SendCommand(cmd, FEM, DCCPacket::packetType::BINARY, 0,
                    DCCPacket::packetDataType::EVENT);
      }
    }

    UpdateRun(sEvent.timestamp, prevEventTime, eventID, prevEvCount);
    eventID++;

    if (sEvent.signalsID.size() == 0)
      continue;

    if (file) {
      FillTree(sEvent.timestamp, lastTimeSaved);

      if (storedEvents % 100 == 0) {
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

    ++storedEvents;
    sEvent.Clear();
  }

  if (file) {
    CloseRootFile(getCurrentTime());
  }

  std::cout << "End of DAQ " << storedEvents << " events acquired" << std::endl;
}

void FEMDAQDCC::stopDAQ() {}

void FEMDAQDCC::SendCommand(const char *cmd, bool wait) {

  for (auto &FEM : FEMArray) {
    if (FEM.active)
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

  if (runConfig.verboseLevel > RunConfig::Verbosity::Info)
    std::cout << "FEM " << FEM.femID << " Command sent " << cmd << std::endl;

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
          std::string error =
              "recvfrom failed: " + std::string(strerror(errno));
          throw std::runtime_error(error);
        }
      }
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

    // show packet if desired
    if (runConfig.verboseLevel > RunConfig::Verbosity::Info) {
      printf("dcc().rep(): %d bytes of data \n", length);
      if (pckType == DCCPacket::packetType::BINARY) {
        DCCPacket::DataPacket *data_pk = (DCCPacket::DataPacket *)buf_ual;
        DCCPacket::DataPacket_Print(data_pk);
      } else {
        *(buf_ual + length) = '\0';
        printf("dcc().rep(): %s", buf_ual);
      }
    }

    if ((*buf_ual == '-') && strncmp(cmd, "wait", 4) == 0) {
      return DCCPacket::packetReply::RETRY;
    } else if ((*buf_ual == '-')) { // ERROR ASCII packet
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
        printf("ERROR packet: %s\n", buf_ual);
      return DCCPacket::packetReply::ERROR;
    }

    if (pckType == DCCPacket::packetType::BINARY) { // DAQ packet

      DCCPacket::DataPacket *data_pkt = (DCCPacket::DataPacket *)buf_ual;

      if (dataType == DCCPacket::packetDataType::EVENT) {
        saveEvent(buf_ual, length);
      } else if (dataType == DCCPacket::packetDataType::PEDESTAL) {
        DCCPacket::DataPacket_Print(data_pkt);
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

void FEMDAQDCC::savePedestals(unsigned char *buf, int size) {
  // If data supplied, copy to temporary buffer
  if (size <= 0)
    return;
  DCCPacket::DataPacket *dp = (DCCPacket::DataPacket *)buf;

  // Check if packet has ADC data
  if (GET_TYPE(ntohs(dp->hdr)) != RESP_TYPE_HISTOSUMMARY)
    return;

  DCCPacket::PedestalHistoSummaryPacket *pck =
      (DCCPacket::PedestalHistoSummaryPacket *)dp;

  unsigned short fec, asic;
  const unsigned short arg1 = GET_RB_ARG1(ntohs(pck->args));
  const unsigned short arg2 = GET_RB_ARG2(ntohs(pck->args));

  const unsigned int nbsw = (ntohs(pck->size) - 2 - 6 - 2) / sizeof(short);

  for (unsigned short ch = 0; ch < (nbsw / 2); ch++) {

    const short mean = ntohs(pck->stat[ch].mean);
    const short stdev = ntohs(pck->stat[ch].stdev);

    const int physChannel =
        DCCPacket::Arg12ToFecAsic(arg1, arg2, fec, asic, ch);

    if (runConfig.verboseLevel > RunConfig::Verbosity::Info) {
      std::cout << "FEC " << fec << " asic " << asic << " channel " << ch
                << " physChann " << physChannel << " mean "
                << (double)(mean) / 100.0 << " stddev "
                << (double)(stdev) / 100.0 << std::endl;
    }

    if (physChannel < 0)
      continue;

    std::vector<short> sData = {mean, stdev};

    sEvent.AddSignal(physChannel, sData);
  }
}

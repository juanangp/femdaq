
#include "FEMDAQARCFEM.h"
#include "ARCPacket.h"
#include "FEMINOSPacket.h"

#include <filesystem>
#include <future>

std::atomic<bool> FEMDAQARCFEM::stopReceiver(false);

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
    packetAPI.GetEventHeader = &ARCPacket::GetEventHeader;
    packetAPI.DataPacket_Print = &ARCPacket::DataPacket_Print;
    packetAPI.ConfigPacket_Print = &ARCPacket::ConfigPacket_Print;
  } else if (runConfig.electronics == "FEMINOS") {
    packetAPI.isMFrame = &FEMINOSPacket::isMFrame;
    packetAPI.isDataFrame = &FEMINOSPacket::isDataFrame;
    packetAPI.ParseEventFromWords = &FEMINOSPacket::ParseEventFromWords;
    packetAPI.TryExtractNextEvent = &FEMINOSPacket::TryExtractNextEvent;
    packetAPI.GetEventHeader = &FEMINOSPacket::GetEventHeader;
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
  // The FEM may send up to reqFrames frames without new credits. The credits
  // of the frames received are requested again as soon as frameThr of them
  // are pending, so the data in flight never exceeds reqFrames frames
  const uint32_t reqFrames = runConfig.daqCredits;
  uint8_t seq = 0x00;
  const uint32_t frameThr = runConfig.daqCreditThreshold;
  // First daq request, do not add sequence number
  sprintf(daq_cmd, "daq 0x%06x F\n", reqFrames);
  SendCommand(daq_cmd, FEM, false);

  while (!abrt && !stopRun) {
    {
      // The receiver thread notifies every frame, the timeout only bounds the
      // delay of a missed notification and of abrt/stopRun
      std::unique_lock<std::mutex> lk(FEM.credit_m);
      FEM.credit_cv.wait_for(lk, std::chrono::milliseconds(1),
                             [&] { return FEM.daq_credit >= frameThr; });
    }
    const uint32_t credits = FEM.daq_credit;
    if (credits >= frameThr && !abrt && !stopRun) {
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
        std::cout << "FEM " << FEM.femID << " Daq frames credits " << credits
                  << std::endl;
      sprintf(daq_cmd, "daq 0x%06x F 0x%02x\n", credits, seq);
      SendCommand(daq_cmd, FEM, false);
      FEM.daq_credit -= credits;
      seq++;
    }
  }

  // Last request with sequence number (as mclient does) so the numbering of
  // the frames sent with the credits left is not restarted
  sprintf(daq_cmd, "daq 0x000000 F 0x%02x\n", seq);
  SendCommand(daq_cmd, FEM, false);
}

// Data frames from the Feminos carry a sequence number in their first word:
// 0x01XX in the first frame after a daq request without sequence number and
// 0x00XX, incrementing by one, in the next ones. Returns the number of frames
// lost before this one. Nothing is checked until the first 0x01XX frame, so
// old firmware, which always sends 0x0000, is never flagged
uint32_t FEMDAQARCFEM::CheckFrameSeq(FEMProxy &FEM, uint16_t seqWord) {

  const uint8_t seq = seqWord & 0xFF;
  uint32_t lost = 0;

  if (seqWord & 0x0100) {
    FEM.seqSynced = true;
  } else if (FEM.seqSynced) {
    lost = static_cast<uint8_t>(seq - FEM.expSeq);
  } else if (++FEM.unsyncedFrames == 16) {
    std::string msg = "[SYNC] WARNING FEM " + std::to_string(FEM.femID) +
                      " data frames without sequence number (old firmware?):"
                      " lost frames cannot be detected and each one reduces"
                      " the DAQ credits until the FEM stops sending data";
    std::cout << msg << std::endl;
    if (FEM.logFile) {
      fprintf(FEM.logFile, "%s\n", msg.c_str());
      fflush(FEM.logFile);
    }
  }

  FEM.expSeq = seq + 1;
  return lost;
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
  const bool isFeminos = (runConfig.electronics == "FEMINOS");
  const size_t offset = isFeminos ? 1 : 0;

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
      const uint32_t lost = isFeminos ? CheckFrameSeq(FEM, buf_rcv[0]) : 0;
      // The lost frames used credits of the FEM, they are requested again.
      // More losses than credits is not a real loss (e.g. duplicated frame)
      const bool recredit = (lost <= (uint32_t)runConfig.daqCredits);
      FEM.daq_credit += 1 + (recredit ? lost : 0);
      FEM.mutex_mem.lock();
      // The marker tells the event builder to drop the event in progress
      if (lost)
        FEM.tmpBuffer.push_back(PFX_FRAME_LOSS_MARKER);
      FEM.tmpBuffer.insert(FEM.tmpBuffer.end(), &buf_rcv[1],
                           &buf_rcv[size - offset]);
      FEM.mutex_mem.unlock();
      FEM.credit_cv.notify_one();

      if (lost) {
        FEM.lostFrames += lost;
        prometheus.setLostFrames(FEM.femID, FEM.lostFrames);
        std::ostringstream msg;
        msg << "[SYNC] FEM " << FEM.femID << " " << lost
            << " data frames lost before frame seq 0x" << std::hex
            << (buf_rcv[0] & 0xFF) << std::dec << " (" << FEM.lostFrames
            << " in this run)";
        if (!recredit)
          msg << " [unexpected sequence number, credits not restored]";
        std::cout << msg.str() << std::endl;
        if (FEM.logFile) {
          fprintf(FEM.logFile, "%s\n", msg.str().c_str());
          fflush(FEM.logFile);
        }
      }
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

  // Reset the receiver state before the builder starts and "daq" is sent
  for (auto &FEM : FEMArray) {
    FEM.mutex_mem.lock();
    FEM.tmpBuffer.clear();
    FEM.mutex_mem.unlock();
    FEM.daq_credit = 0;
    // Re-synched by the first frame after the daq without sequence number
    FEM.seqSynced = false;
    FEM.lostFrames = 0;
    FEM.unsyncedFrames = 0;
    prometheus.setLostFrames(FEM.femID, 0);
  }
  prometheus.setPartialEvents(0);

  if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
    std::cout << "DAQ credits " << runConfig.daqCredits << " frames (request "
              << "threshold " << runConfig.daqCreditThreshold
              << "), resync by timestamp " << (runConfig.resync ? "ON" : "OFF")
              << std::endl;

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

  // Max timestamp difference allowed between FEMs, in ticks of 10 ns
  constexpr int64_t kTsTolerance = 100;
  // Timestamps have 48 bits, the difference is computed modulo 2^48
  auto tsDelta = [](uint64_t a, uint64_t b) {
    return static_cast<int64_t>((a - b) << 16) >> 16;
  };

  double lastTimeSaved = runStartTime;
  uint32_t ev_count = 0;
  uint64_t ts = 0x0;

  if (fileRoot)
    WriteRunStartTime(runStartTime);

  bool newEvent = true;
  bool emptyBuffer = true;
  int tC = 0;
  std::vector<uint16_t> localBuffer;             // swapped with FEM.tmpBuffer
  std::vector<char> inEvent(FEMArray.size(), 0); // FEMs added to the event

  // Sync check between FEMs: all of them must read the same event
  bool lastInSync = true;
  uint64_t nChecked = 0, nOutOfSync = 0, nDropped = 0;

  // Writes a sync message to stdout and to the log of every active FEM
  auto logSync = [&](const std::string &msg) {
    std::cout << msg << std::endl;
    for (auto &FEM : FEMArray)
      if (FEM.active && FEM.logFile) {
        fprintf(FEM.logFile, "%s\n", msg.c_str());
        fflush(FEM.logFile);
      }
  };

  // Initialize FEMs
  for (auto &FEM : FEMArray) {
    FEM.bufferIndex = 0;
    FEM.pendingEvent = true;
    FEM.buffer.clear();
    FEM.eventWords.clear();
  }

  while (!(emptyBuffer && stopEventBuilder)) {
    emptyBuffer = true;
    newEvent = true;
    int nActive = 0;

    for (auto &FEM : FEMArray) {
      if (!FEM.active) // Inactive FEMs never send data
        continue;
      ++nActive;

      // Take the data received so far, the lock is only held for the swap
      FEM.mutex_mem.lock();
      localBuffer.swap(FEM.tmpBuffer);
      FEM.mutex_mem.unlock();

      FEM.buffer.insert(FEM.buffer.end(), localBuffer.begin(),
                        localBuffer.end());
      localBuffer.clear();

      // Read the next event of this FEM (it may still be waiting from a
      // previous loop). Events broken by lost frames cannot be matched with
      // the other FEMs and are dropped
      while (FEM.pendingEvent && !FEM.buffer.empty() &&
             packetAPI.TryExtractNextEvent(FEM.buffer, FEM.bufferIndex,
                                           FEM.eventWords)) {
        size_t sofPos = 0;
        bool lostFrames = false;
        const int nHeaders = packetAPI.GetEventHeader(
            FEM.eventWords, sofPos, FEM.evTs, FEM.evCount, lostFrames);

        if (nHeaders == 0 || lostFrames) {
          // A lone marker (frames lost between two events) drops no data
          if (nHeaders > 0 || FEM.eventWords.size() > 1) {
            ++nDropped;
            std::ostringstream msg;
            msg << "[SYNC] FEM " << FEM.femID << " event dropped: ";
            if (lostFrames)
              msg << "incomplete, data frames lost";
            else
              msg << "no Start Of Event, data frames lost";
            if (nHeaders > 0)
              msg << " | cnt=" << FEM.evCount << " ts=" << FEM.evTs;
            logSync(msg.str());
          }
          FEM.eventWords.clear();
          continue;
        }

        if (nHeaders > 1) {
          // The end of the previous event was lost, its words are dropped
          ++nDropped;
          std::ostringstream msg;
          msg << "[SYNC] FEM " << FEM.femID
              << " event dropped: no End Of Event, data frames lost | next"
              << " cnt=" << FEM.evCount << " ts=" << FEM.evTs;
          logSync(msg.str());
          FEM.eventWords.erase(FEM.eventWords.begin(),
                               FEM.eventWords.begin() + sofPos);
        }

        FEM.pendingEvent = false;
      }

      emptyBuffer &= FEM.buffer.empty() && FEM.pendingEvent;
      newEvent &= !FEM.pendingEvent; // Check if the event is pending
    }

    if (newEvent && nActive > 0) { // Save Event if closed

      // Check the sync of the FEMs: the events must have the same time. The
      // reference is the earliest event. With resync only the FEMs matching
      // it are added to this event, the others keep their event for the
      // next one, so a FEM missing an event does not shift the next events
      const FEMProxy *ref = nullptr;
      for (const auto &FEM : FEMArray)
        if (FEM.active && (!ref || tsDelta(FEM.evTs, ref->evTs) < 0))
          ref = &FEM;

      int nMatched = 0;
      for (size_t i = 0; i < FEMArray.size(); i++) {
        const auto &FEM = FEMArray[i];
        inEvent[i] = 0;
        if (!FEM.active)
          continue;
        const int64_t delta = tsDelta(FEM.evTs, ref->evTs);
        const bool matched = (delta >= -kTsTolerance && delta <= kTsTolerance);
        if (matched)
          ++nMatched;
        inEvent[i] = matched || !runConfig.resync;
      }
      const bool inSync = (nMatched == nActive);

      ++nChecked;
      if (!inSync) {
        ++nOutOfSync;
        if (runConfig.resync)
          prometheus.setPartialEvents(nOutOfSync);
      }

      // Log only when the sync state changes, not on every event
      if (inSync != lastInSync) {
        std::ostringstream msg;
        msg << "[SYNC] "
            << (inSync ? "FEMs back in sync" : "ERROR FEMs out of sync")
            << " at DAQ event " << storedEvents << " (" << nOutOfSync
            << " events out of sync so far)";
        if (!inSync && runConfig.resync)
          msg << " [resync: event built without the waiting FEMs]";
        for (size_t i = 0; i < FEMArray.size(); i++) {
          const auto &FEM = FEMArray[i];
          if (!FEM.active)
            continue;
          msg << " | FEM " << FEM.femID << " cnt=" << FEM.evCount
              << " ts=" << FEM.evTs << " (" << std::showpos
              << tsDelta(FEM.evTs, ref->evTs) << std::noshowpos << ")";
          if (!inEvent[i])
            msg << " waiting";
        }
        logSync(msg.str());
        lastInSync = inSync;
      }

      // Decode the events of the FEMs added to this event
      for (size_t i = 0; i < FEMArray.size(); i++) {
        if (!inEvent[i])
          continue;
        packetAPI.ParseEventFromWords(FEMArray[i].eventWords, sEvent, ts,
                                      ev_count);
        FEMArray[i].eventWords.clear();
      }

      sEvent.eventID = ref->evCount;
      sEvent.timestamp = (double)ref->evTs * 1.E-8 + runStartTime;

      if (storedEvents == 0) {
        if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
          std::cout << "Start time: " << GetTimeStampFromUnixTime(runStartTime)
                    << " Last ev tS: "
                    << GetTimeStampFromUnixTime(sEvent.timestamp) << " "
                    << sEvent.timestamp - runStartTime << std::endl;
      }

      if (fileRoot && !sEvent.signalsID.empty()) {
        // Event counter and timestamp read from each FEM in the event
        // (sEvent.Clear() below empties them after the event is stored)
        for (size_t i = 0; i < FEMArray.size(); i++) {
          if (!inEvent[i])
            continue;
          sEvent.femID.push_back(FEMArray[i].femID);
          sEvent.femEventCount.push_back(FEMArray[i].evCount);
          sEvent.femTimestamp.push_back(FEMArray[i].evTs);
        }

        FillTree(sEvent.timestamp, lastTimeSaved);

        if (storedEvents % 100 == 0) {
          CheckFileSize(sEvent.timestamp);
        }
      }

      for (size_t i = 0; i < FEMArray.size(); i++)
        if (inEvent[i])
          FEMArray[i].pendingEvent = true;

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

  if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
    std::cout << "End time: " << GetTimeStampFromUnixTime(runEndTime)
              << " Last evtS: " << GetTimeStampFromUnixTime(sEvent.timestamp)
              << " " << runEndTime - sEvent.timestamp << std::endl;

  for (auto &FEM : FEMArray)
    if (!FEM.buffer.empty()) {
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
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

  // Sync summary of the run
  std::ostringstream summary;
  summary << "[SYNC] " << nChecked - nOutOfSync << " of " << nChecked
          << " events in sync";
  if (nOutOfSync) {
    if (runConfig.resync)
      summary << ", " << nOutOfSync << " built without some FEM (resync)";
    else
      summary << ", " << nOutOfSync << " POSSIBLY OUT OF SYNC";
  }
  if (nDropped)
    summary << ", " << nDropped << " FEM events dropped";
  if (runConfig.electronics == "FEMINOS") {
    summary << " | lost data frames:";
    for (const auto &FEM : FEMArray)
      if (FEM.active)
        summary << " FEM " << FEM.femID << "=" << FEM.lostFrames;
  }
  logSync(summary.str());

  // Many partial events points to FEM timestamps that are not synchronized,
  // then the events cannot be matched by time
  if (runConfig.resync && nChecked >= 100 && nOutOfSync * 10 > nChecked)
    logSync("[SYNC] WARNING more than 10% of the events were built without "
            "some FEM. Check that the FEM timestamps are synchronized (TCM "
            "timestamp clear), otherwise set 'resync: false' in the config");

  std::cout << "End of event builder " << GetTimeStampFromUnixTime(runEndTime)
            << " " << storedEvents << " events acquired in "
            << FormatElapsedTime(runEndTime - runStartTime)
            << " Avg rate: " << storedEvents / (runEndTime - runStartTime)
            << " Hz" << std::endl;
}

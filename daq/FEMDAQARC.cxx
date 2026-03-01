
#include "FEMDAQARC.h"
#include "ARCPacket.h"

#include <filesystem>

FEMDAQARC::FEMDAQARC(RunConfig &rC) : FEMDAQ(rC) {}

void FEMDAQARC::Receiver() {

  fd_set readfds, writefds, exceptfds, readfds_work;
  struct timeval t_timeout;
  t_timeout.tv_sec = 5;
  t_timeout.tv_usec = 0;

  // Build the socket descriptor set from which we want to read
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&exceptfds);

  int smax = 0;
  for (auto &FEM : FEMArray) {
    FD_SET(FEM.client, &readfds);
    if (FEM.client > smax)
      smax = FEM.client;
  }
  smax++;
  int err = 0;
  while (!FEMDAQ::stopReceiver) {

    // Copy the read fds from what we computed outside of the loop
    readfds_work = readfds;

    // Wait for any of these sockets to be ready
    if ((err = select(smax, &readfds_work, &writefds, &exceptfds, &t_timeout)) <
        0) {
      std::string error = "select failed: " + std::string(strerror(errno));
      throw std::runtime_error(error);
    }

    if (err == 0)
      continue; // Nothing received

    for (auto &FEM : FEMArray) {

      if (!FD_ISSET(FEM.client, &readfds_work))
        continue;

      uint16_t buf_rcv[8192 / (sizeof(uint16_t))];
      int length = 0;
      // protect socket operations
      {
        FEM.mutex_socket.lock();
        length = recvfrom(FEM.client, buf_rcv, 8192, 0,
                          (struct sockaddr *)&FEM.remote, &FEM.remote_size);
        FEM.mutex_socket.unlock();
        if (length < 0) {
          std::string error =
              "recvfrom failed: " + std::string(strerror(errno));
          throw std::runtime_error(error);
        }
      }
      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
        std::cout << "Packet received with length " << length << " bytes"
                  << std::endl;

      if (length <= 6)
        continue; // empty frame?
      const size_t size =
          length /
          sizeof(
              uint16_t); // Note that length is in bytes while size is uint16_t

      if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
        ARCPacket::DataPacket_Print(&buf_rcv[1], size - 1);

      if (ARCPacket::isDataFrame(&buf_rcv[1])) {
        // const std::deque<uint16_t> frame (&buf_rcv[1], &buf_rcv[size -1]);
        FEM.mutex_mem.lock();
        FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size]);
        const size_t bufferSize = FEM.buffer.size();
        FEM.mutex_mem.unlock();
        if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug)
          std::cout << "Packet buffered with size " << (int)size - 1
                    << " queue size: " << bufferSize << std::endl;
        if (bufferSize > 1024 * 1024 * 1024) {
          std::string error = "Buffer FULL with size " +
                              std::to_string(bufferSize / sizeof(uint16_t)) +
                              " bytes";
          throw std::runtime_error(error);
        }

      } else if (ARCPacket::isMFrame(&buf_rcv[1])) {
        FEM.mutex_mem.lock();
        FEM.cmd_rcv++;
        FEM.buffer.insert(FEM.buffer.end(), &buf_rcv[1], &buf_rcv[size]);
        const size_t bufferSize = FEM.buffer.size();
        FEM.mutex_mem.unlock();
        if (runConfig.verboseLevel >= RunConfig::Verbosity::Info)
          ARCPacket::DataPacket_Print(&buf_rcv[1], size - 1);
      } else {
        // std::cout<<"Frame is neither data or monitoring Val
        // 0x"<<std::hex<<buf_rcv[1]<<std::dec<<std::endl;
        FEM.cmd_rcv++;
      }
    }
  }

  std::cout << "End of receiver " << err << std::endl;
}

void FEMDAQARC::startDAQ() {

  SignalEvent sEvent;
  const double startTime = FEMDAQ::getCurrentTime();

  uint32_t ev_count = 0;
  uint64_t ts = 0;

  int fileIndex = 1;
  const std::string baseName = MakeBaseFileName();
  std::string fileName = MakeFileName(baseName, fileIndex);

  // Create model
  auto model = sEvent.CreateModel();
  // Create writer
  auto writer = sEvent.CreateWriter(fileName, std::move(model));

  if (!runConfig.readOnly) {
    writeMetadataStart(fileName, runConfig.GetFileName(), startTime);
  }

  bool stopDAQ = false;
  bool newEvent = true;
  bool emptyBuffer = true;
  int tC = 0;

  while (!(emptyBuffer && stopDAQ)) {
    emptyBuffer = true;
    newEvent = true;

    for (auto &FEM : FEMArray) {
      std::deque<uint16_t> eventBuffer;
      FEM.mutex_mem.lock();
      emptyBuffer &= FEM.buffer.empty();
      if (!FEM.buffer.empty()) {
        if (FEM.pendingEvent) { // Wait till we reach end of event for all the
                                // ARC
          FEM.pendingEvent = !ARCPacket::TryExtractNextEvent(
              FEM.buffer, FEM.bufferIndex, eventBuffer);
        }
      }
      FEM.mutex_mem.unlock();
      if (!FEM.pendingEvent)
        ARCPacket::ParseEventFromWords(eventBuffer, sEvent, ts, ev_count);
      newEvent &= !FEM.pendingEvent; // Check if the event is pending
    }

    stopDAQ = FEMDAQ::abrt ||
              !(runConfig.nEvents == 0 || ev_count < runConfig.nEvents);

    if (newEvent) { // Save Event if closed
      sEvent.eventID = ev_count;
      sEvent.timestamp = (double)ts * 2E-8 + startTime;

      if (!runConfig.readOnly) {

        if (std::filesystem::file_size(fileName) >= runConfig.fileSize) {

          writer->Fill();
          writer.reset();

          writeMetadataEnd(fileName, FEMDAQ::getCurrentTime());

          fileIndex++;

          // Create new writer + new model (must be recreated!)
          auto newModel = sEvent.CreateModel();
          fileName = MakeFileName(baseName, fileIndex);
          writer = SignalEvent::CreateWriter(fileName, std::move(newModel));
          writeMetadataStart(fileName, runConfig.GetFileName(), startTime);

        } else {
          writer->Fill();
        }

        if (ev_count % 100 == 0)
          std::cout << "Events " << ev_count << std::endl;

        for (auto &FEM : FEMArray)
          FEM.pendingEvent = true;

        sEvent.signals.clear();
      }
    }

    if (stopDAQ) {
      for (auto &FEM : FEMArray)
        std::cout << "Buffer size " << FEM.buffer.size() << std::endl;
      tC++;
      if (tC >= 1000)
        break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }

  writer.reset();

  writeMetadataEnd(fileName, FEMDAQ::getCurrentTime());

  std::cout << "End of DAQ " << std::endl;
}

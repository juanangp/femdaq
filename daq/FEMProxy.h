#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "FEMSocket.h"

class FEMProxy : public FEMSocket {
public:
  FEMProxy() { tmpBuffer.reserve(500 * 72 * 4 * 520); }
  ~FEMProxy() = default;

  FEMProxy(const FEMProxy &) = delete;
  FEMProxy &operator=(const FEMProxy &) = delete;

  FEMProxy(FEMProxy &&other) noexcept : FEMSocket(std::move(other)) {
    pendingEvent = other.pendingEvent;
    active = other.active;
    femID = other.femID;
    bufferIndex = other.bufferIndex;
    evCount = other.evCount;
    evTs = other.evTs;
    expSeq = other.expSeq;
    buffer = std::move(other.buffer);
    tmpBuffer = std::move(other.tmpBuffer);
    eventWords = std::move(other.eventWords);
    cmd_sent.store(other.cmd_sent.load());
    cmd_rcv.store(other.cmd_rcv.load());
    daq_credit.store(other.daq_credit.load());
    seqSynced.store(other.seqSynced.load());
    lostFrames.store(other.lostFrames.load());
    unsyncedFrames.store(other.unsyncedFrames.load());
  }

  FEMProxy &operator=(FEMProxy &&other) noexcept {
    if (this != &other) {
      FEMSocket::operator=(std::move(other));
      pendingEvent = other.pendingEvent;
      active = other.active;
      femID = other.femID;
      bufferIndex = other.bufferIndex;
      evCount = other.evCount;
      evTs = other.evTs;
      expSeq = other.expSeq;
      buffer = std::move(other.buffer);
      tmpBuffer = std::move(other.tmpBuffer);
      eventWords = std::move(other.eventWords);

      cmd_sent.store(other.cmd_sent.load());
      cmd_rcv.store(other.cmd_rcv.load());
      daq_credit.store(other.daq_credit.load());
      seqSynced.store(other.seqSynced.load());
      lostFrames.store(other.lostFrames.load());
      unsyncedFrames.store(other.unsyncedFrames.load());
    }
    return *this;
  }

  bool pendingEvent = true;
  bool active = true;

  std::atomic<uint32_t> cmd_sent{0};
  std::atomic<uint32_t> cmd_rcv{0};
  // Frames received (or lost) whose credits are not yet requested again
  std::atomic<uint32_t> daq_credit{0};
  // Wakes up the DAQ command thread when new credits can be requested
  std::condition_variable credit_cv;
  std::mutex credit_m;
  int femID = 0;
  size_t bufferIndex = 0;

  // Event counter before any event is read
  static constexpr uint32_t kNoEvCount = 0xFFFFFFFF;
  // Event counter and timestamp (ticks of 10 ns) of the last event read
  uint32_t evCount = kNoEvCount;
  uint64_t evTs = 0;
  // Words of the last event read, waiting until it is added to a built event
  std::vector<uint16_t> eventWords;

  // Sequence number of the data frames (FEMINOS only), the check starts with
  // the first frame flagged as synch (0x01XX) after a daq without sequence
  std::atomic<bool> seqSynced{false};
  uint8_t expSeq = 0;
  std::atomic<uint64_t> lostFrames{0};
  // Data frames received in the run before the sequence number is synched
  std::atomic<uint32_t> unsyncedFrames{0};

  std::vector<uint16_t> tmpBuffer;
  std::deque<uint16_t> buffer;

  FILE *logFile = nullptr;

  std::mutex mutex_mem;
};

#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "FEMSocket.h"

// Event counter of a FEM, used to check that no event is lost or repeated.
// The counter of the first event of the run is taken as the origin, from then
// on it has to advance like the number of events built.
struct FEMEventCounter {
  // Counter of an event without Start-Of-Event header
  static constexpr uint32_t kNoEvCount = 0xFFFFFFFF;
  // Offset of an event without header
  static constexpr int64_t kNoHeader = INT64_MIN;

  uint32_t count = kNoEvCount; // Counter of the last event read
  uint64_t ts = 0;             // Timestamp of the last event read

  int64_t offset = 0;     // 0 in sync, >0 events lost, <0 events repeated
  int64_t lastOffset = 0; // Offset when it was last reported
  uint64_t nBad = 0;      // Events with offset != 0

  int64_t origin = 0;
  bool hasOrigin = false;

  void Reset() { *this = FEMEventCounter(); }

  // Updates the offset for the event number evIndex (0 is the first one)
  void Check(uint64_t evIndex) {
    if (count == kNoEvCount) {
      offset = kNoHeader;
    } else {
      if (!hasOrigin) {
        origin = (int64_t)count - (int64_t)evIndex;
        hasOrigin = true;
      }
      offset = (int64_t)count - (int64_t)evIndex - origin;
    }
    if (offset != 0)
      ++nBad;
  }
};

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
    evCounter = other.evCounter;
    buffer = std::move(other.buffer);
    tmpBuffer = std::move(other.tmpBuffer);
    cmd_sent.store(other.cmd_sent.load());
    cmd_rcv.store(other.cmd_rcv.load());
    daq_credit.store(other.daq_credit.load());
  }

  FEMProxy &operator=(FEMProxy &&other) noexcept {
    if (this != &other) {
      FEMSocket::operator=(std::move(other));
      pendingEvent = other.pendingEvent;
      active = other.active;
      femID = other.femID;
      bufferIndex = other.bufferIndex;
      evCounter = other.evCounter;
      buffer = std::move(other.buffer);
      tmpBuffer = std::move(other.tmpBuffer);

      cmd_sent.store(other.cmd_sent.load());
      cmd_rcv.store(other.cmd_rcv.load());
      daq_credit.store(other.daq_credit.load());
    }
    return *this;
  }

  bool pendingEvent = true;
  bool active = true;

  std::atomic<uint32_t> cmd_sent{0};
  std::atomic<uint32_t> cmd_rcv{0};
  std::atomic<uint32_t> daq_credit{0};
  int femID = 0;
  size_t bufferIndex = 0;

  FEMEventCounter evCounter;

  std::vector<uint16_t> tmpBuffer;
  std::deque<uint16_t> buffer;

  FILE *logFile = nullptr;

  std::mutex mutex_mem;
};

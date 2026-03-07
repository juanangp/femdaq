#pragma once

#include <deque>
#include <mutex>
#include <string>

#include "FEMSocket.h"

class FEMProxy : public FEMSocket {
public:
  FEMProxy() = default;
  ~FEMProxy() = default;

  FEMProxy(const FEMProxy &) = delete;
  FEMProxy &operator=(const FEMProxy &) = delete;

  FEMProxy(FEMProxy &&other) noexcept : FEMSocket(std::move(other)) {
    pendingEvent = other.pendingEvent;
    active = other.active;
    femID = other.femID;
    bufferIndex = other.bufferIndex;
    buffer = std::move(other.buffer);
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
      buffer = std::move(other.buffer);

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

  std::deque<uint16_t> buffer;

  FILE *logFile = nullptr;

  std::mutex mutex_mem;
};

#pragma once

#include <deque>
#include <mutex>

#include "FEMSocket.h"

class FEMProxy : public FEMSocket {
  
  public:
    FEMProxy() = default;
    ~FEMProxy() = default;

    // Inherits: copy deleted, move allowed
    FEMProxy(const FEMProxy&) = delete;
    FEMProxy& operator=(const FEMProxy&) = delete;

    FEMProxy(FEMProxy&&) = default;
    FEMProxy& operator=(FEMProxy&&) = default;
    
    bool pendingEvent=true;
    bool active = true;

    int cmd_sent=0;
    int cmd_rcv=0;
    int femID = 0;
    size_t bufferIndex=0;

    std::deque <uint16_t> buffer;

    inline static std::mutex mutex_socket;
    inline static std::mutex mutex_mem;
};

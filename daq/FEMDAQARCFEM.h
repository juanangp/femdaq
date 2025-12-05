#pragma once


#include "FEMDAQ.h"
#include "SignalEvent.h"

class FEMDAQARCFEM : public FEMDAQ {
public:

  struct PacketAPI {
    bool (*isMFrame)(uint16_t*);
    bool (*isDataFrame)(uint16_t*);
    bool (*TryExtractNextEvent)(std::deque<uint16_t>&, size_t&, std::deque<uint16_t>&);
    void (*ParseEventFromWords)(std::deque<uint16_t>&, SignalEvent&, uint64_t&, uint32_t&);
    void (*DataPacket_Print)(uint16_t *, const uint16_t &);
  };

  PacketAPI packetAPI;

  std::thread receiveThread, eventBuilderThread;

  explicit FEMDAQARCFEM(RunConfig& rC);
  ~FEMDAQARCFEM( );
  
  virtual void Receiver( ) override;
  virtual void startDAQ( ) override;
  virtual void stopDAQ( ) override;

  void EventBuilder( );

private:
    struct Registrar {
        Registrar();
    };
    static Registrar registrar_;
};

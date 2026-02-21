#pragma once


#include "FEMDAQ.h"

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

  static std::atomic<bool> stopReceiver;
  static std::atomic<bool> stopEventBuilder;

  std::thread receiveThread, eventBuilderThread;

  explicit FEMDAQARCFEM(RunConfig& rC);
  ~FEMDAQARCFEM( );
  
  virtual void startDAQ( const std::string &flags ="" ) override;
  virtual void stopDAQ( ) override;
  virtual void SendCommand(const char* cmd, bool wait = true) override;
  
  void Receiver( );
  void EventBuilder( );
  void SendCommand(const char* cmd, FEMProxy &FEM, bool wait);
  void waitForCmd(FEMProxy &FEM);

private:
    struct Registrar {
        Registrar();
    };
    static Registrar registrar_;
};

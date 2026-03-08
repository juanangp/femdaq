#pragma once

#include "DCCPacket.h"
#include "FEMDAQ.h"

class FEMDAQDCC : public FEMDAQ {

public:
  explicit FEMDAQDCC(RunConfig &rC);
  ~FEMDAQDCC();

  virtual void startDAQ(const std::vector<std::string> &flags) override;
  virtual void stopDAQ() override;
  virtual void SendCommand(const char *cmd) override;

  virtual void Pedestals(const std::vector<std::string> &flags) override;

  DCCPacket::packetReply SendCommand(
      const char *cmd, FEMProxy &FEM,
      DCCPacket::packetType type = DCCPacket::packetType::ASCII,
      size_t nPackets = 0,
      DCCPacket::packetDataType dataType = DCCPacket::packetDataType::NONE);
  void waitForTrigger();
  void saveEvent(unsigned char *buf, int size);
  void PrintMonitoring(DCCPacket::DataPacket *pck);

private:
  struct Registrar {
    Registrar();
  };
  static Registrar registrar_;
};

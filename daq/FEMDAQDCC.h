#pragma once

#include "DCCPacket.h"
#include "FEMDAQ.h"

class FEMDAQDCC : public FEMDAQ {

public:
  static std::atomic<bool> stopEventBuilder;

  std::thread eventBuilderThread;

  explicit FEMDAQDCC(RunConfig &rC);
  ~FEMDAQDCC();

  virtual void startDAQ(const std::vector<std::string> &flags) override;
  virtual void stopDAQ() override;
  virtual void SendCommand(const char *cmd) override;
  virtual void Pedestals(const std::vector<std::string> &flags) override;

  void EventBuilder();
  bool SendCommand(const char *cmd, FEMProxy &FEM, int pckType = -1);
  void waitForTrigger();
  void saveEvent(unsigned char *buf, int size);
  void PrintMonitoring(DCCPacket::DataPacket *pck);

private:
  struct Registrar {
    Registrar();
  };
  static Registrar registrar_;
};

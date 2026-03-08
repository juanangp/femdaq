#pragma once

#include "FEMDAQ.h"

class FEMDAQARCFEM : public FEMDAQ {
public:
  struct PacketAPI {
    bool (*isMFrame)(uint16_t *);
    bool (*isDataFrame)(uint16_t *);
    bool (*TryExtractNextEvent)(std::deque<uint16_t> &, size_t &,
                                std::deque<uint16_t> &);
    void (*ParseEventFromWords)(std::deque<uint16_t> &, SignalEvent &,
                                uint64_t &, uint32_t &);
    void (*DataPacket_Print)(uint16_t *, const uint16_t &, FILE *);
    void (*ConfigPacket_Print)(uint16_t *, const uint16_t &, FILE *);
  };

  PacketAPI packetAPI;

  static std::atomic<bool> stopReceiver;
  static std::atomic<bool> stopEventBuilder;

  std::thread eventBuilderThread;
  std::vector<std::thread> receiverThreads;

  explicit FEMDAQARCFEM(RunConfig &rC);
  ~FEMDAQARCFEM();

  virtual void startDAQ(const std::vector<std::string> &flags) override;
  virtual void stopDAQ() override;
  virtual void SendCommand(const char *cmd) override;

  void FEMReceiverThread(FEMProxy &FEM);
  void EventBuilder();
  void SendCommand(const char *cmd, FEMProxy &FEM, bool wait = true);
  void waitForCmd(FEMProxy &FEM);
  void SendDAQCmdThread(FEMProxy &FEM);

  void PrintMonitoring(uint16_t *buff, const uint16_t &size, FEMProxy &FEM);

private:
  struct Registrar {
    Registrar();
  };
  static Registrar registrar_;
};

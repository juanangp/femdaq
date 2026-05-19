#pragma once

#include "FEMDAQ.h"

class TCM : public FEMDAQ {
public:
  explicit TCM(RunConfig &rC);
  ~TCM();

  virtual void startDAQ(const std::vector<std::string> &flags) override {
    std::cout << "Not implemented for TCM" << std::endl;
  }
  virtual void SendCommand(const char *cmd) override;

private:
  struct Registrar {
    Registrar();
  };
  static Registrar registrar_;
};

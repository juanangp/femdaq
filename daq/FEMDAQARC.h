#pragma once

#include "FEMDAQ.h"

class FEMDAQARC : public FEMDAQ {
public:
  explicit FEMDAQARC(RunConfig &rC);

  virtual void Receiver() override;
  virtual void startDAQ() override;

private:
  struct Registrar {
    Registrar() {
      FEMDAQ::RegisterType("ARC", [](RunConfig &cfg) {
        return std::make_unique<FEMDAQARC>(cfg);
      });
    }
  };
  static Registrar registrar_;
};


#pragma once

#include <memory>
#include <prometheus/exposer.h>
#include <prometheus/gauge.h>
#include <prometheus/info.h>
#include <prometheus/registry.h>
#include <string>

class PrometheusManager {
public:
  // Constructor
  PrometheusManager(const std::string &address = "0.0.0.0:8080");

  void setRunNumber(int num);
  void setRate(double rate);
  void setEvents(int count);
  void setMetadata(const std::string &name);
  void setRunName(const std::string &name);

private:
  prometheus::Exposer _exposer;
  std::shared_ptr<prometheus::Registry> _registry;

  prometheus::Gauge &_runNumberGauge;
  prometheus::Gauge &_rateGauge;
  prometheus::Gauge &_eventsGauge;

  prometheus::Family<prometheus::Info> &_infoFamily;
  prometheus::Family<prometheus::Info> &_runNameFamily;
};

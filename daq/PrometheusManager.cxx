#include "PrometheusManager.h"

using namespace prometheus;

PrometheusManager::PrometheusManager(const std::string &address)
    : _exposer(address), _registry(std::make_shared<Registry>()),
      // Initialization
      _runNumberGauge(BuildGauge()
                          .Name("run_number")
                          .Help("Run Number")
                          .Register(*_registry)
                          .Add({})),
      _rateGauge(BuildGauge()
                     .Name("event_rate")
                     .Help("Acquisition rate")
                     .Register(*_registry)
                     .Add({})),
      _eventsGauge(BuildGauge()
                       .Name("events_total")
                       .Help("Events acquired")
                       .Register(*_registry)
                       .Add({})),
      _infoFamily(BuildInfo()
                      .Name("run_metadata")
                      .Help("Run Metadata")
                      .Register(*_registry)),
      _runNameFamily(BuildInfo()
                         .Name("run_filename")
                         .Help("Run Filename")
                         .Register(*_registry)) {
  // Register exposer via HTTP
  _exposer.RegisterCollectable(_registry);
}

void PrometheusManager::setRunNumber(int num) { _runNumberGauge.Set(num); }

void PrometheusManager::setRate(double rate) { _rateGauge.Set(rate); }

void PrometheusManager::setEvents(int count) { _eventsGauge.Set(count); }

void PrometheusManager::setMetadata(const std::string &name) {
  _infoFamily.Add({{"runName", name}});
}

void PrometheusManager::setRunName(const std::string &name) {
  _runNameFamily.Add({{"filename", name}});
}

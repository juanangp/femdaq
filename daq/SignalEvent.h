#pragma once

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class SignalEvent {
public:
  int eventID;
  double timestamp = 0;
  std::vector<int> signalsID;
  std::vector<short> pulses;
  std::vector<int> femID;
  std::vector<uint32_t> femEventCount;
  std::vector<uint64_t> femTimestamp;

  SignalEvent() = default;
  ~SignalEvent() = default;

  inline void AddSignal(int sID, const std::vector<short> &pulse) {
    bool isSID = std::any_of(signalsID.begin(), signalsID.end(),
                             [sID](const auto &s) { return s == sID; });

    if (isSID) {
      std::cout << "Warning sID " << sID << " already exist for this event"
                << std::endl;
      return;
    }

    signalsID.emplace_back(sID);
    pulses.insert(pulses.end(), pulse.begin(), pulse.end());
  }

  inline void Clear() {
    signalsID.clear();
    pulses.clear();
    femID.clear();
    femEventCount.clear();
    femTimestamp.clear();
  }
};

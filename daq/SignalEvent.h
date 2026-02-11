#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <algorithm>

class SignalEvent {
public:

    int eventID;
    double timestamp = 0;
    std::vector<int> signalsID;
    std::vector<std::vector<short>> pulses;

    SignalEvent() = default;
    ~SignalEvent() = default;

    inline void AddSignal(int sID, const std::vector<short> &pulse){
      bool isSID = std::any_of(signalsID.begin(), signalsID.end(),
                       [sID](const auto& s){ return s == sID; });

        if (isSID){
          std::cout<<"Warning sID "<< sID<< " already exist for this event"<<std::endl;
          return;
        }

      signalsID.emplace_back(sID);
      pulses.emplace_back(std::move(pulse));
    }

    inline void Clear(){
      signalsID.clear();
      pulses.clear();
    }

};

#pragma once

#include <ROOT/RNTuple.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <vector>
#include <memory>
#include <string>

class SignalEvent {
public:

    int eventID;
    double timestamp = 0;
    std::vector<int> signalsID;
    std::vector<std::vector<short>> pulses;

    std::shared_ptr<int> evID;
    std::shared_ptr<double> tS;
    std::shared_ptr<std::vector<int>> sID;
    std::shared_ptr< std::vector<std::vector<short>> > pls;

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

    std::unique_ptr<ROOT::RNTupleModel> CreateModel() {
        auto model = ROOT::RNTupleModel::Create();

        evID = model->MakeField<int>("eventID");
        tS = model->MakeField<double>("timestamp");
        sID = model->MakeField<std::vector<int>>("signalsID");
        pls = model->MakeField< std::vector<std::vector<short>> >("pulses");

        return model;
    }

    void Fill( ){
      *evID = eventID;
      *tS = timestamp;
      *sID = signalsID;
      *pls = pulses;
    }
};

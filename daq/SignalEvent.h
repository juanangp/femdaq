#pragma once

#include <ROOT/RNTuple.hxx>
#include <ROOT/RNTupleModel.hxx>
#include <ROOT/RNTupleWriter.hxx>
#include <vector>
#include <memory>
#include <string>

class SignalEvent {
public:
    struct Signal {
        int signalID = 0;
        std::vector<short> pulse;
        Signal() = default;
        Signal(int sID, std::vector<short> p)
            : signalID(sID), pulse(std::move(p)) {}
    };

    int eventID = 0;
    double timestamp = 0;
    std::vector<Signal> signals;

    SignalEvent() = default;
    ~SignalEvent() = default;

    inline void AddSignal(int sID, const std::vector<short> &pulses){
      bool isSID = std::any_of(signals.begin(), signals.end(),
                       [sID](const Signal& s){ return s.signalID == sID; });

        if (isSID){
          std::cout<<"Warning sID "<< sID<< " already exist for this event"<<std::endl;
          return;
        }
      Signal s (sID, pulses);
      signals.emplace_back(std::move(s));
    }

    static std::unique_ptr<ROOT::RNTupleModel> CreateModel() {
        auto model = ROOT::RNTupleModel::Create();

        model->MakeField<int>("eventID");
        model->MakeField<double>("timestamp");
        model->MakeField<std::vector<Signal>>("signals");

        return model;
    }

    static std::unique_ptr<ROOT::RNTupleWriter> CreateWriter(
        const std::string &filename,
        std::unique_ptr<ROOT::RNTupleModel> model)
    {
        return ROOT::RNTupleWriter::Recreate(
            std::move(model), "SignalEvents", "sEvents");
    }
};

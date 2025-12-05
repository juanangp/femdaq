#include "FEMDAQ.h"

class FEMDAQDCC : public TRESTDAQBase {
public:
    explicit FEMDAQDCC(RunConfig& rC) : FEMDAQ(rC);
    
    virtual void startDAQ() override { };
    virtual void Receiver() override { };
    virtual void EventBuilder() override { };
    

private:
    struct Registrar {
        Registrar() {
            FEMDAQBase::RegisterType("DCC",
                [](RunConfig& cfg){ return std::make_unique<FEMDAQDCC>(cfg);});
        }
    };
    static Registrar registrar_;
};

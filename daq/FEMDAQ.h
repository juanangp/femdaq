#pragma once

#include <iostream>
#include <string>
#include <functional>
#include <thread>

#include <TFile.h>
#include <TTree.h>

#include "FEMProxy.h"
#include "RunConfig.h"
#include "SignalEvent.h"

class FEMDAQ {
   public:
    std::vector<FEMProxy> FEMArray;

    virtual ~FEMDAQ() = default;
    
    void setActiveFEM(const std::string &FEMID);

    virtual void startDAQ( const std::vector<std::string> &flags ) = 0;
    virtual void stopDAQ( ) = 0;
    virtual void SendCommand(const char* cmd, bool wait = true) = 0;
    virtual void Pedestals() { std::cout<<"Not implemented in current electronics"<<std::endl; }

    static std::atomic<bool> abrt;
    std::atomic<uint32_t> storedEvents{0};

    std::unique_ptr<TFile> file = nullptr;
    std::unique_ptr<TTree> event_tree = nullptr;

    void OpenRootFile(const std::string &fileName, SignalEvent &sEvent, const double startTime);
    void CloseRootFile(const double endTime);
    void FillTree(const double eventTime, double &lastTimeSaved);
    void UpdateRate(const double eventTime, double &prevEventTime, const uint32_t evCount, uint32_t &prevEvCount);
    void UpdateRunConfigInfo( ){ runConfig.UpdateInfo();}

    bool isReadOnly( ) const {return runConfig.readOnly;}

    static inline double lastEvTime = 0;
    static double getCurrentTime();

    std::string MakeBaseFileName( );
    std::string MakeFileName(const std::string& base, int index);

    using FactoryFunc = std::function<std::unique_ptr<FEMDAQ>(RunConfig&)>;

    // Factory method using RunConfig.electronics
    static std::unique_ptr<FEMDAQ> Create(RunConfig& cfg) {
        auto it = GetRegistry().find(cfg.electronics);
        if (it != GetRegistry().end()) {
            return it->second(cfg);
        }
        std::cout << cfg.electronics << " not implemented, skipping..." << std::endl;
        return nullptr;
    }

  protected:
    explicit FEMDAQ(RunConfig& rC);
    RunConfig runConfig;
    
    static bool RegisterType(const std::string& electronics, FactoryFunc func) {
        GetRegistry()[electronics] = func;
        return true;
    }

  private:
    static std::map<std::string, FactoryFunc>& GetRegistry() {
        static std::map<std::string, FactoryFunc> registry;
        return registry;
    }
};

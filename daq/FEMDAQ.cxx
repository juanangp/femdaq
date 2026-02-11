#include "FEMDAQ.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <regex>
#include <charconv>

#include <TObjString.h>

std::atomic<bool> FEMDAQ::abrt(false);
std::atomic<bool> FEMDAQ::stopReceiver(false);
std::atomic<bool> FEMDAQ::stopEventBuilder(false);

FEMDAQ::FEMDAQ(RunConfig& rC) : runConfig(rC) {

  for(const auto &fem : runConfig.fems ){
    FEMProxy FEM;
    FEM.Open(fem.IP);
    FEM.femID = fem.id;
    FEMArray.emplace_back(std::move(FEM));
  }

}

std::string FEMDAQ::MakeBaseFileName( ){

    namespace fs = std::filesystem;

    const std::string directory = runConfig.rawDataPath;

    std::regex pattern(R"(Run(\d{5})_.*)");
    int maxRun = -1;

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        const std::string name = entry.path().filename().string();

        std::smatch m;
        if (std::regex_match(name, m, pattern)) {
            maxRun = std::max(maxRun, std::stoi(m[1]));
        }
    }

    int nextRun = maxRun + 1;

    char runStr[16];
    snprintf(runStr, sizeof(runStr), "Run%05d", nextRun);

    std::string base = runStr;
    base += "_"+runConfig.tag;
    base += "_"+runConfig.experiment;
    base += "_"+runConfig.type;

    fs::path full = fs::path(directory) / base;
    return full.string();

    return base;
}

std::string FEMDAQ::MakeFileName(const std::string& base, int index) {
  std::ostringstream oss;
  oss << base << "_" << std::setw(3) << std::setfill('0') << index << ".root";
  return oss.str();
}

double FEMDAQ::getCurrentTime() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count() / 1000000.0;
}


void FEMDAQ::setActiveFEM(const std::string &FEMID){

  if(FEMID =="*") {
    for (auto &FEM : FEMArray){
      FEM.active = true;
    }
  } else {
    int fID;
    auto result = std::from_chars(FEMID.data(), FEMID.data() + FEMID.size(), fID);
      if (result.ec == std::errc()) {
        bool found = false;
        for (auto &FEM : FEMArray){
          if(FEM.femID == fID){
            FEM.active = true;
            found = true;
          } else {
            FEM.active = false;
          }
        }
        if(!found)std::cout<<"Warning FEMID "<<FEMID<<"/"<<fID<<" not found, all FEMs will be disabled!! "<<std::endl;
      } else {
        std::cout<<"Warning cannot decode FEM ID from string "<<FEMID<<" doing nothing!!!"<<std::endl;
      }
  }

}

void FEMDAQ::SendCommand(const char* cmd, bool wait){

  for (auto &FEM : FEMArray){
    if(FEM.active)
      SendCommand(cmd, FEM, wait);
  }

}

void FEMDAQ::OpenRootFile(const std::string &fileName, SignalEvent &sEvent, const double startTime){

  file.reset();
  event_tree.reset();

  file = std::make_unique<TFile>(fileName.c_str(), "RECREATE");
  event_tree = std::make_unique<TTree>("SignalEvent", "Signal events");
  event_tree->Branch("eventID", &sEvent.eventID);
  event_tree->Branch("timestamp", &sEvent.timestamp);
  event_tree->Branch("signalsID", &sEvent.signalsID);
  event_tree->Branch("pulses", &sEvent.pulses);
  
  const std::string yamlDump = runConfig.Dump();
  TObjString yamlObj(yamlDump.c_str());
  yamlObj.Write("RunConfigYAML", TObject::kOverwrite);

  const std::string rCFileName = runConfig.GetFileName();
  TObjString fnameObj(rCFileName.c_str());
  fnameObj.Write("yaml_fileName", TObject::kOverwrite);

  std::ostringstream ts;
  ts << std::fixed << std::setprecision(6) << startTime;
  TObjString tsObj(ts.str().c_str());
  tsObj.Write("startTime", TObject::kOverwrite);
}

void FEMDAQ::CloseRootFile( const double endTime){

   file->cd();
   event_tree->Write(nullptr, TObject::kOverwrite);
   std::ostringstream ts;
   ts << std::fixed << std::setprecision(6) << endTime;
   TObjString tsObj(ts.str().c_str());
   tsObj.Write("endTime", TObject::kOverwrite);
   file->Close();
}

void FEMDAQ::FillEvent(const double eventTime, double &lastTimeSaved){

event_tree->Fill();
double elapsed = eventTime - lastTimeSaved;

  if(storedEvents%1000==0 || elapsed >100 ){
    event_tree->AutoSave("SaveSelf");
    lastTimeSaved = eventTime;
  }
  
}

void FEMDAQ::SendCommand(const char* cmd, FEMProxy &FEM, bool wait){

  FEM.mutex_socket.lock();
  const int e = sendto (FEM.client, cmd, strlen(cmd), 0, (struct sockaddr*)&(FEM.target), sizeof(struct sockaddr));
  FEM.mutex_socket.unlock();
    if ( e == -1) {
      std::string error ="sendto failed: " + std::string(strerror(errno));
      throw std::runtime_error(error);
    }

   if (runConfig.verboseLevel >= RunConfig::Verbosity::Info )std::cout<<"FEM "<<FEM.femID<<" Command sent "<<cmd<<std::endl;

   if(wait){
     FEM.cmd_sent++;
     waitForCmd(FEM, cmd);
   }


}

void FEMDAQ::waitForCmd(FEMProxy &FEM, const char* cmd){

  int timeout = 0;
  bool condition = false;

    do {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      condition = (FEM.cmd_sent > FEM.cmd_rcv);
      timeout++;
    } while ( condition && timeout <20);

  if (runConfig.verboseLevel >= RunConfig::Verbosity::Debug )std::cout<<"Cmd sent "<<FEM.cmd_sent<<" Cmd Received: "<<FEM.cmd_rcv<<std::endl;

  if(timeout>=20)std::cout<<"Cmd timeout "<<cmd<<std::endl;
}



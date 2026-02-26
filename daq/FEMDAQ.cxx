#include "FEMDAQ.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <regex>
#include <charconv>

#include <TObjString.h>

std::atomic<bool> FEMDAQ::abrt(false);

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

   if (!file || !file->IsOpen()) return;

   file->cd();

   std::ostringstream ts;
   ts << std::fixed << std::setprecision(6) << endTime;
   TObjString tsObj(ts.str().c_str());
   tsObj.Write("endTime", TObject::kOverwrite);
   
   if (event_tree) {
      event_tree->Write(nullptr, TObject::kOverwrite);
   }

   file->Write(); 
    
    event_tree.reset(); 
    file.reset(); 
}

void FEMDAQ::FillTree(const double eventTime, double &lastTimeSaved){

event_tree->Fill();
const double elapsed = eventTime - lastTimeSaved;

  if(storedEvents%1000==0 || elapsed >60 ){
    event_tree->AutoSave("SaveSelf");
    lastTimeSaved = eventTime;
  }
  
}

void FEMDAQ::UpdateRate(const double eventTime, double &prevEventTime, const uint32_t evCount, uint32_t &prevEvCount){

const double elapsed = eventTime - prevEventTime;
if (elapsed < 10)return;

  char tmpstm[20]; //"YYYY-MM-DD HH:MM:SS" + \0
  std::time_t currentEvTime = static_cast<std::time_t>(eventTime);

  std::strftime(tmpstm, sizeof(tmpstm), "%Y-%m-%d %H:%M:%S", std::localtime(&currentEvTime));
    
  std::cout<<tmpstm<<" Total events: "<<evCount<<" Rate: "<<(evCount - prevEvCount)/elapsed<<" Hz"<<std::endl;
  prevEvCount = evCount;
  prevEventTime = eventTime;

}


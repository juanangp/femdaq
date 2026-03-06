#include "FEMDAQ.h"

#include <charconv>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>

#include <TObjString.h>

std::atomic<bool> FEMDAQ::abrt(false);
std::atomic<bool> FEMDAQ::stopRun(false);

FEMDAQ::FEMDAQ(RunConfig &rC) : runConfig(rC) {

  for (const auto &fem : runConfig.fems) {
    FEMProxy FEM;
    FEM.Open(fem.IP);
    FEM.femID = fem.id;
    FEMArray.emplace_back(std::move(FEM));
  }
}

void FEMDAQ::MakeBaseFileName() {

  namespace fs = std::filesystem;

  const std::string directory = runConfig.rawDataPath;

  std::regex pattern(R"(Run(\d{5})_.*)");
  int maxRun = -1;

  for (const auto &entry : fs::directory_iterator(directory)) {
    if (!entry.is_regular_file())
      continue;

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
  base += "_" + runConfig.tag;
  base += "_" + runConfig.experiment;
  base += "_" + runConfig.type;

  fs::path full = fs::path(directory) / base;
  baseFileName = full.string();
}

double FEMDAQ::getCurrentTime() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::system_clock::now().time_since_epoch())
             .count() /
         1000000.0;
}

std::string FEMDAQ::FormatElapsedTime(const double seconds) {

  double value;
  const char *unit;

  if (seconds >= 604800) { // 1 week
    value = seconds / 604800.0;
    unit = " weeks";
  } else if (seconds >= 86400) { // 1 day
    value = seconds / 86400.0;
    unit = " days";
  } else if (seconds >= 3600) { // 1 hour
    value = seconds / 3600.0;
    unit = " hours";
  } else if (seconds >= 60) {
    value = seconds / 60.0;
    unit = " minutes";
  } else {
    value = seconds;
    unit = " seconds";
  }

  char timeStr[32];
  int len = std::snprintf(timeStr, sizeof(timeStr), "%.2f", value);

  return std::string(timeStr, len) + unit;
}

std::string FEMDAQ::GetTimeStampFromUnixTime(const double tm) {

  char tmpstm[20]; //"YYYY-MM-DD HH:MM:SS" + \0
  std::time_t time = static_cast<std::time_t>(tm);

  std::strftime(tmpstm, sizeof(tmpstm), "%Y-%m-%d %H:%M:%S",
                std::localtime(&time));

  return std::string(tmpstm);
}

void FEMDAQ::setActiveFEM(const std::string &FEMID) {

  if (FEMID == "*") {
    for (auto &FEM : FEMArray) {
      FEM.active = true;
    }
  } else {
    int fID;
    auto result =
        std::from_chars(FEMID.data(), FEMID.data() + FEMID.size(), fID);
    if (result.ec == std::errc()) {
      bool found = false;
      for (auto &FEM : FEMArray) {
        if (FEM.femID == fID) {
          FEM.active = true;
          found = true;
        } else {
          FEM.active = false;
        }
      }
      if (!found)
        std::cout << "Warning FEMID " << FEMID << "/" << fID
                  << " not found, all FEMs will be disabled!! " << std::endl;
    } else {
      std::cout << "Warning cannot decode FEM ID from string " << FEMID
                << " doing nothing!!!" << std::endl;
    }
  }
}

void FEMDAQ::WriteRunStartTime(const double startTime) {

  if (!fileRoot || !fileRoot->IsOpen())
    return;

  fileRoot->cd();
  std::ostringstream ts;
  ts << std::fixed << std::setprecision(6) << startTime;
  fileRoot->cd();
  TObjString tsObj(ts.str().c_str());
  tsObj.Write("startTime", TObject::kOverwrite);
}

void FEMDAQ::WriteRunEndTime(const double endTime) {

  if (!fileRoot || !fileRoot->IsOpen())
    return;

  fileRoot->cd();
  std::ostringstream ts;
  ts << std::fixed << std::setprecision(6) << endTime;
  fileRoot->cd();
  TObjString tsObj(ts.str().c_str());
  tsObj.Write("endTime", TObject::kOverwrite);
}

void FEMDAQ::OpenFiles(const std::string &flag) {

  MakeBaseFileName();
  fileIndex = 1;

  fileNameRoot.clear();

  if (flag.empty() || flag == "all") {
    OpenRootFile();
    OpenFileLogs();
  } else if (flag == "root") {
    OpenRootFile();
    CloseLogFiles();
  } else if (flag == "log") {
    CloseRootFile();
    OpenFileLogs();
  } else {
    throw std::runtime_error("Invalid fopen flag: " + flag +
                             "; valid flags are: all, root or log");
  }
}

void FEMDAQ::OpenRootFile() {

  fileRoot.reset();
  event_tree.reset();

  std::ostringstream oss;
  oss << baseFileName << "_" << std::setw(3) << std::setfill('0') << fileIndex
      << ".root";
  fileNameRoot = oss.str();

  std::cout << "New file " << fileNameRoot << " " << std::endl;

  fileRoot = std::make_unique<TFile>(fileNameRoot.c_str(), "RECREATE");
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

  fileIndex++;
}

void FEMDAQ::OpenFileLogs() {

  for (auto &FEM : FEMArray) {
    std::string fileName =
        baseFileName + "_FEM" + std::to_string(FEM.femID) + ".log";
    FEM.logFile = fopen(fileName.c_str(), "a");
    std::string ts = GetTimeStampFromUnixTime(getCurrentTime());
    fprintf(FEM.logFile, "\n--- LOG FILE INITIALIZED AT %s ---\n", ts.c_str());
    fflush(FEM.logFile);
    DumpExecFileToFEMLog(FEM);
  }
}

void FEMDAQ::DumpExecFileToFEMLog(FEMProxy &FEM) {

  if (!FEM.logFile)
    return;
  if (execFile.empty())
    return;

  std::ifstream src(execFile);
  if (!src.is_open()) {
    std::cerr << "Error: Could not open text file: " << execFile << std::endl;
    return;
  }

  std::string ts = GetTimeStampFromUnixTime(getCurrentTime());
  fprintf(FEM.logFile, "\n--- [DUMP EXEC FILE: %s] Filename: %s ---\n",
          ts.c_str(), execFile.c_str());
  fflush(FEM.logFile);

  std::string line;
  while (std::getline(src, line)) {
    fprintf(FEM.logFile, "%s\n", line.c_str());
  }

  fprintf(FEM.logFile, "--- [DUMP END] ---\n\n");
  std::fflush(FEM.logFile);
}

void FEMDAQ::CloseLogFiles() {

  for (auto &FEM : FEMArray) {
    if (FEM.logFile != nullptr) {
      fclose(FEM.logFile);
      FEM.logFile = nullptr;
    }
  }

  execFile.clear();
}

void FEMDAQ::CloseFiles() {

  CloseRootFile();
  CloseLogFiles();
}

void FEMDAQ::CloseRootFile() {

  if (!fileRoot || !fileRoot->IsOpen())
    return;

  fileRoot->cd();

  if (event_tree) {
    event_tree->Write(nullptr, TObject::kOverwrite);
  }

  fileRoot->Write();

  event_tree.reset();
  fileRoot.reset();
}

void FEMDAQ::CheckFileSize(const double eventTime) {

  uint64_t fileSize = std::filesystem::file_size(fileNameRoot);

  if (fileSize >= runConfig.fileSize) {
    WriteRunEndTime(eventTime);
    CloseRootFile();

    OpenRootFile();
    WriteRunStartTime(eventTime);
  }
}

void FEMDAQ::FillTree(const double eventTime, double &lastTimeSaved) {

  fileRoot->cd();
  event_tree->Fill();
  const double elapsed = eventTime - lastTimeSaved;

  if (storedEvents % 1000 == 0 || elapsed > 60) {
    event_tree->AutoSave("SaveSelf");
    lastTimeSaved = eventTime;
  }
}

void FEMDAQ::UpdateThread() {

  double eventTime = getCurrentTime();
  double prevEventTime = eventTime;
  int prevEvCount = storedEvents.load();

  while (!abrt && !stopRun) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    double eventTime = getCurrentTime();
    const double elapsed = eventTime - prevEventTime;
    const double runElapsedTime = eventTime - runStartTime;
    auto tmpstm = GetTimeStampFromUnixTime(eventTime);

    std::cout << tmpstm << " Total events: " << storedEvents
              << " Rate: " << (storedEvents - prevEvCount) / elapsed << " Hz "
              << "Run time " << FormatElapsedTime(runElapsedTime) << std::endl;
    prevEvCount = storedEvents.load();
    prevEventTime = eventTime;
    if (runConfig.nEvents > 0 && storedEvents.load() >= runConfig.nEvents) {
      stopRun = true;
      break;
    } else if (runConfig.maxTimeSeconds > 0 &&
               runElapsedTime >= runConfig.maxTimeSeconds) {
      stopRun = true;
      break;
    }
  }
}

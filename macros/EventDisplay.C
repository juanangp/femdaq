#include <TApplication.h>
#include <TCanvas.h>
#include <TChain.h>
#include <TFile.h>
#include <TGButton.h>
#include <TGClient.h>
#include <TGFileDialog.h>
#include <TGLabel.h>
#include <TGNumberEntry.h>
#include <TGTextEntry.h>
#include <TGraph.h>
#include <TH1.h>
#include <TH2.h>
#include <THStack.h>
#include <TObjArray.h>
#include <TObjString.h>
#include <TPRegexp.h>
#include <TRootEmbeddedCanvas.h>
#include <TString.h>
#include <TSystem.h>
#include <TTimer.h>
#include <TTree.h>
#include <TTreeReader.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <vector>

class DAQGUI : public TGMainFrame {
private:
  TRootEmbeddedCanvas *fMainCanvas;
  TGTextEntry *fDataPathEntry;
  TGTextEntry *fDecoPathEntry;
  TGLabel *fStatusLabel;
  TGCheckButton *fCheckNextRun;

  // Pulse Parameter Entries
  TGNumberEntry *fBaseStart, *fBaseEnd;
  TGNumberEntry *fPulseStart, *fPulseEnd;
  TGNumberEntry *fSigmaThr;
  TGNumberEntry *fStepSize;
  TGNumberEntry *fSpecMaxEntry;

  TChain *fChain = nullptr;
  TTreeReader *fReader = nullptr;
  std::map<int, std::pair<std::string, double>> fDecodingMap;
  TTimer *fTimer;
  TTimer *fWatchdogTimer;

  // Lectores para tus ramas específicas
  TTreeReaderValue<std::vector<int>> *fReaderSignalsID = nullptr;
  TTreeReaderValue<std::vector<short>> *fReaderPulses = nullptr;
  TTreeReaderValue<int> *fReaderEventID = nullptr;
  TTreeReaderValue<double> *fReaderTimestamp = nullptr;

  int fEventID;
  double fTimestamp;
  std::vector<int> *fSignalsID = nullptr;
  std::vector<short> *fPulses = nullptr;

  Long64_t fEntry = 0;
  int fNChannels = 1;
  double fTimeRateStart = -1;
  int fRatePoints = 0;
  bool fIsRunning = false;

  int fCurrentRun = -1;
  int fCurrentSubrun = -1;
  TString fBaseFileName = "";
  TString fLastDecoFile = "";

  // Default Pulse Params
  int fBLS = 10, fBLE = 110, fPS = 100, fPE = 500;
  double fSThr = 7.0;
  int fSSz = 100;

  int fNReadouts = 1;

  std::map<int, TH1F *> fSpectraMap;
  std::map<int, TH2F *> fHitMapMap;
  std::map<int, THStack *> fStacksMap;
  TGraph *fRateGraph;
  std::vector<TH1S *> fHistos;

public:
  DAQGUI() : TGMainFrame(gClient->GetRoot(), 1200, 900) {

    fSpectraMap[1] =
        new TH1F("Spectra1", "Total Amplitude (Sys 1);Amplitude;Counts", 2048,
                 0, 100000);
    fHitMapMap[1] = new TH2F("HitMap1", "HitMap (Sys 1);X;Y", 1, 0, 1, 1, 0, 1);
    fRateGraph = new TGraph();
    fRateGraph->SetTitle("Rate;Time [s];Rate [Hz]");
    fRateGraph->SetMarkerStyle(20);
    fRateGraph->SetMarkerSize(0.6);
    fRateGraph->SetLineColor(kAzure + 1);
    fRateGraph->GetXaxis()->SetTimeDisplay(1);

    fTimer = new TTimer();
    fTimer->Connect("Timeout()", "DAQGUI", this, "NextEvent()");
    fWatchdogTimer = new TTimer();
    fWatchdogTimer->Connect("Timeout()", "DAQGUI", this, "CheckForNewData()");

    TGHorizontalFrame *mainFrame = new TGHorizontalFrame(this, 1200, 900);
    AddFrame(mainFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

    TGVerticalFrame *leftFrame = new TGVerticalFrame(mainFrame, 240, 900);
    mainFrame->AddFrame(
        leftFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 5, 5, 5, 5));

    auto btnL = new TGLayoutHints(kLHintsExpandX, 5, 5, 2, 2);
    auto labL = new TGLayoutHints(kLHintsLeft, 5, 5, 2, 0);

    // UI Controls
    TGTextButton *btnExit = new TGTextButton(leftFrame, "&Exit");
    btnExit->Connect("Clicked()", "DAQGUI", this, "DoExit()");
    leftFrame->AddFrame(btnExit, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Data File:"), labL);
    fDataPathEntry = new TGTextEntry(leftFrame, "");
    fDataPathEntry->SetEnabled(kFALSE);
    leftFrame->AddFrame(fDataPathEntry, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Deco File:"), labL);
    fDecoPathEntry = new TGTextEntry(leftFrame, "");
    fDecoPathEntry->SetEnabled(kFALSE);
    leftFrame->AddFrame(fDecoPathEntry, btnL);

    fStatusLabel = new TGLabel(leftFrame, "Status: Waiting");
    leftFrame->AddFrame(fStatusLabel, labL);

    // --- Pulse Parameters ---
    leftFrame->AddFrame(new TGLabel(leftFrame, "Baseline Start/End:"), labL);
    TGHorizontalFrame *fBL = new TGHorizontalFrame(leftFrame);
    fBaseStart =
        new TGNumberEntry(fBL, fBLS, 5, -1, TGNumberFormat::kNESInteger);
    fBaseEnd = new TGNumberEntry(fBL, fBLE, 5, -1, TGNumberFormat::kNESInteger);
    fBL->AddFrame(fBaseStart, btnL);
    fBL->AddFrame(fBaseEnd, btnL);
    leftFrame->AddFrame(fBL, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Pulse Start/End:"), labL);
    TGHorizontalFrame *fPSect = new TGHorizontalFrame(leftFrame);
    fPulseStart =
        new TGNumberEntry(fPSect, fPS, 5, -1, TGNumberFormat::kNESInteger);
    fPulseEnd =
        new TGNumberEntry(fPSect, fPE, 5, -1, TGNumberFormat::kNESInteger);
    fPSect->AddFrame(fPulseStart, btnL);
    fPSect->AddFrame(fPulseEnd, btnL);
    leftFrame->AddFrame(fPSect, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Sigma Threshold:"), labL);
    fSigmaThr =
        new TGNumberEntry(leftFrame, fSThr, 5, -1, TGNumberFormat::kNESRealOne);
    leftFrame->AddFrame(fSigmaThr, btnL);

    // --- Histogram Range ---
    leftFrame->AddFrame(new TGLabel(leftFrame, "Spectra Max Range:"), labL);
    fSpecMaxEntry =
        new TGNumberEntry(leftFrame, 100000, 8, -1, TGNumberFormat::kNESInteger,
                          TGNumberFormat::kNEAAnyNumber);
    fSpecMaxEntry->Connect("ValueSet(Long_t)", "DAQGUI", this,
                           "UpdateRange(Long_t)");
    leftFrame->AddFrame(fSpecMaxEntry, btnL);

    leftFrame->AddFrame(new TGLabel(leftFrame, "Step Size"), labL);
    fStepSize =
        new TGNumberEntry(leftFrame, fSSz, 5, -1, TGNumberFormat::kNESInteger);
    leftFrame->AddFrame(fStepSize, btnL);

    fCheckNextRun = new TGCheckButton(leftFrame, "Auto-load Next Run");
    leftFrame->AddFrame(fCheckNextRun, labL);

    // --- Action Buttons ---
    TGTextButton *btnOpen = new TGTextButton(leftFrame, "&Open Data");
    btnOpen->Connect("Clicked()", "DAQGUI", this, "OpenFile()");
    leftFrame->AddFrame(btnOpen, btnL);

    TGTextButton *btnDeco = new TGTextButton(leftFrame, "&Load Decoding");
    btnDeco->Connect("Clicked()", "DAQGUI", this, "OpenDecoding()");
    leftFrame->AddFrame(btnDeco, btnL);

    TGTextButton *btnReload = new TGTextButton(leftFrame, "&Reload Run");
    btnReload->Connect("Clicked()", "DAQGUI", this, "ReloadRun()");
    leftFrame->AddFrame(btnReload, btnL);

    TGTextButton *btnReset = new TGTextButton(leftFrame, "&Reset Histos");
    btnReset->Connect("Clicked()", "DAQGUI", this, "ManualReset()");
    leftFrame->AddFrame(btnReset, btnL);

    TGTextButton *btnSave = new TGTextButton(leftFrame, "&Save Plots");
    btnSave->Connect("Clicked()", "DAQGUI", this, "SavePlots()");
    leftFrame->AddFrame(btnSave, btnL);

    TGTextButton *btnNext = new TGTextButton(leftFrame, "&Next Event");
    btnNext->Connect("Clicked()", "DAQGUI", this, "NextEvent()");
    leftFrame->AddFrame(btnNext, btnL);

    TGTextButton *btnAuto = new TGTextButton(leftFrame, "&DAQ Live Mode");
    btnAuto->Connect("Clicked()", "DAQGUI", this, "ToggleAuto()");
    leftFrame->AddFrame(btnAuto, btnL);

    fMainCanvas = new TRootEmbeddedCanvas("MainCanvas", mainFrame, 950, 850);
    mainFrame->AddFrame(
        fMainCanvas,
        new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 5, 5, 5));
    fMainCanvas->GetCanvas()->Divide(2, 2);

    LoadConfig();
    MapSubwindows();
    Resize(GetDefaultSize());
    MapWindow();

    Connect("CloseWindow()", "DAQGUI", this, "DoExit()");
    DontCallClose();
  }

  void DoExit() {
    SaveConfig();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    gApplication->Terminate(0);
  }

  void SaveConfig() {
    std::ofstream cfg("/tmp/.femdaq_config.txt",
                      std::ios::out | std::ios::trunc);
    if (cfg.is_open()) {
      cfg << (fCheckNextRun->GetState() == kButtonDown ? 1 : 0) << "\n";
      cfg << fBaseFileName.Data() << "\n" << fLastDecoFile.Data() << "\n";
      cfg << (int)fBaseStart->GetNumber() << "\n"
          << (int)fBaseEnd->GetNumber() << "\n";
      cfg << (int)fPulseStart->GetNumber() << "\n"
          << (int)fPulseEnd->GetNumber() << "\n";
      cfg << fSigmaThr->GetNumber() << std::endl;
      cfg << (int)fStepSize->GetNumber() << std::endl;
      cfg << (int)fSpecMaxEntry->GetNumber() << "\n";
      cfg.close();
    }
  }

  void LoadConfig() {
    std::ifstream cfg("/tmp/.femdaq_config.txt");
    if (cfg.is_open()) {
      std::string line;
      int autoRun = 1;
      if (std::getline(cfg, line))
        autoRun = std::stoi(line);

      fCheckNextRun->SetState(autoRun ? kButtonDown : kButtonUp);

      std::string dataPath, decoPath;
      std::getline(cfg, dataPath);
      std::getline(cfg, decoPath);

      if (std::getline(cfg, line))
        fBaseStart->SetNumber(std::stoi(line));
      if (std::getline(cfg, line))
        fBaseEnd->SetNumber(std::stoi(line));
      if (std::getline(cfg, line))
        fPulseStart->SetNumber(std::stoi(line));
      if (std::getline(cfg, line))
        fPulseEnd->SetNumber(std::stoi(line));
      if (std::getline(cfg, line))
        fSigmaThr->SetNumber(std::stod(line));
      if (std::getline(cfg, line))
        fStepSize->SetNumber(std::stoi(line));
      if (std::getline(cfg, line)) {
        fSpecMaxEntry->SetNumber(std::stoi(line));
        UpdateRange(0);
      }

      cfg.close();
      if (!decoPath.empty())
        LoadDecoding(decoPath.c_str());
      if (!dataPath.empty())
        LoadDataFile(dataPath.c_str());
    }
  }

  void GetParamsFromPulse(const std::vector<short> &p, double &amp,
                          double &area, int &mP) {
    area = 0;
    amp = 0;
    double bL = 0, bS = 0;
    int bS_idx = (int)fBaseStart->GetNumber();
    int bE_idx = (int)fBaseEnd->GetNumber();
    int pS_idx = (int)fPulseStart->GetNumber();
    int pE_idx = (int)fPulseEnd->GetNumber();
    double thrFactor = fSigmaThr->GetNumber();

    int nBase = bE_idx - bS_idx;
    if (nBase <= 0)
      return;

    nBase = 0;

    for (int i = bS_idx; i < bE_idx; i++) {
      if (p[i] == 0)
        continue;
      bL += p[i];
      bS += p[i] * p[i];
      nBase++;
    }

    if (nBase == 0) {
      auto it =
          std::find_if(p.begin(), p.end(), [](short val) { return val != 0; });
      if (it != p.end()) {
        bL = *it;
      } else {
        return;
      }
    } else {
      bL /= nBase;
      bS = sqrt(fabs(bS / nBase - bL * bL));
    }

    auto maxIt = std::max_element(p.begin() + pS_idx, p.begin() + pE_idx);
    mP = std::distance(p.begin(), maxIt);
    const double thr = thrFactor * bS;
    if (*maxIt < bL + thr)
      return;
    amp = *maxIt;
    for (int i = 0; i < (int)p.size(); i++) {
      double v = p[i] - bL;
      if (v > thr)
        area += v;
    }
  }

  void LoadDataFile(const char *path, bool newRun = true) {

    if (gSystem->AccessPathName(path))
      return;
    fBaseFileName = path;
    ParseRunSubrun(fBaseFileName, fCurrentRun, fCurrentSubrun);
    if (fReaderSignalsID) {
      delete fReaderSignalsID;
      fReaderSignalsID = nullptr;
    }
    if (fReaderPulses) {
      delete fReaderPulses;
      fReaderPulses = nullptr;
    }
    if (fReaderEventID) {
      delete fReaderEventID;
      fReaderEventID = nullptr;
    }
    if (fReaderTimestamp) {
      delete fReaderTimestamp;
      fReaderTimestamp = nullptr;
    }
    if (fReader) {
      delete fReader;
      fReader = nullptr;
    }
    if (fChain) {
      delete fChain;
      fChain = nullptr;
    }

    fChain = new TChain("SignalEvent");
    fChain->Add(fBaseFileName, -1);
    fChain->SetEntries(-1);
    fChain->GetEntries();

    fEntry = 0;

    if (newRun) {
      ManualReset();
    }

    fReader = new TTreeReader(fChain);
    fReaderSignalsID =
        new TTreeReaderValue<std::vector<int>>(*fReader, "signalsID");
    fReaderPulses =
        new TTreeReaderValue<std::vector<short>>(*fReader, "pulses");
    fReaderEventID = new TTreeReaderValue<int>(*fReader, "eventID");
    fReaderTimestamp = new TTreeReaderValue<double>(*fReader, "timestamp");

    fDataPathEntry->SetText(gSystem->BaseName(path));
    fDataPathEntry->SetToolTipText(path);
    SetWindowName(Form("DAQ Viewer - %s", gSystem->BaseName(path)));
    NextEvent();
  }

  void LoadDecoding(const char *path) {
    if (gSystem->AccessPathName(path))
      return;
    fDecodingMap = readDecoding(path);
    if (!fDecodingMap.empty()) {
      fLastDecoFile = path;

      for (auto const &[id, hitmap] : fHitMapMap)
        if (hitmap)
          delete hitmap;
      fHitMapMap.clear();

      for (auto const &[id, spectra] : fSpectraMap)
        if (spectra)
          delete spectra;
      fSpectraMap.clear();

      for (auto const &[id, stack] : fStacksMap)
        if (stack)
          delete stack;
      fStacksMap.clear();

      std::map<int, std::map<char, std::set<double>>> systemCoords;

      for (const auto &[ch, info] : fDecodingMap) {
        int sysID = 1;
        if (info.first.size() > 1)
          sysID = std::stoi(info.first.substr(1));
        char axis = info.first[0]; // 'X' or 'Y'
        systemCoords[sysID][axis].insert(info.second);
      }

      for (auto const &[sysID, axes] : systemCoords) {
        if (axes.count('X') == 0 || axes.count('Y') == 0)
          continue;

        const auto &x_set = axes.at('X');
        const auto &y_set = axes.at('Y');

        double minX = *x_set.begin(), maxX = *x_set.rbegin();
        double minY = *y_set.begin(), maxY = *y_set.rbegin();

        if (fHitMapMap.count(sysID)) {
          fHitMapMap[sysID]->SetBins(x_set.size(), minX, maxX, y_set.size(),
                                     minY, maxY);
        } else {
          fHitMapMap[sysID] =
              new TH2F(Form("hHit%d", sysID), Form("HitMap %d;X;Y", sysID),
                       x_set.size(), minX, maxX, y_set.size(), minY, maxY);
          fSpectraMap[sysID] =
              new TH1F(Form("hSpec%d", sysID), Form("Spectra %d", sysID), 2048,
                       0, fSpecMaxEntry->GetNumber());
        }
      }

      fNReadouts = fHitMapMap.size();

      fDecoPathEntry->SetText(gSystem->BaseName(path));
      fDecoPathEntry->SetToolTipText(path);
      UpdateCanvasLayout();
    }
  }

  void UpdateCanvasLayout() {
    TCanvas *c = fMainCanvas->GetCanvas();
    c->Clear();
    c->Divide(fNReadouts + 1, 2);
    DrawCanvas();
  }

  void DrawCanvas() {

    TCanvas *c = fMainCanvas->GetCanvas();

    int nCols = fNReadouts + 1;
    c->cd(1);
    bool first = true;
    int colorIdx = 1;
    for (auto const &[id, h] : fSpectraMap) {
      h->SetLineColor(colorIdx++);
      h->Draw(first ? "" : "same");
      first = false;
    }
    if (fSpectraMap.size() > 1)
      gPad->BuildLegend(0.7, 0.7, 0.9, 0.9);

    c->cd(nCols + 1);
    if (fRateGraph->GetN() > 0)
      fRateGraph->Draw("ALP");

    int col = 2;
    for (auto const &[id, h] : fHitMapMap) {
      c->cd(col);
      h->Draw("COLZ");

      c->cd(col + nCols);
      if (fStacksMap.count(id) && fStacksMap[id]) {
        fStacksMap[id]->Draw("nostack,l");
      }

      col++;
    }

    c->Modified();
    c->Update();
  }

  void NextEvent() {
    if (!fReader || !fChain)
      return;

    if (fChain->GetTree()) {
      fChain->GetTree()->Refresh();
      fChain->GetTree()->SetTreeIndex(0);
    }

    fChain->SetEntries(-1);
    Long64_t totalEntries = fChain->GetEntries();

    if (fEntry >= totalEntries) {
      if (fIsRunning) {
        fStatusLabel->SetText(
            Form("Status: Waiting DAQ... (Processed: %lld/%lld)", fEntry,
                 totalEntries));
      } else {
        fStatusLabel->SetText(
            Form("Status: End of File (%lld/%lld)", fEntry, totalEntries));
      }
      return;
    }

    const int stepSize = fStepSize->GetNumber();

    int eventsToProcess = (fIsRunning) ? stepSize : 1;
    if (fEntry + eventsToProcess >= totalEntries) {
      eventsToProcess = totalEntries - fEntry;
      // cout<<"Events to process "<<eventsToProcess <<" " <<totalEntries <<"
      // "<< fEntry<<endl;
    }

    Long64_t localEntry = fChain->LoadTree(fEntry + eventsToProcess - 1);
    if (localEntry < 0)
      return;
    if (fReader->SetLocalEntry(localEntry) != 0)
      return;

    for (int i = 0; i < eventsToProcess; ++i) {
      // Check if we reached the end of the available data
      if (fEntry >= totalEntries)
        break;

      localEntry = fChain->LoadTree(fEntry);

      if (localEntry < 0)
        break;

      if (fReader->SetLocalEntry(localEntry) != 0)
        break;

      fEventID = **fReaderEventID;
      fTimestamp = **fReaderTimestamp;
      fSignalsID = &(**fReaderSignalsID);
      fPulses = &(**fReaderPulses);

      // Rate logic
      if (fEntry == 0) {
        fTimeRateStart = fTimestamp;
        fRateGraph->SetPoint(fRatePoints, fTimestamp, 0.);
      } else if (fEntry % stepSize == 0) {
        double dt = fTimestamp - fTimeRateStart;
        if (dt > 0)
          fRateGraph->SetPoint(fRatePoints++,
                               (fTimestamp + fTimeRateStart) / 2.,
                               (double)stepSize / dt);
        fTimeRateStart = fTimestamp;
      }

      bool shouldDraw = (i == eventsToProcess - 1);
      if (shouldDraw) {
        ClearEvent();
        fStatusLabel->SetText(
            Form("Status: Running (Ev: %lld / %lld)", fEntry, totalEntries));
      }

      // ... [Rest of the analysis logic: Pulse loop, Spectra fill, HitMap fill]
      struct ReadoutSystem {
        double ampX = 0, ampY = 0, posX = 0, posY = 0, totAmp = 0;
      };
      std::map<int, ReadoutSystem> eventSystems;

      for (size_t s = 0; s < fSignalsID->size(); s++) {
        int sID = fSignalsID->at(s);
        std::vector<short> pulse(fPulses->begin() + s * 512,
                                 fPulses->begin() + s * 512 + 512);
        double amp, area;
        int maxP;
        GetParamsFromPulse(pulse, amp, area, maxP);

        int sysID = 1; // Por defecto todo al sistema 1
        double pos = -1.0;
        char axis = 'N'; // 'N' de None/No definido

        // Intentamos obtener datos del decoding si existe
        if (!fDecodingMap.empty()) {
          auto it = fDecodingMap.find(sID);
          if (it != fDecodingMap.end()) {
            const std::string &label = it->second.first;
            sysID = (label.size() == 1) ? 1 : std::stoi(label.substr(1));
            axis = label[0];
            pos = it->second.second;
          }
        }

        if (shouldDraw) {
          if (!fStacksMap[sysID])
            fStacksMap[sysID] =
                new THStack(Form("hs%d", sysID), Form("Event %d", fEventID));

          TH1S *h = new TH1S(Form("h_s%d", sID), "", 512, 0, 512);
          for (int p = 0; p < 512; p++)
            h->SetBinContent(p + 1, pulse[p]);
          h->SetLineColor((sID % 72) + 1);
          fHistos.push_back(h);
          fStacksMap[sysID]->Add(h);
        }

        if (area > 0 && amp > 0) {
          auto &sys = eventSystems[sysID];
          sys.totAmp += amp;

          if (axis == 'X') {
            sys.posX += amp * pos;
            sys.ampX += amp;
          } else if (axis == 'Y') {
            sys.posY += amp * pos;
            sys.ampY += amp;
          }
        }
      }

      for (auto &[sysID, data] : eventSystems) {
        if (fSpectraMap.count(sysID))
          fSpectraMap[sysID]->Fill(data.totAmp);
        if (fHitMapMap.count(sysID) && data.ampX > 0 && data.ampY > 0) {
          fHitMapMap[sysID]->Fill(data.posX / data.ampX, data.posY / data.ampY);
        }
      }

      fEntry++;
    }

    // Update the canvas to show the latest processed data
    DrawCanvas();
  }

  void UpdateRange(Long_t) {
    double newMax = fSpecMaxEntry->GetNumber();
    std::cout << "Range updated " << newMax << std::endl;

    if (fSpectraMap.empty())
      return;

    for (auto const &[id, spec] : fSpectraMap) {
      spec->SetBins(2048, 0, newMax);
      spec->Reset();
    }

    if (fMainCanvas) {
      TCanvas *c = fMainCanvas->GetCanvas();
      c->cd(1);

      bool first = true;
      for (auto const &[id, spec] : fSpectraMap) {
        spec->Draw(first ? "HIST" : "HIST SAME");
        first = false;
      }

      gPad->Modified();
      gPad->Update();
    }
  }

  void ManualReset() {
    for (auto const &[id, spec] : fSpectraMap) {
      spec->Reset();
    }

    for (auto const &[id, hmap] : fHitMapMap) {
      hmap->Reset();
    }

    fRateGraph->Set(0);
    fRateGraph->SetPoint(0, 0, 0);
    fRatePoints = 0;
    fTimeRateStart = -1;
    fMainCanvas->GetCanvas()->Update();
  }

  void SavePlots() {
    TString outName = Form("Plots_Run%05d_Ev%lld.pdf", fCurrentRun, fEntry);
    fMainCanvas->GetCanvas()->SaveAs(outName);
  }

  void OpenFile() {
    TGFileInfo fi;
    const char *ft[] = {"ROOT", "*.root", 0, 0};
    fi.fFileTypes = ft;
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);
    if (fi.fFilename)
      LoadDataFile(fi.fFilename);
  }

  void ReloadRun() {
    if (fBaseFileName == "")
      return;
    CheckNewFile(0);
  }

  void OpenDecoding() {
    TGFileInfo fi;
    const char *ft[] = {"ALLFILES", "*", 0, 0};
    fi.fFileTypes = ft;
    new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi);
    if (fi.fFilename)
      LoadDecoding(fi.fFilename);
  }

  void ToggleAuto() {
    if (!fIsRunning) {
      fIsRunning = true;
      fTimer->Start(100, kFALSE);
      fWatchdogTimer->Start(5000, kFALSE);
      ((TGTextButton *)gTQSender)->SetText("Stop Monitor");
    } else {
      fIsRunning = false;
      fTimer->Stop();
      fWatchdogTimer->Stop();
      ((TGTextButton *)gTQSender)->SetText("&DAQ Live Mode");
    }
  }

  void CheckForNewData() {
    if (!fChain || !fIsRunning || fCheckNextRun->GetState() != kButtonDown)
      return;

    // Refresh current entries
    if (fEntry < fChain->GetEntries())
      return;

    CheckNewFile();
  }

  void CheckNewFile(int offset = 1) {

    // Extract directory and build search patterns
    TString dirName = gSystem->DirName(fBaseFileName);
    cout << dirName << endl;
    void *dir = gSystem->OpenDirectory(dirName);
    if (!dir)
      return;

    TString nextSubrunPattern;
    nextSubrunPattern.Form("Run%05d_.*_%03d.root", fCurrentRun,
                           fCurrentSubrun + 1);

    TString nextRunPattern;
    nextRunPattern.Form("Run%05d_.*_001.root", fCurrentRun + offset);

    const char *entry;
    TString foundNextSubrun = "";
    TString foundNextRun = "";

    // Scan directory for matches
    TPRegexp reSub(nextSubrunPattern);
    TPRegexp reRun(nextRunPattern);

    while ((entry = gSystem->GetDirEntry(dir))) {
      TString fileName = entry;

      // TPRegexp::Match returns the number of matches found
      if (reSub.Match(fileName) > 0) {
        foundNextSubrun = dirName + "/" + fileName;
        break; // Subrun takes priority
      }
      if (reRun.Match(fileName) > 0) {
        foundNextRun = dirName + "/" + fileName;
      }
    }
    gSystem->FreeDirectory(dir);

    // Load found files
    if (foundNextSubrun != "") {
      std::cout << ">>> New Subrun found: " << foundNextSubrun << std::endl;
      fCurrentSubrun++;
      fBaseFileName = foundNextSubrun;
      LoadDataFile(foundNextSubrun, false);
      fDataPathEntry->SetText(gSystem->BaseName(fBaseFileName));
      fDataPathEntry->SetToolTipText(fBaseFileName);

    } else if (foundNextRun != "") {
      std::cout << ">>> New Run detected: " << foundNextRun << std::endl;
      ManualReset(); // Reset histograms for new main Run
      fCurrentRun += offset;
      fCurrentSubrun = 1;
      fBaseFileName = foundNextRun;
      LoadDataFile(foundNextRun);
    }
  }

  void ClearEvent() {
    for (auto const &[id, stack] : fStacksMap) {
      if (stack) {
        delete stack;
      }
    }
    fStacksMap.clear();

    for (auto h : fHistos) {
      if (h)
        delete h;
    }
    fHistos.clear();
  }

  void ParseRunSubrun(TString fn, int &r, int &s) {
    // Matches "Run" + digits + anything + "_" + digits + ".root"
    TObjArray *sa =
        TPRegexp("Run(\\d+).+_(\\d+)\\.root$").MatchS(gSystem->BaseName(fn));
    if (sa && sa->GetEntries() > 2) {
      r = ((TObjString *)sa->At(1))->GetString().Atoi();
      s = ((TObjString *)sa->At(2))->GetString().Atoi();
    }
    if (sa)
      delete sa;
  }

  std::map<int, std::pair<std::string, double>>
  readDecoding(const std::string &filedec) {
    std::map<int, std::pair<std::string, double>> dec;
    std::ifstream fdec(filedec);
    int ch;
    std::string axis;
    double pos;
    while (fdec >> ch >> axis >> pos)
      dec[ch] = std::make_pair(axis, pos);
    return dec;
  }

  virtual ~DAQGUI() {
    ClearEvent();
    if (fReader)
      delete fReader;
    if (fChain)
      delete fChain;
    if (fTimer)
      delete fTimer;
    if (fWatchdogTimer)
      delete fWatchdogTimer;
  }
  ClassDef(DAQGUI, 0)
};

void EventDisplay() { new DAQGUI(); }

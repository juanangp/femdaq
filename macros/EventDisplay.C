#include <TGClient.h>
#include <TGButton.h>
#include <TGLabel.h>
#include <TGTextEntry.h>
#include <TGNumberEntry.h>
#include <TRootEmbeddedCanvas.h>
#include <TGFileDialog.h>
#include <TCanvas.h>
#include <TFile.h>
#include <TTree.h>
#include <TChain.h>
#include <TTreeReader.h>
#include <TH1.h>
#include <TH2.h>
#include <TGraph.h>
#include <THStack.h>
#include <TSystem.h>
#include <TTimer.h>
#include <TPRegexp.h>
#include <TObjString.h>
#include <TObjArray.h>
#include <TApplication.h>
#include <TString.h>
#include <vector>
#include <map>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>

class DAQGUI : public TGMainFrame {
private:
    TRootEmbeddedCanvas *fMainCanvas;
    TGTextEntry         *fDataPathEntry;
    TGTextEntry         *fDecoPathEntry;
    TGLabel             *fStatusLabel;
    TGCheckButton       *fCheckNextRun;

    // Pulse Parameter Entries
    TGNumberEntry *fBaseStart, *fBaseEnd;
    TGNumberEntry *fPulseStart, *fPulseEnd;
    TGNumberEntry *fSigmaThr;
    TGNumberEntry *fStepSize;
    
    TChain              *fChain = nullptr;
    TTreeReader         *fReader = nullptr;
    std::map<int, int>   fDecodingMap;
    TTimer              *fTimer;
    TTimer              *fWatchdogTimer;
    
    // Lectores para tus ramas específicas
    TTreeReaderValue<std::vector<int>>   *fReaderSignalsID = nullptr;
    TTreeReaderValue<std::vector<short>> *fReaderPulses = nullptr;
    TTreeReaderValue<int>                *fReaderEventID = nullptr;
    TTreeReaderValue<double>             *fReaderTimestamp = nullptr;
    
    int                  fEventID;
    double               fTimestamp;
    std::vector<int>    *fSignalsID = nullptr;
    std::vector<short>  *fPulses = nullptr;
    
    Long64_t             fEntry = 0;
    int                  fNChannels = 1;
    double               fTimeRateStart = -1;
    int                  fRatePoints = 0;
    bool                 fIsRunning = false;
    
    int                  fCurrentRun = -1;
    int                  fCurrentSubrun = -1;
    TString              fBaseFileName = "";
    TString              fLastDecoFile = "";

    // Default Pulse Params
    int fBLS = 10, fBLE = 110, fPS = 100, fPE = 500;
    double fSThr = 7.0;
    int fSSz = 100;

    TH1F *fSpectra = nullptr;
    TH2F *fHitMap = nullptr;
    TGraph   *fRateGraph;
    THStack  *fStacks = nullptr;
    std::vector<TH1S*> fHistos;

public:
    DAQGUI() : TGMainFrame(gClient->GetRoot(), 1200, 900) {
        
        fSpectra = new TH1F("Spectra", "Total Area;Area;Counts", 2048, 0, 1000000);
        fHitMap  = new TH2F("HitMap", "HitMap;X;Y", 1, 0, 1, 1, 0, 1);
        fRateGraph = new TGraph();
        fRateGraph->SetTitle("Rate;Time [s];Rate [Hz]");
        fRateGraph->SetMarkerStyle(20);
        fRateGraph->SetMarkerSize(0.6);
        fRateGraph->SetLineColor(kAzure+1);
        fRateGraph->GetXaxis()->SetTimeDisplay(1);

        fTimer = new TTimer();
        fTimer->Connect("Timeout()", "DAQGUI", this, "NextEvent()");
        fWatchdogTimer = new TTimer();
        fWatchdogTimer->Connect("Timeout()", "DAQGUI", this, "CheckForNewData()");

        TGHorizontalFrame *mainFrame = new TGHorizontalFrame(this, 1200, 900);
        AddFrame(mainFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        TGVerticalFrame *leftFrame = new TGVerticalFrame(mainFrame, 240, 900);
        mainFrame->AddFrame(leftFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 5, 5, 5, 5));

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
        fBaseStart = new TGNumberEntry(fBL, fBLS, 5, -1, TGNumberFormat::kNESInteger);
        fBaseEnd   = new TGNumberEntry(fBL, fBLE, 5, -1, TGNumberFormat::kNESInteger);
        fBL->AddFrame(fBaseStart, btnL); fBL->AddFrame(fBaseEnd, btnL);
        leftFrame->AddFrame(fBL, btnL);

        leftFrame->AddFrame(new TGLabel(leftFrame, "Pulse Start/End:"), labL);
        TGHorizontalFrame *fPSect = new TGHorizontalFrame(leftFrame);
        fPulseStart = new TGNumberEntry(fPSect, fPS, 5, -1, TGNumberFormat::kNESInteger);
        fPulseEnd   = new TGNumberEntry(fPSect, fPE, 5, -1, TGNumberFormat::kNESInteger);
        fPSect->AddFrame(fPulseStart, btnL); fPSect->AddFrame(fPulseEnd, btnL);
        leftFrame->AddFrame(fPSect, btnL);

        leftFrame->AddFrame(new TGLabel(leftFrame, "Sigma Threshold:"), labL);
        fSigmaThr = new TGNumberEntry(leftFrame, fSThr, 5, -1, TGNumberFormat::kNESRealOne);
        leftFrame->AddFrame(fSigmaThr, btnL);

        leftFrame->AddFrame(new TGLabel(leftFrame, "Step Size"), labL);
        fStepSize = new TGNumberEntry(leftFrame, fSSz, 5, -1, TGNumberFormat::kNESInteger);
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
        mainFrame->AddFrame(fMainCanvas, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 0, 5, 5, 5));
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
        std::ofstream cfg("/tmp/.femdaq_config.txt", std::ios::out | std::ios::trunc);
        if (cfg.is_open()) {
            cfg << (fCheckNextRun->GetState() == kButtonDown ? 1 : 0) << "\n";
            cfg << fBaseFileName.Data() << "\n" << fLastDecoFile.Data() << "\n";
            cfg << (int)fBaseStart->GetNumber() << "\n" << (int)fBaseEnd->GetNumber() << "\n";
            cfg << (int)fPulseStart->GetNumber() << "\n" << (int)fPulseEnd->GetNumber() << "\n";
            cfg << fSigmaThr->GetNumber() << std::endl;
            cfg << (int)fStepSize->GetNumber() << std::endl;
            cfg.close();
        }
    }

    void LoadConfig() {
        std::ifstream cfg("/tmp/.femdaq_config.txt");
        if (cfg.is_open()) {
            std::string line;
            int autoRun = 1;
            if (std::getline(cfg, line)) autoRun = std::stoi(line);
            
            fCheckNextRun->SetState(autoRun ? kButtonDown : kButtonUp);

            std::string dataPath, decoPath;
            std::getline(cfg, dataPath);
            std::getline(cfg, decoPath);

            if (std::getline(cfg, line)) fBaseStart->SetNumber(std::stoi(line));
            if (std::getline(cfg, line)) fBaseEnd->SetNumber(std::stoi(line));
            if (std::getline(cfg, line)) fPulseStart->SetNumber(std::stoi(line));
            if (std::getline(cfg, line)) fPulseEnd->SetNumber(std::stoi(line));
            if (std::getline(cfg, line)) fSigmaThr->SetNumber(std::stod(line));
            if (std::getline(cfg, line)) fStepSize->SetNumber(std::stoi(line));

            cfg.close();
            if (!decoPath.empty()) LoadDecoding(decoPath.c_str());
            if (!dataPath.empty()) LoadDataFile(dataPath.c_str());
        }
    }

    void GetParamsFromPulse(const std::vector<short> &p, double &amp, double &area, int &mP) {
        area = 0; amp = 0; double bL = 0, bS = 0;
        int bS_idx = (int)fBaseStart->GetNumber();
        int bE_idx = (int)fBaseEnd->GetNumber();
        int pS_idx = (int)fPulseStart->GetNumber();
        int pE_idx = (int)fPulseEnd->GetNumber();
        double thrFactor = fSigmaThr->GetNumber();

        int nBase = bE_idx - bS_idx;
        if (nBase <= 0) return;

        for (int i = bS_idx; i < bE_idx; i++) { bL += p[i]; bS += p[i] * p[i]; }
        bL /= nBase; 
        bS = sqrt(fabs(bS / nBase - bL * bL));

        auto maxIt = std::max_element(p.begin() + pS_idx, p.begin() + pE_idx);
        mP = std::distance(p.begin(), maxIt);
        const double thr = thrFactor * bS;
        if (*maxIt < bL + thr) return;
        amp = *maxIt;
        for (int i = 0; i < (int)p.size(); i++) { 
            double v = p[i] - bL; 
            if (v > thr) area += v; 
        }
    }

    void LoadDataFile(const char* path) {
        if (gSystem->AccessPathName(path)) return;
        fBaseFileName = path;
        ParseRunSubrun(fBaseFileName, fCurrentRun, fCurrentSubrun);
        if (fReaderSignalsID) { delete fReaderSignalsID; fReaderSignalsID = nullptr; }
        if (fReaderPulses)    { delete fReaderPulses;    fReaderPulses = nullptr; }
        if (fReaderEventID)   { delete fReaderEventID;   fReaderEventID = nullptr; }
        if (fReaderTimestamp) { delete fReaderTimestamp; fReaderTimestamp = nullptr; }
        if (fReader)          { delete fReader;          fReader = nullptr; }
        if (fChain)           { delete fChain;           fChain = nullptr; }
    
        fChain = new TChain("SignalEvent"); fChain->Add(fBaseFileName);
        fEntry = 0; fRatePoints = 0; fTimeRateStart = -1;
        
        fReader = new TTreeReader(fChain);
        fReaderSignalsID = new TTreeReaderValue<std::vector<int>>(*fReader, "signalsID");
        fReaderPulses    = new TTreeReaderValue<std::vector<short>>(*fReader, "pulses");
        fReaderEventID   = new TTreeReaderValue<int>(*fReader, "eventID");
        fReaderTimestamp = new TTreeReaderValue<double>(*fReader, "timestamp");
    
        fDataPathEntry->SetText(gSystem->BaseName(path));
        fDataPathEntry->SetToolTipText(path);
        SetWindowName(Form("DAQ Viewer - %s", gSystem->BaseName(path)));
        NextEvent();
    }

    void LoadDecoding(const char* path) {
        if (gSystem->AccessPathName(path)) return;
        fDecodingMap = readDecoding(path);
        if(!fDecodingMap.empty()) {
            fLastDecoFile = path;
            fNChannels = fDecodingMap.size()/2;
            fHitMap->SetBins(fNChannels, 0, fNChannels, fNChannels, 0, fNChannels);
            fDecoPathEntry->SetText(gSystem->BaseName(path));
            fDecoPathEntry->SetToolTipText(path);
        }
    }

    void NextEvent() {
        if (!fReader) return;

        fChain->SetEntries(-1); 
        Long64_t totalEntries = fChain->GetEntries(); 

        if (fEntry >= totalEntries){
         if (fIsRunning) {
           fStatusLabel->SetText(Form("Status: Waiting DAQ... (Processed: %lld/%lld)", fEntry, totalEntries));
         } else {
           fStatusLabel->SetText(Form("Status: End of File (%lld/%lld)", fEntry, totalEntries));
         }
         return;
       }

        const int stepSize = fStepSize->GetNumber();

        int eventsToProcess = (fIsRunning) ? stepSize : 1;
        if(fEntry + eventsToProcess >= totalEntries){
           eventsToProcess = totalEntries - fEntry;
           //cout<<"Events to process "<<eventsToProcess <<" " <<totalEntries <<" "<< fEntry<<endl;
        }

        Long64_t localEntry = fChain->LoadTree(fEntry + eventsToProcess -1);
        if (localEntry < 0) return;
        if ( fReader->SetLocalEntry(localEntry)!= 0)return;

        for (int i = 0; i < eventsToProcess; ++i) {
            // Check if we reached the end of the available data
            if (fEntry >= totalEntries) break;
            
            localEntry = fChain->LoadTree(fEntry);

            if (localEntry < 0) break;

            if ( fReader->SetLocalEntry(localEntry)!= 0)break;

            fEventID   = **fReaderEventID;
            fTimestamp = **fReaderTimestamp;
            fSignalsID = &(**fReaderSignalsID); 
            fPulses    = &(**fReaderPulses);
            
            // Rate logic
            if (fEntry == 0) {
                fTimeRateStart = fTimestamp; 
                fRateGraph->SetPoint(fRatePoints, fTimestamp, 0.); 
            } else if (fEntry % stepSize == 0) {
                double dt = fTimestamp - fTimeRateStart;
                if (dt > 0) fRateGraph->SetPoint(fRatePoints++, fTimestamp, (double)stepSize / dt);
                fTimeRateStart = fTimestamp;
            }

            bool shouldDraw = (i == eventsToProcess - 1);
            if (shouldDraw) {
                ClearEvent();
                fStatusLabel->SetText(Form("Status: Running (Ev: %lld / %lld)", fEntry, totalEntries));
            }

            // ... [Rest of the analysis logic: Pulse loop, Spectra fill, HitMap fill] ...
            double totArea = 0, areaX = 0, areaY = 0, posX = 0, posY = 0;
            for (size_t s = 0; s < fSignalsID->size(); s++) {
                int sID = fSignalsID->at(s);
                std::vector<short> pulse(fPulses->begin() + s*512, fPulses->begin() + s*512 + 512);
                double amp, area; int maxP;
                GetParamsFromPulse(pulse, amp, area, maxP);
                if (area > 0 && amp > 0) {
                    totArea += area;
                    auto it = fDecodingMap.find(sID);
                    if (it != fDecodingMap.end()) {
                        int pos = it->second; int ch = pos % fNChannels;
                        if (pos / fNChannels == 0) { posX += area * ch; areaX += area; }
                        else { posY += area * ch; areaY += area; }
                    }
                }

                if (shouldDraw){
                  if (!fStacks) fStacks = new THStack("Pulses", Form("Event %d", fEventID));
                  TH1S *h = new TH1S(Form("h_s%d", sID), "", 512, 0, 512);
                  for(int p=0; p<512; p++) h->SetBinContent(p+1, pulse[p]);
                  h->SetLineColor((sID % 72) + 1);
                  fHistos.push_back(h); fStacks->Add(h);
               }
            }
            if(totArea)fSpectra->Fill(totArea);
            if (areaX > 0 && areaY > 0) fHitMap->Fill(posX / areaX, posY / areaY);
            
            fEntry++;
        }

        // Update the canvas to show the latest processed data
        TCanvas *can = fMainCanvas->GetCanvas();
        can->cd(1); fSpectra->Draw();
        can->cd(2); fHitMap->Draw("COLZ");
        can->cd(3); fRateGraph->Draw("ALP");
        can->cd(4); if(fStacks) fStacks->Draw("nostack,l");
        can->Update();
    }

    void ManualReset() {
        fSpectra->Reset(); fHitMap->Reset(); fRateGraph->Set(0);
        fRateGraph->SetPoint(0,0,0); fRatePoints = 0; fTimeRateStart = -1;
        fMainCanvas->GetCanvas()->Update();
    }

    void SavePlots() {
        TString outName = Form("Plots_Run%05d_Ev%lld.pdf", fCurrentRun, fEntry);
        fMainCanvas->GetCanvas()->SaveAs(outName);
    }

    void OpenFile() { TGFileInfo fi; const char *ft[] = {"ROOT", "*.root", 0,0}; fi.fFileTypes = ft; new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi); if(fi.fFilename) LoadDataFile(fi.fFilename); }
    void OpenDecoding() { TGFileInfo fi; const char *ft[] = {"ALLFILES", "*", 0,0}; fi.fFileTypes = ft; new TGFileDialog(gClient->GetRoot(), this, kFDOpen, &fi); if(fi.fFilename) LoadDecoding(fi.fFilename); }
    
    void ToggleAuto() { if(!fIsRunning) { fIsRunning = true; fTimer->Start(100, kFALSE); fWatchdogTimer->Start(30000, kFALSE); ((TGTextButton*)gTQSender)->SetText("Stop Monitor"); } else { fIsRunning = false; fTimer->Stop(); fWatchdogTimer->Stop(); ((TGTextButton*)gTQSender)->SetText("&DAQ Live Mode"); } }
    
    void CheckForNewData() {
    if (!fChain || !fIsRunning || fCheckNextRun->GetState() != kButtonDown) return;

    // 1. Refresh current entries
    fChain->GetEntries(); 

    // 2. Extract directory and build search patterns
    TString dirName = gSystem->DirName(fBaseFileName);
    void *dir = gSystem->OpenDirectory(dirName);
    if (!dir) return;

    TString nextSubrunPattern; 
    nextSubrunPattern.Form("Run%05d_.*_%03d.root", fCurrentRun, fCurrentSubrun + 1);
    
    TString nextRunPattern;
    nextRunPattern.Form("Run%05d_.*_001.root", fCurrentRun + 1);

    const char *entry;
    TString foundNextSubrun = "";
    TString foundNextRun = "";

    // 3. Scan directory for matches
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

    // 4. Load found files
    if (foundNextSubrun != "") {
        std::cout << ">>> New Subrun found: " << foundNextSubrun << std::endl;
        fCurrentSubrun++;
        fBaseFileName = foundNextSubrun;
        fChain->Add(fBaseFileName);
        fDataPathEntry->SetText(gSystem->BaseName(fBaseFileName));
        fDataPathEntry->SetToolTipText(fBaseFileName);
        
    } 
    else if (foundNextRun != "") {
        std::cout << ">>> New Run detected: " << foundNextRun << std::endl;
        ManualReset(); // Reset histograms for new main Run
        fCurrentRun++;
        fCurrentSubrun = 1;
        fBaseFileName = foundNextRun;
        fChain->Add(fBaseFileName);
        fDataPathEntry->SetText(gSystem->BaseName(fBaseFileName));
        fDataPathEntry->SetToolTipText(fBaseFileName);
    }
}


    void ClearEvent() { if(fStacks) { delete fStacks; fStacks = nullptr; } for(auto h : fHistos) delete h; fHistos.clear(); }

    void ParseRunSubrun(TString fn, int &r, int &s) {
    // Matches "Run" + digits + anything + "_" + digits + ".root"
    TObjArray *sa = TPRegexp("Run(\\d+).+_(\\d+)\\.root$").MatchS(gSystem->BaseName(fn));
    if (sa && sa->GetEntries() > 2) {
        r = ((TObjString*)sa->At(1))->GetString().Atoi();
        s = ((TObjString*)sa->At(2))->GetString().Atoi();
    }
    if (sa) delete sa;
}

    std::map<int,int> readDecoding(std::string f) { std::map<int,int> m; std::ifstream i(f); int s,p; while(i>>s>>p) m[s]=p; return m; }
    virtual ~DAQGUI() { ClearEvent(); if(fChain) delete fChain; if(fTimer) delete fTimer; if(fWatchdogTimer) delete fWatchdogTimer; }
    ClassDef(DAQGUI, 0)
};

void EventDisplay() { new DAQGUI(); }


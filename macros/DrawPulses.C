#include <TChain.h>
#include <TCanvas.h>
#include <THStack.h>
#include <TH1S.h>
#include <TGFrame.h>
#include <TGButton.h>
#include <TGTextEntry.h>
#include <TGLabel.h>
#include <TGFileDialog.h>
#include <TSystem.h>
#include <TRootEmbeddedCanvas.h>
#include <TStyle.h>
#include <iostream>
#include <vector>

class EventViewer : public TGMainFrame {
private:
    TChain* chain = nullptr;
    TRootEmbeddedCanvas* fEcan;
    TGTextEntry* fEntryInput;
    TGLabel* fInfoLabel;
    
    long currentEntry = 0;
    TString currentFileName = "No file loaded";
    int eventID;
    std::vector<int>* signalsID = nullptr;
    std::vector<short>* pulses = nullptr;

public:
    EventViewer() : TGMainFrame(gClient->GetDefaultRoot(), 1200, 900) {
        SetCleanup(kDeepCleanup);

        // --- Main Layout ---
        TGHorizontalFrame* hFrame = new TGHorizontalFrame(this);
        
        // --- Control Panel (Left) ---
        TGVerticalFrame* vFrame = new TGVerticalFrame(hFrame, 150, 900, kFixedWidth);
        
        TGTextButton* btnOpen = new TGTextButton(vFrame, "&Open File");
        btnOpen->Connect("Clicked()", "EventViewer", this, "OpenFile()");
        vFrame->AddFrame(btnOpen, new TGLayoutHints(kLHintsExpandX, 5, 5, 5, 5));

        TGTextButton* btnPrev = new TGTextButton(vFrame, "&Prev Event");
        btnPrev->Connect("Clicked()", "EventViewer", this, "Prev()");
        vFrame->AddFrame(btnPrev, new TGLayoutHints(kLHintsExpandX, 5, 5, 5, 5));

        TGTextButton* btnNext = new TGTextButton(vFrame, "&Next Event");
        btnNext->Connect("Clicked()", "EventViewer", this, "Next()");
        vFrame->AddFrame(btnNext, new TGLayoutHints(kLHintsExpandX, 5, 5, 5, 5));

        // Event Input Section
        vFrame->AddFrame(new TGLabel(vFrame, "Jump to Event:"), new TGLayoutHints(kLHintsCenterX, 5, 5, 10, 0));
        fEntryInput = new TGTextEntry(vFrame);
        fEntryInput->Connect("ReturnPressed()", "EventViewer", this, "JumpToEvent()");
        vFrame->AddFrame(fEntryInput, new TGLayoutHints(kLHintsExpandX, 5, 5, 2, 5));

        fInfoLabel = new TGLabel(vFrame, "No data");
        vFrame->AddFrame(fInfoLabel, new TGLayoutHints(kLHintsCenterX, 5, 5, 20, 5));

        TGTextButton* btnSave = new TGTextButton(vFrame, "&Save PDF");
        btnSave->Connect("Clicked()", "EventViewer", this, "SavePDF()");
        vFrame->AddFrame(btnSave, new TGLayoutHints(kLHintsExpandX, 5, 5, 40, 5));

        TGTextButton* btnQuit = new TGTextButton(vFrame, "&Quit");
        btnQuit->SetCommand("gApplication->Terminate()");
        vFrame->AddFrame(btnQuit, new TGLayoutHints(kLHintsExpandX, 5, 5, 10, 5));

        hFrame->AddFrame(vFrame, new TGLayoutHints(kLHintsLeft | kLHintsExpandY, 5, 5, 5, 5));

        // --- Canvas Area (Right) ---
        fEcan = new TRootEmbeddedCanvas("Ecan", hFrame, 1000, 850);
        hFrame->AddFrame(fEcan, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY, 5, 5, 5, 5));
        fEcan->GetCanvas()->Divide(2, 2);

        AddFrame(hFrame, new TGLayoutHints(kLHintsExpandX | kLHintsExpandY));

        SetWindowName("TRest Signal Browser");
        MapSubwindows();
        Layout();
        MapWindow();
        
        gStyle->SetOptStat(0);
    }

    void OpenFile() {
        static TString dir(".");
        TGFileInfo fi;
        const char *ft[] = {"ROOT", "*.root", 0,0};
        fi.fFileTypes = ft;
        fi.fIniDir = StrDup(dir);
        new TGFileDialog(gClient->GetDefaultRoot(), this, kFDOpen, &fi);
        
        if (fi.fFilename) {
            currentFileName = gSystem->BaseName(fi.fFilename);
            if(chain) delete chain;
            chain = new TChain("SignalEvent");
            chain->Add(fi.fFilename);
            chain->SetBranchAddress("eventID", &eventID);
            chain->SetBranchAddress("signalsID", &signalsID);
            chain->SetBranchAddress("pulses", &pulses);
            currentEntry = 0;
            DrawEvent();
            SetWindowName(currentFileName);
        }
    }

    void Next() { if (chain && currentEntry < chain->GetEntries()-1) { currentEntry++; DrawEvent(); } }
    void Prev() { if (chain && currentEntry > 0) { currentEntry--; DrawEvent(); } }

    void JumpToEvent() {
        if (!chain) return;
        long val = atol(fEntryInput->GetText());
        if (val >= 0 && val < chain->GetEntries()) {
            currentEntry = val;
            DrawEvent();
        } else {
            fEntryInput->SetText(Form("%ld", currentEntry)); // Reset on error
        }
    }

    void SavePDF() {
        if (chain) fEcan->GetCanvas()->SaveAs(Form("Event_%ld.pdf", currentEntry));
    }

    void DrawEvent() {
        if (!chain) return;
        chain->GetEntry(currentEntry);
        TCanvas *can = fEcan->GetCanvas();
        
        fInfoLabel->SetText(Form("Entry: %ld / %lld", currentEntry, chain->GetEntries()-1));
        
        for(int i=1; i<=4; i++) can->cd(i)->Clear();

        std::vector<THStack*> hs;
        for(int i=0; i<4; i++) {
            THStack* s = new THStack(Form("ASIC%d", i), Form("ASIC %d", i) );
            hs.push_back(s);
        }

        for (size_t s = 0; s < signalsID->size(); s++) {
            int card = (signalsID->at(s) % 288) / 72;
            int channel = signalsID->at(s) % 72;
            
            // Unique name and SetDirectory(0) to avoid "Replacing existing TH1" warnings
            TH1S *h = new TH1S(Form("h_%zu_e%ld", s, currentEntry), "", 512, 0, 512);
            h->SetDirectory(0); 

            for(int p=0; p<512; p++) h->SetBinContent(p+1, pulses->at(s*512 + p));
            h->SetLineColor(channel + 1);
            if(card < 4) hs[card]->Add(h, "lp");
        }

        for(int i=0; i<4; i++) {
            can->cd(i+1);
            hs[i]->Draw("nostack,lp");
            gPad->BuildLegend(0.05, 0.05, 0.3, 0.3);
        }
        can->Update();
    }
};

// Start macro
void DrawPulses(const std::string &fileName = "") {
    new EventViewer();
}


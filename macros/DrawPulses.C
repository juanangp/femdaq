
//#include "SignalEvent.h"

void DrawPulses(const std::string &fileName){

//SignalEvent sEvent;

auto chain = std::make_unique<TChain>("SignalEvent");

chain->Add(fileName.c_str(), -1);

int eventID;
double timestamp = 0;
std::vector<int>* signalsID = nullptr; 
std::vector<short>* pulses = nullptr;

chain->SetBranchAddress("eventID", &eventID);
chain->SetBranchAddress("timestamp", &timestamp);
chain->SetBranchAddress("signalsID", &signalsID);
chain->SetBranchAddress("pulses", &pulses);

std::vector<THStack *> hs;

TCanvas *can = new TCanvas("c","",800,600);
can->Divide(2,2);
const size_t entries = chain->GetEntries();
cout<<"Entries "<<entries<<endl;
  for(size_t entryID = 0; entryID < entries; entryID++){
    std::vector<TH1S *> histos;
    chain->GetEntry(entryID);
    for(int i=0;i<4;i++){
      auto h = new THStack( );
      hs.emplace_back(h);
    }

        for (size_t s = 0; s<signalsID->size();s++){
          const int signalID = signalsID->at(s);
          const int card = signalID/72;
          const int channel = signalID%72;
          const size_t offset = s * 512;
          std::vector<short> pulse(pulses->begin() + offset, pulses->begin() + offset + 512);
          std::string hName = "Hist_"+std::to_string(signalID);
          auto histo = new TH1S (hName.c_str(),hName.c_str(),pulse.size(),0.,pulse.size());
            for(int p=0;p<pulse.size();p++){
              //cout<<p<<" "<<pulse[p]<<std::endl;
              histo->SetBinContent(p+1,pulse.at(p) );
            }
          short max = *std::max_element(pulse.begin(), pulse.end());
          if(max > 500)std::cout<<"Card "<<card<<" channel "<<channel<<" max "<<max<<endl;
          histo->SetLineColor(channel);
          histo->SetMarkerColor(channel);
          histos.emplace_back(histo);
          hs[card]->Add(histos.back(),hName.c_str());
        }
    can->cd();
    for(int i=0;i<4;i++){
      can->cd(i+1);
      hs[i]->Draw("nostack,lp");
      gPad->BuildLegend(0.05,0.05,0.35,0.35,"");
    }
    can->Update();
    char str[200];
    bool ext = false;
    std::unique_ptr<TTimer> timer (new TTimer("gSystem->ProcessEvents();", 100, kFALSE) );
        do {
          std::cout<<"Type 'n' for next event or 'q' to quit interactive mode "<<std::endl;
          timer->TurnOn();
          timer->Reset();
          scanf("%s",str);
          timer->TurnOff();
          if(strcmp("n",str) == 0)ext=true;
          if(strcmp("q",str) == 0){return;}
        } while (!ext);
    
    for(auto &h : histos)
      delete h;
    histos.clear();
    for(auto &h : hs)
      delete h;
    hs.clear();
  }

}


//#include "SignalEvent.h"

void DrawPulses(const std::string &fileName){

//SignalEvent sEvent;

auto model = ROOT::RNTupleModel::Create();
auto evID = model->MakeField<int>("eventID");
auto tS = model->MakeField<double>("timestamp");
auto sID = model->MakeField<std::vector<int>>("signalsID");
auto pls = model->MakeField< std::vector<std::vector<short>> >("pulses");
auto reader = ROOT::RNTupleReader::Open(std::move(model),"SignalEvents",fileName);

THStack *hs = nullptr;

TCanvas *can = new TCanvas("c","",800,600);
const size_t entries = reader->GetNEntries();
cout<<"Entries "<<entries<<endl;
  for(size_t entryID = 0; entryID < entries; entryID++){
    std::vector<TH1S *> histos;
    reader->LoadEntry(entryID);
    hs = new THStack("Pulses","");
    int histoID = 0;
    std::vector<std::vector<short>> pulses = *pls;
    std::vector<int> signalsID = *sID;
        for (size_t s = 0; s<signalsID.size();s++){
          const int signalID = signalsID[s];
          std::string hName = "Hist_"+std::to_string(signalID);
          auto pulse = pulses[s];
          auto histo = new TH1S (hName.c_str(),hName.c_str(),pulse.size(),0.,pulse.size());
            for(int p=0;p<pulse.size();p++){
              //cout<<p<<" "<<pulse[p]<<std::endl;
              histo->SetBinContent(p+1,pulse[p]);
            }
          short max = *std::max_element(pulse.begin(), pulse.end());
          if(max > 500)std::cout<<"Signal "<<signalID<<" max "<<max<<endl;
          histo->SetLineColor(s);
          histo->SetMarkerColor(s);
          histos.emplace_back(histo);
          histoID++;
          hs->Add(histos.back(),hName.c_str());
        }
    can->cd();
    hs->Draw("nostack,lp");
    gPad->BuildLegend(0.05,0.05,0.35,0.35,"");
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
    delete hs;
  }

}

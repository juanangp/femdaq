
//#include "SignalEvent.h"

#include <glob.h>

void GetParamsFromPulse(const std::vector<short> &pulse, double &amplitude, double &area, int &maxPos){

  area = 0;
  amplitude = 0;

  double baseLine = 0, baseLineSigma = 0;
    for (int p = 10; p < 110; p++) {
      int val = pulse[p];
      baseLine += val;
      baseLineSigma += val * val;
    }

  baseLine /= 100.;
  baseLineSigma = sqrt(baseLineSigma / 100. - baseLine * baseLine);

  auto maxIt = std::max_element(pulse.begin() + 100, pulse.begin() + 500);

  maxPos = std::distance(pulse.begin(), maxIt);

  short max = *maxIt;

  const double threshold = 7.*baseLineSigma;

  if(max < baseLine + threshold)return;

  //cout<<baseLine<<" "<<baseLineSigma<<" "<<max<<" "<< baseLine + threshold <<endl;

  amplitude = max;

  for(int p=0;p<500;p++){
      double val = pulse[p] - baseLine;
      if(val> threshold)area += val;
    }
}

std::vector<std::string> GetFilesFromPattern(const std::string &pattern){
    glob_t glob_result;
    std::vector<std::string> files;

    if (glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; ++i)
            files.emplace_back(glob_result.gl_pathv[i]);
    }

    globfree(&glob_result);
    return files;
}

std::map <int,int> readDecoding(const std::string &decodingFile){

  std::map<int,int> decodingMap;

  if(decodingFile.empty())return decodingMap;
  
  std::ifstream inputFile(decodingFile);
    
    // Check if file exists
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file." << std::endl;
        return decodingMap;
    }

    int sID, pos;
    // Read pairs and insert them into the map
    while (inputFile >> sID >> pos) {
        decodingMap[sID] = pos;
        //cout<<sID<<" "<<pos<<endl;
    }

    //inputFile.close();

return decodingMap;

}


void AnalyzePulses(const std::string &filePattern, const std::string &decodingFile=""){

//SignalEvent sEvent;

auto decodingMap = readDecoding(decodingFile);

//for(const auto &[sID,pCh] :decodingMap )cout<<sID<<" "<<pCh<<endl;

int nChannels = 1;

  if(!decodingMap.empty()){
    nChannels = decodingMap.size()/2;
    cout<<"nChannels "<<nChannels<<endl;
  }


std::vector<THStack *> hs;

TCanvas *can1 = new TCanvas("c1","",800,600);
can1->Divide(2,2);
TCanvas *can2 = new TCanvas("c2","",800,600);
auto spectra = new TH1F ("Spectra","Spectra",2048,0,1000000);
can2->cd();spectra->Draw();
TCanvas *can3 = new TCanvas("c3","",800,600);
auto hitmap = new TH2F("HitMap","HitMap", nChannels, 0, nChannels, nChannels, 0, nChannels);
can3->cd();hitmap->Draw();
TCanvas *can4 = new TCanvas("c4","",800,600);
can4->Divide(2,1);
auto hitX = new TH2F("HitX","HitX", nChannels, 0, nChannels, 511, 0, 511);
auto hitY = new TH2F("HitY","HitY", nChannels, 0, nChannels, 511, 0, 511);

bool interactive = true;

auto files = GetFilesFromPattern(filePattern);

auto chain = std::make_unique<TChain>("SignalEvent");

  for (const auto &fileName : files)
    if(fileName.find(".root") !=std::string::npos){
    cout<<"Opening file "<<fileName<<endl;
    chain->Add(fileName.c_str(), -1);
  }

const size_t entries = chain->GetEntries();
cout<<"Entries "<<entries<<endl;

int eventID;
double timestamp = 0;
std::vector<int>* signalsID = nullptr; 
std::vector<short>* pulses = nullptr;

chain->SetBranchAddress("eventID", &eventID);
chain->SetBranchAddress("timestamp", &timestamp);
chain->SetBranchAddress("signalsID", &signalsID);
chain->SetBranchAddress("pulses", &pulses);

  for(size_t entryID = 0; entryID < entries; entryID++){
    
    std::vector<TH1S *> histos;
    chain->GetEntry(entryID);
      
      if(interactive)
        for(int i=0;i<4;i++){
          auto h = new THStack( );
          hs.emplace_back(h);
        }

    double totArea=0;
    double areaX =0;
    double areaY=0;
    double posX = 0, posY=0;
        for (size_t s = 0; s<signalsID->size();s++){
          const int signalID = signalsID->at(s);
          const int card = signalID/72;
          const int channel = signalID%72;
          double amplitude=0, area=0;
          int maxPos=0;
          const size_t offset = s * 512;
          std::vector<short> pulse(pulses->begin() + offset, pulses->begin() + offset + 512);
          std::string hName = "Hist_"+std::to_string(signalID);
          GetParamsFromPulse(pulse, amplitude, area,maxPos);

          if(area==0 || amplitude == 0)continue;

          //std::cout<<"Card "<<card<<" channel "<<channel<<" max "<<amplitude<<endl;
          if(!decodingMap.empty()){
            auto it = decodingMap.find(signalID);
            if (it != decodingMap.end()){
              int pos = it->second;
              if(interactive)cout<<"SID "<< signalID<< " --> "<<pos <<" "<<area<<endl;
              int posChann = pos%nChannels;
              if(pos/nChannels ==0){
                posX += area*posChann;
                areaX += area;
              } else {
                posY += area*posChann;
                areaY += area;
              }
              if(interactive){
                if(pos/nChannels ==0)hitX->Fill(posChann,maxPos,area);
                else hitY->Fill(posChann,maxPos,area);
              }

            }
          }

          totArea += area;

          if(interactive){
            std::string hName = "Hist_"+std::to_string(signalID);
            auto histo = new TH1S (hName.c_str(),hName.c_str(),pulse.size(),0.,pulse.size());
              for(int p=0;p<pulse.size();p++){
                //cout<<p<<" "<<pulse[p]<<std::endl;
                histo->SetBinContent(p+1,pulse[p]);
              }
          
            histo->SetLineColor(channel);
            histo->SetMarkerColor(channel);
            histos.emplace_back(histo);
            hs[card]->Add(histos.back(),hName.c_str());
          }
        }
    spectra->Fill(totArea);
    if(areaX >0 && areaY >0){
      posX /= areaX;
      posY /= areaY;
      hitmap->Fill(posX,posY);
    }

    if(interactive || entryID%100==0){
      can2->cd();
      spectra->Draw();
      can2->Update();
      can3->cd();
      hitmap->Draw("COLZ");
      can3->Update();
    }

    if(interactive){
      can1->cd();
      for(int i=0;i<4;i++){
        can1->cd(i+1);
        hs[i]->Draw("nostack,lp");
        gPad->BuildLegend(0.05,0.05,0.35,0.35,"");
      }
      can1->Update();
      can4->cd(1);
      hitX->Draw("COLZ");
      can4->cd(2);
      hitY->Draw("COLZ");
      can4->Update();
      
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
            if(strcmp("q",str) == 0){interactive = false; ext=true;}
          } while (!ext);
    
      for(auto &h : histos)
        delete h;
      histos.clear();
      for(auto &h : hs)
        delete h;
      hs.clear();
      hitX->Reset();
      hitY->Reset();
    }
  }


}

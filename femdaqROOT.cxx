#include <TApplication.h>
#include <TROOT.h>
#include <TRint.h>
#include <TSystem.h>
#include <TStyle.h>


int main(int argc, char* argv[]) {

  printf("---------------------Wellcome to FEMDAQRoot--------------------\n");

  gSystem->Load("libfemdaq.so");
  gSystem->AddIncludePath(" -I$FEMDAQ_INCLUDE_PATH");
  gROOT->ProcessLine(".L  $FEMDAQ_PATH/macros/DrawPulses.C");
  gROOT->ProcessLine(".L  $FEMDAQ_PATH/macros/AnalyzePulses.C");

  gStyle->SetPalette(1);
  gStyle->SetTimeOffset(0);
  // display root's command line
  TRint theApp("App", &argc, argv);
  theApp.Run();

  return 0;
}

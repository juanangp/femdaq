#include <TApplication.h>
#include <TROOT.h>
#include <TRint.h>
#include <TStyle.h>
#include <TSystem.h>

int main(int argc, char *argv[]) {

  printf("---------------------Wellcome to FEMDAQRoot--------------------\n");

  TRint theApp("App", &argc, argv);
  gSystem->Load("libfemdaq.so");
  gSystem->AddIncludePath(" -I$FEMDAQ_INCLUDE_PATH");
  gROOT->ProcessLine(".L  $FEMDAQ_PATH/macros/DrawPulses.C+");
  gROOT->ProcessLine(".L  $FEMDAQ_PATH/macros/AnalyzePulses.C");
  gROOT->ProcessLine(".L  $FEMDAQ_PATH/macros/EventDisplay.C+");

  gStyle->SetPalette(1);
  gStyle->SetTimeOffset(0);
  // display root's command line

  theApp.Run();

  return 0;
}

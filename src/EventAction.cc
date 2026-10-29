#include "EventAction.hh"
#include "G4AnalysisManager.hh"
#include "G4Event.hh"
#include "G4SystemOfUnits.hh"

namespace B1 {

EventAction::EventAction(RunAction*) {}

void EventAction::BeginOfEventAction(const G4Event*) {
  fHitPixel.clear();
  fHitEdep.clear();
  fHitTime.clear();
  fHitPdg.clear();
  
  fNGenAlphas = 0;
  fNGenGammas = 0;
  fNGenElectrons = 0;
  fNGenHeavyIons = 0;
}

void EventAction::EndOfEventAction(const G4Event* event) {
  auto analysisManager = G4AnalysisManager::Instance();
  const G4int eventID = event->GetEventID();
  G4int nHitPixels = fHitPixel.size(); 
  G4double edepTotal = 0.;

  // ★ 非対称ジオメトリ対応の定数
  const G4int nx = 20, ny = 30, nz = 10;

  for (size_t i = 0; i < fHitPixel.size(); ++i) {
    edepTotal += fHitEdep[i];

    G4int copy = fHitPixel[i];
    G4int face = -1, ix = -1, iy = -1;
    
    // ★ 1600個のピクセルID(copyNo)を面ごとに正しく分割
    if (copy < nx*ny) { 
      face = 0; ix = copy / ny; iy = copy % ny; 
    } else if (copy < nx*ny + nz*ny) { 
      G4int c = copy - nx*ny; face = 1; ix = c / ny; iy = c % ny; 
    } else if (copy < nx*ny + 2*nz*ny) { 
      G4int c = copy - nx*ny - nz*ny; face = 2; ix = c / ny; iy = c % ny; 
    } else if (copy < nx*ny + 2*nz*ny + nx*nz) { 
      G4int c = copy - nx*ny - 2*nz*ny; face = 3; ix = c / nz; iy = c % nz; 
    } else { 
      G4int c = copy - nx*ny - 2*nz*ny - nx*nz; face = 4; ix = c / nz; iy = c % nz; 
    }

    analysisManager->FillNtupleIColumn(1, 0, eventID);
    analysisManager->FillNtupleIColumn(1, 1, copy);
    analysisManager->FillNtupleIColumn(1, 2, face);
    analysisManager->FillNtupleIColumn(1, 3, ix);
    analysisManager->FillNtupleIColumn(1, 4, iy);
    analysisManager->FillNtupleDColumn(1, 5, fHitEdep[i] / MeV);
    analysisManager->FillNtupleDColumn(1, 6, fHitTime[i] / microsecond);
    analysisManager->FillNtupleIColumn(1, 7, fHitPdg[i]);
    analysisManager->FillH1(1, fHitEdep[i]);
    analysisManager->AddNtupleRow(1);
  }

  analysisManager->FillNtupleIColumn(0, 0, eventID);
  analysisManager->FillNtupleDColumn(0, 1, edepTotal / MeV);
  analysisManager->FillNtupleIColumn(0, 2, nHitPixels);
  analysisManager->FillNtupleIColumn(0, 3, fNGenAlphas);
  analysisManager->FillNtupleIColumn(0, 4, fNGenGammas);
  analysisManager->FillNtupleIColumn(0, 5, fNGenElectrons);
  analysisManager->FillNtupleIColumn(0, 6, fNGenHeavyIons);
  analysisManager->AddNtupleRow(0);
}

}  // namespace B1
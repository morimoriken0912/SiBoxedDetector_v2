/// \file RunAction.cc
/// \brief Implementation of the B1::RunAction class
///
/// 出力ファイル: SiDetector.root
///   ntuple "events" : 全イベントについて1行
///       eventID, edepTotal [MeV], nHitPixels
///   ntuple "hits"   : エネルギーを落とした素子ごとに1行
///       eventID, pixelID (0-319), face (0-4), ix (0-7), iy (0-7), edep [MeV]
///       face: 0=+z(奥), 1=+x, 2=-x, 3=+y, 4=-y
///   histogram "hEdepTotal" : 1イベントの合計エネルギー損失 (bin幅 0.25 MeV, 0-700 MeV)
///   histogram "hEdepPixel" : 素子ごとのエネルギー損失      (bin幅 0.25 MeV, 0-700 MeV)

#include "RunAction.hh"

#include "G4AnalysisManager.hh"
#include "G4Run.hh"
#include "G4RunManager.hh"
#include "G4SystemOfUnits.hh"

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

RunAction::RunAction()
{
  auto analysisManager = G4AnalysisManager::Instance();
  analysisManager->SetDefaultFileType("root");
  analysisManager->SetVerboseLevel(1);

  // マルチスレッドでも1つのROOTファイルにまとめる
  analysisManager->SetNtupleMerging(true);
// ntuple 0: events
  analysisManager->CreateNtuple("events", "Event summary");
  analysisManager->CreateNtupleIColumn("eventID");        // column 0
  analysisManager->CreateNtupleDColumn("edepTotal");      // column 1 [MeV]
  analysisManager->CreateNtupleIColumn("nHitPixels");     // column 2
  analysisManager->CreateNtupleIColumn("nGenAlphas");     // column 3
  analysisManager->CreateNtupleIColumn("nGenGammas");     // column 4
  analysisManager->CreateNtupleIColumn("nGenElectrons");  // column 5
  // ↓↓↓ ここを追加 ↓↓↓
  analysisManager->CreateNtupleIColumn("nGenHeavyIons");  // column 6 (重イオン/原子核の発生数)
  // ↑↑↑ ここまで ↑↑↑
  analysisManager->FinishNtuple();

  // ntuple 1: hits
  analysisManager->CreateNtuple("hits", "Si pixel hits");
  analysisManager->CreateNtupleIColumn("eventID");  // column 0
  analysisManager->CreateNtupleIColumn("pixelID");  // column 1
  analysisManager->CreateNtupleIColumn("face");     // column 2
  analysisManager->CreateNtupleIColumn("ix");       // column 3
  analysisManager->CreateNtupleIColumn("iy");       // column 4
  analysisManager->CreateNtupleDColumn("edep");     // column 5 [MeV]
  analysisManager->CreateNtupleDColumn("time");     // column 6 [us]
  // ↓↓↓ ここを追加 ↓↓↓
  analysisManager->CreateNtupleIColumn("pdg");      // column 7 (粒子のPDGコード)
  // ↑↑↑ ここまで ↑↑↑
  analysisManager->FinishNtuple();

  // エネルギーヒストグラム (bin幅 0.25 MeV)
  // 範囲の上限はビームエネルギー(600 MeV)より少し大きい 700 MeV
  const G4double binWidth = 0.25 * MeV;
  const G4double eMax = 700. * MeV;
  const G4int nBins = static_cast<G4int>(eMax / binWidth);  // 2800 bins

  // H1 id 0: 1イベントの合計エネルギー損失 (ヒットのあったイベントのみ)
  analysisManager->CreateH1("hEdepTotal", "Total energy deposit per event", nBins, 0., eMax);
  // H1 id 1: 素子ごとのエネルギー損失
  analysisManager->CreateH1("hEdepPixel", "Energy deposit per pixel", nBins, 0., eMax);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::BeginOfRunAction(const G4Run*)
{
  auto analysisManager = G4AnalysisManager::Instance();
  analysisManager->OpenFile("SiDetector.root");
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void RunAction::EndOfRunAction(const G4Run* run)
{
  auto analysisManager = G4AnalysisManager::Instance();
  analysisManager->Write();
  analysisManager->CloseFile();

  G4int nofEvents = run->GetNumberOfEvent();
  if (nofEvents == 0) return;

  if (IsMaster()) {
    G4cout << G4endl << "--------------------End of Global Run-----------------------" << G4endl
           << " The run consisted of " << nofEvents << " events" << G4endl
           << " Results were written to SiDetector.root" << G4endl
           << "------------------------------------------------------------" << G4endl;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1

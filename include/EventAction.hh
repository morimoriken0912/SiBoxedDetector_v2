/// \file EventAction.hh
/// \brief Definition of the B1::EventAction class
///        1イベント中の素子ごとのエネルギー損失を集計し、イベント終了時にROOTへ書き出す

#ifndef B1EventAction_h
#define B1EventAction_h 1

#include "G4UserEventAction.hh"
#include "globals.hh"
#include <vector> // ファイル上部に追加
#include <array>

namespace B1
{

class RunAction;

class EventAction : public G4UserEventAction
{
  public:
    EventAction(RunAction* runAction);
    ~EventAction() override = default;

    void BeginOfEventAction(const G4Event* event) override;
    void EndOfEventAction(const G4Event* event) override;

    // 素子IDは 0 〜 1599。DetectorConstruction のコピー番号と同じ
    static constexpr G4int kNPixelTotal = 1600;

    void AddPixelEdep(G4int id, G4double edep, G4double time, G4int pdg)
    {
      if (id < 0 || id >= kNPixelTotal) return;

      // 既に「同じピクセル」で「同じ粒子(PDG)」が「ほぼ同時刻(例:1ns以内)」に
      // エネルギーを落としていれば、同じヒットとみなしてエネルギーを足す
      for (size_t i = 0; i < fHitPixel.size(); ++i) {
        if (fHitPixel[i] == id && fHitPdg[i] == pdg && std::abs(fHitTime[i] - time) < 1.0) {
          fHitEdep[i] += edep;
          return;
        }
      }
      // 見つからなければ、新しい独立したヒットとして追加（これで重イオンとα線が分離される！）
      fHitPixel.push_back(id);
      fHitEdep.push_back(edep);
      fHitTime.push_back(time);
      fHitPdg.push_back(pdg);
    }

    void AddGenAlpha() { fNGenAlphas++; }
    void AddGenGamma() { fNGenGammas++; }
    void AddGenElectron() { fNGenElectrons++; }
    void AddGenHeavyIon() { fNGenHeavyIons++; }

  private:
    std::vector<G4int> fHitPixel;
    std::vector<G4double> fHitEdep;
    std::vector<G4double> fHitTime;
    std::vector<G4int> fHitPdg;
    
    G4int fNGenAlphas = 0;
    G4int fNGenGammas = 0;
    G4int fNGenElectrons = 0;
    G4int fNGenHeavyIons = 0;
};

}  // namespace B1

#endif

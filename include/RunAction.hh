/// \file RunAction.hh
/// \brief Definition of the B1::RunAction class
///        ROOTファイルの作成・ntupleの定義・書き出しを担当する

#ifndef B1RunAction_h
#define B1RunAction_h 1

#include "G4UserRunAction.hh"

class G4Run;

namespace B1
{

class RunAction : public G4UserRunAction
{
  public:
    RunAction();
    ~RunAction() override = default;

    void BeginOfRunAction(const G4Run*) override;
    void EndOfRunAction(const G4Run*) override;
};

}  // namespace B1

#endif

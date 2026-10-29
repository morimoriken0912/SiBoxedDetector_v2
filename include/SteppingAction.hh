/// \file SteppingAction.hh
/// \brief Definition of the B1::SteppingAction class
///        有感部(SiSensitive)でのエネルギー損失を、素子ごとに EventAction へ渡す

#ifndef B1SteppingAction_h
#define B1SteppingAction_h 1

#include "G4UserSteppingAction.hh"

class G4LogicalVolume;

namespace B1
{

class EventAction;

class SteppingAction : public G4UserSteppingAction
{
  public:
    SteppingAction(EventAction* eventAction);
    ~SteppingAction() override = default;

    // method from the base class
    void UserSteppingAction(const G4Step*) override;

  private:
    EventAction* fEventAction = nullptr;
    G4LogicalVolume* fScoringVolume = nullptr;
};

}  // namespace B1

#endif

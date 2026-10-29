//
// ********************************************************************
// * License and Disclaimer                                           *
// *                                                                  *
// * The  Geant4 software  is  copyright of the Copyright Holders  of *
// * the Geant4 Collaboration.  It is provided  under  the terms  and *
// * conditions of the Geant4 Software License,  included in the file *
// * LICENSE and available at  http://cern.ch/geant4/license .  These *
// * include a list of copyright holders.                             *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.  Please see the license in the file  LICENSE  and URL above *
// * for the full disclaimer and the limitation of liability.         *
// *                                                                  *
// * This  code  implementation is the result of  the  scientific and *
// * technical work of the GEANT4 collaboration.                      *
// * By using,  copying,  modifying or  distributing the software (or *
// * any work based  on the software)  you  agree  to acknowledge its *
// * use  in  resulting  scientific  publications,  and indicate your *
// * acceptance of all terms of the Geant4 Software license.          *
// ********************************************************************
//
/// \file PrimaryGeneratorAction.cc
/// \brief Implementation of the B1::PrimaryGeneratorAction class
///        225Np ion beam (600 MeV), 5 cm diameter uniform circular spot, along +z

#include "PrimaryGeneratorAction.hh"

#include "G4Box.hh"
#include "G4IonTable.hh"
#include "G4LogicalVolume.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4ParticleGun.hh"
#include "G4ParticleTable.hh"
#include "G4PhysicalConstants.hh"
#include "G4SystemOfUnits.hh"
#include "Randomize.hh"

#include <cmath>

namespace B1
{

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::PrimaryGeneratorAction()
{
  G4int n_particle = 1;
  fParticleGun = new G4ParticleGun(n_particle);

  // コンストラクタの時点ではイオンを作れない(ワーカースレッドで GenericIon が未準備)。
  // そのためここでは仮の粒子(gamma)を設定しておき、
  // 225Np イオンは GeneratePrimaries() の最初の呼び出しで設定する。
  G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
  G4ParticleDefinition* particle = particleTable->FindParticle("gamma");
  fParticleGun->SetParticleDefinition(particle);
  fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
  fParticleGun->SetParticleEnergy(600. * MeV);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

PrimaryGeneratorAction::~PrimaryGeneratorAction()
{
  delete fParticleGun;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

void PrimaryGeneratorAction::GeneratePrimaries(G4Event* event)
{
  // this function is called at the begining of ecah event
  //

  // In order to avoid dependence of PrimaryGeneratorAction
  // on DetectorConstruction class we get Envelope volume
  // from G4LogicalVolumeStore.

  G4double envSizeZ = 0;

  if (!fEnvelopeBox) {
    G4LogicalVolume* envLV = G4LogicalVolumeStore::GetInstance()->GetVolume("Envelope");
    if (envLV) fEnvelopeBox = dynamic_cast<G4Box*>(envLV->GetSolid());
  }

  if (fEnvelopeBox) {
    envSizeZ = fEnvelopeBox->GetZHalfLength() * 2.;
  }
  else {
    G4ExceptionDescription msg;
    msg << "Envelope volume of box shape not found.\n";
    msg << "Perhaps you have changed geometry.\n";
    msg << "The gun will be place at the center.";
    G4Exception("PrimaryGeneratorAction::GeneratePrimaries()", "MyCode0002", JustWarning, msg);
  }

  // 225Np イオン (Z = 93, A = 225) の設定。
  // 粒子の種類が "nucleus" になっていなければ(= 初回のイベント)ここで設定する。
  //if (fParticleGun->GetParticleDefinition()->GetParticleType() != "nucleus") {
  //  const G4int ionZ = 93;
  //  const G4int ionA = 225;
  //  G4ParticleDefinition* ion = G4IonTable::GetIonTable()->GetIon(ionZ, ionA, 0.);
  //  if (ion) {
  //    fParticleGun->SetParticleDefinition(ion);
  //    fParticleGun->SetParticleMomentumDirection(G4ThreeVector(0., 0., 1.));
      // 運動エネルギー(イオン全体の合計)。600 MeV/u の場合は 600. * ionA * MeV
  //    fParticleGun->SetParticleEnergy(600. * MeV);
  //  }
//}

  // ビーム: 直径 5 cm の円内に一様分布
 // const G4double beamRadius = 0.5 * 5. * cm;
 // G4double r = beamRadius * std::sqrt(G4UniformRand());  // 面積あたり一様にするため sqrt
 // G4double phi = twopi * G4UniformRand();
 // G4double x0 = r * std::cos(phi);
 // G4double y0 = r * std::sin(phi);
 // G4double z0 = -0.5 * envSizeZ;

// ビーム: 楕円形ガウシアン分布
  // 直径を「半値全幅(FWHM)」と解釈して標準偏差(sigma)を計算する場合
  G4double fwhmX = 15.0 * cm;
  G4double fwhmY = 20.0 * cm;
  G4double sigmaX = fwhmX / 3.92;
  G4double sigmaY = fwhmY / 3.92;//95%区間

  G4double x0 = G4RandGauss::shoot(0., sigmaX);
  G4double y0 = G4RandGauss::shoot(0., sigmaY);
  G4double z0 = -0.5 * envSizeZ;

////////////////////////////////////////////////////

  fParticleGun->SetParticlePosition(G4ThreeVector(x0, y0, z0));
  fParticleGun->GeneratePrimaryVertex(event);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo......

}  // namespace B1

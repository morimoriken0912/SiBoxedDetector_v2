#include "DetectorConstruction.hh"
#include "G4Box.hh"
#include "G4Colour.hh"
#include "G4LogicalVolume.hh"
#include "G4NistManager.hh"
#include "G4PVPlacement.hh"
#include "G4RotationMatrix.hh"
#include "G4SystemOfUnits.hh"
#include "G4ThreeVector.hh"
#include "G4Transform3D.hh"
#include "G4VisAttributes.hh"

namespace B1 {

G4VPhysicalVolume* DetectorConstruction::Construct() {
  G4NistManager* nist = G4NistManager::Instance();

  // ★修正点1: 検出器の大型化に合わせ、シミュレーション空間を十分に拡張 (20cm -> 100cm)
  G4double env_sizeXY = 50 * cm, env_sizeZ = 50 * cm;
  G4Material* env_mat = nist->FindOrBuildMaterial("G4_Galactic");
  G4bool checkOverlaps = true;

  G4double world_sizeXY = 1.2 * env_sizeXY;
  G4double world_sizeZ = 1.2 * env_sizeZ;
  G4Material* world_mat = nist->FindOrBuildMaterial("G4_Galactic");

  auto solidWorld = new G4Box("World", 0.5 * world_sizeXY, 0.5 * world_sizeXY, 0.5 * world_sizeZ);
  auto logicWorld = new G4LogicalVolume(solidWorld, world_mat, "World");
  auto physWorld = new G4PVPlacement(nullptr, G4ThreeVector(), logicWorld, "World", nullptr, false, 0, checkOverlaps);

  auto solidEnv = new G4Box("Envelope", 0.5 * env_sizeXY, 0.5 * env_sizeXY, 0.5 * env_sizeZ);
  auto logicEnv = new G4LogicalVolume(solidEnv, env_mat, "Envelope");
  new G4PVPlacement(nullptr, G4ThreeVector(), logicEnv, "Envelope", logicWorld, false, 0, checkOverlaps);

  // --- 検出器のパラメータ ---
  G4Material* si_mat = nist->FindOrBuildMaterial("G4_Si");

  const G4double pixSize = 7.5 * mm;  
  const G4double sensSize = 7.05 * mm;  
  const G4double pixGap = 0.1 * mm;  // Gapは0.1mm固定
  
  const G4int nPixX = 20;
  const G4int nPixY = 30;
  const G4int nPixZ = 10;

  const G4double pixThick = 150 * um;            
  const G4double deadThick = 0.177 * um;         
  const G4double sensThick = pixThick - deadThick; 
  const G4double pixPitch = pixSize + pixGap;  
  
  const G4double halfX = 0.5 * nPixX * pixPitch;
  const G4double halfY = 0.5 * nPixY * pixPitch;
  const G4double halfZ = 0.5 * nPixZ * pixPitch;

  const G4double offsetX = halfX + 0.5 * pixThick;
  const G4double offsetY = halfY + 0.5 * pixThick;
  const G4double offsetZ = halfZ + 0.5 * pixThick;

  // Box定義
  auto solidPixel = new G4Box("SiElement", 0.5 * pixSize, 0.5 * pixSize, 0.5 * pixThick);
  auto logicPixel = new G4LogicalVolume(solidPixel, si_mat, "SiElement");
  logicPixel->SetVisAttributes(new G4VisAttributes(G4Colour(0.4, 0.4, 0.8)));

  auto solidSens = new G4Box("SiSensitive", 0.5 * sensSize, 0.5 * sensSize, 0.5 * sensThick);
  auto logicSens = new G4LogicalVolume(solidSens, si_mat, "SiSensitive");
  logicSens->SetVisAttributes(new G4VisAttributes(G4Colour(0.0, 0.8, 1.0)));

  G4double zOffset = 0.5 * deadThick; 
  new G4PVPlacement(nullptr, G4ThreeVector(0, 0, zOffset), logicSens, "SiSensitive", logicPixel, false, 0, checkOverlaps);

  G4RotationMatrix rotFace0; 
  G4RotationMatrix rotFace1; rotFace1.rotateY(90. * deg);  
  G4RotationMatrix rotFace2; rotFace2.rotateY(-90. * deg); 
  G4RotationMatrix rotFace3; rotFace3.rotateX(-90. * deg); 
  G4RotationMatrix rotFace4; rotFace4.rotateX(90. * deg);  

  G4int copyNo = 0; 

  // Face 0: +z 面
  for (G4int i = 0; i < nPixX; ++i) {
    for (G4int j = 0; j < nPixY; ++j) {
      G4double x = (i - 0.5 * (nPixX - 1)) * pixPitch;
      G4double y = (j - 0.5 * (nPixY - 1)) * pixPitch;
      new G4PVPlacement(G4Transform3D(rotFace0, G4ThreeVector(x, y, +offsetZ)), logicPixel, "SiElement", logicEnv, false, copyNo++, checkOverlaps);
    }
  }
  // Face 1: +x 面
  for (G4int i = 0; i < nPixZ; ++i) {
    for (G4int j = 0; j < nPixY; ++j) {
      G4double z = (i - 0.5 * (nPixZ - 1)) * pixPitch;
      G4double y = (j - 0.5 * (nPixY - 1)) * pixPitch;
      new G4PVPlacement(G4Transform3D(rotFace1, G4ThreeVector(+offsetX, y, z)), logicPixel, "SiElement", logicEnv, false, copyNo++, checkOverlaps);
    }
  }
  // Face 2: -x 面
  for (G4int i = 0; i < nPixZ; ++i) {
    for (G4int j = 0; j < nPixY; ++j) {
      G4double z = (i - 0.5 * (nPixZ - 1)) * pixPitch;
      G4double y = (j - 0.5 * (nPixY - 1)) * pixPitch;
      new G4PVPlacement(G4Transform3D(rotFace2, G4ThreeVector(-offsetX, y, z)), logicPixel, "SiElement", logicEnv, false, copyNo++, checkOverlaps);
    }
  }
  // Face 3: +y 面
  for (G4int i = 0; i < nPixX; ++i) {
    for (G4int j = 0; j < nPixZ; ++j) {
      G4double x = (i - 0.5 * (nPixX - 1)) * pixPitch;
      G4double z = (j - 0.5 * (nPixZ - 1)) * pixPitch;
      new G4PVPlacement(G4Transform3D(rotFace3, G4ThreeVector(x, +offsetY, z)), logicPixel, "SiElement", logicEnv, false, copyNo++, checkOverlaps);
    }
  }
  // Face 4: -y 面
  for (G4int i = 0; i < nPixX; ++i) {
    for (G4int j = 0; j < nPixZ; ++j) {
      G4double x = (i - 0.5 * (nPixX - 1)) * pixPitch;
      G4double z = (j - 0.5 * (nPixZ - 1)) * pixPitch;
      new G4PVPlacement(G4Transform3D(rotFace4, G4ThreeVector(x, -offsetY, z)), logicPixel, "SiElement", logicEnv, false, copyNo++, checkOverlaps);
    }
  }

  // --- PCBボード ---
  G4Material* pcb_mat = nist->FindOrBuildMaterial("G4_POLYETHYLENE"); 
  const G4double pcbThick = 3. * mm;
  
  const G4double pcbOffsetZ = halfZ + pixThick + 0.5 * pcbThick;
  const G4double pcbOffsetX = halfX + pixThick + 0.5 * pcbThick;
  const G4double pcbOffsetY = halfY + pixThick + 0.5 * pcbThick;

  auto colorPcb = new G4VisAttributes(G4Colour(0.0, 0.6, 0.2));

  auto logicPcbZ = new G4LogicalVolume(new G4Box("PCB_Z", halfX, halfY, 0.5 * pcbThick), pcb_mat, "PCB_Z");
  logicPcbZ->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(0, 0, +pcbOffsetZ), logicPcbZ, "PCB_Z", logicEnv, false, 0, checkOverlaps);

  auto logicPcbX = new G4LogicalVolume(new G4Box("PCB_X", 0.5 * pcbThick, halfY, halfZ), pcb_mat, "PCB_X");
  logicPcbX->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(+pcbOffsetX, 0, 0), logicPcbX, "PCB_X", logicEnv, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(-pcbOffsetX, 0, 0), logicPcbX, "PCB_X", logicEnv, false, 1, checkOverlaps);

  auto logicPcbY = new G4LogicalVolume(new G4Box("PCB_Y", halfX, 0.5 * pcbThick, halfZ), pcb_mat, "PCB_Y");
  logicPcbY->SetVisAttributes(colorPcb);
  new G4PVPlacement(nullptr, G4ThreeVector(0, +pcbOffsetY, 0), logicPcbY, "PCB_Y", logicEnv, false, 0, checkOverlaps);
  new G4PVPlacement(nullptr, G4ThreeVector(0, -pcbOffsetY, 0), logicPcbY, "PCB_Y", logicEnv, false, 1, checkOverlaps);

  fScoringVolume = logicSens;
  return physWorld;
}

}  // namespace B1
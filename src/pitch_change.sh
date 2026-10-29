#!/bin/bash

# 格納用ディレクトリの作成
mkdir -p ../build/pitch_changed
mkdir -p pitch_changed_html

# 0.0mm から 1.0mm まで 0.1mm 刻みでループ (0〜10)
for i in $(seq 0 10); do
  gap_str=$(printf "%02d" $i)
  gap_float=$(awk "BEGIN {print $i / 10.0}")
  pitch_float=$(awk "BEGIN {print 7.5 + $gap_float}")

  # 進行状況だけをシンプルに表示
  echo "Processing: Gap = ${gap_float} mm (Pitch = ${pitch_float} mm) ..."

  # 1. DetectorConstruction.cc の pixGap を書き換え
  sed -i -E 's/const G4double pixGap = [0-9.]+ \* mm;/const G4double pixGap = '"$gap_float"' * mm;/' DetectorConstruction.cc

  # 2. Geant4のビルドとシミュレーション実行 (出力を非表示)
  cd ../build/
  make -j$(nproc) 
  ./exampleB1 run.mac 10000 > /dev/null 2>&1

  # 3. ROOTファイルの移動・リネーム
  mv SiDetector.root pitch_changed/SiDetector_${gap_str}mm.root
  cd ../src/

  # 4. makeSiViewer.cc の kPitch を書き換え
  sed -i -E 's/static const double kPitch = [0-9.]+;/static const double kPitch = '"$pitch_float"';/' makeSiViewer.cc

  # 5. ViewerのコンパイルとHTML生成 (出力を非表示)
  g++ -O2 makeSiViewer.cc -o makeSiViewer $(root-config --cflags --libs) > /dev/null 2>&1
  ./makeSiViewer ../build/pitch_changed/SiDetector_${gap_str}mm.root pitch_changed_html/SiViewer_${gap_str}.html > /dev/null 2>&1

done

# 6. ccファイルを初期化
echo "Resetting variables to default (Gap = 0.5 mm, Pitch = 8.0 mm)..."
sed -i -E 's/const G4double pixGap = [0-9.]+ \* mm;/const G4double pixGap = 0.5 * mm;/' DetectorConstruction.cc
sed -i -E 's/static const double kPitch = [0-9.]+;/static const double kPitch = 8.0;/' makeSiViewer.cc

echo "All tasks completed successfully!"
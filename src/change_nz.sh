#!/bin/bash

=============================================================================

change_nz.sh : 検出器のZ方向のピクセル数 (nPixZ) を一時的に変更して実行する

実行場所 : src/ ディレクトリ

使い方   : ./change_nz.sh <変更したいZ方向のピクセル数>

例       : ./change_nz.sh 15

=============================================================================

set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

引数チェック

NZ=${1:-}
if [ -z "$NZ" ] || ! [[ "$NZ" =~ ^[0-9]+$ ]]; then
echo "エラー: Z方向のピクセル数を整数で指定してください。"
echo "Usage: ./change_nz.sh "
echo "Example: ./change_nz.sh 15"
exit 1
fi

対象ファイル群 (環境に合わせて必要ならパスを調整してください)

DET_FILE="DetectorConstruction.cc"
EVT_CC_FILE="EventAction.cc"
EVT_HH_FILE="../include/EventAction.hh"
VIEW_FILE="makeSiViewer_all.cc"
EXEC_SH="./execute_makeSiViewer.sh"

規定値

DEFAULT_NZ=10

ピクセル総数の計算 (nx=20, ny=30)

(nx * ny) + (nz * ny * 2) + (nx * nz * 2) = 600 + nz60 + nz40 = 600 + nz*100

DEFAULT_NTOT=$(( 600 + 100 * DEFAULT_NZ ))
NEW_NTOT=$(( 600 + 100 * NZ ))

=============================================================================

元に戻すための関数 (スクリプト終了時に必ず呼ばれる)

=============================================================================

restore_defaults() {
echo -e "\n=== 終了処理: Z方向のピクセル数を規定値 ($DEFAULT_NZ) に戻しています ==="

if [ -f "$DET_FILE" ]; then
sed -i -e "s/const G4int nPixZ = [0-9]+;/const G4int nPixZ = $DEFAULT_NZ;/" "$DET_FILE"
fi

if [ -f "$EVT_CC_FILE" ]; then
sed -i -e "s/const G4int nx = 20, ny = 30, nz = [0-9]+;/const G4int nx = 20, ny = 30, nz = $DEFAULT_NZ;/" "$EVT_CC_FILE"
fi

if [ -f "$EVT_HH_FILE" ]; then
sed -i -e "s/static constexpr G4int kNPixelTotal = [0-9]+;/static constexpr G4int kNPixelTotal = $DEFAULT_NTOT;/" "$EVT_HH_FILE"
fi

if [ -f "$VIEW_FILE" ]; then
sed -i -e "s/static const int kNPixZ = [0-9]+;/static const int kNPixZ = $DEFAULT_NZ;/" "$VIEW_FILE"
fi

echo "規定値へのリセットが完了しました。"
}

どんな形(正常終了、エラー、Ctrl+C等)でスクリプトが終わっても必ず復元関数を呼ぶ

trap restore_defaults EXIT

=============================================================================

ファイルの書き換え

=============================================================================

echo "=== Z方向のピクセル数を $NZ に変更中... ==="

1. DetectorConstruction.cc

if [ -f "$DET_FILE" ]; then
sed -i -e "s/const G4int nPixZ = [0-9]+;/const G4int nPixZ = $NZ;/" "$DET_FILE"
echo "  Updated: $DET_FILE"
else
echo "  Warning: $DET_FILE が見つかりません。"
fi

2. EventAction.cc

if [ -f "$EVT_CC_FILE" ]; then
sed -i -e "s/const G4int nx = 20, ny = 30, nz = [0-9]+;/const G4int nx = 20, ny = 30, nz = $NZ;/" "$EVT_CC_FILE"
echo "  Updated: $EVT_CC_FILE"
else
echo "  Warning: $EVT_CC_FILE が見つかりません。"
fi

3. EventAction.hh (総ピクセル数の更新)

if [ -f "$EVT_HH_FILE" ]; then
sed -i -e "s/static constexpr G4int kNPixelTotal = [0-9]+;/static constexpr G4int kNPixelTotal = $NEW_NTOT;/" "$EVT_HH_FILE"
echo "  Updated: $EVT_HH_FILE (kNPixelTotal =$NEW_NTOT)"
else
echo "  Warning: $EVT_HH_FILE が見つかりません。"
fi

4. makeSiViewer_all.cc

if [ -f "$VIEW_FILE" ]; then
sed -i -e "s/static const int kNPixZ = [0-9]+;/static const int kNPixZ = $NZ;/" "$VIEW_FILE"
echo "  Updated: $VIEW_FILE"
else
echo "  Warning: $VIEW_FILE が見つかりません。"
fi

=============================================================================

シミュレーションとHTML生成の実行

=============================================================================

echo "=== シミュレーションを実行します ==="
if [ -f "$EXEC_SH" ]; then
bash "$EXEC_SH"
else
echo "エラー: $EXEC_SH が見つかりません。手動でコンパイルと実行を行ってください。"
fi
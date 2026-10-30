#!/bin/bash
# =============================================================================
# change_beam.sh : ビーム粒子(イオン)の種類・エネルギーを一括で書き換える
#
# 実行場所 : src/ ディレクトリ  (./change_beam.sh)
# 書き換え : ../build/run.mac            ( /gun/ion, /gun/energy, nucleusLimits, コメント )
#            SteppingAction.cc           ( ビーム粒子判定の PDG コード(数値部分のみ) )
#            makeSiViewer_all*.cc        ( 'heavy' 判定の PDG コード )
#            execute_makeSiViewer.sh     ( 出力HTMLファイル名 )
#
# 使い方 :
#   1) 下の「ユーザー入力」を編集して ./change_beam.sh
#   2) もしくは引数で指定        ./change_beam.sh <Z> <A> <Q> <E_MeV> [--run]
#        例) ./change_beam.sh 86 204 43 146      # 204Rn 43+, 146 MeV
#   --run を付けると、書き換え後に execute_makeSiViewer.sh まで実行する
#   --full-chain を付けると、崩壊を制限せず崩壊系列をすべて計算する
#     (既定は、ビーム核種だけを崩壊させる。娘核以降は崩壊させない。
#      run.mac に  /process/had/rdm/nucleusLimits A A Z Z  を書き込む)
# =============================================================================
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

# ------------------------------ ユーザー入力 ---------------------------------
Z=86          # 陽子数
A=204         # 質量数 (陽子数 + 中性子数)
Q=43          # 電荷数 (イオンの価数)
E=146         # エネルギー [MeV]  (Geant4 の /gun/energy = 運動エネルギー合計)

# ビーム核種(上の Z, A)だけを崩壊させる設定。娘核以降の崩壊は計算しない。
#   1 : run.mac に  /process/had/rdm/nucleusLimits A A Z Z  を書く
#   0 : 制限なし (崩壊系列をすべて計算)。 --full-chain を付けても同じ
DECAY_ONLY_BEAM=1

# 書き換え対象ファイル (src/ からの相対パス)
MAC="../build/run.mac"
STEPPING="SteppingAction.cc"
EXEC_SH="execute_makeSiViewer.sh"
VIEWERS=("makeSiViewer_all.cc")     # 複数ある場合は ("a.cc" "b.cc") のように追記
# -----------------------------------------------------------------------------

# 引数による上書き
RUN_AFTER=0
POS=()
for a in "$@"; do
  if [ "$a" = "--run" ]; then RUN_AFTER=1
  elif [ "$a" = "--full-chain" ]; then DECAY_ONLY_BEAM=0
  else POS+=("$a"); fi
done
if [ "${#POS[@]}" -ge 4 ]; then Z=${POS[0]}; A=${POS[1]}; Q=${POS[2]}; E=${POS[3]}; fi

# ------------------------------ 入力チェック ---------------------------------
is_int()  { [[ "$1" =~ ^[0-9]+$ ]]; }
is_num()  { [[ "$1" =~ ^[0-9]+(\.[0-9]+)?$ ]]; }
is_int "$Z" && is_int "$A" && is_int "$Q" || { echo "ERROR: Z, A, Q は 0 以上の整数で指定してください" >&2; exit 1; }
is_num "$E" || { echo "ERROR: E は正の数値 [MeV] で指定してください" >&2; exit 1; }
[ "$Z" -ge 1 ] && [ "$Z" -le 118 ] || { echo "ERROR: Z は 1〜118" >&2; exit 1; }
[ "$A" -ge "$Z" ] || { echo "ERROR: 質量数 A は陽子数 Z 以上にしてください (A=$A, Z=$Z)" >&2; exit 1; }
[ "$Q" -le "$Z" ] || { echo "ERROR: 電荷数 Q は陽子数 Z 以下にしてください (Q=$Q, Z=$Z)" >&2; exit 1; }
awk "BEGIN{exit !($E > 0)}" || { echo "ERROR: E は 0 より大きい値" >&2; exit 1; }
[ "$DECAY_ONLY_BEAM" = "0" ] || [ "$DECAY_ONLY_BEAM" = "1" ] || { echo "ERROR: DECAY_ONLY_BEAM は 0 か 1" >&2; exit 1; }

N=$((A - Z))   # 中性子数 (表示用)

# 元素記号 (Z=1..118)
SYMBOLS=(X H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr \
  Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu \
  Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr Rf Db Sg \
  Bh Hs Mt Ds Rg Cn Nh Fl Mc Lv Ts Og)
SYM=${SYMBOLS[$Z]}

# PDG コード: イオンは 10LZZZAAAI (L=0, I=0:基底状態) = 1000000000 + Z*10000 + A*10
# ただし陽子(Z=1,A=1)は Geant4 では通常の陽子 (PDG=2212) になる
if [ "$Z" -eq 1 ] && [ "$A" -eq 1 ]; then
  PDG=2212
else
  PDG=$((1000000000 + Z * 10000 + A * 10))
fi
LABEL="${A}${SYM}"

echo "=== ビーム粒子の設定 ==="
echo "  粒子     : ${LABEL}  (Z=${Z}, N=${N}, A=${A}, Q=${Q}+)"
echo "  エネルギー: ${E} MeV"
echo "  PDG code : ${PDG}"
if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then
  echo "  崩壊     : ${LABEL} のみ (nucleusLimits ${A} ${A} ${Z} ${Z})。娘核以降は崩壊しない"
else
  echo "  崩壊     : 制限なし (崩壊系列をすべて計算)"
fi
echo

# ------------------------------ 補助関数 -------------------------------------
backup_once() { [ -e "$1.orig" ] || cp -p "$1" "$1.orig"; }

# sed を実行し、変更後に検証パターンが存在することを確認する
#   apply_sed <file> <sed-script> <verify-fixed-string>
apply_sed() {
  local f="$1" script="$2" verify="$3"
  [ -f "$f" ] || { echo "ERROR: ファイルが見つかりません: $f" >&2; exit 1; }
  backup_once "$f"
  sed -i -e "$script" "$f"
  grep -qF -- "$verify" "$f" || { echo "ERROR: $f の書き換えに失敗しました (該当行が見つかりません)" >&2; exit 1; }
  echo "  updated: $f"
}

# ------------------------------ 1) run.mac -----------------------------------
echo "[1] $MAC"
apply_sed "$MAC" \
  "s|^/gun/ion .*|/gun/ion ${Z} ${A} ${Q}|;
   s|^/gun/energy .*|/gun/energy ${E} MeV|;
   s|^# Z=.*|# Z=${Z}, A=${A}, 電荷Q=${Q} (${LABEL} ${Q}+)|" \
  "/gun/ion ${Z} ${A} ${Q}"
grep -qF "/gun/energy ${E} MeV" "$MAC" || { echo "ERROR: /gun/energy の書き換えに失敗" >&2; exit 1; }

# --- 崩壊の制限 (nucleusLimits は /run/initialize より後に書く必要がある) ---
NL_CMD="/process/had/rdm/nucleusLimits"
NL_COMMENT="# ビーム核種(${LABEL})のみ崩壊させる。娘核以降の崩壊は計算しない"
if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then
  if grep -q "^${NL_CMD} " "$MAC"; then
    # すでにある場合は値(とコメント)だけ更新
    sed -i -e "s|^${NL_CMD} .*|${NL_CMD} ${A} ${A} ${Z} ${Z}|" \
           -e "s|^# ビーム核種(.*)のみ崩壊させる.*|${NL_COMMENT}|" "$MAC"
  else
    # ない場合は /run/initialize の直後に挿入
    grep -q "^/run/initialize" "$MAC" || { echo "ERROR: $MAC に /run/initialize がありません" >&2; exit 1; }
    awk -v cmd="${NL_CMD} ${A} ${A} ${Z} ${Z}" -v cmt="${NL_COMMENT}" \
      '{ print } /^\/run\/initialize/ && !done { print cmt; print cmd; done = 1 }' \
      "$MAC" > "$MAC.tmp"
    cat "$MAC.tmp" > "$MAC" && rm -f "$MAC.tmp"
  fi
  grep -qF "${NL_CMD} ${A} ${A} ${Z} ${Z}" "$MAC" || { echo "ERROR: nucleusLimits の書き込みに失敗" >&2; exit 1; }
else
  # 制限を外す (行を削除)
  # NL_CMD に / が含まれるので、sed の区切り文字には | を使う
  sed -i -e "\|^${NL_CMD} |d" -e "\|^# ビーム核種(.*)のみ崩壊させる|d" "$MAC"
fi

# ------------------------------ 2) SteppingAction.cc -------------------------
# 「ビーム粒子」判定行: else if (...GetPDGEncoding() == <PDG>) {  の数値部分と、その次行のコメントだけを書き換える
# (行内のその他の記述 ( pName == "Np225" || など ) には触れない)
echo "[2] $STEPPING"
apply_sed "$STEPPING" \
  "/else if (.*GetPDGEncoding() == [0-9][0-9]*) {/{
     s|\(else if (.*GetPDGEncoding() == \)[0-9][0-9]*) {|\1${PDG}) {|
     n
     s|AddGenHeavyIon(); // .*|AddGenHeavyIon(); // ${LABEL}（ビーム粒子）のみをカウント|
   }" \
  "GetPDGEncoding() == ${PDG}) {"

# ------------------------------ 3) viewer ------------------------------------
echo "[3] viewer"
for v in "${VIEWERS[@]}"; do
  apply_sed "$v" \
    "s|if (pdg === [0-9][0-9]*) return 'heavy';.*|if (pdg === ${PDG}) return 'heavy'; // ${LABEL} (PDG = 10LZZZAAAI)|" \
    "if (pdg === ${PDG}) return 'heavy';"
done

# ------------------------------ 4) execute script ----------------------------
echo "[4] $EXEC_SH"
apply_sed "$EXEC_SH" \
  "s|SiViewer_[0-9A-Za-z]*All\.html|SiViewer_${LABEL}All.html|g" \
  "SiViewer_${LABEL}All.html"

# ------------------------------ 注意喚起 -------------------------------------
echo
if [ "$Z" -eq 2 ] && [ "$A" -eq 4 ]; then
  echo "注意: ビームが alpha (He-4) です。SteppingAction.cc では alpha 判定が先に評価され、"
  echo "      ビューアでも alpha 分類が先なので、ビームは 'Heavy Ion' に分類されません。"
fi
if [ "$PDG" -eq 2212 ]; then
  echo "注意: ビームが陽子です (PDG=2212)。"
fi
if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then
  echo "注意: nucleusLimits は G4RadioactiveDecayPhysics が登録されている場合のみ使えます。"
  echo "      娘核以降の崩壊も見たいときは --full-chain を付けてください。"
fi
echo "完了。元ファイルは <file>.orig として残してあります。"
echo "次の手順: ./execute_makeSiViewer.sh  (exampleB1 の再ビルド → 実行 → HTML 生成)"

if [ "$RUN_AFTER" -eq 1 ]; then
  echo; echo "=== execute_makeSiViewer.sh を実行します ==="
  bash "./$EXEC_SH"
fi
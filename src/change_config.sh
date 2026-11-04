#!/bin/bash
# =============================================================================
# change_config.sh : ビーム粒子設定と検出器の素子配置を一括で変更・実行する
#
# 実行場所 : src/ ディレクトリ  (./change_config.sh)
# 使い方 :
#   ./change_config.sh [<Z> <A> <Q> <E_MeV>] [--nx <N>] [--ny <N>] [--nz <N>] [--run]
#
#   例1) 204Rn 43+, 146 MeV に設定し、素子配置を nx=24, nz=12 に変更して実行
#        ./change_config.sh 86 204 43 146 --nx 24 --nz 12 --run
#   例2) ビームはそのまま、nz だけ 12 にして実行 (指定しなかった項目は現在のソースの値のまま)
#        ./change_config.sh --nz 12 --run
#
# オプション:
#   --run          : 書き換え後に再ビルド・実行・HTML生成まで行う
#   --full-chain   : 娘核以降も含めたすべての崩壊系列を計算する
#   --decay-only   : ビーム核種だけを崩壊させる (ビームを指定したときの既定)
#   --nx, --ny, --nz : x, y, z 方向の素子数 (指定なしは現在のソースの値のまま)
#   --dry-run      : 変更対象を表示するだけで書き換えない
#   --restore      : --run の終了後に素子配置を既定値(20x30x10)に戻す
#
# ビームの4つの値 (Z A Q E) は、すべて指定するか、まったく指定しないかのどちらか。
# ビームを指定しなければ、ビームの設定 (run.mac など) は変更しない。
# 失敗したときは、素子数を実行前の値に自動で戻す (ソースが食い違った状態を残さない)。
# ../include/EventAction.hh が古い形式 (kNPixelTotal が固定値) なら、初回に自動で直す。
# =============================================================================
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

# ------------------------------ 既定値・設定 ---------------------------------
DEFAULT_PIX=(20 30 10)     # --restore のときの戻し先 (nx ny nz)
DIMS=(X Y Z)

# [対象ファイル]
MAC="../build/run.mac"
STEPPING="SteppingAction.cc"
DET="DetectorConstruction.cc"
EVENT_HH="../include/EventAction.hh"
EXEC_SH="execute_makeSiViewer.sh"
VIEWERS=("makeSiViewer_all.cc")
EXTRA_FILES=()             # 例: ("EventAction.cc" "../include/DetectorConstruction.hh")

# DetectorConstruction.cc などで「<名前> = <整数>;」と書かれている素子数の変数名の候補
names_X=(nPixX kNPixX NPixX nx nX NX nPixelsX nPixelX numPixX)
names_Y=(nPixY kNPixY NPixY ny nY NY nPixelsY nPixelY numPixY)
names_Z=(nPixZ kNPixZ NPixZ nz nZ NZ nPixelsZ nPixelZ numPixZ)
# EventAction.hh と makeSiViewer_all*.cc の定数名は kNPixX / kNPixY / kNPixZ (固定)
# -----------------------------------------------------------------------------

# ------------------------------ 現在のピクセル数 (ビューアの kNPixX/Y/Z) -----
get_cur() { grep -hoE "\\bkNPix$1[[:space:]]*=[[:space:]]*[0-9]+" "${VIEWERS[0]}" 2>/dev/null | grep -oE '[0-9]+$' | head -1 || true; }
CUR=()
for i in 0 1 2; do
  c=$(get_cur "${DIMS[$i]}")
  [ -n "$c" ] || { echo "ERROR: ${VIEWERS[0]} に kNPix${DIMS[$i]} = <整数>; が見つかりません" >&2; exit 1; }
  CUR+=("$c")
done
NEW_PIX=("${CUR[@]}")      # 指定されなかった方向は「現在のソースの値」のまま

# ------------------------------ 引数パース -----------------------------------
DRY=0; AUTO_RESTORE=0; RUN_AFTER=0
DECAY_ONLY_BEAM=1; DECAY_FLAG=0
POS=()

setdim() {   # <添字> <値> <オプション名>
  if ! { [[ "$2" =~ ^[0-9]+$ ]] && [ "$2" -ge 1 ] && [ "$2" -le 100 ]; }; then
    echo "ERROR: $3 は 1〜100 の整数で指定してください (指定値: '$2')" >&2; exit 1
  fi
  NEW_PIX[$1]="$2"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --nx) setdim 0 "${2:-}" --nx; shift 2 ;;
    --ny) setdim 1 "${2:-}" --ny; shift 2 ;;
    --nz) setdim 2 "${2:-}" --nz; shift 2 ;;
    --nx=*) setdim 0 "${1#*=}" --nx; shift ;;
    --ny=*) setdim 1 "${1#*=}" --ny; shift ;;
    --nz=*) setdim 2 "${1#*=}" --nz; shift ;;
    --run) RUN_AFTER=1; shift ;;
    --full-chain) DECAY_ONLY_BEAM=0; DECAY_FLAG=1; shift ;;
    --decay-only) DECAY_ONLY_BEAM=1; DECAY_FLAG=1; shift ;;
    --dry-run) DRY=1; shift ;;
    --restore) AUTO_RESTORE=1; shift ;;
    -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
    -*) echo "ERROR: 不明な引数: $1" >&2; exit 1 ;;
    *) POS+=("$1"); shift ;;
  esac
done
if [ "$AUTO_RESTORE" -eq 1 ] && [ "$RUN_AFTER" -eq 0 ] && [ "$DRY" -eq 0 ]; then
  echo "ERROR: --restore は --run と一緒に指定してください (実行せずに戻すと意味がありません)" >&2; exit 1
fi

# ------------------------------ ビームの値の決定 -----------------------------
# ビームを指定しなければ、ビームの設定は変更しない (以前のように 204Rn に戻したりしない)
NPOS=${#POS[@]}
if [ "$NPOS" -ne 0 ] && [ "$NPOS" -ne 4 ]; then
  echo "ERROR: ビームは <Z> <A> <Q> <E_MeV> の4つをすべて指定してください (指定数: $NPOS)" >&2; exit 1
fi
BEAM_APPLY=0
if [ "$NPOS" -eq 4 ]; then
  Z=${POS[0]}; A=${POS[1]}; Q=${POS[2]}; E=${POS[3]}; BEAM_APPLY=1
elif [ "$DECAY_FLAG" -eq 1 ]; then
  # 崩壊の設定だけ変える場合は、run.mac の現在のビームをそのまま使う
  ION_LINE=$(grep -E '^/gun/ion[[:space:]]' "$MAC" 2>/dev/null | head -1 || true)
  E_LINE=$(grep -E '^/gun/energy[[:space:]]' "$MAC" 2>/dev/null | head -1 || true)
  [ -n "$ION_LINE" ] && [ -n "$E_LINE" ] || { echo "ERROR: $MAC に /gun/ion, /gun/energy が見つかりません" >&2; exit 1; }
  read -r _ Z A Q <<< "$ION_LINE"; read -r _ E _ <<< "$E_LINE"
  BEAM_APPLY=1
fi

if [ "$BEAM_APPLY" -eq 1 ]; then
  is_int()  { [[ "$1" =~ ^[0-9]+$ ]]; }
  is_num()  { [[ "$1" =~ ^[0-9]+(\.[0-9]+)?$ ]]; }
  is_int "$Z" && is_int "$A" && is_int "$Q" || { echo "ERROR: Z, A, Q は 0 以上の整数" >&2; exit 1; }
  is_num "$E" || { echo "ERROR: E は正の数値" >&2; exit 1; }
  [ "$Z" -ge 1 ] && [ "$Z" -le 118 ] || { echo "ERROR: Z は 1〜118" >&2; exit 1; }
  [ "$A" -ge "$Z" ] || { echo "ERROR: A >= Z としてください" >&2; exit 1; }
  [ "$Q" -le "$Z" ] || { echo "ERROR: Q <= Z としてください" >&2; exit 1; }

  N=$((A - Z))
  SYMBOLS=(X H He Li Be B C N O F Ne Na Mg Al Si P S Cl Ar K Ca Sc Ti V Cr Mn Fe Co Ni Cu Zn Ga Ge As Se Br Kr \
    Rb Sr Y Zr Nb Mo Tc Ru Rh Pd Ag Cd In Sn Sb Te I Xe Cs Ba La Ce Pr Nd Pm Sm Eu Gd Tb Dy Ho Er Tm Yb Lu \
    Hf Ta W Re Os Ir Pt Au Hg Tl Pb Bi Po At Rn Fr Ra Ac Th Pa U Np Pu Am Cm Bk Cf Es Fm Md No Lr Rf Db Sg \
    Bh Hs Mt Ds Rg Cn Nh Fl Mc Lv Ts Og)
  SYM=${SYMBOLS[$Z]}
  if [ "$Z" -eq 1 ] && [ "$A" -eq 1 ]; then PDG=2212; else PDG=$((1000000000 + Z * 10000 + A * 10)); fi
  LABEL="${A}${SYM}"
  if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then FULL_JS=false; else FULL_JS=true; fi
fi

# 値が変わるピクセルの方向
CHG=()
for i in 0 1 2; do [ "${NEW_PIX[$i]}" != "${CUR[$i]}" ] && CHG+=("$i"); done

# ------------------------------ 補助関数 -------------------------------------
backup_once() { [ -e "$1.orig" ] || cp -p "$1" "$1.orig"; }
names_of() { eval "echo \"\${names_$1[@]}\""; }

# apply_sed <file> <sed-script> <verify-fixed-string> : sed を実行し、結果を検証する
apply_sed() {
  local f="$1" script="$2" verify="$3"
  [ -f "$f" ] || { echo "ERROR: ファイルが見つかりません: $f" >&2; exit 1; }
  backup_once "$f"
  sed -i -e "$script" "$f"
  grep -qF -- "$verify" "$f" || { echo "ERROR: $f の書き換えに失敗しました (該当行が見つかりません)" >&2; exit 1; }
  echo "  updated: $f"
}

# check_pat <file> <regex> <説明> : 書き換え前に、書き換え対象の行があるか確認する
check_pat() {
  [ -f "$1" ] || { echo "ERROR: ファイルが見つかりません: $1" >&2; return 1; }
  grep -qE -- "$2" "$1" || { echo "ERROR: $1 に $3 が見つかりません (書き換えできません)" >&2; return 1; }
}

MOD=()   # 実際に書き換えたピクセル数の記録 "ファイル|変数名|方向の添字" (復元に使う)
sed_val() { sed -i -E "s/(\\b$2[[:space:]]*=[[:space:]]*)$3([[:space:]]*[;,)])/\\1$4\\2/g" "$1"; }

# rewrite <file> <required|optional> <dim添字> <from> <to> <name>...
#   「<name> = <from>」(; , ) が続くもの) を「<name> = <to>」に書き換える。値が <from> の行だけが対象。
rewrite() {
  local f="$1" mode="$2" d="$3" from="$4" to="$5"; shift 5
  local name re found=0 hint other
  if [ ! -f "$f" ]; then
    if [ "$mode" = "required" ]; then echo "ERROR: ファイルが見つかりません: $f" >&2; return 1; fi
    return 0
  fi
  for name in "$@"; do
    re="\\b${name}[[:space:]]*=[[:space:]]*${from}[[:space:]]*[;,)]"
    grep -qE "$re" "$f" || continue
    if [ "$DRY" -eq 0 ]; then
      backup_once "$f"
      sed_val "$f" "$name" "$from" "$to"
      MOD+=("$f|$name|$d")
      if ! grep -qE "\\b${name}[[:space:]]*=[[:space:]]*${to}[[:space:]]*[;,)]" "$f"; then
        echo "ERROR: $f の ${name} の書き換えに失敗しました" >&2; return 1
      fi
    fi
    echo "  $f : ${name}  ${from} -> ${to}"
    found=$((found + 1))
  done
  if [ "$found" -eq 0 ] && [ "$mode" = "required" ]; then
    echo "ERROR: $f に「<名前> = ${from};」の形の ${DIMS[$d]} 方向の素子数が見つかりません。(探した名前: $*)" >&2
    for name in "$@"; do
      other=$(grep -nE "\\b${name}[[:space:]]*=[[:space:]]*[0-9]+[[:space:]]*[;,)]" "$f" | head -3 || true)
      [ -n "$other" ] && echo "       名前はあるが値が ${from} ではない行 (ソース間で値が食い違っている可能性): $(echo "$other" | tr '\n' ' ')" >&2
    done
    hint=$(grep -nE "=[[:space:]]*${from}[[:space:]]*[;,)]" "$f" | head -5 || true)
    [ -n "$hint" ] && echo "       「= ${from}」を含む行: $(echo "$hint" | tr '\n' ' ')" >&2
    echo "       変数名が違う場合は、このスクリプトの names_${DIMS[$d]} に追記してください。" >&2
    return 1
  fi
  return 0
}

# EventAction.hh が古い形式 (kNPixelTotal が固定値) なら、kNPixX/Y/Z から計算する形に直す。
#   配置を変えるための準備で、実行後も元には戻さない (バックアップは <file>.orig)。
EVENT_PENDING=0   # --dry-run のとき、直す予定であることを覚えておく
migrate_event_hh() {
  local tot cur_tot
  [ -f "$EVENT_HH" ] || { echo "ERROR: ファイルが見つかりません: $EVENT_HH" >&2; return 1; }
  if grep -qE '\bkNPixZ[[:space:]]*=[[:space:]]*[0-9]+' "$EVENT_HH"; then return 0; fi
  if ! grep -qE '\bkNPixelTotal[[:space:]]*=[[:space:]]*[0-9]+[[:space:]]*;' "$EVENT_HH"; then
    echo "ERROR: $EVENT_HH に kNPixZ も、固定値の kNPixelTotal も見つかりません。" >&2
    echo "       kNPixX/kNPixY/kNPixZ を定義し、kNPixelTotal を kNPixX/kNPixY/kNPixZ から計算する形にしてください。" >&2
    return 1
  fi
  cur_tot=$(grep -oE '\bkNPixelTotal[[:space:]]*=[[:space:]]*[0-9]+' "$EVENT_HH" | grep -oE '[0-9]+$' | head -1)
  tot=$((CUR[0] * CUR[1] + 2 * CUR[2] * CUR[1] + 2 * CUR[0] * CUR[2]))
  echo "  $EVENT_HH は古い形式 (kNPixelTotal = ${cur_tot} の固定値) です。"
  echo "    kNPixX/kNPixY/kNPixZ (= ${CUR[0]}, ${CUR[1]}, ${CUR[2]}) から総数を計算する形に直します。"
  if [ "$cur_tot" != "$tot" ]; then
    echo "    注意: 固定値 ${cur_tot} は、現在の配置から計算した総数 ${tot} と一致していません (計算した値になります)。"
  fi
  if [ "$DRY" -eq 1 ]; then
    echo "    (--dry-run のため、実際には変更しません)"
    EVENT_PENDING=1
    return 0
  fi
  backup_once "$EVENT_HH"
  sed -i -E '/^[[:space:]]*\/\/ 素子IDは 0 〜 [0-9]+/d' "$EVENT_HH"      # 古い値 (1599 など) を書いたコメントを消す
  sed -i -E "s|^([[:space:]]*)static constexpr G4int kNPixelTotal[[:space:]]*=[[:space:]]*[0-9]+[[:space:]]*;.*|\\1// 素子の配置。DetectorConstruction.cc / makeSiViewer_all.cc の値と一致させること (change_config.sh が書き換える)\\n\\1static constexpr G4int kNPixX = ${CUR[0]};\\n\\1static constexpr G4int kNPixY = ${CUR[1]};\\n\\1static constexpr G4int kNPixZ = ${CUR[2]};\\n\\1// 素子IDは 0 〜 kNPixelTotal-1 (面0: nx*ny、面1,2: nz*ny ずつ、面3,4: nx*nz ずつ)\\n\\1static constexpr G4int kNPixelTotal = kNPixX * kNPixY + 2 * kNPixZ * kNPixY + 2 * kNPixX * kNPixZ;|" "$EVENT_HH"
  if ! grep -qE "\\bkNPixZ[[:space:]]*=[[:space:]]*${CUR[2]}[[:space:]]*;" "$EVENT_HH"; then
    echo "ERROR: $EVENT_HH の自動修正に失敗しました。" >&2; return 1
  fi
  echo "    -> 修正しました (元のファイルは $EVENT_HH.orig)"
  return 0
}

# 値が変わるピクセル方向すべてについて、全対象ファイルを <現在の値> → <新しい値> に書き換える
apply_layout() {
  local d D f v nm
  for d in "${CHG[@]}"; do
    D="${DIMS[$d]}"
    read -r -a nm <<< "$(names_of "$D")"
    echo " [${D} 方向: ${CUR[$d]} -> ${NEW_PIX[$d]}]"
    rewrite "$DET" required "$d" "${CUR[$d]}" "${NEW_PIX[$d]}" "${nm[@]}" || return 1
    if [ "$EVENT_PENDING" -eq 1 ]; then
      echo "  $EVENT_HH : kNPix${D}  ${CUR[$d]} -> ${NEW_PIX[$d]}  (形式を直した後に書き換えます)"
    else
      rewrite "$EVENT_HH" required "$d" "${CUR[$d]}" "${NEW_PIX[$d]}" "kNPix${D}" || return 1
    fi
    for v in "${VIEWERS[@]}"; do
      rewrite "$v" required "$d" "${CUR[$d]}" "${NEW_PIX[$d]}" "kNPix${D}" || return 1
    done
    for f in ${EXTRA_FILES[@]+"${EXTRA_FILES[@]}"}; do
      rewrite "$f" optional "$d" "${CUR[$d]}" "${NEW_PIX[$d]}" "${nm[@]}" || return 1
    done
  done
  return 0
}

# 取り残しチェック (警告のみ): 現在の配置での素子 ID の境界値が、ほかのソースに数値のまま書かれていないか
scan_hardcoded() {
  local nx="${CUR[0]}" ny="${CUR[1]}" nz="${CUR[2]}" b2 b3 b4 tot f out pat any=0
  b2=$((nx * ny + nz * ny)); b3=$((b2 + nz * ny))
  b4=$((b3 + nx * nz));      tot=$((b4 + nx * nz))
  pat="\\b(${b2}|${b3}|${b4}|${tot})\\b"
  for f in *.cc *.hh ../include/*.hh ../include/*.cc; do
    [ -f "$f" ] || continue
    case "$f" in makeSiViewer*|*/makeSiViewer*) continue ;; esac
    # 形式を直す予定 (--dry-run) の EventAction.hh は、固定値が残っていて当然なので除く
    if [ "$EVENT_PENDING" -eq 1 ] && [ "$f" = "$EVENT_HH" ]; then continue; fi
    out=$(grep -nE "$pat" "$f" | grep -vE '^[0-9]+:[[:space:]]*(//|/\*|\*)' || true)
    if [ -n "$out" ]; then
      if [ "$any" -eq 0 ]; then
        echo "  注意: 現在の配置 (${nx}x${ny}x${nz}) の素子 ID の境界値 (${b2}, ${b3}, ${b4}, ${tot}) と同じ数字が、"
        echo "        次の行に直接書かれています。配置を変えると合わなくなる可能性があるので、確認してください。"
        any=1
      fi
      echo "$out" | sed "s|^|      ${f}:|"
    fi
  done
  return 0
}

# 書き換え前の確認 (ビーム関連): 書き換え対象の行がすべてあるか。なければ何も変更せずに止まる
preflight_beam() {
  local v ok=0
  check_pat "$MAC" '^/gun/ion[[:space:]]' "/gun/ion の行" || ok=1
  check_pat "$MAC" '^/gun/energy[[:space:]]' "/gun/energy の行" || ok=1
  if [ "$DECAY_ONLY_BEAM" -eq 1 ] && ! grep -qE '^/process/had/rdm/nucleusLimits[[:space:]]' "$MAC" 2>/dev/null; then
    check_pat "$MAC" '^/run/initialize' "/run/initialize の行 (nucleusLimits の挿入位置)" || ok=1
  fi
  check_pat "$STEPPING" 'else if \(.*GetPDGEncoding\(\) == [0-9]+\) \{' "ビーム粒子の判定行 (GetPDGEncoding() == <PDG>) {)" || ok=1
  for v in "${VIEWERS[@]}"; do
    check_pat "$v" "if \(pdg === [0-9]+\) return 'heavy';" "'heavy' の判定行 (if (pdg === <PDG>) return 'heavy';)" || ok=1
    check_pat "$v" '^var BEAM_INFO = ' "行頭の var BEAM_INFO = の行" || ok=1
  done
  check_pat "$EXEC_SH" 'SiViewer_[0-9A-Za-z]*All\.html' "出力 HTML 名 (SiViewer_*All.html)" || ok=1
  return $ok
}

# 終了時の処理: 一時ファイルを消し、必要ならピクセル数を戻す
#   --restore    : 既定値 (20x30x10) に戻す
#   失敗したとき : 実行前の値に戻す (ソースが食い違った状態を残さない)
TMP_EXEC=""
restore() {
  local rc=$? m f n d label
  local target=()
  trap - EXIT INT TERM
  if [ -n "$TMP_EXEC" ]; then rm -f "$TMP_EXEC"; fi
  if [ "${#MOD[@]}" -gt 0 ]; then
    if [ "$AUTO_RESTORE" -eq 1 ]; then
      target=("${DEFAULT_PIX[@]}"); label="既定値 (${DEFAULT_PIX[0]}x${DEFAULT_PIX[1]}x${DEFAULT_PIX[2]})"
    elif [ "$rc" -ne 0 ]; then
      target=("${CUR[@]}"); label="実行前の値 (${CUR[0]}x${CUR[1]}x${CUR[2]})"
    fi
    if [ "${#target[@]}" -gt 0 ]; then
      echo
      if [ "$rc" -ne 0 ] && [ "$AUTO_RESTORE" -eq 0 ]; then echo "=== 失敗したので、素子数を${label}に戻します ==="
      else echo "=== 素子数を${label}に戻します ==="; fi
      for m in "${MOD[@]}"; do
        IFS='|' read -r f n d <<< "$m"
        if sed_val "$f" "$n" "${NEW_PIX[$d]}" "${target[$d]}"; then
          echo "  $f : $n  ${NEW_PIX[$d]} -> ${target[$d]}"
        else
          echo "WARNING: $f の $n を戻せませんでした。手で戻してください。" >&2
        fi
      done
    fi
  fi
  exit "$rc"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# ============================== メイン処理 ===================================
echo "=== 設定パラメータ ==="
if [ "$BEAM_APPLY" -eq 1 ]; then
  echo "[Beam  ] ${LABEL} (Z=${Z}, N=${N}, A=${A}, Q=${Q}+), E=${E} MeV, PDG=${PDG}"
  if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then echo "         崩壊: ${LABEL} のみ (nucleusLimits ${A} ${A} ${Z} ${Z})"
  else echo "         崩壊: 制限なし (崩壊系列をすべて計算)"; fi
else
  echo "[Beam  ] 変更なし (run.mac などの現在の設定のまま)"
fi
echo "[Pixels] nx=${NEW_PIX[0]}, ny=${NEW_PIX[1]}, nz=${NEW_PIX[2]} (現在のソース: ${CUR[0]}, ${CUR[1]}, ${CUR[2]})"
[ "$DRY" -eq 1 ] && echo "(--dry-run: 実際の書き換えは行いません)"
echo

# ------------------------------ [0] 準備と事前確認 ---------------------------
# 何かを書き換える前に、書き換え対象がすべてそろっているか確認する (途中で失敗してずれた状態を残さないため)
if [ "$BEAM_APPLY" -eq 1 ]; then preflight_beam; fi      # ここで止まったときは、何も変更されない
if [ "${#CHG[@]}" -gt 0 ]; then
  echo "[0] EventAction.hh の形式確認"
  migrate_event_hh
fi
if [ "${#CHG[@]}" -gt 0 ] && [ "$DRY" -eq 0 ]; then
  DRY=1; apply_layout > /dev/null; DRY=0      # 書き換えずに確認だけ (エラーは表示される)
fi

# ------------------------------ [1] ビーム書き換え ---------------------------
if [ "$BEAM_APPLY" -eq 1 ]; then
  if [ "$DRY" -eq 1 ]; then
    echo "[1] ビーム関連: ${MAC}, ${STEPPING}, ${VIEWERS[*]}, ${EXEC_SH} を書き換えます (--dry-run のため未実行)"
  else
    echo "[1] ビーム関連の書き換え"
    NL_CMD="/process/had/rdm/nucleusLimits"
    NL_COMMENT="# ビーム核種(${LABEL})のみ崩壊させる。娘核以降の崩壊は計算しない"
    apply_sed "$MAC" \
      "s|^/gun/ion .*|/gun/ion ${Z} ${A} ${Q}|; s|^/gun/energy .*|/gun/energy ${E} MeV|; s|^# Z=.*|# Z=${Z}, A=${A}, 電荷Q=${Q} (${LABEL} ${Q}+)|" \
      "/gun/ion ${Z} ${A} ${Q}"
    if [ "$DECAY_ONLY_BEAM" -eq 1 ]; then
      if grep -q "^${NL_CMD} " "$MAC"; then
        sed -i -e "s|^${NL_CMD} .*|${NL_CMD} ${A} ${A} ${Z} ${Z}|" -e "s|^# ビーム核種(.*)のみ崩壊させる.*|${NL_COMMENT}|" "$MAC"
      else
        awk -v cmd="${NL_CMD} ${A} ${A} ${Z} ${Z}" -v cmt="${NL_COMMENT}" \
          '{ print } /^\/run\/initialize/ && !done { print cmt; print cmd; done = 1 }' "$MAC" > "$MAC.tmp"
        cat "$MAC.tmp" > "$MAC" && rm -f "$MAC.tmp"
      fi
      grep -qF "${NL_CMD} ${A} ${A} ${Z} ${Z}" "$MAC" || { echo "ERROR: nucleusLimits の書き込みに失敗" >&2; exit 1; }
    else
      sed -i -e "\|^${NL_CMD} |d" -e "\|^# ビーム核種(.*)のみ崩壊させる|d" "$MAC"
    fi

    apply_sed "$STEPPING" \
      "/else if (.*GetPDGEncoding() == [0-9][0-9]*) {/{
         s|\(else if (.*GetPDGEncoding() == \)[0-9][0-9]*) {|\1${PDG}) {|
         n
         s|AddGenHeavyIon(); // .*|AddGenHeavyIon(); // ${LABEL}（ビーム粒子）のみをカウント|
       }" \
      "GetPDGEncoding() == ${PDG}) {"

    BEAM_JS="var BEAM_INFO = {label: '${LABEL}', symbol: '${SYM}', Z: ${Z}, A: ${A}, Q: ${Q}, E: ${E}, fullChain: ${FULL_JS}};"
    for v in "${VIEWERS[@]}"; do
      apply_sed "$v" \
        "s|if (pdg === [0-9][0-9]*) return 'heavy';.*|if (pdg === ${PDG}) return 'heavy'; // ${LABEL} (PDG = 10LZZZAAAI)|" \
        "if (pdg === ${PDG}) return 'heavy';"
      apply_sed "$v" "s|^var BEAM_INFO = .*|${BEAM_JS} // change_config.sh が書き換える|" "${BEAM_JS}"
    done

    apply_sed "$EXEC_SH" "s|SiViewer_[0-9A-Za-z]*All\.html|SiViewer_${LABEL}All.html|g" "SiViewer_${LABEL}All.html"
  fi
else
  echo "[1] ビーム関連: 変更なし"
fi

# ------------------------------ [2] ピクセル書き換え -------------------------
echo "[2] 素子数の書き換え"
if [ "${#CHG[@]}" -gt 0 ]; then
  apply_layout
  scan_hardcoded
  for i in 0 1 2; do
    if [ "${NEW_PIX[$i]}" -gt "${DEFAULT_PIX[$i]}" ]; then
      echo "注意: 素子数を既定値より増やすと、検出器の箱が Envelope(env_sizeXY, env_sizeZ) に収まらないことがあります。"
      echo "      Geometry overlap のエラーが出たら DetectorConstruction.cc の Envelope を大きくしてください。"
      break
    fi
  done
else
  echo "  現在のソースと同じ配置のため、素子数の書き換えはありません。"
fi

if [ "$DRY" -eq 1 ]; then echo; echo "完了 (--dry-run)"; exit 0; fi

# ------------------------------ [3] 実行・HTML生成 ---------------------------
if [ "$RUN_AFTER" -eq 1 ]; then
  echo
  echo "[3] $EXEC_SH を実行 (ビルド → 実行 → HTML 生成)"

  # 出力 HTML 名の末尾に素子配置を付けた一時コピーを作って実行する
  HTML_BASE=$(grep -o 'SiViewer_[0-9A-Za-z_]*\.html' "$EXEC_SH" | head -1 || true)
  [ -n "$HTML_BASE" ] || { echo "ERROR: $EXEC_SH に出力 HTML 名 (SiViewer_*.html) が見つかりません" >&2; exit 1; }
  HTML_NEW="${HTML_BASE%.html}_nx${NEW_PIX[0]}_ny${NEW_PIX[1]}_nz${NEW_PIX[2]}.html"
  TMP_EXEC=".execute_combined.tmp.sh"
  sed "s|${HTML_BASE}|${HTML_NEW}|g" "$EXEC_SH" > "$TMP_EXEC"

  START_TS=$(date +%s)
  RC=0
  # 最後の explorer.exe は成功しても 1 を返すことがあるので、成否は HTML ができたかで判断する
  bash -e "./$TMP_EXEC" || RC=$?
  rm -f "$TMP_EXEC"; TMP_EXEC=""

  if [ -f "$HTML_NEW" ] && [ "$(stat -c %Y "$HTML_NEW")" -ge "$START_TS" ]; then
    echo
    echo "完了: $HTML_NEW  (nx=${NEW_PIX[0]}, ny=${NEW_PIX[1]}, nz=${NEW_PIX[2]})"
  else
    echo "ERROR: $HTML_NEW が作られませんでした ($EXEC_SH の終了コード: $RC)" >&2
    exit 1
  fi
else
  echo
  echo "完了。次の手順: ./execute_makeSiViewer.sh  (または --run を付けて再実行)"
fi
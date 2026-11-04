#!/bin/bash
# =============================================================================
# change_pixels.sh : 検出器の素子配置 (x, y, z 方向の素子数 nx, ny, nz) を変えて、
#                    再ビルド → 実行 → HTML 生成 まで行い、最後に既定値に戻す
#
# 実行場所 : src/ ディレクトリ  (./change_pixels.sh)
# 書き換え : DetectorConstruction.cc    ( x, y, z 方向の素子数 )
#            ../include/EventAction.hh  ( kNPixX / kNPixY / kNPixZ。総数 kNPixelTotal は自動で計算される )
#            makeSiViewer_all*.cc       ( kNPixX / kNPixY / kNPixZ )
#            ※ ほかにも素子数を持つファイルがあれば、下の EXTRA_FILES に追記する
# 内部で使うもの :
#   execute_makeSiViewer.sh  ビルド → 実行 → ビューア生成。出力 HTML 名の末尾に _nx<N>_ny<N>_nz<N> を
#                            付けた一時コピーを実行する (元のファイルは変更しない)
#   change_beam.sh           VIEWERS / EXEC_SH の設定を流用する (あれば)
#
# 使い方 :
#   ./change_pixels.sh --nz 12                 nz=12 (nx, ny は既定値) で実行し、終了後に既定値へ戻す
#   ./change_pixels.sh --nx 24 --ny 28 --nz 12 x, y, z をすべて指定
#   ./change_pixels.sh                         既定値 (nx=20, ny=30, nz=10) で実行
#   ./change_pixels.sh --nz 12 --dry-run       書き換え対象を表示するだけ (何も変更しない)
#   ./change_pixels.sh --nz 12 --no-restore    終了後に既定値へ戻さない
#   (指定しなかった方向は既定値になる。--nz=12 のように = でもよい)
#
# 書き換えるのは「<名前> = <現在の値>」の行だけ (現在の値 = ビューアの kNPixX/Y/Z)。
# loop の変数 (nx = 0; など) には触れない。
# 途中で失敗したり Ctrl-C で止めたりしても、自動的に既定値へ戻す。
# =============================================================================
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

# ------------------------------ 既定値 ---------------------------------------
DEFAULT=(20 30 10)     # nx ny nz の既定値。実行後はこの値に戻す
DIMS=(X Y Z)           # 方向の名前 (添字 0,1,2 = x,y,z)

# ------------------------------ 書き換え対象 ---------------------------------
DET="DetectorConstruction.cc"
EVENT_HH="../include/EventAction.hh"
VIEWERS=("makeSiViewer_all.cc")
EXEC_SH="execute_makeSiViewer.sh"
EXTRA_FILES=()         # 例: ("EventAction.cc" "../include/DetectorConstruction.hh")

# DetectorConstruction.cc などで「<名前> = <整数>;」と書かれている素子数の変数名の候補。
# 名前が違う場合はここに追記する。
names_X=(nPixX kNPixX NPixX nx nX NX nPixelsX nPixelX numPixX)
names_Y=(nPixY kNPixY NPixY ny nY NY nPixelsY nPixelY numPixY)
names_Z=(nPixZ kNPixZ NPixZ nz nZ NZ nPixelsZ nPixelZ numPixZ)
# EventAction.hh と makeSiViewer_all*.cc の定数名は kNPixX / kNPixY / kNPixZ (固定)

# change_beam.sh と同じ設定を使う (あれば)
if [ -f change_beam.sh ]; then
  eval "$(grep -E '^(EXEC_SH|VIEWERS)=' change_beam.sh)" || true
fi

# ------------------------------ 引数 -----------------------------------------
NEW=("${DEFAULT[@]}"); DRY=0; NO_RESTORE=0
usage_err() { echo "使い方: ./change_pixels.sh [--nx N] [--ny N] [--nz N] [--dry-run] [--no-restore]" >&2; exit 1; }
setdim() {   # <添字> <値> <オプション名>
  if ! { [[ "$2" =~ ^[0-9]+$ ]] && [ "$2" -ge 1 ] && [ "$2" -le 100 ]; }; then
    echo "ERROR: $3 は 1〜100 の整数で指定してください (指定値: '$2')" >&2; exit 1
  fi
  NEW[$1]="$2"
}
while [ $# -gt 0 ]; do
  case "$1" in
    --nx) setdim 0 "${2:-}" --nx; shift 2 ;;
    --ny) setdim 1 "${2:-}" --ny; shift 2 ;;
    --nz) setdim 2 "${2:-}" --nz; shift 2 ;;
    --nx=*) setdim 0 "${1#*=}" --nx; shift ;;
    --ny=*) setdim 1 "${1#*=}" --ny; shift ;;
    --nz=*) setdim 2 "${1#*=}" --nz; shift ;;
    --dry-run)    DRY=1; shift ;;
    --no-restore) NO_RESTORE=1; shift ;;
    -h|--help)    sed -n '2,27p' "$0"; exit 0 ;;
    *) echo "ERROR: 不明な引数: $1" >&2; usage_err ;;
  esac
done

# ------------------------------ 現在の値 (ビューアの kNPixX/Y/Z) -------------
get_cur() { grep -hoE "\\bkNPix$1[[:space:]]*=[[:space:]]*[0-9]+" "${VIEWERS[0]}" 2>/dev/null | grep -oE '[0-9]+$' | head -1 || true; }
CUR=()
for i in 0 1 2; do
  c=$(get_cur "${DIMS[$i]}")
  [ -n "$c" ] || { echo "ERROR: ${VIEWERS[0]} に kNPix${DIMS[$i]} = <整数>; が見つかりません" >&2; exit 1; }
  CUR+=("$c")
done
CHG=()   # 値が変わる方向の添字
for i in 0 1 2; do [ "${NEW[$i]}" != "${CUR[$i]}" ] && CHG+=("$i"); done

# ------------------------------ 補助関数 -------------------------------------
backup_once() { [ -e "$1.orig" ] || cp -p "$1" "$1.orig"; }
names_of() { eval "echo \"\${names_$1[@]}\""; }

MOD=()   # 実際に書き換えた "ファイル|変数名|方向の添字" の記録 (復元に使う)
# sed_val <file> <name> <from> <to> : 「<name> = <from>」の <from> を <to> にする
sed_val() { sed -i -E "s/(\\b$2[[:space:]]*=[[:space:]]*)$3([[:space:]]*[;,)])/\\1$4\\2/g" "$1"; }

# rewrite <file> <required|optional> <dim添字> <from> <to> <name>...
#   <file> の「<name> = <from>」(; , ) のどれかが続くもの) を「<name> = <to>」に書き換える。
#   値が <from> と一致する行だけが対象。--dry-run のときは表示だけ。
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
    grep -nE "\\b${name}[[:space:]]*=[[:space:]]*(${from}|${to})[[:space:]]*[;,)]" "$f" | sed 's/^/      /'
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
    if [ "$f" = "$EVENT_HH" ]; then
      echo "       ($f は kNPix${DIMS[$d]} を定義し、kNPixelTotal を kNPixX/kNPixY/kNPixZ から計算する形にしてください)" >&2
    fi
    return 1
  fi
  return 0
}

# 値が変わる方向すべてについて、全対象ファイルを <現在の値> → <新しい値> に書き換える
apply_layout() {
  local d D f v nm
  for d in "${CHG[@]}"; do
    D="${DIMS[$d]}"
    read -r -a nm <<< "$(names_of "$D")"
    echo " [${D} 方向: ${CUR[$d]} -> ${NEW[$d]}]"
    rewrite "$DET" required "$d" "${CUR[$d]}" "${NEW[$d]}" "${nm[@]}" || return 1
    rewrite "$EVENT_HH" required "$d" "${CUR[$d]}" "${NEW[$d]}" "kNPix${D}" || return 1
    for v in "${VIEWERS[@]}"; do
      rewrite "$v" required "$d" "${CUR[$d]}" "${NEW[$d]}" "kNPix${D}" || return 1
    done
    for f in ${EXTRA_FILES[@]+"${EXTRA_FILES[@]}"}; do
      rewrite "$f" optional "$d" "${CUR[$d]}" "${NEW[$d]}" "${nm[@]}" || return 1
    done
  done
  return 0
}

# 取り残しチェック (警告のみ):
#   現在の配置での素子 ID の境界値 (面ごとの素子数の累計) が、ほかのソースに数値のまま
#   書かれていないか探す。見つかった行は配置を変えると合わなくなる可能性がある。
scan_hardcoded() {
  local nx="${CUR[0]}" ny="${CUR[1]}" nz="${CUR[2]}" b2 b3 b4 tot f out pat any=0
  b2=$((nx * ny + nz * ny)); b3=$((b2 + nz * ny))
  b4=$((b3 + nx * nz));      tot=$((b4 + nx * nz))
  pat="\\b(${b2}|${b3}|${b4}|${tot})\\b"
  for f in *.cc *.hh ../include/*.hh ../include/*.cc; do
    [ -f "$f" ] || continue
    case "$f" in makeSiViewer*|*/makeSiViewer*) continue ;; esac
    # コメントだけの行は除く
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

# 終了時 (成功・失敗・Ctrl-C のどれでも) に既定値へ戻し、一時ファイルを消す
TMP_EXEC=""
restore() {
  local rc=$? m f n d
  trap - EXIT INT TERM
  if [ -n "$TMP_EXEC" ]; then rm -f "$TMP_EXEC"; fi
  if [ "${#MOD[@]}" -gt 0 ] && [ "$NO_RESTORE" -eq 0 ]; then
    echo
    echo "=== 素子数を既定値 (nx=${DEFAULT[0]}, ny=${DEFAULT[1]}, nz=${DEFAULT[2]}) に戻します ==="
    for m in "${MOD[@]}"; do
      IFS='|' read -r f n d <<< "$m"
      if sed_val "$f" "$n" "${NEW[$d]}" "${DEFAULT[$d]}"; then
        echo "  $f : $n  ${NEW[$d]} -> ${DEFAULT[$d]}"
      else
        echo "WARNING: $f の $n を戻せませんでした。手で戻してください。" >&2
      fi
    done
    echo "  (build/ の実行ファイルは次回の make で既定値に作り直されます)"
  elif [ "${#MOD[@]}" -gt 0 ]; then
    echo
    echo "注意: --no-restore のため nx=${NEW[0]}, ny=${NEW[1]}, nz=${NEW[2]} のままです。"
    echo "      戻すには ./change_pixels.sh を引数なしで実行してください。"
  fi
  exit "$rc"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# ------------------------------ 1) 書き換え ----------------------------------
echo "=== 素子配置: nx=${NEW[0]}, ny=${NEW[1]}, nz=${NEW[2]}  (既定値 ${DEFAULT[0]}, ${DEFAULT[1]}, ${DEFAULT[2]} / 現在のソース ${CUR[0]}, ${CUR[1]}, ${CUR[2]}) ==="
[ "$DRY" -eq 1 ] && echo "(--dry-run: 何も変更しません)"
echo "[1] 素子数の書き換え"
if [ "${#CHG[@]}" -eq 0 ]; then
  echo "  現在のソースと同じ配置なので、書き換えはありません。"
else
  apply_layout
  scan_hardcoded
  for i in 0 1 2; do
    if [ "${NEW[$i]}" -gt "${DEFAULT[$i]}" ]; then
      echo "注意: 素子数を既定値より増やすと、検出器の箱が Envelope(env_sizeXY, env_sizeZ) に収まらないことがあります。"
      echo "      Geometry overlap のエラーが出たら DetectorConstruction.cc の Envelope を大きくしてください。"
      break
    fi
  done
fi
if [ "$DRY" -eq 1 ]; then echo; echo "完了 (--dry-run)"; exit 0; fi

# ------------------------------ 2) 実行 --------------------------------------
# execute_makeSiViewer.sh の出力 HTML 名の末尾に _nx<N>_ny<N>_nz<N> を付けた一時コピーを作って実行する
echo
echo "[2] $EXEC_SH を実行 (ビルド → 実行 → HTML 生成)"
HTML_BASE=$(grep -o 'SiViewer_[0-9A-Za-z_]*\.html' "$EXEC_SH" | head -1 || true)
[ -n "$HTML_BASE" ] || { echo "ERROR: $EXEC_SH に出力 HTML 名 (SiViewer_*.html) が見つかりません" >&2; exit 1; }
HTML_NEW="${HTML_BASE%.html}_nx${NEW[0]}_ny${NEW[1]}_nz${NEW[2]}.html"
TMP_EXEC=".execute_pixels.tmp.sh"
sed "s|${HTML_BASE}|${HTML_NEW}|g" "$EXEC_SH" > "$TMP_EXEC"

START_TS=$(date +%s)
RC=0
# -e: ビルドの失敗などで止める。最後の explorer.exe は成功しても 1 を返すことがあるので、
#     成否は終了コードではなく HTML ができたかどうかで判断する。
bash -e "./$TMP_EXEC" || RC=$?
rm -f "$TMP_EXEC"; TMP_EXEC=""

if [ -f "$HTML_NEW" ] && [ "$(stat -c %Y "$HTML_NEW")" -ge "$START_TS" ]; then
  echo
  echo "完了: $HTML_NEW  (nx=${NEW[0]}, ny=${NEW[1]}, nz=${NEW[2]})"
else
  echo "ERROR: $HTML_NEW が作られませんでした ($EXEC_SH の終了コード: $RC)" >&2
  exit 1
fi
# この後 trap の restore が既定値に戻す
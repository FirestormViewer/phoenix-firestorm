#!/usr/bin/env bash
#
# AYAstorm r10 routing 検証材料ジェネレータ
#
# spec_5_1ch_placement.md §5.1 に従い、ch ごとに「voice ID + tone」を重ねた
# 30 秒の 6ch Vorbis ファイル (test_routing_5_1.ogg) を生成する。
#
# 用途: P9 の routing 検証 / bleed 定量チェック。各 prim に近付いて
#       `Front Left` / 220Hz、`Front Right` / 440Hz、… が単独で聴こえれば
#       routing OK。隣 prim の voice / freq が混じれば bleed あり。
#
# 一発スクリプト。生成済み ogg は repo に入れず AYA Icecast にホストする
# 想定なので、出力ディレクトリは repo 外 (default ./out_r10_test_material/)。
#
# 依存: espeak-ng, sox, oggenc (vorbis-tools)
#
# 使い方:
#   doc/r10/gen_test_material.sh [output_dir]
#

set -euo pipefail

OUT_DIR="${1:-./out_r10_test_material}"

# 検証材料の識別子と tone freq (§5.1 の表そのまま)。
# voice 文字列の "L F E" は espeak-ng が "L-F-E" と離して読むよう空白挿入。
declare -A VOICE=(
    [FL]="Front Left"
    [FR]="Front Right"
    [C]="Center"
    [LFE]="L F E"
    [SL]="Side Left"
    [SR]="Side Right"
)
declare -A TONE_HZ=(
    [FL]=220
    [FR]=440
    [C]=880
    [LFE]=60
    [SL]=1760
    [SR]=3520
)

# WAV interleave 順 (WAV/SMPTE) — sox -M に渡す。oggenc は RIFF channel
# mask を読んで Vorbis channel mapping family 1 (FL,C,FR,SL,SR,LFE) に
# 並べ替えてくれる。FLAC 変換が必要なら本順序のまま flac CLI に渡せる。
WAV_ORDER=(FL FR C LFE SL SR)

DURATION_SEC=30
SAMPLE_RATE=48000

echo "[gen_test_material] checking prerequisites..."
for tool in espeak-ng sox oggenc; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ERROR: required tool not found in PATH: $tool" >&2
        exit 1
    fi
done

echo "[gen_test_material] output directory: $OUT_DIR"
mkdir -p "$OUT_DIR"
TMP_DIR="$(mktemp -d -t r10_test_material.XXXXXX)"
# 失敗しても中間ファイルを残してデバッグできるよう、明示削除のみ。
trap 'rm -rf "$TMP_DIR"' EXIT

for ch in "${WAV_ORDER[@]}"; do
    voice_text="${VOICE[$ch]}"
    freq="${TONE_HZ[$ch]}"
    echo "[gen_test_material]   $ch — voice=\"$voice_text\" tone=${freq}Hz"

    # espeak-ng は固定 22050 Hz で出すので、tone と sample rate を揃える
    # ため一度リサンプルしてから mix する (sox -m はリサンプル機能を持た
    # ず、sample rate 不一致だと FAIL で抜ける)。
    espeak-ng -v en -w "$TMP_DIR/${ch}_voice_raw.wav" "$voice_text"
    sox "$TMP_DIR/${ch}_voice_raw.wav" -r "$SAMPLE_RATE" \
        "$TMP_DIR/${ch}_voice.wav"
    sox -n -c 1 -r "$SAMPLE_RATE" "$TMP_DIR/${ch}_tone.wav" \
        synth "$DURATION_SEC" sine "$freq"
    # voice + tone を重ねる。voice は短いので残り尺は無音 padding になり、
    # 全長は長い方 (tone = 30s) に揃う。
    sox -m "$TMP_DIR/${ch}_voice.wav" "$TMP_DIR/${ch}_tone.wav" \
        "$TMP_DIR/${ch}.wav"
done

echo "[gen_test_material] interleaving 6ch WAV (order: ${WAV_ORDER[*]})"
sox -M \
    "$TMP_DIR/FL.wav" \
    "$TMP_DIR/FR.wav" \
    "$TMP_DIR/C.wav" \
    "$TMP_DIR/LFE.wav" \
    "$TMP_DIR/SL.wav" \
    "$TMP_DIR/SR.wav" \
    -c 6 "$TMP_DIR/test_routing_5_1.wav"

echo "[gen_test_material] encoding Vorbis (channel mapping family 1)"
oggenc --quiet --quality 5 \
    -o "$OUT_DIR/test_routing_5_1.ogg" \
    "$TMP_DIR/test_routing_5_1.wav"

echo "[gen_test_material] done."
echo "  output: $OUT_DIR/test_routing_5_1.ogg"
echo "  next:   AYA Icecast にアップロードし、prim description の {url} に設定"

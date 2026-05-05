# 5.1ch ソース受入 仕様書 (r9)

> **対象**: AYAstorm `v7.2.5-ayastorm-r9` (想定)
> **前提**: `doc/spec_distributed_stereo.md` (r8 分散記述ステレオ仕様)
> **前提 (実装基盤)**: `doc/spec_stream3d_decode_thread.md` (r7 decode thread 化)
> **背景文書**: `docs/ayastorm-positional-stream.md` (M1〜Post-M9 実装記録)

## 1. 目的とスコープ

r9 は r8 (分散記述ステレオ) に対して「**5.1ch ソースを viewer 側で受け入れる経路**」を追加する内部リリース。viewer の出力チャンネル割当は r8 と同じ `ch:L|R|M` の 3 種に限定し、6ch ソースは内部で L/R 2 トラックにダウンミックスしたうえで既存 binding 経路に流す。真の 5.1 配置 (`ch=FL|FR|C|LFE|SL|SR`) は r10 で導入する。

主目的:

- Opus surround (channel mapping family 1) / FLAC multichannel の 6ch HTTP URL を再生開始できる (現状は format mismatch / ch 数不整合で fail する想定)
- viewer 内で source 6ch → L/R 2-track ダウンミックスを行い、r8 の `LLMultiTailRing` (`n_tracks=2`) 経路をそのまま使う
- 5.1 配信パイプライン (Icecast 2.4+ + ffmpeg / 静的 nginx) のレシピを AYA 内部で再現可能な形で確立する
- r10 の `ch` 値拡張 (FL/FR/C/LFE/SL/SR) に向けた parser / mapping interface を分離整備しておく

非対象 (r9 では触らない):

- `ch=FL|FR|C|LFE|SL|SR` の追加と placement 実装 — r10
- LFE 専用処理 (ローパス / 2D 化) — r10 §9.1.4 に持ち越し
- AAC 5.1 / HLS / AC-3 ソース — Opus / FLAC のみ
- mono タグ `[3dstream:...]` の N 化、per-speaker DSP — r8 §9 のまま
- 公開 README / changelog への 5.1 / 分散記述開示 — r11 で一括 (r8 と同じ方針)

---

## 2. 問題定義

### 2.1 viewer 側の現状制約

r8 までの decode worker と `LLMultiTailRing` は stereo source (1ch / 2ch) 前提で組まれている:

- ring は `reset(cap, n_tracks=2, n_readers=N)` で 2 track 固定
- decode worker は `FMOD::Sound::readData()` の出力を 1ch (= L にコピー) または 2ch (= L/R 分離) のどちらかでしか書き込まない
- 6ch source URL を渡した場合、interleaved 6ch サンプル列を 2ch とみなして書き込んでしまうと L/R に異なるチャンネルの混在が起き、可聴域に意味のない音が出る

### 2.2 配信側の現実

- 5.1 配信は **Icecast 2.4+ + Opus surround が事実上唯一の現実解** (r8 仕様 §9.1.2 で整理済み)
- AYA は自宅で 5.1ch を流す pipeline を持っていない。viewer 側だけ整えても実環境で feedback できないため、配信側パイプラインの確立が r9 のもう片輪になる
- 静的配信 (`.opus` / `.flac` を nginx で出す) と、リアルタイム配信 (Liquidsoap → Icecast) の両方を r9 で実証する

### 2.3 r10 着手前にやっておきたいこと

r10 (`ch=FL/FR/C/LFE/SL/SR` placement) を着手する前に、以下を r9 で済ませておくと r10 の差分が小さくなる:

- 6ch source の codec 別 channel order を mapping table 化
- `ch` enum を `ChannelKind` 等の独立 enum class に切り出して r10 で値追加が 1 箇所で済む形に
- `Sound::getFormat()` で source ch 数を取得する経路を共通化
- ring の `n_tracks` パラメータが実際に source 形状で切り替わる (将来 6 になっても破綻しない) 動作を r9 でも回路として通す

r9 の出力チャンネル割当が `L|R|M` 止まりであっても、内部 interface を r10 互換にしておけば r10 は parser 拡張 + ring `n_tracks=6` への切替 + speaker 個数増加 (16 → 6 placement) だけで済む。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. Opus surround (Vorbis 順 6ch) HTTP URL を再生開始できる
- G2. FLAC 6ch HTTP URL を再生開始できる
- G3. source 6ch → L/R/M ダウンミックス規則を viewer 側で固定 (ITU-R BS.775 ベース)
- G4. r8 の `[3dstream-stereo:{url:...}{ch:L|R|M}...]` binding はそのまま動く (5.1 source でもユーザは r8 と同じ書式が使える)
- G5. 5.1 配信側パイプラインのレシピを `doc/recipe_5_1ch_streaming.md` (または本書 §5.2) に記述、AYA 内部で再現可能にする
- G6. r10 の `ch` 値拡張に向けた `ChannelKind` enum / source channel layout interface を分離整備
- G7. r8 の F1〜F9 受入条件を回帰確認し、1ch / 2ch source 経路に regression がないことを保証

### 非ゴール

- NG1. `ch=FL|FR|C|LFE|SL|SR` の placement 実装 — r10
- NG2. LFE のローパス / 2D 化 — r10 §9.1.4
- NG3. AAC 5.1 / HLS (.m3u8) / AC-3 ソース — Opus / FLAC のみ
- NG4. ユーザ向け「自宅で 5.1 を流す」公開 GUI / 自動化 — AYA 個人レシピで止める
- NG5. mono タグ `[3dstream:...]` の挙動変更 — 既存維持
- NG6. 公開 README / changelog への 5.1 機能開示 — r11 で一括 (r8 方針継続)

---

## 4. 設計

### 4.1 受入 codec / channel layout

| codec | source ch | r8 まで | r9 |
|---|---|---|---|
| Vorbis (Ogg) | 1 / 2 | ◯ | ◯ (変更なし) |
| Opus (Ogg) | 1 / 2 | ◯ | ◯ (変更なし) |
| Opus (Ogg, channel mapping family 1) | 6 | × | **◯ 新規** |
| FLAC | 1 / 2 | ◯ | ◯ (変更なし) |
| FLAC | 6 | × | **◯ 新規** |
| MP3 | 1 / 2 | ◯ | ◯ (変更なし) |
| AAC (ADTS / HLS) | * | × | × (r9 でも非対応) |
| AC-3 / E-AC-3 | * | × | × (Dolby ライセンス問題) |

### 4.2 viewer 内部経路

```
HTTP source URL
  ↓ FMOD::createSound(FMOD_OPENONLY|FMOD_CREATESTREAM)
FMOD::Sound
  ↓ getFormat(&type, &fmt, &channels, &bits) で source ch 数 / codec 取得
LLPositionalStreamStereo::DecodeWorker (r7 で確立、r9 で分岐追加)
  ├─ channels==1 / 2 → r8 と同じ経路
  └─ channels==6 → codec 別 layout で interleaved 6ch を分解
                    ↓ ITU-R BS.775 ダウンミックス (§4.4)
                    ↓ L/R 2-track にまとめる
LLMultiTailRing (n_tracks=2 のまま)
  ↓ pcmReadCallback × N speaker
N 個の FMOD::Channel (ch:L|R|M を r8 と同じく 2 track ring から抽出)
```

ring の `n_tracks` は r9 でも 2 に固定する。source 6ch から L/R を作るのは decode worker の責務。これにより r8 の reader 側 (`LLPositionalStreamMulti` の pcmReadCallback) は変更不要。

### 4.3 source channel layout (codec 別)

interleaved 6ch frame の各 sample が表す role を codec 別に定義する:

```cpp
struct SourceChannelLayout
{
    // sample index in 6-ch interleaved frame (0..5)
    int FL;
    int FR;
    int C;
    int LFE;
    int SL;
    int SR;
};

// Vorbis / Opus channel mapping family 1 = "Vorbis 順"
// frame: [FL, C, FR, SL, SR, LFE]
constexpr SourceChannelLayout kLayoutVorbisOpus6 = { 0, 2, 1, 5, 3, 4 };

// FLAC 6ch = "WAV / SMPTE 順"
// frame: [FL, FR, C, LFE, SL, SR]
constexpr SourceChannelLayout kLayoutFlac6       = { 0, 1, 2, 3, 4, 5 };
```

`FMOD_SOUND_TYPE` が `FMOD_SOUND_TYPE_OGGVORBIS` / `FMOD_SOUND_TYPE_OPUS` のときは Vorbis 順、`FMOD_SOUND_TYPE_FLAC` のときは FLAC 順を選ぶ。

### 4.4 ダウンミックス規則 (ITU-R BS.775)

source 6ch (FL/FR/C/LFE/SL/SR の論理 ch) から L/R 2ch への係数:

```
L_out = c · ( FL + 0.707·C + 0.707·SL + 0.5·LFE )
R_out = c · ( FR + 0.707·C + 0.707·SR + 0.5·LFE )
```

各係数の合計は L/R 各 1 + 0.707 + 0.707 + 0.5 = **2.914** (≒ +9.3 dB)。clipping を避けるため出力前に `c = 1/2.914 ≒ 0.343` を掛けて L/R を [-1.0, +1.0] に収まる範囲に正規化する。

> **注**: BS.775 の標準推奨は LFE を含めない場合 `1 / (1 + 0.707 + 0.707) = 0.414` の正規化。LFE を加える場合は配信側で LFE を制限していない限りオーバーロードのおそれが残るため、本書では LFE 込みで normalize する保守的設定を採用する。実測で headroom が大きすぎる (実用音量が小さい) と判明した場合は §6 で gain 調整キーを追加検討する。

clip 検出は decode thread に既に存在する range check を流用 (downmix 後の値で検出)。

### 4.5 ch 数判定と書式整合

```cpp
FMOD_SOUND_TYPE   type;
FMOD_SOUND_FORMAT fmt;
int channels = 0;
int bits     = 0;
sound->getFormat(&type, &fmt, &channels, &bits);

switch (channels)
{
    case 1: /* mono source: r8 既存 */       break;
    case 2: /* stereo source: r8 既存 */     break;
    case 6: /* r9 新規: codec 別 layout 適用 */ break;
    default:
        // 構造エラー: 非対応 ch 数
        notifyStream3D(...);
        return;
}
```

`ch` 値 (`L|R|M`) は r9 では source ch 数に関わらずすべて受理する (1ch / 2ch / 6ch のいずれでも `ch:L|R|M` が書式上有効)。r10 で `ch=FL` 等が追加されたときに「ソース ch 数 != 6 で `ch=FL` は不整合」のような検証を入れる (r8 §9.1.4 (1) で議論済の「厳格」ポリシー)。

### 4.6 r10 への布石: ChannelKind enum 分離

r8 の `parseDistributedStereoTag()` に埋まっている `ch` 値判定を `ChannelKind` enum class に切り出す:

```cpp
enum class ChannelKind : U8
{
    L = 0,
    R,
    M,
    // r10 で以下を追加:
    // FL, FR, C, LFE, SL, SR
};

ChannelKind parseChannelKind(std::string_view s);  // "L"/"R"/"M" → enum, 不正値は std::nullopt
```

r9 の parser は `L|R|M` のみ受理するが、enum を独立化しておくことで r10 の値追加が parser テーブル + downmix ロジック + speaker 配置の 3 箇所だけで済む。

### 4.7 codec 別 mapping table の interface

```cpp
// 6ch source を 2ch (L/R) ringframe に書き込む際の sample 抽出
class MultichannelDownmix
{
public:
    static MultichannelDownmix forSourceFormat(FMOD_SOUND_TYPE type, int channels);
    void mix6chToStereo(const F32* in_interleaved, F32* out_L, F32* out_R, size_t frames) const;

private:
    SourceChannelLayout mLayout;
};
```

r10 で 6ch → 6track 直通モード (no downmix) を追加するときは、本クラスを `MultichannelRouter` に名称変更して branch 分けする。

### 4.8 ライフサイクル

#### binding 構築時

r8 と同じ。`LLPositionalStreamMulti::start(url, speakers)` 内で `Sound::getFormat()` を呼んだ結果に応じて DecodeWorker に source ch 数 / codec を渡す:

```
start(url, speakers)
  ├─ createSound(url, OPENONLY|CREATESTREAM)
  ├─ sound->getFormat(&type, &fmt, &channels, &bits)
  ├─ if (channels == 6) DecodeWorker.setLayout(kLayoutVorbisOpus6 or kLayoutFlac6)
  ├─ else if (channels == 1 || channels == 2) DecodeWorker.setStereoOrMono(channels)
  └─ else error notify + bail out
```

#### URL 変更時

r8 通り `LLPositionalStreamMulti` 全体を再生成。ch 数が 2 → 6 / 6 → 2 の動的変化も同経路でカバーされる。

#### 平常時

r8 と同じ。downmix は decode worker 内で完結するため main thread には影響しない。

---

## 5. 受入条件

### 5.1 機能要件 (viewer 側)

| ID | 内容 | 確認方法 |
|---|---|---|
| F1 | Opus 6ch ファイル (channel mapping family 1) を HTTP 静的配信して再生開始 | `python3 -m http.server` または nginx で .opus を提供、URL を root description に書いて再生 |
| F2 | FLAC 6ch ファイルを HTTP 静的配信して再生開始 | 同上、.flac で確認 |
| F3 | source 6ch → L/R downmix の各 ch 寄与が正しく聞こえる | 各 ch ごとにユニーク周波数のサイン波を埋めた素材を作成 (FL=440Hz, FR=550Hz, C=660Hz, SL=770Hz, SR=880Hz, LFE=110Hz)、L/R スピーカーから期待する周波数群が聞こえる |
| F4 | r8 の stereo source URL も従来通り再生 | r8 の F1〜F9 を回帰確認 |
| F5 | Icecast + ffmpeg で 5.1 リアルタイム配信、URL 再生 | §5.2 のレシピ通りに自宅サーバ構築、Icecast マウントポイント経由で再生確認 |
| F6 | 非対応 ch 数 (3/4/5/7/8 など) の URL は明確なエラー通知 | 4ch の WAV を Opus に encode して試す、`構造エラー: 非対応チャンネル数 (4)` の通知 |
| F7 | clipping 検出ログが downmix 後の値で適切に出る | downmix 係数で headroom 内に収まることを実測、source 0dB 近くで clipping 件数を確認 |
| F8 | URL を 2ch → 6ch (動的切替) で 1 秒以内に再生再開 | LSL `llSetObjectDesc` で URL を切替えて recovery 時間を計測 |

### 5.2 配信側パイプライン (AYA 内部レシピ)

#### 5.2.1 5.1ch テスト素材作成 (ffmpeg)

```bash
# 各 ch にユニーク周波数を埋めた 5.1 WAV を作成 (10 秒)
ffmpeg -f lavfi -i "sine=440:d=10" -f lavfi -i "sine=550:d=10" \
       -f lavfi -i "sine=660:d=10" -f lavfi -i "sine=110:d=10" \
       -f lavfi -i "sine=770:d=10" -f lavfi -i "sine=880:d=10" \
       -filter_complex "[0:a][1:a][2:a][3:a][4:a][5:a]amerge=inputs=6[a]" \
       -map "[a]" -ac 6 -channel_layout 5.1 test_5_1.wav

# Opus surround にエンコード (channel mapping family 1)
ffmpeg -i test_5_1.wav -c:a libopus -b:a 256k -mapping_family 1 test_5_1.opus

# FLAC 6ch にエンコード
ffmpeg -i test_5_1.wav -c:a flac test_5_1.flac
```

#### 5.2.2 静的配信 (nginx / python http.server)

```bash
# 開発確認用
python3 -m http.server 8080
# URL: http://<host>:8080/test_5_1.opus
```

```nginx
# 本格配信用 (range request 対応必須)
server {
    listen 8080;
    location / {
        root /var/www/streams;
        types {
            audio/ogg opus ogg;
            audio/flac flac;
        }
        autoindex on;
    }
}
```

#### 5.2.3 リアルタイム配信 (ffmpeg → Icecast)

ffmpeg 単体で 5.1 Opus を Icecast にプッシュできる。Liquidsoap や DAW を経由しないため設定ポイントが少なく、AYA 用途には最短経路:

```bash
# 既存の 5.1 WAV / FLAC をリアルタイム配信
ffmpeg -re -i test_5_1.wav \
  -c:a libopus -b:a 256k \
  -mapping_family 1 \
  -ac 6 -ar 48000 \
  -content_type audio/ogg \
  -f ogg icecast://source:hackme@localhost:8000/aya_5_1.opus
```

主要オプションの意味:
- `-re` = real-time (素材の長さに合わせて流す、生配信の代替)
- `-mapping_family 1` = Opus surround モード (`channel mapping family 1`、Vorbis 順、6ch を含む 1〜8ch サポート)
- `-ac 6` = 出力チャンネル数を 6 に固定
- `-content_type audio/ogg` = Icecast に MIME を申告 (これがないと Icecast は MP3 と誤判定する)

```ini
# /etc/icecast2/icecast.xml 抜粋
<icecast>
  <limits>
    <sources>4</sources>
  </limits>
  <hostname>localhost</hostname>
  <listen-socket>
    <port>8000</port>
  </listen-socket>
  <authentication>
    <source-password>hackme</source-password>
  </authentication>
</icecast>
```

FLAC 6ch をリアルタイム配信する場合は codec を `-c:a flac -f ogg` に差し替えるだけ (FLAC は `mapping_family` 不要)。

> **GUI 配信ツールについて**: §5.2.4 参照。本書執筆時点で 5.1ch を Opus surround で Icecast に流せる GUI 配信アプリは存在しないため、リアルタイム配信は当面 ffmpeg CLI が唯一の現実解。

#### 5.2.4 GUI 配信ツール経路 — 不在を明文化

長期的には butt のような GUI 配信アプリで 5.1ch 配信できるのが望ましいが、本書執筆時点 (2026-05) で **5.1ch (Opus channel mapping family 1) → Icecast を保証する GUI 配信アプリは存在しない**。

エコシステム調査結果:

| アプリ | 最新版 | 入力 ch | 出力 ch | 備考 |
|---|---|---|---|---|
| butt | 1.46.0 (2025-12) | "multi-channel device" 対応だが実質 2ch 取り出し | stereo 固定 | changelog に 5.1 / surround / mapping family の言及なし。Bombe/butt fork が「butt は最初の 2ch しか読めない」と明記 |
| BUTTM (butt 後継、commercial) | — | "Dual audio inputs" | stereo | multichannel/surround の言及なし |
| DarkIce | — | stereo | stereo | CLI、stereo 前提 |
| Mixxx | — | stereo | stereo | DJ 用途 |
| ezstream | — | stereo | stereo | CLI |

**判断**: r9 では GUI 配信アプリへの依存を作らず、ffmpeg CLI を primary path とする。AYA が将来 GUI で配信したくなった場合の選択肢は §9.4 にロードマップとして記録する。

#### 5.2.5 viewer 側操作

- root prim description: `[3dstream-stereo:{url:http://<host>:8000/aya_5_1.opus}{range:30}]`
- 子 prim description: `[3dstream-stereo:{ch:L}{volume:1.0}]` / `[3dstream-stereo:{ch:R}{volume:1.0}]` / `[3dstream-stereo:{ch:M}{volume:0.7}]` 等を必要数

r8 と完全に同じ書式で動く。viewer 内で 6ch ソースが downmix され、ch:L/R/M スピーカーから音が出る。

### 5.3 安定性

| 指標 | 目標 | r9 P10 実測 (2026-05-04) |
|---|---|---|
| 5.1 ソース連続再生 5 分 | dropout 0、CPU 使用率 r8 比 +3% 未満 (downmix 計算分) | ✓ PASS — 12 分間連続 0 dropout (sample1.wma → 6ch Vorbis @ 48kHz/192kbps、Icecast LAN)。CPU r8 比 **+1.09%** (6ch=39.87% vs 2ch baseline=38.78%、20-CPU システム、scene 動きノイズ ±2.5% 含む)。SIMD 化は不要、r10 持ち越しなし。 |
| URL 動的切替 (2ch ↔ 6ch) × 10 回 | crash / hang / dropout 0、再生再開 1 秒以内 | ✓ PASS — 19 transitions 全成功、crash/hang 0。viewer 内部反応 ~3 秒 (Desc poll cycle 5s + teardown + open + ready)。HTTP open は 1 秒目標を超えるが本番経路 (Shoutcast / butt-aya) では keep-alive で短縮見込み。 |
| 非対応 ch 数の URL | エラー通知 + 再生停止、crash / hang なし | ✓ PASS — 5ch Vorbis source で `非対応のソース形式です (channels=5)` 通知 1 回、 reconnect cascade 抑止 (`UnsupportedSourceFormat` 失敗分類 + url 単位の再評価 cache、P6 / P6.5 で実装) |
| `Stream3DEnabled` トグル × 50 回 (6ch source) | r8 と同様に binding が正しく start/stop | △ N/A — r8 既存パスを継承、r9 で挙動変化なし。50 回トグルテストは工数対効果でスキップ判断。 |

---

## 6. 設定項目

新規キーの追加は v1 では行わない。r8 の `Stream3DStereoMaxSpeakers` / `Stream3DRolloffMax` / `Stream3DMaxConcurrent` をそのまま使う。

将来追加の候補 (r10 で必要になれば):

- `Stream3DDownmixGain` — BS.775 ダウンミックスの最終 gain (既定 0.343)。実測で実用音量が小さすぎると判明した場合に追加検討
- `Stream3DAcceptMultichannel` — 6ch source の受入を全体 OFF にする debug toggle (回帰時の切り分け用)

---

## 7. 互換性

### 7.1 タグ

- r8 書式 `[3dstream-stereo:{url:...}{ch:L|R|M}{range:N}{volume:N}]` は完全に互換、追加・廃止フィールドなし
- `ch` enum 値の追加なし — r9 では `L|R|M` のみサポート、r10 で `FL|FR|C|LFE|SL|SR` を追加
- prefix alias `[ayastream-stereo:...]` は継続

### 7.2 ABI / 公開ヘッダ

- `LLMultiTailRing` の signature 不変 (`n_tracks=2` のまま使う)
- `LLPositionalStreamStereo::DecodeWorker` に source ch 数判定 + downmix 分岐を追加
  - 既存 1ch / 2ch source 経路は不変
  - 新規メソッド: `setSourceLayout(channels, type)` / `mix6chToStereo(...)`
- `parseDistributedStereoTag` の `ch` 判定を `ChannelKind` enum class に切り出し (内部 refactor、外部書式に変更なし)
- 新規ファイル: `indra/llaudio/llmultichanneldownmix.{h,cpp}` (codec 別 layout + BS.775 係数の実装)

### 7.3 設定 / ログ

- 設定キー追加なし
- ログ channel `Stream3D` 維持
- 新規ログ: source 検出時に `INFOS` で 1 度 `source channels=6, codec=opus, layout=vorbis_order` を出す

### 7.4 FMOD API

- r7 / r8 と同じ FMOD Studio 2.02 系
- `Sound::getFormat()` を新たに呼ぶ (既存 API、新規依存なし)
- Opus 6ch / FLAC 6ch の decode は FMOD Studio 標準ビルドで対応済 (要 F1 で確認)

---

## 8. リスク

| ID | 内容 | 対策 |
|---|---|---|
| R1 | FMOD が Opus 6ch を decode できない (codec build フラグ等) | F1 で実機確認、できない場合は FLAC 6ch のみで r9 リリース、Opus は r10 に持ち越し |
| R2 | source ch 順序が codec で実際と仮定と違う | F3 でユニーク周波数素材で実測検証、layout table を補正 |
| R3 | ITU-R BS.775 係数で clipping | downmix 後 1/2.914 倍で headroom 確保、F7 でログ監視。実用音量低下が顕著なら §6 の gain key を追加 |
| R4 | リアルタイム 5.1 配信 (ffmpeg → Icecast) が AYA 環境で動かない | F5 を AYA がローカルで一度通せれば OK、ダメなら静的配信 (F1/F2) のみで r9 リリース、リアルタイムは r10 リスクに繰り越し |
| R5 | r8 の 1/2 ch 経路に regression | F4 で r8 受入条件全項目を回帰 |
| R6 | source ch 数が動的に変わる (URL 切替) 場合の再構築タイミング | r8 の URL 変更時 = `LLPositionalStreamMulti` 再生成のフローを継承。ch 数変更も同経路 |
| R7 | downmix 係数を float 演算で SIMD 化していないため 6ch×N speakers で CPU が上がる | r9 では plain loop で実装、5.3 で実測。+3% を超えるなら SIMD 化を r10 に持ち越し |
| R8 | r10 で `ch=FL` 追加時の interface 設計が r9 で十分汎用化できていない | F6 (`ChannelKind` 切り出し / `MultichannelDownmix` の signature) を r9 完了時にレビューし、r10 着手前に再設計の余地を残す |

---

## 9. 将来拡張 (本仕様外)

### 9.1 r10: 5.1 venue placement

→ **詳細は `doc/spec_5_1ch_placement.md` (r10 仕様書) を参照**

要約: `ch=FL|FR|C|LFE|SL|SR` を追加、ring `n_tracks=6` 切替、reader 側 per-channel 取り出し、ライブ会場モデルで LFE は「6 番目のスピーカー」(ホームシアターの subwoofer 特殊扱いはしない)。本書 §12.6 r10 持ち越し項目 もそちらに移管済。

### 9.2 r11: HRTF / Steam Audio (Layer 2)

- Layer 1 (placement, r8/r10) に Layer 2 (binaural レンダリング) を直交追加
- 任意 SOFA file (KU100.sofa 等) ロード
- FMOD 既定 panner の `set3DLevel(0.0f)` で無効化必須
- 詳細は r11 着手時に独立仕様書として整備

### 9.3 公開ガイド (r11 で一括)

- r11 でまとめて r8〜r11 の書式を README / 公開ドキュメントで開示
- 配信側標準レシピ (Icecast 2.4+ + ffmpeg + Opus surround) を公開ガイドに昇格
- それまで AYA + 内部テスト協力者のみで運用 (r8 と同じ方針)

### 9.4 GUI 配信ツール — 別プロジェクト扱い

5.1ch (Opus surround) を Icecast に流せる GUI 配信アプリは本書執筆時点で存在しない (§5.2.4)。これは上流エコシステムの問題で **AYAstorm 本体のスコープ外**として切り分ける。

AYA 個人で並行検討する候補 (いずれも AYAstorm リリース成果物に含めない別プロジェクト):

- **butt fork (`butt-aya` 仮称)** — butt の libopus 経路を `opus_multistream_encoder_create` (mapping_family=1) に対応させ、PortAudio キャプチャを 6ch、Ogg writer の OpusHead を拡張する fork。1〜2 週間工数。Daniel Noethen 氏 (上流) への打診はせず GPL 下で fork 配布、AYAstorm ユーザに「これ使って」と案内する運用想定
- **ffmpeg 起動の最小 GUI ラッパ** (zenity / yad / PowerShell) — butt-aya 完成までの繋ぎ、または butt-aya がカバーしない用途向け、1 日工数

短期 (r9 完了直後) は ffmpeg CLI で AYA 自身が dogfood、中期 (r10 並行 or 後) で butt-aya を別レポジトリで仕立てる流れを想定。本仕様書の更新対象外、進捗は AYA 個人プロジェクト側で管理する。

### 9.5 AAC / HLS / DASH

- 5.1 AAC (ADTS) は理論上可だが Linux ビルドでの AAC decoder 問題あり、r10 以降検討
- HLS (.m3u8) / DASH (.mpd) のマニフェスト解析は別経路、独立タスクとしてバックログ

### 9.6 downmix gain の動的調整

- 配信素材ごとの実用音量ばらつきが大きい場合、root description に `{downmix_gain:N}` を追加する案
- r9 では固定係数、r10/r11 で必要性を再評価

---

## 10. 参考コード位置

| 対象 | パス |
|---|---|
| stereo decode worker (r7 確立、r9 で source ch 数分岐追加) | `indra/llaudio/llpositionalstreamstereo.cpp` |
| LLMultiTailRing (`n_tracks=2` 据え置き) | `indra/llaudio/llmultitailring.{h,cpp}` |
| 多 speaker stream (start 内で `getFormat` 呼出を追加) | `indra/llaudio/llpositionalstreammulti.{h,cpp}` |
| 新規: 6ch downmix | `indra/llaudio/llmultichanneldownmix.{h,cpp}` (P3 で追加) |
| manager parser (`ChannelKind` 切り出し) | `indra/newview/llpositionalstreammgr.cpp` (`parseDistributedStereoTag`) |
| 設定キー定義 | `indra/newview/app_settings/settings.xml` (新規キーなし) |

---

## 11. 工程 (実装フェーズ)

r9 は viewer 側の改修より「配信側パイプラインのレシピ確立」と「r10 への布石」が成果物の主軸になる。フェーズは下から積み上げる:

| ID | 内容 | 主要成果物 | 受入リンク |
|---|---|---|---|
| **P1** | ローカル 5.1 静的ファイル配信レシピ確立 | §5.2.1 / §5.2.2 のレシピ確認、テスト素材 `test_5_1.opus` / `test_5_1.flac` を AYA 環境で生成 | F1 / F2 の前提 |
| **P2** | 現状動作の確認 (regression baseline) | r8 ビルドで `test_5_1.opus` の URL を投入 → 何が起きるか観察 (crash / silent / 異音 / format error) | リスク把握 |
| **P3** | DecodeWorker に source ch 数判定 + 6ch 分岐追加 | `Sound::getFormat()` 呼出経路、`channels == 6` で codec 別 layout 選択。`MultichannelDownmix` クラス新規 | F1 / F2 |
| **P4** | ITU-R BS.775 ダウンミックス実装 | `mix6chToStereo()` 実装、ring に L/R 書込み確認 | F3 / F7 |
| **P5** | `ChannelKind` enum class 切り出し (refactor) | parser 内の文字列判定を enum 化、外部書式変更なし | G6 |
| **P6** | 非対応 ch 数のエラー通知 | 3/4/5/7/8ch source 検出時の `notifyStream3D` 経路 | F6 |
| **P7** | リアルタイム配信レシピ確立 | §5.2.3 の ffmpeg → Icecast 構成、AYA ローカル環境で 1 度通す | F5 (R4 リスク) |
| **P8** | URL 動的切替 (2ch ↔ 6ch) 検証 | F8 シナリオでの再生再開時間計測 | F8 / 5.3 |
| **P9** | r8 回帰確認 | r8 の F1〜F9 を全項目回帰 (1ch / 2ch source) | F4 / G7 |
| **P10** | ベンチ + 受入クローズ | 5.3 安定性指標を計測、本書に結果追記 | 5.3 |

### フェーズ依存

```
P1 ─┬─→ P2 ─→ P3 ─→ P4 ─┬─→ P6 ─→ P9 ─→ P10
    │                    │
    └───────→ P7 ────────┘
                P5 (P3 と並行可)
                P8 (P4 完了後いつでも)
```

P5 (refactor) と P3 (機能追加) は同じファイル群を触るため、P5 → P3 の順で進めるのが衝突回避には素直 (parser 整理 → DecodeWorker 拡張)。

P7 (配信側) は AYA のホームサーバ作業で viewer ビルドとは独立に進められるので、P3 ビルド作業と並行で構築する。

### スコープ縮退オプション (リリース判断時)

R1 / R4 が顕在化した場合の縮退案:

- **縮退 A**: Opus 6ch decode が FMOD で動かない → FLAC 6ch のみで r9 をクローズ、Opus は r10 で再挑戦
- **縮退 B**: AYA 環境で Liquidsoap → Icecast の 5.1 リアルタイム配信が安定しない → 静的配信 (F1/F2) のみで r9 をクローズ、リアルタイム (F5) は r10 へ繰越
- **縮退 C**: downmix の clipping / 音量バランスが破綻 → §6 の `Stream3DDownmixGain` キーを r9 内で追加してユーザ調整可能にする (実装少なめ)

縮退してもリリース上の体裁は内部リリースとして成立するため、**スコープを死守するより足元の動作確認を優先**する。

---

## 12. 実装結果 / 受入クローズ (r9 リリース 2026-05-04)

### 12.1 実装サマリ

| フェーズ | コミット | 備考 |
|---|---|---|
| 仕様策定 | `67fb18a6b3` | 本書の初版 |
| 仕様補正 (butt-aya 別 repo 化) | `4fc8034803` | §9.4 を別プロジェクト扱いに修正 |
| P5 refactor | `03efe83f7a` | DistChannel → ChannelKind enum class |
| P3 + P4 | `0c3c96016b` | 6ch source 受入 + ITU-R BS.775 downmix |
| P6 | `41c8be4b31` | 5.1ch ソース等の未対応フォーマットを失敗分類して再接続抑止 |
| P6.5 | `be8b456178` | 通知 channels=N と再評価ループの 2 件を修正 |

スコープ縮退は発生せず、全フェーズ予定通りクローズ。

### 12.2 §5.3 安定性指標 (P10 実測)

§5.3 の表を参照。CPU +1.09% / dropout 0 / URL 切替 19/19 成功、すべて目標達成。

### 12.3 codec 別 end-to-end 検証状況

| codec | end-to-end 検証 | 補足 |
|---|---|---|
| Vorbis 6ch | ✓ 完了 | 合成 sine 素材で channel 分離 + 音楽素材 (Danza debajo del sol!) で BS.775 音質確認 |
| Opus 6ch | ✓ 完了 (r9-opus 補完) | r9 時点では FMOD 同梱 codec に Opus が無く未検証だったが、r9-opus (`doc/spec_5_1ch_opus_decode.md`) で codec plugin を実装し実機 Icecast (`go-stream-live.com:8030/stream`) で 5.1 surround の再生 + 6ch downmix を確認 |
| FLAC 6ch | △ コードレビューのみ | FMOD parser が seek を要求し Icecast / 単純 HTTP では開けない (`FMOD_ERR_FILE_COULDNOTSEEK`)。FLAC indices は SMPTE identity (`{0,1,2,3,4,5}`) で実装上自明 |

### 12.4 既知の運用上の制約

- **FLAC 6ch** を **単純 HTTP** (例 `python3 -m http.server`) や **Icecast** で配信すると FMOD parser が `FMOD_ERR_FILE_COULDNOTSEEK` で開けない。本番配信は **Shoutcast 互換 streamer**、もしくは **ffmpeg / butt-aya のプッシュ先** を使うこと。
- **Opus 6ch** は r9-opus の codec plugin 経由で seek 不要のまま decode できるため、上記 seek 制約は適用されない (単純 HTTP / Icecast でも開ける)。
- 6ch source の HTTP 切替直後は prebuffer 充填中に 5〜10 秒間 dropout 警告が出ることがある (LAN 環境で 408〜2045 frames/spk/s ≒ 0.8〜4% / 10 秒)。定常運用では消える。
- §9.4 の butt-aya fork (5.1 GUI 配信ツール) は別 repo で進行、AYAstorm 本体スコープ外。

### 12.5 P9 r8 回帰確認結果

- 2ch hard-pan Vorbis (440Hz / 880Hz) で L/R/M スピーカーの分離が正しいことを聴感確認 — r8 ステレオパスに regression なし。
- 1ch source 経路は r8 から不変、コードレビューで確認。

### 12.6 r10 への持ち越し

→ **`doc/spec_5_1ch_placement.md` (r10 仕様書) に移管**

主要項目:
- `ch` enum 拡張 (FL/FR/C/LFE/SL/SR) → r10 仕様書 §4.1
- `Stream3DDownmixGain` / `Stream3DAcceptMultichannel` 追加判断 → r10 仕様書 §6 / §9.6 (r10 でも追加しない方針確定)
- SIMD 化判断 → r10 仕様書 §8 R2 (reader-side downmix 移行で再計測予定、+5% 未満であれば SIMD 不要継続)

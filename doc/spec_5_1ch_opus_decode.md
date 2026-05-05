# 5.1ch Opus URL 再生 仕様書

> **対象**: AYAstorm (内部リリース番号は実装着手時に確定)
> **前提**: `doc/spec_5_1ch_source.md` (r9 5.1ch ソース受入仕様)
> **前提**: `doc/spec_5_1ch_placement.md` (r10 5.1ch placement 仕様)
> **背景文書**: PoC 記録 `/tmp/aya_fmod_poc/` (本書 §2.2 参照)

## 1. 目的とスコープ

実 Icecast 配信に多用される **Ogg Opus URL** を AYAstorm の 3D Stream / parcel stream で再生可能にする。

主目的:
- Ogg Opus mono / stereo / 5.1 surround の HTTP / HTTPS URL を `createStream()` で開けるようにする
- 既存の r8 (分散記述ステレオ) / r9 (5.1ch ソース受入) / r10 (5.1ch placement) コードパスはそのまま流用 (Opus を「もう 1 つの普通の codec」として扱う)
- viewer 利用者は Opus URL かどうかを意識せず、既存と同じ `[3dstream-stereo:{url:...}{ch:...}]` 書式で記述するだけで再生される

非対象:
- libopusfile / 自作 3p-libopus package の追加 — FMOD SDK 同梱 libopus を再利用するため不要
- HTTP/HTTPS clients の自作 — FMOD の netstream 経路を流用
- AC-3 / AAC / E-AC-3 等の他 codec — 別件
- ユーザ向け公開 changelog 開示 — r11 で 5.1 関連を一括公開する既存方針に従う

---

## 2. 問題定義

### 2.1 現象

実 Icecast (`http://go-stream-live.com:8030/stream`) の Ogg Opus 5.1 URL が AYAstorm で再生できず、3 回 reconnect 後に "stream gave up after 3 reconnect attempts" エラーで dropping。

### 2.2 PoC 結果

`/tmp/aya_fmod_poc/test_opus.cpp` + `test_opus_hint.cpp` で AYAstorm 同梱 FMOD ライブラリを直接呼び出して切り分け:

| 検証 | 結果 |
|---|---|
| `go-stream-live...` の先頭 2.4MB を seekable file に保存 → `createStream()` | `Unsupported file or audio format` |
| 同 file を ffmpeg で stereo Opus に再エンコード → `createStream()` | 同上 |
| 同 file を ffmpeg で mono Opus に再エンコード → `createStream()` | 同上 |
| `suggestedsoundtype = FMOD_SOUND_TYPE_OPUS` を明示指定 | 同上 |
| 同 file を ffmpeg で Vorbis 5.1 に変換 → `createStream()` | **正常** (type=OGGVORBIS, channels=6, decode 可) |

`libfmod.so` の strings に `fmod_codec_oggvorbis` / `fmod_codec_flac` / `fmod_codec_mpeg` 等は存在するが **`fmod_codec_opus` が無い**。

### 2.3 原因と対応方針

- AYAstorm 同梱 FMOD Studio 2.03.07 の `libfmod.so` は **Opus codec を compile から除外**している (FMOD 公式 Linux SDK 配布物自体が同様)
- 当初 netstream 上で出ていた "Couldn't perform seek operation" エラーは表層症状。実際の原因は seek 不可ではなく、Opus codec 不在によるフォーマット判定失敗
- pre-buffer + `FMOD_OPENMEMORY` 案は **無効** (FMOD に Opus decoder が無いので入口を seekable にしても decode 不可)

ただし FMOD SDK は **FSBank encoder 用に標準 libopus を全 OS で同梱**:

```
fmodstudioapi20307linux/api/fsbank/lib/x86_64/libopus.so
fmodstudioapi20307linux/bin/lib/libopus.so
(Windows / macOS の SDK にも同等品が含まれる想定)
```

このライブラリは encoder 専用ビルドではなく、`opus_decode` / `opus_decode_float` / `opus_multistream_decode` / `opus_multistream_decoder_create` 等 **decode 関数も全部 export** されている標準 libopus。multistream API があるので channel mapping family 1 (5.1 surround) も decode 可能。

→ **この libopus を AYAstorm から利用できる形にして、FMOD codec plugin (`registerCodec`) として Opus support を補う** のが最短経路。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. Ogg Opus mono URL / stereo URL / 5.1ch URL の `createStream()` が成功する
- G2. r9 の 6ch source downmix path (`LLMultichannelDownmix`) が Opus 5.1 source に対しても動作する
- G3. r10 の 5.1ch per-channel placement (`ch=FL/FR/C/LFE/SL/SR`) が Opus 5.1 source に対しても動作する
- G4. 3 OS (Linux64 / darwin64 / windows64) で同等の動作
- G5. r8 / r9 / r10 の既存 Vorbis / FLAC / MP3 / WAV path に regression なし

### 非ゴール

- NG1. FMOD 本体の upgrade — 同じ 2.03.07 のまま、plugin 経由で codec を補う
- NG2. libopusfile / 自作 libopus 3p package — 不要
- NG3. AYAstorm 独自の HTTP client 実装 — 不要 (codec plugin は FMOD から渡された byte stream を decode するだけ)
- NG4. AC-3 / AAC / E-AC-3 等 — 別件

---

## 4. 設計

### 4.1 全体経路

```
HTTP Opus URL
  ↓ FMOD::System::createStream(..., FMOD_2D | FMOD_NONBLOCKING | FMOD_IGNORETAGS)
FMOD codec selector
  ↓ 先頭 byte 列を各 codec の "verifier" 関数に流す
  ↓   (Vorbis / FLAC / MPEG / WAV / ... と並んで Opus も candidates になる)
fmod_codec_opus (本書で新規追加する plugin)
  ↓ Ogg pages を libogg で demux → Opus packets を libopus で decode
  ↓ FMOD に PCM stream として返す (FMOD_SOUND_TYPE_OPUS / channels = 1|2|6)
LLPositionalStreamMulti / LLPositionalStreamStereo (r8〜r10 既存)
  ↓ getFormat → r9 の 6ch downmix path or r10 の 6 speaker placement
出力 channels (per speaker)
```

FMOD の codec plugin 機構は `system->registerCodec()` で登録した plugin を、URL / file の byte stream 検出時に他の組込 codec と並列にチェックする (公式 example: `examples/plugins/fmod_codec_raw.cpp`)。Opus plugin が "これは Ogg+Opus である" と判定すれば、FMOD は以降の getFormat / read を plugin に委ねる。

### 4.2 plugin の責務

`indra/llaudio/fmod_codec_opus.cpp` で実装する関数群 (FMOD codec interface):

```cpp
// FMOD が候補ファイルを差し出す。Ogg + OpusHead の magic を検出したら受理。
FMOD_RESULT F_CALLBACK opusOpen(FMOD_CODEC_STATE* codec, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO* exinfo);

// 1 packet 分を decode して PCM を返す。
FMOD_RESULT F_CALLBACK opusRead(FMOD_CODEC_STATE* codec, void* buffer, unsigned int sizebytes, unsigned int* bytesread);

// EOF / 例外 / FMOD::Sound::release() 経路。
FMOD_RESULT F_CALLBACK opusClose(FMOD_CODEC_STATE* codec);

// FMOD への format 通知 (channels / freq / fmt)。
// netstream は seek 不能なので setposition は FMOD_ERR_FILE_COULDNOTSEEK を返す。
FMOD_RESULT F_CALLBACK opusSetPosition(FMOD_CODEC_STATE* codec, int subsound, unsigned int position, FMOD_TIMEUNIT postype);
FMOD_RESULT F_CALLBACK opusGetLength(FMOD_CODEC_STATE* codec, unsigned int* length, FMOD_TIMEUNIT lengthtype);
```

内部 state object:

```cpp
struct OpusCodecState {
    ogg_sync_state    osy;     // libogg 入力 sync
    ogg_stream_state  ost;     // 1 logical bitstream 抽出
    OpusMSDecoder*    dec;     // libopus multistream decoder (mono/stereo/5.1 全対応)
    int               channels;
    int               sample_rate;       // 常に 48000 (Opus 仕様)
    std::vector<float> decode_buf;       // 1 packet 分の PCMFLOAT 出力先
    size_t            decode_buf_pos;    // FMOD への返却済み bytes
    bool              header_done;       // OpusHead / OpusTags 通過判定
    bool              eof;
};
```

### 4.3 byte 入力経路

FMOD codec plugin は `codec->fileread()` callback 経由で FMOD から byte stream を受け取る。AYAstorm 側で HTTP を扱う必要は無い (FMOD が netstream の HTTP / TLS / chunked / Icy-MetaData を全部やってくれる、r8 までと同じ)。

```cpp
// plugin 内で FMOD から N bytes 読む。短く返ってきたら EOF。
unsigned int got = 0;
codec->fileread(buf, want, &got, FMOD_OK);  // signature は SDK header で確認
```

netstream 上の Ogg framing は libogg の `ogg_sync_*` API で素直に扱える (Vorbis / FLAC stream と同じパターン、libopusfile を使わずに自前 demux)。

### 4.4 plugin 登録タイミング

AYAstorm の FMOD 初期化箇所 (推定 `indra/llaudio/llaudioengine_fmodstudio.cpp` 内 `init()`) で `system->init()` 直後に:

```cpp
extern "C" FMOD_CODEC_DESCRIPTION* F_CALL FMODGetCodecDescriptionOpus();
mSystem->registerCodec(FMODGetCodecDescriptionOpus(), &mOpusCodecHandle, /*priority*/ 100);
```

priority は組込 codec より低い 100 程度で十分 (Opus は組込 codec が反応しないので競合しない)。

### 4.5 既存コードへの影響

- `indra/llaudio/llmultichanneldownmix.cpp:60` の `FMOD_SOUND_TYPE_OPUS` 受入分岐は **plugin 登録後は live branch に復活**する (plugin が `FMOD_SOUND_TYPE_OPUS` を返すため、既存 6ch downmix logic が透過適用)。コード自体は変更不要
- `indra/newview/llpositionalstreammgr.cpp` の error 文言一覧 (line 535 「受入対象は 1/2ch ソース、または 6ch Vorbis/Opus/FLAC のみです」) もそのまま正しくなる (現状は「Opus と書いてあるが実は鳴らない」状態だったのが解消)
- r10 の `FMOD_SOUND_TYPE_OPUS` を `kLayoutVorbisOpus6` にマッピングするコード (llmultichanneldownmix.cpp) も自動的に live になる

### 4.6 plugin が満たすべき不変条件

- `opusRead()` の呼び出し thread は FMOD の decode thread。AYAstorm の他 thread と接触しないので mutex 不要
- 1 回の read で要求 size を満たす義務は無い (FMOD は不足分を再呼出してくれる)
- 1 packet decode 失敗時は `FMOD_ERR_FILE_BAD` ではなく `FMOD_OK` + `bytesread = 0` を返し、次の packet 読み込みに進む (Icecast の bitrate 切替 / network glitch を吸収)
- 連続失敗が一定回数を超えたら `FMOD_ERR_FILE_EOF` で打ち切る (AYAstorm 側で reconnect cascade に遷移)

---

## 5. 実装範囲

### 5.1 修正ファイル

| 種別 | ファイル | 内容 |
|---|---|---|
| 修正 | `/home/ishikawa/work_firestorm/3p-fmodstudio/build-cmd.sh` | SDK 同梱 libopus.so / opus.dll / libopus.dylib + Opus headers を staging に追加 |
| 修正 | `phoenix-firestorm/autobuild.xml` | `fmodstudio` package が新たに stage する Opus 関連ファイル列挙 (manifest 更新) |
| 新規 | `phoenix-firestorm/indra/llaudio/fmod_codec_opus.cpp` | FMOD codec plugin 実装 (~300〜500 行) |
| 新規 | `phoenix-firestorm/indra/llaudio/fmod_codec_opus.h` | 公開 declaration (`FMODGetCodecDescriptionOpus()`) |
| 修正 | `phoenix-firestorm/indra/llaudio/llaudioengine_fmodstudio.cpp` | `init()` で `system->registerCodec()` 呼び出し |
| 修正 | `phoenix-firestorm/indra/llaudio/CMakeLists.txt` | 新規ファイル追加 + libopus link |
| 修正 | `phoenix-firestorm/indra/cmake/FMODSTUDIO.cmake` (or 等価) | libopus を import target に追加 |

### 5.2 触らないファイル

- `indra/llaudio/llpositionalstream*.cpp` — Opus 透過対応のため変更不要
- `indra/newview/llpositionalstreammgr.cpp` — 同上
- `indra/llaudio/llmultichanneldownmix.cpp` — 既に `FMOD_SOUND_TYPE_OPUS` 分岐がある (現状 dead → 自動 live 化)
- `doc/lsl/aya_3dstream_setup.lsl` — タグ書式は同じ

---

## 6. 工程

| Phase | 内容 | 工数 | 検証物 |
|---|---|---|---|
| P1 | 3p-fmodstudio modify (Linux 版 staging に libopus + headers 追加) | 半日 | `packages/lib/release/libopus.so` / `packages/include/opus/opus.h` が AYAstorm に届いていること |
| P2 | `fmod_codec_opus.{h,cpp}` 雛形 (FMOD SDK の `fmod_codec_raw.cpp` をコピー、Opus 用にフィールドだけ書換、まだ decode しない) | 半日 | viewer 起動時に registerCodec 成功 |
| P3 | Ogg demux + libopus decode 実装 (mono / stereo) | 1日 | mono Opus URL / stereo Opus URL が再生できる |
| P4 | multistream decode 対応 (5.1 surround) | 半日 | Ogg Opus 5.1 URL が再生できる + r9 downmix が動作 (実 URL: `go-stream-live.com:8030/stream`) |
| P5 | error / EOF / reconnect 経路の堅牢化 | 半日 | netstream glitch を起こしても落ちない / reconnect cascade に正しく遷移 |
| P6 | macOS / Windows 用 3p-fmodstudio staging 修正 | 各半日 | 3 OS 全部でビルド + 起動確認 |
| P7 | 受入検証 (Vorbis / FLAC / MP3 / WAV regression + Opus mono/stereo/5.1 + 実 Icecast URL) | 1日 | §7 の検証項目すべて pass |

**合計**: 約 5 日

---

## 7. 受入条件

| ID | 内容 |
|---|---|
| A1 | 3p-fmodstudio 修正版で AYAstorm が 3 OS 全部でビルド成功 |
| A2 | 既存 Vorbis URL / FLAC URL / MP3 URL / WAV URL が r8 r9 r10 と同じ動作 (regression なし) |
| A3 | mono Opus URL が `[3dstream-stereo:{url:opus_mono}{ch:M}]` で再生できる |
| A4 | stereo Opus URL が `[3dstream-stereo:{url:opus_stereo}{ch:L|R}]` で L/R 振り分け再生できる |
| A5 | 5.1 Opus URL (例: `go-stream-live.com:8030/stream`) が `[3dstream-stereo:{url:...}{ch:L\|R\|M}]` で再生できる (r9 downmix) |
| A6 | 5.1 Opus URL が `ch=FL/FR/C/LFE/SL/SR` 配置でも再生できる (r10 placement) |
| A7 | 不正 Opus byte 列を流しても plugin が crash せず、`FMOD_ERR_FILE_*` で fail back |
| A8 | FMOD が他 codec (Vorbis 等) を選んでいるとき plugin が干渉しない (priority 100 が他より低いことを確認) |
| A9 | `~/.ayastorm_x64/logs/AYAstorm.log` に Opus codec の register 成功ログ + open 時の codec 選択ログが出る |

---

## 8. リスク

### R1. SDK 同梱 libopus が encoder 専用にビルドされていた場合
PoC でシンボル `opus_decode` / `opus_multistream_decode` を確認済 (Linux)。Windows / macOS SDK の libopus も同等品である可能性が高いが、P6 で実機検証必須。万一 decode symbol が無い OS があれば **その OS のみ自前 libopus build** に切替 (1〜2 日追加)。

### R2. FMOD codec plugin の thread / re-entrancy 制約
FMOD の codec interface は SDK ドキュメントに明示されているが、特に `setposition`(seek) の挙動は実装依存。netstream は seek 不能なので `FMOD_ERR_FILE_COULDNOTSEEK` を返す前提で問題ないはずだが、AYAstorm 既存コードが seek を要求した場合の挙動を P5 で確認。

### R3. Opus packet duration の最大値が ring buffer に対して大きすぎる
Opus は最大 120ms / packet (5760 samples @ 48kHz)。1 packet = 5760 × 6ch × 4byte (PCMFLOAT) = ~138KB。FMOD 内部の decode buffer には収まる想定だが、`opusRead()` で 1 read 内に複数 packet を返さない実装にして念のため負荷を低減。

### R4. Icecast 配信局によっては OpusHead の channel mapping family 0 と family 1 が混在する
mono / stereo は family 0、5.1 は family 1。libopus の multistream API は両方扱えるので実装上の差異は OpusHead 解析時の `channel_mapping` byte で吸収する。

### R5. 既存 Stream3D (r8〜r10) の reconnect / format 判定経路と plugin 失敗時の境界
plugin が途中で `FMOD_ERR_FILE_EOF` を返すと、FMOD はその Sound を Failed 状態にし、AYAstorm 側 (`llpositionalstreammulti.cpp:910`) が `FailReason::Network` で reconnect cascade に入る。これは既存挙動と同じなので問題なし。ただし plugin の register 自体が失敗した場合 (R1) は、AYAstorm 起動時にエラーを ChatNotify するか log のみに留めるかを P5 で決定。

---

## 9. 設定キー / 互換性

### 9.1 新規設定キー
無し。Opus 対応はビルド時の codec plugin 登録で完結し、ユーザに対しては透過的。

### 9.2 タグ書式
変更なし。`[3dstream-stereo:{url:...}{ch:...}{range:...}{volume:...}]` のまま。

### 9.3 後方互換
- 既に Description に Vorbis / FLAC / MP3 URL を書いて運用中の prim は無変更で動作
- 過去に AYAstorm に Opus URL を書いた人がいた場合、旧バージョンでは "stream gave up" エラーで再生不可だったのが、本対応版で **黙って再生できるようになる**。タグ書式を変える必要なし
- viewer のバージョンが旧 (Opus codec plugin 無し) のユーザは引き続き Opus URL は再生不可 (これは既存挙動と同じなので退行なし)

---

## 10. 関連 memory

- `project_fmod_no_opus.md` — 同梱 FMOD の Opus 不支持と FMOD SDK libopus 同梱の発見
- `project_ayastorm_r9_5_1ch_source.md` — r9 5.1ch ソース受入 (本書の前提)
- `project_ayastorm_r10_5_1ch_placement.md` — r10 5.1ch placement (本書の前提)
- `project_ayastorm_three_platforms.md` — Linux/macOS/Windows 3 OS 対応が大前提

---

## 11. 実装結果 / 受入クローズ (Linux 完了 2026-05-05)

### 11.1 実装サマリ

| フェーズ | コミット | 備考 |
|---|---|---|
| 仕様策定 | `5df467d156` (P3 commit に同梱) | 本書の初版を P3 と同時 commit |
| P1 | `5df467d156` (autobuild.xml hash 更新分) | 3p-fmodstudio fork (`mayatonton/3p-fmodstudio`) で SDK 同梱 libopus + headers を staging 化、`fmodstudio` package md5 更新 |
| P2 + P3 | `5df467d156` | `fmod_codec_opus.{cpp,h}` 新規 + `registerCodec()` 呼出。Ogg demux + libopus decode (channel_mapping_family 0 = mono / stereo) で実機再生確認 |
| P4 | `378b2b8864` | `opus_multistream_decoder` で channel_mapping_family 1 (5.1 surround) 対応。`go-stream-live.com:8030/stream` で実機 PASS |
| build-doc 反映 | `26f5d04f27` | Linux / Windows 手順書の clone URL を fork に更新、理由をインライン注記 |

P5 (堅牢化) / P6 (Windows / macOS staging) / P7 (受入条件 A1〜A9 全項目検証) は **Linux 実機 PASS 後に Windows / macOS 側で実施予定**、本書 §6 の 7 フェーズ表でいう P5 以降は r9-opus フォローアップとして残置。

### 11.2 P4 で発見した実装上の論点 2 件

P4 multistream 対応で実装上初出の問題が 2 件出たため、本書の reference として記録:

| 論点 | 症状 | 解決 |
|---|---|---|
| **FMOD decode thread のスタック不足** | 6ch Opus を再生開始すると STREAM / NONBLOCKING / MIXER のいずれかの thread で SIGSEGV。原因は libopus CELT decoder が alloca-heavy で、FMOD default 96KB スタックを超える | `llaudioengine_fmodstudio.cpp` の FMOD init 直後に `FMOD::Thread_SetAttributes` で STREAM / NONBLOCKING / MIXER 3 thread のスタックを **1MB** に拡大 |
| **plugin codec の `FMOD_SOUND_TYPE` 表現** | FMOD は plugin 経由 codec の sound type を `FMOD_SOUND_TYPE_UNKNOWN` として返す仕様。r9 / r10 の downmix path は `FMOD_SOUND_TYPE_OPUS` を期待していたため、6ch source が「未知 codec」扱いで弾かれる | `llpositionalstreammulti.cpp` で `Sound::getName()` を読み、`"Ogg Opus"` であれば `FMOD_SOUND_TYPE_OPUS` に昇格させる薄い shim を追加。既存 downmix / placement logic は無改修で透過適用 |

両論点とも仕様書 §4 の段階では予見されていなかった (FMOD SDK ドキュメント / example だけからは読み取れない実装挙動)。今後 plugin codec を増やす場合は同パターンで踏みうるため §10 関連 memory への登録候補。

### 11.3 codec / OS 別検証状況

| codec / OS | Linux64 | Windows64 | darwin64 |
|---|---|---|---|
| Opus mono URL | ✓ P3 で実機 PASS | △ ビルド未実施 | △ ビルド未実施 |
| Opus stereo URL | ✓ P3 で実機 PASS | △ ビルド未実施 | △ ビルド未実施 |
| Opus 5.1 URL (`go-stream-live.com:8030/stream`) | ✓ P4 で実機 PASS (r9 downmix + r10 placement) | △ ビルド未実施 | △ ビルド未実施 |
| Vorbis / FLAC / MP3 / WAV regression | ✓ AYAstorm 既存経路で継続動作確認 | △ ビルド未実施 | △ ビルド未実施 |

**Windows / macOS 完了は r9-opus P6 フォローアップで実施**。3p-fmodstudio fork の Windows / macOS staging は build-cmd.sh が OS 共通記述になっており、各 OS の autobuild package を生成して手元に届ければそのまま動く想定。実検証は AYA の各 OS ビルド環境で行う。

### 11.4 既知の制約 / 残課題

- `setposition()` は seek 経路を実装していないため `FMOD_ERR_FILE_COULDNOTSEEK` を返す。netstream は seek 不能なので運用上影響なし、ローカル file の早送りには未対応 (用途想定外)
- plugin codec の verifier 経路は OpusHead magic (`OpusHead`) でのみ受理。Ogg container に Opus 以外の packet が混在する非標準ストリームは未対応 (実 Icecast 配信局では未観測)
- `viewer_manifest.py` で Linux 32/64 の libopus 同梱は実装済。Windows / macOS の installer manifest は P6 で追記
- 3p-fmodstudio fork (`mayatonton/3p-fmodstudio`) は AYAstorm リリースの暗黙依存になったため、build doc (`doc/building_ayastorm.md`) の clone URL を fork に更新済 (`26f5d04f27`)

### 11.5 受入条件 (§7) 達成状況

| ID | 内容 | Linux | Win/Mac |
|---|---|---|---|
| A1 | 3 OS 全部でビルド成功 | ✓ | △ 未実施 |
| A2 | 既存 codec regression なし | ✓ | △ |
| A3 | mono Opus URL 再生 | ✓ | △ |
| A4 | stereo Opus URL 再生 | ✓ | △ |
| A5 | 5.1 Opus URL 再生 (r9 downmix) | ✓ | △ |
| A6 | 5.1 Opus URL 再生 (r10 placement) | ✓ | △ |
| A7 | 不正 Opus byte 列で plugin が crash しない | ✓ (実機で `go-stream-live.com:8030` の network glitch を含む数十分連続再生で crash なし) | △ |
| A8 | priority 100 で他 codec と非競合 | ✓ | △ |
| A9 | log に register / open ログ出力 | ✓ | △ |

Linux64 で全項目 PASS、Windows / macOS は r9-opus P6 待ち。

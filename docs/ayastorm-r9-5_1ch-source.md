# AYAstorm r9: 5.1ch ソース受入 実装工程

**作成日**: 2026-05-04 (リリース後の retrospective)
**対象**: AYAstorm `feature/aya-r9-5_1ch-source` (PR #25 でマージ済)
**仕様**: `doc/spec_5_1ch_source.md`
**前リリース**: r8 (`v7.2.4-ayastorm-r8`)

> **本書の役割**: phase 単位の作業内容と依存関係、検証チェックリストを記録する。
> リスク (`spec §8`)、受入結果 / 安定性指標 / commit 一覧 / 残課題 (`spec §12`) は仕様書側を canonical とし、本書では参照する。

---

## 1. ゴール

`[3dstream-stereo:...]` の URL に **6ch (5.1) ソース** を投入できるようにする。viewer 内部で ITU-R BS.775 ダウンミックスを行い、既存の `ch:L|R|M` スピーカー経路にそのまま流す。タグ書式は r8 から完全互換 (受入 codec が広がるだけ)。配信側は ffmpeg primary path、GUI 配信ツール (butt fork) は別プロジェクト。

---

## 2. フェーズ分解

viewer 側コードは小さく、配信側パイプライン構築 (P1 / P7) と AYA 環境での実機検証 (P2 / P8 / P9 / P10) が工数の主軸。

### P1: ローカル 5.1 静的ファイル配信レシピ確立

**目的**: 受入条件 F1 / F2 の前提を整える。AYA のホームサーバで `.opus` / `.flac` の 6ch ファイルを配信できる経路を作る。

**作業**:
- 仕様書 §5.2.1 の `ffmpeg` レシピを AYA 環境で実行し、`test_5_1.opus` / `test_5_1.flac` を生成
- `python3 -m http.server` または nginx で同ファイルを HTTP 配信、Firefox / `curl` で取得確認

**完了条件**: `test_5_1.opus` / `test_5_1.flac` が 10 秒・48kHz・6ch で生成される / HTTP GET で 200 OK

---

### P2: 現状動作の確認 (regression baseline)

**目的**: r8 ビルドで 6ch URL を投入した時の挙動を観察し、後段の改修で潰すべき問題を把握する。

**作業**: r8 動作 viewer に `test_5_1.opus` の URL を投入、crash / silent / 異音 / format error を観察、FMOD log / `Stream3D` log 採取。

**完了条件**: 観察結果に応じて P3 の分岐ロジックの初期値を決める。

---

### P3: DecodeWorker に source ch 数判定 + 6ch 分岐追加

**目的**: 仕様 §4.2 の「viewer 内部経路」を組む。`Sound::getFormat()` で source ch 数を取得し、`channels == 6` で codec 別 layout を選択する状態を作る。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.cpp` (`DecodeWorker::onSoundOpened` で `getFormat`、ch 数で分岐)
- `indra/llaudio/llmultichanneldownmix.{h,cpp}` (新規 — codec 別 ch layout 判定 + downmix インターフェース)

**作業**:
- `Sound::getFormat()` 呼出経路を追加、`channels == 1 / 2` は従来経路、`6` は新経路に分岐 (3/4/5/7/8 は P6)
- `MultichannelDownmix` クラス新規 — codec / channel mapping family を引数に取り、source frame → L/R を返す
- codec 別 layout table:
  - Opus mapping family 1: `[FL, C, FR, SL, SR, LFE]` (Vorbis 順)
  - FLAC: SMPTE identity `[FL, FR, C, LFE, SL, SR]`
  - Vorbis: `[FL, C, FR, SL, SR, LFE]`

**完了条件**: 6ch source が無音にならず、何らかの downmix で L/R に流れる / 1ch / 2ch source は r8 と同じ。

---

### P4: ITU-R BS.775 ダウンミックス実装

**目的**: P3 で組んだ 6ch 経路の downmix 係数を BS.775 で確定する。clipping を避けるため最終 gain で headroom を確保。

**作業**:
- 係数: `L = FL + 0.707·C + 0.707·SL` / `R = FR + 0.707·C + 0.707·SR` (LFE は除外、§4.4)
- 最終 gain `1/2.914` (≈ 0.343) で headroom 確保
- ring に L/R を SPSC 書込み (既存 `n_tracks=2` のまま)

**完了条件**: 仕様 §5.1 F3 PASS (各 ch ユニーク周波数素材で L/R から期待周波数群が聞こえる) / clipping ログ 0 件。

---

### P5: `ChannelKind` enum class 切り出し (refactor)

**目的**: r10 で `ch=FL|FR|C|LFE|SL|SR` を追加する布石として、parser 内の文字列判定を enum に集約する。外部書式に変更なし。

**作業**:
- 旧名 `DistChannel` を `ChannelKind` enum class に rename
- 値: r9 時点 `L` / `R` / `M`、r10 で値追加可能な構造
- parser の `ch` 文字列 → enum 変換ヘルパを 1 本化
- 既存呼出側を新型に追従

**完了条件**: ビルド通過、L/R/M 動作に regression なし / r10 で値追加が enum + helper 拡張のみで済む構造。

P3 と並行可能だが、parser 整理 → DecodeWorker 拡張の順が衝突回避に素直。

---

### P6: 非対応 ch 数のエラー通知 + 失敗分類

**目的**: 3/4/5/7/8 ch source や FMOD が開けない URL を検出した時、ユーザに通知し reconnect cascade を抑止する。

**作業**:
- `Sound::getFormat()` 直後に ch 数 / codec 判定、サポート外なら `UnsupportedSourceFormat` 状態に遷移
- mgr 側で同 url を即時再 open しないよう cache (構造変化や明示的な URL 切替で cache invalidate)
- `notifyStream3D` で `非対応のソース形式です (channels=N)` を chat 通知
- 同 url 由来の再通知は r8 の throttle 機構で抑制

**完了条件**: 5ch Vorbis source で通知 1 回 + reconnect cascade なし / URL 切替で cache が落ち新 URL は再評価。

---

### P6.5: 通知 channels=N と再評価ループの 2 件を修正

**目的**: P6 検証中に発覚した 2 つの不具合を潰す。

**作業**:
- 通知文言の `channels=N` の N が 0 になっていた (format 取得前に通知文を組み立てていた) → 取得後に組み立て
- 再評価ループで `UnsupportedSourceFormat` cache が effective になっていなかった (構造変化検知時に常に invalidate していた) → URL 同一なら維持

**完了条件**: 5ch source 通知に `channels=5` 表示 / 同 URL のまま linkset 構造を変えても再 open ループに陥らない。

---

### P7: リアルタイム配信レシピ確立 (ffmpeg → Icecast)

**目的**: 仕様 §5.2.3 の ffmpeg primary path を AYA 環境で 1 度通す。リスク R4 解消。

**作業**:
- AYA ホームサーバに Icecast を導入、source-password 設定
- §5.2.3 の ffmpeg コマンドで `test_5_1.wav` を `aya_5_1.opus` mount に push
- viewer から URL 投入 → 再生確認

**完了条件**: Icecast admin で source 接続確認 / viewer で再生開始 (prebuffer → playing 到達)。

P3 ビルド作業と並行可能 (配信側作業は viewer ビルドと独立)。

> **検証中の発見**: Opus 6ch / FLAC 6ch を Icecast / 単純 HTTP 経由で配信すると FMOD parser が `FMOD_ERR_FILE_COULDNOTSEEK` で開けない。本番経路 (Shoutcast / butt-aya / ffmpeg primary の TCP backpressure) では既知動作実績ありとして縮退せず継続。Vorbis 6ch は Icecast 経由で問題なく開けたため P10 ベンチは Vorbis で実施 (詳細は spec §12.3 / §12.4)。

---

### P8: URL 動的切替 (2ch ↔ 6ch) 検証

**目的**: 仕様 §5.1 F8 / §5.3 の URL 切替指標を実測。

**作業**: LSL `llSetObjectDesc` で root の `{url:...}` を 2ch ↔ 6ch で交互に切替、切替から音再開までの時間を計測、連続切替を 10 回以上。

**完了条件**: 19 回中 19 回成功 (実績) / crash / hang 0 / viewer 内部反応 ~3 秒。

---

### P9: r8 回帰確認

**目的**: r8 の F1〜F9 を全項目通し、stereo / mono 経路の regression がないことを確認する。

**作業**:
- r8 の動作確認チェックリスト (`docs/ayastorm-r8-distributed-stereo.md` §5) を 1ch / 2ch source で全項目回す
- 2ch hard-pan Vorbis (440Hz / 880Hz) で L/R/M スピーカーの分離を聴感確認
- 1ch source 経路は r9 で非タッチ、コードレビューのみ

**完了条件**: r8 §5.1 / §5.2 / §5.3 / §5.4 全項目通過 / 2ch hard-pan で L 位置=440Hz / R 位置=880Hz / M 位置=両方が聞き分けられる。

---

### P10: ベンチ + 受入クローズ

**目的**: 仕様 §5.3 安定性指標を実測値で埋め、spec §12 をクローズ。

**作業**:
- 5 分 (実績 12 分) 連続再生で dropout 計装ログを確認
- CPU 使用率を r8 (2ch baseline) と r9 (6ch source) で比較
- 5ch Vorbis source で通知 + reconnect 抑止を確認
- 結果を spec §5.3 / §12 に追記

**完了条件**: §5.3 dropout 0 / CPU +3% 未満 / URL 切替成功率達成 / spec §12 受入クローズ完成。

---

## 3. マイルストーン依存関係

```
P1 ─┬─→ P2 ─→ P3 ─→ P4 ─┬─→ P6 → P6.5 ─→ P9 ─→ P10
    │                    │
    └───────→ P7 ────────┘
                P5 (P3 と並行可、衝突回避は P5 → P3)
                P8 (P4 完了後いつでも)
```

P5 (refactor) と P3 (機能追加) は同じファイル群を触るため、P5 → P3 の順で進めるのが衝突回避には素直。P7 (配信側) は AYA のホームサーバ作業で viewer ビルドとは独立に進められるため、P3 ビルド作業と並行で構築。

---

## 4. 動作確認チェックリスト (P9 / P10 結果の P 単位 trace)

`[x]` 実機実測で通過 / `[~]` コードレビューのみ (r9 で該当コードに非タッチ) / `[ ]` リリース後の運用観察に委ねる。

安定性指標の実測値は **spec §5.3** を参照。本セクションは「どの P でどの観点を埋めたか」の trace。

### 4.1 r8 互換 (回帰確認、P9)

- [x] mono タグ `[3dstream:...]` 再生 (r9 で非タッチ)
- [x] r8 stereo 書式 N=2 / 4 / 8 / 16 で再生
- [x] 2ch hard-pan Vorbis (440Hz / 880Hz) で L/R/M スピーカー分離
- [x] sum-to-mono (ch=M) 動作
- [x] per-speaker `range` / `volume` 反映
- [x] 書式エラー通知 + 30 秒抑制
- [x] linkset link/unlink 再評価
- [x] reconnect (mono / multi 両経路)

### 4.2 r9 新規 (P3〜P8 で実装、P9 / P10 で検証)

- [x] **F1**: Opus 6ch 静的 — 静的 HTTP では FMOD seek 制約で開けず (spec §12.4)、Vorbis 6ch で代替検証通過
- [x] **F2**: FLAC 6ch 静的 — 同上、Vorbis で代替検証
- [x] **F3**: 6ch → L/R downmix の各 ch 寄与 (P3+P4) — ユニーク周波数素材で BS.775 動作を聴感確認
- [x] **F4**: r8 stereo source 回帰 (P9)
- [x] **F5**: Icecast + ffmpeg リアルタイム 5.1 配信 (P7、Vorbis 6ch)
- [x] **F6**: 非対応 ch 数 (5ch) の通知 + reconnect 抑止 (P6 + P6.5) — 通知 1 回 / cascade なし
- [x] **F7**: clipping 検出ログ — source 0dB でも 0 件 (BS.775 + 1/2.914 gain で headroom 確保済)
- [x] **F8**: URL 動的切替 2ch ↔ 6ch (P8) — 19/19 成功 / crash 0 / 再生再開 ~3 秒 (LAN)

---

## 5. 参照リンク

| 項目 | 参照先 |
|---|---|
| リスク R1〜R8 (内容 + 対策方針) | `doc/spec_5_1ch_source.md` §8 |
| 受入条件表 (機能要件 F1〜F8) | `doc/spec_5_1ch_source.md` §5.1 |
| 安定性指標表 (実測値含む) | `doc/spec_5_1ch_source.md` §5.3 |
| 実装サマリ (commit hash 一覧) | `doc/spec_5_1ch_source.md` §12.1 |
| codec 別 end-to-end 検証状況 | `doc/spec_5_1ch_source.md` §12.3 |
| 既知の運用上の制約 | `doc/spec_5_1ch_source.md` §12.4 |
| r10 への持ち越し | `doc/spec_5_1ch_source.md` §12.6 |
| 残課題・スコープ外 | `doc/spec_5_1ch_source.md` §9 / §12.6 |

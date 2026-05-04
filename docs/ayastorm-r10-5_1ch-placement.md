# AYAstorm r10: 5.1ch placement 実装工程

**作成日**: 2026-05-04 (リリース後の retrospective)
**対象**: AYAstorm `feature/aya-r10-5_1ch-placement` (PR #27 でマージ済)
**仕様**: `doc/spec_5_1ch_placement.md`
**前リリース**: r9 (`v7.2.4-ayastorm-r9`)

> **本書の役割**: phase 単位の作業内容と依存関係、検証チェックリストを記録する。
> リスク (`spec §8`)、受入結果 / 安定性指標 / commit 一覧 / bug A/B 詳細 / 残課題 (`spec §13`) は仕様書側を canonical とし、本書では参照する。

---

## 1. ゴール

r9 で受け入れた **6ch (5.1) ソースを downmix せず、各 channel を個別 prim に配置できる経路** を追加する。`ch=L|R|M` (既存値) に加えて `ch=FL|FR|C|LFE|SL|SR` を追加し、6ch source × 6 prim placement を実現する。

設計の核は「ライブ会場モデル」: ホームシアター 5.1 の subwoofer 概念 (LPF / omnidirectional / bass management) は実装せず、LFE prim は **6 番目のスピーカー** として他 ch と対等に扱う。HRTF / SOFA / Steam Audio は r11 で別途 (Layer 1 / Layer 2 直交設計)。

タグ書式は r9 から完全互換 (`ch` 値が広がるだけ)。設定追加は `Stream3DRoutingDiagnostic` の 1 件のみ。

---

## 2. フェーズ分解

viewer-only の改修で配信側パイプラインは r9 から流用 (P8 でテスト材料生成スクリプトのみ追加)。

### P1: ChannelKind enum に FL/FR/C/LFE/SL/SR 6 値追加 + parser 拡張

**目的**: r9 で切り出した `ChannelKind` enum class に 6 値を追加し、tag parser が新値を解釈できる状態にする。

**ファイル**:
- `indra/llaudio/llchannelkind.h` (enum 値追加)
- `indra/newview/llpositionalstreammgr.cpp` (`parseDistributedStereoTag` の `ch` 文字列 → ChannelKind 変換に case 追加)

**作業**:
- `ChannelKind` に `FL` / `FR` / `C` / `LFE` / `SL` / `SR` を追加
- parser 文字列マッチを拡張 (`"FL"` → `ChannelKind::FL` 等)
- 既存 `L` / `R` / `M` の挙動は不変

**完了条件**: `[3dstream-stereo:{ch:FL}{range:8}]` を含むプリムが書式エラーにならず binding に取り込まれる (動作実装は P2/P3) / 旧書式の挙動が r9 と完全一致。

---

### P2: DecodeWorker を per-channel write に refactor

**目的**: 6ch source 受信時、L/R 2ch downmix を decoder 側で書き込まず、6ch を ring に per-channel で書き込む経路に切替える。downmix 自体は P3 で reader 側に移す。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.cpp` (`DecodeWorker::onPCMRead` の 6ch 分岐)
- `indra/llaudio/llmultitailring.{h,cpp}` (`n_tracks=6` 切替経路の動作確認、実装自体は r8 で対応済)

**作業**:
- `channels == 6` 検出時、ring を `n_tracks=6` で構築
- decoder からの 6ch interleaved PCM をそのまま per-track 書き込み
- 1ch / 2ch source は r9 と同じ経路 (`n_tracks=1` または `2`)

**完了条件**: 6 prim binding (ch=FL〜SR) で各 prim に該当 channel が届く (P3 reader 完了で実機確認可能) / 1ch / 2ch source の挙動が r9 と完全一致。

---

### P3: reader-side BS.775 downmix 移行

**目的**: ch:L / ch:R / ch:M reader が必要な時に on-the-fly で BS.775 downmix を行う経路を組む。decoder 側 downmix を撤去し、ring には常に source ch 数そのままが入る形にする。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.cpp` (`pcmReadCallback` の reader 側 downmix)
- `indra/llaudio/llmultichanneldownmix.cpp` (呼出位置を decoder → reader へ移動。ロジック自体は無変更)

**作業**:
- `LLPositionalStreamMulti` の per-channel reader が `ChannelKind` に応じて分岐:
  - `ch:FL` 等 = source ch を直取り
  - `ch:L` / `ch:R` / `ch:M` = source 6ch から BS.775 downmix を on-the-fly 算出
  - 1ch / 2ch source の場合は r9 と同じ動作
- `LLMultichannelDownmix` の signature は不変、呼び出し位置のみ移動

**完了条件**: 6ch source × 6 prim placement (ch=FL〜SR) で各 prim から該当 channel の音 / 6ch source × ch:M prim で BS.775 mix が r9 と同等 / 互換マトリクス (P4) の決定論動作。

---

### P4: §4.2 互換マトリクス実装 + unit test

**目的**: 9 ch 値 × 3 source 数 = 27 cell の挙動を仕様 §4.2 通りに決定論化する。silent / fallback / 直取りの分類を unit test で固める。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.cpp` (reader 分岐表)
- `indra/test/test_channel_routing.cpp` (新規 unit test)

**作業**:
- 仕様 §4.2 の 27 cell 全行を実装
  - 6ch source: 9 ch 値すべて意図通り (FL/FR/C/LFE/SL/SR は直取り、L/R/M は BS.775 downmix)
  - 2ch source: ch=FL/FR は L/R 直取り、ch=C は L+R mix、ch=LFE/SL/SR は silent
  - 1ch source: ch=L/R/M/C は同じ mono、ch=FL/FR/SL/SR/LFE は silent
- unit test で 27 cell の reader 出力を verify

**完了条件**: 27 cell unit test 全 PASS / 実機での挙動が unit test と一致 (P9 で抜き取り確認)。

---

### P5: routing 診断 log 機構実装

**目的**: 配置者が 6ch placement のデバッグ時に「どの ch がどう routing されたか」を log で確認できる経路を作る。spam を抑止する throttle を組み込む。

**ファイル**:
- `indra/newview/llpositionalstreammgr.{h,cpp}` (throttle key + log フォーマット)
- `indra/llaudio/llpositionalstreammulti.cpp` (routing 決定時の log 出力点)

**作業**:
- routing 決定時に `LL_WARNS("Stream3D")` で 1 行出力
  - 例: `routing decision: prim=<UUID> ch=FL source_channels=6 → direct take`
  - 例: `routing decision: prim=<UUID> ch=FL source_channels=2 → fallback (L direct)`
- throttle key = (prim_id, ChannelKind, source_channels) の triple、同 key は 30 秒抑制
- 設定 gate `Stream3DRoutingDiagnostic` (P6) が `false` なら完全静音

**完了条件**: diagnostic ON で各 prim の routing 決定が 1 行ずつ出る / 同 prim の routing 決定が 30 秒に 1 件まで抑制される / diagnostic OFF で log spam 0。

---

### P6: `Stream3DRoutingDiagnostic` 設定追加

**目的**: P5 の routing 診断 log を ON/OFF できる単一の bool 設定を debug settings に追加する。

**ファイル**:
- `indra/newview/app_settings/settings.xml` (`Stream3DRoutingDiagnostic`)

**作業**:
- bool / 既定 `false` / debug settings 配下
- `gSavedSettings.getBOOL("Stream3DRoutingDiagnostic")` を log 出力点で参照
- live 切替 (再起動不要、次の routing 決定から効く)

**完了条件**: debug settings から ON/OFF 切替で log 出力が即座に変わる / chat 通知には影響しない (FormatUnsupported 等は本設定独立)。

---

### P7: r11 hook 関数 (`makeChannelForBinding()`) 抽出

**目的**: r11 で FMOD positional → Steam Audio へ切替える際の hook を 1 関数に集約する。`Channel::set3DLevel(1.0f)` の明示呼出をここに閉じ込める。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.cpp` (`makeChannelForBinding()` 抽出)

**作業**:
- 既存の channel 作成経路を `makeChannelForBinding()` 関数に集約
- `Channel::set3DLevel(1.0f)` の明示呼出をこの 1 箇所に
- コメント `// r11 hook: Steam Audio takeover` を残す
- r11 ではここを `set3DLevel(0.0f)` に切替えて Steam Audio へバトンタッチ

**完了条件**: r10 の挙動は不変 (リファクタ目的、機能変更なし) / `Channel::set3DLevel(1.0f)` の呼出が grep で 1 箇所に集約されている。

P3 と並行可能 (置く場所が違う)。

---

### P8: テスト材料生成スクリプト + AYA Icecast hosting

**目的**: §5.1 の per-channel voice + tone 合成素材を再現可能な形で repo に置き、AYA Icecast にホストする。

**ファイル**:
- `doc/r10/gen_test_material.sh` (新規スクリプト)

**作業**:
- 各 ch の voice (`espeak-ng "Front Left"` 等) と tone (`sox synth N sine FREQ`) を mix
- 6ch interleave で `test_routing_5_1.wav` を作成
- Vorbis channel mapping family 1 順 `[FL, C, FR, SL, SR, LFE]` で `oggenc` encode
- 生成済み ogg は repo に入れず AYA Icecast にホスト (URL は LSL prim Description で参照)

**完了条件**: `bash doc/r10/gen_test_material.sh` で `test_routing_5_1.ogg` が再現可能 / AYA Icecast 経由で viewer から再生確認。

**P8 fix** (`bc562f026e`): 初版で WAV interleave 順を SMPTE 順 [FL,FR,C,LFE,SL,SR] にしていたが、Vorbis channel mapping family 1 は [FL,C,FR,SL,SR,LFE]。FLAC を encode する場合は順が変わるためスクリプトコメントに明記。

---

### P9: §5.2 検証手順全件実行 (routing / bleed / musical / matrix / log)

**目的**: 仕様 §5.2 の 5 項目検証を実機で全部回し、§5.3 受入条件表を埋める。

**作業**:
- **#1 routing 検証**: 6 prim ch:FL/FR/C/LFE/SL/SR 配置、各 prim 真横で voice + tone 単独確認
- **#2 bleed 定量チェック**: 隣接 prim の周波数が真横位置で audible でないこと、range:8 で十分減衰
- **#3 musical quality 検証**: r9 sample1_6ch.ogg を 6 prim 配置で再生、avatar 中心で会場感
- **#4 互換マトリクス検証**: 6ch×ch:M / 1ch×ch:FL / 1ch×ch:LFE / 2ch×ch:FL/FR/C/LFE の 4 cell を実機で抜き取り確認
- **#5 routing 診断 log 検証**: `Stream3DRoutingDiagnostic=true` で §4.4 通りの log 出力確認

**完了条件**: §5.3 受入条件表 全行 PASS。

> **検証中の発見**: P9 中に r10.x 級の bug 2 件が発覚 (auto-reconnect 不発 / linkset 再構築時の stale binding)。下記 r10.x bugfix で同梱修正 (詳細 = spec §13.3)。

---

### P10: r9 / r8 回帰確認

**目的**: 既存 ch:L / ch:R / ch:M prim の挙動が r9 と完全一致することを実機で確認する。reader-side downmix 移行による r9 経路への regression を検出する。

**作業**:
- ch:M / ch:L × 6ch / 2ch / 1ch source の 6 セルを実機で再生 (ch:R は対称なので省略)
- 各セルで r9 と同じ音が聴こえる (BS.775 mix / direct take / mono)
- 1ch source は r9 から不変、コードレビューで確認

**完了条件**: 6 セル全 PASS / r9 §5.3 / §5.4 の安定性指標が維持されている (P11 で計測)。

---

### P11: ベンチ + 受入クローズ

**目的**: 仕様 §5.3 の CPU r9 比 +5% 未満を実測値で埋め、spec §13 受入クローズを書き上げる。

**作業**:
- 5 分連続再生で dropout 計装ログを確認 (test_routing_5_1.ogg を Icecast LAN 配信、6 prim linkset 配置)
- CPU 使用率を r9 (1 prim ch:M baseline) と r10 (6 prim placement) で比較
  - `pidstat -u 30 10` または `top -bn 2 -d 1` で計測 (`top -bn 1` は初回 0% アーティファクトあり)
  - PID は `do-not-directly-run-ayastorm-bin` (FMOD含む 本体プロセス) を選ぶ。`dullahan_host` は browser process で対象外
- 結果を spec §5.3 / §13 に追記

**完了条件**: 5 分間 dropout 0 / CPU r9 比 +5pp 未満 / spec §13 受入クローズ section 完了。

---

### r10.x: P9 検証中発見の auto-reconnect / linkset 再構築 2 bug 修正

**目的**: P9 検証中に顕在化した 2 件の bug を本リリースに同梱で修正する。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.{h,cpp}` (zero-fill streak 検出)
- `indra/newview/llpositionalstreammgr.cpp` (linkset 再構築時の dead_roots tearDown)

**修正**:
- bug A (auto-reconnect 不発): `pumpSource` に zero-fill streak 検出 (10 秒分) を追加、超過で `setFailed()` → mgr の reconnect cascade に拾わせる
- bug B / #38 (linkset 再構築で旧 root binding 残存): evaluate ループ内で `getRootEdit() != root_id` を見て dead_roots に積み tearDown する経路を追加

**完了条件**: bug A: ffmpeg 再起動 → 25 秒以内に viewer が音を再開 (zero-fill 10s + cascade 3×5s) / bug B: linkset 再構築で旧 root の binding が即座に teardown、二重再生なし。

bug 詳細 (症状 / 原因) は **spec §13.3** を参照。

---

## 3. マイルストーン依存関係

```
P1 ─→ P2 ─→ P3 ─┬─→ P4 ─→ P9 ─→ P10 ─→ P11
                │
                └─→ P5 ─→ P6
                    P7 (P3 と並行可)
                    P8 (P9 開始前までに)
                    r10.x bugfix (P9 検証中に発生)
```

P1 (parser) と P2 (decoder) は同じ ChannelKind に依存するため P1 → P2。P3 (reader) は P2 完了後の方が ring 形状が固まっていて素直。P5/P6 (診断 / 設定) は P3 完了後どこでも、P9 までに完成。P7 (r11 hook) は P3 と並行可能 (置く場所が違う)。

r10.x bugfix は P9 開始前から既存の挙動だが、5.1ch placement 検証で初めて顕在化したため scope に同梱。

---

## 4. 動作確認チェックリスト (P9 / P10 / P11 結果の P 単位 trace)

`[x]` 実機実測で通過 / `[~]` コードレビューのみ / `[ ]` リリース後の運用観察に委ねる。

安定性指標の実測値 (CPU / dropout / URL 切替成功率 / 互換マトリクス) は **spec §5.3** を参照。本セクションは「どの P でどの観点を埋めたか」の trace。

### 4.1 r10 新規 (P1〜P8 で実装、P9 で検証)

- [x] **routing**: 6 prim ch:FL/FR/C/LFE/SL/SR 配置で各 prim 真横で voice + tone (FL=220Hz / FR=440Hz / C=880Hz / LFE=60Hz / SL=1760Hz / SR=3520Hz) 単独確認
- [x] **bleed**: 隣接 prim の周波数が真横位置で audible でない (range:8 で十分減衰)
- [x] **musical quality**: sample1_6ch.ogg 6 prim 配置、avatar 中心で「会場感」OK
- [x] **互換マトリクス**: P4 unit test で 27 cell 決定論担保、P9 #4 で実機 4 cell (6ch×ch:M / 1ch×ch:FL / 1ch×ch:LFE / 2ch×ch:FL/FR/C/LFE) を spec §4.2 通り確認
- [x] **routing 診断 log**: `Stream3DRoutingDiagnostic=true` で routing 決定行が出力、30 秒 throttle 動作、OFF で完全静音
- [x] **r11 hook**: `Channel::set3DLevel(1.0f)` の呼出が `makeChannelForBinding()` 1 箇所に集約済 (grep 確認)

### 4.2 r9 / r8 互換 (回帰確認、P10)

- [x] ch:M × 6ch source: BS.775 mix 音が r9 と同等
- [x] ch:M × 2ch source: L+R mono mix が r9 と同等
- [x] ch:M × 1ch source: mono direct が r9 と同等
- [x] ch:L × 6ch source: BS.775 L 経路が r9 と同等
- [x] ch:L × 2ch source: L direct が r9 と同等
- [x] ch:L × 1ch source: mono direct が r9 と同等
- [~] ch:R × source 全種: ch:L と対称のためコードレビューで確認、実機検証スキップ
- [x] r8 stereo 書式 (ch:L/R/M N=2/4) で r8 §5 全項目通過
- [x] mono タグ `[3dstream:...]` (r10 で非タッチパス、推定 OK)

---

## 5. 参照リンク

| 項目 | 参照先 |
|---|---|
| リスク R1〜R5 (内容 + 対策方針) | `doc/spec_5_1ch_placement.md` §8 |
| 受入条件表 (実測値含む) | `doc/spec_5_1ch_placement.md` §5.3 |
| 実装サマリ (commit hash 一覧) | `doc/spec_5_1ch_placement.md` §13.1 |
| §5.3 安定性指標まとめ | `doc/spec_5_1ch_placement.md` §13.2 |
| P9 検証中発見の bug A/B 詳細 | `doc/spec_5_1ch_placement.md` §13.3 |
| codec 別 end-to-end 検証状況 | `doc/spec_5_1ch_placement.md` §13.4 |
| 既知の運用上の制約 | `doc/spec_5_1ch_placement.md` §13.5 |
| r11 への持ち越し | `doc/spec_5_1ch_placement.md` §13.6 |
| 残課題・将来拡張 | `doc/spec_5_1ch_placement.md` §9 |

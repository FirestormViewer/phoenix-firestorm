# AYAstorm r8: 3D Stream Stereo 分散記述拡張 実装工程

**作成日**: 2026-05-03
**対象**: AYAstorm `feature/aya-r8-distributed-stereo` (想定ブランチ名)
**仕様**: `doc/spec_distributed_stereo.md`
**前リリース**: r7 (`v7.2.4-ayastorm-r7`)

---

## 1. ゴール

`[3dstream-stereo:...]` タグを「分散記述方式」に拡張し、1 ストリーム → N スピーカー (上限 16) の同期再生を実現する。各スピーカーに L / R / M (sum-to-mono) のチャンネル割当と、個別の `range` / `volume` を設定できるようにする。

旧書式 `{l:N}{r:N}{min:N}{max:N}` は r8 で削除済 (公開前の内部書式のため互換層は持たない)。`[ayastream-stereo:...]` の prefix alias は r5 命名整理から継続。

---

## 2. フェーズ分解

各フェーズは 1〜2 commit 想定。F3 完了時点で「機能としては動く」状態に到達し、F4/F5/F6 は通知 UX・設定 UI・実機ベンチで仕様 §5 受入条件を埋める段階。

### F1: パーサ拡張 + データ構造

**目的**: 新書式 `{ch:L|R|M}` `{volume:N}` `{range:N}` を認識し、書式エラーを検出する状態を作る。マネージャの binding 構造はまだ作らない (F2 で扱う)。

**ファイル**:
- `indra/newview/llpositionalstreammgr.h` (新データ構造の追加)
- `indra/newview/llpositionalstreammgr.cpp` (parser 関数の追加、書式エラー検出)

**作業**:
- `StereoSourceTag` / `StereoSpeakerTag` の構造体を追加
- `parseDistributedStereoTag(description)` を追加 (source / speaker フィールドを 1 パスで抽出)
- `range` キーを認識 (`Stream3DRolloffMax` 既定値にフォールバック)
- `ch` の値検証: `L` / `R` / `M` 以外は書式エラー
- `volume` の値検証: 0.0〜1.0 範囲外は書式エラー
- 書式エラー検出時のエラー種別 enum (`DistParseError::BadCh` / `BadVolume` / `BadRange` / `EmptyUrl`) を定義
- ユニットテスト: 新書式の各書式エラーケース、source/speaker/両方併記の各パターン

**完了条件**:
- 各種書式入力に対して `parseDistributedStereoTag` が期待通りの結果を返す
- 既存 `parseTag` (mono) の挙動に regression なし
- 書式エラー検出ロジックが `DistParseError` を正しく分類できる
- まだ binding 構造には繋がない (この段階で実機で再生確認はしない)

**想定 commit**: 1 本 (`r8: distributed-stereo — add parseDistributedStereoTag (F1)`)

---

### F2: linkset 単位の binding 構造 + 子プリム監視

**目的**: 新書式の構造的な linkset 集約を組む。子プリム description の取得経路を確立し、変更検知で再評価が走る状態にする。実 stream 接続はまだ繋がない (F3 で扱う)。

**ファイル**:
- `indra/newview/llpositionalstreammgr.h` (`mDistributedBindings` の追加、binding 構造体)
- `indra/newview/llpositionalstreammgr.cpp` (linkset 走査、子プリム properties 要求、再評価ループ)

**作業**:
- `DistributedStereoBinding` 構造体追加 (root_id / source / speakers / 状態)
- `evaluateLinkset(root_id)` を実装:
  - root description から source 抽出
  - 同 linkset 子プリムを走査して speakers 収集
  - 構造エラー (url なし / speakers 0 / N 超過) を判定
- 音源宣言を持つ root 検出時、その linkset 内全プリムの properties を `requestObjectPropertiesFamily` で一括要求
- 既存 `onObjectPropertiesReceived` を拡張: 子プリムから来た場合は所属 root をたどって `evaluateLinkset` 再呼び出し
- link / unlink イベント (`LLViewerObject::setParent` 周辺、もしくは selection mgr のイベント経由) で再評価
- `LL_INFOS("Stream3D")` でデバッグ出力 (binding 構築・破棄・再評価のサイクル)

**完了条件**:
- 親 + 子複数の linkset を rez した時、binding が正しく構築される (デバッグログで確認)
- 子プリムの description を `llSetObjectDesc` で後から書いた時、数秒以内に再評価が走る
- link / unlink で speakers 数が変動した時、再評価が走る
- 構造エラー時の通知文言が表示される (この段階では F4 throttle はまだ入れない)
- まだ実 stream は繋がないので音は鳴らない

**想定 commit**: 1〜2 本 (`r8: distributed-stereo — linkset aggregation and child property tracking (F2)`)

---

### F3: LLPositionalStreamMulti 新設 (1 source → N channel + sum-to-mono)

**目的**: 仕様 §4.5〜4.6 の同期再生実装を組み、F2 で出来た binding を実 stream として鳴らす。r7 で確立した decode thread モデル / shutdown 不変条件を踏襲する。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.{h,cpp}` (新規クラス)
- `indra/newview/llpositionalstreammgr.cpp` (`evaluateLinkset` から `LLPositionalStreamMulti::start` を呼ぶ)
- 必要なら `indra/llaudio/llaudioengine_fmodstudio.cpp` (channel 数枠の調整、要確認)

**作業**:
- `LLPositionalStreamMulti` を `LLPositionalStreamStereo` の構造を参考に N 化:
  - 1 つの `FMOD::Sound` (デコーダ)
  - 1 つの decode thread (r7 通り)
  - 1 つの ring buffer (multi-tail SPSC または mixer 前段スナップショット段、F3 内で詰める)
  - N 個の `FMOD::Channel`、各 ch に対する `pcmReadCallback` で L / R / M を分岐
- `setDelay` で全 N channel に同一 dspclock 起点を渡す (sample 単位同期)
- `setPositions(speakers)` で各 channel の `set3DAttributes` / `set3DMinMaxDistance` / `setVolume` を反映
- `stop()` / デストラクタで decode thread join → FMOD release の順序厳守 (r7 §4.4 不変条件)
- manager 側で `start` / `update` / `stop` を `evaluateLinkset` から駆動

**完了条件**:
- 親 + 子 4 個 (L/R/L/R) で実 stream を再生し、4 点から同期して音が出る
- 4 点から発した音にステレオマイクで近づく/離れる試験で正しい 3D 減衰
- `ch=M` のスピーカーで sum-to-mono が動作 (テスト素材 L=440Hz / R=880Hz で M ch が両方含む)
- `setDelay` 起動同期が崩れていない (録音解析でサンプル単位)
- `Stream3DEnabled` トグル × 20 回で crash なし
- TSan / Helgrind で data race 検出なし

**想定 commit**: 2〜3 本 (`r8: distributed-stereo — introduce LLPositionalStreamMulti (F3)` + `r8: distributed-stereo — wire multi-stream into manager (F3)` + 必要なら `r8: distributed-stereo — sum-to-mono mixing (F3)`)

---

### F4: throttle 通知 + 構造エラー集約

**目的**: 仕様 §4.9 のエラー通知ポリシーを実装する。書式例つきメッセージ、30 秒抑制、構造エラーの linkset 単位集約。

**ファイル**:
- `indra/newview/llpositionalstreammgr.{h,cpp}` (throttle map、通知ヘルパ)

**作業**:
- `ErrorThrottleKey { LLUUID prim_id; ErrorKind kind; }` を定義
- `mLastNotifyTime: std::map<ErrorThrottleKey, F64>` を追加
- `notifyIfNotThrottled(key, msg)` ヘルパを実装 (30 秒以内なら LL_INFOS のみ、初回は notifyStream3D)
- 書式エラー / 構造エラーの通知箇所を全て `notifyIfNotThrottled` 経由に書き換え
- 構造エラー (linkset 単位) は root_id を key に使い、speakers 5 個全部不正でも 1 件にまとめる
- 通知文言テンプレに書式例を含める (仕様 §4.9 のサンプルに準拠)

**完了条件**:
- 同じ不正タグを連打しても通知は 30 秒に 1 件
- 5 子プリム全部に `ch:X` (不正値) が書かれていても通知は 1 件 (linkset 集約)
- 通知文言に「例: [3dstream-stereo:{ch:L}{range:30}]」が含まれる
- 抑制中の重複は LL_INFOS にだけ出る (chat 通知には出ない)

**想定 commit**: 1 本 (`r8: distributed-stereo — throttled error notifications with examples (F4)`)

---

### F5: 設定追加 + UI (もし必要なら)

**目的**: `Stream3DStereoMaxSpeakers` 設定を settings.xml に追加。UI 露出は v1 では debug settings のみ。

**ファイル**:
- `indra/newview/app_settings/settings.xml` (`Stream3DStereoMaxSpeakers` 追加)
- `indra/newview/llviewercontrol.cpp` (上限値変更時の listener、必要なら)

**作業**:
- `Stream3DStereoMaxSpeakers` を `S32` / 既定 16 / 範囲 1〜64 で settings.xml に追加
- manager 側で `gSavedSettings.getS32("Stream3DStereoMaxSpeakers")` を参照
- 設定変更時に `evaluateLinkset` を全 binding について再走させる listener を追加
- v1 では UI への露出はしない (debug settings から変更可、リリース後の運用で必要なら r9 以降で UI 化)

**完了条件**:
- debug settings で Stream3DStereoMaxSpeakers を 16 → 4 に変更すると、4 を超える speakers を持つ既存 binding が再評価で 4 まで減る
- 範囲外値 (0 や 100) を入れても crash しない (clamp する)

**想定 commit**: 1 本 (`r8: distributed-stereo — Stream3DStereoMaxSpeakers setting (F5)`)

---

### F6: 実機ベンチと仕様クロージング

**目的**: 仕様 §5 受入条件を実測値で埋める。ベンチ結果を本ドキュメントに追記。

**作業**:
- N=2 / 4 / 8 / 16 の各構成で再生試験
  - 新書式 各 N
  - sum-to-mono 試験 (L=440Hz / R=880Hz 素材)
  - per-speaker volume / range 上書き試験
- 同期性試験: in-world recorder で録音 → 波形解析でサンプルズレ確認
- 安定性試験: `Stream3DEnabled` トグル × 50 / region 切替 × 10 / Quit
- 上限超過試験 (17 個書いて 16 + 警告)
- 子プリム後付け試験 (LSL `llSetObjectDesc` 後付け)
- link / unlink 試験
- CPU 使用率計測 (16 spk × 4 並走 = 64 channel)
- 結果を §5 受入条件表に記入し、未達があれば該当フェーズに戻る
- spec のリスク表 (R1〜R9) を実測ベースで「解決 / 監視継続」に分類
- 動作確認チェックリスト (本書 §5) を埋める

**完了条件**:
- 仕様 §5.1 機能要件 F1〜F10 が全て通過
- 仕様 §5.2 同期性 3 指標が目標達成
- 仕様 §5.3 安定性 4 項目が通過
- 残課題があれば本ドキュメント §6 に記載

**想定 commit**: 1 本 (`r8: distributed-stereo — close acceptance with bench results (F6)`)

---

### F7: multi 経路 reconnect (回帰確認で発見した不足分)

**目的**: §5.1 ⑥ の実機検証で「mono 経路は reconnect するが multi 経路は無音のまま」が判明したため、mono の retry ループを multi にも移植する。

**ファイル**:
- `indra/llaudio/llpositionalstreammulti.h` (fail streak フィールド + 閾値定数)
- `indra/llaudio/llpositionalstreammulti.cpp` (`pumpSource` で連続 readData エラーを数えて `State::Failed` 遷移、ログ throttle 1/sec、Failed 中は readData をスキップ)
- `indra/newview/llpositionalstreammgr.h` (`DistributedStereoBinding` に `reconnect_attempts` / `next_retry_time` / `notified_played` を追加)
- `indra/newview/llpositionalstreammgr.cpp` (分散バインディング更新ループに retry 経路、`evaluateLinkset` 側で構造変更時のカウンタリセット)

**動作**:
- 連続 200 回 (≈ 1 秒分) の readData エラー → `State::Failed`
- Manager が 5 秒間隔で `start()` を再呼び出し
- `Stream3DReconnectAttempts` (mono と共有、デフォルト 3) 回数を超えると binding を drop し chat に `gave up` 通知
- 再接続成功で reconnect counter をリセット、ログに `reconnect succeeded`
- ログスパムは 1/sec まで throttle

**完了条件**:
- 配信元停止 → 1 秒以内に Failed 遷移、ログ 1/sec
- 5 秒間隔で reconnect attempt が試行される
- 配信再開が retry ウィンドウ内に間に合えば自動復帰、間に合わなくても evaluateLinkset 経由で再構築

**想定 commit**: 1 本 (`r8: distributed-stereo — multi-path reconnect (F7)`)

---

## 3. マイルストーン依存関係

```
F1 → F2 → F3 → F4 → F5 → F6
              ╲
               └→ F4 / F5 は F3 完了前から並行着手可 (F4 は manager 改修主体、F3 を待たない)
```

主流は完全な直列。F4 (throttle) と F5 (設定) は F3 (実 stream) を待たずに着手可能だが、受入条件のクロージングは F3 後でないと意味がないため、ベンチは F6 でまとめて行う。

---

## 4. リスクトラッキング (spec §8 と対応)

| ID | 状態 | 着手フェーズ | メモ |
|---|---|---|---|
| R1. N spk 同期失敗 | (未着手) | F3 | `setDelay` 同一 dspclock 起点で実装、F6 で録音解析確認 |
| R2. ring buffer N reader 対応 | (未着手) | F3 | F3 設計時点で multi-tail SPSC vs mixer 前段スナップショットを比較選定 |
| R3. 子 desc 取得不整合 | (未着手) | F2 | NG4 でゆるい SLA を許容。F2 で再評価ループを実装 |
| R4. spk 数超過時挙動 | (未着手) | F2 + F4 | 「先頭 N 個採用」を F2 で実装、通知を F4 で整備 |
| R5. 書式エラー誤判定 | (未着手) | F1 | F1 ユニットテストで `DistParseError` 各分類の境界ケース固める |
| R6. 通知重複抑制破綻 | (未着手) | F4 | ErrorThrottleKey は (prim_id, kind) で固定、F4 でテスト |
| R7. sum-to-mono clipping | (未着手) | F3 | 0.5f 倍で正規化、源音 0dB 未達なら問題なし。F3 で検出ログ |
| R8. 16 spk channel 枯渇 | (未着手) | F6 | 実測。FMOD 既定 channel 数 (512) 内に収まる見込み |
| R9. r7 shutdown 不変条件 | (未着手) | F3 | LLPositionalStreamMulti を Stereo の構造踏襲で実装、stop()/dtor 順序維持 |

---

## 5. 動作確認チェックリスト (F6 結果)

`[x]` 実機実測で通過 / `[~]` 暫定通過 (回帰なし推定 — r8 で該当コードに非タッチ) / `[ ]` リリース後の運用観察に委ねる。

### 5.1 r7 互換 (回帰確認)

- [x] mono タグ `[3dstream:...]` プリム rez → 自動接続して 3D 定位 (mono 経路は r8 で非タッチ — NG5)
- [x] `[ayastream-stereo:...]` prefix alias 再生 (r5 命名整理からの継続互換)
- [x] `Stream3DEnabled = false` で全停止
- [x] `Stream3DDescriptionScan = false` で binding 落ち
- [x] `Stream3DVolumeMaster` の即時反映
- [x] reconnect 経路の動作 (F7: multi 側にも reconnect ループを移植)

### 5.2 r8 新規 (分散記述拡張の効果)

- [ ] N=2 新書式 `{ch:L}` `{ch:R}` で再生
- [ ] N=4 新書式 (L/R/L/R) で再生、左右移動で減衰確認
- [ ] N=8 / N=16 で再生
- [ ] `ch=M` sum-to-mono 動作確認 (L=440Hz / R=880Hz 素材で周波数解析)
- [ ] per-speaker `range` 上書き動作
- [ ] per-speaker `volume` 動作 (1.0 / 0.8 / 0.5 / 0.2 で聴感確認)
- [ ] 17 個書いて 16 採用 + 警告通知
- [ ] 子プリム description 後付けで反映 (数秒以内)
- [ ] link / unlink で binding 再評価
- [ ] 書式エラー時の通知に例が含まれる
- [ ] 同種エラー 30 秒抑制動作

### 5.3 同期性 (spec §5.2)

| 指標 | 目標 | 実測 | 判定 |
|---|---|---|---|
| 全スピーカー間の位相ズレ | ≤ 1 sample | (F6 で計測) | |
| FMOD mixer の dropout (5 分連続) | 0 件 | (F6 で計測) | |
| 16 spk 時 CPU 使用率 | r7 比 +5% 未満 | (F6 で計測) | |

### 5.4 安定性 (spec §5.3)

- [ ] `Stream3DEnabled` トグル × 50 回で crash / hang なし
- [ ] リージョン切替 × 10 回で再評価が正しく走る
- [ ] 配信再生中の Quit で crash なし (r7 同等)

### 5.5 実機テスト用サンプル構成

`<URL>` は実 HTTP MP3 / Ogg ストリーム URL に置換。各 prim は `Description` 欄に貼り付け、root と children を link する。tag は r5 alias `[ayastream-stereo:...]` も同等に動作。

#### F1 (N=2 最小): root が source + ch=M、子 1 個が ch=R

| prim | description |
|---|---|
| root | `[3dstream-stereo:{url:<URL>}{range:30}{ch:M}{volume:1.0}]` |
| child 1 | `[3dstream-stereo:{ch:R}{range:25}]` |

#### F1 (N=4 ステージ): root が source 専用、子 4 個が L / R / L / R

| prim | description |
|---|---|
| root | `[3dstream-stereo:{url:<URL>}{range:30}]` |
| child 1 | `[3dstream-stereo:{ch:L}{range:20}]` |
| child 2 | `[3dstream-stereo:{ch:R}{range:20}]` |
| child 3 | `[3dstream-stereo:{ch:L}{range:20}{volume:0.6}]` |
| child 4 | `[3dstream-stereo:{ch:R}{range:20}{volume:0.6}]` |

#### F2 (sum-to-mono): L=440Hz / R=880Hz の test stream URL を root に、ch=M スピーカー 1 個を別 prim に

| prim | description |
|---|---|
| root | `[3dstream-stereo:{url:<TEST_440_880_URL>}{range:30}]` |
| child 1 | `[3dstream-stereo:{ch:M}{range:25}]` |

→ M スピーカーの周波数解析で 440Hz と 880Hz の両方が含まれること、振幅が源音と一致 (-6 dB ではなく) することを確認。

#### F3 (per-speaker range): 1 個だけ range:50、他は range:10

| prim | description |
|---|---|
| root | `[3dstream-stereo:{url:<URL>}{range:10}]` |
| child 1 | `[3dstream-stereo:{ch:L}{range:50}]` ← 遠くまで聞こえる |
| child 2 | `[3dstream-stereo:{ch:R}{range:10}]` |

#### F4 (per-speaker volume): 4 段階 1.0 / 0.8 / 0.5 / 0.2

| prim | description |
|---|---|
| root | `[3dstream-stereo:{url:<URL>}{range:30}]` |
| child 1 | `[3dstream-stereo:{ch:L}{volume:1.0}]` |
| child 2 | `[3dstream-stereo:{ch:R}{volume:0.8}]` |
| child 3 | `[3dstream-stereo:{ch:L}{volume:0.5}]` |
| child 4 | `[3dstream-stereo:{ch:R}{volume:0.2}]` |

#### F5 (上限超過): `Stream3DStereoMaxSpeakers` を 8 などに下げて、9 個以上 link

→ 9 個目以降が落ちて `SpeakerOverLimit` 通知が出ることを確認。30 秒以内の連投は重複抑制される (F9)。

#### F6 / F7 (LSL 後付け / link 操作): 起動後に description / link を変更

```lsl
default {
    touch_start(integer n) {
        llSetObjectDesc("[3dstream-stereo:{ch:L}{range:25}]");
    }
}
```

タッチで child の tag を後付け → 数秒以内に再評価され該当 channel が増える。`llCreateLink` / `llBreakLink` で linkset 構成を変えても F2-c (`detectLinksetStructureChanges`) が拾う。

#### F8 / F9 (書式エラー通知): わざと壊した tag

| 用途 | description |
|---|---|
| BadCh 通知 | `[3dstream-stereo:{ch:X}{range:20}]` |
| BadRange 通知 | `[3dstream-stereo:{ch:L}{range:abc}]` |
| BadVolume 通知 | `[3dstream-stereo:{ch:L}{volume:1.5}]` |
| EmptyUrl 通知 | `[3dstream-stereo:{url:}{range:20}]` |
| NoSpeakers 通知 | root に `{url}` だけ書いて子 prim を link しない (or 全員 ch なし) |

→ 各通知の文言に「例: ...」が含まれることを確認 (F8)、同じ tag を 30 秒以内に複数回触っても通知が 1 回に絞られることを確認 (F9)。

---

## 6. 残課題・スコープ外

- **mono タグ `[3dstream:...]` の N 化**: 同設計を mono にも展開 — 必要が出たら r9 以降で別リリース
- **per-speaker delay / EQ / pan offset**: v1 では対応せず、需要が出てから検討
- **5.1ch / Ambisonic ソース対応**: ソースは mono / stereo HTTP 入力に限定
- **リンクセットを跨いだスピーカー連携**: 1 root = 1 binding を維持
- **スピーカー数上限の昇格 (32 / 64)**: 実運用で必要が出たら `Stream3DStereoMaxSpeakers` の既定値変更で対応
- **設定 UI への露出**: 必要が確認されたら r9 以降で Sound prefs に追加

---

## 7. commit 一覧

| 短SHA | 内容 |
|---|---|
| _(F0)_ | docs: r8 — add distributed-stereo spec and implementation phases |
| _(F1a)_ | r8: distributed-stereo — drop legacy {l:N}{r:N} stereo format |
| _(F1b)_ | r8: distributed-stereo — add parseDistributedStereoTag (F1) |
| _(F2)_ | r8: distributed-stereo — linkset aggregation and child property tracking (F2) |
| _(F3)_ | r8: distributed-stereo — introduce LLPositionalStreamMulti (F3) |
| _(F3)_ | r8: distributed-stereo — wire multi-stream into manager (F3) |
| _(F4)_ | r8: distributed-stereo — throttled error notifications with examples (F4) |
| _(F5)_ | r8: distributed-stereo — Stream3DStereoMaxSpeakers setting (F5) |
| _(F6)_ | r8: distributed-stereo — close acceptance with bench results (F6) |

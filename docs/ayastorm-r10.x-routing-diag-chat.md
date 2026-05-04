# AYAstorm r10.x: routing 診断を chat 通知 + Preferences UI 化

**作成日**: 2026-05-04
**対象**: AYAstorm `feature/aya-r10.x-routing-diag-chat`
**仕様**: `doc/spec_5_1ch_placement.md` §4.4 / §13.7
**前リリース**: r10 (`feature/aya-r10-5_1ch-placement`, PR #27)

> **本書の役割**: r10 P9 #5 「routing 診断 log 検証」が本来意図 (chat 通知 + Preferences UI) と乖離していた件の修正リリース工程を記録する。受入条件 / commit 一覧は spec §13.7 を canonical とし、本書は phase trace を担う。

---

## 1. ゴール

r10 P5/P6 で実装した routing 診断は以下の 2 点で AYA さんの本来意図と乖離していた:

1. **出力先**: `LL_WARNS("Stream3D")` への log 出力 → **chat 通知 (`notifyStream3D`)** に変更
2. **設定 UI**: debug settings (`Stream3DRoutingDiagnostic`) のみ → **Preferences > Sound タブの checkbox** を追加 (debug settings は同キーで機能維持)

加えて r10 P9 #5 検証は 6ch source の log emit を grep で確認しただけで、2ch×6prim placement での fallback 文言出力は実機未確認だった。r10.x P4 で 5 ケースの chat 文言を実機で観察し、本来の検証を完了する。

タグ書式 / 内部 routing ロジックは r10 から完全互換 (出力チャネルと UI が変わるだけ)。

---

## 2. フェーズ分解

viewer 側コードの差分は小さい (chat 切替 + checkbox 追加 + 文言調整)。工数の主軸は「5 ケースの実機検証」と「pre-compaction で取り残された spec / header コメント整合の pre-commit check」。

### P1: 仕様書側の更新

**目的**: §4.4 を chat 通知化、§13.7 に r10.x スコープを追加。実装より先に仕様で本来意図を確定する。

**作業**:
- `doc/spec_5_1ch_placement.md` §4.4.1 を 5 ケース表形式に整理 (各 emit 文言を確定)
- §4.4.2 throttle key 維持 / §4.4.3 出力先を「Preferences > Sound checkbox」と明記
- §6 設定キー一覧を r10.x 仕様に更新 (Preferences UI bind)
- §13.7 として r10.x スコープ・互換性・残課題を追記

**完了条件**: spec が r10.x 完成形を canonical として記述している。

---

### P2: `emitRoutingDiagnostic` を chat 通知化 (LL_WARNS 撤去)

**目的**: `llpositionalstreammgr.cpp::emitRoutingDiagnostic` を spec §4.4.1 通りの chat 通知に書き換える。

**ファイル**:
- `indra/newview/llpositionalstreammgr.cpp` (`emitRoutingDiagnostic` 本体)
- `indra/newview/llpositionalstreammgr.h` (ヘッダコメントは P3 で同梱更新)

**作業**:
- `LL_WARNS("Stream3D")` 全撤去 + `<cstring>` include 撤去
- (a) source-side: 6ch source × `ch_count[FL/FR/C/LFE/SL/SR]==0` の各 ch で 1 行ずつ通知
  - `[ch:L/R/M] bucket あり` → `"FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)"`
  - `[ch:L/R/M] bucket なし` → `"FL content has no destination — dropped (source is 6ch, no ch:L/R/M prim)"`
- (b) prim-side: 1ch/2ch source × 5.1 prim あり の各 ch で 1 行通知
  - `ch:FL` → 2ch なら `"ch:FL prim playing L (source is 2ch)"` / 1ch なら `"...playing M..."`
  - `ch:FR` → 2ch なら `"...playing R..."` / 1ch なら `"...playing M..."`
  - `ch:C` → `"ch:C prim playing M (source is Nch)"`
  - `ch:LFE/SL/SR` → `"ch:X prim silent (source is Nch)"`
- emit 経路は `notifyStream3D` (auto-prefix `"3D Stream: "`)

**完了条件**: ビルド成功 / r10 で grep されていた LL_WARNS が 0 件 / chat 経路の文言が spec §4.4.1 と完全一致。

---

### P3: Preferences > Sound タブに checkbox 追加

**目的**: debug settings 限定だった `Stream3DRoutingDiagnostic` を Preferences UI に bind する。

**ファイル**:
- `indra/newview/skins/default/xui/en/panel_preferences_sound.xml` (3D Stream slider 行直下に checkbox 追加)
- `indra/newview/app_settings/settings.xml` (`Stream3DRoutingDiagnostic` の Comment を r10.x 仕様に更新)
- `indra/newview/llpositionalstreammgr.h` (`emitRoutingDiagnostic` ヘッダコメントを r10.x 仕様に更新)
- `doc/spec_5_1ch_placement.md` (§4.4.3 設定 UI 位置を実装ファイル名/コントロール名で確定 / §4.4.1 / §13.7 文言は EN 固定 + 多言語化将来課題に修正)

**作業**:
- `stream3d_routing_diagnostic_check` checkbox を `Stream3DEnabled` 行の隣に配置
- label: `Show channel routing diagnostics in chat`
- tool_tip で「6ch source × stereo prim / stereo source × 5.1 配置で 1 ケース 1 行」「production では OFF」を案内
- default value (0 = OFF) は据え置き

**完了条件**: Preferences UI に checkbox が表示され、debug settings と双方向同期する / default OFF で chat 静音。

---

### P4: 5 fallback ケースの実機検証

**目的**: r10 P9 #5 で grep 確認のみだった routing 診断を、chat 文言として 5 ケース全てを実機で観察する。

**作業**: 各ケースで該当配置を組み、chat 文言が spec §4.4.1 と一致することを目視確認。

| # | ケース | 確認内容 |
|---|---|---|
| 1 | 1ch source × ch:FL/FR/C prim | `ch:FL prim playing M (source is 1ch)` ほか 3 行 |
| 2 | 1ch source × ch:LFE/SL/SR prim | `ch:LFE prim silent (source is 1ch)` ほか 3 行 |
| 3 | 1ch direct path (`[3dstream:url=...]`) | r10 非タッチ、文言出ずに通常再生 |
| 4 | 6ch source × ch:L/R/M only | `FL content folded into BS.775 downmix ...` ほか 6 行 |
| 5 | 6ch source × prim なし (drop) | `FL content has no destination — dropped ...` ほか 6 行 |

**配信検証材料の補足**:
- WAV (FMOD type 15) は 6ch で受け付けられず → FLAC で代替 (Opus は Icecast push で `FMOD_ERR_FILE_COULDNOTSEEK` 発生のため static HTTP に変更)
- ffmpeg → `test_5_1.flac` (15 秒) → `python3 -m http.server 8080`

**完了条件**: 5 ケース全て chat に spec 通りの文言 / 30 秒 throttle 動作 / OFF で完全静音。

---

### P5 (予定): phase doc 整理 + クローズ

**目的**: 本書 + r10 phase doc P9 #5 印を更新、PR / merge 後に r10.x をクローズする。

**作業**:
- `docs/ayastorm-r10-5_1ch-placement.md` の P9 #5 印 / §4.1 routing 診断項目を `[x]` → `[~]` に下げ、本書を参照
- 本書の commit hash 表 (§5) を最終 commit で埋める
- spec §13.7 受入クローズを書き上げる

---

## 3. マイルストーン依存関係

```
P1 ─→ P2 ─→ P3 ─→ P4 ─→ P5
```

P1 (仕様確定) → P2 (chat 切替) → P3 (UI) → P4 (検証) は 1 本道。コードベースの差分が小さいため並列化の旨味なし。

---

## 4. 動作確認チェックリスト

`[x]` 実機実測で通過 / `[~]` コードレビューのみ / `[ ]` 未実施。

### 4.1 r10.x 新規 (P2 / P3 / P4)

- [x] **chat 通知**: `Stream3DRoutingDiagnostic=true` で spec §4.4.1 の 5 ケース文言が chat に発火
- [x] **throttle**: `(root_id, url, observed_channel_count, prim_set_signature)` key 同条件 30 秒以内の再発火抑止
- [x] **OFF 静音**: checkbox OFF で chat に何も出ない / log にも出ない (LL_WARNS 撤去済)
- [x] **Preferences UI**: 「Show channel routing diagnostics in chat」が 3D Stream slider 行の下に表示、debug settings (`Stream3DRoutingDiagnostic`) と双方向同期
- [x] **default OFF**: 新規環境 / 設定 reset で値 0

### 4.2 r10 互換 (回帰確認)

- [x] r10 §4.2 互換マトリクス (6ch / 2ch / 1ch × 9 ch tag) 経路は r10.x で非タッチ、コードレビューのみ
- [x] r9 / r8 経路 (ch:L/R/M only) は r10.x で非タッチ、回帰なし

---

## 5. 参照リンク

| 項目 | 参照先 |
|---|---|
| r10.x スコープ / 互換性 | `doc/spec_5_1ch_placement.md` §13.7 |
| chat 通知 5 ケース文言表 | `doc/spec_5_1ch_placement.md` §4.4.1 |
| throttle key 仕様 | `doc/spec_5_1ch_placement.md` §4.4.2 |
| 設定 UI 位置 | `doc/spec_5_1ch_placement.md` §4.4.3 |
| 設定キー一覧 | `doc/spec_5_1ch_placement.md` §6 |
| r10 phase doc (本書の前段) | `docs/ayastorm-r10-5_1ch-placement.md` |

### r10.x commit 一覧

| Phase | Commit | 内容 |
|---|---|---|
| P1 | `efc20693fa` | spec — routing 診断を chat / Preferences 化 (§4.4 / §6 / §13.7) |
| P2 | `535512718c` | emitRoutingDiagnostic を chat 通知化 (LL_WARNS 撤去) |
| P3 | `603abb529f` | Stream3DRoutingDiagnostic を Preferences UI に追加 |
| P5 | (本 commit) | phase doc 整理 |

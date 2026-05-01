# AYAstorm r5: Naming Refactor

## 1. 目的とスコープ

r5 (`v7.2.5-ayastorm-r5` 想定) は新機能追加ではなく、AYAstorm 系機能の命名一貫性整理を目的としたリファクタリリース。**3D Stream** (旧 AYAstream) と **Parcel Hide** (旧 RenderHideOutsideParcel + `[AYAstorm:...]` タグ) の 2 機能を一括整理する。

主対象:
1. **ユーザー開示される識別子** (Description / About Land タグ、UI 文言、設定キー名) — 統一感が必須
2. **内部コード/ログ/コメント** — 上記との整合のため随伴して整理
3. **タグ書式の統一** — 2 機能で異なっていた `{key:value}` 区切り規則を片方に揃える

非対象 (r5 では触らない):
- 機能の挙動変更 / 新機能追加
- 内部クラス名 (`LLPositionalStream*` 等) — 既にユーザー開示面と分離されており技術的にも正確

**後方互換方針**: 既存配置プリム/About Land 記述を再編集させない。旧タグ prefix (`[ayastream:...]` / `[ayastream-stereo:...]` / `[AYAstorm:...]`) は parser に恒久 alias として残す。設定キー (旧 `AYAStream*` / `FSRenderHideOutsideParcel*`) は起動時に旧→新へ migrate (詳細は §5)。

---

## 2. 現状調査 (r4 時点)

### 2.0 対象 2 機能の構造的問題サマリ

| 機能 | 利用者側設定キー prefix | 区画/プリム側タグ prefix | UI ラベル | 内部クラス/変数語彙 | 一貫性 |
|---|---|---|---|---|---|
| 3D Stream (旧 AYAstream) | `AYAStream*` | `[ayastream:...]` / `[ayastream-stereo:...]` | "3D Stream" (r4 で確定) | `LLPositionalStream*` / log channel `AYAStream` | UI とタグ/設定キーが不一致 |
| Parcel Hide | `FSRenderHideOutsideParcel*` (FS 接頭辞) | `[AYAstorm:...]` (プロジェクト名そのまま) | "Hide objects outside your parcel" | `sRenderHideOutsideParcel*` / `sParcelOwnerTag*` | 設定キーは FS、タグは AYAstorm、内部は説明的英語の三系統 |

タグ書式の差異:
- 3D Stream: `[ayastream:{key:value},{key:value}]` — `{}` 間に **comma 必須**
- Parcel Hide: `[AYAstorm:{key:value}{key:value}]` — `{}` 間に **何もなし** (parser は `{...}` だけスキャン)

### 2.1 ブランド名表記の揺れ

現コードベースには同一機能を指す表記が **3種類** 混在:

| 表記 | 用途 | 出現箇所 |
|---|---|---|
| `AYAStream` | 設定キー、ログ channel、XUI element name | settings.xml, llviewercontrol.cpp, llpositionalstream*.cpp, panel_*.xml の `name=`/`control_name=` |
| `AYAstream` | ユーザー通知テキスト、tooltip | llpositionalstreammgr.cpp:198 (`"AYAstream: "`)、panel_volume_pulldown.xml の `tool_tip` |
| `AYAstorm` | settings.xml の Comment 文字列内 | settings.xml の `<Comment>` 内 (機能ではなく "AYAstorm 由来" の意味で使用) |
| `3D Stream` | UI 表示ラベル (r4 で統一) | panel_volume_pulldown.xml, panel_preferences_sound.xml の `label=` |

加えて、内部コード (クラス名/ファイル名) は **`PositionalStream`** という別の語彙を使用している (§2.5)。

### 2.2 Description タグ (ユーザー開示・最重要)

LSL から `llSetObjectDesc()` で書き込まれる、ユーザーがプリム編集 UI で目にする文字列。

| タグ | 用途 | 定義箇所 |
|---|---|---|
| `[ayastream:url=...,min=...,max=...]` | mono 位置音響ストリーム | `llpositionalstreammgr.cpp:247` (`kPrefix`) |
| `[ayastream-stereo:url=...,l=N,r=M]` | stereo 位置音響ストリーム (L/R 分離) | `llpositionalstreammgr.cpp:270` (`kPrefix`) |
| `ayastream` (substring) | Description 早期 reject 用の存在チェック | `llpositionalstreammgr.cpp:322` |

ヘッダのコメントでも参照: `llpositionalstreammgr.h:84,92,103,107`。

### 2.3 設定キー (`AYAStream*`、全 12 件)

定義: `indra/newview/app_settings/settings.xml`

| キー | 行 | 型 | デフォルト | 用途 |
|---|---|---|---|---|
| `AYAStreamDebugUrl` | 27560 | String | `""` | Debug 再生用 URL |
| `AYAStreamDebugPlay` | 27571 | Boolean | 0 | Debug mono 再生トグル |
| `AYAStreamDebugStereoPlay` | 27582 | Boolean | 0 | Debug stereo 再生トグル |
| `AYAStreamRolloffMin` | 27593 | F32 | 1.0 | FMOD 3D rolloff 最小距離 |
| `AYAStreamRolloffMax` | 27604 | F32 | 20.0 | FMOD 3D rolloff 最大距離 |
| `AYAStreamMaxDistance` | 27615 | F32 | 64.0 | プリム polling 最大距離 |
| `AYAStreamPollInterval` | 27626 | F32 | 30.0 | polling 間隔 (秒) |
| `AYAStreamVolumeMaster` | 27637 | F32 | 0.5 | マスターボリューム倍率 |
| `AYAStreamReconnectAttempts` | 27648 | S32 | 3 | 再接続試行回数 |
| `AYAStreamEnabled` | 27659 | Boolean | 1 | 機能全体 kill switch |
| `AYAStreamDescriptionScan` | 27670 | Boolean | 1 | Description scan 個別 kill switch |
| `AYAStreamMaxConcurrent` | 27681 | S32 | 4 | 同時バインド上限 |

参照箇所:
- `indra/newview/llviewercontrol.cpp` — signal listener 登録 6件 (1600-1606) + handler 関数群 (559-639)
- `indra/newview/llpositionalstreammgr.cpp` — 14箇所で `getF32`/`getBOOL`/`getS32` 経由参照

### 2.4 UI 文言と XUI 要素名

| ファイル | 行 | 種別 | 値 | 備考 |
|---|---|---|---|---|
| panel_volume_pulldown.xml (default) | 348 | `name=` | `AYAStream Volume` | スピーカーアイコンの pulldown |
| 同 | 349 | `label=` | `3D Stream` | r4 で確定 |
| 同 | 356 | `control_name=` | `AYAStreamVolumeMaster` | |
| 同 | 365 | `name=` | `enable_ayastream_check` | |
| 同 | 366 | `tool_tip=` | `Positional 3D audio streams from prims (AYAstream)` | "AYAstream" 表記残存 |
| 同 | 373 | `control_name=` | `AYAStreamEnabled` | |
| panel_volume_pulldown.xml (metaharper) | 317-341 | 同上一式 | 同上 | スキン側オーバライド |
| panel_preferences_sound.xml | 597 | `control_name=` | `AYAStreamVolumeMaster` | Preferences > Sound |
| 同 | 603 | `label=` | `3D Stream` | |
| 同 | 608 | `name=` | `AYAStream Volume` | |
| 同 | 617 | `control_name=` | `AYAStreamEnabled` | |
| 同 | 622 | `name=` | `enable_ayastream_check_volume` | |
| 同 | 623 | `tool_tip=` | `Positional 3D audio streams that prims publish via Description tags. Unlike parcel music, these streams start automatically based on what is in-world.` | tooltip 本文に "AYAstream" は含まれず |
| 同 | 637 | text content | `Hear media and sounds from (3D Stream):` | r4 で `(3D Stream)` 注記追加済 |

### 2.5 内部コード命名 (クラス・ファイル・シンボル)

ユーザー開示側 (`AYAStream`/`3D Stream`) とは別系統で **`PositionalStream`** が定着している。

ファイル:
- `indra/llaudio/llpositionalstream.{cpp,h}` — mono ストリーム実装
- `indra/llaudio/llpositionalstreamstereo.{cpp,h}` — stereo ストリーム実装
- `indra/newview/llpositionalstreammgr.{cpp,h}` — マネージャ (シングルトン)

クラス:
- `LLPositionalStream` (llpositionalstream.h:42)
- `LLPositionalStreamStereo` (llpositionalstreamstereo.h:72)
- `LLPositionalStreamMgr` (llpositionalstreammgr.h:40)

統合ポイントマーカー:
- `// <FS:AYA> [PositionalStream] ...` 形式のブロックコメントで挿入箇所を明示
  - `llappviewer.cpp:6175-6177` (mainloop tick)
  - `llselectmgr.cpp:75-76, 6225-6228, 6441-6444` (Description フィード)

ビルド登録:
- `indra/llaudio/CMakeLists.txt:34-35,42-43`
- `indra/newview/CMakeLists.txt:680, 1536`

### 2.6 ログ channel 名

`LL_INFOS`/`LL_WARNS`/`LL_DEBUGS` の channel 名として **`"AYAStream"`** で統一済 (約 30 箇所)。
- `indra/llaudio/llpositionalstream.cpp` (8件)
- `indra/llaudio/llpositionalstreamstereo.cpp` (10件)
- `indra/newview/llpositionalstreammgr.cpp` (約 17件)

ユーザーは `--logdebug AYAStream` で出力を絞り込む (llpositionalstreammgr.cpp:915 にコメントあり)。channel 名変更はこの operator interface に影響する。

### 2.7 ユーザー通知文字列

`notifyAYAStream()` (llpositionalstreammgr.cpp:196-) は近接チャットに以下プレフィックスで投稿:
```cpp
const std::string text = "AYAstream: " + message;
```
M8 で実装。LL/FS 両スタイルでチャットに表示される。ここの表記が `[ayastream:...]` タグや UI ラベルとずれている。

### 2.8 ドキュメント

- `docs/ayastorm-positional-stream.md` — 40 occurrences (実装サマリ)
- `doc/spec_positional_stream_audio.md` — 18 occurrences (仕様書)

両ドキュメント中、タグ名・設定キー名・クラス名すべてが現行表記で記述されている。

### 2.9 3D Stream — 観察メモ (§3 命名方針検討時の入力)

1. **3種類のブランド表記**: `AYAStream` / `AYAstream` / `AYAstorm` が混在。同じ機能を指している。
2. **コード語彙との分離**: 内部は既に `PositionalStream` で統一されており、ブランド名は変更しても波及しない。これは r5 でクラス名はそのまま残す選択肢を取りやすい。
3. **ユーザー開示面の不整合の核**: タグ (`[ayastream:...]`) と UI (`3D Stream`) が直接対応していないため、ユーザーが "Description に何を書けば 3D Stream を有効にできるか" を推測しづらい。
4. **operator interface**: ログ channel 名 `AYAStream` は debug logging の引数として既に文書化されているため、変更は破壊的。
5. **設定キー数**: 12個。XML 側は機械的置換で済むが、`gSavedSettings.getXxx("AYAStream...")` の C++ 参照が約 20箇所あり、ここは慎重に。

### 2.10 Parcel Hide — タグ・設定キー・UI

**Description タグ** (About Land の Description に書く)

| タグ | 用途 | 定義箇所 |
|---|---|---|
| `[AYAstorm:]` | デフォルト発火 (hideoutside=true, keepavatars=false, keepownobject=false) | `pipeline.cpp:3127` (`PREFIX`) |
| `[AYAstorm:{keepavatars:true}]` | アバター可視を維持 | 同 parser |
| `[AYAstorm:{keepavatars:true}{keepownobject:true}]` | 自分の物も可視 | 同 |
| `[AYAstorm:{hideoutside:false}]` | タグ一時無効化 | 同 |

**設定キー** (3 件、`indra/newview/app_settings/settings.xml`)

| キー | 行 | 型 | デフォルト | 用途 |
|---|---|---|---|---|
| `FSRenderHideOutsideParcel` | 26066 | Boolean | 0 | 機能 ON/OFF (利用者側) |
| `FSRenderHideOutsideParcelKeepAvatars` | 26077 | Boolean | 1 | アバター可視維持 |
| `FSRenderHideOutsideParcelKeepOwn` | 26088 | Boolean | 1 | 自所有物可視維持 |

参照: `llviewercontrol.cpp` の handler 3件 (332-348) + listener 登録 3件 (1593-1599) + 初期値読み出し 3件 (1597-1599)

**内部 C++ シンボル** (LLPipeline 静的変数)

| シンボル | ファイル:行 | 用途 |
|---|---|---|
| `LLPipeline::sRenderHideOutsideParcel` | pipeline.h:684 / pipeline.cpp:344 | 設定キーミラー (利用者側) |
| `LLPipeline::sRenderHideOutsideParcelKeepAvatars` | pipeline.h:685 / pipeline.cpp:345 | 同 |
| `LLPipeline::sRenderHideOutsideParcelKeepOwn` | pipeline.h:686 / pipeline.cpp:346 | 同 |
| `LLPipeline::sParcelOwnerTagActive` | pipeline.h:691 / pipeline.cpp:349 | タグ由来 ON/OFF (区画オーナー側) |
| `LLPipeline::sParcelOwnerTagKeepAvatars` | pipeline.h:692 / pipeline.cpp:350 | 同 (タグ値) |
| `LLPipeline::sParcelOwnerTagKeepOwn` | pipeline.h:693 / pipeline.cpp:351 | 同 |
| `LLPipeline::shouldHideForOutsideParcel()` | pipeline.h:701 / pipeline.cpp:3210 | 描画フィルタ判定本体 |
| `LLPipeline::parseAYAstormParcelTag()` | pipeline.cpp:3123 | タグ parser |
| `LLPipeline::ParcelTagOverride` 構造体 | pipeline.h:688 周辺 | parser 戻り値型 |

参照箇所: `llvograss.cpp:640`、`llhudtext.cpp:136`、`llviewerpartsim.cpp:795`、`lldrawpooltree.cpp:99`、`llvovolume.cpp:5996` (描画パイプラインから利用)

**統合点マーカコメント**

`// <FS:AYA> [RenderHideOutsideParcel] ...` および `// <FS:AYA> [RenderHideOutsideParcel-Tag] ...` 形式で約 14 箇所。

**UI** (`indra/newview/skins/default/xui/en/panel_preferences_firestorm.xml`)

| 行 | 種別 | 値 |
|---|---|---|
| 1534 | check_box `label` | `Hide objects outside your parcel` |
| 1536-1537 | name/control_name | `FSRenderHideOutsideParcel` |
| 1545 | check_box `label` | `Keep avatars and attachments visible` |
| 1547-1548 | name/control_name | `FSRenderHideOutsideParcelKeepAvatars` |
| 1556 | check_box `label` | `Keep your own objects visible` |
| 1558-1559 | name/control_name | `FSRenderHideOutsideParcelKeepOwn` |
| 1580 | section header text | `Parcel owner: enforce these settings on visiting AYAstorm users` |
| 1591 | description text | `... When an AYAstorm user is standing in your parcel, the tag overrides their personal settings. ...` |
| 1601-1604 | example text | `[AYAstorm:]` 等 4例 |

### 2.11 Parcel Hide — ドキュメント

- `docs/ayastorm-render-hide-outside-parcel.md` — 実装サマリ (タグ仕様詳細あり、§7.1 参照)

### 2.12 Parcel Hide — 観察メモ

1. **二系統のブランド混在**: 同一機能なのに利用者側設定キーは `FS*`、タグは `[AYAstorm:...]`。ユーザーから見ると「これは Firestorm の機能? AYAstorm の機能?」が分からない。
2. **タグ書式が 3D Stream と非対称**: parcel parser は `{...}` を直接スキャンするため間隔は無視 (緩い側)。3D Stream parser は comma 必須 (厳しい側)。r5 で書式統一の好機。
3. **内部 C++ シンボル数**: 3つの static bool (利用者設定ミラー) + 3つの static bool (タグ由来) + 1関数 + 1構造体 + 1 parser 関数 + 描画利用 5箇所。
4. **マーカコメント**: `[RenderHideOutsideParcel]` および `[RenderHideOutsideParcel-Tag]` の 2 種が約 14 箇所に挿入されている (機能識別用途)。

---

## 3. 命名方針

**共通方針: ユーザー開示面の各機能を、その機能を表す名詞 1 語で統一する**

- タグ prefix・設定キー・UI ラベルを 1 つの機能名に揃える
- 設定キーから AYAstorm 追加分の `FS*` 接頭辞を外す (`FS*` は Firestorm オリジナル設定の慣行を維持し、AYAstorm 追加分は接頭辞なし)
- 内部クラス/ファイル名は技術的に正確であれば温存 (`LLPositionalStream*` 等)
- ブランド名 "AYAstorm" はプロジェクト識別子として settings.xml の Comment 等に残す (機能名と区別)
- タグ書式は **comma 任意** に統一 — `{a:1}{b:2}` も `{a:1},{b:2}` も `{a:1} , {b:2}` も同じ意味

### 3.1 3D Stream

**採用: UI に合わせて全面的に "3D Stream" 系へ統一**

- 設定キー prefix は **`Stream3D*`** (`3DStream*` だと先頭数字で C++ 識別子と非対称になるため)
- ログ channel も `Stream3D` に揃える
- 内部クラス/ファイル名 (`LLPositionalStream*`) は温存

棄却案:
- **`[aya:3dstream:...]` 名前空間**: 将来拡張に強いが、現時点で他 AYAstorm 機能はタグを使っておらず過剰設計
- **現状維持 + ドキュメント化のみ**: タグと UI の不整合がユーザーに伝わらない問題が解消しない

### 3.2 Parcel Hide

**採用: 機能を表す動詞+名詞 "Parcel Hide" 系へ統一**

- タグ prefix: **`[parcelhide:...]`** — UI ラベル "Hide objects outside your parcel" の動詞 (hide) と境界 (parcel) を直接反映
- 設定キー prefix: **`ParcelHide*`** — `FS` 接頭辞を外す
- 内部 static bool は新キー名にミラー: `sParcelHideEnabled` 等
- タグ由来 state を保持する `sParcelOwnerTag*` は温存 (タグ概念の語彙として正確)
- parser 関数 `parseAYAstormParcelTag()` は `parseParcelHideTag()` にリネーム
- マーカコメント `[RenderHideOutsideParcel]` / `[RenderHideOutsideParcel-Tag]` は `[ParcelHide]` / `[ParcelHide-Tag]` に
- 描画判定関数 `shouldHideForOutsideParcel()` は描画 API として温存 (利用側コードへの波及を抑える)

棄却案:
- **`[parcelview:...]` / `ParcelView*`**: 中立だが曖昧で機能内容が伝わらない
- **`[parcelprivacy:...]` / `ParcelPrivacy*`**: オーナー強制側のニュアンスが強すぎ、利用者側設定 (個人での撮影用途) と合わない

---

## 4. リネーム対応表

### 4.0 タグ書式統一 (両機能共通)

新 parser は **`{...}` ブロックを直接スキャン**し、ブロック間の文字 (comma, 空白, 改行, 何もない) はすべて区切りとして無視する。これは現 parcel-hide parser と同じ挙動で、3D Stream の従来書式 (comma 必須) も新書式も両方通る。共通ヘルパに切り出すか、各機能で別実装かは実装時に判断 (parser ロジックは数十行で軽量)。

### 4.1 Description タグ (3D Stream)

| 旧 | 新 |
|---|---|
| `[ayastream:{url:...},...]` | `[3dstream:{url:...},...]` |
| `[ayastream-stereo:{url:...},...]` | `[3dstream-stereo:{url:...},...]` |
| substring 早期 reject `"ayastream"` | `"3dstream"` |

### 4.2 設定キー (3D Stream、12件)

| 旧 | 新 |
|---|---|
| `AYAStreamDebugUrl` | `Stream3DDebugUrl` |
| `AYAStreamDebugPlay` | `Stream3DDebugPlay` |
| `AYAStreamDebugStereoPlay` | `Stream3DDebugStereoPlay` |
| `AYAStreamRolloffMin` | `Stream3DRolloffMin` |
| `AYAStreamRolloffMax` | `Stream3DRolloffMax` |
| `AYAStreamMaxDistance` | `Stream3DMaxDistance` |
| `AYAStreamPollInterval` | `Stream3DPollInterval` |
| `AYAStreamVolumeMaster` | `Stream3DVolumeMaster` |
| `AYAStreamReconnectAttempts` | `Stream3DReconnectAttempts` |
| `AYAStreamEnabled` | `Stream3DEnabled` |
| `AYAStreamDescriptionScan` | `Stream3DDescriptionScan` |
| `AYAStreamMaxConcurrent` | `Stream3DMaxConcurrent` |

### 4.3 C++ 識別子 (3D Stream)

| 旧 | 新 |
|---|---|
| `handleAYAStreamRolloffChanged` | `handleStream3DRolloffChanged` |
| `handleAYAStreamVolumeMasterChanged` | `handleStream3DVolumeMasterChanged` |
| `handleAYAStreamEnabledChanged` | `handleStream3DEnabledChanged` |
| `handleAYAStreamDescriptionScanChanged` | `handleStream3DDescriptionScanChanged` |
| `handleAYAStreamDebugPlayChanged` | `handleStream3DDebugPlayChanged` |
| `handleAYAStreamDebugStereoPlayChanged` | `handleStream3DDebugStereoPlayChanged` |

### 4.4 ログ channel と通知文字列 (3D Stream)

| 旧 | 新 |
|---|---|
| `LL_INFOS("AYAStream") << ...` (約 30 箇所) | `LL_INFOS("Stream3D") << ...` |
| `notifyAYAStream()` 関数名 | `notifyStream3D()` |
| 通知 prefix `"AYAstream: "` | `"3D Stream: "` |

### 4.5 XUI element (3D Stream)

| ファイル | 旧 | 新 |
|---|---|---|
| panel_volume_pulldown.xml (default + metaharper) | `name="AYAStream Volume"` | `name="Stream3D Volume"` |
| 同 | `name="enable_ayastream_check"` | `name="enable_stream3d_check"` |
| 同 | `tool_tip="Positional 3D audio streams from prims (AYAstream)"` | `tool_tip="Positional 3D audio streams from prims"` (括弧内冗長なので削除) |
| panel_preferences_sound.xml | `name="AYAStream Volume"` | `name="Stream3D Volume"` |
| 同 | `name="enable_ayastream_check_volume"` | `name="enable_stream3d_check_volume"` |

### 4.6 Description タグ (Parcel Hide)

| 旧 | 新 |
|---|---|
| `[AYAstorm:]` | `[parcelhide:]` |
| `[AYAstorm:{keepavatars:true}]` | `[parcelhide:{keepavatars:true}]` |
| `[AYAstorm:{keepavatars:true}{keepownobject:true}]` | `[parcelhide:{keepavatars:true}{keepownobject:true}]` |
| `[AYAstorm:{hideoutside:false}]` | `[parcelhide:{hideoutside:false}]` |

タグ内のキー名 (`hideoutside`, `keepavatars`, `keepownobject`) はそのまま温存。

### 4.7 設定キー (Parcel Hide、3 件)

| 旧 | 新 |
|---|---|
| `FSRenderHideOutsideParcel` | `ParcelHideEnabled` |
| `FSRenderHideOutsideParcelKeepAvatars` | `ParcelHideKeepAvatars` |
| `FSRenderHideOutsideParcelKeepOwn` | `ParcelHideKeepOwn` |

### 4.8 C++ 識別子 (Parcel Hide)

| 旧 | 新 |
|---|---|
| `LLPipeline::sRenderHideOutsideParcel` | `LLPipeline::sParcelHideEnabled` |
| `LLPipeline::sRenderHideOutsideParcelKeepAvatars` | `LLPipeline::sParcelHideKeepAvatars` |
| `LLPipeline::sRenderHideOutsideParcelKeepOwn` | `LLPipeline::sParcelHideKeepOwn` |
| `handleRenderHideOutsideParcelChanged` | `handleParcelHideEnabledChanged` |
| `handleRenderHideOutsideParcelKeepAvatarsChanged` | `handleParcelHideKeepAvatarsChanged` |
| `handleRenderHideOutsideParcelKeepOwnChanged` | `handleParcelHideKeepOwnChanged` |
| `LLPipeline::parseAYAstormParcelTag` | `LLPipeline::parseParcelHideTag` |
| マーカコメント `// <FS:AYA> [RenderHideOutsideParcel]` | `// <FS:AYA> [ParcelHide]` |
| マーカコメント `// <FS:AYA> [RenderHideOutsideParcel-Tag]` | `// <FS:AYA> [ParcelHide-Tag]` |

### 4.9 XUI element (Parcel Hide)

| ファイル | 旧 | 新 |
|---|---|---|
| panel_preferences_firestorm.xml | `name="FSRenderHideOutsideParcel"` / `control_name="FSRenderHideOutsideParcel"` | `name="ParcelHideEnabled"` / `control_name="ParcelHideEnabled"` |
| 同 | `name="FSRenderHideOutsideParcelKeepAvatars"` / `control_name=...` | `name="ParcelHideKeepAvatars"` / `control_name="ParcelHideKeepAvatars"` |
| 同 | `name="FSRenderHideOutsideParcelKeepOwn"` / `control_name=...` | `name="ParcelHideKeepOwn"` / `control_name="ParcelHideKeepOwn"` |
| 同 | `enabled_control="FSRenderHideOutsideParcel"` x2 | `enabled_control="ParcelHideEnabled"` x2 |
| 同 (1601-1604) | `[AYAstorm:...]` 例示 4件 | `[parcelhide:...]` 例示 4件 |
| 同 (1591) | "When an AYAstorm user is standing in your parcel, ..." 説明文 | "When an AYAstorm viewer user is standing in your parcel, ..." (project 名として明示) |

### 4.10 温存するもの (両機能共通)

**3D Stream:**
- `LLPositionalStream` / `LLPositionalStreamStereo` / `LLPositionalStreamMgr` クラス名
- `llpositionalstream*.cpp/h` ファイル名
- `// <FS:AYA> [PositionalStream] ...` 統合点マーカコメント

**Parcel Hide:**
- `LLPipeline::sParcelOwnerTagActive` / `sParcelOwnerTagKeepAvatars` / `sParcelOwnerTagKeepOwn` (タグ概念の語彙として正確)
- `LLPipeline::shouldHideForOutsideParcel()` 関数 (描画 API として記述的)
- `LLPipeline::ParcelTagOverride` 構造体

**両機能共通:**
- settings.xml Comment 中の "AYAstorm" プロジェクト識別子 (機能を指す箇所のみ新名に書き換え)
- パネル上部 UI ラベル ("Hide objects outside your parcel" 等の説明的英語) は温存

---

## 5. 後方互換戦略

### 5.1 Description / About Land タグ — 恒久エイリアス

両機能とも parser に **新 prefix を先に試し、無ければ旧 prefix を試す** 二段構成を入れる。旧タグはサンセットせず**恒久的に parse 可能**とする。

**3D Stream** (`parseTag()` / `parseStereoTag()` at llpositionalstreammgr.cpp:244-291):
```cpp
// 擬似コード
static const std::string kPrefix       = "[3dstream:";
static const std::string kPrefixStereo = "[3dstream-stereo:";
static const std::string kPrefixOld       = "[ayastream:";        // legacy
static const std::string kPrefixOldStereo = "[ayastream-stereo:"; // legacy

if (!findTagBody(d, kPrefix, ...) && !findTagBody(d, kPrefixOld, ...)) return std::nullopt;
```

substring 早期 reject (llpositionalstreammgr.cpp:322) も `"3dstream"` || `"ayastream"` の OR 判定に。

**Parcel Hide** (`parseParcelHideTag()` at pipeline.cpp:3123 でリネーム後):
```cpp
// 擬似コード
static const std::string PREFIX    = "[parcelhide:";
static const std::string PREFIX_OLD = "[AYAstorm:"; // legacy, case-sensitive (元仕様準拠)

start = desc.find(PREFIX);
if (start == std::string::npos) start = desc.find(PREFIX_OLD);
```

理由: 既存配置プリム/About Land 記述を再編集させない。配布済みオブジェクト/notecard/インワールド作品/区画記述の互換性を完全保証。

### 5.2 タグ書式統一 — 後方互換

新 parser ロジック (4.0 参照) は `{...}` ブロック直接スキャン方式。旧 3D Stream 書式 (`{a:1},{b:2}`) も旧 Parcel Hide 書式 (`{a:1}{b:2}`) も同一 parser で処理可能。互換性破壊なし。

### 5.3 設定キー — 起動時マイグレーション

`LLAppViewer` 初期化の早い段階 (signal listener 登録前) で両機能分の旧→新 migration を実行:

```cpp
// 擬似コード
static const std::pair<const char*, const char*> kSettingsMigration[] = {
    // 3D Stream (12件)
    {"AYAStreamEnabled",            "Stream3DEnabled"},
    {"AYAStreamVolumeMaster",       "Stream3DVolumeMaster"},
    /* ... 残り 10 件 ... */
    // Parcel Hide (3件)
    {"FSRenderHideOutsideParcel",            "ParcelHideEnabled"},
    {"FSRenderHideOutsideParcelKeepAvatars", "ParcelHideKeepAvatars"},
    {"FSRenderHideOutsideParcelKeepOwn",     "ParcelHideKeepOwn"},
};

for (const auto& [old_key, new_key] : kSettingsMigration)
{
    // 旧キーがユーザー保存値として存在し、新キーが未設定 (= デフォルトのまま) なら値コピー
    if (gSavedSettings.controlExists(old_key) &&
        !gSavedSettings.getControl(old_key)->isDefault() &&
        gSavedSettings.getControl(new_key)->isDefault())
    {
        // 旧 → 新 へ値を移送
    }
}
```

settings.xml には **新キーのみ宣言**。旧キーが user settings.xml にだけ存在する場合に migration が走る。

サンセット計画:
- **r5**: 新キー追加 + マイグレーションコード追加 + 旧キー宣言は settings.xml から削除
- **r6 以降**: マイグレーションコード自体を削除 (旧キー残存ユーザーは r5 を1回でも起動していれば移行済)

### 5.4 ログ channel

3D Stream の `--logdebug AYAStream` → `--logdebug Stream3D` のみ破壊的変更。Parcel Hide は専用 log channel を持たないため影響なし。リリースノート・コミットメッセージで周知:

> `--logdebug AYAStream` is renamed to `--logdebug Stream3D`. Update your launcher / debug settings accordingly.

### 5.5 ユーザー通知文字列

3D Stream の通知 prefix `"AYAstream: "` → `"3D Stream: "` のみ。表示文字列のため互換性問題なし。Parcel Hide は通知を発しないため影響なし。

---

## 6. 影響ファイル一覧

### 6.1 コア (3D Stream)

| ファイル | 種別 | 概要 |
|---|---|---|
| `indra/newview/app_settings/settings.xml` | XML | 12 キー rename + Comment 中の "AYAstream" → "3D Stream" 置換 |
| `indra/newview/llpositionalstreammgr.cpp` | C++ | kPrefix x2 (旧 alias 追加)、substring 判定、設定キー参照約 14 箇所、log channel 約 17 箇所、`notifyAYAStream` 関数名、parser ロジックを comma 任意化 |
| `indra/newview/llpositionalstreammgr.h` | C++ | コメント中の旧キー名・タグ名置換 |
| `indra/llaudio/llpositionalstream.cpp` | C++ | log channel 8 箇所 |
| `indra/llaudio/llpositionalstreamstereo.cpp` | C++ | log channel 10 箇所 |
| `indra/newview/llviewercontrol.cpp` | C++ | handler 関数名 6 件、signal listener 登録 6 件、log channel、handler 内の設定キー参照 |

### 6.2 コア (Parcel Hide)

| ファイル | 種別 | 概要 |
|---|---|---|
| `indra/newview/app_settings/settings.xml` | XML | 3 キー rename + Comment 中の旧キー名参照置換 |
| `indra/newview/pipeline.cpp` | C++ | PREFIX (旧 alias 追加)、`parseAYAstormParcelTag` 関数名、static 変数 3件、マーカコメント、設定キー参照 |
| `indra/newview/pipeline.h` | C++ | static 変数宣言 3件、関数名、マーカコメント |
| `indra/newview/llviewercontrol.cpp` | C++ | handler 関数名 3件、signal listener 登録 3件、初期値読み出し 3件 |
| `indra/newview/llvograss.cpp` | C++ | static 変数参照 + マーカコメント |
| `indra/newview/llhudtext.cpp` | C++ | 同上 |
| `indra/newview/llviewerpartsim.cpp` | C++ | 同上 |
| `indra/newview/lldrawpooltree.cpp` | C++ | 同上 |
| `indra/newview/llvovolume.cpp` | C++ | 同上 |
| `indra/newview/lldrawable.cpp` / `lldrawable.h` | C++ | マーカコメント |

### 6.3 共通 (両機能横断)

| ファイル | 概要 |
|---|---|
| `indra/newview/llappviewer.cpp` | (新規) 起動時設定キーマイグレーション呼び出し (3D Stream 12件 + Parcel Hide 3件) |

### 6.4 UI (XUI)

| ファイル | 機能 | 概要 |
|---|---|---|
| `indra/newview/skins/default/xui/en/panel_volume_pulldown.xml` | 3D Stream | control_name x2、name x2、tool_tip |
| `indra/newview/skins/metaharper/xui/en/panel_volume_pulldown.xml` | 3D Stream | 同上 (skin override) |
| `indra/newview/skins/default/xui/en/panel_preferences_sound.xml` | 3D Stream | control_name x2、name x2、tool_tip |
| `indra/newview/skins/default/xui/en/panel_preferences_firestorm.xml` | Parcel Hide | name/control_name x3、enabled_control x2、タグ例示 4件、説明文 |

### 6.5 ドキュメント

| ファイル | 方針 |
|---|---|
| `doc/spec_positional_stream_audio.md` | **更新**: 仕様書として現行表記に揃える。冒頭に "r5 で `[ayastream:...]` から rename。旧 prefix は恒久 alias" を追記 |
| `docs/ayastorm-positional-stream.md` | **温存**: 履歴ドキュメントとして当時の表記を残す。最終節に "r5 で命名整理 → r5 doc 参照" のポインタを追加 |
| `docs/ayastorm-render-hide-outside-parcel.md` | **更新**: §7.1 タグ仕様を新表記に書き換え + 旧 prefix 恒久 alias を明記 |
| `docs/ayastorm-r5-naming-refactor.md` | (本ドキュメント) |

### 6.6 温存ファイル (変更なし)

- `indra/llaudio/llpositionalstream.h`、`llpositionalstreamstereo.h` — クラス名温存
- `llappviewer.cpp` の `LLPositionalStreamMgr::instance().update()` 呼び出し
- `llselectmgr.cpp` の Description フィードポイント
- `CMakeLists.txt` (ファイル名温存のため変更なし)
- `LLPipeline::shouldHideForOutsideParcel()` 関数 (描画 API として温存)
- `LLPipeline::sParcelOwnerTag*` 系 static 変数 (タグ概念の語彙として温存)

---

## 7. テスト観点

### 7.1 3D Stream — 後方互換 (回帰防止)

- [ ] 旧 `[ayastream:{url:...}]` タグ付きプリムを inworld に配置 → 従来通り mono 再生
- [ ] 旧 `[ayastream-stereo:{url:...},{l:1},{r:2}]` タグ → 従来通り stereo 再生
- [ ] 旧設定キー `AYAStreamVolumeMaster=0.7` を含む user settings.xml を r4 から引き継いで r5 起動 → `Stream3DVolumeMaster` に値が migrate されている
- [ ] migration 後、Sound prefs / pulldown の slider が `Stream3DVolumeMaster` を駆動

### 7.2 3D Stream — 新表記 (前進確認)

- [ ] 新タグ `[3dstream:{url:...}]` でプリム配置 → mono 再生
- [ ] 新タグ `[3dstream-stereo:{url:...},{l:1},{r:2}]` → stereo 再生
- [ ] 新書式 (comma 任意): `[3dstream:{url:...}{l:1}{r:2}]` も同等動作
- [ ] Debug Settings に `Stream3D*` キー一覧が表示される
- [ ] Sound prefs / pulldown UI が `Stream3DVolumeMaster` / `Stream3DEnabled` で動作

### 7.3 3D Stream — ログ / 通知

- [ ] `--logdebug Stream3D` で従来 `AYAStream` channel 出力相当が得られる
- [ ] チャット通知が `"3D Stream: ..."` プレフィックスで表示
- [ ] LL/FS 両チャットスタイルで通知到達 (M8 検証の継続)

### 7.4 Parcel Hide — 後方互換 (回帰防止)

- [ ] 旧 `[AYAstorm:]` タグの区画に立ち入る → 従来通り外側オブジェクト非表示
- [ ] 旧 `[AYAstorm:{keepavatars:true}]` → アバターは可視のまま外側オブジェクト非表示
- [ ] 旧 `[AYAstorm:{hideoutside:false}]` → タグ無効化、利用者側設定だけが効く Phase A 挙動
- [ ] 旧設定キー `FSRenderHideOutsideParcel=true` を含む user settings.xml を r4 から引き継いで r5 起動 → `ParcelHideEnabled` に migrate されチェック ON 状態で起動
- [ ] `FSRenderHideOutsideParcelKeepAvatars` / `KeepOwn` も同様に migrate

### 7.5 Parcel Hide — 新表記 (前進確認)

- [ ] 新タグ `[parcelhide:]` で区画 → 従来 `[AYAstorm:]` と同じ挙動
- [ ] 新タグ `[parcelhide:{keepavatars:true}{keepownobject:true}]` → 期待通りの可視維持
- [ ] 新書式 (comma 入り): `[parcelhide:{keepavatars:true},{keepownobject:true}]` も同等動作
- [ ] Debug Settings / Preferences > Firestorm に `ParcelHide*` 一覧が表示される
- [ ] Preferences UI のチェックボックス 3 つが `ParcelHideEnabled` / `KeepAvatars` / `KeepOwn` を駆動
- [ ] `enabled_control` 連動 (KeepAvatars / KeepOwn は親 Enabled OFF で disabled 表示) が機能

### 7.6 ビルド整合

- [ ] フルビルド (`autobuild build --fmodstudio`) パス
- [ ] grep `AYAStream`/`AYAstream` 残存ゼロ確認 (`docs/ayastorm-positional-stream.md` 履歴記述を除く)
- [ ] grep `FSRenderHideOutsideParcel` 残存ゼロ確認 (settings.xml の旧キー宣言は削除済のはず)
- [ ] grep `[AYAstorm:` (タグ表記) が pipeline.cpp の `PREFIX_OLD` 定義箇所のみであること

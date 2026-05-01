# AYAstorm r5: Naming Refactor

## 1. 目的とスコープ

r5 (`v7.2.5-ayastorm-r5` 想定) は新機能追加ではなく、AYAstorm 系機能の命名一貫性整理を目的としたリファクタリリース。

主対象:
1. **ユーザー開示される識別子** (Description タグ、UI 文言) — 統一感が必須
2. **設定キー** (`AYAStream*`) — 即時インパクトはないが将来メンテナンス性のため整理
3. **内部コード/ログ/コメント** — 上記との整合のため随伴して整理

非対象 (r5 では触らない):
- 機能の挙動変更 / 新機能追加
- 既存 Viewer/Firestorm 由来の用語 (parcel 関連等) の整理 — 別タイミングで検討

**後方互換方針**: 既存配置プリムを再編集させない。旧 Description タグはエイリアスとして残す。設定キーは旧→新マイグレーションを検討 (詳細は §3 で決定)。

---

## 2. 現状調査 (r4 時点)

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

### 2.9 観察メモ (§3 命名方針検討時の入力)

1. **3種類のブランド表記**: `AYAStream` / `AYAstream` / `AYAstorm` が混在。同じ機能を指している。
2. **コード語彙との分離**: 内部は既に `PositionalStream` で統一されており、ブランド名は変更しても波及しない。これは r5 でクラス名はそのまま残す選択肢を取りやすい。
3. **ユーザー開示面の不整合の核**: タグ (`[ayastream:...]`) と UI (`3D Stream`) が直接対応していないため、ユーザーが "Description に何を書けば 3D Stream を有効にできるか" を推測しづらい。
4. **operator interface**: ログ channel 名 `AYAStream` は debug logging の引数として既に文書化されているため、変更は破壊的。
5. **設定キー数**: 12個。XML 側は機械的置換で済むが、`gSavedSettings.getXxx("AYAStream...")` の C++ 参照が約 20箇所あり、ここは慎重に。

---

## 3. 命名方針 (TBD — r5 着手時に決定)

候補:
- (a) UI に合わせる (`[3dstream:...]`、`Stream3D*` 等)
- (b) `[aya:xxx:...]` 名前空間プレフィックス
- (c) 現状維持 + ドキュメント化のみ

→ §2 の調査結果を踏まえて AYA と方針合意してから記入する。

---

## 4. リネーム対応表 (TBD)

§3 の方針確定後に旧→新の対応表を埋める。

---

## 5. 後方互換戦略 (TBD)

- Description タグ: 旧 `[ayastream:...]` をエイリアスとして残すか、サンセット期間を設けるか
- 設定キー: 旧キーを読んで新キーへ migration するか、deprecated 警告のみ出すか
- ログ channel: 変更時の operator 周知方法

---

## 6. 影響ファイル一覧 (TBD)

§4 確定後に変更対象ファイルとその種別 (置換/手動/コメント) を整理。

---

## 7. テスト観点 (TBD)

- 既存 `[ayastream:...]` タグでの動作維持
- 新タグでの動作確認
- 設定キー migration の双方向動作
- ログ channel 切り替え後の `--logdebug` 動作

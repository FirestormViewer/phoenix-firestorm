# AYAstream: プリム発音ストリーミングの空間音響化 実装記録

**作成日**: 2026-05-01
**対象**: AYAstorm `feature/aya-positional-stream` ブランチ
**仕様**: `doc/spec_positional_stream_audio.md`

---

## 1. ゴール

リンクセット + Description タグ (`[ayastream:...]` / `[ayastream-stereo:...]`) で指定された URL を、プリムの 3D 位置に固定して FMOD 3D サウンドとして再生する。LSL からタグを書き換えれば追従し、プリムが破棄されればストリームも止まる。ステレオ配信は L/R を別々の子プリムに割り当てて空間分離する。

- パーセル BGM とは独立に動作し、`AYAStreamVolumeMaster` で全体音量を制御
- ストリームが落ちたら自動再接続 (5 秒 × 3 回 → binding 解放)
- プリム最大同時数を `AYAStreamMaxConcurrent` で抑制 (既定 4)
- 全機能を 1 トグル (`AYAStreamEnabled`) で停止可能

---

## 2. 実装内容

### 2.1 M1: モノラル 3D ストリーム最小実装

**ファイル**: `indra/llaudio/llpositionalstream.{h,cpp}`, `indra/llaudio/CMakeLists.txt`, `indra/newview/llappviewer.cpp`, `app_settings/settings.xml`
**commit**: `0a4a68f4fe`, `1f5b3b202b`

- `LLPositionalStream` を新規追加。FMOD `createStream` (NONBLOCKING) で HTTP ソースを開き、`FMOD_3D` モードの Channel に貼って `set3DAttributes()` で位置を流し込む
- デバッグ用設定 `AYAStreamDebugUrl` / `AYAStreamDebugPlay` でトグル動作確認 (アバター前方 5m に固定アンカー)
- Rolloff 既定値を後追いで 1m / 20m に修正 (`1f5b3b202b`)

### 2.2 M2: ロールオフ live 調整

**commit**: `164e073034`

- `AYAStreamRolloffMin` / `AYAStreamRolloffMax` を追加し、信号で次フレームから全アクティブストリームに反映

### 2.3 M3a: Description タグ binding + リスナー向き

**ファイル**: `indra/newview/llpositionalstreammgr.{h,cpp}`, ObjectProperties ハンドラ, `indra/newview/llvieweraudio.cpp`
**commit**: `33a53dd2df`, `25c44f9407`

仕様書の M3 (フローター UI) と M4 (Description タグスキャン) は、現実の利用形態が「LSL でタグ書き換え → 自動拾い上げ」が主だったためフローター UI を省略し、Description タグ binding だけを `M3a` として実装:

- `LLPositionalStreamMgr` をシングルトンで追加。`mBindings: map<UUID, Binding>` で prim→stream を管理
- `[ayastream:{url=...},{min=...},{max=...}]` を `parseTag()` でパース
- ObjectProperties / ObjectPropertiesFamily 受信時に `onObjectPropertiesReceived()` 経由で binding 評価
- 関連修正として、ear location = avatar の場合のリスナー向きをカメラではなくアバターの向きで揃えるよう変更 (`25c44f9407`)

### 2.4 M3b: アクティブポーリング

**commit**: `a5b6935e40`

ObjectProperties は通常 SELECT/CAMERA イベントで来るため、LSL `llSetObjectDesc` で後追いに変えたタグを取りこぼす。範囲内プリムを round-robin で `RequestObjectPropertiesFamily` するスキャンを追加:

- `AYAStreamMaxDistance` (既定 64m) 内のプリムを対象
- `AYAStreamPollInterval` (既定 30s) で同じ prim を再ポール
- `gObjectList` に対する `mPollCursor` で 1 パスあたりの予算を分散

### 2.5 M5-a: ステレオ PCM 抽出 spike

**ファイル**: `indra/llaudio/llpositionalstreamstereo.{h,cpp}`
**commit**: `94e1265f16`

L/R 分離の前段として、HTTP ソースから PCM を吸い出して 2 つの `OPENUSER` 3D モノラルサウンドに供給する経路を立ち上げ。`AYAStreamDebugStereoPlay` でトグル。M5-a 時点では L/R に同じダウンミックス信号を流して経路の死活を確認。

### 2.6 M5-b: 真の L/R デインタリーブ + サンプル同期

**commit**: `3cc28cb0da`

ソースストリームのインタリーブ PCM を L/R に正しく分離。両 OPENUSER の書き込みポインタを共有するリングバッファ越しに同期させ、サンプル単位で位相が揃うように変更。

### 2.7 M6: ステレオ prim-pair binding

**commit**: `b0d7714f3c`

Description タグ `[ayastream-stereo:{url=...},{l=1},{r=2}]` をパース (`parseStereoTag()`)。ルートプリムに対するリンク番号で L/R 子プリムを解決し、それぞれの世界座標に L/R チャンネルを送り込む。リンクセット構成変更にも各フレームで追従。

### 2.8 M7: マスター音量 + 自動再接続

**ファイル**: `llpositionalstream.{h,cpp}`, `llpositionalstreamstereo.{h,cpp}`, `llpositionalstreammgr.{h,cpp}`, `llviewercontrol.cpp`
**commit**: `b5b7560e05`

- `AYAStreamVolumeMaster` (既定 0.5) を追加。signal 経由で全アクティブストリームに次フレーム適用
- `LLPositionalStream` に `enum class State { Idle, Opening, Playing, Failed }` と `isFailed()` を追加
- 中断検出を 3 シグナル化: `Channel::isPlaying()` false / `getOpenState()` ERROR / `starving` フラグの 3s デバウンス
- 失敗時は manager 側で `AYAStreamReconnectAttempts` 回 (既定 3, 5s 間隔) 自動再接続。使い切ったら binding を解放

### 2.9 M8 step 1: kill switch / scan toggle / cap

**commit**: `a415d94f00`

仕様 §9 の未充足キーを追加し、ハンドラを実装:

- `AYAStreamEnabled` (既定 true): false で `shutdownAll()` 即時実行 (debug を含む全停止)、polling/ObjectProperties 受信も無視。再有効化は M3b polling が次パスで再発見
- `AYAStreamDescriptionScan` (既定 true): false で `shutdownPrimBindings()` (debug は残す)
- `AYAStreamMaxConcurrent` (既定 4): mono+stereo 合算で上限超えた新規 binding は拒否 (既存は維持、追い出さない)

### 2.10 M8 step 2: 通知 UI

**commit**: `c8f9583c0f`

3 イベントを `ChatSystemMessageTip` でトースト + Nearby Chat に出す:

- 初回再生成功 (`now playing: <url>`)
- 再接続全失敗 (`stream gave up after N reconnect attempts: <url>`)
- 同時数上限到達 (`max concurrent streams (N) reached...`)

スパム防止: `Binding::notified_played` で初回のみ、`mCapNotified` を edge-trigger 化 (cap を割り込んだら再 arm)。

**LL Chat 対応**: Firestorm が `LLHandlerUtil::logToNearbyChat()` を `FSFloaterNearbyChat` 専用に書き換えていたため、`AYAChatWindowStyle=2` のユーザーはトーストしか見えなかった。`notifyAYAStream()` 内で `ayastorm_is_ll_style()` を判定し、LL の場合は `LLFloaterIMNearbyChat::addMessage()` にも同じ行を流すよう拡張。

### 2.11 M8 step 3: 実機チューニング判定

**commit**: `e6713cb2af`

仕様 §11 R2/R4/R5 を「現状値で確定」として spec に追記。実装での既定値 (rolloff 1m/20m, master 0.5, max concurrent 4) で利用上の破綻が出なかったため、追加チューニング・新規 knob は見送り。

### 2.12 Post-M9: Preferences / Volume Pulldown UI 統合

**ファイル**: `panel_preferences_sound.xml`, `panel_volume_pulldown.xml`, `floater_fs_volume_controls.xml`, metaharper skin override
**commit**: `46e11fc970`, `3d80cd6be1`

仕様 §8.1 のフローター UI は実装せず、既存 Sound 系 UI に最小限の露出を追加した:

- **Preferences > Sound & Media**: Voice 行の下に「3D Stream」slider + Enabled checkbox を追加。Music/Media/Voice と同じレイアウト (mute ボタンは省略 — Enabled が hard mute 相当)
- **スピーカーアイコン pulldown** (`panel_volume_pulldown.xml`): 末尾に同じ「3D Stream」slider + enable checkbox を追加。`floater_fs_volume_controls.xml` 側も含めパネル高さを調整 (155 → 176, 外側 floater 170 → 191)。metaharper skin override にも同じ行を反映
- **Ear-location ラベル**: 既存「Hear media and sounds from:」を「Hear media and sounds from (3D Stream):」に変更し、`MediaSoundsEarLocation` ラジオが AYAstream のリスナー位置にも効くことを明示

C++ 側の追加コードは不要 — `AYAStreamVolumeMaster` / `AYAStreamEnabled` は M7/M8 段階で既に signal listener に繋がっており、新規 control_name バインドだけで動作する。

---

## 3. 設定キー一覧

すべて `app_settings/settings.xml` に登録、live 反映。

| Key | Type | 既定 | 用途 |
|---|---|---|---|
| `AYAStreamEnabled` | Boolean | true | M8 マスターキルスイッチ |
| `AYAStreamDescriptionScan` | Boolean | true | プリムタグスキャン on/off (debug は別) |
| `AYAStreamMaxConcurrent` | S32 | 4 | mono+stereo 合算の同時 binding 上限 (0=無制限) |
| `AYAStreamVolumeMaster` | F32 | 0.5 | 全アクティブストリームのマスター音量 |
| `AYAStreamRolloffMin` | F32 | 1.0 | FMOD 3D rolloff near (m) |
| `AYAStreamRolloffMax` | F32 | 20.0 | FMOD 3D rolloff far (m) |
| `AYAStreamMaxDistance` | F32 | 64.0 | M3b ポーリング対象とする最大距離 |
| `AYAStreamPollInterval` | F32 | 30.0 | M3b 同一 prim 再ポーリング間隔 (秒, 0=停止) |
| `AYAStreamReconnectAttempts` | S32 | 3 | 自動再接続最大試行回数 (0=無効, 1 回 5s 待機) |
| `AYAStreamDebugUrl` | String | "" | デバッグ用 HTTP URL |
| `AYAStreamDebugPlay` | Boolean | false | デバッグ mono ストリーム start/stop |
| `AYAStreamDebugStereoPlay` | Boolean | false | デバッグ stereo ストリーム start/stop |

---

## 4. リスク追跡 (spec §11)

| ID | 状態 | メモ |
|---|---|---|
| R1. PCM DSP コールバック安定性 | 解決 | M1/M5 で実機検証、FMOD Studio 2.02 系で問題なし |
| R2. ステレオ位相違和感 | 見送り (M8) | 既定 rolloff で破綻ケース未確認。報告ベースで再着手 |
| R3. SHOUTcast 互換性 | FMOD 依存 | `createStream` の URL 対応範囲に準ずる |
| R4. パーセル BGM 干渉 | 解決 (M8) | 既定 0.5 を維持、ユーザースライダー調整可 |
| R5. 高頻度移動時の負荷 | 解決 (M8) | 既定 cap=4 で問題なし、追加スロットリングなし |
| R6. CEF (MoaP) 二重再生 | 範囲外 | M3 段階で「タグ優先 + MoaP 音声ミュート」を方針化 |

---

## 5. commit 一覧

| commit | 内容 |
|---|---|
| `0a4a68f4fe` | PositionalStream: add 3D HTTP audio stream playback (M1) |
| `1f5b3b202b` | PositionalStream: restore FMOD_3D mode and tune default max rolloff |
| `164e073034` | PositionalStream: live-adjustable rolloff settings (M2) |
| `33a53dd2df` | PositionalStream: bind streams to prim Description tags (M3a) |
| `25c44f9407` | ViewerAudio: orient 3D listener by avatar when ear location = avatar |
| `a5b6935e40` | PositionalStream: active polling for LSL-driven tag updates (M3b) |
| `94e1265f16` | PositionalStream: stereo PCM extraction spike (M5-a) |
| `3cc28cb0da` | PositionalStream: true L/R deinterleave + sample-accurate sync (M5-b) |
| `b0d7714f3c` | PositionalStream: stereo prim-pair binding via Description tag (M6) |
| `b5b7560e05` | PositionalStream: master volume + auto-reconnect on stream drop (M7) |
| `a415d94f00` | PositionalStream: kill switch + scan toggle + concurrent cap (M8 step 1) |
| `c8f9583c0f` | PositionalStream: toast + nearby chat notifications (M8 step 2) |
| `e6713cb2af` | spec: close R2/R4/R5 with M8 in-world judgments (M8 step 3) |
| `46e11fc970` | Sound prefs: expose AYAstream as "3D Stream" volume row |
| `3d80cd6be1` | Sound prefs: hint that ear-location radio also covers 3D Stream |

---

## 6. 動作確認済み項目

- mono タグ付プリムを rez → 自動接続して 3D 定位
- LSL `llSetObjectDesc` でタグを後付け → 30 秒以内に自動 binding
- ステレオタグ付リンクセットで L/R が左右に分離
- `AYAStreamVolumeMaster` を 0→1 に動かすと即時反映
- 配信サーバーを止める → 5s × 3 回再接続を試み、失敗で binding drop + 通知
- `AYAStreamEnabled = false` で全ストリームが即時停止 (FMOD バッファドレイン分の数十 ms 残響あり)
- `AYAStreamDescriptionScan = false` で prim binding のみ落ち、debug は維持
- `AYAStreamMaxConcurrent = 1` で 2 個目のタグ付プリムを拒否 + 通知、解放後に再 bind
- 通知が FS Nearby Chat / LL Nearby Chat の両方に出る
- Preferences > Sound & Media と speaker icon pulldown の「3D Stream」スライダ・Enabled チェックが live に反映される
- 「Hear media and sounds from (3D Stream):」ラジオを Camera / Avatar に切り替えると AYAstream の定位中心も即時切替

---

## 7. 残課題・スコープ外

- 専用フローター UI (`floater_ayastream`) は仕様 §8.1 に記述があるが未実装。代わりに既存 Sound Preferences と speaker icon pulldown に「3D Stream」行を追加して必要最小限の露出を確保した (§2.12)。右クリックメニュー追加 (§8.2) は引き続き未着手
- ICY メタデータ (曲タイトル等) のフローター表示 (spec §12)
- HTTPS Icecast の挙動差は FMOD 側に依存。新規サーバとぶつかった場合は個別検証
- `LLPositionalStreamMgr::pollObjectPropertiesFamily()` は `AYAStreamMaxDistance` 内の全プリムを round-robin する単純実装。タグなしプリム比率が高い大型 SIM では最初の発見までタイムラグが出る可能性あり

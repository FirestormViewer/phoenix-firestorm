# プリム発音ストリーミングの空間音響化 仕様書

## 1. 概要

プリム(オブジェクト)から SHOUTcast / Icecast 等の HTTP オーディオストリームを再生し、3D ワールド内でリスナー(カメラ)との位置関係に応じた**方向定位・距離減衰**を伴うように再生する機能を追加する。

最終ゴールは「ステレオ音源を 2 つのプリムに L/R 分離して再生する」こと。中間ゴールとして単一プリムからのモノラル化 3D 再生を先行実装する。

## 2. 背景

現状の Firestorm/Second Life 標準実装:

| 種別 | 空間音響 | 備考 |
|---|---|---|
| LSL `llPlaySound` 等の効果音 | ✓ FMOD 3D | `llaudioengine_fmodstudio.cpp:747-778` で `set3DAttributes()` |
| Media-on-a-Prim (MoaP) の音声 | △ 距離減衰のみ | `LLViewerMediaImpl::updateVolume()` (`llviewermedia.cpp:2205-2243`)。CEF プロセス内で OS ミキサーへ直行 |
| パーセル単位の音楽ストリーム | ✗ 完全 2D | `LLStreamingAudioInterface` (`llstreamingaudio.h`) に位置 API なし |

ストリームソース(HTTP オーディオ)を 3D 定位再生する経路は存在しない。本仕様で追加する。

## 3. 用語

- **ストリーム**: 本仕様では SHOUTcast / Icecast 互換の HTTP オーディオストリーム(MP3 / AAC)を指す。
- **ホストプリム / 発音プリム**: ストリーム音源として扱われる 3D ワールド上のプリム。
- **ペアプリム**: ステレオ L/R 分離再生における、L 側プリムと R 側プリムの 2 つ 1 組のホストプリム。
- **3D チャンネル**: FMOD Studio の `Channel` のうち `FMOD_3D` フラグが付与されたもの。`set3DAttributes()` で位置・速度を設定できる。

## 4. ゴール / 非ゴール

### ゴール

- G1. プリムの世界座標から発音されているように聞こえるストリーム再生。
- G2. リスナーが移動・回転すると音の方向と距離感が追従する。
- G3. ステレオ音源の L/R を別プリムに割り当てて、空間にステレオ像を構築できる。
- G4. パーセル BGM とは独立に動作し、共存できる。
- G5. SL サーバー側変更を要さない(ビューア単体改造で完結)。

### 非ゴール

- NG1. HRTF / バイノーラル処理(出力側)。FMOD 既存の 3D パン(等価強度ステレオ)で十分とする。
- NG2. 動画(MoaP)音声の空間化。本仕様はオーディオ専用ストリームのみ対象。
- NG3. LSL 経由でのストリーム指定 API 追加。
- NG4. 任意数の同時ストリーム。最大同時数は実装で上限を設ける(初期値 4)。

## 5. 機能仕様

### 5.1 フェーズ 1: 単一プリム 3D モノラル再生

- ホストプリムを 1 つ指定し、ストリーム URL を関連付ける。
- ストリームをモノラル化(L/R ミックス)して FMOD の 3D チャンネルに流す。
- リスナー位置・プリム位置に基づいて FMOD が距離減衰とパンを自動計算する。
- ホストプリムが移動・スケーリング・削除されたら追従、または停止する。

### 5.2 フェーズ 2: ステレオ L/R 2 プリム分離再生

- L プリム / R プリムの 2 つを 1 組として登録する(紐付け方式は §6)。
- 単一の HTTP デコーダから取得した PCM をデインターリーブし、L サンプルを L プリムの 3D モノラルチャンネルに、R サンプルを R プリムの 3D モノラルチャンネルに流す。
- 両チャンネルはサンプル単位で同期する(同一バッファから供給するため自動的に揃う)。
- ペア解除時は両チャンネルとも停止。

### 5.3 共通動作

- 再生開始/停止: ユーザー操作(UI)またはプリムの存在状態変化で制御。
- 音量: グローバル音量(ストリーム系)× 個別音量(プリム単位)。
- 距離減衰モデル: FMOD の `FMOD_3D_LINEARSQUAREROLLOFF` を初期値とする(MoaP の現行挙動に近い)。Min/Max 距離はパーセル MoaP 設定(`MediaRollOffMin` / `MediaRollOffMax`)を流用または別キーで持つ。
- 再接続: ストリーム切断時は最大 N 回(初期値 3)まで自動再接続を試行。
- メタデータ: ICY タイトル取得は将来拡張(本仕様では取得のみ可能で UI には出さない)。

## 6. プリムとストリームの紐付け仕様

### 採用案: リンクセット + Description タグ方式

- ホストプリム判定:
  - プリムの **Description フィールド** に特定タグを記載する。
    - 単一プリム: `[ayastream:URL]`
    - ステレオペア: ルートプリムに `[ayastream-stereo:URL]` を記載し、リンクセット内のリンク番号で L/R を決定。
      - リンク番号 1 (ルート) = L、リンク番号 2 (最初の子) = R を既定とする。
      - 明示指定する場合は `[ayastream-stereo:URL|L=2|R=3]` のように書ける。
- 利点:
  - LSL 不要で完結する。
  - 既存の建築ワークフローに沿う(Description は誰でも編集可)。
  - リンクセット移動でペアが自動で一緒に動く。
- 制限:
  - Description は他用途と衝突しうる → タグプレフィックスを `[ayastream` で固定し誤検出を防ぐ。
  - Description は 127 byte 上限 → URL 長が長い場合 UI 入力経由(後述)に切り替え。

### 補助案: 右クリックメニュー / フローター UI

- ペア指定や URL 上書きを Description に書きづらいケース向け。
- 「このプリムをストリームソースに設定」メニューから URL 入力フローター起動。
- 設定はビューアローカル設定(プリム UUID キー)に保存し、Description タグより優先する。

## 7. 技術設計

### 7.1 全体構成

```
              ┌─────────────────────────────┐
              │ HTTP Stream (SHOUTcast)     │
              └──────────────┬──────────────┘
                             │
                ┌────────────▼────────────┐
                │ FMOD createStream(2D)   │  ← 1 デコーダのみ
                │ (ステレオ PCM 出力)      │
                └────────────┬────────────┘
                             │ DSP read callback
                ┌────────────▼────────────┐
                │ Deinterleave & Buffer   │
                │ (L / R リングバッファ)   │
                └──────┬──────────┬───────┘
                       │          │
        ┌──────────────▼──┐  ┌────▼─────────────┐
        │ 3D Mono Sound L │  │ 3D Mono Sound R  │  ← FMOD_OPENUSER + FMOD_3D
        │ (pcmread cb)    │  │ (pcmread cb)     │
        └──────────┬──────┘  └────┬─────────────┘
                   │              │
        ┌──────────▼──┐    ┌──────▼──────┐
        │ Channel L   │    │ Channel R   │  set3DAttributes(prim位置)
        └──────────┬──┘    └──────┬──────┘
                   └──────┬───────┘
                          ▼
                    FMOD 3D Listener
                  (既存の viewer リスナー)
```

フェーズ 1(モノラル)の場合は L/R をミックスして単一の 3D Mono Sound に流すだけ。デインターリーブは行わない。

### 7.2 新規モジュール

新規ファイルを `indra/llaudio/` 配下に追加する。

- `llpositionalstream.h` / `llpositionalstream.cpp`
  - クラス `LLPositionalStream`
    - 1 ストリーム = 1 インスタンス
    - 役割: HTTP ストリーム取得(FMOD 委譲)、PCM 取り出し、L/R 分離、3D Sound への供給。
    - 公開 API:
      - `bool start(const std::string& url, EPairingMode mode)`
      - `void stop()`
      - `void setHostPrim(const LLUUID& prim_id)` (モノラル時)
      - `void setHostPrimPair(const LLUUID& l_prim, const LLUUID& r_prim)` (ステレオ時)
      - `void setVolume(F32 v)`
      - `void update()` (毎フレーム呼び出し、プリム位置から `set3DAttributes()`)
- `llpositionalstreammgr.h` / `.cpp`
  - シングルトン `LLPositionalStreamMgr`
  - 役割: 全ストリームの登録/解除、フレーム更新、上限管理、Description タグスキャン連携。

### 7.3 既存コードへの統合点

| 統合箇所 | ファイル | 内容 |
|---|---|---|
| FMOD バックエンド | `indra/llaudio/llaudioengine_fmodstudio.cpp` | `LLPositionalStream` から FMOD `System*` を取得するためのアクセサ追加 |
| フレーム更新 | `indra/newview/llappviewer.cpp` (idle ループ) | `LLPositionalStreamMgr::instance().update()` を毎フレーム呼ぶ |
| プリム生成/破棄/移動の検知 | `indra/newview/llviewerobject.cpp` および `LLViewerObjectList` | プリムが範囲外/削除された際にストリーム停止 |
| Description スキャン | `LLViewerObject` の Description 変更通知 | タグ検出して `LLPositionalStreamMgr` に登録/更新/解除 |
| UI フローター | `indra/newview/skins/default/xui/en/floater_ayastream.xml` 新規 | URL 入力、ペア指定、再生/停止、音量 |
| 設定キー | `indra/newview/app_settings/settings.xml` | 後述 §9 を追加 |

### 7.4 リスナー連携

既存の FMOD 3D リスナー(LSL サウンド用)が `set3DListenerAttributes()` で毎フレーム更新されている。本機能はこの同じ FMOD `System` 上のリスナーを共有するため、**リスナー更新コードに変更は不要**。3D Sound を同 System に作成すれば自動で同じ座標系で計算される。

### 7.5 同期の扱い

ステレオ分離時、L 側 Sound と R 側 Sound はそれぞれ独立した FMOD Channel で再生されるが、**同一の PCM バッファ(リングバッファ)から供給**するためサンプル同期は維持される。各 Channel の遅延差は FMOD のミックス側で発生しうるが、同 System 内では実用上無視できるレベル(< 1ms)。

### 7.6 デコーダ単一性

「同一 URL を 2 回開く」アンチパターンを避けるため、`LLPositionalStream` は内部で **1 本の `FMOD::Sound* (FMOD_2D, FMOD_CREATESTREAM)`** を保持し、これを発音には使わず、DSP read callback で PCM のみ吸い出す。発音は別途生成した `FMOD_OPENUSER` の Sound から行う。

**実装上の選択 (M5-a 以降):** カスタム DSP を挟まず、`FMOD::Sound::readData()` をメインスレッドから毎フレーム呼んでソース Sound から PCM を吸い出し、SPSC リングバッファ経由で `FMOD_OPENUSER` の `pcmreadcallback` (FMOD ミキサスレッド) に供給する。意味的にはカスタム DSP と等価で、実装が簡潔・FMOD バージョン差にも強い。詳細は `LLPositionalStreamStereo` (`indra/llaudio/llpositionalstreamstereo.{h,cpp}`)。

## 8. UI 仕様

### 8.1 フローター `floater_ayastream`

- 起動: メニュー「Avatar → AYAstorm → Positional Stream...」または右クリック「このプリムをストリームソースに...」。
- 内容:
  - ストリーム URL 入力欄
  - モード選択(Mono / Stereo Pair)
  - 対象プリム選択(現在選択中のプリムから自動取得 / クリアボタン)
  - 再生 / 停止ボタン
  - 音量スライダ
  - 状態表示(接続中 / 再生中 / 切断 / エラー)
  - アクティブストリーム一覧(URL / ホストプリム名 / 音量)

### 8.2 右クリックメニュー追加項目(将来)

将来的にプリムの右クリックメニューから個別 start/stop と種別指定ができるようにする(初期フェーズでは Description タグ編集が唯一の制御経路)。

- 「Play Stream Here」/「Stop Stream Here」(個別 start/stop)
- 「Set as Stream Source (Mono)」
- 「Set as Stream Pair Left」
- 「Set as Stream Pair Right」
- 「Clear Stream Source」

### 8.3 通知

- 再接続失敗時に通知バーへ警告。
- ストリーム上限到達時にエラートースト。

## 9. 設定項目 (settings.xml)

| キー | 型 | 既定値 | 説明 |
|---|---|---|---|
| `AYAStreamEnabled` | Boolean | true | 機能全体の有効化 |
| `AYAStreamMaxConcurrent` | S32 | 4 | 同時再生可能ストリーム数 |
| `AYAStreamRolloffMin` | F32 | 1.0 | 距離減衰開始距離(m) |
| `AYAStreamRolloffMax` | F32 | 20.0 | 距離減衰終了距離(m) |
| `AYAStreamVolumeMaster` | F32 | 0.5 | ストリーム種マスター音量 |
| `AYAStreamReconnectAttempts` | S32 | 3 | 切断時の再接続試行回数 |
| `AYAStreamDescriptionScan` | Boolean | true | Description タグからの自動検出を有効化 |

## 10. 工程

### マイルストーン

| M | 内容 | 期間目安 |
|---|---|---|
| M0 | 設計レビュー(本書)・アーキ確定 | 0.5 日 |
| M1 | `LLPositionalStream` モノラル経路の最小実装 + Debug Setting `AYAStreamDebugUrl` / `AYAStreamDebugPlay` でのトグル動作確認 | 1.5 日 |
| M2 | フレーム更新 / プリム位置追従 / プリム破棄ハンドリング | 0.5 日 |
| M3 | フローター UI + 単一プリムモノラル再生の手動操作対応 | 1 日 |
| M4 | Description タグスキャン + ローカル設定保存 | 0.5 日 |
| M5 | ステレオ分離(PCM デインターリーブ + 2 チャンネル供給) | 1.5 日 |
| M6 | ペア紐付け(リンクセット + Description) UI 対応 | 1 日 |
| M7 | 設定項目反映 / 音量・再接続・上限管理の仕上げ | 0.5 日 |
| M8 | 実機チューニング(ロールオフ距離・音量バランス・離間効果検証) | 1 日 |
| M9 | ドキュメント・テスト計画整備・PR | 0.5 日 |

合計: **8.5 日**目安(フェーズ 1 のみで停止する場合は M0〜M4 + M7 の **4 日**)。

### マイルストーン依存関係

```
M0 → M1 → M2 → M3 → M4 ─┐
                M2 → M5 → M6 ┴→ M7 → M8 → M9
```

M5(ステレオ分離)は M2 完了後ただちに着手可能で、M3/M4(UI)とは並行できる。

## 11. リスク / 未解決事項

- **R1. FMOD のストリームから PCM を吸い出す DSP コールバックの安定性**: FMOD Studio のバージョン依存で動作が変わる可能性。M1 で早期検証。
- **R2. ステレオ位相による違和感**: L/R プリムの間隔が広すぎる/近すぎると音場が崩れる。M8 で許容レンジを実測しガイドラインに追記。
- **R3. SHOUTcast サーバ互換性**: ICY ヘッダ非対応サーバや HTTPS Icecast の挙動差。FMOD `createStream` の URL 対応範囲に準ずる。
- **R4. パーセル BGM との干渉**: 同時再生時の音量バランス。`AYAStreamVolumeMaster` で個別調整可能だが、ユーザー体験上のデフォルト値は M8 で決定。
- **R5. プリム高頻度移動時の負荷**: 物理プリムや乗り物に紐づけた場合、毎フレームの `set3DAttributes()` 呼び出しが多数になる。最大同時数 4 で実用上は問題ない見込みだが M8 で確認。
- **R6. CEF (MoaP) の音声と二重再生**: 同じプリムで MoaP URL とストリームタグが両方有効な場合の優先順位。タグ優先 + MoaP 音声ミュートを既定とする(M3 で実装)。

## 12. 将来拡張(本仕様外)

- ICY メタデータ(曲タイトル等)のフローター表示。
- 4ch / 5.1ch ストリームを 4〜6 プリムに展開するサラウンドモード。
- LSL からのストリーム指定 API(SL サーバー側調整が必要なため除外)。
- ドップラー効果(`Channel::set3DAttributes` の vel 引数で原理的には可能、要実機検証)。
- パーセル BGM のフォールバック自動切り替え(ペア範囲外で BGM 復帰)。

## 13. 参考コード位置

- FMOD 3D 設定例: `indra/llaudio/llaudioengine_fmodstudio.cpp:747-778`
- 既存メディア音量計算(距離減衰の参考): `indra/newview/llviewermedia.cpp:2205-2243`
- ストリーミング基底インタフェース: `indra/llaudio/llstreamingaudio.h`
- パーセルストリーム呼び出し例: `indra/newview/llviewerparcelmedia.cpp`

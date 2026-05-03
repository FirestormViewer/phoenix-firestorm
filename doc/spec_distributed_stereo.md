# 3D Stream Stereo 分散記述拡張 仕様書 (r8)

> **対象**: AYAstorm `v7.2.5-ayastorm-r8` (想定)
> **前提**: `doc/spec_positional_stream_audio.md` (3D Stream 本体仕様、r5 改訂後)
> **前提 (実装基盤)**: `doc/spec_stream3d_decode_thread.md` (r7 で完了した decode thread 化)
> **背景文書**: `docs/ayastorm-positional-stream.md` (M1〜Post-M9 実装記録)

## 1. 目的とスコープ

r8 は `[3dstream-stereo:...]` タグ書式を「分散記述方式」に拡張し、1 ストリーム → N スピーカー (上限 16) で同期再生できるようにする機能拡張リリース。

主目的:
- 会場演出向けに同一音源を空間内の多点スピーカーから同時に鳴らせる
- 各スピーカーごとに L / R / M (sum-to-mono) のチャンネル割当と個別の `range` / `volume` を設定できる
- 親プリムの description 文字数制限 (LSL 127 byte) に縛られず、子プリム側に分散記述できる

非対象 (r8 では触らない):
- mono タグ `[3dstream:...]` の機能拡張 — 既存挙動のまま
- decode thread 化の追加最適化 — r7 で完了。N 並走分の thread 数は r7 の `Stream3DMaxConcurrent` 枠に収める
- mono 経路 (`LLPositionalStream`) の N 化 — 必要性が出た時点で別リリース
- 設定 UI に新 knob を追加 — 既定値変更のみで済むことを確認後、必要なら追加

---

## 2. 問題定義

### 2.1 現行制約

`[3dstream-stereo:URL|L=2|R=3]` (r5 以降) は L/R 2 点固定。会場運営の用途で「四隅・八方にスピーカーを置きたい」「ステージ前列と後列で同じ音を同期再生したい」といった多点配置のニーズに応えられない。

### 2.2 親プリム集約の限界

仮に書式を `[3dstream-stereo:URL|L=2|R=3|L=4|R=5|...]` のように拡張しても、LSL `llSetObjectDesc` の **127 byte 制限**に引っ掛かる。スピーカー数が増えるほど書ききれない。

### 2.3 物理的に正しい multi-speaker モデル

実世界の PA システムは「各スピーカー筐体は mono 出力、空間配置で全体のステレオ感を作る」のが基本。r5 の L/R 固定モデルは「2 個のスピーカーで音源のステレオを再現する」用途には適切だが、「1 つの音を空間に広く撒く」用途には合わない。後者には sum-to-mono を選択肢として用意したい。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. 1 ストリーム → 16 スピーカーまで同期再生できる (16 を実装上限、運用上限はこれより小さくしてよい)
- G2. 各スピーカーに L / R / M チャンネル割当を独立に設定できる
- G3. 各スピーカーに個別の `range` (rolloff 距離) と `volume` を設定できる、省略時はリンクセット既定値にフォールバック
- G4. スピーカーは子プリムの description で記述でき、親プリムは音源宣言だけで完結する
- G5. 既存 `[3dstream-stereo:{l:N}{r:N}{min:N}{max:N}]` 旧書式は完全互換で継続動作
- G6. 書式エラー / 構造エラー時はユーザーにわかる形で通知 (書式例つき)、同種エラーは 30 秒間は重複抑制

### 非ゴール

- NG1. 5.1ch / Ambisonic 等 mono/stereo 以外のソース対応 — 対象は mono / stereo HTTP 入力に限定
- NG2. スピーカー個別の delay / EQ / pan offset — v1 では位置と volume のみ
- NG3. リンクセットを跨いだスピーカー連携 — スピーカーは音源宣言と同一リンクセット内に限定
- NG4. ストリーム再生中のスピーカー追加/削除を「即時反映」保証 — ゆるい SLA (数秒以内) とする
- NG5. mono タグ `[3dstream:...]` の同様な拡張 — 必要が出た時点で別途検討

---

## 4. 設計

### 4.1 タグ書式

```
tag           ::= "[3dstream-stereo:" { field } "]"
field         ::= "{" key ":" value "}"
key           ::= "url" | "range" | "ch" | "volume"
                | "l" | "r" | "min" | "max"        ; 旧書式互換
value         ::= url-string | number | channel
channel       ::= "L" | "R" | "M"                   ; 大文字小文字無視 (lower 化)
```

ホワイトスペースは key/value の前後で trim。フィールド間の区切りは任意 (なし/カンマ/空白可)。これは r5 で `forEachKeyValue` が既に対応している挙動を踏襲する。

### 4.2 プリム役割の判定

各プリムの description を `[3dstream-stereo:...]` で抽出し、フィールド構成によって役割を決める:

| Description 内容 | 役割 |
|---|---|
| `{url:...}` を含む | **音源宣言** (root プリム限定。非 root で `{url:...}` を見つけた場合は無視 + ログ警告) |
| `{ch:L\|R\|M}` を含む | **スピーカー** (root / child いずれの prim でも可) |
| 両方を含む (root のみ) | 音源宣言 + 自身もスピーカー (1 タグに併記する) |
| どちらも含まず `{l:N}{r:N}` を含む | **旧書式** (`parseStereoTag` 既存パスへフォールバック) |

新書式 (`{ch:...}`) と旧書式 (`{l:N}{r:N}`) が同一 prim に混在した場合: **新書式を採用、旧書式フィールドは無視**。

### 4.3 フィールド仕様

#### トップレベル (root のみ有効)

| キー | 必須 | 範囲 | 意味 |
|---|---|---|---|
| `url` | ★ | 文字列 (非空) | ストリーム URL |
| `range` | - | F32 > 0 | リンクセット既定 rolloff (省略時 `Stream3DRolloffMax` 設定値) |

#### スピーカー宣言 (任意 prim)

| キー | 必須 | 範囲 | 意味 |
|---|---|---|---|
| `ch` | ★ | `L` / `R` / `M` | チャンネル割当 (大小無視) |
| `range` | - | F32 > 0 | このスピーカー個別の rolloff (root の `range` を上書き) |
| `volume` | - | 0.0 〜 1.0 | このスピーカーの音量倍率 (省略時 1.0) |

`min` (近接サチュレーション距離) は廃止。FMOD `set3DMinMaxDistance` の min 引数には内部固定 `1.0m` を渡す。

### 4.4 リンクセット集約

manager は以下のステップで 1 ストリーム = 1 binding を組み立てる:

```
1. 音源宣言を持つ root prim を起点とする
2. 同じリンクセット (root + 子) を走査
3. 各 prim の description を parseSpeakerFields() に通し、ch を持つものをスピーカーとして収集
4. 1 つも収集できなければ「構造エラー: スピーカー 0 個」として通知 + binding せず
5. 収集数が Stream3DStereoMaxSpeakers を超えた場合、前から N 個まで採用 + 警告
6. binding 確立後、以下を保持:
   - source: { url, root_range }
   - speakers: [ { uuid, ch, range_resolved, volume_resolved } ]
```

`range_resolved` は per-speaker `range` → root `range` → settings.xml `Stream3DRolloffMax` の優先順位でフォールバック。

### 4.5 同期再生実装

```
                    ┌────────────────────────────────────────┐
                    │  decode thread (r7 で確立済み)          │
                    │   1 デコーダ = 1 リングバッファ         │
                    │   readData → S16 → F32 → ring write     │
                    └─────────────────┬──────────────────────┘
                                      │ (SPSC ring)
                                      ▼
       ┌──────────────────────────────────────────────────────────┐
       │ FMOD mixer thread                                        │
       │   pcmReadCallback × N (各 speaker channel ごと)           │
       │     ch=L → ring の L サンプル抽出                         │
       │     ch=R → ring の R サンプル抽出                         │
       │     ch=M → (L+R)*0.5 を返す                              │
       └──────────────────────────────────────────────────────────┘
                                      │
                                      ▼
                  N 個の FMOD::Channel (各 speaker 1 ch)
                  - set3DAttributes(prim 位置)
                  - set3DMinMaxDistance(1.0, range_resolved)
                  - setVolume(volume_resolved)
                  - setDelay(同一 dspclock 起点) ← 同期の要
```

**同期メカニズム**: 全 channel に同一 `setDelay(start_dspclock, 0, true)` で起動 → サンプル単位の同期。1 デコーダから N 個の `pcmReadCallback` が同じ ring を読むため、ring の cap は N に依存させない (read 側は各 callback 独立に位置を進める)。

> **要設計確認**: 既存 ring buffer は SPSC 前提。N 個の reader が同じ ring から読む場合、ring を MPSC ではなく「ring に書き、各 reader は自分の read position を持つ multi-tail SPSC」モデルに変える必要がある。F3 で実装しつつ詰める。代替案: ring を 1 つに保ち、F3 内で **mixer-thread 側にスナップショットバッファを 1 段噛ませる**設計も検討余地。

> **5.1 への布石**: 本仕様では source は mono / stereo 限定だが、ring の **track 数は実装定数化せずパラメータ化**しておく (現状 N_track = 2、将来 5.1 ソース対応で N_track = 6)。これにより r10 で 5.1 venue placement モードに展開する際、ring 容量と mapping table の差し替えで済む。詳細は §9.1。

### 4.6 sum-to-mono 処理

`ch=M` の channel は `pcmReadCallback` 内で:

```cpp
for (i = 0 .. samples) {
    const F32 l = ring.readL(read_pos);
    const F32 r = ring.readR(read_pos);
    out[i] = (l + r) * 0.5f;
    advance(read_pos);
}
```

mono ソース時 (元から 1 ch) は L=R 扱いで同じ値を出力するため `M` 指定でも問題なし。

### 4.7 子プリム description 取得

子プリムの description を取るには `LLViewerObject::getDescription()` が利用できるが、これは `onObjectPropertiesReceived` が来ないと埋まらない。現状 manager は parcel 入域時に root のみ properties 要求している。

r8 では:
- 音源宣言 (root) を検出した時点で、その linkset 内の子プリム全ての properties を一括要求する (`LLSelectMgr::requestObjectPropertiesFamily`)
- 子プリムの description が後から到着した時点で linkset 再評価をトリガ
- 子プリムの description が変更された場合 (`onObjectPropertiesReceived` が来た時) は所属 root をたどって再評価
- link / unlink イベント (`LLViewerObject::setParent` 系) でも再評価

この経路追加は manager 側の改修主体で、`LLPositionalStreamStereo` クラス (r7 で decode thread 化済み) は触らない。

### 4.8 ライフサイクル

#### binding 構築時

```
LLPositionalStreamMgr::evaluateLinkset(root_id)
  ├─ root description から source 抽出 (url, range)
  ├─ url なし → 構造エラー (binding せず)
  ├─ linkset 走査 → speakers 抽出
  ├─ speakers 0 → 構造エラー
  ├─ speakers > Max → 先頭 N 個 + 警告
  └─ LLPositionalStreamMulti::start(url, speakers)
       ├─ createStream + decode thread 起動 (r7 と同じ)
       ├─ N 個の FMOD::Channel 作成、各 set3DAttributes / volume
       └─ 全 channel 同一 dspclock 起点で playSound
```

#### 平常時

```
manager update()
  ├─ 各 binding について speakers の prim 位置取得
  ├─ 各 channel に set3DAttributes 反映
  └─ stream->update() (r7 通り、main thread は軽量)
```

#### 構成変化時

```
onObjectPropertiesReceived(child_id, description)
  ├─ child の所属 root を resolve
  ├─ root が既知の binding を持っていれば evaluateLinkset(root_id) 再実行
  └─ ch / range / volume / リンク追加削除に応じて差分更新
       (URL 変更なし & 既存 channel 維持できる場合は最小差分)
       (URL 変更あり or speaker 数変動あり: 全 channel 再構築)
```

### 4.9 通知ポリシー

書式エラー / 構造エラーは `notifyStream3D()` で chat に出力。同一 prim・同一エラー種別は **30 秒間抑制**:

```cpp
struct ErrorThrottleKey {
    LLUUID prim_id;
    ErrorKind kind;  // BAD_CH / BAD_VOLUME / NO_URL_ON_ROOT / SPEAKER_OVER_LIMIT / ...
};
std::map<ErrorThrottleKey, F64> mLastNotifyTime;

void notifyIfNotThrottled(const ErrorThrottleKey& k, const std::string& msg) {
    F64 now = LLTimer::getTotalSeconds();
    auto it = mLastNotifyTime.find(k);
    if (it != mLastNotifyTime.end() && (now - it->second) < 30.0) {
        LL_INFOS("Stream3D") << "[suppressed] " << msg << LL_ENDL;
        return;
    }
    mLastNotifyTime[k] = now;
    notifyStream3D(msg);
}
```

メッセージ書式例:

```
3D Stream: タグ書式エラー (オブジェクト名: "MySpeaker")
  ch の値が L/R/M のいずれかである必要があります。
  例: [3dstream-stereo:{ch:L}{range:30}]
```

```
3D Stream: 構造エラー (リンクセット root: "MainStage")
  音源宣言 (url) が root にあるがスピーカー (ch) が見つかりません。
  各スピーカープリムに [3dstream-stereo:{ch:L|R|M}] を記載してください。
```

---

## 5. 受入条件

### 5.1 機能要件

| ID | 内容 | 確認方法 |
|---|---|---|
| F1 | 旧書式 `[3dstream-stereo:{l:2}{r:3}{min:5}{max:30}]` が r7 と同じく動作 | 旧書式プリム 1 個で再生確認 |
| F2 | 新書式 root + 子 N 個 (N=2,4,8,16) で再生 | 各 N で stereo source 再生、左右移動で減衰確認 |
| F3 | `ch=M` の sum-to-mono が L/R 平均値を出力 | テスト用 L=440Hz / R=880Hz 素材で M スピーカーの周波数解析 |
| F4 | per-speaker `range` 上書きが効く | 子 1 個だけ `range:50`、他 `range:10` で距離減衰の差を観察 |
| F5 | per-speaker `volume` が反映される | 子 4 個に volume 1.0 / 0.8 / 0.5 / 0.2 を設定し聴感確認 |
| F6 | speakers 17 個書いた場合に 16 個まで採用 + 警告通知 | 通知文言と再生本数を確認 |
| F7 | 子プリム description 後付けで反映 (LSL `llSetObjectDesc`) | 起動後に `ch:L` を子に書き込み、数秒以内に再評価 |
| F8 | リンク (link) / アンリンク (unlink) で binding 再評価 | 動的に link 操作してスピーカー追加/削除を確認 |
| F9 | 書式エラー時の通知に書式例が含まれる | `{ch:X}` (不正値) を書いて通知文確認 |
| F10 | 同一 prim 同種エラーは 30 秒抑制される | 同じ不正タグを連打して 1 件/30 秒に絞られることを確認 |

### 5.2 同期性

| 指標 | 計測方法 | 目標 |
|---|---|---|
| 全スピーカー間の位相ズレ | 同一素材を 4 スピーカーに割当、ステレオマイク(in-world recorder) で録音解析 | サンプル単位で同期 (ズレ ≤ 1 sample) |
| FMOD mixer の dropout | log の `Stream dropped`/starving 件数 | 5 分連続再生で 0 件 |
| CPU 使用率 (16 spk 時) | OS タスクマネージャ / ToolBox | r7 比 +5% 未満 (ring read が N 倍になる影響を見る) |

### 5.3 安定性

| 指標 | 目標 |
|---|---|
| `Stream3DEnabled` トグル × 50 回 | crash / hang なし、binding が正しく start/stop |
| リージョン切替 × 10 回 | 再評価が走り、新 region の linkset が正しく拾える |
| Viewer Quit 中の配信再生中状態 | r7 同等の shutdown 順序を維持、crash なし |
| 旧書式と新書式が混在する parcel | それぞれ独立に動作、相互干渉なし |

---

## 6. 設定項目

| キー | 既定値 | scope | 意味 |
|---|---|---|---|
| `Stream3DStereoMaxSpeakers` | 16 | 新規 | 1 ストリームあたりスピーカー数上限 |
| `Stream3DRolloffMax` | (現行値) | 既存 | 新書式の `range` 既定値としても流用 (key rename せず) |
| `Stream3DMaxConcurrent` | (現行値) | 既存 | 1 linkset = 1 ストリーム単位でカウント。N スピーカー = 1 枠 |

`Stream3DRolloffMin` は新書式では参照されない (内部固定 1.0m)。旧書式パス (`parseStereoTag`) では引き続き使う。

UI 追加は v1 では行わない。`Stream3DStereoMaxSpeakers` は debug settings からのみ変更可能。

---

## 7. 互換性

### 7.1 タグ

- 新書式 `{ch:...}` と旧書式 `{l:N}{r:N}` は別々のパスで処理、共存可能
- 旧書式 alias `[ayastream-stereo:...]` も継続サポート (parseStereoTag 既存パス)
- 同一 prim 内で新旧混在: 新書式優先、旧書式無視

### 7.2 ABI / 公開ヘッダ

`indra/llaudio/llpositionalstream*.{h,cpp}` の公開メソッド変更なし。新規:
- `indra/llaudio/llpositionalstreammulti.{h,cpp}` (新規クラス、F3 で導入)

manager (`llpositionalstreammgr.{h,cpp}`) には:
- `parseSourceFields` / `parseSpeakerFields` を追加
- `mDistributedBindings` を追加 (既存 `mBindings` / `mStereoBindings` と並列)
- 既存メソッドのシグネチャは変更しない

### 7.3 設定 / ログ

- 設定キー: `Stream3DStereoMaxSpeakers` 1 件追加、既存キーは rename / default 変更なし
- ログ channel: `Stream3D` 維持
- 通知文: 既存 `notifyStream3D` を共用、書式エラー / 構造エラー用テンプレを追加

### 7.4 FMOD API

r7 と同じ FMOD Studio 2.02 系。`Channel::setDelay` は dspclock ベースの起動同期に既存 API 使用。新規 FMOD 機能要求なし。

---

## 8. リスク

| ID | 内容 | 対策 |
|---|---|---|
| R1 | N スピーカー間の同期失敗 (位相ズレ → comb filter) | F3 で `setDelay` 同一起点を厳守。受入 5.2 で実測検証 |
| R2 | ring buffer が N 個の reader に対応できない | F3 で multi-tail SPSC または mixer 前段にスナップショット段を導入。設計時点で詰める |
| R3 | 子プリム description 取得タイミングの不整合 | manager 側で linkset 探索 + 再評価ループを組む。SLA は数秒以内 (NG4) と緩める |
| R4 | `Stream3DStereoMaxSpeakers` 超過時の挙動が不明瞭 | 「先頭 N 個採用 + 警告」を仕様化、通知文言で「超過した M 個は無視」を明示 |
| R5 | 旧書式と新書式の判定ミス (`{l:N}` を新書式と誤認等) | parseSpeakerFields は `ch` キーの存在で判定、`l`/`r` が数値 vs `ch` の値が文字、で完全に区別可能。ユニットテストで境界ケースを固める |
| R6 | 通知の重複抑制が破綻 (key 設計ミスで抑制効かない / 抑制しすぎる) | ErrorThrottleKey は `(prim_id, error_kind)` で固定、F4 でテスト |
| R7 | sum-to-mono の clipping (L+R が 0dB 超え) | 0.5f 倍で正規化、源音が 0dB に達していなければ問題なし。clipping 検出ログを decode thread に追加 |
| R8 | 16 spk 時の FMOD::Channel 数枯渇 | `Stream3DMaxConcurrent` 4 並走 × 16 spk = 64 channel 消費。FMOD 既定 channel 数 (512) には収まる範囲だが、F6 で実測 |
| R9 | r7 で確立した shutdown 不変条件 (decode thread join → FMOD release) を壊す | F3 で `LLPositionalStreamMulti` を `LLPositionalStreamStereo` のテンプレートとして実装、stop() / デストラクタ順序を踏襲 |

---

## 9. 将来拡張 (本仕様外)

### 9.1 5.1ch サラウンドソース対応 (r10 想定)

r8 の「1 ソース → N FMOD Channel」アーキテクチャは 5.1ch ソース配信の自然な土台になる。本節は r10 で着手することを前提とした方向性メモ。

#### 9.1.1 概念: venue placement モード

実 5.1 (映画基準・ITU-R BS.775) は **リスナーが固定位置にいる前提**で各 ch に方向感を埋め込む。SL のリスナーは自由視点なので「sweet spot」概念は適用できない。本拡張で目指すのは:

> **6 個のスピーカー prim を空間に固定配置し、5.1 ソースの各 ch をそれぞれに割り当てる「venue placement モード」**

これは PA (Public Address) 的な発想であり、シネマ的サラウンド体験の再現ではない。リスナーが空間を歩き回ると 5.1 mix の意図した定位は崩れるが、「会場で 5.1 ソースを多点再生する」という実用は満たせる。シネマ的体験を望む場合は head-tracked binaural / Ambisonic / Steam Audio 等の別経路で実現を検討する (本拡張のスコープ外)。

#### 9.1.2 codec / 配信スコープ

| 対象 | 配信側 | viewer 側 |
|---|---|---|
| **Opus surround** (Ogg/Opus, channel mapping family 1) | Icecast 2.4+ + opusenc / Liquidsoap | FMOD 2.02 内蔵デコーダで完結、追加実装ゼロ |
| **FLAC multichannel** | Icecast or HTTP 静的配信 | FMOD 内蔵デコーダで完結 |
| AAC 5.1 (ADTS over HTTP) | 理論上可、Linux ビルドで AAC デコーダ問題あり | 非対象 (r10 では扱わない) |
| AAC HLS (.m3u8) | HLS マニフェスト解析が新規実装 | 非対象 |
| AC-3 / E-AC-3 | Dolby ライセンス必要 | 非対象 |

**実用的には Icecast 2.4+ + Opus surround が事実上唯一の現実解**。viewer 側の codec 改修はゼロで済む。Shoutcast は Opus 非対応なので、5.1 化を選ぶ配信者は Icecast 移行が前提となる (= 配信側の社会的・運用コストが本拡張の最大の障壁)。

#### 9.1.3 必要な追加実装 (viewer)

1. **`ch` 値拡張**: r8 の `L` / `R` / `M` に **`FL` / `FR` / `C` / `LFE` / `SL` / `SR` を追加** (parser enum と検証ロジックの更新のみ)
2. **ソース ch 数検証**: `FMOD::Sound::getFormat()` で source ch 数を取得、書式の `ch` 値と整合チェック (§9.1.4 参照)
3. **N-track ring**: r8 で N_track をパラメータ化しておけば、5.1 で N_track = 6 に切り替えるだけ
4. **ch 値 → ring track の mapping table**: Opus (Vorbis 順) / FLAC (規格順) のチャンネル順序に従い、`{ch:FL}` を track 0、`{ch:FR}` を track 1、… のように対応付ける

mapping 例 (Opus channel mapping family 1 = Vorbis 順):
```
track 0: FL    →  {ch:FL}
track 1: C     →  {ch:C}
track 2: FR    →  {ch:FR}
track 3: SL    →  {ch:SL}
track 4: SR    →  {ch:SR}
track 5: LFE   →  {ch:LFE}
```

(Vorbis 順は L/C/R/SL/SR/LFE で WAV/AAC の L/R/C/LFE/SL/SR と異なる点に注意。codec 別の mapping table を実装側で吸収する。)

#### 9.1.4 設計選択 (r10 着手時に決める)

##### (1) ソース ch 数と書式の不整合ポリシー

書式に書かれた `ch` 値と実 source の ch 数が合わない場合の挙動:

| source | `ch=L/R/M` | `ch=FL/FR/C/LFE/SL/SR` |
|---|---|---|
| mono (1ch) | M のみ意味あり、L/R は M 同等扱い | 不整合 |
| stereo (2ch) | 通常 | 不整合 |
| 5.1 (6ch) | 不整合 (or 自動 downmix) | 通常 |

選択肢:
- **(a) 厳格**: 不整合は書式エラー、再生拒否 + 通知
- (b) 寛容: 自動 fallback (FL/FR → L/R、C → mono mix 等)

→ **推奨: (a) 厳格**。混乱防止と debug 容易性のため。source 切替時に黙って fallback すると「なぜ音が変わったか分からない」事態を招く。

##### (2) LFE (サブウーファー) の扱い

実 5.1 では LFE は「ローパスフィルタ済み、方向感なし、リスナー周辺常時」が基準。SL の自由視点モデルに合わせる際の選択肢:

- **(p) 普通のスピーカーと同等**: 3D 配置、距離減衰、特殊処理なし
- (q) 2D 化: `FMOD_2D` で配置、距離減衰なし、常時聞こえる
- (r) ローパスフィルタ追加: ~120Hz 以下のみ通す厳密モード

→ **推奨: (p)**。SL 内で「サブウーファー筐体」の prim を物理位置に置く運用が直感的。低域フィルタリングは配信側 (mix 段階) で済ませる前提とすれば、viewer 側に DSP を入れずに済む。

#### 9.1.5 r8 → r10 ロードマップ

```
r8 (本仕様)  : stereo source × N speakers (ch = L/R/M)
              ★ ring 実装は N-track 汎用化しておく (§4.5 の布石)

r9 (省略可)  : Opus / FLAC URL の動作確認・MIME 認識調整
              viewer 改修なしで動けば skip 可、F6 ベンチで確認するだけで済む可能性大

r10          : 5.1 venue placement
              - ch 値拡張 (FL/FR/C/LFE/SL/SR)
              - source ch 数検証ロジック
              - codec 別 channel mapping table
              - LFE 扱い決定
              - 仕様で「sweet spot 不在」を明文化
              - 配信側ガイドライン (Icecast + Opus surround 推奨)
```

r9 は実質「F6 ベンチの一部として Opus/FLAC URL を試す」だけで済む可能性が高く、形式的なリリースとしては不要かもしれない。r8 完成 → 直接 r10 着手も選択肢。

---

### 9.2 mono タグ `[3dstream:...]` の N 化

同じ分散記述方式を mono タグにも適用する。現状 mono は単一 prim 1 点で完結しているが、複数 prim から同じ mono ストリームを同期再生したい用途が出てきた時点で別リリースで対応。

### 9.3 per-speaker DSP

per-speaker delay (ディレイラインの再現)、簡易 EQ、pan offset などのスピーカー個別 DSP 挿入。実装には FMOD DSP 機能の活用と書式拡張が必要。

### 9.4 リンクセットを跨いだスピーカー連携

複数 root の binding を 1 グループに束ねて N speakers の上限を超える大規模配置を可能にする。あるいは、巨大会場で複数のリンクセットを横断して同期再生する用途。

### 9.5 スピーカー数上限の昇格

32 / 64 のニーズが出たら検討。`Stream3DStereoMaxSpeakers` の既定値変更で対応できるよう、設計時点でハードコード上限を持たせない。

### 9.6 head-tracked binaural / Ambisonic 拡張

5.1 venue placement (§9.1) の範囲外として、リスナーの位置・向きに追従する binaural / Ambisonic 再生。Steam Audio (Valve) のような外部 SDK を FMOD DSP プラグインとして組み込む案あり。codec / N-channel 経路とは独立した別軸の拡張。

---

## 10. 参考コード位置

| 対象 | パス |
|---|---|
| 現行 stereo 実装 (r7 decode thread 化済) | `indra/llaudio/llpositionalstreamstereo.{h,cpp}` |
| 現行 mono 実装 | `indra/llaudio/llpositionalstream.{h,cpp}` |
| manager (parse / binding / 再評価) | `indra/newview/llpositionalstreammgr.{h,cpp}` |
| 既存 `parseTag` / `parseStereoTag` | `indra/newview/llpositionalstreammgr.cpp:246-300` |
| 既存 `forEachKeyValue` | `indra/newview/llpositionalstreammgr.cpp:142-167` |
| 既存 `notifyStream3D` | `indra/newview/llpositionalstreammgr.cpp:198-218` |
| 子プリム properties 要求の参考 | `indra/newview/llselectmgr.cpp` (`requestObjectPropertiesFamily`) |
| 設定キー定義 | `indra/newview/app_settings/settings.xml` (`Stream3DRolloffMax` 周辺) |
| 設定 listener | `indra/newview/llviewercontrol.cpp` (`handleStream3DRolloffChanged`) |

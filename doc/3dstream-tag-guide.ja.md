# 3D Stream タグ書式ガイド

> AYAstorm の **3D Stream** 機能で、プリムから HTTP オーディオストリームを 3D 空間定位再生するためのタグ書式リファレンスです。
>
> このドキュメントは AYAstorm `r10` 時点の最終仕様に基づきます。今後 r11 / r12 以降で機能追加されたら、本書も追従更新します。

---

## 目次

1. [3D Stream とは](#1-3d-stream-とは)
2. [クイックスタート](#2-クイックスタート)
3. [用語](#3-用語)
4. [タグの全体像](#4-タグの全体像)
5. [モノラルタグ `[3dstream:...]`](#5-モノラルタグ-3dstream)
6. [分散ステレオ / 会場配置タグ `[3dstream-stereo:...]`](#6-分散ステレオ--会場配置タグ-3dstream-stereo)
7. [ch (チャンネル) 値リファレンス](#7-ch-チャンネル-値リファレンス)
8. [ソース ch 数 × タグ値 の互換マトリクス](#8-ソース-ch-数--タグ値-の互換マトリクス)
9. [配信側 (ソース URL の作り方)](#9-配信側-ソース-url-の作り方)
10. [viewer 側の設定](#10-viewer-側の設定)
11. [エラー通知 / 診断](#11-エラー通知--診断)
12. [トラブルシューティング](#12-トラブルシューティング)
13. [既知の制約 / 仕様上の注意](#13-既知の制約--仕様上の注意)
14. [関連ドキュメント / 内部仕様書](#14-関連ドキュメント--内部仕様書)

---

## 1. 3D Stream とは

Second Life 標準の Viewer では HTTP オーディオストリーム (SHOUTcast / Icecast) は **パーセル単位の BGM** として 2D 再生されるのみで、空間内のどこから鳴っているかという情報を持ちません。

AYAstorm の **3D Stream** 機能は、プリム (オブジェクト) を「スピーカー」として扱い、ストリーム音源を **そのプリムから鳴っているように 3D 定位再生** します。リスナー (カメラまたはアバター) が動くと、音の方向と距離感がリアルタイムに追従します。

主な用途:

- **ライブ会場の PA**: ステージ前にスピーカープリムを置き、配信中の音源をその位置から鳴らす
- **環境音**: 川辺・ジュークボックス・テレビ等のオブジェクトから対応する音を鳴らす
- **ステレオ配置 / マルチスピーカー会場**: 複数のプリムに L / R / モノラルを割り当ててステレオ感を空間に広げる
- **5.1ch ソースの会場展開**: 6 個のプリムに 5.1ch 各チャンネルを配置 (FL / FR / C / LFE / SL / SR)

操作はすべて **プリムの Description (説明文) フィールドにタグを書き込む** だけで完結します。LSL スクリプト不要、SL サーバー側変更不要、相手は AYAstorm を使っているユーザーのみ。他の Viewer (本家 Firestorm / 公式 LL Viewer 等) はこのタグを無視するため互換性問題は起きません。

---

## 2. クイックスタート

### 2.1 単一プリムから音を鳴らす (もっとも簡単な例)

任意のプリムを 1 つ作り、その **Description フィールド**に以下を書き込みます:

```
[3dstream:{url:http://example.com/stream.mp3}]
```

これだけで、そのプリムの位置から `http://example.com/stream.mp3` のストリームが 3D 定位で再生されます。プリムから離れると音が小さくなり、左右に動くと自然にパンが変わります。

### 2.2 ステレオ配置 (L / R を別プリムに分離)

ステレオ音源を 2 つのプリムに分けて、空間にステレオ感を作ります。

1. 親プリム (Root) と子プリム (Child) を 1 つずつリンクします (Ctrl+L)
2. **Root の Description**:
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]
   ```
3. **Root の Description に追記** (= Root 自身も L スピーカー):
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}{ch:L}]
   ```
4. **Child の Description**:
   ```
   [3dstream-stereo:{ch:R}]
   ```

これで Root から L、Child から R が鳴ります。

### 2.3 さらに詳しく

3 以降を読んでください。マルチスピーカー / 5.1ch / 細かいチューニング / 配信側のレシピ等を解説します。

---

## 3. 用語

| 用語 | 意味 |
|---|---|
| **ストリーム** | HTTP 経由で配信されるオーディオデータ (SHOUTcast / Icecast / 静的ファイル等)。MP3 / Vorbis / Opus / FLAC が主な対応 codec |
| **リンクセット** | SL の「リンク」(Ctrl+L) で 1 つにまとめられたプリム集合。ルート 1 個 + 子 N 個 |
| **ルートプリム** | リンクセットの親プリム。Build → Edit で「Selected linked」「Edit linked」OFF 時に最初に選択されるプリム |
| **子プリム** | ルート以外のリンクセット内プリム |
| **音源宣言** | `{url:...}` を含むタグを書いたプリム。本書での「どのストリームを鳴らすか」を宣言する役割。**ルートプリムにのみ書ける** (子プリムに書いても無視されます) |
| **スピーカープリム** | `{ch:...}` を含むタグを書いたプリム。実際に音を鳴らすプリム。**ルート/子プリムどちらでも可** |
| **binding** | 1 つのリンクセットに対して内部で組み立てられる「音源 → スピーカー群」の対応関係。1 リンクセット = 1 binding |
| **ch (チャンネル)** | スピーカープリムが受け持つ音声チャンネル。`L` / `R` / `M` (モノラル) のほか、5.1ch 用の `FL` / `FR` / `C` / `LFE` / `SL` / `SR` |
| **rolloff** | 距離減衰。リスナーがスピーカーから離れるにつれ音量が減衰する設定 |

---

## 4. タグの全体像

### 4.1 2 種類のタグ

| タグ | 接頭辞 | 用途 |
|---|---|---|
| **モノラルタグ** | `[3dstream:...]` | 単一プリムから 1 ストリームを再生 (最小構成) |
| **分散ステレオ / 会場配置タグ** | `[3dstream-stereo:...]` | リンクセットの複数プリムから 1 ストリームを同期再生 (ステレオ / マルチスピーカー / 5.1ch) |

### 4.2 旧プレフィクスのエイリアス

両タグとも、旧プレフィクス (`[ayastream:...]` / `[ayastream-stereo:...]`) を **恒久エイリアス** として受け付けます。r5 (2026-05) で `ayastream` → `3dstream` にリネームした際、それ以前に配置されたプリムを再編集せずに済むよう温存しています。新規記述は **`3dstream` 系を推奨**しますが、混在しても問題ありません。

```
[3dstream:{url:...}]              ← 推奨 (canonical)
[ayastream:{url:...}]             ← 旧式、互換受付

[3dstream-stereo:{url:...}{ch:L}] ← 推奨 (canonical)
[ayastream-stereo:{ch:L}]         ← 旧式、互換受付
```

### 4.3 共通の書式ルール

- **タグは Description のどこに書いてもよい**。前後に他の文章があっても無視されます (例: `お店の名前 [3dstream:{url:...}] 営業中` のように説明文と同居可)。
- フィールドは `{key:value}` 形式の集合で、フィールド間に区切り文字を要しません (空白・カンマ・連結いずれも可)。
- **キー名は大文字小文字を区別しません** (内部で小文字に正規化されます)。`{URL:...}` も `{url:...}` も同じ。
- **値の前後の空白は trim** されます。`{ url : http://example/  }` でも OK。
- **未知のキーは黙って無視** されます。例えば `{foo:bar}` は何の効果もありませんがエラーにもなりません。
- 1 つのプリムの Description に **同種タグを複数書いた場合は最初の 1 個**だけが採用されます。

### 4.4 SL の Description 制限 (127 byte)

LSL `llSetObjectDesc` が書ける Description は **127 byte 上限**です。日本語を含む場合は UTF-8 換算でこれを超えやすいので、**長い URL は短縮する** か、ルートに音源宣言だけ書いて子プリムに `{ch:...}` だけ書く分散方式 (§6) でやりくりします。

### 4.5 タグ反映のタイミング

- AYAstorm は **30 秒間隔で範囲内のプリム Description をポーリング** します (`Stream3DPollInterval` 設定)。
- LSL `llSetObjectDesc` で Description を変更すると、次のポーリングで再評価が走り反映されます (= 通常 5〜30 秒以内)。
- プリムを手動で右クリック → Edit → Description 変更した場合は、その編集確定で即座に再評価されます (Properties 通知経由)。
- リンク / アンリンク操作も再評価のトリガになります。

---

## 5. モノラルタグ `[3dstream:...]`

### 5.1 書式

```
[3dstream:{url:URL}{min:N}{max:N}]
```

または旧プレフィクス:

```
[ayastream:{url:URL}{min:N}{max:N}]
```

### 5.2 キー一覧

| キー | 必須 | 型 | 既定値 | 意味 |
|---|---|---|---|---|
| `url` | **必須** | 文字列 | — | ストリーム URL (`http://` / `https://`)。空文字列はエラー |
| `min` | 任意 | F32 (m) | `Stream3DRolloffMin` (1.0) | 距離減衰の **近距離** (このプリムからこの距離以内では音量 100%) |
| `max` | 任意 | F32 (m) | `Stream3DRolloffMax` (20.0) | 距離減衰の **遠距離** (この距離以上では音量 0%) |

距離減衰モデルは FMOD の `FMOD_3D_LINEARSQUAREROLLOFF` (リニア二乗ロールオフ)。`min` と `max` の間で滑らかに減衰します。

### 5.3 動作

- リンクセットの **どのプリム** でも書けます (ルートでも子でも)。書いたプリム自身がスピーカーとして音を鳴らします。
- ステレオ音源を渡した場合は **内部で L/R をミックスしてモノラル化** されます。
- 同じリンクセット内に `[3dstream-stereo:...]` も同時に書かれている場合、`[3dstream:...]` 側がそのプリムのスピーカー指定として優先されることはありません — 両方の binding 経路は独立に評価されます。同一プリムを両用途に使うのは推奨しません (動作未定義)。

### 5.4 例

#### 5.4.1 最小構成

```
[3dstream:{url:http://example.com/radio.mp3}]
```

`min` / `max` は省略され、設定既定値 (1m / 20m) で減衰します。

#### 5.4.2 距離をカスタムする

```
[3dstream:{url:http://example.com/radio.mp3}{min:2}{max:50}]
```

プリムから 2m 以内では音量最大、50m で消えます。広い屋外フィールドで遠くまで聞かせたいときに使います。

#### 5.4.3 説明文と併記

```
お店の BGM [3dstream:{url:http://radio.example.jp/8000/jazz}] よろしく
```

タグ前後に他の文があっても問題ありません。

---

## 6. 分散ステレオ / 会場配置タグ `[3dstream-stereo:...]`

### 6.1 書式

```
[3dstream-stereo:{url:URL}{range:N}{ch:CH}{volume:V}]
```

または旧プレフィクス:

```
[ayastream-stereo:...]
```

このタグは **リンクセット全体で 1 つのストリーム** を扱う形式です。ルートプリムが「どのストリームを鳴らすか」を宣言し、リンクセット内の各プリムが「自分はどのチャンネルを担当するか」を宣言します。

### 6.2 プリムの役割

各プリムはタグの中身によって以下の役割を持ちます:

| Description のフィールド | 役割 |
|---|---|
| `{url:...}` を含む | **音源宣言** (ルートプリム限定。子プリムで `{url:...}` を書くと無視されます) |
| `{ch:...}` を含む | **スピーカー** (ルート/子プリムどちらでも可) |
| 両方を含む (= ルートのみ) | 音源宣言 + 自身もスピーカーを兼ねる |
| どちらも含まない | 何もしない (binding 対象外) |

リンクセット内に **音源宣言 (= `{url}` を持つルート)** と **少なくとも 1 個のスピーカー (= `{ch}` を持つプリム)** が両方あって初めて再生開始されます。スピーカー 0 個では「構造エラー」となり、エラー通知が出ます (§11)。

### 6.3 キー一覧

#### 6.3.1 ルートプリムでのみ意味があるキー

| キー | 必須 | 型 | 既定値 | 意味 |
|---|---|---|---|---|
| `url` | **必須** | 文字列 | — | ストリーム URL。空文字列はエラー |
| `range` | 任意 | F32 (m) | `Stream3DRolloffMax` (20.0) | リンクセット内のスピーカーが個別に `range` を持たないときの既定減衰距離 |

#### 6.3.2 スピーカー宣言キー (任意のプリム)

| キー | 必須 | 型 | 既定値 | 意味 |
|---|---|---|---|---|
| `ch` | **必須** | 列挙値 | — | このプリムが受け持つチャンネル (詳細 §7) |
| `range` | 任意 | F32 (m) | ルートの `range` → `Stream3DRolloffMax` の順でフォールバック | このスピーカー個別の減衰距離 |
| `volume` | 任意 | F32 [0.0〜1.0] | 1.0 | このスピーカー個別の音量倍率 |

> **重要**: モノラルタグの `min` / `max` キーは分散ステレオタグ側では **無視** されます。分散ステレオでは近距離は内部固定で 1.0m、遠距離は `range` キー (または既定値 `Stream3DRolloffMax`) が使われます。

### 6.4 ルート 1 つ + 子 1 つ (基本のステレオペア)

最小のステレオ配置:

```
ルート Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:L}]

子 Description:
  [3dstream-stereo:{ch:R}]
```

ルート自身が L、子が R を担当します。ルートと子の **位置関係 (= リンク番号)** は再生に影響しません。空間内のどこに置くかが定位を決めます。

### 6.5 マルチスピーカー (4 個以上の配置)

同じステレオストリームを 4 つのスピーカーから鳴らす例 (会場の 4 隅):

```
ルート Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]

子 #1 Description:
  [3dstream-stereo:{ch:L}{range:50}]

子 #2 Description:
  [3dstream-stereo:{ch:R}{range:50}]

子 #3 Description:
  [3dstream-stereo:{ch:L}{volume:0.7}]

子 #4 Description:
  [3dstream-stereo:{ch:R}{volume:0.7}]
```

- ルートは音源宣言だけで、自分はスピーカーとして鳴りません (`{ch}` なし)
- 子 #1, #2 は L/R それぞれ近距離 50m、音量 100%
- 子 #3, #4 は同じ L/R を 70% で鳴らす (前段の補助)
- スピーカー数の上限は `Stream3DStereoMaxSpeakers` 設定で **既定 16 個まで** (§10)

### 6.6 5.1ch 会場配置 (6 プリム)

5.1ch ソース (Opus surround / FLAC 6ch) を 6 個のスピーカーに展開:

```
ルート Description:
  [3dstream-stereo:{url:http://example.com/test_5_1.flac}{range:30}]

FL プリム:  [3dstream-stereo:{ch:FL}]
FR プリム:  [3dstream-stereo:{ch:FR}]
C プリム:   [3dstream-stereo:{ch:C}]
LFE プリム: [3dstream-stereo:{ch:LFE}]
SL プリム:  [3dstream-stereo:{ch:SL}]
SR プリム:  [3dstream-stereo:{ch:SR}]
```

- 各プリムを物理的に「会場のスピーカー位置」に配置します (ステージ前 L/R、センター、サブウーファー、サラウンド L/R)
- LFE は他の 5 個と同等に扱われます (ローパスフィルタ等の特殊処理は viewer 側に入っていません。低域フィルタリングが必要なら配信側 mix で済ませてください)
- リスナーは「映画のスイートスポット」を持ちません (= SL の自由視点モデル)。会場を歩き回ると 5.1 mix の意図した定位は当然崩れます。シネマ的サラウンド体験ではなく、会場 PA 的な多点配置として運用してください

### 6.7 同じ ch を複数のプリムに割り当てる

`{ch:L}` を 2 つ以上のプリムに書くと、両プリムから同じ L チャンネルが鳴ります。配信会場で「ステージ前列の L」と「ステージ後列の L」のように複数台のスピーカーを置く用途に使えます。

逆にどの ch も書かれていないと「スピーカー 0 個」エラーになります。

### 6.8 ルート自身もスピーカー兼用

```
ルート Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:M}{range:25}]
```

このように 1 タグに `{url}` と `{ch}` を併記すると、ルートが音源宣言 + 自身も M (モノラル) スピーカーとして機能します。子プリムが 1 つもないシンプルな mono 構成にも使えます (`[3dstream:...]` と機能的にはほぼ等価)。

### 6.9 ルートプリムの判別方法

リンクセットを編集中、Build フローターの **Object** タブで「Selected」が表示されているプリムが選択中、その linkset の親 (= ルート) は通常 **最初に選択した状態でリンクされたプリム** です。

確認するもっとも確実な方法:
- Build → Edit → 「Edit linked」を OFF にしてプリムをクリック → そのリンクセットのルートが選択される
- LSL: `llGetLinkNumber()` でルートは `1` (子プリムが存在する場合)。子プリムなしの単一プリムは `0`

ルートと子の位置関係 (リンク番号 1, 2, 3, ...) は **3D Stream の再生に影響しません**。リンク番号で L/R を決めていた仕様は r5 までで廃止され、r8 以降は `{ch:...}` タグ宣言ベースになっています。

---

## 7. ch (チャンネル) 値リファレンス

`{ch:値}` には以下の 9 種類が指定できます。**大文字小文字は区別しません** (`{ch:l}` も `{ch:L}` も同じ)。

| 値 | 意味 | 主な用途 |
|---|---|---|
| `L` | 左チャンネル | ステレオの L 担当 |
| `R` | 右チャンネル | ステレオの R 担当 |
| `M` | モノラル (L+R の平均) | 「中央スピーカー」用途、または 1 点だけで鳴らす場合 |
| `FL` | Front Left | 5.1ch のフロント左 |
| `FR` | Front Right | 5.1ch のフロント右 |
| `C` | Center | 5.1ch のセンター |
| `LFE` | Low Frequency Effects (サブウーファー) | 5.1ch の低域 |
| `SL` | Surround Left | 5.1ch のサラウンド左 |
| `SR` | Surround Right | 5.1ch のサラウンド右 |

ソースの実 ch 数と書式の `ch` 値が合わない場合、「不整合」ではなく「自動フォールバック」が働きます (§8)。例えば 2ch ソースに `{ch:FL}` を書くと L が再生されます。

不正値 (`{ch:foo}` 等) は **書式エラー**として通知されます (§11)。

---

## 8. ソース ch 数 × タグ値 の互換マトリクス

実際にスピーカープリムから何が鳴るかは、**ソース URL のチャンネル数** と **書いた `ch` 値** の組み合わせで決まります。

### 8.1 互換マトリクス

| ソース | `{ch:L}` | `{ch:R}` | `{ch:M}` | `{ch:FL}` | `{ch:FR}` | `{ch:C}` | `{ch:LFE}` | `{ch:SL}` | `{ch:SR}` |
|---|---|---|---|---|---|---|---|---|---|
| **1ch (mono)** | M | M | M | M | M | M | 無音 | 無音 | 無音 |
| **2ch (stereo)** | L | R | (L+R)/2 | L | R | (L+R)/2 | 無音 | 無音 | 無音 |
| **6ch (5.1)** | BS.775 L | BS.775 R | (BS.775 L + R)/2 | FL | FR | C | LFE | SL | SR |

凡例:
- `L` / `R` / `FL` / `FR` / `C` / `LFE` / `SL` / `SR` はソース該当チャンネルの直接再生
- `BS.775 L` = ITU-R BS.775 ダウンミックス係数で 6ch を L/R 2ch に縮約した値 (§8.2)
- `無音` = そのスピーカーは音を出しません (binding は維持されますがプリムから音が出ない)

### 8.2 BS.775 ダウンミックス係数 (6ch ソース → L/R)

```
L_out = c × ( FL + 0.707·C + 0.707·SL + 0.5·LFE )
R_out = c × ( FR + 0.707·C + 0.707·SR + 0.5·LFE )
c = 1 / 2.914 ≒ 0.343 (clipping 防止の正規化)
```

センターは均等に左右に振り分け、サラウンドは同側に、LFE は両側に均等に混ぜます。

### 8.3 同じソースを混在配置で使う

5.1ch ソースを `{ch:L}` と `{ch:FL}` の両方に割り当てると、L プリムは BS.775 ダウンミックスで、FL プリムはダイレクトに鳴ります。混乱しやすいので、**同じソースには同じ系統の ch (`L/R/M` 系 か `FL/FR/...` 系 のどちらか) で揃える** のが推奨です。

混在状態でフォールバックが起きた場合、**routing 診断 chat 通知** (§10.3 / §11.3) で各 ch の実際の挙動を確認できます。5.1ch 会場の構築中はこれを ON にしておくと配置ミスがすぐ見つかります。

### 8.4 5.1ch 会場配置のまま 2ch / 1ch ソースを流したとき

会場に 6 個のスピーカープリム (`ch:FL` / `FR` / `C` / `LFE` / `SL` / `SR`) を配置済みの状態で、ソース URL を 5.1ch 配信から **普通のステレオ (2ch) 配信** や **モノラル (1ch) 配信** に切り替えるケース。例えば「ライブ本番は 5.1ch、休憩中は通常のステレオ BGM」「DJ セットの間に MC のモノラル音声を挟む」といった運用です。

このとき **配置変更・設定変更は一切不要** です。各スピーカープリムは自動的に以下のように振る舞います。

#### 2ch (ステレオ) ソースが流れた場合

| プリムの `ch` 値 | 鳴る内容 |
|---|---|
| `{ch:FL}` | **L** (フロント左の代わりにステレオ L 直取り) |
| `{ch:FR}` | **R** (フロント右の代わりにステレオ R 直取り) |
| `{ch:C}` | **(L+R)/2** (センターは L+R の平均 = モノラルダウンミックス) |
| `{ch:LFE}` | **無音** (LFE 信号がソースに存在しない) |
| `{ch:SL}` | **無音** (サラウンド左信号がソースに存在しない) |
| `{ch:SR}` | **無音** (サラウンド右信号がソースに存在しない) |

聴感上は「**会場前方の 3 本 (FL / FR / C) でステレオが鳴り、サラウンド 3 本 (LFE / SL / SR) は黙る**」状態になります。

#### 1ch (モノラル) ソースが流れた場合

| プリムの `ch` 値 | 鳴る内容 |
|---|---|
| `{ch:FL}` / `{ch:FR}` / `{ch:C}` | **M** (フロント 3 本ともモノラル直取り、3 箇所から同じ音が鳴る) |
| `{ch:LFE}` / `{ch:SL}` / `{ch:SR}` | **無音** |

#### 設計意図 / 補足

- **フロント 3 本 (FL / FR / C) は、どんな ch 数のソースでも必ず何かしら鳴る** ─ これにより 5.1ch ↔ 2ch ↔ 1ch とソースを切り替えても、会場前方からの音が途絶えません。
- **LFE / SL / SR はソースに該当信号がない場合は無音で固定** ─ 偽の bass や偽のサラウンドを合成して鳴らすことはしません。
- **ソースが 5.1ch に戻ると、各プリムは自動的に該当チャンネルの直取りに復帰** します (URL 切替後の reconnect で再評価)。再配置・再設定は不要です。
- 5.1ch 会場のまま日常的に 2ch BGM を流すのは **正規の運用パターン** です。「サラウンドが黙る」のは仕様であり、不具合ではありません。

#### 設定でフォールバック内容を Chat に出す (推奨: 構築・検証中)

「無音になっている prim は本当にフォールバック仕様で黙っているのか、それとも何か壊れているのか」を **Local Chat で確認できる診断スイッチ** が用意されています。

**設定の場所** (どちらでも同じ動作、両者は同期):

- **Preferences > Sound > Show channel routing diagnostics in chat** (チェックボックス)
- **Debug Settings: `Stream3DRoutingDiagnostic`** (`true` / `false`)

ON にすると、5.1 配置 × 2ch / 1ch ソースのフォールバック発生時に **Local Chat の自分自身からの発言** として以下のような行が出ます (`3D Stream:` プレフィクス付き、§11.3 の helper 経由):

**2ch ソース × 5.1 配置 (FL/FR/C/LFE/SL/SR の 6 prim) の場合**:

```
[12:34] You: 3D Stream: ch:FL prim playing L (source is 2ch)
[12:34] You: 3D Stream: ch:FR prim playing R (source is 2ch)
[12:34] You: 3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)
[12:34] You: 3D Stream: ch:LFE prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SL prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SR prim silent (source is 2ch)
```

**1ch ソース × 5.1 配置 の場合**:

```
[12:35] You: 3D Stream: ch:FL prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:FR prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:C prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:LFE prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SL prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SR prim silent (source is 1ch)
```

これで「LFE / SL / SR が無音なのは仕様、FL / FR / C はフォールバック動作中」が一目で分かります。

通知は `(root_id, url, observed_channel_count, prim_set_signature)` の組をキーに throttle されるため、配置やソース ch 数が変わるまで同じ通知は再送されません。**5.1ch 会場の構築・検証中だけ ON にして、本番では OFF (`false`、これが既定値)** が推奨です。詳しくは §11.3 を参照してください。

#### 逆方向: 2ch 配置のまま 5.1ch ソースが流れた場合

参考までに反対方向も整理します。`{ch:L}` / `{ch:R}` / `{ch:M}` だけで配置したステレオ会場 (= 2ch 配置) に 5.1ch ソースが流れた場合は、

- L / R / M プリムが **BS.775 ダウンミックス** (§8.2) で 5.1ch を 2ch に縮約して鳴らします。FL / C / SL / LFE はすべて L 側に、FR / C / SR / LFE はすべて R 側に係数付きで合成されます。
- すべての ch 信号が L / R 経由で聴こえるため、**音が消えるチャンネルはありません**。
- `Stream3DRoutingDiagnostic` ON 時の Local Chat 出力例:

```
[12:36] You: 3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)
[12:36] You: 3D Stream: C content folded into BS.775 downmix (source is 6ch, no ch:C prim)
[12:36] You: 3D Stream: LFE content folded into BS.775 downmix (source is 6ch, no ch:LFE prim)
[12:36] You: 3D Stream: SL content folded into BS.775 downmix (source is 6ch, no ch:SL prim)
[12:36] You: 3D Stream: SR content folded into BS.775 downmix (source is 6ch, no ch:SR prim)
```

「専用 prim がないので BS.775 ダウンミックス path に折り込んだ」旨が ch 単位で通知されます。

---

## 9. 配信側 (ソース URL の作り方)

### 9.1 対応 codec / コンテナ

| codec / コンテナ | 1ch | 2ch | 6ch | 備考 |
|---|---|---|---|---|
| **MP3** | ✓ | ✓ | — | SHOUTcast / Icecast の伝統的経路 |
| **Vorbis (Ogg)** | ✓ | ✓ | ✓ | 6ch も実機検証済み (r9 P10) |
| **Opus (Ogg)** | ✓ | ✓ | △ | 6ch は Opus channel mapping family 1。**単純 HTTP / Icecast push は seek 失敗** で開けない場合あり (§9.4) |
| **FLAC** | ✓ | ✓ | △ | 6ch は理論上対応、Opus と同じ seek 制約あり |
| AAC (ADTS / HLS) | — | — | — | 非対応 |
| AC-3 / E-AC-3 | — | — | — | Dolby ライセンス問題で非対応 |

ソース URL は `http://` / `https://` のいずれも受け付けます。HTTP/1.1 keep-alive を維持する経路 (= SHOUTcast 互換 streamer や ffmpeg の TCP 出力) のほうが、単純な静的 HTTP より安定する傾向があります。

### 9.2 1ch / 2ch の配信

ふつうの SHOUTcast / Icecast / 静的 HTTP で OK です。MP3 / Vorbis / Opus / FLAC のいずれでも問題なく動作します。`oggenc` や ffmpeg / butt 等の通常の配信ツールがそのまま使えます。

### 9.3 5.1ch (Vorbis 6ch) の配信

5.1ch を viewer 側で確実に動かす経路として **Vorbis 6ch** が推奨されます (r9 P10 で実機検証済み)。

#### 9.3.1 テスト素材作成 (ffmpeg)

```bash
# 各 ch にユニーク周波数を埋めた 5.1 WAV (10 秒)
ffmpeg -f lavfi -i "sine=440:d=10" -f lavfi -i "sine=550:d=10" \
       -f lavfi -i "sine=660:d=10" -f lavfi -i "sine=110:d=10" \
       -f lavfi -i "sine=770:d=10" -f lavfi -i "sine=880:d=10" \
       -filter_complex "[0:a][1:a][2:a][3:a][4:a][5:a]amerge=inputs=6[a]" \
       -map "[a]" -ac 6 -channel_layout 5.1 test_5_1.wav

# Vorbis 6ch にエンコード
ffmpeg -i test_5_1.wav -c:a libvorbis -q:a 5 test_5_1.ogg
```

#### 9.3.2 静的 HTTP 配信 (検証用)

```bash
python3 -m http.server 8080
```

URL: `http://<host>:8080/test_5_1.ogg`

#### 9.3.3 リアルタイム配信 (ffmpeg → Icecast)

```bash
ffmpeg -re -i test_5_1.wav \
  -c:a libvorbis -q:a 5 \
  -ac 6 -ar 48000 \
  -content_type audio/ogg \
  -f ogg icecast://source:hackme@localhost:8000/aya_5_1.ogg
```

主要オプション:
- `-re` = real-time (素材長に合わせて流す。生配信のシミュレーション)
- `-content_type audio/ogg` = Icecast に MIME を申告 (これがないと MP3 と誤判定する)
- `-ac 6 -ar 48000` = 6ch 48kHz を維持

### 9.4 Opus 6ch / FLAC 6ch の制約

Opus 6ch (channel mapping family 1) や FLAC 6ch を **単純な HTTP** (例 `python3 -m http.server`) や **Icecast push** で配信すると、FMOD の parser が **seek 要求** をかけるため `FMOD_ERR_FILE_COULDNOTSEEK` で開けないケースがあります。

回避策:

- **SHOUTcast 互換 streamer** を使う (keep-alive + range 対応)
- **ffmpeg primary** を経由する (TCP backpressure で解決)
- **5.1ch GUI 配信ツール `butt-aya`** (AYA さん別プロジェクトで開発中、本書執筆時点で未公開) で push する

確実に動かしたい場合は **Vorbis 6ch** を選ぶのが現状の最短経路です。

### 9.5 配信側ツールの選び方

| ツール | 用途 | 注意 |
|---|---|---|
| **ffmpeg** | 任意 codec / 任意 ch / 静的 / リアルタイム | CLI 操作が必要、最も柔軟 |
| **butt** (公式) | DJ 配信 | 1ch / 2ch のみ、5.1ch 非対応 |
| **butt-aya** (5.1ch fork) | 5.1ch GUI 配信 | AYA さん別プロジェクト、本書執筆時点で未公開 |
| **Liquidsoap** | 高度な放送オートメーション | 設定の難易度高、上級者向け |
| **Mixxx / DarkIce / ezstream** | DJ / 自動化 | stereo 前提、5.1ch 非対応 |

---

## 10. viewer 側の設定

### 10.1 Preferences 経由

**環境設定 → Sound** タブに以下のコントロールがあります:

- **3D Stream** スライダー — 全ストリームの音量倍率 (`Stream3DVolumeMaster`)
- **Enabled** チェックボックス — 機能全体の ON/OFF (`Stream3DEnabled`)
- **Show channel routing diagnostics in chat** — routing 診断通知 (`Stream3DRoutingDiagnostic`、§11.3)
- **Hear media and sounds from:** — リスナー位置を Camera / Avatar から選択 (`MediaSoundsEarLocation`、§13.1)

スピーカーアイコンの Volume プルダウンにも同じ「3D Stream」スライダーが現れ、ボイスチャットの近くから音量を即時調整できます。

### 10.2 Debug Settings (詳細チューニング)

`Ctrl + Alt + D` で Advanced メニューを出し → Show Debug Settings から各キーを直接編集できます。

| 設定キー | 型 | 既定値 | 意味 |
|---|---|---|---|
| `Stream3DEnabled` | bool | `true` | 機能全体のキルスイッチ。`false` で全 binding を即座に解除、再 enable しても自動再 bind しない (次の poll で再発見) |
| `Stream3DDescriptionScan` | bool | `true` | `false` で Description タグスキャンを停止し、すべてのプリム binding を解除 (debug stream は影響受けない) |
| `Stream3DMaxConcurrent` | S32 | `4` | 同時 binding 上限 (mono + stereo の合計)。0 で無制限 |
| `Stream3DStereoMaxSpeakers` | S32 | `16` | 1 リンクセットあたりのスピーカー数上限。超過分は traversal 末尾から落ちて警告通知 |
| `Stream3DRolloffMin` | F32 (m) | `1.0` | モノラルタグ既定の近距離 (mono の `{min}` 省略時) |
| `Stream3DRolloffMax` | F32 (m) | `20.0` | 既定の遠距離 (mono の `{max}` / stereo の `{range}` 省略時の共通フォールバック) |
| `Stream3DMaxDistance` | F32 (m) | `64.0` | プリム検出のポーリング半径。`Stream3DRolloffMax` 以上に設定 |
| `Stream3DPollInterval` | F32 (秒) | `30.0` | Description ポーリング間隔。LSL 経由のタグ変更検出にこの間隔がかかる。0 で能動ポーリング無効 |
| `Stream3DVolumeMaster` | F32 [0〜1] | `0.5` | マスター音量倍率。Preferences の 3D Stream スライダーと同じ |
| `Stream3DReconnectAttempts` | S32 | `3` | ストリーム切断時の自動再接続試行回数。各リトライは 5 秒待機。0 で再接続無効 |
| `Stream3DRoutingDiagnostic` | bool | `false` | routing 診断 chat 通知の ON/OFF (§11.3)。Preferences のチェックボックスと同期 |

#### Debug 専用 (動作確認用)

| 設定キー | 型 | 用途 |
|---|---|---|
| `Stream3DDebugUrl` | 文字列 | デバッグ用 URL |
| `Stream3DDebugPlay` | bool | `true` でアバター前方 5m にモノラルストリームを設置・再生 (タグ書き込み不要のクイックテスト) |
| `Stream3DDebugStereoPlay` | bool | 同様にステレオ版デバッグ再生 |

### 10.3 設定の永続化と即時反映

ほとんどの設定は **「Live」** = 値を変更した次フレームから反映されます。Viewer 再起動は不要です。例外:

- `Stream3DEnabled` を `false` → `true` に切替: 自動で再 bind されません。次の poll cycle (既定 30 秒以内) で再発見されます。
- `Stream3DDescriptionScan` を切替: 即時に全 binding 解除 / 再発見の挙動。

---

## 11. エラー通知 / 診断

### 11.1 通知の出方

タグの書式エラーや構造エラーは **ローカルチャットに通知** されます。先頭に「3D Stream:」が付き、システムメッセージとして表示されます。

```
3D Stream: タグ書式エラー (オブジェクト名: "MySpeaker")
  ch の値が L/R/M/FL/FR/C/LFE/SL/SR のいずれかである必要があります。
  例: [3dstream-stereo:{ch:L}{range:30}]
```

```
3D Stream: 構造エラー (リンクセット root: "MainStage")
  音源宣言 (url) が root にあるがスピーカー (ch) が見つかりません。
  各スピーカープリムに [3dstream-stereo:{ch:L|R|M}] を記載してください。
```

### 11.2 30 秒抑制

同じプリム × 同じエラー種別の通知は **30 秒間抑制** されます。タグを連続編集している間にチャットが洪水になるのを防ぐためです。30 秒以上経つと再度 1 回だけ通知が出ます。

抑制された通知は LL_DEBUGS ログ (debug 用ログ) には残るので、内部で何が起きているかは log で確認できます。

### 11.3 Routing 診断 (5.1ch 配置時)

5.1ch / マルチスピーカー会場の **構築・検証中** に「どのプリムが何を鳴らしているか / なぜ無音なのか」を Local Chat で確認できる診断機能です。**既定は OFF**、明示的に ON にしないと出ません。

#### 11.3.1 ON にする方法

以下のどちらでも有効化できます (両者は同期しています):

- **Preferences > Sound > Show channel routing diagnostics in chat** ─ チェックボックスを ON
- **Debug Settings: `Stream3DRoutingDiagnostic` を `true`** に設定 (Advanced > Show Debug Settings から)

設定は即時反映されます。Viewer 再起動不要です。

#### 11.3.2 出力先と書式

ON にすると、フォールバック発生時に **Local Chat の自分自身からの発言** として以下の書式で 1 行ずつ出ます。

```
[HH:MM] You: 3D Stream: <内容>
```

`3D Stream:` プレフィクスは `notifyStream3D` ヘルパが自動付与します。チャットログ (`Show in Chat`) にも当然残るため、検証後に見返せます。**他人には見えません** (自分の Local Chat にのみ表示される擬似発言)。

#### 11.3.3 通知文言一覧

| 状況 | Local Chat に出る行 |
|---|---|
| 6ch ソース × `ch:L/R/M` プリムあり (専用 prim なし) | `3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)` (FL/FR/C/LFE/SL/SR の各 ch について個別に出る) |
| 6ch ソース × 専用 prim も `ch:L/R/M` prim もなし | `3D Stream: FL content has no destination — dropped (source is 6ch, no ch:L/R/M prim)` |
| 2ch ソース × `ch:FL` プリム | `3D Stream: ch:FL prim playing L (source is 2ch)` |
| 2ch ソース × `ch:FR` プリム | `3D Stream: ch:FR prim playing R (source is 2ch)` |
| 2ch ソース × `ch:C` プリム | `3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)` |
| 1ch ソース × `ch:FL/FR/C` プリム | `3D Stream: ch:FL prim playing M (source is 1ch)` (FR / C も同様の書式) |
| 1ch / 2ch ソース × `ch:LFE/SL/SR` プリム | `3D Stream: ch:LFE prim silent (source is 2ch)` (1ch のときは `1ch`、SL / SR も同様) |

5.1ch 配置 × 2ch ソースの具体的な出力サンプルは §8.4 の「設定でフォールバック内容を Chat に出す」を参照してください。

#### 11.3.4 throttle と再表示条件

通知は `(root_id, url, observed_channel_count, prim_set_signature)` の組をキーに throttle されます。同じ会場 / 同じソース構成のままでは **再表示されません** (Chat が洪水になるのを防ぐため)。以下のいずれかが変わると再評価され、再度通知が出ます:

- ソース URL が変わる (= 別ストリームに差し替え)
- ソースの ch 数が変わる (= 同じ URL でも 5.1ch ↔ 2ch 切替が起きた)
- 会場のスピーカープリム構成が変わる (prim 追加 / 削除 / `ch` 値変更)
- `Stream3DRoutingDiagnostic` を OFF→ON に切り替えた直後

#### 11.3.5 運用推奨

- **会場の組み立て中・配置検証中は ON** にして、各 prim のフォールバック挙動を Local Chat で確認
- **本番運用 (ライブ中など) は OFF** に戻す。チャットを綺麗に保つため
- 既定 (OFF) は本番想定。配置検証時のみ手動で ON にする運用を想定しています

### 11.4 ログ (`LL_INFOS("Stream3D")`)

詳細な動作ログは AYAstorm のログファイルに記録されます。

```
~/.ayastorm_x64/logs/AYAstorm.log
```

`Stream3D` channel で grep すると、binding の確立 / 切断 / 再接続 / source format 検出 / dropout 等が確認できます。

---

## 12. トラブルシューティング

### 12.1 タグを書いたのに音が出ない

確認順序:

1. **Description が正しく書き換わっているか**: プリムを右クリック → Edit → Description タブで現在値を確認
2. **タグの綴り**: `[3dstream:` または `[3dstream-stereo:` が含まれているか (タイポ注意)
3. **`{url:...}` のスキームが http/https**: `file://` や相対 URL は不可
4. **`Stream3DEnabled` / `Stream3DDescriptionScan` が true**: Preferences > Sound または Debug Settings で確認
5. **ポーリング待ち**: LSL `llSetObjectDesc` 経由の変更は最大 30 秒待つ (= `Stream3DPollInterval`)
6. **チャットにエラー通知が出ていないか**: §11 のエラー文言を確認
7. **ログに `LL_INFOS("Stream3D")` の reconnect attempt が出ていないか**: ストリーム URL が落ちている可能性

### 12.2 ステレオの片方しか鳴らない

- ルートに `{url}` のみ書いて子に `{ch:R}` だけ書いた場合、L チャンネルを担当するプリムがいないので「L 片肺」状態になります。ルートに `{ch:L}` を併記するか、別のプリムに `{ch:L}` を割り当ててください
- 同じ `{ch:L}` を 2 つのプリムに書いて両方からダブって鳴らしたい場合は意図通りなので OK
- routing 診断 (`Stream3DRoutingDiagnostic`) を ON にすると、各 ch が何を再生しているかが chat に出ます

### 12.3 5.1ch ソースが開けない / 音がブツブツ切れる

- §9.4 の seek 制約: 単純 HTTP / Icecast push の Opus 6ch / FLAC 6ch で起きやすい問題。**Vorbis 6ch に切り替える** か、SHOUTcast 互換 streamer / ffmpeg primary 経由に切り替えてください
- HTTP 切替直後の最初の 5〜10 秒は prebuffer 充填中に dropout 警告が出る場合があります (LAN 環境で 408〜2045 frames/spk/s ≒ 0.8〜4% 程度)。定常運用では消えます
- ストリームのビットレートが高すぎる / ネットワークが詰まっている場合の dropout: 配信側でビットレートを下げる (256kbps 以下推奨) / 同時 binding 数を減らす

### 12.4 子プリムにタグを書いてもスピーカーとして認識されない

- 子プリムの Description は Properties 通信で取得されます。**初回のリンクセット入域時に少し時間がかかる** ことがあります (数秒〜10 秒)
- LSL `llSetObjectDesc` で子プリムの Description を変更した場合は、次の poll cycle (= 30 秒以内) で反映されます
- `{ch:...}` の値が typo になっていないか (大文字小文字は問題なし、ただしスペル間違いは無効)

### 12.5 リスナー位置がおかしい (音の方向が変)

- カメラを大きく動かすとリスナー位置がカメラ移動に追従するため、定位が変化します。Avatar 視点で固定したい場合は Preferences > Sound の **「Hear media and sounds from:」を Avatar に切替** (`MediaSoundsEarLocation = 1`)
- Camera / Avatar の切替は 3D Stream にも効きます (パーセル BGM / LSL `llPlaySound` 等と共通設定)

### 12.6 同時に複数の 3D Stream を鳴らしたい

- `Stream3DMaxConcurrent` (既定 4) を超えると新しい binding は拒否されます。同時運用したい場合は値を増やしてください (8 / 16 程度まで実用)
- ただし 1 binding = 1 デコーダスレッド + N スピーカーチャンネルで CPU を消費します。20 並走等は CPU 負荷が大きいので、必要分だけ増やしてください

### 12.7 タグを消したのに音が止まらない

- 再評価のトリガが発火していない可能性。プリムを 1 度移動するか、ぐるりと回って再 polling を待ってください
- それでも止まらない場合は `Stream3DEnabled` を一旦 false にして全 binding を強制解除、再 true で再発見

---

## 13. 既知の制約 / 仕様上の注意

### 13.1 リスナー位置はカメラまたはアバター

3D Stream の音場計算に使われるリスナー位置は、Preferences > Sound の **「Hear media and sounds from:」** 設定に従います。

- `Camera` (既定): カメラ位置 / カメラ向き
- `Avatar`: アバター位置 / アバター向き

これは LSL `llPlaySound` / パーセル BGM / Media-on-a-Prim とも共通の設定です。

### 13.2 1 リンクセット = 1 ストリーム

1 つのリンクセット内に `{url}` を持つルートが「ある」/「ない」だけが意味を持ちます。**複数の `{url}` を 1 リンクセットに書くことはできません** (子プリムに `{url}` を書いても無視されます)。

複数の異なるストリームを 1 つの会場で鳴らしたい場合は、リンクセットを分けて配置してください (= `Stream3DMaxConcurrent` の枠内で複数 binding を持つ)。

### 13.3 Description 文字数 (127 byte)

LSL `llSetObjectDesc` が書き込める Description は **127 byte 上限**です。日本語を含む URL や説明文は UTF-8 で容易に超過します。

長くなる場合の対処:

- ルートに `{url}` だけ書いて子プリムに `{ch}` だけ書く分散方式 (この場合、各プリムの Description は短く保てます)
- URL を短縮 (URL shortener、または配信側のパス短縮)

### 13.4 codec 別の動作実績

| codec | 1ch / 2ch | 6ch |
|---|---|---|
| Vorbis (Ogg) | ✓ 実機検証済 | ✓ 実機検証済 (r9 P10、12 分連続 0 dropout) |
| Opus (Ogg) | ✓ 実機検証済 | △ コードレビューのみ (本番経路で動作実績、static HTTP / Icecast push は seek 失敗) |
| FLAC | ✓ 実機検証済 | △ コードレビューのみ (Opus と同制約) |
| MP3 | ✓ 実機検証済 | — |

5.1ch を確実に動かしたい場合は **Vorbis 6ch** を選んでください。

### 13.5 LFE の特殊扱いはなし

5.1ch の LFE (サブウーファー) は、viewer 側で「ローパスフィルタ」「2D 化」などの特殊処理は行われず、他の 5 チャンネルと同等に 3D 配置 + 距離減衰されます。低域フィルタリングが必要なら配信側 mix で済ませてください。

物理的なサブウーファー筐体を SL 内のプリムとして配置し、その位置から低域音を出すという運用が想定されています。

### 13.6 5.1ch の自由視点モデル

実 5.1 (映画基準・ITU-R BS.775) は **リスナーが固定位置にいる前提**で各 ch に方向感を埋め込みます。SL のリスナーは自由視点なので「sweet spot」概念は適用できません。本機能で目指すのは **「会場で 5.1 ソースを多点再生する」** という PA 的な発想であり、シネマ的サラウンド体験の再現ではありません。

リスナーが空間を歩き回ると 5.1 mix の意図した定位は当然崩れますが、「会場感」「面で鳴っている感」は十分に出ます。

### 13.7 他 Viewer での挙動

`[3dstream:...]` / `[3dstream-stereo:...]` タグは **AYAstorm 専用** です。本家 Firestorm / 公式 LL Viewer / Catznip 等の他 Viewer は完全に無視します。

- AYAstorm 利用者には 3D 定位再生される
- 他 Viewer 利用者にはタグが説明文の一部として表示されるだけで、音は鳴らない (パーセル BGM とは独立に動作するため、パーセル BGM が設定されていればそれは聞こえる)

### 13.8 同時最大数

| 上限 | 既定 |
|---|---|
| `Stream3DMaxConcurrent` (linkset 単位の binding 数) | 4 |
| `Stream3DStereoMaxSpeakers` (1 binding あたりのスピーカー数) | 16 |
| 結果として最大 同時スピーカー数 | 4 × 16 = 64 |

64 channel 程度までは FMOD の余裕があります。それ以上必要な場合は debug settings で値を上げてください (実機での CPU 負荷確認は必須)。

### 13.9 音量の合成

最終音量 = `Stream3DVolumeMaster` × `{volume:N}` × FMOD 距離減衰 × Master Audio Slider × 各種ミュート状態。

通常は `Stream3DVolumeMaster` (Preferences の 3D Stream スライダー) で全体調整、`{volume:N}` でプリム個別の補正、距離減衰は `range` (スピーカー個別) または `Stream3DRolloffMax` (全体既定) で制御します。

---

## 14. 関連ドキュメント / 内部仕様書

本ガイドは **利用者向け** の書式リファレンスです。実装内部の詳細 (decode thread / FMOD 経路 / リング bufferr / シャットダウン順序等) は以下の仕様書を参照してください。

| ドキュメント | 内容 |
|---|---|
| `doc/spec_positional_stream_audio.md` | 3D Stream 本体仕様 (r5 改訂) — 基本アーキテクチャ |
| `doc/spec_stream3d_decode_thread.md` | r7 で確立した 3-thread モデル |
| `doc/spec_distributed_stereo.md` | r8 分散記述ステレオ仕様 — `[3dstream-stereo:...]` の field 書式 |
| `doc/spec_5_1ch_source.md` | r9 5.1ch ソース受入仕様 — Opus/FLAC 6ch decode 経路 + BS.775 ダウンミックス |
| `doc/spec_5_1ch_placement.md` | r10 5.1ch 会場配置仕様 — `ch=FL/FR/C/LFE/SL/SR` 拡張 + 互換マトリクス |
| `doc/spec_binaural_venue_reverb.md` | r11 (未リリース) バイノーラル + 会場残響仕様 — 本ガイドは r10 までのみ記載 |
| `docs/ayastorm-stream3d-roadmap.md` | 3D Stream 全体ロードマップ |

---

## 改訂履歴

- **2026-05-05 (初版)**: r10 時点の最終仕様として整備。r5 / r8 / r9 / r10 / r10.x の累積仕様をまとめて記述。r11 以降は未リリースのため対象外。

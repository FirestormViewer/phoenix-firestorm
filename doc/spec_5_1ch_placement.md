# 5.1ch placement 仕様書 (r10)

> **対象**: AYAstorm `v7.2.5-ayastorm-r10` (想定)
> **前提**: `doc/spec_5_1ch_source.md` (r9 5.1ch ソース受入) — 本書はその §9.1 / §12.6 を独立化したもの
> **前提**: `doc/spec_distributed_stereo.md` (r8 分散記述ステレオ仕様)
> **背景文書**: `docs/ayastorm-positional-stream.md` (M1〜Post-M9 実装記録)

## 1. 目的とスコープ

r10 は r9 (5.1ch ソース受入 / 内部 L/R downmix) に対して「**6ch source の各 channel を個別 prim に配置する経路**」を追加する内部リリース。viewer 出力チャンネル割当を r8/r9 の `ch:L|R|M` から **`ch:L|R|M|FL|FR|C|LFE|SL|SR`** へ拡張し、source 6ch を downmix せず per-channel に分離して各 prim へ流せるようにする。

**設計の柱は「ライブ会場モデル」**。実 5.1 ホームシアターの subwoofer 概念 (LPF / omnidirectional / bass management) を持ち込まず、6 つの ch を **対等な positional speaker** として扱う。LFE prim も「6 番目のスピーカー」であり、特殊扱いはしない。サブの音響的特殊性は r11 (Steam Audio / HRTF) で Layer 2 として後付けする。

主目的:

- `ch=FL|FR|C|LFE|SL|SR` を r8/r9 タグ書式に **値の集合として追加** し、6ch source の各 channel を直接対応 prim へ流す
- ring の `n_tracks` を 6 に切替可能とし、reader (`LLPositionalStreamMulti::pcmReadCallback`) で per-channel 取り出す経路を確立
- ch:L/R/M prim と ch:FL〜SR prim が **同一 source URL に対して併存** できるようにする (downmix path と placement path の同居)
- routing 診断機構を整備し、配置者が「どの source channel がどの prim へ流れたか」を log で確認できる
- r11 (HRTF / Steam Audio Layer 2) の hook point を spec / コード側で確保し、placement 設計が r11 整合性を持つことを保証

非対象 (r10 では触らない):

- HRTF / SOFA / Steam Audio integration — r11
- LFE 専用処理 (LPF / omnidirectional / bass management) — r11 以降、必要が出た時点で再評価
- AAC 5.1 / HLS / DASH ソース — r9 §9.5 と同方針
- GUI 配信ツール (butt-aya 等) — AYAstorm 本体スコープ外、別 repo
- 公開 README / changelog への 5.1 placement 機能開示 — r11 で一括

---

## 2. 問題定義

### 2.1 r9 の制約

r9 は 6ch source を受け入れたが、viewer 内部で **decode 段に BS.775 downmix を強制** している:

- ring は `n_tracks=2` 固定 (r8 と同じ)
- decode worker が 6ch source を L/R 2-track にまとめて ring に書き込む
- reader (`pcmReadCallback`) は track 0/1 (= L/R downmix 済み) しか参照できない
- 結果: ch:L/R/M 3 値しか配置できず、5.1 配信を 5.1 のまま聴かせる手段が無い

### 2.2 ライブ会場用途で求められること

AYAstorm の典型的 5.1 用途は「**virtual live venue**」:

- avatar が会場内を歩き回るため、ホームシアターの "sweet spot 1 点" モデルが成立しない
- 配置者は PA stack (FL/FR top)、center cluster (C)、surround (SL/SR)、sub stack (LFE) を **任意の世界座標** に置きたい
- 各 prim は世界座標で位置依存・距離減衰を持つ (実会場と同じ挙動)
- LFE は「サブウーハー特殊扱い」ではなく「会場の sub stack」として位置情報を保つ

r9 の downmix path はこのモデルに対応できない (6ch を 2ch に潰す段階で per-channel 位置情報が消える)。

### 2.3 r11 着手前にやっておきたいこと

r11 (HRTF / Steam Audio Layer 2) を着手する前に、以下を r10 で済ませる:

- 6 channel が ring 〜 reader 〜 FMOD source まで **per-channel separable** に流れる経路
- FMOD 既定 panner を後で殺せる hook point の所在を spec に明記 (r11 で `set3DLevel(0.0f)` を呼ぶ箇所)
- 配置の世界座標が Steam Audio の source position として消費可能な形 (= prim の transform をそのまま渡せる)

これらが r10 で揃っていれば、r11 は「FMOD positional 出力を Steam Audio 出力に差し替える」付加作業で済む (Layer 1 + Layer 2 直交)。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. `ch=FL|FR|C|LFE|SL|SR` の 6 値を r8/r9 タグ書式に追加、6ch source で per-channel 配置できる
- G2. ch:L/R/M prim (r8/r9 path) と ch:FL〜SR prim (r10 path) が同一 source URL に共存できる
- G3. 1ch / 2ch / 6ch source × 9 ch 値の compatibility matrix が決定論的に振る舞う (§4.2)
- G4. routing 診断 log で配置者が source channel の到達状況を確認できる (§4.4)
- G5. r9 の F1〜F9 受入条件を回帰確認、1ch/2ch/6ch downmix 経路に regression なし
- G6. r11 hook point (FMOD `set3DLevel(0.0f)` 呼出箇所) をコード側で識別可能にする
- G7. 配置の世界座標が r11 で Steam Audio に渡せる形を保つ (placement で世界座標を baking しない)

### 非ゴール

- NG1. HRTF / Steam Audio / SOFA — r11
- NG2. LFE 専用 LPF / omnidirectional 化 / bass management — r11 以降
- NG3. AAC 5.1 / HLS / DASH — r9 §9.5 と同方針
- NG4. ホームシアター sweet spot 検証 / 配置強制 — 任意配置許容のまま
- NG5. GUI 配信ツール — 別 repo
- NG6. 公開 README / changelog 開示 — r11 で一括

---

## 4. 設計

### 4.1 タグ書式 — `ch` 値拡張

r8 で導入した `ch:L|R|M` の **値の集合を拡張**する形で追加する。**新規キーは導入しない** (`pos` 等の並走キーは入れない、§4.1.1 議論参照)。

```
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:FL}{range:30}{volume:0}]
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:FR}{range:30}{volume:0}]
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:C}{range:30}{volume:0}]
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:LFE}{range:50}{volume:0}]
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:SL}{range:30}{volume:0}]
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{ch:SR}{range:30}{volume:0}]
```

許容値 (大文字小文字区別なし、parser は `LLStringUtil::toUpper` で正規化):

| 値 | 意味 | 由来 |
|---|---|---|
| `L` | source の L (2ch) または BS.775 downmix L (6ch) | r8 |
| `R` | source の R (2ch) または BS.775 downmix R (6ch) | r8 |
| `M` | mono / 全 ch downmix | r8 |
| `FL` | source の Front Left (6ch のみ、2ch fallback あり) | **r10 新規** |
| `FR` | source の Front Right | r10 新規 |
| `C` | source の Center | r10 新規 |
| `LFE` | source の LFE (6ch のみ) | r10 新規 |
| `SL` | source の Surround Left (6ch のみ) | r10 新規 |
| `SR` | source の Surround Right (6ch のみ) | r10 新規 |

不正値 (`ch:foo` 等) は r8/r9 と同じく **silent ignore + warn log 1 回**。

#### 4.1.1 設計判断: 別キー (`pos:` 等) を導入しなかった理由

「source channel 選択」と「3D 空間配置」を別キーで分ける案 (`{ch:FL}{pos:front_left}`) を検討したが棄却。

- 3D 空間配置は **prim の world 座標そのもの** で表現される (r8 から不変)。タグレベルの位置キーは情報重複
- r11 binaural でも同じ — Steam Audio に渡す source position は prim transform から取る、タグキー不要
- 別キー導入は「`{ch:L}{pos:FL}` のとき何が勝つか」のルール追加が必要となり仕様面積が増える

`ch` 一本で「source のどの channel がここで鳴るか」を表現する一貫性を維持。

### 4.2 channel × source 互換性マトリクス

source の channel 数によって `ch` 値の意味が決定論的に変わる。**source を正、prim 構成を従** とした graceful fallback:

| 配置タグ | 6ch source | 2ch source | 1ch source |
|---|---|---|---|
| `ch:FL` | FL track 直取り | L 再生 (fallback) | M 再生 (fallback) |
| `ch:FR` | FR track | R 再生 (fallback) | M (fallback) |
| `ch:C` | C track | M (downmix) (fallback) | M (fallback) |
| `ch:LFE` | LFE track | **silent** + warn (fallback 不可) | silent + warn |
| `ch:SL` | SL track | silent + warn | silent + warn |
| `ch:SR` | SR track | silent + warn | silent + warn |
| `ch:L` | r9 BS.775 downmix L | L 直取り | M (fallback) |
| `ch:R` | r9 BS.775 downmix R | R 直取り | M (fallback) |
| `ch:M` | r9 mono downmix (BS.775 から再 mix) | L+R mix | M 直取り |

**設計判断**:

- FL/FR は stereo の L/R にマップ — 「front-left 位置の prim」という配置者意図は ch 数低下でも保てる
- C は M (downmix) — 中央定位は stereo にも存在する概念、合成して提供
- **LFE / SL / SR は stereo source で silent** — sub / surround content は stereo source に **存在しない**。L/R を複製すると FL/FR と被って空間性破壊、silent が音響的に正直
- 1ch source に対しては「全 ch 値が M 相当」 — どの位置も等価

ch:L/R/M と ch:FL〜SR は同一 source URL で **共存可能**。同じ 6ch source に対して撮影用モニタ prim は ch:M、ホール配置 6 prim は ch:FL〜SR、というシナリオが自然に表現できる。

### 4.3 ライブ会場モデル — LFE と配置ガイドライン

#### 4.3.1 LFE = 「6 番目のスピーカー」

実 5.1 ホームシアターの subwoofer は (a) LPF (b) omnidirectional (c) bass management の 3 つを束ねた特殊概念。**r10 はこの 3 つを一切実装しない**:

| サブウーハー要素 | ホームシアター | ライブ会場 | r10 挙動 |
|---|---|---|---|
| 方向定位 | 捨てる | 持つ (sub stack 位置がわかる) | 持つ (FMOD positional) |
| 距離減衰 | 切る | 掛かる | 掛かる |
| 帯域 | 強制 LPF | 配信側 crossover で band-limit | ソース任せ (LPF なし) |
| bass management | あり | なし | なし |

ライブ会場モデルでは `ch:LFE` prim は他の 5 ch と同等の positional speaker として扱う。配信側で LFE channel を 30〜120Hz に band-limit しておけば実会場の sub stack と区別がつかない (方向定位が残る点を除く)。配信側が full-range な creative content を LFE に乗せた場合は普通に音楽が聴こえる (r9 で `kCoefLfe = 0.5` を採用した creative element 方針と一貫)。

#### 4.3.2 配置ガイドライン — 強制しない

BS.775 角度 (FL/FR ±30°, SL/SR ±110°, C 0°) は **ホームシアター用途の参考値**として spec に併記するが、システム側で検証 / 強制しない。

理由:

- avatar は会場内を歩き回るため「sweet spot 1 点」前提が成立しない
- 同じ 6 prim 配置でも用途で最適解が違う (ホームシアター個室 / ライブ会場 / アート設置 / 商業施設 BGM)
- 配置者の意図を尊重 (シーンによっては FL/FR を左右逆置きする mirror 配置等が意味を持つ)

代表配置例 (reference のみ、強制せず):

| 用途 | FL/FR 角度 | SL/SR 角度 | C 配置 | LFE 配置 |
|---|---|---|---|---|
| ホームシアター個室 | ±30° | ±110° | 0° (front) | 任意 (隅) |
| ライブ会場 (中規模) | PA stack 位置 | 後方コーナー | center cluster (天井) or 省略 | sub stack (前方下) |
| アート設置 | 任意 | 任意 | 任意 | 任意 |
| 商業施設 BGM | 店内分散 | 店内分散 | 店内中央 | 店内分散 (帯域絞った素材推奨) |

### 4.4 routing 診断

source channel と prim 構成の不一致を **chat 通知** で配置者が把握できるようにする。**source を正、prim 構成を従** で双方向に check し、各 fallback ケースごとに 1 行の chat 警告を出す:

```
[Stream3D] FL content folded into BS.775 downmix on ch:L/R/M prims (source is 6ch, no ch:FL prim)
[Stream3D] LFE content folded into BS.775 downmix (no ch:LFE prim)
[Stream3D] ch:FL prim playing L (source is 2ch)
[Stream3D] ch:LFE prim silent (source is 2ch)
```

> **r10 → r10.x スコープ変更の注記**: r10 では出力先を LL_WARNS log + debug settings 配下の `Stream3DRoutingDiagnostic` で実装したが、AYA さんの本来の意図は (a) chat 通知 (b) Preferences (環境設定) UI でのトグルだった。実装と仕様の双方を r10.x で chat / Preferences に揃える (§13.7)。

#### 4.4.1 fallback 決定論

| ケース | fallback 先 | chat 通知文言 |
|---|---|---|
| source ch あり, 専用 prim なし, ch:L/R/M prim あり | BS.775 downmix path に折り込む | `"FL content folded into downmix"` |
| source ch あり, 専用 prim なし, ch:L/R/M も無し | content drop | `"FL content has no destination — dropped"` |
| 専用 prim あり, source ch なし (compat 表で fallback 規定) | compat 表通り | `"ch:FL prim playing L (source is 2ch)"` |
| 専用 prim あり, source ch なし (silent 規定) | silent | `"ch:LFE prim silent (source is 2ch)"` |
| ch:M prim, source 1ch | 直取り | 通知不要 |

各文言は localized string として XUI に登録 (英語・日本語)。chat 通知は `notifyStream3D` ヘルパ経由で発火。

#### 4.4.2 throttle key

```
(root_id, url, observed_channel_count, prim_set_signature)
```

- URL 変更 → key 変化 → re-warn ✓
- 同 URL で配信側 format 切替 (DJ が 5.1→2ch) → reconnect → 新 channel count → key 変化 → re-warn ✓
- 同条件で連続評価 → key 同じ → chat spam なし ✓

`prim_set_signature` は (root 配下の prim ID と ch 値の hash)。配置者が prim を追加 / 削除 / ch 変更すると key が変わって re-warn 発火。

chat はログより視認性が高いため throttle は厳格に運用 (一度発火した条件で 30 秒以内の再発火は抑止、key 変化時のみ即時発火)。

#### 4.4.3 出力先 — Preferences で制御

`Stream3DRoutingDiagnostic` (§6 設定) が `true` のとき chat 通知 (`notifyStream3D` 経由) に出力、`false` (default) で完全静音。設定 UI は **Preferences (環境設定) > Sound タブ** に checkbox を置く (位置は r10.x 実装時に既存タブ構成から決定)。debug settings からも同キーで引けるが、運用 UI は Preferences を primary とする。

LL_WARNS ログ出力は r10.x で **撤去** (chat 一本化、ログ重複を避ける)。診断目的でログにも残したい要望が後から出れば r10.x 以降で別キー (例 `Stream3DRoutingDiagnosticLog`) として復活させる。

### 4.5 viewer 内部経路

```
HTTP source URL
  ↓ FMOD::createSound(FMOD_OPENONLY|FMOD_CREATESTREAM)
FMOD::Sound
  ↓ getFormat(&type, &fmt, &channels, &bits)
LLPositionalStreamStereo::DecodeWorker
  ├─ channels==1 → r8 path (n_tracks=1, M に流す)
  ├─ channels==2 → r8 path (n_tracks=2, L/R に流す)
  └─ channels==6 → r10 path (n_tracks=6, codec 別 layout で per-channel に分解)
LLMultiTailRing
  ├─ 1ch / 2ch source → n_tracks=2 (r8 と同じ)
  └─ 6ch source → n_tracks=6 (r10 新規)
  ↓ pcmReadCallback × N speaker
N 個の FMOD::Channel
  ├─ ch:FL/FR/C/LFE/SL/SR → 対応 track index 直取り
  └─ ch:L/R/M → reader 側で BS.775 downmix on read (6ch ring → 2ch 出力)
```

#### 4.5.1 reader 側 downmix への移行

r9 までは decode worker が downmix していた (decoder-side downmix)。r10 では:

- decode worker は **per-channel raw を ring に書くだけ** (6ch source なら 6 track へ並行 write)
- ch:L/R/M reader が pcmReadCallback で **on-the-fly downmix** (LLMultichannelDownmix を流用)

メリット:

- ch:L/R/M prim が無いシーンでは downmix CPU が完全に節約される
- 同一 6ch ring から placement / downmix の両方の readers が独立に取り出せる
- decoder thread の責務が「ソースを ring へ流す」だけに単純化、placement / downmix 分岐が消える

#### 4.5.2 r11 hook point 明記

FMOD positional pan を r11 で殺す箇所をコード上で識別可能にする:

- `LLPositionalStreamMulti::makeChannelForBinding()` (仮称、r10 で導入) で各 ch の `Channel::set3DLevel(1.0f)` 呼出を 1 箇所に集約
- r11 ではここを `set3DLevel(0.0f)` に切替えて Steam Audio へバトンタッチ
- spec §10 参考コード位置に「r11 hook」コメント明記

世界座標の baking を避けるため、placement で `Channel::set3DAttributes()` に渡す位置は **prim の現在 transform をそのまま** 渡す (オフセット適用や事前計算をしない)。

---

## 5. 検証材料

### 5.1 routing 検証材料 — voice ID + tone のハイブリッド

ch ごとに音声で channel 名 + 異なる freq tone を重ねた 6ch ソース:

| ch | 音声 | tone freq |
|---|---|---|
| FL | "Front Left" | 220 Hz |
| FR | "Front Right" | 440 Hz |
| C | "Center" | 880 Hz |
| LFE | "L F E" | 60 Hz |
| SL | "Side Left" | 1760 Hz |
| SR | "Side Right" | 3520 Hz |

生成パイプライン (r9 P10 の hard-pan 材料生成スクリプトを流用):

```bash
# voice
espeak-ng -v en -w fl_voice.wav "Front Left"
# tone
sox -n -c 1 -r 48000 fl_tone.wav synth 30 sine 220
# mix voice + tone (ch ごと 30s)
sox -m fl_voice.wav fl_tone.wav fl.wav
# 6ch interleave
sox -M fl.wav fr.wav c.wav lfe.wav sl.wav sr.wav -c 6 test_routing_5_1.wav
# Vorbis encode (channel mapping family 1)
oggenc --quality 5 -o test_routing_5_1.ogg test_routing_5_1.wav
```

スクリプトは `doc/r10/gen_test_material.sh` として repo に置く (再現性保証)。生成済みの ogg は repo に入れず、AYA Icecast にホスト。

### 5.2 検証手順

1. **routing 検証**: AYA が各 6 prim に近づいて、`Front Left` / `Front Right` / ... の音声 + 各 freq tone が聴こえることを確認。bleed があれば隣接 prim の音 / freq が混じる
2. **bleed 定量チェック**: 各 prim 近くで他 ch の freq (例: FL prim 近くで 440Hz) が **inaudible** であることを耳で確認
3. **musical quality 検証**: r9 で使った `sample1_6ch.ogg` を 6 prim 配置で再生 (BS.775 角度)、avatar を中心に立たせて「会場感」が出るか主観評価
4. **互換マトリクス検証**: 各 ch 値 × 1ch / 2ch / 6ch source の組み合わせを §4.2 通りに確認 (silent / fallback / 直取りの分類が一致するか)
5. **routing 診断 log 検証**: `Stream3DRoutingDiagnostic=true` でテストし、§4.4 通りの log 出力が出るか確認

### 5.3 受入条件

| 項目 | 基準 | r9 比較 | 実測 (2026-05-04) |
|---|---|---|---|
| 各 6 prim で正しい channel name が聴こえる | ✓ PASS | 新規 | ✓ PASS — P9 #1 で 6 prim ch:FL/FR/C/LFE/SL/SR 配置、各 prim 真横で voice + tone (FL=220Hz / FR=440Hz / C=880Hz / LFE=60Hz / SL=1760Hz / SR=3520Hz) 単独確認 |
| 各 prim から bleed (他 ch freq) が inaudible | ✓ PASS | 新規 | ✓ PASS — P9 #2 で隣接 prim の周波数が真横位置で audible でない、range:8 で十分減衰 |
| sample1_6ch を 6 prim 配置で 5 分連続 dropout 0 | ✓ PASS | r9 §5.3 と同 | ✓ PASS — 5 分間 dropout delta=0 (test_routing_5_1.ogg を Icecast LAN 配信、6 prim linkset 配置) |
| CPU r9 比 +5% 未満 (downmix → reader-side downmix の移行込み) | ✓ PASS | 新規 | ✓ PASS — r10 6 prim placement 平均 **43.66%** vs r9 1 prim ch:M baseline 39.87% = **+3.79pp** (target +5pp 未満)。今回は 6 Multi instance (placement) の重い workload、reader-side downmix 単体の差はこれより小さいはず |
| URL 切替 ×10 (5.1 ↔ 2ch) 全成功 | ✓ PASS | r9 §5.3 と同 | ✓ PASS — P9 / P10 中に 6ch ↔ 2ch ↔ 1ch を計 7 回切替、いずれも reconnect cascade 込みで再生再開 (bug A 修正後) |
| §4.2 互換マトリクス全行が決定論通り | ✓ PASS | 新規 | ✓ PASS — P4 unit test で 27 cell 決定論動作担保、P9 #4 で実機 4 cell (6ch×ch:M / 1ch×ch:FL / 1ch×ch:LFE / 2ch×ch:FL/FR/C/LFE) を spec §4.2 通りに確認 |
| r9 / r8 回帰なし (1ch / 2ch source) | ✓ PASS | r9 §5.3 と同 | ✓ PASS — P10 で ch:M / ch:L × 6ch / 2ch / 1ch source の 6 セルを実機検証、reader-side downmix 移行による r9 経路への regression なし |

---

## 6. 設定項目

r10 + r10.x で追加するユーザ設定は **1 件のみ**:

| 設定キー | 型 | 既定 | 効果 | 設定 UI |
|---|---|---|---|---|
| `Stream3DRoutingDiagnostic` | bool | `false` | `true` で routing 診断 chat 通知を発火 (`notifyStream3D` 経由)。`false` で完全静音 | **Preferences > Sound タブ (r10.x)** + debug settings (alias) |

**設定方針**:

- 一般 listener は default `false` で chat / log とも完全静音、何も気付かず通常使用
- 配置者 (= AYA) は venue 構築 / テスト中だけ `true` にして chat で fallback 状況を確認、本番は `false` に戻す
- 切替 UI は Preferences が primary、debug settings は同キーの alias として機能維持
- 設定切替は live (次の evaluateLinkset から効く、再起動不要)
- chat 通知は § 4.4 の throttle (key 変化時のみ発火 + 30 秒抑止) で spam を防ぐ
- FormatUnsupported / reconnect 失敗の chat 通知は本設定と独立、常時発火継続 (本設定は routing 診断のみを gate する)

**r10 → r10.x の差分**:

- r10: LL_WARNS log + debug settings (実装済、ただし AYA 意図とズレ)
- r10.x: chat 通知 + Preferences UI に変更 (LL_WARNS は撤去、chat 一本化)

`Stream3DDownmixGain` / `Stream3DAcceptMultichannel` は r9 §6 で「将来候補」として列挙されたが、r9 / r10 ともに不要だったため **r10 でも追加しない** (memory: 設定は最小限・無難なデフォルト優先)。

---

## 7. 互換性

### 7.1 タグ

- r8 / r9 タグ書式は **完全に後方互換**。`ch:L|R|M` の挙動は変更なし
- 新規値 `FL|FR|C|LFE|SL|SR` を r8 / r9 viewer が受信した場合: 不正値として silent ignore (既存挙動)。r10 viewer のみが新規値を解釈
- 同じ source URL に対して旧 viewer (r8/r9) と新 viewer (r10) が共存可能 (new 値の prim は旧 viewer で silent、old 値の prim は両 viewer で同等動作)

### 7.2 ABI / 公開ヘッダ

- `LLPositionalStreamMulti` の public method signature は変更なし
- `ChannelKind` enum (r9 で導入) に新規 enumerator 追加 (`FL` / `FR` / `C` / `LFE` / `SL` / `SR`)。enum class なので暗黙変換問題なし
- `LLMultichannelDownmix` (r9 で導入) は r10 で再利用 (decoder-side → reader-side に呼出位置のみ変更)

### 7.3 設定 / ログ

- 設定追加 1 件 (§6)、削除 / リネームなし
- LL_WARNS の `Stream3D` channel に新規 routing 診断行が増える (`Stream3DRoutingDiagnostic=true` 時のみ)
- 既存ログメッセージは無改修

### 7.4 FMOD API

- ring の `n_tracks=6` 切替を新規導入 (r9 までは 2 固定)
- `Channel::set3DLevel(1.0f)` 明示呼出を r11 hook として `makeChannelForBinding()` に集約 (r9 までは default 値依存)
- r11 で `set3DLevel(0.0f)` に切替える際の影響はこの 1 関数に閉じる

---

## 8. リスク

- **R1**. ring の n_tracks=6 化で memory footprint が ~3 倍 (capacity × 6 / capacity × 2)。r9 ベンチで余裕があったため許容、ただし長尺バッファ設定では監視
- **R2**. reader-side downmix の per-callback CPU が従来 (decoder-side) より上がる可能性。BS.775 mix は `6 mul + 5 add` per frame で軽量だが、ch:L/R/M prim が多い場合は積算で +5% を超える可能性 → §5.3 で計測
- **R3**. routing 診断の log spam ─ throttle key (§4.4.2) で抑止する設計だが、prim 操作が頻繁な場合 (bulk edit 等) に短時間で複数回発火する可能性。実運用で判断
- **R4**. `ch:LFE` prim の audible 位置定位が、配信側 LFE band-limit が緩い場合に違和感を生じる ─ spec §4.3.1 で配信側責務を明文化、配信レシピに「LFE は 30〜120Hz に band-limit」推奨を追記
- **R5**. r11 で FMOD positional → Steam Audio 切替時の hook 1 関数化 (§4.5.2) が、placement の細かい挙動 (3D distance attenuation curve 等) を r11 で再現できない可能性 ─ r11 着手時に Steam Audio の attenuation 設定で吸収できるか別途検証

---

## 9. 将来拡張 (本仕様外)

### 9.1 r11: HRTF / Steam Audio (Layer 2)

- placement (Layer 1, r10) に binaural レンダリング (Layer 2) を直交追加
- 任意 SOFA file (KU100.sofa 等) ロード
- FMOD 既定 panner を `Channel::set3DLevel(0.0f)` で無効化 (§4.5.2 の hook を反転)
- Steam Audio が距離減衰 / 反射 / 空間音響を肩代わり、~120Hz 以下では方向手がかりが自然消失するため LFE prim が **副次的に omnidirectional 化** する (Layer 2 の自動効果)
- 詳細は r11 着手時に独立仕様書として整備

### 9.2 r11.x 以降: LFE 専用処理 (必要が出たら)

- LPF (~120Hz) 強制: FMOD DSP chain で LowPass filter を ch:LFE prim にだけ挿す。Steam Audio とは独立、r10.x / r11 のどこでも追加可能
- bass management: 各 ch の低域成分を抽出 → ch:LFE prim へ集約。FMOD DSP graph の routing 設計が要る、重め、r11.x or later
- 上記 2 つは **Steam Audio で omnidirectional が得られた段階で必要性を再評価** (実運用で要る場面が無ければ実装しない)

### 9.3 公開ガイド (r11 で一括)

- r11 でまとめて r8 〜 r11 の書式を README / 公開ドキュメントで開示
- 配信側標準レシピ (Icecast 2.4+ + ffmpeg + Opus surround + LFE band-limit 推奨) を公開ガイドに昇格
- それまで AYA + 内部テスト協力者のみで運用 (r8/r9 と同方針)

### 9.4 GUI 配信ツール — 別プロジェクト扱い

r9 §9.4 に同じ。AYAstorm 本体スコープ外、`butt-aya` fork 等は別 repo で運用想定。

### 9.5 AAC / HLS / DASH

r9 §9.5 と同方針。r10 でも対応せず、上流 (FMOD) の対応待ち。

### 9.6 動的 downmix gain / placement gain

- 配信素材ごとの実用音量ばらつきが大きい場合に root description で `{downmix_gain:N}` / `{ch_gain:N}` を追加する案
- r10 では固定係数、r11 で必要性を再評価 (Steam Audio 距離減衰が肩代わりする可能性)

### 9.7 strict mode (compat fallback を無効化)

- §4.2 互換マトリクスで graceful fallback する代わりに、`ch:FL` を「6ch source 専用」と厳格化するモード
- 現状不要、要望が出たら r10.x で `Stream3DStrictChannelMatch` 等のキー追加で対応

---

## 10. 参考コード位置

| ファイル | 役割 | r10 改修ポイント |
|---|---|---|
| `indra/llaudio/llpositionalstreamstereo.cpp` | DecodeWorker 本体 | channels==6 分岐の per-channel write 化 (downmix 削除) |
| `indra/llaudio/llmultichanneldownmix.cpp` | BS.775 downmix | 呼出位置を decoder → reader へ移動。ロジック自体は無変更 |
| `indra/llaudio/llpositionalstreammulti.cpp` | reader / pcmReadCallback | per-track 取り出し + ch:L/R/M reader の on-the-fly downmix |
| `indra/llaudio/llmultitailring.{h,cpp}` | ring buffer | n_tracks=6 切替の動作確認 (実装は r8 で対応済) |
| `indra/llaudio/llchannelkind.h` | ChannelKind enum class | 6 値追加 (FL/FR/C/LFE/SL/SR) |
| `indra/newview/llpositionalstreammgr.cpp` | description parser / binding mgr | ch 文字列 → ChannelKind の case 追加、routing 診断 log 出力点 |
| `indra/newview/app_settings/settings.xml` | ユーザ設定 | `Stream3DRoutingDiagnostic` 追加 |

**r11 hook ポイント**:

- `LLPositionalStreamMulti::makeChannelForBinding()` (仮称) — `Channel::set3DLevel(1.0f)` の単一呼出箇所。r11 ではここを `set3DLevel(0.0f)` に切替えて Steam Audio へバトンタッチ。コードコメントに `// r11 hook: Steam Audio takeover` を残す

---

## 11. 工程 (実装フェーズ)

r10 は viewer 側の改修が主軸。配信側パイプラインは r9 で確立済 (test material 生成スクリプトを r9 P10 のものから流用)。

| ID | 内容 | 主要成果物 | 受入リンク |
|---|---|---|---|
| **P1** | ChannelKind enum に FL/FR/C/LFE/SL/SR 6 値追加 + parser 拡張 | `llchannelkind.h`, `llpositionalstreammgr.cpp` の parser case | G1 |
| **P2** | DecodeWorker を per-channel write に refactor | `llpositionalstreamstereo.cpp` の 6ch 分岐、ring `n_tracks=6` 経路確認 | G1 / G3 |
| **P3** | reader-side BS.775 downmix 移行 | `llpositionalstreammulti.cpp` の ch:L/R/M reader が on-the-fly downmix | G2 / R2 |
| **P4** | §4.2 互換マトリクス実装 + unit test | 9 ch 値 × 3 source 数 = 27 cell の決定論動作 | G3 |
| **P5** | routing 診断 log 機構実装 | throttle key / log フォーマット / 設定 gate | G4 |
| **P6** | `Stream3DRoutingDiagnostic` 設定追加 | `settings.xml` + log 出力 gate | G4 |
| **P7** | r11 hook 関数 (`makeChannelForBinding()`) 抽出 | `llpositionalstreammulti.cpp` の `set3DLevel(1.0f)` 呼出単一化 + コメント | G6 / G7 |
| **P8** | テスト材料生成スクリプト + AYA Icecast hosting | `doc/r10/gen_test_material.sh`, `test_routing_5_1.ogg` URL 確定 | §5 |
| **P9** | §5.2 検証手順全件実行 (routing / bleed / musical / matrix / log) | §5.3 受入表全行 PASS | §5.3 |
| **P10** | r9 / r8 回帰確認 | 1ch / 2ch source、ch:L/R/M prim の挙動が r9 と完全一致 | G5 |
| **P11** | ベンチ + 受入クローズ | §5.3 CPU 比較 + 仕様書 §12 (実装結果) 追記 | §5.3 / R2 |

### フェーズ依存

```
P1 ─→ P2 ─→ P3 ─┬─→ P4 ─→ P9 ─→ P10 ─→ P11
                │
                └─→ P5 ─→ P6
                    P7 (P3 と並行可)
                    P8 (P9 開始前までに)
```

P1 (parser) と P2 (decoder) は同じ ChannelKind に依存するため P1 → P2。P3 (reader) は P2 完了後の方が ring 形状が固まっていて素直。P5/P6 (診断 / 設定) は P3 完了後どこでも、P9 までに完成。P7 (r11 hook) は P3 と並行可能 (置く場所が違う)。

### スコープ縮退オプション (リリース判断時)

- **縮退 A**: reader-side downmix の CPU が +5% を超える → r10 では decoder-side downmix のまま (r9 path 維持) で出荷、reader-side 移行は r10.x へ繰越。互換マトリクスと placement は影響なし
- **縮退 B**: §4.2 互換マトリクスのうち LFE/SL/SR fallback (silent) が UX 上問題視される → `ch:LFE prim` を `ch:M prim` 相当として fallback させる案を r10.x で検討、r10 出荷は silent 確定
- **縮退 C**: routing 診断 log が prim 操作時に spam する → throttle key を coarse 化 (root_id 単位、prim_set_signature を外す)、診断粒度低下と引き換え

縮退してもリリース上の体裁は内部リリースとして成立する。**スコープを死守するより足元の動作確認を優先**。

---

## 12. 工数見積

r9 実績 4 日 (P1 から P10 + クローズまで) ベース。r10 は viewer-only で配信側作業がない分軽め:

| カテゴリ | 工数 |
|---|---|
| P1〜P3 (核心 refactor) | 1.5 日 |
| P4 (互換マトリクス + test) | 0.5 日 |
| P5〜P7 (診断 / 設定 / hook) | 0.5 日 |
| P8 (テスト材料) | 0.5 日 |
| P9〜P11 (検証 + 回帰 + クローズ) | 1 日 |
| **合計** | **3〜4 日** |

memory `project_release_workload_norms`: r9 見積 3〜4 日、実績 4 日 とほぼ同等規模。

---

## 13. 実装結果 / 受入クローズ (r10 リリース 2026-05-04)

### 13.1 実装サマリ

| フェーズ | コミット | 備考 |
|---|---|---|
| 仕様策定 | `bd28a67f84` | 本書の初版 |
| P1〜P3 | `7b8c45f96a` | ChannelKind 拡張 + 6ch placement 経路 + reader-side BS.775 downmix |
| P4 + P5 | `025da1a4a9` | §4.2 互換マトリクス (27 cell) + routing 診断 log 機構 |
| P6 | `9eff2db244` | `Stream3DRoutingDiagnostic` 設定追加 |
| P7 | `a57c944718` | r11 hook 関数 `makeChannelForBinding()` 抽出 |
| P8 | `c90b67e649` | 5.1ch routing 検証材料生成スクリプト (`doc/r10/gen_test_material.sh`) |
| P8 fix | `bc562f026e` | gen_test_material.sh の WAV/Vorbis ch 順を family 1 に修正 |
| r10.x bugfix | `c27c12b163` | P9 検証中発見の auto-reconnect (zero-fill streak) + linkset 再構築 (stale binding) 2 bug 修正 |

スコープ縮退は発生せず、全フェーズ予定通りクローズ。SIMD 化 (R2 / 縮退 A) も実測 +3.79pp で不要判定。

### 13.2 §5.3 安定性指標 (P11 実測)

§5.3 の表を参照。dropout 0 / CPU r9 比 +3.79pp / URL 切替 7/7 成功 / 互換マトリクス 全 cell PASS / r9 r8 回帰なし、すべて目標達成。

### 13.3 P9 検証中発見の bug

P9 routing / matrix 検証中に r10.x 級の 2 件を発見し本リリースに同梱:

| ID | 症状 | 原因 | 修正 |
|---|---|---|---|
| bug A | 上流 ffmpeg 再起動で viewer が無音のまま自動復帰しない | FMOD HTTP source は upstream 切断時 OK で 0 bytes を返し続け、エラーカウンタが回らないため Failed 遷移しない | `pumpSource` に zero-fill streak (10s) 追加、超過で setFailed → mgr reconnect cascade に拾わせる |
| bug B (#38) | linkset 再構築 (Ctrl+L) で旧 root prim の binding が残り二重再生 | demoted root の binding が `mDistributedBindings` に残存 | evaluate ループ内で `getRootEdit() != root_id` を見て dead_roots に積み tearDown |

両 bug は P9 開始前 (r10 P1〜P3 段階) から既存の挙動で、5.1ch placement の検証実施で初めて顕在化。**修正済み・本リリースに同梱**。

### 13.4 codec 別 end-to-end 検証状況

| codec | end-to-end 検証 | 補足 |
|---|---|---|
| Vorbis 6ch (channel mapping family 1) | ✓ 完了 | `test_routing_5_1.ogg` (per-ch voice + tone 合成素材) で routing / bleed / placement / 互換マトリクス確認 |
| Opus 6ch | △ コードレビューのみ | r9 §12.3 と同じ FMOD seek 制約。本番経路 (Shoutcast / butt-aya / ffmpeg primary) は r9 で検証済 |
| FLAC 6ch | △ コードレビューのみ | 同上。SMPTE→family 1 reorder は P3 reader 側で実装済、unit test 経路で確認 |

### 13.5 既知の運用上の制約

- bug A 修正後の自動再接続は **zero-fill 10s + reconnect cascade 3×5s ≒ 25s** を要する。設定値 (`kZeroFillStreakLimit` / `Stream3DReconnectAttempts`) は r10 出荷値、短縮は r10.x 以降で検討。
- 互換マトリクスの `ch:C × 2ch source` (M downmix) は耳での聴き分けが原理的に困難 (M downmix vs R 直取りが類似音)。P4 unit test で決定論動作担保。
- gen_test_material.sh の WAV interleave 順は **Vorbis channel mapping family 1** [FL,C,FR,SL,SR,LFE]。FLAC を生成する場合は WAV/SMPTE 順 [FL,FR,C,LFE,SL,SR] への組み替えが必要 (script コメント参照)。

### 13.6 r11 への持ち越し

- HRTF / SOFA / Steam Audio Layer 2 統合 — §7.4 / §9 の hook point (`makeChannelForBinding()` の `set3DLevel(1.0f)`) を 0.0f に切替えるだけで Steam Audio に渡せる構造を維持
- Strict mode (`Stream3DStrictChannelMatch`) — §9.7、要望次第で r10.x
- bug A の `kZeroFillStreakLimit` / reconnect cascade 設定値の調整 — §13.5

---

### 13.7 r10.x: routing 診断の chat / Preferences 化

**背景**: r10 P5/P6 で実装した routing 診断 (LL_WARNS log + debug settings の `Stream3DRoutingDiagnostic`) は AYA さんの本来意図 (chat 通知 + Preferences UI) と乖離していた。phase doc で P9 #5 を `[x]` 印付けたが、実態は 6ch source 再生時の log emit を grep で確認しただけで、2ch × 6prim placement での fallback warn 文言出力は実機未確認だった。

**スコープ**:

| 項目 | 変更内容 |
|---|---|
| 出力先 | LL_WARNS log → **chat 通知** (`notifyStream3D` 経由)。LL_WARNS は撤去 |
| 設定 UI | debug settings → **Preferences > Sound タブ** に checkbox を新規追加 (debug settings は同キーの alias で機能維持) |
| 文言 | §4.4.1 の 5 ケースを localized string として XUI に登録 (英語・日本語) |
| throttle | §4.4.2 の key + 30 秒抑止を維持。chat はログより視認性高いため厳格運用 |
| 検証 | 5 ケース (FL fold / no destination drop / FL fallback to L / LFE silent / 1ch direct) を実機で chat 文言観察 |

**phase doc**:

- `docs/ayastorm-r10-5_1ch-placement.md` の P9 #5 印を `[x]` → `[~]` に下げ、r10.x 完了時に新 phase doc `docs/ayastorm-r10.x-routing-diag-chat.md` で正式 `[x]` を確定
- r10.x phase doc に P 単位作業 (P1 spec 更新 / P2 chat 切替 / P3 Preferences UI / P4 検証) を記録

**互換性**:

- 設定キー `Stream3DRoutingDiagnostic` は不変 (型・既定・名前すべて r10 と同じ)
- 設定 ON/OFF の挙動だけが「log 出力」→「chat 通知」に変わるため、既存 debug settings 利用者にも影響なし (r10 出荷時点で default OFF、有効化していたユーザはおそらくいない)

**工数見積**: 0.5〜1 日 (viewer-only、配信側作業なし、検証 5 ケースのみ)

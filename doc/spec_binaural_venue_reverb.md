# バイノーラル + 会場残響 仕様書 (r11)

> **対象**: AYAstorm `v7.2.5-ayastorm-r11` (想定)
> **前提**: `doc/spec_5_1ch_placement.md` (r10 5.1ch placement 仕様)
> **前提**: `doc/spec_5_1ch_source.md` (r9 5.1ch ソース受入)
> **前提**: `doc/spec_distributed_stereo.md` (r8 分散記述ステレオ)
> **背景文書**: `docs/ayastorm-positional-stream.md` (M1〜Post-M9 実装記録)
> **roadmap 整合**: `docs/ayastorm-stream3d-roadmap.md` (本書策定で Layer 2 の中身を Steam Audio から venue convolution reverb + lite-HRTF へ再定義)

## 1. 目的とスコープ

r11 は r8 〜 r10 で確立した **Layer 1 (placement)** に対して **Layer 2 (rendering)** を載せる初のリリース。Layer 2 の役割は「ある方向から音が来ることをどう耳で感じさせるか」「会場に居る感をどう作るか」の 2 つ。

roadmap (`ayastorm-stream3d-roadmap.md`) では当初 Layer 2 = Steam Audio + 任意 SOFA (KU100 等) を想定していたが、AYAstorm の主用途 (virtual live venue / ヘッドホン視聴) で**実際に効く順序**を再評価した結果、

1. **早期反射 / 残響 (会場感)** ─ 「ホールに居る」体験の主成分。スピーカー視聴でも効く
2. **ITD / ILD (両耳差)** ─ 水平定位の主役。FMOD 既定 panner は ILD のみで ITD なし、ここが薄い
3. **HRTF (個人 SOFA)** ─ 上下/前後の曖昧性解消。ヘッドホン専用、個人差が大きく非個人 SOFA は逆効果になる場合あり

の優先順位 (1 ≫ 2 > 3) と判断した。投資対効果から r11 は **1 と 2 のみ** を実装し、SOFA per-source HRTF (3) は r12 以降に降格する。

主目的:

- 6 個前後の placement prim 配置 (r10) に対して、**listener (avatar/カメラ) 中心のヘッドホン体感** を一段引き上げる
- 配置者が venue タグ (`{venue:NAME}`) で会場残響種別を選び、**「素材は dry、viewer 側で venue 着替え」** という運用を成立させる
- 既存配置 (r8 / r10 で既に置かれた全 prim) を、**ユーザの再配置なしで** 自動的にリッチ化する
- r12+ で SOFA per-source HRTF を載せる際の hook point を spec とコードに残す

**設計思想 — 配信者主導モデル**:

r11 は「**配信者がスピーカープリム Desc に書いた表現意図を、listener viewer は忠実にレンダリングする**」モデルで一貫させる。binaural / venue / wet 強度の制御は **すべて配信者がプリム Desc のタグキー (`{venue}` / `{binaural}` / `{wetgain}`) で指定** し、listener 側の Preferences UI 経由の override は提供しない。実装/検証時の独立 toggle が必要な場面のために **debug settings 経由の強制 override** だけは残すが、これは一般 listener 向け機能ではない。SL 内 snapshot / machinima での音響演出のための listener venue override (旧 G5) も廃止する (= SL 内録音/録画では Stream3D の per-channel placement が再現不能なため、listener 側で venue を変える価値が無い)。詳細は §4.5 / NG8 で詳述。

**過去仕様との関係**:

- `doc/spec_positional_stream_audio.md` (r5 改訂) §4 NG1 で「HRTF / バイノーラル処理は FMOD 既存 3D パン (等価強度ステレオ) で十分」と宣言していた。本書はこの NG1 を **明示的に反転** する。理由は §2.3 で詳述。なお r11 で導入するのは ITD/ILD ベースの **lite-HRTF** であり、SOFA 個人 HRTF (per-source 畳み込み) は引き続き r12+ への保留。
- `docs/ayastorm-stream3d-roadmap.md` §3 r11 (Steam Audio + 任意 SOFA) の項目は本書策定で再定義される。spec 確定後に roadmap を更新する (§11 P0)。

非対象 (r11 では触らない):

- 個人 SOFA / KU100.sofa の per-source HRTF 畳み込み — r12+
- Steam Audio 統合 / ジオメトリ食わせ込み (occlusion / 自動 reflection) — r12+
- venue IR のユーザアップロード UI — r11 では bundled IR 固定セット、upload は r11.x 以降
- voice / SFX / ambient world sound への適用 — 3D Stream channel group のみが対象
- スピーカー視聴向け listener 設定 — 配信者が `{binaural:off}` をタグ宣言した場合のみスピーカー対応、listener 側の UI toggle は提供しない
- listener viewer 側からの venue 強制差し替え 一般 UI — 配信者主導モデルに反するため r11 では不採用 (旧 G5)
- SL 内 snapshot / machinima 用途の listener override — そもそも SL 内録音 / 録画で Stream3D の per-channel placement が再現不能 (= 動画は stereo mix で固定) のため、listener 側で venue だけ差し替えても価値が無い。**r11 ではこのニーズ自体を考慮対象外** とする
- Preferences > Sound タブ への新規 UI 追加 — r11 では一般 UI 改修ゼロ、debug settings のみ
- 公開 README / changelog への開示 — 機能成熟後 (r12 以降) に一括

---

## 2. 問題定義

### 2.1 r10 までの体感の限界

r10 で 6ch source の per-channel placement は完成した (`spec_5_1ch_placement.md` §13)。受入指標 (dropout / CPU / routing 決定論) はすべて達成しているが、**音像の体感** には次の薄さが残る:

- FMOD 既定 panner (`FMOD_3D_LINEARSQUAREROLLOFF`) は ILD (左右レベル差) のみを生成。**ITD (左右時間差) は無し**。ヘッドホンで聴くと「左から音が来る感」が「左の耳だけ大きく鳴っている感」に留まる
- 距離減衰は線形二乗で機能しているが、**air absorption (距離 HF rolloff)** は無い。50m 先のスピーカーが 5m 先と同じ明るさで聴こえてしまう
- **会場残響が無い**。dry な素材を dry のまま 6 点から鳴らしているだけで、「ホール」「クラブ」「カテドラル」の体感差が出ない

これらは実空間では物理現象として「無料」で発生するが、SL のヘッドホン視聴ではすべて計算で代替する必要がある。

### 2.2 Steam Audio 案 (旧 r11 案) を採らなかった理由

旧 roadmap (§3 r11) は Steam Audio + 任意 SOFA を想定していた。再検討の結果、以下の理由で本リリースでは採らない:

- **配布負債が大きい**: Win/Mac/Linux の 3 platform で Steam Audio binary を build / 同梱 / 検証が必要 (roadmap RR1 で「Linux が鬼門」と既知)
- **per-source HRTF 畳み込みの CPU 負荷**: 16 spk × HRTF × 4 stream 並走で CPU 想定超のリスク (roadmap RR3)
- **個人 SOFA 問題**: 非個人 SOFA は前後/上下の曖昧化を**増やす**ことがあり、KU100 一択で済まない
- **「reflection」機能を活かしきれない**: SL の世界 mesh を Steam Audio に食わせる経路を新設しないと geometry-based reflection は使えず、機能の半分が遊ぶ
- **convolution reverb で代替可能**: 「会場感」の主成分は早期反射 + 残響で、これは IR convolution で per-venue に解決できる。Steam Audio の geometry simulation を待つ必要なし

旧 r11 案の hook point (`set3DLevel(1.0f)` @ `llpositionalstreammulti.cpp:684`) は本リリースでも引き続き使う。Steam Audio 案を完全に捨てるのではなく r12+ への保留とする。

### 2.3 r5 NG1 反転の根拠

`spec_positional_stream_audio.md` (r5 改訂) §4 NG1 は「HRTF / バイノーラル処理は FMOD 既定 3D パンで十分」と判断していた。当時の前提:

- 配置 prim は **モノラル 1 個 / 親プリム 1 個** の最小構成中心 (placement の概念は r8 で初めて多 prim 化)
- 配信源も基本ステレオ 1 本、6ch / N spk 配置は仕様外
- viewer 側の検証は **avatar 周辺数 m 範囲** での placement 体感に閉じる

その時点の構成では FMOD 既定 panner (ILD のみ + 距離減衰) で実用十分というのは合理的判断だった。

r10 (= 6 spk venue placement) 完成後に前提が変わった点:

- 配置半径が r10 で **5〜30m スケールの会場モデル** に拡張、距離 HF 減衰 (air absorption) の不在が体感的に目立つ
- 6 spk 同時定位で **ITD なし** の弱点 (= 「左から鳴っている」ではなく「左の耳だけ大きい」感) が複数 source の組合せで顕在化
- 主用途が virtual live venue + ヘッドホン視聴に固まり、「会場感の有無」が体験品質の主成分に昇格

つまり r5 NG1 は当時の構成では正しく、r10 で前提が変わった結果として不足になった。本書はこの構成変化を受けた **追加** であり、r5 の判断を遡って否定するものではない。SOFA 個人 HRTF (per-source 畳み込み) は引き続き r12+ への保留 = r5 NG1 のうち「HRTF 不要」部分は今も部分的に活きている。

### 2.4 r12 着手前にやっておきたいこと

r12 で per-source SOFA HRTF を載せる場合に備えて、以下を r11 で済ませる:

- per-channel に DSP を挿入する経路 (FMOD `Channel::addDSP()` 経由) を実装し動作実証
- listener pose (camera/avatar) を DSP の per-frame parameter として渡す経路の確立
- 3D Stream 専用 ChannelGroup の分離 (= voice / SFX を巻き込まずに DSP を gate)

これらが r11 で揃えば、r12 は「lite-HRTF DSP を SOFA HRTF DSP に差し替える」付加作業で済む。

§2.3 で「r5 当時は ILD のみで十分だった、r10 までの構成変化で不足になった」と説明したが、その NG1 反転と並行して、 r12 への hook point を r11 で **同じ DSP 挿入箇所に SOFA HRTF DSP も hot-swap できる構造** で残しておくのが、r12 着手時の手戻りを最小化する条件。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. ヘッドホン視聴で **ITD + ILD ベースの lite-HRTF** が効く (左右定位が「耳元で鳴る感」になる)
- G2. **air absorption (距離 HF rolloff)** が効く (遠いスピーカーが暗く聴こえる)
- G3. **venue convolution reverb** が動く (bundled IR セットから venue 種別を選んで会場残響が掛かる)
- G4. タグ書式に `{venue:NAME}` / `{binaural:on|off}` / `{wetgain:N}` キーを追加、配置者がプリム Desc 経由で binaural / venue / wet 強度を完全制御できる (配信者主導モデル)
- G5. 実装/検証時の独立 toggle 用に **debug settings 経由の強制 override** を 3 件提供 (Preferences UI には出さない、sentinel 値 = 「タグ通り」を default)
- G6. 既存配置 (r8 / r10) が **タグ無改修で** binaural レンダリングの恩恵を受ける (lite-HRTF は default ON、`{binaural}` 未指定で on 扱い)
- G7. 3D Stream **専用** ChannelGroup を導入し、voice / UI / SFX / ambient world sound に reverb / binaural が漏れない
- G8. r10 の §5.3 受入条件 (dropout / CPU / URL 切替 / 互換マトリクス / 回帰) は **すべて維持**
- G9. r12+ で SOFA per-source HRTF を載せる場合の hook point を仕様書とコードに明記

### 非ゴール

- NG1. SOFA / KU100 / Steam Audio per-source HRTF — r12+
- NG2. world geometry-based occlusion / reflection (Steam Audio simulation) — r12+
- NG3. venue IR のユーザアップロード UI — r11.x 以降
- NG4. voice / SFX / ambient world sound への DSP 適用 — 3D Stream のみ
- NG5. スピーカー視聴向けの cross-talk cancellation — ヘッドホンが主、配信者が `{binaural:off}` をタグ宣言した場合のみスピーカー想定
- NG6. 動的 venue (avatar 位置で reverb が変化) — venue は配置者宣言の固定種別
- NG7. 多階層 reverb / convolution chain — single-stage IR convolve のみ
- NG8. **listener 側 UI 設定 / Preferences > Sound タブ追加** — r11 では一般 UI 改修ゼロ、debug settings 3 件のみ。listener viewer 側からの venue 強制差し替え 一般 UI も提供しない (= 配信者主導モデル)。SL 内 snapshot / machinima のニーズは「Stream3D の per-channel placement が録画で再現不能」のため考慮対象外
- NG9. 公開 README / changelog 開示 — r12+ で一括

---

## 4. 設計

### 4.1 タグ書式 — `venue` / `binaural` / `wetgain` キー追加

r8 で確立した親プリム書式に **値の集合ではなく新キー** として 3 種を追加する。いずれも親プリム (= 音源宣言) にだけ意味がある。子プリム (placement) には書かない。

```
[3dstream-stereo:{url:http://example/sample_5_1.ogg}{venue:hall_medium}{binaural:on}{wetgain:1.2}]
```

#### 4.1.0 新規タグキー一覧

| キー | 値域 | default (未指定時) | 意味 |
|---|---|---|---|
| `{venue:NAME}` | §4.1.1 表参照 (9 種) | `dry` | 会場残響種別 |
| `{binaural:on\|off}` | `on` / `off` (大文字小文字区別なし) | `on` | lite-HRTF DSP の有効化。配信者が「既に binaural-encoded な配信源を二重処理しない」「全 listener にスピーカー視聴互換で聴かせる」等の判断で `off` を選べる |
| `{wetgain:N}` | F32、`0.0` 〜 `2.0` (clamp) | `1.0` | venue reverb の wet 強度倍率。1.0 で IR のオリジナル強度。配信者が会場の「派手さ / 控えめ」を曲ごとに調整 |

`{binaural}` は親プリム属性 (= venue / wetgain と同列)。**子プリムに書いても無視** される (子プリム属性は r8/r9/r10 で確立した `{ch}` `{range}` `{volume}` 系のみ)。

#### 4.1.1 venue 許容値

許容値 (大文字小文字区別なし、parser は `LLStringUtil::toLower` で正規化):

| 値 | 想定用途 | IR 想定特性 |
|---|---|---|
| `dry` (= 省略時 default) | dry 再生、reverb 無し | — |
| `room_small` | 6〜10 畳の部屋 | RT60 ~0.3s |
| `room_medium` | 練習室 / 小ホール | RT60 ~0.6s |
| `hall_small` | 小規模ライブハウス | RT60 ~1.0s |
| `hall_medium` | ホール (300〜1000 席相当) | RT60 ~1.5s |
| `hall_large` | 大ホール | RT60 ~2.0s |
| `club` | クラブ / ダンスフロア | RT60 ~0.8s, dense |
| `cathedral` | カテドラル | RT60 ~3.0s |
| `outdoor` | 野外、軽い early reflection のみ | RT60 ~0.2s |

不正値 (`venue:foo` 等) は r8/r9/r10 と同じく **silent ignore + warn 1 回 (chat 通知)**。chat 通知は r10.x で確立した `notifyStream3D()` helper 経由 (前置「3D Stream: 」+ throttle + Preferences 表示 gate、`docs/ayastorm-r10.x-routing-diag-chat.md` 参照)。venue 関連の通知種別:

- `unknown venue name: <name>` (タグの `{venue:NAME}` が許容値外、§4.1 表に無い名前)
- `venue IR file not found: <path>` (起動時 / venue 切替時に IR が読めない、ファイル欠損 / corrupt)
- `venue IR sample rate / length out of range: <name>` (48kHz resample 失敗 / 3s 超過)

throttle key は `venue:<name>` 単位、30s 抑止 (r10.x 準拠)。

#### 4.1.2 旧 alias での venue 利用

r5 命名整理 (`project_ayastorm_r5_naming.md` / `docs/ayastorm-r5-naming-refactor.md`) で確立した恒久 alias `[ayastream-stereo:...]` でも venue / binaural / wetgain キーは効く。

```
[ayastream-stereo:{url:http://example/sample_5_1.ogg}{venue:hall_medium}]    ← OK (r11)
[3dstream-stereo:{url:...}{venue:hall_medium}]                                ← OK (canonical)
```

新規 description 書く際は `[3dstream-stereo:...]` 推奨 (canonical)、ただし旧 description が `[ayastream-stereo:...]` のまま残っていても r11 viewer は同等に解釈する (= 配置者再編集を強制しない)。後方互換維持は r5 で約束した範囲。

#### 4.1.3 設計判断: `venue` / `binaural` / `wetgain` を子プリムに書かなかった理由

子プリムごとに変える案 (= speaker 単位の reverb 種別 / binaural 切替) も検討したが棄却。

- 物理的に「同一会場内のスピーカーごとに残響種別が違う」状況は存在しない
- 6 prim placement で各 prim が異なる venue を宣言したらどう mix するかルール追加が必要
- binaural は listener 中心の rendering 性質、source 単位ではなく audio engine 全体で 1 つのモード
- 配信源 1 本につき venue / binaural / wetgain 1 組、という対応が ライブ会場モデル (`spec_5_1ch_placement.md` §4.3) と整合

これら 3 キーは **音源宣言の属性**であり、placement (子プリム) の属性ではない。

### 4.2 viewer 内部経路

```
HTTP source URL
  ↓ (r10 までと同じ decode + ring + per-channel reader)
N 個の FMOD::Sound (OPENUSER, 1ch each) + FMOD::Channel
  ↓ Channel::set3DLevel(0.0f)         ← r11 hook 反転 (FMOD 既定 panner OFF)
  ↓ Channel::addDSP(LiteHrtfDsp)       ← r11 新規 DSP (per-channel)
LiteHrtfDsp (mono in → stereo out)
  ├─ source pos / listener pose を per-frame param で受け取る
  ├─ distance attenuation (linear-square、r10 までの set3DMinMaxDistance を移植)
  ├─ air absorption (距離 HF rolloff、1 次 IIR shelf)
  ├─ ITD (sample-fractional delay) — 左右で interaural delay を差をつける
  └─ ILD (contralateral HF shadow) — 反対側の耳に 1 次 IIR low-shelf
  ↓
Stream3D ChannelGroup
  ↓ ChannelGroup::addDSP(VenueReverbDsp)  ← r11 新規 DSP (group 末尾)
VenueReverbDsp (stereo in → stereo out)
  ├─ 選択された venue IR (stereo, ≤ 3s, 48kHz) を partitioned FFT convolve
  ├─ wet/dry mix (default wet=1.0、dry はバイパス、master は両方の合成)
  └─ venue=dry 指定時は完全バイパス (CPU ゼロ)
  ↓
Master ChannelGroup (= 既存の voice / SFX / ambient と同列)
```

#### 4.2.1 r11 hook の反転

r10 で挿入した `Channel::set3DLevel(1.0f)` (`llpositionalstreammulti.cpp:684`) を **`0.0f` に反転**。これにより:

- FMOD の 3D 距離減衰 / 既定 panner が **per-channel で完全 OFF**
- 距離減衰 / 定位は LiteHrtfDsp が肩代わり
- `set3DMinMaxDistance(1.0f, range)` の値は LiteHrtfDsp の param として読み出して再利用 (linear-square の式は r10 と完全一致を狙う)

配信者タグ `{binaural:off}` のとき、または debug `Stream3DBinauralRender=0` のときは `set3DLevel(1.0f)` のまま (= r10 と完全同一動作)、DSP は挿入しない。判定は `effectiveBinaural()` 合成 getter で行う (= debug sentinel `-1` ならタグ値、それ以外なら debug 値が優先)。

#### 4.2.2 Stream3D ChannelGroup の分離

r10 までは 3D Stream の per-speaker channel が **どのグループにも明示的に属さない** 状態で master 直結だった。r11 では:

- 起動時に `Stream3D` という ChannelGroup を作り、`makeChannelForBinding()` で `Channel::setChannelGroup(stream3DGroup)` を呼ぶ
- VenueReverbDsp はこの group の出力 DSP として 1 つだけ挿入
- voice / UI / ambient world sound は別 group (既存) なので reverb の影響を受けない

これは r10 で意図せず master 直結になっていた仕様の正常化でもある。

#### 4.2.3 Thread モデル (r7 3-thread の継承)

r7 (`spec_stream3d_decode_thread.md`) で確立した 3-thread モデルを r11 でも崩さない:

| Thread | 役割 (r7 から不変) | r11 で追加される責務 |
|---|---|---|
| **main thread** | start/stop/pause、tag parse、placement 判定、UI、設定変更 | LiteHrtfDsp の per-frame param push (listener pose、source pos、range)、VenueReverbDsp の venue 切替 |
| **decode thread** | HTTP fetch → libvorbis/libopus/libFLAC decode → ring buffer 投入 | 変更なし (r11 の DSP は decode 経路に介入しない) |
| **FMOD mixer thread** | ring → OPENUSER pcmreadcallback → channel mix → output | LiteHrtfDsp `read` callback (per-channel)、VenueReverbDsp `read` callback (group 末尾) を実行 |

**Param 受け渡し**:

- main → mixer 方向 (per-frame param) は **lock-free single-writer atomic** で実装。`std::atomic<F32>` を listener pose / source pos / range / venue index 等の各フィールドに 1 個ずつ持つ。読み手 (mixer thread) は古い値を 1 frame 読む可能性があるが、視聴上の影響なし (= 数 ms 遅延)
- mixer → main 方向の通信は **不要** (r11 では DSP から main thread へ通知すべき情報なし)
- mutex / condition variable は使わない (mixer thread の jitter 増加を避ける、r7 P5 と同方針)

**Shutdown 不変条件 (r7 から継承、r11 で追加なし)**:

1. main: `cleanupStreams()` で start を止める → decode thread に stop 通知 → decode thread join
2. main: FMOD `Channel::stop()` → `Sound::release()` → `ChannelGroup::release()`
3. main: `LLLiteHrtfDsp` / `LLVenueReverbDsp` インスタンス破棄 (FMOD が自動 detach する)

DSP インスタンスは Channel / ChannelGroup より先に破棄してはいけない (FMOD の DSP graph が dangling pointer を抱える)。逆順は OK。これは r7 「decode → FMOD」の release 順を 1 段拡張しただけ。

**rebuild / venue 切替時**:

- venue 切替 (タグ live 変更) は main thread が VenueReverbDsp の `venue index` atomic を書き換える → mixer thread が次 callback で IR slot を切替 → 200ms crossfade で繋ぐ (R7)
- IR ロード自体は main thread で事前に全 venue 分済ませて RAM 上に持つ (= 切替時の disk I/O ゼロ)
- ChannelGroup 自体の re-create は不要 (DSP の param 切替で完結)

### 4.3 LiteHrtfDsp の処理

#### 4.3.1 入力 / 出力

- 入力: 1 ch mono PCMFLOAT (FMOD OPENUSER の出力そのまま)
- 出力: 2 ch stereo PCMFLOAT (FMOD_CHANNELMASK_STEREO)
- DSP 形式: `FMOD_DSP_DESCRIPTION` の `read` callback (`Wind Unit` と同じ枠組み、`llaudioengine_fmodstudio.cpp:542-583` 参照)

#### 4.3.2 per-frame parameter

DSP に毎フレーム main thread から push する param:

| param | 型 | 内容 |
|---|---|---|
| `source_pos` | LLVector3 | speaker world 座標 (mSpeakers[i].position) |
| `listener_pos` | LLVector3 | gAudiop->getListener (camera or avatar、§ FS:AYA toggle) |
| `listener_at` | LLVector3 | listener forward axis |
| `listener_up` | LLVector3 | listener up axis |
| `min_dist` | F32 | 1.0f (固定) |
| `max_dist` | F32 | mSpeakers[i].range |

DSP 側で azimuth / elevation / distance を計算する (mixer thread)。スレッド境界は parameter の `lock-free single-writer` で済むので mutex 不要。

#### 4.3.3 距離減衰

r10 までと同じ式を再現:

```
gain = 1.0f                                     (d ≤ min_dist)
gain = 1.0f - ((d - min_dist) / (max_dist - min_dist))²    (min_dist < d ≤ max_dist)
gain = 0.0f                                     (d > max_dist)
```

= `FMOD_3D_LINEARSQUAREROLLOFF` の挙動。これにより `Stream3DBinauralRender` の on/off で距離減衰の体感差が出ないことを保証 (G9)。

#### 4.3.4 Air absorption

距離 d に応じて HF を緩やかに削る 1 次 IIR low-shelf:

```
shelf gain (dB) = -0.5 * d   (d ≤ 50m, cap at -25 dB)
shelf knee = 4 kHz
```

**Why 0.5 dB/m**: 物理的な大気吸収係数 (50% 湿度 / 20°C / 1 atm) は 4 kHz で約 -10 dB/100m = -0.1 dB/m だが、SL の体感距離スケールでは過小。`-0.5 dB/m` は roadmap の体感主義 (実物 1:5 スケール) で実用化。実機検証で再調整可能。

#### 4.3.5 ITD (interaural time delay)

水平面の azimuth θ (radian、source から listener への vector を listener 平面に projection) から:

```
ITD = (a / c) * (sin(θ) + θ)        (Woodworth-Schlosberg approximation)
  a = 0.0875 m  (頭半径)
  c = 343 m/s
```

最大 ITD ~0.66 ms = 31.5 samples @ 48kHz。

実装は左右両耳に別々の **fractional sample delay line** (Lagrange interpolation 3rd order) を配置。負側 (= 同側耳は 0 delay、反対側耳のみ delay) で実装。

#### 4.3.6 ILD (interaural level difference) と shadow

水平 azimuth から:

- 同側耳: gain 1.0
- 反対側耳: gain `cos(θ/2)^0.5` (broadband) + 1 次 IIR low-shelf で 2 kHz 以上を最大 -6 dB shadow

**Why minimal**: 過度な shadow は不自然、軽めから始めて実機で調整。

#### 4.3.7 elevation の扱い

r11 では elevation cue を意図的に薄く扱う。SOFA HRTF 無しに elevation を ITD/ILD だけで説得力ある形で表現するのは困難で、不自然な誤定位の元になる。

- elevation > +30°: わずかな broadband HF emphasis (+1 dB)
- elevation < -30°: わずかな broadband HF rolloff (-1 dB)
- それ以外: フラット

この粒度で十分。本格的な elevation cue は r12+ の SOFA で扱う。

### 4.4 VenueReverbDsp の処理

#### 4.4.1 入力 / 出力

- 入力: 2 ch stereo (Stream3D group の合算出力)
- 出力: 2 ch stereo
- DSP 形式: `FMOD_DSP_DESCRIPTION` の group 末尾 DSP

#### 4.4.2 IR フォーマット

- WAV 32-bit float または 16-bit PCM
- 1 ch (mono IR、stereo に複製) または 2 ch (true stereo IR)
- サンプルレート: 任意 (起動時に viewer の出力レート 48kHz に linear resample)
- 長さ上限: **3 秒** (= cathedral の RT60 余裕、48kHz で 144,000 samples)

#### 4.4.3 Convolution 実装

partitioned FFT convolution (uniform partition):

- block size: 1024 samples (mixer callback の典型値)
- 3s IR / 1024 = 141 partitions
- per-block: 2× FFT (input + 1 partition advance) + complex multiply-accumulate + 1× IFFT
- KissFFT または FMOD 内蔵 FFT を流用 (要調査、なければ minimal KissFFT を vendor)

CPU 見積: stereo 3s IR で ~3-5% (CPU 4 GHz × 1 core) と想定。実機計測で確認 (R3)。

#### 4.4.4 wet / dry

- 配置者タグ `{venue:dry}` または debug settings `Stream3DVenueOverride="dry"` のとき: DSP を bypass (= CPU ゼロ)
- それ以外: wet を **タグ `{wetgain:N}`** (default 1.0) 倍して dry に加算。debug settings `Stream3DVenueWetGain` が `0.0` 以上なら **タグ値を上書き** (sentinel `-1.0` = タグ通り)
- dry signal は DSP 内で完全 bypass 出力に保持 (= zero-latency dry)

#### 4.4.5 IR ファイルのバンドルと配置

- 配置先: `{viewer install}/app_settings/venue_ir/<name>.wav`
- 参考 IR ソース (Apache 2.0 / CC0 / Public Domain のみ採用):
  - **OpenAIR** (https://www.openair.hosted.york.ac.uk/) — CC-BY 4.0、要 attribution
  - **EchoThief** (http://www.echothief.com/) — CC-BY 3.0、要 attribution
  - **MIT IR Survey** — CC-BY 3.0
- ライセンス確認の上、各 venue IR の出典を `app_settings/venue_ir/CREDITS.md` に明記
- 全 9 種で合計 ~30 MB (3s × 9 venue × stereo × 32-bit = 推定値) を viewer install に同梱

実機検証の結果次第で 9 種を絞る (例: small/medium/large/cathedral/outdoor の 5 種で十分なら削減)。

### 4.5 debug settings 経由の強制 override (= 平時は不使用)

平時は **配信者がプリム Desc に書いたタグキー (`{venue}` / `{binaural}` / `{wetgain}`) が root truth** として動作する。listener viewer は自動的にタグから値を読み出し、Preferences UI から override する経路は提供しない (NG8)。

実装/検証時の独立 toggle / debug 用途のみ、debug settings 経由の **強制 override** を 3 件提供する。各 default 値は **「タグ通り」を意味する sentinel** に固定し、debug settings を触らない一般 listener は配信者意図そのままで聴ける。

| キー | 型 | sentinel (= タグ通り) | 強制値の効果 |
|---|---|---|---|
| `Stream3DBinauralRender` | int | `-1` (default) | `0` = lite-HRTF を強制 OFF (= FMOD 既定 panner で r10 同等動作) / `1` = 強制 ON (タグ `{binaural:off}` を無視) |
| `Stream3DVenueOverride` | string | `""` (default) | venue 名 (`dry` / `hall_medium` 等) を listener 側で固定。`"dry"` で reverb DSP 完全 bypass |
| `Stream3DVenueWetGain` | F32 | `-1.0` (default) | `0.0` 以上で wet 強度を強制 (タグ `{wetgain:N}` を上書き) |

**動作優先順位** (先に評価される側ほど強い):

1. debug settings に強制値あり (sentinel 以外) → 強制値を採用
2. プリム Desc タグ (`{venue}` / `{binaural}` / `{wetgain}`) → タグ値を採用
3. タグ未指定 → §4.1.0 表の default (dry / on / 1.0)

**listener viewer 側からの venue 強制差し替え 一般 UI は r11 では提供しない** (NG8)。SL 内 snapshot / machinima での音響演出のために listener が venue を上書きしたい、というニーズは spec 上で **考慮対象外** とする。理由は SL 内録画/録音では Stream3D の per-channel placement が再現できず (= 動画は stereo mix で固定)、listener 側で venue だけ差し替えても価値が発生しないため。これは r10 / `FSRenderHideOutsideParcel` の「利用者側設定は任意の区画で有効」とは別判断で、Stream3D は配信者主導モデルを r11 で確立する。

### 4.6 Listener pose (camera / avatar 切替) の整合

r11 では現状の `gAudiop->setListener()` 経路 (`llvieweraudio.cpp:592` の FS:AYA toggle) をそのまま再利用する:

- `0` (default): listener pos = camera pos / camera at axis
- `1`: listener pos = avatar pos / avatar at axis

LiteHrtfDsp は per-frame param として常に「現在の listener pose」を受け取る。撮影モードで camera と avatar が乖離する場合は **camera pose 優先** が既定。avatar 視点を選んだユーザは `1` 設定で切替えればそのまま binaural もアバター視点になる。

---

## 5. 検証材料

### 5.1 binaural 検証材料 — pink noise + voice + tone のハイブリッド

ITD / ILD / air absorption のそれぞれが分離して聞き取れる素材:

| 素材 | 用途 |
|---|---|
| `pink_noise_30s.ogg` (mono 1ch) | air absorption (HF rolloff) を距離変更で聴取 |
| `voice_panning_test.ogg` (mono 1ch、"left ... center ... right" 連呼) | 単独 source の placement で頭の向きを 360° 動かす |
| `click_train_440Hz.ogg` (mono 1ch、20ms tick × 1Hz) | ITD の sample-level な聴感確認 (sharp transient) |

生成パイプライン (r10 P8 流用):

```bash
# pink noise
sox -n -c 1 -r 48000 pink_noise_30s.wav synth 30 pinknoise
# voice
espeak-ng -v en -w voice_panning_test.wav "left ... center ... right ... left ... center ... right"
# click train
sox -n -c 1 -r 48000 click_train_440Hz.wav synth 30 sine 440 vol 0.3 fade 0 30 0
# Vorbis encode
oggenc --quality 5 -o pink_noise_30s.ogg pink_noise_30s.wav
oggenc --quality 5 -o voice_panning_test.ogg voice_panning_test.wav
oggenc --quality 5 -o click_train_440Hz.ogg click_train_440Hz.wav
```

スクリプトは `doc/r11/gen_test_material.sh` として repo に置く。

### 5.2 venue 検証材料

r10 で使った `sample1_6ch.ogg` (musical content) を流用。各 venue 種別を切り替えて主観評価:

- `dry` → `room_medium` → `hall_medium` → `cathedral` の順で「会場感」が段階的に増えるか
- `outdoor` で「外」感 (residual ambience がない open feel) が出るか
- `club` で dense reflection (close-miked PA 感) が出るか

### 5.3 検証手順

1. **binaural 単体検証 (lite-HRTF、配信者タグ駆動)**:
   - `voice_panning_test.ogg` を 1 prim mono 配置 (avatar 前方 5m)、タグ未指定 (= `{binaural}` 省略 = on default) で再生
   - avatar を 360° 回転させ、音源方向が頭の動きに自然追従するか確認
   - 同じ素材で配信者がタグを `{binaural:off}` に書き換え (live 反映)、定位の体感差を A/B 比較
2. **air absorption 検証**:
   - `pink_noise_30s.ogg` を 1 prim 配置、avatar が prim から 5m / 25m / 50m に移動して HF が暗くなるか確認
3. **ITD 検証**:
   - `click_train_440Hz.ogg` を avatar 真横 (90°) に配置、左右どちらの耳から click が先に聞こえるか確認 (タグ `{binaural:on/off}` 切替で差が出るはず)
4. **r10 placement との合わせ込み検証**:
   - r10 の 6 prim BS.775 配置 (`test_routing_5_1.ogg`) で `{binaural:on/off}` を切替、6 spk の体感差を確認
5. **venue 検証**:
   - `sample1_6ch.ogg` の 6 prim 配置に対して `{venue:dry/hall_medium/cathedral/outdoor}` を順に切替、会場感の段階差を主観評価
   - venue 切替が live で反映されるか (再生途中のタグ変更で next evaluateLinkset から効くか)
   - `{wetgain:0.5}` `{wetgain:1.5}` 等を切替、wet 強度の段階差を主観評価
6. **debug settings 経由 override 検証**:
   - 配置者が `{venue:hall_medium}` を宣言した状態で、debug `Stream3DVenueOverride="cathedral"` に設定、cathedral 残響が掛かるか
   - debug `Stream3DVenueOverride="dry"` に設定で、配置者宣言があっても dry になるか
   - debug `Stream3DBinauralRender=0` で `{binaural:on}` を強制 OFF、debug `Stream3DBinauralRender=1` で `{binaural:off}` を強制 ON
   - debug `Stream3DBinauralRender=-1` (sentinel) でタグ通りに戻るか
7. **3D Stream 専用 group 検証**:
   - venue 残響を ON にした状態で voice (Vivox / WebRTC) / UI sound (button click) / ambient world sound に reverb が漏れていないか
8. **回帰確認**:
   - debug `Stream3DBinauralRender=0` + `Stream3DVenueOverride="dry"` で r10 までの動作が完全一致 (dropout / CPU / placement / 互換マトリクス)
   - r10 の §5.3 受入条件全行を再実行

### 5.4 Codec 別 end-to-end 検証

r10 §13.4 で確立した「codec 別 end-to-end は Vorbis を主、Opus / FLAC はコードレビュー convention」枠組みを r11 でも踏襲:

| Codec | ch | end-to-end | 備考 |
|---|---|---|---|
| Vorbis | 1, 2, 6 | ✓ 実機聴取 | §5.1 / §5.2 の検証材料は Vorbis を主とする |
| Opus | 1, 2, 6 | コードレビューのみ | r9 / r10 同様、libopus の 6ch decode 経路 / FMOD OPENUSER 投入経路は r9 で確立済、r11 で経路は触らない |
| FLAC | 1, 2, 6 | コードレビューのみ | 同上 |

r11 の DSP 改修は「decode 後 → FMOD Channel」以降の経路にのみ介入するため、codec の違いは DSP 入力側に到達した時点で吸収済 (= 32-bit float PCM mono ch each)。本リリースで codec 別の end-to-end 実機回しは Vorbis のみで足る。Opus / FLAC は P13 回帰確認 (r9 の §5.3) で従来と同経路で通ることだけ確認。

実機検証材料 (`pink_noise_30s` / `voice_panning_test` / `click_train_440Hz`) は **Vorbis (`oggenc --quality 5`) のみ生成**。Opus / FLAC 版を作っても r11 の DSP 検証目的に対しては情報量がほぼゼロ (= decode 経路は r9 で検証済)。

### 5.5 受入条件

| 項目 | 基準 | 実測 (リリース時記入) |
|---|---|---|
| binaural ON で voice_panning 素材の左/中/右が頭の向きに追従 | ✓ PASS | (リリース時) |
| binaural ON で 50m 距離の pink noise が 5m 距離より明確に暗い (-15dB 以上 @ 4kHz) | ✓ PASS | (リリース時) |
| binaural ON で click_train を真横に置いた時の左右耳の click 着信差が知覚可能 | ✓ PASS | (リリース時) |
| 9 venue で会場感の段階差が主観評価で識別可能 (dry/medium/cathedral の 3 段だけでも可) | ✓ PASS | (リリース時) |
| venue 切替が live (再生継続中のタグ変更で次の evaluateLinkset から効く) | ✓ PASS | (リリース時) |
| タグ `{binaural:on/off}` / `{venue:NAME}` / `{wetgain:N}` が live 切替で正しく反映 | ✓ PASS | (リリース時) |
| debug `Stream3DBinauralRender` が `-1`/`0`/`1` の 3 sentinel で正しく動作 (= タグ通り / 強制 OFF / 強制 ON) | ✓ PASS | (リリース時) |
| debug `Stream3DVenueOverride` が空文字 / venue 名 / `"dry"` で正しく動作 (= タグ通り / 強制差し替え / 強制 dry) | ✓ PASS | (リリース時) |
| voice / UI sound / ambient world sound に venue reverb が漏れない | ✓ PASS | (リリース時) |
| debug `Stream3DBinauralRender=0` + `Stream3DVenueOverride="dry"` で r10 との挙動完全一致 (dropout 0、CPU 同等、placement 同じ) | ✓ PASS | (リリース時) |
| binaural ON + reverb ON で 5 分連続再生 dropout 0 | ✓ PASS | (リリース時) |
| CPU r10 比 +10% 未満 (binaural + reverb 両方有効、6 spk 配置時) | ✓ PASS | (リリース時) |
| URL 切替 ×10 (5.1 ↔ 2ch) 全成功 | ✓ PASS | (リリース時) |
| r8 / r9 / r10 の §5.3 全行が回帰なし | ✓ PASS | (リリース時) |

---

## 6. 設定項目

r11 で追加する listener 側設定は **debug settings 3 件のみ** (一般 UI / Preferences タブへの露出はゼロ、NG8)。配信者がプリム Desc に書いたタグキー (`{venue}` / `{binaural}` / `{wetgain}`) が **平時の root truth**、debug settings は実装/検証時の強制 override 用。

| 設定キー | 型 | 既定 (= タグ通り) | 効果 | 設定 UI |
|---|---|---|---|---|
| `Stream3DBinauralRender` | int | `-1` | sentinel `-1` でタグ通り (= `{binaural}` を参照、未指定で on)。`0` = 強制 OFF (FMOD 既定 panner で r10 同等動作)。`1` = 強制 ON (タグ `{binaural:off}` も無視) | debug settings |
| `Stream3DVenueOverride` | string | `""` | sentinel 空文字でタグ通り。venue 名 (`dry` / `hall_medium` 等) を指定で listener 側強制差し替え。`"dry"` で reverb DSP 完全 bypass | debug settings |
| `Stream3DVenueWetGain` | F32 | `-1.0` | sentinel `-1.0` でタグ通り (= `{wetgain}` を参照、未指定で 1.0)。`0.0` 以上で wet 強度を強制 | debug settings |

**設定方針**:

- **一般 listener は debug settings ノータッチで配信者意図そのまま聴ける** (= 全 sentinel で「タグ通り」動作)。`{binaural}` 未指定で on / `{venue}` 未指定で dry / `{wetgain}` 未指定で 1.0 が default で適用される (G6)
- 配置者は `{venue:NAME}` / `{binaural:on|off}` / `{wetgain:N}` をタグ宣言、listener が触らなくても表現が成立 (= 配信者主導モデル)
- スピーカー視聴対応 / 二重 binaural 回避が必要なら **配信者が `{binaural:off}` をタグ宣言**、listener 側 UI toggle は提供しない (NG8)
- 撮影 (snapshot / machinima) 用途の listener venue 上書きは spec 考慮対象外 (NG8、§4.5)
- 設定切替は live (debug settings 変更で次の evaluateLinkset から効く、再起動不要)
- `Stream3DRoutingDiagnostic` (r10) は本書と独立、unchanged

memory `project_prefer_defaults_over_config.md` の方針 (= 多数の tuning キーより 1 つの妥当値) に従い、IR ごとの個別 wet / dry / pre-delay tuning は配信者タグ `{wetgain}` + debug 強制 1 件に集約。一般 listener UI の追加件数はゼロ。

---

## 7. 互換性

### 7.1 タグ

- r8 / r9 / r10 タグ書式は **完全に後方互換**。`{ch:...}` / `{range:...}` / `{volume:...}` / `{url:...}` の挙動は変更なし
- 新規キー 3 種 `{venue:NAME}` / `{binaural:on|off}` / `{wetgain:N}` を r8 / r9 / r10 viewer が受信した場合: 不正キーとして silent ignore (既存挙動)。r11 viewer のみが新規キーを解釈
- 同じ source URL に対して旧 viewer (r8/r9/r10) と新 viewer (r11) が共存可能
- r5 命名整理で確立した恒久 alias `[ayastream-stereo:...]` でも `{venue}` / `{binaural}` / `{wetgain}` は同じく解釈される (§4.1.2)。新規 description 推奨は canonical `[3dstream-stereo:...]` 側だが、旧 alias 利用者の再編集は強制しない

### 7.2 ABI / 公開ヘッダ

- `LLPositionalStreamMulti` の public method signature は変更なし
- `SpeakerConfig` 構造体に変更なし (venue は親プリム属性なので Multi 側は知らなくてよい)
- 新規クラス `LLLiteHrtfDsp`, `LLVenueReverbDsp` を `indra/llaudio/` に追加
- ChannelGroup `Stream3D` の所有は `LLAudioEngine_FMODSTUDIO` (既存 master / SFX group と同列に追加)

### 7.3 設定 / ログ

- listener 側設定追加 3 件 (§6、すべて debug settings、Preferences UI 露出ゼロ)、削除 / リネームなし
- 新規タグキー追加 3 件: `{venue}` / `{binaural}` / `{wetgain}` (§4.1.0)。配信者主導モデルのため、タグ側で全制御
- 新規 `LL_INFOS("Stream3DReverb")` channel で venue ロード結果 / IR ファイル不在を log
- 既存ログメッセージは無改修

### 7.4 FMOD API

- `Channel::set3DLevel(0.0f)` を r10 hook (`makeChannelForBinding()`) で反転 (`effectiveBinaural()` が ON のときのみ)
- `Channel::addDSP(0, lite_hrtf_dsp)` を per-channel で挿入 (`effectiveBinaural()` が ON のときのみ)
- `ChannelGroup::addDSP(end, venue_reverb_dsp)` を Stream3D group で挿入 (`effectiveVenue()` が dry 以外のときのみ)
- `Channel::setChannelGroup(stream3DGroup)` を全 3D Stream channel に呼ぶ (常時)
- `setChannelFormat(FMOD_CHANNELMASK_STEREO, 2, ...)` で DSP の出力 ch 数を変更 (Wind Unit が同パターン)

### 7.5 既存 audio engine への影響

- voice (Vivox / WebRTC) / UI sound / ambient world sound / SFX は別 ChannelGroup (既存)、本リリースで触らない
- Master volume / mute は ChannelGroup ヒエラルキーの最上位で評価されるので Stream3D group の DSP より上で効く (既存挙動)

---

## 8. リスク

- **R1**. lite-HRTF の体感が薄い可能性 ─ ITD/ILD だけでは「ヘッドホンで定位がリッチになった」と感じない場合、shadow filter の急峻化や ITD scaling 拡大で再調整。最悪 binaural OFF を default にする縮退 (§11 縮退オプション A)
- **R2**. air absorption の dB/m 係数が体感とズレる ─ -0.5 dB/m は仮値、実機聴感で再調整。設定キーには出さず内部定数で固定
- **R3**. convolution reverb の CPU が想定超 ─ 3s IR × stereo × partitioned FFT で 5%以下を見込むが、実測で +10pp 超なら IR 上限を 2s に短縮 / mono IR 強制 / partition size 拡大で対応
- **R4**. IR ファイルのライセンス問題 ─ 採用する IR ソースのライセンスを必ず CC-BY 以上で確認、attribution を `CREDITS.md` に明記。再配布禁止のものは絶対に同梱しない (roadmap RR2 と同思想)
- **R5**. ChannelGroup 分離による副作用 ─ 既存 master volume / mute / 全体 setVolume が Stream3D に正しく伝播するか確認。継承関係が誤るとボリューム slider が効かない / mute が効かないなどの致命的 regression。**P1 完了直後にチェックポイントを設けて早期発見** (§11 P1)
- **R6**. listener pose の per-frame 取得経路で thread race ─ DSP は mixer thread、listener pose 更新は main thread。lock-free single-writer atomics で済むが、設計時点で確認
- **R7**. venue 切替の live 反映 ─ 再生中の IR 切替で wave-front 不連続 (click/pop) が起きる可能性。crossfade 200ms で吸収する設計、検証で確認
- **R8**. debug `Stream3DBinauralRender=0` + `Stream3DVenueOverride="dry"` で r10 完全互換を保証する難しさ ─ ChannelGroup 分離は常に行うので、group の DSP が空 (DSP chain が空) でも素通しが完全 bit-identical かを §5.3 で確認

---

## 9. 将来拡張 (本仕様外)

### 9.1 r12: SOFA per-source HRTF (SOFA HRTF DSP)

- LiteHrtfDsp を **同じ DSP 挿入点** に対して SOFA HRTF DSP に差し替える
- KU100.sofa 同梱 + ユーザ任意 SOFA upload UI
- per-channel × HRTF convolution の CPU を §8 R3 と同じ枠組みで評価
- r11 の binaural toggle (タグ `{binaural:on/off}` + debug `Stream3DBinauralRender` int sentinel) を r12 で `{binaural:lite|sofa|off}` 等の enum 化に拡張するか、配信者主導モデルを維持したまま listener が SOFA upload を任意で当てる設計か、を r12 着手時に再検討

### 9.2 r12+: Steam Audio integration (occlusion / geometry reflection)

- 旧 r11 案。Apache 2.0 の Steam Audio plugin を viewer に同梱
- world geometry を Steam Audio に食わせて occlusion / reflection を simulation
- Linux / macOS の binary build 問題 (roadmap RR1) は r12 着手時に再評価
- venue convolution reverb (r11) との二重残響を避けるため、Steam Audio reflection ON 時は convolution reverb を自動 disable する条件分岐が必要

### 9.3 r11.x 以降: venue IR upload UI

- ユーザが任意の WAV IR を viewer から upload してリストに追加
- ファイル名 / IR length / channel count の validation
- upload した IR は `{viewer config}/venue_ir/user/<name>.wav` に保存
- 配置タグの `{venue:user_<name>}` で参照

### 9.4 r11.x 以降: dynamic venue (位置依存残響)

- avatar が venue prim 周辺の特定領域に入ったら自動で venue 種別が変わる
- 例: 屋外から屋内に入ったら `outdoor` → `hall_medium` に crossfade
- 配置者が venue prim の bounding box を宣言する別タグ (`[3dstream-venue:{name:hall}{box:30,20,15}]`) が必要
- 仕様面積が大きいので別 release

### 9.5 r12+: 個人 HRTF measurement / personalization

- ユーザ個人の HRTF を simple measurement (head photo + AI / ear photo upload) で生成
- KU100 一律より大幅に個人差 fit する可能性
- Mesh2HRTF, EAC ear-canal estimator 等の OSS と連携検討
- 仕様面積が広く、本気 release は r13 以降

### 9.6 公開ガイド (機能成熟後 r12+ で一括)

- README / 公開ドキュメントで r8〜r12 の書式と使い方を一括開示
- 配置者向けガイド (venue 種別の選び方、配信側 dry 推奨) を公開
- AYAstorm 個性として「位置音響 + binaural + venue」が立つ位置付け

### 9.7 listener pose smoothing

- 現状 `gAudiop->setListener()` は毎フレーム瞬時値を渡している
- 急激な camera 動き (zoom / mouse rotate) で binaural の音像が「飛ぶ」可能性
- 1-pole low-pass smoothing (~30ms time constant) を追加検討。r11 で問題が無ければ追加しない

---

## 10. 参考コード位置

| ファイル | 役割 | r11 改修ポイント |
|---|---|---|
| `indra/llaudio/llpositionalstreammulti.cpp` | per-channel speaker bring-up | `makeChannelForBinding()` で `set3DLevel` を `effectiveBinaural()` (= タグ + debug override の合成値) に応じて切替、`addDSP(LiteHrtfDsp)` 挿入 |
| `indra/llaudio/llpositionalstreammulti.h` | 同 | DSP per-frame param push のメソッド追加 |
| `indra/llaudio/lllitehrtfdsp.{h,cpp}` | 新規: lite-HRTF DSP | ITD / ILD / air absorption / 距離減衰の DSP description |
| `indra/llaudio/llvenuereverbdsp.{h,cpp}` | 新規: venue convolution reverb DSP | partitioned FFT convolve + IR ロード経路 |
| `indra/llaudio/llaudioengine_fmodstudio.{h,cpp}` | FMOD engine 本体 | Stream3D ChannelGroup 作成、venue reverb DSP の group 末尾挿入 |
| `indra/newview/llpositionalstreammgr.cpp` | description parser / binding mgr | `venue` / `binaural` / `wetgain` キーの parse 追加、不正値 chat 通知、debug settings 強制 override の合成 |
| `indra/newview/llpositionalstreammgr.h` | 同 | `mVenue` / `mBinaural` / `mWetGain` フィールド追加、root prim 単位で保持。`effectiveVenue()` / `effectiveBinaural()` / `effectiveWetGain()` 等の合成 getter 追加 |
| `indra/newview/llvieweraudio.cpp` | listener pose 設定 | (改修なし、既存の `setListener()` 経路を DSP の per-frame param 取得元として利用) |
| `indra/newview/app_settings/settings.xml` | debug settings | `Stream3DBinauralRender` (int, default -1) / `Stream3DVenueOverride` (string, default "") / `Stream3DVenueWetGain` (F32, default -1.0) 追加。**Preferences UI には出さない** |
| `indra/newview/app_settings/venue_ir/*.wav` | bundled IR | 9 種の venue IR を同梱 (CC-BY 以上ライセンスのみ) |
| `indra/newview/app_settings/venue_ir/CREDITS.md` | IR 出典明記 | OpenAIR / EchoThief / MIT IR Survey 等の attribution |

**r12+ hook ポイント**:

- `LLPositionalStreamMulti::makeChannelForBinding()` の DSP 挿入箇所で `LLLiteHrtfDsp` を `LLSofaHrtfDsp` に差し替える (r12 SOFA 化)。コードコメントに `// r12 hook: SOFA HRTF takeover` を残す
- `LLAudioEngine_FMODSTUDIO::createStream3DGroup()` で Steam Audio plugin を group の DSP として挿入する形にも展開可 (r12+ Steam Audio 化)

---

## 11. 工程 (実装フェーズ)

r11 は viewer-only の改修。配信側パイプラインは r9 / r10 から流用、本リリースで配信側作業はなし。

| ID | 内容 | 主要成果物 | 受入リンク |
|---|---|---|---|
| **P0** | 仕様確定 + roadmap doc 同時更新 | 本書 (`spec_binaural_venue_reverb.md`) を AYA レビュー込みで確定、`docs/ayastorm-stream3d-roadmap.md` §3 r11 entry を「Steam Audio + 任意 SOFA」から「lite-HRTF + venue convolution reverb (配信者主導モデル)」へ書き換え。Steam Audio / 個人 SOFA は r12+ へ降格表記。Preferences UI 改修ゼロ、debug settings 3 件のみの方針も明記 | §1, §9.2 |
| **P1** | Stream3D ChannelGroup 分離 | `LLAudioEngine_FMODSTUDIO` に Stream3D group 追加、`makeChannelForBinding()` で `setChannelGroup` 呼出。**P1 完了直後に master volume / mute / 全体 setVolume の Stream3D group への伝播を確認** (R5 の早期発見) | G7 |
| **P2** | LiteHrtfDsp 骨格 (DSP description, mono in → stereo out, 距離減衰だけ) | `lllitehrtfdsp.{h,cpp}` 新規 | G2, G8 |
| **P3** | LiteHrtfDsp に ITD + ILD shadow + air absorption 実装 | DSP read callback の 3 components 完成 | G1 |
| **P4** | LiteHrtfDsp の per-frame param push 経路 | listener pose / source pos / range の atomic-write 経路 | G1 |
| **P5** | `{binaural}` タグ parser + debug `Stream3DBinauralRender` + hook 反転 | parser に `{binaural}` キー追加、`mBinaural` フィールド + `effectiveBinaural()` 合成 getter、settings.xml に debug 1 件 (int sentinel `-1`)、`makeChannelForBinding()` で gate | G4, G5, G8 |
| **P6** | VenueReverbDsp 骨格 (partitioned FFT convolve, dummy IR で動作確認) | `llvenuereverbdsp.{h,cpp}` 新規 | G3 |
| **P7** | bundled IR 選定 + WAV ロード経路 + CREDITS.md | `app_settings/venue_ir/` に 9 IR 配置、ロード経路実装 | G3, R4 |
| **P8** | `{venue}` タグ parser + debug `Stream3DVenueOverride` | `llpositionalstreammgr.cpp` の `venue` キー処理、`mVenue` フィールド + `effectiveVenue()` 合成 getter、settings.xml に debug 1 件 (string sentinel `""`)、不正値 chat 通知 | G4, G5 |
| **P9** | `{wetgain}` タグ parser + debug `Stream3DVenueWetGain` + venue reverb DSP の group 末尾挿入 | parser に `{wetgain}` キー追加、`mWetGain` + `effectiveWetGain()`、settings.xml に debug 1 件 (F32 sentinel `-1.0`)、reverb DSP gate (= venue=dry または override=dry で完全 bypass) | G4, G5 |
| **P11** | 検証材料生成 + AYA Icecast hosting | `doc/r11/gen_test_material.sh` + 3 素材 | §5 |
| **P12** | §5.3 検証手順 全件実行 | §5.5 受入表全行 PASS。listener override の動作確認は debug settings 経由で打鍵 | §5.5 |
| **P13** | r8/r9/r10 回帰確認 | debug `Stream3DBinauralRender=0` + `Stream3DVenueOverride="dry"` で r10 完全一致、各リリースの §5.3 受入条件再実行 | G8 |
| **P14** | CPU ベンチ + 仕様クローズ | §5.5 CPU 比較 + 仕様書 §13 (実装結果) 追記 | §5.5 / R3 |

(旧 P10 = Preferences UI Sound タブ追加は **削除**。NG8 に従い r11 では一般 UI 改修ゼロ)

### フェーズ依存

```
P0 ─→ P1 ─→ P2 ─→ P3 ─→ P4 ─→ P5 ──┐
            │                         ├─→ P12 ─→ P13 ─→ P14
            └→ P6 ─→ P7 ─→ P9 ────────┤
            └→ P8 (P5 と並行可) ───────┘
                  P11 (P12 開始前までに)
```

P0 (仕様 + roadmap doc 同時更新) は本書策定と同タイミングで roadmap §3 を整合させ、以降 r11 を進める誰もが同じ「lite-HRTF + venue reverb (配信者主導モデル)」前提で動けるようにする (= 旧 Steam Audio 案を root truth として残さない)。P0 完了後に P1 着手。P1 (group 分離) は両 DSP の前提なので最先着、**P1 完了直後に R5 (master volume 伝播) の検証** を行いリスクを早期に潰す。P2-P5 (LiteHrtfDsp 系統) と P6-P9 (VenueReverbDsp 系統) は P1 完了後に独立並行可。Preferences UI フェーズ (旧 P10) は削除。P12 (検証) は実装全完了後、P13 で回帰、P14 でクローズ。

### スコープ縮退オプション (リリース判断時)

- **縮退 A**: lite-HRTF の体感が薄く実機改善できない → タグ `{binaural}` の default を `off` に倒して r10 同等を default にし、binaural は配信者が `{binaural:on}` を明示宣言した時のみ有効化する opt-in 機能として r11 出荷。lite-HRTF の改善は r11.x 以降
- **縮退 B**: venue reverb の CPU が +10pp 超 → IR 上限を 3s → 2s に短縮、`hall_large` / `cathedral` を mono IR に降格、それでも超なら 9 venue → 5 venue (small/medium/large/cathedral/dry/outdoor) に削減
- **縮退 C**: ChannelGroup 分離で master volume / mute の伝播に問題 → group 分離を撤回し、Stream3D channel を master 直結のままにし、reverb DSP を per-channel addDSP で個別配置 (= 6 spk × reverb instances、CPU 6x、IR を short にして補償)
- **縮退 D**: bundled IR のライセンス確認に時間が掛かる → 同梱 IR を 1〜2 種 (medium hall + cathedral) に絞り、その他は r11.x 以降

縮退してもリリース上の体裁は内部リリースとして成立する。**スコープを死守するより足元の動作確認を優先** (r10 と同方針)。

---

## 12. 工数見積

r10 実績 4 日 + roadmap r11 旧見積 19-33 日 (Steam Audio 案) を参照、本案では:

| カテゴリ | 工数 |
|---|---|
| P1 (Stream3D group 分離 + R5 master volume 伝播確認) | 0.5 日 |
| P2-P4 (LiteHrtfDsp 実装 + per-frame param) | 2.5 日 |
| P5 (`{binaural}` タグ + debug binaural 設定 + hook 反転) | 0.5 日 |
| P6-P7 (VenueReverbDsp + bundled IR) | 3 日 |
| P8 (`{venue}` タグ + debug venue override) | 0.5 日 |
| P9 (`{wetgain}` タグ + debug wetgain 設定 + group 挿入) | 0.5 日 |
| P11 (検証材料生成) | 0.5 日 |
| P12-P14 (検証 + 回帰 + クローズ) | 2 日 |
| **合計** | **9.5〜10.5 日** |

(旧 P10 = Preferences UI 0.5 日は削除、合計は 0.5 日減。代わりに P5 / P8 / P9 に「タグ parser + debug settings 1 件」の作業が分散している)

memory `project_release_workload_norms`: r9 / r10 が 3〜4 日、r11 (本案) は DSP 2 個新設 + ChannelGroup 改修で約 2.5 倍見込み。Steam Audio 案 (19-33 日) より大幅に軽い (= 3rd party SDK / 3 platform binary 配布の負債が無いため)。Preferences UI 改修ゼロ化により旧見積より 0.5 日軽量。

リスク発火時の振れ幅:

- 最速 7 日 (lite-HRTF 体感 OK + 縮退発生せず + IR ライセンスがすんなり通る)
- 平均 9.5〜10.5 日 (上記カテゴリ通り)
- 最悪 13.5 日 (ChannelGroup 分離の master volume 問題で縮退 C / IR 選定再やり直し / lite-HRTF re-tuning)

---

## 13. 実装結果 / 受入クローズ

(リリース時記入。r10 spec §13 のフォーマットに合わせて、実装サマリ / §5.4 安定性指標 / 検証中発見の bug / codec 別検証 / 既知の運用上の制約 / r12+ への持ち越し を記録する)

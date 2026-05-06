# AYAstorm 3D Stream 長期ロードマップ (r7 → r11)

**作成日**: 2026-05-03
**対象**: AYAstorm 3D Stream 機能の中長期計画
**位置づけ**: 各リリースの実装工程 / 仕様書とは別軸の、リリース間を貫く全体像と工数感を保持する文書

---

## 1. ねらい

3D Stream 機能を **「分散記述ステレオ」(r8)** から **「lite-HRTF + 会場残響を持つ配信者主導の SL Viewer」(r11)** まで段階的に発展させる。各リリースは独立に完結し、個別にユーザ価値を届けられる構成にする。途中で中断しても価値が積み上がるのが本ロードマップの強み。

---

## 2. アーキテクチャの 2 層モデル

設計の核心は次の **直交した 2 層構造**:

| Layer | 何を解く問題か | 担当リリース |
|---|---|---|
| **Layer 1: 配置** | 音源は空間のどこにあるか? | r8 (stereo × N) → r10 (5.1 venue placement) |
| **Layer 2: レンダリング** | ある方向から音が来ることをどう耳で感じさせるか? / 会場感をどう作るか? | r11 (lite-HRTF + venue convolution reverb、配信者主導モデル) |

両層は **競合せず、掛け算で効果**が出る。実物の 5.1 サラウンドでは Layer 2 (HRTF) は頭・耳介・胴の物理現象として無料で発生するが、SL のヘッドホン視聴ではこれを計算で代替する必要がある。

```
┌──────── Layer 1: WHERE (位置) ────────┐
│ ・mono prim 1 個 → 1 点               │
│ ・stereo prim 2 個 → 2 点              │
│ ・5.1 prim 6 個 → 6 点                 │ ← r8 / r10
└────────────────────────────────────────┘
                  ▼
┌──────── Layer 2: HOW (届き方) ────────────┐
│ ・FMOD 既定: ILD のみ (ITD なし)           │
│ ・lite-HRTF: ITD + ILD shadow + air abs    │ ← r11
│ ・venue convolution reverb (9 種 IR)       │ ← r11
│ ・SOFA per-source HRTF / Steam Audio       │ ← r12+
└────────────────────────────────────────────┘
```

---

## 3. リリース計画

### r7 (完了): decode thread 化

- リリース: `v7.2.4-ayastorm-r7` (2026-05-02)
- 仕様: `doc/spec_stream3d_decode_thread.md`
- 工程: `docs/ayastorm-r7-stream3d-decode-thread.md`
- 効果: ステレオストリームのデコードを main thread から分離、配信受信中の cark を解消

### r8 (完了): 分散記述ステレオ

- リリース: `v7.2.4-ayastorm-r8`
- 仕様: `doc/spec_distributed_stereo.md`
- 工程: `docs/ayastorm-r8-distributed-stereo.md`
- 主要変更: `[3dstream-stereo:{ch:L|R|M}{range:N}{volume:N}]` で N スピーカー (上限 16) 同期再生
- **r10 への布石 (重要)**: F3 で **ring buffer の track 数を実装定数化せず汎用化**したため、r10 で N=6 への切り替えコストを 5-7 日節約できた

### r9 (完了): 5.1ch ソース受入 + 形式判定

- リリース: `v7.2.4-ayastorm-r9`
- 仕様: `doc/spec_5_1ch_source.md`
- 工程: `docs/ayastorm-r9-5_1ch-source.md`
- 6ch Vorbis/FLAC を内部 L/R downmix で受入、未対応形式 (Opus 等) を失敗分類して再接続抑止
- Opus は FMOD 不支持で実 viewer 未対応 (codec plugin 自作で対応案あり、別軸)、配信は ffmpeg primary

### r10 (完了): 5.1 venue placement

- リリース: `v7.2.4-ayastorm-r10` (+ `r10.x`、`r10.x-bugfix-1`)
- 仕様: `doc/spec_5_1ch_placement.md`
- 工程: `docs/ayastorm-r10-5_1ch-placement.md`、`docs/ayastorm-r10.x-routing-diag-chat.md`
- `ch` 値拡張 (`FL` / `FR` / `C` / `LFE` / `SL` / `SR`)
- source ch 数検証、codec 別 channel mapping table
- 配信側の前提: **Icecast 2.4+ + Opus surround** が事実上唯一の現実解 (Shoutcast は Opus 非対応、AAC は Linux ビルド問題、AC-3 はライセンス障壁)
- **「sweet spot 不在」を仕様で明文化**: 自由視点モデルでは cinematic 5.1 体験ではなく **venue placement モード** (= 6 個の固定スピーカーから 5.1 ソースを撒く PA 的発想)
- LFE は通常スピーカー同等扱い (推奨案 (p))
- r10.x で漏れ commit (r9-opus 等) を再リリース、r10.x-bugfix-1 でレガシー SessionSettingsFile 環境の起動クラッシュ修正

### r11: lite-HRTF + venue convolution reverb (配信者主導モデル)

- 仕様: `doc/spec_binaural_venue_reverb.md` (PR #31, #36 マージ済、本リリースで実装着手)
- 主要変更:
  - **lite-HRTF DSP** (`lllitehrtfdsp.{h,cpp}` 新設): ITD + ILD shadow + air absorption を per-channel 適用、FMOD 既定 panner (ILD のみ) を反転して肩代わり
  - **venue convolution reverb DSP** (`llvenuereverbdsp.{h,cpp}` 新設): bundled IR 9 種 (`dry` / `room_small` / `room_medium` / `hall_small` / `hall_medium` / `hall_large` / `club` / `cathedral` / `outdoor`) で会場残響を Stream3D ChannelGroup 末尾に挿入、`{wetgain}` で wet 強度倍率
  - **配信者主導モデル**: 制御は配信者プリム Desc タグ (`{venue:NAME}` / `{binaural:on|off}` / `{wetgain:N}`) で完結、listener 側 Preferences UI 改修ゼロ。実装/検証用に debug settings 4 件のみ追加 (`Stream3DBinauralRender` / `Stream3DVenueOverride` / `Stream3DVenueWetGain` / `Stream3DUrlPreResolve`、いずれも sentinel 値で「タグ通り / 既定有効」が default)
  - **Stream3D ChannelGroup 分離** (P1): `LLAudioEngine_FMODSTUDIO` に Stream3D 専用 group を追加、両 DSP を group 単位で gate。master volume / mute 伝播は P1 完了直後に検証 (R5)
  - **URL pre-resolve (P10、毛色違い同梱)**: HTTPS→HTTP redirect (Cloudflare/CDN 経由 Shoutcast/Icecast) を libcurl HEAD + FOLLOWLOCATION で事前解決、FMOD netstream のクロスプロトコル redirect 非追従問題を回避 (`indra/llaudio/llstream3durlresolve.{h,cpp}` 新設)
- **ヘッドホン視聴前提** — スピーカー視聴時は配信者が `{binaural:off}` でタグ宣言する運用
- **既存配置の自動恩恵**: r8/r10 で過去に置かれた全 prim が r11 投入時点で自動的にリッチ化される (ユーザの再配置不要)
- **r5 NG1 (HRTF/binaural 不要) を反転**: r10 で 5〜30m 会場モデル + 6 spk 同時定位に拡張された結果、ITD なし / air abs なし / 会場残響なしの薄さが体感的に目立つようになったため (詳細は `doc/spec_binaural_venue_reverb.md` §2.3)
- **SOFA per-source HRTF / Steam Audio は r12+ に降格**: 配布負債 (3 OS binary)、CPU 負荷 (16 spk × HRTF × 4 stream)、個人 SOFA 問題 (非個人 SOFA は逆効果になり得る) が大きく、AYAstorm 主用途 (virtual live venue / ヘッドホン視聴) で実際に効く順序を再評価した結果、優先度を「会場感 ≫ ITD/ILD > HRTF」と判断 (詳細は同 §2.2)

---

## 4. 工数見積り

「集中作業日換算」と「個人プロジェクトペース暦週」の二軸。

| リリース | 集中工数 | 個人ペース暦週 | 不確実性 | 実績 |
|---|---|---|---|---|
| r8 | 14-25 日 | 5-7 週 | 中 (F3 ring 設計次第) | 実績 ~2 日 (memory: F3 汎用化が短工数で完了) |
| r9 | 0-3 日 | 0-2 週 (F6 吸収可) | 低 | 完了 (独立リリース化) |
| r10 | 6-11 日 | 2-3 週 | 低-中 (r8 設計依存) | 実績 4 日 |
| r11 | 9.5-10.5 日 | 3-5 週 | 中 (R5 master volume 伝播 / IR ライセンス / lite-HRTF 体感) | 着手中 |
| **合計** | **r8-r10 実績 + r11 計画 ~10 日** | **r11 のみ約 3-5 週** | - | - |

### r8 の内訳
- F1 パーサ拡張: 1-2 日
- F2 linkset 集約 + 子 prim 監視: 3-5 日
- F3 `LLPositionalStreamMulti` 新設: 5-10 日 (★最大)
- F4 throttle 通知: 1-2 日
- F5 設定追加: 0.5 日
- F6 実機ベンチ: 3-5 日

### r10 の内訳
- `ch` enum 拡張: 1 日
- source ch 数検証: 1-2 日
- codec 別 channel mapping table: 1-2 日
- LFE 扱い実装: 0-1 日 (推奨案なら実質ゼロ)
- N-track ring 拡張: 0.5-1 日 (r8 で汎用化済み前提)
- 仕様書整備: 1 日
- 実機ベンチ: 2-3 日

### r11 の内訳

詳細は `doc/spec_binaural_venue_reverb.md` §11 (実装フェーズ) / §12 (工数見積)。

- P0 仕様確定 + roadmap doc 同時更新: 着手中 (本コミット)
- P1 Stream3D ChannelGroup 分離 + R5 master volume 伝播確認: 0.5 日
- P2-P4 LiteHrtfDsp 実装 (骨格 → ITD+ILD shadow+air abs → per-frame param): 2.5 日
- P5 `{binaural}` タグ + debug `Stream3DBinauralRender` + hook 反転: 0.5 日
- P6-P7 VenueReverbDsp + bundled IR (9 種) + CREDITS.md: 3 日
- P8 `{venue}` タグ + debug `Stream3DVenueOverride`: 0.5 日
- P9 `{wetgain}` タグ + debug `Stream3DVenueWetGain` + group 末尾挿入: 0.5 日
- P10 URL pre-resolve (HTTPS→HTTP redirect 解決): 0.5 日 (P0 後に独立並行可)
- P11 検証材料生成 + AYA Icecast hosting: 0.5 日
- P12-P14 検証 + r8/r9/r10 回帰 + CPU ベンチ + 仕様クローズ: 2 日
- 合計 9.5〜10.5 日 (Preferences UI 改修ゼロ、debug settings 4 件のみ)
- リスク発火時の振れ幅: 最速 7 日 / 平均 9.5-10.5 日 / 最悪 13.5 日

---

## 5. リスクと圧縮策

### 主要リスク

| ID | 内容 | 対象 |
|---|---|---|
| RR1 | Steam Audio + FMOD Studio 2.02 の Linux 動作確認が薄い領域。詰まるとリリース全体停止 | r12+ (r11 では Steam Audio 不採用) |
| RR2 | KU100.sofa など個人 SOFA の入手元によっては再配布禁止。viewer 同梱可否を要調査 | r12+ (r11 では SOFA 不採用) |
| RR3 | 16 spk × HRTF 畳み込み × 4 stream 並走で CPU 想定超なら DSP プールや音源数制限が必要 | r12+ |
| RR4 | r8 F3 で N-track ring 汎用化を怠ると r10 で再設計コスト発生 (5-7 日) | r8 → r10 (回避済) |
| RR5 | 配信側コミュニティの Icecast 移行が進まないと r10/r11 の価値が活かしきれない | r10 (運用面) |
| **RR6** | **r11 P1 で Stream3D ChannelGroup 分離後、master volume / mute / 全体 setVolume が Stream3D group に伝播しない** → 縮退 C (group 分離撤回 + per-channel addDSP) | r11 |
| **RR7** | **r11 bundled IR (9 種) のライセンス確認が時間掛かる** (CC0 / 自作録音 / Open Air ライブラリの利用条件) → 縮退 D (1〜2 種に絞って r11 出荷、残りは r11.x) | r11 |
| **RR8** | **r11 lite-HRTF (ITD+ILD shadow) の体感が薄い** → 縮退 A (default off + opt-in 化) | r11 |
| **RR9** | **r11 venue reverb の CPU が +10pp 超** → 縮退 B (IR 上限 3s→2s、`hall_large`/`cathedral` mono 化、9→5 venue) | r11 |

### 工数圧縮の選択肢

1. **r9 を独立リリースしない** (r8 F6 で Opus/FLAC URL を一緒に試す) → 1-2 週節約 [採用せず: 形式判定の独立価値が高く r9 として切り出し]
2. **r8 F3 で N-track ring 汎用化を仕込む** (仕様書 §4.5 の布石を実装で履行) → r10 で 5-7 日節約 [採用、r10 で実証]
3. **r11 で Steam Audio / SOFA を不採用、lite-HRTF + venue reverb で代替** → 配布負債 (3 OS binary) 回避、r11 工数を 19-33 日 → 9.5-10.5 日へ大幅圧縮、Linux 沼回避 [採用、本書策定で確定]
4. **r11 で Preferences UI 改修ゼロ、配信者主導モデルで debug settings 4 件のみ** → 0.5 日節約、一般 listener UI 増殖を回避、配信者主導モデル (タグ root truth) と整合 [採用]
5. **r11 で `{binaural}` default off + opt-in 化** → lite-HRTF 体感が薄い場合の縮退 A (リリース判断時)

---

## 6. 依存関係

```
r7 (done)
  └→ r8 (done)
       ├→ r9 (done — 独立リリース化)
       └→ r10 (done)
            └→ r10.x / r10.x-bugfix-1 (done)
                 └→ r11 (current)
                      └→ r12+ (SOFA per-source HRTF / Steam Audio)
```

- **r8 → r10**: F3 の N-track ring 汎用化に強く依存
- **r10 → r11**: 独立に着手可 (Layer 1 と Layer 2 が直交)。だが順序的に r10 を先にした利点:
  - r10 が安定動作している状態で r11 の実機試聴を行うことで lite-HRTF / venue reverb の効果が分かりやすい
  - r10.x-bugfix-1 まで含めた安定化後に CPU ヘッドルームが把握できているので、r11 の DSP 2 個追加の余地評価が容易

---

## 7. 完成時のユーザ価値

r11 完成時点で AYAstorm が提供する音響体験:

- **水平定位の改善**: lite-HRTF (ITD + ILD shadow) で「左の耳だけ大きい」から「左から音が来る」へ。FMOD 既定 panner (ILD のみ) の薄さを解消
- **距離感の改善**: air absorption で 50m 先のスピーカーが 5m 先と同じ明るさに聴こえる問題を解消
- **会場感の獲得**: venue convolution reverb で dry 素材に「ホール / クラブ / カテドラル / 野外」など 9 種の会場残響を viewer 側で着替え。配信者は dry 素材を流すだけで OK
- **配信者主導の表現**: 配信者がプリム Desc タグに `{venue}` / `{binaural}` / `{wetgain}` を書くだけで、全 listener が同じ意図でレンダリングされた音を聴ける。listener 設定 UI 改修ゼロ
- **ライブ会場 venue placement** (r8 / r10 + r11): 6 個のスピーカー prim から 5.1 ソースを撒き、ChannelGroup 末尾の venue reverb で会場感を載せ、各 channel に lite-HRTF で水平定位を整える → ヘッドホン視聴で **「ホール体験」が成立**
- **既存配置の自動恩恵**: r8 / r10 で過去に置かれた全 prim が r11 投入時点で自動的にリッチ化 (ユーザの再配置不要)
- **HTTPS Shoutcast/Icecast 配信の信頼性向上** (P10 同梱): Cloudflare/CDN 経由 HTTPS フロントの HTTP redirect を viewer 側で事前解決し、FMOD netstream のクロスプロトコル redirect 非追従問題を回避

→ 全体として **「SL のヘッドホン視聴体験を一段階引き上げ」つつ、「配信者が会場の音響を作る」運用モデルを成立させる** 規模の変化になる。

r12 以降での更なる発展余地:

- per-source SOFA HRTF (個人 SOFA で上下/前後の曖昧性解消、KU100 等)
- Steam Audio integration (occlusion / geometry-based reflection、SL 世界 mesh の食わせ込み)
- venue IR ユーザアップロード UI / dynamic venue (位置依存残響)

---

## 8. 検討中で本ロードマップに入っていない事項

- **mono タグ `[3dstream:...]` の N 化**: 同設計を mono にも展開。需要が出たら別リリース
- **per-speaker delay / EQ / pan offset**: ステージスピーカーの時間差再現など
- **リンクセット跨ぎのスピーカー連携**: 複数 root を 1 グループに束ねる大規模配置
- **スピーカー数上限の 32 / 64 への昇格**: 設定既定値変更で済むよう設計時点で配慮済
- **HLS / AAC / AC-3 ソース対応**: ライセンスや実装コストが大きく、現時点では非対象

---

## 9. 更新履歴

- 2026-05-03: 初版作成 (r8 着手時点)。Layer 1/2 直交モデル、r7→r11 計画、工数見積り、リスク策定
- 2026-05-06: r8/r9/r10/r10.x/r10.x-bugfix-1 完了状態を反映。**r11 案を Steam Audio + 任意 SOFA から「lite-HRTF + venue convolution reverb (配信者主導モデル)」に再定義**、SOFA per-source HRTF / Steam Audio は r12+ に降格。工数 19-33 日 → 9.5-10.5 日。Preferences UI 改修ゼロ + debug settings 4 件 + 配信者主導モデル方針を明記。リスク表に R5/IR ライセンス/lite-HRTF 体感/reverb CPU を追加、旧 RR1-3 (Steam Audio/SOFA 系) は r12+ 印つけ。仕様詳細は `doc/spec_binaural_venue_reverb.md` 参照。P0 (本書改訂) として `feature/aya-r11-p0-roadmap-update` ブランチで実施

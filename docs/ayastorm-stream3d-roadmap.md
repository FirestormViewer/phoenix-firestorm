# AYAstorm 3D Stream 長期ロードマップ (r7 → r11)

**作成日**: 2026-05-03
**対象**: AYAstorm 3D Stream 機能の中長期計画
**位置づけ**: 各リリースの実装工程 / 仕様書とは別軸の、リリース間を貫く全体像と工数感を保持する文書

---

## 1. ねらい

3D Stream 機能を **「分散記述ステレオ」(r8)** から **「KU100 ベースのバイノーラル空間音響を持つ SL Viewer」(r11)** まで段階的に発展させる。各リリースは独立に完結し、個別にユーザ価値を届けられる構成にする。途中で中断しても価値が積み上がるのが本ロードマップの強み。

---

## 2. アーキテクチャの 2 層モデル

設計の核心は次の **直交した 2 層構造**:

| Layer | 何を解く問題か | 担当リリース |
|---|---|---|
| **Layer 1: 配置** | 音源は空間のどこにあるか? | r8 (stereo × N) → r10 (5.1 venue placement) |
| **Layer 2: レンダリング** | ある方向から音が来ることをどう耳で感じさせるか? | r11 (HRTF / SOFA / Steam Audio) |

両層は **競合せず、掛け算で効果**が出る。実物の 5.1 サラウンドでは Layer 2 (HRTF) は頭・耳介・胴の物理現象として無料で発生するが、SL のヘッドホン視聴ではこれを計算で代替する必要がある。

```
┌──────── Layer 1: WHERE (位置) ────────┐
│ ・mono prim 1 個 → 1 点               │
│ ・stereo prim 2 個 → 2 点              │
│ ・5.1 prim 6 個 → 6 点                 │ ← r8 / r10
└────────────────────────────────────────┘
                  ▼
┌──────── Layer 2: HOW (届き方) ────────┐
│ ・FMOD 既定: 単純 ITD/ILD              │
│ ・HRTF (KU100.sofa): 畳み込みレンダリング │ ← r11
└────────────────────────────────────────┘
```

---

## 3. リリース計画

### r7 (完了): decode thread 化

- リリース: `v7.2.4-ayastorm-r7` (2026-05-02)
- 仕様: `doc/spec_stream3d_decode_thread.md`
- 工程: `docs/ayastorm-r7-stream3d-decode-thread.md`
- 効果: ステレオストリームのデコードを main thread から分離、配信受信中の cark を解消

### r8 (現行): 分散記述ステレオ

- 仕様: `doc/spec_distributed_stereo.md`
- 工程: `docs/ayastorm-r8-distributed-stereo.md`
- 主要変更: `[3dstream-stereo:{ch:L|R|M}{range:N}{volume:N}]` で N スピーカー (上限 16) 同期再生
- **r10 への布石 (重要)**: F3 で **ring buffer の track 数を実装定数化せず汎用化**しておく — r10 で N=6 への切り替えコストが 5-7 日節約される

### r9 (省略可): Opus / FLAC URL 動作確認

- 詳細: `doc/spec_distributed_stereo.md` §9.1.2
- viewer 改修ほぼゼロ。FMOD 2.02 内蔵デコーダで Opus / FLAC とも完結する想定
- **r8 の F6 ベンチで一緒に試せば独立リリース不要**になる可能性が高い
- 配信側で Icecast 2.4+ を立てて検証

### r10: 5.1 venue placement

- 詳細: `doc/spec_distributed_stereo.md` §9.1
- `ch` 値拡張 (`FL` / `FR` / `C` / `LFE` / `SL` / `SR`)
- source ch 数検証、codec 別 channel mapping table
- 配信側の前提: **Icecast 2.4+ + Opus surround** が事実上唯一の現実解 (Shoutcast は Opus 非対応、AAC は Linux ビルド問題、AC-3 はライセンス障壁)
- **「sweet spot 不在」を仕様で明文化**: 自由視点モデルでは cinematic 5.1 体験ではなく **venue placement モード** (= 6 個の固定スピーカーから 5.1 ソースを撒く PA 的発想)
- LFE は通常スピーカー同等扱い (推奨案 (p))

### r11: Steam Audio + 任意 SOFA (例 KU100.sofa)

- 詳細: `doc/spec_distributed_stereo.md` §9.6
- Steam Audio (Valve, Apache 2.0) を FMOD プラグインとして組み込み
- 任意の SOFA ファイル (KU100 等) を HRTF として読み込める
- 各 3D channel に Spatializer DSP を挿入、`Channel::set3DLevel(0.0f)` で FMOD 既定 panner を無効化 (Layer 1 の panner と Layer 2 の HRTF が二重にかからないように)
- **ヘッドホン視聴前提** — スピーカー視聴では効果が出ない
- **既存配置の自動恩恵**: r8/r10 で過去に置かれた全 prim が r11 投入時点で自動的にリッチ化される (ユーザの再配置不要)

---

## 4. 工数見積り

「集中作業日換算」と「個人プロジェクトペース暦週」の二軸。

| リリース | 集中工数 | 個人ペース暦週 | 不確実性 |
|---|---|---|---|
| r8 | 14-25 日 | 5-7 週 | 中 (F3 ring 設計次第) |
| r9 | 0-3 日 | 0-2 週 (F6 吸収可) | 低 |
| r10 | 6-11 日 | 2-3 週 | 低-中 (r8 設計依存) |
| r11 | 19-33 日 | 8-12 週 | **高** (Linux + ライセンス + CPU) |
| **合計** | **39-72 日** | **15-24 週 (約 4-6 ヶ月)** | - |

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
- R11-1 Steam Audio バイナリ build (Win/Mac/Linux): 3-7 日 ← **Linux が鬼門**
- R11-2 init code: 2-3 日
- R11-3 SOFA ロード経路: 1-2 日
- R11-4 Spatializer DSP 挿入 + FMOD panner 無効化: 3-5 日
- R11-5 source/listener pos 反映: 2-3 日
- R11-6 設定 UI / SOFA ファイル管理: 2-3 日
- R11-7 CPU ベンチ: 3-5 日
- R11-8 実機試聴 + 仕様クロージング: 3-5 日

---

## 5. リスクと圧縮策

### 主要リスク

| ID | 内容 | 対象 |
|---|---|---|
| RR1 | Steam Audio + FMOD Studio 2.02 の Linux 動作確認が薄い領域。詰まると r11 全体停止 | r11 |
| RR2 | KU100.sofa の入手元によっては再配布禁止。viewer 同梱可否を要調査 | r11 |
| RR3 | 16 spk × HRTF 畳み込み × 4 stream 並走で CPU 想定超なら DSP プールや音源数制限が必要 | r11 |
| RR4 | r8 F3 で N-track ring 汎用化を怠ると r10 で再設計コスト発生 (5-7 日) | r8 → r10 |
| RR5 | 配信側コミュニティの Icecast 移行が進まないと r10/r11 の価値が活かしきれない | r10 (運用面) |

### 工数圧縮の選択肢

1. **r9 を独立リリースしない** (r8 F6 で Opus/FLAC URL を一緒に試す) → 1-2 週節約
2. **r8 F3 で N-track ring 汎用化を仕込む** (仕様書 §4.5 の布石を実装で履行) → r10 で 5-7 日節約
3. **r11 を Win/Mac 先行リリース、Linux は r11.1** → Linux build 沼で全体が止まることを回避
4. **r11-6 の SOFA UI を最低限化** (固定パス + debug settings のみ) → 2-3 日節約、本格 UI は r12 以降

---

## 6. 依存関係

```
r7 (done)
  └→ r8 (current)
       ├→ r9 (optional — r8 F6 に吸収可)
       └→ r10
            └→ r11
```

- **r8 → r10**: F3 の N-track ring 汎用化に強く依存
- **r10 → r11**: 独立に着手可 (Layer 1 と Layer 2 が直交)。だが順序的に r10 を先にする利点:
  - r10 が安定動作している方が r11 の実機試聴で効果が分かりやすい
  - r11 着手前に r10 のベンチで CPU ヘッドルームを把握しておきたい

---

## 7. 完成時のユーザ価値

r11 完成時点で AYAstorm が提供する音響体験:

- **アバターの足音・ジェスチャー音**: KU100 HRTF で「耳の周囲で鳴る」感じ
- **耳元アクセサリ** (近接 mono prim): 真にバイノーラル収録に近い臨場感
- **ライブ会場 venue placement** (r8 / r10 + r11): 6 個のスピーカー prim から 5.1 ソースを KU100 経由で空間レンダリング → ヘッドホン視聴で **「ホール体験」が成立**
- **既存配置の自動恩恵**: r8 / r10 で過去に置かれた全 prim が r11 投入時点で自動的にリッチ化

→ 全体として **「SL のヘッドホン視聴体験を一段階引き上げる」規模の変化** になる。

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

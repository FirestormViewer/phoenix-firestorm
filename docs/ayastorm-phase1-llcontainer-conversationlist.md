# Phase 1: LLFloaterIMContainer 会話リスト表示 仕様書

**作成日**: 2026-04-28
**対象**: AYAstorm `feature/llfloaterimcontainer-coexist` ブランチ
**前提**: Phase 0 完了 (`LLFloaterIMContainerLL` スタブがメニューから開ける状態)

---

## 0. このドキュメントの読み方 (Claude Code 向け)

1. **Phase 0 仕様書 (`ayastorm-phase0-llcontainer-coexist.md`) を先に確認** (役割分担・運用ルール・ビルド手順は共通)
2. **各ステップで AYA に diff を提示して承認を得る**
3. **AYA の許可なく commit / push / 大規模変更しない**

---

## 1. ゴール

左ペインに Conversation List を表示する。

```
┌─ CONVERSATIONS (LL) ─────────────────────────────┐
│ ┌─ Conversation List ─┐ ┌─ (右ペインは Phase 2) ─┐ │
│ │ ▼ Nearby Chat       │ │                        │ │
│ │ ▼ IM session 1      │ │                        │ │
│ └─────────────────────┘ └────────────────────────┘ │
└──────────────────────────────────────────────────┘
```

**Phase 1 成功条件**:
- ビルドが通る
- `Conversations (LL Style)` を開くと左ペインにリストが表示される
- FS 版の挙動が壊れていない

右ペインの会話内容表示は Phase 2 以降。

---

## 2. 方針

- **`#if 0` / `#endif` を外すだけ** (新規コードは書かない)
- 依存関係の底から順に 1 レイヤーずつ再有効化し、毎回ビルド確認
- Phase 0 のスタブ (`llfloaterimcontainer_ll.cpp/.h`) は最終ステップで本家実装に置き換えて削除
- FS 版 (`FSFloaterIMContainer`、`FSFloaterIM` 等) は触らない

### なぜ `#if 0` を外すだけで済むか

- merge-base 調査により、AYAstorm の `#if 0` 内のコードは SL Release/26.1.1 (2026-04-07) と同一
- `sl-upstream/main` との差分もゼロ (conversation 系ファイル全て)
- `<FS:Ansariel>` マーカーはほぼゼロ (llconversationmodel.h に 1 件のみ)
- よって SL 公式から改めて取り込む必要はない

---

## 3. 調査結果サマリー

| ファイル | `<FS:Ansariel>` | SL との差分 | 役割 |
|---|---|---|---|
| `llconversationmodel.h/.cpp` | 1件 (Zi検索) | +5〜8行 | 会話リストのデータモデル |
| `llparticipantlist.h/.cpp` | 0件 | +5〜12行 | 参加者リスト |
| `llconversationview.h/.cpp` | 0件 | +4〜7行 | 会話リストのウィジェット |
| `llfloaterimsessiontab.h/.cpp` | 0件 | +5行 | IM セッションタブの基底クラス |
| `llfloaterimsession.h/.cpp` | 0件 | +6〜62行 | LL 版 IM セッション個別ウィンドウ |
| `llfloaterimnearbychat.h/.cpp` | 0件 | +5〜68行 | LL 版 Nearby Chat |

差分の大半は `#if 0`/`#endif` ラッパーと RLVa 追加分。

---

## 4. ステップ構成

### Step 1: データモデル層の再有効化

対象ファイルの `#if 0` (先頭) と `#endif` (末尾) を削除:

- `indra/newview/llconversationmodel.h`
- `indra/newview/llconversationmodel.cpp`
- `indra/newview/llparticipantlist.h`
- `indra/newview/llparticipantlist.cpp`

ビルド確認 → AYA 承認 → commit。

### Step 2: ビュー層の再有効化

- `indra/newview/llconversationview.h`
- `indra/newview/llconversationview.cpp`
- `indra/newview/llfloaterimsessiontab.h`
- `indra/newview/llfloaterimsessiontab.cpp`

ビルド確認 → AYA 承認 → commit。

### Step 3: IM・Nearby Chat ヘッダ層の再有効化

`llfloaterimcontainer.cpp` が依存しているため Step 4 の前に必要。
右ペインの完全動作は Phase 2 だが、ヘッダ定義は Phase 1 で必要。

- `indra/newview/llfloaterimsession.h`
- `indra/newview/llfloaterimsession.cpp`
- `indra/newview/llfloaterimnearbychat.h`
- `indra/newview/llfloaterimnearbychat.cpp`

ビルド確認 → AYA 承認 → commit。

### Step 4: メインコンテナの昇格

- `indra/newview/llfloaterimcontainer.h`
- `indra/newview/llfloaterimcontainer.cpp`

の `#if 0` を除去。`LLFloaterIMContainer` クラスが復活する。

ビルド確認 → AYA 承認 → commit。

### Step 5: Phase 0 スタブの置き換えと仕上げ

#### 5.1 registry 更新 (`llviewerfloaterreg.cpp`)

```cpp
// Before (Phase 0 スタブ):
LLFloaterReg::add("ll_im_container", "floater_im_container_ll.xml",
    (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterIMContainerLL>);

// After (Phase 1 本家):
LLFloaterReg::add("ll_im_container", "floater_im_container.xml",
    (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterIMContainer>);
```

include も変更:
```cpp
// 削除: #include "llfloaterimcontainer_ll.h"
// (llfloaterimcontainer.h は既に line 165 で include 済み)
```

#### 5.2 Phase 0 スタブファイルの削除

- `indra/newview/llfloaterimcontainer_ll.h`
- `indra/newview/llfloaterimcontainer_ll.cpp`
- `indra/newview/skins/default/xui/en/floater_im_container_ll.xml`
- `CMakeLists.txt` から `_ll` エントリを削除

#### 5.3 XUI/C++ 整合性検証

`llfloaterimcontainer.cpp` が参照する XUI ノード名と `floater_im_container.xml` のノード名を突合せ確認。

既知の参照ノード:
- `conversations_stack`、`conversations_layout_panel`、`conversations_list_panel`
- `conversations_pane_buttons_stack`、`conversations_pane_buttons_expanded`
- `messages_layout_panel`、`im_box_tab_container`
- `expand_collapse_btn`、`stub_panel`、`stub_textbox`、`stub_collapse_btn`

ビルド確認 → XUI/C++ 整合確認 → AYA 承認 → commit。

---

## 5. ビルド手順

Phase 0 仕様書 `ayastorm-phase0-llcontainer-coexist.md` の **1.4 節** を参照。

新規ファイル変更なし (既存ファイルの `#if 0` 除去のみ) なので、
**インクリメンタルビルド** (`--no-configure`) で可。

ただし Step 5 でスタブ削除 (CMakeLists.txt 変更) が入るため、その時だけフルビルド (configure 込み) が必要。

---

## 6. スコープ外 (Phase 2 以降)

- 右ペインに会話内容を表示
- LL 版から IM を新規開始
- Nearby Chat の入力欄を LL 版で動作させる
- FS 版との連携ブリッジ
- ショートカットキー割り当て

---

## 7. Phase 1 完了記録

- 完了日時: 2026-04-28
- 各 Step の commit hash:
  - Step 1: `8af0ca7dab` Phase 1 Step1: Re-enable llconversationmodel and llparticipantlist
  - Step 2-5: `88778f201d` Phase 1 Steps 2-5: Re-enable LL Chat UI files and wire up LLFloaterIMContainer
  - Step 5 fixes: `a17ce33b79`, `0e821250d0`
- 動作確認結果: LL 窓を開くと左ペインに会話リストが表示される。FS 版の挙動に影響なし。
- 気づき・課題: Phase 0 スタブ削除・registry 置き換えで若干のクラッシュがあり、
  `impanel` の LLFloaterReg 登録追加で解決した。

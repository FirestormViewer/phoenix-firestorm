# Phase 3: LL Chat Window スタイル切替と参加者リスト range filter 実装記録

**作成日**: 2026-04-28
**対象**: AYAstorm `feature/llfloaterimcontainer-coexist` ブランチ
**前提**: Phase 2 完了 (LL 窓でチャット送受信・遷移・フラッシュ・アイコン表示が動作する状態)

---

## 1. ゴール

LL Chat Window と FS Chat Window を**同時起動の常時並行**ではなく、**ユーザー設定でどちらかを選んで使う**方式に切り替える。

- 設定 `AYAChatWindowStyle` (S32: 0=FS V1 / 1=FS V7 / 2=LL Style) を導入
- メニュー Ctrl+T、ツールバーのチャットボタン、IM トースト、IM 開始など**すべての導線**が現在の選択に従う
- LL 窓の「参加者リスト」がチャットレンジ (`sayRange`, デフォルト 20m) 内のユーザーだけを表示する
- LL 窓のコンパクト/拡張表示は FS 側の `PlainTextChatHistory` から独立させる

---

## 2. 実装内容

### 2.1 設定とヘルパー導入

**ファイル**: `app_settings/settings_firestorm.xml`, `llviewercontrol.{cpp,h}`, `panel_preferences_chat.xml`
**commit**: `1d5ba9959f`

- `AYAChatWindowStyle` (S32, 既定 1=FS V7) を追加
- `AYALLChatCompactView` (Boolean, 既定 true) を追加し、LL 窓のコンパクト/拡張を独立管理
- 環境設定の「Use V1 style chat headers」チェックボックスを 3 択ラジオに置換
- 自由関数を追加:

```cpp
bool ayastorm_is_ll_style();          // AYAChatWindowStyle == 2
std::string ayastorm_im_container_name(); // "ll_im_container" or "fs_im_container"
```

設定変更時のリスナーで以下を実行:

- `AYAChatWindowStyle` 変化 → FS IM/Nearby のチャット履歴スタイル更新
- `AYALLChatCompactView` 変化 → `LLFloaterIMSessionTab::processChatHistoryStyleUpdate()`
- LL 選択時に `ll_im_container` を即時 instantiate (auto-open)

### 2.2 IM/近隣チャットのルーティング

**ファイル**: `llavataractions.cpp`, `llgroupactions.cpp`, `llchiclet.cpp`, `llimview.cpp`, `llfloaterimnearbychathandler.cpp`
**commit**: `93d6dd3e05`

Phase 2 で「常に両方」だった分岐を `ayastorm_is_ll_style()` で切替に変更:

- `LLAvatarActions::startIM()` / `LLGroupActions::startIM()` → LL or FS の片方だけに `showConversation()`
- `LLIMChiclet::onMouseDown()` / `onCurrentVoiceChannelChanged()` → 同上
- `LLIMMgr::addMessage()` のオフライン IM 通知 → `ayastorm_im_container_name()` で `showInstance()`
- IM トースト click → LL 選択時のみ `ayastorm_show_ll_im_conversation()` (新規 free 関数) を呼ぶ
- `LLFloaterIMNearbyChatHandler::processChat()` → LL 選択時のみ `nearby_chat` floater にフォワード

`ayastorm_show_ll_im_conversation()` は循環インクルード回避のため `llfloaterimcontainer.cpp` 側に定義。

### 2.3 メニュー/ツールバーの動的ルーティング

**ファイル**: `llviewerwindow.cpp`, `llviewermenu.cpp`, `menu_viewer.xml`, `commands.xml`, `panel_toolbar_view.xml`
**commit**: `df2760d422`

XUI の固定パラメータ (`fs_im_container` / `fs_nearby_chat` 等) を、設定を見て切り替える callback に置換:

- `AYA.Conversations.Toggle` / `AYA.Conversations.Check` / `AYA.Conversations.IsOpen`
  - メニュー「Conversations」(Ctrl+T) と toolbar `chat` コマンドから利用
  - `ayastorm_im_container_name()` を経由して toggle / 状態取得
- `AYA.NearbyChat.Toggle`
  - `panel_toolbar_view.xml` のローカルチャットボタンから利用
  - LL 選択時 `ll_im_container` を、FS 選択時 `fs_nearby_chat` を toggle

`menu_viewer.xml` から「Conversations (LL Style)」の重複エントリを削除し、Ctrl+T 一本に統一。

callback は `panel_toolbar_view.xml` の load 前に解決される必要があるため、`LLViewerWindow::initBase()` の toolbar view 構築直前で `CommitCallbackRegistry` / `EnableCallbackRegistry` に登録する (`llviewermenu.cpp` の `initialize_menus()` だとタイミングが遅すぎる)。

### 2.4 チャット履歴スタイルの適用

**ファイル**: `llfloaterimsessiontab.cpp`, `fsfloaterim.cpp`, `fsfloaternearbychat.cpp`, `fschatoptionsmenu.cpp`, `llfloaterconversationpreview.cpp`, `llfloaterpreference.cpp`
**commit**: `09f897296b`

これまで `PlainTextChatHistory` (Boolean) を直接読んでいた箇所を整理:

- FS 窓 (`FSFloaterIM`, `FSFloaterNearbyChat`, `LLFloaterConversationPreview`)
  - `use_plain_text_chat_history = (AYAChatWindowStyle == 0)` // V1 のみ plain
- `FSChatOptionsMenu` の「show_mini_icons」可視判定も同じ規則に
- LL 窓 (`LLFloaterIMSessionTab::appendMessage()`)
  - LL 選択時は `AYALLChatCompactView`、それ以外は `AYAChatWindowStyle == 0`
- LL 窓のコンパクト/拡張メニュー (`onIMSessionMenuItemClicked` / `onIMCompactExpandedMenuItemCheck` / `onIMShowModesMenuItemEnable`) を `ayastorm_is_ll_style()` で `AYALLChatCompactView` に振り分け
- `LLFloaterPreference::onOpen()` で旧 `plain_text_chat_history` チェックボックスへの set 処理をコメントアウト (XUI から削除済みのため)

### 2.5 LL 窓 参加者リストの range filter

**ファイル**: `llfloaterimcontainer.cpp`, `llfloaterimsessiontab.cpp`
**commit**: `09f897296b`

**問題**: LL 窓の参加者リストはボイス参加者を `STATUS_VOICE_ACTIVE` で取り込むため、`LLLocalSpeakerMgr::updateSpeakerList()` の `sayRange` チェックが効かず、20m 外のユーザーが残り続けていた。

**設計判断**: `mSpeakers` を直接いじる方法 (`removeSpeaker()` / 検証コールバック) はすべて副作用で挫折:

- `setVisible(false)` は `LLFolderViewFolder::arrange()` が毎レイアウトで `setVisible(isPotentiallyVisible())` で上書きするため恒久化できない
- `removeSpeaker()` は protected。public ラッパーを生やしても、ボイス側が直後に `setSpeaker()` で再追加してしまい、しかも mSpeakers にいるため "add" イベントが二度と飛ばず、再表示が完全に詰む

**最終形**: `mSpeakers` は触らず、**参加者モデルとの差分を 1 秒ごとにリコンサイル**する方式を `LLFloaterIMContainer::idleUpdate()` の `mParticipantRefreshTimer` 内に実装:

```cpp
if (ayastorm_is_ll_style())
{
    LLParticipantList* nearby_list = dynamic_cast<LLParticipantList*>(getSessionModel(LLUUID::null));
    if (nearby_list)
    {
        F32 r = (F32)LFSimFeatureHandler::getInstance()->sayRange();
        F32 rsq = r * r;

        // Step 1: モデルから sayRange 外の参加者を削除
        std::vector<LLUUID> to_remove;
        for (auto pit = nearby_list->getChildrenBegin(); pit != nearby_list->getChildrenEnd(); ++pit)
        {
            LLConversationItemParticipant* p = dynamic_cast<LLConversationItemParticipant*>((*pit).get());
            if (!p || p->getUUID() == gAgentID) continue;
            LLVOAvatar* av = (LLVOAvatar*)gObjectList.findObject(p->getUUID());
            if (av && dist_vec_squared(av->getPositionAgent(), gAgent.getPositionAgent()) > rsq)
                to_remove.push_back(p->getUUID());
        }
        for (const LLUUID& id : to_remove)
            nearby_list->removeParticipant(id);

        // Step 2: sayRange 内に戻った speaker でモデルにいないものを再追加
        LLSpeakerMgr::speaker_list_t speaker_list;
        LLLocalSpeakerMgr::getInstance()->getSpeakerList(&speaker_list, true);
        for (const LLPointer<LLSpeaker>& sp : speaker_list)
        {
            if (sp->mID == gAgentID) continue;
            LLVOAvatar* av = (LLVOAvatar*)gObjectList.findObject(sp->mID);
            if (av && dist_vec_squared(av->getPositionAgent(), gAgent.getPositionAgent()) <= rsq
                && !nearby_list->findParticipant(sp->mID))
            {
                nearby_list->addAvatarIDExceptAgent(sp->mID);
            }
        }
    }
}
```

ポイント:

- `mSpeakers` の責務はボイスサブシステム側に残し、モデル側だけを真実に追従させる
- `gObjectList.findObject()` が null の場合 (アバターオブジェクト未ロード) は判定スキップ → 誤削除しない
- `LFSimFeatureHandler::sayRange()` を都度参照するので、SIM extras の `say-range` が反映される

補助として `LLFloaterIMSessionTab::refreshConversation()` でも nearby chat 参加者の `STATUS_NOT_IN_CHANNEL` を `setVisible(false)` 化しておく (FS 窓側の挙動カバー、LL 窓ではリコンサイラ側が事実上の真)。

---

## 3. 技術的な詰まりポイントと解決策

### Toolbar callback 解決タイミング

`AYA.NearbyChat.Toggle` などを `initialize_menus()` で登録すると、`panel_toolbar_view.xml` の parse はそれより前に走るため `Function ... not found` 警告でボタンが無反応になる。
→ `LLViewerWindow::initBase()` の toolbar view 構築直前で `CommitCallbackRegistry::defaultRegistrar().add(...)` する形に修正。

### `setVisible(false)` が永続しない

`LLFolderViewFolder::arrange()` が毎レイアウトで `setVisible(isPotentiallyVisible())` を呼び、`isPotentiallyVisible()` は `passedFilter()` (会話では常時 true) と OR を取るため、外から false を入れても次の arrange で true に戻される。
→ 表示制御では解決不可と判断し、モデルから本当に消す方針に転換。

### `removeSpeaker()` 後に再追加が来ない

`LLSpeakerMgr::removeSpeaker()` は protected。public wrapper を作って削除しても、voice 側 (`LLSpeakerMgr::setSpeaker()`) が直後に同じ ID で `mSpeakers` に再登録する。`setSpeaker()` 内部では「既に存在する」ため "add" イベントが飛ばず、`LLParticipantList` が永遠に拾い直してくれなくなる。
→ `mSpeakers` を一切触らず、`LLParticipantList::addAvatarIDExceptAgent()` / `removeParticipant()` で**モデルだけを** sync するリコンサイラに切替。

### Validate callback で再追加が止まる

最初に `LLLocalSpeakerMgr` に validate コールバックを差して `sayRange` 判定したところ、一度 false を返すとその ID が "burned" 状態になり、`mSpeakers` から消えても (上記の理由で) 二度と再追加されなくなった。
→ 同様にリコンサイラ方式に置き換えて回避。

---

## 4. commit 一覧

| commit | 内容 |
|---|---|
| `1d5ba9959f` | Phase 3: Introduce AYAChatWindowStyle setting and routing helpers |
| `93d6dd3e05` | Phase 3: Route IM and nearby chat actions on AYAChatWindowStyle |
| `df2760d422` | Phase 3: Dynamic Conversations menu and toolbar routing |
| `09f897296b` | Phase 3: Apply chat style and range-filter LL nearby participants |

---

## 5. 動作確認済み項目

- 環境設定で V1 / V7 / LL を切り替えると、即座に対応する Chat Window だけが開く / 表示スタイルが切り替わる
- Ctrl+T、toolbar の Chat / Local Chat ボタン、メニュー、IM 開始、IM トースト、オフライン IM 通知、近隣チャット転送、すべて選択中のスタイル側にだけ届く
- LL 窓のコンパクト/拡張トグルが FS 側の V1/V7 設定に影響しない (逆も同じ)
- LL 窓「ローカルチャット」の参加者リストが SIM の `sayRange` (例: 20m) 内のユーザーだけを表示し、レンジ外に出たら 1 秒以内に消え、レンジ内に戻ったら再追加される
- ボイス参加者も範囲判定の対象になっている

---

## 6. 残課題・スコープ外

- `AYAChatWindowStyle` 切替時、開いていた旧スタイルの floater は閉じない (ユーザーが明示的に閉じる必要あり)
- `ayastorm_show_ll_im_conversation()` は循環 include 回避のため `llfloaterimcontainer.cpp` に置いた free 関数。同種ヘルパが増えたら `ayastorm_routing.{h,cpp}` 等に分離を検討
- リコンサイラは 1Hz 駆動。ユーザーが高速移動する場合の体感ラグが気になれば `mParticipantRefreshTimer` の周期を別途調整可
- ペーパー上 LL 窓は依然として `LLLocalSpeakerMgr` の voice 参加者追加ロジックに乗っているため、SIM 仕様変更で `sayRange` 概念が変わると再検討が必要

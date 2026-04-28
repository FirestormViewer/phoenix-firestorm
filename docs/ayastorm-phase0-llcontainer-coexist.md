# Phase 0: LLFloaterIMContainer 共存実装 試金石仕様書 (v2)

**作成日**: 2026-04-28 (v2 改訂版)
**対象**: 新セッションの Claude Code (`--dangerously-skip-permissions` ではなく、AYA との対話モード)
**作業時間想定**: 数日〜1 週間
**前提**: AYAstorm の `ayastorm-release` ブランチが正常にビルド・起動できる状態

## 改訂履歴

- **v2 (2026-04-28)**: レビュー指摘を反映
  - Step 3 に依存解析駆動の事前マトリクス作成を追加 (反応的→事前的)
  - Step 3.0 として「置き換え/merge/別名取り込み」の判断ステップを追加
  - Step 4 に registry 名衝突確認を追加
  - Step 6 に XUI/C++ 整合性検証コマンドを具体化
  - ショートカットキー衝突を考慮、Phase 0 ではメニューからのみ開く方針に統一
- **v1 (2026-04-28)**: 初版

---

## 0. このドキュメントの読み方 (Claude Code 向け)

このドキュメントは Claude Code (Anthropic の Claude Sonnet 系を想定) と AYA の協同作業用です。Claude Code として作業する場合:

1. **章 1〜3 を最初に熟読** (目的、役割分担、設計方針)
2. **章 4 の進め方に沿って 1 ステップずつ進める**
3. **各ステップで AYA に diff を見せて承認を得る**
4. **AYA の許可なく commit / push / 大規模変更しない**
5. **章 7 の運用ルールを厳守**
6. **章 8 のトラブル対処を活用**

---

## 1. プロジェクト概要

### 1.1 AYAstorm

AYAstorm は Phoenix Firestorm (Second Life の高機能 viewer) を AYA がフォークしたカスタム版です。

- リポジトリ: `~/work_firestorm/phoenix-firestorm`
- ベースブランチ: `ayastorm-release`
- 作業ブランチ: `feature/llfloaterimcontainer-coexist` (Phase 0 で新規作成)

リモート:
- `origin`: AYA の個人 fork (push OK)
- `upstream`: Phoenix Firestorm 本家 (push 禁止)
- `sl-upstream`: SL 公式 viewer (push 禁止、参照のみ)

### 1.2 Phase 0 の目的

**LL 公式版の `LLFloaterIMContainer` (Folder View 形式の Chat Window) を AYAstorm に追加**して、メニューから開けるようにする。

#### なぜこれが必要か

AYA は SL を 3D 世界の没入感を大事にしている人で、「画面占有率に対する情報密度が高い」LL 版 Chat Window の UX (左ペインに会話リスト + 右ペインに会話内容) を AYAstorm で使いたい。

現状の Firestorm は Tab UI 形式 (`FSFloaterIM` + `FSFloaterIMContainer`) で、近隣アバター操作や複数 IM の管理に Chat Window 以外のウィンドウが必要になり、3D 描画の視界が狭くなる。

#### 実現したい UI イメージ

```
┌─ Chat Window (LLFloaterIMContainer) ─────────────┐
│ ┌─ Conversation List ─┐ ┌─ Conversation View ─┐ │
│ │ ▼ Nearby Chat       │ │ <選択された会話の中身>  │ │
│ │   - alice (距離付)  │ │                     │ │
│ │   - bob             │ │                     │ │
│ │ ▼ IM session 1      │ │                     │ │
│ │ ▼ IM session 2      │ │                     │ │
│ └─────────────────────┘ └─────────────────────┘ │
└──────────────────────────────────────────────────┘
```

### 1.3 Phase 0 のゴール

**最小限の動く状態**:
- LL 版 `LLFloaterIMContainer` が Communicate メニューから開ける
- ウィンドウが表示される (中身が空でも可)
- ビルドが通る
- FS 版の挙動 (`FSChatWindow=1`) は壊れない

→ Chat Window UI が表示されたら Phase 0 成功。中身の機能 (Nearby Chat 連携、IM 起動連携) は Phase 1 以降で段階的に追加。

**v2 補足**: ショートカットキー (Ctrl+T 等) は Phase 0 では割り当てない。Firestorm が既に使用している可能性が高いため、衝突回避を優先。Phase 1 以降で空きキーを調査して割り当てる。

---

## 2. 役割分担

この作業は AYA と Claude Code の協同で進めます。役割分担を明確化します。

### 2.1 AYA の役割

- **判断と承認**:
  - 各 commit の diff レビュー (FS 拡張機能を壊していないか)
  - 取り込むファイルの選定で迷ったら判断 (Claude Code が提案、AYA が承認)
  - commit メッセージの最終確認
  - **取り込み方式の判断 (置き換え / merge / 別名取り込み)** ← v2 で重要に
- **動作確認**:
  - ローカルでビルド実行 (`autobuild build`)
  - AYAstorm 起動 (`./firestorm`)
  - メニューから Chat Window を開く操作
  - **見た目** での動作確認 (ウィンドウが表示されるか、UI が壊れていないか)
- **状況共有**:
  - ビルドエラーが出たら Claude Code にコピペ
  - 起動時のクラッシュ/エラーログを共有

### 2.2 Claude Code の役割

- **実装**:
  - SL 公式版から必要なファイルを取り込む (`git show sl-upstream/main:...`)
  - 取り込み後の調整 (Firestorm 環境への適合、include 修正等)
  - registry 登録、メニュー追加
  - ビルドエラーの解析と修正提案
- **コードレベル検証** (AYA が見ても分からない部分):
  - **XUI と C++ の紐付けが正しいか確認** ← v2 で具体的検証コマンドを章 4 Step 6 に明記
  - **C++ の関数シグネチャが LL 版と AYAstorm で整合しているか**
  - **include 依存関係が解決しているか**
  - **AYAstorm 既存のクラスと競合していないか**
  - **registry key が衝突していないか** ← v2 で章 4 Step 4 に明記
- **調査と提案**:
  - 各ステップで何を取り込むべきか提案
  - **依存解析を事前実行して取り込み順序を決定** ← v2 でリアクティブ→プロアクティブに変更
  - 進捗状況のまとめ

### 2.3 役割分担で重要な点

> **AYA は「見た目」だけでは XUI と C++ の連携の正しさは判断できない**

例えば、Chat Window が表示されても、内部で `getChild<LLPanel>("conversations_panel")` が `nullptr` を返している可能性がある (XUI に `name="conversations_panel"` がない、等)。これは **見た目は正常だが、実際は機能が動かない** 状態を生む。

→ **Claude Code が XUI/C++ の整合性を検証する責務を負う**。Step 6 で具体的な検証コマンドを実行し、AYA に「整合確認済み」を明示的に報告する。

---

## 3. 設計方針

### 3.1 共存方式 (Coexistence)

- LL 版 `LLFloaterIMContainer` を **新規追加**
- FS 版 (`FSFloaterIMContainer` の Tab UI) は **そのまま保持**
- 両方が registry に登録される
- ユーザーはどちらでも開ける
  - メニュー > Communicate > Conversations (LL 版、新規追加)
  - 既存のショートカット等で FS 版 (現状通り)

### 3.2 通信レイヤーは触らない

過去の引き算アプローチで失敗した最大の理由は **通信レイヤー (LSL message 受信、IM 配信、通知処理等) に手を入れた** こと。テスト範囲が爆発した。

Phase 0 では:
- `llimview.cpp` (IM 通信中核) は **触らない**
- `llnotificationhandlerutil.cpp` (通知配信) は **触らない**
- `llstartup.cpp` (起動シーケンス) は **触らない**
- 既存の `<FS:Ansariel> [FS communication UI]` マーカーは **触らない**
- → IM の送受信や通知の挙動は ayastorm-release と完全に同じ

### 3.3 LL 版 Chat Window は表示専用 (まずは)

Phase 0 の段階では:
- LL 版 `LLFloaterIMContainer` は表示できればいい
- 中身の Conversation List は空または最小限でいい
- IM 起動などの「内部の機能」は後回し

→ Chat Window UI が表示できたら Phase 0 成功。Phase 1 以降で段階的に機能を増やす。

### 3.4 「壊れたら戻せる」を最優先

- 各 commit は小さく (新規ファイル追加 1 つ、または既存ファイル修正数行)
- 各 commit でビルド確認
- ビルド失敗したら commit を revert
- ayastorm-release ブランチに戻れば常に動く環境がある

### 3.5 取り込み方針: 「別名取り込み」を主軸に検討する (v2 で追加)

過去の Firestorm は LL チャット系コードに `<FS:Ansariel>` マーカーで改修を入れている可能性が高い。SL 公式版で単純置換すると、**長年蓄積された FS 改修が消える危険** がある。

そこで Phase 0 では以下の優先順位で取り込みを判断する:

1. **AYAstorm に対象ファイルが存在しない場合**: 単純取り込み
2. **AYAstorm に存在し、`<FS:Ansariel>` マーカーがほぼゼロの場合**: 単純置換 (リスク低)
3. **AYAstorm に存在し、`<FS:Ansariel>` マーカーがそこそこある場合**: **別名取り込み** (例: `llfloaterimcontainer_ll.cpp`) を **第一候補**
4. **AYAstorm に存在し、複雑に改修されている場合**: 別名取り込み + クラス名も別名 (例: `LLFloaterIMContainerLL`) で衝突回避

**別名取り込みの利点**:
- SL 公式の進化と FS の改修を完全に切り離せる
- AYAstorm 側の既存ファイルを変更しないので、回帰リスクなし
- Phase 1 以降で「LL 版の機能を改善するか、FS 版を改善するか」を独立に判断できる

**別名取り込みのデメリット**:
- クラス名/ファイル名がプロジェクト内で 2 系統存在する
- 今後 SL 公式から merge する時、別名管理を継続する必要がある

判断は Claude Code が事前調査して提案、AYA が決定。

---

## 4. 進め方

Phase 0 を以下のステップで進めます。各ステップでビルド確認 + AYA 承認を取ります。

### Step 1: ブランチ作成と環境確認

#### 1.1 ブランチ作成

```bash
cd ~/work_firestorm/phoenix-firestorm
git status  # ayastorm-release で clean を確認
git checkout -b feature/llfloaterimcontainer-coexist
```

#### 1.2 環境確認

```bash
# sl-upstream リモートの確認 (前回登録済み)
git remote -v | grep sl-upstream

# ベース commit の確認
git rev-parse HEAD

# build-linux-x86_64 の状態確認
ls -d build-linux-x86_64/ 2>&1
```

#### 1.3 出発点ビルド試行 (オプション)

ブランチ作成直後の状態で 1 度ビルドして、ベースが動くことを確認。ビルド手順は **1.4 参照**。

→ 成功したら起動して FS 版 Chat Window が普段通り動くことを確認。これが **回帰テストの基準点**。

#### 1.4 AYA 環境の実際のビルド・インストール手順

**フルビルド** (新規ファイル追加・CMakeLists.txt 変更時は必須):

```bash
cd ~/work_firestorm/phoenix-firestorm
source .venv/bin/activate
export AUTOBUILD_VARIABLES_FILE=$HOME/work_firestorm/fs-build-variables/variables
autobuild configure -A 64 -c ReleaseFS_open -- \
    --fmodstudio -DLL_TESTS:BOOL=FALSE --package --chan AYAstorm-release
autobuild build -A 64 -c ReleaseFS_open --no-configure
```

**インクリメンタルビルド** (既存ファイルのみ変更時):

```bash
autobuild build -A 64 -c ReleaseFS_open --no-configure
```

**インストール・起動**:

```bash
cd build-linux-x86_64/newview/packaged
rm -rf ~/ayastorm/
rm -rf ~/.local/share/applications/ayastorm-viewer.desktop
./install.sh
rm -rf ~/.ayastorm_x64/cache/
```

### Step 2: 依存解析と取り込みマトリクスの作成 (v2 で大幅強化)

リアクティブな「ビルドエラーを見て依存を継ぎ足す」アプローチではなく、**事前に依存マトリクスを作成して取り込み順序を決定する**。

#### 2.1 SL 公式版 `llfloaterimcontainer.cpp/.h` の存在確認

```bash
cd ~/work_firestorm/phoenix-firestorm

# SL 公式版に存在するか
git cat-file -e sl-upstream/main:indra/newview/llfloaterimcontainer.cpp && echo "SL exists: cpp"
git cat-file -e sl-upstream/main:indra/newview/llfloaterimcontainer.h && echo "SL exists: h"

# AYAstorm 側に存在するか
ls indra/newview/llfloaterimcontainer.cpp indra/newview/llfloaterimcontainer.h 2>&1
```

#### 2.2 SL 公式版 `llfloaterimcontainer.cpp` の include 依存解析

```bash
# SL 公式版が依存している header の一覧
git show sl-upstream/main:indra/newview/llfloaterimcontainer.cpp \
  | grep -E '^#include[[:space:]]+"' \
  | sort -u > /tmp/aya_phase0_sl_includes.txt

# AYAstorm 側のディレクトリ tree
git ls-tree -r --name-only HEAD indra/newview/ \
  | grep -E '\.h$' > /tmp/aya_phase0_ayastorm_headers.txt

# AYAstorm に存在しない header をリストアップ
echo "=== SL が依存していて AYAstorm に存在しない header ==="
while read inc_line; do
  hname=$(echo "$inc_line" | sed -E 's/^#include[[:space:]]+"([^"]+)".*/\1/')
  if ! grep -q "/$hname$" /tmp/aya_phase0_ayastorm_headers.txt; then
    echo "MISSING: $hname"
  fi
done < /tmp/aya_phase0_sl_includes.txt
```

#### 2.3 SL 公式版の forward declaration 確認

```bash
# class FooBar; のような forward declaration を検出
git show sl-upstream/main:indra/newview/llfloaterimcontainer.h \
  | grep -E '^class[[:space:]]+[A-Z][a-zA-Z]+;' \
  | sort -u
```

これらの class が AYAstorm に存在するか確認。存在しないなら追加取り込み候補。

#### 2.4 関連 XUI ファイルの依存確認

```bash
# SL 公式版の cpp で参照されている XUI ファイル名
git show sl-upstream/main:indra/newview/llfloaterimcontainer.cpp \
  | grep -oE '"[^"]+\.xml"' \
  | sort -u

# 抽出された XUI が SL 公式版に存在するか
for xml_name in $(git show sl-upstream/main:indra/newview/llfloaterimcontainer.cpp | grep -oE '"[^"]+\.xml"' | tr -d '"' | sort -u); do
  path="indra/newview/skins/default/xui/en/$xml_name"
  if git cat-file -e sl-upstream/main:$path 2>/dev/null; then
    echo "SL exists: $path"
  fi
done
```

#### 2.5 関連クラス (LLConversationModel 等) の調査

```bash
# SL 公式版で関連するクラスのファイルを探す
git ls-tree -r --name-only sl-upstream/main indra/newview/ \
  | grep -iE 'llconversation|llparticipantlist' | sort -u

# それぞれが AYAstorm に存在するかチェック
for f in $(git ls-tree -r --name-only sl-upstream/main indra/newview/ | grep -iE 'llconversation|llparticipantlist' | sort -u); do
  if [ -f "$f" ]; then
    echo "EXISTS in AYAstorm: $f"
  else
    echo "MISSING in AYAstorm: $f"
  fi
done
```

#### 2.6 取り込みマトリクスの作成

Claude Code が以下のマトリクスを `/tmp/aya_phase0_matrix.md` に書き出す:

```
| ファイル | SL 存在 | AYA 存在 | <FS:Ansariel> 件数 | 推奨方式 |
|----------|---------|----------|-------------------|---------|
| llfloaterimcontainer.cpp | ✓ | ✓ | XX | 別名取り込み |
| llfloaterimcontainer.h | ✓ | ✓ | XX | 別名取り込み |
| llconversationmodel.cpp | ✓ | ? | ? | ... |
| floater_im_container.xml | ✓ | ? | - | ... |
```

**v2 ポイント**: マーカー件数の確認方法
```bash
# AYAstorm 側にファイルが存在する場合のみ意味がある
for f in indra/newview/llfloaterimcontainer.cpp indra/newview/llfloaterimcontainer.h; do
  if [ -f "$f" ]; then
    count=$(grep -c '<FS:Ansariel>' "$f" 2>/dev/null || echo 0)
    echo "$f: <FS:Ansariel> count = $count"
  fi
done
```

#### 2.7 取り込み順序と方式を AYA に提案

マトリクスを基に、Claude Code は以下を AYA に提示:

- 取り込み対象ファイル一覧 (依存順)
- 各ファイルの推奨方式 (単純置換 / 別名取り込み)
- 想定される commit 数
- 想定される追加調整 (新クラス名、include 修正等)

AYA が承認したら Step 3 へ。承認しなければ調査やり直し or 方針変更。

### Step 3: ファイルの取り込み (1 commit = 1 ファイル または小グループ)

#### 3.0 取り込み方式の確定 (v2 で追加)

Step 2.6 のマトリクスに基づき、各ファイルについて:

##### 3.0.1 単純取り込み (AYAstorm に存在しない、または `<FS:Ansariel>` ゼロ件)

```bash
git checkout sl-upstream/main -- <path>
```

#### 3.0.2 別名取り込み (推奨パターン、AYAstorm に存在 + `<FS:Ansariel>` あり)

```bash
# SL 公式版を別名で取り込み
git show sl-upstream/main:indra/newview/llfloaterimcontainer.cpp \
  > indra/newview/llfloaterimcontainer_ll.cpp
git show sl-upstream/main:indra/newview/llfloaterimcontainer.h \
  > indra/newview/llfloaterimcontainer_ll.h

# クラス名を変更 (LLFloaterIMContainer → LLFloaterIMContainerLL)
sed -i 's/class LLFloaterIMContainer\b/class LLFloaterIMContainerLL/g' \
  indra/newview/llfloaterimcontainer_ll.h indra/newview/llfloaterimcontainer_ll.cpp
sed -i 's/LLFloaterIMContainer::/LLFloaterIMContainerLL::/g' \
  indra/newview/llfloaterimcontainer_ll.cpp
sed -i 's/\bLLFloaterIMContainer\b/LLFloaterIMContainerLL/g' \
  indra/newview/llfloaterimcontainer_ll.cpp

# include guard も変更
sed -i 's/LL_LLFLOATERIMCONTAINER_H/LL_LLFLOATERIMCONTAINERLL_H/g' \
  indra/newview/llfloaterimcontainer_ll.h
sed -i 's/llfloaterimcontainer\.h/llfloaterimcontainer_ll.h/g' \
  indra/newview/llfloaterimcontainer_ll.cpp

# CMakeLists.txt にも追加 (別 commit)
```

⚠️ 上記 sed は単純化した例。実際は Claude Code が SL 公式版 cpp を読んで、置換対象を慎重に特定する必要あり (例: `LLFloaterIMContainer::sActiveSession` のような static 変数名、`LLFloaterIMContainerSelf` のような派生型名等)。

##### 3.0.3 merge (高度、Phase 0 では避ける)

`<FS:Ansariel>` ブロックを保持しつつ SL 公式の進化を取り込む。複雑なので Phase 0 では推奨せず、別名取り込みで代替。

#### 3.1 第 1 取り込み: `llfloaterimcontainer.cpp/.h` (例)

Step 2.6 のマトリクスで決まった方式で取り込み実行。

##### 単純取り込みの場合
```bash
git checkout sl-upstream/main -- indra/newview/llfloaterimcontainer.cpp
git checkout sl-upstream/main -- indra/newview/llfloaterimcontainer.h
git diff --staged indra/newview/llfloaterimcontainer.cpp | head -50
```

##### 別名取り込みの場合
```bash
# Step 3.0.2 のスクリプト的な手順を実行
# 結果として indra/newview/llfloaterimcontainer_ll.{h,cpp} が新規作成される
git status  # 新規ファイルとして検出されるはず
git diff indra/newview/llfloaterimcontainer_ll.cpp | head -50
```

→ AYA に diff を提示して承認を得る。承認後 commit。

commit メッセージ (案):

**単純取り込みの場合**:
```
Phase 0-1: Import LLFloaterIMContainer from sl-upstream/main

(中略)
```

**別名取り込みの場合**:
```
Phase 0-1: Import SL upstream LLFloaterIMContainer as LLFloaterIMContainerLL (parallel-implementation)

To preserve AYAstorm's existing FS modifications in the original
llfloaterimcontainer.cpp, the SL upstream version is imported under
a parallel name (LLFloaterIMContainerLL in llfloaterimcontainer_ll.cpp/.h).
This allows the FS-modified version and the SL canonical version to
coexist without merge conflicts.

The class name and include guards are renamed to avoid symbol collision.
The FS version (LLFloaterIMContainer) is preserved untouched.

Build status: TBD (verify in next step)
```

#### 3.2 ビルド試行

新規ファイルを追加した場合はフルビルド (configure 込み) が必要。手順は **1.4 参照**。
ログを残す場合は build コマンドに `2>&1 | tee /tmp/aya_phase0_build.log` を追加。

ビルドエラーが出たら:
- Claude Code にエラーログを共有
- 個別対処 (include 不足、シンボル不一致等)

#### 3.3 第 2 取り込み: 関連 XUI ファイル

ビルドが通ったら次に進む:

##### 単純取り込みの場合
```bash
git checkout sl-upstream/main -- indra/newview/skins/default/xui/en/floater_im_container.xml
```

##### 別名取り込みの場合 (XUI も別名にするか?)
- 別名にする: `floater_im_container_ll.xml` として新規作成、registry も別名で登録
- 別名にしない: 既存の AYAstorm `floater_im_container.xml` (もし存在すれば) を変更せず、SL 公式版を別ファイル名で配置

判断: Step 2.6 のマトリクスで AYAstorm に既存の `floater_im_container.xml` があるか次第。なければ単純配置でよい。

#### 3.4 第 3 取り込み以降: 依存ファイル

Step 2.6 のマトリクスで判明した依存ファイルを順次取り込む。**ビルドエラー駆動ではなく、マトリクス駆動で進める**。

各取り込みで:
1. マトリクスで決まった方式で取り込み
2. ビルド試行
3. エラーがあれば対処 (新規依存が判明することもある、その時はマトリクス更新)
4. AYA 承認
5. commit

### Step 4: registry 登録 (v2 で衝突確認を強化)

#### 4.0 registry 名衝突確認 (v2 で追加)

```bash
cd ~/work_firestorm/phoenix-firestorm

# "im_container" が既に登録されているか確認
echo "=== 既存の im_container 登録 ==="
grep -n '"im_container"' indra/newview/llviewerfloaterreg.cpp || echo "(未使用)"

# 同様に他の候補名も確認
for key in "ll_im_container" "ll_conversations" "im_container_ll"; do
  echo "=== $key ==="
  grep -n "\"$key\"" indra/newview/llviewerfloaterreg.cpp || echo "(未使用)"
done
```

判定:
- `"im_container"` が **未使用** → そのまま使える
- `"im_container"` が **既に FS 版で使用中** → 別の registry key に変更 (例: `"ll_im_container"`)

#### 4.1 LL 版を registry に追加登録

`indra/newview/llviewerfloaterreg.cpp` で:

```cpp
// 既存の FS 版登録は保持 (触らない)
// (既存コード)

// <FS:AYA> Add LL version coexistence (Phase 0)
LLFloaterReg::add("ll_im_container", "floater_im_container_ll.xml",
    (LLFloaterBuildFunc)&LLFloaterReg::build<LLFloaterIMContainerLL>);
// </FS:AYA>
```

⚠️ XUI ファイル名と registry key 名は **Step 4.0 と Step 3 の決定** に従う。

include の追加も忘れずに:

```cpp
// <FS:AYA> Add LL version coexistence (Phase 0)
#include "llfloaterimcontainer_ll.h"
// </FS:AYA>
```

→ AYA に diff 提示 → 承認 → commit。

### Step 5: メニューエントリ追加 (v2 でショートカット扱い明確化)

#### 5.0 メニュー XUI ファイルの確認

```bash
cd ~/work_firestorm/phoenix-firestorm

# Communicate メニュー周辺の確認
grep -n 'Communicate\|conversations\|im_container' \
  indra/newview/skins/default/xui/en/menu_viewer.xml | head -20
```

#### 5.1 menu_viewer.xml の編集

Communicate メニューに新エントリを追加 (Step 4 で確定した registry key を使う):

```xml
<!-- 既存の Communicate メニュー内 -->
<!-- <FS:AYA> Add LL version Chat Window menu entry (Phase 0) -->
<menu_item_check
 label="Conversations (LL Style)"
 name="LL Conversations">
    <menu_item_check.on_check
     function="Floater.Visible"
     parameter="ll_im_container" />
    <menu_item_check.on_click
     function="Floater.ToggleOrBringToFront"
     parameter="ll_im_container" />
</menu_item_check>
<!-- </FS:AYA> -->
```

**v2 注意点**: ショートカットキー (shortcut="control|T") は **Phase 0 では割り当てない**。Firestorm 既存のショートカットと衝突する可能性が高い。Phase 1 以降で衝突調査して割り当てる。

→ AYA に diff を提示して承認 → commit。

### Step 6: ビルド & XUI/C++ 整合性検証 & 起動 & 動作確認 (v2 で大幅強化)

#### 6.1 完全ビルド

ビルド手順は **1.4 参照**。ログを残す場合は build コマンドに `2>&1 | tee /tmp/aya_phase0_final_build.log` を追加。

#### 6.2 XUI と C++ の整合性検証 (v2 で追加、起動前必須)

ビルドが通っても、XUI と C++ の紐付けが正しいとは限らない。**起動前に必ず実行**:

##### 6.2.1 C++ 側で参照する XUI ノード名を抽出

```bash
# 取り込んだ cpp ファイル (別名取り込みの場合 _ll サフィックス)
TARGET_CPP="indra/newview/llfloaterimcontainer_ll.cpp"  # または取り込み方式に応じて

# getChild<>("xxx") のパターンを抽出
echo "=== C++ 側で参照されている XUI ノード名 ==="
grep -oE 'getChild<[^>]+>\s*\(\s*"[^"]+"' "$TARGET_CPP" \
  | grep -oE '"[^"]+"' \
  | sort -u > /tmp/aya_phase0_cpp_refs.txt
cat /tmp/aya_phase0_cpp_refs.txt

# findChild<>("xxx") も同様
echo "=== findChild の参照 ==="
grep -oE 'findChild<[^>]+>\s*\(\s*"[^"]+"' "$TARGET_CPP" \
  | grep -oE '"[^"]+"' \
  | sort -u >> /tmp/aya_phase0_cpp_refs.txt

# getChildView, getName 系も確認
echo "=== その他の名前参照 ==="
grep -E 'getChildView|getControlName|getName|setName' "$TARGET_CPP" | head -20
```

##### 6.2.2 XUI 側で定義されているノード名を抽出

```bash
TARGET_XML="indra/newview/skins/default/xui/en/floater_im_container.xml"  # または別名

# name="xxx" のパターンを抽出
echo "=== XUI 側で定義されているノード名 ==="
grep -oE 'name="[^"]+"' "$TARGET_XML" \
  | sort -u > /tmp/aya_phase0_xml_names.txt
cat /tmp/aya_phase0_xml_names.txt
```

##### 6.2.3 突合せ

```bash
echo "=== C++ で参照されているが XUI に存在しないノード (要確認) ==="
comm -23 \
  <(sort -u /tmp/aya_phase0_cpp_refs.txt) \
  <(sort -u /tmp/aya_phase0_xml_names.txt | sed 's/name=//')
# (上記は単純比較、実際はクォート除去等の前処理が必要、Claude Code が調整)
```

不一致が出た場合:
- **C++ 側にあるが XUI 側にない**: 起動時に `getChild<>` が `nullptr` を返し、後で segfault するリスク。原因調査 (XUI ファイルの取り込み漏れ?)。
- **XUI 側にあるが C++ 側にない**: 起動には影響しないが、未使用ノード (デザイン上の意図を確認)。

##### 6.2.4 結果を AYA に報告

Claude Code は突合せ結果を AYA に明示:
- 「整合確認 OK、起動試行可」
- 「不整合あり、原因調査必要」

#### 6.3 起動

整合確認 OK の場合のみ起動:

```bash
cd build-linux-x86_64/newview/packaged
./firestorm
```

#### 6.4 AYA の動作確認チェックリスト

AYA が起動後に確認すること:

- [ ] AYAstorm が普段通り起動する (クラッシュしない)
- [ ] FS 版 Chat Window が普段通り動く (`FSChatWindow=1` のデフォルト)
- [ ] IM の送受信ができる
- [ ] デスクトップ通知が出る
- [ ] **メニュー > Communicate > "Conversations (LL Style)" を開く** (LL 版の表示確認)
- [ ] LL 版 Chat Window が表示される (中身は空でも OK)
- [ ] LL 版 Chat Window を閉じることができる
- [ ] Console や Log にクラッシュ/エラーが大量に出ていない

### Step 7: Phase 0 完了判定

#### 7.1 成功条件

以下が全て満たされれば Phase 0 成功:
- ビルドが通る
- XUI/C++ 整合確認 OK
- AYAstorm が起動する
- FS 版が壊れていない
- LL 版 Chat Window がメニューから開ける
- 致命的な console エラーなし

#### 7.2 部分成功の扱い

- LL 版 Chat Window は開けるが中身が空 → **Phase 0 成功** (中身は Phase 1 以降)
- LL 版 Chat Window は開けるが何かのボタンを押すとクラッシュ → **Phase 0 部分成功**、Phase 1 でクラッシュ箇所を特定
- LL 版 Chat Window が開かない、または表示が完全に崩れる → **Phase 0 失敗**、原因調査

---

## 5. AYA への引き継ぎ・次のステップ

### Phase 0 が成功したら

選択肢:
- **続ける**: Phase 1 で Conversation List の Nearby Chat 表示を有効化、IM Session の表示を実装
- **満足**: 現状で十分なら Phase 1 をやらない選択肢もアリ
- **改善**: Phase 0 で気づいた小さな改善点を積み重ねる

### Phase 0 が失敗したら

選択肢:
- **原因調査**: なぜ動かないか深掘り (LL 版コードと AYAstorm 環境の不整合、化石度等)
- **撤退**: Phase 0 で分かった工数感を踏まえて、別アプローチを考える (FS 版改造等)
- **保留**: しばらく置いて、Firestorm 本家の動向を見る

---

## 6. 既知の落とし穴

過去のセッションで遭遇した問題を Claude Code が把握しておくべきもの。

### 6.1 `gntp-growl` キャッシュ汚染

`autobuild.xml` の `gntp-growl` 定義は darwin64 (Mac) 用のみ。しかし `autobuild install` が Linux 環境で `gntp-growl` を install しようとし、`libnotify-0.4.4-linux-20101003.tar.bz2` をダウンロードする。アーカイブ名 `libnotify` と autobuild.xml の `gntp-growl` が不整合になり、`autobuild install` が中断する。

#### Phase 0 での扱い

- **`build-linux-x86_64/` を `rm -rf` しない**: これにより autobuild install が走らないので問題に遭遇しない
- **AYA の libnotify キャッシュ (`/var/tmp/$USER/install.cache/libnotify-*.tar.bz2`) は触らない**: AYAstorm の通知機能 (`growlmanager.cpp`) で使われている
- **`autobuild build --no-configure` を使う**: configure 段階を飛ばすので install も走らない

通常の作業はインクリメンタルビルド (`--no-configure`) で行う。再 configure が必要な場合のみ問題に遭遇する可能性あり、その時に対処を考える。

### 6.2 NDOF / open-libndofdev

`indra/cmake/NDOF.cmake` は Linux で `use_prebuilt_binary(open-libndofdev)` を呼ぶ。`autobuild.xml` には Linux 用 `open-libndofdev` 定義あり。

#### Phase 0 での扱い

- インクリメンタルビルドでは触らないので問題なし
- もし configure が必要になったら、`-DNDOF:BOOL=OFF` で緊急回避可能

### 6.3 FMOD 手動セットアップ

FMOD は AYAstorm 標準のビルド手順に含まれない。手動でローカルビルド + `autobuild installables edit` で登録される。

#### Phase 0 での扱い

- `build-linux-x86_64/` を `rm -rf` しない限り FMOD 登録は維持される
- 触らない

### 6.4 LL 版コードと AYAstorm の API 不整合

LL 版 `LLFloaterIMContainer` の取り込みで、依存している LL クラスの API が AYAstorm 既存版と異なる可能性。Claude Code は注意:

- `getChild<>` で取得するノード名が XUI に存在するか (Step 6.2 で検証)
- 関数シグネチャの整合性 (返り値型、引数等)
- include 依存関係 (Step 2 で事前解析)

ビルドエラーで判明することが多い。1 つずつ対処。

### 6.5 別名取り込みのクラス名置換漏れ (v2 で追加)

別名取り込みで `LLFloaterIMContainer` → `LLFloaterIMContainerLL` に置換する時、以下の置換漏れに注意:

- `LLFloaterIMContainerSelf` のような派生型 (誤って `LLFloaterIMContainerLLSelf` にしてしまうと別物)
- `sLLFloaterIMContainer` のような static 変数の prefix
- マクロ名 (例: `LLFloaterIMContainerKey`)
- friend class 宣言
- include guard
- ファイル名参照 (例: `floater_im_container.xml` を別名にする場合は cpp 内の string も置換)

→ Claude Code は SL 公式版を grep で全文走査して、置換対象を慎重に特定すること。

### 6.6 registry key 衝突 (v2 で追加)

`LLFloaterReg::add("im_container", ...)` で登録する key が既に Firestorm で使われている可能性。Step 4.0 で必ず事前確認。

衝突した場合の対処:
- 別の key に変更 (例: `"ll_im_container"`)
- メニューエントリの parameter も同じ key に揃える

---

## 7. 運用ルール

### 7.1 commit に関するルール

- **commit メッセージは AYA が指定**: Claude Code が draft を提案、AYA が承認・修正
- **形式**: `-m "title" -m "body"` 形式必須
- **ヒアドキュメント禁止**: AYA が過去のセッションで決めた運用
- **body 内の `"` は `\"` でエスケープ**

### 7.2 編集に関するルール

- **AYA の許可なく commit しない**: 編集して diff 提示 → AYA 承認 → commit
- **`git push` 禁止**: Phase 0 完走時、または AYA が明示指示した時のみ
- **`git rm` / `git reset --hard` 禁止**: AYA の明示承認が必要
- **`rm -rf build-linux-x86_64/` 禁止**: ビルド環境保護

### 7.3 調査と編集の分離

- **調査ターン**: read-only、結果を AYA に提示
- **編集ターン**: AYA の指示に基づいて編集
- 1 つのターンで両方やらない

### 7.4 Claude Code 自身の振る舞い

- **AYA を待たずに先回りで判断しない**: 不明な点があれば AYA に質問
- **「次に commit しましょうか?」のような誘導禁止**: AYA が判断する
- **エラーが出たら正直に報告**: 隠蔽せず

### 7.5 トラブル時の判断基準

- **ビルドエラー**: Claude Code が解析して対処案を提示
- **動作不良**: AYA が動作確認、現象を Claude Code に共有
- **判断が分かれたら**: AYA の判断を尊重 (Claude Code は補助役)

### 7.6 別名取り込み時の追加ルール (v2 で追加)

- **クラス名置換は機械的にやらない**: Claude Code は SL 公式版コードを精読して、置換対象を慎重に列挙
- **置換後に grep 検証**: `grep -n 'LLFloaterIMContainer' indra/newview/llfloaterimcontainer_ll.cpp` で旧クラス名が残っていないか確認
- **AYA に置換結果の diff を提示**: 機械的処理に見えても、AYA が念のため目視確認

---

## 8. トラブル対処ガイド

### 8.1 ビルドエラー

#### 症状: 取り込んだファイルで undefined symbol

**原因**: 依存ファイルが取り込まれていない or 既存と不整合

**対処**:
1. エラーメッセージで参照されているシンボルを確認
2. SL 公式版でそのシンボルが定義されているファイルを探す
3. そのファイルも取り込み候補に追加 (Step 2 のマトリクス更新)
4. AYA に提案 → 承認 → 取り込み

#### 症状: include 不足エラー

**対処**:
- ファイル内の `#include` を確認、不足している header を追加
- forward declaration で済むなら header より forward 宣言を追加

#### 症状: 関数シグネチャ不一致

**原因**: AYAstorm の依存クラスが SL 公式と API 違い

**対処**:
- AYA と相談して、AYAstorm 既存 API に合わせるか、依存クラスも更新するか判断

### 8.2 ビルドが通っても起動でクラッシュ

#### 症状: 起動時 segfault

**原因**: XUI 読み込み時の `getChild<>` 失敗、null pointer access

**対処**:
- console / log を確認
- どの XUI ノードで失敗しているか特定
- XUI ファイル内のノード名を確認
- **Step 6.2 の整合確認をスキップしていなかったか?** スキップしていたら遡って実行

#### 症状: 起動はするが LL 版 Chat Window が開かない

**原因**: registry 登録の失敗、メニューエントリの設定ミス

**対処**:
- console で `LLFloaterReg` 関連のエラーを確認
- `llviewerfloaterreg.cpp` の登録部分を確認
- registry key とメニューの parameter が一致しているか

### 8.3 ビルドが通って起動もできるが、Chat Window 中身が空

**これは Phase 0 のゴール内** (中身は Phase 1 以降の課題)。Phase 0 成功と判定。

### 8.4 別名取り込みでビルドエラー (v2 で追加)

#### 症状: クラス名置換漏れによるエラー

**対処**:
```bash
# 旧クラス名が残っていないか全件 grep
grep -n 'LLFloaterIMContainer\b' indra/newview/llfloaterimcontainer_ll.cpp

# friend 宣言や static 変数の prefix 等を個別確認
grep -E 'friend|static|extern' indra/newview/llfloaterimcontainer_ll.cpp | head
```

残存箇所を sed 等で追加置換。

---

## 9. Phase 0 のスコープ外

以下は Phase 0 の対象外。Phase 1 以降または別作業:

- LL 版 Chat Window 内の Nearby Chat の表示・操作
- LL 版 Chat Window 内の IM Session の表示・操作
- LL 版から FS 版 IM への遷移ブリッジ
- 通信レイヤーの変更
- `<FS:Ansariel> [FS communication UI]` ブロックの精査
- `FSChatDispatcher` のような共存切り替え機構
- viewer 全体の `FSFloaterIM` 等の呼び出し書き換え
- ショートカットキー割り当て (Phase 1 以降で空きキー調査)

---

## 10. 引き継ぎ運用

### 10.1 セッション開始時に Claude Code に共有するもの

新セッションで Claude Code に以下を共有:

1. このドキュメント (`ayastorm-phase0-llcontainer-coexist-v2.md`)
2. AYAstorm の現在のブランチ・commit hash
3. 進行中の Step (どこまで完了したか)
4. 直近のビルド状況

### 10.2 Phase 0 完了後

完了したらこのドキュメントの末尾に **完了記録** を追記:

- 完了日時
- 取り込んだファイル一覧 (取り込み方式も)
- 採用した registry key 名
- commit hash
- 動作確認結果
- 気づき・課題

---

## 11. 最初のターンで Claude Code がやること

新セッション開始時、Claude Code は以下を実行:

### 11.1 環境確認

```bash
cd ~/work_firestorm/phoenix-firestorm
git status
git log --oneline -5
ls -d build-linux-x86_64/
```

### 11.2 sl-upstream の状態確認

```bash
git remote -v | grep sl-upstream
git cat-file -e sl-upstream/main:indra/newview/llfloaterimcontainer.cpp && echo "exists"
```

### 11.3 取り込み候補の事前調査 (Step 2 の準備)

Step 2 のコマンド (依存解析、マトリクス作成) を実行する前段階として、最初のターンでは:

```bash
# AYAstorm に既に存在する LL 版ファイルの確認
ls indra/newview/llfloaterimcontainer.cpp 2>&1
ls indra/newview/llfloaterimcontainer.h 2>&1

# 存在する場合の <FS:Ansariel> マーカー件数
for f in indra/newview/llfloaterimcontainer.cpp indra/newview/llfloaterimcontainer.h; do
  if [ -f "$f" ]; then
    count=$(grep -c '<FS:Ansariel>' "$f" 2>/dev/null || echo 0)
    echo "$f: <FS:Ansariel> count = $count"
  fi
done

# SL 公式版の概況
echo "=== SL 公式版の行数 ==="
git show sl-upstream/main:indra/newview/llfloaterimcontainer.cpp | wc -l
git show sl-upstream/main:indra/newview/llfloaterimcontainer.h | wc -l

# AYAstorm 版の行数
echo "=== AYAstorm 版の行数 ==="
ls indra/newview/llfloaterimcontainer.cpp 2>/dev/null && wc -l indra/newview/llfloaterimcontainer.cpp
ls indra/newview/llfloaterimcontainer.h 2>/dev/null && wc -l indra/newview/llfloaterimcontainer.h
```

### 11.4 AYA への提案

調査結果を AYA に提示し、Step 1 (ブランチ作成) と Step 2 (依存解析) に進んでよいか確認。

特に以下を AYA に明示:
- 取り込み方式の推奨 (単純取り込み / 別名取り込み)
- `<FS:Ansariel>` マーカー件数から見える FS 改修の規模
- 想定される work load (commit 数、所要日数)

---

## 12. Phase 0 完了記録

**完了日時**: 2026-04-28

### 取り込んだファイル一覧

| ファイル | 方式 | 備考 |
|---|---|---|
| `indra/newview/llfloaterimcontainer_ll.h` | 新規作成 (最小スタブ) | SL 版からは取り込まず、`LLFloater` を継承した独自最小実装 |
| `indra/newview/llfloaterimcontainer_ll.cpp` | 新規作成 (最小スタブ) | コンストラクタと `postBuild()` のみ |
| `indra/newview/skins/default/xui/en/floater_im_container_ll.xml` | 新規作成 | 空の `<floater>` (ウィンドウ枠のみ) |
| `indra/newview/CMakeLists.txt` | 既存変更 (+2 行) | `_ll.cpp/.h` を source list に追加 |
| `indra/newview/llviewerfloaterreg.cpp` | 既存変更 (+4 行) | include + registry 登録 |
| `indra/newview/skins/default/xui/en/menu_viewer.xml` | 既存変更 (+13 行) | Communicate メニューにエントリ追加 |

### 採用した設定

- **Registry key**: `"ll_im_container"`
- **XUI ファイル名**: `floater_im_container_ll.xml`
- **クラス名**: `LLFloaterIMContainerLL`
- **メニューラベル**: `Conversations (LL Style)`

### Commit

- **Branch**: `feature/llfloaterimcontainer-coexist`
- **Hash**: `1c6910839e`
- **出発点 (ayastorm-release HEAD)**: `33c58819161c9c47d25ebbe412726a3eb8734c65`

### 動作確認結果

- [x] ビルド成功 (configure + build)
- [x] AYAstorm 起動成功
- [x] Communicate メニュー > `Conversations (LL Style)` からウィンドウが開く
- [x] ウィンドウは空白 (Phase 0 仕様通り)
- [x] FS 版 `Conversations` (Ctrl+T) は従来通り動作
- [x] クラッシュなし

### 気づき・課題

- AYAstorm の LL Chat UI ファイル群 (`llfloaterimcontainer.cpp/.h`、`llfloaterimsession.h`、`llconversationmodel.h` 等) は **全て `#if 0` で丸ごと無効化** されていた。SL 公式版そのままの取り込みではなく最小スタブ方式を採用したのはこのため。
- Phase 1 で Conversation List を実装する場合、`#if 0` チェーンの段階的な再有効化、または FS 版クラスとのブリッジ実装が必要になる。
- ショートカットキーは未割り当て (仕様通り)。Phase 1 以降で空きキーを調査して割り当てる。

---

**Document End**

このドキュメントは AYA と Claude Code (前セッション) の対話を経て作成され、外部レビューを反映して v2 改訂されました。Phase 0 の成果は AYAstorm の `feature/llfloaterimcontainer-coexist` ブランチに記録されます。

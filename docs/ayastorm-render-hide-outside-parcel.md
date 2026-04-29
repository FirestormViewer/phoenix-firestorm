# 区画外オブジェクト非表示機能 (RenderHideOutsideParcel) 仕様書

**作成日**: 2026-04-29
**対象**: AYA + Claude Code
**作業ブランチ (予定)**: `feature/render-hide-outside-parcel`
**前提**: AYAstorm の `ayastorm-release` ブランチが正常にビルド・起動できる状態

## 改訂履歴

- **v1 (2026-04-29)**: 初版

---

## 0. このドキュメントの読み方 (Claude Code 向け)

1. **章 1〜3 を最初に熟読** (目的、設計方針)
2. **Phase A を完了させてからでないと Phase B には進まない**
3. **各ステップで AYA に diff を見せて承認を得る**
4. **AYA の許可なく commit / push / 大規模変更しない**
5. **ビルドは AYA が実行する**

---

## 1. 機能概要

### 1.1 何を作るか

「自分が立っている区画 (Parcel) の外にあるオブジェクトを描画しない」設定を AYAstorm に追加する。

3D 没入感を重視するユーザーが、自分の区画の隣で他人が置いた巨大プリムや看板等で視界を汚されることを避けるための機能。

### 1.2 二段階で実装する

| フェーズ | 内容 | このブランチで実装 |
|---------|------|-------------------|
| **Phase A** | 自分の現在区画外のオブジェクトを単純に非表示 | ✅ |
| **Phase B** | 隣接区画の description に特殊タグがあれば、その区画は描画許可 (オプトイン) | ❌ (別ブランチで後日) |

Phase A だけでも実用機能として完結するため、まず Phase A を完成させる。Phase B は description 取得のための非同期リクエスト管理が必要で複雑なので、機能を切り分ける。

### 1.3 ゴール (Phase A)

- 設定 ON で自分の区画外のオブジェクトが描画されない
- アバター・添付物・HUD は常に描画される (除外オプションで制御)
- 自分の区画内のオブジェクトは通常通り描画される
- 設定 OFF (デフォルト) で従来通りの挙動
- ビルドが通り、起動・移動・テレポート時にクラッシュしない

---

## 2. 役割分担

### 2.1 AYA

- ビルド実行 (`autobuild build` 等)
- 動作確認:
  - 自分の区画内に立つ → 周囲が普通に見える状態
  - 設定 ON にする → 区画外オブジェクトが消える
  - 設定 OFF にする → 復帰
  - 隣の区画にテレポートで移動 → 描画対象が切り替わる
  - アバター・添付物が消えないことの確認
- 各 commit の diff レビュー、コミットメッセージ確認

### 2.2 Claude Code

- 設定項目の追加 (settings.xml)
- 描画フックの実装 (llvovolume.cpp 等)
- 区画判定キャッシュの実装 (パフォーマンス対策)
- 設定変更リスナー (llviewercontrol.cpp)
- preferences UI への項目追加 (任意, Phase A 終盤)
- 既存の挙動への影響範囲調査

### 2.3 重要な観点

> AYA は「見た目」で表面的な動作は判断できるが、内部のパフォーマンス劣化や境界条件の漏れは検出しにくい。

特に注意:
- **毎フレーム数千オブジェクト × `inAgentParcel` 呼び出し** はそのままだと重い → キャッシュ機構必須
- **区画境界をまたぐリンクセット** はルートプリム位置で判定 → 子プリムが境界外でもリンクで一括判定
- **アバター除外漏れ** で自分や近隣が消えるとほぼバグ扱い → アバター系の早期 return を最初に書く

---

## 3. 設計方針

### 3.1 描画フックの位置

判定を入れる候補:

| 候補 | メリット | デメリット |
|------|---------|------------|
| `LLVOVolume::isVisible()` | volume 系 (プリム) のみに自然に効く | mesh, sculpt 等の派生には別途確認必要 |
| `LLDrawable::setVisible()` | 全 drawable を一律に処理できる | アバター除外などの分岐が複雑 |
| `LLPipeline::renderObjects()` | 描画直前で一括フィルタ | drawable レイヤより下で複雑 |

**第一選択**: `LLVOVolume::isVisible()` にフックを入れる。アバター (`LLVOAvatar`) は別系統なので影響を受けない。HUD・添付物の判定ロジックは既存メソッド (`isHUDAttachment()`, `isAttachment()`) を利用。

### 3.2 区画判定キャッシュ

- `LLDrawable` (もしくは `LLViewerObject`) に以下を持たせる:
  - `mLastParcelCheckFrame` (S32): 最後に判定したフレーム番号
  - `mLastParcelCheckResult` (bool): 結果
- フレーム内では再計算しない
- 区画境界は `LLViewerParcelMgr::mAgentParcelChanged` のタイミングで全 drawable のキャッシュを無効化 (フレーム番号を進めるだけで OK)

### 3.3 アバター・添付物の扱い

以下は **常に描画する** (= 設定無効と同じ扱い):

- `LLVOAvatar` 全般 (`LLVOVolume::isVisible()` には来ないが念のため)
- HUD (`isHUDAttachment()` true)
- アバター添付物 (`isAttachment()` true → アバター本体の判定に従う)
- 自分のオブジェクト (オーナー = `gAgentID`) ※オプションで切替可能にしておく
- パーティクル系: 暫定 Phase A では現状維持 (パーティクルは別パイプライン)

### 3.4 設定 OFF 時の影響

設定 OFF の場合は早期 return で従来コードと同じパス。性能劣化ゼロを目指す。

---

## 4. 設定項目

`indra/newview/app_settings/settings.xml` に追加。命名は AYAstorm の慣例 (FS で始まる Firestorm 拡張に倣い `FS` プレフィックス) に合わせる。

| キー | 型 | デフォルト | 説明 |
|------|-----|-----------|------|
| `FSRenderHideOutsideParcel` | Boolean | false | この機能の ON/OFF |
| `FSRenderHideOutsideParcelKeepAvatars` | Boolean | true | アバター・添付物・HUD は常に描画 |
| `FSRenderHideOutsideParcelKeepOwn` | Boolean | true | 自分が所有するオブジェクトは常に描画 |

Phase B で追加予定 (このブランチでは触らない):

- `FSRenderHideOutsideParcelTag` (String): description 内のオプトインタグ (例 `[AYAVIS]`)
- `FSRenderHideOutsideParcelTagCaseSensitive` (Boolean)

---

## 5. Phase A 実装ステップ

各ステップ完了後に AYA に diff を見せて承認 → ビルド確認 → 次へ。

### Step 1: 設定項目の追加

- `indra/newview/app_settings/settings.xml` に `FSRenderHideOutsideParcel` 等 3 項目を追加
- ビルド確認のみ (機能はまだ何もしない)

### Step 2: 設定変更リスナー登録

- `indra/newview/llviewercontrol.cpp` で `FSRenderHideOutsideParcel` の `signal_ptr` を取得
- 値変更時に「全 drawable のキャッシュを無効化する」ためのフラグを立てる仕組みを用意 (実体は Step 3 で使う)

### Step 3: drawable へのキャッシュメンバ追加

- `indra/newview/lldrawable.h` に
  - `S32 mLastParcelCheckFrame{-1};`
  - `bool mLastParcelCheckResult{true};`
- 初期化を constructor に追加
- ヘッダ変更による再ビルド範囲は広い → このコミットは独立に切る

### Step 4: `LLVOVolume::isVisible()` への判定挿入

- 既存の戻り値ロジックの最後に:
  ```cpp
  if (LLPipeline::sRenderHideOutsideParcel
      && !isHUDAttachment()
      && !isAttachment()
      && !isInCurrentAgentParcelCached())
  {
      return false;
  }
  ```
  のようなチェックを追加
- `isInCurrentAgentParcelCached()` は drawable のキャッシュを見て、未判定なら `LLViewerParcelMgr::getInstance()->inAgentParcel(getPositionGlobal())` を呼ぶ

### Step 5: パイプラインフラグの追加

- `LLPipeline::sRenderHideOutsideParcel` (static bool) を追加
- `llviewercontrol.cpp` の handler で更新
- フレーム番号も `LLPipeline` に持たせる (`sParcelCheckFrameSeq`)

### Step 6: 区画変更時のキャッシュ無効化

- `LLViewerParcelMgr` で `mAgentParcel` 切り替えイベントをフック (既存の signal を利用)
- `sParcelCheckFrameSeq++` するだけで全 drawable のキャッシュは無効化される (frame 比較で外れる)

### Step 7: preferences UI の追加 (任意・後回し可)

- どの preferences パネルに置くか相談 (Graphics? Firestorm/Display?)
- XUI を 1 ファイル編集
- Phase A の最後にやる

### Step 8: 動作テスト

- 自区画内移動 / 自区画外移動 / TP / リージョン跨ぎ / 設定 ON-OFF / アバター可視性 / FPS 影響

---

## 6. リスクと懸念

| リスク | 対策 |
|--------|------|
| 区画判定が毎フレーム重い | フレーム単位キャッシュ + 設定 OFF 時早期 return |
| アバター/HUD/添付物が消える | 早期 return を最初に実装、テストで明示確認 |
| 区画境界をまたぐリンクセットの片側だけ消える | ルートプリム位置で判定 (個別プリムでは判定しない) |
| 区画情報が未取得時 (TP 直後など) の挙動 | `LLViewerParcelMgr::inAgentParcel` は安全側 (false) を返す → 一瞬全部消える可能性あり。許容する |
| 自分のオブジェクト (建築途中) が消える | `FSRenderHideOutsideParcelKeepOwn` で除外 |
| パーティクル | Phase A では対象外。気になれば Phase B 以降で別途検討 |

---

## 7. Phase B (将来) 概要メモ

詳細は別ドキュメントで。ここは方針メモのみ。

- 隣接区画の description にタグがあれば、その区画も描画対象に含める
- description 取得には `LLViewerParcelMgr::sendParcelPropertiesRequest` を未カバーの 4m セルに対して投げるスキャン処理が必要
- リージョン入域時に 1 回だけスキャン (区画数 ≒ リクエスト数)
- 結果を `FSParcelDescCache` (region UUID → {LocalID → desc, bitmap}) にキャッシュ
- `LLVOVolume::isVisible()` 内で「対象オブジェクトのセル → LocalID → cache 参照 → desc にタグ?」を確認
- 隣接リージョンも同様にスキャン

---

## 8. 完了条件 (Phase A)

- [ ] 設定 3 項目が settings.xml に存在し、debug settings で見える
- [ ] 設定 ON で区画外オブジェクトが描画されない
- [ ] 設定 ON でアバター・添付物・HUD が消えない
- [ ] 設定 ON で自分の所有オブジェクトが消えない (デフォルト)
- [ ] TP / 区画移動でキャッシュが正しく更新される
- [ ] 設定 OFF で従来通り
- [ ] FPS が設定 OFF と比べて顕著に低下しない (目安: 自区画外で +1 ms 以内のオーバーヘッド)
- [ ] AYA がローカルビルド成功・起動成功
- [ ] AYA が UX 面で OK 出す

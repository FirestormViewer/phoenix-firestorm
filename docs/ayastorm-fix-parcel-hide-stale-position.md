# [Bug Fix] ParcelHide で TP 戻り後にオブジェクトが消える問題

**作成日**: 2026-05-05
**対象**: AYA + Claude Code
**作業ブランチ**: `fix/parcel-hide-stale-position`
**ベース**: `ayastorm-release`
**関連仕様**: `docs/ayastorm-render-hide-outside-parcel.md` (機能本体)
**関連命名**: `docs/ayastorm-r5-naming-refactor.md` (`sParcelHide*` / `sParcelOwnerTag*` 命名)

---

## 1. 症状 (報告内容)

自区画の description に `[AYAstorm:]` (= `[parcelhide:]` の旧 alias、空 body) タグを設定し、隣接区画を非表示にした状態で運用している。

> 他の SIM にテレポートしてから戻ってくると、装着物の一部が表示されなくなる不具合が頻発する。

加えて懸念事項として:

- 自区画に rez してあるオブジェクトの一部も同じ症状を起こしていないか。

## 2. 関連コード

判定の本体は `indra/newview/pipeline.cpp` の `LLPipeline::shouldHideForOutsideParcel()` (l.3219–3266)。

該当部分の要点:

```cpp
// 現状の実装 (抜粋)
if (keep_avatars && (vobj->isAvatar() || vobj->isAttachment()))
{
    return false;
}
if (keep_own && vobj->permYouOwner())
{
    return false;
}

if (drawablep->mLastParcelCheckSeq == sParcelCheckSeq)
{
    return drawablep->mLastParcelCheckHidden;
}

bool hidden = !LLViewerParcelMgr::getInstance()->inAgentParcel(vobj->getPositionGlobal());
drawablep->mLastParcelCheckSeq = sParcelCheckSeq;
drawablep->mLastParcelCheckHidden = hidden;
return hidden;
```

`sParcelCheckSeq` は `refreshOutsideParcelHiding()` (= `addParcelChangedCallback` で発火) 経由でしか bump されない。つまり **per-drawable のキャッシュは「次の区画変更まで」固定** される設計。

## 3. 根本原因

### 3.1 装着物 (attachment) の場合

`LLViewerObject::getPositionGlobal()` (`indra/newview/llviewerobject.cpp:4535`) で attachment は以下を経由する:

```cpp
if (isAttachment())
{
    position_global = gAgent.getPosGlobalFromAgent(getRenderPosition());
}
```

- `getRenderPosition()` は通常 `mDrawable->getPositionAgent()` (drawable のキャッシュ位置) を返す。
- `gAgent.getPosGlobalFromAgent(...)` は **gAgent の現在 region 原点** で agent → global 変換。

TP で別 SIM に行って戻ってきた直後:

1. `addParcelChangedCallback` 経由で `refreshOutsideParcelHiding()` が走り `sParcelCheckSeq` が 1 度だけ bump される。
2. その後フレーム単位で各 spatial group の `rebuildGeom` が実行され、装着物 drawable ごとに `shouldHideForOutsideParcel()` が呼ばれる。
3. **ある drawable のチェックが、その drawable の `mPositionAgent` 更新よりも先に来る瞬間** がある (アニメーション/skinning 更新のタイミング差)。
4. その瞬間の `mPositionAgent` は旧 SIM 時点の値のまま。新 region 原点で global 変換するので、出来上がる global 座標はその区画の境界外になる。
5. `inAgentParcel()` = false → `mLastParcelCheckHidden = true` でキャッシュ確定。
6. 以後、再度 agent parcel が変わるまでキャッシュ固定 → **その装着物だけ消えたまま**。

タイミング依存なので「全部消える」ではなく「一部消える」という症状になる。

### 3.2 自区画 rez 物の場合

`getPositionGlobal()` の else 分岐 (l.4549-4552):

```cpp
else  // mRegionp が null or LLWorld の region list から外れている
{
    LLVector3d position_global(getPosition());  // 局所座標を global として返す
    return position_global;
}
```

TP / region 遷移中に `mRegionp` が一瞬無効化される瞬間に `rebuildGeom` がそのオブジェクトを処理すると、`getPositionGlobal()` は局所座標 (例: `(1.5, 2.5, 30)` のような小さい値) を global 座標として返す。当然どの parcel にも入らないので `inAgentParcel()` = false → `mLastParcelCheckHidden=true` でキャッシュ確定。

装着物ほどの頻度ではないが、同じ caching 機構の弱点を共有している。

### 3.3 共通の本質

**「一過性の不安定状態でつかんだ判定結果を per-drawable cache に焼き付け、その後 agent parcel が変わるまで再評価しない」** という設計が問題。

## 4. 修正方針 (最小)

`shouldHideForOutsideParcel()` の判定ロジックの先頭に 2 段ガードを追加する。設定追加なし、後方互換あり。

### 4.1 ガード A: 自分の装着物は常に表示

自分の装着物は wearer = gAgent であり、gAgent は常に agent parcel 内にいる (= タグ判定の基準と一致)。よって position-check する意味がない。早期 return で除外すればキャッシュにも入らず stale も無関係になる。

```cpp
if (vobj->isAttachment())
{
    LLVOAvatar* wearer = vobj->getAvatar();
    if (wearer && wearer->isSelf())
    {
        return false;
    }
}
```

これは **§3.1 の症状の直接修正**。

### 4.2 ガード B: region 不確定中は判定保留 (キャッシュ書込まない)

`mRegionp` が null または `LLWorld` の region list から外れている間は判定を保留して `false` (= 表示) を返す。**キャッシュ書込はしない** ので次フレーム以降に region が確定したら正しく再評価される。

```cpp
LLViewerRegion* objRegion = vobj->getRegion();
if (!objRegion || !LLWorld::instance().isRegionListed(objRegion))
{
    return false; // 保留: cache 書込なし
}
```

これは **§3.2 の懸念をカバー** する。装着物以外の rez 物にも保険が効く。

### 4.3 適用位置

`indra/newview/pipeline.cpp:3247` の `keep_avatars` 判定の **直前** に `4.1` を、`l.3257` のキャッシュ参照の **直前** に `4.2` を挿入する。既存の `keep_avatars` / `keep_own` / cache 参照ロジックは触らない。

### 4.4 修正後の関数フロー

```
shouldHideForOutsideParcel(drawable)
 ├ drawable null / spatial bridge / vobj null / HUD → false (既存)
 ├ [NEW] 自分の装着物 (wearer.isSelf()) → false
 ├ keep_avatars && (avatar | attachment) → false (既存)
 ├ keep_own && permYouOwner → false (既存)
 ├ [NEW] region 無効/未登録 → false (cache 書込なし)
 ├ cache hit → cached value (既存)
 └ inAgentParcel(getPositionGlobal()) で判定 + cache 書込 (既存)
```

## 5. 実装範囲

| ファイル | 変更内容 | 規模 |
|---|---|---|
| `indra/newview/pipeline.cpp` | `shouldHideForOutsideParcel()` に 2 段ガード追加 | +約 15 行 |

ヘッダ変更なし、設定追加なし、リスナー追加なし、命名変更なし。

include の追加が必要になる可能性:
- `LLVOAvatar::isSelf()` 用に `llvoavatar.h` (pipeline.cpp は既に include 済の可能性大、要確認)
- `LLWorld::isRegionListed()` 用に `llworld.h` (同上)

## 6. 検証手順

ビルドは AYA が実行 (memory: ビルド手順)。検証は 1 ステップずつ:

1. **修正前の再現確認**: `[AYAstorm:]` タグありで自区画に立ち、装着物多めの状態で他 SIM へ TP → 戻る、を数回繰り返して一部装着物が消える症状が出ることを確認 (現状の挙動)。
2. **修正後 — 装着物**: 同じ手順で TP 往復を 5 回行い、装着物が一切欠けないことを確認。
3. **修正後 — 自区画 rez 物**: 自区画に複数の rez 物 (異なる位置) を置いた状態で TP 往復、rez 物が欠けないことを確認。
4. **回帰確認 — 隣区画非表示**: 隣の区画に rez された物体が、修正後も期待通り非表示になることを確認 (機能本体が壊れていない)。
5. **回帰確認 — 他人の装着物**: `[AYAstorm:]` (= `keepavatars:false`) の状態で他人が隣区画に立っている時、その装着物が引き続き非表示になることを確認 (`§4.1` のガードは `wearer.isSelf()` 条件付きなので影響しない設計)。
6. **回帰確認 — keep_avatars / keep_own ON 経路**: `[parcelhide:{keepavatars:true}]` および visitor 設定 `ParcelHideKeepAvatars=true` で従来通り全 avatar/装着物が表示されることを確認。

## 7. リスクと留意点

- **ガード A の例外性**: 「自分の装着物は常に表示」は `keep_avatars=false` の意図 (= 自分も含めて区画内のみ表示) を一見破るように見えるが、自分自身が agent parcel 内にいる前提では結果同値。区画判定そのものが「自分が立っている parcel」基準なので、自分の装着物が「区画外」になるケースは存在しない。
- **ガード B の保留設計**: `false` を返してキャッシュしないので、「本当に区画外にある (= 非表示が正解な) オブジェクト」が region 無効状態で 1 フレームだけ表示される可能性がある。が、次フレームで region が確定し次第正しく非表示判定される。視覚上はちらつき程度で実害なし。
- **他人の装着物の同等問題**: `§3.1` と同じ stale-position 問題が他人の装着物にも理論上ありうるが、現状報告されていないため本 fix では触らない。必要になったら別途「wearer 位置で判定」案を検討する (本ドキュメント §3.1 で検討済の代替案)。
- **キャッシュ設計の根本見直し**: 本 fix は最小修正であり、「per-drawable cache が agent parcel 変更時しか無効化されない」という設計弱点そのものは残る。動的な物体 (vehicle 等) で類似症状が出たら、cache 無効化トリガの拡張 (object 移動 / region 変更時にも seq bump) を検討する。これは将来課題。

## 8. 参考: 検討した代替案

### 案 X: 全装着物について wearer avatar の位置で判定

`§3.1` の代替案として「attachment は wearer avatar の `getPositionGlobal()` で判定」も検討した。

- メリット: 自分 + 他人の装着物両方の stale-position 問題を一括解消。仕様書 (`ayastorm-render-hide-outside-parcel.md` §3.3) の「アバター添付物 → アバター本体の判定に従う」と意味的に整合する。
- デメリット: 修正範囲がやや広く、wearer 未解決時の取り扱いを別途決める必要がある。
- 判断: 報告された症状は自分の装着物に限定されており、他人の装着物の症状は未報告。最小修正で確実に直すため、本 fix では採用せず将来オプションとして残す。

## 9. 改訂履歴

- **v1 (2026-05-05)**: 初版。`fix/parcel-hide-stale-position` ブランチで作業開始。

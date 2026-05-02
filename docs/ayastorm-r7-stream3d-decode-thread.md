# AYAstorm r7: Stream3D decode-thread 化 実装工程

**作成日**: 2026-05-02
**対象**: AYAstorm `feature/aya-r7-stream3d-decode-thread` (想定ブランチ名)
**仕様**: `doc/spec_stream3d_decode_thread.md`
**前リリース**: r6 (`v7.2.4-ayastorm-r6`)

---

## 1. ゴール

`LLPositionalStreamStereo` の HTTP 取り込みと PCM 変換を専用 decode thread に移し、配信受信中の main thread stall を解消する。10 fps 環境でも音切れせずに 3D Stream を再生できる状態を r7 リリース時点で達成する。

機能仕様 (タグ書式・設定キー・UI) は r6 と完全互換。

---

## 2. フェーズ分解

各フェーズは 1〜2 commit 想定。M3 完了時点で「機能としては動く」状態に到達し、M4/M5 は計測と実機ベンチで仕様 §5 受入条件を埋める段階。

### M1: decode thread shell

**目的**: thread の起動・停止だけを先に整備し、空 loop を回しても既存挙動が壊れないことを確認する。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.h` (private member 追加)
- `indra/llaudio/llpositionalstreamstereo.cpp` (start/stop でのライフサイクル制御)

**作業**:
- `std::thread mDecodeThread`, `std::atomic<bool> mDecodeStop`, `std::condition_variable mDecodeCv`, `std::mutex mDecodeMutex` を追加
- `State::Playing` 到達時に `startDecodeThread()` を呼ぶフックを置く (中身は空 loop で sleep するだけ)
- `stop()` / デストラクタで `mDecodeStop = true` → `notify_all` → `join` の順を守る
- thread 関数内で `apr_thread_set_name` 相当 (利用可能なら `pthread_setname_np`) で `Stream3D-decode-N` を設定
- pumpSource はまだ main thread 側で従来通り呼ぶ (M2 で剥がす)

**完了条件**:
- 起動 → 配信再生 → `Stream3DEnabled = false` → true → 配信再開 を 5 回繰り返し、log に thread 起動/停止が対で出る
- 配信再生中の挙動が r6 と同等 (まだ cark は残る — それは M2 で消える)
- `Stream3DEnabled = false` で thread が確実に終了する (TSan 走らせて leak 0)

**想定 commit**: 1 本 (`r7: Stream3D — introduce decode thread skeleton (M1)`)

---

### M2: pumpSource を decode thread に移動

**目的**: main thread の負荷を実際に剥がす。仕様 §4.4 の「平常時」モデルを実現する。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.cpp` (`pumpSource()` の呼び出し元変更、loop 本体実装)
- `indra/newview/llpositionalstreammgr.cpp` (main 側 `update()` から pumpSource を呼ばなくなることへの追従、ただし mgr 側は `stream->update()` を呼ぶだけなので変更最小)

**作業**:
- decode thread 関数の中身を実装:
  ```
  while (!mDecodeStop.load(memory_order_acquire)) {
      const size_t got = pumpOnce();
      std::unique_lock lk(mDecodeMutex);
      if (got == 0) {
          mDecodeCv.wait_for(lk, std::chrono::milliseconds(kDecodeIdleSleepMs));
      } else if (mRingL.writeAvailable() < kDecodeMaxFramesPerPump) {
          mDecodeCv.wait_for(lk, std::chrono::milliseconds(5));
      }
  }
  ```
- `LLPositionalStreamStereo::update()` (main thread) から `pumpSource()` 呼び出しを削除
- 既存 `pumpSource()` を `pumpOnce()` にリネームし、実装はそのまま流用 (decode thread 内で同じ readData / 変換 / ring write を行う)
- `mState` を `std::atomic<State>` 化。Failed への遷移は decode thread が atomic write、main thread は atomic read
- `mStarvingSince` (現状 `F64`) は decode thread 単独で更新するなら atomic 不要、main thread から読むなら `std::atomic<F64>` 化

**完了条件**:
- 配信受信中、main thread の Frame Stats から見て fps が安定 (60 fps 上限解除環境で配信 ON/OFF の差が ±0.5 ms 以内)
- カメラ操作中の cark 体感が消える (主観 + Frame Stats max frame time 計測)
- `Stream3DEnabled` トグル × 20 回で crash しない
- TSan / Helgrind で data race 検出なし

**想定 commit**: 1〜2 本 (`r7: Stream3D — move pumpSource into decode thread (M2)` + 必要なら `r7: Stream3D — atomic state transition (M2)`)

---

### M3: shutdown 順序の安全化

**目的**: 仕様 §4.4 の不変条件 (decode thread join → FMOD release の順) を強制する。reconnect / region cross / Quit 時のいずれでも safe であることを担保する。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.cpp` (`stop()` / `releaseAll()` の順序見直し)
- `indra/newview/llpositionalstreammgr.cpp` (manager 側 reconnect 経路で stop → start を踏む箇所、既存と整合確認)

**作業**:
- `stop()` 内で順序を明示:
  ```
  void stop() {
      requestDecodeStop();      // mDecodeStop = true; mDecodeCv.notify_all();
      joinDecodeThread();       // mDecodeThread.join();
      releaseAll();             // ここから FMOD release
      mUrl.clear();
      mState = State::Idle;
  }
  ```
- デストラクタが `gAudiop` 健在時に `stop()` を呼ぶ既存ロジックを維持しつつ、`gAudiop == nullptr` でも thread join だけは実行するパスを足す (Quit シーケンスでの leak 回避)
- 「decode thread が走っている間に main thread から `releaseAll()` が呼ばれない」ことを assert で守る (debug ビルドのみ)

**完了条件**:
- `Stream3DEnabled` トグル × 50 回 / 配信中 region 切替 × 10 回 / Viewer Quit 中の配信再生中状態、いずれも crash / hang なし
- valgrind helgrind で lock 順序違反検出なし

**想定 commit**: 1 本 (`r7: Stream3D — enforce decode thread join before FMOD release (M3)`)

---

### M4: fast_timer 計測コード追加

**目的**: r6 / r7 の差分を客観計測できる状態にする。今後の調整 (mono 化検討、thread pool 化判断) の判断材料を残す。

**ファイル**:
- `indra/llaudio/llpositionalstreamstereo.cpp` (`LL_RECORD_BLOCK_TIME` 挿入)
- `indra/newview/llpositionalstreammgr.cpp` (`update()` 全体を 1 セクションで囲む)

**作業**:
- `static LLTrace::BlockTimerStatHandle FTM_STREAM3D_MGR_UPDATE("Stream3D Mgr Update");` を追加し manager update を囲む
- `static LLTrace::BlockTimerStatHandle FTM_STREAM3D_PUMP("Stream3D Pump (decode)");` を decode thread 側 `pumpOnce()` に挿入 (decode thread 計測が fast_timer に乗るかは実装確認、乗らないようなら decode thread 内は `LLTimer` 直接計測でログに dump)
- main thread 側 `update()` には `FTM_STREAM3D_MGR_UPDATE` と `FTM_STREAM3D_STREAM_UPDATE` を入れて、ここに `pumpSource` 由来の時間が消えていることを可視化

**完了条件**:
- Develop > Show Debug > Fast Timers から `Stream3D` セクションが見え、main thread 側の値が r7 で大幅減 (主観 0 ms ヒストグラム多数)
- decode thread 側の計測がログまたは別表示に出ている

**想定 commit**: 1 本 (`r7: Stream3D — fast_timer instrumentation for decode path (M4)`)

---

### M5: 実機ベンチと仕様クロージング

**目的**: 仕様 §5 受入条件を実測値で埋める。ベンチ結果を本ドキュメントに追記。

**作業**:
- 計測シナリオ (10 fps 会場想定 / DJ シナリオ / 通常リージョン) で配信 ON/OFF の Frame Stats を採取
- `Stream3DEnabled` トグル × 50 回 / region 切替 × 10 回 / Quit テスト
- 結果を §5 受入条件表に記入し、未達があれば該当フェーズに戻る
- spec のリスク表 (R1〜R7) を実測ベースで「解決 / 監視継続」に分類
- 動作確認チェックリスト (本書 §5) を埋める

**完了条件**:
- 仕様 §5.1 の 4 指標が全て目標達成
- 仕様 §5.2 の機能互換 11 項目が全て通過
- 残課題があれば本ドキュメント §6 に記載

**想定 commit**: 1 本 (`r7: Stream3D — close M5 acceptance with bench results`)

---

## 3. マイルストーン依存関係

```
M1 → M2 → M3 → M4 → M5
```

完全な直列。M2 で初めて main thread から負荷が剥がれるため、M4 計測 (差分の客観確認) は M2 以降でないと意味を持たない。M3 (shutdown 安全化) は M2 完了後 / M5 ベンチ前のタイミングで実施。

---

## 4. リスクトラッキング (spec §8 と対応)

| ID | 状態 | 着手フェーズ | メモ |
|---|---|---|---|
| R1. shutdown 順序 | 未着手 | M3 で対処 | デストラクタ経路 / Quit 経路 / reconnect 経路の 3 系統を網羅 |
| R2. CPU 浪費 | 未着手 | M2 設計時点 で考慮 | condvar + 10 ms timeout を初期値とし、M5 で実測 |
| R3. ring SPSC 前提崩壊 | 未着手 | M2 で対処 | main 側 `update()` から pumpSource を物理削除し write 経路一本化 |
| R4. 同時走行 thread 数 | 監視のみ | M5 で実測 | `Stream3DMaxConcurrent = 4` の上限内なら問題なし |
| R5. 状態遷移取りこぼし | 未着手 | M2 で対処 | `mState` を atomic 化 |
| R6. log 連発による新 lock 源 | 未着手 | M2 / M5 で監視 | EOF / write full を debounce、起動 1 回・失敗 1 回・回復 1 回が目安 |
| R7. reconnect 時の thread 残存 | 自動解決見込み | M3 で確認 | manager は `b.stream->start()` を呼ぶ前に既存 binding を破棄するため、stop() に join が入っていれば自動的に安全 |

---

## 5. 動作確認チェックリスト (M5 で埋める)

- [ ] mono タグ付プリムを rez → 自動接続して 3D 定位 (r6 と同等)
- [ ] LSL `llSetObjectDesc` でステレオタグを後付け → 自動 binding (r6 と同等)
- [ ] ステレオタグ付リンクセットで L/R が左右に分離 (r6 と同等)
- [ ] `Stream3DVolumeMaster` を 0 → 1 で即時反映
- [ ] 配信サーバーを止める → 5s × 3 回再接続を試み、失敗で binding drop + 通知
- [ ] `Stream3DEnabled = false` で全ストリーム即時停止
- [ ] `Stream3DDescriptionScan = false` で prim binding 落ち、debug は維持
- [ ] `Stream3DMaxConcurrent = 1` で 2 個目を拒否 + 通知、解放後に再 bind
- [ ] 通知が FS Nearby Chat / LL Nearby Chat 両方に出る
- [ ] Preferences > Sound & Media と speaker icon pulldown の「3D Stream」が live 反映
- [ ] Ear-location ラジオの即時切替
- [ ] **r7 新規**: 配信受信中、カメラを 360° 回転 → cark なし
- [ ] **r7 新規**: 静止中、長文をチャット入力 → 入力ラグなし
- [ ] **r7 新規**: 混雑会場 (実 fps 10 程度) で配信 5 分聴取 → 音切れなし
- [ ] **r7 新規**: `Stream3DEnabled` トグル × 50 回で crash / hang なし
- [ ] **r7 新規**: 配信再生中の region 切替 × 10 回で crash / hang なし

---

## 6. 残課題・スコープ外

- **mono 経路 (`LLPositionalStream`) の thread 化**: 現状 cark 報告なしのため r7 では触らない。報告が出た時点で同設計を適用
- **decode thread pool 化**: 4 stream を超える同時走行需要が出た時点で着手
- **decode thread 優先度の調整**: 実測で必要性が出た時点で検討
- **`Stream3DPollInterval` の既定値見直し**: 当初検討した「30s → 300s」は本 r7 のスコープ外。decode thread 化で main thread 負荷が解消すれば scan walk のコストは無視できるはず

---

## 7. commit 一覧 (作業中に追記)

| commit | 内容 |
|---|---|
| _(M1 着手後追記)_ | r7: Stream3D — introduce decode thread skeleton (M1) |
| _(M2 着手後追記)_ | r7: Stream3D — move pumpSource into decode thread (M2) |
| _(M3 着手後追記)_ | r7: Stream3D — enforce decode thread join before FMOD release (M3) |
| _(M4 着手後追記)_ | r7: Stream3D — fast_timer instrumentation for decode path (M4) |
| _(M5 着手後追記)_ | r7: Stream3D — close M5 acceptance with bench results |

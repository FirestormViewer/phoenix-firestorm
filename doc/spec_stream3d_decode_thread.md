# Stream3D decode-thread 化 仕様書 (r7)

> **対象**: AYAstorm `v7.2.5-ayastorm-r7` (想定)
> **前提**: `doc/spec_positional_stream_audio.md` (3D Stream 本体仕様、r5 改訂後)
> **背景文書**: `docs/ayastorm-positional-stream.md` (M1〜Post-M9 実装記録)

## 1. 目的とスコープ

r7 は新機能追加ではなく、`LLPositionalStreamStereo` の HTTP ストリーム取り込み経路をビューア main thread から専用 decode thread に移動するアーキテクチャ改修リリース。

主目的:
- 配信受信中の main thread stall を解消し、3D Stream 利用時のフレーム drop / 入力ラグを根絶する
- 低 fps (10 fps 以下) の混雑会場でも音切れせず再生継続できるようにする

非対象 (r7 では触らない):
- mono 経路 (`LLPositionalStream`) のスレッド化 — 現状 cark 報告なし、stereo 経路と異なり自前 PCM pump をしていないため後回し
- 機能仕様 (タグ書式、設定キー名、UI 文言) の変更 — 完全互換維持
- 複数 stream 同時走行時の thread 数最適化 — `Stream3DMaxConcurrent = 4` 既定で N=4 まで thread 1 本/インスタンスで運用

---

## 2. 問題定義

### 2.1 症状

3D Stream (stereo タグ) で配信受信中、以下が観測される:
- 不定期かつ高頻度に画面が cark する
- カメラを動かすと特に顕著 (描画コスト + stall が重なる)
- 静止中でもチャット入力にラグが出る (key event 処理が main thread)
- `Stream3DDescriptionScan = OFF` で部分緩和、配信 URL 削除で完全消失

### 2.2 計測根拠

`~/.ayastorm_x64/logs/AYAstorm.log` の `Stream3D` channel を観察:
- `Stream starving` 0 件 — buffer 不足ではない
- `Stream dropped mid-playback` / `openstate error` 0 件 — network も健全
- `onObjectPropertiesReceived` は数十秒間隔 — scan 走査負荷も否定
- `Stereo source ready: ... 48000 Hz x 2 ch, fmt=PCM16, ring cap 32768 frames` — 自前 ring buffer + PCM pump 経路で動作

### 2.3 根本原因

`indra/llaudio/llpositionalstreamstereo.cpp:412` の `pumpSource()` が **main thread から毎フレーム呼ばれている** (`llpositionalstreammgr.cpp:518` の `update()` 経由 → 同 588 行 `pumpSource()`)。

`pumpSource()` 内の処理:
1. `mSourceSound->readData(...)` — HTTP buffer から PCM 読み出し。FMOD 内部で lock を取り、packet 受信 thread と競合した瞬間 main thread が短時間 stall
2. S16 → F32 変換 / deinterleave / ring write — CPU 処理だが実測で μs オーダー、本質的負荷ではない

つまり stall の主因は `readData()` の lock 競合。1 回あたり最大 `kMaxFramesPerPump = 8192` frames (= 32 KB) を読みに行く設計のため、競合発生時の stall が長時間化しやすい。

### 2.4 通常 stream との設計差

通常 internet stream (`LLStreamingAudio_FMODSTUDIO`) は FMOD `playSound()` に直流し込み、PCM 取り出しは FMOD 内部 thread が行う。main thread に負荷は来ない。

`LLPositionalStreamStereo` だけが自前 pump を持つ理由は L/R を別 channel として独立した 3D 位置に配置するため (`FMOD_OPENUSER` + `pcmreadcallback` モデル)。この構造自体は維持しつつ、pump 主体だけを decode thread に移すのが r7 の設計意図。

---

## 3. ゴール / 非ゴール

### ゴール

- G1. 配信受信中の main thread への 3D Stream 由来 stall を実質ゼロ (1 ms 未満) にする
- G2. 10 fps 環境でも 3D Stream 再生が音切れしない
- G3. shutdown 時に thread と FMOD リソースを安全に開放する (use-after-free / dangling callback なし)
- G4. 現行の mono 経路、`Stream3DEnabled` / `Stream3DDescriptionScan` / `Stream3DVolumeMaster` 等の挙動とユーザー観点の互換性を保つ

### 非ゴール

- NG1. mono 経路 (`LLPositionalStream`) の thread 化 — r7 では触らない
- NG2. 複数 stream 並走時の thread プール化 — 当面はインスタンス毎に 1 thread
- NG3. ring buffer 容量や prebuffer 量のチューニング — 既存値 (32768 frames cap, kPrebufferFrames) を踏襲し、変更は r8 以降の課題とする
- NG4. 設定 UI への新 knob 追加 — 既存設定だけで完結

---

## 4. 設計

### 4.1 スレッドモデル

```
┌──────────────────────────────────────────────────────────────────┐
│ main thread (ビューア)                                            │
│   LLPositionalStreamMgr::update() (毎フレーム)                    │
│     └─ stream->update()  ← 軽量化 (state 確認 / 位置反映のみ)     │
│         pumpSource() を呼ばない                                   │
└──────────────────────────────────────────────────────────────────┘
                          │ (start/stop signal、位置の atomic 反映)
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│ decode thread (新設、インスタンス毎に 1 本)                        │
│   loop:                                                          │
│     1. mSourceSound->readData(...)   ← lock 競合は thread 内に閉じる │
│     2. S16/F32 変換 / deinterleave                                 │
│     3. mRingL / mRingR への write (lock-free SPSC)                │
│     4. 短時間 sleep (~5 ms) または condvar wait で yield          │
│   shutdown signal で loop 終了 → join                            │
└──────────────────────────────────────────────────────────────────┘
                          │ (ring 経由)
                          ▼
┌──────────────────────────────────────────────────────────────────┐
│ FMOD mixer thread (既存、FMOD 管理)                                │
│   pcmReadCallbackL / pcmReadCallbackR                            │
│     └─ mRingL / mRingR から read (既存ロジック維持)               │
└──────────────────────────────────────────────────────────────────┘
```

### 4.2 責務境界

| Thread | 担当 |
|---|---|
| main | start/stop 指示、3D 位置 (`set3DAttributes`) 反映、kill switch / volume 反映、状態取得 (isPlaying / isFailed) |
| decode | HTTP 取り込み (`readData`)、PCM 変換、ring 書き込み、starving/drop 検出 → 失敗 state へ遷移 |
| FMOD mixer | ring から read (既存 `pcmReadCallback*`) |

### 4.3 スレッド間データ受け渡し

| 方向 | データ | 同期手段 |
|---|---|---|
| main → decode | shutdown 要求 | `std::atomic<bool> mDecodeStop` |
| main → decode | 位置 (`set3DAttributes` で使う) | `std::atomic` 不要 (FMOD API 経由で main から直接 channel 操作) |
| decode → main | 失敗 state (`State::Failed`) | `std::atomic<State>` (`mState` の atomic 化) |
| decode → FMOD mixer | PCM サンプル | 既存 `LLFloatRing` (lock-free SPSC、変更不要) |
| decode → main (任意) | starving 通知タイムスタンプ | `std::atomic<F64> mStarvingSince` |

### 4.4 ライフサイクル

#### start 時

```
LLPositionalStreamStereo::start(url, l_pos, r_pos)
  ├─ stop()                                  # 既存 thread あれば join
  ├─ createStream(NONBLOCKING)               # 既存
  ├─ mState = State::Opening
  └─ ※ decode thread はまだ起動しない
       (Opening → Buffering 遷移は依然 main thread の update() で判定する)

main thread update() で State::Opening → State::Buffering 確定後、
さらに createUserSounds() / startUserChannels() 完了で State::Playing
に到達した時点で decode thread を起動する。
```

> **理由**: open 中はソース sound のフォーマットが未確定 (`getFormat()` の戻りを待つ)。decode loop は format 確定後でないと variant の deinterleave 経路を選べないため、main thread が format を決め切ってから decode thread に引き渡す方が安全。

#### 平常時

```
decode thread loop:
  while (!mDecodeStop.load(memory_order_acquire)):
    if (pumpOnce() == 0):              # readData 0 byte / NOTREADY
      condvar_wait(timeout = 10 ms)    # CPU 浪費を防ぐ
    else:
      condvar_wait(timeout = 0 ms)     # ring に余裕あれば即継続
                                       # full なら 5 ms 待機
```

main thread の `update()` は以下のみ:
- 失敗 state の検知 (`mState.load() == Failed`) → manager 側で reconnect
- 位置反映 (`setPositions()` → 各 channel の `set3DAttributes()`)
- volume 反映 (`setVolume()` 経由)

#### stop / shutdown 時

```
LLPositionalStreamStereo::stop()
  ├─ mDecodeStop.store(true, memory_order_release)
  ├─ mDecodeCv.notify_all()                  # sleep 中の thread を起こす
  ├─ mDecodeThread.join()                    # 必ず完了を待つ
  └─ releaseAll()                            # ここから FMOD release を実行
       (channel->stop / sound->release は thread join 後でないと
        callback 中の sound 参照と競合する可能性がある)
```

シャットダウン順序の不変条件:
1. **decode thread join 完了前に FMOD `Sound::release()` を呼ばない**
2. **`mUserSoundL` / `mUserSoundR` (`pcmreadcallback` の供給元) の release より先に `mChannelL` / `mChannelR` を `stop()` する** (既存と同じ)
3. デストラクタは `stop()` 経由で必ず join 待ちする

### 4.5 エラー処理

decode thread 内でエラーが発生した場合:

| 検出条件 | 遷移 | 通知 |
|---|---|---|
| `readData` が `FMOD_ERR_FILE_EOF` 連続 N 回 (例 30 回 = ~1.5 秒) | `mState = Failed` | manager の既存再接続ロジックで拾う |
| `readData` が `NOTREADY` 以外の error 返却 | `mState = Failed` | 同上 |
| ring `write` が連続 M 回フル (例 100 回) | logging のみ (再生継続)。FMOD mixer 側が読み出していれば自然解消 | LL_INFOS 1 回 (debounce) |

decode thread 内では LL ログマクロ呼び出しは最小化 (1 frame に複数回出さない)。LL のログ系統は内部で mutex を持つため、log 連発自体が新しい lock 競合源になり得る。

### 4.6 thread 命名と優先度

- thread 名は `apr_thread_set_name` 相当を使い `Stream3D-decode-<short_id>` を設定 (デバッグログでの追跡用)
- 優先度はデフォルト維持 (audio mixer thread と同等。優先度を上げる必要は実測で判断、初期値変更しない)

---

## 5. 受入条件

### 5.1 定量基準

| 指標 | 計測方法 | 目標 |
|---|---|---|
| 配信受信中の main thread stall | fast_timer (`Stream3D pumpSource` セクション、r7 で main thread 側からは消える想定) | r7 後は計測上 0 ms (コードパスに存在しない) |
| 60 fps 描画時の Frame time 上振れ | 通常配信受信中に Frame Stats で平均/最大計測 | r6 比で最大 frame time が短縮し、配信 ON/OFF で frame time の差が ±0.5 ms 以内 |
| 10 fps 環境での音切れ | 混雑会場想定で fps を意図的に下げて 5 分聴取 | underrun 由来のプチノイズ / 無音欠落なし |
| shutdown 安定性 | `Stream3DEnabled` トグル × 50 回、配信中のリージョン切替 × 10 回 | crash / hang / use-after-free なし |

### 5.2 機能互換

- 既存の動作確認項目 (`docs/ayastorm-positional-stream.md` §6 全 11 項目) が引き続き全て通る
- `Stream3DEnabled` / `Stream3DDescriptionScan` / `Stream3DVolumeMaster` / `Stream3DReconnectAttempts` の挙動が r6 と同じ
- log channel `Stream3D` の出力フォーマット (人間可読フィールド名) を維持

---

## 6. 設定項目

r7 で新規追加 / 既定値変更する設定はなし。

将来の調整余地として以下を内部定数で持っておく (settings.xml には出さない):

| 内部定数 | 既定 | 用途 |
|---|---|---|
| `kDecodeIdleSleepMs` | 10 | ring full 時 / readData NOTREADY 時の wait timeout |
| `kDecodeMaxConsecutiveEof` | 30 | EOF 連続でこの回数まで許容後に Failed 遷移 |
| `kDecodeMaxFramesPerPump` | 8192 | 1 回の readData で読む最大 frame 数 (既存値継承) |

調整が必要になった時点で settings.xml に昇格を検討する (r8 以降)。

---

## 7. 互換性

### 7.1 ABI / 公開ヘッダ

`indra/llaudio/llpositionalstreamstereo.h` に以下が追加される (private):
- `std::thread mDecodeThread`
- `std::atomic<bool> mDecodeStop`
- `std::condition_variable mDecodeCv`
- `std::mutex mDecodeMutex`
- `std::atomic<State> mState` (既存 `State mState` を atomic 化)

公開メソッドのシグネチャは変わらない。`LLPositionalStream` (mono) には変更なし。

### 7.2 設定 / タグ / ログ

- 設定キー: 追加・rename・default 変更なし
- Description タグ書式: 変更なし (`[3dstream-stereo:...]` / 旧 `[ayastream-stereo:...]` alias 双方変更なし)
- ログ channel: `Stream3D` 維持。decode thread からの出力にも同 channel を使う

### 7.3 FMOD API バージョン

FMOD Studio 2.02 系で動作確認 (現行ビルド使用バージョン)。`std::thread` / `std::atomic` / `std::condition_variable` の C++17 機能のみ使用、特殊な FMOD API 追加要件なし。

---

## 8. リスク

| ID | 内容 | 対策 |
|---|---|---|
| R1 | shutdown 順序ミスで FMOD callback 中に sound release されて crash | §4.4 不変条件を守る。stop() に必ず thread join を入れ、デストラクタも stop() を経由 |
| R2 | decode thread が CPU を浪費 (busy loop) | `condvar` + 10 ms timeout で yield。 ring full 検出時は明示的 sleep |
| R3 | ring SPSC 前提崩壊 (decode thread と main thread の両方が write しようとする) | write は decode thread のみが触る不変条件をコードコメントで明示。`pumpSource()` を main thread の update() から完全に削除する |
| R4 | 複数 stream 同時走行時 thread 数増加 (4 stream で 4 thread) | `Stream3DMaxConcurrent = 4` 既定で worst case 4 thread。Linux の thread 上限から見て無視できる規模。r8 で必要なら pool 化検討 |
| R5 | 状態遷移 (Opening → Buffering → Playing → Failed) の取りこぼし | state は `std::atomic<State>` で main / decode 双方から read 可能にし、書き込みは decode thread の Failed 遷移と main thread の start/stop に限定 |
| R6 | LL のログマクロが decode thread から呼ばれた時の thread safety | LL_INFOS / LL_WARNS は内部 mutex 持ちで thread-safe。ただし頻発させると新しい lock 源になるため §4.5 の通り debounce |
| R7 | reconnect 時に古い decode thread が残る | reconnect 経路 (`llpositionalstreammgr.cpp` 既存) は `b.stream->start()` 呼び出し前に内部で stop() → join() するため自動で解決 |

---

## 9. 将来拡張 (本仕様外)

- **mono 経路の同等改修**: 現状 `LLPositionalStream::update()` は readData ではなく `playSound` 直接で FMOD に任せているため main thread 負荷は軽い。問題報告が出た時点で同設計を適用
- **decode thread の thread pool 化**: 4 stream 超の同時走行需要が出てきた時点で worker pool に切り替える
- **decode thread 優先度の調整**: 実測で必要性が確認された場合に検討
- **ICY metadata 受信を decode thread 側で処理**: 仕様書 §12 (将来拡張) に既存の項目あり

---

## 10. 参考コード位置

| 対象 | パス |
|---|---|
| 現行 stereo 実装 | `indra/llaudio/llpositionalstreamstereo.{h,cpp}` |
| `pumpSource()` 本体 | `indra/llaudio/llpositionalstreamstereo.cpp:412` |
| ring (lock-free SPSC) | `indra/llaudio/llpositionalstreamstereo.cpp:63-133` (`LLFloatRing`) |
| FMOD pcmreadcallback | `indra/llaudio/llpositionalstreamstereo.cpp:298-328` |
| manager 側 update / 呼び出し元 | `indra/newview/llpositionalstreammgr.cpp:518` (`LLPositionalStreamMgr::update`) |
| 通常 stream (比較対象) | `indra/llaudio/llstreamingaudio_fmodstudio.cpp` |
| FMOD createStream buffer 設定例 | `indra/llaudio/llstreamingaudio_fmodstudio.cpp:78-82` |

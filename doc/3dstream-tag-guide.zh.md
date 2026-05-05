# 3D Stream 标签格式指南

> AYAstorm 的 **3D Stream** 功能用于把 HTTP 音频流以 3D 空间定位的方式从图元 (prim) 播放出来。本文档是其标签格式参考手册。
>
> 本文档基于 AYAstorm `r10` 时点的最终规格。今后 r11 / r12 等版本新增功能时，本文档将随之更新。

---

## 目录

1. [什么是 3D Stream](#1-什么是-3d-stream)
2. [快速上手](#2-快速上手)
3. [术语](#3-术语)
4. [标签总览](#4-标签总览)
5. [单声道标签 `[3dstream:...]`](#5-单声道标签-3dstream)
6. [分散立体声 / 会场布置标签 `[3dstream-stereo:...]`](#6-分散立体声--会场布置标签-3dstream-stereo)
7. [`ch` (声道) 取值参考](#7-ch-声道-取值参考)
8. [源声道数 × 标签值 兼容矩阵](#8-源声道数--标签值-兼容矩阵)
9. [推流端 (制作源 URL)](#9-推流端-制作源-url)
10. [Viewer 端设置](#10-viewer-端设置)
11. [错误通知 / 诊断](#11-错误通知--诊断)
12. [故障排查](#12-故障排查)
13. [已知限制 / 规格说明](#13-已知限制--规格说明)
14. [相关文档 / 内部规格书](#14-相关文档--内部规格书)

---

## 1. 什么是 3D Stream

Second Life 标准 Viewer 把 HTTP 音频流 (SHOUTcast / Icecast) 当作 **地块级 BGM** 进行 2D 播放，没有该声音在空间中来自何处的信息。

AYAstorm 的 **3D Stream** 功能把图元 (对象) 当作"扬声器"，让流媒体音源 **如同从该图元位置发声一样**，以 3D 定位的方式播放。当听者 (摄像机或角色) 移动时，声音的方向感和距离感会实时跟随。

主要用途：

- **现场演出 PA**: 在舞台前布置扬声器图元，让推流的音源从这些位置播放
- **环境音**: 让河畔、点唱机、电视等对象播放对应的音频
- **立体声布置 / 多扬声器会场**: 把 L / R / 单声道分配给多个图元，把立体声铺开到空间中
- **5.1ch 源的会场展开**: 把 5.1ch 各声道分别布置到 6 个图元上 (FL / FR / C / LFE / SL / SR)

所有操作都通过 **在图元的 Description (说明文字) 字段里写标签** 完成。无需 LSL 脚本，无需 SL 服务端改动，听到 3D 音频的只有使用 AYAstorm 的用户。其他 Viewer (主线 Firestorm、官方 LL Viewer 等) 会忽略此类标签，因此不会产生兼容性问题。

---

## 2. 快速上手

### 2.1 让一个图元播放声音 (最简示例)

随意创建一个图元，在它的 **Description 字段** 里写入：

```
[3dstream:{url:http://example.com/stream.mp3}]
```

仅此而已。`http://example.com/stream.mp3` 这条流就会从该图元位置以 3D 定位播放。离开图元音量减小，左右移动时声像自然变化。

### 2.2 立体声布置 (把 L / R 拆到不同图元)

把立体声音源拆给两个图元，让立体声在空间中铺开。

1. 把根图元 (Root) 和子图元 (Child) 各 1 个链接起来 (Ctrl+L)
2. **Root 的 Description**:
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]
   ```
3. **在 Root 的 Description 中追加** (Root 自身也作为 L 扬声器):
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}{ch:L}]
   ```
4. **Child 的 Description**:
   ```
   [3dstream-stereo:{ch:R}]
   ```

这样 Root 出 L、Child 出 R。

### 2.3 更详细的内容

请阅读第 3 节及之后。会讲到多扬声器、5.1ch、细节调优、推流端配方等。

---

## 3. 术语

| 术语 | 含义 |
|---|---|
| **流 (stream)** | 通过 HTTP 推送的音频数据 (SHOUTcast / Icecast / 静态文件等)。主要支持的 codec：MP3 / Vorbis / Opus / FLAC |
| **链接组 (linkset)** | 用 SL 的 "link" 操作 (Ctrl+L) 合并的图元集合。1 个根 + N 个子图元 |
| **根图元 (root prim)** | 链接组的父图元。在 Build → Edit 中关闭 "Edit linked" 时点击会先选中的那一个 |
| **子图元 (child prim)** | 链接组中除根之外的图元 |
| **音源声明** | Description 中含 `{url:...}` 的图元。声明 "播放哪条流"。**只能写在根图元上** (写在子图元上会被忽略) |
| **扬声器图元** | Description 中含 `{ch:...}` 的图元。实际发声的图元。**根图元、子图元都可以** |
| **binding (绑定)** | 内部按链接组组装的"音源 → 扬声器组"对应关系。1 个链接组 = 1 个 binding |
| **ch (声道)** | 扬声器图元负责的音频声道。`L` / `R` / `M` (单声道)，以及 5.1ch 用的 `FL` / `FR` / `C` / `LFE` / `SL` / `SR` |
| **rolloff (距离衰减)** | 听者远离扬声器时音量逐渐减小的设置 |

---

## 4. 标签总览

### 4.1 两种标签

| 标签 | 前缀 | 用途 |
|---|---|---|
| **单声道标签** | `[3dstream:...]` | 单个图元播放 1 条流 (最小配置) |
| **分散立体声 / 会场布置标签** | `[3dstream-stereo:...]` | 链接组中多个图元同步播放 1 条流 (立体声 / 多扬声器 / 5.1ch) |

### 4.2 旧前缀的别名

两种标签都把旧前缀 (`[ayastream:...]` / `[ayastream-stereo:...]`) 作为 **永久别名** 接受。r5 (2026-05) 把 `ayastream` 重命名为 `3dstream` 时，为了让此前已布置好的图元不必重新编辑而保留了旧前缀。**新内容推荐使用 `3dstream`** 系列，但混用也没问题。

```
[3dstream:{url:...}]              ← 推荐 (canonical)
[ayastream:{url:...}]             ← 旧式，兼容接受

[3dstream-stereo:{url:...}{ch:L}] ← 推荐 (canonical)
[ayastream-stereo:{ch:L}]         ← 旧式，兼容接受
```

### 4.3 通用书写规则

- **标签可以写在 Description 的任何位置**。前后的其他文字会被忽略 (例如 `店名 [3dstream:{url:...}] 营业中` 之类的与说明文字共存均可)。
- 字段是 `{key:value}` 形式的集合，字段间不需要分隔符 (空格、逗号、连写都行)。
- **键名不区分大小写** (内部正规化为小写)。`{URL:...}` 与 `{url:...}` 等价。
- **值前后的空格会被 trim**。`{ url : http://example/  }` 也可以。
- **未知键被静默忽略**。例如 `{foo:bar}` 没有效果，但也不会报错。
- 同一个图元的 Description 中写了 **多个同种标签时只采用最先出现的那一个**。

### 4.4 SL 的 Description 字数限制 (127 字节)

LSL `llSetObjectDesc` 能写入的 Description **上限为 127 字节**。包含中文 / 日文等多字节字符时，按 UTF-8 编码很容易超过。因此长 URL 应当 **缩短**，或采用根写音源声明、子图元只写 `{ch:...}` 的分散方式 (§6) 来周转。

### 4.5 标签生效时机

- AYAstorm **每隔 30 秒轮询** 一次范围内的图元 Description (`Stream3DPollInterval` 设置)。
- 通过 LSL `llSetObjectDesc` 修改 Description 后，下一次轮询会重新评估并生效 (通常 5〜30 秒内)。
- 手工右键图元 → Edit → 修改 Description 时，编辑确认后立刻重新评估 (通过 Properties 通知)。
- Link / Unlink 操作也会触发重新评估。

---

## 5. 单声道标签 `[3dstream:...]`

### 5.1 语法

```
[3dstream:{url:URL}{min:N}{max:N}]
```

或使用旧前缀：

```
[ayastream:{url:URL}{min:N}{max:N}]
```

### 5.2 键一览

| 键 | 必需 | 类型 | 默认值 | 含义 |
|---|---|---|---|---|
| `url` | **必需** | 字符串 | — | 流 URL (`http://` / `https://`)。空字符串视为错误 |
| `min` | 可选 | F32 (m) | `Stream3DRolloffMin` (1.0) | 距离衰减的 **近距离** (从该图元起此距离以内音量保持 100%) |
| `max` | 可选 | F32 (m) | `Stream3DRolloffMax` (20.0) | 距离衰减的 **远距离** (此距离以外音量为 0%) |

衰减模型为 FMOD 的 `FMOD_3D_LINEARSQUAREROLLOFF` (线性平方衰减)。在 `min` 与 `max` 之间平滑衰减。

### 5.3 行为

- 标签可以写在链接组中 **任意图元** (根或子)。写了标签的图元自身就是发声扬声器。
- 立体声音源会 **在内部混合 L/R 转为单声道** 播放。
- 如果同一链接组内同时还写了 `[3dstream-stereo:...]`，单声道标签 **不会** 优先用作该图元的扬声器指派 — 两条 binding 路径独立评估。不推荐把同一个图元用于两种用途 (行为未定义)。

### 5.4 示例

#### 5.4.1 最小配置

```
[3dstream:{url:http://example.com/radio.mp3}]
```

省略 `min` / `max`，使用设置默认值 (1m / 20m) 衰减。

#### 5.4.2 自定义距离

```
[3dstream:{url:http://example.com/radio.mp3}{min:2}{max:50}]
```

距图元 2m 以内音量最大，50m 处归零。在大型户外场地希望声音传得远时使用。

#### 5.4.3 与说明文字共存

```
店内 BGM [3dstream:{url:http://radio.example.jp/8000/jazz}] 多谢光临
```

标签前后有其他文字也没问题。

---

## 6. 分散立体声 / 会场布置标签 `[3dstream-stereo:...]`

### 6.1 语法

```
[3dstream-stereo:{url:URL}{range:N}{ch:CH}{volume:V}]
```

或使用旧前缀：

```
[ayastream-stereo:...]
```

此标签 **以整个链接组为单位处理 1 条流**。根图元声明"播放哪条流"，链接组内各图元声明"自己负责哪个声道"。

### 6.2 图元的角色

每个图元根据标签内容承担以下角色：

| Description 中的字段 | 角色 |
|---|---|
| 含 `{url:...}` | **音源声明** (仅根图元有效，子图元写 `{url:...}` 会被忽略) |
| 含 `{ch:...}` | **扬声器** (根 / 子图元都可以) |
| 同时含两者 (= 仅根) | 音源声明 + 自身也作为扬声器 |
| 两者都没有 | 不做任何事 (不属于 binding 对象) |

链接组中同时存在 **音源声明 (= 带 `{url}` 的根)** 和 **至少 1 个扬声器 (= 带 `{ch}` 的图元)** 时才会开始播放。扬声器为 0 个时会触发"结构错误"，并发出错误通知 (§11)。

### 6.3 键一览

#### 6.3.1 仅在根图元上有效的键

| 键 | 必需 | 类型 | 默认值 | 含义 |
|---|---|---|---|---|
| `url` | **必需** | 字符串 | — | 流 URL。空字符串视为错误 |
| `range` | 可选 | F32 (m) | `Stream3DRolloffMax` (20.0) | 链接组内扬声器没有自己 `range` 时使用的默认衰减距离 |

#### 6.3.2 扬声器声明键 (任意图元)

| 键 | 必需 | 类型 | 默认值 | 含义 |
|---|---|---|---|---|
| `ch` | **必需** | 枚举 | — | 该图元负责的声道 (详见 §7) |
| `range` | 可选 | F32 (m) | 按 扬声器自身 → 根 `range` → `Stream3DRolloffMax` 顺序回退 | 该扬声器单独的衰减距离 |
| `volume` | 可选 | F32 [0.0〜1.0] | 1.0 | 该扬声器单独的音量倍率 |

> **重要**: 单声道标签的 `min` / `max` 键在分散立体声标签中 **会被忽略**。分散立体声内部固定近距离为 1.0m，远距离使用 `range` 键 (或默认值 `Stream3DRolloffMax`)。

### 6.4 1 个根 + 1 个子 (基础立体声对)

最小立体声布置：

```
根 Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:L}]

子 Description:
  [3dstream-stereo:{ch:R}]
```

根负责 L，子负责 R。根与子的 **链接顺序 (link number)** 不影响播放。在空间中放在哪里决定了定位。

### 6.5 多扬声器 (4 个以上)

把同一立体声流从场地 4 个角的扬声器播放：

```
根 Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]

子 #1 Description:
  [3dstream-stereo:{ch:L}{range:50}]

子 #2 Description:
  [3dstream-stereo:{ch:R}{range:50}]

子 #3 Description:
  [3dstream-stereo:{ch:L}{volume:0.7}]

子 #4 Description:
  [3dstream-stereo:{ch:R}{volume:0.7}]
```

- 根仅做音源声明，自身不发声 (没有 `{ch}`)
- 子 #1、#2 分别承担 L/R，近距 50m，音量 100%
- 子 #3、#4 把同一 L/R 以 70% 音量播出 (前段补充)
- 扬声器数上限由 `Stream3DStereoMaxSpeakers` 设置控制，**默认 16 个**(§10)

### 6.6 5.1ch 会场布置 (6 图元)

把 5.1ch 源 (Opus surround / FLAC 6ch) 展开到 6 个扬声器：

```
根 Description:
  [3dstream-stereo:{url:http://example.com/test_5_1.flac}{range:30}]

FL 图元:  [3dstream-stereo:{ch:FL}]
FR 图元:  [3dstream-stereo:{ch:FR}]
C 图元:   [3dstream-stereo:{ch:C}]
LFE 图元: [3dstream-stereo:{ch:LFE}]
SL 图元:  [3dstream-stereo:{ch:SL}]
SR 图元:  [3dstream-stereo:{ch:SR}]
```

- 把每个图元物理摆放到 "会场扬声器位置" (舞台前 L/R、中置、低音炮、环绕 L/R)
- LFE 与其余 5 个一视同仁 (Viewer 端不做低通滤波之类的特殊处理。如需低频限制请在推流端 mix 时完成)
- 听者没有 "影院最佳位"概念 (= SL 自由视角模型)。在场地中走动时 5.1 mix 的预定定位当然会破坏。请按 **"会场 PA 风格的多点布置"**，而不是"影院环绕体验"来运用

### 6.7 把同一 ch 分配给多个图元

把 `{ch:L}` 写到 2 个或更多图元，多个图元都会播放 L 声道。可用于"舞台前排 L"和"舞台后排 L"等多个扬声器的场景。

反之，如果没有任何图元写 `{ch}`，会触发 "扬声器 0 个" 结构错误。

### 6.8 根图元同时兼任扬声器

```
根 Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:M}{range:25}]
```

像这样在 1 条标签中并写 `{url}` 与 `{ch}` 时，根作为音源声明，自身也作为 M (单声道) 扬声器工作。也可用于完全没有子图元的简易 mono 配置 (与 `[3dstream:...]` 在功能上几乎等价)。

### 6.9 如何识别根图元

编辑链接组时，Build 浮窗的 **Object** 选项卡里 "Selected" 会显示当前选中的图元，链接组的父图元 (= 根) 通常是 **最初被选中并发起链接的那一个**。

最可靠的确认方法：
- Build → Edit → 关闭 "Edit linked" → 点击任一图元 → 选中的就是该链接组的根
- LSL: `llGetLinkNumber()` 在子图元存在时根返回 `1`。没有子图元的单一图元返回 `0`

根与子的 link number (1, 2, 3, ...) **不影响 3D Stream 的播放**。r5 之前用 link number 决定 L/R 的旧规格已废弃，r8 起改为 `{ch:...}` 标签声明制。

---

## 7. `ch` (声道) 取值参考

`{ch:值}` 可以指定以下 9 种。**不区分大小写** (`{ch:l}` 与 `{ch:L}` 等价)。

| 值 | 含义 | 主要用途 |
|---|---|---|
| `L` | 左声道 | 立体声 L |
| `R` | 右声道 | 立体声 R |
| `M` | 单声道 (L+R 平均) | "中置扬声器"用途，或单点播放 |
| `FL` | Front Left | 5.1ch 前左 |
| `FR` | Front Right | 5.1ch 前右 |
| `C` | Center | 5.1ch 中置 |
| `LFE` | Low Frequency Effects (低音炮) | 5.1ch 低频 |
| `SL` | Surround Left | 5.1ch 环绕左 |
| `SR` | Surround Right | 5.1ch 环绕右 |

源实际声道数与标签 `ch` 值不一致时，系统进行 **自动回退** 而不是报告"不一致" (§8)。例如对 2ch 源写 `{ch:FL}` 会播放 L。

非法值 (例如 `{ch:foo}`) 会触发 **格式错误** 通知 (§11)。

---

## 8. 源声道数 × 标签值 兼容矩阵

扬声器图元实际播放什么，由 **源 URL 的声道数** 与 **写下的 `ch` 值** 的组合决定。

### 8.1 兼容矩阵

| 源 | `{ch:L}` | `{ch:R}` | `{ch:M}` | `{ch:FL}` | `{ch:FR}` | `{ch:C}` | `{ch:LFE}` | `{ch:SL}` | `{ch:SR}` |
|---|---|---|---|---|---|---|---|---|---|
| **1ch (单声道)** | M | M | M | M | M | M | 静音 | 静音 | 静音 |
| **2ch (立体声)** | L | R | (L+R)/2 | L | R | (L+R)/2 | 静音 | 静音 | 静音 |
| **6ch (5.1)** | BS.775 L | BS.775 R | (BS.775 L + R)/2 | FL | FR | C | LFE | SL | SR |

图例：
- `L` / `R` / `FL` / `FR` / `C` / `LFE` / `SL` / `SR` 表示直接播放源对应声道
- `BS.775 L` = 用 ITU-R BS.775 下混系数把 6ch 折叠到 L/R 2ch 的结果 (§8.2)
- `静音` = 该扬声器不发声 (binding 仍保持，仅图元不出声)

### 8.2 BS.775 下混系数 (6ch 源 → L/R)

```
L_out = c × ( FL + 0.707·C + 0.707·SL + 0.5·LFE )
R_out = c × ( FR + 0.707·C + 0.707·SR + 0.5·LFE )
c = 1 / 2.914 ≈ 0.343 (防削波归一化)
```

中置均匀分到左右、环绕加到同侧、LFE 平均加到两侧。

### 8.3 同一源混合布置使用

如果把 5.1ch 源同时分配给 `{ch:L}` 与 `{ch:FL}`，L 图元播 BS.775 下混、FL 图元播 直取声道。容易混淆，因此推荐 **同一源在会场内使用同一系列的 ch (`L/R/M` 系 或 `FL/FR/...` 系，二选一)**。

混合布置中发生回退时，可以通过 **routing 诊断 chat 通知** (§10.3 / §11.3) 确认每个 ch 实际行为。布置 5.1ch 会场时打开它能立刻发现配置失误。

### 8.4 5.1ch 会场布置下播放 2ch / 1ch 源

会场已部署 6 个扬声器图元 (`ch:FL` / `FR` / `C` / `LFE` / `SL` / `SR`)，把源 URL 从 5.1ch 推流切到 **普通立体声 (2ch) 推流** 或 **单声道 (1ch) 推流** 的场景。例如 "正式演出走 5.1ch、休息时间用普通立体声 BGM"，"DJ set 之间穿插 MC 单声道语音"等运营。

此时 **完全无需重新布置或修改设置**。各扬声器图元会自动按下述方式工作。

#### 当 2ch (立体声) 源播放时

| 图元的 `ch` 值 | 播放内容 |
|---|---|
| `{ch:FL}` | **L** (代替前左，直接播放立体声 L) |
| `{ch:FR}` | **R** (代替前右，直接播放立体声 R) |
| `{ch:C}` | **(L+R)/2** (中置播放 L+R 的平均 = 单声道下混) |
| `{ch:LFE}` | **静音** (源中没有 LFE 信号) |
| `{ch:SL}` | **静音** (源中没有环绕左信号) |
| `{ch:SR}` | **静音** (源中没有环绕右信号) |

听感上："**会场前方 3 个 (FL / FR / C) 出立体声，环绕 3 个 (LFE / SL / SR) 静默**"。

#### 当 1ch (单声道) 源播放时

| 图元的 `ch` 值 | 播放内容 |
|---|---|
| `{ch:FL}` / `{ch:FR}` / `{ch:C}` | **M** (前 3 个都播放单声道，3 处发出相同的声音) |
| `{ch:LFE}` / `{ch:SL}` / `{ch:SR}` | **静音** |

#### 设计意图 / 补充

- **前方 3 个 (FL / FR / C) 无论源声道数都一定有声** ─ 因此在 5.1ch ↔ 2ch ↔ 1ch 间切换源时，会场前方不会失声。
- **当源没有对应信号时 LFE / SL / SR 固定为静音** ─ 不会合成假低音或假环绕。
- **源回到 5.1ch 时各图元自动恢复为对应声道直取** (URL 切换后重连时重新评估)。无需重新布置或重新设置。
- 5.1ch 会场长期播放 2ch BGM 是 **正规运营模式**。"环绕静音" 是规格而非故障。

#### 把回退情况输出到 Chat 的设置 (推荐: 布置 / 验证期间)

提供了一个 **可在 Local Chat 中确认** "静音的 prim 究竟是按规格回退而静音、还是出了什么问题" 的诊断开关。

**设置位置** (两者作用相同且同步):

- **Preferences > Sound > Show channel routing diagnostics in chat** (复选框)
- **Debug Settings: `Stream3DRoutingDiagnostic`** (`true` / `false`)

打开后，5.1 布置 × 2ch / 1ch 源出现回退时，会以 **Local Chat 中你自己的发言形式** 按下述格式逐行输出 (`3D Stream:` 前缀由 §11.3 helper 自动添加):

**2ch 源 × 5.1 布置 (FL/FR/C/LFE/SL/SR 6 个 prim) 时**:

```
[12:34] You: 3D Stream: ch:FL prim playing L (source is 2ch)
[12:34] You: 3D Stream: ch:FR prim playing R (source is 2ch)
[12:34] You: 3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)
[12:34] You: 3D Stream: ch:LFE prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SL prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SR prim silent (source is 2ch)
```

**1ch 源 × 5.1 布置 时**:

```
[12:35] You: 3D Stream: ch:FL prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:FR prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:C prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:LFE prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SL prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SR prim silent (source is 1ch)
```

由此可一目了然地看出 "LFE / SL / SR 静音是规格行为，FL / FR / C 处于回退播放中"。

通知按 `(root_id, url, observed_channel_count, prim_set_signature)` 作为 throttle 的键，因此布置或源声道数不变时同一通知不会重复发送。**推荐：仅在 5.1ch 会场布置 / 验证期间打开，正式运行时关闭 (`false`，默认值)**。详见 §11.3。

#### 反方向: 2ch 布置下播放 5.1ch 源

为参考起见整理反方向。仅用 `{ch:L}` / `{ch:R}` / `{ch:M}` 布置的立体声会场 (= 2ch 布置) 播放 5.1ch 源时:

- L / R / M 图元用 **BS.775 下混** (§8.2) 把 5.1ch 折叠为 2ch 播放。FL / C / SL / LFE 都按系数加到 L 侧，FR / C / SR / LFE 都按系数加到 R 侧。
- 所有声道信号都通过 L / R 出来，因此 **没有声道会失声**。
- `Stream3DRoutingDiagnostic` 打开时 Local Chat 输出示例:

```
[12:36] You: 3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)
[12:36] You: 3D Stream: C content folded into BS.775 downmix (source is 6ch, no ch:C prim)
[12:36] You: 3D Stream: LFE content folded into BS.775 downmix (source is 6ch, no ch:LFE prim)
[12:36] You: 3D Stream: SL content folded into BS.775 downmix (source is 6ch, no ch:SL prim)
[12:36] You: 3D Stream: SR content folded into BS.775 downmix (source is 6ch, no ch:SR prim)
```

按声道分别通知 "没有专用 prim，所以折叠到 BS.775 下混 path"。

---

## 9. 推流端 (制作源 URL)

### 9.1 支持的 codec / 容器

| codec / 容器 | 1ch | 2ch | 6ch | 备注 |
|---|---|---|---|---|
| **MP3** | ✓ | ✓ | — | SHOUTcast / Icecast 的传统路径 |
| **Vorbis (Ogg)** | ✓ | ✓ | ✓ | 6ch 也已实机验证 (r9 P10) |
| **Opus (Ogg)** | ✓ | ✓ | △ | 6ch 用 Opus channel mapping family 1。**纯 HTTP / Icecast push 时 seek 失败** 可能无法打开 (§9.4) |
| **FLAC** | ✓ | ✓ | △ | 6ch 理论上支持，与 Opus 相同的 seek 限制 |
| AAC (ADTS / HLS) | — | — | — | 不支持 |
| AC-3 / E-AC-3 | — | — | — | 因 Dolby 授权问题不支持 |

源 URL 接受 `http://` 或 `https://`。能维持 HTTP/1.1 keep-alive 的路径 (= SHOUTcast 兼容 streamer 或 ffmpeg 的 TCP 输出) 比单纯静态 HTTP 更稳定。

### 9.2 1ch / 2ch 推流

普通 SHOUTcast / Icecast / 静态 HTTP 即可。MP3 / Vorbis / Opus / FLAC 均能正常工作。`oggenc` / ffmpeg / butt 等常规推流工具直接可用。

### 9.3 5.1ch (Vorbis 6ch) 推流

要在 Viewer 端可靠地跑 5.1ch，推荐路径是 **Vorbis 6ch** (r9 P10 已实机验证)。

#### 9.3.1 测试素材制作 (ffmpeg)

```bash
# 各声道埋入唯一频率的 5.1 WAV (10 秒)
ffmpeg -f lavfi -i "sine=440:d=10" -f lavfi -i "sine=550:d=10" \
       -f lavfi -i "sine=660:d=10" -f lavfi -i "sine=110:d=10" \
       -f lavfi -i "sine=770:d=10" -f lavfi -i "sine=880:d=10" \
       -filter_complex "[0:a][1:a][2:a][3:a][4:a][5:a]amerge=inputs=6[a]" \
       -map "[a]" -ac 6 -channel_layout 5.1 test_5_1.wav

# 编码为 Vorbis 6ch
ffmpeg -i test_5_1.wav -c:a libvorbis -q:a 5 test_5_1.ogg
```

#### 9.3.2 静态 HTTP 提供 (验证用)

```bash
python3 -m http.server 8080
```

URL: `http://<host>:8080/test_5_1.ogg`

#### 9.3.3 实时推流 (ffmpeg → Icecast)

```bash
ffmpeg -re -i test_5_1.wav \
  -c:a libvorbis -q:a 5 \
  -ac 6 -ar 48000 \
  -content_type audio/ogg \
  -f ogg icecast://source:hackme@localhost:8000/aya_5_1.ogg
```

主要选项:
- `-re` = real-time (按素材时长投递，模拟现场推流)
- `-content_type audio/ogg` = 向 Icecast 申报 MIME (没有这个会被误判为 MP3)
- `-ac 6 -ar 48000` = 维持 6ch 48kHz

### 9.4 Opus 6ch / FLAC 6ch 的限制

通过 **纯 HTTP** (例如 `python3 -m http.server`) 或 **Icecast push** 推送 Opus 6ch (channel mapping family 1) 或 FLAC 6ch 时，FMOD 的 parser 会发出 **seek 请求**，从而以 `FMOD_ERR_FILE_COULDNOTSEEK` 失败而无法打开。

变通方法:

- 使用 **SHOUTcast 兼容 streamer** (支持 keep-alive + range)
- 通过 **ffmpeg primary** 中转 (TCP backpressure 解决)
- 使用 **5.1ch GUI 推流工具 `butt-aya`** push (AYA さん的另一项目，本文撰写时尚未公开)

要确保能跑通，目前最短路径是选择 **Vorbis 6ch**。

### 9.5 推流端工具的选择

| 工具 | 用途 | 注意 |
|---|---|---|
| **ffmpeg** | 任意 codec / 任意 ch / 静态 / 实时 | 需要命令行操作，最灵活 |
| **butt** (官方) | DJ 推流 | 仅支持 1ch / 2ch，不支持 5.1ch |
| **butt-aya** (5.1ch fork) | 5.1ch GUI 推流 | AYA さん的另一项目，本文撰写时尚未公开 |
| **Liquidsoap** | 高级广播自动化 | 配置复杂，面向高阶用户 |
| **Mixxx / DarkIce / ezstream** | DJ / 自动化 | 以立体声为前提，不支持 5.1ch |

---

## 10. Viewer 端设置

### 10.1 通过 Preferences

**首选项 → Sound** 选项卡有以下控件：

- **3D Stream** 滑条 — 全部流的主音量倍率 (`Stream3DVolumeMaster`)
- **Enabled** 复选框 — 整个功能 ON/OFF (`Stream3DEnabled`)
- **Show channel routing diagnostics in chat** — routing 诊断通知 (`Stream3DRoutingDiagnostic`，§11.3)
- **Hear media and sounds from:** — 听者位置选择 Camera / Avatar (`MediaSoundsEarLocation`，§13.1)

扬声器图标的 Volume 下拉菜单中也有同样的 "3D Stream" 滑条，可以在语音聊天附近即时调整音量。

### 10.2 Debug Settings (高级调优)

`Ctrl + Alt + D` 打开 Advanced 菜单 → Show Debug Settings 可以直接编辑各键。

| 设置键 | 类型 | 默认值 | 含义 |
|---|---|---|---|
| `Stream3DEnabled` | bool | `true` | 整个功能的 kill switch。设为 `false` 立刻拆除所有 binding，重新 enable 不会自动 re-bind (下次 poll 时重新发现) |
| `Stream3DDescriptionScan` | bool | `true` | 设为 `false` 时停止 Description 标签扫描，所有图元 binding 解除 (debug stream 不受影响) |
| `Stream3DMaxConcurrent` | S32 | `4` | 同时 binding 上限 (mono + stereo 合计)。0 表示无限 |
| `Stream3DStereoMaxSpeakers` | S32 | `16` | 单链接组扬声器数上限。超过部分按 traversal 末尾被丢弃并发出警告通知 |
| `Stream3DRolloffMin` | F32 (m) | `1.0` | 单声道标签默认近距离 (mono `{min}` 省略时) |
| `Stream3DRolloffMax` | F32 (m) | `20.0` | 默认远距离 (mono `{max}` / stereo `{range}` 省略时的共同回退值) |
| `Stream3DMaxDistance` | F32 (m) | `64.0` | 图元发现 polling 半径。设为 ≥ `Stream3DRolloffMax` |
| `Stream3DPollInterval` | F32 (秒) | `30.0` | Description polling 间隔。LSL 改动标签的检测延迟由此决定。0 关闭主动 polling |
| `Stream3DVolumeMaster` | F32 [0〜1] | `0.5` | 主音量倍率。与 Preferences 的 3D Stream 滑条同步 |
| `Stream3DReconnectAttempts` | S32 | `3` | 流断开时自动重连尝试次数。每次重试等待 5 秒。0 关闭重连 |
| `Stream3DRoutingDiagnostic` | bool | `false` | routing 诊断 chat 通知 ON/OFF (§11.3)。与 Preferences 的复选框同步 |

#### 仅供 Debug 使用 (动作确认用)

| 设置键 | 类型 | 用途 |
|---|---|---|
| `Stream3DDebugUrl` | 字符串 | Debug 用 URL |
| `Stream3DDebugPlay` | bool | `true` 时在角色前方 5m 放置并播放单声道流 (无需写标签的快速测试) |
| `Stream3DDebugStereoPlay` | bool | 立体声版的同样 debug 播放 |

### 10.3 设置的持久化与即时生效

绝大多数设置都是 **"Live"** = 修改后下一帧起生效。无需重启 Viewer。例外:

- `Stream3DEnabled` 从 `false` 切回 `true` 时不会自动 re-bind。下一个 poll cycle (默认 30 秒以内) 再发现。
- `Stream3DDescriptionScan` 切换会立刻拆除 / 重新发现所有 binding。

---

## 11. 错误通知 / 诊断

### 11.1 通知的呈现方式

标签格式错误或结构错误会 **以本地 Chat 通知** 显示。前缀为 "3D Stream:"，作为系统消息呈现。

```
3D Stream: 标签格式错误 (对象名: "MySpeaker")
  ch 的取值必须是 L/R/M/FL/FR/C/LFE/SL/SR 之一。
  示例: [3dstream-stereo:{ch:L}{range:30}]
```

```
3D Stream: 结构错误 (链接组 root: "MainStage")
  根上有音源声明 (url) 但找不到扬声器 (ch)。
  请在各扬声器图元上写 [3dstream-stereo:{ch:L|R|M}]。
```

### 11.2 30 秒抑制

同一图元 × 同一错误种类的通知会被 **抑制 30 秒**。这是为了防止你连续编辑标签时聊天被刷屏。30 秒后会再发出一次。

被抑制的通知仍会写入 LL_DEBUGS 日志 (debug 用日志)，因此可以通过日志看到内部发生了什么。

### 11.3 Routing 诊断 (5.1ch 布置)

为 5.1ch / 多扬声器会场的 **构建 / 验证期间** 提供的诊断功能，在 Local Chat 中确认 "哪个 prim 在播什么 / 为什么静音"。**默认 OFF**，不显式开启就不会输出。

#### 11.3.1 开启方法

下面任一方式均可启用 (两者同步):

- **Preferences > Sound > Show channel routing diagnostics in chat** ─ 勾选复选框
- **Debug Settings: 把 `Stream3DRoutingDiagnostic` 设为 `true`** (Advanced > Show Debug Settings)

设置即时生效。无需重启 Viewer。

#### 11.3.2 输出位置和格式

打开后，回退发生时会以 **Local Chat 中你自己的发言形式** 按下述格式逐行输出:

```
[HH:MM] You: 3D Stream: <内容>
```

`3D Stream:` 前缀由 `notifyStream3D` helper 自动添加。聊天日志 (`Show in Chat`) 当然也会保留，验证后可以回看。**他人看不到** (仅在你自己的 Local Chat 中显示的伪发言)。

#### 11.3.3 通知文言一览

| 情境 | Local Chat 中输出的行 |
|---|---|
| 6ch 源 × 仅有 `ch:L/R/M` 图元 (无专用 prim) | `3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)` (FL/FR/C/LFE/SL/SR 各声道分别输出) |
| 6ch 源 × 既无专用 prim 也无 `ch:L/R/M` prim | `3D Stream: FL content has no destination — dropped (source is 6ch, no ch:L/R/M prim)` |
| 2ch 源 × `ch:FL` 图元 | `3D Stream: ch:FL prim playing L (source is 2ch)` |
| 2ch 源 × `ch:FR` 图元 | `3D Stream: ch:FR prim playing R (source is 2ch)` |
| 2ch 源 × `ch:C` 图元 | `3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)` |
| 1ch 源 × `ch:FL/FR/C` 图元 | `3D Stream: ch:FL prim playing M (source is 1ch)` (FR / C 同样格式) |
| 1ch / 2ch 源 × `ch:LFE/SL/SR` 图元 | `3D Stream: ch:LFE prim silent (source is 2ch)` (1ch 时为 `1ch`，SL / SR 同样格式) |

5.1ch 布置 × 2ch 源的具体输出示例参见 §8.4 的 "把回退情况输出到 Chat 的设置"。

#### 11.3.4 throttle 与重新显示条件

通知按 `(root_id, url, observed_channel_count, prim_set_signature)` 作为 throttle 的键。同一会场 / 同一源结构持续期间 **不会重复显示** (避免聊天被刷屏)。下列任一变化时重新评估并再次输出:

- 源 URL 改变 (= 切到不同的流)
- 源声道数改变 (= 同一 URL 但发生了 5.1ch ↔ 2ch 切换)
- 会场扬声器图元结构改变 (添加 / 删除 prim 或改变 `ch` 值)
- `Stream3DRoutingDiagnostic` 由 OFF 切到 ON 的瞬间

#### 11.3.5 运营建议

- **会场搭建 / 布置验证期间打开** ON，在 Local Chat 中确认每个 prim 的回退行为
- **正式运营 (现场演出等) 时关闭** OFF，保持聊天整洁
- 默认 (OFF) 是面向正式运营的状态。仅在布置验证时手动打开。

### 11.4 日志 (`LL_INFOS("Stream3D")`)

详细的运行日志记录在 AYAstorm 的日志文件中。

```
~/.ayastorm_x64/logs/AYAstorm.log
```

按 `Stream3D` channel grep 可以看到 binding 建立 / 拆除 / 重连 / 源格式探测 / dropout 等。

---

## 12. 故障排查

### 12.1 写了标签但没声音

按顺序检查:

1. **Description 是否真的被改写**: 右键图元 → Edit → 查看 Description 选项卡的当前值
2. **标签拼写**: 是否包含 `[3dstream:` 或 `[3dstream-stereo:` (注意拼写错误)
3. **`{url:...}` 协议是否为 http/https**: `file://` 或相对 URL 不可
4. **`Stream3DEnabled` / `Stream3DDescriptionScan` 是否都为 true**: 在 Preferences > Sound 或 Debug Settings 确认
5. **等待轮询**: LSL `llSetObjectDesc` 的修改最多等 30 秒 (= `Stream3DPollInterval`)
6. **聊天里有没有错误通知**: 参考 §11 错误文言
7. **日志中是否出现 `LL_INFOS("Stream3D")` 的 reconnect attempt**: 可能是流 URL 已断

### 12.2 立体声只出来一边

- 根只写了 `{url}` 而子只写了 `{ch:R}` 时，没有图元负责 L 声道，会变成 "L 缺失" 状态。请在根并写 `{ch:L}`，或给另一个图元分配 `{ch:L}`
- 故意把 `{ch:L}` 写到 2 个图元让两处同时出 L 是符合预期的，没问题
- 打开 routing 诊断 (`Stream3DRoutingDiagnostic`) 后，可以在 chat 中看到每个 ch 实际播放什么

### 12.3 5.1ch 源打不开 / 声音断断续续

- §9.4 的 seek 限制: Opus 6ch / FLAC 6ch 在纯 HTTP / Icecast push 上容易出现的问题。**改用 Vorbis 6ch**，或改走 SHOUTcast 兼容 streamer / ffmpeg primary
- HTTP 切换后最初 5〜10 秒在 prebuffer 充填中可能出现 dropout 警告 (LAN 环境 408〜2045 frames/spk/s ≈ 0.8〜4% 程度)。稳态运行时会消失
- 流码率过高 / 网络拥塞时的 dropout: 推流端降低码率 (推荐 ≤ 256kbps) / 减少同时 binding 数

### 12.4 子图元写了标签但识别不到为扬声器

- 子图元的 Description 通过 Properties 通信获取。**首次进入链接组所在地区时可能稍有延迟** (数秒〜10 秒)
- 通过 LSL `llSetObjectDesc` 改子图元的 Description 后，下一个 poll cycle (30 秒内) 生效
- 检查 `{ch:...}` 的值有无拼写错误 (大小写无关，但拼错就无效)

### 12.5 听者位置不对劲 (声音方向有点怪)

- 摄像机大幅移动时听者位置会跟随摄像机变化，定位也随之改变。希望固定在角色视角的话，把 Preferences > Sound 的 **"Hear media and sounds from:" 切到 Avatar** (`MediaSoundsEarLocation = 1`)
- Camera / Avatar 切换对 3D Stream 同样起作用 (与地块 BGM、LSL `llPlaySound` 等共享设置)

### 12.6 想同时播放多个 3D Stream

- 超过 `Stream3DMaxConcurrent` (默认 4) 后新 binding 会被拒绝。如需同时运行更多请提高该值 (8 / 16 量级仍实用)
- 注意 1 binding = 1 解码线程 + N 个扬声器声道占用 CPU。20 并行 CPU 负担很大，按需增加
  
### 12.7 删了标签声音还停不下来

- 重新评估的触发可能没发生。请把图元移动一下，或绕一圈等待下次 polling
- 仍然停不下来时把 `Stream3DEnabled` 暂时切到 false 强制拆除所有 binding，再切回 true 重新发现

---

## 13. 已知限制 / 规格说明

### 13.1 听者位置基于摄像机或角色

3D Stream 用于声场计算的听者位置遵循 Preferences > Sound 的 **"Hear media and sounds from:"** 设置。

- `Camera` (默认): 摄像机位置 / 朝向
- `Avatar`: 角色位置 / 朝向

这个设置同时作用于 LSL `llPlaySound` / 地块 BGM / Media-on-a-Prim。

### 13.2 1 链接组 = 1 流

每个链接组中只关心带 `{url}` 的根 "存在 / 不存在"。**不能在 1 个链接组里写多个 `{url}`** (子图元写 `{url}` 会被忽略)。

要在 1 个会场里同时跑多个不同的流，请把链接组拆分摆放 (= 在 `Stream3DMaxConcurrent` 限额内持有多个 binding)。

### 13.3 Description 字数 (127 字节)

LSL `llSetObjectDesc` 能写入的 Description 上限为 **127 字节**。包含中文 / 日文 / 韩文等的 URL 或说明文字按 UTF-8 编码很容易超过。

变长时的对策:

- 根上只写 `{url}`、子图元只写 `{ch}` 的分散方式 (这种情况下每个图元的 Description 都能保持简短)
- 缩短 URL (URL shortener，或推流端把路径缩短)

### 13.4 各 codec 的实测情况

| codec | 1ch / 2ch | 6ch |
|---|---|---|
| Vorbis (Ogg) | ✓ 实机已验证 | ✓ 实机已验证 (r9 P10，连续 12 分钟，0 dropout) |
| Opus (Ogg) | ✓ 实机已验证 | △ 仅代码审阅 (生产路径有运行实绩，static HTTP / Icecast push 上 seek 失败) |
| FLAC | ✓ 实机已验证 | △ 仅代码审阅 (与 Opus 同样限制) |
| MP3 | ✓ 实机已验证 | — |

希望可靠地跑 5.1ch 时请选择 **Vorbis 6ch**。

### 13.5 LFE 没有特殊处理

5.1ch 的 LFE (低音炮) 在 Viewer 端不做 "低通滤波"、"2D 化" 等特殊处理，与其余 5 声道同样进行 3D 布置 + 距离衰减。如需低频限制请在推流端 mix 时完成。

设想的运用方式是: 把物理意义上的低音炮形状的图元放在 SL 里的合适位置，让低频音从那里发出来。

### 13.6 5.1ch 的自由视角模型

真实 5.1 (影院基准 / ITU-R BS.775) 以 **听者处于固定位置** 为前提，在每个 ch 中嵌入方向感。SL 的听者是自由视角的，因此 "sweet spot" 概念不适用。本功能追求的是 **"在会场多点重现 5.1 源"** 的 PA 风格构想，而不是影院环绕体验的复刻。

听者在空间中走动时 5.1 mix 的预定定位当然会破坏，但 "会场感"、"面状响起的感觉" 仍然能充分体现。

### 13.7 在其他 Viewer 中的行为

`[3dstream:...]` / `[3dstream-stereo:...]` 标签是 **AYAstorm 专属**。主线 Firestorm、官方 LL Viewer、Catznip 等其他 Viewer 完全忽略它们。

- AYAstorm 用户: 按设计 3D 定位播放
- 其他 Viewer 用户: 标签只作为说明文字的一部分显示，不出声 (与地块 BGM 独立运作，地块 BGM 已设置时仍可听到)

### 13.8 同时上限

| 上限 | 默认 |
|---|---|
| `Stream3DMaxConcurrent` (链接组级 binding 数) | 4 |
| `Stream3DStereoMaxSpeakers` (1 binding 的扬声器数) | 16 |
| 由此推出的最大同时扬声器数 | 4 × 16 = 64 |

64 channel 量级处于 FMOD 余量内。需要更多请通过 debug settings 提高数值 (实机 CPU 负载确认必不可少)。

### 13.9 音量合成

最终音量 = `Stream3DVolumeMaster` × `{volume:N}` × FMOD 距离衰减 × Master Audio Slider × 各种静音状态。

通常用 `Stream3DVolumeMaster` (Preferences 的 3D Stream 滑条) 做整体调整、`{volume:N}` 做图元级别校正、距离衰减由 `range` (扬声器单独) 或 `Stream3DRolloffMax` (整体默认) 控制。

---

## 14. 相关文档 / 内部规格书

本指南是 **面向使用者** 的格式参考。实现内部细节 (decode thread / FMOD 路径 / 环形 buffer / 关闭顺序等) 请参考下述规格书。

| 文档 | 内容 |
|---|---|
| `doc/spec_positional_stream_audio.md` | 3D Stream 主体规格 (r5 修订) — 基础架构 |
| `doc/spec_stream3d_decode_thread.md` | r7 确立的 3-thread 模型 |
| `doc/spec_distributed_stereo.md` | r8 分散描述立体声规格 — `[3dstream-stereo:...]` 的 field 书式 |
| `doc/spec_5_1ch_source.md` | r9 5.1ch 源接收规格 — Opus/FLAC 6ch decode 路径 + BS.775 下混 |
| `doc/spec_5_1ch_placement.md` | r10 5.1ch 会场布置规格 — `ch=FL/FR/C/LFE/SL/SR` 扩展 + 兼容矩阵 |
| `doc/spec_binaural_venue_reverb.md` | r11 (未发布) 双耳 + 会场混响规格 — 本指南只到 r10 为止 |
| `docs/ayastorm-stream3d-roadmap.md` | 3D Stream 整体路线图 |

---

## 修订历史

- **2026-05-05 (初版)**: 作为 r10 时点的最终规格整理。汇总记述 r5 / r8 / r9 / r10 / r10.x 的累积规格。r11 及之后未发布所以不包含。

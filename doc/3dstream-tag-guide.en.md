# 3D Stream Tag Format Guide

> Tag format reference for AYAstorm's **3D Stream** feature, which plays HTTP audio streams as 3D-positional audio from prims.
>
> This document reflects the final specification as of AYAstorm `r10`. It will be updated as features are added in r11 / r12 and beyond.

---

## Table of Contents

1. [What is 3D Stream?](#1-what-is-3d-stream)
2. [Quick Start](#2-quick-start)
3. [Terminology](#3-terminology)
4. [Tag Overview](#4-tag-overview)
5. [Mono Tag `[3dstream:...]`](#5-mono-tag-3dstream)
6. [Distributed Stereo / Venue Placement Tag `[3dstream-stereo:...]`](#6-distributed-stereo--venue-placement-tag-3dstream-stereo)
7. [`ch` (Channel) Value Reference](#7-ch-channel-value-reference)
8. [Source Channel Count × Tag Value Compatibility Matrix](#8-source-channel-count--tag-value-compatibility-matrix)
9. [Streaming Side (Building Source URLs)](#9-streaming-side-building-source-urls)
10. [Viewer-side Settings](#10-viewer-side-settings)
11. [Error Notifications / Diagnostics](#11-error-notifications--diagnostics)
12. [Troubleshooting](#12-troubleshooting)
13. [Known Limitations / Specification Notes](#13-known-limitations--specification-notes)
14. [Related Documents / Internal Specifications](#14-related-documents--internal-specifications)

---

## 1. What is 3D Stream?

The standard Second Life Viewer plays HTTP audio streams (SHOUTcast / Icecast) as **parcel-level BGM** in 2D only — it has no information about where in 3D space the sound is coming from.

AYAstorm's **3D Stream** feature treats prims (objects) as "speakers" and plays the stream **as if the audio is emitted from that prim's location** with full 3D positional rendering. As the listener (camera or avatar) moves, sound direction and distance attenuation update in real time.

Primary use cases:

- **Live venue PA**: Place speaker prims in front of a stage so that the broadcast audio plays from those positions
- **Ambient sound**: Play matching audio from objects like rivers, jukeboxes, TVs
- **Stereo placement / multi-speaker venues**: Assign L / R / mono to multiple prims to spread stereo across space
- **5.1ch source venue deployment**: Place the six 5.1ch channels (FL / FR / C / LFE / SL / SR) on six prims

Everything is configured by **writing a tag into a prim's Description field** — no LSL script, no SL server-side change. Only AYAstorm users hear the 3D audio. Other Viewers (mainline Firestorm, official LL Viewer, etc.) ignore these tags, so there is no compatibility problem.

---

## 2. Quick Start

### 2.1 Single prim playing audio (simplest example)

Create any prim and write the following into its **Description field**:

```
[3dstream:{url:http://example.com/stream.mp3}]
```

That's all you need. The stream `http://example.com/stream.mp3` will be played as 3D-positional audio from that prim's location. Move away and the volume drops; move sideways and the panning naturally follows.

### 2.2 Stereo placement (split L / R into separate prims)

Split a stereo source between two prims to spread stereo across space.

1. Link a root prim and a child prim (Ctrl+L)
2. **Root Description**:
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]
   ```
3. **Add to root Description** (root itself becomes the L speaker):
   ```
   [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}{ch:L}]
   ```
4. **Child Description**:
   ```
   [3dstream-stereo:{ch:R}]
   ```

Now the root plays L and the child plays R.

### 2.3 More details

Read sections 3 onward. Multi-speaker, 5.1ch, fine-tuning, and broadcaster-side recipes are explained there.

---

## 3. Terminology

| Term | Meaning |
|---|---|
| **Stream** | Audio data delivered over HTTP (SHOUTcast / Icecast / static file, etc.). Main supported codecs: MP3 / Vorbis / Opus / FLAC |
| **Linkset** | A group of prims linked together via SL's "link" operation (Ctrl+L). One root + N child prims |
| **Root prim** | The parent prim of a linkset. Selected first when "Edit linked" is OFF in Build → Edit |
| **Child prim** | Any prim in the linkset other than the root |
| **Source declaration** | A prim with a tag containing `{url:...}`. Declares "which stream to play". **Only valid on the root prim** (ignored on child prims) |
| **Speaker prim** | A prim with a tag containing `{ch:...}`. Actually emits sound. **Can be either root or child** |
| **binding** | The internal "source → speaker group" mapping built per linkset. 1 linkset = 1 binding |
| **ch (channel)** | The audio channel a speaker prim is responsible for. `L` / `R` / `M` (mono), or 5.1ch values `FL` / `FR` / `C` / `LFE` / `SL` / `SR` |
| **rolloff** | Distance attenuation. Volume decreases as the listener moves away from the speaker |

---

## 4. Tag Overview

### 4.1 Two tag types

| Tag | Prefix | Purpose |
|---|---|---|
| **Mono tag** | `[3dstream:...]` | Play one stream from a single prim (minimal config) |
| **Distributed stereo / venue tag** | `[3dstream-stereo:...]` | Synchronize one stream across multiple prims of a linkset (stereo / multi-speaker / 5.1ch) |

### 4.2 Aliases for legacy prefixes

Both tags also accept legacy prefixes (`[ayastream:...]` / `[ayastream-stereo:...]`) as **permanent aliases**. When `ayastream` was renamed to `3dstream` in r5 (2026-05), these aliases were kept so that prims placed before the rename do not need to be re-edited. **`3dstream` is recommended for new content**, but mixing is fine.

```
[3dstream:{url:...}]              ← recommended (canonical)
[ayastream:{url:...}]             ← legacy, accepted

[3dstream-stereo:{url:...}{ch:L}] ← recommended (canonical)
[ayastream-stereo:{ch:L}]         ← legacy, accepted
```

### 4.3 Common syntax rules

- **The tag may appear anywhere in the Description.** Surrounding text is ignored (e.g., `Shop name [3dstream:{url:...}] open` mixed with descriptive text is fine).
- Fields are a set of `{key:value}` items. No separator is required between fields (whitespace, commas, or no separator all work).
- **Key names are case-insensitive** (normalized to lowercase internally). `{URL:...}` and `{url:...}` are equivalent.
- **Whitespace around values is trimmed.** `{ url : http://example/  }` is fine.
- **Unknown keys are silently ignored.** For example, `{foo:bar}` has no effect but does not produce an error.
- If multiple tags of the same kind appear in one prim's Description, **only the first one is used.**

### 4.4 SL Description limit (127 bytes)

The Description writable via LSL `llSetObjectDesc` has a **127-byte limit**. UTF-8 multibyte characters (e.g., Japanese) consume this quickly, so for long URLs either **shorten the URL** or use the distributed pattern (write `{url}` only on the root and `{ch:...}` only on child prims; see §6).

### 4.5 Tag activation timing

- AYAstorm **polls in-range prim Descriptions every 30 seconds** (`Stream3DPollInterval` setting).
- When you modify Description via LSL `llSetObjectDesc`, the next poll re-evaluates and applies the change (typically within 5–30 seconds).
- When you manually right-click → Edit → Description, the edit triggers immediate re-evaluation (via Properties broadcast).
- Linking / unlinking also triggers re-evaluation.

---

## 5. Mono Tag `[3dstream:...]`

### 5.1 Syntax

```
[3dstream:{url:URL}{min:N}{max:N}]
```

Or with the legacy prefix:

```
[ayastream:{url:URL}{min:N}{max:N}]
```

### 5.2 Key reference

| Key | Required | Type | Default | Meaning |
|---|---|---|---|---|
| `url` | **required** | string | — | Stream URL (`http://` / `https://`). Empty string is an error |
| `min` | optional | F32 (m) | `Stream3DRolloffMin` (1.0) | **Near distance** for rolloff (volume = 100% within this distance) |
| `max` | optional | F32 (m) | `Stream3DRolloffMax` (20.0) | **Far distance** for rolloff (volume = 0% beyond this distance) |

The rolloff model is FMOD's `FMOD_3D_LINEARSQUAREROLLOFF` (linear-square rolloff). It attenuates smoothly between `min` and `max`.

### 5.3 Behavior

- The tag may be written on **any prim** of the linkset (root or child). The prim with the tag itself acts as the speaker.
- A stereo source is **internally mixed down to mono** (L+R average).
- If the same linkset also has `[3dstream-stereo:...]`, the mono tag is NOT given priority for that prim's speaker assignment — the two binding paths are evaluated independently. Using the same prim for both is not recommended (behavior undefined).

### 5.4 Examples

#### 5.4.1 Minimum

```
[3dstream:{url:http://example.com/radio.mp3}]
```

`min` / `max` are omitted, so the setting defaults (1m / 20m) are used.

#### 5.4.2 Custom distance

```
[3dstream:{url:http://example.com/radio.mp3}{min:2}{max:50}]
```

Volume is at maximum within 2m and fades to silence at 50m. Use this for large outdoor fields where you want sound to carry.

#### 5.4.3 Mixed with descriptive text

```
Shop BGM [3dstream:{url:http://radio.example.jp/8000/jazz}] enjoy
```

Surrounding text is fine.

---

## 6. Distributed Stereo / Venue Placement Tag `[3dstream-stereo:...]`

### 6.1 Syntax

```
[3dstream-stereo:{url:URL}{range:N}{ch:CH}{volume:V}]
```

Or with the legacy prefix:

```
[ayastream-stereo:...]
```

This tag handles **one stream across the entire linkset**. The root prim declares "which stream to play", and each prim in the linkset declares "which channel I'm responsible for".

### 6.2 Per-prim role

Each prim takes on a role based on its tag fields:

| Description fields | Role |
|---|---|
| Contains `{url:...}` | **Source declaration** (root only — `{url}` on a child prim is ignored) |
| Contains `{ch:...}` | **Speaker** (root or child, both fine) |
| Contains both (= root only) | Source declaration + also acts as speaker |
| Contains neither | Does nothing (not part of the binding) |

Playback only starts when the linkset has both **a source declaration (root with `{url}`)** and **at least one speaker (a prim with `{ch}`)**. If there are zero speakers, a "structural error" is raised and a notification is shown (§11).

### 6.3 Key reference

#### 6.3.1 Keys meaningful only on the root prim

| Key | Required | Type | Default | Meaning |
|---|---|---|---|---|
| `url` | **required** | string | — | Stream URL. Empty string is an error |
| `range` | optional | F32 (m) | `Stream3DRolloffMax` (20.0) | Default rolloff distance for speakers in the linkset that don't have their own `range` |

#### 6.3.2 Speaker declaration keys (any prim)

| Key | Required | Type | Default | Meaning |
|---|---|---|---|---|
| `ch` | **required** | enum | — | Channel this prim handles (see §7) |
| `range` | optional | F32 (m) | Falls back: speaker `range` → root `range` → `Stream3DRolloffMax` | Per-speaker rolloff distance |
| `volume` | optional | F32 [0.0–1.0] | 1.0 | Per-speaker volume multiplier |

> **Important**: The `min` / `max` keys from the mono tag are **ignored** in the distributed-stereo tag. For distributed stereo the near distance is internally fixed at 1.0m, and the far distance is the `range` key (or default `Stream3DRolloffMax`).

### 6.4 One root + one child (basic stereo pair)

Smallest stereo placement:

```
Root Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:L}]

Child Description:
  [3dstream-stereo:{ch:R}]
```

The root takes L, the child takes R. The **link order (link number)** of root vs child does NOT affect playback. Spatial positioning is determined by where in space you place each prim.

### 6.5 Multi-speaker (4+ speakers)

The same stereo stream played from four corners of a venue:

```
Root Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{range:30}]

Child #1 Description:
  [3dstream-stereo:{ch:L}{range:50}]

Child #2 Description:
  [3dstream-stereo:{ch:R}{range:50}]

Child #3 Description:
  [3dstream-stereo:{ch:L}{volume:0.7}]

Child #4 Description:
  [3dstream-stereo:{ch:R}{volume:0.7}]
```

- Root only declares the source; it does not play (no `{ch}`)
- Children #1, #2 carry L/R at 50m range, full volume
- Children #3, #4 carry the same L/R at 70% volume (front-row support speakers)
- The speaker count is capped at `Stream3DStereoMaxSpeakers` (**default 16**, see §10)

### 6.6 5.1ch venue placement (6 prims)

Deploy a 5.1ch source (Opus surround / FLAC 6ch) across six speaker prims:

```
Root Description:
  [3dstream-stereo:{url:http://example.com/test_5_1.flac}{range:30}]

FL prim:  [3dstream-stereo:{ch:FL}]
FR prim:  [3dstream-stereo:{ch:FR}]
C prim:   [3dstream-stereo:{ch:C}]
LFE prim: [3dstream-stereo:{ch:LFE}]
SL prim:  [3dstream-stereo:{ch:SL}]
SR prim:  [3dstream-stereo:{ch:SR}]
```

- Place each prim physically in the venue's "speaker positions" (front L/R of the stage, center, subwoofer, surround L/R)
- LFE is treated equivalently to the other 5 channels — no special processing (no low-pass filter, no 2D-ization). If you need LFE band-limiting, do it on the broadcaster side.
- The listener has no "movie sweet spot" (= SL is free-camera). If they walk around the venue, the intended 5.1 image will of course break. Treat this as **multi-point venue PA, not cinema-style surround reproduction**.

### 6.7 Assigning the same `ch` to multiple prims

If you write `{ch:L}` on two or more prims, both will play the L channel. Useful for putting "front-row L" and "back-row L" speakers in a venue.

Conversely, if no `ch` is written on any prim, you'll get a "0 speakers" structural error.

### 6.8 Root acts as both source and speaker

```
Root Description:
  [3dstream-stereo:{url:http://example.com/stream.mp3}{ch:M}{range:25}]
```

By writing both `{url}` and `{ch}` in the root tag, the root declares the source AND acts as an M (mono) speaker. This works for simple mono setups with no child prims (functionally close to `[3dstream:...]`).

### 6.9 How to identify the root prim

While editing a linkset, in the Build floater's **Object** tab the "Selected" indicator shows which prim is selected. The parent of the linkset (= root) is normally the **first prim selected when the linkset was originally linked**.

Most reliable confirmation:
- Build → Edit → "Edit linked" OFF → click any prim → the root of that linkset is selected
- LSL: `llGetLinkNumber()` returns `1` for the root (when child prims exist). For a single un-linked prim, it returns `0`.

The link order (link number 1, 2, 3, ...) of root vs children **does NOT affect 3D Stream playback**. The spec from r5 that used link number to determine L/R was retired in r8; from r8 onwards it's `{ch:...}` declaration based.

---

## 7. `ch` (Channel) Value Reference

`{ch:value}` accepts the following 9 values. **Case is not significant** (`{ch:l}` and `{ch:L}` are the same).

| Value | Meaning | Primary use |
|---|---|---|
| `L` | Left channel | Stereo L |
| `R` | Right channel | Stereo R |
| `M` | Mono (average of L+R) | "Center speaker" use, or single-point playback |
| `FL` | Front Left | 5.1ch front left |
| `FR` | Front Right | 5.1ch front right |
| `C` | Center | 5.1ch center |
| `LFE` | Low Frequency Effects (subwoofer) | 5.1ch low-frequency |
| `SL` | Surround Left | 5.1ch surround left |
| `SR` | Surround Right | 5.1ch surround right |

When the source's actual channel count and the `ch` value don't match, the system performs an **automatic fallback** rather than reporting a mismatch (§8). For example, writing `{ch:FL}` on a stereo (2ch) source plays L.

Invalid values (e.g., `{ch:foo}`) raise a **format error** notification (§11).

---

## 8. Source Channel Count × Tag Value Compatibility Matrix

What a speaker prim actually plays is determined by the combination of the **source URL's channel count** and the **`ch` value you wrote**.

### 8.1 Compatibility matrix

| Source | `{ch:L}` | `{ch:R}` | `{ch:M}` | `{ch:FL}` | `{ch:FR}` | `{ch:C}` | `{ch:LFE}` | `{ch:SL}` | `{ch:SR}` |
|---|---|---|---|---|---|---|---|---|---|
| **1ch (mono)** | M | M | M | M | M | M | silent | silent | silent |
| **2ch (stereo)** | L | R | (L+R)/2 | L | R | (L+R)/2 | silent | silent | silent |
| **6ch (5.1)** | BS.775 L | BS.775 R | (BS.775 L + R)/2 | FL | FR | C | LFE | SL | SR |

Legend:
- `L` / `R` / `FL` / `FR` / `C` / `LFE` / `SL` / `SR` = direct playback of the corresponding source channel
- `BS.775 L` = the value computed by ITU-R BS.775 downmix coefficients, folding 6ch into stereo L/R (see §8.2)
- `silent` = that speaker emits no sound (the binding is preserved, but no audio comes from the prim)

### 8.2 BS.775 downmix coefficients (6ch source → L/R)

```
L_out = c × ( FL + 0.707·C + 0.707·SL + 0.5·LFE )
R_out = c × ( FR + 0.707·C + 0.707·SR + 0.5·LFE )
c = 1 / 2.914 ≈ 0.343 (normalization for clipping prevention)
```

Center is split equally L/R, surround is summed to its same side, LFE is mixed into both sides equally.

### 8.3 Mixing both placement styles for the same source

If you assign a 5.1ch source to both `{ch:L}` and `{ch:FL}`, the L prim plays the BS.775 downmix while the FL prim plays direct. This is confusing, so the recommendation is to **stick to one channel family per source — either `L/R/M` or `FL/FR/...` — across a venue**.

If a fallback occurs in mixed placements, the **routing diagnostic chat notification** (§10.3 / §11.3) lets you confirm what each ch is actually playing. Turning this ON during 5.1ch venue construction makes mistakes immediately visible.

### 8.4 5.1ch venue placement playing a 2ch / 1ch source

Suppose you have a 5.1ch venue with six speaker prims (`ch:FL` / `FR` / `C` / `LFE` / `SL` / `SR`) already deployed, and you switch the source URL from a 5.1ch broadcast to a **regular stereo (2ch) broadcast** or **mono (1ch) broadcast**. For example: "5.1ch during the live show, regular stereo BGM during breaks", or "MC mono voice between DJ sets".

In this case, **no rearrangement or settings change is needed**. Each speaker prim automatically behaves as follows.

#### When a 2ch (stereo) source plays

| Prim's `ch` value | What plays |
|---|---|
| `{ch:FL}` | **L** (instead of front-left, plays stereo L direct) |
| `{ch:FR}` | **R** (instead of front-right, plays stereo R direct) |
| `{ch:C}` | **(L+R)/2** (center plays mono downmix of L+R) |
| `{ch:LFE}` | **silent** (no LFE signal in source) |
| `{ch:SL}` | **silent** (no surround-left signal in source) |
| `{ch:SR}` | **silent** (no surround-right signal in source) |

Audibly: "**The three front speakers (FL / FR / C) play stereo, while the three surround speakers (LFE / SL / SR) go silent**".

#### When a 1ch (mono) source plays

| Prim's `ch` value | What plays |
|---|---|
| `{ch:FL}` / `{ch:FR}` / `{ch:C}` | **M** (all three front speakers play mono; same audio from three locations) |
| `{ch:LFE}` / `{ch:SL}` / `{ch:SR}` | **silent** |

#### Design intent / notes

- **The three front speakers (FL / FR / C) always play something regardless of source channel count** — so when switching 5.1ch ↔ 2ch ↔ 1ch, the front of the venue never goes silent.
- **LFE / SL / SR stay silent when the source has no corresponding signal** — no fake bass or fake surround is synthesized.
- **When the source returns to 5.1ch, each prim automatically reverts to direct channel playback** (re-evaluated on URL-switch reconnect). No rearrangement, no settings change.
- Running 2ch BGM through a 5.1ch venue placement is a **legitimate operational pattern**. "Surround speakers go silent" is by design, not a bug.

#### Show fallback details in chat (recommended during construction / verification)

A diagnostic switch is provided to confirm in **Local Chat** whether a silent prim is silent due to fallback specification or due to something being broken.

**Setting location** (both control the same value and are synchronized):

- **Preferences > Sound > Show channel routing diagnostics in chat** (checkbox)
- **Debug Settings: `Stream3DRoutingDiagnostic`** (`true` / `false`)

When ON, fallback events for 5.1 placement × 2ch / 1ch source produce lines in **Local Chat as messages from yourself** in the form below (`3D Stream:` prefix added by the §11.3 helper):

**For a 2ch source × 5.1 placement (six prims FL/FR/C/LFE/SL/SR)**:

```
[12:34] You: 3D Stream: ch:FL prim playing L (source is 2ch)
[12:34] You: 3D Stream: ch:FR prim playing R (source is 2ch)
[12:34] You: 3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)
[12:34] You: 3D Stream: ch:LFE prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SL prim silent (source is 2ch)
[12:34] You: 3D Stream: ch:SR prim silent (source is 2ch)
```

**For a 1ch source × 5.1 placement**:

```
[12:35] You: 3D Stream: ch:FL prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:FR prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:C prim playing M (source is 1ch)
[12:35] You: 3D Stream: ch:LFE prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SL prim silent (source is 1ch)
[12:35] You: 3D Stream: ch:SR prim silent (source is 1ch)
```

This makes "LFE / SL / SR are silent by spec, FL / FR / C are operating in fallback" obvious at a glance.

Notifications are throttled with a key of `(root_id, url, observed_channel_count, prim_set_signature)`, so the same notification is not repeated until the placement or source channel count changes. **Recommended: turn ON only during 5.1ch venue construction / verification; OFF (`false`, the default) for production**. See §11.3 for details.

#### Reverse direction: 2ch placement playing a 5.1ch source

For reference, the opposite direction. When a stereo venue (= 2ch placement using only `{ch:L}` / `{ch:R}` / `{ch:M}`) plays a 5.1ch source:

- L / R / M prims play the **BS.775 downmix** (§8.2), folding 6ch into 2ch. FL / C / SL / LFE all sum into L with their coefficients; FR / C / SR / LFE all sum into R.
- All channel signals are audible via L / R, so **no channels are dropped to silence**.
- Local Chat output when `Stream3DRoutingDiagnostic` is ON:

```
[12:36] You: 3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)
[12:36] You: 3D Stream: C content folded into BS.775 downmix (source is 6ch, no ch:C prim)
[12:36] You: 3D Stream: LFE content folded into BS.775 downmix (source is 6ch, no ch:LFE prim)
[12:36] You: 3D Stream: SL content folded into BS.775 downmix (source is 6ch, no ch:SL prim)
[12:36] You: 3D Stream: SR content folded into BS.775 downmix (source is 6ch, no ch:SR prim)
```

You're notified per-channel that "no dedicated prim, so folded into BS.775 downmix path".

---

## 9. Streaming Side (Building Source URLs)

### 9.1 Supported codecs / containers

| Codec / container | 1ch | 2ch | 6ch | Notes |
|---|---|---|---|---|
| **MP3** | ✓ | ✓ | — | Traditional SHOUTcast / Icecast path |
| **Vorbis (Ogg)** | ✓ | ✓ | ✓ | 6ch end-to-end verified (r9 P10) |
| **Opus (Ogg)** | ✓ | ✓ | △ | 6ch uses Opus channel mapping family 1. **Plain HTTP / Icecast push may fail to open due to seek failure** (§9.4) |
| **FLAC** | ✓ | ✓ | △ | 6ch supported in theory; same seek limitation as Opus |
| AAC (ADTS / HLS) | — | — | — | Not supported |
| AC-3 / E-AC-3 | — | — | — | Not supported (Dolby licensing) |

Source URLs may be `http://` or `https://`. A path that maintains HTTP/1.1 keep-alive (= a SHOUTcast-compatible streamer or ffmpeg's TCP output) tends to be more stable than plain static HTTP.

### 9.2 1ch / 2ch streaming

Standard SHOUTcast / Icecast / static HTTP works fine. MP3 / Vorbis / Opus / FLAC all play without issues. Tools like `oggenc`, ffmpeg, or butt work as-is.

### 9.3 5.1ch (Vorbis 6ch) streaming

The recommended path for reliable viewer-side 5.1ch playback is **Vorbis 6ch** (verified end-to-end in r9 P10).

#### 9.3.1 Test material (ffmpeg)

```bash
# 5.1 WAV with a unique frequency per ch (10 sec)
ffmpeg -f lavfi -i "sine=440:d=10" -f lavfi -i "sine=550:d=10" \
       -f lavfi -i "sine=660:d=10" -f lavfi -i "sine=110:d=10" \
       -f lavfi -i "sine=770:d=10" -f lavfi -i "sine=880:d=10" \
       -filter_complex "[0:a][1:a][2:a][3:a][4:a][5:a]amerge=inputs=6[a]" \
       -map "[a]" -ac 6 -channel_layout 5.1 test_5_1.wav

# Encode to Vorbis 6ch
ffmpeg -i test_5_1.wav -c:a libvorbis -q:a 5 test_5_1.ogg
```

#### 9.3.2 Static HTTP serving (for verification)

```bash
python3 -m http.server 8080
```

URL: `http://<host>:8080/test_5_1.ogg`

#### 9.3.3 Real-time streaming (ffmpeg → Icecast)

```bash
ffmpeg -re -i test_5_1.wav \
  -c:a libvorbis -q:a 5 \
  -ac 6 -ar 48000 \
  -content_type audio/ogg \
  -f ogg icecast://source:hackme@localhost:8000/aya_5_1.ogg
```

Key options:
- `-re` = real-time (stream at source duration; simulates live broadcast)
- `-content_type audio/ogg` = declare the MIME to Icecast (otherwise it may misidentify as MP3)
- `-ac 6 -ar 48000` = preserve 6ch 48kHz

### 9.4 Opus 6ch / FLAC 6ch limitations

When Opus 6ch (channel mapping family 1) or FLAC 6ch is delivered via **plain HTTP** (e.g., `python3 -m http.server`) or **Icecast push**, the FMOD parser may issue a **seek request** that fails with `FMOD_ERR_FILE_COULDNOTSEEK`, leaving the stream un-openable.

Workarounds:

- Use a **SHOUTcast-compatible streamer** (keep-alive + range support)
- Route through **ffmpeg primary** (TCP backpressure resolves it)
- Use the **5.1ch GUI broadcast tool `butt-aya`** (separate AYA project, unreleased as of writing) to push

To be certain it will work, **Vorbis 6ch is the shortest reliable path right now**.

### 9.5 Choosing a broadcast tool

| Tool | Use case | Notes |
|---|---|---|
| **ffmpeg** | Any codec / any ch / static / real-time | CLI; most flexible |
| **butt** (official) | DJ broadcasting | 1ch / 2ch only; no 5.1ch |
| **butt-aya** (5.1ch fork) | 5.1ch GUI broadcasting | Separate AYA project, unreleased as of writing |
| **Liquidsoap** | Advanced broadcast automation | High config complexity, advanced users |
| **Mixxx / DarkIce / ezstream** | DJ / automation | Stereo-oriented, no 5.1ch |

---

## 10. Viewer-side Settings

### 10.1 Via Preferences

The **Preferences → Sound** tab has these controls:

- **3D Stream** slider — master volume multiplier for all streams (`Stream3DVolumeMaster`)
- **Enabled** checkbox — overall feature ON/OFF (`Stream3DEnabled`)
- **Show channel routing diagnostics in chat** — routing diagnostic notifications (`Stream3DRoutingDiagnostic`, see §11.3)
- **Hear media and sounds from:** — listener position selector, Camera or Avatar (`MediaSoundsEarLocation`, see §13.1)

The same "3D Stream" slider also appears in the speaker icon's Volume dropdown for quick volume adjustment near the voice chat controls.

### 10.2 Debug Settings (advanced tuning)

`Ctrl + Alt + D` opens the Advanced menu → Show Debug Settings to edit any key directly.

| Key | Type | Default | Meaning |
|---|---|---|---|
| `Stream3DEnabled` | bool | `true` | Master kill switch. Setting `false` immediately tears down all bindings; re-enabling does not auto re-bind (it gets re-discovered on the next poll) |
| `Stream3DDescriptionScan` | bool | `true` | When `false`, Description tag scanning is suspended and all prim bindings are released (debug streams unaffected) |
| `Stream3DMaxConcurrent` | S32 | `4` | Max concurrent bindings (mono + stereo combined). 0 = unlimited |
| `Stream3DStereoMaxSpeakers` | S32 | `16` | Max speakers per linkset. Excess is dropped from the tail of the traversal with a warning notification |
| `Stream3DRolloffMin` | F32 (m) | `1.0` | Mono tag default near distance (when `{min}` is omitted) |
| `Stream3DRolloffMax` | F32 (m) | `20.0` | Default far distance (shared fallback when mono `{max}` or stereo `{range}` is omitted) |
| `Stream3DMaxDistance` | F32 (m) | `64.0` | Prim discovery polling radius. Set ≥ `Stream3DRolloffMax` |
| `Stream3DPollInterval` | F32 (sec) | `30.0` | Description polling interval. Affects how quickly LSL-mediated tag changes are picked up. 0 disables active polling |
| `Stream3DVolumeMaster` | F32 [0–1] | `0.5` | Master volume multiplier. Same as the 3D Stream slider in Preferences |
| `Stream3DReconnectAttempts` | S32 | `3` | Auto-reconnect attempts on stream disconnect. Each retry waits 5 seconds. 0 disables reconnect |
| `Stream3DRoutingDiagnostic` | bool | `false` | Routing-diagnostic chat notifications ON/OFF (see §11.3). Synchronized with the Preferences checkbox |

#### Debug-only (for development verification)

| Key | Type | Use |
|---|---|---|
| `Stream3DDebugUrl` | string | Debug-target URL |
| `Stream3DDebugPlay` | bool | When `true`, places & plays a mono stream 5m in front of the avatar (quick test, no tag editing required) |
| `Stream3DDebugStereoPlay` | bool | Same for the stereo version |

### 10.3 Persistence and immediate apply

Most settings are **"Live"** — applied from the next frame after the value changes. No viewer restart needed. Exceptions:

- Toggling `Stream3DEnabled` from `false` to `true` does NOT auto re-bind. Re-discovery happens on the next poll cycle (within ~30s by default).
- Toggling `Stream3DDescriptionScan` immediately tears down or re-discovers all bindings.

---

## 11. Error Notifications / Diagnostics

### 11.1 How notifications appear

Tag format errors and structural errors are **shown in Local Chat** as system messages, prefixed with "3D Stream:".

```
3D Stream: Tag format error (object name: "MySpeaker")
  ch must be one of L/R/M/FL/FR/C/LFE/SL/SR.
  Example: [3dstream-stereo:{ch:L}{range:30}]
```

```
3D Stream: Structural error (linkset root: "MainStage")
  Source declaration (url) found on root, but no speakers (ch) found.
  Each speaker prim should have [3dstream-stereo:{ch:L|R|M}].
```

### 11.2 30-second suppression

The same prim × the same error kind is **suppressed for 30 seconds**, to avoid chat flooding while you edit tags. After 30s, the notification fires once again.

Suppressed notifications still go to the LL_DEBUGS log (debug log channel), so you can confirm what's happening internally.

### 11.3 Routing diagnostics (5.1ch placement)

A diagnostic feature for **construction / verification** of 5.1ch / multi-speaker venues, letting you confirm in Local Chat "what each prim is playing / why a prim is silent". **Default OFF** — does not emit unless explicitly enabled.

#### 11.3.1 How to enable

Either of the following enables it (they are synchronized):

- **Preferences > Sound > Show channel routing diagnostics in chat** — check the box
- **Debug Settings: set `Stream3DRoutingDiagnostic` to `true`** (Advanced > Show Debug Settings)

Applied immediately. No viewer restart needed.

#### 11.3.2 Output destination and format

When ON, fallback events emit one line per occurrence in **Local Chat as messages from yourself**, in this form:

```
[HH:MM] You: 3D Stream: <content>
```

The `3D Stream:` prefix is added automatically by the `notifyStream3D` helper. They are stored in the chat log (`Show in Chat`) so you can review them after the test. **They are NOT visible to anyone else** (these are pseudo-messages displayed only in your own Local Chat).

#### 11.3.3 Notification text reference

| Situation | Local Chat line |
|---|---|
| 6ch source × `ch:L/R/M` prim only (no dedicated ch prim) | `3D Stream: FL content folded into BS.775 downmix (source is 6ch, no ch:FL prim)` (one line per FL/FR/C/LFE/SL/SR) |
| 6ch source × neither dedicated nor `ch:L/R/M` prim | `3D Stream: FL content has no destination — dropped (source is 6ch, no ch:L/R/M prim)` |
| 2ch source × `ch:FL` prim | `3D Stream: ch:FL prim playing L (source is 2ch)` |
| 2ch source × `ch:FR` prim | `3D Stream: ch:FR prim playing R (source is 2ch)` |
| 2ch source × `ch:C` prim | `3D Stream: ch:C prim playing (L+R)/2 (source is 2ch)` |
| 1ch source × `ch:FL/FR/C` prim | `3D Stream: ch:FL prim playing M (source is 1ch)` (FR / C use the same form) |
| 1ch / 2ch source × `ch:LFE/SL/SR` prim | `3D Stream: ch:LFE prim silent (source is 2ch)` (or `1ch`; SL / SR use the same form) |

For concrete sample output for "5.1 placement × 2ch source", see "Show fallback details in chat" in §8.4.

#### 11.3.4 Throttle and re-display conditions

Notifications are throttled with a key of `(root_id, url, observed_channel_count, prim_set_signature)`. While the same venue and the same source configuration are in effect, **lines do not repeat** (to avoid flooding the chat). Re-evaluation and re-emission happen when any of the following changes:

- Source URL changes (= switched to a different stream)
- Source channel count changes (= same URL but it switched 5.1ch ↔ 2ch)
- Speaker prim configuration changes (prim added / removed / `ch` value changed)
- Right after toggling `Stream3DRoutingDiagnostic` from OFF to ON

#### 11.3.5 Operational recommendation

- **Turn ON during venue construction and placement verification** to confirm fallback behavior of each prim in Local Chat
- **Turn OFF for production (live shows etc.)** to keep the chat clean
- The default (OFF) is the production-intended state. Manually flip to ON only during placement verification.

### 11.4 Logs (`LL_INFOS("Stream3D")`)

Detailed runtime logs are recorded in AYAstorm's log file:

```
~/.ayastorm_x64/logs/AYAstorm.log
```

Grep for the `Stream3D` channel to see binding establishment / teardown / reconnect / source format detection / dropouts.

---

## 12. Troubleshooting

### 12.1 Tag was written but no sound plays

Check in order:

1. **Description was actually updated**: Right-click the prim → Edit → Description tab to confirm the current value
2. **Tag spelling**: Confirm the prefix is exactly `[3dstream:` or `[3dstream-stereo:` (typos)
3. **`{url:...}` scheme is http/https**: `file://` and relative URLs are not allowed
4. **`Stream3DEnabled` / `Stream3DDescriptionScan` are both true**: Confirm in Preferences > Sound or Debug Settings
5. **Wait for poll**: Changes via LSL `llSetObjectDesc` take up to 30 seconds (`Stream3DPollInterval`)
6. **Look for an error notification in chat**: See §11
7. **Look in logs for `LL_INFOS("Stream3D")` reconnect attempts**: Stream URL may be down

### 12.2 Only one stereo channel plays

- If the root has only `{url}` and the child only `{ch:R}`, no prim handles L → "L missing" state. Add `{ch:L}` to the root, or assign `{ch:L}` to another prim.
- If you write `{ch:L}` to two prims on purpose to double-up the L speaker, that's intended and fine.
- Turn ON the routing diagnostic (`Stream3DRoutingDiagnostic`) to see what each ch is actually playing, in chat.

### 12.3 5.1ch source won't open / audio glitches

- §9.4 seek limitation: common with Opus 6ch / FLAC 6ch over plain HTTP / Icecast push. **Switch to Vorbis 6ch**, or route via a SHOUTcast-compatible streamer / ffmpeg primary.
- The first 5–10 seconds after an HTTP switch may produce dropout warnings while the prebuffer fills (LAN: ~408–2045 frames/spk/s ≈ 0.8–4%). They subside in steady state.
- Bitrate too high / network congested → dropouts: lower bitrate on broadcaster (≤ 256kbps recommended) / reduce concurrent bindings.

### 12.4 Tag on a child prim but it's not recognized as a speaker

- Child-prim Description is fetched via Properties messaging. The **first time you enter the linkset's region it can take a few seconds (up to ~10s)**.
- After modifying child Description via LSL `llSetObjectDesc`, it applies on the next poll cycle (within 30s).
- Confirm `{ch:...}` is not a typo (case-insensitive, but spelling errors are invalid).

### 12.5 Listener position seems wrong (sound direction is off)

- When you camera-flick around, listener position follows camera, so the localization changes. To lock to avatar, set Preferences > Sound **"Hear media and sounds from:" to Avatar** (`MediaSoundsEarLocation = 1`).
- This Camera/Avatar selector applies to 3D Stream as well (shared with parcel BGM, LSL `llPlaySound`, etc.).

### 12.6 Want to play multiple 3D Streams at once

- New bindings are rejected once `Stream3DMaxConcurrent` (default 4) is reached. Increase the value if you need more concurrent streams (8 / 16 are practical).
- Note that 1 binding = 1 decoder thread + N speaker channels of CPU cost. 20 concurrent will stress CPU; bump only as needed.

### 12.7 Removed the tag but sound continues

- Re-evaluation may not have triggered. Move the prim once, or look around the area to wait for the next poll.
- If still stuck, toggle `Stream3DEnabled` to false (force-release all bindings) then back to true (re-discover).

---

## 13. Known Limitations / Specification Notes

### 13.1 Listener position is camera or avatar

The listener position used for 3D Stream's spatialization follows Preferences > Sound **"Hear media and sounds from:"**:

- `Camera` (default): camera position / orientation
- `Avatar`: avatar position / orientation

This is the same setting used by LSL `llPlaySound`, parcel BGM, and Media-on-a-Prim.

### 13.2 1 linkset = 1 stream

What matters per linkset is whether a root with `{url}` "exists" or "doesn't exist". **Multiple `{url}` declarations in one linkset are not allowed** (`{url}` on a child prim is ignored).

To run multiple distinct streams in one venue, split into separate linksets and place them — they coexist as separate bindings within `Stream3DMaxConcurrent`.

### 13.3 Description byte limit (127)

The Description writable via LSL `llSetObjectDesc` is **limited to 127 bytes**. URLs and descriptions that contain Japanese (or any UTF-8 multibyte) easily exceed this.

When it gets long:

- Use the distributed pattern: write `{url}` on root only, `{ch}` on children only (each prim's Description stays short)
- Shorten the URL (URL shortener, or a shorter path on the broadcaster side)

### 13.4 Verified behavior per codec

| Codec | 1ch / 2ch | 6ch |
|---|---|---|
| Vorbis (Ogg) | ✓ verified end-to-end | ✓ verified end-to-end (r9 P10, 12 minutes continuous, 0 dropout) |
| Opus (Ogg) | ✓ verified end-to-end | △ code review only (works on production paths; static HTTP / Icecast push fail at seek) |
| FLAC | ✓ verified end-to-end | △ code review only (same constraint as Opus) |
| MP3 | ✓ verified end-to-end | — |

For reliable 5.1ch playback, choose **Vorbis 6ch**.

### 13.5 No special LFE handling

5.1ch's LFE (subwoofer) is treated equivalently to the other 5 channels — no low-pass filter, no 2D-ization. It is 3D-positioned and distance-attenuated like the rest. If LFE band-limiting is needed, do it on the broadcaster mix.

The intended pattern is: place a physical subwoofer-shaped prim in SL at the appropriate position and have low-frequency audio emit from there.

### 13.6 5.1ch in a free-camera world

A real 5.1 system (cinema standard / ITU-R BS.775) assumes **the listener is in a fixed position** and bakes directional cues per channel. SL's listener is free-camera, so the "sweet spot" concept does not apply. The intent of this feature is **"multi-point reproduction of a 5.1 source in a venue"**, in the spirit of venue PA — not the reproduction of cinematic surround.

When the listener walks around the space, the intended 5.1 image will of course break, but the "venue feel / sense of sound covering an area" comes through clearly.

### 13.7 Behavior in other Viewers

`[3dstream:...]` / `[3dstream-stereo:...]` tags are **AYAstorm-specific**. Mainline Firestorm, official LL Viewer, Catznip, etc. ignore them entirely.

- AYAstorm users: 3D-positional audio plays as designed
- Other Viewer users: the tag just appears as text in the description, no audio plays (3D Stream is independent of parcel BGM, so any parcel BGM that's set up is still audible to them)

### 13.8 Concurrency caps

| Cap | Default |
|---|---|
| `Stream3DMaxConcurrent` (bindings per linkset) | 4 |
| `Stream3DStereoMaxSpeakers` (speakers per binding) | 16 |
| Resulting max concurrent speakers | 4 × 16 = 64 |

Up to ~64 channels stays within FMOD headroom. If you need more, raise via debug settings (verify CPU load on real hardware first).

### 13.9 Volume composition

Final volume = `Stream3DVolumeMaster` × `{volume:N}` × FMOD distance attenuation × Master Audio Slider × any mute states.

Typically use `Stream3DVolumeMaster` (the 3D Stream slider in Preferences) for global control, `{volume:N}` for per-prim correction, and `range` (per-speaker) or `Stream3DRolloffMax` (global default) for distance attenuation.

---

## 14. Related Documents / Internal Specifications

This guide is the **user-facing** format reference. Implementation details (decode thread / FMOD path / ring buffer / shutdown order, etc.) are in the following internal specs.

| Document | Content |
|---|---|
| `doc/spec_positional_stream_audio.md` | 3D Stream core spec (revised at r5) — base architecture |
| `doc/spec_stream3d_decode_thread.md` | The 3-thread model established in r7 |
| `doc/spec_distributed_stereo.md` | r8 distributed stereo spec — `[3dstream-stereo:...]` field syntax |
| `doc/spec_5_1ch_source.md` | r9 5.1ch source ingestion spec — Opus/FLAC 6ch decode path + BS.775 downmix |
| `doc/spec_5_1ch_placement.md` | r10 5.1ch venue placement spec — `ch=FL/FR/C/LFE/SL/SR` extension + compatibility matrix |
| `doc/spec_binaural_venue_reverb.md` | r11 (unreleased) binaural + venue reverb spec — out of scope for this guide (r10 only) |
| `docs/ayastorm-stream3d-roadmap.md` | Overall 3D Stream roadmap |

---

## Revision History

- **2026-05-05 (initial)**: Compiled as the final spec at r10. Cumulative spec from r5 / r8 / r9 / r10 / r10.x. r11 and later not covered (unreleased).

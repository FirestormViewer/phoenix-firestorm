# Markdown-Style Formatting Feasibility: `_italic_` / `**bold**`

Branch analyzed: `origin/master` @ `99f9ff1722`
Date: 2026-09-06

## Summary

Introducing markdown-style emphasis to the viewer's chat/IM text pipeline is
**feasible with low risk and no renderer changes**. Every layer of the required
infrastructure already exists in `llui`/`llrender`; the work is confined to a
delimiter scanner, a style-merge helper, an opt-in XUI parameter on
`LLTextBase`, and enabling it in the chat history editor.

## Existing Infrastructure

### Font variants are first-class

* `LLFontGL::StyleFlags` (`indra/llrender/llfontgl.h:66`) defines combinable
  `BOLD`, `ITALIC`, `UNDERLINE` bit flags.
* `indra/newview/skins/default/xui/en/fonts.xml` and **all 14 FS font packs**
  register `ITALIC` and `BOLD|ITALIC` variants alongside `BOLD` for every
  face.
* `LLFontRegistry::getFont(const LLFontDescriptor&)`
  (`indra/llrender/llfontregistry.h:146`) resolves any
  (name, size, style) combination; `LLFontGL::getStyleFromString()` /
  `getStringFromStyle()` convert between style strings and flags.

### Styles support per-span font switching

* `LLStyle::Params::font` is an `Optional<const LLFontGL*>` whose specialized
  `ParamValue` (`indra/llui/llui.h:424`, `indra/llui/llui.cpp:629`) exposes
  `name` / `size` / `style` sub-params. Assigning
  `params.font.style("ITALIC")` re-resolves the *same face and size* in italic
  via `updateValueFromBlock()`.
* Precedent exists in `indra/newview/fschathistory.cpp:1548`: whole-message
  italic for emotes, bold for shouts, and moderator name/body style
  overrides — but only at whole-message granularity today.

### Rendering needs zero work

* Each `LLNormalTextSegment` carries its own `LLStyle`; drawing
  (`LLNormalTextSegment::drawClippedSegment`, `lltextbase.cpp:4165`) renders
  with the segment's font. Mixed-font segments (links, icons, hover states)
  already coexist on one line, so reflow and width caching handle splits.
* The VulkanStorm text path (`indra/llvulkan/llvktext.cpp:78`) keys glyph
  atlases off the `LLFontGL*` pointer, so bold/italic variants are just
  additional registered fonts — no Vulkan-specific work.

## Integration Point

The single funnel for all displayed message text:

```
LLChatHistory::appendMessage
  -> mEditor->appendText(message, ...)          (llchathistory.cpp:1623)
  -> LLTextBase::appendText
  -> LLTextBase::appendTextImpl                 (lltextbase.cpp:2826)
       |-- URL/slurl split (LLUrlRegistry)      <- links isolated here first
       +-- appendAndHighlightTextImpl per chunk (lltextbase.cpp:3134)
             |-- LLTextParser keyword highlights (color only)
             +-- LLNormalTextSegment insertion
```

Applying a markdown pass to the plain-text chunks **after** URL splitting
means formatted text can never cross or corrupt links, `<nolink>` spans, or
URLs containing underscores.

## Design Decisions

1. **Opt-in XUI param** `parse_markdown` alongside existing `parse_urls` /
   `parse_highlights` in `LLTextBase::Params` (`lltextbase.h:395`). Enabled
   only for read-only displays (chat history, IM); editable controls
   (chat bar, notecards, script editor) untouched.
2. **CommonMark-style flanking rules** for `_` emphasis: underscores must
   occur at word boundaries so `some_var_name`, object names, and landmark
   names (extremely common in SL) are never mangled. `**` is strong emphasis.
3. **Flag merging**: `**bold**` inside an italic emote merges to
   `BOLD|ITALIC` on the segment font.
4. **Delimiter removal** mirrors existing URL-label masking behavior.
5. **Emote toggle convention**: `/me ...`, a leading `:`, and a leading
   `_` are three markers for the same thing — an emote. The colon is
   already normalized to `/me` at input time by `FSCommon::applyMuPose`
   (called from the nearby-chat and IM send paths); this change extends
   `applyMuPose` to also normalize a leading `_` to `/me` (unconditionally
   — unlike the colon, the underscore conversion is not gated by
   `AllowMUpose`), so the chat history only ever sees `/me` emotes.
   Emotes are rendered **italic unconditionally** (the `EmotesUseItalic`
   preference no longer applies; the toggle convention depends on the
   italic base). Within an emote, `_` toggles italic off (spoken part)
   and back on, and `**bold**` still applies (merging onto the current
   state). A trailing `_` with nothing after it closes italics and is
   consumed; to render a literal underscore, type it twice (`__` shows
   as `_`). An emote that does not end with a trailing `_` does not
   carry formatting forward — the toggle never survives the line (text
   is split per line before parsing). Spans are `EMOTE_TOGGLE_ON`
   (italic) / `EMOTE_TOGGLE_OFF` (normal) / `EMOTE_DELIM` (hidden `_`) /
   `EMOTE_LITERAL` (collapsed `__`); the caller strips or keeps ITALIC
   relative to the base style. The mode is passed via
   `LLStyle::Params::markdown_emote`, set where `irc_me` styling is
   applied.
6. **Unit tests** in the llui tut harness, which already stubs font
   resolution without GL (`llui/tests/llurlmatch_test.cpp`).

## Risks

| Concern | Assessment |
|---|---|
| Intraword underscores | Mitigated by flanking rules in normal chat; emote mode uses toggles per convention |
| Keyword highlight interaction | Orthogonal (color vs. font); passes compose |
| Copy/paste loses delimiters | Same trade-off as URL-label masking; plain-text mode bypasses |
| Performance | ASCII-only delimiter scan on short chat lines; negligible |
| Style-flag string hacks | Replaced with a flag-merging helper (see `lltextbase.cpp:3168`) |

## Upstream Potential

All touched code lives in `llui`/`llrender` — shared Linden viewer code, not
FS-specific. Gated like `parse_urls`, this is a strong candidate for a
Linden Lab upstream proposal: no protocol, asset, or renderer changes needed.

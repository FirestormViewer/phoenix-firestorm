# Literal underscores in chat emoticons

Reference: LLMarkdown::parseEmphasis and stripEmphasisDelimiters at a3ee617385.
Normal chat uses underscore flanking rules, including unmatched openers;
emotes consume single underscores as italic toggles. Both honor literal ranges.
Thus ^_^ loses its underscore in normal chat, and o_o loses it in emotes.

Recognize an underscore run between conventional ASCII or Unicode eye characters before
emphasis matching. Protect that run using the existing literal-range
mechanism in both modes. Letter/digit eyes must not be embedded in larger words;
punctuation eyes may touch surrounding text. Keep double-underscore escapes outside recognized faces,
URL protection, bold, intentional italics and emote toggles unchanged.

This is a shared CPU text-parser correction requested for both renderers, not a
change to the OpenGL renderer to meet Vulkan parity. NV-00/NV-01: the parser
produces semantic text spans, while rendering remains backend-owned. No GPU
state, lifetime or synchronization changes. Using existing literal ranges avoids
adding a new span type or separate behavior for individual rendering consumers.

Discriminating checks: normal/emote spans retain ^_^, o_o, 0_0, ^_~ and other
eye variants; enclosing formatting still works; word underscores and intentional
emote toggles keep their previous behavior. Run the complete parser test group.


Reference corpus (consulted 2026-09-20):
- https://en.wikipedia.org/wiki/List_of_emoticons — upright and Eastern tables:
  surprise, crying, sideways looks, winks, blank eyes and disapproval.
- https://kaomoji.you/en/ — catalog examples for sadness, pain, confusion,
  doubt, surprise and winking, including Unicode eye characters.
- https://en.wikipedia.org/wiki/Kaomoji — extended underscore mouths and
  accented-eye variants.

The recognition vocabulary is explicit, not a claim to recognize every possible
kaomoji. It covers adjacent eyes and a contiguous underscore mouth, including
asymmetric eyes and common wrappers/decoration. Spaced mouths, combining eye
accents and underscores used as arms (such as the shrug) are outside this rule.


Validation on Windows x64 (MSVC 19.44, RelWithDebInfo): compiled the real parser
and complete existing TUT test group against repository headers. All 47 tests
pass. Repeating the same tests against the unchanged baseline parser passes the
43 original tests and fails all four new regression tests. No viewer executable
was rebuilt or launched; in-viewer visual acceptance remains unverified.

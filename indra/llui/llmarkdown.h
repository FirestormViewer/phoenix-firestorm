/**
 * @file llmarkdown.h
 * @brief Markdown-style emphasis (_italic_ / **bold**) tokenizer for UI text
 *
 * $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef LL_LLMARKDOWN_H
#define LL_LLMARKDOWN_H

#include <string>
#include <vector>
#include <utility>

// Parses a UTF-8 string for markdown-style emphasis spans:
//   **bold**   -> strong emphasis
//   _italic_   -> emphasis (CommonMark-style flanking rules: opening '_'
//                 must not follow a word character and must be followed by
//                 non-whitespace; closing '_' must not precede a word
//                 character and must follow non-whitespace). This keeps
//                 intraword underscores such as "some_var_name" literal.
// A single '*' is not treated as emphasis to avoid false positives in
// chat ("2*3*4", emoticons, censoring). Emphasis spans may contain strong
// spans ("_some **bold** text_" -> italic-bold-italic nesting).
// An unmatched valid opening delimiter formats through the end of this input.
// Outside recognized emoticons, two underscores display one literal underscore.
// Underscores between emoticon eyes (^_^, o_o, 0_0, ^_~, etc.) stay literal
// in both modes, including extended mouths (^____^), preserving surrounding style.
//
// The result is a list of spans that exactly reconstructs the original
// string when concatenated, with the delimiter runs either marked as
// emphasis delimiters (rendered hidden) or left as literal text.
class LLMarkdown
{
public:
    enum class ESpanType : U8
    {
        PLAIN,             // ordinary text (keeps the incoming style)
        EMPHASIS,          // text inside _..._ (render italic)
        STRONG,            // text inside **...** (render bold)
        EMPHASIS_DELIM,    // matched '_' delimiters (hidden)
        STRONG_DELIM,      // matched '**' delimiters (hidden)
        EMOTE_TOGGLE_OFF,  // emote mode: base italic toggled OFF (spoken part)
        EMOTE_TOGGLE_ON,   // emote mode: italic toggled back ON (action resumes)
        EMOTE_DELIM,       // emote mode: a '_' toggle character (hidden)
        EMOTE_LITERAL      // emote mode: an escaped '__' shown as one '_'
    };

    struct Span
    {
        ESpanType   mType;
        std::string mText;
    };

    using span_vec_t = std::vector<Span>;
    using literal_ranges_t = std::vector<std::pair<size_t, size_t>>;

    // Split text into emphasis spans. Concatenating spans reconstructs the
    // input except that escaped double underscores collapse to one underscore.
    //
    // When emote is true, '_' *toggles* the base italic off and on rather
    // than delimiting emphasis. Emotes ("/me ...", and their synonyms a
    // leading ':' or '_' which are normalized to "/me" at input time by
    // FSCommon::applyMuPose) are rendered italic by default. Each '_'
    // flips between the action (italic) and the spoken part (normal):
    //   "nods_ yes _grins"   -> ON "nods"(italic), OFF " yes "(normal), ON "grins"(italic)
    //   "shrugs_ whatever"   -> ON "shrugs"(italic), OFF " whatever"(normal, to end of line)
    // A trailing '_' with nothing after it simply closes the current
    // region (consumed, not rendered). To render a literal underscore,
    // type it twice: "__" shows as "_". Regions are reported as
    // EMOTE_TOGGLE_ON (base italic, caller keeps ITALIC) or
    // EMOTE_TOGGLE_OFF (caller strips ITALIC); EMOTE_DELIM is the hidden
    // toggle character and EMOTE_LITERAL a collapsed '__'. The toggle
    // never survives the line (input is split per line upstream).
    static span_vec_t parseEmphasis(const std::string& text, bool emote = false,
        const literal_ranges_t& literal_ranges = {});

    // <FS> Strip the markdown emphasis delimiters, returning the visible text
    // with all EMPHASIS_DELIM / STRONG_DELIM / EMOTE_DELIM runs removed and
    // escaped EMOTE_LITERAL "__" collapsed to a single '_'. Font styling is
    // dropped — this is for consumers that render plain text only (e.g. the
    // on-screen LLConsole chat overlay, which has no per-segment font style)
    // but must not show the raw delimiters.
    static std::string stripEmphasisDelimiters(const std::string& text, bool emote = false);
    // </FS>
};

#endif // LL_LLMARKDOWN_H

/**
 * @file llmarkdown.cpp
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

#include "linden_common.h"

#include "llmarkdown.h"

namespace
{
    // Word characters for flanking rules: ASCII alphanumerics, underscore,
    // and any non-ASCII (UTF-8 continuation/lead) byte. The latter keeps
    // Unicode letters adjacent to a delimiter counting as "word", which
    // matches user expectations for non-Latin text.
    bool isWordByte(char c)
    {
        return LLStringOps::isAlnum(c) || c == '_' || (c & 0x80);
    }

    char byteAt(const std::string& text, size_t pos)
    {
        return pos < text.size() ? text[pos] : '\0';
    }

    // CommonMark-flavored left-flanking check for a single '_' at pos:
    // not preceded by a word character, and followed by non-whitespace.
    // Not used in emote mode (see toggle pass below).
    bool canOpenEmphasis(const std::string& text, size_t pos)
    {
        char after = byteAt(text, pos + 1);
        if (after == '\0' || LLStringOps::isSpace(after))
        {
            return false;
        }
        char before = pos > 0 ? text[pos - 1] : '\0';
        return before == '\0' || !isWordByte(before);
    }

    // Right-flanking check for a single '_' at pos: preceded by
    // non-whitespace, not followed by a word character.
    bool canCloseEmphasis(const std::string& text, size_t pos)
    {
        if (pos == 0 || LLStringOps::isSpace(text[pos - 1]))
        {
            return false;
        }
        char after = byteAt(text, pos + 1);
        return after == '\0' || !isWordByte(after);
    }
}

LLMarkdown::span_vec_t LLMarkdown::parseEmphasis(const std::string& text, bool emote)
{
    span_vec_t spans;
    if (text.empty())
    {
        return spans;
    }

    // ** is resolved first (it may appear intraword, e.g. "see**this**"),
    // then _..._ emphasis is matched over the whole string, skipping
    // underscores inside strong spans. Emphasis spans may therefore
    // contain strong spans ("_some **bold** text_" -> italic-bold-italic
    // nesting); the span emitter below intersects the two sets.
    struct Match
    {
        size_t mDelimPos;   // start of the opening delimiter run
        size_t mDelimLen;   // length of the delimiter run
        size_t mContentPos; // start of emphasized content
        size_t mContentLen; // length of emphasized content
        ESpanType mType;
    };
    std::vector<Match> strong_matches, em_matches;

    // Pass 1: strong emphasis (**...**).
    size_t pos = 0;
    while (pos < text.size())
    {
        if (text[pos] == '*' && byteAt(text, pos + 1) == '*')
        {
            size_t close = text.find("**", pos + 2);
            // Content must be non-empty and not all whitespace ("** **").
            if (close != std::string::npos && close > pos + 2 &&
                text.find_first_not_of(" \t\r\n", pos + 2) < close)
            {
                strong_matches.push_back({ pos, 2, pos + 2, close - (pos + 2), ESpanType::STRONG });
                pos = close + 2;
                continue;
            }
        }
        ++pos;
    }

    auto inside_strong = [&strong_matches](size_t p)
    {
        for (const Match& m : strong_matches)
        {
            size_t strong_end = m.mContentPos + m.mContentLen + m.mDelimLen;
            if (p >= m.mDelimPos && p < strong_end)
            {
                return true;
            }
        }
        return false;
    };

    // Emote mode: '_' toggles the base italic off/on. This is a single
    // self-contained toggle pass, but '**' strong spans (already matched
    // above) still apply inside it, so we split around them too. An
    // unpaired trailing '_' just closes the current region (consumed);
    // '__' is an escaped literal underscore. Handled here, then return.
    if (emote)
    {
        bool italic_on = true; // emotes start italic
        size_t seg_start = 0;
        size_t strong_idx = 0;
        size_t i = 0;
        auto flush = [&](size_t end)
        {
            if (end > seg_start)
            {
                spans.push_back({ italic_on ? ESpanType::EMOTE_TOGGLE_ON : ESpanType::EMOTE_TOGGLE_OFF,
                                  text.substr(seg_start, end - seg_start) });
            }
        };
        while (i < text.size())
        {
            // A matched '**' strong span interrupts the current region.
            if (strong_idx < strong_matches.size() && i == strong_matches[strong_idx].mDelimPos)
            {
                const Match& sm = strong_matches[strong_idx];
                flush(i);
                spans.push_back({ ESpanType::STRONG_DELIM, text.substr(sm.mDelimPos, sm.mDelimLen) });
                spans.push_back({ ESpanType::STRONG, text.substr(sm.mContentPos, sm.mContentLen) });
                spans.push_back({ ESpanType::STRONG_DELIM, text.substr(sm.mContentPos + sm.mContentLen, sm.mDelimLen) });
                i = sm.mContentPos + sm.mContentLen + sm.mDelimLen;
                seg_start = i;
                ++strong_idx;
                continue;
            }
            if (text[i] == '_')
            {
                flush(i);
                if (byteAt(text, i + 1) == '_')
                {
                    // escaped literal: "__" renders as one '_', no toggle
                    spans.push_back({ ESpanType::EMOTE_LITERAL, "_" });
                    i += 2;
                }
                else
                {
                    // toggle: consume the delimiter, flip state
                    spans.push_back({ ESpanType::EMOTE_DELIM, text.substr(i, 1) });
                    italic_on = !italic_on;
                    ++i;
                }
                seg_start = i;
            }
            else
            {
                ++i;
            }
        }
        flush(text.size());
        return spans;
    }

    // Pass 2: emphasis (_..._).
    pos = 0;
    while (pos < text.size())
    {
        if (text[pos] == '_' && !inside_strong(pos))
        {
            if (canOpenEmphasis(text, pos))
            {
                size_t close = pos + 1;
                for (;;)
                {
                    close = text.find('_', close);
                    if (close == std::string::npos)
                    {
                        break;
                    }
                    if (!inside_strong(close) && canCloseEmphasis(text, close))
                    {
                        break;
                    }
                    ++close;
                }
                if (close != std::string::npos)
                {
                    em_matches.push_back({ pos, 1, pos + 1, close - (pos + 1), ESpanType::EMPHASIS });
                    pos = close + 1;
                    continue;
                }
            }
        }
        ++pos;
    }

    // Emit spans. Strong matches partition the string first; emphasis
    // matches are intersected with that partition so that an emphasis span
    // containing a strong span yields emphasis / strong / emphasis pieces.
    auto emit = [&](size_t from, size_t to, ESpanType base_type)
    {
        size_t cursor = from;
        for (const Match& em : em_matches)
        {
            size_t em_end = em.mContentPos + em.mContentLen + em.mDelimLen;
            if (em.mDelimPos >= to || em_end <= from)
            {
                continue;
            }
            // intersection of the emphasis match with [from, to)
            size_t seg_from = std::max(em.mDelimPos, from);
            size_t seg_to = std::min(em_end, to);
            if (seg_from > cursor)
            {
                spans.push_back({ base_type, text.substr(cursor, seg_from - cursor) });
            }
            // classify each piece of the intersected region
            size_t piece = seg_from;
            while (piece < seg_to)
            {
                ESpanType type;
                size_t next;
                if (piece >= em.mContentPos && piece < em.mContentPos + em.mContentLen)
                {
                    type = em.mType; // EMPHASIS or EMOTE_TOGGLE
                    next = std::min(em.mContentPos + em.mContentLen, seg_to);
                }
                else
                {
                    type = ESpanType::EMPHASIS_DELIM;
                    next = std::min(piece < em.mContentPos ? em.mContentPos : em_end, seg_to);
                }
                if (next > piece)
                {
                    spans.push_back({ type, text.substr(piece, next - piece) });
                }
                piece = next;
            }
            cursor = seg_to;
        }
        if (cursor < to)
        {
            spans.push_back({ base_type, text.substr(cursor, to - cursor) });
        }
    };

    size_t cursor = 0;
    for (const Match& strong : strong_matches)
    {
        size_t strong_end = strong.mContentPos + strong.mContentLen + strong.mDelimLen;
        if (strong.mDelimPos > cursor)
        {
            emit(cursor, strong.mDelimPos, ESpanType::PLAIN);
        }
        // opening '**' — strip any emphasis delimiters (can't overlap by construction)
        spans.push_back({ ESpanType::STRONG_DELIM, text.substr(strong.mDelimPos, strong.mDelimLen) });
        emit(strong.mContentPos, strong.mContentPos + strong.mContentLen, ESpanType::STRONG);
        spans.push_back({ ESpanType::STRONG_DELIM, text.substr(strong.mContentPos + strong.mContentLen, strong.mDelimLen) });
        cursor = strong_end;
    }
    if (cursor < text.size())
    {
        emit(cursor, text.size(), ESpanType::PLAIN);
    }

    return spans;
}

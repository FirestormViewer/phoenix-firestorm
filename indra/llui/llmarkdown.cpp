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
#include <string_view>

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

    bool isEmoticonEye(std::string_view eye)
    {
        // Conventional eye characters, including asymmetric pairs (^_~, o_O).
        // See doc/markdown_emoticons.md for the reference corpus.
        constexpr std::string_view ascii_eyes = "^~oO0123456789><-TtUu=+xX;:@*.?QqPpYyEeNn'`/\\";
        constexpr std::string_view unicode_eyes =
            "\xe0\xb2\xa0\xe0\xb2\xa5\xc2\xb0\xe3\x83\xbb\xe2\x80\xa2\xc2\xac"
            "\xe3\x83\xbc\xe4\xb8\x80\xef\xbf\xa3\xef\xbc\xa0\xef\xbc\xbe\xe2\x8a\x99"
            "\xe2\x98\x89\xe2\x97\x8e\xe2\x95\xa5\xc3\xb2\xc3\xb3\xc3\xb5"
            "\xc3\xb9\xc3\x94\xc3\x97\xef\xbf\xa2\xe2\x86\x92\xe2\x86\x90"
            "\xe2\x86\xbc\xe2\x87\x80\xe2\x96\xa1\xce\xbc\xe4\xb8\xaa\xe3\x83\x8e"
            "\xef\xbe\x89\xe3\x83\xbd\xef\xbc\xbc\xef\xbc\x9e\xef\xbc\x9c\xef\xbc\x8b"
            "\xcb\x98\xeb\x88\x88\xe2\x97\xa3\xe2\x97\xa2\xe2\x98\x86";
        return eye.size() == 1 ? ascii_eyes.find(eye) != std::string_view::npos :
            !eye.empty() && unicode_eyes.find(eye) != std::string_view::npos;
    }

    size_t emoticonMouthEnd(const std::string& text, size_t pos)
    {
        if (text[pos] != '_' || pos == 0 || text[pos - 1] == '_')
        {
            return pos;
        }
        const size_t end = text.find_first_not_of('_', pos);
        if (end == std::string::npos)
        {
            return pos;
        }
        // Locate adjacent UTF-8 characters without changing source byte offsets.
        size_t left = pos - 1;
        while (left > 0 && (static_cast<unsigned char>(text[left]) & 0xc0) == 0x80)
        {
            --left;
        }
        size_t right = end + 1;
        while (right < text.size() && (static_cast<unsigned char>(text[right]) & 0xc0) == 0x80)
        {
            ++right;
        }
        const std::string_view view(text);
        if (!isEmoticonEye(view.substr(left, pos - left)) ||
            !isEmoticonEye(view.substr(end, right - end)))
        {
            return pos;
        }
        auto is_letter_or_digit = [](char c)
        {
            return isWordByte(c) && c != '_';
        };
        // Do not mistake the middle of "photo_order" for an o_o face.
        if ((is_letter_or_digit(text[left]) && left > 0 && is_letter_or_digit(text[left - 1])) ||
            (is_letter_or_digit(text[end]) && is_letter_or_digit(byteAt(text, right))))
        {
            return pos;
        }
        return end;
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

LLMarkdown::span_vec_t LLMarkdown::parseEmphasis(const std::string& text, bool emote, const literal_ranges_t& literal_ranges)
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
    literal_ranges_t protected_ranges = literal_ranges;
    for (size_t offset = 0; offset < text.size(); ++offset)
    {
        const size_t end = emoticonMouthEnd(text, offset);
        if (end > offset)
        {
            protected_ranges.emplace_back(offset, end);
            offset = end - 1;
        }
    }
    std::vector<size_t> escaped_underscores;
    auto is_literal = [&protected_ranges](size_t offset)
    {
        return std::any_of(protected_ranges.begin(), protected_ranges.end(), [offset](const auto& range)
        {
            return offset >= range.first && offset < range.second;
        });
    };
    if (!emote)
    {
        for (size_t offset = 0; offset + 1 < text.size(); ++offset)
        {
            if (text[offset] == '_' && text[offset + 1] == '_' &&
                !is_literal(offset) && !is_literal(offset + 1))
            {
                escaped_underscores.push_back(offset);
                protected_ranges.emplace_back(offset, offset + 2);
                ++offset;
            }
        }
    }

    // Pass 1: strong emphasis (**...**).
    size_t pos = 0;
    while (pos < text.size())
    {
        if (text[pos] == '*' && byteAt(text, pos + 1) == '*' && !is_literal(pos) && !is_literal(pos + 1))
        {
            size_t close = text.find("**", pos + 2);
            while (close != std::string::npos && (is_literal(close) || is_literal(close + 1)))
            {
                close = text.find("**", close + 2);
            }
            if (close == std::string::npos && pos + 2 < text.size() &&
                !LLStringOps::isSpace(text[pos + 2]))
            {
                close = text.size();
            }
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
                if (sm.mContentPos + sm.mContentLen < text.size())
                {
                    spans.push_back({ ESpanType::STRONG_DELIM, text.substr(sm.mContentPos + sm.mContentLen, sm.mDelimLen) });
                }
                i = sm.mContentPos + sm.mContentLen + sm.mDelimLen;
                seg_start = i;
                ++strong_idx;
                continue;
            }
            if (text[i] == '_' && !is_literal(i))
            {
                flush(i);
                if (byteAt(text, i + 1) == '_' && !is_literal(i + 1))
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
        if (text[pos] == '_' && !inside_strong(pos) && !is_literal(pos))
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
                    if (!inside_strong(close) && !is_literal(close) && canCloseEmphasis(text, close))
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
                em_matches.push_back({ pos, 1, pos + 1, text.size() - (pos + 1), ESpanType::EMPHASIS });
                break;
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
        if (strong.mContentPos + strong.mContentLen < text.size())
        {
            spans.push_back({ ESpanType::STRONG_DELIM, text.substr(strong.mContentPos + strong.mContentLen, strong.mDelimLen) });
        }
        cursor = strong_end;
    }
    if (cursor < text.size())
    {
        emit(cursor, text.size(), ESpanType::PLAIN);
    }

    size_t source_offset = 0;
    size_t escape_index = 0;
    for (auto& span : spans)
    {
        const size_t original_size = span.mText.size();
        size_t removed = 0;
        while (escape_index < escaped_underscores.size() &&
               escaped_underscores[escape_index] < source_offset + original_size)
        {
            const size_t position = escaped_underscores[escape_index] - source_offset;
            span.mText.erase(position + 1 - removed, 1);
            ++removed;
            ++escape_index;
        }
        source_offset += original_size;
    }
    return spans;
}

// <FS> See llmarkdown.h. Drop the delimiter spans; keep everything else
// (PLAIN / EMPHASIS / STRONG / EMOTE_TOGGLE_* / EMOTE_LITERAL all contribute
// their visible text; EMOTE_LITERAL is already the collapsed single '_').
std::string LLMarkdown::stripEmphasisDelimiters(const std::string& text, bool emote)
{
    // Fast path: no delimiters at all -> unchanged.
    if (text.find('_') == std::string::npos && text.find("**") == std::string::npos)
    {
        return text;
    }
    std::string out;
    out.reserve(text.size());
    for (const Span& span : parseEmphasis(text, emote))
    {
        switch (span.mType)
        {
            case ESpanType::EMPHASIS_DELIM:
            case ESpanType::STRONG_DELIM:
            case ESpanType::EMOTE_DELIM:
                break; // hidden delimiter: drop
            default:
                out += span.mText;
                break;
        }
    }
    return out;
}
// </FS>

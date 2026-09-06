/**
 * @file llmarkdown_test.cpp
 * @brief Unit tests for LLMarkdown emphasis tokenizer
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

#include "../llmarkdown.h"

#include "lltut.h"

namespace tut
{
    struct markdown_data
    {
        // Concatenating all spans must exactly reconstruct the input;
        // verified for every test case.
        static void ensure_roundtrip(const std::string& input, const LLMarkdown::span_vec_t& spans)
        {
            std::string joined;
            for (const auto& span : spans)
            {
                joined += span.mText;
            }
            ensure_equals("roundtrip: " + input, joined, input);
        }

        static LLMarkdown::span_vec_t parse(const std::string& input)
        {
            LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input);
            ensure_roundtrip(input, spans);
            return spans;
        }

        // Serialize span types for compact assertions:
        //   P = PLAIN, I = EMPHASIS, B = STRONG, d = EMPHASIS_DELIM,
        //   D = STRONG_DELIM, O = EMOTE_TOGGLE_ON, F = EMOTE_TOGGLE_OFF,
        //   e = EMOTE_DELIM, L = EMOTE_LITERAL
        static std::string signature(const LLMarkdown::span_vec_t& spans)
        {
            std::string sig;
            for (const auto& span : spans)
            {
                switch (span.mType)
                {
                    case LLMarkdown::ESpanType::PLAIN:            sig += 'P'; break;
                    case LLMarkdown::ESpanType::EMPHASIS:         sig += 'I'; break;
                    case LLMarkdown::ESpanType::STRONG:           sig += 'B'; break;
                    case LLMarkdown::ESpanType::EMPHASIS_DELIM:   sig += 'd'; break;
                    case LLMarkdown::ESpanType::STRONG_DELIM:     sig += 'D'; break;
                    case LLMarkdown::ESpanType::EMOTE_TOGGLE_ON:  sig += 'O'; break;
                    case LLMarkdown::ESpanType::EMOTE_TOGGLE_OFF: sig += 'F'; break;
                    case LLMarkdown::ESpanType::EMOTE_DELIM:      sig += 'e'; break;
                    case LLMarkdown::ESpanType::EMOTE_LITERAL:    sig += 'L'; break;
                }
            }
            return sig;
        }
    };

    typedef test_group<markdown_data> markdown_group;
    typedef markdown_group::object markdown_object;
    markdown_group markdown_grup("llmarkdown");

    template<> template<>
    void markdown_object::test<1>()
    {
        // Plain text without delimiters passes through untouched.
        LLMarkdown::span_vec_t spans = parse("hello world");
        ensure_equals("count", spans.size(), 1u);
        ensure_equals("type", signature(spans), "P");
        ensure_equals("text", spans[0].mText, "hello world");
    }

    template<> template<>
    void markdown_object::test<2>()
    {
        // Empty input produces no spans.
        ensure_equals("count", parse("").size(), 0u);
    }

    template<> template<>
    void markdown_object::test<3>()
    {
        // Basic italic span.
        LLMarkdown::span_vec_t spans = parse("_italic_");
        ensure_equals("signature", signature(spans), "dId");
        ensure_equals("content", spans[1].mText, "italic");
    }

    template<> template<>
    void markdown_object::test<4>()
    {
        // Basic bold span.
        LLMarkdown::span_vec_t spans = parse("**bold**");
        ensure_equals("signature", signature(spans), "DBD");
        ensure_equals("content", spans[1].mText, "bold");
    }

    template<> template<>
    void markdown_object::test<5>()
    {
        // Italic embedded in surrounding text.
        LLMarkdown::span_vec_t spans = parse("say _wow_ now");
        ensure_equals("signature", signature(spans), "PdIdP");
        ensure_equals("prefix", spans[0].mText, "say ");
        ensure_equals("content", spans[2].mText, "wow");
        ensure_equals("suffix", spans[4].mText, " now");
    }

    template<> template<>
    void markdown_object::test<6>()
    {
        // Intraword underscores must stay literal (object/var names).
        LLMarkdown::span_vec_t spans = parse("some_var_name");
        ensure_equals("signature", signature(spans), "P");
        ensure_equals("text", spans[0].mText, "some_var_name");
    }

    template<> template<>
    void markdown_object::test<7>()
    {
        // Underscore inside a word never opens emphasis.
        LLMarkdown::span_vec_t spans = parse("abc_def_ ghi");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<8>()
    {
        // Unmatched underscore stays literal.
        LLMarkdown::span_vec_t spans = parse("a _lonely underscore");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<9>()
    {
        // Unmatched double asterisk stays literal.
        LLMarkdown::span_vec_t spans = parse("a **lonely star");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<10>()
    {
        // Single asterisks are never emphasis (2*3*4, emoticons).
        LLMarkdown::span_vec_t spans = parse("2*3*4");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<11>()
    {
        // Empty emphasis content is not a match: "____" stays literal
        // (closing at index 1 would need a non-space char before it, '_'
        // is a word char, so no close; likewise "** **").
        LLMarkdown::span_vec_t spans = parse("** **");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<12>()
    {
        // Bold content containing spaces.
        LLMarkdown::span_vec_t spans = parse("**two words**");
        ensure_equals("signature", signature(spans), "DBD");
        ensure_equals("content", spans[1].mText, "two words");
    }

    template<> template<>
    void markdown_object::test<13>()
    {
        // Multiple spans in one string.
        LLMarkdown::span_vec_t spans = parse("_a_ and **b**");
        ensure_equals("signature", signature(spans), "dIdPDBD");
        ensure_equals("italic", spans[1].mText, "a");
        ensure_equals("bold", spans[5].mText, "b");
    }

    template<> template<>
    void markdown_object::test<14>()
    {
        // Bold nested inside emphasis: the strong pass claims "**strong**"
        // first; the outer underscores flank punctuation (a valid emphasis
        // boundary), so the whole span is emphasized. The strong content
        // span keeps its EMPHASIS classification (the caller merges both),
        // while the '**' delimiters are STRONG_DELIM for stripping.
        LLMarkdown::span_vec_t spans = parse("_**strong**_");
        ensure_equals("signature", signature(spans), "dDIDd");
        ensure_equals("content", spans[2].mText, "strong");
    }

    template<> template<>
    void markdown_object::test<15>()
    {
        // Closing underscore followed by a word char is not a close;
        // the span extends to the next valid closer.
        LLMarkdown::span_vec_t spans = parse("_a_b_ c");
        ensure_equals("signature", signature(spans), "dIdP");
        ensure_equals("content", spans[1].mText, "a_b");
    }

    template<> template<>
    void markdown_object::test<21>()
    {
        // Emphasis containing a strong span yields italic/bold/italic
        // nesting with both delimiter pairs stripped.
        LLMarkdown::span_vec_t spans = parse("_some **bold** text_");
        ensure_equals("signature", signature(spans), "dIDIDId");
        ensure_equals("italic lead", spans[1].mText, "some ");
        ensure_equals("bold", spans[3].mText, "bold");
        ensure_equals("italic tail", spans[5].mText, " text");
    }

    // Emote mode ("/me ..." messages, rendered italic by default): '_'
    // toggles the base italic OFF (EMOTE_TOGGLE_OFF, the spoken part) and
    // back ON (EMOTE_TOGGLE_ON, the action). A trailing '_' with nothing
    // after it just closes the current region (consumed, not rendered).
    // "__" is an escaped literal underscore. The toggle never survives
    // the line (input is split per line by the caller).
    //
    // signature letters: O = EMOTE_TOGGLE_ON, F = EMOTE_TOGGLE_OFF,
    //                    e = EMOTE_DELIM, L = EMOTE_LITERAL

    template<> template<>
    void markdown_object::test<22>()
    {
        // Roleplay convention: spoken quote inside an emote. The quote is
        // toggled off (normal), "pants" resumes italic after the closing
        // '_', and the trailing '_' closes italics (consumed, not literal).
        std::string input = "throws a glass at Edward_ \"You idiot!\" _pants_";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, true);
        ensure_equals("signature", signature(spans), "OeFeOe");
        ensure_equals("lead (italic)", spans[0].mText, "throws a glass at Edward");
        ensure_equals("quote (normal)", spans[2].mText, " \"You idiot!\" ");
        ensure_equals("tail (italic)", spans[4].mText, "pants");
        ensure_equals("trailing '_' consumed", spans[5].mText, "_");
    }

    template<> template<>
    void markdown_object::test<23>()
    {
        // Toggle pair with the tail reverting to base italic.
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis("nods_ yes _grins", true);
        ensure_equals("signature", signature(spans), "OeFeO");
        ensure_equals("quote (normal)", spans[2].mText, " yes ");
        ensure_equals("tail (italic)", spans[4].mText, "grins");
    }

    template<> template<>
    void markdown_object::test<24>()
    {
        // Long quote: toggle closes at the second underscore; the
        // remainder of the line is back to base italic (the toggle does
        // not survive the line).
        std::string input = "nods_ yes, that's right, I almost got you killed _he grins";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, true);
        ensure_equals("signature", signature(spans), "OeFeO");
        ensure_equals("quote (normal)", spans[2].mText, " yes, that's right, I almost got you killed ");
        ensure_equals("tail (italic)", spans[4].mText, "he grins");
    }

    template<> template<>
    void markdown_object::test<25>()
    {
        // Two toggle pairs: each '_' flips the state, so "and" between
        // the pairs is back to base italic.
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis("waves_ hi _and_ bye _smiles", true);
        ensure_equals("signature", signature(spans), "OeFeOeFeO");
        ensure_equals("first (normal)", spans[2].mText, " hi ");
        ensure_equals("between (italic)", spans[4].mText, "and");
        ensure_equals("second (normal)", spans[6].mText, " bye ");
        ensure_equals("tail (italic)", spans[8].mText, "smiles");
    }

    template<> template<>
    void markdown_object::test<26>()
    {
        // Strict (non-emote) mode leaves the emote convention literal.
        std::string input = "nods_ yes, that's right _he grins";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, false);
        ensure_roundtrip(input, spans);
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<27>()
    {
        // An emote whose italic is toggled off mid-line and never
        // re-opened: the rest of the line is normal (no trailing '_').
        std::string input = "shrugs_ whatever";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, true);
        ensure_equals("signature", signature(spans), "OeF");
        ensure_equals("lead (italic)", spans[0].mText, "shrugs");
        ensure_equals("tail (normal)", spans[2].mText, " whatever");
    }

    template<> template<>
    void markdown_object::test<28>()
    {
        // An escaped '__' renders as a single literal '_' and does not
        // toggle. State stays italic through " loudly"; the '_' after it
        // toggles off, leaving " ok" normal. Roundtrip is intentionally
        // lossy ('__' -> '_').
        std::string input = "sighs__ loudly_ ok";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, true);
        ensure_equals("signature", signature(spans), "OLOeF");
        ensure_equals("lead (italic)", spans[0].mText, "sighs");
        ensure_equals("literal '_'", spans[1].mText, "_");
        ensure_equals("still italic", spans[2].mText, " loudly");
        ensure_equals("toggle", spans[3].mText, "_");
        ensure_equals("tail (normal)", spans[4].mText, " ok");
    }

    template<> template<>
    void markdown_object::test<29>()
    {
        // Bold inside an emote merges with the current toggle state; the
        // strong '**' delimiters are dropped as usual.
        std::string input = "stares_ **seriously**? _blinks";
        LLMarkdown::span_vec_t spans = LLMarkdown::parseEmphasis(input, true);
        ensure_equals("signature", signature(spans), "OeFDBDFeO");
        ensure_equals("lead (italic)", spans[0].mText, "stares");
        ensure_equals("spoken lead (normal)", spans[2].mText, " ");
        ensure_equals("bold (normal)", spans[4].mText, "seriously");
        ensure_equals("spoken tail (normal)", spans[6].mText, "? ");
        ensure_equals("tail (italic)", spans[8].mText, "blinks");
    }

    template<> template<>
    void markdown_object::test<16>()
    {
        // Non-ASCII (UTF-8) text adjacent to delimiters behaves like word
        // characters, so "café_au_lait" stays literal.
        std::string input = "caf\xC3\xA9_au_lait";
        LLMarkdown::span_vec_t spans = parse(input);
        ensure_equals("signature", signature(spans), "P");
        ensure_equals("text", spans[0].mText, input);
    }

    template<> template<>
    void markdown_object::test<17>()
    {
        // Emphasis containing non-ASCII content works.
        std::string input = "_caf\xC3\xA9_";
        LLMarkdown::span_vec_t spans = parse(input);
        ensure_equals("signature", signature(spans), "dId");
    }

    template<> template<>
    void markdown_object::test<18>()
    {
        // Punctuation around emphasis: closing '_' before punctuation closes.
        LLMarkdown::span_vec_t spans = parse("that was _great_!");
        ensure_equals("signature", signature(spans), "PdIdP");
        ensure_equals("content", spans[2].mText, "great");
        ensure_equals("suffix", spans[4].mText, "!");
    }

    template<> template<>
    void markdown_object::test<19>()
    {
        // Opening '_' followed by whitespace cannot open.
        LLMarkdown::span_vec_t spans = parse("_ nope _");
        ensure_equals("signature", signature(spans), "P");
    }

    template<> template<>
    void markdown_object::test<20>()
    {
        // A bold span followed by an unmatched underscore still parses the
        // bold span and leaves the rest literal.
        LLMarkdown::span_vec_t spans = parse("**bold** _dangling");
        ensure_equals("signature", signature(spans), "DBDP");
        ensure_equals("bold", spans[1].mText, "bold");
        ensure_equals("suffix", spans[3].mText, " _dangling");
    }
}

#include "linden_common.h"
#include "llvkfontface.h"
#include "llvkfont.h"
#include "llvkfontsvg.h"
#include "llvkglyphatlas.h"
#include "llvkfontregistry.h"
#include "lltut.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <vector>

namespace
{
    void write16(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint16_t value)
    {
        bytes.at(offset) = static_cast<std::uint8_t>(value >> 8);
        bytes.at(offset + 1) = static_cast<std::uint8_t>(value);
    }

    void write32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
    {
        write16(bytes, offset, static_cast<std::uint16_t>(value >> 16));
        write16(bytes, offset + 2, static_cast<std::uint16_t>(value));
    }

    std::uint32_t checksum(const std::vector<std::uint8_t>& bytes)
    {
        std::uint32_t result = 0;
        for (std::size_t offset = 0; offset < bytes.size(); offset += 4)
        {
            std::uint32_t word = 0;
            for (std::size_t index = 0; index < 4; ++index)
                word = (word << 8) | (offset + index < bytes.size() ? bytes[offset + index] : 0);
            result += word;
        }
        return result;
    }

    std::vector<std::uint8_t> svgFont(FT_Face face, FT_UInt glyphIndex, const std::string& svg)
    {
        std::map<FT_ULong, std::vector<std::uint8_t>> tables;
        for (FT_UInt index = 0; ; ++index)
        {
            FT_ULong tag = 0;
            FT_ULong length = 0;
            if (FT_Sfnt_Table_Info(face, index, &tag, &length)) break;
            auto& table = tables[tag];
            table.resize(length);
            tut::ensure_equals("extract font table", FT_Load_Sfnt_Table(face, tag, 0, table.data(), &length), 0);
        }
        auto& svgTable = tables[FT_MAKE_TAG('S', 'V', 'G', ' ')];
        svgTable.resize(24 + svg.size());
        write32(svgTable, 2, 10);
        write16(svgTable, 10, 1);
        write16(svgTable, 12, static_cast<std::uint16_t>(glyphIndex));
        write16(svgTable, 14, static_cast<std::uint16_t>(glyphIndex));
        write32(svgTable, 16, 14);
        write32(svgTable, 20, static_cast<std::uint32_t>(svg.size()));
        std::copy(svg.begin(), svg.end(), svgTable.begin() + 24);
        write32(tables.at(FT_MAKE_TAG('h', 'e', 'a', 'd')), 8, 0);
        const auto count = static_cast<std::uint16_t>(tables.size());
        std::uint16_t power = 1;
        std::uint16_t selector = 0;
        while (power * 2 <= count) { power *= 2; ++selector; }
        std::vector<std::uint8_t> result(12 + count * 16);
        write32(result, 0, 0x00010000);
        write16(result, 4, count);
        write16(result, 6, power * 16);
        write16(result, 8, selector);
        write16(result, 10, (count - power) * 16);
        std::size_t directory = 12;
        std::size_t headOffset = 0;
        for (const auto& [tag, table] : tables)
        {
            const auto offset = result.size();
            write32(result, directory, static_cast<std::uint32_t>(tag));
            write32(result, directory + 4, checksum(table));
            write32(result, directory + 8, static_cast<std::uint32_t>(offset));
            write32(result, directory + 12, static_cast<std::uint32_t>(table.size()));
            if (tag == FT_MAKE_TAG('h', 'e', 'a', 'd')) headOffset = offset;
            result.insert(result.end(), table.begin(), table.end());
            result.resize((result.size() + 3) & ~std::size_t(3));
            directory += 16;
        }
        write32(result, headOffset + 8, 0xb1b0afba - checksum(result));
        return result;
    }
}

namespace tut
{
    struct fontface_data
    {
        std::vector<std::uint8_t> bytes;
        fontface_data()
        {
            std::ifstream stream(LLVK_FONT_FIXTURE, std::ios::binary);
            ensure("packaged font opens", stream.good());
            bytes.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
        }
    };
    typedef test_group<fontface_data> fontface_group;
    typedef fontface_group::object object;
    fontface_group fontface_tests("llvkfontface");

    template<> template<>
    void object::test<17>()
    {
        set_test_name("all mapped packaged Twemoji glyphs use native SVG ownership");
        std::ifstream stream(LLVK_EMOJI_FIXTURE,std::ios::binary);
        ensure("packaged Twemoji opens",stream.good());
        std::vector<std::uint8_t> emojiBytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        FT_Library library = nullptr;
        FT_Face reference = nullptr;
        ensure_equals("emoji reference library",FT_Init_FreeType(&library),0);
        auto releaseLibrary = [](FT_LibraryRec_* value) { FT_Done_FreeType(value); };
        std::unique_ptr<FT_LibraryRec_,decltype(releaseLibrary)> ownedLibrary(library,releaseLibrary);
        ensure_equals("emoji reference face",FT_New_Memory_Face(library,emojiBytes.data(),static_cast<FT_Long>(emojiBytes.size()),0,&reference),0);
        auto releaseFace = [](FT_FaceRec_* value) { FT_Done_Face(value); };
        std::unique_ptr<FT_FaceRec_,decltype(releaseFace)> ownedFace(reference,releaseFace);
        std::string error;
        auto native = LLVKFontFace::create(emojiBytes,{},error);
        ensure(error,native != nullptr);
        std::set<FT_UInt> rendered;
        FT_UInt index = 0;
        auto codepoint = FT_Get_First_Char(reference,&index);
        std::size_t colored = 0;
        while (index)
        {
            if (rendered.insert(index).second)
            {
                auto glyph = native->rasterize(static_cast<char32_t>(codepoint),true,error);
                ensure("Twemoji codepoint " + std::to_string(codepoint) + ": " + error,glyph.has_value());
                if (glyph->encoding == LLVKFontFace::PixelEncoding::PremultipliedSrgbRgba8)
                {
                    ++colored;
                    ensure("owned color payload",glyph->bottomUpPixels.size() == std::size_t(glyph->width)*glyph->height*4);
                    for (std::size_t offset = 0; offset < glyph->bottomUpPixels.size(); offset += 4)
                        for (std::size_t channel = 0; channel < 3; ++channel)
                            ensure("premultiplied channel bound",glyph->bottomUpPixels[offset+channel] <= glyph->bottomUpPixels[offset+3]);
                }
            }
            codepoint = FT_Get_Next_Char(reference,codepoint,&index);
        }
        ensure("real color glyph coverage",colored > 100);
        std::cout << "Native Twemoji mapped glyphs: " << rendered.size() << ", color: " << colored << '\n';
    }

    template<> template<>
    void object::test<18>()
    {
        set_test_name("packaged variable font weights have independent native state");
        std::ifstream stream(LLVK_VARIABLE_FIXTURE,std::ios::binary);
        ensure("packaged variable font opens",stream.good());
        std::vector<std::uint8_t> fontBytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        LLVKFontFace::Options options;
        options.weight = 300;
        auto light = LLVKFontFace::create(fontBytes,options,error);
        ensure(error,light != nullptr);
        options.weight = 700;
        auto heavy = LLVKFontFace::create(fontBytes,options,error);
        ensure(error,heavy != nullptr);
        ensure("weight axes applied",light->metrics().weightApplied && heavy->metrics().weightApplied);
        ensure("heavy style recognized",!light->metrics().bold && heavy->metrics().bold);
        auto lightGlyph = light->rasterize(U'A',false,error);
        auto heavyGlyph = heavy->rasterize(U'A',false,error);
        ensure("both glyphs",lightGlyph && heavyGlyph);
        ensure("weight affects raster",lightGlyph->bottomUpPixels != heavyGlyph->bottomUpPixels);
        heavy.reset();
        auto repeated = light->rasterize(U'A',false,error);
        ensure("light face unchanged",repeated && repeated->bottomUpPixels == lightGlyph->bottomUpPixels);
    }

    template<> template<>
    void object::test<16>()
    {
        set_test_name("native font declarations resolve without GL defaults or callbacks");
        std::string error;
        LLVKFontRegistry::Configuration configuration;
        configuration.platform = "Windows";
        configuration.searchDirectories = {std::filesystem::path(LLVK_FONT_FIXTURE).parent_path()};
        configuration.sizeAdjustment = 0.5f;
        std::vector<std::string> documents{
            "<fonts><font_size name='Default' size='12'/><font name='NativeSans'><os name='Linux'><file>Roboto-Bold.ttf</file></os><file>missing.ttf</file><os name='Windows'><file>Roboto-Regular.ttf</file></os></font></fonts>"};
        auto registry = LLVKFontRegistry::create(documents,configuration,error);
        ensure(error,registry != nullptr);
        auto font = registry->resolve({"NativeSans","Default"},error);
        ensure(error,font != nullptr);
        ensure("OS filter selected regular",!font->metrics().bold);
        auto again = registry->resolve({"NativeSans","Default"},error);
        const auto oldGlyph=font->glyph(U'A',false,error);
        ensure("live scaled registry prepares replacement",registry->setDisplayScale(1.25f,96.f,96.f,error));
        ensure("live scale preserves control font identity",registry->resolve({"NativeSans","Default"},error)==font);
        const auto scaledGlyph=font->glyph(U'A',false,error);
        ensure("live scale publishes a new raster version",scaledGlyph && oldGlyph && scaledGlyph!=oldGlyph);
        ensure("old raster remains owned for submitted draws",!oldGlyph->raster.bottomUpPixels.empty());
        ensure("invalid scale leaves current font intact",!registry->setDisplayScale(0.f,96.f,96.f,error) && font->displayScale()==1.25f);
        ensure("restore scale",registry->setDisplayScale(1.f,96.f,96.f,error));
        ensure("cache identity",again == font);
        LLVKFontFace::Options options;
        options.pointSize = 12.5f;
        auto reference = LLVKFontFace::create(bytes,options,error);
        ensure(error,reference != nullptr);
        ensure_equals("size adjustment",font->metrics().lineHeight,reference->metrics().lineHeight);
        documents.push_back("<fonts><font name='NativeSans'><file flags='bold'>Roboto-Bold.ttf</file></font></fonts>");
        auto overridden = LLVKFontRegistry::create(documents,configuration,error);
        ensure(error,overridden != nullptr);
        auto bold = overridden->resolve({"NativeSans","Default"},error);
        ensure(error,bold != nullptr);
        ensure("later files prepended",bold->metrics().bold);
        ensure("prior registry unchanged",!font->metrics().bold);
        auto normalized = LLVKFontRegistry::normalize({"SansSerifHugeBoldItalic","",4,true});
        ensure_equals("legacy family",normalized.name,std::string("SansSerif"));
        ensure_equals("legacy size",normalized.size,std::string("Huge"));
        ensure_equals("font style",normalized.style,std::uint8_t(3));
        ensure("tabular policy retained",normalized.tabularNumbers);
        ensure("unknown size fails",!registry->resolve({"NativeSans","Missing"},error));
        documents = {"<!DOCTYPE fonts [<!ENTITY external SYSTEM 'file:///not-read'>]><fonts/>"};
        ensure("DTD rejected",!LLVKFontRegistry::create(documents,configuration,error));
        documents = {"<fonts><font>"};
        ensure("malformed XML rejected",!LLVKFontRegistry::create(documents,configuration,error));
        registry.reset();
        ensure("resolved owner survives registry",font->glyph(U'A',false,error) != nullptr);
    }

    template<> template<>
    void object::test<15>()
    {
        set_test_name("native wrapping and hit testing preserve distinct thresholds");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        auto glyph = font->glyph(U'A', false, error);
        ensure(error, glyph != nullptr);
        const float advance = glyph->raster.advanceX;
        const float inkLimit = std::max(advance, float(glyph->raster.width) + glyph->raster.bearingX);
        auto fits = font->fitCharacters(U"A", inkLimit, 1, 1.f, LLVKFont::Wrap::Anywhere, false, error);
        ensure("exact ink fits", fits && *fits == 1);
        auto clips = font->fitCharacters(U"A", inkLimit - 0.01f, 1, 1.f, LLVKFont::Wrap::Anywhere, false, error);
        ensure("ink beyond limit clips", clips && *clips == 0);
        auto halfLeft = font->hitTest(U"AV", 0, advance * 0.49f, 100.f, 3, 1.f, true, false, error);
        auto halfRight = font->hitTest(U"AV", 0, advance * 0.5f, 100.f, 3, 1.f, true, false, error);
        ensure("left half selects start", halfLeft && *halfLeft == 0);
        ensure("half equality selects next", halfRight && *halfRight == 1);
        auto floorHit = font->hitTest(U"AV", 0, advance * 0.99f, 100.f, 3, 1.f, false, false, error);
        ensure("non-rounded waits full advance", floorHit && *floorHit == 0);
        auto bounded = font->hitTest(U"AV", 0, 100.f, 100.f, 1, 1.f, true, false, error);
        ensure("source count-minus-one bound", bounded && *bounded == 0);
        auto suffixHit = font->hitTest(U"xAV", 1, advance * 0.5f, 100.f, 3, 1.f, true, false, error);
        ensure("relative source offset", suffixHit && *suffixHit == 1);
        auto back = font->firstVisible(U"AV", 1, 100.f, 2, 1.f, false, error);
        ensure("backward full extent", back && *back == 0);
        auto last = font->firstVisible(U"AV", 1, 0.f, 2, 1.f, false, error);
        ensure("backward no-fit anchor", last && *last == 1);
        auto prefix = font->measureRun(U"AA B", 0, 4, 1.f, true, false, error);
        ensure(error, prefix.has_value());
        auto anywhere = font->fitCharacters(U"AA BB", prefix->width + 0.01f, 5, 1.f, LLVKFont::Wrap::Anywhere, false, error);
        auto word = font->fitCharacters(U"AA BB", prefix->width + 0.01f, 5, 1.f, LLVKFont::Wrap::WordsOnly, false, error);
        ensure("partial last word fits", anywhere && *anywhere == 4);
        ensure("word boundary rolls back", word && *word == 3);
        auto strict = font->fitCharacters(U"AAAA", inkLimit, 4, 1.f, LLVKFont::Wrap::WordsOnly, false, error);
        auto possible = font->fitCharacters(U"AAAA", inkLimit, 4, 1.f, LLVKFont::Wrap::WordsWhenPossible, false, error);
        ensure("strict long first word", strict && *strict == 0);
        ensure("fallback splits first word", possible && *possible == 1);
    }

    template<> template<>
    void object::test<14>()
    {
        set_test_name("immutable native atlas preserves coverage gutters and painter order");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        auto line = font->layoutLine(U"A A", 0, 3, {}, error);
        ensure(error, line.has_value());
        auto atlas = LLVKGlyphAtlas::prepare(*line, 32, 4096, error);
        ensure(error, atlas.has_value());
        ensure_equals("one page", atlas->pages().size(), std::size_t(1));
        ensure_equals("all placements retained", atlas->placements().size(), std::size_t(3));
        ensure("space needs no image", !atlas->placements()[1].page);
        ensure_equals("repeated glyph UV", atlas->placements()[0].leftU, atlas->placements()[2].leftU);
        ensure_equals("top gutter UV", atlas->placements()[0].topV,
                      (1.5f + line->glyphs.front().glyph->raster.height) / 32.f);
        const auto& page = atlas->pages().front().rgba;
        ensure_equals("gutter white", page[0], std::uint8_t(255));
        ensure_equals("gutter transparent", page[3], std::uint8_t(0));
        const auto& glyph = line->glyphs.front().glyph->raster;
        for (std::uint32_t row = 0; row < glyph.height; ++row)
            for (std::uint32_t column = 0; column < glyph.width; ++column)
                ensure_equals("bottom-up glyph alpha", page[((row + 1) * 32 + column + 1) * 4 + 3],
                              glyph.bottomUpPixels[row * glyph.width + column]);
        ensure("budget failure explicit", !LLVKGlyphAtlas::prepare(*line, 32, 4095, error));
        ensure("oversized glyph explicit", !LLVKGlyphAtlas::prepare(*line, 4, 4096, error));
        line.reset();
        font.reset();
        ensure("atlas retains glyph owners", !atlas->placements().front().draw.glyph->raster.bottomUpPixels.empty());
    }

    template<> template<>
    void object::test<12>()
    {
        set_test_name("draw layout uses explicit transforms and outside-range kerning");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        auto face = LLVKFontFace::create(bytes, {}, error);
        ensure(error, face != nullptr);
        LLVKFont::LineOptions options;
        options.x = 3.25f;
        options.y = 20.5f;
        options.originX = -0.5f;
        options.originY = 1.25f;
        options.scaleX = 1.5f;
        options.scaleY = 2.f;
        auto line = font->layoutLine(U"AV", 0, 1, options, error);
        ensure(error, line.has_value());
        auto leftGlyph = face->rasterize(U'A', false, error);
        auto rightGlyph = face->rasterize(U'V', false, error);
        ensure("reference glyphs", leftGlyph && rightGlyph);
        auto kern = face->kerning(leftGlyph->requestedIndex, rightGlyph->requestedIndex,
                                  leftGlyph->rsbDelta, rightGlyph->lsbDelta, error);
        ensure(error, kern.has_value());
        const float initialX = options.x * options.scaleX + std::floor(options.originX * options.scaleX);
        const float initialY = options.y * options.scaleY + std::floor(options.originY * options.scaleY);
        ensure_equals("range count", line->sourceCharacters, std::size_t(1));
        ensure_equals("lookahead affects end pen", line->endPixelX,
                      std::floor(initialX + leftGlyph->advanceX + *kern + 0.5f));
        ensure_equals("snapped left", line->glyphs.front().left, std::floor(initialX + leftGlyph->bearingX + 0.5f));
        ensure_equals("snapped top", line->glyphs.front().top, std::floor(initialY + leftGlyph->bearingY + 0.5f));
        ensure_equals("glyph width", line->glyphs.front().right - line->glyphs.front().left, float(leftGlyph->width));
        font.reset();
        ensure("layout retains raster", !line->glyphs.front().glyph->raster.bottomUpPixels.empty());
        LLVKFontFace::Options scaledFace;
        scaledFace.horizontalDpi=scaledFace.verticalDpi=120.f;
        const auto deviceFont=LLVKFont::create({bytes,scaledFace},{},false,error);
        const auto logicalFont=LLVKFont::create({bytes,scaledFace},{},false,error,1.25f);
        ensure(error,deviceFont && logicalFont);
        LLVKFont::LineOptions logicalOptions;
        logicalOptions.x=8.f; logicalOptions.y=20.f;
        auto deviceOptions=logicalOptions; deviceOptions.scaleX=deviceOptions.scaleY=1.25f;
        const auto deviceLine=deviceFont->layoutLine(U"AV",0,2,deviceOptions,error);
        const auto logicalLine=logicalFont->layoutLine(U"AV",0,2,logicalOptions,error);
        ensure(error,deviceLine && logicalLine);
        ensure_equals("logical glyph maps to scaled raster",logicalLine->glyphs[0].left*1.25f,deviceLine->glyphs[0].left);
        ensure_equals("scaled raster stays full resolution",logicalLine->glyphs[0].glyph->raster.width,deviceLine->glyphs[0].glyph->raster.width);
        const auto deviceWidth=deviceFont->measureRun(U"AV",0,2,1.25f,true,false,error);
        const auto logicalWidth=logicalFont->measureRun(U"AV",0,2,1.f,true,false,error);
        ensure("logical widths use scaled glyph metrics",deviceWidth && logicalWidth && deviceWidth->width==logicalWidth->width);
    }

    template<> template<>
    void object::test<13>()
    {
        set_test_name("native line alignment clipping and ellipsis are explicit");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        LLVKFont::LineOptions options;
        options.x = 100.f;
        options.y = 100.f;
        options.maxPixels = 11;
        options.horizontal = LLVKFont::HorizontalAlign::Center;
        options.vertical = LLVKFont::VerticalAlign::Top;
        auto centered = font->layoutLine(U"AVAV", 0, 4, options, error);
        ensure(error, centered.has_value());
        ensure_equals("integer half width", centered->startPixelX, 95.f);
        ensure_equals("top baseline", centered->baselinePixelY, 100.f - std::ceil(font->metrics().ascender));
        ensure("clipped", centered->truncated);
        options.horizontal = LLVKFont::HorizontalAlign::Right;
        options.vertical = LLVKFont::VerticalAlign::Bottom;
        auto right = font->layoutLine(U"AVAV", 0, 4, options, error);
        ensure(error, right.has_value());
        ensure_equals("right width", right->startPixelX, 89.f);
        ensure_equals("bottom baseline", right->baselinePixelY, 100.f + std::ceil(font->metrics().descender));
        options = {};
        options.maxPixels = 40;
        options.ellipses = true;
        auto abbreviated = font->layoutLine(U"AVAVAVAVAVAV", 0, 12, options, error);
        ensure(error, abbreviated.has_value());
        ensure("ellipsis requested by measured overflow", abbreviated->ellipsized);
        ensure("source clipped", abbreviated->sourceCharacters < 12);
        ensure_equals("three rendered dots", std::count_if(abbreviated->glyphs.begin(), abbreviated->glyphs.end(),
                      [](const auto& placement) { return placement.ellipsis; }), std::ptrdiff_t(3));
        ensure_equals("synthetic source anchor", abbreviated->glyphs.back().sourceIndex, abbreviated->sourceCharacters);
        options.maxPixels = 0;
        options.ellipses = false;
        auto clipped = font->layoutLine(U"A", 0, 1, options, error);
        ensure("zero width clips ink", clipped && clipped->glyphs.empty());
        options.scaleY = 0.f;
        ensure("invalid scale", !font->layoutLine(U"A", 0, 1, options, error));
    }

    template<> template<>
    void object::test<10>()
    {
        set_test_name("native measured run preserves source range rounding and padding");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        auto reference = LLVKFontFace::create(bytes, {}, error);
        ensure(error, reference != nullptr);
        const std::u32string text = U"xAVgf";
        auto run = font->measureRun(text, 1, 3, 1.25f, true, false, error);
        ensure(error, run.has_value());
        float expectedAdvance = 0.f;
        float expectedPadding = 0.f;
        for (std::size_t index = 1; index < 4; ++index)
        {
            auto current = reference->rasterize(text[index], false, error);
            ensure(error, current.has_value());
            ensure_equals("source index", run->glyphs[index - 1].sourceIndex, index);
            ensure_equals("pen origin", run->glyphs[index - 1].penX, expectedAdvance);
            const float advance = current->advanceX;
            expectedPadding = std::max({0.f, expectedPadding - advance,
                                       static_cast<float>(current->width) + current->bearingX - advance});
            expectedAdvance += advance;
            if (index + 1 < 4)
            {
                auto next = reference->rasterize(text[index + 1], false, error);
                ensure(error, next.has_value());
                auto kern = reference->kerning(current->requestedIndex, next->requestedIndex,
                                               current->rsbDelta, next->lsbDelta, error);
                ensure(error, kern.has_value());
                expectedAdvance += *kern;
            }
            expectedAdvance = std::floor(expectedAdvance + 0.5f);
        }
        ensure_equals("advance", run->advancePixels, expectedAdvance);
        ensure_equals("padding", run->trailingPaddingPixels, expectedPadding);
        ensure_equals("scaled width", run->width, (expectedAdvance + expectedPadding) / 1.25f);
        auto unpadded = font->measureRun(text, 1, 3, 1.f, false, false, error);
        ensure(error, unpadded.has_value());
        ensure_equals("no padding", unpadded->width, expectedAdvance);
        auto empty = font->measureRun(text, text.size(), std::numeric_limits<std::size_t>::max(), 1.f, true, false, error);
        ensure("empty range", empty && empty->glyphs.empty() && empty->width == 0.f);
        ensure("invalid range", !font->measureRun(text, text.size() + 1, 1, 1.f, true, false, error));
        ensure("invalid scale", !font->measureRun(text, 0, 1, 0.f, true, false, error));
        const std::u32string terminated{U'A', 0, U'V'};
        auto terminatedRun = font->measureRun(terminated, 0, 3, 1.f, true, false, error);
        ensure("terminator honored", terminatedRun && terminatedRun->glyphs.size() == 1);
        font.reset();
        ensure("run owns glyphs after font teardown", !run->glyphs.front().glyph->raster.bottomUpPixels.empty());
    }

    template<> template<>
    void object::test<11>()
    {
        set_test_name("tabular measurement uses complete digit set and weight policy");
        std::string error;
        LLVKFont::FaceSource source{bytes, {}};
        source.options.weight = 500;
        auto font = LLVKFont::create(source, {}, false, error);
        ensure(error, font != nullptr);
        float widest = 0.f;
        for (char32_t codepoint = U'0'; codepoint <= U'9'; ++codepoint)
        {
            auto digit = font->glyph(codepoint, false, error);
            ensure(error, digit != nullptr);
            widest = std::max(widest, digit->raster.advanceX);
        }
        auto run = font->measureRun(U"12345", 0, 5, 1.f, false, true, error);
        ensure(error, run.has_value());
        ensure_equals("equal digit advances", run->advancePixels, std::floor(widest + 0.5f) * 5.f);
        for (const auto& positioned : run->glyphs)
            ensure_equals("digit centered", positioned.digitOffsetX,
                          (widest - positioned.glyph->raster.advanceX) * 0.5f);
        auto proportional = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, proportional != nullptr);
        auto enabled = proportional->measureRun(U"12345", 0, 5, 1.f, false, true, error);
        auto disabled = proportional->measureRun(U"12345", 0, 5, 1.f, false, false, error);
        ensure("both measured", enabled.has_value() && disabled.has_value());
        ensure_equals("nonpositive weight excludes tabular override", enabled->width, disabled->width);
    }

    template<> template<>
    void object::test<9>()
    {
        set_test_name("SVG font table reaches native callbacks and owned color glyphs");
        FT_Library library = nullptr;
        FT_Face reference = nullptr;
        ensure_equals("library", FT_Init_FreeType(&library), 0);
        auto releaseLibrary = [](FT_LibraryRec_* value) { FT_Done_FreeType(value); };
        std::unique_ptr<FT_LibraryRec_, decltype(releaseLibrary)> ownedLibrary(library, releaseLibrary);
        ensure_equals("face", FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &reference), 0);
        auto releaseFace = [](FT_FaceRec_* value) { FT_Done_Face(value); };
        std::unique_ptr<FT_FaceRec_, decltype(releaseFace)> ownedFace(reference, releaseFace);
        const auto index = FT_Get_Char_Index(reference, 'A');
        const std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'><rect width='16' height='8' fill='#ff0000' fill-opacity='0.5'/><rect y='8' width='16' height='8' fill='#0000ff'/></svg>";
        auto fixture = svgFont(reference, index, svg);
        ensure_equals("valid SFNT checksum", checksum(fixture), std::uint32_t(0xb1b0afba));
        std::string error;
        auto native = LLVKFontFace::create(fixture, {}, error);
        ensure(error, native != nullptr);
        auto independent = LLVKFontFace::create(fixture, {}, error);
        ensure(error, independent != nullptr);
        fixture.clear();
        fixture.shrink_to_fit();
        const auto glyph = native->rasterize(U'A', true, error);
        ensure(error, glyph.has_value());
        ensure("color glyph", glyph->encoding == LLVKFontFace::PixelEncoding::PremultipliedSrgbRgba8);
        ensure_equals("SVG width", glyph->width, 16u);
        ensure_equals("SVG height", glyph->height, 16u);
        const auto bottom = (2 * 16 + 8) * 4;
        const auto top = (13 * 16 + 8) * 4;
        ensure_equals("bottom blue", glyph->bottomUpPixels[bottom + 2], std::uint8_t(255));
        ensure_equals("bottom alpha", glyph->bottomUpPixels[bottom + 3], std::uint8_t(255));
        ensure_equals("top red", glyph->bottomUpPixels[top], std::uint8_t(127));
        ensure_equals("top alpha", glyph->bottomUpPixels[top + 3], std::uint8_t(127));
        const auto ordinary = native->rasterize(U'B', false, error);
        ensure(error, ordinary.has_value());
        const auto repeated = native->rasterize(U'A', true, error);
        ensure(error, repeated.has_value());
        ensure("repeat pixels", repeated->bottomUpPixels == glyph->bottomUpPixels);
        const auto other = independent->rasterize(U'A', true, error);
        ensure(error, other.has_value());
        ensure("independent library pixels", other->bottomUpPixels == glyph->bottomUpPixels);
        independent.reset();
        native.reset();
        ensure_equals("owned after teardown", glyph->bottomUpPixels[top], std::uint8_t(127));
    }

    template<> template<>
    void object::test<8>()
    {
        set_test_name("native SVG hooks preserve sizing and premultiplied color");
        const auto& hooks = LLVKFontSvg::hooks();
        FT_Pointer state = nullptr;
        ensure_equals("initialize", hooks.init_svg(&state), 0);
        auto cleanup = [&hooks](void* value) { hooks.free_svg(&value); };
        std::unique_ptr<void, decltype(cleanup)> owner(state, cleanup);
        std::string svg = "<svg xmlns='http://www.w3.org/2000/svg' width='16' height='16'><rect width='16' height='16' fill='#ff0000' fill-opacity='0.5'/></svg>";
        FT_SVG_DocumentRec document{};
        document.svg_document = reinterpret_cast<FT_Byte*>(svg.data());
        document.svg_document_length = static_cast<FT_ULong>(svg.size());
        document.units_per_EM = 16;
        document.metrics.x_ppem = document.metrics.y_ppem = 16;
        document.metrics.ascender = 12 * 64;
        document.transform.xx = document.transform.yy = 65536;
        FT_GlyphSlotRec slot{};
        slot.other = &document;
        ensure_equals("preset", hooks.preset_slot(&slot, false, &state), 0);
        ensure_equals("width", slot.bitmap.width, 16u);
        ensure_equals("baseline", slot.bitmap_top, 12);
        const auto metrics = slot.metrics;
        ensure_equals("cached preset", hooks.preset_slot(&slot, true, &state), 0);
        ensure_equals("metrics stable", slot.metrics.vertBearingY, metrics.vertBearingY);
        ensure_equals("source horizontal bearing", slot.metrics.horiBearingX, FT_Pos(0));
        ensure_equals("source vertical sign", slot.metrics.horiBearingY, FT_Pos(-12 * 64));
        std::vector<unsigned char> pixels(16 * 16 * 4);
        slot.bitmap.buffer = pixels.data();
        ensure_equals("render", hooks.render_svg(&slot, &state), 0);
        const auto offset = (8 * 16 + 8) * 4;
        ensure_equals("blue", pixels[offset], static_cast<unsigned char>(0));
        ensure_equals("green", pixels[offset + 1], static_cast<unsigned char>(0));
        ensure_equals("red premultiplied", pixels[offset + 2], pixels[offset + 3]);
        ensure_equals("half alpha", pixels[offset + 3], static_cast<unsigned char>(127));
        document.delta.x = -64;
        ensure_equals("negative transform rejected", hooks.preset_slot(&slot, false, &state), FT_Err_Unimplemented_Feature);
        ensure_equals("error survives cached preset", hooks.preset_slot(&slot, true, &state), FT_Err_Unimplemented_Feature);
        ensure_equals("error survives render", hooks.render_svg(&slot, &state), FT_Err_Unimplemented_Feature);
        document.delta.x = 0;
        ensure_equals("recover", hooks.preset_slot(&slot, false, &state), 0);
        document.end_glyph_id = 1;
        ensure_equals("multi-glyph document explicit", hooks.preset_slot(&slot, false, &state), FT_Err_Unimplemented_Feature);
        document.end_glyph_id = 0;
        document.svg_document_length = 4 * 1024 * 1024 + 1;
        ensure_equals("oversized document", hooks.preset_slot(&slot, false, &state), FT_Err_Invalid_SVG_Document);
        document.svg_document_length = static_cast<FT_ULong>(svg.size());
        document.metrics.x_ppem = document.metrics.y_ppem = 4097;
        ensure_equals("oversized bitmap", hooks.preset_slot(&slot, false, &state), FT_Err_Invalid_SVG_Document);
        document.metrics.x_ppem = document.metrics.y_ppem = 16;
        svg = "<svg width='0.5' height='16'/>";
        document.svg_document = reinterpret_cast<FT_Byte*>(svg.data());
        document.svg_document_length = static_cast<FT_ULong>(svg.size());
        ensure_equals("fractional dimension cannot divide by zero", hooks.preset_slot(&slot, false, &state), FT_Err_Invalid_SVG_Document);
    }

    template<> template<>
    void object::test<1>()
    {
        set_test_name("owned face metrics without graphics initialization");
        std::string error;
        LLVKFontFace::Options options;
        options.pointSize = 10.125f;
        options.horizontalDpi = 120.f;
        options.verticalDpi = 144.f;
        auto native = LLVKFontFace::create(bytes, options, error);
        ensure(error, native != nullptr);
        FT_Library library = nullptr;
        FT_Face reference = nullptr;
        ensure_equals("reference library", FT_Init_FreeType(&library), 0);
        auto releaseLibrary = [](FT_LibraryRec_* value) { FT_Done_FreeType(value); };
        std::unique_ptr<FT_LibraryRec_, decltype(releaseLibrary)> ownedLibrary(library, releaseLibrary);
        ensure_equals("reference face", FT_New_Memory_Face(library, bytes.data(),
                      static_cast<FT_Long>(bytes.size()), 0, &reference), 0);
        auto releaseFace = [](FT_FaceRec_* value) { FT_Done_Face(value); };
        std::unique_ptr<FT_FaceRec_, decltype(releaseFace)> ownedFace(reference, releaseFace);
        const float scale = ((options.pointSize / 72.f) * options.verticalDpi) * (1.f / reference->units_per_EM);
        ensure_equals("ascender", native->metrics().ascender, reference->ascender * scale);
        ensure_equals("descender", native->metrics().descender, -reference->descender * scale);
        ensure_equals("line height", native->metrics().lineHeight, reference->height * scale);
        ensure("regular face", !native->metrics().bold && !native->metrics().italic);
    }

    template<> template<>
    void object::test<2>()
    {
        set_test_name("invalid inputs fail without poisoning subsequent creation");
        std::string error;
        LLVKFontFace::Options options;
        ensure("empty rejected", !LLVKFontFace::create({}, options, error));
        ensure("failure explained", !error.empty());
        const std::vector<std::uint8_t> corrupt{1, 2, 3, 4};
        ensure("corrupt rejected", !LLVKFontFace::create(corrupt, options, error));
        for (float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::max()})
        {
            options.pointSize = invalid;
            ensure("invalid size rejected", !LLVKFontFace::create(bytes, options, error));
            options = {};
            options.horizontalDpi = invalid;
            ensure("invalid DPI rejected", !LLVKFontFace::create(bytes, options, error));
            options = {};
        }
        ensure("valid after failures", LLVKFontFace::create(bytes, options, error) != nullptr);
        ensure("success clears error", error.empty());
    }

    template<> template<>
    void object::test<3>()
    {
        set_test_name("native instances own bytes and independent settings");
        std::string error;
        LLVKFontFace::Options options;
        auto first = LLVKFontFace::create(bytes, options, error);
        ensure(error, first != nullptr);
        const auto metrics = first->metrics();
        options.verticalDpi *= 2.f;
        options.descriptorBold = true;
        options.weight = 700;
        auto second = LLVKFontFace::create(bytes, options, error);
        ensure(error, second != nullptr);
        bytes.clear();
        bytes.shrink_to_fit();
        const auto glyph = first->rasterize(U'A', false, error);
        ensure("glyph after releasing caller bytes", glyph.has_value() && glyph->advanceX > 0.f);
        ensure_equals("second scale", second->metrics().lineHeight, metrics.lineHeight * 2.f);
        ensure("descriptor bold", second->metrics().bold);
        ensure("static font has no weight axis", !second->metrics().weightApplied);
        second.reset();
        ensure_equals("first unaffected", first->metrics().lineHeight, metrics.lineHeight);
        ensure("first style unaffected", !first->metrics().bold);
    }

    template<> template<>
    void object::test<4>()
    {
        set_test_name("glyph pixels and metrics match independent FreeType slot");
        std::string error;
        FT_Library library = nullptr;
        FT_Face reference = nullptr;
        ensure_equals("library", FT_Init_FreeType(&library), 0);
        auto releaseLibrary = [](FT_LibraryRec_* value) { FT_Done_FreeType(value); };
        std::unique_ptr<FT_LibraryRec_, decltype(releaseLibrary)> ownedLibrary(library, releaseLibrary);
        ensure_equals("face", FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &reference), 0);
        auto releaseFace = [](FT_FaceRec_* value) { FT_Done_Face(value); };
        std::unique_ptr<FT_FaceRec_, decltype(releaseFace)> ownedFace(reference, releaseFace);
        for (auto hinting : {LLVKFontFace::Hinting::Default, LLVKFontFace::Hinting::ForceAutohint,
                             LLVKFontFace::Hinting::DisableAutohint})
        {
            LLVKFontFace::Options options;
            options.hinting = hinting;
            options.pointSize = 10.123f;
            options.horizontalDpi = 119.9f;
            options.verticalDpi = 143.9f;
            auto native = LLVKFontFace::create(bytes, options, error);
            ensure(error, native != nullptr);
            ensure_equals("size", FT_Set_Char_Size(reference, 0, static_cast<FT_F26Dot6>(options.pointSize * 64.f), 119, 143), 0);
            const FT_Int32 flags = hinting == LLVKFontFace::Hinting::Default ? FT_LOAD_DEFAULT :
                                  hinting == LLVKFontFace::Hinting::ForceAutohint ? FT_LOAD_FORCE_AUTOHINT : FT_LOAD_NO_AUTOHINT;
            for (char32_t codepoint : {U'A', U'V', U'g', U' ', U'\u00e9', char32_t(0), char32_t(0x10ffff)})
            {
                const auto glyph = native->rasterize(codepoint, false, error);
                ensure(error, glyph.has_value());
                const auto index = FT_Get_Char_Index(reference, static_cast<FT_ULong>(codepoint));
                ensure_equals("lookup", native->glyphIndex(codepoint), index);
                ensure_equals("requested index", glyph->requestedIndex, index);
                ensure_equals("actual index", glyph->renderedIndex, index);
                ensure_equals("load", FT_Load_Glyph(reference, index, flags), 0);
                ensure_equals("render", FT_Render_Glyph(reference->glyph, FT_RENDER_MODE_NORMAL), 0);
                const auto slot = reference->glyph;
                ensure_equals("advance", glyph->advanceX, slot->advance.x / 64.f);
                ensure_equals("bearing X", glyph->bearingX, slot->bitmap_left);
                ensure_equals("bearing Y", glyph->bearingY, slot->bitmap_top);
                ensure_equals("lsb", glyph->lsbDelta, static_cast<std::int32_t>(slot->lsb_delta));
                ensure_equals("rsb", glyph->rsbDelta, static_cast<std::int32_t>(slot->rsb_delta));
                ensure_equals("width", glyph->width, slot->bitmap.width);
                ensure_equals("height", glyph->height, slot->bitmap.rows);
                for (unsigned row = 0; row < glyph->height; ++row)
                    for (unsigned column = 0; column < glyph->width; ++column)
                        ensure_equals("bottom-up coverage", glyph->bottomUpPixels[row * glyph->width + column],
                                      slot->bitmap.buffer[(glyph->height - 1 - row) * slot->bitmap.pitch + column]);
                const auto colorRequested = native->rasterize(codepoint, true, error);
                ensure(error, colorRequested.has_value());
                ensure("color request can return coverage", colorRequested->encoding == LLVKFontFace::PixelEncoding::Coverage8);
                ensure("slot changes cannot mutate result", glyph->bottomUpPixels == colorRequested->bottomUpPixels);
            }
        }
    }

    template<> template<>
    void object::test<5>()
    {
        set_test_name("exact side-bearing thresholds and result lifetime");
        std::string error;
        auto native = LLVKFontFace::create(bytes, {}, error);
        ensure(error, native != nullptr);
        auto glyph = native->rasterize(U'A', false, error);
        ensure(error, glyph.has_value());
        const auto originalPixels = glyph->bottomUpPixels;
        const auto index = glyph->requestedIndex;
        const auto base = native->kerning(index, index, 0, 0, error);
        ensure(error, base.has_value());
        for (int difference : {-33, -32, -31, 0, 31, 32, 33})
        {
            const auto value = native->kerning(index, index, difference, 0, error);
            ensure(error, value.has_value());
            ensure_equals("threshold", *value, *base + (difference > 32 ? -1.f : difference < -31 ? 1.f : 0.f));
        }
        ensure("invalid index", !native->kerning(std::numeric_limits<std::uint32_t>::max(), index, 0, 0, error));
        native.reset();
        ensure("result owns pixels", glyph->bottomUpPixels == originalPixels && !originalPixels.empty());
    }

    template<> template<>
    void object::test<6>()
    {
        set_test_name("native font primes startup glyphs without a graphics context");
        std::string error;
        auto font = LLVKFont::create({bytes, {}}, {}, false, error);
        ensure(error, font != nullptr);
        ensure_equals("default and printable ASCII", font->cachedGlyphCount(), std::size_t(96));
        const auto first = font->glyph(U'A', false, error);
        ensure(error, first != nullptr);
        ensure("cache identity", first == font->glyph(U'A', false, error));
        ensure_equals("primary owns glyph", first->faceIndex, std::size_t(0));
        const auto missing = font->glyph(char32_t(0x10ffff), false, error);
        ensure(error, missing != nullptr);
        ensure_equals("missing stays primary", missing->faceIndex, std::size_t(0));
        ensure_equals("missing glyph", missing->raster.renderedIndex, std::uint32_t(0));
        font.reset();
        ensure("retained glyph pixels", !first->raster.bottomUpPixels.empty());
    }

    template<> template<>
    void object::test<7>()
    {
        set_test_name("fallback ordering uses explicit native policy and face ownership");
        std::ifstream stream(LLVK_FALLBACK_FIXTURE, std::ios::binary);
        ensure("fallback fixture opens", stream.good());
        std::vector<std::uint8_t> fallbackBytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        std::string error;
        auto primaryFace = LLVKFontFace::create(bytes, {}, error);
        ensure(error, primaryFace != nullptr);
        auto fallbackFace = LLVKFontFace::create(fallbackBytes, {}, error);
        ensure(error, fallbackFace != nullptr);
        char32_t candidate = 0;
        for (char32_t codepoint = 0x100; codepoint < 0x10000; ++codepoint)
        {
            if (!primaryFace->glyphIndex(codepoint) && fallbackFace->glyphIndex(codepoint))
            {
                candidate = codepoint;
                break;
            }
        }
        ensure("fixture exercises fallback", candidate != 0);
        std::vector<LLVKFont::FallbackSource> fallbacks{
            {{fallbackBytes, {}}, LLVKFont::FallbackPolicy::Emoji},
            {{fallbackBytes, {}}, LLVKFont::FallbackPolicy::Unrestricted}};
        auto font = LLVKFont::create({bytes, {}}, fallbacks, false, error);
        ensure(error, font != nullptr);
        auto glyph = font->glyph(candidate, false, error);
        ensure(error, glyph != nullptr);
        ensure_equals("unrestricted precedes emoji for non-emoji", glyph->faceIndex, std::size_t(2));
        auto expected = fallbackFace->rasterize(candidate, false, error);
        ensure(error, expected.has_value());
        ensure("fallback pixels", glyph->raster.bottomUpPixels == expected->bottomUpPixels);
        ensure_equals("fallback advance", glyph->raster.advanceX, expected->advanceX);
        fallbacks.pop_back();
        auto lastResort = LLVKFont::create({bytes, {}}, fallbacks, true, error);
        ensure(error, lastResort != nullptr);
        auto lastGlyph = lastResort->glyph(candidate, false, error);
        ensure(error, lastGlyph != nullptr);
        ensure_equals("last resort ignores predicate", lastGlyph->faceIndex, std::size_t(1));
        auto ordinary = font->glyph(U'A', false, error);
        ensure(error, ordinary != nullptr);
        ensure_equals("primary always precedes fallback", ordinary->faceIndex, std::size_t(0));
        const std::u32string mixed{candidate,U'A',candidate,candidate,U' '};
        const auto measured=font->measureRun(mixed,0,mixed.size(),1.f,true,false,error);
        ensure(error,measured.has_value());
        const auto drawn=font->layoutLine(mixed,0,mixed.size(),{},error);
        ensure(error,drawn.has_value());
        ensure_equals("mixed-face measurement and drawing agree",drawn->endPixelX,measured->advancePixels);
        const auto fitted=font->fitCharacters(mixed,10000.f,mixed.size(),1.f,LLVKFont::Wrap::Anywhere,false,error);
        ensure(error,fitted.has_value());
        ensure_equals("fallback pairs fit without primary-face kerning",*fitted,mixed.size());
        auto minimalBytes=bytes;
        const auto read32=[&](std::size_t offset)
        {
            std::uint32_t value=0;
            for (std::size_t index=0; index<4; ++index) value=(value<<8)|minimalBytes.at(offset+index);
            return value;
        };
        const auto tableCount=(std::uint16_t(minimalBytes.at(4))<<8)|minimalBytes.at(5);
        bool limited=false;
        for (std::size_t table=0; table<tableCount; ++table)
        {
            const auto entry=12+16*table;
            if (read32(entry)==FT_MAKE_TAG('m','a','x','p'))
            { write16(minimalBytes,read32(entry+8)+4,1); limited=true; break; }
        }
        ensure("minimal primary fixture has glyph count table",limited);
        auto minimal=LLVKFontFace::create(minimalBytes,{},error);
        ensure(error,minimal != nullptr);
        ensure_equals("minimal primary has no A",minimal->glyphIndex(U'A'),std::uint32_t(0));
        ensure("old owner rejects fallback A",!minimal->kerning(primaryFace->glyphIndex(U'A'),primaryFace->glyphIndex(U'A'),0,0,error));
        auto limitedFont=LLVKFont::create({minimalBytes,{}},{{{bytes,{}},LLVKFont::FallbackPolicy::Unrestricted}},false,error);
        ensure(error,limitedFont != nullptr);
        const std::u32string foreignText{U'A',U'A',char32_t(0x10ffff),U'A'};
        ensure("foreign index measures on its owner",limitedFont->measureRun(foreignText,0,foreignText.size(),1.f,true,false,error).has_value());
        ensure("foreign index draws on its owner",limitedFont->layoutLine(foreignText,0,foreignText.size(),{},error).has_value());
        ensure("same fallback pairs fit on their owner",limitedFont->fitCharacters(foreignText,10000.f,foreignText.size(),1.f,LLVKFont::Wrap::Anywhere,false,error).has_value());
        fallbacks.front().source.bytes = {1, 2, 3};
        ensure("bad fallback construction fails", !LLVKFont::create({bytes, {}}, fallbacks, false, error));
        ensure("existing font unaffected", font->glyph(candidate, false, error) == glyph);
    }
}
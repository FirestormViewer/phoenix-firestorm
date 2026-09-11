#ifndef LLVKPLAINCONTROL_H
#define LLVKPLAINCONTROL_H

#include "llvklabel.h"
#include "llvkcolor.h"
#include "llvkplaintextlayout.h"
#include "llvkstyledtext.h"

struct LLVKPlainControl
{
    struct Params
    {
        LLVKPlainTextLayout::Options layout;
        LLVKFont::VerticalAlign vertical = LLVKFont::VerticalAlign::Top;
        std::int32_t verticalPadding = 0;
        std::size_t maximumBytes = 4096;
        std::optional<bool> readOnly;
        bool trackEnd = false;
        bool useEllipses = false;
        LLVKColor textColor{1,1,1,1};
        LLVKColor readOnlyColor{1,1,1,1};
        std::function<void(std::uint64_t)> clicked;
        bool showHandCursor = true;
        bool parseUrls = false;
        bool parseWebLinks = false;
        bool skipLinkUnderline = false;
        bool selectable = false;
        bool literal = false;
        bool commitOnFocusLost = false;
        LLVKColor cursorColor{1,1,1,1};
        LLVKColor selectionColor{1,1,1,1}, selectionBackground{0.2f,0.4f,0.7f,1};
        LLVKColor linkColor{0.2f,0.6f,1,1}, queryColor{0.5f,0.5f,0.5f,1};
        std::function<void(std::uint64_t,const std::string&)> linkClicked;
        LLVKColor tentativeColor{1,1,1,1};
        LLVKColor backgroundColor{0,0,0,1};
        LLVKColor readOnlyBackground{0,0,0,1};
        bool backgroundVisible = false;
    };
    Params params;
    LLVKLabel source;
    std::u32string text;
    std::vector<LLVKWebText::Link> links;
    std::optional<std::string> pressedLink;
    std::string value;
    std::uint64_t document = 0;
    std::size_t cursor = 0;
    std::optional<float> desiredCursorX;
    std::size_t selectionStart = 0;
    std::size_t selectionEnd = 0;
    bool readOnly = false;
    bool selecting = false;
    std::optional<LLVKPlainTextLayout::Document> layout;
    std::uint64_t textGeneration = 0;
};

#endif
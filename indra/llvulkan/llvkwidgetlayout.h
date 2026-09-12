#ifndef LLVKWIDGETLAYOUT_H
#define LLVKWIDGETLAYOUT_H

#include "llvkwidgettree.h"

struct LLVKWidgetLayout
{
    struct Value
    {
        std::int64_t value = 0;
        bool provided = false;
    };
    Value left, right, bottom, top, width, height;
    Value bottomDelta, topPad, topDelta, leftPad, leftDelta;
    std::string layout;

    std::optional<LLVKWidgetTree::Rect> resolve(std::string& error) const;
    std::optional<LLVKWidgetTree::Rect> apply(const LLVKWidgetTree& tree, LLVKWidgetTree::Id parent,
        const std::string& parentLayout, std::string& error,
        std::optional<LLVKWidgetTree::Rect> layoutRect = std::nullopt) const;
};

#endif
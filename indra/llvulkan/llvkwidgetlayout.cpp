#include "llvkwidgetlayout.h"

namespace
{
    bool fits(std::int64_t value) { return value >= INT32_MIN && value <= INT32_MAX; }
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetLayout::resolve(std::string& error) const
{
    error.clear();
    for (const auto* field : {&left,&right,&bottom,&top,&width,&height})
        if (!fits(field->value)) { error = "Native rectangle parameter outside 32-bit range"; return std::nullopt; }
    auto resolvedLeft = left.value;
    auto resolvedRight = right.value;
    auto resolvedBottom = bottom.value;
    auto resolvedTop = top.value;
    if (!(left.provided && right.provided) && width.provided)
    {
        if (right.provided) resolvedLeft = right.value-width.value;
        else resolvedRight = left.value+width.value;
    }
    if (!(bottom.provided && top.provided) && height.provided)
    {
        if (top.provided) resolvedBottom = top.value-height.value;
        else resolvedTop = bottom.value+height.value;
    }
    if (!fits(resolvedLeft) || !fits(resolvedRight) || !fits(resolvedBottom) || !fits(resolvedTop) ||
        !fits(resolvedRight-resolvedLeft) || !fits(resolvedTop-resolvedBottom))
    { error = "Native widget rectangle constraint overflow"; return std::nullopt; }
    return LLVKWidgetTree::Rect{static_cast<std::int32_t>(resolvedLeft),static_cast<std::int32_t>(resolvedBottom),
                               static_cast<std::int32_t>(resolvedRight),static_cast<std::int32_t>(resolvedTop)};
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetLayout::apply(const LLVKWidgetTree& tree,
    LLVKWidgetTree::Id parent, const std::string& parentLayout, std::string& error,
    std::optional<LLVKWidgetTree::Rect> layoutRect) const
{
    error.clear();
    if (!parent) return resolve(error);
    for (const auto* field : {&left,&right,&bottom,&top,&width,&height,&bottomDelta,&topPad,&topDelta,&leftPad,&leftDelta})
        if (!fits(field->value)) { error = "Native layout parameter outside 32-bit range"; return std::nullopt; }
    const auto* owner = tree.get(parent);
    if (!owner) { error = "Native layout parent is missing"; return std::nullopt; }
    auto params = *this;
    if (params.layout.empty()) params.layout = parentLayout;
    const auto parentWidth = std::int64_t(owner->params.rect.right)-owner->params.rect.left;
    const auto parentHeight = std::int64_t(owner->params.rect.top)-owner->params.rect.bottom;
    const LLVKWidgetTree::Rect local{0,0,static_cast<std::int32_t>(parentWidth),static_cast<std::int32_t>(parentHeight)};
    if (!layoutRect || layoutRect->left == layoutRect->right || layoutRect->bottom == layoutRect->top) layoutRect = local;
    const auto area = *layoutRect;
    const bool topLeft = params.layout == "topleft";
    for (Value* value : {&params.left,&params.right})
        if (value->provided) value->value += value->value >= 0 ? area.left : area.right;
    for (Value* value : {&params.bottom,&params.top})
    {
        if (value->provided)
        {
            value->value += value->value >= 0 ? area.bottom : area.top;
            if (topLeft) value->value = std::int64_t(area.bottom)+area.top-value->value;
        }
    }
    if (!params.height.provided && !params.top.provided && params.height.value == 0) params.height = {10,true};
    std::int64_t defaultLeft = 0;
    std::int64_t defaultBottom = parentHeight;
    std::int64_t defaultRight = parentWidth;
    std::int64_t defaultTop = parentHeight*2;
    for (auto child : owner->children)
    {
        const auto* previous = tree.get(child);
        if (previous->params.fromDeclaration)
        {
            defaultLeft = previous->params.rect.left;
            defaultBottom = previous->params.rect.bottom;
            defaultRight = previous->params.rect.right;
            defaultTop = previous->params.rect.top;
            break;
        }
    }
    if (topLeft)
    {
        if (params.bottomDelta.provided) params.bottomDelta.value = -params.bottomDelta.value;
        else if (params.topPad.provided) params.bottomDelta = {-(params.height.value+params.topPad.value),true};
        else if (params.topDelta.provided)
            params.bottomDelta = {-(params.topDelta.value+params.height.value-(defaultTop-defaultBottom)),true};
        else if (!params.leftDelta.provided && !params.leftPad.provided)
            params.bottomDelta = {-(params.height.value+4),false};
        else params.bottomDelta = {0,false};
        if (!params.leftDelta.provided) params.leftDelta = {0,false};
        if (params.leftPad.provided) params.leftDelta = {params.leftPad.value+defaultRight-defaultLeft,false};
    }
    else
    {
        if (!params.bottomDelta.provided) params.bottomDelta = {-(params.height.value+4),false};
        if (!params.leftDelta.provided) params.leftDelta = {0,false};
    }
    defaultLeft += params.leftDelta.value;
    defaultRight += params.leftDelta.value;
    defaultBottom += params.bottomDelta.value;
    defaultTop += params.bottomDelta.value;
    if (params.bottomDelta.provided) params.bottom = {0,false};
    if (params.leftDelta.provided) params.left = {0,false};
    if (!params.left.provided) params.left = {defaultLeft,false};
    if (!params.right.provided) params.right = {defaultRight,false};
    if (!params.bottom.provided) params.bottom = {defaultBottom,false};
    if (!params.top.provided) params.top = {defaultTop,false};
    if (!params.width.provided) params.width = {defaultRight-defaultLeft,false};
    if (!params.height.provided) params.height = {defaultTop-defaultBottom,false};
    return params.resolve(error);
}
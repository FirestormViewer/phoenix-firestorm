#include "llvkwidgettree.h"

#include <cmath>

bool LLVKBadge::Params::equals(const Params& other) const noexcept
{
    return image == other.image && borderImage == other.borderImage &&
        imageColor == other.imageColor && borderColor == other.borderColor && labelColor == other.labelColor &&
        label == other.label && labelOffsetHorizontal == other.labelOffsetHorizontal &&
        labelOffsetVertical == other.labelOffsetVertical && location == other.location &&
        offsetHorizontal.value_or(0) == other.offsetHorizontal.value_or(0) &&
        offsetVertical.value_or(0) == other.offsetVertical.value_or(0) &&
        percentHorizontal == other.percentHorizontal && percentVertical == other.percentVertical &&
        paddingHorizontal == other.paddingHorizontal && paddingVertical == other.paddingVertical;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createBadge(const Params& view,
    const LLVKControl::Params& control, const LLVKBadge::Params& params, Id owner, Id parent, std::string& error)
{
    error.clear();
    const auto location = params.location;
    if ((location & ~15) || ((location & LLVKBadge::Left) && (location & LLVKBadge::Right)) ||
        ((location & LLVKBadge::Top) && (location & LLVKBadge::Bottom)) ||
        !std::isfinite(params.paddingHorizontal) || !std::isfinite(params.paddingVertical))
    { error = "Invalid native badge location or padding"; return std::nullopt; }
    for (const auto& color : {params.imageColor,params.borderColor,params.labelColor})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native badge color"; return std::nullopt; }
    LLVKBadge badge;
    badge.params = params;
    badge.labelSource.assign(params.label);
    badge.params.label = badge.labelSource.resolveWide(mLabelContext);
    badge.owner = owner;
    if (location != LLVKBadge::Center)
    {
        const float horizontal = params.percentHorizontal * 0.01f;
        const float vertical = params.percentVertical * 0.01f;
        if (location & LLVKBadge::Right) badge.horizontalCenter = 0.5f * (1.f+horizontal);
        else if (location & LLVKBadge::Left) badge.horizontalCenter = 0.5f * (1.f-horizontal);
        if (location & LLVKBadge::Top) badge.verticalCenter = 0.5f * (1.f+vertical);
        else if (location & LLVKBadge::Bottom) badge.verticalCenter = 0.5f * (1.f-vertical);
    }
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::move(badge));
}

bool LLVKWidgetTree::attachBadge(Id id, Id parent, std::string& error)
{
    error.clear();
    const auto* badge = get(id);
    const auto* target = get(parent);
    if (!badge || !badge->badge || !target)
    { error = "Invalid native badge attachment"; return false; }
    const auto width = std::int64_t(target->params.rect.right)-target->params.rect.left;
    const auto height = std::int64_t(target->params.rect.top)-target->params.rect.bottom;
    if (width < INT32_MIN || width > INT32_MAX || height < INT32_MIN || height > INT32_MAX)
    { error = "Native badge attachment rectangle overflow"; return false; }
    ShapeChanges changes;
    Rect origin = badge->params.rect;
    origin.left = 0;
    origin.bottom = 0;
    if (!planReshape(id,width,height,origin,changes,error)) return false;
    if (!reparent(id,parent,false,0,error)) return false;
    return completeShapes(changes,error);
}

bool LLVKWidgetTree::setAcceptsBadge(Id id, bool accepts)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end()) return false;
    found->second.acceptsBadge = accepts;
    return true;
}

bool LLVKWidgetTree::attachBadgeToHolder(Id id, std::string& error)
{
    error.clear();
    const auto* badge = get(id);
    if (!badge || !badge->badge) { error = "Native holder target is not a badge"; return false; }
    const auto* owner = get(badge->badge->owner);
    if (!owner) return false;
    Id parent = owner->parent;
    while (parent)
    {
        const auto* ancestor = get(parent);
        if (ancestor->acceptsBadge) return attachBadge(id,parent,error);
        parent = ancestor->parent;
    }
    return false;
}

bool LLVKWidgetTree::setBadgeLabel(Id id, std::u32string label)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.badge) return false;
    auto source = found->second.badge->labelSource;
    source.assign(std::move(label));
    auto resolved = source.resolveWide(mLabelContext);
    found->second.badge->labelSource = std::move(source);
    found->second.badge->params.label = std::move(resolved);
    return true;
}

bool LLVKWidgetTree::setBadgeAtParentTop(Id id, bool atTop)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.badge) return false;
    found->second.badge->drawAtParentTop = atTop;
    return true;
}
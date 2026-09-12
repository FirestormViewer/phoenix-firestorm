#include "llvkwidgettree.h"

#include <cmath>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createIcon(const Params& view,
    const LLVKControl::Params& control, const LLVKIcon::Params& params, Id parent, std::string& error)
{
    error.clear();
    for (float channel : params.color)
        if (!std::isfinite(channel)) { error = "Nonfinite native icon color"; return std::nullopt; }
    LLVKIcon icon;
    icon.params = params;
    icon.image = params.image;
    return createControlImpl(view,control,std::move(icon),parent,error);
}

bool LLVKWidgetTree::registerImage(std::shared_ptr<const LLVKWidgetImage> image)
{
    if (!image || image->name().empty()) return false;
    const auto name = image->name();
    return mImages.emplace(name,std::move(image)).second;
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetTree::findImage(const std::string& name) const
{
    std::string error;
    return findImage(name,error);
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetTree::findImage(const std::string& name, std::string& error) const
{
    error.clear();
    if (name.empty() || name == "none") return nullptr;
    const auto found = mImages.find(name);
    if (found != mImages.end()) return found->second;
    return mSkinImages ? mSkinImages->image(name,error) : nullptr;
}

bool LLVKWidgetTree::iconWantsHandCursor(Id id) const noexcept
{
    const auto* node = get(id);
    return node && node->icon && node->icon->params.interactable && node->params.enabled;
}

std::optional<LLVKWidgetTree::IconDraw> LLVKWidgetTree::prepareIcon(Id id, float drawAlpha,
    float controlTransparency, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->icon) { error = "Native image draw target is not an icon"; return std::nullopt; }
    const auto& icon = *node->icon;
    const float alpha = icon.params.useDrawContextAlpha ? drawAlpha : controlTransparency;
    if (!std::isfinite(alpha)) { error = "Nonfinite native icon transparency"; return std::nullopt; }
    auto rect = screenRect(id,error);
    if (!rect) return std::nullopt;
    auto color = icon.params.color.get();
    color[3] *= alpha;
    if (!std::isfinite(color[3])) { error = "Native icon transparency overflow"; return std::nullopt; }
    return IconDraw{id,*rect,color,icon.image};
}

bool LLVKWidgetTree::setIconColor(Id id,LLVKColor color)
{
    if (!get(id) || !get(id)->icon) return false;
    for (const auto channel : color.get()) if (!std::isfinite(channel)) return false;
    mNodes.at(id).icon->params.color=std::move(color);
    return true;
}
#include "llvkwidgettree.h"

#include <cmath>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createBorder(const Params& view,
    const LLVKBorder::Params& params, Id parent, std::string& error)
{
    error.clear();
    if (params.thickness < 0 || params.thickness > 2 ||
        (params.bevel != LLVKBorder::Bevel::In && params.bevel != LLVKBorder::Bevel::Out &&
         params.bevel != LLVKBorder::Bevel::Bright && params.bevel != LLVKBorder::Bevel::None) ||
        (params.style != LLVKBorder::Style::Line && params.style != LLVKBorder::Style::Texture))
    { error = "Invalid native border thickness, style or bevel"; return std::nullopt; }
    for (const auto& color : {params.highlightLight,params.highlightDark,params.shadowLight,params.shadowDark})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native border color"; return std::nullopt; }
    const auto id = create(view,parent,error);
    if (id) mNodes.at(*id).border = LLVKBorder{params,false};
    return id;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createPanel(const Params& view,
    const LLVKControl::Params& control, const LLVKPanel::Params& params, Id parent, std::string& error)
{
    error.clear();
    for (const auto& color : {params.opaqueColor,params.transparentColor,params.opaqueImageOverlay,params.transparentImageOverlay})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native panel color"; return std::nullopt; }
    LLVKPanel panel;
    panel.params = params;
    panel.label.assign(params.label);
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::move(panel));
}

bool LLVKWidgetTree::addPanelBorder(Id id, const LLVKBorder::Params& params, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native border owner is not a panel"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width < INT32_MIN || width > INT32_MAX || height < INT32_MIN || height > INT32_MAX)
    { error = "Native panel border rectangle overflow"; return false; }
    Params view;
    view.name = "view_border";
    view.mouseOpaque = false;
    view.follows = Left | Right | Top | Bottom;
    view.rect = {0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
    const Id previous = node->panel->border;
    const auto border = createBorder(view,params,id,error);
    if (!border) return false;
    if (previous && get(previous) && !erase(previous,error))
    {
        std::string cleanup;
        erase(*border,cleanup);
        return false;
    }
    if (!get(id)) { error = "Native panel deleted during border replacement"; return false; }
    mNodes.at(id).panel->border = *border;
    return true;
}

bool LLVKWidgetTree::removePanelBorder(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native border owner is not a panel"; return false; }
    const Id border = node->panel->border;
    if (get(border) && !erase(border,error)) return false;
    if (get(id)) mNodes.at(id).panel->border = 0;
    return true;
}

std::optional<std::string> LLVKWidgetTree::panelString(Id id, const std::string& name,
    const LLVKLabel::Arguments& arguments, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native string owner is not a panel"; return std::nullopt; }
    const auto found = node->panel->strings.find(name);
    if (found == node->panel->strings.end()) { error = "Missing native panel string: " + name; return std::nullopt; }
    auto label = found->second;
    label.setArguments(arguments);
    return label.resolve(mLabelContext);
}
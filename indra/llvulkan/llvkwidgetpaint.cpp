#include "llvkwidgetpaint.h"
#include "llstring.h"
#include <algorithm>
#include <cmath>

std::optional<LLVKWidgetPaint> LLVKWidgetPaint::prepare(LLVKWidgetTree& tree, Id root, const Input& input, std::string& error)
{
    error.clear();
    if (!tree.prepareLayoutStacks(root,input.button.frameDelta,error)) return std::nullopt;
    const auto rootRect = tree.screenRect(root,error);
    if (!rootRect) return std::nullopt;
    LLVKWidgetPaint output;
    std::vector<Id> popups;
    bool paintingPopups = false;
    const auto intersect = [](Rect first,Rect second)
    { return Rect{std::max(first.left,second.left),std::max(first.bottom,second.bottom),std::min(first.right,second.right),std::min(first.top,second.top)}; };
    const auto visit = [&](const auto& self,Id id,Rect clip) -> bool
    {
        const auto* node = tree.get(id);
        if (!node || !node->params.visible) return true;
        if (node->comboListOwner && !paintingPopups) { popups.push_back(id); return true; }
        const auto screen = tree.screenRect(id,error);
        if (!screen) return false;
        const auto overlap = intersect(*screen,*rootRect);
        if (screen->left >= screen->right || screen->bottom >= screen->top || overlap.left >= overlap.right || overlap.bottom >= overlap.top) return true;
        const auto append = [&](Rect local,LLVKColor::Value color,std::shared_ptr<const LLVKWidgetImage> image = {},
            std::optional<LLVKFont::LineLayout> text = {},bool mask = false,bool additive = false,bool shadow = false) -> bool
        {
            if (output.commands.size() >= 65536) { error = "Native widget paint command budget exceeded"; return false; }
            for (const auto channel : color) if (!std::isfinite(channel)) { error = "Nonfinite native widget paint color"; return false; }
            const auto left = std::int64_t(local.left)+screen->left, right = std::int64_t(local.right)+screen->left;
            const auto bottom = std::int64_t(local.bottom)+screen->bottom, top = std::int64_t(local.top)+screen->bottom;
            for (const auto value : {left,right,bottom,top})
                if (value < INT32_MIN || value > INT32_MAX) { error = "Native paint coordinates overflow"; return false; }
            if (text)
                for (auto& glyph : text->glyphs)
                { glyph.left += screen->left; glyph.right += screen->left; glyph.bottom += screen->bottom; glyph.top += screen->bottom; }
            output.commands.push_back({id,{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)},
                clip,color,std::move(image),std::move(text),mask,additive,shadow});
            return true;
        };
        const auto width = screen->right-screen->left, height = screen->top-screen->bottom;
        if (node->panel && node->panel->params.backgroundVisible)
        {
            const auto& panel = node->panel->params;
            const auto image = panel.backgroundOpaque ? panel.opaqueImage : panel.transparentImage;
            auto color = image ? (panel.backgroundOpaque ? panel.opaqueImageOverlay.get() : panel.transparentImageOverlay.get()) :
                (panel.backgroundOpaque ? panel.opaqueColor.get() : panel.transparentColor.get());
            color[3] *= input.button.transparency;
            if (!append({0,0,width,height},color,image)) return false;
        }
        if (node->comboListOwner)
        {
            const auto* owner = tree.get(node->comboListOwner);
            if (!owner || !owner->combo) { error = "Native popup paint lost combo owner"; return false; }
            const auto& combo = *owner->combo;
            const auto& params = *combo.params;
            const auto tint = [&](LLVKColor::Value color) { color[3] *= input.button.transparency; return color; };
            if (params.listBackgroundVisible && !append({0,0,width,height},tint(params.listBackground.get()))) return false;
            clip = intersect(clip,{screen->left+2,screen->bottom+2,screen->right-2,screen->top-2});
            if (combo.rowHeight <= 0) { error = "Native popup has no prepared row height"; return false; }
            auto top = height-2;
            for (std::size_t row = combo.firstRow; row < combo.items.size() && top > 2; ++row,top -= combo.rowHeight)
            {
                const auto& item = combo.items[row];
                const bool selected = combo.selected == row;
                auto background = selected ? params.listSelectedBackground.get() : !item.enabled ? params.listReadOnlyBackground.get() :
                    combo.hovered == row ? params.listHoverBackground.get() : LLVKColor::Value{0,0,0,0};
                if (!append({2,top-combo.rowHeight,width-2,top},tint(background))) return false;
                const auto foreground = !item.enabled ? params.listDisabledForeground.get() : selected ? params.listSelectedForeground.get() : params.listForeground.get();
                const auto wide = utf8str_to_wstring(item.label);
                const std::u32string label(wide.begin(),wide.end());
                LLVKFont::LineOptions options;
                options.x = 3;
                options.y = float(top-combo.rowHeight);
                options.vertical = LLVKFont::VerticalAlign::Bottom;
                options.maxPixels = std::max(0,width-4);
                options.ellipses = true;
                auto line = params.listControl.font->layoutLine(label,0,label.size(),options,error);
                if (!line || !append({},tint(foreground),{},std::move(line))) return false;
            }
            return true;
        }
        if (node->browser)
        {
            const auto browser = input.browsers.find(id);
            if (browser == input.browsers.end() || !browser->second) output.pendingBrowsers.push_back(id);
            else
            {
                if (!append({0,0,width,height},{1,1,1,input.button.drawAlpha},browser->second)) return false;
                output.commands.back().streamingImage = true;
            }
        }
        else if (node->button)
        {
            auto view = input.button;
            view.mouseX -= screen->left;
            view.mouseY -= screen->bottom;
            const auto draw = tree.prepareButton(id,view,error);
            if (!draw) return false;
            for (const auto& primitive : draw->primitives)
            {
                if (primitive.outline)
                {
                    const auto& rect = primitive.rectangle;
                    for (const Rect edge : {Rect{rect.left,rect.bottom,rect.right,rect.bottom+1},Rect{rect.left,rect.top-1,rect.right,rect.top},
                        Rect{rect.left,rect.bottom+1,rect.left+1,rect.top-1},Rect{rect.right-1,rect.bottom+1,rect.right,rect.top-1}})
                        if (!append(edge,primitive.color)) return false;
                }
                else if (!append(primitive.rectangle,primitive.color,primitive.image,{},primitive.solidImage,primitive.additive)) return false;
            }
            if (!draw->label.empty())
            {
                auto line = draw->font->layoutLine(draw->label,0,draw->label.size(),draw->text,error);
                if (!line || !append({},draw->labelColor,{},std::move(line),false,false,draw->shadow)) return false;
            }
        }
        else if (node->lineEditor)
        {
            const auto draw = tree.prepareLineEditor(id,input.editor,error);
            if (!draw) return false;
            for (const auto& part : draw->parts)
                if (!append(part.rectangle,part.color,part.image,part.text,part.solidImage)) return false;
        }
        else if (node->plainText)
        {
            if (!tree.reflowPlainText(id,error)) return false;
            node = tree.get(id);
            if (!node) return true;
            const auto& text = *node->plainText;
            const auto* document = tree.get(text.document);
            if (!document || !text.layout) { error = "Native text paint document is missing"; return false; }
            clip = intersect(clip,*screen);
            auto color = text.readOnly ? text.params.readOnlyColor.get() : text.params.textColor.get();
            color[3] *= input.button.drawAlpha;
            for (const auto& sourceLine : text.layout->lines)
            {
                const auto begin = std::min(sourceLine.begin,text.text.size()), end = std::min(sourceLine.end,text.text.size());
                auto count = end-begin;
                if (count && text.text[begin+count-1] == U'\n') --count;
                LLVKFont::LineOptions options;
                options.x = float(sourceLine.left+document->params.rect.left);
                options.y = float(sourceLine.bottom+document->params.rect.bottom);
                options.vertical = LLVKFont::VerticalAlign::Bottom;
                auto line = node->control->params.font->layoutLine(text.text,begin,count,options,error);
                if (!line || !append({},color,{},std::move(line))) return false;
            }
            return true;
        }
        else if (node->icon)
        {
            const auto draw = tree.prepareIcon(id,input.button.drawAlpha,input.button.transparency,error);
            if (!draw) return false;
            if (draw->image && !append({0,0,width,height},draw->color,draw->image)) return false;
        }
        else if (node->border || node->scrollbar || node->scrollContainer || node->badge)
        { error = "Native paint consumer not implemented for visible widget: " + node->params.name; return false; }
        node = tree.get(id);
        if (!node) return true;
        const auto children = node->children;
        const bool clipPanels = node->layoutStack && node->layoutStack->clip;
        const bool vertical = node->layoutStack && node->layoutStack->vertical;
        for (auto child = children.rbegin(); child != children.rend(); ++child)
        {
            const auto* current = tree.get(*child);
            if (!current || current->parent != id) continue;
            auto childClip = clip;
            if (clipPanels && current->layoutPanel)
            {
                auto visible = tree.screenRect(*child,error);
                if (!visible) return false;
                const auto amount = current->layoutPanel->visibleAmount;
                if (vertical) visible->bottom = visible->top-static_cast<std::int32_t>(std::floor((visible->top-visible->bottom)*amount+0.5f));
                else visible->right = visible->left+static_cast<std::int32_t>(std::floor((visible->right-visible->left)*amount+0.5f));
                childClip = intersect(childClip,*visible);
            }
            if (childClip.left < childClip.right && childClip.bottom < childClip.top && !self(self,*child,childClip)) return false;
        }
        return true;
    };
    if (!visit(visit,root,*rootRect)) return std::nullopt;
    paintingPopups = true;
    for (const auto popup : popups) if (!visit(visit,popup,*rootRect)) return std::nullopt;
    return output;
}
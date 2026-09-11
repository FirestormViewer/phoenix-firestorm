#include "llvkwidgetpaint.h"
#include "v3color.h"
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
        if (node->scrollList)
        {
            if (!tree.layoutScrollList(id,error)) return false;
            node=tree.get(id);
            if (!node) return true;
        }
        if (node->searchEditor)
        {
            if (!tree.refreshSearchEditor(id,error)) return false;
            node=tree.get(id);
            if (!node) return true;
        }
        if (node->textEditor)
        {
            if (!tree.layoutTextEditor(id,error)) return false;
            node=tree.get(id);
            if (!node) return true;
        }
        if (node->tabContainer && node->tabContainer->layout)
        {
            const auto layout = *node->tabContainer->layout;
            if (!tree.layoutTabPanels(id,layout,error,input.button.frameDelta)) return false;
            node = tree.get(id);
            if (!node) return true;
        }
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
        if (node->scrollList)
        {
            const auto state=*node->scrollList;
            const auto& params=*state.params;
            const auto tint=[&](LLVKColor::Value color) { color[3]*=input.button.transparency; return color; };
            if (params.background && !append({0,0,width,height},tint(tree.enabledInChain(id) ? params.writableBackground.get() : params.readonlyBackground.get()))) return false;
            const auto outerClip=clip;
            clip=intersect(clip,{screen->left+state.content.left,screen->bottom+state.content.bottom,screen->left+state.content.right,screen->bottom+state.content.top});
            for (int index=state.firstRow; index<static_cast<int>(state.rows.size()) && index<=state.firstRow+state.pageLines; ++index)
            {
                const auto& row=state.rows[index];
                const auto top=state.content.top-(index-state.firstRow)*state.lineHeight,bottom=top-state.lineHeight;
                if (params.stripes && index%2 && !append({state.content.left,bottom,state.content.right,top},tint(params.stripeColor.get()))) return false;
                if (state.hovered==index && state.hoveredCell<0 && !append({state.content.left,bottom,state.content.right,top},tint(params.hoveredColor.get()))) return false;
                if (row.selected && row.selectedCell<0 && !append({state.content.left,bottom,state.content.right,top},tint(params.selectedBackground.get()))) return false;
                const auto color=tint(!row.enabled ? params.disabledForeground.get() : row.selected ? params.selectedForeground.get() : params.foreground.get());
                auto left=state.content.left;
                for (std::size_t column=0; column<state.widths.size(); ++column)
                {
                    const Rect cellRect{left,bottom,left+state.widths[column],top-params.rowPadding};
                    if (row.selected && row.selectedCell==static_cast<int>(column))
                    { if (!append(cellRect,tint(params.selectedBackground.get()))) return false; }
                    else if (state.hovered==index && state.hoveredCell==static_cast<int>(column))
                    { if (!append(cellRect,tint(params.hoveredColor.get()))) return false; }
                    if (column<row.cells.size() && state.widths[column]>0)
                    {
                        const auto* style=column<row.styles.size() ? &row.styles[column] : nullptr;
                        const auto font=style && style->font ? style->font : node->control->params.font;
                        const bool iconOnly=style && style->type==LLVKWidgetTree::ListCellStyle::Type::Icon;
                        const bool iconText=style && style->type==LLVKWidgetTree::ListCellStyle::Type::IconText;
                        const auto align=style ? style->alignment : LLVKButton::Align::Left;
                        const auto iconHeight=iconOnly && style->image ? static_cast<int>(style->image->height()) : static_cast<int>(std::ceil(font->metrics().lineHeight));
                        const auto iconWidth=iconOnly && style->image ? static_cast<int>(style->image->width()) : iconHeight;
                        const int iconSpace=iconText && style->image ? iconHeight+4 : 0;
                        const auto wide=utf8str_to_wstring(row.cells[column]);
                        const std::u32string text(wide.begin(),wide.end());
                        if (style && style->image)
                        {
                            int iconLeft=left+(iconText ? 1 : 0);
                            if (iconOnly && align==LLVKButton::Align::Right) iconLeft=left+state.widths[column]-iconWidth;
                            else if (iconOnly && align==LLVKButton::Align::Center) iconLeft=left+(state.widths[column]-iconWidth)/2;
                            else if (iconText && align!=LLVKButton::Align::Left)
                            {
                                const auto measured=font->measureRun(text,0,text.size(),1.f,true,false,error);
                                if (!measured) return false;
                                iconLeft=align==LLVKButton::Align::Right ? left+state.widths[column]-static_cast<int>(measured->width)-iconSpace :
                                    left+(state.widths[column]-static_cast<int>(measured->width)-iconSpace)/2;
                            }
                            if (!append({iconLeft,bottom,iconLeft+iconWidth,bottom+iconHeight},tint(style->imageColor.get()),style->image)) return false;
                        }
                        if (!iconOnly)
                        {
                            LLVKFont::LineOptions options; options.x=float(left+iconSpace+(iconText ? 1 : 0)); options.y=float(bottom);
                            if (align==LLVKButton::Align::Right) { options.x=float(left+state.widths[column]); options.horizontal=LLVKFont::HorizontalAlign::Right; }
                            else if (align==LLVKButton::Align::Center) { options.x=float(left)+(state.widths[column]+iconSpace)*0.5f; options.horizontal=LLVKFont::HorizontalAlign::Center; }
                            options.vertical=LLVKFont::VerticalAlign::Bottom; options.maxPixels=std::max(0,state.widths[column]-iconSpace); options.ellipses=true;
                            const auto line=font->layoutLine(text,0,text.size(),options,error);
                            if (!line || !append({},color,{},*line)) return false;
                        }
                    }
                    left+=state.widths[column]+params.columnPadding;
                }
            }
            clip=outerClip;
            for (auto child=node->children.rbegin(); child!=node->children.rend(); ++child)
                if (!self(self,*child,clip)) return false;
            return true;
        }
        if (node->colorSwatch)
        {
            if (!tree.refreshColorSwatch(id,error)) return false;
            node=tree.get(id);
            if (!node) return true;
            const auto swatch=*node->colorSwatch;
            if (!swatch.valid) { error="Native invalid swatch fallback is not implemented"; return false; }
            const Rect interior{1,swatch.params->labelHeight+1,width-1,height-1};
            const auto alpha=input.button.drawAlpha;
            if (swatch.color[3]<1.f)
            {
                const auto checker=tree.findImage("Checker",error);
                if (!checker || !checker->pixelWidth() || !checker->pixelHeight())
                { if (error.empty()) error="Native swatch requires the Checker skin image"; return false; }
                const auto pixels=checker->bottomUpRgba();
                const auto sample=[&](int x,int y)
                {
                    const auto column=static_cast<std::uint32_t>((float(x%32)+0.5f)*checker->pixelWidth()/32.f);
                    const auto row=static_cast<std::uint32_t>((float(y%32)+0.5f)*checker->pixelHeight()/32.f);
                    const auto offset=4*(std::size_t(row)*checker->pixelWidth()+column);
                    return LLVKColor::Value{pixels[offset]/255.f,pixels[offset+1]/255.f,pixels[offset+2]/255.f,alpha*pixels[offset+3]/255.f};
                };
                for (int bottom=interior.bottom; bottom<interior.top; ++bottom)
                    for (int left=interior.left; left<interior.right; )
                    {
                        const auto color=sample(left-interior.left,bottom-interior.bottom);
                        auto right=left+1;
                        while (right<interior.right && sample(right-interior.left,bottom-interior.bottom)==color) ++right;
                        if (!append({left,bottom,right,bottom+1},color)) return false;
                        left=right;
                    }
            }
            auto color=swatch.color; color[3]*=alpha;
            if (!append(interior,color)) return false;
            if (swatch.color[3]<1.f && swatch.params->alphaBackground && !append(interior,color,swatch.params->alphaBackground)) return false;
            auto border=swatch.params->borderColor.get(); border[3]*=alpha;
            const auto bottom=swatch.params->labelHeight;
            for (const Rect edge : {Rect{0,bottom,width,bottom+1},Rect{0,height-1,width,height},
                Rect{0,bottom+1,1,height-1},Rect{width-1,bottom+1,width,height-1}})
                if (!append(edge,border)) return false;
        }
        if (node->slider)
        {
            if (!tree.updateSliderThumb(id,error)) return false;
            node=tree.get(id);
            const auto slider=*node->slider;
            const auto& params=*slider.params;
            if (!params.thumb || !params.track || !params.highlight)
            { error="Native slider paint requires its thumb, track and highlight images"; return false; }
            const auto enabled=tree.enabledInChain(id);
            auto color=LLVKColor::Value{1,1,1,(enabled ? 1.f : 0.6f)*input.button.drawAlpha};
            const auto trackWidth=static_cast<int>(params.track->width()), trackHeight=static_cast<int>(params.track->height());
            const Rect track=params.vertical ? Rect{width/2-trackWidth/2,0,width/2+trackWidth/2,height} :
                Rect{static_cast<int>(params.thumb->width()/2),height/2-trackHeight/2,width-static_cast<int>(params.thumb->width()/2),height/2+trackHeight/2};
            auto highlight=track;
            if (!params.vertical) highlight.right=(slider.thumb.left+slider.thumb.right)/2;
            if (!append(track,color,params.track) || !append(highlight,color,params.highlight)) return false;
            if (tree.keyboardFocus()==id)
            {
                auto focus=input.button.focusColor;
                focus[3]*=input.button.drawAlpha;
                const auto border=input.button.focusWidth;
                if (!append({slider.thumb.left-border,slider.thumb.bottom-border,slider.thumb.right+border,slider.thumb.top+border},focus,params.thumb,{},true)) return false;
            }
            auto center=params.centerColor.get(); center[3]*=input.button.drawAlpha;
            if (tree.mouseCapture()==id)
            {
                auto ghost=center; ghost[3]*=0.3f;
                if (!append(slider.dragStart,ghost,params.thumb)) return false;
                auto pressed=params.outlineColor.get(); pressed[3]*=input.button.drawAlpha;
                if (params.pressedThumb && !append(slider.thumb,pressed,params.pressedThumb)) return false;
            }
            else if (!enabled)
            { if (params.disabledThumb && !append(slider.thumb,center,params.disabledThumb)) return false; }
            else if (!append(slider.thumb,center,params.thumb)) return false;
            return true;
        }
        if (node->scrollContainer)
        {
            if (!tree.advanceScrollFrame(id,input.button.frameDelta,error)) return false;
            const auto draw = tree.prepareScrollContainer(id,input.button.transparency,error);
            if (!draw) return false;
            if (draw->backgroundVisible && !append({draw->background.left-screen->left,draw->background.bottom-screen->bottom,
                draw->background.right-screen->left,draw->background.top-screen->bottom},draw->backgroundColor)) return false;
            const auto documentClip = intersect(clip,draw->documentClip);
            if (draw->document && documentClip.left < documentClip.right && documentClip.bottom < documentClip.top &&
                !self(self,draw->document,documentClip)) return false;
            for (const auto child : draw->chrome) if (!self(self,child,clip)) return false;
            return true;
        }
        if (node->scrollbar)
        {
            const auto draw = tree.prepareScrollbar(id,input.button.mouseX,input.button.mouseY,
                input.button.frameDelta,input.button.focusColor,error);
            if (!draw) return false;
            for (const auto& primitive : draw->primitives)
                if (!append({primitive.rectangle.left-screen->left,primitive.rectangle.bottom-screen->bottom,
                    primitive.rectangle.right-screen->left,primitive.rectangle.top-screen->bottom},primitive.color,
                    primitive.image,{},primitive.solidImage,primitive.additive)) return false;
            for (auto child = draw->children.rbegin(); child != draw->children.rend(); ++child)
                if (!self(self,*child,clip)) return false;
            return true;
        }
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
            if (text.params.backgroundVisible)
            {
                auto background=text.readOnly ? text.params.readOnlyBackground.get() : text.params.backgroundColor.get();
                background[3]*=input.button.drawAlpha;
                if (!append({0,0,width,height},background)) return false;
            }
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
                const auto selectionBegin = std::min(text.selectionStart,text.selectionEnd);
                const auto selectionEnd = std::max(text.selectionStart,text.selectionEnd);
                const bool selection = text.params.selectable && selectionBegin != selectionEnd;
                if (selection && selectionBegin < begin+count && selectionEnd > begin)
                {
                    const auto first = std::max(begin,selectionBegin), last = std::min(begin+count,selectionEnd);
                    const auto left = node->control->params.font->measureRun(text.text,begin,first-begin,1.f,false,false,error);
                    const auto right = node->control->params.font->measureRun(text.text,begin,last-begin,1.f,false,false,error);
                    if (!left || !right) return false;
                    auto background = text.params.selectionBackground.get();
                    background[3] *= input.button.drawAlpha;
                    if (!append({static_cast<std::int32_t>(options.x+left->advancePixels),static_cast<std::int32_t>(options.y),
                        static_cast<std::int32_t>(options.x+right->advancePixels),sourceLine.top+document->params.rect.bottom},background)) return false;
                }
                auto line = node->control->params.font->layoutLine(text.text,begin,count,options,error);
                if (!line) return false;
                if (text.links.empty() && !selection)
                { if (!append({},color,{},std::move(line))) return false; }
                else
                {
                    for (std::size_t first = 0; first < line->glyphs.size(); )
                    {
                        const auto linkAt = [&](std::size_t glyph) -> const LLVKWebText::Link*
                        {
                            for (const auto& link : text.links)
                                if (line->glyphs[glyph].sourceIndex >= link.begin && line->glyphs[glyph].sourceIndex < link.end) return &link;
                            return nullptr;
                        };
                        const auto* link = linkAt(first);
                        const auto selected = [&](std::size_t glyph)
                        { return selection && line->glyphs[glyph].sourceIndex >= selectionBegin && line->glyphs[glyph].sourceIndex < selectionEnd; };
                        const bool highlighted = selected(first);
                        auto last = first+1;
                        while (last < line->glyphs.size() && linkAt(last) == link && selected(last) == highlighted) ++last;
                        auto part = *line;
                        part.glyphs.assign(line->glyphs.begin()+first,line->glyphs.begin()+last);
                        auto foreground = color;
                        if (link)
                        { foreground = link->query ? text.params.queryColor.get() : text.params.linkColor.get(); foreground[3] *= input.button.drawAlpha; }
                        if (highlighted) { foreground = text.params.selectionColor.get(); foreground[3] *= input.button.drawAlpha; }
                        if (!append({},foreground,{},std::move(part))) return false;
                        const auto hoveredLink=text.params.skipLinkUnderline ? tree.plainTextLinkAt(id,
                            input.button.mouseX-screen->left,input.button.mouseY-screen->bottom,error) : std::optional<std::size_t>();
                        if (!error.empty()) return false;
                        if (link && (!text.params.skipLinkUnderline || (hoveredLink && &text.links[*hoveredLink]==link)))
                        {
                            const auto& initial = line->glyphs[first];
                            const auto& final = line->glyphs[last-1];
                            const auto left = static_cast<std::int32_t>(std::floor(initial.left-initial.glyph->raster.bearingX+0.5f));
                            const auto right = static_cast<std::int32_t>(std::floor(final.left-final.glyph->raster.bearingX+final.glyph->raster.advanceX+0.5f));
                            const auto bottom = static_cast<std::int32_t>(std::floor(line->baselinePixelY-std::floor(node->control->params.font->metrics().descender)));
                            if (right > left && !append({left,bottom,right,bottom+1},foreground)) return false;
                        }
                        first = last;
                    }
                }
            }
            if (text.params.selectable && !text.readOnly && tree.keyboardFocus()==id && input.editor.applicationFocused &&
                (input.editor.secondsSinceKeystroke<1.0 || static_cast<int>(input.editor.secondsSinceKeystroke*2)&1))
            {
                const auto caret=tree.plainTextCaretRect(id,error);
                if (!caret) return false;
                auto cursor=text.params.cursorColor.get(); cursor[3]*=input.button.drawAlpha;
                if (!append(*caret,cursor)) return false;
            }
            return true;
        }
        else if (node->icon)
        {
            const auto draw = tree.prepareIcon(id,input.button.drawAlpha,input.button.transparency,error);
            if (!draw) return false;
            if (draw->image && !append({0,0,width,height},draw->color,draw->image)) return false;
        }
        else if (node->border)
        {
            const auto& border = *node->border;
            const auto& params = border.params;
            if (params.style == LLVKBorder::Style::Line && params.thickness)
            {
                using Bevel = LLVKBorder::Bevel;
                const auto upper = [&](int inset,int thickness,LLVKColor::Value color)
                {
                    return append({inset,inset,std::min(width-inset,inset+thickness),height-inset},color) &&
                        append({inset,std::max(inset,height-inset-thickness),width-inset,height-inset},color);
                };
                const auto lower = [&](int inset,int thickness,LLVKColor::Value color)
                {
                    return append({std::max(inset,width-inset-thickness),inset,width-inset,height-inset},color) &&
                        append({inset,inset,width-inset,std::min(height-inset,inset+thickness)},color);
                };
                if (params.thickness == 1)
                {
                    if (params.bevel == Bevel::Bright) { error = "One-pixel bright border is undefined in the source contract"; return false; }
                    auto top = params.bevel == Bevel::In ? params.shadowDark.get() : params.highlightLight.get();
                    auto bottom = params.bevel == Bevel::Out ? params.shadowDark.get() : params.highlightLight.get();
                    int thickness = 1;
                    if (border.keyboardFocus)
                    {
                        top = bottom = input.button.focusColor;
                        thickness = static_cast<int>(std::floor(1.f+tree.focusFlashAmount()+0.5f));
                    }
                    if (!upper(0,thickness,top) || !lower(0,thickness,bottom)) return false;
                }
                else
                {
                    auto topOuter = params.highlightDark.get(), topInner = params.highlightLight.get();
                    auto bottomOuter = params.shadowDark.get(), bottomInner = params.shadowLight.get();
                    if (params.bevel == Bevel::In)
                    { topOuter = params.shadowLight.get(); topInner = params.shadowDark.get(); bottomOuter = params.highlightLight.get(); bottomInner = params.highlightDark.get(); }
                    else if (params.bevel == Bevel::Bright)
                    { topOuter = topInner = bottomOuter = bottomInner = params.highlightLight.get(); }
                    else if (params.bevel == Bevel::None)
                    { topOuter = topInner = bottomOuter = bottomInner = params.shadowDark.get(); }
                    if (border.keyboardFocus) topOuter = bottomOuter = input.button.focusColor;
                    topOuter[3] = topInner[3] = bottomOuter[3] = bottomInner[3] = 1.f;
                    if (!upper(0,1,topOuter) || (width > 2 && height > 2 && !upper(1,1,topInner)) ||
                        !lower(0,1,bottomOuter) || (width > 2 && height > 2 && !lower(1,1,bottomInner))) return false;
                }
            }
        }
        else if (node->badge)
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
            if (node->tabContainer)
                childClip=intersect(childClip,{screen->left+3,screen->bottom,screen->right-3,screen->top});
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
        node=tree.get(id);
        if (node && node->colorPicker)
        {
            const auto& picker=*node->colorPicker;
            const auto alpha=input.button.drawAlpha;
            if (!append({140,100,396,356},{1,1,1,alpha},picker.hueImage)) return false;
            const auto outline=[&](Rect rect,LLVKColor::Value color)
            {
                return append({rect.left,rect.bottom,rect.right,rect.bottom+1},color) &&
                    append({rect.left,rect.top-1,rect.right,rect.top},color) &&
                    append({rect.left,rect.bottom+1,rect.left+1,rect.top-1},color) &&
                    append({rect.right-1,rect.bottom+1,rect.right,rect.top-1},color);
            };
            const auto hueX=140+static_cast<int>(256.f*picker.hsl[0]);
            const auto saturationY=100+static_cast<int>(256.f*picker.hsl[1]);
            if (!append({hueX-8,saturationY,hueX+8,saturationY+1},{0,0,0,1}) ||
                !append({hueX,saturationY-8,hueX+1,saturationY+8},{0,0,0,1}) ||
                !outline({140,100,397,356},{0,0,0,alpha})) return false;
            for (int row=0; row<256; ++row)
            {
                LLColor3 color; color.setHSL(picker.hsl[0],picker.hsl[1],float(row)/256.f);
                if (!append({412,99+row,428,100+row},{color.mV[0],color.mV[1],color.mV[2],alpha})) return false;
            }
            const auto markerY=100+static_cast<int>(256.f*picker.hsl[2]);
            if (!append({428,markerY-6,434,markerY+6},{0.75f,0.75f,0.75f,1})) return false;
            output.commands.back().triangle=std::array<float,6>{float(screen->left+428),float(screen->bottom+markerY),
                float(screen->left+434),float(screen->bottom+markerY-6),float(screen->left+434),float(screen->bottom+markerY+6)};
            if (!outline({412,100,429,356},{0,0,0,1}) ||
                !append({12,130,128,190},{picker.rgb[0],picker.rgb[1],picker.rgb[2],alpha}) ||
                !outline({12,130,129,190},{0,0,0,1})) return false;
            if (picker.paletteReady)
                for (int index=0; index<32; ++index)
                {
                    const auto column=index%16, row=index/16;
                    const auto left=11+418*column/16, right=11+418*(column+1)/16;
                    const auto top=92-40*row/2, bottom=92-40*(row+1)/2;
                    auto color=picker.palette[index]; color[3]*=alpha;
                    if (!append({left+2,bottom+2,right-2,top-2},color) || !outline({left+1,bottom+1,right-1,top-1},{0,0,0,1})) return false;
                }
        }
        return true;
    };
    if (!visit(visit,root,*rootRect)) return std::nullopt;
    paintingPopups = true;
    for (const auto popup : popups) if (!visit(visit,popup,*rootRect)) return std::nullopt;
    return output;
}
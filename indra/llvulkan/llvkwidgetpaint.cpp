#include "llvkwidgetpaint.h"
#include "llvkmenu.h"
#include "llvkbrowsersurface.h"
#include "v3color.h"
#include "llstring.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

bool LLVKWidgetPaint::appendDropShadow(Id owner,Rect rectangle,Rect clip,LLVKColor::Value inner,float edge,std::string& error)
{
    for (const auto channel : inner)
        if (!std::isfinite(channel)) { error="Nonfinite native shadow color"; return false; }
    for (auto& channel : inner)
        channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
    if (commands.size()>65536-10) { error="Native shadow exceeds paint budget"; return false; }
    auto outer=inner; outer[3]=0;
    const float left=float(rectangle.left),right=float(rectangle.right)-1;
    const float bottom=float(rectangle.bottom)+1,top=float(rectangle.top);
    const std::array<std::array<float,2>,12> positions{{
        {right,top-edge},{right,bottom},{right+edge,bottom},{right+edge,top-edge},
        {left+edge,bottom},{left+edge,bottom-edge},{right,bottom-edge},{left,bottom},
        {left+1,bottom-edge+1},{right+edge-1,bottom-edge+1},{right+edge-1,top-1},{right,top}}};
    constexpr std::array<std::array<unsigned,3>,10> triangles{{
        {0,1,2},{0,2,3},{1,4,5},{1,5,6},{4,7,8},
        {4,8,5},{1,6,9},{1,9,2},{0,3,10},{0,10,11}}};
    for (const auto& indices : triangles)
    {
        Command command;
        command.owner=owner; command.clip=clip;
        command.triangle.emplace(); command.triangleColors.emplace();
        for (std::size_t vertex=0; vertex<indices.size(); ++vertex)
        {
            const auto index=indices[vertex];
            (*command.triangle)[vertex*2]=positions[index][0];
            (*command.triangle)[vertex*2+1]=positions[index][1];
            (*command.triangleColors)[vertex]=(index==0 || index==1 || index==4) ? inner : outer;
        }
        commands.push_back(std::move(command));
    }
    return true;
}

std::optional<LLVKWidgetPaint> LLVKWidgetPaint::prepare(LLVKWidgetTree& tree, Id root, const Input& input, std::string& error)
{
    error.clear();
    if (!tree.prepareLayoutStacks(root,input.button.frameDelta,error)) return std::nullopt;
    const auto rootRect = tree.screenRect(root,error);
    if (!rootRect) return std::nullopt;
    LLVKWidgetPaint output;
    std::vector<Id> popups;
    std::vector<Id> menuPopups;
    const auto paintMenu=[&](Id id,bool dropdowns) -> bool
    {
        const auto node=tree.get(id);
        const auto screen=tree.screenRect(id,error);
        if (!node || !node->menu || !screen) return false;
        LLVKWidgetPaint menuPaint;
        const auto menuAlpha=static_cast<float>(tree.setting("FSMenuBackgroundAlpha").value_or(LLSD(1.f)).asReal());
        if (!node->menu->paint(menuPaint,*rootRect,error,*screen,dropdowns,{},menuAlpha)) return false;
        for (auto& command : menuPaint.commands)
        { command.owner=id; output.commands.push_back(std::move(command)); }
        return true;
    };
    bool paintingPopups = false;
    const auto intersect = [](Rect first,Rect second)
    { return Rect{std::max(first.left,second.left),std::max(first.bottom,second.bottom),std::min(first.right,second.right),std::min(first.top,second.top)}; };
    const auto& rootInput=input;
    std::map<Id,Input> floaterInputs;
    const auto visit = [&](const auto& self,Id id,Rect clip) -> bool
    {
        const auto* node = tree.get(id);
        if (!node || !node->params.visible) return true;
        Id floater=0;
        if (rootInput.foregroundFloaters)
            for (auto ancestor=id; tree.get(ancestor); ancestor=tree.get(ancestor)->parent)
                if (tree.get(ancestor)->floater) { floater=ancestor; break; }
        if (floater && !floaterInputs.contains(floater))
        {
            auto local=rootInput;
            const bool active=rootInput.activeControlFloaters ? rootInput.activeControlFloaters->contains(floater) :
                rootInput.foregroundFloaters->contains(floater);
            const auto opacity=static_cast<float>(tree.setting(active ? "ActiveFloaterTransparency" : "InactiveFloaterTransparency")
                .value_or(LLSD(active ? 1.f : .95f)).asReal());
            if (!std::isfinite(opacity)) { error="Nonfinite native floater transparency"; return false; }
            local.button.transparency=local.editor.transparency=opacity;
            floaterInputs.emplace(floater,std::move(local));
        }
        const auto& input=floater ? floaterInputs.at(floater) : rootInput;
        if (node->containerView && (!node->parent || !tree.get(node->parent)->containerView))
        {
            const auto rectangle=node->params.rect;
            if (!tree.layoutContainerView(id,rectangle.right-rectangle.left,0,error)) return false;
            node=tree.get(id);
            if (!node) return true;
        }
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
        if (node->comboListOwner && !paintingPopups) { popups.push_back(id); return true; }
        if (node->menu)
        {
            if (node->menu->open()) menuPopups.push_back(id);
            return paintMenu(id,false);
        }
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
            {
                const auto scale=text->displayScale;
                const auto originX=std::floor(screen->left*scale)/scale;
                const auto originY=std::floor(screen->bottom*scale)/scale;
                for (auto& glyph : text->glyphs)
                { glyph.left += originX; glyph.right += originX; glyph.bottom += originY; glyph.top += originY; }
            }
            output.commands.push_back({id,{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)},
                clip,color,std::move(image),std::move(text),mask,additive,shadow});
            return true;
        };
        const auto width = screen->right-screen->left, height = screen->top-screen->bottom;
        const auto rootNode=tree.get(root);
        const auto scale=rootNode && rootNode->control && rootNode->control->params.font ? rootNode->control->params.font->displayScale() : 1.f;
        const auto pixelRect=[&](float left,float bottom,float right,float top,LLVKColor::Value color)
        {
            Command command;
            command.owner=id; command.clip=clip; command.color=color;
            left/=scale; bottom/=scale; right/=scale; top/=scale;
            command.triangle=std::array<float,6>{left,bottom,right,bottom,right,top};
            output.commands.push_back(command);
            command.triangle=std::array<float,6>{left,bottom,right,top,left,top};
            output.commands.push_back(std::move(command));
        };
        const auto pixelLine=[&](double startX,double startY,double endX,double endY,LLVKColor::Value color)
        {
            const bool vertical=startX==endX;
            const auto cross=vertical ? startX : startY;
            const auto thickness=std::max(1.0,std::floor(double(scale)+0.5));
            const auto column=std::ceil(cross-(static_cast<int>(thickness)%2 ? 0.0 : 0.5))-std::ceil(thickness/2.0);
            const auto fraction=cross-std::floor(cross);
            const auto radius=std::min(fraction,1.0-fraction);
            const auto start=vertical ? startY : startX,end=vertical ? endY : endX;
            const auto endpoint=[&](double coordinate)
            {
                if (fraction==0.0) return vertical ? std::floor(coordinate+0.5) : std::ceil(coordinate-0.5);
                return start<end ? std::floor(coordinate-0.5-radius)+1.0 : std::ceil(coordinate-0.5+radius);
            };
            const auto low=endpoint(std::min(start,end)),high=endpoint(std::max(start,end));
            if (vertical) pixelRect(static_cast<float>(column),static_cast<float>(low),static_cast<float>(column+thickness),static_cast<float>(high),color);
            else pixelRect(static_cast<float>(low),static_cast<float>(column),static_cast<float>(high),static_cast<float>(column+thickness),color);
        };
        const auto alertShadow=[&](int inset,float alpha,bool floater=false) -> bool
        {
            if (!node->panel) return true;
            const auto color=floater ? input.floaterShadow : node->panel->params.alertShadowColor;
            if (!color) return true;
            auto inner=color->get();
            const bool foreground=!input.foregroundFloaters || input.foregroundFloaters->contains(id);
            const float edge=floater && !foreground ? 2.f : 6.f;
            if (floater && !foreground) alpha*=0.5f;
            inner[3]*=alpha;
            return output.appendDropShadow(id,{screen->left+inset,screen->bottom+inset,
                screen->right-inset,screen->top-inset},clip,inner,edge,error);
        };
        bool searchHighlighted=false;
        auto searchBackground=input.searchBackground.get();
        const auto searchFont=input.searchFont.get();
        for (auto& channel : searchBackground) channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
        for (auto ancestor=id; ancestor && tree.get(ancestor); ancestor=tree.get(ancestor)->parent)
        {
            if (tree.get(ancestor)->searchHighlighted) { searchHighlighted=true; break; }
            if (ancestor==root) break;
        }
        if (node->progressBar)
        {
            const auto progress=*node->progressBar;
            const auto value=tree.value(id).asReal();
            if (!std::isfinite(value) || !std::isfinite(input.animationSeconds) || input.animationSeconds<0)
            { error="Invalid native progress animation state"; return false; }
            auto background=progress.background.get(); background[3]=input.button.drawAlpha;
            auto fill=progress.fill.get();
            fill[3]*=input.button.drawAlpha*static_cast<float>(.75+.25*std::sin(3.*input.animationSeconds));
            if (!progress.imageBar.empty())
            {
                const auto image=tree.findImage(progress.imageBar,error);
                if (!error.empty() || (image && !append({0,0,width,height},background,image))) return false;
            }
            if (!progress.imageFill.empty())
            {
                const auto image=tree.findImage(progress.imageFill,error);
                const auto filled=static_cast<int>(std::floor(width*std::clamp(value,0.,100.)/100.+.5));
                if (!error.empty() || (image && filled>0 && !append({0,0,filled,height},fill,image))) return false;
            }
        }
        else if (node->statBar)
        {
            if (!tree.advanceStatBar(id,input.button.frameDelta,error)) return false;
            const auto bar=*tree.get(id)->statBar;
            clip=intersect(clip,*screen);
            const auto number=[&](float value,bool padded)
            {
                std::ostringstream output;
                const int precision=value==std::trunc(value) ? 0 : bar.decimalDigits;
                if (padded) output<<std::setw(10);
                output<<std::fixed<<std::setprecision(precision)<<value;
                return output.str();
            };
            const auto text=[&](const std::string& value,float x,float y,LLVKFont::HorizontalAlign align,float alpha)
            {
                const auto wide=utf8str_to_wstring(value);
                const std::u32string label(wide.begin(),wide.end());
                LLVKFont::LineOptions options; options.x=x; options.y=y; options.horizontal=align; options.vertical=LLVKFont::VerticalAlign::Top;
                const auto line=bar.font->layoutLine(label,0,label.size(),options,error);
                return line && append({},{1,1,1,alpha},{},*line);
            };
            const float current=bar.samples.empty() ? 0.f : bar.samples.back();
            if (!text(bar.label,0,float(height),LLVKFont::HorizontalAlign::Left,1.f) ||
                !text(number(current,true)+" ",float(width),float(height),LLVKFont::HorizontalAlign::Right,1.f)) return false;
            if (bar.showBar && !bar.samples.empty())
            {
                const Rect plot{0,std::min(std::max(5,height-15)-5,20),width,std::max(5,height-15)};
                const auto range=bar.currentMaximum-bar.currentMinimum;
                const auto scale=range>0 ? float(width)/range : 0.f;
                const auto position=[&](float value)
                { return static_cast<int>(std::clamp((value-bar.currentMinimum)*scale,float(-1000000),float(1000000))); };
                if (bar.tickSpacing>0 && scale>0)
                {
                    const float start=bar.currentMinimum<0 ? -std::ceil(-bar.currentMinimum/bar.tickSpacing)*bar.tickSpacing : 0;
                    int lastTick=-1000000,lastLabel=-1000000;
                    for (int index=0; index<4096; ++index)
                    {
                        const float value=start+index*bar.tickSpacing;
                        const int tick=position(value);
                        if (tick>=lastTick+30)
                        {
                            lastTick=tick;
                            const bool labeled=tick>lastLabel+60;
                            if (!append({tick,plot.bottom-(labeled ? 4 : 2),tick+1,plot.top},{1,1,1,labeled ? 0.25f : 0.1f})) return false;
                            if (labeled)
                            {
                                const auto label=number(value,false);
                                const auto wide=utf8str_to_wstring(label);
                                const std::u32string string(wide.begin(),wide.end());
                                const auto measured=bar.font->measureRun(string,0,string.size(),1.f,true,false,error);
                                if (!measured) return false;
                                const auto left=tick-static_cast<int>(std::round(measured->width*(width ? float(tick)/width : 0.f)));
                                if (!text(label,float(left),float(plot.bottom-4),LLVKFont::HorizontalAlign::Left,0.5f)) return false;
                                lastLabel=left;
                            }
                        }
                        if (value>bar.currentMaximum) break;
                    }
                }
                if (!append(plot,{0,0,0,0.25f})) return false;
                const auto count=std::min(bar.samples.size(),static_cast<std::size_t>(bar.showHistory ? bar.historyFrames : bar.shortFrames));
                const auto begin=bar.samples.end()-count;
                const auto extrema=std::minmax_element(begin,bar.samples.end());
                if (!append({std::max(0,position(*extrema.first)),plot.bottom,position(*extrema.second),plot.top},{1,0,0,0.25f})) return false;
                if (bar.showHistory)
                {
                    for (std::size_t index=1; index<count; ++index)
                    {
                        const auto left=position(bar.samples[bar.samples.size()-1-index]);
                        const auto bottom=plot.bottom+static_cast<int>(float(index)/bar.historyFrames*(plot.top-plot.bottom));
                        if (!append({left,bottom,left+1,bottom+1},{1,0,0,1})) return false;
                    }
                }
                else
                {
                    const auto currentPosition=position(current);
                    if (!append({currentPosition-1,plot.bottom,currentPosition+1,plot.top},{1,0,0,1})) return false;
                }
                double sum=0; for (auto sample=begin; sample!=bar.samples.end(); ++sample) sum+=*sample;
                const auto mean=position(static_cast<float>(sum/count));
                if (!append({mean-1,plot.bottom-2,mean+1,plot.top+2},{0,1,0,1})) return false;
            }
            return true;
        }
        if (node->containerView)
        {
            const auto params=*node->containerView;
            if (params.backgroundVisible && !append({0,0,width,height},params.backgroundColor.get())) return false;
            if (params.showLabel)
            {
                const auto wide=utf8str_to_wstring(params.label);
                const std::u32string text(wide.begin(),wide.end());
                LLVKFont::LineOptions options; options.x=2.f; options.y=float(height-2); options.vertical=LLVKFont::VerticalAlign::Top;
                const auto label=params.font->layoutLine(text,0,text.size(),options,error);
                if (!label || !append({},{1,1,1,input.button.drawAlpha},{},*label)) return false;
            }
        }
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
                        if (style && style->type==LLVKWidgetTree::ListCellStyle::Type::CheckBox)
                        {
                            const bool checked=row.cells[column]=="true" || row.cells[column]=="1";
                            const bool enabled=row.enabled && style->enabled && tree.enabledInChain(id);
                            const auto image=enabled ? (checked ? style->checkedImage : style->image) :
                                (checked ? style->disabledCheckedImage : style->disabledImage);
                            if (!image) { error="Native list checkbox image is unavailable"; return false; }
                            if (!append({left+style->checkLeft,bottom,left+style->checkLeft+style->checkSize,bottom+style->checkSize},
                                tint(style->imageColor.get()),image)) return false;
                            left+=state.widths[column]+params.columnPadding;
                            continue;
                        }
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
        if (node->textureControl)
        {
            if (!tree.refreshTextureControl(id,error)) return false;
            node=tree.get(id);
            if (!node) return true;
            const auto texture=*node->textureControl;
            const Rect interior{1,texture.params->captionHeight+1,width-1,height-1};
            const auto alpha=input.button.drawAlpha;
            const auto preview=texture.valid ? texture.preview : nullptr;
            if (preview && texture.previewHasAlpha)
            {
                const auto checker=tree.findImage("Checker",error);
                if (!checker) return false;
                const auto pixels=checker->bottomUpRgba();
                for (int bottom=interior.bottom; bottom<interior.top; ++bottom)
                    for (int left=interior.left; left<interior.right; ++left)
                    {
                        const auto column=static_cast<std::uint32_t>(((left-interior.left)%32+0.5f)*checker->pixelWidth()/32.f);
                        const auto row=static_cast<std::uint32_t>(((bottom-interior.bottom)%32+0.5f)*checker->pixelHeight()/32.f);
                        const auto offset=4*(std::size_t(row)*checker->pixelWidth()+column);
                        if (!append({left,bottom,left+1,bottom+1},{pixels[offset]/255.f,pixels[offset+1]/255.f,pixels[offset+2]/255.f,alpha*pixels[offset+3]/255.f})) return false;
                    }
            }
            if (preview || texture.params->fallback)
            {
                if (!append(interior,{1,1,1,alpha},preview ? preview : texture.params->fallback)) return false;
            }
            else
            {
                if (!append(interior,{0.5f,0.5f,0.5f,alpha})) return false;
                const auto rows=interior.top-interior.bottom,columns=interior.right-interior.left;
                for (int row=0; row<rows && columns>0; ++row)
                {
                    const auto offset=static_cast<int>(std::int64_t(row)*columns/std::max(1,rows));
                    const auto left=interior.left+offset,right=interior.right-offset-1,bottom=interior.bottom+row;
                    if (!append({left,bottom,left+1,bottom+1},{0,0,0,alpha}) || !append({right,bottom,right+1,bottom+1},{0,0,0,alpha})) return false;
                }
            }
            auto border=texture.params->borderColor.get(); border[3]*=alpha;
            const auto bottom=texture.params->captionHeight;
            for (const Rect edge : {Rect{0,bottom,width,bottom+1},Rect{0,height-1,width,height},
                Rect{0,bottom+1,1,height-1},Rect{width-1,bottom+1,width,height-1}})
                if (!append(edge,border)) return false;
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
            for (auto& channel : color)
            {
                if (!std::isfinite(channel)) { error="Native swatch color is nonfinite"; return false; }
                channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
            }
            if (!append(interior,color)) return false;
            if (swatch.color[3]<1.f && swatch.params->alphaBackground && !append(interior,color,swatch.params->alphaBackground)) return false;
            auto border=swatch.params->borderColor.get();
            for (auto& channel : border)
            {
                if (!std::isfinite(channel)) { error="Native swatch border color is nonfinite"; return false; }
                channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
            }
            const auto bottom=swatch.params->labelHeight;
            if (scale==1.f)
            {
                for (const Rect edge : {Rect{0,bottom-1,width-1,bottom},Rect{0,height-2,width-1,height-1},
                    Rect{-1,bottom,0,height-1},Rect{width-2,bottom,width-1,height-1}})
                    if (!append(edge,border)) return false;
            }
            else
            {
                const auto left=screen->left*scale,right=(screen->right-1)*scale;
                const auto low=(screen->bottom+bottom)*scale,high=(screen->top-1)*scale;
                pixelLine(left,high,left,low,border);
                pixelLine(left,low,right,low,border);
                pixelLine(right,low,right,high,border);
                pixelLine(right,high,left,high,border);
            }
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
            if (node->floater && node->floater->dropShadow && !alertShadow(1,input.button.transparency,true)) return false;
            const auto& panel = node->panel->params;
            const bool opaque=node->floater && input.foregroundFloaters ? input.foregroundFloaters->contains(id) : panel.backgroundOpaque;
            const auto image = opaque ? panel.opaqueImage : panel.transparentImage;
            auto color = image ? (opaque ? panel.opaqueImageOverlay.get() : panel.transparentImageOverlay.get()) :
                (opaque ? panel.opaqueColor.get() : panel.transparentColor.get());
            color[3] *= input.button.transparency;
            for (auto& channel : color)
            {
                if (!std::isfinite(channel)) { error="Native panel color is nonfinite"; return false; }
                channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
            }
            if (!append({0,0,width,height},color,image)) return false;
        }
        if (!alertShadow(0,1.f)) return false;
        if (node->overlapPanel)
        {
            const auto state=*node->overlapPanel;
            int top=height-20;
            const auto label=[&](const std::string& value) -> bool
            {
                const auto wide=utf8str_to_wstring(value);
                const std::u32string text(wide.begin(),wide.end());
                LLVKFont::LineOptions options; options.x=5; options.y=static_cast<float>(top);
                const auto line=state.font->layoutLine(text,0,text.size(),options,error);
                return line && append({},{.5f,.5f,.5f,1.f},{},*line);
            };
            if (state.elements.empty()) return label("Current selection: ");
            bool first=true;
            for (const auto source : state.elements)
            {
                const auto* sourceNode=tree.get(source);
                if (!sourceNode) continue;
                if (!first)
                {
                    top-=10;
                    if (!append({5,top,width-5,top+1},{192.f/255,192.f/255,192.f/255,1.f})) return false;
                    top-=10;
                }
                if (!label(std::string(first ? "Current selection: " : "Overlapper: ")+sourceNode->params.name)) return false;
                const auto rectangle=tree.screenRect(source,error);
                if (!rectangle) return false;
                top-=10+rectangle->top-rectangle->bottom;
                auto snapshotInput=input; snapshotInput.button.frameDelta=0;
                const auto snapshot=LLVKWidgetPaint::prepare(tree,source,snapshotInput,error);
                if (!snapshot) return false;
                const auto dx=screen->left+5-rectangle->left,dy=screen->bottom+top-rectangle->bottom;
                for (auto command : snapshot->commands)
                {
                    command.owner=id;
                    command.rectangle={command.rectangle.left+dx,command.rectangle.bottom+dy,command.rectangle.right+dx,command.rectangle.top+dy};
                    command.clip=intersect(clip,{command.clip.left+dx,command.clip.bottom+dy,command.clip.right+dx,command.clip.top+dy});
                    if (command.text)
                        for (auto& glyph : command.text->glyphs) { glyph.left+=dx; glyph.right+=dx; glyph.bottom+=dy; glyph.top+=dy; }
                    if (command.triangle)
                        for (std::size_t coordinate=0; coordinate<command.triangle->size(); coordinate+=2)
                        { (*command.triangle)[coordinate]+=dx; (*command.triangle)[coordinate+1]+=dy; }
                    output.commands.push_back(std::move(command));
                }
                first=false;
            }
            return true;
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
                const auto display=LLVKBrowserSurface::displayRect(width,height,browser->second->width(),browser->second->height());
                if (!append({display.left,display.bottom,display.right,display.top},{1,1,1,input.button.drawAlpha},browser->second)) return false;
                output.commands.back().streamingImage = true;
                const auto epoch = input.browserEpochs.find(id);
                if (epoch != input.browserEpochs.end()) output.commands.back().imageEpoch = epoch->second;
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
                if (!line || !append({},searchHighlighted ? searchFont : draw->labelColor,{},std::move(line),false,false,draw->shadow)) return false;
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
            const auto visibleClip = intersect(clip,*screen);
            auto color = text.readOnly ? text.params.readOnlyColor.get() : text.params.textColor.get();
            color[3] *= input.button.drawAlpha;
            std::optional<std::size_t> lastVisible;
            std::optional<Rect> visibleLines;
            for (std::size_t index=0; index<text.layout->lines.size(); ++index)
            {
                const auto& line=text.layout->lines[index];
                const auto bottom=line.bottom+document->params.rect.bottom+screen->bottom;
                const auto top=line.top+document->params.rect.bottom+screen->bottom;
                if (text.params.clipPartial ? bottom>=visibleClip.bottom && top<=visibleClip.top : bottom<visibleClip.top && top>visibleClip.bottom)
                {
                    lastVisible=index;
                    const Rect lineRect{screen->left+document->params.rect.left+line.left,bottom,
                        screen->left+document->params.rect.left+line.right,top};
                    if (!visibleLines) visibleLines=lineRect;
                    else visibleLines=Rect{std::min(visibleLines->left,lineRect.left),std::min(visibleLines->bottom,bottom),
                        std::max(visibleLines->right,lineRect.right),std::max(visibleLines->top,top)};
                }
            }
            if (visibleLines)
            {
                if (searchHighlighted && !append({visibleLines->left-screen->left,visibleLines->bottom-screen->bottom,
                    visibleLines->right-screen->left,visibleLines->top-screen->bottom},searchBackground)) return false;
                if (visibleLines->top-screen->bottom>2) visibleLines->top-=2;
                ++visibleLines->right; ++visibleLines->top;
                clip=intersect(clip,*visibleLines);
            }
            for (std::size_t index=0; index<text.layout->lines.size(); ++index)
            {
                const auto& sourceLine=text.layout->lines[index];
                const auto bottom=sourceLine.bottom+document->params.rect.bottom+screen->bottom;
                const auto top=sourceLine.top+document->params.rect.bottom+screen->bottom;
                if (text.params.clipPartial ? bottom<visibleClip.bottom || top>visibleClip.top : bottom>=visibleClip.top || top<=visibleClip.bottom) continue;
                const auto lineEnd=std::min(sourceLine.end,text.text.size());
                float runLeft=float(sourceLine.left+document->params.rect.left);
                for (auto begin=std::min(sourceLine.begin,text.text.size()); begin<lineEnd; )
                {
                const auto icon=text.icons.lower_bound(begin);
                if (icon!=text.icons.end() && icon->first==begin)
                {
                    const auto imageWidth=static_cast<int>(icon->second->width()),imageHeight=static_cast<int>(icon->second->height());
                    const auto imageBottom=static_cast<int>((sourceLine.top+sourceLine.bottom)*0.5f)+document->params.rect.bottom-imageHeight/2;
                    if (!append({static_cast<int>(runLeft),imageBottom,static_cast<int>(runLeft)+imageWidth,imageBottom+imageHeight},
                        {1,1,1,input.button.drawAlpha},icon->second)) return false;
                    runLeft+=imageWidth+3;
                    ++begin;
                    continue;
                }
                auto end=icon==text.icons.end() ? lineEnd : std::min(lineEnd,icon->first);
                for (const auto& link : text.links)
                    for (const auto boundary : {link.begin,link.end})
                        if (boundary>begin) end=std::min(end,boundary);
                auto count = end-begin;
                if (count && text.text[begin+count-1] == U'\n') --count;
                LLVKFont::LineOptions options;
                options.x = runLeft;
                options.y = float(sourceLine.bottom+document->params.rect.bottom);
                options.vertical = LLVKFont::VerticalAlign::Bottom;
                options.maxPixels=std::max(0,static_cast<int>(document->params.rect.right-runLeft));
                if (text.params.vertical==LLVKFont::VerticalAlign::Top)
                {
                    options.y=float(sourceLine.top+document->params.rect.bottom);
                    options.vertical=LLVKFont::VerticalAlign::Top;
                }
                else if (text.params.vertical==LLVKFont::VerticalAlign::Center)
                {
                    options.y=(sourceLine.top+sourceLine.bottom)*0.5f+document->params.rect.bottom;
                    options.vertical=LLVKFont::VerticalAlign::Center;
                }
                if (text.params.useEllipses)
                {
                    options.ellipses=true;
                    options.maxPixels=std::max(0,width-text.params.layout.horizontalPadding-sourceLine.left-document->params.rect.left);
                    if (lastVisible==index && index+1<text.layout->lines.size()) options.maxPixels=std::max(0,options.maxPixels-2);
                }
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
                const auto measured=node->control->params.font->measureRun(text.text,begin,count,1.f,true,text.params.layout.tabularNumbers,error);
                if (!measured) return false;
                if (text.links.empty()) runLeft+=measured->width;
                else runLeft=line->endPixelX;
                if (text.links.empty() && !selection)
                { if (!append({},color,{},std::move(line),false,false,text.params.softShadow)) return false; }
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
                        if (!append({},foreground,{},std::move(part),false,false,text.params.softShadow)) return false;
                        const auto hoveredLink=text.params.skipLinkUnderline ? tree.plainTextLinkAt(id,
                            input.button.mouseX-screen->left,input.button.mouseY-screen->bottom,error) : std::optional<std::size_t>();
                        if (!error.empty()) return false;
                        if (link && (!text.params.skipLinkUnderline || (hoveredLink && &text.links[*hoveredLink]==link)))
                        {
                            const auto& initial = line->glyphs[first];
                            const auto& final = line->glyphs[last-1];
                            if (scale==1.f)
                            {
                                const auto left = static_cast<std::int32_t>(std::floor(initial.left-initial.glyph->raster.bearingX+0.5f));
                                const auto right = static_cast<std::int32_t>(std::floor(final.left-final.glyph->raster.bearingX+final.glyph->raster.advanceX+0.5f));
                                const auto bottom = static_cast<std::int32_t>(std::floor(line->baselinePixelY-std::floor(node->control->params.font->metrics().descender)));
                                if (right > left && !append({left,bottom-1,right,bottom},foreground)) return false;
                            }
                            else
                            {
                                const auto originX=std::floor(screen->left*scale),originY=std::floor(screen->bottom*scale);
                                const auto left=originX+initial.left*scale-initial.glyph->raster.bearingX;
                                const auto right=originX+(last==line->glyphs.size() ? line->endPixelX*scale :
                                    final.left*scale-final.glyph->raster.bearingX+final.glyph->raster.advanceX);
                                const auto bottom=originY+line->baselinePixelY*scale-std::floor(node->control->params.font->metrics().descender*scale);
                                pixelLine(left,bottom,right,bottom,foreground);
                            }
                        }
                        first = last;
                    }
                }
                begin=end;
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
                    if (thickness==1)
                    {
                        for (auto* color : {&top,&bottom})
                            for (auto& channel : *color) channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
                        if (scale==1.f)
                        {
                            if (!append({-1,0,0,height},top) || !append({0,height-1,width,height},top) ||
                                !append({width-1,0,width,height},bottom) || !append({0,-1,width,0},bottom)) return false;
                        }
                        else
                        {
                            const auto left=screen->left*scale,right=screen->right*scale;
                            const auto low=screen->bottom*scale,high=screen->top*scale;
                            pixelLine(left,low,left,high,top);
                            pixelLine(left,high,right,high,top);
                            pixelLine(right,high,right,low,bottom);
                            pixelLine(left,low,right,low,bottom);
                        }
                    }
                    else if (!upper(0,thickness,top) || !lower(0,thickness,bottom)) return false;
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
                childClip=intersect(childClip,{screen->left+3,screen->bottom,screen->right-2,screen->top+1});
            if (clipPanels && current->layoutPanel)
            {
                auto visible = tree.screenRect(*child,error);
                if (!visible) return false;
                const auto amount = current->layoutPanel->visibleAmount;
                if (vertical) visible->bottom = visible->top-static_cast<std::int32_t>(std::floor((visible->top-visible->bottom)*amount+0.5f));
                else visible->right = visible->left+static_cast<std::int32_t>(std::floor((visible->right-visible->left)*amount+0.5f));
                if (visible->left>=visible->right || visible->bottom>=visible->top) continue;
                if (visible->right<clip.right) ++visible->right;
                if (visible->top<clip.top) ++visible->top;
                childClip = intersect(childClip,*visible);
            }
            if (childClip.left < childClip.right && childClip.bottom < childClip.top && !self(self,*child,childClip)) return false;
        }
        node=tree.get(id);
        if (node && node->tabContainer && node->tabContainer->layout)
        {
            const auto layout=*node->tabContainer->layout;
            if (!tree.layoutTabPanels(id,layout,error,input.button.frameDelta)) return false;
            node=tree.get(id);
        }
        if (node && !alertShadow(1,input.button.transparency)) return false;
        if (node && node->colorPicker)
        {
            const auto& picker=*node->colorPicker;
            const auto alpha=input.button.drawAlpha;
            const auto pickerFill=[&](Rect rectangle,LLVKColor::Value color)
            {
                for (auto& channel : color)
                {
                    if (!std::isfinite(channel)) { error="Native picker color is nonfinite"; return false; }
                    channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
                }
                return append(rectangle,color);
            };
            if (!append({140,100,396,356},{1,1,1,alpha},picker.hueImage)) return false;
            const auto outline=[&](Rect rect,LLVKColor::Value color,bool inverted=false)
            {
                const auto low=rect.bottom-(inverted ? 1 : 0),high=rect.top-(inverted ? 0 : 1);
                if (scale!=1.f)
                {
                    for (auto& channel : color) channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
                    const auto left=(screen->left+rect.left)*scale,right=(screen->left+rect.right-1)*scale;
                    const auto start=(screen->bottom+(inverted ? low : high))*scale;
                    const auto end=(screen->bottom+(inverted ? high : low))*scale;
                    pixelLine(left,start,left,end,color);
                    pixelLine(left,end,right,end,color);
                    pixelLine(right,end,right,start,color);
                    pixelLine(right,start,left,start,color);
                    return true;
                }
                return pickerFill({rect.left,low-1,rect.right-1,low},color) &&
                    pickerFill({rect.left,high-1,rect.right-1,high},color) &&
                    pickerFill({rect.left-1,low,rect.left,high},color) &&
                    pickerFill({rect.right-2,low,rect.right-1,high},color);
            };
            const auto hueX=140+static_cast<int>(256.f*picker.hsl[0]);
            const auto saturationY=100+static_cast<int>(256.f*picker.hsl[1]);
            if (scale==1.f)
            {
                if (!append({hueX-8,saturationY-1,hueX+8,saturationY},{0,0,0,1}) ||
                    !append({hueX-1,saturationY-8,hueX,saturationY+8},{0,0,0,1})) return false;
            }
            else
            {
                pixelLine((screen->left+hueX-8)*scale,(screen->bottom+saturationY)*scale,
                    (screen->left+hueX+8)*scale,(screen->bottom+saturationY)*scale,{0,0,0,1});
                pixelLine((screen->left+hueX)*scale,(screen->bottom+saturationY-8)*scale,
                    (screen->left+hueX)*scale,(screen->bottom+saturationY+8)*scale,{0,0,0,1});
            }
            if (!outline({140,100,397,356},{0,0,0,alpha},true)) return false;
            for (int row=0; row<256; ++row)
            {
                LLColor3 color; color.setHSL(picker.hsl[0],picker.hsl[1],float(row)/256.f);
                if (!pickerFill({412,99+row,428,100+row},{color.mV[0],color.mV[1],color.mV[2],alpha})) return false;
            }
            const auto markerY=100+static_cast<int>(256.f*picker.hsl[2]);
            if (!pickerFill({428,markerY-6,434,markerY+6},{0.75f,0.75f,0.75f,1})) return false;
            output.commands.back().triangle=std::array<float,6>{float(screen->left+428),float(screen->bottom+markerY),
                float(screen->left+434),float(screen->bottom+markerY-6),float(screen->left+434),float(screen->bottom+markerY+6)};
            if (!outline({412,100,429,356},{0,0,0,1},true) ||
                !pickerFill({12,130,128,190},{picker.rgb[0],picker.rgb[1],picker.rgb[2],alpha}) ||
                !outline({12,130,129,190},{0,0,0,1},true)) return false;
            if (picker.paletteReady)
                for (int index=0; index<32; ++index)
                {
                    const auto column=index%16, row=index/16;
                    const auto left=11+418*column/16, right=11+418*(column+1)/16;
                    const auto top=92-40*row/2, bottom=92-40*(row+1)/2;
                    auto color=picker.palette[index]; color[3]*=alpha;
                    if (!pickerFill({left+2,bottom+2,right-2,top-2},color) || !outline({left+1,bottom+1,right-1,top-1},{0,0,0,1})) return false;
                }
        }
        return true;
    };
    if (!visit(visit,root,*rootRect)) return std::nullopt;
    paintingPopups = true;
    for (const auto popup : popups) if (!visit(visit,popup,*rootRect)) return std::nullopt;
    for (const auto popup : menuPopups) if (!paintMenu(popup,true)) return std::nullopt;
    return output;
}
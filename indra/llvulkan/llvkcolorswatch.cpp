#include "llvkwidgettree.h"
#include <algorithm>
#include <cmath>
#include <charconv>
#include "v3color.h"
#include "llvkclipboard.h"
#include "llstring.h"

namespace
{
    LLSD colorValue(const LLVKColor::Value& color)
    {
        LLSD value=LLSD::emptyArray();
        for (const auto channel : color) value.append(channel);
        return value;
    }
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createColorSwatch(const Params& view,
    const LLVKControl::Params& control,const ColorSwatchParams& params,Id parent,std::string& error)
{
    error.clear();
    const auto width=view.rect.right-view.rect.left, height=view.rect.top-view.rect.bottom;
    if (width<2 || params.labelHeight<0 || height<params.labelHeight+2 || params.labelWidth<-1)
    { error="Invalid native color swatch geometry"; return std::nullopt; }
    for (const auto channel : params.color.get())
        if (!std::isfinite(channel)) { error="Nonfinite native swatch color"; return std::nullopt; }
    auto initial=control;
    initial.init={}; initial.initialValue.reset(); initial.valueSetting.reset();
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        ColorSwatch swatch;
        swatch.params=std::make_shared<ColorSwatchParams>(params);
        swatch.color=swatch.original=swatch.pending=params.color.get();
        mNodes.at(*id).colorSwatch=std::move(swatch);
        Params child;
        child.name="caption"; child.mouseOpaque=false;
        child.rect={0,0,params.labelWidth<0 ? width : params.labelWidth,params.labelHeight};
        child.follows=Left|Right|Bottom;
        auto captionControl=params.captionControl;
        if (!captionControl.font) captionControl.font=control.font;
        captionControl.tabStop=false; captionControl.initialValue=params.label;
        auto captionParams=params.caption;
        captionParams.textColor=params.enabledText; captionParams.readOnlyColor=params.disabledText;
        const auto caption=createPlainText(child,captionControl,captionParams,*id,error);
        if (!caption) { discard(); return std::nullopt; }
        mNodes.at(*id).colorSwatch->caption=*caption;
        child.name="swatch border"; child.rect={0,params.labelHeight,width-1,height-1};
        child.follows=Left|Right|Top|Bottom;
        const auto border=createBorder(child,params.border,*id,error);
        if (!border) { discard(); return std::nullopt; }
        mNodes.at(*id).colorSwatch->border=*border;
        mNodes.at(*id).control->params=control;
        auto value=control.initialValue.value_or(colorValue(params.color.get()));
        if (control.valueSetting && mSettings.contains(*control.valueSetting)) value=mSettings.at(*control.valueSetting);
        if (!setColorSwatchValue(*id,value,error)) { discard(); return std::nullopt; }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native color swatch removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::setColorSwatchValue(Id id,const LLSD& value,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->colorSwatch || !value.isArray() || value.size()!=4)
    { error="Native color swatch requires four color channels"; return false; }
    LLVKColor::Value color;
    for (std::size_t index=0; index<color.size(); ++index)
    {
        color[index]=static_cast<float>(value[static_cast<LLSD::Integer>(index)].asReal());
        if (!std::isfinite(color[index])) { error="Nonfinite native swatch channel"; return false; }
    }
    auto& state=*mNodes.at(id).colorSwatch;
    const bool changed=state.color!=color;
    state.color=state.pending=color;
    if (changed) ++state.generation;
    mNodes.at(id).control->value=colorValue(color);
    return true;
}

bool LLVKWidgetTree::refreshColorSwatch(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->colorSwatch) return false;
    const auto state=*node->colorSwatch;
    if (get(state.caption)) setEnabled(state.caption,enabledInChain(id));
    if (get(state.border) && get(state.border)->border)
        mNodes.at(state.border).border->keyboardFocus=mKeyboardFocus==id;
    return true;
}

bool LLVKWidgetTree::showColorSwatchPicker(Id id,bool takeFocus,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->colorSwatch || !enabledInChain(id)) return false;
    const auto callback=node->colorSwatch->params->showPicker;
    if (!callback) { error="Native color picker service is not bound"; return false; }
    if (!beginColorSelection(id,error)) return false;
    callback(id,takeFocus);
    return true;
}

bool LLVKWidgetTree::colorSwatchPointer(Id id,const PointerEvent& event,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->colorSwatch) return false;
    if (event.kind==PointerKind::Hover) { cursorEffect(id,true); return true; }
    if (event.kind==PointerKind::LeftDown || event.kind==PointerKind::DoubleClick) return setMouseCapture(id,error);
    if (event.kind!=PointerKind::LeftUp) return basePointer(id,event,error);
    if (mMouseCapture!=id) return true;
    if (!setMouseCapture(0,error)) return false;
    node=get(id);
    if (!node) return true;
    const auto width=node->params.rect.right-node->params.rect.left, height=node->params.rect.top-node->params.rect.bottom;
    if (event.x>=0 && event.x<width && event.y>=0 && event.y<height)
    {
        if (!requestControlFocus(id,true,error)) return false;
        return showColorSwatchPicker(id,false,error);
    }
    return true;
}

bool LLVKWidgetTree::beginColorSelection(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->colorSwatch || !node->colorSwatch->valid || !enabledInChain(id)) return false;
    auto& state=*mNodes.at(id).colorSwatch;
    state.original=state.pending=state.color;
    for (std::size_t channel=0; channel<3; ++channel)
        state.original[channel]=state.pending[channel]=std::clamp(state.color[channel],0.f,1.f);
    state.picking=true;
    ++state.generation;
    return true;
}

bool LLVKWidgetTree::applyColorSelection(Id id,const LLVKColor::Value& input,ColorPickOperation operation,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->colorSwatch || !node->colorSwatch->picking) return false;
    if (operation!=ColorPickOperation::Change && operation!=ColorPickOperation::Select && operation!=ColorPickOperation::Cancel) return false;
    auto color=operation==ColorPickOperation::Cancel ? node->colorSwatch->original : input;
    for (std::size_t channel=0; channel<3; ++channel)
    {
        if (!std::isfinite(color[channel])) { error="Nonfinite native picked color"; return false; }
        color[channel]=std::clamp(color[channel],0.f,1.f);
    }
    color[3]=node->colorSwatch->color[3];
    const auto params=node->colorSwatch->params;
    mNodes.at(id).colorSwatch->pending=color;
    if (operation==ColorPickOperation::Change && !params->applyImmediately) return true;
    const bool changed=color!=node->colorSwatch->color;
    if (!setColorSwatchValue(id,colorValue(color),error)) return false;
    const auto generation=get(id)->colorSwatch->generation;
    if (changed) writeBoundValue(id,colorValue(color));
    if (!get(id) || !get(id)->colorSwatch || get(id)->colorSwatch->generation!=generation) return true;
    const auto callback=operation==ColorPickOperation::Cancel ? params->cancelled :
        operation==ColorPickOperation::Select ? params->selected : LLVKControl::Callback{};
    if (operation!=ColorPickOperation::Change) mNodes.at(id).colorSwatch->picking=false;
    if (callback.function) callback.function(id,callback.parameter.value_or(LLSD()));
    else dispatchControl(id,&LLVKControl::Params::commit);
    if (operation!=ColorPickOperation::Change && get(id) && canReceiveFocus(id) && enabledInChain(id)) requestControlFocus(id,true,error);
    return error.empty();
}

bool LLVKWidgetTree::initializeColorPicker(Id picker,Id swatch,std::string& error,std::function<void()> close)
{
    error.clear();
    const auto* owner=get(picker);
    const auto* source=get(swatch);
    if (!owner || !owner->floater || !source || !source->colorSwatch || owner->colorPicker)
    { error="Native color picker requires a fresh floater and swatch"; return false; }
    ColorPicker state;
    state.swatch=swatch;
    state.close=std::move(close);
    std::vector<std::uint8_t> pixels(256*256*4);
    for (std::size_t row=0; row<256; ++row)
        for (std::size_t column=0; column<256; ++column)
        {
            LLColor3 color;
            color.setHSL(float(column*3)/767.f,float(row)/255.f,0.5f);
            const auto offset=4*(row*256+column);
            for (std::size_t channel=0; channel<3; ++channel)
                pixels[offset+channel]=static_cast<std::uint8_t>(color.mV[channel]*255.f);
            pixels[offset+3]=255;
        }
    state.hueImage=LLVKWidgetImage::fromRgba("native-color-picker-hue",256,256,pixels,error);
    if (!state.hueImage) return false;
    state.immediate=source->colorSwatch->params->applyImmediately && setting("ApplyColorImmediately").value_or(LLSD(false)).asBoolean();
    std::vector<Id> pending=owner->children;
    while (!pending.empty())
    {
        const auto id=pending.back(); pending.pop_back();
        const auto* node=get(id);
        if (!node) continue;
        state.fields.try_emplace(node->params.name,id);
        pending.insert(pending.end(),node->children.begin(),node->children.end());
    }
    for (const auto name : {"rspin","gspin","bspin","rspin_lsl","gspin_lsl","bspin_lsl","hspin","sspin","lspin","hex_value","apply_immediate","select_btn","cancel_btn","copy_lsl_btn","color_pipette"})
        if (!state.fields.contains(name)) { error=std::string("Native picker missing original control: ")+name; return false; }
    if (!beginColorSelection(swatch,error)) return false;
    mNodes.at(picker).colorPicker=std::move(state);
    const auto fields=get(picker)->colorPicker->fields;
    for (const auto name : {"rspin","gspin","bspin","rspin_lsl","gspin_lsl","bspin_lsl","hspin","sspin","lspin","hex_value"})
    {
        LLVKControl::Callback callback;
        callback.function=[this,picker](Id field,const LLSD&) { std::string problem; commitColorPickerField(picker,field,problem); };
        setControlCommit(fields.at(name),std::move(callback));
    }
    setEnabled(fields.at("apply_immediate"),source->colorSwatch->params->applyImmediately);
    setValue(fields.at("apply_immediate"),LLSD(get(picker)->colorPicker->immediate));
    LLVKControl::Callback immediate;
    immediate.function=[this,picker](Id field,const LLSD&)
    {
        if (!get(picker) || !get(picker)->colorPicker) return;
        const auto source=get(get(picker)->colorPicker->swatch);
        if (!source || !source->colorSwatch) return;
        const bool enabled=source->colorSwatch->params->applyImmediately && value(field).asBoolean();
        mNodes.at(picker).colorPicker->immediate=enabled;
        updateSetting("ApplyColorImmediately",LLSD(enabled));
        if (!get(picker) || !get(picker)->colorPicker) return;
        const auto state=*get(picker)->colorPicker;
        std::string problem;
        if (enabled) applyColorSelection(state.swatch,state.rgb,ColorPickOperation::Change,problem);
    };
    setControlCommit(fields.at("apply_immediate"),std::move(immediate));
    LLVKControl::Callback copy;
    copy.function=[this,picker](Id,const LLSD&) { std::string problem; copyColorPickerLsl(picker,problem); };
    setControlCommit(fields.at("copy_lsl_btn"),std::move(copy));
    for (const bool accept : {false,true})
    {
        LLVKControl::Callback callback;
        callback.function=[this,picker,accept](Id,const LLSD&) { std::string problem; finishColorPicker(picker,accept,problem); };
        setControlCommit(fields.at(accept ? "select_btn" : "cancel_btn"),std::move(callback));
    }
    return setColorPickerRgb(picker,source->colorSwatch->color,false,error);
}

void LLVKWidgetTree::closeColorSwatchPickers(Id swatch)
{
    std::vector<Id> pickers;
    for (const auto& [id,node] : mNodes)
        if (node.colorPicker && node.colorPicker->swatch==swatch) pickers.push_back(id);
    for (const auto picker : pickers)
    {
        const auto* node=get(picker);
        if (!node || !node->colorPicker) continue;
        const auto close=node->colorPicker->close;
        const auto* source=get(swatch);
        if (source && source->colorSwatch && source->colorSwatch->picking)
        {
            std::string problem;
            applyColorSelection(swatch,{},ColorPickOperation::Cancel,problem);
        }
        if (!get(picker)) continue;
        if (close) close();
        else setVisible(picker,false);
    }
}

bool LLVKWidgetTree::finishColorPicker(Id picker,bool accept,std::string& error)
{
    error.clear();
    const auto* node=get(picker);
    if (!node || !node->colorPicker) return false;
    const auto state=*node->colorPicker;
    if (!applyColorSelection(state.swatch,state.rgb,accept ? ColorPickOperation::Select : ColorPickOperation::Cancel,error)) return false;
    if (!get(picker)) return true;
    if (state.close) state.close();
    else setVisible(picker,false);
    return true;
}

bool LLVKWidgetTree::copyColorPickerLsl(Id picker,std::string& error)
{
    error.clear();
    const auto* node=get(picker);
    if (!node || !node->colorPicker || !mClipboard) { error="Native picker clipboard unavailable"; return false; }
    const auto color=node->colorPicker->rgb;
    std::string text="<";
    for (std::size_t channel=0; channel<3; ++channel)
    {
        char buffer[32];
        const auto converted=std::to_chars(buffer,buffer+sizeof(buffer),color[channel],std::chars_format::fixed,3);
        if (converted.ec!=std::errc()) { error="Native picker LSL formatting failed"; return false; }
        if (channel) text+=", ";
        text.append(buffer,converted.ptr);
    }
    text+='>';
    const std::u32string display(text.begin(),text.end());
    const auto clipboard=mClipboard;
    return clipboard->write(display,false,error);
}

bool LLVKWidgetTree::setColorPickerPalette(Id picker,std::shared_ptr<LLVKColorTable> colors,std::string& error)
{
    error.clear();
    if (!get(picker) || !get(picker)->colorPicker || !colors) return false;
    std::array<LLVKColor::Value,32> palette;
    for (std::size_t index=0; index<palette.size(); ++index)
    {
        const auto name="ColorPaletteEntry"+std::string(index<9 ? "0" : "")+std::to_string(index+1);
        const auto color=colors->find(name);
        if (!color) { error="Native picker missing palette color: "+name; return false; }
        palette[index]=color->get();
    }
    mNodes.at(picker).colorPicker->palette=palette;
    mNodes.at(picker).colorPicker->paletteReady=true;
    mNodes.at(picker).colorPicker->paletteColors=std::move(colors);
    return true;
}

bool LLVKWidgetTree::colorPickerPointer(Id picker,const PointerEvent& event,std::string& error)
{
    error.clear();
    if (!get(picker) || !get(picker)->colorPicker) return false;
    const auto state=*get(picker)->colorPicker;
    const auto inside=[&](const Rect& rect)
    { return event.x>=rect.left && event.x<rect.right && event.y>=rect.bottom && event.y<rect.top; };
    const auto selectHsl=[&](ColorPicker::Drag drag)
    {
        auto hsl=get(picker)->colorPicker->hsl;
        if (drag==ColorPicker::Drag::Hue)
        {
            hsl[0]=float(std::clamp(event.x,140,396)-140)/256.f;
            hsl[1]=float(std::clamp(event.y,100,356)-100)/256.f;
        }
        else hsl[2]=float(std::clamp(event.y,100,356)-100)/256.f;
        LLColor3 color; color.setHSL(hsl[0],hsl[1],hsl[2]);
        auto rgb=get(picker)->colorPicker->rgb;
        for (std::size_t index=0; index<3; ++index) rgb[index]=color.mV[index];
        auto& current=*mNodes.at(picker).colorPicker;
        current.rgb=rgb; current.hsl=hsl;
        if (!syncColorPickerFields(picker,error)) return false;
        return !state.immediate || applyColorSelection(state.swatch,rgb,ColorPickOperation::Change,error);
    };
    if (event.kind==PointerKind::LeftDown)
    {
        if (inside({140,100,396,356}) || inside({412,100,434,356}))
        {
            const auto drag=event.x<396 ? ColorPicker::Drag::Hue : ColorPicker::Drag::Luminance;
            if (!setMouseCapture(picker,error) || !get(picker)) return false;
            mNodes.at(picker).colorPicker->drag=drag;
            if (!requestControlFocus(state.fields.at("select_btn"),true,error) || !get(picker)) return false;
            return drag==ColorPicker::Drag::Luminance || selectHsl(drag);
        }
        mNodes.at(picker).colorPicker->drag=ColorPicker::Drag::None;
        if (inside({12,130,128,190}))
        { mNodes.at(picker).colorPicker->drag=ColorPicker::Drag::Swatch; return true; }
        if (inside({11,52,429,92}) && state.paletteReady)
        {
            if (!requestControlFocus(state.fields.at("select_btn"),true,error) || !get(picker)) return false;
            const auto column=(event.x-11)*16/418, row=(event.y-52)*2/40;
            const auto index=(1-row)*16+column;
            if (index<0 || index>=32) return false;
            if (!setColorPickerRgb(picker,state.palette[index],true,error)) return false;
            return !state.immediate || !get(picker) || applyColorSelection(state.swatch,state.palette[index],ColorPickOperation::Change,error);
        }
    }
    if (event.kind==PointerKind::Hover)
    {
        if (state.drag==ColorPicker::Drag::Hue || state.drag==ColorPicker::Drag::Luminance)
        {
            if (mMouseCapture!=picker) { mNodes.at(picker).colorPicker->drag=ColorPicker::Drag::None; return false; }
            return selectHsl(state.drag);
        }
        if (state.drag==ColorPicker::Drag::Swatch)
        {
            mNodes.at(picker).colorPicker->highlighted=inside({11,52,429,92}) ? (event.x-11)*16/418+(91-event.y)*2/40*16 : -1;
            return true;
        }
    }
    if (event.kind==PointerKind::LeftUp && state.drag!=ColorPicker::Drag::None)
    {
        mNodes.at(picker).colorPicker->drag=ColorPicker::Drag::None;
        mNodes.at(picker).colorPicker->highlighted=-1;
        if (mMouseCapture==picker && !setMouseCapture(0,error)) return false;
        if (!get(picker)) return true;
        if (state.drag==ColorPicker::Drag::Swatch && state.paletteReady && inside({11,52,429,92}))
        {
            const auto index=(event.x-11)*16/418+(91-event.y)*2/40*16;
            const auto column=index%16, row=index/16;
            const Rect cell{11+418*column/16,92-40*(row+1)/2,11+418*(column+1)/16,92-40*row/2};
            if (index>=0 && index<32 && inside(cell))
            {
                auto color=state.rgb; color[3]=1.f;
                const auto name="ColorPaletteEntry"+std::string(index<9 ? "0" : "")+std::to_string(index+1);
                if (!state.paletteColors || !state.paletteColors->set(name,color))
                { error="Native picker palette update failed"; return false; }
                mNodes.at(picker).colorPicker->palette[index]=color;
            }
        }
        if ((state.drag==ColorPicker::Drag::Hue || state.drag==ColorPicker::Drag::Luminance) && state.immediate)
            return applyColorSelection(state.swatch,state.rgb,ColorPickOperation::Change,error);
        return true;
    }
    return basePointer(picker,event,error);
}

bool LLVKWidgetTree::syncColorPickerFields(Id picker,std::string& error)
{
    error.clear();
    const auto* node=get(picker);
    if (!node || !node->colorPicker) return false;
    const auto state=*node->colorPicker;
    mNodes.at(picker).colorPicker->synchronizing=true;
    const auto assign=[&](const char* name,const LLSD& value)
    {
        if (setValue(state.fields.at(name),value)) return true;
        error=std::string("Native picker could not update field: ")+name;
        return false;
    };
    bool valid=true;
    const char* rgbNames[]{"rspin","gspin","bspin"};
    const char* lslNames[]{"rspin_lsl","gspin_lsl","bspin_lsl"};
    const char* hslNames[]{"hspin","sspin","lspin"};
    for (std::size_t channel=0; channel<3 && valid; ++channel)
        valid=assign(rgbNames[channel],LLSD(state.rgb[channel]*255.f)) && assign(lslNames[channel],LLSD(state.rgb[channel])) &&
            assign(hslNames[channel],LLSD(state.hsl[channel]*(channel==0 ? 360.f : 100.f)));
    std::string hex(6,'0');
    constexpr char digits[]="0123456789abcdef";
    for (std::size_t channel=0; channel<3; ++channel)
    {
        const auto component=static_cast<unsigned>(state.rgb[channel]*255.f);
        hex[channel*2]=digits[(component>>4)&15]; hex[channel*2+1]=digits[component&15];
    }
    if (valid) valid=assign("hex_value",LLSD(hex));
    if (get(picker) && get(picker)->colorPicker) mNodes.at(picker).colorPicker->synchronizing=false;
    return valid;
}

bool LLVKWidgetTree::setColorPickerRgb(Id picker,const LLVKColor::Value& input,bool preview,std::string& error)
{
    error.clear();
    if (!get(picker) || !get(picker)->colorPicker) return false;
    auto color=input;
    for (auto& channel : color)
    {
        if (!std::isfinite(channel)) { error="Nonfinite native picker color"; return false; }
        channel=std::clamp(channel,0.f,1.f);
    }
    auto& state=*mNodes.at(picker).colorPicker;
    state.rgb=color;
    LLColor3(color[0],color[1],color[2]).calcHSL(&state.hsl[0],&state.hsl[1],&state.hsl[2]);
    const auto swatch=state.swatch;
    const bool immediate=state.immediate;
    if (!syncColorPickerFields(picker,error)) return false;
    return !preview || !immediate || applyColorSelection(swatch,color,ColorPickOperation::Change,error);
}

bool LLVKWidgetTree::commitColorPickerField(Id picker,Id field,std::string& error)
{
    error.clear();
    if (!get(picker) || !get(picker)->colorPicker || !get(field)) return false;
    const auto state=*get(picker)->colorPicker;
    if (state.synchronizing) return true;
    const auto name=get(field)->params.name;
    if (!state.fields.contains(name) || state.fields.at(name)!=field) return false;
    auto rgb=state.rgb;
    if (name=="hex_value")
    {
        const auto hex=value(field).asString();
        if (hex.size()!=6) return true;
        for (std::size_t channel=0; channel<3; ++channel)
        {
            unsigned component=0;
            const auto begin=hex.data()+2*channel;
            const auto parsed=std::from_chars(begin,begin+2,component,16);
            if (parsed.ec!=std::errc() || parsed.ptr!=begin+2) return true;
            rgb[channel]=component/255.f;
        }
        return setColorPickerRgb(picker,rgb,true,error);
    }
    if (name=="hspin" || name=="sspin" || name=="lspin")
    {
        auto hsl=state.hsl;
        const auto channel=name=="hspin" ? 0 : name=="sspin" ? 1 : 2;
        hsl[channel]=static_cast<float>(value(field).asReal())/(channel==0 ? 360.f : 100.f);
        if (!std::isfinite(hsl[channel]) || hsl[channel]<0.f || hsl[channel]>1.f)
        { error="Native picker HSL input is outside its range"; return false; }
        LLColor3 color; color.setHSL(hsl[0],hsl[1],hsl[2]);
        for (std::size_t index=0; index<3; ++index) rgb[index]=color.mV[index];
        mNodes.at(picker).colorPicker->rgb=rgb;
        mNodes.at(picker).colorPicker->hsl=hsl;
        if (!syncColorPickerFields(picker,error)) return false;
        return !state.immediate || applyColorSelection(state.swatch,rgb,ColorPickOperation::Change,error);
    }
    const auto channel=name=="rspin" || name=="rspin_lsl" ? 0 : name=="gspin" || name=="gspin_lsl" ? 1 :
        name=="bspin" || name=="bspin_lsl" ? 2 : -1;
    if (channel<0) return false;
    rgb[channel]=static_cast<float>(value(field).asReal())/(name.ends_with("_lsl") ? 1.f : 255.f);
    return setColorPickerRgb(picker,rgb,true,error);
}
#include "llvkwidgettree.h"

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createTextureControl(const Params& view,
    const LLVKControl::Params& control,const LLVKTextureCtrl::Params& params,Id parent,std::string& error)
{
    error.clear();
    const auto width=view.rect.right-view.rect.left,height=view.rect.top-view.rect.bottom;
    if (width<2 || params.captionHeight<0 || height<params.captionHeight+2 || params.labelWidth<-1)
    { error="Invalid native texture control geometry"; return {}; }
    auto initial=control;
    initial.init={}; initial.initialValue.reset(); initial.valueSetting.reset();
    const auto id=createControl(view,initial,parent,error);
    if (!id) return {};
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        LLVKTextureCtrl state;
        state.params=std::make_shared<LLVKTextureCtrl::Params>(params);
        mNodes.at(*id).textureControl=std::move(state);
        Params child;
        child.name="texture caption"; child.mouseOpaque=false;
        child.rect={0,0,params.labelWidth<0 ? width : params.labelWidth,params.captionHeight};
        child.follows=Left|Right|Bottom;
        auto captionControl=params.captionControl;
        if (!captionControl.font) captionControl.font=control.font;
        captionControl.initialValue=params.label; captionControl.tabStop=false;
        auto text=params.caption;
        text.textColor=params.enabledText; text.readOnlyColor=params.disabledText;
        const auto caption=createPlainText(child,captionControl,text,*id,error);
        if (!caption) { discard(); return {}; }
        mNodes.at(*id).textureControl->caption=*caption;
        child.name="texture border"; child.rect={0,params.captionHeight,width,height};
        child.follows=Left|Right|Top|Bottom;
        const auto border=createBorder(child,params.border,*id,error);
        if (!border) { discard(); return {}; }
        mNodes.at(*id).textureControl->border=*border;
        const auto lineHeight=static_cast<int>(std::ceil(control.font->metrics().lineHeight));
        const auto middle=(height+params.captionHeight)/2;
        child.name="texture multiple"; child.rect={0,middle-lineHeight/2,width,middle+(lineHeight+1)/2};
        child.mouseOpaque=false; child.visible=false;
        auto multipleControl=params.multipleControl;
        if (!multipleControl.font) multipleControl.font=control.font;
        multipleControl.tabStop=false; multipleControl.initialValue=params.multipleLabel;
        auto multipleText=params.multiple;
        multipleText.layout.alignment=LLVKFont::HorizontalAlign::Center;
        const auto multiple=createPlainText(child,multipleControl,multipleText,*id,error);
        if (!multiple) { discard(); return {}; }
        mNodes.at(*id).textureControl->multiple=*multiple;
        mNodes.at(*id).control->params=control;
        auto value=control.initialValue.value_or(LLSD(params.initialAsset));
        if (control.valueSetting && setting(*control.valueSetting)) value=*setting(*control.valueSetting);
        if (!setTextureValue(*id,value,error)) { discard(); return {}; }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native texture control was removed during initialization"; return {}; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::setTextureValue(Id id,const LLSD& value,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->textureControl || (!value.isUUID() && !LLUUID::validate(value.asString())))
    { error="Native texture control requires an asset UUID"; return false; }
    const LLUUID asset=value.isUUID() ? value.asUUID() : LLUUID(value.asString());
    auto& state=*mNodes.at(id).textureControl;
    if (asset!=state.current.asset)
    {
        state.current={asset,{},{}};
        state.pending=state.current;
        state.preview.reset();
        ++state.generation;
    }
    mNodes.at(id).control->value=LLSD(asset.asString());
    return true;
}

bool LLVKWidgetTree::beginTextureSelection(Id id,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->textureControl || !get(id)->textureControl->valid || !enabledInChain(id)) return false;
    auto& state=*mNodes.at(id).textureControl;
    state.original=state.pending=state.current;
    state.picking=true;
    return true;
}

bool LLVKWidgetTree::applyTextureSelection(Id id,const LLVKTextureCtrl::Selection& selection,
    LLVKTextureCtrl::Operation operation,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->textureControl || !node->textureControl->picking || !enabledInChain(id)) return false;
    using Operation=LLVKTextureCtrl::Operation;
    if (operation!=Operation::Preview && operation!=Operation::Select && operation!=Operation::Cancel)
    { error="Invalid native texture selection operation"; return false; }
    const auto params=node->textureControl->params;
    const auto selected=operation==Operation::Cancel ? node->textureControl->original : selection;
    if (operation!=Operation::Cancel && selected.asset.isNull() && !params->allowNone)
    { error="This texture control does not allow an empty selection"; return false; }
    auto& state=*mNodes.at(id).textureControl;
    state.pending=selected;
    if (operation==Operation::Preview && !params->applyImmediately) return true;
    if (!setTextureValue(id,LLSD(selected.asset),error)) return false;
    mNodes.at(id).textureControl->current=selected;
    if (operation!=Operation::Preview) mNodes.at(id).textureControl->picking=false;
    const bool commit=params->commitOnSelection || operation==Operation::Select;
    if (operation==Operation::Cancel) mNodes.at(id).control->dirty=false;
    else if (commit) mNodes.at(id).control->dirty=true;
    setTentative(id,false);
    const auto callback=operation==Operation::Select ? params->selected : operation==Operation::Cancel ? params->cancelled : LLVKControl::Callback{};
    if (callback.function) callback.function(id,callback.parameter.value_or(LLSD()));
    else if (commit)
    {
        const auto generation=get(id)->textureControl->generation;
        writeBoundValue(id,LLSD(selected.asset.asString()));
        if (get(id) && get(id)->textureControl && get(id)->textureControl->generation==generation)
            dispatchControl(id,&LLVKControl::Params::commit);
    }
    return error.empty();
}

bool LLVKWidgetTree::publishTexturePreview(Id id,const LLUUID& asset,std::uint64_t generation,
    std::shared_ptr<const LLVKWidgetImage> image)
{
    if (!get(id) || !get(id)->textureControl) return false;
    auto& state=*mNodes.at(id).textureControl;
    if (state.current.asset!=asset || state.generation!=generation) return false;
    state.previewHasAlpha=false;
    if (image)
    {
        const auto pixels=image->bottomUpRgba();
        for (std::size_t offset=3; offset<pixels.size(); offset+=4)
            if (pixels[offset]!=255) { state.previewHasAlpha=true; break; }
    }
    state.preview=std::move(image);
    return true;
}

bool LLVKWidgetTree::refreshTextureControl(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->textureControl) return false;
    const auto state=*node->textureControl;
    if (get(state.caption)) setEnabled(state.caption,enabledInChain(id));
    if (get(state.multiple)) setVisible(state.multiple,tentative(id));
    if (get(state.border) && get(state.border)->border) mNodes.at(state.border).border->keyboardFocus=mKeyboardFocus==id;
    return true;
}

bool LLVKWidgetTree::textureControlPointer(Id id,const PointerEvent& event,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->textureControl) return false;
    const auto params=node->textureControl->params;
    const auto width=node->params.rect.right-node->params.rect.left,height=node->params.rect.top-node->params.rect.bottom;
    const bool inside=event.x>=0 && event.x<width && event.y>=params->captionHeight && event.y<height;
    if (event.kind==PointerKind::Hover) { cursorEffect(id,inside); return true; }
    if (event.kind!=PointerKind::LeftDown || !inside) return basePointer(id,event,error);
    if (!params->showPicker) { error="Native texture picker service is not bound"; return false; }
    if (!beginTextureSelection(id,error)) return false;
    params->showPicker(id,false);
    return true;
}
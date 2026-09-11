#include "llvkwidgettree.h"
#include <cmath>
#include <charconv>
#include "llstring.h"

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createSliderControl(const Params& view,const LLVKControl::Params& control,
    const SliderControlParams& params,Id parent,std::string& error)
{
    error.clear();
    if (params.precision<0 || params.precision>10 || params.spacing<0 || !control.font)
    { error="Invalid native slider control parameters"; return std::nullopt; }
    auto initial=control; initial.init={};
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    mNodes.at(*id).sliderControl=SliderControl{std::make_shared<SliderControlParams>(params)};
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        const auto width=view.rect.right-view.rect.left, height=view.rect.top-view.rect.bottom;
        const auto measure=[&](const std::string& label) -> std::optional<int>
        {
            const auto wide=utf8str_to_wstring(label);
            const std::u32string text(wide.begin(),wide.end());
            const auto measured=control.font->measureRun(text,0,text.size(),1.f,true,false,error);
            if (!measured || measured->width>INT32_MAX) return std::nullopt;
            return static_cast<int>(std::floor(measured->width+0.5f));
        };
        int labelWidth=params.labelWidth.value_or(0), textWidth=params.textWidth.value_or(0);
        if (!params.label.empty() && !params.labelWidth)
        { const auto measured=measure(params.label); if (!measured) { discard(); return std::nullopt; } labelWidth=*measured; }
        if (params.showText && !params.textWidth)
        {
            const auto zero=measure("0"), dot=measure("."), minus=measure("-");
            if (!zero || !dot || !minus) { discard(); return std::nullopt; }
            const auto digits=params.bar.maximum>0.f ? static_cast<int>(std::log10(params.bar.maximum))+params.precision+1 : 0;
            textWidth=std::max(0,digits)*(*zero)+(params.bar.increment<1.f ? *dot : 0)+
                (params.bar.minimum<0.f || params.bar.maximum<0.f ? *minus : 0)+8;
        }
        const auto left=labelWidth ? labelWidth+params.spacing : 0;
        const auto right=params.showText ? width-textWidth-params.spacing : width;
        if (labelWidth<0 || textWidth<0 || right<=left || width<0 || height<=0)
        { error="Native slider control has no bar area"; discard(); return std::nullopt; }
        Params childView; childView.follows=Left|Top;
        LLVKControl::Params childControl; childControl.font=control.font;
        LLVKPlainControl::Params text; text.textColor=text.readOnlyColor=params.textColor;
        if (!params.label.empty())
        {
            childView.name="slider label"; childView.rect={0,0,labelWidth,height}; childView.mouseOpaque=false;
            childControl.initialValue=params.label; childControl.tabStop=false;
            const auto label=createPlainText(childView,childControl,text,*id,error);
            if (!label) { discard(); return std::nullopt; }
            mNodes.at(*id).sliderControl->label=*label;
        }
        childView.name="slider_bar"; childView.rect={left,0,right,height}; childView.mouseOpaque=true;
        childView.follows=Left|Right|Top;
        childControl=control; childControl.init={}; childControl.validate={};
        childControl.initialValue=value(*id);
        childControl.commit.function=[this,owner=*id](Id,const LLSD&) { std::string problem; commitSliderControl(owner,false,problem); };
        const auto bar=createSlider(childView,childControl,params.bar,*id,error);
        if (!bar) { discard(); return std::nullopt; }
        mNodes.at(*id).sliderControl->bar=*bar;
        mNodes.at(*id).control->value=value(*bar);
        if (params.showText)
        {
            childView.rect={width-textWidth,0,width,height}; childView.follows=Right|Top;
            if (params.editable)
            {
                childView.name="slider editor";
                childControl=params.editorControl;
                if (!childControl.font) childControl.font=control.font;
                childControl.commit.function=[this,owner=*id](Id,const LLSD&) { std::string problem; commitSliderControl(owner,true,problem); };
                const auto editor=createLineEditor(childView,childControl,params.editor,*id,error);
                if (!editor) { discard(); return std::nullopt; }
                mNodes.at(*id).sliderControl->editor=*editor;
            }
            else
            {
                childView.name="slider text"; childView.mouseOpaque=false;
                childControl={}; childControl.font=control.font; childControl.tabStop=false;
                const auto label=createPlainText(childView,childControl,text,*id,error);
                if (!label) { discard(); return std::nullopt; }
                mNodes.at(*id).sliderControl->text=*label;
            }
        }
        if (!updateSliderControlText(*id,error)) { discard(); return std::nullopt; }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native slider control removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::updateSliderControlText(Id id,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->sliderControl) return false;
    const auto state=*node->sliderControl;
    if (!state.editor && !state.text) return true;
    const auto power=std::pow(10.,state.params->precision);
    const float number=static_cast<float>(std::floor(value(id).asReal()*power+0.5)/power);
    char buffer[128];
    const auto formatted=std::to_chars(buffer,buffer+sizeof(buffer),number,std::chars_format::fixed,state.params->precision);
    if (formatted.ec!=std::errc()) { error="Native slider text formatting failed"; return false; }
    const std::string text(buffer,formatted.ptr);
    if (state.editor) return setValue(state.editor,LLSD("")) && setValue(state.editor,LLSD(text));
    return setPlainText(state.text,text,error);
}

bool LLVKWidgetTree::setSliderControlValue(Id id,const LLSD& value,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->sliderControl) return false;
    const auto bar=node->sliderControl->bar;
    if (!setSliderValue(bar,static_cast<float>(value.asReal()),false,false,error)) return false;
    if (!get(id)) return true;
    mNodes.at(id).control->value=this->value(bar);
    ++mNodes.at(id).sliderControl->generation;
    return updateSliderControlText(id,error);
}

bool LLVKWidgetTree::commitSliderControl(Id id,bool fromEditor,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->sliderControl) return false;
    const auto state=*node->sliderControl;
    const auto old=value(id);
    float proposed=static_cast<float>(value(state.bar).asReal());
    if (fromEditor)
    {
        const auto text=value(state.editor).asString();
        const auto parsed=std::from_chars(text.data(),text.data()+text.size(),proposed);
        if (parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(proposed) ||
            proposed<state.params->bar.minimum || proposed>state.params->bar.maximum)
        { updateSliderControlText(id,error); if (error.empty()) error="Invalid native slider editor value"; return false; }
        if (!setSliderControlValue(id,LLSD(proposed),error)) return false;
    }
    else mNodes.at(id).control->value=LLSD(proposed);
    const auto validate=node->control->params.validate;
    const auto commit=node->control->params.commit;
    const bool accepted=!validate.function || validate.function(id,validate.parameter.value_or(LLSD(proposed)));
    if (!get(id)) return false;
    if (!accepted) { setSliderControlValue(id,old,error); writeBoundValue(id,old); return false; }
    writeBoundValue(id,value(id));
    if (!get(id)) return true;
    if (commit.function) commit.function(id,commit.parameter.value_or(value(id)));
    if (get(id) && fromEditor && state.params->editorCommit.function)
        state.params->editorCommit.function(id,state.params->editorCommit.parameter.value_or(value(id)));
    return !get(id) || updateSliderControlText(id,error);
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createSlider(const Params& view,const LLVKControl::Params& control,
    const SliderParams& params,Id parent,std::string& error)
{
    error.clear();
    if (!std::isfinite(params.minimum) || !std::isfinite(params.maximum) || !std::isfinite(params.increment) ||
        !std::isfinite(params.initial) || params.maximum<params.minimum || params.increment<=0.f)
    { error="Invalid native slider range or increment"; return std::nullopt; }
    auto initial=control;
    initial.init={};
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    mNodes.at(*id).slider=Slider{std::make_shared<SliderParams>(params)};
    auto value=control.initialValue.value_or(LLSD(params.initial));
    if (control.valueSetting && mSettings.contains(*control.valueSetting)) value=mSettings.at(*control.valueSetting);
    if (!setSliderValue(*id,static_cast<float>(value.asReal()),true,false,error))
    { std::string ignored; erase(*id,ignored); return std::nullopt; }
    if (!get(*id)) return std::nullopt;
    mNodes.at(*id).slider->dragStart=get(*id)->slider->thumb;
    if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
    if (!get(*id)) { error="Native slider removed during initialization"; return std::nullopt; }
    return id;
}

bool LLVKWidgetTree::updateSliderThumb(Id id,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->slider) return false;
    const auto params=node->slider->params;
    const auto width=std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height=std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    const auto thumbWidth=params->thumb ? params->thumb->width() : 16u;
    const auto thumbHeight=params->thumb ? params->thumb->height() : 16u;
    if (width<0 || height<0 || width>INT32_MAX || height>INT32_MAX || thumbWidth>INT32_MAX || thumbHeight>INT32_MAX)
    { error="Native slider geometry overflows"; return false; }
    const auto range=params->maximum-params->minimum;
    const float fraction=range!=0.f ? (static_cast<float>(value(id).asReal())-params->minimum)/range : 0.f;
    const auto travel=params->vertical ? height-2*(thumbHeight/2) : width-2*(thumbWidth/2);
    if (travel<=0) { error="Native slider is too small for its thumb"; return false; }
    const auto center=static_cast<std::int64_t>(fraction*travel)+(params->vertical ? thumbHeight/2 : thumbWidth/2);
    const auto left=params->vertical ? width/2-thumbWidth/2 : center-thumbWidth/2;
    const auto bottom=params->vertical ? center-thumbHeight/2 : height/2-thumbHeight/2;
    mNodes.at(id).slider->thumb={static_cast<int>(left),static_cast<int>(bottom),static_cast<int>(left+thumbWidth),static_cast<int>(bottom+thumbHeight)};
    return true;
}

bool LLVKWidgetTree::setSliderValue(Id id,float number,bool publish,bool commit,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->slider || !std::isfinite(number)) return false;
    const auto params=node->slider->params;
    const auto old=value(id).asReal();
    number=std::clamp(number,params->minimum,params->maximum);
    number-=params->minimum;
    number+=params->increment/2.0001f;
    number-=std::fmod(number,params->increment);
    number+=params->minimum;
    if (!std::isfinite(number)) { error="Native slider quantization overflow"; return false; }
    mNodes.at(id).control->value=LLSD(number);
    if (!updateSliderThumb(id,error)) { if (get(id)) mNodes.at(id).control->value=LLSD(old); return false; }
    if (publish && old!=number) writeBoundValue(id,LLSD(number));
    if (commit && old!=number && get(id)) return dispatchControl(id,&LLVKControl::Params::commit);
    return true;
}

bool LLVKWidgetTree::sliderStep(Id id,std::int32_t steps,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->slider || !enabledInChain(id)) return false;
    return setSliderValue(id,static_cast<float>(value(id).asReal())+steps*node->slider->params->increment,true,true,error);
}

bool LLVKWidgetTree::sliderPointer(Id id,const PointerEvent& event,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->slider || !enabledInChain(id)) return false;
    if (!updateSliderThumb(id,error)) return false;
    node=get(id);
    const auto params=node->slider->params;
    if (event.kind==PointerKind::LeftDown)
    {
        if (!node->control->params.chrome && !requestControlFocus(id,true,error)) return false;
        if (!get(id)) return true;
        if (params->mouseDown.function) params->mouseDown.function(id,params->mouseDown.parameter.value_or(value(id)));
        if (!get(id)) return true;
        if (mInputModifiers.control) return setSliderValue(id,params->initial,true,true,error);
        auto& state=*mNodes.at(id).slider;
        const auto& thumb=state.thumb;
        const bool inside=event.x>=thumb.left && event.x<thumb.right && event.y>=thumb.bottom && event.y<thumb.top;
        state.mouseOffset=inside ? (params->vertical ? (thumb.top+thumb.bottom)/2-event.y : (thumb.left+thumb.right)/2-event.x) : 0;
        state.dragStart=thumb;
        return setMouseCapture(id,error);
    }
    if (event.kind==PointerKind::LeftUp)
    {
        if (mMouseCapture==id)
        {
            if (!setMouseCapture(0,error)) return false;
            if (get(id) && params->mouseUp.function) params->mouseUp.function(id,params->mouseUp.parameter.value_or(value(id)));
        }
        return true;
    }
    if (event.kind==PointerKind::Hover)
    {
        if (mMouseCapture==id)
        {
            const auto half=params->vertical ? (node->slider->thumb.top-node->slider->thumb.bottom)/2 : (node->slider->thumb.right-node->slider->thumb.left)/2;
            const auto extent=params->vertical ? node->params.rect.top-node->params.rect.bottom : node->params.rect.right-node->params.rect.left;
            const auto coordinate=std::clamp(std::int64_t(params->vertical ? event.y : event.x)+node->slider->mouseOffset,std::int64_t(half),std::int64_t(extent-half));
            const float fraction=float(coordinate-half)/float(extent-2*half);
            if (!setSliderValue(id,params->minimum+fraction*(params->maximum-params->minimum),true,true,error)) return false;
        }
        cursorEffect(id,false);
        return true;
    }
    return false;
}
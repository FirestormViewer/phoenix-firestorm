#include "llvkwidgettree.h"
#include "llcalc.h"
#include <charconv>
#include <cmath>

namespace
{
    float precisionValue(float value,int precision)
    {
        double scaled = value;
        for (int index=0; index<precision; ++index) scaled*=10.;
        scaled=std::floor(scaled+0.5);
        for (int index=0; index<precision; ++index) scaled/=10.;
        return static_cast<float>(scaled);
    }
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createSpinner(const Params& view,const LLVKControl::Params& control,
    const SpinnerParams& params,Id parent,std::string& error)
{
    error.clear();
    if (!std::isfinite(params.minimum) || !std::isfinite(params.maximum) || !std::isfinite(params.increment) ||
        params.maximum<params.minimum || params.increment<0 || params.precision<0 || params.precision>10 ||
        params.buttonWidth<=0 || params.buttonHeight<=0 || params.spacing<0)
    { error="Invalid native spinner range or geometry"; return std::nullopt; }
    Spinner state;
    state.params=std::make_shared<SpinnerParams>(params);
    auto ownerView=view;
    ownerView.useBoundingRect=true;
    return createControlImpl(ownerView,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::nullopt,std::nullopt,{},std::nullopt,std::nullopt,std::move(state));
}

bool LLVKWidgetTree::constructSpinnerChildren(Id id,std::string& error)
{
    const auto* owner=get(id);
    const auto params=owner->spinner->params;
    const auto width=owner->params.rect.right-owner->params.rect.left, height=owner->params.rect.top-owner->params.rect.bottom;
    const auto buttonHeight=params->dynamicHeight ? height/2 : params->buttonHeight;
    if (buttonHeight<=0 || buttonHeight>INT32_MAX/2) { error="Native spinner button height is invalid"; return false; }
    int left=0;
    Params view;
    LLVKControl::Params labelControl;
    labelControl.font=owner->control->params.font;
    labelControl.tabStop=false;
    if (!params->label.empty())
    {
        const auto labelWidth=std::clamp(params->labelWidth,0,std::max(0,width-40));
        view.name="SpinCtrl Label"; view.rect={0,height-2*buttonHeight,labelWidth,height}; view.mouseOpaque=false;
        labelControl.initialValue=params->label;
        LLVKPlainControl::Params labelText;
        labelText.layout.wrap=params->labelWrap;
        labelText.textColor=labelText.readOnlyColor=params->textEnabledColor;
        const auto label=createPlainText(view,labelControl,labelText,id,error);
        if (!label) return false;
        mNodes.at(id).spinner->label=*label;
        left=labelWidth+params->spacing;
    }
    if (std::int64_t(left)+params->buttonWidth+1>=width) { error="Native spinner has no editor width"; return false; }
    for (bool increase : {true,false})
    {
        auto button=increase ? params->upButton : params->downButton;
        LLVKControl::Callback action;
        action.function=[this,id,increase](Id,const LLSD&) { std::string problem; stepSpinner(id,increase,mInputModifiers,problem); };
        button.mouseUp=button.held=action;
        button.commitOnCaptureLost=true;
        auto control=params->buttonControl;
        control.tabStop=false;
        view.name=increase ? "SpinCtrl Up" : "SpinCtrl Down";
        view.mouseOpaque=true;
        view.follows=Left|Bottom;
        view.rect={left,height-(increase ? 1 : 2)*buttonHeight,left+params->buttonWidth,height-(increase ? 0 : 1)*buttonHeight};
        const auto child=createButton(view,control,button,id,error);
        if (!child) return false;
        (increase ? mNodes.at(id).spinner->up : mNodes.at(id).spinner->down)=*child;
    }
    view.name="SpinCtrl Editor";
    view.rect={left+params->buttonWidth+1,height-2*buttonHeight,width,height};
    view.follows=Left|Bottom;
    auto editor=params->editor;
    editor.text.maximumBytes=255;
    if (params->digitsOnly)
        editor.inputPrevalidator=[](std::u32string_view text)
        { return std::all_of(text.begin(),text.end(),[](char32_t value) { return value>=U'0' && value<=U'9'; }); };
    auto editorControl=params->editorControl;
    editorControl.commit.function=[this,id](Id,const LLSD&) { std::string problem; commitSpinner(id,problem); };
    const auto child=createLineEditor(view,editorControl,editor,id,error);
    if (!child) return false;
    mNodes.at(id).spinner->editor=*child;
    Events events;
    events.focusLost=[this,id](Id)
    { std::string problem; if (get(id)) refreshSpinnerEditor(id,problem); };
    setEvents(*child,std::move(events));
    return refreshSpinnerEditor(id,error);
}

bool LLVKWidgetTree::refreshSpinnerEditor(Id id,std::string& error)
{
    const auto* owner=get(id);
    if (!owner || !owner->spinner || !get(owner->spinner->editor)) return false;
    const auto rounded=precisionValue(static_cast<float>(value(id).asReal()),owner->spinner->params->precision);
    char buffer[128];
    const auto formatted=std::to_chars(buffer,buffer+sizeof(buffer),rounded,std::chars_format::fixed,owner->spinner->params->precision);
    if (formatted.ec!=std::errc()) { error="Native spinner formatting failed"; return false; }
    return setValue(owner->spinner->editor,LLSD(std::string(buffer,formatted.ptr)));
}

bool LLVKWidgetTree::setSpinnerRange(Id id,float minimum,float maximum,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->spinner || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum>maximum)
    { error="Invalid native spinner range"; return false; }
    auto params=std::make_shared<SpinnerParams>(*get(id)->spinner->params);
    params->minimum=minimum; params->maximum=maximum;
    mNodes.at(id).spinner->params=std::move(params);
    return setSpinnerValue(id,value(id),true,error);
}

bool LLVKWidgetTree::setSpinnerFormat(Id id,const std::string& label,int precision,float increment,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->spinner || precision<0 || precision>9 || !std::isfinite(increment) || increment<=0)
    { error="Invalid native spinner format"; return false; }
    auto params=std::make_shared<SpinnerParams>(*get(id)->spinner->params);
    params->label=label; params->precision=precision; params->increment=increment;
    mNodes.at(id).spinner->params=std::move(params);
    if (const auto text=get(id)->spinner->label) setValue(text,LLSD(label));
    return refreshSpinnerEditor(id,error);
}

bool LLVKWidgetTree::setSpinnerValue(Id id,const LLSD& value,bool forceEditor,std::string& error)
{
    error.clear();
    const auto* owner=get(id);
    const auto number=static_cast<float>(value.asReal());
    if (!owner || !owner->spinner || !std::isfinite(number)) { error="Invalid native spinner value"; return false; }
    mNodes.at(id).control->value=LLSD(number);
    ++mNodes.at(id).spinner->generation;
    return (!forceEditor && mKeyboardFocus==owner->spinner->editor) || refreshSpinnerEditor(id,error);
}

bool LLVKWidgetTree::publishSpinnerValue(Id id,float proposed,std::string& error)
{
    const auto* owner=get(id);
    if (!owner || !owner->spinner || !std::isfinite(proposed)) return false;
    const auto params=owner->spinner->params;
    const auto old=value(id);
    const auto number=std::clamp(proposed,params->minimum,params->maximum);
    if (!setSpinnerValue(id,LLSD(number),false,error)) return false;
    const auto generation=get(id)->spinner->generation;
    const auto validation=get(id)->control->params.validate;
    const bool accepted=!validation.function || validation.function(id,validation.parameter.value_or(LLSD(number)));
    if (!get(id) || !get(id)->spinner || get(id)->spinner->generation!=generation) return false;
    if (!accepted) { setSpinnerValue(id,old,true,error); return false; }
    const auto commit=get(id)->control->params.commit;
    if (!refreshSpinnerEditor(id,error)) return false;
    writeBoundValue(id,LLSD(number));
    if (!get(id)) return true;
    if (commit.function) commit.function(id,commit.parameter.value_or(LLSD(number)));
    return true;
}

bool LLVKWidgetTree::commitSpinner(Id id,std::string& error)
{
    error.clear();
    const auto* owner=get(id);
    if (!owner || !owner->spinner) return false;
    const auto expression=value(owner->spinner->editor).asString();
    if (expression.size()>255) { error="Native spinner expression exceeds byte limit"; return false; }
    LLCalc calculator;
    float number=0.f;
    if (!calculator.evalString(expression,number) || !std::isfinite(number))
    { refreshSpinnerEditor(id,error); if (error.empty()) error="Invalid native spinner expression"; return false; }
    return publishSpinnerValue(id,number,error);
}

bool LLVKWidgetTree::stepSpinner(Id id,bool increase,LLVKLineEditor::Modifiers modifiers,std::string& error)
{
    error.clear();
    const auto* owner=get(id);
    if (!owner || !owner->spinner || !enabledInChain(id)) return false;
    const auto params=owner->spinner->params;
    const auto text=value(owner->spinner->editor).asString();
    float number=0.f;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),number);
    if (parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(number)) return false;
    const auto factor=modifiers.alt ? 10.f : modifiers.control ? 0.1f : modifiers.shift ? 0.01f : 1.f;
    const auto next=precisionValue(number+(increase ? 1.f : -1.f)*params->increment*factor,params->precision);
    return publishSpinnerValue(id,next,error);
}
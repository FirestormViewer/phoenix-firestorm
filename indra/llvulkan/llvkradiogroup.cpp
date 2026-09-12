#include "llvkwidgettree.h"
#include "llvkwidgetlayout.h"

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createRadioGroup(const Params& view,const LLVKControl::Params& control,
    std::span<const RadioItemParams> items,bool allowDeselect,Id parent,std::string& error)
{
    error.clear();
    if (items.size()>maximumNodes) { error="Native radio item count exceeds budget"; return std::nullopt; }
    auto construction=control;
    construction.init={}; construction.valueSetting.reset(); construction.initialValue.reset();
    const auto id=createControl(view,construction,parent,error);
    if (!id) return std::nullopt;
    mNodes.at(*id).radioGroup.emplace();
    mNodes.at(*id).radioGroup->allowDeselect=allowDeselect;
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        for (const auto& item : items)
        {
            auto childView=item.view;
            if (item.layout)
            {
                const auto rect=item.layout->apply(*this,*id,view.layout,error);
                if (!rect) { discard(); return std::nullopt; }
                childView.rect=*rect;
                childView.layout=item.layout->layout.empty() ? view.layout : item.layout->layout;
            }
            auto childControl=item.control;
            if (!childControl.font) childControl.font=control.font;
            childControl.tabStop=false;
            const auto index=static_cast<std::int32_t>(get(*id)->radioGroup->items.size());
            childControl.commit.function=[this,owner=*id,index](Id,const LLSD&)
            {
                const auto* group=get(owner);
                if (!group || !group->radioGroup) return;
                const auto selected=group->radioGroup->selected==index && group->radioGroup->allowDeselect ? -1 : index;
                std::string problem;
                if (selectRadioIndex(owner,selected,true,problem) && get(owner)) dispatchControl(owner,&LLVKControl::Params::commit);
            };
            const auto child=createCheckBox(childView,childControl,item.check,*id,error);
            if (!child || !get(*id)) { discard(); return std::nullopt; }
            mNodes.at(*id).radioGroup->items.push_back({*child,item.payload.value_or(LLSD(item.view.name))});
            setValue(*child,LLSD(false));
            const auto button=get(*child)->checkBox->button;
            mNodes.at(button).control->params.tabStop=false;
        }
        mNodes.at(*id).control->params=control;
        if (control.valueSetting && !mSettings.contains(*control.valueSetting)) mNodes.at(*id).control->params.valueSetting.reset();
        if (control.valueSetting && mSettings.contains(*control.valueSetting))
        {
            if (!setRadioValue(*id,mSettings.at(*control.valueSetting),error)) { discard(); return std::nullopt; }
        }
        else if (control.initialValue && !setRadioValue(*id,*control.initialValue,error)) { discard(); return std::nullopt; }
        if (!get(*id)) return std::nullopt;
        if (!get(*id)->radioGroup->items.empty() && get(*id)->radioGroup->selected<0)
        {
            const auto first=get(*id)->radioGroup->items.front().control;
            mNodes.at(first).control->params.tabStop=true;
            mNodes.at(get(first)->checkBox->button).control->params.tabStop=true;
        }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native radio group removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::selectRadioIndex(Id id,std::int32_t index,bool publish,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->radioGroup) return false;
    const auto state=*node->radioGroup;
    if (index < -1 || (index>=0 && static_cast<std::size_t>(index)>=state.items.size()) ||
        (index==-1 && state.selected>=0 && !state.allowDeselect)) return false;
    const bool focused=hasAncestor(mKeyboardFocus,id);
    mNodes.at(id).radioGroup->selected=index;
    for (std::size_t position=0; position<state.items.size(); ++position)
    {
        const auto child=state.items[position].control;
        if (!get(child) || get(child)->parent!=id || !get(child)->checkBox) continue;
        const bool selected=index==static_cast<std::int32_t>(position);
        setValue(child,LLSD(selected));
        mNodes.at(child).control->params.tabStop=selected;
        const auto button=get(child)->checkBox->button;
        if (get(button)) mNodes.at(button).control->params.tabStop=selected;
    }
    if (focused && index>=0)
    {
        const auto child=state.items[index].control;
        if (get(child) && get(child)->checkBox && !requestControlFocus(get(child)->checkBox->button,true,error)) return false;
    }
    if (!get(id)) return true;
    if (publish) writeBoundValue(id,value(id));
    return true;
}

bool LLVKWidgetTree::setRadioValue(Id id,const LLSD& value,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->radioGroup) return false;
    for (std::size_t index=0; index<node->radioGroup->items.size(); ++index)
        if (node->radioGroup->items[index].payload.asString()==value.asString())
            return selectRadioIndex(id,static_cast<std::int32_t>(index),true,error);
    return selectRadioIndex(id,value.isInteger() ? value.asInteger() : -1,false,error);
}

bool LLVKWidgetTree::radioKey(Id id,bool forward,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->radioGroup || !enabledInChain(id)) return false;
    const auto index=node->radioGroup->selected+(forward ? 1 : -1);
    if (selectRadioIndex(id,index,true,error) && get(id)) return dispatchControl(id,&LLVKControl::Params::commit);
    return false;
}

bool LLVKWidgetTree::setRadioIndexEnabled(Id id,std::int32_t index,bool enabled,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->radioGroup || index<0 || static_cast<std::size_t>(index)>=node->radioGroup->items.size()) return false;
    const auto items=node->radioGroup->items;
    if (!setEnabled(items[index].control,enabled)) return false;
    if (!get(id)) return true;
    if (get(id)->radioGroup->selected==index && !enabled) selectRadioIndex(id,-1,true,error);
    if (!get(id) || !error.empty()) return error.empty();
    if (get(id)->radioGroup->selected<0)
    {
        for (std::size_t current=0; current<items.size(); ++current)
        {
            if (!get(id)) return true;
            if (current>=static_cast<std::size_t>(index) && get(id)->radioGroup->selected>=0) break;
            if (get(items[current].control) && get(items[current].control)->params.enabled)
                selectRadioIndex(id,static_cast<std::int32_t>(current),true,error);
            if (!error.empty()) return false;
        }
        if (get(id) && get(id)->radioGroup->selected<0) selectRadioIndex(id,0,true,error);
    }
    return error.empty();
}
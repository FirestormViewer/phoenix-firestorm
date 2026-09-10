#include "llvkwidgettree.h"

#include <algorithm>
#include <cmath>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createControl(const Params& inputView,
    const LLVKControl::Params& inputControl, Id parent, std::string& error)
{
    return createControlImpl(inputView,inputControl,std::nullopt,parent,error);
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createControlImpl(const Params& inputView,
    const LLVKControl::Params& inputControl, std::optional<LLVKIcon> icon, Id parent, std::string& error,
    std::optional<LLVKButton> button, std::optional<LLVKBadge> badge,
    std::optional<BadgeConstruction> badgeConstruction, std::optional<LLVKPlainControl> plainText,
    std::optional<CheckBox> checkBox, std::optional<LLVKPanel> panel)
{
    error.clear();
    const auto view = inputView;
    const auto control = inputControl;
    if (!control.font || (parent && !get(parent)))
    { error = "Native control requires a resolved native font and valid parent"; return std::nullopt; }
    const auto id = create(view,0,error);
    if (!id) return std::nullopt;
    try
    {
        LLVKControl state;
        state.params = control;
        mNodes.at(*id).icon = std::move(icon);
        mNodes.at(*id).button = std::move(button);
        mNodes.at(*id).badge = std::move(badge);
        mNodes.at(*id).badgeConstruction = std::move(badgeConstruction);
        mNodes.at(*id).plainText = std::move(plainText);
        mNodes.at(*id).checkBox = std::move(checkBox);
        mNodes.at(*id).panel = std::move(panel);
        if (const auto& installed = mNodes.at(*id).icon; installed && installed->image)
        {
            state.value = installed->image->name();
            state.dirty = true;
        }
        state.params.mouseEnter = {};
        state.params.mouseLeave = {};
        const auto keepExisting = [&](std::optional<std::string>& name)
        {
            if (name && !mSettings.contains(*name)) name.reset();
        };
        keepExisting(state.params.valueSetting);
        keepExisting(state.params.enabledSetting);
        keepExisting(state.params.visibleSetting);
        keepExisting(state.params.invisibleSetting);
        mNodes.at(*id).control = std::move(state);
        if (const auto& current = mNodes.at(*id).panel; current)
        {
            mNodes.at(*id).acceptsBadge = current->params.acceptsBadge;
            if (current->params.hasBorder && !addPanelBorder(*id,current->params.border,error))
            {
                std::string cleanup;
                erase(*id,cleanup);
                return std::nullopt;
            }
        }
        if (mNodes.at(*id).checkBox && !constructCheckBoxChildren(*id,error))
        {
            std::string cleanup;
            erase(*id,cleanup);
            return std::nullopt;
        }
        if (mNodes.at(*id).checkBox && control.valueSetting)
        {
            mNodes.at(*id).control->params.valueSetting.reset();
            bindValueSetting(*id,*control.valueSetting);
        }
        if (mNodes.at(*id).plainText)
        {
            Params document;
            document.name = "text_contents";
            document.rect = {0,0,500,500};
            document.mouseOpaque = false;
            const auto created = create(document,*id,error);
            if (!created)
            {
                std::string cleanup;
                erase(*id,cleanup);
                return std::nullopt;
            }
            mNodes.at(*id).plainText->document = *created;
            if (!setPlainText(*id,"",error) || !reflowPlainText(*id,error))
            {
                std::string cleanup;
                erase(*id,cleanup);
                return std::nullopt;
            }
        }
        const auto construction = mNodes.at(*id).badgeConstruction;
        if (mNodes.at(*id).button && construction && construction->provided &&
            !construction->provided->equals(construction->defaults))
        {
            const auto created = createBadge(construction->view,construction->control,*construction->provided,*id,0,error);
            if (!created || !get(*id) || !postBuildControl(*created) || !attachBadge(*created,*id,error))
            {
                std::string cleanup;
                if (created) erase(*created,cleanup);
                erase(*id,cleanup);
                if (error.empty()) error = "Native button badge construction failed";
                return std::nullopt;
            }
            mNodes.at(*id).button->badge = *created;
        }
        const auto valueOwner = mNodes.at(*id).checkBox ? mNodes.at(*id).checkBox->button : *id;
        const auto setting = mNodes.at(valueOwner).control->params.valueSetting;
        const bool valueAccepted = setting ? setValue(*id,mSettings.at(*setting)) :
            control.initialValue && !control.valueSetting ? setValue(*id,*control.initialValue) : true;
        if (!valueAccepted)
        {
            std::string cleanup;
            erase(*id,cleanup);
            error = "Native control rejected its initial value";
            return std::nullopt;
        }
        applyControlSettings(*id);
        const auto panelParams = mNodes.at(*id).panel ? std::optional(mNodes.at(*id).panel->params) : std::nullopt;
        if (control.init.function)
        {
            const auto callback = control.init.function;
            const LLSD parameter = control.init.parameter.value_or(LLSD());
            callback(*id,parameter);
        }
        if (!get(*id)) { error = "Native init callback destroyed its control"; return std::nullopt; }
        mNodes.at(*id).control->params.mouseEnter = control.mouseEnter;
        mNodes.at(*id).control->params.mouseLeave = control.mouseLeave;
        if (auto& text = mNodes.at(*id).plainText; text)
        {
            mNodes.at(*id).control->dirty = false;
            text->readOnly = text->params.readOnly.value_or(!mNodes.at(*id).params.enabled);
        }
        if (panelParams)
        {
            auto& current = *mNodes.at(*id).panel;
            current.params = *panelParams;
            current.visible = panelParams->visible;
            current.label.assign(panelParams->label);
            for (const auto& [name,value] : panelParams->strings) current.strings[name].assign(value);
            mNodes.at(*id).acceptsBadge = panelParams->acceptsBadge;
            if (panelParams->hasBorder && !addPanelBorder(*id,panelParams->border,error))
            {
                std::string cleanup;
                erase(*id,cleanup);
                return std::nullopt;
            }
        }
        if (parent && !reparent(*id,parent,false,view.tabGroup.value_or(INT32_MAX),error))
        {
            std::string cleanupError;
            erase(*id,cleanupError);
            return std::nullopt;
        }
    }
    catch (...)
    {
        if (get(*id)) { std::string cleanupError; erase(*id,cleanupError); }
        throw;
    }
    return id;
}

void LLVKWidgetTree::applyControlSettings(Id id)
{
    auto& node = mNodes.at(id);
    const auto& params = node.control->params;
    if (params.enabledSetting)
    {
        const auto& value = mSettings.at(*params.enabledSetting);
        const bool enabled = value.asString() == "0" ? false : value.asBoolean();
        setEnabled(id,params.invertEnabled ? !enabled : enabled);
    }
    if (params.visibleSetting || params.invisibleSetting)
    {
        const bool visible = !params.visibleSetting || mSettings.at(*params.visibleSetting).asBoolean();
        const bool invisible = params.invisibleSetting && mSettings.at(*params.invisibleSetting).asBoolean();
        setVisible(id,visible && !invisible);
    }
}

bool LLVKWidgetTree::defineSetting(const std::string& name, const LLSD& value, SettingType type)
{
    if (name.empty() || mSettings.contains(name)) return false;
    if (type != SettingType::Opaque && type != SettingType::Boolean && type != SettingType::Integer &&
        type != SettingType::Real && type != SettingType::String) return false;
    auto inserted = mSettings.emplace(name,value);
    try { mSettingTypes.emplace(name,type); }
    catch (...) { mSettings.erase(inserted.first); throw; }
    return true;
}

bool LLVKWidgetTree::updateSetting(const std::string& name, const LLSD& inputValue)
{
    const auto found = mSettings.find(name);
    if (found == mSettings.end()) return false;
    LLSD value = inputValue;
    if (name == "FlashPeriod" && !std::isfinite(static_cast<float>(value.asReal()))) return false;
    const auto type = mSettingTypes.at(name);
    if (type == SettingType::Boolean && value.isString())
    {
        auto text = value.asString();
        const auto first = text.find_first_not_of(" \t\r\n\v\f");
        text = first == std::string::npos ? std::string() :
               text.substr(first,text.find_last_not_of(" \t\r\n\v\f")-first+1);
        value = text == "1" || text == "T" || text == "t" || text == "TRUE" || text == "true" || text == "True";
    }
    bool equal = false;
    switch (type)
    {
        case SettingType::Boolean: equal = found->second.asBoolean() == value.asBoolean(); break;
        case SettingType::Integer: equal = found->second.asInteger() == value.asInteger(); break;
        case SettingType::Real: equal = found->second.asReal() == value.asReal(); break;
        case SettingType::String: equal = found->second.asString() == value.asString(); break;
        case SettingType::Opaque: break;
    }
    found->second = value;
    if (equal) return true;
    if (name == "FlashCount" || name == "FlashPeriod") updateFlashSettings();
    std::vector<Id> subscribers;
    for (const auto& [id,node] : mNodes) if (node.control) subscribers.push_back(id);
    for (Id id : subscribers)
    {
        const auto* node = get(id);
        if (!node || !node->control) continue;
        const auto params = node->control->params;
        if (params.valueSetting == name) setValue(id,value);
        if (params.enabledSetting == name)
        {
            const bool enabled = value.asString() == "0" ? false : value.asBoolean();
            setEnabled(id,params.invertEnabled ? !enabled : enabled);
        }
        if (params.visibleSetting == name || params.invisibleSetting == name)
        {
            const bool visible = !params.visibleSetting || mSettings.at(*params.visibleSetting).asBoolean();
            const bool invisible = params.invisibleSetting && mSettings.at(*params.invisibleSetting).asBoolean();
            setVisible(id,visible && !invisible);
        }
    }
    return true;
}

bool LLVKWidgetTree::setValue(Id id, const LLSD& value)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    if (found->second.checkBox) return setValue(found->second.checkBox->button,value);
    if (found->second.plainText)
    {
        std::string error;
        return setPlainText(id,value.asString(),error);
    }
    found->second.control->value = value;
    if (found->second.icon)
    {
        auto& icon = *found->second.icon;
        auto& stored = found->second.control->value;
        if (stored.isString() && LLUUID::validate(stored.asString())) stored = LLUUID(stored.asString());
        icon.image = findImage(stored.asString());
        if (icon.image && icon.params.minimumWidth && icon.params.minimumHeight)
        {
            icon.desiredImageWidth = std::max(icon.params.minimumWidth,static_cast<std::int32_t>(icon.image->width()));
            icon.desiredImageHeight = std::max(icon.params.minimumHeight,static_cast<std::int32_t>(icon.image->height()));
        }
    }
    found->second.control->dirty = true;
    return true;
}

bool LLVKWidgetTree::resetDirty(Id id)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    if (found->second.checkBox) return resetDirty(found->second.checkBox->button);
    found->second.control->dirty = false;
    return true;
}

bool LLVKWidgetTree::dispatchControl(Id id, LLVKControl::Callback LLVKControl::Params::* event)
{
    const auto* node = get(id);
    if (!node || !node->control) return false;
    const auto callback = node->control->params.*event;
    const LLSD argument = callback.parameter.value_or(value(id));
    if (callback.function) callback.function(id,argument);
    return true;
}

bool LLVKWidgetTree::commit(Id id)
{
    const auto* node = get(id);
    if (node && node->button) { std::string error; return activateButton(id,error); }
    if (node && node->checkBox) return commitCheckBox(id);
    return dispatchControl(id,&LLVKControl::Params::commit);
}
bool LLVKWidgetTree::mouseEnter(Id id) { return dispatchControl(id,&LLVKControl::Params::mouseEnter); }
bool LLVKWidgetTree::mouseLeave(Id id)
{
    const bool dispatched = dispatchControl(id,&LLVKControl::Params::mouseLeave);
    auto found = mNodes.find(id);
    if (found != mNodes.end() && found->second.button && found->second.button->highlighted)
    {
        found->second.button->highlighted = false;
        ++found->second.button->textGeneration;
    }
    return dispatched;
}

bool LLVKWidgetTree::validate(Id id)
{
    const auto* node = get(id);
    if (!node || !node->control) return false;
    const auto callback = node->control->params.validate;
    const LLSD argument = callback.parameter.value_or(value(id));
    return !callback.function || callback.function(id,argument);
}

bool LLVKWidgetTree::writeBoundValue(Id id, const LLSD& value)
{
    const auto* node = get(id);
    if (!node || !node->control || !node->control->params.valueSetting) return false;
    const auto name = *node->control->params.valueSetting;
    return updateSetting(name,value);
}

bool LLVKWidgetTree::bindValueSetting(Id id, const std::string& name)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    if (found->second.checkBox) return bindValueSetting(found->second.checkBox->button,name);
    if (name.empty()) return true;
    found->second.control->params.valueSetting.reset();
    const auto setting = mSettings.find(name);
    if (setting == mSettings.end()) return false;
    if (!setValue(id,setting->second)) return false;
    found->second.control->params.valueSetting = name;
    return true;
}

bool LLVKWidgetTree::postBuildControl(Id id)
{
    const auto* node = get(id);
    if (!node || !node->control) return false;
    std::vector<Id> requested;
    for (Id child : node->children)
    {
        const auto* candidate = get(child);
        if (candidate->control && candidate->control->params.requestsFront) requested.push_back(child);
    }
    auto& children = mNodes.at(id).children;
    for (Id child : requested)
    {
        auto found = std::find(children.begin(),children.end(),child);
        std::rotate(children.begin(),found,found+1);
    }
    return true;
}

bool LLVKWidgetTree::chromeInChain(Id id) const noexcept
{
    for (const Node* node = get(id); node; node = get(id))
    {
        if (node->control && node->control->params.chrome) return true;
        id = node->parent;
    }
    return false;
}

bool LLVKWidgetTree::requestControlFocus(Id id, bool focus, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->control) { error = "Native focus target is not a control"; return false; }
    if (!node->params.enabled) return true;
    if (focus && !hasAncestor(mKeyboardFocus,id)) return setKeyboardFocus(id,false,false,error);
    if (!focus && hasAncestor(mKeyboardFocus,id)) return setKeyboardFocus(0,false,false,error);
    return true;
}
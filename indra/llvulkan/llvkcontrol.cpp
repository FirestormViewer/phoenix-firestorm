#include "llsd.h"
#include "llsdutil.h"
#include "llvkwidgettree.h"

#include <algorithm>
#include <cmath>

bool LLVKWidgetTree::bindPreferenceColorAlpha(Id panel,std::shared_ptr<LLVKColorTable> colors,std::string& error)
{
    error.clear();
    if (!get(panel) || !get(panel)->panel || !colors) return false;
    Id slider=0,swatch=0;
    std::vector<Id> children{panel};
    for (std::size_t index=0; index<children.size(); ++index)
    {
        const auto* node=get(children[index]);
        if (node->params.name=="MapPickRadiusTransparency") slider=children[index];
        if (node->params.name=="MapPickRadiusColor") swatch=children[index];
        children.insert(children.end(),node->children.begin(),node->children.end());
    }
    if (!slider) return true;
    const auto color=colors->find("MapPickRadiusColor");
    if (!color || !get(slider)->sliderControl || !swatch || !get(swatch)->colorSwatch)
    { error="Native Preferences map-radius color controls are incomplete"; return false; }
    if (!setValue(slider,LLSD(color->get()[3])))
    { error="Native Preferences cannot initialize map-radius alpha"; return false; }
    auto& locals=mNodes.at(panel).preferenceLocalValues;
    if (std::find(locals.begin(),locals.end(),slider)==locals.end()) locals.push_back(slider);
    LLVKControl::Callback callback;
    callback.function=[this,colors,swatch](Id slider,const LLSD&)
    {
        const auto color=colors->find("MapPickRadiusColor");
        if (!color || !get(slider) || !get(swatch)) return;
        auto rgba=color->get();
        rgba[3]=static_cast<float>(value(slider).asReal());
        if (!colors->set("MapPickRadiusColor",rgba)) return;
        LLSD updated=LLSD::emptyArray();
        for (const auto channel : rgba) updated.append(channel);
        std::string problem;
        if (setColorSwatchValue(swatch,updated,problem)) writeBoundValue(swatch,updated);
    };
    return setControlCommit(slider,std::move(callback));
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createControl(const Params& inputView,
    const LLVKControl::Params& inputControl, Id parent, std::string& error)
{
    return createControlImpl(inputView,inputControl,std::nullopt,parent,error);
}

bool LLVKWidgetTree::setSearchHighlighted(Id id,bool highlighted)
{
    const auto found=mNodes.find(id);
    if (found==mNodes.end()) return false;
    found->second.searchHighlighted=highlighted;
    return true;
}

bool LLVKWidgetTree::setControlCommit(Id id, LLVKControl::Callback callback)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    found->second.control->params.commit = std::move(callback);
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createControlImpl(const Params& inputView,
    const LLVKControl::Params& inputControl, std::optional<LLVKIcon> icon, Id parent, std::string& error,
    std::optional<LLVKButton> button, std::optional<LLVKBadge> badge,
    std::optional<BadgeConstruction> badgeConstruction, std::optional<LLVKPlainControl> plainText,
    std::optional<CheckBox> checkBox, std::optional<LLVKPanel> panel, std::optional<LineEditor> lineEditor,
    std::optional<Scrollbar> scrollbar, std::shared_ptr<const ScrollContainerParams> scrollContainer, std::optional<Combo> combo,
    std::optional<Node::Browser> browser, std::optional<Spinner> spinner)
{
    error.clear();
    const auto view = inputView;
    const auto control = inputControl;
    if (!control.font || (parent && !get(parent)))
    { error = "Native control '" + view.name + "' requires a resolved native font and valid parent"; return std::nullopt; }
    auto baseView = view;
    if (lineEditor) baseView.enabled = true;
    const auto id = create(baseView,0,error);
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
        mNodes.at(*id).lineEditor = std::move(lineEditor);
        mNodes.at(*id).scrollbar = std::move(scrollbar);
        mNodes.at(*id).combo = std::move(combo);
        mNodes.at(*id).browser = std::move(browser);
        mNodes.at(*id).spinner = std::move(spinner);
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
        if (mNodes.at(*id).spinner && !constructSpinnerChildren(*id,error))
        { std::string cleanup; erase(*id,cleanup); return std::nullopt; }
        if (mNodes.at(*id).combo && !constructComboChildren(*id,error))
        {
            std::string cleanup;
            erase(*id,cleanup);
            return std::nullopt;
        }
        if (scrollContainer && !constructScrollContainerChildren(*id,*scrollContainer,error))
        {
            std::string cleanup;
            erase(*id,cleanup);
            return std::nullopt;
        }
        if (auto& editor = mNodes.at(*id).lineEditor; editor)
        {
            mNodes.at(*id).control->value = editor->text.text();
            Params border;
            border.name = "view_border";
            border.mouseOpaque = false;
            border.follows = Left | Right | Top | Bottom;
            const auto width = std::int64_t(view.rect.right)-view.rect.left-1;
            const auto height = std::int64_t(view.rect.top)-view.rect.bottom-1;
            if (width < INT32_MIN || width > INT32_MAX || height < INT32_MIN || height > INT32_MAX)
            {
                std::string cleanup;
                erase(*id,cleanup);
                error = "Native line editor border dimensions overflow";
                return std::nullopt;
            }
            border.rect = {0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
            auto borderParams = editor->params.border;
            borderParams.bevel = LLVKBorder::Bevel::In;
            const auto child = createBorder(border,borderParams,*id,error);
            if (!child)
            {
                std::string cleanup;
                erase(*id,cleanup);
                return std::nullopt;
            }
            editor->border = *child;
        }
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
        if (mNodes.at(*id).scrollbar && !constructScrollbarChildren(*id,error))
        {
            std::string cleanup;
            erase(*id,cleanup);
            return std::nullopt;
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
            error = "Native control '"+view.name+"' rejected its initial value";
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
        if (const auto& installed = get(*id)->combo; installed && !installed->params->allowTextEntry && installed->params->label.empty() &&
            !installed->items.empty() && installed->items.front().enabled)
        {
            if (!selectComboItem(*id,0,error)) { std::string cleanup; erase(*id,cleanup); return std::nullopt; }
        }
        mNodes.at(*id).control->params.mouseEnter = control.mouseEnter;
        mNodes.at(*id).control->params.mouseLeave = control.mouseLeave;
        if (mNodes.at(*id).lineEditor) setEnabled(*id,view.enabled);
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

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createProgressBar(const Params& view,const LLVKControl::Params& control,
    const Node::ProgressBar& progress,Id parent,std::string& error)
{
    for (const auto& color : {progress.background,progress.fill})
        for (const auto component : color.get()) if (!std::isfinite(component))
        { error="Invalid native progress color"; return std::nullopt; }
    const auto id=createControl(view,control,parent,error);
    if (id) mNodes.at(*id).progressBar=progress;
    return id;
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

bool LLVKWidgetTree::setPreferenceSnapshotAllowlist(Id panel,std::optional<std::set<std::string>> names)
{
    if (!get(panel) || !get(panel)->panel) return false;
    mNodes.at(panel).preferenceSnapshotAllowlist=std::move(names);
    return true;
}

std::optional<LLVKWidgetTree::PreferenceSnapshot> LLVKWidgetTree::snapshotPreferences(Id root,std::string& error) const
{
    error.clear();
    if (!get(root)) { error="Native Preferences snapshot root is missing"; return std::nullopt; }
    PreferenceSnapshot snapshot;
    std::vector<Id> pending{root};
    for (std::size_t index=0; index<pending.size(); ++index)
    {
        const auto id=pending[index];
        const auto* node=get(id);
        if (!node) continue;
        for (const auto local : node->preferenceLocalValues)
            if (get(local)) snapshot.localValues[local]=value(local);
        if (node->colorSwatch) snapshot.colors[id]=value(id);
        if (node->control && node->control->params.valueSetting)
        {
            const auto& name=*node->control->params.valueSetting;
            bool included=true;
            for (auto ancestor=id; ancestor && get(ancestor); ancestor=get(ancestor)->parent)
            {
                const auto& allowed=get(ancestor)->preferenceSnapshotAllowlist;
                if (allowed && !allowed->contains(name)) { included=false; break; }
                if (ancestor==root) break;
            }
            if (included)
                if (const auto current=setting(name)) snapshot.settings[name]=*current;
        }
        pending.insert(pending.end(),node->children.begin(),node->children.end());
    }
    return snapshot;
}

bool LLVKWidgetTree::restorePreferences(const PreferenceSnapshot& snapshot,const std::vector<std::string>& skip,std::string& error)
{
    error.clear();
    for (const auto& [name,value] : snapshot.settings)
    {
        if ((name=="InstantMessageLogPath" && value.asString().empty()) || std::find(skip.begin(),skip.end(),name)!=skip.end()) continue;
        if (!updateSetting(name,value)) { error="Native Preferences could not restore setting: "+name; return false; }
    }
    for (const auto& [id,value] : snapshot.colors)
    {
        const auto* node=get(id);
        if (!node || !node->colorSwatch) continue;
        if (!setColorSwatchValue(id,value,error)) return false;
        writeBoundValue(id,value);
        if (get(id)) dispatchControl(id,&LLVKControl::Params::commit);
    }
    for (const auto& [id,value] : snapshot.localValues)
        if (get(id) && !setValue(id,value))
        { error="Native Preferences could not restore local control value"; return false; }
    return true;
}

std::optional<std::uint64_t> LLVKWidgetTree::subscribeSetting(const std::string& name,SettingCallback callback)
{
    if (!mSettings.contains(name) || !callback || !mNextSettingSubscription) return std::nullopt;
    const auto subscription=mNextSettingSubscription++;
    mSettingSubscriptions.emplace(subscription,SettingSubscription{name,std::move(callback)});
    return subscription;
}

bool LLVKWidgetTree::unsubscribeSetting(std::uint64_t subscription)
{
    return mSettingSubscriptions.erase(subscription)!=0;
}

bool LLVKWidgetTree::bindSettings(LLControlGroup& group,std::string& error)
{
    error.clear();
    struct Collect final : LLControlGroup::ApplyFunctor
    {
        std::map<std::string,LLControlVariablePtr> controls;
        void apply(const std::string& name,LLControlVariable* control) override { controls.emplace(name,control); }
    } collect;
    group.applyToAll(&collect);
    for (const auto& [name,control] : collect.controls)
        if (mSettingControls.contains(name)) { error="Duplicate settings group binding: "+name; return false; }
    for (auto& [name,control] : collect.controls)
    {
        auto type=SettingType::Opaque;
        switch (control->type())
        {
            case TYPE_BOOLEAN: type=SettingType::Boolean; break;
            case TYPE_U32: case TYPE_S32: type=SettingType::Integer; break;
            case TYPE_F32: type=SettingType::Real; break;
            case TYPE_STRING: type=SettingType::String; break;
            default: break;
        }
        const auto value=control->isType(TYPE_BOOLEAN) ? LLSD(control->getValue().asBoolean()) : control->getValue();
        if (!mSettings.contains(name)) defineSetting(name,value,type);
        else { mSettingTypes[name]=type; publishSetting(name,value); }
        mSettingControls.emplace(name,control);
        mControlConnections.emplace_back(control->getSignal()->connect([this,name](LLControlVariable* changed,const LLSD& value,const LLSD&)
        { publishSetting(name,changed->isType(TYPE_BOOLEAN) ? LLSD(value.asBoolean()) : value); }));
    }
    return true;
}

bool LLVKWidgetTree::updateSetting(const std::string& name,const LLSD& value)
{
    const auto found=mSettingControls.find(name);
    if (found==mSettingControls.end()) return publishSetting(name,value);
    auto control=found->second;
    control->setValue(value,false);
    const auto current=control->isType(TYPE_BOOLEAN) ? LLSD(control->getValue().asBoolean()) : control->getValue();
    publishSetting(name,current);
    return llsd_equals(current,value);
}

bool LLVKWidgetTree::publishSetting(const std::string& name, const LLSD& inputValue)
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
        case SettingType::Opaque: equal = llsd_equals(found->second,value); break;
    }
    const auto previous=found->second;
    found->second = value;
    if (equal) return true;
    std::vector<std::uint64_t> listeners;
    for (const auto& [subscription,listener] : mSettingSubscriptions)
        if (listener.name==name) listeners.push_back(subscription);
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
    for (const auto subscription : listeners)
    {
        const auto found=mSettingSubscriptions.find(subscription);
        if (found==mSettingSubscriptions.end()) continue;
        const auto callback=found->second.callback;
        callback(value,previous);
    }
    return true;
}

bool LLVKWidgetTree::setValue(Id id, const LLSD& value)
{
    if (get(id) && get(id)->textureControl)
    { std::string error; return setTextureValue(id,value,error); }
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    if (found->second.scrollList) { std::string error; return selectScrollListValue(id,value,true,error); }
    if (found->second.colorSwatch) { std::string error; return setColorSwatchValue(id,value,error); }
    if (found->second.searchEditor) return setValue(found->second.searchEditor->editor,value);
    if (found->second.textEditor) { std::string error; return setTextEditorText(id,value.asString(),error); }
    if (found->second.sliderControl) { std::string error; return setSliderControlValue(id,value,error); }
    if (found->second.slider) { std::string error; return setSliderValue(id,static_cast<float>(value.asReal()),false,false,error); }
    if (found->second.radioGroup) { std::string error; return setRadioValue(id,value,error); }
    if (found->second.spinner) { std::string error; return setSpinnerValue(id,value,false,error); }
    if (found->second.tabContainer)
    {
        const auto index = value.asInteger();
        const auto& tabs = found->second.tabContainer->tabs;
        if (index < 0 || static_cast<std::size_t>(index) >= tabs.size()) return false;
        const auto panel = tabs[index].panel;
        std::string error;
        return selectTabPanel(id,panel,error);
    }
    if (found->second.combo) { std::string error; return setComboValue(id,value,error); }
    if (found->second.checkBox) return setValue(found->second.checkBox->button,value);
    if (found->second.scrollbar)
    {
        std::string error;
        setScrollPosition(id,value.asInteger(),true,error);
        return error.empty();
    }
    if (found->second.lineEditor)
    {
        std::string error;
        auto text = found->second.lineEditor->text;
        if (!text.assign(value.asString(),true,hasAncestor(mKeyboardFocus,id),mLabelContext,error)) return false;
        LLSD stored(text.text());
        found->second.lineEditor->text = std::move(text);
        found->second.control->value = std::move(stored);
        found->second.control->dirty = false;
        return true;
    }
    if (found->second.plainText)
    {
        std::string error;
        return setPlainText(id,value.asString(),error);
    }
    std::shared_ptr<const LLVKWidgetImage> image;
    if (found->second.icon)
    {
        std::string error;
        image = findImage(value.asString(),error);
        if (!error.empty()) return false;
    }
    found->second.control->value = value;
    if (found->second.icon)
    {
        auto& icon = *found->second.icon;
        auto& stored = found->second.control->value;
        if (stored.isString() && LLUUID::validate(stored.asString())) stored = LLUUID(stored.asString());
        icon.image = std::move(image);
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
    if (found->second.combo) found->second.combo->dirty = false;
    if (found->second.lineEditor) found->second.lineEditor->text.resetDirty();
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
    if (node && node->textEditor) { std::string error; return commitTextEditor(id,error); }
    if (node && node->spinner) { std::string error; return commitSpinner(id,error); }
    if (node && node->combo) return commitCombo(id);
    if (node && node->button) { std::string error; return activateButton(id,error); }
    if (node && node->checkBox) return commitCheckBox(id);
    if (node && node->lineEditor) return commitLineEditor(id);
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
    if (node->textEditor) return requestControlFocus(node->textEditor->body,focus,error);
    if (node->searchEditor) return requestControlFocus(node->searchEditor->editor,focus,error);
    if (node->spinner) return requestControlFocus(node->spinner->editor,focus,error);
    if (node->lineEditor)
    {
        if (!focus) lineLanguageInput(id,true);
        if (!get(id)) return true;
        if (focus && !hasAncestor(mKeyboardFocus,id) && node->lineEditor->params.text.selectOnFocus)
        {
            const auto validator = node->lineEditor->params.inputPrevalidator;
            const auto text = node->lineEditor->text.display();
            const bool accepted = !validator || validator(text);
            if (!get(id)) return true;
            if (accepted && !mNodes.at(id).lineEditor->text.selectAll(error)) return false;
            if (accepted) mNodes.at(id).lineEditor->text.finishSelection();
        }
        node = get(id);
    }
    if (!node->params.enabled) return true;
    if (focus && node->panel && !hasAncestor(mKeyboardFocus,id))
    {
        if (!setKeyboardFocus(id,false,false,error)) return false;
        if (!get(id)) return true;
        focusFirst(id,true,error);
        return error.empty();
    }
    if (focus && !hasAncestor(mKeyboardFocus,id)) return setKeyboardFocus(id,false,false,error);
    if (!focus && hasAncestor(mKeyboardFocus,id)) return setKeyboardFocus(0,false,false,error);
    return true;
}

std::optional<std::vector<LLVKWidgetTree::Id>> LLVKWidgetTree::tabOrder(Id root, std::string& error, bool textOnly) const
{
    error.clear();
    if (!get(root)) { error = "Native tab-order root is missing"; return std::nullopt; }
    std::vector<Id> result;
    const auto visit = [&](auto&& self, Id id) -> void
    {
        const auto& node = *get(id);
        if (!node.params.visible || !node.params.enabled) return;
        const bool accepted = node.control && node.control->params.tabStop && (!textOnly || node.lineEditor.has_value());
        const bool descend = !node.control || node.control->params.tabStop;
        const auto before = result.size();
        if (descend)
        {
            auto children = node.children;
            std::stable_sort(children.begin(),children.end(),[&](Id first,Id second)
            {
                const auto firstGroup = get(first)->params.tabGroup.value_or(0);
                const auto secondGroup = get(second)->params.tabGroup.value_or(0);
                const auto defaultGroup = node.params.defaultTabGroup;
                if (firstGroup < defaultGroup && secondGroup >= defaultGroup) return true;
                if (secondGroup < defaultGroup && firstGroup >= defaultGroup) return false;
                return firstGroup > secondGroup;
            });
            for (const Id child : children) self(self,child);
        }
        if (accepted && result.size() == before) result.push_back(id);
    };
    visit(visit,root);
    return result;
}

bool LLVKWidgetTree::focusFirst(Id root, bool flash, std::string& error)
{
    const auto candidates = tabOrder(root,error);
    if (!candidates || candidates->empty()) return false;
    const Id target = candidates->back();
    if (hasAncestor(mKeyboardFocus,target)) return true;
    return enterFocus(target,flash,error);
}

bool LLVKWidgetTree::enterFocus(Id target, bool flash, std::string& error)
{
    if (!requestControlFocus(target,true,error)) return false;
    const auto* node = get(target);
    if (!node) return true;
    if (node->lineEditor)
    {
        const auto validator = node->lineEditor->params.inputPrevalidator;
        const auto text = node->lineEditor->text.display();
        const bool accepted = !validator || validator(text);
        if (!get(target)) return true;
        if (accepted && !mNodes.at(target).lineEditor->text.selectAll(error)) return false;
    }
    notify(target,&Events::tabInto);
    if (flash) mFocusFlashTime = mTime;
    return true;
}

bool LLVKWidgetTree::moveFocus(Id root, bool forward, bool textOnly, std::string& error)
{
    const auto setting = mSettings.find("TabToTextFieldsOnly");
    const auto candidates = tabOrder(root,error,textOnly || (setting != mSettings.end() && setting->second.asBoolean()));
    if (!candidates || candidates->empty()) return false;
    const auto count = candidates->size();
    std::optional<std::size_t> focused;
    for (std::size_t step = 0; step < count; ++step)
    {
        const auto index = forward ? count-1-step : step;
        if (hasAncestor(mKeyboardFocus,candidates->at(index))) { focused = index; break; }
    }
    const auto index = focused ? (forward ? (*focused ? *focused-1 : count-1) : (*focused+1)%count) : (forward ? count-1 : 0);
    const Id target = candidates->at(index);
    if (!forward && hasAncestor(mKeyboardFocus,target)) return true;
    return enterFocus(target,true,error);
}

float LLVKWidgetTree::focusFlashAmount() const noexcept
{
    return std::clamp(1.f-static_cast<float>(mTime-mFocusFlashTime)/0.3f,0.f,1.f);
}

void LLVKWidgetTree::triggerFocusFlash() noexcept
{
    mFocusFlashTime=mTime;
}
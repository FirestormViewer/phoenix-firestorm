#include "llvkwidgettree.h"

#include <cmath>

bool LLVKWidgetTree::moveTab(Id id, bool forward, std::string& error)
{
    error.clear();
    const auto* owner = get(id);
    if (!owner || !owner->tabContainer || owner->tabContainer->tabs.empty()) return false;
    const auto tabs = owner->tabContainer->tabs;
    const auto selected = owner->tabContainer->selected;
    std::size_t current = forward ? tabs.size()-1 : 0;
    bool buttonFocused = false;
    for (std::size_t index = 0; index < tabs.size(); ++index)
        if (tabs[index].panel == selected) { current = index; buttonFocused = mKeyboardFocus == tabs[index].button; break; }
    for (std::size_t attempt = 0; attempt < tabs.size(); ++attempt)
    {
        current = forward ? (current+1)%tabs.size() : (current+tabs.size()-1)%tabs.size();
        if (selectTabPanel(id,tabs[current].panel,error))
        {
            if (buttonFocused && get(tabs[current].button)) return requestControlFocus(tabs[current].button,true,error);
            return true;
        }
        if (!error.empty() || !get(id)) return false;
    }
    return false;
}

bool LLVKWidgetTree::tabContainerKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error)
{
    error.clear();
    const auto* owner = get(id);
    if (!owner || !owner->tabContainer) return false;
    const bool horizontal = key == ScrollKey::Left || key == ScrollKey::Right;
    if (horizontal && modifiers.alt && !modifiers.control)
    {
        if (modifiers.shift)
            for (Id parent = owner->parent; get(parent); parent = get(parent)->parent)
                if (get(parent)->tabContainer) { id = parent; break; }
        moveTab(id,key == ScrollKey::Right,error);
        owner = get(id);
        if (owner && owner->tabContainer && get(owner->tabContainer->selected))
            requestControlFocus(owner->tabContainer->selected,true,error);
        return error.empty();
    }
    if (hasAncestor(mKeyboardFocus,owner->tabContainer->selected)) return false;
    if (horizontal) { moveTab(id,key == ScrollKey::Right,error); return error.empty(); }
    if (key == ScrollKey::Down)
    {
        if (get(owner->tabContainer->selected)) requestControlFocus(owner->tabContainer->selected,true,error);
        return error.empty();
    }
    return key == ScrollKey::Up;
}

bool LLVKWidgetTree::layoutTopTabs(Id container, const Node::TabContainer::Layout& layout, std::string& error)
{
    error.clear();
    const auto* owner = get(container);
    if (!owner || !owner->tabContainer || layout.tabHeight <= 0 || layout.minimumWidth < 0 ||
        layout.maximumWidth < layout.minimumWidth || layout.labelPadding < 0 || layout.horizontalPadding < 0 ||
        layout.panelOverlap < 0 || layout.panelOverlap > layout.tabHeight)
    { error = "Invalid native top-tab layout"; return false; }
    const auto width = std::int64_t(owner->params.rect.right)-owner->params.rect.left;
    const auto height = std::int64_t(owner->params.rect.top)-owner->params.rect.bottom;
    const auto tabs = owner->tabContainer->tabs;
    const auto top = layout.hidden ? height : height-1-layout.tabHeight+layout.panelOverlap;
    const auto left = layout.panelOffset ? 3 : 1;
    const auto right = width-(layout.panelOffset ? 2 : 1);
    if (top < 1 || right < left) { error = "Native tab container is too small for its content"; return false; }
    struct Placement { Id panel, button; Rect content, tab; };
    std::vector<Placement> placements;
    std::int64_t next = 1+std::int64_t(layout.horizontalPadding);
    for (const auto& entry : tabs)
    {
        const auto* panel = get(entry.panel);
        const auto* button = get(entry.button);
        if (!panel || panel->parent != container || !button || !button->button || button->parent != container) continue;
        const auto& label = button->button->params.label;
        const auto measured = button->control->params.font->measureRun(label,0,label.size(),1.f,true,false,error);
        if (!measured) return false;
        const double padded = std::ceil(measured->width)+layout.labelPadding;
        if (!std::isfinite(padded) || padded > INT32_MAX) { error = "Native tab label width overflows"; return false; }
        const auto tabWidth = std::clamp(static_cast<std::int32_t>(padded),layout.minimumWidth,layout.maximumWidth);
        if (next+tabWidth > INT32_MAX) { error = "Native tab strip width overflows"; return false; }
        placements.push_back({entry.panel,entry.button,{left,1,static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)},
            {static_cast<std::int32_t>(next),static_cast<std::int32_t>(height-layout.tabHeight),
                static_cast<std::int32_t>(next+tabWidth),static_cast<std::int32_t>(height)}});
        next += tabWidth;
    }
    if (!layout.hidden && next > width-1)
    { error = "Native top-tab overflow scrolling is not implemented"; return false; }
    ShapeChanges changes;
    for (const auto& placement : placements)
        for (const auto& [id,rect] : {std::pair{placement.panel,placement.content},std::pair{placement.button,placement.tab}})
            if (get(id)->params.rect != rect && !planReshape(id,std::int64_t(rect.right)-rect.left,std::int64_t(rect.top)-rect.bottom,rect,changes,error)) return false;
    if (!completeShapes(changes,error)) return false;
    for (const auto& placement : placements)
    {
        if (get(placement.panel)) mNodes.at(placement.panel).params.follows = Left|Right|Top|Bottom;
        if (get(placement.button)) setVisible(placement.button,!layout.hidden);
    }
    if (!get(container) || !get(container)->tabContainer) { error = "Native tab layout owner was removed"; return false; }
    mNodes.at(container).tabContainer->layout = layout;
    return true;
}

bool LLVKWidgetTree::initializeTabContainer(Id panel, std::string& error)
{
    error.clear();
    const auto* node = get(panel);
    if (!node || !node->panel || !node->control || node->tabContainer)
    { error = "Invalid native tab container initialization"; return false; }
    mNodes.at(panel).tabContainer.emplace();
    return true;
}

bool LLVKWidgetTree::attachTabPanel(Id container, Id panel, Id button, std::string& error)
{
    error.clear();
    const auto* owner = get(container);
    const auto* content = get(panel);
    const auto* tab = get(button);
    if (!owner || !owner->tabContainer || !content || !content->panel || !tab || !tab->button ||
        content->parent != container || tab->parent != container || panel == container)
    { error = "Native tab requires an owned panel and button"; return false; }
    for (const auto& entry : owner->tabContainer->tabs)
        if (entry.panel == panel || entry.button == button)
        { error = "Native tab panel or button is already registered"; return false; }
    mNodes.at(container).tabContainer->tabs.push_back({panel,button});
    mNodes.at(button).control->params.tabStop = false;
    LLVKControl::Callback callback;
    callback.function = [this,container,panel](Id,const LLSD&)
    {
        std::string problem;
        selectTabPanel(container,panel,problem);
        if (get(panel)) requestControlFocus(panel,true,problem);
    };
    setControlCommit(button,std::move(callback));
    setVisible(panel,false);
    return get(container) && get(panel) && get(button);
}

bool LLVKWidgetTree::selectTabPanel(Id container, Id panel, std::string& error)
{
    error.clear();
    const auto* owner = get(container);
    const auto* content = get(panel);
    if (!owner || !owner->tabContainer || !content || content->parent != container) return false;
    Id button = 0;
    for (const auto& entry : owner->tabContainer->tabs) if (entry.panel == panel) button = entry.button;
    if (!button || !get(button) || !get(button)->params.enabled) return false;
    const auto validate = owner->control->params.validate;
    const LLSD argument(content->params.name);
    const auto before = owner->tabContainer->selectionGeneration;
    if (validate.function && !validate.function(container,validate.parameter.value_or(argument))) return false;
    owner = get(container);
    if (!owner || !owner->tabContainer || owner->tabContainer->selectionGeneration != before || !get(panel) ||
        get(panel)->parent != container || !get(button) || get(button)->parent != container || !get(button)->params.enabled) return false;
    const auto tabs = owner->tabContainer->tabs;
    auto& state = *mNodes.at(container).tabContainer;
    state.selected = panel;
    const auto generation = ++state.selectionGeneration;
    for (const auto& entry : tabs)
    {
        if (get(entry.button) && get(entry.button)->parent == container)
        {
            if (!setButtonToggle(entry.button,entry.panel == panel,error)) return false;
            mNodes.at(entry.button).control->params.tabStop = entry.panel == panel;
        }
        if (get(entry.panel) && get(entry.panel)->parent == container) setVisible(entry.panel,entry.panel == panel);
        owner = get(container);
        if (!owner || !owner->tabContainer || owner->tabContainer->selectionGeneration != generation) return false;
    }
    const auto callback = owner->control->params.commit;
    if (callback.function) callback.function(container,callback.parameter.value_or(argument));
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createBorder(const Params& view,
    const LLVKBorder::Params& params, Id parent, std::string& error)
{
    error.clear();
    if (params.thickness < 0 || params.thickness > 2 ||
        (params.bevel != LLVKBorder::Bevel::In && params.bevel != LLVKBorder::Bevel::Out &&
         params.bevel != LLVKBorder::Bevel::Bright && params.bevel != LLVKBorder::Bevel::None) ||
        (params.style != LLVKBorder::Style::Line && params.style != LLVKBorder::Style::Texture))
    { error = "Invalid native border thickness, style or bevel"; return std::nullopt; }
    for (const auto& color : {params.highlightLight,params.highlightDark,params.shadowLight,params.shadowDark})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native border color"; return std::nullopt; }
    const auto id = create(view,parent,error);
    if (id) mNodes.at(*id).border = LLVKBorder{params,false};
    return id;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createPanel(const Params& view,
    const LLVKControl::Params& control, const LLVKPanel::Params& params, Id parent, std::string& error)
{
    error.clear();
    for (const auto& color : {params.opaqueColor,params.transparentColor,params.opaqueImageOverlay,params.transparentImageOverlay})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native panel color"; return std::nullopt; }
    LLVKPanel panel;
    panel.params = params;
    panel.label.assign(params.label);
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::move(panel));
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::constructPanel(const Params& view,
    const LLVKControl::Params& control, const LLVKPanel::Params& params, std::string& error)
{
    error.clear();
    if (!control.font) { error = "Native panel constructor requires a resolved font"; return std::nullopt; }
    for (const auto& color : {params.opaqueColor,params.transparentColor,params.opaqueImageOverlay,params.transparentImageOverlay})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native panel constructor color"; return std::nullopt; }
    const auto id = create(view,0,error);
    if (!id) return std::nullopt;
    try
    {
        LLVKControl state;
        state.params.font = control.font;
        state.params.tabStop = control.tabStop;
        state.params.chrome = control.chrome;
        state.params.requestsFront = control.requestsFront;
        LLVKPanel panel;
        panel.params = params;
        panel.label.assign(params.label);
        mNodes.at(*id).control = std::move(state);
        mNodes.at(*id).panel = std::move(panel);
        mNodes.at(*id).acceptsBadge = params.acceptsBadge;
        if (params.hasBorder && !addPanelBorder(*id,params.border,error))
        {
            std::string cleanup;
            erase(*id,cleanup);
            return std::nullopt;
        }
    }
    catch (...)
    {
        std::string cleanup;
        erase(*id,cleanup);
        throw;
    }
    return id;
}

bool LLVKWidgetTree::initializePanel(Id id, const Params& inputView, const LLVKControl::Params& inputControl,
    const LLVKPanel::Params& inputPanel, std::string& error)
{
    error.clear();
    const auto view = inputView;
    const auto control = inputControl;
    const auto panel = inputPanel;
    const auto* node = get(id);
    if (!node || !node->panel || !control.font)
    { error = "Invalid native panel initialization target or font"; return false; }
    const auto append = [this](LLVKControl::Callback first, LLVKControl::Callback second)
    {
        if (!second.function) return first;
        if (!first.function) return second;
        return LLVKControl::Callback{[this,first,second](Id target,const LLSD& argument)
        {
            first.function(target,first.parameter.value_or(argument));
            if (get(target)) second.function(target,second.parameter.value_or(argument));
        },std::nullopt};
    };
    setVisible(id,view.visible);
    if (!get(id)) { error = "Native panel removed by visibility callback"; return false; }
    setEnabled(id,view.enabled);
    auto& params = mNodes.at(id).control->params;
    params.font = control.font;
    params.requestsFront = control.requestsFront;
    params.chrome = control.chrome;
    params.tabStop = control.tabStop;
    if (!view.name.empty()) mNodes.at(id).params.name = view.name;
    mNodes.at(id).params.layout = view.layout;
    mNodes.at(id).params.soundFlags = view.soundFlags;
    if (control.valueSetting) bindValueSetting(id,*control.valueSetting);
    if (control.enabledSetting && mSettings.contains(*control.enabledSetting))
    { params.enabledSetting = control.enabledSetting; params.invertEnabled = control.invertEnabled; }
    if (control.visibleSetting && mSettings.contains(*control.visibleSetting)) params.visibleSetting = control.visibleSetting;
    if (control.invisibleSetting && mSettings.contains(*control.invisibleSetting)) params.invisibleSetting = control.invisibleSetting;
    applyControlSettings(id);
    if (!get(id)) { error = "Native panel removed by settings callback"; return false; }
    if (control.initialValue && !control.valueSetting && !setValue(id,*control.initialValue))
    { error = "Native panel rejected initial value"; return false; }
    params.commit = append(params.commit,control.commit);
    if (control.validate.function)
    {
        const auto previous = params.validate;
        const auto next = control.validate;
        if (!previous.function) params.validate = next;
        else params.validate = {[this,previous,next](Id target,const LLSD& value)
        {
            const bool first = previous.function(target,previous.parameter.value_or(value));
            if (!get(target)) return first;
            const bool second = next.function(target,next.parameter.value_or(value));
            return first && second;
        },std::nullopt};
    }
    if (control.init.function) control.init.function(id,control.init.parameter.value_or(LLSD()));
    if (!get(id)) { error = "Native panel init callback destroyed its owner"; return false; }
    auto& current = mNodes.at(id);
    current.control->params.mouseEnter = append(current.control->params.mouseEnter,control.mouseEnter);
    current.control->params.mouseLeave = append(current.control->params.mouseLeave,control.mouseLeave);
    current.panel->visible = append(current.panel->visible,panel.visible);
    current.panel->params = panel;
    current.panel->label.assign(panel.label);
    for (const auto& [name,value] : panel.strings) current.panel->strings[name].assign(value);
    ShapeChanges changes;
    if (!planReshape(id,std::int64_t(view.rect.right)-view.rect.left,std::int64_t(view.rect.top)-view.rect.bottom,
                     view.rect,changes,error)) return false;
    if (!completeShapes(changes,error)) return false;
    if (!get(id)) { error = "Native panel removed during descendant resize"; return false; }
    auto& resized = mNodes.at(id);
    resized.params.follows = view.follows;
    resized.params.tooltip = view.tooltip;
    resized.params.fromDeclaration = view.fromDeclaration;
    resized.params.useBoundingRect = view.useBoundingRect;
    resized.params.mouseOpaque = view.mouseOpaque;
    resized.params.tabGroup = view.tabGroup;
    resized.params.defaultTabGroup = view.defaultTabGroup;
    resized.params.focusRoot = view.focusRoot;
    resized.acceptsBadge = panel.acceptsBadge;
    return !panel.hasBorder || addPanelBorder(id,panel.border,error);
}

LLVKWidgetTree::Id LLVKWidgetTree::rootMostFocusRoot(Id id) const
{
    Id root = 0;
    for (const Node* node = get(id); node && node->control && node->control->params.tabStop; node = get(id))
    {
        if (node->params.focusRoot) root = id;
        id = node->parent;
        while (get(id) && !get(id)->control) id = get(id)->parent;
    }
    return root;
}

bool LLVKWidgetTree::setPanelDefaultButton(Id id, Id button, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel || (button && (!get(button) || !get(button)->button)))
    { error = "Invalid native panel default button"; return false; }
    mNodes.at(id).panel->defaultButton = button;
    return true;
}

bool LLVKWidgetTree::panelKey(Id id, PanelKey key, LLVKLineEditor::Modifiers modifiers, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native panel key target is missing"; return false; }
    if (key == PanelKey::Escape)
    {
        requestControlFocus(id,false,error);
        return error.empty();
    }
    const auto* focused = get(mKeyboardFocus);
    if (!focused || !focused->control) return false;
    if (key == PanelKey::Tab && !modifiers.control && !modifiers.alt)
    {
        const auto root = rootMostFocusRoot(mKeyboardFocus);
        return root && moveFocus(root,!modifiers.shift,false,error);
    }
    if (key != PanelKey::Return || modifiers.shift || modifiers.control || modifiers.alt) return false;
    if (focused->button && focused->button->params.commitOnReturn) return false;
    const Id button = node->panel->defaultButton;
    const auto* target = get(button);
    if (target && target->button && target->params.visible && target->params.enabled)
    {
        commit(button);
        return true;
    }
    if (focused->lineEditor)
    {
        commit(mKeyboardFocus);
        return true;
    }
    return false;
}

bool LLVKWidgetTree::addPanelBorder(Id id, const LLVKBorder::Params& params, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native border owner is not a panel"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width < INT32_MIN || width > INT32_MAX || height < INT32_MIN || height > INT32_MAX)
    { error = "Native panel border rectangle overflow"; return false; }
    Params view;
    view.name = "view_border";
    view.mouseOpaque = false;
    view.follows = Left | Right | Top | Bottom;
    view.rect = {0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
    const Id previous = node->panel->border;
    const auto border = createBorder(view,params,id,error);
    if (!border) return false;
    if (previous && get(previous) && !erase(previous,error))
    {
        std::string cleanup;
        erase(*border,cleanup);
        return false;
    }
    if (!get(id)) { error = "Native panel deleted during border replacement"; return false; }
    mNodes.at(id).panel->border = *border;
    return true;
}

bool LLVKWidgetTree::removePanelBorder(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native border owner is not a panel"; return false; }
    const Id border = node->panel->border;
    if (get(border) && !erase(border,error)) return false;
    if (get(id)) mNodes.at(id).panel->border = 0;
    return true;
}

bool LLVKWidgetTree::setPanelFilename(Id id, const std::string& filename)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.panel) return false;
    found->second.panel->params.filename = filename;
    return true;
}

std::optional<std::string> LLVKWidgetTree::panelString(Id id, const std::string& name,
    const LLVKLabel::Arguments& arguments, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->panel) { error = "Native string owner is not a panel"; return std::nullopt; }
    const auto found = node->panel->strings.find(name);
    if (found == node->panel->strings.end()) { error = "Missing native panel string: " + name; return std::nullopt; }
    auto label = found->second;
    label.setArguments(arguments);
    return label.resolve(mLabelContext);
}
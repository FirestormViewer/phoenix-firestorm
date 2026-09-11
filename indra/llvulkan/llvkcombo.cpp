#include "llvkwidgettree.h"
#include "llstring.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <cwctype>

namespace
{
    std::string foldedLabel(std::string value)
    {
        for (auto& character : value) character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        return value;
    }
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createCombo(const Params& view,
    const LLVKControl::Params& control, const ComboParams& params, Id parent, std::string& error)
{
    error.clear();
    if (params.items.size() > 10000 || params.buttonShadow < 0 || params.maximumBytes > 4*1024*1024 || !params.maximumBytes)
    { error = "Invalid native login combo item or text limits"; return std::nullopt; }
    Combo combo;
    combo.params = std::make_shared<const ComboParams>(params);
    combo.items = params.items;
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::nullopt,{},std::move(combo));
}

bool LLVKWidgetTree::constructComboChildren(Id id, std::string& error)
{
    const auto params = get(id)->combo->params;
    const auto rect = get(id)->params.rect;
    const auto width = std::int64_t(rect.right)-rect.left, height = std::int64_t(rect.top)-rect.bottom;
    const auto arrowWidth = std::int64_t(std::max<std::uint32_t>(8,params->button.images.unselected ? params->button.images.unselected->width() : 0))+
        2*std::int64_t(params->buttonShadow);
    if (width < 0 || height < 0 || width > INT32_MAX || height > INT32_MAX || (params->allowTextEntry && arrowWidth > width))
    { error = "Native combo '" + get(id)->params.name + "' dimensions cannot fit its arrow"; return false; }
    Params buttonView;
    buttonView.name = "Combobox Button";
    buttonView.rect = {params->allowTextEntry ? static_cast<std::int32_t>(width-arrowWidth) : 0,0,
        static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
    buttonView.follows = params->allowTextEntry ? Right | Top | Bottom : Left | Bottom | Right;
    auto buttonParams = params->button;
    buttonParams.mouseDown.function = [this,id](Id,const LLSD&)
    {
        const auto* node = get(id);
        if (!node || !node->combo || !get(node->combo->list)) return;
        if (get(node->combo->list)->params.visible) { hideComboList(id); return; }
        std::string error;
        if (!showComboList(id,error) || !get(id)) return;
        const auto state = *get(id)->combo;
        if (mMouseCapture == state.button) setMouseCapture(state.list,error);
    };
    auto buttonControl = params->buttonControl;
    if (params->allowTextEntry) { buttonParams.rightPad = 2; buttonControl.tabStop = false; buttonParams.labelAlign = LLVKButton::Align::Center; }
    const auto button = createButton(buttonView,buttonControl,buttonParams,0,error);
    if (!button) return false;
    if (!get(id) || !postBuildButton(*button,error) || !reparent(*button,id,false,0,error))
    { std::string cleanup; erase(*button,cleanup); return false; }
    mNodes.at(id).combo->button = *button;
    Params listView;
    listView.name = "ComboBox";
    listView.visible = false;
    const auto list = createControl(listView,params->listControl,0,error);
    if (!list) return false;
    if (!get(id) || !postBuildControl(*list) || !reparent(*list,id,false,0,error))
    { std::string cleanup; erase(*list,cleanup); return false; }
    mNodes.at(id).combo->list = *list;
    mNodes.at(*list).comboListOwner = id;
    if (params->allowTextEntry)
    {
        Params editorView;
        editorView.name = "Combo Text Entry";
        editorView.rect = {0,0,static_cast<std::int32_t>(width-arrowWidth),static_cast<std::int32_t>(height)};
        editorView.follows = Left | Right | Top | Bottom;
        auto editorParams = params->editor;
        editorParams.text.defaultText.clear();
        editorParams.text.maximumBytes = params->maximumBytes;
        editorParams.commitOnFocusLost = false;
        editorParams.ignoreTab = true;
        editorParams.label = params->label;
        editorParams.keystroke.function = [this,id](Id editor,const LLSD&)
        {
            if (!get(editor) || !get(editor)->lineEditor) return;
            std::string error;
            refreshComboText(id,get(editor)->lineEditor->lastKey,error);
        };
        auto editorControl = params->editorControl;
        editorControl.commit.function = [this,id](Id,const LLSD&)
        {
            const auto* node = get(id);
            if (!node || !node->combo || !get(node->combo->editor)) return;
            const auto text = value(node->combo->editor).asString();
            std::optional<std::size_t> found;
            for (std::size_t index = 0; index < node->combo->items.size(); ++index)
                if (!text.empty() && node->combo->items[index].enabled && foldedLabel(node->combo->items[index].label) == foldedLabel(text)) { found = index; break; }
            std::string error;
            if (!selectComboItem(id,found,error)) return;
            commitCombo(id);
            node = get(id);
            if (!node || !node->combo) return;
            const auto editor = node->combo->editor;
            const auto* child = get(editor);
            if (!child || !child->lineEditor) return;
            const auto validator = child->lineEditor->params.inputPrevalidator;
            const auto display = child->lineEditor->text.display();
            const bool accepted = !validator || validator(display);
            if (accepted && get(editor)) mNodes.at(editor).lineEditor->text.selectAll(error);
        };
        const auto editor = createLineEditor(editorView,editorControl,editorParams,0,error);
        if (!editor) return false;
        if (!get(id) || !postBuildControl(*editor) || !reparent(*editor,id,false,0,error))
        { std::string cleanup; erase(*editor,cleanup); return false; }
        mNodes.at(id).combo->editor = *editor;
        if (!setValue(*editor,LLSD(""))) { error = "Native combo rejected empty initial editor text"; return false; }
    }
    else
    {
        const auto label = utf8str_to_wstring(params->label);
        if (!setButtonLabel(*button,std::u32string(label.begin(),label.end())))
        { error = "Native combo button rejected label"; return false; }
    }
    return true;
}

bool LLVKWidgetTree::selectComboItem(Id id, std::optional<std::size_t> index, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->combo || (index && (*index >= node->combo->items.size() || !node->combo->items[*index].enabled)))
    { error = "Native combo selection is invalid or disabled"; return false; }
    const auto combo = *node->combo;
    if (index)
    {
        const auto label = combo.items[*index].label;
        if (combo.editor)
        {
            if (!setValue(combo.editor,LLSD(label))) { error = "Native combo editor rejected selected label"; return false; }
            setTentative(combo.editor,false);
        }
        else
        {
            const auto wide = utf8str_to_wstring(label);
            if (!setButtonLabel(combo.button,std::u32string(wide.begin(),wide.end())))
            { error = "Native combo button rejected selected label"; return false; }
        }
    }
    mNodes.at(id).combo->dirty = combo.dirty || combo.selected != index;
    mNodes.at(id).combo->selected = index;
    return true;
}

bool LLVKWidgetTree::replaceComboItems(Id id,std::vector<ComboItem> items,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->combo || items.size()>10000)
    { error="Invalid native combo row replacement"; return false; }
    hideComboList(id);
    if (!get(id)) { error="Native combo removed while closing its list"; return false; }
    auto& combo=*mNodes.at(id).combo;
    const auto button=combo.button,editor=combo.editor;
    combo.items=std::move(items); combo.selected.reset(); combo.hovered.reset(); combo.firstRow=0; combo.autocompleted=false;
    if (!setButtonLabel(button,U"")) return false;
    if (editor && !setValue(editor,LLSD(""))) return false;
    return true;
}

bool LLVKWidgetTree::setComboValue(Id id, const LLSD& value, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->combo) { error = "Native combo value owner is missing"; return false; }
    const auto& combo = *node->combo;
    if (combo.selected && combo.items[*combo.selected].value.asString() == value.asString()) return true;
    std::optional<std::size_t> found;
    for (std::size_t index = 0; index < combo.items.size(); ++index)
    {
        const auto& item = combo.items[index];
        if (!item.enabled) continue;
        const bool matches = value.isBinary() ? item.value.isBinary() && item.value.asBinary() == value.asBinary() : item.value.asString() == value.asString();
        if (matches) { found = index; break; }
    }
    return selectComboItem(id,found,error);
}

bool LLVKWidgetTree::commitCombo(Id id)
{
    const auto* node = get(id);
    if (!node || !node->combo) return false;
    const auto selected = node->combo->selected;
    if (selected && node->combo->editor)
    {
        std::string error;
        if (!selectComboItem(id,selected,error)) return false;
    }
    const LLSD committed = value(id);
    writeBoundValue(id,committed);
    if (!get(id)) return true;
    return dispatchControl(id,&LLVKControl::Params::commit);
}

bool LLVKWidgetTree::postBuildCombo(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->combo) { error = "Native combo post-build owner is missing"; return false; }
    const auto& setting = node->control->params.valueSetting;
    return !setting || setComboValue(id,mSettings.at(*setting),error);
}

bool LLVKWidgetTree::showComboList(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->combo) { error = "Native combo popup owner is missing"; return false; }
    const auto combo = *node->combo;
    if (combo.items.empty()) return true;
    Id root = id;
    while (get(root)->parent) root = get(root)->parent;
    const auto rootRect = screenRect(root,error);
    const auto ownerRect = screenRect(id,error);
    if (!rootRect || !ownerRect) return false;
    const auto rootWidth = std::int64_t(rootRect->right)-rootRect->left;
    const auto rootHeight = std::int64_t(rootRect->top)-rootRect->bottom;
    const auto controlWidth = std::int64_t(ownerRect->right)-ownerRect->left;
    const auto controlHeight = std::int64_t(ownerRect->top)-ownerRect->bottom;
    const auto& font = combo.params->listControl.font;
    const double lineHeight = std::ceil(font->metrics().ascender)+std::ceil(font->metrics().descender)+2;
    if (!std::isfinite(lineHeight) || lineHeight <= 0 || lineHeight > INT32_MAX || rootHeight <= 50)
    { error = "Native combo popup has no valid row or root extent"; return false; }
    const auto rowHeight = static_cast<std::int32_t>(lineHeight);
    std::int64_t contentWidth = 0;
    for (const auto& item : combo.items)
    {
        const auto wide = utf8str_to_wstring(item.label);
        const std::u32string text(wide.begin(),wide.end());
        const auto measured = font->measureRun(text,0,text.size(),1.f,true,false,error);
        if (!measured) return false;
        contentWidth = std::max(contentWidth,static_cast<std::int64_t>(std::floor(measured->width+0.5f))+15);
    }
    const auto width = std::clamp(contentWidth,controlWidth,std::max<std::int64_t>(controlWidth,500));
    auto height = std::min(std::int64_t(combo.items.size())*rowHeight+4,rootHeight-50);
    const auto below = std::int64_t(ownerRect->bottom)-rootRect->bottom;
    const auto above = std::int64_t(rootRect->top)-ownerRect->top;
    bool placeAbove = combo.params->listAbove;
    if (height > (placeAbove ? above : below)) placeAbove = above >= below;
    height = std::min(height,std::max<std::int64_t>(0,placeAbove ? above : below));
    if (height <= 4 || width <= 0 || width > rootWidth)
    { error = "Native combo popup cannot fit root bounds"; return false; }
    const auto left = std::clamp<std::int64_t>(ownerRect->left,rootRect->left,std::int64_t(rootRect->right)-width)-ownerRect->left;
    const auto bottom = placeAbove ? controlHeight : -height;
    for (const auto value : {left,bottom,left+width,bottom+height})
        if (value < INT32_MIN || value > INT32_MAX) { error = "Native combo popup geometry overflows"; return false; }
    if (!setShape(combo.list,{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
        static_cast<std::int32_t>(left+width),static_cast<std::int32_t>(bottom+height)},error)) return false;
    if (!requestControlFocus(combo.list,true,error)) return false;
    if (!get(id) || !get(combo.list)) return true;
    if (!setButtonToggle(combo.button,true,error)) return false;
    setVisible(combo.list,true);
    if (!get(id)) return true;
    mNodes.at(id).combo->rowHeight = rowHeight;
    mNodes.at(id).combo->hovered = combo.selected;
    mNodes.at(id).params.useBoundingRect = true;
    return setTopControl(id,error);
}

bool LLVKWidgetTree::hideComboList(Id id)
{
    const auto* node = get(id);
    if (!node || !node->combo) return false;
    const auto combo = *node->combo;
    std::string error;
    setButtonToggle(combo.button,false,error);
    setVisible(combo.list,false);
    if (!get(id)) return true;
    mNodes.at(id).combo->hovered.reset();
    mNodes.at(id).params.useBoundingRect = false;
    if (mTopControl == id) mTopControl = 0;
    return true;
}

bool LLVKWidgetTree::comboListPointer(Id list, const PointerEvent& event, std::string& error)
{
    const Id id = get(list)->comboListOwner;
    const auto* node = get(id);
    if (!node || !node->combo) { error = "Native combo list lost its owner"; return false; }
    const auto combo = *node->combo;
    const auto& rect = get(list)->params.rect;
    const auto width = std::int64_t(rect.right)-rect.left, height = std::int64_t(rect.top)-rect.bottom;
    const bool inside = event.x >= 2 && event.x < width-2 && event.y >= 2 && event.y < height-2;
    std::optional<std::size_t> hit;
    if (inside && combo.rowHeight > 0)
    {
        const auto index = combo.firstRow+static_cast<std::size_t>((height-3-event.y)/combo.rowHeight);
        if (index < combo.items.size() && combo.items[index].enabled) hit = index;
    }
    if (event.kind == PointerKind::Hover)
    {
        mNodes.at(id).combo->hovered = hit;
        cursorEffect(list,false);
        return true;
    }
    if (event.kind == PointerKind::LeftDown)
    {
        if (!setMouseCapture(list,error)) return false;
        if (get(id)) mNodes.at(id).combo->hovered = hit;
        return true;
    }
    if (event.kind == PointerKind::LeftUp)
    {
        if (mMouseCapture == list && !setMouseCapture(0,error)) return false;
        if (!get(id)) return true;
        if (!inside) return true;
        if (hit && !selectComboItem(id,hit,error)) return false;
        if (combo.editor && get(combo.editor))
        {
            if (!setKeyboardFocus(combo.editor,false,false,error)) return false;
            if (!get(id) || !get(combo.editor)) return true;
            const auto validator = get(combo.editor)->lineEditor->params.inputPrevalidator;
            const auto text = get(combo.editor)->lineEditor->text.display();
            const bool accepted = !validator || validator(text);
            if (accepted && get(combo.editor)) mNodes.at(combo.editor).lineEditor->text.selectAll(error);
        }
        if (!get(id)) return true;
        hideComboList(id);
        return commitCombo(id);
    }
    return false;
}

bool LLVKWidgetTree::refreshComboText(Id id, std::optional<LLVKLineEditor::Key> key, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->combo || !get(node->combo->editor)) { error = "Native combo text editor is missing"; return false; }
    const auto params = node->combo->params;
    const Id editor = node->combo->editor;
    const auto entry = params->textEntry;
    if (entry.function) entry.function(editor,entry.parameter.value_or(LLSD()));
    if (!get(id) || !get(editor)) return true;
    using Key = LLVKLineEditor::Key;
    if (key == Key::Left || key == Key::Right) return true;
    const bool deleting = key == Key::Backspace || key == Key::Delete;
    const auto snapshot = get(editor)->lineEditor->text;
    const auto prefix = snapshot.display().substr(0,snapshot.cursor());
    const auto userText = get(id)->combo->autocompleted ? prefix : snapshot.display();
    const auto lowerWide = [](std::u32string value)
    {
        for (auto& character : value)
            if (character <= WCHAR_MAX) character = static_cast<char32_t>(std::towlower(static_cast<wint_t>(character)));
        return value;
    };
    if (!deleting && snapshot.display().size() == 1 && params->prearrange.function)
        params->prearrange.function(id,params->prearrange.parameter.value_or(LLSD(snapshot.text())));
    if (!get(id) || !get(editor)) return true;
    const auto items = get(id)->combo->items;
    std::optional<std::size_t> selected;
    const auto exact = foldedLabel(snapshot.text());
    if (!exact.empty())
        for (std::size_t index = 0; index < items.size(); ++index)
            if (foldedLabel(items[index].label) == exact) { selected = index; break; }
    bool completed = false;
    auto text = get(editor)->lineEditor->text;
    if (!selected && !deleting)
    {
        const auto setting = mSettings.find("FSComboboxSubstringSearch");
        const bool substring = !params->forceDisableSubstring && setting != mSettings.end() && setting->second.asBoolean();
        const auto target = lowerWide(prefix);
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            if (!items[index].enabled) continue;
            const auto wide = utf8str_to_wstring(items[index].label);
            const std::u32string label(wide.begin(),wide.end());
            auto searchable = lowerWide(label);
            if (!target.empty())
            {
                const auto space = [](char32_t character) { return character <= WCHAR_MAX && std::iswspace(static_cast<wint_t>(character)) != 0; };
                while (!searchable.empty() && space(searchable.front())) searchable.erase(searchable.begin());
                while (!searchable.empty() && space(searchable.back())) searchable.pop_back();
            }
            const bool matches = target.empty() ? label.empty() : substring ? searchable.find(target) != std::u32string::npos : searchable.starts_with(target);
            if (!matches) continue;
            selected = index;
            std::u32string completion = prefix;
            if (substring) completion += U" ("+label+U")";
            else if (prefix.size() <= label.size()) completion += label.substr(prefix.size());
            const auto encoded = wstring_to_utf8str(LLWString(completion.begin(),completion.end()));
            if (!text.assign(encoded,true,true,mLabelContext,error) || !text.setSelection(prefix.size(),text.display().size(),error)) return false;
            text.endSelection();
            completed = true;
            break;
        }
        if (!selected)
        {
            const auto encoded = wstring_to_utf8str(LLWString(userText.begin(),userText.end()));
            if (!text.assign(encoded,true,true,mLabelContext,error)) return false;
        }
    }
    auto& combo = *mNodes.at(id).combo;
    combo.dirty = combo.dirty || combo.selected != selected;
    combo.selected = selected;
    if (completed) combo.autocompleted = true;
    else if (!selected && !deleting) combo.autocompleted = false;
    mNodes.at(editor).lineEditor->text = std::move(text);
    mNodes.at(editor).control->value = LLSD(mNodes.at(editor).lineEditor->text.text());
    mNodes.at(editor).control->tentative = !selected && params->tentativeText;
    const auto changed = params->textChanged;
    if (changed.function) changed.function(editor,changed.parameter.value_or(LLSD()));
    return true;
}
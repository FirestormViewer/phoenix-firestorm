#include "llvkwidgettree.h"

#include <algorithm>
#include <cmath>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createCheckBox(const Params& inputView,
    const LLVKControl::Params& inputControl, const CheckBoxConstruction& construction, Id parent, std::string& error)
{
    error.clear();
    if (construction.wrap != CheckBoxWrap::None && construction.wrap != CheckBoxWrap::Up && construction.wrap != CheckBoxWrap::Down)
    { error = "Invalid native checkbox wrapping mode"; return std::nullopt; }
    auto view = inputView;
    view.useBoundingRect = true;
    auto control = inputControl;
    if (!control.initialValue) control.initialValue = construction.initialValue;
    CheckBox checkbox;
    checkbox.construction = construction;
    const auto padding = mSettings.find("UICheckboxctrlHPad");
    if (padding != mSettings.end()) checkbox.construction.horizontalPadding = padding->second.asInteger();
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::move(checkbox));
}

bool LLVKWidgetTree::constructCheckBoxChildren(Id id, std::string& error)
{
    const auto construction = get(id)->checkBox->construction;
    auto labelControl = construction.labelControl;
    labelControl.initialValue = construction.label.empty() ? " " : construction.label;
    if (construction.fontProvided) labelControl.font = get(id)->control->params.font;
    auto labelParams = construction.labelText;
    auto labelView = construction.labelView;
    if (construction.wrap != CheckBoxWrap::None)
    {
        labelParams.layout.wrap = true;
        const auto width = std::int64_t(get(id)->params.rect.right)-get(id)->params.rect.left -
            (std::int64_t(construction.buttonView.rect.right)-construction.buttonView.rect.left)-construction.horizontalPadding;
        const auto right = std::int64_t(labelView.rect.left)+width;
        if (right < INT32_MIN || right > INT32_MAX)
        { error = "Native checkbox label width overflow"; return false; }
        labelView.rect.right = static_cast<std::int32_t>(right);
    }
    const auto label = createPlainText(labelView,labelControl,labelParams,0,error);
    if (!label) return false;
    const auto discardLabel = [&] { std::string cleanup; erase(*label,cleanup); };
    if (!get(id) || !postBuildControl(*label) || !fitPlainText(*label,error))
    { discardLabel(); if (error.empty()) error = "Native checkbox label lost its owner"; return false; }
    auto labelRect = get(*label)->params.rect;
    if (!reflowPlainText(*label,error)) { discardLabel(); return false; }
    if (construction.wrap == CheckBoxWrap::Down && get(*label)->plainText->layout->lines.size() > 1)
    {
        const auto& metrics = labelControl.font->metrics();
        const float lineHeight = std::ceil(metrics.ascender/labelParams.layout.scaleY)+std::ceil(metrics.descender/labelParams.layout.scaleY);
        const double delta = std::floor(lineHeight*labelParams.layout.spacingMultiple+0.5f)-
            (std::int64_t(labelRect.top)-labelRect.bottom);
        if (labelRect.bottom+delta < INT32_MIN || labelRect.bottom+delta > INT32_MAX ||
            labelRect.top+delta < INT32_MIN || labelRect.top+delta > INT32_MAX)
        { discardLabel(); error = "Native checkbox wrap translation overflow"; return false; }
        labelRect.bottom = static_cast<std::int32_t>(labelRect.bottom+delta);
        labelRect.top = static_cast<std::int32_t>(labelRect.top+delta);
        mNodes.at(*label).params.rect = labelRect;
    }
    if (!reparent(*label,id,false,0,error)) { discardLabel(); return false; }
    mNodes.at(id).checkBox->label = *label;
    auto buttonView = construction.buttonView;
    const auto bottom = std::min(buttonView.rect.bottom,labelRect.bottom);
    const auto width = std::max<std::int64_t>(buttonView.rect.right,std::int64_t(labelRect.right)-buttonView.rect.left);
    const auto height = std::max<std::int64_t>(std::int64_t(labelRect.top)-labelRect.bottom,buttonView.rect.top);
    const auto right = std::int64_t(buttonView.rect.left)+width;
    const auto top = std::int64_t(bottom)+height;
    if (right < INT32_MIN || right > INT32_MAX || top < INT32_MIN || top > INT32_MAX)
    { error = "Native checkbox click rectangle overflow"; return false; }
    buttonView.rect = {buttonView.rect.left,bottom,static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
    buttonView.follows = Left | Bottom;
    auto buttonControl = construction.buttonControl;
    buttonControl.initialValue = construction.initialValue;
    auto buttonParams = construction.button;
    buttonParams.commitOnReturn = false;
    buttonParams.click = LLVKControl::Callback{[this,id](Id,const LLSD&) { commitCheckBox(id); },std::nullopt};
    const auto button = createButton(buttonView,buttonControl,buttonParams,0,error);
    if (!button) return false;
    if (!get(id) || !postBuildButton(*button,error) || !reparent(*button,id,false,0,error))
    {
        std::string cleanup;
        erase(*button,cleanup);
        if (error.empty()) error = "Native checkbox button lost its owner";
        return false;
    }
    mNodes.at(id).checkBox->button = *button;
    return true;
}

LLSD LLVKWidgetTree::value(Id id) const
{
    const auto* node = get(id);
    if (node && node->radioGroup)
    {
        const auto& radio=*node->radioGroup;
        return radio.selected>=0 && static_cast<std::size_t>(radio.selected)<radio.items.size() ? radio.items[radio.selected].payload : LLSD();
    }
    if (!node || !node->control) return LLSD();
    if (node->combo)
    {
        const auto& combo = *node->combo;
        if (combo.selected && *combo.selected < combo.items.size()) return combo.items[*combo.selected].value;
        return combo.editor ? value(combo.editor) : LLSD();
    }
    return node->checkBox ? value(node->checkBox->button) : node->control->value;
}

bool LLVKWidgetTree::dirty(Id id) const
{
    const auto* node = get(id);
    if (node && node->lineEditor) return node->lineEditor->text.dirty();
    if (node && node->combo) return node->combo->dirty;
    return node && node->control && (node->checkBox ? dirty(node->checkBox->button) : node->control->dirty);
}

bool LLVKWidgetTree::commitCheckBox(Id id)
{
    const auto* node = get(id);
    if (!node || !node->checkBox) return false;
    if (!node->params.enabled) return true;
    setTentative(id,false);
    writeBoundValue(id,value(id));
    return dispatchControl(id,&LLVKControl::Params::commit);
}

bool LLVKWidgetTree::refreshCheckBox(Id id)
{
    const auto* node = get(id);
    if (!node || !node->checkBox) return false;
    const auto callback = node->checkBox->construction.onCheck;
    if (callback.function)
    {
        const bool checked = callback.function(id,callback.parameter.value_or(LLSD()));
        if (!get(id)) return true;
        if (value(id).asBoolean() != checked) setValue(id,LLSD(checked));
    }
    return true;
}

bool LLVKWidgetTree::setTentative(Id id, bool tentative)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.control) return false;
    if (found->second.checkBox) return setTentative(found->second.checkBox->button,tentative);
    found->second.control->tentative = tentative;
    return true;
}

bool LLVKWidgetTree::tentative(Id id) const
{
    const auto* node = get(id);
    return node && node->control && (node->checkBox ? tentative(node->checkBox->button) : node->control->tentative);
}

bool LLVKWidgetTree::planCheckBoxReshape(Id id, std::int64_t width,
    ShapeChanges& changes, std::string& error) const
{
    const auto& checkbox = *get(id)->checkBox;
    const auto* label = get(checkbox.label);
    const auto* button = get(checkbox.button);
    if (!label || !label->plainText || !button)
    { error = "Native checkbox children no longer exist"; return false; }
    auto labelRect = label->params.rect;
    const auto available = width-labelRect.left;
    const auto oldHeight = std::int64_t(labelRect.top)-labelRect.bottom;
    if (available < 0 || available > INT32_MAX || oldHeight < 0 || oldHeight > INT32_MAX)
    { error = "Native checkbox label reshape dimensions invalid"; return false; }
    auto options = label->plainText->params.layout;
    options.width = static_cast<std::int32_t>(available);
    auto layout = LLVKPlainTextLayout::document(label->plainText->text,*label->control->params.font,options,
        static_cast<std::int32_t>(oldHeight),label->plainText->params.verticalPadding,label->plainText->params.vertical,error);
    if (!layout) return false;
    const auto left = labelRect.left;
    const auto bottom = checkbox.construction.wrap == CheckBoxWrap::Down ?
        std::int64_t(labelRect.top)-layout->fitHeight : std::int64_t(labelRect.bottom);
    const auto right = std::int64_t(left)+layout->fitWidth;
    const auto top = bottom+layout->fitHeight;
    if (bottom < INT32_MIN || bottom > INT32_MAX || right < INT32_MIN || right > INT32_MAX || top < INT32_MIN || top > INT32_MAX)
    { error = "Native checkbox label reshape overflow"; return false; }
    labelRect = {left,static_cast<std::int32_t>(bottom),static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
    if (!planReshape(checkbox.label,layout->fitWidth,layout->fitHeight,labelRect,changes,error)) return false;
    auto buttonRect = button->params.rect;
    const auto buttonBottom = std::min(buttonRect.bottom,labelRect.bottom);
    const auto buttonWidth = std::max(std::int64_t(buttonRect.right)-buttonRect.left,std::int64_t(labelRect.right)-buttonRect.left);
    const auto buttonHeight = std::max(std::int64_t(labelRect.top)-buttonBottom,std::int64_t(buttonRect.top)-buttonRect.bottom);
    buttonRect.bottom = buttonBottom;
    return planReshape(checkbox.button,buttonWidth,buttonHeight,buttonRect,changes,error);
}

bool LLVKWidgetTree::setCheckBoxLabel(Id id, std::string label, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->checkBox) { error = "Native label target is not a checkbox"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width < INT32_MIN || width > INT32_MAX || height < INT32_MIN || height > INT32_MAX)
    { error = "Native checkbox dimensions overflow"; return false; }
    return setPlainText(node->checkBox->label,std::move(label),error) &&
        reshape(id,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height),error);
}

bool LLVKWidgetTree::setCheckBoxLabelArgument(Id id, std::string key, std::string replacement, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->checkBox) { error = "Native argument target is not a checkbox"; return false; }
    if (!setPlainTextArgument(node->checkBox->label,std::move(key),std::move(replacement),error)) return false;
    const auto& source = get(node->checkBox->label)->plainText->source.original();
    return setCheckBoxLabel(id,source,error);
}
#include "llvkwidgettree.h"
#include "llvkfont.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createButton(const Params& inputView,
    const LLVKControl::Params& inputControl, const LLVKButton::Params& inputButton,
    Id parent, std::string& error, std::optional<BadgeConstruction> badge)
{
    error.clear();
    const auto view = inputView;
    auto control = inputControl;
    const auto params = inputButton;
    if (!control.font || !std::isfinite(params.scaleX) || params.scaleX <= 0.f ||
        !std::isfinite(params.heldSeconds) || params.heldSeconds < 0.0)
    { error = "Invalid native button font, scale or held duration"; return std::nullopt; }
    for (auto alignment : {params.overlayAlign,params.labelAlign})
        if (alignment != LLVKButton::Align::Left && alignment != LLVKButton::Align::Center && alignment != LLVKButton::Align::Right)
        { error = "Invalid native button alignment"; return std::nullopt; }
    if (!std::isfinite(params.hoverGlow))
    { error = "Nonfinite native button hover glow"; return std::nullopt; }
    for (const auto& color : {params.labelColor,params.selectedLabelColor,params.disabledLabelColor,params.disabledSelectedLabelColor,
        params.imageColor,params.disabledImageColor,params.flashColor,params.alternateFlashColor,
        params.overlayColor,params.disabledOverlayColor,params.selectedOverlayColor})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native button color"; return std::nullopt; }
    LLVKButton button;
    button.params = params;
    if (!std::isfinite(params.flashPeriod))
    { error = "Nonfinite native button flash period"; return std::nullopt; }
    if (params.flashEnable)
    {
        const auto countSetting = mSettings.find("FlashCount");
        const auto periodSetting = mSettings.find("FlashPeriod");
        const auto count = params.flashCount > 0 ? params.flashCount :
                           countSetting == mSettings.end() ? 8 : countSetting->second.asInteger();
        const auto period = params.flashPeriod > 0.f ? params.flashPeriod :
                            periodSetting == mSettings.end() ? 0.5f : static_cast<float>(periodSetting->second.asReal());
        if (!std::isfinite(period))
        { error = "Nonfinite native flash period setting"; return std::nullopt; }
        button.flashTimer.emplace();
        button.flashTimer->limit = 2 * static_cast<std::uint64_t>(std::max(count,0));
        button.flashTimer->period = period;
    }
    button.labelSource.assign(params.label);
    button.selectedLabelSource.assign(params.selectedLabel.value_or(params.label));
    button.params.label = button.labelSource.resolveWide(mLabelContext);
    button.selectedLabel = button.selectedLabelSource.resolveWide(mLabelContext);
    button.images = params.images;
    button.leftPad = params.leftPad;
    button.rightPad = params.rightPad;
    const auto width = std::int64_t(view.rect.right)-view.rect.left;
    if (view.rect.right >= 0 && width > 0)
    {
        auto space = control.font->measureRun(U" ",0,1,params.scaleX,true,false,error);
        if (!space) return std::nullopt;
        if (width-std::int64_t(params.leftPad)-params.rightPad < std::floor(space->width+0.5f))
            button.leftPad = button.rightPad = params.originalHorizontalPad;
    }
    if (params.images.unselected != params.defaultImages.unselected)
    {
        if (params.images.disabled == params.defaultImages.disabled)
        {
            button.images.disabled = params.images.unselected;
            button.fadeWhenDisabled = true;
        }
        if (params.images.pressedSelected == params.defaultImages.pressedSelected)
            button.images.pressedSelected = params.images.unselected;
    }
    if (params.images.selected != params.defaultImages.selected)
    {
        if (params.images.disabledSelected == params.defaultImages.disabledSelected)
        {
            button.images.disabledSelected = params.images.selected;
            button.fadeWhenDisabled = true;
        }
        if (params.images.pressed == params.defaultImages.pressed)
            button.images.pressed = params.images.selected;
    }
    if (!params.pressedProvided) button.images.pressed = params.images.selected;
    if (!params.pressedSelectedProvided) button.images.pressedSelected = params.images.unselected;
    if (!control.initialValue) control.initialValue = LLSD(false);
    return createControlImpl(view,control,std::nullopt,parent,error,std::move(button),std::nullopt,std::move(badge));
}

bool LLVKWidgetTree::resizeButton(Id id, std::string& error)
{
    const auto* node = get(id);
    if (!node || !node->button || !node->control) { error = "Native resize target is not a button"; return false; }
    const auto& button = *node->button;
    const auto& label = node->control->value.asBoolean() ? button.selectedLabel : button.params.label;
    auto measured = node->control->params.font->measureRun(label,0,label.size(),button.params.scaleX,true,false,error);
    if (!measured) return false;
    if (!button.params.autoResize) return true;
    const double labelWidth = std::floor(measured->width+0.5f);
    const auto horizontalPadding = std::int64_t(button.leftPad)+button.rightPad;
    double minimumWidth = labelWidth + horizontalPadding;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (button.images.overlay)
    {
        const auto available = height-std::int64_t(button.params.overlayBottomPad)-button.params.overlayTopPad;
        const float scale = static_cast<float>(available) / button.images.overlay->height();
        const double overlayWidth = std::floor(button.images.overlay->width()*scale+0.5f);
        if (button.params.overlayAlign == LLVKButton::Align::Center)
            minimumWidth = std::max(minimumWidth,overlayWidth+horizontalPadding);
        else minimumWidth += overlayWidth+button.params.overlayLabelSpace;
    }
    if (!std::isfinite(minimumWidth) || minimumWidth < INT32_MIN || minimumWidth > INT32_MAX)
    { error = "Native button resize overflow"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    return width >= minimumWidth || reshape(id,static_cast<std::int32_t>(minimumWidth),static_cast<std::int32_t>(height),error);
}

bool LLVKWidgetTree::postBuildButton(Id id, std::string& error)
{
    error.clear();
    if (!resizeButton(id,error)) return false;
    auto& button = *mNodes.at(id).button;
    if (button.badge)
    {
        const bool attached = attachBadgeToHolder(button.badge,error);
        if (!error.empty()) return false;
        if (attached) button.hasBadgeHolderParent = true;
    }
    return postBuildControl(id);
}

bool LLVKWidgetTree::setButtonBadgeLabel(Id id, std::u32string label, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->button) { error = "Native badge owner is not a button"; return false; }
    Id badge = node->button->badge;
    if (!get(badge))
    {
        if (!node->badgeConstruction) { error = "Native badge defaults have not been resolved"; return false; }
        const auto construction = *node->badgeConstruction;
        const auto created = createBadge(construction.view,construction.control,construction.defaults,id,0,error);
        if (!created) return false;
        if (!postBuildControl(*created) || !get(id))
        {
            std::string cleanup;
            erase(*created,cleanup);
            error = "Native badge construction destroyed its owner";
            return false;
        }
        badge = *created;
        mNodes.at(id).button->badge = badge;
        const bool attached = attachBadgeToHolder(badge,error);
        if (!error.empty()) return false;
        if (attached) mNodes.at(id).button->hasBadgeHolderParent = true;
    }
    setBadgeLabel(badge,std::move(label));
    const auto* current = get(badge);
    return !current->parent || reparent(badge,current->parent,false,current->params.tabGroup.value_or(0),error);
}

bool LLVKWidgetTree::setButtonBadgeVisible(Id id, bool visible)
{
    const auto* node = get(id);
    return node && node->button && setVisible(node->button->badge,visible);
}

bool LLVKWidgetTree::setButtonToggle(Id id, bool selected, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->button || !node->control) { error = "Native toggle target is not a button"; return false; }
    if (node->control->value.asBoolean() == selected) return true;
    writeBoundValue(id,LLSD(selected));
    setValue(id,LLSD(selected));
    auto& button = *mNodes.at(id).button;
    setButtonFlashing(id,false);
    ++button.textGeneration;
    return resizeButton(id,error);
}

bool LLVKWidgetTree::setButtonLabel(Id id, std::u32string label, std::optional<bool> selectedOnly)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.button) return false;
    auto button = *found->second.button;
    if (!selectedOnly || *selectedOnly)
    {
        button.selectedLabelSource.assign(label);
        button.selectedLabel = button.selectedLabelSource.resolveWide(mLabelContext);
    }
    if (!selectedOnly || !*selectedOnly)
    {
        button.labelSource.assign(std::move(label));
        button.params.label = button.labelSource.resolveWide(mLabelContext);
    }
    ++button.textGeneration;
    found->second.button = std::move(button);
    return true;
}

bool LLVKWidgetTree::setButtonLabelArgument(Id id, std::string key, std::string replacement)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.button) return false;
    auto button = *found->second.button;
    button.labelSource.setArgument(key,replacement);
    button.selectedLabelSource.setArgument(std::move(key),std::move(replacement));
    button.params.label = button.labelSource.resolveWide(mLabelContext);
    button.selectedLabel = button.selectedLabelSource.resolveWide(mLabelContext);
    ++button.textGeneration;
    found->second.button = std::move(button);
    return true;
}

void LLVKWidgetTree::setLabelContext(LLVKLabel::Context context)
{
    struct Resolved
    {
        Id id;
        std::u32string label, selected;
        std::optional<LLVKPlainControl> text;
        LLSD value;
    };
    std::vector<Resolved> updates;
    for (const auto& [id,node] : mNodes)
    {
        if (node.button)
            updates.push_back({id,node.button->labelSource.resolveWide(context),node.button->selectedLabelSource.resolveWide(context),{},LLSD()});
        else if (node.badge)
            updates.push_back({id,node.badge->labelSource.resolveWide(context),{},{},LLSD()});
        else if (node.plainText)
        {
            std::string error;
            auto text = resolvePlainText(*node.plainText,node.plainText->source,context,error);
            if (!text) throw std::invalid_argument(error);
            LLSD value(text->value);
            updates.push_back({id,{},{},std::move(text),std::move(value)});
        }
    }
    mLabelContext = std::move(context);
    for (auto& update : updates)
    {
        auto& node = mNodes.at(update.id);
        if (node.button)
        {
            node.button->params.label = std::move(update.label);
            node.button->selectedLabel = std::move(update.selected);
            ++node.button->textGeneration;
        }
        else if (node.badge) node.badge->params.label = std::move(update.label);
        else
        {
            node.plainText = std::move(update.text);
            node.control->value = std::move(update.value);
            node.control->dirty = true;
        }
    }
}

bool LLVKWidgetTree::setButtonFlashing(Id id, bool flashing, bool force, bool alternateColor)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.button) return false;
    auto& button = *found->second.button;
    button.forceFlashing = force;
    button.alternateFlashColor = alternateColor;
    if (button.flashTimer)
    {
        button.flashTimer->running = flashing;
        button.flashTimer->highlighted = flashing;
        if (flashing) button.flashTimer->resetTime = mTime;
        else button.flashTimer->ticks = 0;
    }
    else if (button.flashing != flashing) button.flashResetTime = mTime;
    button.flashing = flashing;
    return true;
}

bool LLVKWidgetTree::advanceTime(double time, std::string& error)
{
    error.clear();
    if (!std::isfinite(time) || time < mTime)
    { error = "Native UI clock must be finite and monotonic"; return false; }
    mTime = time;
    for (auto& [id,node] : mNodes)
    {
        if (!node.button || !node.button->flashTimer) continue;
        auto& timer = *node.button->flashTimer;
        if (!timer.running || static_cast<float>(mTime-timer.resetTime) <= timer.period) continue;
        timer.resetTime = mTime;
        timer.highlighted = !timer.highlighted;
        if (++timer.ticks >= timer.limit)
        {
            timer.running = false;
            timer.highlighted = false;
            timer.ticks = 0;
        }
    }
    return true;
}

void LLVKWidgetTree::updateFlashSettings()
{
    const auto countSetting = mSettings.find("FlashCount");
    const auto periodSetting = mSettings.find("FlashPeriod");
    const auto count = countSetting == mSettings.end() ? 8 : countSetting->second.asInteger();
    const auto period = periodSetting == mSettings.end() ? 0.5f : static_cast<float>(periodSetting->second.asReal());
    for (auto& [id,node] : mNodes)
    {
        if (!node.button || !node.button->flashTimer) continue;
        auto& timer = *node.button->flashTimer;
        timer.running = false;
        timer.highlighted = false;
        timer.ticks = 0;
        timer.limit = 2 * static_cast<std::uint64_t>(std::max(count,0));
        timer.period = std::max(period,0.f);
    }
}

bool LLVKWidgetTree::buttonCallback(Id id, LLVKControl::Callback LLVKButton::Params::* event, const LLSD& value)
{
    const auto* node = get(id);
    if (!node || !node->button) return false;
    const auto callback = node->button->params.*event;
    const LLSD argument = callback.parameter.value_or(value);
    if (callback.function) callback.function(id,argument);
    return get(id) != nullptr;
}

void LLVKWidgetTree::buttonSound(Id id, bool release)
{
    const auto* node = get(id);
    const auto events = mEvents.find(id);
    if (!node || !(node->params.soundFlags & (release ? 2 : 1)) || events == mEvents.end()) return;
    const auto callback = events->second.sound;
    if (callback) callback(id,release);
}

bool LLVKWidgetTree::buttonCommitSignal(Id id)
{
    const auto* node = get(id);
    if (!node || !node->button || !node->control) return false;
    const auto click = node->button->params.click;
    const auto commit = node->control->params.commit;
    const LLSD value = node->control->value;
    if (click && click->function)
    {
        const LLSD argument = click->parameter.value_or(value);
        click->function(id,argument);
    }
    if (get(id) && commit.function)
    {
        const LLSD argument = commit.parameter.value_or(value);
        commit.function(id,argument);
    }
    return true;
}

bool LLVKWidgetTree::activateButton(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->button) { error = "Native activation target is not a button"; return false; }
    if (!buttonCallback(id,&LLVKButton::Params::mouseDown,LLSD())) return true;
    if (!buttonCallback(id,&LLVKButton::Params::mouseUp,LLSD())) return true;
    buttonSound(id,false);
    if (!get(id)) return true;
    buttonSound(id,true);
    node = get(id);
    if (!node) return true;
    if (node->button->params.toggle && !setButtonToggle(id,!node->control->value.asBoolean(),error)) return false;
    return buttonCommitSignal(id);
}

bool LLVKWidgetTree::buttonUnicode(Id id, char32_t character, bool repeated, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->button) { error = "Native Unicode target is not a button"; return false; }
    if (character != U' ' || repeated) return false;
    if (node->button->params.toggle && !setButtonToggle(id,!node->control->value.asBoolean(),error)) return false;
    return buttonCommitSignal(id);
}

bool LLVKWidgetTree::buttonReturn(Id id, std::uint32_t modifiers, bool repeated, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->button) { error = "Native Return target is not a button"; return false; }
    if (!node->button->params.commitOnReturn || modifiers || repeated) return false;
    if (node->button->params.toggle && !setButtonToggle(id,!node->control->value.asBoolean(),error)) return false;
    return buttonCommitSignal(id);
}
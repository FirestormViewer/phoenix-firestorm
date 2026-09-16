#include "llvkwidgettree.h"
#include "llvkmenu.h"

#include <cmath>

bool LLVKWidgetTree::setMenu(Id id,std::shared_ptr<LLVKMenu> menu)
{
    if (!get(id)) return false;
    mNodes.at(id).menu=std::move(menu);
    return true;
}

bool LLVKWidgetTree::routeWheel(Id root, std::int32_t x, std::int32_t y, std::int32_t clicks,
    bool horizontal, std::string& error)
{
    error.clear();
    const auto rectangle = screenRect(root,error);
    if (!rectangle) return false;
    const auto localX = std::int64_t(x)-rectangle->left, localY = std::int64_t(y)-rectangle->bottom;
    if (localX < INT32_MIN || localX > INT32_MAX || localY < INT32_MIN || localY > INT32_MAX)
    { error = "Native wheel coordinate conversion overflows"; return false; }
    return handleWheel(root,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),clicks,horizontal,error);
}

bool LLVKWidgetTree::handleWheel(Id id, std::int32_t x, std::int32_t y, std::int32_t clicks,
    bool horizontal, std::string& error)
{
    const auto* node = get(id);
    if (!node) return false;
    if (node->slider && node->slider->params->vertical) return sliderStep(id,-clicks,error);
    if (node->scrollbar) return scrollbarWheel(id,clicks,horizontal,error);
    if (node->scrollList)
    {
        const auto state=*node->scrollList;
        const bool changed=scrollbarWheel(state.scrollbar,clicks,horizontal,error);
        return error.empty() && (changed || state.params->wheelOpaque);
    }
    if (node->tabContainer && node->tabContainer->layout && !node->tabContainer->layout->hidden &&
        !node->tabContainer->tabs.empty())
    {
        const auto& layout=*node->tabContainer->layout;
        const auto left=layout.verticalPadding+3;
        const auto height=node->params.rect.top-node->params.rect.bottom;
        const bool vertical=layout.position==Node::TabContainer::Layout::Position::Left;
        const auto width=node->params.rect.right-node->params.rect.left;
        const auto bottom=layout.position==Node::TabContainer::Layout::Position::Top ? height-layout.tabHeight : 1;
        if (vertical ? x>=left && x<left+layout.minimumWidth && y>=0 && y<height :
            x>=2 && x<width-layout.rightPadding-2 && y>=bottom && y<bottom+layout.tabHeight)
            return scrollTabStrip(id,clicks,error);
    }
    const auto children = node->children;
    for (const Id child : children)
    {
        const auto* current = get(child);
        if (!current || current->parent != id || !current->params.visible || !current->params.enabled) continue;
        const auto localX = std::int64_t(x)-current->params.rect.left, localY = std::int64_t(y)-current->params.rect.bottom;
        if (localX < INT32_MIN || localX > INT32_MAX || localY < INT32_MIN || localY > INT32_MAX)
        { error = "Native child wheel coordinates overflow"; return false; }
        const auto inside = containsLocal(child,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),true,mTopControl,error);
        if (!inside) return false;
        if (*inside && handleWheel(child,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),clicks,horizontal,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
    }
    node = get(id);
    if (!node || !node->scrollContainer) return false;
    const auto state = *node->scrollContainer;
    const auto* vertical = get(state.vertical);
    if (!horizontal && vertical && vertical->params.visible && vertical->params.enabled)
    {
        const bool changed = scrollbarWheel(state.vertical,clicks,false,error);
        if (!error.empty()) return false;
        if (changed && get(id) && !updateScrollContainer(id,error)) return false;
        return true;
    }
    const auto* horizontalBar = get(state.horizontal);
    if (horizontalBar && horizontalBar->params.visible && horizontalBar->params.enabled)
    {
        const bool changed = scrollbarWheel(state.horizontal,clicks,horizontal,error);
        if (!error.empty()) return false;
        if (changed && get(id) && !updateScrollContainer(id,error)) return false;
        return changed;
    }
    return false;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::tooltipAt(Id root,std::int32_t x,std::int32_t y,std::string& error) const
{
    error.clear();
    if (!get(root)) { error="Invalid native tooltip root"; return std::nullopt; }
    Id target=0;
    const auto visit=[&](auto&& self,Id id) -> bool
    {
        const auto* node=get(id);
        if (!node || !node->params.visible) return false;
        const auto rect=screenRect(id,error);
        if (!rect) return false;
        const auto localX=std::int64_t(x)-rect->left,localY=std::int64_t(y)-rect->bottom;
        if (localX<INT32_MIN || localX>INT32_MAX || localY<INT32_MIN || localY>INT32_MAX)
        { error="Native tooltip coordinate conversion overflows"; return false; }
        const auto inside=containsLocal(id,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),true,0,error);
        if (!inside || !*inside) return false;
        const bool own=!node->params.tooltip.empty();
        if (own) target=id;
        for (const auto child : node->children)
        {
            if (self(self,child)) return true;
            if (!error.empty()) return false;
        }
        return own || node->params.mouseOpaque;
    };
    visit(visit,root);
    return error.empty() ? std::optional<Id>(target) : std::nullopt;
}

bool LLVKWidgetTree::updatePointerHover(Id root,const PointerEvent& screenEvent,std::string& error)
{
    error.clear();
    if (!get(root) || !std::isfinite(screenEvent.time) || screenEvent.time<0)
    { error="Invalid native hover root or event time"; return false; }
    std::set<Id> hovered;
    const auto visit=[&](auto&& self,Id id,bool occlude) -> bool
    {
        const auto* node=get(id);
        if (!node || !node->params.visible) return false;
        const auto rect=screenRect(id,error);
        if (!rect) return false;
        const auto localX=std::int64_t(screenEvent.x)-rect->left,localY=std::int64_t(screenEvent.y)-rect->bottom;
        if (localX<INT32_MIN || localX>INT32_MAX || localY<INT32_MIN || localY>INT32_MAX)
        { error="Native hover coordinate conversion overflows"; return false; }
        const auto inside=containsLocal(id,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),true,0,error);
        if (!inside || !*inside) return false;
        hovered.insert(id);
        const bool opaque=node->params.mouseOpaque;
        const auto children=node->children;
        for (const auto child : children)
        {
            const bool blocked=self(self,child,occlude);
            if (!error.empty()) return false;
            if (blocked && occlude) return true;
        }
        return opaque;
    };
    const auto target=mMouseCapture ? mMouseCapture : mTopControl;
    bool exclusive=false;
    if (target && get(target))
    {
        visit(visit,target,false);
        exclusive=hovered.contains(target);
    }
    if (!exclusive) visit(visit,root,true);
    if (!error.empty()) return false;
    const auto previous=mPointerHover;
    mPointerHover=hovered;
    for (const auto id : hovered) if (!previous.contains(id) && get(id)) mouseEnter(id);
    for (const auto id : previous) if (!hovered.contains(id) && get(id)) mouseLeave(id);
    return true;
}

bool LLVKWidgetTree::routePointer(Id root, const PointerEvent& screenEvent, std::string& error)
{
    error.clear();
    if (!get(root) || !std::isfinite(screenEvent.time) || screenEvent.time < 0.0)
    { error = "Invalid native pointer root or event time"; return false; }
    switch (screenEvent.kind)
    {
        case PointerKind::LeftDown: case PointerKind::LeftUp:
        case PointerKind::RightDown: case PointerKind::RightUp:
        case PointerKind::DoubleClick: case PointerKind::Hover: case PointerKind::MiddleDown: break;
        default: error = "Invalid native pointer kind"; return false;
    }
    if (screenEvent.kind==PointerKind::Hover && !updatePointerHover(root,screenEvent,error)) return false;
    if (!mMouseCapture && mTopControl && get(mTopControl))
    {
        const Id top = mTopControl;
        const auto rectangle = screenRect(top,error);
        if (!rectangle) return false;
        const auto localX = std::int64_t(screenEvent.x)-rectangle->left;
        const auto localY = std::int64_t(screenEvent.y)-rectangle->bottom;
        if (localX < INT32_MIN || localX > INT32_MAX || localY < INT32_MIN || localY > INT32_MAX)
        { error = "Native top-control pointer coordinates overflow"; return false; }
        const auto inside = containsLocal(top,static_cast<std::int32_t>(localX),static_cast<std::int32_t>(localY),true,0,error);
        if (!inside) return false;
        if (*inside)
        {
            auto event = screenEvent;
            event.x = static_cast<std::int32_t>(localX);
            event.y = static_cast<std::int32_t>(localY);
            if (handlePointer(top,event,error)) return true;
            if (!error.empty()) return false;
        }
        else if (screenEvent.kind == PointerKind::LeftDown && get(top)->combo) hideComboList(top);
    }
    const Id target = mMouseCapture ? mMouseCapture : root;
    if (!canReceiveFocus(target)) { error = "Native pointer target is erasing"; return false; }
    auto rect = screenRect(target,error);
    if (!rect) return false;
    const auto localX = std::int64_t(screenEvent.x)-rect->left;
    const auto localY = std::int64_t(screenEvent.y)-rect->bottom;
    if (localX < INT32_MIN || localX > INT32_MAX || localY < INT32_MIN || localY > INT32_MAX)
    { error = "Native pointer coordinate conversion overflow"; return false; }
    auto event = screenEvent;
    event.x = static_cast<std::int32_t>(localX);
    event.y = static_cast<std::int32_t>(localY);
    return handlePointer(target,event,error);
}

bool LLVKWidgetTree::childrenPointer(Id id, const PointerEvent& event, std::string& error)
{
    const auto* node = get(id);
    if (!node) return false;
    const auto children = node->children;
    for (Id child : children)
    {
        const auto* current = get(child);
        if (!current || current->parent != id || !current->params.visible || !current->params.enabled) continue;
        const auto localX = std::int64_t(event.x)-current->params.rect.left;
        const auto localY = std::int64_t(event.y)-current->params.rect.bottom;
        if (localX < INT32_MIN || localX > INT32_MAX || localY < INT32_MIN || localY > INT32_MAX)
        { error = "Native child pointer coordinates overflow"; return false; }
        auto localEvent = event;
        localEvent.x = static_cast<std::int32_t>(localX);
        localEvent.y = static_cast<std::int32_t>(localY);
        auto contains = containsLocal(child,localEvent.x,localEvent.y,true,mTopControl,error);
        if (!contains) return false;
        if (!*contains) continue;
        if (handlePointer(child,localEvent,error)) return true;
        if (!error.empty()) return false;
        current = get(child);
        if (!get(id)) return true;
        if (current && current->params.mouseOpaque)
        {
            contains = containsLocal(child,localEvent.x,localEvent.y,false,mTopControl,error);
            if (!contains) return false;
            if (*contains)
            {
                if (event.kind == PointerKind::Hover) cursorEffect(child,false);
                return true;
            }
        }
    }
    return false;
}

bool LLVKWidgetTree::basePointer(Id id, const PointerEvent& event, std::string& error)
{
    const bool handled = childrenPointer(id,event,error);
    if (!error.empty()) return false;
    const auto* node = get(id);
    if (!node) return true;
    const auto events = mEvents.find(id);
    if (node->control && event.kind != PointerKind::Hover && events != mEvents.end())
    {
        const auto callback = events->second.pointer;
        if (callback) callback(id,event);
    }
    return handled;
}

void LLVKWidgetTree::cursorEffect(Id id, bool hand)
{
    const auto events = mEvents.find(id);
    if (events == mEvents.end() || !get(id)) return;
    const auto callback = events->second.cursor;
    if (callback) callback(id,hand);
}

bool LLVKWidgetTree::handlePointer(Id id, PointerEvent event, std::string& error)
{
    const auto* node = get(id);
    if (!node) return false;
    if (node->layoutStack && layoutStackPointer(id,event,error)) return true;
    if (!error.empty()) return false;
    if (node->menu)
    {
        const auto menu=node->menu;
        const auto rect=screenRect(id,error);
        if (!rect) return false;
        event.x+=rect->left; event.y+=rect->bottom;
        const bool handled=menu->pointer(event);
        if (handled && event.kind==PointerKind::LeftDown) setKeyboardFocus(id,false,false,error);
        return handled;
    }
    if (node->slider) return sliderPointer(id,event,error);
    if (node->colorSwatch) return colorSwatchPointer(id,event,error);
    if (node->textureControl) return textureControlPointer(id,event,error);
    if (node->colorPicker) return colorPickerPointer(id,event,error);
    if (node->scrollList) return scrollListPointer(id,event,error);
    if (node->statBar)
        return event.kind==PointerKind::LeftDown ? cycleStatBar(id,error) : basePointer(id,event,error);
    if (node->containerView)
    {
        const auto params=*node->containerView;
        const auto height=node->params.rect.top-node->params.rect.bottom;
        if (params.displayChildren && childrenPointer(id,event,error)) return true;
        if (!error.empty() || !get(id)) return error.empty();
        if (params.showLabel && event.y>=height-10 &&
            (event.kind==PointerKind::LeftDown || event.kind==PointerKind::DoubleClick))
            return setContainerExpanded(id,!params.displayChildren,error);
        return false;
    }
    if (node->comboListOwner) return comboListPointer(id,event,error);
    if (node->lineEditor) return lineEditorPointer(id,event,error);
    if (node->scrollbar) return scrollbarPointer(id,event,error);
    if (node->button) return buttonPointer(id,event,error);
    if (node->plainText) return plainTextPointer(id,event,error);
    if (event.kind == PointerKind::Hover && iconWantsHandCursor(id))
    { cursorEffect(id,true); return true; }
    return basePointer(id,event,error);
}

bool LLVKWidgetTree::buttonPointer(Id id, PointerEvent event, std::string& error)
{
    if (event.kind == PointerKind::MiddleDown) return basePointer(id,event,error);
    if (event.kind == PointerKind::DoubleClick) event.kind = PointerKind::LeftDown;
    const auto focusButton = [&]
    {
        const auto* node = get(id);
        if (mLockedFocus && !hasAncestor(id,mLockedFocus)) return;
        if (node && node->control->params.tabStop && !chromeInChain(id))
            requestControlFocus(id,true,error);
    };
    if (event.kind == PointerKind::LeftDown)
    {
        if (childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        if (!setMouseCapture(id,error)) return false;
        if (!get(id)) return true;
        focusButton();
        if (!get(id)) return true;
        if (!error.empty()) return false;
        basePointer(id,event,error);
        if (!error.empty()) return false;
        if (!buttonCallback(id,&LLVKButton::Params::mouseDown,LLSD())) return true;
        auto& button = *mNodes.at(id).button;
        button.mouseDownTime = event.time;
        button.mouseDownFrame = event.frame;
        button.heldCount = 0;
        buttonSound(id,false);
        return true;
    }
    if (event.kind == PointerKind::LeftUp)
    {
        if (mMouseCapture == id)
        {
            mNodes.at(id).button->mouseDownTime.reset();
            setMouseCapture(0,error);
            if (!get(id)) return true;
            basePointer(id,event,error);
            if (!error.empty()) return false;
            if (!buttonCallback(id,&LLVKButton::Params::mouseUp,LLSD())) return true;
            auto contains = containsLocal(id,event.x,event.y,true,mTopControl,error);
            if (!contains) return false;
            if (!*contains) mouseLeave(id);
            if (*contains)
            {
                buttonSound(id,true);
                const auto* node = get(id);
                if (!node) return true;
                if (node->button->params.toggle && !setButtonToggle(id,!node->control->value.asBoolean(),error)) return false;
                buttonCommitSignal(id);
            }
        }
        else childrenPointer(id,event,error);
        return error.empty();
    }
    if (event.kind == PointerKind::RightDown)
    {
        if (!get(id)->button->params.handleRightMouse) return true;
        if (childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        if (!setMouseCapture(id,error)) return false;
        if (!get(id)) return true;
        focusButton();
        if (!get(id)) return true;
        if (!error.empty()) return false;
        basePointer(id,event,error);
        return error.empty();
    }
    if (event.kind == PointerKind::RightUp)
    {
        if (!get(id)->button->params.handleRightMouse) return true;
        if (mMouseCapture == id) setMouseCapture(0,error);
        else childrenPointer(id,event,error);
        if (!get(id)) return true;
        if (!error.empty()) return false;
        basePointer(id,event,error);
        return error.empty();
    }
    auto& button = *mNodes.at(id).button;
    if (enabledInChain(id) && (!mMouseCapture || mMouseCapture == id) && !button.highlighted)
    { button.highlighted = true; ++button.textGeneration; }
    if (childrenPointer(id,event,error)) return true;
    if (!error.empty()) return false;
    const auto* node = get(id);
    if (!node) return true;
    const auto& current = *node->button;
    if (current.mouseDownTime && event.time >= *current.mouseDownTime && event.frame >= current.mouseDownFrame &&
        static_cast<float>(event.time-*current.mouseDownTime) >= current.params.heldSeconds &&
        event.frame-current.mouseDownFrame >= current.params.heldFrames)
    {
        LLSD argument;
        argument["count"] = static_cast<LLSD::Integer>(mNodes.at(id).button->heldCount++);
        if (!buttonCallback(id,&LLVKButton::Params::held,argument)) return true;
    }
    if (get(id)) cursorEffect(id,get(id)->button->params.hoverHandCursor);
    return true;
}

void LLVKWidgetTree::buttonCaptureLost(Id id)
{
    const auto* node = get(id);
    if (!node || !node->button) return;
    if (node->button->params.commitOnCaptureLost && node->button->mouseDownTime)
    {
        if (!buttonCallback(id,&LLVKButton::Params::mouseUp,LLSD())) return;
        node = get(id);
        std::string error;
        if (node->button->params.toggle && !setButtonToggle(id,!node->control->value.asBoolean(),error))
        {
            if (get(id)) mNodes.at(id).button->mouseDownTime.reset();
            return;
        }
        buttonCommitSignal(id);
    }
    if (get(id)) mNodes.at(id).button->mouseDownTime.reset();
}
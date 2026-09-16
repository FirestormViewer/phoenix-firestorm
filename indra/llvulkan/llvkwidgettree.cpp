#include "llvkwidgettree.h"

#include <algorithm>
#include <limits>

namespace
{
    bool coordinate(std::int64_t value)
    {
        return value >= INT32_MIN && value <= INT32_MAX;
    }
}

bool LLVKWidgetTree::validRect(const Rect& rect) noexcept
{
    const auto width = std::int64_t(rect.right) - rect.left;
    const auto height = std::int64_t(rect.top) - rect.bottom;
    return coordinate(width) && coordinate(height);
}

const LLVKWidgetTree::Node* LLVKWidgetTree::get(Id id) const noexcept
{
    const auto found = mNodes.find(id);
    return found == mNodes.end() ? nullptr : &found->second;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::create(const Params& params, Id parent, std::string& error)
{
    error.clear();
    if ((parent && !get(parent)) || !validRect(params.rect) || (params.follows & ~15) || !mNextId)
    {
        error = "Invalid native widget parent, rectangle, follows flags or exhausted identity space";
        return std::nullopt;
    }
    if (parent && !canReceiveFocus(parent))
    { error = "Cannot construct beneath an erasing native widget"; return std::nullopt; }
    std::size_t depth = 1;
    for (Id ancestor = parent; ancestor; ancestor = get(ancestor)->parent) ++depth;
    if (depth > maximumDepth || mNodes.size() >= maximumNodes)
    {
        error = "Native widget tree size/depth limit exceeded";
        return std::nullopt;
    }
    const Id id = mNextId;
    auto inserted = mNodes.try_emplace(id);
    try
    {
        inserted.first->second.params = params;
        inserted.first->second.parent = parent;
        if (parent)
        {
            auto& owner = mNodes.at(parent);
            owner.children.insert(owner.children.begin(),id);
            owner.lastTabGroup = params.tabGroup.value_or(INT32_MAX);
        }
    }
    catch (...)
    {
        mNodes.erase(inserted.first);
        throw;
    }
    ++mNextId;
    return id;
}

bool LLVKWidgetTree::reparent(Id child, Id parent, bool inBack, std::int32_t tabGroup, std::string& error)
{
    error.clear();
    auto* childNode = get(child);
    if (!childNode || (parent && !get(parent)))
    {
        error = "Native widget reparent references a missing owner";
        return false;
    }
    if (!canReceiveFocus(child) || (parent && !canReceiveFocus(parent)))
    { error = "Cannot reparent an erasing native widget"; return false; }
    std::size_t parentDepth = 0;
    for (Id ancestor = parent; ancestor; ancestor = get(ancestor)->parent)
    {
        ++parentDepth;
        if (ancestor == child)
        {
            error = "Native widget parenting would create a cycle";
            return false;
        }
    }
    std::vector<std::pair<Id,std::size_t>> descendants{{child,parentDepth+1}};
    for (std::size_t index = 0; index < descendants.size(); ++index)
    {
        const auto [id,depth] = descendants[index];
        if (depth > maximumDepth)
        {
            error = "Native widget reparent exceeds the depth limit";
            return false;
        }
        for (Id descendant : get(id)->children) descendants.emplace_back(descendant,depth+1);
    }
    std::vector<Id> destination;
    if (parent)
    {
        destination = mNodes.at(parent).children;
        std::erase(destination,child);
        if (inBack) destination.push_back(child);
        else destination.insert(destination.begin(),child);
    }
    const Id previous = childNode->parent;
    if (previous && previous != parent) std::erase(mNodes.at(previous).children,child);
    if (parent)
    {
        auto& owner = mNodes.at(parent);
        owner.children.swap(destination);
        owner.lastTabGroup = tabGroup;
    }
    auto& node = mNodes.at(child);
    node.parent = parent;
    node.params.tabGroup = tabGroup;
    return true;
}

void LLVKWidgetTree::eraseSubtree(Id id)
{
    auto& node = mNodes.at(id);
    for (Id child : node.children) eraseSubtree(child);
    mEvents.erase(id);
    mLastGroupFocus.erase(id);
    std::erase(mFocusChain,id);
    if (mKeyboardFocus == id) mKeyboardFocus = 0;
    if (mMouseCapture == id) mMouseCapture = 0;
    if (mTopControl == id) mTopControl = 0;
    if (mLockedFocus == id) mLockedFocus = 0;
    mNodes.erase(id);
}

bool LLVKWidgetTree::erase(Id id, std::string& error)
{
    bool controls = false;
    for (const auto& [candidate,node] : mNodes)
        if (node.control && hasAncestor(candidate,id)) { controls = true; break; }
    return eraseOwned(id,controls,error);
}

bool LLVKWidgetTree::eraseControl(Id id, std::string& error)
{
    return eraseOwned(id,true,error);
}

bool LLVKWidgetTree::eraseOwned(Id id, bool notifyFocus, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node) { error = "Native widget to erase does not exist"; return false; }
    for (Id active : mErasing)
    {
        if (hasAncestor(id,active) || hasAncestor(active,id))
        { error = "Native subtree destruction is already active"; return false; }
    }
    if (node->lineEditor) mNodes.at(id).lineEditor->params.commitOnFocusLost = false;
    mErasing.insert(id);
    try
    {
        std::vector<Id> swatches;
        for (const auto& [candidate,current] : mNodes)
            if (current.colorSwatch && hasAncestor(candidate,id)) swatches.push_back(candidate);
        for (const auto swatch : swatches) closeColorSwatchPickers(swatch);
        if (notifyFocus)
        {
            if (hasAncestor(mMouseCapture,id)) setMouseCapture(0,error);
            if (hasAncestor(mKeyboardFocus,id))
            {
                if (hasAncestor(mLockedFocus,id)) mLockedFocus = 0;
                setKeyboardFocus(mLockedFocus,false,false,error);
            }
        }
    }
    catch (...)
    {
        mErasing.erase(id);
        throw;
    }
    node = get(id);
    if (node->parent) std::erase(mNodes.at(node->parent).children,id);
    eraseSubtree(id);
    mErasing.erase(id);
    ++mFocusEpoch;
    error.clear();
    return true;
}

bool LLVKWidgetTree::hasAncestor(Id id, Id ancestor) const noexcept
{
    if (!ancestor) return false;
    for (const Node* node = get(id); node; node = get(id))
    {
        if (id == ancestor) return true;
        id = node->parent;
    }
    return false;
}

bool LLVKWidgetTree::canReceiveFocus(Id id) const noexcept
{
    if (!id) return true;
    if (!get(id)) return false;
    for (Id active : mErasing) if (hasAncestor(id,active)) return false;
    return true;
}

void LLVKWidgetTree::notify(Id id, std::function<void(Id)> Events::* event)
{
    if (event==&Events::focusReceived && get(id) && get(id)->lineEditor)
        mNodes.at(id).lineEditor->caretResetTime=mTime;
    if (event==&Events::focusLost && get(id) && get(id)->textEditor &&
        get(id)->textEditor->commitOnFocusLost && canReceiveFocus(id))
    { std::string error; commitTextEditor(id,error); }
    if (event == &Events::focusLost && get(id) && get(id)->lineEditor)
    {
        lineLanguageInput(id);
        const auto* node = get(id);
        if (node && node->lineEditor->params.commitOnFocusLost && node->lineEditor->text.dirty()) commitLineEditor(id);
    }
    const auto found = mEvents.find(id);
    if (found != mEvents.end() && get(id))
    {
        const auto callback = found->second.*event;
        if (callback) callback(id);
    }
    if (event == &Events::focusReceived) lineLanguageInput(id);
}

bool LLVKWidgetTree::setEvents(Id id, Events events)
{
    if (!get(id) || !canReceiveFocus(id)) return false;
    mEvents.insert_or_assign(id,std::move(events));
    return true;
}

bool LLVKWidgetTree::setKeyboardFocus(Id id, bool lock, bool keystrokesOnly, std::string& error)
{
    error.clear();
    if (!canReceiveFocus(id)) { error = "Native focus target is missing or erasing"; return false; }
    if (mLockedFocus && !hasAncestor(id,mLockedFocus))
    { error = "Native focus is locked to a different subtree"; return false; }
    mKeystrokesOnly = keystrokesOnly;
    const auto epoch = ++mFocusEpoch;
    if (id != mKeyboardFocus)
    {
        std::vector<Id> newChain;
        for (Id ancestor = id; ancestor; ancestor = get(ancestor)->parent) newChain.push_back(ancestor);
        auto oldChain = mFocusChain;
        while (!newChain.empty() && !oldChain.empty() && newChain.back() == oldChain.back())
        { newChain.pop_back(); oldChain.pop_back(); }
        mKeyboardFocus = id;
        for (Id old : oldChain)
        {
            if (mFocusEpoch != epoch) return true;
            std::erase(mFocusChain,old);
            notify(old,&Events::focusLost);
            notify(old,&Events::focusChanged);
        }
        for (auto next = newChain.rbegin(); next != newChain.rend(); ++next)
        {
            if (mFocusEpoch != epoch) return true;
            if (!get(*next)) continue;
            mFocusChain.insert(mFocusChain.begin(),*next);
            notify(*next,&Events::focusReceived);
            notify(*next,&Events::focusChanged);
        }
    }
    if (mFocusEpoch != epoch) return true;
    for (auto ancestor=mKeyboardFocus; get(ancestor); ancestor=get(ancestor)->parent)
        if (get(ancestor)->params.focusRoot) mLastGroupFocus[ancestor]=mKeyboardFocus;
    if (lock) mLockedFocus = mKeyboardFocus;
    return true;
}

LLVKWidgetTree::Id LLVKWidgetTree::lastFocusForGroup(Id group) const noexcept
{
    const auto found=mLastGroupFocus.find(group);
    if (found==mLastGroupFocus.end() || !get(found->second) || !hasAncestor(found->second,group) ||
        !visibleInChain(found->second) || !enabledInChain(found->second)) return 0;
    return found->second;
}

bool LLVKWidgetTree::setMouseCapture(Id id, std::string& error)
{
    error.clear();
    if (!canReceiveFocus(id)) { error = "Native captor is missing or erasing"; return false; }
    if (id != mMouseCapture)
    {
        const Id previous = mMouseCapture;
        mMouseCapture = id;
        buttonCaptureLost(previous);
        if (const auto* node = get(previous); node && node->lineEditor)
            mNodes.at(previous).lineEditor->text.endSelection();
        notify(previous,&Events::captureLost);
    }
    return true;
}

bool LLVKWidgetTree::setTopControl(Id id, std::string& error)
{
    error.clear();
    if (!canReceiveFocus(id)) { error = "Native top control is missing or erasing"; return false; }
    if (id != mTopControl)
    {
        const Id previous = mTopControl;
        mTopControl = id;
        if (get(previous) && get(previous)->combo) hideComboList(previous);
        notify(previous,&Events::topLost);
    }
    return true;
}

bool LLVKWidgetTree::planReshape(Id id, std::int64_t width, std::int64_t height, const Rect& origin,
                                ShapeChanges& changes, std::string& error) const
{
    const auto& node = mNodes.at(id);
    const auto deltaWidth = width - (std::int64_t(node.params.rect.right)-node.params.rect.left);
    const auto deltaHeight = height - (std::int64_t(node.params.rect.top)-node.params.rect.bottom);
    const auto right = std::int64_t(origin.left) + width;
    const auto top = std::int64_t(origin.bottom) + height;
    if (!coordinate(width) || !coordinate(height) || !coordinate(right) || !coordinate(top))
    { error = "Native widget reshape would overflow its coordinates"; return false; }
    changes.rectangles[id] = {origin.left,origin.bottom,static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
    if (node.lineEditor)
    {
        auto text = node.lineEditor->text;
        if (!text.resize(static_cast<std::int32_t>(width),error)) return false;
        changes.editors.insert_or_assign(id,std::move(text));
    }
    if (node.checkBox && node.checkBox->label && node.checkBox->button)
        return planCheckBoxReshape(id,width,changes,error);
    if (deltaWidth || deltaHeight) for (Id child : node.children)
    {
        const auto& params = mNodes.at(child).params;
        const bool left = (params.follows & Left) != 0;
        const bool rightFollow = (params.follows & Right) != 0;
        const bool bottom = (params.follows & Bottom) != 0;
        const bool topFollow = (params.follows & Top) != 0;
        const auto childLeft = std::int64_t(params.rect.left) + (rightFollow && !left ? deltaWidth : 0);
        const auto childBottom = std::int64_t(params.rect.bottom) + (topFollow && !bottom ? deltaHeight : 0);
        const auto childWidth = std::int64_t(params.rect.right)-params.rect.left + (left && rightFollow ? deltaWidth : 0);
        const auto childHeight = std::int64_t(params.rect.top)-params.rect.bottom + (bottom && topFollow ? deltaHeight : 0);
        if (!coordinate(childLeft) || !coordinate(childBottom))
        { error = "Native widget follows translation would overflow"; return false; }
        Rect childOrigin = params.rect;
        childOrigin.left = static_cast<std::int32_t>(childLeft);
        childOrigin.bottom = static_cast<std::int32_t>(childBottom);
        if (!planReshape(child,childWidth,childHeight,childOrigin,changes,error)) return false;
    }
    if (node.scrollbar && (deltaWidth || deltaHeight))
    {
        const auto& scrollbar = *node.scrollbar;
        if (width < 0 || height < 0) { error = "Native scrollbar resize dimensions cannot be negative"; return false; }
        LLVKScrollLayout::ThumbParams thumb;
        thumb.documentSize = scrollbar.documentSize; thumb.pageSize = scrollbar.pageSize; thumb.position = scrollbar.position;
        thumb.thickness = scrollbar.thickness; thumb.vertical = scrollbar.params->vertical;
        thumb.length = static_cast<std::int32_t>(thumb.vertical ? height : width);
        const auto rectangle = LLVKScrollLayout::thumb(thumb,error);
        if (!rectangle) return false;
        changes.thumbs[id] = *rectangle;
        for (const Id child : {scrollbar.decrease,scrollbar.increase})
        {
            if (!child || !get(child) || !changes.rectangles.contains(child))
            { error = "Native scrollbar resize requires its button children"; return false; }
            const auto planned = changes.rectangles.at(child);
            auto childWidth = std::int64_t(planned.right)-planned.left;
            auto childHeight = std::int64_t(planned.top)-planned.bottom;
            Rect childOrigin{};
            if (thumb.vertical)
            {
                childHeight = std::min(height/2,std::int64_t(scrollbar.thickness));
                childOrigin.bottom = child == scrollbar.decrease ? static_cast<std::int32_t>(height-childHeight) : 0;
            }
            else
            {
                childWidth = std::min(width/2,std::int64_t(scrollbar.thickness));
                childOrigin.left = child == scrollbar.increase ? static_cast<std::int32_t>(width-childWidth) : 0;
            }
            if (!planReshape(child,childWidth,childHeight,childOrigin,changes,error)) return false;
        }
    }
    if (node.scrollContainer) changes.scrollContainers.push_back(id);
    return true;
}

bool LLVKWidgetTree::reshape(Id id, std::int32_t width, std::int32_t height, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node) { error = "Native widget to reshape does not exist"; return false; }
    ShapeChanges changes;
    if (!planReshape(id,width,height,node->params.rect,changes,error)) return false;
    return completeShapes(changes,error);
}

bool LLVKWidgetTree::completeShapes(ShapeChanges& changes, std::string& error)
{
    publishShapes(changes);
    for (const Id id : changes.scrollContainers)
        if (get(id) && !finishScrollResize(id,error)) return false;
    return true;
}

void LLVKWidgetTree::publishShapes(ShapeChanges& changes)
{
    for (const auto& [changed,rect] : changes.rectangles)
    {
        auto& current = mNodes.at(changed);
        if (current.plainText && current.params.rect != rect) current.plainText->layout.reset();
        if (current.layoutStack && current.params.rect != rect) current.layoutStack->needsLayout = true;
        current.params.rect = rect;
    }
    for (auto& [id,text] : changes.editors) mNodes.at(id).lineEditor->text = std::move(text);
    for (const auto& [id,thumb] : changes.thumbs) mNodes.at(id).scrollbar->thumb = thumb;
}

bool LLVKWidgetTree::setShape(Id id, const Rect& rectangle, std::string& error)
{
    error.clear();
    if (!get(id)) { error = "Native shape target does not exist"; return false; }
    ShapeChanges changes;
    if (!planReshape(id,std::int64_t(rectangle.right)-rectangle.left,std::int64_t(rectangle.top)-rectangle.bottom,
                     rectangle,changes,error)) return false;
    return completeShapes(changes,error);
}

bool LLVKWidgetTree::expandFloaterHeader(Id panel,std::string& error)
{
    error.clear();
    auto found=mNodes.find(panel);
    if (found==mNodes.end() || !found->second.floater) { error="Native header expansion requires a floater"; return false; }
    auto& node=found->second;
    auto& state=*node.floater;
    if (state.headerExpanded) return true;
    const auto stretch=std::max<std::int64_t>(0,std::int64_t(state.headerHeight)-state.legacyHeaderHeight);
    if (std::int64_t(node.params.rect.top)+stretch>INT32_MAX || stretch>16384)
    { error="Native floater header expansion exceeds bounds"; return false; }
    node.params.rect.top+=static_cast<std::int32_t>(stretch);
    state.headerExpanded=true;
    return true;
}

bool LLVKWidgetTree::setVisible(Id id, bool visible)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end()) return false;
    if (found->second.params.visible == visible) return true;
    found->second.params.visible = visible;
    const Id parent = found->second.parent;
    if (found->second.layoutPanel && get(parent) && get(parent)->layoutStack)
        mNodes.at(parent).layoutStack->needsLayout = true;
    if (!parent || visibleInChain(parent)) notifyVisibility(id,visible);
    return true;
}

void LLVKWidgetTree::notifyVisibility(Id id, bool visible)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end()) return;
    if (found->second.button) ++found->second.button->textGeneration;
    const auto children = found->second.children;
    for (Id child : children)
    {
        const auto* node = get(child);
        if (node && node->parent == id && node->params.visible) notifyVisibility(child,visible);
        if (!get(id)) return;
    }
    const auto* node = get(id);
    if (node && node->panel)
    {
        const auto callback = node->panel->visible;
        if (callback.function) callback.function(id,callback.parameter.value_or(LLSD(visible)));
    }
}

bool LLVKWidgetTree::setEnabled(Id id, bool enabled)
{
    auto found = mNodes.find(id);
    if (found == mNodes.end()) return false;
    if (found->second.textEditor)
    {
        const auto body=found->second.textEditor->body;
        found->second.textEditor->readOnly=!enabled;
        if (get(body) && get(body)->plainText) mNodes.at(body).plainText->readOnly=!enabled;
        return true;
    }
    if (found->second.lineEditor)
    {
        found->second.lineEditor->readOnly = !enabled;
        found->second.control->params.tabStop = enabled;
        lineLanguageInput(id);
        return true;
    }
    found->second.params.enabled = enabled;
    if (found->second.colorSwatch && !enabled)
    {
        closeColorSwatchPickers(id);
        found=mNodes.find(id);
        if (found==mNodes.end()) return true;
    }
    if (found->second.spinner)
    {
        const auto spinner = *found->second.spinner;
        if (get(spinner.editor)) setEnabled(spinner.editor,enabled);
        if (get(spinner.label) && get(spinner.label)->plainText)
        {
            auto& text = *mNodes.at(spinner.label).plainText;
            text.params.textColor = text.params.readOnlyColor = enabled ? spinner.params->textEnabledColor : spinner.params->textDisabledColor;
        }
    }
    if (found->second.plainText) found->second.plainText->readOnly = !enabled;
    if (found->second.checkBox)
    {
        const auto label = mNodes.find(found->second.checkBox->label);
        if (label != mNodes.end() && label->second.plainText)
        {
            const auto& colors = found->second.checkBox->construction.labelText;
            label->second.plainText->params.textColor = enabled ? colors.textColor : colors.readOnlyColor;
            ++label->second.plainText->textGeneration;
        }
    }
    return true;
}

bool LLVKWidgetTree::visibleInChain(Id id) const noexcept
{
    if (!get(id)) return false;
    for (Id ancestor = id; ancestor; ancestor = get(ancestor)->parent)
        if (!get(ancestor)->params.visible) return false;
    return true;
}

bool LLVKWidgetTree::enabledInChain(Id id) const noexcept
{
    if (!get(id)) return false;
    for (Id ancestor = id; ancestor; ancestor = get(ancestor)->parent)
        if (!get(ancestor)->params.enabled) return false;
    return true;
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetTree::boundingRect(Id id, Id topControl, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node) { error = "Missing native view for bounding rectangle"; return std::nullopt; }
    if (!node->params.useBoundingRect) return node->params.rect;
    Rect bounds;
    const auto empty = [](const Rect& rect) { return rect.left == rect.right || rect.bottom == rect.top; };
    for (Id child : node->children)
    {
        if (child == topControl || !get(child)->params.visible) continue;
        auto childBounds = boundingRect(child,topControl,error);
        if (!childBounds) return std::nullopt;
        if (empty(bounds)) bounds = *childBounds;
        else if (!empty(*childBounds))
        {
            bounds.left = std::min(bounds.left,childBounds->left);
            bounds.right = std::max(bounds.right,childBounds->right);
            bounds.bottom = std::min(bounds.bottom,childBounds->bottom);
            bounds.top = std::max(bounds.top,childBounds->top);
        }
    }
    const auto left = std::int64_t(bounds.left)+node->params.rect.left;
    const auto right = std::int64_t(bounds.right)+node->params.rect.left;
    const auto bottom = std::int64_t(bounds.bottom)+node->params.rect.bottom;
    const auto top = std::int64_t(bounds.top)+node->params.rect.bottom;
    if (!coordinate(left) || !coordinate(right) || !coordinate(bottom) || !coordinate(top) ||
        !coordinate(right-left) || !coordinate(top-bottom))
    { error = "Native bounding rectangle overflow"; return std::nullopt; }
    return Rect{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
                static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetTree::screenRect(Id id, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node) { error = "Missing native view for screen rectangle"; return std::nullopt; }
    std::int64_t left = node->params.rect.left;
    std::int64_t right = node->params.rect.right;
    std::int64_t bottom = node->params.rect.bottom;
    std::int64_t top = node->params.rect.top;
    for (Id ancestor = node->parent; ancestor; ancestor = get(ancestor)->parent)
    {
        const auto& rect = get(ancestor)->params.rect;
        left += rect.left; right += rect.left;
        bottom += rect.bottom; top += rect.bottom;
        if (!coordinate(left) || !coordinate(right) || !coordinate(bottom) || !coordinate(top))
        { error = "Native screen rectangle overflow"; return std::nullopt; }
    }
    return Rect{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
                static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
}

std::optional<bool> LLVKWidgetTree::containsLocal(Id id, std::int32_t x, std::int32_t y,
    bool useBounds, Id topControl, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node) { error = "Missing native view for hit containment"; return std::nullopt; }
    if (!useBounds || !node->params.useBoundingRect)
        return x >= 0 && y >= 0 && x < std::int64_t(node->params.rect.right)-node->params.rect.left &&
               y < std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    auto bounds = boundingRect(id,topControl,error);
    if (!bounds) return std::nullopt;
    const auto parentX = std::int64_t(x)+node->params.rect.left;
    const auto parentY = std::int64_t(y)+node->params.rect.bottom;
    return bounds->left <= parentX && parentX < bounds->right &&
           bounds->bottom <= parentY && parentY < bounds->top;
}

std::uint8_t LLVKWidgetTree::parseFollows(const std::string& text)
{
    std::uint8_t flags = 0;
    std::size_t begin = 0;
    while (begin < text.size())
    {
        const auto end = text.find('|',begin);
        const auto token = text.substr(begin,end == std::string::npos ? end : end-begin);
        if (token == "left") flags |= Left;
        else if (token == "right") flags |= Right;
        else if (token == "top") flags |= Top;
        else if (token == "bottom") flags |= Bottom;
        else if (token == "all") flags |= 15;
        if (end == std::string::npos) break;
        begin = end+1;
    }
    return flags;
}
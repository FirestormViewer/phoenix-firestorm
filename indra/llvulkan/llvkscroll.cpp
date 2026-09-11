#include "llvkscroll.h"
#include "llvkwidgettree.h"

#include <climits>
#include <algorithm>
#include <cmath>

std::optional<LLVKScrollLayout::Visible> LLVKScrollLayout::visible(const Params& params, std::string& error)
{
    error.clear();
    if (params.width < 0 || params.height < 0 || params.documentWidth < 0 || params.documentHeight < 0 ||
        params.borderWidth < 0 || params.scrollbarSize < 0)
    { error = "Native scroll layout extents cannot be negative"; return std::nullopt; }
    std::int64_t width = std::int64_t(params.width)-2*std::int64_t(params.borderWidth);
    std::int64_t height = std::int64_t(params.height)-2*std::int64_t(params.borderWidth);
    Visible result;
    if (!params.hideScrollbars)
    {
        if (std::int64_t(params.documentHeight)-height > 1)
        {
            result.vertical = true;
            width -= params.scrollbarSize;
        }
        if (std::int64_t(params.documentWidth)-width > 1)
        {
            result.horizontal = true;
            height -= params.scrollbarSize;
            if (!result.vertical && std::int64_t(params.documentHeight)-height > 1)
            {
                result.vertical = true;
                width -= params.scrollbarSize;
            }
        }
    }
    if (width < INT32_MIN || height < INT32_MIN)
    { error = "Native scroll content extent overflows"; return std::nullopt; }
    result.width = static_cast<std::int32_t>(width);
    result.height = static_cast<std::int32_t>(height);
    return result;
}

std::optional<LLVKScrollLayout::Rect> LLVKScrollLayout::thumb(const ThumbParams& params, std::string& error)
{
    error.clear();
    if (params.documentSize < 0 || params.pageSize < 0 || params.length < 0 || params.thickness < 0)
    { error = "Native thumb dimensions cannot be negative"; return std::nullopt; }
    const auto track = std::max<std::int64_t>(0,std::int64_t(params.length)-2*std::int64_t(params.thickness));
    const auto visible = std::min(params.documentSize,params.pageSize);
    const auto length = params.documentSize ? std::min(std::max<std::int64_t>(visible*track/params.documentSize,16),track) : track;
    const auto variable = params.documentSize-visible;
    const auto displacement = variable ? std::int64_t(params.position)*(track-length)/variable : 0;
    std::int64_t left = 0, bottom = 0, right = params.thickness, top = params.thickness;
    if (params.vertical)
    {
        const auto maximum = track+params.thickness;
        const auto minimum = std::int64_t(params.thickness)+16;
        top = variable ? std::min(std::max(maximum-displacement,minimum),maximum) : maximum;
        bottom = top-length;
    }
    else
    {
        const auto maximum = track+params.thickness-length;
        left = variable ? std::min(std::max(std::int64_t(params.thickness)+displacement,std::int64_t(params.thickness)),maximum) : params.thickness;
        right = left+length;
    }
    for (const auto value : {left,bottom,right,top})
        if (value < INT32_MIN || value > INT32_MAX)
        { error = "Native thumb rectangle overflows"; return std::nullopt; }
    return Rect{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
        static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createScrollbar(const Params& view,
    const LLVKControl::Params& control, const ScrollbarParams& params, Id parent, std::string& error)
{
    error.clear();
    const auto setting = mSettings.find("UIScrollbarSize");
    const auto thickness = params.thickness.value_or(setting == mSettings.end() ? 0 : setting->second.asInteger());
    if (params.documentSize < 0 || params.pageSize < 0 || thickness < 0)
    { error = "Native scrollbar dimensions cannot be negative"; return std::nullopt; }
    Scrollbar scrollbar;
    scrollbar.params = std::make_shared<const ScrollbarParams>(params);
    scrollbar.documentSize = params.documentSize;
    scrollbar.position = params.position;
    scrollbar.pageSize = params.pageSize;
    scrollbar.thickness = thickness;
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::move(scrollbar));
}

bool LLVKWidgetTree::refreshScrollbarThumb(Id id, std::string& error)
{
    const auto* node = get(id);
    if (!node || !node->scrollbar) { error = "Native scrollbar does not exist"; return false; }
    const auto& scrollbar = *node->scrollbar;
    const auto length = scrollbar.params->vertical ? std::int64_t(node->params.rect.top)-node->params.rect.bottom :
        std::int64_t(node->params.rect.right)-node->params.rect.left;
    if (length < 0 || length > INT32_MAX) { error = "Native scrollbar length is invalid"; return false; }
    LLVKScrollLayout::ThumbParams params;
    params.documentSize = scrollbar.documentSize; params.pageSize = scrollbar.pageSize; params.position = scrollbar.position;
    params.length = static_cast<std::int32_t>(length); params.thickness = scrollbar.thickness; params.vertical = scrollbar.params->vertical;
    const auto thumb = LLVKScrollLayout::thumb(params,error);
    if (!thumb) return false;
    mNodes.at(id).scrollbar->thumb = *thumb;
    return true;
}

bool LLVKWidgetTree::constructScrollbarChildren(Id id, std::string& error)
{
    if (!refreshScrollbarThumb(id,error)) return false;
    const auto params = get(id)->scrollbar->params;
    const auto rect = get(id)->params.rect;
    const auto width = std::int64_t(rect.right)-rect.left, height = std::int64_t(rect.top)-rect.bottom;
    if (width < 0 || width > INT32_MAX || height < 0 || height > INT32_MAX)
    { error = "Native scrollbar child geometry is invalid"; return false; }
    const auto thickness = get(id)->scrollbar->thickness;
    for (bool increase : {false,true})
    {
        Params view;
        view.name = increase ? "Line Down" : "Line Up";
        if (params->vertical)
        {
            view.rect = increase ? Rect{0,0,thickness,thickness} :
                Rect{0,static_cast<std::int32_t>(height-thickness),thickness,static_cast<std::int32_t>(height)};
            view.follows = Right | (increase ? Bottom : Top);
        }
        else
        {
            view.rect = increase ? Rect{static_cast<std::int32_t>(width-thickness),0,static_cast<std::int32_t>(width),thickness} :
                Rect{0,0,thickness,thickness};
            view.follows = Bottom | (increase ? Right : Left);
        }
        auto control = increase ? params->increaseControl : params->decreaseControl;
        control.tabStop = false;
        auto button = increase ? params->increaseButton : params->decreaseButton;
        LLVKControl::Callback change;
        change.function = [this,id,increase](Id,const LLSD&)
        {
            const auto* node = get(id);
            if (!node || !node->scrollbar) return;
            const auto& current = *node->scrollbar;
            const auto position = std::int64_t(current.position)+(increase ? std::int64_t(current.params->stepSize) : -std::int64_t(current.params->stepSize));
            std::string failure;
            setScrollPosition(id,static_cast<std::int32_t>(std::clamp<std::int64_t>(position,INT32_MIN,INT32_MAX)),true,failure);
        };
        button.click = change;
        button.held = change;
        const auto child = createButton(view,control,button,0,error);
        if (!child) return false;
        if (!get(id) || !postBuildButton(*child,error) || !reparent(*child,id,false,0,error))
        {
            std::string cleanup;
            erase(*child,cleanup);
            if (error.empty()) error = "Native scrollbar child lost its owner";
            return false;
        }
        if (increase) mNodes.at(id).scrollbar->increase = *child;
        else mNodes.at(id).scrollbar->decrease = *child;
    }
    return true;
}

bool LLVKWidgetTree::setScrollPosition(Id id, std::int32_t position, bool updateThumb, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar) { error = "Native scroll position target is missing"; return false; }
    const auto& state = *node->scrollbar;
    position = std::clamp(position,0,std::max(0,state.documentSize-state.pageSize));
    if (position == state.position) return false;
    const auto callback = state.params->changed;
    mNodes.at(id).scrollbar->position = position;
    mNodes.at(id).scrollbar->documentChanged = true;
    if (callback) callback(id,position);
    if (!get(id)) return true;
    if (updateThumb && !refreshScrollbarThumb(id,error)) return false;
    return true;
}

bool LLVKWidgetTree::setScrollDocumentSize(Id id, std::int32_t size, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar || size < 0) { error = "Invalid native scroll document size"; return false; }
    if (size == node->scrollbar->documentSize) return true;
    const auto position = node->scrollbar->position;
    mNodes.at(id).scrollbar->documentSize = size;
    setScrollPosition(id,position,true,error);
    if (!error.empty()) return false;
    if (!get(id)) return true;
    mNodes.at(id).scrollbar->documentChanged = true;
    return refreshScrollbarThumb(id,error);
}

bool LLVKWidgetTree::setScrollPageSize(Id id, std::int32_t size, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar || size < 0) { error = "Invalid native scroll page size"; return false; }
    if (size == node->scrollbar->pageSize) return true;
    const auto position = node->scrollbar->position;
    mNodes.at(id).scrollbar->pageSize = size;
    setScrollPosition(id,position,true,error);
    if (!error.empty()) return false;
    if (!get(id)) return true;
    mNodes.at(id).scrollbar->documentChanged = true;
    return refreshScrollbarThumb(id,error);
}

bool LLVKWidgetTree::scrollbarKey(Id id, ScrollKey key, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar) { error = "Native scrollbar key target is missing"; return false; }
    const auto& state = *node->scrollbar;
    const auto maximum = std::max(0,state.documentSize-state.pageSize);
    if (!maximum && !node->params.visible) return false;
    std::int64_t position = state.position;
    bool handled = true;
    switch (key)
    {
        case ScrollKey::Home: position = 0; break;
        case ScrollKey::End: position = maximum; break;
        case ScrollKey::Up: position -= state.params->stepSize; break;
        case ScrollKey::Down: position += state.params->stepSize; break;
        case ScrollKey::PageUp:
            if (state.documentSize > state.pageSize) position -= std::int64_t(state.pageSize)-1;
            handled = false;
            break;
        case ScrollKey::PageDown:
            if (state.documentSize > state.pageSize) position += std::int64_t(state.pageSize)-1;
            handled = false;
            break;
        case ScrollKey::Left: case ScrollKey::Right: return false;
        default: error = "Invalid native scrollbar key"; return false;
    }
    setScrollPosition(id,static_cast<std::int32_t>(std::clamp<std::int64_t>(position,INT32_MIN,INT32_MAX)),true,error);
    return error.empty() && handled;
}

bool LLVKWidgetTree::scrollbarWheel(Id id, std::int32_t clicks, bool horizontal, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar) { error = "Native scrollbar wheel target is missing"; return false; }
    const auto& state = *node->scrollbar;
    if (horizontal && state.params->vertical) return false;
    const auto position = std::int64_t(state.position)+std::int64_t(clicks)*state.params->stepSize;
    return setScrollPosition(id,static_cast<std::int32_t>(std::clamp<std::int64_t>(position,INT32_MIN,INT32_MAX)),true,error);
}

bool LLVKWidgetTree::scrollbarPointer(Id id, PointerEvent event, std::string& error)
{
    if (event.kind == PointerKind::DoubleClick) event.kind = PointerKind::LeftDown;
    if (event.kind == PointerKind::LeftDown)
    {
        if (childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        const auto state = *get(id)->scrollbar;
        const auto& thumb = state.thumb;
        if (event.x >= thumb.left && event.x < thumb.right && event.y >= thumb.bottom && event.y < thumb.top)
        {
            if (!setMouseCapture(id,error)) return false;
            if (!get(id)) return true;
            auto& current = *mNodes.at(id).scrollbar;
            current.dragThumb = current.thumb;
            current.dragStart = current.params->vertical ? event.y : event.x;
            current.lastDragDelta = 0;
        }
        else if (state.documentSize > state.pageSize)
        {
            const bool before = state.params->vertical ? event.y > thumb.top : event.x < thumb.left;
            const bool after = state.params->vertical ? event.y < thumb.bottom : event.x > thumb.right;
            if (before || after)
            {
                const auto position = std::int64_t(state.position)+(before ? -std::int64_t(state.pageSize) : state.pageSize);
                setScrollPosition(id,static_cast<std::int32_t>(std::clamp<std::int64_t>(position,INT32_MIN,INT32_MAX)),true,error);
            }
        }
        return error.empty();
    }
    if (event.kind == PointerKind::LeftUp)
    {
        if (mMouseCapture == id) return setMouseCapture(0,error);
        if (childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        const auto* node = get(id);
        if (!node) return true;
        const auto inside = containsLocal(id,event.x,event.y,false,mTopControl,error);
        return inside && *inside && node->params.mouseOpaque;
    }
    if (event.kind != PointerKind::Hover) return basePointer(id,event,error);
    if (mMouseCapture != id)
    {
        const bool handled = childrenPointer(id,event,error);
        if (!error.empty()) return false;
        if (!get(id)) return true;
        if (!handled) cursorEffect(id,false);
        if (get(id)) mNodes.at(id).scrollbar->documentChanged = false;
        return true;
    }
    const auto state = *get(id)->scrollbar;
    const auto& view = get(id)->params.rect;
    const bool vertical = state.params->vertical;
    const auto length = vertical ? std::int64_t(view.top)-view.bottom : std::int64_t(view.right)-view.left;
    auto delta = std::int64_t(vertical ? event.y : event.x)-state.dragStart;
    const auto start = vertical ? state.dragThumb.bottom : state.dragThumb.left;
    const auto end = vertical ? state.dragThumb.top : state.dragThumb.right;
    if (start+delta < state.thickness) delta = std::int64_t(state.thickness)-start-1;
    else if (end+delta > length-state.thickness) delta = length-state.thickness-end+1;
    if (start+delta < INT32_MIN || start+delta > INT32_MAX || end+delta < INT32_MIN || end+delta > INT32_MAX)
    { error = "Native scrollbar drag rectangle overflows"; return false; }
    auto thumb = state.dragThumb;
    if (vertical) { thumb.bottom = static_cast<std::int32_t>(start+delta); thumb.top = static_cast<std::int32_t>(end+delta); }
    else { thumb.left = static_cast<std::int32_t>(start+delta); thumb.right = static_cast<std::int32_t>(end+delta); }
    mNodes.at(id).scrollbar->thumb = thumb;
    const auto usable = length-2*std::int64_t(state.thickness)-(end-start);
    if (usable > 0 && (delta != state.lastDragDelta || state.documentChanged))
    {
        const auto maximum = std::max(0,state.documentSize-state.pageSize);
        const float ratio = float(start+delta-state.thickness)/float(usable);
        const float raw = vertical ? float(maximum)-ratio*float(maximum)+0.5f : ratio*float(maximum)+0.5f;
        const auto position = static_cast<std::int32_t>(std::clamp(double(raw),0.0,double(maximum)));
        setScrollPosition(id,position,false,error);
        if (!error.empty()) return false;
        if (!get(id)) return true;
    }
    mNodes.at(id).scrollbar->lastDragDelta = delta;
    cursorEffect(id,false);
    if (get(id)) mNodes.at(id).scrollbar->documentChanged = false;
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createScrollContainer(const Params& view,
    const LLVKControl::Params& control, const ScrollContainerParams& params, Id parent, std::string& error)
{
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::nullopt,std::make_shared<const ScrollContainerParams>(params));
}

bool LLVKWidgetTree::constructScrollContainerChildren(Id id, const ScrollContainerParams& params, std::string& error)
{
    const auto rectangle = get(id)->params.rect;
    const auto width = std::int64_t(rectangle.right)-rectangle.left;
    const auto height = std::int64_t(rectangle.top)-rectangle.bottom;
    const auto setting = mSettings.find("UIScrollbarSize");
    const auto size = params.size.value_or(setting == mSettings.end() ? 0 : setting->second.asInteger());
    const auto borderWidth = params.borderVisible ? params.border.thickness : 0;
    if (!std::isfinite(params.minAutoRate) || !std::isfinite(params.maxAutoRate) ||
        params.minAutoRate < 0.f || params.maxAutoRate < params.minAutoRate || params.maxAutoZone < 0)
    { error = "Invalid native auto-scroll rate or zone"; return false; }
    if (size < 0 || width < 2*std::int64_t(borderWidth) || height < 2*std::int64_t(borderWidth) || width > INT32_MAX || height > INT32_MAX)
    { error = "Invalid native scroll container dimensions"; return false; }
    Node::ScrollContainer state;
    state.scrollbarSize = size;
    state.useSizeSetting = !params.size;
    state.hideScrollbars = params.hideScrollbars;
    state.reserveCorner = params.reserveCorner;
    state.minAutoRate = params.minAutoRate; state.maxAutoRate = params.maxAutoRate;
    state.maxAutoZone = params.maxAutoZone;
    state.opaque = params.opaque;
    state.ignoreArrowKeys = params.ignoreArrowKeys;
    state.backgroundColor = params.backgroundColor;
    mNodes.at(id).scrollContainer = state;
    Params borderView;
    borderView.name = "scroll border";
    borderView.rect = {0,0,static_cast<std::int32_t>(width),static_cast<std::int32_t>(height)};
    borderView.visible = params.borderVisible;
    borderView.mouseOpaque = false;
    borderView.follows = Left | Right | Top | Bottom;
    auto borderParams = params.border;
    borderParams.bevel = LLVKBorder::Bevel::In;
    const auto border = createBorder(borderView,borderParams,id,error);
    if (!border) return false;
    mNodes.at(id).scrollContainer->border = *border;
    for (bool vertical : {true,false})
    {
        Params view;
        view.name = vertical ? "scrollable vertical" : "scrollable horizontal";
        view.visible = false;
        view.follows = vertical ? Right | Top | Bottom : Left | Right | Bottom;
        view.rect = vertical ? Rect{static_cast<std::int32_t>(width-borderWidth-size),borderWidth,
            static_cast<std::int32_t>(width-borderWidth),static_cast<std::int32_t>(height-borderWidth)} :
            Rect{0,0,static_cast<std::int32_t>(width-2*borderWidth),size};
        auto bar = vertical ? params.vertical : params.horizontal;
        bar.vertical = vertical;
        bar.position = 0;
        bar.stepSize = 16;
        bar.documentSize = bar.pageSize = static_cast<std::int32_t>((vertical ? height : width)-2*borderWidth);
        const auto child = createScrollbar(view,params.scrollbarControl,bar,0,error);
        if (!child) return false;
        if (!get(id) || !postBuildControl(*child) || !reparent(*child,id,false,0,error))
        {
            std::string cleanup;
            erase(*child,cleanup);
            if (error.empty()) error = "Native scroll container lost its owner during child construction";
            return false;
        }
        if (vertical) mNodes.at(id).scrollContainer->vertical = *child;
        else mNodes.at(id).scrollContainer->horizontal = *child;
    }
    return true;
}

bool LLVKWidgetTree::attachScrollContent(Id id, Id child, std::int32_t tabGroup, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer) { error = "Native scroll container is missing"; return false; }
    const auto state = *node->scrollContainer;
    if (child == state.border || child == state.vertical || child == state.horizontal)
    { error = "Native scroll chrome cannot become document content"; return false; }
    if (!reparent(child,id,false,tabGroup,error)) return false;
    if (!state.document || !get(state.document)) mNodes.at(id).scrollContainer->document = child;
    for (const Id bar : {state.horizontal,state.vertical})
        if (!reparent(bar,id,false,tabGroup,error)) return false;
    return true;
}

bool LLVKWidgetTree::updateScrollContainer(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer) { error = "Native scroll container is missing"; return false; }
    const auto state = *node->scrollContainer;
    if (!node->params.visible || !state.document) return true;
    const auto* document = get(state.document);
    const auto* border = get(state.border);
    const auto* vertical = get(state.vertical);
    const auto* horizontal = get(state.horizontal);
    if (!document || document->parent != id || !border || !border->border || !vertical || !vertical->scrollbar ||
        !horizontal || !horizontal->scrollbar)
    { error = "Native scroll container children are missing or detached"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    const auto docWidth = std::int64_t(document->params.rect.right)-document->params.rect.left;
    const auto docHeight = std::int64_t(document->params.rect.top)-document->params.rect.bottom;
    for (const auto extent : {width,height,docWidth,docHeight})
        if (extent < 0 || extent > INT32_MAX) { error = "Native scroll extent is invalid"; return false; }
    const auto edge = border->params.visible ? border->border->params.thickness : 0;
    const auto setting = mSettings.find("UIScrollbarSize");
    const auto size = state.useSizeSetting && setting != mSettings.end() ? setting->second.asInteger() : state.scrollbarSize;
    LLVKScrollLayout::Params layout;
    layout.width = static_cast<std::int32_t>(width); layout.height = static_cast<std::int32_t>(height);
    layout.documentWidth = static_cast<std::int32_t>(docWidth); layout.documentHeight = static_cast<std::int32_t>(docHeight);
    layout.borderWidth = edge; layout.scrollbarSize = size; layout.hideScrollbars = state.hideScrollbars;
    const auto visible = LLVKScrollLayout::visible(layout,error);
    if (!visible) return false;
    if (visible->width < 0 || visible->height < 0)
    { error = "Native scrollbar page cannot have negative content dimensions"; return false; }
    const auto oldDocument = document->params.rect;
    const bool resetHorizontal = !visible->horizontal || oldDocument.left > edge;
    const auto left = std::int64_t(edge)-(resetHorizontal ? 0 : horizontal->scrollbar->position);
    const auto top = height-edge+(visible->vertical ? vertical->scrollbar->position : 0);
    for (const auto coordinate : {left,left+docWidth,top,top-docHeight})
        if (coordinate < INT32_MIN || coordinate > INT32_MAX) { error = "Native scroll document translation overflows"; return false; }
    ShapeChanges verticalShapes, horizontalShapes;
    if (visible->vertical)
    {
        auto origin = vertical->params.rect;
        const auto offset = visible->horizontal || state.reserveCorner ? size : 0;
        const auto barHeight = std::int64_t(visible->height)-(!visible->horizontal && state.reserveCorner ? size : 0);
        if (barHeight < 0 || std::int64_t(edge)+offset > INT32_MAX)
        { error = "Native vertical scrollbar cannot fit reserved corner"; return false; }
        origin.bottom = edge+offset;
        if (!planReshape(state.vertical,size,barHeight,origin,verticalShapes,error)) return false;
    }
    if (visible->horizontal)
    {
        const auto barWidth = std::int64_t(visible->width)-(!visible->vertical && state.reserveCorner ? size : 0);
        if (barWidth < 0) { error = "Native horizontal scrollbar cannot fit reserved corner"; return false; }
        if (!planReshape(state.horizontal,barWidth,size,horizontal->params.rect,horizontalShapes,error)) return false;
    }
    mNodes.at(state.document).params.rect.top = static_cast<std::int32_t>(top);
    mNodes.at(state.document).params.rect.bottom = static_cast<std::int32_t>(top-docHeight);
    const auto publish = [&](ShapeChanges& changes)
    {
        for (const auto& [changed,rectangle] : changes.rectangles)
            if (!get(changed)) { error = "Native scroll shape target was removed during callback"; return false; }
        return completeShapes(changes,error);
    };
    setVisible(state.vertical,visible->vertical);
    const auto alive = [&]
    {
        const auto* current = get(id);
        const auto* content = get(state.document);
        return current && current->scrollContainer && current->scrollContainer->document == state.document &&
            content && content->parent == id && get(state.vertical) && get(state.horizontal);
    };
    if (!alive()) return true;
    if (visible->vertical) { if (!publish(verticalShapes)) return false; }
    else
    {
        setScrollPosition(state.vertical,0,true,error);
        if (!error.empty()) return false;
        if (!alive()) return true;
    }
    mNodes.at(state.document).params.rect.left = static_cast<std::int32_t>(left);
    mNodes.at(state.document).params.rect.right = static_cast<std::int32_t>(left+docWidth);
    if (visible->horizontal && resetHorizontal)
    {
        setScrollPosition(state.horizontal,0,true,error);
        if (!error.empty()) return false;
        if (!alive()) return true;
    }
    setVisible(state.horizontal,visible->horizontal);
    if (!alive()) return true;
    if (visible->horizontal) { if (!publish(horizontalShapes)) return false; }
    else
    {
        setScrollPosition(state.horizontal,0,true,error);
        if (!error.empty()) return false;
        if (!alive()) return true;
    }
    for (const auto [bar,documentSize,pageSize] : {
        std::tuple{state.horizontal,static_cast<std::int32_t>(docWidth),visible->width},
        std::tuple{state.vertical,static_cast<std::int32_t>(docHeight),visible->height}})
    {
        if (!setScrollDocumentSize(bar,documentSize,error)) return false;
        if (!alive()) return true;
        if (!setScrollPageSize(bar,pageSize,error)) return false;
        if (!alive()) return true;
    }
    return true;
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetTree::scrollContentWindow(Id id, std::string& error)
{
    if (!updateScrollContainer(id,error)) return std::nullopt;
    const auto* node = get(id);
    if (!node || !node->scrollContainer) { error = "Native scroll container disappeared during update"; return std::nullopt; }
    const auto state = *node->scrollContainer;
    const auto* border = get(state.border);
    const auto* horizontal = get(state.horizontal);
    const auto* document = get(state.document);
    if (!border || !border->border || !horizontal || !document)
    { error = "Native scroll window requires live content and chrome"; return std::nullopt; }
    const auto edge = border->params.visible ? border->border->params.thickness : 0;
    const auto setting = mSettings.find("UIScrollbarSize");
    LLVKScrollLayout::Params params;
    const auto& rect = node->params.rect;
    const auto width = std::int64_t(rect.right)-rect.left;
    const auto height = std::int64_t(rect.top)-rect.bottom;
    const auto docWidth = std::int64_t(document->params.rect.right)-document->params.rect.left;
    const auto docHeight = std::int64_t(document->params.rect.top)-document->params.rect.bottom;
    for (const auto extent : {width,height,docWidth,docHeight})
        if (extent < 0 || extent > INT32_MAX) { error = "Native scroll window extent is invalid"; return std::nullopt; }
    params.width = static_cast<std::int32_t>(width); params.height = static_cast<std::int32_t>(height);
    params.documentWidth = static_cast<std::int32_t>(docWidth); params.documentHeight = static_cast<std::int32_t>(docHeight);
    params.borderWidth = edge;
    params.scrollbarSize = state.useSizeSetting && setting != mSettings.end() ? setting->second.asInteger() : state.scrollbarSize;
    params.hideScrollbars = state.hideScrollbars;
    const auto visible = LLVKScrollLayout::visible(params,error);
    if (!visible) return std::nullopt;
    const auto bottom = visible->horizontal ? horizontal->params.rect.top : edge;
    const auto right = std::int64_t(edge)+visible->width;
    const auto top = std::int64_t(bottom)+visible->height;
    if (right < INT32_MIN || right > INT32_MAX || top < INT32_MIN || top > INT32_MAX)
    { error = "Native scroll content window overflows"; return std::nullopt; }
    return Rect{edge,bottom,static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetTree::scrollToReveal(Id id, const Rect& target,
    const Rect& constraint, std::string& error)
{
    error.clear();
    if (!validRect(target) || !validRect(constraint) || target.right < target.left || target.top < target.bottom ||
        constraint.right < constraint.left || constraint.top < constraint.bottom)
    { error = "Native scroll reveal rectangles are invalid"; return std::nullopt; }
    const auto contentWindow = scrollContentWindow(id,error);
    if (!contentWindow) return std::nullopt;
    const auto state = *get(id)->scrollContainer;
    const auto* document = get(state.document);
    if (!document || !get(state.vertical) || !get(state.horizontal))
    { error = "Native scroll reveal children are missing"; return std::nullopt; }
    const auto doc = document->params.rect;
    const auto width = std::int64_t(constraint.right)-constraint.left;
    const auto height = std::int64_t(constraint.top)-constraint.bottom;
    const auto bottom = std::max(std::int64_t(target.bottom),std::int64_t(target.top)-height);
    const auto right = std::min(std::int64_t(target.right),std::int64_t(target.left)+width);
    const auto windowHeight = std::int64_t(contentWindow->top)-contentWindow->bottom;
    const auto windowWidth = std::int64_t(contentWindow->right)-contentWindow->left;
    const auto documentSize = get(state.vertical)->scrollbar->documentSize;
    const auto minVertical = std::int64_t(documentSize)-(bottom-constraint.bottom+windowHeight);
    const auto maxVertical = std::int64_t(documentSize)-(std::int64_t(target.top)-constraint.top+windowHeight);
    const auto verticalPosition = std::clamp(std::int64_t(get(state.vertical)->scrollbar->position),minVertical,maxVertical);
    const auto minHorizontal = right-constraint.right;
    const auto maxHorizontal = std::int64_t(target.left)-constraint.left;
    for (const auto value : {bottom,right,windowWidth,windowHeight,verticalPosition,minHorizontal,maxHorizontal})
        if (value < INT32_MIN || value > INT32_MAX)
        { error = "Native scroll reveal arithmetic overflows"; return std::nullopt; }
    const auto live = [&]
    {
        if (get(id) && get(state.document) && get(state.vertical) && get(state.horizontal) && get(state.document)->parent == id) return true;
        error = "Native scroll reveal owner changed during callback";
        return false;
    };
    if (!setScrollDocumentSize(state.vertical,doc.top-doc.bottom,error) || !live() ||
        !setScrollPageSize(state.vertical,static_cast<std::int32_t>(windowHeight),error) || !live()) return std::nullopt;
    setScrollPosition(state.vertical,static_cast<std::int32_t>(verticalPosition),true,error);
    if (!error.empty() || !live()) return std::nullopt;
    const auto horizontalPosition = std::clamp(std::int64_t(get(state.horizontal)->scrollbar->position),minHorizontal,maxHorizontal);
    if (!setScrollDocumentSize(state.horizontal,doc.right-doc.left,error) || !live() ||
        !setScrollPageSize(state.horizontal,static_cast<std::int32_t>(windowWidth),error) || !live()) return std::nullopt;
    setScrollPosition(state.horizontal,static_cast<std::int32_t>(horizontalPosition),true,error);
    if (!error.empty() || !live() || !updateScrollContainer(id,error) || !live()) return std::nullopt;
    const auto screen = screenRect(id,error);
    if (!screen) return std::nullopt;
    const auto screenLeft = std::int64_t(screen->left)+target.left;
    const auto screenRight = std::int64_t(screen->left)+right;
    const auto screenBottom = std::int64_t(screen->bottom)+bottom;
    const auto screenTop = std::int64_t(screen->bottom)+target.top;
    for (const auto value : {screenLeft,screenRight,screenBottom,screenTop})
        if (value < INT32_MIN || value > INT32_MAX)
        { error = "Native scroll reveal parent notification rectangle overflows"; return std::nullopt; }
    return Rect{static_cast<std::int32_t>(screenLeft),static_cast<std::int32_t>(screenBottom),
        static_cast<std::int32_t>(screenRight),static_cast<std::int32_t>(screenTop)};
}

bool LLVKWidgetTree::finishScrollResize(Id id, std::string& error)
{
    const auto* node = get(id);
    if (!node || !node->scrollContainer) { error = "Native resized scroll container is missing"; return false; }
    const auto state = *node->scrollContainer;
    if (!state.document) return true;
    const auto* document = get(state.document);
    const auto* border = get(state.border);
    if (!document || document->parent != id || !border || !border->border)
    { error = "Native resized scroll container has detached content or border"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    const auto docWidth = std::int64_t(document->params.rect.right)-document->params.rect.left;
    const auto docHeight = std::int64_t(document->params.rect.top)-document->params.rect.bottom;
    for (const auto extent : {width,height,docWidth,docHeight})
        if (extent < 0 || extent > INT32_MAX) { error = "Invalid native scroll resize extent"; return false; }
    const auto setting = mSettings.find("UIScrollbarSize");
    LLVKScrollLayout::Params params;
    params.width = static_cast<std::int32_t>(width); params.height = static_cast<std::int32_t>(height);
    params.documentWidth = static_cast<std::int32_t>(docWidth); params.documentHeight = static_cast<std::int32_t>(docHeight);
    params.borderWidth = border->params.visible ? border->border->params.thickness : 0;
    params.scrollbarSize = state.useSizeSetting && setting != mSettings.end() ? setting->second.asInteger() : state.scrollbarSize;
    params.hideScrollbars = state.hideScrollbars;
    const auto visible = LLVKScrollLayout::visible(params,error);
    if (!visible) return false;
    if (visible->width < 0 || visible->height < 0) { error = "Native resized scroll page cannot be negative"; return false; }
    for (const auto [bar,documentSize,pageSize] : {
        std::tuple{state.vertical,params.documentHeight,visible->height},
        std::tuple{state.horizontal,params.documentWidth,visible->width}})
    {
        if (!setScrollDocumentSize(bar,documentSize,error)) return false;
        if (!get(id)) return true;
        if (!setScrollPageSize(bar,pageSize,error)) return false;
        if (!get(id)) return true;
    }
    return updateScrollContainer(id,error);
}

std::optional<LLVKWidgetTree::ScrollbarDraw> LLVKWidgetTree::prepareScrollbar(Id id, std::int32_t mouseX,
    std::int32_t mouseY, float frameDelta, LLVKColor::Value focusColor, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollbar || !std::isfinite(frameDelta) || frameDelta < 0.f)
    { error = "Invalid native scrollbar preparation inputs"; return std::nullopt; }
    const auto& state = *node->scrollbar;
    const auto params = state.params;
    for (const auto& color : {focusColor,params->trackColor.get(),params->thumbColor.get(),params->backgroundColor.get()})
        for (const float channel : color)
            if (!std::isfinite(channel)) { error = "Native scrollbar color is nonfinite"; return std::nullopt; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    ScrollbarDraw result;
    if (width <= 0 || height <= 0) return result;
    const auto screen = screenRect(id,error);
    if (!screen) return std::nullopt;
    const auto append = [&](std::int64_t left, std::int64_t bottom, std::int64_t right, std::int64_t top,
        LLVKColor::Value color, std::shared_ptr<const LLVKWidgetImage> image = {}, bool solid = false, bool additive = false)
    {
        left += screen->left; right += screen->left; bottom += screen->bottom; top += screen->bottom;
        for (const auto coordinate : {left,bottom,right,top})
            if (coordinate < INT32_MIN || coordinate > INT32_MAX)
            { error = "Native scrollbar draw rectangle overflows"; return false; }
        result.primitives.push_back({{static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),
            static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)},color,std::move(image),solid,additive});
        return true;
    };
    if (params->backgroundVisible && !append(0,0,width,height,params->backgroundColor.get())) return std::nullopt;
    const auto& thumb = state.thumb;
    const auto localX = std::int64_t(mouseX)-screen->left, localY = std::int64_t(mouseY)-screen->bottom;
    const bool hovered = node->params.enabled && (!mMouseCapture || mMouseCapture == id) &&
        (mMouseCapture == id || (localX >= thumb.left && localX < thumb.right && localY >= thumb.bottom && localY < thumb.top));
    const float interpolant = std::clamp(1.f-std::pow(2.f,-frameDelta/0.05f),0.f,1.f);
    const float glow = state.glow+((hovered ? 0.15f : 0.f)-state.glow)*interpolant;
    const bool fallback = params->vertical ? !params->thumbVertical || !params->thumbHorizontal :
        !params->trackHorizontal || !params->trackVertical;
    const auto thickness = std::int64_t(state.thickness);
    if (fallback)
    {
        if (!append(params->vertical ? 0 : thickness,params->vertical ? thickness : 0,
            params->vertical ? width : width-2*thickness,params->vertical ? height-2*thickness : height,params->trackColor.get()) ||
            !append(thumb.left,thumb.bottom,thumb.right,thumb.top,params->thumbColor.get())) return std::nullopt;
    }
    else
    {
        const auto trackImage = params->vertical ? params->trackVertical : params->trackHorizontal;
        const auto thumbImage = params->vertical ? params->thumbVertical : params->thumbHorizontal;
        if (!trackImage || !thumbImage)
        { error = "Native scrollbar image branch requires its selected track and thumb"; return std::nullopt; }
        if (!append(params->vertical ? 0 : thickness,params->vertical ? thickness : 0,
            params->vertical ? width : width-thickness,params->vertical ? height-thickness : height,
            params->trackColor.get(),trackImage,true)) return std::nullopt;
        if (mKeyboardFocus == id && !append(std::int64_t(thumb.left)-2,std::int64_t(thumb.bottom)-2,
            std::int64_t(thumb.right)+2,std::int64_t(thumb.top)+2,focusColor,trackImage)) return std::nullopt;
        if (!append(thumb.left,thumb.bottom,thumb.right,thumb.top,params->thumbColor.get(),thumbImage)) return std::nullopt;
        if (glow > 0.01f && !append(thumb.left,thumb.bottom,thumb.right,thumb.top,{1,1,1,glow},thumbImage,true,true)) return std::nullopt;
    }
    result.children = node->children;
    mNodes.at(id).scrollbar->glow = glow;
    return result;
}

bool LLVKWidgetTree::advanceScrollFrame(Id id, float frameDelta, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer || !std::isfinite(frameDelta) || frameDelta < 0.f)
    { error = "Invalid native scroll frame inputs"; return false; }
    auto& state = *mNodes.at(id).scrollContainer;
    state.autoRate = state.autoScrolling ? std::min(state.autoRate+frameDelta*120.f,state.maxAutoRate) : state.minAutoRate;
    state.autoScrolling = false;
    return true;
}

bool LLVKWidgetTree::autoScroll(Id id, std::int32_t x, std::int32_t y, const Rect& rootInLocal,
    float frameDelta, bool apply, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer || !std::isfinite(frameDelta) || frameDelta < 0.f ||
        !validRect(rootInLocal) || rootInLocal.right < rootInLocal.left || rootInLocal.top < rootInLocal.bottom)
    { error = "Invalid native auto-scroll inputs"; return false; }
    const auto state = *node->scrollContainer;
    if (!apply && state.autoScrolling) return true;
    const auto* horizontal = get(state.horizontal);
    const auto* vertical = get(state.vertical);
    const auto* border = get(state.border);
    if (!horizontal || !horizontal->scrollbar || !vertical || !vertical->scrollbar || !border || !border->border)
    { error = "Native auto-scroll chrome is missing"; return false; }
    if (!horizontal->params.visible && !vertical->params.visible) return false;
    const auto edge = border->params.visible ? border->border->params.thickness : 0;
    const auto setting = mSettings.find("UIScrollbarSize");
    const auto size = state.useSizeSetting && setting != mSettings.end() ? setting->second.asInteger() : state.scrollbarSize;
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left-2*std::int64_t(edge);
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom-2*std::int64_t(edge);
    const auto left = std::max<std::int64_t>(0,rootInLocal.left);
    const auto right = std::min(width-(vertical->params.visible ? size : 0),std::int64_t(rootInLocal.right));
    const auto bottom = std::max<std::int64_t>(horizontal->params.visible ? size : 0,rootInLocal.bottom);
    const auto top = std::min(height,std::int64_t(rootInLocal.top));
    const auto zoneWidth = std::min((right-left)/3,std::int64_t(state.maxAutoZone));
    const auto zoneHeight = std::min((top-bottom)/3,std::int64_t(state.maxAutoZone));
    const double speedValue = std::floor(double(state.autoRate*frameDelta)+0.5);
    if (!std::isfinite(speedValue) || speedValue > INT32_MAX)
    { error = "Native auto-scroll speed overflows"; return false; }
    const auto speed = static_cast<std::int32_t>(speedValue);
    bool scrolling = false;
    const bool inRoot = x >= rootInLocal.left && x < rootInLocal.right && y >= rootInLocal.bottom && y < rootInLocal.top;
    const auto move = [&](Id bar, bool zone, bool increase)
    {
        if (!zone || !get(id) || !get(bar)) return true;
        const auto& range = *get(bar)->scrollbar;
        const auto maximum = std::max(0,range.documentSize-range.pageSize);
        if (increase ? range.position >= maximum : range.position <= 0) return true;
        scrolling = true;
        if (apply)
        {
            const auto position = std::int64_t(range.position)+(increase ? std::int64_t(speed) : -std::int64_t(speed));
            setScrollPosition(bar,static_cast<std::int32_t>(std::clamp<std::int64_t>(position,INT32_MIN,INT32_MAX)),true,error);
            if (!error.empty()) return false;
            if (get(id)) mNodes.at(id).scrollContainer->autoScrolling = true;
        }
        return true;
    };
    const bool horizontalVisible = horizontal->params.visible, verticalVisible = vertical->params.visible;
    if (horizontalVisible && (!move(state.horizontal,inRoot && x < left+zoneWidth,false) ||
        !move(state.horizontal,inRoot && x >= right-zoneWidth,true))) return false;
    if (verticalVisible && (!move(state.vertical,inRoot && y < bottom+zoneHeight,true) ||
        !move(state.vertical,inRoot && y >= top-zoneHeight,false))) return false;
    return scrolling;
}

std::optional<LLVKWidgetTree::ScrollContainerDraw> LLVKWidgetTree::prepareScrollContainer(Id id,
    float transparency, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer || !std::isfinite(transparency))
    { error = "Invalid native scroll container preparation inputs"; return std::nullopt; }
    if (!hasAncestor(mKeyboardFocus,id) &&
        (mMouseCapture == node->scrollContainer->vertical || mMouseCapture == node->scrollContainer->horizontal))
    {
        focusFirst(id,true,error);
        if (!error.empty()) return std::nullopt;
        node = get(id);
        if (!node) { error = "Native scroll container removed during focus transfer"; return std::nullopt; }
    }
    ScrollContainerDraw result;
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width <= 0 || height <= 0) return result;
    std::optional<Rect> contentWindow;
    if (node->scrollContainer->document)
    {
        contentWindow = scrollContentWindow(id,error);
        if (!contentWindow) return std::nullopt;
    }
    node = get(id);
    if (!node || !node->scrollContainer) { error = "Native scroll preparation owner disappeared"; return std::nullopt; }
    const auto state = *node->scrollContainer;
    const auto* border = get(state.border);
    const auto* vertical = get(state.vertical);
    const auto* horizontal = get(state.horizontal);
    if (!border || !border->border || !vertical || !horizontal)
    { error = "Native scroll preparation chrome is missing"; return std::nullopt; }
    const auto screen = screenRect(id,error);
    if (!screen) return std::nullopt;
    const auto edge = border->params.visible ? border->border->params.thickness : 0;
    const auto setting = mSettings.find("UIScrollbarSize");
    const auto size = state.useSizeSetting && setting != mSettings.end() ? setting->second.asInteger() : state.scrollbarSize;
    const auto rectangle = [&](std::int64_t left, std::int64_t bottom, std::int64_t right, std::int64_t top, Rect& output)
    {
        left += screen->left; right += screen->left; bottom += screen->bottom; top += screen->bottom;
        for (const auto value : {left,bottom,right,top})
            if (value < INT32_MIN || value > INT32_MAX) { error = "Native scroll prepared rectangle overflows"; return false; }
        output = {static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)};
        return true;
    };
    result.backgroundVisible = state.opaque;
    result.backgroundColor = state.backgroundColor.get();
    result.backgroundColor[3] *= transparency;
    for (float channel : result.backgroundColor)
        if (!std::isfinite(channel)) { error = "Native scroll background color is nonfinite"; return std::nullopt; }
    if (!rectangle(edge,edge,width-edge,height-edge,result.background)) return std::nullopt;
    result.document = state.document;
    if (contentWindow)
    {
        const auto bottom = std::int64_t(edge)+(horizontal->params.visible ? size : 0);
        const auto clipHeight = std::int64_t(contentWindow->top)-contentWindow->bottom;
        if (!rectangle(edge,bottom,width-edge-(vertical->params.visible ? size : 0),bottom+clipHeight,result.documentClip)) return std::nullopt;
    }
    for (auto child = node->children.rbegin(); child != node->children.rend(); ++child)
        if (*child != state.document && get(*child) && get(*child)->params.visible) result.chrome.push_back(*child);
    if (border->params.visible) mNodes.at(state.border).border->keyboardFocus = hasAncestor(mKeyboardFocus,id);
    return result;
}

bool LLVKWidgetTree::scrollContainerKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->scrollContainer) { error = "Native scroll key target is not a container"; return false; }
    const auto state = *node->scrollContainer;
    if (state.ignoreArrowKeys) return false;
    const auto* document = get(state.document);
    bool handled = false;
    if (document && document->parent == id)
    {
        if (document->scrollContainer) handled = scrollContainerKey(state.document,key,modifiers,error);
        else if (document->scrollbar) handled = scrollbarKey(state.document,key,error);
        else if (document->lineEditor)
        {
            LLVKLineEditor::Key editorKey;
            switch (key)
            {
                case ScrollKey::Home: editorKey = LLVKLineEditor::Key::Home; break;
                case ScrollKey::End: editorKey = LLVKLineEditor::Key::End; break;
                case ScrollKey::Up: editorKey = LLVKLineEditor::Key::Up; break;
                case ScrollKey::Down: editorKey = LLVKLineEditor::Key::Down; break;
                case ScrollKey::PageUp: editorKey = LLVKLineEditor::Key::PageUp; break;
                case ScrollKey::PageDown: editorKey = LLVKLineEditor::Key::PageDown; break;
                case ScrollKey::Left: editorKey = LLVKLineEditor::Key::Left; break;
                case ScrollKey::Right: editorKey = LLVKLineEditor::Key::Right; break;
                default: error = "Invalid native scroll navigation key"; return false;
            }
            handled = lineEditorKey(state.document,editorKey,modifiers,error);
        }
        if (!error.empty() || handled || !get(id)) return error.empty() && (handled || !get(id));
    }
    for (const Id scrollbar : {state.vertical,state.horizontal})
    {
        handled = scrollbarKey(scrollbar,key,error);
        if (!error.empty()) return false;
        if (!get(id)) return true;
        if (handled) return updateScrollContainer(id,error);
    }
    return false;
}
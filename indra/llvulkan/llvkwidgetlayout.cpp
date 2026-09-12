#include "llvkwidgetlayout.h"
#include <cmath>
std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createLayoutStack(const Params& view, bool vertical,
    std::int32_t spacing, bool clip, Id parent, std::string& error)
{
    if (spacing == -1)
    {
        const auto setting = mSettings.find("UIResizeBarHeight");
        spacing = setting == mSettings.end() ? 0 : setting->second.asInteger();
    }
    if (spacing < 0) { error = "Native layout stack spacing cannot be negative"; return std::nullopt; }
    const auto id = create(view,parent,error);
    if (id) mNodes.at(*id).layoutStack = Node::LayoutStack{vertical,clip,spacing,{}};
    return id;
}

bool LLVKWidgetTree::attachLayoutPanel(Id stack, Id panel, const Node::LayoutPanel& params, std::string& error)
{
    error.clear();
    const auto* owner = get(stack);
    const auto* child = get(panel);
    if (!owner || !owner->layoutStack || !child || !child->panel || params.minimum < 0 ||
        params.expandedMinimum < 0 || params.maximum < params.minimum)
    { error = "Invalid native layout panel owner or dimensions"; return false; }
    auto state = params;
    state.visibleAmount = child->params.visible ? 1.f : 0.f;
    const auto dimension = owner->layoutStack->vertical ? std::int64_t(child->params.rect.top)-child->params.rect.bottom :
        std::int64_t(child->params.rect.right)-child->params.rect.left;
    if (dimension < 0 || dimension > INT32_MAX) { error = "Invalid native layout panel extent"; return false; }
    state.target = std::min(std::max(static_cast<std::int32_t>(dimension),state.minimum),state.maximum);
    auto panels = owner->layoutStack->panels;
    std::erase(panels,panel);
    panels.push_back(panel);
    if (!reparent(panel,stack,false,child->params.tabGroup.value_or(0),error)) return false;
    mNodes.at(panel).layoutPanel = state;
    mNodes.at(panel).params.follows = 0;
    mNodes.at(stack).layoutStack->panels = std::move(panels);
    mNodes.at(stack).layoutStack->needsLayout = true;
    float total = 0.f;
    for (const Id current : mNodes.at(stack).layoutStack->panels)
    {
        auto& node = mNodes.at(current);
        if (!node.layoutPanel->autoResize) continue;
        const auto dim = owner->layoutStack->vertical ? std::int64_t(node.params.rect.top)-node.params.rect.bottom :
            std::int64_t(node.params.rect.right)-node.params.rect.left;
        node.layoutPanel->fraction = std::max(0.00001f,float(dim-node.layoutPanel->expandedMinimum));
        total += node.layoutPanel->fraction;
    }
    float normalized = 0.f;
    for (const Id current : mNodes.at(stack).layoutStack->panels)
    {
        auto& panelState = *mNodes.at(current).layoutPanel;
        if (panelState.autoResize) { panelState.fraction = std::clamp(panelState.fraction/total,0.00001f,1.f); normalized += panelState.fraction; }
    }
    for (const Id current : mNodes.at(stack).layoutStack->panels)
        if (mNodes.at(current).layoutPanel->autoResize) mNodes.at(current).layoutPanel->fraction /= normalized;
    return true;
}

bool LLVKWidgetTree::configureLayoutStack(Id id, bool animate, float openTime, float closeTime, std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->layoutStack || !std::isfinite(openTime) || !std::isfinite(closeTime) || openTime < 0.f || closeTime < 0.f)
    { error = "Invalid native layout animation configuration"; return false; }
    auto& stack = *mNodes.at(id).layoutStack;
    stack.animate = animate; stack.openTime = openTime; stack.closeTime = closeTime;
    stack.needsLayout = true;
    return true;
}

bool LLVKWidgetTree::updateLayoutStack(Id id, std::string& error, float frameDelta)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->layoutStack) { error = "Native layout stack is missing"; return false; }
    if (!std::isfinite(frameDelta) || frameDelta < 0.f) { error = "Invalid native layout frame delta"; return false; }
    const auto stack = *node->layoutStack;
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width < 0 || height < 0 || width > INT32_MAX || height > INT32_MAX)
    { error = "Invalid native layout stack extent"; return false; }
    std::map<Id,std::int64_t> targets;
    std::map<Id,float> visibility;
    bool animated = false, continuing = false;
    auto remaining = stack.vertical ? height : width;
    float totalFraction = 0.f;
    for (const Id panel : stack.panels)
    {
        const auto* child = get(panel);
        if (!child || child->parent != id || !child->layoutPanel)
        { error = "Native layout panel is detached"; return false; }
        const auto& state = *child->layoutPanel;
        float amount = state.visibleAmount;
        const float targetAmount = child->params.visible ? 1.f : 0.f;
        if (amount != targetAmount)
        {
            if (!stack.animate) amount = targetAmount;
            else
            {
                if (!animated)
                {
                    const float time = child->params.visible ? stack.openTime : stack.closeTime;
                    const float interpolant = time == 0.f ? 1.f : std::clamp(1.f-std::pow(2.f,-frameDelta/time),0.f,1.f);
                    amount += (targetAmount-amount)*interpolant;
                    if ((child->params.visible && amount > 0.99f) || (!child->params.visible && amount < 0.001f)) amount = targetAmount;
                }
                continuing = true;
            }
            animated = true;
        }
        visibility[panel] = amount;
        targets[panel] = state.autoResize ? state.expandedMinimum : state.target;
        remaining -= static_cast<std::int64_t>(std::floor(amount*float(targets[panel])+0.5f)) +
            static_cast<std::int64_t>(std::floor(amount*float(stack.spacing)+0.5f));
        if (state.autoResize) totalFraction += state.fraction*amount;
    }
    if (!stack.panels.empty()) remaining += static_cast<std::int64_t>(std::floor(float(stack.spacing)*visibility.at(stack.panels.back())+0.5f));
    const auto extra = remaining;
    if (extra > 0 && totalFraction > 0.f)
        for (const Id panel : stack.panels)
        {
            const auto& child = *get(panel);
            const auto& state = *child.layoutPanel;
            if (!state.autoResize) continue;
            const auto delta = static_cast<std::int64_t>(std::floor(float(extra)*(state.fraction*visibility.at(panel)/totalFraction)+0.5f));
            targets[panel] = std::min(targets[panel]+delta,std::int64_t(state.maximum));
            remaining -= delta;
        }
    for (const Id panel : stack.panels)
    {
        if (!remaining) break;
        const auto& child = *get(panel);
        if (child.layoutPanel->autoResize && child.params.visible)
        {
            const auto delta = remaining > 0 ? 1 : -1;
            targets[panel] = std::min(targets[panel]+delta,std::int64_t(child.layoutPanel->maximum));
            remaining -= delta;
        }
    }
    ShapeChanges changes;
    double position = stack.vertical ? double(height) : 0.0;
    for (const Id panel : stack.panels)
    {
        const auto& child = *get(panel);
        const auto dimension = std::max(std::int64_t(child.layoutPanel->expandedMinimum),targets[panel]);
        const auto origin = std::floor(position+0.5);
        const auto bottom = stack.vertical ? origin-double(dimension) : 0.0;
        const auto left = stack.vertical ? 0.0 : origin;
        if (left < INT32_MIN || left > INT32_MAX || bottom < INT32_MIN || bottom > INT32_MAX ||
            targets[panel] < INT32_MIN || targets[panel] > INT32_MAX)
        { error = "Native layout stack position overflows"; return false; }
        if (!planReshape(panel,stack.vertical ? width : dimension,stack.vertical ? dimension : height,
            {static_cast<std::int32_t>(left),static_cast<std::int32_t>(bottom),0,0},changes,error)) return false;
        position += (stack.vertical ? -1 : 1)*(std::floor(visibility.at(panel)*float(targets[panel])+0.5f)+visibility.at(panel)*float(stack.spacing));
    }
    if (!completeShapes(changes,error)) return false;
    for (const auto& [panel,target] : targets)
        if (get(panel) && get(panel)->layoutPanel)
        {
            mNodes.at(panel).layoutPanel->target = static_cast<std::int32_t>(target);
            mNodes.at(panel).layoutPanel->visibleAmount = visibility.at(panel);
        }
    if (get(id) && get(id)->layoutStack) mNodes.at(id).layoutStack->needsLayout = continuing;
    return true;
}

bool LLVKWidgetTree::layoutStackPointer(Id id,const PointerEvent& event,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->layoutStack) return false;
    auto& stack=*mNodes.at(id).layoutStack;
    if (stack.resizeFirst && mouseCapture()!=id) stack.resizeFirst=stack.resizeSecond=0;
    const auto coordinate=stack.vertical ? -std::int64_t(event.y) : std::int64_t(event.x);
    if (stack.resizeFirst)
    {
        if (event.kind==PointerKind::LeftUp)
        { stack.resizeFirst=stack.resizeSecond=0; setMouseCapture(0,error); return true; }
        if (event.kind!=PointerKind::Hover) return true;
        const auto first=get(stack.resizeFirst),second=get(stack.resizeSecond);
        if (!first || !second || !first->params.visible || !second->params.visible)
        { stack.resizeFirst=stack.resizeSecond=0; setMouseCapture(0,error); return true; }
        const auto& firstState=*first->layoutPanel;
        const auto& secondState=*second->layoutPanel;
        const auto total=std::int64_t(stack.resizeFirstSize)+stack.resizeSecondSize;
        const auto minimum=std::max<std::int64_t>(std::max(firstState.minimum,firstState.expandedMinimum),total-secondState.maximum);
        const auto maximum=std::min<std::int64_t>(firstState.maximum,total-std::max(secondState.minimum,secondState.expandedMinimum));
        if (minimum>maximum) return true;
        const auto size=std::clamp(std::int64_t(stack.resizeFirstSize)+coordinate-stack.resizeOrigin,minimum,maximum);
        mNodes.at(stack.resizeFirst).layoutPanel->target=static_cast<int>(size);
        mNodes.at(stack.resizeSecond).layoutPanel->target=static_cast<int>(total-size);
        float totalWeight=0;
        for (const auto panel : stack.panels)
        {
            auto& state=*mNodes.at(panel).layoutPanel;
            if (!state.autoResize) continue;
            state.fraction=std::max(.00001f,static_cast<float>(state.target-state.expandedMinimum));
            totalWeight+=state.fraction;
        }
        if (totalWeight>0)
            for (const auto panel : stack.panels)
                if (auto& state=*mNodes.at(panel).layoutPanel; state.autoResize) state.fraction/=totalWeight;
        stack.needsLayout=true;
        return updateLayoutStack(id,error);
    }
    if (event.kind!=PointerKind::LeftDown && event.kind!=PointerKind::Hover) return false;
    Id previous=0;
    for (const auto panel : stack.panels)
    {
        const auto* current=get(panel);
        if (!current || !current->params.visible) continue;
        if (previous)
        {
            const auto* first=get(previous);
            const auto& firstState=*first->layoutPanel;
            const auto& secondState=*current->layoutPanel;
            const auto start=stack.vertical ? -first->params.rect.bottom : first->params.rect.right;
            const auto end=stack.vertical ? -current->params.rect.top : current->params.rect.left;
            if (coordinate>=start && coordinate<end && (firstState.userResize || secondState.userResize) &&
                (firstState.autoResize || firstState.userResize) && (secondState.autoResize || secondState.userResize))
            {
                if (event.kind==PointerKind::LeftDown)
                {
                    stack.resizeFirst=previous; stack.resizeSecond=panel; stack.resizeOrigin=static_cast<int>(coordinate);
                    stack.resizeFirstSize=firstState.target; stack.resizeSecondSize=secondState.target;
                    if (!setMouseCapture(id,error)) { stack.resizeFirst=stack.resizeSecond=0; return false; }
                }
                return true;
            }
        }
        previous=panel;
    }
    return false;
}

bool LLVKWidgetTree::prepareLayoutStacks(Id root, float frameDelta, std::string& error)
{
    error.clear();
    if (!get(root) || !std::isfinite(frameDelta) || frameDelta < 0.f)
    { error = "Invalid native layout preparation root or delta"; return false; }
    const auto visit = [&](auto&& self, Id id) -> bool
    {
        const auto* node = get(id);
        if (!node) return true;
        if (node->layoutStack && node->layoutStack->needsLayout && !updateLayoutStack(id,error,frameDelta)) return false;
        node = get(id);
        if (!node) return true;
        const auto children = node->children;
        for (const Id child : children)
            if (get(child) && get(child)->parent == id && !self(self,child)) return false;
        return true;
    };
    return visit(visit,root);
}

namespace
{
    bool fits(std::int64_t value) { return value >= INT32_MIN && value <= INT32_MAX; }
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetLayout::resolve(std::string& error) const
{
    error.clear();
    for (const auto* field : {&left,&right,&bottom,&top,&width,&height})
        if (!fits(field->value)) { error = "Native rectangle parameter outside 32-bit range"; return std::nullopt; }
    auto resolvedLeft = left.value;
    auto resolvedRight = right.value;
    auto resolvedBottom = bottom.value;
    auto resolvedTop = top.value;
    if (!(left.provided && right.provided) && width.provided)
    {
        if (right.provided) resolvedLeft = right.value-width.value;
        else resolvedRight = left.value+width.value;
    }
    if (!(bottom.provided && top.provided) && height.provided)
    {
        if (top.provided) resolvedBottom = top.value-height.value;
        else resolvedTop = bottom.value+height.value;
    }
    if (!fits(resolvedLeft) || !fits(resolvedRight) || !fits(resolvedBottom) || !fits(resolvedTop) ||
        !fits(resolvedRight-resolvedLeft) || !fits(resolvedTop-resolvedBottom))
    { error = "Native widget rectangle constraint overflow"; return std::nullopt; }
    return LLVKWidgetTree::Rect{static_cast<std::int32_t>(resolvedLeft),static_cast<std::int32_t>(resolvedBottom),
                               static_cast<std::int32_t>(resolvedRight),static_cast<std::int32_t>(resolvedTop)};
}

std::optional<LLVKWidgetTree::Rect> LLVKWidgetLayout::apply(const LLVKWidgetTree& tree,
    LLVKWidgetTree::Id parent, const std::string& parentLayout, std::string& error,
    std::optional<LLVKWidgetTree::Rect> layoutRect) const
{
    error.clear();
    if (!parent) return resolve(error);
    for (const auto* field : {&left,&right,&bottom,&top,&width,&height,&bottomDelta,&topPad,&topDelta,&leftPad,&leftDelta})
        if (!fits(field->value)) { error = "Native layout parameter outside 32-bit range"; return std::nullopt; }
    const auto* owner = tree.get(parent);
    if (!owner) { error = "Native layout parent is missing"; return std::nullopt; }
    auto params = *this;
    if (params.layout.empty()) params.layout = parentLayout;
    const auto parentWidth = std::int64_t(owner->params.rect.right)-owner->params.rect.left;
    const auto parentHeight = std::int64_t(owner->params.rect.top)-owner->params.rect.bottom;
    const LLVKWidgetTree::Rect local{0,0,static_cast<std::int32_t>(parentWidth),static_cast<std::int32_t>(parentHeight)};
    if (!layoutRect || layoutRect->left == layoutRect->right || layoutRect->bottom == layoutRect->top) layoutRect = local;
    const auto area = *layoutRect;
    const bool topLeft = params.layout == "topleft";
    for (Value* value : {&params.left,&params.right})
        if (value->provided) value->value += value->value >= 0 ? area.left : area.right;
    for (Value* value : {&params.bottom,&params.top})
    {
        if (value->provided)
        {
            value->value += value->value >= 0 ? area.bottom : area.top;
            if (topLeft) value->value = std::int64_t(area.bottom)+area.top-value->value;
        }
    }
    if (!params.height.provided && !params.top.provided && params.height.value == 0) params.height = {10,true};
    std::int64_t defaultLeft = 0;
    std::int64_t defaultBottom = parentHeight;
    std::int64_t defaultRight = parentWidth;
    std::int64_t defaultTop = parentHeight*2;
    for (auto child : owner->children)
    {
        const auto* previous = tree.get(child);
        if (previous->params.fromDeclaration)
        {
            defaultLeft = previous->params.rect.left;
            defaultBottom = previous->params.rect.bottom;
            defaultRight = previous->params.rect.right;
            defaultTop = previous->params.rect.top;
            break;
        }
    }
    if (topLeft)
    {
        if (params.bottomDelta.provided) params.bottomDelta.value = -params.bottomDelta.value;
        else if (params.topPad.provided) params.bottomDelta = {-(params.height.value+params.topPad.value),true};
        else if (params.topDelta.provided)
            params.bottomDelta = {-(params.topDelta.value+params.height.value-(defaultTop-defaultBottom)),true};
        else if (!params.leftDelta.provided && !params.leftPad.provided)
            params.bottomDelta = {-(params.height.value+4),false};
        else params.bottomDelta = {0,false};
        if (!params.leftDelta.provided) params.leftDelta = {0,false};
        if (params.leftPad.provided) params.leftDelta = {params.leftPad.value+defaultRight-defaultLeft,false};
    }
    else
    {
        if (!params.bottomDelta.provided) params.bottomDelta = {-(params.height.value+4),false};
        if (!params.leftDelta.provided) params.leftDelta = {0,false};
    }
    defaultLeft += params.leftDelta.value;
    defaultRight += params.leftDelta.value;
    defaultBottom += params.bottomDelta.value;
    defaultTop += params.bottomDelta.value;
    if (params.bottomDelta.provided) params.bottom = {0,false};
    if (params.leftDelta.provided) params.left = {0,false};
    if (!params.left.provided) params.left = {defaultLeft,false};
    if (!params.right.provided) params.right = {defaultRight,false};
    if (!params.bottom.provided) params.bottom = {defaultBottom,false};
    if (!params.top.provided) params.top = {defaultTop,false};
    if (!params.width.provided) params.width = {defaultRight-defaultLeft,false};
    if (!params.height.provided) params.height = {defaultTop-defaultBottom,false};
    return params.resolve(error);
}
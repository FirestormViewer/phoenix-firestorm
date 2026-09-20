#include "llvkwidgettree.h"

#include <cmath>

std::optional<std::string> LLVKWidgetTree::findHelpTopic(Id id) const
{
    const auto* origin=get(id);
    if (!origin || !origin->control) return std::nullopt;
    std::vector<Id> descendants=origin->children;
    for (std::size_t index=0;index<descendants.size();++index)
        if (const auto* child=get(descendants[index]))
            descendants.insert(descendants.end(),child->children.begin(),child->children.end());
    for (const auto* owner=origin;owner;owner=get(owner->parent))
    {
        if (!owner->panel) continue;
        for (const auto childId : descendants)
            if (const auto* child=get(childId); child && child->panel && visibleInChain(childId) && !child->panel->params.helpTopic.empty())
                return child->panel->params.helpTopic;
        for (const auto childId : descendants)
        {
            const auto* child=get(childId);
            if (!child || !child->tabContainer || !child->params.visible) continue;
            const auto* selected=get(child->tabContainer->selected);
            if (selected && selected->panel && !selected->panel->params.helpTopic.empty()) return selected->panel->params.helpTopic;
        }
        if (!owner->panel->params.helpTopic.empty()) return owner->panel->params.helpTopic;
    }
    return std::nullopt;
}

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
    using Position = Node::TabContainer::Layout::Position;
    const auto position = owner->tabContainer->layout ? owner->tabContainer->layout->position : Position::Top;
    if (position == Position::Left)
    {
        if (key == ScrollKey::Up || key == ScrollKey::Down)
        { moveTab(id,key == ScrollKey::Down,error); return error.empty(); }
        if (key == ScrollKey::Right)
        {
            if (get(owner->tabContainer->selected)) requestControlFocus(owner->tabContainer->selected,true,error);
            return error.empty();
        }
        return key == ScrollKey::Left;
    }
    if (horizontal) { moveTab(id,key == ScrollKey::Right,error); return error.empty(); }
    if ((key == ScrollKey::Down && position == Position::Top) || (key == ScrollKey::Up && position == Position::Bottom))
    {
        if (get(owner->tabContainer->selected)) requestControlFocus(owner->tabContainer->selected,true,error);
        return error.empty();
    }
    return key == ScrollKey::Up || key == ScrollKey::Down;
}

bool LLVKWidgetTree::layoutTopTabs(Id container, const Node::TabContainer::Layout& layout, std::string& error)
{
    auto top = layout;
    top.position = Node::TabContainer::Layout::Position::Top;
    return layoutTabPanels(container,top,error);
}

bool LLVKWidgetTree::layoutTabPanels(Id container, const Node::TabContainer::Layout& layout, std::string& error,float frameDelta,bool positionButtons)
{
    error.clear();
    const auto* owner = get(container);
    if (!owner || !owner->tabContainer || layout.tabHeight <= 0 || layout.minimumWidth < 0 ||
        layout.maximumWidth < layout.minimumWidth || layout.labelPadding < 0 || layout.horizontalPadding < 0 ||
        layout.panelOverlap < 0 || layout.panelOverlap > layout.tabHeight || layout.verticalHeight <= 0 ||
        layout.verticalPadding < 0 || layout.rightPadding < 0 || layout.verticalArrowSize < 0 ||
        layout.horizontalArrowSize < 0 || layout.partialTabWidth < 0 || !std::isfinite(frameDelta) || frameDelta<0.f)
    { error = "Invalid native tab layout"; return false; }
    using Position = Node::TabContainer::Layout::Position;
    if (layout.position != Position::Top && layout.position != Position::Bottom && layout.position != Position::Left)
    { error = "Invalid native tab position"; return false; }
    const bool vertical = layout.position == Position::Left;
    const auto width = std::int64_t(owner->params.rect.right)-owner->params.rect.left;
    const auto height = std::int64_t(owner->params.rect.top)-owner->params.rect.bottom;
    auto tabs = owner->tabContainer->tabs;
    std::erase_if(tabs,[&](const auto& tab) { return owner->tabContainer->hiddenPanels.contains(tab.panel); });
    const auto top = layout.hidden ? height : layout.position == Position::Top ? height-1-layout.tabHeight+layout.panelOverlap : height-1;
    const auto bottom = !layout.hidden && layout.position == Position::Bottom ? layout.tabHeight-layout.panelOverlap : 1;
    const auto left = vertical && !layout.hidden ? std::int64_t(layout.minimumWidth)+layout.rightPadding+2+layout.verticalPadding : layout.panelOffset ? 3 : 1;
    const auto right = width-(!vertical && layout.panelOffset ? 2 : 1);
    if (top < bottom || right < left)
    {
        error = "Native tab container is too small for its content: "+owner->params.name+
            " ("+std::to_string(width)+"x"+std::to_string(height)+")";
        return false;
    }
    struct Placement { Id panel, button; Rect content, tab; };
    std::vector<Placement> placements;
    const auto rowHeight=std::int64_t(layout.verticalHeight)+layout.verticalPadding;
    const auto arrowSize=layout.verticalArrowSize ? layout.verticalArrowSize : setting("UITabCntrvArrowBtnSize").value_or(LLSD(0)).asInteger();
    std::int32_t maximumScroll=0;
    if (vertical && !layout.hidden && rowHeight*static_cast<std::int64_t>(tabs.size())>height-1)
    {
        if (arrowSize<=0 || height<=2*(std::int64_t(arrowSize)+3*layout.verticalPadding))
        { error="Native vertical tab overflow requires space for its arrow controls"; return false; }
        const auto available=height-2*(std::int64_t(arrowSize)+3*layout.verticalPadding);
        const auto needed=rowHeight*static_cast<std::int64_t>(tabs.size())-available;
        maximumScroll=static_cast<std::int32_t>(std::min<std::int64_t>(tabs.size()-1,(needed+rowHeight-1)/rowHeight));
    }
    auto scrollPosition=std::clamp(owner->tabContainer->scrollPosition,0,maximumScroll);
    std::int32_t scrollPixels=0;
    std::int32_t targetScrollPixels=0;
    std::int64_t next = vertical ? height-3-(maximumScroll ? arrowSize : 0)+rowHeight*scrollPosition : 1+std::int64_t(layout.horizontalPadding);
    for (const auto& entry : tabs)
    {
        const auto* panel = get(entry.panel);
        const auto* button = get(entry.button);
        if (!panel || panel->parent != container || !button || !button->button || button->parent != container) continue;
        const auto& label = button->button->params.label;
        const auto measured = button->control->params.font->measureRun(label,0,label.size(),1.f,true,false,error);
        if (!measured) return false;
        const double padded = std::floor(measured->width+0.5f)+layout.labelPadding;
        if (!std::isfinite(padded) || padded > INT32_MAX) { error = "Native tab label width overflows"; return false; }
        const auto tabWidth = vertical ? layout.minimumWidth : std::clamp(static_cast<std::int32_t>(padded),layout.minimumWidth,layout.maximumWidth);
        if (next+tabWidth > INT32_MAX) { error = "Native tab strip width overflows"; return false; }
        const auto tabBottom = layout.position == Position::Bottom ? 1 : height-layout.tabHeight;
        Rect tab;
        if (vertical)
        {
            const auto tabLeft = std::int64_t(layout.verticalPadding)+3;
            if (next-layout.verticalHeight < INT32_MIN || tabLeft+tabWidth > INT32_MAX)
            { error = "Native vertical tab geometry overflows"; return false; }
            tab = {static_cast<std::int32_t>(tabLeft),static_cast<std::int32_t>(next-layout.verticalHeight),
                static_cast<std::int32_t>(tabLeft+tabWidth),static_cast<std::int32_t>(next)};
            next -= std::int64_t(layout.verticalHeight)+layout.verticalPadding;
        }
        else
        {
            tab = {static_cast<std::int32_t>(next),static_cast<std::int32_t>(tabBottom),
                static_cast<std::int32_t>(next+tabWidth),static_cast<std::int32_t>(tabBottom+layout.tabHeight)};
            next += tabWidth;
        }
        placements.push_back({entry.panel,entry.button,{static_cast<std::int32_t>(left),bottom,static_cast<std::int32_t>(right),static_cast<std::int32_t>(top)},tab});
    }
    if (!layout.hidden && !vertical)
    {
        const auto total=next-1-layout.horizontalPadding;
        const auto available=width-layout.rightPadding-2*(1+std::int64_t(layout.horizontalPadding));
        if (total>available || owner->tabContainer->scrollPixels>0)
        {
            const auto horizontalArrow=layout.horizontalArrowSize ? layout.horizontalArrowSize : setting("UITabCntrArrowBtnSize").value_or(LLSD(0)).asInteger();
            const auto partial=layout.partialTabWidth ? layout.partialTabWidth : setting("UITabCntrTabPartialWidth").value_or(LLSD(0)).asInteger();
            const auto withArrows=width-layout.rightPadding-2*(2+2*std::int64_t(horizontalArrow));
            if (horizontalArrow<=0 || partial<0 || withArrows<=partial)
            { error="Native horizontal tab overflow requires space for its arrow controls"; return false; }
            maximumScroll=total>available ? static_cast<std::int32_t>(placements.size()) : 0;
            std::int64_t accumulated=0;
            for (auto tab=placements.rbegin(); tab!=placements.rend(); ++tab)
            {
                accumulated+=tab->tab.right-tab->tab.left;
                if (accumulated>withArrows-partial) break;
                if (maximumScroll>0) --maximumScroll;
            }
            maximumScroll=std::min(maximumScroll,static_cast<std::int32_t>(placements.size())-1);
            scrollPosition=std::clamp(owner->tabContainer->scrollPosition,0,maximumScroll);
            std::int64_t target=0;
            if (scrollPosition)
            {
                for (std::int32_t index=0; index<scrollPosition; ++index) target+=placements[index].tab.right-placements[index].tab.left;
                target=std::min(total-withArrows,target-partial);
            }
            if (target<INT32_MIN || target>INT32_MAX) { error="Native tab pixel scroll overflows"; return false; }
            targetScrollPixels=static_cast<std::int32_t>(target);
            const auto interpolant=std::clamp(1.f-std::pow(2.f,-frameDelta/0.08f),0.f,1.f);
            const auto previous=owner->tabContainer->scrollPixels;
            scrollPixels=static_cast<std::int32_t>(previous+(targetScrollPixels-previous)*interpolant);
            const auto offset=(maximumScroll>0 || scrollPixels>0 ? 2*std::int64_t(horizontalArrow)-layout.horizontalPadding : 0)-scrollPixels;
            for (auto& placement : placements)
            {
                const auto tabLeft=placement.tab.left+offset, tabRight=placement.tab.right+offset;
                if (tabLeft<INT32_MIN || tabRight>INT32_MAX) { error="Native horizontal tab position overflows"; return false; }
                placement.tab.left=static_cast<std::int32_t>(tabLeft);
                placement.tab.right=static_cast<std::int32_t>(tabRight);
            }
        }
    }
    ShapeChanges changes;
    for (const auto& placement : placements)
        for (const auto& [id,rect] : {std::pair{placement.panel,placement.content},std::pair{placement.button,placement.tab}})
            if ((positionButtons || id!=placement.button) && get(id)->params.rect != rect &&
                !planReshape(id,std::int64_t(rect.right)-rect.left,std::int64_t(rect.top)-rect.bottom,rect,changes,error)) return false;
    if (!completeShapes(changes,error)) return false;
    std::size_t index=0;
    for (const auto& placement : placements)
    {
        if (get(placement.panel)) mNodes.at(placement.panel).params.follows = Left|Right|Top|Bottom;
        const auto visibleEnd=tabs.size()-maximumScroll+scrollPosition;
        if (get(placement.button)) setVisible(placement.button,!layout.hidden &&
            (!vertical || (index>=static_cast<std::size_t>(scrollPosition) && index<visibleEnd)));
        ++index;
    }
    if (!get(container) || !get(container)->tabContainer) { error = "Native tab layout owner was removed"; return false; }
    mNodes.at(container).tabContainer->layout = layout;
    mNodes.at(container).tabContainer->maximumScroll=maximumScroll;
    mNodes.at(container).tabContainer->scrollPosition=scrollPosition;
    mNodes.at(container).tabContainer->scrollPixels=scrollPixels;
    mNodes.at(container).tabContainer->targetScrollPixels=targetScrollPixels;
    const auto previousArrow=get(container)->tabContainer->previousArrow;
    const auto nextArrow=get(container)->tabContainer->nextArrow;
    if (vertical)
        for (const auto& [arrow,up] : {std::pair{previousArrow,true},std::pair{nextArrow,false}})
            if (get(arrow))
            {
                const auto arrowLeft=layout.verticalPadding+3;
                const auto arrowTop=up ? static_cast<std::int32_t>(height) : arrowSize;
                const Rect rectangle{arrowLeft,arrowTop-arrowSize,arrowLeft+layout.minimumWidth,arrowTop};
                if (get(arrow)->params.rect!=rectangle && !setShape(arrow,rectangle,error)) return false;
                setVisible(arrow,!layout.hidden && maximumScroll>0);
            }
    if (!vertical)
    {
        const auto size=layout.horizontalArrowSize ? layout.horizontalArrowSize : setting("UITabCntrArrowBtnSize").value_or(LLSD(0)).asInteger();
        const auto arrowTop=layout.position==Position::Top ? static_cast<std::int32_t>(height) : size+2;
        const auto first=get(container)->tabContainer->firstArrow, last=get(container)->tabContainer->lastArrow;
        for (const auto& [arrow,arrowLeft] : {std::pair{first,2},std::pair{previousArrow,2+size},
            std::pair{nextArrow,static_cast<std::int32_t>(width)-layout.rightPadding-2-2*size},
            std::pair{last,static_cast<std::int32_t>(width)-layout.rightPadding-2-size}})
            if (get(arrow))
            {
                const Rect rectangle{arrowLeft,arrowTop-layout.tabHeight,arrowLeft+size,arrowTop};
                if (get(arrow)->params.rect!=rectangle && !setShape(arrow,rectangle,error)) return false;
                setVisible(arrow,!layout.hidden && (maximumScroll>0 || scrollPixels>0));
            }
    }
    return true;
}

bool LLVKWidgetTree::createVerticalTabArrows(Id container,const LLVKControl::Params& control,
    const LLVKButton::Params& defaults,std::string& error)
{
    const auto* node=get(container);
    if (!node || !node->tabContainer || !node->tabContainer->layout ||
        node->tabContainer->layout->position!=Node::TabContainer::Layout::Position::Left) return false;
    return createTabArrows(container,control,defaults,error);
}

bool LLVKWidgetTree::createTabArrows(Id container,const LLVKControl::Params& control,
    const LLVKButton::Params& defaults,std::string& error)
{
    error.clear();
    const auto* node=get(container);
    if (!node || !node->tabContainer || !node->tabContainer->layout) return false;
    if (node->tabContainer->previousArrow || node->tabContainer->nextArrow)
    { error="Native tab arrows already exist"; return false; }
    const auto layout=*node->tabContainer->layout;
    const bool vertical=layout.position==Node::TabContainer::Layout::Position::Left;
    const auto action=[this,container](bool forward,bool held)
    {
        auto* node=get(container);
        if (!node || !node->tabContainer) return;
        auto& state=*mNodes.at(container).tabContainer;
        if (held && mTime-state.lastArrowStep<=0.4) return;
        const bool scroll=held || !state.arrowHeld;
        state.arrowHeld=held;
        if (held) state.lastArrowStep=mTime;
        std::string problem;
        if (scroll && !scrollTabStrip(container,forward ? 1 : -1,problem)) return;
        node=get(container);
        if (!node || !node->tabContainer) return;
        auto tabs=node->tabContainer->tabs;
        std::erase_if(tabs,[&](const auto& tab) { return node->tabContainer->hiddenPanels.contains(tab.panel); });
        const auto selected=node->tabContainer->selected;
        const auto found=std::find_if(tabs.begin(),tabs.end(),[&](const auto& tab) { return tab.panel==selected; });
        if (found!=tabs.end() && (forward ? found+1!=tabs.end() : found!=tabs.begin())) moveTab(container,forward,problem);
    };
    for (int index=0; index<(vertical ? 2 : 4); ++index)
    {
        const bool forward=index%2!=0, jump=index>=2;
        Params view;
        view.name=vertical ? (forward ? "Down Arrow" : "Up Arrow") :
            jump ? (forward ? "Jump Right Arrow" : "Jump Left Arrow") : (forward ? "Right Arrow" : "Left Arrow");
        view.rect={0,0,layout.minimumWidth,1}; view.visible=false;
        view.follows=vertical ? Left|(forward ? Bottom : Top) :
            (forward ? Right : Left)|(layout.position==Node::TabContainer::Layout::Position::Top ? Top : Bottom);
        auto button=defaults;
        button.label.clear(); button.selectedLabel.reset(); button.click.reset();
        if (vertical) button.images.overlay=findImage(forward ? "down_arrow.tga" : "up_arrow.tga",error);
        else
        {
            const std::string prefix=jump ? (forward ? "jump_right_" : "jump_left_") :
                (forward ? "scrollbutton_right_" : "scrollbutton_left_");
            button.images.unselected=findImage(prefix+(jump ? "out.tga" : "out_blue.tga"),error);
            if (!error.empty()) return false;
            button.images.selected=findImage(prefix+(jump ? "in.tga" : "in_blue.tga"),error);
            button.images.pressed=button.images.selected; button.pressedProvided=true;
            button.images.overlay.reset();
        }
        if (!error.empty()) return false;
        button.held={};
        if (!jump) button.held.function=[action,forward](Id,const LLSD&) { action(forward,true); };
        auto buttonControl=control;
        buttonControl.init={}; buttonControl.tabStop=false;
        buttonControl.commit.function=[this,container,action,forward,jump](Id,const LLSD&)
        {
            if (!jump) { action(forward,false); return; }
            const auto* owner=get(container);
            if (!owner || !owner->tabContainer) return;
            std::string problem;
            scrollTabStrip(container,forward ? owner->tabContainer->maximumScroll : -owner->tabContainer->maximumScroll,problem);
        };
        const auto arrow=createButton(view,buttonControl,button,container,error);
        if (!arrow) return false;
        auto& state=*mNodes.at(container).tabContainer;
        (jump ? (forward ? state.lastArrow : state.firstArrow) : (forward ? state.nextArrow : state.previousArrow))=*arrow;
    }
    return layoutTabPanels(container,layout,error);
}

bool LLVKWidgetTree::scrollTabStrip(Id container,std::int32_t rows,std::string& error)
{
    error.clear();
    const auto* node=get(container);
    if (!node || !node->tabContainer || !node->tabContainer->layout) return false;
    const auto state=*node->tabContainer;
    const auto next=std::clamp(std::int64_t(state.scrollPosition)+rows,std::int64_t(0),std::int64_t(state.maximumScroll));
    mNodes.at(container).tabContainer->scrollPosition=static_cast<std::int32_t>(next);
    if (layoutTabPanels(container,*state.layout,error)) return true;
    if (get(container) && get(container)->tabContainer) mNodes.at(container).tabContainer->scrollPosition=state.scrollPosition;
    return false;
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

bool LLVKWidgetTree::initializeOverlapPanel(Id id,const Node::OverlapPanel& params,std::string& error)
{
    error.clear();
    if (!get(id) || !params.font || params.minimumWidth<0)
    { error="Invalid native overlap panel"; return false; }
    mNodes.at(id).overlapPanel=params;
    return setOverlapElements(id,params.elements,error);
}

bool LLVKWidgetTree::setOverlapElements(Id id,std::vector<Id> elements,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->overlapPanel || elements.size()>128)
    { error="Invalid native overlap inspection list"; return false; }
    for (const auto source : elements)
    {
        if (!get(source) || get(source)->overlapPanel || hasAncestor(id,source))
        { error="Invalid or recursive native overlap source"; return false; }
        std::vector<Id> pending{source};
        for (std::size_t index=0; index<pending.size(); ++index)
        {
            const auto* node=get(pending[index]);
            if (node->overlapPanel) { error="Native overlap source contains an inspector"; return false; }
            pending.insert(pending.end(),node->children.begin(),node->children.end());
        }
    }
    mNodes.at(id).overlapPanel->elements=std::move(elements);
    return true;
}

bool LLVKWidgetTree::initializeStatBar(Id id,const Node::StatBar& params,std::string& error)
{
    error.clear();
    if (!get(id) || !params.font || !std::isfinite(params.minimum) || !std::isfinite(params.maximum) ||
        params.minimum>params.maximum || !std::isfinite(params.tickSpacing) || params.tickSpacing<0 ||
        params.historyFrames<1 || params.historyFrames>10000 || params.shortFrames<1 || params.shortFrames>10000 ||
        params.decimalDigits<0 || params.decimalDigits>9 || params.maximumHeight<14 || params.maximumHeight>4096)
    { error="Invalid native statistic bar parameters"; return false; }
    mNodes.at(id).statBar=params;
    auto& bar=*mNodes.at(id).statBar;
    bar.currentMinimum=0.f; bar.currentMaximum=bar.maximum;
    auto rect=get(id)->params.rect;
    rect.bottom=rect.top-(bar.showBar ? bar.showHistory ? bar.maximumHeight : 40 : 14);
    return setShape(id,rect,error);
}

bool LLVKWidgetTree::sampleStatBar(Id id,float value,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->statBar || !std::isfinite(value))
    { error="Invalid native statistic sample"; return false; }
    auto& bar=*mNodes.at(id).statBar;
    bar.samples.push_back(value);
    const auto maximum=static_cast<std::size_t>(std::max(bar.historyFrames,bar.shortFrames));
    if (bar.samples.size()>maximum) bar.samples.erase(bar.samples.begin(),bar.samples.begin()+(bar.samples.size()-maximum));
    return true;
}

bool LLVKWidgetTree::setStatBarRange(Id id,float minimum,float maximum,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->statBar || !std::isfinite(minimum) || !std::isfinite(maximum) || minimum>=maximum)
    { error="Invalid native statistic range"; return false; }
    auto& bar=*mNodes.at(id).statBar;
    bar.minimum=minimum; bar.maximum=maximum;
    return true;
}

bool LLVKWidgetTree::advanceStatBar(Id id,float frameDelta,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->statBar || !std::isfinite(frameDelta) || frameDelta<0)
    { error="Invalid native statistic frame"; return false; }
    auto& bar=*mNodes.at(id).statBar;
    const auto blend=1.f-std::exp2(-frameDelta/0.05f);
    bar.currentMinimum+=(bar.minimum-bar.currentMinimum)*blend;
    bar.currentMaximum+=(bar.maximum-bar.currentMaximum)*blend;
    return true;
}

bool LLVKWidgetTree::cycleStatBar(Id id,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->statBar) return false;
    auto& bar=*mNodes.at(id).statBar;
    if (!bar.showBar) bar.showBar=true;
    else if (!bar.showHistory) bar.showHistory=true;
    else { bar.showBar=false; bar.showHistory=false; }
    auto rect=get(id)->params.rect;
    rect.bottom=rect.top-(bar.showBar ? bar.showHistory ? bar.maximumHeight : 40 : 14);
    if (!setShape(id,rect,error)) return false;
    auto parent=get(id)->parent;
    while (get(parent) && get(parent)->parent && get(get(parent)->parent)->containerView) parent=get(parent)->parent;
    if (get(parent) && get(parent)->containerView)
    {
        rect=get(parent)->params.rect;
        return layoutContainerView(parent,rect.right-rect.left,0,error);
    }
    return true;
}

bool LLVKWidgetTree::initializeContainerView(Id id,const Node::ContainerView& params,std::string& error)
{
    error.clear();
    if (!get(id) || (params.showLabel && !params.font))
    { error="Invalid native container owner or label font"; return false; }
    mNodes.at(id).containerView=params;
    const auto children=get(id)->children;
    for (const auto child : children) setVisible(child,params.displayChildren);
    const auto rect=get(id)->params.rect;
    return layoutContainerView(id,rect.right-rect.left,0,error);
}

bool LLVKWidgetTree::layoutContainerView(Id id,std::int32_t width,std::int32_t minimumHeight,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->containerView || width<0 || minimumHeight<0)
    { error="Invalid native container layout"; return false; }
    const auto params=*get(id)->containerView;
    auto children=get(id)->children;
    std::reverse(children.begin(),children.end());
    std::vector<std::int32_t> heights;
    std::int64_t total=params.showLabel ? 20 : 0;
    const auto childWidth=std::max(0,width-12);
    if (params.displayChildren)
        for (const auto child : children)
        {
            if (!get(child)) { error="Native container child disappeared"; return false; }
            if (get(child)->containerView && !layoutContainerView(child,childWidth,0,error)) return false;
            const auto rect=get(child)->params.rect;
            const auto height=rect.top-rect.bottom;
            heights.push_back(height); total+=std::int64_t(height)+2;
        }
    total=std::max(total,std::int64_t(minimumHeight));
    if (total>INT32_MAX) { error="Native container height exceeds limit"; return false; }
    auto rect=get(id)->params.rect;
    if (std::int64_t(rect.left)+width>INT32_MAX) { error="Native container width exceeds limit"; return false; }
    rect.right=rect.left+width;
    if (get(id)->params.follows&Top)
    {
        if (std::int64_t(rect.top)-total<INT32_MIN) { error="Native container bottom exceeds limit"; return false; }
        rect.bottom=rect.top-static_cast<int>(total);
    }
    else
    {
        if (std::int64_t(rect.bottom)+total>INT32_MAX) { error="Native container top exceeds limit"; return false; }
        rect.top=rect.bottom+static_cast<int>(total);
    }
    if (!setShape(id,rect,error)) return false;
    int top=static_cast<int>(total)-(params.showLabel ? 20 : 0);
    for (std::size_t index=0; params.displayChildren && index<children.size(); ++index)
    {
        if (!setShape(children[index],{10,top-heights[index],10+childWidth,top},error)) return false;
        top-=heights[index]+2;
    }
    return true;
}

bool LLVKWidgetTree::setContainerExpanded(Id id,bool expanded,std::string& error)
{
    error.clear();
    if (!get(id) || !get(id)->containerView) return false;
    mNodes.at(id).containerView->displayChildren=expanded;
    const auto children=get(id)->children;
    for (const auto child : children) if (!setVisible(child,expanded)) return false;
    while (get(id)->parent && get(get(id)->parent)->containerView) id=get(id)->parent;
    const auto rect=get(id)->params.rect;
    return layoutContainerView(id,rect.right-rect.left,0,error);
}

bool LLVKWidgetTree::initializeFloater(Id panel,const Node::Floater& params,std::string& error)
{
    error.clear();
    const auto* node=get(panel);
    if (!node || !node->panel || params.legacyHeaderHeight<0)
    { error="Invalid native floater initialization"; return false; }
    mNodes.at(panel).floater=params;
    mNodes.at(panel).params.focusRoot=true;
    mNodes.at(panel).params.mouseOpaque=true;
    auto& background=mNodes.at(panel).panel->params;
    background.backgroundVisible=background.backgroundOpaque=true;
    background.opaqueImage=findImage("Window_Foreground",error);
    if (!error.empty()) return false;
    background.transparentImage=findImage("Window_Background",error);
    return error.empty();
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

bool LLVKWidgetTree::setTabVisibility(Id container,Id panel,bool visible,std::string& error)
{
    error.clear();
    const auto* owner=get(container);
    if (!owner || !owner->tabContainer) { error="Invalid native tab visibility owner"; return false; }
    const auto tabs=owner->tabContainer->tabs;
    const auto found=std::find_if(tabs.begin(),tabs.end(),[&](const auto& tab) { return tab.panel==panel; });
    if (found==tabs.end()) { error="Native tab panel is not registered"; return false; }
    auto& state=*mNodes.at(container).tabContainer;
    if (visible) state.hiddenPanels.erase(panel);
    else state.hiddenPanels.insert(panel);
    setVisible(found->button,visible);
    if (!visible)
    {
        setVisible(panel,false);
        if (state.selected==panel) state.selected=0;
    }
    if (!state.selected)
        for (const auto& tab : tabs)
            if (!state.hiddenPanels.contains(tab.panel) && selectTabPanel(container,tab.panel,error)) break;
    const auto layout=get(container)->tabContainer->layout;
    return !layout || layoutTabPanels(container,*layout,error,0.f,false);
}

bool LLVKWidgetTree::selectTabPanel(Id container, Id panel, std::string& error)
{
    error.clear();
    const auto* owner = get(container);
    const auto* content = get(panel);
    if (!owner || !owner->tabContainer || !content || content->parent != container) return false;
    if (owner->tabContainer->hiddenPanels.contains(panel)) return false;
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
    auto visibleTabs=tabs;
    std::erase_if(visibleTabs,[&](const auto& tab) { return owner->tabContainer->hiddenPanels.contains(tab.panel); });
    auto& state = *mNodes.at(container).tabContainer;
    state.selected = panel;
    if (state.layout && state.layout->position==Node::TabContainer::Layout::Position::Left)
    {
        const auto found=std::find_if(visibleTabs.begin(),visibleTabs.end(),[&](const auto& tab) { return tab.panel==panel; });
        const auto index=static_cast<std::int32_t>(found-visibleTabs.begin());
        const auto visible=static_cast<std::int32_t>(visibleTabs.size())-state.maximumScroll;
        if (index<state.scrollPosition || index>=state.scrollPosition+visible)
            state.scrollPosition=std::min(index,state.maximumScroll);
    }
    else if (state.layout && state.maximumScroll>0)
    {
        const auto index=static_cast<std::int32_t>(std::find_if(visibleTabs.begin(),visibleTabs.end(),[&](const auto& tab) { return tab.panel==panel; })-visibleTabs.begin());
        if (index<state.scrollPosition) state.scrollPosition=index;
        else
        {
            const auto& layout=*state.layout;
            const auto arrowSize=layout.horizontalArrowSize ? layout.horizontalArrowSize : setting("UITabCntrArrowBtnSize").value_or(LLSD(0)).asInteger();
            const auto available=owner->params.rect.right-owner->params.rect.left-layout.rightPadding-2*(2+2*std::int64_t(arrowSize));
            auto running=std::int64_t(get(button)->params.rect.right)-get(button)->params.rect.left;
            auto minimum=index;
            if (running<available)
            {
                auto previous=index-1;
                while (previous>=0)
                {
                    const auto* tab=get(visibleTabs[previous].button);
                    if (!tab) break;
                    running+=tab->params.rect.right-tab->params.rect.left;
                    if (running>available) break;
                    --previous;
                }
                minimum=previous+1;
            }
            state.scrollPosition=std::min(state.maximumScroll,std::clamp(state.scrollPosition,minimum,index));
        }
    }
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
    if (owner->tabContainer->layout && !layoutTabPanels(container,*owner->tabContainer->layout,error,0.f,false)) return false;
    owner=get(container);
    if (!owner || !owner->tabContainer) return false;
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

bool LLVKWidgetTree::routePanelKey(Id root, PanelKey key, LLVKLineEditor::Modifiers modifiers, std::string& error)
{
    error.clear();
    std::vector<Id> parents;
    for (auto current=mKeyboardFocus; get(current); current=get(current)->parent)
    {
        parents.push_back(current);
        if (current==root) break;
    }
    if (parents.empty() || parents.back()!=root) return false;
    if (key==PanelKey::Return)
        for (const auto current : parents)
            if (const auto* node=get(current); node && node->panel && node->panel->defaultButton)
                return panelKey(current,key,modifiers,error);
    for (const auto current : parents)
        if (const auto* node=get(current); node && node->panel)
            if (panelKey(current,key,modifiers,error) || !error.empty()) return error.empty();
    return false;
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

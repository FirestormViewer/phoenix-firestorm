#include "llvkfloater.h"
#include <algorithm>

std::unique_ptr<LLVKFloater> LLVKFloater::create(LLVKWidgetTree& tree,LLVKWidgetFactory& factory,Id root,
    std::string name,std::string title,int width,int height,std::shared_ptr<LLVKFont> font,std::string& error)
{
    error.clear();
    const auto* parent = tree.get(root);
    if (!parent || width < 100 || height < 60 || width > 4096 || height > 4096)
    { error = "Invalid native floater parent or dimensions"; return nullptr; }
    auto floater = std::unique_ptr<LLVKFloater>(new LLVKFloater(tree));
    floater->mRoot = root;
    LLVKWidgetTree::Params view;
    view.name = std::move(name);
    view.rect = {0,0,width,height};
    view.visible = false; view.focusRoot = true; view.mouseOpaque = true;
    LLVKControl::Params control; control.font = font;
    LLVKPanel::Params panel;
    panel.backgroundVisible = panel.backgroundOpaque = true;
    panel.opaqueImage = tree.findImage("Window_Foreground",error);
    if (!error.empty()) return nullptr;
    panel.opaqueColor = LLVKColor{0.16f,0.16f,0.16f,1};
    const auto id = tree.createPanel(view,control,panel,root,error);
    if (!id) return nullptr;
    floater->mId = *id;
    if (!floater->createChrome(factory,title,font,error)) return nullptr;
    return floater;
}

std::unique_ptr<LLVKFloater> LLVKFloater::createFile(LLVKWidgetTree& tree,LLVKWidgetFactory& factory,Id root,
    const std::string& filename,std::string& error)
{
    const auto id=factory.constructFile(tree,filename,root,error);
    if (!id) return nullptr;
    return adopt(tree,factory,root,*id,error);
}

std::unique_ptr<LLVKFloater> LLVKFloater::createXml(LLVKWidgetTree& tree,LLVKWidgetFactory& factory,Id root,
    const std::string& xml,std::string& error)
{
    const auto id=factory.construct(tree,xml,root,error);
    return id ? adopt(tree,factory,root,*id,error) : nullptr;
}

std::unique_ptr<LLVKFloater> LLVKFloater::adopt(LLVKWidgetTree& tree,LLVKWidgetFactory& factory,Id root,Id widget,std::string& error)
{
    const auto id=std::optional<Id>(widget);
    auto floater=std::unique_ptr<LLVKFloater>(new LLVKFloater(tree));
    floater->mRoot=root; floater->mId=*id;
    const auto* node=tree.get(*id);
    if (!node || !node->floater) { error="Native floater file did not create a floater"; return nullptr; }
    if (!tree.expandFloaterHeader(*id,error)) return nullptr;
    const auto title=node->floater->title;
    const auto font=node->control->params.font;
    floater->mCanClose=node->floater->canClose;
    floater->mCanMinimize=node->floater->canMinimize;
    floater->mCanResize=node->floater->canResize;
    floater->mMinWidth=node->floater->minWidth;
    floater->mMinHeight=node->floater->minHeight;
    tree.setVisible(*id,false);
    if (!floater->createChrome(factory,title,font,error)) return nullptr;
    return floater;
}

bool LLVKFloater::createChrome(LLVKWidgetFactory& factory,const std::string& title,std::shared_ptr<LLVKFont> font,std::string& error)
{
    auto& tree=mTree;
    const auto* node=tree.get(mId);
    const auto width=node->params.rect.right-node->params.rect.left, height=node->params.rect.top-node->params.rect.bottom;
    const auto id=mId;
    const auto buttonSize=tree.setting("UIFloaterCloseBoxSize").value_or(LLSD(16)).asInteger();
    const auto buttonInset=tree.setting("UICloseBoxFromTop").value_or(LLSD(5)).asInteger();
    if (buttonSize<1 || buttonSize>256 || buttonInset<0 || buttonInset>256)
    { error="Invalid native floater chrome dimensions"; return false; }
    const auto buttonBottom=height-buttonInset-buttonSize;
    const auto titleId=factory.construct(tree,
        "<text name='floater_title' font='SansSerif' font_shadow='soft' use_ellipses='true' parse_urls='false' "
        "follows='left|right|top' mouse_opaque='false'/>",id,error);
    if (!titleId) return false;
    mTitle=*titleId;
    const auto titleHeight=static_cast<int>(tree.get(mTitle)->control->params.font->metrics().lineHeight);
    const auto titleTop=height-5;
    if (!tree.setShape(mTitle,{14,titleTop-titleHeight,width-30,titleTop},error) || !tree.setValue(mTitle,LLSD(title))) return false;
    const auto close = factory.construct(tree,
        "<button name='floater_close' layout='bottomleft' left='"+std::to_string(width-1-(buttonSize+1))+"' bottom='"+std::to_string(buttonBottom)+
        "' width='"+std::to_string(buttonSize)+"' height='"+std::to_string(buttonSize)+"' follows='right|top' tab_stop='false' image_unselected='Icon_Close_Foreground' "
        "image_selected='Icon_Close_Foreground' image_pressed='Icon_Close_Press' label='' />",id,error);
    if (!close) return false;
    mCloseButton=*close;
    tree.setVisible(*close,mCanClose);
    if (mCanResize && !factory.construct(tree,
        "<icon name='floater_resize_corner' layout='bottomleft' left='"+std::to_string(width-11)+
            "' bottom='0' width='11' height='11' follows='right|bottom' mouse_opaque='false' image_name='Resize_Corner'/>",id,error)) return false;
    LLVKControl::Callback callback;
    callback.function = [this](auto,const LLSD&) { std::string problem; this->close(problem); };
    tree.setControlCommit(*close,std::move(callback));
    if (mCanMinimize)
    {
        for (const bool restore : {false,true})
        {
            const std::string name=restore ? "restore" : "minimize";
            const std::string icon=restore ? "Restore" : "Minimize";
            const auto button=factory.construct(tree,"<button name='floater_"+name+"' layout='bottomleft' left='"+
                std::to_string(width-1-(buttonSize+1)*(mCanClose ? 2 : 1))+"' bottom='"+std::to_string(buttonBottom)+
                "' width='"+std::to_string(buttonSize)+"' height='"+std::to_string(buttonSize)+"' follows='right|top' tab_stop='false' image_unselected='Icon_"+icon+
                "_Foreground' image_selected='Icon_"+icon+"_Foreground' image_pressed='Icon_"+icon+"_Press' label=''/>",id,error);
            if (!button) return false;
            (restore ? mRestoreButton : mMinimizeButton)=*button;
            tree.setVisible(*button,!restore);
            LLVKControl::Callback action;
            action.function=[this,restore](auto,const LLSD&) { std::string problem; setMinimized(!restore,problem); };
            tree.setControlCommit(*button,std::move(action));
        }
        if (!tree.setShape(mTitle,{14,titleTop-titleHeight,width-(mCanClose ? 51 : 30),titleTop},error)) return false;
    }
    if (tree.get(mId)->floater && tree.get(mId)->floater->canDock)
    {
        const auto right=width-1-(buttonSize+1)*(1+static_cast<int>(mCanClose)+static_cast<int>(mCanMinimize));
        const auto dock=factory.construct(tree,"<button name='floater_dock' layout='bottomleft' left='"+std::to_string(right)+
            "' bottom='"+std::to_string(buttonBottom)+"' width='"+std::to_string(buttonSize)+"' height='"+std::to_string(buttonSize)+"' follows='right|top' tab_stop='false' "
            "image_unselected='Icon_Dock_Foreground' image_selected='Icon_Dock_Foreground' image_pressed='Icon_Dock_Press' label=''/>",id,error);
        if (!dock) return false;
        mDockButton=*dock;
        LLVKControl::Callback action;
        action.function=[this](auto,const LLSD&) { std::string problem; setDocked(true,problem); };
        tree.setControlCommit(*dock,std::move(action));
        if (!tree.setShape(mTitle,{14,titleTop-titleHeight,right-7,titleTop},error)) return false;
    }
    if (node->panel && !node->panel->params.helpTopic.empty() && factory.helpHandler() &&
        !tree.setting("FSHideHelpButtons").value_or(LLSD(false)).asBoolean())
    {
        const auto left=width-1-(buttonSize+1)*(1+static_cast<int>(mCanClose)+static_cast<int>(mCanMinimize)+static_cast<int>(mDockButton!=0));
        auto tooltip=factory.helpTooltip();
        for (const auto& [from,to] : {std::pair{"&","&amp;"},std::pair{"'","&apos;"},std::pair{"<","&lt;"},std::pair{">","&gt;"}})
        {
            std::size_t position=0;
            while ((position=tooltip.find(from,position))!=std::string::npos)
            { tooltip.replace(position,std::char_traits<char>::length(from),to); position+=std::char_traits<char>::length(to); }
        }
        const auto help=factory.construct(tree,"<button name='floater_help' layout='bottomleft' left='"+std::to_string(left)+
            "' bottom='"+std::to_string(buttonBottom)+"' width='"+std::to_string(buttonSize)+"' height='"+std::to_string(buttonSize)+
            "' follows='right|top' tab_stop='false' image_unselected='Icon_Help_Foreground' image_selected='Icon_Help_Press' "
            "image_pressed='Icon_Help_Press' hover_glow_amount='0.33' tool_tip='"+tooltip+"' label=''/>",id,error);
        if (!help) return false;
        LLVKControl::Callback action;
        action.function=[handler=factory.helpHandler(),id](auto,const LLSD&) { handler(id); };
        tree.setControlCommit(*help,std::move(action));
        if (!tree.setShape(mTitle,{14,titleTop-titleHeight,left-7,titleTop},error)) return false;
    }
    return true;
}

bool LLVKFloater::setDocked(bool docked,std::string& error)
{
    error.clear();
    const auto* node=mTree.get(mId);
    if (!node || !node->floater || !node->floater->canDock) return false;
    if (docked && !setMinimized(false,error)) return false;
    auto state=*mTree.get(mId)->floater;
    state.docked=docked;
    if (!mTree.initializeFloater(mId,state,error)) return false;
    return mTree.setVisible(mDockButton,!docked);
}

LLVKFloater::~LLVKFloater() { if (mId) { std::string error; mTree.erase(mId,error); } }
bool LLVKFloater::visible() const { const auto* node = mTree.get(mId); return node && node->params.visible; }
bool LLVKFloater::open(std::string& error,std::optional<LLVKWidgetTree::Rect> placement)
{
    error.clear();
    if (!mTree.get(mId) || !mTree.get(mRoot)) { error = "Native floater owner is missing"; return false; }
    if (mMinimized && !setMinimized(false,error)) return false;
    if (!visible())
    {
        mPreviousFocus = mTree.keyboardFocus();
        const auto root = placement.value_or(mTree.get(mRoot)->params.rect), rect = mTree.get(mId)->params.rect;
        const auto width = rect.right-rect.left, height = rect.top-rect.bottom;
        auto left = std::max(0,(root.right-root.left-width)/2), bottom = std::max(0,(root.top-root.bottom-height)/2);
        if (const auto& params=mTree.get(mId)->floater; params && params->relativeX && params->relativeY)
        {
            const auto position=[](float relative,int available,int extent)
            {
                if (relative < -.5f) return static_cast<int>(std::floor((relative+.5f)*2.f*(extent-16)+.5f));
                if (relative > .5f) return available-extent+static_cast<int>(std::floor((relative-.5f)*2.f*(extent-16)+.5f));
                return static_cast<int>(std::floor((relative+.5f)*(available-extent)+.5f));
            };
            left=position(*params->relativeX,root.right-root.left,width);
            bottom=position(*params->relativeY,root.top-root.bottom,height);
        }
        if (mTree.get(mId)->floater && mTree.get(mId)->floater->positioning=="cascading")
        {
            left=0; bottom=std::max(0,root.top-root.bottom-18-height);
            for (const auto sibling : mTree.get(mRoot)->children)
            {
                const auto* other=mTree.get(sibling);
                if (sibling==mId || !other || !other->params.visible || !other->floater || other->floater->positioning!="cascading") continue;
                const auto offset=mTree.setting("UIFloaterOffset").value_or(LLSD(16)).asInteger();
                left=other->params.rect.left+offset; bottom=other->params.rect.top-offset-height;
                break;
            }
        }
        if (!mTree.setShape(mId,{left,bottom,left+width,bottom+height},error)) return false;
    }
    if (!mTree.reparent(mId,mRoot,false,0,error)) return false;
    if (mTree.topControl() && !mTree.setTopControl(0,error)) return false;
    mTree.setVisible(mId,true);
    mControlActive=true;
    if (const auto previous=mTree.lastFocusForGroup(mId))
        return mTree.requestControlFocus(previous,true,error);
    return mTree.requestControlFocus(mId,true,error);
}
bool LLVKFloater::close(std::string& error)
{
    error.clear();
    if (!visible()) return true;
    if (mMinimized && !setMinimized(false,error)) return false;
    const auto callback = mClose;
    if (mCloseDependents && !mCloseDependents(error)) return false;
    for (Id capture = mTree.mouseCapture(); capture && mTree.get(capture); capture = mTree.get(capture)->parent)
        if (capture == mId) { mTree.setMouseCapture(0,error); break; }
    mDragging = false;
    mResizeEdges = 0;
    for (Id popup = mTree.topControl(); popup && mTree.get(popup); popup = mTree.get(popup)->parent)
        if (popup == mId) { mTree.setTopControl(0,error); break; }
    mTree.setVisible(mId,false);
    mControlActive=false;
    const bool focused = mCloseFocus ? mCloseFocus(error) : mTree.get(mPreviousFocus) ? mTree.requestControlFocus(mPreviousFocus,true,error) :
        mTree.setKeyboardFocus(0,false,false,error);
    if (callback) callback();
    return focused;
}
bool LLVKFloater::setMinimized(bool minimized,std::string& error)
{
    error.clear();
    if (minimized==mMinimized) return true;
    if (!mCanMinimize || !mTree.get(mId) || !mTree.get(mRoot)) return false;
    if (minimized)
    {
        const auto width=mTree.setting("UIMinimizedWidth").value_or(LLSD(160)).asInteger();
        if (width<60) { error="Invalid native minimized floater width"; return false; }
        mExpandedRect=mTree.get(mId)->params.rect;
        const auto children=mTree.get(mId)->children;
        mExpandedVisibility.clear();
        for (const auto child : children)
        {
            if (child==mTitle || child==mCloseButton || child==mMinimizeButton || child==mRestoreButton) continue;
            mExpandedVisibility[child]=mTree.get(child)->params.visible;
            mTree.setVisible(child,false);
        }
        const auto root=mTree.get(mRoot)->params.rect;
        const bool legacy=mTree.setting("FSLegacyMinimize").value_or(LLSD(false)).asBoolean();
        const auto offset=(mTree.setting("ShowNavbarFavoritesPanel").value_or(LLSD(false)).asBoolean() ? 20 : 0)+
            (mTree.setting("ShowNavbarNavigationPanel").value_or(LLSD(false)).asBoolean() ? 30 : 0);
        const int bottom=legacy ? 0 : std::max(0,root.top-root.bottom-18-25-offset);
        if (!mTree.setShape(mId,{0,bottom,width,bottom+25},error))
        {
            for (const auto& [child,visible] : mExpandedVisibility) mTree.setVisible(child,visible);
            mExpandedVisibility.clear(); return false;
        }
        mTree.setKeyboardFocus(0,false,false,error);
    }
    else
    {
        if (!mTree.setShape(mId,mExpandedRect,error)) return false;
        for (const auto& [child,visible] : mExpandedVisibility) mTree.setVisible(child,visible);
        mExpandedVisibility.clear();
    }
    mMinimized=minimized;
    mTree.setVisible(mMinimizeButton,!minimized); mTree.setVisible(mRestoreButton,minimized);
    return error.empty();
}

bool LLVKFloater::pointer(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (!visible()) return false;
    const auto rect = mTree.screenRect(mId,error);
    if (!rect) return false;
    using Kind = LLVKWidgetTree::PointerKind;
    if (mResizeEdges && mTree.mouseCapture()!=mId) mResizeEdges=0;
    if (mResizeEdges)
    {
        if (event.kind==Kind::LeftUp) { mResizeEdges=0; return mTree.setMouseCapture(0,error); }
        if (event.kind!=Kind::Hover) return true;
        const auto parent=mTree.screenRect(mRoot,error);
        if (!parent) return false;
        const auto deltaX=std::clamp(event.x,parent->left,parent->right)-mDragX;
        const auto deltaY=std::clamp(event.y,parent->bottom,parent->top)-mDragY;
        auto resized=mResizeRect;
        if (mResizeEdges&1) resized.left=std::min(resized.left+deltaX,resized.right-mMinWidth);
        if (mResizeEdges&2) resized.right=std::max(resized.right+deltaX,resized.left+mMinWidth);
        if (mResizeEdges&4) resized.bottom=std::min(resized.bottom+deltaY,resized.top-mMinHeight);
        if (mResizeEdges&8) resized.top=std::max(resized.top+deltaY,resized.bottom+mMinHeight);
        return mTree.setShape(mId,resized,error);
    }
    if (mCanResize && !mMinimized && event.kind==Kind::LeftDown && event.x>=rect->left && event.x<rect->right && event.y>=rect->bottom && event.y<rect->top)
    {
        constexpr int edge=3,corner=16;
        std::uint8_t edges=0;
        if (event.x<rect->left+edge) edges|=1;
        if (event.x>=rect->right-edge) edges|=2;
        if (event.y<rect->bottom+edge) edges|=4;
        if (event.y>=rect->top-edge) edges|=8;
        if (event.x-rect->left+event.y-rect->bottom<corner) edges=1|4;
        if (rect->right-1-event.x+event.y-rect->bottom<corner) edges=2|4;
        if (event.x-rect->left+rect->top-1-event.y<corner) edges=1|8;
        if (rect->right-1-event.x+rect->top-1-event.y<corner) edges=2|8;
        if (edges)
        {
            if (!mTree.setMouseCapture(mId,error)) return false;
            mResizeEdges=edges; mResizeRect=mTree.get(mId)->params.rect; mDragX=event.x; mDragY=event.y;
            return true;
        }
    }
    if (mDragging && mTree.mouseCapture() != mId) mDragging = false;
    if (mDragging)
    {
        if (event.kind == Kind::LeftUp) { mDragging = false; mTree.setMouseCapture(0,error); return true; }
        if (event.kind == Kind::Hover)
        {
            const auto parent = mTree.screenRect(mRoot,error);
            if (!parent) return false;
            const auto width = rect->right-rect->left, height = rect->top-rect->bottom;
            const auto left = std::clamp(event.x-mDragX,parent->left,std::max(parent->left,parent->right-width));
            const auto bottom = std::clamp(event.y-mDragY,parent->bottom,std::max(parent->bottom,parent->top-height-18));
            return mTree.setShape(mId,{left-parent->left,bottom-parent->bottom,left-parent->left+width,bottom-parent->bottom+height},error);
        }
        return true;
    }
    const auto dragRight=rect->right-3-(mCanClose ? 23 : 0)-(mCanMinimize ? 21 : 0)-
        (mDockButton && mTree.get(mDockButton)->params.visible ? 21 : 0);
    if (event.kind == Kind::LeftDown && event.x >= rect->left && event.x < dragRight && event.y >= rect->top-25 && event.y < rect->top)
    {
        mDragX = event.x-rect->left; mDragY = event.y-rect->bottom;
        mDragging = mTree.setMouseCapture(mId,error);
        return mDragging;
    }
    return false;
}
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
    auto floater=std::unique_ptr<LLVKFloater>(new LLVKFloater(tree));
    floater->mRoot=root; floater->mId=*id;
    const auto* node=tree.get(*id);
    if (!node || !node->floater) { error="Native floater file did not create a floater"; return nullptr; }
    const auto title=node->floater->title;
    const auto font=node->control->params.font;
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
    LLVKWidgetTree::Params view;
    LLVKControl::Params control; control.font=font;
    LLVKPlainControl::Params label;
    label.maximumBytes = 4096;
    view.name = "floater_title"; view.visible = true; view.mouseOpaque = false;
    view.rect = {8,height-23,width-30,height-3}; view.follows = LLVKWidgetTree::Left|LLVKWidgetTree::Right|LLVKWidgetTree::Top;
    control.tabStop = false; control.initialValue = std::move(title);
    if (!tree.createPlainText(view,control,label,id,error)) return false;
    const auto close = factory.construct(tree,
        "<button name='floater_close' layout='bottomleft' left='"+std::to_string(width-23)+"' bottom='"+std::to_string(height-22)+
        "' width='18' height='18' follows='right|top' tab_stop='false' image_unselected='Icon_Close_Foreground' "
        "image_selected='Icon_Close_Foreground' image_pressed='Icon_Close_Press' label='' />",id,error);
    if (!close) return false;
    LLVKControl::Callback callback;
    callback.function = [this](auto,const LLSD&) { std::string problem; this->close(problem); };
    tree.setControlCommit(*close,std::move(callback));
    return true;
}

LLVKFloater::~LLVKFloater() { if (mId) { std::string error; mTree.erase(mId,error); } }
bool LLVKFloater::visible() const { const auto* node = mTree.get(mId); return node && node->params.visible; }
bool LLVKFloater::open(std::string& error)
{
    error.clear();
    if (!mTree.get(mId) || !mTree.get(mRoot)) { error = "Native floater owner is missing"; return false; }
    if (!visible())
    {
        mPreviousFocus = mTree.keyboardFocus();
        const auto root = mTree.get(mRoot)->params.rect, rect = mTree.get(mId)->params.rect;
        const auto width = rect.right-rect.left, height = rect.top-rect.bottom;
        const auto left = std::max(0,(root.right-root.left-width)/2), bottom = std::max(0,(root.top-root.bottom-height)/2);
        if (!mTree.setShape(mId,{left,bottom,left+width,bottom+height},error)) return false;
    }
    if (!mTree.reparent(mId,mRoot,false,0,error)) return false;
    if (mTree.topControl() && !mTree.setTopControl(0,error)) return false;
    mTree.setVisible(mId,true);
    return mTree.requestControlFocus(mId,true,error);
}
bool LLVKFloater::close(std::string& error)
{
    error.clear();
    if (!visible()) return true;
    const auto callback = mClose;
    for (Id capture = mTree.mouseCapture(); capture && mTree.get(capture); capture = mTree.get(capture)->parent)
        if (capture == mId) { mTree.setMouseCapture(0,error); break; }
    mDragging = false;
    for (Id popup = mTree.topControl(); popup && mTree.get(popup); popup = mTree.get(popup)->parent)
        if (popup == mId) { mTree.setTopControl(0,error); break; }
    mTree.setVisible(mId,false);
    const bool focused = mTree.get(mPreviousFocus) ? mTree.requestControlFocus(mPreviousFocus,true,error) :
        mTree.setKeyboardFocus(0,false,false,error);
    if (callback) callback();
    return focused;
}
bool LLVKFloater::pointer(const LLVKWidgetTree::PointerEvent& event,std::string& error)
{
    if (!visible()) return false;
    const auto rect = mTree.screenRect(mId,error);
    if (!rect) return false;
    using Kind = LLVKWidgetTree::PointerKind;
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
    if (event.kind == Kind::LeftDown && event.x >= rect->left && event.x < rect->right-26 && event.y >= rect->top-25 && event.y < rect->top)
    {
        mDragX = event.x-rect->left; mDragY = event.y-rect->bottom;
        mDragging = mTree.setMouseCapture(mId,error);
        return mDragging;
    }
    return false;
}
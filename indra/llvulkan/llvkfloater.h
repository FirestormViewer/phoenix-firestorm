#ifndef LLVKFLOATER_H
#define LLVKFLOATER_H

#include "llvkwidgetfactory.h"

class LLVKFloater final
{
public:
    using Id = LLVKWidgetTree::Id;
    static std::unique_ptr<LLVKFloater> create(LLVKWidgetTree& tree, LLVKWidgetFactory& factory, Id root,
        std::string name, std::string title, int width, int height, std::shared_ptr<LLVKFont> font, std::string& error);
    static std::unique_ptr<LLVKFloater> createFile(LLVKWidgetTree& tree, LLVKWidgetFactory& factory, Id root,
        const std::string& filename, std::string& error);
    static std::unique_ptr<LLVKFloater> createXml(LLVKWidgetTree& tree, LLVKWidgetFactory& factory, Id root,
        const std::string& xml, std::string& error);
    ~LLVKFloater();
    Id id() const noexcept { return mId; }
    bool visible() const;
    bool controlActive() const noexcept { return mControlActive; }
    Id snapTarget() const noexcept { return mSnapTarget; }
    void setSnapTarget(Id target) noexcept { mSnapTarget=target; }
    void updateForeground(bool foreground)
    {
        if (foreground!=mForeground && !foreground) mControlActive=false;
        mForeground=foreground;
    }
    bool open(std::string& error,std::optional<LLVKWidgetTree::Rect> placement={});
    bool close(std::string& error);
    bool setMinimized(bool minimized, std::string& error);
    bool setDocked(bool docked,std::string& error);
    bool minimized() const noexcept { return mMinimized; }
    enum class ResizeCursor { None, Horizontal, Vertical, NorthwestSoutheast, NortheastSouthwest };
    ResizeCursor resizeCursor(int x,int y,std::string& error) const;
    bool pointer(const LLVKWidgetTree::PointerEvent& event, std::string& error);
    void onClose(std::function<void()> callback) { mClose = std::move(callback); }
    void onCloseFocus(std::function<bool(std::string&)> callback) { mCloseFocus=std::move(callback); }
    void onCloseDependents(std::function<bool(std::string&)> callback) { mCloseDependents=std::move(callback); }
private:
    std::uint8_t resizeEdgesAt(int x,int y,std::string& error) const;
    static std::unique_ptr<LLVKFloater> adopt(LLVKWidgetTree& tree, LLVKWidgetFactory& factory, Id root, Id id, std::string& error);
    bool createChrome(LLVKWidgetFactory& factory, const std::string& title, std::shared_ptr<LLVKFont> font, std::string& error);
    explicit LLVKFloater(LLVKWidgetTree& tree) : mTree(tree) {}
    LLVKWidgetTree& mTree;
    Id mId = 0, mRoot = 0, mPreviousFocus = 0;
    Id mSnapTarget=0;
    Id mTitle = 0, mCloseButton = 0, mMinimizeButton = 0, mRestoreButton = 0;
    Id mDockButton = 0;
    bool mCanClose = true, mCanMinimize = false, mMinimized = false;
    LLVKWidgetTree::Rect mExpandedRect;
    std::map<Id,bool> mExpandedVisibility;
    bool mDragging = false;
    bool mForeground=true, mControlActive=true;
    bool mCanResize = false;
    std::uint8_t mResizeEdges = 0;
    int mMinWidth = 0, mMinHeight = 0;
    LLVKWidgetTree::Rect mResizeRect;
    int mDragX = 0, mDragY = 0;
    std::function<void()> mClose;
    std::function<bool(std::string&)> mCloseFocus;
    std::function<bool(std::string&)> mCloseDependents;
};

#endif

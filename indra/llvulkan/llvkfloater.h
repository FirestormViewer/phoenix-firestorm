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
    ~LLVKFloater();
    Id id() const noexcept { return mId; }
    bool visible() const;
    bool open(std::string& error);
    bool close(std::string& error);
    bool pointer(const LLVKWidgetTree::PointerEvent& event, std::string& error);
    void onClose(std::function<void()> callback) { mClose = std::move(callback); }
private:
    bool createChrome(LLVKWidgetFactory& factory, const std::string& title, std::shared_ptr<LLVKFont> font, std::string& error);
    explicit LLVKFloater(LLVKWidgetTree& tree) : mTree(tree) {}
    LLVKWidgetTree& mTree;
    Id mId = 0, mRoot = 0, mPreviousFocus = 0;
    bool mDragging = false;
    int mDragX = 0, mDragY = 0;
    std::function<void()> mClose;
};

#endif
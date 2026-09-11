#ifndef LLVKLOGINMENU_H
#define LLVKLOGINMENU_H

#include "llvkwidgetpaint.h"

class LLVKLoginMenu final
{
public:
    struct Item
    {
        std::string name, label, shortcut, action, parameter;
        bool visible = true, separator = false, branch = false;
        std::vector<std::size_t> children;
    };
    using Handler = std::function<void(const std::string&,const std::string&)>;
    static std::unique_ptr<LLVKLoginMenu> create(std::string_view xml, std::shared_ptr<LLVKFont> font,
        std::shared_ptr<LLVKColorTable> colors, LLVKLabel::Context labels, bool debug, std::string& error);
    void bind(std::string action, Handler handler);
    void bindItem(std::string action, std::string parameter, Handler handler);
    void setVisible(std::string_view name, bool visible);
    bool shortcut(std::string key, bool control, bool shift, bool alt);
    bool paint(LLVKWidgetPaint& output, LLVKWidgetTree::Rect viewport, std::string& error);
    bool pointer(const LLVKWidgetTree::PointerEvent& event);
    enum class Key { Activate, Escape, Left, Right, Up, Down, Return };
    bool key(Key key);
    void dismiss();
    bool open() const noexcept { return !mOpen.empty(); }
    const std::vector<Item>& items() const noexcept { return mItems; }
private:
    struct Hit { std::size_t item, level; LLVKWidgetTree::Rect rect; };
    bool enabled(std::size_t item) const;
    void activate(std::size_t item, std::size_t level);
    std::vector<Item> mItems;
    std::vector<std::size_t> mRoots, mOpen;
    std::vector<Hit> mHits;
    std::optional<std::size_t> mHovered;
    std::map<std::string,Handler> mHandlers;
    std::map<std::pair<std::string,std::string>,Handler> mItemHandlers;
    std::shared_ptr<LLVKFont> mFont;
    std::shared_ptr<LLVKColorTable> mColors;
    LLVKLabel::Context mLabels;
    LLVKWidgetTree::Rect mViewport;
    bool mPressed = false;
};

#endif
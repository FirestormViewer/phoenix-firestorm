#ifndef LLVKMENU_H
#define LLVKMENU_H

#include "llvkwidgetpaint.h"

class LLVKMenu final
{
public:
    struct Item
    {
        std::string name, label, shortcut, action, parameter;
        bool visible = true, separator = false, branch = false;
        std::vector<std::size_t> children;
        std::string checkAction, checkParameter, enableAction, enableParameter, visibleAction, visibleParameter;
        bool checkable = false, checked = false, enabled = true;
        unsigned char jumpKey = 0;
        bool createJumpKeys = false;
        std::function<void()> invoke;
    };
    using Handler = std::function<void(const std::string&,const std::string&)>;
    using Predicate = std::function<bool(const std::string&)>;
    static std::unique_ptr<LLVKMenu> create(std::string_view xml, std::shared_ptr<LLVKFont> font,
        std::shared_ptr<LLVKColorTable> colors, LLVKLabel::Context labels, bool debug, std::string& error);
    void bind(std::string action, Handler handler);
    void bindItem(std::string action, std::string parameter, Handler handler);
    void bindPredicate(std::string action, Predicate predicate);
    bool itemChecked(std::size_t item) const;
    bool itemVisible(std::size_t item) const;
    void setVisible(std::string_view name, bool visible);
    bool showContext(std::vector<Item> items, int x, int y, std::string& error);
    bool showPopup(std::vector<Item> items, LLVKWidgetTree::Rect anchor, const std::string& position,
        std::function<void()> dismissed, std::string& error);
    bool shortcut(std::string key, bool control, bool shift, bool alt);
    bool paint(LLVKWidgetPaint& output, LLVKWidgetTree::Rect viewport, std::string& error,
        std::optional<LLVKWidgetTree::Rect> bar = {},bool dropdowns = true,std::optional<float> backingBottom = {});
    bool pointer(const LLVKWidgetTree::PointerEvent& event);
    enum class Key { Activate, Escape, Left, Right, Up, Down, Return };
    bool key(Key key);
    bool character(char32_t character);
    std::optional<int> barWidth(std::string& error) const;
    void dismiss();
    bool open() const noexcept { return !mOpen.empty(); }
    std::optional<std::size_t> selectedItem() const noexcept { return mHovered; }
    const std::vector<Item>& items() const noexcept { return mItems; }
private:
    struct Hit { std::size_t item, level; LLVKWidgetTree::Rect rect; };
    bool enabled(std::size_t item) const;
    void activate(std::size_t item, std::size_t level);
    void assignJumpKeys(const std::vector<std::size_t>& siblings);
    std::vector<Item> mItems;
    std::vector<std::size_t> mRoots, mOpen;
    std::vector<Hit> mHits;
    std::optional<std::size_t> mHovered;
    std::map<std::string,Handler> mHandlers;
    std::map<std::string,Predicate> mPredicates;
    std::map<std::pair<std::string,std::string>,Handler> mItemHandlers;
    std::shared_ptr<LLVKFont> mFont;
    std::shared_ptr<LLVKColorTable> mColors;
    LLVKLabel::Context mLabels;
    LLVKWidgetTree::Rect mViewport;
    LLVKWidgetTree::Rect mBar;
    bool mPressed = false;
    bool mKeyboardMode = false;
    std::optional<std::size_t> mContextRoot;
    LLVKWidgetTree::Rect mContextAnchor;
    std::string mPopupPosition;
    std::function<void()> mDismissed;
};

#endif
#ifndef LLVKMENU_H
#define LLVKMENU_H

#include "llvkwidgetpaint.h"

class LLVKMenu final
{
    struct Model;
public:
    LLVKMenu();
    LLVKMenu(const LLVKMenu&)=delete;
    LLVKMenu& operator=(const LLVKMenu&)=delete;
    struct Item
    {
        std::string name, label, shortcut, action, parameter;
        bool visible = true, separator = false, branch = false;
        std::vector<std::size_t> children;
        std::string checkAction, checkParameter, enableAction, enableParameter, visibleAction, visibleParameter;
        bool checkable = false, checked = false, enabled = true;
        unsigned char jumpKey = 0;
        bool createJumpKeys = false;
        bool canTearOff = false;
        int shortcutPad = 15;
        bool dropShadow = true, backgroundVisible = true;
        std::string backgroundColor="MenuDefaultBgColor";
        std::function<void()> invoke;
    };
    using Handler = std::function<void(const std::string&,const std::string&)>;
    using Predicate = std::function<bool(const std::string&)>;
    using TearOff = std::function<void(std::size_t,LLVKWidgetTree::Rect)>;
    void setTearOffHandler(TearOff handler) { mTearOff=std::move(handler); }
    std::shared_ptr<LLVKMenu> detachedView(std::size_t item,std::string& error);
    bool detached() const noexcept { return mFixedRoot.has_value(); }
    void setDetachedActive(bool active);
    void setDetachedFocus(bool focused);
    static std::unique_ptr<LLVKMenu> create(std::string_view xml, std::shared_ptr<LLVKFont> font,
        std::shared_ptr<LLVKColorTable> colors, LLVKLabel::Context labels, bool debug, std::string& error);
    void bind(std::string action, Handler handler);
    void bindItem(std::string action, std::string parameter, Handler handler);
    void bindPredicate(std::string action, Predicate predicate);
    bool itemChecked(std::size_t item) const;
    bool itemVisible(std::size_t item) const;
    void setVisible(std::string_view name, bool visible);
    void setEnabled(std::string_view name, bool enabled);
    bool showContext(std::vector<Item> items, int x, int y, std::string& error);
    bool showPopup(std::vector<Item> items, LLVKWidgetTree::Rect anchor, const std::string& position,
        std::function<void()> dismissed, std::string& error);
    bool shortcut(std::string key, bool control, bool shift, bool alt);
    bool paint(LLVKWidgetPaint& output, LLVKWidgetTree::Rect viewport, std::string& error,
        std::optional<LLVKWidgetTree::Rect> bar = {},bool dropdowns = true,std::optional<float> backingBottom = {},float backgroundAlpha=1.f);
    bool pointer(const LLVKWidgetTree::PointerEvent& event);
    enum class Key { Activate, Escape, Left, Right, Up, Down, Return };
    bool key(Key key);
    bool character(char32_t character);
    std::optional<int> barWidth(std::string& error) const;
    std::optional<std::pair<int,int>> menuSize(std::size_t item,std::string& error) const;
    void dismiss();
    void setTime(double seconds) noexcept { mModel->time=seconds; }
    bool open() const noexcept { return !mOpen.empty(); }
    std::optional<std::size_t> selectedItem() const noexcept { return mHovered; }
    const std::vector<Item>& items() const noexcept { return mItems; }
private:
    struct Model
    {
        std::vector<Item> items;
        std::map<std::string,Handler> handlers;
        std::map<std::string,Predicate> predicates;
        std::map<std::pair<std::string,std::string>,Handler> itemHandlers;
        std::set<std::size_t> torn;
        std::set<std::size_t> focusedTorn;
        struct Activation { std::size_t item; LLVKWidgetTree::Rect rectangle; double started; };
        std::optional<Activation> activation;
        double time=0.0;
    };
    explicit LLVKMenu(std::shared_ptr<Model> model);
    struct Hit { std::size_t item, level; LLVKWidgetTree::Rect rect; bool tearOff=false; };
    bool enabled(std::size_t item) const;
    bool commandEnabled(std::size_t item) const;
    void activate(std::size_t item, std::size_t level);
    void invokeItem(std::size_t item);
    void assignJumpKeys(const std::vector<std::size_t>& siblings);
    std::shared_ptr<Model> mModel;
    std::vector<Item>& mItems;
    std::vector<std::size_t> mRoots, mOpen;
    std::vector<Hit> mHits;
    std::optional<std::size_t> mHovered;
    std::map<std::string,Handler>& mHandlers;
    std::map<std::string,Predicate>& mPredicates;
    std::map<std::pair<std::string,std::string>,Handler>& mItemHandlers;
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
    TearOff mTearOff;
    std::optional<std::size_t> mFixedRoot;
    bool mDetachedFocus=false;
    std::optional<std::size_t> mTearHovered;
    std::map<std::size_t,std::pair<int,int>> mLastHover;
    std::map<std::size_t,LLVKWidgetTree::Rect> mPopupRects;
};

#endif
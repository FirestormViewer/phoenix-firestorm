#ifndef LLVKWIDGETTREE_H
#define LLVKWIDGETTREE_H

#include "llvkcontrol.h"
#include "llvkicon.h"
#include "llvkbutton.h"
#include "llvkbadge.h"
#include "llvkplaincontrol.h"
#include "llvkpanel.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <set>

class LLVKWidgetTree final
{
public:
    using Id = std::uint64_t;
    static constexpr std::size_t maximumNodes = 10000;
    static constexpr std::size_t maximumDepth = 64;
    enum Follows : std::uint8_t { Left = 1, Right = 2, Top = 4, Bottom = 8 };
    struct Rect
    {
        std::int32_t left = 0;
        std::int32_t bottom = 0;
        std::int32_t right = 0;
        std::int32_t top = 0;
        auto operator<=>(const Rect&) const = default;
    };
    struct Params
    {
        std::string name = "unnamed";
        Rect rect;
        std::uint8_t follows = 0;
        bool visible = true;
        bool enabled = true;
        bool mouseOpaque = true;
        bool useBoundingRect = false;
        bool fromDeclaration = false;
        std::uint8_t soundFlags = 2;
        std::string layout;
        std::string tooltip;
        std::optional<std::int32_t> tabGroup;
    };
    struct BadgeConstruction
    {
        Params view;
        LLVKControl::Params control;
        LLVKBadge::Params defaults;
        std::optional<LLVKBadge::Params> provided;
    };
    enum class CheckBoxWrap { None, Down, Up };
    struct CheckBoxConstruction
    {
        Params labelView;
        LLVKControl::Params labelControl;
        LLVKPlainControl::Params labelText;
        Params buttonView;
        LLVKControl::Params buttonControl;
        LLVKButton::Params button;
        std::string label;
        bool initialValue = false;
        bool fontProvided = false;
        CheckBoxWrap wrap = CheckBoxWrap::None;
        std::int32_t horizontalPadding = 0;
        LLVKControl::Validation onCheck;
    };
    struct CheckBox
    {
        CheckBoxConstruction construction;
        Id label = 0;
        Id button = 0;
    };
    struct Node
    {
        Params params;
        std::optional<LLVKControl> control;
        std::optional<LLVKIcon> icon;
        std::optional<LLVKButton> button;
        std::optional<LLVKBadge> badge;
        std::optional<LLVKPlainControl> plainText;
        std::optional<CheckBox> checkBox;
        std::optional<LLVKPanel> panel;
        std::optional<LLVKBorder> border;
        bool acceptsBadge = false;
        std::optional<BadgeConstruction> badgeConstruction;
        Id parent = 0;
        std::vector<Id> children;
        std::int32_t lastTabGroup = 0;
    };

    enum class PointerKind { LeftDown, LeftUp, RightDown, RightUp, DoubleClick, Hover };
    struct PointerEvent
    {
        PointerKind kind = PointerKind::Hover;
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::uint32_t modifiers = 0;
        double time = 0.0;
        std::uint64_t frame = 0;
    };
    bool routePointer(Id root, const PointerEvent& screenEvent, std::string& error);
    struct Events
    {
        std::function<void(Id)> focusLost;
        std::function<void(Id)> focusReceived;
        std::function<void(Id)> focusChanged;
        std::function<void(Id)> captureLost;
        std::function<void(Id)> topLost;
        std::function<void(Id,bool)> sound;
        std::function<void(Id,bool)> cursor;
        std::function<void(Id,const PointerEvent&)> pointer;
    };
    bool setEvents(Id id, Events events);
    bool setKeyboardFocus(Id id, bool lock, bool keystrokesOnly, std::string& error);
    bool setMouseCapture(Id id, std::string& error);
    bool setTopControl(Id id, std::string& error);
    void unlockFocus() noexcept { mLockedFocus = 0; }
    Id keyboardFocus() const noexcept { return mKeyboardFocus; }
    Id mouseCapture() const noexcept { return mMouseCapture; }
    Id topControl() const noexcept { return mTopControl; }
    bool keystrokesOnly() const noexcept { return mKeystrokesOnly; }
    bool eraseControl(Id id, std::string& error);
    std::optional<Id> createControl(const Params& view, const LLVKControl::Params& control,
                                    Id parent, std::string& error);
    enum class SettingType { Opaque, Boolean, Integer, Real, String };
    bool defineSetting(const std::string& name, const LLSD& value, SettingType type = SettingType::Opaque);
    bool updateSetting(const std::string& name, const LLSD& value);
    bool setValue(Id id, const LLSD& value);
    bool resetDirty(Id id);
    bool commit(Id id);
    bool validate(Id id);
    bool mouseEnter(Id id);
    bool mouseLeave(Id id);
    bool writeBoundValue(Id id, const LLSD& value);
    bool postBuildControl(Id id);
    bool chromeInChain(Id id) const noexcept;
    bool requestControlFocus(Id id, bool focus, std::string& error);
    std::optional<Id> createIcon(const Params& view, const LLVKControl::Params& control,
                                 const LLVKIcon::Params& icon, Id parent, std::string& error);
    std::optional<Id> createButton(const Params& view, const LLVKControl::Params& control,
                                   const LLVKButton::Params& button, Id parent, std::string& error,
                                   std::optional<BadgeConstruction> badge = std::nullopt);
    bool postBuildButton(Id id, std::string& error);
    std::optional<Id> createBadge(const Params& view, const LLVKControl::Params& control,
                                  const LLVKBadge::Params& badge, Id owner, Id parent, std::string& error);
    bool attachBadge(Id id, Id parent, std::string& error);
    bool setAcceptsBadge(Id id, bool accepts);
    bool attachBadgeToHolder(Id id, std::string& error);
    bool setBadgeLabel(Id id, std::u32string label);
    bool setBadgeAtParentTop(Id id, bool atTop);
    bool setButtonBadgeLabel(Id id, std::u32string label, std::string& error);
    bool setButtonBadgeVisible(Id id, bool visible);
    std::optional<Id> createPlainText(const Params& view, const LLVKControl::Params& control,
                                     const LLVKPlainControl::Params& text, Id parent, std::string& error);
    bool setPlainText(Id id, std::string text, std::string& error);
    bool setPlainTextArgument(Id id, std::string key, std::string replacement, std::string& error);
    bool reflowPlainText(Id id, std::string& error);
    bool fitPlainText(Id id, std::string& error);
    std::optional<Id> createCheckBox(const Params& view, const LLVKControl::Params& control,
                                    const CheckBoxConstruction& checkbox, Id parent, std::string& error);
    LLSD value(Id id) const;
    bool dirty(Id id) const;
    bool refreshCheckBox(Id id);
    bool bindValueSetting(Id id, const std::string& name);
    bool setTentative(Id id, bool tentative);
    bool tentative(Id id) const;
    bool setCheckBoxLabel(Id id, std::string label, std::string& error);
    bool setCheckBoxLabelArgument(Id id, std::string key, std::string replacement, std::string& error);
    std::optional<Id> createBorder(const Params& view, const LLVKBorder::Params& border, Id parent, std::string& error);
    std::optional<Id> createPanel(const Params& view, const LLVKControl::Params& control,
                                 const LLVKPanel::Params& panel, Id parent, std::string& error);
    bool addPanelBorder(Id id, const LLVKBorder::Params& border, std::string& error);
    bool removePanelBorder(Id id, std::string& error);
    std::optional<std::string> panelString(Id id, const std::string& name,
        const LLVKLabel::Arguments& arguments, std::string& error) const;
    bool setButtonToggle(Id id, bool selected, std::string& error);
    bool setButtonLabel(Id id, std::u32string label, std::optional<bool> selectedOnly = std::nullopt);
    bool setButtonLabelArgument(Id id, std::string key, std::string replacement);
    void setLabelContext(LLVKLabel::Context context);
    bool activateButton(Id id, std::string& error);
    bool setButtonFlashing(Id id, bool flashing, bool force = false, bool alternateColor = false);
    bool advanceTime(double time, std::string& error);
    bool buttonUnicode(Id id, char32_t character, bool repeated, std::string& error);
    bool buttonReturn(Id id, std::uint32_t modifiers, bool repeated, std::string& error);
    bool registerImage(std::shared_ptr<const LLVKWidgetImage> image);
    std::shared_ptr<const LLVKWidgetImage> findImage(const std::string& name) const;
    bool iconWantsHandCursor(Id id) const noexcept;
    struct IconDraw
    {
        Id id = 0;
        Rect rectangle;
        std::array<float,4> color;
        std::shared_ptr<const LLVKWidgetImage> image;
    };
    std::optional<IconDraw> prepareIcon(Id id, float drawAlpha, float controlTransparency, std::string& error) const;

    std::optional<Id> create(const Params& params, Id parent, std::string& error);
    bool reparent(Id child, Id parent, bool inBack, std::int32_t tabGroup, std::string& error);
    bool erase(Id id, std::string& error);
    bool reshape(Id id, std::int32_t width, std::int32_t height, std::string& error);
    bool setVisible(Id id, bool visible);
    bool setEnabled(Id id, bool enabled);
    const Node* get(Id id) const noexcept;
    bool visibleInChain(Id id) const noexcept;
    bool enabledInChain(Id id) const noexcept;
    std::optional<Rect> boundingRect(Id id, Id topControl, std::string& error) const;
    std::optional<Rect> screenRect(Id id, std::string& error) const;
    std::optional<bool> containsLocal(Id id, std::int32_t x, std::int32_t y,
                                      bool useBounds, Id topControl, std::string& error) const;
    std::size_t size() const noexcept { return mNodes.size(); }
    static std::uint8_t parseFollows(const std::string& text);

private:
    friend class LLVKWidgetFactory;
    bool planReshape(Id id, std::int64_t width, std::int64_t height, const Rect& origin,
                     std::map<Id,Rect>& changes, std::string& error) const;
    static bool validRect(const Rect& rect) noexcept;
    void eraseSubtree(Id id);
    bool hasAncestor(Id id, Id ancestor) const noexcept;
    bool canReceiveFocus(Id id) const noexcept;
    void notify(Id id, std::function<void(Id)> Events::* event);
    void notifyVisibility(Id id, bool visible);
    bool eraseOwned(Id id, bool notifyFocus, std::string& error);
    bool dispatchControl(Id id, LLVKControl::Callback LLVKControl::Params::* event);
    void applyControlSettings(Id id);
    std::optional<Id> createControlImpl(const Params& view, const LLVKControl::Params& control,
                                       std::optional<LLVKIcon> icon, Id parent, std::string& error,
                                       std::optional<LLVKButton> button = std::nullopt,
                                       std::optional<LLVKBadge> badge = std::nullopt,
                                       std::optional<BadgeConstruction> badgeConstruction = std::nullopt,
                                       std::optional<LLVKPlainControl> plainText = std::nullopt,
                                       std::optional<CheckBox> checkBox = std::nullopt,
                                       std::optional<LLVKPanel> panel = std::nullopt);
    bool constructCheckBoxChildren(Id id, std::string& error);
    bool commitCheckBox(Id id);
    bool planCheckBoxReshape(Id id, std::int64_t width, std::map<Id,Rect>& changes, std::string& error) const;
    bool updatePlainText(Id id, LLVKLabel source, const LLVKLabel::Context& context, std::string& error);
    std::optional<LLVKPlainControl> resolvePlainText(const LLVKPlainControl& state, LLVKLabel source,
        const LLVKLabel::Context& context, std::string& error) const;
    bool resizeButton(Id id, std::string& error);
    bool buttonCommitSignal(Id id);
    bool buttonCallback(Id id, LLVKControl::Callback LLVKButton::Params::* event, const LLSD& value);
    void buttonSound(Id id, bool release);
    void buttonCaptureLost(Id id);
    bool handlePointer(Id id, PointerEvent event, std::string& error);
    bool childrenPointer(Id id, const PointerEvent& event, std::string& error);
    bool basePointer(Id id, const PointerEvent& event, std::string& error);
    bool buttonPointer(Id id, PointerEvent event, std::string& error);
    void cursorEffect(Id id, bool hand);
    void updateFlashSettings();
    std::map<Id,Node> mNodes;
    std::map<Id,Events> mEvents;
    std::map<std::string,LLSD> mSettings;
    std::map<std::string,SettingType> mSettingTypes;
    std::map<std::string,std::shared_ptr<const LLVKWidgetImage>> mImages;
    std::set<Id> mErasing;
    std::vector<Id> mFocusChain;
    Id mKeyboardFocus = 0;
    Id mMouseCapture = 0;
    Id mTopControl = 0;
    Id mLockedFocus = 0;
    std::uint64_t mFocusEpoch = 0;
    bool mKeystrokesOnly = false;
    Id mNextId = 1;
    double mTime = 0.0;
    LLVKLabel::Context mLabelContext;
};

#endif
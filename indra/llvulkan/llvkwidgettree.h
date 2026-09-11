#ifndef LLVKWIDGETTREE_H
#define LLVKWIDGETTREE_H

#include "llvkcontrol.h"
#include "llvkicon.h"
#include "llvkbutton.h"
#include "llvkbadge.h"
#include "llvkplaincontrol.h"
#include "llvkpanel.h"
#include "llvkskinimages.h"
#include "llvklineeditor.h"
#include "llvkclipboard.h"
#include "llvkscroll.h"
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <functional>
#include <set>

struct LLVKWidgetLayout;

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
        std::int32_t defaultTabGroup = 0;
        bool focusRoot = false;
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
    struct LineEditorParams
    {
        LLVKLineEditor::Params text;
        LLVKBorder::Params border;
        std::string label;
        std::shared_ptr<const LLVKWidgetImage> background, disabledBackground, focusedBackground;
        LLVKColor cursorColor, backgroundColor, textColor, readOnlyColor, tentativeColor, highlightColor, preeditColor;
        bool commitOnFocusLost = true;
        bool selectOnCommit = true;
        bool revertOnEscape = true;
        bool keystrokeOnEscape = false;
        bool ignoreTab = true;
        bool ignoreArrowKeys = false;
        bool passDelete = false;
        bool drawFocusBorder = true;
        bool showFocusedBackground = false;
        bool showFocusedLabel = false;
        bool useBackgroundColor = false;
        LLVKControl::Callback keystroke;
        std::function<bool(std::u32string_view)> prevalidator, inputPrevalidator;
        std::optional<std::string> prevalidatorName, inputPrevalidatorName;
    };
    struct LineEditor
    {
        LineEditorParams params;
        LLVKLineEditor text;
        LLVKLabel label;
        Id border = 0;
        bool readOnly = false;
        bool historyEnabled = false;
        std::vector<std::string> history;
        std::size_t historyPosition = 0;
        std::optional<LLVKLineEditor::Key> lastKey;
        std::size_t lastSelectionStart = 0, lastSelectionEnd = 0;
        std::optional<double> tripleClickUntil;
        double scrollTime = 0.0;
    };
    struct SearchEditorParams
    {
        LineEditorParams editor;
        LLVKControl::Params buttonControl;
        LLVKButton::Params searchButton, clearButton;
        bool searchVisible = true, clearVisible = false, highlight = true;
        std::int32_t searchWidth = 13, searchHeight = 13, searchLeft = 4, searchBottom = 4;
        std::int32_t clearWidth = 16, clearHeight = 16, clearBottom = 4, clearRight = 4, clearLeft = 4;
        std::shared_ptr<const LLVKWidgetImage> highlightBackground;
        LLVKControl::Callback keystroke, textChanged;
    };
    struct SearchEditor
    {
        Id editor = 0, search = 0, clear = 0;
        std::shared_ptr<const SearchEditorParams> params;
    };
    struct ListColumn
    {
        std::string name, label;
        std::int32_t width = -1;
        float relativeWidth = -1.f;
    };
    struct ListCellStyle
    {
        enum class Type { Text, Icon, IconText };
        Type type = Type::Text;
        std::shared_ptr<LLVKFont> font;
        std::shared_ptr<const LLVKWidgetImage> image;
        LLVKButton::Align alignment = LLVKButton::Align::Left;
        LLVKColor imageColor{1,1,1,1};
        std::string tooltip;
    };
    struct ListRow
    {
        LLSD value;
        std::vector<std::string> cells;
        bool enabled = true, selected = false;
        std::int32_t selectedCell = -1;
        std::vector<ListCellStyle> styles;
    };
    struct ScrollListParams;
    struct ScrollList
    {
        std::shared_ptr<const ScrollListParams> params;
        std::vector<ListRow> rows;
        std::vector<ListColumn> columns;
        std::vector<std::int32_t> widths;
        std::vector<std::pair<std::size_t,bool>> sortColumns;
        std::vector<Id> headers;
        Id scrollbar = 0, border = 0;
        Rect content;
        std::int32_t lineHeight = 0, firstRow = 0, pageLines = 0, hovered = -1, anchor = -1;
        std::int32_t hoveredCell = -1;
    };
    struct ColorSwatchParams
    {
        LLVKColor color{1,1,1,1}, borderColor{1,1,1,1};
        LLVKColor enabledText{1,1,1,1}, disabledText{0.5f,0.5f,0.5f,1};
        std::string label;
        std::int32_t labelWidth = -1, labelHeight = 16;
        LLVKPlainControl::Params caption;
        LLVKControl::Params captionControl;
        LLVKBorder::Params border;
        std::shared_ptr<const LLVKWidgetImage> alphaBackground;
        bool applyImmediately = false;
        LLVKControl::Callback selected, cancelled;
        std::function<void(Id,bool)> showPicker;
    };
    struct ColorSwatch
    {
        std::shared_ptr<const ColorSwatchParams> params;
        LLVKColor::Value color{1,1,1,1}, original{1,1,1,1}, pending{1,1,1,1};
        Id caption = 0, border = 0;
        bool valid = true, picking = false;
        std::uint64_t generation = 0;
    };
    enum class ColorPickOperation { Change, Select, Cancel };
    struct ColorPicker
    {
        Id swatch = 0;
        std::map<std::string,Id> fields;
        LLVKColor::Value rgb{1,1,1,1};
        std::array<float,3> hsl{0,0,1};
        std::shared_ptr<const LLVKWidgetImage> hueImage;
        std::array<LLVKColor::Value,32> palette{};
        bool paletteReady = false;
        std::shared_ptr<LLVKColorTable> paletteColors;
        enum class Drag { None, Hue, Luminance, Swatch };
        Drag drag = Drag::None;
        std::int32_t highlighted = -1;
        bool immediate = false, synchronizing = false;
        std::function<void()> close;
    };
    struct ComboItem
    {
        std::string label;
        LLSD value;
        bool enabled = true;
    };
    struct ComboParams
    {
        LLVKControl::Params buttonControl, listControl, editorControl;
        LLVKButton::Params button;
        LineEditorParams editor;
        std::vector<ComboItem> items;
        std::string label;
        bool allowTextEntry = false, tentativeText = true;
        std::size_t maximumBytes = 20;
        std::int32_t buttonShadow = 2;
        bool listAbove = false;
        bool forceDisableSubstring = false;
        LLVKControl::Callback textEntry, textChanged, prearrange;
        LLVKColor listBackground{1,1,1,1};
        bool listBackgroundVisible = true;
        LLVKColor listForeground{0,0,0,1}, listSelectedForeground{1,1,1,1}, listDisabledForeground{0.5f,0.5f,0.5f,1};
        LLVKColor listSelectedBackground{0,0,0,1}, listHoverBackground{0,0,0,0}, listReadOnlyBackground{0,0,0,0};
    };
    struct Combo
    {
        std::shared_ptr<const ComboParams> params;
        std::vector<ComboItem> items;
        std::optional<std::size_t> selected;
        Id button = 0, list = 0, editor = 0;
        bool dirty = false;
        std::int32_t rowHeight = 0;
        std::size_t firstRow = 0;
        std::optional<std::size_t> hovered;
        bool autocompleted = false;
    };
    struct ScrollbarParams
    {
        LLVKControl::Params decreaseControl, increaseControl;
        LLVKButton::Params decreaseButton, increaseButton;
        std::int32_t documentSize = 0, position = 0, pageSize = 0, stepSize = 1;
        std::optional<std::int32_t> thickness;
        bool vertical = false;
        std::shared_ptr<const LLVKWidgetImage> thumbVertical, thumbHorizontal, trackVertical, trackHorizontal;
        LLVKColor trackColor{1,1,1,1}, thumbColor{1,1,1,1}, backgroundColor{0,0,0,1};
        bool backgroundVisible = false;
        std::function<void(Id,std::int32_t)> changed;
    };
    struct ListHeaderParams
    {
        LLVKControl::Params control;
        LLVKButton::Params button;
        std::shared_ptr<const LLVKWidgetImage> ascendingImage, descendingImage;
    };
    struct ScrollListParams
    {
        std::vector<ListColumn> columns;
        std::vector<ListRow> rows;
        ScrollbarParams scrollbar;
        LLVKControl::Params scrollbarControl;
        LLVKBorder::Params border;
        std::shared_ptr<const ListHeaderParams> header;
        bool multiSelect = false, heading = false, drawBorder = false, background = true, stripes = true;
        bool commitOnSelection = false, commitOnKeyboard = true, wheelOpaque = true;
        std::int32_t searchColumn = 0;
        std::int32_t sortColumn = -1;
        bool sortAscending = true;
        enum class Selection { Row, Cell, Header };
        Selection selection = Selection::Row;
        bool canSort = true;
        LLVKColor highlightedColor{1,1,1,1};
        std::int32_t headingHeight = 23, rowPadding = 2, columnPadding = 5, scrollbarSize = 16;
        LLVKColor foreground{1,1,1,1}, selectedForeground{1,1,1,1}, disabledForeground{0.5f,0.5f,0.5f,1};
        LLVKColor selectedBackground{0.2f,0.4f,0.7f,1}, writableBackground{0,0,0,1}, readonlyBackground{0,0,0,1};
        LLVKColor stripeColor{0.1f,0.1f,0.1f,1}, hoveredColor{0.3f,0.3f,0.3f,1};
    };
    struct Scrollbar
    {
        std::shared_ptr<const ScrollbarParams> params;
        std::int32_t documentSize = 0, position = 0, pageSize = 0, thickness = 0;
        bool documentChanged = false;
        LLVKScrollLayout::Rect thumb;
        LLVKScrollLayout::Rect dragThumb;
        std::int32_t dragStart = 0;
        std::int64_t lastDragDelta = 0;
        float glow = 0.f;
        Id decrease = 0, increase = 0;
    };
    struct SpinnerParams
    {
        float minimum = 0.f, maximum = 1.f, increment = 0.1f;
        std::int32_t precision = 3, labelWidth = 40, buttonWidth = 16, buttonHeight = 10, spacing = 2;
        std::string label;
        bool dynamicHeight = false, digitsOnly = false, labelWrap = false;
        LLVKColor textEnabledColor{1,1,1,1}, textDisabledColor{0.5f,0.5f,0.5f,1};
        LLVKControl::Params buttonControl, editorControl;
        LLVKButton::Params upButton, downButton;
        LineEditorParams editor;
    };
    struct Spinner
    {
        std::shared_ptr<const SpinnerParams> params;
        Id editor = 0, up = 0, down = 0, label = 0;
        std::uint64_t generation = 0;
    };
    struct RadioItemParams
    {
        Params view;
        std::shared_ptr<const LLVKWidgetLayout> layout;
        LLVKControl::Params control;
        CheckBoxConstruction check;
        std::optional<LLSD> payload;
    };
    struct RadioGroup
    {
        struct Item { Id control = 0; LLSD payload; };
        std::vector<Item> items;
        std::int32_t selected = -1;
        bool allowDeselect = false;
    };
    struct SliderParams
    {
        float minimum = 0.f, maximum = 1.f, increment = 0.1f, initial = 0.f;
        bool vertical = false;
        std::shared_ptr<const LLVKWidgetImage> thumb, pressedThumb, disabledThumb, track, highlight;
        LLVKColor outlineColor{1,1,1,1}, centerColor{1,1,1,1};
        LLVKControl::Callback mouseDown, mouseUp;
    };
    struct Slider
    {
        std::shared_ptr<const SliderParams> params;
        Rect thumb, dragStart;
        std::int32_t mouseOffset = 0;
    };
    struct SliderControlParams
    {
        SliderParams bar;
        LLVKControl::Params editorControl;
        LineEditorParams editor;
        std::string label;
        std::optional<std::int32_t> labelWidth, textWidth;
        std::int32_t precision = 3, spacing = 4;
        bool showText = true, editable = false;
        LLVKColor textColor{1,1,1,1}, disabledColor{0.5f,0.5f,0.5f,1};
        LLVKControl::Callback editorCommit;
    };
    struct SliderControl
    {
        std::shared_ptr<const SliderControlParams> params;
        Id bar = 0, label = 0, editor = 0, text = 0;
        std::uint64_t generation = 0;
    };
    struct Node
    {
        struct Floater
        {
            std::string title, positioning;
            std::int32_t legacyHeaderHeight = 18;
            bool saveRect = false, singleInstance = false;
            bool canClose = true, canMinimize = true;
        };
        std::optional<Floater> floater;
        struct TextEditor
        {
            struct Revision { std::string before, after; std::size_t cursorBefore=0, cursorAfter=0; };
            struct History { std::vector<Revision> revisions; std::size_t position=0, bytes=0; };
            std::shared_ptr<History> history=std::make_shared<History>();
            Id scroller = 0, document = 0, body = 0, border = 0;
            bool readOnly = true;
            bool commitOnFocusLost = false;
            std::uint64_t laidOutGeneration = UINT64_MAX;
            std::int32_t width = -1, height = -1;
        };
        std::optional<TextEditor> textEditor;
        struct TabContainer
        {
            struct Tab { Id panel = 0, button = 0; };
            std::vector<Tab> tabs;
            Id selected = 0;
            std::uint64_t selectionGeneration = 0;
            std::int32_t scrollPosition = 0, maximumScroll = 0;
            std::int32_t scrollPixels = 0, targetScrollPixels = 0;
            Id previousArrow = 0, nextArrow = 0;
            Id firstArrow = 0, lastArrow = 0;
            double lastArrowStep = 0.0;
            bool arrowHeld = false;
            struct Layout
            {
                enum class Position { Top, Bottom, Left };
                Position position = Position::Top;
                std::int32_t tabHeight = 21, minimumWidth = 60, maximumWidth = 160;
                std::int32_t labelPadding = 0, horizontalPadding = 0, panelOverlap = 0;
                std::int32_t verticalHeight = 23, verticalPadding = 0, rightPadding = 0;
                std::int32_t verticalArrowSize = 0;
                std::int32_t horizontalArrowSize = 0, partialTabWidth = 0;
                bool hidden = false, panelOffset = false;
            };
            std::optional<Layout> layout;
        };
        std::optional<TabContainer> tabContainer;
        struct Browser
        {
            std::string startUrl, mimeType, errorUrl;
            bool trusted = false, focusOnClick = true, borderVisible = true;
            bool decoupleSize = false;
            std::int32_t textureWidth = 1024, textureHeight = 1024;
        };
        std::optional<Browser> browser;
        struct LayoutPanel
        {
            bool autoResize = true;
            std::int32_t minimum = 0, expandedMinimum = 0, maximum = INT32_MAX, target = 0;
            float fraction = 0.f;
            float visibleAmount = 1.f;
        };
        struct LayoutStack
        {
            bool vertical = false, clip = true;
            std::int32_t spacing = 0;
            std::vector<Id> panels;
            bool animate = true;
            float openTime = 0.02f, closeTime = 0.03f;
            bool needsLayout = true;
        };
        std::optional<LayoutPanel> layoutPanel;
        std::optional<LayoutStack> layoutStack;
        Params params;
        std::optional<LLVKControl> control;
        std::optional<LLVKIcon> icon;
        std::optional<LLVKButton> button;
        std::optional<LLVKBadge> badge;
        std::optional<LLVKPlainControl> plainText;
        std::optional<CheckBox> checkBox;
        std::optional<LLVKPanel> panel;
        std::optional<LLVKBorder> border;
        std::optional<LineEditor> lineEditor;
        std::optional<SearchEditor> searchEditor;
        std::optional<ScrollList> scrollList;
        std::optional<ColorSwatch> colorSwatch;
        std::optional<ColorPicker> colorPicker;
        std::vector<Id> preferenceLocalValues;
        std::optional<Scrollbar> scrollbar;
        std::optional<Combo> combo;
        std::optional<Spinner> spinner;
        std::optional<RadioGroup> radioGroup;
        std::optional<Slider> slider;
        std::optional<SliderControl> sliderControl;
        Id comboListOwner = 0;
        struct ScrollContainer
        {
            Id border = 0, vertical = 0, horizontal = 0, document = 0;
            std::int32_t scrollbarSize = 0;
            bool useSizeSetting = false;
            bool hideScrollbars = false, reserveCorner = false;
            float minAutoRate = 100.f, maxAutoRate = 1000.f, autoRate = 0.f;
            std::int32_t maxAutoZone = 16;
            bool autoScrolling = false;
            bool opaque = false;
            bool ignoreArrowKeys = false;
            LLVKColor backgroundColor{0,0,0,1};
        };
        std::optional<ScrollContainer> scrollContainer;
        bool acceptsBadge = false;
        std::optional<BadgeConstruction> badgeConstruction;
        Id parent = 0;
        std::vector<Id> children;
        std::int32_t lastTabGroup = 0;
    };

    enum class PointerKind { LeftDown, LeftUp, RightDown, RightUp, DoubleClick, Hover, MiddleDown };
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
    std::optional<Id> createBrowser(const Params& view, const LLVKControl::Params& control,
        const LLVKPanel::Params& panel, const Node::Browser& browser, Id parent, std::string& error);
    bool routeWheel(Id root, std::int32_t x, std::int32_t y, std::int32_t clicks, bool horizontal, std::string& error);
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
        std::function<void(Id,bool)> languageInput;
        std::function<void(Id)> badKeystroke;
        std::function<void(Id)> hideCursor;
        std::function<void(Id)> textCursor;
        std::function<void(Id)> tabInto;
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
    using SettingCallback = std::function<void(const LLSD&,const LLSD&)>;
    std::optional<std::uint64_t> subscribeSetting(const std::string& name, SettingCallback callback);
    bool unsubscribeSetting(std::uint64_t subscription);
    struct PreferenceSnapshot
    {
        std::map<std::string,LLSD> settings;
        std::map<Id,LLSD> colors;
        std::map<Id,LLSD> localValues;
    };
    bool bindPreferenceColorAlpha(Id panel, std::shared_ptr<LLVKColorTable> colors, std::string& error);
    std::optional<PreferenceSnapshot> snapshotPreferences(Id root, std::string& error) const;
    bool restorePreferences(const PreferenceSnapshot& snapshot, const std::vector<std::string>& skip, std::string& error);
    std::optional<LLSD> setting(const std::string& name) const
    { const auto found = mSettings.find(name); return found == mSettings.end() ? std::nullopt : std::optional(found->second); }
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
    std::optional<std::vector<Id>> tabOrder(Id root, std::string& error, bool textOnly = false) const;
    bool focusFirst(Id root, bool flash, std::string& error);
    bool moveFocus(Id root, bool forward, bool textOnly, std::string& error);
    Id rootMostFocusRoot(Id control) const;
    enum class PanelKey { Escape, Tab, Return };
    bool panelKey(Id id, PanelKey key, LLVKLineEditor::Modifiers modifiers, std::string& error);
    bool setPanelDefaultButton(Id id, Id button, std::string& error);
    bool initializeTabContainer(Id panel, std::string& error);
    bool scrollTabStrip(Id container, std::int32_t rows, std::string& error);
    bool createVerticalTabArrows(Id container, const LLVKControl::Params& control,
        const LLVKButton::Params& button, std::string& error);
    bool createTabArrows(Id container, const LLVKControl::Params& control,
        const LLVKButton::Params& button, std::string& error);
    bool initializeFloater(Id panel, const Node::Floater& params, std::string& error);
    bool attachTabPanel(Id container, Id panel, Id button, std::string& error);
    bool selectTabPanel(Id container, Id panel, std::string& error);
    bool layoutTopTabs(Id container, const Node::TabContainer::Layout& layout, std::string& error);
    bool layoutTabPanels(Id container, const Node::TabContainer::Layout& layout, std::string& error, float frameDelta = 0.f);
    std::optional<Id> createLayoutStack(const Params& view, bool vertical, std::int32_t spacing, bool clip, Id parent, std::string& error);
    bool attachLayoutPanel(Id stack, Id panel, const Node::LayoutPanel& params, std::string& error);
    bool updateLayoutStack(Id id, std::string& error, float frameDelta = 0.f);
    bool prepareLayoutStacks(Id root, float frameDelta, std::string& error);
    bool configureLayoutStack(Id id, bool animate, float openTime, float closeTime, std::string& error);
    std::optional<Id> createCombo(const Params& view, const LLVKControl::Params& control, const ComboParams& params, Id parent, std::string& error);
    bool selectComboItem(Id id, std::optional<std::size_t> index, std::string& error);
    bool setComboValue(Id id, const LLSD& value, std::string& error);
    bool replaceComboItems(Id id, std::vector<ComboItem> items, std::string& error);
    bool commitCombo(Id id);
    bool postBuildCombo(Id id, std::string& error);
    bool showComboList(Id id, std::string& error);
    bool hideComboList(Id id);
    bool refreshComboText(Id id, std::optional<LLVKLineEditor::Key> key, std::string& error);
    float focusFlashAmount() const noexcept;
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
    std::optional<Id> createLineEditor(const Params& view, const LLVKControl::Params& control,
                                      const LineEditorParams& editor, Id parent, std::string& error);
    bool clearLineEditor(Id id, std::string& error);
    bool setLineEditorPassword(Id id, bool password);
    bool setLineEditorKeystroke(Id id, LLVKControl::Callback callback);
    bool selectLineEditorAll(Id id, std::string& error);
    bool setControlCommit(Id id, LLVKControl::Callback callback);
    std::optional<Id> createSpinner(const Params& view, const LLVKControl::Params& control,
        const SpinnerParams& params, Id parent, std::string& error);
    bool commitSpinner(Id id, std::string& error);
    bool stepSpinner(Id id, bool increase, LLVKLineEditor::Modifiers modifiers, std::string& error);
    void setInputModifiers(LLVKLineEditor::Modifiers modifiers) { mInputModifiers = modifiers; }
    bool setSpinnerValue(Id id, const LLSD& value, bool forceEditor, std::string& error);
    std::optional<Id> createRadioGroup(const Params& view, const LLVKControl::Params& control,
        std::span<const RadioItemParams> items, bool allowDeselect, Id parent, std::string& error);
    bool selectRadioIndex(Id id, std::int32_t index, bool publish, std::string& error);
    bool setRadioValue(Id id, const LLSD& value, std::string& error);
    bool radioKey(Id id, bool forward, std::string& error);
    bool setRadioIndexEnabled(Id id, std::int32_t index, bool enabled, std::string& error);
    std::optional<Id> createSlider(const Params& view, const LLVKControl::Params& control,
        const SliderParams& params, Id parent, std::string& error);
    bool setSliderValue(Id id, float value, bool publish, bool commit, std::string& error);
    bool updateSliderThumb(Id id, std::string& error);
    bool sliderPointer(Id id, const PointerEvent& event, std::string& error);
    bool sliderStep(Id id, std::int32_t steps, std::string& error);
    std::optional<Id> createSliderControl(const Params& view, const LLVKControl::Params& control,
        const SliderControlParams& params, Id parent, std::string& error);
    bool setSliderControlValue(Id id, const LLSD& value, std::string& error);
    bool commitSliderControl(Id id, bool fromEditor, std::string& error);
    bool updateSliderControlText(Id id, std::string& error);
    bool commitLineEditor(Id id);
    bool enableLineHistory(Id id, bool enabled);
    bool lineEditorUnicode(Id id, char32_t character, bool overwrite, std::string& error);
    bool lineEditorUnicode(Id id, char32_t character, std::string& error);
    bool overwriteMode() const noexcept { return mOverwrite; }
    bool lineEditorKey(Id id, LLVKLineEditor::Key key, LLVKLineEditor::Modifiers modifiers, std::string& error);
    bool canLineEditorDelete(Id id) const;
    bool deleteLineEditor(Id id, std::string& error);
    bool pasteLineEditorText(Id id, std::u32string text, bool primary, std::string& error);
    void setClipboard(std::shared_ptr<LLVKClipboard> clipboard) { mClipboard = std::move(clipboard); }
    bool canLineEditorCopy(Id id) const;
    bool canLineEditorCut(Id id) const;
    bool canLineEditorPaste(Id id, bool primary) const;
    bool copyLineEditor(Id id, bool primary, std::string& error);
    bool cutLineEditor(Id id, std::string& error);
    bool pasteLineEditor(Id id, bool primary, std::string& error);
    bool resetLinePreedit(Id id, std::string& error);
    bool updateLinePreedit(Id id, std::u32string_view text, const std::vector<std::size_t>& segments,
        const std::vector<bool>& standouts, std::size_t caret, std::string& error);
    bool markLinePreedit(Id id, std::size_t position, std::size_t length, std::string& error);
    struct EditorView
    {
        bool applicationFocused = true;
        double secondsSinceKeystroke = 0;
        float drawAlpha = 1.f, transparency = 1.f;
        LLVKColor::Value focusColor{1,1,1,1};
        std::int32_t focusWidth = 1;
    };
    struct EditorDraw
    {
        struct Part
        {
            Rect rectangle;
            LLVKColor::Value color;
            std::shared_ptr<const LLVKWidgetImage> image;
            bool solidImage = false;
            std::optional<LLVKFont::LineLayout> text;
        };
        std::vector<Part> parts;
        bool caretVisible = false;
        std::int32_t caretX = 0, imeY = 0;
    };
    std::optional<EditorDraw> prepareLineEditor(Id id, const EditorView& view, std::string& error);
    std::optional<Id> createScrollbar(const Params& view, const LLVKControl::Params& control,
        const ScrollbarParams& params, Id parent, std::string& error);
    bool setScrollPosition(Id id, std::int32_t position, bool updateThumb, std::string& error);
    bool setScrollDocumentSize(Id id, std::int32_t size, std::string& error);
    bool setScrollPageSize(Id id, std::int32_t size, std::string& error);
    enum class ScrollKey { Home, End, Up, Down, PageUp, PageDown, Left, Right };
    bool tabContainerKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error);
    bool moveTab(Id id, bool forward, std::string& error);
    bool scrollbarKey(Id id, ScrollKey key, std::string& error);
    bool scrollbarWheel(Id id, std::int32_t clicks, bool horizontal, std::string& error);
    struct ScrollbarDraw
    {
        struct Primitive
        {
            Rect rectangle;
            LLVKColor::Value color;
            std::shared_ptr<const LLVKWidgetImage> image;
            bool solidImage = false, additive = false;
        };
        std::vector<Primitive> primitives;
        std::vector<Id> children;
    };
    std::optional<ScrollbarDraw> prepareScrollbar(Id id, std::int32_t mouseX, std::int32_t mouseY,
        float frameDelta, LLVKColor::Value focusColor, std::string& error);
    struct ScrollContainerParams
    {
        LLVKControl::Params scrollbarControl;
        ScrollbarParams vertical, horizontal;
        LLVKBorder::Params border;
        std::optional<std::int32_t> size;
        bool borderVisible = false, hideScrollbars = false, reserveCorner = false;
        float minAutoRate = 100.f, maxAutoRate = 1000.f;
        std::int32_t maxAutoZone = 16;
        bool opaque = false;
        bool ignoreArrowKeys = false;
        LLVKColor backgroundColor{0,0,0,1};
    };
    std::optional<Id> createScrollContainer(const Params& view, const LLVKControl::Params& control,
        const ScrollContainerParams& params, Id parent, std::string& error);
    bool attachScrollContent(Id id, Id child, std::int32_t tabGroup, std::string& error);
    bool scrollContainerKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error);
    bool updateScrollContainer(Id id, std::string& error);
    std::optional<Rect> scrollContentWindow(Id id, std::string& error);
    std::optional<Rect> scrollToReveal(Id id, const Rect& target, const Rect& constraint, std::string& error);
    bool advanceScrollFrame(Id id, float frameDelta, std::string& error);
    bool autoScroll(Id id, std::int32_t x, std::int32_t y, const Rect& rootInLocal,
        float frameDelta, bool apply, std::string& error);
    struct ScrollContainerDraw
    {
        Rect background, documentClip;
        LLVKColor::Value backgroundColor;
        bool backgroundVisible = false;
        Id document = 0;
        std::vector<Id> chrome;
    };
    std::optional<ScrollContainerDraw> prepareScrollContainer(Id id, float transparency, std::string& error);
    struct PreeditLocation
    {
        std::int32_t x = 0, y = 0, fontSize = 0;
        Rect bounds, control;
        std::size_t position = 0, length = 0;
    };
    std::optional<PreeditLocation> linePreeditLocation(Id id, std::int32_t queryOffset,
        float scaleX, float scaleY, std::string& error) const;
    std::optional<Id> createPlainText(const Params& view, const LLVKControl::Params& control,
                                     const LLVKPlainControl::Params& text, Id parent, std::string& error);
    bool setPlainText(Id id, std::string text, std::string& error);
    bool setPlainTextArgument(Id id, std::string key, std::string replacement, std::string& error);
    bool reflowPlainText(Id id, std::string& error);
    bool fitPlainText(Id id, std::string& error);
    bool setPlainTextClicked(Id id, std::function<void(Id)> callback);
    std::optional<std::size_t> plainTextLinkAt(Id id, std::int32_t x, std::int32_t y, std::string& error);
    std::optional<std::size_t> plainTextIndexAt(Id id, std::int32_t x, std::int32_t y, std::string& error);
    bool selectAllPlainText(Id id);
    bool deselectPlainText(Id id);
    bool copyPlainText(Id id, std::string& error);
    std::optional<Id> createSearchEditor(const Params& view, const LLVKControl::Params& control,
        const SearchEditorParams& params, Id parent, std::string& error);
    bool refreshSearchEditor(Id id, std::string& error);
    bool clearSearchEditor(Id id, std::string& error);
    std::optional<Id> createScrollList(const Params& view, const LLVKControl::Params& control,
        const ScrollListParams& params, Id parent, std::string& error);
    bool setScrollListRows(Id id, std::vector<ListRow> rows, std::string& error);
    bool layoutScrollList(Id id, std::string& error);
    bool sortScrollList(Id id, std::size_t column, bool ascending, std::string& error);
    bool setScrollListCommitOnSelection(Id id, bool enabled);
    bool selectScrollListValue(Id id, const LLSD& value, bool selected, std::string& error);
    bool scrollListPointer(Id id, const PointerEvent& event, std::string& error);
    bool scrollListKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error);
    std::optional<Id> createColorSwatch(const Params& view, const LLVKControl::Params& control,
        const ColorSwatchParams& params, Id parent, std::string& error);
    bool setColorSwatchValue(Id id, const LLSD& value, std::string& error);
    bool beginColorSelection(Id id, std::string& error);
    bool refreshColorSwatch(Id id, std::string& error);
    bool colorSwatchPointer(Id id, const PointerEvent& event, std::string& error);
    bool showColorSwatchPicker(Id id, bool takeFocus, std::string& error);
    bool applyColorSelection(Id id, const LLVKColor::Value& color, ColorPickOperation operation, std::string& error);
    bool initializeColorPicker(Id picker, Id swatch, std::string& error, std::function<void()> close = {});
    bool finishColorPicker(Id picker, bool accept, std::string& error);
    void closeColorSwatchPickers(Id swatch);
    bool setColorPickerRgb(Id picker, const LLVKColor::Value& color, bool preview, std::string& error);
    bool commitColorPickerField(Id picker, Id field, std::string& error);
    bool syncColorPickerFields(Id picker, std::string& error);
    bool setColorPickerPalette(Id picker, std::shared_ptr<LLVKColorTable> colors, std::string& error);
    bool copyColorPickerLsl(Id picker, std::string& error);
    bool colorPickerPointer(Id picker, const PointerEvent& event, std::string& error);
    std::optional<Id> createTextEditor(const Params& view, const LLVKControl::Params& control,
        const LLVKPlainControl::Params& text, const ScrollContainerParams& scroller, bool borderVisible,
        Id parent, std::string& error);
    bool layoutTextEditor(Id id, std::string& error);
    bool setTextEditorText(Id id, const std::string& text, std::string& error);
    bool insertTextEditorText(Id id, std::u32string_view text, std::string& error);
    bool commitTextEditor(Id id, std::string& error);
    bool undoTextEditor(Id id, bool redo, std::string& error);
    bool deleteTextEditor(Id id, bool backward, bool word, std::string& error);
    bool pasteTextEditor(Id id, std::string& error);
    bool cutTextEditor(Id id, std::string& error);
    std::optional<Rect> plainTextCaretRect(Id body, std::string& error) const;
    bool revealTextEditorCursor(Id id, std::string& error);
    bool startTextEditorDocument(Id id, std::string& error);
    bool textEditorKey(Id id, ScrollKey key, LLVKLineEditor::Modifiers modifiers, std::string& error);
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
    std::optional<Id> constructPanel(const Params& view, const LLVKControl::Params& control,
                                    const LLVKPanel::Params& panel, std::string& error);
    bool initializePanel(Id id, const Params& view, const LLVKControl::Params& control,
                          const LLVKPanel::Params& panel, std::string& error);
    bool addPanelBorder(Id id, const LLVKBorder::Params& border, std::string& error);
    bool removePanelBorder(Id id, std::string& error);
    bool setPanelFilename(Id id, const std::string& filename);
    std::optional<std::string> panelString(Id id, const std::string& name,
        const LLVKLabel::Arguments& arguments, std::string& error) const;
    bool setButtonToggle(Id id, bool selected, std::string& error);
    struct ButtonView
    {
        std::int32_t mouseX = 0, mouseY = 0;
        bool spaceDown = false, returnDown = false;
        float frameDelta = 0.f, drawAlpha = 1.f, transparency = 1.f;
        LLVKColor::Value focusColor{1,1,1,1};
        std::int32_t focusWidth = 1;
    };
    struct ButtonDraw
    {
        struct Primitive
        {
            Rect rectangle;
            LLVKColor::Value color;
            std::shared_ptr<const LLVKWidgetImage> image;
            bool solidImage = false, additive = false, outline = false;
        };
        std::vector<Primitive> primitives;
        std::u32string label;
        LLVKFont::LineOptions text;
        LLVKColor::Value labelColor;
        bool shadow = false;
        std::shared_ptr<LLVKFont> font;
    };
    std::optional<ButtonDraw> prepareButton(Id id, const ButtonView& view, std::string& error);
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
    std::shared_ptr<const LLVKWidgetImage> findImage(const std::string& name, std::string& error) const;
    void setSkinImages(std::shared_ptr<LLVKSkinImages> images) { mSkinImages = std::move(images); }
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
    bool setShape(Id id, const Rect& rectangle, std::string& error);
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
    Id nextWidgetId() const noexcept { return mNextId; }
    static std::uint8_t parseFollows(const std::string& text);

private:
    friend class LLVKWidgetFactory;
    struct ShapeChanges
    {
        std::map<Id,Rect> rectangles;
        std::map<Id,LLVKLineEditor> editors;
        std::map<Id,LLVKScrollLayout::Rect> thumbs;
        std::vector<Id> scrollContainers;
    };
    bool planReshape(Id id, std::int64_t width, std::int64_t height, const Rect& origin,
                     ShapeChanges& changes, std::string& error) const;
    void publishShapes(ShapeChanges& changes);
    bool completeShapes(ShapeChanges& changes, std::string& error);
    bool finishScrollResize(Id id, std::string& error);
    static bool validRect(const Rect& rect) noexcept;
    void eraseSubtree(Id id);
    bool hasAncestor(Id id, Id ancestor) const noexcept;
    bool canReceiveFocus(Id id) const noexcept;
    bool enterFocus(Id target, bool flash, std::string& error);
    void notify(Id id, std::function<void(Id)> Events::* event);
    void notifyVisibility(Id id, bool visible);
    void lineLanguageInput(Id id, bool forceOff = false);
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
                                       std::optional<LLVKPanel> panel = std::nullopt,
                                       std::optional<LineEditor> lineEditor = std::nullopt,
                                       std::optional<Scrollbar> scrollbar = std::nullopt,
                                       std::shared_ptr<const ScrollContainerParams> scrollContainer = {},
                                       std::optional<Combo> combo = std::nullopt,
                                       std::optional<Node::Browser> browser = std::nullopt,
                                       std::optional<Spinner> spinner = std::nullopt);
    bool constructSpinnerChildren(Id id, std::string& error);
    bool refreshSpinnerEditor(Id id, std::string& error);
    bool publishSpinnerValue(Id id, float value, std::string& error);
    bool constructComboChildren(Id id, std::string& error);
    bool comboListPointer(Id id, const PointerEvent& event, std::string& error);
    bool constructScrollContainerChildren(Id id, const ScrollContainerParams& params, std::string& error);
    bool constructScrollbarChildren(Id id, std::string& error);
    bool refreshScrollbarThumb(Id id, std::string& error);
    bool constructCheckBoxChildren(Id id, std::string& error);
    bool commitCheckBox(Id id);
    void updateLineHistory(Id id);
    bool finishLineEdit(Id id, const LLVKLineEditor& rollback, bool forceRollback);
    bool planCheckBoxReshape(Id id, std::int64_t width, ShapeChanges& changes, std::string& error) const;
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
    bool plainTextPointer(Id id, const PointerEvent& event, std::string& error);
    bool lineEditorPointer(Id id, PointerEvent event, std::string& error);
    bool scrollbarPointer(Id id, PointerEvent event, std::string& error);
    bool handleWheel(Id id, std::int32_t x, std::int32_t y, std::int32_t clicks, bool horizontal, std::string& error);
    void cursorEffect(Id id, bool hand);
    void updateFlashSettings();
    std::map<Id,Node> mNodes;
    std::map<Id,Events> mEvents;
    std::map<std::string,LLSD> mSettings;
    std::map<std::string,SettingType> mSettingTypes;
    struct SettingSubscription { std::string name; SettingCallback callback; };
    std::map<std::uint64_t,SettingSubscription> mSettingSubscriptions;
    std::uint64_t mNextSettingSubscription = 1;
    std::map<std::string,std::shared_ptr<const LLVKWidgetImage>> mImages;
    std::shared_ptr<LLVKSkinImages> mSkinImages;
    std::shared_ptr<LLVKClipboard> mClipboard;
    std::set<Id> mErasing;
    std::vector<Id> mFocusChain;
    Id mKeyboardFocus = 0;
    Id mMouseCapture = 0;
    Id mTopControl = 0;
    Id mLockedFocus = 0;
    std::uint64_t mFocusEpoch = 0;
    bool mKeystrokesOnly = false;
    bool mOverwrite = false;
    Id mNextId = 1;
    double mTime = 0.0;
    double mFocusFlashTime = 0.0;
    LLVKLineEditor::Modifiers mInputModifiers;
    LLVKLabel::Context mLabelContext;
};

#endif
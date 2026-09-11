#ifndef LLVKWIDGETFACTORY_H
#define LLVKWIDGETFACTORY_H

#include "llvkwidgetlayout.h"
#include "llvkskinfiles.h"
#include <string_view>

class LLVKWidgetFactory final
{
public:
    struct Defaults
    {
        LLVKWidgetTree::Params view;
        LLVKWidgetLayout geometry;
    };
    struct IconDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKIcon::Params icon;
        IconDefaults();
    };
    struct BadgeDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKBadge::Params badge;
        BadgeDefaults();
    };
    struct ButtonDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKButton::Params button;
        BadgeDefaults badge;
        std::optional<BadgeDefaults> providedBadge;
        ButtonDefaults();
    };
    struct Callbacks
    {
        std::map<std::string,std::function<void(LLVKWidgetTree::Id,const LLSD&)>> actions;
        std::map<std::string,std::function<bool(LLVKWidgetTree::Id,const LLSD&)>> predicates;
        std::map<std::string,std::function<bool(std::u32string_view)>> textValidators;
    };
    struct LineEditorDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::LineEditorParams editor;
        bool borderProvided = false;
        LineEditorDefaults();
    };
    struct CheckBoxDefaults
    {
        Defaults view, labelView, buttonView;
        LLVKControl::Params control;
        LLVKWidgetTree::CheckBoxConstruction construction;
    };
    struct SearchEditorDefaults
    {
        LineEditorDefaults editor;
        LLVKWidgetTree::SearchEditorParams search;
        ButtonDefaults searchButton, clearButton;
        std::optional<std::string> highlightImage;
        bool initialized = false;
    };
    struct PanelDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKPanel::Params panel;
        Defaults borderView;
        PanelDefaults();
    };
    struct ScrollbarDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::ScrollbarParams scrollbar;
        LLVKControl::Callback changed;
        std::map<std::string,ButtonDefaults> buttons;
    };
    struct ScrollContainerDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::ScrollContainerParams container;
        LLVKControl::Callback scrolled;
    };
    struct ScrollListDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::ScrollListParams list;
        ScrollbarDefaults scrollbar;
        bool initialized = false;
    };
    struct LayoutDefaults
    {
        Defaults view;
        LLVKWidgetTree::Node::LayoutStack stack;
        LayoutDefaults() { stack.spacing = -1; }
    };
    struct ComboDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::ComboParams combo;
        ButtonDefaults button, dropDown;
        LineEditorDefaults editor;
        bool initialized = false;
    };
    struct TextDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKPlainControl::Params text;
        TextDefaults()
        {
            view.view.mouseOpaque = false;
            view.view.soundFlags = 0;
            control.tabStop = false;
        }
    };
    struct BrowserDefaults
    {
        PanelDefaults panel;
        LLVKWidgetTree::Node::Browser browser;
    };
    struct ColorSwatchDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::ColorSwatchParams swatch;
        TextDefaults caption;
        std::optional<std::string> alphaImage;
        bool initialized = false;
    };
    struct TextEditorDefaults
    {
        TextDefaults text;
        bool borderVisible = false, backgroundVisible = true, contextMenu = true;
        LLVKColor readOnlyBackground{0,0,0,1}, focusBackground{0,0,0,1}, cursorColor{1,1,1,1};
    };
    struct TabDefaults
    {
        PanelDefaults panel;
        LLVKWidgetTree::Node::TabContainer::Layout layout;
        std::optional<std::int32_t> width;
        std::array<std::map<std::string,std::string>,3> images;
        LLVKButton::Align alignment = LLVKButton::Align::Center;
        std::int32_t labelPadLeft = 4, labelPadBottom = 1;
        LLVKColor flashColor{1,1,1,1};
    };
    struct SpinnerDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::SpinnerParams spinner;
        ButtonDefaults up, down;
        LineEditorDefaults editor;
        bool initialized = false;
    };
    struct SliderDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::SliderParams slider;
        std::map<std::string,std::string> images;
    };
    struct SliderControlDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKWidgetTree::SliderControlParams slider;
        LineEditorDefaults editor;
        TextDefaults label, text;
        bool initialized = false;
    };
    class Construction
    {
    public:
        using Function = std::function<std::optional<LLVKWidgetTree::Id>(std::string_view,LLVKWidgetTree::Id,std::string&)>;
        Construction() = default;
        explicit Construction(Function function) : mFunction(std::move(function)) {}
        std::optional<LLVKWidgetTree::Id> construct(std::string_view xml, LLVKWidgetTree::Id parent, std::string& error) const
        {
            error.clear();
            if (!mFunction) { error = "Native construction context is unavailable"; return std::nullopt; }
            return mFunction(xml,parent,error);
        }
    private:
        Function mFunction;
    };
    struct PanelInstance;
    using PanelConstructor = std::function<PanelInstance(LLVKWidgetTree&,const PanelDefaults&,const Construction&,std::string&)>;
    struct PanelInstance
    {
        LLVKWidgetTree::Id id = 0;
        std::shared_ptr<Callbacks> callbacks;
        std::map<std::string,PanelConstructor> childFactories;
        std::function<bool(LLVKWidgetTree&,LLVKWidgetTree::Id,const Construction&,std::string&)> postBuild;
    };
    struct Resources
    {
        std::shared_ptr<LLVKSkinFiles> skinFiles;
        std::shared_ptr<LLVKColorTable> colors;
        std::shared_ptr<const TabDefaults> tabs = std::make_shared<TabDefaults>();
        std::shared_ptr<const SpinnerDefaults> spinner = std::make_shared<SpinnerDefaults>();
        std::shared_ptr<const SearchEditorDefaults> searchEditor = std::make_shared<SearchEditorDefaults>();
        std::shared_ptr<const ScrollListDefaults> scrollList = std::make_shared<ScrollListDefaults>();
        std::shared_ptr<const ButtonDefaults> scrollColumnHeader;
        std::shared_ptr<const ColorSwatchDefaults> colorSwatch = std::make_shared<ColorSwatchDefaults>();
        std::shared_ptr<const SliderDefaults> slider = std::make_shared<SliderDefaults>();
        std::shared_ptr<const SliderControlDefaults> sliderControl = std::make_shared<SliderControlDefaults>();
        std::shared_ptr<const CheckBoxDefaults> radioItem;
        LLVKControl::Params radioControl;
        std::function<void(LLVKWidgetTree::Id,const std::string&)> webLinkHandler;
        std::function<void(LLVKWidgetTree::Id,bool)> colorPickerHandler;
            std::shared_ptr<const TextEditorDefaults> textEditor = std::make_shared<TextEditorDefaults>();
        std::map<std::string,std::shared_ptr<LLVKFont>> fonts;
        std::shared_ptr<LLVKFontRegistry> fontRegistry;
        LLVKFontRegistry::Request defaultFontRequest;
        std::shared_ptr<LLVKFont> fallbackFont;
        std::map<std::string,std::string> declarations;
        std::map<std::string,std::vector<std::string>> declarationLayers;
        std::map<std::string,PanelConstructor> panelClasses;
        std::map<std::string,PanelConstructor> panelFactories;
    };
    explicit LLVKWidgetFactory(Defaults defaults);
    LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults);
    LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults, ButtonDefaults buttonDefaults, Callbacks callbacks,
                       Resources resources = {}, PanelDefaults panelDefaults = {}, LineEditorDefaults lineDefaults = {}, CheckBoxDefaults checkDefaults = {});
    bool loadDefaults(const LLVKWidgetTree& tree, std::string_view xml, std::string& error);
    bool loadDefaultsFile(const LLVKWidgetTree& tree, const std::string& filename, std::string& error);
    bool loadListContents(const LLVKWidgetTree& tree, const std::string& filename,
        LLVKWidgetTree::ScrollListParams& contents, std::string& error) const;
    std::optional<LLVKWidgetTree::Id> constructFile(LLVKWidgetTree& tree, const std::string& filename,
                                                  LLVKWidgetTree::Id parent, std::string& error) const;
    std::optional<LLVKWidgetTree::Id> construct(LLVKWidgetTree& tree, std::string_view xml,
                                              LLVKWidgetTree::Id parent, std::string& error) const;
    std::optional<LLVKWidgetTree::Id> construct(LLVKWidgetTree& tree,
                                              const LLVKWidgetTree::Params& params,
                                              LLVKWidgetTree::Id parent, std::string& error) const;
private:
    Defaults mDefaults;
    IconDefaults mIconDefaults;
    ButtonDefaults mButtonDefaults;
    Callbacks mCallbacks;
    Resources mResources;
    PanelDefaults mPanelDefaults;
    LineEditorDefaults mLineDefaults;
    CheckBoxDefaults mCheckDefaults;
    std::shared_ptr<const ScrollbarDefaults> mScrollDefaults = std::make_shared<ScrollbarDefaults>();
    std::shared_ptr<const ScrollContainerDefaults> mContainerDefaults = std::make_shared<ScrollContainerDefaults>();
    LayoutDefaults mLayoutDefaults;
    std::shared_ptr<const ComboDefaults> mComboDefaults = std::make_shared<ComboDefaults>();
    std::shared_ptr<const TextDefaults> mTextDefaults = std::make_shared<TextDefaults>();
    std::shared_ptr<const BrowserDefaults> mBrowserDefaults = std::make_shared<BrowserDefaults>();
};

#endif
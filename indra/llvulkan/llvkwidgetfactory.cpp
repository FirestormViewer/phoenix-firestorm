#include "llvkwidgetfactory.h"
#include "llvkmenu.h"
#include "lltrace.h"
#include "llvkxmllayers.h"
#include "llstring.h"

#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

#include <charconv>
#include <cmath>
#include <cwctype>
#include <exception>
#include <locale.h>
#include <memory>
#include <utility>

namespace
{
#if defined(_WIN32)
    struct TextValidationLocale
    {
        _locale_t handle = _create_locale(LC_ALL,"English_United States.1252");
        ~TextValidationLocale() { if (handle) _free_locale(handle); }
        TextValidationLocale() = default;
        TextValidationLocale(const TextValidationLocale&) = delete;
        TextValidationLocale& operator=(const TextValidationLocale&) = delete;
    };
#endif

    std::function<bool(std::u32string_view)> builtinTextValidator(std::string_view name)
    {
        if (name == "ascii" || name == "ascii_with_newline")
        {
            const bool allowNewline = name == "ascii_with_newline";
            return [allowNewline](std::u32string_view text)
            {
                return std::all_of(text.begin(),text.end(),[allowNewline](char32_t character)
                { return (character >= 0x20 && character <= 0x7f) || (allowNewline && character == U'\n'); });
            };
        }
        if (name == "ascii_printable_no_pipe" || name == "ascii_printable_no_space")
        {
            const bool allowSpace = name == "ascii_printable_no_pipe";
            return [allowSpace](std::u32string_view text)
            {
                return std::all_of(text.begin(),text.end(),[allowSpace](char32_t character)
                {
                    if (character < 0x20 || character > 0x7f) return false;
                    if (allowSpace && character == U'|') return false;
                    if (character == U' ') return allowSpace;
                    const auto code = static_cast<wint_t>(character);
                    return std::iswalnum(code) != 0 || std::iswpunct(code) != 0;
                });
            };
        }
#if defined(_WIN32)
        if (name == "float" || name == "int" || name == "positive_s32" || name == "non_negative_s32" ||
            name == "alpha_num" || name == "alpha_num_space")
        {
            auto locale = std::make_shared<TextValidationLocale>();
            if (!locale->handle) return {};
            return [locale,kind = std::string(name)](std::u32string_view text)
            {
                const auto space = [&](char32_t character)
                { return character <= 0xffff && _iswspace_l(static_cast<wint_t>(character),locale->handle) != 0; };
                const auto digit = [&](char32_t character)
                { return character <= 0xffff && _iswdigit_l(static_cast<wint_t>(character),locale->handle) != 0; };
                if (kind == "alpha_num" || kind == "alpha_num_space")
                {
                    return std::all_of(text.begin(),text.end(),[&](char32_t character)
                    {
                        if (kind == "alpha_num_space" && character == U' ') return true;
                        return character <= 0xffff && _iswalnum_l(static_cast<wint_t>(character),locale->handle) != 0;
                    });
                }
                while (!text.empty() && space(text.front())) text.remove_prefix(1);
                while (!text.empty() && space(text.back())) text.remove_suffix(1);
                if (kind == "positive_s32" || kind == "non_negative_s32")
                {
                    if (!text.empty() && (text.front() == U'-' || (kind == "positive_s32" && text.front() == U'0'))) return false;
                    if (!std::all_of(text.begin(),text.end(),digit)) return false;
                    if (kind == "non_negative_s32") return true;
                    return !text.empty() && text.front() >= U'1' && text.front() <= U'9';
                }
                if (!text.empty() && text.front() == U'-') text.remove_prefix(1);
                return std::all_of(text.begin(),text.end(),[&](char32_t character)
                { return digit(character) || (kind == "float" && character == U'.'); });
            };
        }
#endif
        return {};
    }

    struct Declaration
    {
        struct CallbackDeclaration
        {
            std::string tag;
            std::vector<std::pair<std::string,std::string>> attributes;
        };
        std::string tag;
        std::string menuXml;
        std::vector<std::pair<std::string,std::string>> attributes;
        std::vector<CallbackDeclaration> panelCallbacks;
        std::map<std::string,std::string> panelStrings;
        LLVKWidgetFactory::Defaults params;
        std::optional<LLVKControl::Params> control;
        std::optional<LLVKIcon::Params> icon;
        std::optional<LLVKButton::Params> button;
        std::optional<LLVKBadge::Params> badge;
        std::optional<LLVKPanel::Params> panel;
        std::optional<LLVKWidgetTree::Node::Floater> floater;
        std::shared_ptr<LLVKWidgetTree::Node::ContainerView> containerView;
        std::shared_ptr<LLVKWidgetTree::Node::StatBar> statBar;
        std::optional<LLVKWidgetTree::Node::OverlapPanel> overlapPanel;
        std::optional<LLVKWidgetTree::Node::ProgressBar> progress;
        std::shared_ptr<LLVKWidgetFactory::SearchEditorDefaults> searchEditor;
        std::map<std::string,std::unique_ptr<Declaration>> searchButtons;
        std::string menuFilename, menuPosition = "bottomleft";
        std::shared_ptr<LLVKWidgetFactory::ColorSwatchDefaults> colorSwatch;
        std::shared_ptr<LLVKWidgetFactory::TextureDefaults> texture;
        std::unique_ptr<Declaration> textureCaption, textureMultiple;
        std::unique_ptr<Declaration> swatchCaption;
        std::shared_ptr<LLVKWidgetFactory::ScrollListDefaults> scrollList;
        std::optional<LLVKWidgetTree::Node::Browser> browser;
        std::shared_ptr<LLVKWidgetFactory::TabDefaults> tabs;
        std::shared_ptr<LLVKWidgetFactory::SpinnerDefaults> spinner;
        std::shared_ptr<LLVKWidgetFactory::SliderDefaults> slider;
        std::shared_ptr<LLVKWidgetFactory::SliderControlDefaults> sliderControl;
        std::map<std::string,std::unique_ptr<Declaration>> sliderParts;
        bool radioGroup = false, radioItem = false, allowDeselect = false;
        bool explicitFont = false;
        std::optional<LLSD> radioPayload;
        std::map<std::string,std::unique_ptr<Declaration>> spinnerButtons;
        std::optional<LLVKBorder::Params> border;
        std::optional<LLVKWidgetTree::LineEditorParams> lineEditor;
        std::optional<LLVKWidgetFactory::CheckBoxDefaults> checkBox;
        std::shared_ptr<LLVKWidgetFactory::ScrollbarDefaults> scrollbar;
        std::shared_ptr<LLVKWidgetFactory::ScrollContainerDefaults> scrollContainer;
        std::optional<LLVKWidgetTree::Node::LayoutStack> layoutStack;
        std::optional<LLVKWidgetTree::Node::LayoutPanel> layoutPanel;
        bool expandedMinimumProvided = false;
        std::shared_ptr<LLVKWidgetFactory::ComboDefaults> combo;
        std::map<std::string,std::unique_ptr<Declaration>> comboParts;
        bool comboList = false;
        LLVKColor comboListBackground{1,1,1,1};
        bool comboListBackgroundVisible = true;
        std::map<std::string,std::unique_ptr<Declaration>> scrollButtons;
        std::map<std::string,std::string> scrollImages;
        std::optional<LLVKPlainControl::Params> plainLabel;
            std::shared_ptr<LLVKWidgetFactory::TextEditorDefaults> textEditor;
        std::string textBody;
        std::unique_ptr<Declaration> checkLabel, checkButton;
        bool lineBorderProvided = false;
        std::map<std::string,std::string> lineImages;
        std::string panelClass;
        LLVKWidgetFactory::PanelDefaults panelConstructor;
        std::optional<std::string> panelOpaqueImage, panelTransparentImage;
        LLVKWidgetTree::BadgeConstruction badgeConstruction;
        std::unique_ptr<Declaration> ownedBadge;
        std::optional<std::string> badgeImage, badgeBorderImage;
        std::map<std::string,std::string> buttonImages;
        std::optional<std::string> imageName;
        std::vector<std::unique_ptr<Declaration>> children;
    };

    bool integer(std::string_view text, std::int32_t& value)
    {
        const auto result = std::from_chars(text.data(),text.data()+text.size(),value);
        return result.ec == std::errc() && result.ptr == text.data()+text.size();
    }

    std::u32string labelText(std::string_view text)
    {
        const auto wide = utf8str_to_wstring(std::string(text));
        return std::u32string(wide.begin(),wide.end());
    }

    bool alignment(std::string_view text, LLVKButton::Align& value)
    {
        if (text == "left") value = LLVKButton::Align::Left;
        else if (text == "center") value = LLVKButton::Align::Center;
        else if (text == "right") value = LLVKButton::Align::Right;
        else return false;
        return true;
    }

    LLVKButton::Image* buttonImage(LLVKButton::Images& images, std::string_view name)
    {
        if (name == "image_unselected") return &images.unselected;
        if (name == "image_selected") return &images.selected;
        if (name == "image_disabled") return &images.disabled;
        if (name == "image_disabled_selected") return &images.disabledSelected;
        if (name == "image_hover_unselected") return &images.hover;
        if (name == "image_hover_selected") return &images.hoverSelected;
        if (name == "image_pressed") return &images.pressed;
        if (name == "image_pressed_selected") return &images.pressedSelected;
        if (name == "image_overlay") return &images.overlay;
        if (name == "image_flash") return &images.flash;
        return nullptr;
    }

    bool boolean(std::string_view text, bool& value)
    {
        if (text == "true" || text == "1") { value = true; return true; }
        if (text == "false" || text == "0") { value = false; return true; }
        return false;
    }

    std::string panelText(std::string text)
    {
        auto position = text.find_first_not_of(" \t\n");
        if (position == std::string::npos) return {};
        if (text[position] != '"')
        {
            auto result = text.substr(position,text.find_last_not_of(" \t\n")-position+1);
            std::erase(result,'\r');
            return result;
        }
        std::string result;
        std::size_t lines = 0;
        for (;;)
        {
            const auto start = ++position;
            auto scan = start;
            std::size_t end;
            for (;;)
            {
                end = text.find_first_of("\\\"",scan);
                if (end == std::string::npos || text[end] == '"') break;
                text.erase(end,1);
                scan = end+1;
            }
            if (end == std::string::npos) break;
            result.append(text,start,end-start);
            result.push_back('\n');
            ++lines;
            position = text.find('"',end+1);
            if (position == std::string::npos)
            {
                if (lines == 1) result.pop_back();
                break;
            }
        }
        return result;
    }

    struct Parser
    {
        XML_Parser parser = nullptr;
        const LLVKWidgetFactory::Defaults& defaults;
        const LLVKWidgetFactory::IconDefaults& iconDefaults;
        const LLVKWidgetFactory::ButtonDefaults& buttonDefaults;
        const LLVKWidgetFactory::Callbacks& callbacks;
        const LLVKWidgetFactory::Resources& resources;
        const LLVKWidgetFactory::PanelDefaults& panelDefaults;
        const LLVKWidgetFactory::LineEditorDefaults& lineDefaults;
        const LLVKWidgetFactory::CheckBoxDefaults& checkDefaults;
        const LLVKWidgetFactory::ScrollbarDefaults& scrollDefaults;
        const LLVKWidgetFactory::ScrollContainerDefaults& containerDefaults;
        const LLVKWidgetFactory::LayoutDefaults& layoutDefaults;
        const LLVKWidgetFactory::ComboDefaults& comboDefaults;
        const LLVKWidgetFactory::TextDefaults& textDefaults;
        const LLVKWidgetFactory::BrowserDefaults& browserDefaults;
        std::unique_ptr<Declaration> root;
        std::vector<Declaration*> stack;
        std::optional<LLVKWidgetTree::ListRow> inlineRow;
        std::optional<std::size_t> inlineCell;
        std::string inlineCellText;
        std::string error;
        std::exception_ptr exception;
        std::size_t nodes = 0;
        std::string_view source;
        std::size_t menuStart=0,menuDepth=0;
        bool callbackElement = false;
        struct PanelString
        {
            std::string name;
            std::optional<std::string> value;
            std::string body;
        };
        std::optional<PanelString> panelString;
        Parser(const LLVKWidgetFactory::Defaults& source, const LLVKWidgetFactory::IconDefaults& icons,
                    const LLVKWidgetFactory::ButtonDefaults& buttons, const LLVKWidgetFactory::Callbacks& handlers,
                    const LLVKWidgetFactory::Resources& assets, const LLVKWidgetFactory::PanelDefaults& panels,
                    const LLVKWidgetFactory::LineEditorDefaults& editors, const LLVKWidgetFactory::CheckBoxDefaults& checks,
                    const LLVKWidgetFactory::ScrollbarDefaults& scrolls, const LLVKWidgetFactory::ScrollContainerDefaults& containers,
                    const LLVKWidgetFactory::LayoutDefaults& layouts, const LLVKWidgetFactory::ComboDefaults& combos,
                    const LLVKWidgetFactory::TextDefaults& texts, const LLVKWidgetFactory::BrowserDefaults& browsers)
                : defaults(source), iconDefaults(icons), buttonDefaults(buttons), callbacks(handlers), resources(assets), panelDefaults(panels), lineDefaults(editors), checkDefaults(checks), scrollDefaults(scrolls), containerDefaults(containers), layoutDefaults(layouts), comboDefaults(combos), textDefaults(texts), browserDefaults(browsers) {}
        ~Parser() { if (parser) XML_ParserFree(parser); }

        void reject(const std::string& reason)
        {
            error = reason;
            XML_StopParser(parser,XML_FALSE);
        }

        template<class Operation> void guarded(Operation operation) noexcept
        {
            try { operation(); }
            catch (...) { exception = std::current_exception(); XML_StopParser(parser,XML_FALSE); }
        }

        bool color(std::string_view text, LLVKColor& output)
        {
            if (resources.colors)
                if (auto named = resources.colors->find(std::string(text))) { output = *named; return true; }
            LLVKColor::Value value;
            bool firstChannel=true;
            for (auto& channel : value)
            {
                auto start = text.find_first_not_of(" \t\r\n");
                if (start == std::string_view::npos) return false;
                text.remove_prefix(start);
                if (!firstChannel && text.front()==',')
                {
                    text.remove_prefix(1);
                    start=text.find_first_not_of(" \t\r\n");
                    if (start==std::string_view::npos) return false;
                    text.remove_prefix(start);
                }
                firstChannel=false;
                const auto result = std::from_chars(text.data(),text.data()+text.size(),channel);
                if (result.ec != std::errc() || !std::isfinite(channel)) return false;
                text.remove_prefix(result.ptr-text.data());
            }
            if (text.find_first_not_of(" \t\r\n") != std::string_view::npos) return false;
            output = LLVKColor(value);
            return true;
        }

        Declaration* comboPart(Declaration& owner, std::string_view name)
        {
            if (!owner.combo || (name != "combo_editor" && name != "combo_button" && name != "drop_down_button" && name != "combo_list" &&
                !(name=="action_button" && owner.combo->combo.flyout))) return nullptr;
            auto& part = owner.comboParts[std::string(name)];
            if (!part)
            {
                part = std::make_unique<Declaration>();
                if (name == "combo_editor")
                {
                    part->params = owner.combo->editor.view;
                    part->control = owner.combo->editor.control;
                    part->lineEditor = owner.combo->editor.editor;
                }
                else if (name == "combo_list")
                {
                    part->control = owner.combo->combo.listControl;
                    part->comboList = true;
                    part->comboListBackground = owner.combo->combo.listBackground;
                    part->comboListBackgroundVisible = owner.combo->combo.listBackgroundVisible;
                }
                else
                {
                    const auto& defaults = name == "action_button" ? owner.combo->action : name == "combo_button" ? owner.combo->button : owner.combo->dropDown;
                    part->params = defaults.view;
                    part->control = defaults.control;
                    part->button = defaults.button;
                }
            }
            return part.get();
        }

        bool attribute(Declaration& declaration, std::string_view name, std::string_view text)
        {
            auto& view = declaration.params.view;
            auto& geometry = declaration.params.geometry;
            if (declaration.overlapPanel && name=="min_width") return integer(text,declaration.overlapPanel->minimumWidth);
            if (declaration.statBar)
            {
                auto& bar=*declaration.statBar;
                if (name=="label") { bar.label=text; return true; }
                if (name=="stat") { bar.statName=text; return true; }
                if (name=="show_bar") return boolean(text,bar.showBar);
                if (name=="show_history") return boolean(text,bar.showHistory);
                if (name=="max_height") return integer(text,bar.maximumHeight);
                if (name=="decimal_digits") return integer(text,bar.decimalDigits);
                if (name=="num_frames") return integer(text,bar.historyFrames);
                if (name=="num_frames_short") return integer(text,bar.shortFrames);
                if (name=="bar_min" || name=="bar_max" || name=="tick_spacing")
                {
                    auto& value=name=="bar_min" ? bar.minimum : name=="bar_max" ? bar.maximum : bar.tickSpacing;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    return parsed.ec==std::errc() && parsed.ptr==text.data()+text.size() && std::isfinite(value);
                }
            }
            if (declaration.containerView)
            {
                auto& container=*declaration.containerView;
                if (name=="show_label") return boolean(text,container.showLabel);
                if (name=="display_children") return boolean(text,container.displayChildren);
                if (name=="background_visible") return boolean(text,container.backgroundVisible);
                if (name=="bg_color") return color(text,container.backgroundColor);
                if (name=="label") { container.label=text; return true; }
            }
            if (declaration.control && !declaration.panel)
            {
                if (name=="commit_callback.function") { declaration.control->commit.functionName=std::string(text); return true; }
                if (name=="commit_callback.parameter" || name=="commit_callback.userdata")
                { declaration.control->commit.parameter=LLSD(std::string(text)); return true; }
            }
            if (declaration.scrollList)
            {
                if (name=="desired_line_height") return integer(text,declaration.scrollList->list.desiredLineHeight);
                if (name=="content_type") return text=="Misc";
                auto& list=declaration.scrollList->list;
                if (name=="multi_select") return boolean(text,list.multiSelect);
                if (name=="draw_heading") return boolean(text,list.heading);
                if (name=="draw_border") return boolean(text,list.drawBorder);
                if (name=="draw_stripes") return boolean(text,list.stripes);
                if (name=="background_visible") return boolean(text,list.background);
                if (name=="mouse_wheel_opaque") return boolean(text,list.wheelOpaque);
                if (name=="commit_on_keyboard_movement") return boolean(text,list.commitOnKeyboard);
                if (name=="commit_on_selection_change") return boolean(text,list.commitOnSelection);
                if (name=="heading_height") return integer(text,list.headingHeight);
                if (name=="row_padding") return integer(text,list.rowPadding);
                if (name=="column_padding") return integer(text,list.columnPadding);
                if (name=="search_column") return integer(text,list.searchColumn);
                if (name=="sort_column") return integer(text,list.sortColumn);
                if (name=="sort_ascending") return boolean(text,list.sortAscending);
                if (name=="can_sort") return boolean(text,list.canSort);
                if (name=="selection_type")
                {
                    if (text=="row") list.selection=LLVKWidgetTree::ScrollListParams::Selection::Row;
                    else if (text=="cell") list.selection=LLVKWidgetTree::ScrollListParams::Selection::Cell;
                    else if (text=="header") list.selection=LLVKWidgetTree::ScrollListParams::Selection::Header;
                    else return false;
                    return true;
                }
                if (name=="label") { list.label=text; return true; }
                if (name=="fg_unselected_color") return color(text,list.foreground);
                if (name=="fg_selected_color") return color(text,list.selectedForeground);
                if (name=="fg_disable_color") return color(text,list.disabledForeground);
                if (name=="bg_selected_color") return color(text,list.selectedBackground);
                if (name=="bg_writeable_color") return color(text,list.writableBackground);
                if (name=="bg_readonly_color") return color(text,list.readonlyBackground);
                if (name=="bg_stripe_color") return color(text,list.stripeColor);
                if (name=="hovered_color") return color(text,list.hoveredColor);
                if (name=="highlighted_color") return color(text,list.highlightedColor);
                if (name=="scroll_bar_bg_visible") return boolean(text,list.scrollbar.backgroundVisible);
                if (name=="scroll_bar_bg_color") return color(text,list.scrollbar.backgroundColor);
                if (name=="border.name") return text=="dig border";
                if (name=="border.bevel_style")
                {
                    if (text=="in") list.border.bevel=LLVKBorder::Bevel::In;
                    else if (text=="out") list.border.bevel=LLVKBorder::Bevel::Out;
                    else if (text=="none") list.border.bevel=LLVKBorder::Bevel::None;
                    else return false;
                    return true;
                }
            }
            if (declaration.texture)
            {
                auto& texture=declaration.texture->texture;
                if (name=="label") { texture.label=text; return true; }
                if (name=="label_width") return integer(text,texture.labelWidth);
                if (name=="show_caption") { bool show; if (!boolean(text,show)) return false; texture.captionHeight=show ? 23 : 0; return true; }
                if (name=="allow_no_texture") return boolean(text,texture.allowNone);
                if (name=="can_apply_immediately") return boolean(text,texture.applyImmediately);
                if (name=="no_commit_on_selection") { bool disable; if (!boolean(text,disable)) return false; texture.commitOnSelection=!disable; return true; }
                if (name=="border_color") return color(text,texture.borderColor);
                if (name=="text_enabled_color") return color(text,texture.enabledText);
                if (name=="text_disabled_color") return color(text,texture.disabledText);
                if (name=="image_id" || name=="default_image_id")
                {
                    if (!LLUUID::validate(std::string(text))) return false;
                    (name=="image_id" ? texture.initialAsset : texture.defaultAsset).set(std::string(text)); return true;
                }
                if (name=="default_image_name") return text=="Default";
                if (name=="fallback_image") { declaration.texture->fallbackImage=std::string(text); return true; }
            }
            if (declaration.colorSwatch)
            {
                auto& swatch=declaration.colorSwatch->swatch;
                if (name=="color")
                {
                    if (color(text,swatch.color)) return true;
                    if (!text.empty() && text.find_first_of(" \t\r\n")==std::string_view::npos &&
                        ((text.front()>='A' && text.front()<='Z') || (text.front()>='a' && text.front()<='z')))
                    {
                        swatch.color=LLVKColor{1,0,1,1};
                        return true;
                    }
                    error="Invalid native swatch color: "+std::string(text);
                    return false;
                }
                if (name=="border_color") return color(text,swatch.borderColor);
                if (name=="border.border_thickness") return integer(text,swatch.border.thickness);
                if (name=="text_enabled_color") return color(text,swatch.enabledText);
                if (name=="text_disabled_color") return color(text,swatch.disabledText);
                if (name=="label") { swatch.label=text; return true; }
                if (name=="label_width") return integer(text,swatch.labelWidth);
                if (name=="label_height") return integer(text,swatch.labelHeight);
                if (name=="can_apply_immediately") return boolean(text,swatch.applyImmediately);
                if (name=="alpha_background_image") { declaration.colorSwatch->alphaImage=text; return true; }
            }
            if (declaration.searchEditor)
            {
                auto& search=*declaration.searchEditor;
                if (name=="search_button_visible") return boolean(text,search.search.searchVisible);
                if (name=="clear_button_visible") return boolean(text,search.search.clearVisible);
                if (name=="highlight_text_field") return boolean(text,search.search.highlight);
                if (name=="background_image_highlight") { search.highlightImage=text; return true; }
            }
            if (declaration.tag=="menu_button")
            {
                if (name=="menu_filename") { declaration.menuFilename=text; return true; }
                if (name=="menu_position" || name=="position")
                {
                    if (text!="bottomleft" && text!="bottomright" && text!="topleft" && text!="topright") return false;
                    declaration.menuPosition=text; return true;
                }
            }
            if (declaration.floater)
            {
                auto& floater=*declaration.floater;
                if (name=="title") { floater.title=text; return true; }
                if (name=="positioning") { floater.positioning=text; return text=="centered" || text=="cascading"; }
                if (name=="save_visibility") return boolean(text,floater.saveVisibility);
                if (name=="rel_x" || name=="rel_y")
                {
                    float value=0;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    if (parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(value) || value < -1.f || value > 1.f) return false;
                    (name=="rel_x" ? floater.relativeX : floater.relativeY)=value;
                    return true;
                }
                if (name=="reuse_instance") { bool enabled; return boolean(text,enabled) && enabled; }
                if (name=="legacy_header_height") return integer(text,floater.legacyHeaderHeight);
                if (name=="header_height") return integer(text,floater.headerHeight) && floater.headerHeight>=0;
                if (name=="drop_shadow") return boolean(text,floater.dropShadow);
                if (name=="save_rect") return boolean(text,floater.saveRect);
                if (name=="single_instance") return boolean(text,floater.singleInstance);
                if (name=="can_close") return boolean(text,floater.canClose);
                if (name=="can_minimize") return boolean(text,floater.canMinimize);
                if (name=="can_dock") return boolean(text,floater.canDock);
                if (name=="can_drag_on_left") { bool enabled; return boolean(text,enabled) && !enabled; }
                if (name=="can_resize") return boolean(text,floater.canResize);
                if (name=="min_width") return integer(text,floater.minWidth) && floater.minWidth>=0;
                if (name=="min_height") return integer(text,floater.minHeight) && floater.minHeight>=0;
            }
            if (declaration.plainLabel && name=="line_spacing.pixels")
                return integer(text,declaration.plainLabel->layout.spacingPixels);
                        if (declaration.textEditor)
                        {
                            auto& editor=*declaration.textEditor;
                            if (name=="prevalidator" || name=="prevalidate_callback" || name=="text_type")
                            { declaration.plainLabel->prevalidator=builtinTextValidator(text); return bool(declaration.plainLabel->prevalidator); }
                            if (name=="border_visible") return boolean(text,editor.borderVisible);
                            if (name=="bg_visible") return boolean(text,declaration.plainLabel->backgroundVisible);
                            if (name=="bg_readonly_color") return color(text,declaration.plainLabel->readOnlyBackground);
                            if (name=="bg_focus_color" || name=="bg_highlighted_color") return color(text,editor.focusBackground);
                            if (name=="cursor_color") return color(text,declaration.plainLabel->cursorColor);
                            if (name=="default_color") return color(text,declaration.plainLabel->textColor);
                            if (name=="text_selected_color") return color(text,declaration.plainLabel->selectionColor);
                            if (name=="bg_selected_color") return color(text,declaration.plainLabel->selectionBackground);
                            if (name=="show_context_menu") return boolean(text,editor.contextMenu);
                            if (name=="commit_on_focus_lost") return boolean(text,declaration.plainLabel->commitOnFocusLost);
                            if (name=="read_only") { bool value; if (!boolean(text,value)) return false; declaration.plainLabel->readOnly=value; return true; }
                            if (name=="allow_scroll" || name=="ignore_tab") { bool value; return boolean(text,value) && value; }
                            if (name=="embedded_items" || name=="track_bottom") { bool value; return boolean(text,value) && !value; }
                        }
            if (declaration.sliderControl)
            {
                auto& slider=declaration.sliderControl->slider;
                if (name=="show_text") return boolean(text,slider.showText);
                if (name=="can_edit_text") return boolean(text,slider.editable);
                if (name=="decimal_digits") return integer(text,slider.precision);
                if (name=="label") { slider.label=text; return true; }
                if (name=="label_width" || name=="text_width")
                { std::int32_t value; if (!integer(text,value) || value<0) return false; (name=="label_width" ? slider.labelWidth : slider.textWidth)=value; return true; }
                if (name=="text_color") return color(text,slider.textColor);
                if (name=="slider_label.halign")
                {
                    if (text=="left") slider.labelAlignment=LLVKFont::HorizontalAlign::Left;
                    else if (text=="right") slider.labelAlignment=LLVKFont::HorizontalAlign::Right;
                    else if (text=="center") slider.labelAlignment=LLVKFont::HorizontalAlign::Center;
                    else return false;
                    return true;
                }
                if (name=="text_disabled_color") return color(text,slider.disabledColor);
                if (name=="volume") { bool unused; return boolean(text,unused); }
            }
            if (declaration.slider)
            {
                auto& slider=declaration.slider->slider;
                if (name=="min_val" || name=="max_val" || name=="increment" || name=="initial_value")
                {
                    auto& value=name=="min_val" ? slider.minimum : name=="max_val" ? slider.maximum : name=="increment" ? slider.increment : slider.initial;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    if (parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(value)) return false;
                    if (name=="initial_value") declaration.control->initialValue=LLSD(value);
                    return true;
                }
                if (name=="orientation") { if (text!="horizontal" && text!="vertical") return false; slider.vertical=text=="vertical"; return true; }
                if (name=="thumb_outline_color") return color(text,slider.outlineColor);
                if (name=="thumb_center_color") return color(text,slider.centerColor);
                if (name=="thumb_image" || name=="thumb_image_pressed" || name=="thumb_image_disabled" ||
                    name=="track_image_horizontal" || name=="track_image_vertical" || name=="track_highlight_horizontal_image" || name=="track_highlight_vertical_image")
                { declaration.slider->images[std::string(name)]=text; return true; }
            }
            if (declaration.radioGroup && name=="allow_deselect") return boolean(text,declaration.allowDeselect);
            if (declaration.radioItem && (name=="value" || name=="initial_value"))
            { declaration.radioPayload=LLSD(std::string(text)); return true; }
            if (declaration.radioGroup && name=="initial_value")
            { declaration.control->initialValue=LLSD(std::string(text)); return true; }
            if (declaration.spinner)
            {
                auto& spinner=declaration.spinner->spinner;
                if (name=="initial_value")
                {
                    float value=0.f;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    if (parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(value)) return false;
                    declaration.control->initialValue=LLSD(value);
                    return true;
                }
                if (name=="min_val" || name=="max_val" || name=="increment")
                {
                    auto& value=name=="min_val" ? spinner.minimum : name=="max_val" ? spinner.maximum : spinner.increment;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    return parsed.ec==std::errc() && parsed.ptr==text.data()+text.size() && std::isfinite(value);
                }
                if (name=="decimal_digits") return integer(text,spinner.precision);
                if (name=="label_width") return integer(text,spinner.labelWidth);
                if (name=="label") { spinner.label=text; return true; }
                if (name=="label_wrap") return boolean(text,spinner.labelWrap);
                if (name=="dynamic_button_height") return boolean(text,spinner.dynamicHeight);
                if (name=="allow_digits_only") return boolean(text,spinner.digitsOnly);
                if (name=="allow_text_entry") { bool unused; return boolean(text,unused); }
                if (name=="text_enabled_color") return color(text,spinner.textEnabledColor);
                if (name=="text_disabled_color") return color(text,spinner.textDisabledColor);
            }
            if (declaration.tabs)
            {
                auto& tabs = *declaration.tabs;
                if (name == "tab_position")
                {
                    using Position = LLVKWidgetTree::Node::TabContainer::Layout::Position;
                    if (text != "top" && text != "bottom" && text != "left") return false;
                    tabs.layout.position = text == "top" ? Position::Top : text == "bottom" ? Position::Bottom : Position::Left;
                    return true;
                }
                if (name == "tab_width")
                { std::int32_t width; if (!integer(text,width) || width < 0) return false; tabs.width = width; return true; }
                if (name == "tab_padding_right") return integer(text,tabs.layout.rightPadding);
                if (name == "tab_height") return integer(text,tabs.layout.tabHeight);
                if (name == "tab_min_width") return integer(text,tabs.layout.minimumWidth);
                if (name == "tab_max_width") return integer(text,tabs.layout.maximumWidth);
                if (name == "hide_tabs") return boolean(text,tabs.layout.hidden);
                if (name == "use_tab_offset") return boolean(text,tabs.layout.panelOffset);
                if (name == "label_pad_left") return integer(text,tabs.labelPadLeft);
                if (name == "label_pad_bottom") return integer(text,tabs.labelPadBottom);
                if (name == "label_shadow") return boolean(text,tabs.labelShadow);
                if (name == "tabs_flashing_color") return color(text,tabs.flashColor);
                if (name == "use_custom_icon_ctrl") { bool enabled; return boolean(text,enabled) && !enabled; }
                if (name == "halign")
                {
                    if (text != "left" && text != "center" && text != "right") return false;
                    tabs.alignment = text == "left" ? LLVKButton::Align::Left : text == "right" ? LLVKButton::Align::Right : LLVKButton::Align::Center;
                    return true;
                }
            }
            if (declaration.browser)
            {
                auto& browser = *declaration.browser;
                if (name == "start_url") { browser.startUrl = text; return true; }
                if (name == "initial_mime_type") { browser.mimeType = text; return true; }
                if (name == "error_page_url") { browser.errorUrl = text; return true; }
                if (name == "trusted_content") return boolean(text,browser.trusted);
                if (name == "focus_on_click") return boolean(text,browser.focusOnClick);
                if (name == "border_visible") return boolean(text,browser.borderVisible);
                if (name == "decouple_texture_size") return boolean(text,browser.decoupleSize);
                if (name == "texture_width") return integer(text,browser.textureWidth);
                if (name == "texture_height") return integer(text,browser.textureHeight);
            }
            if (declaration.combo)
            {
                const auto separator = name.find('.');
                if (separator != std::string_view::npos)
                    if (auto* part = comboPart(declaration,name.substr(0,separator))) return attribute(*part,name.substr(separator+1),text);
                auto& combo = declaration.combo->combo;
                if (name == "label") { combo.label = text; return true; }
                if (name == "value" || name == "initial_value") { declaration.control->initialValue=std::string(text); return true; }
                if (name == "allow_text_entry") return boolean(text,combo.allowTextEntry) && (!combo.flyout || !combo.allowTextEntry);
                if (name == "show_text_as_tentative") return boolean(text,combo.tentativeText);
                if (name == "force_disable_fulltext_search") return boolean(text,combo.forceDisableSubstring);
                if (name == "allow_new_values") { bool enabled; return boolean(text,enabled) && !enabled; }
                if (name == "max_chars")
                {
                    std::int32_t count;
                    if (!integer(text,count) || count <= 0) return false;
                    combo.maximumBytes = static_cast<std::size_t>(count);
                    return true;
                }
                if (name == "list_position")
                {
                    if (text != "above" && text != "below") return false;
                    combo.listAbove = text == "above";
                    return true;
                }
            }
            if (declaration.comboList)
            {
                if (name == "bg_writeable_color") return color(text,declaration.comboListBackground);
                if (name == "background_visible") return boolean(text,declaration.comboListBackgroundVisible);
            }
            if (declaration.layoutStack)
            {
                auto& stack = *declaration.layoutStack;
                if (name == "orientation")
                {
                    if (text != "horizontal" && text != "vertical") return false;
                    stack.vertical = text == "vertical";
                    return true;
                }
                if (name == "clip") return boolean(text,stack.clip);
                if (name == "animate") return boolean(text,stack.animate);
                if (name == "border_size" || name == "drag_handle_gap") return integer(text,stack.spacing);
                if (name == "save_sizes" || name == "show_drag_handle") { bool enabled; return boolean(text,enabled) && !enabled; }
                if (name == "open_time_constant" || name == "close_time_constant")
                {
                    auto& time = name == "open_time_constant" ? stack.openTime : stack.closeTime;
                    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),time);
                    return parsed.ec == std::errc() && parsed.ptr == text.data()+text.size() && std::isfinite(time) && time >= 0.f;
                }
            }
            if (declaration.layoutPanel)
            {
                auto& panel = *declaration.layoutPanel;
                if (name == "auto_resize") return boolean(text,panel.autoResize);
                if (name == "user_resize") return boolean(text,panel.userResize);
                if (name == "min_dim" || name == "min_width" || name == "min_height")
                {
                    std::int32_t value;
                    if (!integer(text,value)) return false;
                    panel.minimum = std::max(0,value);
                    if (!declaration.expandedMinimumProvided) panel.expandedMinimum = panel.minimum;
                    return true;
                }
                if (name == "expanded_min_dim")
                {
                    if (!integer(text,panel.expandedMinimum)) return false;
                    if (panel.expandedMinimum < 0) panel.expandedMinimum = panel.minimum;
                    declaration.expandedMinimumProvided = true;
                    return true;
                }
                if (name == "max_dim" || name == "max_width" || name == "max_height")
                {
                    if (!integer(text,panel.maximum)) return false;
                    if (panel.maximum < 0) panel.maximum = INT32_MAX;
                    return true;
                }
            }
            if (declaration.scrollContainer)
            {
                auto& container = declaration.scrollContainer->container;
                if (name == "opaque") return boolean(text,container.opaque);
                if (name == "color") return color(text,container.backgroundColor);
                if (name == "border_visible") return boolean(text,container.borderVisible);
                if (name == "hide_scrollbar") return boolean(text,container.hideScrollbars);
                if (name == "reserve_scroll_corner") return boolean(text,container.reserveCorner);
                if (name == "size")
                {
                    std::int32_t size;
                    if (!integer(text,size) || size < -1) return false;
                    container.size = size == -1 ? std::nullopt : std::optional(size);
                    return true;
                }
                if (name == "max_auto_scroll_zone") return integer(text,container.maxAutoZone);
                if (name == "min_auto_scroll_rate" || name == "max_auto_scroll_rate")
                {
                    auto& rate = name == "min_auto_scroll_rate" ? container.minAutoRate : container.maxAutoRate;
                    const auto parsed = std::from_chars(text.data(),text.data()+text.size(),rate);
                    return parsed.ec == std::errc() && parsed.ptr == text.data()+text.size() && std::isfinite(rate);
                }
                if (name == "ignore_arrow_keys") return boolean(text,container.ignoreArrowKeys);
            }
            if (declaration.scrollbar)
            {
                auto& scrollbar = declaration.scrollbar->scrollbar;
                if (name == "orientation")
                {
                    if (text != "vertical" && text != "horizontal") return false;
                    scrollbar.vertical = text == "vertical";
                    return true;
                }
                if (name == "doc_size") return integer(text,scrollbar.documentSize);
                if (name == "doc_pos") return integer(text,scrollbar.position);
                if (name == "page_size") return integer(text,scrollbar.pageSize);
                if (name == "step_size") return integer(text,scrollbar.stepSize);
                if (name == "thickness")
                {
                    std::int32_t thickness;
                    if (!integer(text,thickness)) return false;
                    scrollbar.thickness = thickness;
                    return true;
                }
                if (name == "track_color") return color(text,scrollbar.trackColor);
                if (name == "thumb_color") return color(text,scrollbar.thumbColor);
                if (name == "bg_color") return color(text,scrollbar.backgroundColor);
                if (name == "bg_visible") return boolean(text,scrollbar.backgroundVisible);
                if (name == "thumb_image_vertical" || name == "thumb_image_horizontal" ||
                    name == "track_image_vertical" || name == "track_image_horizontal")
                { declaration.scrollImages[std::string(name)] = text; return true; }
            }
            if (declaration.checkBox)
            {
                auto& check = declaration.checkBox->construction;
                if (name=="label_text.text_color") return color(text,check.labelText.textColor);
                if (name=="label_text.text_readonly_color") return color(text,check.labelText.readOnlyColor);
                if (name=="label_text.halign")
                {
                    if (text=="left") check.labelText.layout.alignment=LLVKFont::HorizontalAlign::Left;
                    else if (text=="right") check.labelText.layout.alignment=LLVKFont::HorizontalAlign::Right;
                    else if (text=="center") check.labelText.layout.alignment=LLVKFont::HorizontalAlign::Center;
                    else return false;
                    return true;
                }
                if (name=="halign") { LLVKButton::Align ignored; return alignment(text,ignored); }
                if (name == "label") { check.label = text; return true; }
                if (name == "initial_value" || name == "value")
                {
                    if (!boolean(text,check.initialValue)) return false;
                    declaration.control->initialValue = check.initialValue;
                    return true;
                }
                if (name == "word_wrap")
                {
                    if (text == "none") check.wrap = LLVKWidgetTree::CheckBoxWrap::None;
                    else if (text == "up") check.wrap = LLVKWidgetTree::CheckBoxWrap::Up;
                    else if (text == "down") check.wrap = LLVKWidgetTree::CheckBoxWrap::Down;
                    else return false;
                    return true;
                }
                if (name == "radio_style") { bool ignored; return boolean(text,ignored); }
                if (name == "font") check.fontProvided = true;
            }
            if (declaration.progress)
            {
                auto& progress=*declaration.progress;
                if (name=="image_bar") { progress.imageBar=text; return true; }
                if (name=="image_fill") { progress.imageFill=text; return true; }
                if (name=="color_bar") return color(text,progress.fill);
                if (name=="color_bg") return color(text,progress.background);
                if (name.starts_with("color_bar.") || name.starts_with("color_bg."))
                {
                    const auto component=name.substr(name.find('.')+1);
                    const auto index=component=="red" ? 0 : component=="green" ? 1 : component=="blue" ? 2 : component=="alpha" ? 3 : -1;
                    float value=0;
                    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
                    if (index<0 || parsed.ec!=std::errc() || parsed.ptr!=text.data()+text.size() || !std::isfinite(value)) return false;
                    auto& target=name.starts_with("color_bar.") ? progress.fill : progress.background;
                    auto channels=target.get(); channels[index]=value; target=LLVKColor(channels); return true;
                }
            }
            if (declaration.plainLabel)
            {
                auto& label = *declaration.plainLabel;
                if (name=="use_ellipses") return boolean(text,label.useEllipses);
                if (name=="clip_partial") return boolean(text,label.clipPartial);
                if (name == "label") return true;
                if (name == "skip_link_underline") return boolean(text,label.skipLinkUnderline);
                if (name == "bg_readonly_color") return color(text,label.readOnlyBackground);
                if (name == "bg_visible") return boolean(text,label.backgroundVisible);
                if (name == "text_color") return color(text,label.textColor);
                if (name == "text_readonly_color") return color(text,label.readOnlyColor);
                if (name == "text_tentative_color") return color(text,label.tentativeColor);
                if (name == "bg_writeable_color") return color(text,label.backgroundColor);
                if (name == "word_wrap" || name == "wrap") return boolean(text,label.layout.wrap);
                if (name == "parse_urls" || name == "allow_html")
                {
                    if (!boolean(text,label.parseUrls)) return false;
                    label.parseWebLinks=label.parseUrls && bool(label.linkClicked);
                    return true;
                }
                if (name == "h_pad") return integer(text,label.layout.horizontalPadding);
                if (name == "v_pad") return integer(text,label.verticalPadding);
                if (name == "max_length")
                {
                    std::int32_t maximum;
                    if (!integer(text,maximum) || maximum < 0) return false;
                    label.maximumBytes = static_cast<std::size_t>(maximum);
                    return true;
                }
                if (name == "halign")
                {
                    if (text == "left") label.layout.alignment = LLVKFont::HorizontalAlign::Left;
                    else if (text == "center") label.layout.alignment = LLVKFont::HorizontalAlign::Center;
                    else if (text == "right") label.layout.alignment = LLVKFont::HorizontalAlign::Right;
                    else return false;
                    return true;
                }
                if (name == "valign" || name == "text_valign")
                {
                    if (text == "top") label.vertical = LLVKFont::VerticalAlign::Top;
                    else if (text == "center") label.vertical = LLVKFont::VerticalAlign::Center;
                    else if (text == "bottom") label.vertical = LLVKFont::VerticalAlign::Bottom;
                    else return false;
                    return true;
                }
                if (name == "read_only")
                { bool value; if (!boolean(text,value)) return false; label.readOnly = value; return true; }
                if (name == "track_end" || name == "track_bottom") return boolean(text,label.trackEnd);
                if (name == "font_shadow")
                {
                    if (text!="none" && text!="soft") return false;
                    label.softShadow=text=="soft";
                    return true;
                }
                if (name == "allow_scroll" || name == "use_ellipses" || name == "bg_visible" || name == "border_visible" ||
                    name == "parse_markdown" || name == "parse_highlights" || name == "spellcheck")
                { bool enabled; return boolean(text,enabled) && !enabled; }
            }
            if (declaration.lineEditor)
            {
                auto& editor = *declaration.lineEditor;
                if (name == "default_text") { editor.text.defaultText = text; return true; }
                if (name == "label" || name == "watermark_text") { editor.label = text; return true; }
                if (name == "value" || name == "initial_value") { declaration.control->initialValue = std::string(text); return true; }
                if (name == "max_length_bytes" || name == "max_length_chars" || name == "max_length")
                {
                    std::int32_t value;
                    if (!integer(text,value) || value < 0) return false;
                    if (name == "max_length_bytes") { editor.text.maximumBytes = value; editor.text.maximumCharacters = 0; }
                    else { editor.text.maximumCharacters = value; editor.text.maximumBytes = 4096; }
                    return true;
                }
                if (name == "text_pad_left") return integer(text,editor.text.leftPadding);
                if (name == "text_pad_right") return integer(text,editor.text.rightPadding);
                if (name == "is_password") return boolean(text,editor.text.password);
                if (name == "allow_emoji") return boolean(text,editor.text.allowEmoji);
                if (name == "select_on_focus" || name == "select_all_on_focus_received") return boolean(text,editor.text.selectOnFocus);
                if (name == "commit_on_focus_lost") return boolean(text,editor.commitOnFocusLost);
                if (name == "revert_on_esc") return boolean(text,editor.revertOnEscape);
                if (name == "ignore_tab") return boolean(text,editor.ignoreTab);
                if (name == "draw_focus_border") return boolean(text,editor.drawFocusBorder);
                if (name == "bg_image_always_focused") return boolean(text,editor.showFocusedBackground);
                if (name == "show_label_focused") return boolean(text,editor.showFocusedLabel);
                if (name == "use_bg_color") return boolean(text,editor.useBackgroundColor);
                if (name == "bg_visible") { bool ignored; return boolean(text,ignored); }
                if (name == "spellcheck") { bool enabled; return boolean(text,enabled) && !enabled; }
                if (name == "prevalidator" || name == "prevalidate_callback") { editor.prevalidatorName = text; editor.prevalidator = {}; return true; }
                if (name == "input_prevalidator" || name == "prevalidate_input_callback") { editor.inputPrevalidatorName = text; editor.inputPrevalidator = {}; return true; }
                if (name == "cursor_color") return color(text,editor.cursorColor);
                if (name == "bg_color") return color(text,editor.backgroundColor);
                if (name == "text_color") return color(text,editor.textColor);
                if (name == "text_readonly_color") return color(text,editor.readOnlyColor);
                if (name == "text_tentative_color") return color(text,editor.tentativeColor);
                if (name == "highlight_color") return color(text,editor.highlightColor);
                if (name == "preedit_bg_color") return color(text,editor.preeditColor);
                if (name == "background_image" || name == "background_image_disabled" || name == "background_image_focused")
                { declaration.lineImages[std::string(name)] = text; return true; }
                if (name == "border_thickness" || name == "thickness" || name == "bevel_style" || name == "border_style" ||
                    name == "style" || name == "highlight_light_color" || name == "highlight_dark_color" ||
                    name == "shadow_light_color" || name == "shadow_dark_color") declaration.lineBorderProvided = true;
            }
            auto* border = declaration.border ? &*declaration.border : declaration.panel ? &declaration.panel->border :
                           declaration.lineEditor ? &declaration.lineEditor->border : nullptr;
            if (border)
            {
                if (name == "border_thickness" || name == "thickness") return integer(text,border->thickness);
                if (name == "highlight_light_color") return color(text,border->highlightLight);
                if (name == "highlight_dark_color") return color(text,border->highlightDark);
                if (name == "shadow_light_color") return color(text,border->shadowLight);
                if (name == "shadow_dark_color") return color(text,border->shadowDark);
                if (name == "bevel_style")
                {
                    if (text == "in") border->bevel = LLVKBorder::Bevel::In;
                    else if (text == "out") border->bevel = LLVKBorder::Bevel::Out;
                    else if (text == "bright") border->bevel = LLVKBorder::Bevel::Bright;
                    else if (text == "none") border->bevel = LLVKBorder::Bevel::None;
                    else return false;
                    return true;
                }
                if (name == "border_style" || name == "style")
                {
                    if (text == "line") border->style = LLVKBorder::Style::Line;
                    else if (text == "texture") border->style = LLVKBorder::Style::Texture;
                    else return false;
                    return true;
                }
            }
            if (declaration.panel)
            {
                auto& panel = *declaration.panel;
                if (name == "background_visible" || name == "bg_visible") return boolean(text,panel.backgroundVisible);
                if (name == "background_opaque") return boolean(text,panel.backgroundOpaque);
                if (name == "accepts_badge") return boolean(text,panel.acceptsBadge);
                if (name == "border" || name == "border_visible") return boolean(text,panel.hasBorder);
                if (name == "bg_opaque_color") return color(text,panel.opaqueColor);
                if (name == "bg_alpha_color") return color(text,panel.transparentColor);
                if (name == "bg_opaque_image_overlay") return color(text,panel.opaqueImageOverlay);
                if (name == "bg_alpha_image_overlay") return color(text,panel.transparentImageOverlay);
                if (name == "bg_opaque_image") { declaration.panelOpaqueImage = text; return true; }
                if (name == "bg_alpha_image") { declaration.panelTransparentImage = text; return true; }
                if (name == "label" || name == "title") { panel.label = text; return true; }
                if (name == "help_topic") { panel.helpTopic = text; return true; }
                if (name == "filename") { panel.filename = text; return true; }
                if (name == "class") { declaration.panelClass = text; return true; }
                if (name == "min_width" || name == "min_height")
                { std::int32_t dimension; return integer(text,dimension); }
                if (name == "border_thickness" || name == "thickness") return integer(text,panel.border.thickness);
            }
            if ((name == "font" || name == "font.name" || name == "font.size" || name == "font.style" || name == "font.style.") && declaration.control)
            {
                declaration.explicitFont=true;
                auto& request = declaration.control->fontRequest;
                if (!request) request = resources.defaultFontRequest;
                if (name == "font" || name == "font.name") request->name = text;
                else if (name == "font.size") request->size = text;
                else
                {
                    request->style = 0;
                    if (text.find("BOLD") != text.npos) request->style |= 1;
                    if (text.find("ITALIC") != text.npos) request->style |= 2;
                    if (text.find("UNDERLINE") != text.npos) request->style |= 4;
                }
                return true;
            }
            if (declaration.badge)
            {
                auto& badge = *declaration.badge;
                if (name == "label") { badge.label = labelText(text); return true; }
                if (name == "image") { declaration.badgeImage = text; return true; }
                if (name == "border_image") { declaration.badgeBorderImage = text; return true; }
                if (name == "label_offset_horiz") return integer(text,badge.labelOffsetHorizontal);
                if (name == "label_offset_vert") return integer(text,badge.labelOffsetVertical);
                if (name == "location_offset_hcenter" || name == "location_offset_vcenter")
                {
                    std::int32_t offset;
                    if (!integer(text,offset)) return false;
                    (name == "location_offset_hcenter" ? badge.offsetHorizontal : badge.offsetVertical) = offset;
                    return true;
                }
                if (name == "location_percent_hcenter" || name == "location_percent_vcenter")
                {
                    auto& value = name == "location_percent_hcenter" ? badge.percentHorizontal : badge.percentVertical;
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),value);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size();
                }
                if (name == "padding_horiz" || name == "padding_vert")
                {
                    auto& value = name == "padding_horiz" ? badge.paddingHorizontal : badge.paddingVertical;
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),value);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size() && std::isfinite(value);
                }
                if (name == "location")
                {
                    if (text == "center") badge.location = LLVKBadge::Center;
                    else if (text == "left") badge.location = LLVKBadge::Left;
                    else if (text == "right") badge.location = LLVKBadge::Right;
                    else if (text == "top") badge.location = LLVKBadge::Top;
                    else if (text == "bottom") badge.location = LLVKBadge::Bottom;
                    else if (text == "top_left") badge.location = LLVKBadge::TopLeft;
                    else if (text == "top_right") badge.location = LLVKBadge::TopRight;
                    else if (text == "bottom_left") badge.location = LLVKBadge::BottomLeft;
                    else if (text == "bottom_right") badge.location = LLVKBadge::BottomRight;
                    else return false;
                    return true;
                }
                if (name == "image_color" || name == "border_color" || name == "label_color")
                {
                    return color(text,name == "image_color" ? badge.imageColor : name == "border_color" ? badge.borderColor : badge.labelColor);
                }
            }
            if (declaration.button)
            {
                auto& button = *declaration.button;
                if (name == "label_color") return color(text,button.labelColor);
                if (name == "label_color_selected") return color(text,button.selectedLabelColor);
                if (name == "label_color_disabled") return color(text,button.disabledLabelColor);
                if (name == "label_color_disabled_selected") return color(text,button.disabledSelectedLabelColor);
                if (name == "image_color") return color(text,button.imageColor);
                if (name == "image_color_disabled") return color(text,button.disabledImageColor);
                if (name == "flash_color") return color(text,button.flashColor);
                if (name == "flash_alt_color") return color(text,button.alternateFlashColor);
                if (name == "image_overlay_color") return color(text,button.overlayColor);
                if (name == "image_overlay_disabled_color") return color(text,button.disabledOverlayColor);
                if (name == "image_overlay_selected_color") return color(text,button.selectedOverlayColor);
                if (name == "image_overlay_right_delta") return integer(text,button.overlayRightDelta);
                if (name == "hover_glow_amount")
                {
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),button.hoverGlow);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size() && std::isfinite(button.hoverGlow);
                }
                if (name == "label") { button.label = labelText(text); return true; }
                if (name == "label_selected") { button.selectedLabel = labelText(text); return true; }
                if (buttonImage(button.images,name))
                {
                    declaration.buttonImages[std::string(name)] = text;
                    if (name == "image_pressed") button.pressedProvided = true;
                    if (name == "image_pressed_selected") button.pressedSelectedProvided = true;
                    return true;
                }
                if (name == "is_toggle" || name == "toggle") return boolean(text,button.toggle);
                if (name == "auto_resize") return boolean(text,button.autoResize);
                if (name == "commit_on_return") return boolean(text,button.commitOnReturn);
                if (name == "commit_on_capture_lost") return boolean(text,button.commitOnCaptureLost);
                if (name == "handle_right_mouse") return boolean(text,button.handleRightMouse);
                if (name == "display_pressed_state") return boolean(text,button.displayPressed);
                if (name == "scale_image") return boolean(text,button.scaleImage);
                if (name == "label_shadow") return boolean(text,button.labelShadow);
                if (name == "use_ellipses") return boolean(text,button.useEllipses);
                if (name == "use_font_color") return boolean(text,button.useFontColor);
                if (name == "use_draw_context_alpha") return boolean(text,button.useDrawContextAlpha);
                if (name == "draw_focus_border") return boolean(text,button.drawFocusBorder);
                if (name == "hover_hand_cursor") return boolean(text,button.hoverHandCursor);
                if (name == "button_flash_enable") return boolean(text,button.flashEnable);
                if (name == "button_flash_count") return integer(text,button.flashCount);
                if (name == "button_flash_rate")
                {
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),button.flashPeriod);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size() && std::isfinite(button.flashPeriod);
                }
                if (name == "pad_left") return integer(text,button.leftPad);
                if (name == "pad_right") return integer(text,button.rightPad);
                if (name == "pad_bottom") return integer(text,button.bottomPad);
                if (name == "image_top_pad") return integer(text,button.overlayTopPad);
                if (name == "image_bottom_pad") return integer(text,button.overlayBottomPad);
                if (name == "imgoverlay_label_space") return integer(text,button.overlayLabelSpace);
                if (name == "halign") return alignment(text,button.labelAlign);
                if (name == "image_overlay_alignment") return alignment(text,button.overlayAlign);
                if (name == "held_down_delay.seconds")
                {
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),button.heldSeconds);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size() && std::isfinite(button.heldSeconds);
                }
                if (name == "held_down_delay.frames")
                {
                    const auto result = std::from_chars(text.data(),text.data()+text.size(),button.heldFrames);
                    return result.ec == std::errc() && result.ptr == text.data()+text.size();
                }
            }
            if (declaration.control && !declaration.icon)
            {
                auto& control = *declaration.control;
                if (name == "tab_stop") return boolean(text,control.tabStop);
                if (name == "chrome") return boolean(text,control.chrome);
                if (name == "requests_front") return boolean(text,control.requestsFront);
                if (name == "control_name") { control.valueSetting = text; return true; }
                if (name == "enabled_control" || name == "disabled_control")
                {
                    control.enabledSetting = text;
                    control.invertEnabled = name == "disabled_control";
                    return true;
                }
                if (name == "visibility_control" || name == "visiblity_control") { control.visibleSetting = text; return true; }
                if (name == "invisibility_control" || name == "invisiblity_control") { control.invisibleSetting = text; return true; }
                if (name == "length" || name == "type") return true;
                if (name == "value" || name == "initial_value")
                {
                    bool value;
                    if (!boolean(text,value)) return false;
                    control.initialValue = value;
                    return true;
                }
            }
            if (declaration.icon)
            {
                auto& control = *declaration.control;
                auto& icon = *declaration.icon;
                if (name == "image_name") { declaration.imageName = text; return true; }
                if (name == "interactable") return boolean(text,icon.interactable);
                if (name == "use_draw_context_alpha") return boolean(text,icon.useDrawContextAlpha);
                if (name == "min_width") return integer(text,icon.minimumWidth);
                if (name == "min_height") return integer(text,icon.minimumHeight);
                if (name == "scale_image" || name == "length" || name == "type") return true;
                if (name == "tab_stop") return boolean(text,control.tabStop);
                if (name == "chrome") return boolean(text,control.chrome);
                if (name == "requests_front") return boolean(text,control.requestsFront);
                if (name == "control_name") { control.valueSetting = text; return true; }
                if (name == "enabled_control" || name == "disabled_control")
                {
                    control.enabledSetting = text;
                    control.invertEnabled = name == "disabled_control";
                    return true;
                }
                if (name == "visibility_control" || name == "visiblity_control")
                { control.visibleSetting = text; return true; }
                if (name == "invisibility_control" || name == "invisiblity_control")
                { control.invisibleSetting = text; return true; }
                if (name == "value" || name == "initial_value") { control.initialValue = std::string(text); return true; }
                if (name == "color") return color(text,icon.color);
            }
            if (name == "name") view.name = text;
            else if (name == "layout") geometry.layout = text;
            else if (name == "tool_tip") view.tooltip = text;
            else if (name == "enabled") return boolean(text,view.enabled);
            else if (name == "visible") return boolean(text,view.visible);
            else if (name == "mouse_opaque") return boolean(text,view.mouseOpaque);
            else if (name == "use_bounding_rect") return boolean(text,view.useBoundingRect);
            else if (name == "focus_root") return boolean(text,view.focusRoot);
            else if (name == "default_tab_group") return integer(text,view.defaultTabGroup);
            else if (name == "sound_flags")
            {
                std::int32_t value;
                if (!integer(text,value) || value < 0 || value > 3) return false;
                view.soundFlags = static_cast<std::uint8_t>(value);
            }
            else if (name == "follows") view.follows = LLVKWidgetTree::parseFollows(std::string(text));
            else if (name == "tab_group")
            {
                std::int32_t value;
                if (!integer(text,value)) return false;
                view.tabGroup = value;
            }
            else if (name == "xmlns" || name.starts_with("xmlns:") || name == "xsi:schemaLocation" ||
                     name == "xsi:type" || name == "translate") return true;
            else
            {
                LLVKWidgetLayout::Value* field = nullptr;
                if (name == "left") field = &geometry.left;
                else if (name == "right") field = &geometry.right;
                else if (name == "bottom") field = &geometry.bottom;
                else if (name == "top") field = &geometry.top;
                else if (name == "width") field = &geometry.width;
                else if (name == "height") field = &geometry.height;
                else if (name == "bottom_delta") field = &geometry.bottomDelta;
                else if (name == "top_pad") field = &geometry.topPad;
                else if (name == "top_delta") field = &geometry.topDelta;
                else if (name == "left_pad") field = &geometry.leftPad;
                else if (name == "left_delta") field = &geometry.leftDelta;
                if (!field) return false;
                std::int32_t value;
                if (!integer(text,value)) return false;
                *field = {value,true};
            }
            return true;
        }

        bool callback(Declaration& declaration, std::string_view tag, const XML_Char** attributes)
        {
            const auto separator = tag.find('.');
            if (separator==std::string_view::npos && declaration.control && tag.ends_with("_callback"))
                return callback(declaration,declaration.tag+"."+std::string(tag),attributes);
            if (separator == std::string_view::npos || !declaration.control) return false;
            const auto prefix = tag.substr(0,separator);
            if (declaration.radioGroup && prefix!="radio_group") return false;
            if (declaration.slider && prefix!=(declaration.sliderControl ? "slider" : "slider_bar")) return false;
            if (declaration.spinner && prefix != "spinner") return false;
            if (declaration.scrollList && prefix!="scroll_list") return false;
            if (declaration.colorSwatch && prefix!="color_swatch") return false;
            if ((declaration.button && prefix != "button") || (declaration.icon && prefix != "icon") ||
                (declaration.badge && prefix != "badge") || (declaration.panel && prefix != (declaration.tabs ? "tab_container" : declaration.browser ? "web_browser" : declaration.layoutPanel ? "layout_panel" : "panel")) ||
                (declaration.lineEditor && prefix != (declaration.searchEditor ? declaration.tag : "line_editor")) || (declaration.checkBox && prefix != (declaration.radioItem ? "radio_item" : "check_box")) ||
                (declaration.scrollbar && prefix != "scroll_bar") ||
                (declaration.scrollContainer && prefix != "scroll_container") ||
                (declaration.combo && prefix != "combo_box" && !(declaration.combo->combo.flyout && prefix=="flyout_button"))) return false;
            const auto name = tag.substr(separator+1);
            const auto remember = [&]
            {
                if (!declaration.panel) return;
                Declaration::CallbackDeclaration stored;
                stored.tag = tag;
                for (std::size_t index = 0; attributes[index]; index += 2)
                    stored.attributes.emplace_back(attributes[index],attributes[index+1]);
                declaration.panelCallbacks.push_back(std::move(stored));
            };
            std::string function;
            std::optional<LLSD> parameter;
            for (std::size_t index = 0; attributes[index]; index += 2)
            {
                const std::string_view attribute = attributes[index];
                if (attribute == "function") function = attributes[index+1];
                else if (attribute == "parameter" || attribute == "userdata") parameter = LLSD(attributes[index+1]);
                else return false;
            }
            LLVKControl::Callback* action = nullptr;
            LLVKControl::Validation* predicate = nullptr;
            auto& control = *declaration.control;
            if (name == "commit_callback") action = &control.commit;
            else if (declaration.checkBox && name=="check_button.commit_callback") action=&declaration.checkBox->construction.buttonControl.commit;
            else if (declaration.sliderControl && name=="editor_commit_callback") action=&declaration.sliderControl->slider.editorCommit;
            else if (declaration.slider && name=="mouse_down_callback") action=&declaration.slider->slider.mouseDown;
            else if (declaration.slider && name=="mouse_up_callback") action=&declaration.slider->slider.mouseUp;
            else if (name == "init_callback") action = &control.init;
            else if (name == "mouseenter_callback") action = &control.mouseEnter;
            else if (name == "mouseleave_callback") action = &control.mouseLeave;
            else if (name == "validate_callback") predicate = &control.validate;
            else if (declaration.panel && name == "visible_callback") action = &declaration.panel->visible;
            else if (declaration.lineEditor && name == "keystroke_callback") action = &declaration.lineEditor->keystroke;
            else if (declaration.checkBox && name == "on_check") predicate = &declaration.checkBox->construction.onCheck;
            else if (declaration.scrollbar && name == "change_callback") action = &declaration.scrollbar->changed;
            else if (declaration.scrollContainer && name == "scroll_callback") action = &declaration.scrollContainer->scrolled;
            else if (declaration.combo && name == "prearrange_callback") action = &declaration.combo->combo.prearrange;
            else if (declaration.combo && name == "text_entry_callback") action = &declaration.combo->combo.textEntry;
            else if (declaration.combo && name == "text_changed_callback") action = &declaration.combo->combo.textChanged;
            else if (declaration.colorSwatch)
            {
                if (name=="select_callback") action=&declaration.colorSwatch->swatch.selected;
                else if (name=="cancel_callback") action=&declaration.colorSwatch->swatch.cancelled;
            }
            else if (declaration.button)
            {
                auto& button = *declaration.button;
                if (name == "click_callback") { button.click.emplace(); action = &*button.click; }
                else if (name == "mouse_down_callback") action = &button.mouseDown;
                else if (name == "mouse_up_callback") action = &button.mouseUp;
                else if (name == "mouse_held_callback") action = &button.held;
                else if (name == "is_toggled_callback") predicate = &button.isToggled;
            }
            if (action)
            {
                *action = {{},parameter,function};
                remember();
                return true;
            }
            if (predicate)
            {
                *predicate = {{},parameter,function};
                remember();
                return true;
            }
            return false;
        }

        static void XMLCALL start(void* pointer, const XML_Char* tag, const XML_Char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&]
            {
                if (state.menuDepth)
                {
                    if (++state.menuDepth>LLVKWidgetTree::maximumDepth) { state.reject("Native menu exceeds depth limit"); }
                    return;
                }
                if (state.panelString) { state.reject("Native panel strings cannot contain nested elements"); return; }
                if (state.callbackElement) { state.reject("Nested native callback declarations are unsupported"); return; }
                if (!state.stack.empty() && state.stack.back()->scrollList &&
                    (std::string_view(tag)=="row" || std::string_view(tag)=="scroll_list.row"))
                {
                    if (state.inlineRow || state.stack.back()->scrollList->list.rows.size()>=10000)
                    { state.reject("Nested or over-budget native list row"); return; }
                    state.inlineRow.emplace();
                    for (std::size_t index=0; attributes[index]; index+=2)
                    {
                        const std::string_view name(attributes[index]),value(attributes[index+1]);
                        if (name=="value") state.inlineRow->value=std::string(value);
                        else if (name=="name") { if (state.inlineRow->value.isUndefined()) state.inlineRow->value=std::string(value); }
                        else if (name=="enabled") { if (!boolean(value,state.inlineRow->enabled)) { state.reject("Invalid native row enabled flag"); return; } }
                        else { state.reject("Unsupported native list row attribute"); return; }
                    }
                    return;
                }
                if (state.inlineRow)
                {
                    if (state.inlineCell || (std::string_view(tag)!="column" && std::string_view(tag)!="scroll_list.column"))
                    { state.reject("Unsupported nested native list cell"); return; }
                    std::map<std::string,std::string> fields;
                    for (std::size_t index=0; attributes[index]; index+=2)
                    {
                        const std::string name=std::string_view(attributes[index])=="name" ? "column" : attributes[index];
                        fields.insert_or_assign(name,attributes[index+1]);
                    }
                    const auto& columns=state.stack.back()->scrollList->list.columns;
                    const auto found=std::find_if(columns.begin(),columns.end(),[&](const auto& column) { return column.name==fields["column"]; });
                    if (found==columns.end()) { state.reject("Native inline cell references unknown column"); return; }
                    const auto index=static_cast<std::size_t>(found-columns.begin());
                    state.inlineRow->cells.resize(columns.size()); state.inlineRow->styles.resize(columns.size());
                    auto& style=state.inlineRow->styles[index];
                    if (fields["type"].empty() || fields["type"]=="text") style.type=LLVKWidgetTree::ListCellStyle::Type::Text;
                    else if (fields["type"]=="checkbox") style.type=LLVKWidgetTree::ListCellStyle::Type::CheckBox;
                    else { state.reject("Unsupported native inline cell type"); return; }
                    if (fields.contains("enabled") && !boolean(fields["enabled"],style.enabled))
                    { state.reject("Invalid native cell enabled flag"); return; }
                    state.inlineRow->cells[index]=fields["value"];
                    style.tooltip=fields["tool_tip"];
                    state.inlineCell=index; state.inlineCellText.clear(); return;
                }
                if (!state.stack.empty() && state.stack.back()->scrollList &&
                    (std::string_view(tag)=="fs_scroll_list.columns" || std::string_view(tag)=="scroll_list.columns" || std::string_view(tag)=="scroll_list.column" ||
                     std::string_view(tag)=="columns" || std::string_view(tag)=="column"))
                {
                    auto& columns=state.stack.back()->scrollList->list.columns;
                    if (columns.size()>=128) { state.reject("Native list column budget exceeded"); return; }
                    LLVKWidgetTree::ListColumn column;
                    for (std::size_t index=0; attributes[index]; index+=2)
                    {
                        const std::string_view name(attributes[index]),value(attributes[index+1]);
                        if (name=="name") column.name=value;
                        else if (name=="label") column.label=value;
                        else if (name=="dynamic_width")
                        {
                            bool dynamic=false;
                            if (!boolean(value,dynamic)) { state.reject("Invalid native dynamic column width"); return; }
                            column.width=dynamic ? -1 : 0; column.relativeWidth=-1.f; column.hidden=false;
                        }
                        else if (name=="width")
                        { if (!integer(value,column.width)) { state.reject("Invalid native list column width"); return; } column.hidden=column.width<0; }
                        else if (name=="relative_width" || name=="relwidth")
                        {
                            const auto result=std::from_chars(value.data(),value.data()+value.size(),column.relativeWidth);
                            if (result.ec!=std::errc() || result.ptr!=value.data()+value.size() || !std::isfinite(column.relativeWidth))
                            { state.reject("Invalid native list relative width"); return; }
                        }
                        else { state.reject("Unsupported native list column parameter: "+std::string(name)); return; }
                    }
                    columns.push_back(std::move(column)); state.callbackElement=true; return;
                }
                if (!state.stack.empty() && state.stack.back()->texture &&
                    (std::string_view(tag)=="caption_text" || std::string_view(tag)=="texture_picker.caption_text" ||
                     std::string_view(tag)=="multiselect_text" || std::string_view(tag)=="texture_picker.multiselect_text"))
                {
                    auto& owner=*state.stack.back();
                    const bool multiple=std::string_view(tag).find("multiselect_text")!=std::string_view::npos;
                    auto& slot=multiple ? owner.textureMultiple : owner.textureCaption;
                    if (slot || state.stack.size()>=LLVKWidgetTree::maximumDepth || ++state.nodes>LLVKWidgetTree::maximumNodes)
                    { state.reject("Duplicate or over-budget native texture text parameters"); return; }
                    const auto& defaults=multiple ? owner.texture->multiple : owner.texture->caption;
                    auto child=std::make_unique<Declaration>();
                    child->tag="text"; child->params=defaults.view; child->control=defaults.control; child->plainLabel=defaults.text;
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (!state.attribute(*child,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native texture text parameter: "+std::string(attributes[index])); return; }
                    auto* current=child.get(); slot=std::move(child); state.stack.push_back(current); return;
                }
                if (!state.stack.empty() && state.stack.back()->texture &&
                    (std::string_view(tag)=="border" || std::string_view(tag)=="texture_picker.border"))
                {
                    auto& border=state.stack.back()->texture->texture.border;
                    for (std::size_t index=0; attributes[index]; index+=2)
                    {
                        const std::string_view name(attributes[index]),value(attributes[index+1]);
                        if (name=="bevel_style")
                        {
                            if (value=="in") border.bevel=LLVKBorder::Bevel::In;
                            else if (value=="out") border.bevel=LLVKBorder::Bevel::Out;
                            else if (value=="none") border.bevel=LLVKBorder::Bevel::None;
                            else { state.reject("Invalid texture border bevel"); return; }
                        }
                        else if (name=="border_thickness")
                        { if (!integer(value,border.thickness)) { state.reject("Invalid texture border thickness"); return; } }
                        else { state.reject("Unsupported texture border parameter"); return; }
                    }
                    state.callbackElement=true; return;
                }
                if (std::string_view(tag)=="color_swatch.caption_text")
                {
                    if (state.stack.empty() || !state.stack.back()->colorSwatch || state.stack.back()->swatchCaption ||
                        state.stack.size()>=LLVKWidgetTree::maximumDepth || ++state.nodes>LLVKWidgetTree::maximumNodes)
                    { state.reject("Invalid native swatch caption parameters"); return; }
                    auto& owner=*state.stack.back();
                    const auto& defaults=owner.colorSwatch->caption;
                    auto caption=std::make_unique<Declaration>();
                    caption->tag="text"; caption->params=defaults.view; caption->control=defaults.control; caption->plainLabel=defaults.text;
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (!state.attribute(*caption,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native swatch caption parameter: "+std::string(attributes[index])); return; }
                    auto* current=caption.get();
                    owner.swatchCaption=std::move(caption);
                    state.stack.push_back(current);
                    return;
                }
                if (!state.stack.empty() && state.stack.back()->searchEditor)
                {
                    std::string_view part(tag);
                    if (part.starts_with("search_editor.")) part.remove_prefix(14);
                    if (part.starts_with("filter_editor.")) part.remove_prefix(14);
                    if (part=="search_button" || part=="clear_button")
                    {
                        auto& owner=*state.stack.back();
                        if (owner.searchButtons.contains(std::string(part)) ||
                            state.stack.size()>=LLVKWidgetTree::maximumDepth || ++state.nodes>LLVKWidgetTree::maximumNodes)
                        { state.reject("Duplicate or over-budget search button parameters"); return; }
                        const auto& defaults=part=="search_button" ? owner.searchEditor->searchButton : owner.searchEditor->clearButton;
                        auto button=std::make_unique<Declaration>();
                        button->tag="button"; button->params=defaults.view; button->control=defaults.control; button->button=defaults.button;
                        for (std::size_t index=0; attributes[index]; index+=2)
                        {
                            std::string_view attribute(attributes[index]);
                            if (attribute.starts_with("rect.")) attribute.remove_prefix(5);
                            if (!state.attribute(*button,attribute,attributes[index+1]))
                            { state.reject("Unsupported search button parameter: "+std::string(attribute)); return; }
                        }
                        auto* current=button.get();
                        owner.searchButtons.emplace(std::string(part),std::move(button));
                        state.stack.push_back(current);
                        return;
                    }
                }
                if (!state.stack.empty() && state.stack.back()->combo)
                {
                    std::string_view name(tag);
                    if (name.starts_with("combo_box.")) name.remove_prefix(10);
                    if (name.starts_with("flyout_button.")) name.remove_prefix(14);
                    if (name == "item" || name == "combo_item")
                    {
                        LLVKWidgetTree::ComboItem item;
                        bool labelProvided = false;
                        for (std::size_t index = 0; attributes[index]; index += 2)
                        {
                            const std::string_view attribute(attributes[index]);
                            if (attribute == "label") { item.label = attributes[index+1]; labelProvided = true; }
                            else if (attribute == "value") item.value = attributes[index+1];
                            else if (attribute == "enabled")
                            { if (!boolean(attributes[index+1],item.enabled)) { state.reject("Invalid native combo item enabled flag"); return; } }
                            else if (attribute != "name") { state.reject("Unsupported native combo item attribute"); return; }
                        }
                        if (!labelProvided) item.label = item.value.asString();
                        if (++state.nodes > LLVKWidgetTree::maximumNodes) { state.reject("Native combo item budget exceeded"); return; }
                        state.stack.back()->combo->combo.items.push_back(std::move(item));
                        state.callbackElement = true;
                        return;
                    }
                    if (auto* part = state.comboPart(*state.stack.back(),name))
                    {
                        if (state.stack.size() >= LLVKWidgetTree::maximumDepth || ++state.nodes > LLVKWidgetTree::maximumNodes)
                        { state.reject("Native combo parameter budget exceeded"); return; }
                        for (std::size_t index = 0; attributes[index]; index += 2)
                            if (!state.attribute(*part,attributes[index],attributes[index+1]))
                            { state.reject("Unsupported native combo parameter: " + std::string(attributes[index])); return; }
                        state.stack.push_back(part);
                        return;
                    }
                }
                if ((std::string_view(tag) == "string" || std::string_view(tag) == "panel.string" || std::string_view(tag) == "floater.string") &&
                    !state.stack.empty() && state.stack.back()->panel)
                {
                    PanelString entry;
                    bool named = false;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                    {
                        const std::string_view name(attributes[index]);
                        if (name == "name") { entry.name = attributes[index+1]; named = true; }
                        else if (name == "value") entry.value = attributes[index+1];
                        else { state.reject("Unsupported native panel string attribute"); return; }
                    }
                    if (!named || ++state.nodes > LLVKWidgetTree::maximumNodes)
                    { state.reject("Native panel string is unnamed or exceeds declaration budget"); return; }
                    state.panelString = std::move(entry);
                    return;
                }
                std::string_view scrollButton(tag);
                if (scrollButton.starts_with("scroll_bar.")) scrollButton.remove_prefix(11);
                if (!state.stack.empty() && state.stack.back()->scrollbar &&
                    (scrollButton == "up_button" || scrollButton == "down_button" || scrollButton == "left_button" || scrollButton == "right_button"))
                {
                    auto& owner = *state.stack.back();
                    if (owner.scrollButtons.contains(std::string(scrollButton)) ||
                        state.stack.size() >= LLVKWidgetTree::maximumDepth || ++state.nodes > LLVKWidgetTree::maximumNodes)
                    { state.reject("Duplicate or over-budget native scrollbar button parameters"); return; }
                    const auto& defaults = owner.scrollbar->buttons.at(std::string(scrollButton));
                    auto declaration = std::make_unique<Declaration>();
                    declaration->tag = "button";
                    declaration->params = defaults.view;
                    declaration->control = defaults.control;
                    declaration->button = defaults.button;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                        if (!state.attribute(*declaration,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native scrollbar button parameter: " + std::string(attributes[index])); return; }
                    auto* current = declaration.get();
                    owner.scrollButtons.emplace(std::string(scrollButton),std::move(declaration));
                    state.stack.push_back(current);
                    return;
                }
                if (std::string_view(tag) == "check_box.label_text" || std::string_view(tag) == "check_box.check_button" ||
                    std::string_view(tag) == "radio_item.label_text" || std::string_view(tag) == "radio_item.check_button")
                {
                    if (state.stack.empty() || !state.stack.back()->checkBox ||
                        state.stack.size() >= LLVKWidgetTree::maximumDepth || ++state.nodes > LLVKWidgetTree::maximumNodes)
                    { state.reject("Invalid native checkbox parameter owner or budget"); return; }
                    auto& owner = *state.stack.back();
                    const bool label = std::string_view(tag) == "check_box.label_text" || std::string_view(tag) == "radio_item.label_text";
                    auto& destination = label ? owner.checkLabel : owner.checkButton;
                    if (destination) { state.reject("Duplicate native checkbox parameter block"); return; }
                    const auto& defaults = *owner.checkBox;
                    auto declaration = std::make_unique<Declaration>();
                    declaration->tag = label ? "text" : "button";
                    declaration->params = label ? defaults.labelView : defaults.buttonView;
                    declaration->control = label ? defaults.construction.labelControl : defaults.construction.buttonControl;
                    if (label) declaration->plainLabel = defaults.construction.labelText;
                    else declaration->button = defaults.construction.button;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                        if (!state.attribute(*declaration,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native checkbox parameter: " + std::string(attributes[index])); return; }
                    auto* current = declaration.get();
                    destination = std::move(declaration);
                    state.stack.push_back(current);
                    return;
                }
                if (std::string_view(tag) == "button.badge")
                {
                    if (state.stack.empty() || !state.stack.back()->button || state.stack.back()->ownedBadge ||
                        state.stack.size() >= LLVKWidgetTree::maximumDepth || ++state.nodes > LLVKWidgetTree::maximumNodes)
                    { state.reject("Invalid native button badge parameter"); return; }
                    auto declaration = std::make_unique<Declaration>();
                    const auto& defaults = state.buttonDefaults.providedBadge.value_or(state.buttonDefaults.badge);
                    declaration->tag = "badge";
                    declaration->params = defaults.view;
                    declaration->control = defaults.control;
                    declaration->badge = defaults.badge;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                        if (!state.attribute(*declaration,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native button badge attribute: " + std::string(attributes[index])); return; }
                    auto* current = declaration.get();
                    state.stack.back()->ownedBadge = std::move(declaration);
                    state.stack.push_back(current);
                    return;
                }
                if (!state.stack.empty() && state.stack.back()->tabs &&
                    (std::string_view(tag) == "first_tab" || std::string_view(tag) == "middle_tab" || std::string_view(tag) == "last_tab"))
                {
                    const auto part = std::string_view(tag) == "first_tab" ? 0 : std::string_view(tag) == "middle_tab" ? 1 : 2;
                    for (std::size_t index = 0; attributes[index]; index += 2)
                    {
                        const std::string name = attributes[index];
                        if (name != "tab_top_image_unselected" && name != "tab_top_image_selected" && name != "tab_top_image_flash" &&
                            name != "tab_bottom_image_unselected" && name != "tab_bottom_image_selected" && name != "tab_bottom_image_flash" &&
                            name != "tab_left_image_unselected" && name != "tab_left_image_selected" && name != "tab_left_image_flash")
                        { state.reject("Unsupported native tab image parameter: "+name); return; }
                        state.stack.back()->tabs->images[part][name] = attributes[index+1];
                    }
                    state.callbackElement = true;
                    return;
                }
                if (!state.stack.empty() && state.stack.back()->sliderControl &&
                    (std::string_view(tag)=="slider.value_editor" || std::string_view(tag)=="slider.value_text" || std::string_view(tag)=="slider.slider_label"))
                {
                    if (++state.nodes>LLVKWidgetTree::maximumNodes || state.stack.size()>=LLVKWidgetTree::maximumDepth)
                    { state.reject("Native slider parameter budget exceeded"); return; }
                    auto part=std::make_unique<Declaration>();
                    const auto& defaults=*state.stack.back()->sliderControl;
                    const bool editor=std::string_view(tag)=="slider.value_editor";
                    const std::string name=editor ? "editor" : std::string_view(tag)=="slider.value_text" ? "text" : "label";
                    if (editor)
                    { part->params=defaults.editor.view; part->control=defaults.editor.control; part->lineEditor=defaults.editor.editor; }
                    else
                    {
                        const auto& text=name=="text" ? defaults.text : defaults.label;
                        part->params=text.view; part->control=text.control; part->plainLabel=text.text;
                    }
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (!state.attribute(*part,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native slider child parameter: "+std::string(attributes[index])); return; }
                    auto* current=part.get();
                    state.stack.back()->sliderParts.insert_or_assign(name,std::move(part));
                    state.stack.push_back(current);
                    return;
                }
                if (!state.stack.empty() && state.stack.back()->spinner &&
                    (std::string_view(tag)=="spinner.up_button" || std::string_view(tag)=="spinner.down_button"))
                {
                    const std::string name=std::string_view(tag)=="spinner.up_button" ? "up" : "down";
                    if (state.stack.size()>=LLVKWidgetTree::maximumDepth || ++state.nodes>LLVKWidgetTree::maximumNodes)
                    { state.reject("Native spinner parameter budget exceeded"); return; }
                    auto part=std::make_unique<Declaration>();
                    const auto& defaults=name=="up" ? state.stack.back()->spinner->up : state.stack.back()->spinner->down;
                    part->tag="button"; part->params=defaults.view; part->control=defaults.control; part->button=defaults.button;
                    for (std::size_t index=0; attributes[index]; index+=2)
                        if (!state.attribute(*part,attributes[index],attributes[index+1]))
                        { state.reject("Unsupported native spinner button attribute"); return; }
                    auto* current=part.get();
                    state.stack.back()->spinnerButtons.insert_or_assign(name,std::move(part));
                    state.stack.push_back(current);
                    return;
                }
                if (std::string_view(tag).find('.') != std::string_view::npos || std::string_view(tag).ends_with("_callback"))
                {
                    if (state.stack.empty() || !state.callback(*state.stack.back(),tag,attributes))
                    { state.reject("Unresolved or unsupported native callback: " + std::string(tag)); return; }
                    state.callbackElement = true;
                    return;
                }
                const bool icon = std::string_view(tag) == "icon";
                const bool containerView = std::string_view(tag)=="container_view" || std::string_view(tag)=="stat_view";
                const bool statBar = std::string_view(tag)=="stat_bar";
                const bool progress = std::string_view(tag)=="progress_bar";
                const bool button = std::string_view(tag) == "button" || std::string_view(tag) == "scroll_column_header" || std::string_view(tag)=="menu_button";
                const bool badge = std::string_view(tag) == "badge";
                const bool tabs = std::string_view(tag) == "tab_container";
                const bool floater = std::string_view(tag) == "floater";
                const bool overlapPanel = std::string_view(tag)=="overlap_panel";
                const bool panel = std::string_view(tag) == "panel" || tabs || floater || overlapPanel;
                const bool border = std::string_view(tag) == "view_border";
                const bool searchEditor = std::string_view(tag) == "search_editor" || std::string_view(tag) == "filter_editor";
                const bool colorSwatch = std::string_view(tag) == "color_swatch";
                const bool texture = std::string_view(tag)=="texture_picker";
                const bool scrollList = std::string_view(tag) == "scroll_list" || std::string_view(tag)=="fs_scroll_list";
                const bool editor = std::string_view(tag) == "line_editor" || std::string_view(tag)=="fs_copytrans_inventory_drop_target" || searchEditor;
                const bool radioGroup = std::string_view(tag) == "radio_group";
                const bool radioItem = std::string_view(tag) == "radio_item" ||
                    (std::string_view(tag) == "item" && !state.stack.empty() && state.stack.back()->radioGroup);
                const bool check = std::string_view(tag) == "check_box" || radioItem;
                const bool scroll = std::string_view(tag) == "scroll_bar";
                const bool container = std::string_view(tag) == "scroll_container";
                const bool layoutStack = std::string_view(tag) == "layout_stack";
                const bool layoutPanel = std::string_view(tag) == "layout_panel";
                const bool flyout = std::string_view(tag) == "flyout_button";
                const bool combo = std::string_view(tag) == "combo_box" || flyout;
                const bool menuBar = std::string_view(tag) == "menu_bar";
                const bool textEditor = std::string_view(tag) == "text_editor" || std::string_view(tag) == "simple_text_editor";
                const bool textWidget = std::string_view(tag) == "text" || std::string_view(tag)=="fs_embedded_item_drop_target" || textEditor;
                const bool browser = std::string_view(tag) == "web_browser";
                const bool spinner = std::string_view(tag) == "spinner";
                const bool sliderControl = std::string_view(tag) == "slider";
                const bool slider = std::string_view(tag) == "slider_bar" || sliderControl;
                if (std::string_view(tag) != "view" && std::string_view(tag)!="locate" && !menuBar && !progress && !statBar && !containerView && !icon && !button && !badge && !panel && !border && !editor && !check && !scroll && !container && !layoutStack && !layoutPanel && !combo && !textWidget && !browser && !spinner && !radioGroup && !slider && !colorSwatch && !scrollList && !texture)
                { state.reject("Native constructor not implemented for tag: " + std::string(tag)); return; }
                if (++state.nodes > LLVKWidgetTree::maximumNodes || state.stack.size() >= LLVKWidgetTree::maximumDepth)
                { state.reject("Native widget declaration exceeds node/depth limits"); return; }
                auto declaration = std::make_unique<Declaration>();
                declaration->tag = tag;
                if (overlapPanel) declaration->overlapPanel.emplace();
                if (menuBar)
                {
                    declaration->control=state.panelDefaults.control;
                    declaration->control->fontRequest=state.resources.defaultFontRequest;
                    state.menuStart=static_cast<std::size_t>(XML_GetCurrentByteIndex(state.parser));
                    state.menuDepth=1;
                }
                if (containerView) declaration->containerView=std::make_shared<LLVKWidgetTree::Node::ContainerView>();
                if (statBar) declaration->statBar=std::make_shared<LLVKWidgetTree::Node::StatBar>();
                if (progress)
                {
                    declaration->progress=state.resources.progress;
                    declaration->control=LLVKControl::Params{};
                    declaration->control->fontRequest=state.resources.defaultFontRequest;
                    declaration->control->tabStop=false;
                    declaration->control->initialValue=LLSD(0.);
                }
                declaration->params = icon ? state.iconDefaults.view : button ? state.buttonDefaults.view :
                                      badge ? state.buttonDefaults.badge.view : panel ? state.panelDefaults.view :
                                      border ? state.panelDefaults.borderView : editor ? state.lineDefaults.view : check ? state.checkDefaults.view :
                                      scroll ? state.scrollDefaults.view : container ? state.containerDefaults.view :
                                      layoutStack ? state.layoutDefaults.view : layoutPanel ? state.panelDefaults.view : combo ? state.comboDefaults.view : textWidget ? state.textDefaults.view : state.defaults;
                if (containerView) declaration->params.view.mouseOpaque=false;
                if (std::string_view(tag)=="locate")
                {
                    declaration->control=LLVKControl::Params{};
                    declaration->control->tabStop=false;
                    declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (statBar) declaration->params.view.follows=LLVKWidgetTree::Left|LLVKWidgetTree::Top;
                if (slider)
                {
                    declaration->slider=std::make_shared<LLVKWidgetFactory::SliderDefaults>(*state.resources.slider);
                    declaration->params=declaration->slider->view;
                    declaration->control=declaration->slider->control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (texture)
                {
                    declaration->texture=std::make_shared<LLVKWidgetFactory::TextureDefaults>(*state.resources.texture);
                    auto& defaults=*declaration->texture;
                    if (!defaults.initialized)
                    {
                        defaults.caption=defaults.multiple=state.textDefaults;
                        defaults.texture.border=state.panelDefaults.panel.border;
                        defaults.initialized=true;
                    }
                    declaration->params=defaults.view; declaration->control=defaults.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (colorSwatch)
                {
                    declaration->colorSwatch=std::make_shared<LLVKWidgetFactory::ColorSwatchDefaults>(*state.resources.colorSwatch);
                    auto& swatch=*declaration->colorSwatch;
                    if (!swatch.initialized)
                    {
                        swatch.caption=state.textDefaults;
                        swatch.swatch.border=state.panelDefaults.panel.border;
                        swatch.initialized=true;
                    }
                    declaration->params=swatch.view;
                    declaration->control=swatch.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (scrollList)
                {
                    declaration->scrollList=std::make_shared<LLVKWidgetFactory::ScrollListDefaults>(*state.resources.scrollList);
                    declaration->scrollList->list.preserveContextSelection=std::string_view(tag)=="fs_scroll_list";
                    auto& list=*declaration->scrollList;
                    if (!list.initialized)
                    {
                        list.scrollbar=state.scrollDefaults;
                        list.list.border=state.panelDefaults.panel.border;
                        list.initialized=true;
                    }
                    declaration->params=list.view; declaration->control=list.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (sliderControl)
                {
                    declaration->sliderControl=std::make_shared<LLVKWidgetFactory::SliderControlDefaults>(*state.resources.sliderControl);
                    if (!declaration->sliderControl->initialized)
                    {
                        declaration->sliderControl->editor=state.lineDefaults;
                        declaration->sliderControl->label=declaration->sliderControl->text=state.textDefaults;
                        declaration->sliderControl->initialized=true;
                    }
                    declaration->params=declaration->sliderControl->view;
                    declaration->control=declaration->sliderControl->control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (spinner)
                {
                    declaration->spinner=std::make_shared<LLVKWidgetFactory::SpinnerDefaults>(*state.resources.spinner);
                    if (!declaration->spinner->initialized)
                    {
                        declaration->spinner->up=declaration->spinner->down=state.buttonDefaults;
                        declaration->spinner->editor=state.lineDefaults;
                        declaration->spinner->initialized=true;
                    }
                    declaration->params=declaration->spinner->view;
                    declaration->control=declaration->spinner->control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (textWidget)
                {
                    const auto& defaults=textEditor ? state.resources.textEditor->text : state.textDefaults;
                    declaration->plainLabel = defaults.text;
                    if (textEditor)
                    {
                        declaration->textEditor=std::make_shared<LLVKWidgetFactory::TextEditorDefaults>(*state.resources.textEditor);
                        declaration->params=defaults.view;
                        declaration->scrollContainer=std::make_shared<LLVKWidgetFactory::ScrollContainerDefaults>(state.containerDefaults);
                    }
                    if (state.resources.webLinkHandler)
                    {
                        declaration->plainLabel->parseWebLinks=declaration->plainLabel->parseUrls;
                        declaration->plainLabel->linkClicked=state.resources.webLinkHandler;
                        if (state.resources.colors)
                        {
                            if (const auto color=state.resources.colors->find("HTMLLinkColor")) declaration->plainLabel->linkColor=*color;
                            if (const auto color=state.resources.colors->find("UriQueryPartColor")) declaration->plainLabel->queryColor=*color;
                        }
                    }
                    declaration->control = defaults.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                }
                    if (browser)
                    {
                        declaration->params = state.browserDefaults.panel.view;
                        declaration->panel = state.browserDefaults.panel.panel;
                        declaration->control = state.browserDefaults.panel.control;
                        declaration->browser = state.browserDefaults.browser;
                        if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                    }
                if (combo)
                {
                    declaration->combo = std::make_shared<LLVKWidgetFactory::ComboDefaults>(flyout && state.resources.flyout ?
                        *state.resources.flyout : state.comboDefaults);
                    declaration->combo->combo.flyout=flyout;
                    if (flyout && !state.resources.flyout) declaration->combo->action=state.buttonDefaults;
                    if (flyout) { declaration->combo->combo.allowTextEntry=false; declaration->params=declaration->combo->view; }
                    if (!declaration->combo->initialized)
                    {
                        declaration->combo->button = state.buttonDefaults;
                        declaration->combo->dropDown = state.buttonDefaults;
                        declaration->combo->editor = state.lineDefaults;
                        declaration->combo->combo.listControl.fontRequest = state.resources.defaultFontRequest;
                        if (state.resources.colors)
                        {
                            auto& params = declaration->combo->combo;
                            for (const auto& [name,color] : {std::pair{"ScrollUnselectedColor",&params.listForeground},
                                std::pair{"ScrollSelectedFGColor",&params.listSelectedForeground},std::pair{"ScrollDisabledColor",&params.listDisabledForeground},
                                std::pair{"ScrollSelectedBGColor",&params.listSelectedBackground},std::pair{"ScrollHoveredColor",&params.listHoverBackground},
                                std::pair{"ScrollBgReadOnlyColor",&params.listReadOnlyBackground}})
                                if (const auto resolved = state.resources.colors->find(name)) *color = *resolved;
                        }
                        declaration->combo->initialized = true;
                    }
                    declaration->control = declaration->combo->control;
                    if (!declaration->control->font && !declaration->control->fontRequest) declaration->control->fontRequest = state.resources.defaultFontRequest;
                }
                if (layoutStack) declaration->layoutStack = state.layoutDefaults.stack;
                if (layoutPanel)
                {
                    declaration->layoutPanel.emplace();
                    declaration->control = state.panelDefaults.control;
                    declaration->panel = state.panelDefaults.panel;
                }
                if (container)
                {
                    declaration->scrollContainer = std::make_shared<LLVKWidgetFactory::ScrollContainerDefaults>(state.containerDefaults);
                    declaration->control = state.containerDefaults.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                }
                if (scroll)
                {
                    declaration->scrollbar = std::make_shared<LLVKWidgetFactory::ScrollbarDefaults>(state.scrollDefaults);
                    declaration->control = state.scrollDefaults.control;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                    for (const std::string name : {"up_button","down_button","left_button","right_button"})
                        if (!declaration->scrollbar->buttons.contains(name)) declaration->scrollbar->buttons.emplace(name,state.buttonDefaults);
                }
                if (check)
                {
                    const auto& defaults=radioItem && state.resources.radioItem ? *state.resources.radioItem : state.checkDefaults;
                    declaration->checkBox = defaults;
                    declaration->control = defaults.control;
                    if (radioItem) declaration->params=defaults.view;
                    declaration->radioItem=radioItem;
                }
                if (radioGroup)
                {
                    declaration->radioGroup=true;
                    declaration->params=state.resources.radioView;
                    declaration->control=state.resources.radioControl;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest=state.resources.defaultFontRequest;
                }
                if (editor)
                {
                    declaration->lineEditor = state.lineDefaults.editor;
                    declaration->control = state.lineDefaults.control;
                    declaration->lineBorderProvided = state.lineDefaults.borderProvided;
                    if (!declaration->lineBorderProvided) declaration->lineEditor->border = state.panelDefaults.panel.border;
                    if (searchEditor)
                    {
                        const bool filter=std::string_view(tag)=="filter_editor";
                        declaration->searchEditor=std::make_shared<LLVKWidgetFactory::SearchEditorDefaults>(*(filter ? state.resources.filterEditor : state.resources.searchEditor));
                        auto& search=*declaration->searchEditor;
                        search.search.commitOnKeystroke=filter;
                        if (!search.initialized)
                        {
                            search.editor=state.lineDefaults;
                            search.searchButton=search.clearButton=state.buttonDefaults;
                            search.initialized=true;
                        }
                        declaration->params=search.editor.view;
                        declaration->control=search.editor.control;
                        if (!declaration->control->font && !declaration->control->fontRequest)
                            declaration->control->fontRequest=state.resources.defaultFontRequest;
                        declaration->lineEditor=search.editor.editor;
                        declaration->lineBorderProvided=search.editor.borderProvided;
                        if (!declaration->lineBorderProvided) declaration->lineEditor->border=state.panelDefaults.panel.border;
                    }
                }
                if (border) declaration->border = state.panelDefaults.panel.border;
                if (panel)
                {
                    declaration->control = state.panelDefaults.control;
                    declaration->panel = state.panelDefaults.panel;
                    declaration->panelConstructor = state.panelDefaults;
                    if (floater) declaration->floater.emplace();
                }
                if (tabs)
                {
                    declaration->tabs = std::make_shared<LLVKWidgetFactory::TabDefaults>(*state.resources.tabs);
                    declaration->params = declaration->tabs->panel.view;
                    declaration->control = declaration->tabs->panel.control;
                    declaration->panel = declaration->tabs->panel.panel;
                    declaration->panelConstructor = state.panelDefaults;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                }
                if (icon)
                {
                    declaration->control = state.iconDefaults.control;
                    declaration->icon = state.iconDefaults.icon;
                    if (!declaration->control->font && !declaration->control->fontRequest)
                        declaration->control->fontRequest = state.resources.defaultFontRequest;
                }
                if (button)
                {
                    declaration->control = state.buttonDefaults.control;
                    declaration->button = state.buttonDefaults.button;
                    declaration->button->defaultImages = state.buttonDefaults.button.images;
                    declaration->badgeConstruction.view = state.buttonDefaults.badge.view.view;
                    declaration->badgeConstruction.control = state.buttonDefaults.badge.control;
                    declaration->badgeConstruction.defaults = state.buttonDefaults.badge.badge;
                    if (state.buttonDefaults.providedBadge)
                    {
                        declaration->badgeConstruction.view = state.buttonDefaults.providedBadge->view.view;
                        declaration->badgeConstruction.control = state.buttonDefaults.providedBadge->control;
                        declaration->badgeConstruction.provided = state.buttonDefaults.providedBadge->badge;
                    }
                }
                if (badge)
                {
                    declaration->control = state.buttonDefaults.badge.control;
                    declaration->badge = state.buttonDefaults.badge.badge;
                }
                declaration->params.view.fromDeclaration = true;
                for (std::size_t index = 0; attributes[index]; index += 2)
                {
                    if (panel) declaration->attributes.emplace_back(attributes[index],attributes[index+1]);
                    if (!state.attribute(*declaration,attributes[index],attributes[index+1]))
                    { state.reject("Unsupported or invalid native view attribute: " + std::string(attributes[index])); return; }
                }
                auto* current = declaration.get();
                if (state.stack.empty()) state.root = std::move(declaration);
                else state.stack.back()->children.push_back(std::move(declaration));
                state.stack.push_back(current);
            });
        }

        static void XMLCALL end(void* pointer, const XML_Char* tag)
        {
            auto& state = *static_cast<Parser*>(pointer);
            if (state.menuDepth)
            {
                if (--state.menuDepth) return;
                state.guarded([&]
                {
                    const auto end=static_cast<std::size_t>(XML_GetCurrentByteIndex(state.parser)+XML_GetCurrentByteCount(state.parser));
                    state.stack.back()->menuXml=state.source.substr(state.menuStart,end-state.menuStart);
                    state.stack.pop_back();
                });
                return;
            }
            if (state.inlineRow)
            {
                state.guarded([&]
                {
                    if (state.inlineCell)
                    {
                        const auto body=panelText(std::move(state.inlineCellText));
                        if (!body.empty()) state.inlineRow->cells[*state.inlineCell]=body;
                        state.inlineCell.reset();
                    }
                    else if (std::string_view(tag)=="row" || std::string_view(tag)=="scroll_list.row")
                    {
                        state.stack.back()->scrollList->list.rows.push_back(std::move(*state.inlineRow));
                        state.inlineRow.reset();
                    }
                });
                return;
            }
            if (state.panelString)
            {
                state.guarded([&]
                {
                    auto entry = std::move(*state.panelString);
                    auto body = panelText(std::move(entry.body));
                    if (!body.empty()) entry.value = std::move(body);
                    if (!entry.value) entry.value=std::string();
                    state.stack.back()->panelStrings.insert_or_assign(entry.name,*entry.value);
                    state.stack.back()->panel->strings.insert_or_assign(std::move(entry.name),std::move(*entry.value));
                    state.panelString.reset();
                });
                return;
            }
            if (state.callbackElement) { state.callbackElement = false; return; }
            if (!state.stack.empty() && (state.stack.back()->plainLabel || state.stack.back()->panel || state.stack.back()->lineEditor))
            {
                state.guarded([&]
                {
                    auto& declaration = *state.stack.back();
                    auto body = panelText(std::move(declaration.textBody));
                    if (!body.empty()) declaration.control->initialValue = std::move(body);
                    if (declaration.plainLabel && !declaration.children.empty()) state.reject("Native literal text does not support embedded child widgets");
                });
            }
            if (!state.stack.empty()) state.stack.pop_back();
        }

        static void XMLCALL text(void* pointer, const XML_Char* text, int length)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&]
            {
                if (state.menuDepth) return;
                if (state.panelString) { state.panelString->body.append(text,length); return; }
                if (state.inlineCell) { state.inlineCellText.append(text,length); return; }
                if (!state.callbackElement && !state.stack.empty() && (state.stack.back()->plainLabel || state.stack.back()->panel || state.stack.back()->lineEditor))
                { state.stack.back()->textBody.append(text,length); return; }
                if (std::string_view(text,length).find_first_not_of(" \t\r\n") != std::string_view::npos)
                    state.reject("Native widget does not accept text content: "+
                        (state.stack.empty() ? std::string("outside root") : state.stack.back()->tag+" "+state.stack.back()->params.view.name));
            });
        }

        static void XMLCALL doctype(void* pointer,const XML_Char*,const XML_Char*,const XML_Char*,int)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&] { state.reject("Native widget declarations do not allow DTDs or external entities"); });
        }
    };

    bool parse(Parser& state, std::string_view xml, std::string& error)
    {
        if (xml.size() > 4 * 1024 * 1024)
        { error = "Native widget declaration exceeds byte budget"; return false; }
        state.parser = XML_ParserCreate(nullptr);
        state.source=xml;
        if (!state.parser) { error = "Native widget parser allocation failed"; return false; }
        XML_SetUserData(state.parser,&state);
        XML_SetElementHandler(state.parser,Parser::start,Parser::end);
        XML_SetCharacterDataHandler(state.parser,Parser::text);
        XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
        XML_SetParamEntityParsing(state.parser,XML_PARAM_ENTITY_PARSING_NEVER);
        const auto result = XML_Parse(state.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE);
        if (state.exception) std::rethrow_exception(state.exception);
        if (result != XML_STATUS_OK || !state.root)
        {
            error = state.error.empty() ? "Invalid native widget XML" : state.error;
            error += " at line " + std::to_string(XML_GetCurrentLineNumber(state.parser));
            return false;
        }
        return true;
    }

    struct BuildState
    {
        std::vector<std::string> files;
        std::vector<const LLVKWidgetFactory::PanelInstance*> panels;
        std::size_t bytes = 0;
        std::size_t depth = 0;
    };

    bool resolveImages(const LLVKWidgetTree& tree, const Declaration& declaration, std::string& error)
    {
        for (const auto* name : {&declaration.imageName,&declaration.badgeImage,&declaration.badgeBorderImage,
                                 &declaration.panelOpaqueImage,&declaration.panelTransparentImage})
            if (*name)
            {
                tree.findImage(**name,error);
                if (!error.empty()) return false;
            }
        for (const auto& [attribute,name] : declaration.buttonImages)
        {
            tree.findImage(name,error);
            if (!error.empty()) return false;
        }
        for (const auto& [attribute,name] : declaration.lineImages)
        {
            tree.findImage(name,error);
            if (!error.empty()) return false;
        }
        if (declaration.checkLabel && !resolveImages(tree,*declaration.checkLabel,error)) return false;
        if (declaration.checkButton && !resolveImages(tree,*declaration.checkButton,error)) return false;
        for (const auto& [attribute,name] : declaration.scrollImages)
        {
            tree.findImage(name,error);
            if (!error.empty()) return false;
        }
        for (const auto& [name,button] : declaration.scrollButtons)
            if (!resolveImages(tree,*button,error)) return false;
        for (const auto& [name,part] : declaration.comboParts)
            if (!resolveImages(tree,*part,error)) return false;
        return !declaration.ownedBadge || resolveImages(tree,*declaration.ownedBadge,error);
    }

    std::optional<std::string> declarationFile(const LLVKWidgetFactory::Resources& resources,
        const std::string& filename, std::size_t& bytes, std::string& error)
    {
        if (resources.skinFiles && !resources.declarationLayers.contains(filename) && !resources.declarations.contains(filename))
        {
            const auto files = resources.skinFiles->read("xui",filename,LLVKSkinFiles::Policy::Current,error);
            if (!files) return std::nullopt;
            std::vector<std::string_view> layers;
            std::size_t additional = 0;
            for (const auto& file : *files)
            {
                if (file.size() > 64*1024*1024-bytes-additional)
                { error = "Native skin construction exceeds total byte budget"; return std::nullopt; }
                additional += file.size();
                layers.push_back(file);
            }
            auto merged = LLVKXmlLayers::merge(layers,error);
            if (merged) bytes += additional;
            return merged;
        }
        std::vector<std::string> paths{filename};
        const auto layered = resources.declarationLayers.find(filename);
        if (layered != resources.declarationLayers.end()) paths = layered->second;
        if (paths.empty() || paths.front().empty())
        { error = "Native layered declaration has no base file: " + filename; return std::nullopt; }
        std::vector<std::string_view> documents;
        std::size_t additional = 0;
        for (std::size_t index = 0; index < paths.size(); ++index)
        {
            const auto& path = paths[index];
            if (index && (path.empty() || path == paths.front())) continue;
            const auto file = resources.declarations.find(path);
            if (file == resources.declarations.end())
            { error = "Native panel declaration is unavailable: " + path; return std::nullopt; }
            constexpr std::size_t maximumBytes = 64 * 1024 * 1024;
            if (file->second.size() > maximumBytes-bytes-additional)
            { error = "Native declaration files exceed total byte budget"; return std::nullopt; }
            additional += file->second.size();
            documents.push_back(file->second);
        }
        auto merged = LLVKXmlLayers::merge(documents,error);
        if (!merged) { error = filename + ": " + error; return std::nullopt; }
        bytes += additional;
        return merged;
    }

    LLVKWidgetFactory::Callbacks activeCallbacks(const LLVKWidgetFactory::Callbacks& defaults, const BuildState& state)
    {
        auto callbacks = defaults;
        for (const auto* panel : state.panels)
        {
            if (!panel->callbacks) continue;
            for (const auto& [name,function] : panel->callbacks->actions) callbacks.actions.insert_or_assign(name,function);
            for (const auto& [name,function] : panel->callbacks->predicates) callbacks.predicates.insert_or_assign(name,function);
            for (const auto& [name,function] : panel->callbacks->textValidators) callbacks.textValidators.insert_or_assign(name,function);
        }
        return callbacks;
    }

    template<class Callback, class Registry>
    bool resolveCallback(Callback& callback, const Registry& registry, std::string& error)
    {
        if (callback.function || !callback.functionName) return true;
        const auto found = registry.find(*callback.functionName);
        if (found == registry.end() || !found->second)
        { error = "Unresolved native callback: " + *callback.functionName; return false; }
        callback.function = found->second;
        return true;
    }

    bool resolveFont(LLVKControl::Params& control, const LLVKWidgetFactory::Resources& resources, std::string& error)
    {
        if (!control.fontRequest) return true;
        const auto named = resources.fonts.find(control.fontRequest->name);
        if (named != resources.fonts.end() && named->second) { control.font = named->second; return true; }
        std::shared_ptr<LLVKFont> font;
        if (resources.fontRegistry) font = resources.fontRegistry->resolve(*control.fontRequest,error);
        if (!font) font = resources.fallbackFont;
        if (!font)
        {
            if (error.empty()) error = "Unresolved native font: " + control.fontRequest->name;
            return false;
        }
        control.font = std::move(font);
        error.clear();
        return true;
    }

    bool resolveControl(LLVKControl::Params& control, const LLVKWidgetFactory::Callbacks& callbacks,
                         const LLVKWidgetFactory::Resources& resources, std::string& error)
    {
        if (!resolveFont(control,resources,error)) return false;
        for (auto* callback : {&control.init,&control.commit,&control.mouseEnter,&control.mouseLeave})
            if (!resolveCallback(*callback,callbacks.actions,error)) return false;
        return resolveCallback(control.validate,callbacks.predicates,error);
    }

    bool resolveButton(LLVKButton::Params& button, const LLVKWidgetFactory::Callbacks& callbacks, std::string& error)
    {
        for (auto* callback : {&button.mouseDown,&button.mouseUp,&button.held})
            if (!resolveCallback(*callback,callbacks.actions,error)) return false;
        return (!button.click || resolveCallback(*button.click,callbacks.actions,error)) &&
               resolveCallback(button.isToggled,callbacks.predicates,error);
    }

    bool resolveCheckBox(const Declaration& declaration, LLVKWidgetFactory::CheckBoxDefaults& defaults,
        const LLVKWidgetTree& tree, const LLVKWidgetFactory::Resources& resources, std::string& error)
    {
        if (declaration.checkLabel)
        {
            const auto& label = *declaration.checkLabel;
            if (!label.children.empty()) { error = "Checkbox label parameters cannot contain widgets"; return false; }
            defaults.labelView = label.params;
            defaults.construction.labelControl = *label.control;
            defaults.construction.labelText = *label.plainLabel;
        }
        if (declaration.checkButton)
        {
            const auto& button = *declaration.checkButton;
            if (!button.children.empty() || button.ownedBadge) { error = "Nested checkbox button construction is not implemented"; return false; }
            defaults.buttonView = button.params;
            defaults.construction.buttonControl = *button.control;
            defaults.construction.button = *button.button;
            for (const auto& [attribute,name] : button.buttonImages)
                *buttonImage(defaults.construction.button.images,attribute) = tree.findImage(name);
        }
        return resolveFont(defaults.construction.labelControl,resources,error) &&
            resolveFont(defaults.construction.buttonControl,resources,error);
    }

    bool resolveTexture(const Declaration& declaration,LLVKWidgetFactory::TextureDefaults& defaults,const LLVKWidgetTree& tree,std::string& error)
    {
        defaults.view=declaration.params; defaults.control=*declaration.control;
        for (const auto& pair : {std::pair{declaration.textureCaption.get(),&defaults.caption},std::pair{declaration.textureMultiple.get(),&defaults.multiple}})
            if (pair.first)
            { pair.second->view=pair.first->params; pair.second->control=*pair.first->control; pair.second->text=*pair.first->plainLabel; }
        defaults.texture.captionControl=defaults.caption.control; defaults.texture.caption=defaults.caption.text;
        defaults.texture.multipleControl=defaults.multiple.control; defaults.texture.multiple=defaults.multiple.text;
        if (defaults.fallbackImage)
        {
            defaults.texture.fallback=tree.findImage(*defaults.fallbackImage,error);
            if (!defaults.texture.fallback) return false;
        }
        return true;
    }

    bool resolveColorSwatch(const Declaration& declaration,LLVKWidgetFactory::ColorSwatchDefaults& defaults,
        const LLVKWidgetTree& tree,std::string& error)
    {
        defaults.view=declaration.params;
        defaults.control=*declaration.control;
        if (declaration.swatchCaption)
        {
            defaults.caption.view=declaration.swatchCaption->params;
            defaults.caption.control=*declaration.swatchCaption->control;
            defaults.caption.text=*declaration.swatchCaption->plainLabel;
        }
        defaults.swatch.caption=defaults.caption.text;
        defaults.swatch.captionControl=defaults.caption.control;
        if (defaults.alphaImage)
        {
            defaults.swatch.alphaBackground=tree.findImage(*defaults.alphaImage,error);
            if (!error.empty()) return false;
        }
        return true;
    }

    bool resolveSearchEditor(const Declaration& declaration, LLVKWidgetFactory::SearchEditorDefaults& defaults,
        const LLVKWidgetTree& tree, std::string& error)
    {
        defaults.editor.view=declaration.params;
        defaults.editor.control=*declaration.control;
        defaults.editor.editor=*declaration.lineEditor;
        defaults.editor.borderProvided=declaration.lineBorderProvided;
        for (const auto& [attribute,name] : declaration.lineImages)
        {
            auto& image=attribute=="background_image" ? defaults.editor.editor.background :
                attribute=="background_image_disabled" ? defaults.editor.editor.disabledBackground : defaults.editor.editor.focusedBackground;
            image=tree.findImage(name,error);
            if (!error.empty()) return false;
        }
        if (defaults.highlightImage)
        {
            defaults.search.highlightBackground=tree.findImage(*defaults.highlightImage,error);
            if (!error.empty()) return false;
        }
        for (const auto& [name,part] : declaration.searchButtons)
        {
            auto& button=name=="search_button" ? defaults.searchButton : defaults.clearButton;
            button.view=part->params; button.control=*part->control; button.button=*part->button;
            for (const auto& [attribute,image] : part->buttonImages)
            {
                *buttonImage(button.button.images,attribute)=tree.findImage(image,error);
                if (!error.empty()) return false;
            }
        }
        auto& search=defaults.search;
        const auto dimension=[](const LLVKWidgetLayout::Value& field,std::int32_t fallback)
        { return field.provided ? static_cast<std::int32_t>(field.value) : fallback; };
        const auto& searchGeometry=defaults.searchButton.view.geometry;
        const auto& clearGeometry=defaults.clearButton.view.geometry;
        search.searchWidth=dimension(searchGeometry.width,search.searchWidth);
        search.searchHeight=dimension(searchGeometry.height,search.searchHeight);
        search.searchLeft=dimension(searchGeometry.leftPad,search.searchLeft);
        search.searchBottom=dimension(searchGeometry.topPad,search.searchBottom);
        search.clearWidth=dimension(clearGeometry.width,search.clearWidth);
        search.clearHeight=dimension(clearGeometry.height,search.clearHeight);
        search.clearBottom=dimension(clearGeometry.bottom,search.clearBottom);
        search.clearRight=defaults.clearButton.button.rightPad;
        search.clearLeft=defaults.clearButton.button.leftPad;
        search.searchButton=defaults.searchButton.button;
        search.clearButton=defaults.clearButton.button;
        search.buttonControl=defaults.searchButton.control;
        search.editor=defaults.editor.editor;
        return true;
    }

    bool resolveScrollbar(const Declaration& declaration, LLVKWidgetFactory::ScrollbarDefaults& defaults,
        const LLVKWidgetTree& tree, const LLVKWidgetFactory::Resources& resources, std::string& error)
    {
        for (const auto& [name,button] : declaration.scrollButtons)
        {
            if (!button->children.empty() || button->ownedBadge)
            { error = "Nested scrollbar button widgets or badges are not implemented"; return false; }
            auto& destination = defaults.buttons.at(name);
            destination.view = button->params;
            destination.control = *button->control;
            destination.button = *button->button;
            for (const auto& [attribute,image] : button->buttonImages)
                *buttonImage(destination.button.images,attribute) = tree.findImage(image);
            destination.button.defaultImages = destination.button.images;
        }
        for (auto& [name,button] : defaults.buttons)
        {
            if (!button.control.font && !button.control.fontRequest) button.control.fontRequest = resources.defaultFontRequest;
            if (!resolveFont(button.control,resources,error)) return false;
        }
        for (const auto& [attribute,name] : declaration.scrollImages)
        {
            auto& params = defaults.scrollbar;
            auto& image = attribute == "thumb_image_vertical" ? params.thumbVertical :
                attribute == "thumb_image_horizontal" ? params.thumbHorizontal :
                attribute == "track_image_vertical" ? params.trackVertical : params.trackHorizontal;
            image = tree.findImage(name);
        }
        return true;
    }

    bool resolveScrollList(const Declaration& declaration,LLVKWidgetFactory::ScrollListDefaults& defaults,
        const LLVKWidgetTree& tree,const LLVKWidgetFactory::Resources& resources,std::string& error)
    {
        defaults.view=declaration.params; defaults.control=*declaration.control;
        const auto bar=std::make_unique<Declaration>();
        if (!resolveScrollbar(*bar,defaults.scrollbar,tree,resources,error)) return false;
        const auto background=defaults.list.scrollbar.backgroundColor;
        const auto backgroundVisible=defaults.list.scrollbar.backgroundVisible;
        defaults.list.scrollbar=defaults.scrollbar.scrollbar;
        defaults.list.scrollbar.backgroundColor=background; defaults.list.scrollbar.backgroundVisible=backgroundVisible;
        defaults.list.scrollbar.vertical=true;
        defaults.list.scrollbar.decreaseControl=defaults.scrollbar.buttons.at("up_button").control;
        defaults.list.scrollbar.increaseControl=defaults.scrollbar.buttons.at("down_button").control;
        defaults.list.scrollbar.decreaseButton=defaults.scrollbar.buttons.at("up_button").button;
        defaults.list.scrollbar.increaseButton=defaults.scrollbar.buttons.at("down_button").button;
        defaults.list.scrollbarControl=defaults.control;
        defaults.list.scrollbarControl.init={}; defaults.list.scrollbarControl.commit={};
        defaults.list.scrollbarControl.valueSetting.reset();
        defaults.list.scrollbarSize=tree.setting("UIScrollbarSize").value_or(LLSD(16)).asInteger();
        if (resources.scrollColumnHeader)
        {
            auto header=std::make_shared<LLVKWidgetTree::ListHeaderParams>();
            header->control=resources.scrollColumnHeader->control;
            header->button=resources.scrollColumnHeader->button;
            if (!resolveFont(header->control,resources,error)) return false;
            header->ascendingImage=tree.findImage("up_arrow.tga",error);
            if (!header->ascendingImage) return false;
            header->descendingImage=tree.findImage("down_arrow.tga",error);
            if (!header->descendingImage) return false;
            defaults.list.header=std::move(header);
        }
        return true;
    }

    bool resolveCombo(const Declaration& declaration, LLVKWidgetFactory::ComboDefaults& defaults,
        const LLVKWidgetTree& tree, const LLVKWidgetFactory::Resources& resources, std::string& error)
    {
        for (const auto& [name,part] : declaration.comboParts)
        {
            if (!part->children.empty() || part->ownedBadge)
            { error = "Native combo parameter blocks cannot construct nested widgets"; return false; }
            if (name == "combo_editor")
            {
                defaults.editor.control = *part->control;
                defaults.editor.editor = *part->lineEditor;
                for (const auto& [attribute,imageName] : part->lineImages)
                {
                    auto& image = attribute == "background_image" ? defaults.editor.editor.background :
                        attribute == "background_image_disabled" ? defaults.editor.editor.disabledBackground : defaults.editor.editor.focusedBackground;
                    image = tree.findImage(imageName);
                }
            }
            else if (name == "combo_list")
            {
                defaults.combo.listControl = *part->control;
                defaults.combo.listBackground = part->comboListBackground;
                defaults.combo.listBackgroundVisible = part->comboListBackgroundVisible;
            }
            else
            {
                auto& button = name == "action_button" ? defaults.action : name == "combo_button" ? defaults.button : defaults.dropDown;
                button.control = *part->control;
                button.button = *part->button;
                for (const auto& [attribute,imageName] : part->buttonImages) *buttonImage(button.button.images,attribute) = tree.findImage(imageName);
            }
        }
        return (!defaults.combo.flyout || resolveFont(defaults.action.control,resources,error)) &&
            resolveFont(defaults.editor.control,resources,error) && resolveFont(defaults.button.control,resources,error) &&
            resolveFont(defaults.dropDown.control,resources,error) && resolveFont(defaults.combo.listControl,resources,error);
    }

    bool resolveSliderControl(const Declaration& declaration,LLVKWidgetFactory::SliderControlDefaults& defaults,
        const LLVKWidgetTree& tree,const LLVKWidgetFactory::Resources& resources,std::string& error)
    {
        for (const auto& [name,part] : declaration.sliderParts)
        {
            if (!resolveImages(tree,*part,error)) return false;
            if (name=="editor")
            {
                defaults.editor.view=part->params; defaults.editor.control=*part->control; defaults.editor.editor=*part->lineEditor;
                for (const auto& [attribute,image] : part->lineImages)
                {
                    auto& target=attribute=="background_image" ? defaults.editor.editor.background :
                        attribute=="background_image_disabled" ? defaults.editor.editor.disabledBackground : defaults.editor.editor.focusedBackground;
                    target=tree.findImage(image);
                }
            }
            else
            { auto& text=name=="text" ? defaults.text : defaults.label; text.view=part->params; text.control=*part->control; text.text=*part->plainLabel; }
        }
        if (!defaults.editor.control.font && !defaults.editor.control.fontRequest) defaults.editor.control.fontRequest=resources.defaultFontRequest;
        if (!resolveFont(defaults.editor.control,resources,error)) return false;
        defaults.slider.editor=defaults.editor.editor;
        defaults.slider.editorControl=defaults.editor.control;
        defaults.slider.spacing=tree.setting("UISliderctrlSpacing").value_or(LLSD(4)).asInteger();
        return true;
    }

    bool resolveSpinner(const Declaration& declaration,LLVKWidgetFactory::SpinnerDefaults& defaults,
        const LLVKWidgetTree& tree,const LLVKWidgetFactory::Resources& resources,std::string& error)
    {
        if (declaration.control->fontRequest || declaration.control->font)
        {
            defaults.editor.control.fontRequest=declaration.control->fontRequest;
            defaults.editor.control.font=declaration.control->font;
        }
        for (const auto& [name,part] : declaration.spinnerButtons)
        {
            if (!resolveImages(tree,*part,error)) return false;
            auto& button=name=="up" ? defaults.up : defaults.down;
            button.view=part->params; button.control=*part->control; button.button=*part->button;
            for (const auto& [attribute,image] : part->buttonImages) *buttonImage(button.button.images,attribute)=tree.findImage(image);
        }
        for (auto* child : {&defaults.up.control,&defaults.down.control,&defaults.editor.control})
        {
            if (!child->font && !child->fontRequest) child->fontRequest=resources.defaultFontRequest;
            if (!resolveFont(*child,resources,error)) return false;
        }
        defaults.spinner.buttonControl=defaults.up.control;
        defaults.spinner.upButton=defaults.up.button;
        defaults.spinner.downButton=defaults.down.button;
        defaults.spinner.editorControl=defaults.editor.control;
        defaults.spinner.editor=defaults.editor.editor;
        defaults.spinner.spacing=tree.setting("UISpinctrlSpacing").value_or(LLSD(2)).asInteger();
        defaults.spinner.buttonWidth=tree.setting("UISpinctrlBtnWidth").value_or(LLSD(16)).asInteger();
        defaults.spinner.buttonHeight=tree.setting("UISpinctrlBtnHeight").value_or(LLSD(10)).asInteger();
        return true;
    }

    std::optional<LLVKWidgetTree::Id> build(LLVKWidgetTree& tree, const Declaration& declaration,
                                          LLVKWidgetTree::Id layoutParent,
                                          LLVKWidgetTree::Id owningParent, std::string& error,
                                          const Parser& environment, BuildState& buildState)
    {
        if (buildState.depth >= LLVKWidgetTree::maximumDepth)
        { error = "Native construction recursion exceeds depth limit"; return std::nullopt; }
        ++buildState.depth;
        struct DepthScope
        {
            BuildState& state;
            ~DepthScope() { --state.depth; }
        } depthScope{buildState};
        if (!resolveImages(tree,declaration,error)) return std::nullopt;
        const auto callbacks = activeCallbacks(environment.callbacks,buildState);
        LLVKWidgetFactory::PanelConstructor panelConstructor;
        if (declaration.panel)
        {
            if (!declaration.panelClass.empty())
            {
                const auto found = environment.resources.panelClasses.find(declaration.panelClass);
                if (found == environment.resources.panelClasses.end() || !found->second)
                { error = "Native panel class is not implemented: " + declaration.panelClass; return std::nullopt; }
                panelConstructor = found->second;
            }
            else
            {
                const auto found = environment.resources.panelFactories.find(declaration.params.view.name);
                if (found != environment.resources.panelFactories.end()) panelConstructor = found->second;
                if (!panelConstructor)
                    for (const auto* scope : buildState.panels)
                    {
                        const auto factory = scope->childFactories.find(declaration.params.view.name);
                        if (factory != scope->childFactories.end()) { panelConstructor = factory->second; break; }
                    }
            }
        }
        auto params = declaration.params.view;
        const auto* parent = tree.get(layoutParent);
        const auto parentLayout = parent ? parent->params.layout : std::string();
        auto rect = declaration.params.geometry.apply(tree,layoutParent,parentLayout,error);
        if (!rect) return std::nullopt;
        params.rect = *rect;
        params.layout = declaration.params.geometry.layout.empty() ? parentLayout : declaration.params.geometry.layout;
        auto icon = declaration.icon;
        const auto controlStorage = std::make_unique<std::optional<LLVKControl::Params>>(declaration.control);
        auto& control = *controlStorage;
        if (control && !declaration.panel && !resolveControl(*control,callbacks,environment.resources,error)) return std::nullopt;
        if (icon && declaration.imageName) icon->image = tree.findImage(*declaration.imageName);
        auto button = declaration.button;
        auto slider=declaration.slider ? std::make_unique<LLVKWidgetFactory::SliderDefaults>(*declaration.slider) : nullptr;
        if (slider)
        {
            auto& params=slider->slider;
            for (const auto& [name,target] : {std::pair{"thumb_image",&params.thumb},std::pair{"thumb_image_pressed",&params.pressedThumb},
                std::pair{"thumb_image_disabled",&params.disabledThumb},std::pair{params.vertical ? "track_image_vertical" : "track_image_horizontal",&params.track},
                std::pair{params.vertical ? "track_highlight_vertical_image" : "track_highlight_horizontal_image",&params.highlight}})
            {
                const auto found=slider->images.find(name);
                if (found!=slider->images.end()) { *target=tree.findImage(found->second,error); if (!error.empty()) return std::nullopt; }
            }
            if (!resolveCallback(params.mouseDown,callbacks.actions,error) || !resolveCallback(params.mouseUp,callbacks.actions,error)) return std::nullopt;
        }
        auto spinner=declaration.spinner ? std::make_unique<LLVKWidgetFactory::SpinnerDefaults>(*declaration.spinner) : nullptr;
        auto sliderControl=declaration.sliderControl ? std::make_unique<LLVKWidgetFactory::SliderControlDefaults>(*declaration.sliderControl) : nullptr;
        if (sliderControl)
        {
            if (!resolveSliderControl(declaration,*sliderControl,tree,environment.resources,error)) return std::nullopt;
            sliderControl->slider.bar=slider->slider;
            if (!resolveCallback(sliderControl->slider.editorCommit,callbacks.actions,error)) return std::nullopt;
        }
        if (spinner && !resolveSpinner(declaration,*spinner,tree,environment.resources,error)) return std::nullopt;
        std::vector<LLVKWidgetTree::RadioItemParams> radioItems;
        if (declaration.radioGroup)
        {
            for (const auto& child : declaration.children)
            {
                if (!child->radioItem || !child->checkBox) { error="Native radio group requires radio items"; return std::nullopt; }
                auto defaults=std::make_unique<LLVKWidgetFactory::CheckBoxDefaults>(*child->checkBox);
                if (!resolveImages(tree,*child,error) || !resolveCheckBox(*child,*defaults,tree,environment.resources,error)) return std::nullopt;
                auto item=std::make_unique<LLVKWidgetTree::RadioItemParams>();
                item->view=child->params.view;
                item->layout=std::make_shared<LLVKWidgetLayout>(child->params.geometry);
                item->control=*child->control;
                if (!child->explicitFont)
                {
                    item->control.font=control->font;
                    item->control.fontRequest=control->fontRequest;
                }
                if (!resolveControl(item->control,callbacks,environment.resources,error)) return std::nullopt;
                item->check=defaults->construction;
                item->check.fontProvided=true;
                const auto labelRect=defaults->labelView.geometry.apply(tree,0,"",error);
                const auto buttonRect=defaults->buttonView.geometry.apply(tree,0,"",error);
                if (!labelRect || !buttonRect) return std::nullopt;
                item->check.labelView=defaults->labelView.view;
                item->check.labelView.rect=*labelRect;
                item->check.buttonView=defaults->buttonView.view;
                item->check.buttonView.rect=*buttonRect;
                item->payload=child->radioPayload;
                radioItems.push_back(std::move(*item));
            }
        }
        auto combo = declaration.combo ? std::make_unique<LLVKWidgetFactory::ComboDefaults>(*declaration.combo) : nullptr;
        if (combo)
        {
            if (!resolveCombo(declaration,*combo,tree,environment.resources,error)) return std::nullopt;
            auto& childButton = combo->combo.allowTextEntry ? combo->button : combo->dropDown;
            if (!resolveControl(childButton.control,callbacks,environment.resources,error) || !resolveButton(childButton.button,callbacks,error) ||
                !resolveControl(combo->editor.control,callbacks,environment.resources,error) ||
                !resolveControl(combo->combo.listControl,callbacks,environment.resources,error)) return std::nullopt;
            for (auto* callback : {&combo->combo.textEntry,&combo->combo.textChanged,&combo->combo.prearrange})
                if (!resolveCallback(*callback,callbacks.actions,error)) return std::nullopt;
            auto& editor = combo->editor.editor;
            for (const auto& [name,function] : {std::pair{&editor.prevalidatorName,&editor.prevalidator},std::pair{&editor.inputPrevalidatorName,&editor.inputPrevalidator}})
            {
                if (!*name || *function) continue;
                const auto found = callbacks.textValidators.find(**name);
                *function = found == callbacks.textValidators.end() ? builtinTextValidator(**name) : found->second;
                if (!*function) { error = "Unresolved native combo text validator: " + **name; return std::nullopt; }
            }
            combo->combo.editorControl = combo->editor.control;
            combo->combo.editor = editor;
            combo->combo.buttonControl = childButton.control;
            combo->combo.button = childButton.button;
            if (combo->combo.flyout)
            {
                if (!resolveControl(combo->action.control,callbacks,environment.resources,error) ||
                    !resolveButton(combo->action.button,callbacks,error)) return std::nullopt;
                combo->combo.actionControl=combo->action.control;
                combo->combo.actionButton=combo->action.button;
            }
        }
        auto container = declaration.scrollContainer ? std::make_unique<LLVKWidgetFactory::ScrollContainerDefaults>(*declaration.scrollContainer) : nullptr;
        if (container)
        {
            for (const auto& child : declaration.children)
                if (!child->panel && !child->containerView)
                { error = "Native scroll container child constructor is not implemented: " + child->tag; return std::nullopt; }
            if (!resolveCallback(container->scrolled,callbacks.actions,error)) return std::nullopt;
            auto defaults = std::make_unique<LLVKWidgetFactory::ScrollbarDefaults>(environment.scrollDefaults);
            if (defaults->buttons.size() != 4)
            { error = "Native scroll container requires loaded scrollbar defaults"; return std::nullopt; }
            container->container.scrollbarControl = defaults->control;
            if (!resolveControl(container->container.scrollbarControl,callbacks,environment.resources,error)) return std::nullopt;
            container->container.border = environment.panelDefaults.panel.border;
            for (bool vertical : {true,false})
            {
                auto& bar = vertical ? container->container.vertical : container->container.horizontal;
                bar = defaults->scrollbar;
                bar.vertical = vertical;
                auto& decrease = defaults->buttons.at(vertical ? "up_button" : "left_button");
                auto& increase = defaults->buttons.at(vertical ? "down_button" : "right_button");
                for (auto* child : {&decrease,&increase})
                    if (!resolveControl(child->control,callbacks,environment.resources,error) ||
                        !resolveButton(child->button,callbacks,error)) return std::nullopt;
                bar.decreaseControl = decrease.control; bar.increaseControl = increase.control;
                bar.decreaseButton = decrease.button; bar.increaseButton = increase.button;
                const auto callback = container->scrolled;
                bar.changed = callback.function ? std::function<void(LLVKWidgetTree::Id,std::int32_t)>([callback](auto id,auto position)
                    { callback.function(id,callback.parameter.value_or(LLSD(position))); }) : std::function<void(LLVKWidgetTree::Id,std::int32_t)>();
            }
        }
        auto scroll = declaration.scrollbar ? std::make_unique<LLVKWidgetFactory::ScrollbarDefaults>(*declaration.scrollbar) : nullptr;
        if (scroll)
        {
            if (!resolveScrollbar(declaration,*scroll,tree,environment.resources,error) ||
                !resolveCallback(scroll->changed,callbacks.actions,error)) return std::nullopt;
            auto& decrease = scroll->buttons.at(scroll->scrollbar.vertical ? "up_button" : "left_button");
            auto& increase = scroll->buttons.at(scroll->scrollbar.vertical ? "down_button" : "right_button");
            for (auto* child : {&decrease,&increase})
                if (!resolveControl(child->control,callbacks,environment.resources,error) ||
                    !resolveButton(child->button,callbacks,error)) return std::nullopt;
            scroll->scrollbar.decreaseControl = decrease.control;
            scroll->scrollbar.increaseControl = increase.control;
            scroll->scrollbar.decreaseButton = decrease.button;
            scroll->scrollbar.increaseButton = increase.button;
            const auto changed = scroll->changed;
            if (changed.function) scroll->scrollbar.changed = [changed](auto id,auto position)
            { changed.function(id,changed.parameter.value_or(LLSD(position))); };
        }
        auto check = declaration.checkBox ? std::make_unique<LLVKWidgetFactory::CheckBoxDefaults>(*declaration.checkBox) : nullptr;
        if (check)
        {
            if (!resolveCheckBox(declaration,*check,tree,environment.resources,error)) return std::nullopt;
            auto& construction = check->construction;
            if (!resolveControl(construction.labelControl,callbacks,environment.resources,error) ||
                !resolveControl(construction.buttonControl,callbacks,environment.resources,error) ||
                !resolveButton(construction.button,callbacks,error) ||
                !resolveCallback(construction.onCheck,callbacks.predicates,error)) return std::nullopt;
            const auto labelRect = check->labelView.geometry.apply(tree,0,"",error);
            if (!labelRect) return std::nullopt;
            const auto buttonRect = check->buttonView.geometry.apply(tree,0,"",error);
            if (!buttonRect) return std::nullopt;
            construction.labelView = check->labelView.view;
            construction.labelView.rect = *labelRect;
            construction.buttonView = check->buttonView.view;
            construction.buttonView.rect = *buttonRect;
        }
        auto lineEditor = declaration.lineEditor;
        if (lineEditor)
        {
            for (const auto& [name,function] : {std::pair{&lineEditor->prevalidatorName,&lineEditor->prevalidator},
                                               std::pair{&lineEditor->inputPrevalidatorName,&lineEditor->inputPrevalidator}})
            {
                if (!*name || *function) continue;
                const auto found = callbacks.textValidators.find(**name);
                *function = found == callbacks.textValidators.end() ? builtinTextValidator(**name) : found->second;
                if (!*function)
                { error = "Unresolved native text validator: " + **name; return std::nullopt; }
            }
            if (!resolveCallback(lineEditor->keystroke,callbacks.actions,error)) return std::nullopt;
            for (const auto& [attribute,name] : declaration.lineImages)
            {
                auto& image = attribute == "background_image" ? lineEditor->background :
                    attribute == "background_image_disabled" ? lineEditor->disabledBackground : lineEditor->focusedBackground;
                image = tree.findImage(name);
            }
        }
        if (button && !resolveButton(*button,callbacks,error)) return std::nullopt;
        std::shared_ptr<LLVKWidgetFactory::SearchEditorDefaults> searchEditor;
        std::shared_ptr<LLVKWidgetFactory::ScrollListDefaults> scrollList;
        if (declaration.scrollList)
        {
            scrollList=std::make_shared<LLVKWidgetFactory::ScrollListDefaults>(*declaration.scrollList);
            if (!resolveScrollList(declaration,*scrollList,tree,environment.resources,error) ||
                !resolveControl(scrollList->list.scrollbarControl,callbacks,environment.resources,error)) return std::nullopt;
            for (auto& row : scrollList->list.rows)
                for (auto& style : row.styles)
                    if (style.type==LLVKWidgetTree::ListCellStyle::Type::CheckBox)
                    {
                        const auto& images=environment.checkDefaults.construction.button.images;
                        style.image=images.unselected;
                        style.checkedImage=images.selected;
                        style.disabledImage=images.disabled;
                        style.disabledCheckedImage=images.disabledSelected;
                        if (!style.image || !style.checkedImage || !style.disabledImage || !style.disabledCheckedImage)
                        { error="Native inline list requires resolved checkbox images"; return {}; }
                    }
        }
        std::shared_ptr<LLVKWidgetFactory::ColorSwatchDefaults> colorSwatch;
        if (declaration.colorSwatch)
        {
            colorSwatch=std::make_shared<LLVKWidgetFactory::ColorSwatchDefaults>(*declaration.colorSwatch);
            colorSwatch->swatch.showPicker=environment.resources.colorPickerHandler;
            if (!resolveColorSwatch(declaration,*colorSwatch,tree,error) ||
                !resolveControl(colorSwatch->swatch.captionControl,callbacks,environment.resources,error) ||
                !resolveCallback(colorSwatch->swatch.selected,callbacks.actions,error) ||
                !resolveCallback(colorSwatch->swatch.cancelled,callbacks.actions,error)) return std::nullopt;
        }
            std::shared_ptr<LLVKWidgetFactory::TextureDefaults> texture;
            if (declaration.texture)
            {
                texture=std::make_shared<LLVKWidgetFactory::TextureDefaults>(*declaration.texture);
                texture->texture.showPicker=environment.resources.texturePickerHandler;
                if (!resolveTexture(declaration,*texture,tree,error) ||
                !resolveControl(texture->texture.captionControl,callbacks,environment.resources,error) ||
                !resolveControl(texture->texture.multipleControl,callbacks,environment.resources,error)) return {};
            }
        if (declaration.searchEditor)
        {
            searchEditor=std::make_shared<LLVKWidgetFactory::SearchEditorDefaults>(*declaration.searchEditor);
            if (!resolveSearchEditor(declaration,*searchEditor,tree,error)) return std::nullopt;
            searchEditor->search.editor=*lineEditor;
            searchEditor->search.keystroke=lineEditor->keystroke;
            if (!resolveControl(searchEditor->search.buttonControl,callbacks,environment.resources,error) ||
                !resolveButton(searchEditor->search.searchButton,callbacks,error) ||
                !resolveButton(searchEditor->search.clearButton,callbacks,error)) return std::nullopt;
        }
        if (button)
            for (const auto& [attribute,name] : declaration.buttonImages)
                *buttonImage(button->images,attribute) = tree.findImage(name);
        auto badge = declaration.badge;
        if (badge && declaration.badgeImage) badge->image = tree.findImage(*declaration.badgeImage);
        if (badge && declaration.badgeBorderImage) badge->borderImage = tree.findImage(*declaration.badgeBorderImage);
        auto construction = declaration.badgeConstruction;
        if (button && declaration.ownedBadge)
        {
            const auto& owned = *declaration.ownedBadge;
            if (!owned.children.empty())
            { error = "Native button badge parameters cannot contain child widgets"; return std::nullopt; }
            construction.provided = owned.badge;
            construction.view = owned.params.view;
            auto rectangle = owned.params.geometry.apply(tree,0,"",error);
            if (!rectangle) return std::nullopt;
            construction.view.rect = *rectangle;
            construction.control = *owned.control;
            if (owned.badgeImage) construction.provided->image = tree.findImage(*owned.badgeImage);
            if (owned.badgeBorderImage) construction.provided->borderImage = tree.findImage(*owned.badgeBorderImage);
        }
        auto panel = declaration.panel;
        const bool typedLayoutPanel = declaration.layoutPanel.has_value();
        if (declaration.layoutStack)
            for (const auto& child : declaration.children)
                if (!child->layoutPanel) { error = "Native layout stack requires layout_panel children"; return std::nullopt; }
        if ((typedLayoutPanel || declaration.browser) && (!resolveControl(*control,callbacks,environment.resources,error) ||
            !resolveCallback(panel->visible,callbacks.actions,error))) return std::nullopt;
        if (button && !resolveControl(construction.control,callbacks,environment.resources,error)) return std::nullopt;
        if (panel && declaration.panelOpaqueImage) panel->opaqueImage = tree.findImage(*declaration.panelOpaqueImage);
        if (panel && declaration.panelTransparentImage) panel->transparentImage = tree.findImage(*declaration.panelTransparentImage);
        auto constructorDefaults = std::make_unique<LLVKWidgetFactory::PanelDefaults>(declaration.panelConstructor);
        auto& constructorView = constructorDefaults->view.view;
        auto& constructorControl = constructorDefaults->control;
        auto& constructorPanel = constructorDefaults->panel;
        if (panel && !typedLayoutPanel && !declaration.browser)
        {
            if (!panelConstructor && (!resolveControl(constructorControl,callbacks,environment.resources,error) ||
                !resolveCallback(constructorPanel.visible,callbacks.actions,error))) return std::nullopt;
            const auto constructorRect = declaration.panelConstructor.view.geometry.resolve(error);
            if (!constructorRect) return std::nullopt;
            constructorView.rect = *constructorRect;
        }
        std::optional<LLVKWidgetFactory::PanelInstance> instance;
        const auto lease = std::make_shared<bool>(true);
        const LLVKWidgetFactory::Construction context(
            [lifetime = std::weak_ptr<bool>(lease),&tree,&environment,&buildState](std::string_view xml,LLVKWidgetTree::Id parent,std::string& problem)
            -> std::optional<LLVKWidgetTree::Id>
            {
                const auto alive = lifetime.lock();
                if (!alive) { problem = "Native construction context has expired"; return std::nullopt; }
                if (parent && !tree.get(parent)) { problem = "Native nested construction parent is missing"; return std::nullopt; }
                constexpr std::size_t maximumBytes = 64 * 1024 * 1024;
                if (xml.size() > maximumBytes-buildState.bytes)
                { problem = "Native nested construction exceeds total byte budget"; return std::nullopt; }
                buildState.bytes += xml.size();
                const auto nested = std::make_unique<Parser>(environment.defaults,environment.iconDefaults,environment.buttonDefaults,
                    environment.callbacks,environment.resources,environment.panelDefaults,environment.lineDefaults,environment.checkDefaults,environment.scrollDefaults,environment.containerDefaults,environment.layoutDefaults,environment.comboDefaults,environment.textDefaults,environment.browserDefaults);
                if (!parse(*nested,xml,problem)) return std::nullopt;
                return build(tree,*nested->root,parent,parent,problem,environment,buildState);
            });
        if (panelConstructor)
        {
            const auto boundary = tree.nextWidgetId();
            instance = panelConstructor(tree,declaration.panelConstructor,context,error);
            const auto* created = tree.get(instance->id);
            if (!created || instance->id < boundary || !created->panel || created->parent)
            {
                if (created && instance->id >= boundary) { std::string cleanup; tree.erase(instance->id,cleanup); }
                if (error.empty()) error = "Native panel constructor must transfer a newly constructed detached panel";
                return std::nullopt;
            }
        }
        const auto id = instance ? std::optional(instance->id) : typedLayoutPanel ? tree.createPanel(params,*control,*panel,0,error) :
             declaration.browser ? tree.createBrowser(params,*control,*panel,*declaration.browser,0,error) :
               panel ? tree.createPanel(constructorView,constructorControl,constructorPanel,0,error) :
               declaration.layoutStack ? tree.createLayoutStack(params,declaration.layoutStack->vertical,declaration.layoutStack->spacing,declaration.layoutStack->clip,owningParent,error) :
                   sliderControl ? tree.createSliderControl(params,*control,sliderControl->slider,owningParent,error) :
                   declaration.progress ? tree.createProgressBar(params,*control,*declaration.progress,owningParent,error) :
                   slider ? tree.createSlider(params,*control,slider->slider,owningParent,error) :
                   declaration.radioGroup ? tree.createRadioGroup(params,*control,radioItems,declaration.allowDeselect,owningParent,error) :
                   spinner ? tree.createSpinner(params,*control,spinner->spinner,owningParent,error) :
                   combo ? tree.createCombo(params,*control,combo->combo,owningParent,error) :
                   declaration.textEditor ? tree.createTextEditor(params,*control,*declaration.plainLabel,container->container,
                       declaration.textEditor->borderVisible,owningParent,error) :
                   container ? tree.createScrollContainer(params,*control,container->container,owningParent,error) :
                   scroll ? tree.createScrollbar(params,*control,scroll->scrollbar,owningParent,error) :
                   check ? tree.createCheckBox(params,*control,check->construction,owningParent,error) :
                   scrollList ? tree.createScrollList(params,*control,scrollList->list,owningParent,error) :
                   colorSwatch ? tree.createColorSwatch(params,*control,colorSwatch->swatch,owningParent,error) :
                   texture ? tree.createTextureControl(params,*control,texture->texture,owningParent,error) :
                   searchEditor ? tree.createSearchEditor(params,*control,searchEditor->search,owningParent,error) :
                   lineEditor ? tree.createLineEditor(params,*control,*lineEditor,owningParent,error) :
                   declaration.plainLabel ? tree.createPlainText(params,*control,*declaration.plainLabel,owningParent,error) :
                   button ? tree.createButton(params,*control,*button,owningParent,error,construction) :
                       badge ? tree.createBadge(params,*control,*badge,0,owningParent,error) :
                       icon ? tree.createIcon(params,*control,*icon,owningParent,error) :
                       declaration.border ? tree.createBorder(params,*declaration.border,owningParent,error) :
                       (declaration.tag=="locate" || !declaration.menuXml.empty()) ? tree.createControl(params,*control,owningParent,error) :
                       tree.create(params,owningParent,error);
        if (!id) return std::nullopt;
        if (check && !tree.reshape(*id,params.rect.right-params.rect.left,params.rect.top-params.rect.bottom,error))
        { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
        if (!declaration.menuXml.empty())
        {
            auto menu=LLVKMenu::create(declaration.menuXml,control->font,environment.resources.colors,{},false,error);
            if (!menu) { std::string ignored; tree.erase(*id,ignored); return std::nullopt; }
            const auto width=menu->barWidth(error);
            if (!width || !tree.reshape(*id,*width,18,error))
            { std::string ignored; tree.erase(*id,ignored); return std::nullopt; }
            for (const auto& [name,handler] : callbacks.actions)
                menu->bind(name,[handler,owner=*id](const auto&,const auto& parameter) { handler(owner,LLSD(parameter)); });
            for (const auto& [name,predicate] : callbacks.predicates)
                menu->bindPredicate(name,[predicate,owner=*id](const auto& parameter) { return predicate(owner,LLSD(parameter)); });
            tree.setMenu(*id,std::move(menu));
        }
        if (declaration.tag=="menu_button" && !declaration.menuFilename.empty())
        {
            if (!environment.resources.menuHandler)
            { std::string ignored; tree.erase(*id,ignored); error="Native menu-button service is unavailable"; return std::nullopt; }
            auto registry=std::make_shared<LLVKWidgetFactory::Callbacks>(callbacks);
            tree.setMenuButtonHandler(*id,[handler=environment.resources.menuHandler,registry,
                filename=declaration.menuFilename,position=declaration.menuPosition](auto owner)
            { handler(owner,filename,position,*registry); });
        }
        if ((declaration.tag=="fs_copytrans_inventory_drop_target" || declaration.tag=="fs_embedded_item_drop_target") && !tree.initializeInventoryDropTarget(*id,error))
        { std::string ignored; tree.erase(*id,ignored); return std::nullopt; }
        struct PanelScope
        {
            BuildState& state;
            bool pushed = false;
            ~PanelScope() { if (pushed) state.panels.pop_back(); }
        } scope{buildState};
        try
        {
            if (declaration.layoutStack && !tree.configureLayoutStack(*id,declaration.layoutStack->animate,
                declaration.layoutStack->openTime,declaration.layoutStack->closeTime,error))
            { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            if (typedLayoutPanel)
            {
                const auto* owner = tree.get(owningParent);
                if (!owner || !owner->layoutStack || !tree.attachLayoutPanel(owningParent,*id,*declaration.layoutPanel,error))
                { if (error.empty()) error = "Native layout panel requires a layout stack"; std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            }
            if (instance) { buildState.panels.push_back(&*instance); scope.pushed = true; }
            const auto initialize = [&]() -> bool
            {
                if (!panel || typedLayoutPanel || declaration.browser) return true;
                if (!instance && !tree.postBuildControl(*id)) { error = "Native panel default post-build failed"; return false; }
                const auto filename = tree.get(*id)->panel->params.filename.empty() ? panel->filename :
                                      tree.get(*id)->panel->params.filename;
                if (!tree.setPanelFilename(*id,filename)) { error = "Native panel disappeared before file resolution"; return false; }
                std::unique_ptr<Parser> referenced;
                const Declaration* effective = &declaration;
                if (!filename.empty())
                {
                    if (buildState.files.size() >= LLVKWidgetTree::maximumDepth ||
                        std::find(buildState.files.begin(),buildState.files.end(),filename) != buildState.files.end())
                    { error = "Native panel reference cycle or depth limit: " + filename; return false; }
                    const auto file = declarationFile(environment.resources,filename,buildState.bytes,error);
                    if (!file) return false;
                    referenced = std::make_unique<Parser>(environment.defaults,environment.iconDefaults,
                        environment.buttonDefaults,environment.callbacks,environment.resources,environment.panelDefaults,environment.lineDefaults,environment.checkDefaults,environment.scrollDefaults,environment.containerDefaults,environment.layoutDefaults,environment.comboDefaults,environment.textDefaults,environment.browserDefaults);
                    if (!parse(*referenced,*file,error)) { error = filename + ": " + error; return false; }
                    if (!referenced->root->panel)
                    { error = "Native referenced declaration is not a panel: " + filename; return false; }
                    const auto referenceRect = referenced->root->params.geometry.resolve(error);
                    if (!referenceRect || !tree.setShape(*id,*referenceRect,error)) return false;
                    {
                        buildState.files.push_back(filename);
                        struct FileScope
                        {
                            std::vector<std::string>& files;
                            ~FileScope() { files.pop_back(); }
                        } scope{buildState.files};
                        for (const auto& child : referenced->root->children)
                            if (!build(tree,*child,*id,*id,error,environment,buildState)) return false;
                    }
                    for (const auto& [name,value] : declaration.attributes)
                        if (!referenced->attribute(*referenced->root,name,value))
                        { error = "Invalid native panel override: " + name; return false; }
                    for (const auto& callback : declaration.panelCallbacks)
                    {
                        std::vector<const XML_Char*> attributes;
                        for (const auto& [name,value] : callback.attributes)
                        { attributes.push_back(name.c_str()); attributes.push_back(value.c_str()); }
                        attributes.push_back(nullptr);
                        if (!referenced->callback(*referenced->root,callback.tag,attributes.data()))
                        { error = "Unresolved native panel callback override"; return false; }
                    }
                    for (const auto& [name,value] : declaration.panelStrings)
                        referenced->root->panel->strings.insert_or_assign(name,value);
                    effective = referenced->root.get();
                }
                params = effective->params.view;
                const auto rectangle = effective->params.geometry.apply(tree,layoutParent,parentLayout,error);
                if (!rectangle) return false;
                params.rect = *rectangle;
                params.layout = effective->params.geometry.layout.empty() ? parentLayout : effective->params.geometry.layout;
                panel = effective->panel;
                if (!resolveImages(tree,*effective,error)) return false;
                panel->filename = filename;
                if (effective->panelOpaqueImage) panel->opaqueImage = tree.findImage(*effective->panelOpaqueImage);
                if (effective->panelTransparentImage) panel->transparentImage = tree.findImage(*effective->panelTransparentImage);
                control = effective->control;
                const auto localCallbacks = activeCallbacks(environment.callbacks,buildState);
                if (!resolveControl(*control,localCallbacks,environment.resources,error) ||
                    !resolveCallback(panel->visible,localCallbacks.actions,error)) return false;
                return tree.initializePanel(*id,params,*control,*panel,error);
            };
            if (!initialize())
            {
                std::string cleanupError;
                tree.erase(*id,cleanupError);
                return std::nullopt;
            }
            if (declaration.floater && !tree.initializeFloater(*id,*declaration.floater,error))
            { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            for (const auto& child : declaration.children)
            {
                if (declaration.radioGroup) continue;
                if (!child->panelClass.empty() && environment.resources.excludedPanelClasses.contains(child->panelClass)) continue;
                if (!build(tree,*child,*id,*id,error,environment,buildState))
                {
                    std::string cleanupError;
                    tree.erase(*id,cleanupError);
                    return std::nullopt;
                }
            }
            if (declaration.layoutStack && !tree.updateLayoutStack(*id,error))
            { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            if (declaration.statBar)
            {
                auto params=std::make_unique<LLVKWidgetTree::Node::StatBar>(*declaration.statBar);
                if (!params->statName.empty() &&
                    (LLTrace::StatType<LLTrace::CountAccumulator>::getInstance(params->statName) ||
                     LLTrace::StatType<LLTrace::EventAccumulator>::getInstance(params->statName) ||
                     LLTrace::StatType<LLTrace::SampleAccumulator>::getInstance(params->statName)))
                { error="Native live named-stat recording is not implemented: "+params->statName; std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
                auto label=std::make_unique<LLVKControl::Params>();
                label->fontRequest=LLVKFontRegistry::Request{"Monospace","Medium"};
                if (!resolveFont(*label,environment.resources,error)) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
                params->font=label->font;
                if (!tree.initializeStatBar(*id,*params,error)) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            }
            if (declaration.overlapPanel)
            {
                auto params=*declaration.overlapPanel;
                auto text=std::make_unique<LLVKControl::Params>();
                text->fontRequest=LLVKFontRegistry::Request{"SansSerif","Small"};
                if (!resolveFont(*text,environment.resources,error)) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
                params.font=text->font;
                if (!tree.initializeOverlapPanel(*id,params,error)) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            }
            if (declaration.containerView)
            {
                auto params=std::make_unique<LLVKWidgetTree::Node::ContainerView>(*declaration.containerView);
                if (params->showLabel)
                {
                    auto label=std::make_unique<LLVKControl::Params>();
                    label->fontRequest=LLVKFontRegistry::Request{"Monospace","Medium"};
                    if (!resolveFont(*label,environment.resources,error)) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
                    params->font=label->font;
                }
                if (!tree.initializeContainerView(*id,*params,error) || (owningParent && tree.get(owningParent)->scrollContainer &&
                    !tree.attachScrollContent(owningParent,*id,0,error)))
                { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            }
            if (declaration.tabs)
            {
                const auto constructTabs = [&]() -> bool
                {
                    if (!tree.initializeTabContainer(*id,error)) return false;
                    auto children = tree.get(*id)->children;
                    std::reverse(children.begin(),children.end());
                    std::vector<LLVKWidgetTree::Id> panels;
                    for (const auto child : children) if (tree.get(child) && tree.get(child)->panel) panels.push_back(child);
                    for (std::size_t index = 0; index < panels.size(); ++index)
                    {
                        const auto child = panels[index];
                        auto button = environment.buttonDefaults.button;
                        button.label = labelText(tree.get(child)->panel->params.label);
                        button.selectedLabel = button.label;
                        button.labelAlign = declaration.tabs->alignment;
                        button.leftPad = declaration.tabs->labelPadLeft;
                        button.bottomPad = declaration.tabs->labelPadBottom;
                        button.labelShadow = declaration.tabs->labelShadow;
                        button.toggle = false;
                        button.click.reset();
                        const auto& images = declaration.tabs->images[index == 0 ? 0 : index+1 == panels.size() ? 2 : 1];
                        using Position = LLVKWidgetTree::Node::TabContainer::Layout::Position;
                        if (declaration.tabs->layout.position!=Position::Left) button.rightPad=2;
                        const std::string prefix = declaration.tabs->layout.position == Position::Top ? "tab_top_image_" :
                            declaration.tabs->layout.position == Position::Bottom ? "tab_bottom_image_" : "tab_left_image_";
                        for (const auto& [name,target] : {std::pair{"unselected",&button.images.unselected},
                            std::pair{"selected",&button.images.selected},std::pair{"flash",&button.images.flash}})
                        {
                            const auto found = images.find(prefix+name);
                            if (found != images.end()) { *target = tree.findImage(found->second,error); if (!error.empty()) return false; }
                        }
                        button.images.pressed = button.images.selected;
                        button.images.pressedSelected = button.images.selected;
                        button.pressedProvided = button.pressedSelectedProvided = true;
                        LLVKControl::Params buttonControl;
                        buttonControl.font = control->font;
                        buttonControl.tabStop = false;
                        LLVKWidgetTree::Params buttonView;
                        buttonView.name = (declaration.tabs->layout.position == Position::Left ? "vtab_" : "htab_")+tree.get(child)->params.name;
                        buttonView.rect = {0,0,60,declaration.tabs->layout.tabHeight};
                        const auto tab = tree.createButton(buttonView,buttonControl,button,*id,error);
                        if (!tab || !tree.attachTabPanel(*id,child,*tab,error)) return false;
                    }
                    auto layout = declaration.tabs->layout;
                    layout.minimumWidth = declaration.tabs->width.value_or(layout.position == LLVKWidgetTree::Node::TabContainer::Layout::Position::Left ?
                        tree.setting("UITabCntrVertTabMinWidth").value_or(LLSD(100)).asInteger() : layout.minimumWidth);
                    layout.verticalPadding = tree.setting("UITabCntrvPad").value_or(LLSD(0)).asInteger();
                    layout.labelPadding = tree.setting("UITabPadding").value_or(LLSD(0)).asInteger();
                    layout.horizontalPadding = tree.setting("UITabCntrTabHPad").value_or(LLSD(0)).asInteger();
                    layout.panelOverlap = tree.setting("UITabCntrButtonPanelOverlap").value_or(LLSD(0)).asInteger();
                    if (!tree.layoutTabPanels(*id,layout,error)) return false;
                    auto arrows=environment.buttonDefaults.button;
                    if (!resolveButton(arrows,callbacks,error) || !tree.createTabArrows(*id,*control,arrows,error)) return false;
                    return panels.empty() || tree.selectTabPanel(*id,panels.front(),error);
                };
                if (!constructTabs()) { std::string cleanup; tree.erase(*id,cleanup); return std::nullopt; }
            }
            if (panel && !typedLayoutPanel && owningParent)
            {
                const auto* owner = tree.get(owningParent);
                const bool attached = owner && (owner->scrollContainer ?
                    tree.attachScrollContent(owningParent,*id,params.tabGroup.value_or(owner->lastTabGroup),error) :
                    tree.reparent(*id,owningParent,false,params.tabGroup.value_or(owner->lastTabGroup),error));
                if (!attached)
                {
                    if (error.empty()) error = "Native panel lost its parent before attachment";
                    std::string cleanupError;
                    tree.erase(*id,cleanupError);
                    return std::nullopt;
                }
            }
            const bool built = instance && instance->postBuild ? instance->postBuild(tree,*id,context,error) :
                combo ? tree.postBuildCombo(*id,error) :
                button ? tree.postBuildButton(*id,error) :
                icon || badge || panel || lineEditor || check || scroll || container || spinner ? tree.postBuildControl(*id) : true;
            if (!built || !tree.get(*id))
            {
                if (error.empty()) error = "Native control no longer exists at post-build";
                if (tree.get(*id)) { std::string cleanupError; tree.eraseControl(*id,cleanupError); }
                return std::nullopt;
            }
        }
        catch (const std::exception& failure)
        {
            const auto message="Native construction of "+params.name+" ("+declaration.panelClass+"): "+failure.what();
            std::string cleanupError;
            tree.erase(*id,cleanupError);
            throw std::runtime_error(message);
        }
        catch (...)
        {
            std::string cleanupError;
            tree.erase(*id,cleanupError);
            throw;
        }
        return id;
    }
}

LLVKWidgetFactory::LLVKWidgetFactory(Defaults defaults) : mDefaults(std::move(defaults)) {}

LLVKWidgetFactory::IconDefaults::IconDefaults()
{
    view.view.name = "icon";
    view.view.mouseOpaque = false;
    view.view.follows = LLVKWidgetTree::Left | LLVKWidgetTree::Top;
    control.tabStop = false;
}

LLVKWidgetFactory::ButtonDefaults::ButtonDefaults()
{
    view.geometry.height = {23,true};
    button.bottomPad = 2;
}

LLVKWidgetFactory::BadgeDefaults::BadgeDefaults()
{
    view.view.name = "badge";
    view.view.mouseOpaque = false;
    control.requestsFront = true;
    badge.percentHorizontal = badge.percentVertical = 85;
    badge.paddingHorizontal = 7.f;
    badge.paddingVertical = 4.f;
}

LLVKWidgetFactory::PanelDefaults::PanelDefaults()
{
    view.view.name = "panel";
    borderView.view.name = "view_border";
    borderView.view.mouseOpaque = false;
    borderView.view.follows = LLVKWidgetTree::Left | LLVKWidgetTree::Right | LLVKWidgetTree::Top | LLVKWidgetTree::Bottom;
}

LLVKWidgetFactory::LineEditorDefaults::LineEditorDefaults()
{
    view.view.name = "line_editor";
}

LLVKWidgetFactory::LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults)
    : mDefaults(std::move(defaults)), mIconDefaults(std::move(iconDefaults)) {}

LLVKWidgetFactory::LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults,
        ButtonDefaults buttonDefaults, Callbacks callbacks, Resources resources, PanelDefaults panelDefaults, LineEditorDefaults lineDefaults, CheckBoxDefaults checkDefaults)
        : mDefaults(std::move(defaults)), mIconDefaults(std::move(iconDefaults)),
            mButtonDefaults(std::move(buttonDefaults)), mCallbacks(std::move(callbacks)), mResources(std::move(resources)),
            mPanelDefaults(std::move(panelDefaults)), mLineDefaults(std::move(lineDefaults)), mCheckDefaults(std::move(checkDefaults)) {}

std::optional<LLVKWidgetTree::Id> LLVKWidgetFactory::construct(LLVKWidgetTree& tree,
    const LLVKWidgetTree::Params& params, LLVKWidgetTree::Id parent, std::string& error) const
{
    return tree.create(params,parent,error);
}

bool LLVKWidgetFactory::loadDefaultsFile(const LLVKWidgetTree& tree, const std::string& filename, std::string& error)
{
    error.clear();
    std::size_t bytes = 0;
    const auto xml = declarationFile(mResources,filename,bytes,error);
    return xml && loadDefaults(tree,*xml,error);
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetFactory::constructFile(LLVKWidgetTree& tree,
    const std::string& filename, LLVKWidgetTree::Id parent, std::string& error) const
{
    error.clear();
    BuildState buildState;
    const auto xml = declarationFile(mResources,filename,buildState.bytes,error);
    if (!xml) return std::nullopt;
    Parser parser(mDefaults,mIconDefaults,mButtonDefaults,mCallbacks,mResources,mPanelDefaults,mLineDefaults,mCheckDefaults,*mScrollDefaults,*mContainerDefaults,mLayoutDefaults,*mComboDefaults,*mTextDefaults,*mBrowserDefaults);
    if (!parse(parser,*xml,error)) return std::nullopt;
    const auto* owner = tree.get(parent);
    if (parent && !owner) { error = "Native file construction parent is missing"; return std::nullopt; }
    const auto children = owner ? owner->children : std::vector<LLVKWidgetTree::Id>();
    const auto tabGroup = owner ? owner->lastTabGroup : 0;
    const auto restore = [&]
    {
        const auto* current = tree.get(parent);
        if (current && current->children == children) tree.mNodes.at(parent).lastTabGroup = tabGroup;
    };
    try
    {
        const auto result = build(tree,*parser.root,parent,parent,error,parser,buildState);
        if (!result) restore();
        return result;
    }
    catch (...) { restore(); throw; }
}

bool LLVKWidgetFactory::loadDefaults(const LLVKWidgetTree& tree, std::string_view xml, std::string& error)
{
    error.clear();
    Parser state(mDefaults,mIconDefaults,mButtonDefaults,mCallbacks,mResources,mPanelDefaults,mLineDefaults,mCheckDefaults,*mScrollDefaults,*mContainerDefaults,mLayoutDefaults,*mComboDefaults,*mTextDefaults,*mBrowserDefaults);
    if (!parse(state,xml,error)) return false;
    auto& declaration = *state.root;
    if (!resolveImages(tree,declaration,error)) return false;
    if (declaration.control && !resolveFont(*declaration.control,mResources,error)) return false;
    if (declaration.ownedBadge && declaration.ownedBadge->control && !resolveFont(*declaration.ownedBadge->control,mResources,error)) return false;
    if (!declaration.children.empty() || (declaration.ownedBadge && !declaration.ownedBadge->children.empty()))
    { error = "Native widget defaults cannot construct child widgets"; return false; }
    declaration.params.view.fromDeclaration = false;
    if (declaration.sliderControl)
    {
        auto defaults=std::make_shared<SliderControlDefaults>(*declaration.sliderControl);
        if (!resolveSliderControl(declaration,*defaults,tree,mResources,error)) return false;
        defaults->view=declaration.params;
        defaults->control=*declaration.control;
        mResources.sliderControl=std::move(defaults);
    }
    else if (declaration.progress)
    {
        mResources.progress=*declaration.progress;
    }
    else if (declaration.slider)
    {
        auto defaults=std::make_shared<SliderDefaults>(*declaration.slider);
        defaults->view=declaration.params;
        defaults->control=*declaration.control;
        mResources.slider=std::move(defaults);
    }
    else if (declaration.radioGroup)
    {
        mResources.radioView=declaration.params;
        mResources.radioControl=*declaration.control;
    }
    else if (declaration.radioItem)
    {
        auto defaults=std::make_shared<CheckBoxDefaults>(*declaration.checkBox);
        if (!resolveCheckBox(declaration,*defaults,tree,mResources,error)) return false;
        defaults->view=declaration.params;
        defaults->control=*declaration.control;
        mResources.radioItem=std::move(defaults);
    }
    else if (declaration.spinner)
    {
        auto defaults=std::make_shared<SpinnerDefaults>(*declaration.spinner);
        if (!resolveSpinner(declaration,*defaults,tree,mResources,error)) return false;
        defaults->view=declaration.params;
        defaults->control=*declaration.control;
        mResources.spinner=std::move(defaults);
    }
    else if (declaration.tabs)
    {
        auto defaults = std::make_shared<TabDefaults>(*declaration.tabs);
        defaults->panel.view = declaration.params;
        defaults->panel.control = *declaration.control;
        defaults->panel.panel = *declaration.panel;
        mResources.tabs = std::move(defaults);
    }
    else if (declaration.icon)
    {
        auto defaults = mIconDefaults;
        defaults.view = std::move(declaration.params);
        defaults.control = std::move(*declaration.control);
        defaults.icon = std::move(*declaration.icon);
        if (declaration.imageName) defaults.icon.image = tree.findImage(*declaration.imageName);
        mIconDefaults = std::move(defaults);
    }
    else if (declaration.badge)
    {
        auto defaults = mButtonDefaults.badge;
        defaults.view = std::move(declaration.params);
        defaults.control = std::move(*declaration.control);
        defaults.badge = std::move(*declaration.badge);
        if (declaration.badgeImage) defaults.badge.image = tree.findImage(*declaration.badgeImage);
        if (declaration.badgeBorderImage) defaults.badge.borderImage = tree.findImage(*declaration.badgeBorderImage);
        mButtonDefaults.badge = std::move(defaults);
    }
    else if (declaration.button)
    {
        auto defaults = mButtonDefaults;
        defaults.view = std::move(declaration.params);
        defaults.control = std::move(*declaration.control);
        defaults.button = std::move(*declaration.button);
        for (const auto& [attribute,name] : declaration.buttonImages)
            *buttonImage(defaults.button.images,attribute) = tree.findImage(name);
        defaults.button.defaultImages = defaults.button.images;
        if (declaration.ownedBadge)
        {
            auto& owned = *declaration.ownedBadge;
            BadgeDefaults badge;
            badge.view = std::move(owned.params);
            badge.control = std::move(*owned.control);
            badge.badge = std::move(*owned.badge);
            if (owned.badgeImage) badge.badge.image = tree.findImage(*owned.badgeImage);
            if (owned.badgeBorderImage) badge.badge.borderImage = tree.findImage(*owned.badgeBorderImage);
            defaults.providedBadge = std::move(badge);
        }
        if (declaration.tag=="scroll_column_header") mResources.scrollColumnHeader=std::make_shared<ButtonDefaults>(std::move(defaults));
        else mButtonDefaults = std::move(defaults);
    }
    else if (declaration.textEditor)
    {
        auto defaults=std::make_shared<TextEditorDefaults>(*declaration.textEditor);
        defaults->text.view=declaration.params;
        defaults->text.control=*declaration.control;
        defaults->text.text=*declaration.plainLabel;
        mResources.textEditor=std::move(defaults);
    }
    else if (declaration.plainLabel)
    {
        auto defaults = std::make_shared<TextDefaults>();
        defaults->view = declaration.params;
        defaults->control = *declaration.control;
        defaults->text = *declaration.plainLabel;
        mTextDefaults = std::move(defaults);
    }
    else if (declaration.combo)
    {
        auto defaults = std::make_shared<ComboDefaults>(*declaration.combo);
        if (!resolveCombo(declaration,*defaults,tree,mResources,error)) return false;
        defaults->view = declaration.params;
        defaults->control = *declaration.control;
        if (defaults->combo.flyout) mResources.flyout=std::move(defaults);
        else mComboDefaults = std::move(defaults);
    }
    else if (declaration.layoutStack)
    {
        mLayoutDefaults.view = declaration.params;
        mLayoutDefaults.stack = *declaration.layoutStack;
    }
    else if (declaration.scrollContainer)
    {
        auto defaults = std::make_shared<ScrollContainerDefaults>(*declaration.scrollContainer);
        defaults->view = declaration.params;
        defaults->control = *declaration.control;
        mContainerDefaults = std::move(defaults);
    }
    else if (declaration.scrollbar)
    {
        auto defaults = std::make_shared<ScrollbarDefaults>(*declaration.scrollbar);
        if (!resolveScrollbar(declaration,*defaults,tree,mResources,error)) return false;
        defaults->view = declaration.params;
        defaults->control = *declaration.control;
        mScrollDefaults = std::move(defaults);
    }
    else if (declaration.checkBox)
    {
        auto defaults = *declaration.checkBox;
        if (!resolveCheckBox(declaration,defaults,tree,mResources,error)) return false;
        defaults.view = declaration.params;
        defaults.control = *declaration.control;
        mCheckDefaults = std::move(defaults);
    }
    else if (declaration.scrollList)
    {
        auto defaults=std::make_shared<ScrollListDefaults>(*declaration.scrollList);
        if (!resolveScrollList(declaration,*defaults,tree,mResources,error)) return false;
        mResources.scrollList=std::move(defaults);
    }
    else if (declaration.texture)
    {
        auto defaults=std::make_shared<TextureDefaults>(*declaration.texture);
        if (!resolveTexture(declaration,*defaults,tree,error)) return false;
        mResources.texture=std::move(defaults);
    }
    else if (declaration.colorSwatch)
    {
        auto defaults=std::make_shared<ColorSwatchDefaults>(*declaration.colorSwatch);
        if (!resolveColorSwatch(declaration,*defaults,tree,error)) return false;
        mResources.colorSwatch=std::move(defaults);
    }
    else if (declaration.searchEditor)
    {
        auto defaults=std::make_shared<SearchEditorDefaults>(*declaration.searchEditor);
        if (!resolveSearchEditor(declaration,*defaults,tree,error)) return false;
        if (declaration.tag=="filter_editor") mResources.filterEditor=std::move(defaults);
        else mResources.searchEditor=std::move(defaults);
    }
    else if (declaration.lineEditor)
    {
        auto defaults = mLineDefaults;
        defaults.view = std::move(declaration.params);
        defaults.control = std::move(*declaration.control);
        defaults.editor = std::move(*declaration.lineEditor);
        defaults.borderProvided = declaration.lineBorderProvided;
        for (const auto& [attribute,name] : declaration.lineImages)
        {
            auto& image = attribute == "background_image" ? defaults.editor.background :
                attribute == "background_image_disabled" ? defaults.editor.disabledBackground : defaults.editor.focusedBackground;
            image = tree.findImage(name);
        }
        mLineDefaults = std::move(defaults);
    }
    else if (declaration.border)
    {
        mPanelDefaults.borderView = std::move(declaration.params);
        mPanelDefaults.panel.border = std::move(*declaration.border);
    }
    else if (declaration.browser)
    {
        auto defaults = std::make_shared<BrowserDefaults>();
        defaults->panel.view = declaration.params;
        defaults->panel.control = *declaration.control;
        defaults->panel.panel = *declaration.panel;
        defaults->browser = *declaration.browser;
        mBrowserDefaults = std::move(defaults);
    }
    else if (declaration.panel)
    {
        auto defaults = mPanelDefaults;
        defaults.view = std::move(declaration.params);
        defaults.control = std::move(*declaration.control);
        defaults.panel = std::move(*declaration.panel);
        if (declaration.panelOpaqueImage) defaults.panel.opaqueImage = tree.findImage(*declaration.panelOpaqueImage);
        if (declaration.panelTransparentImage) defaults.panel.transparentImage = tree.findImage(*declaration.panelTransparentImage);
        mPanelDefaults = std::move(defaults);
    }
    else mDefaults = std::move(declaration.params);
    return true;
}

bool LLVKWidgetFactory::loadListContents(const LLVKWidgetTree& tree,const std::string& filename,
    LLVKWidgetTree::ScrollListParams& contents,std::string& error) const
{
    error.clear();
    if (!mResources.skinFiles) { error="Native list contents require a skin source"; return false; }
    const auto files=mResources.skinFiles->read("xui",filename,LLVKSkinFiles::Policy::Current,error);
    if (!files) return false;
    std::vector<std::string_view> layers;
    for (const auto& file : *files) layers.push_back(file);
    const auto xml=LLVKXmlLayers::merge(layers,error);
    if (!xml) return false;
    struct ContentsParser
    {
        XML_Parser parser=XML_ParserCreate(nullptr);
        const LLVKWidgetTree& tree;
        const Resources& resources;
        LLVKWidgetTree::ScrollListParams contents;
        int depth=0;
        std::optional<LLVKWidgetTree::ListRow> row;
        std::string error;
        ContentsParser(const LLVKWidgetTree& widgets,const Resources& assets,const LLVKWidgetTree::ScrollListParams& initial)
            : tree(widgets),resources(assets),contents(initial) {}
        ContentsParser(const ContentsParser&) = delete;
        ContentsParser& operator=(const ContentsParser&) = delete;
        ~ContentsParser() { if (parser) XML_ParserFree(parser); }
        void fail(const char* message) { if (error.empty()) error=message; XML_StopParser(parser,XML_FALSE); }
        static void XMLCALL start(void* pointer,const char* tag,const char** attributes)
        {
            auto& state=*static_cast<ContentsParser*>(pointer);
            try
            {
                ++state.depth;
                std::map<std::string,std::string> fields;
                for (std::size_t index=0; attributes[index]; index+=2) fields.emplace(attributes[index],attributes[index+1]);
                if (state.depth==1)
                { if (std::string_view(tag)!="contents") state.fail("Native list contents root is invalid"); return; }
                if (state.depth==2 && std::string_view(tag)=="rows")
                {
                    if (state.contents.rows.size()>=10000) { state.fail("Native list row budget exceeded"); return; }
                    state.row.emplace(); state.row->value=fields.contains("value") ? LLSD(fields["value"]) : LLSD();
                    if (fields.contains("enabled") && !boolean(fields["enabled"],state.row->enabled)) state.fail("Invalid native row enabled flag");
                    return;
                }
                if (state.depth==2 && std::string_view(tag)=="columns")
                {
                    if (state.contents.columns.size()>=128) { state.fail("Native list column budget exceeded"); return; }
                    LLVKWidgetTree::ListColumn column; column.name=fields["name"]; column.label=fields["label"];
                    if (fields.contains("width") && !integer(fields["width"],column.width)) { state.fail("Invalid native column width"); return; }
                    if (fields.contains("relative_width"))
                    {
                        const auto& value=fields["relative_width"];
                        const auto parsed=std::from_chars(value.data(),value.data()+value.size(),column.relativeWidth);
                        if (parsed.ec!=std::errc() || parsed.ptr!=value.data()+value.size() || !std::isfinite(column.relativeWidth))
                        { state.fail("Invalid native relative column width"); return; }
                    }
                    state.contents.columns.push_back(std::move(column)); return;
                }
                if (state.depth!=3 || !state.row || std::string_view(tag)!="columns")
                { state.fail("Unsupported native list contents element"); return; }
                const auto found=std::find_if(state.contents.columns.begin(),state.contents.columns.end(),[&](const auto& column)
                { return column.name==fields["column"]; });
                if (found==state.contents.columns.end()) { state.fail("Native list cell refers to an unknown column"); return; }
                const auto index=static_cast<std::size_t>(found-state.contents.columns.begin());
                state.row->cells.resize(state.contents.columns.size()); state.row->styles.resize(state.contents.columns.size());
                auto& style=state.row->styles[index];
                const auto type=fields.contains("type") ? fields["type"] : "text";
                if (type=="text") style.type=LLVKWidgetTree::ListCellStyle::Type::Text;
                else if (type=="icon") style.type=LLVKWidgetTree::ListCellStyle::Type::Icon;
                else if (type=="icontext") style.type=LLVKWidgetTree::ListCellStyle::Type::IconText;
                else { state.fail("Unsupported native list cell type"); return; }
                state.row->cells[index]=type=="icontext" ? fields["label"] : type=="text" ? fields["value"] : "";
                if (type!="text")
                {
                    style.image=state.tree.findImage(fields["value"],state.error);
                    if (!style.image) { state.fail("Native list cell image is unavailable"); return; }
                }
                LLVKControl::Params control;
                control.fontRequest=state.resources.defaultFontRequest;
                if (fields.contains("font")) control.fontRequest->name=fields["font"];
                if (!resolveFont(control,state.resources,state.error)) { state.fail("Native list cell font is unavailable"); return; }
                style.font=control.font;
                if (fields.contains("halign") && !alignment(fields["halign"],style.alignment)) { state.fail("Invalid native cell alignment"); return; }
                if (fields.contains("color"))
                {
                    std::istringstream channels(fields["color"]);
                    LLVKColor::Value color;
                    for (auto& channel : color) if (!(channels>>channel) || !std::isfinite(channel)) { state.fail("Invalid native cell color"); return; }
                    style.imageColor=LLVKColor(color);
                }
                style.tooltip=fields["tool_tip"];
            }
            catch (...) { state.fail("Native list contents parsing failed"); }
        }
        static void XMLCALL end(void* pointer,const char*)
        {
            auto& state=*static_cast<ContentsParser*>(pointer);
            try
            {
                if (state.depth==2 && state.row) { state.contents.rows.push_back(std::move(*state.row)); state.row.reset(); }
                --state.depth;
            }
            catch (...) { state.fail("Native list contents publication failed"); }
        }
        static void XMLCALL doctype(void* pointer,const char*,const char*,const char*,int)
        { static_cast<ContentsParser*>(pointer)->fail("Native list contents DTD is not allowed"); }
    };
    const auto state=std::make_unique<ContentsParser>(tree,mResources,contents);
    if (!state->parser) { error="Native list contents parser allocation failed"; return false; }
    XML_SetUserData(state->parser,state.get()); XML_SetElementHandler(state->parser,ContentsParser::start,ContentsParser::end);
    XML_SetStartDoctypeDeclHandler(state->parser,ContentsParser::doctype);
    if (XML_Parse(state->parser,xml->data(),static_cast<int>(xml->size()),XML_TRUE)!=XML_STATUS_OK || !state->error.empty())
    { error=state->error.empty() ? "Invalid native list contents XML" : state->error; return false; }
    contents=std::move(state->contents);
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetFactory::construct(LLVKWidgetTree& tree,
    std::string_view xml, LLVKWidgetTree::Id parent, std::string& error) const
{
    error.clear();
    if ((parent && !tree.get(parent)) || xml.size() > 4 * 1024 * 1024)
    { error = "Invalid native widget parent or oversized declaration"; return std::nullopt; }
    Parser state(mDefaults,mIconDefaults,mButtonDefaults,mCallbacks,mResources,mPanelDefaults,mLineDefaults,mCheckDefaults,*mScrollDefaults,*mContainerDefaults,mLayoutDefaults,*mComboDefaults,*mTextDefaults,*mBrowserDefaults);
    if (!parse(state,xml,error)) return std::nullopt;
    const auto* owner = tree.get(parent);
    const auto previousChildren = owner ? owner->children : std::vector<LLVKWidgetTree::Id>();
    const auto previousTabGroup = owner ? owner->lastTabGroup : 0;
    const auto restoreFactoryMetadata = [&]
    {
        const auto* current = tree.get(parent);
        if (current && current->children == previousChildren)
            tree.mNodes.at(parent).lastTabGroup = previousTabGroup;
    };
    try
    {
        BuildState buildState;
        buildState.bytes = xml.size();
        auto root = build(tree,*state.root,parent,parent,error,state,buildState);
        if (!root) restoreFactoryMetadata();
        return root;
    }
    catch (...)
    {
        restoreFactoryMetadata();
        throw;
    }
}
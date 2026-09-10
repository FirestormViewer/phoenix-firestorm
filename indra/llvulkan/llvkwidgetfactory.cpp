#include "llvkwidgetfactory.h"
#include "llstring.h"

#if __has_include(<expat.h>)
#include <expat.h>
#else
#include <expat/expat.h>
#endif

#include <charconv>
#include <cmath>
#include <exception>
#include <memory>
#include <utility>

namespace
{
    struct Declaration
    {
        std::string tag;
        LLVKWidgetFactory::Defaults params;
        std::optional<LLVKControl::Params> control;
        std::optional<LLVKIcon::Params> icon;
        std::optional<LLVKButton::Params> button;
        std::optional<LLVKBadge::Params> badge;
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

    struct Parser
    {
        XML_Parser parser = nullptr;
        const LLVKWidgetFactory::Defaults& defaults;
        const LLVKWidgetFactory::IconDefaults& iconDefaults;
        const LLVKWidgetFactory::ButtonDefaults& buttonDefaults;
        const LLVKWidgetFactory::Callbacks& callbacks;
        const LLVKWidgetFactory::Resources& resources;
        std::unique_ptr<Declaration> root;
        std::vector<Declaration*> stack;
        std::string error;
        std::exception_ptr exception;
        std::size_t nodes = 0;
        bool callbackElement = false;
        Parser(const LLVKWidgetFactory::Defaults& source, const LLVKWidgetFactory::IconDefaults& icons,
                    const LLVKWidgetFactory::ButtonDefaults& buttons, const LLVKWidgetFactory::Callbacks& handlers,
                    const LLVKWidgetFactory::Resources& assets)
                : defaults(source), iconDefaults(icons), buttonDefaults(buttons), callbacks(handlers), resources(assets) {}
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
            for (auto& channel : value)
            {
                const auto start = text.find_first_not_of(" \t\r\n");
                if (start == std::string_view::npos) return false;
                text.remove_prefix(start);
                const auto result = std::from_chars(text.data(),text.data()+text.size(),channel);
                if (result.ec != std::errc() || !std::isfinite(channel)) return false;
                text.remove_prefix(result.ptr-text.data());
            }
            if (text.find_first_not_of(" \t\r\n") != std::string_view::npos) return false;
            output = LLVKColor(value);
            return true;
        }

        bool attribute(Declaration& declaration, std::string_view name, std::string_view text)
        {
            auto& view = declaration.params.view;
            auto& geometry = declaration.params.geometry;
            if (name == "font" && declaration.control)
            {
                const auto found = resources.fonts.find(std::string(text));
                if (found == resources.fonts.end() || !found->second) return false;
                declaration.control->font = found->second;
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
                    if (control.enabledSetting) return false;
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
                    if (control.enabledSetting) return false;
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
            if (separator == std::string_view::npos || !declaration.control) return false;
            const auto prefix = tag.substr(0,separator);
            if ((declaration.button && prefix != "button") || (declaration.icon && prefix != "icon") ||
                (declaration.badge && prefix != "badge")) return false;
            const auto name = tag.substr(separator+1);
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
            else if (name == "init_callback") action = &control.init;
            else if (name == "mouseenter_callback") action = &control.mouseEnter;
            else if (name == "mouseleave_callback") action = &control.mouseLeave;
            else if (name == "validate_callback") predicate = &control.validate;
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
                const auto found = callbacks.actions.find(function);
                if (found == callbacks.actions.end()) return false;
                *action = {found->second,parameter};
                return true;
            }
            if (predicate)
            {
                const auto found = callbacks.predicates.find(function);
                if (found == callbacks.predicates.end()) return false;
                *predicate = {found->second,parameter};
                return true;
            }
            return false;
        }

        static void XMLCALL start(void* pointer, const XML_Char* tag, const XML_Char** attributes)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&]
            {
                if (state.callbackElement) { state.reject("Nested native callback declarations are unsupported"); return; }
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
                if (std::string_view(tag).find('.') != std::string_view::npos)
                {
                    if (state.stack.empty() || !state.callback(*state.stack.back(),tag,attributes))
                    { state.reject("Unresolved or unsupported native callback: " + std::string(tag)); return; }
                    state.callbackElement = true;
                    return;
                }
                const bool icon = std::string_view(tag) == "icon";
                const bool button = std::string_view(tag) == "button";
                const bool badge = std::string_view(tag) == "badge";
                if (std::string_view(tag) != "view" && !icon && !button && !badge)
                { state.reject("Native constructor not implemented for tag: " + std::string(tag)); return; }
                if (++state.nodes > LLVKWidgetTree::maximumNodes || state.stack.size() >= LLVKWidgetTree::maximumDepth)
                { state.reject("Native widget declaration exceeds node/depth limits"); return; }
                auto declaration = std::make_unique<Declaration>();
                declaration->tag = tag;
                declaration->params = icon ? state.iconDefaults.view : button ? state.buttonDefaults.view :
                                      badge ? state.buttonDefaults.badge.view : state.defaults;
                if (icon)
                {
                    declaration->control = state.iconDefaults.control;
                    declaration->icon = state.iconDefaults.icon;
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
                    if (!state.attribute(*declaration,attributes[index],attributes[index+1]))
                    { state.reject("Unsupported or invalid native view attribute: " + std::string(attributes[index])); return; }
                }
                auto* current = declaration.get();
                if (state.stack.empty()) state.root = std::move(declaration);
                else state.stack.back()->children.push_back(std::move(declaration));
                state.stack.push_back(current);
            });
        }

        static void XMLCALL end(void* pointer, const XML_Char*)
        {
            auto& state = *static_cast<Parser*>(pointer);
            if (state.callbackElement) { state.callbackElement = false; return; }
            if (!state.stack.empty()) state.stack.pop_back();
        }

        static void XMLCALL text(void* pointer, const XML_Char* text, int length)
        {
            auto& state = *static_cast<Parser*>(pointer);
            state.guarded([&]
            {
                if (std::string_view(text,length).find_first_not_of(" \t\r\n") != std::string_view::npos)
                    state.reject("Native view/icon does not accept text content");
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

    std::optional<LLVKWidgetTree::Id> build(LLVKWidgetTree& tree, const Declaration& declaration,
                                          LLVKWidgetTree::Id layoutParent,
                                          LLVKWidgetTree::Id owningParent, std::string& error)
    {
        auto params = declaration.params.view;
        const auto* parent = tree.get(layoutParent);
        const auto parentLayout = parent ? parent->params.layout : std::string();
        auto rect = declaration.params.geometry.apply(tree,layoutParent,parentLayout,error);
        if (!rect) return std::nullopt;
        params.rect = *rect;
        params.layout = declaration.params.geometry.layout.empty() ? parentLayout : declaration.params.geometry.layout;
        auto icon = declaration.icon;
        if (icon && declaration.imageName) icon->image = tree.findImage(*declaration.imageName);
        auto button = declaration.button;
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
        const auto id = button ? tree.createButton(params,*declaration.control,*button,owningParent,error,construction) :
                       badge ? tree.createBadge(params,*declaration.control,*badge,0,owningParent,error) :
                       icon ? tree.createIcon(params,*declaration.control,*icon,owningParent,error) :
                       tree.create(params,owningParent,error);
        if (!id) return std::nullopt;
        try
        {
            for (const auto& child : declaration.children)
            {
                if (!build(tree,*child,*id,*id,error))
                {
                    std::string cleanupError;
                    tree.erase(*id,cleanupError);
                    return std::nullopt;
                }
            }
            if ((button && !tree.postBuildButton(*id,error)) || ((icon || badge) && !tree.postBuildControl(*id)))
            {
                if (error.empty()) error = "Native control no longer exists at post-build";
                if (tree.get(*id)) { std::string cleanupError; tree.eraseControl(*id,cleanupError); }
                return std::nullopt;
            }
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

LLVKWidgetFactory::LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults)
    : mDefaults(std::move(defaults)), mIconDefaults(std::move(iconDefaults)) {}

LLVKWidgetFactory::LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults,
        ButtonDefaults buttonDefaults, Callbacks callbacks, Resources resources)
        : mDefaults(std::move(defaults)), mIconDefaults(std::move(iconDefaults)),
            mButtonDefaults(std::move(buttonDefaults)), mCallbacks(std::move(callbacks)), mResources(std::move(resources)) {}

std::optional<LLVKWidgetTree::Id> LLVKWidgetFactory::construct(LLVKWidgetTree& tree,
    const LLVKWidgetTree::Params& params, LLVKWidgetTree::Id parent, std::string& error) const
{
    return tree.create(params,parent,error);
}

bool LLVKWidgetFactory::loadDefaults(const LLVKWidgetTree& tree, std::string_view xml, std::string& error)
{
    error.clear();
    Parser state(mDefaults,mIconDefaults,mButtonDefaults,mCallbacks,mResources);
    if (!parse(state,xml,error)) return false;
    auto& declaration = *state.root;
    if (!declaration.children.empty() || (declaration.ownedBadge && !declaration.ownedBadge->children.empty()))
    { error = "Native widget defaults cannot construct child widgets"; return false; }
    declaration.params.view.fromDeclaration = false;
    if (declaration.icon)
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
        mButtonDefaults = std::move(defaults);
    }
    else mDefaults = std::move(declaration.params);
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetFactory::construct(LLVKWidgetTree& tree,
    std::string_view xml, LLVKWidgetTree::Id parent, std::string& error) const
{
    error.clear();
    if ((parent && !tree.get(parent)) || xml.size() > 4 * 1024 * 1024)
    { error = "Invalid native widget parent or oversized declaration"; return std::nullopt; }
    Parser state(mDefaults,mIconDefaults,mButtonDefaults,mCallbacks,mResources);
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
        auto root = build(tree,*state.root,parent,parent,error);
        if (!root) restoreFactoryMetadata();
        return root;
    }
    catch (...)
    {
        restoreFactoryMetadata();
        throw;
    }
}
#include "llvkloginmenu.h"
#include "llstring.h"
#include <expat/expat.h>
#include <algorithm>
#include <cmath>

namespace
{
    std::string shortcutLabel(std::string value)
    {
        LLStringUtil::replaceString(value,"control|","Ctrl+");
        LLStringUtil::replaceString(value,"shift|","Shift+");
        LLStringUtil::replaceString(value,"alt|","Alt+");
        return value;
    }
}

std::unique_ptr<LLVKLoginMenu> LLVKLoginMenu::create(std::string_view xml,std::shared_ptr<LLVKFont> font,
    std::shared_ptr<LLVKColorTable> colors,LLVKLabel::Context labels,bool debug,std::string& error)
{
    error.clear();
    if (!font || xml.size() > 1024*1024) { error = "Invalid native login menu resources"; return nullptr; }
    auto menu = std::make_unique<LLVKLoginMenu>();
    menu->mFont = std::move(font); menu->mColors = std::move(colors); menu->mLabels = std::move(labels);
    struct Parser
    {
        LLVKLoginMenu& menu;
        XML_Parser parser;
        std::vector<std::optional<std::size_t>> stack;
        bool failed = false, root = false;
        static void XMLCALL start(void* data,const char* tag,const char** attributes)
        {
            auto& state = *static_cast<Parser*>(data);
            try
            {
                const std::string_view name(tag);
                if (state.stack.size() >= 64 || state.menu.mItems.size() >= 2048) throw std::runtime_error("menu limit");
                if (state.stack.empty())
                {
                    if (name != "menu_bar") throw std::runtime_error("menu root");
                    state.root = true; state.stack.push_back({}); return;
                }
                const bool item = name == "menu" || name == "menu_item_call" || name == "menu_item_check" || name == "menu_item_separator";
                if (item)
                {
                    Item entry;
                    entry.branch = name == "menu"; entry.separator = name == "menu_item_separator";
                    for (std::size_t index = 0; attributes[index]; index += 2)
                    {
                        const std::string_view attribute(attributes[index]);
                        const std::string value(attributes[index+1]);
                        if (attribute == "name") entry.name = value;
                        else if (attribute == "label") entry.label = value;
                        else if (attribute == "shortcut") entry.shortcut = value;
                        else if (attribute == "visible") entry.visible = value != "false" && value != "0";
                    }
                    const auto index = state.menu.mItems.size();
                    state.menu.mItems.push_back(std::move(entry));
                    if (state.stack.back()) state.menu.mItems[*state.stack.back()].children.push_back(index);
                    else state.menu.mRoots.push_back(index);
                    state.stack.push_back(index);
                }
                else
                {
                    if ((name == "on_click" || name.ends_with(".on_click")) && state.stack.back())
                        for (std::size_t index = 0; attributes[index]; index += 2)
                        {
                            auto& entry = state.menu.mItems[*state.stack.back()];
                            if (std::string_view(attributes[index]) == "function") entry.action = attributes[index+1];
                            else if (std::string_view(attributes[index]) == "parameter") entry.parameter = attributes[index+1];
                        }
                    state.stack.push_back({});
                }
            }
            catch (...) { state.failed = true; XML_StopParser(state.parser,XML_FALSE); }
        }
        static void XMLCALL end(void* data,const char*) { auto& state = *static_cast<Parser*>(data); if (!state.stack.empty()) state.stack.pop_back(); }
        static void XMLCALL doctype(void* data,const char*,const char*,const char*,int)
        { auto& state = *static_cast<Parser*>(data); state.failed = true; XML_StopParser(state.parser,XML_FALSE); }
    };
    Parser state{*menu,XML_ParserCreate(nullptr)};
    if (!state.parser) { error = "Native menu parser allocation failed"; return nullptr; }
    XML_SetUserData(state.parser,&state);
    XML_SetElementHandler(state.parser,Parser::start,Parser::end);
    XML_SetStartDoctypeDeclHandler(state.parser,Parser::doctype);
    const auto result = XML_Parse(state.parser,xml.data(),static_cast<int>(xml.size()),XML_TRUE);
    XML_ParserFree(state.parser);
    if (result != XML_STATUS_OK || state.failed || !state.root)
    { error = "Invalid native login menu declaration"; return nullptr; }
    for (auto& item : menu->mItems)
    {
        if (item.name == "Debug") item.visible = debug;
        LLVKLabel label; label.assign(item.label); label.setArgument("[APP_NAME]","Vulkanstorm");
        item.label = label.resolve(menu->mLabels);
    }
    return menu;
}

void LLVKLoginMenu::bind(std::string action,Handler handler) { mHandlers.insert_or_assign(std::move(action),std::move(handler)); }
void LLVKLoginMenu::bindItem(std::string action,std::string parameter,Handler handler)
{ mItemHandlers.insert_or_assign({std::move(action),std::move(parameter)},std::move(handler)); }
void LLVKLoginMenu::setVisible(std::string_view name,bool visible)
{ for (auto& item : mItems) if (item.name == name) item.visible = visible; }
bool LLVKLoginMenu::shortcut(std::string key,bool control,bool shift,bool alt)
{
    std::string shortcut;
    if (control) shortcut += "control|";
    if (alt) shortcut += "alt|";
    if (shift) shortcut += "shift|";
    shortcut += key;
    const auto visit = [&](const auto& self,std::size_t item) -> bool
    {
        if (!mItems[item].visible) return false;
        if (mItems[item].shortcut == shortcut && enabled(item) && !mItems[item].branch)
        { activate(item,0); return true; }
        for (auto child : mItems[item].children) if (self(self,child)) return true;
        return false;
    };
    for (auto root : mRoots) if (visit(visit,root)) return true;
    return false;
}
bool LLVKLoginMenu::enabled(std::size_t item) const
{
    const auto& entry = mItems[item];
    const auto specific = mItemHandlers.find({entry.action,entry.parameter});
    const auto handler = mHandlers.find(entry.action);
    return entry.branch || (!entry.separator && ((specific != mItemHandlers.end() && bool(specific->second)) ||
        (handler != mHandlers.end() && bool(handler->second))));
}
void LLVKLoginMenu::dismiss() { mOpen.clear(); mHovered.reset(); mHits.clear(); mPressed = false; }
void LLVKLoginMenu::activate(std::size_t item,std::size_t level)
{
    if (!enabled(item)) return;
    if (mItems[item].branch)
    { mOpen.resize(std::min(level,mOpen.size())); mOpen.push_back(item); mHovered = item; return; }
    const auto entry = mItems[item];
    const auto specific = mItemHandlers.find({entry.action,entry.parameter});
    const auto handler = specific != mItemHandlers.end() ? specific->second : mHandlers.at(entry.action);
    dismiss();
    handler(entry.action,entry.parameter);
}

bool LLVKLoginMenu::pointer(const LLVKWidgetTree::PointerEvent& event)
{
    using Kind = LLVKWidgetTree::PointerKind;
    auto hit = std::find_if(mHits.rbegin(),mHits.rend(),[&](const Hit& hit)
        { return event.x >= hit.rect.left && event.x < hit.rect.right && event.y >= hit.rect.bottom && event.y < hit.rect.top; });
    if (hit != mHits.rend())
    {
        const auto selected = *hit;
        if (event.kind == Kind::LeftDown) mPressed = true;
        if (event.kind == Kind::Hover)
        {
            mHovered = selected.item;
            if (open() && mItems[selected.item].branch && (mOpen.size() <= selected.level || mOpen[selected.level] != selected.item))
                activate(selected.item,selected.level);
        }
        else if (event.kind == Kind::LeftDown && mItems[selected.item].branch)
        { if (selected.level == 0 && !mOpen.empty() && mOpen[0] == selected.item) dismiss(); else activate(selected.item,selected.level); }
        else if (event.kind == Kind::LeftUp)
        { const bool pressed = mPressed; mPressed = false; if (pressed && !mItems[selected.item].branch) activate(selected.item,selected.level); }
        return true;
    }
    if (open())
    { if (event.kind == Kind::LeftDown || event.kind == Kind::RightDown) dismiss(); return true; }
    if (event.kind == Kind::Hover) mHovered.reset();
    return event.y >= mViewport.top-18 && event.y < mViewport.top;
}

bool LLVKLoginMenu::key(Key key)
{
    if (key == Key::Activate)
    { for (auto item : mRoots) if (mItems[item].visible && mItems[item].branch) { activate(item,0); return true; } return false; }
    if (!open()) return false;
    if (key == Key::Escape) { dismiss(); return true; }
    if (key == Key::Left || key == Key::Right)
    {
        std::vector<std::size_t> roots;
        for (auto item : mRoots) if (mItems[item].visible && mItems[item].branch) roots.push_back(item);
        const auto position = std::find(roots.begin(),roots.end(),mOpen.front())-roots.begin();
        activate(roots[(position+roots.size()+(key == Key::Left ? -1 : 1))%roots.size()],0); return true;
    }
    if (key == Key::Up || key == Key::Down)
    {
        std::vector<std::size_t> rows;
        for (auto item : mItems[mOpen.back()].children) if (mItems[item].visible && enabled(item)) rows.push_back(item);
        if (rows.empty()) return true;
        auto found = mHovered ? std::find(rows.begin(),rows.end(),*mHovered) : rows.end();
        const auto position = found == rows.end() ? (key == Key::Down ? rows.size()-1 : 0) : std::size_t(found-rows.begin());
        mHovered = rows[(position+rows.size()+(key == Key::Up ? -1 : 1))%rows.size()]; return true;
    }
    if (key == Key::Return && mHovered && *mHovered != mOpen.back()) activate(*mHovered,mOpen.size());
    return true;
}

bool LLVKLoginMenu::paint(LLVKWidgetPaint& output,LLVKWidgetTree::Rect viewport,std::string& error)
{
    error.clear(); mViewport = viewport; mHits.clear();
    using Rect = LLVKWidgetTree::Rect;
    if (viewport.right-viewport.left < 100 || viewport.top-viewport.bottom < 18) { error = "Native menu viewport too small"; return false; }
    const auto color = [&](const char* name,LLVKColor::Value fallback)
    { const auto found = mColors ? mColors->find(name) : std::nullopt; return found ? found->get() : fallback; };
    const auto background = color("MenuDefaultBgColor",{0.15f,0.15f,0.15f,1});
    const auto normal = color("MenuItemEnabledColor",{1,1,1,1});
    const auto disabled = color("MenuItemDisabledColor",{0.5f,0.5f,0.5f,1});
    const auto highlight = color("MenuItemHighlightBgColor",{0.3f,0.3f,0.3f,1});
    const auto foreground = color("MenuItemHighlightFgColor",{1,1,1,1});
    const auto solid = [&](std::size_t item,Rect rect,LLVKColor::Value tint)
    { output.commands.push_back({UINT64_MAX-item,rect,viewport,tint}); };
    const auto text = [&](std::size_t item,const std::string& label,float x,float y,LLVKColor::Value tint,LLVKFont::HorizontalAlign align) -> bool
    {
        const auto wide = utf8str_to_wstring(label); const std::u32string value(wide.begin(),wide.end());
        LLVKFont::LineOptions options; options.x = x; options.y = y; options.vertical = LLVKFont::VerticalAlign::Bottom; options.horizontal = align;
        auto line = mFont->layoutLine(value,0,value.size(),options,error);
        if (!line) return false;
        output.commands.push_back({UINT64_MAX-item,{},viewport,tint,{},std::move(line)}); return true;
    };
    const auto measure = [&](const std::string& value) -> int
    {
        const auto wide = utf8str_to_wstring(value); const std::u32string string(wide.begin(),wide.end());
        const auto measured = mFont->measureRun(string,0,string.size(),1,true,false,error);
        return measured ? static_cast<int>(std::floor(measured->width+0.5f)) : 0;
    };
    solid(mItems.size(),{viewport.left,viewport.top-18,viewport.right,viewport.top},color("MenuBarBgColor",background));
    int left = viewport.left;
    for (auto item : mRoots)
    {
        if (!mItems[item].visible || !mItems[item].branch) continue;
        const int width = measure(mItems[item].label)+25;
        if (!error.empty()) return false;
        const Rect rect{left,viewport.top-18,left+width,viewport.top};
        mHits.push_back({item,0,rect});
        const bool selected = (!mOpen.empty() && mOpen[0] == item) || mHovered == item;
        if (selected) solid(item,rect,highlight);
        if (!text(item,mItems[item].label,left+width/2.f,rect.bottom+1.f,selected ? foreground : normal,LLVKFont::HorizontalAlign::Center)) return false;
        left += width;
    }
    const int rowHeight = static_cast<int>(mFont->metrics().lineHeight)+4;
    for (std::size_t level = 0; level < mOpen.size(); ++level)
    {
        const auto parent = std::find_if(mHits.begin(),mHits.end(),[&](const Hit& hit) { return hit.item == mOpen[level]; });
        if (parent == mHits.end()) break;
        const auto parentRect = parent->rect;
        int width = 80, height = 4;
        for (auto item : mItems[mOpen[level]].children) if (mItems[item].visible)
        { width = std::max(width,measure(mItems[item].label)+measure(shortcutLabel(mItems[item].shortcut))+65); height += mItems[item].separator ? 8 : rowHeight; }
        if (!error.empty()) return false;
        width = std::min(width,viewport.right-viewport.left);
        left = std::clamp(level ? parentRect.right : parentRect.left,viewport.left,viewport.right-width);
        int top = level ? parentRect.top : parentRect.bottom;
        top = std::min(viewport.top,std::max(top,viewport.bottom+height));
        solid(mOpen[level],{left,top-height,left+width,top},background);
        top -= 2;
        for (auto item : mItems[mOpen[level]].children)
        {
            const auto& entry = mItems[item]; if (!entry.visible) continue;
            if (entry.separator) { solid(item,{left+3,top-4,left+width-3,top-3},disabled); top -= 8; continue; }
            const Rect rect{left,top-rowHeight,left+width,top}; mHits.push_back({item,level+1,rect});
            const bool selected = mHovered == item && enabled(item);
            if (selected) solid(item,rect,highlight);
            const auto tint = enabled(item) ? (selected ? foreground : normal) : disabled;
            if (!text(item,entry.label,left+18.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Left)) return false;
            if (!entry.shortcut.empty() && !text(item,shortcutLabel(entry.shortcut),left+width-7.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            if (entry.branch && !text(item,">",left+width-7.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            top -= rowHeight;
        }
    }
    return true;
}
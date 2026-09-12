#include "llvkmenu.h"
#include "llstring.h"
#include <expat/expat.h>
#include <algorithm>
#include <cmath>
#include <boost/tokenizer.hpp>

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

std::unique_ptr<LLVKMenu> LLVKMenu::create(std::string_view xml,std::shared_ptr<LLVKFont> font,
    std::shared_ptr<LLVKColorTable> colors,LLVKLabel::Context labels,bool debug,std::string& error)
{
    error.clear();
    if (!font || xml.size() > 1024*1024) { error = "Invalid native login menu resources"; return nullptr; }
    auto menu = std::make_unique<LLVKMenu>();
    menu->mFont = std::move(font); menu->mColors = std::move(colors); menu->mLabels = std::move(labels);
    struct Parser
    {
        LLVKMenu& menu;
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
                    if (name != "menu_bar" && name!="toggleable_menu" && name!="context_menu") throw std::runtime_error("menu root");
                    state.root = true; state.stack.push_back({}); return;
                }
                const bool item = name == "menu" || name == "menu_item_call" || name == "menu_item_check" || name == "menu_item_separator";
                if (item)
                {
                    Item entry;
                    entry.branch = name == "menu"; entry.separator = name == "menu_item_separator";
                    entry.checkable=name=="menu_item_check";
                    for (std::size_t index = 0; attributes[index]; index += 2)
                    {
                        const std::string_view attribute(attributes[index]);
                        const std::string value(attributes[index+1]);
                        if (attribute == "name") entry.name = value;
                        else if (attribute == "label") entry.label = value;
                        else if (attribute == "shortcut") entry.shortcut = value;
                        else if (attribute == "visible") entry.visible = value != "false" && value != "0";
                        else if (attribute == "enabled") entry.enabled = value != "false" && value != "0";
                        else if (attribute == "create_jump_keys") entry.createJumpKeys = value == "true" || value == "1";
                        else if (attribute == "jump_key" && value.size()==1)
                            entry.jumpKey=static_cast<unsigned char>(LLStringOps::toUpper(value.front()));
                    }
                    const auto index = state.menu.mItems.size();
                    state.menu.mItems.push_back(std::move(entry));
                    if (state.stack.back()) state.menu.mItems[*state.stack.back()].children.push_back(index);
                    else state.menu.mRoots.push_back(index);
                    state.stack.push_back(index);
                }
                else
                {
                    if ((name == "on_click" || name.ends_with(".on_click") || name=="on_check" || name.ends_with(".on_check") ||
                        name=="on_enable" || name.ends_with(".on_enable") || name=="on_visible" || name.ends_with(".on_visible")) && state.stack.back())
                        for (std::size_t index = 0; attributes[index]; index += 2)
                        {
                            auto& entry = state.menu.mItems[*state.stack.back()];
                            auto* action=&entry.action; auto* parameter=&entry.parameter;
                            if (name=="on_check" || name.ends_with(".on_check")) { action=&entry.checkAction; parameter=&entry.checkParameter; }
                            else if (name=="on_enable" || name.ends_with(".on_enable")) { action=&entry.enableAction; parameter=&entry.enableParameter; }
                            else if (name=="on_visible" || name.ends_with(".on_visible")) { action=&entry.visibleAction; parameter=&entry.visibleParameter; }
                            if (std::string_view(attributes[index]) == "function") *action = attributes[index+1];
                            else if (std::string_view(attributes[index]) == "parameter") *parameter = attributes[index+1];
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
        if (!item.branch && !item.separator && item.action.empty()) item.invoke=[] {};
    }
    for (const auto& item : menu->mItems)
        if (item.createJumpKeys) menu->assignJumpKeys(item.children);
    return menu;
}

std::optional<int> LLVKMenu::barWidth(std::string& error) const
{
    int width=0;
    for (const auto root : mRoots)
    {
        if (!itemVisible(root) || !mItems[root].branch) continue;
        const auto wide=utf8str_to_wstring(mItems[root].label);
        const std::u32string label(wide.begin(),wide.end());
        const auto measured=mFont->measureRun(label,0,label.size(),1,true,false,error);
        if (!measured) return std::nullopt;
        if (measured->width>INT32_MAX-width-25) { error="Native menu bar width overflows"; return std::nullopt; }
        width+=static_cast<int>(std::floor(measured->width+.5f))+25;
    }
    return width;
}

void LLVKMenu::assignJumpKeys(const std::vector<std::size_t>& siblings)
{
    using Tokens=boost::tokenizer<boost::char_separator<char>>;
    const boost::char_separator<char> separator(" ");
    std::set<std::string> unique,shared;
    std::set<unsigned char> assigned;
    for (const auto index : siblings)
    {
        auto label=mItems[index].label; LLStringUtil::toUpper(label);
        for (const auto& word : Tokens(label,separator))
            if (!unique.insert(word).second) shared.insert(word);
        auto& key=mItems[index].jumpKey;
        if (key && !assigned.insert(key).second) key=0;
    }
    for (const auto index : siblings)
    {
        auto& item=mItems[index];
        if (item.jumpKey || item.separator) continue;
        auto label=item.label; LLStringUtil::toUpper(label);
        for (const auto& word : Tokens(label,separator))
        {
            if (shared.contains(word)) continue;
            for (const unsigned char candidate : word)
                if ((candidate>='0' && candidate<='9') ||
                    (candidate>='A' && candidate<='Z' && !assigned.contains(candidate)))
                { item.jumpKey=candidate; assigned.insert(candidate); break; }
            if (item.jumpKey) break;
        }
    }
}

bool LLVKMenu::character(char32_t character)
{
    if (!open()) return false;
    if (std::any_of(mOpen.begin(),mOpen.end(),[this](auto item) { return !enabled(item); }))
    { dismiss(); return false; }
    mKeyboardMode=true;
    if (character>127) return true;
    const auto key=static_cast<unsigned char>(LLStringOps::toUpper(static_cast<char>(character)));
    for (const auto item : mItems[mOpen.back()].children)
        if (mItems[item].jumpKey==key && key)
        {
            if (enabled(item)) activate(item,mOpen.size());
            break;
        }
    return true;
}

void LLVKMenu::bind(std::string action,Handler handler) { mHandlers.insert_or_assign(std::move(action),std::move(handler)); }
void LLVKMenu::bindItem(std::string action,std::string parameter,Handler handler)
{ mItemHandlers.insert_or_assign({std::move(action),std::move(parameter)},std::move(handler)); }
void LLVKMenu::bindPredicate(std::string action,Predicate predicate)
{ mPredicates.insert_or_assign(std::move(action),std::move(predicate)); }
bool LLVKMenu::itemChecked(std::size_t item) const
{
    if (item>=mItems.size() || !mItems[item].checkable) return false;
    const auto& entry=mItems[item];
    const auto predicate=mPredicates.find(entry.checkAction);
    return predicate!=mPredicates.end() && predicate->second ? predicate->second(entry.checkParameter) : entry.checked;
}
void LLVKMenu::setVisible(std::string_view name,bool visible)
{ for (auto& item : mItems) if (item.name == name) item.visible = visible; }
bool LLVKMenu::itemVisible(std::size_t item) const
{
    if (item>=mItems.size() || !mItems[item].visible) return false;
    const auto& entry=mItems[item];
    const auto predicate=mPredicates.find(entry.visibleAction);
    return predicate==mPredicates.end() || !predicate->second || predicate->second(entry.visibleParameter);
}
bool LLVKMenu::shortcut(std::string key,bool control,bool shift,bool alt)
{
    std::string shortcut;
    if (control) shortcut += "control|";
    if (alt) shortcut += "alt|";
    if (shift) shortcut += "shift|";
    shortcut += key;
    const auto visit = [&](const auto& self,std::size_t item) -> bool
    {
        if (!itemVisible(item) || !enabled(item)) return false;
        if (mItems[item].shortcut == shortcut && enabled(item) && !mItems[item].branch)
        { activate(item,0); return true; }
        for (auto child : mItems[item].children) if (self(self,child)) return true;
        return false;
    };
    for (auto root : mRoots) if (visit(visit,root)) return true;
    return false;
}
bool LLVKMenu::enabled(std::size_t item) const
{
    if (!itemVisible(item) || !mItems[item].enabled) return false;
    const auto& entry = mItems[item];
    const auto predicate=mPredicates.find(entry.enableAction);
    if (predicate!=mPredicates.end() && predicate->second && !predicate->second(entry.enableParameter)) return false;
    const auto specific = mItemHandlers.find({entry.action,entry.parameter});
    const auto handler = mHandlers.find(entry.action);
    return entry.branch || (!entry.separator && (bool(entry.invoke) || (specific != mItemHandlers.end() && bool(specific->second)) ||
        (handler != mHandlers.end() && bool(handler->second))));
}
bool LLVKMenu::showContext(std::vector<Item> items,int x,int y,std::string& error)
{
    error.clear();
    if (items.empty() || items.size()>128) { error="Invalid native context menu size"; return false; }
    for (const auto& item : items)
        if (item.branch || !item.children.empty()) { error="Native context submenu construction is not implemented"; return false; }
    dismiss();
    mContextRoot=mItems.size();
    Item root; root.branch=true; root.name="native_context_menu";
    for (std::size_t index=0; index<items.size(); ++index) root.children.push_back(*mContextRoot+1+index);
    mItems.push_back(std::move(root));
    for (auto& item : items) mItems.push_back(std::move(item));
    mContextAnchor={x,y,x,y}; mOpen.push_back(*mContextRoot);
    return true;
}

bool LLVKMenu::showPopup(std::vector<Item> items,LLVKWidgetTree::Rect anchor,const std::string& position,
    std::function<void()> dismissed,std::string& error)
{
    if (!showContext(std::move(items),anchor.left,anchor.bottom,error)) return false;
    mContextAnchor=anchor; mPopupPosition=position; mDismissed=std::move(dismissed);
    return true;
}

void LLVKMenu::dismiss()
{
    mOpen.clear(); mHovered.reset(); mHits.clear(); mPressed=false; mKeyboardMode=false;
    if (mContextRoot) { mItems.resize(*mContextRoot); mContextRoot.reset(); }
    mPopupPosition.clear();
    auto callback=std::move(mDismissed); mDismissed={};
    if (callback) callback();
}
void LLVKMenu::activate(std::size_t item,std::size_t level)
{
    if (!enabled(item)) return;
    if (mContextRoot && item<*mContextRoot) dismiss();
    if (mItems[item].branch)
    { mOpen.resize(std::min(level,mOpen.size())); mOpen.push_back(item); mHovered = item; return; }
    const auto entry = mItems[item];
    if (entry.invoke) { dismiss(); entry.invoke(); return; }
    const auto specific = mItemHandlers.find({entry.action,entry.parameter});
    const auto handler = specific != mItemHandlers.end() ? specific->second : mHandlers.at(entry.action);
    dismiss();
    handler(entry.action,entry.parameter);
}

bool LLVKMenu::pointer(const LLVKWidgetTree::PointerEvent& event)
{
    mKeyboardMode=false;
    if (std::any_of(mOpen.begin(),mOpen.end(),[this](auto item) { return !itemVisible(item); }))
    { dismiss(); return false; }
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
    return event.x>=mBar.left && event.x<mBar.right && event.y>=mBar.bottom && event.y<mBar.top;
}

bool LLVKMenu::key(Key key)
{
    if (std::any_of(mOpen.begin(),mOpen.end(),[this](auto item) { return !itemVisible(item); })) dismiss();
    if (key == Key::Activate)
    { for (auto item : mRoots) if (enabled(item) && mItems[item].branch) { mKeyboardMode=true; activate(item,0); return true; } return false; }
    if (!open()) return false;
    mKeyboardMode=true;
    if (key == Key::Escape) { dismiss(); return true; }
    if (key == Key::Left || key == Key::Right)
    {
        if (mContextRoot) return true;
        if (key==Key::Right && mHovered && *mHovered!=mOpen.back() && mItems[*mHovered].branch && enabled(*mHovered))
        { activate(*mHovered,mOpen.size()); return true; }
        if (key==Key::Left && mOpen.size()>1)
        { mHovered=mOpen.back(); mOpen.pop_back(); return true; }
        std::vector<std::size_t> roots;
        for (auto item : mRoots) if (enabled(item) && mItems[item].branch) roots.push_back(item);
        if (roots.empty()) { dismiss(); return false; }
        const auto position = std::find(roots.begin(),roots.end(),mOpen.front())-roots.begin();
        activate(roots[(position+roots.size()+(key == Key::Left ? -1 : 1))%roots.size()],0); return true;
    }
    if (key == Key::Up || key == Key::Down)
    {
        std::vector<std::size_t> rows;
        for (auto item : mItems[mOpen.back()].children) if (enabled(item)) rows.push_back(item);
        if (rows.empty()) return true;
        auto found = mHovered ? std::find(rows.begin(),rows.end(),*mHovered) : rows.end();
        const auto position = found == rows.end() ? (key == Key::Down ? rows.size()-1 : 0) : std::size_t(found-rows.begin());
        mHovered = rows[(position+rows.size()+(key == Key::Up ? -1 : 1))%rows.size()]; return true;
    }
    if (key == Key::Return && mHovered && *mHovered != mOpen.back()) activate(*mHovered,mOpen.size());
    return true;
}

bool LLVKMenu::paint(LLVKWidgetPaint& output,LLVKWidgetTree::Rect viewport,std::string& error,
    std::optional<LLVKWidgetTree::Rect> bar,bool dropdowns)
{
    error.clear(); mViewport = viewport; mHits.clear();
    mBar=bar.value_or(LLVKWidgetTree::Rect{viewport.left,viewport.top-18,viewport.right,viewport.top});
    if (std::any_of(mOpen.begin(),mOpen.end(),[this](auto item) { return !itemVisible(item); })) dismiss();
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
    solid(mItems.size(),mBar,color("MenuBarBgColor",background));
    int left = mBar.left;
    for (auto item : mRoots)
    {
        if (!itemVisible(item) || !mItems[item].branch) continue;
        const int width = measure(mItems[item].label)+25;
        if (!error.empty()) return false;
        const Rect rect{left,mBar.bottom,left+width,mBar.top};
        mHits.push_back({item,0,rect});
        const bool selected = (!mOpen.empty() && mOpen[0] == item) || mHovered == item;
        if (selected) solid(item,rect,highlight);
        if (!text(item,mItems[item].label,left+width/2.f,rect.bottom+1.f,selected ? foreground : normal,LLVKFont::HorizontalAlign::Center)) return false;
        left += width;
    }
    const int rowHeight = static_cast<int>(mFont->metrics().lineHeight)+4;
    if (!dropdowns) return true;
    if (mContextRoot) mHits.push_back({*mContextRoot,0,mContextAnchor});
    for (std::size_t level = 0; level < mOpen.size(); ++level)
    {
        const auto parent = std::find_if(mHits.begin(),mHits.end(),[&](const Hit& hit) { return hit.item == mOpen[level]; });
        if (parent == mHits.end()) break;
        const auto parentRect = parent->rect;
        int width = 80, height = 4;
        for (auto item : mItems[mOpen[level]].children) if (itemVisible(item))
        { width = std::max(width,measure(mItems[item].label)+measure(shortcutLabel(mItems[item].shortcut))+65); height += mItems[item].separator ? 8 : rowHeight; }
        if (!error.empty()) return false;
        width = std::min(width,viewport.right-viewport.left);
        left = std::clamp(level ? parentRect.right : parentRect.left,viewport.left,viewport.right-width);
        int top = level ? parentRect.top : parentRect.bottom;
        if (mContextRoot && level==0)
        {
            left=parentRect.left;
            if (mPopupPosition.ends_with("right")) left=parentRect.right-width;
            if (mPopupPosition.starts_with("top")) top=parentRect.top+height;
            if (left+width>viewport.right) left-=width;
            left=std::max(left,viewport.left);
            if (top-height<viewport.bottom) top+=height;
        }
        top = std::min(viewport.top,std::max(top,viewport.bottom+height));
        solid(mOpen[level],{left,top-height,left+width,top},background);
        top -= 2;
        for (auto item : mItems[mOpen[level]].children)
        {
            const auto& entry = mItems[item]; if (!itemVisible(item)) continue;
            if (entry.separator) { solid(item,{left+3,top-4,left+width-3,top-3},disabled); top -= 8; continue; }
            const Rect rect{left,top-rowHeight,left+width,top}; mHits.push_back({item,level+1,rect});
            const bool selected = mHovered == item && enabled(item);
            if (selected) solid(item,rect,highlight);
            const auto tint = enabled(item) ? (selected ? foreground : normal) : disabled;
            if (itemChecked(item) && !text(item,"\xE2\x9C\x94",left+2.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Left)) return false;
            if (!text(item,entry.label,left+18.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Left)) return false;
            if (mKeyboardMode && level+1==mOpen.size() && entry.jumpKey)
            {
                auto label=entry.label; LLStringUtil::toUpper(label);
                const auto offset=label.find(static_cast<char>(entry.jumpKey));
                if (offset!=std::string::npos)
                {
                    const auto start=left+18+measure(entry.label.substr(0,offset));
                    const auto end=left+18+measure(entry.label.substr(0,offset+1));
                    if (!error.empty()) return false;
                    solid(item,{start,rect.bottom+3,end,rect.bottom+4},tint);
                }
            }
            if (!entry.shortcut.empty() && !text(item,shortcutLabel(entry.shortcut),left+width-7.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            if (entry.branch && !text(item,">",left+width-7.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            top -= rowHeight;
        }
    }
    return true;
}
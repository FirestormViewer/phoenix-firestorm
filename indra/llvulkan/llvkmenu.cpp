#include "llvkmenu.h"
#include "llstring.h"
#include <expat/expat.h>
#include <algorithm>
#include <cmath>
#include <boost/tokenizer.hpp>

namespace
{
    std::string shortcutBinding(std::string_view declaration)
    {
        if (declaration.empty()) return {};
        std::string binding;
        if (declaration.find("control")!=std::string_view::npos) binding+="control|";
        if (declaration.find("alt")!=std::string_view::npos) binding+="alt|";
        if (declaration.find("shift")!=std::string_view::npos) binding+="shift|";
        std::string key(declaration.substr(declaration.rfind('|')+1));
        LLStringUtil::toUpper(key);
        return binding+key;
    }

    std::string shortcutLabel(std::string value)
    {
        LLStringUtil::replaceString(value,"control|","Ctrl+");
        LLStringUtil::replaceString(value,"shift|","Shift+");
        LLStringUtil::replaceString(value,"alt|","Alt+");
        return value;
    }
}

LLVKMenu::LLVKMenu() : LLVKMenu(std::make_shared<Model>()) {}

LLVKMenu::LLVKMenu(std::shared_ptr<Model> model) : mModel(std::move(model)),mItems(mModel->items),
    mHandlers(mModel->handlers),mPredicates(mModel->predicates),mItemHandlers(mModel->itemHandlers) {}

std::shared_ptr<LLVKMenu> LLVKMenu::detachedView(std::size_t item,std::string& error)
{
    error.clear();
    if (item>=mItems.size() || !mItems[item].branch || !mItems[item].canTearOff || !enabled(item))
    { error="Native menu cannot detach this item"; return nullptr; }
    auto view=std::shared_ptr<LLVKMenu>(new LLVKMenu(mModel));
    view->mFont=mFont; view->mColors=mColors; view->mLabels=mLabels;
    view->mFixedRoot=item; view->mOpen={item};
    view->mLastHover=mLastHover;
    return view;
}

void LLVKMenu::setDetachedActive(bool active)
{
    if (!mFixedRoot) return;
    if (active) mModel->torn.insert(*mFixedRoot);
    else { mModel->torn.erase(*mFixedRoot); setDetachedFocus(false); }
}

void LLVKMenu::setDetachedFocus(bool focused)
{
    if (!mFixedRoot || focused==mDetachedFocus) return;
    mDetachedFocus=focused;
    if (focused)
    {
        mModel->focusedTorn.insert(*mFixedRoot);
        if (!mHovered && !mTearHovered) mTearHovered=*mFixedRoot;
    }
    else
    {
        mModel->focusedTorn.erase(*mFixedRoot);
        dismiss();
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
        bool rootCreateJumpKeys = false;
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
                    for (std::size_t index = 0; attributes[index]; index += 2)
                        if (std::string_view(attributes[index]) == "create_jump_keys")
                        {
                            const std::string_view value(attributes[index+1]);
                            state.rootCreateJumpKeys = value == "true" || value == "1";
                        }
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
                        else if (attribute == "tear_off") entry.canTearOff=value=="true" || value=="1";
                        else if (attribute == "drop_shadow") entry.dropShadow=value!="false" && value!="0";
                        else if (attribute == "bg_visible") entry.backgroundVisible=value!="false" && value!="0";
                        else if (attribute == "bg_color") entry.backgroundColor=value;
                        else if (attribute == "shortcut_pad")
                        {
                            std::size_t consumed=0;
                            entry.shortcutPad=std::stoi(value,&consumed);
                            if (consumed!=value.size() || entry.shortcutPad<0 || entry.shortcutPad>4096) throw std::runtime_error("menu shortcut padding");
                        }
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
    }
    if (state.rootCreateJumpKeys) menu->assignJumpKeys(menu->mRoots);
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

std::optional<std::pair<int,int>> LLVKMenu::menuSize(std::size_t item,std::string& error) const
{
    error.clear();
    if (!mFont || item>=mItems.size() || !mItems[item].branch)
    { error="Invalid native menu measurement"; return std::nullopt; }
    const auto measure=[&](const std::string& text) -> int
    {
        const auto wide=utf8str_to_wstring(text);
        const std::u32string value(wide.begin(),wide.end());
        const auto result=mFont->measureRun(value,0,value.size(),1,true,false,error);
        if (!result) return 0;
        if (!std::isfinite(result->width) || result->width>INT32_MAX-8192.f)
        { error="Native menu width overflows"; return 0; }
        return static_cast<int>(std::floor(result->width+0.5f));
    };
    const auto rowHeight=static_cast<int>(std::ceil(mFont->metrics().ascender)+
        std::ceil(mFont->metrics().descender))+4;
    int width=0,height=4+(mItems[item].canTearOff && mTearOff ? 10 : 0);
    for (const auto child : mItems[item].children)
    {
        if (!itemVisible(child)) continue;
        const auto& entry=mItems[child];
        const auto label=measure(entry.label);
        if (!error.empty()) return std::nullopt;
        const auto shortcut=entry.shortcut.empty() ? 0 : measure(shortcutLabel(entry.shortcut))+mItems[item].shortcutPad;
        if (!error.empty()) return std::nullopt;
        if (label>INT32_MAX-shortcut-40 || height>INT32_MAX-rowHeight)
        { error="Native menu dimensions overflow"; return std::nullopt; }
        width=std::max(width,label+shortcut+40);
        height+=entry.separator ? 8 : rowHeight;
    }
    return std::pair{width,height};
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
void LLVKMenu::setEnabled(std::string_view name,bool enabled)
{ for (auto& item : mItems) if (item.name == name) item.enabled = enabled; }
bool LLVKMenu::itemVisible(std::size_t item) const
{
    if (item>=mItems.size() || !mItems[item].visible) return false;
    const auto& entry=mItems[item];
    const auto predicate=mPredicates.find(entry.visibleAction);
    return predicate==mPredicates.end() || !predicate->second || predicate->second(entry.visibleParameter);
}
bool LLVKMenu::shortcut(std::string key,bool control,bool shift,bool alt)
{
    LLStringUtil::toUpper(key);
    std::string shortcut;
    if (control) shortcut += "control|";
    if (alt) shortcut += "alt|";
    if (shift) shortcut += "shift|";
    shortcut += key;
    const auto visit = [&](const auto& self,std::size_t item) -> bool
    {
        if (!mItems[item].branch)
        {
            if (mItems[item].shortcut.empty() || shortcutBinding(mItems[item].shortcut) != shortcut || !commandEnabled(item)) return false;
            invokeItem(item); return true;
        }
        if (!commandEnabled(item)) return false;
        for (auto child : mItems[item].children) if (self(self,child)) return true;
        return false;
    };
    for (auto root : mRoots) if (visit(visit,root)) return true;
    return false;
}
bool LLVKMenu::enabled(std::size_t item) const
{
    return itemVisible(item) && commandEnabled(item);
}
bool LLVKMenu::commandEnabled(std::size_t item) const
{
    if (item>=mItems.size() || !mItems[item].enabled) return false;
    const auto& entry = mItems[item];
    const auto predicate=mPredicates.find(entry.enableAction);
    if (predicate!=mPredicates.end() && predicate->second && !predicate->second(entry.enableParameter)) return false;
    const auto specific = mItemHandlers.find({entry.action,entry.parameter});
    const auto handler = mHandlers.find(entry.action);
    return entry.branch || (!entry.separator && (bool(entry.invoke) || (specific != mItemHandlers.end() ? bool(specific->second) :
        (handler != mHandlers.end() && bool(handler->second)))));
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
    mTearHovered.reset();
    if (mFixedRoot) mOpen.push_back(*mFixedRoot);
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
    {
        if (mModel->torn.contains(item) && mFixedRoot!=item)
        {
            const auto callback=mTearOff;
            dismiss();
            if (callback) callback(item,{});
            return;
        }
        mOpen.resize(std::min(level,mOpen.size())); mOpen.push_back(item); mHovered = item; return;
    }
    invokeItem(item);
}

void LLVKMenu::invokeItem(std::size_t item)
{
    if (mContextRoot && item<*mContextRoot) dismiss();
    const auto entry = mItems[item];
    const auto hit=std::find_if(mHits.begin(),mHits.end(),[item](const Hit& value) { return value.item==item && !value.tearOff; });
    if (!mContextRoot && hit!=mHits.end()) mModel->activation=Model::Activation{item,hit->rect,mModel->time};
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
        if (event.kind==Kind::Hover && selected.level && selected.level<=mOpen.size())
        {
            const auto owner=mOpen[selected.level-1];
            const auto rectangle=mPopupRects.find(owner);
            if (rectangle!=mPopupRects.end())
            {
                const auto position=std::pair{event.x-rectangle->second.left,event.y-rectangle->second.bottom};
                const auto previous=mLastHover.find(owner);
                const bool moved=previous!=mLastHover.end() && previous->second!=position;
                mLastHover[owner]=position;
                if (!moved) return true;
            }
        }
        if (selected.tearOff)
        {
            mTearHovered=selected.item;
            if (event.kind==Kind::LeftDown) mPressed=true;
            if (event.kind==Kind::LeftUp)
            {
                const bool pressed=mPressed; mPressed=false;
                const auto found=mPopupRects.find(selected.item);
                if (pressed && mTearOff && found!=mPopupRects.end())
                {
                    const auto callback=mTearOff;
                    const auto rect=found->second;
                    dismiss(); callback(selected.item,rect);
                }
            }
            return true;
        }
        mTearHovered.reset();
        if (event.kind == Kind::LeftDown) mPressed = true;
        if (event.kind == Kind::Hover)
        {
            mHovered = selected.item;
            if (open() && mItems[selected.item].branch && !mModel->torn.contains(selected.item) &&
                (mOpen.size() <= selected.level || mOpen[selected.level] != selected.item))
                activate(selected.item,selected.level);
        }
        else if (event.kind == Kind::LeftDown && mItems[selected.item].branch)
        { if (selected.level == 0 && !mOpen.empty() && mOpen[0] == selected.item) dismiss(); else activate(selected.item,selected.level); }
        else if (event.kind == Kind::LeftUp)
        { const bool pressed = mPressed; mPressed = false; if (pressed && !mItems[selected.item].branch) activate(selected.item,selected.level); }
        return true;
    }
    if (open())
    {
        const bool popup=!mFixedRoot || mOpen.size()>1;
        if (popup && (event.kind == Kind::LeftDown || event.kind == Kind::RightDown)) dismiss();
        if (event.kind == Kind::Hover && !mFixedRoot) { mHovered.reset(); mTearHovered.reset(); }
        return popup;
    }
    if (event.kind == Kind::Hover) { mHovered.reset(); mTearHovered.reset(); }
    return event.x>=mBar.left && event.x<mBar.right && event.y>=mBar.bottom && event.y<mBar.top;
}

bool LLVKMenu::key(Key key)
{
    if (std::any_of(mOpen.begin(),mOpen.end(),[this](auto item) { return !itemVisible(item); })) dismiss();
    if (key == Key::Activate)
    {
        if (mFixedRoot) { mKeyboardMode=true; return this->key(Key::Down); }
        for (auto item : mRoots)
        {
            if (enabled(item) && mItems[item].branch)
            {
                mKeyboardMode = true;
                activate(item,0);
                return true;
            }
        }
        return false;
    }
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
        if (mFixedRoot) return true;
        std::vector<std::size_t> roots;
        for (auto item : mRoots) if (enabled(item) && mItems[item].branch) roots.push_back(item);
        if (roots.empty()) { dismiss(); return false; }
        const auto position = std::find(roots.begin(),roots.end(),mOpen.front())-roots.begin();
        activate(roots[(position+roots.size()+(key == Key::Left ? -1 : 1))%roots.size()],0); return true;
    }
    if (key == Key::Up || key == Key::Down)
    {
        std::vector<std::size_t> rows;
        if (mItems[mOpen.back()].canTearOff && mTearOff) rows.push_back(mOpen.back());
        for (auto item : mItems[mOpen.back()].children) if (enabled(item)) rows.push_back(item);
        if (rows.empty()) return true;
        auto selected=mTearHovered ? mTearHovered : mHovered;
        auto found = selected ? std::find(rows.begin(),rows.end(),*selected) : rows.end();
        const auto position = found == rows.end() ? (key == Key::Down ? rows.size()-1 : 0) : std::size_t(found-rows.begin());
        selected=rows[(position+rows.size()+(key == Key::Up ? -1 : 1))%rows.size()];
        if (selected==mOpen.back()) { mTearHovered=selected; mHovered.reset(); }
        else { mHovered=selected; mTearHovered.reset(); }
        return true;
    }
    if (key==Key::Return && mTearHovered && mTearOff)
    {
        const auto item=*mTearHovered;
        const auto found=mPopupRects.find(item);
        if (found!=mPopupRects.end())
        {
            const auto callback=mTearOff;
            const auto rectangle=found->second;
            dismiss(); callback(item,rectangle);
        }
        return true;
    }
    if (key == Key::Return && mHovered && *mHovered != mOpen.back()) activate(*mHovered,mOpen.size());
    return true;
}

bool LLVKMenu::paint(LLVKWidgetPaint& output,LLVKWidgetTree::Rect viewport,std::string& error,
    std::optional<LLVKWidgetTree::Rect> bar,bool dropdowns,std::optional<float> backingBottom,float backgroundAlpha)
{
    error.clear(); mViewport = viewport; mHits.clear(); mPopupRects.clear();
    if (!std::isfinite(backgroundAlpha)) { error="Invalid native menu background alpha"; return false; }
    mBar=bar.value_or(LLVKWidgetTree::Rect{viewport.left,viewport.top-18,viewport.right,viewport.top});
    if (backingBottom && !std::isfinite(*backingBottom)) { error="Invalid native header background boundary"; return false; }
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
    const auto solid = [&](std::size_t item,Rect rect,LLVKColor::Value tint,bool headerBacking=false)
    {
        for (auto& channel : tint) channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
        if (backingBottom && !bar && rect.bottom==mBar.bottom && rect.top==mBar.top)
        {
            LLVKWidgetPaint::Command fill;
            fill.owner=UINT64_MAX-item; fill.rectangle=rect; fill.clip=viewport; fill.color=tint;
            const float left=float(rect.left),right=float(rect.right),top=float(rect.top);
            const auto bottom=std::clamp(*backingBottom,float(headerBacking ? viewport.bottom : rect.bottom),top);
            fill.triangle=std::array<float,6>{left,bottom,right,bottom,right,top};
            output.commands.push_back(fill);
            fill.triangle=std::array<float,6>{left,bottom,right,top,left,top};
            output.commands.push_back(std::move(fill));
        }
        else output.commands.push_back({UINT64_MAX-item,rect,viewport,tint});
    };
    const auto horizontalLine=[&](std::size_t item,int left,int right,int height,LLVKColor::Value tint)
    {
        const auto scale=mFont->displayScale();
        if (scale==1.f) { solid(item,{left,height-1,right,height},tint); return; }
        for (auto& channel : tint) channel=static_cast<std::uint8_t>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
        const auto cross=height*scale,fraction=cross-std::floor(cross);
        const auto radius=std::min(fraction,1.f-fraction);
        const auto endpoint=[&](float position) { return fraction==0.f ? std::ceil(position-.5f) : std::floor(position-.5f-radius)+1.f; };
        const auto low=endpoint(left*scale)/scale,high=endpoint(right*scale)/scale;
        const auto thickness=std::max(1.f,std::floor(scale+.5f));
        const auto base=std::ceil(cross-(static_cast<int>(thickness)%2 ? 0.f : .5f))-std::ceil(thickness/2.f);
        const auto bottom=base/scale,top=(base+thickness)/scale;
        LLVKWidgetPaint::Command command;
        command.owner=UINT64_MAX-item; command.clip=viewport; command.color=tint;
        command.triangle=std::array<float,6>{low,bottom,high,bottom,high,top}; output.commands.push_back(command);
        command.triangle=std::array<float,6>{low,bottom,high,top,low,top}; output.commands.push_back(std::move(command));
    };
    const auto text = [&](std::size_t item,const std::string& label,float x,float y,LLVKColor::Value tint,LLVKFont::HorizontalAlign align) -> bool
    {
        const auto wide = utf8str_to_wstring(label); const std::u32string value(wide.begin(),wide.end());
        LLVKFont::LineOptions options; options.x = x; options.y = y; options.vertical = LLVKFont::VerticalAlign::Bottom; options.horizontal = align;
        const auto row=std::find_if(mHits.begin(),mHits.end(),[item](const Hit& hit) { return hit.item==item && hit.level>0 && !hit.tearOff; });
        if (row!=mHits.end())
        {
            options.originX=static_cast<float>(row->rect.left); options.originY=static_cast<float>(row->rect.bottom);
            options.x-=options.originX; options.y-=options.originY;
        }
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
    if (!bar && !mFixedRoot)
    {
        int width=0;
        for (auto item : mRoots)
            if (itemVisible(item) && mItems[item].branch) width+=measure(mItems[item].label)+25;
        if (!error.empty()) return false;
        solid(mItems.size(),mBar,{0,0,0,1},true);
        mBar.right=mBar.left+width;
    }
    auto barColor=color("MenuBarBgColor",background); barColor[3]*=backgroundAlpha;
    if (!mFixedRoot) solid(mItems.size(),mBar,barColor);
    int left = mBar.left;
    for (auto item : mRoots)
    {
        if (!itemVisible(item) || !mItems[item].branch) continue;
        const int width = measure(mItems[item].label)+25;
        if (!error.empty()) return false;
        const Rect rect{left,mBar.bottom,left+width,mBar.top};
        mHits.push_back({item,0,rect});
        const bool selected = (!mOpen.empty() && mOpen[0] == item) || mHovered == item || mModel->focusedTorn.contains(item);
        if (selected) solid(item,rect,highlight);
        const auto scale=mFont->displayScale();
        const auto itemOriginX=std::floor(left*scale)/scale;
        const auto itemOriginY=std::floor(rect.bottom*scale)/scale;
        if (!text(item,mItems[item].label,itemOriginX+width/2.f,itemOriginY+1.f,selected ? foreground : normal,LLVKFont::HorizontalAlign::Center)) return false;
        left += width;
    }
    const int rowHeight = static_cast<int>(std::ceil(mFont->metrics().ascender)+
        std::ceil(mFont->metrics().descender))+4;
    if (!dropdowns && !mFixedRoot) return true;
    if (mFixedRoot) mHits.push_back({*mFixedRoot,0,{mBar.left,mBar.top,mBar.right,mBar.top}});
    if (mContextRoot) mHits.push_back({*mContextRoot,0,mContextAnchor});
    for (std::size_t level = 0; level < mOpen.size(); ++level)
    {
        if (!dropdowns && level) break;
        const auto commandBegin=output.commands.size();
        const auto parent = std::find_if(mHits.begin(),mHits.end(),[&](const Hit& hit) { return hit.item == mOpen[level]; });
        if (parent == mHits.end()) break;
        const auto parentRect = parent->rect;
        const bool tearOff=mItems[mOpen[level]].canTearOff && bool(mTearOff);
        const auto dimensions=menuSize(mOpen[level],error);
        if (!dimensions) return false;
        auto [width,height]=*dimensions;
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
        mPopupRects[mOpen[level]]={left,top-height,left+width,top};
        if (mItems[mOpen[level]].dropShadow && (!mFixedRoot || level) && !output.appendDropShadow(UINT64_MAX-mOpen[level],
            mPopupRects[mOpen[level]],viewport,color("ColorDropShadow",{0,0,0,0.5f}),6.f,error)) return false;
        auto popupColor=color(mItems[mOpen[level]].backgroundColor.c_str(),background); popupColor[3]*=backgroundAlpha;
        if (mItems[mOpen[level]].backgroundVisible) solid(mOpen[level],{left,top-height,left+width,top},popupColor);
        if (tearOff)
        {
            const Rect rect{left,top-10,left+width,top};
            mHits.push_back({mOpen[level],level+1,rect,true});
            if (mTearHovered==mOpen[level]) solid(mOpen[level],rect,highlight);
            const auto tint=disabled;
            horizontalLine(mOpen[level],left+6,left+width-6,rect.bottom+3,tint);
            horizontalLine(mOpen[level],left+6,left+width-6,rect.bottom+6,tint);
            top-=10;
        }
        for (auto item : mItems[mOpen[level]].children)
        {
            const auto& entry = mItems[item]; if (!itemVisible(item)) continue;
            if (entry.separator) { horizontalLine(item,left+6,left+width-6,top-4,disabled); top -= 8; continue; }
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
            if (!entry.shortcut.empty() && !text(item,shortcutLabel(entry.shortcut),left+width-22.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            if (entry.branch && !text(item,"\xE2\x96\xB8",left+width-7.f,rect.bottom+2.f,tint,LLVKFont::HorizontalAlign::Right)) return false;
            top -= rowHeight;
        }
        if (mFixedRoot && dropdowns && !level) output.commands.resize(commandBegin);
    }
    if (!mFixedRoot && dropdowns && mModel->activation)
    {
        const auto activation=*mModel->activation;
        const auto elapsed=static_cast<float>(mModel->time-activation.started);
        if (elapsed>=0.f && elapsed<0.3f && itemVisible(activation.item))
        {
            const auto& entry=mItems[activation.item];
            const auto rectangle=activation.rectangle;
            auto tint=highlight; tint[3]*=elapsed/0.3f;
            solid(activation.item,rectangle,tint);
            const auto labelColor=enabled(activation.item) ? normal : disabled;
            if (itemChecked(activation.item) && !text(activation.item,"\xE2\x9C\x94",rectangle.left+2.f,rectangle.bottom+2.f,labelColor,LLVKFont::HorizontalAlign::Left)) return false;
            if (!text(activation.item,entry.label,rectangle.left+18.f,rectangle.bottom+2.f,labelColor,LLVKFont::HorizontalAlign::Left)) return false;
            if (!entry.shortcut.empty() && !text(activation.item,shortcutLabel(entry.shortcut),rectangle.right-22.f,rectangle.bottom+2.f,labelColor,LLVKFont::HorizontalAlign::Right)) return false;
        }
        else mModel->activation.reset();
    }
    return true;
}

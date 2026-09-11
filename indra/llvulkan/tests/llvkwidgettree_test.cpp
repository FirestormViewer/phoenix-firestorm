#include "linden_common.h"
#include "llvkwidgettree.h"
#include "llvkwidgetlayout.h"
#include "llvkwidgetfactory.h"
#include "llvklabel.h"
#include "llvkcolor.h"
#include "llvkplaintextlayout.h"
#include "llvkxmllayers.h"
#include "llvkskinfiles.h"
#include "llvkskinimages.h"
#include "llvklineeditor.h"
#include "llvkclipboard.h"
#include "llvkbrowsersurface.h"
#include "llvkwidgetpaint.h"
#include "llvkloginui.h"
#include "llvkstartupsettings.h"
#include "lluri.h"
#include "llvkstyledtext.h"
#include "llvkscroll.h"
#include "llvkfont.h"
#include "llvkwidgetimage.h"
#include "lltut.h"
#include <png.h>
extern "C" {
#if __has_include(<jpeglib.h>)
#include <jpeglib.h>
#else
#include <jpeglib/jpeglib.h>
#endif
}

#include <fstream>
#include <iterator>

namespace tut
{
    struct widgettree_data
    {
        std::shared_ptr<LLVKFont> loadFont()
        {
            std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
            ensure("font fixture",stream.good());
            std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
            std::string error;
            std::shared_ptr<LLVKFont> font = LLVKFont::create({bytes,{}},{},false,error);
            ensure(error,font != nullptr);
            return font;
        }
        std::shared_ptr<const LLVKWidgetImage> image(const std::string& name)
        {
            png_image encoder{};
            encoder.version = PNG_IMAGE_VERSION;
            encoder.width = encoder.height = 1;
            encoder.format = PNG_FORMAT_RGBA;
            auto cleanup = [](png_image* value) { png_image_free(value); };
            std::unique_ptr<png_image,decltype(cleanup)> encoderOwner(&encoder,cleanup);
            const std::uint8_t pixel[]{255,255,255,255};
            png_alloc_size_t length = 0;
            ensure("image size",png_image_write_to_memory(&encoder,nullptr,&length,0,pixel,0,nullptr) != 0);
            std::vector<std::uint8_t> png(length);
            ensure("encode image",png_image_write_to_memory(&encoder,png.data(),&length,0,pixel,0,nullptr) != 0);
            std::string error;
            auto result = LLVKWidgetImage::decodePng(name,png,error);
            ensure(error,result != nullptr);
            return result;
        }
    };
    typedef test_group<widgettree_data,200> widgettree_group;
    typedef widgettree_group::object object;
    widgettree_group widgettree_tests("llvkwidgettree");

    template<> template<> void object::test<146>()
    {
        set_test_name("native Preferences snapshots restore bound settings and commit swatch colors");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view; view.rect={0,0,300,200};
        LLVKControl::Params control; control.font=loadFont();
        std::string error;
        const auto root=tree.createPanel(view,control,{},0,error);
        ensure(error,root.has_value());
        ensure("Boolean preference",tree.defineSetting("RestrainedLove",LLSD(false),LLVKWidgetTree::SettingType::Boolean));
        ensure("empty chat path",tree.defineSetting("InstantMessageLogPath",LLSD(""),LLVKWidgetTree::SettingType::String));
        for (const auto name : {"RestrainedLove","InstantMessageLogPath"})
        {
            control.valueSetting=name;
            ensure("bound preference control",tree.createControl(view,control,*root,error).has_value());
        }
        control.valueSetting.reset();
        int colorCommits=0;
        bool settingAtColorCommit=true;
        control.commit.function=[&](auto,const LLSD&)
        { settingAtColorCommit=tree.setting("RestrainedLove")->asBoolean(); ++colorCommits; };
        LLVKWidgetTree::ColorSwatchParams params; params.color=LLVKColor{0.25f,0.5f,0.75f,1.f};
        const auto swatch=tree.createColorSwatch(view,control,params,*root,error);
        ensure(error,swatch.has_value());
        const auto snapshot=tree.snapshotPreferences(*root,error);
        ensure(error,snapshot.has_value());
        ensure_equals("two bound settings captured",snapshot->settings.size(),std::size_t(2));
        ensure_equals("one swatch captured",snapshot->colors.size(),std::size_t(1));
        tree.updateSetting("RestrainedLove",LLSD(true));
        tree.updateSetting("InstantMessageLogPath",LLSD("new-path"));
        ensure("start preview",tree.beginColorSelection(*swatch,error));
        ensure("select replacement",tree.applyColorSelection(*swatch,{1,0,0,1},LLVKWidgetTree::ColorPickOperation::Select,error));
        colorCommits=0;
        ensure("Cancel restores snapshot",tree.restorePreferences(*snapshot,{},error));
        ensure("Boolean restored",!tree.setting("RestrainedLove")->asBoolean());
        ensure_equals("empty original chat path deliberately skipped",tree.setting("InstantMessageLogPath")->asString(),std::string("new-path"));
        ensure("original swatch restored",tree.get(*swatch)->colorSwatch->color==params.color.get());
        ensure_equals("restored color committed",colorCommits,1);
        ensure("settings restored before color callback",!settingAtColorCommit);
        tree.updateSetting("RestrainedLove",LLSD(true));
        auto settingsOnly=*snapshot; settingsOnly.colors.clear();
        ensure("explicit skip retained",tree.restorePreferences(settingsOnly,{"RestrainedLove"},error));
        ensure("skipped setting unchanged",tree.setting("RestrainedLove")->asBoolean());
    }

    template<> template<> void object::test<145>()
    {
        set_test_name("native color swatch preserves alpha and preview cancellation order");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view; view.rect={0,0,80,60};
        LLVKControl::Params control; control.font=loadFont();
        std::vector<std::string> events;
        control.commit.function=[&](auto,const LLSD&) { events.push_back("commit"); };
        LLVKWidgetTree::ColorSwatchParams params;
        params.color=LLVKColor{0.2f,0.3f,0.4f,0.5f}; params.label="Tint";
        params.applyImmediately=true;
        int opened=0;
        params.showPicker=[&](auto,bool takeFocus)
        {
            ensure("click releases capture before opening",tree.mouseCapture()==0);
            ensure("mouse picker does not force focus",!takeFocus);
            ++opened;
        };
        params.cancelled.function=[&](auto,const LLSD&) { events.push_back("cancel"); };
        params.selected.function=[&](auto,const LLSD&) { events.push_back("select"); };
        LLSD initialColor=LLSD::emptyArray();
        for (const auto channel : params.color.get()) initialColor.append(channel);
        ensure("bound native color",tree.defineSetting("Tint",initialColor));
        control.valueSetting="Tint";
        std::string error;
        const auto swatch=tree.createColorSwatch(view,control,params,0,error);
        ensure(error,swatch.has_value());
        LLVKWidgetTree::PointerEvent pointer;
        pointer.kind=LLVKWidgetTree::PointerKind::LeftDown; pointer.x=10; pointer.y=30;
        ensure("swatch press",tree.routePointer(*swatch,pointer,error));
        pointer.kind=LLVKWidgetTree::PointerKind::LeftUp; pointer.x=90;
        ensure("release outside swatch",tree.routePointer(*swatch,pointer,error));
        ensure_equals("outside release does not open picker",opened,0);
        pointer.kind=LLVKWidgetTree::PointerKind::LeftDown; pointer.x=10;
        ensure("second swatch press",tree.routePointer(*swatch,pointer,error));
        pointer.kind=LLVKWidgetTree::PointerKind::LeftUp;
        ensure("inside release opens picker",tree.routePointer(*swatch,pointer,error));
        ensure_equals("picker opens once",opened,1);
        ensure("begin native selection",tree.beginColorSelection(*swatch,error));
        ensure("live preview",tree.applyColorSelection(*swatch,{0.8f,0.7f,0.6f,1.f},LLVKWidgetTree::ColorPickOperation::Change,error));
        ensure("preview callback",events==std::vector<std::string>{"commit"});
        ensure_equals("preview updates bound setting",tree.setting("Tint")->operator[](0).asReal(),double(0.8f));
        ensure_equals("picker preserves current alpha",tree.value(*swatch)[3].asReal(),0.5);
        ensure("cancel selection",tree.applyColorSelection(*swatch,{},LLVKWidgetTree::ColorPickOperation::Cancel,error));
        ensure("cancel overrides default commit",events==std::vector<std::string>{"commit","cancel"});
        ensure("original RGB restored",tree.get(*swatch)->colorSwatch->color==params.color.get());
        ensure_equals("cancel restores bound setting",tree.setting("Tint")->operator[](0).asReal(),double(0.2f));
        ensure("begin selected color",tree.beginColorSelection(*swatch,error));
        ensure("select color",tree.applyColorSelection(*swatch,{1.f,0.f,0.f,0.f},LLVKWidgetTree::ColorPickOperation::Select,error));
        ensure("select callback",events.back()=="select");
        ensure("selection closed",!tree.get(*swatch)->colorSwatch->picking);
        ensure_equals("focus restored to swatch",tree.keyboardFocus(),*swatch);
        ensure("checker fixture registered",tree.registerImage(image("Checker")));
        const auto paint=LLVKWidgetPaint::prepare(tree,*swatch,{},error);
        ensure(error,paint.has_value());
        ensure("native color fill retains alpha",std::any_of(paint->commands.begin(),paint->commands.end(),
            [&](const auto& command) { return command.owner==*swatch && command.rectangle==LLVKWidgetTree::Rect{1,17,79,59} && command.color==LLVKColor::Value{1,0,0,0.5f}; }));
    }

    template<> template<> void object::test<144>()
    {
        set_test_name("native setting subscribers observe changes and tolerate nested disconnects");
        LLVKWidgetTree tree;
        ensure("RLVa setting",tree.defineSetting("RestrainedLove",LLSD(false),LLVKWidgetTree::SettingType::Boolean));
        std::vector<std::string> changes;
        std::optional<std::uint64_t> later;
        const auto first=tree.subscribeSetting("RestrainedLove",[&](const LLSD& value,const LLSD& previous)
        {
            changes.push_back(previous.asString()+":"+value.asString());
            if (value.asBoolean())
            {
                if (later) tree.unsubscribeSetting(*later);
                ensure("nested setting update",tree.updateSetting("RestrainedLove",LLSD(false)));
            }
        });
        ensure("subscribed",first.has_value());
        later=tree.subscribeSetting("RestrainedLove",[&](const LLSD&,const LLSD&) { changes.push_back("disconnected"); });
        ensure("same value accepted",tree.updateSetting("RestrainedLove",LLSD(false)));
        ensure("unchanged values do not notify",changes.empty());
        ensure("changed setting",tree.updateSetting("RestrainedLove",LLSD(true)));
        ensure_equals("outer and nested notifications",changes.size(),std::size_t(2));
        ensure("outer value snapshot stable",changes[0]==LLSD(false).asString()+":"+LLSD(true).asString());
        ensure("nested previous value",changes[1]==LLSD(true).asString()+":"+LLSD(false).asString());
        ensure("nested value retained",!tree.setting("RestrainedLove")->asBoolean());
        ensure("disconnect",tree.unsubscribeSetting(*first));
        ensure("missing setting rejected",!tree.subscribeSetting("missing",[](const LLSD&,const LLSD&){}));
    }

    template<> template<> void object::test<143>()
    {
        set_test_name("native horizontal tabs clamp pixel scrolling to the last tab");
        LLVKWidgetTree tree;
        LLVKControl::Params control; control.font=loadFont();
        LLVKWidgetTree::Params view; view.rect={0,0,300,120};
        std::string error;
        const auto owner=tree.createPanel(view,control,{},0,error);
        ensure(error,owner.has_value());
        ensure("tab owner",tree.initializeTabContainer(*owner,error));
        for (int index=0; index<8; ++index)
        {
            view.name="page"+std::to_string(index);
            const auto panel=tree.createPanel(view,control,{},*owner,error);
            view.name="tab"+std::to_string(index);
            LLVKButton::Params button; button.label=U"Tab";
            const auto tab=tree.createButton(view,control,button,*owner,error);
            ensure(error,panel && tab);
            ensure("attach tab",tree.attachTabPanel(*owner,*panel,*tab,error));
        }
        LLVKWidgetTree::Node::TabContainer::Layout layout;
        layout.minimumWidth=layout.maximumWidth=60;
        layout.horizontalArrowSize=16; layout.partialTabWidth=20;
        ensure("horizontal overflow layout",tree.layoutTabPanels(*owner,layout,error));
        auto state=*tree.get(*owner)->tabContainer;
        ensure_equals("source maximum scroll position",state.maximumScroll,5);
        ensure_equals("arrows reserve left strip",tree.get(state.tabs.front().button)->params.rect.left,33);
        ensure("scroll one tab",tree.scrollTabStrip(*owner,1,error));
        ensure_equals("partial previous tab allowance",tree.get(*owner)->tabContainer->targetScrollPixels,40);
        ensure_equals("input does not jump animated pixels",tree.get(*owner)->tabContainer->scrollPixels,0);
        ensure("source half-life animation",tree.layoutTabPanels(*owner,layout,error,0.08f));
        ensure_equals("one half-life moves halfway",tree.get(*owner)->tabContainer->scrollPixels,20);
        ensure("scroll to end",tree.scrollTabStrip(*owner,100,error));
        state=*tree.get(*owner)->tabContainer;
        ensure_equals("source last-tab pixel clamp",state.targetScrollPixels,248);
        ensure("settled pixel layout",tree.layoutTabPanels(*owner,layout,error,10.f));
        ensure_equals("rightmost tab remains beside arrows",tree.get(state.tabs.back().button)->params.rect.right,265);
        ensure("horizontal tabs remain visible for clipping",std::all_of(state.tabs.begin(),state.tabs.end(),[&](const auto& tab) { return tree.get(tab.button)->params.visible; }));
        ensure("horizontal arrow owners",tree.createTabArrows(*owner,control,{},error));
        state=*tree.get(*owner)->tabContainer;
        ensure("four arrows",state.previousArrow && state.nextArrow && state.firstArrow && state.lastArrow);
        ensure("selected tab reveals first",tree.selectTabPanel(*owner,state.tabs.front().panel,error));
        ensure_equals("selection reveals first pixel",tree.get(*owner)->tabContainer->scrollPosition,0);
        ensure("jump last",tree.commit(state.lastArrow));
        ensure_equals("jump does not select",tree.get(*owner)->tabContainer->selected,state.tabs.front().panel);
        ensure_equals("jump reaches maximum",tree.get(*owner)->tabContainer->scrollPosition,5);
        ensure("selected last tab remains visible",tree.selectTabPanel(*owner,state.tabs.back().panel,error));
        ensure("jump first",tree.commit(state.firstArrow));
        ensure_equals("first resets position",tree.get(*owner)->tabContainer->scrollPosition,0);
        ensure("horizontal strip wheel",tree.routeWheel(*owner,120,110,2,false,error));
        ensure_equals("horizontal wheel scrolls",tree.get(*owner)->tabContainer->scrollPosition,2);
        LLVKWidgetPaint::Input paintInput; paintInput.button.frameDelta=0.08f;
        const auto paint=LLVKWidgetPaint::prepare(tree,*owner,paintInput,error);
        ensure(error,paint.has_value());
        ensure("overflow paint remains inside source clip",std::all_of(paint->commands.begin(),paint->commands.end(),
            [&](const auto& command) { return command.owner==*owner || (command.clip.left>=3 && command.clip.right<=297); }));
    }

    template<> template<> void object::test<142>()
    {
        set_test_name("native vertical tab overflow owns bounded row positions");
        LLVKWidgetTree tree;
        LLVKControl::Params control; control.font=loadFont();
        LLVKWidgetTree::Params view; view.rect={0,0,300,120};
        std::string error;
        const auto owner=tree.createPanel(view,control,{},0,error);
        ensure(error,owner.has_value());
        ensure("tab owner",tree.initializeTabContainer(*owner,error));
        for (int index=0; index<8; ++index)
        {
            view.name="page"+std::to_string(index);
            const auto panel=tree.createPanel(view,control,{},*owner,error);
            view.name="tab"+std::to_string(index);
            LLVKButton::Params button; button.label=U"Tab";
            const auto tab=tree.createButton(view,control,button,*owner,error);
            ensure(error,panel && tab);
            ensure("attach tab",tree.attachTabPanel(*owner,*panel,*tab,error));
        }
        LLVKWidgetTree::Node::TabContainer::Layout layout;
        layout.position=decltype(layout.position)::Left;
        layout.minimumWidth=100; layout.verticalArrowSize=16;
        ensure("overflow layout",tree.layoutTabPanels(*owner,layout,error));
        auto state=*tree.get(*owner)->tabContainer;
        ensure_equals("source maximum row scroll",state.maximumScroll,5);
        ensure("first row visible",tree.get(state.tabs[0].button)->params.visible);
        ensure("fourth row hidden",!tree.get(state.tabs[3].button)->params.visible);
        ensure("scroll to last rows",tree.scrollTabStrip(*owner,100,error));
        state=*tree.get(*owner)->tabContainer;
        ensure_equals("bounded scroll position",state.scrollPosition,5);
        ensure("first row hidden",!tree.get(state.tabs[0].button)->params.visible);
        ensure("last row visible",tree.get(state.tabs.back().button)->params.visible);
        ensure("arrow owners",tree.createVerticalTabArrows(*owner,control,{},error));
        state=*tree.get(*owner)->tabContainer;
        ensure("overflow arrow visible",tree.get(state.previousArrow)->params.visible && tree.get(state.nextArrow)->params.visible);
        ensure("select first reveals row",tree.selectTabPanel(*owner,state.tabs.front().panel,error));
        ensure_equals("selection scrolls into view",tree.get(*owner)->tabContainer->scrollPosition,0);
        ensure("next arrow click",tree.commit(state.nextArrow));
        ensure_equals("arrow changes selected panel",tree.get(*owner)->tabContainer->selected,state.tabs[1].panel);
        ensure_equals("arrow advances scroll",tree.get(*owner)->tabContainer->scrollPosition,1);
        ensure("wheel over tab strip",tree.routeWheel(*owner,20,60,2,false,error));
        ensure_equals("wheel advances rows",tree.get(*owner)->tabContainer->scrollPosition,3);
        ensure_equals("wheel preserves selected panel",tree.get(*owner)->tabContainer->selected,state.tabs[1].panel);
        ensure("wheel outside tab strip falls through",!tree.routeWheel(*owner,200,60,2,false,error));
        ensure_equals("content wheel leaves strip unchanged",tree.get(*owner)->tabContainer->scrollPosition,3);
        ensure("expand owner",tree.reshape(*owner,300,400,error));
        ensure("expanded layout",tree.layoutTabPanels(*owner,layout,error));
        state=*tree.get(*owner)->tabContainer;
        ensure_equals("no overflow after resize",state.maximumScroll,0);
        ensure_equals("resize resets scroll",state.scrollPosition,0);
        ensure("resize hides arrows",!tree.get(state.previousArrow)->params.visible && !tree.get(state.nextArrow)->params.visible);
        ensure("all tabs restored",std::all_of(state.tabs.begin(),state.tabs.end(),[&](const auto& tab) { return tree.get(tab.button)->params.visible; }));
    }

    template<> template<> void object::test<141>()
    {
        set_test_name("native search editor owns input and clear callback ordering");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view; view.rect={0,0,200,24};
        LLVKControl::Params control; control.font=loadFont(); control.initialValue="query";
        std::vector<std::string> calls;
        control.commit.function=[&](auto,const LLSD& value) { calls.push_back("commit:"+value.asString()); };
        LLVKWidgetTree::SearchEditorParams search;
        search.clearVisible=true; search.editor.text.leftPadding=6;
        search.textChanged.function=[&](auto,const LLSD& value) { calls.push_back("changed:"+value.asString()); };
        std::string error;
        const auto owner=tree.createSearchEditor(view,control,search,0,error);
        ensure(error,owner.has_value());
        const auto state=*tree.get(*owner)->searchEditor;
        ensure_equals("public input value",tree.value(*owner).asString(),std::string("query"));
        ensure("clear initially visible",tree.get(state.clear)->params.visible);
        ensure_equals("search reserves input padding",tree.get(state.editor)->lineEditor->params.text.leftPadding,19);
        ensure("focus forwards",tree.requestControlFocus(*owner,true,error));
        ensure_equals("input owns focus",tree.keyboardFocus(),state.editor);
        ensure("clear button",tree.commit(state.clear));
        ensure("changed before commit",calls==std::vector<std::string>{"changed:","commit:"});
        ensure("refresh after clear",tree.refreshSearchEditor(*owner,error));
        ensure("empty hides clear",!tree.get(state.clear)->params.visible);
        ensure("set value forwards",tree.setValue(*owner,LLSD("next")));
        ensure_equals("inner value",tree.value(state.editor).asString(),std::string("next"));
        calls.clear();
        ensure("navigation handled",tree.lineEditorKey(state.editor,LLVKLineEditor::Key::Left,{},error));
        ensure("left is not text change",calls.empty());
        ensure("typing after left",tree.lineEditorUnicode(state.editor,U'x',error));
        ensure("typing not suppressed by previous arrow",calls.size()==1 && calls.front().starts_with("changed:"));
        search.textChanged.function=[&](auto id,const LLSD&) { tree.erase(id,error); };
        const auto deleting=tree.createSearchEditor(view,control,search,0,error);
        ensure(error,deleting.has_value());
        ensure("clear tolerates callback deletion",tree.clearSearchEditor(*deleting,error));
        ensure("deleted owner",!tree.get(*deleting));
    }

    template<> template<> void object::test<140>()
    {
        set_test_name("native read-only editor owns scroll document border and selectable contents");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view; view.rect={0,0,140,70}; view.enabled=false;
        LLVKControl::Params control; control.font=loadFont();
        control.initialValue="first line\nsecond line\nthird line\nfourth line\nfifth line\nsixth line";
        LLVKPlainControl::Params text; text.layout.wrap=true;
        LLVKWidgetTree::ScrollContainerParams scroll;
        scroll.size=16; scroll.scrollbarControl.font=control.font;
        scroll.vertical.decreaseControl.font=scroll.vertical.increaseControl.font=control.font;
        scroll.horizontal.decreaseControl.font=scroll.horizontal.increaseControl.font=control.font;
        const auto editor=tree.createTextEditor(view,control,text,scroll,true,0,error);
        ensure(error,editor.has_value());
        const auto state=*tree.get(*editor)->textEditor;
        ensure("read-only editor stays enabled",tree.get(*editor)->params.enabled && tree.get(state.body)->plainText->readOnly);
        const auto bar=tree.get(state.scroller)->scrollContainer->vertical;
        ensure("editor scrollbar visible",tree.get(bar)->params.visible);
        ensure("editor painter",LLVKWidgetPaint::prepare(tree,*editor,{},error).has_value());
        ensure("read-only wheel",tree.routeWheel(*editor,20,20,2,false,error));
        ensure("scroll position changes",tree.get(bar)->scrollbar->position>0);
        ensure("editor start of document",tree.startTextEditorDocument(*editor,error));
        ensure_equals("document start",tree.get(bar)->scrollbar->position,0);
        ensure("read-only Down delegates before tabs",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Down,{},error));
        ensure_equals("source line scroll step",tree.get(bar)->scrollbar->position,16);
        ensure("read-only End",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::End,{},error));
        ensure_equals("source End reaches scroll maximum",tree.get(bar)->scrollbar->position,
            tree.get(bar)->scrollbar->documentSize-tree.get(bar)->scrollbar->pageSize);
        ensure("read-only Home",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Home,{},error));
        ensure_equals("source Home scrolls without changing text",tree.get(bar)->scrollbar->position,0);
        const auto beforePage=tree.value(*editor).asString();
        ensure("source page key returns unhandled",!tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::PageDown,{},error));
        ensure("source page key still scrolls",tree.get(bar)->scrollbar->position>0);
        ensure_equals("keyboard scrolling preserves contents",tree.value(*editor).asString(),beforePage);
        ensure("public editor select-all",tree.selectAllPlainText(*editor));
        ensure_equals("selection covers display text",tree.get(state.body)->plainText->selectionStart,tree.get(state.body)->plainText->text.size());
        ensure("public text update",tree.setValue(*editor,LLSD("short")));
        ensure("short document hides bar",!tree.get(bar)->params.visible);
        ensure("short document unmodified Right is unhandled",!tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Right,{},error));
        tree.deselectPlainText(*editor);
        ensure("Shift Right selects a display character",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Right,{true,false,false},error));
        ensure_equals("selection begins at cursor",tree.get(state.body)->plainText->selectionStart,std::size_t(0));
        ensure_equals("one display character selected",tree.get(state.body)->plainText->selectionEnd,std::size_t(1));
        ensure("Ctrl Right follows word boundary",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Right,{false,true,false},error));
        ensure_equals("word navigation reaches end",tree.get(state.body)->plainText->cursor,std::size_t(5));
        ensure_equals("word navigation clears selection",tree.get(state.body)->plainText->selectionEnd,std::size_t(0));
        ensure("Ctrl Shift Left selects back to start",tree.textEditorKey(*editor,LLVKWidgetTree::ScrollKey::Left,{true,true,false},error));
        ensure_equals("reverse selection anchor",tree.get(state.body)->plainText->selectionStart,std::size_t(5));
        ensure_equals("reverse selection end",tree.get(state.body)->plainText->selectionEnd,std::size_t(0));
        ensure("editor resize",tree.reshape(*editor,200,100,error));
        ensure("editor layout after resize",tree.layoutTextEditor(*editor,error));
        ensure_equals("scroller follows editor",tree.get(state.scroller)->params.rect.right,200);
    }

    template<> template<> void object::test<139>()
    {
        set_test_name("native slider rounding, change-only commits and drag offset");
        LLVKWidgetTree tree;
        LLVKControl::Params control; control.font=loadFont();
        int commits=0;
        control.commit.function=[&](auto,const LLSD&) { ++commits; };
        LLVKWidgetTree::Params view; view.rect={0,0,116,24};
        LLVKWidgetTree::SliderParams params; params.maximum=10; params.increment=1; params.initial=3;
        std::string error;
        const auto id=tree.createSlider(view,control,params,0,error);
        ensure(error,id.has_value());
        ensure("half rounds down",tree.setSliderValue(*id,4.5f,false,true,error));
        ensure_equals("source tie bias",tree.value(*id).asReal(),4.);
        ensure("same value accepted",tree.setSliderValue(*id,4.f,false,true,error));
        ensure_equals("no unchanged commit",commits,1);
        const auto thumb=tree.get(*id)->slider->thumb;
        LLVKWidgetTree::PointerEvent event;
        event.kind=LLVKWidgetTree::PointerKind::LeftDown; event.x=thumb.left+2; event.y=12;
        ensure("thumb captured",tree.routePointer(*id,event,error));
        event.kind=LLVKWidgetTree::PointerKind::Hover;
        ensure("stationary drag",tree.routePointer(*id,event,error));
        ensure_equals("grab offset prevents jump",tree.value(*id).asReal(),4.);
        event.x=500;
        ensure("drag clamps",tree.routePointer(*id,event,error));
        ensure_equals("drag maximum",tree.value(*id).asReal(),10.);
        event.kind=LLVKWidgetTree::PointerKind::LeftUp;
        ensure("release",tree.routePointer(*id,event,error));
        ensure_equals("capture released",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        tree.setInputModifiers({false,true,false});
        event.kind=LLVKWidgetTree::PointerKind::LeftDown; event.x=40;
        ensure("control-click restores initial",tree.routePointer(*id,event,error));
        ensure_equals("initial restored",tree.value(*id).asReal(),3.);
        LLVKWidgetTree::SliderControlParams composite;
        composite.bar=params;
        composite.editable=true; composite.precision=1;
        composite.textWidth=40;
        composite.editorControl.font=control.font;
        view.rect={0,0,200,24};
        control.initialValue=LLSD(2.f);
        bool reject=false;
        control.validate.function=[&](auto,const LLSD&) { return !reject; };
        const auto slider=tree.createSliderControl(view,control,composite,0,error);
        ensure(error,slider.has_value());
        const auto editor=tree.get(*slider)->sliderControl->editor;
        ensure_equals("slider value editor",tree.value(editor).asString(),std::string("2.0"));
        tree.setValue(editor,LLSD("7"));
        ensure("slider editor commit",tree.commitSliderControl(*slider,true,error));
        ensure_equals("editor updates bar and parent",tree.value(*slider).asReal(),7.);
        reject=true;
        tree.setValue(editor,LLSD("8"));
        ensure("slider validation veto",!tree.commitSliderControl(*slider,true,error));
        ensure_equals("parent value restored",tree.value(*slider).asReal(),7.);
        ensure_equals("value editor restored",tree.value(editor).asString(),std::string("7.0"));
    }

    template<> template<> void object::test<138>()
    {
        set_test_name("native radio group payload selection, exclusion and repeated commits");
        LLVKWidgetTree tree;
        tree.defineSetting("Choice",LLSD("second"),LLVKWidgetTree::SettingType::String);
        LLVKControl::Params control;
        control.font=loadFont(); control.valueSetting="Choice";
        int commits=0;
        control.commit.function=[&](auto,const LLSD&) { ++commits; };
        std::vector<LLVKWidgetTree::RadioItemParams> items(2);
        for (std::size_t index=0; index<items.size(); ++index)
        {
            auto& item=items[index];
            item.view.name=index ? "second" : "first";
            item.view.rect={0,static_cast<int>(index)*24,120,static_cast<int>(index)*24+20};
            item.check.label=item.view.name;
            item.check.labelControl.font=item.check.buttonControl.font=control.font;
            item.check.labelView.rect={20,0,120,20};
            item.check.buttonView.rect={0,0,16,16};
            item.check.button.toggle=true;
        }
        std::string error;
        LLVKWidgetTree::Params view; view.rect={0,0,140,60};
        const auto id=tree.createRadioGroup(view,control,items,false,0,error);
        ensure(error,id.has_value());
        const auto first=tree.get(*id)->radioGroup->items[0].control;
        const auto second=tree.get(*id)->radioGroup->items[1].control;
        ensure("initial setting selects payload",!tree.value(first).asBoolean() && tree.value(second).asBoolean());
        ensure("index fallback selects first",tree.setRadioValue(*id,LLSD(0),error));
        ensure_equals("payload returned",tree.value(*id).asString(),std::string("first"));
        ensure("cannot deselect",!tree.selectRadioIndex(*id,-1,true,error));
        ensure("radio click",tree.commit(first));
        ensure("repeat radio click",tree.commit(first));
        ensure_equals("unchanged clicks still commit",commits,2);
        ensure_equals("clicked payload saved",tree.setting("Choice")->asString(),std::string("first"));
        ensure("keyboard next",tree.radioKey(*id,true,error));
        ensure("mutual exclusion",!tree.value(first).asBoolean() && tree.value(second).asBoolean());
        ensure("no wrap past final radio",!tree.radioKey(*id,true,error));
        control.valueSetting.reset();
        control.initialValue=LLSD("second");
        const auto deselectable=tree.createRadioGroup(view,control,items,true,0,error);
        ensure(error,deselectable.has_value());
        ensure("disable selected item",tree.setRadioIndexEnabled(*deselectable,1,false,error));
        ensure_equals("nearest enabled lower item selected",tree.value(*deselectable).asString(),std::string("first"));
        ensure("allow deselection",tree.selectRadioIndex(*deselectable,-1,false,error));
        ensure("deselected payload undefined",tree.value(*deselectable).isUndefined());
        LLVKControl::Callback remove;
        remove.function=[&](auto owner,const LLSD&) { tree.erase(owner,error); };
        tree.setControlCommit(*deselectable,remove);
        ensure("radio commit may delete owner",tree.radioKey(*deselectable,true,error));
        ensure("radio owner removed",tree.get(*deselectable)==nullptr);
    }

    template<> template<> void object::test<137>()
    {
        set_test_name("native spinner expression, clamp, validation and setting publication");
        LLVKWidgetTree tree;
        tree.defineSetting("Number",LLSD(2.f),LLVKWidgetTree::SettingType::Real);
        LLVKControl::Params control;
        control.font=loadFont(); control.valueSetting="Number";
        bool reject=false;
        int commits=0;
        control.validate.function=[&](auto,const LLSD&) { return !reject; };
        control.commit.function=[&](auto,const LLSD& value) { ++commits; ensure_equals("setting published before callback",tree.setting("Number")->asReal(),value.asReal()); };
        LLVKWidgetTree::SpinnerParams params;
        params.minimum=-10; params.maximum=10; params.increment=1; params.precision=2;
        params.buttonControl.font=params.editorControl.font=control.font;
        LLVKWidgetTree::Params view; view.rect={0,0,150,24};
        std::string error;
        const auto id=tree.createSpinner(view,control,params,0,error);
        ensure(error,id.has_value());
        const auto editor=tree.get(*id)->spinner->editor;
        ensure_equals("bound initial editor",tree.value(editor).asString(),std::string("2.00"));
        tree.setValue(editor,LLSD("2+3*2"));
        ensure("shared calculator expression",tree.commitSpinner(*id,error));
        ensure_equals("evaluated value",tree.value(*id).asReal(),8.);
        ensure("modified increment",tree.stepSpinner(*id,true,{false,true,false},error));
        ensure("control increment",std::abs(tree.value(*id).asReal()-8.1)<0.0001);
        reject=true;
        const auto before=tree.value(*id).asReal();
        ensure("validation veto",!tree.stepSpinner(*id,false,{},error));
        ensure_equals("veto restored value",tree.value(*id).asReal(),before);
        ensure_equals("veto did not publish",commits,2);
        reject=false;
        tree.setValue(editor,LLSD("100"));
        ensure("clamped expression",tree.commitSpinner(*id,error));
        ensure_equals("maximum",tree.value(*id).asReal(),10.);
        tree.setValue(editor,LLSD("SQRT(-1)"));
        ensure("invalid expression rejected",!tree.commitSpinner(*id,error));
        ensure_equals("invalid expression restores editor",tree.value(editor).asString(),std::string("10.00"));
        ensure("spinner focus forwards to editor",tree.requestControlFocus(*id,true,error));
        ensure_equals("editor owns keyboard focus",tree.keyboardFocus(),editor);
        ensure("external value while focused",tree.setSpinnerValue(*id,LLSD(3.f),false,error));
        ensure_equals("focused editor keeps draft",tree.value(editor).asString(),std::string("10.00"));
        ensure("focus leaves spinner",tree.requestControlFocus(*id,false,error));
        ensure_equals("focus loss reconciles clean editor",tree.value(editor).asString(),std::string("3.00"));
        tree.setEnabled(*id,false);
        ensure("disabled spinner editor read-only",tree.get(editor)->lineEditor->readOnly);
        ensure("disabled spinner does not step",!tree.stepSpinner(*id,true,{},error));
    }

    template<> template<> void object::test<136>()
    {
        set_test_name("native web text owns display ranges and release-note targets without GL URL owners");
        std::string error;
        const auto text=LLVKWebText::parse("Caf\xc3\xa9 [https://example.com/notes Release%20Notes] <nolink>https://hidden.example/a</nolink> https://example.com/path?q=1.",error);
        ensure(error,text.has_value());
        ensure("source wiki label and nolink contents",text->text==U"Caf\u00e9 Release Notes https://hidden.example/a https://example.com/path?q=1.");
        ensure_equals("label plus host and suffix links",text->links.size(),std::size_t(3));
        ensure_equals("Unicode display offset",text->links.front().begin,std::size_t(5));
        ensure_equals("release-note target retained",text->links.front().target,std::string("https://example.com/notes"));
        ensure("query suffix marked separately",!text->links[1].query && text->links[2].query);
        ensure_equals("punctuation not part of target",text->links.back().target,std::string("https://example.com/path?q=1"));
        const auto masked=LLVKWebText::parse("[https://actual.example/path https://other.example]",error);
        ensure("URL-shaped label cannot hide target",masked && masked->text==U"https://actual.example/path");
        const auto appLink=LLVKWebText::parse("[secondlife:///app/openfloater/preferences?tab=ui Interface]",error);
        ensure("application label",appLink && appLink->text==U"Interface");
        ensure_equals("application target preserved",appLink->links.front().target,std::string("secondlife:///app/openfloater/preferences?tab=ui"));
        ensure("bounded markup",!LLVKWebText::parse(std::string(65537,'a'),error));
        ensure("embedded NUL rejected",!LLVKWebText::parse(std::string("a\0b",3),error));
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect={0,0,180,70};
        LLVKControl::Params control;
        control.font=loadFont();
        control.initialValue="[https://example.com/notes Release Notes]";
        LLVKPlainControl::Params params;
        params.parseWebLinks=true;
        params.selectable=true;
        params.layout.wrap=true;
        std::string opened;
        params.linkClicked=[&](auto,const std::string& target) { opened=target; };
        const auto id=tree.createPlainText(view,control,params,0,error);
        ensure(error,id.has_value());
        ensure("link label replaces markup",tree.get(*id)->plainText->text==U"Release Notes");
        const auto paint=LLVKWidgetPaint::prepare(tree,*id,{},error);
        ensure(error,paint.has_value());
        ensure("native link color",paint->commands.front().color==params.linkColor.get());
        ensure("link underline emitted",std::any_of(paint->commands.begin(),paint->commands.end(),
            [](const auto& command) { return !command.text && command.rectangle.top-command.rectangle.bottom==1; }));
        LLVKWidgetTree::PointerEvent event;
        event.x=3; event.y=63; event.kind=LLVKWidgetTree::PointerKind::LeftDown;
        ensure("link press captured",tree.routePointer(*id,event,error));
        event.kind=LLVKWidgetTree::PointerKind::LeftUp;
        ensure("link release",tree.routePointer(*id,event,error));
        ensure_equals("native link dispatched",opened,std::string("https://example.com/notes"));
        ensure_equals("link releases capture",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        opened.clear();
        event.kind=LLVKWidgetTree::PointerKind::LeftDown;
        ensure("second link press",tree.routePointer(*id,event,error));
        event.kind=LLVKWidgetTree::PointerKind::LeftUp;
        event.x=179;
        tree.routePointer(*id,event,error);
        ensure("release outside link does not navigate",opened.empty());
        struct Clipboard final : LLVKClipboard
        {
            std::u32string copied;
            bool available(bool) const override { return false; }
            std::optional<std::u32string> read(bool,std::string&) override { return std::nullopt; }
            bool write(std::u32string_view text,bool,std::string&) override { copied=text; return true; }
        };
        auto clipboard=std::make_shared<Clipboard>();
        tree.setClipboard(clipboard);
        ensure("select readonly display text",tree.selectAllPlainText(*id));
        ensure("copy readonly selection",tree.copyPlainText(*id,error));
        ensure("clipboard contains label not markup",clipboard->copied==U"Release Notes");
        const auto selectedPaint=LLVKWidgetPaint::prepare(tree,*id,{},error);
        ensure(error,selectedPaint.has_value());
        ensure("selection background drawn",selectedPaint->commands.front().color==params.selectionBackground.get());
        event.kind=LLVKWidgetTree::PointerKind::LeftDown; event.x=2;
        ensure("selection drag begins",tree.routePointer(*id,event,error));
        event.kind=LLVKWidgetTree::PointerKind::Hover; event.x=50;
        ensure("selection drag updates",tree.routePointer(*id,event,error));
        event.kind=LLVKWidgetTree::PointerKind::LeftUp;
        tree.routePointer(*id,event,error);
        ensure("drag selects rather than activating link",opened.empty() && tree.get(*id)->plainText->selectionStart!=tree.get(*id)->plainText->selectionEnd);
    }

    template<> template<> void object::test<135>()
    {
        set_test_name("native border paint preserves bevel color order and two-pixel opaque alpha");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect={0,0,100,60};
        LLVKBorder::Params border;
        border.thickness=2;
        border.highlightLight=LLVKColor{1,0,0,0.2f};
        border.highlightDark=LLVKColor{0,1,0,0.3f};
        border.shadowLight=LLVKColor{0,0,1,0.4f};
        border.shadowDark=LLVKColor{0.2f,0.3f,0.4f,0.5f};
        std::string error;
        const auto id=tree.createBorder(view,border,0,error);
        ensure(error,id.has_value());
        const auto paint=LLVKWidgetPaint::prepare(tree,*id,{},error);
        ensure(error,paint.has_value());
        ensure_equals("two-pixel eight edges",paint->commands.size(),std::size_t(8));
        ensure("outer highlight RGB alpha one",paint->commands[0].color==LLVKColor::Value{0,1,0,1});
        ensure("inner highlight",paint->commands[2].color==LLVKColor::Value{1,0,0,1});
        ensure("outer shadow",paint->commands[4].color==LLVKColor::Value{0.2f,0.3f,0.4f,1});
        ensure("inner shadow",paint->commands[6].color==LLVKColor::Value{0,0,1,1});
        border.thickness=1;
        border.bevel=LLVKBorder::Bevel::In;
        const auto inset=tree.createBorder(view,border,0,error);
        ensure(error,inset.has_value());
        const auto one=LLVKWidgetPaint::prepare(tree,*inset,{},error);
        ensure(error,one.has_value());
        ensure_equals("one-pixel four edges",one->commands.size(),std::size_t(4));
        ensure("one-pixel inset shadow preserves alpha",one->commands[0].color==border.shadowDark.get());
        ensure("one-pixel inset highlight preserves alpha",one->commands[2].color==border.highlightLight.get());
        border.thickness=0;
        const auto invisible=tree.createBorder(view,border,0,error);
        ensure(error,invisible.has_value());
        const auto empty=LLVKWidgetPaint::prepare(tree,*invisible,{},error);
        ensure("zero border emits no edges",empty && empty->commands.empty());
    }

    template<> template<> void object::test<134>()
    {
        set_test_name("native tab selection validates panel names and commits after visibility changes");
        LLVKWidgetTree tree;
        LLVKControl::Params control;
        control.font=loadFont();
        LLVKWidgetTree::Params view;
        view.rect={0,0,200,100};
        std::string error;
        bool reject=false;
        control.validate.function=[&](auto,const LLSD& name) { ensure("validation receives panel name",name.asString()=="first" || name.asString()=="second"); return !reject; };
        const auto container=tree.createPanel(view,control,{},0,error);
        ensure(error,container.has_value());
        ensure("native tab state",tree.initializeTabContainer(*container,error));
        control.validate={};
        view.name="first";
        const auto first=tree.createPanel(view,control,{},*container,error);
        view.name="second";
        const auto second=tree.createPanel(view,control,{},*container,error);
        const auto firstButton=tree.createButton(view,control,{},*container,error);
        const auto secondButton=tree.createButton(view,control,{},*container,error);
        ensure(error,first && second && firstButton && secondButton);
        ensure("attach first",tree.attachTabPanel(*container,*first,*firstButton,error));
        ensure("attach second",tree.attachTabPanel(*container,*second,*secondButton,error));
        LLVKWidgetTree::Node::TabContainer::Layout layout;
        layout.panelOverlap=3;
        layout.horizontalPadding=2;
        ensure("source top-tab geometry",tree.layoutTopTabs(*container,layout,error));
        ensure("panel uses tab content bounds",tree.get(*first)->params.rect == LLVKWidgetTree::Rect{1,1,199,81});
        ensure("button uses top strip",tree.get(*firstButton)->params.rect == LLVKWidgetTree::Rect{3,79,63,100});
        ensure("resize tab owner",tree.reshape(*container,300,140,error));
        ensure("resized tab geometry",tree.layoutTopTabs(*container,layout,error));
        ensure_equals("resized panel top",tree.get(*first)->params.rect.top,121);
        layout.minimumWidth=layout.maximumWidth=200;
        const auto beforeLayout=tree.get(*firstButton)->params.rect;
        ensure("overflow remains explicit",!tree.layoutTopTabs(*container,layout,error));
        ensure("rejected layout preserves geometry",tree.get(*firstButton)->params.rect==beforeLayout);
        layout.minimumWidth=125;
        layout.maximumWidth=160;
        layout.position=LLVKWidgetTree::Node::TabContainer::Layout::Position::Left;
        layout.rightPadding=4;
        ensure("left tabs for Preferences",tree.layoutTabPanels(*container,layout,error));
        ensure("source left panel bounds",tree.get(*first)->params.rect==LLVKWidgetTree::Rect{131,1,299,139});
        ensure("source vertical button geometry",tree.get(*firstButton)->params.rect==LLVKWidgetTree::Rect{3,114,128,137});
        layout.position=LLVKWidgetTree::Node::TabContainer::Layout::Position::Bottom;
        layout.minimumWidth=60;
        ensure("bottom tabs",tree.layoutTabPanels(*container,layout,error));
        ensure("source bottom panel bounds",tree.get(*first)->params.rect==LLVKWidgetTree::Rect{1,18,299,139});
        ensure_equals("bottom tab offset",tree.get(*firstButton)->params.rect.bottom,1);
        ensure("panels hidden before selection",!tree.get(*first)->params.visible && !tree.get(*second)->params.visible);
        int commits=0;
        LLVKControl::Callback callback;
        callback.function=[&](auto,const LLSD& name)
        {
            ++commits;
            const bool selectedFirst=name.asString()=="first";
            ensure("commit sees new panel visibility",tree.get(*first)->params.visible==selectedFirst && tree.get(*second)->params.visible!=selectedFirst);
        };
        tree.setControlCommit(*container,callback);
        ensure("select first",tree.selectTabPanel(*container,*first,error));
        reject=true;
        ensure("selection veto",!tree.selectTabPanel(*container,*second,error));
        ensure("veto preserves selected panel",tree.get(*first)->params.visible);
        ensure_equals("veto does not commit",commits,1);
        reject=false;
        ensure("button selects second",tree.commit(*secondButton));
        ensure("only selected tab in keyboard traversal",!tree.get(*firstButton)->control->params.tabStop && tree.get(*secondButton)->control->params.tabStop);
        ensure("tab strip focus",tree.requestControlFocus(*secondButton,true,error));
        ensure("right wraps to first tab",tree.tabContainerKey(*container,LLVKWidgetTree::ScrollKey::Right,{},error));
        ensure_equals("arrow keeps tab focus",tree.keyboardFocus(),*firstButton);
        ensure("numeric tab selection",tree.setValue(*container,LLSD(1)));
        ensure_equals("numeric value selects second",tree.get(*container)->tabContainer->selected,*second);
        ensure("invalid index rejected",!tree.setValue(*container,LLSD(-1)));
        layout.position=LLVKWidgetTree::Node::TabContainer::Layout::Position::Left;
        ensure("vertical navigation layout",tree.layoutTabPanels(*container,layout,error));
        ensure("vertical strip focused",tree.requestControlFocus(*secondButton,true,error));
        ensure("up selects previous vertical tab",tree.tabContainerKey(*container,LLVKWidgetTree::ScrollKey::Up,{},error));
        ensure_equals("vertical tab focus follows",tree.keyboardFocus(),*firstButton);
        ensure("right enters selected vertical panel",tree.tabContainerKey(*container,LLVKWidgetTree::ScrollKey::Right,{},error));
        ensure_equals("vertical panel receives focus",tree.keyboardFocus(),*first);
        callback.function=[&](auto id,const LLSD&) { tree.erase(id,error); };
        tree.setControlCommit(*container,callback);
        ensure("commit may erase tab owner",tree.selectTabPanel(*container,*first,error));
        ensure_equals("tab subtree retired",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<133>()
    {
        set_test_name("native scroll painter clips document before painting scrollbar controls");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect={0,0,120,100};
        LLVKControl::Params control;
        control.font=loadFont();
        LLVKWidgetTree::ScrollContainerParams container;
        container.size=16;
        container.scrollbarControl=control;
        container.vertical.decreaseControl=container.vertical.increaseControl=control;
        container.horizontal.decreaseControl=container.horizontal.increaseControl=control;
        const auto root=tree.createScrollContainer(view,control,container,0,error);
        ensure(error,root.has_value());
        view.rect={0,0,100,300};
        LLVKPanel::Params panel;
        panel.backgroundVisible=panel.backgroundOpaque=true;
        panel.opaqueColor=LLVKColor{1,0,0,1};
        const auto document=tree.createPanel(view,control,panel,*root,error);
        ensure(error,document.has_value());
        ensure("attach scroll document",tree.attachScrollContent(*root,*document,0,error));
        const auto paint=LLVKWidgetPaint::prepare(tree,*root,{},error);
        ensure(error,paint.has_value());
        const auto vertical=tree.get(*root)->scrollContainer->vertical;
        ensure("vertical scrollbar visible",tree.get(vertical)->params.visible);
        bool foundDocument=false,foundScrollbar=false;
        for (const auto& command : paint->commands)
        {
            if (command.owner==*document)
            {
                foundDocument=true;
                ensure("document cannot paint over scrollbar",command.clip.right<=104);
                ensure("document vertically clipped",command.clip.bottom>=0 && command.clip.top<=100);
            }
            if (command.owner==vertical)
            {
                ensure("scrollbar paints after document",foundDocument);
                ensure_equals("scrollbar retains outer clip",command.clip.right,120);
                foundScrollbar=true;
            }
        }
        ensure("document and scrollbar emitted",foundDocument && foundScrollbar);
        ensure("wheel moves document",tree.routeWheel(*root,50,50,1,false,error));
        ensure("scrolled paint succeeds",LLVKWidgetPaint::prepare(tree,*root,{},error).has_value());
    }

    template<> template<> void object::test<132>()
    {
        set_test_name("native preference persistence merges saved changes without saving transient overrides");
        const auto directory=std::filesystem::temp_directory_path()/"vulkanstorm-settings-test";
        std::filesystem::create_directories(directory);
        const auto path=directory/"settings.xml";
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::filesystem::remove(path/"settings.xml"); std::filesystem::remove(path); } } cleanup{directory};
        std::ofstream file(path);
        file << "<llsd><map><key>Unrelated</key><map><key>Type</key><string>String</string><key>Value</key><string>keep</string></map></map></llsd>";
        file.close();
        LLVKStartupSettings settings;
        std::string error;
        ensure("defaults",settings.load("<llsd><map><key>RenderBackend</key><map><key>Type</key><string>String</string>"
            "<key>Value</key><string>OpenGL</string><key>Comment</key><string>Backend</string></map>"
            "<key>RememberPassword</key><map><key>Type</key><string>Boolean</string><key>Value</key><boolean>false</boolean></map></map></llsd>",true,true,error));
        ensure("transient renderer",settings.set("RenderBackend",LLSD("Vulkan"),false,error));
        ensure("save changed flag",settings.saveChanges(path,{{"RememberPassword",LLSD(true)}},error));
        LLVKStartupSettings reloaded;
        ensure("reload",reloaded.loadFile(path,true,false,true,error));
        ensure_equals("unrelated retained",reloaded.find("Unrelated")->value().asString(),std::string("keep"));
        ensure("changed flag retained",reloaded.find("RememberPassword")->value().asBoolean());
        ensure("transient backend excluded",reloaded.find("RenderBackend") == nullptr);
        ensure("explicit backend change",settings.saveChanges(path,{{"RenderBackend",LLSD("Zink")}},error));
        ensure("reload backend",reloaded.loadFile(path,true,false,true,error));
        ensure_equals("backend persisted",reloaded.find("RenderBackend")->value().asString(),std::string("Zink"));
        std::ofstream corrupt(path); corrupt << "not settings"; corrupt.close();
        ensure("malformed file preserved",!settings.saveChanges(path,{{"RenderBackend",LLSD("OpenGL")}},error));
        ensure_equals("failed save leaves memory unchanged",settings.find("RenderBackend")->value().asString(),std::string("Zink"));
    }

    template<> template<> void object::test<131>()
    {
        set_test_name("native login dialogs paint and preserve Cancel and accepted settings transactions");
        LLVKLoginUi::Configuration configuration;
        const auto fonts = std::filesystem::path(LLVK_WIDGET_FONT_FIXTURE).parent_path();
        configuration.skin.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.fontDescription = fonts/"fonts.xml";
        configuration.fonts.platform = "Windows";
        configuration.fonts.searchDirectories = {fonts,std::filesystem::path(LLVK_WIDGET_PACKAGED_FONTS)};
        configuration.settings = {{"FSRememberUsername",LLSD(true)},{"RememberPassword",LLSD(false)},
            {"RenderBackend",LLSD("Vulkan")},{"RenderBackendPending",LLSD("Vulkan")},{"UIScrollbarSize",LLSD(16)}};
        configuration.settings["SessionSettingsFile"]="settings_phoenix.xml";
        configuration.settings["RestrainedLove"]=LLSD(false);
        configuration.settings["PreferredMaturity"]=LLSD(21);
        configuration.settings["UsePeopleAPI"]=LLSD(false);
        configuration.settings["UseDisplayNames"]=LLSD(true);
        configuration.settingDefaults["RememberPassword"]=LLSD(false);
        configuration.settings["ShowAdultSims"]=LLSD(true);
        configuration.appliedSettingsMode="settings_firestorm.xml";
        std::map<std::string,LLSD> saved;
        configuration.savePreferences = [&](const auto& values,std::string&) { saved=values; return true; };
        std::string error;
        auto login = LLVKLoginUi::create(configuration,error);
        ensure(error,login != nullptr);
        auto& tree=login->tree();
        ensure("RLVa startup does not fabricate an activation notice",login->takeNotices().empty());
        ensure("RLVa pre-login preference enabled",tree.updateSetting("RestrainedLove",LLSD(true)));
        auto rlvNotices=login->takeNotices();
        ensure_equals("one RLVa change notice",rlvNotices.size(),std::size_t(1));
        ensure_equals("original RLVa notification type",rlvNotices.front().name,std::string("GenericAlert"));
        ensure_equals("original pre-login RLVa enable message",rlvNotices.front().message,std::string("RLVa has been enabled (no restart required)"));
        ensure("equal RLVa value accepted",tree.updateSetting("RestrainedLove",LLSD(true)));
        ensure("equal RLVa setting has no duplicate notice",login->takeNotices().empty());
        ensure("RLVa setting rollback",tree.updateSetting("RestrainedLove",LLSD(false)));
        rlvNotices=login->takeNotices();
        ensure_equals("original pre-login RLVa disable message",rlvNotices.front().message,std::string("RLVa has been disabled (no restart required)"));
        const auto password=login->find("password_edit");
        ensure("focus before dialog",tree.requestControlFocus(password,true,error));
        ensure("RLVa enable produces modal",tree.updateSetting("RestrainedLove",LLSD(true)));
        ensure("show queued RLVa modal",login->advanceNotices(1.0,error));
        const auto modal=login->modalNotice();
        ensure("RLVa modal exists",modal!=0);
        ensure("RLVa modal paints",login->preparePaint({},error).has_value());
        ensure("modal locks underlying focus",!tree.requestControlFocus(password,true,error));
        ensure("early Return consumed",login->noticeKey(true,false,error));
        ensure_equals("source default action delay",login->modalNotice(),modal);
        ensure("notice delay elapses",login->advanceNotices(1.5,error));
        ensure("Return dismisses notice",login->noticeKey(true,false,error));
        ensure_equals("notice closed",login->modalNotice(),LLVKWidgetTree::Id(0));
        ensure_equals("notice restores editor focus",tree.keyboardFocus(),password);
        ensure("Preferences shortcut",login->menu().shortcut("P",true,false,false));
        ensure(login->dialogError(),login->dialogError().empty());
        const auto preferences=login->activeFloater();
        ensure("Preferences visible",preferences != 0);
        ensure("Preferences painter",login->preparePaint({},error).has_value());
        ensure("change bound remember preference",tree.updateSetting("FSRememberUsername",LLSD(false)));
        ensure("Cancel",login->closeFloater(error));
        ensure("Cancel restores snapshot",tree.setting("FSRememberUsername")->asBoolean());
        ensure_equals("close restores focus",tree.keyboardFocus(),password);
        ensure("Cancel never saves",saved.empty());
        ensure("reopen",login->showPreferences(error));
        ensure_equals("single instance",login->activeFloater(),preferences);
        ensure("change remember preference",tree.updateSetting("RememberPassword",LLSD(true)));
        ensure("accept",login->applyPreferences(error));
        ensure("accepted value persisted",saved["RememberPassword"].asBoolean());
        ensure("open for source default reset",login->showPreferences(error));
        ensure("reset to loaded default",login->resetPreference("RememberPassword",error));
        ensure("reset differs from saved value",!tree.setting("RememberPassword")->asBoolean());
        ensure("Cancel reverses reset",login->closeFloater(error));
        ensure("Cancel retains accepted value",tree.setting("RememberPassword")->asBoolean());
        ensure("unknown defaults rejected",!login->resetPreference("MissingDefault",error));
        LLSD initialStructured=LLSD::emptyArray(); initialStructured.append(1.0); initialStructured.append(0.5);
        ensure("structured preference definition",tree.defineSetting("StructuredPreference",initialStructured));
        LLVKWidgetTree::Params structuredView; structuredView.rect={0,0,1,1}; structuredView.visible=false;
        LLVKControl::Params structuredControl; structuredControl.font=tree.get(password)->control->params.font;
        structuredControl.valueSetting="StructuredPreference";
        const auto structured=tree.createControl(structuredView,structuredControl,preferences,error);
        ensure(error,structured.has_value());
        ensure("snapshot structured preference",login->showPreferences(error));
        auto changedStructured=initialStructured; changedStructured[1]=0.75;
        ensure("structured preference changed",tree.updateSetting("StructuredPreference",changedStructured));
        ensure("accept structured preference",login->applyPreferences(error));
        ensure("structured change is persisted",saved.contains("StructuredPreference"));
        ensure_equals("structured setting retains changed channel",saved.at("StructuredPreference")[1].asReal(),0.75);
        ensure("snapshot unchanged structured setting",login->showPreferences(error));
        saved.clear();
        ensure("accept unchanged structured preference",login->applyPreferences(error));
        ensure("unchanged structured setting does not save",saved.empty());
        ensure("remove structured fixture",tree.erase(*structured,error));
        ensure("About opens",login->showAbout(error));
        ensure_equals("localized About title",tree.value(login->find("floater_title")).asString(),std::string("About Vulkanstorm"));
        const auto closeRect=tree.get(login->find("floater_close"))->params.rect;
        ensure_equals("close button at original header",closeRect.top,596);
        LLSD aboutInfo;
        aboutInfo["VIEWER_VERSION"]=LLSD::emptyArray();
        for (const auto value : {"7","2","5","test"}) aboutInfo["VIEWER_VERSION"].append(value);
        aboutInfo["RENDERING_API"]="Vulkan";
        aboutInfo["RENDERING_API_VERSION"]="1.4";
        aboutInfo["CPU"]="fixture CPU";
        aboutInfo["BANDWIDTH"]=3000;
        aboutInfo["LIBCURL_VERSION"]="curl fixture";
        ensure("source About formatter",login->setAboutInfo(aboutInfo,error));
        const auto support=login->find("support_editor");
        const auto supportBody=tree.get(support)->textEditor->body;
        ensure("support URLs enabled by original declaration",tree.get(supportBody)->plainText->params.parseWebLinks);
        const auto licenseBody=tree.get(login->find("licenses_editor"))->textEditor->body;
        ensure("license URLs remain literal",!tree.get(licenseBody)->plainText->params.parseWebLinks);
        ensure("source read-only background stays transparent",tree.get(supportBody)->plainText->params.readOnlyBackground.get()[3]==0.f);
        const auto formatted=tree.get(supportBody)->plainText->text;
        ensure("source Info header",formatted.find(U"Vulkanstorm 7.2.5 (test)")!=std::u32string::npos);
        ensure("source Info renderer line",formatted.find(U"Rendering API: Vulkan\nVersion: 1.4")!=std::u32string::npos);
        ensure("source Info system field",formatted.find(U"CPU: fixture CPU")!=std::u32string::npos);
        ensure("inactive RLVa uses source state",formatted.find(U"RestrainedLove API: (disabled)")!=std::u32string::npos);
        ensure("absent audio uses source state",formatted.find(U"Audio Driver Version: Undefined")!=std::u32string::npos);
        aboutInfo["AUDIO_DRIVER_VERSION"]="OpenAL, version active fixture";
        ensure("active audio metadata",login->setAboutInfo(aboutInfo,error));
        ensure("active audio not replaced by fallback",tree.get(supportBody)->plainText->text.find(U"Audio Driver Version: OpenAL, version active fixture")!=std::u32string::npos);
        ensure("curl version from producer",formatted.find(U"libcurl Version: curl fixture")!=std::u32string::npos);
        ensure("actual native J2C provider",formatted.find(U"J2C Decoder Version: OpenJPEG:")!=std::u32string::npos);
        ensure("mode reports applied preset",formatted.find(U"Settings mode: Vulkanstorm")!=std::u32string::npos);
        ensure("unapplied mode not reported",formatted.find(U"Settings mode: Phoenix")==std::u32string::npos);
        ensure("reopen Info resets scroll after focus",login->showAbout(error));
        const auto infoScroll=tree.get(tree.get(support)->textEditor->scroller)->scrollContainer->vertical;
        ensure_equals("Info opens at document start",tree.get(infoScroll)->scrollbar->position,0);
        ensure("Info data refresh",login->setAboutInfo(aboutInfo,error));
        ensure_equals("Info refresh preserves document start",tree.get(infoScroll)->scrollbar->position,0);
        login->setAboutInfo("Vulkanstorm test\nRenderer: native Vulkan\nDevice: fixture\nNot connected");
        struct Clipboard final : LLVKClipboard
        {
            std::u32string copied;
            bool available(bool) const override { return false; }
            std::optional<std::u32string> read(bool,std::string&) override { return std::nullopt; }
            bool write(std::u32string_view text,bool,std::string&) override { copied=text; return true; }
        };
        auto clipboard=std::make_shared<Clipboard>();
        login->setDialogClipboard(clipboard);
        ensure("original About Copy action",tree.commit(login->find("copy_btn")));
        ensure("Copy exports editor display text",clipboard->copied==tree.get(supportBody)->plainText->text);
        ensure("Copy deselects original editor",tree.get(supportBody)->plainText->selectionStart==0 && tree.get(supportBody)->plainText->selectionEnd==0);
        ensure("About painter",login->preparePaint({},error).has_value());
        const auto tabs=login->find("about_tab");
        const auto& tabItems=tree.get(tabs)->tabContainer->tabs;
        ensure_equals("original About tab count",tabItems.size(),std::size_t(4));
        ensure("credits tab",tree.commit(tabItems[1].button));
        ensure(login->dialogError(),login->dialogError().empty());
        ensure("Linden introduction stays outside scroll document",tree.get(login->find("linden_intro"))->plainText->text.find(U"Firestorm would not be possible")!=std::u32string::npos);
        ensure("Copy belongs only to Info",tree.get(login->find("copy_btn"))->parent==login->find("support_panel"));
        ensure("Info panel hidden on credits",!tree.get(login->find("support_panel"))->params.visible);
        ensure("credits painter",login->preparePaint({},error).has_value());
        ensure("Firestorm tab",tree.commit(tabItems[2].button));
        ensure(login->dialogError(),login->dialogError().empty());
        const auto body=login->find("firestorm_intro");
        ensure("actual credits text loaded",tree.get(body)->plainText->text.find(U"community development project") != std::u32string::npos);
        ensure("Firestorm uses its own scrolling introduction",tree.get(body)->parent==login->find("fs_credits_scroll_container_content_panel"));
        const auto creditsPaint=login->preparePaint({},error);
        ensure(error,creditsPaint.has_value());
        bool visibleGlyph=false;
        for (const auto& command : creditsPaint->commands)
            if (command.owner==body && command.text)
                for (const auto& glyph : command.text->glyphs)
                    if (glyph.left<command.clip.right && glyph.right>command.clip.left && glyph.bottom<command.clip.top && glyph.top>command.clip.bottom)
                        visibleGlyph=true;
        ensure("credits glyphs intersect visible body",visibleGlyph);
        const auto scroll=login->find("fs_credits_scroll_container");
        const auto vertical=tree.get(scroll)->scrollContainer->vertical;
        ensure("Firestorm scrollbar visible",tree.get(vertical)->params.visible);
        ensure("Firestorm scrollbar painted",std::any_of(creditsPaint->commands.begin(),creditsPaint->commands.end(),
            [vertical](const auto& command) { return command.owner==vertical; }));
        const auto wholeText=tree.get(body)->plainText->text;
        const auto scrollRect=tree.screenRect(scroll,error);
        ensure(error,scrollRect.has_value());
        ensure("Firestorm wheel scrolls",login->floaterWheel(scrollRect->left+10,scrollRect->top-10,3,error));
        ensure("scrollbar position advances",tree.get(vertical)->scrollbar->position>0);
        ensure("scrolling preserves full credits",tree.get(body)->plainText->text==wholeText);
        const auto creditsPosition=tree.get(vertical)->scrollbar->position;
        ensure("license tab",tree.commit(tabItems[3].button));
        const auto licenses=login->find("licenses_editor");
        const auto licenseScroll=tree.get(tree.get(licenses)->textEditor->scroller)->scrollContainer->vertical;
        ensure_equals("license document starts at top",tree.get(licenseScroll)->scrollbar->position,0);
        ensure("license text exists",!tree.value(licenses).asString().empty());
        ensure("return to credits",tree.commit(tabItems[2].button));
        ensure_equals("credits retain independent scroll position",tree.get(vertical)->scrollbar->position,creditsPosition);
        ensure("About closes",login->closeFloater(error));
        const auto generalPanel=login->constructPreferencePanel("panel_preferences_general.xml",login->root(),error);
        ensure(error,generalPanel.has_value());
        ensure("startup maturity choice disabled",!tree.get(login->find("maturity_desired_combobox"))->params.enabled);
        ensure_equals("saved maturity choice displayed",tree.value(login->find("maturity_desired_textbox")).asString(),std::string("General and Moderate"));
        ensure("saved moderate rating shown",tree.get(login->find("rating_icon_moderate"))->params.visible);
        ensure("adult rating hidden",!tree.get(login->find("rating_icon_adult"))->params.visible);
        ensure("display name control follows People API",!tree.get(login->find("display_names_check"))->params.enabled);
        ensure("unavailable display-name option unchecked",!tree.value(login->find("display_names_check")).asBoolean());
        ensure("display-name preference itself not overwritten",tree.setting("UseDisplayNames")->asBoolean());
        ensure("startup access constrains legacy search",!tree.setting("ShowAdultSims")->asBoolean());
        ensure("General application panel paints",login->preparePaint({},error).has_value());
        ensure("remove tested General panel",tree.erase(*generalPanel,error));
        const auto colorPanel=login->constructPreferencePanel("panel_preferences_colors.xml",login->root(),error);
        ensure(error,colorPanel.has_value());
        ensure("native application owner installs alpha policy",!tree.get(*colorPanel)->preferenceLocalValues.empty());
        const auto localSnapshot=tree.snapshotPreferences(*colorPanel,error);
        ensure(error,localSnapshot.has_value());
        const auto localAlpha=tree.get(*colorPanel)->preferenceLocalValues.front();
        ensure("live owner alpha edit",tree.setValue(localAlpha,LLSD(0.4)));
        ensure("live owner alpha callback",tree.commit(localAlpha));
        ensure("live owner Colors restoration",tree.restorePreferences(*localSnapshot,{},error));
        ensure_equals("live local slider restored",tree.value(localAlpha).asReal(),localSnapshot->localValues.at(localAlpha).asReal());
        ensure("remove tested application panel",tree.erase(*colorPanel,error));
        LLVKWidgetTree::Params swatchView; swatchView.rect={10,10,90,70};
        LLVKControl::Params swatchControl; swatchControl.font=tree.get(password)->control->params.font;
        LLVKWidgetTree::ColorSwatchParams swatchParams; swatchParams.color=LLVKColor{0.4f,0.5f,0.6f,1.f};
        const auto swatch=tree.createColorSwatch(swatchView,swatchControl,swatchParams,login->root(),error);
        ensure(error,swatch.has_value());
        ensure("native dialog picker opens",login->showColorPicker(*swatch,true,error));
        const auto picker=login->activeFloater();
        ensure("native dialog owns original picker",tree.get(picker)->colorPicker.has_value());
        ensure("native dialog picker paints",login->preparePaint({},error).has_value());
        ensure("picker close cancels",login->closeFloater(error));
        ensure("picker transaction ended",!tree.get(*swatch)->colorSwatch->picking);
        ensure("same swatch reopens picker",login->showColorPicker(*swatch,false,error));
        ensure_equals("picker identity retained",login->activeFloater(),picker);
        ensure("disable swatch",tree.setEnabled(*swatch,false));
        ensure("disabled swatch closes its picker",!tree.get(picker)->params.visible);
        ensure("disabled swatch cancels transaction",!tree.get(*swatch)->colorSwatch->picking);
        ensure("reenable swatch",tree.setEnabled(*swatch,true));
        ensure("open before owner removal",login->showColorPicker(*swatch,false,error));
        ensure("remove swatch owner",tree.erase(*swatch,error));
        ensure("removed swatch leaves picker closed",!tree.get(picker)->params.visible);
    }

    template<> template<> void object::test<130>()
    {
        set_test_name("native login menu paints at top and routes popup selection without GL menu owners");
        std::ifstream file(std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE)/"xui"/"en"/"menu_login.xml");
        ensure("packaged login menu",file.good());
        const std::string xml{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        std::string error;
        auto menu = LLVKLoginMenu::create(xml,loadFont(),{}, {},false,error);
        ensure(error,menu != nullptr);
        int quits = 0;
        menu->bind("File.Quit",[&](const auto&,const auto&) { ensure("menu dismissed before action",!menu->open()); ++quits; });
        LLVKWidgetPaint paint;
        ensure("menu paint",menu->paint(paint,{0,0,1024,768},error));
        ensure_equals("menu bar top",paint.commands[0].rectangle.top,768);
        ensure_equals("menu bar height",paint.commands[0].rectangle.bottom,750);
        ensure_equals("bar and two visible headings",paint.commands.size(),std::size_t(3));
        LLVKWidgetTree::PointerEvent event;
        event.x = 15; event.y = 759; event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        ensure("Viewer opens on press",menu->pointer(event) && menu->open());
        paint = {};
        ensure("popup paint",menu->paint(paint,{0,0,1024,768},error));
        ensure("popup adds rows",paint.commands.size() > 3);
        ensure("down selects enabled exit skipping preferences",menu->key(LLVKLoginMenu::Key::Down));
        ensure("return activates exit",menu->key(LLVKLoginMenu::Key::Return));
        ensure_equals("exit dispatched once",quits,1);
        menu->key(LLVKLoginMenu::Key::Activate);
        menu->key(LLVKLoginMenu::Key::Right);
        paint = {};
        ensure("Help popup paint",menu->paint(paint,{0,0,1280,900},error));
        ensure_equals("menu follows resize",paint.commands[0].rectangle.top,900);
        event.x = 900; event.y = 200;
        ensure("outside press consumed",menu->pointer(event));
        ensure("outside press dismisses",!menu->open());
        ensure("bound exit shortcut",menu->shortcut("Q",true,false,false));
        ensure_equals("shortcut dispatches",quits,2);
        ensure("unbound Preferences shortcut unavailable",!menu->shortcut("P",true,false,false));
        ensure("unmodified letter is not a shortcut",!menu->shortcut("Q",false,false,false));
        ensure("DTD rejected",!LLVKLoginMenu::create("<!DOCTYPE menu_bar><menu_bar/>",loadFont(),{}, {},false,error));
    }

    template<> template<> void object::test<129>()
    {
        set_test_name("native login page preserves existing query and encodes viewer metadata");
        LLVKLoginUi::Page page;
        page.url = "https://example.com/login/?existing=yes";
        page.language = "en";
        page.version = "7.2.5 (79279)";
        page.channel = "Vulkanstorm Test";
        page.grid = "agni";
        page.operatingSystem = "Win";
        page.skin = "default";
        page.settings = {{"FirstLoginThisInstall",LLSD(true)},{"FSSplashScreenHideBlogs",LLSD(true)}};
        const LLURI uri(LLVKLoginUi::pageUrl(page));
        const auto query = uri.queryMap();
        ensure_equals("original query retained",query["existing"].asString(),std::string("yes"));
        ensure_equals("version encoded and recovered",query["version"].asString(),page.version);
        ensure_equals("first login source spelling",query["firstlogin"].asString(),std::string("TRUE"));
        ensure_equals("splash preference",query["hideblogs"].asString(),std::string("1"));
        ensure_equals("unset splash preference",query["hidetopbar"].asString(),std::string("0"));
    }

    template<> template<> void object::test<128>()
    {
        set_test_name("native startup settings preserve default saved and transient precedence");
        LLVKStartupSettings settings;
        std::string error;
        const auto document = [](const std::string& value)
        { return "<llsd><map><key>RenderBackend</key><map><key>Type</key><string>String</string><key>Value</key><string>"+value+"</string></map></map></llsd>"; };
        ensure("initial default",settings.load(document("OpenGL"),true,true,error));
        ensure("user override",settings.load(document("Vulkan"),false,true,error));
        ensure_equals("saved backend",settings.find("RenderBackend")->saveValue().asString(),std::string("Vulkan"));
        ensure("session defaults reset active layers",settings.load(document("Zink"),true,false,error));
        ensure_equals("session default active",settings.find("RenderBackend")->value().asString(),std::string("Zink"));
        ensure("user reloaded after mode",settings.load(document("Vulkan"),false,true,error));
        ensure("command line transient override",settings.set("RenderBackend",LLSD("OpenGL"),false,error));
        ensure_equals("command line wins",settings.find("RenderBackend")->value().asString(),std::string("OpenGL"));
        ensure_equals("command line not saved",settings.find("RenderBackend")->saveValue().asString(),std::string("Vulkan"));
        ensure("malformed load rejected",!settings.load("<llsd><array/></llsd>",true,true,error));
        ensure_equals("failure preserves active settings",settings.find("RenderBackend")->value().asString(),std::string("OpenGL"));
        const auto appSettings = std::filesystem::path(LLVK_WIDGET_FONT_FIXTURE).parent_path().parent_path()/"app_settings"/"settings.xml";
        LLVKStartupSettings packaged;
        ensure("actual default settings parse",packaged.loadFile(appSettings,true,true,true,error));
        ensure("native backend setting exists",packaged.find("RenderBackend") != nullptr);
        ensure("Boolean defaults converted",packaged.find("FSRememberUsername")->value().isBoolean());
    }

    template<> template<> void object::test<127>()
    {
        set_test_name("native login resource owner resolves real font sizes and packaged controls");
        LLVKLoginUi::Configuration configuration;
        const auto fonts = std::filesystem::path(LLVK_WIDGET_FONT_FIXTURE).parent_path();
        configuration.skin.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.fontDescription = fonts/"fonts.xml";
        configuration.fonts.platform = "Windows";
        configuration.fonts.searchDirectories = {fonts,std::filesystem::path(LLVK_WIDGET_PACKAGED_FONTS)};
        configuration.settings = {{"FSRememberUsername",LLSD(true)},{"RememberPassword",LLSD(false)},
            {"NextLoginLocation",LLSD("home")},{"UIResizeBarHeight",LLSD(3)}};
        std::string error;
        auto login = LLVKLoginUi::create(configuration,error);
        ensure(error,login != nullptr);
        const auto& tree = login->tree();
        const auto password = login->find("password_edit"), link = login->find("forgot_password_text");
        ensure("real different font sizes",tree.get(password)->control->params.font->metrics().lineHeight > tree.get(link)->control->params.font->metrics().lineHeight);
        ensure_equals("bound location applies after construction",tree.value(login->find("start_location_combo")).asString(),std::string("home"));
        ensure("native browser retained",tree.get(login->find("login_html"))->browser.has_value());
        ensure("login paint with real fonts",LLVKWidgetPaint::prepare(login->tree(),login->root(),{},error).has_value());
        const auto location = login->find("start_location_combo");
        ensure("open actual location popup",login->tree().showComboList(location,error));
        const auto popup = tree.get(location)->combo->list;
        const auto painted = LLVKWidgetPaint::prepare(login->tree(),login->root(),{},error);
        ensure(error,painted.has_value());
        ensure_equals("popup painted above tree",painted->commands.back().owner,popup);
        std::size_t rows = 0;
        for (const auto& command : painted->commands) if (command.owner == popup && command.text) ++rows;
        ensure_equals("actual location rows painted",rows,std::size_t(3));
        login->tree().setValue(password,LLSD("mask-test"));
        const auto cursor = tree.get(password)->lineEditor->text.cursor();
        ensure("show password action",login->tree().commit(login->find("password_show_btn")));
        ensure("mask disabled",!tree.get(password)->lineEditor->params.text.password);
        ensure("hide action shown",tree.get(login->find("password_hide_btn"))->params.visible);
        ensure("hide password action",login->tree().commit(login->find("password_hide_btn")));
        ensure("mask restored",tree.get(password)->lineEditor->params.text.password);
        ensure_equals("toggle preserves password value",tree.value(password).asString(),std::string("mask-test"));
        ensure_equals("toggle preserves cursor",tree.get(password)->lineEditor->text.cursor(),cursor);
    }

    template<> template<> void object::test<126>()
    {
        set_test_name("native password editor prepares masked selection and focus-sensitive caret");
        LLVKWidgetTree tree;
        tree.defineSetting("UILineEditorCursorThickness",LLSD(2),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,201,32};
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = "secret";
        LLVKWidgetTree::LineEditorParams params;
        params.text.password = true;
        params.text.leftPadding = params.text.rightPadding = 8;
        params.background = image("normal");
        params.focusedBackground = image("focused");
        params.textColor = LLVKColor{0.2f,0.4f,0.6f,0.1f};
        std::string error;
        const auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        tree.setKeyboardFocus(*editor,false,false,error);
        LLVKWidgetTree::EditorView input;
        input.drawAlpha = 0.75f;
        const auto prepared = tree.prepareLineEditor(*editor,input,error);
        ensure(error,prepared.has_value());
        ensure("caret visible initially",prepared->caretVisible);
        ensure("focus image",prepared->parts[1].image->name() == "focused");
        ensure("programmatic border hidden",!tree.get(tree.get(*editor)->lineEditor->border)->params.visible);
        ensure_equals("text alpha replaces color alpha",prepared->parts[2].color[3],0.75f);
        const auto& glyphs = prepared->parts[2].text->glyphs;
        ensure_equals("one masked glyph per character",glyphs.size(),std::size_t(6));
        for (const auto& glyph : glyphs) ensure("same password mask glyph",glyph.glyph == glyphs.front().glyph);
        input.secondsSinceKeystroke = 1.1;
        const auto blink = tree.prepareLineEditor(*editor,input,error);
        ensure(error,blink.has_value());
        ensure("caret blinks off",!blink->caretVisible);
        input.secondsSinceKeystroke = 1.6;
        input.applicationFocused = false;
        const auto inactive = tree.prepareLineEditor(*editor,input,error);
        ensure(error,inactive.has_value());
        ensure("inactive application has no caret",!inactive->caretVisible);
    }

    template<> template<> void object::test<125>()
    {
        set_test_name("native button preparation preserves image callback label ordering and pressed offset");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,32};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params params;
        params.label = U" Before ";
        params.selectedLabel = U" After ";
        params.images.unselected = image("normal");
        params.images.selected = image("selected");
        params.images.disabledSelected = image("disabled-selected");
        params.images.pressedSelected = image("pressed-selected");
        params.pressedSelectedProvided = true;
        params.selectedLabelColor = LLVKColor{0,1,0,1};
        params.hoverGlow = 0.5f;
        params.isToggled.function = [](auto,const LLSD&) { return true; };
        std::string error;
        const auto button = tree.createButton(view,control,params,0,error);
        ensure(error,button.has_value());
        LLVKWidgetTree::ButtonView input;
        const auto first = tree.prepareButton(*button,input,error);
        ensure(error,first.has_value());
        ensure_equals("image selected before callback",first->primitives[0].image->name(),std::string("normal"));
        ensure("label after callback trimmed",first->label == U"After");
        ensure_equals("selected label color",first->labelColor[1],1.f);
        tree.setKeyboardFocus(*button,false,false,error);
        input.spaceDown = true;
        const auto pressed = tree.prepareButton(*button,input,error);
        ensure(error,pressed.has_value());
        ensure_equals("focused border precedes image",pressed->primitives[1].image->name(),std::string("pressed-selected"));
        ensure_equals("pressed text x offset",pressed->text.x,first->text.x+1.f);
        tree.setEnabled(*button,false);
        input.spaceDown = false;
        const auto disabled = tree.prepareButton(*button,input,error);
        ensure(error,disabled.has_value());
        ensure_equals("disabled checked image overrides",disabled->primitives.back().image->name(),std::string("disabled-selected"));
    }

    template<> template<> void object::test<124>()
    {
        set_test_name("native browser declaration retains login policy without starting media during construction");
        LLVKWidgetTree tree;
        LLVKWidgetFactory::Resources resources;
        resources.fallbackFont = loadFont();
        LLVKWidgetFactory::Callbacks callbacks;
        bool initialized = false;
        callbacks.actions["browser-init"] = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            initialized = node && node->browser && node->browser->trusted && node->browser->startUrl.empty();
        };
        LLVKWidgetFactory factory({}, {}, {},callbacks,resources);
        std::string error;
        const auto browser = factory.construct(tree,
            "<web_browser name='login_html' width='1024' height='598' start_url='' trusted_content='true' border_visible='false' tab_stop='false'>"
            "<web_browser.init_callback function='browser-init'/></web_browser>",0,error);
        ensure(error,browser.has_value());
        ensure("browser configuration before init",initialized);
        ensure("native panel base retained",tree.get(*browser)->panel.has_value());
        ensure("browser border policy retained",!tree.get(*browser)->browser->borderVisible);
    }

    template<> template<> void object::test<123>()
    {
        set_test_name("native browser frames copy borrowed BGRA with opaque alpha and resize invalidation");
        LLVKBrowserSurface surface;
        std::string error;
        ensure("size accepted",surface.resize(2,2,error));
        std::vector<std::uint8_t> pixels{1,2,3,4, 5,6,7,8, 9,10,11,12, 13,14,15,16};
        ensure("frame published",surface.publish(2,2,pixels,error));
        auto first = surface.frame();
        const std::vector<std::uint8_t> expected{11,10,9,255, 15,14,13,255, 3,2,1,255, 7,6,5,255};
        ensure("bottom-up RGBA matches RGB upload contract",std::ranges::equal(first->bottomUpRgba(),expected));
        pixels.assign(16,99);
        ensure("borrowed pixels copied",std::ranges::equal(first->bottomUpRgba(),expected));
        ensure("replacement frame",surface.publish(2,2,pixels,error));
        ensure("old frame remains immutable",std::ranges::equal(first->bottomUpRgba(),expected));
        ensure("resize invalidates publication",surface.resize(3,2,error) && !surface.frame());
        const auto generation = surface.generation();
        ensure("stale callback ignored",!surface.publish(2,2,pixels,error) && error.empty());
        ensure_equals("stale callback does not advance generation",surface.generation(),generation);
        ensure("malformed current frame rejected",!surface.publish(3,2,pixels,error) && !error.empty());
        ensure("oversized surface rejected",!surface.resize(8192,8192,error));
        ensure_equals("failed resize retains width",surface.width(),3u);
    }

    template<> template<> void object::test<122>()
    {
        set_test_name("native image preparation preserves nine-slice borders shrink and fractional scale");
        png_image encoder{};
        encoder.version = PNG_IMAGE_VERSION;
        encoder.width = encoder.height = 8;
        encoder.format = PNG_FORMAT_RGBA;
        std::vector<std::uint8_t> pixels(8*8*4,255);
        png_alloc_size_t length = 0;
        ensure("geometry PNG size",png_image_write_to_memory(&encoder,nullptr,&length,0,pixels.data(),0,nullptr) != 0);
        std::vector<std::uint8_t> encoded(length);
        ensure("geometry PNG",png_image_write_to_memory(&encoder,encoded.data(),&length,0,pixels.data(),0,nullptr) != 0);
        png_image_free(&encoder);
        std::string error;
        LLVKWidgetImage::Metadata metadata;
        metadata.scale = LLVKWidgetImage::Rect{2,2,6,6};
        const auto image = LLVKWidgetImage::decodeSkinPng("bordered",encoded,metadata,error);
        ensure(error,image != nullptr);
        const auto geometry = image->prepare({10,20,30,32},1,1,0,0,error);
        ensure(error,geometry.has_value());
        ensure_equals("nine quads",geometry->count,std::size_t(9));
        ensure_equals("left border fixed",geometry->quads[0].position.right,12.f);
        ensure_equals("bottom border fixed",geometry->quads[0].position.top,22.f);
        ensure_equals("center stretches right",geometry->quads[4].position.right,28.f);
        ensure_equals("center stretches top",geometry->quads[4].position.top,30.f);
        ensure_equals("center UV unaffected",geometry->quads[4].uv.right,0.75f);
        const auto small = image->prepare({0,0,2,8},1,1,0,0,error);
        ensure(error,small.has_value());
        ensure_equals("uniform shrink horizontal",small->quads[0].position.right,1.f);
        ensure_equals("uniform shrink vertical",small->quads[0].position.top,1.f);
        ensure_equals("collapsed center",small->quads[4].position.right,1.f);
        const auto scaled = image->prepare({0,0,20,12},1.25f,1.25f,0.25f,0.25f,error);
        ensure(error,scaled.has_value());
        ensure_equals("outer origin stays fractional",scaled->quads[0].position.left,0.3125f);
        ensure_equals("inner edge rounds after transform",scaled->quads[0].position.right,3.f);
        metadata.style = LLVKWidgetImage::Scale::Outer;
        const auto outerImage = LLVKWidgetImage::decodeSkinPng("outer",encoded,metadata,error);
        ensure(error,outerImage != nullptr);
        const auto outer = outerImage->prepare({0,0,20,12},1,1,0,0,error);
        ensure(error,outer.has_value());
        ensure_equals("outer scale keeps center width",outer->quads[4].position.right-outer->quads[4].position.left,4.f);
        ensure_equals("outer scale centers region",outer->quads[4].position.left,8.f);
        ensure("invalid scale rejected",!image->prepare({0,0,20,12},0,1,0,0,error));
        metadata = {};
        const auto simple = LLVKWidgetImage::decodeSkinPng("simple",encoded,metadata,error);
        const auto single = simple->prepare({0,0,3,3},1.25f,1.25f,0.25f,0.25f,error);
        ensure(error,single.has_value());
        ensure_equals("no border single quad",single->count,std::size_t(1));
        ensure_equals("single quad extent rounds independently",single->quads[0].position.right,4.3125f);
    }

    template<> template<> void object::test<121>()
    {
        set_test_name("native login text declarations preserve body layout and reject unresolved rich content");
        LLVKWidgetTree tree;
        LLVKWidgetFactory::Resources resources;
        resources.fallbackFont = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        std::string error;
        ensure("text defaults",factory.loadDefaults(tree,
            "<text parse_urls='true' font='SansSerifSmall' mouse_opaque='false' tab_stop='false' font_shadow='none' "
            "allow_scroll='false' h_pad='0' v_pad='0' max_length='4096'/>",error));
        const auto label = factory.construct(tree,
            "<text name='forgot_password_text' width='140' height='32' font='SansSerifMedium' valign='center' halign='right'>\n"
            "    Forgot password?\n</text>",0,error);
        ensure(error,label.has_value());
        ensure_equals("trimmed text body",tree.value(*label).asString(),std::string("Forgot password?"));
        ensure("native document reflows",tree.reflowPlainText(*label,error));
        ensure("right alignment",tree.get(*label)->plainText->params.layout.alignment == LLVKFont::HorizontalAlign::Right);
        ensure("vertical center",tree.get(*label)->plainText->params.vertical == LLVKFont::VerticalAlign::Center);
        ensure("URL update fails explicitly",!tree.setPlainText(*label,"https://example.com",error));
        ensure("rich processing diagnostic",error.find("rich") != error.npos);
        ensure_equals("failed update retains old value",tree.value(*label).asString(),std::string("Forgot password?"));
        ensure("issue code cannot silently become literal",!tree.setPlainText(*label,"FIRE-123",error));
        ensure("URL-shaped literal allowed with parsing disabled",factory.construct(tree,
            "<text width='200' height='32' parse_urls='false'>https://example.com</text>",0,error).has_value());
    }

    template<> template<> void object::test<120>()
    {
        set_test_name("native login literal link releases capture before destructive click callback");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,140,32};
        view.mouseOpaque = false;
        view.soundFlags = 3;
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = "Create an account";
        std::string error;
        const auto label = tree.createPlainText(view,control,{},0,error);
        ensure(error,label.has_value());
        std::vector<std::string> order;
        LLVKWidgetTree::Events events;
        events.sound = [&](auto,bool release) { order.push_back(release ? "up" : "down"); };
        events.cursor = [&](auto,bool hand) { ensure("hand cursor",hand); order.push_back("hover"); };
        events.captureLost = [&](auto) { order.push_back("release"); };
        tree.setEvents(*label,std::move(events));
        tree.setPlainTextClicked(*label,[&](auto id)
        {
            ensure_equals("capture cleared before callback",tree.mouseCapture(),LLVKWidgetTree::Id(0));
            order.push_back("click");
            ensure("callback erases label",tree.erase(id,error));
        });
        LLVKWidgetTree::PointerEvent event;
        event.x = 5; event.y = 5;
        ensure("hover link",tree.routePointer(*label,event,error));
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        ensure("press link",tree.routePointer(*label,event,error));
        ensure_equals("label captures",tree.mouseCapture(),*label);
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        ensure("release link",tree.routePointer(*label,event,error));
        ensure("label removed",!tree.get(*label));
        ensure("sound capture callback ordering",order == std::vector<std::string>({"hover","down","up","release","click"}));
    }

    template<> template<> void object::test<119>()
    {
        set_test_name("native login combo commits canonical item values and stacks resolve configured spacing");
        LLVKWidgetTree tree;
        tree.defineSetting("UIResizeBarHeight",LLSD(3),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetFactory::PanelDefaults panels;
        panels.control.font = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, {}, panels);
        std::string error;
        const auto stack = factory.construct(tree,"<layout_stack width='100' height='20' orientation='horizontal'/>" ,0,error);
        ensure(error,stack.has_value());
        ensure_equals("configured spacing",tree.get(*stack)->layoutStack->spacing,3);
        const auto explicitStack = factory.construct(tree,"<layout_stack width='100' height='20' border_size='0'/>" ,0,error);
        ensure(error,explicitStack.has_value());
        ensure_equals("explicit zero retained",tree.get(*explicitStack)->layoutStack->spacing,0);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,211,32};
        LLVKControl::Params control;
        control.font = panels.control.font;
        auto params = std::make_shared<LLVKWidgetTree::ComboParams>();
        params->buttonControl.font = params->listControl.font = params->editorControl.font = control.font;
        params->allowTextEntry = true;
        params->items = {{"Home",LLSD("canonical-home-value"),true}};
        LLSD committed;
        control.commit.function = [&](auto,const LLSD& value) { committed = value; };
        const auto combo = tree.createCombo(view,control,*params,0,error);
        ensure(error,combo.has_value());
        const auto editor = tree.get(*combo)->combo->editor;
        tree.setValue(editor,LLSD("hOmE"));
        ensure("commit case-insensitive label",tree.commit(editor));
        ensure_equals("canonical value, not typed casing",committed.asString(),std::string("canonical-home-value"));
        ensure_equals("selected label capitalization",tree.value(editor).asString(),std::string("Home"));
    }

    template<> template<> void object::test<118>()
    {
        set_test_name("native login combo typing completes labels and clears stale item values");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,211,32};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ComboParams>();
        params->buttonControl.font = params->listControl.font = params->editorControl.font = control.font;
        params->allowTextEntry = true; params->maximumBytes = 128;
        params->items = {{"Home",LLSD("home"),true},{"Last location",LLSD("last"),true}};
        int prearranged = 0, changed = 0;
        params->prearrange.function = [&](auto,const LLSD&) { ++prearranged; };
        params->textChanged.function = [&](auto,const LLSD&) { ++changed; };
        std::string error;
        auto combo = tree.createCombo(view,control,*params,0,error);
        ensure(error,combo.has_value());
        const auto editor = tree.get(*combo)->combo->editor;
        tree.setKeyboardFocus(editor,false,false,error);
        ensure("type h",tree.lineEditorUnicode(editor,U'h',false,error));
        ensure_equals("prefix completed preserving case",tree.value(editor).asString(),std::string("home"));
        ensure_equals("selected item value",tree.value(*combo).asString(),std::string("home"));
        ensure_equals("completion selection start",tree.get(editor)->lineEditor->text.selectionStart(),std::size_t(4));
        ensure_equals("completion caret at prefix",tree.get(editor)->lineEditor->text.cursor(),std::size_t(1));
        ensure_equals("prearrange first character",prearranged,1);
        ensure("replace completion with unmatched character",tree.lineEditorUnicode(editor,U'z',false,error));
        ensure_equals("typed unmatched prefix retained",tree.value(*combo).asString(),std::string("hz"));
        ensure("selection cleared",!tree.get(*combo)->combo->selected);
        ensure("unmatched text tentative",tree.get(editor)->control->tentative);
        ensure("backspace avoids autocomplete",tree.lineEditorKey(editor,LLVKLineEditor::Key::Backspace,{},error));
        ensure_equals("deletion retains literal text",tree.value(editor).asString(),std::string("h"));
        ensure_equals("three text-change notifications",changed,3);
    }

    template<> template<> void object::test<117>()
    {
        set_test_name("native login combo opens and selects a location through pointer routing");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,1024,768};
        std::string error;
        auto root = tree.create(view,0,error);
        ensure(error,root.has_value());
        view.rect = {300,50,511,82};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ComboParams>();
        params->buttonControl.font = params->listControl.font = params->editorControl.font = control.font;
        params->allowTextEntry = true;
        params->items = {{"Last location",LLSD("last"),true},{"Home",LLSD("home"),true}};
        LLSD committed;
        control.commit.function = [&](auto,const LLSD& value) { committed = value; };
        auto combo = tree.createCombo(view,control,*params,*root,error);
        ensure(error,combo.has_value());
        const auto children = *tree.get(*combo)->combo;
        LLVKWidgetTree::PointerEvent event;
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.x = 505; event.y = 65;
        ensure("arrow click",tree.routePointer(*root,event,error));
        ensure("list visible",tree.get(children.list)->params.visible);
        ensure_equals("capture transfers to list",tree.mouseCapture(),children.list);
        const auto popup = tree.screenRect(children.list,error);
        ensure(error,popup.has_value());
        ensure("popup stays in root",popup->bottom >= 0 && popup->top <= 768);
        const auto rowHeight = tree.get(*combo)->combo->rowHeight;
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        event.x = popup->left+5; event.y = popup->top-2-rowHeight-rowHeight/2;
        ensure("choose second row",tree.routePointer(*root,event,error));
        ensure_equals("Home committed",committed.asString(),std::string("home"));
        ensure_equals("editor shows selected label",tree.value(children.editor).asString(),std::string("Home"));
        ensure("popup closed",!tree.get(children.list)->params.visible);
        ensure_equals("focus restored to editor",tree.keyboardFocus(),children.editor);
        ensure_equals("capture released",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.x = 505; event.y = 65;
        ensure("reopen popup",tree.routePointer(*root,event,error));
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        tree.routePointer(*root,event,error);
        ensure_equals("arrow release clears capture",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.x = popup->left+5; event.y = popup->top-2-rowHeight/2;
        ensure("second click reaches popup",tree.routePointer(*root,event,error));
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        ensure("second click selects popup row",tree.routePointer(*root,event,error));
        ensure_equals("first row committed after separate click",committed.asString(),std::string("last"));
    }

    template<> template<> void object::test<116>()
    {
        set_test_name("native login combo owns editor button list and distinguishes item from typed values");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,211,32};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ComboParams>();
        params->buttonControl.font = params->listControl.font = params->editorControl.font = control.font;
        params->allowTextEntry = true;
        params->maximumBytes = 128;
        params->items = {{"Last location",LLSD("last"),true},{"Home",LLSD("home"),true},{"Unavailable",LLSD("no"),false}};
        int initializations = 0;
        LLSD committed;
        control.init.function = [&](auto id,const LLSD&)
        {
            ++initializations;
            const auto& combo = *tree.get(id)->combo;
            ensure("all children exist before init",tree.get(combo.button)->button && tree.get(combo.editor)->lineEditor && tree.get(combo.list)->control);
        };
        control.commit.function = [&](auto,const LLSD& value) { committed = value; };
        std::string error;
        auto combo = tree.createCombo(view,control,*params,0,error);
        ensure(error,combo.has_value());
        const auto children = *tree.get(*combo)->combo;
        ensure_equals("once initialized",initializations,1);
        ensure("list initially hidden",!tree.get(children.list)->params.visible);
        ensure("editable button not tab-stop",!tree.get(children.button)->control->params.tabStop);
        ensure("native select home",tree.setComboValue(*combo,LLSD("home"),error));
        ensure_equals("selection exposes item value",tree.value(*combo).asString(),std::string("home"));
        ensure_equals("editor exposes label",tree.value(children.editor).asString(),std::string("Home"));
        ensure("clear selection for unmatched value",tree.setComboValue(*combo,LLSD("missing"),error));
        ensure_equals("unmatched selection retains editor",tree.value(*combo).asString(),std::string("Home"));
        tree.setValue(children.editor,LLSD("Region/128/128"));
        ensure("commit typed entry",tree.commit(children.editor));
        ensure_equals("typed value committed by parent",committed.asString(),std::string("Region/128/128"));
        ensure("disabled selection rejected",!tree.selectComboItem(*combo,2,error));
        ensure("erase composite",tree.erase(*combo,error));
        ensure("all children released",!tree.get(children.button) && !tree.get(children.editor) && !tree.get(children.list));
    }

    template<> template<> void object::test<115>()
    {
        set_test_name("native login layout preparation propagates resize and visibility animation");
        LLVKWidgetTree tree;
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, {}, defaults);
        std::string error;
        auto root = factory.construct(tree,
            "<layout_stack width='200' height='60' orientation='horizontal' open_time_constant='0.02' close_time_constant='0.03'>"
            "<layout_panel width='100' height='60'/><layout_panel width='100' height='60'/></layout_stack>",0,error);
        ensure(error,root.has_value());
        const auto panels = tree.get(*root)->layoutStack->panels;
        tree.reshape(*root,400,60,error);
        ensure("resize marks dirty",tree.get(*root)->layoutStack->needsLayout);
        ensure("frame prepares resize",tree.prepareLayoutStacks(*root,0.03f,error));
        ensure_equals("both expand",tree.get(panels.front())->params.rect.right,200);
        tree.setVisible(panels.front(),false);
        ensure("frame animates close",tree.prepareLayoutStacks(*root,0.03f,error));
        ensure_equals("close half life",tree.get(panels.front())->layoutPanel->visibleAmount,0.5f);
        ensure("closing still occupies visible extent",tree.get(panels.back())->params.rect.left > 0);
        ensure("animation remains dirty",tree.get(*root)->layoutStack->needsLayout);
        ensure("close settles",tree.prepareLayoutStacks(*root,1.f,error));
        ensure_equals("closed panel amount",tree.get(panels.front())->layoutPanel->visibleAmount,0.f);
        ensure_equals("remaining panel at left",tree.get(panels.back())->params.rect.left,0);
        tree.setVisible(panels.front(),true);
        ensure("opening",tree.prepareLayoutStacks(*root,0.02f,error));
        ensure_equals("open half life",tree.get(panels.front())->layoutPanel->visibleAmount,0.5f);
    }

    template<> template<> void object::test<114>()
    {
        set_test_name("native login layout declarations construct typed panels and nested rows");
        LLVKWidgetTree tree;
        LLVKWidgetFactory::PanelDefaults panels;
        panels.control.font = loadFont();
        int initialized = 0;
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["init"] = [&](auto id,const LLSD&)
        { ++initialized; ensure("typed init before parent attachment",tree.get(id)->parent == 0); };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, {}, panels);
        std::string error;
        auto stack = factory.construct(tree,
            "<layout_stack width='1024' height='152' orientation='horizontal' animate='false' border_size='0'>"
            "<layout_panel name='left' width='27' height='152'/>"
            "<layout_panel name='center' width='970' height='152' min_width='970' auto_resize='false'>"
            "<layout_panel.init_callback function='init'/>"
            "<layout_stack width='685' height='152' orientation='vertical' animate='false'>"
            "<layout_panel name='row' width='685' height='86' auto_resize='false'/></layout_stack>"
            "</layout_panel><layout_panel name='right' width='27' height='152'/></layout_stack>",0,error);
        ensure(error,stack.has_value());
        ensure_equals("one typed initialization",initialized,1);
        const auto center = tree.get(*stack)->layoutStack->panels.at(1);
        ensure("center geometry",tree.get(center)->params.rect == LLVKWidgetTree::Rect{27,0,997,152});
        ensure_equals("layout panel follows reset",tree.get(center)->params.follows,std::uint8_t(0));
        ensure("nested stack exists",tree.get(tree.get(center)->children.front())->layoutStack.has_value());
        const auto count = tree.size();
        ensure("wrong child type rejects",!factory.construct(tree,"<layout_stack><panel/></layout_stack>",0,error));
        ensure_equals("rejected construction atomic",tree.size(),count);
        ensure("unimplemented user resizing explicit",!factory.construct(tree,"<layout_stack><layout_panel user_resize='true'/></layout_stack>",0,error));
    }

    template<> template<> void object::test<113>()
    {
        set_test_name("native login stack distributes elastic margins around fixed content");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,1024,152};
        std::string error;
        auto stack = tree.createLayoutStack(view,false,0,true,0,error);
        ensure(error,stack.has_value());
        ensure("settled geometry test disables animation",tree.configureLayoutStack(*stack,false,0.02f,0.03f,error));
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::Node::LayoutPanel flexible, fixed;
        fixed.autoResize = false; fixed.minimum = fixed.expandedMinimum = 970;
        view.rect = {0,0,27,152};
        auto left = tree.createPanel(view,control,{},0,error);
        ensure(error,left.has_value());
        ensure("attach left",tree.attachLayoutPanel(*stack,*left,flexible,error));
        view.rect = {0,0,970,152};
        auto center = tree.createPanel(view,control,{},0,error);
        ensure(error,center.has_value());
        ensure("attach center",tree.attachLayoutPanel(*stack,*center,fixed,error));
        view.rect = {0,0,27,152};
        auto right = tree.createPanel(view,control,{},0,error);
        ensure(error,right.has_value());
        ensure("attach right",tree.attachLayoutPanel(*stack,*right,flexible,error));
        ensure("login baseline layout",tree.updateLayoutStack(*stack,error));
        ensure("center at baseline",tree.get(*center)->params.rect == LLVKWidgetTree::Rect{27,0,997,152});
        tree.reshape(*stack,1200,152,error);
        ensure("wide login layout",tree.updateLayoutStack(*stack,error));
        ensure("elastic margins expanded equally",tree.get(*center)->params.rect == LLVKWidgetTree::Rect{115,0,1085,152});
        tree.setVisible(*right,false);
        ensure("hidden panel redistributes space",tree.updateLayoutStack(*stack,error));
        ensure_equals("single elastic margin",tree.get(*center)->params.rect.left,230);
        view.rect = {0,0,685,152};
        auto column = tree.createLayoutStack(view,true,0,false,0,error);
        ensure(error,column.has_value());
        fixed.minimum = fixed.expandedMinimum = 0;
        view.rect = {0,0,685,86};
        auto row = tree.createPanel(view,control,{},0,error);
        ensure(error,row.has_value());
        tree.attachLayoutPanel(*column,*row,fixed,error);
        ensure("vertical login row",tree.updateLayoutStack(*column,error));
        ensure("rows begin at top",tree.get(*row)->params.rect == LLVKWidgetTree::Rect{0,66,685,152});
    }

    template<> template<> void object::test<112>()
    {
        set_test_name("native panel keys respect focus roots default buttons and Return capture");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.focusRoot = true;
        LLVKControl::Params control;
        control.font = loadFont();
        std::string error;
        auto panel = tree.createPanel(view,control,{},0,error);
        ensure(error,panel.has_value());
        view.focusRoot = false;
        view.rect = {0,0,100,23};
        int textCommits = 0, buttonCommits = 0;
        control.commit.function = [&](auto,const LLSD&) { ++textCommits; };
        auto editor = tree.createLineEditor(view,control,{},*panel,error);
        ensure(error,editor.has_value());
        control.commit.function = [&](auto,const LLSD&) { ++buttonCommits; };
        auto button = tree.createButton(view,control,{},*panel,error);
        ensure(error,button.has_value());
        tree.setPanelDefaultButton(*panel,*button,error);
        tree.setKeyboardFocus(*editor,false,false,error);
        using Key = LLVKWidgetTree::PanelKey;
        ensure("Return invokes default",tree.panelKey(*panel,Key::Return,{},error));
        ensure_equals("default committed",buttonCommits,1);
        ensure_equals("editor not committed when default exists",textCommits,0);
        tree.setVisible(*button,false);
        ensure("hidden default falls back to editor",tree.panelKey(*panel,Key::Return,{},error));
        ensure_equals("editor committed",textCommits,1);
        tree.setVisible(*button,true);
        tree.setKeyboardFocus(*button,false,false,error);
        ensure("Return-capturing button handles own key",!tree.panelKey(*panel,Key::Return,{},error));
        ensure_equals("no duplicate commit",buttonCommits,1);
        ensure("Tab from button wraps",tree.panelKey(*panel,Key::Tab,{},error));
        ensure_equals("Tab selects editor",tree.keyboardFocus(),*editor);
        ensure("Shift-Tab reverses",tree.panelKey(*panel,Key::Tab,{true,false,false},error));
        ensure_equals("Shift-Tab selects button",tree.keyboardFocus(),*button);
        ensure("modified Return unhandled",!tree.panelKey(*panel,Key::Return,{false,true,false},error));
        ensure("Escape handled",tree.panelKey(*panel,Key::Escape,{},error));
        ensure_equals("Escape clears subtree focus",tree.keyboardFocus(),LLVKWidgetTree::Id(0));
        ensure("Return without focus unhandled",!tree.panelKey(*panel,Key::Return,{},error));
    }

    template<> template<> void object::test<111>()
    {
        set_test_name("native panel focus takes ownership before handing focus to first child");
        LLVKWidgetTree tree;
        LLVKControl::Params control;
        control.font = loadFont();
        std::string error;
        auto panel = tree.createPanel({},control,{},0,error);
        ensure(error,panel.has_value());
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "child";
        auto editor = tree.createLineEditor(view,control,params,*panel,error);
        ensure(error,editor.has_value());
        bool sawPanelFocus = false;
        LLVKWidgetTree::Events events;
        events.focusReceived = [&](auto id) { sawPanelFocus = tree.keyboardFocus() == id; };
        tree.setEvents(*panel,events);
        ensure("request panel focus",tree.requestControlFocus(*panel,true,error));
        ensure("panel first owns focus",sawPanelFocus);
        ensure_equals("focus handed to child",tree.keyboardFocus(),*editor);
        ensure_equals("child tab entry selects all",tree.get(*editor)->lineEditor->text.selectionStart(),std::size_t(5));
        ensure("repeat panel focus",tree.requestControlFocus(*panel,true,error));
        ensure_equals("repeat preserves descendant",tree.keyboardFocus(),*editor);
        auto empty = tree.createPanel({},control,{},0,error);
        ensure(error,empty.has_value());
        ensure("empty panel focus",tree.requestControlFocus(*empty,true,error));
        ensure_equals("empty panel retains own focus without recursion",tree.keyboardFocus(),*empty);
    }

    template<> template<> void object::test<110>()
    {
        set_test_name("native container keys preserve document delegation axis order and ignore policy");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,100};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,300,400};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        tree.updateScrollContainer(*container,error);
        const auto chrome = *tree.get(*container)->scrollContainer;
        using Key = LLVKWidgetTree::ScrollKey;
        ensure("page key remains unhandled",!tree.scrollContainerKey(*container,Key::PageDown,{},error));
        ensure_equals("vertical page moved",tree.get(chrome.vertical)->scrollbar->position,83);
        ensure_equals("horizontal page also moved",tree.get(chrome.horizontal)->scrollbar->position,83);
        ensure_equals("unhandled page does not force document update",tree.get(*document)->params.rect.top,100);
        ensure("Down handled",tree.scrollContainerKey(*container,Key::Down,{},error));
        ensure_equals("vertical priority",tree.get(chrome.vertical)->scrollbar->position,99);
        ensure_equals("horizontal not changed by Down",tree.get(chrome.horizontal)->scrollbar->position,83);
        ensure_equals("handled key updates document",tree.get(*document)->params.rect.top,199);
        params->ignoreArrowKeys = true;
        view.rect = {0,0,100,100};
        auto ignored = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,ignored.has_value());
        ensure("ignore rejects before delegation",!tree.scrollContainerKey(*ignored,Key::End,{},error));
        params->ignoreArrowKeys = false;
        auto delegate = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,delegate.has_value());
        LLVKWidgetTree::LineEditorParams editorParams;
        editorParams.text.defaultText = "abc";
        auto editor = tree.createLineEditor(view,control,editorParams,0,error);
        ensure(error,editor.has_value());
        tree.attachScrollContent(*delegate,*editor,0,error);
        tree.setKeyboardFocus(*editor,false,false,error);
        ensure("document receives navigation first",tree.scrollContainerKey(*delegate,Key::Home,{},error));
        ensure_equals("editor home applied",tree.get(*editor)->lineEditor->text.cursor(),std::size_t(0));
    }

    template<> template<> void object::test<109>()
    {
        set_test_name("native directional tab movement wraps and honors text-only prefilter");
        LLVKWidgetTree tree;
        std::string error;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        auto first = tree.createControl({},control,*root,error);
        ensure(error,first.has_value());
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        auto editor = tree.createLineEditor(view,control,{},*root,error);
        ensure(error,editor.has_value());
        auto last = tree.createControl({},control,*root,error);
        ensure(error,last.has_value());
        ensure("next with no focus starts oldest",tree.moveFocus(*root,true,false,error));
        ensure_equals("first target",tree.keyboardFocus(),*first);
        tree.moveFocus(*root,true,false,error);
        ensure_equals("next target",tree.keyboardFocus(),*editor);
        tree.moveFocus(*root,true,false,error);
        ensure_equals("last target",tree.keyboardFocus(),*last);
        tree.moveFocus(*root,true,false,error);
        ensure_equals("forward wrap",tree.keyboardFocus(),*first);
        tree.moveFocus(*root,false,false,error);
        ensure_equals("backward wrap",tree.keyboardFocus(),*last);
        tree.defineSetting("TabToTextFieldsOnly",LLSD(true),LLVKWidgetTree::SettingType::Boolean);
        int tabs = 0;
        LLVKWidgetTree::Events events;
        events.tabInto = [&](auto) { ++tabs; };
        tree.setEvents(*editor,events);
        tree.moveFocus(*root,true,false,error);
        ensure_equals("setting selects only text",tree.keyboardFocus(),*editor);
        tree.moveFocus(*root,true,false,error);
        ensure_equals("single forward candidate still tabs into",tabs,2);
        tree.moveFocus(*root,false,false,error);
        ensure_equals("single backward candidate does not retab",tabs,2);
        tree.setEnabled(*editor,false);
        ensure("no text candidate returns false",!tree.moveFocus(*root,true,false,error));
    }

    template<> template<> void object::test<108>()
    {
        set_test_name("native first focus runs editor tab selection notification and flash in order");
        LLVKWidgetTree tree;
        std::string error;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "select me";
        auto editor = tree.createLineEditor(view,control,params,*root,error);
        ensure(error,editor.has_value());
        int tabEvents = 0;
        LLVKWidgetTree::Events events;
        events.tabInto = [&](auto id)
        {
            ++tabEvents;
            ensure_equals("focus precedes tab event",tree.keyboardFocus(),id);
            ensure_equals("selection precedes tab event",tree.get(id)->lineEditor->text.selectionStart(),std::size_t(9));
            ensure("selection is active",tree.get(id)->lineEditor->text.selecting());
            ensure_equals("flash reset follows tab event",tree.focusFlashAmount(),0.f);
        };
        tree.setEvents(*editor,events);
        tree.advanceTime(1.0,error);
        ensure("focus first",tree.focusFirst(*root,true,error));
        ensure_equals("flash starts",tree.focusFlashAmount(),1.f);
        ensure("repeat focus no extra tab event",tree.focusFirst(*root,true,error));
        ensure_equals("one tab notification",tabEvents,1);
        tree.advanceTime(1.3,error);
        ensure("flash decays",tree.focusFlashAmount() < 0.00001f);
        events.tabInto = [&](auto id) { std::string failure; ensure("tab callback deletion",tree.erase(id,failure)); };
        tree.setEvents(*editor,events);
        tree.setKeyboardFocus(0,false,false,error);
        ensure("deletion during tab entry tolerated",tree.focusFirst(*root,true,error));
        ensure("editor removed",!tree.get(*editor));
    }

    template<> template<> void object::test<107>()
    {
        set_test_name("native tab query preserves stable group ordering and leaf filters");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.defaultTabGroup = 2;
        std::string error;
        auto root = tree.create(view,0,error);
        ensure(error,root.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        view.tabGroup = 1;
        auto low = tree.createControl(view,control,*root,error);
        ensure(error,low.has_value());
        view.tabGroup = 2;
        auto first = tree.createControl(view,control,*root,error);
        auto second = tree.createControl(view,control,*root,error);
        ensure(error,first && second);
        view.tabGroup = 3;
        auto high = tree.createControl(view,control,*root,error);
        ensure(error,high.has_value());
        auto order = tree.tabOrder(*root,error);
        ensure(error,order.has_value());
        ensure("reverse group query with stable front order",*order == std::vector<LLVKWidgetTree::Id>({*low,*high,*second,*first}));
        view.tabGroup = 0;
        auto leaf = tree.createControl(view,control,*first,error);
        ensure(error,leaf.has_value());
        order = tree.tabOrder(*root,error);
        ensure(error,order.has_value());
        ensure_equals("matching parent replaced by matching leaf",order->back(),*leaf);
        tree.setEnabled(*leaf,false);
        order = tree.tabOrder(*root,error);
        ensure(error,order.has_value());
        ensure_equals("disabled leaf restores matching parent",order->back(),*first);
        control.tabStop = false;
        auto gate = tree.createControl(view,control,*root,error);
        ensure(error,gate.has_value());
        control.tabStop = true;
        auto hiddenByGate = tree.createControl(view,control,*gate,error);
        ensure(error,hiddenByGate.has_value());
        order = tree.tabOrder(*root,error);
        ensure(error,order.has_value());
        ensure("tab-stop false prevents control descendant traversal",std::find(order->begin(),order->end(),*hiddenByGate) == order->end());
        tree.setVisible(*root,false);
        order = tree.tabOrder(*root,error);
        ensure(error,order.has_value());
        ensure("hidden root prunes query",order->empty());
    }

    template<> template<> void object::test<106>()
    {
        set_test_name("native scroll preparation separates background document clip and chrome order");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {10,20,110,120};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        params->opaque = true; params->borderVisible = true;
        params->backgroundColor = LLVKColor{0.2f,0.3f,0.4f,0.5f};
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,300,400};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        auto prepared = tree.prepareScrollContainer(*container,0.5f,error);
        ensure(error,prepared.has_value());
        const auto state = *tree.get(*container)->scrollContainer;
        ensure("background outside clip",prepared->background == LLVKWidgetTree::Rect{11,21,109,119});
        ensure("document clip uses border plus strips",prepared->documentClip == LLVKWidgetTree::Rect{11,37,93,119});
        ensure_equals("background alpha multiplied",prepared->backgroundColor[3],0.25f);
        ensure_equals("background RGB retained",prepared->backgroundColor[0],0.2f);
        ensure("document not in chrome list",std::find(prepared->chrome.begin(),prepared->chrome.end(),*document) == prepared->chrome.end());
        ensure("chrome painter order",prepared->chrome == std::vector<LLVKWidgetTree::Id>({state.border,state.horizontal,state.vertical}));
        tree.setMouseCapture(state.vertical,error);
        prepared = tree.prepareScrollContainer(*container,1.f,error);
        ensure(error,prepared.has_value());
        ensure("active scrollbar transfers focus to native content",tree.keyboardFocus() == state.horizontal || tree.keyboardFocus() == state.vertical);
        tree.setKeyboardFocus(state.vertical,false,false,error);
        prepared = tree.prepareScrollContainer(*container,1.f,error);
        ensure(error,prepared.has_value());
        ensure_equals("existing descendant focus retained",tree.keyboardFocus(),state.vertical);
        ensure("border focus prepared",tree.get(state.border)->border->keyboardFocus);
    }

    template<> template<> void object::test<105>()
    {
        set_test_name("native auto-scroll preserves edge zones query purity and next-frame acceleration");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,100};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        params->minAutoRate = 120.f; params->maxAutoRate = 500.f;
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,300,400};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        tree.updateScrollContainer(*container,error);
        const auto chrome = *tree.get(*container)->scrollContainer;
        const LLVKWidgetTree::Rect root{0,0,100,100};
        ensure("edge query",tree.autoScroll(*container,95,5,root,0.1f,false,error));
        ensure("query does not activate scrolling",!tree.get(*container)->scrollContainer->autoScrolling);
        ensure("initial zero-speed activation",tree.autoScroll(*container,95,5,root,0.1f,true,error));
        ensure_equals("zero-speed first call",tree.get(chrome.vertical)->scrollbar->position,0);
        ensure("accelerate next frame",tree.advanceScrollFrame(*container,0.1f,error));
        ensure_equals("initial active acceleration from zero",tree.get(*container)->scrollContainer->autoRate,12.f);
        ensure("edge scroll",tree.autoScroll(*container,95,5,root,0.1f,true,error));
        ensure_equals("vertical one-pixel rounded step",tree.get(chrome.vertical)->scrollbar->position,1);
        ensure_equals("horizontal one-pixel rounded step",tree.get(chrome.horizontal)->scrollbar->position,1);
        tree.advanceScrollFrame(*container,0.1f,error);
        tree.advanceScrollFrame(*container,0.1f,error);
        ensure_equals("inactive frame resets minimum",tree.get(*container)->scrollContainer->autoRate,120.f);
        ensure("outside root not active",!tree.autoScroll(*container,101,5,root,0.1f,true,error));
        ensure("center not active",!tree.autoScroll(*container,50,50,root,0.1f,true,error));
        ensure("minimum-rate scroll",tree.autoScroll(*container,95,5,root,0.1f,true,error));
        ensure_equals("minimum-rate step",tree.get(chrome.vertical)->scrollbar->position,13);
    }

    template<> template<> void object::test<104>()
    {
        set_test_name("native scrollbar preparation preserves fallback image focus and glow order");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {10,20,110,30};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::ScrollbarParams params;
        params.decreaseControl.font = params.increaseControl.font = control.font;
        params.thickness = 10; params.documentSize = 1000; params.pageSize = 100;
        params.backgroundVisible = true;
        params.trackColor = LLVKColor{0.2f,0.3f,0.4f,0.5f};
        std::string error;
        auto bar = tree.createScrollbar(view,control,params,0,error);
        ensure(error,bar.has_value());
        auto fallback = tree.prepareScrollbar(*bar,25,25,0.05f,{1,1,1,1},error);
        ensure(error,fallback.has_value());
        ensure_equals("background then fallback track and thumb",fallback->primitives.size(),std::size_t(3));
        ensure("fallback uses literal right edge",fallback->primitives.at(1).rectangle == LLVKWidgetTree::Rect{20,20,90,30});
        ensure("fallback no image",!fallback->primitives.at(1).image);
        ensure_equals("stored alpha unmodified",fallback->primitives.at(1).color[3],0.5f);
        params.trackHorizontal = params.trackVertical = image("track");
        params.thumbHorizontal = params.thumbVertical = image("thumb");
        bar = tree.createScrollbar(view,control,params,0,error);
        ensure(error,bar.has_value());
        tree.setKeyboardFocus(*bar,false,false,error);
        auto prepared = tree.prepareScrollbar(*bar,25,25,0.05f,{0,1,0,0.7f},error);
        ensure(error,prepared.has_value());
        ensure_equals("background track focus thumb glow",prepared->primitives.size(),std::size_t(5));
        ensure("image track extent differs from fallback",prepared->primitives.at(1).rectangle == LLVKWidgetTree::Rect{20,20,100,30});
        ensure("drawSolid keeps image",prepared->primitives.at(1).solidImage && prepared->primitives.at(1).image == params.trackHorizontal);
        ensure_equals("focus alpha",prepared->primitives.at(2).color[3],0.7f);
        ensure("glow additive",prepared->primitives.back().additive && prepared->primitives.back().solidImage);
        ensure_equals("half-life glow",prepared->primitives.back().color[3],0.075f);
        ensure_equals("children retained after chrome",prepared->children.size(),std::size_t(2));
        const auto owner = params.thumbHorizontal;
        ensure("erase drawable",tree.erase(*bar,error));
        ensure("prepared image ownership retained",prepared->primitives.back().image == owner);
    }

    template<> template<> void object::test<103>()
    {
        set_test_name("native ancestor resize completes scroll page and document geometry updates");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,100};
        std::string error;
        auto parent = tree.create(view,0,error);
        ensure(error,parent.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        view.follows = LLVKWidgetTree::Left | LLVKWidgetTree::Right | LLVKWidgetTree::Top | LLVKWidgetTree::Bottom;
        auto container = tree.createScrollContainer(view,control,*params,*parent,error);
        ensure(error,container.has_value());
        view.follows = 0;
        view.rect = {0,0,200,300};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        tree.updateScrollContainer(*container,error);
        const auto chrome = *tree.get(*container)->scrollContainer;
        tree.setScrollPosition(chrome.vertical,200,true,error);
        ensure("ancestor grows",tree.reshape(*parent,400,500,error));
        ensure("no query needed to hide bars",!tree.get(chrome.vertical)->params.visible && !tree.get(chrome.horizontal)->params.visible);
        ensure_equals("vertical page reflects parent",tree.get(chrome.vertical)->scrollbar->pageSize,500);
        ensure_equals("horizontal page reflects parent",tree.get(chrome.horizontal)->scrollbar->pageSize,400);
        ensure_equals("position clamped during resize",tree.get(chrome.vertical)->scrollbar->position,0);
        ensure("document repositioned during resize",tree.get(*document)->params.rect == LLVKWidgetTree::Rect{0,200,200,500});
        ensure("explicit container shrinks",tree.setShape(*container,{5,6,105,106},error));
        ensure("both bars restored",tree.get(chrome.vertical)->params.visible && tree.get(chrome.horizontal)->params.visible);
        ensure_equals("small page restored",tree.get(chrome.vertical)->scrollbar->pageSize,84);
    }

    template<> template<> void object::test<102>()
    {
        set_test_name("native scroll update rejects stale descendant shape plans after callbacks");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,100};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        LLVKWidgetTree::Id arrow = 0;
        bool remove = false;
        params->vertical.changed = [&](auto,auto)
        {
            if (remove) { std::string error; ensure("remove arrow during range callback",tree.erase(arrow,error)); }
        };
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,200,300};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        ensure("initial update",tree.updateScrollContainer(*container,error));
        const auto chrome = *tree.get(*container)->scrollContainer;
        arrow = tree.get(chrome.horizontal)->scrollbar->increase;
        tree.setScrollPosition(chrome.vertical,30,true,error);
        tree.reshape(*document,200,20,error);
        remove = true;
        ensure("stale plan rejected without dereference",!tree.updateScrollContainer(*container,error));
        ensure("actionable stale-target error",error.find("removed during callback") != std::string::npos);
        ensure("callback-owned deletion preserved",!tree.get(arrow));
        ensure("container remains owned",tree.get(*container) != nullptr);
    }

    template<> template<> void object::test<101>()
    {
        set_test_name("native reveal scrolls minimally and biases oversized target to top left");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {10,20,110,120};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,300,400};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        const LLVKWidgetTree::Rect constraint{0,0,84,84};
        const auto target = LLVKWidgetTree::Rect{200,10,220,30};
        auto result = tree.scrollToReveal(*container,target,constraint,error);
        ensure(error,result.has_value());
        const auto chrome = *tree.get(*container)->scrollContainer;
        ensure_equals("minimal horizontal scroll",tree.get(chrome.horizontal)->scrollbar->position,136);
        ensure_equals("minimal vertical scroll",tree.get(chrome.vertical)->scrollbar->position,306);
        ensure("parent notification data uses container coordinates",*result == LLVKWidgetTree::Rect{210,30,230,50});
        result = tree.scrollToReveal(*container,target,constraint,error);
        ensure(error,result.has_value());
        ensure_equals("repeat reveal stable",tree.get(chrome.horizontal)->scrollbar->position,136);
        result = tree.scrollToReveal(*container,{20,0,200,200},constraint,error);
        ensure(error,result.has_value());
        ensure_equals("oversized target left bias",tree.get(chrome.horizontal)->scrollbar->position,20);
        ensure_equals("oversized target top bias",tree.get(chrome.vertical)->scrollbar->position,200);
        ensure("oversized parent target clipped",*result == LLVKWidgetTree::Rect{30,136,114,220});
        ensure("inverted constraint rejected",!tree.scrollToReveal(*container,target,{0,10,84,0},error));
    }

    template<> template<> void object::test<100>()
    {
        set_test_name("native container wheel routing respects axis priority and boundary propagation");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {10,20,110,120};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,200,300};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(*container,*document,0,error);
        tree.updateScrollContainer(*container,error);
        const auto chrome = *tree.get(*container)->scrollContainer;
        ensure("vertical edge consumed despite passive opaque content",tree.routeWheel(*container,30,70,-1,false,error));
        ensure("vertical wheel",tree.routeWheel(*container,30,70,1,false,error));
        ensure_equals("vertical step",tree.get(chrome.vertical)->scrollbar->position,16);
        ensure_equals("horizontal unchanged",tree.get(chrome.horizontal)->scrollbar->position,0);
        ensure_equals("document translation applied",tree.get(*document)->params.rect.top,116);
        ensure("horizontal wheel",tree.routeWheel(*container,30,70,1,true,error));
        ensure_equals("horizontal step",tree.get(chrome.horizontal)->scrollbar->position,16);
        tree.setScrollPosition(chrome.horizontal,INT32_MAX,true,error);
        ensure("horizontal edge propagates",!tree.routeWheel(*container,30,70,1,true,error));
        tree.reshape(*document,200,20,error);
        tree.updateScrollContainer(*container,error);
        ensure("horizontal-only boundary propagates",!tree.routeWheel(*container,30,70,1,false,error));
        ensure("horizontal-only ordinary wheel",tree.routeWheel(*container,30,70,-1,false,error));
    }

    template<> template<> void object::test<99>()
    {
        set_test_name("native scroll updates survive callback teardown and preserve reserved corner");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,100};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        params->reserveCorner = true;
        LLVKWidgetTree::Id owner = 0;
        bool destroy = false;
        params->vertical.changed = [&](auto,auto)
        {
            if (destroy) { std::string error; ensure("callback destroys container",tree.erase(owner,error)); }
        };
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        owner = *container;
        view.rect = {0,0,30,300};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(owner,*document,0,error);
        ensure("initial update",tree.updateScrollContainer(owner,error));
        const auto chrome = *tree.get(owner)->scrollContainer;
        ensure("reserve corner without horizontal",tree.get(chrome.vertical)->params.rect == LLVKWidgetTree::Rect{84,16,100,100});
        ensure_equals("corner does not reduce page",tree.get(chrome.vertical)->scrollbar->pageSize,100);
        tree.setScrollPosition(chrome.vertical,50,true,error);
        tree.reshape(*document,30,20,error);
        destroy = true;
        ensure("update tolerates callback destruction",tree.updateScrollContainer(owner,error));
        ensure("entire subtree gone",!tree.get(owner) && !tree.get(*document));
        destroy = false;
        view.rect = {0,0,100,100};
        container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        owner = *container;
        view.rect = {INT32_MIN,0,INT32_MAX,20};
        ensure("oversized document rejected at tree boundary",!tree.create(view,0,error));
        view.rect = {0,0,INT32_MAX,20};
        document = tree.create(view,0,error);
        ensure(error,document.has_value());
        tree.attachScrollContent(owner,*document,0,error);
        tree.setVisible(owner,false);
        ensure("hidden update skips content",tree.updateScrollContainer(owner,error));
        const auto hidden = tree.scrollContentWindow(owner,error);
        ensure(error,hidden.has_value());
        ensure_equals("hidden query supports maximum document extent",hidden->right,100);
    }

    template<> template<> void object::test<98>()
    {
        set_test_name("native scroll update aligns document and negotiates scrollbar pages");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,80};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        params->borderVisible = true;
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        view.rect = {0,0,200,300};
        auto document = tree.create(view,0,error);
        ensure(error,document.has_value());
        ensure("attach",tree.attachScrollContent(*container,*document,0,error));
        ensure("update",tree.updateScrollContainer(*container,error));
        const auto state = *tree.get(*container)->scrollContainer;
        ensure("both bars visible",tree.get(state.vertical)->params.visible && tree.get(state.horizontal)->params.visible);
        ensure_equals("horizontal page",tree.get(state.horizontal)->scrollbar->pageSize,82);
        ensure_equals("vertical page",tree.get(state.vertical)->scrollbar->pageSize,62);
        ensure("document aligned top left",tree.get(*document)->params.rect == LLVKWidgetTree::Rect{1,-221,201,79});
        const auto content = tree.scrollContentWindow(*container,error);
        ensure(error,content.has_value());
        ensure("source content window",*content == LLVKWidgetTree::Rect{1,16,83,78});
        tree.setScrollPosition(state.vertical,40,true,error);
        tree.setScrollPosition(state.horizontal,20,true,error);
        ensure("apply positions",tree.updateScrollContainer(*container,error));
        ensure("document offset",tree.get(*document)->params.rect == LLVKWidgetTree::Rect{-19,-181,181,119});
        ensure("shrink content",tree.reshape(*document,30,20,error));
        ensure("update smaller document",tree.updateScrollContainer(*container,error));
        ensure("bars hidden",!tree.get(state.vertical)->params.visible && !tree.get(state.horizontal)->params.visible);
        ensure_equals("vertical reset",tree.get(state.vertical)->scrollbar->position,0);
        ensure_equals("horizontal reset",tree.get(state.horizontal)->scrollbar->position,0);
        ensure("small content top aligned",tree.get(*document)->params.rect == LLVKWidgetTree::Rect{1,59,31,79});
    }

    template<> template<> void object::test<97>()
    {
        set_test_name("native scroll container owns chrome before init and content behind scrollbars");
        LLVKWidgetTree tree;
        tree.defineSetting("UIScrollbarSize",LLSD(16),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,80};
        LLVKControl::Params control;
        control.font = loadFont();
        auto params = std::make_shared<LLVKWidgetTree::ScrollContainerParams>();
        params->scrollbarControl.font = control.font;
        control.chrome = true;
        params->vertical.decreaseControl.font = params->vertical.increaseControl.font = control.font;
        params->horizontal.decreaseControl.font = params->horizontal.increaseControl.font = control.font;
        params->borderVisible = true;
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto& state = *tree.get(id)->scrollContainer;
            ensure("border before init",tree.get(state.border)->border.has_value());
            ensure("bars before init",tree.get(state.vertical)->scrollbar && tree.get(state.horizontal)->scrollbar);
            ensure("no document assigned to chrome",state.document == 0);
        };
        std::string error;
        auto container = tree.createScrollContainer(view,control,*params,0,error);
        ensure(error,container.has_value());
        const auto chrome = *tree.get(*container)->scrollContainer;
        ensure("bars initially hidden",!tree.get(chrome.vertical)->params.visible && !tree.get(chrome.horizontal)->params.visible);
        ensure_equals("vertical page inner height",tree.get(chrome.vertical)->scrollbar->pageSize,78);
        ensure_equals("container sets scrollbar step",tree.get(chrome.vertical)->scrollbar->params->stepSize,16);
        ensure("child control independent of parent chrome",!tree.get(chrome.vertical)->control->params.chrome);
        ensure_equals("horizontal page inner width",tree.get(chrome.horizontal)->scrollbar->pageSize,98);
        auto content = tree.create(view,0,error);
        ensure(error,content.has_value());
        ensure("attach content",tree.attachScrollContent(*container,*content,4,error));
        ensure_equals("first child selected as document",tree.get(*container)->scrollContainer->document,*content);
        ensure_equals("vertical in front",tree.get(*container)->children.at(0),chrome.vertical);
        ensure_equals("horizontal next",tree.get(*container)->children.at(1),chrome.horizontal);
        auto second = tree.create(view,0,error);
        ensure(error,second.has_value());
        ensure("attach second",tree.attachScrollContent(*container,*second,4,error));
        ensure_equals("first document retained",tree.get(*container)->scrollContainer->document,*content);
        ensure("chrome rejected as content",!tree.attachScrollContent(*container,chrome.border,0,error));
        ensure("erase owner",tree.erase(*container,error));
        ensure("all children destroyed",!tree.get(*content) && !tree.get(*second) && !tree.get(chrome.vertical));
    }

    template<> template<> void object::test<96>()
    {
        set_test_name("native scrollbar drag preserves pixel thumb and callback lifetime");
        for (bool vertical : {false,true})
        {
            LLVKWidgetTree tree;
            LLVKWidgetTree::Params view;
            view.rect = vertical ? LLVKWidgetTree::Rect{0,0,10,100} : LLVKWidgetTree::Rect{0,0,100,10};
            LLVKControl::Params control;
            control.font = loadFont();
            LLVKWidgetTree::ScrollbarParams params;
            params.decreaseControl.font = params.increaseControl.font = control.font;
            params.vertical = vertical; params.thickness = 10;
            params.documentSize = 1000; params.pageSize = 100;
            int changes = 0;
            bool destroy = false;
            params.changed = [&](auto id,auto)
            {
                ++changes;
                if (destroy) { std::string error; ensure("erase in drag callback",tree.erase(id,error)); }
            };
            std::string error;
            auto bar = tree.createScrollbar(view,control,params,0,error);
            ensure(error,bar.has_value());
            LLVKWidgetTree::PointerEvent event;
            event.kind = LLVKWidgetTree::PointerKind::LeftDown;
            event.x = vertical ? 5 : 15; event.y = vertical ? 80 : 5;
            ensure("thumb down",tree.routePointer(*bar,event,error));
            ensure_equals("thumb captures",tree.mouseCapture(),*bar);
            event.kind = LLVKWidgetTree::PointerKind::Hover;
            if (vertical) event.y -= 31; else event.x += 31;
            ensure("drag",tree.routePointer(*bar,event,error));
            ensure_equals("rounded position",tree.get(*bar)->scrollbar->position,436);
            const auto dragged = tree.get(*bar)->scrollbar->thumb;
            ensure_equals("one callback",changes,1);
            ensure("unchanged hover",tree.routePointer(*bar,event,error));
            ensure_equals("same drag does not notify",changes,1);
            event.kind = LLVKWidgetTree::PointerKind::LeftUp;
            ensure("release",tree.routePointer(*bar,event,error));
            ensure_equals("capture released",tree.mouseCapture(),LLVKWidgetTree::Id(0));
            ensure("release keeps pixel thumb",tree.get(*bar)->scrollbar->thumb == dragged);
            event.kind = LLVKWidgetTree::PointerKind::DoubleClick;
            event.x = vertical ? 5 : dragged.left+1; event.y = vertical ? dragged.bottom+1 : 5;
            ensure("double click begins drag",tree.routePointer(*bar,event,error));
            destroy = true;
            event.kind = LLVKWidgetTree::PointerKind::Hover;
            if (vertical) event.y = -100; else event.x = 200;
            ensure("drag callback deletion",tree.routePointer(*bar,event,error));
            ensure("bar removed",!tree.get(*bar));
            ensure_equals("capture cleared by teardown",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        }
    }

    template<> template<> void object::test<95>()
    {
        set_test_name("native scrollbar keys and wheel retain propagation and page overlap rules");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,10,100};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::ScrollbarParams params;
        params.decreaseControl.font = params.increaseControl.font = control.font;
        params.vertical = true; params.thickness = 10;
        params.documentSize = 100; params.pageSize = 20; params.stepSize = 3;
        std::string error;
        auto scrollbar = tree.createScrollbar(view,control,params,0,error);
        ensure(error,scrollbar.has_value());
        using Key = LLVKWidgetTree::ScrollKey;
        ensure("Home handled even unchanged",tree.scrollbarKey(*scrollbar,Key::Home,error));
        ensure("wheel at beginning propagates",!tree.scrollbarWheel(*scrollbar,-1,false,error));
        ensure("horizontal wheel skips vertical",!tree.scrollbarWheel(*scrollbar,1,true,error));
        ensure("ordinary wheel moves",tree.scrollbarWheel(*scrollbar,2,false,error));
        ensure_equals("wheel step",tree.get(*scrollbar)->scrollbar->position,6);
        ensure("page down propagates after movement",!tree.scrollbarKey(*scrollbar,Key::PageDown,error));
        ensure_equals("page overlap one",tree.get(*scrollbar)->scrollbar->position,25);
        ensure("page up propagates after movement",!tree.scrollbarKey(*scrollbar,Key::PageUp,error));
        ensure_equals("page up overlap one",tree.get(*scrollbar)->scrollbar->position,6);
        ensure("End handled",tree.scrollbarKey(*scrollbar,Key::End,error));
        ensure_equals("End maximum",tree.get(*scrollbar)->scrollbar->position,80);
        ensure("wheel at end propagates",!tree.scrollbarWheel(*scrollbar,1,false,error));
        ensure("large negative wheel clamps",tree.scrollbarWheel(*scrollbar,INT32_MIN,false,error));
        ensure_equals("large wheel no overflow",tree.get(*scrollbar)->scrollbar->position,0);
        tree.setVisible(*scrollbar,false);
        ensure("hidden but scrollable still handles",tree.scrollbarKey(*scrollbar,Key::Down,error));
        ensure("zero scroll range",tree.setScrollPageSize(*scrollbar,100,error));
        ensure("hidden without scroll range propagates",!tree.scrollbarKey(*scrollbar,Key::Home,error));
    }

    template<> template<> void object::test<94>()
    {
        set_test_name("native scrollbar reshape stages arrow extents and thumb atomically");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,10,100};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::ScrollbarParams params;
        params.decreaseControl.font = params.increaseControl.font = control.font;
        params.vertical = true; params.thickness = 10;
        params.documentSize = 1000; params.pageSize = 100;
        std::string error;
        auto scrollbar = tree.createScrollbar(view,control,params,0,error);
        ensure(error,scrollbar.has_value());
        ensure("short vertical resize",tree.reshape(*scrollbar,10,12,error));
        const auto state = *tree.get(*scrollbar)->scrollbar;
        ensure("upper button shrinks and anchors",tree.get(state.decrease)->params.rect == LLVKWidgetTree::Rect{0,6,10,12});
        ensure("lower button shrinks",tree.get(state.increase)->params.rect == LLVKWidgetTree::Rect{0,0,10,6});
        ensure("zero thumb track",state.thumb.top == state.thumb.bottom);
        const auto before = tree.get(*scrollbar)->params.rect;
        ensure("negative scrollbar resize rejects",!tree.reshape(*scrollbar,10,-1,error));
        ensure("parent geometry unchanged",tree.get(*scrollbar)->params.rect == before);
        ensure("button geometry unchanged",tree.get(state.decrease)->params.rect == LLVKWidgetTree::Rect{0,6,10,12});
        ensure("thumb unchanged",tree.get(*scrollbar)->scrollbar->thumb == state.thumb);
        params.vertical = false;
        view.rect = {0,0,100,10};
        auto horizontal = tree.createScrollbar(view,control,params,0,error);
        ensure(error,horizontal.has_value());
        ensure("horizontal shape",tree.setShape(*horizontal,{20,30,32,40},error));
        const auto other = *tree.get(*horizontal)->scrollbar;
        ensure("left arrow",tree.get(other.decrease)->params.rect == LLVKWidgetTree::Rect{0,0,6,10});
        ensure("right arrow",tree.get(other.increase)->params.rect == LLVKWidgetTree::Rect{6,0,12,10});
    }

    template<> template<> void object::test<93>()
    {
        set_test_name("native scrollbar constructs button owners and notifies before thumb refresh");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,10,100};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::ScrollbarParams params;
        params.decreaseControl.font = params.increaseControl.font = control.font;
        params.vertical = true; params.thickness = 10;
        params.documentSize = 1000; params.pageSize = 100; params.stepSize = 5;
        int changes = 0;
        bool destroy = false;
        LLVKScrollLayout::Rect thumbAtCallback;
        params.changed = [&](auto id,auto position)
        {
            ++changes;
            ensure_equals("new position published before callback",tree.get(id)->scrollbar->position,position);
            thumbAtCallback = tree.get(id)->scrollbar->thumb;
            if (destroy) { std::string error; ensure("destroy in changed callback",tree.erase(id,error)); }
        };
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto& state = *tree.get(id)->scrollbar;
            ensure("real buttons before init",tree.get(state.decrease)->button && tree.get(state.increase)->button);
        };
        std::string error;
        auto scrollbar = tree.createScrollbar(view,control,params,0,error);
        ensure(error,scrollbar.has_value());
        const auto before = tree.get(*scrollbar)->scrollbar->thumb;
        ensure("scroll changed",tree.setScrollPosition(*scrollbar,450,true,error));
        ensure("callback sees old thumb",thumbAtCallback == before);
        ensure("thumb updated after callback",tree.get(*scrollbar)->scrollbar->thumb != before);
        ensure("same position no callback",!tree.setScrollPosition(*scrollbar,450,true,error));
        ensure_equals("one position notification",changes,1);
        ensure("page clamps position",tree.setScrollPageSize(*scrollbar,900,error));
        ensure_equals("position clamped",tree.get(*scrollbar)->scrollbar->position,100);
        const auto decrease = tree.get(*scrollbar)->scrollbar->decrease;
        ensure("button changes position",tree.buttonUnicode(decrease,U' ',false,error));
        ensure_equals("decrease step",tree.get(*scrollbar)->scrollbar->position,95);
        ensure("small document clamps",tree.setScrollDocumentSize(*scrollbar,50,error));
        ensure_equals("no scrolling when page exceeds doc",tree.get(*scrollbar)->scrollbar->position,0);
        ensure("restore range",tree.setScrollDocumentSize(*scrollbar,1000,error));
        destroy = true;
        ensure("callback deletion safe",tree.setScrollPosition(*scrollbar,50,true,error));
        ensure("owner and children gone",!tree.get(*scrollbar) && !tree.get(decrease));
    }

    template<> template<> void object::test<92>()
    {
        set_test_name("native scrollbar thumb geometry preserves integer sizing and axis conventions");
        LLVKScrollLayout::ThumbParams params;
        params.length = 100; params.thickness = 10;
        params.documentSize = 1000; params.pageSize = 100;
        std::string error;
        auto thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("vertical start thumb",*thumb == LLVKScrollLayout::Rect{0,74,10,90});
        params.position = 900;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("vertical end thumb",*thumb == LLVKScrollLayout::Rect{0,10,10,26});
        params.vertical = false;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("horizontal end thumb",*thumb == LLVKScrollLayout::Rect{74,0,90,10});
        params.position = 450;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("horizontal middle thumb",*thumb == LLVKScrollLayout::Rect{42,0,58,10});
        params.documentSize = 0; params.pageSize = 0;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("empty document fills track",*thumb == LLVKScrollLayout::Rect{10,0,90,10});
        params.length = 25; params.documentSize = 1000; params.pageSize = 1;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure(error,thumb.has_value());
        ensure("small track caps minimum thumb",*thumb == LLVKScrollLayout::Rect{10,0,15,10});
        params.length = INT32_MAX; params.documentSize = INT32_MAX;
        params.pageSize = INT32_MAX/2; params.position = INT32_MAX/2;
        thumb = LLVKScrollLayout::thumb(params,error);
        ensure("large products use checked wide arithmetic",thumb.has_value() && thumb->right <= INT32_MAX);
        params.documentSize = -1;
        ensure("negative document rejects",!LLVKScrollLayout::thumb(params,error));
    }

    template<> template<> void object::test<91>()
    {
        set_test_name("native scroll visibility preserves one-pixel allowance and cross-axis dependencies");
        LLVKScrollLayout::Params params;
        params.width = 102; params.height = 102;
        params.borderWidth = 1; params.scrollbarSize = 16;
        params.documentWidth = 101; params.documentHeight = 101;
        std::string error;
        auto visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure("one pixel overflow does not show bars",!visible->horizontal && !visible->vertical);
        ensure_equals("border removed from width",visible->width,100);
        ensure_equals("border removed from height",visible->height,100);
        params.documentHeight = 102;
        visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure("vertical creates horizontal need",visible->horizontal && visible->vertical);
        ensure_equals("both bars narrow width",visible->width,84);
        ensure_equals("both bars narrow height",visible->height,84);
        params.documentWidth = 102; params.documentHeight = 100;
        visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure("horizontal creates vertical need",visible->horizontal && visible->vertical);
        params.documentWidth = 84; params.documentHeight = 200;
        visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure("narrow document only needs vertical",visible->vertical && !visible->horizontal);
        params.hideScrollbars = true;
        visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure("hidden bars do not reserve room",!visible->vertical && !visible->horizontal && visible->width == 100 && visible->height == 100);
        params.width = 0;
        visible = LLVKScrollLayout::visible(params,error);
        ensure(error,visible.has_value());
        ensure_equals("signed small-window content retained",visible->width,-2);
        params.borderWidth = INT32_MAX;
        ensure("border arithmetic overflow rejects",!LLVKScrollLayout::visible(params,error));
    }

    template<> template<> void object::test<90>()
    {
        set_test_name("native plain append produces styled newline spans and preserves terminal owner");
        LLVKStyledTextSegment::Params defaults;
        defaults.font = loadFont();
        auto alternate = defaults;
        alternate.font = loadFont();
        std::string error;
        auto document = LLVKStyledTextDocument::create(U"",defaults,error);
        ensure(error,document.has_value());
        ensure("empty append ignores prepend",document->appendPlain(U"",alternate,true,error));
        ensure("empty remains empty",document->text().empty());
        ensure("literal append",document->appendPlain(U"one\n\ntwo\n",alternate,false,error));
        ensure("literal text retained",document->text() == U"one\n\ntwo\n");
        ensure_equals("normal break break normal break EOF",document->segments().size(),std::size_t(6));
        ensure("new text uses supplied font",document->segments().front().params().font == alternate.font);
        ensure("EOF keeps original font",document->segments().back().params().font == defaults.font);
        LLVKPlainTextLayout::Options options;
        options.width = 200;
        ensure("produced segments reflow",document->reflow(options,error));
        ensure_equals("blank and final lines retained",document->lines()->size(),std::size_t(4));
        ensure("prepend append",document->appendPlain(U"end",defaults,true,error));
        ensure("explicit extra newline retained",document->text() == U"one\n\ntwo\n\nend");
        const auto before = document->text();
        ensure("over-budget newline segments reject",!document->appendPlain(std::u32string(10001,U'\n'),defaults,false,error));
        ensure("failed append leaves text and ranges unchanged",document->text() == before);
        ensure("document remains reflowable",document->reflow(options,error));
    }

    template<> template<> void object::test<89>()
    {
        set_test_name("native styled truncation preserves scalar boundaries styles and EOF at every byte limit");
        LLVKStyledTextSegment::Params defaults;
        defaults.font = loadFont();
        const std::u32string text = U"A\u00e9\u3042\U0001f600Z";
        const std::size_t boundaries[]{1,3,6,10,11};
        std::string error;
        for (std::size_t limit = 0; limit <= 12; ++limit)
        {
            auto document = LLVKStyledTextDocument::create(text,defaults,error);
            ensure(error,document.has_value());
            auto highlighted = defaults;
            highlighted.begin = 1; highlighted.end = 4; highlighted.highlightBackground = true;
            ensure("style overlay",document->overlay(highlighted,error));
            const auto expected = static_cast<std::size_t>(std::count_if(std::begin(boundaries),std::end(boundaries),
                [&](auto boundary) { return boundary <= limit; }));
            const auto truncated = document->truncate(limit,error);
            ensure(error,truncated.has_value());
            ensure_equals("reports actual truncation",*truncated,limit < 11);
            ensure("exact scalar prefix",document->text() == text.substr(0,expected));
            ensure_equals("EOF follows truncated text",document->segments().back().params().end,expected+1);
            if (expected > 1)
            {
                ensure("prefix style retained",!document->segments().at(1).editable());
                ensure_equals("clipped style end",document->segments().at(1).params().end,std::min(expected,std::size_t(4)));
            }
            const auto again = document->truncate(limit,error);
            ensure(error,again.has_value());
            ensure("truncation idempotent",!*again);
        }
    }

    template<> template<> void object::test<88>()
    {
        set_test_name("native styled edits preserve style spans immutable ranges and EOF");
        LLVKStyledTextSegment::Params defaults;
        defaults.font = loadFont();
        std::string error;
        auto document = LLVKStyledTextDocument::create(U"abcdef",defaults,error);
        ensure(error,document.has_value());
        auto highlight = defaults;
        highlight.begin = 2; highlight.end = 5; highlight.highlightBackground = true;
        ensure("highlight range",document->overlay(highlight,error));
        const auto insertion = document->insert(3,U"XY",error);
        ensure(error,insertion.has_value());
        ensure_equals("insert snaps past noneditable span",insertion->position,std::size_t(5));
        ensure("inserted text",document->text() == U"abcdeXYf");
        ensure_equals("highlight unchanged",document->segments().at(1).params().end,std::size_t(5));
        ensure_equals("following editable span grows",document->segments().back().params().end,std::size_t(9));
        const auto boundary = document->insert(2,U"Z",error);
        ensure(error,boundary.has_value());
        ensure_equals("editable predecessor extends",document->segments().front().params().end,std::size_t(3));
        ensure_equals("highlight shifts",document->segments().at(1).params().begin,std::size_t(3));
        const auto removed = document->erase(1,6,error);
        ensure(error,removed.has_value());
        ensure_equals("removed count",removed->removed,std::size_t(6));
        ensure("cross-span deletion",document->text() == U"aYf");
        ensure_equals("noneditable span removed",document->segments().size(),std::size_t(2));
        ensure_equals("EOF retained",document->segments().back().params().end,std::size_t(4));
        ensure("clear all text",document->erase(0,100,error).has_value());
        ensure("empty text",document->text().empty());
        ensure_equals("empty document EOF segment",document->segments().back().params().end,std::size_t(1));
        ensure("invalid insertion rejects",!document->insert(0,std::u32string(1,0xd800),error));
        ensure("failed insertion leaves empty document",document->text().empty());
        auto leading = LLVKStyledTextDocument::create(U"abc",defaults,error);
        ensure(error,leading.has_value());
        highlight.begin = 0; highlight.end = 2;
        ensure("leading noneditable",leading->overlay(highlight,error));
        ensure("insert before leading indivisible",leading->insert(0,U"X",error).has_value());
        ensure("native default editable prefix",leading->segments().front().editable());
        ensure("leading insertion text",leading->text() == U"Xabc");
        LLVKPlainTextLayout::Options options;
        options.width = 100;
        ensure("edited document reflows",leading->reflow(options,error));
    }

    template<> template<> void object::test<87>()
    {
        set_test_name("native styled document overlays preserve coverage and editable boundaries");
        LLVKStyledTextSegment::Params defaults;
        defaults.font = loadFont();
        std::string error;
        auto document = LLVKStyledTextDocument::create(U"abcdef",defaults,error);
        ensure(error,document.has_value());
        ensure_equals("default EOF coverage",document->segments().front().params().end,std::size_t(7));
        LLVKPlainTextLayout::Options options;
        options.width = 100;
        ensure("initial reflow",document->reflow(options,error));
        ensure("clean reflow index",!document->reflowIndex());
        auto highlight = defaults;
        highlight.begin = 2; highlight.end = 5; highlight.highlightBackground = true;
        ensure("overlay highlight",document->overlay(highlight,error));
        ensure_equals("prefix overlay suffix",document->segments().size(),std::size_t(3));
        ensure_equals("invalidated containing segment",*document->reflowIndex(),std::size_t(0));
        ensure("old lines invalidated",!document->lines());
        ensure_equals("forward snaps interior",document->editableIndex(3,true),std::size_t(5));
        ensure_equals("backward snaps interior",document->editableIndex(3,false),std::size_t(2));
        ensure_equals("boundary unchanged",document->editableIndex(2,true),std::size_t(2));
        ensure_equals("editable predecessor at boundary",*document->editableSegment(2),std::size_t(0));
        auto overlay = defaults;
        overlay.begin = 1; overlay.end = 3;
        ensure("overlapping replacement",document->overlay(overlay,error));
        ensure_equals("remaining highlight begins after overlap",document->segments().at(2).params().begin,std::size_t(3));
        ensure("remaining highlight intact",!document->segments().at(2).editable());
        const auto count = document->segments().size();
        overlay.end = 100;
        ensure("invalid overlay rejects",!document->overlay(overlay,error));
        ensure_equals("failed overlay atomic",document->segments().size(),count);
        ensure("reset defaults",document->resetSegments(error));
        ensure_equals("one default restored",document->segments().size(),std::size_t(1));
        ensure("default owner reflows",document->reflow(options,error));
        ensure("invalid document scalar rejects",!LLVKStyledTextDocument::create(std::u32string(1,0x110000),defaults,error));
    }

    template<> template<> void object::test<86>()
    {
        set_test_name("native segmented reflow preserves plain lines and mixed inline heights");
        const auto font = loadFont();
        const std::u32string text = U"one two three\nlast";
        LLVKStyledTextSegment::Params params;
        params.font = font;
        std::vector<LLVKStyledTextSegment> segments;
        std::string error;
        const auto append = [&](std::size_t begin, std::size_t end, LLVKStyledTextSegment::Kind kind)
        {
            params.begin = begin; params.end = end; params.kind = kind;
            auto segment = LLVKStyledTextSegment::create(params,text,error);
            ensure(error,segment.has_value());
            segments.push_back(std::move(*segment));
        };
        const auto newline = text.find(U'\n');
        append(0,newline,LLVKStyledTextSegment::Kind::Normal);
        append(newline,newline+1,LLVKStyledTextSegment::Kind::LineBreak);
        append(newline+1,text.size()+1,LLVKStyledTextSegment::Kind::Normal);
        LLVKPlainTextLayout::Options options;
        options.width = 45; options.wrap = true; options.horizontalPadding = 2;
        const auto plain = LLVKPlainTextLayout::plain(text,*font,options,error);
        ensure(error,plain.has_value());
        const auto rich = LLVKStyledTextSegment::reflow(text,segments,options,error);
        ensure(error,rich.has_value());
        ensure_equals("same line count",rich->size(),plain->size());
        for (std::size_t index = 0; index < rich->size(); ++index)
        {
            const auto& actual = rich->at(index);
            const auto& expected = plain->at(index);
            ensure("same ranges",actual.begin == expected.begin && actual.end == expected.end && actual.paragraph == expected.paragraph);
            ensure("same rectangles",actual.left == expected.left && actual.right == expected.right &&
                actual.top == expected.top && actual.bottom == expected.bottom);
        }
        segments.clear();
        params.widget = 1; params.widgetWidth = 10; params.widgetHeight = 80;
        params.forceNewLine = true;
        append(0,3,LLVKStyledTextSegment::Kind::Normal);
        append(3,7,LLVKStyledTextSegment::Kind::InlineWidget);
        append(7,text.size()+1,LLVKStyledTextSegment::Kind::Normal);
        const auto mixed = LLVKStyledTextSegment::reflow(text,segments,options,error);
        ensure(error,mixed.has_value());
        ensure("inline forces split",mixed->size() > 1 && mixed->front().end == 3);
        ensure("widget height retained",std::any_of(mixed->begin(),mixed->end(),[](const auto& line) { return line.top-line.bottom == 80; }));
        segments.erase(segments.begin());
        ensure("gap rejected",!LLVKStyledTextSegment::reflow(text,segments,options,error));
    }

    template<> template<> void object::test<85>()
    {
        set_test_name("native image and inline widget metrics retain distinct fit thresholds");
        LLVKStyledTextSegment::Params params;
        params.font = loadFont();
        params.image = image("inline");
        params.kind = LLVKStyledTextSegment::Kind::Image;
        params.end = 1;
        std::string error;
        auto segment = LLVKStyledTextSegment::create(params,U" ",error);
        ensure(error,segment.has_value());
        const auto dimensions = segment->measure(U" ",0,1,error);
        ensure(error,dimensions.has_value());
        ensure_equals("image padded width",dimensions->width,4.f);
        ensure("image retains independent base emoji flag",segment->permitsEmoji() && !segment->editable());
        ensure_equals("image exact fit midline rejected",*segment->fit(U" ",4,0,1,1,error),std::size_t(0));
        ensure_equals("image extra pixel fits",*segment->fit(U" ",5,0,1,1,error),std::size_t(1));
        ensure_equals("image forced at line start",*segment->fit(U" ",0,0,0,1,error),std::size_t(1));
        params.kind = LLVKStyledTextSegment::Kind::InlineWidget;
        params.widget = 17;
        params.widgetWidth = 10; params.widgetHeight = 20;
        params.leftPad = 2; params.rightPad = 3;
        params.bottomPad = 4; params.topPad = 5;
        params.end = 3;
        auto widget = LLVKStyledTextSegment::create(params,U"abc",error);
        ensure(error,widget.has_value());
        const auto size = widget->measure(U"abc",0,3,error);
        ensure(error,size.has_value());
        ensure_equals("widget padded width",size->width,15.f);
        ensure_equals("widget padded height",size->height,29);
        ensure_equals("widget exact fit accepted",*widget->fit(U"abc",15,0,1,3,error),std::size_t(3));
        ensure_equals("widget cannot fit midline",*widget->fit(U"abc",14,0,1,3,error),std::size_t(0));
        ensure_equals("widget forced at line start",*widget->fit(U"abc",0,0,0,3,error),std::size_t(3));
        ensure("inline cannot split for emoji",!widget->permitsEmoji());
        params.forceNewLine = true;
        auto forced = LLVKStyledTextSegment::create(params,U"abc",error);
        ensure(error,forced.has_value());
        ensure_equals("forced widget skips line zero",*forced->fit(U"abc",100,0,0,3,error,0),std::size_t(0));
        ensure_equals("forced widget appears next line",*forced->fit(U"abc",100,0,0,3,error,1),std::size_t(3));
        const auto blank = forced->measure(U"abc",0,0,error);
        ensure(error,blank.has_value());
        ensure("forced blank keeps font height and breaks",blank->lineBreak && blank->height > 0 && blank->width == 0.f);
        params.widgetWidth = INT32_MAX;
        ensure("padded extent overflow rejects",!LLVKStyledTextSegment::create(params,U"abc",error));
    }

    template<> template<> void object::test<84>()
    {
        set_test_name("native styled segments retain word-wrap EOF and line-break metrics");
        const std::u32string text = U"ABC";
        LLVKStyledTextSegment::Params params;
        params.font = loadFont();
        params.end = text.size()+1;
        std::string error;
        auto segment = LLVKStyledTextSegment::create(params,text,error);
        ensure(error,segment.has_value());
        auto first = segment->fit(text,0,0,0,4,error);
        ensure(error,first.has_value());
        ensure_equals("line start forces progress",*first,std::size_t(1));
        const auto midline = segment->fit(text,0,0,1,4,error);
        ensure(error,midline.has_value());
        ensure_equals("midline requires whole word",*midline,std::size_t(0));
        const auto all = segment->fit(text,1000,0,0,4,error);
        ensure(error,all.has_value());
        ensure_equals("fit includes EOF",*all,std::size_t(4));
        const auto eof = segment->measure(text,3,1,error);
        ensure(error,eof.has_value());
        ensure_equals("EOF width",eof->width,0.f);
        ensure("EOF contributes line height",eof->height > 0);
        const auto empty = segment->measure(text,0,0,error);
        ensure(error,empty.has_value());
        ensure_equals("empty normal run no height",empty->height,0);
        const auto hit = segment->hit(text,1000,0,4,true,error);
        ensure(error,hit.has_value());
        ensure_equals("normal hit count reserves EOF",*hit,std::size_t(3));
        params.highlightBackground = true;
        auto highlighted = LLVKStyledTextSegment::create(params,text,error);
        ensure(error,highlighted.has_value());
        ensure("highlight prevents editing and emoji splitting",!highlighted->editable() && !highlighted->permitsEmoji());
        params.kind = LLVKStyledTextSegment::Kind::LineBreak;
        params.end = 1;
        auto newline = LLVKStyledTextSegment::create(params,U"\n",error);
        ensure(error,newline.has_value());
        const auto metrics = newline->measure(U"\n",0,1,error);
        ensure(error,metrics.has_value());
        ensure("newline forces break",metrics->lineBreak && metrics->height > 0 && metrics->width == 0.f);
        ensure("newline retains independent base emoji flag",newline->permitsEmoji() && !newline->editable());
        ensure("wrong line-break text rejects",!LLVKStyledTextSegment::create(params,U"A",error));
        ensure("stale segment range rejects",!segment->measure(U"",0,4,error));
    }

    template<> template<> void object::test<83>()
    {
        set_test_name("native checkbox construction samples horizontal padding without live reshaping");
        LLVKWidgetTree tree;
        tree.defineSetting("UICheckboxctrlHPad",LLSD(2),LLVKWidgetTree::SettingType::Integer);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,90,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::CheckBoxConstruction construction;
        construction.labelControl.font = control.font;
        construction.buttonControl.font = control.font;
        construction.label = "A wrapped checkbox label";
        construction.wrap = LLVKWidgetTree::CheckBoxWrap::Down;
        construction.labelView.rect = {20,3,20,3};
        construction.buttonView.rect = {2,1,15,14};
        construction.button.toggle = true;
        std::string error;
        auto first = tree.createCheckBox(view,control,construction,0,error);
        ensure(error,first.has_value());
        ensure_equals("native configured padding",tree.get(*first)->checkBox->construction.horizontalPadding,2);
        const auto label = tree.get(*first)->checkBox->label;
        const auto before = tree.get(label)->params.rect;
        ensure("update native setting",tree.updateSetting("UICheckboxctrlHPad",LLSD(35)));
        ensure("existing label does not reactively reshape",tree.get(label)->params.rect == before);
        auto second = tree.createCheckBox(view,control,construction,0,error);
        ensure(error,second.has_value());
        ensure_equals("next construction samples update",tree.get(*second)->checkBox->construction.horizontalPadding,35);
        const auto secondLabel = tree.get(*second)->checkBox->label;
        ensure("narrower initial label wraps more",tree.get(secondLabel)->params.rect.top-tree.get(secondLabel)->params.rect.bottom > before.top-before.bottom);
    }

    template<> template<> void object::test<82>()
    {
        set_test_name("native checkbox XML retains embedded bindings callbacks and nested overrides");
        LLVKWidgetTree tree;
        tree.defineSetting("checked",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        LLVKWidgetFactory::Resources resources;
        resources.fonts["outer"] = loadFont();
        resources.fonts["inner"] = loadFont();
        LLVKWidgetFactory::CheckBoxDefaults defaults;
        defaults.control.font = resources.fonts.at("outer");
        defaults.construction.labelControl.font = resources.fonts.at("inner");
        defaults.construction.buttonControl.font = resources.fonts.at("inner");
        defaults.construction.button.toggle = true;
        defaults.buttonView.geometry.width = {13,true};
        defaults.buttonView.geometry.height = {13,true};
        int commits = 0;
        bool predicate = true;
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["changed"] = [&](auto,const LLSD&) { ++commits; };
        callbacks.predicates["checked"] = [&](auto,const LLSD&) { return predicate; };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources, {}, {}, defaults);
        std::string error;
        auto checkbox = factory.construct(tree,
            "<check_box width='150' height='23' font='outer' label='Choice' control_name='checked' initial_value='true'>"
            "<check_box.label_text left='20' bottom='3' width='0' height='0' font='inner'/>"
            "<check_box.check_button left='2' bottom='1' width='15' height='15'/>"
            "<check_box.on_check function='checked'/><check_box.commit_callback function='changed'/></check_box>",0,error);
        ensure(error,checkbox.has_value());
        const auto children = *tree.get(*checkbox)->checkBox;
        ensure("setting overrides initial checkbox value",!tree.value(*checkbox).asBoolean());
        ensure("inner binding owns value",tree.get(children.button)->control->params.valueSetting == "checked");
        ensure("outer font overrides nested label font",tree.get(children.label)->control->params.font == resources.fonts.at("outer"));
        ensure("nested button keeps own font",tree.get(children.button)->control->params.font == resources.fonts.at("inner"));
        ensure("predicate resolved",tree.refreshCheckBox(*checkbox));
        ensure("predicate updates native toggle",tree.value(*checkbox).asBoolean());
        predicate = false;
        ensure("predicate refresh",tree.refreshCheckBox(*checkbox));
        ensure("predicate updates false",!tree.value(*checkbox).asBoolean());
        ensure("toggle activation",tree.buttonUnicode(children.button,U' ',false,error));
        ensure_equals("declared commit callback",commits,1);
        const auto size = tree.size();
        ensure("unknown checkbox predicate rejects",!factory.construct(tree,
            "<check_box><check_box.on_check function='missing'/></check_box>",0,error));
        ensure_equals("unknown callback leaves tree intact",tree.size(),size);
        ensure("invalid boolean rejects",!factory.construct(tree,"<check_box initial_value='maybe'/>",0,error));
    }

    template<> template<> void object::test<81>()
    {
        set_test_name("native preedit geometry preserves nested coordinates scale and range checks");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {100,200,500,400};
        std::string error;
        auto parent = tree.create(view,0,error);
        ensure(error,parent.has_value());
        view.rect = {10,20,210,43};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.leftPadding = 3;
        auto editor = tree.createLineEditor(view,control,params,*parent,error);
        ensure(error,editor.has_value());
        ensure("composition",tree.updateLinePreedit(*editor,U"abc",{1,2},{false,true},1,error));
        const auto geometry = tree.linePreeditLocation(*editor,-1,2.f,2.f,error);
        ensure(error,geometry.has_value());
        const auto first = tree.get(*editor)->lineEditor->text.pixelPosition(0,error);
        const auto caret = tree.get(*editor)->lineEditor->text.pixelPosition(1,error);
        const auto last = tree.get(*editor)->lineEditor->text.pixelPosition(3,error);
        ensure("measured locations",first && caret && last);
        ensure_equals("scaled caret X",geometry->x,2*(110+*caret));
        ensure_equals("scaled half-height caret Y",geometry->y,462);
        ensure_equals("bounds left",geometry->bounds.left,2*(110+*first));
        ensure_equals("bounds right",geometry->bounds.right,2*(110+*last));
        ensure_equals("bounds bottom",geometry->bounds.bottom,440);
        ensure_equals("bounds top",geometry->bounds.top,486);
        ensure("source control-rectangle offset retained",geometry->control == LLVKWidgetTree::Rect{240,480,640,526});
        ensure_equals("preedit position",geometry->position,std::size_t(0));
        ensure_equals("preedit length",geometry->length,std::size_t(3));
        ensure("font size positive",geometry->fontSize > 0);
        ensure("out-of-range query rejects",!tree.linePreeditLocation(*editor,4,1.f,1.f,error));
        ensure("nonpositive scale rejects",!tree.linePreeditLocation(*editor,0,0.f,1.f,error));
        ensure("scaled overflow rejects",!tree.linePreeditLocation(*editor,0,1.e20f,1.f,error));
    }

    template<> template<> void object::test<80>()
    {
        set_test_name("native preedit owns clauses overwrite restoration and composition ordering");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,200,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "abcd";
        params.text.maximumBytes = 4;
        int strokes = 0, validations = 0;
        params.prevalidator = [&](auto) { ++validations; return false; };
        params.keystroke.function = [&](auto,const LLSD&) { ++strokes; };
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        ensure("initial reset",tree.resetLinePreedit(*editor,error));
        ensure("preedit beyond ordinary cap",tree.updateLinePreedit(*editor,U"\u3042\u3044",{1,1},{true,false},1,error));
        const auto& text = tree.get(*editor)->lineEditor->text;
        ensure("native preedit present",text.hasPreedit());
        ensure("clause boundaries",text.preedit().positions == std::vector<std::size_t>({4,5,6}));
        ensure_equals("composition caret",text.cursor(),std::size_t(5));
        ensure_equals("preedit bypasses whole validator",validations,0);
        ensure_equals("update notifies",strokes,1);
        ensure("missing reset rejects",!tree.updateLinePreedit(*editor,U"x",{1},{false},0,error));
        ensure("reset composition",tree.resetLinePreedit(*editor,error));
        ensure_equals("reset restores base",tree.value(*editor).asString(),std::string("abcd"));
        ensure_equals("reset does not notify",strokes,1);
        ensure("bad clause sum rejects",!tree.updateLinePreedit(*editor,U"xy",{1},{false},1,error));
        ensure("malformed preedit atomic",!tree.get(*editor)->lineEditor->text.hasPreedit());
        params.prevalidator = {};
        auto overwrite = tree.createLineEditor(view,control,params,0,error);
        ensure(error,overwrite.has_value());
        tree.setKeyboardFocus(*overwrite,false,false,error);
        tree.lineEditorKey(*overwrite,LLVKLineEditor::Key::Home,{},error);
        tree.lineEditorKey(*overwrite,LLVKLineEditor::Key::Insert,{},error);
        ensure("overwrite composition",tree.updateLinePreedit(*overwrite,U"XY",{2},{true},1,error));
        ensure_equals("overwritten text retained",tree.value(*overwrite).asString(),std::string("XYcd"));
        ensure("original range retained",tree.get(*overwrite)->lineEditor->text.preedit().overwritten == U"ab");
        ensure("reset overwrite",tree.resetLinePreedit(*overwrite,error));
        ensure_equals("overwrite restored",tree.value(*overwrite).asString(),std::string("abcd"));
        ensure("mark reconversion",tree.markLinePreedit(*overwrite,1,2,error));
        ensure("reset marked overwrite",tree.resetLinePreedit(*overwrite,error));
        ensure_equals("marked overwrite retains text",tree.value(*overwrite).asString(),std::string("abcd"));
        tree.lineEditorKey(*overwrite,LLVKLineEditor::Key::Insert,{},error);
        ensure("mark insert reconversion",tree.markLinePreedit(*overwrite,1,2,error));
        ensure("reset marked insertion",tree.resetLinePreedit(*overwrite,error));
        ensure_equals("marked insert removes range",tree.value(*overwrite).asString(),std::string("ad"));
        tree.setEnabled(*overwrite,false);
        ensure("readonly composition refuses",!tree.updateLinePreedit(*overwrite,U"x",{1},{false},1,error));
    }

    template<> template<> void object::test<79>()
    {
        set_test_name("native editor pointer selection owns capture focus and release ordering");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {10,20,210,43};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "one two";
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        int beams = 0;
        bool endedBeforeUp = false;
        LLVKWidgetTree::Events events;
        events.textCursor = [&](auto) { ++beams; };
        events.pointer = [&](auto,const auto& event)
        { if (event.kind == LLVKWidgetTree::PointerKind::LeftUp) endedBeforeUp = !tree.get(*editor)->lineEditor->text.selecting(); };
        tree.setEvents(*editor,events);
        LLVKWidgetTree::PointerEvent event;
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.x = 10; event.y = 25; event.time = 1.0;
        ensure("click",tree.routePointer(*editor,event,error));
        ensure_equals("capture",tree.mouseCapture(),*editor);
        ensure_equals("focus",tree.keyboardFocus(),*editor);
        ensure_equals("cursor at start",tree.get(*editor)->lineEditor->text.cursor(),std::size_t(0));
        event.kind = LLVKWidgetTree::PointerKind::Hover;
        event.x = 210; event.time = 1.1;
        ensure("drag",tree.routePointer(*editor,event,error));
        ensure_equals("drag selection",tree.get(*editor)->lineEditor->text.selectionEnd(),std::size_t(7));
        ensure_equals("I-beam requested",beams,1);
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        event.x = 10; event.time = 1.2;
        ensure("release",tree.routePointer(*editor,event,error));
        ensure_equals("release does not reposition ended selection",tree.get(*editor)->lineEditor->text.selectionEnd(),std::size_t(7));
        ensure("capture callback ended selection before up",endedBeforeUp);
        ensure_equals("capture released",tree.mouseCapture(),LLVKWidgetTree::Id(0));
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.x = 10; event.time = 2.0;
        ensure("word click",tree.routePointer(*editor,event,error));
        event.kind = LLVKWidgetTree::PointerKind::DoubleClick;
        event.time = 2.1;
        ensure("double click",tree.routePointer(*editor,event,error));
        ensure_equals("word selection start",tree.get(*editor)->lineEditor->text.selectionStart(),std::size_t(0));
        ensure_equals("word selection end",tree.get(*editor)->lineEditor->text.selectionEnd(),std::size_t(3));
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        event.time = 2.2;
        ensure("triple click",tree.routePointer(*editor,event,error));
        ensure_equals("triple selects all",tree.get(*editor)->lineEditor->text.selectionStart(),std::size_t(7));
        ensure("triple selection complete",!tree.get(*editor)->lineEditor->text.selecting());
    }

    template<> template<> void object::test<78>()
    {
        set_test_name("native editor clipboard commands enforce password ownership and rollback contracts");
        struct Clipboard final : LLVKClipboard
        {
            std::u32string text;
            int writes = 0;
            std::function<void()> onWrite;
            bool available(bool primary) const override { return !primary; }
            std::optional<std::u32string> read(bool primary, std::string& error) override
            { error.clear(); return primary ? std::nullopt : std::optional(text); }
            bool write(std::u32string_view value, bool primary, std::string& error) override
            {
                error.clear();
                if (primary) return false;
                ++writes;
                text = value;
                if (onWrite) onWrite();
                return true;
            }
        };
        LLVKWidgetTree tree;
        auto clipboard = std::make_shared<Clipboard>();
        tree.setClipboard(clipboard);
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "secret";
        params.text.selectOnFocus = true;
        params.text.password = true;
        std::string error;
        auto password = tree.createLineEditor(view,control,params,0,error);
        ensure(error,password.has_value());
        ensure("password focus and selection",tree.requestControlFocus(*password,true,error));
        ensure("password not copyable",!tree.canLineEditorCopy(*password));
        ensure("password not cuttable",!tree.canLineEditorCut(*password));
        ensure("password copy does not write",!tree.copyLineEditor(*password,false,error));
        ensure_equals("no password leaked",clipboard->writes,0);
        clipboard->text = U"replacement";
        ensure("password paste allowed",tree.pasteLineEditor(*password,false,error));
        ensure_equals("password replacement",tree.value(*password).asString(),std::string("replacement"));
        params.text.password = false;
        params.prevalidator = [](auto text) { return !text.empty(); };
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        ensure("editor focus and selection",tree.requestControlFocus(*editor,true,error));
        ensure("cut",tree.cutLineEditor(*editor,error));
        ensure("clipboard gets selection before rejected deletion",clipboard->text == U"secret");
        ensure_equals("cut rollback restores text",tree.value(*editor).asString(),std::string("secret"));
        ensure("cut rollback restores selection",tree.canLineEditorCopy(*editor));
        tree.setEnabled(*editor,false);
        ensure("readonly copy allowed",tree.copyLineEditor(*editor,false,error));
        ensure("readonly cut unavailable",!tree.canLineEditorCut(*editor));
        ensure("readonly paste unavailable",!tree.canLineEditorPaste(*editor,false));
        tree.setEnabled(*editor,true);
        ensure("primary unavailable",!tree.canLineEditorPaste(*editor,true));
        clipboard->onWrite = [&] { std::string failure; ensure("erase during clipboard callback",tree.erase(*editor,failure)); tree.setClipboard({}); };
        ensure("retained clipboard supports reentrant deletion",tree.cutLineEditor(*editor,error));
        ensure("editor deleted",tree.get(*editor) == nullptr);
    }

        template<> template<> void object::test<77>()
        {
        set_test_name("native clipboard encoding owns Unicode and Windows newline contracts");
        std::string error;
        const std::u32string input = U"A\r\nB\n\U0001f600\r";
        auto encoded = LLVKClipboard::encodeWindows(input,error);
        ensure(error,encoded.has_value());
        auto decoded = LLVKClipboard::decodeWindows(*encoded,error);
        ensure(error,decoded.has_value());
        ensure("copy/paste newline round trip",*decoded == input);
        ensure("copy inserts CR even before existing CRLF",encoded->substr(0,4) == L"A\r\r\n");
        ensure("invalid scalar write rejected",!LLVKClipboard::encodeWindows(std::u32string(1,0xd800),error));
        ensure("embedded NUL write rejected",!LLVKClipboard::encodeWindows(std::u32string(1,0),error));
        ensure("oversized write rejected",!LLVKClipboard::encodeWindows(std::u32string(1024*1024+1,U'a'),error));
        ensure("no window cannot create transport",!LLVKClipboard::forWindow(nullptr,error));
    #if defined(_WIN32)
        ensure("trailing high surrogate rejected",!LLVKClipboard::decodeWindows(std::wstring(1,wchar_t(0xd800)),error));
        ensure("lone low surrogate rejected",!LLVKClipboard::decodeWindows(std::wstring(1,wchar_t(0xdc00)),error));
    #endif
        const auto external = LLVKClipboard::decodeWindows(L"A\r\nB\rC\n",error);
        ensure(error,external.has_value());
        ensure("only paired CR removed",*external == U"A\nB\rC\n");
        }

    template<> template<> void object::test<76>()
    {
        set_test_name("native paste preserves validation order limits and primary selection behavior");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "old";
        params.text.maximumBytes = 7;
        std::vector<std::u32string> validated;
        params.inputPrevalidator = [&](auto text) { validated.emplace_back(text); return true; };
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        tree.setKeyboardFocus(*editor,false,false,error);
        tree.lineEditorKey(*editor,LLVKLineEditor::Key::Home,{true,false,false},error);
        validated.clear();
        int limited = 0;
        LLVKWidgetTree::Events events;
        events.badKeystroke = [&](auto) { ++limited; };
        tree.setEvents(*editor,events);
        ensure("paste",tree.pasteLineEditorText(*editor,U"A\t\n\U0001f600Z",false,error));
        ensure("validate before cleanup",validated.front() == U"A\t\n\U0001f600Z");
        ensure("selection checked separately",validated.back() == U"old");
        ensure("byte-safe prefix",tree.get(*editor)->lineEditor->text.display() == U"A  \U0001f600");
        ensure_equals("byte-limit effect",limited,1);
        ensure("selection gone",tree.get(*editor)->lineEditor->text.selectionStart() == tree.get(*editor)->lineEditor->text.selectionEnd());
        params.text.maximumBytes = 100;
        params.text.maximumCharacters = 12;
        params.text.replaceNewlinesWithSpaces = false;
        auto primary = tree.createLineEditor(view,control,params,0,error);
        ensure(error,primary.has_value());
        tree.setEvents(*primary,events);
        tree.setKeyboardFocus(*primary,false,false,error);
        tree.lineEditorKey(*primary,LLVKLineEditor::Key::Home,{true,false,false},error);
        const auto before = limited;
        ensure("primary paste",tree.pasteLineEditorText(*primary,U"\n",true,error));
        ensure("primary preserves selected text",tree.get(*primary)->lineEditor->text.display() == U"\u00b6old");
        ensure_equals("configured character cap always reports",limited,before+1);
        params.text.maximumCharacters = 0;
        params.prevalidator = [](auto) { return false; };
        auto rejecting = tree.createLineEditor(view,control,params,0,error);
        ensure(error,rejecting.has_value());
        ensure("invalid whole paste handled",tree.pasteLineEditorText(*rejecting,U"X",false,error));
        ensure_equals("rejected paste rollback",tree.value(*rejecting).asString(),std::string("old"));
        ensure("rejected paste resets baseline",!tree.dirty(*rejecting));
        ensure("invalid scalar rejected",!tree.pasteLineEditorText(*rejecting,std::u32string(1,0xd800),false,error));
        ensure_equals("invalid input atomic",tree.value(*rejecting).asString(),std::string("old"));
    }

    template<> template<> void object::test<75>()
    {
        set_test_name("native Delete command keeps validation multiplicity and propagation policy");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "abc";
        int inputCalls = 0, strokes = 0;
        bool denyFirst = false, denySecond = false;
        params.inputPrevalidator = [&](auto) { ++inputCalls; return !denyFirst && !(denySecond && inputCalls%2 == 0); };
        params.keystroke.function = [&](auto,const LLSD&) { ++strokes; };
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        tree.setKeyboardFocus(*editor,false,false,error);
        tree.lineEditorKey(*editor,LLVKLineEditor::Key::Home,{},error);
        ensure("forward delete",tree.deleteLineEditor(*editor,error));
        ensure_equals("single character validates twice",inputCalls,2);
        ensure_equals("first removed",tree.value(*editor).asString(),std::string("bc"));
        denySecond = true;
        ensure("second predicate may refuse",tree.deleteLineEditor(*editor,error));
        ensure_equals("refusal preserves text",tree.value(*editor).asString(),std::string("bc"));
        ensure_equals("second refusal retains advanced cursor",tree.get(*editor)->lineEditor->text.cursor(),std::size_t(1));
        denyFirst = true;
        const auto before = strokes;
        ensure("first predicate may refuse",tree.deleteLineEditor(*editor,error));
        ensure_equals("first refusal notifies",strokes,before+1);
        ensure_equals("first refusal does not advance",tree.get(*editor)->lineEditor->text.cursor(),std::size_t(1));
        tree.lineEditorKey(*editor,LLVKLineEditor::Key::End,{},error);
        ensure("default consumes delete at end",tree.canLineEditorDelete(*editor));
        params.passDelete = true;
        auto propagating = tree.createLineEditor(view,control,params,0,error);
        ensure(error,propagating.has_value());
        ensure("configured end-of-text delete propagates",!tree.canLineEditorDelete(*propagating));
        tree.setEnabled(*editor,false);
        ensure("readonly delete unavailable",!tree.canLineEditorDelete(*editor));
        ensure("readonly delete untouched",!tree.deleteLineEditor(*editor,error));
    }

    template<> template<> void object::test<74>()
    {
        set_test_name("native history preserves drafts and Return Escape and Insert semantics");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "first";
        params.keystrokeOnEscape = true;
        params.selectOnCommit = false;
        int commits = 0, strokes = 0;
        control.commit.function = [&](auto,const LLSD&) { ++commits; };
        params.keystroke.function = [&](auto,const LLSD&) { ++strokes; };
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        tree.setKeyboardFocus(*editor,false,false,error);
        tree.enableLineHistory(*editor,true);
        using Key = LLVKLineEditor::Key;
        ensure("Return propagates",!tree.lineEditorKey(*editor,Key::Return,{},error));
        ensure_equals("Return records text",tree.get(*editor)->lineEditor->history.front(),std::string("first"));
        ensure_equals("Return does not commit",commits,0);
        ensure("commit",tree.commitLineEditor(*editor));
        ensure_equals("Return then commit deduplicates",tree.get(*editor)->lineEditor->history.size(),std::size_t(2));
        ensure("clear for draft",tree.clearLineEditor(*editor,error));
        ensure("draft input",tree.lineEditorUnicode(*editor,U'd',error));
        ensure("browse up",tree.lineEditorKey(*editor,Key::Up,{},error));
        ensure_equals("history recalled",tree.value(*editor).asString(),std::string("first"));
        ensure("browse down",tree.lineEditorKey(*editor,Key::Down,{},error));
        ensure_equals("draft restored",tree.value(*editor).asString(),std::string("d"));
        const auto beforeEscape = strokes;
        ensure("Escape propagates",!tree.lineEditorKey(*editor,Key::Escape,{},error));
        ensure_equals("Escape restores baseline",tree.value(*editor).asString(),std::string("first"));
        ensure_equals("Escape optional notification",strokes,beforeEscape+1);
        ensure("Escape resets dirty",!tree.dirty(*editor));
        ensure("home",tree.lineEditorKey(*editor,Key::Home,{},error));
        ensure("insert",tree.lineEditorKey(*editor,Key::Insert,{},error));
        ensure("native mode toggled",tree.overwriteMode());
        ensure("overwrite uses native mode",tree.lineEditorUnicode(*editor,U'X',error));
        ensure_equals("replaced character",tree.value(*editor).asString(),std::string("Xirst"));
        ensure("modified Insert consumed",tree.lineEditorKey(*editor,Key::Insert,{true,false,false},error));
        ensure("modified Insert does not toggle",tree.overwriteMode());
        auto other = tree.createLineEditor(view,control,params,0,error);
        ensure(error,other.has_value());
        tree.setKeyboardFocus(*other,false,false,error);
        ensure("insert mode shared by native tree",tree.overwriteMode());
        ensure("toggle in second editor",tree.lineEditorKey(*other,Key::Insert,{},error));
        ensure("native mode insert",!tree.overwriteMode());
    }

    template<> template<> void object::test<73>()
    {
        set_test_name("native line editor keyboard navigation deletion and validation rollback");
        LLVKWidgetTree tree;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,23};
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "one two!";
        int strokes = 0, rejected = 0;
        params.keystroke.function = [&](auto,const LLSD&) { ++strokes; };
        LLVKWidgetTree::Events events;
        events.badKeystroke = [&](auto) { ++rejected; };
        std::string error;
        auto editor = tree.createLineEditor(view,control,params,0,error);
        ensure(error,editor.has_value());
        ensure("editor events",tree.setEvents(*editor,events));
        ensure("focus",tree.setKeyboardFocus(*editor,false,false,error));
        using Key = LLVKLineEditor::Key;
        const LLVKLineEditor::Modifiers shift{true,false,false}, ctrl{false,true,false};
        ensure("left selects",tree.lineEditorKey(*editor,Key::Left,shift,error));
        ensure_equals("selection begins at end",tree.get(*editor)->lineEditor->text.selectionStart(),std::size_t(8));
        ensure_equals("selection moves one",tree.get(*editor)->lineEditor->text.selectionEnd(),std::size_t(7));
        ensure("backspace selection",tree.lineEditorKey(*editor,Key::Backspace,{},error));
        ensure_equals("removed punctuation",tree.value(*editor).asString(),std::string("one two"));
        ensure("word backspace",tree.lineEditorKey(*editor,Key::Backspace,ctrl,error));
        ensure_equals("word removed",tree.value(*editor).asString(),std::string("one "));
        ensure("home",tree.lineEditorKey(*editor,Key::Home,{},error));
        ensure("boundary backspace consumed",tree.lineEditorKey(*editor,Key::Backspace,{},error));
        ensure_equals("boundary effect",rejected,1);
        ensure("delete left for external edit routing",!tree.lineEditorKey(*editor,Key::Delete,{},error));
        ensure("word delete",tree.lineEditorKey(*editor,Key::Delete,ctrl,error));
        ensure_equals("word and trailing spaces removed",tree.value(*editor).asString(),std::string());
        ensure_equals("handled keys notify",strokes,6);
        params.text.defaultText = "12";
        params.prevalidator = [](auto text) { return text.size() == 2; };
        auto validated = tree.createLineEditor(view,control,params,0,error);
        ensure(error,validated.has_value());
        ensure("focus validated",tree.setKeyboardFocus(*validated,false,false,error));
        ensure("invalid deletion handled",tree.lineEditorKey(*validated,Key::Backspace,{},error));
        ensure_equals("rollback text",tree.value(*validated).asString(),std::string("12"));
        ensure_equals("rollback cursor",tree.get(*validated)->lineEditor->text.cursor(),std::size_t(2));
        ensure("rollback resets baseline",!tree.dirty(*validated));
        tree.setEnabled(*validated,false);
        ensure("readonly plain navigation unhandled",!tree.lineEditorKey(*validated,Key::Home,{},error));
        ensure("readonly shift consumed then rolled back",tree.lineEditorKey(*validated,Key::Home,shift,error));
        ensure_equals("readonly cursor restored",tree.get(*validated)->lineEditor->text.cursor(),std::size_t(2));
    }

    template<> template<> void object::test<72>()
    {
        set_test_name("native Windows numeric validators retain edit-state and locale contracts");
#if defined(_WIN32)
        LLVKWidgetTree tree;
        LLVKWidgetFactory::LineEditorDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, {}, {}, defaults);
        struct Case { const char* name; std::u32string text; bool accepted; };
        const Case cases[]{
            {"float",U"",true},{"float",U"-",true},{"float",U" -..1.2 \t",true},
            {"float",U"1,2",false},{"float",U"+1",false},{"float",U"1e2",false},{"float",U"1-2",false},
            {"int",U"",true},{"int",U"-",true},{"int",U" \t-12\n",true},{"int",U"1 2",false},
            {"int",U"1.2",false},{"int",U"+12",false},
            {"positive_s32",U"",false},{"positive_s32",U"0",false},{"positive_s32",U"01",false},
            {"positive_s32",U"-1",false},{"positive_s32",U" 1 ",true},
            {"positive_s32",U"999999999999999999999999",true},
            {"non_negative_s32",U"",true},{"non_negative_s32",U" \t",true},
            {"non_negative_s32",U"0001",true},{"non_negative_s32",U"-0",false},
            {"non_negative_s32",U"999999999999999999999999",true},
            {"alpha_num",U"",true},{"alpha_num",U"aZ09\u00e9",true},
            {"alpha_num",U"a b",false},{"alpha_num",U"a_b",false},
            {"alpha_num_space",U"a b9",true},{"alpha_num_space",U"a\tb",false}
        };
        std::string error;
        for (const auto& entry : cases)
        {
            auto editor = factory.construct(tree,std::string("<line_editor prevalidator='") + entry.name + "'/>",0,error);
            ensure(error,editor.has_value());
            const auto validator = tree.get(*editor)->lineEditor->params.prevalidator;
            ensure_equals(std::string(entry.name) + " predicate",validator(entry.text),entry.accepted);
            ensure("invalid scalar rejects without narrowing",!validator(std::u32string(1,0x110031)));
            ensure("remove editor",tree.erase(*editor,error));
            ensure("predicate owns locale beyond widget lifetime",validator(entry.text) == entry.accepted);
        }
#endif
    }

    template<> template<> void object::test<71>()
    {
        set_test_name("native built-in ASCII validator declarations preserve exact character sets");
        LLVKWidgetTree tree;
        LLVKWidgetFactory::Resources resources;
        LLVKWidgetFactory::LineEditorDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources, {}, defaults);
        std::string error;
        for (const std::string name : {"ascii","ascii_with_newline","ascii_printable_no_pipe","ascii_printable_no_space"})
        {
            auto editor = factory.construct(tree,"<line_editor width='100' height='23' prevalidator='" + name +
                "' input_prevalidator='" + name + "'/>",0,error);
            ensure(error,editor.has_value());
            const auto& params = tree.get(*editor)->lineEditor->params;
            ensure("empty field accepted",params.prevalidator(U""));
            for (char32_t character = 0; character < 256; ++character)
            {
                bool accepted = character >= 0x20 && character <= 0x7f;
                if (name == "ascii_with_newline") accepted = accepted || character == U'\n';
                if (name == "ascii_printable_no_pipe") accepted = accepted && character != U'|' && character != 0x7f;
                if (name == "ascii_printable_no_space") accepted = accepted && character != U' ' && character != 0x7f;
                const std::u32string text(1,character);
                ensure_equals(name + " full-text character " + std::to_string(character),params.prevalidator(text),accepted);
                ensure_equals(name + " input character " + std::to_string(character),params.inputPrevalidator(text),accepted);
            }
            ensure("non-ASCII rejected",!params.prevalidator(U"\u03a9"));
            ensure("invalid scalar rejected",!params.prevalidator(std::u32string(1,0x110000)));
        }
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.textValidators["ascii"] = [](auto text) { return text == U"override"; };
        LLVKWidgetFactory scoped({}, {}, {}, callbacks, resources, {}, defaults);
        auto editor = scoped.construct(tree,"<line_editor prevalidator='ascii'/>",0,error);
        ensure(error,editor.has_value());
        ensure("native explicit registration has precedence",tree.get(*editor)->lineEditor->params.prevalidator(U"override"));
        ensure("built-in does not mask explicit registration",!tree.get(*editor)->lineEditor->params.prevalidator(U"other"));
    }

    template<> template<> void object::test<70>()
    {
        set_test_name("native line editor XML constructs usable text state from packaged defaults");
        LLVKWidgetTree tree;
        std::string error;
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        LLVKWidgetFactory::Resources resources;
        resources.skinFiles = std::make_shared<LLVKSkinFiles>(configuration);
        resources.fonts["EmojiSmall"] = loadFont();
        resources.colors = std::make_shared<LLVKColorTable>();
        const auto colors = resources.skinFiles->read("","colors.xml",LLVKSkinFiles::Policy::All,error);
        ensure(error,colors.has_value());
        std::vector<std::string> warnings;
        for (const auto& file : *colors) ensure("colors",resources.colors->load(file,LLVKColorTable::Layer::Loaded,warnings,error));
        auto images = std::make_shared<LLVKSkinImages>(resources.skinFiles);
        ensure("image declarations",images->loadDeclarations(error));
        tree.setSkinImages(images);
        LLVKWidgetFactory::Callbacks callbacks;
        std::string typed;
        callbacks.actions["changed"] = [&](auto,const LLSD& value) { typed = value.asString(); };
        callbacks.textValidators["digits"] = [](auto text)
        { return std::all_of(text.begin(),text.end(),[](char32_t character) { return character >= U'0' && character <= U'9'; }); };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources);
        const bool border = factory.loadDefaultsFile(tree,"widgets/view_border.xml",error);
        ensure(error,border);
        const bool defaults = factory.loadDefaultsFile(tree,"widgets/line_editor.xml",error);
        ensure(error,defaults);
        auto editor = factory.construct(tree,
            "<line_editor width='100' height='23' prevalidator='digits' max_length='3' value='12'>"
            "<line_editor.keystroke_callback function='changed'/></line_editor>",0,error);
        ensure(error,editor.has_value());
        ensure("real native editor component",tree.get(*editor)->lineEditor.has_value());
        ensure("real background image",tree.get(*editor)->lineEditor->params.background != nullptr);
        ensure("native font from template",tree.get(*editor)->control->params.font == resources.fonts.at("EmojiSmall"));
        ensure_equals("source byte default retained by char limit choice",tree.get(*editor)->lineEditor->params.text.maximumBytes,std::size_t(4096));
        tree.setKeyboardFocus(*editor,false,false,error);
        tree.lineEditorUnicode(*editor,U'3',false,error);
        ensure_equals("typed callback from declaration",typed,std::string("123"));
        tree.lineEditorUnicode(*editor,U'x',false,error);
        ensure_equals("native validator prevents invalid value",tree.value(*editor).asString(),std::string("123"));
        const auto size = tree.size();
        ensure("unknown validator explicit",!factory.construct(tree,"<line_editor prevalidator='unknown'/>",0,error));
        ensure_equals("unknown validator creates no partial widget",tree.size(),size);
        auto readonly = factory.construct(tree,"<line_editor enabled='false' is_password='true'/>",0,error);
        ensure(error,readonly.has_value());
        ensure("readonly password constructed",tree.get(*readonly)->lineEditor->readOnly && tree.get(*readonly)->lineEditor->params.text.password);
        resources.fonts["SansSerifSmall"] = resources.fonts.at("EmojiSmall");
        resources.fonts["SansSerif"] = resources.fonts.at("EmojiSmall");
        LLVKWidgetFactory checkboxFactory({}, {}, {}, {}, resources);
        ensure("checkbox template",checkboxFactory.loadDefaultsFile(tree,"widgets/check_box.xml",error));
        auto checkbox = checkboxFactory.construct(tree,"<check_box width='150' height='23' label='Remember' initial_value='true'/>",0,error);
        ensure(error,checkbox.has_value());
        const auto children = *tree.get(*checkbox)->checkBox;
        ensure("actual native label",tree.get(children.label)->plainText.has_value());
        ensure("actual native toggle",tree.get(children.button)->button.has_value());
        ensure("real checkbox image",tree.get(children.button)->button->params.images.selected != nullptr);
        ensure("initial checkbox value",tree.value(*checkbox).asBoolean());
        ensure_equals("checkbox label from declaration",tree.value(children.label).asString(),std::string("Remember"));
        resources.fallbackFont = resources.fonts.at("SansSerifSmall");
        int scrollPosition = -1;
        callbacks.actions["scrolled"] = [&](auto,const LLSD& value) { scrollPosition = value.asInteger(); };
        LLVKWidgetFactory scrollFactory({}, {}, {}, callbacks, resources);
        ensure("scrollbar packaged template",scrollFactory.loadDefaultsFile(tree,"widgets/scroll_bar.xml",error));
        for (const std::string orientation : {"vertical","horizontal"})
        {
            const auto scrollbar = scrollFactory.construct(tree,
                "<scroll_bar width='100' height='100' orientation='" + orientation + "' doc_size='1000' page_size='100'>"
                "<scroll_bar.change_callback function='scrolled'/></scroll_bar>",0,error);
            ensure(error,scrollbar.has_value());
            const auto& state = *tree.get(*scrollbar)->scrollbar;
            ensure_equals("packaged scrollbar thickness",state.thickness,15);
            const auto& images = tree.get(state.decrease)->button->images;
            ensure("orientation selects actual arrow asset",images.unselected && images.unselected->name() ==
                (orientation == "vertical" ? "ScrollArrow_Up" : "ScrollArrow_Left"));
            ensure("real native track and thumb",state.params->trackVertical && state.params->trackHorizontal &&
                state.params->thumbVertical && state.params->thumbHorizontal);
            ensure("declared scroll update",tree.setScrollPosition(*scrollbar,12,true,error));
            ensure_equals("declared native callback",scrollPosition,12);
        }
        ensure("container packaged template",scrollFactory.loadDefaultsFile(tree,"widgets/scroll_container.xml",error));
        ensure("base text editor template",scrollFactory.loadDefaultsFile(tree,"widgets/simple_text_editor.xml",error));
        ensure("search editor template",scrollFactory.loadDefaultsFile(tree,"widgets/search_editor.xml",error));
        ensure("color swatch template",scrollFactory.loadDefaultsFile(tree,"widgets/color_swatch.xml",error));
        const auto colorSwatch=scrollFactory.construct(tree,
            "<color_swatch name='background_color' width='80' height='60' label='Color' color='1 0 0 0.5' can_apply_immediately='true'/>",0,error);
        ensure(error,colorSwatch.has_value());
        ensure("original swatch constructor",tree.get(*colorSwatch)->colorSwatch.has_value());
        ensure("source alpha image",tree.get(*colorSwatch)->colorSwatch->params->alphaBackground!=nullptr);
        ensure_equals("source swatch caption",tree.value(tree.get(*colorSwatch)->colorSwatch->caption).asString(),std::string("Color"));
        ensure("original swatch painter",LLVKWidgetPaint::prepare(tree,*colorSwatch,{},error).has_value());
        const auto searchEditor=scrollFactory.construct(tree,
            "<search_editor name='search_prefs_edit' width='220' height='18' clear_button_visible='true' max_length_bytes='255'>"
            "<search_editor.clear_button rect.height='18' rect.width='18' rect.bottom='-1'/>"
            "<search_editor.search_button rect.height='12' rect.width='12' rect.bottom='-1'/></search_editor>",0,error);
        ensure(error,searchEditor.has_value());
        ensure("search constructor is composite",tree.get(*searchEditor)->searchEditor.has_value());
        const auto searchState=*tree.get(*searchEditor)->searchEditor;
        ensure_equals("source search text padding",tree.get(searchState.editor)->lineEditor->params.text.leftPadding,18);
        ensure_equals("source clear height override",tree.get(searchState.clear)->params.rect.top-tree.get(searchState.clear)->params.rect.bottom,18);
        ensure("search query update",tree.setValue(*searchEditor,LLSD("cache")));
        const auto searchPaint=LLVKWidgetPaint::prepare(tree,*searchEditor,{},error);
        ensure(error,searchPaint.has_value());
        ensure("visible native clear image",std::any_of(searchPaint->commands.begin(),searchPaint->commands.end(),
            [&](const auto& command) { return command.owner==searchState.clear && command.image; }));
        const auto clearRect=tree.screenRect(searchState.clear,error);
        ensure(error,clearRect.has_value());
        LLVKWidgetTree::PointerEvent clearClick;
        clearClick.kind=LLVKWidgetTree::PointerKind::LeftDown;
        clearClick.x=(clearRect->left+clearRect->right)/2; clearClick.y=(clearRect->bottom+clearRect->top)/2;
        ensure("pointer reaches nested clear",tree.routePointer(*searchEditor,clearClick,error));
        clearClick.kind=LLVKWidgetTree::PointerKind::LeftUp;
        ensure("pointer releases nested clear",tree.routePointer(*searchEditor,clearClick,error));
        ensure_equals("pointer clears original search input",tree.value(*searchEditor).asString(),std::string());
        ensure("text editor overlay template",scrollFactory.loadDefaultsFile(tree,"widgets/text_editor.xml",error));
        const auto aboutEditor=scrollFactory.construct(tree,
            "<text_editor name='support_editor' width='300' height='90' enabled='false' word_wrap='true' max_length='65536'>Original editor contents</text_editor>",0,error);
        ensure(error,aboutEditor.has_value());
        ensure("editor constructor is composite",tree.get(*aboutEditor)->textEditor.has_value());
        ensure_equals("original editor body",tree.value(*aboutEditor).asString(),std::string("Original editor contents"));
        ensure_equals("source editor padding",tree.get(tree.get(*aboutEditor)->textEditor->body)->plainText->params.layout.horizontalPadding,6);
        ensure("panel construction font default",scrollFactory.loadDefaults(tree,"<panel font='SansSerifSmall'/>",error));
        ensure("spinner editor defaults",scrollFactory.loadDefaultsFile(tree,"widgets/line_editor.xml",error));
        ensure("packaged spinner template",scrollFactory.loadDefaultsFile(tree,"widgets/spinner.xml",error));
        const auto spinner=scrollFactory.construct(tree,
            "<spinner name='RenderNameShowTime' width='40' height='20' min_val='1' max_val='60' increment='1' decimal_digits='0' initial_value='5'/>",0,error);
        ensure(error,spinner.has_value());
        const auto spinState=*tree.get(*spinner)->spinner;
        ensure_equals("spinner skin arrow",tree.get(spinState.up)->button->images.unselected->name(),std::string("Stepper_Up_Off"));
        ensure_equals("spinner precision from original declaration",tree.value(spinState.editor).asString(),std::string("5"));
        ensure("spinner from XUI commits",tree.stepSpinner(*spinner,true,{},error));
        ensure_equals("spinner native result",tree.value(*spinner).asReal(),6.);
        ensure("slider bar template",scrollFactory.loadDefaultsFile(tree,"widgets/slider_bar.xml",error));
        const auto slider=scrollFactory.construct(tree,
            "<slider_bar name='gain' width='140' height='24' min_val='0' max_val='1' increment='0.1' initial_value='0.5'/>",0,error);
        ensure(error,slider.has_value());
        const auto sliderPaint=LLVKWidgetPaint::prepare(tree,*slider,{},error);
        ensure(error,sliderPaint.has_value());
        ensure_equals("slider thumb real skin image",tree.get(*slider)->slider->params->thumb->name(),std::string("SliderThumb_Off"));
        ensure("slider track and thumb emitted",sliderPaint->commands.size()>=3);
        ensure("composite slider template",scrollFactory.loadDefaultsFile(tree,"widgets/slider.xml",error));
        const auto compositeSlider=scrollFactory.construct(tree,
            "<slider name='volume' label='Volume' label_width='60' text_width='40' width='240' height='24' min_val='0' max_val='1' increment='0.1' initial_value='0.5' decimal_digits='1' can_edit_text='true'/>",0,error);
        ensure(error,compositeSlider.has_value());
        const auto compositeState=*tree.get(*compositeSlider)->sliderControl;
        ensure("full slider owns label bar and editor",compositeState.label && compositeState.bar && compositeState.editor);
        ensure_equals("declared slider value",tree.value(compositeState.editor).asString(),std::string("0.5"));
        ensure("composite slider painted",LLVKWidgetPaint::prepare(tree,*compositeSlider,{},error).has_value());
        ensure("radio checkbox defaults",scrollFactory.loadDefaultsFile(tree,"widgets/check_box.xml",error));
        ensure("radio item template",scrollFactory.loadDefaultsFile(tree,"widgets/radio_item.xml",error));
        ensure("radio group template",scrollFactory.loadDefaultsFile(tree,"widgets/radio_group.xml",error));
        const auto radio=scrollFactory.construct(tree,
            "<radio_group width='200' height='80' layout='topleft' initial_value='second'>"
            "<radio_item name='first' value='first' label='First' left='0' top='0' width='150' height='20'/>"
            "<radio_item name='second' value='second' label='Second' left='0' top_pad='4' width='150' height='20'/></radio_group>",0,error);
        ensure(error,radio.has_value());
        ensure_equals("radio payload initial selection",tree.value(*radio).asString(),std::string("second"));
        const auto radioFirst=tree.get(*radio)->radioGroup->items.front().control;
        ensure_equals("source radio image",tree.get(tree.get(radioFirst)->checkBox->button)->button->images.unselected->name(),std::string("RadioButton_Off"));
        ensure_equals("source radio XUI top",tree.get(radioFirst)->params.rect.top,80);
        auto preferenceCallbacks=callbacks;
        preferenceCallbacks.actions["Pref.MaturitySettings"]=[](auto,const LLSD&) {};
        preferenceCallbacks.actions["Pref.getUIColor"]=[&](auto id,const LLSD& parameter)
        {
            const auto color=resources.colors->find(parameter.asString());
            ensure("original preference color exists",color.has_value());
            LLSD value=LLSD::emptyArray();
            for (const auto channel : color->get()) value.append(channel);
            ensure("original color initialization",tree.setColorSwatchValue(id,value,error));
        };
        preferenceCallbacks.actions["Pref.applyUIColor"]=[&](auto id,const LLSD& parameter)
        { ensure("native user color commit",resources.colors->set(parameter.asString(),tree.get(id)->colorSwatch->color)); };
        auto preferenceResources=resources;
        std::string preferenceLink;
        preferenceResources.webLinkHandler=[&](auto,const std::string& target) { preferenceLink=target; };
        LLVKWidgetFactory generalFactory({}, {}, {}, preferenceCallbacks,preferenceResources);
        ensure("General panel constructor font",generalFactory.loadDefaults(tree,"<panel font='SansSerifSmall'/>",error));
        for (const std::string widget : {"button","view_border","line_editor","check_box","radio_item","radio_group","spinner","combo_box","text","color_swatch","slider_bar","slider"})
            ensure("General widget template "+widget,generalFactory.loadDefaultsFile(tree,"widgets/"+widget+".xml",error));
            for (const std::string widget : {"scroll_bar","scroll_container","tab_container","simple_text_editor","text_editor"})
                ensure("About widget template "+widget,generalFactory.loadDefaultsFile(tree,"widgets/"+widget+".xml",error));
            const auto about=generalFactory.constructFile(tree,"floater_about.xml",0,error);
            ensure(error,about.has_value());
            ensure("original About root is native floater",tree.get(*about)->floater.has_value());
            const auto colorPreferences=generalFactory.constructFile(tree,"panel_preferences_colors.xml",0,error);
            ensure(error,colorPreferences.has_value());
            const auto colorPreferencePaint=LLVKWidgetPaint::prepare(tree,*colorPreferences,{},error);
            ensure(error,colorPreferencePaint.has_value());
            std::map<std::string,LLVKWidgetTree::Id> colorFields;
            std::vector<LLVKWidgetTree::Id> colorChildren{*colorPreferences};
            for (std::size_t index=0; index<colorChildren.size(); ++index)
            {
                const auto* node=tree.get(colorChildren[index]);
                colorFields.try_emplace(node->params.name,colorChildren[index]);
                colorChildren.insert(colorChildren.end(),node->children.begin(),node->children.end());
            }
            const auto dialogSwatch=colorFields.at("ScriptDialogFb");
            ensure("original init replaces missing named-color fallback",tree.get(dialogSwatch)->colorSwatch->color==resources.colors->find("ScriptDialogFg")->get());
            const auto colorsTabs=colorFields.at("tabs");
            const auto colorTabs=tree.get(colorsTabs)->tabContainer->tabs;
            for (const auto& tab : colorTabs)
            {
                ensure("select original Colors subpanel",tree.selectTabPanel(colorsTabs,tab.panel,error));
                const auto paint=LLVKWidgetPaint::prepare(tree,*colorPreferences,{},error);
                ensure(error,paint.has_value());
            }
            const auto colorsSnapshot=tree.snapshotPreferences(*colorPreferences,error);
            ensure("source map alpha post-build binding",tree.bindPreferenceColorAlpha(*colorPreferences,resources.colors,error));
            const auto alphaSnapshot=tree.snapshotPreferences(*colorPreferences,error);
            ensure(error,alphaSnapshot.has_value());
            const auto alphaSlider=colorFields.at("MapPickRadiusTransparency");
            const auto radiusSwatch=colorFields.at("MapPickRadiusColor");
            const auto originalRadius=resources.colors->find("MapPickRadiusColor")->get();
            ensure("edit original map alpha",tree.setValue(alphaSlider,LLSD(0.25)));
            ensure("commit original map alpha",tree.commit(alphaSlider));
            ensure_equals("map alpha updates native color table",resources.colors->find("MapPickRadiusColor")->get()[3],0.25f);
            ensure_equals("map alpha updates native swatch",tree.get(radiusSwatch)->colorSwatch->color[3],0.25f);
            ensure("Cancel restores local alpha",tree.restorePreferences(*alphaSnapshot,{},error));
            ensure_equals("alpha slider restored",tree.value(alphaSlider).asReal(),alphaSnapshot->localValues.at(alphaSlider).asReal());
            ensure("map color restored",resources.colors->find("MapPickRadiusColor")->get()==originalRadius);
            ensure(error,colorsSnapshot.has_value());
            const auto userSwatch=colorFields.at("user");
            const auto originalUserColor=resources.colors->find("UserChatColor")->get();
            ensure("edit original user-color swatch",tree.beginColorSelection(userSwatch,error));
            ensure("preview original user-color callback",tree.applyColorSelection(userSwatch,{0.15f,0.25f,0.35f,1.f},LLVKWidgetTree::ColorPickOperation::Change,error));
            ensure("original color callback reaches native table",resources.colors->find("UserChatColor")->get()==LLVKColor::Value{0.15f,0.25f,0.35f,originalUserColor[3]});
            ensure("original Colors Cancel",tree.restorePreferences(*colorsSnapshot,{},error));
            ensure("original color table restored",resources.colors->find("UserChatColor")->get()==originalUserColor);
            const auto picker=generalFactory.constructFile(tree,"floater_color_picker.xml",0,error);
            ensure(error,picker.has_value());
            ensure("original picker is a native floater",tree.get(*picker)->floater.has_value());
            ensure("native picker owns original fields",tree.initializeColorPicker(*picker,*colorSwatch,error));
            ensure("original palette colors",tree.setColorPickerPalette(*picker,resources.colors,error));
            const auto hueImage=tree.get(*picker)->colorPicker->hueImage;
            ensure("native picker owns generated color plane",hueImage && hueImage->pixelWidth()==256 && hueImage->pixelHeight()==256);
            const auto huePixels=hueImage->bottomUpRgba();
            ensure("source low-saturation edge is grey",huePixels[0]==127 && huePixels[1]==127 && huePixels[2]==127 && huePixels[3]==255);
            const auto redOffset=4*255*256;
            ensure("source top-left hue is red",huePixels[redOffset]==255 && huePixels[redOffset+1]==0 && huePixels[redOffset+2]==0);
            const auto pickerPaint=LLVKWidgetPaint::prepare(tree,*picker,{},error);
            ensure(error,pickerPaint.has_value());
            const auto paletteOrigin=tree.screenRect(*picker,error);
            ensure(error,paletteOrigin.has_value());
            ensure("first palette cell retains source bounds",std::any_of(pickerPaint->commands.begin(),pickerPaint->commands.end(),
                [&](const auto& command) { return command.owner==*picker && command.rectangle==LLVKWidgetTree::Rect{
                    paletteOrigin->left+13,paletteOrigin->bottom+74,paletteOrigin->left+35,paletteOrigin->bottom+90}; }));
            ensure("color plane occupies original region",std::any_of(pickerPaint->commands.begin(),pickerPaint->commands.end(),
                [&](const auto& command) { return command.owner==*picker && command.image==hueImage && command.rectangle.right-command.rectangle.left==256; }));
            const auto pickerFields=tree.get(*picker)->colorPicker->fields;
            ensure_equals("original picker RGB",tree.value(pickerFields.at("rspin")).asReal(),255.);
            ensure_equals("original picker hex",tree.value(pickerFields.at("hex_value")).asString(),std::string("ff0000"));
            ensure("hex entry",tree.setValue(pickerFields.at("hex_value"),LLSD("336699")));
            ensure("hex commit",tree.commit(pickerFields.at("hex_value")));
            ensure_equals("hex synchronizes RGB",tree.value(pickerFields.at("gspin")).asReal(),102.);
            ensure("hue edit",tree.setValue(pickerFields.at("hspin"),LLSD(120.)));
            ensure("hue field commit",tree.commitColorPickerField(*picker,pickerFields.at("hspin"),error));
            ensure_equals("hue state preserved",tree.get(*picker)->colorPicker->hsl[0],120.f/360.f);
            ensure("invalid hex retained without color change",tree.setValue(pickerFields.at("hex_value"),LLSD("oops")));
            const auto beforeInvalid=tree.get(*picker)->colorPicker->rgb;
            ensure("invalid hex ignored",tree.commit(pickerFields.at("hex_value")));
            ensure("invalid hex leaves RGB unchanged",tree.get(*picker)->colorPicker->rgb==beforeInvalid);
            const auto pickerRect=tree.screenRect(*picker,error);
            ensure(error,pickerRect.has_value());
            LLVKWidgetTree::PointerEvent colorPoint;
            colorPoint.kind=LLVKWidgetTree::PointerKind::LeftDown;
            colorPoint.x=pickerRect->left+268; colorPoint.y=pickerRect->bottom+228;
            ensure("hue plane click",tree.routePointer(*picker,colorPoint,error));
            ensure_equals("hue plane owns capture",tree.mouseCapture(),*picker);
            ensure_equals("hue midpoint",tree.get(*picker)->colorPicker->hsl[0],0.5f);
            ensure_equals("saturation midpoint",tree.get(*picker)->colorPicker->hsl[1],0.5f);
            colorPoint.kind=LLVKWidgetTree::PointerKind::Hover;
            colorPoint.x=pickerRect->left+500; colorPoint.y=pickerRect->bottom+500;
            ensure("hue drag clamps",tree.routePointer(*picker,colorPoint,error));
            ensure_equals("hue upper boundary",tree.get(*picker)->colorPicker->hsl[0],1.f);
            ensure_equals("saturation upper boundary",tree.get(*picker)->colorPicker->hsl[1],1.f);
            colorPoint.kind=LLVKWidgetTree::PointerKind::LeftUp;
            ensure("hue release",tree.routePointer(*picker,colorPoint,error));
            ensure_equals("hue capture released",tree.mouseCapture(),LLVKWidgetTree::Id(0));
            colorPoint.kind=LLVKWidgetTree::PointerKind::LeftDown;
            colorPoint.x=pickerRect->left+15; colorPoint.y=pickerRect->bottom+85;
            ensure("original palette selection",tree.routePointer(*picker,colorPoint,error));
            ensure("palette selects first source color",tree.get(*picker)->colorPicker->rgb==tree.get(*picker)->colorPicker->palette[0]);
            const auto originalColor=tree.get(*colorSwatch)->colorSwatch->original;
            ensure("picker Cancel button",tree.commit(pickerFields.at("cancel_btn")));
            ensure("Cancel closes picker",!tree.get(*picker)->params.visible);
            ensure("Cancel restores original swatch",tree.get(*colorSwatch)->colorSwatch->color==originalColor);
            ensure("reopen source transaction",tree.beginColorSelection(*colorSwatch,error));
            tree.setVisible(*picker,true);
            ensure("new picker RGB",tree.setColorPickerRgb(*picker,{0.1f,0.2f,0.3f,1.f},false,error));
            ensure("picker OK button",tree.commit(pickerFields.at("select_btn")));
            ensure("OK closes picker",!tree.get(*picker)->params.visible);
            ensure("OK preserves swatch alpha",tree.get(*colorSwatch)->colorSwatch->color==LLVKColor::Value{0.1f,0.2f,0.3f,0.5f});
            tree.setVisible(*picker,true);
            colorPoint.kind=LLVKWidgetTree::PointerKind::LeftDown;
            colorPoint.x=pickerRect->left+30; colorPoint.y=pickerRect->bottom+160;
            ensure("current-color drag starts",tree.routePointer(*picker,colorPoint,error));
            colorPoint.kind=LLVKWidgetTree::PointerKind::LeftUp;
            colorPoint.x=pickerRect->left+15; colorPoint.y=pickerRect->bottom+85;
            ensure("palette receives current color",tree.routePointer(*picker,colorPoint,error));
            ensure("palette updates native user color",resources.colors->find("ColorPaletteEntry01")->get()==LLVKColor::Value{0.1f,0.2f,0.3f,1.f});
        const auto general=generalFactory.constructFile(tree,"panel_preferences_general.xml",0,error);
        ensure(error,general.has_value());
        ensure_equals("original General panel name",tree.get(*general)->params.name,std::string("general_panel"));
        const auto generalPaint=LLVKWidgetPaint::prepare(tree,*general,{},error);
        ensure(error,generalPaint.has_value());
        ensure("original General controls painted",generalPaint->commands.size()>30);
        ensure("tab container packaged template",scrollFactory.loadDefaultsFile(tree,"widgets/tab_container.xml",error));
        const auto tabs = scrollFactory.construct(tree,
            "<tab_container width='501' height='572'><panel name='info' label='Info'/><panel name='credits' label='Credits'/></tab_container>",0,error);
        ensure(error,tabs.has_value());
        const auto tabState = *tree.get(*tabs)->tabContainer;
        ensure_equals("two declared native tabs",tabState.tabs.size(),std::size_t(2));
        ensure_equals("first declared tab selected",tabState.selected,tabState.tabs.front().panel);
        ensure_equals("source tab skin image",tree.get(tabState.tabs.front().button)->button->params.images.unselected->name(),std::string("TabTop_Left_Off"));
        ensure("declared tab click",tree.commit(tabState.tabs.back().button));
        ensure("second panel visible",tree.get(tabState.tabs.back().panel)->params.visible);
        const auto leftTabs=scrollFactory.construct(tree,
            "<tab_container width='673' height='480' tab_position='left' tab_width='125' tab_padding_right='4'>"
            "<panel name='general' label='General'/><panel name='graphics' label='Graphics'/></tab_container>",0,error);
        ensure(error,leftTabs.has_value());
        const auto leftState=*tree.get(*leftTabs)->tabContainer;
        ensure_equals("source vertical tab image",tree.get(leftState.tabs.front().button)->button->params.images.unselected->name(),std::string("SegmentedBtn_Left_Disabled"));
        ensure_equals("source Preferences panel starts after tabs",tree.get(leftState.tabs.front().panel)->params.rect.left,131);
        const auto container = scrollFactory.construct(tree,
            "<scroll_container width='100' height='100'><panel name='document' width='300' height='400' font='SansSerifSmall'/></scroll_container>",0,error);
        ensure(error,container.has_value());
        const auto containerState = *tree.get(*container)->scrollContainer;
        ensure("factory assigns document",containerState.document != 0 && tree.get(containerState.document)->panel.has_value());
        ensure_equals("template minimum auto rate",containerState.minAutoRate,120.f);
        ensure_equals("template maximum auto rate",containerState.maxAutoRate,500.f);
        ensure_equals("vertical stays in front",tree.get(*container)->children.front(),containerState.vertical);
        ensure("declared container updates",tree.updateScrollContainer(*container,error));
        ensure("declared scrollbars visible",tree.get(containerState.vertical)->params.visible && tree.get(containerState.horizontal)->params.visible);
        ensure("native declared container wheel",tree.routeWheel(*container,30,50,1,false,error));
        ensure_equals("declared container moves document",tree.get(containerState.vertical)->scrollbar->position,16);
        const bool comboEditorDefaults = scrollFactory.loadDefaultsFile(tree,"widgets/line_editor.xml",error);
        ensure(error,comboEditorDefaults);
        const bool comboDefaults = scrollFactory.loadDefaultsFile(tree,"widgets/combo_box.xml",error);
        ensure(error,comboDefaults);
        const auto combo = scrollFactory.construct(tree,
            "<combo_box width='211' height='32' allow_text_entry='true' max_chars='128' combo_editor.prevalidator='ascii'>"
            "<combo_box.combo_editor text_pad_left='8'/><combo_box.item label='Last location' value='last'/>"
            "<combo_box.item label='Home' value='home'/></combo_box>",0,error);
        ensure(error,combo.has_value());
        const auto comboState = *tree.get(*combo)->combo;
        ensure_equals("declared items",comboState.items.size(),std::size_t(2));
        ensure("real combo arrow image",tree.get(comboState.button)->button->images.unselected != nullptr);
        ensure("native editor validator",tree.get(comboState.editor)->lineEditor->params.prevalidator(U"Home"));
        ensure("native editor rejects nonascii",!tree.get(comboState.editor)->lineEditor->params.prevalidator(U"\u03a9"));
        ensure("declared Home selection",tree.setValue(*combo,LLSD("home")));
        ensure_equals("declared combo label",tree.value(comboState.editor).asString(),std::string("Home"));
        const bool textDefaults = scrollFactory.loadDefaultsFile(tree,"widgets/text.xml",error);
        ensure(error,textDefaults);
        const auto textLabel = scrollFactory.construct(tree,
            "<text width='140' height='16' font='SansSerifMedium' text_color='EmphasisColor'>Create an account</text>",0,error);
        ensure(error,textLabel.has_value());
        ensure_equals("packaged literal text",tree.value(*textLabel).asString(),std::string("Create an account"));
        for (const std::string widget : {"button","icon","check_box","web_browser","layout_stack"})
        {
            const bool loaded = scrollFactory.loadDefaultsFile(tree,"widgets/"+widget+".xml",error);
            ensure(widget+": "+error,loaded);
        }
        tree.defineSetting("FSRememberUsername",LLSD(true),LLVKWidgetTree::SettingType::Boolean);
        tree.defineSetting("RememberPassword",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        tree.defineSetting("NextLoginLocation",LLSD("last"),LLVKWidgetTree::SettingType::String);
        const auto login = scrollFactory.constructFile(tree,"panel_fs_nui_login.xml",0,error);
        ensure("packaged login: "+error,login.has_value());
        ensure("native login root",tree.get(*login)->panel.has_value());
        ensure("native login stack layout",tree.prepareLayoutStacks(*login,0,error));
        LLVKWidgetPaint::Input paintInput;
        const auto paint = LLVKWidgetPaint::prepare(tree,*login,paintInput,error);
        ensure("native login paint: "+error,paint.has_value());
        ensure("login produces native paint commands",paint->commands.size() > 20);
        ensure_equals("browser explicitly pending",paint->pendingBrowsers.size(),std::size_t(1));
        bool logoPainted = false, buttonPainted = false, textPainted = false;
        for (const auto& command : paint->commands)
        {
            const auto* owner = tree.get(command.owner);
            logoPainted |= command.image && command.image->name() == "login_fs_logo";
            buttonPainted |= owner->params.name == "connect_btn" && bool(command.image);
            textPainted |= owner->params.name == "forgot_password_text" && command.text && !command.text->glyphs.empty();
        }
        ensure("actual login logo painted",logoPainted);
        ensure("actual connect button painted",buttonPainted);
        ensure("actual password link painted",textPainted);
    }

    template<> template<> void object::test<69>()
    {
        set_test_name("native line editor reshape stages cursor scroll with ancestor geometry");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,200,30};
        auto parent = tree.create(view,0,error);
        ensure(error,parent.has_value());
        view.follows = LLVKWidgetTree::Left | LLVKWidgetTree::Right;
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::LineEditorParams params;
        params.text.defaultText = "A long editable line that must scroll";
        params.text.leftPadding = params.text.rightPadding = 2;
        auto editor = tree.createLineEditor(view,control,params,*parent,error);
        ensure(error,editor.has_value());
        auto expected = tree.get(*editor)->lineEditor->text;
        ensure("reference text resize",expected.resize(25,error));
        ensure("ancestor resize",tree.reshape(*parent,25,30,error));
        ensure_equals("child geometry follows",tree.get(*editor)->params.rect.right,25);
        ensure_equals("text scroll follows ancestor resize",tree.get(*editor)->lineEditor->text.scroll(),expected.scroll());
        ensure_equals("text cursor retained",tree.get(*editor)->lineEditor->text.cursor(),expected.cursor());
        const auto before = tree.get(*parent)->params.rect;
        const auto scroll = tree.get(*editor)->lineEditor->text.scroll();
        ensure("negative child text width fails transaction",!tree.reshape(*parent,-1,30,error));
        ensure("parent geometry unchanged on failure",tree.get(*parent)->params.rect == before);
        ensure_equals("text scroll unchanged on failure",tree.get(*editor)->lineEditor->text.scroll(),scroll);
        ensure("explicit shape update",tree.setShape(*editor,{4,5,104,35},error));
        ensure("expected explicit width",expected.resize(100,error));
        ensure_equals("shape update synchronizes text scroll",tree.get(*editor)->lineEditor->text.scroll(),expected.scroll());
        const auto border = tree.get(*editor)->lineEditor->border;
        ensure_equals("border width follows editor",tree.get(border)->params.rect.right,99);
    }

    template<> template<> void object::test<68>()
    {
        set_test_name("native Unicode edits dirty the editor and preserve validation rollback ordering");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = LLSD("ab");
        LLVKWidgetTree::LineEditorParams params;
        params.text.maximumBytes = 3;
        std::vector<std::string> events;
        params.keystroke.function = [&](auto,const LLSD& value) { events.push_back("key:"+value.asString()); };
        params.prevalidator = [](auto text) { return text.find(U'x') == text.npos; };
        auto id = tree.createLineEditor({},control,params,0,error);
        ensure(error,id.has_value());
        LLVKWidgetTree::Events effects;
        effects.badKeystroke = [&](auto) { events.push_back("bad"); };
        effects.hideCursor = [&](auto) { events.push_back("hide"); };
        tree.setEvents(*id,effects);
        ensure("unfocused ignores Unicode",!tree.lineEditorUnicode(*id,U'c',false,error));
        tree.setKeyboardFocus(*id,false,false,error);
        ensure("character inserted",tree.lineEditorUnicode(*id,U'c',false,error));
        ensure_equals("typed value",tree.value(*id).asString(),std::string("abc"));
        ensure("typing dirty",tree.dirty(*id));
        ensure("effects before callback",events == std::vector<std::string>{"hide","key:abc"});
        events.clear();
        tree.lineEditorUnicode(*id,U'd',false,error);
        ensure_equals("byte cap retains text",tree.value(*id).asString(),std::string("abc"));
        ensure("limited input still callback",events == std::vector<std::string>{"bad","hide","key:abc"});
        events.clear();
        tree.setValue(*id,LLSD("ab"));
        tree.lineEditorUnicode(*id,U'x',false,error);
        ensure_equals("invalid input rolled back",tree.value(*id).asString(),std::string("ab"));
        ensure("rollback baseline reset",!tree.dirty(*id));
        ensure("rollback no keystroke callback",events == std::vector<std::string>{"hide","bad"});
        params.text.selectOnFocus = true;
        auto selected = tree.createLineEditor({},control,params,0,error);
        ensure(error,selected.has_value());
        tree.requestControlFocus(*selected,true,error);
        ensure("focus selects but finishes dragging",!tree.get(*selected)->lineEditor->text.selecting());
        tree.lineEditorUnicode(*selected,U'Q',false,error);
        ensure_equals("typed selection replaced",tree.value(*selected).asString(),std::string("Q"));
        params.inputPrevalidator = [&](auto) { std::string ignored; tree.eraseControl(*selected,ignored); return true; };
        params.keystroke.function = [&](auto target,const LLSD&) { std::string ignored; tree.eraseControl(target,ignored); };
        auto deleting = tree.createLineEditor({},control,params,0,error);
        ensure(error,deleting.has_value());
        tree.setKeyboardFocus(*deleting,false,false,error);
        ensure("self deleting callback handled",tree.lineEditorUnicode(*deleting,U'Q',false,error));
        ensure("keystroke owner removed",!tree.get(*deleting));
    }

    template<> template<> void object::test<67>()
    {
        set_test_name("native line editor language input precedes focus-loss commit and teardown suppresses commit");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = LLSD("history");
        std::vector<std::string> events;
        control.commit.function = [&](auto,const LLSD&) { events.push_back("commit"); };
        LLVKWidgetTree::LineEditorParams params;
        auto id = tree.createLineEditor({},control,params,0,error);
        ensure(error,id.has_value());
        LLVKWidgetTree::Events callbacks;
        callbacks.languageInput = [&](auto,bool enabled) { events.push_back(enabled ? "language on" : "language off"); };
        callbacks.focusReceived = [&](auto) { events.push_back("focus received"); };
        callbacks.focusLost = [&](auto) { events.push_back("focus lost"); };
        tree.setEvents(*id,callbacks);
        tree.setKeyboardFocus(*id,false,false,error);
        ensure("language enable after focus callback",events == std::vector<std::string>{"focus received","language on"});
        events.clear();
        tree.enableLineHistory(*id,true);
        tree.commit(*id);
        tree.commit(*id);
        ensure("history duplicate suppressed",tree.get(*id)->lineEditor->history == std::vector<std::string>{"history",""});
        tree.clearLineEditor(*id,error);
        events.clear();
        tree.setKeyboardFocus(0,false,false,error);
        ensure("language shutdown before commit before base callback",events == std::vector<std::string>{"language off","commit","focus lost"});
        ensure("commit resets baseline",!tree.dirty(*id));
        tree.setValue(*id,LLSD("changed"));
        tree.setKeyboardFocus(*id,false,false,error);
        tree.clearLineEditor(*id,error);
        events.clear();
        tree.eraseControl(*id,error);
        ensure("destructor suppresses commit",events == std::vector<std::string>{"language off","focus lost"});
        params.text.password = true;
        id = tree.createLineEditor({},control,params,0,error);
        ensure(error,id.has_value());
        tree.setEvents(*id,callbacks);
        events.clear();
        tree.setKeyboardFocus(*id,false,false,error);
        ensure("Windows password disables IME",events == std::vector<std::string>{"focus received","language off"});
    }

    template<> template<> void object::test<66>()
    {
        set_test_name("native line editor owns border before init and disabled state is read-only");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect = {5,6,105,36};
        view.enabled = false;
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = LLSD("long initial description");
        tree.defineSetting("enabled",LLSD(true),LLVKWidgetTree::SettingType::Boolean);
        control.enabledSetting = "enabled";
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            ensure("real line editor before init",node->lineEditor.has_value());
            ensure("native border already attached",tree.get(node->lineEditor->border)->border.has_value());
            ensure("binding applied before init",!node->lineEditor->readOnly);
            ensure_equals("identical initial assignment does not retruncate",tree.value(id).asString(),std::string("long initial description"));
        };
        LLVKWidgetTree::LineEditorParams editor;
        editor.text.maximumBytes = 4;
        auto id = tree.createLineEditor(view,control,editor,0,error);
        ensure(error,id.has_value());
        ensure("base remains enabled",tree.get(*id)->params.enabled);
        ensure("explicit enabled reapplied after init",tree.get(*id)->lineEditor->readOnly);
        ensure("readonly not in tab order",!tree.get(*id)->control->params.tabStop);
        const auto border = tree.get(*id)->lineEditor->border;
        ensure("border inset top and right",tree.get(border)->params.rect == LLVKWidgetTree::Rect{0,0,99,29});
        ensure("border inward",tree.get(border)->border->params.bevel == LLVKBorder::Bevel::In);
        ensure("setValue routes native text state",tree.setValue(*id,LLSD("abcdef")));
        ensure_equals("subsequent value respects byte limit",tree.value(*id).asString(),std::string("abcd"));
        ensure("assigned value clean",!tree.dirty(*id));
        ensure("clear changes baseline",tree.clearLineEditor(*id,error));
        ensure("dirty forwarded",tree.dirty(*id));
        tree.resetDirty(*id);
        ensure("dirty reset forwarded",!tree.dirty(*id));
        tree.setEnabled(*id,true);
        ensure("writable tab stop restored",!tree.get(*id)->lineEditor->readOnly && tree.get(*id)->control->params.tabStop);
        tree.eraseControl(*id,error);
        ensure("border retired with editor",!tree.get(border));
    }

    template<> template<> void object::test<65>()
    {
        set_test_name("native line editor text initialization selection dirty baseline and password hit testing");
        auto font = loadFont();
        LLVKLineEditor::Params params;
        params.width = 100;
        params.maximumBytes = 4;
        params.defaultText = "abcdef";
        std::string error;
        auto editor = LLVKLineEditor::create(font,params,std::nullopt,{},error);
        ensure(error,editor.has_value());
        ensure_equals("default text limited",editor->text(),std::string("abcd"));
        ensure_equals("constructor cursor at end",editor->cursor(),std::size_t(4));
        ensure("constructor clean",!editor->dirty());
        editor = LLVKLineEditor::create(font,params,std::string("descriptive initial value"),{},error);
        ensure(error,editor.has_value());
        ensure_equals("constructor initial ignores limit",editor->text(),std::string("descriptive initial value"));
        ensure("select all",editor->selectAll(error));
        ensure("replace selected value",editor->assign("new",true,false,{},error));
        ensure_equals("whole selection retained",editor->selectionStart(),std::size_t(3));
        ensure_equals("selected cursor at zero",editor->cursor(),std::size_t(0));
        ensure("assignment resets dirty baseline",!editor->dirty());
        ensure("identical text keeps selection",editor->assign("new",true,false,{},error));
        ensure("selection unchanged",editor->selecting());
        ensure("clear",editor->clear(error));
        ensure("clear differs from prior baseline",editor->dirty());
        editor->resetDirty();
        ensure("reset dirty",!editor->dirty());
        params.maximumBytes = 100;
        params.maximumCharacters = 2;
        params.defaultText = "A\xc3\xa9Z";
        editor = LLVKLineEditor::create(font,params,std::nullopt,{},error);
        ensure(error,editor.has_value());
        ensure("character cap preserves Unicode scalar",editor->display() == U"A\u00e9");
        params.maximumCharacters = 0;
        params.defaultText = "WWWW";
        params.password = true;
        editor = LLVKLineEditor::create(font,params,std::nullopt,{},error);
        ensure(error,editor.has_value());
        auto expected = font->hitTest(U"\u2022\u2022\u2022\u2022",0,10,101,4,1,true,false,error);
        ensure(error,expected.has_value());
        auto hit = editor->hitTest(10,error);
        ensure(error,hit.has_value());
        ensure_equals("password uses bullets for hit test",*hit,*expected);
        ensure("negative width rejected",!editor->resize(-1,error));
        ensure_equals("failure retains text",editor->text(),std::string("WWWW"));
    }

    template<> template<> void object::test<64>()
    {
        set_test_name("native image aliases share immutable pixel storage but retain separate metadata and names");
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        auto files = std::make_shared<LLVKSkinFiles>(configuration);
        LLVKSkinImages catalog(files);
        std::string error;
        ensure("packaged declarations",catalog.loadDeclarations(error));
        const auto filename = catalog.declaration("PushButton_Off")->filename;
        const std::string metadata[]{"<textures version='101'><texture name='first' file_name='"+filename+"'/>"
            "<texture name='second' file_name='"+filename+"' clip.left='1' clip.bottom='1' clip.right='4' clip.top='5'/></textures>"};
        ensure("alias declarations",catalog.loadDeclarations(metadata,error));
        auto first = catalog.image("first",error);
        ensure(error,first != nullptr);
        const auto resident = catalog.residentBytes();
        auto second = catalog.image("second",error);
        ensure(error,second != nullptr);
        ensure("named views have distinct identities",first != second);
        ensure("decoded pixel storage shared",first->bottomUpRgba().data() == second->bottomUpRgba().data());
        ensure_equals("second view clip width",second->width(),3u);
        ensure_equals("second view clip height",second->height(),4u);
        ensure("first view unchanged",first->width() > second->width());
        ensure_equals("aliased pixels counted once",catalog.residentBytes(),resident);
        ensure_equals("first logical name",first->name(),std::string("first"));
        ensure_equals("second logical name",second->name(),std::string("second"));
        first.reset();
        ensure("remaining view retains pixel owner",!second->bottomUpRgba().empty());
    }

    template<> template<> void object::test<63>()
    {
        set_test_name("native J2C local decoder owns pixels and rejects incomplete codestreams");
        std::ifstream stream(std::string(LLVK_WIDGET_SKIN_FIXTURE)+"/textures/rounded_square.j2c",std::ios::binary);
        ensure("J2C fixture exists",stream.good());
        std::vector<std::uint8_t> encoded{std::istreambuf_iterator<char>(stream),{}};
        std::string error;
        const auto image = LLVKWidgetImage::decodeJ2c("rounded",encoded,error);
        ensure(error,image != nullptr);
        ensure("decoded local dimensions",image->width() > 1 && image->height() > 1);
        const auto skin = LLVKWidgetImage::decodeSkin("skin",encoded,{},error);
        ensure(error,skin != nullptr);
        ensure_equals("skin logical size",skin->width(),image->width());
        ensure("truncated header fails",!LLVKWidgetImage::decodeJ2c("bad",std::span(encoded).first(20),error));
        ensure("truncated codestream fails",!LLVKWidgetImage::decodeJ2c("bad",std::span(encoded).first(encoded.size()/2),error));
        const std::uint8_t invalid[]{0xff,0x4f,0xff,0x51};
        ensure("invalid codestream fails",!LLVKWidgetImage::decodeJ2c("bad",invalid,error));
    }

    template<> template<> void object::test<62>()
    {
        set_test_name("native decoder qualifies every available packaged local image declaration");
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        auto files = std::make_shared<LLVKSkinFiles>(configuration);
        LLVKSkinImages catalog(files);
        std::string error;
        const bool loaded = catalog.loadDeclarations(error);
        ensure(error,loaded);
        std::size_t decoded = 0, missing = 0, unsupported = 0;
        for (const auto& [name,declaration] : catalog.declarations())
        {
            const auto filename = declaration.filename.empty() ? name : declaration.filename;
            const auto extension = std::filesystem::path(filename).extension().string();
            if (extension != ".png" && extension != ".tga" && extension != ".jpg" && extension != ".jpeg" && extension != ".j2c")
            { ++unsupported; continue; }
            const auto paths = files->find("textures",filename,LLVKSkinFiles::Policy::Current,error);
            ensure(error,paths.has_value());
            if (paths->empty()) { ++missing; continue; }
            auto image = catalog.image(name,error);
            ensure(name+": "+error,image != nullptr);
            ensure_equals("published bytes match pixel extent",image->bottomUpRgba().size(),
                std::size_t(image->pixelWidth())*image->pixelHeight()*4);
            ensure("nonempty logical extent",image->width() > 0 && image->height() > 0);
            ++decoded;
        }
        ensure("substantial packaged declaration coverage",decoded > 500);
        ensure_equals("all declared image formats implemented",unsupported,std::size_t(0));
        std::cout << "Native skin qualification: decoded=" << decoded << " missing=" << missing
                  << " other_formats=" << unsupported << " bytes=" << catalog.residentBytes() << '\n';
    }

    template<> template<> void object::test<61>()
    {
        set_test_name("native TGA decoder preserves orientation RLE palette rounding and padding alpha");
        const auto header = [](std::uint8_t type,std::uint8_t depth,std::uint8_t flags)
        {
            std::vector<std::uint8_t> bytes(18,0);
            bytes[2] = type; bytes[12] = 1; bytes[14] = 2; bytes[16] = depth; bytes[17] = flags;
            return bytes;
        };
        std::string error;
        auto bytes = header(2,32,0);
        bytes.insert(bytes.end(),{255,0,0,255,0,0,255,255});
        auto image = LLVKWidgetImage::decodeTga("raw",bytes,error);
        ensure(error,image != nullptr);
        ensure_equals("bottom blue",unsigned(image->bottomUpRgba()[2]),255u);
        ensure_equals("top red",unsigned(image->bottomUpRgba()[4]),255u);
        auto skin = LLVKWidgetImage::decodeSkin("skin",bytes,{},error);
        ensure(error,skin != nullptr);
        ensure_equals("all opaque TGA compacted before padding",unsigned(skin->bottomUpRgba()[7]),255u);
        bytes[21] = 127;
        skin = LLVKWidgetImage::decodeSkin("skin",bytes,{},error);
        ensure(error,skin != nullptr);
        ensure_equals("alpha retained means transparent padding",unsigned(skin->bottomUpRgba()[7]),0u);
        bytes[17] = 0x20;
        image = LLVKWidgetImage::decodeTga("top",bytes,error);
        ensure(error,image != nullptr);
        ensure_equals("top origin reverses input",unsigned(image->bottomUpRgba()[0]),255u);
        auto rle = header(10,24,0);
        rle.insert(rle.end(),{0x81,0,255,0});
        image = LLVKWidgetImage::decodeTga("rle",rle,error);
        ensure(error,image != nullptr);
        ensure_equals("repeated green",unsigned(image->bottomUpRgba()[5]),255u);
        rle[18] = 0x82;
        ensure("packet overflow rejected",!LLVKWidgetImage::decodeTga("bad",rle,error));
        auto palette = header(1,8,0);
        palette[1] = 1; palette[3] = 5; palette[5] = 2; palette[7] = 24;
        palette.insert(palette.end(),{255,0,0,0,0,255,5,255});
        image = LLVKWidgetImage::decodeTga("palette",palette,error);
        ensure(error,image != nullptr);
        ensure_equals("palette origin normalized",unsigned(image->bottomUpRgba()[2]),255u);
        ensure_equals("out of range palette index clamps last",unsigned(image->bottomUpRgba()[4]),255u);
        auto rgb16 = header(2,16,0);
        rgb16.insert(rgb16.end(),{0x08,0x21,0x08,0x21});
        image = LLVKWidgetImage::decodeTga("rgb16",rgb16,error);
        ensure(error,image != nullptr);
        ensure_equals("5bit rounded not truncated",unsigned(image->bottomUpRgba()[0]),66u);
        ensure("truncated packet rejected",!LLVKWidgetImage::decodeTga("bad",std::span(bytes).first(20),error));
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        LLVKSkinImages catalog(std::make_shared<LLVKSkinFiles>(configuration));
        ensure("catalog declarations",catalog.loadDeclarations(error));
        const auto actual = catalog.image("Folder_Arrow",error);
        ensure(error,actual != nullptr);
        ensure("actual skin TGA has dimensions",actual->width() > 1 && actual->height() > 1);
    }

    template<> template<> void object::test<60>()
    {
        set_test_name("native JPEG decodes bottom-up opaque RGB and rejects corrupt payloads");
        jpeg_compress_struct encoder{};
        jpeg_error_mgr errors{};
        encoder.err = jpeg_std_error(&errors);
        jpeg_create_compress(&encoder);
        unsigned char* buffer = nullptr;
        unsigned long size = 0;
        jpeg_mem_dest(&encoder,&buffer,&size);
        encoder.image_width = 8;
        encoder.image_height = 16;
        encoder.input_components = 3;
        encoder.in_color_space = JCS_RGB;
        jpeg_set_defaults(&encoder);
        jpeg_set_quality(&encoder,100,TRUE);
        jpeg_start_compress(&encoder,TRUE);
        std::vector<std::uint8_t> row(8*3);
        while (encoder.next_scanline < encoder.image_height)
        {
            for (std::size_t pixel = 0; pixel < 8; ++pixel)
            {
                row[pixel*3] = encoder.next_scanline < 8 ? 255 : 0;
                row[pixel*3+1] = 0;
                row[pixel*3+2] = encoder.next_scanline < 8 ? 0 : 255;
            }
            JSAMPROW scanline = row.data();
            jpeg_write_scanlines(&encoder,&scanline,1);
        }
        jpeg_finish_compress(&encoder);
        jpeg_destroy_compress(&encoder);
        std::unique_ptr<unsigned char,decltype(&std::free)> owner(buffer,&std::free);
        std::string error;
        const std::span<const std::uint8_t> bytes(buffer,size);
        auto image = LLVKWidgetImage::decodeJpeg("jpeg",bytes,error);
        ensure(error,image != nullptr);
        ensure_equals("decoded height",image->height(),16u);
        const auto pixels = image->bottomUpRgba();
        ensure("bottom source blue",pixels[2] > 240 && pixels[0] < 15);
        ensure("top source red",pixels[15*8*4] > 240 && pixels[15*8*4+2] < 15);
        for (std::size_t alpha = 3; alpha < pixels.size(); alpha += 4) ensure_equals("opaque JPEG alpha",unsigned(pixels[alpha]),255u);
        auto skin = LLVKWidgetImage::decodeSkin("skin",bytes,{},error);
        ensure(error,skin != nullptr);
        ensure_equals("skin dispatch preserves height",skin->height(),16u);
        ensure("truncated header rejects",!LLVKWidgetImage::decodeJpeg("bad",bytes.first(20),error));
        ensure("truncated end rejects warning",!LLVKWidgetImage::decodeJpeg("bad",bytes.first(bytes.size()-2),error));
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        auto files = std::make_shared<LLVKSkinFiles>(configuration);
        LLVKSkinImages catalog(files);
        auto actual = catalog.image("windows/first_login_image.jpg",error);
        ensure(error,actual != nullptr);
        ensure("packaged JPEG decoded",actual->width() > 8 && actual->height() > 8);
    }

    template<> template<> void object::test<59>()
    {
        set_test_name("native font descriptors resolve after all attributes with alias and fallback precedence");
        std::string error;
        LLVKFontRegistry::Configuration configuration;
        configuration.platform = "windows";
        configuration.searchDirectories = {std::filesystem::path(LLVK_WIDGET_FONT_FIXTURE).parent_path()};
        const std::string documents[]{"<fonts><font_size name='Small' size='10'/><font_size name='Large' size='18'/>"
            "<font name='Test' font_style='NORMAL'><file>Roboto-Regular.ttf</file></font>"
            "<font name='Test' font_style='BOLD'><file>Roboto-Regular.ttf</file></font></fonts>"};
        LLVKWidgetFactory::Resources resources;
        resources.fontRegistry = LLVKFontRegistry::create(documents,configuration,error);
        ensure(error,resources.fontRegistry != nullptr);
        resources.defaultFontRequest = {"Test","Small",0,false};
        resources.fonts["Alias"] = loadFont();
        resources.fallbackFont = resources.fonts.at("Alias");
        LLVKWidgetTree tree;
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        auto first = factory.construct(tree,"<button font.size='Large' font.style='BOLD' font='Test'/>",0,error);
        ensure(error,first.has_value());
        auto second = factory.construct(tree,"<button font='Test' font.style='BOLD' font.size='Large'/>",0,error);
        ensure(error,second.has_value());
        ensure("attribute order does not change descriptor",tree.get(*first)->control->params.font == tree.get(*second)->control->params.font);
        ensure("requested descriptor retained",tree.get(*first)->control->params.fontRequest->style == 1);
        const auto expected = resources.fontRegistry->resolve({"Test","Large",1,false},error);
        ensure(error,expected != nullptr);
        ensure("native registry supplies font",tree.get(*first)->control->params.font == expected);
        auto alias = factory.construct(tree,"<button font='Alias' font.size='Large' font.style='BOLD'/>",0,error);
        ensure(error,alias.has_value());
        ensure("named alias overrides descriptor fields",tree.get(*alias)->control->params.font == resources.fonts.at("Alias"));
        auto fallback = factory.construct(tree,"<button font='Missing' font.size='Missing'/>",0,error);
        ensure(error,fallback.has_value());
        ensure("explicit native fallback on resolution failure",tree.get(*fallback)->control->params.font == resources.fallbackFont);
        auto lower = factory.construct(tree,"<button font='Test' font.style='bold'/>",0,error);
        ensure(error,lower.has_value());
        ensure_equals("source style matching remains case-sensitive",unsigned(tree.get(*lower)->control->params.fontRequest->style),0u);
    }

    template<> template<> void object::test<58>()
    {
        set_test_name("native widget constructors resolve real named assets and reject failed image publication");
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        auto files = std::make_shared<LLVKSkinFiles>(configuration);
        auto images = std::make_shared<LLVKSkinImages>(files);
        std::string error;
        const bool loaded = images->loadDeclarations(error);
        ensure(error,loaded);
        LLVKWidgetTree tree;
        tree.setSkinImages(images);
        LLVKWidgetFactory::Resources resources;
        resources.skinFiles = files;
        resources.fonts["SansSerifSmall"] = loadFont();
        resources.colors = std::make_shared<LLVKColorTable>();
        const auto colors = files->read("","colors.xml",LLVKSkinFiles::Policy::All,error);
        ensure(error,colors.has_value());
        std::vector<std::string> warnings;
        for (const auto& file : *colors) ensure("colors load",resources.colors->load(file,LLVKColorTable::Layer::Loaded,warnings,error));
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        const bool defaults = factory.loadDefaultsFile(tree,"widgets/button.xml",error);
        ensure(error,defaults);
        auto button = factory.construct(tree,"<button label='Real assets' width='100'/>",0,error);
        ensure(error,button.has_value());
        const auto original = tree.get(*button)->button->images.unselected;
        ensure("decoded image available at construction",original && original->width() > 1);
        ensure("default disabled image not replaced",tree.get(*button)->button->images.disabled != original);
        auto icon = factory.construct(tree,"<icon font='SansSerifSmall' image_name='PushButton_Off'/>",0,error);
        ensure(error,icon.has_value());
        ensure("icon uses same native cached image",tree.get(*icon)->icon->image == original);
        ensure("icon value resolves another native asset",tree.setValue(*icon,LLSD("PushButton_Selected")));
        ensure("icon updated owner",tree.get(*icon)->icon->image == tree.get(*button)->button->images.selected);
        const auto value = tree.value(*icon);
        ensure("bad image assignment rejected",!tree.setValue(*icon,LLSD("missing.png")));
        ensure_equals("failed assignment keeps value",tree.value(*icon).asString(),value.asString());
        const auto size = tree.size();
        ensure("missing declared image rejects constructor",!factory.construct(tree,"<button image_unselected='missing.png'/>",0,error));
        ensure_equals("failed constructor no partial widget",tree.size(),size);
        auto none = factory.construct(tree,"<button image_unselected='none'/>",0,error);
        ensure(error,none.has_value());
        ensure("explicit none remains valid",!tree.get(*none)->button->images.unselected);
    }

    template<> template<> void object::test<57>()
    {
        set_test_name("native image declarations merge provided fields and load real skin PNG ownership");
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        auto files = std::make_shared<LLVKSkinFiles>(configuration);
        LLVKSkinImages catalog(files);
        std::string error;
        const std::string declarations[]{
            "<textures version='101'><texture name='image' file_name='old.png' preload='true' scale.left='1' scale.bottom='2' scale.right='3' scale.top='4'/></textures>",
            "<textures><texture name='image' file_name='new.png' use_mips='true' scale.left='9' scale_type='scale_outer'/></textures>"};
        ensure("metadata layers",catalog.loadDeclarations(declarations,error));
        const auto* entry = catalog.declaration("image");
        ensure("declaration exists",entry != nullptr);
        ensure_equals("filename override",entry->filename,std::string("new.png"));
        ensure("absent preload retained",entry->preload);
        ensure("mip flag retained as source metadata",entry->useMips);
        ensure("partial scale rect ignored",entry->metadata.scale == LLVKWidgetImage::Rect{1,2,3,4});
        ensure("scale style override",entry->metadata.style == LLVKWidgetImage::Scale::Outer);
        const bool loaded = catalog.loadDeclarations(error);
        ensure(error,loaded);
        auto button = catalog.image("PushButton_Off",error);
        ensure(error,button != nullptr);
        ensure("real button has multiple pixels",button->width() > 1 && button->height() > 1);
        ensure("actual nine-slice metadata",button->scaleRegion().left > 0.f && button->scaleRegion().right < 1.f);
        ensure("same named image stable owner",button == catalog.image("PushButton_Off",error));
        const auto resident = catalog.residentBytes();
        ensure_equals("cache hit no allocation",resident,button->bottomUpRgba().size());
        ensure("none is an explicit null",!catalog.image("none",error) && error.empty());
        ensure("published metadata cannot be replaced silently",!catalog.loadDeclarations(declarations,error));
        LLVKSkinImages limited(files,1);
        ensure("limited declarations load",limited.loadDeclarations(error));
        ensure("residency failure explicit",!limited.image("PushButton_Off",error));
        ensure_equals("failed image not published",limited.residentBytes(),std::size_t(0));
    }

    template<> template<> void object::test<56>()
    {
        set_test_name("native skin pixels preserve padded extent alpha and clipped logical dimensions");
        std::string error;
        for (const bool alpha : {false,true})
        {
            png_image encoder{};
            encoder.version = PNG_IMAGE_VERSION;
            encoder.width = 3;
            encoder.height = 2;
            encoder.format = alpha ? PNG_FORMAT_RGBA : PNG_FORMAT_RGB;
            const std::vector<std::uint8_t> pixels(3*2*(alpha ? 4 : 3),127);
            png_alloc_size_t length = 0;
            ensure("encoded size",png_image_write_to_memory(&encoder,nullptr,&length,0,pixels.data(),0,nullptr) != 0);
            std::vector<std::uint8_t> encoded(length);
            ensure("encode skin pixels",png_image_write_to_memory(&encoder,encoded.data(),&length,0,pixels.data(),0,nullptr) != 0);
            png_image_free(&encoder);
            auto decoded = LLVKWidgetImage::decodeSkinPng("native",encoded,{},error);
            ensure(error,decoded != nullptr);
            ensure_equals("logical width preserves original",decoded->width(),3u);
            ensure_equals("logical height preserves original",decoded->height(),2u);
            ensure_equals("padded pixel width",decoded->pixelWidth(),4u);
            ensure_equals("padded pixel height",decoded->pixelHeight(),4u);
            ensure_equals("original right UV",decoded->clipRegion().right,0.75f);
            ensure_equals("original top UV",decoded->clipRegion().top,0.5f);
            const auto rgba = decoded->bottomUpRgba();
            ensure_equals("padded pixel bytes",rgba.size(),std::size_t(64));
            ensure_equals("padding RGB black",unsigned(rgba[12]),0u);
            ensure_equals("padding alpha follows source components",unsigned(rgba[15]),alpha ? 0u : 255u);
            LLVKWidgetImage::Metadata metadata;
            metadata.clip = LLVKWidgetImage::Rect{1,0,3,2};
            metadata.scale = LLVKWidgetImage::Rect{1,-3,8,1};
            metadata.style = LLVKWidgetImage::Scale::Outer;
            decoded = LLVKWidgetImage::decodeSkinPng("clipped",encoded,metadata,error);
            ensure(error,decoded != nullptr);
            ensure_equals("clipped width",decoded->width(),2u);
            ensure_equals("clip coordinate based on padded extent",decoded->clipRegion().left,0.25f);
            ensure_equals("scale based on logical width",decoded->scaleRegion().left,0.5f);
            ensure_equals("scale right clamped",decoded->scaleRegion().right,1.f);
            ensure_equals("scale bottom clamped",decoded->scaleRegion().bottom,0.f);
            ensure("outer scaling retained",decoded->scaleStyle() == LLVKWidgetImage::Scale::Outer);
            metadata.clip = LLVKWidgetImage::Rect{3,0,1,2};
            ensure("inverted clip explicit",!LLVKWidgetImage::decodeSkinPng("bad",encoded,metadata,error));
        }
    }

    template<> template<> void object::test<55>()
    {
        set_test_name("native factory loads packaged widget templates through owned skin IO");
        LLVKWidgetTree tree;
        std::string error;
        LLVKSkinFiles::Configuration configuration;
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.userAppDirectory = "unused-profile";
        LLVKWidgetFactory::Resources resources;
        resources.skinFiles = std::make_shared<LLVKSkinFiles>(configuration);
        resources.colors = std::make_shared<LLVKColorTable>();
        resources.fonts["SansSerifSmall"] = loadFont();
        const auto colors = resources.skinFiles->read("","colors.xml",LLVKSkinFiles::Policy::All,error);
        ensure(error,colors.has_value());
        std::vector<std::string> warnings;
        for (const auto& file : *colors)
        {
            const bool loaded = resources.colors->load(file,LLVKColorTable::Layer::Loaded,warnings,error);
            ensure(error,loaded);
        }
        LLVKWidgetFactory::PanelDefaults panelDefaults;
        panelDefaults.control.font = resources.fonts.at("SansSerifSmall");
        LLVKWidgetFactory::IconDefaults iconDefaults;
        iconDefaults.control.font = resources.fonts.at("SansSerifSmall");
        LLVKWidgetFactory factory({},iconDefaults,{}, {},resources,panelDefaults);
        for (const auto* file : {"widgets/view_border.xml","widgets/badge.xml","widgets/button.xml","widgets/icon.xml","widgets/panel.xml"})
        {
            const bool loaded = factory.loadDefaultsFile(tree,file,error);
            ensure(error,loaded);
        }
        auto panel = factory.construct(tree,
            "<panel width='200' height='100' border='true'><button name='action' label='Ready' width='80'/></panel>",0,error);
        ensure(error,panel.has_value());
        const auto button = tree.get(*panel)->children.front();
        ensure("button font from packaged template",tree.get(button)->control->params.font == resources.fonts.at("SansSerifSmall"));
        ensure("panel theme loaded from packaged template",tree.get(*panel)->panel->params.opaqueColor == *resources.colors->find("PanelFocusBackgroundColor"));
        ensure("panel border theme loaded",tree.get(tree.get(*panel)->panel->border)->border->params.highlightLight == *resources.colors->find("DefaultHighlightLight"));
        ensure_equals("packaged button height",tree.get(button)->params.rect.top-tree.get(button)->params.rect.bottom,23);
        ensure("no image substitution for unavailable assets",!tree.get(button)->button->images.unselected);
    }

    template<> template<> void object::test<54>()
    {
        set_test_name("native skin lookup selects default and current language independently");
        LLVKSkinFiles::Configuration configuration;
        configuration.executableDirectory = "app";
        configuration.workingDirectory = "app";
        configuration.skinBaseDirectory = "skins";
        configuration.userAppDirectory = "profile";
        configuration.skin = "custom";
        configuration.theme = "theme";
        configuration.language = "fr";
        std::set<std::string> files{
            "skins/default/xui/en", "skins/default/xui/en/panel.xml", "skins/default/xui/fr/panel.xml",
            "skins/custom/xui/en/panel.xml", "skins/custom/themes/theme/xui/fr/panel.xml",
            "profile/skins/default/xui/en/panel.xml", "profile/skins/custom/xui/fr/panel.xml",
            "skins/default/textures/image.png", "skins/custom/themes/theme/textures/image.png"};
        unsigned probes = 0;
        LLVKSkinFiles resolver(configuration,[&](const auto& path)
        { ++probes; return files.contains(path.generic_string()); });
        std::string error;
        auto paths = resolver.find("xui","panel.xml",LLVKSkinFiles::Policy::Current,error);
        ensure(error,paths.has_value());
        ensure_equals("two independent choices",paths->size(),std::size_t(2));
        ensure_equals("user default language override",paths->front().generic_string(),std::string("profile/skins/default/xui/en/panel.xml"));
        ensure_equals("user current language override",paths->back().generic_string(),std::string("profile/skins/custom/xui/fr/panel.xml"));
        const auto count = probes;
        paths = resolver.find("xui","panel.xml",LLVKSkinFiles::Policy::All,error);
        ensure(error,paths.has_value());
        ensure_equals("all skin paths preserved in search order",paths->size(),std::size_t(6));
        ensure_equals("existence results cached",probes,count);
        files.erase("profile/skins/custom/xui/fr/panel.xml");
        resolver.invalidate();
        paths = resolver.find("xui","panel.xml",LLVKSkinFiles::Policy::Current,error);
        ensure(error,paths.has_value());
        ensure_equals("explicit invalidation discovers theme fallback",paths->back().generic_string(),std::string("skins/custom/themes/theme/xui/fr/panel.xml"));
        paths = resolver.find("textures","image.png",LLVKSkinFiles::Policy::Current,error);
        ensure(error,paths.has_value());
        ensure_equals("textures unlocalized",paths->size(),std::size_t(1));
        ensure("parent traversal rejected",!resolver.find("xui","../panel.xml",LLVKSkinFiles::Policy::Current,error));
        ensure("absolute path rejected",!resolver.find("xui","C:/panel.xml",LLVKSkinFiles::Policy::Current,error));
        configuration.skinBaseDirectory = std::filesystem::path(LLVK_WIDGET_SKIN_FIXTURE).parent_path();
        configuration.skin = "default";
        configuration.theme.clear();
        configuration.language = "en";
        LLVKSkinFiles actual(configuration);
        const auto documents = actual.read("xui","widgets/button.xml",LLVKSkinFiles::Policy::Current,error);
        ensure(error,documents.has_value());
        ensure_equals("packaged default path deduplicated",documents->size(),std::size_t(1));
        ensure("actual file read",documents->front().find("PushButton_Off") != std::string::npos);
    }

    template<> template<> void object::test<53>()
    {
        set_test_name("native layered files feed construction references and widget defaults");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::PanelDefaults panelDefaults;
        panelDefaults.control.font = loadFont();
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = panelDefaults.control.font;
        resources.declarations["base"] = "<panel name='localized' width='100' height='50'>"
            "<string name='caption'>Base</string><button name='action' font='Font' label='Base' width='30'/></panel>";
        resources.declarations["locale"] = "<panel name='localized' width='150'>"
            "<string name='caption'>Localized &amp; preserved</string><button name='action' label='Localized'/></panel>";
        resources.declarationLayers["panel.xml"] = {"base","","locale","base"};
        resources.declarations["buttonBase"] = "<button name='default' font='Font' pad_left='4' height='23'/>";
        resources.declarations["buttonLocale"] = "<button name='default' pad_left='9'/>";
        resources.declarationLayers["widgets/button.xml"] = {"buttonBase","buttonLocale"};
        LLVKWidgetFactory factory({}, {}, {}, {}, resources, panelDefaults);
        ensure("layered defaults load",factory.loadDefaultsFile(tree,"widgets/button.xml",error));
        auto panel = factory.constructFile(tree,"panel.xml",0,error);
        ensure(error,panel.has_value());
        ensure_equals("locale width",tree.get(*panel)->params.rect.right,150);
        const auto button = tree.get(*panel)->children.front();
        ensure("locale label",tree.get(button)->button->params.label == U"Localized");
        ensure_equals("layered defaults inherited",tree.get(button)->button->leftPad,9);
        auto caption = tree.panelString(*panel,"caption",{},error);
        ensure(error,caption.has_value());
        ensure_equals("body and entity survive merge",*caption,std::string("Localized & preserved"));
        auto referenced = factory.construct(tree,"<panel filename='panel.xml' width='200'/>",0,error);
        ensure(error,referenced.has_value());
        ensure_equals("outer reference overrides layered dimensions",tree.get(*referenced)->params.rect.right,200);
        caption = tree.panelString(*referenced,"caption",{},error);
        ensure(error,caption.has_value());
        ensure_equals("references consume layered strings",*caption,std::string("Localized & preserved"));
        const auto size = tree.size();
        ensure("missing logical file rejects",!factory.constructFile(tree,"missing",0,error));
        ensure_equals("missing file leaves owners alone",tree.size(),size);
    }

    template<> template<> void object::test<52>()
    {
        set_test_name("native XML layers preserve source attribute and keyed child update rules");
        std::string error;
        const std::string_view layers[]{
            "<view name='root' width='100'><view name='same' tool_tip='first'/><view name='same' tool_tip='second'/>"
            "<view name='last' tool_tip='old'/></view>",
            "<other name='root' width='200' height='999'><other name='same' tool_tip='one'/>"
            "<other name='same' tool_tip='two'/><view name='unmatched'/><view name='last' tool_tip='new'/></other>"};
        auto merged = LLVKXmlLayers::merge(layers,error);
        ensure(error,merged.has_value());
        LLVKWidgetTree tree;
        LLVKWidgetFactory factory({});
        auto root = factory.construct(tree,*merged,0,error);
        ensure(error,root.has_value());
        ensure_equals("existing attribute updated",tree.get(*root)->params.rect.right,200);
        ensure_equals("new attribute not added",tree.get(*root)->params.rect.top,0);
        const auto& children = tree.get(*root)->children;
        ensure_equals("unmatched nodes not appended",children.size(),std::size_t(3));
        ensure_equals("first duplicate matched first",tree.get(children[2])->params.tooltip,std::string("one"));
        ensure_equals("rotating duplicate match",tree.get(children[1])->params.tooltip,std::string("two"));
        ensure_equals("search continues after missing key",tree.get(children[0])->params.tooltip,std::string("new"));
        const std::string_view combo[]{"<combo name='box'><item value='one' label='old'/></combo>",
            "<combo name='box'><item value='one' label='new'/></combo>"};
        merged = LLVKXmlLayers::merge(combo,error);
        ensure(error,merged.has_value());
        ensure("value-key matching",merged->find("label=\"new\"") != std::string::npos);
        const std::string_view mismatch[]{"<view name='base' width='10'/>","<view name='other' width='20'/>"};
        merged = LLVKXmlLayers::merge(mismatch,error);
        ensure(error,merged.has_value());
        ensure("root name mismatch ignored",merged->find("width=\"10\"") != std::string::npos);
        const std::string_view invalid[]{"<view/>","<view>"};
        ensure("bad overlay rejects entire merge",!LLVKXmlLayers::merge(invalid,error));
        const std::string_view entities[]{"<!DOCTYPE view [<!ENTITY bad 'text'>]><view/>"};
        ensure("DTD rejected",!LLVKXmlLayers::merge(entities,error));
    }

    template<> template<> void object::test<51>()
    {
        set_test_name("native border declarations preserve packaged defaults and panel overrides");
        LLVKWidgetTree tree;
        std::string error;
        std::vector<std::string> warnings;
        LLVKWidgetFactory::Resources resources;
        resources.colors = std::make_shared<LLVKColorTable>();
        std::ifstream colors(std::string(LLVK_WIDGET_SKIN_FIXTURE)+"/colors.xml",std::ios::binary);
        ensure("colors fixture exists",colors.good());
        const std::string colorXml{std::istreambuf_iterator<char>(colors),{}};
        const bool loaded = resources.colors->load(colorXml,LLVKColorTable::Layer::Loaded,warnings,error);
        ensure(error,loaded);
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources, defaults);
        std::ifstream borderFile(std::string(LLVK_WIDGET_SKIN_FIXTURE)+"/xui/en/widgets/view_border.xml",std::ios::binary);
        ensure("border fixture exists",borderFile.good());
        const std::string borderXml{std::istreambuf_iterator<char>(borderFile),{}};
        ensure("packaged border defaults",factory.loadDefaults(tree,borderXml,error));
        auto border = factory.construct(tree,"<view_border width='40' height='20'/>",0,error);
        ensure(error,border.has_value());
        ensure("real border not generic view",tree.get(*border)->border.has_value());
        ensure("border not a control",!tree.get(*border)->control);
        ensure("default color reference retained",tree.get(*border)->border->params.highlightLight == *resources.colors->find("DefaultHighlightLight"));
        ensure("border follows all",tree.get(*border)->params.follows == 15);
        auto panel = factory.construct(tree,"<panel width='80' height='30' border='true' bevel_style='in' thickness='2'/>",0,error);
        ensure(error,panel.has_value());
        const auto* child = tree.get(tree.get(*panel)->panel->border);
        ensure_equals("panel border override thickness",child->border->params.thickness,2);
        ensure("panel border override bevel",child->border->params.bevel == LLVKBorder::Bevel::In);
        ensure("panel border native theme reference",child->border->params.shadowDark == *resources.colors->find("DefaultShadowDark"));
        const auto size = tree.size();
        ensure("unsupported border width rejects",!factory.construct(tree,"<view_border thickness='7'/>",0,error));
        ensure_equals("failed border construction atomic",tree.size(),size);
    }

    template<> template<> void object::test<50>()
    {
        set_test_name("native post-build construction inherits scopes and expires with its owner operation");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = defaults.control.font;
        LLVKWidgetFactory::Construction saved;
        LLVKWidgetTree::Id dynamicButton = 0;
        std::vector<std::string> events;
        resources.panelClasses["Dynamic"] = [&](auto& widgets,const auto& params,const auto&,std::string& problem)
        {
            LLVKWidgetFactory::PanelInstance result;
            auto id = widgets.constructPanel(params.view.view,params.control,params.panel,problem);
            if (!id) return result;
            result.id = *id;
            result.callbacks = std::make_shared<LLVKWidgetFactory::Callbacks>();
            result.callbacks->actions["OnlyLocal"] = [&](auto,const LLSD&) { events.push_back("local"); };
            result.postBuild = [&](auto& owner,auto target,const LLVKWidgetFactory::Construction& context,std::string& failure)
            {
                saved = context;
                auto button = context.construct(
                    "<button name='dynamic' font='Font'><button.commit_callback function='OnlyLocal'/></button>",target,failure);
                if (!button) return false;
                dynamicButton = *button;
                return owner.postBuildControl(target);
            };
            return result;
        };
        LLVKWidgetFactory factory({}, {}, {}, {}, resources, defaults);
        auto panel = factory.construct(tree,"<panel class='Dynamic'/>",0,error);
        ensure(error,panel.has_value());
        ensure_equals("dynamic child attached",tree.get(dynamicButton)->parent,*panel);
        ensure("dynamic handler retained",tree.buttonReturn(dynamicButton,0,false,error));
        ensure("post-build inherited local registry",events == std::vector<std::string>{"local"});
        const auto size = tree.size();
        ensure("retained construction context expires",!saved.construct("<view/>",*panel,error));
        ensure_equals("expired context creates nothing",tree.size(),size);
        resources.panelClasses["Recurse"] = [&](auto& widgets,const auto& params,const LLVKWidgetFactory::Construction& context,std::string& problem)
        {
            LLVKWidgetFactory::PanelInstance result;
            auto id = widgets.constructPanel(params.view.view,params.control,params.panel,problem);
            if (!id) return result;
            const auto nested = context.construct("<panel class='Recurse'/>",*id,problem);
            if (!nested) { std::string cleanup; widgets.erase(*id,cleanup); return result; }
            result.id = *id;
            return result;
        };
        LLVKWidgetFactory recursive({}, {}, {}, {}, resources, defaults);
        ensure("recursive constructor bounded",!recursive.construct(tree,"<panel class='Recurse'/>",0,error));
        ensure_equals("recursive partial construction cleaned",tree.size(),size);
    }

    template<> template<> void object::test<49>()
    {
        set_test_name("native panel constructor scopes preserve callback and named factory precedence");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = loadFont();
        unsigned initialized = 0;
        defaults.control.init.function = [&](auto,const LLSD&) { ++initialized; };
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = defaults.control.font;
        std::vector<std::string> events;
        const auto constructor = [&](std::string tag) -> LLVKWidgetFactory::PanelConstructor
        {
            return [&,tag](LLVKWidgetTree& widgets,const LLVKWidgetFactory::PanelDefaults& params,const LLVKWidgetFactory::Construction&,std::string& problem)
            {
                LLVKWidgetFactory::PanelInstance result;
                auto id = widgets.constructPanel(params.view.view,params.control,params.panel,problem);
                if (!id) return result;
                result.id = *id;
                result.callbacks = std::make_shared<LLVKWidgetFactory::Callbacks>();
                result.callbacks->actions["Action"] = [&,tag](auto,const LLSD&) { events.push_back(tag); };
                result.postBuild = [&,tag](auto& owner,auto target,const auto&,std::string&)
                { events.push_back("post:"+tag); return owner.postBuildControl(target); };
                events.push_back("construct:"+tag);
                return result;
            };
        };
        resources.panelClasses["Outer"] = [&,make = constructor("outer")](auto& owner,const auto& params,const auto& context,std::string& problem)
        {
            auto result = make(owner,params,context,problem);
            result.childFactories["slot"] = constructor("outer-slot");
            return result;
        };
        resources.panelClasses["Inner"] = [&,make = constructor("inner")](auto& owner,const auto& params,const auto& context,std::string& problem)
        {
            auto result = make(owner,params,context,problem);
            result.childFactories["slot"] = constructor("inner-slot");
            return result;
        };
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["Action"] = [&](auto,const LLSD&) { events.push_back("global"); };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources, defaults);
        auto root = factory.construct(tree,
            "<panel class='Outer'><button name='before' font='Font'><button.commit_callback function='Action'/></button>"
            "<panel class='Inner' name='inner'><button name='inside' font='Font'><button.commit_callback function='Action'/></button>"
            "<panel name='slot'/></panel><button name='after' font='Font'><button.commit_callback function='Action'/></button></panel>",0,error);
        ensure(error,root.has_value());
        ensure_equals("constructor-only classes initialize once",initialized,3u);
        ensure("outer named factory takes precedence",std::find(events.begin(),events.end(),"construct:outer-slot") != events.end());
        ensure("inner named factory not selected",std::find(events.begin(),events.end(),"construct:inner-slot") == events.end());
        ensure_equals("outer post-build last",events.back(),std::string("post:outer"));
        std::map<std::string,LLVKWidgetTree::Id> buttons;
        std::function<void(LLVKWidgetTree::Id)> collect = [&](auto id)
        {
            const auto* node = tree.get(id);
            if (node->button) buttons[node->params.name] = id;
            for (auto child : node->children) collect(child);
        };
        collect(*root);
        events.clear();
        for (const auto* name : {"before","inside","after"}) tree.buttonReturn(buttons.at(name),0,false,error);
        ensure("innermost callback wins and outer scope resumes",events == std::vector<std::string>{"outer","inner","outer"});
        const auto size = tree.size();
        ensure("missing scoped callback fails",!factory.construct(tree,
            "<panel class='Outer'><button font='Font'><button.commit_callback function='missing'/></button></panel>",0,error));
        ensure_equals("failed scoped construction rolls back",tree.size(),size);
        auto standalone = factory.construct(tree,"<button font='Font'><button.commit_callback function='Action'/></button>",0,error);
        ensure(error,standalone.has_value());
        events.clear();
        tree.buttonReturn(*standalone,0,false,error);
        ensure("scope does not leak",events == std::vector<std::string>{"global"});
    }

    template<> template<> void object::test<48>()
    {
        set_test_name("native named callbacks resolve at construction instead of default parsing");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::ButtonDefaults defaults;
        defaults.control.font = loadFont();
        LLVKWidgetFactory::Callbacks callbacks;
        std::string argument;
        callbacks.actions["known"] = [&](auto,const LLSD& value) { argument = value.asString(); };
        LLVKWidgetFactory factory({}, {}, defaults, callbacks);
        ensure("unresolved names retained in defaults",factory.loadDefaults(tree,
            "<button><button.commit_callback function='missing'/></button>",error));
        ensure_equals("default parsing creates nothing",tree.size(),std::size_t(0));
        ensure("missing handler rejects actual construction",!factory.construct(tree,"<button/>",0,error));
        ensure_equals("failed resolution creates nothing",tree.size(),std::size_t(0));
        ensure("known callback overrides deferred name",factory.loadDefaults(tree,
            "<button><button.commit_callback function='known' parameter='resolved'/></button>",error));
        auto button = factory.construct(tree,"<button/>",0,error);
        ensure(error,button.has_value());
        tree.buttonReturn(*button,0,false,error);
        ensure_equals("resolved callable and fixed argument",argument,std::string("resolved"));
        defaults.control.commit.functionName = "missing";
        defaults.control.commit.function = [&](auto,const LLSD&) { argument = "direct"; };
        LLVKWidgetFactory direct({}, {}, defaults, {});
        button = direct.construct(tree,"<button/>",0,error);
        ensure(error,button.has_value());
        tree.buttonReturn(*button,0,false,error);
        ensure_equals("direct function precedes name",argument,std::string("direct"));
    }

    template<> template<> void object::test<47>()
    {
        set_test_name("native referenced panel builds file children before outer initialization and overrides");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = loadFont();
        resources.declarations["panel.xml"] =
            "<panel width='100' height='80' background_visible='true'><string name='message'>reference</string>"
            "<button font='Font' name='referenced' layout='topleft' left='0' top='0' width='20' height='20' follows='all'>"
            "<button.init_callback function='reference_child'/></button></panel>";
        resources.declarations["cycle.xml"] = "<panel><panel filename='cycle.xml'/></panel>";
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = resources.fonts.at("Font");
        std::vector<std::string> events;
        LLVKWidgetTree::Id panelId = 0;
        LLVKWidgetTree::Id referencedChild = 0;
        defaults.control.init.function = [&](auto id,const LLSD&) { panelId = id; };
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["reference_child"] = [&](auto id,const LLSD&)
        {
            referencedChild = id;
            ensure_equals("reference dimensions before children",tree.get(panelId)->params.rect.right-tree.get(panelId)->params.rect.left,100);
            ensure_equals("filename visible to referenced children",tree.get(panelId)->panel->params.filename,std::string("panel.xml"));
            ensure("reference strings not installed yet",tree.get(panelId)->panel->strings.empty());
            events.push_back("reference child");
        };
        callbacks.actions["outer_init"] = [&](auto id,const LLSD&)
        {
            ensure("referenced child exists before outer init",tree.get(referencedChild) != nullptr);
            ensure_equals("outer init still at reference width",tree.get(id)->params.rect.right-tree.get(id)->params.rect.left,100);
            events.push_back("outer init");
        };
        callbacks.actions["outer_child"] = [&](auto,const LLSD&)
        {
            ensure_equals("outer child uses final width",tree.get(panelId)->params.rect.right-tree.get(panelId)->params.rect.left,200);
            events.push_back("outer child");
        };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources, defaults);
        auto panel = factory.construct(tree,
            "<panel filename='panel.xml' width='200' height='120' background_visible='false'>"
            "<panel.init_callback function='outer_init'/><string name='message'>override</string>"
            "<button font='Font'><button.init_callback function='outer_child'/></button></panel>",0,error);
        ensure(error,panel.has_value());
        ensure("construction order",events == std::vector<std::string>{"reference child","outer init","outer child"});
        ensure("outer scalar overrides reference",!tree.get(*panel)->panel->params.backgroundVisible);
        auto message = tree.panelString(*panel,"message",{},error);
        ensure(error,message.has_value());
        ensure_equals("outer string overrides reference",*message,std::string("override"));
        ensure_equals("referenced child follows resize",tree.get(referencedChild)->params.rect.right-tree.get(referencedChild)->params.rect.left,120);
        ensure_equals("both sets of children retained",tree.get(*panel)->children.size(),std::size_t(2));
        const auto size = tree.size();
        ensure("missing file rejects",!factory.construct(tree,"<panel filename='missing.xml'/>",0,error));
        ensure_equals("missing file no orphan default panel",tree.size(),size);
        ensure("cycle rejects",!factory.construct(tree,"<panel filename='cycle.xml'/>",0,error));
        ensure_equals("cycle removes every partial nested panel",tree.size(),size);
    }

    template<> template<> void object::test<46>()
    {
        set_test_name("native panel string declarations preserve body and attribute sanitation");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = loadFont();
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = resources.fonts.at("Font");
        LLVKWidgetTree::Id owner = 0;
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["init"] = [&](auto id,const LLSD&)
        { owner = id; ensure("declared strings not yet installed",!tree.get(id)->panel->strings.contains("trimmed")); };
        callbacks.actions["child"] = [&](auto,const LLSD&)
        { ensure("panel strings before children",tree.get(owner)->panel->strings.contains("trimmed")); };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources, defaults);
        auto panel = factory.construct(tree,
            "<panel><panel.init_callback function='init'/>"
            "<string name='trimmed'> \n Hello [NAME] &amp; viewer \n </string>"
            "<panel.string name='attribute' value='  preserved  '/>"
            "<string name='overridden' value='attribute'>body</string>"
            "<string name='quoted'> &quot;  padded  &quot; </string>"
            "<string name='lines'>&quot;one&quot; &quot;two&quot;</string>"
            "<string name='escaped'>&quot;a\\&quot;b&quot;</string>"
            "<string name='cr'>a&#13;b</string>"
            "<string name='empty' value=''/><string name='duplicate' value='first'/>"
            "<string name='duplicate' value='last'/>"
            "<button font='Font'><button.init_callback function='child'/></button></panel>",0,error);
        ensure(error,panel.has_value());
        const auto lookup = [&](const std::string& name)
        {
            const auto value = tree.panelString(*panel,name,{{"NAME","native"}},error);
            ensure(error,value.has_value());
            return *value;
        };
        ensure_equals("trimmed and substituted",lookup("trimmed"),std::string("Hello native & viewer"));
        ensure_equals("attribute keeps whitespace",lookup("attribute"),std::string("  preserved  "));
        ensure_equals("body overrides attribute",lookup("overridden"),std::string("body"));
        ensure_equals("quoted keeps whitespace",lookup("quoted"),std::string("  padded  "));
        ensure_equals("quoted multiline trailing newline",lookup("lines"),std::string("one\ntwo\n"));
        ensure_equals("quoted escapes",lookup("escaped"),std::string("a\"b"));
        ensure_equals("embedded CR removed",lookup("cr"),std::string("ab"));
        ensure("explicit empty string",lookup("empty").empty());
        ensure_equals("last duplicate wins",lookup("duplicate"),std::string("last"));
        const auto size = tree.size();
        ensure("nested string elements rejected",!factory.construct(tree,"<panel><string name='bad'><view/></string></panel>",0,error));
        ensure_equals("bad declaration creates nothing",tree.size(),size);
    }

    template<> template<> void object::test<45>()
    {
        set_test_name("native panel XML performs default init before declared init and retains signal connections");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::PanelDefaults defaults;
        defaults.control.font = loadFont();
        defaults.view.geometry.width = {10,true};
        defaults.view.geometry.height = {20,true};
        std::vector<std::string> events;
        LLVKWidgetTree::Id original = 0;
        defaults.control.init.function = [&](auto id,const LLSD&)
        {
            original = id;
            ensure_equals("default init name",tree.get(id)->params.name,std::string("panel"));
            ensure_equals("default init geometry",tree.get(id)->params.rect.right-tree.get(id)->params.rect.left,10);
            events.push_back("default init");
        };
        defaults.control.commit.function = [&](auto,const LLSD&) { events.push_back("default commit"); };
        defaults.control.validate.function = [&](auto,const LLSD&) { events.push_back("default validate"); return false; };
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["declared_init"] = [&](auto id,const LLSD&)
        {
            ensure_equals("same owner reinitialized",id,original);
            ensure_equals("declared name before init",tree.get(id)->params.name,std::string("declared"));
            ensure_equals("declared rectangle not applied before callback",tree.get(id)->params.rect.right-tree.get(id)->params.rect.left,10);
            events.push_back("declared init");
        };
        callbacks.actions["declared_commit"] = [&](auto,const LLSD&) { events.push_back("declared commit"); };
        callbacks.predicates["declared_validate"] = [&](auto,const LLSD&) { events.push_back("declared validate"); return true; };
        LLVKWidgetFactory factory({}, {}, {}, callbacks, {}, defaults);
        auto panel = factory.construct(tree,
            "<panel name='declared' width='80' height='40'><panel.init_callback function='declared_init'/>"
            "<panel.commit_callback function='declared_commit'/><panel.validate_callback function='declared_validate'/></panel>",0,error);
        ensure(error,panel.has_value());
        ensure("both initialization phases",events == std::vector<std::string>{"default init","declared init"});
        ensure_equals("declared geometry after init",tree.get(*panel)->params.rect.right-tree.get(*panel)->params.rect.left,80);
        events.clear();
        tree.commit(*panel);
        ensure("commit connections retained",events == std::vector<std::string>{"default commit","declared commit"});
        events.clear();
        ensure("combined validation false",!tree.validate(*panel));
        ensure("all validators evaluated",events == std::vector<std::string>{"default validate","declared validate"});
    }

    template<> template<> void object::test<44>()
    {
        set_test_name("native panel XML keeps constructor state and attaches after child initialization");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.visible = false;
        view.rect = {0,0,300,200};
        auto parent = tree.create(view,0,error);
        ensure(error,parent.has_value());
        auto prior = tree.create({},*parent,error);
        ensure(error,prior.has_value());
        tree.reparent(*prior,*parent,false,7,error);
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = loadFont();
        LLVKWidgetTree::Id panelId = 0;
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["panel_init"] = [&](auto id,const LLSD&)
        {
            panelId = id;
            ensure("constructor default before init",!tree.get(id)->panel->params.backgroundVisible);
            ensure_equals("no declared border before init",tree.get(id)->panel->border,LLVKWidgetTree::Id(0));
            ensure_equals("panel not attached",tree.get(id)->parent,LLVKWidgetTree::Id(0));
        };
        callbacks.actions["child_init"] = [&](auto,const LLSD&)
        {
            ensure_equals("panel remains detached while constructing children",tree.get(panelId)->parent,LLVKWidgetTree::Id(0));
            ensure("external hidden ancestor not connected yet",tree.visibleInChain(panelId));
            ensure("declared panel state installed before children",tree.get(panelId)->panel->params.backgroundVisible);
            ensure("declared border installed before children",tree.get(tree.get(panelId)->panel->border) != nullptr);
        };
        LLVKWidgetFactory::PanelDefaults panelDefaults;
        panelDefaults.control.font = resources.fonts.at("Font");
        LLVKWidgetFactory factory({}, {}, {}, callbacks, resources, panelDefaults);
        auto panel = factory.construct(tree,
            "<panel font='Font' width='100' height='60' border='true' background_visible='true'>"
            "<panel.init_callback function='panel_init'/><button font='Font' width='30' height='20'>"
            "<button.init_callback function='child_init'/></button></panel>",*parent,error);
        ensure(error,panel.has_value());
        ensure_equals("parent attached after children",tree.get(*panel)->parent,*parent);
        ensure("now in hidden hierarchy",!tree.visibleInChain(*panel));
        ensure_equals("panel inherits parent last tab group",tree.get(*panel)->params.tabGroup.value_or(-1),7);
        ensure("actual panel component",tree.get(*panel)->panel.has_value());
        const auto size = tree.size();
        ensure("unknown custom panel is not silently generic",!factory.construct(tree,"<panel font='Font' class='missing'/>",*parent,error));
        ensure_equals("rejection leaves tree unchanged",tree.size(),size);
    }

    template<> template<> void object::test<43>()
    {
        set_test_name("native panel visibility callbacks are descendant-first and safe during settings updates");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("show",LLSD(true),LLVKWidgetTree::SettingType::Boolean);
        LLVKControl::Params control;
        control.font = loadFont();
        std::vector<std::string> events;
        LLVKPanel::Params panel;
        panel.visible.function = [&](auto,const LLSD& value) { events.push_back(value.asBoolean() ? "root on" : "root off"); };
        control.init.function = [&](auto id,const LLSD&)
        { tree.setVisible(id,false); tree.setVisible(id,true); };
        auto root = tree.createPanel({},control,panel,0,error);
        ensure(error,root.has_value());
        ensure("visible callback installed after control init",events.empty());
        control.init = {};
        panel.visible.function = [&](auto,const LLSD& value) { events.push_back(value.asBoolean() ? "child on" : "child off"); };
        auto child = tree.createPanel({},control,panel,*root,error);
        ensure(error,child.has_value());
        tree.setVisible(*root,false);
        ensure("descendant first",events == std::vector<std::string>{"child off","root off"});
        events.clear();
        tree.setVisible(*child,false);
        ensure("hidden hierarchy suppresses notification",events.empty());
        tree.setVisible(*root,true);
        ensure("locally hidden child excluded",events == std::vector<std::string>{"root on"});
        events.clear();
        control.visibleSetting = "show";
        panel.visible.function = [&](auto id,const LLSD& value)
        {
            if (!value.asBoolean()) { std::string ignored; tree.eraseControl(id,ignored); }
        };
        auto first = tree.createPanel({},control,panel,*root,error);
        auto second = tree.createPanel({},control,panel,*root,error);
        ensure(error,first.has_value() && second.has_value());
        ensure("setting update tolerates subscriber deletion",tree.updateSetting("show",LLSD(false)));
        ensure("both deleting subscribers processed",!tree.get(*first) && !tree.get(*second));
        ensure("unrelated tree retained",tree.get(*root) && tree.get(*child));
    }

    template<> template<> void object::test<42>()
    {
        set_test_name("native typed panel border replacement and strings follow init ordering");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKPanel::Params panel;
        panel.hasBorder = true;
        panel.strings["greeting"] = "Hello [NAME]";
        LLVKWidgetTree::Id constructorBorder = 0;
        control.init.function = [&](auto id,const LLSD&)
        {
            constructorBorder = tree.get(id)->panel->border;
            ensure("border exists before typed panel init",tree.get(constructorBorder)->border.has_value());
            ensure("panel strings installed after control init",tree.get(id)->panel->strings.empty());
        };
        LLVKWidgetTree::Params view;
        view.rect = {5,6,105,66};
        auto id = tree.createPanel(view,control,panel,0,error);
        ensure(error,id.has_value());
        const auto border = tree.get(*id)->panel->border;
        ensure("panel init replaces constructor border",border != constructorBorder && !tree.get(constructorBorder));
        ensure("border fills local rect",tree.get(border)->params.rect == LLVKWidgetTree::Rect{0,0,100,60});
        ensure("native badge holder",tree.get(*id)->acceptsBadge);
        auto greeting = tree.panelString(*id,"greeting",{{"NAME","viewer"}},error);
        ensure(error,greeting.has_value());
        ensure_equals("local string formatting",*greeting,std::string("Hello viewer"));
        ensure("native panel resize",tree.reshape(*id,120,80,error));
        ensure("border follows panel",tree.get(border)->params.rect == LLVKWidgetTree::Rect{0,0,120,80});
        ensure("border removal",tree.removePanelBorder(*id,error));
        ensure("border owner cleared",!tree.get(border) && !tree.get(*id)->panel->border);
        ensure("missing string explicit",!tree.panelString(*id,"missing",{},error));
        tree.eraseControl(*id,error);
        ensure_equals("panel ownership retired",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<41>()
    {
        set_test_name("native default templates preserve overlay fields and provided image identity");
        LLVKWidgetTree tree;
        std::string error;
        tree.registerImage(image("base"));
        tree.registerImage(image("disabled"));
        tree.registerImage(image("custom"));
        LLVKWidgetFactory::Resources resources;
        resources.fonts["Font"] = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        ensure("native button template",factory.loadDefaults(tree,
            "<button font='Font' width='80' height='23' image_unselected='base' image_disabled='disabled' pad_left='4'/>",error));
        ensure("ordered overlay",factory.loadDefaults(tree,"<button pad_left='8' label='Default'/>",error));
        ensure_equals("template parsing creates no widgets",tree.size(),std::size_t(0));
        auto button = factory.construct(tree,"<button/>",0,error);
        ensure(error,button.has_value());
        ensure_equals("overlay padding",tree.get(*button)->button->leftPad,8);
        ensure_equals("base geometry retained",tree.get(*button)->params.rect.right-tree.get(*button)->params.rect.left,80);
        ensure("default image identities retained",tree.get(*button)->button->images.disabled == tree.findImage("disabled"));
        ensure("unchanged default image does not fade",!tree.get(*button)->button->fadeWhenDisabled);
        auto custom = factory.construct(tree,"<button image_unselected='custom'/>",0,error);
        ensure(error,custom.has_value());
        ensure("custom image uses disabled fallback",tree.get(*custom)->button->images.disabled == tree.findImage("custom"));
        ensure("custom image enables fade",tree.get(*custom)->button->fadeWhenDisabled);
        ensure("invalid template no publication",!factory.loadDefaults(tree,"<button pad_left='bad'/>",error));
        button = factory.construct(tree,"<button/>",0,error);
        ensure(error,button.has_value());
        ensure_equals("prior defaults intact",tree.get(*button)->button->leftPad,8);
        ensure("badge defaults load",factory.loadDefaults(tree,"<badge font='Font'/>",error));
        ensure("button badge defaults load",factory.loadDefaults(tree,"<button><button.badge label='new'/></button>",error));
        button = factory.construct(tree,"<button/>",0,error);
        ensure(error,button.has_value());
        const auto badge = tree.get(*button)->button->badge;
        ensure("provided badge default constructs child",tree.get(badge) != nullptr);
        ensure("provided badge text",tree.get(badge)->badge->params.label == U"new");
    }

    template<> template<> void object::test<40>()
    {
        set_test_name("native factory consumes packaged color and button declarations");
        std::string error;
        std::vector<std::string> warnings;
        auto colors = std::make_shared<LLVKColorTable>();
        std::ifstream colorFile(std::string(LLVK_WIDGET_SKIN_FIXTURE)+"/colors.xml",std::ios::binary);
        ensure("packaged color fixture",colorFile.good());
        const std::string colorXml{std::istreambuf_iterator<char>(colorFile),std::istreambuf_iterator<char>()};
        const bool loaded = colors->load(colorXml,LLVKColorTable::Layer::Loaded,warnings,error);
        ensure(error,loaded);
        ensure("button label color resolves",colors->find("ButtonLabelColor").has_value());
        ensure("badge label color resolves",colors->find("BadgeLabelColor").has_value());
        LLVKWidgetTree tree;
        for (const auto& name : {"PushButton_Off","PushButton_Selected","PushButton_Selected_Disabled","PushButton_Disabled"})
            tree.registerImage(image(name));
        LLVKWidgetFactory::Resources resources;
        resources.colors = colors;
        resources.fonts["SansSerifSmall"] = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        std::ifstream buttonFile(std::string(LLVK_WIDGET_SKIN_FIXTURE)+"/xui/en/widgets/button.xml",std::ios::binary);
        ensure("packaged button fixture",buttonFile.good());
        const std::string buttonXml{std::istreambuf_iterator<char>(buttonFile),std::istreambuf_iterator<char>()};
        auto button = factory.construct(tree,buttonXml,0,error);
        ensure(error,button.has_value());
        ensure("packaged label uses native color slot",tree.get(*button)->button->params.labelColor == *colors->find("ButtonLabelColor"));
        ensure_equals("packaged height",tree.get(*button)->params.rect.top-tree.get(*button)->params.rect.bottom,23);
        ensure_equals("packaged flash count",tree.get(*button)->button->params.flashCount,8);
        ensure_equals("packaged hover glow",tree.get(*button)->button->params.hoverGlow,0.25f);
        ensure("native image retained",tree.get(*button)->button->images.unselected == tree.findImage("PushButton_Off"));
    }

    template<> template<> void object::test<39>()
    {
        set_test_name("native color XML resolves copied aliases layers cycles and malformed input");
        LLVKColorTable table;
        std::string error;
        std::vector<std::string> warnings;
        ensure("load forward chain",table.load("<colors><color name='A' reference='B'/><color name='B' reference='Base'/>"
            "<color name='Base' value='0.1 0.2 0.3 1'/></colors>",LLVKColorTable::Layer::Loaded,warnings,error));
        ensure("valid graph no warnings",warnings.empty());
        const auto alias = *table.find("A");
        const auto base = *table.find("Base");
        ensure("aliases copy values",alias.get() == base.get());
        ensure("aliases are separate parameter identities",!(alias == base));
        table.set("Base",{1,0,0,1});
        ensure("alias not a live reference to base",alias.get() == LLVKColor::Value{0.1f,0.2f,0.3f,1});
        ensure("user aliases resolve against loaded table",table.load(
            "<colors><color name='UserAlias' reference='Base'/><color name='OnlyUser' value='0 0 1 1'/></colors>",
            LLVKColorTable::Layer::User,warnings,error));
        ensure("loaded base used rather than user override",table.find("UserAlias")->get() == alias.get());
        ensure("cycles do not reject unrelated literals",table.load(
            "<colors><color name='CycleA' reference='CycleB'/><color name='CycleB' reference='CycleA'/>"
            "<color name='MissingAlias' reference='Missing'/><color name='Valid' value='1 1 0 1'/></colors>",
            LLVKColorTable::Layer::Loaded,warnings,error));
        ensure_equals("cycle and missing diagnostics",warnings.size(),std::size_t(2));
        ensure("unresolved colors omitted",!table.find("CycleA") && !table.find("MissingAlias"));
        ensure("unrelated literal published",table.find("Valid").has_value());
        ensure("malformed load atomic",!table.load("<colors><color name='A' value='1 1 1 1'/><color name='bad' value='1 1'></colors>",
            LLVKColorTable::Layer::Loaded,warnings,error));
        ensure("old alias retained after failure",alias.get() == LLVKColor::Value{0.1f,0.2f,0.3f,1});
        ensure("invalid value skipped while valid RGB survives",table.load(
            "<colors><color name='InvalidValue' value='not_a_reference'/><color name='RGB' value='0 1 0'/></colors>",
            LLVKColorTable::Layer::Loaded,warnings,error));
        ensure_equals("invalid value warning",warnings.size(),std::size_t(1));
        ensure("invalid named value not aliased",!table.find("InvalidValue"));
        ensure("RGB defaults alpha",table.find("RGB")->get() == LLVKColor::Value{0,1,0,1});
        ensure("nonfinite component diagnosed without publication",table.load(
            "<colors><color name='BadAlpha' value='1 0 0 nan'/><color name='RGBTail' value='1 0 0 invalid'/></colors>",
            LLVKColorTable::Layer::Loaded,warnings,error));
        ensure("nonfinite alpha omitted",!table.find("BadAlpha"));
        ensure("failed alpha parse preserves default",table.find("RGBTail")->get() == LLVKColor::Value{1,0,0,1});
        ensure("DTD rejected",!table.load("<!DOCTYPE colors [<!ENTITY c '1 1 1 1'>]><colors/>",
            LLVKColorTable::Layer::Loaded,warnings,error));
    }

    template<> template<> void object::test<38>()
    {
        set_test_name("native color user overrides retain references and loaded defaults");
        LLVKColorTable table;
        table.define("Color",{0.1f,0.2f,0.3f,1});
        const auto reference = *table.find("Color");
        ensure("loaded is default",table.isDefault("Color"));
        table.set("Color",{1,1,0,0.5f});
        ensure("override keeps prior reference identity",reference == *table.find("Color"));
        ensure("override is not default",!table.isDefault("Color"));
        ensure("reset finds original",table.resetToDefault("Color"));
        ensure("reset preserves identity",reference == *table.find("Color"));
        ensure("reference sees original",reference.get() == LLVKColor::Value{0.1f,0.2f,0.3f,1});
        ensure("now default",table.isDefault("Color"));
        table.set("UserOnly",{0,0,1,1});
        ensure("source user-only default query",table.isDefault("UserOnly"));
        ensure("no absent reset",!table.resetToDefault("UserOnly"));
        ensure("unknown not default",!table.isDefault("Missing"));
        table.clear();
        ensure("clear preserves names and references",reference == *table.find("Color"));
        ensure("clear recolors magenta",reference.get() == LLVKColor::Value{1,0,1,1});
    }

    template<> template<> void object::test<37>()
    {
        set_test_name("native factory named fonts and live colors survive construction");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::Resources resources;
        resources.colors = std::make_shared<LLVKColorTable>();
        resources.colors->define("Tint",{1,0,0,0.8f});
        resources.fonts["SansSerifSmall"] = loadFont();
        LLVKWidgetFactory factory({}, {}, {}, {}, resources);
        auto icon = factory.construct(tree,"<icon font='SansSerifSmall' color='Tint' width='10' height='10'/>",0,error);
        ensure(error,icon.has_value());
        auto before = tree.prepareIcon(*icon,0.5f,1.f,error);
        ensure(error,before.has_value());
        resources.colors->set("Tint",{0,1,0,0.6f});
        auto after = tree.prepareIcon(*icon,0.5f,1.f,error);
        ensure(error,after.has_value());
        ensure("draw snapshots remain independent",before->color == LLVKColor::Value{1,0,0,0.4f});
        ensure("new draw sees live color",after->color == LLVKColor::Value{0,1,0,0.3f});
        auto button = factory.construct(tree,
            "<button font='SansSerifSmall' label_color='Tint' image_color='0.25 0.5 0.75 1' hover_glow_amount='0.25'/>",0,error);
        ensure(error,button.has_value());
        ensure("button retains color reference",tree.get(*button)->button->params.labelColor == *resources.colors->find("Tint"));
        ensure("literal remains literal",!tree.get(*button)->button->params.imageColor.isReference());
        ensure("resolved native font",tree.get(*button)->control->params.font == resources.fonts.at("SansSerifSmall"));
        ensure("unknown font explicit",!factory.construct(tree,"<button font='missing'/>",0,error));
        ensure("unknown theme color explicit",!factory.construct(tree,"<icon font='SansSerifSmall' color='missing'/>",0,error));
    }

    template<> template<> void object::test<36>()
    {
        set_test_name("native color slots preserve live references and parameter identity");
        LLVKColor retained;
        {
            LLVKColorTable table;
            ensure("define color",table.define("Label",{1,0,0,1}));
            auto color = table.find("Label");
            ensure("resolved color",color.has_value());
            retained = *color;
            ensure("live reference",retained.isReference());
            ensure("same named color same identity",retained == *table.find("Label"));
            ensure("literal same value not parameter-equivalent",!(retained == LLVKColor(1,0,0,1)));
            ensure("distinct named entry",table.define("Other",{1,0,0,1}));
            ensure("equal values distinct references",!(retained == *table.find("Other")));
            ensure("update color",table.set("Label",{0,1,0,0.5f}));
            ensure("existing references refresh",retained.get() == LLVKColor::Value{0,1,0,0.5f});
            ensure("unknown explicit",!table.find("Missing"));
            ensure("nonfinite update rejected",!table.set("Label",{0,0,std::numeric_limits<float>::infinity(),1}));
            ensure("rejected update unchanged",retained.get() == LLVKColor::Value{0,1,0,0.5f});
        }
        ensure("reference retains native slot lifetime",retained.get() == LLVKColor::Value{0,1,0,0.5f});
        ensure("literals compare by value",LLVKColor(1,0,0,1) == LLVKColor(1,0,0,1));
    }

    template<> template<> void object::test<35>()
    {
        set_test_name("native checkbox reshape colors tentative and draw predicate state");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKWidgetTree::CheckBoxConstruction checkbox;
        checkbox.labelControl.font = checkbox.buttonControl.font = control.font;
        checkbox.labelView.rect = {20,3,20,3};
        checkbox.buttonView.rect = {2,1,15,14};
        checkbox.button.toggle = true;
        checkbox.label = "Long label with several words";
        checkbox.wrap = LLVKWidgetTree::CheckBoxWrap::Down;
        checkbox.labelText.textColor = {1,0,0,1};
        checkbox.labelText.readOnlyColor = {0,1,0,1};
        checkbox.onCheck.function = [](auto,const LLSD&) { return true; };
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,30};
        auto id = tree.createCheckBox(view,control,checkbox,0,error);
        ensure(error,id.has_value());
        const auto label = tree.get(*id)->checkBox->label;
        const auto button = tree.get(*id)->checkBox->button;
        const auto labelTop = tree.get(label)->params.rect.top;
        const auto oldWidth = tree.get(button)->params.rect.right-tree.get(button)->params.rect.left;
        ensure("narrow reshape",tree.reshape(*id,50,30,error));
        ensure_equals("wrap down preserves label top",tree.get(label)->params.rect.top,labelTop);
        ensure("click area never shrinks",tree.get(button)->params.rect.right-tree.get(button)->params.rect.left >= oldWidth);
        tree.setEnabled(*id,false);
        ensure("disabled label color",tree.get(label)->plainText->params.textColor == checkbox.labelText.readOnlyColor);
        tree.setEnabled(*id,true);
        ensure("enabled label color restored",tree.get(label)->plainText->params.textColor == checkbox.labelText.textColor);
        tree.setTentative(*id,true);
        ensure("tentative forwarded",tree.get(button)->control->tentative);
        tree.commit(*id);
        ensure("commit clears tentative",!tree.tentative(*id));
        ensure("draw predicate refresh",tree.refreshCheckBox(*id));
        ensure("predicate updates actual button",tree.value(*id).asBoolean());
        ensure("label updates refit",tree.setCheckBoxLabel(*id,"[TEXT]",error));
        ensure("label arguments refit",tree.setCheckBoxLabelArgument(*id,"TEXT","Updated",error));
        ensure("resolved new label",tree.get(label)->plainText->text == U"Updated");
        const auto before = tree.get(*id)->params.rect;
        ensure("invalid narrow label width explicit",!tree.reshape(*id,1,30,error));
        ensure("failed reshape atomic",tree.get(*id)->params.rect == before);
    }

    template<> template<> void object::test<34>()
    {
        set_test_name("native checkbox binding lives on embedded button and supports rebinding");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("first",LLSD(true),LLVKWidgetTree::SettingType::Boolean);
        tree.defineSetting("second",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        LLVKControl::Params control;
        control.font = loadFont();
        control.valueSetting = "first";
        LLVKWidgetTree::CheckBoxConstruction checkbox;
        checkbox.labelControl.font = control.font;
        checkbox.buttonControl.font = control.font;
        checkbox.button.toggle = true;
        checkbox.buttonView.rect = {0,0,13,13};
        auto id = tree.createCheckBox({},control,checkbox,0,error);
        ensure(error,id.has_value());
        const auto button = tree.get(*id)->checkBox->button;
        ensure("initial setting overrides constructor false",tree.value(*id).asBoolean());
        ensure("outer checkbox has no value binding",!tree.get(*id)->control->params.valueSetting);
        ensure("embedded button bound",tree.get(button)->control->params.valueSetting == "first");
        ensure("rebind",tree.bindValueSetting(*id,"second"));
        ensure("rebind loads new value",!tree.value(*id).asBoolean());
        tree.updateSetting("first",LLSD(false));
        tree.updateSetting("first",LLSD(true));
        ensure("old binding disconnected",!tree.value(*id).asBoolean());
        tree.setEnabled(*id,false);
        ensure("programmatic button activation",tree.buttonUnicode(button,U' ',false,error));
        control.valueSetting = "second";
        auto observer = tree.createControl({},control,0,error);
        ensure(error,observer.has_value());
        ensure("button toggle wrote before disabled checkbox commit gate",tree.value(*observer).asBoolean());
        ensure("empty name does not disconnect",tree.bindValueSetting(*id,""));
        ensure("binding unchanged",tree.get(button)->control->params.valueSetting == "second");
        ensure("missing name reports failure",!tree.bindValueSetting(*id,"missing"));
        ensure("missing name disconnects existing binding",!tree.get(button)->control->params.valueSetting);
        ensure("value retained on missing name",tree.value(*id).asBoolean());
    }

    template<> template<> void object::test<33>()
    {
        set_test_name("native checkbox constructs real label and button with forwarded value");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("checked",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        LLVKControl::Params control;
        control.font = loadFont();
        control.valueSetting = "checked";
        LLVKWidgetTree::CheckBoxConstruction checkbox;
        checkbox.labelControl.font = control.font;
        checkbox.buttonControl.font = control.font;
        checkbox.labelView.rect = {20,3,20,3};
        checkbox.labelView.mouseOpaque = false;
        checkbox.buttonView.rect = {2,1,15,14};
        checkbox.button.toggle = true;
        checkbox.label = "Choice";
        bool committed = false;
        control.commit.function = [&](auto id,const LLSD& value)
        {
            ensure("commit exposes child value",value.asBoolean() && tree.value(id).asBoolean());
            committed = true;
        };
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            ensure("both children before init",tree.get(node->checkBox->label) && tree.get(node->checkBox->button));
            ensure("checkbox bounds enabled",node->params.useBoundingRect);
        };
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,20};
        auto id = tree.createCheckBox(view,control,checkbox,0,error);
        ensure(error,id.has_value());
        const auto button = tree.get(*id)->checkBox->button;
        const auto label = tree.get(*id)->checkBox->label;
        ensure_equals("button above label",tree.get(*id)->children.front(),button);
        ensure("real native label",tree.get(label)->plainText->text == U"Choice");
        ensure("click area covers label",tree.get(button)->params.rect.right >= tree.get(label)->params.rect.right);
        ensure("embedded Return disabled",!tree.buttonReturn(button,0,false,error));
        ensure("embedded space activates checkbox",tree.buttonUnicode(button,U' ',false,error));
        ensure("checkbox committed",committed);
        ensure("dirty forwarded",tree.dirty(*id));
        tree.resetDirty(*id);
        ensure("dirty reset forwarded",!tree.dirty(*id));
        tree.updateSetting("checked",LLSD(false));
        ensure("binding forwards to button",!tree.value(*id).asBoolean());
        tree.setEnabled(*id,false);
        committed = false;
        tree.commit(*id);
        ensure("disabled checkbox does not commit",!committed);
        tree.eraseControl(*id,error);
        ensure_equals("all child owners retired",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<32>()
    {
        set_test_name("native text context refresh and constructor rejection remain transactional");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("enabled",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        LLVKControl::Params control;
        control.font = loadFont();
        control.enabledSetting = "enabled";
        control.initialValue = LLSD("L$[N]");
        control.init.function = [&](auto id,const LLSD&) { ensure("binding drives readonly before init",tree.get(id)->plainText->readOnly); };
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,30};
        auto id = tree.createPlainText(view,control,{},0,error);
        ensure(error,id.has_value());
        tree.updateSetting("enabled",LLSD(true));
        ensure("binding updates readonly",!tree.get(*id)->plainText->readOnly);
        LLVKLabel::Context context;
        context.currency = "R$";
        context.defaults["N"] = "42";
        tree.setLabelContext(context);
        ensure_equals("context updates plain value",tree.get(*id)->control->value.asString(),std::string("R$42"));
        context.defaults["N"] = std::string("bad\0value",9);
        bool rejected = false;
        try { tree.setLabelContext(context); }
        catch (const std::invalid_argument&) { rejected = true; }
        ensure("bad context rejected",rejected);
        ensure_equals("old value retained",tree.get(*id)->control->value.asString(),std::string("R$42"));
        tree.setPlainText(*id,"L$[N]",error);
        ensure_equals("old context retained",tree.get(*id)->control->value.asString(),std::string("R$42"));
        const auto size = tree.size();
        control.initialValue = LLSD(std::string("bad\0value",9));
        control.init = {};
        ensure("bad initial text fails constructor",!tree.createPlainText(view,control,{},0,error));
        ensure_equals("failed constructor removes document too",tree.size(),size);
    }

    template<> template<> void object::test<31>()
    {
        set_test_name("native plain text construction document ownership init and byte limits");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,100,40};
        view.enabled = false;
        LLVKControl::Params control;
        control.font = loadFont();
        control.initialValue = LLSD("Caf\xc3\xa9!");
        bool initialized = false;
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            ensure("real document before init",tree.get(node->plainText->document) != nullptr);
            ensure_equals("initial value UTF-8 truncates whole scalar",node->control->value.asString(),std::string("Caf"));
            ensure("dirty observable in init",node->control->dirty);
            ensure("enabled initially implies readonly",node->plainText->readOnly);
            initialized = true;
        };
        LLVKPlainControl::Params text;
        text.maximumBytes = 4;
        text.readOnly = false;
        auto id = tree.createPlainText(view,control,text,0,error);
        ensure(error,id.has_value());
        ensure("init called",initialized);
        ensure("dirty reset after init",!tree.get(*id)->control->dirty);
        ensure("explicit readonly restored after init",!tree.get(*id)->plainText->readOnly);
        ensure("plain text update",tree.setPlainText(*id,"a\r\nb",error));
        ensure("CR removed",tree.get(*id)->plainText->text == U"a\nb");
        ensure("text reflow",tree.reflowPlainText(*id,error));
        ensure_equals("two lines",tree.get(*id)->plainText->layout->lines.size(),std::size_t(2));
        ensure("fit to text",tree.fitPlainText(*id,error));
        ensure("reshape invalidates cached layout",!tree.get(*id)->plainText->layout.has_value());
        tree.setEnabled(*id,false);
        ensure("subsequent enabled change uses readonly contract",tree.get(*id)->plainText->readOnly);
        ensure("new argument",tree.setPlainTextArgument(*id,"A","xyz",error));
        ensure("new original",tree.setPlainText(*id,"[A]",error));
        ensure("owned text arguments",tree.get(*id)->plainText->text == U"xyz");
        const auto documentId = tree.get(*id)->plainText->document;
        tree.eraseControl(*id,error);
        ensure("document retired with control",!tree.get(documentId));
        ensure_equals("tree empty",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<30>()
    {
        set_test_name("native plain document placement and fit preserve source padding and alignment");
        const auto font = loadFont();
        std::string error;
        LLVKPlainTextLayout::Options options;
        options.width = 100;
        options.horizontalPadding = 2;
        const auto height = static_cast<std::int32_t>(std::ceil(font->metrics().ascender)+std::ceil(font->metrics().descender));
        auto document = LLVKPlainTextLayout::document(U"X",*font,options,100,3,LLVKFont::VerticalAlign::Top,error);
        ensure(error,document.has_value());
        ensure_equals("top bounds include padding",document->bounds.top,100);
        ensure_equals("line top below padding",document->lines.front().top,97);
        ensure_equals("fit height includes source bounds padding",document->fitHeight,height+9);
        ensure_equals("fit width extra missing pixel",document->fitWidth,document->bounds.right-document->bounds.left+5);
        ensure("document fills view",document->rectangle == LLVKPlainTextLayout::Rect{0,0,100,100});
        document = LLVKPlainTextLayout::document(U"X",*font,options,100,0,LLVKFont::VerticalAlign::Bottom,error);
        ensure(error,document.has_value());
        ensure_equals("bottom aligned",document->lines.front().bottom,0);
        document = LLVKPlainTextLayout::document(U"X",*font,options,100,0,LLVKFont::VerticalAlign::Center,error);
        ensure(error,document.has_value());
        ensure_equals("center integer rounding",document->lines.front().bottom,(100+height)/2-height);
        document = LLVKPlainTextLayout::document(U"X\nX",*font,options,1,0,LLVKFont::VerticalAlign::Top,error);
        ensure(error,document.has_value());
        ensure_equals("overflowing document anchored to top",document->rectangle.top,1);
        ensure_equals("overflowing height retained",document->rectangle.top-document->rectangle.bottom,2*height);
        ensure("padding overflow explicit",!LLVKPlainTextLayout::document(U"X",*font,options,1,INT32_MAX,
            LLVKFont::VerticalAlign::Top,error));
    }

    template<> template<> void object::test<29>()
    {
        set_test_name("native plain text line wrapping preserves newline and EOF positions");
        const auto font = loadFont();
        std::string error;
        LLVKPlainTextLayout::Options options;
        options.width = 100;
        options.horizontalPadding = 3;
        options.spacingPixels = 2;
        options.fontSpacingAdjustment = 1;
        const auto height = static_cast<std::int32_t>(std::ceil(font->metrics().ascender)+std::ceil(font->metrics().descender));
        auto lines = LLVKPlainTextLayout::plain(U"one\n\ntwo\n",*font,options,error);
        ensure(error,lines.has_value());
        ensure_equals("trailing newline adds empty EOF line",lines->size(),std::size_t(4));
        ensure_equals("first includes newline",lines->at(0).end,std::size_t(4));
        ensure_equals("empty paragraph includes newline",lines->at(1).end,std::size_t(5));
        ensure_equals("EOF index",lines->back().end,std::size_t(10));
        ensure_equals("paragraph numbering",lines->back().paragraph,std::size_t(3));
        ensure_equals("empty final line full font height",lines->back().top-lines->back().bottom,height);
        ensure_equals("spacing includes both settings",lines->at(1).top,-height-3);
        options.wrap = true;
        options.width = 0;
        options.horizontalPadding = 0;
        lines = LLVKPlainTextLayout::plain(U"AB",*font,options,error);
        ensure(error,lines.has_value());
        ensure_equals("unfittable character makes progress",lines->size(),std::size_t(2));
        ensure_equals("soft wrap keeps paragraph",lines->back().paragraph,std::size_t(0));
        ensure_equals("final char includes EOF",lines->back().end,std::size_t(3));
        lines = LLVKPlainTextLayout::plain(U"",*font,options,error);
        ensure(error,lines.has_value());
        ensure_equals("empty document has EOF line",lines->size(),std::size_t(1));
        ensure_equals("empty EOF end",lines->front().end,std::size_t(1));
        options.width = 100;
        options.alignment = LLVKFont::HorizontalAlign::Right;
        lines = LLVKPlainTextLayout::plain(U"AB",*font,options,error);
        ensure(error,lines.has_value());
        ensure_equals("right alignment reserves extra pixel",lines->front().right,99);
        options.horizontalPadding = INT32_MIN;
        ensure("available width overflow rejects",!LLVKPlainTextLayout::plain(U"AB",*font,options,error));
    }

    template<> template<> void object::test<28>()
    {
        set_test_name("native button badge labels refresh from owned originals without resize");
        LLVKWidgetTree tree;
        std::string error;
        LLVKLabel::Context context;
        context.defaults["AMOUNT"] = "10";
        context.currency = "G$";
        tree.setLabelContext(context);
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params button;
        button.label = U"Pay L$[AMOUNT]";
        button.selectedLabel = U"Paid L$[AMOUNT]";
        LLVKWidgetTree::Params view;
        view.rect = {0,0,80,20};
        auto id = tree.createButton(view,control,button,0,error);
        ensure(error,id.has_value());
        ensure("constructor resolves currency and args",tree.get(*id)->button->params.label == U"Pay G$10");
        tree.setButtonLabelArgument(*id,"AMOUNT","15");
        ensure("default still precedes local",tree.get(*id)->button->selectedLabel == U"Paid G$10");
        context.defaults.clear();
        context.currency = "R$";
        tree.setLabelContext(context);
        ensure("refresh uses originals and retained local args",tree.get(*id)->button->params.label == U"Pay R$15");
        ensure("selected refresh too",tree.get(*id)->button->selectedLabel == U"Paid R$15");
        tree.setButtonLabel(*id,U"Cost L$[AMOUNT]",false);
        ensure("assign preserves local args",tree.get(*id)->button->params.label == U"Cost R$15");
        ensure("label update does not resize",tree.get(*id)->params.rect == view.rect);
        LLVKBadge::Params badge;
        badge.label = U"L$[AMOUNT]";
        auto badgeId = tree.createBadge({},control,badge,*id,*id,error);
        ensure(error,badgeId.has_value());
        context.defaults["AMOUNT"] = "20";
        tree.setLabelContext(context);
        ensure("badge resolves latest context",tree.get(*badgeId)->badge->params.label == U"R$20");
        ensure("button shares context not label owner",tree.get(*id)->button->params.label == U"Cost R$20");
    }

    template<> template<> void object::test<27>()
    {
        set_test_name("native label owns substitutions and explicit currency context");
        LLVKLabel label;
        LLVKLabel::Context context;
        context.defaults["NAME"] = "default";
        label.setArgument("NAME","local");
        label.setArgument("[VALUE]","17");
        label.setArgument("EMPTY","");
        label.assign("[NAME] [VALUE] [MISSING] [EMPTY] L$ [VALUE,number,0]");
        context.currency = "G$";
        ensure_equals("default precedence and unresolved tokens",label.resolve(context),
            std::string("default 17 [MISSING]  G$ 17"));
        context.defaults.clear();
        ensure_equals("local resumes without default",label.resolve(context),
            std::string("local 17 [MISSING]  G$ 17"));
        label.clear();
        ensure("empty original stays empty",label.resolve(context).empty());
        label.assign("[NAME] [[VALUE]]");
        ensure_equals("clear preserves arguments and nested brackets",label.resolve(context),std::string("local [17]"));
        label.setArguments({{"VALUE","[NAME] L$"}});
        label.assign("[VALUE]");
        ensure_equals("replacement not recursively formatted but currency runs last",label.resolve(context),std::string("[NAME] G$"));
        context.currency = "L$L$";
        ensure_equals("currency replacement not recursive",label.resolve(context),std::string("[NAME] L$L$"));
        context.currency.clear();
        ensure_equals("empty currency removes token",label.resolve(context),std::string("[NAME] "));
    }

    template<> template<> void object::test<26>()
    {
        set_test_name("native badge XML parameters construct before button initialization");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::ButtonDefaults defaults;
        defaults.control.font = loadFont();
        defaults.badge.control.font = loadFont();
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["check"] = [&](auto id,const LLSD&)
        {
            const auto badge = tree.get(id)->button->badge;
            ensure("XML badge before button init",tree.get(badge) != nullptr);
            ensure("XML badge label",tree.get(badge)->badge->params.label == U"7");
        };
        LLVKWidgetFactory factory({}, {}, defaults, callbacks);
        auto id = factory.construct(tree,
            "<button width='80'><button.badge label='7' location='bottom_right' location_offset_hcenter='0'/>"
            "<button.init_callback function='check'/></button>",0,error);
        ensure(error,id.has_value());
        const auto badge = tree.get(*id)->button->badge;
        ensure_equals("one constructed badge child",tree.get(*id)->children.size(),std::size_t(1));
        ensure_equals("badge location",tree.get(badge)->badge->params.location,LLVKBadge::BottomRight);
        ensure("provided zero survives XML",tree.get(badge)->badge->params.offsetHorizontal.has_value());
        const auto empty = factory.construct(tree,"<button><button.badge/></button>",0,error);
        ensure(error,empty.has_value());
        ensure_equals("default badge parameter makes no widget",tree.get(*empty)->button->badge,LLVKWidgetTree::Id(0));
        const auto size = tree.size();
        ensure("duplicate badge rejected",!factory.construct(tree,"<button><button.badge/><button.badge/></button>",0,error));
        ensure_equals("parse rejection no partial widget",tree.size(),size);
        auto standalone = factory.construct(tree,"<badge label='standalone' width='20' height='10'/>",0,error);
        ensure(error,standalone.has_value());
        ensure("standalone has real badge state",tree.get(*standalone)->badge.has_value());
    }

    template<> template<> void object::test<25>()
    {
        set_test_name("native button owns badge before init and reparents at postbuild");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,200,100};
        auto holder = tree.create(view,0,error);
        ensure(error,holder.has_value());
        tree.setAcceptsBadge(*holder,true);
        LLVKWidgetTree::BadgeConstruction badge;
        badge.control.font = loadFont();
        badge.view.mouseOpaque = false;
        badge.control.requestsFront = true;
        badge.provided = badge.defaults;
        badge.provided->label = U"1";
        std::vector<std::string> events;
        badge.control.init.function = [&](auto id,const LLSD&)
        {
            ensure_equals("badge initially detached",tree.get(id)->parent,LLVKWidgetTree::Id(0));
            events.push_back("badge init");
        };
        LLVKControl::Params control;
        control.font = loadFont();
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto badgeId = tree.get(id)->button->badge;
            ensure("badge created before button init",tree.get(badgeId) != nullptr);
            ensure_equals("badge initially parented to button",tree.get(badgeId)->parent,id);
            events.push_back("button init");
        };
        auto button = tree.createButton(view,control,{},*holder,error,badge);
        ensure(error,button.has_value());
        ensure("nested init order",events == std::vector<std::string>{"badge init","button init"});
        const auto badgeId = tree.get(*button)->button->badge;
        ensure("button postbuild",tree.postBuildButton(*button,error));
        ensure_equals("postbuild moves badge to holder",tree.get(badgeId)->parent,*holder);
        ensure("owner remembers holder",tree.get(*button)->button->hasBadgeHolderParent);
        auto cover = tree.create(view,*holder,error);
        ensure(error,cover.has_value());
        ensure("badge label",tree.setButtonBadgeLabel(*button,U"22",error));
        ensure_equals("label update fronts badge",tree.get(*holder)->children.front(),badgeId);
        tree.setButtonBadgeVisible(*button,false);
        ensure("visibility forwarded",!tree.get(badgeId)->params.visible);
        control.init = {};
        badge.provided = badge.defaults;
        auto plain = tree.createButton(view,control,{},*holder,error,badge);
        ensure(error,plain.has_value());
        ensure_equals("equal defaults omit constructor badge",tree.get(*plain)->button->badge,LLVKWidgetTree::Id(0));
        ensure("lazy badge label",tree.setButtonBadgeLabel(*plain,U"3",error));
        ensure_equals("lazy badge attaches to holder",tree.get(tree.get(*plain)->button->badge)->parent,*holder);
        tree.erase(*holder,error);
        ensure_equals("full ownership teardown",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<24>()
    {
        set_test_name("native badge constructor separates owner attachment and provided offsets");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params view;
        view.rect = {10,20,210,120};
        auto holder = tree.create(view,0,error);
        ensure(error,holder.has_value());
        tree.setAcceptsBadge(*holder,true);
        view.rect = {5,6,55,36};
        auto intermediate = tree.create(view,*holder,error);
        ensure(error,intermediate.has_value());
        auto owner = tree.create(view,*intermediate,error);
        ensure(error,owner.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        control.init.function = [&](auto id,const LLSD&)
        { ensure("badge installed before control init",tree.get(id)->badge.has_value()); };
        LLVKBadge::Params params;
        params.percentHorizontal = params.percentVertical = 80;
        params.offsetHorizontal = 0;
        auto equivalent = params;
        equivalent.offsetHorizontal.reset();
        ensure("source equality ignores provided bits",params.equals(equivalent));
        auto badge = tree.createBadge({},control,params,*owner,0,error);
        ensure(error,badge.has_value());
        ensure("horizontal percent",std::abs(tree.get(*badge)->badge->horizontalCenter-0.1f) < 0.00001f);
        ensure("vertical percent",std::abs(tree.get(*badge)->badge->verticalCenter-0.9f) < 0.00001f);
        ensure("explicit zero offset retained",tree.get(*badge)->badge->params.offsetHorizontal.has_value());
        ensure("attach to owner",tree.attachBadge(*badge,*owner,error));
        ensure("fills owner local rectangle",tree.get(*badge)->params.rect == LLVKWidgetTree::Rect{0,0,50,30});
        ensure("find accepting ancestor",tree.attachBadgeToHolder(*badge,error));
        ensure_equals("parent is holder",tree.get(*badge)->parent,*holder);
        ensure_equals("owner unchanged",tree.get(*badge)->badge->owner,*owner);
        ensure("fills holder local rectangle",tree.get(*badge)->params.rect == LLVKWidgetTree::Rect{0,0,200,100});
        tree.setBadgeLabel(*badge,U"42");
        tree.setBadgeAtParentTop(*badge,true);
        ensure("native label",tree.get(*badge)->badge->params.label == U"42");
        tree.erase(*owner,error);
        ensure("badge parent retains it after owner deletion",tree.get(*badge) != nullptr);
        ensure("dead owner cannot reattach",!tree.attachBadgeToHolder(*badge,error));
        tree.erase(*holder,error);
        ensure_equals("holder deletion owns badge",tree.size(),std::size_t(0));
    }

    template<> template<> void object::test<23>()
    {
        set_test_name("native flash timer strict thresholds restart and settings cancellation");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("FlashCount",LLSD(2),LLVKWidgetTree::SettingType::Integer);
        tree.defineSetting("FlashPeriod",LLSD(0.5),LLVKWidgetTree::SettingType::Real);
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params params;
        params.flashEnable = true;
        auto id = tree.createButton({},control,params,0,error);
        ensure(error,id.has_value());
        ensure_equals("setting count doubled",tree.get(*id)->button->flashTimer->limit,std::uint64_t(4));
        tree.setButtonFlashing(*id,true,true,true);
        ensure("starts highlighted",tree.get(*id)->button->flashTimer->highlighted);
        tree.advanceTime(0.5,error);
        ensure_equals("strict period",tree.get(*id)->button->flashTimer->ticks,std::uint64_t(0));
        tree.advanceTime(0.75,error);
        ensure("first transition",!tree.get(*id)->button->flashTimer->highlighted);
        tree.advanceTime(10.0,error);
        ensure_equals("one tick per update no catchup",tree.get(*id)->button->flashTimer->ticks,std::uint64_t(2));
        tree.setButtonFlashing(*id,true);
        ensure_equals("restart preserves count",tree.get(*id)->button->flashTimer->ticks,std::uint64_t(2));
        tree.advanceTime(11.0,error);
        tree.advanceTime(12.0,error);
        ensure("timer finished",!tree.get(*id)->button->flashTimer->running);
        ensure("flashing flag persists for draw-time policy",tree.get(*id)->button->flashing);
        tree.setButtonFlashing(*id,true);
        tree.updateSetting("FlashCount",LLSD(3));
        ensure("setting change stops timer",!tree.get(*id)->button->flashTimer->running);
        ensure_equals("new setting count",tree.get(*id)->button->flashTimer->limit,std::uint64_t(6));
        tree.setButtonFlashing(*id,true,true,true);
        tree.setButtonToggle(*id,true,error);
        ensure("toggle resets all flash flags",!tree.get(*id)->button->flashing &&
            !tree.get(*id)->button->forceFlashing && !tree.get(*id)->button->alternateFlashColor &&
            !tree.get(*id)->button->flashTimer->running);
        tree.updateSetting("FlashCount",LLSD(0));
        tree.updateSetting("FlashPeriod",LLSD(0.0));
        tree.setButtonFlashing(*id,true);
        tree.advanceTime(12.0,error);
        ensure("zero period still strict",tree.get(*id)->button->flashTimer->running);
        tree.advanceTime(12.25,error);
        ensure("zero count stops on first eligible tick",!tree.get(*id)->button->flashTimer->running);
        ensure("backwards time rejected",!tree.advanceTime(1.0,error));
        ensure("nonfinite time rejected",!tree.advanceTime(std::numeric_limits<double>::infinity(),error));
        tree.eraseControl(*id,error);
        ensure("no deferred timer after deletion",tree.advanceTime(20.0,error));
        params.flashEnable = false;
        id = tree.createButton({},control,params,0,error);
        ensure(error,id.has_value());
        tree.setButtonFlashing(*id,true);
        tree.advanceTime(21.0,error);
        tree.setButtonFlashing(*id,true);
        ensure_equals("untimed flash resets only on change",tree.get(*id)->button->flashResetTime,20.0);
    }

    template<> template<> void object::test<22>()
    {
        set_test_name("native declared button resolves callbacks resources and activation without GL");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::ButtonDefaults defaults;
        defaults.control.font = loadFont();
        defaults.button.images.unselected = image("default");
        auto custom = image("custom");
        tree.registerImage(custom);
        tree.defineSetting("state",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        std::vector<std::string> observed;
        LLVKWidgetFactory::Callbacks callbacks;
        callbacks.actions["initialize"] = [&](auto id,const LLSD& argument)
        {
            ensure("button before init callback",tree.get(id)->button.has_value());
            ensure_equals("init unparented",tree.get(id)->parent,LLVKWidgetTree::Id(0));
            observed.push_back(argument.asString());
        };
        callbacks.actions["click"] = [&](auto,const LLSD& argument) { observed.push_back(argument.asString()); };
        callbacks.actions["commit"] = [&](auto,const LLSD& value) { observed.push_back(value.asBoolean() ? "on" : "off"); };
        LLVKWidgetFactory factory({}, {}, defaults, callbacks);
        auto root = factory.construct(tree,
            "<view width='200' height='100' layout='topleft'><button name='native' label='Caf&#233;'"
            " image_unselected='custom' control_name='state' is_toggle='true' top='4' left='5' width='80'>"
            "<button.init_callback function='initialize' parameter='initialized'/>"
            "<button.click_callback function='click' userdata='clicked'/>"
            "<button.commit_callback function='commit'/></button></view>",0,error);
        ensure(error,root.has_value());
        const auto id = tree.get(*root)->children.front();
        ensure("UTF-8 label",tree.get(id)->button->params.label == U"Caf\u00e9");
        ensure("custom disabled fallback from template identity",tree.get(id)->button->images.disabled == custom);
        ensure_equals("default template height",tree.get(id)->params.rect.top-tree.get(id)->params.rect.bottom,23);
        ensure("native callback installed",observed == std::vector<std::string>{"initialized"});
        ensure("declared button activation",tree.buttonReturn(id,0,false,error));
        ensure("click alias precedes commit",observed == std::vector<std::string>{"initialized","clicked","on"});
        const auto size = tree.size();
        ensure("unresolved callback explicit",!factory.construct(tree,
            "<button><button.commit_callback function='missing'/></button>",*root,error));
        ensure_equals("unresolved callback creates no partial control",tree.size(),size);
        auto locked = tree.createControl({},defaults.control,*root,error);
        ensure(error,locked.has_value());
        tree.setKeyboardFocus(*locked,true,false,error);
        LLVKWidgetTree::PointerEvent down{LLVKWidgetTree::PointerKind::LeftDown,10,90,0,1.0,1};
        ensure("mouse press not aborted by focus lock",tree.routePointer(*root,down,error));
        ensure_equals("keyboard focus stays locked",tree.keyboardFocus(),*locked);
        ensure_equals("button still captures",tree.mouseCapture(),id);
    }

    template<> template<> void object::test<20>()
    {
        set_test_name("native button mouse routing timer and capture-loss commit ordering");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params rootParams;
        rootParams.rect = {0,0,200,100};
        auto root = tree.create(rootParams,0,error);
        ensure(error,root.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params params;
        params.toggle = true;
        params.commitOnCaptureLost = true;
        params.heldSeconds = 0.5;
        params.heldFrames = 2;
        std::vector<std::string> events;
        params.mouseDown.function = [&](auto,const LLSD&) { events.push_back("button down"); };
        params.mouseUp.function = [&](auto,const LLSD&) { events.push_back("button up"); };
        params.held.function = [&](auto,const LLSD& value) { events.push_back("held"+std::to_string(value["count"].asInteger())); };
        control.commit.function = [&](auto,const LLSD& value) { events.push_back(value.asBoolean() ? "on" : "off"); };
        LLVKWidgetTree::Params view;
        view.rect = {10,10,110,40};
        view.soundFlags = 3;
        auto id = tree.createButton(view,control,params,*root,error);
        ensure(error,id.has_value());
        LLVKWidgetTree::Events callbacks;
        callbacks.pointer = [&](auto,const auto& event)
        { events.push_back(event.kind == LLVKWidgetTree::PointerKind::LeftDown ? "base down" : "base up"); };
        callbacks.sound = [&](auto,bool release) { events.push_back(release ? "release" : "press"); };
        callbacks.captureLost = [&](auto) { events.push_back("capture lost"); };
        tree.setEvents(*id,std::move(callbacks));
        LLVKWidgetTree::PointerEvent event{LLVKWidgetTree::PointerKind::LeftDown,20,20,0,1.0,10};
        ensure("mouse down",tree.routePointer(*root,event,error));
        ensure_equals("capture routed",tree.mouseCapture(),*id);
        ensure_equals("focus assigned",tree.keyboardFocus(),*id);
        ensure("down ordering",events == std::vector<std::string>{"base down","button down","press"});
        events.clear();
        event.kind = LLVKWidgetTree::PointerKind::Hover;
        event.time = 1.5; event.frame = 11;
        tree.routePointer(*root,event,error);
        ensure("held frame gate",events.empty());
        event.frame = 12;
        tree.routePointer(*root,event,error);
        tree.routePointer(*root,event,error);
        ensure("held count",events == std::vector<std::string>{"held0","held1"});
        events.clear();
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        ensure("mouse up",tree.routePointer(*root,event,error));
        ensure("timer reset suppresses capture-lost duplicate",events == std::vector<std::string>{"capture lost","base up","button up","release","on"});
        ensure("timer stopped",!tree.get(*id)->button->mouseDownTime);
        events.clear();
        event.kind = LLVKWidgetTree::PointerKind::LeftDown;
        tree.routePointer(*root,event,error);
        events.clear();
        tree.setMouseCapture(0,error);
        ensure("capture loss commits separately without sound",events == std::vector<std::string>{"button up","off","capture lost"});
        events.clear();
        tree.routePointer(*root,event,error);
        events.clear();
        event.kind = LLVKWidgetTree::PointerKind::LeftUp;
        event.x = 150;
        tree.routePointer(*root,event,error);
        ensure("outside release callbacks only",events == std::vector<std::string>{"capture lost","base up","button up"});
    }

    template<> template<> void object::test<21>()
    {
        set_test_name("native pointer child ordering opacity and self-deletion");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params params;
        params.rect = {0,0,100,100};
        auto root = tree.create(params,0,error);
        ensure(error,root.has_value());
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params button;
        unsigned downs = 0;
        button.mouseDown.function = [&](auto,const LLSD&) { ++downs; };
        auto id = tree.createButton(params,control,button,*root,error);
        ensure(error,id.has_value());
        auto blocker = tree.create(params,*root,error);
        ensure(error,blocker.has_value());
        LLVKWidgetTree::PointerEvent event{LLVKWidgetTree::PointerKind::LeftDown,10,10,0,1,1};
        ensure("opaque view handles",tree.routePointer(*root,event,error));
        ensure_equals("front opaque view blocks button",downs,0u);
        tree.setEnabled(*blocker,false);
        ensure("disabled front skipped",tree.routePointer(*root,event,error));
        ensure_equals("button receives",downs,1u);
        tree.setMouseCapture(0,error);
        LLVKWidgetTree::Events callbacks;
        callbacks.pointer = [&](auto target,const auto&) { std::string ignored; tree.eraseControl(target,ignored); };
        tree.setEvents(*id,std::move(callbacks));
        ensure("base callback deletion handled",tree.routePointer(*root,event,error));
        ensure("deleted safely",!tree.get(*id) && !tree.mouseCapture());
        ensure_equals("no late button callback",downs,1u);
    }

    template<> template<> void object::test<18>()
    {
        set_test_name("native programmatic and keyboard button activation preserve different effects");
        LLVKWidgetTree tree;
        std::string error;
        tree.defineSetting("toggle",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        LLVKControl::Params control;
        control.font = loadFont();
        control.valueSetting = "toggle";
        LLVKButton::Params button;
        button.toggle = true;
        std::vector<std::string> events;
        button.mouseDown.function = [&](auto,const LLSD& argument) { ensure("down argument undefined",argument.isUndefined()); events.push_back("down"); };
        button.mouseUp.function = [&](auto,const LLSD& argument) { ensure("up argument undefined",argument.isUndefined()); events.push_back("up"); };
        button.click = LLVKControl::Callback{};
        button.click->function = [&](auto,const LLSD&) { events.push_back("click"); };
        control.commit.function = [&](auto,const LLSD& value) { events.push_back(value.asBoolean() ? "on" : "off"); };
        LLVKWidgetTree::Params view;
        view.soundFlags = 3;
        auto id = tree.createButton(view,control,button,0,error);
        ensure(error,id.has_value());
        LLVKWidgetTree::Events effects;
        effects.sound = [&](auto,bool release) { events.push_back(release ? "release sound" : "press sound"); };
        tree.setEvents(*id,std::move(effects));
        ensure("activate",tree.activateButton(*id,error));
        ensure("programmatic ordering",events == std::vector<std::string>{"down","up","press sound","release sound","click","on"});
        events.clear();
        ensure("space",tree.buttonUnicode(*id,U' ',false,error));
        ensure("keyboard omits mouse effects",events == std::vector<std::string>{"click","off"});
        events.clear();
        ensure("repeat ignored",!tree.buttonUnicode(*id,U' ',true,error));
        ensure("nonspace ignored",!tree.buttonUnicode(*id,U'x',false,error));
        ensure("modified Return ignored",!tree.buttonReturn(*id,1,false,error));
        ensure("repeat Return ignored",!tree.buttonReturn(*id,0,true,error));
        ensure("no rejected-key effects",events.empty());
        ensure("Return",tree.buttonReturn(*id,0,false,error));
        ensure("Return only commits",events == std::vector<std::string>{"click","on"});
        tree.resetDirty(*id);
        tree.updateSetting("toggle",LLSD(true));
        ensure("toggle already wrote bound setting",!tree.get(*id)->control->dirty);
        auto observer = tree.createControl({},control,0,error);
        ensure(error,observer.has_value());
        ensure("bound value true",tree.get(*observer)->control->value.asBoolean());
    }

    template<> template<> void object::test<19>()
    {
        set_test_name("native button activation rechecks lifetime after user callbacks");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params button;
        unsigned commits = 0;
        control.commit.function = [&](auto,const LLSD&) { ++commits; };
        button.mouseDown.function = [&](auto id,const LLSD&) { std::string ignored; tree.eraseControl(id,ignored); };
        auto id = tree.createButton({},control,button,0,error);
        ensure(error,id.has_value());
        ensure("self deleting down handled",tree.activateButton(*id,error));
        ensure("node destroyed",!tree.get(*id));
        ensure_equals("no late commit after deletion",commits,0u);
        button.mouseDown = {};
        button.click = LLVKControl::Callback{};
        button.click->function = [&](auto id,const LLSD&) { std::string ignored; tree.eraseControl(id,ignored); };
        id = tree.createButton({},control,button,0,error);
        ensure(error,id.has_value());
        ensure("self deleting click handled",tree.buttonUnicode(*id,U' ',false,error));
        ensure_equals("no late signal after deletion",commits,0u);
    }

    template<> template<> void object::test<17>()
    {
        set_test_name("native button constructor image fallback padding and post-build sizing");
        LLVKWidgetTree tree;
        std::string error;
        LLVKControl::Params control;
        control.font = loadFont();
        LLVKButton::Params params;
        params.label = U"Button";
        params.selectedLabel = U"A much longer selected label";
        params.images.unselected = image("custom-off");
        params.images.selected = image("custom-on");
        params.leftPad = params.rightPad = 12;
        params.originalHorizontalPad = 2;
        params.autoResize = true;
        LLVKWidgetTree::Params view;
        view.rect = {0,0,10,23};
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            ensure("button exists before init",node->button.has_value());
            ensure("default initial state false",node->control->value.isBoolean() && !node->control->value.asBoolean());
            ensure_equals("constructor padding fallback",node->button->leftPad,2);
            ensure("custom disabled image",node->button->images.disabled == params.images.unselected);
            ensure("custom disabled selected",node->button->images.disabledSelected == params.images.selected);
            ensure("pressed image uses selected",node->button->images.pressed == params.images.selected);
            ensure("pressed selected uses unselected",node->button->images.pressedSelected == params.images.unselected);
            ensure("custom disabled fades",node->button->fadeWhenDisabled);
        };
        auto id = tree.createButton(view,control,params,0,error);
        ensure(error,id.has_value());
        ensure("post-build",tree.postBuildButton(*id,error));
        auto width = control.font->measureRun(params.label,0,params.label.size(),1.f,true,false,error);
        ensure(error,width.has_value());
        ensure_equals("grow to text plus padding",tree.get(*id)->params.rect.right,
                      static_cast<int>(std::floor(width->width+0.5f))+4);
        ensure("select",tree.setButtonToggle(*id,true,error));
        const auto expanded = tree.get(*id)->params.rect.right;
        ensure("selected label grows",expanded > 10);
        ensure("unselect",tree.setButtonToggle(*id,false,error));
        ensure_equals("never shrinks",tree.get(*id)->params.rect.right,expanded);
        const auto generation = tree.get(*id)->button->textGeneration;
        tree.setButtonToggle(*id,false,error);
        ensure_equals("equal state no cache invalidation",tree.get(*id)->button->textGeneration,generation);
        tree.setButtonLabel(*id,U"Updated");
        ensure("both labels updated",tree.get(*id)->button->selectedLabel == U"Updated" && tree.get(*id)->button->params.label == U"Updated");
        control.init = {};
        params.selectedLabel.reset();
        params.images.pressed = image("explicit-pressed");
        params.pressedProvided = true;
        auto second = tree.createButton(view,control,params,0,error);
        ensure(error,second.has_value());
        ensure("absent selected label uses primary",tree.get(*second)->button->selectedLabel == params.label);
        ensure("provided pressed image retained",tree.get(*second)->button->images.pressed == params.images.pressed);
    }

    template<> template<> void object::test<16>()
    {
        set_test_name("native icon constructor seeds value before init and prepares alpha-only modulation");
        std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
        ensure("font fixture",stream.good());
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        LLVKControl::Params control;
        control.font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,control.font != nullptr);
        png_image encoder{};
        encoder.version = PNG_IMAGE_VERSION;
        encoder.width = encoder.height = 1;
        encoder.format = PNG_FORMAT_RGBA;
        auto cleanup = [](png_image* value) { png_image_free(value); };
        std::unique_ptr<png_image,decltype(cleanup)> encoderOwner(&encoder,cleanup);
        const std::uint8_t pixel[]{255,255,255,255};
        png_alloc_size_t length = 0;
        ensure("image size",png_image_write_to_memory(&encoder,nullptr,&length,0,pixel,0,nullptr) != 0);
        std::vector<std::uint8_t> png(length);
        ensure("encode image",png_image_write_to_memory(&encoder,png.data(),&length,0,pixel,0,nullptr) != 0);
        auto image = LLVKWidgetImage::decodePng("icon",png,error);
        ensure(error,image != nullptr);
        LLVKWidgetTree tree;
        ensure("native image registry",tree.registerImage(image));
        LLVKIcon::Params icon;
        icon.image = image;
        icon.color = {0.25f,0.5f,0.75f,0.8f};
        icon.interactable = true;
        icon.minimumWidth = 12;
        icon.minimumHeight = 15;
        control.init.function = [&](auto id,const LLSD&)
        {
            const auto* node = tree.get(id);
            ensure("icon exists before init",node->icon.has_value());
            ensure_equals("constructor image name before init",node->control->value.asString(),std::string("icon"));
            ensure("constructor value marked dirty",node->control->dirty);
        };
        LLVKWidgetTree::Params view;
        view.rect = {10,20,30,40};
        auto id = tree.createIcon(view,control,icon,0,error);
        ensure(error,id.has_value());
        ensure("hand cursor requested",tree.iconWantsHandCursor(*id));
        auto draw = tree.prepareIcon(*id,0.5f,0.1f,error);
        ensure(error,draw.has_value());
        ensure("screen geometry unchanged by intrinsic dimensions",draw->rectangle == view.rect);
        ensure("image retained",draw->image == image);
        ensure_equals("red not multiplied by inherited alpha",draw->color[0],0.25f);
        ensure_equals("only alpha modulated",draw->color[3],0.4f);
        ensure("no constructor draw-size hint",tree.get(*id)->icon->desiredImageWidth == 0);
        tree.setValue(*id,LLSD("icon"));
        ensure_equals("reload records known size",tree.get(*id)->icon->desiredImageWidth,12);
        ensure_equals("reload records known height",tree.get(*id)->icon->desiredImageHeight,15);
        tree.setValue(*id,LLSD("missing"));
        draw = tree.prepareIcon(*id,1.f,1.f,error);
        ensure("missing image not a solid placeholder",draw && !draw->image);
        tree.setEnabled(*id,false);
        ensure("disabled no hand cursor",!tree.iconWantsHandCursor(*id));
        LLVKWidgetFactory::IconDefaults iconDefaults;
        iconDefaults.control.font = control.font;
        auto external = tree.create({},0,error);
        ensure(error,external.has_value());
        auto observer = tree.create({},*external,error);
        ensure(error,observer.has_value());
        bool hierarchyObserved = false;
        iconDefaults.control.init.function = [&](auto initializing,const LLSD&)
        {
            ensure_equals("own init still precedes parenting",tree.get(initializing)->parent,LLVKWidgetTree::Id(0));
            const auto& children = tree.get(*external)->children;
            ensure("root attached before child init",children.front() != *observer);
            const auto rootId = children.front();
            ensure_equals("constructed root has real external parent",tree.get(rootId)->parent,*external);
            hierarchyObserved = true;
        };
        LLVKWidgetFactory factory({},iconDefaults);
        auto declared = factory.construct(tree,
            "<view width='100' height='80' layout='topleft'>"
            "<icon image_name='icon' left='5' top='10' width='12' height='15' color='0.25 0.5 0.75 0.8' interactable='true'/>"
            "</view>",*external,error);
        ensure(error,declared.has_value());
        ensure("descendant observed source hierarchy",hierarchyObserved);
        const auto declaredIcon = tree.get(*declared)->children.front();
        const auto* node = tree.get(declaredIcon);
        ensure("real icon constructed from XML",node->icon && node->control);
        ensure_equals("icon template default name",node->params.name,std::string("icon"));
        ensure("icon template tab and mouse defaults",!node->control->params.tabStop && !node->params.mouseOpaque);
        ensure("native font owner from construction defaults",node->control->params.font == control.font);
        ensure("native named image owner",node->icon->image == image);
        ensure("icon XUI geometry",node->params.rect == LLVKWidgetTree::Rect{5,55,17,70});
        draw = tree.prepareIcon(declaredIcon,0.5f,0.1f,error);
        ensure("declared alpha",draw && draw->color[3] == 0.4f);
        const auto count = tree.size();
        ensure("bad icon color rejected",!factory.construct(tree,"<icon color='1 1 unknown 1'/>",*declared,error));
        ensure_equals("invalid icon does not leak",tree.size(),count);
        LLVKWidgetTree::Events focused;
        unsigned lost = 0;
        focused.focusLost = [&](auto) { ++lost; };
        tree.setEvents(declaredIcon,std::move(focused));
        ensure("focus nested icon",tree.setKeyboardFocus(declaredIcon,false,false,error));
        ensure("erase plain view with native control descendants",tree.erase(*declared,error));
        ensure_equals("control teardown invoked beneath plain view",lost,1u);
        ensure("focus reference cleared",!tree.keyboardFocus());
    }

    template<> template<> void object::test<15>()
    {
        set_test_name("native widget PNG decoder owns bottom-up straight RGBA pixels");
        const std::vector<std::uint8_t> source{255,0,0,128, 0,255,0,255,
                                             0,0,255,0, 255,255,255,64};
        png_image image{};
        image.version = PNG_IMAGE_VERSION;
        image.width = image.height = 2;
        image.format = PNG_FORMAT_RGBA;
        auto cleanup = [](png_image* value) { png_image_free(value); };
        std::unique_ptr<png_image,decltype(cleanup)> encoder(&image,cleanup);
        png_alloc_size_t size = 0;
        ensure("PNG fixture length",png_image_write_to_memory(&image,nullptr,&size,0,source.data(),0,nullptr) != 0);
        std::vector<std::uint8_t> encoded(size);
        ensure("PNG fixture encode",png_image_write_to_memory(&image,encoded.data(),&size,0,source.data(),0,nullptr) != 0);
        encoded.resize(size);
        std::string error;
        auto decoded = LLVKWidgetImage::decodePng("fixture",encoded,error);
        ensure(error,decoded != nullptr);
        ensure_equals("width",decoded->width(),2u);
        ensure_equals("height",decoded->height(),2u);
        for (std::size_t index = 0; index < source.size(); ++index)
            ensure_equals("row reversal, no premultiplication",decoded->bottomUpRgba()[index],source[(index+8)%16]);
        for (std::size_t length : {std::size_t(0),std::size_t(7),std::size_t(20),encoded.size()-1})
        {
            ensure("truncated PNG rejected",!LLVKWidgetImage::decodePng("bad",std::span(encoded).first(length),error));
            ensure("decode error explained",!error.empty());
        }
        encoded.clear();
        encoded.shrink_to_fit();
        ensure_equals("decoded ownership independent",decoded->bottomUpRgba()[10],std::uint8_t(0));
    }

    template<> template<> void object::test<13>()
    {
        set_test_name("native control callbacks cannot invalidate borrowed construction parameters");
        std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
        ensure("font fixture",stream.good());
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        LLVKWidgetTree tree;
        auto view = std::make_unique<LLVKWidgetTree::Params>();
        auto control = std::make_unique<LLVKControl::Params>();
        control->font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,control->font != nullptr);
        bool entered = false;
        control->mouseEnter.function = [&](auto,const LLSD&) { entered = true; };
        control->init.function = [&](auto,const LLSD&) { view.reset(); control.reset(); };
        auto id = tree.createControl(*view,*control,0,error);
        ensure(error,id.has_value());
        ensure("params really destroyed",!view && !control);
        tree.mouseEnter(*id);
        ensure("snapshot preserves later callback installation",entered);
    }

    template<> template<> void object::test<14>()
    {
        set_test_name("independent native bindings do not reset unrelated local flags");
        std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
        ensure("font fixture",stream.good());
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        LLVKControl::Params params;
        params.font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,params.font != nullptr);
        params.enabledSetting = "enabled";
        params.visibleSetting = "visible";
        LLVKWidgetTree tree;
        tree.defineSetting("enabled",LLSD(true));
        tree.defineSetting("visible",LLSD(true));
        auto id = tree.createControl({},params,0,error);
        ensure(error,id.has_value());
        tree.setVisible(*id,false);
        tree.updateSetting("enabled",LLSD(false));
        ensure("enabled event leaves local visibility",!tree.get(*id)->params.visible);
        tree.setEnabled(*id,true);
        tree.updateSetting("visible",LLSD(false));
        ensure("visibility event leaves local enabled",tree.get(*id)->params.enabled);
    }

    template<> template<> void object::test<11>()
    {
        set_test_name("native control initialization honors bindings callback order and owned fonts");
        std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
        ensure("font fixture",stream.good());
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        std::shared_ptr<LLVKFont> font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,font != nullptr);
        LLVKWidgetTree tree;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        tree.defineSetting("value",LLSD(23),LLVKWidgetTree::SettingType::Integer);
        tree.defineSetting("enabled",LLSD("0"));
        tree.defineSetting("visible",LLSD(true));
        tree.defineSetting("invisible",LLSD(false));
        LLVKControl::Params control;
        control.font = font;
        control.initialValue = LLSD(99);
        control.valueSetting = "value";
        control.enabledSetting = "enabled";
        control.visibleSetting = "visible";
        control.invisibleSetting = "invisible";
        std::vector<std::string> events;
        control.commit.function = [&](auto, const LLSD& value)
        { events.push_back("commit"+std::to_string(value.asInteger())); };
        control.mouseEnter.function = [&](auto, const LLSD&) { events.push_back("enter"); };
        control.validate.function = [](auto, const LLSD&) { return false; };
        control.init.function = [&](auto id, const LLSD& parameter)
        {
            const auto* node = tree.get(id);
            ensure_equals("init before parent",node->parent,LLVKWidgetTree::Id(0));
            ensure_equals("bound value precedes init",node->control->value.asInteger(),23);
            ensure("enabled string zero applied",!node->params.enabled);
            ensure("visible bindings applied",node->params.visible);
            ensure("init parameter undefined by default",parameter.isUndefined());
            ensure("commit installed before init",tree.commit(id));
            ensure("validate installed before init",!tree.validate(id));
            tree.mouseEnter(id);
            events.push_back("init");
        };
        auto id = tree.createControl({},control,*root,error);
        ensure(error,id.has_value());
        ensure("init sequence",events == std::vector<std::string>{"commit23","init"});
        ensure_equals("parent after init",tree.get(*id)->parent,*root);
        tree.mouseEnter(*id);
        ensure_equals("enter installed after init",events.back(),std::string("enter"));
        tree.updateSetting("enabled",LLSD(true));
        ensure("live enable binding",tree.get(*id)->params.enabled);
        tree.updateSetting("invisible",LLSD(true));
        ensure("invisible overrides visible",!tree.get(*id)->params.visible);
        tree.resetDirty(*id);
        tree.updateSetting("value",LLSD(23));
        ensure("equal typed setting does not notify",!tree.get(*id)->control->dirty);
        tree.setValue(*id,LLSD(23));
        ensure("equal assignment dirties",tree.get(*id)->control->dirty);
        ensure("bound write distinct",tree.writeBoundValue(*id,LLSD(42)));
        tree.commit(*id);
        ensure_equals("commit does not call validation",events.back(),std::string("commit42"));
        std::weak_ptr<LLVKFont> lifetime = font;
        font.reset();
        control.font.reset();
        ensure("control owns native font",!lifetime.expired());
        ensure("control erase",tree.eraseControl(*id,error));
        ensure("font retires with last control",lifetime.expired());
        tree.updateSetting("value",LLSD(12));
        ensure("no dead binding target",!tree.get(*id));
        control.init = {};
        control.enabledSetting = "boolean";
        control.font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,control.font != nullptr);
        tree.defineSetting("boolean",LLSD(false),LLVKWidgetTree::SettingType::Boolean);
        auto booleanControl = tree.createControl({},control,0,error);
        ensure(error,booleanControl.has_value());
        tree.updateSetting("boolean",LLSD(" True "));
        ensure("typed boolean string parsed",tree.get(*booleanControl)->params.enabled);
        tree.updateSetting("boolean",LLSD("TrUe"));
        ensure("unrecognized source boolean spelling becomes false",!tree.get(*booleanControl)->params.enabled);
    }

    template<> template<> void object::test<12>()
    {
        set_test_name("native init and commit callbacks may delete their control safely");
        std::ifstream stream(LLVK_WIDGET_FONT_FIXTURE,std::ios::binary);
        ensure("font fixture",stream.good());
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};
        std::string error;
        LLVKControl::Params control;
        control.font = LLVKFont::create({bytes,{}},{},false,error);
        ensure(error,control.font != nullptr);
        LLVKWidgetTree tree;
        control.init.function = [&](auto id, const LLSD&) { std::string ignored; tree.eraseControl(id,ignored); };
        auto failed = tree.createControl({},control,0,error);
        ensure("self-deleting init not returned",!failed);
        ensure_equals("no leaked node",tree.size(),std::size_t(0));
        control.init = {};
        control.valueSetting = "missing";
        control.initialValue = LLSD(99);
        control.commit.function = [&](auto id, const LLSD&) { std::string ignored; tree.eraseControl(id,ignored); };
        auto created = tree.createControl({},control,0,error);
        ensure(error,created.has_value());
        ensure("provided missing control name suppresses initial value",tree.get(*created)->control->value.isUndefined());
        ensure("commit self deletion",tree.commit(*created));
        ensure("deleted node remains invalid",!tree.get(*created));
    }

    template<> template<> void object::test<9>()
    {
        set_test_name("native focus notifications preserve branch order and reentrancy");
        LLVKWidgetTree tree;
        std::string error;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        auto first = tree.create({},*root,error);
        auto second = tree.create({},*root,error);
        auto third = tree.create({},*root,error);
        ensure("children",first && second && third);
        std::vector<std::string> notifications;
        for (auto id : {*root,*first,*second,*third})
        {
            LLVKWidgetTree::Events events;
            events.focusReceived = [&](auto target) { notifications.push_back("gain"+std::to_string(target)); };
            events.focusLost = [&](auto target) { notifications.push_back("lose"+std::to_string(target)); };
            tree.setEvents(id,std::move(events));
        }
        ensure("initial focus",tree.setKeyboardFocus(*first,false,false,error));
        ensure("gain descends",notifications == std::vector<std::string>{"gain"+std::to_string(*root),"gain"+std::to_string(*first)});
        notifications.clear();
        ensure("sibling focus",tree.setKeyboardFocus(*second,false,true,error));
        ensure("common ancestor omitted",notifications == std::vector<std::string>{"lose"+std::to_string(*first),"gain"+std::to_string(*second)});
        ensure("keystroke policy",tree.keystrokesOnly());
        LLVKWidgetTree::Events reentrant;
        reentrant.focusLost = [&](auto)
        {
            notifications.push_back("redirect");
            std::string nestedError;
            ensure("nested focus",tree.setKeyboardFocus(*third,false,false,nestedError));
        };
        tree.setEvents(*second,std::move(reentrant));
        notifications.clear();
        ensure("outer focus",tree.setKeyboardFocus(*first,false,false,error));
        ensure_equals("redirect won",tree.keyboardFocus(),*third);
        ensure("no stale receive",notifications == std::vector<std::string>{"redirect","gain"+std::to_string(*third)});
        ensure("lock current",tree.setKeyboardFocus(*third,true,false,error));
        ensure("lock rejects outside",!tree.setKeyboardFocus(*first,false,false,error));
        tree.unlockFocus();
        ensure("unlocked",tree.setKeyboardFocus(*first,false,false,error));
    }

    template<> template<> void object::test<10>()
    {
        set_test_name("native control teardown releases capture before focus and prevents resurrection");
        LLVKWidgetTree tree;
        std::string error;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        auto control = tree.create({},*root,error);
        ensure(error,control.has_value());
        std::vector<std::string> notifications;
        LLVKWidgetTree::Events events;
        events.captureLost = [&](auto target)
        {
            notifications.push_back("capture");
            ensure_equals("captor cleared first",tree.mouseCapture(),LLVKWidgetTree::Id(0));
            std::string nestedError;
            ensure("cannot recapture erasing control",!tree.setMouseCapture(target,nestedError));
            ensure("cannot erase active ancestor twice",!tree.erase(*root,nestedError));
        };
        events.focusLost = [&](auto target)
        {
            notifications.push_back("focus");
            std::string nestedError;
            ensure("cannot refocus erasing control",!tree.setKeyboardFocus(target,false,false,nestedError));
        };
        events.topLost = [&](auto) { notifications.push_back("top"); };
        tree.setEvents(*control,std::move(events));
        ensure("capture",tree.setMouseCapture(*control,error));
        ensure("keyboard",tree.setKeyboardFocus(*control,true,false,error));
        ensure("top",tree.setTopControl(*control,error));
        ensure("destroy",tree.eraseControl(*control,error));
        ensure("capture before focus, top removed silently",notifications == std::vector<std::string>{"capture","focus"});
        ensure("all references cleared",!tree.keyboardFocus() && !tree.mouseCapture() && !tree.topControl());
        ensure("no node",!tree.get(*control));
        auto replacement = tree.create({},*root,error);
        ensure(error,replacement.has_value());
        ensure("old callback target remains invalid",!tree.setKeyboardFocus(*control,false,false,error));
        ensure("tree still usable",tree.setKeyboardFocus(*replacement,false,false,error));
    }

    template<> template<> void object::test<8>()
    {
        set_test_name("native construction limits cannot publish partial ownership");
        LLVKWidgetTree tree;
        std::string error;
        auto root = tree.create({},0,error);
        ensure(error,root.has_value());
        auto deepest = *root;
        for (std::size_t depth = 1; depth < LLVKWidgetTree::maximumDepth; ++depth)
        {
            auto child = tree.create({},deepest,error);
            ensure(error,child.has_value());
            deepest = *child;
        }
        ensure("depth failure",!tree.create({},deepest,error));
        const auto count = tree.size();
        LLVKWidgetFactory factory({});
        ensure("external depth prevents publication",!factory.construct(tree,"<view><view/></view>",deepest,error));
        ensure_equals("unpublished subtree released",tree.size(),count);
        ensure("deep parent unchanged",tree.get(deepest)->children.empty());
        auto detached = tree.create({},0,error);
        ensure(error,detached.has_value());
        ensure("reparent rejected",!tree.reparent(*detached,deepest,false,12,error));
        ensure_equals("old parent preserved",tree.get(*detached)->parent,LLVKWidgetTree::Id(0));
        ensure("root recursive teardown stays bounded",tree.erase(*root,error));
        ensure_equals("detached remains",tree.size(),std::size_t(1));
        LLVKWidgetLayout overflow;
        overflow.width = {INT64_MAX,true};
        ensure("untrusted numeric value rejected",!overflow.resolve(error));
        ensure("untrusted numeric layout rejected",!overflow.apply(tree,*detached,"topleft",error));
    }

    template<> template<> void object::test<7>()
    {
        set_test_name("native bounds and screen geometry reflect current constructed owners");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory factory({});
        auto root = factory.construct(tree,
            "<view left='10' bottom='20' width='100' height='100' use_bounding_rect='true'>"
            "<view name='one' left='2' bottom='3' width='10' height='15'/>"
            "<view name='two' left='30' bottom='40' width='20' height='25'/></view>",0,error);
        ensure(error,root.has_value());
        const auto first = tree.get(*root)->children[1];
        const auto second = tree.get(*root)->children[0];
        auto bounds = tree.boundingRect(*root,0,error);
        ensure("union translated to parent",bounds && *bounds == LLVKWidgetTree::Rect{12,23,60,85});
        auto screen = tree.screenRect(second,error);
        ensure("screen coordinate accumulation",screen && *screen == LLVKWidgetTree::Rect{40,60,60,85});
        bounds = tree.boundingRect(*root,second,error);
        ensure("top-control excluded from bounds",bounds && *bounds == LLVKWidgetTree::Rect{12,23,22,38});
        tree.setVisible(second,false);
        bounds = tree.boundingRect(*root,0,error);
        ensure("hidden child excluded immediately",bounds && *bounds == LLVKWidgetTree::Rect{12,23,22,38});
        auto inside = tree.containsLocal(first,0,0,false,0,error);
        auto outside = tree.containsLocal(first,10,15,false,0,error);
        ensure("lower edge inclusive",inside && *inside);
        ensure("upper edge exclusive",outside && !*outside);
        auto boundedInside = tree.containsLocal(*root,2,3,true,0,error);
        auto boundedOutside = tree.containsLocal(*root,0,0,true,0,error);
        ensure("child bounds hit",boundedInside && *boundedInside);
        ensure("bounds not own rectangle",boundedOutside && !*boundedOutside);
        ensure("erase visible child",tree.erase(first,error));
        bounds = tree.boundingRect(*root,0,error);
        ensure("empty bounds anchored at origin",bounds && *bounds == LLVKWidgetTree::Rect{10,20,10,20});
    }

    template<> template<> void object::test<6>()
    {
        set_test_name("native XML view construction owns a layout-ready subtree");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetFactory::Defaults defaults;
        defaults.view.mouseOpaque = false;
        defaults.geometry.height = {12,true};
        LLVKWidgetFactory factory(defaults);
        auto root = factory.construct(tree,
            "<view name='root' layout='topleft' width='200' height='100'><view name='one' left='5' top='8' width='50' height='20' tab_group='0' tool_tip='A &amp; B'/><view name='two' top_pad='3' width='30'/></view>",0,error);
        ensure(error,root.has_value());
        const auto* owner = tree.get(*root);
        ensure_equals("two constructed children",owner->children.size(),std::size_t(2));
        const auto* second = tree.get(owner->children[0]);
        const auto* first = tree.get(owner->children[1]);
        ensure_equals("front ordering",second->params.name,std::string("two"));
        ensure("first rectangle",first->params.rect == LLVKWidgetTree::Rect{5,72,55,92});
        ensure("sibling/default layout",second->params.rect == LLVKWidgetTree::Rect{5,57,35,69});
        ensure("inherited native defaults",!first->params.mouseOpaque && !second->params.mouseOpaque);
        ensure("layout inheritance",second->params.layout == "topleft");
        ensure("provided zero tab group",first->params.tabGroup && *first->params.tabGroup == 0);
        ensure_equals("XML text decoding",first->params.tooltip,std::string("A & B"));
        const auto count = tree.size();
        const auto oldChildren = owner->children;
        const auto oldTab = owner->lastTabGroup;
        auto failed = factory.construct(tree,
            "<view width='10' height='10'><view width='2' height='2'/><view left='2147483647' width='20'/></view>",*root,error);
        ensure("invalid descendant fails",!failed);
        ensure_equals("no leaked subtree",tree.size(),count);
        ensure("parent order unchanged",tree.get(*root)->children == oldChildren);
        ensure_equals("parent metadata unchanged",tree.get(*root)->lastTabGroup,oldTab);
        ensure("unsupported constructor not substituted",!factory.construct(tree,"<button/>",*root,error));
        ensure("invalid bool rejected",!factory.construct(tree,"<view visible='perhaps'/>",*root,error));
        ensure("DTD rejected",!factory.construct(tree,"<!DOCTYPE view [<!ENTITY x SYSTEM 'file:///not-read'>]><view/>",*root,error));
        ensure_equals("failures do not mutate tree",tree.size(),count);
        auto appended = factory.construct(tree,"<view name='three' width='20' height='5' top_pad='2'/>",*root,error);
        ensure(error,appended.has_value());
        ensure_equals("publish new child front",tree.get(*root)->children.front(),*appended);
        ensure("external sibling placement",tree.get(*appended)->params.rect == LLVKWidgetTree::Rect{5,50,25,55});
    }

    template<> template<> void object::test<4>()
    {
        set_test_name("native rectangle constraints honor provided edge precedence");
        LLVKWidgetLayout layout;
        std::string error;
        layout.left = {10,true}; layout.right = {45,true}; layout.width = {100,true};
        layout.top = {90,true}; layout.height = {20,true};
        auto rect = layout.resolve(error);
        ensure(error,rect.has_value());
        ensure_equals("edges win width",rect->right-rect->left,35);
        ensure_equals("top height",rect->bottom,70);
        layout.left.provided = false;
        rect = layout.resolve(error);
        ensure("right width resolves left",rect && rect->left == -55);
        layout.bottom = {11,true};
        rect = layout.resolve(error);
        ensure("vertical edges win height",rect && rect->bottom == 11);
        layout.right = {INT32_MAX,true}; layout.width = {-20,true};
        ensure("overflow rejected",!layout.resolve(error));
    }

    template<> template<> void object::test<5>()
    {
        set_test_name("native declaration layout preserves parent and sibling rules");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params params;
        params.rect = {10,20,210,120};
        auto root = tree.create(params,0,error);
        ensure(error,root.has_value());
        LLVKWidgetLayout layout;
        layout.left = {5,true}; layout.top = {8,true};
        layout.width = {50,true}; layout.height = {20,true};
        auto rect = layout.apply(tree,*root,"topleft",error);
        ensure(error,rect.has_value());
        ensure("top-left conversion",*rect == LLVKWidgetTree::Rect{5,72,55,92});
        params.rect = *rect;
        params.fromDeclaration = true;
        auto child = tree.create(params,*root,error);
        ensure(error,child.has_value());
        LLVKWidgetLayout next;
        next.width = {30,true}; next.height = {10,true};
        next.topPad = {3,true};
        rect = next.apply(tree,*root,"topleft",error);
        ensure("below prior declared child",rect && *rect == LLVKWidgetTree::Rect{5,59,35,69});
        next.topPad.provided = false;
        next.leftPad = {7,true};
        rect = next.apply(tree,*root,"topleft",error);
        ensure("left padding uses sibling right",rect && *rect == LLVKWidgetTree::Rect{62,72,92,82});
        next.left = {100,true}; next.leftDelta = {2,true}; next.leftPad.provided = false;
        rect = next.apply(tree,*root,"topleft",error);
        ensure("delta overrides explicit left",rect && rect->left == 7);
        layout.left = {-60,true}; layout.top = {-8,true};
        rect = layout.apply(tree,*root,"topleft",error);
        ensure("negative coordinates use opposite edge",rect && *rect == LLVKWidgetTree::Rect{140,-12,190,8});
        LLVKWidgetLayout minimum;
        rect = minimum.apply(tree,*root,"bottomleft",error);
        ensure("legacy minimum height",rect && rect->top-rect->bottom == 10);
        ensure("input not mutated",layout.left.value == -60 && layout.top.value == -8);
    }

    template<> template<> void object::test<1>()
    {
        set_test_name("native widget ownership order reparent and destruction");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params params;
        params.name = "root";
        auto root = tree.create(params,0,error);
        ensure(error,root.has_value());
        params.name = "first";
        auto first = tree.create(params,*root,error);
        auto second = tree.create(params,*root,error);
        ensure("children created",first && second);
        ensure_equals("new child at front",tree.get(*root)->children.front(),*second);
        ensure_equals("unspecified tab group",tree.get(*root)->lastTabGroup,INT32_MAX);
        ensure("reparent",tree.reparent(*first,*second,false,0,error));
        ensure_equals("new parent",tree.get(*first)->parent,*second);
        ensure_equals("old parent has one child",tree.get(*root)->children.size(),std::size_t(1));
        ensure("cycle rejected",!tree.reparent(*root,*first,false,0,error));
        ensure("self rejected",!tree.reparent(*first,*first,false,0,error));
        ensure("detach",tree.reparent(*first,0,false,0,error));
        ensure("delete original tree",tree.erase(*root,error));
        ensure("subtree gone",!tree.get(*root) && !tree.get(*second));
        ensure("detached owner preserved",tree.get(*first) != nullptr);
        ensure("delete detached",tree.erase(*first,error));
        auto replacement = tree.create({},0,error);
        ensure("identity not reused",replacement && *replacement != *first && *replacement != *root);
    }

    template<> template<> void object::test<2>()
    {
        set_test_name("all native follows combinations preserve source geometry");
        for (std::uint8_t follows = 0; follows < 16; ++follows)
        {
            LLVKWidgetTree tree;
            std::string error;
            LLVKWidgetTree::Params params;
            params.rect = {0,0,100,100};
            auto root = tree.create(params,0,error);
            ensure(error,root.has_value());
            params.rect = {10,20,40,60};
            params.follows = follows;
            auto child = tree.create(params,*root,error);
            ensure(error,child.has_value());
            ensure("reshape",tree.reshape(*root,120,130,error));
            const auto rect = tree.get(*child)->params.rect;
            const auto leftShift = (follows & LLVKWidgetTree::Right) && !(follows & LLVKWidgetTree::Left) ? 20 : 0;
            const auto bottomShift = (follows & LLVKWidgetTree::Top) && !(follows & LLVKWidgetTree::Bottom) ? 30 : 0;
            ensure_equals("left",rect.left,10+leftShift);
            ensure_equals("right",rect.right,40+((follows & LLVKWidgetTree::Right) ? 20 : 0));
            ensure_equals("bottom",rect.bottom,20+bottomShift);
            ensure_equals("top",rect.top,60+((follows & LLVKWidgetTree::Top) ? 30 : 0));
        }
    }

    template<> template<> void object::test<3>()
    {
        set_test_name("native visibility chains and transactional resize failure");
        LLVKWidgetTree tree;
        std::string error;
        LLVKWidgetTree::Params params;
        params.rect = {0,0,10,10};
        auto root = tree.create(params,0,error);
        ensure(error,root.has_value());
        params.rect = {INT32_MAX-2,0,INT32_MAX,2};
        params.follows = LLVKWidgetTree::Right;
        auto child = tree.create(params,*root,error);
        ensure(error,child.has_value());
        ensure("resize failure",!tree.reshape(*root,20,10,error));
        ensure_equals("parent unchanged",tree.get(*root)->params.rect.right,10);
        ensure_equals("child unchanged",tree.get(*child)->params.rect.right,INT32_MAX);
        ensure("initial visible",tree.visibleInChain(*child));
        tree.setVisible(*root,false);
        ensure("ancestor hides child",!tree.visibleInChain(*child));
        ensure("local flag unchanged",tree.get(*child)->params.visible);
        tree.setEnabled(*root,false);
        ensure("ancestor disables child",!tree.enabledInChain(*child));
        ensure("no missing view",!tree.enabledInChain(0));
        ensure_equals("exact follows tokens",LLVKWidgetTree::parseFollows("left||top| right|unknown"),std::uint8_t(5));
        ensure_equals("all follows",LLVKWidgetTree::parseFollows("all"),std::uint8_t(15));
    }
}
#include "linden_common.h"
#include "llvkwidgettree.h"
#include "llvkwidgetlayout.h"
#include "llvkwidgetfactory.h"
#include "llvklabel.h"
#include "llvkcolor.h"
#include "llvkplaintextlayout.h"
#include "llvkfont.h"
#include "llvkwidgetimage.h"
#include "lltut.h"
#include <png.h>

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
    typedef test_group<widgettree_data> widgettree_group;
    typedef widgettree_group::object object;
    widgettree_group widgettree_tests("llvkwidgettree");

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
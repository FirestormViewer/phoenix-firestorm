/**
 * @file llpanelpluginsettings.cpp
 * @brief Dynamic settings UI for Manikineko viewer plugins.
 */

#include "llviewerprecompiledheaders.h"

#include "llpanelpluginsettings.h"

#include "mkopluginmanager.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

#include <vector>

namespace
{
const S32 ROW_HEIGHT = 22;
const S32 HEADER_HEIGHT = 18;
const S32 LEFT_MARGIN = 10;
const S32 LABEL_WIDTH = 200;

std::string getPluginSetting(const std::string& name, const std::string& default_value)
{
    char buf[4096];
    if (MkoPluginManager::getSetting(name.c_str(), buf, sizeof(buf)) == 0)
    {
        return std::string(buf);
    }
    return default_value;
}

void commitSetting(const std::string& name, const std::string& value)
{
    MkoPluginManager::setSettingFromUI(name.c_str(), value.c_str());
}

void addBoolean(LLPanel* parent, S32& top, const std::string& name, const MkoSettingDef& def)
{
    LLCheckBoxCtrl::Params p;
    p.rect(LLRect(LEFT_MARGIN, top, parent->getRect().getWidth() - LEFT_MARGIN, top - ROW_HEIGHT));
    p.layout("topleft");
    p.name(name);
    p.label(def.label);
    p.initial_value(LLSD(getPluginSetting(name, def.default_value) == "1"
                         || getPluginSetting(name, def.default_value) == "true"));
    LLCheckBoxCtrl* ctrl = LLUICtrlFactory::create<LLCheckBoxCtrl>(p);
    ctrl->setCommitCallback([name](LLUICtrl* c, const LLSD& value)
    {
        commitSetting(name, value.asBoolean() ? "1" : "0");
    });
    parent->addChild(ctrl);
    top -= ROW_HEIGHT;
}

void addLabelledRow(LLPanel* parent, S32& top, const std::string& name,
                    const MkoSettingDef& def, LLUICtrl* editor)
{
    LLTextBox::Params tp;
    tp.rect(LLRect(LEFT_MARGIN, top, LEFT_MARGIN + LABEL_WIDTH, top - ROW_HEIGHT));
    tp.layout("topleft");
    tp.name(name + "_label");
    LLTextBox* label = LLUICtrlFactory::create<LLTextBox>(tp);
    label->setValue(def.label);
    parent->addChild(label);

    LLRect editor_rect(LEFT_MARGIN + LABEL_WIDTH + 10, top,
                       parent->getRect().getWidth() - LEFT_MARGIN, top - ROW_HEIGHT);
    editor->setRect(editor_rect);
    parent->addChild(editor);
    top -= ROW_HEIGHT;
}

void addString(LLPanel* parent, S32& top, const std::string& name, const MkoSettingDef& def)
{
    LLLineEditor::Params p;
    p.layout("topleft");
    p.name(name);
    p.initial_value(getPluginSetting(name, def.default_value));
    LLLineEditor* editor = LLUICtrlFactory::create<LLLineEditor>(p);
    editor->setCommitCallback([name](LLUICtrl* c, const LLSD&)
    {
        commitSetting(name, c->getValue().asString());
    });
    addLabelledRow(parent, top, name, def, editor);
}

void addSpinner(LLPanel* parent, S32& top, const std::string& name, const MkoSettingDef& def)
{
    LLSpinCtrl::Params p;
    p.layout("topleft");
    p.name(name);
    p.initial_value(LLSD(getPluginSetting(name, def.default_value)));
    p.min_value((F32)def.min_value);
    p.max_value((F32)def.max_value);
    p.increment(1.0f);
    LLSpinCtrl* spinner = LLUICtrlFactory::create<LLSpinCtrl>(p);
    spinner->setCommitCallback([name](LLUICtrl* c, const LLSD&)
    {
        commitSetting(name, c->getValue().asString());
    });
    addLabelledRow(parent, top, name, def, spinner);
}

void addEnum(LLPanel* parent, S32& top, const std::string& name, const MkoSettingDef& def)
{
    LLComboBox::Params p;
    p.layout("topleft");
    p.name(name);
    LLComboBox* combo = LLUICtrlFactory::create<LLComboBox>(p);
    std::string current = getPluginSetting(name, def.default_value);
    bool found = false;
    size_t start = 0;
    std::string options = def.options;
    while (start <= options.size())
    {
        size_t end = options.find('|', start);
        if (end == std::string::npos) end = options.size();
        std::string option = options.substr(start, end - start);
        if (!option.empty())
        {
            combo->add(option, LLSD(option));
            if (option == current) found = true;
        }
        start = end + 1;
    }
    if (!found && !current.empty())
    {
        combo->add(current, LLSD(current));
    }
    combo->selectByValue(LLSD(current));
    combo->setCommitCallback([name](LLUICtrl* c, const LLSD&)
    {
        commitSetting(name, c->getValue().asString());
    });
    addLabelledRow(parent, top, name, def, combo);
}
} // namespace

void LLPanelPluginSettings::populate(LLPanel* container)
{
    if (!container) return;

    container->deleteAllChildren();

    MkoPluginManager& mgr = MkoPluginManager::instance();
    std::vector<MkoSettingsTab> tabs = mgr.getSettingsTabs();

    S32 top = container->getRect().getHeight() - 5;
    bool drew_any = false;

    for (const MkoSettingsTab& tab : tabs)
    {
        std::vector<std::pair<std::string, MkoSettingDef> > settings;
        mgr.getSettingsForTab(tab.id, settings);
        if (settings.empty()) continue;

        LLTextBox::Params hp;
        hp.rect(LLRect(LEFT_MARGIN, top, container->getRect().getWidth() - LEFT_MARGIN, top - HEADER_HEIGHT));
        hp.layout("topleft");
        hp.name("mko_tab_header_" + tab.id);
        LLTextBox* header = LLUICtrlFactory::create<LLTextBox>(hp);
        header->setValue(tab.label);
        container->addChild(header);
        top -= HEADER_HEIGHT;
        drew_any = true;

        for (const auto& kv : settings)
        {
            const std::string& name = kv.first;
            const MkoSettingDef& def = kv.second;
            if (def.type == "boolean")
            {
                addBoolean(container, top, name, def);
            }
            else if (def.type == "string")
            {
                addString(container, top, name, def);
            }
            else if (def.type == "float" || def.type == "integer")
            {
                addSpinner(container, top, name, def);
            }
            else if (def.type == "enum")
            {
                addEnum(container, top, name, def);
            }
        }
        top -= 8; // gap between tabs
    }

    if (!drew_any)
    {
        LLTextBox::Params np;
        np.rect(LLRect(LEFT_MARGIN, top, container->getRect().getWidth() - LEFT_MARGIN, top - ROW_HEIGHT));
        np.layout("topleft");
        np.name("mko_no_plugins");
        LLTextBox* none = LLUICtrlFactory::create<LLTextBox>(np);
        none->setValue("No plugin settings registered.");
        container->addChild(none);
    }
}

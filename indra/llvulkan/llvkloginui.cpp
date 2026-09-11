#include "llvkloginui.h"
#include "llvkskinimages.h"
#include "lluri.h"
#include "llvkxmllayers.h"
#include <fstream>

std::string LLVKLoginUi::pageUrl(const Page& page)
{
    const LLURI uri(page.url);
    LLSD query = uri.queryMap();
    const auto setting = [&](const char* name)
    { const auto found = page.settings.find(name); return found == page.settings.end() ? LLSD() : found->second; };
    query["lang"] = page.language;
    if (setting("FirstLoginThisInstall").asBoolean()) query["firstlogin"] = "TRUE";
    query["version"] = page.version;
    query["channel"] = page.channel;
    query["grid"] = page.grid;
    query["os"] = page.operatingSystem;
    query["sourceid"] = setting("sourceid").asString();
    query["login_content_version"] = setting("LoginContentVersion").asString();
    query["skin"] = page.skin+" "+page.theme;
    if (setting("FSNoVersionPopup").asBoolean()) query["noversionpopup"] = "true";
    for (const auto& [name,key] : {std::pair{"FSSplashScreenHideTopBar","hidetopbar"},std::pair{"FSSplashScreenHideBlogs","hideblogs"},
        std::pair{"FSSplashScreenHideDestinations","hidedestinations"},std::pair{"FSSplashScreenUseGrayMode","usegraymode"},
        std::pair{"FSSplashScreenUseHighContrast","usehighcontrast"},std::pair{"FSSplashScreenUseAllCaps","useallcaps"},
        std::pair{"FSSplashScreenUseLargerFonts","uselargerfonts"},std::pair{"FSSplashScreenNoTransparency","notransparency"}})
        query[key] = setting(name).asBoolean() ? "1" : "0";
    const auto authority = uri.scheme().empty() ? uri.authority() : uri.scheme()+"://"+uri.authority();
    return LLURI::buildHTTP(authority,uri.path(),query).asString();
}

std::unique_ptr<LLVKLoginUi> LLVKLoginUi::create(const Configuration& configuration, std::string& error)
{
    error.clear();
    auto ui = std::make_unique<LLVKLoginUi>();
    ui->mSkin = std::make_shared<LLVKSkinFiles>(configuration.skin);
    std::vector<std::string> descriptions;
    if (!configuration.fontDescription.empty())
    {
        std::ifstream stream(configuration.fontDescription,std::ios::binary | std::ios::ate);
        if (!stream) { error = "Native login font descriptor is unavailable"; return nullptr; }
        const auto size = stream.tellg();
        if (size <= 0 || size > 4*1024*1024) { error = "Native login font descriptor exceeds byte limit"; return nullptr; }
        std::string text(static_cast<std::size_t>(size),'\0');
        stream.seekg(0);
        if (!stream.read(text.data(),size)) { error = "Native login font descriptor read failed"; return nullptr; }
        descriptions.push_back(std::move(text));
    }
    else
    {
        auto files = ui->mSkin->read("xui","fonts.xml",LLVKSkinFiles::Policy::Current,error);
        if (!files) return nullptr;
        descriptions = std::move(*files);
    }
    ui->mFonts = LLVKFontRegistry::create(descriptions,configuration.fonts,error);
    if (!ui->mFonts) return nullptr;
    ui->mColors = std::make_shared<LLVKColorTable>();
    const auto colors = ui->mSkin->read("","colors.xml",LLVKSkinFiles::Policy::All,error);
    if (!colors) return nullptr;
    std::vector<std::string> warnings;
    for (const auto& document : *colors)
        if (!ui->mColors->load(document,LLVKColorTable::Layer::Loaded,warnings,error)) return nullptr;
    auto images = std::make_shared<LLVKSkinImages>(ui->mSkin);
    if (!images->loadDeclarations(error)) return nullptr;
    ui->mTree.setSkinImages(images);
    ui->mTree.setLabelContext(configuration.labels);
    for (const auto& [name,value] : configuration.settings)
    {
        auto type = LLVKWidgetTree::SettingType::Opaque;
        if (value.isBoolean()) type = LLVKWidgetTree::SettingType::Boolean;
        else if (value.isInteger()) type = LLVKWidgetTree::SettingType::Integer;
        else if (value.isReal()) type = LLVKWidgetTree::SettingType::Real;
        else if (value.isString()) type = LLVKWidgetTree::SettingType::String;
        if (!ui->mTree.defineSetting(name,value,type)) { error = "Invalid native login setting: "+name; return nullptr; }
    }
    LLVKWidgetFactory::Resources resources;
    resources.skinFiles = ui->mSkin;
    resources.fontRegistry = ui->mFonts;
    resources.colors = ui->mColors;
    resources.defaultFontRequest = {"SansSerif","Small"};
    LLVKWidgetFactory::PanelDefaults panel;
    panel.control.fontRequest = resources.defaultFontRequest;
    LLVKWidgetFactory factory({}, {}, {}, {},resources,panel);
    for (const std::string widget : {"view_border","button","icon","line_editor","check_box","scroll_bar",
        "scroll_container","combo_box","text","web_browser","layout_stack"})
        if (!factory.loadDefaultsFile(ui->mTree,"widgets/"+widget+".xml",error))
        { error = "Native login "+widget+": "+error; return nullptr; }
    const auto root = factory.constructFile(ui->mTree,"panel_fs_nui_login.xml",0,error);
    if (!root) return nullptr;
    ui->mRoot = *root;
    if (!ui->mTree.prepareLayoutStacks(*root,0,error)) return nullptr;
    for (const auto name : {"login_html","username_combo","password_edit","start_location_combo","server_combo","connect_btn"})
        if (!ui->find(name)) { error = std::string("Native login is missing required control: ")+name; return nullptr; }
    LLVKControl::Callback togglePassword;
    togglePassword.function = [owner = ui.get()](auto,const LLSD&)
    {
        auto& tree = owner->tree();
        const auto password = owner->find("password_edit");
        const auto* node = tree.get(password);
        if (!node || !node->lineEditor) return;
        const bool mask = !node->lineEditor->params.text.password;
        tree.setLineEditorPassword(password,mask);
        tree.setVisible(owner->find("password_show_btn"),mask);
        tree.setVisible(owner->find("password_hide_btn"),!mask);
    };
    ui->mTree.setControlCommit(ui->find("password_show_btn"),togglePassword);
    ui->mTree.setControlCommit(ui->find("password_hide_btn"),std::move(togglePassword));
    const auto mode = configuration.settings.find("SessionSettingsFile");
    if (mode != configuration.settings.end()) ui->mTree.setValue(ui->find("mode_combo"),mode->second);
    const auto menuFiles = ui->mSkin->read("xui","menu_login.xml",LLVKSkinFiles::Policy::Current,error);
    if (!menuFiles) return nullptr;
    std::vector<std::string_view> menuLayers;
    for (const auto& file : *menuFiles) menuLayers.push_back(file);
    const auto menuXml = LLVKXmlLayers::merge(menuLayers,error);
    if (!menuXml) return nullptr;
    const auto menuFont = ui->mFonts->resolve({"SansSerif","Small"},error);
    if (!menuFont) return nullptr;
    const auto debug = configuration.settings.find("UseDebugMenus");
    ui->mMenu = LLVKLoginMenu::create(*menuXml,menuFont,ui->mColors,configuration.labels,
        debug != configuration.settings.end() && debug->second.asBoolean(),error);
    if (!ui->mMenu) return nullptr;
#ifndef OPENSIM
    for (const auto name : {"current_grid_help_login","current_grid_about_login","grid_help_seperator_login"})
        ui->mMenu->setVisible(name,false);
#endif
    ui->mDialogFactory = std::make_unique<LLVKWidgetFactory>(factory);
    if (!ui->initializeDialogs(configuration,error)) return nullptr;
    return ui;
}

std::optional<LLVKWidgetPaint> LLVKLoginUi::preparePaint(const LLVKWidgetPaint::Input& input,std::string& error)
{
    auto paint = LLVKWidgetPaint::prepare(mTree,mRoot,input,error);
    if (!paint) return std::nullopt;
    const auto viewport = mTree.screenRect(mRoot,error);
    if (!viewport || !mMenu->paint(*paint,*viewport,error)) return std::nullopt;
    return paint;
}

LLVKWidgetTree::Id LLVKLoginUi::find(std::string_view name) const
{
    const auto visit = [&](const auto& self,LLVKWidgetTree::Id id) -> LLVKWidgetTree::Id
    {
        const auto* node = mTree.get(id);
        if (!node) return 0;
        if (node->params.name == name) return id;
        for (const auto child : node->children) if (const auto found = self(self,child)) return found;
        return 0;
    };
    return visit(visit,mRoot);
}
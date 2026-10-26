/**
 * @file mkopluginmanager.cpp
 * @brief Shared-library plugin loader and protocol hook callbacks.
 */

#include "llviewerprecompiledheaders.h"

#include "mkopluginmanager.h"
#include "mko_plugin_api.h"

#include "message.h"
#include "lldir.h"
#include "llsdserialize.h"
#include "llnotificationsutil.h"
#include "fsnearbychathub.h"
#include "llchat.h"
#include "llviewercontrol.h"
#include "llviewershadermgr.h"

#include <sstream>
#include <fstream>
#include <cstdio>
#include <mutex>

#if LL_WINDOWS
#include <windows.h>
#else
#include <dlfcn.h>
#endif

MkoHostInterface MkoPluginManager::sHostInterface = { 0 };

MkoPluginManager& MkoPluginManager::instance()
{
    static MkoPluginManager inst;
    return inst;
}

MkoPluginManager::~MkoPluginManager()
{
    unloadPlugins();
}

void MkoPluginManager::init()
{
    sHostInterface.version = MKO_PLUGIN_API_VERSION;
    sHostInterface.get_name = &MkoPluginManager::hostGetName;
    sHostInterface.log = &MkoPluginManager::hostLog;
    sHostInterface.send_message = &MkoPluginManager::hostSendMessage;
    sHostInterface.app_ptr = &MkoPluginManager::hostAppPtr;
    sHostInterface.get_plugin_dir = &MkoPluginManager::hostGetPluginDir;
    sHostInterface.show_notification = &MkoPluginManager::hostShowNotification;
    sHostInterface.chat = &MkoPluginManager::hostChat;
    sHostInterface.get_setting = &MkoPluginManager::hostGetSetting;
    sHostInterface.set_setting = &MkoPluginManager::hostSetSetting;
    sHostInterface.register_setting = &MkoPluginManager::hostRegisterSetting;
    sHostInterface.register_settings_tab = &MkoPluginManager::hostRegisterSettingsTab;
    sHostInterface.register_setting2 = &MkoPluginManager::hostRegisterSetting2;
    sHostInterface.get_graphics_info = &MkoPluginManager::hostGetGraphicsInfo;
    sHostInterface.register_shader = &MkoPluginManager::hostRegisterShader;
    sHostInterface.unregister_shader = &MkoPluginManager::hostUnregisterShader;

    loadSettings();

    // Register this manager as a protocol dispatch interceptor.
    LLMessageSystem::setDispatchInterceptor(&MkoPluginManager::dispatchMessage);

    loadPlugins();

    // Forward shader override requests from the rendering pipeline.
    LLShaderMgr::setShaderSourceOverride(
        std::bind(&MkoPluginManager::getShaderSource,
                  &instance(),
                  std::placeholders::_1,
                  std::placeholders::_2));
}

void MkoPluginManager::shutdown()
{
    saveSettings();
    LLMessageSystem::setDispatchInterceptor(nullptr);
    unloadPlugins();
}

bool MkoPluginManager::dispatchMessage(const std::string& msg_name,
                                       const LLSD& message,
                                       LLHTTPNode::ResponsePtr responsep)
{
    // Internal control messages that the host handles directly.
    if (msg_name == "MkoReloadShaders")
    {
        LL_INFOS("MkoPlugin") << "Reloading shaders via plugin request" << LL_ENDL;
        LLViewerShaderMgr::instance()->setShaders();
        return true;
    }

    if (msg_name == "MkoClearShaderOverrides")
    {
        LL_INFOS("MkoPlugin") << "Clearing shader overrides via plugin request" << LL_ENDL;
        {
            MkoPluginManager& self = instance();
            std::lock_guard<std::mutex> lock(self.mShaderMutex);
            self.mShaderOverrides.clear();
        }
        LLViewerShaderMgr::instance()->setShaders();
        return true;
    }

    if (msg_name == "MkoSetSavedSetting")
    {
        if (message.has("name") && message.has("value"))
        {
            const std::string name = message["name"].asString();
            const LLSD value = message["value"];
            LL_INFOS("MkoPlugin") << "Setting gSavedSettings['" << name << "'] from plugin" << LL_ENDL;
            gSavedSettings.setUntypedValue(name, value);
        }
        return true;
    }

    MkoPluginManager& self = instance();
    if (self.mPlugins.empty())
    {
        return false;
    }

    // Convert the incoming LLSD to a notation string for C plugins.
    std::ostringstream ostr;
    LLSDSerialize::serialize(message, ostr, LLSDSerialize::LLSD_NOTATION);
    const std::string payload = ostr.str();
    const char* payload_cstr = payload.c_str();
    const char* msg_cstr = msg_name.c_str();

    for (LoadedPlugin& p : self.mPlugins)
    {
        if (!p.iface || !p.iface->on_message)
        {
            continue;
        }

        if (p.iface->on_message(msg_cstr, payload_cstr) != 0)
        {
            return true;
        }
    }

    return false;
}

void MkoPluginManager::loadPlugins()
{
    std::string plugins_dir = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins");
    if (!gDirUtilp->fileExists(plugins_dir))
    {
        LL_INFOS("MkoPlugin") << "Plugin directory not found: " << plugins_dir << LL_ENDL;
        return;
    }

    std::vector<std::string> files = gDirUtilp->getFilesInDir(plugins_dir);
    for (const std::string& f : files)
    {
        std::string::size_type dot = f.find_last_of('.');
        if (dot == std::string::npos)
        {
            continue;
        }

        std::string ext = f.substr(dot + 1);
        if (ext == "so" || ext == "dll" || ext == "dylib")
        {
            loadPlugin(gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins", f));
        }
    }
}

void MkoPluginManager::loadPlugin(const std::string& path)
{
    void* handle = nullptr;

#if LL_WINDOWS
    HMODULE hmod = LoadLibraryA(path.c_str());
    handle = reinterpret_cast<void*>(hmod);
#else
    handle = dlopen(path.c_str(), RTLD_NOW);
#endif

    if (!handle)
    {
        const char* err = "unknown";
#if !LL_WINDOWS
        err = dlerror();
#endif
        LL_WARNS("MkoPlugin") << "Failed to load plugin " << path << ": " << err << LL_ENDL;
        return;
    }

    MkoGetInterfaceFunc get_iface = nullptr;
#if LL_WINDOWS
    get_iface = reinterpret_cast<MkoGetInterfaceFunc>(
        GetProcAddress(reinterpret_cast<HMODULE>(handle), "mko_get_interface"));
#else
    get_iface = reinterpret_cast<MkoGetInterfaceFunc>(dlsym(handle, "mko_get_interface"));
#endif

    if (!get_iface)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " has no mko_get_interface" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    const MkoPluginInterface* iface = get_iface();
    if (!iface
        || iface->version != MKO_PLUGIN_API_VERSION
        || !iface->init
        || !iface->on_message)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " has an incompatible interface" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    if (iface->init(&sHostInterface) != 0)
    {
        LL_WARNS("MkoPlugin") << "Plugin " << path << " init failed" << LL_ENDL;
#if !LL_WINDOWS
        dlclose(handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(handle));
#endif
        return;
    }

    mPlugins.push_back({ handle, iface });
    LL_INFOS("MkoPlugin") << "Loaded plugin "
                          << (iface->name ? iface->name : path)
                          << LL_ENDL;
}

void MkoPluginManager::unloadPlugins()
{
    for (LoadedPlugin& p : mPlugins)
    {
        if (p.iface && p.iface->shutdown)
        {
            p.iface->shutdown();
        }

#if !LL_WINDOWS
        dlclose(p.handle);
#else
        FreeLibrary(reinterpret_cast<HMODULE>(p.handle));
#endif
    }

    mPlugins.clear();
}

const char* MkoPluginManager::hostGetName(void)
{
    return "Manikineko Online";
}

void MkoPluginManager::hostLog(MkoLogLevel level, const char* msg)
{
    if (!msg)
    {
        return;
    }

    switch (level)
    {
        case MKO_LOG_DEBUG:
            LL_DEBUGS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_WARN:
            LL_WARNS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_ERROR:
            LL_ERRS("MkoPlugin") << msg << LL_ENDL;
            break;
        case MKO_LOG_INFO:
        default:
            LL_INFOS("MkoPlugin") << msg << LL_ENDL;
            break;
    }
}

int MkoPluginManager::hostSendMessage(const char* msg_name, const char* llsd_notation)
{
    if (!msg_name || !llsd_notation)
    {
        return -1;
    }

    std::istringstream istr(llsd_notation);
    LLSD message;
    if (!LLSDSerialize::deserialize(message, istr, LLSDSerialize::SIZE_UNLIMITED))
    {
        LL_WARNS("MkoPlugin") << "Invalid LLSD from plugin for " << msg_name << LL_ENDL;
        return -1;
    }

    // Plugin messages (e.g., MkoDiscord, MkoSteamworks) are not registered
    // with the legacy message template, so route them directly through the
    // plugin manager instead of LLMessageSystem::dispatch.
    MkoPluginManager::dispatchMessage(std::string(msg_name), message, LLHTTPNode::ResponsePtr());
    return 0;
}

void* MkoPluginManager::hostAppPtr(const char* name)
{
    if (!name)
    {
        return nullptr;
    }

    if (std::string(name) == "message_system")
    {
        return static_cast<void*>(gMessageSystem);
    }

    return nullptr;
}

const char* MkoPluginManager::hostGetPluginDir(void)
{
    static std::string dir = gDirUtilp->getExpandedFilename(LL_PATH_EXECUTABLE, "plugins");
    return dir.c_str();
}

void MkoPluginManager::hostShowNotification(const char* message)
{
    if (!message) return;
    LLSD args;
    args["MESSAGE"] = std::string(message);
    LLNotificationsUtil::add("GenericAlert", args);
}

void MkoPluginManager::hostChat(const char* message, int chat_type)
{
    if (!message) return;
    EChatType type = CHAT_TYPE_NORMAL;
    if (chat_type == 0) type = CHAT_TYPE_WHISPER;
    else if (chat_type == 2) type = CHAT_TYPE_SHOUT;
    FSNearbyChat::instance().sendChatFromViewer(std::string(message), type, false);
}

void MkoPluginManager::broadcastToPlugins(const std::string& msg_name,
                                          const LLSD& message)
{
    dispatchMessage(msg_name, message, LLHTTPNode::ResponsePtr());
}

std::string MkoPluginManager::getSettingsFilePath() const
{
    return gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "mko_settings.xml");
}

void MkoPluginManager::loadSettings()
{
    std::string path = getSettingsFilePath();
    if (!gDirUtilp->fileExists(path))
    {
        return;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        LL_WARNS("MkoPlugin") << "Could not open settings file " << path << LL_ENDL;
        return;
    }

    LLSD data;
    LLSDSerialize::fromXML(data, ifs, false);
    if (data.isMap())
    {
        std::lock_guard<std::mutex> lock(mSettingsMutex);
        for (LLSD::map_const_iterator it = data.beginMap(); it != data.endMap(); ++it)
        {
            mSettings[it->first] = it->second.asString();
        }
    }
}

void MkoPluginManager::saveSettings()
{
    std::string path = getSettingsFilePath();

    LLSD data(LLSD::emptyMap());
    {
        std::lock_guard<std::mutex> lock(mSettingsMutex);
        for (const auto& kv : mSettings)
        {
            data[kv.first] = kv.second;
        }
    }

    std::ofstream ofs(path, std::ios::out | std::ios::binary);
    if (ofs.is_open())
    {
        LLSDSerialize::toXML(data, ofs);
    }
    else
    {
        LL_WARNS("MkoPlugin") << "Could not write settings file " << path << LL_ENDL;
    }
}

int MkoPluginManager::getSetting(const char* name, char* out, size_t out_len)
{
    if (!name || !out || out_len == 0) return -1;
    MkoPluginManager& self = instance();
    std::lock_guard<std::mutex> lock(self.mSettingsMutex);
    auto it = self.mSettings.find(name);
    if (it == self.mSettings.end())
    {
        return -1;
    }
    snprintf(out, out_len, "%s", it->second.c_str());
    out[out_len - 1] = '\0';
    return 0;
}

int MkoPluginManager::setSetting(const char* name, const char* value)
{
    if (!name || !value) return -1;
    MkoPluginManager& self = instance();
    {
        std::lock_guard<std::mutex> lock(self.mSettingsMutex);
        self.mSettings[name] = value;
    }
    self.saveSettings();
    return 0;
}

int MkoPluginManager::setSettingFromUI(const char* name, const char* value)
{
    int r = setSetting(name, value);
    if (r == 0)
    {
        LLSD msg;
        msg["name"] = std::string(name);
        msg["value"] = std::string(value);
        instance().broadcastToPlugins("MkoSettingChanged", msg);
    }
    return r;
}

int MkoPluginManager::registerSetting(const char* name, const char* default_value,
                                      const char* label, const char* type)
{
    MkoSettingDesc2 desc;
    desc.name = name;
    desc.default_value = default_value;
    desc.label = label;
    desc.type = type;
    desc.tab_id = nullptr;
    desc.options = nullptr;
    desc.min_value = 0.0;
    desc.max_value = 0.0;
    return registerSetting2(&desc);
}

int MkoPluginManager::registerSettingsTab(const char* id, const char* label)
{
    if (!id || !label) return -1;
    MkoPluginManager& self = instance();
    std::lock_guard<std::mutex> lock(self.mSettingsMutex);
    for (const MkoSettingsTab& tab : self.mTabs)
    {
        if (tab.id == id)
        {
            return 0; // already registered
        }
    }
    MkoSettingsTab tab;
    tab.id = id;
    tab.label = label;
    self.mTabs.push_back(tab);
    LL_INFOS("MkoPlugin") << "Registered settings tab '" << id << "' (" << label << ")" << LL_ENDL;
    return 0;
}

int MkoPluginManager::registerSetting2(const MkoSettingDesc2* desc)
{
    if (!desc || !desc->name || !desc->default_value || !desc->label || !desc->type) return -1;
    MkoPluginManager& self = instance();
    std::lock_guard<std::mutex> lock(self.mSettingsMutex);
    if (self.mSettings.find(desc->name) == self.mSettings.end())
    {
        self.mSettings[desc->name] = desc->default_value;
    }
    MkoSettingDef def;
    def.default_value = desc->default_value;
    def.label = desc->label;
    def.type = desc->type;
    def.tab_id = desc->tab_id ? desc->tab_id : "";
    def.options = desc->options ? desc->options : "";
    def.min_value = desc->min_value;
    def.max_value = desc->max_value;
    self.mRegistry[desc->name] = def;
    return 0;
}

std::vector<MkoSettingsTab> MkoPluginManager::getSettingsTabs() const
{
    std::lock_guard<std::mutex> lock(mSettingsMutex);
    std::vector<MkoSettingsTab> tabs = mTabs;
    // Always offer the default tab for legacy (tab-less) settings.
    bool has_default = false;
    for (const MkoSettingsTab& tab : tabs)
    {
        if (tab.id == "plugins") has_default = true;
    }
    if (!has_default)
    {
        MkoSettingsTab def;
        def.id = "plugins";
        def.label = "Plugins";
        tabs.push_back(def);
    }
    return tabs;
}

void MkoPluginManager::getSettingsForTab(const std::string& tab_id,
                                         std::vector<std::pair<std::string, MkoSettingDef> >& out) const
{
    std::lock_guard<std::mutex> lock(mSettingsMutex);
    for (const auto& kv : mRegistry)
    {
        std::string setting_tab = kv.second.tab_id.empty() ? "plugins" : kv.second.tab_id;
        if (setting_tab == tab_id)
        {
            out.push_back(kv);
        }
    }
}

const char* MkoPluginManager::getGraphicsInfo(void)
{
    static std::string s_info;
    LLSD info;
    info["vendor"] = std::string(gGLManager.mGLVendor);
    info["renderer"] = std::string(gGLManager.mGLRenderer);
    info["version"] = std::string(gGLManager.mGLVersionString);
    std::ostringstream ostr;
    LLSDSerialize::serialize(info, ostr, LLSDSerialize::LLSD_NOTATION);
    s_info = ostr.str();
    return s_info.c_str();
}

int MkoPluginManager::hostGetSetting(const char* name, char* out, size_t out_len)
{
    return getSetting(name, out, out_len);
}

int MkoPluginManager::hostSetSetting(const char* name, const char* value)
{
    int r = setSetting(name, value);
    if (r == 0)
    {
        LLSD msg;
        msg["name"] = std::string(name);
        msg["value"] = std::string(value);
        instance().broadcastToPlugins("MkoSettingChanged", msg);
    }
    return r;
}

int MkoPluginManager::hostRegisterSetting(const char* name, const char* default_value,
                                          const char* label, const char* type)
{
    return registerSetting(name, default_value, label, type);
}

int MkoPluginManager::hostRegisterSettingsTab(const MkoSettingsTabDesc* tab)
{
    if (!tab || !tab->id || !tab->label) return -1;
    return registerSettingsTab(tab->id, tab->label);
}

int MkoPluginManager::hostRegisterSetting2(const MkoSettingDesc2* setting)
{
    return registerSetting2(setting);
}

const char* MkoPluginManager::hostGetGraphicsInfo(void)
{
    return getGraphicsInfo();
}

int MkoPluginManager::hostRegisterShader(const MkoShaderDesc* desc)
{
    return registerShader(desc);
}

int MkoPluginManager::hostUnregisterShader(const char* name, MkoShaderType type)
{
    return unregisterShader(name, type);
}

int MkoPluginManager::unregisterShader(const char* name, MkoShaderType type)
{
    if (!name) return -1;

    GLenum gltype = 0;
    switch (type)
    {
        case MKO_SHADER_VERTEX:   gltype = GL_VERTEX_SHADER;   break;
        case MKO_SHADER_FRAGMENT: gltype = GL_FRAGMENT_SHADER; break;
        case MKO_SHADER_GEOMETRY: gltype = GL_GEOMETRY_SHADER; break;
        default:
            return -1;
    }

    std::string key = std::string(name) + ":" + std::to_string(gltype);
    MkoPluginManager& self = instance();
    {
        std::lock_guard<std::mutex> lock(self.mShaderMutex);
        self.mShaderOverrides.erase(key);
    }

    LL_INFOS("MkoPlugin") << "Unregistered shader override " << name << LL_ENDL;
    return 0;
}

int MkoPluginManager::registerShader(const MkoShaderDesc* desc)
{
    if (!desc || !desc->name || !desc->source)
    {
        return -1;
    }

    GLenum gltype = 0;
    switch (desc->type)
    {
        case MKO_SHADER_VERTEX:   gltype = GL_VERTEX_SHADER;   break;
        case MKO_SHADER_FRAGMENT: gltype = GL_FRAGMENT_SHADER; break;
        case MKO_SHADER_GEOMETRY: gltype = GL_GEOMETRY_SHADER; break;
        default:
            return -1;
    }

    std::string source;
    if (desc->defines)
    {
        source = desc->defines;
        if (!source.empty() && source.back() != '\n')
        {
            source += '\n';
        }
    }
    source += desc->source;

    std::string key = std::string(desc->name) + ":" + std::to_string(gltype);

    MkoPluginManager& self = instance();
    {
        std::lock_guard<std::mutex> lock(self.mShaderMutex);
        self.mShaderOverrides[key] = source;
    }

    LL_INFOS("MkoPlugin") << "Registered shader override " << desc->name
                          << " (type=" << gltype << ")" << LL_ENDL;
    return 0;
}

std::string MkoPluginManager::getShaderSource(const std::string& name, GLenum type) const
{
    std::string key = name + ":" + std::to_string(type);
    std::lock_guard<std::mutex> lock(mShaderMutex);
    auto it = mShaderOverrides.find(key);
    if (it != mShaderOverrides.end())
    {
        LL_DEBUGS("MkoPlugin") << "Shader override found for " << name << LL_ENDL;
        return it->second;
    }
    return std::string();
}

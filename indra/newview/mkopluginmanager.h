/**
 * @file mkopluginmanager.h
 * @brief DLL plugin loader and protocol dispatch hooks.
 *
 * Loads shared libraries from <viewer-binary-dir>/plugins at startup and
 * registers them as interceptors on the LLMessageSystem dispatch path.
 */

#ifndef MKO_PLUGIN_MANAGER_H
#define MKO_PLUGIN_MANAGER_H

#include <vector>
#include <string>
#include <map>
#include <mutex>

#include "llsd.h"
#include "llhttpnode.h"
#include "llgl.h"
#include "llshadermgr.h"
#include "mko_plugin_api.h"

struct MkoSettingDef
{
    std::string label;
    std::string type;
    std::string default_value;
    std::string tab_id;   /* empty = default "plugins" tab */
    std::string options;  /* pipe-separated enum options */
    double min_value = 0.0;
    double max_value = 0.0;
};

struct MkoSettingsTab
{
    std::string id;
    std::string label;
};

class MkoPluginManager
{
public:
    static MkoPluginManager& instance();

    void init();
    void shutdown();

    static bool dispatchMessage(const std::string& msg_name,
                                const LLSD& message,
                                LLHTTPNode::ResponsePtr responsep);

    // Broadcast a message to all loaded protocol plugins.
    void broadcastToPlugins(const std::string& msg_name, const LLSD& message);

    // Plugin-accessible settings (Preferences > Graphics > plugin tabs).
    static int getSetting(const char* name, char* out, size_t out_len);
    static int setSetting(const char* name, const char* value);
    static int registerSetting(const char* name, const char* default_value,
                               const char* label, const char* type);
    static int registerSettingsTab(const char* id, const char* label);
    static int registerSetting2(const MkoSettingDesc2* setting);
    static const char* getGraphicsInfo(void);

    // UI entry points (used by the Preferences plugin-settings panel).
    static int setSettingFromUI(const char* name, const char* value);
    std::vector<MkoSettingsTab> getSettingsTabs() const;
    void getSettingsForTab(const std::string& tab_id,
                           std::vector<std::pair<std::string, MkoSettingDef> >& out) const;

    // Shader override registration (called from the plugin API).
    static int registerShader(const MkoShaderDesc* desc);
    static int unregisterShader(const char* name, MkoShaderType type);
    std::string getShaderSource(const std::string& name, GLenum type) const;

private:
    MkoPluginManager() = default;
    ~MkoPluginManager();

    MkoPluginManager(const MkoPluginManager&) = delete;
    MkoPluginManager& operator=(const MkoPluginManager&) = delete;

    struct LoadedPlugin
    {
        void* handle;
        const MkoPluginInterface* iface;
    };

    void loadPlugins();
    void loadPlugin(const std::string& path);
    void unloadPlugins();

    std::vector<LoadedPlugin> mPlugins;

    static MkoHostInterface sHostInterface;

    // Host service callbacks passed to each plugin.
    static const char* hostGetName(void);
    static void hostLog(MkoLogLevel level, const char* msg);
    static int hostSendMessage(const char* msg_name, const char* llsd_notation);
    static void* hostAppPtr(const char* name);
    static const char* hostGetPluginDir(void);
    static void hostShowNotification(const char* message);
    static void hostChat(const char* message, int chat_type);
    static int hostGetSetting(const char* name, char* out, size_t out_len);
    static int hostSetSetting(const char* name, const char* value);
    static int hostRegisterSetting(const char* name, const char* default_value,
                                   const char* label, const char* type);
    static int hostRegisterSettingsTab(const MkoSettingsTabDesc* tab);
    static int hostRegisterSetting2(const MkoSettingDesc2* setting);
    static const char* hostGetGraphicsInfo(void);
    static int hostRegisterShader(const MkoShaderDesc* desc);
    static int hostUnregisterShader(const char* name, MkoShaderType type);

    std::string getSettingsFilePath() const;
    void loadSettings();
    void saveSettings();

    std::map<std::string, std::string> mSettings;
    std::map<std::string, MkoSettingDef> mRegistry;
    std::vector<MkoSettingsTab> mTabs;
    mutable std::mutex mSettingsMutex;

    std::map<std::string, std::string> mShaderOverrides;
    mutable std::mutex mShaderMutex;
};

#endif /* MKO_PLUGIN_MANAGER_H */

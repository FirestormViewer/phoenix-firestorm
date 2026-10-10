/**
 * @file mkolua.cpp
 * @brief Optional Manikineko Lua plugin add-on.
 *
 * Build this as libmkolua.so and place it in <viewer-exe>/plugins.
 * It loads every *.lua file in the same directory, exposes a global
 * `mko` table, and lets scripts register an on_message callback.
 *
 * Requires a Lua development package (e.g. liblua5.4-dev).
 */

#include "mko_plugin_api.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

#include <vector>
#include <string>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif

static const MkoHostInterface* sHost = nullptr;
static std::vector<lua_State*> sStates;

static const char* mkolua_get_version(void) { return "1.0"; }

// ------------------------------------------------------------------
// Lua-bound host services
// ------------------------------------------------------------------

static int l_log(lua_State* L)
{
    if (!sHost) return 0;
    MkoLogLevel level = (MkoLogLevel)luaL_checkinteger(L, 1);
    const char* msg = luaL_checkstring(L, 2);
    sHost->log(level, msg);
    return 0;
}

static int l_send_message(lua_State* L)
{
    if (!sHost)
    {
        lua_pushinteger(L, -1);
        return 1;
    }
    const char* name = luaL_checkstring(L, 1);
    const char* body = luaL_checkstring(L, 2);
    int r = sHost->send_message(name, body);
    lua_pushinteger(L, r);
    return 1;
}

static int l_get_name(lua_State* L)
{
    if (!sHost)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, sHost->get_name());
    return 1;
}

static int l_get_plugin_dir(lua_State* L)
{
    if (!sHost)
    {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, sHost->get_plugin_dir());
    return 1;
}

static int l_show_notification(lua_State* L)
{
    if (!sHost) return 0;
    const char* msg = luaL_checkstring(L, 1);
    sHost->show_notification(msg);
    return 0;
}

static int l_chat(lua_State* L)
{
    if (!sHost) return 0;
    const char* msg = luaL_checkstring(L, 1);
    int chat_type = (int)luaL_optinteger(L, 2, 1);  /* 1 = normal */
    sHost->chat(msg, chat_type);
    return 0;
}

static int l_get_setting(lua_State* L)
{
    if (!sHost) return 0;
    const char* name = luaL_checkstring(L, 1);
    char buf[4096];
    if (sHost->get_setting(name, buf, sizeof(buf)) == 0)
    {
        lua_pushstring(L, buf);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

static int l_set_setting(lua_State* L)
{
    if (!sHost) return 0;
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);
    int r = sHost->set_setting(name, value);
    lua_pushboolean(L, r == 0);
    return 1;
}

static int l_register_setting(lua_State* L)
{
    if (!sHost) return 0;
    const char* name = luaL_checkstring(L, 1);
    const char* default_value = luaL_checkstring(L, 2);
    const char* label = luaL_checkstring(L, 3);
    const char* type = luaL_checkstring(L, 4);

    if (lua_gettop(L) > 4)
    {
        // Extended form:
        //   register_setting(name, default, label, type[, tab_id[, options[, min, max]]])
        MkoSettingDesc2 desc;
        desc.name = name;
        desc.default_value = default_value;
        desc.label = label;
        desc.type = type;
        desc.tab_id = lua_isstring(L, 5) ? lua_tostring(L, 5) : nullptr;
        desc.options = lua_isstring(L, 6) ? lua_tostring(L, 6) : nullptr;
        desc.min_value = (lua_gettop(L) >= 7 && lua_isnumber(L, 7)) ? lua_tonumber(L, 7) : 0.0;
        desc.max_value = (lua_gettop(L) >= 8 && lua_isnumber(L, 8)) ? lua_tonumber(L, 8) : 0.0;
        int r = sHost->register_setting2(&desc);
        lua_pushboolean(L, r == 0);
        return 1;
    }

    int r = sHost->register_setting(name, default_value, label, type);
    lua_pushboolean(L, r == 0);
    return 1;
}

static int l_register_settings_tab(lua_State* L)
{
    if (!sHost) return 0;
    const char* id = luaL_checkstring(L, 1);
    const char* label = luaL_checkstring(L, 2);
    MkoSettingsTabDesc tab;
    tab.id = id;
    tab.label = label;
    int r = sHost->register_settings_tab(&tab);
    lua_pushboolean(L, r == 0);
    return 1;
}

static int l_get_graphics_info(lua_State* L)
{
    if (!sHost || !sHost->get_graphics_info)
    {
        lua_pushnil(L);
        return 1;
    }
    const char* info = sHost->get_graphics_info();
    lua_pushstring(L, info ? info : "");
    return 1;
}

static int l_register_on_message(lua_State* L)
{
    if (!lua_isfunction(L, 1))
    {
        return luaL_error(L, "mko.register_on_message: expected a function");
    }

    lua_pushvalue(L, 1);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    lua_pushnumber(L, ref);
    lua_setglobal(L, "__mko_on_message_ref");
    return 0;
}

static int l_register_shader(lua_State* L)
{
    if (!sHost)
    {
        lua_pushboolean(L, false);
        return 1;
    }

    const char* name = luaL_checkstring(L, 1);

    MkoShaderType type = MKO_SHADER_FRAGMENT;
    if (lua_isnumber(L, 2))
    {
        int t = (int)lua_tointeger(L, 2);
        if (t >= 0 && t < MKO_SHADER_COUNT)
        {
            type = (MkoShaderType)t;
        }
    }
    else if (lua_isstring(L, 2))
    {
        const char* ts = lua_tostring(L, 2);
        if (strcmp(ts, "vertex") == 0 || strcmp(ts, "v") == 0 || strcmp(ts, "vert") == 0)
        {
            type = MKO_SHADER_VERTEX;
        }
        else if (strcmp(ts, "geometry") == 0 || strcmp(ts, "g") == 0 || strcmp(ts, "geom") == 0)
        {
            type = MKO_SHADER_GEOMETRY;
        }
        else
        {
            type = MKO_SHADER_FRAGMENT;
        }
    }

    const char* source = luaL_checkstring(L, 3);
    const char* defines = luaL_optstring(L, 4, NULL);

    MkoShaderDesc desc;
    desc.name = name;
    desc.type = type;
    desc.source = source;
    desc.defines = defines;

    int r = sHost->register_shader(&desc);
    lua_pushboolean(L, r == 0);
    return 1;
}

static void register_mko_table(lua_State* L)
{
    lua_newtable(L);
    lua_pushcfunction(L, l_log);          lua_setfield(L, -2, "log");
    lua_pushcfunction(L, l_send_message); lua_setfield(L, -2, "send_message");
    lua_pushcfunction(L, l_get_name);     lua_setfield(L, -2, "get_name");
    lua_pushcfunction(L, l_get_plugin_dir); lua_setfield(L, -2, "get_plugin_dir");
    lua_pushcfunction(L, l_show_notification); lua_setfield(L, -2, "show_notification");
    lua_pushcfunction(L, l_chat);         lua_setfield(L, -2, "chat");
    lua_pushcfunction(L, l_get_setting);  lua_setfield(L, -2, "get_setting");
    lua_pushcfunction(L, l_set_setting);  lua_setfield(L, -2, "set_setting");
    lua_pushcfunction(L, l_register_setting); lua_setfield(L, -2, "register_setting");
    lua_pushcfunction(L, l_register_settings_tab); lua_setfield(L, -2, "register_settings_tab");
    lua_pushcfunction(L, l_get_graphics_info); lua_setfield(L, -2, "get_graphics_info");
    lua_pushcfunction(L, l_register_on_message); lua_setfield(L, -2, "register_on_message");
    lua_pushcfunction(L, l_register_shader); lua_setfield(L, -2, "register_shader");
    lua_setglobal(L, "mko");
}

// ------------------------------------------------------------------

static bool load_lua_script(lua_State* L, const std::string& path)
{
    if (luaL_loadfile(L, path.c_str()) != 0)
    {
        if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (lua_pcall(L, 0, 0, 0) != 0)
    {
        if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

static void scan_and_load_scripts(const char* dir)
{
    if (!dir) return;

    std::string prefix = dir;
    if (!prefix.empty() && prefix.back() != '/' && prefix.back() != '\\')
    {
        prefix += '/';
    }

#if defined(_WIN32)
    // Windows implementation left as future exercise.
    (void)dir;
#else
    DIR* d = opendir(dir);
    if (!d)
    {
        if (sHost) sHost->log(MKO_LOG_INFO, "mkolua: no plugin directory; not loading scripts");
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        const char* name = entry->d_name;
        size_t len = std::strlen(name);
        if (len > 4 && std::strcmp(name + len - 4, ".lua") == 0)
        {
            std::string path = prefix + name;
            lua_State* L = luaL_newstate();
            if (!L) continue;

            luaL_openlibs(L);
            register_mko_table(L);

            if (load_lua_script(L, path))
            {
                sStates.push_back(L);
                if (sHost)
                {
                    std::string msg = "mkolua: loaded " + path;
                    sHost->log(MKO_LOG_INFO, msg.c_str());
                }
            }
            else
            {
                lua_close(L);
            }
        }
    }
    closedir(d);
#endif
}

// ------------------------------------------------------------------
// MkoPluginInterface callbacks
// ------------------------------------------------------------------

static int mkolua_init(const MkoHostInterface* host)
{
    sHost = host;
    if (!sHost || sHost->version != MKO_PLUGIN_API_VERSION)
    {
        if (sHost) sHost->log(MKO_LOG_ERROR, "mkolua: host API version mismatch");
        return -1;
    }

    scan_and_load_scripts(sHost->get_plugin_dir());
    return 0;
}

static void mkolua_shutdown(void)
{
    for (lua_State* L : sStates)
    {
        lua_close(L);
    }
    sStates.clear();
    sHost = nullptr;
}

static int mkolua_on_message(const char* msg_name, const char* llsd_notation)
{
    if (!sHost || sStates.empty()) return 0;

    for (lua_State* L : sStates)
    {
        lua_getglobal(L, "__mko_on_message_ref");
        if (!lua_isnumber(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        int ref = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
        if (!lua_isfunction(L, -1))
        {
            lua_pop(L, 1);
            continue;
        }

        lua_pushstring(L, msg_name);
        lua_pushstring(L, llsd_notation);

        if (lua_pcall(L, 2, 1, 0) == 0)
        {
            int handled = lua_toboolean(L, -1) ? 1 : 0;
            lua_pop(L, 1);
            if (handled) return 1;
        }
        else
        {
            if (sHost) sHost->log(MKO_LOG_WARN, lua_tostring(L, -1));
            lua_pop(L, 1);
        }
    }

    return 0;
}

// ------------------------------------------------------------------

extern "C" MKO_PLUGIN_EXPORT const MkoPluginInterface* mko_get_interface(void)
{
    static MkoPluginInterface sIface = { 0 };
    if (sIface.version == 0)
    {
        sIface.version = MKO_PLUGIN_API_VERSION;
        sIface.name = "mkolua";
        sIface.get_version = mkolua_get_version;
        sIface.init = mkolua_init;
        sIface.shutdown = mkolua_shutdown;
        sIface.on_message = mkolua_on_message;
    }
    return &sIface;
}

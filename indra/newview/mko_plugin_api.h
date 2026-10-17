/**
 * @file mko_plugin_api.h
 * @brief Manikineko Online protocol plugin C API.
 *
 * A protocol-level plugin is a shared library (.so / .dll / .dylib) that
 * exports a single symbol: mko_get_interface(). The viewer loads the
 * library at startup, calls init(), then forwards every dispatched message
 * to on_message(). Returning a non-zero value from on_message() marks the
 * message as handled and skips the default viewer handler.
 *
 * The host interface provides basic services (logging, local message
 * dispatch, and future Lua access points).
 */

#ifndef MKO_PLUGIN_API_H
#define MKO_PLUGIN_API_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
  #define MKO_PLUGIN_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
  #define MKO_PLUGIN_EXPORT __attribute__((visibility("default")))
#else
  #define MKO_PLUGIN_EXPORT
#endif

#define MKO_PLUGIN_API_VERSION 7

typedef enum
{
    MKO_LOG_DEBUG,
    MKO_LOG_INFO,
    MKO_LOG_WARN,
    MKO_LOG_ERROR
} MkoLogLevel;

typedef enum
{
    MKO_SHADER_VERTEX,
    MKO_SHADER_FRAGMENT,
    MKO_SHADER_GEOMETRY,
    MKO_SHADER_COUNT
} MkoShaderType;

typedef struct
{
    const char* name;            /* e.g. "deferred/diffuseV"                 */
    MkoShaderType type;          /* vertex / fragment / geometry             */
    const char* source;          /* full GLSL source for this stage          */
    const char* defines;         /* optional "#define FOO 1\n#define BAR 2"   */
} MkoShaderDesc;

/* A settings tab a plugin can add to the viewer's Preferences dialog
 * (installed under the Graphics preference panel). Tabs are keyed by a
 * stable id; the label is shown to the user. */
typedef struct
{
    const char* id;              /* stable tab id, e.g. "rtx"                 */
    const char* label;           /* display label, e.g. "RTX"                 */
} MkoSettingsTabDesc;

/* Richer setting descriptor. Extends the legacy 4-argument
 * register_setting() with tab placement, enum options and slider bounds.
 * Supported types: "boolean", "string", "float", "integer", "enum". */
typedef struct
{
    const char* name;            /* setting key, e.g. "MkoRtxQuality"         */
    const char* default_value;  /* persisted as a string                     */
    const char* label;           /* user-visible label                        */
    const char* type;            /* boolean | string | float | integer | enum */
    const char* tab_id;          /* settings tab id, NULL = default "plugins" */
    const char* options;         /* enum only: pipe-separated, e.g. "Low|High" */
    double min_value;            /* float/integer only: lower bound            */
    double max_value;            /* float/integer only: upper bound            */
} MkoSettingDesc2;

/* Forward declarations */
typedef struct MkoHostInterface MkoHostInterface;
typedef struct MkoPluginInterface MkoPluginInterface;

/* Services the viewer exposes to every plugin. */
struct MkoHostInterface
{
    int version;                       /* Set to MKO_PLUGIN_API_VERSION. */
    const char* (*get_name)(void);     /* Returns the viewer's display name. */
    void (*log)(MkoLogLevel level, const char* msg);
    int (*send_message)(const char* msg_name, const char* llsd_notation);
    void* (*app_ptr)(const char* name);
    const char* (*get_plugin_dir)(void); /* Absolute path to the plugin dir. */
    void (*show_notification)(const char* message);
    void (*chat)(const char* message, int chat_type);
    int (*get_setting)(const char* name, char* out, size_t out_len);
    int (*set_setting)(const char* name, const char* value);
    int (*register_setting)(const char* name, const char* default_value, const char* label, const char* type);
    int (*register_settings_tab)(const MkoSettingsTabDesc* tab);
    int (*register_setting2)(const MkoSettingDesc2* setting);
    const char* (*get_graphics_info)(void); /* LLSD notation: vendor/renderer/glsl_version */
    int (*register_shader)(const MkoShaderDesc* desc);
    int (*unregister_shader)(const char* name, MkoShaderType type);
};

/* Plugin entry point. A plugin must export a function named
 * mko_get_interface with this exact signature. */
struct MkoPluginInterface
{
    int version;                       /* Set to MKO_PLUGIN_API_VERSION. */
    const char* name;
    const char* (*get_version)(void);
    int (*init)(const MkoHostInterface* host);
    void (*shutdown)(void);
    /* Return a non-zero value to mark the message as handled. */
    int (*on_message)(const char* msg_name, const char* llsd_notation);
};

typedef const MkoPluginInterface* (*MkoGetInterfaceFunc)(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MKO_PLUGIN_API_H */

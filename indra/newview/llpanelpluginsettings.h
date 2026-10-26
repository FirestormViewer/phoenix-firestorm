/**
 * @file llpanelpluginsettings.h
 * @brief Builds the plugin-provided settings UI inside Preferences > Graphics.
 *
 * <Mko> Plugins register settings tabs and setting descriptors through the
 * MkoPluginManager host interface; this panel turns that registry into live
 * widgets. Changing a widget persists the value through the plugin manager
 * and broadcasts "MkoSettingChanged" to all loaded plugins.
 */

#ifndef LL_PANELPLUGINSETTINGS_H
#define LL_PANELPLUGINSETTINGS_H

#include "llpanel.h"

class LLPanelPluginSettings
{
public:
    // Populates an (initially empty) container panel from the plugin
    // settings registry. Safe to call multiple times; clears the container
    // first. Does nothing when no plugins registered settings.
    static void populate(LLPanel* container);
};

#endif // LL_PANELPLUGINSETTINGS_H

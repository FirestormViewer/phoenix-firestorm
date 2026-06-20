/**
 * @file sspanelprefs.cpp
 * @brief Soapstorm-specific preferences panel
 *
 * $LicenseInfo:firstyear=2024&license=fsviewerlgpl$
 * Soapstorm Viewer Source Code
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "sspanelprefs.h"

static LLPanelInjector<SSPanelPrefs> t_pref_ss("panel_preference_soapstorm");

SSPanelPrefs::SSPanelPrefs() : LLPanelPreference()
{
}

bool SSPanelPrefs::postBuild()
{
    return LLPanelPreference::postBuild();
}

void SSPanelPrefs::apply()
{
    LLPanelPreference::apply();
}

void SSPanelPrefs::cancel(const std::vector<std::string> settings_to_skip)
{
    LLPanelPreference::cancel(settings_to_skip);
}

/**
 * @file sspanelprefs.h
 * @brief Soapstorm-specific preferences panel
 *
 * $LicenseInfo:firstyear=2024&license=fsviewerlgpl$
 * Soapstorm Viewer Source Code
 * $/LicenseInfo$
 */

#ifndef SS_PANELPREFS_H
#define SS_PANELPREFS_H

#include "llfloaterpreference.h"

class SSPanelPrefs : public LLPanelPreference
{
public:
    SSPanelPrefs();
    virtual ~SSPanelPrefs() {}

    bool postBuild() override;
    void apply() override;
    void cancel(const std::vector<std::string> settings_to_skip = {}) override;
};

#endif // SS_PANELPREFS_H

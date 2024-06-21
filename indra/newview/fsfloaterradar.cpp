/**
 * @file fsfloaterradar.cpp
 * @brief Firestorm radar floater implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterradar.h"

FSFloaterRadar::FSFloaterRadar(const LLSD& key)
    :   LLFloater(key),
        mRadarPanel(nullptr)
{
}

FSFloaterRadar::~FSFloaterRadar()
{
}

bool FSFloaterRadar::postBuild()
{
    mRadarPanel = findChild<FSPanelRadar>("panel_radar");
    if (!mRadarPanel)
    {
        return false;
    }
    mRadarPanel->setVisibleCheckFunction(boost::bind(&FSFloaterRadar::getVisible, this));

    return true;
}

// virtual
void FSFloaterRadar::onOpen(const LLSD& key)
{
    // Fill radar with most recent data so we don't have a blank window until next radar update
    mRadarPanel->requestUpdate();
    LLFloater::onOpen(key);
}


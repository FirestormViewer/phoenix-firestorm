/**
* @file alfloaterregiontracker.cpp
* @brief Region tracking floater
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Alchemy Viewer Source Code
* Copyright (C) 2014, Alchemy Viewer Project.
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
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "alfloaterregiontracker.h"

// library
#include "llbutton.h"
#include "lldir.h"
#include "llfile.h"
#include "llscrolllistctrl.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdserialize_xml.h"
#include "lltextbox.h"

// newview
#include "llagent.h"
#include "llfloaterworldmap.h"
#include "llfloaterreg.h"
#include "llnotificationsutil.h"
#include "llviewermessage.h"
#include "llworldmap.h"
#include "llworldmapmessage.h"

const std::string TRACKER_FILE = "tracked_regions.json";
const F64 REGION_UPDATE_TIMER = 60.0;

ALFloaterRegionTracker::ALFloaterRegionTracker(const LLSD& key)
    : LLFloater(key),
      LLEventTimer(5.f),
      mRefreshRegionListBtn(NULL),
      mRemoveRegionBtn(NULL),
      mOpenMapBtn(NULL),
      mRegionScrollList(NULL),
      mLastRegionUpdate(0.0)
{
    loadFromJSON();
}

ALFloaterRegionTracker::~ALFloaterRegionTracker()
{
    saveToJSON();
}

BOOL ALFloaterRegionTracker::postBuild()
{
    mRefreshRegionListBtn = getChild<LLButton>("refresh");
    mRefreshRegionListBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::refresh, this));

    mRemoveRegionBtn = getChild<LLButton>("remove");
    mRemoveRegionBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::removeRegions, this));

    mOpenMapBtn = getChild<LLButton>("open_map");
    mOpenMapBtn->setClickedCallback(boost::bind(&ALFloaterRegionTracker::openMap, this));

    mRegionScrollList = getChild<LLScrollListCtrl>("region_list");
    mRegionScrollList->setCommitOnSelectionChange(TRUE);
    mRegionScrollList->setCommitCallback(boost::bind(&ALFloaterRegionTracker::updateHeader, this));
    mRegionScrollList->setDoubleClickCallback(boost::bind(&ALFloaterRegionTracker::openMap, this));

    updateHeader();

    return LLFloater::postBuild();
}

void ALFloaterRegionTracker::onOpen(const LLSD& key)
{
    requestRegionData();
    mEventTimer.start();
    refresh();
}

void ALFloaterRegionTracker::onClose(bool app_quitting)
{
    mEventTimer.stop();
}

void ALFloaterRegionTracker::updateHeader()
{
    S32 num_selected(mRegionScrollList->getNumSelected());
    mRefreshRegionListBtn->setEnabled(mRegionMap.size() != 0);
    mRemoveRegionBtn->setEnabled(!!num_selected);
    mOpenMapBtn->setEnabled(num_selected <= 1);
}

void ALFloaterRegionTracker::refresh()
{
    if (!mRegionMap.size())
    {
        updateHeader();
        return;
    }

    const std::string& saved_selected_value = mRegionScrollList->getSelectedValue().asString();
    S32 saved_scroll_pos = mRegionScrollList->getScrollPos();
    mRegionScrollList->deleteAllItems();

    const std::string& cur_region_name = gAgent.getRegion() ? gAgent.getRegion()->getName() : LLStringUtil::null;

    F64 time_now = LLTimer::getElapsedSeconds();
    bool request_region_update = (time_now - mLastRegionUpdate > REGION_UPDATE_TIMER);
    if (request_region_update)
    {
        mLastRegionUpdate = time_now;
    }

    for (LLSD::map_const_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
    {
        const std::string& sim_name = it->first;
        const LLSD& data = it->second;
        if (data.isMap()) // Assume the rest is correct.
        {
            LLScrollListCell::Params label;
            LLScrollListCell::Params maturity;
            LLScrollListCell::Params region;
            LLScrollListCell::Params count;
            label.column("region_label").type("text").value(data["label"].asString());
            maturity.column("region_maturity_icon").type("icon").font_halign(LLFontGL::HCENTER);
            region.column("region_name").type("text").value(sim_name);
            count.column("region_agent_count").type("text").value("...");
            if (LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromName(sim_name))
            {
                maturity.value(info->getAccessIcon());

                info->updateAgentCount(LLTimer::getElapsedSeconds());
                S32 agent_count = info->getAgentCount();
                if (info->isDown())
                {
                    label.color(LLColor4::red);
                    maturity.color(LLColor4::red);
                    region.color(LLColor4::red);
                    count.color(LLColor4::red);
                    count.value(0);
                }
                else
                {
                    count.value((sim_name == cur_region_name) ? agent_count + 1 : agent_count);
                }

                if (request_region_update)
                {
                    LLWorldMapMessage::getInstance()->sendNamedRegionRequest(sim_name);
                }
            }
            else
            {
                label.color(LLColor4::grey);
                maturity.color(LLColor4::grey);
                region.color(LLColor4::grey);
                count.color(LLColor4::grey);

                LLWorldMapMessage::getInstance()->sendNamedRegionRequest(sim_name);
                if (!mEventTimer.getStarted()) mEventTimer.start();
            }
            LLScrollListItem::Params row;
            row.value = sim_name;
            row.columns.add(label);
            row.columns.add(maturity);
            row.columns.add(region);
            row.columns.add(count);
            mRegionScrollList->addRow(row);
        }
    }
    if (!saved_selected_value.empty())
    {
        mRegionScrollList->selectByValue(saved_selected_value);
    }
    mRegionScrollList->setScrollPos(saved_scroll_pos);
}

BOOL ALFloaterRegionTracker::tick()
{
    refresh();
    return FALSE;
}

void ALFloaterRegionTracker::requestRegionData()
{
    if (!mRegionMap.size())
        return;

    for (LLSD::map_const_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
    {
        const std::string& name = it->first;
        if (LLSimInfo* info = LLWorldMap::getInstance()->simInfoFromName(name))
        {
            info->updateAgentCount(LLTimer::getElapsedSeconds());
        }
        else
        {
            LLWorldMapMessage::getInstance()->sendNamedRegionRequest(name);
        }
    }
    mEventTimer.start();
}

void ALFloaterRegionTracker::removeRegions()
{
    typedef std::vector<LLScrollListItem*> item_t;
    item_t items = mRegionScrollList->getAllSelected();
    for (item_t::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        mRegionMap.erase((*it)->getValue().asString());
    }
    mRegionScrollList->deleteSelectedItems();
    saveToJSON();
    updateHeader();
}

bool ALFloaterRegionTracker::saveToJSON()
{
    const std::string& filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, TRACKER_FILE);
    llofstream out_file;
    out_file.open(filename.c_str());
    if (out_file.is_open())
    {
        LLSDSerialize::toPrettyNotation(mRegionMap, out_file);
        out_file.close();
        return true;
    }
    return false;
}

bool ALFloaterRegionTracker::loadFromJSON()
{
    const std::string& filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, TRACKER_FILE);
    llifstream in_file;
    in_file.open(filename.c_str());
    if (in_file.is_open())
    {
        LLSDSerialize::fromNotation(mRegionMap, in_file, LLSDSerialize::SIZE_UNLIMITED);
        in_file.close();
        return true;
    }
    return false;
}

std::string ALFloaterRegionTracker::getRegionLabelIfExists(const std::string& name)
{
    return mRegionMap.get(name)["label"].asString();
}

void ALFloaterRegionTracker::onRegionAddedCallback(const LLSD& notification, const LLSD& response)
{
    const S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option == 0)
    {
        const std::string& name = notification["payload"]["name"].asString();
        std::string label = response["label"].asString();
        LLStringUtil::trim(label);
        if (!name.empty() && !label.empty())
        {
            if (mRegionMap.has(name))
            {
                for (LLSD::map_iterator it = mRegionMap.beginMap(); it != mRegionMap.endMap(); it++)
                    if (it->first == name) it->second["label"] = label;
            }
            else
            {
                LLSD region;
                region["label"] = label;
                mRegionMap.insert(name, region);
            }
            saveToJSON();
            refresh();
        }
    }
}

void ALFloaterRegionTracker::openMap()
{
    if (mRegionScrollList->getNumSelected() == 0)
    {
        LLFloaterReg::showInstance("world_map", "center");
    }
    else
    {
        const std::string& region = mRegionScrollList->getFirstSelected()->getValue().asString();
        LLFloaterWorldMap* worldmap_floaterp = LLFloaterWorldMap::getInstance();
        if (!region.empty() && worldmap_floaterp)
        {
            worldmap_floaterp->trackURL(region, 128, 128, 0);
            LLFloaterReg::showInstance("world_map", "center");
        }
    }
}

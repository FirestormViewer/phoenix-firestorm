/**
 * @file fsslurlcommand.cpp
 * @brief SLurl command handler for Firestorm commands
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (c) 2012 Ansariel Hiller @ Second Life
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
#include "fsslurlcommand.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llcommandhandler.h"
#include "llfloatersettingsdebug.h"
#include "llgroupactions.h"
#include "llgroupmgr.h"
#include "lllogchat.h"
#include "llnotificationsutil.h"
#include "llspeakers.h"

#include "fsfloaterimcontainer.h"
#include "fsfloaterim.h"

class FSSlurlCommandHandler : public LLCommandHandler
{
public:
    // not allowed from outside the app
    FSSlurlCommandHandler() : LLCommandHandler("firestorm", UNTRUSTED_BLOCK) { }

    bool handle(const LLSD& params, const LLSD& query_map, const std::string& grid, LLMediaCtrl* web)
    {
        if (params.size() < 2)
        {
            return false;
        }

        LLUUID target_id;
        if (!target_id.set(params[0], false))
        {
            return false;
        }

        const std::string verb = params[1].asString();

        if (verb == "zoom")
        {
            if (LLAvatarActions::canZoomIn(target_id))
            {
                LLAvatarActions::zoomIn(target_id);
                return true;
            }

            LLNotificationsUtil::add("ZoomToAvatarNotPossible");
            return true;
        }

        if (verb == "offerteleport")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::offerTeleport(target_id);
            }

            return true;
        }

        if (verb == "requestteleport")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::teleportRequest(target_id);
            }

            return true;
        }

        if (verb == "teleportto")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::teleportTo(target_id);
            }

            return true;
        }

        if (verb == "track")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::track(target_id);
            }

            return true;
        }

        if (verb == "addtocontactset")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::addToContactSet(target_id);
            }
            return true;
        }

        if (verb == "blockavatar")
        {
            if (gAgentID != target_id)
            {
                LLAvatarActions::toggleBlock(target_id);
            }
            return true;
        }

        if (verb == "viewlog")
        {
            if (gAgentID != target_id && LLLogChat::isTranscriptExist(target_id))
            {
                LLAvatarActions::viewChatHistory(target_id);
            }
            return true;
        }

        if (verb == "groupjoin")
        {
            LLGroupActions::join(target_id);
            return true;
        }

        if (verb == "groupleave")
        {
            LLGroupActions::leave(target_id);
            return true;
        }

        if (verb == "groupactivate")
        {
            LLGroupActions::activate(target_id);
            return true;
        }

        // please someone tell me there is an easier way to get the group UUID of the
        // currently focused group, or transfer the UUID through the context menu somehow - Zi
        LLFloater* focused_floater = gFloaterView->getFocusedFloater();

        FSFloaterIM* floater_im;

        // let's guess that the user has tabbed IMs
        FSFloaterIMContainer* floater_container = dynamic_cast<FSFloaterIMContainer*>(focused_floater);

        if (floater_container)
        {
            // get the active sub-floater inside the tabbed IMs
            floater_im = dynamic_cast<FSFloaterIM*>(floater_container->getActiveFloater());
        }
        else
        {
            // no tabbed IMs or torn-off group floater, try if this is already what we wanted
            floater_im = dynamic_cast<FSFloaterIM*>(focused_floater);
        }

        // still not an IM floater, give up
        if (!floater_im)
        {
            return true;
        }

        // get the group's UUID
        LLUUID group_id = floater_im->getKey();

        // is this actually a valid IM session?
        LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(group_id);
        if (!session)
        {
            return true;
        }

        // is this actually a valid group IM session?
        if (!session->isGroupSessionType())
        {
            return true;
        }

        // finally! let's see what we came here to do

        if (verb == "groupchatallow")
        {
            // no safety checks, just allow
            LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(group_id);
            speaker_mgr->allowTextChat(target_id, true);

            return true;
        }

        if (verb == "groupchatforbid")
        {
            // no safety checks, just mute
            LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(group_id);
            speaker_mgr->allowTextChat(target_id, false);

            return true;
        }

        if (verb == "groupeject")
        {
            // no safety checks, just eject
            LLGroupActions::ejectFromGroup(group_id, target_id);
            return true;
        }

        if (verb == "groupban")
        {
            std::vector<LLUUID> ids;
            ids.push_back(target_id);

            // no safety checks, just ban
            LLGroupMgr::getInstance()->sendGroupBanRequest(LLGroupMgr::REQUEST_POST, group_id, LLGroupMgr::BAN_CREATE, ids);
            LLGroupMgr::getInstance()->sendGroupMemberEjects(group_id, ids);
            LLGroupMgr::getInstance()->sendGroupMembersRequest(group_id);

            return true;
        }

        return false;
    }
};

// Creating the object registers with the dispatcher.
FSSlurlCommandHandler gFSSlurlHandler;

class FSHelpSlurlCommandHandler : public LLCommandHandler
{
public:
    // not allowed from outside the app
    FSHelpSlurlCommandHandler() : LLCommandHandler("fshelp", UNTRUSTED_THROTTLE) { }

    bool handle(const LLSD& params, const LLSD& query_map, const std::string& grid, LLMediaCtrl* web)
    {
        if (params.size() < 2)
        {
            return false;
        }

        if (params[0].asString() == "showdebug")
        {
            std::string setting_name = params[1].asString();
            LLFloaterSettingsDebug::showControl(setting_name);
            return true;
        }

        return false;
    }
};

// Creating the object registers with the dispatcher.
FSHelpSlurlCommandHandler gFSHelpSlurlCommandHandler;

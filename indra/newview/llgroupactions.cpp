/** 
 * @file llgroupactions.cpp
 * @brief Group-related actions (join, leave, new, delete, etc)
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */


#include "llviewerprecompiledheaders.h"

#include "llgroupactions.h"

#include "message.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llgroupmgr.h"
#include "llfloaterimcontainer.h"
#include "llimview.h" // for gIMMgr
#include "llnotificationsutil.h"
#include "llstatusbar.h"	// can_afford_transaction()
#include "groupchatlistener.h"
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.3.0)
#include "llslurl.h"
#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvhandler.h"
// [/RLVa:KB]

// Firestorm includes
#include "exogroupmutelist.h"
#include "fscommon.h"
#include "fsdata.h"
#include "fsfloatercontacts.h"
#include "fsfloatergroup.h"
#include "fsfloaterim.h"
#include "llpanelgroup.h"
#include "llresmgr.h"

//
// Globals
//
static GroupChatListener sGroupChatListener;

class LLGroupHandler : public LLCommandHandler
{
public:
	// requires trusted browser to trigger
	LLGroupHandler() : LLCommandHandler("group", UNTRUSTED_THROTTLE) { }
	bool handle(const LLSD& tokens, const LLSD& query_map,
				LLMediaCtrl* web)
	{
		if (!LLUI::sSettingGroups["config"]->getBOOL("EnableGroupInfo"))
		{
			LLNotificationsUtil::add("NoGroupInfo", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
			return true;
		}

		if (tokens.size() < 1)
		{
			return false;
		}

		if (tokens[0].asString() == "create")
		{
			LLGroupActions::createGroup();
			return true;
		}

		if (tokens.size() < 2)
		{
			return false;
		}

		if (tokens[0].asString() == "list")
		{
			if (tokens[1].asString() == "show")
			{
				// <FS:Ansariel> Obey FSUseV2Friends setting where to open the group list
				//LLSD params;
				//params["people_panel_tab_name"] = "groups_panel";
				//LLFloaterSidePanelContainer::showPanel("people", "panel_people", params);
				if (gSavedSettings.getBOOL("FSUseV2Friends") && !FSCommon::isLegacySkin())
				{
					LLSD params;
					params["people_panel_tab_name"] = "groups_panel";
					LLFloaterSidePanelContainer::showPanel("people", "panel_people", params);
				}
				else
				{
					FSFloaterContacts::getInstance()->openTab("groups");
				}
				// </FS:Ansariel>
				return true;
			}
            return false;
		}

		LLUUID group_id;
		if (!group_id.set(tokens[0], FALSE))
		{
			return false;
		}

		if (tokens[1].asString() == "about")
		{
			if (group_id.isNull())
				return true;

			LLGroupActions::show(group_id);

			return true;
		}
		if (tokens[1].asString() == "inspect")
		{
			if (group_id.isNull())
				return true;
			LLGroupActions::inspect(group_id);
			return true;
		}
		return false;
	}
};
LLGroupHandler gGroupHandler;

// This object represents a pending request for specified group member information
// which is needed to check whether avatar can leave group
class LLFetchGroupMemberData : public LLGroupMgrObserver
{
public:
	LLFetchGroupMemberData(const LLUUID& group_id) : 
		mGroupId(group_id),
		mRequestProcessed(false),
		LLGroupMgrObserver(group_id) 
	{
		LL_INFOS() << "Sending new group member request for group_id: "<< group_id << LL_ENDL;
		LLGroupMgr* mgr = LLGroupMgr::getInstance();
		// register ourselves as an observer
		mgr->addObserver(this);
		// send a request
		mgr->sendGroupPropertiesRequest(group_id);
		mgr->sendCapGroupMembersRequest(group_id);
	}

	~LLFetchGroupMemberData()
	{
		if (!mRequestProcessed)
		{
			// Request is pending
			LL_WARNS() << "Destroying pending group member request for group_id: "
				<< mGroupId << LL_ENDL;
		}
		// Remove ourselves as an observer
		LLGroupMgr::getInstance()->removeObserver(this);
	}

	void changed(LLGroupChange gc)
	{
		if (gc == GC_PROPERTIES && !mRequestProcessed)
		{
			LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(mGroupId);
			if (!gdatap)
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData() was NULL" << LL_ENDL;
			} 
			else if (!gdatap->isMemberDataComplete())
			{
				LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData()->isMemberDataComplete() was FALSE" << LL_ENDL;
				processGroupData();
				mRequestProcessed = true;
			}
		}
	}

	LLUUID getGroupId() { return mGroupId; }
	virtual void processGroupData() = 0;
protected:
	LLUUID mGroupId;
private:
	bool mRequestProcessed;
};

class LLFetchLeaveGroupData: public LLFetchGroupMemberData
{
public:
	 LLFetchLeaveGroupData(const LLUUID& group_id)
		 : LLFetchGroupMemberData(group_id)
	 {}
	 void processGroupData()
	 {
		 LLGroupActions::processLeaveGroupDataResponse(mGroupId);
	 }
};

LLFetchLeaveGroupData* gFetchLeaveGroupData = NULL;

// static
void LLGroupActions::search()
{
	// <FS:Ansariel> Open groups search panel instead of invoking presumed failed websearch
	//LLFloaterReg::showInstance("search", LLSD().with("category", "groups"));
	LLFloaterReg::showInstance("search", LLSD().with("tab", "groups"));
	// </FS:Ansariel>
}

// static
void LLGroupActions::startCall(const LLUUID& group_id)
{
	// create a new group voice session
	LLGroupData gdata;

	if (!gAgent.getGroupData(group_id, gdata))
	{
		LL_WARNS() << "Error getting group data" << LL_ENDL;
		return;
	}

// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
	if (!RlvActions::canStartIM(group_id))
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return;
	}
// [/RLVa:KB]

	LLUUID session_id = gIMMgr->addSession(gdata.mName, IM_SESSION_GROUP_START, group_id, true);
	if (session_id == LLUUID::null)
	{
		LL_WARNS() << "Error adding session" << LL_ENDL;
		return;
	}

	// start the call
	gIMMgr->autoStartCallOnStartup(session_id);

	make_ui_sound("UISndStartIM");
}

// static
void LLGroupActions::join(const LLUUID& group_id)
{
	if (!gAgent.canJoinGroups())
	{
		LLNotificationsUtil::add("JoinedTooManyGroups");
		return;
	}

	// <FS:Techwolf Lupindo> fsdata support
	if (FSData::instance().isSupportGroup(group_id) && FSData::instance().isAgentFlag(gAgentID, FSData::NO_SUPPORT))
	{
		return;
	}
	// </FS:Techwolf Lupindo>

	LLGroupMgrGroupData* gdatap = 
		LLGroupMgr::getInstance()->getGroupData(group_id);

	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		args["NAME"] = gdatap->mName;
		LLSD payload;
		payload["group_id"] = group_id;

		if (can_afford_transaction(cost))
		{
			if(cost > 0)
				LLNotificationsUtil::add("JoinGroupCanAfford", args, payload, onJoinGroup);
			else
				LLNotificationsUtil::add("JoinGroupNoCost", args, payload, onJoinGroup);
				
		}
		else
		{
			LLNotificationsUtil::add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		LL_WARNS() << "LLGroupMgr::getInstance()->getGroupData(" << group_id 
			<< ") was NULL" << LL_ENDL;
	}
}

// static
bool LLGroupActions::onJoinGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (option == 1)
	{
		// user clicked cancel
		return false;
	}

	LLGroupMgr::getInstance()->
		sendGroupMemberJoin(notification["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLGroupActions::leave(const LLUUID& group_id)
{
//	if (group_id.isNull())
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
	if ( (group_id.isNull()) || ((gAgent.getGroupID() == group_id) && (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP))) )
// [/RLVa:KB]
	{
		return;
	}

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			if (gFetchLeaveGroupData != NULL)
			{
				delete gFetchLeaveGroupData;
				gFetchLeaveGroupData = NULL;
			}
			gFetchLeaveGroupData = new LLFetchLeaveGroupData(group_id);
		}
		else
		{
			processLeaveGroupDataResponse(group_id);
		}
	}
}

//static
void LLGroupActions::processLeaveGroupDataResponse(const LLUUID group_id)
{
	LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_id);
	LLUUID agent_id = gAgent.getID();
	LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.find(agent_id);
	//get the member data for the group
	if ( mit != gdatap->mMembers.end() )
	{
		LLGroupMemberData* member_data = (*mit).second;

		if ( member_data && member_data->isOwner() && gdatap->mMemberCount == 1)
		{
			LLNotificationsUtil::add("OwnerCannotLeaveGroup");
			return;
		}
	}
	LLSD args;
	args["GROUP"] = gdatap->mName;
	LLSD payload;
	payload["group_id"] = group_id;
	// <FS:Ansariel> FIRE-17676: Add special group leave notification in case of join fees
	if (gdatap->mMembershipFee > 0)
	{
		args["AMOUNT"] = LLResMgr::getInstance()->getMonetaryString(gdatap->mMembershipFee);
		LLNotificationsUtil::add("GroupLeaveConfirmMemberWithFee", args, payload, onLeaveGroup);
		return;
	}
	// </FS:Ansariel>
	LLNotificationsUtil::add("GroupLeaveConfirmMember", args, payload, onLeaveGroup);
}

// static
void LLGroupActions::activate(const LLUUID& group_id)
{
// [RLVa:KB] - Checked: 2011-03-28 (RLVa-1.4.1a) | Added: RLVa-1.3.0f
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_SETGROUP)) && (gRlvHandler.getAgentGroup() != group_id) )
	{
		return;
	}
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ActivateGroup);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	gAgent.sendReliableMessage();
}

static bool isGroupUIVisible()
{
	static LLPanel* panel = 0;
	if(!panel)
		panel = LLFloaterSidePanelContainer::getPanel("people", "panel_group_info_sidetray");
	if(!panel)
		return false;
	return panel->isInVisibleChain();
}

// static 
void LLGroupActions::inspect(const LLUUID& group_id)
{
	LLFloaterReg::showInstance("inspect_group", LLSD().with("group_id", group_id));
}

// static
void LLGroupActions::show(const LLUUID& group_id)
{
	if (group_id.isNull())
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	LLFloater* floater = NULL;
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
	{
		floater = FSFloaterGroup::openGroupFloater(group_id);
	}
	else
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
		LLFloaterSidePanelContainer* floater_people = LLFloaterReg::findTypedInstance<LLFloaterSidePanelContainer>("people");
		if (floater_people)
		{
			LLPanel* group_info_panel = floater_people->getPanel("people", "panel_group_info_sidetray");
			if (group_info_panel && group_info_panel->getVisible())
			{
				floater = floater_people;
			}
		}
	}

	if (floater && floater->isMinimized())
	{
		floater->setMinimized(FALSE);
	}
	// </FS:Ansariel>
}

// static
void LLGroupActions::show(const LLUUID& group_id, const std::string& tab_name)
{
	if (group_id.isNull())
		return;

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = tab_name;

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	LLFloater* floater = NULL;
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
	{
		floater = FSFloaterGroup::openGroupFloater(params);
	}
	else
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
		LLFloaterSidePanelContainer* floater_people = LLFloaterReg::findTypedInstance<LLFloaterSidePanelContainer>("people");
		if (floater_people)
		{
			LLPanel* group_info_panel = floater_people->getPanel("people", "panel_group_info_sidetray");
			if (group_info_panel && group_info_panel->getVisible())
			{
				floater = floater_people;
			}
		}
	}

	if (floater && floater->isMinimized())
	{
		floater->setMinimized(FALSE);
	}
	// </FS:Ansariel>
}

// <FS:Ansariel> Standalone group floaters
//void LLGroupActions::refresh_notices()
void LLGroupActions::refresh_notices(const LLUUID& group_id /*= LLUUID::null*/)
{
	// <FS:Ansariel> Standalone group floaters
	//if(!isGroupUIVisible())
	//	return;
	// </FS:Ansariel>

	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh_notices";

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
	{
		if (FSFloaterGroup::isFloaterVisible(group_id))
		{
			FSFloaterGroup::openGroupFloater(LLSD().with("group_id", group_id).with("open_tab_name", "panel_group_info_sidetray").with("action", "refresh_notices"));
		}
	}
	else
	{
		if (isGroupUIVisible())
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
		}
	}
	// </FS:Ansariel>
}

//static 
void LLGroupActions::refresh(const LLUUID& group_id)
{
	// <FS:Ansariel> Standalone group floaters
	//if(!isGroupUIVisible())
	//	return;
	// </FS:Ansariel>

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "refresh";

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
	{
		if (FSFloaterGroup::isFloaterVisible(group_id))
		{
			FSFloaterGroup::openGroupFloater(params);
		}
	}
	else
	{
		if (isGroupUIVisible())
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
		}
	}
	// </FS:Ansariel>
}

//static 
void LLGroupActions::createGroup()
{
	LLSD params;
	params["group_id"] = LLUUID::null;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "create";

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater"))
	{
		FSFloaterGroup::openGroupFloater(params);
	}
	else
	{
		LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	}
	// </FS:Ansariel>
}
//static
void LLGroupActions::closeGroup(const LLUUID& group_id)
{
	// <FS:Ansariel> Standalone group floaters
	//if(!isGroupUIVisible())
	//	return;
	// </FS:Ansariel>

	LLSD params;
	params["group_id"] = group_id;
	params["open_tab_name"] = "panel_group_info_sidetray";
	params["action"] = "close";

	// <FS:Ansariel> Standalone group floaters
	//LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
	if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
	{
		FSFloaterGroup::closeGroupFloater(group_id);
	}
	else
	{
		if (isGroupUIVisible())
		{
			LLFloaterSidePanelContainer::showPanel("people", "panel_group_info_sidetray", params);
		}
	}
	// </FS:Ansariel>
}

// static
LLUUID LLGroupActions::startIM(const LLUUID& group_id)
{
	if (group_id.isNull()) return LLUUID::null;

// [RLVa:KB] - Checked: 2013-05-09 (RLVa-1.4.9)
	if (!RlvActions::canStartIM(group_id))
	{
		make_ui_sound("UISndInvalidOp");
		RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_STARTIM, LLSD().with("RECIPIENT", LLSLURL("group", group_id, "about").getSLURLString()));
		return LLUUID::null;
	}
// [/RLVa:KB]

	LLGroupData group_data;
	if (gAgent.getGroupData(group_id, group_data))
	{
		// <exodus>
		// Unmute the group if the user tries to start a session with it.
		exoGroupMuteList::instance().remove(group_id);
		// </exodus>
		LLUUID session_id = gIMMgr->addSession(
			group_data.mName,
			IM_SESSION_GROUP_START,
			group_id);
		if (session_id != LLUUID::null)
		{
			// <FS:Ansariel> [FS communication UI]
			//LLFloaterIMContainer::getInstance()->showConversation(session_id);
			FSFloaterIM::show(session_id);
			// </FS:Ansariel> [FS communication UI]
		}
		make_ui_sound("UISndStartIM");
		return session_id;
	}
	else
	{
		// this should never happen, as starting a group IM session
		// relies on you belonging to the group and hence having the group data
		make_ui_sound("UISndInvalidOp");
		return LLUUID::null;
	}
}

// [SL:KB] - Patch: Chat-GroupSnooze | Checked: 2012-06-17 (Catznip-3.3)
static void snooze_group_im(const LLUUID& group_id, S32 duration = -1)
{
	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id.notNull())
	{
		LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(session_id);

		if (!session)
		{
			LL_WARNS() << "Empty session." << LL_ENDL;
			return;
		}

		session->mSnoozeTime = duration;
		session->mCloseAction = LLIMModel::LLIMSession::CLOSE_SNOOZE;

		gIMMgr->leaveSession(session_id);
	}
}

static void snooze_group_im_duration_callback(const LLSD& notification, const LLSD& response, const LLUUID& group_id)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		std::istringstream duration_str(response["duration"].asString());
		S32 duration(-1);
		if (duration_str >> duration && duration >= 0)
		{
			snooze_group_im(group_id, duration);
		}
		else
		{
			LLNotificationsUtil::add("SnoozeDurationInvalidInput");
		}
	}
}

static void confirm_group_im_snooze(const LLUUID& group_id)
{
	if (group_id.isNull())
	{
		return;
	}

	if (gSavedSettings.getBOOL("FSEnablePerGroupSnoozeDuration"))
	{
		LLSD args;
		args["DURATION"] = gSavedSettings.getS32("GroupSnoozeTime");

		LLNotificationsUtil::add("SnoozeDuration", args, LLSD(), boost::bind(&snooze_group_im_duration_callback, _1, _2, group_id));
		return;
	}

	snooze_group_im(group_id);
}

static void close_group_im(const LLUUID& group_id, LLIMModel::LLIMSession::SCloseAction close_action)
{
	if (group_id.isNull())
	{
		return;
	}
	
	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
	if (session_id.notNull())
	{
		LLIMModel::LLIMSession* pIMSession = LLIMModel::getInstance()->findIMSession(session_id);
		if (pIMSession)
		{
			pIMSession->mCloseAction = close_action;
		}
		gIMMgr->leaveSession(session_id);
	}
}

void LLGroupActions::leaveIM(const LLUUID& group_id)
{
	close_group_im(group_id, LLIMModel::LLIMSession::CLOSE_LEAVE);
}

void LLGroupActions::snoozeIM(const LLUUID& group_id)
{
	confirm_group_im_snooze(group_id);
}

void LLGroupActions::endIM(const LLUUID& group_id)
{
	close_group_im(group_id, LLIMModel::LLIMSession::CLOSE_DEFAULT);
}

// [/SL:KB]
//// static
//void LLGroupActions::endIM(const LLUUID& group_id)
//{
//	if (group_id.isNull())
//		return;
//	
//	LLUUID session_id = gIMMgr->computeSessionID(IM_SESSION_GROUP_START, group_id);
//	if (session_id != LLUUID::null)
//	{
//		gIMMgr->leaveSession(session_id);
//	}
//}

// static
bool LLGroupActions::isInGroup(const LLUUID& group_id)
{
	// *TODO: Move all the LLAgent group stuff into another class, such as
	// this one.
	return gAgent.isInGroup(group_id);
}

// static
bool LLGroupActions::isAvatarMemberOfGroup(const LLUUID& group_id, const LLUUID& avatar_id)
{
	if(group_id.isNull() || avatar_id.isNull())
	{
		return false;
	}

	LLGroupMgrGroupData* group_data = LLGroupMgr::getInstance()->getGroupData(group_id);
	if(!group_data)
	{
		return false;
	}

	if(group_data->mMembers.end() == group_data->mMembers.find(avatar_id))
	{
		return false;
	}

	return true;
}

// [SL:KB] - Patch: Chat-GroupSessionEject | Checked: 2012-02-04 (Catznip-3.2.1) | Added: Catznip-3.2.1
bool LLGroupActions::canEjectFromGroup(const LLUUID& idGroup, const LLUUID& idAgent)
{
	if (gAgent.isGodlike())
	{
		return true;
	}

	if (gAgent.hasPowerInGroup(idGroup, GP_MEMBER_EJECT))
	{
		const LLGroupMgrGroupData* pGroupData = LLGroupMgr::getInstance()->getGroupData(idGroup);
		if ( (!pGroupData) || (!pGroupData->isMemberDataComplete()) )
		{
			// There is no (or not enough) information on this group but the user does have the group eject power
			return true;
		}

		LLGroupMgrGroupData::member_list_t::const_iterator itMember = pGroupData->mMembers.find(idAgent);
		if (pGroupData->mMembers.end() != itMember)
		{
			const LLGroupMemberData* pMemberData = (*itMember).second;
			if ( (pGroupData->isRoleDataComplete()) && (pGroupData->isRoleMemberDataComplete()) )
			{
				for (LLGroupMemberData::role_list_t::const_iterator itRole = pMemberData->roleBegin(); 
						itRole != pMemberData->roleEnd(); ++itRole)
				{
					if ((*itRole).first.notNull())
					{
						// Someone who belongs to any roles other than "Everyone" can't be ejected
						return false;
					}
				}
			}
			// Owners can never be ejected
			return (itMember->second) && (!itMember->second->isOwner());
		}
	}
	return false;
}

void LLGroupActions::ejectFromGroup(const LLUUID& idGroup, const LLUUID& idAgent)
{
	LLSD args;
	LLSD payload;
	payload["avatar_id"] = idAgent;
	payload["group_id"] = idGroup;
	std::string fullname = LLSLURL("agent", idAgent, "inspect").getSLURLString();
	args["AVATAR_NAME"] = fullname;
	LLNotificationsUtil::add("EjectGroupMemberWarning",
							 args,
							 payload,
							 callbackEject);
}

bool LLGroupActions::callbackEject(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (2 == option) // Cancel button
	{
		return false;
	}
	if (0 == option) // Eject button
	{
		LLUUID idAgent = notification["payload"]["avatar_id"].asUUID();
		LLUUID idGroup = notification["payload"]["group_id"].asUUID();
		
		if (!canEjectFromGroup(idGroup, idAgent))
			return false;
		
		uuid_vec_t idAgents;
		idAgents.push_back(idAgent);
		LLGroupMgr::instance().sendGroupMemberEjects(idGroup, idAgents);
	}
	return false;
}
// [/SL:KB]

//-- Private methods ----------------------------------------------------------

// static
bool LLGroupActions::onLeaveGroup(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID group_id = notification["payload"]["group_id"].asUUID();
	if(option == 0)
	{
		// <FS:Ansariel> Standalone group floaters
		if (gSavedSettings.getBOOL("FSUseStandaloneGroupFloater")) 
		{
			FSFloaterGroup::closeGroupFloater(group_id);
		}
		// </FS:Ansariel>
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_LeaveGroupRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_GroupData);
		msg->addUUIDFast(_PREHASH_GroupID, group_id);
		gAgent.sendReliableMessage();
	}
	return false;
}

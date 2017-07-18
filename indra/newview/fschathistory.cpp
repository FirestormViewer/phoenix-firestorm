/** 
 * @file fschathistory.cpp
 * @brief LLTextEditor base class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

// Original file: LLChatHistory.cpp

#include "llviewerprecompiledheaders.h"

#include "fschathistory.h"

#include <boost/signals2.hpp>

#include "llavatarnamecache.h"
#include "llinstantmessage.h"

#include "llimview.h"
#include "llcommandhandler.h"
#include "llpanel.h"
#include "lluictrlfactory.h"
#include "llscrollcontainer.h"
#include "llagent.h"
#include "llagentdata.h"
#include "llavataractions.h"
#include "llavatariconctrl.h"
#include "llcallingcard.h" //for LLAvatarTracker
#include "llgroupactions.h"
#include "llgroupmgr.h"
#include "llspeakers.h" //for LLIMSpeakerMgr
#include "lltrans.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llstylemap.h"
#include "llslurl.h"
#include "lllayoutstack.h"
#include "llnotificationsutil.h"
#include "lltoastnotifypanel.h"
#include "lltooltip.h"
#include "llviewerregion.h"
#include "llviewertexteditor.h"
#include "llworld.h"
#include "lluiconstants.h"
#include "llstring.h"
#include "llurlaction.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h"

#include "fscommon.h"
#include "llchatentry.h"
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llpanelblockedlist.h"
#include "rlvcommon.h"
#include "rlvhandler.h"

static LLDefaultChildRegistry::Register<FSChatHistory> r("fs_chat_history");

const static std::string NEW_LINE(rawstr_to_utf8("\n"));

const static std::string SLURL_APP_AGENT = "secondlife:///app/agent/";
const static std::string SLURL_ABOUT = "/about";

// support for secondlife:///app/objectim/{UUID}/ SLapps
class LLObjectIMHandler : public LLCommandHandler
{
public:
	// requests will be throttled from a non-trusted browser
	LLObjectIMHandler() : LLCommandHandler("objectim", UNTRUSTED_THROTTLE) {}

	bool handle(const LLSD& params, const LLSD& query_map, LLMediaCtrl* web)
	{
		if (params.size() < 1)
		{
			return false;
		}

		LLUUID object_id;
		if (!object_id.set(params[0], FALSE))
		{
			return false;
		}

		LLSD payload;
		payload["object_id"] = object_id;
		payload["owner_id"] = query_map["owner"];
// [RLVa:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
		if (query_map.has("rlv_shownames"))
			payload["rlv_shownames"] = query_map["rlv_shownames"];
// [/RLVa:KB]
		payload["name"] = query_map["name"];
		payload["slurl"] = LLWeb::escapeURL(query_map["slurl"]);
		payload["group_owned"] = query_map["groupowned"];
		LLFloaterReg::showInstance("inspect_remote_object", payload);
		return true;
	}
};
LLObjectIMHandler gObjectIMHandler;

class FSChatHistoryHeader: public LLPanel
{
public:
	FSChatHistoryHeader()
	:	LLPanel(),
		mInfoCtrl(NULL),
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		mShowContextMenu(true), 
		mShowInfoCtrl(true),
// [/RLVa:KB]
		mPopupMenuHandleAvatar(),
		mPopupMenuHandleObject(),
		mAvatarID(),
		mSourceType(CHAT_SOURCE_UNKNOWN),
		mType(CHAT_TYPE_NORMAL), // FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		mFrom(),
		mSessionID(),
		mMinUserNameWidth(0),
		mUserNameFont(NULL),
		mUserNameTextBox(NULL),
		mTimeBoxTextBox(NULL),
		mAvatarNameCacheConnection()
	{}

	static FSChatHistoryHeader* createInstance(const std::string& file_name)
	{
		FSChatHistoryHeader* pInstance = new FSChatHistoryHeader;
		pInstance->buildFromFile(file_name);	
		return pInstance;
	}

	~FSChatHistoryHeader()
	{
		if (mAvatarNameCacheConnection.connected())
		{
			mAvatarNameCacheConnection.disconnect();
		}
	}

	BOOL handleMouseUp(S32 x, S32 y, MASK mask)
	{
		return LLPanel::handleMouseUp(x,y,mask);
	}

	void onObjectIconContextMenuItemClicked(const LLSD& userdata)
	{
		std::string level = userdata.asString();

		if (level == "profile")
		{
			LLFloaterReg::showInstance("inspect_remote_object", mObjectData);
		}
		else if (level == "block")
		{
			LLMuteList::getInstance()->add(LLMute(getAvatarId(), mFrom, LLMute::OBJECT));
			LLPanelBlockedList::showPanelAndSelect(getAvatarId());
		}
		else if (level == "unblock")
		{
			LLMuteList::getInstance()->remove(LLMute(getAvatarId(), mFrom, LLMute::OBJECT));
		}
		else if (level == "map")
		{
			std::string url = "secondlife://" + mObjectData["slurl"].asString();
			LLUrlAction::showLocationOnMap(url);
		}
		else if (level == "teleport")
		{
			std::string url = "secondlife://" + mObjectData["slurl"].asString();
			LLUrlAction::teleportToLocation(url);
		}
	}

	bool onObjectIconContextMenuItemVisible(const LLSD& userdata)
	{
		std::string level = userdata.asString();
		if (level == "is_blocked")
		{
			return LLMuteList::getInstance()->isMuted(getAvatarId(), mFrom, LLMute::flagTextChat);
		}
		else if (level == "not_blocked")
		{
			return !LLMuteList::getInstance()->isMuted(getAvatarId(), mFrom, LLMute::flagTextChat);
		}
		return false;
	}

	void banGroupMember(const LLUUID& participant_uuid)
	{
		LLUUID group_uuid = mSessionID;
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_uuid);
		if (!gdatap)
		{
			// Not a group
			return;
		}

		gdatap->banMemberById(participant_uuid);
	}

	bool canBanInGroup()
	{
		LLUUID group_uuid = mSessionID;
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_uuid);
		if (!gdatap)
		{
			// Not a group
			return false;
		}

		if (gAgent.hasPowerInGroup(group_uuid, GP_ROLE_REMOVE_MEMBER)
			&& gAgent.hasPowerInGroup(group_uuid, GP_GROUP_BAN_ACCESS))
		{
			return true;
		}

		return false;
	}

	bool canBanGroupMember(const LLUUID& participant_uuid)
	{
		LLUUID group_uuid = mSessionID;
		LLGroupMgrGroupData* gdatap = LLGroupMgr::getInstance()->getGroupData(group_uuid);
		if (!gdatap)
		{
			// Not a group
			return false;
		}

		if (gdatap->mPendingBanRequest)
		{
			return false;
		}

		if (gAgentID == getAvatarId())
		{
			//Don't ban self
			return false;
		}

		if (gdatap->isRoleMemberDataComplete())
		{
			if (gdatap->mMembers.size())
			{
				LLGroupMgrGroupData::member_list_t::iterator mi = gdatap->mMembers.find(participant_uuid);
				if (mi != gdatap->mMembers.end())
				{
					LLGroupMemberData* member_data = (*mi).second;
					// Is the member an owner?
					if (member_data && member_data->isInRole(gdatap->mOwnerRole))
					{
						return false;
					}

					if (gAgent.hasPowerInGroup(group_uuid, GP_ROLE_REMOVE_MEMBER)
						&& gAgent.hasPowerInGroup(group_uuid, GP_GROUP_BAN_ACCESS))
					{
						return true;
					}
				}
			}
		}

		LLSpeakerMgr * speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
		if (speaker_mgr)
		{
			LLSpeaker * speakerp = speaker_mgr->findSpeaker(participant_uuid).get();

			if (speakerp
				&& gAgent.hasPowerInGroup(group_uuid, GP_ROLE_REMOVE_MEMBER)
				&& gAgent.hasPowerInGroup(group_uuid, GP_GROUP_BAN_ACCESS))
			{
				return true;
			}
		}

		return false;
	}

	bool isGroupModerator()
	{
		LLSpeakerMgr * speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
		if (!speaker_mgr)
		{
			LL_WARNS() << "Speaker manager is missing" << LL_ENDL;
			return false;
		}

		// Is session a group call/chat?
		if(gAgent.isInGroup(mSessionID))
		{
			LLSpeaker * speakerp = speaker_mgr->findSpeaker(gAgentID).get();

			// Is agent a moderator?
			return speakerp && speakerp->mIsModerator;
		}

		return false;
	}

	bool canModerate(const std::string& userdata)
	{
		// only group moderators can perform actions related to this "enable callback"
		if (!isGroupModerator() || gAgentID == getAvatarId())
		{
			return false;
		}

		LLSpeakerMgr * speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
		if (!speaker_mgr)
		{
			return false;
		}

		LLSpeaker * speakerp = speaker_mgr->findSpeaker(getAvatarId()).get();
		if (!speakerp)
		{
			return false;
		}

		bool voice_channel = speakerp->isInVoiceChannel();

		if ("can_moderate_voice" == userdata)
		{
			return voice_channel;
		}
		else if ("can_mute" == userdata)
		{
			return voice_channel && (speakerp->mStatus != LLSpeaker::STATUS_MUTED);
		}
		else if ("can_unmute" == userdata)
		{
			return speakerp->mStatus == LLSpeaker::STATUS_MUTED;
		}
		else if ("can_allow_text_chat" == userdata)
		{
			return true;
		}

		return false;
	}

	void onAvatarIconContextMenuItemClicked(const LLSD& userdata)
	{
		std::string level = userdata.asString();

		if (level == "profile")
		{
			LLAvatarActions::showProfile(getAvatarId());
		}
		else if (level == "im")
		{
			LLAvatarActions::startIM(getAvatarId());
		}
		else if (level == "teleport_to")
		{
			LLAvatarActions::teleportTo(getAvatarId());
		}
		else if (level == "teleport")
		{
			LLAvatarActions::offerTeleport(getAvatarId());
		}
		else if (level == "request_teleport")
		{
			LLAvatarActions::teleportRequest(getAvatarId());
		}
		else if (level == "voice_call")
		{
			LLAvatarActions::startCall(getAvatarId());
		}
		else if (level == "chat_history")
		{
			LLAvatarActions::viewChatHistory(getAvatarId());
		}
		else if (level == "add")
		{
			LLAvatarActions::requestFriendshipDialog(getAvatarId(), mFrom);
		}
		else if (level == "add_set")
		{
			LLAvatarActions::addToContactSet(getAvatarId());
		}
		else if (level == "remove")
		{
			LLAvatarActions::removeFriendDialog(getAvatarId());
		}
		else if (level == "invite_to_group")
		{
			LLAvatarActions::inviteToGroup(getAvatarId());
		}
		else if (level == "zoom_in")
		{
			handle_zoom_to_object(getAvatarId());
		}
		else if (level == "map")
		{
			LLAvatarActions::showOnMap(getAvatarId());
		}
		else if (level == "track")
		{
			LLAvatarActions::track(getAvatarId());
		}
		else if (level == "share")
		{
			LLAvatarActions::share(getAvatarId());
		}
		else if (level == "pay")
		{
			LLAvatarActions::pay(getAvatarId());
		}
		else if (level == "copy_name")
		{
			LLUrlAction::copyLabelToClipboard(LLSLURL("agent", getAvatarId(), "inspect").getSLURLString());
		}
		else if (level == "copy_url")
		{
			LLUrlAction::copyURLToClipboard(LLSLURL("agent", getAvatarId(), "about").getSLURLString());
		}
		else if(level == "block_unblock")
		{
			LLAvatarActions::toggleMute(getAvatarId(), LLMute::flagVoiceChat);
		}
		else if(level == "mute_unmute")
		{
			LLAvatarActions::toggleMute(getAvatarId(), LLMute::flagTextChat);
		}
		else if(level == "toggle_allow_text_chat")
		{
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			speaker_mgr->toggleAllowTextChat(getAvatarId());
		}
		else if(level == "group_mute")
		{
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if (speaker_mgr)
			{
				speaker_mgr->moderateVoiceParticipant(getAvatarId(), false);
			}
		}
		else if(level == "group_unmute")
		{
			LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if (speaker_mgr)
			{
				speaker_mgr->moderateVoiceParticipant(getAvatarId(), true);
			}
		}
		else if(level == "ban_member")
		{
			banGroupMember(getAvatarId());
		}
	}

	bool onAvatarIconContextMenuItemChecked(const LLSD& userdata)
	{
		std::string level = userdata.asString();

		if (level == "is_blocked")
		{
			return LLMuteList::getInstance()->isMuted(getAvatarId(), LLMute::flagVoiceChat);
		}
		if (level == "is_muted")
		{
			return LLMuteList::getInstance()->isMuted(getAvatarId(), LLMute::flagTextChat);
		}
		else if (level == "is_allowed_text_chat")
		{
			if (gAgent.isInGroup(mSessionID))
			{
				LLIMSpeakerMgr* speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
				const LLSpeaker * speakerp = speaker_mgr->findSpeaker(getAvatarId());

				if (NULL != speakerp)
				{
					return !speakerp->mModeratorMutedText;
				}
			}
			return false;
		}
		return false;
	}

	bool onAvatarIconContextMenuItemEnabled(const LLSD& userdata)
	{
		std::string level = userdata.asString();

		if (level == "can_allow_text_chat" || level == "can_mute" || level == "can_unmute")
		{
			return canModerate(userdata);
		}
		else if (level == "can_ban_member")
		{
			return canBanGroupMember(getAvatarId());
		}
		return false;
	}

	bool onAvatarIconContextMenuItemVisible(const LLSD& userdata)
	{
		std::string level = userdata.asString();

		if (level == "show_mute")
		{
			LLSpeakerMgr * speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if (speaker_mgr)
			{
				LLSpeaker * speakerp = speaker_mgr->findSpeaker(getAvatarId()).get();
				if (speakerp)
				{
					return speakerp->isInVoiceChannel() && speakerp->mStatus != LLSpeaker::STATUS_MUTED;
				}
			}
			return false;
		}
		else if (level == "show_unmute")
		{
			LLSpeakerMgr * speaker_mgr = LLIMModel::getInstance()->getSpeakerManager(mSessionID);
			if (speaker_mgr)
			{
				LLSpeaker * speakerp = speaker_mgr->findSpeaker(getAvatarId()).get();
				if (speakerp)
				{
					return speakerp->mStatus == LLSpeaker::STATUS_MUTED;
				}
			}
			return false;
		}
		return false;
	}

	BOOL postBuild()
	{
		LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
		LLUICtrl::EnableCallbackRegistry::ScopedRegistrar registrar_enable;

		registrar.add("AvatarIcon.Action", boost::bind(&FSChatHistoryHeader::onAvatarIconContextMenuItemClicked, this, _2));
		registrar_enable.add("AvatarIcon.Check", boost::bind(&FSChatHistoryHeader::onAvatarIconContextMenuItemChecked, this, _2));
		registrar_enable.add("AvatarIcon.Enable", boost::bind(&FSChatHistoryHeader::onAvatarIconContextMenuItemEnabled, this, _2));
		registrar_enable.add("AvatarIcon.Visible", boost::bind(&FSChatHistoryHeader::onAvatarIconContextMenuItemVisible, this, _2));
		registrar.add("ObjectIcon.Action", boost::bind(&FSChatHistoryHeader::onObjectIconContextMenuItemClicked, this, _2));
		registrar_enable.add("ObjectIcon.Visible", boost::bind(&FSChatHistoryHeader::onObjectIconContextMenuItemVisible, this, _2));

		LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_avatar_icon.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
		mPopupMenuHandleAvatar = menu->getHandle();

		menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_object_icon.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
		mPopupMenuHandleObject = menu->getHandle();

		setDoubleClickCallback(boost::bind(&FSChatHistoryHeader::showInspector, this));

		setMouseEnterCallback(boost::bind(&FSChatHistoryHeader::showInfoCtrl, this));
		setMouseLeaveCallback(boost::bind(&FSChatHistoryHeader::hideInfoCtrl, this));

		mUserNameTextBox = getChild<LLTextBox>("user_name");
		mTimeBoxTextBox = getChild<LLTextBox>("time_box");

		mInfoCtrl = LLUICtrlFactory::getInstance()->createFromFile<LLUICtrl>("inspector_info_ctrl.xml", this, LLPanel::child_registry_t::instance());
		llassert(mInfoCtrl != NULL);
		mInfoCtrl->setCommitCallback(boost::bind(&FSChatHistoryHeader::onClickInfoCtrl, mInfoCtrl));
		mInfoCtrl->setVisible(FALSE);

		return LLPanel::postBuild();
	}

	bool pointInChild(const std::string& name,S32 x,S32 y)
	{
		LLUICtrl* child = findChild<LLUICtrl>(name);
		if(!child)
			return false;
		
		LLView* parent = child->getParent();
		if(parent!=this)
		{
			x-=parent->getRect().mLeft;
			y-=parent->getRect().mBottom;
		}

		S32 local_x = x - child->getRect().mLeft ;
		S32 local_y = y - child->getRect().mBottom ;
		return 	child->pointInView(local_x, local_y);
	}

	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask)
	{
		if(pointInChild("avatar_icon",x,y) || pointInChild("user_name",x,y))
		{
			showContextMenu(x,y);
			return TRUE;
		}

		return LLPanel::handleRightMouseDown(x,y,mask);
	}

	void showInspector()
	{
//		if (mAvatarID.isNull() && CHAT_SOURCE_SYSTEM != mSourceType) return;
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		// Don't double-click show the inspector if we're not showing the info control
		if ( (!mShowInfoCtrl) || (mAvatarID.isNull() && CHAT_SOURCE_SYSTEM != mSourceType) ) return;
// [/RLVa:KB]
		
		if (mSourceType == CHAT_SOURCE_OBJECT)
		{
			LLFloaterReg::showInstance("inspect_remote_object", mObjectData);
		}
		else if (mSourceType == CHAT_SOURCE_AGENT || (mSourceType == CHAT_SOURCE_SYSTEM && mType == CHAT_TYPE_RADAR)) // FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		{
			LLUrlAction::executeSLURL(LLSLURL("agent", mAvatarID, "inspect").getSLURLString());
		}
		//if chat source is system, you may add "else" here to define behaviour.
	}

	static void onClickInfoCtrl(LLUICtrl* info_ctrl)
	{
		if (!info_ctrl) return;

		FSChatHistoryHeader* header = dynamic_cast<FSChatHistoryHeader*>(info_ctrl->getParent());	
		if (!header) return;

		header->showInspector();
	}

	const LLUUID& getAvatarId () const { return mAvatarID;}

	void setup(const LLChat& chat, const LLStyle::Params& style_params, const LLSD& args)
	{
		mAvatarID = chat.mFromID;
		mSessionID = chat.mSessionID;
		mSourceType = chat.mSourceType;
		mType = chat.mChatType; // FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		mNameStyleParams = style_params;

		//*TODO overly defensive thing, source type should be maintained out there
		if((chat.mFromID.isNull() && chat.mFromName.empty()) || (chat.mFromName == SYSTEM_FROM && chat.mFromID.isNull()))
		{
			mSourceType = CHAT_SOURCE_SYSTEM;
		}

		// Use the original font defined in panel_chat_header.xml
		mNameStyleParams.font.name = "SansSerifSmall";

		// To be able to use the group chat moderator options, we use the
		// original font style "BOLD" for everything except group chats.
		// Group chats have the option to show moderators in bold, so
		// we display both display and username in "NORMAL" for now.
		static LLCachedControl<bool> fsHighlightGroupMods(gSavedSettings, "FSHighlightGroupMods");
		LLIMModel::LLIMSession* session = LLIMModel::instance().findIMSession(mSessionID);
		if (!fsHighlightGroupMods || !session || !session->isGroupSessionType())
		{
			mNameStyleParams.font.style = "BOLD";
		}

		mUserNameFont = mNameStyleParams.font();
		mUserNameTextBox->setReadOnlyColor(mNameStyleParams.readonly_color());
		mUserNameTextBox->setColor(mNameStyleParams.color());
		mUserNameTextBox->setFont(mUserNameFont);

		// Make sure we use the correct font style for everything after the display name
		mNameStyleParams.font.style = style_params.font.style;

		if (chat.mFromName.empty()
			//|| mSourceType == CHAT_SOURCE_SYSTEM
			// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
			|| (mSourceType == CHAT_SOURCE_SYSTEM && mType != CHAT_TYPE_RADAR)
			|| mAvatarID.isNull())
		{
			if (mSourceType == CHAT_SOURCE_UNKNOWN)
			{
				// Avatar names may come up as CHAT_SOURCE_UNKNOWN - don't append the grid name in that case
				mFrom = chat.mFromName;
			}
			else
			{
				mFrom = LLTrans::getString("SECOND_LIFE"); // Will automatically be substituted!
				if (!chat.mFromName.empty() && (mFrom != chat.mFromName))
				{
					mFrom += " (" + chat.mFromName + ")";
				}
			}
			mUserNameTextBox->setValue(mFrom);
			updateMinUserNameWidth();
		}
		else if ((mSourceType == CHAT_SOURCE_AGENT || (mSourceType == CHAT_SOURCE_SYSTEM && mType == CHAT_TYPE_RADAR))
				 && !mAvatarID.isNull()
				 && chat.mChatStyle != CHAT_STYLE_HISTORY)
		{
			// ...from a normal user, lookup the name and fill in later.
			// *NOTE: Do not do this for chat history logs, otherwise the viewer
			// will flood the People API with lookup requests on startup

			// Start with blank so sample data from XUI XML doesn't
			// flash on the screen
//			user_name->setValue( LLSD() );
//			fetchAvatarName();
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
			if (!chat.mRlvNamesFiltered)
			{
				mUserNameTextBox->setValue( LLSD() );
				fetchAvatarName();
			}
			else
			{
				// If the agent's chat was subject to @shownames=n we should display their anonimized name
				mFrom = chat.mFromName;
				mUserNameTextBox->setValue(mFrom);
				mUserNameTextBox->setToolTip(mFrom);
				setToolTip(mFrom);
				updateMinUserNameWidth();
			}
// [/RLVa:KB]
		}
		else if (chat.mChatStyle == CHAT_STYLE_HISTORY ||
				 (mSourceType == CHAT_SOURCE_AGENT || (mSourceType == CHAT_SOURCE_SYSTEM && mType == CHAT_TYPE_RADAR)))
		{
			//if it's an avatar name with a username add formatting
			S32 username_start = chat.mFromName.rfind(" (");
			S32 username_end = chat.mFromName.rfind(')');
			
			if (username_start != std::string::npos &&
				username_end == (chat.mFromName.length() - 1))
			{
				mFrom = chat.mFromName.substr(0, username_start);
				mUserNameTextBox->setValue(mFrom);
			}
			else
			{
				// If the agent's chat was subject to @shownames=n we should display their anonimized name
				mFrom = chat.mFromName;
				mUserNameTextBox->setValue(mFrom);
				updateMinUserNameWidth();
			}
		}
		else
		{
			// ...from an object, just use name as given
			mFrom = chat.mFromName;
			mUserNameTextBox->setValue(mFrom);
			updateMinUserNameWidth();
		}


		setTimeField(chat);

		// Set up the icon.
		LLAvatarIconCtrl* icon = getChild<LLAvatarIconCtrl>("avatar_icon");
		
		// Hacky preference to hide avatar icons for people who don't like them by overdrawing them. Will change to disable soon. -AO
		if (!gSavedSettings.getBOOL("ShowChatMiniIcons"))
		{
			icon->setColor(LLUIColorTable::instance().getColor("Transparent"));
		}

		if(mSourceType != CHAT_SOURCE_AGENT ||	mAvatarID.isNull())
			icon->setDrawTooltip(false);

// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		// Don't show the context menu, info control or avatar icon tooltip if this chat was subject to @shownames=n
		if ( (chat.mRlvNamesFiltered) && ((mSourceType == CHAT_SOURCE_AGENT || (mSourceType == CHAT_SOURCE_SYSTEM && mType == CHAT_TYPE_RADAR)) || (CHAT_SOURCE_OBJECT == mSourceType))  )
		{
			mShowInfoCtrl = mShowContextMenu = false;
			icon->setDrawTooltip(false);
		}
// [/RLVa:KB]

		switch (mSourceType)
		{
			case CHAT_SOURCE_AGENT:
				if (!chat.mRlvNamesFiltered)
				{
					icon->setValue(chat.mFromID);
				}
				else
				{
					icon->setValue(LLSD("Unknown_Icon"));
				}
				break;
			case CHAT_SOURCE_OBJECT:
				icon->setValue(LLSD("OBJECT_Icon"));
				break;
			case CHAT_SOURCE_SYSTEM:
				//icon->setValue(LLSD("SL_Logo"));
				// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
				if(chat.mChatType == CHAT_TYPE_RADAR)
				{
					if (!chat.mRlvNamesFiltered)
					{
						icon->setValue(chat.mFromID);
					}
					else
					{
						icon->setValue(LLSD("Unknown_Icon"));
					}
				}
				else
				{
					icon->setValue(LLSD("SL_Logo"));
				}
				// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
				break;
			case CHAT_SOURCE_UNKNOWN: 
				icon->setValue(LLSD("Unknown_Icon"));
		}

		// In case the message came from an object, save the object info
		// to be able properly show its profile.
		if ( chat.mSourceType == CHAT_SOURCE_OBJECT)
		{
			std::string slurl = args["slurl"].asString();
			if (slurl.empty())
			{
				LLViewerRegion *region = LLWorld::getInstance()->getRegionFromPosAgent(chat.mPosAgent);
				if(region)
				{
					LLSLURL region_slurl(region->getName(), chat.mPosAgent);
					slurl = region_slurl.getLocationString();
				}
			}

			LLSD payload;
			payload["object_id"]	= chat.mFromID;
			payload["name"]			= chat.mFromName;
			payload["owner_id"]		= chat.mOwnerID;
			payload["slurl"]		= LLWeb::escapeURL(slurl);

			mObjectData = payload;
		}
	}

	/*virtual*/ void draw()
	{
		LLRect user_name_rect = mUserNameTextBox->getRect();
		S32 user_name_width = user_name_rect.getWidth();
		S32 time_box_width = mTimeBoxTextBox->getRect().getWidth();

		if (!mTimeBoxTextBox->getVisible() && user_name_width > mMinUserNameWidth)
		{
			user_name_rect.mRight -= time_box_width;
			mUserNameTextBox->reshape(user_name_rect.getWidth(), user_name_rect.getHeight());
			mUserNameTextBox->setRect(user_name_rect);

			mTimeBoxTextBox->setVisible(TRUE);
		}

		LLPanel::draw();
	}

	void updateMinUserNameWidth()
	{
		if (mUserNameFont)
		{
			const LLWString& text = mUserNameTextBox->getWText();
			mMinUserNameWidth = mUserNameFont->getWidth(text.c_str()) + PADDING;
		}
	}

protected:
	static const S32 PADDING = 20;

	void showContextMenu(S32 x,S32 y)
	{
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		if (!mShowContextMenu)
			return;
// [/RLVa:KB]
		if(mSourceType == CHAT_SOURCE_SYSTEM)
			//showSystemContextMenu(x,y);
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		{
			// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
			if(mType == CHAT_TYPE_RADAR)
			{
				showAvatarContextMenu(x,y);
			}
			else
			{
				showSystemContextMenu(x,y);
			}
		}
		// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
		if(mAvatarID.notNull() && mSourceType == CHAT_SOURCE_AGENT)
			showAvatarContextMenu(x,y);
		if(mAvatarID.notNull() && mSourceType == CHAT_SOURCE_OBJECT)
			showObjectContextMenu(x,y);
	}

	void showSystemContextMenu(S32 x,S32 y)
	{
	}
	
	void showObjectContextMenu(S32 x,S32 y)
	{
		LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandleObject.get();
		if(menu)
			LLMenuGL::showPopup(this, menu, x, y);
	}
	
	void showAvatarContextMenu(S32 x,S32 y)
	{
		LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandleAvatar.get();

		if(menu)
		{
			bool is_friend = LLAvatarActions::isFriend(mAvatarID);
			bool is_group_session = gAgent.isInGroup(mSessionID);
			
			menu->setItemEnabled("Add Friend", !is_friend);
			menu->setItemEnabled("Remove Friend", is_friend);
			menu->setItemVisible("Moderator Options Separator", is_group_session && isGroupModerator());
			menu->setItemVisible("Moderator Options", is_group_session && isGroupModerator());
			menu->setItemVisible("Group Ban Separator", is_group_session && canBanInGroup());
			menu->setItemVisible("BanMember", is_group_session && canBanInGroup());

			if(gAgentID == mAvatarID)
			{
				menu->setItemEnabled("Add Friend", false);
				menu->setItemEnabled("Send IM", false);
				menu->setItemEnabled("Remove Friend", false);
				menu->setItemEnabled("Offer Teleport",false);
				menu->setItemEnabled("Request Teleport",false);
				menu->setItemEnabled("Teleport to",false);
				menu->setItemEnabled("Voice Call", false);
				menu->setItemEnabled("Chat History", false);
				menu->setItemEnabled("Add Contact Set", false);
				menu->setItemEnabled("Invite Group", false);
				menu->setItemEnabled("Zoom In", false);
				menu->setItemEnabled("track", false);
				menu->setItemEnabled("Share", false);
				menu->setItemEnabled("Pay", false);
				menu->setItemEnabled("Block Unblock", false);
				menu->setItemEnabled("Mute Text", false);
			}
			else
			{
				if (mSessionID == LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, mAvatarID))
				{
					menu->setItemVisible("Send IM", false);
				}
				menu->setItemEnabled("Teleport to", FSCommon::checkIsActionEnabled(mAvatarID, FS_RGSTR_ACT_TELEPORT_TO));
				menu->setItemEnabled("Offer Teleport", LLAvatarActions::canOfferTeleport(mAvatarID));
				menu->setItemEnabled("Request Teleport", LLAvatarActions::canRequestTeleport(mAvatarID));
				menu->setItemEnabled("Voice Call", LLAvatarActions::canCall());
				menu->setItemEnabled("Zoom In", FSCommon::checkIsActionEnabled(mAvatarID, FS_RGSTR_ACT_ZOOM_IN));
				menu->setItemEnabled("track", FSCommon::checkIsActionEnabled(mAvatarID, FS_RGSTR_ACT_TRACK_AVATAR));
				menu->setItemEnabled("Block Unblock", LLAvatarActions::canBlock(mAvatarID));
				menu->setItemEnabled("Mute Text", LLAvatarActions::canBlock(mAvatarID));
				menu->setItemEnabled("Chat History", LLLogChat::isTranscriptExist(mAvatarID));
			}

			menu->setItemEnabled("Map", (LLAvatarTracker::instance().isBuddyOnline(mAvatarID) && is_agent_mappable(mAvatarID)) || gAgent.isGodlike() );
			menu->buildDrawLabels();
			menu->updateParent(LLMenuGL::sMenuContainer);
			LLMenuGL::showPopup(this, menu, x, y);
		}
	}

	void showInfoCtrl()
	{
//		const bool isVisible = !mAvatarID.isNull() && !mFrom.empty() && (CHAT_SOURCE_SYSTEM != mSourceType || mType == CHAT_TYPE_RADAR);;
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		const bool isVisible = mShowInfoCtrl && !mAvatarID.isNull() && !mFrom.empty() && (CHAT_SOURCE_SYSTEM != mSourceType || mType == CHAT_TYPE_RADAR);;
// [/RLVa:KB]
		if (isVisible)
		{
			const LLRect sticky_rect = mUserNameTextBox->getRect();
			S32 icon_x = llmin(sticky_rect.mLeft + mUserNameTextBox->getTextBoundingRect().getWidth() + 7, sticky_rect.mRight - 3);
			mInfoCtrl->setOrigin(icon_x, sticky_rect.getCenterY() - mInfoCtrl->getRect().getHeight() / 2 ) ;
		}
		mInfoCtrl->setVisible(isVisible);
	}

	void hideInfoCtrl()
	{
		mInfoCtrl->setVisible(FALSE);
	}

private:
	void setTimeField(const LLChat& chat)
	{
		LLRect rect_before = mTimeBoxTextBox->getRect();

		mTimeBoxTextBox->setValue(chat.mTimeStr);

		// set necessary textbox width to fit all text
		mTimeBoxTextBox->reshapeToFitText();
		LLRect rect_after = mTimeBoxTextBox->getRect();

		// move rect to the left to correct position...
		S32 delta_pos_x = rect_before.getWidth() - rect_after.getWidth();
		S32 delta_pos_y = rect_before.getHeight() - rect_after.getHeight();
		mTimeBoxTextBox->translate(delta_pos_x, delta_pos_y);

		//... & change width of the name control
		const LLRect& user_rect = mUserNameTextBox->getRect();
		mUserNameTextBox->reshape(user_rect.getWidth() + delta_pos_x, user_rect.getHeight());
	}

	void fetchAvatarName()
	{
		if (mAvatarID.notNull())
		{
			if (mAvatarNameCacheConnection.connected())
			{
				mAvatarNameCacheConnection.disconnect();
			}
			mAvatarNameCacheConnection = LLAvatarNameCache::get(mAvatarID,
				boost::bind(&FSChatHistoryHeader::onAvatarNameCache, this, _1, _2));
		}
	}

	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
	{
		mAvatarNameCacheConnection.disconnect();

		mFrom = av_name.getDisplayName();

		mUserNameTextBox->setValue( LLSD(mFrom) );
		mUserNameTextBox->setToolTip( av_name.getUserName() );

		if (gSavedSettings.getBOOL("NameTagShowUsernames") &&
			av_name.useDisplayNames() &&
			!av_name.isDisplayNameDefault())
		{
			LLStyle::Params style_params_name;
			LLColor4 userNameColor = LLUIColorTable::instance().getColor("EmphasisColor");
			style_params_name.color(userNameColor);
			style_params_name.font.name("SansSerifSmall");
			style_params_name.font.style(mNameStyleParams.font.style);
			style_params_name.readonly_color(userNameColor);
			mUserNameTextBox->appendText(" - " + av_name.getUserNameForDisplay(), false, style_params_name);
		}
		setToolTip( av_name.getUserName() );
		// name might have changed, update width
		updateMinUserNameWidth();
	}

protected:
	LLHandle<LLView>	mPopupMenuHandleAvatar;
	LLHandle<LLView>	mPopupMenuHandleObject;

	LLUICtrl*			mInfoCtrl;

	LLUUID				mAvatarID;
	LLSD				mObjectData;
	EChatSourceType		mSourceType;
	EChatType			mType; // FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
	std::string			mFrom;
	LLUUID				mSessionID;
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
	bool				mShowContextMenu;
	bool				mShowInfoCtrl;
// [/RLVa:KB]

	S32					mMinUserNameWidth;
	const LLFontGL*		mUserNameFont;
	LLTextBox*			mUserNameTextBox;
	LLTextBox*			mTimeBoxTextBox;

	LLStyle::Params		mNameStyleParams;

private:
	boost::signals2::connection mAvatarNameCacheConnection;
};

FSChatHistory::FSChatHistory(const FSChatHistory::Params& p)
:	LLTextEditor(p),	// <FS:Zi> FIRE-8600: TAB out of chat history
	mMessageHeaderFilename(p.message_header),
	mMessageSeparatorFilename(p.message_separator),
	mLeftTextPad(p.left_text_pad),
	mRightTextPad(p.right_text_pad),
	mLeftWidgetPad(p.left_widget_pad),
	mRightWidgetPad(p.right_widget_pad),
	mTopSeparatorPad(p.top_separator_pad),
	mBottomSeparatorPad(p.bottom_separator_pad),
	mTopHeaderPad(p.top_header_pad),
	mBottomHeaderPad(p.bottom_header_pad),
	mChatInputLine(NULL),	// <FS_Zi> FIRE-8602: Typing in chat history focuses chat input line
	mIsLastMessageFromLog(false),
	mScrollToBottom(false),
	mUnreadChatSources(0)
{
	mLineSpacingPixels = llclamp(gSavedSettings.getS32("FSFontChatLineSpacingPixels"), 0, 36);
	
	setIsObjectBlockedCallback(boost::bind(&LLMuteList::isMuted, LLMuteList::getInstance(), _1, _2, 0));
}

LLSD FSChatHistory::getValue() const
{
  LLSD* text=new LLSD(); 
  text->assign(getText());
  return *text;
}

FSChatHistory::~FSChatHistory()
{
	this->clear();
}

void FSChatHistory::initFromParams(const FSChatHistory::Params& p)
{
	// initialize the LLTextEditor base class first ... -Zi
	LLTextEditor::initFromParams(p);
	/// then set up these properties, because otherwise they would get lost when we
	/// do this in the constructor only -Zi
	setEnabled(false);
	setReadOnly(true);
	setShowContextMenu(true);
}

LLView* FSChatHistory::getSeparator()
{
	LLPanel* separator = LLUICtrlFactory::getInstance()->createFromFile<LLPanel>(mMessageSeparatorFilename, NULL, LLPanel::child_registry_t::instance());
	return separator;
}

LLView* FSChatHistory::getHeader(const LLChat& chat,const LLStyle::Params& style_params, const LLSD& args)
{
	FSChatHistoryHeader* header = FSChatHistoryHeader::createInstance(mMessageHeaderFilename);
	header->setup(chat, style_params, args);
	return header;
}

void FSChatHistory::clear()
{
	mLastFromName.clear();
	// workaround: Setting the text to an empty line before clear() gets rid of
	// the scrollbar, if present, which otherwise would get stuck until the next
	// line was appended. -Zi
	setText(std::string(" \n"));	// <FS:Zi> FIRE-8600: TAB out of chat history
	mLastFromID = LLUUID::null;
}

enum e_moderation_options
{
	NORMAL = 0,
	BOLD = 1,
	ITALIC = 2,
	BOLD_ITALIC = 3,
	UNDERLINE = 4,
	BOLD_UNDERLINE = 5,
	ITALIC_UNDERLINE = 6,
	BOLD_ITALIC_UNDERLINE = 7
};

std::string applyModeratorStyle(U32 moderator_style)
{	
	std::string style;
	switch (moderator_style)
	{
		case BOLD:
			style = "BOLD";
			break;
		case ITALIC:
			style = "ITALIC";
			break;
		case BOLD_ITALIC:
			style = "BOLDITALIC";
			break;
		case UNDERLINE:
			style = "UNDERLINE";
			break;
		case BOLD_UNDERLINE:
			style = "BOLDUNDERLINE";
			break;
		case ITALIC_UNDERLINE:
			style = "ITALICUNDERLINE";
			break;
		case BOLD_ITALIC_UNDERLINE:
			style = "BOLDITALICUNDERLINE";
			break;
		case NORMAL:
		default:
			style = "NORMAL";
			break;
	}
	return style;
}

static LLTrace::BlockTimerStatHandle FTM_APPEND_MESSAGE("Append Chat Message");

void FSChatHistory::appendMessage(const LLChat& chat, const LLSD &args, const LLStyle::Params& input_append_params)
{
	LL_RECORD_BLOCK_TIME(FTM_APPEND_MESSAGE);
	// Ansa: FIRE-12754: Hack around a weird issue where the doc size magically increases by 1px
	//       during draw if the doc exceeds the visible space and the scrollbar is getting visible.
	mScrollToBottom = (mScroller->isAtBottom() || mScroller->getScrollbar(LLScrollContainer::VERTICAL)->getDocPosMax() <= 1);

	bool use_plain_text_chat_history = args["use_plain_text_chat_history"].asBoolean();
	bool square_brackets = false; // square brackets necessary for a system messages
	bool is_p2p = args.has("is_p2p") && args["is_p2p"].asBoolean();
	bool is_conversation_log = args.has("conversation_log") && args["conversation_log"].asBoolean();	// <FS:CR> Don't dim chat in conversation log
	bool is_local = args.has("is_local") && args["is_local"].asBoolean();

	bool from_me = chat.mFromID == gAgent.getID();
	setPlainText(use_plain_text_chat_history);

	if (!scrolledToEnd() && !from_me && !chat.mFromName.empty())
	{
		mUnreadChatSources++;
		if (!mUnreadMessagesUpdateSignal.empty())
		{
			mUnreadMessagesUpdateSignal(mUnreadChatSources);
		}
	}

	LLColor4 txt_color = LLUIColorTable::instance().getColor("White");
	LLColor4 name_color = LLUIColorTable::instance().getColor("ChatNameColor");
	LLViewerChat::getChatColor(chat, txt_color, LLSD().with("is_local", is_local));
	LLFontGL* fontp = LLViewerChat::getChatFont();
	std::string font_name = LLFontGL::nameFromFont(fontp);
	std::string font_size = LLFontGL::sizeFromFont(fontp);

	LLStyle::Params body_message_params;
	body_message_params.color(txt_color);
	body_message_params.readonly_color(txt_color);
	body_message_params.font.name(font_name);
	body_message_params.font.size(font_size);
	body_message_params.font.style(input_append_params.font.style);

	LLStyle::Params name_params(body_message_params);
	name_params.color(name_color);
	name_params.readonly_color(name_color);

	// FS:LO FIRE-2899 - Faded text for IMs in nearby chat
	F32 FSIMChatHistoryFade = gSavedSettings.getF32("FSIMChatHistoryFade");

	if(FSIMChatHistoryFade > 1.0f)
	{
		FSIMChatHistoryFade = 1.0f;
		gSavedSettings.setF32("FSIMChatHistoryFade",FSIMChatHistoryFade);
	}
	// FS:LO FIRE-2899 - Faded text for IMs in nearby chat

	//IRC styled /me messages.
	bool irc_me = is_irc_me_prefix(chat.mText);

	// Delimiter after a name in header copy/past and in plain text mode
	std::string delimiter = ": ";
	std::string shout = LLTrans::getString("shout");
	std::string whisper = LLTrans::getString("whisper");
	if (chat.mChatType == CHAT_TYPE_SHOUT || 
		chat.mChatType == CHAT_TYPE_WHISPER ||
		// FS:TS FIRE-6049: No : in radar chat header
		chat.mChatType == CHAT_TYPE_RADAR ||
		// FS:TS FIRE-6049: No : in radar chat header
		chat.mText.compare(0, shout.length(), shout) == 0 ||
		chat.mText.compare(0, whisper.length(), whisper) == 0)
	{
		delimiter = " ";
	}

	// Don't add any delimiter after name in irc styled messages
	if (irc_me || chat.mChatStyle == CHAT_STYLE_IRC)
	{
		delimiter = LLStringUtil::null;

		// italics for emotes -Zi
		if(gSavedSettings.getBOOL("EmotesUseItalic"))
		{
			body_message_params.font.style = "ITALIC";
			name_params.font.style = "ITALIC";
		}
	}

	if (chat.mChatType == CHAT_TYPE_WHISPER && gSavedSettings.getBOOL("FSEmphasizeShoutWhisper"))
	{
		body_message_params.font.style = "ITALIC";
		name_params.font.style = "ITALIC";
	}
	else if(chat.mChatType == CHAT_TYPE_SHOUT && gSavedSettings.getBOOL("FSEmphasizeShoutWhisper"))
	{
		body_message_params.font.style = "BOLD";
		name_params.font.style = "BOLD";
	}

	bool message_from_log = chat.mChatStyle == CHAT_STYLE_HISTORY;
	// We graying out chat history by graying out messages that contains full date in a time string
	if (message_from_log && !is_conversation_log)
	{
		txt_color = LLColor4::grey;
		body_message_params.color(txt_color);
		body_message_params.readonly_color(txt_color);
		name_params.color(txt_color);
		name_params.readonly_color(txt_color);
	}

	//<FS:HG> FS-1734 seperate name and text styles for moderator
	bool moderator_style_active = false;
	U32 moderator_name_style_value = gSavedSettings.getU32("FSModNameStyle");
	U32 moderator_body_style_value = gSavedSettings.getU32("FSModTextStyle");
	std::string moderator_name_style = applyModeratorStyle(moderator_name_style_value);
	std::string moderator_body_style = applyModeratorStyle(moderator_body_style_value);

	if (chat.mChatStyle == CHAT_STYLE_MODERATOR)
	{
		moderator_style_active = true;

		name_params.font.style(moderator_name_style);
		body_message_params.font.style(moderator_body_style);

		if ( irc_me && gSavedSettings.getBOOL("EmotesUseItalic") )
		{
			if ( (ITALIC & moderator_name_style_value) != ITALIC )//HG: if ITALIC isn't one of the styles... add it
			{
				moderator_name_style += "ITALIC";
				name_params.font.style(moderator_name_style);
			}
			if ( (ITALIC & moderator_body_style_value) != ITALIC )
			{
				moderator_body_style += "ITALIC";
				body_message_params.font.style(moderator_body_style);
			}
			body_message_params.font.style(moderator_body_style);
		}
	}
	//</FS:HG> FS-1734 seperate name and text styles for moderator

	bool prependNewLineState = getText().size() != 0;

	// compact mode: show a timestamp and name
	if (use_plain_text_chat_history)
	{
		square_brackets = (chat.mSourceType == CHAT_SOURCE_SYSTEM && chat.mChatType != CHAT_TYPE_RADAR && gSavedSettings.getBOOL("FSIMSystemMessageBrackets"));

		LLStyle::Params timestamp_style(body_message_params);

		// out of the timestamp
		if (args["show_time"].asBoolean())
		{
			LLColor4 timestamp_color = LLUIColorTable::instance().getColor("ChatTimestampColor");
			timestamp_style.color(timestamp_color);
			timestamp_style.readonly_color(timestamp_color);
			if (message_from_log && !is_conversation_log)
			{
				timestamp_style.color.alpha = FSIMChatHistoryFade;
				timestamp_style.readonly_color.alpha = FSIMChatHistoryFade;
			}
			//<FS:HG> FS-1734 seperate name and text styles for moderator
			if ( moderator_style_active )
			{
				timestamp_style.font.style(moderator_name_style);
			}
			//</FS:HG> FS-1734 seperate name and text styles for moderator
			appendText("[" + chat.mTimeStr + "] ", prependNewLineState, timestamp_style);
			prependNewLineState = false;
		}

        // out the opening square bracket (if need)
		if (square_brackets)
		{
			appendText("[", prependNewLineState, body_message_params);
			prependNewLineState = false;
		}

		// names showing
		if ( (!is_p2p || args["show_names_for_p2p_conv"].asBoolean()) &&
			utf8str_trim(chat.mFromName).size() != 0)
		{
			// Don't hotlink any messages from the system (e.g. "Second Life:"), so just add those in plain text.
			if ( chat.mSourceType == CHAT_SOURCE_OBJECT && chat.mFromID.notNull())
			{
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
				// NOTE-RLVa: we don't need to do any @shownames or @showloc filtering here because we'll already have an existing URL
				std::string url = chat.mURL;
				RLV_ASSERT( (url.empty()) || (std::string::npos != url.find("objectim")) );
				if ( (url.empty()) || (std::string::npos == url.find("objectim")) )
				{
// [/RLVa:KB]
					// for object IMs, create a secondlife:///app/objectim SLapp
					/*std::string*/ url = LLViewerChat::getSenderSLURL(chat, args);
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
				}
// [/RLVa:KB]

				// set the link for the object name to be the objectim SLapp
				// (don't let object names with hyperlinks override our objectim Url)
				LLStyle::Params link_params(body_message_params);
				link_params.color.control = "ChatNameObjectColor";
				LLColor4 link_color = LLUIColorTable::instance().getColor("ChatNameObjectColor");
				link_params.color = link_color;
				link_params.readonly_color = link_color;
				if (message_from_log && !is_conversation_log)
				{
					link_params.color.alpha = FSIMChatHistoryFade;
					link_params.readonly_color.alpha = FSIMChatHistoryFade;
				}
				link_params.is_link = true;
				link_params.link_href = url;

				appendText(chat.mFromName + delimiter, prependNewLineState, link_params);	// <FS:Zi> FIRE-8600: TAB out of chat history
				prependNewLineState = false;
			}
//			else if (chat.mFromName != SYSTEM_FROM && chat.mFromID.notNull() && !message_from_log)
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
			else if (chat.mFromName != SYSTEM_FROM && chat.mFromID.notNull() && !message_from_log && !chat.mRlvNamesFiltered)
// [/RLVa:KB]
			{
				name_params.color = name_color;
				name_params.readonly_color = name_color;
				if (chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP)
				{
					name_params.color.alpha = FSIMChatHistoryFade;
					name_params.readonly_color.alpha = FSIMChatHistoryFade;
					appendText(LLTrans::getString("IMPrefix") + " ", prependNewLineState, name_params);
					prependNewLineState = false;
					
					if (!chat.mFromNameGroup.empty())
					{
						appendText(chat.mFromNameGroup, prependNewLineState, name_params);
						prependNewLineState = false;
					}
				}
				
				name_params.is_name_slurl = true;
				name_params.link_href = LLSLURL("agent", chat.mFromID, "inspect").getSLURLString();

				if (from_me && gSavedSettings.getBOOL("FSChatHistoryShowYou"))
				{
					appendText(LLTrans::getString("AgentNameSubst"), prependNewLineState, name_params);
				}
				else
				{
					// Add link to avatar's inspector and delimiter to message.
					appendText(std::string(name_params.link_href), prependNewLineState, name_params);
				}

				prependNewLineState = false;

				if (delimiter.length() > 0 && delimiter[0] == ':')
				{
					LLStyle::Params delimiter_params(body_message_params);
					delimiter_params.font.style = name_params.font.style;

					appendText(":", prependNewLineState, delimiter_params);
					prependNewLineState = false;

					appendText(delimiter.substr(1, std::string::npos), prependNewLineState, body_message_params);
				}
				else
				{
					appendText(delimiter, prependNewLineState, body_message_params);
				}
				prependNewLineState = false;
			}
			else
			{
				appendText("<nolink>" + chat.mFromName + "</nolink>" + delimiter,
						prependNewLineState, body_message_params);
				prependNewLineState = false;
			}
		}
	}
	else // showing timestamp and name in the expanded mode
	{
		prependNewLineState = false;
		LLView* view = NULL;
		LLInlineViewSegment::Params p;
		p.force_newline = true;
		p.left_pad = mLeftWidgetPad;
		p.right_pad = mRightWidgetPad;

		LLDate new_message_time = LLDate::now();

		if (mLastFromName == chat.mFromName 
			&& mLastFromID == chat.mFromID
			&& mLastMessageTime.notNull() 
			&& (new_message_time.secondsSinceEpoch() - mLastMessageTime.secondsSinceEpoch()) < 60.0
			&& mIsLastMessageFromLog == message_from_log)  //distinguish between current and previous chat session's histories
		{
			view = getSeparator();
			p.top_pad = mTopSeparatorPad;
			p.bottom_pad = mBottomSeparatorPad;
		}
		else
		{
			view = getHeader(chat, name_params, args);
			if (getLength() == 0)
				p.top_pad = 0;
			else
				p.top_pad = mTopHeaderPad;
			p.bottom_pad = mBottomHeaderPad;
		}
		p.view = view;

		//Prepare the rect for the view
		LLRect target_rect = getDocumentView()->getRect();	// <FS:Zi> FIRE-8600: TAB out of chat history
		// squeeze down the widget by subtracting padding off left and right
		target_rect.mLeft += mLeftWidgetPad + getHPad();	// <FS:Zi> FIRE-8600: TAB out of chat history
		target_rect.mRight -= mRightWidgetPad;
		view->reshape(target_rect.getWidth(), view->getRect().getHeight());
		view->setOrigin(target_rect.mLeft, view->getRect().mBottom);

		std::string widget_associated_text = "\n[" + chat.mTimeStr + "] ";
		if (utf8str_trim(chat.mFromName).size() != 0 && chat.mFromName != SYSTEM_FROM)
			widget_associated_text += chat.mFromName + delimiter;

		appendWidget(p, widget_associated_text, false);	// <FS:Zi> FIRE-8600: TAB out of chat history

		mLastFromName = chat.mFromName;
		mLastFromID = chat.mFromID;
		mLastMessageTime = new_message_time;
		mIsLastMessageFromLog = message_from_log;
	}

	// body of the message processing

	// notify processing
	if (chat.mNotifId.notNull())
	{
		LLNotificationPtr notification = LLNotificationsUtil::find(chat.mNotifId);
		if (notification != NULL)
		{
			LLIMToastNotifyPanel* notify_box = new LLIMToastNotifyPanel(
					notification, chat.mSessionID, LLRect::null, true, this);

			//Prepare the rect for the view
			LLRect target_rect = getDocumentView()->getRect();	// <FS:Zi> FIRE-8600: TAB out of chat history
			// squeeze down the widget by subtracting padding off left and right
			target_rect.mLeft += mLeftWidgetPad + getHPad();	// <FS:Zi> FIRE-8600: TAB out of chat history
			target_rect.mRight -= mRightWidgetPad;
			notify_box->reshape(target_rect.getWidth(),	notify_box->getRect().getHeight());
			notify_box->setOrigin(target_rect.mLeft, notify_box->getRect().mBottom);

			LLInlineViewSegment::Params params;
			params.view = notify_box;
			params.left_pad = mLeftWidgetPad;
			params.right_pad = mRightWidgetPad;
			appendWidget(params, "\n", false);	// <FS:Zi> FIRE-8600: TAB out of chat history
		}
	}

	// usual messages showing
	else
	{
		std::string message = irc_me ? chat.mText.substr(3) : chat.mText;

		//MESSAGE TEXT PROCESSING
		//*HACK getting rid of redundant sender names in system notifications sent using sender name (see EXT-5010)
		if (use_plain_text_chat_history && !from_me && chat.mFromID.notNull())
		{
			std::string slurl_about = SLURL_APP_AGENT + chat.mFromID.asString() + SLURL_ABOUT;
			if (message.length() > slurl_about.length() && 
				message.compare(0, slurl_about.length(), slurl_about) == 0)
			{
				message = message.substr(slurl_about.length(), message.length()-1);
			}
		}

		if (irc_me && !use_plain_text_chat_history)
		{
			if (chat.mSourceType == CHAT_SOURCE_AGENT)
			{
				static LLCachedControl<bool> useDisplayNames(gSavedSettings, "UseDisplayNames");
				static LLCachedControl<bool> nameTagShowUsernames(gSavedSettings, "NameTagShowUsernames");

				std::string name_format = (useDisplayNames && nameTagShowUsernames) ? "completename" : "displayname";
				if (is_local && gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					name_format = "rlvanonym";
				}
				message = LLSLURL("agent", chat.mFromID, name_format).getSLURLString() + message;
			}
			else
			{
				message = chat.mFromName + message;
			}
		}

		if(chat.mSourceType != CHAT_SOURCE_OBJECT && (chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP)) // FS::LO Fix for FIRE-6334; Fade IM Text into background of chat history default setting should not be 0.5; made object IM text not fade into the background as per phoenix behavior.
		{
			body_message_params.color.alpha = FSIMChatHistoryFade;
			body_message_params.readonly_color.alpha = FSIMChatHistoryFade;
			body_message_params.selected_color.alpha = FSIMChatHistoryFade;
		}
		// FS:LO FIRE-2899 - Faded text for IMs in nearby chat

		if (square_brackets)
		{
			message += "]";
		}

		bool is_trusted = isContentTrusted();
		setContentTrusted(chat.mFromID.isNull() && is_p2p); // <FS:Ansariel> Set trusted content temporarily for system messages
		setPlainText((use_plain_text_chat_history && is_p2p) ? chat.mFromID.notNull() : use_plain_text_chat_history);
		appendText(message, prependNewLineState, body_message_params);	// <FS:Zi> FIRE-8600: TAB out of chat history
		setContentTrusted(is_trusted);
		setPlainText(use_plain_text_chat_history);
		// Uncomment this if we never need to append to the end of a message. [FS:CR]
		//prependNewLineState = false;
	}

	blockUndo();	// <FS:Zi> FIRE-8600: TAB out of chat history

	// automatically scroll to end when receiving chat from myself
	if (from_me)
	{
		setCursorAndScrollToEnd();	// <FS:Zi> FIRE-8600: TAB out of chat history
	}
}

// <FS_Zi> FIRE-8602: Typing in chat history focuses chat input line
BOOL FSChatHistory::handleUnicodeCharHere(llwchar uni_char)
{
	// do not change focus when the CTRL key is used to make copy/select all etc. possible
	if(gKeyboard->currentMask(false) & MASK_CONTROL)
	{
		// instead, let the base class handle things
		return LLTextEditor::handleUnicodeCharHere(uni_char);
	}

	// we don't know which is our chat input line yet
	if(!mChatInputLine)
	{
		// get our focus root
		LLUICtrl* focusRoot=findRootMostFocusRoot();
		if(focusRoot)
		{
			// focus on the next item that is a text input control
			focusRoot->focusNextItem(true);
			// remember the control's pointer if it really is a LLLineEditor
			mChatInputLine = dynamic_cast<LLChatEntry*>(gFocusMgr.getKeyboardFocus());
		}
	}

	// do we know our chat input line now?
	if(mChatInputLine)
	{
		// we do, so focus on it
		mChatInputLine->setFocus(true);
		// and let it handle the keystroke
		return mChatInputLine->handleUnicodeCharHere(uni_char);
	}

	// we don't know what to do, so let our base class handle the keystroke
	return LLTextEditor::handleUnicodeCharHere(uni_char);
}

void FSChatHistory::draw()
{
	LLTextEditor::draw();
	// Ansa: FIRE-12754: Hack around a weird issue where the doc size magically increases by 1px
	//       during draw if the doc exceeds the visible space and the scrollbar is getting visible.
	if (mScrollToBottom)
	{
		mScroller->goToBottom();
		mScrollToBottom = false;
	}

	if (scrolledToEnd() && mUnreadChatSources != 0)
	{
		mUnreadChatSources = 0;
		if (!mUnreadMessagesUpdateSignal.empty())
		{
			mUnreadMessagesUpdateSignal(mUnreadChatSources);
		}
	}
}
// </FS:Zi>

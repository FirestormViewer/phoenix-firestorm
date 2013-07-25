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
#include "llavatariconctrl.h"
#include "llcallingcard.h" //for LLAvatarTracker
#include "llagentdata.h"
#include "llavataractions.h"
#include "lltrans.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llmutelist.h"
#include "llstylemap.h"
#include "llslurl.h"
#include "lllayoutstack.h"
#include "llagent.h"
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
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.0f)
#include "rlvcommon.h"
// [/RLVa:KB]

// llviewernetwork.h : SJ: Needed to find the grid we are running on
#include "llviewernetwork.h"

// <FS_Zi> FIRE-8602: Typing in chat history focuses chat input line
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llchatentry.h"
// </FS:Zi>
#include "llpanelblockedlist.h"

static LLDefaultChildRegistry::Register<FSChatHistory> r("fs_chat_history");

const static std::string NEW_LINE(rawstr_to_utf8("\n"));

const static std::string SLURL_APP_AGENT = "secondlife:///app/agent/";
const static std::string SLURL_ABOUT = "/about";

// <FS:CR> Moved this to a small function
std::string prefixIM(std::string from, const LLChat& chat)
{
	if (chat.mChatType == CHAT_TYPE_IM)
	{
		from = LLTrans::getString("IMPrefix") + from;
	}
	else if (chat.mChatType == CHAT_TYPE_IM_GROUP)
	{
		from = LLTrans::getString("IMPrefix") + chat.mFromNameGroup + from;
	}
	
	return from;
}
// </FS:CR>

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
		// Detach the info button so that it doesn't get destroyed (EXT-8463).
		hideInfoCtrl();

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
		else if (level == "add")
		{
			LLAvatarActions::requestFriendshipDialog(getAvatarId(), mFrom);
		}
		else if (level == "remove")
		{
			LLAvatarActions::removeFriendDialog(getAvatarId());
		}
	}

	BOOL postBuild()
	{
		LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;

		registrar.add("AvatarIcon.Action", boost::bind(&FSChatHistoryHeader::onAvatarIconContextMenuItemClicked, this, _2));
		registrar.add("ObjectIcon.Action", boost::bind(&FSChatHistoryHeader::onObjectIconContextMenuItemClicked, this, _2));

		LLMenuGL* menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_avatar_icon.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
		mPopupMenuHandleAvatar = menu->getHandle();

		menu = LLUICtrlFactory::getInstance()->createFromFile<LLMenuGL>("menu_object_icon.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
		mPopupMenuHandleObject = menu->getHandle();

		setDoubleClickCallback(boost::bind(&FSChatHistoryHeader::showInspector, this));

		setMouseEnterCallback(boost::bind(&FSChatHistoryHeader::showInfoCtrl, this));
		setMouseLeaveCallback(boost::bind(&FSChatHistoryHeader::hideInfoCtrl, this));

		mUserNameTextBox = getChild<LLTextBox>("user_name");
		mTimeBoxTextBox = getChild<LLTextBox>("time_box");

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
			LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", mAvatarID));
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


	const LLUUID&		getAvatarId () const { return mAvatarID;}

	void setup(const LLChat& chat, const LLStyle::Params& style_params, const LLSD& args)
	{
		mAvatarID = chat.mFromID;
		mSessionID = chat.mSessionID;
		mSourceType = chat.mSourceType;
		mType = chat.mChatType; // FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports

		//*TODO overly defensive thing, source type should be maintained out there
		if((chat.mFromID.isNull() && chat.mFromName.empty()) || (chat.mFromName == SYSTEM_FROM && chat.mFromID.isNull()))
		{
			mSourceType = CHAT_SOURCE_SYSTEM;
		}  

		mUserNameFont = style_params.font();
		LLTextBox* user_name = getChild<LLTextBox>("user_name");
		user_name->setReadOnlyColor(style_params.readonly_color());
		user_name->setColor(style_params.color());

		if (chat.mFromName.empty()
			//|| mSourceType == CHAT_SOURCE_SYSTEM
			// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
			|| (mSourceType == CHAT_SOURCE_SYSTEM && mType != CHAT_TYPE_RADAR)
			|| mAvatarID.isNull())
		{
			//mFrom = LLTrans::getString("SECOND_LIFE");
			//[FIX FIRE-2852] Changed function to find the right Gridname
			mFrom = LLGridManager::getInstance()->getGridLabel();
			user_name->setValue(mFrom);
			updateMinUserNameWidth();
		}
		else if (mSourceType == CHAT_SOURCE_AGENT
				 && !mAvatarID.isNull()
				 && chat.mChatStyle != CHAT_STYLE_HISTORY)
		{
			// ...from a normal user, lookup the name and fill in later.
			// *NOTE: Do not do this for chat history logs, otherwise the viewer
			// will flood the People API with lookup requests on startup

			// Start with blank so sample data from XUI XML doesn't
			// flash on the screen
//			user_name->setValue( LLSD() );
//			fetchAvatarName(chat);
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
			if (!chat.mRlvNamesFiltered)
			{
				user_name->setValue( LLSD() );
				fetchAvatarName(chat);
			}
			else
			{
				// If the agent's chat was subject to @shownames=n we should display their anonimized name
				mFrom = prefixIM(chat.mFromName, chat);	// <FS:CR> Prefix im's and group chats for the console
				user_name->setValue(mFrom);
				user_name->setToolTip(mFrom);
				setToolTip(mFrom);
				updateMinUserNameWidth();
			}
// [/RLVa:KB]
		}
		else if (chat.mChatStyle == CHAT_STYLE_HISTORY ||
				 mSourceType == CHAT_SOURCE_AGENT)
		{
			//if it's an avatar name with a username add formatting
			S32 username_start = chat.mFromName.rfind(" (");
			S32 username_end = chat.mFromName.rfind(')');
			
			if (username_start != std::string::npos &&
				username_end == (chat.mFromName.length() - 1))
			{
				mFrom = chat.mFromName.substr(0, username_start);
				user_name->setValue(mFrom);
			}
			else
			{
				// If the agent's chat was subject to @shownames=n we should display their anonimized name
				mFrom = chat.mFromName;
				mFrom = prefixIM(mFrom, chat);
				user_name->setValue(mFrom);
				updateMinUserNameWidth();
			}
// [/RLVa:KB]
		}
		else
		{
			// ...from an object, just use name as given
			mFrom = chat.mFromName;
			user_name->setValue(mFrom);
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
		if ( (chat.mRlvNamesFiltered) && ((CHAT_SOURCE_AGENT == mSourceType) || (CHAT_SOURCE_OBJECT == mSourceType))  )
		{
			mShowInfoCtrl = mShowContextMenu = false;
			icon->setDrawTooltip(false);
		}
// [/RLVa:KB]

		switch (mSourceType)
		{
			case CHAT_SOURCE_AGENT:
				icon->setValue(chat.mFromID);
				break;
			case CHAT_SOURCE_OBJECT:
				icon->setValue(LLSD("OBJECT_Icon"));
				break;
			case CHAT_SOURCE_SYSTEM:
				//icon->setValue(LLSD("SL_Logo"));
				// FS:LO FIRE-1439 - Clickable avatar names on local chat radar crossing reports
				if(chat.mChatType == CHAT_TYPE_RADAR)
				{
					icon->setValue(chat.mFromID);
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
		LLTextBox* user_name = mUserNameTextBox; //getChild<LLTextBox>("user_name");
		LLTextBox* time_box = mTimeBoxTextBox; //getChild<LLTextBox>("time_box");

		LLRect user_name_rect = user_name->getRect();
		S32 user_name_width = user_name_rect.getWidth();
		S32 time_box_width = time_box->getRect().getWidth();

		if (!time_box->getVisible() && user_name_width > mMinUserNameWidth)
		{
			user_name_rect.mRight -= time_box_width;
			user_name->reshape(user_name_rect.getWidth(), user_name_rect.getHeight());
			user_name->setRect(user_name_rect);

			time_box->setVisible(TRUE);
		}

		LLPanel::draw();
	}

	void updateMinUserNameWidth()
	{
		if (mUserNameFont)
		{
			LLTextBox* user_name = getChild<LLTextBox>("user_name");
			const LLWString& text = user_name->getWText();
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
		if(mAvatarID.notNull() && mSourceType == CHAT_SOURCE_OBJECT && SYSTEM_FROM != mFrom)
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
			bool is_friend = LLAvatarTracker::instance().getBuddyInfo(mAvatarID) != NULL;
			
			menu->setItemEnabled("Add Friend", !is_friend);
			menu->setItemEnabled("Remove Friend", is_friend);

			if(gAgentID == mAvatarID)
			{
				menu->setItemEnabled("Add Friend", false);
				menu->setItemEnabled("Send IM", false);
				menu->setItemEnabled("Remove Friend", false);
			}

			if (mSessionID == LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, mAvatarID))
			{
				menu->setItemVisible("Send IM", false);
			}

			menu->buildDrawLabels();
			menu->updateParent(LLMenuGL::sMenuContainer);
			LLMenuGL::showPopup(this, menu, x, y);
		}
	}

	void showInfoCtrl()
	{
//		if (mAvatarID.isNull() || mFrom.empty() || CHAT_SOURCE_SYSTEM == mSourceType) return;
// [RLVa:KB] - Checked: 2010-04-22 (RLVa-1.2.2a) | Added: RLVa-1.2.0f
		if ( (!mShowInfoCtrl) || (mAvatarID.isNull() || mFrom.empty() || CHAT_SOURCE_SYSTEM == mSourceType) ) return;
// [/RLVa:KB]
				
		if (!sInfoCtrl)
		{
			// *TODO: Delete the button at exit.
			sInfoCtrl = LLUICtrlFactory::createFromFile<LLUICtrl>("inspector_info_ctrl.xml", NULL, LLPanel::child_registry_t::instance());
			if (sInfoCtrl)
			{
				sInfoCtrl->setCommitCallback(boost::bind(&FSChatHistoryHeader::onClickInfoCtrl, sInfoCtrl));
			}
		}

		if (!sInfoCtrl)
		{
			llassert(sInfoCtrl != NULL);
			return;
		}

		LLTextBox* name = getChild<LLTextBox>("user_name");
		LLRect sticky_rect = name->getRect();
		S32 icon_x = llmin(sticky_rect.mLeft + name->getTextBoundingRect().getWidth() + 7, sticky_rect.mRight - 3);
		sInfoCtrl->setOrigin(icon_x, sticky_rect.getCenterY() - sInfoCtrl->getRect().getHeight() / 2 ) ;
		addChild(sInfoCtrl);
	}

	void hideInfoCtrl()
	{
		if (!sInfoCtrl) return;

		if (sInfoCtrl->getParent() == this)
		{
			removeChild(sInfoCtrl);
		}
	}

private:
	void setTimeField(const LLChat& chat)
	{
		LLTextBox* time_box = getChild<LLTextBox>("time_box");

		LLRect rect_before = time_box->getRect();

		time_box->setValue(chat.mTimeStr);

		// set necessary textbox width to fit all text
		time_box->reshapeToFitText();
		LLRect rect_after = time_box->getRect();

		// move rect to the left to correct position...
		S32 delta_pos_x = rect_before.getWidth() - rect_after.getWidth();
		S32 delta_pos_y = rect_before.getHeight() - rect_after.getHeight();
		time_box->translate(delta_pos_x, delta_pos_y);

		//... & change width of the name control
		LLView* user_name = getChild<LLView>("user_name");
		const LLRect& user_rect = user_name->getRect();
		user_name->reshape(user_rect.getWidth() + delta_pos_x, user_rect.getHeight());
	}

	void fetchAvatarName(const LLChat& chat)
	{
		if (mAvatarID.notNull())
		{
			if (mAvatarNameCacheConnection.connected())
			{
				mAvatarNameCacheConnection.disconnect();
			}
			mAvatarNameCacheConnection = LLAvatarNameCache::get(mAvatarID,
				boost::bind(&FSChatHistoryHeader::onAvatarNameCache, this, _1, _2, chat.mChatType, chat.mFromNameGroup)); // FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		}
	}

	void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name, EChatType chat_type, std::string& group)
	{
		mAvatarNameCacheConnection.disconnect();
		mFrom = "";
		if (chat_type == CHAT_TYPE_IM || chat_type == CHAT_TYPE_IM_GROUP)
		{
			mFrom = LLTrans::getString("IMPrefix");
			if(!group.empty())
			{
				mFrom.append(group);
			}
		}
		mFrom.append(av_name.getDisplayName());
		// FS:LO FIRE-5230 - Chat Console Improvement: Replacing the "IM" in front of group chat messages with the actual group name
		
		LLTextBox* user_name = getChild<LLTextBox>("user_name");
		user_name->setValue( LLSD(mFrom) );
		user_name->setToolTip( av_name.getUserName() );

		if (gSavedSettings.getBOOL("NameTagShowUsernames") &&
			av_name.useDisplayNames() &&
			!av_name.isDisplayNameDefault())
		{
			LLStyle::Params style_params_name;
			LLColor4 userNameColor = LLUIColorTable::instance().getColor("EmphasisColor");
			style_params_name.color(userNameColor);
			style_params_name.font.name("SansSerifSmall");
			style_params_name.font.style("NORMAL");
			style_params_name.readonly_color(userNameColor);
			user_name->appendText("  - " + av_name.getUserName(), false, style_params_name);
		}
		setToolTip( av_name.getUserName() );
		// name might have changed, update width
		updateMinUserNameWidth();
	}

protected:
	LLHandle<LLView>	mPopupMenuHandleAvatar;
	LLHandle<LLView>	mPopupMenuHandleObject;

	static LLUICtrl*	sInfoCtrl;

	LLUUID			    mAvatarID;
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

private:
	boost::signals2::connection mAvatarNameCacheConnection;
};

LLUICtrl* FSChatHistoryHeader::sInfoCtrl = NULL;

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
	mNotifyAboutUnreadMsg(p.notify_unread_msg)
{
	mLineSpacingPixels=llclamp(gSavedSettings.getS32("FSFontChatLineSpacingPixels"),0,36);
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

static LLFastTimer::DeclareTimer FTM_APPEND_MESSAGE("Append Chat Message");

void FSChatHistory::appendMessage(const LLChat& chat, const LLSD &args, const LLStyle::Params& input_append_params)
{
	LLFastTimer _(FTM_APPEND_MESSAGE);
	bool use_plain_text_chat_history = args["use_plain_text_chat_history"].asBoolean();
	bool square_brackets = false; // square brackets necessary for a system messages
	bool is_p2p = args.has("is_p2p") && args["is_p2p"].asBoolean();
	bool is_conversation_log = args.has("conversation_log") && args["conversation_log"].asBoolean();	// <FS:CR> Don't dim chat in conversation log

	bool from_me = chat.mFromID == gAgent.getID();
	setPlainText(use_plain_text_chat_history);	// <FS:Zi> FIRE-8600: TAB out of chat history

	LLColor4 txt_color = LLUIColorTable::instance().getColor("White");
	LLColor4 name_color = LLUIColorTable::instance().getColor("ChatNameColor");
	LLViewerChat::getChatColor(chat,txt_color);
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

	std::string prefix = chat.mText.substr(0, 4);

	// FS:LO FIRE-2899 - Faded text for IMs in nearby chat
	F32 FSIMChatHistoryFade = gSavedSettings.getF32("FSIMChatHistoryFade");

	if(FSIMChatHistoryFade > 1.0f)
	{
		FSIMChatHistoryFade = 1.0f;
		gSavedSettings.setF32("FSIMChatHistoryFade",FSIMChatHistoryFade);
	}
	// FS:LO FIRE-2899 - Faded text for IMs in nearby chat

	//IRC styled /me messages.
	bool irc_me = (prefix == "/me " || prefix == "/me'"
	 || prefix == "/ME " || prefix == "/ME'");	// <FS>

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
			body_message_params.font.style = "ITALIC";
	}

	if (chat.mChatType == CHAT_TYPE_WHISPER && gSavedSettings.getBOOL("FSEmphasizeShoutWhisper"))
	{
			body_message_params.font.style = "ITALIC";
	}
	else if(chat.mChatType == CHAT_TYPE_SHOUT && gSavedSettings.getBOOL("FSEmphasizeShoutWhisper"))
	{
			body_message_params.font.style = "BOLD";
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
				body_message_params.font.style(moderator_name_style);
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
		square_brackets = (chat.mFromName == SYSTEM_FROM && gSavedSettings.getBOOL("FSIMSystemMessageBrackets"));

		LLStyle::Params timestamp_style(body_message_params);

		// out of the timestamp
		if (args["show_time"].asBoolean())
		{
			LLColor4 timestamp_color = LLUIColorTable::instance().getColor("ChatTimestampColor");
			timestamp_style.color(timestamp_color);
			timestamp_style.readonly_color(timestamp_color);
			if (message_from_log && is_conversation_log)
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
				link_params.color.control = "HTMLLinkColor";
				LLColor4 link_color = LLUIColorTable::instance().getColor("HTMLLinkColor");
				link_params.color = link_color;
				link_params.readonly_color = link_color;
				if (message_from_log && is_conversation_log)
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
				LLStyle::Params link_params(body_message_params);
				link_params.overwriteFrom(LLStyleMap::instance().lookupAgent(chat.mFromID));

				if (from_me && gSavedSettings.getBOOL("FSChatHistoryShowYou"))
				{
					std::string localized_name;
					bool is_localized = LLTrans::findString(localized_name, "AgentNameSubst");
					appendText((is_localized? localized_name:"(You)") + delimiter,
							prependNewLineState, link_params);
					prependNewLineState = false;
				}
				else
				{
				// Add link to avatar's inspector and delimiter to message.
					// <FS:Ansariel> Append delimiter with different style params or
					//               it will be replaced with the avatar name once it's
					//               returned from the server!
					//appendText(std::string(link_params.link_href) + delimiter,
					//		prependNewLineState, link_params);
					appendText(std::string(link_params.link_href), prependNewLineState, link_params);
					appendText(delimiter, prependNewLineState, body_message_params);
					// </FS:Ansariel>
					prependNewLineState = false;
				}
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

		std::string tmp_from_name(chat.mFromName);
		tmp_from_name = prefixIM(tmp_from_name, chat);
		if (mLastFromName == tmp_from_name 
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
		
		//if (chat.mChatType == CHAT_TYPE_IM) mLastFromName = LLTrans::getString("IMPrefix") + mLastFromName;
		mLastFromName = prefixIM(mLastFromName, chat);
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
					notification, chat.mSessionID, LLRect::null, !use_plain_text_chat_history);

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
			message = chat.mFromName + message;
		}

		// <FS:Ansariel> Optional muted chat history
		if (chat.mMuted)
		{
			LLUIColor muted_text_color = LLUIColorTable::instance().getColor("ChatHistoryMutedTextColor");
			body_message_params.color = muted_text_color;
			body_message_params.readonly_color = muted_text_color;
			body_message_params.selected_color = muted_text_color;
		}
		// </FS:Ansariel> Optional muted chat history

		if(chat.mSourceType != CHAT_SOURCE_OBJECT && (chat.mChatType == CHAT_TYPE_IM || chat.mChatType == CHAT_TYPE_IM_GROUP)) // FS::LO Fix for FIRE-6334; Fade IM Text into background of chat history default setting should not be 0.5; made object IM text not fade into the background as per phoenix behavior.
		{
			body_message_params.color.alpha = FSIMChatHistoryFade;
			body_message_params.readonly_color.alpha = FSIMChatHistoryFade;
			body_message_params.selected_color.alpha = FSIMChatHistoryFade;
		}
		// FS:LO FIRE-2899 - Faded text for IMs in nearby chat

		// <FS:PP> FIRE-7625: Option to display group chats, IM sessions and nearby chat always in uppercase
		static LLCachedControl<bool> sFSChatsUppercase(gSavedSettings, "FSChatsUppercase");
		if (sFSChatsUppercase)
		{
			LLStringUtil::toUpper(message);
			LLStringUtil::toUpper(mLastFromName);
		}
		// </FS:PP>
		if (square_brackets)
		{
			message += "]";
		}

		appendText(message, prependNewLineState, body_message_params);	// <FS:Zi> FIRE-8600: TAB out of chat history
		prependNewLineState = false;
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
// </FS:Zi>

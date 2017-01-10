/** 
 * @file llfloaterbump.cpp
 * @brief Floater showing recent bumps, hits with objects, pushes, etc.
 * @author Cory Ondrejka, James Cook
 *
 * $LicenseInfo:firstyear=2003&license=viewerlgpl$
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

#include "llsd.h"
#include "mean_collision_data.h"

#include "llavataractions.h"
#include "llfloaterbump.h"
#include "llfloaterreg.h"
#include "llfloaterreporter.h"
#include "llmutelist.h"
#include "llpanelblockedlist.h"
#include "llscrolllistctrl.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llviewermessage.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"

#include "fsradar.h"
#include "fsscrolllistctrl.h"
#include "llclipboard.h"
#include "llfloaterreg.h"

///----------------------------------------------------------------------------
/// Class LLFloaterBump
///----------------------------------------------------------------------------

// Default constructor
LLFloaterBump::LLFloaterBump(const LLSD& key) 
:	LLFloater(key),
	// <FS:Ansariel> Instant bump list floater update
	mDirty(false),
	mList(NULL)
	// </FS:Ansariel>
{
	// <FS:Ansariel> Improved bump list
	//mCommitCallbackRegistrar.add("Avatar.SendIM", boost::bind(&LLFloaterBump::startIM, this));
	//mCommitCallbackRegistrar.add("Avatar.ReportAbuse", boost::bind(&LLFloaterBump::reportAbuse, this));
	//mCommitCallbackRegistrar.add("ShowAgentProfile", boost::bind(&LLFloaterBump::showProfile, this));
	//mCommitCallbackRegistrar.add("Avatar.InviteToGroup", boost::bind(&LLFloaterBump::inviteToGroup, this));
	//mCommitCallbackRegistrar.add("Avatar.Call", boost::bind(&LLFloaterBump::startCall, this));
	//mEnableCallbackRegistrar.add("Avatar.EnableCall", boost::bind(&LLAvatarActions::canCall));
	//mCommitCallbackRegistrar.add("Avatar.AddFriend", boost::bind(&LLFloaterBump::addFriend, this));
	//mEnableCallbackRegistrar.add("Avatar.EnableAddFriend", boost::bind(&LLFloaterBump::enableAddFriend, this));
	//mCommitCallbackRegistrar.add("Avatar.Mute", boost::bind(&LLFloaterBump::muteAvatar, this));
	//mEnableCallbackRegistrar.add("Avatar.EnableMute", boost::bind(&LLFloaterBump::enableMute, this));
	//mCommitCallbackRegistrar.add("PayObject", boost::bind(&LLFloaterBump::payAvatar, this));
	//mCommitCallbackRegistrar.add("Tools.LookAtSelection", boost::bind(&LLFloaterBump::zoomInAvatar, this));
	// </FS:Ansariel>
}


// Destroys the object
LLFloaterBump::~LLFloaterBump()
{
}

BOOL LLFloaterBump::postBuild()
{
	// <FS:Ansariel> FIRE-13888: Add copy function to bumps list
	//mList = getChild<LLScrollListCtrl>("bump_list");
	//mList->setAllowMultipleSelection(false);
	//mList->setRightMouseDownCallback(boost::bind(&LLFloaterBump::onScrollListRightClicked, this, _1, _2, _3));

	//mPopupMenu = LLUICtrlFactory::getInstance()->createFromFile<LLContextMenu>("menu_avatar_other.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	//mPopupMenu->setItemVisible(std::string("Normal"), false);
	//mPopupMenu->setItemVisible(std::string("Always use impostor"), false);
	//mPopupMenu->setItemVisible(std::string("Never use impostor"), false);
	//mPopupMenu->setItemVisible(std::string("Impostor seperator"), false);

	//return TRUE;
	mList = getChild<FSScrollListCtrl>("bump_list");
	mList->setContextMenu(&gFSBumpListMenu);

	return LLFloater::postBuild();
	// </FS:Ansariel>
}

// virtual
void LLFloaterBump::onOpen(const LLSD& key)
{
// <FS:Ansariel> Instant bump list floater update
	//if (gMeanCollisionList.empty())
	//{
	//	mNames.clear();
	//	mList->deleteAllItems();
	updateList();
}

void LLFloaterBump::draw()
{
	if (mDirty)
	{
		updateList();
		mDirty = false;
	}

	LLFloater::draw();
}

void LLFloaterBump::updateList()
{
	if (!mList)
	{
		return;
	}
	mList->deleteAllItems();

	if (gMeanCollisionList.empty())
	{
// </FS:Ansariel>
		std::string none_detected = getString("none_detected");
		LLSD row;
		row["columns"][0]["value"] = none_detected;
		row["columns"][0]["font"] = "SansSerifBold";
		mList->addElement(row);
	}
	else
	{
// <FS:Ansariel> Instant bump list floater update
//		populateCollisionList();
//	}
//}

//void LLFloaterBump::populateCollisionList()
//{
//	mNames.clear();
//	mList->deleteAllItems();
// </FS:Ansariel>

	for (mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
				 iter != gMeanCollisionList.end(); ++iter)
	{
		LLMeanCollisionData *mcd = *iter;
		add(mList, mcd);
	}
	// <FS:Ansariel> Instant bump list floater update
	}
	// </FS:Ansariel>
}

void LLFloaterBump::add(LLScrollListCtrl* list, LLMeanCollisionData* mcd)
{
	if (mcd->mFullName.empty() || list->getItemCount() >= 20)
	{
		return;
	}

	std::string timeStr = getString ("timeStr");
	LLSD substitution;

	substitution["datetime"] = (S32) mcd->mTime;
	LLStringUtil::format (timeStr, substitution);

	std::string action;
	switch(mcd->mType)
	{
	case MEAN_BUMP:
		action = "bump";
		break;
	case MEAN_LLPUSHOBJECT:
		action = "llpushobject";
		break;
	case MEAN_SELECTED_OBJECT_COLLIDE:
		action = "selected_object_collide";
		break;
	case MEAN_SCRIPTED_OBJECT_COLLIDE:
		action = "scripted_object_collide";
		break;
	case MEAN_PHYSICAL_OBJECT_COLLIDE:
		action = "physical_object_collide";
		break;
	default:
		LL_INFOS() << "LLFloaterBump::add unknown mean collision type "
			<< mcd->mType << LL_ENDL;
		return;
	}

	// All above action strings are in XML file
	LLUIString text = getString(action);
	text.setArg("[TIME]", timeStr);
	text.setArg("[NAME]", mcd->mFullName);

	LLSD row;
	row["id"] = mcd->mPerp;
	row["columns"][0]["value"] = text;
	row["columns"][0]["font"] = "SansSerifBold";
	list->addElement(row);

// <FS:Ansariel> Improved bump list
}
#if 0
	mNames[mcd->mPerp] = mcd->mFullName;
}


void LLFloaterBump::onScrollListRightClicked(LLUICtrl* ctrl, S32 x, S32 y)
{
	if (!gMeanCollisionList.empty())
	{
		LLScrollListItem* item = mList->hitItem(x, y);
		if (item && mPopupMenu)
		{
			mItemUUID = item->getUUID();
			mPopupMenu->buildDrawLabels();
			mPopupMenu->updateParent(LLMenuGL::sMenuContainer);

			std::string mute_msg = (LLMuteList::getInstance()->isMuted(mItemUUID, mNames[mItemUUID])) ? "UnmuteAvatar" : "MuteAvatar";
			mPopupMenu->getChild<LLUICtrl>("Avatar Mute")->setValue(LLTrans::getString(mute_msg));
			mPopupMenu->setItemEnabled(std::string("Zoom In"), bool(gObjectList.findObject(mItemUUID)));

			((LLContextMenu*)mPopupMenu)->show(x, y);
			LLMenuGL::showPopup(ctrl, mPopupMenu, x, y);
		}
	}
}


void LLFloaterBump::startIM()
{
	LLAvatarActions::startIM(mItemUUID);
}

void LLFloaterBump::startCall()
{
	LLAvatarActions::startCall(mItemUUID);
}

void LLFloaterBump::reportAbuse()
{
	LLFloaterReporter::showFromAvatar(mItemUUID, "av_name");
}

void LLFloaterBump::showProfile()
{
	LLAvatarActions::showProfile(mItemUUID);
}

void LLFloaterBump::addFriend()
{
	LLAvatarActions::requestFriendshipDialog(mItemUUID);
}

bool LLFloaterBump::enableAddFriend()
{
	return !LLAvatarActions::isFriend(mItemUUID);
}

void LLFloaterBump::muteAvatar()
{
	LLMute mute(mItemUUID, mNames[mItemUUID], LLMute::AGENT);
	if (LLMuteList::getInstance()->isMuted(mute.mID))
	{
		LLMuteList::getInstance()->remove(mute);
	}
	else
	{
		LLMuteList::getInstance()->add(mute);
		LLPanelBlockedList::showPanelAndSelect(mute.mID);
	}
}

void LLFloaterBump::payAvatar()
{
	LLAvatarActions::pay(mItemUUID);
}

void LLFloaterBump::zoomInAvatar()
{
	handle_zoom_to_object(mItemUUID);
}

bool LLFloaterBump::enableMute()
{
	return LLAvatarActions::canBlock(mItemUUID);
}

void LLFloaterBump::inviteToGroup()
{
	LLAvatarActions::inviteToGroup(mItemUUID);
}

LLFloaterBump* LLFloaterBump::getInstance()
{
	return LLFloaterReg::getTypedInstance<LLFloaterBump>("bumps");
}
#endif
// </FS:Ansariel>

// <FS:Ansariel> FIRE-13888: Add copy function to bumps list
LLContextMenu* FSBumpListMenu::createMenu()
{
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

	registrar.add("BumpList.Action", boost::bind(&FSBumpListMenu::onContextMenuItemClick, this, _2));
	enable_registrar.add("BumpList.Enable", boost::bind(&FSBumpListMenu::onContextMenuItemEnable, this, _2));

	return createFromFile("menu_fs_bump_list.xml");
}

void FSBumpListMenu::onContextMenuItemClick(const LLSD& userdata)
{
	std::string item = userdata.asString();

	if (item == "show_profile")
	{
		LLAvatarActions::showProfile(mUUIDs.front());
	}
	else if (item == "sendim")
	{
		if (mUUIDs.size() == 1)
		{
			LLAvatarActions::startIM(mUUIDs.front());
		}
		else
		{
			LLAvatarActions::startConference(mUUIDs);
		}
	}
	else if (item == "zoom")
	{
		LLAvatarActions::zoomIn(mUUIDs.front());
	}
	else if (item == "teleportto")
	{
		LLAvatarActions::teleportTo(mUUIDs.front());
	}
	else if (item == "block")
	{
		LLAvatarActions::toggleBlock(mUUIDs.front());
	}
	else if (item == "report")
	{
		LLAvatarActions::report(mUUIDs.front());
	}
	else if (item == "copy")
	{
		LLFloaterBump* floater = LLFloaterReg::findTypedInstance<LLFloaterBump>("bumps");
		if (floater && !gMeanCollisionList.empty() && !mUUIDs.empty())
		{
			std::string bumps_text;
			FSScrollListCtrl* list = floater->getChild<FSScrollListCtrl>("bump_list");

			std::vector<LLScrollListItem*> selected = list->getAllSelected();
			for (std::vector<LLScrollListItem*>::iterator it = selected.begin(); it != selected.end(); ++it)
			{
				bumps_text += ( (bumps_text.empty() ? "" : "\n") + (*it)->getColumn(0)->getValue().asString() );
			}

			if (!bumps_text.empty())
			{
				LLClipboard::instance().copyToClipboard(utf8str_to_wstring(bumps_text), 0, bumps_text.size() );
			}
		}
	}
}

bool FSBumpListMenu::onContextMenuItemEnable(const LLSD& userdata)
{
	const LLFloaterBump* floater = LLFloaterReg::findTypedInstance<LLFloaterBump>("bumps");
	const std::string item = userdata.asString();

	if (!floater)
	{
		return false;
	}

	if (item == "can_show_profile")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() == 1);
	}
	else if (item == "can_sendim")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() > 0);
	}
	else if (item == "can_zoom")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() == 1 && LLAvatarActions::canZoomIn(mUUIDs.front()));
	}
	else if (item == "can_teleportto")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() == 1 && FSRadar::getInstance()->getEntry(mUUIDs.front()) != NULL);
	}
	else if (item == "can_block")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() == 1);
	}
	else if (item == "is_blocked")
	{
		if (!gMeanCollisionList.empty() && mUUIDs.size() == 1)
		{
			std::string name;
			gCacheName->getFullName(mUUIDs.front(), name);
			return LLMuteList::getInstance()->isMuted(mUUIDs.front(), name);
		}
		else
		{
			return false;
		}
	}
	else if (item == "can_report")
	{
		return (!gMeanCollisionList.empty() && mUUIDs.size() == 1);
	}
	else if (item == "can_copy")
	{
		return (!gMeanCollisionList.empty() && !mUUIDs.empty());
	}

	return false;
}

FSBumpListMenu gFSBumpListMenu;
// </FS:Ansariel>

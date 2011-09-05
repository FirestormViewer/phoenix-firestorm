/** 
* @file fscontactlist.cpp
* @brief Firestorm contacts list control
*
* $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
* Phoenix Firestorm Viewer Source Code
* Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
* The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
* $/LicenseInfo$
*/

#include "llviewerprecompiledheaders.h"

#include "fscontactlist.h"

// libs
#include "llavatarname.h"
#include "llnotificationsutil.h"
#include "lleventtimer.h"

// llui
#include "llaccordionctrl.h"
#include "llaccordionctrltab.h"
// #include "llfiltereditor.h"
// #include "llfloaterreg.h"
#include "llmenubutton.h"
// #include "llmenugl.h"
//#include "lltextutil.h"
#include "lltoggleablemenu.h"
#include "lluictrlfactory.h"

// newview
// #include "llavatarpropertiesprocessor.h"
#include "llagent.h"
#include "llavataractions.h"
#include "llavatarlist.h"
#include "llavatarlistitem.h"
#include "llcallingcard.h"			// for LLAvatarTracker
#include "llfriendcard.h"
#include "llinventoryobserver.h"
#include "llfloateravatarpicker.h"
#include "llfriendcard.h"
#include "llinventoryobserver.h"
#include "llpanelpeoplemenus.h"
#include "llstartup.h"
#include "llviewercontrol.h"		// for gSavedSettings
#include "llviewermenu.h"			// for gMenuHolder
#include "llvoiceclient.h"
// #include "llworld.h"
// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a)
#include "rlvhandler.h"
// [/RLVa:KB]
// #include <algorithm>
// #include <boost/algorithm/string.hpp>
// using namespace std;
// using namespace boost;

// common
//#include "lltrans.h"
//#include "llcommonutils.h"


#define FRIEND_LIST_UPDATE_TIMEOUT	0.5


static LLRegisterPanelClassWrapper<FSContactList> t_contact_list("contact_list");
static LLRegisterPanelClassWrapper<FSContactSet> t_contact_set("contact_set");

//=============================================================================

/** Compares avatar items by online status, then by name */
class LLAvatarItemStatusComparator : public LLAvatarItemNameComparator
{
public:
	LLAvatarItemStatusComparator() {};

protected:
	/**
	 * @return true if item1 < item2, false otherwise
	 */
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		LLAvatarTracker& at = LLAvatarTracker::instance();
		bool online1 = at.isBuddyOnline(item1->getAvatarId());
		bool online2 = at.isBuddyOnline(item2->getAvatarId());

		if (online1 == online2)
		{
			return LLAvatarItemNameComparator::doCompare(item1, item2);
		}
		
		return online1 > online2; 
	}
};

/** Compares avatar items by online status, then by name */
class LLAvatarItemUserNameComparator : public LLAvatarItemComparator
{
public:
	LLAvatarItemUserNameComparator() {};

protected:
	/**
	 * @return true if item1 < item2, false otherwise
	 */
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		std::string name1 = item1->getUserName();
		std::string name2 = item2->getUserName();

		LLStringUtil::toUpper(name1);
		LLStringUtil::toUpper(name2);

		return name1 < name2;
	}
};

/** Compares avatar items by online status, then by name */
class LLAvatarItemStatusUserNameComparator : public LLAvatarItemUserNameComparator
{
public:
	LLAvatarItemStatusUserNameComparator() {};

protected:
	/**
	 * @return true if item1 < item2, false otherwise
	 */
	virtual bool doCompare(const LLAvatarListItem* item1, const LLAvatarListItem* item2) const
	{
		LLAvatarTracker& at = LLAvatarTracker::instance();
		bool online1 = at.isBuddyOnline(item1->getAvatarId());
		bool online2 = at.isBuddyOnline(item2->getAvatarId());

		if (online1 == online2)
		{
			return LLAvatarItemUserNameComparator::doCompare(item1, item2);
		}
		
		return online1 > online2; 
	}
};

static const LLAvatarItemStatusComparator STATUS_COMPARATOR;
static const LLAvatarItemUserNameComparator USERNAME_COMPARATOR;
static const LLAvatarItemStatusUserNameComparator STATUSUSERNAME_COMPARATOR;

//=============================================================================

/**
 * Updates given list either on regular basis or on external events (up to implementation). 
 */
class FSContactList::Updater
{
public:
	typedef boost::function<void()> callback_t;
	Updater(callback_t cb)
	: mCallback(cb)
	{
	}

	virtual ~Updater()
	{
	}

	/**
	 * Activate/deactivate updater.
	 *
	 * This may start/stop regular updates.
	 */
	virtual void setActive(bool) {}

protected:
	void update()
	{
		mCallback();
	}

	callback_t		mCallback;
};

/**
 * Update buttons on changes in our friend relations (STORM-557).
 */
class LLButtonsUpdater : public FSContactList::Updater, public LLFriendObserver
{
public:
	LLButtonsUpdater(callback_t cb)
	:	FSContactList::Updater(cb)
	{
		LLAvatarTracker::instance().addObserver(this);
	}

	~LLButtonsUpdater()
	{
		LLAvatarTracker::instance().removeObserver(this);
	}

	/*virtual*/ void changed(U32 mask)
	{
		(void) mask;
		update();
	}
};

//=============================================================================

class LLAvatarListUpdater : public FSContactList::Updater, public LLEventTimer
{
public:
	LLAvatarListUpdater(callback_t cb, F32 period)
	:	LLEventTimer(period),
		FSContactList::Updater(cb)
	{
		mEventTimer.stop();
	}

	virtual BOOL tick() // from LLEventTimer
	{
		return FALSE;
	}
};

/**
 * Updates the friends list.
 * 
 * Updates the list on external events which trigger the changed() method. 
 */
class LLFriendListUpdater : public LLAvatarListUpdater, public LLFriendObserver
{
	LOG_CLASS(LLFriendListUpdater);
	class LLInventoryFriendCardObserver;

public: 
	friend class LLInventoryFriendCardObserver;
	LLFriendListUpdater(callback_t cb)
	:	LLAvatarListUpdater(cb, FRIEND_LIST_UPDATE_TIMEOUT)
	,	mIsActive(false)
	{
		LLAvatarTracker::instance().addObserver(this);

		// For notification when SIP online status changes.
		LLVoiceClient::getInstance()->addObserver(this);
		mInvObserver = new LLInventoryFriendCardObserver(this);
	}

	~LLFriendListUpdater()
	{
		// will be deleted by ~LLInventoryModel
		// delete mInvObserver;
		LLVoiceClient::getInstance()->removeObserver(this);
		LLAvatarTracker::instance().removeObserver(this);
	}

	/*virtual*/ void changed(U32 mask)
	{
		if (mIsActive)
		{
			// events can arrive quickly in bulk - we need not process EVERY one of them -
			// so we wait a short while to let others pile-in, and process them in aggregate.
			mEventTimer.start();
		}

		// save-up all the mask-bits which have come-in
		mMask |= mask;
	}


	/*virtual*/ BOOL tick()
	{
		if (!mIsActive) return FALSE;

		if (mMask & (LLFriendObserver::ADD | LLFriendObserver::REMOVE | LLFriendObserver::ONLINE))
		{
			update();
		}

		// Stop updates.
		mEventTimer.stop();
		mMask = 0;

		return FALSE;
	}

	// virtual
	void setActive(bool active)
	{
		mIsActive = active;
		if (active)
		{
			tick();
		}
	}

private:
	U32 mMask;
	LLInventoryFriendCardObserver* mInvObserver;
	bool mIsActive;

	/**
	 *	This class is intended for updating Friend List when Inventory Friend Card is added/removed.
	 * 
	 *	The main usage is when Inventory Friends/All content is added while synchronizing with 
	 *		friends list on startup is performed. In this case Friend Panel should be updated when 
	 *		missing Inventory Friend Card is created.
	 *	*NOTE: updating is fired when Inventory item is added into CallingCards/Friends subfolder.
	 *		Otherwise LLFriendObserver functionality is enough to keep Friends Panel synchronized.
	 */
	class LLInventoryFriendCardObserver : public LLInventoryObserver
	{
		LOG_CLASS(LLFriendListUpdater::LLInventoryFriendCardObserver);

		friend class LLFriendListUpdater;

	private:
		LLInventoryFriendCardObserver(LLFriendListUpdater* updater) : mUpdater(updater)
		{
			gInventory.addObserver(this);
		}
		~LLInventoryFriendCardObserver()
		{
			gInventory.removeObserver(this);
		}
		/*virtual*/ void changed(U32 mask)
		{
			lldebugs << "Inventory changed: " << mask << llendl;

			static bool synchronize_friends_folders = true;
			if (synchronize_friends_folders)
			{
				// Checks whether "Friends" and "Friends/All" folders exist in "Calling Cards" folder,
				// fetches their contents if needed and synchronizes it with buddies list.
				// If the folders are not found they are created.
				LLFriendCardsManager::instance().syncFriendCardsFolders();
				synchronize_friends_folders = false;
			}

			// *NOTE: deleting of InventoryItem is performed via moving to Trash. 
			// That means LLInventoryObserver::STRUCTURE is present in MASK instead of LLInventoryObserver::REMOVE
			//if ((CALLINGCARD_ADDED & mask) == CALLINGCARD_ADDED)
			if ((mask & LLInventoryObserver::CALLING_CARD ||
			mask & LLFolderType::FT_CALLINGCARD)
			&& (mask & LLInventoryObserver::ADD ||
			mask & LLInventoryObserver::REMOVE ||
			mask & LLInventoryObserver::STRUCTURE))
			{
				lldebugs << "Calling card added: count: " << gInventory.getChangedIDs().size() 
					<< ", first Inventory ID: "<< (*gInventory.getChangedIDs().begin())
					<< llendl;

				bool friendFound = false;
				std::set<LLUUID> changedIDs = gInventory.getChangedIDs();
				for (std::set<LLUUID>::const_iterator it = changedIDs.begin(); it != changedIDs.end(); ++it)
				{
					if (isDescendentOfInventoryFriends(*it))
					{
						friendFound = true;
						break;
					}
				}

				if (friendFound)
				{
					lldebugs << "friend found, panel should be updated" << llendl;
					mUpdater->changed(LLFriendObserver::ADD);
				}
			}
		}

		bool isDescendentOfInventoryFriends(const LLUUID& invItemID)
		{
			LLViewerInventoryItem * item = gInventory.getItem(invItemID);
			if (NULL == item)
				return false;

			return LLFriendCardsManager::instance().isItemInAnyFriendsList(item);
		}
		LLFriendListUpdater* mUpdater;

		static const U32 CALLINGCARD_ADDED = LLInventoryObserver::ADD | LLInventoryObserver::CALLING_CARD;
	};
};


//=============================================================================

const LLAccordionCtrlTab::Params& get_accordion_tab_params()
{
	static LLAccordionCtrlTab::Params tab_params;
	static bool initialized = false;
	if (!initialized)
	{
		initialized = true;

		LLXMLNodePtr xmlNode;
		if (LLUICtrlFactory::getLayeredXMLNode("contact_set_accordion_tab.xml", xmlNode))
		{
			LLXUIParser parser;
			parser.readXUI(xmlNode, tab_params, "contact_set_accordion_tab.xml");
		}
		else
		{
			llwarns << "Failed to read xml of Outfit's Accordion Tab from contact_set_accordion_tab.xml" << llendl;
		}
	}

	return tab_params;
}

//=============================================================================

FSContactList::FSContactList(const FSContactList::Params& p /*= LLPanel::Params()*/)
:	LLPanel(p)
{
	
	if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT)
	{
		mFriendListUpdater = new LLFriendListUpdater(boost::bind(&FSContactList::updateFriendList,	this));
	}
	mButtonsUpdater = new LLButtonsUpdater(boost::bind(&FSContactList::updateButtons, this));
}

FSContactList::~FSContactList()
{
	delete mButtonsUpdater;
	if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT)
	{
		delete mFriendListUpdater;
	}
	mContactSets.clear();
	
	if(LLVoiceClient::instanceExists())
	{
		LLVoiceClient::getInstance()->removeObserver(this);
	}
}

BOOL FSContactList::postBuild()
{
	mAccordion = getChild<LLAccordionCtrl>("accordion_list");
	
	if (LLStartUp::getStartupState() > STATE_LOGIN_WAIT)
	{
		setVisibleCallback(boost::bind(&Updater::setActive, mFriendListUpdater, _2));
	}
	
	//HACK* this should be checked against the value in llfriendcard
	mAllFriendList = addContactSet(LLUUID::null, "All");
	
	setSortOrder((ESortOrder)gSavedSettings.getU32("FSContactsSortOrder"), false);
	
	// addContactSet("Test");
	LLFriendCardsManager::instance().setFriendsDirLoadedCallback(boost::bind(&FSContactList::onFriendsLoaded, this));
	
	// Create menus.
	mViewOptionMenuButton = getChild<LLMenuButton>("sort_options_gear_btn");
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	
	registrar.add("ContactList.ViewOption.Action", boost::bind(&FSContactList::onViewOptionChecked, this, _2));
	enable_registrar.add("ContactList.ViewOption.Check", boost::bind(&FSContactList::isViewOptionChecked, this, _2));
	
	registrar.add("ContactList.CreateSet.Action", boost::bind(&FSContactList::onCreateContactSet, this));
	registrar.add("ContactList.CreateSet.Enabled", boost::bind(&FSContactList::isFriendsLoaded, this));

	
	mViewOptionMenu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_contact_list_sort_gear.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	mViewOptionMenuButton->setMenu(mViewOptionMenu, LLMenuButton::MP_BOTTOM_LEFT);
	
	
	buttonSetAction("add_btn",			boost::bind(&FSContactList::onAddFriendWizButtonClicked,	this));
	buttonSetAction("del_btn",			boost::bind(&FSContactList::onDeleteFriendButtonClicked,	this));
	buttonSetAction("GlobalOnlineStatusToggle", boost::bind(&FSContactList::onGlobalVisToggleButtonClicked,	this));
	buttonSetAction("view_profile_btn",	boost::bind(&FSContactList::onViewProfileButtonClicked,		this));
	buttonSetAction("im_btn",			boost::bind(&FSContactList::onImButtonClicked,				this));
	buttonSetAction("call_btn",			boost::bind(&FSContactList::onCallButtonClicked,			this));
	buttonSetAction("teleport_btn",		boost::bind(&FSContactList::onTeleportButtonClicked,		this));
	buttonSetAction("share_btn",		boost::bind(&FSContactList::onShareButtonClicked,			this));
	
	// call this method in case some list is empty and buttons can be in inconsistent state
	updateButtons();
	
	return TRUE;
}

void FSContactList::onFriendsLoaded()
{
	mIsFriendsLoaded = true;
}

void FSContactList::onCreateContactSet()
{
	LLNotificationsUtil::add("CreateContactSet", LLSD(), LLSD(), boost::bind(onContactSetCreate, _1, _2));
}

// static
void FSContactList::onContactSetCreate(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option != 0) return; // canceled

	std::string set_name = response["new_name"].asString();
	LLStringUtil::trim(set_name);
	if (!set_name.empty())
	{
		LLFriendCardsManager::instance().createFriendSubfolder(set_name);
	}
}

FSContactSet*	 FSContactList::addContactSet(const LLUUID& sub_folder_ID, const std::string& set_name)
{
	LLAccordionCtrlTab::Params tab_params(get_accordion_tab_params());
	LLAccordionCtrlTab* tab = LLUICtrlFactory::create<LLAccordionCtrlTab>(tab_params);
	if (!tab) return NULL;
	
	
	// FSContactSet* contact_list_set = LLUICtrlFactory::create<FSContactSet>(tab_params.contact_list_set);
	FSContactSet* contact_list_set = new FSContactSet();
	
	//HACK* this should be checked against the value in llfriendcard
	contact_list_set->setHandleFriendsAll(set_name == "All");
	
	contact_list_set->setTab(tab);
	contact_list_set->setName(set_name);
	
	tab->addChild(contact_list_set);
	
	mContactSets[sub_folder_ID] = contact_list_set;
	
	LLAvatarList* avatar_list = contact_list_set->getList();
	
	
	avatar_list->setItemDoubleClickCallback(boost::bind(&FSContactList::onAvatarListDoubleClicked, this, _1));
	avatar_list->setCommitCallback(boost::bind(&FSContactList::onAvatarListCommitted, this, avatar_list));
	avatar_list->setReturnCallback(boost::bind(&FSContactList::onImButtonClicked, this));
	// avatar_list->setRefreshCompleteCallback(boost::bind(&FSContactList::onFriendListRefreshComplete, this, _1, _2));
	
	
	// *TODO: LLUICtrlFactory::defaultBuilder does not use "display_children" from xml. Should be investigated.
	tab->setDisplayChildren(true);

	mAccordion->addCollapsibleCtrl(tab);
	
	// tab->setDropDownStateChangedCallback(
		// boost::bind(&FSContactList::onFriendsAccordionExpandedCollapsed, this, _1, _2, avatar_list));

	return contact_list_set;
}

// Unused for now
#if 0
void FSContactList::onFriendsAccordionExpandedCollapsed(LLUICtrl* ctrl, const LLSD& param, LLAvatarList* avatar_list)
{
	if(!avatar_list)
	{
		llerrs << "Bad parameter" << llendl;
		return;
	}

	bool expanded = param.asBoolean();

	setAccordionCollapsedByUser(ctrl, !expanded);
	if(!expanded)
	{
		avatar_list->resetSelection();
	}
}
#endif

FSContactSet* FSContactList::getConstactSet(const LLUUID& sub_folder_ID)
{
	return mContactSets[sub_folder_ID];
}

FSContactSet* FSContactList::getOrCreateConstactSet(const LLUUID& sub_folder_ID)
{
	LLViewerInventoryCategory* cat = gInventory.getCategory(sub_folder_ID);
	std::string cat_name = cat ? cat->getName() : "unknown";
	
	contact_set_map_t::iterator it = mContactSets.find(sub_folder_ID);
	if(it != mContactSets.end()) 
	{
		FSContactSet* contact_list_set = it->second;
		contact_list_set->setName(cat_name);
		
		return contact_list_set;
	}
	
	//is new set, make list control for it
	llinfos << "Making new contact set control for: " << cat_name << llendl;
	return addContactSet(sub_folder_ID, cat_name);
}

void FSContactList::updateFriendList()
{
	
	LLFriendCardsManager::folderid_buddies_map_t listMap;
	LLFriendCardsManager::instance().collectFriendsLists(listMap);
	
	if (listMap.size() > 0)
	{
		lldebugs << "Friends Cards were found, count: " << listMap.begin()->second.size() << llendl;
		
		LLFriendCardsManager::folderid_buddies_map_t::iterator dir_it;
		for (dir_it = listMap.begin(); dir_it != listMap.end(); ++dir_it)
		{
			FSContactSet* contact_list_set = getOrCreateConstactSet(dir_it->first);
			contact_list_set->update(dir_it->second);
		}
	}
	else
 	{
		llinfos << "Friends Cards were not found" << llendl;
 	}
	
	// Friends/All is processed from LLAvatarTracker instead for reliablity and speed reasons
	// Calling Cards takes time to fetch and it could take several minutes from login for to load
	mAllFriendList->update();
	
	updateButtons();
}

void FSContactList::buttonSetEnabled(const std::string& btn_name, bool enabled)
{
	// To make sure we're referencing the right widget (a child of the button bar).
	LLButton* button = getChild<LLView>("button_bar")->getChild<LLButton>(btn_name);
	button->setEnabled(enabled);
}

void FSContactList::buttonSetAction(const std::string& btn_name, const commit_signal_t::slot_type& cb)
{
	// To make sure we're referencing the right widget (a child of the button bar).
	LLButton* button = getChild<LLView>("button_bar")->getChild<LLButton>(btn_name);
	button->setClickedCallback(cb);
}

void FSContactList::updateButtons()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	
	bool item_selected = (selected_uuids.size() == 1);
	bool multiple_selected = (selected_uuids.size() >= 1);
	
	bool enable_calls = LLVoiceClient::getInstance()->isVoiceWorking() && LLVoiceClient::getInstance()->voiceEnabled();

	buttonSetEnabled("view_profile_btn",item_selected);
	buttonSetEnabled("share_btn",		item_selected);
	buttonSetEnabled("im_btn",			multiple_selected); // allow starting the friends conference for multiple selection
	buttonSetEnabled("call_btn",		multiple_selected && enable_calls);
	buttonSetEnabled("teleport_btn",	multiple_selected && LLAvatarActions::canOfferTeleport(selected_uuids));
}

LLUUID FSContactList::getCurrentItemID() const
{
	LLUUID cur_online_friend;
	
	//for(av_list_t::const_iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
	for (contact_set_map_t::const_iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
	{
		LLAvatarList* avatar_list = it->second->getList();
		if ((cur_online_friend = avatar_list->getSelectedUUID()).notNull())
			break;
	}
	
	return cur_online_friend;
}

void FSContactList::getCurrentItemIDs(uuid_vec_t& selected_uuids) const
{
		//for(av_list_t::const_iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
		for (contact_set_map_t::const_iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
		{
			LLAvatarList* avatar_list = it->second->getList();
			avatar_list->getSelectedUUIDs(selected_uuids);
		}
}

void FSContactList::onViewOptionChecked(LLSD::String param)
{
	if ("sortby_name" == param)
	{
		setSortOrder(E_SORT_BY_NAME);
	}
	else if ("sortby_username" == param)
	{
		setSortOrder(E_SORT_BY_USERNAME);
	}
	else if ("sortby_statusname" == param)
	{
		setSortOrder(E_SORT_BY_STATUS);
	}
	else if ("sortby_statususername" == param)
	{
		setSortOrder(E_SORT_BY_STATUSUSERNAME);
	}
	else if ("view_icons" == param)
	{
		//for(av_list_t::iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
		for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
		{
			LLAvatarList* avatar_list = it->second->getList();
			avatar_list->toggleIcons();
		}
	}
	else if ("view_permissions" == param)
	{
		bool show_permissions = !gSavedSettings.getBOOL("FriendsListShowPermissions");
		gSavedSettings.setBOOL("FriendsListShowPermissions", show_permissions);
		
		//for(av_list_t::iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
		for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
		{
			LLAvatarList* avatar_list = it->second->getList();
			avatar_list->showPermissions(show_permissions);
		}
	}
	
}

bool FSContactList::isViewOptionChecked(LLSD::String param)
{
	if ("sortby_name" == param)
	{
		return gSavedSettings.getU32("FSContactsSortOrder") == E_SORT_BY_NAME;
	}
	else if ("sortby_username" == param)
	{
		return gSavedSettings.getU32("FSContactsSortOrder") == E_SORT_BY_USERNAME;
	}
	else if ("sortby_statusname" == param)
	{
		return gSavedSettings.getU32("FSContactsSortOrder") == E_SORT_BY_STATUS;
	}
	else if ("sortby_statususername" == param)
	{
		return gSavedSettings.getU32("FSContactsSortOrder") == E_SORT_BY_STATUSUSERNAME;
	}

	return false;
}

void FSContactList::setSortOrder(ESortOrder order, bool save)
{
	//for(av_list_t::iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
	for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
	{
		FSContactSet* contact_set = it->second;
		contact_set->setSortOrder(order);
	}
	
	if (save)
	{
		gSavedSettings.setU32("FSContactsSortOrder", order);
	}
}

void FSContactList::showPermissions(bool visible)
{
	//for(av_list_t::iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
	for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
	{
		LLAvatarList* avatar_list = it->second->getList();
		avatar_list->showPermissions(visible);
	}
}

void FSContactList::forceUpdate()
{
	updateFriendList();
}

// Unused for now
#if 0
void FSContactList::onFilterEdit(const std::string& search_string)
{
	mFilterSubStringOrig = search_string;
	LLStringUtil::trimHead(mFilterSubStringOrig);
	// Searches are case-insensitive
	std::string search_upper = mFilterSubStringOrig;
	LLStringUtil::toUpper(search_upper);

	if (mFilterSubString == search_upper)
		return;

	mFilterSubString = search_upper;

	//store accordion tabs state before any manipulation with accordion tabs
	if(!mFilterSubString.empty())
	{
		notifyChildren(LLSD().with("action","store_state"));
	}


	// Apply new filter.
	// AO: currently broken
	// mNearbyList->setNameFilter(mFilterSubStringOrig);
	mOnlineFriendList->setNameFilter(mFilterSubStringOrig);
	mAllFriendList->setNameFilter(mFilterSubStringOrig);
	mRecentList->setNameFilter(mFilterSubStringOrig);
	mGroupList->setNameFilter(mFilterSubStringOrig);

	setAccordionCollapsedByUser("tab_online", false);
	setAccordionCollapsedByUser("tab_all", false);

	showFriendsAccordionsIfNeeded();

	//restore accordion tabs state _after_ all manipulations...
	if(mFilterSubString.empty())
	{
		notifyChildren(LLSD().with("action","restore_state"));
	}
}
#endif

void FSContactList::onAvatarListDoubleClicked(LLUICtrl* ctrl)
{
	LLAvatarListItem* item = dynamic_cast<LLAvatarListItem*>(ctrl);
	if(!item)
	{
		return;
	}

	LLUUID clicked_id = item->getAvatarId();
	LLAvatarActions::startIM(clicked_id);
}

void FSContactList::onAvatarListCommitted(LLAvatarList* list)
{
	//for(av_list_t::iterator it = mContactSets.begin(), end_it = mContactSets.end(); it != end_it; ++it)
	for (contact_set_map_t::iterator it = mContactSets.begin(); it != mContactSets.end(); ++it)
	{
		LLAvatarList* avatar_list = it->second->getList();
		if (avatar_list != list)
			avatar_list->resetSelection(true);
	}
		
	updateButtons();
}

void FSContactList::onViewProfileButtonClicked()
{
	LLUUID id = getCurrentItemID();
	LLAvatarActions::showProfile(id);
}

bool FSContactList::isItemsFreeOfFriends(const uuid_vec_t& uuids)
{
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	for ( uuid_vec_t::const_iterator
			  id = uuids.begin(),
			  id_end = uuids.end();
		  id != id_end; ++id )
	{
		if (av_tracker.isBuddy (*id))
		{
			return false;
		}
	}
	return true;
}

void FSContactList::onAddFriendWizButtonClicked()
{
	// Show add friend wizard.
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSContactList::onAvatarPicked, _1, _2), FALSE, TRUE);
	// Need to disable 'ok' button when friend occurs in selection
	if (picker)	picker->setOkBtnEnableCb(boost::bind(&FSContactList::isItemsFreeOfFriends, this, _1));
	LLFloater* root_floater = gFloaterView->getParentFloater(this);
	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}
}

// static
void FSContactList::onAvatarPicked(const uuid_vec_t& ids, const std::vector<LLAvatarName> names)
{
	if (!names.empty() && !ids.empty())
		LLAvatarActions::requestFriendshipDialog(ids[0], names[0].getCompleteName());
}

void FSContactList::onGlobalVisToggleButtonClicked()
// Iterate through friends lists, toggling status permission on or off 
{	
	bool vis = getChild<LLUICtrl>("GlobalOnlineStatusToggle")->getValue().asBoolean();
	gSavedSettings.setBOOL("GlobalOnlineStatusToggle", vis);
	
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	LLAvatarTracker::buddy_map_t all_buddies;
	av_tracker.copyBuddyList(all_buddies);
	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	for (; buddy_it != all_buddies.end(); ++buddy_it)
	{
		LLUUID buddy_id = buddy_it->first;
		const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(buddy_id);
		if (relation == NULL)
		{
			// Lets have a warning log message instead of having a crash. EXT-4947.
			llwarns << "Trying to modify rights for non-friend avatar. Skipped." << llendl;
			return;
		}
		
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (vis)
			new_rights = LLRelationship::GRANT_ONLINE_STATUS + (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
		else
			new_rights = (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);

		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(buddy_id,new_rights);
	}		
	
	showPermissions(true);
	
	LLSD args;
	args["MESSAGE"] = 
	llformat("Due to server load, mass toggling visibility can take a while to become effective. Please be patient." );
	LLNotificationsUtil::add("GenericAlert", args);
}

void FSContactList::onDeleteFriendButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		LLAvatarActions::removeFriendDialog( selected_uuids.front() );
	}
	else if (selected_uuids.size() > 1)
	{
		LLAvatarActions::removeFriendsDialog( selected_uuids );
	}
}

void FSContactList::onImButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	if ( selected_uuids.size() == 1 )
	{
		// if selected only one person then start up IM
		LLAvatarActions::startIM(selected_uuids.at(0));
	}
	else if ( selected_uuids.size() > 1 )
	{
		// for multiple selection start up friends conference
		LLAvatarActions::startConference(selected_uuids);
	}
}

void FSContactList::onCallButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);

	if (selected_uuids.size() == 1)
	{
		// initiate a P2P voice chat with the selected user
		LLAvatarActions::startCall(getCurrentItemID());
	}
	else if (selected_uuids.size() > 1)
	{
		// initiate an ad-hoc voice chat with multiple users
		LLAvatarActions::startAdhocCall(selected_uuids);
	}
}

void FSContactList::onTeleportButtonClicked()
{
	uuid_vec_t selected_uuids;
	getCurrentItemIDs(selected_uuids);
	LLAvatarActions::offerTeleport(selected_uuids);
}

void FSContactList::onShareButtonClicked()
{
	LLAvatarActions::share(getCurrentItemID());
}

// Unused for now
#if 0
void FSContactList::showAccordion(const std::string name, bool show)
{
	if(name.empty())
	{
		llwarns << "No name provided" << llendl;
		return;
	}

	LLAccordionCtrlTab* tab = getChild<LLAccordionCtrlTab>(name);
	tab->setVisible(show);
	if(show)
	{
		// don't expand accordion if it was collapsed by user
		if(!isAccordionCollapsedByUser(tab))
		{
			// expand accordion
			tab->changeOpenClose(false);
		}
	}
}

void FSContactList::showFriendsAccordionsIfNeeded()
{
	if(FRIENDS_TAB_NAME == getActiveTabName())
	{
		// Expand and show accordions if needed, else - hide them
		showAccordion("tab_online", mOnlineFriendList->filterHasMatches());
		showAccordion("tab_all", mAllFriendList->filterHasMatches());

		// Rearrange accordions
		LLAccordionCtrl* accordion = getChild<LLAccordionCtrl>("friends_accordion");
		accordion->arrange();

		// *TODO: new no_matched_tabs_text attribute was implemented in accordion (EXT-7368).
		// this code should be refactored to use it
		// keep help text in a synchronization with accordions visibility.
		updateFriendListHelpText();
	}
}

void FSContactList::onFriendListRefreshComplete(LLUICtrl*ctrl, const LLSD& param)
{
	if(ctrl == mOnlineFriendList)
	{
		showAccordion("tab_online", param.asInteger());
	}
	else if(ctrl == mAllFriendList)
	{
		showAccordion("tab_all", param.asInteger());
	}
}

void FSContactList::setAccordionCollapsedByUser(LLUICtrl* acc_tab, bool collapsed)
{
	if(!acc_tab)
	{
		llwarns << "Invalid parameter" << llendl;
		return;
	}

	LLSD param = acc_tab->getValue();
	param[COLLAPSED_BY_USER] = collapsed;
	acc_tab->setValue(param);
}

void FSContactList::setAccordionCollapsedByUser(const std::string& name, bool collapsed)
{
	setAccordionCollapsedByUser(getChild<LLUICtrl>(name), collapsed);
}

bool FSContactList::isAccordionCollapsedByUser(LLUICtrl* acc_tab)
{
	if(!acc_tab)
	{
		llwarns << "Invalid parameter" << llendl;
		return false;
	}

	LLSD param = acc_tab->getValue();
	if(!param.has(COLLAPSED_BY_USER))
	{
		return false;
	}
	return param[COLLAPSED_BY_USER].asBoolean();
}

bool FSContactList::isAccordionCollapsedByUser(const std::string& name)
{
	return isAccordionCollapsedByUser(getChild<LLUICtrl>(name));
}
#endif

// virtual
void FSContactList::onChange(EStatusType status, const std::string &channelURI, bool proximal)
{
	if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
	{
		return;
	}
	
	updateButtons();
}


FSContactSet::FSContactSet()
: LLPanel()
, mAvatarList(NULL)
, mViewOptionMenuButton(NULL)
, mViewOptionMenu(NULL)
, mInfoTextLabel(NULL)
, mShowOffline(true)
, mHandleFriendsAll(true)
{
	buildFromFile("panel_contact_list_set.xml");
}

BOOL FSContactSet::postBuild()
{
	mAvatarList = getChild<LLAvatarList>("avatar_list");
	mAvatarList->setNoItemsCommentText(getString("no_friends"));
	mAvatarList->setShowIcons("FriendsListShowIcons");
	mAvatarList->showPermissions(gSavedSettings.getBOOL("FriendsListShowPermissions"));
	mAvatarList->setContextMenu(&LLPanelPeopleMenus::gNearbyMenu);
	
	setSortOrder((ESortOrder)gSavedSettings.getU32("FSContactsSortOrder"));
	
	// initilize info text, for now show offline is default on start
	mInfoTextLabel = getChild<LLTextBox>("list_info_text");
	mInfoTextLabel->setValue(getString("showing_offline"));
	
	LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
	registrar.add("ContactSet.ViewOption.Action", boost::bind(&FSContactSet::onViewOptionChecked, this, _2));
	
	LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
	enable_registrar.add("ContactSet.ViewOption.Check", boost::bind(&FSContactSet::isViewOptionChecked, this, _2));
	enable_registrar.add("ContactSet.ViewOption.Enabled", boost::bind(&FSContactSet::isViewOptionEnabled, this, _2));
	
	mViewOptionMenuButton	= getChild<LLMenuButton>("options_gear_btn");
	mViewOptionMenu			= LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_contact_set_gear.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
	mViewOptionMenuButton->setMenu(mViewOptionMenu, LLMenuButton::MP_BOTTOM_LEFT);
	
	
	
	return TRUE;
}

FSContactSet::~FSContactSet()
{
}

void FSContactSet::setTab(LLAccordionCtrlTab* tab)
{
	mTab = tab;
	setShape(tab->getLocalRect());
}

void FSContactSet::setName(std::string name)
{
	mName = name;
	mTab->setName(name);
	mTab->setTitle(name);
}

void FSContactSet::update( const uuid_vec_t& buddies_uuids )
{
	if (mHandleFriendsAll)
		return; //shouldn't happen, but just incase
	
	
	lldebugs << "Friends added to the list: " << buddies_uuids.size() << llendl;
	
	uuid_vec_t& all_friendsp = mAvatarList->getIDs();
	all_friendsp.clear();
	
	mFriendsIDs = all_friendsp;
	
	if (mShowOffline)
	{
		all_friendsp = buddies_uuids;
	}
	else
	{
		const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
		for ( uuid_vec_t::const_iterator
			id = buddies_uuids.begin(),
			id_end = buddies_uuids.end();
			id != id_end; ++id )
		{
			if (av_tracker.isBuddyOnline(*id))
				all_friendsp.push_back(*id);
		}
	}
	
	mAvatarList->setDirty(true, !mAvatarList->filterHasMatches());// do force update if list do NOT have items
	mAvatarList->sort();
	
}

void FSContactSet::update()
{
	if (!mHandleFriendsAll)
		return; //shouldn't happen, but just incase
	
	// Friends/All is processed from LLAvatarTracker instead for reliablity and speed reasons
	// Calling Cards takes time to fetch and it could take several minutes from login for to load
	
	// get all buddies we know about
	const LLAvatarTracker& av_tracker = LLAvatarTracker::instance();
	LLAvatarTracker::buddy_map_t all_buddies;
	av_tracker.copyBuddyList(all_buddies);
	// save them to the avatar_list vector
	uuid_vec_t& all_friendsp = mAvatarList->getIDs();
	all_friendsp.clear();
	
	if (mShowOffline)
	{
		uuid_vec_t buddies_uuids;
		LLAvatarTracker::buddy_map_t::const_iterator buddies_iter;

		// Fill the avatar list with friends UUIDs
		for (buddies_iter = all_buddies.begin(); buddies_iter != all_buddies.end(); ++buddies_iter)
		{
			buddies_uuids.push_back(buddies_iter->first);
		}

		if (buddies_uuids.size() > 0)
		{
			lldebugs << "Friends added to the list: " << buddies_uuids.size() << llendl;
			all_friendsp = buddies_uuids;
		}
		else
		{
			llwarns << "Friends Cards failed to load" << llendl;
			lldebugs << "No friends found" << llendl;
		}
	}
	else
	{
		LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
		for (; buddy_it != all_buddies.end(); ++buddy_it)
		{
			LLUUID buddy_id = buddy_it->first;
			if (av_tracker.isBuddyOnline(buddy_id))
				all_friendsp.push_back(buddy_id);
		}
	}
	
	mAvatarList->setDirty(true, !mAvatarList->filterHasMatches());// do force update if list do NOT have items
	mAvatarList->sort();
}

void FSContactSet::forceUpdate()
{
	if (mHandleFriendsAll)
	{
		update();
	}
	else
	{
		update(mFriendsIDs);
	}
}


void FSContactSet::onViewOptionChecked(LLSD::String param)
{
	if ("show_offline" == param)
	{
		mShowOffline = !mShowOffline;
		if (mShowOffline)
			mInfoTextLabel->setValue(getString("showing_offline"));
		else
			mInfoTextLabel->setValue(getString("hiding_offline"));
		
		forceUpdate();
	}
	
}

bool FSContactSet::isViewOptionChecked(LLSD::String param)
{
	if ("show_offline" == param)
	{
		return mShowOffline;
	}

	return false;
}

bool FSContactSet::isViewOptionEnabled(LLSD::String param)
{
	//does nothing for now
	return true;
}

void FSContactSet::setSortOrder(ESortOrder order)
{
	switch (order)
	{
	case E_SORT_BY_NAME:
		mAvatarList->sortByName();
		break;
	case E_SORT_BY_USERNAME:
		mAvatarList->setComparator(&USERNAME_COMPARATOR);
		mAvatarList->sort();
		break;
	case E_SORT_BY_STATUS:
		mAvatarList->setComparator(&STATUS_COMPARATOR);
		mAvatarList->sort();
		break;
	case E_SORT_BY_STATUSUSERNAME:
		mAvatarList->setComparator(&STATUSUSERNAME_COMPARATOR);
		mAvatarList->sort();
		break;
	default:
		llwarns << "Unrecognized people sort order for " << mAvatarList->getName() << llendl;
		return;
	}
}

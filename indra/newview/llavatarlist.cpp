/** 
 * @file llavatarlist.h
 * @brief Generic avatar list
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

#include "llavatarlist.h"

// common
#include "lltrans.h"
#include "llcommonutils.h"

// llui
#include "lltextutil.h"

// newview
#include "llagentdata.h" // for comparator
#include "llavatariconctrl.h"
#include "llavatarnamecache.h"
#include "llcallingcard.h" // for LLAvatarTracker
#include "llcachename.h"
#include "lllistcontextmenu.h"
#include "llrecentpeople.h"
#include "lluuid.h"
#include "llvoiceclient.h"
#include "llviewercontrol.h"	// for gSavedSettings
// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a)
#include "rlvhandler.h"
// [/RLVa:KB]

static LLDefaultChildRegistry::Register<LLAvatarList> r("avatar_list");

// Last interaction time update period.
static const F32 LIT_UPDATE_PERIOD = 5;

// Maximum number of avatars that can be added to a list in one pass.
// Used to limit time spent for avatar list update per frame.
static const unsigned ADD_LIMIT = 50;

bool LLAvatarList::contains(const LLUUID& id)
{
	const uuid_vec_t& ids = getIDs();
	return std::find(ids.begin(), ids.end(), id) != ids.end();
}

LLAvatarListItem* LLAvatarList::getAvatarListItem(const LLUUID& id)
{
	return (LLAvatarListItem*)getItemByValue(id);
}

void LLAvatarList::toggleIcons()
{
	if (!mIgnoreGlobalIcons)
	{
		// Save the new value for new items to use.
		mShowIcons = !mShowIcons;
		gSavedSettings.setBOOL(mIconParamName, mShowIcons);
		
		// Show/hide avatar icons for all existing items.
		std::vector<LLPanel*> items;
		getItems(items);
		for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
		{
			static_cast<LLAvatarListItem*>(*it)->setAvatarIconVisible(mShowIcons);
		}
	}
}

void LLAvatarList::setSpeakingIndicatorsVisible(bool visible)
{
	// Save the new value for new items to use.
	mShowSpeakingIndicator = visible;
	
	// Show/hide icons for all existing items.
	std::vector<LLPanel*> items;
	getItems(items);
	for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
	{
		static_cast<LLAvatarListItem*>(*it)->showSpeakingIndicator(mShowSpeakingIndicator);
	}
}

void LLAvatarList::showPermissions(bool visible)
{
	// Save the value for new items to use.
	mShowPermissions = visible;

	// Enable or disable showing permissions icons for all existing items.
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->setShowPermissions(mShowPermissions);
	}
}

void LLAvatarList::showRange(bool visible)
{
	mShowRange = visible;
	// Enable or disable showing distance field for all detected avatars.
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showRange(mShowRange);
	}	
}

void LLAvatarList::showFirstSeen(bool visible)
{
	mShowFirstSeen = visible;
	// Enable or disable showing time since noticed, for all detected avatars.
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showFirstSeen(visible);
	}	
}

void LLAvatarList::showStatusFlags(bool visible)
{
	mShowStatusFlags = visible;
	// Enable or disable showing movement flags for all detected avatars.
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showStatusFlags(visible);
	}	
}


void LLAvatarList::showDisplayName(bool visible)
{
	mShowDisplayName = visible;
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showDisplayName(visible);
	}
	mNeedUpdateNames = true;
}

void LLAvatarList::showUsername(bool visible)
{
	mShowUsername = visible;
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showUsername(visible);
	}
	mNeedUpdateNames = true;
}

void LLAvatarList::showVoiceVolume(bool visible)
{
	mShowVoiceVolume=visible;
}

void LLAvatarList::showAvatarAge(bool visible)
{
	mShowAge = visible;
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showAvatarAge(visible);
	}
}

void LLAvatarList::showPaymentStatus(bool visible)
{
	mShowPaymentStatus = visible;
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		static_cast<LLAvatarListItem*>(*it)->showPaymentStatus(visible);
	}
	mNeedUpdateNames = true;
}


static bool findInsensitive(std::string haystack, const std::string& needle_upper)
{
    LLStringUtil::toUpper(haystack);
    return haystack.find(needle_upper) != std::string::npos;
}


//comparators
static const LLAvatarItemNameComparator NAME_COMPARATOR;
static const LLFlatListView::ItemReverseComparator REVERSE_NAME_COMPARATOR(NAME_COMPARATOR);

LLAvatarList::Params::Params()
: ignore_online_status("ignore_online_status", false)
, show_last_interaction_time("show_last_interaction_time", false)
, show_info_btn("show_info_btn", false)
, show_profile_btn("show_profile_btn", true)
, show_speaking_indicator("show_speaking_indicator", true)
, show_permissions_granted("show_permissions_granted", false)
, show_icons("show_icons",true)
, show_voice_volume("show_voice_volume", false)
{
}

LLAvatarList::LLAvatarList(const Params& p)
:	LLFlatListViewEx(p)
, mIgnoreOnlineStatus(p.ignore_online_status)
, mShowLastInteractionTime(p.show_last_interaction_time)
, mContextMenu(NULL)
, mDirty(true) // to force initial update
, mNeedUpdateNames(false)
, mLITUpdateTimer(NULL)
, mShowIcons(p.show_icons)
, mShowInfoBtn(p.show_info_btn)
, mShowProfileBtn(p.show_profile_btn)
, mShowSpeakingIndicator(p.show_speaking_indicator)
, mShowPermissions(p.show_permissions_granted)
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
, mRlvCheckShowNames(false)
// [/RLVa:KB]
, mShowVoiceVolume(p.show_voice_volume)
, mShowRange(false)
, mShowStatusFlags(false)
, mShowUsername(true)
, mShowDisplayName(true)
, mIgnoreGlobalIcons(false)
, mShowAge(false)
, mShowPaymentStatus(false)
, mItemHeight(0)
// [Ansariel: Colorful radar]
, mUseRangeColors(false)
// [Ansariel: Colorful radar]
{
	setCommitOnSelectionChange(true);

	// Set default sort order.
	setComparator(&NAME_COMPARATOR);

	if (mShowLastInteractionTime)
	{
		mLITUpdateTimer = new LLTimer();
		mLITUpdateTimer->setTimerExpirySec(0); // zero to force initial update
		mLITUpdateTimer->start();
	}
	
	LLAvatarNameCache::addUseDisplayNamesCallback(boost::bind(&LLAvatarList::handleDisplayNamesOptionChanged, this));
}


void LLAvatarList::handleDisplayNamesOptionChanged()
{
	mNeedUpdateNames = true;
}


LLAvatarList::~LLAvatarList()
{
	delete mLITUpdateTimer;
}

void LLAvatarList::setShowIcons(std::string param_name)
{
	if (!mIgnoreGlobalIcons)
	{
		mIconParamName= param_name;
		mShowIcons = gSavedSettings.getBOOL(mIconParamName);
	}
}

// AO: This can be used to disable icon display on a particular list, without affecting the global preference.
void LLAvatarList::showMiniProfileIcons(bool visible)
{
	mShowIcons = visible;
	mIgnoreGlobalIcons = true;
	// Show/hide icons for all existing items.
	
	std::vector<LLPanel*> items;
	getItems(items);
	for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
	{
		static_cast<LLAvatarListItem*>(*it)->setAvatarIconVisible(mShowIcons);
	}
}

// [Ansariel: Colorful radar]
void LLAvatarList::setUseRangeColors(bool UseRangeColors)
{
	mUseRangeColors = UseRangeColors;

	std::vector<LLPanel*> items;
	getItems(items);
	for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
	{
		static_cast<LLAvatarListItem*>(*it)->setUseRangeColors(mUseRangeColors);
	}
}
// [Ansariel: Colorful radar]

// virtual
void LLAvatarList::draw()
{
	// *NOTE dzaporozhan
	// Call refresh() after draw() to avoid flickering of avatar list items.

	// AO: skip llflatlistview's implementation to better manage mSelectedItemsBorder.
	LLScrollContainer::draw();

	if (mNeedUpdateNames)
	{
		updateAvatarNames();
	}

	if (mDirty)
		refresh();

	if (mShowLastInteractionTime && mLITUpdateTimer->hasExpired())
	{
		updateLastInteractionTimes();
		mLITUpdateTimer->setTimerExpirySec(LIT_UPDATE_PERIOD); // restart the timer
	}
}

//virtual
void LLAvatarList::clear()
{
	getIDs().clear();
	setDirty(true);
	LLFlatListViewEx::clear();
}

void LLAvatarList::setNameFilter(const std::string& filter)
{
	std::string filter_upper = filter;
	LLStringUtil::toUpper(filter_upper);
	if (mNameFilter != filter_upper)
	{
		mNameFilter = filter_upper;

		// update message for empty state here instead of refresh() to avoid blinking when switch
		// between tabs.
		updateNoItemsMessage(filter);
		setDirty();
	}
}

void LLAvatarList::setItemHeight(S32 height)
// AO: Adjust some parameters that need to be changed when we adjust item spacing form the .xml default
// If you change these, also change addNewItem()
{
	mItemHeight = height;
	std::vector<LLPanel*> items;
	getItems(items);
	for(std::vector<LLPanel*>::const_iterator it = items.begin(), end_it = items.end(); it != end_it; ++it)
	{
		LLAvatarListItem* avItem = static_cast<LLAvatarListItem*>(*it);
		if (mItemHeight != 0)
		{
			S32 width = avItem->getRect().getWidth();
			avItem->reshape(width,mItemHeight);
			LLIconCtrl* highlight = avItem->getChild<LLIconCtrl>("hovered_icon");
			LLIconCtrl* select = avItem->getChild<LLIconCtrl>("selected_icon");
			highlight->setOrigin(0,24-height); // temporary hack to be in the right ballpark.
			highlight->reshape(width,mItemHeight);
			select->setOrigin(0,24-height);
			select->reshape(width,mItemHeight);
		}
	}
	mNeedUpdateNames = true;
}

void LLAvatarList::onFocusReceived()
// AO: Override this from base class to bypass highlighting border. It has issues with resized item spacing.
{
	gEditMenuHandler = this;
}

void LLAvatarList::sortByName()
{
	setComparator(&NAME_COMPARATOR);
	sort();
}

void LLAvatarList::setDirty(bool val /*= true*/, bool force_refresh /*= false*/)
{
	mDirty = val;
	if(mDirty && force_refresh)
	{
		refresh();
	}
}

void LLAvatarList::addAvalineItem(const LLUUID& item_id, const LLUUID& session_id, const std::string& item_name)
{
	LL_DEBUGS("Avaline") << "Adding avaline item into the list: " << item_name << "|" << item_id << ", session: " << session_id << LL_ENDL;
	LLAvalineListItem* item = new LLAvalineListItem(/*hide_number=*/false);
	item->setAvatarId(item_id, session_id, true, false);
	item->setName(item_name);
	item->showLastInteractionTime(mShowLastInteractionTime);
	item->showSpeakingIndicator(mShowSpeakingIndicator);
	item->setOnline(false);

	addItem(item, item_id);
	mIDs.push_back(item_id);
	sort();
}

//////////////////////////////////////////////////////////////////////////
// PROTECTED SECTION
//////////////////////////////////////////////////////////////////////////
void LLAvatarList::refresh()
{
	bool have_names			= TRUE;
	bool add_limit_exceeded	= false;
	bool modified			= false;
	bool have_filter		= !mNameFilter.empty();

	// Save selection.	
	uuid_vec_t selected_ids;
	getSelectedUUIDs(selected_ids);
	LLUUID current_id = getSelectedUUID();

	// Determine what to add and what to remove.
	uuid_vec_t added, removed;
	LLAvatarList::computeDifference(getIDs(), added, removed);

	// Handle added items.
	unsigned nadded = 0;
	const std::string waiting_str = LLTrans::getString("AvatarNameWaiting");

	for (uuid_vec_t::const_iterator it=added.begin(); it != added.end(); it++)
	{
		const LLUUID& buddy_id = *it;
		LLAvatarName av_name;
		have_names &= LLAvatarNameCache::get(buddy_id, &av_name);

		if (!have_filter || findInsensitive(av_name.mDisplayName, mNameFilter))
		{
			if (nadded >= ADD_LIMIT)
			{
				add_limit_exceeded = true;
				break;
			}
			else
			{
				// *NOTE: If you change the UI to show a different string,
				// be sure to change the filter code below.
				addNewItem(buddy_id, 
					       //av_name.mDisplayName.empty() ? waiting_str : av_name.mDisplayName,
					       av_name.getCompleteName(),
					       LLAvatarTracker::instance().isBuddyOnline(buddy_id));
				modified = true;
				nadded++;
			}
		}
	}

	// Handle removed items.
	for (uuid_vec_t::const_iterator it=removed.begin(); it != removed.end(); it++)
	{
		removeItemByUUID(*it);
		modified = true;
	}

	// Handle filter.
	if (have_filter)
	{
		std::vector<LLSD> cur_values;
		getValues(cur_values);

		for (std::vector<LLSD>::const_iterator it=cur_values.begin(); it != cur_values.end(); it++)
		{
			const LLUUID& buddy_id = it->asUUID();
			LLAvatarName av_name;
			have_names &= LLAvatarNameCache::get(buddy_id, &av_name);
			if (!findInsensitive(av_name.getCompleteName(), mNameFilter))
			{
				removeItemByUUID(buddy_id);
				modified = true;
			}
		}
	}

	// Changed item in place, need to request sort and update columns
	// because we might have changed data in a column on which the user
	// has already sorted. JC
	sort();

	// re-select items
	//	selectMultiple(selected_ids); // TODO: implement in LLFlatListView if need
	selectItemByUUID(current_id);

	// If the name filter is specified and the names are incomplete,
	// we need to re-update when the names are complete so that
	// the filter can be applied correctly.
	//
	// Otherwise, if we have no filter then no need to update again
	// because the items will update their names.
	bool dirty = add_limit_exceeded || (have_filter && !have_names);
	setDirty(dirty);

	// Refreshed all items.
	if(!dirty)
	{
		// Highlight items matching the filter.
		std::vector<LLPanel*> items;
		getItems(items);
		for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
		{
			static_cast<LLAvatarListItem*>(*it)->setHighlight(mNameFilter);
		}

		// Send refresh_complete signal.
		mRefreshCompleteSignal(this, LLSD((S32)size(false)));
	}

	// Commit if we've added/removed items.
	if (modified)
		onCommit();
}

void LLAvatarList::updateAvatarNames()
{
	std::vector<LLPanel*> items;
	getItems(items);

	for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
	{
		LLAvatarListItem* item = static_cast<LLAvatarListItem*>(*it);
		item->updateAvatarName();
	}
	mNeedUpdateNames = false;
}


bool LLAvatarList::filterHasMatches()
{
	uuid_vec_t values = getIDs();

	for (uuid_vec_t::const_iterator it=values.begin(); it != values.end(); it++)
	{
		const LLUUID& buddy_id = *it;
		LLAvatarName av_name;
		bool have_name = LLAvatarNameCache::get(buddy_id, &av_name);

		// If name has not been loaded yet we consider it as a match.
		// When the name will be loaded the filter will be applied again(in refresh()).

		if (have_name && !findInsensitive(av_name.mDisplayName, mNameFilter))
		{
			continue;
		}

		return true;
	}
	return false;
}

boost::signals2::connection LLAvatarList::setRefreshCompleteCallback(const commit_signal_t::slot_type& cb)
{
	return mRefreshCompleteSignal.connect(cb);
}

boost::signals2::connection LLAvatarList::setItemDoubleClickCallback(const mouse_signal_t::slot_type& cb)
{
	return mItemDoubleClickSignal.connect(cb);
}

//virtual
S32 LLAvatarList::notifyParent(const LLSD& info)
{
	if (info.has("sort") && &NAME_COMPARATOR == mItemComparator)
	{
		sort();
		return 1;
	}
// [SL:KB] - Patch: UI-AvatarListDndShare | Checked: 2011-06-19 (Catznip-2.6.0c) | Added: Catznip-2.6.0c
	else if ( (info.has("select")) && (info["select"].isUUID()) )
	{
		const LLSD& sdValue = getSelectedValue();
		const LLUUID idItem = info["select"].asUUID();
		if ( (!sdValue.isDefined()) || ((sdValue.isUUID()) && (sdValue.asUUID() != idItem)) )
		{
			resetSelection();
			selectItemByUUID(info["select"].asUUID());
		}
	}
// [/SL:KB]
	return LLFlatListViewEx::notifyParent(info);
}

void LLAvatarList::addNewItem(const LLUUID& id, const std::string& name, BOOL is_online, EAddPosition pos)
{
	LLAvatarListItem* item = new LLAvatarListItem();
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	item->setRlvCheckShowNames(mRlvCheckShowNames);
// [/RLVa:KB]
	
	// AO: Adjust some parameters that need to be changed when we adjust item spacing form the .xml default
	// If you change these, also change setLineHeight()
	if (mItemHeight != 0)
	{
		S32 width = item->getRect().getWidth();
		item->reshape(width,mItemHeight);
		LLIconCtrl* highlight = item->getChild<LLIconCtrl>("hovered_icon");
		LLIconCtrl* select = item->getChild<LLIconCtrl>("selected_icon");
		highlight->setOrigin(0,24-mItemHeight); // temporary hack to be in the right ballpark.
		highlight->reshape(width,mItemHeight);
		select->setOrigin(0,24-mItemHeight);
		select->reshape(width,mItemHeight);
	}
	
	// This sets the name as a side effect
	item->setAvatarId(id, mSessionID, mIgnoreOnlineStatus);
	item->setOnline(mIgnoreOnlineStatus ? true : is_online);
	item->showLastInteractionTime(mShowLastInteractionTime);

	item->setAvatarIconVisible(mShowIcons);
	item->setShowInfoBtn(mShowInfoBtn);
	item->setShowVoiceVolume(mShowVoiceVolume);
	item->setShowProfileBtn(mShowProfileBtn);
	item->showSpeakingIndicator(mShowSpeakingIndicator);
	item->setShowPermissions(mShowPermissions);
	item->showUsername(mShowUsername);
	item->showDisplayName(mShowDisplayName);
	item->showRange(mShowRange);
	item->showFirstSeen(mShowFirstSeen);
	item->showStatusFlags(mShowStatusFlags);
	item->showPaymentStatus(mShowPaymentStatus);
	item->showAvatarAge(mShowAge);
	
	// [Ansariel: Colorful radar]
	item->setUseRangeColors(mUseRangeColors);
	LLUIColorTable* colorTable = &LLUIColorTable::instance();
	item->setShoutRangeColor(colorTable->getColor("AvatarListItemShoutRange", LLColor4::yellow));
	item->setBeyondShoutRangeColor(colorTable->getColor("AvatarListItemBeyondShoutRange", LLColor4::red));
	// [/Ansariel: Colorful radar]

	item->setDoubleClickCallback(boost::bind(&LLAvatarList::onItemDoubleClicked, this, _1, _2, _3, _4));

	addItem(item, id, pos);
}

// virtual
BOOL LLAvatarList::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	BOOL handled = LLUICtrl::handleRightMouseDown(x, y, mask);
//	if ( mContextMenu && !isAvalineItemSelected())
// [RLVa:KB] - Checked: 2010-06-04 (RLVa-1.2.2a) | Modified: RLVa-1.2.0d
	if ( (mContextMenu && !isAvalineItemSelected()) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) )
// [/RLVa:KB]
	{
		uuid_vec_t selected_uuids;
		getSelectedUUIDs(selected_uuids);
		mContextMenu->show(this, selected_uuids, x, y);
	}
	return handled;
}

bool LLAvatarList::isAvalineItemSelected()
{
	std::vector<LLPanel*> selected_items;
	getSelectedItems(selected_items);
	std::vector<LLPanel*>::iterator it = selected_items.begin();
	
	for(; it != selected_items.end(); ++it)
	{
		if (dynamic_cast<LLAvalineListItem*>(*it))
			return true;
	}

	return false;
}

void LLAvatarList::setVisible(BOOL visible)
{
	if ( visible == FALSE && mContextMenu )
	{
		mContextMenu->hide();
	}
	LLFlatListViewEx::setVisible(visible);
}

void LLAvatarList::computeDifference(
	const uuid_vec_t& vnew_unsorted,
	uuid_vec_t& vadded,
	uuid_vec_t& vremoved)
{
	uuid_vec_t vcur;

	// Convert LLSDs to LLUUIDs.
	{
		std::vector<LLSD> vcur_values;
		getValues(vcur_values);

		for (size_t i=0; i<vcur_values.size(); i++)
			vcur.push_back(vcur_values[i].asUUID());
	}

	LLCommonUtils::computeDifference(vnew_unsorted, vcur, vadded, vremoved);
}

// Refresh shown time of our last interaction with all listed avatars.
void LLAvatarList::updateLastInteractionTimes()
{
	S32 now = (S32) LLDate::now().secondsSinceEpoch();
	std::vector<LLPanel*> items;
	getItems(items);

	for( std::vector<LLPanel*>::const_iterator it = items.begin(); it != items.end(); it++)
	{
		// *TODO: error handling
		LLAvatarListItem* item = static_cast<LLAvatarListItem*>(*it);
		S32 secs_since = now - (S32) LLRecentPeople::instance().getDate(item->getAvatarId()).secondsSinceEpoch();
		if (secs_since >= 0)
			item->setLastInteractionTime(secs_since);
	}
}

void LLAvatarList::onItemDoubleClicked(LLUICtrl* ctrl, S32 x, S32 y, MASK mask)
{
//	mItemDoubleClickSignal(ctrl, x, y, mask);
// [RLVa:KB] - Checked: 2010-06-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	if ( (!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES)) )
		mItemDoubleClickSignal(ctrl, x, y, mask);
// [/RLVa:KB]
}

bool LLAvatarItemComparator::compare(const LLPanel* item1, const LLPanel* item2) const
{
	const LLAvatarListItem* avatar_item1 = dynamic_cast<const LLAvatarListItem*>(item1);
	const LLAvatarListItem* avatar_item2 = dynamic_cast<const LLAvatarListItem*>(item2);
	
	if (!avatar_item1 || !avatar_item2)
	{
		llerror("item1 and item2 cannot be null", 0);
		return true;
	}

	return doCompare(avatar_item1, avatar_item2);
}

bool LLAvatarItemNameComparator::doCompare(const LLAvatarListItem* avatar_item1, const LLAvatarListItem* avatar_item2) const
{
	std::string name1 = avatar_item1->getAvatarName();
	std::string name2 = avatar_item2->getAvatarName();

	LLStringUtil::toUpper(name1);
	LLStringUtil::toUpper(name2);

	return name1 < name2;
}
bool LLAvatarItemAgentOnTopComparator::doCompare(const LLAvatarListItem* avatar_item1, const LLAvatarListItem* avatar_item2) const
{
	//keep agent on top, if first is agent, 
	//then we need to return true to elevate this id, otherwise false.
	if(avatar_item1->getAvatarId() == gAgentID)
	{
		return true;
	}
	else if (avatar_item2->getAvatarId() == gAgentID)
	{
		return false;
	}
	return LLAvatarItemNameComparator::doCompare(avatar_item1,avatar_item2);
}

/************************************************************************/
/*             class LLAvalineListItem                                  */
/************************************************************************/
LLAvalineListItem::LLAvalineListItem(bool hide_number/* = true*/) : LLAvatarListItem(false)
, mIsHideNumber(hide_number)
{
	// should not use buildPanel from the base class to ensure LLAvalineListItem::postBuild is called.
	buildFromFile( "panel_avatar_list_item.xml");
}

BOOL LLAvalineListItem::postBuild()
{
	BOOL rv = LLAvatarListItem::postBuild();

	if (rv)
	{
		setOnline(true);
		showLastInteractionTime(false);
		setShowProfileBtn(false);
		
		mAvatarIcon->setValue("Avaline_Icon");
		mAvatarIcon->setToolTip(std::string(""));
	}
	return rv;
}

// to work correctly this method should be called AFTER setAvatarId for avaline callers with hidden phone number
void LLAvalineListItem::setName(const std::string& name)
{
	if (mIsHideNumber)
	{
		static U32 order = 0;
		typedef std::map<LLUUID, U32> avaline_callers_nums_t;
		static avaline_callers_nums_t mAvalineCallersNums;

		llassert(getAvatarId() != LLUUID::null);

		const LLUUID &uuid = getAvatarId();

		if (mAvalineCallersNums.find(uuid) == mAvalineCallersNums.end())
		{
			mAvalineCallersNums[uuid] = ++order;
			LL_DEBUGS("Avaline") << "Set name for new avaline caller: " << uuid << ", order: " << order << LL_ENDL;
		}
		LLStringUtil::format_map_t args;
		args["[ORDER]"] = llformat("%u", mAvalineCallersNums[uuid]);
		std::string hidden_name = LLTrans::getString("AvalineCaller", args);

		LL_DEBUGS("Avaline") << "Avaline caller: " << uuid << ", name: " << hidden_name << LL_ENDL;
		LLAvatarListItem::setAvatarName(hidden_name);
		LLAvatarListItem::setAvatarToolTip(hidden_name);
	}
	else
	{
		const std::string& formatted_phone = LLTextUtil::formatPhoneNumber(name);
		LLAvatarListItem::setAvatarName(formatted_phone);
		LLAvatarListItem::setAvatarToolTip(formatted_phone);
	}
}

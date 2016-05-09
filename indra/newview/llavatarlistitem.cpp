/** 
 * @file llavatarlistitem.cpp
 * @brief avatar list item source file
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

#include <boost/signals2.hpp>

#include "llavataractions.h"
#include "llavatarlistitem.h"

#include "llbutton.h"
#include "llfloaterreg.h"
#include "lltextutil.h"

#include "llagent.h"
#include "llslurl.h" // AO: Used for confirm modify rights
#include "llavatarnamecache.h"
#include "llavatariconctrl.h"
#include "lloutputmonitorctrl.h"
#include "lltooldraganddrop.h"
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a)
#include "rlvhandler.h"
// [/RLVa:KB]
#include  <time.h>
#include "llnotificationsutil.h"
#include "llvoiceclient.h"

#include "lfsimfeaturehandler.h"	// <FS:CR> Opensim
#include "llavatarlist.h"

bool LLAvatarListItem::sStaticInitialized = false;
S32 LLAvatarListItem::sLeftPadding = 0;
S32 LLAvatarListItem::sNameRightPadding = 0;
S32 LLAvatarListItem::sChildrenWidths[LLAvatarListItem::ALIC_COUNT];

static LLWidgetNameRegistry::StaticRegistrar sRegisterAvatarListItemParams(&typeid(LLAvatarListItem::Params), "avatar_list_item");

LLAvatarListItem::Params::Params()
:	default_style("default_style"),
	voice_call_invited_style("voice_call_invited_style"),
	voice_call_joined_style("voice_call_joined_style"),
	voice_call_left_style("voice_call_left_style"),
	online_style("online_style"),
	offline_style("offline_style"),
	group_moderator_style("group_moderator_style"),
	name_right_pad("name_right_pad",0)
{};


LLAvatarListItem::LLAvatarListItem(bool not_from_ui_factory/* = true*/)
	: LLPanel(),
	LLFriendObserver(),
	mAvatarIcon(NULL),
	mAvatarName(NULL),
	mLastInteractionTime(NULL),
	mBtnPermissionOnline(NULL),
	mBtnPermissionMap(NULL),
	mBtnPermissionEditMine(NULL),
	mIconPermissionEditTheirs(NULL),
	mSpeakingIndicator(NULL),
	mInfoBtn(NULL),
	mProfileBtn(NULL),
	mOnlineStatus(E_UNKNOWN),
	mShowInfoBtn(true),
	mShowProfileBtn(true),
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	mRlvCheckShowNames(false),
// [/RLVa:KB]
	mShowPermissions(false),
	mShowCompleteName(false),
	mHovered(false),
	mAvatarNameCacheConnection(),
	mGreyOutUsername(""),
	mShowVoiceVolume(false),
	mShowDisplayName(true),
	mShowUsername(true)
{
	if (not_from_ui_factory)
	{
		buildFromFile("panel_avatar_list_item.xml");
	}
	// *NOTE: mantipov: do not use any member here. They can be uninitialized here in case instance
	// is created from the UICtrlFactory

	// <FS:Ansariel> Add callback for user volume change
	mVoiceLevelChangeCallbackConnection = LLVoiceClient::getInstance()->setUserVolumeUpdateCallback(boost::bind(&LLAvatarListItem::onUserVoiceLevelChange, this, _1));
	// </FS:Ansariel>
}

LLAvatarListItem::~LLAvatarListItem()
{
	if (mAvatarId.notNull())
	{
		LLAvatarTracker::instance().removeParticularFriendObserver(mAvatarId, this);
		// <FS> Remove our own observers
		LLAvatarTracker::instance().removeFriendPermissionObserver(mAvatarId, this);
	}

	if (mAvatarNameCacheConnection.connected())
	{
		mAvatarNameCacheConnection.disconnect();
	}

	// <FS:Ansariel> Add callback for user volume change
	if (mVoiceLevelChangeCallbackConnection.connected())
	{
		mVoiceLevelChangeCallbackConnection.disconnect();
	}
	// </FS:Ansariel>
}

BOOL  LLAvatarListItem::postBuild()
{
	mAvatarIcon = getChild<LLAvatarIconCtrl>("avatar_icon");
	mAvatarName = getChild<LLTextBox>("avatar_name");
	mLastInteractionTime = getChild<LLTextBox>("last_interaction");

	// permissions
	mBtnPermissionOnline = getChild<LLButton>("permission_online_btn");
	mBtnPermissionMap = getChild<LLButton>("permission_map_btn");
	mBtnPermissionEditMine = getChild<LLButton>("permission_edit_mine_btn");
	mIconPermissionEditTheirs = getChild<LLIconCtrl>("permission_edit_theirs_icon");
	
	mBtnPermissionOnline->setClickedCallback(boost::bind(&LLAvatarListItem::onPermissionOnlineClick, this));
	mBtnPermissionMap->setClickedCallback(boost::bind(&LLAvatarListItem::onPermissionMapClick, this));
	mBtnPermissionEditMine->setClickedCallback(boost::bind(&LLAvatarListItem::onPermissionEditMineClick, this));
	
	mBtnPermissionOnline->setVisible(false);
	mBtnPermissionOnline->setIsChrome(TRUE);
	mBtnPermissionMap->setVisible(false);
	mBtnPermissionMap->setIsChrome(TRUE);
	mBtnPermissionEditMine->setVisible(false);
	mBtnPermissionEditMine->setIsChrome(TRUE);
	mIconPermissionEditTheirs->setVisible(false);

	mSpeakingIndicator = getChild<LLOutputMonitorCtrl>("speaking_indicator");
	mInfoBtn = getChild<LLButton>("info_btn");
	mProfileBtn = getChild<LLButton>("profile_btn");

	mInfoBtn->setVisible(false);
	mInfoBtn->setClickedCallback(boost::bind(&LLAvatarListItem::onInfoBtnClick, this));

	mVoiceSlider = getChild<LLUICtrl>("volume_slider");
	mVoiceSlider->setVisible(false);
	mVoiceSlider->setCommitCallback(boost::bind(&LLAvatarListItem::onVolumeChange, this, _2));

	mProfileBtn->setVisible(false);
	mProfileBtn->setClickedCallback(boost::bind(&LLAvatarListItem::onProfileBtnClick, this));

	if (!sStaticInitialized)
	{
		// Remember children widths including their padding from the next sibling,
		// so that we can hide and show them again later.
		initChildrenWidths(this);

		// Right padding between avatar name text box and nearest visible child.
		sNameRightPadding = LLUICtrlFactory::getDefaultParams<LLAvatarListItem>().name_right_pad;

		sStaticInitialized = true;
	}

	return TRUE;
}

void LLAvatarListItem::onVolumeChange(const LLSD& data)
{
	F32 volume = (F32)data.asReal();
	LLVoiceClient::getInstance()->setUserVolume(mAvatarId, volume);
}

// <FS:Ansariel> LL refactoring error
//void LLAvatarListItem::handleVisibilityChange ( BOOL new_visibility )
void LLAvatarListItem::onVisibilityChange ( BOOL new_visibility )
// </FS:Ansariel>
{
    //Adjust positions of icons (info button etc) when 
    //speaking indicator visibility was changed/toggled while panel was closed (not visible)
    if(new_visibility && mSpeakingIndicator->getIndicatorToggled())
    {
        updateChildren();
        mSpeakingIndicator->setIndicatorToggled(false);
    }
}

void LLAvatarListItem::fetchAvatarName()
{
	if (mAvatarId.notNull())
	{
		if (mAvatarNameCacheConnection.connected())
		{
			mAvatarNameCacheConnection.disconnect();
		}
		mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLAvatarListItem::onAvatarNameCache, this, _2));
	}
}

S32 LLAvatarListItem::notifyParent(const LLSD& info)
{
	if (info.has("visibility_changed"))
	{
		updateChildren();
		return 1;
	}
	return LLPanel::notifyParent(info);
}

void LLAvatarListItem::onMouseEnter(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible( true);
	// <FS:AO>, removed on-hover visibility. Don't do this. instead flip info buttons on full-time in postbuild.
//	mInfoBtn->setVisible(mShowInfoBtn);
//	mProfileBtn->setVisible(mShowProfileBtn);
// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
	//mInfoBtn->setVisible( (mShowInfoBtn) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) );
	//mProfileBtn->setVisible( (mShowProfileBtn) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) );
// [/RLVa:KB]

	mHovered = true;
	LLPanel::onMouseEnter(x, y, mask);

	//<FS:AO> don't update these on-hover, because we want to give users instant feedback when they change a permission state, even if the
	//process takes n-seconds to complete. Hover-reprocessing can confuse the user if it takes place before the async permissions change
	//goes through, appearing to mysteriously erase the user's choice.
	//showPermissions(mShowPermissions);
	//updateChildren();
}

void LLAvatarListItem::onMouseLeave(S32 x, S32 y, MASK mask)
{
	getChildView("hovered_icon")->setVisible( false);
	// <FS:Wolf> commented out to have the info button always shown
	//mInfoBtn->setVisible(false);
	//mProfileBtn->setVisible(false);
	// </FS:Wolf>

	mHovered = false;
	LLPanel::onMouseLeave(x, y, mask);
	// <FS:Wolf> commented out to have the info button always shown
	//showPermissions(false);
	//updateChildren();
}

// virtual, called by LLAvatarTracker
void LLAvatarListItem::changed(U32 mask)
{
	// no need to check mAvatarId for null in this case
	setOnline(LLAvatarTracker::instance().isBuddyOnline(mAvatarId));

	if ((mask & LLFriendObserver::POWERS) || (mask & LLFriendObserver::PERMS)) 
	{
		showPermissions(mShowPermissions && gSavedSettings.getBOOL("FriendsListShowPermissions"));
		updateChildren();
	}
}

void LLAvatarListItem::setOnline(bool online)
{
	// *FIX: setName() overrides font style set by setOnline(). Not an issue ATM.

	if (mOnlineStatus != E_UNKNOWN && (bool) mOnlineStatus == online)
		return;

	mOnlineStatus = (EOnlineStatus) online;

	// Change avatar name font style depending on the new online status.
	setState(online ? IS_ONLINE : IS_OFFLINE);
}

void LLAvatarListItem::setAvatarName(const std::string& name)
{
	setNameInternal(name, mHighlihtSubstring);
}

void LLAvatarListItem::setAvatarToolTip(const std::string& tooltip)
{
	mAvatarName->setToolTip(tooltip);
}

void LLAvatarListItem::setHighlight(const std::string& highlight)
{
	setNameInternal(mAvatarName->getText(), mHighlihtSubstring = highlight);
}

void LLAvatarListItem::setState(EItemState item_style)
{
	const LLAvatarListItem::Params& params = LLUICtrlFactory::getDefaultParams<LLAvatarListItem>();

	switch(item_style)
	{
	default:
	case IS_DEFAULT:
		mAvatarNameStyle = params.default_style();
		break;
	case IS_VOICE_INVITED:
		mAvatarNameStyle = params.voice_call_invited_style();
		break;
	case IS_VOICE_JOINED:
		mAvatarNameStyle = params.voice_call_joined_style();
		break;
	case IS_VOICE_LEFT:
		mAvatarNameStyle = params.voice_call_left_style();
		break;
	case IS_ONLINE:
		mAvatarNameStyle = params.online_style();
		break;
	case IS_OFFLINE:
		mAvatarNameStyle = params.offline_style();
		break;
	case IS_GROUPMOD:
		mAvatarNameStyle = params.group_moderator_style();
		break;
	}

	// *NOTE: You cannot set the style on a text box anymore, you must
	// rebuild the text.  This will cause problems if the text contains
	// hyperlinks, as their styles will be wrong.
	setNameInternal(mAvatarName->getText(), mHighlihtSubstring);

	icon_color_map_t& item_icon_color_map = getItemIconColorMap();
	mAvatarIcon->setColor(item_icon_color_map[item_style]);
}

void LLAvatarListItem::setAvatarId(const LLUUID& id, const LLUUID& session_id, bool ignore_status_changes/* = false*/, bool is_resident/* = true*/)
{
	if (mAvatarId.notNull())
	{
		LLAvatarTracker::instance().removeParticularFriendObserver(mAvatarId, this);
		LLAvatarTracker::instance().removeFriendPermissionObserver(mAvatarId, this);
	}

	mAvatarId = id;
	mSpeakingIndicator->setSpeakerId(id, session_id);

	// <FS:Ansariel> FIRE-10278: Show correct voice volume if we have it stored
	updateVoiceLevelSlider();

	// We'll be notified on avatar online status changes
	if (!ignore_status_changes && mAvatarId.notNull())
		LLAvatarTracker::instance().addParticularFriendObserver(mAvatarId, this);

	if (mAvatarId.notNull())
		LLAvatarTracker::instance().addFriendPermissionObserver(mAvatarId, this);
	
	if (is_resident)
	{
		mAvatarIcon->setValue(id);

		// Set avatar name.
		fetchAvatarName();
	}
	
	// <FS:AO> Always show permissions icons, like in V1.
	// we put this here so because it's the nearest update point where we have good av data.
	showPermissions(mShowPermissions && gSavedSettings.getBOOL("FriendsListShowPermissions"));
	updateChildren();
}

// <FS:Ansariel> Add callback for user volume change
void LLAvatarListItem::onUserVoiceLevelChange(const LLUUID& avatar_id)
{
	if (avatar_id == mAvatarId)
	{
		updateVoiceLevelSlider();
	}
}

void LLAvatarListItem::updateVoiceLevelSlider()
{
	if (mVoiceSlider->getVisible() && LLVoiceClient::getInstance()->getVoiceEnabled(mAvatarId))
	{
		bool is_muted = LLAvatarActions::isVoiceMuted(mAvatarId);

		F32 volume;
		if (is_muted)
		{
			// it's clearer to display their volume as zero
			volume = 0.f;
		}
		else
		{
			// actual volume
			volume = LLVoiceClient::getInstance()->getUserVolume(mAvatarId);
		}
		mVoiceSlider->setValue((F64)volume);
	}
}
// </FS:Ansariel>

void LLAvatarListItem::showLastInteractionTime(bool show)
{
	mLastInteractionTime->setVisible(show);
	updateChildren();
}

void LLAvatarListItem::setLastInteractionTime(U32 secs_since)
{
	mLastInteractionTime->setValue(formatSeconds(secs_since));
}

void LLAvatarListItem::setShowInfoBtn(bool show)
{
	mShowInfoBtn = show;
	mInfoBtn->setVisible( (mShowInfoBtn) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) );
}

void LLAvatarListItem::setShowVoiceVolume(bool show)
{
	mShowVoiceVolume = show;
	mVoiceSlider->setVisible( (mShowVoiceVolume) && ((!mRlvCheckShowNames) || (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))) );
	if (show)
	{
		updateVoiceLevelSlider();
	}
}

// [RLVa:KB] - Checked: 2010-04-05 (RLVa-1.2.2a) | Added: RLVa-1.2.0d
void LLAvatarListItem::setRlvCheckShowNames(bool fRlvCheckShowNames)
{
	mRlvCheckShowNames = fRlvCheckShowNames;
	updateRlvRestrictions();
}

void LLAvatarListItem::updateRlvRestrictions()
{
	setShowVoiceVolume(mShowVoiceVolume);
	setShowInfoBtn(mShowInfoBtn);
}
// [/RLVa:KB]

void LLAvatarListItem::setShowProfileBtn(bool show)
{
	mShowProfileBtn = show;
}

void LLAvatarListItem::showSpeakingIndicator(bool visible)
{
	// Already done? Then do nothing.
	if (mSpeakingIndicator->getVisible() == (BOOL)visible)
		return;
// Disabled to not contradict with SpeakingIndicatorManager functionality. EXT-3976
// probably this method should be totally removed.
//	mSpeakingIndicator->setVisible(visible);
//	updateChildren();
}

void LLAvatarListItem::setAvatarIconVisible(bool visible)
{
	// Already done? Then do nothing.
	if (mAvatarIcon->getVisible() == (BOOL)visible)
	{
		return;
	}

	// Show/hide avatar icon.
	mAvatarIcon->setVisible(visible);
	updateChildren();
}

void LLAvatarListItem::showDisplayName(bool show, bool updateName /* = true*/)
{
	mShowDisplayName = show;
	if (updateName)
	{
		updateAvatarName();
	}
}

void LLAvatarListItem::showUsername(bool show, bool updateName /* = true*/)
{
	mShowUsername = show;
	if (updateName)
	{
		updateAvatarName();
	}
}

void LLAvatarListItem::onInfoBtnClick()
{
	LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", mAvatarId));
}

void LLAvatarListItem::onProfileBtnClick()
{
	LLAvatarActions::showProfile(mAvatarId);
}

BOOL LLAvatarListItem::handleDoubleClick(S32 x, S32 y, MASK mask)
{
//	if(mInfoBtn->getRect().pointInRect(x, y))
// [SL:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
	if ( (mInfoBtn->getVisible()) && (mInfoBtn->getEnabled()) && (mInfoBtn->getRect().pointInRect(x, y)) )
// [/SL:KB]
	{
		onInfoBtnClick();
		return TRUE;
	}
//	if(mProfileBtn->getRect().pointInRect(x, y))
// [SL:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
	if ( (mProfileBtn->getVisible()) && (mProfileBtn->getEnabled()) && (mProfileBtn->getRect().pointInRect(x, y)) )
// [/SL:KB]
	{
		onProfileBtnClick();
		return TRUE;
	}
	return LLPanel::handleDoubleClick(x, y, mask);
}

// [SL:KB] - Patch: UI-AvatarListDndShare | Checked: 2011-06-19 (Catznip-2.6.0c) | Added: Catznip-2.6.0c
BOOL LLAvatarListItem::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop, EDragAndDropType cargo_type, void *cargo_data, 
	                                     EAcceptance *accept, std::string& tooltip_msg)
{
	notifyParent(LLSD().with("select", mAvatarId));
	return LLToolDragAndDrop::handleGiveDragAndDrop(mAvatarId, LLUUID::null, drop, cargo_type, cargo_data, accept, getAvatarName());
}
// [/SL:KB]

void LLAvatarListItem::setValue( const LLSD& value )
{
	if (!value.isMap()) return;;
	if (!value.has("selected")) return;
	getChildView("selected_icon")->setVisible( value["selected"]);
}

const LLUUID& LLAvatarListItem::getAvatarId() const
{
	return mAvatarId;
}

std::string LLAvatarListItem::getAvatarName() const
{
	return mAvatarName->getValue();
}

std::string LLAvatarListItem::getAvatarToolTip() const
{
	return mAvatarName->getToolTip();
}

bool LLAvatarListItem::getShowingBothNames() const
{
	return (mShowDisplayName && mShowUsername);
}

void LLAvatarListItem::updateAvatarName()
{
	fetchAvatarName();
}

//== PRIVATE SECITON ==========================================================

void LLAvatarListItem::setNameInternal(const std::string& name, const std::string& highlight)
{
	// <FS:Ansariel> Commented out because horrible LL implementation - we control it via global display name settings!
    //if(mShowCompleteName && highlight.empty())
    //{
    //    LLTextUtil::textboxSetGreyedVal(mAvatarName, mAvatarNameStyle, name, mGreyOutUsername);
    //}
    //else
    //{
    //    LLTextUtil::textboxSetHighlightedVal(mAvatarName, mAvatarNameStyle, name, highlight);
    //}
	LLTextUtil::textboxSetHighlightedVal(mAvatarName, mAvatarNameStyle, name, highlight);
	// </FS:Ansariel>
}

void LLAvatarListItem::onAvatarNameCache(const LLAvatarName& av_name)
{
	mAvatarNameCacheConnection.disconnect();
	//mGreyOutUsername = "";
	//std::string name_string = mShowCompleteName? av_name.getCompleteName(false) : av_name.getDisplayName();
	//if(av_name.getCompleteName() != av_name.getUserName())
	//{
	//    mGreyOutUsername = "[ " + av_name.getUserName(true) + " ]";
	//    LLStringUtil::toLower(mGreyOutUsername);
	//}
	//setAvatarName(name_string);
	//setAvatarToolTip(av_name.getUserName());
// [RLVa:KB] - Checked: 2010-10-31 (RLVa-1.2.2a) | Modified: RLVa-1.2.2a
	// <FS:Ansa> Centralized in LLAvatarList::getNameForDisplay!
	bool fRlvFilter = (mRlvCheckShowNames) && (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES));
	//if (mShowDisplayName && !mShowUsername)
	//	setAvatarName( (!fRlvFilter) ? av_name.getDisplayName() : RlvStrings::getAnonym(av_name) );
	//else if (!mShowDisplayName && mShowUsername)
	//	setAvatarName( (!fRlvFilter) ? av_name.getUserName() : RlvStrings::getAnonym(av_name) );
	//else 
	//	setAvatarName( (!fRlvFilter) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name) );
	setAvatarName(LLAvatarList::getNameForDisplay(av_name, mShowDisplayName, mShowUsername, mRlvCheckShowNames));

	// NOTE: If you change this, you will break sorting the contacts list
	//  by username unless you go change the comparator too. -- TS	
	setAvatarToolTip( (!fRlvFilter) ? av_name.getUserName() : RlvStrings::getAnonym(av_name) );
	// TODO-RLVa: bit of a hack putting this here. Maybe find a better way?
	mAvatarIcon->setDrawTooltip(!fRlvFilter);
// [/RLVa:KB]

	//KC - store the username for use in sorting
	mUserName = av_name.getUserName();
	
	//requesting the list to resort
	notifyParent(LLSD().with("sort", LLSD()));
	
	//update children, because this call tends to effect the size of the name field width
	updateChildren();
}

// Convert given number of seconds to a string like "23 minutes", "15 hours" or "3 years",
// taking i18n into account. The format string to use is taken from the panel XML.
std::string LLAvatarListItem::formatSeconds(U32 secs)
{
	static const U32 LL_ALI_MIN		= 60;
	static const U32 LL_ALI_HOUR	= LL_ALI_MIN	* 60;
	static const U32 LL_ALI_DAY		= LL_ALI_HOUR	* 24;
	static const U32 LL_ALI_WEEK	= LL_ALI_DAY	* 7;
	static const U32 LL_ALI_MONTH	= LL_ALI_DAY	* 30;
	static const U32 LL_ALI_YEAR	= LL_ALI_DAY	* 365;

	std::string fmt; 
	U32 count = 0;

	if (secs >= LL_ALI_YEAR)
	{
		fmt = "FormatYears"; count = secs / LL_ALI_YEAR;
	}
	else if (secs >= LL_ALI_MONTH)
	{
		fmt = "FormatMonths"; count = secs / LL_ALI_MONTH;
	}
	else if (secs >= LL_ALI_WEEK)
	{
		fmt = "FormatWeeks"; count = secs / LL_ALI_WEEK;
	}
	else if (secs >= LL_ALI_DAY)
	{
		fmt = "FormatDays"; count = secs / LL_ALI_DAY;
	}
	else if (secs >= LL_ALI_HOUR)
	{
		fmt = "FormatHours"; count = secs / LL_ALI_HOUR;
	}
	else if (secs >= LL_ALI_MIN)
	{
		fmt = "FormatMinutes"; count = secs / LL_ALI_MIN;
	}
	else
	{
		fmt = "FormatSeconds"; count = secs;
	}

	LLStringUtil::format_map_t args;
	args["[COUNT]"] = llformat("%u", count);
	return getString(fmt, args);
}

// static
LLAvatarListItem::icon_color_map_t& LLAvatarListItem::getItemIconColorMap()
{
	static icon_color_map_t item_icon_color_map;
	if (!item_icon_color_map.empty()) return item_icon_color_map;

	item_icon_color_map.insert(
		std::make_pair(IS_DEFAULT,
		LLUIColorTable::instance().getColor("AvatarListItemIconDefaultColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_INVITED,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceInvitedColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_JOINED,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceJoinedColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_VOICE_LEFT,
		LLUIColorTable::instance().getColor("AvatarListItemIconVoiceLeftColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_ONLINE,
		LLUIColorTable::instance().getColor("AvatarListItemIconOnlineColor", LLColor4::white)));

	item_icon_color_map.insert(
		std::make_pair(IS_OFFLINE,
		LLUIColorTable::instance().getColor("AvatarListItemIconOfflineColor", LLColor4::white)));

	// TS: Group moderators get the online color
	item_icon_color_map.insert(
		std::make_pair(IS_GROUPMOD,
		LLUIColorTable::instance().getColor("AvatarListItemIconOnlineColor", LLColor4::white)));

	return item_icon_color_map;
}



// static
void LLAvatarListItem::initChildrenWidths(LLAvatarListItem* avatar_item)
{
	//speaking indicator width + padding
	S32 speaking_indicator_width = avatar_item->getRect().getWidth() - avatar_item->mSpeakingIndicator->getRect().mLeft;

	//profile btn width + padding
	S32 profile_btn_width = avatar_item->mSpeakingIndicator->getRect().mLeft - avatar_item->mProfileBtn->getRect().mLeft;

	//info btn width + padding
	S32 info_btn_width = avatar_item->mProfileBtn->getRect().mLeft - avatar_item->mInfoBtn->getRect().mLeft;

	//volume slider width + padding
	S32 volume_slider_width = avatar_item->mInfoBtn->getRect().mLeft - avatar_item->mVoiceSlider->getRect().mLeft;

	// online permission icon width + padding
	S32 permission_online_width = avatar_item->mVoiceSlider->getRect().mLeft - avatar_item->mBtnPermissionOnline->getRect().mLeft;

	// map permission icon width + padding
	S32 permission_map_width = avatar_item->mBtnPermissionOnline->getRect().mLeft - avatar_item->mBtnPermissionMap->getRect().mLeft;

	// edit my objects permission icon width + padding
	S32 permission_edit_mine_width = avatar_item->mBtnPermissionMap->getRect().mLeft - avatar_item->mBtnPermissionEditMine->getRect().mLeft;

	// edit their objects permission icon width + padding
	S32 permission_edit_theirs_width = avatar_item->mBtnPermissionEditMine->getRect().mLeft - avatar_item->mIconPermissionEditTheirs->getRect().mLeft;
	
	// last interaction time textbox width + padding
	S32 last_interaction_time_width = avatar_item->mIconPermissionEditTheirs->getRect().mLeft - avatar_item->mLastInteractionTime->getRect().mLeft;
	
	// avatar icon width + padding
	S32 icon_width = avatar_item->mAvatarName->getRect().mLeft - avatar_item->mAvatarIcon->getRect().mLeft;

	sLeftPadding = avatar_item->mAvatarIcon->getRect().mLeft;

	S32 index = ALIC_COUNT;
	sChildrenWidths[--index] = icon_width;
	sChildrenWidths[--index] = 0; // for avatar name we don't need its width, it will be calculated as "left available space"
	sChildrenWidths[--index] = last_interaction_time_width;
	sChildrenWidths[--index] = permission_edit_theirs_width;
	sChildrenWidths[--index] = permission_edit_mine_width;
	sChildrenWidths[--index] = permission_map_width;
	sChildrenWidths[--index] = permission_online_width;
	sChildrenWidths[--index] = volume_slider_width;
	sChildrenWidths[--index] = info_btn_width;
	sChildrenWidths[--index] = profile_btn_width;
	sChildrenWidths[--index] = speaking_indicator_width;
	llassert(index == 0);
}

void LLAvatarListItem::updateChildren()
{
	LL_DEBUGS("AvatarItemReshape") << LL_ENDL;
	LL_DEBUGS("AvatarItemReshape") << "Updating for: " << getAvatarName() << LL_ENDL;

	S32 name_new_width = getRect().getWidth();
	S32 ctrl_new_left = name_new_width;
	S32 name_new_left = sLeftPadding;

	// iterate through all children and set them into correct position depend on each child visibility
	// assume that child indexes are in back order: the first in Enum is the last (right) in the item
	// iterate & set child views starting from right to left
	for (S32 i = 0; i < ALIC_COUNT; ++i)
	{
		// skip "name" textbox, it will be processed out of loop later
		if (ALIC_NAME == i) continue;

		LLView* control = getItemChildView((EAvatarListItemChildIndex)i);

		LL_DEBUGS("AvatarItemReshape") << "Processing control: " << control->getName() << LL_ENDL;
		// skip invisible views
		if (!control->getVisible()) continue;

		S32 ctrl_width = sChildrenWidths[i]; // including space between current & left controls

		// decrease available for 
		name_new_width -= ctrl_width;
		LL_DEBUGS("AvatarItemReshape") << "width: " << ctrl_width << ", name_new_width: " << name_new_width << LL_ENDL;

		LLRect control_rect = control->getRect();
		LL_DEBUGS("AvatarItemReshape") << "rect before: " << control_rect << LL_ENDL;

		if (ALIC_ICON == i)
		{
			// assume that this is the last iteration,
			// so it is not necessary to save "ctrl_new_left" value calculated on previous iterations
			ctrl_new_left = sLeftPadding;
			name_new_left = ctrl_new_left + ctrl_width;
		}
		else
		{
			ctrl_new_left -= ctrl_width;
		}

		LL_DEBUGS("AvatarItemReshape") << "ctrl_new_left: " << ctrl_new_left << LL_ENDL;

		control_rect.setLeftTopAndSize(
			ctrl_new_left,
			control_rect.mTop,
			control_rect.getWidth(),
			control_rect.getHeight());

		LL_DEBUGS("AvatarItemReshape") << "rect after: " << control_rect << LL_ENDL;
		control->setShape(control_rect);
	}

	// set size and position of the "name" child
	LLView* name_view = getItemChildView(ALIC_NAME);
	LLRect name_view_rect = name_view->getRect();
	LL_DEBUGS("AvatarItemReshape") << "name rect before: " << name_view_rect << LL_ENDL;

	// apply paddings
	name_new_width -= sLeftPadding;
	name_new_width -= sNameRightPadding;

	name_view_rect.setLeftTopAndSize(
		name_new_left,
		name_view_rect.mTop,
		name_new_width,
		name_view_rect.getHeight());

	name_view->setShape(name_view_rect);

	LL_DEBUGS("AvatarItemReshape") << "name rect after: " << name_view_rect << LL_ENDL;
}

void LLAvatarListItem::setShowPermissions(bool show)
{
	mShowPermissions=show;
	showPermissions(show);
}

bool LLAvatarListItem::showPermissions(bool visible)
{
	const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if(relation && visible)
	{
		/*
		 * Change visibility method from removing the icon to just hiding it.
		 * This lets the hidden icons fill a position and prevent reflow
		 * Allows for V1-like absolute permission positioning. -AO
		 *
		 * 
		 * mIconPermissionOnline->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS));
		 * mIconPermissionMap->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION));
		 * mIconPermissionEditMine->setVisible(relation->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS));
		 * mIconPermissionEditTheirs->setVisible(relation->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS));
		 *
		*/ 
		
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS))
			mBtnPermissionOnline->setColor(LLUIColorTable::instance().getColor("White_10"));
		else 
			mBtnPermissionOnline->setColor(LLUIColorTable::instance().getColor("White"));
		
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))
			mBtnPermissionMap->setColor(LLUIColorTable::instance().getColor("White_10"));			
		else
			mBtnPermissionMap->setColor(LLUIColorTable::instance().getColor("White"));
			
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
			mBtnPermissionEditMine->setColor(LLUIColorTable::instance().getColor("White_10"));
		else
			mBtnPermissionEditMine->setColor(LLUIColorTable::instance().getColor("White"));
				
		if (!relation->isRightGrantedFrom(LLRelationship::GRANT_MODIFY_OBJECTS))
			mIconPermissionEditTheirs->setColor(LLUIColorTable::instance().getColor("White_10"));
		else
			mIconPermissionEditTheirs->setColor(LLUIColorTable::instance().getColor("White"));
		
		mBtnPermissionOnline->setVisible(true);
		mBtnPermissionMap->setVisible(true);
		mBtnPermissionEditMine->setVisible(true);
		mIconPermissionEditTheirs->setVisible(true);
			
	}
	else
	{
		mBtnPermissionOnline->setVisible(false);
		mBtnPermissionMap->setVisible(false);
		mBtnPermissionEditMine->setVisible(false);
		mIconPermissionEditTheirs->setVisible(false);
	}
	
	updateChildren();
	return NULL != relation;
}

void LLAvatarListItem::onPermissionOnlineClick()
{
	const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if(relation)
	{
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_ONLINE_STATUS))
		{
			new_rights = LLRelationship::GRANT_ONLINE_STATUS + (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
			mBtnPermissionOnline->setColor(LLUIColorTable::instance().getColor("White"));
		}
		else
		{
			new_rights = (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
			mBtnPermissionOnline->setColor(LLUIColorTable::instance().getColor("White_10"));
		}
		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(getAvatarId(),new_rights);
		mBtnPermissionOnline->setFocus(FALSE);
	}
}

void LLAvatarListItem::onPermissionMapClick()
{
	const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if(relation)
	{
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))
		{
			new_rights = LLRelationship::GRANT_MAP_LOCATION + (cur_rights &  LLRelationship::GRANT_ONLINE_STATUS) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
			mBtnPermissionMap->setColor(LLUIColorTable::instance().getColor("White"));
		}
		else 
		{
			new_rights = (cur_rights &  LLRelationship::GRANT_ONLINE_STATUS) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
			mBtnPermissionMap->setColor(LLUIColorTable::instance().getColor("White_10"));
		}
		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(getAvatarId(),new_rights);
		mBtnPermissionMap->setFocus(FALSE);
	}
}

void LLAvatarListItem::onPermissionEditMineClick()
{
	const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
	if(relation)
	{
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (!relation->isRightGrantedTo(LLRelationship::GRANT_MODIFY_OBJECTS))
		{
			new_rights = LLRelationship::GRANT_MODIFY_OBJECTS + (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_ONLINE_STATUS);			
			confirmModifyRights(true, new_rights);
		}
		else
		{
			new_rights = (cur_rights &  LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_ONLINE_STATUS);
			mBtnPermissionEditMine->setColor(LLUIColorTable::instance().getColor("White_10"));
			LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(getAvatarId(),new_rights);
		}
		
		mBtnPermissionEditMine->setFocus(FALSE);
	}
}

void LLAvatarListItem::confirmModifyRights(bool grant, S32 rights)
// Same as llpanelavatar::confirmModifyRights
{
	LLSD args;
	// <FS:Ansariel> Always show complete name in rights confirmation dialogs
	//args["NAME"] = LLSLURL("agent", getAvatarId(), "displayname").getSLURLString();
	args["NAME"] = LLSLURL("agent", getAvatarId(), "completename").getSLURLString();
	
	if (grant)
	{
		LLNotificationsUtil::add("GrantModifyRights", args, LLSD(),
								 boost::bind(&LLAvatarListItem::rightsConfirmationCallback, this,
											 _1, _2, rights));
	}
	else
	{
		LLNotificationsUtil::add("RevokeModifyRights", args, LLSD(),
								 boost::bind(&LLAvatarListItem::rightsConfirmationCallback, this,
											 _1, _2, rights));
	}
}

void LLAvatarListItem::rightsConfirmationCallback(const LLSD& notification,
													const LLSD& response, S32 rights)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (option == 0)
	{
		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(getAvatarId(), rights);
		mBtnPermissionEditMine->setColor(LLUIColorTable::instance().getColor("White"));
	}
}

LLView* LLAvatarListItem::getItemChildView(EAvatarListItemChildIndex child_view_index)
{
	LLView* child_view = mAvatarName;

	switch (child_view_index)
	{
	case ALIC_ICON:
		child_view = mAvatarIcon;
		break;
	case ALIC_NAME:
		child_view = mAvatarName;
		break;
	case ALIC_INTERACTION_TIME:
		child_view = mLastInteractionTime;
		break;
	case ALIC_SPEAKER_INDICATOR:
		child_view = mSpeakingIndicator;
		break;
	case ALIC_PERMISSION_ONLINE:
		child_view = mBtnPermissionOnline;
		break;
	case ALIC_PERMISSION_MAP:
		child_view = mBtnPermissionMap;
		break;
	case ALIC_PERMISSION_EDIT_MINE:
		child_view = mBtnPermissionEditMine;
		break;
	case ALIC_PERMISSION_EDIT_THEIRS:
		child_view = mIconPermissionEditTheirs;
		break;
	case ALIC_INFO_BUTTON:
		child_view = mInfoBtn;
		break;
	case ALIC_PROFILE_BUTTON:
		child_view = mProfileBtn;
		break;
	case ALIC_VOLUME_SLIDER:
		child_view = mVoiceSlider;
		break;
	default:
		LL_WARNS("AvatarItemReshape") << "Unexpected child view index is passed: " << child_view_index << LL_ENDL;
		// leave child_view untouched
	}
	
	return child_view;
}

// EOF

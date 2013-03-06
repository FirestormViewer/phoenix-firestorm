/** 
 * @file kcwlinterface.cpp
 * @brief Parcel Windlight Interface
 *
 * Copyright (C) 2010, Kadah Coba
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

#include "kcwlinterface.h"

#include "llagent.h"
#include "llcallingcard.h" // isBuddy
#include "llstartup.h"
#include "llstatusbar.h"
#include "llparcel.h"
#include "llviewercontrol.h" // gSavedSettings, gSavedPerAccountSettings
#include "llviewermenu.h" // is_agent_friend
#include "llviewerparcelmgr.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "lldaycyclemanager.h"

#include <boost/regex.hpp>

#include "rlvhandler.h"

const F32 PARCEL_WL_CHECK_TIME  = 5;
const S32 PARCEL_WL_MIN_ALT_CHANGE = 3;

KCWindlightInterface::KCWindlightInterface() :
	LLEventTimer(PARCEL_WL_CHECK_TIME),
	mWLset(false),
	mWeChangedIt(false),
	mCurrentSpace(-2.f),
	mLastParcelID(-1),
	mLastRegion(NULL),
	mRegionOverride(false),
	mHaveRegionSettings(false)
{
	if (!gSavedSettings.getBOOL("FSWLParcelEnabled") ||
	!gSavedSettings.getBOOL("UseEnvironmentFromRegionAlways"))
	{
		mEventTimer.stop();
		mDisabled = true;
	}
}

void KCWindlightInterface::ParcelChange()
{
	if (checkSettings())
		return;
	
	mDisabled = false;

	LLParcel *parcel = NULL;
	S32 this_parcel_id = 0;
	std::string desc;

 	parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	// Since we cannot depend on the order in which the EnvironmentSettings cap and parcel info
	// will come in, we must check if the other has set something before this one for the current region.
	if (gAgent.getRegion() != mLastRegion)
	{
		mRegionOverride = false;
		mHaveRegionSettings = false;
		mLastRegion = gAgent.getRegion();
	}

	if (parcel)
	{
		this_parcel_id = parcel->getLocalID();
		desc = parcel->getDesc();
	}

	if ( (this_parcel_id != mLastParcelID) || (mLastParcelDesc != desc) ) //parcel changed
	{
		//llinfos << "agent in new parcel: "<< this_parcel_id << " : "  << parcel->getName() << llendl;

		mLastParcelID = this_parcel_id;
		mLastParcelDesc = desc;
		mCurrentSpace = -2.f;
		mCurrentSettings.clear();
		setWL_Status(false); //clear the status bar icon
		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		mLastZ = lltrunc( agent_pos_region.mV[VZ] );

		//clear the last notification if its still open
		if (mSetWLNotification && !mSetWLNotification->isRespondedTo())
		{
			LLSD response = mSetWLNotification->getResponseTemplate();
			response["Ignore"] = true;
			mSetWLNotification->respond(response);
		}
		
		mEventTimer.reset();
		mEventTimer.start();
		
		// Apply new WL settings instantly on TP
		if (mTPing)
		{
			mTPing = false;
			tick();
		}
	}
}

BOOL KCWindlightInterface::tick()
{
	if ((LLStartUp::getStartupState() < STATE_STARTED) || checkSettings())
		return FALSE;
	
	//TODO: there has to be a better way of doing this...
	if (mCurrentSettings.has("sky"))
	{
		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		S32 z = lltrunc( agent_pos_region.mV[VZ] );
		if (llabs(z - mLastZ) >= PARCEL_WL_MIN_ALT_CHANGE)
		{
			mLastZ = z;
			ApplySkySettings(mCurrentSettings);
		}
		return FALSE;
	}

	LLParcel *parcel = NULL;

	parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	if (parcel)
	{
		LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (!LoadFromPacel(parcel) || !mCurrentSettings.has("sky"))
			mEventTimer.stop();
	}

	return FALSE;
}


void KCWindlightInterface::ApplySettings(const LLSD& settings)
{
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!settings.has("local_id") || (settings["local_id"].asInteger() == parcel->getLocalID()) )
	{
		mCurrentSettings = settings;
		
		mRegionOverride = settings.has("region_override");

		ApplySkySettings(settings);

		if (settings.has("water") && (!mHaveRegionSettings || mRegionOverride))
		{
			LLEnvManagerNew::instance().setUseWaterPreset(settings["water"].asString(), gSavedSettings.getBOOL("FSInterpolateParcelWL"));
			setWL_Status(true);
		}
	}
}

void KCWindlightInterface::ApplySkySettings(const LLSD& settings)
{
	if (settings.has("sky"))
	{
		//TODO: there has to be a better way of doing this...
		mEventTimer.reset();
		mEventTimer.start();

		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		S32 z = lltrunc( agent_pos_region.mV[VZ] );
		LLSD::array_const_iterator end_it = settings["sky"].endArray();
		for (LLSD::array_const_iterator space_it = settings["sky"].beginArray(); space_it != end_it; ++space_it)
		{
			S32 lower = (*space_it)["lower"].asInteger();
			S32 upper = (*space_it)["upper"].asInteger();
			if ( (z >= lower) && (z <= upper) )
			{
				if (lower != mCurrentSpace) //workaround: only apply once
				{
					mCurrentSpace = lower; //use lower as an id
					ApplyWindLightPreset((*space_it)["preset"].asString());
				}
				return;
			}
		}
	}

	if (mCurrentSpace != -1.f)
	{
		mCurrentSpace = -1.f;
		// set notes on KCWindlightInterface::haveParcelOverride
		if (settings.has("sky_default") && (!mHaveRegionSettings || mRegionOverride))
		{
			//llinfos << "WL set : " << settings["sky_default"] << llendl;
			ApplyWindLightPreset(settings["sky_default"].asString());
		}
		else //reset to default
		{
			//llinfos << "WL set : Default" << llendl;
			ApplyWindLightPreset("Default");
		}
	}
}

void KCWindlightInterface::ApplyWindLightPreset(const std::string& preset)
{
	if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SETENV))
		return;

	LLWLParamManager* wlprammgr = LLWLParamManager::getInstance();
	LLWLParamKey key(preset, LLEnvKey::SCOPE_LOCAL);
	if ( (preset != "Default") && (wlprammgr->hasParamSet(key)) )
	{
		LLEnvManagerNew::instance().setUseSkyPreset(preset, gSavedSettings.getBOOL("FSInterpolateParcelWL"));
		setWL_Status(true);
		mWeChangedIt = true;
	}
	else
	{
		if (!LLEnvManagerNew::instance().getUseRegionSettings())
			LLEnvManagerNew::instance().setUseRegionSettings(true, gSavedSettings.getBOOL("FSInterpolateParcelWL"));
		setWL_Status(false);
		mWeChangedIt = false;
	}
}

void KCWindlightInterface::ResetToRegion(bool force)
{
	if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SETENV))
		return;

	//TODO: clear per parcel
	if (mWeChangedIt || force) //dont reset anything if we didnt do it
	{
		ApplyWindLightPreset("Default");
	}
}

//KC: Disabling this for now
#if 0
bool KCWindlightInterface::ChatCommand(std::string message, std::string from_name, LLUUID source_id, LLUUID owner_id)
{
	boost::cmatch match;
	const boost::regex prefix_exp("^\\)\\*\\((.*)");
	if(boost::regex_match(message.c_str(), match, prefix_exp))
	{
		std::string data(match[1].first, match[1].second);
		
		//TODO: expand these or good as is?
		/*const boost::regex setWLpreset_exp("^setWLpreset\\|(.*)");
		const boost::regex setWWpreset_exp("^setWWpreset\\|(.*)");
		if(boost::regex_match(data.c_str(), match, setWLpreset_exp))
		{
			llinfos << "got setWLpreset : " << match[1] << llendl;
			LLWLParamManager::instance()->mAnimator.mIsRunning = false;
			LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
			LLWLParamManager::instance()->loadPreset(match[1]);
			return true;
		}
		else if(boost::regex_match(data.c_str(), match, setWWpreset_exp))
		{
			llinfos << "got setWWpreset : " << match[1] << llendl;
			LLWaterParamManager::instance()->loadPreset(match[1], true);
			return true;
		}
		else 
		{*/
		
		//TODO: add save settings for reuse instead of just clearing on parcel change
		//TODO: add support for region wide settings on non-mainland
		//TODO: add support for targeting specfic users
		//TODO: add support for custom settings via notecards or something
		//TODO: improved data processing, possibly just use LLSD as input instead
		
			boost::smatch match2;
			const boost::regex Parcel_exp("^(Parcel),WLPreset=\"([^\"\\r\\n]+)\"(,WWPreset=\"([^\"\\r\\n]+)\")?$");
			//([\\w]{8}-[\\w]{4}-[\\w]{4}-[\\w]{4}-[\\w]{12})
			if(boost::regex_search(data, match2, Parcel_exp))
			{
				if (SetDialogVisible) //TODO: handle this better
					return true;

				if (match2[1]=="Parcel")
				{
					llinfos << "Got Parcel WL : " << match[2] << llendl;
					
					LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
					LLSD payload;
					payload["local_id"] = parcel->getLocalID();
					payload["land_owner"] = parcel->getOwnerID();
					payload["wlpreset"] = std::string(match2[2].first, match2[2].second);
					payload["wwpreset"] = std::string(match2[3].first, match2[3].second);

					LLSD args;
					args["PARCEL_NAME"] = parcel->getName();
					
					LLNotifications::instance().add("FSWL", args, payload, boost::bind(&KCWindlightInterface::callbackParcelWL, this, _1, _2));
					SetDialogVisible = true;
				}
				return true;
			}
		/*}*/
	}
	return false;
}
#endif

bool KCWindlightInterface::LoadFromPacel(LLParcel *parcel)
{
	if (!parcel)
		return false;

	LLSD payload;
	if (ParsePacelForWLSettings(parcel->getDesc(), payload))
	{
		const LLUUID owner_id = getOwnerID(parcel);
		//basic auth for now
		if (AllowedLandOwners(owner_id))
		{
			ApplySettings(payload);
		}
		else
		{
			LLSD args;
			args["PARCEL_NAME"] = parcel->getName();
			args["OWNER_NAME"] = getOwnerName(parcel);
			payload["parcel_name"] = parcel->getName();
			payload["local_id"] = parcel->getLocalID();
			payload["land_owner"] = owner_id;

			mSetWLNotification = LLNotifications::instance().add("FSWL", args, payload, boost::bind(&KCWindlightInterface::callbackParcelWL, this, _1, _2));
		}
		return true;
	}
	
	//if nothing defined, reset to region settings
	ResetToRegion();

	return false;
}

bool KCWindlightInterface::ParsePacelForWLSettings(const std::string& desc, LLSD& settings)
{
	bool found_settings = false;
	try
	{
		boost::smatch mat_block;
		//parcel desc /*[data goes here]*/
		const boost::regex Parcel_exp("(?i)\\/\\*(?:Windlight)?([\\s\\S]*?)\\*\\/");
		if(boost::regex_search(desc, mat_block, Parcel_exp))
		{
			std::string data1(mat_block[1].first, mat_block[1].second);
			//llinfos << "found parcel flags block: " << mat_block[1] << llendl;
			
			S32 sky_index = 0;
			LLWLParamManager* wlprammgr = LLWLParamManager::getInstance();
			LLWaterParamManager* wwprammgr = LLWaterParamManager::getInstance();
			boost::smatch match;
			std::string::const_iterator start = mat_block[1].first;
			std::string::const_iterator end = mat_block[1].second;
			//Sky: "preset" Water: "preset"
			const boost::regex key("(?i)(?:(?:(Sky)(?:\\s?@\\s?([\\d]+)m?\\s?(?:to|-)\\s?([\\d]+)m?)?)|(Water))\\s?:\\s?\"([^\"\\r\\n]+)\"|(RegionOverride)");
			while (boost::regex_search(start, end, match, key, boost::match_default))
			{
				if (match[1].matched)
				{
					//llinfos << "sky flag: " << match[1] << " : " << match[2] << " : " << match[3] << " : " << match[5] << llendl;

					std::string preset(match[5]);
					LLWLParamKey key(preset, LLEnvKey::SCOPE_LOCAL);
					if(wlprammgr->hasParamSet(key))
					{
						if (match[2].matched && match[3].matched)
						{
							S32 lower = (S32)atoi(std::string(match[2]).c_str());
							S32 upper = (S32)atoi(std::string(match[3]).c_str());
							if ( (upper > lower) && (lower >= 0) )
							{
								LLSD space;
								space["lower"] = lower;
								space["upper"] = upper;
								space["preset"] = preset;
								if (!settings.has("sky"))
									settings["sky"] = LLSD();
								settings["sky"][sky_index++] = space;
								found_settings = true;
							}
						}
						else
						{
							settings["sky_default"] = preset;
							found_settings = true;
						}
					}
				}
				else if (match[4].matched)
				{
					std::string preset(match[5]);
					//llinfos << "got water: " << preset << llendl;
					if(wwprammgr->hasParamSet(preset))
					{
						settings["water"] = preset;
						found_settings = true;
					}
				}
				else if (match[6].matched)
				{
					std::string preset(match[5]);
					llinfos << "got region override flag" << llendl;
					settings["region_override"] = true;
				}
				
				// update search position 
				start = match[0].second; 
			}
		}
	}
	catch(...)
	{
		found_settings = false;
	}

	return found_settings;
}

void KCWindlightInterface::onClickWLStatusButton()
{
	//clear the last notification if its still open
	if (mClearWLNotification && !mClearWLNotification->isRespondedTo())
	{
		LLSD response = mClearWLNotification->getResponseTemplate();
		response["Ignore"] = true;
		mClearWLNotification->respond(response);
	}

	if (mWLset)
	{
		LLParcel *parcel = NULL;
 		parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
 		if (parcel)
		{
			//TODO: this could be better
			LLSD payload;
			payload["local_id"] = parcel->getLocalID();
			payload["land_owner"] = getOwnerID(parcel);

			LLSD args;
			args["PARCEL_NAME"] = parcel->getName();
			
			mClearWLNotification = LLNotifications::instance().add("FSWLClear", args, payload, boost::bind(&KCWindlightInterface::callbackParcelWLClear, this, _1, _2));
		}
	}
}

bool KCWindlightInterface::callbackParcelWL(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		mAllowedLand.insert(notification["payload"]["land_owner"].asUUID());
		
		ApplySettings(notification["payload"]);
	}
	else
	{
		ResetToRegion();
	}
	return false;
}

bool KCWindlightInterface::callbackParcelWLClear(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option == 0)
	{
		LLUUID owner_id = notification["payload"]["land_owner"].asUUID();

		mAllowedLand.erase(owner_id);
		ResetToRegion();
	}
	return false;
}

bool KCWindlightInterface::AllowedLandOwners(const LLUUID& owner_id)
{
	if ( gSavedSettings.getBOOL("FSWLWhitelistAll") ||	// auto all
		(owner_id == gAgent.getID()) ||						// land is owned by agent
		(LLAvatarTracker::instance().isBuddy(owner_id) && gSavedSettings.getBOOL("FSWLWhitelistFriends")) || // is friend's land
		(gAgent.isInGroup(owner_id) && gSavedSettings.getBOOL("FSWLWhitelistGroups")) || // is member of land's group
		(mAllowedLand.find(owner_id) != mAllowedLand.end()) ) // already on whitelist
	{
		return true;
	}
	return false;
}

LLUUID KCWindlightInterface::getOwnerID(LLParcel *parcel)
{
	if (parcel->getIsGroupOwned())
	{
		return parcel->getGroupID();
	}
	return parcel->getOwnerID();
}

std::string KCWindlightInterface::getOwnerName(LLParcel *parcel)
{
	//TODO: say if its a group or avatar on notice
	std::string owner;
	if (parcel->getIsGroupOwned())
	{
		gCacheName->getGroupName(parcel->getGroupID(), owner);
	}
	else
	{
		gCacheName->getFullName(parcel->getOwnerID(), owner);
	}
	return owner;
}

//KC: this is currently not used
//TODO: merge this relay code in to bridge when more final, currently only supports "Parcel,WLPreset='[preset name]'"
/*
integer PHOE_WL_CH = -1346916165;
default
{
    state_entry()
    {
		llListen(PHOE_WL_CH, "", NULL_KEY, "");
    }
    listen( integer iChan, string sName, key kID, string sMsg )
    {
        if ( llGetOwnerKey(kID) == llGetLandOwnerAt(llGetPos()) )
        {
            llOwnerSay(")*(" + sMsg);
        }
    }
}
*/

// Region settings are prefered for default parcel ones
// But parcel height mapped skies always override region's
// And parcels can use the "RegionOverride" in their config line
bool KCWindlightInterface::haveParcelOverride(const LLEnvironmentSettings& new_settings)
{
	// Since we cannot depend on the order in which the EnvironmentSettings cap and parcel info
	// will come in, we must check if the other has set something before this one for the current region.
	if (gAgent.getRegion() != mLastRegion)
	{
		mRegionOverride = false;
		mCurrentSettings.clear();
		mLastRegion = gAgent.getRegion();
	}
	
	//*ASSUMPTION: if region day cycle is empty, its set to default
	mHaveRegionSettings = new_settings.getWLDayCycle().size() > 0;
	
	return  mRegionOverride || mCurrentSpace != -1.f;
}

void KCWindlightInterface::setWL_Status(bool pwl_status)
{
	mWLset = pwl_status;
	gStatusBar->updateParcelIcons();
}

bool KCWindlightInterface::checkSettings()
{
	static LLCachedControl<bool> sFSWLParcelEnabled(gSavedSettings, "FSWLParcelEnabled");
	static LLCachedControl<bool> sUseEnvironmentFromRegionAlways(gSavedSettings, "UseEnvironmentFromRegionAlways");
	if (!sFSWLParcelEnabled || !sUseEnvironmentFromRegionAlways ||
	(rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SETENV)))
	{
		// The setting changed, clear everything
		if (!mDisabled)
		{
			mCurrentSettings.clear();
			mWeChangedIt = false;
			mCurrentSpace = -2.f;
			mLastParcelID = -1;
			mRegionOverride = false;
			mHaveRegionSettings = false;
			mLastRegion = NULL;
			mEventTimer.stop();
			setWL_Status(false);
			mDisabled = true;
		}
		return true;
	}
	mDisabled = false;
	return false;
}

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
#include "lldaycyclemanager.h"
#include "llparcel.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "llviewercontrol.h" // gSavedSettings, gSavedPerAccountSettings
#include "llviewerparcelmgr.h"
#include "llwaterparammanager.h"
#include "llwlparammanager.h"
#include "rlvhandler.h"

#include <boost/regex.hpp>

const F32 PARCEL_WL_CHECK_TIME  = 5.f;
const S32 PARCEL_WL_MIN_ALT_CHANGE = 3;
const std::string PARCEL_WL_DEFAULT = "Default";

const S32 NO_ZONES = -1; // Parcel WL is using default sky or region defaults (no height-mapped parcel WL)
const S32 NO_SETTINGS = -2; // We didn't receive any parcel WL settings yet

KCWindlightInterface::KCWindlightInterface() :
	LLEventTimer(PARCEL_WL_CHECK_TIME),
	mWLset(false),
	mWeChangedIt(false),
	mCurrentSpace(NO_SETTINGS),
	mLastParcelID(-1),
	mLastRegion(NULL),
	mHasRegionOverride(false),
	mHaveRegionSettings(false),
	mDisabled(false),
	mUsingParcelWLSkyDefault(false)
{
	if (!gSavedSettings.getBOOL("FSWLParcelEnabled") || !gSavedSettings.getBOOL("UseEnvironmentFromRegionAlways"))
	{
		mEventTimer.stop();
		mDisabled = true;
	}
	
	mParcelMgrConnection = gAgent.addParcelChangedCallback(boost::bind(&KCWindlightInterface::parcelChange, this));
}

KCWindlightInterface::~KCWindlightInterface()
{
	if (mParcelMgrConnection.connected())
	{
		mParcelMgrConnection.disconnect();
	}
}

void KCWindlightInterface::parcelChange()
{
	if (checkSettings())
	{
		return;
	}

	mDisabled = false;

	S32 this_parcel_id = 0;
	std::string desc;

	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	// Since we cannot depend on the order in which the EnvironmentSettings cap and parcel info
	// will come in, we must check if the other has set something before this one for the current region.
	if (gAgent.getRegion() != mLastRegion)
	{
		mHaveRegionSettings = false;
		mLastRegion = gAgent.getRegion();
		mCurrentSpace = NO_SETTINGS;
	}
	mHasRegionOverride = false;
	mUsingParcelWLSkyDefault = false;

	if (parcel)
	{
		this_parcel_id = parcel->getLocalID();
		desc = parcel->getDesc();
	}

	if ( (this_parcel_id != mLastParcelID) || (mLastParcelDesc != desc) ) //parcel changed
	{
		LL_DEBUGS() << "Agent in new parcel: " << this_parcel_id << LL_ENDL;

		mLastParcelID = this_parcel_id;
		mLastParcelDesc = desc;
		mCurrentSpace = NO_SETTINGS;
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
	{
		return FALSE;
	}
	
	//TODO: there has to be a better way of doing this...
	if (mCurrentSettings.has("sky"))
	{
		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		S32 z = lltrunc( agent_pos_region.mV[VZ] );
		if (llabs(z - mLastZ) >= PARCEL_WL_MIN_ALT_CHANGE)
		{
			mLastZ = z;
			applySkySettings(mCurrentSettings);
		}
		return FALSE;
	}

	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	if (parcel)
	{
		if (!loadFromParcel(parcel) || !mCurrentSettings.has("sky"))
		{
			mEventTimer.stop();
		}
	}

	return FALSE;
}


void KCWindlightInterface::applySettings(const LLSD& settings)
{
	LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (!settings.has("local_id") || (settings["local_id"].asInteger() == parcel->getLocalID()) )
	{
		mCurrentSettings = settings;
		
		mHasRegionOverride = settings.has("region_override");

		bool non_region_default_applied = applySkySettings(settings);

		// We can only apply a water preset if we didn't set region WL default previously
		// or there will be unpredictable behavior where the region WL defaults will be
		// disabled again and sky/day cycle presets will be reverted to whatever the user
		// has set before.
		if (non_region_default_applied)
		{
			if (settings.has("water") && (!mHaveRegionSettings || mHasRegionOverride))
			{
				LL_INFOS() << "Applying WL water set: " << settings["water"].asString() << LL_ENDL;
				LLEnvManagerNew::instance().setUseWaterPreset(settings["water"].asString(), gSavedSettings.getBOOL("FSInterpolateParcelWL"));
				setWL_Status(true);
			}
			else
			{
				LL_INFOS() << "Applying region default WL water set" << LL_ENDL;
				// Not nice to not interpolate, but these 2836724 methods of changing a WL
				// setting will nicely screw up each other and this will most likely happen
				// if calling useRegionWater() because it doesn't even interpolate at all.
				LLWLParamManager::getInstance()->mAnimator.stopInterpolation();
				LLEnvManagerNew::instance().useRegionWater();
			}
		}
		else
		{
			LL_WARNS() << "Cannot apply Parcel WL water preset because region WL default has been set due to invalid sky preset" << LL_ENDL;
		}
	}
}

bool KCWindlightInterface::applySkySettings(const LLSD& settings)
{
	if (settings.has("sky"))
	{
		LL_DEBUGS() << "Checking if agent is in a defined zone" << LL_ENDL;

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
					LL_INFOS() << "Applying WL sky set: " << (*space_it)["preset"].asString() << " (agent in zone " << lower << " to " << upper << ")" << LL_ENDL;
					applyWindlightPreset((*space_it)["preset"].asString());
				}
				return true;
			}
		}

		LL_DEBUGS() << "Agent is not within a defined zone. Trying default now" << LL_ENDL;
	}

	if (mCurrentSpace != NO_ZONES)
	{
		mCurrentSpace = NO_ZONES;
		// set notes on KCWindlightInterface::haveParcelOverride
		if (settings.has("sky_default") && (!mHaveRegionSettings || mHasRegionOverride))
		{
			LL_INFOS() << "Applying WL sky set: " << settings["sky_default"] << " (Parcel WL Default)" << LL_ENDL;
			mUsingParcelWLSkyDefault = true;
			applyWindlightPreset(settings["sky_default"].asString());
		}
		else //reset to default
		{
			mUsingParcelWLSkyDefault = false;
			std::string reason;
			if (!settings.has("sky_default"))
			{
				reason = "No zone or not in a defined zone and no default sky defined";
			}
			else if (mHaveRegionSettings && !mHasRegionOverride)
			{
				reason = "No zone defined or not in a defined zone, region has custom WL and \"RegionOverride\" parameter was not set";
			}

			LL_INFOS() << "Applying WL sky set \"Region Default\": " << reason << LL_ENDL;
			applyWindlightPreset(PARCEL_WL_DEFAULT);
			return false;
		}
	}

	return true;
}

void KCWindlightInterface::applyWindlightPreset(const std::string& preset)
{
	if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SETENV))
	{
		return;
	}

	LLWLParamManager* wlprammgr = LLWLParamManager::getInstance();
	LLWLParamKey key(preset, LLEnvKey::SCOPE_LOCAL);
	if ( (preset != PARCEL_WL_DEFAULT) && (wlprammgr->hasParamSet(key)) )
	{
		LLEnvManagerNew::instance().setUseSkyPreset(preset, gSavedSettings.getBOOL("FSInterpolateParcelWL"));
		setWL_Status(true);
		mWeChangedIt = true;
	}
	else
	{
		if (!LLEnvManagerNew::instance().getUseRegionSettings())
		{
			LLEnvManagerNew::instance().setUseRegionSettings(true, gSavedSettings.getBOOL("FSInterpolateParcelWL"));
		}
		setWL_Status(false);
		mWeChangedIt = false;
	}
}

void KCWindlightInterface::resetToRegion(bool force)
{
	if (rlv_handler_t::isEnabled() && gRlvHandler.hasBehaviour(RLV_BHVR_SETENV))
	{
		return;
	}

	//TODO: clear per parcel
	if (mWeChangedIt || force) //dont reset anything if we didnt do it
	{
		applyWindlightPreset(PARCEL_WL_DEFAULT);
	}
}

//KC: Disabling this for now
#if 0
bool KCWindlightInterface::chatCommand(std::string message, std::string from_name, LLUUID source_id, LLUUID owner_id)
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
			LL_INFOS() << "got setWLpreset : " << match[1] << LL_ENDL;
			LLWLParamManager::instance()->mAnimator.mIsRunning = false;
			LLWLParamManager::instance()->mAnimator.mUseLindenTime = false;
			LLWLParamManager::instance()->loadPreset(match[1]);
			return true;
		}
		else if(boost::regex_match(data.c_str(), match, setWWpreset_exp))
		{
			LL_INFOS() << "got setWWpreset : " << match[1] << LL_ENDL;
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
					LL_INFOS() << "Got Parcel WL : " << match[2] << LL_ENDL;
					
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

bool KCWindlightInterface::loadFromParcel(LLParcel *parcel)
{
	if (!parcel)
	{
		return false;
	}

	LLSD payload;
	if (parseParcelForWLSettings(parcel->getDesc(), payload))
	{
		const LLUUID owner_id = getOwnerID(parcel);
		//basic auth for now
		if (allowedLandOwners(owner_id))
		{
			applySettings(payload);
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
	resetToRegion();

	return false;
}

bool KCWindlightInterface::parseParcelForWLSettings(const std::string& desc, LLSD& settings)
{
	bool found_settings = false;
	try
	{
		boost::smatch mat_block;
		//parcel desc /*[data goes here]*/
		const boost::regex Parcel_exp("(?i)\\/\\*(?:Windlight)?([\\s\\S]*?)\\*\\/");
		if (boost::regex_search(desc, mat_block, Parcel_exp))
		{
			//std::string data1(mat_block[1].first, mat_block[1].second);
			//LL_INFOS() << "found parcel flags block: " << mat_block[1] << LL_ENDL;
			
			S32 sky_index = 0;
			LLWLParamManager* wlprammgr = LLWLParamManager::getInstance();
			LLWaterParamManager* wwprammgr = LLWaterParamManager::getInstance();
			boost::smatch match;
			std::string::const_iterator start = mat_block[1].first;
			std::string::const_iterator end = mat_block[1].second;
			//Sky: "preset" Water: "preset"
			const boost::regex key_regex("(?i)(?:(?:(Sky)(?:\\s?@\\s?([\\d]+)m?\\s?(?:to|-)\\s?([\\d]+)m?)?)|(Water))\\s?:\\s?\"([^\"\\r\\n]+)\"|(RegionOverride)");
			while (boost::regex_search(start, end, match, key_regex, boost::match_default))
			{
				if (match[1].matched)
				{
					LL_DEBUGS() << "Sky Flags: type = " << match[1] << " from = " << match[2] << " to = " << match[3] << " preset = " << match[5] << LL_ENDL;

					std::string preset(match[5]);
					LLWLParamKey key(preset, LLEnvKey::SCOPE_LOCAL);
					if (wlprammgr->hasParamSet(key))
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
								{
									settings["sky"] = LLSD();
								}
								settings["sky"][sky_index++] = space;
								found_settings = true;
							}
						}
						else
						{
							LL_DEBUGS() << "Sky Default = " << preset << LL_ENDL;
							settings["sky_default"] = preset;
							found_settings = true;
						}
					}
					else
					{
						LL_WARNS() << "Parcel Windlight contains unknown sky: " << preset << LL_ENDL;
					}
				}
				else if (match[4].matched)
				{
					std::string preset(match[5]);
					LL_DEBUGS() << "Got Water Preset: " << preset << LL_ENDL;
					if(wwprammgr->hasParamSet(preset))
					{
						settings["water"] = preset;
						found_settings = true;
					}
				}
				else if (match[6].matched)
				{
					LL_DEBUGS() << "Got Region Override Flag" << LL_ENDL;
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
 		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
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
		
		applySettings(notification["payload"]);
	}
	else
	{
		resetToRegion();
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
		resetToRegion();
	}
	return false;
}

bool KCWindlightInterface::allowedLandOwners(const LLUUID& owner_id)
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

LLUUID KCWindlightInterface::getOwnerID(LLParcel* parcel)
{
	if (parcel->getIsGroupOwned())
	{
		return parcel->getGroupID();
	}
	return parcel->getOwnerID();
}

std::string KCWindlightInterface::getOwnerName(LLParcel* parcel)
{
	std::string owner = "";
	if (parcel->getIsGroupOwned())
	{
		owner = LLSLURL("group", parcel->getGroupID(), "inspect").getSLURLString();
	}
	else
	{
		owner = LLSLURL("agent", parcel->getOwnerID(), "inspect").getSLURLString();
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
		mHasRegionOverride = false;
		mUsingParcelWLSkyDefault = false;
		mCurrentSettings.clear();
		mLastRegion = gAgent.getRegion();
		mCurrentSpace = NO_SETTINGS;
	}

	//*ASSUMPTION: if region day cycle is empty, its set to default
	mHaveRegionSettings = new_settings.getWLDayCycle().size() > 0;

	bool has_override = (mCurrentSpace == NO_ZONES && mUsingParcelWLSkyDefault && mHasRegionOverride) ||	// Using a default parcel WL sky and "RegionOverride" parameter set
						(mCurrentSpace == NO_ZONES && !mHaveRegionSettings) ||								// Custom parcel WL default sky and region default WL (no custom region default WL!)
						(mCurrentSpace != NO_SETTINGS && mCurrentSpace != NO_ZONES);						// Height-mapped parcel WL

	if (!has_override)
	{
		LL_INFOS() << "Region environment settings received. Parcel WL settings will be overridden. Reason: No \"RegionOverride\", region not using default WL and no zones defined or Parcel WL settings received - Region settings taking precedence" << LL_ENDL;
	}
	else
	{
		LL_INFOS() << "Parcel WL override active" << LL_ENDL;
		LL_DEBUGS() << "mCurrentSpace == NO_ZONES && mUsingParcelWLSkyDefault && mHasRegionOverride = " << ((mCurrentSpace == NO_ZONES && mUsingParcelWLSkyDefault && mHasRegionOverride) ? "true" : "false") << " - "
			<< "mCurrentSpace == NO_ZONES && !mHaveRegionSettings = " << ((mCurrentSpace == NO_ZONES && !mHaveRegionSettings) ? "true" : "false") << " - "
			<< "mCurrentSpace != NO_SETTINGS && mCurrentSpace != NO_ZONES = " << ((mCurrentSpace != NO_SETTINGS && mCurrentSpace != NO_ZONES) ? "true" : "false")
			<< LL_ENDL;
	}

	return has_override;
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
			mCurrentSpace = NO_SETTINGS;
			mLastParcelID = -1;
			mHasRegionOverride = false;
			mHaveRegionSettings = false;
			mLastRegion = NULL;
			mEventTimer.stop();
			setWL_Status(false);
			mDisabled = true;
			mUsingParcelWLSkyDefault = false;
		}
		return true;
	}
	mDisabled = false;
	return false;
}

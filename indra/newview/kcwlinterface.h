/** 
 * @file kcwlinterface.h
 * @brief Windlight Interface
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

#ifndef KC_WLINTERFACE_H
#define KC_WLINTERFACE_H

#include "llviewerprecompiledheaders.h"
#include "llsingleton.h"
#include "lleventtimer.h"
#include "llnotifications.h"
#include "lluuid.h"
#include <set>
#include <string>

class LLParcel;
class LLViewerRegion;
class LLEnvironmentSettings;

class KCWindlightInterface : public LLSingleton<KCWindlightInterface>,LLEventTimer
{
public:
	KCWindlightInterface();
	void ParcelChange();
	virtual BOOL tick();
	void ApplySettings(const LLSD& settings);
	void ApplySkySettings(const LLSD& settings);
	void ApplyWindLightPreset(const std::string& preset);
	void ResetToRegion(bool force = false);
	//bool ChatCommand(std::string message, std::string from_name, LLUUID source_id, LLUUID owner_id);
	bool LoadFromPacel(LLParcel *parcel);
	bool ParsePacelForWLSettings(const std::string& desc, LLSD& settings);
	void onClickWLStatusButton();
	void setTPing(bool value) { mTPing = value; }
	bool haveParcelOverride(const LLEnvironmentSettings& new_settings);
	
	bool getWLset() { return mWLset; }
	
private:
	bool callbackParcelWL(const LLSD& notification, const LLSD& response);
	bool callbackParcelWLClear(const LLSD& notification, const LLSD& response);
	bool AllowedLandOwners(const LLUUID& agent_id);
	LLUUID getOwnerID(LLParcel *parcel);
	std::string getOwnerName(LLParcel *parcel);
	void setWL_Status(bool pwl_status);
	bool checkSettings();

protected:
	bool mWLset; //display the status bar icon?
	std::set<LLUUID> mAllowedLand;
	LLNotificationPtr mSetWLNotification;
	LLNotificationPtr mClearWLNotification;
	S32 mLastParcelID;
	std::string mLastParcelDesc; //used to check if its changed
	S32 mCurrentSpace; //we use the lower bound of a space as its id
	LLSD mCurrentSettings;
	S32 mLastZ;
	bool mWeChangedIt; //dont reset anything if we didnt do it
	bool mTPing; //agent just TP'd (hack, might be better way)
	LLViewerRegion* mLastRegion;
	bool mRegionOverride;
	bool mHaveRegionSettings;
	bool mDisabled; // control bool to clear all states after being disabled
};

#endif // KC_WLINTERFACE_H
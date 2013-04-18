/**
 * @file fsradar.h
 * @brief Firestorm radar implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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

#ifndef FS_RADAR_H
#define FS_RADAR_H

#include "llsingleton.h"

#include "fsradarentry.h"
#include <boost/unordered_map.hpp>

class LLAvatarName;

const U32	FSRADAR_MAX_AVATARS_PER_ALERT = 6;	// maximum number of UUIDs we can cram into a single channel radar alert message
const U32	FSRADAR_COARSE_OFFSET_INTERVAL = 7;	// seconds after which we query the bridge for a coarse location adjustment
const U32	FSRADAR_MAX_OFFSET_REQUESTS = 60;	// 2048 / UUID size, leaving overhead space
const U32	FSRADAR_CHAT_MIN_SPACING = 6;		// minimum delay between radar chat messages

typedef enum e_radar_name_format
{
	FSRADAR_NAMEFORMAT_DISPLAYNAME,
	FSRADAR_NAMEFORMAT_USERNAME,
	FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME,
	FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME
} ERadarNameFormat;


class FSRadar 
	: public LLSingleton<FSRadar>
{
	LOG_CLASS(FSRadar);

friend class LLSingleton<FSRadar>;

public:
	typedef boost::unordered_map<const LLUUID, FSRadarEntry*, FSUUIDHash> entry_map_t;
	entry_map_t getRadarList() { return mEntryList; }

	void startTracking(const LLUUID& avatar_id);
	void zoomAvatar(const LLUUID& avatar_id, const std::string& name);
	void teleportToAvatar(const LLUUID& targetAv);
	void requestRadarChannelAlertSync();
	void updateNames();

	static void	onRadarNameFmtClicked(const LLSD& userdata);
	static bool	radarNameFmtCheck(const LLSD& userdata);
	static void	onRadarReportToClicked(const LLSD& userdata);
	static bool	radarReportToCheck(const LLSD& userdata);

	void getCurrentData(std::vector<LLSD>& entries, LLSD& stats) const { entries = mRadarEntriesData; stats = mAvatarStats; }
	FSRadarEntry* getEntry(const LLUUID& avatar_id);

	// internals
	class Updater
	{
	public:
		typedef boost::function<void()> callback_t;
		Updater(callback_t cb)
		: mCallback(cb)
		{ }

		virtual ~Updater()
		{ }

	protected:
		void update()
		{
			mCallback();
		}

		callback_t		mCallback;
	};

	typedef boost::signals2::signal<void(const std::vector<LLSD>& entries, const LLSD& stats)> radar_update_callback_t;
	boost::signals2::connection setUpdateCallback(const radar_update_callback_t::slot_type& cb)
	{
		return mUpdateSignal.connect(cb);
	}

private:
	FSRadar();
	virtual ~FSRadar();

	void					updateRadarList();
	void					updateTracking();
	void					checkTracking();
	void					radarAlertMsg(const LLUUID& agent_id, const LLAvatarName& av_name, const std::string& postMsg);

	Updater*				mRadarListUpdater;
	
	struct radarFields 
	{
		std::string avName;
		F32			lastDistance;
		LLVector3d	lastGlobalPos;
		LLUUID		lastRegion;
		time_t		firstSeen;
		S32			lastStatus;
		U32			ZOffset;
		time_t		lastZOffsetTime;
	};

	entry_map_t				mEntryList;

	std::multimap < LLUUID, radarFields > lastRadarSweep;
	std::vector <LLUUID>	mRadarEnterAlerts;
	std::vector <LLUUID>	mRadarLeaveAlerts;
	std::vector <LLUUID>	mRadarOffsetRequests;
	std::vector <LLSD>		mRadarEntriesData;
	 	
	S32						mRadarFrameCount;
	bool					mRadarAlertRequest;
	F32						mRadarLastRequestTime;
	U32						mRadarLastBulkOffsetRequestTime;

	LLUUID					mTrackedAvatarId;
	LLSD					mAvatarStats;

	radar_update_callback_t mUpdateSignal;
};

#endif // FS_RADAR_H

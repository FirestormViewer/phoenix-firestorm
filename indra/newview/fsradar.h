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
#include <unordered_map>

class LLAvatarName;

constexpr U32 FSRADAR_MAX_AVATARS_PER_ALERT{ 6 };   // maximum number of UUIDs we can cram into a single channel radar alert message
constexpr U32 FSRADAR_COARSE_OFFSET_INTERVAL{ 7 };  // seconds after which we query the bridge for a coarse location adjustment
constexpr U32 FSRADAR_MAX_OFFSET_REQUESTS{ 60 };    // 2048 / UUID size, leaving overhead space
constexpr U32 FSRADAR_CHAT_MIN_SPACING{ 6 };        // minimum delay between radar chat messages

typedef enum e_radar_name_format
{
    FSRADAR_NAMEFORMAT_DISPLAYNAME,
    FSRADAR_NAMEFORMAT_USERNAME,
    FSRADAR_NAMEFORMAT_DISPLAYNAME_USERNAME,
    FSRADAR_NAMEFORMAT_USERNAME_DISPLAYNAME
} ERadarNameFormat;

typedef enum e_radar_payment_info_flag
{
    FSRADAR_PAYMENT_INFO_NONE,
    FSRADAR_PAYMENT_INFO_FILLED,
    FSRADAR_PAYMENT_INFO_USED
} ERadarPaymentInfoFlag;

class FSRadar
    : public LLSingleton<FSRadar>
{
    LOG_CLASS(FSRadar);

    LLSINGLETON(FSRadar);
    virtual ~FSRadar();

public:
    typedef std::unordered_map<LLUUID, std::shared_ptr<FSRadarEntry>> entry_map_t;
    entry_map_t getRadarList() { return mEntryList; }

    void startTracking(const LLUUID& avatar_id);
    void zoomAvatar(const LLUUID& avatar_id, std::string_view name);
    void teleportToAvatar(const LLUUID& targetAv);
    void requestRadarChannelAlertSync();
    void updateNames();
    void updateName(const LLUUID& avatar_id);
    void updateNotes(const LLUUID& avatar_id, std::string_view notes);

    static void onRadarNameFmtClicked(const LLSD& userdata);
    static bool radarNameFmtCheck(const LLSD& userdata);
    static void onRadarReportToClicked(const LLSD& userdata);
    static bool radarReportToCheck(const LLSD& userdata);

    void getCurrentData(std::vector<LLSD>& entries, LLSD& stats) const { entries = mRadarEntriesData; stats = mAvatarStats; }
    std::shared_ptr<FSRadarEntry> getEntry(const LLUUID& avatar_id);

    // internals
    class Updater
    {
    public:
        typedef std::function<void()> callback_t;
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

        callback_t mCallback;
    };

    typedef boost::signals2::signal<void(const std::vector<LLSD>& entries, const LLSD& stats)> radar_update_callback_t;
    boost::signals2::connection setUpdateCallback(const radar_update_callback_t::slot_type& cb)
    {
        return mUpdateSignal.connect(cb);
    }

protected:
    void initSingleton() override;

private:
    void updateRadarList();
    void updateTracking();
    void checkTracking();
    void radarAlertMsg(const LLUUID& agent_id, const LLAvatarName& av_name, std::string_view postMsg);
    void updateAgeAlertCheck();

    void onRegionChanged();

    std::unique_ptr<Updater> mRadarListUpdater;

    struct RadarFields
    {
        F32         lastDistance;
        LLUUID      lastRegion;
        bool        lastIgnore;
    };

    typedef std::unordered_map<LLUUID, RadarFields> radarfields_map_t;
    radarfields_map_t       mLastRadarSweep;
    entry_map_t             mEntryList;

    uuid_vec_t              mRadarEnterAlerts;
    uuid_vec_t              mRadarLeaveAlerts;
    uuid_vec_t              mRadarOffsetRequests;
    std::vector<LLSD>       mRadarEntriesData;

    S32                     mRadarFrameCount;
    bool                    mRadarAlertRequest;
    F32                     mRadarLastRequestTime;
    U32                     mRadarLastBulkOffsetRequestTime;

    LLUUID                  mTrackedAvatarId;
    LLSD                    mAvatarStats;

    radar_update_callback_t mUpdateSignal;

    boost::signals2::connection mShowUsernamesCallbackConnection;
    boost::signals2::connection mNameFormatCallbackConnection;
    boost::signals2::connection mAgeAlertCallbackConnection;

    boost::signals2::connection mRegionCapabilitiesReceivedCallbackConnection;
    boost::signals2::connection mRegionChangedCallbackConnection;
};

#endif // FS_RADAR_H

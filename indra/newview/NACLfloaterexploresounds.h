/**
 * @file NACLfloaterexploresounds.h
 */

#ifndef LL_LLFLOATEREXPLORESOUNDS_H
#define LL_LLFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llaudioengine.h"
#include "llavatarnamecache.h"
#include "fsassetblacklist.h"

class LLCheckBoxCtrl;
class LLScrollListCtrl;

class NACLFloaterExploreSounds
: public LLFloater, public LLEventTimer
{
public:
    NACLFloaterExploreSounds(const LLSD& key);
    bool postBuild() override;

    bool tick() override;

    LLSoundHistoryItem getItem(const LLUUID& itemID) const;

private:
    virtual ~NACLFloaterExploreSounds();
    void handlePlayLocally();
    void handleLookAt();
    void handleStop();
    void handleStopLocally();
    void handleSelection();
    void blacklistSound(FSAssetBlacklist::eBlacklistFlag flag);

    LLScrollListCtrl* mHistoryScroller{ nullptr };
    LLCheckBoxCtrl*   mCollisionSounds{ nullptr };
    LLCheckBoxCtrl*   mRepeatedAssets{ nullptr };
    LLCheckBoxCtrl*   mAvatarSounds{ nullptr };
    LLCheckBoxCtrl*   mObjectSounds{ nullptr };
    LLCheckBoxCtrl*   mPaused{ nullptr };

    std::list<LLSoundHistoryItem> mLastHistory;

    uuid_vec_t mLocalPlayingAudioSourceIDs;

    typedef std::map<LLUUID, boost::signals2::connection> blacklist_avatar_name_cache_connection_map_t;
    blacklist_avatar_name_cache_connection_map_t mBlacklistAvatarNameCacheConnections;

    void onBlacklistAvatarNameCacheCallback(const LLUUID& request_id, const LLUUID& av_id, const LLAvatarName& av_name, const LLUUID& asset_id, const std::string& region_name, FSAssetBlacklist::eBlacklistFlag flag);
};

#endif

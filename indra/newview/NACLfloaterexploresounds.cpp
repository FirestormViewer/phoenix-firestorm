/**
 * @file NACLfloaterexploresounds.cpp
 */

#include "llviewerprecompiledheaders.h"

#include "NACLfloaterexploresounds.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarnamecache.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "fsassetblacklist.h"
#include "fscommon.h"
#include "rlvhandler.h"

constexpr size_t num_collision_sounds = 28;
const LLUUID collision_sounds[num_collision_sounds] =
{
    LLUUID("dce5fdd4-afe4-4ea1-822f-dd52cac46b08"),
    LLUUID("51011582-fbca-4580-ae9e-1a5593f094ec"),
    LLUUID("68d62208-e257-4d0c-bbe2-20c9ea9760bb"),
    LLUUID("75872e8c-bc39-451b-9b0b-042d7ba36cba"),
    LLUUID("6a45ba0b-5775-4ea8-8513-26008a17f873"),
    LLUUID("992a6d1b-8c77-40e0-9495-4098ce539694"),
    LLUUID("2de4da5a-faf8-46be-bac6-c4d74f1e5767"),
    LLUUID("6e3fb0f7-6d9c-42ca-b86b-1122ff562d7d"),
    LLUUID("14209133-4961-4acc-9649-53fc38ee1667"),
    LLUUID("bc4a4348-cfcc-4e5e-908e-8a52a8915fe6"),
    LLUUID("9e5c1297-6eed-40c0-825a-d9bcd86e3193"),
    LLUUID("e534761c-1894-4b61-b20c-658a6fb68157"),
    LLUUID("8761f73f-6cf9-4186-8aaa-0948ed002db1"),
    LLUUID("874a26fd-142f-4173-8c5b-890cd846c74d"),
    LLUUID("0e24a717-b97e-4b77-9c94-b59a5a88b2da"),
    LLUUID("75cf3ade-9a5b-4c4d-bb35-f9799bda7fb2"),
    LLUUID("153c8bf7-fb89-4d89-b263-47e58b1b4774"),
    LLUUID("55c3e0ce-275a-46fa-82ff-e0465f5e8703"),
    LLUUID("24babf58-7156-4841-9a3f-761bdbb8e237"),
    LLUUID("aca261d8-e145-4610-9e20-9eff990f2c12"),
    LLUUID("0642fba6-5dcf-4d62-8e7b-94dbb529d117"),
    LLUUID("25a863e8-dc42-4e8a-a357-e76422ace9b5"),
    LLUUID("9538f37c-456e-4047-81be-6435045608d4"),
    LLUUID("8c0f84c3-9afd-4396-b5f5-9bca2c911c20"),
    LLUUID("be582e5d-b123-41a2-a150-454c39e961c8"),
    LLUUID("c70141d4-ba06-41ea-bcbc-35ea81cb8335"),
    LLUUID("7d1826f4-24c4-4aac-8c2e-eff45df37783"),
    LLUUID("063c97d3-033a-4e9b-98d8-05c8074922cb")
};

NACLFloaterExploreSounds::NACLFloaterExploreSounds(const LLSD& key)
:   LLFloater(key), LLEventTimer(0.25f)
{
}

NACLFloaterExploreSounds::~NACLFloaterExploreSounds()
{
    for (blacklist_avatar_name_cache_connection_map_t::iterator it = mBlacklistAvatarNameCacheConnections.begin(); it != mBlacklistAvatarNameCacheConnections.end(); ++it)
    {
        if (it->second.connected())
        {
            it->second.disconnect();
        }
    }
    mBlacklistAvatarNameCacheConnections.clear();

    mLocalPlayingAudioSourceIDs.clear();
}

bool NACLFloaterExploreSounds::postBuild()
{
    getChild<LLButton>("play_locally_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::handlePlayLocally, this));
    getChild<LLButton>("look_at_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::handleLookAt, this));
    getChild<LLButton>("stop_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::handleStop, this));
    getChild<LLButton>("bl_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::blacklistSound, this, FSAssetBlacklist::eBlacklistFlag::NONE));
    getChild<LLButton>("stop_locally_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::handleStopLocally, this));
    getChild<LLButton>("block_avatar_worn_sounds_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::blacklistSound, this, FSAssetBlacklist::eBlacklistFlag::WORN));
    getChild<LLButton>("block_avatar_rezzed_sounds_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::blacklistSound, this, FSAssetBlacklist::eBlacklistFlag::REZZED));
    getChild<LLButton>("block_avatar_gesture_sounds_btn")->setClickedCallback(boost::bind(&NACLFloaterExploreSounds::blacklistSound, this, FSAssetBlacklist::eBlacklistFlag::GESTURE));

    mHistoryScroller = getChild<LLScrollListCtrl>("sound_list");
    mHistoryScroller->setCommitCallback(boost::bind(&NACLFloaterExploreSounds::handleSelection, this));
    mHistoryScroller->setDoubleClickCallback(boost::bind(&NACLFloaterExploreSounds::handlePlayLocally, this));
    mHistoryScroller->sortByColumn("playing", true);

    mCollisionSounds = getChild<LLCheckBoxCtrl>("collision_chk");
    mRepeatedAssets = getChild<LLCheckBoxCtrl>("repeated_asset_chk");
    mAvatarSounds = getChild<LLCheckBoxCtrl>("avatars_chk");
    mObjectSounds = getChild<LLCheckBoxCtrl>("objects_chk");
    mPaused = getChild<LLCheckBoxCtrl>("pause_chk");

    return true;
}

void NACLFloaterExploreSounds::handleSelection()
{
    size_t num_selected = mHistoryScroller->getAllSelected().size();
    bool multiple = (num_selected > 1);
    childSetEnabled("look_at_btn", (num_selected && !multiple));
    childSetEnabled("play_locally_btn", num_selected);
    childSetEnabled("stop_btn", num_selected);
    childSetEnabled("bl_btn", num_selected);
    childSetEnabled("block_avatar_worn_sounds_btn", num_selected);
    childSetEnabled("block_avatar_rezzed_sounds_btn", num_selected);
    childSetEnabled("block_avatar_gesture_sounds_btn", num_selected);
}

LLSoundHistoryItem NACLFloaterExploreSounds::getItem(const LLUUID& itemID) const
{
    if (auto found = gSoundHistory.find(itemID); found != gSoundHistory.end())
    {
        return found->second;
    }
    else
    {
        // If log is paused, hopefully we can find it in mLastHistory
        if (auto foundHistory = std::find_if(mLastHistory.begin(), mLastHistory.end(), [&](const auto& item) { return item.mID == itemID; });
            foundHistory != mLastHistory.end())
            return *foundHistory;
    }

    return {};
}

class LLSoundHistoryItemCompare
{
public:
    bool operator() (LLSoundHistoryItem first, LLSoundHistoryItem second)
    {
        if (first.mPlaying)
        {
            if (second.mPlaying)
            {
                return (first.mTimeStarted > second.mTimeStarted);
            }
            else
            {
                return true;
            }
        }
        else if (second.mPlaying)
        {
            return false;
        }
        else
        {
            return (first.mTimeStopped > second.mTimeStopped);
        }
    }
};

bool NACLFloaterExploreSounds::tick()
{
    static const std::string str_playing =  getString("Playing");
    static const std::string str_not_playing = getString("NotPlaying");
    static const std::string str_type_ui =  getString("Type_UI");
    static const std::string str_type_avatar = getString("Type_Avatar");
    static const std::string str_type_trigger_sound = getString("Type_llTriggerSound");
    static const std::string str_type_loop_sound = getString("Type_llLoopSound");
    static const std::string str_type_play_sound = getString("Type_llPlaySound");
    static const std::string str_unknown_name = LLTrans::getString("AvatarNameWaiting");

    bool show_collision_sounds = mCollisionSounds->get();
    bool show_repeated_assets = mRepeatedAssets->get();
    bool show_avatars = mAvatarSounds->get();
    bool show_objects = mObjectSounds->get();

    std::list<LLSoundHistoryItem> history;
    if (mPaused->get())
    {
        history = mLastHistory;
    }
    else
    {
        for (const auto& [id, item] : gSoundHistory)
        {
            history.emplace_back(item);
        }
        LLSoundHistoryItemCompare c;
        history.sort(c);
        mLastHistory = history;
    }

    // Save scroll pos and selection so they can be restored
    S32 scroll_pos = mHistoryScroller->getScrollPos();
    uuid_vec_t selected_ids;
    for (const auto* item : mHistoryScroller->getAllSelected())
    {
        selected_ids.emplace_back(item->getUUID());
    }

    mHistoryScroller->clearRows();

    std::unordered_set<LLUUID> unique_asset_list;
    for (auto& item : history)
    {
        bool is_avatar = item.mOwnerID == item.mSourceID;
        if (is_avatar && !show_avatars)
        {
            continue;
        }

        bool is_object = !is_avatar;
        if (is_object && !show_objects)
        {
            continue;
        }

        bool is_repeated_asset = unique_asset_list.contains(item.mAssetID);
        if (is_repeated_asset && !show_repeated_assets)
        {
            continue;
        }

        if (!item.mReviewed)
        {
            item.mReviewedCollision = std::find(&collision_sounds[0], &collision_sounds[num_collision_sounds], item.mAssetID) != &collision_sounds[num_collision_sounds];
            item.mReviewed = true;
        }

        bool is_collision_sound = item.mReviewedCollision;
        if (is_collision_sound && !show_collision_sounds)
        {
            continue;
        }

        unique_asset_list.emplace(item.mAssetID);

        LLSD element;
        element["id"] = item.mID;

        LLSD& playing_column = element["columns"][0];
        playing_column["column"] = "playing";
        if (item.mPlaying)
        {
            playing_column["value"] = " " + str_playing;
        }
        else
        {
            LLStringUtil::format_map_t format_args;
            format_args["TIME"] = llformat("%.1f", static_cast<F32>((LLTimer::getElapsedSeconds() - item.mTimeStopped) / 60.0));
            playing_column["value"] = FSCommon::format_string(str_not_playing, format_args);
        }

        LLSD& type_column = element["columns"][1];
        type_column["column"] = "type";
        if (item.mType == LLAudioEngine::AUDIO_TYPE_UI)
        {
            // this shouldn't happen for now, as UI is forbidden in the log
            type_column["value"] = str_type_ui;
        }
        else
        {
            std::string type;

            if (is_avatar)
            {
                type = str_type_avatar;
            }
            else
            {
                if (item.mIsTrigger)
                {
                    type = str_type_trigger_sound;
                }
                else
                {
                    if (item.mIsLooped)
                    {
                        type = str_type_loop_sound;
                    }
                    else
                    {
                        type = str_type_play_sound;
                    }
                }
            }

            type_column["value"] = type;
        }

        LLSD& owner_column = element["columns"][2];
        owner_column["column"] = "owner";
        LLAvatarName av_name;
        if (LLAvatarNameCache::get(item.mOwnerID, &av_name))
        {
            owner_column["value"] = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name);
        }
        else
        {
            owner_column["value"] = str_unknown_name;
        }

        LLSD& sound_column = element["columns"][3];
        sound_column["column"] = "sound";
        sound_column["value"] = item.mAssetID.asString().substr(0,16);

        mHistoryScroller->addElement(element, ADD_BOTTOM);
    }

    mHistoryScroller->selectMultiple(selected_ids);
    mHistoryScroller->setScrollPos(scroll_pos);

    // Clean up stopped local audio source IDs
    uuid_vec_t::iterator audio_src_id_iter = mLocalPlayingAudioSourceIDs.begin();
    while (audio_src_id_iter != mLocalPlayingAudioSourceIDs.end())
    {
        const LLUUID& audio_src_id = *audio_src_id_iter;
        if (LLAudioSource* audio_source = gAudiop->findAudioSource(audio_src_id); !audio_source || audio_source->isDone())
        {
            audio_src_id_iter = mLocalPlayingAudioSourceIDs.erase(audio_src_id_iter);
        }
        else
        {
            audio_src_id_iter++;
        }
    }

    childSetEnabled("stop_locally_btn", mLocalPlayingAudioSourceIDs.size() > 0);

    return false;
}

void NACLFloaterExploreSounds::handlePlayLocally()
{
    std::unordered_set<LLUUID> asset_list;
    for (const auto* selected_item : mHistoryScroller->getAllSelected())
    {
        LLSoundHistoryItem item = getItem(selected_item->getValue());
        if (item.mID.isNull())
        {
            continue;
        }

        // Unique assets only
        if (!asset_list.contains(item.mAssetID))
        {
            asset_list.emplace(item.mAssetID);
            LLUUID audio_source_id = LLUUID::generateNewID();
            gAudiop->triggerSound(item.mAssetID, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI, LLVector3d::zero, LLUUID::null, audio_source_id);
            mLocalPlayingAudioSourceIDs.emplace_back(audio_source_id);
        }
    }

    childSetEnabled("stop_locally_btn", !mLocalPlayingAudioSourceIDs.empty());
}

void NACLFloaterExploreSounds::handleLookAt()
{
    LLUUID selection = mHistoryScroller->getSelectedValue().asUUID();
    LLSoundHistoryItem item = getItem(selection); // Single item only
    if (item.mID.isNull())
    {
        return;
    }

    LLVector3d pos_global = item.mPosition;

    // Try to find object position
    if (item.mSourceID.notNull())
    {
        LLViewerObject* object = gObjectList.findObject(item.mSourceID);
        if (object)
        {
            pos_global = object->getPositionGlobal();
        }
    }

    // Move the camera
    // Find direction to self (reverse)
    LLVector3d cam = gAgent.getPositionGlobal() - pos_global;
    cam.normalize();
    // Go 4 meters back and 3 meters up
    cam *= 4.0;
    cam += pos_global;
    cam += LLVector3d(0.0, 0.0, 3.0);

    gAgentCamera.setFocusOnAvatar(false, false);
    gAgentCamera.setCameraPosAndFocusGlobal(cam, pos_global, item.mSourceID);
    gAgentCamera.setCameraAnimating(false);
}

void NACLFloaterExploreSounds::handleStop()
{
    for (const auto& selection_item : mHistoryScroller->getAllSelected())
    {
        LLSoundHistoryItem item = getItem(selection_item->getValue());
        if (item.mID.notNull() && item.mPlaying)
        {
            if (LLAudioSource* audio_source = gAudiop->findAudioSource(item.mSourceID))
            {
                S32 type = item.mType;
                audio_source->setType(LLAudioEngine::AUDIO_TYPE_UI);
                audio_source->play(LLUUID::null);
                audio_source->setType(type);
            }
            else
            {
                LL_WARNS("SoundExplorer") << "audio source for source ID " << item.mSourceID << " already gone but still marked as playing. Fixing ..." << LL_ENDL;
                if (gSoundHistory.find(item.mID) != gSoundHistory.end())
                {
                    gSoundHistory[item.mID].mPlaying = false;
                    gSoundHistory[item.mID].mTimeStopped = LLTimer::getElapsedSeconds();
                }
                else
                {
                    for (auto& histItem : mLastHistory)
                    {
                        if (histItem.mID == item.mID)
                        {
                            histItem.mPlaying = false;
                            histItem.mTimeStopped = LLTimer::getElapsedSeconds();
                            break;
                        }
                    }
                }
                continue;
            }
        }
    }
}

void NACLFloaterExploreSounds::handleStopLocally()
{
    for (const auto& audio_source_id : mLocalPlayingAudioSourceIDs)
    {
        if (LLAudioSource* audio_source = gAudiop->findAudioSource(audio_source_id); audio_source && !audio_source->isDone())
        {
            audio_source->play(LLUUID::null);
        }
    }

    mLocalPlayingAudioSourceIDs.clear();
}

//add sound to blacklist
void NACLFloaterExploreSounds::blacklistSound(FSAssetBlacklist::eBlacklistFlag flag)
{
    for (const auto* selected_item : mHistoryScroller->getAllSelected())
    {
        LLSoundHistoryItem item = getItem(selected_item->getValue());
        if (item.mID.isNull())
        {
            continue;
        }

        std::string region_name;
        if (LLViewerRegion* cur_region = gAgent.getRegion())
        {
            region_name = cur_region->getName();
        }

        if (LLAvatarName av_name; LLAvatarNameCache::get(item.mOwnerID, &av_name))
        {
            FSAssetBlacklist::getInstance()->addNewItemToBlacklist(flag == FSAssetBlacklist::eBlacklistFlag::NONE ? item.mAssetID : item.mOwnerID, av_name.getCompleteName(), region_name, LLAssetType::AT_SOUND, flag);
        }
        else
        {
            // Create unique UUID here instead of avatar UUID because we might be blacklisting more than one sound of the same user
            LLUUID requestId = LLUUID::generateNewID();
            mBlacklistAvatarNameCacheConnections.try_emplace(requestId, LLAvatarNameCache::get(item.mOwnerID, boost::bind(&NACLFloaterExploreSounds::onBlacklistAvatarNameCacheCallback, this, requestId, _1, _2, item.mAssetID, region_name, flag)));
        }
    }
}

void NACLFloaterExploreSounds::onBlacklistAvatarNameCacheCallback(const LLUUID& request_id, const LLUUID& av_id, const LLAvatarName& av_name, const LLUUID& asset_id, const std::string& region_name, FSAssetBlacklist::eBlacklistFlag flag)
{
    if (auto found = mBlacklistAvatarNameCacheConnections.find(request_id); found != mBlacklistAvatarNameCacheConnections.end())
    {
        if (found->second.connected())
        {
            found->second.disconnect();
        }
        mBlacklistAvatarNameCacheConnections.erase(found);
    }
    FSAssetBlacklist::getInstance()->addNewItemToBlacklist(flag == FSAssetBlacklist::eBlacklistFlag::NONE ? asset_id : av_id, av_name.getCompleteName(), region_name, LLAssetType::AT_SOUND, flag);
}

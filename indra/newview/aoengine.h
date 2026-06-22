/**
 * @file aoengine.h
 * @brief The core Animation Overrider engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#ifndef AOENGINE_H
#define AOENGINE_H

#include "aoset.h"

#include "llassettype.h"
#include "lleventtimer.h"
#include "llextendedstatus.h"
#include "llsingleton.h"
#include "llviewerinventory.h"

#include <map>
#include <memory>

class LLInventoryCategory;

class AOTimerCollection : public LLEventTimer
{
public:
    AOTimerCollection();
    ~AOTimerCollection() = default;

    bool tick() override;

    void enableInventoryTimer(bool enable);
    void enableSettingsTimer(bool enable);
    void enableReloadTimer(bool enable);
    void enableImportTimer(bool enable);

protected:
    void updateTimers();

    bool mInventoryTimer;
    bool mSettingsTimer;
    bool mReloadTimer;
    bool mImportTimer;
};

// ----------------------------------------------------

class AOSitCancelTimer : public LLEventTimer
{
public:
    AOSitCancelTimer();
    ~AOSitCancelTimer() = default;

    void oneShot();
    void stop();

    bool tick() override;

protected:
    S32 mTickCount;
};

// ----------------------------------------------------


// timer cycles the state's step list instead of one of its tracks - used for the continuously playing "Always" state
constexpr S32 AO_TRACK_INDEX_STEPS = -1;

class AOTrackTimer : public LLEventTimer
{
public:
    AOTrackTimer(S32 stateEnum, S32 trackIndex, F32 period);
    ~AOTrackTimer() = default;

    bool tick() override;

protected:
    S32 mStateEnum;
    S32 mTrackIndex;
};

class AOEngine : public LLSingleton<AOEngine>
{
    LLSINGLETON(AOEngine);
    ~AOEngine();

public:
    enum eCycleMode
    {
        CycleAny,
        CycleNext,
        CyclePrevious
    };

    void   enable(bool enable);
    void   enableStands(bool enable_stands);
    LLUUID override(const LLUUID& motion, bool start);
    void   tick();
    void   update();
    void   reload(bool fromTimer);
    bool   reloadStateAnimations(AOSet* set, AOSet::AOState* state);
    void   clear(bool fromTimer);

    const LLUUID& getAOFolder() const;

    void addSet(const std::string& name, inventory_func_type callback, bool reload = true);
    bool cloneSet(AOSet* sourceSet, std::string_view newName);
    bool removeSet(AOSet* set);

    void addAnimation(const AOSet* set, AOSet::AOState* state, const LLInventoryItem* item, bool reload = true);
    bool removeAnimation(const AOSet* set, AOSet::AOState* state, S32 stepIndex, S32 memberIndex = -1);
    void checkSitCancel();
    void checkBelowWater(bool check_underwater);

    bool importNotecard(const LLInventoryItem* item);
    void processImport(bool from_timer);

    bool swapWithPrevious(AOSet::AOState* state, S32 stepIndex, S32 memberIndex = -1);
    bool swapWithNext(AOSet::AOState* state, S32 stepIndex, S32 memberIndex = -1);

    void cycleTimeout(const AOSet* set);
    void cycle(eCycleMode cycleMode, bool resetTimer = false);

    void addAnimationToGroup(AOSet::AOState* state, S32 stepIndex, const LLInventoryItem* item);
    void createGroupFromMerge(AOSet::AOState* state, S32 stepIndex, const LLInventoryItem* item);
    bool createGroupFromFolder(AOSet::AOState* state, const LLInventoryCategory* category);
    void extractMemberFromGroup(AOSet::AOState* state, S32 stepIndex, S32 memberIndex);
    bool renameGroup(AOSet::AOState* state, S32 stepIndex, std::string_view newName);

    std::string createEmptyTrack(AOSet::AOState* state);
    void addAnimationToTrack(AOSet::AOState* state, S32 trackIndex, const LLInventoryItem* item);
    void removeTrack(AOSet::AOState* state, S32 trackIndex);
    void removeTrackAnimation(AOSet::AOState* state, S32 trackIndex, S32 memberIndex);
    bool swapTrackAnimationWithPrevious(AOSet::AOState* state, S32 trackIndex, S32 memberIndex);
    bool swapTrackAnimationWithNext(AOSet::AOState* state, S32 trackIndex, S32 memberIndex);
    void setTrackCycle(AOSet::AOState* state, S32 trackIndex, bool cycle);
    void setTrackRandomize(AOSet::AOState* state, S32 trackIndex, bool randomize);
    void setTrackCycleTime(AOSet::AOState* state, S32 trackIndex, F32 time);
    void playTrackAnimation(AOSet::AOState* state, S32 trackIndex, S32 memberIndex);
    void trackTimeout(S32 stateEnum, S32 trackIndex);

    void playAlwaysAnimation(S32 stepIndex, S32 memberIndex = -1);

    void                  playAnimation(S32 stepIndex, S32 memberIndex = -1);
    const AOSet*          getCurrentSet() const;
    const AOSet::AOState* getCurrentState() const;

    void   inMouselook(bool mouselook);
    void   selectSet(AOSet* set);
    AOSet* selectSetByName(std::string_view name);
    AOSet* getSetByName(std::string_view name) const;

    // callback from LLAppViewer
    static void onLoginComplete();

    const std::vector<AOSet*> getSetList() const;
    const std::string         getCurrentSetName() const;
    const AOSet*              getDefaultSet() const;
    bool                      renameSet(AOSet* set, std::string_view name);

    void setDefaultSet(AOSet* set);
    void setOverrideSits(AOSet* set, bool override_sit);
    void setSmart(AOSet* set, bool smart);
    void setDisableMouselookStands(AOSet* set, bool disabled);
    void setCycle(AOSet::AOState* set, bool cycle);
    void setRandomize(AOSet::AOState* state, bool randomize);
    void setCycleTime(AOSet::AOState* state, F32 time);

    void saveSettings();

    typedef boost::signals2::signal<void()> updated_signal_t;
    boost::signals2::connection setReloadCallback(const updated_signal_t::slot_type& cb)
    {
        return mUpdatedSignal.connect(cb);
    };

    typedef boost::signals2::signal<void(const LLUUID&)> animation_changed_signal_t;
    boost::signals2::connection setAnimationChangedCallback(const animation_changed_signal_t::slot_type& cb)
    {
        return mAnimationChangedSignal.connect(cb);
    };

protected:
    void init();

    void setLastMotion(const LLUUID& motion);
    void setLastOverriddenMotion(const LLUUID& motion);
    void setStateCycleTimer(const AOSet::AOState* state);

    void stopAllStandVariants();
    void stopAllSitVariants();

    bool            foreignAnimations() const;
    AOSet::AOState* mapSwimming(const LLUUID& motion) const;
    AOSet::AOState* getStateForMotion(const LLUUID& motion) const;

    void updateSortOrder(AOSet::AOState* state);
    void saveSet(const AOSet* set);
    void saveState(const AOSet::AOState* state);

    std::string getSetFolderName(const AOSet* set) const;
    std::string getStateFolderName(const AOSet::AOState* state) const;

    void        stopCurrentAnimations(AOSet::AOState* state, bool skipFirst = false);
    bool        swapStateAnimations(AOSet::AOState* state, const uuid_vec_t& animations);
    void        startStateTracks(AOSet::AOState* state);
    void        stopActiveTracks();
    void        startAlwaysAnimations();
    void        stopAlwaysAnimations();
    void        refreshAlwaysTimers();
    void        cycleAlwaysStep();
    void        setItemDescription(const LLUUID& itemID, const std::string& description);
    void        updateMemberSortOrder(std::vector<AOSet::AOAnimation>& animations);
    void        collapseGroupToSingle(AOSet::AOState* state, AOSet::AOAnimationStep& step);
    void        saveGroup(const AOSet::AOAnimationStep& step);
    void        saveTrack(const AOSet::AOTrack& track);
    std::string getGroupFolderName(const AOSet::AOAnimationStep& step) const;
    std::string getTrackFolderName(const AOSet::AOTrack& track) const;
    std::string sanitiseFolderName(const std::string& name, const std::string& fallback) const;
    std::string nextGroupName(const AOSet::AOState* state) const;
    std::string nextTrackName(const AOSet::AOState* state) const;
    bool        readAnimationLinks(const LLUUID& categoryID, std::vector<AOSet::AOAnimation>& animations);
    void        processImportState(AOSet::AOState* state);

    bool createAnimationLink(const LLUUID& categoryID, const LLInventoryItem* item);
    bool findForeignItems(const LLUUID& uuid) const;
    void purgeFolder(const LLUUID& uuid) const;

    void onRegionChange();

    void onToggleAOControl();
    void onToggleAOStandsControl();
    void onPauseAO();

    static void onNotecardLoadComplete(const LLUUID& assetUUID, LLAssetType::EType type, void* userdata, S32 status, LLExtStat extStatus);
    void        parseNotecard(const char* buffer);

    void updatePersistedStateAnimations();

    updated_signal_t           mUpdatedSignal;
    animation_changed_signal_t mAnimationChangedSignal;

    AOTimerCollection mTimerCollection;
    AOSitCancelTimer  mSitCancelTimer;

    bool mEnabled;
    bool mEnabledStands;
    bool mInMouselook;
    bool mUnderWater;

    LLUUID mAOFolder;
    LLUUID mLastMotion;
    LLUUID mLastOverriddenMotion;
    LLUUID mTransitionId;

    // this motion will be ignored once in the overrider when stopping, fixes a case
    // where the AO doesn't correctly start up on login or when getting enabled manually
    LLUUID mIgnoreMotionStopOnce;

    std::vector<AOSet*> mSets;
    std::vector<AOSet*> mOldSets;
    AOSet*              mCurrentSet;
    AOSet*              mDefaultSet;

    AOSet*              mImportSet;
    std::vector<AOSet*> mOldImportSets;
    S32                 mImportRetryCount;

    // timers of the active state's cycling tracks, and the state
    // whose tracks are currently playing. The pointer stays valid because tracks get
    // stopped in clear() before any AOSet object goes away
    std::vector<std::unique_ptr<AOTrackTimer>> mActiveTrackTimers;
    AOSet::AOState*                            mActiveTrackState{ nullptr };

    // the "Always" pseudo state of the current set while it is playing, with its
    // own step cycle timer and track timers that survive motion changes
    std::vector<std::unique_ptr<AOTrackTimer>> mAlwaysTimers;
    AOSet::AOState*                            mActiveAlwaysState{ nullptr };

    // pending additions while a group subfolder is created asynchronously, keyed by
    // the merge target's link UUID - prevents duplicate folders on fast multi-drop
    std::map<LLUUID, std::vector<const LLInventoryItem*>> mPendingGroupMerges;

    boost::signals2::connection mRegionChangeConnection;
};

#endif // AOENGINE_H

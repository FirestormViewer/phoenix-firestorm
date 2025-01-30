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
#include <boost/signals2.hpp>

class AOTimerCollection
:   public LLEventTimer
{
    public:
        AOTimerCollection();
        ~AOTimerCollection();

        virtual bool tick();

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

class AOSitCancelTimer
:    public LLEventTimer
{
    public:
        AOSitCancelTimer();
        ~AOSitCancelTimer();

        void oneShot();
        void stop();

        virtual bool tick();

    protected:
        S32 mTickCount;
};

// ----------------------------------------------------

class AOState;
class LLInventoryItem;
class LLVFS;

class AOEngine
:   public LLSingleton<AOEngine>
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

        void enable(bool enable);
        void enableStands(bool enable_stands);
        const LLUUID override(const LLUUID& motion, bool start);
        void tick();
        void update();
        void reload(bool);
        void reloadStateAnimations(AOSet::AOState* state);
        void clear(bool from_timer);

        const LLUUID& getAOFolder() const;

        void addSet(const std::string& name, inventory_func_type callback, bool reload = true);
        bool removeSet(AOSet* set);

        void addAnimation(const AOSet* set, AOSet::AOState* state, const LLInventoryItem* item, bool reload = true);
        bool removeAnimation(const AOSet* set, AOSet::AOState* state, S32 index);
        void checkSitCancel();
        void checkBelowWater(bool check_underwater);

        bool importNotecard(const LLInventoryItem* item);
        void processImport(bool from_timer);

        bool swapWithPrevious(AOSet::AOState* state, S32 index);
        bool swapWithNext(AOSet::AOState* state, S32 index);

        void cycleTimeout(const AOSet* set);
        void cycle(eCycleMode cycleMode);

// <AS:Chanayane> Double click on animation in AO
        void playAnimation(const LLUUID& animation);
        const AOSet* getCurrentSet() const;
        const AOSet::AOState* getCurrentState() const;
// </AS:Chanayane>

        void inMouselook(bool mouselook);
        void selectSet(AOSet* set);
        AOSet* selectSetByName(const std::string& name);
        AOSet* getSetByName(const std::string& name) const;

        // callback from LLAppViewer
        static void onLoginComplete();

        const std::vector<AOSet*> getSetList() const;
        const std::string getCurrentSetName() const;
        const AOSet* getDefaultSet() const;
        bool renameSet(AOSet* set, const std::string& name);

        void setDefaultSet(AOSet* set);
        void setOverrideSits(AOSet* set, bool override_sit);
        void setSmart(AOSet* set, bool smart);
        void setDisableMouselookStands(AOSet* set, bool disabled);
        void setCycle(AOSet::AOState* set, bool cycle);
        void setRandomize(AOSet::AOState* state, bool randomize);
        void setCycleTime(AOSet::AOState* state, F32 time);

        void saveSettings();

        typedef boost::signals2::signal<void ()> updated_signal_t;
        boost::signals2::connection setReloadCallback(const updated_signal_t::slot_type& cb)
        {
            return mUpdatedSignal.connect(cb);
        };

        typedef boost::signals2::signal<void (const LLUUID&)> animation_changed_signal_t;
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

        bool foreignAnimations();
        AOSet::AOState* mapSwimming(const LLUUID& motion) const;
        AOSet::AOState* getStateForMotion(const LLUUID& motion) const;

        void updateSortOrder(AOSet::AOState* state);
        void saveSet(const AOSet* set);
        void saveState(const AOSet::AOState* state);

        bool createAnimationLink(AOSet::AOState* state, const LLInventoryItem* item);
        bool findForeignItems(const LLUUID& uuid) const;
        void purgeFolder(const LLUUID& uuid) const;

        void onRegionChange();

        void onToggleAOControl();
        void onToggleAOStandsControl();
        void onPauseAO();

        static void onNotecardLoadComplete(const LLUUID& assetUUID, LLAssetType::EType type,
                                                void* userdata, S32 status, LLExtStat extStatus);
        void parseNotecard(const char* buffer);

        updated_signal_t mUpdatedSignal;
        animation_changed_signal_t mAnimationChangedSignal;

        AOTimerCollection mTimerCollection;
        AOSitCancelTimer mSitCancelTimer;

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
        AOSet* mCurrentSet;
        AOSet* mDefaultSet;

        AOSet* mImportSet;
        std::vector<AOSet*> mOldImportSets;
        S32 mImportRetryCount;

        boost::signals2::connection mRegionChangeConnection;
};

#endif // AOENGINE_H

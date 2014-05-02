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

#include <boost/signals2.hpp>

#include "aoset.h"

#include "llassettype.h"
#include "lleventtimer.h"
#include "llsingleton.h"
#include "lluuid.h"

// NaCl - feex
#include "llextendedstatus.h"
// NaCl End

class AOTimerCollection
:	public LLEventTimer
{
	public:
		AOTimerCollection();
		~AOTimerCollection();

		virtual BOOL tick();

		void enableInventoryTimer(BOOL yes);
		void enableSettingsTimer(BOOL yes);
		void enableReloadTimer(BOOL yes);
		void enableImportTimer(BOOL yes);

	protected:
		void updateTimers();

		BOOL mInventoryTimer;
		BOOL mSettingsTimer;
		BOOL mReloadTimer;
		BOOL mImportTimer;
};

// ----------------------------------------------------

class AOSitCancelTimer
:	 public LLEventTimer
{
	public:
		AOSitCancelTimer();
		~AOSitCancelTimer();

		void oneShot();
		void stop();

		virtual BOOL tick();

	protected:
		S32 mTickCount;
};

// ----------------------------------------------------

class AOState;
class LLInventoryItem;
class LLVFS;

class AOEngine
:	public LLSingleton<AOEngine>
{
	friend class LLSingleton<AOEngine>;

	private:
		AOEngine();
		~AOEngine();

	public:
		enum eCycleMode
		{
			CycleAny,
			CycleNext,
			CyclePrevious
		};

		void enable(BOOL yes);
		const LLUUID override(const LLUUID& motion,BOOL start);
		void tick();
		void update();
		void reload(bool);
		void reloadStateAnimations(AOSet::AOState* state);
		void clear( bool );

		const LLUUID& getAOFolder() const;

		LLUUID addSet(const std::string& name,BOOL reload=TRUE);
		BOOL removeSet(AOSet* set);

		BOOL addAnimation(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item,BOOL reload=TRUE);
		BOOL removeAnimation(const AOSet* set,AOSet::AOState* state,S32 index);
		void checkSitCancel();
		void checkBelowWater(BOOL yes);

		BOOL importNotecard(const LLInventoryItem* item);
		void processImport( bool );

		BOOL swapWithPrevious(AOSet::AOState* state,S32 index);
		BOOL swapWithNext(AOSet::AOState* state,S32 index);

		void cycleTimeout(const AOSet* set);
		void cycle(eCycleMode cycleMode);

		void inMouselook(BOOL yes);
		void selectSet(AOSet* set);
		AOSet* selectSetByName(const std::string& name);
		AOSet* getSetByName(const std::string& name) const;

		// callback from LLAppViewer
		static void onLoginComplete();

		const std::vector<AOSet*> getSetList() const;
		const std::string getCurrentSetName() const;
		const AOSet* getDefaultSet() const;
		BOOL renameSet(AOSet* set,const std::string& name);

		void setDefaultSet(AOSet* set);
		void setOverrideSits(AOSet* set,BOOL yes);
		void setSmart(AOSet* set,BOOL yes);
		void setDisableStands(AOSet* set,BOOL yes);
		void setCycle(AOSet::AOState* set,BOOL yes);
		void setRandomize(AOSet::AOState* state,BOOL yes);
		void setCycleTime(AOSet::AOState* state,F32 time);

		void saveSettings();

		typedef boost::signals2::signal<void ()> updated_signal_t;
		boost::signals2::connection setReloadCallback(const updated_signal_t::slot_type& cb) { return mUpdatedSignal.connect(cb); };

	protected:
		void init();

		void setLastMotion(const LLUUID& motion);
		void setLastOverriddenMotion(const LLUUID& motion);
		void setStateCycleTimer(const AOSet::AOState* state);

		void stopAllStandVariants();
		void stopAllSitVariants();

		BOOL foreignAnimations(const LLUUID& seat);
		const LLUUID& mapSwimming(const LLUUID& motion) const;

		void updateSortOrder(AOSet::AOState* state);
		void saveSet(const AOSet* set);
		void saveState(const AOSet::AOState* state);

		BOOL createAnimationLink(const AOSet* set,AOSet::AOState* state,const LLInventoryItem* item);
		BOOL findForeignItems(const LLUUID& uuid) const;
		void purgeFolder(const LLUUID& uuid) const;

		void onToggleAOControl();
		static void onNotecardLoadComplete(	LLVFS* vfs,const LLUUID& assetUUID,LLAssetType::EType type,
												void* userdata,S32 status,LLExtStat extStatus);
		void parseNotecard(const char* buffer);

		updated_signal_t mUpdatedSignal;

		AOTimerCollection mTimerCollection;
		AOSitCancelTimer mSitCancelTimer;

		BOOL mEnabled;
		BOOL mInMouselook;
		BOOL mUnderWater;

		LLUUID mAOFolder;
		LLUUID mLastMotion;
		LLUUID mLastOverriddenMotion;

		std::vector<AOSet*> mSets;
		std::vector<AOSet*> mOldSets;
		AOSet* mCurrentSet;
		AOSet* mDefaultSet;

		AOSet* mImportSet;
		std::vector<AOSet*> mOldImportSets;
		LLUUID mImportCategory;
		S32 mImportRetryCount;
};

#endif // AOENGINE_H

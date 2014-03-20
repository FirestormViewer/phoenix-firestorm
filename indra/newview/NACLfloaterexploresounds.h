/** 
 * @file NACLfloaterexploresounds.h
 */

#ifndef LL_LLFLOATEREXPLORESOUNDS_H
#define LL_LLFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llaudioengine.h"

class LLCheckBoxCtrl;
class LLScrollListCtrl;

class NACLFloaterExploreSounds
: public LLFloater, public LLEventTimer
{
public:
	NACLFloaterExploreSounds(const LLSD& key);
	BOOL postBuild();

	BOOL tick();

	LLSoundHistoryItem getItem(const LLUUID& itemID);

private:
	virtual ~NACLFloaterExploreSounds();
	void handlePlayLocally();
	void handleLookAt();
	void handleStop();
	void handleSelection();
	void blacklistSound();

	LLScrollListCtrl*	mHistoryScroller;
	LLCheckBoxCtrl*		mCollisionSounds;
	LLCheckBoxCtrl*		mRepeatedAssets;
	LLCheckBoxCtrl*		mAvatarSounds;
	LLCheckBoxCtrl*		mObjectSounds;
	LLCheckBoxCtrl*		mPaused;

	std::list<LLSoundHistoryItem> mLastHistory;
};

#endif

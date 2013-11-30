/** 
 * @file NACLfloaterexploresounds.h
 */

#ifndef LL_LLFLOATEREXPLORESOUNDS_H
#define LL_LLFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llaudioengine.h"

class LLScrollListCtrl;

class NACLFloaterExploreSounds
: public LLFloater, public LLEventTimer
{
public:
	NACLFloaterExploreSounds(const LLSD& key);
	BOOL postBuild();

	BOOL tick();

	LLSoundHistoryItem getItem(LLUUID itemID);

private:
	virtual ~NACLFloaterExploreSounds();
	void handlePlayLocally();
	void handleLookAt();
	void handleStop();
	void handleSelection();
	void blacklistSound();

	LLScrollListCtrl* mHistoryScroller;
	std::list<LLSoundHistoryItem> mLastHistory;
};

#endif

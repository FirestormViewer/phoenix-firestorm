/** 
 * @file NACLfloaterexploresounds.h
 */

#ifndef LL_LLFLOATEREXPLORESOUNDS_H
#define LL_LLFLOATEREXPLORESOUNDS_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llaudioengine.h"

class NACLFloaterExploreSounds
: public LLFloater, public LLEventTimer
{
	friend class LLFloaterReg;
public:
	NACLFloaterExploreSounds(const LLSD& key);
	BOOL postBuild(void);

	BOOL tick();

	LLSoundHistoryItem getItem(LLUUID itemID);

	void handle_play_locally();
	void handle_look_at();
	void handle_stop();
	void blacklistSound();

private:
	virtual ~NACLFloaterExploreSounds();
	std::list<LLSoundHistoryItem> mLastHistory;
};

#endif

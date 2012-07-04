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
	void close(bool app_quitting);

	BOOL tick();

	LLSoundHistoryItem getItem(LLUUID itemID);

	static void handle_play_locally(void* user_data);
	static void handle_play_in_world(void* user_data);
	static void handle_look_at(void* user_data);
	static void handle_open(void* user_data);
	static void handle_copy_uuid(void* user_data);
	static void handle_stop(void* user_data);
	static void blacklistSound(void* user_data);

private:
	virtual ~NACLFloaterExploreSounds();
	std::list<LLSoundHistoryItem> mLastHistory;

// static
public:
	static NACLFloaterExploreSounds* sInstance;

	static void toggle();
};

#endif

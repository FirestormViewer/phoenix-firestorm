#ifndef NACL_ANTISPAM_H
#define NACL_ANTISPAM_H

#include <boost/unordered_map.hpp>
#include "fscommon.h"
#include "lluuid.h"
#include "llsingleton.h"

typedef enum e_antispam_queue
{
	ANTISPAM_QUEUE_CHAT,
	ANTISPAM_QUEUE_INVENTORY,
	ANTISPAM_QUEUE_IM,
	ANTISPAM_QUEUE_CALLING_CARD,
	ANTISPAM_QUEUE_SOUND,
	ANTISPAM_QUEUE_SOUND_PRELOAD,
	ANTISPAM_QUEUE_SCRIPT_DIALOG,
	ANTISPAM_QUEUE_TELEPORT,
	ANTISPAM_QUEUE_MAX
} EAntispamQueue;

class NACLAntiSpamQueueEntry
{
	friend class NACLAntiSpamQueue;
	friend class NACLAntiSpamRegistry;

public:
	U32 getEntryAmount();
	U32 getEntryTime();

protected:
	NACLAntiSpamQueueEntry();

	void clearEntry();
	void updateEntryAmount();
	void updateEntryTime();
	bool getBlocked();
	void setBlocked();

private:
	U32		mEntryAmount;
	U32		mEntryTime;
	bool	mBlocked;
};

typedef boost::unordered_map<LLUUID, NACLAntiSpamQueueEntry*, FSUUIDEntryHasher> t_spam_queue_entry_map;

class NACLAntiSpamQueue
{
	friend class NACLAntiSpamRegistry;

public:
	U32 getAmount();
	U32 getTime();

protected:
	NACLAntiSpamQueue(U32 time, U32 amount);
	~NACLAntiSpamQueue();

	void setAmount(U32 amount);
	void setTime(U32 time);

	void blockEntry(const LLUUID& source);
	S32 checkEntry(const LLUUID& source, U32 multiplier);

	void clearEntries();
	void purgeEntries();

private:
	t_spam_queue_entry_map	mEntries;
	U32						mQueueAmount;
	U32						mQueueTime;
};

class NACLAntiSpamRegistry : public LLSingleton<NACLAntiSpamRegistry>
{
	friend class LLSingleton<NACLAntiSpamRegistry>;

public:
	void setGlobalQueue(bool value);
	void setGlobalAmount(U32 amount);
	void setGlobalTime(U32 time);
	void setRegisteredQueueTime(EAntispamQueue queue, U32 time);
	void setRegisteredQueueAmount(EAntispamQueue queue, U32 amount);
	void setAllQueueTimes(U32 amount);
	void setAllQueueAmounts(U32 time);

	void blockOnQueue(EAntispamQueue queue, const LLUUID& source);
	bool checkQueue(EAntispamQueue queue, const LLUUID& source, U32 multiplier = 1, bool silent = false);

	void clearRegisteredQueue(EAntispamQueue queue);
	void purgeRegisteredQueue(EAntispamQueue queue);
	void clearAllQueues();
	void purgeAllQueues();

private:
	NACLAntiSpamRegistry();
	~NACLAntiSpamRegistry();

	const char* getQueueName(EAntispamQueue queue);

	void blockGlobalEntry(const LLUUID& source);
	S32 checkGlobalEntry(const LLUUID& source, U32 multiplier);

	void clearGlobalEntries();
	void purgeGlobalEntries();

	NACLAntiSpamQueue*		mQueues[ANTISPAM_QUEUE_MAX];
	t_spam_queue_entry_map	mGlobalEntries;
	U32						mGlobalTime;
	U32						mGlobalAmount;
	bool					mGlobalQueue;
};
#endif // NACL_ANTISPAM_H

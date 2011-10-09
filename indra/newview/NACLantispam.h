#ifndef NACLANTISPAM_H
#define NACLANTISPAM_H
#include <boost/tr1/unordered_map.hpp>
#include "stdtypes.h"
#include "lluuid.h"
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
	U32 entryAmount;
	U32 entryTime;
	bool blocked;
};
class NACLAntiSpamQueue
{
	friend class NACLAntiSpamRegistry;
public:
	U32 getAmount();
	U32 getTime();
protected:
	NACLAntiSpamQueue(U32 time, U32 amount);
	void setAmount(U32 amount);
	void setTime(U32 time);
	void clearEntries();
	void purgeEntries();
	void blockEntry(LLUUID& source);
	int checkEntry(LLUUID& source, U32 multiplier);
private:
	std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*> entries;
	std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*>::iterator it;
	U32 queueAmount;
	U32 queueTime;
};
class NACLAntiSpamRegistry
{
public:
	NACLAntiSpamRegistry(U32 time=2, U32 amount=10);
	static void registerQueues(U32 time=2, U32 amount=10);
	static void registerQueue(U32 name, U32 time, U32 amount);
	static void setRegisteredQueueTime(U32 name, U32 time);
	static void setRegisteredQueueAmount(U32 name,U32 amount);
	static void setAllQueueTimes(U32 amount);
	static void setAllQueueAmounts(U32 time);
	static bool checkQueue(U32 name, LLUUID& source, U32 multiplier=1, bool silent=false);
	static void clearRegisteredQueue(U32 name);
	static void purgeRegisteredQueue(U32 name);
	static void clearAllQueues();
	static void purgeAllQueues();
	static void setGlobalQueue(bool value);
	static void setGlobalAmount(U32 amount);
	static void setGlobalTime(U32 time);
	static void blockOnQueue(U32 name,LLUUID& owner_id);
	enum {
		QUEUE_CHAT,
		QUEUE_INVENTORY,
		QUEUE_IM,
		QUEUE_CALLING_CARD,
		QUEUE_SOUND,
		QUEUE_SOUND_PRELOAD,
		QUEUE_SCRIPT_DIALOG,
		QUEUE_TELEPORT,
		QUEUE_MAX
	};
private:
	static const char* getQueueName(U32 queue_id);
	static NACLAntiSpamQueue* queues[QUEUE_MAX];
	static std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*> globalEntries;
	static std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*>::iterator it2;
	static U32 globalTime;
	static U32 globalAmount;
	static bool bGlobalQueue;

	static int checkGlobalEntry(LLUUID& source, U32 multiplier);
	static void clearGlobalEntries();
	static void purgeGlobalEntries();
	static void blockGlobalEntry(LLUUID& source);
};
#endif //NACLANTISPAM_H

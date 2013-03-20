#include "llviewerprecompiledheaders.h"

#include "NACLantispam.h"

#include "llagent.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include <time.h>

U32 NACLAntiSpamRegistry::globalAmount;
U32 NACLAntiSpamRegistry::globalTime;
bool NACLAntiSpamRegistry::bGlobalQueue;
NACLAntiSpamQueue* NACLAntiSpamRegistry::queues[NACLAntiSpamRegistry::QUEUE_MAX] = {0};

t_spam_queue_entry_map NACLAntiSpamRegistry::globalEntries;
t_spam_queue_entry_map::iterator NACLAntiSpamRegistry::it2;

// NACLAntiSpamQueueEntry

NACLAntiSpamQueueEntry::NACLAntiSpamQueueEntry() :
	entryTime(0),
	entryAmount(0),
	blocked(false)
{
}

void NACLAntiSpamQueueEntry::clearEntry()
{
	entryTime = 0;
	entryAmount = 0;
	blocked = false;
}

U32 NACLAntiSpamQueueEntry::getEntryAmount()
{
	return entryAmount;
}

U32 NACLAntiSpamQueueEntry::getEntryTime()
{
	return entryTime;
}

void NACLAntiSpamQueueEntry::updateEntryAmount()
{
	entryAmount++;
}

void NACLAntiSpamQueueEntry::updateEntryTime()
{
	entryTime = time(0);
}

void NACLAntiSpamQueueEntry::setBlocked()
{
	blocked = true;
}

bool NACLAntiSpamQueueEntry::getBlocked()
{
	return blocked;
}

// NACLAntiSpamQueue

NACLAntiSpamQueue::NACLAntiSpamQueue(U32 time, U32 amount)
{
	queueTime = time;
	queueAmount = amount;
}

void NACLAntiSpamQueue::setAmount(U32 amount)
{
	queueAmount = amount;
}

void NACLAntiSpamQueue::setTime(U32 time)
{
	queueTime = time;
}

U32 NACLAntiSpamQueue::getTime()
{
	return queueTime;
}

U32 NACLAntiSpamQueue::getAmount()
{
	return queueAmount;
}

void NACLAntiSpamQueue::clearEntries()
{
	for (it = entries.begin(); it != entries.end(); it++)
	{
		//AO: Only clear entries that are not blocked.
		if (!it->second->getBlocked())
		{
			it->second->clearEntry();
		}
	}
}

void NACLAntiSpamQueue::purgeEntries()
{
	for (it = entries.begin(); it != entries.end(); it++)
	{
		delete it->second;
	}
	entries.clear();
}

void NACLAntiSpamQueue::blockEntry(const LLUUID& source)
{
	it = entries.find(source);
	if (it == entries.end())
	{
		entries[source] = new NACLAntiSpamQueueEntry();
	}

	entries[source]->setBlocked();
}

S32 NACLAntiSpamQueue::checkEntry(const LLUUID& name, U32 multiplier)
// Returns 0 if unblocked, 1 if check results in a new block, 2 if by an existing block
{
	it = entries.find(name);
	if (it != entries.end())
	{
		if (it->second->getBlocked())
		{
			return 2;
		}
		U32 eTime = it->second->getEntryTime();
		U32 currentTime = time(0);
		if ((currentTime - eTime) <= queueTime)
		{
			it->second->updateEntryAmount();
			U32 eAmount = it->second->getEntryAmount();
			if (eAmount > (queueAmount * multiplier))
			{
				it->second->setBlocked();
				return 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			it->second->clearEntry();
			it->second->updateEntryAmount();
			it->second->updateEntryTime();
			return 0;
		}
	}
	else
	{
		lldebugs << "[antispam] New queue entry:" << name.asString() << llendl;
		NACLAntiSpamQueueEntry* entry = new NACLAntiSpamQueueEntry();
		entry->updateEntryAmount();
		entry->updateEntryTime();
		entries[name] = entry;
		return 0;
	}
}

// NACLAntiSpamRegistry

static const char* QUEUE_NAME[NACLAntiSpamRegistry::QUEUE_MAX] = {
"Chat",
"Inventory",
"Instant Message",
"Calling Card",
"Sound",
"Sound Preload",
"Script Dialog",
"Teleport"
};

NACLAntiSpamRegistry::NACLAntiSpamRegistry(U32 time, U32 amount)
{
	globalTime = time;
	globalAmount = amount;
	static LLCachedControl<bool> _NACL_AntiSpamGlobalQueue(gSavedSettings, "_NACL_AntiSpamGlobalQueue");
	bGlobalQueue = _NACL_AntiSpamGlobalQueue;
	for (int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		queues[queue] = new NACLAntiSpamQueue(time, amount);
	}
}

const char* NACLAntiSpamRegistry::getQueueName(U32 queue_id)
{
	if (queue_id >= QUEUE_MAX)
	{
		return "Unknown";
	}
	return QUEUE_NAME[queue_id];
}

void NACLAntiSpamRegistry::registerQueues(U32 time, U32 amount)
{
	globalTime = time;
	globalAmount = amount;
	static LLCachedControl<bool> _NACL_AntiSpamGlobalQueue(gSavedSettings, "_NACL_AntiSpamGlobalQueue");
	bGlobalQueue = _NACL_AntiSpamGlobalQueue;
	for (int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		queues[queue] = new NACLAntiSpamQueue(time, amount);
	}
}

void NACLAntiSpamRegistry::registerQueue(U32 name, U32 time, U32 amount)
{
	/*
	it=queues.find(name);
	if(it == queues.end())
	{
		queues[name]=new NACLAntiSpamQueue(time,amount);
	}
	*/

}

void NACLAntiSpamRegistry::setRegisteredQueueTime(U32 name, U32 time)
{
	if (name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->setTime(time);
}

void NACLAntiSpamRegistry::setRegisteredQueueAmount(U32 name, U32 amount)
{
	if (name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->setAmount(amount);
}

void NACLAntiSpamRegistry::setAllQueueTimes(U32 time)
{
	globalTime = time;
	for (int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		if (queues[queue])
		{
			queues[queue]->setTime(time);
		}
	}
}

void NACLAntiSpamRegistry::setAllQueueAmounts(U32 amount)
{
	globalAmount = amount;
	for (int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		if (!queues[queue])
		{
			continue;
		}

		if (queue == QUEUE_SOUND || queue == QUEUE_SOUND_PRELOAD)
		{
			queues[queue]->setAmount(amount * 5);
		}
		else
		{
			queues[queue]->setAmount(amount);
		}
	}
}

void NACLAntiSpamRegistry::clearRegisteredQueue(U32 name)
{
	if (name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->clearEntries();
}

void NACLAntiSpamRegistry::purgeRegisteredQueue(U32 name)
{
	if (name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->purgeEntries();
}

void NACLAntiSpamRegistry::blockOnQueue(U32 name, const LLUUID& source)
{
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (!useAntiSpam)
	{
		return;
	}

	if (bGlobalQueue)
	{
		NACLAntiSpamRegistry::blockGlobalEntry(source);
	}
	else
	{
		if (name >= QUEUE_MAX || queues[name] == 0)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
			return;
		}
		queues[name]->blockEntry(source);
	}
}

void NACLAntiSpamRegistry::blockGlobalEntry(const LLUUID& source)
{
	it2 = globalEntries.find(source);
	if (it2 == globalEntries.end())
	{
		globalEntries[source] = new NACLAntiSpamQueueEntry();
	}

	globalEntries[source]->setBlocked();
}

bool NACLAntiSpamRegistry::checkQueue(U32 name, const LLUUID& source, U32 multiplier, bool silent)
// returns TRUE if blocked, FALSE otherwise
{
	// skip all checks if we're we've been administratively turned off
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (!useAntiSpam)
	{
		return false;
	}
	
	if (source.isNull() || gAgentID == source)
	{
		return false;
	}

	LLViewerObject* obj = gObjectList.findObject(source);
	if (obj && obj->permYouOwner())
	{
		return false;
	}
	
	int result = 0;
	if (bGlobalQueue)
	{
		result = NACLAntiSpamRegistry::checkGlobalEntry(source, multiplier);
	}
	else
	{
		if (name >= QUEUE_MAX || queues[name] == 0)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
			return false;
		}
		result = queues[name]->checkEntry(source, multiplier);
	}

	if (result == 0) // safe
	{
		return false;
	}
	
	if (result == 2) // previously blocked
	{
		return true;
	}
	
	if (result == 1) // newly blocked, result == 1
	{
		std::string msg = llformat("AntiSpam: Blocked %s for spamming a %s (%d) times in %d seconds.", source.asString().c_str(), getQueueName(name), multiplier * queues[name]->getAmount(), queues[name]->getTime());
		if (!silent)
		{
			LLSD args;
			args["MESSAGE"] = msg;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}
		llinfos << "[antispam] " << msg << llendl;
		return true;
	}

	// fallback, should not get here
	return false;
}

// Global queue
void NACLAntiSpamRegistry::setGlobalQueue(bool value)
{
	NACLAntiSpamRegistry::purgeAllQueues();
	bGlobalQueue = value;
}

void NACLAntiSpamRegistry::setGlobalAmount(U32 amount)
{
	globalAmount = amount;
}

void NACLAntiSpamRegistry::setGlobalTime(U32 time)
{
	globalTime = time;
}

void NACLAntiSpamRegistry::clearAllQueues()
{
	if(bGlobalQueue)
	{
		NACLAntiSpamRegistry::clearGlobalEntries();
	}
	else
	{
		for (int queue = 0; queue < QUEUE_MAX; ++queue)
		{
			if (queues[queue])
			{
				queues[queue]->clearEntries();
			}
		}
	}
}

void NACLAntiSpamRegistry::purgeAllQueues()
{
	if (bGlobalQueue)
	{
		NACLAntiSpamRegistry::purgeGlobalEntries();
	}
	else
	{
		for (int queue = 0; queue < QUEUE_MAX; ++queue)
		{
			if (queues[queue])
			{
				queues[queue]->purgeEntries();
			}
		}
	}
	llinfos << "AntiSpam Queues Purged" << llendl;
}

int NACLAntiSpamRegistry::checkGlobalEntry(const LLUUID& name, U32 multiplier)
{
	it2 = globalEntries.find(name);
	if (it2 != globalEntries.end())
	{
		if (it2->second->getBlocked())
		{
			return 2;
		}

		U32 eTime = it2->second->getEntryTime();
		U32 currentTime = time(0);
		if ((currentTime - eTime) <= globalTime)
		{
			it2->second->updateEntryAmount();
			U32 eAmount = it2->second->getEntryAmount();
			if (eAmount > (globalAmount * multiplier))
			{
				return 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			it2->second->clearEntry();
			it2->second->updateEntryAmount();
			it2->second->updateEntryTime();
			return 0;
		}
	}
	else
	{
		NACLAntiSpamQueueEntry* entry = new NACLAntiSpamQueueEntry();
		entry->updateEntryAmount();
		entry->updateEntryTime();
		globalEntries[name] = entry;
		return 0;
	}
}

void NACLAntiSpamRegistry::clearGlobalEntries()
{
	for (it2 = globalEntries.begin(); it2 != globalEntries.end(); it2++)
	{
		it2->second->clearEntry();
	}
}

void NACLAntiSpamRegistry::purgeGlobalEntries()
{
	for (it2 = globalEntries.begin(); it2 != globalEntries.end(); it2++)
	{
		delete it2->second;
		it2->second = 0;
	}
	globalEntries.clear();
}

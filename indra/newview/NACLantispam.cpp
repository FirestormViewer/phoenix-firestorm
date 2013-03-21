#include "llviewerprecompiledheaders.h"

#include "NACLantispam.h"

#include "llagent.h"
#include "llnotificationsutil.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include <time.h>

// NACLAntiSpamQueueEntry

NACLAntiSpamQueueEntry::NACLAntiSpamQueueEntry() :
	mEntryTime(0),
	mEntryAmount(0),
	mBlocked(false)
{
}

void NACLAntiSpamQueueEntry::clearEntry()
{
	mEntryTime = 0;
	mEntryAmount = 0;
	mBlocked = false;
}

U32 NACLAntiSpamQueueEntry::getEntryAmount()
{
	return mEntryAmount;
}

U32 NACLAntiSpamQueueEntry::getEntryTime()
{
	return mEntryTime;
}

void NACLAntiSpamQueueEntry::updateEntryAmount()
{
	mEntryAmount++;
}

void NACLAntiSpamQueueEntry::updateEntryTime()
{
	mEntryTime = time(NULL);
}

void NACLAntiSpamQueueEntry::setBlocked()
{
	mBlocked = true;
}

bool NACLAntiSpamQueueEntry::getBlocked()
{
	return mBlocked;
}

// NACLAntiSpamQueue

NACLAntiSpamQueue::NACLAntiSpamQueue(U32 time, U32 amount) :
	mQueueTime(time),
	mQueueAmount(amount)
{
}

NACLAntiSpamQueue::~NACLAntiSpamQueue()
{
	purgeEntries();
}

void NACLAntiSpamQueue::setAmount(U32 amount)
{
	mQueueAmount = amount;
}

void NACLAntiSpamQueue::setTime(U32 time)
{
	mQueueTime = time;
}

U32 NACLAntiSpamQueue::getTime()
{
	return mQueueTime;
}

U32 NACLAntiSpamQueue::getAmount()
{
	return mQueueAmount;
}

void NACLAntiSpamQueue::clearEntries()
{
	for (t_spam_queue_entry_map::iterator it = mEntries.begin(); it != mEntries.end(); ++it)
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
	for (t_spam_queue_entry_map::iterator it = mEntries.begin(); it != mEntries.end(); ++it)
	{
		delete it->second;
	}
	mEntries.clear();
}

void NACLAntiSpamQueue::blockEntry(const LLUUID& source)
{
	t_spam_queue_entry_map::iterator it = mEntries.find(source);
	if (it == mEntries.end())
	{
		mEntries[source] = new NACLAntiSpamQueueEntry();
	}

	mEntries[source]->setBlocked();
}

S32 NACLAntiSpamQueue::checkEntry(const LLUUID& name, U32 multiplier)
// Returns 0 if unblocked, 1 if check results in a new block, 2 if by an existing block
{
	t_spam_queue_entry_map::iterator it = mEntries.find(name);
	if (it != mEntries.end())
	{
		if (it->second->getBlocked())
		{
			return 2;
		}
		U32 eTime = it->second->getEntryTime();
		U32 currentTime = time(0);
		if ((currentTime - eTime) <= mQueueTime)
		{
			it->second->updateEntryAmount();
			U32 eAmount = it->second->getEntryAmount();
			if (eAmount > (mQueueAmount * multiplier))
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
		mEntries[name] = entry;
		return 0;
	}
}

// NACLAntiSpamRegistry

static const char* QUEUE_NAME[ANTISPAM_QUEUE_MAX] = {
	"Chat",
	"Inventory",
	"Instant Message",
	"Calling Card",
	"Sound",
	"Sound Preload",
	"Script Dialog",
	"Teleport"
};

NACLAntiSpamRegistry::NACLAntiSpamRegistry() :
	mGlobalTime(gSavedSettings.getU32("_NACL_AntiSpamTime")),
	mGlobalAmount(gSavedSettings.getU32("_NACL_AntiSpamAmount")),
	mGlobalQueue(gSavedSettings.getBOOL("_NACL_AntiSpamGlobalQueue"))
{
	for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
	{
		mQueues[queue] = new NACLAntiSpamQueue(mGlobalTime, mGlobalAmount);
	}
}

NACLAntiSpamRegistry::~NACLAntiSpamRegistry()
{
	purgeAllQueues();
	for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
	{
		delete mQueues[queue];
	}
}

const char* NACLAntiSpamRegistry::getQueueName(EAntispamQueue queue)
{
	if (queue >= ANTISPAM_QUEUE_MAX)
	{
		return "Unknown";
	}
	return QUEUE_NAME[queue];
}

void NACLAntiSpamRegistry::setRegisteredQueueTime(EAntispamQueue queue, U32 time)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
		return;
	}
	
	mQueues[queue]->setTime(time);
}

void NACLAntiSpamRegistry::setRegisteredQueueAmount(EAntispamQueue queue, U32 amount)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
		return;
	}
	
	mQueues[queue]->setAmount(amount);
}

void NACLAntiSpamRegistry::setAllQueueTimes(U32 time)
{
	mGlobalTime = time;
	for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
	{
		if (mQueues[queue])
		{
			mQueues[queue]->setTime(time);
		}
	}
}

void NACLAntiSpamRegistry::setAllQueueAmounts(U32 amount)
{
	mGlobalAmount = amount;
	for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
	{
		if (!mQueues[queue])
		{
			continue;
		}

		if (queue == ANTISPAM_QUEUE_SOUND || queue == ANTISPAM_QUEUE_SOUND_PRELOAD)
		{
			mQueues[queue]->setAmount(amount * 5);
		}
		else
		{
			mQueues[queue]->setAmount(amount);
		}
	}
}

void NACLAntiSpamRegistry::clearRegisteredQueue(EAntispamQueue queue)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
		return;
	}
	
	mQueues[queue]->clearEntries();
}

void NACLAntiSpamRegistry::purgeRegisteredQueue(EAntispamQueue queue)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
		return;
	}
	
	mQueues[queue]->purgeEntries();
}

void NACLAntiSpamRegistry::blockOnQueue(EAntispamQueue queue, const LLUUID& source)
{
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (!useAntiSpam)
	{
		return;
	}

	if (mGlobalQueue)
	{
		blockGlobalEntry(source);
	}
	else
	{
		if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
			return;
		}
		mQueues[queue]->blockEntry(source);
	}
}

void NACLAntiSpamRegistry::blockGlobalEntry(const LLUUID& source)
{
	t_spam_queue_entry_map::iterator it = mGlobalEntries.find(source);
	if (it == mGlobalEntries.end())
	{
		mGlobalEntries[source] = new NACLAntiSpamQueueEntry();
	}

	mGlobalEntries[source]->setBlocked();
}

bool NACLAntiSpamRegistry::checkQueue(EAntispamQueue queue, const LLUUID& source, U32 multiplier, bool silent)
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
	
	S32 result = 0;
	if (mGlobalQueue)
	{
		result = checkGlobalEntry(source, multiplier);
	}
	else
	{
		if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << llendl;
			return false;
		}
		result = mQueues[queue]->checkEntry(source, multiplier);
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
		std::string msg = llformat("AntiSpam: Blocked %s for spamming a %s (%d) times in %d seconds.", source.asString().c_str(), getQueueName(queue), multiplier * mQueues[queue]->getAmount(), mQueues[queue]->getTime());
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
	purgeAllQueues();
	mGlobalQueue = value;
}

void NACLAntiSpamRegistry::setGlobalAmount(U32 amount)
{
	mGlobalAmount = amount;
}

void NACLAntiSpamRegistry::setGlobalTime(U32 time)
{
	mGlobalTime = time;
}

void NACLAntiSpamRegistry::clearAllQueues()
{
	if (mGlobalQueue)
	{
		clearGlobalEntries();
	}
	else
	{
		for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
		{
			if (mQueues[queue])
			{
				mQueues[queue]->clearEntries();
			}
		}
	}
}

void NACLAntiSpamRegistry::purgeAllQueues()
{
	if (mGlobalQueue)
	{
		purgeGlobalEntries();
	}
	else
	{
		for (S32 queue = 0; queue < ANTISPAM_QUEUE_MAX; ++queue)
		{
			if (mQueues[queue])
			{
				mQueues[queue]->purgeEntries();
			}
		}
	}
	llinfos << "AntiSpam Queues Purged" << llendl;
}

S32 NACLAntiSpamRegistry::checkGlobalEntry(const LLUUID& source, U32 multiplier)
{
	t_spam_queue_entry_map::iterator it = mGlobalEntries.find(source);
	if (it != mGlobalEntries.end())
	{
		if (it->second->getBlocked())
		{
			return 2;
		}

		U32 eTime = it->second->getEntryTime();
		U32 currentTime = time(NULL);
		if ((currentTime - eTime) <= mGlobalTime)
		{
			it->second->updateEntryAmount();
			U32 eAmount = it->second->getEntryAmount();
			if (eAmount > (mGlobalAmount * multiplier))
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
			it->second->clearEntry();
			it->second->updateEntryAmount();
			it->second->updateEntryTime();
			return 0;
		}
	}
	else
	{
		NACLAntiSpamQueueEntry* entry = new NACLAntiSpamQueueEntry();
		entry->updateEntryAmount();
		entry->updateEntryTime();
		mGlobalEntries[source] = entry;
		return 0;
	}
}

void NACLAntiSpamRegistry::clearGlobalEntries()
{
	for (t_spam_queue_entry_map::iterator it = mGlobalEntries.begin(); it != mGlobalEntries.end(); ++it)
	{
		it->second->clearEntry();
	}
}

void NACLAntiSpamRegistry::purgeGlobalEntries()
{
	for (t_spam_queue_entry_map::iterator it = mGlobalEntries.begin(); it != mGlobalEntries.end(); ++it)
	{
		delete it->second;
	}
	mGlobalEntries.clear();
}

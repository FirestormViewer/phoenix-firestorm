#include "llviewerprecompiledheaders.h"
#include "NACLantispam.h"
#include "llviewercontrol.h"
#include "llnotificationsutil.h"
#include "llviewerobjectlist.h"
#include "llagent.h"
#include <time.h>

U32 NACLAntiSpamRegistry::globalAmount;
U32 NACLAntiSpamRegistry::globalTime;
bool NACLAntiSpamRegistry::bGlobalQueue;
NACLAntiSpamQueue* NACLAntiSpamRegistry::queues[NACLAntiSpamRegistry::QUEUE_MAX] = {0};
std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*> NACLAntiSpamRegistry::globalEntries;
std::tr1::unordered_map<std::string,NACLAntiSpamQueueEntry*>::iterator NACLAntiSpamRegistry::it2;

// NACLAntiSpamQueueEntry

NACLAntiSpamQueueEntry::NACLAntiSpamQueueEntry()
{
	entryTime=0;
	entryAmount=0;
	blocked=false;
}
void NACLAntiSpamQueueEntry::clearEntry()
{
	entryTime=0;
	entryAmount=0;
	blocked=false;
}
U32 NACLAntiSpamQueueEntry::getEntryAmount()
{
	return entryAmount;
}
U32 NACLAntiSpamQueueEntry::getEntryTime()
{
	return entryTime;
}
void NACLAntiSpamQueueEntry::updateEntry()
{
	entryAmount++;
	entryTime=time(0);
}
void NACLAntiSpamQueueEntry::setBlocked()
{
	blocked=true;
}
bool NACLAntiSpamQueueEntry::getBlocked()
{
	return blocked;
}

// NACLAntiSpamQueue

NACLAntiSpamQueue::NACLAntiSpamQueue(U32 time, U32 amount)
{
	queueTime=time;
	queueAmount=amount;
}
void NACLAntiSpamQueue::setAmount(U32 amount)
{
	queueAmount=amount;
}
void NACLAntiSpamQueue::setTime(U32 time)
{
	queueTime=time;
}
void NACLAntiSpamQueue::clearEntries()
{
	for(it = entries.begin(); it != entries.end(); it++)
	{
		it->second->clearEntry();
	}
}
void NACLAntiSpamQueue::purgeEntries()
{
	for(it = entries.begin(); it != entries.end(); it++)
	{
		delete it->second;
	}
	entries.clear();
}
void NACLAntiSpamQueue::blockEntry(LLUUID& source)
{
	it=entries.find(source.asString());
	if(it != entries.end())
	{
		entries[source.asString()]=new NACLAntiSpamQueueEntry();
	}
	entries[source.asString()]->setBlocked();
}
int NACLAntiSpamQueue::checkEntry(LLUUID& name, int multiplier)
{
	it=entries.find(name.asString());
	if(it != entries.end())
	{
		if(it->second->getBlocked()) return 2;
		U32 eTime=it->second->getEntryTime();
		U32 eAmount=it->second->getEntryAmount();
		U32 currentTime=time(0);
		if((currentTime-eTime) <= queueTime)
		{
			it->second->updateEntry();
			if(eAmount > (queueAmount*multiplier))
			{
				it->second->setBlocked();
				return 1;
			}
			else
				return 0;
		}
		else
		{
			it->second->clearEntry();
			it->second->updateEntry();
			return 0;
		}
	}
	else
	{
		llinfos << "New entry:" << name.asString() << llendl;
		entries[name.asString()]=new NACLAntiSpamQueueEntry();
		entries[name.asString()]->updateEntry();
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
"Teleport"};

NACLAntiSpamRegistry::NACLAntiSpamRegistry(U32 time, U32 amount)
{
	globalTime=time;
	globalAmount=amount;
	static LLCachedControl<bool> _NACL_AntiSpamGlobalQueue(gSavedSettings,"_NACL_AntiSpamGlobalQueue");
	bGlobalQueue=_NACL_AntiSpamGlobalQueue;
	for(int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		queues[queue] = new NACLAntiSpamQueue(time,amount);
	}
}
const char* NACLAntiSpamRegistry::getQueueName(U32 queue_id)
{
	if(queue_id >= QUEUE_MAX)
		return "Unknown";
	return QUEUE_NAME[queue_id];
}
void NACLAntiSpamRegistry::registerQueues(U32 time, U32 amount)
{
	globalTime=time;
	globalAmount=amount;
	static LLCachedControl<bool> _NACL_AntiSpamGlobalQueue(gSavedSettings,"_NACL_AntiSpamGlobalQueue");
	bGlobalQueue=_NACL_AntiSpamGlobalQueue;
	for(int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		queues[queue] = new NACLAntiSpamQueue(time,amount);
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
	if(name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->setTime(time);
}
void NACLAntiSpamRegistry::setRegisteredQueueAmount(U32 name, U32 amount)
{
	if(name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->setAmount(amount);
}
void NACLAntiSpamRegistry::setAllQueueTimes(U32 time)
{
	globalTime=time;
	for(int queue = 0; queue < QUEUE_MAX; ++queue)
		queues[queue]->setTime(time);
}
void NACLAntiSpamRegistry::setAllQueueAmounts(U32 amount)
{
	globalAmount=amount;
	for(int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		if(queue == QUEUE_SOUND || queue == QUEUE_SOUND_PRELOAD)
			queues[queue]->setAmount(amount*5);
		else
			queues[queue]->setAmount(amount);
	}
}
void NACLAntiSpamRegistry::clearRegisteredQueue(U32 name)
{
	if(name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->clearEntries();
}
void NACLAntiSpamRegistry::purgeRegisteredQueue(U32 name)
{
	if(name >= QUEUE_MAX || queues[name] == 0)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
		return;
	}
	
	queues[name]->purgeEntries();
}
void NACLAntiSpamRegistry::blockOnQueue(U32 name, LLUUID& source)
{
	if(bGlobalQueue)
	{
		NACLAntiSpamRegistry::blockGlobalEntry(source);
	}
	else
	{
		if(name >= QUEUE_MAX || queues[name] == 0)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
			return;
		}
		queues[name]->blockEntry(source);
	}
}
void NACLAntiSpamRegistry::blockGlobalEntry(LLUUID& source)
{
	it2=globalEntries.find(source.asString());
	if(it2 != globalEntries.end())
	{
		globalEntries[source.asString()]=new NACLAntiSpamQueueEntry();
	}
	globalEntries[source.asString()]->setBlocked();
}
bool NACLAntiSpamRegistry::checkQueue(U32 name, LLUUID& source, int multiplier, bool silent)
{
	if(source.isNull()) return false;
	if(gAgent.getID() == source) return false;
	LLViewerObject *obj=gObjectList.findObject(source);
	if(obj)
		if(obj->permYouOwner()) return false;
	
	int result;
	if(bGlobalQueue)
	{
		result=NACLAntiSpamRegistry::checkGlobalEntry(source,multiplier);
	}
	else
	{
		if(name >= QUEUE_MAX || queues[name] == 0)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(name) << llendl;
			return false;
		}
		result=queues[name]->checkEntry(source,multiplier);
	}
	if(result==0)
	{
		return false;
	}
	else if(result==2)
	{
		return true;
	}
	else
	{
		if(!silent)
		{
		LLSD args;
		args["MESSAGE"] = std::string(getQueueName(name))+": Blocked object "+source.asString();
		LLNotificationsUtil::add("SystemMessageTip", args);
		}
		return true;
	}
}

// Global queue stoof
void NACLAntiSpamRegistry::setGlobalQueue(bool value)
{
	NACLAntiSpamRegistry::purgeAllQueues();
	bGlobalQueue=value;
}
void NACLAntiSpamRegistry::setGlobalAmount(U32 amount)
{
	globalAmount=amount;
}
void NACLAntiSpamRegistry::setGlobalTime(U32 time)
{
	globalTime=time;
}
void NACLAntiSpamRegistry::clearAllQueues()
{
	if(bGlobalQueue)
		NACLAntiSpamRegistry::clearGlobalEntries();
	else
	for(int queue = 0; queue < QUEUE_MAX; ++queue)
	{
		queues[queue]->clearEntries();
	}
}
void NACLAntiSpamRegistry::purgeAllQueues()
{
	if(bGlobalQueue)
		NACLAntiSpamRegistry::purgeGlobalEntries();
	else
		for(int queue = 0; queue < QUEUE_MAX; ++queue)
		{
			queues[queue]->purgeEntries();
		}
}
int NACLAntiSpamRegistry::checkGlobalEntry(LLUUID& name, int multiplier)
{
	it2=globalEntries.find(name.asString());
	if(it2 != globalEntries.end())
	{
		if(it2->second->getBlocked()) return 2;
		U32 eTime=it2->second->getEntryTime();
		U32 eAmount=it2->second->getEntryAmount();
		U32 currentTime=time(0);
		if((currentTime-eTime) <= globalTime)
		{
			it2->second->updateEntry();
			if(eAmount > (globalAmount*multiplier))
				return 1;
			else
				return 0;
		}
		else
		{
			it2->second->clearEntry();
			it2->second->updateEntry();
			return 0;
		}
	}
	else
	{
		globalEntries[name.asString()]=new NACLAntiSpamQueueEntry();
		globalEntries[name.asString()]->updateEntry();
		return 0;
	}
}
void NACLAntiSpamRegistry::clearGlobalEntries()
{
	for(it2 = globalEntries.begin(); it2 != globalEntries.end(); it2++)
	{
		it2->second->clearEntry();
	}
}
void NACLAntiSpamRegistry::purgeGlobalEntries()
{
	for(it2 = globalEntries.begin(); it2 != globalEntries.end(); it2++)
	{
		delete it2->second;
		it2->second = 0;
	}
	globalEntries.clear();
}
#include "llviewerprecompiledheaders.h"

#include "NACLantispam.h"

#include "llagent.h"
#include "llmutelist.h"
#include "llnotificationsutil.h"
#include "llslurl.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llworld.h"
#include "sound_ids.h"
#include <time.h>
#include <boost/regex.hpp>

const static boost::regex NEWLINES("\\n{1}");

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

NACLAntiSpamQueueEntry* NACLAntiSpamQueue::getEntry(const LLUUID& source)
{
	spam_queue_entry_map_t::iterator found = mEntries.find(source);
	if (found != mEntries.end())
	{
		return found->second;
	}
	else
	{
		return NULL;
	}
}

void NACLAntiSpamQueue::clearEntries()
{
	for (spam_queue_entry_map_t::iterator it = mEntries.begin(); it != mEntries.end(); ++it)
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
	for (spam_queue_entry_map_t::iterator it = mEntries.begin(); it != mEntries.end(); ++it)
	{
		delete it->second;
	}
	mEntries.clear();
}

void NACLAntiSpamQueue::blockEntry(const LLUUID& source)
{
	spam_queue_entry_map_t::iterator it = mEntries.find(source);
	if (it == mEntries.end())
	{
		mEntries[source] = new NACLAntiSpamQueueEntry();
	}

	mEntries[source]->setBlocked();
}

S32 NACLAntiSpamQueue::checkEntry(const LLUUID& name, U32 multiplier)
// Returns 0 if unblocked, 1 if check results in a new block, 2 if by an existing block
{
	spam_queue_entry_map_t::iterator it = mEntries.find(name);
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
		LL_DEBUGS("AntiSpam") << "New queue entry:" << name.asString() << LL_ENDL;
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

	for (S32 i = 0; i < COLLISION_SOUNDS_SIZE; ++i)
	{
		mCollisionSounds.insert(LLUUID(COLLISION_SOUNDS[i]));
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
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
		return;
	}
	
	mQueues[queue]->setTime(time);
}

void NACLAntiSpamRegistry::setRegisteredQueueAmount(EAntispamQueue queue, U32 amount)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
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
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
		return;
	}
	
	mQueues[queue]->clearEntries();
}

void NACLAntiSpamRegistry::purgeRegisteredQueue(EAntispamQueue queue)
{
	if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
	{
		LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
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
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
			return;
		}
		mQueues[queue]->blockEntry(source);
	}
}

void NACLAntiSpamRegistry::blockGlobalEntry(const LLUUID& source)
{
	spam_queue_entry_map_t::iterator it = mGlobalEntries.find(source);
	if (it == mGlobalEntries.end())
	{
		mGlobalEntries[source] = new NACLAntiSpamQueueEntry();
	}

	mGlobalEntries[source]->setBlocked();
}

bool NACLAntiSpamRegistry::checkQueue(EAntispamQueue queue, const LLUUID& source, EAntispamSource sourcetype, U32 multiplier)
// returns TRUE if blocked, FALSE otherwise
{
	// skip all checks if we're we've been administratively turned off
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	static LLCachedControl<bool> useAntiSpamMine(gSavedSettings, "FSUseAntiSpamMine");
	if (!useAntiSpam)
	{
		return false;
	}
	
	if (source.isNull() || gAgentID == source)
	{
		return false;
	}

	LLViewerObject* obj = gObjectList.findObject(source);
	if (obj && obj->permYouOwner() && !useAntiSpamMine)
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
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
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
		if (!LLMuteList::getInstance()->isMuted(source))
		{
			AntispamObjectData data;
			data.mName = source.asString();
			data.mQueue = queue;
			data.mCount = multiplier * mQueues[queue]->getAmount();
			data.mPeriod = mQueues[queue]->getTime();
			data.mNotificationId = "AntiSpamBlocked";

			if (sourcetype == ANTISPAM_SOURCE_OBJECT)
			{
				bool sent = false;

				for (LLWorld::region_list_t::const_iterator iter = LLWorld::getInstance()->getRegionList().begin();
					iter != LLWorld::getInstance()->getRegionList().end(); ++iter)
				{
					LLViewerRegion* region = *iter;
					if (gMessageSystem && region && region->isAlive())
					{
						gMessageSystem->newMessage(_PREHASH_RequestObjectPropertiesFamily);
						gMessageSystem->nextBlockFast(_PREHASH_AgentData);
						gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgentID);
						gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
						gMessageSystem->addU32Fast(_PREHASH_RequestFlags, 0);
						gMessageSystem->addUUIDFast(_PREHASH_ObjectID, source);
						gMessageSystem->sendReliable(region->getHost());
						sent = true;
					}
				}

				if (sent)
				{
					mObjectData[source] = data;
				}
			}
			else if (sourcetype == ANTISPAM_SOURCE_AGENT)
			{
				LLUUID request_id;
				request_id.generate();
				LLAvatarNameCache::callback_connection_t cb = LLAvatarNameCache::get(source, boost::bind(&NACLAntiSpamRegistry::onAvatarNameCallback, this, _1, _2, data, request_id));
				mAvatarNameCallbackConnections.insert(std::make_pair(request_id, cb));
			}
		}
		LL_INFOS("AntiSpam") << "Blocked " << source.asString() << " for spamming a " << getQueueName(queue) << " (" << multiplier * mQueues[queue]->getAmount() << ") times in " << mQueues[queue]->getTime() << " seconds." << LL_ENDL;
		return true;
	}

	// fallback, should not get here
	return false;
}

bool NACLAntiSpamRegistry::checkNewlineFlood(EAntispamQueue queue, const LLUUID& source, const std::string& message)
{
	if (!isBlockedOnQueue(ANTISPAM_QUEUE_IM, source))
	{
		static LLCachedControl<U32> _NACL_AntiSpamNewlines(gSavedSettings, "_NACL_AntiSpamNewlines");
		boost::sregex_iterator iter(message.begin(), message.end(), NEWLINES);
		if ((std::abs(std::distance(iter, boost::sregex_iterator())) > _NACL_AntiSpamNewlines))
		{
			blockOnQueue(ANTISPAM_QUEUE_IM, source);
			if (!LLMuteList::getInstance()->isMuted(source))
			{
				LL_INFOS("AntiSpam") << "[antispam] blocked owner due to too many newlines: " << source << LL_ENDL;

				AntispamObjectData data;
				data.mName = source.asString();
				data.mQueue = queue;
				data.mCount = _NACL_AntiSpamNewlines();
				data.mPeriod = 0;
				data.mNotificationId = (queue == ANTISPAM_QUEUE_IM ? "AntiSpamImNewLineFloodBlocked" : "AntiSpamChatNewLineFloodBlocked");

				LLUUID request_id;
				request_id.generate();
				LLAvatarNameCache::callback_connection_t cb = LLAvatarNameCache::get(source,
																					 boost::bind(&NACLAntiSpamRegistry::onAvatarNameCallback,
																								 this, _1, _2, data, request_id));
				mAvatarNameCallbackConnections.insert( std::make_pair( request_id, cb ) );
				
			}
			return true;
		}
		return false;
	}
	return true;
}

bool NACLAntiSpamRegistry::isBlockedOnQueue(EAntispamQueue queue, const LLUUID& source)
{
	// skip all checks if we're we've been administratively turned off
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (!useAntiSpam)
	{
		return false;
	}

	if (mGlobalQueue)
	{
		spam_queue_entry_map_t::iterator found = mGlobalEntries.find(source);
		if (found != mGlobalEntries.end())
		{
			return (found->second->getBlocked());
		}
		else
		{
			return false;
		}
	}
	else
	{
		if (queue >= ANTISPAM_QUEUE_MAX || mQueues[queue] == NULL)
		{
			LL_ERRS("AntiSpam") << "CODE BUG: Attempting to use a antispam queue that was not created or was outside of the reasonable range of queues. Queue: " << getQueueName(queue) << LL_ENDL;
			return false;
		}

		NACLAntiSpamQueueEntry* entry = mQueues[queue]->getEntry(source);
		return (entry && entry->getBlocked());
	}
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
	std::map<LLUUID, LLAvatarNameCache::callback_connection_t>::iterator it = mAvatarNameCallbackConnections.begin();
	for (; it != mAvatarNameCallbackConnections.end(); ++it)
	{
		if (it->second.connected())
		{
			it->second.disconnect();
		}
	}
	mAvatarNameCallbackConnections.clear();

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
	mObjectData.clear();
}

S32 NACLAntiSpamRegistry::checkGlobalEntry(const LLUUID& source, U32 multiplier)
{
	spam_queue_entry_map_t::iterator it = mGlobalEntries.find(source);
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
	for (spam_queue_entry_map_t::iterator it = mGlobalEntries.begin(); it != mGlobalEntries.end(); ++it)
	{
		it->second->clearEntry();
	}
}

void NACLAntiSpamRegistry::purgeGlobalEntries()
{
	for (spam_queue_entry_map_t::iterator it = mGlobalEntries.begin(); it != mGlobalEntries.end(); ++it)
	{
		delete it->second;
	}
	mGlobalEntries.clear();
}

bool NACLAntiSpamRegistry::isCollisionSound(const LLUUID& sound_id)
{
	return (mCollisionSounds.find(sound_id) != mCollisionSounds.end());
}

void NACLAntiSpamRegistry::processObjectPropertiesFamily(LLMessageSystem* msg)
{
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (!useAntiSpam)
	{
		return;
	}

	LLUUID id;
	LLUUID owner_id;
	std::string name;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, id);
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, owner_id);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);

	std::map<LLUUID, AntispamObjectData>::iterator found = mObjectData.find(id);
	if (found != mObjectData.end())
	{
		AntispamObjectData data = found->second;

		if (!isBlockedOnQueue(data.mQueue, owner_id))
		{
			data.mName = LLSLURL("objectim", id, "").getSLURLString() + "?name=" + LLURI::escape(name) + "&owner=" + owner_id.asString();
			notify(data);
		}

		mObjectData.erase(found);
	}
}

void NACLAntiSpamRegistry::onAvatarNameCallback(const LLUUID& av_id, const LLAvatarName& av_name, AntispamObjectData data, const LLUUID& request_id)
{
	std::map<LLUUID, LLAvatarNameCache::callback_connection_t>::iterator found = mAvatarNameCallbackConnections.find(request_id);
	if (found != mAvatarNameCallbackConnections.end())
	{
		if (found->second.connected())
		{
			found->second.disconnect();
		}
		mAvatarNameCallbackConnections.erase(found);
	}

	data.mName = LLSLURL("agent", av_id, "inspect").getSLURLString();
	notify(data);
}

void NACLAntiSpamRegistry::notify(AntispamObjectData data)
{
	LLSD args;
	args["SOURCE"] = data.mName;
	args["QUEUE"] = getQueueName(data.mQueue);
	args["COUNT"] = llformat("%d", data.mCount);
	args["PERIOD"] = llformat("%d", data.mPeriod);
	LLNotificationsUtil::add(data.mNotificationId, args);
}

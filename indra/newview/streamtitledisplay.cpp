/* Copyright (c) 2010 Katharine Berry All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Katharine Berry nor the names of any contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY KATHARINE BERRY AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL KATHARINE BERRY OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "streamtitledisplay.h"
#include "llagent.h"
#include "llaudioengine.h"
#include "llnotificationsutil.h"
#include "llstreamingaudio.h"
#include "llviewercontrol.h"
#include "lltrans.h"
#include "fscommon.h"
#include "message.h"

StreamTitleDisplay::StreamTitleDisplay() : LLEventTimer(2) { };

BOOL StreamTitleDisplay::tick()
{
	checkMetadata();
	return false;
}

void StreamTitleDisplay::checkMetadata()
{
	LLCachedControl<U32> ShowStreamMetadata(gSavedSettings, "ShowStreamMetadata");
	LLCachedControl<bool> StreamMetadataAnnounceToChat(gSavedSettings, "StreamMetadataAnnounceToChat");

	if(!gAudiop)
		return;
	if(gAudiop->getStreamingAudioImpl()->hasNewMetadata() && (ShowStreamMetadata > 0 || StreamMetadataAnnounceToChat))
	{
		std::string chat = "";
		std::string title = gAudiop->getStreamingAudioImpl()->getCurrentTitle();
		std::string artist = gAudiop->getStreamingAudioImpl()->getCurrentArtist();

		if(artist.length() > 0)
		{
			chat = artist;
		}
		if(title.length() > 0)
		{
			if (chat.length() > 0)
			{
				chat += " - ";
			}
			chat += title;
		}
		if (chat.length() > 0)
		{
			if (StreamMetadataAnnounceToChat)
			{
				sendStreamTitleToChat(chat);
			}

			if (ShowStreamMetadata > 1)
			{
				chat = LLTrans::getString("StreamtitleNowPlaying") + " " + chat;
				reportToNearbyChat(chat);
			}
			else if (ShowStreamMetadata == 1)
			{
				LLSD args;
				args["TITLE"] = title;
				if(artist.length() > 0)
				{
					args["ARTIST"] = artist;
					LLNotificationsUtil::add("StreamMetadata", args);
					
				}
				else
				{
					LLNotificationsUtil::add("StreamMetadataNoArtist", args);
				}
			}
		}
	}
}

void StreamTitleDisplay::sendStreamTitleToChat(const std::string& Title)
{
	LLCachedControl<S32> StreamMetadataAnnounceChannel(gSavedSettings, "StreamMetadataAnnounceChannel");
	if (StreamMetadataAnnounceChannel != 0)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_ChatFromViewer);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_ChatData);
		msg->addStringFast(_PREHASH_Message, Title);
		msg->addU8Fast(_PREHASH_Type, CHAT_TYPE_WHISPER);
		msg->addS32("Channel", StreamMetadataAnnounceChannel);

		gAgent.sendReliableMessage();
	}
}

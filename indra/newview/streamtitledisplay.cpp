/* Copyright (c) 2010 Katharine Berry All rights reserved.
 * Copyright (c) 2013 Cinder Roxley <cinder.roxley@phoenixviewer.com>
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

#include "fscommon.h"
#include "llagent.h"
#include "llaudioengine.h"
#include "llchat.h"
#include "llnotificationsutil.h"
#include "llstreamingaudio.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "message.h"
#include <functional>

StreamTitleDisplay::~StreamTitleDisplay()
{
    if (mMetadataUpdateConnection.connected())
    {
        mMetadataUpdateConnection.disconnect();
    }
}

void StreamTitleDisplay::initSingleton()
{
    if (!gAudiop || !gAudiop->getStreamingAudioImpl())
    {
        return;
    }

    mMetadataUpdateConnection = gAudiop->getStreamingAudioImpl()->setMetadataUpdateCallback(std::bind(&StreamTitleDisplay::checkMetadata, this, std::placeholders::_1));
    checkMetadata(gAudiop->getStreamingAudioImpl()->getCurrentMetadata());
}

void StreamTitleDisplay::checkMetadata(const LLSD& metadata)
{
    static LLCachedControl<U32> ShowStreamMetadata(gSavedSettings, "ShowStreamMetadata", 1);
    static LLCachedControl<bool> StreamMetadataAnnounceToChat(gSavedSettings, "StreamMetadataAnnounceToChat", false);

    if (ShowStreamMetadata > 0 || StreamMetadataAnnounceToChat)
    {
        std::string chat{};

        if (metadata.has("ARTIST"))
        {
            chat = metadata["ARTIST"].asString();
        }
        if (metadata.has("TITLE"))
        {
            if (chat.length() > 0)
            {
                chat.append(" - ");
            }
            chat.append(metadata["TITLE"].asString());
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
                FSCommon::report_to_nearby_chat(chat);
            }
            else if (ShowStreamMetadata == 1 && (metadata.has("TITLE") || metadata.has("ARTIST")))
            {
                LLSD substitutions(metadata);
                if (!substitutions.has("TITLE"))
                {
                    substitutions["TITLE"] = "";
                }
                LLNotificationsUtil::add((substitutions.has("ARTIST") ? "StreamMetadata" : "StreamMetadataNoArtist"), substitutions);
            }
        }
    }
}

void StreamTitleDisplay::sendStreamTitleToChat(std::string_view title)
{
    static LLCachedControl<S32> streamMetadataAnnounceChannel(gSavedSettings, "StreamMetadataAnnounceChannel");
    if (streamMetadataAnnounceChannel != 0)
    {
        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_ChatFromViewer);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
        msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
        msg->nextBlockFast(_PREHASH_ChatData);
        msg->addStringFast(_PREHASH_Message, title.data());
        msg->addU8Fast(_PREHASH_Type, CHAT_TYPE_WHISPER);
        msg->addS32("Channel", streamMetadataAnnounceChannel);

        gAgent.sendReliableMessage();
    }
}

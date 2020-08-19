/**
 * @file llviewerparcelmedia.cpp
 * @brief Handlers for multimedia on a per-parcel basis
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llviewerparcelmedia.h"

#include "llagent.h"
#include "llaudioengine.h"
#include "llmimetypes.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewerregion.h"
#include "llparcel.h"
#include "llviewerparcelmgr.h"
#include "lluuid.h"
#include "message.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmediaautoplay.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
//#include "llfirstuse.h"
#include "llpluginclassmedia.h"
#include "llviewertexture.h"
#include "llcorehttputil.h"

#include "llsdserialize.h"

#include "lltrans.h"
#include "llvieweraudio.h"
#include "fscommon.h"	// <FS:CR> For media filter report_to_nearby_chat

// Local functions
bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel);
bool callback_enable_media_filter(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert(const LLSD& notification, const LLSD& response, LLParcel* parcel);
void callback_media_alert2(const LLSD& notification, const LLSD& response, LLParcel* parcel, bool allow);
void callback_media_alert_single(const LLSD& notification, const LLSD& response, LLParcel* parcel);
bool callback_enable_audio_filter(const LLSD& notification, const LLSD& response, std::string media_url);
void callback_audio_alert(const LLSD& notification, const LLSD& response, std::string media_url);
void callback_audio_alert2(const LLSD& notification, const LLSD& response, std::string media_url, bool allow);
void callback_audio_alert_single(const LLSD& notification, const LLSD& response, std::string media_url);

LLViewerParcelMedia::LLViewerParcelMedia():
mMediaParcelLocalID(0)
	, mMediaLastActionPlay(false)
	, mMediaLastURL()
	, mAudioLastActionPlay(false)
	, mAudioLastURL()
	, mMediaReFilter(false)
	, mMediaFilterAlertActive(false)
	, mQueuedMusic()
	, mCurrentMusic()
	, mMediaQueueEmpty(true)
	, mMusicQueueEmpty(true)
	, mMediaCommandQueue(0)
	, mMediaCommandTime(0)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->setHandlerFunc("ParcelMediaCommandMessage", parcelMediaCommandMessageHandler );
	msg->setHandlerFunc("ParcelMediaUpdate", parcelMediaUpdateHandler );

    // LLViewerParcelMediaAutoPlay will regularly check and autoplay media,
    // might be good idea to just integrate it into LLViewerParcelMedia
    LLSingleton<LLViewerParcelMediaAutoPlay>::getInstance();

	loadDomainFilterList();
}

LLViewerParcelMedia::~LLViewerParcelMedia()
{
	// This needs to be destroyed before global destructor time.
	mMediaImpl = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////
void LLViewerParcelMedia::update(LLParcel* parcel)
{
	if (/*LLViewerMedia::hasMedia()*/ true)
	{
		// we have a player
		if (parcel)
		{
			if(!gAgent.getRegion())
			{
				mMediaRegionID = LLUUID() ;
				stop() ;
				LL_DEBUGS("Media") << "no agent region, bailing out." << LL_ENDL;
				return ;				
			}

			// we're in a parcel
			S32 parcelid = parcel->getLocalID();						

			LLUUID regionid = gAgent.getRegion()->getRegionID();
			bool location_changed = false;
			if (parcelid != mMediaParcelLocalID || regionid != mMediaRegionID)
			{
				LL_DEBUGS("Media") << "New parcel, parcel id = " << parcelid << ", region id = " << regionid << LL_ENDL;
				mMediaParcelLocalID = parcelid;
				mMediaRegionID = regionid;
				location_changed = true;
			}

			std::string mediaUrl = std::string ( parcel->getMediaURL () );
			std::string mediaCurrentUrl = std::string( parcel->getMediaCurrentURL());

			// First use warning
			if(!mediaUrl.empty() && gWarningSettings.getBOOL("FirstStreamingVideo"))
			{
				LLNotifications::instance().add("ParcelCanPlayMedia", LLSD(), LLSD(), 
					boost::bind(callback_play_media, _1, _2, parcel));
				return;
			}

			// if we have a current (link sharing) url, use it instead
			if (mediaCurrentUrl != "" && parcel->getMediaType() == HTTP_CONTENT_TEXT_HTML)
			{
				mediaUrl = mediaCurrentUrl;
			}
			
			LLStringUtil::trim(mediaUrl);
			
			// If no parcel media is playing, nothing left to do
			if(mMediaImpl.isNull())

			{
				// media will be autoplayed by LLViewerParcelMediaAutoPlay
				return;
			}

			// Media is playing...has something changed?
			else if (( mMediaImpl->getMediaURL() != mediaUrl )
				|| ( mMediaImpl->getMediaTextureID() != parcel->getMediaID() )
				|| ( mMediaImpl->getMimeType() != parcel->getMediaType() ))
			{
				// Only play if the media types are the same and parcel stays same.
				if(mMediaImpl->getMimeType() == parcel->getMediaType()
					&& !location_changed)
				{
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						LL_INFOS() << "Filtering media URL." << LL_ENDL;
						filterMediaUrl(parcel);
					}
					else
					{
						play(parcel);
					}
				}

				else
				{
					stop();
				}
			}
		}
		else
		{
			stop();
		}
	}
}

// static
void LLViewerParcelMedia::play(LLParcel* parcel)
{
	LL_INFOS() << "LLViewerParcelMedia::play" << LL_ENDL;

	if (!parcel) return;

	if (!gSavedSettings.getBOOL("AudioStreamingMedia"))
		return;

	std::string media_url = parcel->getMediaURL();
	std::string media_current_url = parcel->getMediaCurrentURL();
	std::string mime_type = parcel->getMediaType();
	LLUUID placeholder_texture_id = parcel->getMediaID();
	U8 media_auto_scale = parcel->getMediaAutoScale();
	U8 media_loop = parcel->getMediaLoop();
	S32 media_width = parcel->getMediaWidth();
	S32 media_height = parcel->getMediaHeight();

	if(mMediaImpl)
	{
		// If the url and mime type are the same, call play again
		if(mMediaImpl->getMediaURL() == media_url 
			&& mMediaImpl->getMimeType() == mime_type
			&& mMediaImpl->getMediaTextureID() == placeholder_texture_id)
		{
			LL_DEBUGS("Media") << "playing with existing url " << media_url << LL_ENDL;

			mMediaImpl->play();
		}
		// Else if the texture id's are the same, navigate and rediscover type
		// MBW -- This causes other state from the previous parcel (texture size, autoscale, and looping) to get re-used incorrectly.
		// It's also not really necessary -- just creating a new instance is fine.
//		else if(mMediaImpl->getMediaTextureID() == placeholder_texture_id)
//		{
//			mMediaImpl->navigateTo(media_url, mime_type, true);
//		}
		else
		{
			// Since the texture id is different, we need to generate a new impl

			// Delete the old one first so they don't fight over the texture.
			mMediaImpl = NULL;
			
			// A new impl will be created below.
		}
	}
	
	// Don't ever try to play if the media type is set to "none/none"
	if(stricmp(mime_type.c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if(!mMediaImpl)
		{
			LL_DEBUGS("Media") << "new media impl with mime type " << mime_type << ", url " << media_url << LL_ENDL;

			// There is no media impl, make a new one
			mMediaImpl = LLViewerMedia::getInstance()->newMediaImpl(
				placeholder_texture_id,
				media_width, 
				media_height, 
				media_auto_scale,
				media_loop);
			mMediaImpl->setIsParcelMedia(true);
			mMediaImpl->navigateTo(media_url, mime_type, true);
		}

		//LLFirstUse::useMedia();

		LLViewerParcelMediaAutoPlay::playStarted();
	}
}

// static
void LLViewerParcelMedia::stop()
{
	if(mMediaImpl.isNull())
	{
		return;
	}
	
	// We need to remove the media HUD if it is up.
	LLViewerMediaFocus::getInstance()->clearFocus();

	// This will unload & kill the media instance.
	mMediaImpl = NULL;
}

// static
void LLViewerParcelMedia::pause()
{
	if(mMediaImpl.isNull())
	{
		return;
	}
	mMediaImpl->pause();
}

// static
void LLViewerParcelMedia::start()
{
	if(mMediaImpl.isNull())
	{
		return;
	}
	mMediaImpl->start();

	//LLFirstUse::useMedia();

	LLViewerParcelMediaAutoPlay::playStarted();
}

// static
void LLViewerParcelMedia::seek(F32 time)
{
	if(mMediaImpl.isNull())
	{
		return;
	}
	mMediaImpl->seek(time);
}

// static
void LLViewerParcelMedia::focus(bool focus)
{
	mMediaImpl->focus(focus);
}

// static
LLPluginClassMediaOwner::EMediaStatus LLViewerParcelMedia::getStatus()
{	
	LLPluginClassMediaOwner::EMediaStatus result = LLPluginClassMediaOwner::MEDIA_NONE;
	
	if(mMediaImpl.notNull() && mMediaImpl->hasMedia())
	{
		result = mMediaImpl->getMediaPlugin()->getStatus();
	}
	
	return result;
}

// static
std::string LLViewerParcelMedia::getMimeType()
{
	return mMediaImpl.notNull() ? mMediaImpl->getMimeType() : LLMIMETypes::getDefaultMimeType();
}

//static 
std::string LLViewerParcelMedia::getURL()
{
	std::string url;
	if(mMediaImpl.notNull())
		url = mMediaImpl->getMediaURL();
	
	if(stricmp(LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaType().c_str(), LLMIMETypes::getDefaultMimeType().c_str()) != 0)
	{
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaCurrentURL();
		
		if (url.empty())
			url = LLViewerParcelMgr::getInstance()->getAgentParcel()->getMediaURL();
	}
	
	return url;
}

//static 
std::string LLViewerParcelMedia::getName()
{
	if(mMediaImpl.notNull())
		return mMediaImpl->getName();
	return "";
}

viewer_media_t LLViewerParcelMedia::getParcelMedia()
{
	return mMediaImpl;
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::parcelMediaCommandMessageHandler(LLMessageSystem *msg, void **)
{
    getInstance()->processParcelMediaCommandMessage(msg);
}

void LLViewerParcelMedia::processParcelMediaCommandMessage( LLMessageSystem *msg)
{
	// extract the agent id
	//	LLUUID agent_id;
	//	msg->getUUID( agent_id );

	U32 flags;
	U32 command;
	F32 time;
	msg->getU32( "CommandBlock", "Flags", flags );
	msg->getU32( "CommandBlock", "Command", command);
	msg->getF32( "CommandBlock", "Time", time );

	if (flags &( (1<<PARCEL_MEDIA_COMMAND_STOP)
				| (1<<PARCEL_MEDIA_COMMAND_PAUSE)
				| (1<<PARCEL_MEDIA_COMMAND_PLAY)
				| (1<<PARCEL_MEDIA_COMMAND_LOOP)
				| (1<<PARCEL_MEDIA_COMMAND_UNLOAD) ))
	{
		// stop
		if( command == PARCEL_MEDIA_COMMAND_STOP )
		{
			if (!mMediaFilterAlertActive)
			{
				stop();
			}
			else
			{
				LL_INFOS() << "Queueing PARCEL_MEDIA_STOP command." << LL_ENDL;
				mMediaCommandQueue = PARCEL_MEDIA_COMMAND_STOP;
			}
		}
		else
		// pause
		if( command == PARCEL_MEDIA_COMMAND_PAUSE )
		{
			if (!mMediaFilterAlertActive)
			{
				pause();
			}
			else
			{
				LL_INFOS() << "Queueing PARCEL_MEDIA_PAUSE command." << LL_ENDL;
				mMediaCommandQueue = PARCEL_MEDIA_COMMAND_PAUSE;
			}
		}
		else
		// play
		if(( command == PARCEL_MEDIA_COMMAND_PLAY ) ||
		   ( command == PARCEL_MEDIA_COMMAND_LOOP ))
		{
			if (getStatus() == LLViewerMediaImpl::MEDIA_PAUSED)
			{
				start();
			}
			else
			{
				//AO: Disallow scripted media option
				if (( !gSavedSettings.getBOOL("PermAllowScriptedMedia")) && (!gSavedSettings.getBOOL("TempAllowScriptedMedia")))
				{
					LL_INFOS() << "Disallowing scripted media." <<LL_ENDL;
				}
				else 
				{
					LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						LL_INFOS() << "PARCEL_MEDIA_COMMAND_PLAY: Filtering media URL." << LL_ENDL;
						filterMediaUrl(parcel);
					}
					else
					{
						play(parcel);
					}
				}
			}
		}
		else
		// unload
		if( command == PARCEL_MEDIA_COMMAND_UNLOAD )
		{
			if (!mMediaFilterAlertActive)
			{
				stop();
			}
			else
			{
				LL_INFOS() << "Queueing PARCEL_MEDIA_UNLOAD command." << LL_ENDL;
				mMediaCommandQueue = PARCEL_MEDIA_COMMAND_UNLOAD;
			}
		}
	}

	if (flags & (1<<PARCEL_MEDIA_COMMAND_TIME))
	{
		if(mMediaImpl.isNull())
		{
			//AO: Disallow scripted media option
			if (( !gSavedSettings.getBOOL("PermAllowScriptedMedia")) && (!gSavedSettings.getBOOL("TempAllowScriptedMedia")))
			{
				LL_INFOS() << "Disallowing scripted media." << LL_ENDL;
			}
			else 
			{
				LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					LL_INFOS() << "PARCEL_MEDIA_COMMAND_TIME: Filtering media URL." << LL_ENDL;
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
		if (!mMediaFilterAlertActive)
		{
			seek(time);
		}
		else
		{
			LL_INFOS() << "Queueing PARCEL_MEDIA_TIME command." << LL_ENDL;
			mMediaCommandQueue = PARCEL_MEDIA_COMMAND_TIME;
			mMediaCommandTime = time;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// static
void LLViewerParcelMedia::parcelMediaUpdateHandler(LLMessageSystem *msg, void **)
{
    getInstance()->processParcelMediaUpdate(msg);
}

void LLViewerParcelMedia::processParcelMediaUpdate( LLMessageSystem *msg)
{
	LLUUID media_id;
	std::string media_url;
	std::string media_type;
	S32 media_width = 0;
	S32 media_height = 0;
	U8 media_auto_scale = FALSE;
	U8 media_loop = FALSE;

	msg->getUUID( "DataBlock", "MediaID", media_id );
	char media_url_buffer[257];
	msg->getString( "DataBlock", "MediaURL", 255, media_url_buffer );
	media_url = media_url_buffer;
	msg->getU8("DataBlock", "MediaAutoScale", media_auto_scale);

	if (msg->has("DataBlockExtended")) // do we have the extended data?
	{
		char media_type_buffer[257];
		msg->getString("DataBlockExtended", "MediaType", 255, media_type_buffer);
		media_type = media_type_buffer;
		msg->getU8("DataBlockExtended", "MediaLoop", media_loop);
		msg->getS32("DataBlockExtended", "MediaWidth", media_width);
		msg->getS32("DataBlockExtended", "MediaHeight", media_height);
	}

	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	BOOL same = FALSE;
	if (parcel)
	{
		same = ((parcel->getMediaURL() == media_url) &&
				(parcel->getMediaType() == media_type) &&
				(parcel->getMediaID() == media_id) &&
				(parcel->getMediaWidth() == media_width) &&
				(parcel->getMediaHeight() == media_height) &&
				(parcel->getMediaAutoScale() == media_auto_scale) &&
				(parcel->getMediaLoop() == media_loop));

		if (!same)
		{
			// temporarily store these new values in the parcel
			parcel->setMediaURL(media_url);
			parcel->setMediaType(media_type);
			parcel->setMediaID(media_id);
			parcel->setMediaWidth(media_width);
			parcel->setMediaHeight(media_height);
			parcel->setMediaAutoScale(media_auto_scale);
			parcel->setMediaLoop(media_loop);

			if (mMediaImpl.notNull())
			{
				if (gSavedSettings.getBOOL("MediaEnableFilter"))
				{
					LL_INFOS() << "Parcel media changed. Filtering media URL." << LL_ENDL;
					filterMediaUrl(parcel);
				}
				else
				{
					play(parcel);
				}
			}
		}
	}
}
// Static
/////////////////////////////////////////////////////////////////////////////////////////
// *TODO: I can not find any active code where this method is called...
void LLViewerParcelMedia::sendMediaNavigateMessage(const std::string& url)
{
	std::string region_url = gAgent.getRegionCapability("ParcelNavigateMedia");
	if (!region_url.empty())
	{
		// send navigate event to sim for link sharing
		LLSD body;
		body["agent-id"] = gAgent.getID();
		body["local-id"] = LLViewerParcelMgr::getInstance()->getAgentParcel()->getLocalID();
		body["url"] = url;

        LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(region_url, body,
            "Media Navigation sent to sim.", "Media Navigation failed to send to sim.");
	}
	else
	{
		LL_WARNS() << "can't get ParcelNavigateMedia capability" << LL_ENDL;
	}

}

/////////////////////////////////////////////////////////////////////////////////////////
// inherited from LLViewerMediaObserver
// virtual 
void LLViewerParcelMedia::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	switch(event)
	{
		case MEDIA_EVENT_DEBUG_MESSAGE:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_DEBUG_MESSAGE " << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CONTENT_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CONTENT_UPDATED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
			// LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_TIME_DURATION_UPDATED, time is " << self->getCurrentTime() << " of " << self->getDuration() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_SIZE_CHANGED " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CURSOR_CHANGED, new cursor is " << self->getCursorName() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_BEGIN " << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: " << self->getNavigateResultString() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PROGRESS_UPDATED, loading at " << self->getProgressPercent() << "%" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: " << self->getStatusText() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LOCATION_CHANGED, new uri is: " << self->getLocation() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAVIGATE_ERROR_PAGE" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_HREF, target is \"" << self->getClickTarget() << "\", uri is " << self->getClickURL() << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is " << self->getClickURL() << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PLUGIN_FAILED_LAUNCH" << LL_ENDL;
		};
		break;
		
		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_NAME_CHANGED" << LL_ENDL;
		};
		break;

		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_CLOSE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_PICK_FILE_REQUEST" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_FILE_DOWNLOAD:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_FILE_DOWNLOAD" << LL_ENDL;
		}
		break;
		
		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event:  MEDIA_EVENT_GEOMETRY_CHANGE, uuid is " << self->getClickUUID() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_AUTH_REQUEST:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_AUTH_REQUEST, url " << self->getAuthURL() << ", realm " << self->getAuthRealm() << LL_ENDL;
		}
		break;

		case MEDIA_EVENT_LINK_HOVERED:
		{
			LL_DEBUGS("Media") <<  "Media event:  MEDIA_EVENT_LINK_HOVERED, hover text is: " << self->getHoverText() << LL_ENDL;
		};
		break;
	};
}

bool callback_play_media(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if ((option == 0) || (option == 1))
	{
		if (option == 1)
		{
			gSavedSettings.setBOOL("AudioStreamingVideo", TRUE);
		}
		if (gSavedSettings.getBOOL("MediaEnableFilter"))
		{
			LLViewerParcelMedia::getInstance()->filterMediaUrl(parcel);
		}
		else
		{
			LLViewerParcelMedia::getInstance()->play(parcel);
		}
	}
	else // option == 2
	{
		gSavedSettings.setBOOL("AudioStreamingVideo", FALSE);
	}
	gWarningSettings.setBOOL("FirstStreamingVideo", FALSE);
	return false;
}

bool callback_enable_media_filter(const LLSD& notification, const LLSD& response, LLParcel* parcel)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	gWarningSettings.setBOOL("FirstMediaFilter", FALSE);
	if (option == 0)
	{
		LLViewerParcelMedia::getInstance()->filterMediaUrl(parcel);
	}
	else // option == 1
	{
		gSavedSettings.setBOOL("MediaEnableFilter", FALSE);
		LLViewerParcelMedia::getInstance()->play(parcel);
	}
	return false;
}

void LLViewerParcelMedia::filterMediaUrl(LLParcel* parcel)
{
	// First use dialog
	if(gWarningSettings.getBOOL("FirstMediaFilter"))
	{
		LLNotifications::instance().add("EnableMediaFilter", LLSD(), LLSD(), 
			boost::bind(callback_enable_media_filter, _1, _2, parcel));
		return;
	}

	LLParcel* currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();

	LL_INFOS() << "Current media: " << mCurrentMedia.getMediaURL() << LL_ENDL;
	LL_INFOS() << "New media: " << parcel->getMediaURL() << LL_ENDL;

	// If there is no alert active, filter the media and flag media
	//  queue empty.
	if (!mMediaFilterAlertActive)
	{
		if (parcel->getMediaURL() == mCurrentMedia.getMediaURL() && !mMediaReFilter)
		{
			LL_INFOS() << "Media URL filter: no active alert, same URL as previous: " +parcel->getMediaURL() << LL_ENDL;
			mCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName() && mMediaLastActionPlay)
			{
				// Only play if we're still there.
				play(parcel);
			}
			mMediaQueueEmpty = true;
			return;
		}

		LL_INFOS() << "Media URL filter: no active alert, filtering new URL: "+parcel->getMediaURL() << LL_ENDL;
		mMediaQueueEmpty = true;
	}
	// If an alert is active, place the media in the media queue if not the same as previous request
	else
	{
		if (!mMediaQueueEmpty)
		{
			if (parcel->getMediaURL() != mQueuedMedia.getMediaURL())
			{
				LL_INFOS() << "Media URL filter: active alert, replacing current queued media URL with: " << mQueuedMedia.getMediaURL() << LL_ENDL;
				mQueuedMedia = *parcel;
				mMediaQueueEmpty = false;
			}
			mMediaCommandQueue = 0;
			return;
		}
		else
		{
			if (parcel->getMediaURL() != mCurrentMedia.getMediaURL())
			{
				LL_INFOS() << "Media URL filter: active alert, nothing queued, adding new queued media URL: " << mQueuedMedia.getMediaURL() << LL_ENDL;
				mQueuedMedia = *parcel;
				mMediaQueueEmpty = false;
			}
			mMediaCommandQueue = 0;
			return;
		}
	}

	std::string media_url = parcel->getMediaURL();
	if (media_url.empty())
	{
		// Treat it as allowed; it'll get stopped elsewhere
		mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			// We haven't moved, so let it run.
			LLViewerParcelMedia::play(parcel);
		}
		return;
	}

	if (media_url == mMediaLastURL)
	{
		// Don't bother the user if all we're doing is repeating
		//  ourselves.
		if (mMediaLastActionPlay)
		{
			// We played it last time...so if we're still there...
			mCurrentMedia = *parcel;
			if (parcel->getName() == currentparcel->getName())
			{
				// The parcel hasn't changed (we didn't
				//  teleport, or move), so play it again, Sam.
				LLViewerParcelMedia::play(parcel);
			}
		}
		return;
	}

	mMediaLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);

	for (LLSD::array_iterator it = mMediaFilterList.beginArray(); it != mMediaFilterList.endArray(); ++it)
	{
		bool found = false;
		std::string listed_domain = (*it)["domain"].asString();
		if (media_url == listed_domain)
		{
			found = true;
		}
		else if (domain.length() >= listed_domain.length())
		{
			size_t pos = domain.rfind(listed_domain);
			if ((pos != std::string::npos)
				&& (pos == domain.length()-listed_domain.length()))
			{
				found = true;
			}
		}
		if (found)
		{
			media_action = (*it)["action"].asString();
			break;
		}
	}
	if (media_action == "allow")
	{
		LL_INFOS("MediaFilter") << "Media filter: URL allowed by whitelist: " << parcel->getMediaURL() << LL_ENDL;
		mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			LLViewerParcelMedia::play(parcel);
		}
		mMediaLastActionPlay = true;
	}
	else if (media_action == "deny")
	{
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentBlocked", format_args));
		mMediaLastActionPlay = false;
	}
	else
	{
		// We haven't been told what to do, and no alert is already
		//  active, so put up the alert and note the fact.
		LLSD args;
		args["MEDIAURL"] = media_url;
		args["MEDIADOMAIN"] = domain;
		mMediaFilterAlertActive = true;
		mCurrentAlertMedia = *parcel;
		LLParcel* pParcel = &mCurrentAlertMedia;
		if (gSavedSettings.getBOOL("MediaFilterSinglePrompt"))
		{
			LLNotifications::instance().add("MediaAlertSingle", args, LLSD(), boost::bind(callback_media_alert_single, _1, _2, pParcel));
		}
		else
		{
			LLNotifications::instance().add("MediaAlert", args, LLSD(), boost::bind(callback_media_alert, _1, _2, pParcel));
		}
	}

	// No need to refilter now.
	mMediaReFilter = false;
}

void callback_media_alert(const LLSD &notification, const LLSD &response, LLParcel* parcel)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	LLSD args;
	bool allow;
	std::string media_url = parcel->getMediaURL();
	std::string domain = LLViewerParcelMedia::getInstance()->extractDomain(media_url);
	if (option == 0) // allow
	{
		args["ACTION"] = LLTrans::getString("MediaFilterActionAllow");
		args["CONDITION"] = LLTrans::getString("MediaFilterConditionAlways");
		args["LCONDITION"] = LLTrans::getString("MediaFilterConditionAlwaysLower");
		allow = true;
	}
	else
	{
		args["ACTION"] = LLTrans::getString("MediaFilterActionDeny");
		args["CONDITION"] = LLTrans::getString("MediaFilterConditionNever");
		args["LCONDITION"] = LLTrans::getString("MediaFilterConditionNeverLower");
		allow = false;
	}
	args["MEDIAURL"] = media_url;
	args["MEDIADOMAIN"] = domain;
	LLNotifications::instance().add("MediaAlert2", args, LLSD(), boost::bind(callback_media_alert2, _1, _2, parcel, allow));
}

void callback_media_alert2(const LLSD &notification, const LLSD &response, LLParcel* parcel, bool allow)
{
	LLParcel* currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	LLViewerParcelMedia* inst = LLViewerParcelMedia::getInstance();

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string media_url = parcel->getMediaURL();
	std::string domain = inst->extractDomain(media_url);

	inst->mMediaLastActionPlay = false;
	if ((option == 0) && allow) //allow now
	{
		inst->mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			inst->play(parcel);
		}
		inst->mMediaLastActionPlay = true;
	}
	else if ((option == 1) && allow) // Whitelist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentDomainAlwaysAllowed", format_args));
		inst->mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			inst->play(parcel);
		}
		inst->mMediaLastActionPlay = true;
	}
	else if ((option == 1) && !allow) //Blacklist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentDomainAlwaysBlocked", format_args));
	}
	else if ((option == 2) && allow) // Whitelist URL
	{
		LLSD newmedia;
		newmedia["domain"] = media_url;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[MEDIAURL]"] = media_url;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentUrlAlwaysAllowed", format_args));
		inst->mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			inst->play(parcel);
		}
		inst->mMediaLastActionPlay = true;
	}
	else if ((option == 2) && !allow) //Blacklist URL
	{
		LLSD newmedia;
		newmedia["domain"] = media_url;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[MEDIAURL]"] = media_url;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentUrlAlwaysBlocked", format_args));
	}

	// We've dealt with the alert, so mark it as inactive.
	inst->mMediaFilterAlertActive = false;

	// Check for any queued alerts.
	if (!inst->mMusicQueueEmpty)
	{
		// There's a queued audio stream. Ask about it.
		inst->filterAudioUrl(inst->mQueuedMusic);
	}
	else if (!inst->mMediaQueueEmpty)
	{
		// There's a queued media stream. Ask about it.
		LLParcel* pParcel = &inst->mQueuedMedia;
		inst->filterMediaUrl(pParcel);
	}
	else if (inst->mMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_STOP command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_PAUSE command." << LL_ENDL;
			inst->pause();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_UNLOAD command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_TIME command." << LL_ENDL;
			inst->seek(inst->mMediaCommandTime);
		}
		inst->mMediaCommandQueue = 0;
	}
}

void callback_media_alert_single(const LLSD &notification, const LLSD &response, LLParcel* parcel)
{
	LLParcel* currentparcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	LLViewerParcelMedia* inst = LLViewerParcelMedia::getInstance();

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string media_url = parcel->getMediaURL();
	std::string domain = inst->extractDomain(media_url);

	inst->mMediaLastActionPlay = false;
	if (option == 0) //allow now
	{
		inst->mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			inst->play(parcel);
		}
		inst->mMediaLastActionPlay = true;	
	}
	else if (option == 2) //Blacklist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentDomainAlwaysBlocked", format_args));
	}
	else if (option == 3) // Whitelist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterMediaContentDomainAlwaysAllowed", format_args));
		inst->mCurrentMedia = *parcel;
		if (parcel->getName() == currentparcel->getName())
		{
			inst->play(parcel);
		}
		inst->mMediaLastActionPlay = true;
	}

	// We've dealt with the alert, so mark it as inactive.
	inst->mMediaFilterAlertActive = false;

	// Check for any queued alerts.
	if (!inst->mMusicQueueEmpty)
	{
		// There's a queued audio stream. Ask about it.
		inst->filterAudioUrl(inst->mQueuedMusic);
	}
	else if (!inst->mMediaQueueEmpty)
	{
		// There's a queued media stream. Ask about it.
		LLParcel* pParcel = &inst->mQueuedMedia;
		inst->filterMediaUrl(pParcel);
	}
	else if (inst->mMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_STOP command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_PAUSE command." << LL_ENDL;
			inst->pause();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_UNLOAD command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_TIME command." << LL_ENDL;
			inst->seek(inst->mMediaCommandTime);
		}
		inst->mMediaCommandQueue = 0;
	}
}

bool callback_enable_audio_filter(const LLSD& notification, const LLSD& response, std::string media_url)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	gWarningSettings.setBOOL("FirstMediaFilter", FALSE);
	if (option == 0)
	{
		LLViewerParcelMedia::getInstance()->filterAudioUrl(media_url);
	}
	else // option == 1
	{
		gSavedSettings.setBOOL("MediaEnableFilter", FALSE);
		if (gAudiop)
		{
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
	}
	return false;
}

void LLViewerParcelMedia::filterAudioUrl(std::string media_url)
{
	// First use dialog
	if(gWarningSettings.getBOOL("FirstMediaFilter"))
	{
		LLNotifications::instance().add("EnableMediaFilter", LLSD(), LLSD(), 
			boost::bind(callback_enable_audio_filter, _1, _2, media_url));
		return;
	}

	// If there is no alert active, filter the media and flag the music
	//  queue empty.
	if (!mMediaFilterAlertActive)
	{
		if (media_url == mCurrentMusic && !mMediaReFilter)
		{
			LL_INFOS() << "Audio URL filter: no active alert, same URL as previous: " << media_url << LL_ENDL;
			// The music hasn't changed, so keep playing if we were.
			if (gAudiop && mAudioLastActionPlay)
			{
				LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
			}
			mMusicQueueEmpty = true;
			return;
		}
		// New music, so flag the queue empty and filter it.
		LL_INFOS() << "Audio URL filter: no active alert, filtering new URL: " << media_url << LL_ENDL;
		mMusicQueueEmpty = true;
	}
	// If an alert is active, place the media url in the music queue
	//  if not the same as previous request.
	else
	{
		if (!mMusicQueueEmpty)
		{
			if (media_url != mQueuedMusic)
			{
				LL_INFOS() << "Audio URL filter: active alert, replacing existing queue with: " << media_url << LL_ENDL;
				mQueuedMusic = media_url;
				mMusicQueueEmpty = false;
			}
			
			return;
		}
		else
		{
			if (media_url != mCurrentMusic)
			{
				LL_INFOS() << "Audio URL filter: active alert, nothing queued, adding queue with: " << media_url << LL_ENDL;
				mQueuedMusic = media_url;
				mMusicQueueEmpty = false;
			}

			return;
		}
	}

	mCurrentMusic = media_url;

	// If the new URL is empty, just play it.
	if (media_url.empty())
	{
		// Treat it as allowed; it'll get stopped elsewhere
		if (gAudiop)
		{
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		return;
	}

	// If this is the same as the last one we asked about, don't bug the
	//  user with it again.
	if (media_url == mAudioLastURL)
	{
		if (mAudioLastActionPlay)
		{
			if (gAudiop)
			{
				LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
			}
		}
		return;
	}

	mAudioLastURL = media_url;

	std::string media_action;
	std::string domain = extractDomain(media_url);

	for (LLSD::array_iterator it = mMediaFilterList.beginArray(); it != mMediaFilterList.endArray(); ++it)
	{
		bool found = false;
		std::string listed_domain = (*it)["domain"].asString();
		if (media_url == listed_domain)
		{
			found = true;
		}
		else if (domain.length() >= listed_domain.length())
		{
			size_t pos = domain.rfind(listed_domain);
			if ((pos != std::string::npos) && 
				(pos == domain.length() - listed_domain.length()))
			{
				found = true;
			}
		}
		if (found)
		{
			media_action = (*it)["action"].asString();
			break;
		}
	}
	if (media_action == "allow")
	{
		if (gAudiop)
		{
			LL_INFOS("MediaFilter") << "Audio filter: URL allowed by whitelist" << LL_ENDL;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		mAudioLastActionPlay = true;
	}
	else if (media_action == "deny")
	{
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentBlocked", format_args));
		LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		mAudioLastActionPlay = false;
	}
	else
	{
		LLSD args;
		args["AUDIOURL"] = media_url;
		args["AUDIODOMAIN"] = domain;
		mMediaFilterAlertActive = true;
		if (gSavedSettings.getBOOL("MediaFilterSinglePrompt"))
		{
			LLNotifications::instance().add("AudioAlertSingle", args, LLSD(), boost::bind(callback_audio_alert_single, _1, _2, media_url));
		}
		else
		{
			LLNotifications::instance().add("AudioAlert", args, LLSD(), boost::bind(callback_audio_alert, _1, _2, media_url));
		}
	}

	// No need to refilter now.
	mMediaReFilter = false;
}

void callback_audio_alert(const LLSD &notification, const LLSD &response, std::string media_url)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	LLSD args;
	bool allow;
	std::string domain = LLViewerParcelMedia::getInstance()->extractDomain(media_url);
	if (option == 0) // allow
	{
		args["ACTION"] = LLTrans::getString("MediaFilterActionAllow");
		args["CONDITION"] = LLTrans::getString("MediaFilterConditionAlways");
		args["LCONDITION"] = LLTrans::getString("MediaFilterConditionAlwaysLower");
		allow = true;
	}
	else
	{
		args["ACTION"] = LLTrans::getString("MediaFilterActionDeny");
		args["CONDITION"] = LLTrans::getString("MediaFilterConditionNever");
		args["LCONDITION"] = LLTrans::getString("MediaFilterConditionNeverLower");
		allow = false;
	}
	args["AUDIOURL"] = media_url;
	args["AUDIODOMAIN"] = domain;
	LLNotifications::instance().add("AudioAlert2", args, LLSD(), boost::bind(callback_audio_alert2, _1, _2, media_url, allow));
}

void callback_audio_alert2(const LLSD &notification, const LLSD &response, std::string media_url, bool allow)
{
	LLViewerParcelMedia* inst = LLViewerParcelMedia::getInstance();

	inst->mMediaFilterAlertActive = true;
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string domain = inst->extractDomain(media_url);

	if ((option == 0) && allow) // allow now
	{
		if (gAudiop)
		{
			inst->mCurrentMusic = media_url;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		inst->mAudioLastActionPlay = true;
	}
	else if ((option == 0) && !allow) //deny now
	{
		if (gAudiop)
		{
			inst->mCurrentMusic = "";
			LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		}
		inst->mAudioLastActionPlay = false;
	}
	else if ((option == 1) && allow) // Whitelist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentDomainAlwaysAllowed", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = media_url;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		inst->mAudioLastActionPlay = true;
	}
	else if ((option == 1) && !allow) //Blacklist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentDomainAlwaysBlocked", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = "";
			LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		}
		inst->mAudioLastActionPlay = false;
	}
	else if ((option == 2) && allow) // Whitelist URL
	{
		LLSD newmedia;
		newmedia["domain"] = media_url;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[MEDIAURL]"] = media_url;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentUrlAlwaysAllowed", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = media_url;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		inst->mAudioLastActionPlay = true;
	}
	else if ((option == 2) && !allow) //Blacklist URL
	{
		LLSD newmedia;
		newmedia["domain"] = media_url;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[MEDIAURL]"] = media_url;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentUrlAlwaysBlocked", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = "";
			LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		}
		inst->mAudioLastActionPlay = false;
	}
	inst->mMediaFilterAlertActive = false;

	// Check for queues 
	if (!inst->mMusicQueueEmpty)
	{
		inst->filterAudioUrl(inst->mQueuedMusic);
	}
	else if (!inst->mMediaQueueEmpty)
	{
		LLParcel* pParcel = &inst->mQueuedMedia;
		inst->filterMediaUrl(pParcel);
	}
	else if (inst->mMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_STOP command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_PAUSE command." << LL_ENDL;
			inst->pause();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_UNLOAD command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_TIME command." << LL_ENDL;
			inst->seek(inst->mMediaCommandTime);
		}
		inst->mMediaCommandQueue = 0;
	}
}

void callback_audio_alert_single(const LLSD &notification, const LLSD &response, std::string media_url)
{
	LLViewerParcelMedia* inst = LLViewerParcelMedia::getInstance();

	inst->mMediaFilterAlertActive = true;
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	std::string domain = inst->extractDomain(media_url);

	if (option == 0) // allow now
	{
		if (gAudiop)
		{
			inst->mCurrentMusic = media_url;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		inst->mAudioLastActionPlay = true;
	}
	else if (option == 1) //deny now
	{
		if (gAudiop)
		{
			inst->mCurrentMusic = "";
			LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		}
		inst->mAudioLastActionPlay = false;
	}
	else if (option == 3) // Whitelist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentDomainAlwaysAllowed", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = media_url;
			LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(media_url);
		}
		inst->mAudioLastActionPlay = true;
	}
	else if (option == 4) //Blacklist domain
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		inst->mMediaFilterList.append(newmedia);
		inst->saveDomainFilterList();
		LLStringUtil::format_map_t format_args;
		format_args["[DOMAIN]"] = domain;
		report_to_nearby_chat(LLTrans::getString("MediaFilterAudioContentDomainAlwaysBlocked", format_args));
		if (gAudiop)
		{
			inst->mCurrentMusic = "";
			LLViewerAudio::getInstance()->stopInternetStreamWithAutoFade();
		}
		inst->mAudioLastActionPlay = false;
	}
	inst->mMediaFilterAlertActive = false;

	// Check for queues 
	if (!inst->mMusicQueueEmpty)
	{
		inst->filterAudioUrl(inst->mQueuedMusic);
	}
	else if (!inst->mMediaQueueEmpty)
	{
		LLParcel* pParcel = &inst->mQueuedMedia;
		inst->filterMediaUrl(pParcel);
	}
	else if (inst->mMediaCommandQueue != 0)
	{
		// There's a queued media command. Process it.
		if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_STOP)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_STOP command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_PAUSE)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_PAUSE command." << LL_ENDL;
			inst->pause();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_UNLOAD)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_UNLOAD command." << LL_ENDL;
			inst->stop();
		}
		else if (inst->mMediaCommandQueue == PARCEL_MEDIA_COMMAND_TIME)
		{
			LL_INFOS() << "Executing Queued PARCEL_MEDIA_TIME command." << LL_ENDL;
			inst->seek(inst->mMediaCommandTime);
		}
		inst->mMediaCommandQueue = 0;
	}
}

void LLViewerParcelMedia::saveDomainFilterList()
{
	const std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	llofstream medialist;
	medialist.open(medialist_filename.c_str());
	LLSDSerialize::toPrettyXML(mMediaFilterList, medialist);
	medialist.close();
}

void LLViewerParcelMedia::loadDomainFilterList()
{
	const std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, "medialist.xml");

	if (LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename.c_str());
		LLSDSerialize::fromXML(mMediaFilterList, medialistFile);
		medialistFile.close();
	}
	else
	{
		LLSD emptyllsd;
		llofstream medialist;
		medialist.open(medialist_filename.c_str());
		LLSDSerialize::toPrettyXML(emptyllsd, medialist);
		medialist.close();
	}
}

std::string LLViewerParcelMedia::extractDomain(std::string url)
{
	// First, find and strip any protocol prefix.
	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		size_t count = url.size() - pos + 2;
		url = url.substr(pos + 2, count);
	}

	// Now, look for a / marking a local part; if there is one,
	//  strip it and anything after.
	pos = url.find("/");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}

	// If there's a user{,:password}@ part, remove it,
	pos = url.find("@");

	if (pos != std::string::npos)
	{
		size_t count = url.size() - pos + 1;
		url = url.substr(pos + 1, count);
	}

	// Finally, find and strip away any port number. This has to be done
	//  after the previous step, or else the extra : for the password,
	//  if supplied, will confuse things.
	pos = url.find(":");

	if (pos != std::string::npos)
	{
		url = url.substr(0, pos);
	}
	
	// Now map the whole thing to lowercase, since domain names aren't
	// case sensitive.
	std::transform(url.begin(), url.end(), url.begin(), ::tolower);

	return url;
}

// TODO: observer
/*
void LLViewerParcelMediaNavigationObserver::onNavigateComplete( const EventType& event_in )
{
	std::string url = event_in.getStringValue();

	if (mCurrentURL != url && ! mFromMessage)
	{
		LLViewerParcelMedia::sendMediaNavigateMessage(url);
	}

	mCurrentURL = url;
	mFromMessage = false;

}
*/

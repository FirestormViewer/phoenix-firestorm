/**
 * @file llviewermedia_streamingaudio.h
 * @author Tofu Linden, Sam Kolb
 * @brief LLStreamingAudio_MediaPlugins implementation - an implementation of the streaming audio interface which is implemented as a client of the media plugin API.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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
#include "linden_common.h"
#include "llpluginclassmedia.h"
#include "llpluginclassmediaowner.h"
#include "llviewermedia.h"

#include "llviewermedia_streamingaudio.h"

#include "llmimetypes.h"
#include "lldir.h"

// <FS> Icecast status sidechannel
#include "llaudioengine.h"
#include "llcorehttputil.h"
#include "llcoros.h"
// </FS>

LLStreamingAudio_MediaPlugins::LLStreamingAudio_MediaPlugins() :
    mMediaPlugin(NULL),
    mStatusValid(false),
    mStatusPollDone(false),
    mStatusProbeFails(0),
    mGain(1.0)
{
    // nothing interesting to do?
    // we will lazily create a media plugin at play-time, if none exists.
}

LLStreamingAudio_MediaPlugins::~LLStreamingAudio_MediaPlugins()
{
    delete mMediaPlugin;
    mMediaPlugin = NULL;
}

void LLStreamingAudio_MediaPlugins::start(const std::string& url)
{
    if (!mMediaPlugin) // lazy-init the underlying media plugin
    {
        mMediaPlugin = initializeMedia("audio/mpeg"); // assumes that whatever media implementation supports mp3 also supports vorbis.
        LL_INFOS() << "streaming audio mMediaPlugin is now " << mMediaPlugin << LL_ENDL;
    }

    if(!mMediaPlugin)
        return;

    if (!url.empty())
    {
        LL_INFOS() << "Starting internet stream: " << url << LL_ENDL;

        mURL = url; // keep original url here for comparison purposes
        resetMetadata(); // <FS/>
        std::string snt_url = url;
        LLStringUtil::trim(snt_url);
        size_t pos = snt_url.find(' ');
        if (pos != std::string::npos)
        {
            // fmod permited having names after the url and people were using it.
            // People label their streams this way, ignore the 'label'.
            snt_url = snt_url.substr(0, pos);
        }
        mMediaPlugin->loadURI(snt_url);
        mMediaPlugin->start();
        LL_INFOS() << "Playing stream..." << LL_ENDL;
    }
    else
    {
        LL_INFOS() << "setting stream to NULL"<< LL_ENDL;
        mURL.clear();
        mMediaPlugin->stop();
        delete mMediaPlugin;
        mMediaPlugin = nullptr;
    }
}

void LLStreamingAudio_MediaPlugins::stop()
{
    LL_INFOS() << "Stopping internet stream." << LL_ENDL;
    if(mMediaPlugin)
    {
        mMediaPlugin->stop();
        delete mMediaPlugin;
        mMediaPlugin = nullptr;
    }

    mURL.clear();

    // <FS:Ansariel> Stream meta data display
    resetMetadata();
}

void LLStreamingAudio_MediaPlugins::pause(int pause)
{
    if(!mMediaPlugin)
        return;

    if(pause)
    {
        LL_INFOS() << "Pausing internet stream." << LL_ENDL;
        mMediaPlugin->pause();
    }
    else
    {
        LL_INFOS() << "Unpausing internet stream." << LL_ENDL;
        mMediaPlugin->start();
    }
}

void LLStreamingAudio_MediaPlugins::update()
{
    if (mMediaPlugin)
        mMediaPlugin->idle();

    // <FS:Ansariel> Stream meta data display
    updateMetadata();
}

int LLStreamingAudio_MediaPlugins::isPlaying()
{
    if (!mMediaPlugin)
        return 0; // stopped

    LLPluginClassMediaOwner::EMediaStatus status =
        mMediaPlugin->getStatus();

    switch (status)
    {
    case LLPluginClassMediaOwner::MEDIA_LOADING: // but not MEDIA_LOADED
    case LLPluginClassMediaOwner::MEDIA_PLAYING:
        return 1; // Active and playing
    case LLPluginClassMediaOwner::MEDIA_PAUSED:
        return 2; // paused
    default:
        return 0; // stopped
    }
}

void LLStreamingAudio_MediaPlugins::setGain(F32 vol)
{
    mGain = vol;

    if(!mMediaPlugin)
        return;

    vol = llclamp(vol, 0.f, 1.f);
    mMediaPlugin->setVolume(vol);
}

F32 LLStreamingAudio_MediaPlugins::getGain()
{
    return mGain;
}

std::string LLStreamingAudio_MediaPlugins::getURL()
{
    return mURL;
}

LLPluginClassMedia* LLStreamingAudio_MediaPlugins::initializeMedia(const std::string& media_type)
{
    LLPluginClassMediaOwner* owner = NULL;
    S32 default_size = 1; // audio-only - be minimal, doesn't matter
    F64 default_zoom = 1.0;
    LLPluginClassMedia* media_source = LLViewerMediaImpl::newSourceFromMediaType(media_type, owner, default_size, default_size, default_zoom);

    if (media_source)
    {
        media_source->setLoop(false); // audio streams are not expected to loop
    }

    return media_source;
}

// <FS:ND> stream metadata from plugin
void LLStreamingAudio_MediaPlugins::updateMetadata() noexcept
{
    if (mMediaPlugin &&
        (mPluginTitle != mMediaPlugin->getTitle() || mPluginArtist != mMediaPlugin->getArtist()))
    {
        mPluginArtist = mMediaPlugin->getArtist();
        mPluginTitle = mMediaPlugin->getTitle();

        // The status sidechannel wins once it has delivered track data for
        // this stream; in-band data only reaches the user until then
        if (!mStatusValid)
        {
            emitMetadata(mPluginArtist, mPluginTitle);
        }
    }

    pollStreamStatus();
}
// </FS:ND>

// <FS> Icecast status sidechannel
void LLStreamingAudio_MediaPlugins::resetMetadata()
{
    mArtist.clear();
    mTitle.clear();
    mPluginArtist.clear();
    mPluginTitle.clear();
    mStatusUrl.clear();
    mMetadata.clear();
    mStatusValid = false;
    mStatusPollDone = false;
    mStatusProbeFails = 0;
    mStatusPollTimer.reset();
}

void LLStreamingAudio_MediaPlugins::emitMetadata(const std::string& artist, const std::string& title)
{
    if (artist == mArtist && title == mTitle)
    {
        return;
    }

    mArtist = artist;
    mTitle = title;
    mMetadata.clear();
    mMetadata["ARTIST"] = mArtist;
    mMetadata["TITLE"] = mTitle;

    LL_INFOS("StreamMetadata") << "Stream metadata changed: artist='" << mArtist
                               << "' title='" << mTitle << "'" << LL_ENDL;

    mMetadataUpdateSignal(mMetadata);
}

// Many stream servers (Icecast, AzuraCast, ...) publish per-mount metadata
// at /status-json.xsl regardless of codec. This is the only reliable track
// source for chained-Ogg streams, whose in-band Vorbis comments libVLC does
// not surface.
void LLStreamingAudio_MediaPlugins::pollStreamStatus()
{
    const F32 FIRST_POLL_DELAY = 5.f;
    const F32 POLL_INTERVAL = 20.f;
    const S32 MAX_PROBE_FAILS = 3;

    if (mURL.empty() || !mMediaPlugin || isPlaying() != 1)
    {
        return;
    }

    if (mStatusProbeFails >= MAX_PROBE_FAILS)
    {
        return; // this server has no reachable status endpoint; stop asking
    }

    F32 threshold = POLL_INTERVAL;
    if (!mStatusPollDone)
    {
        threshold = FIRST_POLL_DELAY;
    }
    if (mStatusPollTimer.getElapsedTimeF32() < threshold)
    {
        return;
    }
    mStatusPollDone = true;
    mStatusPollTimer.reset();

    size_t scheme_end = mURL.find("://");
    if (scheme_end == std::string::npos)
    {
        return;
    }
    size_t path_start = mURL.find('/', scheme_end + 3);
    std::string base;
    std::string mount;
    if (path_start == std::string::npos)
    {
        base = mURL;
    }
    else
    {
        base = mURL.substr(0, path_start);
        mount = mURL.substr(path_start);
    }

    LLSD candidates = LLSD::emptyArray();
    if (!mStatusUrl.empty())
    {
        // A previous poll found the endpoint; keep using it
        candidates.append(mStatusUrl);
    }
    else
    {
        // The status document sits at the Icecast root, which reverse
        // proxies (AzuraCast et al.) mount at a subpath rather than the
        // server root. Walk the mount path upward, most specific first:
        // /listen/station/radio.ogg -> /listen/station/status-json.xsl,
        // /listen/status-json.xsl, /status-json.xsl
        std::string path = mount;
        while (true)
        {
            size_t last_slash = path.rfind('/');
            if (last_slash == std::string::npos)
            {
                break;
            }
            path = path.substr(0, last_slash);
            candidates.append(base + path + "/status-json.xsl");
            if (path.empty())
            {
                break;
            }
        }
        if (candidates.size() == 0)
        {
            candidates.append(base + "/status-json.xsl");
        }
    }

    LLCoros::instance().launch("StreamStatusPoll",
        boost::bind(&LLStreamingAudio_MediaPlugins::streamStatusCoro,
                    this, candidates, mount, mURL));
}

// static
void LLStreamingAudio_MediaPlugins::streamStatusCoro(LLStreamingAudio_MediaPlugins* self,
    LLSD candidates, std::string mount_path, std::string stream_url)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t httpAdapter(
        new LLCoreHttpUtil::HttpCoroutineAdapter("StreamStatusPoll", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);

    LLSD result;
    std::string status_url;
    bool got_status = false;
    for (LLSD::array_const_iterator cand = candidates.beginArray(); cand != candidates.endArray(); ++cand)
    {
        status_url = cand->asString();
        result = httpAdapter->getJsonAndSuspend(httpRequest, status_url);

        LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
        if (httpResults[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_SUCCESS].asBoolean() &&
            result.has("icestats"))
        {
            got_status = true;
            break;
        }
    }

    // The streaming impl outlives the coroutine in normal operation, but
    // guard against shutdown and stream changes while the requests ran
    if (!gAudiop || gAudiop->getStreamingAudioImpl() != (LLStreamingAudioInterface*)self ||
        self->getURL() != stream_url)
    {
        return;
    }

    if (!got_status)
    {
        self->onStreamStatusFailed();
        return;
    }

    // icestats.source is a map for a single mount, an array for several
    LLSD sources = result["icestats"]["source"];
    LLSD source;
    if (sources.isArray())
    {
        for (LLSD::array_const_iterator it = sources.beginArray(); it != sources.endArray(); ++it)
        {
            std::string listenurl = (*it)["listenurl"].asString();
            size_t mount_pos = listenurl.rfind(mount_path);
            if (!mount_path.empty() && mount_pos != std::string::npos &&
                mount_pos + mount_path.size() == listenurl.size())
            {
                source = *it;
                break;
            }
        }
        if (source.isUndefined() && sources.size() == 1)
        {
            source = sources[0];
        }
    }
    else if (sources.isMap())
    {
        source = sources;
    }

    if (source.isUndefined())
    {
        return;
    }

    std::string artist = source["artist"].asString();
    std::string title = source["title"].asString();

    // Some servers publish a combined "Artist - Title" string
    if (artist.empty())
    {
        size_t sep = title.find(" - ");
        if (sep != std::string::npos)
        {
            artist = title.substr(0, sep);
            title = title.substr(sep + 3);
        }
    }

    self->onStreamStatus(artist, title, status_url);
}

void LLStreamingAudio_MediaPlugins::onStreamStatus(const std::string& artist, const std::string& title, const std::string& status_url)
{
    if (mStatusUrl != status_url)
    {
        LL_INFOS("StreamMetadata") << "Using stream status endpoint " << status_url << LL_ENDL;
        mStatusUrl = status_url;
    }
    mStatusProbeFails = 0;

    if (artist.empty() && title.empty())
    {
        return;
    }

    mStatusValid = true;
    emitMetadata(artist, title);
}

void LLStreamingAudio_MediaPlugins::onStreamStatusFailed()
{
    // A previously working endpoint failing may be transient; forget it and
    // re-probe. Repeated full-probe failures disable the sidechannel for
    // this stream.
    mStatusUrl.clear();
    ++mStatusProbeFails;
}
// </FS>

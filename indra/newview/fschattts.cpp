#include "llviewerprecompiledheaders.h"

#include "fschattts.h"

#include "llagent.h"
#include "llchat.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "lleventcoro.h"
#include "llfile.h"
#include "llaudioengine.h"
#include "llviewercontrol.h"
#include "llcoros.h"

#include <queue>

FSChatTTS* FSChatTTS::sInstance = nullptr;

FSChatTTS::FSChatTTS()
{
    sInstance = this;
}

FSChatTTS::~FSChatTTS()
{
    sInstance = nullptr;
}

FSChatTTS& FSChatTTS::instance()
{
    if (!sInstance)
    {
        sInstance = new FSChatTTS();
    }
    return *sInstance;
}

void FSChatTTS::onChatMessage(const LLSD& chat)
{
    if (!gSavedPerAccountSettings.getBOOL("FSVoiceBoxTTSEnabled"))
    {
        return;
    }

    S32 source = chat["source"].asInteger();
    if (source != CHAT_SOURCE_AGENT)
    {
        return;
    }

    LLUUID from_id = chat["from_id"].asUUID();
    if (from_id.isNull())
    {
        return;
    }

    if (from_id == gAgentID)
    {
        return;
    }

    std::string text = chat["message"].asString();
    if (text.empty())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mMutex);
        mQueue.push({text, from_id});
    }

    processQueue();
}

void FSChatTTS::processQueue()
{
    bool expected = false;
    if (!mProcessing.compare_exchange_strong(expected, true))
    {
        return;
    }

    LLCoros::instance().launch("FSChatTTSCoro",
        [this]()
        {
            while (true)
            {
                TTSRequest req;
                {
                    std::lock_guard<std::mutex> lock(mMutex);
                    if (mQueue.empty())
                    {
                        mProcessing = false;
                        return;
                    }
                    req = mQueue.front();
                    mQueue.pop();
                }

                std::string text = req.text;
                LL_DEBUGS("VoiceBoxTTS") << "Speaking: " << text << LL_ENDL;

                doTTS(text);
            }
        });
}

void FSChatTTS::doTTS(const std::string& text)
{
    std::string endpoint = gSavedPerAccountSettings.getString("FSVoiceBoxEndpoint");
    std::string profile_id = gSavedPerAccountSettings.getString("FSVoiceBoxProfileID");
    std::string language = gSavedPerAccountSettings.getString("FSVoiceBoxTTSLanguage");

    if (endpoint.empty() || profile_id.empty())
    {
        LL_WARNS("VoiceBoxTTS") << "Missing endpoint or profile_id" << LL_ENDL;
        return;
    }

    LLSD body;
    body["profile"] = profile_id;
    body["text"] = text;
    body["language"] = language;

    LLCore::HttpHeaders::ptr_t json_headers = std::make_shared<LLCore::HttpHeaders>();
    json_headers->append(HTTP_OUT_HEADER_CONTENT_TYPE, "application/json");
    json_headers->append(HTTP_OUT_HEADER_ACCEPT, "application/json");

    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    auto httpAdapter = std::make_shared<LLCoreHttpUtil::HttpCoroutineAdapter>("VoiceBoxTTS", httpPolicy);

    // Step 1: POST /speak to start generation
    auto httpRequest = std::make_shared<LLCore::HttpRequest>();
    std::string speak_url = endpoint + "/speak";
    LLSD result = httpAdapter->postJsonAndSuspend(httpRequest, speak_url, body, LLCore::HttpOptions::ptr_t(), json_headers);

    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
    if (!status)
    {
        LL_WARNS("VoiceBoxTTS") << "POST /speak failed: " << status.toString() << LL_ENDL;
        return;
    }

    std::string generation_id = result["id"].asString();
    if (generation_id.empty())
    {
        LL_WARNS("VoiceBoxTTS") << "No generation ID in response" << LL_ENDL;
        return;
    }
    LL_INFOS("VoiceBoxTTS") << "Generation started: " << generation_id << LL_ENDL;

    // Step 2: Poll /history/{id} until completed
    std::string history_url = endpoint + "/history/" + generation_id;
    std::string gen_status = result["status"].asString();
    int attempts = 0;

    while (gen_status != "completed" && attempts < 60)
    {
        if (gen_status == "failed" || gen_status == "error")
        {
            LL_WARNS("VoiceBoxTTS") << "Generation failed with status: " << gen_status << LL_ENDL;
            return;
        }

        if (!gen_status.empty())
        {
            LL_DEBUGS("VoiceBoxTTS") << "Generation status: " << gen_status << LL_ENDL;
        }

        llcoro::suspendUntilTimeout(0.5f);

        auto pollRequest = std::make_shared<LLCore::HttpRequest>();
        LLSD pollResult = httpAdapter->getJsonAndSuspend(pollRequest, history_url, LLCore::HttpOptions::ptr_t(), json_headers);

        LLCore::HttpStatus pollStatus = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(pollResult);
        if (!pollStatus)
        {
            LL_WARNS("VoiceBoxTTS") << "Poll failed: " << pollStatus.toString() << LL_ENDL;
            // Continue polling anyway
        }
        else
        {
            gen_status = pollResult["status"].asString();
        }

        attempts++;
    }

    if (gen_status != "completed")
    {
        LL_WARNS("VoiceBoxTTS") << "Generation did not complete after 30s" << LL_ENDL;
        return;
    }

    LL_INFOS("VoiceBoxTTS") << "Generation completed" << LL_ENDL;

    // Step 3: GET /audio/{id} to download WAV
    std::string audio_url = endpoint + "/audio/" + generation_id;

    LLCore::HttpHeaders::ptr_t raw_headers = std::make_shared<LLCore::HttpHeaders>();

    auto audioRequest = std::make_shared<LLCore::HttpRequest>();
    LLSD audioResult = httpAdapter->getRawAndSuspend(audioRequest, audio_url, LLCore::HttpOptions::ptr_t(), raw_headers);

    LLCore::HttpStatus audioStatus = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(audioResult);
    if (!audioStatus)
    {
        LL_WARNS("VoiceBoxTTS") << "Audio download failed: " << audioStatus.toString() << LL_ENDL;
        return;
    }

    const LLSD::Binary& wav_data = audioResult[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
    if (wav_data.empty())
    {
        LL_WARNS("VoiceBoxTTS") << "Empty audio data" << LL_ENDL;
        return;
    }

    // Step 4: Save to sound cache and play
    LLUUID audio_uuid;
    audio_uuid.generate();
    std::string uuid_str;
    audio_uuid.toString(uuid_str);

    std::string wav_path = gDirUtilp->getExpandedFilename(LL_PATH_FS_SOUND_CACHE, uuid_str) + ".dsf";

    {
        llofstream outfile(wav_path, llofstream::binary);
        outfile.write(reinterpret_cast<const char*>(wav_data.data()), wav_data.size());
    }

    if (!gAudiop)
    {
        LL_WARNS("VoiceBoxTTS") << "No audio engine available" << LL_ENDL;
        return;
    }

    LLAudioData *adp = gAudiop->getAudioData(audio_uuid);
    if (adp)
    {
        adp->setHasLocalData(true);
        adp->setHasDecodedData(true);
        adp->setHasCompletedDecode(true);
        adp->setHasDecodeFailed(false);
        adp->setHasWAVLoadFailed(false);

        gAudiop->triggerSound(audio_uuid, gAgent.getID(), 1.0f, LLAudioEngine::AUDIO_TYPE_UI);
        LL_INFOS("VoiceBoxTTS") << "Playing TTS audio" << LL_ENDL;
    }
    else
    {
        LL_WARNS("VoiceBoxTTS") << "Failed to create audio data" << LL_ENDL;
    }
}

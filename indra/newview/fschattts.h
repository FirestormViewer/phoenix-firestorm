#ifndef FS_FSCHATTTS_H
#define FS_FSCHATTTS_H

#include "llsd.h"
#include "lluuid.h"
#include <queue>
#include <mutex>
#include <atomic>
#include <string>

class FSChatTTS
{
public:
    FSChatTTS();
    ~FSChatTTS();

    void onChatMessage(const LLSD& chat);

    static FSChatTTS& instance();

private:
    struct TTSRequest
    {
        std::string text;
        LLUUID from_id;
    };

    void processQueue();
    void doTTS(const std::string& text);

    std::queue<TTSRequest> mQueue;
    std::mutex mMutex;
    std::atomic<bool> mProcessing{ false };

    static FSChatTTS* sInstance;
};

#endif

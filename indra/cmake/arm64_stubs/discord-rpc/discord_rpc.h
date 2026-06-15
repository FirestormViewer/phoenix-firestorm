#ifndef DISCORD_RPC_STUB_H
#define DISCORD_RPC_STUB_H

struct DiscordRichPresence
{
    const char* state;
    const char* details;
    int64_t startTimestamp;
    int64_t endTimestamp;
    const char* largeImageKey;
    const char* largeImageText;
    const char* smallImageKey;
    const char* smallImageText;
    const char* partyId;
    int partySize;
    int partyMax;
    const char* matchSecret;
    const char* joinSecret;
    const char* spectateSecret;
    int8_t instance;
};

struct DiscordUser
{
    const char* userId;
    const char* username;
    const char* discriminator;
    const char* avatar;
};

struct DiscordEventHandlers
{
    void (*ready)(const DiscordUser*);
    void (*disconnected)(int, const char*);
    void (*errored)(int, const char*);
    void (*joinGame)(const char*);
    void (*spectateGame)(const char*);
    void (*joinRequest)(const DiscordUser*);
};

inline void Discord_Initialize(const char*, DiscordEventHandlers*, int, const char*) {}
inline void Discord_Shutdown() {}
inline void Discord_RunCallbacks() {}
inline void Discord_UpdatePresence(const DiscordRichPresence*) {}

#endif

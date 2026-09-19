#ifndef LL_LLVLCSPEAKERFILLCONFIG_H
#define LL_LLVLCSPEAKERFILLCONFIG_H

struct libvlc_media_player_t;
#include <cstdint>
bool configureSpeakerFill(libvlc_media_player_t* player, int layout);
bool configureMusicGain(libvlc_media_player_t* player, int layout, float volume);
bool setMusicGain(libvlc_media_player_t* player, float volume, bool muted = false);
bool configureMusicFade(libvlc_media_player_t* player, float initial);
bool setMusicFade(libvlc_media_player_t* player, float target, float seconds, std::uint32_t serial);
bool musicFadeComplete(libvlc_media_player_t* player, std::uint32_t serial);
bool musicOutputFailed(libvlc_media_player_t* player);

#endif
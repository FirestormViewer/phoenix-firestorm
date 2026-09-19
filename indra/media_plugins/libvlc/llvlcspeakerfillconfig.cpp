#include "llvlcspeakerfillconfig.h"
#include <winsock2.h>
#include <basetsd.h>
typedef SSIZE_T ssize_t;
static inline int poll(struct pollfd* descriptors, unsigned count, int timeout)
{
    return WSAPoll(descriptors, count, timeout);
}
#include <vlc_common.h>
#include <vlc_modules.h>
#include <vlc/vlc.h>
#include <vlc/libvlc_version.h>
#include <cstring>
#include <cmath>
#include <initializer_list>

static_assert(LIBVLC_VERSION_MAJOR == 3 && LIBVLC_VERSION_MINOR == 0 &&
    LIBVLC_VERSION_REVISION == 21, "Reaudit the media player VLC_COMMON_MEMBERS ABI before updating VLC.");

bool configureSpeakerFill(libvlc_media_player_t* player, int layout)
{
    if (!player) return false;
    if (layout == 0) return true;
    if (layout != 21 && layout != 41 && layout != 51 && layout != 71) return false;
    const char* version = libvlc_get_version();
    if (std::strncmp(version, "3.0.21", 6) != 0 || (version[6] != ' ' && version[6] != '\0')) return false;
    if (!module_exists("speakerfill") || !module_exists("speakerfill_output")) return false;
    auto* owner = reinterpret_cast<vlc_object_t*>(player);
    return var_Create(owner, "mmdevice-backend", VLC_VAR_STRING) == VLC_SUCCESS &&
        var_Create(owner, "speakerfill-layout", VLC_VAR_INTEGER) == VLC_SUCCESS &&
        var_SetString(owner, "mmdevice-backend", "speakerfill_output,none") == VLC_SUCCESS &&
        var_SetInteger(owner, "speakerfill-layout", layout) == VLC_SUCCESS &&
        var_SetString(owner, "audio-filter", "speakerfill") == VLC_SUCCESS;
}

bool setMusicGain(libvlc_media_player_t* player, float volume, bool muted)
{
    if (!player || !std::isfinite(volume) || volume < 0.f || volume > 1.f) return false;
    auto* owner = reinterpret_cast<vlc_object_t*>(player);
    return var_SetBool(owner, "music-hard-mute", muted) == VLC_SUCCESS &&
        var_SetFloat(owner, "speakerfill-gain", volume * volume * volume) == VLC_SUCCESS;
}

bool configureMusicGain(libvlc_media_player_t* player, int layout, float volume)
{
    const char* version = libvlc_get_version();
    if (std::strncmp(version, "3.0.21", 6) != 0 || (version[6] != ' ' && version[6] != '\0') ||
        !module_exists("speakerfill") || !configureSpeakerFill(player, layout)) return false;
    auto* owner = reinterpret_cast<vlc_object_t*>(player);
    return var_Create(owner, "speakerfill-gain", VLC_VAR_FLOAT) == VLC_SUCCESS &&
        var_Create(owner, "music-hard-mute", VLC_VAR_BOOL) == VLC_SUCCESS &&
        var_Create(owner, "speakerfill-smooth-gain", VLC_VAR_BOOL) == VLC_SUCCESS &&
        var_Create(owner, "speakerfill-mask", VLC_VAR_INTEGER) == VLC_SUCCESS &&
        var_SetBool(owner, "speakerfill-smooth-gain", true) == VLC_SUCCESS &&
        setMusicGain(player, volume) &&
        var_SetString(owner, "audio-filter", "speakerfill") == VLC_SUCCESS;
}

bool configureMusicFade(libvlc_media_player_t* player, float initial)
{
    if (!player || !std::isfinite(initial) || initial < 0.f || initial > 1.f || !module_exists("speakerfill_output")) return false;
    auto* owner = reinterpret_cast<vlc_object_t*>(player);
    for (const char* name : {"music-fade-command", "music-fade-completed", "music-fade-fence", "music-fade-epoch",
                             "music-clock-pts", "music-clock-epoch", "music-clock-error"})
        if (var_Create(owner, name, VLC_VAR_INTEGER) != VLC_SUCCESS || var_SetInteger(owner, name, 0) != VLC_SUCCESS) return false;
    return var_Create(owner, "music-fade-initial", VLC_VAR_FLOAT) == VLC_SUCCESS &&
        var_SetFloat(owner, "music-fade-initial", initial) == VLC_SUCCESS &&
        var_Create(owner, "mmdevice-backend", VLC_VAR_STRING) == VLC_SUCCESS &&
        var_SetString(owner, "mmdevice-backend", "speakerfill_output,none") == VLC_SUCCESS;
}

bool setMusicFade(libvlc_media_player_t* player, float target, float seconds, std::uint32_t serial)
{
    if (!player || !serial || serial > 0x7fffffff || !std::isfinite(target) || target < 0.f || target > 1.f ||
        !std::isfinite(seconds) || seconds < 0.f || seconds > 60.f) return false;
    const auto command = (static_cast<std::uint64_t>(serial) << 32) |
        (static_cast<std::uint64_t>(std::llround(seconds * 1000.)) << 16) |
        static_cast<std::uint64_t>(std::llround(target * 65535.));
    return var_SetInteger(reinterpret_cast<vlc_object_t*>(player), "music-fade-command", static_cast<int64_t>(command)) == VLC_SUCCESS;
}

bool musicOutputFailed(libvlc_media_player_t* player)
{
    return !player || var_GetInteger(reinterpret_cast<vlc_object_t*>(player), "music-clock-error") != 0;
}

bool musicFadeComplete(libvlc_media_player_t* player, std::uint32_t serial)
{
    if (!player || !serial || musicOutputFailed(player)) return false;
    auto* owner = reinterpret_cast<vlc_object_t*>(player);
    const auto command = var_GetInteger(owner, "music-fade-command");
    const auto epoch = var_GetInteger(owner, "music-clock-epoch");
    const auto fence = var_GetInteger(owner, "music-fade-fence");
    return (static_cast<std::uint64_t>(command) >> 32) == serial &&
        var_GetInteger(owner, "music-fade-completed") == command && epoch > 0 &&
        var_GetInteger(owner, "music-fade-epoch") == epoch && fence > 0 &&
        var_GetInteger(owner, "music-clock-pts") >= fence &&
        var_GetInteger(owner, "music-clock-epoch") == epoch &&
        var_GetInteger(owner, "music-fade-command") == command;
}
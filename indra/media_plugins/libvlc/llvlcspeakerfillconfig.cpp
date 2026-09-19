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
/**
 * @file fmod_codec_opus.h
 * @brief AYAstorm FMOD codec plugin for Ogg Opus.
 *
 * Bundled libfmod lacks Opus decode support, so we extend FMOD via its
 * registerCodec() API. The actual decoder relies on libopus that ships with
 * FMOD SDK (FSBank dependency, but exports full decode + multistream API).
 */

#ifndef LL_FMOD_CODEC_OPUS_H
#define LL_FMOD_CODEC_OPUS_H

// fmod.h pulls in fmod_common.h first, which defines F_CALL and the typedefs
// used by fmod_codec.h. Including fmod_codec.h alone would fail with
// "F_CALL was not declared" / "FMOD_TIMEUNIT does not name a type".
#include "fmodstudio/fmod.h"
#include "fmodstudio/fmod_codec.h"

extern "C" FMOD_CODEC_DESCRIPTION* F_CALL FMODGetCodecDescriptionOpus();

#endif // LL_FMOD_CODEC_OPUS_H

function(speakerfill_source name remote digest)
  set(destination "${SPEAKERFILL_SOURCE_DIR}/${name}")
  if (EXISTS "${destination}")
    file(SHA256 "${destination}" actual)
    if (NOT actual STREQUAL digest)
      message(FATAL_ERROR "Unexpected VLC source contents: ${destination}")
    endif ()
  else ()
    file(DOWNLOAD "https://raw.githubusercontent.com/videolan/vlc/3.0.21/${remote}"
      "${destination}" EXPECTED_HASH "SHA256=${digest}" TLS_VERIFY ON)
  endif ()
endfunction()

file(MAKE_DIRECTORY "${SPEAKERFILL_SOURCE_DIR}/audio_output")
speakerfill_source(wasapi.c modules/audio_output/wasapi.c d485ec637ed2891b794734630942d82598be713f95a2c2beb6c3997efa3a4841)
speakerfill_source(audio_output/mmdevice.h modules/audio_output/mmdevice.h 0dd3863c3e604ac8ca383b309ed44fb4766999c6a213a8a7772afaf9100d8770)
speakerfill_source(vlc_codecs.h include/vlc_codecs.h 2434807ee1aea805a972aff706f87c79a5440c3cfe9cff5d9bb7e72524a6a68a)
file(READ "${SPEAKERFILL_SOURCE_DIR}/wasapi.c" source)

function(speakerfill_replace before after)
  string(FIND "${source}" "${before}" position)
  if (position EQUAL -1)
    message(FATAL_ERROR "VLC speaker-fill patch anchor missing: ${before}")
  endif ()
  string(REPLACE "${before}" "${after}" source "${source}")
  set(source "${source}" PARENT_SCOPE)
endfunction()

speakerfill_replace("#include <vlc_common.h>" "#define N_(text) text\n#include <winsock2.h>\nstatic inline int poll(struct pollfd *descriptors, unsigned count, int timeout)\n{ return WSAPoll(descriptors, count, timeout); }\n#include <vlc_common.h>")
speakerfill_replace("#include <audioclient.h>" "#include <audioclient.h>\n#include <mmdeviceapi.h>\n#include <functiondiscoverykeys_devpkey.h>")
speakerfill_replace("static HRESULT Start(aout_stream_t *s," [=[
static bool SpeakerFillEligible(aout_stream_t *stream)
{
    IPropertyStore *properties = NULL;
    if (FAILED(IMMDevice_OpenPropertyStore((IMMDevice *)stream->owner.device,
                                          STGM_READ, &properties)))
        return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    HRESULT result = IPropertyStore_GetValue(properties, &PKEY_AudioEndpoint_FormFactor, &value);
    bool eligible = SUCCEEDED(result) && value.vt == VT_UI4 &&
                    value.ulVal != Headphones && value.ulVal != Headset;
    PropVariantClear(&value);
    IPropertyStore_Release(properties);
    return eligible;
}

static HRESULT Start(aout_stream_t *s,
]=])
speakerfill_replace("static HRESULT Start(aout_stream_t *s," [=[
static uint32_t SpeakerFillLayout(int64_t layout, uint32_t available)
{
  uint32_t requested = AOUT_CHANS_2_0 | AOUT_CHAN_LFE;
  if (layout == 41 || layout == 51)
  {
    const uint32_t rear = AOUT_CHAN_REARLEFT | AOUT_CHAN_REARRIGHT;
    const uint32_t side = AOUT_CHAN_MIDDLELEFT | AOUT_CHAN_MIDDLERIGHT;
    requested |= (available & rear) == rear ? rear : side;
    if (layout == 51) requested |= AOUT_CHAN_CENTER;
  }
  else if (layout == 71) requested = AOUT_CHANS_7_1;
  else if (layout != 21) return 0;
  return (requested & available) == requested ? requested : 0;
}

static HRESULT Start(aout_stream_t *s,
]=])
speakerfill_replace("    audio_sample_format_t fmt = *pfmt;" [=[
    audio_sample_format_t fmt = *pfmt;
    var_Create(s->obj.parent, "speakerfill-mask", VLC_VAR_INTEGER);
    var_SetInteger(s->obj.parent, "speakerfill-mask", 0);
    uint32_t requested_mask = 0;
    const bool speakerfill = fmt.channel_type == AUDIO_CHANNEL_TYPE_BITMAP &&
        fmt.i_physical_channels == AOUT_CHANS_2_0 && SpeakerFillEligible(s);
]=])
speakerfill_replace("            vlc_ToWave(pwfe, &fmt);\n            buffer_duration = AOUT_MAX_PREPARE_TIME * 10;" [=[
            if (speakerfill)
            {
                audio_sample_format_t mix = fmt;
                hr = IAudioClient_GetMixFormat(sys->client, &pwf_mix);
                if (SUCCEEDED(hr) && vlc_FromWave(pwf_mix, &mix) == 0)
                {
                  requested_mask = SpeakerFillLayout(var_InheritInteger(s, "speakerfill-layout"),
                                     mix.i_physical_channels);
                  if (requested_mask)
                  {
                    fmt = mix;
                    fmt.i_physical_channels = requested_mask;
                    aout_FormatPrepare(&fmt);
                  }
                }
            }
            vlc_ToWave(pwfe, &fmt);
            buffer_duration = AOUT_MAX_PREPARE_TIME * 10;
]=])
speakerfill_replace("    hr = IAudioClient_IsFormatSupported(sys->client, shared_mode,\n                                        pwf, &pwf_closest);" [=[
  hr = IAudioClient_IsFormatSupported(sys->client, shared_mode,
                    pwf, &pwf_closest);
  if (FAILED(hr) && requested_mask)
  {
    requested_mask = 0;
    fmt = *pfmt;
    vlc_ToWave(pwfe, &fmt);
    pwf = &pwfe->Format;
    hr = IAudioClient_IsFormatSupported(sys->client, shared_mode,
                       pwf, &pwf_closest);
  }
]=])
speakerfill_replace("    *pfmt = fmt;" [=[
    *pfmt = fmt;
    if (speakerfill && requested_mask && fmt.i_physical_channels == requested_mask)
        var_SetInteger(s->obj.parent, "speakerfill-mask", fmt.i_physical_channels);
]=])
speakerfill_replace("    set_shortname(\"WASAPI\")" "    set_shortname(\"VLC speaker-fill output\")\n    add_shortcut(\"speakerfill_output\")\n    add_integer(\"speakerfill-layout\", 0, \"Spatial sound\", \"Requested music speaker layout\", false)")
speakerfill_replace("    set_capability(\"aout stream\", 50)" "    set_capability(\"aout stream\", 0)")
file(CONFIGURE OUTPUT "${SPEAKERFILL_SOURCE_DIR}/speakerfill_output.c" CONTENT "${source}" @ONLY)
#include "llvkwidgetimage.h"

#include <openjpeg.h>
#include <algorithm>
#include <cstring>

namespace
{
    struct J2cDecode
    {
        opj_codec_t* codec = nullptr;
        opj_stream_t* stream = nullptr;
        opj_image_t* image = nullptr;
        opj_codestream_info_v2_t* info = nullptr;
        std::span<const std::uint8_t> encoded;
        std::size_t offset = 0;
        char error[256]{};
        ~J2cDecode()
        {
            if (codec) opj_destroy_codec(codec);
            if (stream) opj_stream_destroy(stream);
            if (image) opj_image_destroy(image);
            if (info) opj_destroy_cstr_info(&info);
        }
        static void message(const char* text, void* pointer)
        {
            auto& state = *static_cast<J2cDecode*>(pointer);
            std::strncpy(state.error,text,sizeof(state.error)-1);
        }
        static OPJ_SIZE_T read(void* output,OPJ_SIZE_T size,void* pointer)
        {
            auto& state = *static_cast<J2cDecode*>(pointer);
            const auto count = std::min<std::size_t>(size,state.encoded.size()-state.offset);
            if (!count) return static_cast<OPJ_SIZE_T>(-1);
            std::memcpy(output,state.encoded.data()+state.offset,count);
            state.offset += count;
            return count;
        }
        static OPJ_OFF_T skip(OPJ_OFF_T count,void* pointer)
        {
            auto& state = *static_cast<J2cDecode*>(pointer);
            if (count < 0 || std::uint64_t(count) > state.encoded.size()-state.offset) return -1;
            state.offset += static_cast<std::size_t>(count);
            return count;
        }
        static OPJ_BOOL seek(OPJ_OFF_T offset,void* pointer)
        {
            auto& state = *static_cast<J2cDecode*>(pointer);
            if (offset < 0 || std::uint64_t(offset) > state.encoded.size()) return OPJ_FALSE;
            state.offset = static_cast<std::size_t>(offset);
            return OPJ_TRUE;
        }
    };
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodeJ2c(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    return decodeJ2cPixels(std::move(name),encoded,error);
}

std::shared_ptr<LLVKWidgetImage> LLVKWidgetImage::decodeJ2cPixels(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    error.clear();
    if (encoded.size() < 4 || encoded.size() > 64*1024*1024 || encoded[0] != 0xff || encoded[1] != 0x4f)
    { error = "Invalid or oversized native J2C payload"; return nullptr; }
    J2cDecode state;
    state.encoded = encoded;
    state.codec = opj_create_decompress(OPJ_CODEC_J2K);
    if (!state.codec) { error = "Native J2C decoder allocation failed"; return nullptr; }
    opj_set_error_handler(state.codec,J2cDecode::message,&state);
    opj_set_warning_handler(state.codec,J2cDecode::message,&state);
    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    parameters.cp_reduce = 0;
    if (!opj_setup_decoder(state.codec,&parameters))
    { error = "Native J2C decoder setup failed"; return nullptr; }
    opj_decoder_set_strict_mode(state.codec,OPJ_TRUE);
    state.stream = opj_stream_create(16384,OPJ_TRUE);
    if (!state.stream) { error = "Native J2C stream allocation failed"; return nullptr; }
    opj_stream_set_user_data(state.stream,&state,nullptr);
    opj_stream_set_user_data_length(state.stream,encoded.size());
    opj_stream_set_read_function(state.stream,J2cDecode::read);
    opj_stream_set_skip_function(state.stream,J2cDecode::skip);
    opj_stream_set_seek_function(state.stream,J2cDecode::seek);
    if (!opj_read_header(state.stream,state.codec,&state.image) || !state.image || !state.image->numcomps || state.image->numcomps > 4)
    { error = state.error[0] ? state.error : "Invalid native J2C header/components"; return nullptr; }
    const auto width = state.image->comps[0].w, height = state.image->comps[0].h;
    if (!width || !height || width > 8192 || height > 8192 || std::uint64_t(width)*height > 16*1024*1024)
    { error = "Native J2C exceeds decoded pixel budget"; return nullptr; }
    for (std::uint32_t index = 0; index < state.image->numcomps; ++index)
    {
        const auto& component = state.image->comps[index];
        if (component.w != width || component.h != height || component.prec != 8 || component.sgnd ||
            component.dx != 1 || component.dy != 1 || component.factor)
        { error = "Native local J2C requires equal unsigned 8-bit full-resolution components"; return nullptr; }
    }
    state.info = opj_get_cstr_info(state.codec);
    if (!state.info || state.info->tdx > 8192 || state.info->tdy > 8192 ||
        std::uint64_t(state.info->tw)*state.info->th > 4096)
    { error = "Native J2C tile geometry exceeds supported budget"; return nullptr; }
    if (!opj_decode(state.codec,state.stream,state.image) || !opj_end_decompress(state.codec,state.stream))
    { error = state.error[0] ? state.error : "Native J2C decode incomplete"; return nullptr; }
    for (std::uint32_t index = 0; index < state.image->numcomps; ++index)
        if (!state.image->comps[index].data) { error = "Native J2C component data missing"; return nullptr; }
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = result->mLogicalWidth = width;
    result->mHeight = result->mLogicalHeight = height;
    result->mHasAlpha = state.image->numcomps == 2 || state.image->numcomps == 4;
    result->mPixels.resize(std::size_t(width)*height*4);
    for (std::size_t row = 0; row < height; ++row)
        for (std::size_t column = 0; column < width; ++column)
        {
            const auto source = (height-1-row)*width+column;
            const auto target = (row*width+column)*4;
            for (std::size_t channel = 0; channel < 3; ++channel)
            {
                const auto plane = state.image->numcomps < 3 ? 0 : channel;
                result->mPixels[target+channel] = static_cast<std::uint8_t>(state.image->comps[plane].data[source]);
            }
            result->mPixels[target+3] = result->mHasAlpha ?
                static_cast<std::uint8_t>(state.image->comps[state.image->numcomps-1].data[source]) : 255;
        }
    return result;
}
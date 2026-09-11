#include "llvkwidgetimage.h"

#include <cstdio>
#include <csetjmp>
extern "C" {
#if __has_include(<jpeglib.h>)
#include <jpeglib.h>
#else
#include <jpeglib/jpeglib.h>
#endif
}

namespace
{
    struct JpegDecode
    {
        jpeg_decompress_struct decoder{};
        jpeg_error_mgr errors{};
        std::jmp_buf jump;
        char message[JMSG_LENGTH_MAX]{};
        std::vector<std::uint8_t> pixels, row;
        ~JpegDecode() { if (decoder.mem) jpeg_destroy_decompress(&decoder); }
        static void fail(j_common_ptr decoder)
        {
            auto* state = static_cast<JpegDecode*>(decoder->client_data);
            decoder->err->format_message(decoder,state->message);
            std::longjmp(state->jump,1);
        }
        static void output(j_common_ptr decoder)
        {
            auto* state = static_cast<JpegDecode*>(decoder->client_data);
            decoder->err->format_message(decoder,state->message);
        }
    };
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodeJpeg(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    return decodeJpegPixels(std::move(name),encoded,error);
}

std::shared_ptr<LLVKWidgetImage> LLVKWidgetImage::decodeJpegPixels(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    error.clear();
    if (encoded.size() < 2 || encoded.size() > 64*1024*1024 || encoded[0] != 0xff || encoded[1] != 0xd8)
    { error = "Invalid or oversized native JPEG payload"; return nullptr; }
    auto state = std::make_unique<JpegDecode>();
    state->decoder.err = jpeg_std_error(&state->errors);
    state->errors.error_exit = JpegDecode::fail;
    state->errors.output_message = JpegDecode::output;
    state->decoder.client_data = state.get();
    if (setjmp(state->jump)) { error = state->message; return nullptr; }
    jpeg_create_decompress(&state->decoder);
    jpeg_mem_src(&state->decoder,encoded.data(),static_cast<unsigned long>(encoded.size()));
    if (jpeg_read_header(&state->decoder,TRUE) != JPEG_HEADER_OK)
    { error = "Native JPEG header is incomplete"; return nullptr; }
    const auto width = state->decoder.image_width, height = state->decoder.image_height;
    if (!width || !height || width > 8192 || height > 8192 || std::uint64_t(width)*height > 16*1024*1024)
    { error = "Native JPEG exceeds decoded pixel budget"; return nullptr; }
    state->decoder.out_color_space = JCS_RGB;
    if (!jpeg_start_decompress(&state->decoder) || state->decoder.output_components != 3 ||
        state->decoder.output_width != width || state->decoder.output_height != height)
    { error = "Unexpected native JPEG output layout"; return nullptr; }
    state->pixels.resize(std::size_t(width)*height*4);
    state->row.resize(std::size_t(width)*3);
    while (state->decoder.output_scanline < height)
    {
        const auto rowIndex = height-1-state->decoder.output_scanline;
        JSAMPROW row = state->row.data();
        if (jpeg_read_scanlines(&state->decoder,&row,1) != 1)
        { error = "Native JPEG scanline decode incomplete"; return nullptr; }
        auto* destination = state->pixels.data()+std::size_t(rowIndex)*width*4;
        for (std::size_t column = 0; column < width; ++column)
        {
            destination[column*4] = state->row[column*3];
            destination[column*4+1] = state->row[column*3+1];
            destination[column*4+2] = state->row[column*3+2];
            destination[column*4+3] = 255;
        }
    }
    if (!jpeg_finish_decompress(&state->decoder) || state->errors.num_warnings)
    { error = state->message[0] ? state->message : "Native JPEG contains corrupt data"; return nullptr; }
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = result->mLogicalWidth = width;
    result->mHeight = result->mLogicalHeight = height;
    result->mPixels = std::move(state->pixels);
    return result;
}
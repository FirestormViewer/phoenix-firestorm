#include "llvkwidgetimage.h"

#include <png.h>
#include <cstring>

namespace
{
    struct Decode
    {
        png_structp png = nullptr;
        png_infop info = nullptr;
        std::span<const std::uint8_t> encoded;
        std::size_t offset = 0;
        char error[256]{};
        std::vector<std::uint8_t> pixels;
        std::vector<png_bytep> rows;
        ~Decode() { if (png) png_destroy_read_struct(&png,&info,nullptr); }

        static void fail(png_structp png, png_const_charp message)
        {
            auto* state = static_cast<Decode*>(png_get_error_ptr(png));
            if (state)
            {
                std::strncpy(state->error,message,sizeof(state->error)-1);
                state->error[sizeof(state->error)-1] = '\0';
            }
            png_longjmp(png,1);
        }
        static void warn(png_structp, png_const_charp) {}
        static void read(png_structp png, png_bytep output, png_size_t length)
        {
            auto* state = static_cast<Decode*>(png_get_io_ptr(png));
            if (length > state->encoded.size()-state->offset) png_error(png,"Truncated native PNG payload");
            std::memcpy(output,state->encoded.data()+state->offset,length);
            state->offset += length;
        }
    };
}

std::shared_ptr<const LLVKWidgetImage> LLVKWidgetImage::decodePng(std::string name,
    std::span<const std::uint8_t> encoded, std::string& error)
{
    error.clear();
    if (encoded.size() < 8 || encoded.size() > 64 * 1024 * 1024 || png_sig_cmp(encoded.data(),0,8))
    { error = "Invalid or oversized native PNG payload"; return nullptr; }
    auto state = std::make_unique<Decode>();
    state->encoded = encoded;
    state->png = png_create_read_struct(PNG_LIBPNG_VER_STRING,state.get(),Decode::fail,Decode::warn);
    if (!state->png) { error = "Native PNG decoder allocation failed"; return nullptr; }
    if (setjmp(png_jmpbuf(state->png)))
    { error = state->error; return nullptr; }
    state->info = png_create_info_struct(state->png);
    if (!state->info) { error = "Native PNG metadata allocation failed"; return nullptr; }
    png_set_read_fn(state->png,state.get(),Decode::read);
    png_set_user_limits(state->png,8192,8192);
    png_set_chunk_malloc_max(state->png,4 * 1024 * 1024);
    png_set_chunk_cache_max(state->png,128);
    png_read_info(state->png,state->info);
    const auto width = png_get_image_width(state->png,state->info);
    const auto height = png_get_image_height(state->png,state->info);
    if (!width || !height || std::uint64_t(width)*height > 16 * 1024 * 1024)
    { error = "Native widget image exceeds decoded pixel budget"; return nullptr; }
    const auto type = png_get_color_type(state->png,state->info);
    const auto depth = png_get_bit_depth(state->png,state->info);
    if (type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(state->png);
    if (type == PNG_COLOR_TYPE_GRAY && depth < 8) png_set_expand_gray_1_2_4_to_8(state->png);
    if (type == PNG_COLOR_TYPE_GRAY || type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(state->png);
    const bool transparency = png_get_valid(state->png,state->info,PNG_INFO_tRNS) != 0;
    if (transparency) png_set_tRNS_to_alpha(state->png);
    if (depth < 8) png_set_packing(state->png);
    else if (depth == 16) png_set_strip_16(state->png);
    if (!(type & PNG_COLOR_MASK_ALPHA) && !transparency) png_set_add_alpha(state->png,255,PNG_FILLER_AFTER);
    double gamma = 1.0 / 2.2;
    png_get_gAMA(state->png,state->info,&gamma);
    png_set_gamma(state->png,2.2,gamma);
    png_set_interlace_handling(state->png);
    png_read_update_info(state->png,state->info);
    if (png_get_channels(state->png,state->info) != 4 ||
        png_get_rowbytes(state->png,state->info) != std::size_t(width)*4)
    { error = "Native PNG decoder produced an unexpected pixel layout"; return nullptr; }
    state->pixels.resize(std::size_t(width)*height*4);
    state->rows.resize(height);
    for (std::uint32_t row = 0; row < height; ++row)
        state->rows[row] = state->pixels.data()+std::size_t(height-1-row)*width*4;
    png_read_image(state->png,state->rows.data());
    png_read_end(state->png,nullptr);
    auto result = std::shared_ptr<LLVKWidgetImage>(new LLVKWidgetImage);
    result->mName = std::move(name);
    result->mWidth = width;
    result->mHeight = height;
    result->mPixels = std::move(state->pixels);
    return result;
}
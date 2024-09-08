/*
 * @file llimagewebp.cpp
 * @brief LLImageFormatted glue to encode / decode WebP files.
 *
 * $LicenseInfo:firstyear=2020&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2020, Rye Mutt <rye@alchemyviewer.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $/LicenseInfo$
 */

#include "linden_common.h"
#include "stdtypes.h"
#include "llerror.h"

#include "llimage.h"
#include "llimagewebp.h"

#include "webp/encode.h"
#include "webp/decode.h"

// ---------------------------------------------------------------------------
// LLImageWebP
// ---------------------------------------------------------------------------
LLImageWebP::LLImageWebP()
    : LLImageFormatted(IMG_CODEC_WEBP)
{
}

// Virtual
// Parse WebP image information and set the appropriate
// width, height and component (channel) information.
bool LLImageWebP::updateData()
{
    resetLastError();

    // Check to make sure that this instance has been initialized with data
    if (isBufferInvalid() || (0 == getDataSize()))
    {
        setLastError("Uninitialized instance of LLImageWebP");
        return false;
    }

    WebPBitstreamFeatures config;

    // Decode the WebP data and extract sizing information
    if(WebPGetFeatures(getData(), getDataSize(), &config) != VP8_STATUS_OK)
    {
        setLastError("LLImageWebP data does not have a valid WebP header!");
        return false;
    }

    setSize(config.width, config.height, config.has_alpha ? 4 : 3);

    return true;
}

// Virtual
// Decode an in-memory WebP image into the raw RGB or RGBA format
// used within SecondLife.
bool LLImageWebP::decode(LLImageRaw* raw_image, F32 decode_time)
{
    llassert_always(raw_image);

    resetLastError();

    // Check to make sure that this instance has been initialized with data
    if (isBufferInvalid() || (0 == getDataSize()))
    {
        setLastError("LLImageWebP trying to decode an image with no data!");
        return false;
    }

    WebPDecoderConfig config;
    if(!WebPInitDecoderConfig(&config))
    {
        setLastError("LLImageWebP could not initialize decoder");
        return false;
    }

    // Decode the WebP metadata and extract sizing information
    if (WebPGetFeatures(getData(), getDataSize(), &config.input) != VP8_STATUS_OK)
    {
        setLastError("LLImageWebP data does not have a valid WebP header!");
        return false;
    }

    const bool has_alph = config.input.has_alpha;

    setSize(config.input.width, config.input.height, has_alph ? 4 : 3);

    if (!raw_image->resize(getWidth(), getHeight(), getComponents()))
    {
        setLastError("LLImageWebP failed to resize raw image output buffer");
        return false;
    }

    // Flip the image because SL is opengl
    config.options.flip = true;

    // Specify the desired output colorspace:
    config.output.colorspace = has_alph ? MODE_RGBA : MODE_RGB;

    // Have config.output point to an external buffer:
    config.output.u.RGBA.rgba = (uint8_t*)raw_image->getData();
    config.output.u.RGBA.stride = raw_image->getWidth() * raw_image->getComponents();
    config.output.u.RGBA.size = raw_image->getDataSize();
    config.output.is_external_memory = 1;

    // Decode the WebP data into the raw image
    if(WebPDecode(getData(), getDataSize(), &config) != VP8_STATUS_OK)
    {
        setLastError("LLImageWebP failed to decode WebP image");
        return false;
    }

    WebPFreeDecBuffer(&config.output);

    return true;
}

// Virtual
// Encode the in memory RGB image into WebP format.
bool LLImageWebP::encode(const LLImageRaw* raw_image, F32 encode_time)
{
    llassert_always(raw_image);

    resetLastError();

    if (raw_image->isBufferInvalid() || (0 == raw_image->getDataSize()))
    {
        setLastError("LLImageWebP trying to decode an image with no data!");
        return false;
    }

    //// Image logical size
    const auto datap = raw_image->getData();
    const auto height = raw_image->getHeight();
    const auto width = raw_image->getWidth();
    const auto components = raw_image->getComponents();
    const size_t stride = width * components;

    setSize(width, height, components);

    // Flip image vertically into temporary buffer for encode
    std::unique_ptr<U8[]> tmp_buff = nullptr;
    try
    {
        tmp_buff = std::make_unique<U8[]>(height * stride);
    }
    catch(const std::bad_alloc&)
    {
        setLastError("LLImageWebP::out of memory");
        return false;
    }

    for (U32 i = 0; i < height; i++)
    {
        const U8* row = &datap[(height - 1 - i) * stride];
        memcpy(tmp_buff.get() + i * stride, row, stride);
    }

    U8* encodedData = nullptr;
    size_t encodedSize = 0;
    if (components == 4)
    {
        encodedSize = WebPEncodeLosslessRGBA(tmp_buff.get(), width, height, (int)stride, &encodedData);
    }
    else
    {
        encodedSize = WebPEncodeLosslessRGB(tmp_buff.get(), width, height, (int)stride, &encodedData);
    }

    if (encodedData == nullptr || encodedSize == 0)
    {
        setLastError("LLImageWebP::Failed to encode image");
        return false;
    }

    if (!allocateData((S32)encodedSize))
    {
        setLastError("LLImageWebP::Failed to allocate final buffer for image");
        WebPFree(encodedData);
        return false;
    }

    // Copy
    memcpy(getData(), encodedData, encodedSize);
    WebPFree(encodedData);

    return true;
}


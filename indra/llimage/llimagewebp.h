/*
 * @file llimagewebp.h
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

#ifndef AL_ALIMAGEWEBP_H
#define AL_ALIMAGEWEBP_H

#include "stdtypes.h"
#include "llimage.h"

class LLImageWebP final : public LLImageFormatted
{
protected:
    ~LLImageWebP() = default;

public:
    LLImageWebP();

    /*virtual*/ std::string getExtension() { return std::string("webp"); }
    /*virtual*/ bool updateData();
    /*virtual*/ bool decode(LLImageRaw* raw_image, F32 decode_time);
    /*virtual*/ bool encode(const LLImageRaw* raw_image, F32 encode_time);
};

#endif

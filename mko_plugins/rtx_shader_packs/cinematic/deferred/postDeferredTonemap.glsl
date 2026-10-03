/**
 * @file postDeferredTonemap.glsl
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 *
 * Manikineko "cinematic" RTX pack: derived from the viewer's
 * class1/deferred/postDeferredTonemap.glsl. The viewer's own ACES filmic /
 * Khronos Neutral tonemapping (tonemapUtilF.glsl) is kept unchanged; the
 * RTX block adds an optional cinematic post-grade (contrast, vignette,
 * highlight tint) after tone mapping.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_color;

uniform sampler2D diffuseRect;

in vec2 vary_fragcoord;

#ifdef GAMMA_CORRECT
uniform float gamma;
#endif

vec3 linear_to_srgb(vec3 cl);
vec3 toneMap(vec3 color);

vec3 clampHDRRange(vec3 color);

#ifdef GAMMA_CORRECT
vec3 legacyGamma(vec3 color)
{
    vec3 c = 1. - clamp(color, vec3(0.), vec3(1.));
    c = 1. - pow(c, vec3(gamma)); // s/b inverted already CPU-side

    return c;
}
#endif

#if defined(RTX_CINEMATIC)
vec3 rtxCinematicGrade(vec3 color, vec2 uv)
{
    // Gentle S-curve contrast around mid grey (operates in linear space,
    // applied after the ACES tonemap curve and before sRGB encoding).
    vec3 c = color;
    c = mix(c, c * c * (3.0 - 2.0 * c), 0.35);

    // Subtle cool-shadow / warm-highlight split for depth.
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  #if defined(MKO_VENDOR_NVIDIA)
    c += (0.5 - lum) * vec3(0.012, 0.006, -0.010);
  #elif defined(MKO_VENDOR_AMD)
    c += (0.5 - lum) * vec3(0.010, 0.005, -0.008);
  #else
    c += (0.5 - lum) * vec3(0.008, 0.004, -0.006);
  #endif

  #if defined(MKO_RTX_QUALITY_Cinematic)
    // Soft vignette to frame the shot.
    vec2 d = uv - vec2(0.5);
    float vig = 1.0 - dot(d, d) * 0.55;
    c *= clamp(vig, 0.0, 1.0);
  #endif

    return c;
}
#endif

void main()
{
    //this is the one of the rare spots where diffuseRect contains linear color values (not sRGB)
    vec4 diff = texture(diffuseRect, vary_fragcoord);

#ifndef NO_POST
    diff.rgb = toneMap(diff.rgb);
#else
    diff.rgb = clamp(diff.rgb, vec3(0.0), vec3(1.0));
#endif

#if defined(RTX_CINEMATIC)
#ifndef NO_POST
    diff.rgb = rtxCinematicGrade(diff.rgb, vary_fragcoord);
#endif
#endif

#ifdef GAMMA_CORRECT
    diff.rgb = linear_to_srgb(diff.rgb);

#ifdef LEGACY_GAMMA
    diff.rgb = legacyGamma(diff.rgb);
#endif

#endif

    diff.rgb = clamp(diff.rgb, vec3(0.0), vec3(1.0)); // We should always be 0-1 past this point

    //debugExposure(diff.rgb);
    frag_color = diff;
}

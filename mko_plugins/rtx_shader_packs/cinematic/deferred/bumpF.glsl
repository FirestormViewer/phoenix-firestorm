/**
 * @file bumpF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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
 * class1/deferred/bumpF.glsl with an optional filmic grade block.
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_data[4];

uniform float minimum_alpha;
uniform sampler2D diffuseMap;
uniform sampler2D bumpMap;

in vec3 vary_mat0;
in vec3 vary_mat1;
in vec3 vary_mat2;

in vec4 vertex_color;
in vec2 vary_texcoord0;
in vec3 vary_position;

void mirrorClip(vec3 pos);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);

void main()
{
    mirrorClip(vary_position);

    vec4 col = texture(diffuseMap, vary_texcoord0.xy);

    if(col.a < minimum_alpha)
    {
        discard;
    }
    col *= vertex_color;

    vec3 norm = texture(bumpMap, vary_texcoord0.xy).rgb * 2.0 - 1.0;

    vec3 tnorm = vec3(dot(norm,vary_mat0),
            dot(norm,vary_mat1),
            dot(norm,vary_mat2));

    frag_data[0] = vec4(col.rgb, 0.0);
    frag_data[1] = vertex_color.aaaa; // spec
    //frag_data[1] = vec4(vec3(vertex_color.a), vertex_color.a+(1.0-vertex_color.a)*vertex_color.a); // spec - from former class3 - maybe better, but not so well tested
    vec3 nvn = normalize(tnorm);
    frag_data[2] = encodeNormal(nvn, vertex_color.a, GBUFFER_FLAG_HAS_ATMOS);

#if defined(HAS_EMISSIVE)
    frag_data[3] = vec4(0, 0, 0, 0);
#endif

#if defined(RTX_CINEMATIC)
    // Cinematic RTX grade on the gbuffer albedo: filmic contrast curve with
    // cool highlights, vendor-tuned per GPU (NVIDIA / AMD).
    vec3 c = frag_data[0].rgb;
    c = (c * (c * (c * 0.2272 + 0.3743) + 0.2847) + 0.0228)
        / (c * (c * (c * 0.2416 + 0.3025) + 0.3210) + 0.0247);
  #if defined(MKO_VENDOR_NVIDIA)
    c = mix(c, c * vec3(0.94, 0.97, 1.06), 0.35);
  #elif defined(MKO_VENDOR_AMD)
    c = mix(c, c * vec3(0.96, 0.98, 1.04), 0.30);
  #endif
  #if defined(MKO_RTX_QUALITY_Cinematic)
    c += (1.0 - c) * 0.02; // lifted blacks
  #endif
    frag_data[0].rgb = c;
#endif
}

/**
 * @file starsF.glsl
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
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/

out vec4 frag_data[4];

// Input varyings from Vertex Shader
in vec4 vertex_color;     // Base vertex color (usually white for stars)
in vec2 vary_texcoord0;   // Texture coordinates for diffuseMap
in vec2 screenpos;        // Stable position derived from vertex position (for noise)
in float vary_intensity;  // <AP:WW> Per-star intensity (0.0 to 1.0)
in vec3 vary_worldDir;    // <AP:WW> World direction vector for horizon dimming

// Uniforms from C++
uniform sampler2D diffuseMap; // Star texture (e.g., soft dot)
uniform float blend_factor;
uniform float custom_alpha;   // Global alpha factor (Star Brightness setting)
uniform float time;           // Time for animation

// --- <AP:WW> Procedural Noise Functions ---
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+10.0)*x); }
vec4 taylorInvSqrt(vec4 r) { return 1.79284291400159 - 0.85373472095314 * r; }

vec3 fade(vec3 t) { return t*t*t*(t*(t*6.0-15.0)+10.0); }
vec2 fade(vec2 t) { return t*t*t*(t*(t*6.0-15.0)+10.0); }

float grad_noise(vec2 P)
{
    vec4 Pi = floor(P.xyxy) + vec4(0.0, 0.0, 1.0, 1.0);
    vec4 Pf = fract(P.xyxy) - vec4(0.0, 0.0, 1.0, 1.0);
    Pi = mod289(Pi);
    vec4 ix = Pi.xzxz;
    vec4 iy = Pi.yyww;
    vec4 fx = Pf.xzxz;
    vec4 fy = Pf.yyww;
    vec4 i = permute(permute(ix) + iy);
    vec4 gx = fract(i * (1.0 / 41.0)) * 2.0 - 1.0 ;
    vec4 gy = abs(gx) - 0.5 ;
    vec4 tx = floor(gx + 0.5);
    gx = gx - tx;
    vec2 g00 = vec2(gx.x,gy.x);
    vec2 g10 = vec2(gx.y,gy.y);
    vec2 g01 = vec2(gx.z,gy.z);
    vec2 g11 = vec2(gx.w,gy.w);
    vec4 norm = taylorInvSqrt(vec4(dot(g00,g00), dot(g01,g01), dot(g10,g10), dot(g11,g11)));
    g00 *= norm.x;
    g01 *= norm.y;
    g10 *= norm.z;
    g11 *= norm.w;
    float n00 = dot(g00, vec2(fx.x, fy.x));
    float n10 = dot(g10, vec2(fx.y, fy.y));
    float n01 = dot(g01, vec2(fx.z, fy.z));
    float n11 = dot(g11, vec2(fx.w, fy.w));
    vec2 fade_xy = fade(Pf.xy);
    vec2 n_x = mix(vec2(n00, n01), vec2(n10, n11), fade_xy.x);
    float n_xy = mix(n_x.x, n_x.y, fade_xy.y);
    return 2.3 * n_xy;
}

// Atmospheric twinkle with color scintillation.
// Returns vec4: .rgb = additive color shift, .a = multiplicative alpha factor
vec4 atmospheric_twinkle(vec2 stable_pos, float intensity, float time, float worldDirZ) {
    float noise_scale = 50.0;
    float time_scale = 0.3;
    vec2 noise_coord = stable_pos * noise_scale;
    float noise_r = grad_noise(noise_coord + vec2(time * time_scale, 0.0));
    float noise_g = grad_noise(noise_coord + vec2(time * time_scale * 1.1, 0.1));
    float noise_b = grad_noise(noise_coord + vec2(time * time_scale * 0.9, 0.2));

    float airmass_effect_strength = 2.5;
    float airmass_factor = 1.0 + smoothstep(0.3, 0.0, worldDirZ) * airmass_effect_strength;

    float base_amplitude_alpha = 0.3;
    float base_amplitude_color = 0.04;
    float intensity_factor_amp = smoothstep(0.1, 0.7, intensity);

    float final_amplitude_alpha = base_amplitude_alpha * intensity_factor_amp * airmass_factor;
    float final_amplitude_color = base_amplitude_color * intensity_factor_amp * airmass_factor;

    float twinkle_factor_alpha = 1.0 + (noise_g * final_amplitude_alpha);
    twinkle_factor_alpha = clamp(twinkle_factor_alpha, 0.1, 2.0);

    vec3 twinkle_color_shift = vec3(
        noise_r * final_amplitude_color,
        noise_g * final_amplitude_color,
        noise_b * final_amplitude_color
    );
    twinkle_color_shift = clamp(twinkle_color_shift, -0.2, 0.2);

    return vec4(twinkle_color_shift, twinkle_factor_alpha);
}
// --- </AP:WW> ---

void main()
{
    // 1. Base Texture Color & Alpha
    vec4 tex_color = texture(diffuseMap, vary_texcoord0.xy);
    vec3 base_rgb = tex_color.rgb * vertex_color.rgb;
    float texture_alpha = tex_color.a;

    // 2. Intensity Factor (from C++)
    float intensity_factor = clamp(vary_intensity, 0.0, 1.0);

    // 3. Global Alpha Factor (from C++)
    float global_alpha_factor = smoothstep(0.0, 0.9, custom_alpha);

    // 4. Horizon Atmospheric Extinction (color dependent)
    float worldDirZ = vary_worldDir.z; // 1=zenith, 0=horizon
    const vec3 SCATTERING_COEFFS = vec3(0.17, 0.45, 1.0); // R, G, B
    const float EXTINCTION_STRENGTH = 3.0;
    const float HORIZON_FADE_START_Z = 0.20;
    const float MIN_TRANSMITTANCE = 0.05;
    float altitude_factor = 1.0 - smoothstep(0.0, HORIZON_FADE_START_Z, worldDirZ);
    float optical_depth = altitude_factor * EXTINCTION_STRENGTH;
    vec3 transmittance = exp(-optical_depth * SCATTERING_COEFFS);
    transmittance = max(transmittance, vec3(MIN_TRANSMITTANCE));
    float horizon_luminance_factor = transmittance.g;

    // 5. Twinkle factor & color shift
    vec4 twinkle_result = atmospheric_twinkle(screenpos, intensity_factor, time, worldDirZ);
    vec3 twinkle_color_shift = twinkle_result.rgb;
    float twinkle_alpha_factor = twinkle_result.a;

    // 6. Final Alpha
    float final_alpha = texture_alpha
                      * intensity_factor
                      * global_alpha_factor
                      * horizon_luminance_factor
                      * twinkle_alpha_factor;
    final_alpha = clamp(final_alpha, 0.0, 1.0);

    // 7. Final RGB Color
    vec3 final_rgb = base_rgb * transmittance;
    final_rgb = final_rgb + twinkle_color_shift;
    final_rgb = clamp(final_rgb, 0.0, 1.0);

    // 8. Final Output Color
    vec4 final_color = vec4(final_rgb, final_alpha);

    // --- G-Buffer Outputs ---
    frag_data[1] = vec4(0.0f);
    frag_data[2] = vec4(0.0, 1.0, 0.0, GBUFFER_FLAG_SKIP_ATMOS);

#if defined(HAS_EMISSIVE)
    // Emissive path: black albedo, boosted brightness for bright stars.
    frag_data[0] = vec4(0.0);

    float bloom_intensity_threshold_low = 0.6;
    float bloom_intensity_threshold_high = 0.9;
    float bloom_boost_factor = 1.5;

    float bloom_factor = smoothstep(bloom_intensity_threshold_low,
                                     bloom_intensity_threshold_high,
                                     intensity_factor);

    vec3 emissive_rgb = final_color.rgb;
    if (bloom_boost_factor > 1.0) {
         emissive_rgb += final_color.rgb * bloom_factor * (bloom_boost_factor - 1.0);
    }
    emissive_rgb = max(emissive_rgb, vec3(0.0));

    frag_data[3] = vec4(emissive_rgb, final_color.a);
#else
    frag_data[0] = final_color;
#endif
}


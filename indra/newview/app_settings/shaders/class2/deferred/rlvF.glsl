/**
 *
 * Copyright (c) 2018, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
	out vec4 frag_color;
#else
	#define frag_color gl_FragColor
#endif

VARYING vec2 vary_fragcoord;

uniform sampler2DRect diffuseRect;
uniform sampler2DRect depthMap;
uniform mat4 inv_proj;
uniform vec2 screen_res;

uniform vec4 avPosLocal;
uniform vec4 rlvEffectParam1;
uniform vec4 rlvEffectParam2;
uniform vec4 rlvEffectParam3;

vec4 getPosition_d(vec2 pos_screen, float depth)
{
	vec2 sc = pos_screen.xy * 2.0;
	sc /= screen_res;
	sc -= vec2(1.0, 1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0 * depth - 1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main()
{
	vec2 fragTC = vary_fragcoord.xy;
	float fragDepth = texture2DRect(depthMap, fragTC.xy).r;
	vec3 fragPosLocal = getPosition_d(fragTC, fragDepth).xyz;

	vec3 fragColor = texture2DRect(diffuseRect, fragTC).rgb;
	{
		vec2 blendStart = rlvEffectParam1.xy;
		vec2 blendEnd = rlvEffectParam1.zw;
		vec3 blendColor = rlvEffectParam2.rgb;

		float cutoff = blendStart.y;
		float cutoff2 = blendEnd.y;
		float minAlpha = blendStart.x;
		float maxAlpha = blendEnd.x;

		float distance = length(fragPosLocal.xyz - avPosLocal.xyz);
		if (distance < cutoff)
		{
			discard;
		}
		else if (distance < cutoff2)
		{
			fragColor = mix(fragColor, blendColor, minAlpha + (distance - cutoff) * (maxAlpha - minAlpha) / (cutoff2 - cutoff));
		}
		else
		{
			fragColor = mix(fragColor, blendColor, maxAlpha);
		}
	}

	frag_color.rgb = fragColor;
	frag_color.a = 0.0;
}

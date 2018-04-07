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

vec4 getPosition_d(vec2 pos_screen, float depth)
{
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

void main()
{
	vec2 tc = vary_fragcoord.xy;

	vec4 diffuse = texture2DRect(diffuseRect, tc);
	vec3 col = diffuse.rgb;

	float depth = texture2DRect(depthMap, tc.xy).r;
	vec3 pos = getPosition_d(tc, depth).xyz;

	{
		float cutoff = 20;
		float cutoff2 = 45;
		float minAlpha = 0.0f;
		float maxAlpha = 0.95f;
		vec3 fogColor = vec3(0., 0., 0.);

		float distance = length(pos.xyz - avPosLocal.xyz) ;

		float density = 1;
		float fracDistance = (distance - cutoff) / (cutoff2 - cutoff);
		float visionFactor = clamp((1 - maxAlpha) + (exp(-density * fracDistance) * (maxAlpha - minAlpha)), 0.0, 1.0);
		col = mix(fogColor, col, visionFactor);
		col *= 1 - ((distance - cutoff) / (cutoff2 - cutoff));
	}

	frag_color.rgb = col;
	frag_color.a = 0.0;
}

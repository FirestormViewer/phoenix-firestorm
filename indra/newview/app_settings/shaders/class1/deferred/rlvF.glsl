/**
 *
 * Copyright (c) 2018-2020, Kitty Barnett
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
uniform int  rlvEffectMode;
uniform vec4 rlvEffectParam1;
uniform vec4 rlvEffectParam2;
uniform vec2 rlvEffectParam3;

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

vec3 blur13(sampler2DRect image, vec2 uv, vec2 direction)
{
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;

  color += texture2D(image, uv) * 0.1964825501511404;

  color += texture2D(image, uv + off1) * 0.2969069646728344;
  color += texture2D(image, uv - off1) * 0.2969069646728344;

  color += texture2D(image, uv + off2) * 0.09447039785044732;
  color += texture2D(image, uv - off2) * 0.09447039785044732;

  color += texture2D(image, uv + off3) * 0.010381362401148057;
  color += texture2D(image, uv - off3) * 0.010381362401148057;

  return color.xyz;
}

void main()
{
	vec2 fragTC = vary_fragcoord.st;
	float fragDepth = texture2DRect(depthMap, fragTC).x;
	vec3 fragPosLocal = getPosition_d(fragTC, fragDepth).xyz;
	vec3 fragColor = texture2DRect(diffuseRect, fragTC).rgb;
	float distance = length(fragPosLocal.xyz - avPosLocal.xyz);

	vec2 sphereMinMaxDist = rlvEffectParam1.yw;
	vec2 sphereMinMaxValue = rlvEffectParam1.xz;
	vec3 sphereColour = rlvEffectParam2.rgb;

	// Linear non-branching interpolation of the strength of the sphere effect (replaces if/elseif/else for x < min, min <= x <= max and x > max)
	float effectStrength = mix(sphereMinMaxValue.x, 0, distance < sphereMinMaxDist.x) +
	                       mix(0, sphereMinMaxValue.y - sphereMinMaxValue.x, clamp((distance - sphereMinMaxDist.x) / (sphereMinMaxDist.y - sphereMinMaxDist.x), 0.0, 1.0));

	// *TODO: It should be fine to branch on a uniform?
	switch (rlvEffectMode)
	{
		case 0:		// Blend
			fragColor = mix(fragColor, sphereColour, effectStrength);
			break;
		case 1:		// Blur
			fragColor = blur13(diffuseRect, fragTC, effectStrength * rlvEffectParam3.xy);
			break;
	}

	frag_color.rgb = fragColor;
	frag_color.a = 0.0;
}

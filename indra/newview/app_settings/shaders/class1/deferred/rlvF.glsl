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

uniform int  rlvEffectMode;     // ESphereMode
uniform vec4 rlvEffectParam1;   // Sphere origin (in local coordinates)
uniform vec4 rlvEffectParam2;   // Min/max dist + min/max value
uniform bvec2 rlvEffectParam3;  // Min/max dist extend
uniform vec4 rlvEffectParam4;   // Sphere params (=color when using blend)
uniform vec2 rlvEffectParam5;   // Blur direction (not used for blend)

#define SPHERE_ORIGIN		rlvEffectParam1.xyz
#define SPHERE_DISTMIN		rlvEffectParam2.y
#define SPHERE_DISTMAX		rlvEffectParam2.w
#define SPHERE_DISTEXTEND	rlvEffectParam3
#define SPHERE_VALUEMIN		rlvEffectParam2.x
#define SPHERE_VALUEMAX		rlvEffectParam2.z
#define SPHERE_PARAMS		rlvEffectParam4
#define BLUR_DIRECTION		rlvEffectParam5.xy

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

vec3 blur13(sampler2DRect source, vec2 tc, vec2 direction)
{
  vec4 color = vec4(0.0);
  vec2 off1 = vec2(1.411764705882353) * direction;
  vec2 off2 = vec2(3.2941176470588234) * direction;
  vec2 off3 = vec2(5.176470588235294) * direction;

  color += texture2DRect(source, tc) * 0.1964825501511404;

  color += texture2DRect(source, tc + off1) * 0.2969069646728344;
  color += texture2DRect(source, tc - off1) * 0.2969069646728344;

  color += texture2DRect(source, tc + off2) * 0.09447039785044732;
  color += texture2DRect(source, tc - off2) * 0.09447039785044732;

  color += texture2DRect(source, tc + off3) * 0.010381362401148057;
  color += texture2DRect(source, tc - off3) * 0.010381362401148057;

  return color.xyz;
}

const float pi = 3.14159265;

// http://callumhay.blogspot.com/2010/09/gaussian-blur-shader-glsl.html
vec3 blurVariable(sampler2DRect source, vec2 tc, float kernelSize, vec2 direction, float strength) {
  float numBlurPixelsPerSide = float(kernelSize / 2);

  // Incremental Gaussian Coefficent Calculation (See GPU Gems 3 pp. 877 - 889)
  vec3 incrementalGaussian;
  incrementalGaussian.x = 1.0 / (sqrt(2.0 * pi) * strength);
  incrementalGaussian.y = exp(-0.5 / (strength * strength));
  incrementalGaussian.z = incrementalGaussian.y * incrementalGaussian.y;

  vec4 avgValue = vec4(0.0, 0.0, 0.0, 0.0);
  float coefficientSum = 0.0;

  // Take the central sample first...
  avgValue += texture2DRect(source, tc) * incrementalGaussian.x;
  coefficientSum += incrementalGaussian.x;
  incrementalGaussian.xy *= incrementalGaussian.yz;

  // Go through the remaining 8 vertical samples (4 on each side of the center)
  for (float i = 1.0; i <= numBlurPixelsPerSide; i++) {
	avgValue += texture2DRect(source, tc - i * direction) * incrementalGaussian.x;
	avgValue += texture2DRect(source, tc + i * direction) * incrementalGaussian.x;
	coefficientSum += 2.0 * incrementalGaussian.x;
	incrementalGaussian.xy *= incrementalGaussian.yz;
  }

  return (avgValue / coefficientSum).rgb;
}

vec3 chromaticAberration(sampler2DRect source, vec2 tc, vec2 redDrift, vec2 blueDrift, float strength)
{
	vec3 sourceColor = texture2DRect(source, tc).rgb;

	// Sample the color components
	vec3 driftColor;
	driftColor.r = texture2DRect(source, tc + redDrift).r;
	driftColor.g = sourceColor.g;
	driftColor.b = texture2DRect(source, tc + blueDrift).b;

	// Adjust the strength of the effect
	return mix(sourceColor, driftColor, strength);
}

void main()
{
	vec2 fragTC = vary_fragcoord.st;
	float fragDepth = texture2DRect(depthMap, fragTC).x;
	vec3 fragPosLocal = getPosition_d(fragTC, fragDepth).xyz;
	float distance = length(fragPosLocal.xyz - SPHERE_ORIGIN);

	// Linear non-branching interpolation of the strength of the sphere effect (replaces if/elseif/else for x < min, min <= x <= max and x > max)
	float effectStrength = SPHERE_VALUEMIN + mix(0, SPHERE_VALUEMAX - SPHERE_VALUEMIN, (distance - SPHERE_DISTMIN) / (SPHERE_DISTMAX - SPHERE_DISTMIN));
	effectStrength = mix(effectStrength, mix(0, SPHERE_VALUEMIN, SPHERE_DISTEXTEND.x), distance < SPHERE_DISTMIN);
	effectStrength = mix(effectStrength, mix(0, SPHERE_VALUEMAX, SPHERE_DISTEXTEND.y), distance > SPHERE_DISTMAX);

	vec3 fragColor;
	switch (rlvEffectMode)
	{
		case 0:		// Blend
			fragColor = texture2DRect(diffuseRect, fragTC).rgb;
			fragColor = mix(fragColor, SPHERE_PARAMS.rgb, effectStrength);
			break;
		case 1:		// Blur (fixed)
			fragColor = blur13(diffuseRect, fragTC, effectStrength * BLUR_DIRECTION);
			break;
		case 2:		// Blur (variable)
			fragColor = texture2DRect(diffuseRect, fragTC).rgb;
			fragColor = mix(fragColor, blurVariable(diffuseRect, fragTC, SPHERE_PARAMS.x, BLUR_DIRECTION, effectStrength), effectStrength > 0);
			break;
		case 3:		// ChromaticAberration
			fragColor = chromaticAberration(diffuseRect, fragTC, SPHERE_PARAMS.xy, SPHERE_PARAMS.zw, effectStrength);
			break;
		case 4:		// Pixelate
			{
				effectStrength = sign(effectStrength);
				float pixelWidth = max(1, round(SPHERE_PARAMS.x * effectStrength)); float pixelHeight = max(1, round(SPHERE_PARAMS.y * effectStrength));
				fragTC = vec2(pixelWidth * floor(fragTC.x / pixelWidth), pixelHeight * floor(fragTC.y / pixelHeight));
				fragColor = texture2DRect(diffuseRect, fragTC).rgb;
			}
			break;
	}

	frag_color.rgb = fragColor;
	frag_color.a = 0.0;
}

/** 
 * @file sunLightSSAOF.glsl
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
 

#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

//class 1 -- no shadow, SSAO only

uniform sampler2DRect depthMap;
uniform sampler2DRect normalMap;
uniform sampler2D noiseMap;


// Inputs
uniform float ssao_radius;
uniform float ssao_max_radius;
uniform float ssao_factor;
uniform float ssao_factor_inv;

VARYING vec2 vary_fragcoord;

uniform mat4 inv_proj;
uniform vec2 screen_res;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).r;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

vec2 getKern(int i)
{
	vec2 kern[8];
	// exponentially (^2) distant occlusion samples spread around origin
	kern[0] = vec2(-1.0, 0.0) * 0.125*0.125;
	kern[1] = vec2(1.0, 0.0) * 0.250*0.250;
	kern[2] = vec2(0.0, 1.0) * 0.375*0.375;
	kern[3] = vec2(0.0, -1.0) * 0.500*0.500;
	kern[4] = vec2(0.7071, 0.7071) * 0.625*0.625;
	kern[5] = vec2(-0.7071, -0.7071) * 0.750*0.750;
	kern[6] = vec2(-0.7071, 0.7071) * 0.875*0.875;
	kern[7] = vec2(0.7071, -0.7071) * 1.000*1.000;

	return kern[i];
}

//calculate decreases in ambient lighting when crowded out (SSAO)
float calcAmbientOcclusion(vec4 pos, vec3 norm)
{
	vec2 pos_screen = vary_fragcoord.xy;
	vec3 pos_world = pos.xyz;
	vec2 noise_reflect = texture2D(noiseMap, vary_fragcoord.xy/128.0).xy;
		
  // We treat the first sample as the origin, which definitely doesn't obscure itself thanks to being visible for sampling in the first place.
  float points = 1.0;
	float angle_hidden = 0.0;
		
  // use a kernel scale that diminishes with distance.
  // a scale of less than 32 is just wasting good samples, though.
  float scale = max(32.0, min(ssao_radius / -pos.z, ssao_max_radius));
		
  // it was found that keeping # of samples a constant was the fastest, probably due to compiler optimizations (unrolling?)
	for (int i = 0; i < 8; i++)
	{
    vec2 samppos_screen = pos_screen + scale * reflect(getKern(i), noise_reflect);
			
    // if sample is out-of-screen then give it no weight by continuing
    if (any(lessThan(samppos_screen.xy, vec2(0.0, 0.0))) ||
       any(greaterThan(samppos_screen.xy, vec2(screen_res.xy)))) continue;
			
    vec3 samppos_world = getPosition(samppos_screen).xyz; 
			
    vec3 diff = samppos_world - pos.xyz;
			
    if (diff.z < ssao_factor && diff.z != 0.0)
    {
	float dist = length(diff);
	float angrel = max(0.0, dot(norm.xyz, diff/dist));
	float distrel = 1.0/(1.0+dist*dist);
	float samplehidden = min(angrel, distrel);

	angle_hidden += (samplehidden);
	points += 1.0;
      }
	}
		
  angle_hidden /= points;
		
  float rtn = (1.0 - angle_hidden);
	
  return (rtn * rtn);
}

void main() 
{
	vec2 pos_screen = vary_fragcoord.xy;
	
	//try doing an unproject here
	
	vec4 pos = getPosition(pos_screen);
	
	vec3 norm = texture2DRect(normalMap, pos_screen).xyz;
	norm = (norm.xyz-0.5)*2.0; // unpack norm
		
	frag_color[0] = 1.0;
	frag_color[1] = calcAmbientOcclusion(pos, norm);
	frag_color[2] = 1.0; 
	frag_color[3] = 1.0;
}

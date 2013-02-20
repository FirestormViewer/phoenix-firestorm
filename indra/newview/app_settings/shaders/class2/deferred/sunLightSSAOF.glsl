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

//class 2 -- shadows and SSAO

uniform sampler2DRect depthMap;
uniform sampler2DRect normalMap;
uniform sampler2DShadow shadowMap0;
uniform sampler2DShadow shadowMap1;
uniform sampler2DShadow shadowMap2;
uniform sampler2DShadow shadowMap3;
uniform sampler2DShadow shadowMap4;
uniform sampler2DShadow shadowMap5;
uniform sampler2D noiseMap;


// Inputs
uniform mat4 shadow_matrix[6];
uniform vec4 shadow_clip;
uniform float ssao_radius;
uniform float ssao_max_radius;
uniform float ssao_factor;
uniform float ssao_factor_inv;

VARYING vec2 vary_fragcoord;

uniform mat4 inv_proj;
uniform vec2 screen_res;
uniform vec2 proj_shadow_res;
uniform vec3 sun_dir;

uniform vec2 shadow_res;

uniform float shadow_bias;
uniform float shadow_offset;

uniform float spot_shadow_bias;
uniform float spot_shadow_offset;

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
			
    if (diff.z < ssao_factor // only use sample if it's near enough
	&& diff.z != 0.0     // Z is very quantized at distance, this lessens noise and eliminates dist==0
	)
    {
	float dist = length(diff);
	float angrel = max(0.0, dot(norm.xyz, diff/dist)); // how much the origin faces the sample
	float distrel = 1.0/(1.0+dist*dist); // 'closeness' of origin to sample
		
	// origin is obscured by this sample according to how directly the origin is facing the sample and how close the sample is.  It has to score high on both to be a good occluder.  (a*d) seems the most intuitive way to score, but min(a,d) gives a less localized effect...
	float samplehidden = min(angrel, distrel);
			
	angle_hidden += (samplehidden);
	points += 1.0;
      }
	}
		
  angle_hidden = angle_hidden / points;
	
  float rtn = (1.0 - angle_hidden);

  return (rtn * rtn);
}

float pcfShadow(sampler2DShadow shadowMap, vec4 stc, float scl, vec2 pos_screen)
{
        vec2 recip_shadow_res = 1.0 / shadow_res.xy;
	stc.xyz /= stc.w;
	stc.z += shadow_bias;

	stc.x = floor(stc.x*shadow_res.x + fract(pos_screen.y*0.666666666)) * recip_shadow_res.x;
	float cs = shadow2D(shadowMap, stc.xyz).x;
	
	float shadow = cs;
	
	shadow += shadow2D(shadowMap, stc.xyz+vec3(2.0*recip_shadow_res.x, 1.5*recip_shadow_res.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(1.0*recip_shadow_res.x, -1.5*recip_shadow_res.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-1.0*recip_shadow_res.x, 1.5*recip_shadow_res.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-2.0*recip_shadow_res.x, -1.5*recip_shadow_res.y, 0.0)).x;
	         
        return shadow*0.2;
}

float pcfSpotShadow(sampler2DShadow shadowMap, vec4 stc, float scl, vec2 pos_screen)
{
	stc.xyz /= stc.w;
	stc.z += spot_shadow_bias*scl;
	stc.x = floor(proj_shadow_res.x * stc.x + fract(pos_screen.y*0.666666666)) / proj_shadow_res.x; // snap
		
	float cs = shadow2D(shadowMap, stc.xyz).x;
	float shadow = cs;

	vec2 off = vec2(1.0, 1.5) / proj_shadow_res.xy;
	
	shadow += shadow2D(shadowMap, stc.xyz+vec3(off.x*2.0, off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(off.x, -off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-off.x, off.y, 0.0)).x;
	shadow += shadow2D(shadowMap, stc.xyz+vec3(-off.x*2.0, -off.y, 0.0)).x;

        return shadow*0.2;
}

void main() 
{
	vec2 pos_screen = vary_fragcoord.xy;
	
	//try doing an unproject here
	
	vec4 pos = getPosition(pos_screen);
	
	vec4 nmap4 = texture2DRect(normalMap, pos_screen);
	nmap4 = vec4((nmap4.xyz-0.5)*2.0,nmap4.w); // unpack norm
	float displace = nmap4.w;
	vec3 norm = nmap4.xyz;
	
	/*if (pos.z == 0.0) // do nothing for sky *FIX: REMOVE THIS IF/WHEN THE POSITION MAP IS BEING USED AS A STENCIL
	{
		frag_color = vec4(0.0); // doesn't matter
		return;
	}*/
	
	float shadow = 0.0;
	float dp_directional_light = max(0.0, dot(norm, sun_dir.xyz));

	vec3 shadow_pos = pos.xyz + displace*norm;
	vec3 offset = sun_dir.xyz * (1.0-dp_directional_light);
	
	vec4 spos = vec4(shadow_pos+offset*shadow_offset, 1.0);
	
	if (spos.z > -shadow_clip.w)
	{	
		if (dp_directional_light == 0.0)
		{
			// if we know this point is facing away from the sun then we know it's in shadow without having to do a squirrelly shadow-map lookup
			shadow = 0.0;
		}
		else
		{
			vec4 lpos;

			vec4 near_split = shadow_clip*-0.75;
			vec4 far_split = shadow_clip*-1.25;
			vec4 transition_domain = near_split-far_split;
			float weight = 0.0;

			if (spos.z < near_split.z)
			{
				lpos = shadow_matrix[3]*spos;
				
				float w = 1.0;
				w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;
				shadow += pcfShadow(shadowMap3, lpos, 0.25, pos_screen)*w;
				weight += w;
				shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
			}

			if (spos.z < near_split.y && spos.z > far_split.z)
			{
				lpos = shadow_matrix[2]*spos;
				
				float w = 1.0;
				w -= max(spos.z-far_split.y, 0.0)/transition_domain.y;
				w -= max(near_split.z-spos.z, 0.0)/transition_domain.z;
				shadow += pcfShadow(shadowMap2, lpos, 0.5, pos_screen)*w;
				weight += w;
			}

			if (spos.z < near_split.x && spos.z > far_split.y)
			{
				lpos = shadow_matrix[1]*spos;
				
				float w = 1.0;
				w -= max(spos.z-far_split.x, 0.0)/transition_domain.x;
				w -= max(near_split.y-spos.z, 0.0)/transition_domain.y;
				shadow += pcfShadow(shadowMap1, lpos, 0.75, pos_screen)*w;
				weight += w;
			}

			if (spos.z > far_split.x)
			{
				lpos = shadow_matrix[0]*spos;
								
				float w = 1.0;
				w -= max(near_split.x-spos.z, 0.0)/transition_domain.x;
				
				shadow += pcfShadow(shadowMap0, lpos, 1.0, pos_screen)*w;
				weight += w;
			}
		

			shadow /= weight;

			// take the most-shadowed value out of these two:
			//  * the blurred sun shadow in the light (shadow) map
			//  * an unblurred dot product between the sun and this norm
			// the goal is to err on the side of most-shadow to fill-in shadow holes and reduce artifacting
			shadow = min(shadow, dp_directional_light);
			
			//lpos.xy /= lpos.w*32.0;
			//if (fract(lpos.x) < 0.1 || fract(lpos.y) < 0.1)
			//{
			//	shadow = 0.0;
			//}
			
		}
	}
	else
	{
		// more distant than the shadow map covers
		shadow = 1.0;
	}
	
	frag_color[0] = shadow;
	frag_color[1] = calcAmbientOcclusion(pos, norm);
	
	spos = vec4(shadow_pos+norm*spot_shadow_offset, 1.0);
	
	//spotlight shadow 1
	vec4 lpos = shadow_matrix[4]*spos;
	frag_color[2] = pcfSpotShadow(shadowMap4, lpos, 0.8, pos_screen);
	
	//spotlight shadow 2
	lpos = shadow_matrix[5]*spos;
	frag_color[3] = pcfSpotShadow(shadowMap5, lpos, 0.8, pos_screen);

	//frag_color.rgb = pos.xyz;
	//frag_color.b = shadow;
}

/** 
 * @file postDeferredF.glsl
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

#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect diffuseRect;

uniform mat4 inv_proj;
uniform vec2 screen_res;
uniform float max_cof;
uniform float res_scale;

VARYING vec2 vary_fragcoord;

float weightByColor(vec4 s)
{
  // de-weight dull areas to make highlights 'pop'
  const float base_weight = 0.26; // how much all areas get *regardless* of their 'pop'
  return base_weight + (s.r + s.g + s.b);
}

void dofSample(inout vec4 diff, inout float w, vec2 tc)
{
	vec4 s = texture2DRect(diffuseRect, tc);

	float sweight = weightByColor(s);
		
	// modulate weight by how similarly-focused the origin and sample-point are
	sweight *= 1.0 - abs(s.a - diff.a);
	
	diff.rgb += sweight * s.rgb;
	
	w += sweight;
}

void main() 
{
  vec4 diff = texture2DRect(diffuseRect, vary_fragcoord.xy);
	
  float w = weightByColor(diff);
  diff.rgb *= w;

  float sc = (diff.a*2.0-1.0)*max_cof;
  const float PI = 3.14159265358979323846264;
  
  // sample quite uniformly spaced points within a circle, for a circular 'bokeh'
  sc = abs(sc);
  while (sc > 0.5)
    {
      int its = int(max(1.0,(sc*PI)));
      for (int i=0; i<its; ++i)
	{
	  float ang = sc+i*2*PI/its; // sc is added for rotary perturbance
	  float samp_x = sc*sin(ang);
	  float samp_y = sc*cos(ang);
	  dofSample(diff, w, vary_fragcoord.xy + vec2(samp_x,samp_y));
	}
      sc -= 2.0;
    }
  
  diff.rgb /= w;

  frag_color = diff;
}

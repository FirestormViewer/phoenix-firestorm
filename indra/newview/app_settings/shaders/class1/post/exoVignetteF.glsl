/***********************************
 * exolineartoneF.glsl
 * Provides linear tone mapping functionality.
 * Copyright Geenz Spad, 2012
 ***********************************/
#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect exo_screen;
uniform vec2 screen_res;
uniform vec3 exo_vignette;
VARYING vec2 vary_fragcoord;


void main ()
{
	vec4 diff = texture2DRect(exo_screen, vary_fragcoord.xy);
	vec2 tc = vary_fragcoord / screen_res - 0.5f;
	float vignette = 1 - dot(tc, tc);
	diff.rgb *= clamp(pow(mix(1, vignette * vignette * vignette * vignette * exo_vignette.z, exo_vignette.x), exo_vignette.y), 0, 1);
	frag_color = diff;
}
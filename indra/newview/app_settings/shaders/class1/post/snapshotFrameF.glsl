out vec4 frag_color;

uniform sampler2D diffuseRect;
uniform vec2 screen_res;
uniform vec4 frame_rect; // x, y, width, height (normalized 0->1)
uniform vec3 border_color;
uniform float border_thickness; // in pixels

in vec2 vary_fragcoord;

void main()
{
    vec4 diff = texture(diffuseRect, vary_fragcoord);
    vec2 tc = vary_fragcoord * screen_res;

    // Convert normalized frame_rect to pixel values
    vec4 frame_rect_px = vec4(frame_rect.x * screen_res.x, frame_rect.y * screen_res.y, frame_rect.z * screen_res.x, frame_rect.w * screen_res.y);
    vec4 border_rect_px = vec4(
        (frame_rect.x * screen_res.x) - border_thickness,
        (frame_rect.y * screen_res.y) - border_thickness,
        (frame_rect.z * screen_res.x) + border_thickness,
        (frame_rect.w * screen_res.y) + border_thickness);

    // Desaturate fragments outside the snapshot frame
    if (tc.x < border_rect_px.x || tc.x > border_rect_px.z ||
        tc.y < border_rect_px.y || tc.y > border_rect_px.w)
    {
        // Simple box blur
        vec3 blur_color = vec3(0.0);
        float blur_size = 2;
        int blur_samples = 9;

        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                vec2 offset = vec2(x, y) * blur_size / screen_res;
                blur_color += texture(diffuseRect, vary_fragcoord + offset).rgb;
            }
        }

        blur_color /= float(blur_samples);
        float gray = dot(blur_color, vec3(0.299, 0.587, 0.114));
        diff.rgb = vec3(gray);
    }
    else
    {
        // Draw border around the snapshot frame
        if ((tc.x >= border_rect_px.x && tc.x < frame_rect_px.x) ||
            (tc.x > frame_rect_px.z && tc.x <= border_rect_px.z) ||
            (tc.y >= border_rect_px.y && tc.y < frame_rect_px.y) ||
            (tc.y > frame_rect_px.w && tc.y <= border_rect_px.w))
        {
            diff.rgb = mix(diff.rgb, border_color, 0.5);
        }

    }
    frag_color = diff;
}

-- Manikineko RTX cinematic shader pack manifest (Lua format, loaded by rtx.lua)
-- Real viewer shader overrides (LGPL viewer sources, enhanced under RTX defines).
return {
    name = "Manikineko Cinematic RTX Pack",
    description = "Filmic grade on deferred gbuffer fills, cinematic post-grade (contrast, vignette) after the viewer's ACES tonemap, and boosted bloom extraction. NVIDIA/AMD vendor variants via MKO_VENDOR_* defines.",
    shaders = {
        {
            name = "deferred/diffuseF.glsl",
            type = "fragment",
            file = "deferred/diffuseF.glsl",
            defines = "#define RTX 1\n#define RTX_CINEMATIC 1\n"
        },
        {
            name = "deferred/bumpF.glsl",
            type = "fragment",
            file = "deferred/bumpF.glsl",
            defines = "#define RTX 1\n#define RTX_CINEMATIC 1\n"
        },
        {
            name = "deferred/postDeferredTonemap.glsl",
            type = "fragment",
            file = "deferred/postDeferredTonemap.glsl",
            defines = "#define RTX 1\n#define RTX_CINEMATIC 1\n"
        },
        {
            name = "effects/glowExtractF.glsl",
            type = "fragment",
            file = "effects/glowExtractF.glsl",
            defines = "#define RTX 1\n#define RTX_CINEMATIC 1\n"
        }
    }
}

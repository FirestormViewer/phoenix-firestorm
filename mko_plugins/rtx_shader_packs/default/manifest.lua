-- Manikineko RTX shader pack manifest (Lua format, loaded by rtx.lua)
-- NOTE: shader names must match the viewer's shader FILE names exactly,
-- including the .glsl extension (see llviewershadermgr.cpp mShaderFiles).
return {
    name = "Default RTX Pack",
    description = "Minimal shader pack: overrides the deferred diffuse gbuffer fill with an RTX-tinted variant. Replace with production packs as desired.",
    shaders = {
        {
            name = "deferred/diffuseF.glsl",
            type = "fragment",
            file = "deferred/diffuseF.glsl",
            defines = "#define RTX 1\n"
        }
    }
}

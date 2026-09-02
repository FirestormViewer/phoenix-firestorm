/**
 * @file llui2drouter.cpp
 * @brief The neutral 2D/UI drawing router — dispatch/forwarding (see header).
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (C) 2026, The Vulkanstorm Project
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llui2drouter.h"

namespace
{
    // The bound backend. Null until a backend binds; calls no-op when null so
    // the tree can run before a backend is selected without crashing.
    LLUI2DBackend* s_active = nullptr;
}

namespace LLUI2DRouter
{
    void bind(LLUI2DBackend* backend)       { s_active = backend; }
    LLUI2DBackend* active()                  { return s_active; }
    bool isBound()                           { return s_active != nullptr; }
    bool activeIsVulkan()                    { return s_active && s_active->isVulkan(); }

    void pushTransform()                     { if (s_active) s_active->pushTransform(); }
    void popTransform()                      { if (s_active) s_active->popTransform(); }
    void translate(float x, float y)         { if (s_active) s_active->translate(x, y); }
    void scale(float sx, float sy)           { if (s_active) s_active->scale(sx, sy); }
    void loadIdentityTransform()             { if (s_active) s_active->loadIdentityTransform(); }
    void setColor(float r, float g, float b, float a) { if (s_active) s_active->setColor(r, g, b, a); }
    void setBlend(int blend_type)            { if (s_active) s_active->setBlend(blend_type); }
    void setScissor(int x, int y, int w, int h)       { if (s_active) s_active->setScissor(x, y, w, h); }
    void clearScissor()                      { if (s_active) s_active->clearScissor(); }
    void rect(float l, float t, float r, float b)     { if (s_active) s_active->rect(l, t, r, b); }
    void outlineRect(float l, float t, float r, float b) { if (s_active) s_active->outlineRect(l, t, r, b); }
    void line(float x1, float y1, float x2, float y2) { if (s_active) s_active->line(x1, y1, x2, y2); }
    void rectColored(float l, float t, float r, float b, float cr, float cg, float cb, float ca)
    { if (s_active) s_active->rectColored(l, t, r, b, cr, cg, cb, ca); }
}

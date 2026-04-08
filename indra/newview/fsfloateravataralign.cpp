 /**
 * @file fsfloateravataralign.cpp
 * @brief Floater for rotating the avatar to face cardinal directions or nearest avatar
 * @author chanayane@firestorm
 *
 * $LicenseInfo:firstyear=2026&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, Ayane Lyla
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloateravataralign.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llcharacter.h"
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llrender.h"
#include "llrender2dutils.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llviewermessage.h"

// ============================================================
// FSAvatarAlignBase
// ============================================================

FSAvatarAlignBase::FSAvatarAlignBase(const LLSD& key)
    : LLFloater(key)
{
}

// static
FSAvatarAlignBase* FSAvatarAlignBase::getActive()
{
    if (gSavedSettings.getBOOL("AvatarAlignMini"))
        return LLFloaterReg::getTypedInstance<FSFloaterAvatarAlignMini>("avatar_align_mini");
    return LLFloaterReg::getTypedInstance<FSFloaterAvatarAlign>("avatar_align");
}

void FSAvatarAlignBase::draw()
{
    LLFloater::draw();
    drawCompass();
}

FSAvatarAlignBase::CompassLayout FSAvatarAlignBase::buildCompassLayout() const
{
    LLRect local    = getLocalRect();
    S32 header_h    = getHeaderHeight();
    S32 area_top    = local.mTop    - header_h - getToolbarHeight();
    S32 area_bottom = local.mBottom + getBottomReserve();
    S32 avail_h     = area_top - area_bottom;
    S32 avail_w     = local.getWidth();

    // Vertical clearances: top_clear + bot_clear = overhead.
    //   Full:  27 (N label) + 53 (S label + bearing gap + margin) = 80
    //   Mini:   7 (toolbar gap) + 22 (bearing gap + text) = 29
    // R is always computed with the same overhead so it never jumps discontinuously.
    const bool mini   = isMiniMode();
    S32 top_clear     = mini ?  7 : 27;
    S32 overhead      = mini ? 29 : 80;
    // Full mode needs extra horizontal margin so E/W labels don't clip the floater edge
    S32 h_margin      = mini ?  8 : 28;
    S32 R             = llmax(llmin(avail_w / 2 - h_margin, (avail_h - overhead) / 2), 30);

    CompassLayout lo;
    lo.R            = R;
    lo.cx           = (F32)local.getCenterX();
    lo.cy           = (F32)(area_top - top_clear - R);
    lo.mini_compact = mini && R < 50;  // too small to show bearing/intercardinals
    lo.show_labels  = !mini && R >= 50;
    lo.bearing_y    = mini ? ((S32)lo.cy - R - 10) : ((S32)lo.cy - R - 25);
    lo.sq_half      = mini ? R : (R + 20);
    lo.toggle_label = mini ? "Mini" : "Full";
    return lo;
}

void FSAvatarAlignBase::drawCompass()
{
    const CompassLayout lo = buildCompassLayout();

    mCompassCX = (S32)lo.cx;
    mCompassCY = (S32)lo.cy;
    mCompassR  = lo.R;

    F32 cx = lo.cx;
    F32 cy = lo.cy;
    F32 fR = (F32)lo.R;

    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

    // Background circle
    gGL.color4f(0.08f, 0.08f, 0.10f, 0.90f);
    gl_circle_2d(cx, cy, fR, 64, TRUE);

    // Outer ring
    gGL.color4f(0.45f, 0.45f, 0.45f, 1.f);
    gl_circle_2d(cx, cy, fR, 64, FALSE);

    // Inner ring at half radius
    gGL.color4f(0.25f, 0.25f, 0.25f, 1.f);
    gl_circle_2d(cx, cy, fR * 0.5f, 48, FALSE);

    // Tick marks at 45° intervals
    gGL.begin(LLRender::LINES);
    gGL.color4f(0.35f, 0.35f, 0.35f, 1.f);
    for (S32 i = 0; i < 8; ++i)
    {
        F32 a  = i * F_PI / 4.f;
        F32 sa = sinf(a), ca = cosf(a);
        gGL.vertex2f(cx + (fR - 7.f) * sa, cy + (fR - 7.f) * ca);
        gGL.vertex2f(cx +  fR        * sa, cy +  fR        * ca);
    }
    gGL.end();

    // Hover highlight: a semi-transparent light overlay drawn on whichever zone the mouse is over
    if (mHoverOctant == -2)
    {
        // Center zone: fill the inner circle
        gGL.color4f(1.f, 1.f, 1.f, 0.18f);
        gl_circle_2d(cx, cy, fR * 0.25f, 24, TRUE);
    }
    else if (mHoverOctant >= 0)
    {
        // Outer ring: fill the 45° pie slice for the hovered octant.
        // The slice is built as a triangle fan between the inner and outer radius.
        F32 hoverAngle = mHoverOctant * 45.f * DEG_TO_RAD; // center angle of the slice
        F32 halfSpan   = F_PI / 8.f;                       // half of 45° in radians
        F32 innerR     = fR * 0.25f;                       // inner boundary (center zone edge)
        S32 segs       = 8;                                // subdivisions for a smooth arc
        gGL.begin(LLRender::TRIANGLES);
        gGL.color4f(1.f, 1.f, 1.f, 0.13f);
        for (S32 i = 0; i < segs; ++i)
        {
            // Two adjacent arc angles for this subdivision
            F32 a0 = hoverAngle - halfSpan + (F32)i       * (2.f * halfSpan / segs);
            F32 a1 = hoverAngle - halfSpan + (F32)(i + 1) * (2.f * halfSpan / segs);
            // Two triangles forming a trapezoid between innerR and fR for this strip
            gGL.vertex2f(cx + innerR * sinf(a0), cy + innerR * cosf(a0));
            gGL.vertex2f(cx + fR     * sinf(a0), cy + fR     * cosf(a0));
            gGL.vertex2f(cx + fR     * sinf(a1), cy + fR     * cosf(a1));
            gGL.vertex2f(cx + innerR * sinf(a0), cy + innerR * cosf(a0));
            gGL.vertex2f(cx + fR     * sinf(a1), cy + fR     * cosf(a1));
            gGL.vertex2f(cx + innerR * sinf(a1), cy + innerR * cosf(a1));
        }
        gGL.end();
    }

    // Draw the 4 cardinal arms as kite (diamond) shapes.
    // Each arm points outward from center, colored by convention (red=North, white=South, grey=E/W).
    struct ArmDef { F32 angle_deg; F32 r, g, b; };
    static const ArmDef ARMS[] = {
        {   0.f, 0.85f, 0.15f, 0.15f },  // North: red
        { 180.f, 0.85f, 0.85f, 0.85f },  // South: white
        {  90.f, 0.65f, 0.65f, 0.65f },  // East:  grey
        { 270.f, 0.65f, 0.65f, 0.65f },  // West:  grey
    };

    F32 tipR  = fR - 4.f;    // how far out the arm tip reaches
    F32 sideR = fR * 0.245f; // half-width of the arm at its base (center of compass)

    gGL.begin(LLRender::TRIANGLES);
    for (const auto& arm : ARMS)
    {
        F32 a  = arm.angle_deg * DEG_TO_RAD;
        F32 al = a - F_PI_BY_TWO; // left side angle
        F32 ar = a + F_PI_BY_TWO; // right side angle

        // tip point and the two base corners of the kite
        F32 tx = cx + tipR  * sinf(a);   F32 ty = cy + tipR  * cosf(a);
        F32 lx = cx + sideR * sinf(al);  F32 ly = cy + sideR * cosf(al);
        F32 rx = cx + sideR * sinf(ar);  F32 ry = cy + sideR * cosf(ar);

        // two triangles: tip->center->left, tip->right->center
        gGL.color4f(arm.r, arm.g, arm.b, 1.f);
        gGL.vertex2f(tx, ty); gGL.vertex2f(cx, cy); gGL.vertex2f(lx, ly);
        gGL.vertex2f(tx, ty); gGL.vertex2f(rx, ry); gGL.vertex2f(cx, cy);
    }
    gGL.end();

    // Intercardinal arms (NE, SE, SW, NW): same kite shape as cardinals but shorter and narrower.
    // Skipped entirely when the floater is in compact mini mode (too small to be legible).
    F32 tipR2  = fR * 0.62f; // shorter reach than cardinal arms
    F32 sideR2 = fR * 0.10f; // narrower base
    static const F32 INTER_ANGLES[] = { 45.f, 135.f, 225.f, 315.f };

    if (!lo.mini_compact)
    {
        gGL.begin(LLRender::TRIANGLES);
        gGL.color4f(0.55f, 0.55f, 0.55f, 1.f);
        for (F32 angle_deg : INTER_ANGLES)
        {
            F32 a  = angle_deg * DEG_TO_RAD;
            F32 al = a - F_PI_BY_TWO;
            F32 ar = a + F_PI_BY_TWO;

            // tip and base corners, same geometry as cardinal arms
            F32 tx = cx + tipR2  * sinf(a);   F32 ty = cy + tipR2  * cosf(a);
            F32 lx = cx + sideR2 * sinf(al);  F32 ly = cy + sideR2 * cosf(al);
            F32 rx = cx + sideR2 * sinf(ar);  F32 ry = cy + sideR2 * cosf(ar);

            gGL.vertex2f(tx, ty); gGL.vertex2f(cx, cy); gGL.vertex2f(lx, ly);
            gGL.vertex2f(tx, ty); gGL.vertex2f(rx, ry); gGL.vertex2f(cx, cy);
        }
        gGL.end();
    }

    // Center dot
    gGL.color4f(0.20f, 0.20f, 0.20f, 1.f);
    gl_circle_2d(cx, cy, 5.f, 16, TRUE);
    gGL.color4f(0.50f, 0.50f, 0.50f, 1.f);
    gl_circle_2d(cx, cy, 5.f, 16, FALSE);

    // Heading needle: a yellow triangle pointing in the direction the agent is currently facing.
    LLVector3 at = gAgent.getAtAxis();
    at.mV[VZ] = 0.f;
    if (at.normalize() > 0.01f)
    {
        F32 nl  = tipR * 0.88f; // needle tip reaches slightly inside the compass rim
        F32 pw  = 4.f;          // half-width of the needle base

        // Tip of the needle, pointing in the facing direction
        F32 nx  = cx + nl * at.mV[VX];
        F32 ny  = cy + nl * at.mV[VY];
        // Two base corners, perpendicular to the facing direction
        F32 bx1 = cx - at.mV[VY] * pw;  F32 by1 = cy + at.mV[VX] * pw;
        F32 bx2 = cx + at.mV[VY] * pw;  F32 by2 = cy - at.mV[VX] * pw;

        gGL.begin(LLRender::TRIANGLES);
        gGL.color4f(1.f, 0.85f, 0.f, 0.95f); // yellow
        gGL.vertex2f(nx, ny); gGL.vertex2f(bx1, by1); gGL.vertex2f(bx2, by2);
        gGL.end();
    }

    // Cardinal labels: only in full mode and when compass is large enough
    LLFontGL* font = LLFontGL::getFontSansSerifSmall();
    if (lo.show_labels)
    {
        S32      ld    = lo.R + 10;
        LLColor4 col_n(1.f, 0.55f, 0.55f, 1.f);
        LLColor4 col_o(0.90f, 0.90f, 0.90f, 1.f);

        font->renderUTF8("N", 0, cx,                    (F32)(mCompassCY + ld), col_n, LLFontGL::HCENTER, LLFontGL::BOTTOM,  LLFontGL::BOLD,   LLFontGL::DROP_SHADOW);
        font->renderUTF8("S", 0, cx,                    (F32)(mCompassCY - ld), col_o, LLFontGL::HCENTER, LLFontGL::TOP,     LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
        font->renderUTF8("E", 0, (F32)(mCompassCX + ld),(F32)mCompassCY,        col_o, LLFontGL::LEFT,    LLFontGL::VCENTER, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
        font->renderUTF8("W", 0, (F32)(mCompassCX - ld),(F32)mCompassCY,        col_o, LLFontGL::RIGHT,   LLFontGL::VCENTER, LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
    }

    // Bearing label: hidden in mini compact mode to give compass more room
    if (!lo.mini_compact)
    {
        LLVector3 hat = gAgent.getAtAxis();
        hat.mV[VZ] = 0.f;
        hat.normalize();
        F32 bearing = fmodf(atan2f(hat.mV[VX], hat.mV[VY]) * RAD_TO_DEG + 360.f, 360.f);
        std::string bearing_str = llformat("%03.0f\xC2\xB0", bearing);
        font->renderUTF8(bearing_str, 0, cx, (F32)lo.bearing_y,
            LLColor4(0.85f, 0.85f, 0.85f, 1.f), LLFontGL::HCENTER, LLFontGL::TOP,
            LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
    }

    // Toggle-mode button, placed in the top-right corner of the compass bounding square.
    // Size is computed from the label text so it fits snugly regardless of font scaling.
    const std::string& lbl = lo.toggle_label;
    S32 btn_w   = (S32)font->getWidth(lbl) + 8;
    S32 btn_h   = 14;
    // In mini mode the square edge is just the ring radius; in full mode it extends further to include the N/E labels.
    S32 btn_x   = mCompassCX + lo.sq_half - btn_w;
    S32 btn_yt  = mCompassCY + lo.sq_half;

    mToggleBtnRect.set(btn_x, btn_yt, btn_x + btn_w, btn_yt - btn_h);

    // Dark fill, then a white border that brightens on hover
    gGL.color4f(0.f, 0.f, 0.f, 0.55f);
    gl_rect_2d(mToggleBtnRect, true);
    gGL.color4f(1.f, 1.f, 1.f, mHoverToggle ? 1.f : 0.65f);
    gl_rect_2d(mToggleBtnRect, false);

    // Label centered inside the button
    font->renderUTF8(lbl, 0,
        (F32)(mToggleBtnRect.mLeft + mToggleBtnRect.mRight) * 0.5f,
        (F32)(mToggleBtnRect.mBottom + mToggleBtnRect.mTop) * 0.5f,
        LLColor4::white, LLFontGL::HCENTER, LLFontGL::VCENTER,
        LLFontGL::NORMAL, LLFontGL::NO_SHADOW);
}

bool FSAvatarAlignBase::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // The toggle button (mini/full mode switch) takes priority over everything else
    if (mToggleBtnRect.notEmpty() && mToggleBtnRect.pointInRect(x, y))
    {
        onToggleMode();
        return true;
    }

    if (mCompassR > 0)
    {
        // Check if the click landed inside the compass circle
        S32 dx   = x - mCompassCX;
        S32 dy   = y - mCompassCY;
        F32 dist = sqrtf((F32)(dx * dx + dy * dy));

        if (dist <= (F32)mCompassR)
        {
            if (dist < (F32)mCompassR * 0.25f)
            {
                // Click in the center zone: face the nearest avatar
                onClickFaceNearestAvatar();
            }
            else
            {
                // Click in the outer ring: snap to the closest cardinal/intercardinal direction.
                // Convert the click angle to degrees (north-up), then round to the nearest 45° octant.
                F32 deg    = atan2f((F32)dx, (F32)dy) * RAD_TO_DEG;
                deg        = fmodf(deg + 360.f, 360.f);
                F32 octant = fmodf((F32)(ll_round(deg / 45.f)) * 45.f, 360.f);
                onClickCardinal(octant);
            }
            return true;
        }
    }
    return LLFloater::handleMouseDown(x, y, mask);
}

bool FSAvatarAlignBase::handleHover(S32 x, S32 y, MASK mask)
{
    // Check if the mouse is over the toggle button; reset octant highlight in any case.
    mHoverToggle = mToggleBtnRect.notEmpty() && mToggleBtnRect.pointInRect(x, y);
    mHoverOctant = -1; // -1 means no compass zone is highlighted

    // Only test compass zones when the toggle button is not already consuming the hover
    if (!mHoverToggle && mCompassR > 0)
    {
        S32 dx   = x - mCompassCX;
        S32 dy   = y - mCompassCY;
        F32 dist = sqrtf((F32)(dx * dx + dy * dy));
        if (dist <= (F32)mCompassR)
        {
            if (dist < (F32)mCompassR * 0.25f)
                mHoverOctant = -2; // -2 means center zone (face nearest avatar)
            else
            {
                // Outer ring: determine which of the 8 octants the mouse is in
                F32 deg = fmodf(atan2f((F32)dx, (F32)dy) * RAD_TO_DEG + 360.f, 360.f);
                mHoverOctant = (S32)(ll_round(deg / 45.f)) % 8;
            }
        }
    }
    return LLFloater::handleHover(x, y, mask);
}

void FSAvatarAlignBase::snapAvatarBody(const LLVector3& target_at)
{
    if (!isAgentAvatarValid() || !gAgentAvatarp->mRoot)
        return;

    // Strip any vertical component so the avatar stays upright
    LLVector3 at = target_at;
    at.mV[VZ] = 0.f;
    if (at.normalize() < 0.001f)
        return;

    // Build a clean rotation from the forward direction, keeping "up" as the world Z axis
    LLVector3 up(0.f, 0.f, 1.f);
    LLVector3 left = up % at;
    left.normalize();
    at = left % up;

    // Apply the new orientation and plant the avatar at the agent's current position
    gAgentAvatarp->mRoot->setWorldRotation(LLQuaternion(at, left, up));
    gAgentAvatarp->mRoot->setWorldPosition(gAgent.getPositionAgent());
}

void FSAvatarAlignBase::snapRemoteAvatarBody(LLVOAvatar* avatar)
{
    if (!avatar || avatar->isDead() || !avatar->mRoot)
        return;

    // Reset the avatar's skeleton to exactly the server rotation and position
    avatar->mRoot->setWorldRotation(avatar->getRotation());
    avatar->mRoot->setWorldPosition(avatar->getPositionAgent());
}

void FSAvatarAlignBase::applyRotation(const LLVector3& direction)
{
    // Rotate the agent camera/frame, tell the server, then fix up both avatar bodies visually
    gAgent.resetAxes(direction);
    send_agent_update(true, false);
    snapAvatarBody(direction);
    snapRemoteAvatarBody(mTargetAvatar);
    mTargetAvatar = nullptr;
}

void FSAvatarAlignBase::rotateAgentTo(F32 target_deg)
{
    // Convert an absolute compass angle (degrees, north=0) to a world direction and rotate
    F32 yaw_rad = target_deg * DEG_TO_RAD;
    LLVector3 look_at(sinf(yaw_rad), cosf(yaw_rad), 0.f);
    applyRotation(look_at);
}

void FSAvatarAlignBase::onClickCardinal(F32 target_deg)
{
    // Rotate to the exact compass angle that was clicked on the compass ring
    rotateAgentTo(target_deg);
}

void FSAvatarAlignBase::onClickRotate(F32 delta_deg)
{
    // Rotate by a relative offset from the current facing direction
    LLVector3 at = gAgent.getFrameAgent().getAtAxis();
    at.mV[VZ] = 0.f;
    at.normalize();
    F32 yaw_deg = atan2f(at.mV[VX], at.mV[VY]) * RAD_TO_DEG;
    rotateAgentTo(fmodf(yaw_deg + delta_deg + 360.f, 360.f));
}

void FSAvatarAlignBase::onClickNearest()
{
    // Snap the current heading to the closest 45-degree increment
    LLVector3 at = gAgent.getFrameAgent().getAtAxis();
    at.mV[VZ] = 0.f;
    at.normalize();
    F32 yaw_deg = atan2f(at.mV[VX], at.mV[VY]) * RAD_TO_DEG;
    yaw_deg = fmodf(yaw_deg + 360.f, 360.f);
    F32 nearest_deg = fmodf((F32)(ll_round(yaw_deg / 45.f) * 45), 360.f);
    rotateAgentTo(nearest_deg);
}

bool FSAvatarAlignBase::isAvatarInRange(LLVOAvatar* avatar) const
{
    if (!avatar || avatar->isDead())
        return false;
    return dist_vec(avatar->getPositionAgent(), gAgent.getPositionAgent()) <= MAX_FACE_DISTANCE;
}

void FSAvatarAlignBase::faceAvatar(LLVOAvatar* avatar)
{
    if (!avatar || !isAgentAvatarValid())
        return;

    mTargetAvatar = avatar;

    // Compute the horizontal direction from us to the target avatar and rotate to face it
    LLVector3 direction = avatar->getPositionAgent() - gAgentAvatarp->getPositionAgent();
    direction.mV[VZ] = 0.f;
    direction.normalize();

    applyRotation(direction);
}

void FSAvatarAlignBase::onClickFaceNearestAvatar()
{
    if (!isAgentAvatarValid())
        return;

    LLVector3   my_pos         = gAgent.getPositionAgent();
    LLVOAvatar* nearest        = nullptr;
    F32         nearest_dist_sq = F32_MAX;

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        LLVOAvatar* avatar = (LLVOAvatar*)character;
        // do not select ourself or dead avatar as the nearest avatar
        if (avatar->isDead() || avatar->isControlAvatar() || avatar->isSelf())
            continue;

        // do not select someone that is located further than MAX_FACE_DISTANCE meters from us, in any 3D direction
        F32 dist_sq = dist_vec_squared(avatar->getPositionAgent(), my_pos);
        if (dist_sq > MAX_FACE_DISTANCE * MAX_FACE_DISTANCE)
            continue;

        // if that avatar is nearer the previously selected avatar, select it instead
        if (dist_sq < nearest_dist_sq)
        {
            nearest_dist_sq = dist_sq;
            nearest = avatar;
        }
    }

    if (!nearest)
    {
        LL_WARNS("AvatarAlign") << "No nearby avatar found to face." << LL_ENDL;
        return;
    }

    faceAvatar(nearest);
}

// When switching between full and mini mode :
// if floater is located on the left of the app, grow from the left. Otherwiser grow from the right.
// if floater is located on the bottom of the app, grow from the bottom. Otherwiser grow from the top.
void FSAvatarAlignBase::repositionOnToggle(LLFloater* next, const LLRect& old_rect)
{
    LLRect view  = gFloaterView->getRect();
    S32 new_w    = next->getRect().getWidth();
    S32 new_h    = next->getRect().getHeight();
    bool on_left = (old_rect.getCenterX() < view.getCenterX());
    bool on_bot  = (old_rect.getCenterY() < view.getCenterY());
    S32 new_left = on_left ? old_rect.mLeft : (old_rect.mRight - new_w);
    S32 new_bot  = on_bot  ? old_rect.mBottom : (old_rect.mTop - new_h);
    next->setOrigin(new_left, new_bot);
}

// ============================================================
// FSFloaterAvatarAlign  (full mode)
// ============================================================

FSFloaterAvatarAlign::FSFloaterAvatarAlign(const LLSD& key)
    : FSAvatarAlignBase(key)
{
}

bool FSFloaterAvatarAlign::postBuild()
{
    // Wire up all rotation buttons and the face-avatar button
    childSetAction("btn_rotate_left_90",  [this](void*) { onClickRotate(-90.f);         }, this);
    childSetAction("btn_rotate_left_45",  [this](void*) { onClickRotate(-45.f);         }, this);
    childSetAction("btn_rotate_right_45", [this](void*) { onClickRotate( 45.f);         }, this);
    childSetAction("btn_rotate_right_90", [this](void*) { onClickRotate( 90.f);         }, this);
    childSetAction("btn_rotate_left_10",  [this](void*) { onClickRotate(-10.f);         }, this);
    childSetAction("btn_rotate_left_1",   [this](void*) { onClickRotate( -1.f);         }, this);
    childSetAction("btn_rotate_right_1",  [this](void*) { onClickRotate(  1.f);         }, this);
    childSetAction("btn_rotate_right_10", [this](void*) { onClickRotate( 10.f);         }, this);
    childSetAction("btn_nearest",         [this](void*) { onClickNearest();             }, this);
    childSetAction("btn_avatar",          [this](void*) { onClickFaceNearestAvatar();   }, this);

    return true;
}

void FSFloaterAvatarAlign::onOpen(const LLSD& key)
{
}

void FSFloaterAvatarAlign::onToggleMode()
{
    // Switch to mini mode: save the preference, close this floater, open the mini one
    gSavedSettings.setBOOL("AvatarAlignMini", true);
    LLRect rect = getRect();
    closeFloater(false);
    FSFloaterAvatarAlignMini* mini = LLFloaterReg::showTypedInstance<FSFloaterAvatarAlignMini>("avatar_align_mini");
    if (mini) { repositionOnToggle(mini, rect); }
}

// ============================================================
// FSFloaterAvatarAlignMini  (mini mode)
// ============================================================

FSFloaterAvatarAlignMini::FSFloaterAvatarAlignMini(const LLSD& key)
    : FSAvatarAlignBase(key)
{
}

bool FSFloaterAvatarAlignMini::postBuild()
{
    // Mini mode only has fine-step rotation and the face-avatar button
    childSetAction("btn_rotate_left_1",  [this](void*) { onClickRotate(-1.f);           }, this);
    childSetAction("btn_rotate_right_1", [this](void*) { onClickRotate( 1.f);           }, this);
    childSetAction("btn_avatar",         [this](void*) { onClickFaceNearestAvatar();    }, this);

    return true;
}

void FSFloaterAvatarAlignMini::onOpen(const LLSD& key)
{
}

void FSFloaterAvatarAlignMini::onToggleMode()
{
    // Switch to full mode: save the preference, close this floater, open the full one
    gSavedSettings.setBOOL("AvatarAlignMini", false);
    LLRect rect = getRect();
    closeFloater(false);
    FSFloaterAvatarAlign* full = LLFloaterReg::showTypedInstance<FSFloaterAvatarAlign>("avatar_align");
    if (full) { repositionOnToggle(full, rect); }
}

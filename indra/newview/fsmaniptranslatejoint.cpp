/**
 * @file fsmaniptranslatejoint.cpp
 * @brief custom manipulator for moving joints
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2025 Angeldark Raymaker @ Second Life
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

#include "fsmaniptranslatejoint.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h"
#include "llcylinder.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llviewerwindow.h"
#include "llworld.h"
#include "fsfloaterposer.h"
#include "llfloaterreg.h"

constexpr S32 NUM_AXES = 3;
constexpr S32 MOUSE_DRAG_SLOP = 2;       // pixels
constexpr F32 SELECTED_ARROW_SCALE = 1.3f;
constexpr F32 MANIPULATOR_HOTSPOT_START = 0.2f;
constexpr F32 MANIPULATOR_HOTSPOT_END = 1.2f;
constexpr F32 MIN_PLANE_MANIP_DOT_PRODUCT = 0.25f;
constexpr F32 PLANE_TICK_SIZE = 0.4f;

static LLPointer<LLViewerTexture> sGridTex = nullptr;

constexpr LLManip::EManipPart MANIPULATOR_IDS[9] =
{
    LLManip::LL_X_ARROW,
    LLManip::LL_Y_ARROW,
    LLManip::LL_Z_ARROW,
    LLManip::LL_X_ARROW,
    LLManip::LL_Y_ARROW,
    LLManip::LL_Z_ARROW,
    LLManip::LL_YZ_PLANE,
    LLManip::LL_XZ_PLANE,
    LLManip::LL_XY_PLANE
};

constexpr U32 ARROW_TO_AXIS[4] =
{
    VX,
    VX,
    VY,
    VZ
};

// Sort manipulator handles by their screen-space projection
struct ClosestToCamera
{
    bool operator()(const FSManipTranslateJoint::ManipulatorHandle& a,
                    const FSManipTranslateJoint::ManipulatorHandle& b) const
    {
        return a.mEndPosition.mV[VZ] < b.mEndPosition.mV[VZ];
    }
};

FSManipTranslateJoint::FSManipTranslateJoint(LLToolComposite* composite) :
    LLManipTranslate(composite)
{
    if (sGridTex.isNull())
    {
        restoreGL();
    }
}

void FSManipTranslateJoint::setAvatar(LLVOAvatar* avatar)
{
    mAvatar = avatar;
}

void FSManipTranslateJoint::setJoint(LLJoint* joint)
{
    mJoint = joint;
}

//static
U32 FSManipTranslateJoint::getGridTexName()
{
    if (sGridTex.isNull())
    {
        restoreGL();
    }

    return sGridTex.isNull() ? 0 : sGridTex->getTexName();
}

//static
void FSManipTranslateJoint::destroyGL()
{
    if (sGridTex)
    {
        sGridTex = nullptr;
    }
}

//static
void FSManipTranslateJoint::restoreGL()
{
    //generate grid texture
    U32 rez = 512;
    U32 mip = 0;

    destroyGL();
    sGridTex = LLViewerTextureManager::getLocalTexture();
    if (!sGridTex->createGLTexture())
    {
        sGridTex = nullptr;
        return;
    }

    GLuint* d = new GLuint[rez * rez];

    gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, sGridTex->getTexName(), true);
    gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_TRILINEAR);

    while (rez >= 1)
    {
        for (U32 i = 0; i < rez * rez; i++)
        {
            d[i] = 0x00FFFFFF;
        }

        U32 subcol = 0xFFFFFFFF;
        if (rez >= 4)
        {   //large grain grid
            for (U32 i = 0; i < rez; i++)
            {
                if (rez == 16)
                {
                    subcol = 0xA0FFFFFF;
                }
                else if (rez == 8)
                {
                    subcol = 0x80FFFFFF;
                }
                else if (rez < 16)
                {
                    subcol = 0x40FFFFFF;
                }
                else
                {
                    subcol = 0xFFFFFFFF;
                }

                d[i         *rez+ 0      ] = subcol;
                d[0         *rez+ i      ] = subcol;
                if (rez >= 32)
                {
                    d[i         *rez+ (rez-1)] = subcol;
                    d[(rez-1)   *rez+ i      ] = subcol;
                }

                if (rez >= 64)
                {
                    subcol = 0xFFFFFFFF;

                    if (i > 0 && i < (rez-1))
                    {
                        d[i         *rez+ 1      ] = subcol;
                        d[i         *rez+ (rez-2)] = subcol;
                        d[1         *rez+ i      ] = subcol;
                        d[(rez-2)   *rez+ i      ] = subcol;
                    }
                }
            }
        }

        subcol = 0x50A0A0A0;
        if (rez >= 128)
        { //small grain grid
            for (U32 i = 8; i < rez; i+=8)
            {
                for (U32 j = 2; j < rez-2; j++)
                {
                    d[i *rez+ j] = subcol;
                    d[j *rez+ i] = subcol;
                }
            }
        }
        if (rez >= 64)
        { //medium grain grid
            if (rez == 64)
            {
                subcol = 0x50A0A0A0;
            }
            else
            {
                subcol = 0xA0D0D0D0;
            }

            for (U32 i = 32; i < rez; i+=32)
            {
                U32 pi = i-1;
                for (U32 j = 2; j < rez-2; j++)
                {
                    d[i     *rez+ j] = subcol;
                    d[j     *rez+ i] = subcol;

                    if (rez > 128)
                    {
                        d[pi    *rez+ j] = subcol;
                        d[j     *rez+ pi] = subcol;
                    }
                }
            }
        }
        LLImageGL::setManualImage(GL_TEXTURE_2D, mip, GL_RGBA, rez, rez, GL_RGBA, GL_UNSIGNED_BYTE, d);
        rez = rez >> 1;
        mip++;
    }
    delete[] d;
}

void FSManipTranslateJoint::handleSelect()
{
    LLSelectMgr::getInstance()->saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
    if (gFloaterTools)
    {
        gFloaterTools->setStatusText("move");
    }
    LLManip::handleSelect();
}

bool FSManipTranslateJoint::handleMouseDown(S32 x, S32 y, MASK mask)
{
    bool handled = false;

    // didn't click in any UI object, so must have clicked in the world
    if ((mHighlightedPart == LL_X_ARROW ||
         mHighlightedPart == LL_Y_ARROW ||
         mHighlightedPart == LL_Z_ARROW ||
         mHighlightedPart == LL_YZ_PLANE ||
         mHighlightedPart == LL_XZ_PLANE ||
         mHighlightedPart == LL_XY_PLANE))
    {
        handled = handleMouseDownOnPart(x, y, mask);
    }

    return handled;
}

// Assumes that one of the arrows on an object was hit.
bool FSManipTranslateJoint::handleMouseDownOnPart(S32 x, S32 y, MASK mask)
{
    bool can_move = canAffectSelection();
    if (!can_move)
    {
        return false;
    }

    highlightManipulators(x, y);
    S32 hit_part = mHighlightedPart;

    if ((hit_part != LL_X_ARROW) &&
        (hit_part != LL_Y_ARROW) &&
        (hit_part != LL_Z_ARROW) &&
        (hit_part != LL_YZ_PLANE) &&
        (hit_part != LL_XZ_PLANE) &&
        (hit_part != LL_XY_PLANE))
    {
        return true;
    }

    mHelpTextTimer.reset();
    sNumTimesHelpTextShown++;

    LLSelectMgr::getInstance()->getGrid(mGridOrigin, mGridRotation, mGridScale);

    mManipPart = (EManipPart)hit_part;
    mMouseDownX = x;
    mMouseDownY = y;
    mMouseOutsideSlop = false;

    LLVector3 axis;

    if (!isAvatarJointSafeToUse())
    {
        LL_WARNS() << "Translate manip joint lost the joint or avatar." << LL_ENDL;
        gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
        return true;
    }

    auto* poser = LLFloaterReg::findTypedInstance<FSFloaterPoser>("fs_poser");
    if (!poser)
        return false;

    poser->setFocus(true);

    // Compute unit vectors for arrow hit and a plane through that vector
    bool axis_exists = getManipJointAxis(mJoint, mManipPart, axis);
    getManipJointNormal(mJoint, mManipPart, mManipNormal);

    LLVector3 select_center_agent = mJoint->getWorldPosition();

    // if we clicked on a planar manipulator, recenter mouse cursor
    if (mManipPart >= LL_YZ_PLANE && mManipPart <= LL_XY_PLANE)
    {
        LLCoordGL mouse_pos;
        if (!LLViewerCamera::getInstance()->projectPosAgentToScreen(select_center_agent, mouse_pos))
        {
            // mouse_pos may be nonsense
            LL_WARNS() << "Failed to project object center to screen" << LL_ENDL;
        }
        else if (gSavedSettings.getBOOL("SnapToMouseCursor"))
        {
            LLUI::getInstance()->setMousePositionScreen(mouse_pos.mX, mouse_pos.mY);
            x = mouse_pos.mX;
            y = mouse_pos.mY;
        }
    }

    LLVector3d object_start_global = gAgent.getPosGlobalFromAgent(mJoint->getWorldPosition());
    getMousePointOnPlaneGlobal(mDragCursorStartGlobal, x, y, object_start_global, mManipNormal);
    mDragSelectionStartGlobal = object_start_global;
    mDragCursorLastGlobal.setZero();

    // Route future Mouse messages here preemptively.  (Release on mouse up.)
    setMouseCapture(true);

    return true;
}

bool FSManipTranslateJoint::handleHover(S32 x, S32 y, MASK mask)
{
    // Translation tool only works if mouse button is down.
    // Bail out if mouse not down.
    if (!hasMouseCapture())
    {
        LL_DEBUGS("UserInput") << "hover handled by FSManipTranslateJoint (inactive)" << LL_ENDL;
        // Always show cursor
        gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);

        highlightManipulators(x, y);
        return true;
    }

    // Handle auto-rotation if necessary.
    LLRect world_rect = gViewerWindow->getWorldViewRectScaled();
    constexpr F32 ROTATE_ANGLE_PER_SECOND = 30.f * DEG_TO_RAD;
    const S32 ROTATE_H_MARGIN = world_rect.getWidth() / 20;
    const F32 rotate_angle = ROTATE_ANGLE_PER_SECOND / gFPSClamped;
    bool rotated = false;

    // ...moving joints can move camera about focus point
    if (x < ROTATE_H_MARGIN)
    {
        gAgentCamera.cameraOrbitAround(rotate_angle);
        rotated = true;
    }
    else if (x > world_rect.getWidth() - ROTATE_H_MARGIN)
    {
        gAgentCamera.cameraOrbitAround(-rotate_angle);
        rotated = true;
    }

    // Suppress processing if mouse hasn't actually moved.
    // This may cause problems if the camera moves outside of the
    // rotation above.
    if (x == mLastHoverMouseX && y == mLastHoverMouseY && !rotated)
    {
        LL_DEBUGS("UserInput") << "hover handled by FSManipTranslateJoint (mouse unmoved)" << LL_ENDL;
        gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
        return true;
    }
    mLastHoverMouseX = x;
    mLastHoverMouseY = y;

    // Suppress if mouse hasn't moved past the initial slop region
    // Reset once we start moving
    if (!mMouseOutsideSlop)
    {
        if (abs(mMouseDownX - x) < MOUSE_DRAG_SLOP && abs(mMouseDownY - y) < MOUSE_DRAG_SLOP )
        {
            LL_DEBUGS("UserInput") << "hover handled by FSManipTranslateJoint (mouse inside slop)" << LL_ENDL;
            gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
            return true;
        }
        else
        {
            // ...just went outside the slop region
            mMouseOutsideSlop = true;
        }
    }

    // Throttle updates to 10 per second.
    LLVector3  axis_f;
    LLVector3d axis_d;

    if (!isAvatarJointSafeToUse())
    {
        LL_WARNS() << "Translate joint manip lost the joint or avatar" << LL_ENDL;
        gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
        return true;
    }

    // Compute unit vectors for arrow hit and a plane through that vector
    bool axis_exists = getManipJointAxis(mJoint, mManipPart, axis_f);
    axis_d.setVec(axis_f);

    LLVector3d current_pos_global = gAgent.getPosGlobalFromAgent(mJoint->getWorldPosition());

    // Project the cursor onto that plane
    LLVector3d relative_move;
    getMousePointOnPlaneGlobal(relative_move, x, y, current_pos_global, mManipNormal);
    relative_move -= mDragCursorStartGlobal;

    F64 axis_magnitude = relative_move * axis_d; // dot product
    LLVector3d cursor_point_snap_line;
    F64 off_axis_magnitude;

    getMousePointOnPlaneGlobal(cursor_point_snap_line, x, y, current_pos_global, mSnapOffsetAxis % axis_f);
    off_axis_magnitude = axis_exists ? llabs((cursor_point_snap_line - current_pos_global) * LLVector3d(mSnapOffsetAxis)) : 0.f;

    // Clamp to arrow direction
    // *FIX: does this apply anymore?
    if (!axis_exists)
    {
        axis_magnitude = relative_move.normVec();
        axis_d.setVec(relative_move);
        axis_d.normVec();
        axis_f.setVec(axis_d);
    }

    LLVector3d clamped_relative_move = axis_magnitude * axis_d; // scalar multiply
    LLVector3 clamped_relative_move_f = (F32)axis_magnitude * axis_f; // scalar multiply
    auto* poser = dynamic_cast<FSFloaterPoser*>(LLFloaterReg::findInstance("fs_poser"));
    if (poser && mJoint)
        poser->updatePosedBones(mJoint->getName(), LLQuaternion(), getChangeInPosition(clamped_relative_move_f), LLVector3::zero);

    mDragCursorLastGlobal = clamped_relative_move_f;
    gAgentCamera.clearFocusObject();

    LL_DEBUGS("UserInput") << "hover handled by FSManipTranslateJoint (active)" << LL_ENDL;
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);

    return true;
}

LLVector3 FSManipTranslateJoint::getChangeInPosition(const LLVector3& newPosition) const
{
    LLVector3 rawChange = newPosition - mDragCursorLastGlobal;

    switch (mManipPart)
    {
        case LL_X_ARROW:
            rawChange[VY] = 0.f;
            rawChange[VZ] = 0.f;
            break;
        case LL_Y_ARROW:
            rawChange[VX] = 0.f;
            rawChange[VZ] = 0.f;
            break;
        case LL_Z_ARROW:
            rawChange[VX] = 0.f;
            rawChange[VY] = 0.f;
            break;
        case LL_XY_PLANE:
            rawChange[VZ] = 0.f;
            break;
        case LL_XZ_PLANE:
            rawChange[VY] = 0.f;
            break;
        case LL_YZ_PLANE:
            rawChange[VX] = 0.f;
            break;
            break;
        default:
            break;
    }

    return rawChange;
}

void FSManipTranslateJoint::highlightManipulators(S32 x, S32 y)
{
    mHighlightedPart = LL_NO_PART;

    if (!isAvatarJointSafeToUse())
        return;

    LLMatrix4 projMatrix = LLViewerCamera::getInstance()->getProjection();
    LLMatrix4 modelView = LLViewerCamera::getInstance()->getModelview();

    LLVector3 object_position = mJoint->getWorldPosition();

    LLVector3 grid_origin;
    LLVector3 grid_scale;
    LLQuaternion grid_rotation;

    LLSelectMgr::getInstance()->getGrid(grid_origin, grid_rotation, grid_scale);

    LLVector3 relative_camera_dir;

    LLMatrix4 transform;
    relative_camera_dir = (object_position - LLViewerCamera::getInstance()->getOrigin()) * ~grid_rotation;
    relative_camera_dir.normVec();

    transform.initRotTrans(grid_rotation, LLVector4(object_position));
    transform *= modelView;
    transform *= projMatrix;

    S32 numManips = 0;

    // edges
    mManipulatorVertices[numManips++] = LLVector4(mArrowLengthMeters * MANIPULATOR_HOTSPOT_START, 0.f, 0.f, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(mArrowLengthMeters * MANIPULATOR_HOTSPOT_END, 0.f, 0.f, 1.f);

    mManipulatorVertices[numManips++] = LLVector4(0.f, mArrowLengthMeters * MANIPULATOR_HOTSPOT_START, 0.f, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(0.f, mArrowLengthMeters * MANIPULATOR_HOTSPOT_END, 0.f, 1.f);

    mManipulatorVertices[numManips++] = LLVector4(0.f, 0.f, mArrowLengthMeters * MANIPULATOR_HOTSPOT_START, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(0.f, 0.f, mArrowLengthMeters * MANIPULATOR_HOTSPOT_END, 1.f);

    mManipulatorVertices[numManips++] = LLVector4(mArrowLengthMeters * -MANIPULATOR_HOTSPOT_START, 0.f, 0.f, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(mArrowLengthMeters * -MANIPULATOR_HOTSPOT_END, 0.f, 0.f, 1.f);

    mManipulatorVertices[numManips++] = LLVector4(0.f, mArrowLengthMeters * -MANIPULATOR_HOTSPOT_START, 0.f, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(0.f, mArrowLengthMeters * -MANIPULATOR_HOTSPOT_END, 0.f, 1.f);

    mManipulatorVertices[numManips++] = LLVector4(0.f, 0.f, mArrowLengthMeters * -MANIPULATOR_HOTSPOT_START, 1.f);
    mManipulatorVertices[numManips++] = LLVector4(0.f, 0.f, mArrowLengthMeters * -MANIPULATOR_HOTSPOT_END, 1.f);

    S32 num_arrow_manips = numManips;

    // planar manipulators
    bool planar_manip_yz_visible = false;
    bool planar_manip_xz_visible = false;
    bool planar_manip_xy_visible = false;

    mManipulatorVertices[numManips] = LLVector4(0.f, mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    mManipulatorVertices[numManips] = LLVector4(0.f, mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    if (llabs(relative_camera_dir.mV[VX]) > MIN_PLANE_MANIP_DOT_PRODUCT)
    {
        planar_manip_yz_visible = true;
    }

    mManipulatorVertices[numManips] = LLVector4(mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), 0.f, mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    mManipulatorVertices[numManips] = LLVector4(mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), 0.f, mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    if (llabs(relative_camera_dir.mV[VY]) > MIN_PLANE_MANIP_DOT_PRODUCT)
    {
        planar_manip_xz_visible = true;
    }

    mManipulatorVertices[numManips] = LLVector4(mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), mPlaneManipOffsetMeters * (1.f - PLANE_TICK_SIZE * 0.5f), 0.f, 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    mManipulatorVertices[numManips] = LLVector4(mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), mPlaneManipOffsetMeters * (1.f + PLANE_TICK_SIZE * 0.5f), 0.f, 1.f);
    mManipulatorVertices[numManips++].scaleVec(mPlaneManipPositions);
    if (llabs(relative_camera_dir.mV[VZ]) > MIN_PLANE_MANIP_DOT_PRODUCT)
    {
        planar_manip_xy_visible = true;
    }

    // Project up to 9 manipulators to screen space 2*X, 2*Y, 2*Z, 3*planes
    std::vector<ManipulatorHandle> projected_manipulators;
    projected_manipulators.reserve(9);

    for (S32 i = 0; i < num_arrow_manips; i+= 2)
    {
        LLVector4 projected_start = mManipulatorVertices[i] * transform;
        projected_start = projected_start * (1.f / projected_start.mV[VW]);

        LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
        projected_end = projected_end * (1.f / projected_end.mV[VW]);

        ManipulatorHandle projected_manip(
                LLVector3(projected_start.mV[VX], projected_start.mV[VY], projected_start.mV[VZ]),
                LLVector3(projected_end.mV[VX], projected_end.mV[VY], projected_end.mV[VZ]),
                MANIPULATOR_IDS[i / 2],
                10.f); // 10 pixel hotspot for arrows
        projected_manipulators.push_back(projected_manip);
    }

    if (planar_manip_yz_visible)
    {
        S32 i = num_arrow_manips;
        LLVector4 projected_start = mManipulatorVertices[i] * transform;
        projected_start = projected_start * (1.f / projected_start.mV[VW]);

        LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
        projected_end = projected_end * (1.f / projected_end.mV[VW]);

        ManipulatorHandle projected_manip(
                LLVector3(projected_start.mV[VX], projected_start.mV[VY], projected_start.mV[VZ]),
                LLVector3(projected_end.mV[VX], projected_end.mV[VY], projected_end.mV[VZ]),
                MANIPULATOR_IDS[i / 2],
                20.f); // 20 pixels for planar manipulators
        projected_manipulators.push_back(projected_manip);
    }

    if (planar_manip_xz_visible)
    {
        S32 i = num_arrow_manips + 2;
        LLVector4 projected_start = mManipulatorVertices[i] * transform;
        projected_start = projected_start * (1.f / projected_start.mV[VW]);

        LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
        projected_end = projected_end * (1.f / projected_end.mV[VW]);

        ManipulatorHandle projected_manip(
                LLVector3(projected_start.mV[VX], projected_start.mV[VY], projected_start.mV[VZ]),
                LLVector3(projected_end.mV[VX], projected_end.mV[VY], projected_end.mV[VZ]),
                MANIPULATOR_IDS[i / 2],
                20.f); // 20 pixels for planar manipulators
        projected_manipulators.push_back(projected_manip);
    }

    if (planar_manip_xy_visible)
    {
        S32 i = num_arrow_manips + 4;
        LLVector4 projected_start = mManipulatorVertices[i] * transform;
        projected_start = projected_start * (1.f / projected_start.mV[VW]);

        LLVector4 projected_end = mManipulatorVertices[i + 1] * transform;
        projected_end = projected_end * (1.f / projected_end.mV[VW]);

        ManipulatorHandle projected_manip(
                LLVector3(projected_start.mV[VX], projected_start.mV[VY], projected_start.mV[VZ]),
                LLVector3(projected_end.mV[VX], projected_end.mV[VY], projected_end.mV[VZ]),
                MANIPULATOR_IDS[i / 2],
                20.f); // 20 pixels for planar manipulators
        projected_manipulators.push_back(projected_manip);
    }

    LLVector2 manip_start_2d;
    LLVector2 manip_end_2d;
    LLVector2 manip_dir;
    LLRect world_view_rect = gViewerWindow->getWorldViewRectScaled();
    F32 half_width = (F32)world_view_rect.getWidth() / 2.f;
    F32 half_height = (F32)world_view_rect.getHeight() / 2.f;
    LLVector2 mousePos((F32)x - half_width, (F32)y - half_height);
    LLVector2 mouse_delta;

    // Keep order consistent with insertion via stable_sort
    std::stable_sort( projected_manipulators.begin(),
        projected_manipulators.end(),
        ClosestToCamera() );

    std::vector<ManipulatorHandle>::iterator it = projected_manipulators.begin();
    for (; it != projected_manipulators.end(); ++it)
    {
        ManipulatorHandle& manipulator = *it;
        {
            manip_start_2d.setVec(manipulator.mStartPosition.mV[VX] * half_width, manipulator.mStartPosition.mV[VY] * half_height);
            manip_end_2d.setVec(manipulator.mEndPosition.mV[VX] * half_width, manipulator.mEndPosition.mV[VY] * half_height);
            manip_dir = manip_end_2d - manip_start_2d;

            mouse_delta = mousePos - manip_start_2d;

            F32 manip_length = manip_dir.normVec();

            F32 mouse_pos_manip = mouse_delta * manip_dir;
            F32 mouse_dist_manip_squared = mouse_delta.magVecSquared() - (mouse_pos_manip * mouse_pos_manip);

            if (mouse_pos_manip > 0.f &&
                mouse_pos_manip < manip_length &&
                mouse_dist_manip_squared < manipulator.mHotSpotRadius * manipulator.mHotSpotRadius)
            {
                mHighlightedPart = manipulator.mManipID;
                break;
            }
        }
    }
}

bool FSManipTranslateJoint::handleMouseUp(S32 x, S32 y, MASK mask)
{
    // first, perform normal processing in case this was a quick-click
    handleHover(x, y, mask);

    if (hasMouseCapture())
    {
        // make sure arrow colors go back to normal
        mManipPart = LL_NO_PART;
        LLSelectMgr::getInstance()->enableSilhouette(true);

        // Might have missed last update due to UPDATE_DELAY timing.
        LLSelectMgr::getInstance()->sendMultipleUpdate( UPD_POSITION );

        LLSelectMgr::getInstance()->saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
    }

    return LLManip::handleMouseUp(x, y, mask);
}

void FSManipTranslateJoint::render()
{
    if (!isMoveableJoint())
        return;

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    {
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
        renderGuidelines();
    }
    {
        renderTranslationHandles();
    }
    gGL.popMatrix();

    renderText();
}

void FSManipTranslateJoint::renderText()
{
    if (!isAvatarJointSafeToUse())
        return;

    LLVector3 pos = mJoint->getWorldPosition();
    renderXYZ(pos);
}

void FSManipTranslateJoint::renderTranslationHandles()
{
    LLGLDepthTest gls_depth(GL_FALSE);
    if (!isAvatarJointSafeToUse())
        return;

    LLVector3    grid_origin, grid_scale, at_axis;
    LLQuaternion grid_rotation;

    LLSelectMgr::getInstance()->getGrid(grid_origin, grid_rotation, grid_scale);
    at_axis = LLViewerCamera::getInstance()->getAtAxis() * ~grid_rotation;

    if (at_axis.mV[VX] > 0.f)
    {
        mPlaneManipPositions.mV[VX] = 1.f;
    }
    else
    {
        mPlaneManipPositions.mV[VX] = -1.f;
    }

    if (at_axis.mV[VY] > 0.f)
    {
        mPlaneManipPositions.mV[VY] = 1.f;
    }
    else
    {
        mPlaneManipPositions.mV[VY] = -1.f;
    }

    if (at_axis.mV[VZ] > 0.f)
    {
        mPlaneManipPositions.mV[VZ] = 1.f;
    }
    else
    {
        mPlaneManipPositions.mV[VZ] = -1.f;
    }

    LLVector3 selection_center = mJoint->getWorldPosition();

    // Drag handles
    LLVector3 camera_pos_agent = gAgentCamera.getCameraPositionAgent();
    F32 range = dist_vec(camera_pos_agent, selection_center);
    F32 range_from_agent = dist_vec(gAgent.getPositionAgent(), selection_center);

    // Don't draw handles if you're too far away
    if (gSavedSettings.getBOOL("LimitSelectDistance") && range_from_agent > gSavedSettings.getF32("MaxSelectDistance"))
        return;

    if (range > 0.001f)
    {
        F32 fraction_of_fov = mAxisArrowLength / (F32) LLViewerCamera::getInstance()->getViewHeightInPixels();
        F32 apparent_angle = fraction_of_fov * LLViewerCamera::getInstance()->getView();  // radians
        mArrowLengthMeters = range * tan(apparent_angle);
    }
    else
    {
        mArrowLengthMeters = 1.0f;
    }

    //Assume that UI scale factor is equivalent for X and Y axis
    F32 ui_scale_factor = LLUI::getScaleFactor().mV[VX];
    mArrowLengthMeters *= ui_scale_factor;

    mPlaneManipOffsetMeters = mArrowLengthMeters * 1.8f;
    mConeSize = mArrowLengthMeters / 4.f;

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    {
        gGL.translatef(selection_center.mV[VX], selection_center.mV[VY], selection_center.mV[VZ]);

        F32 angle_radians, x, y, z;
        grid_rotation.getAngleAxis(&angle_radians, &x, &y, &z);

        gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

        LLQuaternion invRotation = grid_rotation;
        invRotation.conjQuat();

        LLVector3 relative_camera_dir;
        relative_camera_dir = (selection_center - LLViewerCamera::getInstance()->getOrigin()) * invRotation;
        relative_camera_dir.normVec();

        {
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
            LLGLDisable cull_face(GL_CULL_FACE);

            LLColor4 color1;
            LLColor4 color2;

            // update manipulator sizes
            for (S32 index = 0; index < 3; index++)
            {
                if (index == mManipPart - LL_X_ARROW || index == mHighlightedPart - LL_X_ARROW)
                {
                    mArrowScales.mV[index] = lerp(mArrowScales.mV[index], SELECTED_ARROW_SCALE, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                    mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index], 1.f, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                }
                else if (index == mManipPart - LL_YZ_PLANE || index == mHighlightedPart - LL_YZ_PLANE)
                {
                    mArrowScales.mV[index] = lerp(mArrowScales.mV[index], 1.f, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                    mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index], SELECTED_ARROW_SCALE, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                }
                else
                {
                    mArrowScales.mV[index] = lerp(mArrowScales.mV[index], 1.f, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                    mPlaneScales.mV[index] = lerp(mPlaneScales.mV[index], 1.f, LLSmoothInterpolation::getInterpolant(MANIPULATOR_SCALE_HALF_LIFE ));
                }
            }

            if ((mManipPart == LL_NO_PART || mManipPart == LL_YZ_PLANE) && llabs(relative_camera_dir.mV[VX]) > MIN_PLANE_MANIP_DOT_PRODUCT)
            {
                // render YZ plane manipulator
                gGL.pushMatrix();
                gGL.scalef(mPlaneManipPositions.mV[VX], mPlaneManipPositions.mV[VY], mPlaneManipPositions.mV[VZ]);
                gGL.translatef(0.f, mPlaneManipOffsetMeters, mPlaneManipOffsetMeters);
                gGL.scalef(mPlaneScales.mV[VX], mPlaneScales.mV[VX], mPlaneScales.mV[VX]);
                if (mHighlightedPart == LL_YZ_PLANE)
                {
                    color1.setVec(0.f, 1.f, 0.f, 1.f);
                    color2.setVec(0.f, 0.f, 1.f, 1.f);
                }
                else
                {
                    color1.setVec(0.f, 1.f, 0.f, 0.6f);
                    color2.setVec(0.f, 0.f, 1.f, 0.6f);
                }
                gGL.begin(LLRender::TRIANGLES);
                {
                    gGL.color4fv(color1.mV);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f));
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));

                    gGL.color4fv(color2.mV);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f), mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f));
                }
                gGL.end();

                gGL.setLineWidth(3.0f);
                gGL.begin(LLRender::LINES);
                {
                    gGL.color4f(0.f, 0.f, 0.f, 0.3f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.1f,   mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.1f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.1f,   mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.4f);

                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.1f,  mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.1f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.4f,  mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.1f);
                }
                gGL.end();
                gGL.setLineWidth(1.0f);
                gGL.popMatrix();
            }

            if ((mManipPart == LL_NO_PART || mManipPart == LL_XZ_PLANE) && llabs(relative_camera_dir.mV[VY]) > MIN_PLANE_MANIP_DOT_PRODUCT)
            {
                // render XZ plane manipulator
                gGL.pushMatrix();
                gGL.scalef(mPlaneManipPositions.mV[VX], mPlaneManipPositions.mV[VY], mPlaneManipPositions.mV[VZ]);
                gGL.translatef(mPlaneManipOffsetMeters, 0.f, mPlaneManipOffsetMeters);
                gGL.scalef(mPlaneScales.mV[VY], mPlaneScales.mV[VY], mPlaneScales.mV[VY]);
                if (mHighlightedPart == LL_XZ_PLANE)
                {
                    color1.setVec(0.f, 0.f, 1.f, 1.f);
                    color2.setVec(1.f, 0.f, 0.f, 1.f);
                }
                else
                {
                    color1.setVec(0.f, 0.f, 1.f, 0.6f);
                    color2.setVec(1.f, 0.f, 0.f, 0.6f);
                }

                gGL.begin(LLRender::TRIANGLES);
                {
                    gGL.color4fv(color1.mV);
                    gGL.vertex3f(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f), 0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f), 0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), 0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f));

                    gGL.color4fv(color2.mV);
                    gGL.vertex3f(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), 0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f));
                    gGL.vertex3f(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f),   0.f, mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f));
                    gGL.vertex3f(mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f),   0.f, mPlaneManipOffsetMeters * (PLANE_TICK_SIZE * 0.25f));
                }
                gGL.end();

                gGL.setLineWidth(3.0f);
                gGL.begin(LLRender::LINES);
                {
                    gGL.color4f(0.f, 0.f, 0.f, 0.3f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.1f,   0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.1f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.25f,  0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * PLANE_TICK_SIZE  * 0.1f,   0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.4f);

                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  0.f, mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.1f,   0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.1f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.25f,  0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.25f);
                    gGL.vertex3f(mPlaneManipOffsetMeters * -PLANE_TICK_SIZE * 0.4f,   0.f, mPlaneManipOffsetMeters * PLANE_TICK_SIZE * 0.1f);
                }
                gGL.end();
                gGL.setLineWidth(1.0f);

                gGL.popMatrix();
            }

            if ((mManipPart == LL_NO_PART || mManipPart == LL_XY_PLANE) && llabs(relative_camera_dir.mV[VZ]) > MIN_PLANE_MANIP_DOT_PRODUCT)
            {
                // render XY plane manipulator
                gGL.pushMatrix();
                gGL.scalef(mPlaneManipPositions.mV[VX], mPlaneManipPositions.mV[VY], mPlaneManipPositions.mV[VZ]);

/*                            Y
                              ^
                              v1
                              |  \
                              |<- v0
                              |  /| \
                              v2__v__v3 > X
*/
                    LLVector3 v0,v1,v2,v3;
                    gGL.translatef(mPlaneManipOffsetMeters, mPlaneManipOffsetMeters, 0.f);
                    v0 = LLVector3(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.25f), 0.f);
                    v1 = LLVector3(mPlaneManipOffsetMeters * ( PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f), 0.f);
                    v2 = LLVector3(mPlaneManipOffsetMeters * ( PLANE_TICK_SIZE * 0.25f), mPlaneManipOffsetMeters * ( PLANE_TICK_SIZE * 0.25f), 0.f);
                    v3 = LLVector3(mPlaneManipOffsetMeters * (-PLANE_TICK_SIZE * 0.75f), mPlaneManipOffsetMeters * ( PLANE_TICK_SIZE * 0.25f), 0.f);

                    gGL.scalef(mPlaneScales.mV[VZ], mPlaneScales.mV[VZ], mPlaneScales.mV[VZ]);
                    if (mHighlightedPart == LL_XY_PLANE)
                    {
                        color1.setVec(1.f, 0.f, 0.f, 1.f);
                        color2.setVec(0.f, 1.f, 0.f, 1.f);
                    }
                    else
                    {
                        color1.setVec(0.8f, 0.f, 0.f, 0.6f);
                        color2.setVec(0.f, 0.8f, 0.f, 0.6f);
                    }

                    gGL.begin(LLRender::TRIANGLES);
                    {
                        gGL.color4fv(color1.mV);
                        gGL.vertex3fv(v0.mV);
                        gGL.vertex3fv(v1.mV);
                        gGL.vertex3fv(v2.mV);

                        gGL.color4fv(color2.mV);
                        gGL.vertex3fv(v2.mV);
                        gGL.vertex3fv(v3.mV);
                        gGL.vertex3fv(v0.mV);
                    }
                    gGL.end();

                    gGL.setLineWidth(3.0f);
                    gGL.begin(LLRender::LINES);
                    {
                        gGL.color4f(0.f, 0.f, 0.f, 0.3f);
                        LLVector3 v12 = (v1 + v2) * .5f;
                        gGL.vertex3fv(v0.mV);
                        gGL.vertex3fv(v12.mV);
                        gGL.vertex3fv(v12.mV);
                        gGL.vertex3fv((v12 + (v0-v12)*.3f + (v2-v12)*.3f).mV);
                        gGL.vertex3fv(v12.mV);
                        gGL.vertex3fv((v12 + (v0-v12)*.3f + (v1-v12)*.3f).mV);

                        LLVector3 v23 = (v2 + v3) * .5f;
                        gGL.vertex3fv(v0.mV);
                        gGL.vertex3fv(v23.mV);
                        gGL.vertex3fv(v23.mV);
                        gGL.vertex3fv((v23 + (v0-v23)*.3f + (v3-v23)*.3f).mV);
                        gGL.vertex3fv(v23.mV);
                        gGL.vertex3fv((v23 + (v0-v23)*.3f + (v2-v23)*.3f).mV);
                    }
                    gGL.end();
                    gGL.setLineWidth(1.0f);

                gGL.popMatrix();
            }
        }
        {
            gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

            // Since we draw handles with depth testing off, we need to draw them in the
            // proper depth order.

            // Copied from LLDrawable::updateGeometry
            LLVector3 pos_agent     = mJoint->getWorldPosition();
            LLVector3 camera_agent  = gAgentCamera.getCameraPositionAgent();
            LLVector3 headPos       = pos_agent - camera_agent;

            LLVector3 orientWRTHead    = headPos * invRotation;

            // Find nearest vertex
            U32 nearest = (orientWRTHead.mV[0] < 0.0f ? 1 : 0) +
                (orientWRTHead.mV[1] < 0.0f ? 2 : 0) +
                (orientWRTHead.mV[2] < 0.0f ? 4 : 0);

            // opposite faces on Linden cubes:
            // 0 & 5
            // 1 & 3
            // 2 & 4

            // Table of order to draw faces, based on nearest vertex
            constexpr U32 face_list[8][NUM_AXES * 2] = {
                { 2,0,1, 4,5,3 }, // v6  F201 F453
                { 2,0,3, 4,5,1 }, // v7  F203 F451
                { 4,0,1, 2,5,3 }, // v5  F401 F253
                { 4,0,3, 2,5,1 }, // v4  F403 F251
                { 2,5,1, 4,0,3 }, // v2  F251 F403
                { 2,5,3, 4,0,1 }, // v3  F253 F401
                { 4,5,1, 2,0,3 }, // v1  F451 F203
                { 4,5,3, 2,0,1 }, // v0  F453 F201
            };
            constexpr EManipPart which_arrow[6] = {
                LL_Z_ARROW,
                LL_X_ARROW,
                LL_Y_ARROW,
                LL_X_ARROW,
                LL_Y_ARROW,
                LL_Z_ARROW };

            // draw arrows for deeper faces first, closer faces last
            LLVector3 camera_axis;
            camera_axis.setVec(gAgentCamera.getCameraPositionAgent() - mJoint->getWorldPosition());

            for (U32 i = 0; i < NUM_AXES*2; i++)
            {
                U32 face = face_list[nearest][i];

                LLVector3 arrow_axis;
                getManipJointAxis(mJoint, which_arrow[face], arrow_axis);

                renderArrow(which_arrow[face],
                            mManipPart,
                            (face >= 3) ? -mConeSize : mConeSize,
                            (face >= 3) ? -mArrowLengthMeters : mArrowLengthMeters,
                            mConeSize,
                            false);
            }
        }
    }
    gGL.popMatrix();
}

void FSManipTranslateJoint::renderArrow(S32 which_arrow, S32 selected_arrow, F32 box_size, F32 arrow_size, F32 handle_size, bool reverse_direction)
{
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    LLGLEnable gls_blend(GL_BLEND);

    for (S32 pass = 1; pass <= 2; pass++)
    {
        LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, pass == 1 ? GL_LEQUAL : GL_GREATER);
        gGL.pushMatrix();

        S32 index = 0;

        index = ARROW_TO_AXIS[which_arrow];

        // assign a color for this arrow
        LLColor4 color;  // black
        if (which_arrow == selected_arrow || which_arrow == mHighlightedPart)
        {
            color.mV[index] = (pass == 1) ? 1.f : 0.5f;
        }
        else if (selected_arrow != LL_NO_PART)
        {
            color.mV[VALPHA] = 0.f;
        }
        else
        {
            color.mV[index] = pass == 1 ? .8f : .35f;          // red, green, or blue
            color.mV[VALPHA] = 0.6f;
        }
        gGL.color4fv(color.mV);

        LLVector3 vec;

        {
            gGL.setLineWidth(2.0f);
            gGL.begin(LLRender::LINES);
                vec.mV[index] = box_size;
                gGL.vertex3f(vec.mV[VX], vec.mV[VY], vec.mV[VZ]);

                vec.mV[index] = arrow_size;
                gGL.vertex3f(vec.mV[VX], vec.mV[VY], vec.mV[VZ]);
            gGL.end();
            gGL.setLineWidth(1.0f);
        }

        gGL.translatef(vec.mV[VX], vec.mV[VY], vec.mV[VZ]);
        gGL.scalef(handle_size, handle_size, handle_size);

        F32 rot = 0.0f;
        LLVector3 axis;

        switch(which_arrow)
        {
        case LL_X_ARROW:
            rot = reverse_direction ? -90.0f : 90.0f;
            axis.mV[1] = 1.0f;
            break;
        case LL_Y_ARROW:
            rot = reverse_direction ? 90.0f : -90.0f;
            axis.mV[0] = 1.0f;
            break;
        case LL_Z_ARROW:
            rot = reverse_direction ? 180.0f : 0.0f;
            axis.mV[0] = 1.0f;
            break;
        default:
            LL_ERRS() << "renderArrow called with bad arrow " << which_arrow << LL_ENDL;
            break;
        }

        gGL.diffuseColor4fv(color.mV);
        gGL.rotatef(rot, axis.mV[VX], axis.mV[VY], axis.mV[VZ]);
        gGL.scalef(mArrowScales.mV[index], mArrowScales.mV[index], mArrowScales.mV[index] * 1.5f);

        gCone.render();

        gGL.popMatrix();
    }
}

bool FSManipTranslateJoint::isAvatarJointSafeToUse()
{
    if (!mJoint || !mAvatar)
        return false;

    if (mAvatar->isDead() || !mAvatar->isFullyLoaded())
    {
        setAvatar(nullptr);
        return false;
    }

    return true;
}

bool FSManipTranslateJoint::isMoveableJoint()
{
    if (!mJoint)
        return false;

    std::string jointName = mJoint->getName();
    if (jointName == "mPelvis" || jointName == "mRoot")
        return true; // on the root we want to do a simple translation

    // if the parent or grandparent joints are null or root, we don't want to do anything
    LLJoint* parentJoint = mJoint->getParent();
    if (!parentJoint)
        return false;

    jointName = parentJoint->getName();
    if (jointName == "mPelvis" || jointName == "mRoot")
        return false;

    LLJoint* grandParentJoint = parentJoint->getParent();
    if (!grandParentJoint)
        return false;

    jointName = grandParentJoint->getName();
    if (jointName == "mPelvis" || jointName == "mRoot")
        return false;

    return true;
}

// virtual
bool FSManipTranslateJoint::canAffectSelection()
{
    return isAvatarJointSafeToUse();
}

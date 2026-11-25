/**
 * @file fstoolhudmove.cpp
 * @brief Ctrl+drag moves a HUD on screen without the build tools
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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

#include "fstoolhudmove.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
#include "llmanip.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "pipeline.h"
#include "llvoavatarself.h"
#include "message.h"
#include "rlvactions.h"

namespace FSHudMove
{
    constexpr S32 DRAG_SLOP = 3; // pixels before a Ctrl+click becomes a drag

    // Rotation of the HUD attachment point the object hangs from, to turn a HUD space move into its local one
    bool attachment_point_rotation(const LLViewerObject* object, LLQuaternion& rotation)
    {
        if (object->mDrawable.isNull() || !object->mDrawable->mXform.getParent())
        {
            return false;
        }
        rotation = object->mDrawable->mXform.getParent()->getWorldRotation();
        return true;
    }
}

FSToolHudMove::FSToolHudMove()
:   LLTool(std::string("HUD Move"))
{
}

// static
LLViewerObject* FSToolHudMove::movable(LLViewerObject* object, MASK mask)
{
    static LLCachedControl<bool> drag_huds(gSavedSettings, "FSCtrlDragHUDs", false);
    if (!drag_huds || mask != MASK_CONTROL || !object || object->isDead() || !object->isHUDAttachment())
    {
        return nullptr;
    }

    LLViewerObject* root = object->getRootEdit();
    if (!root || root->isDead() || !root->isHUDAttachment() || !root->getRegion())
    {
        return nullptr;
    }
    if (!root->permModify() || !root->permMove() || !RlvActions::canEdit(root))
    {
        return nullptr;
    }
    return root;
}

// static
bool FSToolHudMove::overMovableHUD(MASK mask)
{
    static LLCachedControl<bool> drag_huds(gSavedSettings, "FSCtrlDragHUDs", false);
    if (!drag_huds || mask != MASK_CONTROL || !isAgentAvatarValid() || !gAgentAvatarp->hasHUDAttachment())
    {
        return false;
    }

    static U32 checked_frame = 0;
    static S32 checked_x = -1;
    static S32 checked_y = -1;
    static bool over = false;
    const LLCoordGL mouse = gViewerWindow->getCurrentMouse();
    if (checked_frame != gFrameCount || checked_x != mouse.mX || checked_y != mouse.mY)
    {
        checked_frame = gFrameCount;
        checked_x = mouse.mX;
        checked_y = mouse.mY;

        // The HUD part of a pick: a ray straight through the HUD at the pointer
        constexpr F32 DEPTH = 512.f;
        const LLVector3 point = gViewerWindow->mousePointHUD(mouse.mX, mouse.mY);
        LLVector4a start, end;
        start.load3((point - LLVector3(DEPTH, 0.f, 0.f)).mV);
        end.load3((point + LLVector3(DEPTH, 0.f, 0.f)).mV);
        S32 face = -1;
        over = movable(gPipeline.lineSegmentIntersectInHUD(start, end, false, &face), mask) != nullptr;
    }
    return over;
}

bool FSToolHudMove::start(const LLPickInfo& pick)
{
    LLViewerObject* root = movable(pick.getObject(), pick.mKeyMask);
    LLQuaternion rotation;
    if (!root || !FSHudMove::attachment_point_rotation(root, rotation))
    {
        return false;
    }

    mObject = root;
    mStartPosition = root->getPosition();
    mStartMouseHUD = gViewerWindow->mousePointHUD(pick.mMousePt.mX, pick.mMousePt.mY);
    mMouseDownX = pick.mMousePt.mX;
    mMouseDownY = pick.mMousePt.mY;
    mMoved = false;

    setMouseCapture(true);
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);
    return true;
}

bool FSToolHudMove::handleHover(S32 x, S32 y, MASK mask)
{
    if (!hasMouseCapture())
    {
        return false;
    }
    gViewerWindow->setCursor(UI_CURSOR_TOOLTRANSLATE);

    if (!mMoved && llabs(x - mMouseDownX) < FSHudMove::DRAG_SLOP && llabs(y - mMouseDownY) < FSHudMove::DRAG_SLOP)
    {
        return true;
    }
    mMoved = true;
    moveTo(x, y);
    return true;
}

bool FSToolHudMove::handleMouseUp(S32 x, S32 y, MASK mask)
{
    if (hasMouseCapture())
    {
        if (mMoved)
        {
            moveTo(x, y);
        }
        // Lets go of the mouse, which sends the position onMouseCaptureLost
        setMouseCapture(false);
    }
    return true;
}

void FSToolHudMove::onMouseCaptureLost()
{
    finish();
}

void FSToolHudMove::moveTo(S32 x, S32 y)
{
    LLQuaternion rotation;
    if (mObject.isNull() || mObject->isDead() || !mObject->isHUDAttachment() || !FSHudMove::attachment_point_rotation(mObject, rotation))
    {
        // Gone or detached while dragging
        mObject = nullptr;
        return;
    }

    const LLVector3 move_hud = gViewerWindow->mousePointHUD(x, y) - mStartMouseHUD;
    mObject->setPosition(mStartPosition + move_hud * ~rotation);
    LLManip::rebuild(mObject);
    if (isAgentAvatarValid())
    {
        gAgentAvatarp->clampAttachmentPositions();
    }
}

void FSToolHudMove::finish()
{
    LLPointer<LLViewerObject> object = mObject;
    const bool moved = mMoved;
    mObject = nullptr;
    mMoved = false;

    if (!moved || object.isNull() || object->isDead() || !object->getRegion() || object->getPosition() == mStartPosition)
    {
        // Gone or detached while dragging
        return;
    }

    // The new position of the root relative to its attachment point
    U8 data[12];
    htolememcpy(&data[0], &(object->getPosition().mV), MVT_LLVector3, 12);

    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_MultipleObjectUpdate);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->nextBlockFast(_PREHASH_ObjectData);
    msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
    msg->addU8Fast(_PREHASH_Type, UPD_POSITION | UPD_LINKED_SETS);
    msg->addBinaryDataFast(_PREHASH_Data, data, 12);
    msg->sendReliable(object->getRegion()->getHost());
}

/**
 * @file bsremoteactions.cpp
 * @brief Permission-gated avatar actions used by Blazing Storm remote control.
 *
 * This file is part of the Blazing Storm viewer fork.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremoteactions.h"

#include "blazingstorm/remote/bsremotesession.h"
#include "fsnearbychathub.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llvoavatarself.h"
#include "llviewerjoystick.h"
#include "rlvhandler.h"
#include "llviewerobjectlist.h"
#include "llviewerobject.h"

namespace BlazingStorm
{
    RemoteActions& RemoteActions::instance()
    {
        static RemoteActions actions;
        return actions;
    }

    bool RemoteActions::canMove() const
    {
        return RemoteSession::instance().hasPermission(RemotePermission::Movement)
            && !gAgent.isMovementLocked();
    }

    bool RemoteActions::cameraStep(RemoteCommandType command)
    {
        if (!RemoteSession::instance().hasPermission(RemotePermission::Camera)
            || !isAgentAvatarValid()
            || !gAgentCamera.cameraThirdPerson()
            || LLViewerJoystick::getInstance()->getOverrideCamera()
            || gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)
            || gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK)) return false;

        if (command == RemoteCommandType::CameraReset)
        {
            releaseCamera();
            return true;
        }

        // Orbit with a detached focus so camera permission cannot rotate the avatar.
        gAgentCamera.unlockView();
        mCameraControlled = true;
        switch (command)
        {
            case RemoteCommandType::CameraLeft:  gAgentCamera.cameraOrbitAround(0.1f); break;
            case RemoteCommandType::CameraRight: gAgentCamera.cameraOrbitAround(-0.1f); break;
            case RemoteCommandType::CameraUp:    gAgentCamera.cameraOrbitOver(0.1f); break;
            case RemoteCommandType::CameraDown:  gAgentCamera.cameraOrbitOver(-0.1f); break;
            case RemoteCommandType::CameraIn:    gAgentCamera.cameraOrbitIn(0.25f); break;
            case RemoteCommandType::CameraOut:   gAgentCamera.cameraOrbitIn(-0.25f); break;
            default: return false;
        }
        return true;
    }

    void RemoteActions::releaseCamera()
    {
        if (!mCameraControlled) return;
        mCameraControlled = false;
        // Return normal avatar focus without the resetView avatar-axis side effect.
        gAgentCamera.setFocusOnAvatar(true, false, false);
    }

    bool RemoteActions::cameraFocus(const std::string& object_id)
    {
        LLUUID id;
        if (!id.set(object_id, false) || id.isNull()
            || !RemoteSession::instance().hasPermission(RemotePermission::Camera)
            || !isAgentAvatarValid() || !gAgentCamera.cameraThirdPerson()
            || LLViewerJoystick::getInstance()->getOverrideCamera()
            || gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM)
            || gRlvHandler.hasBehaviour(RLV_BHVR_SETCAM_UNLOCK)) return false;
        auto* object = gObjectList.findObject(id);
        if (!object || object->isDead() || !object->getRegion() || object->isHUDAttachment()) return false;
        gAgentCamera.unlockView();
        gAgentCamera.setFocusGlobal(object->getPositionGlobal(), id);
        mCameraControlled = true;
        return true;
    }

    void RemoteActions::beginMovement()
    {
        gAgent.stopAutoPilot(true);
    }

    bool RemoteActions::moveForward()
    {
        if (!canMove()) return false;
        beginMovement();
        mForward = 1;
        return true;
    }

    bool RemoteActions::moveBackward()
    {
        if (!canMove()) return false;
        beginMovement();
        mForward = -1;
        return true;
    }

    bool RemoteActions::strafeLeft()
    {
        if (!canMove()) return false;
        beginMovement();
        mStrafe = 1;
        return true;
    }

    bool RemoteActions::strafeRight()
    {
        if (!canMove()) return false;
        beginMovement();
        mStrafe = -1;
        return true;
    }

    bool RemoteActions::turnLeft()
    {
        if (!canMove()) return false;
        beginMovement();
        mTurn = 1.f;
        return true;
    }

    bool RemoteActions::turnRight()
    {
        if (!canMove()) return false;
        beginMovement();
        mTurn = -1.f;
        return true;
    }

    bool RemoteActions::moveUp()
    {
        if (!canMove()) return false;
        beginMovement();
        mVertical = 1;
        return true;
    }

    bool RemoteActions::moveDown()
    {
        if (!canMove()) return false;
        beginMovement();
        mVertical = -1;
        return true;
    }

    bool RemoteActions::flyOn()
    {
        if (!canMove()) return false;
        beginMovement();
        gAgent.setFlying(true, true);
        return gAgent.getFlying();
    }

    bool RemoteActions::flyOff()
    {
        if (!canMove()) return false;
        beginMovement();
        gAgent.setFlying(false);
        return true;
    }

    bool RemoteActions::toggleFly()
    {
        if (!canMove()) return false;
        beginMovement();
        gAgent.setFlying(!gAgent.getFlying(), true);
        return true;
    }

    bool RemoteActions::jump()
    {
        if (!canMove()) return false;
        beginMovement();
        mJumpPending = true;
        return true;
    }

    bool RemoteActions::say(const std::string& text)
    {
        if (!RemoteSession::instance().hasPermission(RemotePermission::Chat) || text.empty())
        {
            return false;
        }

        FSNearbyChat::instance().sendChatFromViewer(text, CHAT_TYPE_NORMAL, true);
        return true;
    }

    void RemoteActions::stopForward()
    {
        mForward = 0;
    }

    void RemoteActions::stopStrafe()
    {
        mStrafe = 0;
    }

    void RemoteActions::stopTurn()
    {
        mTurn = 0.f;
    }

    void RemoteActions::stopVertical()
    {
        mVertical = 0;
    }

    void RemoteActions::stopMovement()
    {
        stopForward();
        stopStrafe();
        stopTurn();
        stopVertical();
        mJumpPending = false;
    }

    void RemoteActions::update()
    {
        if (!RemoteSession::instance().hasPermission(RemotePermission::Camera))
            releaseCamera();
        if (!canMove())
        {
            stopMovement();
            return;
        }

        if (mForward != 0)
        {
            gAgent.moveAt(mForward, false);
        }

        if (mStrafe != 0)
        {
            gAgent.moveLeft(mStrafe);
        }

        if (mTurn != 0.f)
        {
            gAgent.moveYaw(mTurn, false);
        }

        if (mVertical != 0)
        {
            gAgent.moveUp(mVertical);
        }

        if (mJumpPending)
        {
            gAgent.moveUp(1);
            mJumpPending = false;
        }
    }
}

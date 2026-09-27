/**
 * @file bsremoteactions.h
 * @brief Permission-gated avatar actions used by Blazing Storm remote control.
 *
 * This file is part of the Blazing Storm viewer fork.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 */

#ifndef BS_REMOTE_ACTIONS_H
#define BS_REMOTE_ACTIONS_H

#include <string>

namespace BlazingStorm
{
    class RemoteActions final
    {
    public:
        static RemoteActions& instance();

        bool moveForward();
        bool moveBackward();
        bool strafeLeft();
        bool strafeRight();
        bool turnLeft();
        bool turnRight();
        bool jump();
        bool say(const std::string& text);

        void stopMovement();
        void update();

        int forwardState() const { return mForward; }
        int strafeState() const { return mStrafe; }
        float turnState() const { return mTurn; }

    private:
        RemoteActions() = default;

        bool canMove() const;
        void beginMovement();

        int mForward = 0;
        int mStrafe = 0;
        float mTurn = 0.f;
        bool mJumpPending = false;
    };
}

#endif // BS_REMOTE_ACTIONS_H

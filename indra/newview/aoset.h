/**
 * @file aoset.h
 * @brief Implementation of an Animation Overrider set of animations
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#ifndef AOSET_H
#define AOSET_H

#include "lleventtimer.h"

class LLInventoryItem;

class AOSet
:   public LLEventTimer
{
    public:
        AOSet(const LLUUID inventoryID);
        ~AOSet();

        // keep number and order in sync with list of names in the constructor
        enum
        {
            Start = 0,      // convenience, so we don't have to know the name of the first state
            Standing = 0,
            Walking,
            Running,
            Sitting,
            SittingOnGround,
            Crouching,
            CrouchWalking,
            Landing,
            SoftLanding,
            StandingUp,
            Falling,
            FlyingDown,
            FlyingUp,
            Flying,
            FlyingSlow,
            Hovering,
            Jumping,
            PreJumping,
            TurningRight,
            TurningLeft,
            Typing,
            Floating,
            SwimmingForward,
            SwimmingUp,
            SwimmingDown,
            AOSTATES_MAX
        };

        struct AOAnimation
        {
            std::string mName;
            LLUUID mAssetUUID;
            LLUUID mInventoryUUID;
            LLUUID mOriginalUUID;
            S32 mSortOrder;
        };

        struct AOState
        {
            std::string mName;
            std::vector<std::string> mAlternateNames;
            std::vector<const LLInventoryItem*> mAddQueue;
            LLUUID mRemapID;
            bool mCycle;
            bool mRandom;
            F32 mCycleTime;
            std::vector<AOAnimation> mAnimations;
            U32 mCurrentAnimation;
            LLUUID mCurrentAnimationID;
            LLUUID mInventoryUUID;
            bool mDirty;
        };

        const LLUUID& getInventoryUUID() const;
        void setInventoryUUID(const LLUUID& inventoryID);

        const std::string& getName() const;
        void setName(const std::string& name);

        bool getSitOverride() const;
        void setSitOverride(bool override_sit);

        bool getSmart() const;
        void setSmart(bool smart);

        bool getMouselookStandDisable() const;
        void setMouselookStandDisable(bool disable);

        bool getComplete() const;
        void setComplete(bool complete);

        const LLUUID& getMotion() const;
        void setMotion(const LLUUID& motion);

        bool getDirty() const;
        void setDirty(bool dirty);

        AOState* getState(S32 eName);
        AOState* getStateByName(const std::string& name);
        AOState* getStateByRemapID(const LLUUID& id);
        const LLUUID& getAnimationForState(AOState* state) const;

        void startTimer(F32 timeout);
        void stopTimer();
        bool tick() override;

        std::vector<std::string> mStateNames;

    protected:
        LLUUID mInventoryID;

        std::string mName;
        bool mSitOverride;
        bool mSmart;
        bool mMouselookStandDisable;
        bool mComplete;
        LLUUID mCurrentMotion;
        bool mDirty;

        AOState mStates[AOSTATES_MAX];
};

#endif // AOSET_H

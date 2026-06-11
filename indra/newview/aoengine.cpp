/**
 * @file aoengine.cpp
 * @brief The core Animation Overrider engine
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

#include "llviewerprecompiledheaders.h"


#include "roles_constants.h"

#include "aoengine.h"
#include "aoset.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llanimationstates.h"
#include "llassetstorage.h"
#include "llfilesystem.h"
#include "llinventoryfunctions.h"       // for ROOT_FIRESTORM_FOLDER
#include "llinventorymodel.h"
#include "llnotificationsutil.h"
#include "llstring.h"
#include "llviewercontrol.h"
#include "llxorcipher.h"

#define ROOT_AO_FOLDER "#AO"

static const LLUUID ENCRYPTION_MAGIC_ID("4b552ff5-fd63-408c-8288-cd09429852ba");
constexpr F32 INVENTORY_POLLING_INTERVAL = 5.0f;

AOEngine::AOEngine() :
    LLSingleton<AOEngine>(),
    mCurrentSet(nullptr),
    mDefaultSet(nullptr),
    mEnabled(false),
    mEnabledStands(false),
    mInMouselook(false),
    mUnderWater(false),
    mImportSet(nullptr),
    mAOFolder(LLUUID::null),
    mLastMotion(ANIM_AGENT_STAND),
    mLastOverriddenMotion(ANIM_AGENT_STAND)
{
    gSavedPerAccountSettings.getControl("UseAO")->getCommitSignal()->connect(boost::bind(&AOEngine::onToggleAOControl, this));
    gSavedPerAccountSettings.getControl("UseAOStands")->getCommitSignal()->connect(boost::bind(&AOEngine::onToggleAOStandsControl, this));
    gSavedPerAccountSettings.getControl("PauseAO")->getCommitSignal()->connect(boost::bind(&AOEngine::onPauseAO, this));

    mRegionChangeConnection = gAgent.addRegionChangedCallback(boost::bind(&AOEngine::onRegionChange, this));
}

AOEngine::~AOEngine()
{
    LLSD currentState = LLSD::emptyMap();
    if (mCurrentSet)
    {
        currentState = LLSD().with("CurrentSet", mCurrentSet->getName());
        LLSD currentAnimations;
        for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
        {
            // store the active step's first member asset as the shadow id
            if (auto state = mCurrentSet->getState(index); state && state->mCurrentAnimation < state->mSteps.size() && !state->mSteps[state->mCurrentAnimation].mMembers.empty())
            {
                LL_DEBUGS("AOEngine") << "Storing current animation for state " << index << ": Step index " << state->mCurrentAnimation << LL_ENDL;
                LLUUID shadow_id{ state->mSteps[state->mCurrentAnimation].mMembers.front().mAssetUUID };
                LLXORCipher cipher(ENCRYPTION_MAGIC_ID.mData, UUID_BYTES);
                cipher.encrypt(shadow_id.mData, UUID_BYTES);
                currentAnimations.insert(state->mName, shadow_id);
            }
            else
            {
                LL_DEBUGS("AOEngine") << "No state " << index << " or no animations defined for this state" << LL_ENDL;
            }
        }
        currentState.insert("CurrentStateAnimations", currentAnimations);
    }
    LL_DEBUGS("AOEngine") << "Stored AO state: " << currentState << LL_ENDL;
    gSavedPerAccountSettings.setLLSD("FSCurrentAOState", currentState);

    clear(false);

    if (mRegionChangeConnection.connected())
    {
        mRegionChangeConnection.disconnect();
    }
}

void AOEngine::init()
{
    bool do_enable = gSavedPerAccountSettings.getBOOL("UseAO");
    bool do_enable_stands = gSavedPerAccountSettings.getBOOL("UseAOStands");
    if (do_enable)
    {
        // enable_stands() calls enable(), but we need to set the
        // mEnabled variable properly
        mEnabled = true;
        // Enabling the AO always enables stands to start with
        enableStands(true);
    }
    else
    {
        enableStands(do_enable_stands);
        enable(false);
    }
}

// static
void AOEngine::onLoginComplete()
{
    AOEngine::instance().init();
}

void AOEngine::onToggleAOControl()
{
    enable(gSavedPerAccountSettings.getBOOL("UseAO"));
    if (mEnabled)
    {
        // Enabling the AO always enables stands to start with
        gSavedPerAccountSettings.setBOOL("UseAOStands", true);
    }
}

void AOEngine::onToggleAOStandsControl()
{
    enableStands(gSavedPerAccountSettings.getBOOL("UseAOStands"));
}

void AOEngine::onPauseAO()
{
    // can't use mEnabled here as that gets switched over by enable()
    if (gSavedPerAccountSettings.getBOOL("UseAO"))
    {
        enable(!gSavedPerAccountSettings.getBOOL("PauseAO"));
    }
}

void AOEngine::clear(bool fromTimer)
{
    stopActiveTracks();
    stopAlwaysAnimations();

    std::move(mSets.begin(), mSets.end(), std::back_inserter(mOldSets));
    mSets.clear();

    mCurrentSet = nullptr;

    //<ND/> FIRE-3801; We cannot delete any AOSet object if we're called from a timer tick. AOSet is derived from LLEventTimer and destruction will
    // fail in ~LLInstanceTracker when a destructor runs during iteration.
    if (!fromTimer)
    {
        std::for_each(mOldSets.begin(), mOldSets.end(), DeletePointer());
        mOldSets.clear();

        std::for_each(mOldImportSets.begin(), mOldImportSets.end(), DeletePointer());
        mOldImportSets.clear();
    }
}

void AOEngine::stopAllStandVariants()
{
    LL_DEBUGS("AOEngine") << "stopping all STAND variants." << LL_ENDL;
    gAgent.sendAnimationRequest(ANIM_AGENT_STAND_1, ANIM_REQUEST_STOP);
    gAgent.sendAnimationRequest(ANIM_AGENT_STAND_2, ANIM_REQUEST_STOP);
    gAgent.sendAnimationRequest(ANIM_AGENT_STAND_3, ANIM_REQUEST_STOP);
    gAgent.sendAnimationRequest(ANIM_AGENT_STAND_4, ANIM_REQUEST_STOP);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_1);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_2);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_3);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_STAND_4);
}

void AOEngine::stopAllSitVariants()
{
    LL_DEBUGS("AOEngine") << "stopping all SIT variants." << LL_ENDL;
    gAgent.sendAnimationRequest(ANIM_AGENT_SIT_FEMALE, ANIM_REQUEST_STOP);
    gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_FEMALE);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GENERIC);

    // scripted seats that use ground_sit as animation need special treatment
    const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
    if (agentRoot && agentRoot->getID() != gAgentID)
    {
        LL_DEBUGS("AOEngine") << "Not stopping ground sit animations while sitting on a prim." << LL_ENDL;
        return;
    }

    gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND, ANIM_REQUEST_STOP);
    gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GROUND_CONSTRAINED, ANIM_REQUEST_STOP);

    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND);
    gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_SIT_GROUND_CONSTRAINED);
}

void AOEngine::stopCurrentAnimations(AOSet::AOState* state, bool skipFirst)
{
    for (size_t index = skipFirst ? 1 : 0; index < state->mCurrentAnimationIDs.size(); ++index)
    {
        const LLUUID& animation = state->mCurrentAnimationIDs[index];
        if (animation.notNull())
        {
            gAgent.sendAnimationRequest(animation, ANIM_REQUEST_STOP);
            if (isAgentAvatarValid())
            {
                gAgentAvatarp->LLCharacter::stopMotion(animation);
            }
        }
    }
    state->mCurrentAnimationIDs.clear();
}

bool AOEngine::swapStateAnimations(AOSet::AOState* state, const uuid_vec_t& animations)
{
    uuid_vec_t oldAnimations = state->mCurrentAnimationIDs;
    if (animations == oldAnimations)
    {
        return false;
    }

    state->mCurrentAnimationIDs = animations;

    for (const LLUUID& animation : animations)
    {
        if (std::find(oldAnimations.begin(), oldAnimations.end(), animation) == oldAnimations.end())
        {
            gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
        }
    }

    for (const LLUUID& oldAnimation : oldAnimations)
    {
        if (std::find(animations.begin(), animations.end(), oldAnimation) == animations.end())
        {
            gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
            if (isAgentAvatarValid())
            {
                gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
            }
        }
    }

    return true;
}

void AOEngine::startStateTracks(AOSet::AOState* state)
{
    stopActiveTracks();

    if (!state || !mEnabled)
    {
        return;
    }

    for (size_t index = 0; index < state->mTracks.size(); ++index)
    {
        AOSet::AOTrack& track = state->mTracks[index];
        if (track.mAnimations.empty())
        {
            continue;
        }

        track.mCurrentAnimationID = mCurrentSet ? mCurrentSet->getAnimationForTrack(track) : LLUUID::null;
        if (track.mCurrentAnimationID.isNull())
        {
            continue;
        }

        gAgent.sendAnimationRequest(track.mCurrentAnimationID, ANIM_REQUEST_START);

        // tracks with only one animation and no cycling don't need a timer at all
        if (track.mCycle && track.mCycleTime > 0.0f && track.mAnimations.size() > 1)
        {
            mActiveTrackTimers.emplace_back(std::make_unique<AOTrackTimer>(state->mNum, static_cast<S32>(index), track.mCycleTime));
        }
    }

    mActiveTrackState = state;
    mAnimationChangedSignal(LLUUID::null);
}

void AOEngine::stopActiveTracks()
{
    mActiveTrackTimers.clear();

    if (!mActiveTrackState)
    {
        return;
    }

    for (AOSet::AOTrack& track : mActiveTrackState->mTracks)
    {
        if (track.mCurrentAnimationID.notNull())
        {
            gAgent.sendAnimationRequest(track.mCurrentAnimationID, ANIM_REQUEST_STOP);
            if (isAgentAvatarValid())
            {
                gAgentAvatarp->LLCharacter::stopMotion(track.mCurrentAnimationID);
            }
            track.mCurrentAnimationID.setNull();
        }
    }

    mActiveTrackState = nullptr;
}

void AOEngine::startAlwaysAnimations()
{
    AOSet::AOState* state = (mEnabled && mCurrentSet) ? mCurrentSet->getState(AOSet::Always) : nullptr;

    if (state && state == mActiveAlwaysState)
    {
        return;
    }

    stopAlwaysAnimations();

    if (!state || (state->mSteps.empty() && state->mTracks.empty()))
    {
        return;
    }

    if (!state->mSteps.empty())
    {
        uuid_vec_t animations;
        mCurrentSet->getAnimationsForState(state, animations);
        state->mCurrentAnimationIDs = animations;
        for (const LLUUID& animation : animations)
        {
            gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
        }
    }

    for (AOSet::AOTrack& track : state->mTracks)
    {
        if (track.mAnimations.empty())
        {
            continue;
        }

        track.mCurrentAnimationID = mCurrentSet->getAnimationForTrack(track);
        if (track.mCurrentAnimationID.notNull())
        {
            gAgent.sendAnimationRequest(track.mCurrentAnimationID, ANIM_REQUEST_START);
        }
    }

    mActiveAlwaysState = state;
    refreshAlwaysTimers();
    mAnimationChangedSignal(LLUUID::null);
}

void AOEngine::stopAlwaysAnimations()
{
    mAlwaysTimers.clear();

    if (!mActiveAlwaysState)
    {
        return;
    }

    stopCurrentAnimations(mActiveAlwaysState);

    for (AOSet::AOTrack& track : mActiveAlwaysState->mTracks)
    {
        if (track.mCurrentAnimationID.notNull())
        {
            gAgent.sendAnimationRequest(track.mCurrentAnimationID, ANIM_REQUEST_STOP);
            if (isAgentAvatarValid())
            {
                gAgentAvatarp->LLCharacter::stopMotion(track.mCurrentAnimationID);
            }
            track.mCurrentAnimationID.setNull();
        }
    }

    mActiveAlwaysState = nullptr;
}

void AOEngine::refreshAlwaysTimers()
{
    mAlwaysTimers.clear();

    AOSet::AOState* state = mActiveAlwaysState;
    if (!state)
    {
        return;
    }

    if (state->mCycle && state->mCycleTime > 0.0f && state->mSteps.size() > 1)
    {
        mAlwaysTimers.emplace_back(std::make_unique<AOTrackTimer>(state->mNum, AO_TRACK_INDEX_STEPS, state->mCycleTime));
    }

    for (size_t index = 0; index < state->mTracks.size(); ++index)
    {
        const AOSet::AOTrack& track = state->mTracks[index];
        if (track.mCycle && track.mCycleTime > 0.0f && track.mAnimations.size() > 1)
        {
            mAlwaysTimers.emplace_back(std::make_unique<AOTrackTimer>(state->mNum, static_cast<S32>(index), track.mCycleTime));
        }
    }
}

void AOEngine::cycleAlwaysStep()
{
    AOSet::AOState* state = mActiveAlwaysState;
    if (!mEnabled || !mCurrentSet || !state || state->mSteps.empty())
    {
        return;
    }

    uuid_vec_t animations;
    mCurrentSet->getAnimationsForState(state, animations);

    if (swapStateAnimations(state, animations))
    {
        mAnimationChangedSignal(LLUUID::null);
    }
}

void AOEngine::trackTimeout(S32 stateNum, S32 trackIndex)
{
    if (!mEnabled || !mCurrentSet)
    {
        return;
    }

    AOSet::AOState* state = nullptr;
    if (mActiveTrackState && mActiveTrackState->mNum == stateNum)
    {
        state = mActiveTrackState;
    }
    else if (mActiveAlwaysState && mActiveAlwaysState->mNum == stateNum)
    {
        state = mActiveAlwaysState;
    }

    if (!state)
    {
        return;
    }

    if (trackIndex == AO_TRACK_INDEX_STEPS)
    {
        if (state == mActiveAlwaysState)
        {
            cycleAlwaysStep();
        }
        return;
    }

    if (trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return;
    }

    AOSet::AOTrack& track = state->mTracks[trackIndex];

    LLUUID oldAnimation = track.mCurrentAnimationID;
    LLUUID animation = mCurrentSet->getAnimationForTrack(track);

    if (animation == oldAnimation)
    {
        return;
    }

    track.mCurrentAnimationID = animation;
    if (animation.notNull())
    {
        gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
    }

    if (oldAnimation.notNull())
    {
        gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
        if (isAgentAvatarValid())
        {
            gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
        }
    }

    mAnimationChangedSignal(LLUUID::null);
}

void AOEngine::setLastMotion(const LLUUID& motion)
{
    if (motion != ANIM_AGENT_TYPE)
    {
        mLastMotion = motion;
    }
}

void AOEngine::setLastOverriddenMotion(const LLUUID& motion)
{
    if (motion != ANIM_AGENT_TYPE)
    {
        mLastOverriddenMotion = motion;
    }
}

bool AOEngine::foreignAnimations() const
{
    // checking foreign animations only makes sense when smart sit is enabled
    if (!mCurrentSet->getSmart())
    {
        return false;
    }

    // get the seat the avatar is sitting on
    const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
    if (!agentRoot)
    {
        // this should not happen, ever
        return false;
    }

    const LLUUID& seat = agentRoot->getID();
    if (seat == gAgentID)
    {
        LL_DEBUGS("AOEngine") << "Not checking for foreign animation when not sitting." << LL_ENDL;
        return false;
    }

    LL_DEBUGS("AOEngine") << "Checking for foreign animation on seat " << seat << LL_ENDL;

    for (const auto& [source_id, animation_id] : gAgentAvatarp->mAnimationSources)
    {
        // skip animations run by the avatar itself
        if (source_id != gAgentID)
        {
            // find the source object where the animation came from
            LLViewerObject* source = gObjectList.findObject(source_id);

            // proceed if it's not an attachment
            if (source && !source->isAttachment())
            {
                LL_DEBUGS("AOEngine") << "Source " << source_id << " is running animation " << animation_id << LL_ENDL;

                // get the source's root prim
                LLViewerObject* sourceRoot = dynamic_cast<LLViewerObject*>(source->getRoot());

                // if the root prim is the same as the animation source, report back as true
                if (sourceRoot && sourceRoot->getID() == seat)
                {
                    LL_DEBUGS("AOEngine") << "foreign animation " << animation_id << " found on seat." << LL_ENDL;
                    return true;
                }
            }
        }
    }

    return false;
}

// map motion to underwater state, return nullptr if not applicable
AOSet::AOState* AOEngine::mapSwimming(const LLUUID& motion) const
{
    S32 stateNum;

    if (motion == ANIM_AGENT_HOVER)
    {
        stateNum = AOSet::Floating;
    }
    else if (motion == ANIM_AGENT_FLY)
    {
        stateNum = AOSet::SwimmingForward;
    }
    else if (motion == ANIM_AGENT_HOVER_UP)
    {
        stateNum = AOSet::SwimmingUp;
    }
    else if (motion == ANIM_AGENT_HOVER_DOWN)
    {
        stateNum = AOSet::SwimmingDown;
    }
    else
    {
        // motion not applicable for underwater mapping
        return nullptr;
    }

    return mCurrentSet->getState(stateNum);
}

// switch between swimming and flying on transition in and out of Linden region water
void AOEngine::checkBelowWater(bool check_underwater)
{
    // there was no transition, do nothing
    if (mUnderWater == check_underwater)
    {
        return;
    }

    // only applies to motions that actually change underwater and have animations inside
    AOSet::AOState* mapped = mapSwimming(mLastMotion);
    if (!mapped || mapped->mSteps.empty())
    {
        // set underwater status but do nothing else
        mUnderWater = check_underwater;
        return;
    }

    // find animation id to stop when transitioning
    LLUUID id = override(mLastMotion, false);
    if (id.isNull())
    {
        // no animation in overrider for this state, use Linden Lab motion
        id = mLastMotion;
    }

    // stop currently running animation
    gAgent.sendAnimationRequest(id, ANIM_REQUEST_STOP);

    if (!mUnderWater)
    {
        // remember which animation we stopped while going under water, to catch the stop
        // request later in the overrider - this prevents the overrider from triggering itself
        // after the region comes back with the stop request for Linden Lab motion ids
        mTransitionId = id;
    }

    // set requested underwater status for overrider
    mUnderWater = check_underwater;

    // find animation id to start when transitioning
    id = override(mLastMotion, true);
    if (id.isNull())
    {
        // no animation in overrider for this state, use Linden Lab motion
        id = mLastMotion;
    }

    // start new animation
    gAgent.sendAnimationRequest(id, ANIM_REQUEST_START);
}

// find the correct animation state for the requested motion, mapping flying to
// swimming where necessary
AOSet::AOState* AOEngine::getStateForMotion(const LLUUID& motion) const
{
    // get default state for this motion
    AOSet::AOState* defaultState = mCurrentSet->getStateByRemapID(motion);
    if (!mUnderWater)
    {
        return defaultState;
    }

    // get state for underwater motion
    AOSet::AOState* mapped = mapSwimming(motion);
    if (!mapped)
    {
        // not applicable for underwater motion, so use default state
        return defaultState;
    }

    // check if the underwater state has any animations to play
    if (mapped->mSteps.empty())
    {
        // no animations in underwater state, return default
        return defaultState;
    }
    return mapped;
}

void AOEngine::enableStands(bool enable_stands)
{
    mEnabledStands = enable_stands;
    // let the main enable routine decide if we need to change animations
    // but don't actually change the state of the enabled flag
    enable(mEnabled);
}

void AOEngine::enable(bool enable)
{
    LL_DEBUGS("AOEngine") << "using " << gAnimLibrary.animationName(mLastMotion) << " enable " << enable << LL_ENDL;
    mEnabled = enable;

    if (!mCurrentSet)
    {
        LL_DEBUGS("AOEngine") << "enable(" << enable << ") without animation set loaded." << LL_ENDL;
        return;
    }

    AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
    if (mEnabled)
    {
        startAlwaysAnimations();
        if (state && !state->mSteps.empty())
        {
            LL_DEBUGS("AOEngine") << "Enabling animation state " << state->mName << LL_ENDL;

            // do not stop underlying sit animations when re-enabling the AO
            if (mLastOverriddenMotion != ANIM_AGENT_SIT_GROUND &&
                mLastOverriddenMotion != ANIM_AGENT_SIT_GROUND_CONSTRAINED &&
                mLastOverriddenMotion != ANIM_AGENT_SIT)
            {
                gAgent.sendAnimationRequest(mLastOverriddenMotion, ANIM_REQUEST_STOP);
            }

            LLUUID animation = override(mLastMotion, true);
            if (animation.isNull())
            {
                return;
            }

            if (mLastMotion == ANIM_AGENT_STAND)
            {
                if (!mEnabledStands)
                {
                    LL_DEBUGS("AOEngine") << "Last motion was a STAND, but disabled for stands, ignoring." << LL_ENDL;
                    return;
                }
                stopAllStandVariants();
            }
            else if (mLastMotion == ANIM_AGENT_WALK)
            {
                LL_DEBUGS("AOEngine") << "Last motion was a WALK, stopping all variants." << LL_ENDL;
                gAgent.sendAnimationRequest(ANIM_AGENT_WALK_NEW, ANIM_REQUEST_STOP);
                gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK, ANIM_REQUEST_STOP);
                gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_WALK_NEW, ANIM_REQUEST_STOP);
                gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_WALK_NEW);
                gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK);
                gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_WALK_NEW);
            }
            else if (mLastMotion == ANIM_AGENT_RUN)
            {
                LL_DEBUGS("AOEngine") << "Last motion was a RUN, stopping all variants." << LL_ENDL;
                gAgent.sendAnimationRequest(ANIM_AGENT_RUN_NEW, ANIM_REQUEST_STOP);
                gAgent.sendAnimationRequest(ANIM_AGENT_FEMALE_RUN_NEW, ANIM_REQUEST_STOP);
                gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_RUN_NEW);
                gAgentAvatarp->LLCharacter::stopMotion(ANIM_AGENT_FEMALE_RUN_NEW);
            }
            else if (mLastMotion == ANIM_AGENT_SIT)
            {
                stopAllSitVariants();
                gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
            }
            else
            {
                LL_WARNS("AOEngine") << "Unhandled last motion id " << gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
            }

            gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
            if (state->mCurrentAnimation < state->mSteps.size())
            {
                mAnimationChangedSignal(state->mSteps[state->mCurrentAnimation].mInventoryUUID);
            }

            // remember to ignore this motion once in the overrider so stopping the Linden motion
            // will not trigger a stop of the override animation
            mIgnoreMotionStopOnce = mLastMotion;
        }
    }
    else
    {
        if (mLastOverriddenMotion == ANIM_AGENT_SIT)
        {
            // remove sit cycle cover up
            gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
        }

        stopActiveTracks();
        stopAlwaysAnimations();

        // stop all overriders, catch leftovers
        for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
        {
            if (auto state = mCurrentSet->getState(index))
            {
                if (!state->mCurrentAnimationIDs.empty())
                {
                    LL_DEBUGS("AOEngine") << "Stopping leftover animation(s) from state " << state->mName << LL_ENDL;
                    stopCurrentAnimations(state);
                }
            }
            else
            {
                LL_DEBUGS("AOEngine") << "state "<< index <<" returned NULL." << LL_ENDL;
            }
        }

        mAnimationChangedSignal(LLUUID::null);

        // restore Linden animation if applicable
        if (mLastOverriddenMotion != ANIM_AGENT_SIT || !foreignAnimations())
        {
            gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
        }

        mCurrentSet->stopTimer();
    }
}

void AOEngine::setStateCycleTimer(const AOSet::AOState* state)
{
    F32 timeout = state->mCycleTime;
    LL_DEBUGS("AOEngine") << "Setting cycle timeout for state " << state->mName << " of " << timeout << LL_ENDL;
    if (timeout > 0.0f)
    {
        mCurrentSet->startTimer(timeout);
    }
}

LLUUID AOEngine::override(const LLUUID& motion, bool start)
{
    LL_DEBUGS("AOEngine") << "override(" << gAnimLibrary.animationName(motion) << "," << start << ")" << LL_ENDL;

    if (!mEnabled)
    {
        if (start && mCurrentSet)
        {
            const AOSet::AOState* state = mCurrentSet->getStateByRemapID(motion);
            if (state)
            {
                setLastMotion(motion);
                LL_DEBUGS("AOEngine") << "(disabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
                if (!state->mSteps.empty())
                {
                    setLastOverriddenMotion(motion);
                    LL_DEBUGS("AOEngine") << "(disabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
                }
            }
        }
        return LLUUID::null;
    }

    if (mSets.empty())
    {
        LL_DEBUGS("AOEngine") << "No sets loaded. Skipping overrider." << LL_ENDL;
        return LLUUID::null;
    }

    if (!mCurrentSet)
    {
        LL_DEBUGS("AOEngine") << "No current AO set chosen. Skipping overrider." << LL_ENDL;
        return LLUUID::null;
    }

    // ignore stopping this motion once so we can stop the Linden animation
    // without killing our overrider when logging in or re-enabling
    if (!start && motion == mIgnoreMotionStopOnce)
    {
        LL_DEBUGS("AOEngine") << "Not stop-overriding motion " << gAnimLibrary.animationName(motion)
            << " within same state." << LL_ENDL;

        mIgnoreMotionStopOnce = LLUUID::null;

        // when stopping a sit motion make sure to stop the cycle point cover-up animation
        if (motion == ANIM_AGENT_SIT)
        {
            gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
        }

        return LLUUID::null;
    }

    // map the requested motion to an animation state, taking underwater
    // swimming into account where applicable
    AOSet::AOState* state = getStateForMotion(motion);
    if (!state)
    {
        LL_DEBUGS("AOEngine") << "No current AO state for motion " << gAnimLibrary.animationName(motion) << LL_ENDL;
//      This part of the code was added to capture an edge case where animations got stuck
//      However, it seems it isn't needed anymore and breaks other, more important cases.
//      So we disable this code for now, unless bad things happen and the stuck animations
//      come back again. -Zi
//      if (!gAnimLibrary.animStateToString(motion) && !start)
//      {
//          state = mCurrentSet->getStateByRemapID(mLastOverriddenMotion);
//          if (state && state->mCurrentAnimationID == motion)
//          {
//              LL_DEBUGS("AOEngine") << "Stop requested for current overridden animation UUID " << motion << " - Skipping." << LL_ENDL;
//          }
//          else
//          {
//              LL_DEBUGS("AOEngine") << "Stop requested for unknown UUID " << motion << " - Stopping it just in case." << LL_ENDL;
//              gAgent.sendAnimationRequest(motion, ANIM_REQUEST_STOP);
//              gAgentAvatarp->LLCharacter::stopMotion(motion);
//          }
//      }
        return LLUUID::null;
    }

    mAnimationChangedSignal(LLUUID::null);

    // clean up stray animations as an additional safety measure
    // unless current motion is ANIM_AGENT_TYPE, which is a special
    // case, as it plays at the same time as other motions
    if (motion != ANIM_AGENT_TYPE)
    {
        constexpr S32 cleanupStates[] =
        {
            AOSet::Standing,
            AOSet::Walking,
            AOSet::Running,
            AOSet::Sitting,
            AOSet::SittingOnGround,
            AOSet::Crouching,
            AOSet::CrouchWalking,
            AOSet::Falling,
            AOSet::FlyingDown,
            AOSet::FlyingUp,
            AOSet::Flying,
            AOSet::FlyingSlow,
            AOSet::Hovering,
            AOSet::Jumping,
            AOSet::TurningRight,
            AOSet::TurningLeft,
            AOSet::Floating,
            AOSet::SwimmingForward,
            AOSet::SwimmingUp,
            AOSet::SwimmingDown,
            AOSet::AOSTATES_MAX     // end marker, guaranteed to be different from any other entry
        };

        S32 index = 0;
        S32 stateNum;

        // loop through the list of states
        while ((stateNum = cleanupStates[index]) != AOSet::AOSTATES_MAX)
        {
            // check if the next state is the one we are currently animating and skip that
            if (AOSet::AOState* stateToCheck = mCurrentSet->getState(stateNum); stateToCheck != state)
            {
                // check if there are animations left over for that state
                if (!stateToCheck->mCurrentAnimationIDs.empty())
                {
                    LL_WARNS() << "cleaning up animation(s) in state " << stateToCheck->mName << LL_ENDL;

                    // stop the leftover animations locally and in the region for everyone
                    stopCurrentAnimations(stateToCheck);
                }
            }
            index++;
        }
    }

    LLUUID animation;

    mCurrentSet->stopTimer();
    if (start)
    {
        if (motion != ANIM_AGENT_TYPE)
        {
            stopActiveTracks();
        }

        setLastMotion(motion);
        LL_DEBUGS("AOEngine") << "(enabled AO) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;

        // Disable start stands in Mouselook
        if (mCurrentSet->getMouselookStandDisable() &&
            motion == ANIM_AGENT_STAND &&
            mInMouselook)
        {
            LL_DEBUGS("AOEngine") << "(enabled AO, mouselook stand stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
            return LLUUID::null;
        }

        // Don't override start and turning stands if stand override is disabled
        if (!mEnabledStands &&
            (motion == ANIM_AGENT_STAND || motion == ANIM_AGENT_TURNRIGHT || motion == ANIM_AGENT_TURNLEFT))
        {
            LL_DEBUGS("AOEngine") << "(enabled AO, stands disabled) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
            return LLUUID::null;
        }

        // Do not start override sits if not selected
        if (!mCurrentSet->getSitOverride() && motion == ANIM_AGENT_SIT)
        {
            LL_DEBUGS("AOEngine") << "(enabled AO, sit override stopped) setting last motion id to " <<  gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
            return LLUUID::null;
        }

        // scripted seats that use ground_sit as animation need special treatment
        if (motion == ANIM_AGENT_SIT_GROUND || motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED)
        {
            const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
            if (agentRoot && agentRoot->getID() != gAgentID)
            {
                LL_DEBUGS("AOEngine") << "Ground sit animation playing but sitting on a prim - disabling overrider." << LL_ENDL;
                return LLUUID::null;
            }
        }

        if (!state->mSteps.empty())
        {
            setLastOverriddenMotion(motion);
            LL_DEBUGS("AOEngine") << "(enabled AO) setting last overridden motion id to " <<  gAnimLibrary.animationName(mLastOverriddenMotion) << LL_ENDL;
        }

        // do not remember typing as set-wide motion
        if (motion != ANIM_AGENT_TYPE)
        {
            mCurrentSet->setMotion(motion);
        }

        uuid_vec_t animations;
        mCurrentSet->getAnimationsForState(state, animations);

        if (!state->mCurrentAnimationIDs.empty())
        {
            LL_DEBUGS("AOEngine")   << "Previous animation(s) for state "
                        << gAnimLibrary.animationName(motion)
                        << " were not stopped, but we were asked to start a new one. Killing old animations." << LL_ENDL;
            stopCurrentAnimations(state);
        }

        state->mCurrentAnimationIDs = animations;
        animation = animations.empty() ? LLUUID::null : animations.front();
        LL_DEBUGS("AOEngine") << "overriding " <<  gAnimLibrary.animationName(motion)
                    << " with " << animation
                    << " (" << animations.size() << " animation(s))"
                    << " in state " << state->mName
                    << " of set " << mCurrentSet->getName()
                    << " (" << mCurrentSet << ")" << LL_ENDL;

        if (animation.notNull() && state->mCurrentAnimation < state->mSteps.size())
        {
            mAnimationChangedSignal(state->mSteps[state->mCurrentAnimation].mInventoryUUID);
        }

        setStateCycleTimer(state);

        if (motion == ANIM_AGENT_SIT)
        {
            // Use ANIM_AGENT_SIT_GENERIC, so we don't create an overrider loop with ANIM_AGENT_SIT
            // while still having a base sitting pose to cover up cycle points
            gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
            if (mCurrentSet->getSmart())
            {
                mSitCancelTimer.oneShot();
            }
        }
        // special treatment for "transient animations" because the viewer needs the Linden animation to know the agent's state
        else if (motion == ANIM_AGENT_SIT_GROUND ||
                motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
                motion == ANIM_AGENT_PRE_JUMP ||
                motion == ANIM_AGENT_STANDUP ||
                motion == ANIM_AGENT_LAND ||
                motion == ANIM_AGENT_MEDIUM_LAND)
        {
            for (const LLUUID& animationId : animations)
            {
                gAgent.sendAnimationRequest(animationId, ANIM_REQUEST_START);
            }
            return LLUUID::null;
        }

        for (size_t index = 1; index < animations.size(); ++index)
        {
            gAgent.sendAnimationRequest(animations[index], ANIM_REQUEST_START);
        }
        if (motion != ANIM_AGENT_TYPE)
        {
            startStateTracks(state);
        }
    }
    else
    {
        // check for previously remembered transition motion from/to underwater movement and don't
        // allow the overrider to stop it, or it will cancel the transition to underwater motion
        // immediately after starting it
        if (motion == mTransitionId)
        {
            // clear transition motion id and return a null UUID to allow the stock Linden animation
            // system to take over
            mTransitionId.setNull();
            return LLUUID::null;
        }

        // clear transition motion id here as well, to make sure there is no stray id left behind
        mTransitionId.setNull();

        animation = state->mCurrentAnimationIDs.empty() ? LLUUID::null : state->mCurrentAnimationIDs.front();

        // for typing animaiton, just return the stored animation, reset the state timer, and don't memorize anything else
        if (motion == ANIM_AGENT_TYPE)
        {
            stopCurrentAnimations(state, true);
            mAnimationChangedSignal(LLUUID::null);
            AOSet::AOState* previousState = mCurrentSet->getStateByRemapID(mLastMotion);
            if (previousState)
            {
                setStateCycleTimer(previousState);
            }
            return animation;
        }

        // stop the underlying Linden Lab motion, in case it's still running.
        // frequently happens with sits, so we keep it only for those currently.
        if (motion == ANIM_AGENT_SIT)
        {
            stopAllSitVariants();
        }

        if (motion != mCurrentSet->getMotion())
        {
            LL_WARNS("AOEngine") << "trying to stop-override motion " <<  gAnimLibrary.animationName(motion)
                    << " but the current set has motion " <<  gAnimLibrary.animationName(mCurrentSet->getMotion()) << LL_ENDL;
            stopCurrentAnimations(state, true);
            mAnimationChangedSignal(LLUUID::null);
            return animation;
        }

        mCurrentSet->setMotion(LLUUID::null);
        stopActiveTracks();

        // again, special treatment for "transient" animations to make sure our own animation gets stopped properly
        if (motion == ANIM_AGENT_SIT_GROUND ||
            motion == ANIM_AGENT_SIT_GROUND_CONSTRAINED ||
            motion == ANIM_AGENT_PRE_JUMP ||
            motion == ANIM_AGENT_STANDUP ||
            motion == ANIM_AGENT_LAND ||
            motion == ANIM_AGENT_MEDIUM_LAND)
        {
            stopCurrentAnimations(state);
            mAnimationChangedSignal(LLUUID::null);
            setStateCycleTimer(state);
            return LLUUID::null;
        }

        stopCurrentAnimations(state, true);
        mAnimationChangedSignal(LLUUID::null);
    }

    return animation;
}

void AOEngine::checkSitCancel()
{
    if (!foreignAnimations())
        return;

    if (AOSet::AOState* aoState = mCurrentSet->getStateByRemapID(ANIM_AGENT_SIT))
    {
        if (!aoState->mCurrentAnimationIDs.empty())
        {
            LL_DEBUGS("AOEngine") << "Stopping sit animation due to foreign animations running" << LL_ENDL;
            stopCurrentAnimations(aoState);
            // remove cycle point cover-up
            gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);
            mSitCancelTimer.stop();
            // stop cycle tiemr
            mCurrentSet->stopTimer();
            // sitting tracks make no sense when the sit override backs off
            stopActiveTracks();
            mAnimationChangedSignal(LLUUID::null);
        }
    }
}

void AOEngine::cycleTimeout(const AOSet* set)
{
    if (!mEnabled)
    {
        return;
    }

    if (set != mCurrentSet)
    {
        LL_WARNS("AOEngine") << "cycleTimeout for set " << set->getName() << " but current set is " << mCurrentSet->getName() << LL_ENDL;
        return;
    }

    cycle(CycleAny);
}

void AOEngine::cycle(eCycleMode cycleMode, bool resetTimer)
{
    if (!mEnabled)
    {
        return;
    }

    if (!mCurrentSet)
    {
        LL_DEBUGS("AOEngine") << "cycle without set." << LL_ENDL;
        return;
    }

    // do not cycle if we're sitting and sit-override is off
    if (mLastMotion == ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
    {
        return;
    }
    // do not cycle if we're standing and mouselook stand override is disabled while being in mouselook
    else if (mLastMotion == ANIM_AGENT_STAND && mCurrentSet->getMouselookStandDisable() && mInMouselook)
    {
        return;
    }

    AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
    if (!state)
    {
        LL_DEBUGS("AOEngine") << "cycle without state." << LL_ENDL;
        return;
    }

    if (state->mSteps.empty())
    {
        LL_DEBUGS("AOEngine") << "cycle without animations in state." << LL_ENDL;
        return;
    }

    // make sure we disable cycling only for timed cycle, so manual cycling still works, even with cycling switched off
    if (!state->mCycle && cycleMode == CycleAny)
    {
        LL_DEBUGS("AOEngine") << "cycle timeout, but state is set to not cycling." << LL_ENDL;
        return;
    }

    uuid_vec_t oldAnimations = state->mCurrentAnimationIDs;
    uuid_vec_t animations;

    if (cycleMode == CycleAny)
    {
        mCurrentSet->getAnimationsForState(state, animations);
    }
    else
    {
        if (cycleMode == CyclePrevious)
        {
            if (state->mCurrentAnimation == 0)
            {
                state->mCurrentAnimation = static_cast<U32>(state->mSteps.size()) - 1;
            }
            else
            {
                state->mCurrentAnimation--;
            }
        }
        else if (cycleMode == CycleNext)
        {
            state->mCurrentAnimation++;
            if (state->mCurrentAnimation >= state->mSteps.size())
            {
                state->mCurrentAnimation = 0;
            }
        }

        for (AOSet::AOAnimation& anim : state->mSteps[state->mCurrentAnimation].mMembers)
        {
            if (LLUUID assetId = AOSet::resolveAssetUUID(anim); assetId.notNull())
            {
                animations.emplace_back(assetId);
            }
        }
    }

    // don't do anything if the animation didn't change
    if (animations == oldAnimations)
    {
        return;
    }

    state->mCurrentAnimationIDs = animations;
    mAnimationChangedSignal(LLUUID::null);

    if (!animations.empty())
    {
        LL_DEBUGS("AOEngine") << "requesting animation start for motion " << gAnimLibrary.animationName(mLastMotion) << ": " << animations.front() << " (" << animations.size() << " animation(s))" << LL_ENDL;
        for (const LLUUID& animation : animations)
        {
            // don't restart animations that play in both the old and the new step
            if (std::find(oldAnimations.begin(), oldAnimations.end(), animation) == oldAnimations.end())
            {
                gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
            }
        }
    }
    else
    {
        LL_DEBUGS("AOEngine") << "overrider came back with no animations for motion " << gAnimLibrary.animationName(mLastMotion) << "." << LL_ENDL;
    }

    for (const LLUUID& oldAnimation : oldAnimations)
    {
        if (std::find(animations.begin(), animations.end(), oldAnimation) == animations.end())
        {
            LL_DEBUGS("AOEngine") << "Cycling state " << state->mName << " - stopping animation " << oldAnimation << LL_ENDL;
            gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
            gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
        }
    }

    if (resetTimer)
    {
        mCurrentSet->resetTimer();
    }
}

void AOEngine::playAnimation(S32 stepIndex, S32 memberIndex)
{
    if (!mEnabled)
    {
        return;
    }

    if (!mCurrentSet)
    {
        LL_DEBUGS("AOEngine") << "cycle without set." << LL_ENDL;
        return;
    }

    // do not cycle if we're sitting and sit-override is off
    if (mLastMotion == ANIM_AGENT_SIT && !mCurrentSet->getSitOverride())
    {
        return;
    }
    // do not cycle if we're standing and mouselook stand override is disabled while being in mouselook
    else if (mLastMotion == ANIM_AGENT_STAND && mCurrentSet->getMouselookStandDisable() && mInMouselook)
    {
        return;
    }

    AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastMotion);
    if (!state)
    {
        LL_DEBUGS("AOEngine") << "cycle without state." << LL_ENDL;
        return;
    }

    if (stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        LL_WARNS("AOEngine") << "play request for step index " << stepIndex << " out of range." << LL_ENDL;
        return;
    }

    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];

    uuid_vec_t animations;
    if (memberIndex >= 0)
    {
        if (memberIndex >= (S32)step.mMembers.size())
        {
            LL_WARNS("AOEngine") << "play request for member index " << memberIndex << " out of range." << LL_ENDL;
            return;
        }
        if (LLUUID assetId = AOSet::resolveAssetUUID(step.mMembers[memberIndex]); assetId.notNull())
        {
            animations.emplace_back(assetId);
        }
    }
    else
    {
        for (AOSet::AOAnimation& anim : step.mMembers)
        {
            if (LLUUID assetId = AOSet::resolveAssetUUID(anim); assetId.notNull())
            {
                animations.emplace_back(assetId);
            }
        }
    }

    if (animations.empty())
    {
        LL_WARNS("AOEngine") << "no animation assets resolved for step " << step.mName << LL_ENDL;
        return;
    }

    uuid_vec_t oldAnimations = state->mCurrentAnimationIDs;

    // don't do anything if the animation didn't change
    if (animations == oldAnimations)
    {
        return;
    }

    mAnimationChangedSignal(LLUUID::null);

    state->mCurrentAnimation = static_cast<U32>(stepIndex);
    state->mCurrentAnimationIDs = animations;

    for (const LLUUID& animation : animations)
    {
        if (std::find(oldAnimations.begin(), oldAnimations.end(), animation) == oldAnimations.end())
        {
            LL_DEBUGS("AOEngine") << "requesting animation start for motion " << gAnimLibrary.animationName(mLastMotion) << ": " << animation << LL_ENDL;
            gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
        }
    }

    mAnimationChangedSignal(step.mInventoryUUID);

    for (const LLUUID& oldAnimation : oldAnimations)
    {
        if (std::find(animations.begin(), animations.end(), oldAnimation) == animations.end())
        {
            LL_DEBUGS("AOEngine") << "Cycling state " << state->mName << " - stopping animation " << oldAnimation << LL_ENDL;
            gAgent.sendAnimationRequest(oldAnimation, ANIM_REQUEST_STOP);
            gAgentAvatarp->LLCharacter::stopMotion(oldAnimation);
        }
    }

    mCurrentSet->resetTimer();
}

const AOSet* AOEngine::getCurrentSet() const
{
    return mCurrentSet;
}
const AOSet::AOState* AOEngine::getCurrentState() const
{
    return mCurrentSet->getStateByRemapID(mLastMotion);
}

void AOEngine::setItemDescription(const LLUUID& itemID, const std::string& description)
{
    LLViewerInventoryItem* item = gInventory.getItem(itemID);
    if (!item)
    {
        LL_WARNS("AOEngine") << "NULL inventory item found while trying to copy " << itemID << LL_ENDL;
        return;
    }

    LLPointer<LLViewerInventoryItem> newItem = new LLViewerInventoryItem(item);
    newItem->setDescription(description);
    newItem->setComplete(true);
    newItem->updateServer(false);

    gInventory.updateItem(newItem);
}

void AOEngine::updateMemberSortOrder(std::vector<AOSet::AOAnimation>& animations)
{
    for (U32 index = 0; index < animations.size(); ++index)
    {
        if (animations[index].mSortOrder != (S32)index)
        {
            LL_DEBUGS("AOEngine") << "member sort order is " << animations[index].mSortOrder << " but index is " << index << LL_ENDL;
            animations[index].mSortOrder = index;
            setItemDescription(animations[index].mInventoryUUID, llformat("%d", index));
        }
    }
}

void AOEngine::updateSortOrder(AOSet::AOState* state)
{
    for (U32 index = 0; index < state->mSteps.size(); ++index)
    {
        AOSet::AOAnimationStep& step = state->mSteps[index];

        if (step.mSortOrder != (S32)index)
        {
            LL_DEBUGS("AOEngine") << "step sort order is " << step.mSortOrder << " but index is " << index << LL_ENDL;

            step.mSortOrder = index;

            if (step.mIsGroup)
            {
                saveGroup(step);
            }
            else
            {
                setItemDescription(step.mInventoryUUID, llformat("%d", index));
            }
        }
    }
}

void AOEngine::addSet(const std::string& name, inventory_func_type callback, bool reload)
{
    if (mAOFolder.isNull())
    {
        LL_WARNS("AOEngine") << ROOT_AO_FOLDER << " folder not there yet. Requesting recreation." << LL_ENDL;
        tick();
        return;
    }

    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    LL_DEBUGS("AOEngine") << "adding set folder " << name << LL_ENDL;
    gInventory.createNewCategory(mAOFolder, LLFolderType::FT_NONE, name, [callback, wasProtected](const LLUUID &new_cat_id)
    {
        gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

        callback(new_cat_id);
    });

    if (reload)
    {
        mTimerCollection.enableReloadTimer(true);
    }
}

bool AOEngine::cloneSet(AOSet* sourceSet, std::string_view newName)
{
    if (!sourceSet)
    {
        return false;
    }

    if (mImportSet)
    {
        LL_WARNS("AOEngine") << "Set import or clone already running, ignoring clone request." << LL_ENDL;
        return false;
    }

    if (mAOFolder.isNull())
    {
        LL_WARNS("AOEngine") << ROOT_AO_FOLDER << " folder not there yet. Requesting recreation." << LL_ENDL;
        tick();
        return false;
    }

    LL_INFOS("AOEngine") << "cloning set " << sourceSet->getName() << " into " << newName << LL_ENDL;
    mImportSet = new AOSet(sourceSet->getInventoryUUID());
    mImportSet->setName(newName);
    mImportSet->setSitOverride(sourceSet->getSitOverride());
    mImportSet->setSmart(sourceSet->getSmart());
    mImportSet->setMouselookStandDisable(sourceSet->getMouselookStandDisable());

    for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
    {
        const AOSet::AOState* sourceState = sourceSet->getState(index);
        AOSet::AOState* newState = mImportSet->getState(index);
        newState->mCycle = sourceState->mCycle;
        newState->mRandom = sourceState->mRandom;
        newState->mCycleTime = sourceState->mCycleTime;
        newState->mSteps = sourceState->mSteps;
        newState->mTracks = sourceState->mTracks;
    }

    mImportSet->setInventoryUUID(LLUUID::null);
    mTimerCollection.enableImportTimer(true);
    mImportRetryCount = 0;
    processImport(false);
    return true;
}

bool AOEngine::createAnimationLink(const LLUUID& categoryID, const LLInventoryItem* item)
{
    LL_DEBUGS("AOEngine") << "Asset ID " << item->getAssetUUID() << " inventory id " << item->getUUID() << " category id " << categoryID << LL_ENDL;

    if (categoryID.isNull())
    {
        LL_DEBUGS("AOEngine") << "target category UUID not found, failing." << LL_ENDL;
        return false;
    }

    LLInventoryObject::const_object_list_t obj_array{ LLConstPointer<LLInventoryObject>(item) };
    link_inventory_array(categoryID,
                            obj_array,
                            LLPointer<LLInventoryCallback>(nullptr));

    return true;
}

void AOEngine::addAnimation(const AOSet* set, AOSet::AOState* state, const LLInventoryItem* item, bool reload)
{
    AOSet::AOAnimation anim;
    anim.mAssetUUID = item->getAssetUUID();
    anim.mInventoryUUID = item->getUUID();
    anim.mOriginalUUID = item->getLinkedUUID();
    anim.mName = item->getName();
    anim.mSortOrder = static_cast<S32>(state->mSteps.size()) + 1;

    AOSet::AOAnimationStep step;
    step.mName = anim.mName;
    step.mInventoryUUID = anim.mInventoryUUID;
    step.mSortOrder = anim.mSortOrder;
    step.mIsGroup = false;
    step.mMembers.emplace_back(std::move(anim));
    state->mSteps.emplace_back(std::move(step));

    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    bool success = createAnimationLink(state->mInventoryUUID, item);
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

    if (success)
    {
        if (reload)
        {
            mTimerCollection.enableReloadTimer(true);
        }
        return;
    }

    // creating the animation link failed, so we need to create a new folder for this state -
    // add the animation asset to the queue of animations to insert into the state - this takes
    // care of multi animation drag & drop that come in faster than the viewer can create a new
    // inventory folder
    state->mAddQueue.push_back(item);

    // if this is the first queued animation for this state, create the folder asyncronously
    if (state->mAddQueue.size() == 1)
    {
        gInventory.createNewCategory(set->getInventoryUUID(), LLFolderType::FT_NONE, state->mName, [this, state, reload, wasProtected](const LLUUID &new_cat_id)
        {
            state->mInventoryUUID = new_cat_id;
            gSavedPerAccountSettings.setBOOL("LockAOFolders", false);

            // add all queued animations to this state's folder and then clear the queue
            for (const auto item : state->mAddQueue)
            {
                createAnimationLink(state->mInventoryUUID, item);
            }
            state->mAddQueue.clear();

            gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

            if (reload)
            {
                mTimerCollection.enableReloadTimer(true);
            }
        });
    }
}

std::string AOEngine::sanitiseFolderName(const std::string& name, const std::string& fallback) const
{
    std::string clean;
    clean.reserve(name.size());

    for (char character : name)
    {
        if (character == ':' || character == '|')
        {
            continue;
        }
        if (static_cast<unsigned char>(character) < 0x20 || static_cast<unsigned char>(character) > 0x7E)
        {
            continue;
        }
        clean += character;
    }

    LLStringUtil::trim(clean);

    if (clean.empty())
    {
        return fallback;
    }
    return clean;
}

std::string AOEngine::nextGroupName(const AOSet::AOState* state) const
{
    S32 number = 1;
    while (true)
    {
        std::string candidate = llformat("Group %d", number);
        bool taken = false;
        for (const auto& step : state->mSteps)
        {
            if (step.mIsGroup && step.mName == candidate)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            return candidate;
        }
        ++number;
    }
}

std::string AOEngine::nextTrackName(const AOSet::AOState* state) const
{
    S32 number = 1;
    while (true)
    {
        std::string candidate = llformat("Track %d", number);
        bool taken = false;
        for (const auto& track : state->mTracks)
        {
            if (track.mName == candidate)
            {
                taken = true;
                break;
            }
        }
        if (!taken)
        {
            return candidate;
        }
        ++number;
    }
}

// group subfolder name: "<name>:OR<step order>" - categories have no description field, so order travels in the folder name
std::string AOEngine::getGroupFolderName(const AOSet::AOAnimationStep& step) const
{
    return step.mName + llformat(":OR%d", step.mSortOrder);
}

// track subfolder name: "<name>:TR[:CT<time>][:CY][:RN]" - same scheme as state folders, with :TR marking the subfolder as a track
std::string AOEngine::getTrackFolderName(const AOSet::AOTrack& track) const
{
    std::string params = track.mName + ":TR";
    if (track.mCycleTime > 0.0f)
    {
        params += llformat(":CT%.2f", track.mCycleTime);
    }
    if (track.mCycle)
    {
        params += ":CY";
    }
    if (track.mRandom)
    {
        params += ":RN";
    }
    return params;
}

void AOEngine::saveGroup(const AOSet::AOAnimationStep& step)
{
    if (!step.mIsGroup || step.mInventoryUUID.isNull())
    {
        return;
    }
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    rename_category(&gInventory, step.mInventoryUUID, getGroupFolderName(step));
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

void AOEngine::saveTrack(const AOSet::AOTrack& track)
{
    if (track.mInventoryUUID.isNull())
    {
        return;
    }
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    rename_category(&gInventory, track.mInventoryUUID, getTrackFolderName(track));
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

void AOEngine::addAnimationToGroup(AOSet::AOState* state, S32 stepIndex, const LLInventoryItem* item)
{
    if (!state || stepIndex < 0 || stepIndex >= (S32)state->mSteps.size() || !item)
    {
        return;
    }
    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    if (!step.mIsGroup)
    {
        // dropped onto a single step - that is a merge
        createGroupFromMerge(state, stepIndex, item);
        return;
    }
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    createAnimationLink(step.mInventoryUUID, item);
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
    mTimerCollection.enableReloadTimer(true);
}

void AOEngine::createGroupFromMerge(AOSet::AOState* state, S32 stepIndex, const LLInventoryItem* item)
{
    if (!state || stepIndex < 0 || stepIndex >= (S32)state->mSteps.size() || !item)
    {
        return;
    }
    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    if (step.mIsGroup)
    {
        addAnimationToGroup(state, stepIndex, item);
        return;
    }
    const LLUUID targetLinkID = step.mInventoryUUID;

    // queue the drop - more drops onto the same single may arrive while the subfolder is still being created asynchronously
    auto& queue = mPendingGroupMerges[targetLinkID];
    queue.push_back(item);
    if (queue.size() > 1)
    {
        return;
    }

    std::string groupName = sanitiseFolderName(nextGroupName(state), "Group") + llformat(":OR%d", stepIndex);

    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    gInventory.createNewCategory(state->mInventoryUUID, LLFolderType::FT_NONE, groupName, [this, targetLinkID, wasProtected](const LLUUID& new_cat_id)
    {
        gSavedPerAccountSettings.setBOOL("LockAOFolders", false);

        // move the existing animation link into the new subfolder as first member
        if (LLViewerInventoryItem* linkItem = gInventory.getItem(targetLinkID))
        {
            gInventory.changeItemParent(linkItem, new_cat_id, false);
        }

        // link all queued animations behind it
        for (const auto queuedItem : mPendingGroupMerges[targetLinkID])
        {
            createAnimationLink(new_cat_id, queuedItem);
        }
        mPendingGroupMerges.erase(targetLinkID);

        gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
        mTimerCollection.enableReloadTimer(true);
    });
}

bool AOEngine::createGroupFromFolder(AOSet::AOState* state, const LLInventoryCategory* category)
{
    if (!state || !category || state->mInventoryUUID.isNull())
    {
        return false;
    }

    LLInventoryModel::cat_array_t* cats;
    LLInventoryModel::item_array_t* items;
    gInventory.getDirectDescendentsOf(category->getUUID(), cats, items);
    std::vector<const LLInventoryItem*> animations;
    if (items)
    {
        for (const auto& item : *items)
        {
            if (item->getInventoryType() == LLInventoryType::IT_ANIMATION)
            {
                animations.push_back(item);
            }
        }
    }

    if (animations.empty())
    {
        return false;
    }

    std::string groupName = sanitiseFolderName(category->getName(), nextGroupName(state)) + llformat(":OR%d", static_cast<S32>(state->mSteps.size()));

    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    gInventory.createNewCategory(state->mInventoryUUID, LLFolderType::FT_NONE, groupName, [this, animations, wasProtected](const LLUUID& new_cat_id)
    {
        gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
        for (const auto animationItem : animations)
        {
            createAnimationLink(new_cat_id, animationItem);
        }
        gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
        mTimerCollection.enableReloadTimer(true);
    });

    return true;
}

void AOEngine::extractMemberFromGroup(AOSet::AOState* state, S32 stepIndex, S32 memberIndex)
{
    if (!state || stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        return;
    }
    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    if (!step.mIsGroup || memberIndex < 0 || memberIndex >= (S32)step.mMembers.size())
    {
        return;
    }

    LLViewerInventoryItem* linkItem = gInventory.getItem(step.mMembers[memberIndex].mInventoryUUID);
    if (!linkItem)
    {
        LL_WARNS("AOEngine") << "group member link " << step.mMembers[memberIndex].mInventoryUUID << " not found, cannot move it out of group " << step.mName << LL_ENDL;
        return;
    }

    const LLUUID oldLinkID = linkItem->getUUID();
    const LLUUID groupFolderID = (step.mMembers.size() == 1) ? step.mInventoryUUID : LLUUID::null; // extracting the last member retires the whole subfolder, otherwise only the old link
    const std::string sortOrder = llformat("%d", stepIndex); // place the extracted single right behind its former group

    LLInventoryObject::const_object_list_t obj_array{ LLConstPointer<LLInventoryObject>(linkItem) };
    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([this, oldLinkID, groupFolderID, sortOrder](const LLUUID& newItemID)
    {
        // only dispose of the old link when the replacement actually exists
        if (newItemID.isNull())
        {
            return;
        }
        setItemDescription(newItemID, sortOrder);
        if (groupFolderID.notNull())
        {
            purgeFolder(groupFolderID);
        }
        else
        {
            remove_inventory_object(oldLinkID, nullptr);
            gInventory.notifyObservers();
        }
        mTimerCollection.enableReloadTimer(true);
    });
    link_inventory_array(state->mInventoryUUID, obj_array, cb);

    step.mMembers.erase(step.mMembers.begin() + memberIndex);

    if (step.mMembers.size() == 1)
    {
        collapseGroupToSingle(state, step);
    }
    else if (step.mMembers.empty())
    {
        state->mSteps.erase(state->mSteps.begin() + stepIndex);
    }
    else
    {
        updateMemberSortOrder(step.mMembers);
    }
}

void AOEngine::collapseGroupToSingle(AOSet::AOState* state, AOSet::AOAnimationStep& step)
{
    if (!step.mIsGroup || step.mMembers.size() != 1)
    {
        return;
    }

    LLViewerInventoryItem* linkItem = gInventory.getItem(step.mMembers.front().mInventoryUUID);
    if (!linkItem)
    {
        LL_WARNS("AOEngine") << "last group member link " << step.mMembers.front().mInventoryUUID << " not found, cannot dissolve group " << step.mName << LL_ENDL;
        return;
    }

    const LLUUID groupFolderID = step.mInventoryUUID;
    const std::string sortOrder = llformat("%d", step.mSortOrder);

    LLInventoryObject::const_object_list_t obj_array{ LLConstPointer<LLInventoryObject>(linkItem) };
    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([this, groupFolderID, sortOrder](const LLUUID& newItemID)
    {
        // only retire the subfolder when the replacement link actually exists
        if (newItemID.isNull())
        {
            return;
        }
        setItemDescription(newItemID, sortOrder);
        purgeFolder(groupFolderID);
        mTimerCollection.enableReloadTimer(true);
    });
    link_inventory_array(state->mInventoryUUID, obj_array, cb);
}

bool AOEngine::renameGroup(AOSet::AOState* state, S32 stepIndex, std::string_view newName)
{
    if (!state || stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        return false;
    }
    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    if (!step.mIsGroup)
    {
        return false;
    }
    std::string cleanName = sanitiseFolderName(std::string(newName), "");
    if (cleanName.empty())
    {
        return false;
    }
    step.mName = cleanName;
    saveGroup(step);
    mUpdatedSignal();
    return true;
}

std::string AOEngine::createEmptyTrack(AOSet::AOState* state)
{
    if (!state || state->mInventoryUUID.isNull())
    {
        return std::string();
    }
    std::string trackName = sanitiseFolderName(nextTrackName(state), "Track");
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    gInventory.createNewCategory(state->mInventoryUUID, LLFolderType::FT_NONE, trackName + ":TR", [this, wasProtected](const LLUUID& new_cat_id)
    {
        gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
        mTimerCollection.enableReloadTimer(true);
    });
    return trackName;
}

void AOEngine::addAnimationToTrack(AOSet::AOState* state, S32 trackIndex, const LLInventoryItem* item)
{
    if (!state || trackIndex < 0 || trackIndex >= (S32)state->mTracks.size() || !item)
    {
        return;
    }
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    createAnimationLink(state->mTracks[trackIndex].mInventoryUUID, item);
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
    mTimerCollection.enableReloadTimer(true);
}

void AOEngine::removeTrack(AOSet::AOState* state, S32 trackIndex)
{
    if (!state || trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return;
    }

    if (mActiveTrackState == state)
    {
        stopActiveTracks();
    }
    if (mActiveAlwaysState == state)
    {
        stopAlwaysAnimations();
    }

    purgeFolder(state->mTracks[trackIndex].mInventoryUUID);
    state->mTracks.erase(state->mTracks.begin() + trackIndex);
    mTimerCollection.enableReloadTimer(true);
}

void AOEngine::removeTrackAnimation(AOSet::AOState* state, S32 trackIndex, S32 memberIndex)
{
    if (!state || trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return;
    }
    AOSet::AOTrack& track = state->mTracks[trackIndex];
    if (memberIndex < 0 || memberIndex >= (S32)track.mAnimations.size())
    {
        return;
    }

    if (mActiveTrackState == state)
    {
        stopActiveTracks();
    }
    if (mActiveAlwaysState == state)
    {
        stopAlwaysAnimations();
    }

    LL_DEBUGS("AOEngine") << "purging track member: " << track.mAnimations[memberIndex].mInventoryUUID << LL_ENDL;
    remove_inventory_object(track.mAnimations[memberIndex].mInventoryUUID, nullptr);
    gInventory.notifyObservers();
    track.mAnimations.erase(track.mAnimations.begin() + memberIndex);
    if (track.mAnimations.empty())
    {
        purgeFolder(track.mInventoryUUID);
        state->mTracks.erase(state->mTracks.begin() + trackIndex);
    }
    else
    {
        updateMemberSortOrder(track.mAnimations);
    }

    mTimerCollection.enableReloadTimer(true);
}

bool AOEngine::swapTrackAnimationWithPrevious(AOSet::AOState* state, S32 trackIndex, S32 memberIndex)
{
    if (!state || trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return false;
    }
    AOSet::AOTrack& track = state->mTracks[trackIndex];
    if (memberIndex < 1 || memberIndex >= (S32)track.mAnimations.size())
    {
        return false;
    }
    std::swap(track.mAnimations[memberIndex], track.mAnimations[memberIndex - 1]);
    updateMemberSortOrder(track.mAnimations);
    return true;
}

bool AOEngine::swapTrackAnimationWithNext(AOSet::AOState* state, S32 trackIndex, S32 memberIndex)
{
    if (!state || trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return false;
    }
    AOSet::AOTrack& track = state->mTracks[trackIndex];
    if (memberIndex < 0 || memberIndex >= (S32)track.mAnimations.size() - 1)
    {
        return false;
    }
    std::swap(track.mAnimations[memberIndex], track.mAnimations[memberIndex + 1]);
    updateMemberSortOrder(track.mAnimations);
    return true;
}

void AOEngine::setTrackCycle(AOSet::AOState* state, S32 trackIndex, bool cycle)
{
    if (state && trackIndex >= 0 && trackIndex < (S32)state->mTracks.size())
    {
        state->mTracks[trackIndex].mCycle = cycle;
        saveTrack(state->mTracks[trackIndex]);
        if (state == mActiveAlwaysState)
        {
            refreshAlwaysTimers();
        }
    }
}

void AOEngine::setTrackRandomize(AOSet::AOState* state, S32 trackIndex, bool randomize)
{
    if (state && trackIndex >= 0 && trackIndex < (S32)state->mTracks.size())
    {
        state->mTracks[trackIndex].mRandom = randomize;
        saveTrack(state->mTracks[trackIndex]);
    }
}

void AOEngine::setTrackCycleTime(AOSet::AOState* state, S32 trackIndex, F32 time)
{
    if (state && trackIndex >= 0 && trackIndex < (S32)state->mTracks.size())
    {
        state->mTracks[trackIndex].mCycleTime = time;
        saveTrack(state->mTracks[trackIndex]);
        if (state == mActiveAlwaysState)
        {
            refreshAlwaysTimers();
        }
    }
}

void AOEngine::playTrackAnimation(AOSet::AOState* state, S32 trackIndex, S32 memberIndex)
{
    if (!mEnabled || !state || (mActiveTrackState != state && mActiveAlwaysState != state))
    {
        return;
    }

    if (trackIndex < 0 || trackIndex >= (S32)state->mTracks.size())
    {
        return;
    }

    AOSet::AOTrack& track = state->mTracks[trackIndex];
    if (memberIndex < 0)
    {
        memberIndex = track.mCurrentAnimation;
    }
    if (memberIndex >= (S32)track.mAnimations.size())
    {
        return;
    }

    LLUUID animation = AOSet::resolveAssetUUID(track.mAnimations[memberIndex]);
    if (animation.isNull() || animation == track.mCurrentAnimationID)
    {
        return;
    }

    if (track.mCurrentAnimationID.notNull())
    {
        gAgent.sendAnimationRequest(track.mCurrentAnimationID, ANIM_REQUEST_STOP);
        if (isAgentAvatarValid())
        {
            gAgentAvatarp->LLCharacter::stopMotion(track.mCurrentAnimationID);
        }
    }

    track.mCurrentAnimation = memberIndex;
    track.mCurrentAnimationID = animation;
    gAgent.sendAnimationRequest(animation, ANIM_REQUEST_START);
    mAnimationChangedSignal(LLUUID::null);
}

void AOEngine::playAlwaysAnimation(S32 stepIndex, S32 memberIndex)
{
    AOSet::AOState* state = mActiveAlwaysState;
    if (!mEnabled || !state)
    {
        return;
    }

    if (stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        LL_WARNS("AOEngine") << "always state play request for step index " << stepIndex << " out of range." << LL_ENDL;
        return;
    }

    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    uuid_vec_t animations;
    if (memberIndex >= 0)
    {
        if (memberIndex >= (S32)step.mMembers.size())
        {
            LL_WARNS("AOEngine") << "always state play request for member index " << memberIndex << " out of range." << LL_ENDL;
            return;
        }
        if (LLUUID assetId = AOSet::resolveAssetUUID(step.mMembers[memberIndex]); assetId.notNull())
        {
            animations.emplace_back(assetId);
        }
    }
    else
    {
        for (AOSet::AOAnimation& anim : step.mMembers)
        {
            if (LLUUID assetId = AOSet::resolveAssetUUID(anim); assetId.notNull())
            {
                animations.emplace_back(assetId);
            }
        }
    }

    if (animations.empty())
    {
        LL_WARNS("AOEngine") << "no animation assets resolved for always state step " << step.mName << LL_ENDL;
        return;
    }

    if (animations == state->mCurrentAnimationIDs)
    {
        return;
    }

    state->mCurrentAnimation = static_cast<U32>(stepIndex);
    swapStateAnimations(state, animations);
    refreshAlwaysTimers();
    mAnimationChangedSignal(LLUUID::null);
}

bool AOEngine::findForeignItems(const LLUUID& uuid) const
{
    bool moved = false;

    LLInventoryModel::item_array_t* items;
    LLInventoryModel::cat_array_t* cats;

    gInventory.getDirectDescendentsOf(uuid, cats, items);

    if (cats)
    {
        for (const auto& cat : *cats)
        {
            if (findForeignItems(cat->getUUID()))
            {
                moved = true;
            }
        }
    }

    // count backwards in case we have to remove items
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);

    if (items)
    {
        for (S32 index = static_cast<S32>(items->size()) - 1; index >= 0; --index)
        {
            bool move{ false };

            const LLPointer<LLViewerInventoryItem>& item = items->at(index);
            if (item->getIsLinkType())
            {
                if (item->getInventoryType() != LLInventoryType::IT_ANIMATION)
                {
                    LL_DEBUGS("AOEngine") << item->getName() << " is a link but does not point to an animation." << LL_ENDL;
                    move = true;
                }
                else
                {
                    LL_DEBUGS("AOEngine") << item->getName() << " is an animation link." << LL_ENDL;
                }
            }
            else
            {
                LL_DEBUGS("AOEngine") << item->getName() << " is not a link!" << LL_ENDL;
                move = true;
            }

            if (move)
            {
                moved = true;
                gInventory.changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), false);
                LL_DEBUGS("AOEngine") << item->getName() << " moved to lost and found!" << LL_ENDL;
            }
        }
    }

    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

    return moved;
}

// needs a three-step process, since purge of categories only seems to work from trash
void AOEngine::purgeFolder(const LLUUID& uuid) const
{
    // unprotect it
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);

    // move everything that's not an animation link to "lost and found"
    if (findForeignItems(uuid))
    {
        LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
    }

    // trash it
    gInventory.removeCategory(uuid);

    // clean it
    purge_descendents_of(uuid, nullptr);
    gInventory.notifyObservers();

    // purge it
    remove_inventory_object(uuid, nullptr);
    gInventory.notifyObservers();

    // protect it
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

bool AOEngine::removeSet(AOSet* set)
{
    purgeFolder(set->getInventoryUUID());

    mTimerCollection.enableReloadTimer(true);
    return true;
}

bool AOEngine::removeAnimation(const AOSet* set, AOSet::AOState* state, S32 stepIndex, S32 memberIndex)
{
    if (stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        return false;
    }

    AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
    if (memberIndex >= 0 && step.mIsGroup)
    {
        if (memberIndex >= (S32)step.mMembers.size())
        {
            return false;
        }

        LL_DEBUGS("AOEngine") << "purging group member: " << step.mMembers[memberIndex].mInventoryUUID << LL_ENDL;
        remove_inventory_object(step.mMembers[memberIndex].mInventoryUUID, nullptr);
        gInventory.notifyObservers();

        step.mMembers.erase(step.mMembers.begin() + memberIndex);

        if (step.mMembers.size() == 1)
        {
            collapseGroupToSingle(state, step);
        }
        else if (step.mMembers.empty())
        {
            purgeFolder(step.mInventoryUUID);
            state->mSteps.erase(state->mSteps.begin() + stepIndex);
        }
        else
        {
            updateMemberSortOrder(step.mMembers);
        }

        return true;
    }

    if (step.mIsGroup)
    {
        LL_DEBUGS("AOEngine") << "purging group folder: " << step.mInventoryUUID << LL_ENDL;
        purgeFolder(step.mInventoryUUID);
        state->mSteps.erase(state->mSteps.begin() + stepIndex);
    }
    else
    {
        LLViewerInventoryItem* item = gInventory.getItem(step.mInventoryUUID);
        if (item)
        {
            // check if this item is actually an animation link
            bool move = true;
            if (item->getIsLinkType())
            {
                if (item->getInventoryType() == LLInventoryType::IT_ANIMATION)
                {
                    // it is an animation link, so mark it to be purged
                    move = false;
                }
            }

            // this item was not an animation link, move it to lost and found
            if (move)
            {
                gInventory.changeItemParent(item, gInventory.findCategoryUUIDForType(LLFolderType::FT_LOST_AND_FOUND), false);
                LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
                update();
                return false;
            }
        }

        // purge the item from inventory
        LL_DEBUGS("AOEngine") << __LINE__ << " purging: " << step.mInventoryUUID << LL_ENDL;
        remove_inventory_object(step.mInventoryUUID, nullptr);
        gInventory.notifyObservers();

        state->mSteps.erase(state->mSteps.begin() + stepIndex);
    }

    // count steps, not raw inventory items - group subfolders alone must not keep a state folder alive once its last step is gone
    if (state->mSteps.empty() && state->mTracks.empty())
    {
        LL_DEBUGS("AOEngine") << "purging folder " << state->mName << " from inventory because it's empty." << LL_ENDL;

        LLInventoryModel::item_array_t* items;
        LLInventoryModel::cat_array_t* cats;
        gInventory.getDirectDescendentsOf(set->getInventoryUUID(), cats, items);

        if (cats)
        {
            for (const auto& cat : *cats)
            {
                std::vector<std::string> params;
                LLStringUtil::getTokens(cat->getName(), params, ":");

                if (params.empty())
                {
                    LL_WARNS("AOEngine") << "Unexpected folder found in ao set folder: " << cat->getName() << LL_ENDL;
                    return false;
                }

                const std::string& stateName = params[0];

                if (state->mName.compare(stateName) == 0)
                {
                    LL_DEBUGS("AOEngine") << "folder found: " << cat->getName() << " purging uuid " << cat->getUUID() << LL_ENDL;

                    purgeFolder(cat->getUUID());
                    state->mInventoryUUID.setNull();
                    break;
                }
            }
        }
    }
    else
    {
        updateSortOrder(state);
    }

    return true;
}

// reorder steps (memberIndex -1) or members within a group; member moves are clamped to their group, they never leave it through the edges
bool AOEngine::swapWithPrevious(AOSet::AOState* state, S32 stepIndex, S32 memberIndex)
{
    if (stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        return false;
    }

    if (memberIndex >= 0)
    {
        AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
        if (!step.mIsGroup || memberIndex < 1 || memberIndex >= (S32)step.mMembers.size())
        {
            return false;
        }
        std::swap(step.mMembers[memberIndex], step.mMembers[memberIndex - 1]);
        updateMemberSortOrder(step.mMembers);
        return true;
    }

    if (state->mSteps.size() < 2 || stepIndex == 0)
    {
        return false;
    }

    std::swap(state->mSteps[stepIndex], state->mSteps[stepIndex - 1]);
    updateSortOrder(state);

    return true;
}

bool AOEngine::swapWithNext(AOSet::AOState* state, S32 stepIndex, S32 memberIndex)
{
    if (stepIndex < 0 || stepIndex >= (S32)state->mSteps.size())
    {
        return false;
    }

    if (memberIndex >= 0)
    {
        AOSet::AOAnimationStep& step = state->mSteps[stepIndex];
        if (!step.mIsGroup || memberIndex < 0 || memberIndex >= (S32)step.mMembers.size() - 1)
        {
            return false;
        }
        std::swap(step.mMembers[memberIndex], step.mMembers[memberIndex + 1]);
        updateMemberSortOrder(step.mMembers);
        return true;
    }

    if (state->mSteps.size() < 2 || stepIndex == (S32)state->mSteps.size() - 1)
    {
        return false;
    }

    std::swap(state->mSteps[stepIndex], state->mSteps[stepIndex + 1]);
    updateSortOrder(state);

    return true;
}

bool AOEngine::readAnimationLinks(const LLUUID& categoryID, std::vector<AOSet::AOAnimation>& animations)
{
    if (!gInventory.isCategoryComplete(categoryID))
    {
        LL_DEBUGS("AOEngine") << "category " << categoryID << " is incomplete, fetching descendents" << LL_ENDL;
        gInventory.fetchDescendentsOf(categoryID);
        return false;
    }
    LLInventoryModel::item_array_t* items;
    LLInventoryModel::cat_array_t* dummy;

    gInventory.getDirectDescendentsOf(categoryID, dummy, items);

    if (items)
    {
        for (const auto& item : *items)
        {
            LL_DEBUGS("AOEngine") << "Found animation link " << item->LLInventoryItem::getName()
                << " desc " << item->LLInventoryItem::getDescription()
                << " asset " << item->getAssetUUID() << LL_ENDL;

            AOSet::AOAnimation anim;
            anim.mName = item->getName();
            anim.mInventoryUUID = item->getUUID();
            anim.mOriginalUUID = item->getLinkedUUID();
            anim.mAssetUUID.setNull();

            // if we can find the original animation already right here, save its asset ID, otherwise this will
            // be tried again in AOSet::getAnimationsForState() and/or AOEngine::cycle()
            if (item->getLinkedItem())
            {
                anim.mAssetUUID = item->getAssetUUID();
            }

            S32 sortOrder;
            if (!LLStringUtil::convertToS32(item->LLInventoryItem::getDescription(), sortOrder))
            {
                sortOrder = -1;
            }
            anim.mSortOrder = sortOrder;

            LL_DEBUGS("AOEngine") << "current sort order is " << sortOrder << LL_ENDL;

            if (sortOrder == -1)
            {
                LL_WARNS("AOEngine") << "sort order was unknown so append to the end of the list" << LL_ENDL;
                animations.emplace_back(std::move(anim));
            }
            else
            {
                bool inserted = false;
                for (size_t index = 0; index < animations.size(); ++index)
                {
                    if (animations[index].mSortOrder > sortOrder)
                    {
                        LL_DEBUGS("AOEngine") << "inserting at index " << index << LL_ENDL;
                        animations.insert(animations.begin() + index, anim);
                        inserted = true;
                        break;
                    }
                }
                if (!inserted)
                {
                    LL_DEBUGS("AOEngine") << "not inserted yet, appending to the list instead" << LL_ENDL;
                    animations.emplace_back(std::move(anim));
                }
            }
        }
    }

    return true;
}

bool AOEngine::reloadStateAnimations(AOSet* set, AOSet::AOState* state)
{
    LLInventoryModel::item_array_t* dummyItems;
    LLInventoryModel::cat_array_t* cats;
    bool allComplete = true;
    state->mSteps.clear();
    state->mTracks.clear();

    auto insertStep = [state](AOSet::AOAnimationStep&& step)
    {
        if (step.mSortOrder == -1)
        {
            LL_WARNS("AOEngine") << "step sort order was unknown so append to the end of the list" << LL_ENDL;
            state->mSteps.emplace_back(std::move(step));
            return;
        }

        for (size_t index = 0; index < state->mSteps.size(); ++index)
        {
            if (state->mSteps[index].mSortOrder > step.mSortOrder)
            {
                state->mSteps.insert(state->mSteps.begin() + index, std::move(step));
                return;
            }
        }
        state->mSteps.emplace_back(std::move(step));
    };

    std::vector<AOSet::AOAnimation> directLinks;
    if (!readAnimationLinks(state->mInventoryUUID, directLinks))
    {
        return false;
    }

    for (auto& anim : directLinks)
    {
        AOSet::AOAnimationStep step;
        step.mName = anim.mName;
        step.mInventoryUUID = anim.mInventoryUUID;
        step.mSortOrder = anim.mSortOrder;
        step.mIsGroup = false;
        step.mMembers.emplace_back(std::move(anim));
        insertStep(std::move(step));
    }

    gInventory.getDirectDescendentsOf(state->mInventoryUUID, cats, dummyItems);

    if (cats)
    {
        for (const auto& cat : *cats)
        {
            std::vector<std::string> params;
            LLStringUtil::getTokens(cat->getName(), params, ":");
            if (params.empty())
            {
                LL_WARNS("AOEngine") << "Subfolder with empty name in state folder: " << cat->getName() << LL_ENDL;
                continue;
            }

            bool isTrack = false;
            for (size_t num = 1; num < params.size(); ++num)
            {
                if (params[num] == "TR")
                {
                    isTrack = true;
                    break;
                }
            }

            if (isTrack)
            {
                AOSet::AOTrack track;
                track.mName = params[0];
                track.mInventoryUUID = cat->getUUID();

                for (size_t num = 1; num < params.size(); ++num)
                {
                    if (params[num] == "TR")
                    {
                        continue;
                    }
                    else if (params[num] == "CY")
                    {
                        track.mCycle = true;
                    }
                    else if (params[num] == "RN")
                    {
                        track.mRandom = true;
                    }
                    else if (params[num].substr(0, 2) == "CT")
                    {
                        LLStringUtil::convertToF32(params[num].substr(2), track.mCycleTime);
                    }
                    else
                    {
                        LL_WARNS("AOEngine") << "Unknown AO track option " << params[num] << LL_ENDL;
                    }
                }

                if (!readAnimationLinks(track.mInventoryUUID, track.mAnimations))
                {
                    allComplete = false;
                    continue;
                }

                updateMemberSortOrder(track.mAnimations);
                state->mTracks.emplace_back(std::move(track));
            }
            else
            {
                AOSet::AOAnimationStep step;
                step.mName = params[0];
                step.mInventoryUUID = cat->getUUID();
                step.mIsGroup = true;
                step.mSortOrder = -1;

                for (size_t num = 1; num < params.size(); ++num)
                {
                    if (params[num].substr(0, 2) == "OR")
                    {
                        S32 sortOrder;
                        if (LLStringUtil::convertToS32(params[num].substr(2), sortOrder))
                        {
                            step.mSortOrder = sortOrder;
                        }
                    }
                    else
                    {
                        LL_WARNS("AOEngine") << "Unknown AO group option " << params[num] << LL_ENDL;
                    }
                }

                if (!readAnimationLinks(step.mInventoryUUID, step.mMembers))
                {
                    allComplete = false;
                    continue;
                }

                if (step.mMembers.empty())
                {
                    LL_DEBUGS("AOEngine") << "ignoring empty group subfolder " << cat->getName() << LL_ENDL;
                    continue;
                }

                updateMemberSortOrder(step.mMembers);
                insertStep(std::move(step));
            }
        }
    }

    updateSortOrder(state);

    if (auto currentState = gSavedPerAccountSettings.getLLSD("FSCurrentAOState");
        currentState.has("CurrentSet") && currentState["CurrentSet"].asString() == set->getName() &&
        currentState.has("CurrentStateAnimations") && currentState["CurrentStateAnimations"].has(state->mName))
    {
        auto        currentStateAnimId{ currentState["CurrentStateAnimations"][state->mName].asUUID() };
        LLXORCipher cipher(ENCRYPTION_MAGIC_ID.mData, UUID_BYTES);
        cipher.decrypt(currentStateAnimId.mData, UUID_BYTES);

        for (size_t stepIndex = 0; stepIndex < state->mSteps.size(); ++stepIndex)
        {
            bool found = false;
            for (const auto& member : state->mSteps[stepIndex].mMembers)
            {
                if (member.mAssetUUID == currentStateAnimId)
                {
                    state->mCurrentAnimation = static_cast<U32>(stepIndex);
                    found = true;
                    break;
                }
            }
            if (found)
            {
                break;
            }
        }
    }

    return allComplete;
}

void AOEngine::update()
{
    if (mAOFolder.isNull())
    {
        return;
    }

    if (!gInventory.isCategoryComplete(mAOFolder))
    {
        LL_DEBUGS("AOEngine") << "#AO folder hasn't fully fetched yet, try again next timer tick." << LL_ENDL;
        gInventory.fetchDescendentsOf(mAOFolder);
        return;
    }

    // move everything that's not an animation link to "lost and found"
    if (findForeignItems(mAOFolder))
    {
        LLNotificationsUtil::add("AOForeignItemsFound", LLSD());
    }

    LLInventoryModel::cat_array_t* categories;
    LLInventoryModel::item_array_t* items;

    bool allComplete = true;
    mTimerCollection.enableSettingsTimer(false);

    gInventory.getDirectDescendentsOf(mAOFolder, categories, items);

    if (categories)
    {
        std::vector<std::pair<LLUUID, AOSet*>> setsToWrite;
        for (const auto& currentCategory : *categories)
        {
            const std::string& setFolderName = currentCategory->getName();

            if (setFolderName.empty())
            {
                LL_WARNS("AOEngine") << "Folder with emtpy name in AO folder" << LL_ENDL;
                continue;
            }

            std::vector<std::string> params;
            LLStringUtil::getTokens(setFolderName, params, ":");
            if (params.empty())
            {
                LL_WARNS("AOEngine") << "Unexpected folder found in ao set folder: " << currentCategory->getName() << LL_ENDL;
                continue;
            }

            auto setName{ params[0] };
            AOSet* newSet = getSetByName(setName);
            if (!newSet)
            {
                LL_DEBUGS("AOEngine") << "Adding set " << setFolderName << " to AO." << LL_ENDL;
                newSet = new AOSet(currentCategory->getUUID());
                newSet->setName(setName);
                mSets.emplace_back(newSet);
                setsToWrite.emplace_back(currentCategory->getUUID(), newSet);
            }
            else
            {
                if (newSet->getComplete())
                {
                    LL_DEBUGS("AOEngine") << "Set " << setName << " already complete. Skipping." << LL_ENDL;
                    continue;
                }
                LL_DEBUGS("AOEngine") << "Updating set " << setFolderName << " in AO." << LL_ENDL;
            }
            allComplete = false;

            for (auto num = 1; num < params.size(); ++num)
            {
                if (params[num].size() != 2)
                {
                    LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
                }
                else if (params[num] == "SO")
                {
                    newSet->setSitOverride(true);
                }
                else if (params[num] == "SM")
                {
                    newSet->setSmart(true);
                }
                else if (params[num] == "DM")
                {
                    newSet->setMouselookStandDisable(true);
                }
                else if (params[num] == "**")
                {
                    mDefaultSet = newSet;
                    mCurrentSet = newSet;
                }
                else
                {
                    LL_WARNS("AOEngine") << "Unknown AO set option " << params[num] << LL_ENDL;
                }
            }
        }

        for (const auto& [categoryUUID, newSet] : setsToWrite)
        {
            if (!mDefaultSet)
            {
                // No default is set, so instead use the set saved in FSCurrentAOState
                if (auto currentState = gSavedPerAccountSettings.getLLSD("FSCurrentAOState");
                    currentState.has("CurrentSet") && currentState["CurrentSet"].asString() == newSet->getName())
                {
                    LL_DEBUGS("AOEngine") << "Selecting current set from settings: " << newSet->getName() << LL_ENDL;
                    mCurrentSet = newSet;
                }
            }

            if (gInventory.isCategoryComplete(categoryUUID))
            {
                LL_DEBUGS("AOEngine") << "Set " << newSet->getName() << " is complete, reading states ..." << LL_ENDL;

                LLInventoryModel::cat_array_t* stateCategories;
                gInventory.getDirectDescendentsOf(categoryUUID, stateCategories, items);
                newSet->setComplete(true);

                for (const auto& stateCategory : *stateCategories)
                {
                    std::vector<std::string> state_params;
                    LLStringUtil::getTokens(stateCategory->getName(), state_params, ":");
                    if (state_params.empty())
                    {
                        LL_WARNS("AOEngine") << "Unexpected state folder found in ao set: " << stateCategory->getName() << LL_ENDL;
                        continue;
                    }
                    const std::string& stateName = state_params[0];

                    AOSet::AOState* state = newSet->getStateByName(stateName);
                    if (!state)
                    {
                        LL_WARNS("AOEngine") << "Unknown state " << stateName << ". Skipping." << LL_ENDL;
                        continue;
                    }
                    LL_DEBUGS("AOEngine") << "Reading state " << stateName << LL_ENDL;

                    state->mInventoryUUID = stateCategory->getUUID();
                    for (auto num = 1; num < state_params.size(); ++num)
                    {
                        if (state_params[num] == "CY")
                        {
                            state->mCycle = true;
                            LL_DEBUGS("AOEngine") << "Cycle on" << LL_ENDL;
                        }
                        else if (state_params[num] == "RN")
                        {
                            state->mRandom = true;
                            LL_DEBUGS("AOEngine") << "Random on" << LL_ENDL;
                        }
                        else if (state_params[num].substr(0, 2) == "CT")
                        {
                            LLStringUtil::convertToF32(state_params[num].substr(2, state_params[num].size() - 2), state->mCycleTime);
                            LL_DEBUGS("AOEngine") << "Cycle Time specified:" << state->mCycleTime << LL_ENDL;
                        }
                        else
                        {
                            LL_WARNS("AOEngine") << "Unknown AO set option " << state_params[num] << LL_ENDL;
                        }
                    }

                    if (!gInventory.isCategoryComplete(state->mInventoryUUID))
                    {
                        LL_DEBUGS("AOEngine") << "State category " << stateName << " is incomplete, fetching descendents" << LL_ENDL;
                        gInventory.fetchDescendentsOf(state->mInventoryUUID);
                        allComplete = false;
                        newSet->setComplete(false);
                        continue;
                    }
                    if (!reloadStateAnimations(newSet, state))
                    {
                        LL_DEBUGS("AOEngine") << "State " << stateName << " has incomplete subfolders, fetching" << LL_ENDL;
                        allComplete = false;
                        newSet->setComplete(false);
                    }
                }
            }
            else
            {
                LL_DEBUGS("AOEngine") << "Set " << newSet->getName() << " is incomplete, fetching descendents" << LL_ENDL;
                gInventory.fetchDescendentsOf(categoryUUID);
            }
        }
    }

    if (allComplete)
    {
        mEnabled = gSavedPerAccountSettings.getBOOL("UseAO");

        if (!mCurrentSet && !mSets.empty())
        {
            LL_DEBUGS("AOEngine") << "No set currently selected, choosing the first in the list." << LL_ENDL;
            selectSet(mSets[0]);
        }

        mTimerCollection.enableInventoryTimer(false);
        mTimerCollection.enableSettingsTimer(true);

        LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
        mUpdatedSignal();
        enable(mEnabled);
    }
}

void AOEngine::reload(bool fromTimer)
{
    bool wasEnabled = mEnabled;

    mTimerCollection.enableReloadTimer(false);

    if (wasEnabled)
    {
        enable(false);
    }

    gAgent.stopCurrentAnimations();
    mLastOverriddenMotion = ANIM_AGENT_STAND;

    clear(fromTimer);
    mAOFolder.setNull();
    mTimerCollection.enableInventoryTimer(true);
    tick();

    if (wasEnabled)
    {
        enable(true);
    }
}

AOSet* AOEngine::getSetByName(std::string_view name) const
{
    for (auto set : mSets)
    {
        if (set->getName() == name)
        {
            return set;
        }
    }

    return nullptr;
}

const std::string AOEngine::getCurrentSetName() const
{
    if (mCurrentSet)
    {
        return mCurrentSet->getName();
    }
    return std::string();
}

const AOSet* AOEngine::getDefaultSet() const
{
    return mDefaultSet;
}

void AOEngine::selectSet(AOSet* set)
{
    if (mEnabled && mCurrentSet)
    {
        stopActiveTracks();
        stopAlwaysAnimations();
        if (AOSet::AOState* state = mCurrentSet->getStateByRemapID(mLastOverriddenMotion))
        {
            stopCurrentAnimations(state);
            mCurrentSet->stopTimer();
        }
    }

    mCurrentSet = set;

    if (mEnabled)
    {
        LL_DEBUGS("AOEngine") << "enabling with motion " << gAnimLibrary.animationName(mLastMotion) << LL_ENDL;
        startAlwaysAnimations();
        gAgent.sendAnimationRequest(override(mLastMotion, true), ANIM_REQUEST_START);
    }
}

AOSet* AOEngine::selectSetByName(std::string_view name)
{
    if (AOSet* set = getSetByName(name))
    {
        selectSet(set);
        return set;
    }
    LL_WARNS("AOEngine") << "Could not find AO set " << name << LL_ENDL;
    return nullptr;
}

const std::vector<AOSet*> AOEngine::getSetList() const
{
    return mSets;
}

void AOEngine::saveSet(const AOSet* set)
{
    if (!set)
    {
        return;
    }

    std::string setParams = getSetFolderName(set);
    if (set == mDefaultSet)
    {
        setParams += ":**";
    }

/*
    // This works fine, but LL seems to have added a few helper functions in llinventoryfunctions.h
    // so let's make use of them. This code is just for reference

    LLViewerInventoryCategory* cat=gInventory.getCategory(set->getInventoryUUID());
    LL_WARNS("AOEngine") << cat << LL_ENDL;
    cat->rename(setParams);
    cat->updateServer(false);
    gInventory.addChangedMask(LLInventoryObserver::LABEL, cat->getUUID());
    gInventory.notifyObservers();
*/
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    rename_category(&gInventory, set->getInventoryUUID(), setParams);
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);

    LL_INFOS("AOEngine") << "sending update signal" << LL_ENDL;
    mUpdatedSignal();
}

bool AOEngine::renameSet(AOSet* set, std::string_view name)
{
    if (name.empty() || name.find(":") != std::string_view::npos)
    {
        return false;
    }
    set->setName(name);
    set->setDirty(true);

    return true;
}

std::string AOEngine::getSetFolderName(const AOSet* set) const
{
    std::string params = set->getName();
    if (set->getSitOverride())
    {
        params += ":SO";
    }
    if (set->getSmart())
    {
        params += ":SM";
    }
    if (set->getMouselookStandDisable())
    {
        params += ":DM";
    }
    return params;
}

std::string AOEngine::getStateFolderName(const AOSet::AOState* state) const
{
    std::string params = state->mName;
    if (state->mCycleTime > 0.0f)
    {
        params += llformat(":CT%.2f", state->mCycleTime);
    }
    if (state->mCycle)
    {
        params += ":CY";
    }
    if (state->mRandom)
    {
        params += ":RN";
    }
    return params;
}

void AOEngine::saveState(const AOSet::AOState* state)
{
    std::string stateParams = getStateFolderName(state);
    bool wasProtected = gSavedPerAccountSettings.getBOOL("LockAOFolders");
    gSavedPerAccountSettings.setBOOL("LockAOFolders", false);
    rename_category(&gInventory, state->mInventoryUUID, stateParams);
    gSavedPerAccountSettings.setBOOL("LockAOFolders", wasProtected);
}

void AOEngine::saveSettings()
{
    for (auto set : mSets)
    {
        if (set->getDirty())
        {
            saveSet(set);
            LL_INFOS("AOEngine") << "dirty set saved " << set->getName() << LL_ENDL;
            set->setDirty(false);
        }

        for (S32 stateIndex = 0; stateIndex < AOSet::AOSTATES_MAX; ++stateIndex)
        {
            AOSet::AOState* state = set->getState(stateIndex);
            if (state->mDirty)
            {
                saveState(state);
                LL_INFOS("AOEngine") << "dirty state saved " << state->mName << LL_ENDL;
                state->mDirty = false;
            }
        }
    }
}

void AOEngine::inMouselook(bool mouselook)
{
    if (mInMouselook == mouselook)
    {
        return;
    }

    mInMouselook = mouselook;

    if (!mCurrentSet)
    {
        return;
    }

    if (!mCurrentSet->getMouselookStandDisable())
    {
        return;
    }

    if (!mEnabled)
    {
        return;
    }

    if (mLastMotion != ANIM_AGENT_STAND)
    {
        return;
    }

    if (mouselook)
    {
        AOSet::AOState* state = mCurrentSet->getState(AOSet::Standing);
        if (!state)
        {
            return;
        }

        if (!state->mCurrentAnimationIDs.empty())
        {
            LL_DEBUGS("AOEngine") << " stopping animation(s) in state " << state->mName << LL_ENDL;
            stopCurrentAnimations(state);
        }
        stopActiveTracks();
        gAgent.sendAnimationRequest(ANIM_AGENT_STAND, ANIM_REQUEST_START);
    }
    else
    {
        stopAllStandVariants();
        gAgent.sendAnimationRequest(override(ANIM_AGENT_STAND, true), ANIM_REQUEST_START);
    }
}

void AOEngine::setDefaultSet(AOSet* set)
{
    mDefaultSet = set;
    for (auto set : mSets)
    {
        set->setDirty(true);
    }
}

void AOEngine::setOverrideSits(AOSet* set, bool override_sit)
{
    set->setSitOverride(override_sit);
    set->setDirty(true);

    if (mCurrentSet != set)
    {
        return;
    }

    if (mLastMotion != ANIM_AGENT_SIT)
    {
        return;
    }

    if (!mEnabled)
    {
        return;
    }

    if (override_sit)
    {
        stopAllSitVariants();
        gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_START);
    }
    else
    {
        // remove sit cycle cover up
        gAgent.sendAnimationRequest(ANIM_AGENT_SIT_GENERIC, ANIM_REQUEST_STOP);

        AOSet::AOState* state = mCurrentSet->getState(AOSet::Sitting);
        if (state)
        {
            if (!state->mCurrentAnimationIDs.empty())
            {
                stopCurrentAnimations(state);
            }
            stopActiveTracks();
            mAnimationChangedSignal(LLUUID::null);
        }

        if (!foreignAnimations())
        {
            gAgent.sendAnimationRequest(ANIM_AGENT_SIT, ANIM_REQUEST_START);
        }
    }
}

void AOEngine::setSmart(AOSet* set, bool smart)
{
    set->setSmart(smart);
    set->setDirty(true);

    if (!mEnabled)
    {
        return;
    }

    if (smart)
    {
        // make sure to restart the sit cancel timer to fix sit overrides when the object we are
        // sitting on is playing its own animation
        const LLViewerObject* agentRoot = dynamic_cast<LLViewerObject*>(gAgentAvatarp->getRoot());
        if (agentRoot && agentRoot->getID() != gAgentID)
        {
            mSitCancelTimer.oneShot();
        }
    }
}

void AOEngine::setDisableMouselookStands(AOSet* set, bool disabled)
{
    set->setMouselookStandDisable(disabled);
    set->setDirty(true);

    if (mCurrentSet != set)
    {
        return;
    }

    if (!mEnabled)
    {
        return;
    }

    // make sure an update happens if needed
    mInMouselook = !gAgentCamera.cameraMouselook();
    inMouselook(!mInMouselook);
}

void AOEngine::setCycle(AOSet::AOState* state, bool cycle)
{
    state->mCycle = cycle;
    state->mDirty = true;
    if (state == mActiveAlwaysState)
    {
        refreshAlwaysTimers();
    }
}

void AOEngine::setRandomize(AOSet::AOState* state, bool randomize)
{
    state->mRandom = randomize;
    state->mDirty = true;
}

void AOEngine::setCycleTime(AOSet::AOState* state, F32 time)
{
    state->mCycleTime = time;
    state->mDirty = true;
    if (state == mActiveAlwaysState)
    {
        refreshAlwaysTimers();
    }
}

void AOEngine::tick()
{
    // <FS:ND> make sure agent is alive and kicking before doing anything
    if (!isAgentAvatarValid())
    {
        return;
    }
    // </FS:ND>

    const LLUUID categoryID = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);

    if (categoryID.isNull())
    {
        LL_WARNS("AOEngine") << "no " << ROOT_FIRESTORM_FOLDER << " folder yet. Creating ..." << LL_ENDL;
        gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER);
        mAOFolder.setNull();
    }
    else
    {
        LLInventoryModel::cat_array_t* categories;
        LLInventoryModel::item_array_t* items;
        gInventory.getDirectDescendentsOf(categoryID, categories, items);
        LL_DEBUGS("AOEngine") << "cat " << categories->size() << " items " << items->size() << LL_ENDL;

        for (const auto& cat : *categories)
        {
            const std::string& catName = cat->getName();
            if (catName.compare(ROOT_AO_FOLDER) == 0)
            {
                mAOFolder = cat->getUUID();
                break;
            }
        }

        if (mAOFolder.isNull())
        {
            LL_WARNS("AOEngine") << "no " << ROOT_AO_FOLDER << " folder yet. Creating ..." << LL_ENDL;
            gInventory.createNewCategory(categoryID, LLFolderType::FT_NONE, ROOT_AO_FOLDER);
        }
        else
        {
            LL_INFOS("AOEngine") << "AO basic folder structure intact." << LL_ENDL;
            update();
        }
    }
}

bool AOEngine::importNotecard(const LLInventoryItem* item)
{
    if (item)
    {
        LL_INFOS("AOEngine") << "importing AO notecard: " << item->getName() << LL_ENDL;
        if (getSetByName(item->getName()))
        {
            LLNotificationsUtil::add("AOImportSetAlreadyExists", LLSD());
            return false;
        }

        if (!gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE) && !gAgent.isGodlike())
        {
            LLNotificationsUtil::add("AOImportPermissionDenied", LLSD());
            return false;
        }

        if (item->getAssetUUID().notNull())
        {
            // create the new set with the folder UUID where the notecard is in, so we can reference it
            // in the notecard reader, this will later be cleared to make room for the real #AO subfolder
            mImportSet = new AOSet(item->getParentUUID());
            mImportSet->setName(item->getName());

            LLUUID* newUUID = new LLUUID(item->getAssetUUID());
            const LLHost sourceSim;

            gAssetStorage->getInvItemAsset(
                sourceSim,
                gAgentID,
                gAgentSessionID,
                item->getPermissions().getOwner(),
                LLUUID::null,
                item->getUUID(),
                item->getAssetUUID(),
                item->getType(),
                &onNotecardLoadComplete,
                (void*) newUUID,
                true
            );

            return true;
        }
    }
    return false;
}

// static
void AOEngine::onNotecardLoadComplete(const LLUUID& assetUUID, LLAssetType::EType type,
                                            void* userdata, S32 status, LLExtStat extStatus)
{
    if (status != LL_ERR_NOERR)
    {
        // AOImportDownloadFailed
        LLNotificationsUtil::add("AOImportDownloadFailed", LLSD());
        // NULL tells the importer to cancel all operations and free the import set memory
        AOEngine::instance().parseNotecard(nullptr);
        return;
    }
    LL_DEBUGS("AOEngine") << "Downloading import notecard complete." << LL_ENDL;

    LLFileSystem file(assetUUID, type, LLFileSystem::READ);

    S32 notecardSize = file.getSize();
    char* buffer = new char[notecardSize + 1];
    buffer[notecardSize] = 0;

    if (file.read((U8*)buffer, notecardSize))
    {
        AOEngine::instance().parseNotecard(buffer);
    }
    else
    {
        delete[] buffer;
    }
}

void AOEngine::parseNotecard(const char* buffer)
{
    LL_DEBUGS("AOEngine") << "parsing import notecard" << LL_ENDL;

    bool isValid = false;

    if (!buffer)
    {
        LL_WARNS("AOEngine") << "buffer==NULL - aborting import" << LL_ENDL;
        // NOTE: cleanup is always the same, needs streamlining
        delete mImportSet;
        mImportSet = nullptr;
        mUpdatedSignal();
        return;
    }

    std::string text(buffer);
    delete[] buffer;

    std::vector<std::string> lines;
    LLStringUtil::getTokens(text, lines, "\n");

    auto it = std::find_if(lines.begin(), lines.end(), [](const std::string& line) {
        return line.find("Text length ") == 0;
    });

    if (it == lines.end()) {
        // Line not found
        LLNotificationsUtil::add("AOImportNoText", LLSD());
        delete mImportSet;
        mImportSet = nullptr;
        mUpdatedSignal();
        return;
    }

    // Line found, 'it' points to the found element
    std::size_t found = std::distance(lines.begin(), it) + 1;

    // mImportSet->getInventoryUUID() right now contains the folder UUID where the notecard is in
    LLViewerInventoryCategory* importCategory = gInventory.getCategory(mImportSet->getInventoryUUID());
    if (!importCategory)
    {
        LLNotificationsUtil::add("AOImportNoFolder", LLSD());
        delete mImportSet;
        mImportSet = nullptr;
        mUpdatedSignal();
        return;
    }

    std::map<std::string, LLUUID> animationMap;
    LLInventoryModel::cat_array_t* dummy;
    LLInventoryModel::item_array_t* items;

    gInventory.getDirectDescendentsOf(mImportSet->getInventoryUUID(), dummy, items);
    for (auto& item : *items)
    {
        animationMap[item->getName()] = item->getUUID();
        LL_DEBUGS("AOEngine") << "animation " << item->getName() << " has inventory UUID " << animationMap[item->getName()] << LL_ENDL;
    }

    // [ State ]Anim1|Anim2|Anim3
    for (auto index = found; index < lines.size(); ++index)
    {
        std::string line = lines[index];

        // cut off the trailing } of a notecard asset's text portion in the last line
        if (index == lines.size() - 1)
        {
            line = line.substr(0, line.size() - 1);
        }

        LLStringUtil::trim(line);

        if (line.empty())
        {
            continue;
        }

        if (line[0] == '#') // <ND/> FIRE-3801; skip comments to reduce spam to local chat.
        {
            continue;
        }

        if (line.find("[") != 0)
        {
            LLSD args;
            args["LINE"] = (S32)index;
            LLNotificationsUtil::add("AOImportNoStatePrefix", args);
            continue;
        }

        if (line.find("]") == std::string::npos)
        {
            LLSD args;
            args["LINE"] = (S32)index;
            LLNotificationsUtil::add("AOImportNoValidDelimiter", args);
            continue;
        }
        auto endTag = line.find("]");

        std::string stateName = line.substr(1, endTag - 1);
        LLStringUtil::trim(stateName);

        AOSet::AOState* newState = mImportSet->getStateByName(stateName);
        if (!newState)
        {
            LLSD args;
            args["NAME"] = stateName;
            LLNotificationsUtil::add("AOImportStateNameNotFound", args);
            continue;
        }

        std::string animationLine = line.substr(endTag + 1);
        std::vector<std::string> stepList;
        LLStringUtil::getTokens(animationLine, stepList, "|");

        for (auto stepIndex = 0; stepIndex < stepList.size(); ++stepIndex)
        {
            std::vector<std::string> memberList;
            LLStringUtil::getTokens(stepList[stepIndex], memberList, ",");
            AOSet::AOAnimationStep step;
            step.mSortOrder = static_cast<S32>(newState->mSteps.size());
            for (auto memberIndex = 0; memberIndex < memberList.size(); ++memberIndex)
            {
                std::string animationName = memberList[memberIndex];
                LLStringUtil::trim(animationName);
                if (animationName.empty())
                {
                    continue;
                }
                AOSet::AOAnimation animation;
                animation.mName = animationName;
                animation.mInventoryUUID = animationMap[animation.mName];
                if (animation.mInventoryUUID.isNull())
                {
                    LLSD args;
                    args["NAME"] = animation.mName;
                    LLNotificationsUtil::add("AOImportAnimationNotFound", args);
                    continue;
                }
                animation.mSortOrder = static_cast<S32>(step.mMembers.size());
                step.mMembers.emplace_back(std::move(animation));
            }

            if (step.mMembers.empty())
            {
                continue;
            }
            step.mIsGroup = step.mMembers.size() > 1;
            step.mName = step.mIsGroup ? nextGroupName(newState) : step.mMembers.front().mName;
            newState->mSteps.emplace_back(std::move(step));
            isValid = true;
        }
    }

    if (!isValid)
    {
        LLNotificationsUtil::add("AOImportInvalid", LLSD());
        // NOTE: cleanup is always the same, needs streamlining
        delete mImportSet;
        mImportSet = nullptr;
        mUpdatedSignal();
        return;
    }

    // clear out set UUID so processImport() knows we need to create a new folder for it
    mImportSet->setInventoryUUID(LLUUID::null);
    mTimerCollection.enableImportTimer(true);
    mImportRetryCount = 0;
    processImport(false);
}

void AOEngine::processImportState(AOSet::AOState* state)
{
    if (state->mInventoryUUID.isNull())
    {
        return;
    }

    for (S32 stepIndex = static_cast<S32>(state->mSteps.size()) - 1; stepIndex >= 0; --stepIndex)
    {
        AOSet::AOAnimationStep step = std::move(state->mSteps[stepIndex]);
        state->mSteps.erase(state->mSteps.begin() + stepIndex);

        if (!step.mIsGroup)
        {
            LL_DEBUGS("AOEngine") << "linking animation " << step.mName << LL_ENDL;
            if (!createAnimationLink(state->mInventoryUUID, gInventory.getItem(step.mMembers.front().mInventoryUUID)))
            {
                LLSD args;
                args["NAME"] = step.mName;
                LLNotificationsUtil::add("AOImportLinkFailed", args);
            }
            continue;
        }

        std::string groupName = sanitiseFolderName(step.mName, "Group") + llformat(":OR%d", step.mSortOrder);
        std::vector<AOSet::AOAnimation> members = std::move(step.mMembers);

        LL_DEBUGS("AOEngine") << "creating group subfolder " << groupName << LL_ENDL;
        gInventory.createNewCategory(state->mInventoryUUID, LLFolderType::FT_NONE, groupName, [this, members](const LLUUID& new_cat_id)
        {
            for (S32 memberIndex = static_cast<S32>(members.size()) - 1; memberIndex >= 0; --memberIndex)
            {
                if (!createAnimationLink(new_cat_id, gInventory.getItem(members[memberIndex].mInventoryUUID)))
                {
                    LLSD args;
                    args["NAME"] = members[memberIndex].mName;
                    LLNotificationsUtil::add("AOImportLinkFailed", args);
                }
            }
        });
    }

    for (S32 trackIndex = static_cast<S32>(state->mTracks.size()) - 1; trackIndex >= 0; --trackIndex)
    {
        AOSet::AOTrack track = std::move(state->mTracks[trackIndex]);
        state->mTracks.erase(state->mTracks.begin() + trackIndex);

        track.mInventoryUUID.setNull();
        std::string trackName = getTrackFolderName(track);
        std::vector<AOSet::AOAnimation> members = std::move(track.mAnimations);

        LL_DEBUGS("AOEngine") << "creating track subfolder " << trackName << LL_ENDL;
        gInventory.createNewCategory(state->mInventoryUUID, LLFolderType::FT_NONE, trackName, [this, members](const LLUUID& new_cat_id)
        {
            for (S32 memberIndex = static_cast<S32>(members.size()) - 1; memberIndex >= 0; --memberIndex)
            {
                if (!createAnimationLink(new_cat_id, gInventory.getItem(members[memberIndex].mInventoryUUID)))
                {
                    LLSD args;
                    args["NAME"] = members[memberIndex].mName;
                    LLNotificationsUtil::add("AOImportLinkFailed", args);
                }
            }
        });
    }
}

void AOEngine::processImport(bool from_timer)
{
    if (mImportSet->getInventoryUUID().isNull())
    {
        // create new inventory folder for this AO set, the next timer tick should pick it up
        addSet(getSetFolderName(mImportSet), [this](const LLUUID& new_cat_id)
        {
            mImportSet->setInventoryUUID(new_cat_id);
        }, false);

        mImportRetryCount++;

        // if it takes this long to create a new inventoey category, there might be something wrong,
        // so give some user feedback at first
        if (mImportRetryCount == 2)
        {
            LLSD args;
            args["NAME"] = mImportSet->getName();
            LLNotificationsUtil::add("AOImportRetryCreateSet", args);
            return;
        }
        // by now there clearly is something wrong, so stop trying
        else if (mImportRetryCount == 5)
        {
            // NOTE: cleanup is the same as at the end of this function. Needs streamlining.
            LLSD args;
            args["NAME"] = mImportSet->getName();
            mTimerCollection.enableImportTimer(false);
            delete mImportSet;
            mImportSet = nullptr;
            mUpdatedSignal();
            LLNotificationsUtil::add("AOImportAbortCreateSet", args);
        }

        return;
    }

    bool allComplete = true;
    for (S32 index = 0; index < AOSet::AOSTATES_MAX; ++index)
    {
        AOSet::AOState* state = mImportSet->getState(index);
        if (!state->mSteps.empty() || !state->mTracks.empty())
        {
            allComplete = false;
            LL_DEBUGS("AOEngine") << "state " << state->mName << " still has animations to link." << LL_ENDL;

            if (state->mInventoryUUID.isNull())
            {
                gInventory.createNewCategory(mImportSet->getInventoryUUID(), LLFolderType::FT_NONE, getStateFolderName(state), [this, state](const LLUUID& new_cat_id)
                {
                    LL_DEBUGS("AOEngine") << "new_cat_id: " << new_cat_id << LL_ENDL;
                    state->mInventoryUUID = new_cat_id;
                    processImportState(state);
                });
            }
            else
            {
                processImportState(state);
            }
        }
    }

    if (allComplete)
    {
        mTimerCollection.enableImportTimer(false);
        mOldImportSets.push_back(mImportSet); //<ND/> FIRE-3801; Cannot delete here, or LLInstanceTracker gets upset. Just remember and delete mOldImportSets once we can.
        mImportSet = nullptr;
        reload(from_timer);
        LLNotificationsUtil::add("AOImportComplete");
    }
}

const LLUUID& AOEngine::getAOFolder() const
{
    return mAOFolder;
}

void AOEngine::onRegionChange()
{
    // do nothing if the AO is off
    if (!mEnabled)
    {
        return;
    }

    // catch errors without crashing
    if (!mCurrentSet)
    {
        LL_DEBUGS("AOEngine") << "Current set was NULL" << LL_ENDL;
        return;
    }

    // sitting needs special attention
    if (mLastMotion == ANIM_AGENT_SIT)
    {
        // do nothing if sit overrides was disabled
        if (!mCurrentSet->getSitOverride())
        {
            return;
        }

        // do nothing if the last overridden motion wasn't a sit.
        // happens when sit override is enabled but there were no
        // sit animations added to the set yet
        if (mLastOverriddenMotion != ANIM_AGENT_SIT)
        {
            return;
        }

        AOSet::AOState* state = mCurrentSet->getState(AOSet::Sitting);
        if (!state)
        {
            return;
        }

        // do nothing if no AO animation is playing (e.g. smart sit cancel)
        if (state->mCurrentAnimationIDs.empty())
        {
            return;
        }
    }

    // restart current animation on region crossing
    gAgent.sendAnimationRequest(mLastMotion, ANIM_REQUEST_START);
}

void AOEngine::updatePersistedStateAnimations()
{

}

// ----------------------------------------------------

AOSitCancelTimer::AOSitCancelTimer()
:   LLEventTimer(0.1f),
    mTickCount(0)
{
    mEventTimer.stop();
}

void AOSitCancelTimer::oneShot()
{
    mTickCount = 0;
    mEventTimer.start();
}

void AOSitCancelTimer::stop()
{
    mEventTimer.stop();
}

bool AOSitCancelTimer::tick()
{
    mTickCount++;
    AOEngine::instance().checkSitCancel();
    if (mTickCount == 10)
    {
        mEventTimer.stop();
    }
    return false;
}

// ----------------------------------------------------

AOTimerCollection::AOTimerCollection()
:   LLEventTimer(INVENTORY_POLLING_INTERVAL),
    mInventoryTimer(true),
    mSettingsTimer(false),
    mReloadTimer(false),
    mImportTimer(false)
{
    updateTimers();
}

bool AOTimerCollection::tick()
{
    if (mInventoryTimer)
    {
        LL_DEBUGS("AOEngine") << "Inventory timer tick()" << LL_ENDL;
        AOEngine::instance().tick();
    }
    if (mSettingsTimer)
    {
        LL_DEBUGS("AOEngine") << "Settings timer tick()" << LL_ENDL;
        AOEngine::instance().saveSettings();
    }
    if (mReloadTimer)
    {
        LL_DEBUGS("AOEngine") << "Reload timer tick()" << LL_ENDL;
        AOEngine::instance().reload(true);
    }
    if (mImportTimer)
    {
        LL_DEBUGS("AOEngine") << "Import timer tick()" << LL_ENDL;
        AOEngine::instance().processImport(true);
    }

    // always return false or the LLEventTimer will be deleted -> crash
    return false;
}

void AOTimerCollection::enableInventoryTimer(bool enable)
{
    mInventoryTimer = enable;
    updateTimers();
}

void AOTimerCollection::enableSettingsTimer(bool enable)
{
    mSettingsTimer = enable;
    updateTimers();
}

void AOTimerCollection::enableReloadTimer(bool enable)
{
    mReloadTimer = enable;
    updateTimers();
}

void AOTimerCollection::enableImportTimer(bool enable)
{
    mImportTimer = enable;
    updateTimers();
}

void AOTimerCollection::updateTimers()
{
    if (!mInventoryTimer && !mSettingsTimer && !mReloadTimer && !mImportTimer)
    {
        LL_DEBUGS("AOEngine") << "no timer needed, stopping internal timer." << LL_ENDL;
        mEventTimer.stop();
    }
    else
    {
        LL_DEBUGS("AOEngine") << "timer needed, starting internal timer." << LL_ENDL;
        mEventTimer.start();
    }
}

AOTrackTimer::AOTrackTimer(S32 stateEnum, S32 trackIndex, F32 period)
:   LLEventTimer(period),
    mStateEnum(stateEnum),
    mTrackIndex(trackIndex)
{
}

bool AOTrackTimer::tick()
{
    AOEngine::instance().trackTimeout(mStateEnum, mTrackIndex);
    // never self-delete, the engine owns this timer through mActiveTrackTimers
    return false;
}

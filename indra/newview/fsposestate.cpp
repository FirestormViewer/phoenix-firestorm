#include "fsposestate.h"
#include "llinventorymodel.h" // gInventory

std::map<LLUUID, std::vector<FSPoseState::fsMotionState>> FSPoseState::sMotionStates;
std::map<LLUUID, int> FSPoseState::sCaptureOrder;
std::map<LLUUID, bool> FSPoseState::sMotionStatesOwnedByMe;

void FSPoseState::captureMotionStates(LLVOAvatar* avatar)
{
    if (!avatar)
        return;

    sCaptureOrder[avatar->getID()] = 0;

    for (auto anim_it = avatar->mPlayingAnimations.begin(); anim_it != avatar->mPlayingAnimations.end(); ++anim_it)
    {
        LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(avatar->findMotion(anim_it->first));
        if (!motion)
            continue;

        fsMotionState newState;
        newState.motionId       = anim_it->first;
        newState.lastUpdateTime = motion->getLastUpdateTime();
        newState.captureOrder   = 0;
        newState.gAgentOwnsPose = canSaveMotionId(anim_it->first);

        sMotionStates[avatar->getID()].push_back(newState);
    }
}

void FSPoseState::updateMotionStates(LLVOAvatar* avatar, FSPosingMotion* posingMotion, std::vector<S32> jointNumbersRecaptured)
{
    if (!avatar || !posingMotion)
        return;

    sCaptureOrder[avatar->getID()]++;

    // if an animation for avatar is a subset of jointNumbersRecaptured, delete it
    // this happens on second/subsequent recaptures; the first recapture is no longer needed
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end();)
    {
        if (vector2IsSubsetOfVector1(jointNumbersRecaptured, (*it).jointNumbersAnimated))
            it = sMotionStates[avatar->getID()].erase(it);
        else
            it++;
    }

    for (auto anim_it = avatar->mPlayingAnimations.begin(); anim_it != avatar->mPlayingAnimations.end(); anim_it++)
    {
        LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(avatar->findMotion(anim_it->first));
        if (!motion)
            continue;

        if (!posingMotion->otherMotionAnimatesJoints(motion, jointNumbersRecaptured))
            continue;

        bool foundMatch = false;
        for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); it++)
        {
            bool motionIdMatches = (*it).motionId == anim_it->first;
            bool updateTimesMatch = (*it).lastUpdateTime == motion->getLastUpdateTime(); // consider when recapturing the same animation at different times for a subset of bones

            foundMatch = motionIdMatches && updateTimesMatch;
            if (foundMatch)
                break;
        }

        if (foundMatch)
            continue;

        fsMotionState newState;
        newState.motionId             = anim_it->first;
        newState.lastUpdateTime       = motion->getLastUpdateTime();
        newState.jointNumbersAnimated = jointNumbersRecaptured;
        newState.captureOrder         = sCaptureOrder[avatar->getID()];
        newState.gAgentOwnsPose       = canSaveMotionId(anim_it->first);

        sMotionStates[avatar->getID()].push_back(newState);
    }
}

void FSPoseState::purgeMotionStates(LLVOAvatar* avatar)
{
    if (!avatar)
        return;

     sMotionStates[avatar->getID()].clear();
}

void FSPoseState::writeMotionStates(LLVOAvatar* avatar, bool ignoreOwnership, LLSD* saveRecord)
{
    if (!avatar)
        return;

    int animNumber = 0;
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); ++it)
    {
        if (!ignoreOwnership && !it->gAgentOwnsPose)
            continue;

        std::string uniqueAnimId                            = "poseState" + std::to_string(animNumber++);
        (*saveRecord)[uniqueAnimId]["animationId"]          = it->motionId.asString();
        (*saveRecord)[uniqueAnimId]["lastUpdateTime"]       = it->lastUpdateTime;
        (*saveRecord)[uniqueAnimId]["jointNumbersAnimated"] = encodeVectorToString(it->jointNumbersAnimated);
        (*saveRecord)[uniqueAnimId]["captureOrder"]         = it->captureOrder;
    }
}

void FSPoseState::restoreMotionStates(LLVOAvatar* avatar, bool ignoreOwnership, LLSD pose)
{
    if (!avatar)
        return;

    sCaptureOrder[avatar->getID()] = 0;

    for (auto itr = pose.beginMap(); itr != pose.endMap(); ++itr)
    {
        std::string const& name = itr->first;
        LLSD const& control_map = itr->second;

        if (!name.starts_with("poseState"))
            continue;

        fsMotionState newState;

        if (control_map.has("animationId"))
        {
            std::string const name = control_map["animationId"].asString();
            LLUUID animId;
            if (LLUUID::parseUUID(name, &animId))
            {
                newState.motionId = animId;
                newState.gAgentOwnsPose = ignoreOwnership || canSaveMotionId(animId);

                if (ignoreOwnership)
                    sMotionStatesOwnedByMe[animId] = true;
            }
        }

        if (control_map.has("lastUpdateTime"))
            newState.lastUpdateTime = (F32)control_map["lastUpdateTime"].asReal();

        if (control_map.has("jointNumbersAnimated"))
            newState.jointNumbersAnimated = decodeStringToVector(control_map["jointNumbersAnimated"].asString());

        if (control_map.has("captureOrder"))
            newState.captureOrder = control_map["captureOrder"].asInteger();

        if (newState.captureOrder > sCaptureOrder[avatar->getID()])
            sCaptureOrder[avatar->getID()] = newState.captureOrder;

        sMotionStates[avatar->getID()].push_back(newState);
    }
}

bool FSPoseState::applyMotionStatesToPosingMotion(LLVOAvatar* avatar, FSPosingMotion* posingMotion)
{
    if (!avatar || !posingMotion)
        return false;

    bool allMotionsApplied = true;

    std::sort(sMotionStates[avatar->getID()].begin(), sMotionStates[avatar->getID()].end(), compareByCaptureOrder());

    int lastCaptureOrder   = 0;
    bool needPriorityReset = false;
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); it++)
    {
        needPriorityReset = it->captureOrder > lastCaptureOrder;

        if (it->motionApplied)
            continue;

        LLKeyframeMotion* kfm = dynamic_cast<LLKeyframeMotion*>(avatar->findMotion(it->motionId));

        if (kfm)
        {
            if (needPriorityReset)
            {
                lastCaptureOrder = it->captureOrder;
                resetPriorityForCaptureOrder(avatar, posingMotion, lastCaptureOrder);
            }

            it->motionApplied = posingMotion->loadOtherMotionToBaseOfThisMotion(kfm, it->lastUpdateTime, it->jointNumbersAnimated);
        }
        else
        {
            avatar->startMotion(it->motionId); // only start if not a kfm; then wait until it casts as a kfm
            avatar->stopMotion(it->motionId);  // only stop if we have used it and we started it
        }

        allMotionsApplied &= it->motionApplied;
    }

    return allMotionsApplied;
}

void FSPoseState::resetPriorityForCaptureOrder(LLVOAvatar* avatar, FSPosingMotion* posingMotion, int captureOrder)
{
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); it++)
    {
        if (it->jointNumbersAnimated.empty())
            continue;
        if (it->motionApplied)
            continue;
        if (it->captureOrder != captureOrder)
            continue;

        posingMotion->resetBonePriority(it->jointNumbersAnimated);
    }
}

bool FSPoseState::canSaveMotionId(LLAssetID motionId)
{
    if (!gAgentAvatarp || gAgentAvatarp.isNull())
        return false;

    if (sMotionStatesOwnedByMe[motionId])
        return true;

    // does the animation exist in inventory
    LLInventoryItem* item = gInventory.getItem(motionId);
    if (item && item->getPermissions().getOwner() == gAgentAvatarp->getID())
    {
        sMotionStatesOwnedByMe[motionId] = true;
        return sMotionStatesOwnedByMe[motionId];
    }

    for (const auto& [anim_object_id, anim_anim_id] : gAgentAvatarp->mAnimationSources)
    {
        if (anim_anim_id != motionId)
            continue;

        // is the item that started the anim in inventory
        item = gInventory.getItem(anim_object_id);
        if (item && item->getPermissions().getOwner() == gAgentAvatarp->getID())
        {
            sMotionStatesOwnedByMe[motionId] = true;
            return sMotionStatesOwnedByMe[motionId];
        }

        // is the item that start the animation in-world
        LLViewerObject* object = gObjectList.findObject(anim_object_id);
        if (object && object->permYouOwner())
        {
            sMotionStatesOwnedByMe[motionId] = true;
            return sMotionStatesOwnedByMe[motionId];
        }

        return false;
    }

    return false;
}

bool FSPoseState::vector2IsSubsetOfVector1(std::vector<S32> newRecapture, std::vector<S32> oldRecapture)
{
    if (newRecapture.size() < 1)
        return false;
    if (oldRecapture.size() < 1)
        return false;

    if (newRecapture.size() < oldRecapture.size())
        return false;

    for (S32 number : oldRecapture)
        if (std::find(newRecapture.begin(), newRecapture.end(), number) == newRecapture.end())
            return false;

    return true;
}

std::string FSPoseState::encodeVectorToString(std::vector<S32> vector)
{
    std::string encoded = "";
    if (vector.size() < 1)
        return encoded;

    for (S32 numberToEncode : vector)
    {
        if (numberToEncode > 251) // max 216 at time of writing
            continue;

        S32 number = numberToEncode;

        if (number >= 189)
        {
            encoded += "~";
            number -= 189;
        }

        if (number >= 126)
        {
            encoded += "}";
            number -= 126;
        }

        if (number >= 63)
        {
            encoded += "|";
            number -= 63;
        }

        encoded += char(number + int('?'));
    }

    return encoded;
}

std::vector<S32> FSPoseState::decodeStringToVector(std::string vector)
{
    std::vector<S32> decoded;
    if (vector.empty())
        return decoded;

    S32 number = 0;
    for (char ch : vector)
    {
        if (ch > '~' || ch < '?')
            continue;

        if (ch == '~')
        {
            number += 189;
            continue;
        }

        if (ch == '}')
        {
            number += 126;
            continue;
        }

        if (ch == '|')
        {
            number += 63;
            continue;
        }

        number -= int('?');
        number += S32(ch);
        decoded.push_back(number);
        number = 0;
    }

    return decoded;
}

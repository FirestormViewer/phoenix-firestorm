#include "fsposestate.h"
#include "llinventorymodel.h" // gInventory

std::map<LLUUID, std::vector<FSPoseState::fsMotionState>> FSPoseState::sMotionStates;
std::map<LLUUID, int> FSPoseState::sCaptureOrder;

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
        newState.avatarId       = avatar->getID();
        newState.motionId       = anim_it->first;
        newState.lastUpdateTime = motion->getLastUpdateTime();
        newState.captureOrder   = 0;
        newState.avatarOwnsPose = canSaveMotionId(avatar, anim_it->first);

        sMotionStates[avatar->getID()].push_back(newState);
    }
}

void FSPoseState::updateMotionStates(LLVOAvatar* avatar, FSPosingMotion* posingMotion, std::string jointNamesRecaptured)
{
    if (!avatar || !posingMotion)
        return;

    sCaptureOrder[avatar->getID()]++;

    // if an animation for avatar is a subset of jointNamesRecaptured, delete it
    // this happens on second/subsequent recaptures; the first recapture is no longer needed
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end();)
    {
        bool avatarMatches    = (*it).avatarId == avatar->getID();
        std::string joints    = (*it).jointNamesAnimated;
        bool recaptureMatches = !joints.empty() && !jointNamesRecaptured.empty() && jointNamesRecaptured.find(joints) != std::string::npos;

        if (avatarMatches && recaptureMatches)
            it = sMotionStates[avatar->getID()].erase(it);
        else
            it++;
    }

    for (auto anim_it = avatar->mPlayingAnimations.begin(); anim_it != avatar->mPlayingAnimations.end(); anim_it++)
    {
        LLKeyframeMotion* motion = dynamic_cast<LLKeyframeMotion*>(avatar->findMotion(anim_it->first));
        if (!motion)
            continue;

        if (!posingMotion->otherMotionAnimatesJoints(motion, jointNamesRecaptured))
            continue;

        bool foundMatch = false;
        for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); it++)
        {
            bool avatarMatches = (*it).avatarId == avatar->getID();
            bool motionIdMatches = (*it).motionId == anim_it->first;
            bool updateTimesMatch = (*it).lastUpdateTime == motion->getLastUpdateTime(); // consider when recapturing the same animation at different times for a subset of bones

            foundMatch = avatarMatches && motionIdMatches && updateTimesMatch;
            if (foundMatch)
                break;
        }

        if (foundMatch)
            continue;

        fsMotionState newState;
        newState.avatarId           = avatar->getID();
        newState.motionId           = anim_it->first;
        newState.lastUpdateTime     = motion->getLastUpdateTime();
        newState.jointNamesAnimated = jointNamesRecaptured;
        newState.captureOrder       = sCaptureOrder[avatar->getID()];
        newState.avatarOwnsPose     = canSaveMotionId(avatar, anim_it->first);

        sMotionStates[avatar->getID()].push_back(newState);
    }
}

void FSPoseState::purgeMotionStates(LLVOAvatar* avatar)
{
    if (!avatar)
        return;

    std::vector<fsMotionState>::iterator it;
    for (it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end();)
    {
        if ((*it).avatarId == avatar->getID())
            it = sMotionStates[avatar->getID()].erase(it);
        else
            it++;
    }
}

void FSPoseState::writeMotionStates(LLVOAvatar* avatar, LLSD* saveRecord)
{
    if (!avatar)
        return;

    int animNumber = 0;
    for (auto it = sMotionStates[avatar->getID()].begin(); it != sMotionStates[avatar->getID()].end(); ++it)
    {
        if (it->avatarId != avatar->getID())
            continue;
        if (!it->avatarOwnsPose)
            continue;

        std::string uniqueAnimId                          = "poseState" + std::to_string(animNumber++);
        (*saveRecord)[uniqueAnimId]["animationId"]        = it->motionId.asString();
        (*saveRecord)[uniqueAnimId]["lastUpdateTime"]     = it->lastUpdateTime;
        (*saveRecord)[uniqueAnimId]["jointNamesAnimated"] = it->jointNamesAnimated;
        (*saveRecord)[uniqueAnimId]["captureOrder"]       = it->captureOrder;
        (*saveRecord)[uniqueAnimId]["playOrder"]          = animNumber;
    }
}

void FSPoseState::restoreMotionStates(LLVOAvatar* avatar, LLSD pose)
{
    if (!avatar)
        return;

    sCaptureOrder[avatar->getID()] = 0;

    for (auto itr = pose.beginMap(); itr != pose.endMap(); ++itr)
    {
        std::string const& name        = itr->first;
        LLSD const&        control_map = itr->second;

        if (!name.starts_with("poseState"))
            continue;

        fsMotionState newState;
        newState.avatarId = avatar->getID();

        if (control_map.has("animationId"))
        {
            std::string const name = control_map["animationId"].asString();
            LLUUID animId;
            if (LLUUID::parseUUID(name, &animId))
                newState.motionId = animId;
        }

        if (control_map.has("lastUpdateTime"))
            newState.lastUpdateTime = (F32)control_map["lastUpdateTime"].asReal();

        if (control_map.has("jointNamesAnimated"))
            newState.jointNamesAnimated = control_map["jointNamesAnimated"].asString();

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
        if (it->avatarId != avatar->getID())
            continue;

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

            it->motionApplied = posingMotion->loadOtherMotionToBaseOfThisMotion(kfm, it->lastUpdateTime, it->jointNamesAnimated);
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
        if (it->jointNamesAnimated.empty())
            continue;
        if (it->avatarId != avatar->getID())
            continue;
        if (it->motionApplied)
            continue;
        if (it->captureOrder != captureOrder)
            continue;

        posingMotion->resetBonePriority(it->jointNamesAnimated);
    }
}

bool FSPoseState::canSaveMotionId(LLVOAvatar* avatar, LLAssetID motionId)
{
    if (!gAgentAvatarp || gAgentAvatarp.isNull())
        return false;

    // does the animation exist in inventory
    LLInventoryItem* item = gInventory.getItem(motionId);
    if (item && item->getPermissions().getOwner() == avatar->getID())
        return true;

    for (const auto& [anim_object_id, anim_anim_id] : gAgentAvatarp->mAnimationSources)
    {
        if (anim_anim_id != motionId)
            continue;

        // is the item that started the anim in inventory
        item = gInventory.getItem(anim_object_id);
        if (item && item->getPermissions().getOwner() == avatar->getID())
            return true;

        // is the item that start the animation in-world
        LLViewerObject* object = gObjectList.findObject(anim_object_id);
        if (object && object->permYouOwner())
            return true;

        return false;
    }

    return false;
}

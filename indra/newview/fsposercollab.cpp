#include "llvoavatar.h"
#include "fsposercollab.h"
#include "llagent.h"
#include "llinstantmessage.h"
#include "llimview.h"
#include "llagentui.h"
#include "llmath.h"

constexpr std::string_view POSER_MESSAGE_PREFIX      = "#POSER+"; // preamble on all poser messages.
constexpr std::string_view POSER_MESSAGE_START_BODY  = "#SBOD";   // token indicating message has joint rot/pos/scale info
constexpr std::string_view POSER_MESSAGE_START_POSES = "#SPOS";   // token indicating message has playing animation payload
constexpr std::string_view POSER_MESSAGE_STOP_POSING = "#STOP";   // token indicating sender is stopping posing
constexpr std::string_view POSER_MESSAGE_PERMISSION  = "#PERM";   // token indicating message has a permission payload
constexpr std::string_view DELIM                     = "+";       // delimiter separating message tokens
constexpr std::string_view VECTOR_ALL_ZEROS          = "%";       // char encoding the trivial vector
constexpr size_t           POSER_MESSAGE_LENGTH      = 970;       // maximum message length

/// <summary>
/// The constant time interval, in seconds, we wait after the last message to update arrived, before we do anything.
/// UI controls like sliders and track pads send many events, one per frame more-or-less: the user is doing a thing.
/// We want to wait until they have stopped or paused before enqueuing an update and sending an IM.
/// </summary>
constexpr std::chrono::duration<double> EnqueueWaitInterval = std::chrono::duration<double>(0.3);
constexpr std::chrono::duration<double> PoseReloadWaitInterval = std::chrono::duration<double>(0.5);

FSPoserCollab* gPoserCollaborator = NULL;
std::map<LLUUID, E_CollabState> FSPoserCollab::sAvatarIdToCollabState;
std::map<LLUUID, int> FSPoserCollab::sAvatarIdToPosingStateReloadTries;

FSPoserCollab::FSPoserCollab(FSPoserAnimator* poserAnimator, FSPoserCollab::callback_t1 callback)
{
    mPoserAnimator        = poserAnimator;
    mPoseChatTimer        = new FSPoseChatTimer(boost::bind(&FSPoserCollab::onTimedCollabEvent, this));
    mFloaterPoserCallback = callback;
}

FSPoserCollab::~FSPoserCollab()
{
    delete mPoseChatTimer;
}

FSPoserCollab* FSPoserCollab::initiateCollaborator(FSPoserAnimator* poserAnimator, FSPoserCollab::callback_t1 callback)
{
    if (!gPoserCollaborator)
        gPoserCollaborator = new FSPoserCollab(poserAnimator, callback);

    return gPoserCollaborator;
}

void FSPoserCollab::destroyCollaborator()
{
    if (!gPoserCollaborator)
        return;

    delete gPoserCollaborator;
    gPoserCollaborator = NULL;
}

bool FSPoserCollab::onTimedCollabEvent()
{
    if (!gPoserCollaborator)
        return false;

    checkOnlineStatuses();
    checkAnyPosesNeedReloading();
    processEnqueuedMessages();
    if (mEnqueuedChatMessages.size() < 1)
        return false;

    FSEnqueuedPoseMessage nextMessageToSend = mEnqueuedChatMessages[0];
    mEnqueuedChatMessages.pop_front();

    LLVOAvatar* avatarToMessage = getAvatarByUuid(nextMessageToSend.mRecipient);
    if (!avatarToMessage)
        return false;

    sendMessage(avatarToMessage, nextMessageToSend.message);

    return true;
}

// this is from our UI, from our point of view: we are telling someone else what we want
void FSPoserCollab::updateCollabPermission(LLVOAvatar* avatarToUpdate, E_CollabState stateToSupply)
{
    if (!avatarToUpdate)
        return;

    bool          shouldSendMessage       = false;
    LLUUID        avatarWhoseStateChanged = avatarToUpdate->getID();
    E_CollabState currentState            = sAvatarIdToCollabState[avatarWhoseStateChanged];

    switch (stateToSupply)
    {
        case COLLAB_THEY_ASKED_ME:
        case COLLAB_I_POSE_THEM:
        case COLLAB_POSE_EACH_OTHER:
            // these states cannot be asserted by us
            break;

        case COLLAB_I_ASKED_THEM:
            if (currentState == COLLAB_THEY_ASKED_ME)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_PERM_GRANTED;
            else
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_I_ASKED_THEM;

            shouldSendMessage = true;
            break;

        case COLLAB_PERM_GRANTED:
            if (currentState == COLLAB_POSE_EACH_OTHER)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_I_POSE_THEM;
            if (currentState == COLLAB_THEY_POSE_ME)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_PERM_GRANTED;

            shouldSendMessage = true;
            break;

        case COLLAB_THEY_POSE_ME:
            if (currentState == COLLAB_I_POSE_THEM)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_POSE_EACH_OTHER;
            else
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_THEY_POSE_ME;

            shouldSendMessage = true;
            break;

        case COLLAB_NONE:
        case COLLAB_PERM_ENDED:
        case COLLAB_PERM_DENIED:
            sAvatarIdToCollabState[avatarWhoseStateChanged] = stateToSupply;
            shouldSendMessage = true;
            break;

        default:
            break;
    }

    if (shouldSendMessage)
    {
        std::string message = std::string(POSER_MESSAGE_PERMISSION);
        message += DELIM;
        message += std::to_string(stateToSupply);

        sendMessage(avatarToUpdate, message);
    }

    if (!mFloaterPoserCallback.empty())
        mFloaterPoserCallback(avatarWhoseStateChanged);
}

// this is a message from another client
void FSPoserCollab::processCollabStateMessage(std::string newState, LLVOAvatar* avatar)
{
    if (newState.empty() || !avatar)
        return;

    int stateValue;
    if (!tryParseInt(newState, &stateValue))
        return;

    if (stateValue < 0 || stateValue >= COLLAB_LAST)
        return;

    E_CollabState stateTheyWant           = static_cast<E_CollabState>(stateValue);
    LLUUID        avatarWhoseStateChanged = avatar->getID();
    E_CollabState currentState            = sAvatarIdToCollabState[avatarWhoseStateChanged];

    switch (stateTheyWant)
    {
        case COLLAB_POSE_EACH_OTHER:
        case COLLAB_I_POSE_THEM:
        case COLLAB_THEY_ASKED_ME:
            break; // these states cannot be asserted by another

        case COLLAB_PERM_GRANTED: // they can use this only to rescind previously granted permission
            if (currentState == COLLAB_POSE_EACH_OTHER)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_THEY_POSE_ME;

            if (currentState == COLLAB_I_POSE_THEM)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_PERM_GRANTED;

            break;

        case COLLAB_I_ASKED_THEM:
            // if they sent us 'I asked them', on receipt that means 'they asked me'
            if (currentState == COLLAB_I_ASKED_THEM)
            {
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_PERM_GRANTED;

                // In the instance other party asked while our poser was closed, we send this so they pose.
                std::string message = std::string(POSER_MESSAGE_PERMISSION);
                message += DELIM;
                message += std::to_string(COLLAB_I_ASKED_THEM);

                sendMessage(avatar, message);
            }
            else
            {
                if (currentState != COLLAB_PERM_GRANTED)
                    sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_THEY_ASKED_ME;
            }
            break;

        case COLLAB_THEY_POSE_ME:
            // if they sent us 'They may pose me', on receipt that means 'i can pose them'
            if (currentState == COLLAB_PERM_GRANTED)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_I_POSE_THEM;

            if (currentState == COLLAB_THEY_POSE_ME)
                sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_POSE_EACH_OTHER;

            break;

        case COLLAB_NONE:
        case COLLAB_PERM_ENDED:
        case COLLAB_PERM_DENIED:
            // it is some kind of 'stop'
            sAvatarIdToCollabState[avatarWhoseStateChanged] = stateTheyWant;
            break;

        default:
            break;
    }

    if (!mFloaterPoserCallback.empty())
        mFloaterPoserCallback(avatarWhoseStateChanged);
}

E_CollabState FSPoserCollab::getCollabLocalState(LLVOAvatar* avatar)
{
    if (!avatar || avatar->isSelf())
        return COLLAB_NONE;

    return sAvatarIdToCollabState[avatar->getID()];
}

void FSPoserCollab::checkOnlineStatuses()
{
    for (auto it = sAvatarIdToPosingStateReloadTries.begin(); it != sAvatarIdToPosingStateReloadTries.end(); it++)
    {
        if (it->first == gAgentID)
            continue;

        bool isOnline = LLAvatarTracker::instance().isBuddyOnline(it->first);
        if (isOnline)
            continue;

        if (sAvatarIdToCollabState[it->first] <= COLLAB_PERM_ENDED)
            continue;

        sAvatarIdToCollabState[it->first] = it->second >= COLLAB_PERM_ENDED ? COLLAB_PERM_ENDED : COLLAB_NONE;

        if (!mFloaterPoserCallback.empty())
            mFloaterPoserCallback(it->first);
    }
}

void FSPoserCollab::checkAnyPosesNeedReloading()
{
    auto timeIntervalSinceLastLoadCheck = std::chrono::system_clock::now() - mTimeLastAttemptedPoseLoad;
    if (timeIntervalSinceLastLoadCheck < PoseReloadWaitInterval)
        return;

    mTimeLastAttemptedPoseLoad = std::chrono::system_clock::now();

    for (auto it = sAvatarIdToPosingStateReloadTries.begin(); it != sAvatarIdToPosingStateReloadTries.end(); it++)
    {
        if (it->first == gAgentID)
            continue;

        if (it->second < 1)
            continue;

        if (sAvatarIdToCollabState[it->first] < COLLAB_PARTY_MODE)
            continue;

        LLVOAvatar* avatarToPose = getAvatarByUuid(it->first);
        if (!avatarToPose)
            continue;

        bool loadSuccess = mPoserAnimator->applyStatesToPosingMotion(avatarToPose);
        if (loadSuccess)
            sAvatarIdToPosingStateReloadTries[it->first] = 0;
        else
            sAvatarIdToPosingStateReloadTries[it->first] -= 1;
    }
}

void FSPoserCollab::processEnqueuedMessages()
{
    auto timeIntervalSinceLastChange = std::chrono::system_clock::now() - mTimeLastUpdateMessageArrived;
    if (timeIntervalSinceLastChange < EnqueueWaitInterval)
        return;

    for (auto messIt = mAvatarIdToMessageType.begin(); messIt != mAvatarIdToMessageType.end(); messIt++)
    {
        LLVOAvatar* avatarWhosePoseChanged = getAvatarByUuid(messIt->first);
        if (!avatarWhosePoseChanged)
            continue;

        auto changeType = messIt->second;
        std::string changedAvId = messIt->first.asString();

        std::vector<std::string> bodyRotPosScaleText;
        if (changeType == POSECHANGE_BONE || changeType == POSECHANGE_BOTH)
            bodyRotPosScaleText = getRotPosScaleDiffAsText(avatarWhosePoseChanged);

        std::vector<std::string> poseInfoText;
        if (changeType == POSECHANGE_BASE || changeType == POSECHANGE_BOTH)
            poseInfoText = getPoseInfoAsText(avatarWhosePoseChanged);

        for (auto it = sAvatarIdToCollabState.begin(); it != sAvatarIdToCollabState.end(); it++)
        {
            if (it->second < COLLAB_PERM_GRANTED)
                continue;

            // add body message(s)
            for (size_t i = 0; i < bodyRotPosScaleText.size(); i++)
            {
                FSEnqueuedPoseMessage newMessage;
                newMessage.mRecipient = it->first;
                newMessage.message = std::string(POSER_MESSAGE_START_BODY);
                newMessage.message += DELIM;
                newMessage.message += changedAvId;
                newMessage.message += DELIM;
                newMessage.message += bodyRotPosScaleText[i];

                mEnqueuedChatMessages.push_back(newMessage);
            }

            // add pose message(s)
            for (size_t i = 0; i < poseInfoText.size(); i++)
            {
                FSEnqueuedPoseMessage newMessage;
                newMessage.mRecipient   = it->first;
                newMessage.message    = std::string(POSER_MESSAGE_START_POSES);
                newMessage.message += DELIM;
                newMessage.message += changedAvId;
                newMessage.message += DELIM;
                newMessage.message += poseInfoText[i];

                mEnqueuedChatMessages.push_back(newMessage);
            }
        }
    }

    mAvatarIdToMessageType.clear();
}

void FSPoserCollab::enqueuePoserChatMessage(LLVOAvatar* avatarWhosePoseChanged, E_CollabPoseChangeType changeType)
{
    if (!avatarWhosePoseChanged)
        return;

    E_CollabPoseChangeType currentType = mAvatarIdToMessageType[avatarWhosePoseChanged->getID()];

    if (changeType > currentType)
        mAvatarIdToMessageType[avatarWhosePoseChanged->getID()] = changeType;

    mTimeLastUpdateMessageArrived = std::chrono::system_clock::now();
}

void FSPoserCollab::stopPosingMyAvatar(bool quittingPoser)
{
    for (auto it = sAvatarIdToCollabState.begin(); it != sAvatarIdToCollabState.end(); it++)
    {
        LLVOAvatar* avatar = getAvatarByUuid(it->first);
        if (!avatar)
            continue;

        if (it->second >= COLLAB_THEY_ASKED_ME)
        {
            sendMessage(avatar, std::string(POSER_MESSAGE_STOP_POSING) + std::string(DELIM) + "Stopped");
            it->second = COLLAB_PERM_ENDED;

            if (!mFloaterPoserCallback.empty())
                    mFloaterPoserCallback(it->first);
        }
    }

    if (quittingPoser)
    {
        sAvatarIdToCollabState.clear();
        sAvatarIdToPosingStateReloadTries.clear();
    }
}

void FSPoserCollab::processPoserMessage(LLUUID senderId, std::string chatMessage)
{
    if (chatMessage.empty())
        return;
    if (chatMessage.length() > 2 * POSER_MESSAGE_LENGTH)
        return;
    if (!mPoserAnimator)
        return;

    try
    {
        std::vector<std::string> result = tokenizeChatString(chatMessage, std::string(DELIM)[0]);
        if (result.size() < 3)
            return;

        LLUUID avatarIdMessageWantsToPose;
        if (!LLUUID::parseUUID(result[2], &avatarIdMessageWantsToPose))
            avatarIdMessageWantsToPose = senderId;

        bool messageIsFromThemAndAboutThem = avatarIdMessageWantsToPose == senderId;
        bool messageIsAboutMe              = avatarIdMessageWantsToPose == gAgentID;
        bool senderCanChangeMyPose =
            sAvatarIdToCollabState[senderId] == COLLAB_THEY_POSE_ME || sAvatarIdToCollabState[senderId] == COLLAB_POSE_EACH_OTHER;

        if (!messageIsFromThemAndAboutThem && !(messageIsAboutMe && senderCanChangeMyPose))
            return;

        LLVOAvatar* avatarToPose = getAvatarByUuid(avatarIdMessageWantsToPose);
        if (!avatarToPose)
            return;

        std::string messageType = result[1];

        if (messageType == std::string(POSER_MESSAGE_START_BODY))
            processBodyMessage(result, avatarToPose);
        else if (messageType == std::string(POSER_MESSAGE_START_POSES))
            processPlayingPosesMessage(result, avatarToPose);
        else if (messageType == std::string(POSER_MESSAGE_STOP_POSING))
            processStopMessage(avatarToPose);
        else if (messageType == std::string(POSER_MESSAGE_PERMISSION))
            processCollabStateMessage(result[2], avatarToPose);
    }
    catch (const std::exception& e)
    {
        LL_WARNS("Posing") << "Exception caught trying to parse poser message from sender ID: " << senderId.asString() << e.what() << LL_ENDL;
    }
}

void FSPoserCollab::processStopMessage(LLVOAvatar* avatar)
{
    if (!avatar)
        return;

    mPoserAnimator->stopPosingAvatar(avatar);
    sAvatarIdToCollabState[avatar->getID()] = COLLAB_PERM_ENDED;
    if (!mFloaterPoserCallback.empty())
        mFloaterPoserCallback(avatar->getID());
}

void FSPoserCollab::sendMessage(LLVOAvatar* recipient, std::string message) const
{
    if (!recipient || message.empty())
        return;

    std::string msg = std::string(POSER_MESSAGE_PREFIX);
    msg.append(message);

    const LLUUID          idSession  = gIMMgr->computeSessionID(IM_NOTHING_SPECIAL, recipient->getID());
    const LLRelationship* pBuddyInfo = LLAvatarTracker::instance().getBuddyInfo(recipient->getID());
    std::string           strAgentName;
    LLAgentUI::buildFullname(strAgentName);

    pack_instant_message(gMessageSystem,
                         gAgent.getID(),
                         false,
                         gAgent.getSessionID(),
                         recipient->getID(),
                         strAgentName.c_str(),
                         msg.c_str(),
                         ((!pBuddyInfo) || (pBuddyInfo->isOnline())) ? IM_ONLINE : IM_OFFLINE,
                         IM_NOTHING_SPECIAL,
                         idSession);

    gAgent.sendReliableMessage();
}

std::vector<std::string> FSPoserCollab::getRotPosScaleDiffAsText(LLVOAvatar* avatar)
{
    std::vector<std::string> result;
    if (!avatar || !mPoserAnimator)
        return result;

    if (!mPoserAnimator->isPosingAvatar(avatar))
        return result;

    LLVector3   rotation, position, scale;
    bool        baseRotationIsZero, jointIsMirrored;
    int         jointNumber;

    std::string line;
    for (const FSPoserAnimator::FSPoserJoint& pj : mPoserAnimator->PoserJoints)
    {
        bool posingThisJoint = mPoserAnimator->isPosingAvatarJoint(avatar, pj);
        if (!posingThisJoint)
            continue;

        bool jointShouldUpdateChat = mPoserAnimator->hasJointBeenChanged(avatar, pj);
        if (!jointShouldUpdateChat)
            continue;

        if (!mPoserAnimator->tryGetJointNumber(avatar, pj, jointNumber))
            continue;

        if (!mPoserAnimator->tryGetJointSaveVectors(avatar, pj, &rotation, &position, &scale, &baseRotationIsZero))
            continue;

        jointIsMirrored = mPoserAnimator->getRotationIsMirrored(avatar, pj);

        if (!line.empty())
            line.append(DELIM);

        line.append(encodeJointToString(jointNumber, jointIsMirrored, baseRotationIsZero, rotation, position, scale));

        if (line.size() < POSER_MESSAGE_LENGTH)
            continue;

        result.push_back(line);
        line = "";
    }

    if (line.size() > 0)
        result.push_back(line);

    return result;
}

std::vector<std::string> FSPoserCollab::getPoseInfoAsText(LLVOAvatar* avatar)
{
    std::vector<std::string> result;
    if (!avatar || !mPoserAnimator)
        return result;

    if (!mPoserAnimator->isPosingAvatar(avatar))
        return result;

    LLSD record;
    mPoserAnimator->savePosingState(avatar, true, &record);

    std::string line;
    for (auto itr = record.beginMap(); itr != record.endMap(); ++itr)
    {
        std::string const& name = itr->first;
        LLSD const& control_map = itr->second;

        if (!name.starts_with("poseState"))
            continue;

        if (!line.empty())
            line += DELIM;

        if (control_map.has("animationId"))
            line += control_map["animationId"].asString();

        line += DELIM;

        if (control_map.has("lastUpdateTime"))
            line += control_map["lastUpdateTime"].asString();

        line += DELIM;

        if (control_map.has("jointNamesAnimated"))
            line += control_map["jointNamesAnimated"].asString();

        line += DELIM;

        if (control_map.has("captureOrder"))
            line += control_map["captureOrder"].asString();

        if (line.size() < POSER_MESSAGE_LENGTH)
            continue;

        result.push_back(line);
        line = "";
    }

    if (line.size() > 0)
        result.push_back(line);

    return result;
}

std::string FSPoserCollab::encodeJointToString(int jointNumber, bool isMirrored, bool baseIsZero, const LLVector3 rotation, const LLVector3 position, const LLVector3 scale)
{
    std::string result = "";

    int encodedBools     = isMirrored ? 1 : 0 + baseIsZero ? 2 : 0;
    F32 jointNumAndBools = jointNumber * 0.01f + encodedBools * 0.001f; // joint (range 0..150ish) in units, tenths, hundredths, bools in thou

    result += f32ToTwoBytes(jointNumAndBools); // two chars

    bool rotZero = rotation == LLVector3::zero;
    bool posZero = position == LLVector3::zero;
    bool scaZero = scale == LLVector3::zero;

    if (rotZero && posZero && scaZero)
    {
        result.append(VECTOR_ALL_ZEROS); // encode all zeros as one char, total 3 chars
        return result;
    }

    // or add 6 more chars for rot : 8 chars
    result.append(vectorToSixBytes(rotation));

    if (posZero && scaZero) // no adding zeros
        return result;

    if (!posZero || !scaZero) // when !scaZero we have to add pos
        result.append(vectorToSixBytes(position)); // optional add pos makes 14 chars

    if (!scaZero)
        result.append(vectorToSixBytes(scale));  // optional add scale makes 20 chars

    return result;
}

std::string FSPoserCollab::vectorToSixBytes(const LLVector3 vector)
{
    std::string result = "";
    result.append(f32ToTwoBytes(vector[VX]));
    result.append(f32ToTwoBytes(vector[VY]));
    result.append(f32ToTwoBytes(vector[VZ]));

    return result;
}

std::string FSPoserCollab::f32ToTwoBytes(const F32 number)
{
    std::string result = "";
    S32 roundedNumberTimes1000 = ll_round((llclamp(number * 1000.f, -3200.f, 3199.f)));
    roundedNumberTimes1000 += 3200;

    result += char(roundedNumberTimes1000 / 80 + int('.')); // the upper; int('~') - int('.') = 80
    result += char(roundedNumberTimes1000 % 80 + int('.')); // 80 * 80 = 6400, provides a gamut to encode ints -3200..3199; reasonable for +/-PI

    return result;
}

bool FSPoserCollab::tryDecodeJointFromString(std::string token, int& jointNumber, bool& isMirrored, bool& baseIsZero, LLVector3& rotation, LLVector3& position, LLVector3& scale)
{
    if (token.empty())
        return false;

    if (token.size() > 1)
    {
        F32 numberAndBools = twoBytesToF32(token[0], token[1]);
        numberAndBools *= 100.f;
        jointNumber = ll_round(numberAndBools);

        numberAndBools *= 10.f;
        int encodedBools = ll_round(numberAndBools) % 10;

        if (encodedBools > 3)
            encodedBools -= 4; // unused 3rd bool; in case needed in future
        baseIsZero       = encodedBools > 1;
        if (baseIsZero)
            encodedBools -= 2;
        isMirrored = encodedBools > 0;
    }

    if (token.size() == 3 && token[2] == '%')
    {
        rotation = LLVector3::zero;
        position = LLVector3::zero;
        scale    = LLVector3::zero;

        return true;
    }

    if (token.size() == 8) // with rotation only
    {
        rotation = sixBytesToVector(token.substr(2, 6));
        position = LLVector3::zero;
        scale    = LLVector3::zero;

        return true;
    }

    if (token.size() == 14) // rotation and position
    {
        rotation = sixBytesToVector(token.substr(2, 6));
        position = sixBytesToVector(token.substr(8, 6));
        scale    = LLVector3::zero;

        return true;
    }

    if (token.size() == 20) // with 3 vectors
    {
        rotation = sixBytesToVector(token.substr(2, 6));
        position = sixBytesToVector(token.substr(8, 6));
        scale    = sixBytesToVector(token.substr(14, 6));

        return true;
    }

    return false;
}

LLVector3 FSPoserCollab::sixBytesToVector(const std::string token)
{
    LLVector3 vec3;
    if (token.size() != 6)
        return vec3;

    vec3[VX] = twoBytesToF32(token[0], token[1]);
    vec3[VY] = twoBytesToF32(token[2], token[3]);
    vec3[VZ] = twoBytesToF32(token[4], token[5]);

    return vec3;
}

F32 FSPoserCollab::twoBytesToF32(char upper, char lower)
{
    if (upper > '~' || lower > '~' || upper < '.' || lower < '.')
        return 0.f;

    int roundedNumberTimes1000 = (int(upper) - int('.')) * 80 + int(lower) - int('.');
    roundedNumberTimes1000 -= 3200;

    return roundedNumberTimes1000 / 1000.f;
}

void FSPoserCollab::processInstantMessage(const LLUUID& idSender, const std::string& strMessage)
{
    if (!gPoserCollaborator)
        return;
    if (idSender.isNull())
        return;
    if (strMessage.empty())
        return;

    if (strMessage.find(std::string(POSER_MESSAGE_PREFIX)) != 0)
        return; // the chat must start with our header

    gPoserCollaborator->processPoserMessage(idSender, strMessage);
}

bool FSPoserCollab::isInstantMessageForPoser(const std::string& strMessage)
{
    if (strMessage.empty())
        return false;

    if (strMessage.length() <= POSER_MESSAGE_PREFIX.length())
        return false;

    for (int i = 0; i < POSER_MESSAGE_PREFIX.length(); i++)
        if (strMessage[i] != POSER_MESSAGE_PREFIX[i])
            return false;

    return true;
}

void FSPoserCollab::processBodyMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar)
{
    if (!avatar || !mPoserAnimator)
        return;

    size_t      size = mesageTokens.size();
    LLVector3   rotation, position, scale;
    std::string token;
    bool        zeroBaseRot, mirrored;
    int         jointNumber;

    for (size_t index = 3; index < size; index++)
    {
        token = mesageTokens[index];
        if (token.empty())
            continue;

        if (!tryDecodeJointFromString(token, jointNumber, mirrored, zeroBaseRot, rotation, position, scale))
            continue;

        const FSPoserAnimator::FSPoserJoint* poserJoint = mPoserAnimator->getPoserJointByNumber(avatar, jointNumber);
        if (!poserJoint)
            continue;

        mPoserAnimator->loadJointRotation(avatar, poserJoint, zeroBaseRot, rotation);
        mPoserAnimator->loadJointPosition(avatar, poserJoint, true, position);
        mPoserAnimator->loadJointScale(avatar, poserJoint, true, scale);

        mPoserAnimator->setRotationIsMirrored(avatar, *poserJoint, mirrored);
    }
}

void FSPoserCollab::processPlayingPosesMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar)
{
    size_t size       = mesageTokens.size();
    bool   needUpdate = false;
    LLUUID animId;
    LLSD   saveRecord;
    int    animNumber = 0;

    for (size_t index = 3; index < size - 1; index++)
    {
        if (mesageTokens[index].empty())
            continue;

        if (index + 3 >= size)
            break;

       if (!LLUUID::parseUUID(mesageTokens[index], &animId))
           continue;

        std::string uniqueAnimId                         = "poseState" + std::to_string(animNumber++);
        (saveRecord)[uniqueAnimId]["animationId"]        = mesageTokens[index];
        (saveRecord)[uniqueAnimId]["lastUpdateTime"]     = mesageTokens[index + 1];
        (saveRecord)[uniqueAnimId]["jointNamesAnimated"] = mesageTokens[index + 2];
        (saveRecord)[uniqueAnimId]["captureOrder"]       = mesageTokens[index + 3];

        index += 2;
    }

    sAvatarIdToPosingStateReloadTries[avatar->getID()] = mPoserAnimator->loadPosingState(avatar, false, saveRecord) ? 0 : 5;
}

bool FSPoserCollab::tryParseInt(std::string token, int* number)
{
    try
    {
        *number = std::stoi(token);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

LLVOAvatar* FSPoserCollab::getAvatarByUuid(const LLUUID& avatarToFind) const
{
    if (avatarToFind.isNull())
        return nullptr;

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        if (avatarToFind != character->getID())
            continue;

        LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(character);
        return avatar;
    }

    return nullptr;
}

std::vector<std::string> FSPoserCollab::tokenizeChatString(const std::string& s, char delim)
{
    std::vector<std::string> result;
    std::stringstream        ss(s);
    std::string              item;

    while (getline(ss, item, delim))
        result.push_back(item);

    return result;
}

FSPoseChatTimer::FSPoseChatTimer(FSPoserCollab::callback_t callback) : LLEventTimer(0.1f), mCallback(callback)
{
}

/// <summary>
/// We want to throttle IM output similar to llInstantMessage: one per two seconds (script sleep time).
/// We also want to avoid the feeling of making more lag: make an edit and not sending it for 2 seconds.
/// </summary>
/// <returns>False.</returns>
bool FSPoseChatTimer::tick()
{
    if (mCallback.empty())
        return false;

    mTicksSinceLastMessage++;
    if (mTicksSinceLastMessage < 19)
        return false;

    if (mTicksSinceLastMessage > 20)
        mTicksSinceLastMessage = 20;

    if (mCallback())
        mTicksSinceLastMessage = 0;

    return false;
}

#include "llvoavatar.h"
#include "fsposercollab.h"
#include "llagent.h"
#include "llinstantmessage.h"
#include "llimview.h"
#include "llagentui.h"

constexpr std::string_view POSER_MESSAGE_PREFIX = "#POSER:"; // preamble on all poser messages.
constexpr std::string_view POSER_MESSAGE_START_BODY = "#SBOD"; // token indicating message has joint rot/pos/scale info
constexpr std::string_view POSER_MESSAGE_START_POSES = "#SPOS"; // token indicating message has playing animation payload
constexpr std::string_view POSER_MESSAGE_STOP_POSING = "#STOP"; // token indicating sender is stopping posing
constexpr std::string_view POSER_MESSAGE_PERMISSION = "#PERM"; // token indicating message has a permission payload
constexpr size_t POSER_MESSAGE_LENGTH = 950;

FSPoserCollab* gPoserCollaborator = NULL;
std::map<LLUUID, E_CollabState> FSPoserCollab::sAvatarIdToCollabState;

FSPoserCollab::FSPoserCollab(FSPoserAnimator* poserAnimator, FSPoserCollab::callback_t1 callback)
{
    mPoserAnimator        = poserAnimator;
    mPoseChatTimer        = new FSPoseChatTimer(boost::bind(&FSPoserCollab::onTimedChatMessage, this));
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

void FSPoserCollab::onTimedChatMessage()
{
    if (!gPoserCollaborator)
        return;

    processEnqueuedMessages();
    if (mEnqueuedChatMessages.size() < 1)
        return;

    FSEnqueuedPoseMessage nextMessageToSend = mEnqueuedChatMessages[0];
    mEnqueuedChatMessages.pop_front();

    LLVOAvatar* avatarToMessage = getAvatarByUuid(nextMessageToSend.mRecipient);
    if (!avatarToMessage)
        return;

    sendMessage(avatarToMessage, nextMessageToSend.message);
}

void FSPoserCollab::updateCollabPermission(LLVOAvatar* avatarToUpdate, E_CollabState stateToSupply)
{
    if (!avatarToUpdate)
        return;

    updateLocalCollabState(avatarToUpdate, stateToSupply);

    sendMessage(avatarToUpdate, std::string(POSER_MESSAGE_PERMISSION) + ":" + std::to_string(stateToSupply));
}

void FSPoserCollab::updateLocalCollabState(std::string newState, LLVOAvatar* avatar)
{
    if (newState.empty())
        return;

    int stateValue;
    if (!tryParseInt(newState, &stateValue))
        return;

    if (stateValue < 1 || stateValue >= COLLAB_LAST)
        return;

    E_CollabState state = static_cast<E_CollabState>(stateValue);
    if (state == COLLAB_I_ASKED_THEM) // if they sent us 'I asked them', on receipt that means 'they asked me'
        state = COLLAB_THEY_ASKED_ME;

    if (state == COLLAB_THEY_POSE_ME) // if they sent us 'They may pose me', on receipt that means 'i can pose them'
        state = COLLAB_I_POSE_THEM;

    updateLocalCollabState(avatar, state);
}

void FSPoserCollab::updateLocalCollabState(LLVOAvatar* avatar, E_CollabState newState)
{
    if (!avatar)
        return;

    LLUUID        avatarWhoseStateChanged = avatar->getID();
    E_CollabState currentState            = sAvatarIdToCollabState[avatarWhoseStateChanged];

    if (newState == COLLAB_I_ASKED_THEM && currentState == COLLAB_THEY_ASKED_ME ||
        newState == COLLAB_THEY_ASKED_ME && currentState == COLLAB_I_ASKED_THEM) // once we have asked each other, permission is granted
        sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_PERM_GRANTED;
    else if (newState == COLLAB_PERM_GRANTED && currentState == COLLAB_POSE_EACH_OTHER) // if we are posing each other, and I recind your perm to pose me, I can still pose you
        sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_I_POSE_THEM;
    else if (newState == COLLAB_THEY_POSE_ME && currentState == COLLAB_I_POSE_THEM) // if I am already posing you, and I give your perm to pose me, we pose each other
        sAvatarIdToCollabState[avatarWhoseStateChanged] = COLLAB_POSE_EACH_OTHER;
    else
        sAvatarIdToCollabState[avatarWhoseStateChanged] = newState;

    if (!mFloaterPoserCallback.empty())
        mFloaterPoserCallback(avatarWhoseStateChanged);
}

E_CollabState FSPoserCollab::getCollabLocalState(LLVOAvatar* avatar)
{
    if (!avatar)
        return COLLAB_NONE;

    return sAvatarIdToCollabState[avatar->getID()];
}

void FSPoserCollab::processEnqueuedMessages()
{
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
                newMessage.message    = std::string(POSER_MESSAGE_START_BODY) + ":" + changedAvId + ":" + bodyRotPosScaleText[i];

                mEnqueuedChatMessages.push_back(newMessage);
            }

            // add pose message(s)
            for (size_t i = 0; i < poseInfoText.size(); i++)
            {
                FSEnqueuedPoseMessage newMessage;
                newMessage.mRecipient     = it->first;
                newMessage.message    = std::string(POSER_MESSAGE_START_POSES) + ":" + changedAvId + ":" + poseInfoText[i];

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
}

void FSPoserCollab::stopPosingAvatar()
{
    for (auto it = sAvatarIdToCollabState.begin(); it != sAvatarIdToCollabState.end(); it++)
    {
        LLVOAvatar* avatar = getAvatarByUuid(it->first);
        if (!avatar)
            continue;
        if (it->second < COLLAB_PERM_GRANTED)
            continue;

        sendMessage(avatar, std::string(POSER_MESSAGE_STOP_POSING) + ":Stopped");
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

    std::vector<std::string> result = tokenizeChatString(chatMessage, ':');
    if (result.size() < 3)
        return;

    LLUUID avatarIdMessageWantsToPose;
    if (!LLUUID::parseUUID(result[2], &avatarIdMessageWantsToPose))
        avatarIdMessageWantsToPose = senderId;

    bool messageIsFromThemAndAboutThem = avatarIdMessageWantsToPose == senderId;
    bool messageIsAboutMe = avatarIdMessageWantsToPose == gAgentID;
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
        processPoseMessage(result, avatarToPose);
    else if (messageType == std::string(POSER_MESSAGE_STOP_POSING))
        processStopMessage(avatarToPose);
    else if (messageType == std::string(POSER_MESSAGE_PERMISSION))
        updateLocalCollabState(result[2], avatarToPose);
}

void FSPoserCollab::processStopMessage(LLVOAvatar* avatar)
{
    mPoserAnimator->stopPosingAvatar(avatar);
    updateLocalCollabState(avatar, COLLAB_PERM_ENDED);
}

void FSPoserCollab::sendMessage(LLVOAvatar* recipient, std::string message) const
{
    if (!recipient || message.empty())
        return;

    std::string msg = std::string(POSER_MESSAGE_PREFIX) + message;

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

    LLVector3 rotation, position, scale;
    bool      baseRotationIsZero;

    std::string line;
    for (const FSPoserAnimator::FSPoserJoint& pj : mPoserAnimator->PoserJoints)
    {
        bool posingThisJoint = mPoserAnimator->isPosingAvatarJoint(avatar, pj);
        if (!posingThisJoint)
            continue;

        bool jointShouldUpdateChat = mPoserAnimator->hasJointBeenChanged(avatar, pj);
        if (!jointShouldUpdateChat)
            continue;

        if (!mPoserAnimator->tryGetJointSaveVectors(avatar, pj, &rotation, &position, &scale, &baseRotationIsZero))
            continue;

        if (!line.empty())
            line += ":";

        line += pj.jointName() + ":";
        line += baseRotationIsZero ? "true:" : "false:";
        line += getChatStringForVector(rotation) + ":";
        line += getChatStringForVector(position) + ":";
        line += getChatStringForVector(scale);

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
            line += ":";

        if (control_map.has("animationId"))
            line += control_map["animationId"].asString();

        line += ":";

        if (control_map.has("lastUpdateTime"))
            line += control_map["lastUpdateTime"].asString();

        line += ":";

        if (control_map.has("jointNamesAnimated"))
            line += control_map["jointNamesAnimated"].asString();

        line += ":";

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

std::string FSPoserCollab::getChatStringForVector(const LLVector3& val)
{
    std::string result = "";
    if (llabs(val[VX]) >= 0.001f)
        result += std::format("{:.3f}", val[VX]);
    else
        result += "0";

    result += " ";
    if (llabs(val[VY]) >= 0.001f)
        result += std::format("{:.3f}", val[VY]);
    else
        result += "0";

    result += " ";
    if (llabs(val[VZ]) >= 0.001f)
        result += std::format("{:.3f}", val[VZ]);
    else
        result += "0";

    return result;
}

void FSPoserCollab::processInstantMessage(const LLUUID& idSender, const std::string& strMessage)
{
    if (!gPoserCollaborator)
        return;
    if (idSender.isNull())
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
    LLVector3   vec3;
    std::string token;
    bool        zeroBaseRot;
    for (size_t index = 3; index < size - 1; index++)
    {
        token = mesageTokens[index];
        if (token.empty())
            continue;

        const FSPoserAnimator::FSPoserJoint* poserJoint = mPoserAnimator->getPoserJointByName(token);
        if (!poserJoint)
            continue;

        if (index + 4 >= size)
            break;

        token = mesageTokens[index + 1];
        zeroBaseRot = token == "true";

        token = mesageTokens[index + 2];
        if (LLVector3::parseVector3(token, &vec3))
            mPoserAnimator->loadJointRotation(avatar, poserJoint, zeroBaseRot, vec3);

        token = mesageTokens[index + 3];
        if (LLVector3::parseVector3(token, &vec3))
            mPoserAnimator->loadJointPosition(avatar, poserJoint, true, vec3);

        token = mesageTokens[index + 4];
        if (LLVector3::parseVector3(token, &vec3))
            mPoserAnimator->loadJointScale(avatar, poserJoint, true, vec3);

        index += 4;
    }
}

void FSPoserCollab::processPoseMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar)
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

    mPoserAnimator->loadPosingState(avatar, saveRecord);
}

bool FSPoserCollab::tryParseFloat(std::string token, F32* number)
{
    char* ending;
    *number = strtof(token.c_str(), &ending);

    return *ending == 0;
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

FSPoseChatTimer::FSPoseChatTimer(FSPoserCollab::callback_t callback) : LLEventTimer(2.0f), mCallback(callback)
{
}

bool FSPoseChatTimer::tick()
{
    if (mCallback.empty())
        return false;

    mCallback();

    return false;
}

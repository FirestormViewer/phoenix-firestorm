/**
 * @file fsposercollab.h
 * @brief a class for sharing the posing information from one viewer to another via local-chat and/or IM.
 *
 * $LicenseInfo:firstyear=2025&license=viewerlgpl$
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
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_FSPoserCollab_H
#define LL_FSPoserCollab_H

#include "fsposeranimator.h"
#include <deque>

/// <summary>
/// Describes the columns of the avatars scroll-list.
/// Order is important: note increasing permission state allowing use of >= etc.
/// </summary>
typedef enum E_CollabState
{
    COLLAB_NONE             = 0, // default state
    COLLAB_PERM_ENDED       = 1, // stop posing, etc.
    COLLAB_PERM_DENIED      = 2, // we are telling them no
    COLLAB_I_ASKED_THEM     = 3,
    COLLAB_THEY_ASKED_ME    = 4,
    COLLAB_PERM_GRANTED     = 5,
    COLLAB_THEY_POSE_ME     = 6,
    COLLAB_I_POSE_THEM      = 7,
    COLLAB_POSE_EACH_OTHER  = 8,
    COLLAB_LAST
} E_CollabState;

/// <summary>
/// Defines the type of pose change message to enqueue.
/// </summary>
typedef enum E_CollabPoseChangeType
{
    POSECHANGE_NONE = 0,
    POSECHANGE_BONE = 1,
    POSECHANGE_BASE = 2,
    POSECHANGE_BOTH = 3,
} E_CollabPoseChangeType;

class FSPoseChatTimer;

class FSPoserCollab
{
public:
    typedef boost::function<void()> callback_t;
    typedef boost::function<void(const LLUUID)> callback_t1;
    FSPoserCollab(FSPoserAnimator* poserAnimator, FSPoserCollab::callback_t1 callback);
    virtual ~FSPoserCollab();

public:
    static FSPoserCollab* initiateCollaborator(FSPoserAnimator* poserAnimator, FSPoserCollab::callback_t1 callback);
    static void destroyCollaborator();

    /// <summary>
    /// Parser for poser-intended instant messages.
    /// </summary>
    /// <param name="chat">The chat record.</param>
    static void processInstantMessage(const LLUUID& idSender, const std::string& strMessage);
    static bool isInstantMessageForPoser(const std::string& strMessage);

    /// <summary>
    /// Makes an ASYNC request to the supplied avatar to pose together.
    /// </summary>
    /// <param name="avatarToUpdate">The avatar which is to be updated with this client's state.</param>
    /// <remarks>
    /// ASYNC meaning the message is not enqueued and sent when message throttle is ready; it is sent immediately.
    /// </remarks>
    void updateCollabPermission(LLVOAvatar* avatarToUpdate, E_CollabState stateToSupply);

    /// <summary>
    /// Gets the collaborator state for the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to query.</param>
    /// <returns>The local state we have documented.</returns>
    /// <remarks>
    /// Intended for UI queries.
    /// This doesn't query another avatar over the internet, just our local state.
    /// To change local state, updateCollabPermission(...) which will send & process messages.
    /// </remarks>
    E_CollabState getCollabLocalState(LLVOAvatar* avatar);

    /// <summary>
    /// Enqueues a message to be sent indicating this poser has changed the supplied avatar's pose.
    /// </summary>
    /// <param name="avatarWhosePoseChanged">The avatar whose pose was updated.</param>
    /// <param name="changeType">The type of change message that should be enqueued.</param>
    void enqueuePoserChatMessage(LLVOAvatar* avatarWhosePoseChanged, E_CollabPoseChangeType changeType);

    /// <summary>
    /// Sends an ASYNC message to everyone we have permitted to see us we have stopped posing.
    /// </summary>
    /// <remarks>
    /// ASYNC meaning the message is not enqueued and sent when message throttle is ready; it is sent immediately.
    /// </remarks>
    void stopPosingAvatar();

private:

    /// <summary>
    /// Class representing an enqueued message.
    /// </summary>
    class FSEnqueuedPoseMessage
    {
    public:
        /// <summary>
        /// The ID of whom the message should be sent to.
        /// </summary>
        LLUUID mRecipient;

        /// <summary>
        /// The message that will be sent to the Recipient.
        /// </summary>
        std::string message;

        /// <summary>
        /// The type of change message(s) that should be sent.
        /// </summary>
        E_CollabPoseChangeType mMessageType;
    };

    /// <summary>
    /// Sends the supplied message to local chat.
    /// </summary>
    /// <param name="recipient">The avatar we are sending a message to.</param>
    /// <param name="message">The message payload.</param>
    /// <remarks>
    /// Message may contain several 'sections' headed by different tokens.
    /// </remarks>
    void sendMessage(LLVOAvatar* recipient, std::string message) const;

    /// <summary>
    /// Tokenizes the chat string with the supplied delimiter.
    /// </summary>
    /// <param name="s">The string to tokenize.</param>
    /// <param name="delim">The delimiter to split by.</param>
    /// <returns>The collection of strings recovered.</returns>
    static std::vector<std::string> tokenizeChatString(const std::string& s, char delim);

    /// <summary>
    /// Gets the rotation/position/scale information for the supplied avatar, and containerizes it to send via chat.
    /// </summary>
    /// <param name="avatar">The avatar to get posing information for.</param>
    /// <returns>A collection of strings to send via chat; an empty collection if there is an issue.</returns>
    std::vector<std::string> getRotPosScaleDiffAsText(LLVOAvatar* avatar);

    /// <summary>
    /// Gets the playing-poses information for the supplied avatar, and containerizes it to send via chat.
    /// </summary>
    /// <param name="avatar">The avatar to get posing information for.</param>
    /// <returns>A collection of strings to send via chat; an empty collection if there is an issue.</returns>
    std::vector<std::string> getPoseInfoAsText(LLVOAvatar* avatar);

    /// <summary>
    /// Stringifies the supplied vector, shortening it somewhat for efficient transmission.
    /// </summary>
    /// <param name="val">The vector to stringify.</param>
    /// <returns>A string of the supplied vector</returns>
    std::string getChatStringForVector(const LLVector3& val);

    /// <summary>
    /// Parses the supplied chat message, verifying it is for poser and parsing it for posing information for the supplied avatar ID.
    /// </summary>
    /// <param name="avatarId">The avatar ID the chat message may have posing information for.</param>
    /// <param name="chatMessage">The posing information to parse.</param>
    /// <remarks>
    /// Messages are semi-colon separated tokens.
    /// The first 3 tokens are always the same
    /// Token 1 = POSER_MESSAGE_PREFIX - used at a high level to determine if the IM is intended for poser
    /// Token 2 = message type, eg POSER_MESSAGE_START_BODY
    /// Token 3 = LLUUID for whom the message is about, could be blank, implying it is for the sender
    /// Followed by the tokens for the message, whose parser is chosen by token 2
    /// </remarks>
    void processPoserMessage(LLUUID avatarId, std::string chatMessage);

    /// <summary>
    /// Processes the supplied message tokens into joint rotations/positions/scales for the supplied avatar.
    /// </summary>
    /// <param name="mesageTokens">The message tokens containing the joint/pos/rot/scales.</param>
    /// <param name="avatar">The avatar to which the tokens should be applied.</param>
    void processBodyMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar);

    /// <summary>
    /// Processes the supplied message tokens into animations which are playing for the supplied avatar.
    /// </summary>
    /// <param name="mesageTokens">The message tokens containing the animation data.</param>
    /// <param name="avatar">The avatar to which the tokens should be applied.</param>
    void processPoseMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar);

    /// <summary>
    /// Processes the command to stop posing the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar which told us to stop posing them.</param>
    void processStopMessage(LLVOAvatar* avatar);

    /// <summary>
    /// Gets a detectable avatar by its UUID.
    /// </summary>
    /// <param name="avatarToFind">The ID of the avatar to find.</param>
    /// <returns>The avatar, if found, otherwise nullptr.</returns>
    LLVOAvatar* getAvatarByUuid(const LLUUID& avatarToFind) const;

    /// <summary>
    /// Call periodically to formulate a response to all of the enqueuePoserChatMessage
    /// </summary>
    void processEnqueuedMessages();

    /// <summary>
    /// The event handler which polls chat messages.
    /// </summary>
    /// <remarks>
    /// A timer runs this every second or so, and messages are emitted if any have accumulated.
    /// This effectively throttles the sending of messages from this clients poser to others.
    /// </remarks>
    void onTimedChatMessage();

    bool tryParseFloat(std::string token, F32* number);
    bool tryParseInt(std::string token, int* number);

    /// <summary>
    /// A pointer to the instance of the class which does the business of manipulating the avatars.
    /// </summary>
    FSPoserAnimator* mPoserAnimator = nullptr;

    /// <summary>
    /// The collection of messages which need sending.
    /// </summary>
    std::deque<FSEnqueuedPoseMessage> mEnqueuedChatMessages;

    /// <summary>
    /// The callback to invoke informing the UI something has changed.
    /// </summary>
    FSPoserCollab::callback_t1 mFloaterPoserCallback;

    /// <summary>
    /// The chat timer, throttling chat message emmission.
    /// </summary>
    FSPoseChatTimer* mPoseChatTimer = nullptr;

    /// <summary>
    /// Updates sAvatarIdToCollabState with the appropriate state.
    /// </summary>
    /// <param name="avatar">That avatar to update.</param>
    /// <param name="newState">The new collab state.</param>
    void updateLocalCollabState(LLVOAvatar* avatar, E_CollabState newState);

    /// <summary>
    /// Updates sAvatarIdToCollabState with the appropriate state.
    /// </summary>
    /// <param name="avatar">That avatar to update.</param>
    /// <param name="newState">The new collab state.</param>
    void updateLocalCollabState(std::string newState, LLVOAvatar* avatar);

    /// <summary>
    /// The map collecting the kinds of poser messages that should be sent.
    /// </summary>
    std::map<LLUUID, E_CollabPoseChangeType> mAvatarIdToMessageType;

    /// <summary>
    /// Maintains the mapping of avatar IDs to posing state
    /// </summary>
    static std::map<LLUUID, E_CollabState> sAvatarIdToCollabState;
};

/// <summary>
/// A class for timing the emmision of chat messages
/// </summary>
class FSPoseChatTimer : public LLEventTimer
{
public:
    FSPoseChatTimer(FSPoserCollab::callback_t callback);
    /*virtual*/ bool tick();

private:
    FSPoserCollab::callback_t mCallback;
};

/// <summary>
/// The instance of this, referenced by the static chat event-handler.
/// </summary>
extern FSPoserCollab* gPoserCollaborator;


#endif // LL_FSPoserCollab_H

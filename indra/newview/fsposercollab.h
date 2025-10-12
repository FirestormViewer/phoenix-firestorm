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
    COLLAB_PERM_ENDED       = 1, // user has rescinded our permission to view them.
    COLLAB_PERM_DENIED      = 2, // we are telling them no
    COLLAB_I_ASKED_THEM     = 3, // we have asked them to share
    COLLAB_THEY_ASKED_ME    = 4, // They have asked us to share
    COLLAB_PARTY_MODE       = 5, // We are posing them in party mode
    COLLAB_PERM_GRANTED     = 6, // there has been an offer to share and an acceptance
    COLLAB_THEY_POSE_ME     = 7, // we tell them they can pose us
    COLLAB_I_POSE_THEM      = 8, // we have been told we can pose them
    COLLAB_POSE_EACH_OTHER  = 9, // we both have allowed the other to pose us
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
    typedef boost::function<bool()> callback_t;
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

    /// <summary>
    /// Examines the supplied message and determines if it is for the poser.
    /// </summary>
    /// <param name="strMessage">The message to examine.</param>
    /// <returns>True if the message appears intended for poser, otherwise false.</returns>
    static bool isInstantMessageForPoser(const std::string& strMessage);

    /// <summary>
    /// Updates the posing permission for the supplied avatar and sends an ASYNC message to them.
    /// </summary>
    /// <param name="avatarToUpdate">The avatar which is to be updated with this client's state.</param>
    /// <remarks>
    /// This method is for use by this client: by our UI to specify our offers and acceptances.
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
    /// <remarks>
    /// This implementation is like a 2-stage filter.
    /// First stage concatenates all of the (many) calls a UI-control callback could make over a few seconds into one call.
    /// The second stage time-filters the UI calls, so only one call is acted on.
    ///
    /// The first stage is implemented by a map: whose avatar changed and what kind of change.
    /// Repeat calls to the same avatar keep updating the same member of the map.
    /// This is common for spinners, sliders and trackpad: they make a call every frame.
    /// The second stage is implemented by a time-stamp: the process to compose and send an IM
    /// only happens once the UI events have stopped for about 0.1 seconds.
    /// </remarks>
    void enqueuePoserChatMessage(LLVOAvatar* avatarWhosePoseChanged, E_CollabPoseChangeType changeType);

    /// <summary>
    /// Sends an ASYNC message to everyone we have permitted to see us we have stopped posing.
    /// </summary>
    /// <param name="quittingPoser">If we are quitting, and should discard all held permissions.</param>
    /// <remarks>
    /// ASYNC meaning the message is not enqueued and sent when message throttle is ready; it is sent immediately.
    /// </remarks>
    void stopPosingMyAvatar(bool quittingPoser);

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
    };

    /// <summary>
    /// Sends the supplied message as an instant message to the supplied recipient.
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
    /// Encodes the supplied joint properties to a variable-length string of chars.
    /// </summary>
    /// <param name="jointNumber">The number of the joint to encode, needs to be between -320 and 319.</param>
    /// <param name="isMirrored">Whether the supplied joint is mirrored.</param>
    /// <param name="baseIsZero">Whether the supplied joint is base-zeroed.</param>
    /// <param name="rotation">The rotation of the joint; values should be between -3.200 and 3.199; values outside range are clamped. More than 3 decimals in precision is lost.</param>
    /// <param name="position">The position of the joint; values should be between -3.200 and 3.199; values outside range are clamped. More than 3 decimals in precision is lost.</param>
    /// <param name="scale">The scale of the joint; values should be between -3.200 and 3.199; values outside range are clamped. More than 3 decimals in precision is lost.</param>
    /// <returns></returns>
    std::string encodeJointToString(int jointNumber, bool isMirrored, bool baseIsZero, const LLVector3 rotation, const LLVector3 position, const LLVector3 scale);
    bool tryDecodeJointFromString(std::string token, int& jointNumber, bool& isMirrored, bool& baseIsZero, LLVector3& rotation, LLVector3& position,
                                  LLVector3& scale);

    /// <summary>
    /// Encodes the supplied vector to 6 ASCII printable chars.
    /// </summary>
    /// <param name="vector">The vector to encode, whose X/Y/Z values are in range -3.200 and 3.199 or clamped.</param>
    /// <returns>A string encoding the supplied vector.</returns>
    std::string vectorToSixBytes(const LLVector3 vector);
    LLVector3   sixBytesToVector(const std::string token);

    /// <summary>
    /// Encodes an F32 of range -3.200 to 3.199 into two bytes preserving three decimal places precision and no accuracy loss.
    /// </summary>
    /// <param name="number">The number to encode.</param>
    /// <returns>A string of two ASCII-printable bytes with chars ranging '.' to '~'.</returns>
    std::string f32ToTwoBytes(const F32 number);
    F32         twoBytesToF32(char upper, char lower);

    /// <summary>
    /// Parses the supplied chat message, verifying it is for poser and parsing it for posing information for the supplied avatar ID.
    /// </summary>
    /// <param name="avatarId">The avatar ID the chat message may have posing information for.</param>
    /// <param name="chatMessage">The posing information to parse.</param>
    /// <remarks>
    /// Messages are colon separated tokens.
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
    void processPlayingPosesMessage(std::vector<std::string> mesageTokens, LLVOAvatar* avatar);

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
    /// Checks whether all avatar IDs are still online, and if not, preens and informs UI.
    /// </summary>
    void checkOnlineStatuses();

    /// <summary>
    /// Checks whether all playing poses have been loaded, and if not, attempts a reload.
    /// </summary>
    void checkAnyPosesNeedReloading();

    /// <summary>
    /// The event handler which polls chat messages.
    /// </summary>
    /// <remarks>
    /// A timer runs this every second or so, and messages are emitted if any have accumulated.
    /// This effectively throttles the sending of messages from this clients poser to others.
    /// </remarks>
    /// <returns>True if a message was sent, otherwise false.</returns>
    bool onTimedCollabEvent();

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
    /// Updates Collab State for the supplied avatar with the appropriate state from the supplied avatar.
    /// </summary>
    /// <param name="avatar">That avatar to update.</param>
    /// <param name="newState">The new collab state.</param>
    void processCollabStateMessage(std::string newState, LLVOAvatar* avatar);

    /// <summary>
    /// The map collecting the kinds of poser messages that should be sent.
    /// </summary>
    std::map<LLUUID, E_CollabPoseChangeType> mAvatarIdToMessageType;

    /// <summary>
    /// The time when the last message to update arrived, normally the last time someone edited a bone.
    /// </summary>
    std::chrono::system_clock::time_point mTimeLastUpdateMessageArrived = std::chrono::system_clock::now();

    /// <summary>
    /// The time when the last attempt load playing-poses was made, see: sAvatarIdToPosingStateReloadTries.
    /// </summary>
    std::chrono::system_clock::time_point mTimeLastAttemptedPoseLoad = std::chrono::system_clock::now();

    /// <summary>
    /// Maintains the mapping of avatar IDs to Collaborator permission state.
    /// </summary>
    static std::map<LLUUID, E_CollabState> sAvatarIdToCollabState;

    /// <summary>
    /// Maintains the mapping of avatar IDs to playing-pose load state; true meaning their poses need re-loading.
    /// </summary>
    /// <remarks>
    /// When a message arrives from them telling us what LLPosingMotions to load for an avatar and at what time,
    /// it can be the case this client needs to fetch the LLPosingMotion asset, load and play it.
    /// This process can take several seconds to complete, and during, the pose presented is incomplete.
    /// Sometimes it loads first try: for example when they have been sitting on a couch and we are updating the time of that motion.
    /// Thus we track the load-outcome, and use it to periodically retry loading.
    /// We will retry loading several times, and eventually give up.
    /// </remarks>
    static std::map<LLUUID, int> sAvatarIdToPosingStateReloadTries;
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

    /// <summary>
    /// Counts ticks, an IM can go out if there are 20 ticks.
    /// </summary>
    int mTicksSinceLastMessage = 0;
};

/// <summary>
/// The instance of this, referenced by the static chat event-handler.
/// </summary>
extern FSPoserCollab* gPoserCollaborator;


#endif // LL_FSPoserCollab_H

/**
 * @file bsworldinteraction.h
 * @brief Remote object interaction and scoped script-dialog forwarding.
 */

#ifndef BS_WORLD_INTERACTION_H
#define BS_WORLD_INTERACTION_H

#include "blazingstorm/remote/bsremoteprotocol.h"
#include "lluuid.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace BlazingStorm
{
    struct ForwardedScriptDialog
    {
        std::string dialogId;
        std::string sourceObjectId;
        std::string rootObjectId;
        std::string objectName;
        std::string message;
        std::vector<std::string> buttons;
    };

    class WorldInteraction final
    {
    public:
        static WorldInteraction& instance();

        // Controller-side actions based on the controller viewer's current pick.
        bool requestSitFromCurrentPick();
        bool requestTouchFromCurrentPick();
        bool requestStand();
        // Native Subject touches also establish the permitted dialog scope.
        void observeTouch(const LLUUID& object_id);

        // Subject-side execution of validated remote commands.
        bool sitAsSubject(const RemoteCommand& command);
        bool touchAsSubject(const RemoteCommand& command);
        bool standAsSubject();
        bool replyToScriptDialog(const std::string& dialog_id, S32 button_index);

        // Subject-side capture of native blue script dialogs. Returns an
        // internal dialog id only when the dialog belongs to the current
        // controlled object/linkset and forwarding is permitted.
        std::string captureScriptDialog(const LLUUID& source_object_id,
                                        const std::string& object_name,
                                        const std::string& message,
                                        S32 chat_channel,
                                        const std::string& sender,
                                        const std::vector<std::string>& buttons);
        void linkSubjectNotification(const std::string& dialog_id,
                                     const LLUUID& notification_id);
        void subjectDialogAnswered(const std::string& dialog_id);

        std::vector<ForwardedScriptDialog> takeDialogsToForward();
        std::vector<std::string> takeDialogClosures();

        // Controller-side rendering of a mirrored blue menu.
        void showForwardedDialog(const ForwardedScriptDialog& dialog);
        void closeForwardedDialog(const std::string& dialog_id);

        void reset();

    private:
        struct PendingDialog
        {
            ForwardedScriptDialog forwarded;
            std::string sender;
            S32 chatChannel = 0;
            LLUUID subjectNotificationId;
            std::chrono::steady_clock::time_point expiresAt{};
        };

        WorldInteraction() = default;

        bool sendControllerCommand(RemoteCommandType type,
                                   const std::string& target_id = {},
                                   const std::string& text = {});
        void noteInteraction(const LLUUID& source_id, const LLUUID& root_id);
        LLUUID rootIdForObject(const LLUUID& object_id) const;
        LLUUID currentSeatRootId() const;
        bool dialogBelongsToControlledObject(const LLUUID& source_id,
                                             const LLUUID& root_id);
        void pruneExpiredDialogs();

        static std::string serializeSitOffset(F32 x, F32 y, F32 z);
        static bool parseSitOffset(const std::string& text, F32& x, F32& y, F32& z);
        static std::string serializeTouchPick();
        static bool parseTouchPick(const std::string& text, std::vector<F32>& values);

        LLUUID mLastInteractionSource;
        LLUUID mLastInteractionRoot;
        std::chrono::steady_clock::time_point mLastInteractionExpires{};

        std::map<std::string, PendingDialog> mPendingDialogs;
        std::vector<ForwardedScriptDialog> mOutboundDialogs;
        std::vector<std::string> mOutboundClosures;
        std::map<std::string, LLUUID> mControllerNotifications;
    };
}

#endif // BS_WORLD_INTERACTION_H

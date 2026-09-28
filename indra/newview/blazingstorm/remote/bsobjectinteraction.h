/**
 * @file bsobjectinteraction.h
 * @brief Remote sit, touch and scripted-object dialog interaction.
 */

#ifndef BS_OBJECT_INTERACTION_H
#define BS_OBJECT_INTERACTION_H

#include "lluuid.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

class LLPickInfo;
class LLViewerObject;

namespace BlazingStorm
{
    class ObjectInteraction final
    {
    public:
        static ObjectInteraction& instance();

        // Controller-side actions using the object currently under the
        // controller's right-click/pie-menu pick.
        bool requestSitSelected();
        bool requestTouchSelected();
        bool requestStand();

        // Subject-side execution of validated remote commands.
        bool performSit(const std::string& object_id, const std::string& pick_data);
        bool performTouch(const std::string& object_id, const std::string& pick_data);
        bool performStand();

        // Subject-side scripted-dialog forwarding. Returns an empty string
        // when the dialog is unrelated to the current controlled interaction.
        std::string registerScriptDialog(const LLUUID& object_id,
                                         const std::string& object_name,
                                         const std::string& message,
                                         S32 chat_channel,
                                         const std::string& sender_host,
                                         const std::vector<std::string>& buttons);
        void bindSubjectNotification(const std::string& dialog_id,
                                     const LLUUID& notification_id);
        void onSubjectDialogResponded(const std::string& dialog_id);
        bool replyScriptDialog(const std::string& dialog_id, S32 button_index);

        // Controller-side mirrored blue-menu handling.
        void showControllerDialog(const std::string& dialog_id,
                                  const std::string& object_id,
                                  const std::string& object_name,
                                  const std::string& message,
                                  const std::vector<std::string>& buttons);
        void closeControllerDialog(const std::string& dialog_id);
        bool sendControllerDialogReply(const std::string& dialog_id, S32 button_index);
        void dismissControllerDialog(const std::string& dialog_id);

        void reset();

    private:
        ObjectInteraction() = default;

        struct PendingDialog
        {
            LLUUID objectId;
            LLUUID rootId;
            std::string objectName;
            std::string message;
            S32 chatChannel = 0;
            std::string senderHost;
            std::vector<std::string> buttons;
            LLUUID subjectNotificationId;
        };

        static std::string serializePick(const LLPickInfo& pick);
        static bool deserializePick(const std::string& data,
                                    const LLUUID& object_id,
                                    LLPickInfo& pick);
        static LLUUID rootIdForObject(LLViewerObject* object);

        bool sendSelectedCommand(int command_type);
        void rememberInteraction(LLViewerObject* object);
        bool isDialogRelevant(const LLUUID& object_id, LLUUID& root_id) const;

        LLUUID mRecentObjectId;
        LLUUID mRecentRootId;
        std::chrono::steady_clock::time_point mRecentInteractionDeadline{};

        std::map<std::string, PendingDialog> mPendingDialogs;
        std::map<std::string, LLUUID> mControllerDialogNotifications;
    };
}

#endif // BS_OBJECT_INTERACTION_H

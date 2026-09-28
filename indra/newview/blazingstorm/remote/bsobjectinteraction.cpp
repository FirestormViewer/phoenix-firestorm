/**
 * @file bsobjectinteraction.cpp
 * @brief Remote sit, touch and scripted-object dialog interaction.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsobjectinteraction.h"

#include "blazingstorm/remote/bslocaltransport.h"
#include "blazingstorm/remote/bsremotecontroller.h"
#include "blazingstorm/remote/bsremoteprotocol.h"
#include "blazingstorm/remote/bsremotesession.h"

#include "fscommon.h"
#include "llagent.h"
#include "llmessage.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llselectmgr.h"
#include "lltoolgrab.h"
#include "lltoolpie.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewermenu.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "rlvactions.h"

#include <iomanip>
#include <sstream>
#include <utility>

namespace
{
    bool callback_blazing_remote_script_dialog(const LLSD& notification,
                                                const LLSD& response)
    {
        const std::string dialog_id =
            notification["payload"]["blazing_dialog_id"].asString();
        const S32 option =
            LLNotification::getSelectedOption(notification, response);

        if (option >= 0)
        {
            BlazingStorm::ObjectInteraction::instance()
                .sendControllerDialogReply(dialog_id, option);
        }
        else
        {
            BlazingStorm::ObjectInteraction::instance()
                .dismissControllerDialog(dialog_id);
        }
        return false;
    }

    static LLNotificationFunctorRegistration
        sBlazingRemoteScriptDialogRegistration(
            "BlazingStormRemoteScriptDialog",
            callback_blazing_remote_script_dialog);
}

namespace BlazingStorm
{
    ObjectInteraction& ObjectInteraction::instance()
    {
        static ObjectInteraction interaction;
        return interaction;
    }

    LLUUID ObjectInteraction::rootIdForObject(LLViewerObject* object)
    {
        if (!object)
        {
            return LLUUID::null;
        }

        LLViewerObject* root = object->getRootEdit();
        return root ? root->getID() : object->getID();
    }

    std::string ObjectInteraction::serializePick(const LLPickInfo& pick)
    {
        std::ostringstream out;
        out << std::setprecision(9)
            << pick.mObjectFace << ' '
            << pick.mObjectOffset.mV[VX] << ' '
            << pick.mObjectOffset.mV[VY] << ' '
            << pick.mObjectOffset.mV[VZ] << ' '
            << pick.mUVCoords.mV[VX] << ' '
            << pick.mUVCoords.mV[VY] << ' '
            << pick.mSTCoords.mV[VX] << ' '
            << pick.mSTCoords.mV[VY] << ' '
            << pick.mIntersection.mV[VX] << ' '
            << pick.mIntersection.mV[VY] << ' '
            << pick.mIntersection.mV[VZ] << ' '
            << pick.mNormal.mV[VX] << ' '
            << pick.mNormal.mV[VY] << ' '
            << pick.mNormal.mV[VZ] << ' '
            << pick.mBinormal.mV[VX] << ' '
            << pick.mBinormal.mV[VY] << ' '
            << pick.mBinormal.mV[VZ];
        return out.str();
    }

    bool ObjectInteraction::deserializePick(const std::string& data,
                                            const LLUUID& object_id,
                                            LLPickInfo& pick)
    {
        std::istringstream in(data);
        in >> pick.mObjectFace
           >> pick.mObjectOffset.mV[VX]
           >> pick.mObjectOffset.mV[VY]
           >> pick.mObjectOffset.mV[VZ]
           >> pick.mUVCoords.mV[VX]
           >> pick.mUVCoords.mV[VY]
           >> pick.mSTCoords.mV[VX]
           >> pick.mSTCoords.mV[VY]
           >> pick.mIntersection.mV[VX]
           >> pick.mIntersection.mV[VY]
           >> pick.mIntersection.mV[VZ]
           >> pick.mNormal.mV[VX]
           >> pick.mNormal.mV[VY]
           >> pick.mNormal.mV[VZ]
           >> pick.mBinormal.mV[VX]
           >> pick.mBinormal.mV[VY]
           >> pick.mBinormal.mV[VZ];

        if (in.fail())
        {
            return false;
        }

        pick.mObjectID = object_id;
        pick.mPickType = LLPickInfo::PICK_OBJECT;
        return true;
    }

    bool ObjectInteraction::sendSelectedCommand(int command_type)
    {
        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();

        if (!controller.isActive() || !transport.isPaired())
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No active controller session.");
            return false;
        }

        const LLPickInfo& pick = LLToolPie::getInstance()->getPick();
        LLViewerObject* object = pick.getObject();
        if (!object)
        {
            object =
                LLSelectMgr::getInstance()->getSelection()->getPrimaryObject();
        }

        if (!object)
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No target object is selected.");
            return false;
        }

        LLPickInfo effective_pick = pick;
        if (effective_pick.mObjectID.isNull())
        {
            effective_pick.mObjectID = object->getID();
            effective_pick.mPickType = LLPickInfo::PICK_OBJECT;
            effective_pick.mObjectOffset.setVec(0.f, 0.f, 0.f);
        }

        const auto type = static_cast<RemoteCommandType>(command_type);
        RemoteCommand command =
            controller.makeCommand(
                type,
                serializePick(effective_pick),
                object->getID().asString());

        if (!transport.sendCommand(command))
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] " + transport.lastStatus());
            return false;
        }
        return true;
    }

    bool ObjectInteraction::requestSitSelected()
    {
        return sendSelectedCommand(static_cast<int>(RemoteCommandType::SitObject));
    }

    bool ObjectInteraction::requestTouchSelected()
    {
        return sendSelectedCommand(static_cast<int>(RemoteCommandType::TouchObject));
    }

    bool ObjectInteraction::requestStand()
    {
        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (!controller.isActive() || !transport.isPaired())
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No active controller session.");
            return false;
        }

        return transport.sendCommand(
            controller.makeCommand(RemoteCommandType::Stand));
    }

    void ObjectInteraction::rememberInteraction(LLViewerObject* object)
    {
        if (!object)
        {
            return;
        }

        mRecentObjectId = object->getID();
        mRecentRootId = rootIdForObject(object);
        mRecentInteractionDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(30);
    }

    bool ObjectInteraction::performSit(const std::string& object_id,
                                       const std::string& pick_data)
    {
        auto& session = RemoteSession::instance();
        if (!session.hasPermission(RemotePermission::SitStand))
        {
            return false;
        }

        LLUUID id(object_id);
        LLViewerObject* object = gObjectList.findObject(id);
        if (id.isNull() || !object)
        {
            return false;
        }

        LLPickInfo pick;
        if (!deserializePick(pick_data, id, pick))
        {
            return false;
        }

        rememberInteraction(object);

        // Preserve the exact clicked prim and object-relative offset. This is
        // important for furniture/linksets that expose multiple sit targets.
        handle_object_sit(object, pick.mObjectOffset);
        return true;
    }

    bool ObjectInteraction::performTouch(const std::string& object_id,
                                         const std::string& pick_data)
    {
        auto& session = RemoteSession::instance();
        if (!session.hasPermission(RemotePermission::Touch))
        {
            return false;
        }

        LLUUID id(object_id);
        LLViewerObject* object = gObjectList.findObject(id);
        if (id.isNull() || !object || !object->getRegion())
        {
            return false;
        }

        LLPickInfo pick;
        if (!deserializePick(pick_data, id, pick))
        {
            return false;
        }

        if (RlvActions::isRlvEnabled()
            && !RlvActions::canTouch(object, pick.mObjectOffset))
        {
            return false;
        }

        rememberInteraction(object);

        send_ObjectGrab_message(object, pick, LLVector3::zero);
        send_ObjectDeGrab_message(object, pick);
        return true;
    }

    bool ObjectInteraction::performStand()
    {
        auto& session = RemoteSession::instance();
        if (!session.hasPermission(RemotePermission::SitStand))
        {
            return false;
        }

        if (RlvActions::isRlvEnabled() && !RlvActions::canStand())
        {
            return false;
        }

        gAgent.standUp();
        return true;
    }

    bool ObjectInteraction::isDialogRelevant(const LLUUID& object_id,
                                              LLUUID& root_id) const
    {
        auto& session = RemoteSession::instance();
        if (!session.isActive()
            || !session.hasPermission(RemotePermission::ScriptDialogs))
        {
            return false;
        }

        LLViewerObject* dialog_object = gObjectList.findObject(object_id);
        root_id = rootIdForObject(dialog_object);
        if (root_id.isNull())
        {
            root_id = object_id;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now < mRecentInteractionDeadline
            && (object_id == mRecentObjectId
                || root_id == mRecentRootId))
        {
            return true;
        }

        // While seated, accept menus from any prim in the sitting linkset.
        // This covers furniture where the sit target lives on a child prim but
        // a root/main script owns the animation dialog (and vice versa).
        if (isAgentAvatarValid()
            && gAgentAvatarp->isSitting()
            && gAgentAvatarp->getParent())
        {
            LLViewerObject* seated_parent =
                static_cast<LLViewerObject*>(gAgentAvatarp->getParent());
            const LLUUID seated_root = rootIdForObject(seated_parent);
            if (object_id == seated_parent->getID()
                || root_id == seated_root)
            {
                return true;
            }
        }

        return false;
    }

    std::string ObjectInteraction::registerScriptDialog(
        const LLUUID& object_id,
        const std::string& object_name,
        const std::string& message,
        S32 chat_channel,
        const std::string& sender_host,
        const std::vector<std::string>& buttons)
    {
        LLUUID root_id;
        if (!isDialogRelevant(object_id, root_id)
            || buttons.empty())
        {
            return {};
        }

        LLUUID dialog_uuid;
        dialog_uuid.generate();
        const std::string dialog_id = dialog_uuid.asString();

        PendingDialog dialog;
        dialog.objectId = object_id;
        dialog.rootId = root_id;
        dialog.objectName = object_name;
        dialog.message = message;
        dialog.chatChannel = chat_channel;
        dialog.senderHost = sender_host;
        dialog.buttons = buttons;
        mPendingDialogs[dialog_id] = dialog;

        if (!LocalTransport::instance().sendScriptDialogEvent(
                dialog_id,
                object_id.asString(),
                object_name,
                message,
                buttons))
        {
            mPendingDialogs.erase(dialog_id);
            return {};
        }

        return dialog_id;
    }

    void ObjectInteraction::bindSubjectNotification(
        const std::string& dialog_id,
        const LLUUID& notification_id)
    {
        const auto found = mPendingDialogs.find(dialog_id);
        if (found != mPendingDialogs.end())
        {
            found->second.subjectNotificationId = notification_id;
        }
    }

    void ObjectInteraction::onSubjectDialogResponded(
        const std::string& dialog_id)
    {
        const auto found = mPendingDialogs.find(dialog_id);
        if (found == mPendingDialogs.end())
        {
            return;
        }

        LocalTransport::instance().sendScriptDialogClosed(dialog_id);
        mPendingDialogs.erase(found);
    }

    bool ObjectInteraction::replyScriptDialog(const std::string& dialog_id,
                                              S32 button_index)
    {
        auto& session = RemoteSession::instance();
        if (!session.hasPermission(RemotePermission::ScriptDialogs))
        {
            return false;
        }

        const auto found = mPendingDialogs.find(dialog_id);
        if (found == mPendingDialogs.end()
            || button_index < 0
            || button_index >= static_cast<S32>(found->second.buttons.size()))
        {
            return false;
        }

        const PendingDialog dialog = found->second;

        LLMessageSystem* msg = gMessageSystem;
        msg->newMessage("ScriptDialogReply");
        msg->nextBlock("AgentData");
        msg->addUUID("AgentID", gAgent.getID());
        msg->addUUID("SessionID", gAgent.getSessionID());
        msg->nextBlock("Data");
        msg->addUUID("ObjectID", dialog.objectId);
        msg->addS32("ChatChannel", dialog.chatChannel);
        msg->addS32("ButtonIndex", button_index);
        msg->addString("ButtonLabel", dialog.buttons[button_index]);
        msg->sendReliable(LLHost(dialog.senderHost));

        if (dialog.subjectNotificationId.notNull())
        {
            LLNotificationPtr notification =
                LLNotificationsUtil::find(dialog.subjectNotificationId);
            if (notification)
            {
                LLNotificationsUtil::cancel(notification);
            }
        }

        LocalTransport::instance().sendScriptDialogClosed(dialog_id);
        mPendingDialogs.erase(found);
        return true;
    }

    void ObjectInteraction::showControllerDialog(
        const std::string& dialog_id,
        const std::string& object_id,
        const std::string& object_name,
        const std::string& message,
        const std::vector<std::string>& buttons)
    {
        closeControllerDialog(dialog_id);

        LLNotificationForm form;
        for (const std::string& button : buttons)
        {
            form.addElement("button", button);
        }

        LLSD args;
        args["TITLE"] = object_name;
        args["MESSAGE"] = message;

        LLSD payload;
        payload["blazing_dialog_id"] = dialog_id;
        payload["object_id"] = object_id;

        LLNotificationPtr notification =
            LLNotifications::instance().add(
                LLNotification::Params("BlazingStormRemoteScriptDialog")
                    .substitutions(args)
                    .payload(payload)
                    .form_elements(form.asLLSD()));

        if (notification)
        {
            mControllerDialogNotifications[dialog_id] =
                notification->getID();
        }
    }

    void ObjectInteraction::closeControllerDialog(
        const std::string& dialog_id)
    {
        const auto found =
            mControllerDialogNotifications.find(dialog_id);
        if (found == mControllerDialogNotifications.end())
        {
            return;
        }

        LLNotificationPtr notification =
            LLNotificationsUtil::find(found->second);
        if (notification)
        {
            LLNotificationsUtil::cancel(notification);
        }
        mControllerDialogNotifications.erase(found);
    }

    bool ObjectInteraction::sendControllerDialogReply(
        const std::string& dialog_id,
        S32 button_index)
    {
        mControllerDialogNotifications.erase(dialog_id);

        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (!controller.isActive() || !transport.isPaired())
        {
            return false;
        }

        return transport.sendCommand(
            controller.makeCommand(
                RemoteCommandType::ScriptDialogReply,
                std::to_string(button_index),
                dialog_id));
    }

    void ObjectInteraction::dismissControllerDialog(
        const std::string& dialog_id)
    {
        mControllerDialogNotifications.erase(dialog_id);
    }

    void ObjectInteraction::reset()
    {
        for (const auto& pair : mControllerDialogNotifications)
        {
            LLNotificationPtr notification =
                LLNotificationsUtil::find(pair.second);
            if (notification)
            {
                LLNotificationsUtil::cancel(notification);
            }
        }

        for (const auto& pair : mPendingDialogs)
        {
            if (pair.second.subjectNotificationId.notNull())
            {
                LLNotificationPtr notification =
                    LLNotificationsUtil::find(
                        pair.second.subjectNotificationId);
                if (notification)
                {
                    LLNotificationsUtil::cancel(notification);
                }
            }
        }

        mControllerDialogNotifications.clear();
        mPendingDialogs.clear();
        mRecentObjectId.setNull();
        mRecentRootId.setNull();
        mRecentInteractionDeadline =
            std::chrono::steady_clock::time_point{};
    }
}

/**
 * @file bsworldinteraction.cpp
 * @brief Remote object interaction and scoped script-dialog forwarding.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsworldinteraction.h"

#include "blazingstorm/remote/bslocaltransport.h"
#include "blazingstorm/remote/bsremotecontroller.h"
#include "blazingstorm/remote/bsremotesession.h"

#include "fscommon.h"
#include "llagent.h"
#include "llmessage.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "lltoolgrab.h"
#include "lltoolpie.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvhandler.h"

#include <iomanip>
#include <sstream>

namespace BlazingStorm
{
    WorldInteraction& WorldInteraction::instance()
    {
        static WorldInteraction interaction;
        return interaction;
    }

    bool WorldInteraction::sendControllerCommand(RemoteCommandType type,
                                                 const std::string& target_id,
                                                 const std::string& text)
    {
        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (!controller.isActive() || !transport.isPaired())
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No active controller session.");
            return false;
        }

        return transport.sendCommand(controller.makeCommand(type, text, target_id));
    }

    std::string WorldInteraction::serializeSitOffset(F32 x, F32 y, F32 z)
    {
        std::ostringstream out;
        out << std::setprecision(9) << x << ',' << y << ',' << z;
        return out.str();
    }

    bool WorldInteraction::parseSitOffset(const std::string& text,
                                          F32& x, F32& y, F32& z)
    {
        std::vector<F32> values;
        std::istringstream in(text);
        std::string token;
        try
        {
            while (std::getline(in, token, ','))
            {
                values.push_back((F32)std::stof(token));
            }
        }
        catch (...)
        {
            return false;
        }

        if (values.size() != 3)
        {
            return false;
        }

        x = values[0];
        y = values[1];
        z = values[2];
        return true;
    }

    std::string WorldInteraction::serializeTouchPick()
    {
        const LLPickInfo& pick = LLToolPie::getInstance()->getPick();

        std::ostringstream out;
        out << std::setprecision(9)
            << pick.mObjectOffset.mV[VX] << ','
            << pick.mObjectOffset.mV[VY] << ','
            << pick.mObjectOffset.mV[VZ] << ','
            << (F32)pick.mObjectFace << ','
            << pick.mUVCoords.mV[VX] << ','
            << pick.mUVCoords.mV[VY] << ','
            << pick.mSTCoords.mV[VX] << ','
            << pick.mSTCoords.mV[VY] << ','
            << pick.mIntersection.mV[VX] << ','
            << pick.mIntersection.mV[VY] << ','
            << pick.mIntersection.mV[VZ] << ','
            << pick.mNormal.mV[VX] << ','
            << pick.mNormal.mV[VY] << ','
            << pick.mNormal.mV[VZ] << ','
            << pick.mBinormal.mV[VX] << ','
            << pick.mBinormal.mV[VY] << ','
            << pick.mBinormal.mV[VZ];
        return out.str();
    }

    bool WorldInteraction::parseTouchPick(const std::string& text,
                                          std::vector<F32>& values)
    {
        values.clear();
        std::istringstream in(text);
        std::string token;

        try
        {
            while (std::getline(in, token, ','))
            {
                values.push_back((F32)std::stof(token));
            }
        }
        catch (...)
        {
            values.clear();
            return false;
        }

        return values.size() == 17;
    }

    bool WorldInteraction::requestSitFromCurrentPick()
    {
        const LLPickInfo& pick = LLToolPie::getInstance()->getPick();
        LLViewerObject* object = pick.getObject();
        if (!object || object->getPCode() != LL_PCODE_VOLUME)
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No sittable object is under the current pick.");
            return false;
        }

        return sendControllerCommand(
            RemoteCommandType::SitObject,
            object->getID().asString(),
            serializeSitOffset(
                pick.mObjectOffset.mV[VX],
                pick.mObjectOffset.mV[VY],
                pick.mObjectOffset.mV[VZ]));
    }

    bool WorldInteraction::requestTouchFromCurrentPick()
    {
        const LLPickInfo& pick = LLToolPie::getInstance()->getPick();
        LLViewerObject* object = pick.getObject();
        if (!object)
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] No object is under the current pick.");
            return false;
        }

        return sendControllerCommand(
            RemoteCommandType::TouchObject,
            object->getID().asString(),
            serializeTouchPick());
    }

    bool WorldInteraction::requestStand()
    {
        return sendControllerCommand(RemoteCommandType::Stand);
    }

    LLUUID WorldInteraction::rootIdForObject(const LLUUID& object_id) const
    {
        LLViewerObject* object = gObjectList.findObject(object_id);
        if (!object)
        {
            return LLUUID::null;
        }

        LLViewerObject* root = object->getRootEdit();
        return root ? root->getID() : object->getID();
    }

    LLUUID WorldInteraction::currentSeatRootId() const
    {
        if (!isAgentAvatarValid() || !gAgentAvatarp->isSitting())
        {
            return LLUUID::null;
        }

        LLViewerObject* parent =
            static_cast<LLViewerObject*>(gAgentAvatarp->getParent());
        if (!parent)
        {
            return LLUUID::null;
        }

        LLViewerObject* root = parent->getRootEdit();
        return root ? root->getID() : parent->getID();
    }

    void WorldInteraction::noteInteraction(const LLUUID& source_id,
                                           const LLUUID& root_id)
    {
        mLastInteractionSource = source_id;
        mLastInteractionRoot = root_id;
        mLastInteractionExpires =
            std::chrono::steady_clock::now() + std::chrono::seconds(120);
    }

    bool WorldInteraction::sitAsSubject(const RemoteCommand& command)
    {
        LLUUID object_id(command.targetId);
        LLViewerObject* object = gObjectList.findObject(object_id);
        if (!object || !object->getRegion() || object->getPCode() != LL_PCODE_VOLUME)
        {
            return false;
        }

        F32 x = 0.f, y = 0.f, z = 0.f;
        if (!parseSitOffset(command.text, x, y, z))
        {
            return false;
        }

        const LLVector3 offset(x, y, z);
        if (RlvActions::isRlvEnabled() && !RlvActions::canSit(object, offset))
        {
            return false;
        }

        LLViewerObject* root = object->getRootEdit();
        noteInteraction(object->getID(), root ? root->getID() : object->getID());

        if (gRlvHandler.hasBehaviour(RLV_BHVR_STANDTP) && isAgentAvatarValid())
        {
            if (gAgentAvatarp->isSitting())
            {
                gAgent.standUp();
                return true;
            }
            gRlvHandler.setSitSource(gAgent.getPositionGlobal());
        }

        gMessageSystem->newMessageFast(_PREHASH_AgentRequestSit);
        gMessageSystem->nextBlockFast(_PREHASH_AgentData);
        gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
        gMessageSystem->nextBlockFast(_PREHASH_TargetObject);
        gMessageSystem->addUUIDFast(_PREHASH_TargetID, object->getID());
        gMessageSystem->addVector3Fast(_PREHASH_Offset, offset);
        object->getRegion()->sendReliableMessage();
        return true;
    }

    bool WorldInteraction::standAsSubject()
    {
        if (RlvActions::isRlvEnabled() && !RlvActions::canStand())
        {
            return false;
        }

        gAgent.standUp();
        return true;
    }

    bool WorldInteraction::touchAsSubject(const RemoteCommand& command)
    {
        LLUUID object_id(command.targetId);
        LLViewerObject* object = gObjectList.findObject(object_id);
        if (!object || !object->getRegion())
        {
            return false;
        }

        std::vector<F32> v;
        if (!parseTouchPick(command.text, v))
        {
            return false;
        }

        LLPickInfo pick;
        pick.mPickType = LLPickInfo::PICK_OBJECT;
        pick.mObjectID = object_id;
        pick.mObjectOffset.set(v[0], v[1], v[2]);
        pick.mObjectFace = (S32)v[3];
        pick.mUVCoords.set(v[4], v[5]);
        pick.mSTCoords.set(v[6], v[7]);
        pick.mIntersection.set(v[8], v[9], v[10]);
        pick.mNormal.set(v[11], v[12], v[13]);
        pick.mBinormal.set(v[14], v[15], v[16]);

        if (RlvActions::isRlvEnabled()
            && !RlvActions::canTouch(object, pick.mObjectOffset))
        {
            return false;
        }

        LLViewerObject* root = object->getRootEdit();
        noteInteraction(object->getID(), root ? root->getID() : object->getID());

        send_ObjectGrab_message(object, pick, LLVector3::zero);
        send_ObjectDeGrab_message(object, pick);
        return true;
    }

    bool WorldInteraction::dialogBelongsToControlledObject(
        const LLUUID& source_id,
        const LLUUID& root_id)
    {
        const auto now = std::chrono::steady_clock::now();

        if (now < mLastInteractionExpires)
        {
            if (source_id == mLastInteractionSource
                || source_id == mLastInteractionRoot
                || (root_id.notNull() && root_id == mLastInteractionRoot))
            {
                return true;
            }
        }

        const LLUUID seat_root = currentSeatRootId();
        if (seat_root.notNull())
        {
            return source_id == seat_root
                || (root_id.notNull() && root_id == seat_root);
        }

        return false;
    }

    void WorldInteraction::pruneExpiredDialogs()
    {
        const auto now = std::chrono::steady_clock::now();
        for (auto it = mPendingDialogs.begin(); it != mPendingDialogs.end(); )
        {
            if (now >= it->second.expiresAt)
            {
                mOutboundClosures.push_back(it->first);
                it = mPendingDialogs.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::string WorldInteraction::captureScriptDialog(
        const LLUUID& source_object_id,
        const std::string& object_name,
        const std::string& message,
        S32 chat_channel,
        const std::string& sender,
        const std::vector<std::string>& buttons)
    {
        auto& session = RemoteSession::instance();
        if (!session.isActive()
            || !session.hasPermission(RemotePermission::ScriptDialogs)
            || source_object_id.isNull()
            || buttons.empty())
        {
            return {};
        }

        pruneExpiredDialogs();

        const LLUUID root_id = rootIdForObject(source_object_id);
        if (!dialogBelongsToControlledObject(source_object_id, root_id))
        {
            return {};
        }

        LLUUID dialog_uuid;
        dialog_uuid.generate();

        PendingDialog pending;
        pending.forwarded.dialogId = dialog_uuid.asString();
        pending.forwarded.sourceObjectId = source_object_id.asString();
        pending.forwarded.rootObjectId = root_id.asString();
        pending.forwarded.objectName = object_name;
        pending.forwarded.message = message;
        pending.forwarded.buttons = buttons;
        pending.sender = sender;
        pending.chatChannel = chat_channel;
        pending.expiresAt =
            std::chrono::steady_clock::now() + std::chrono::minutes(2);

        mPendingDialogs[pending.forwarded.dialogId] = pending;
        mOutboundDialogs.push_back(pending.forwarded);
        return pending.forwarded.dialogId;
    }

    void WorldInteraction::linkSubjectNotification(
        const std::string& dialog_id,
        const LLUUID& notification_id)
    {
        auto found = mPendingDialogs.find(dialog_id);
        if (found != mPendingDialogs.end())
        {
            found->second.subjectNotificationId = notification_id;
        }
    }

    void WorldInteraction::subjectDialogAnswered(const std::string& dialog_id)
    {
        auto found = mPendingDialogs.find(dialog_id);
        if (found == mPendingDialogs.end())
        {
            return;
        }

        mOutboundClosures.push_back(dialog_id);
        mPendingDialogs.erase(found);
    }

    bool WorldInteraction::replyToScriptDialog(const std::string& dialog_id,
                                               S32 button_index)
    {
        pruneExpiredDialogs();

        auto& session = RemoteSession::instance();
        if (!session.hasPermission(RemotePermission::ScriptDialogs))
        {
            return false;
        }

        auto found = mPendingDialogs.find(dialog_id);
        if (found == mPendingDialogs.end())
        {
            return false;
        }

        PendingDialog pending = found->second;
        if (button_index < 0
            || button_index >= (S32)pending.forwarded.buttons.size()
            || pending.sender.empty())
        {
            return false;
        }

        if (RlvActions::isRlvEnabled()
            && pending.chatChannel == 0
            && RlvActions::hasBehaviour(RLV_BHVR_SENDCHAT))
        {
            return false;
        }

        LLMessageSystem* msg = gMessageSystem;
        msg->newMessage("ScriptDialogReply");
        msg->nextBlock("AgentData");
        msg->addUUID("AgentID", gAgent.getID());
        msg->addUUID("SessionID", gAgent.getSessionID());
        msg->nextBlock("Data");
        msg->addUUID("ObjectID", LLUUID(pending.forwarded.sourceObjectId));
        msg->addS32("ChatChannel", pending.chatChannel);
        msg->addS32("ButtonIndex", button_index);
        msg->addString("ButtonLabel", pending.forwarded.buttons[button_index]);
        msg->sendReliable(LLHost(pending.sender));

        if (pending.subjectNotificationId.notNull())
        {
            LLNotificationPtr notification =
                LLNotifications::instance().find(pending.subjectNotificationId);
            if (notification)
            {
                LLNotifications::instance().cancel(notification);
            }
        }

        mOutboundClosures.push_back(dialog_id);
        mPendingDialogs.erase(found);
        return true;
    }

    std::vector<ForwardedScriptDialog> WorldInteraction::takeDialogsToForward()
    {
        pruneExpiredDialogs();
        std::vector<ForwardedScriptDialog> result;
        result.swap(mOutboundDialogs);
        return result;
    }

    std::vector<std::string> WorldInteraction::takeDialogClosures()
    {
        std::vector<std::string> result;
        result.swap(mOutboundClosures);
        return result;
    }

    void WorldInteraction::showForwardedDialog(
        const ForwardedScriptDialog& dialog)
    {
        LLNotificationForm form;
        for (const auto& button : dialog.buttons)
        {
            form.addElement("button", button);
        }

        LLSD substitutions;
        substitutions["TITLE"] = dialog.objectName;
        substitutions["MESSAGE"] = dialog.message;

        LLSD payload;
        payload["dialog_id"] = dialog.dialogId;

        LLNotification::Params params("BlazingStormForwardedScriptDialog");
        params.substitutions(substitutions)
              .payload(payload)
              .form_elements(form.asLLSD());
        params.functor.function(
            [dialog_id = dialog.dialogId](const LLSD& notification,
                                          const LLSD& response)
            {
                const S32 option =
                    LLNotificationsUtil::getSelectedOption(notification, response);
                if (option >= 0)
                {
                    auto& controller = RemoteController::instance();
                    auto& transport = LocalTransport::instance();
                    if (controller.isActive() && transport.isPaired())
                    {
                        transport.sendCommand(
                            controller.makeCommand(
                                RemoteCommandType::DialogReply,
                                std::to_string(option),
                                dialog_id));
                    }
                }
            });

        LLNotificationPtr notification = LLNotifications::instance().add(params);
        if (notification)
        {
            mControllerNotifications[dialog.dialogId] = notification->getID();
        }
    }

    void WorldInteraction::closeForwardedDialog(const std::string& dialog_id)
    {
        auto found = mControllerNotifications.find(dialog_id);
        if (found == mControllerNotifications.end())
        {
            return;
        }

        LLNotificationPtr notification = LLNotifications::instance().find(found->second);
        if (notification)
        {
            LLNotifications::instance().cancel(notification);
        }
        mControllerNotifications.erase(found);
    }

    void WorldInteraction::reset()
    {
        for (const auto& pair : mControllerNotifications)
        {
            LLNotificationPtr notification =
                LLNotifications::instance().find(pair.second);
            if (notification)
            {
                LLNotifications::instance().cancel(notification);
            }
        }

        mControllerNotifications.clear();
        mPendingDialogs.clear();
        mOutboundDialogs.clear();
        mOutboundClosures.clear();
        mLastInteractionSource.setNull();
        mLastInteractionRoot.setNull();
        mLastInteractionExpires = {};
    }
}

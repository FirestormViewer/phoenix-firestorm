/**
 * @file bsremotefloater.cpp
 * @brief Blazing Storm possession/control test UI.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/ui/bsremotefloater.h"

#include "blazingstorm/remote/bslocaltransport.h"
#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotecontroller.h"
#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremoteprotocol.h"
#include "blazingstorm/remote/bsremotesession.h"

#include "fscommon.h"
#include "llagent.h"
#include "llagentui.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "lluuid.h"

namespace
{
    void report(const std::string& message)
    {
        FSCommon::report_to_nearby_chat("[Blazing Storm] " + message);
    }

    std::string transportRoleName(BlazingStorm::RemoteRole role)
    {
        switch (role)
        {
            case BlazingStorm::RemoteRole::Host: return "Subject / Host";
            case BlazingStorm::RemoteRole::Controller: return "Controller";
            default: return "Idle";
        }
    }
}

namespace BlazingStorm
{
    RemoteFloater::RemoteFloater(const LLSD& key)
        : LLFloater(key)
    {
    }

    bool RemoteFloater::postBuild()
    {
        getChild<LLButton>("start_host")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onStartHost(); });
        getChild<LLButton>("connect")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onConnect(); });
        getChild<LLButton>("accept")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onAccept(); });
        getChild<LLButton>("reject")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onReject(); });
        getChild<LLButton>("disconnect")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onDisconnect(); });
        getChild<LLButton>("emergency_release")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onEmergencyRelease(); });

        getChild<LLCheckBoxCtrl>("allow_controller_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onAllowControllerIM(); });
        getChild<LLCheckBoxCtrl>("allow_restrictions")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onAllowRestrictions(); });
        getChild<LLCheckBoxCtrl>("disable_local_movement")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onDisableLocalMovement(); });
        getChild<LLCheckBoxCtrl>("subject_restrict_chat")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSubjectRestrictChat(); });
        getChild<LLCheckBoxCtrl>("subject_restrict_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSubjectRestrictIM(); });

        getChild<LLButton>("remote_forward")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::MoveForward); });
        getChild<LLButton>("remote_back")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::MoveBackward); });
        getChild<LLButton>("remote_left")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::StrafeLeft); });
        getChild<LLButton>("remote_right")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::StrafeRight); });
        getChild<LLButton>("remote_turn_left")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::TurnLeft); });
        getChild<LLButton>("remote_turn_right")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::TurnRight); });
        getChild<LLButton>("remote_jump")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::Jump); });
        getChild<LLButton>("remote_stop")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::Stop); });
        getChild<LLButton>("remote_say")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteSay(); });
        getChild<LLButton>("remote_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteIM(); });
        getChild<LLCheckBoxCtrl>("remote_restrict_chat")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteRestrictChat(); });
        getChild<LLCheckBoxCtrl>("remote_restrict_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteRestrictIM(); });
        getChild<LLButton>("remote_release")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::EmergencyRelease); });

        refresh();
        return true;
    }

    void RemoteFloater::draw()
    {
        refresh();
        LLFloater::draw();
    }

    void RemoteFloater::refresh()
    {
        mRefreshing = true;

        auto& transport = LocalTransport::instance();
        auto& session = RemoteSession::instance();
        auto& controller = RemoteController::instance();

        std::string status = transportRoleName(transport.role());
        status += transport.isConnected() ? " | connected" : " | disconnected";
        status += transport.isPaired() ? " | paired" : " | not paired";
        if (!transport.lastStatus().empty())
        {
            status += "\n" + transport.lastStatus();
        }
        getChild<LLTextBox>("status")->setText(status);

        getChild<LLLineEditor>("pairing_code")->setText(transport.pairingCode());

        const bool is_host = transport.role() == RemoteRole::Host;
        const bool is_controller = transport.role() == RemoteRole::Controller;
        const bool subject_active = session.isActive();
        const bool controller_active = controller.isActive() && transport.isPaired();

        getChild<LLButton>("accept")->setEnabled(is_host && transport.hasPendingPairing());
        getChild<LLButton>("reject")->setEnabled(is_host && transport.hasPendingPairing());
        getChild<LLButton>("disconnect")->setEnabled(transport.role() != RemoteRole::None);
        getChild<LLButton>("emergency_release")->setEnabled(subject_active || is_host);

        auto* allow_im = getChild<LLCheckBoxCtrl>("allow_controller_im");
        allow_im->setEnabled(subject_active && is_host);
        allow_im->setValue(session.hasPermission(RemotePermission::InstantMessage));

        auto* allow_restrictions = getChild<LLCheckBoxCtrl>("allow_restrictions");
        allow_restrictions->setEnabled(subject_active && is_host);
        allow_restrictions->setValue(session.hasPermission(RemotePermission::ManageSubjectRestrictions));

        auto* disable_local = getChild<LLCheckBoxCtrl>("disable_local_movement");
        disable_local->setEnabled(subject_active && is_host);
        disable_local->setValue(session.subjectLocalMovementDisabled());

        auto* restrict_chat = getChild<LLCheckBoxCtrl>("subject_restrict_chat");
        restrict_chat->setEnabled(subject_active && is_host);
        restrict_chat->setValue(session.isSubjectRestricted(SubjectRestriction::NearbyChat));

        auto* restrict_im = getChild<LLCheckBoxCtrl>("subject_restrict_im");
        restrict_im->setEnabled(subject_active && is_host);
        restrict_im->setValue(session.isSubjectRestricted(SubjectRestriction::InstantMessage));

        const char* remote_buttons[] = {
            "remote_forward", "remote_back", "remote_left", "remote_right",
            "remote_turn_left", "remote_turn_right", "remote_jump", "remote_stop",
            "remote_say", "remote_im", "remote_release"
        };
        for (const char* name : remote_buttons)
        {
            getChild<LLButton>(name)->setEnabled(controller_active);
        }

        getChild<LLCheckBoxCtrl>("remote_restrict_chat")->setEnabled(controller_active);
        getChild<LLCheckBoxCtrl>("remote_restrict_im")->setEnabled(controller_active);
        getChild<LLLineEditor>("remote_say_text")->setEnabled(controller_active);
        getChild<LLLineEditor>("remote_im_target")->setEnabled(controller_active);
        getChild<LLLineEditor>("remote_im_text")->setEnabled(controller_active);

        mRefreshing = false;
    }

    void RemoteFloater::onStartHost()
    {
        auto& session = RemoteSession::instance();
        RemoteActions::instance().stopMovement();
        if (session.isActive())
        {
            session.emergencyRelease();
        }

        auto& transport = LocalTransport::instance();
        if (!transport.startHost())
        {
            report(transport.lastStatus());
        }
        refresh();
    }

    void RemoteFloater::onConnect()
    {
        const std::string code = getChild<LLLineEditor>("connect_code")->getText();
        if (code.empty())
        {
            report("Enter the subject viewer pairing code first.");
            return;
        }

        std::string controller_name;
        LLAgentUI::buildFullname(controller_name);

        auto& transport = LocalTransport::instance();
        if (!transport.connectController(code, gAgentID.asString(), controller_name))
        {
            report(transport.lastStatus());
        }
        refresh();
    }

    void RemoteFloater::onAccept()
    {
        auto& transport = LocalTransport::instance();
        if (!transport.acceptPending())
        {
            report(transport.lastStatus());
        }
        refresh();
    }

    void RemoteFloater::onReject()
    {
        LocalTransport::instance().rejectPending();
        refresh();
    }

    void RemoteFloater::onDisconnect()
    {
        RemoteActions::instance().stopMovement();
        LocalTransport::instance().disconnect();
        RemoteController::instance().end();
        refresh();
    }

    void RemoteFloater::onEmergencyRelease()
    {
        RemoteActions::instance().stopMovement();
        LocalTransport::instance().disconnect();
        RemoteSession::instance().emergencyRelease();
        RemoteController::instance().end();
        RemoteEvents::instance().clear();
        report("Emergency release: possession ended locally.");
        refresh();
    }

    void RemoteFloater::onAllowControllerIM()
    {
        if (mRefreshing)
        {
            return;
        }

        auto& session = RemoteSession::instance();
        if (!session.isActive())
        {
            return;
        }

        auto permissions = session.permissions();
        const auto mask = toMask(RemotePermission::InstantMessage);
        if (getChild<LLCheckBoxCtrl>("allow_controller_im")->getValue().asBoolean())
        {
            permissions |= mask;
        }
        else
        {
            permissions &= ~mask;
        }
        session.setPermissions(permissions);
    }

    void RemoteFloater::onAllowRestrictions()
    {
        if (mRefreshing)
        {
            return;
        }

        auto& session = RemoteSession::instance();
        if (!session.isActive())
        {
            return;
        }

        auto permissions = session.permissions();
        const auto mask = toMask(RemotePermission::ManageSubjectRestrictions);
        if (getChild<LLCheckBoxCtrl>("allow_restrictions")->getValue().asBoolean())
        {
            permissions |= mask;
        }
        else
        {
            permissions &= ~mask;
        }
        session.setPermissions(permissions);
    }

    void RemoteFloater::onDisableLocalMovement()
    {
        if (mRefreshing)
        {
            return;
        }
        RemoteSession::instance().setSubjectLocalMovementDisabled(
            getChild<LLCheckBoxCtrl>("disable_local_movement")->getValue().asBoolean());
    }

    void RemoteFloater::onSubjectRestrictChat()
    {
        if (mRefreshing)
        {
            return;
        }

        auto& session = RemoteSession::instance();
        auto restrictions = session.subjectRestrictions();
        const auto mask = toMask(SubjectRestriction::NearbyChat);
        if (getChild<LLCheckBoxCtrl>("subject_restrict_chat")->getValue().asBoolean())
        {
            restrictions |= mask;
        }
        else
        {
            restrictions &= ~mask;
        }
        session.setSubjectRestrictions(restrictions);
    }

    void RemoteFloater::onSubjectRestrictIM()
    {
        if (mRefreshing)
        {
            return;
        }

        auto& session = RemoteSession::instance();
        auto restrictions = session.subjectRestrictions();
        const auto mask = toMask(SubjectRestriction::InstantMessage);
        if (getChild<LLCheckBoxCtrl>("subject_restrict_im")->getValue().asBoolean())
        {
            restrictions |= mask;
        }
        else
        {
            restrictions &= ~mask;
        }
        session.setSubjectRestrictions(restrictions);
    }

    void RemoteFloater::sendRemoteCommand(RemoteCommandType type,
                                           const std::string& text,
                                           const std::string& target_id)
    {
        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (!controller.isActive() || !transport.isPaired())
        {
            report("No accepted controller session is active.");
            return;
        }

        RemoteCommand command = controller.makeCommand(type, text, target_id);
        if (!transport.sendCommand(command))
        {
            report(transport.lastStatus());
        }
    }

    void RemoteFloater::onRemoteSay()
    {
        const std::string text = getChild<LLLineEditor>("remote_say_text")->getText();
        if (!text.empty())
        {
            sendRemoteCommand(RemoteCommandType::Say, text);
            getChild<LLLineEditor>("remote_say_text")->setText(LLStringExplicit(""));
        }
    }

    void RemoteFloater::onRemoteIM()
    {
        const std::string target = getChild<LLLineEditor>("remote_im_target")->getText();
        const std::string text = getChild<LLLineEditor>("remote_im_text")->getText();

        LLUUID target_id(target);
        if (target_id.isNull() || text.empty())
        {
            report("Enter a valid avatar UUID and an IM message.");
            return;
        }

        sendRemoteCommand(RemoteCommandType::SendInstantMessage, text, target);
        getChild<LLLineEditor>("remote_im_text")->setText(LLStringExplicit(""));
    }

    void RemoteFloater::onRemoteRestrictChat()
    {
        if (mRefreshing)
        {
            return;
        }
        sendRemoteCommand(
            getChild<LLCheckBoxCtrl>("remote_restrict_chat")->getValue().asBoolean()
                ? RemoteCommandType::RestrictNearbyChatOn
                : RemoteCommandType::RestrictNearbyChatOff);
    }

    void RemoteFloater::onRemoteRestrictIM()
    {
        if (mRefreshing)
        {
            return;
        }
        sendRemoteCommand(
            getChild<LLCheckBoxCtrl>("remote_restrict_im")->getValue().asBoolean()
                ? RemoteCommandType::RestrictInstantMessageOn
                : RemoteCommandType::RestrictInstantMessageOff);
    }
}

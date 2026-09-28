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
#include "blazingstorm/remote/bstruststore.h"

#include "fscommon.h"
#include "llagent.h"
#include "llagentui.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "lluuid.h"
#include "llviewercontrol.h"

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
        getChild<LLButton>("save_current_controller")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSaveCurrentController(); });

        auto* accepted_message = getChild<LLLineEditor>("accepted_message");
        accepted_message->setText(
            gSavedPerAccountSettings.getString("BlazingStormPossessionAcceptedMessage"));
        accepted_message->setCommitCallback(
            [](LLUICtrl* ctrl, const LLSD&)
            {
                gSavedPerAccountSettings.setString(
                    "BlazingStormPossessionAcceptedMessage",
                    ctrl->getValue().asString());
            });

        auto* whitelist_message = getChild<LLLineEditor>("whitelist_accepted_message");
        whitelist_message->setText(
            gSavedPerAccountSettings.getString("BlazingStormWhitelistAcceptedMessage"));
        whitelist_message->setCommitCallback(
            [](LLUICtrl* ctrl, const LLSD&)
            {
                gSavedPerAccountSettings.setString(
                    "BlazingStormWhitelistAcceptedMessage",
                    ctrl->getValue().asString());
            });

        getChild<LLCheckBoxCtrl>("allow_controller_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onAllowControllerIM(); });
        getChild<LLCheckBoxCtrl>("allow_restrictions")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onAllowRestrictions(); });
        getChild<LLCheckBoxCtrl>("disable_local_movement")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onDisableLocalMovement(); });
        getChild<LLCheckBoxCtrl>("subject_restrict_movement")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSubjectRestrictMovement(); });
        getChild<LLCheckBoxCtrl>("subject_restrict_chat")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSubjectRestrictChat(); });
        getChild<LLCheckBoxCtrl>("subject_restrict_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSubjectRestrictIM(); });

        getChild<LLComboBox>("movement_mode")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementModeChanged(); });

        getChild<LLButton>("remote_forward")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::MoveForward); });
        getChild<LLButton>("remote_back")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::MoveBackward); });
        getChild<LLButton>("remote_left")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::StrafeLeft); });
        getChild<LLButton>("remote_right")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::StrafeRight); });
        getChild<LLButton>("remote_turn_left")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::TurnLeft); });
        getChild<LLButton>("remote_turn_right")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::TurnRight); });
        getChild<LLButton>("remote_jump")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::Jump); });
        getChild<LLButton>("remote_crouch")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::MoveDown); });
        getChild<LLButton>("remote_fly")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::FlyOn); });
        getChild<LLButton>("remote_land")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::FlyOff); });
        getChild<LLButton>("remote_stop")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onMovementCommand(RemoteCommandType::Stop); });
        getChild<LLButton>("remote_say")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteSay(); });
        getChild<LLButton>("remote_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteIM(); });
        getChild<LLCheckBoxCtrl>("remote_restrict_movement")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteRestrictMovement(); });
        getChild<LLCheckBoxCtrl>("remote_restrict_chat")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteRestrictChat(); });
        getChild<LLCheckBoxCtrl>("remote_restrict_im")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoteRestrictIM(); });
        getChild<LLButton>("remote_release")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { sendRemoteCommand(RemoteCommandType::EmergencyRelease); });

        getChild<LLComboBox>("trusted_controller_list")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onTrustedControllerSelected(); });
        getChild<LLButton>("trusted_save")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onSaveTrustedController(); });
        getChild<LLButton>("trusted_remove")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { onRemoveTrustedController(); });
        getChild<LLButton>("trusted_new")->setCommitCallback(
            [this](LLUICtrl*, const LLSD&) { clearTrustedControllerEditor(); });

        refreshTrustedControllers();
        clearTrustedControllerEditor();
        refresh();
        return true;
    }

    void RemoteFloater::draw()
    {
        applyLocalMovement();
        refresh();
        LLFloater::draw();
    }

    void RemoteFloater::onClose(bool app_quitting)
    {
        stopLocalMovement();

        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (controller.isActive() && transport.isPaired())
        {
            RemoteCommand stop = controller.makeCommand(RemoteCommandType::Stop);
            transport.sendCommand(stop);
        }

        LLFloater::onClose(app_quitting);
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

        getChild<LLLineEditor>("pairing_code")->setText(gAgentID.asString());

        const bool is_host = transport.role() == RemoteRole::Host;
        const bool is_controller = transport.role() == RemoteRole::Controller;
        const bool subject_active = session.isActive();
        const bool controller_active = controller.isActive() && transport.isPaired();

        getChild<LLButton>("accept")->setEnabled(is_host && transport.hasPendingPairing());
        getChild<LLButton>("reject")->setEnabled(is_host && transport.hasPendingPairing());
        getChild<LLButton>("disconnect")->setEnabled(transport.role() != RemoteRole::None);
        getChild<LLButton>("emergency_release")->setEnabled(subject_active || is_host);
        getChild<LLButton>("save_current_controller")->setEnabled(subject_active && is_host);

        auto* allow_im = getChild<LLCheckBoxCtrl>("allow_controller_im");
        allow_im->setEnabled(subject_active && is_host);
        allow_im->setValue(session.hasPermission(RemotePermission::InstantMessage));

        auto* allow_restrictions = getChild<LLCheckBoxCtrl>("allow_restrictions");
        allow_restrictions->setEnabled(subject_active && is_host);
        allow_restrictions->setValue(session.hasPermission(RemotePermission::ManageSubjectRestrictions));

        auto* disable_local = getChild<LLCheckBoxCtrl>("disable_local_movement");
        disable_local->setEnabled(subject_active && is_host);
        disable_local->setValue(session.subjectLocalMovementDisabled());

        auto* restrict_movement = getChild<LLCheckBoxCtrl>("subject_restrict_movement");
        restrict_movement->setEnabled(subject_active && is_host);
        restrict_movement->setValue(session.isSubjectRestricted(SubjectRestriction::Movement));

        auto* restrict_chat = getChild<LLCheckBoxCtrl>("subject_restrict_chat");
        restrict_chat->setEnabled(subject_active && is_host);
        restrict_chat->setValue(session.isSubjectRestricted(SubjectRestriction::NearbyChat));

        auto* restrict_im = getChild<LLCheckBoxCtrl>("subject_restrict_im");
        restrict_im->setEnabled(subject_active && is_host);
        restrict_im->setValue(session.isSubjectRestricted(SubjectRestriction::InstantMessage));

        const bool movement_available =
            controlsController() || (controlsSubject() && controller_active);

        const char* movement_buttons[] = {
            "remote_forward", "remote_back", "remote_left", "remote_right",
            "remote_turn_left", "remote_turn_right", "remote_jump", "remote_crouch",
            "remote_fly", "remote_land", "remote_stop"
        };
        for (const char* name : movement_buttons)
        {
            getChild<LLButton>(name)->setEnabled(movement_available);
        }

        getChild<LLButton>("remote_say")->setEnabled(controller_active);
        getChild<LLButton>("remote_im")->setEnabled(controller_active);
        getChild<LLButton>("remote_release")->setEnabled(controller_active);

        S32 movement_mode_index = 0;
        if (controller.movementMode() == RemoteMovementMode::MirrorBoth)
        {
            movement_mode_index = 1;
        }
        else if (controller.movementMode() == RemoteMovementMode::ControllerOnly)
        {
            movement_mode_index = 2;
        }
        getChild<LLComboBox>("movement_mode")->setCurrentByIndex(movement_mode_index);
        getChild<LLComboBox>("movement_mode")->setEnabled(true);

        getChild<LLCheckBoxCtrl>("remote_restrict_movement")->setEnabled(controller_active);
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
        if (!transport.startHost(LocalTransport::portForAvatarId(gAgentID.asString())))
        {
            report(transport.lastStatus());
        }
        refresh();
    }

    void RemoteFloater::onConnect()
    {
        const std::string subject_id = getChild<LLLineEditor>("connect_code")->getText();
        LLUUID subject_uuid(subject_id);
        if (subject_uuid.isNull())
        {
            report("Enter a valid subject avatar UUID.");
            return;
        }

        std::string controller_name;
        LLAgentUI::buildFullname(controller_name);

        auto& transport = LocalTransport::instance();
        if (!transport.requestController(subject_id, gAgentID.asString(), controller_name))
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

    void RemoteFloater::onSaveCurrentController()
    {
        auto& session = RemoteSession::instance();
        auto& transport = LocalTransport::instance();

        if (!session.isActive() || session.controllerId().empty())
        {
            report("There is no active subject possession session to save.");
            return;
        }

        TrustedController entry;
        entry.avatarId = session.controllerId();
        entry.avatarName = transport.pendingControllerName();
        if (entry.avatarName.empty())
        {
            entry.avatarName = entry.avatarId;
        }
        entry.permissions = session.permissions();

        TrustStore::instance().upsert(entry);
        refreshTrustedControllers();
        getChild<LLComboBox>("trusted_controller_list")->setValue(LLSD(entry.avatarId));
        onTrustedControllerSelected();

        report("Current controller added to whitelist. Future matching requests will auto-accept.");
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

    void RemoteFloater::onSubjectRestrictMovement()
    {
        if (mRefreshing)
        {
            return;
        }

        auto& session = RemoteSession::instance();
        auto restrictions = session.subjectRestrictions();
        const auto mask = toMask(SubjectRestriction::Movement);
        if (getChild<LLCheckBoxCtrl>("subject_restrict_movement")->getValue().asBoolean())
        {
            restrictions |= mask;
        }
        else
        {
            restrictions &= ~mask;
        }
        session.setSubjectRestrictions(restrictions);
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

    RemoteMovementMode RemoteFloater::movementMode() const
    {
        return RemoteController::instance().movementMode();
    }

    bool RemoteFloater::controlsSubject() const
    {
        return RemoteController::instance().controlsSubject();
    }

    bool RemoteFloater::controlsController() const
    {
        return RemoteController::instance().controlsController();
    }

    void RemoteFloater::onMovementModeChanged()
    {
        if (mRefreshing)
        {
            return;
        }

        // Never leave either avatar moving when switching routing modes.
        stopLocalMovement();

        auto& controller = RemoteController::instance();
        auto& transport = LocalTransport::instance();
        if (controller.isActive() && transport.isPaired())
        {
            RemoteCommand stop = controller.makeCommand(RemoteCommandType::Stop);
            transport.sendCommand(stop);
        }

        const S32 index = getChild<LLComboBox>("movement_mode")->getCurrentIndex();
        if (index == 1)
        {
            controller.setMovementMode(RemoteMovementMode::MirrorBoth);
        }
        else if (index == 2)
        {
            controller.setMovementMode(RemoteMovementMode::ControllerOnly);
        }
        else
        {
            controller.setMovementMode(RemoteMovementMode::SubjectOnly);
        }
    }

    void RemoteFloater::onMovementCommand(RemoteCommandType type)
    {
        if (type == RemoteCommandType::Stop)
        {
            stopLocalMovement();
            if (controlsSubject())
            {
                sendRemoteCommand(RemoteCommandType::Stop);
            }
            return;
        }

        if (controlsSubject())
        {
            sendRemoteCommand(type);
        }

        if (!controlsController())
        {
            return;
        }

        switch (type)
        {
            case RemoteCommandType::MoveForward:
                mLocalMoveAt = 1;
                mLocalMoveLeft = 0;
                mLocalYaw = 0.f;
                break;
            case RemoteCommandType::MoveBackward:
                mLocalMoveAt = -1;
                mLocalMoveLeft = 0;
                mLocalYaw = 0.f;
                break;
            case RemoteCommandType::StrafeLeft:
                mLocalMoveAt = 0;
                mLocalMoveLeft = 1;
                mLocalYaw = 0.f;
                break;
            case RemoteCommandType::StrafeRight:
                mLocalMoveAt = 0;
                mLocalMoveLeft = -1;
                mLocalYaw = 0.f;
                break;
            case RemoteCommandType::TurnLeft:
                mLocalMoveAt = 0;
                mLocalMoveLeft = 0;
                mLocalYaw = 1.f;
                break;
            case RemoteCommandType::TurnRight:
                mLocalMoveAt = 0;
                mLocalMoveLeft = 0;
                mLocalYaw = -1.f;
                break;
            case RemoteCommandType::MoveUp:
                mLocalMoveUp = 1;
                break;
            case RemoteCommandType::MoveDown:
                mLocalMoveUp = -1;
                break;
            case RemoteCommandType::FlyOn:
                gAgent.setFlying(true, true);
                break;
            case RemoteCommandType::FlyOff:
                gAgent.setFlying(false);
                break;
            case RemoteCommandType::ToggleFly:
                gAgent.setFlying(!gAgent.getFlying(), true);
                break;
            case RemoteCommandType::Jump:
                gAgent.moveUp(1);
                break;
            default:
                break;
        }
    }

    void RemoteFloater::applyLocalMovement()
    {
        if (!controlsController())
        {
            return;
        }

        if (mLocalMoveAt != 0)
        {
            gAgent.moveAt(mLocalMoveAt);
        }
        if (mLocalMoveLeft != 0)
        {
            gAgent.moveLeft(mLocalMoveLeft);
        }
        if (mLocalYaw != 0.f)
        {
            gAgent.moveYaw(mLocalYaw);
        }
        if (mLocalMoveUp != 0)
        {
            gAgent.moveUp(mLocalMoveUp);
        }
    }

    void RemoteFloater::stopLocalMovement()
    {
        mLocalMoveAt = 0;
        mLocalMoveLeft = 0;
        mLocalMoveUp = 0;
        mLocalYaw = 0.f;
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

    void RemoteFloater::onRemoteRestrictMovement()
    {
        if (mRefreshing)
        {
            return;
        }
        sendRemoteCommand(
            getChild<LLCheckBoxCtrl>("remote_restrict_movement")->getValue().asBoolean()
                ? RemoteCommandType::RestrictMovementOn
                : RemoteCommandType::RestrictMovementOff);
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

    void RemoteFloater::refreshTrustedControllers()
    {
        auto* combo = getChild<LLComboBox>("trusted_controller_list");
        combo->clearRows();
        combo->add("New controller...", LLSD(""));

        for (const auto& entry : TrustStore::instance().entries())
        {
            const std::string label =
                entry.avatarName.empty() ? entry.avatarId : entry.avatarName;
            combo->add(label, LLSD(entry.avatarId));
        }

        combo->selectFirstItem();
    }

    void RemoteFloater::clearTrustedControllerEditor()
    {
        getChild<LLLineEditor>("trusted_avatar_id")->setText(LLStringExplicit(""));
        getChild<LLLineEditor>("trusted_avatar_name")->setText(LLStringExplicit(""));

        getChild<LLCheckBoxCtrl>("trusted_movement")->setValue(true);
        getChild<LLCheckBoxCtrl>("trusted_chat")->setValue(true);
        getChild<LLCheckBoxCtrl>("trusted_im")->setValue(false);
        getChild<LLCheckBoxCtrl>("trusted_restrictions")->setValue(false);
    }

    void RemoteFloater::onTrustedControllerSelected()
    {
        const std::string avatar_id =
            getChild<LLComboBox>("trusted_controller_list")->getValue().asString();

        if (avatar_id.empty())
        {
            clearTrustedControllerEditor();
            return;
        }

        const TrustedController* entry = TrustStore::instance().find(avatar_id);
        if (!entry)
        {
            clearTrustedControllerEditor();
            return;
        }

        getChild<LLLineEditor>("trusted_avatar_id")->setText(entry->avatarId);
        getChild<LLLineEditor>("trusted_avatar_name")->setText(entry->avatarName);
        getChild<LLCheckBoxCtrl>("trusted_movement")->setValue(
            (entry->permissions & toMask(RemotePermission::Movement)) != 0);
        getChild<LLCheckBoxCtrl>("trusted_chat")->setValue(
            (entry->permissions & toMask(RemotePermission::Chat)) != 0);
        getChild<LLCheckBoxCtrl>("trusted_im")->setValue(
            (entry->permissions & toMask(RemotePermission::InstantMessage)) != 0);
        getChild<LLCheckBoxCtrl>("trusted_restrictions")->setValue(
            (entry->permissions & toMask(RemotePermission::ManageSubjectRestrictions)) != 0);
    }

    void RemoteFloater::onSaveTrustedController()
    {
        const std::string avatar_id =
            getChild<LLLineEditor>("trusted_avatar_id")->getText();
        LLUUID id(avatar_id);
        if (id.isNull())
        {
            report("Enter a valid controller avatar UUID before saving.");
            return;
        }

        TrustedController entry;
        entry.avatarId = avatar_id;
        entry.avatarName = getChild<LLLineEditor>("trusted_avatar_name")->getText();

        if (getChild<LLCheckBoxCtrl>("trusted_movement")->getValue().asBoolean())
            entry.permissions |= toMask(RemotePermission::Movement);
        if (getChild<LLCheckBoxCtrl>("trusted_chat")->getValue().asBoolean())
            entry.permissions |= toMask(RemotePermission::Chat);
        if (getChild<LLCheckBoxCtrl>("trusted_im")->getValue().asBoolean())
            entry.permissions |= toMask(RemotePermission::InstantMessage);
        if (getChild<LLCheckBoxCtrl>("trusted_restrictions")->getValue().asBoolean())
            entry.permissions |= toMask(RemotePermission::ManageSubjectRestrictions);

        TrustStore::instance().upsert(entry);
        refreshTrustedControllers();
        getChild<LLComboBox>("trusted_controller_list")->setValue(LLSD(avatar_id));
        onTrustedControllerSelected();
        report("Saved trusted-controller permission profile.");
    }

    void RemoteFloater::onRemoveTrustedController()
    {
        std::string avatar_id =
            getChild<LLComboBox>("trusted_controller_list")->getValue().asString();
        if (avatar_id.empty())
        {
            avatar_id = getChild<LLLineEditor>("trusted_avatar_id")->getText();
        }

        if (!avatar_id.empty())
        {
            TrustStore::instance().remove(avatar_id);
        }

        refreshTrustedControllers();
        clearTrustedControllerEditor();
        report("Trusted-controller profile removed.");
    }
}

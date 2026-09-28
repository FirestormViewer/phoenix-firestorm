/**
 * @file bsremotefloater.h
 * @brief Blazing Storm possession/control test UI.
 */

#ifndef BS_REMOTE_FLOATER_H
#define BS_REMOTE_FLOATER_H

#include "llfloater.h"
#include "blazingstorm/remote/bsremoteprotocol.h"

namespace BlazingStorm
{
    class RemoteFloater final : public LLFloater
    {
    public:
        explicit RemoteFloater(const LLSD& key);

        bool postBuild() override;
        void draw() override;
        void onClose(bool app_quitting) override;

    private:
        void refresh();

        void onStartHost();
        void onConnect();
        void onAccept();
        void onReject();
        void onDisconnect();
        void onEmergencyRelease();

        void onAllowControllerIM();
        void onAllowRestrictions();
        void onDisableLocalMovement();
        void onSubjectRestrictChat();
        void onSubjectRestrictIM();

        enum class MovementMode
        {
            SubjectOnly,
            MirrorBoth,
            ControllerOnly
        };

        MovementMode movementMode() const;
        bool controlsSubject() const;
        bool controlsController() const;
        void onMovementModeChanged();
        void onMovementCommand(RemoteCommandType type);
        void applyLocalMovement();
        void stopLocalMovement();

        void sendRemoteCommand(RemoteCommandType type,
                               const std::string& text = {},
                               const std::string& target_id = {});
        void onRemoteSay();
        void onRemoteIM();
        void onRemoteRestrictChat();
        void onRemoteRestrictIM();

        bool mRefreshing = false;
        S32 mLocalMoveAt = 0;
        S32 mLocalMoveLeft = 0;
        F32 mLocalYaw = 0.f;
    };
}

#endif // BS_REMOTE_FLOATER_H

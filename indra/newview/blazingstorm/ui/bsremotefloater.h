/**
 * @file bsremotefloater.h
 * @brief Blazing Storm possession/control test UI.
 */

#ifndef BS_REMOTE_FLOATER_H
#define BS_REMOTE_FLOATER_H

#include "llfloater.h"

namespace BlazingStorm
{
    class RemoteFloater final : public LLFloater
    {
    public:
        explicit RemoteFloater(const LLSD& key);

        bool postBuild() override;
        void draw() override;

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

        void sendRemoteCommand(RemoteCommandType type,
                               const std::string& text = {},
                               const std::string& target_id = {});
        void onRemoteSay();
        void onRemoteIM();
        void onRemoteRestrictChat();
        void onRemoteRestrictIM();

        bool mRefreshing = false;
    };
}

#endif // BS_REMOTE_FLOATER_H

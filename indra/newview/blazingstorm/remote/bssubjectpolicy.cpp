/**
 * @file bssubjectpolicy.cpp
 * @brief Enforcement points for restrictions placed on the possessed user.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bssubjectpolicy.h"

#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "fscommon.h"

namespace BlazingStorm
{
    bool SubjectPolicy::handleOutgoingNearbyChat(const std::string& text, int channel)
    {
        auto& session = RemoteSession::instance();

        if (!session.isActive()
            || !session.isSubjectRestricted(SubjectRestriction::NearbyChat)
            || channel != 0
            || text.empty())
        {
            return false;
        }

        // Subject-entered public chat becomes a private controller event.
        // The local echo is visible only to the subject and is not transmitted
        // to Second Life.
        RemoteEvents::instance().pushThought(text);
        FSCommon::report_to_nearby_chat("[Thought to controller] " + text);
        return true;
    }
    bool SubjectPolicy::handleOutgoingInstantMessage(const std::string& text)
    {
        auto& session = RemoteSession::instance();

        if (!session.isActive()
            || !session.isSubjectRestricted(SubjectRestriction::InstantMessage)
            || text.empty())
        {
            return false;
        }

        // Keep third-party IM privacy intact: only the subject-authored text is
        // diverted to the controller. Incoming IMs are not mirrored.
        RemoteEvents::instance().pushThought("[IM thought] " + text);
        FSCommon::report_to_nearby_chat("[Private thought to controller] " + text);
        return true;
    }

}

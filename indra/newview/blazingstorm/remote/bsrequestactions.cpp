/**
 * @file bsrequestactions.cpp
 * @brief Shared entry points for starting Blazing Storm possession requests.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsrequestactions.h"

#include "blazingstorm/remote/bslocaltransport.h"

#include "fscommon.h"
#include "llagent.h"
#include "llagentui.h"

namespace BlazingStorm
{
    bool RequestActions::requestPossession(const LLUUID& subject_id)
    {
        if (subject_id.isNull())
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] Cannot request possession: target avatar is invalid.");
            return false;
        }

        if (subject_id == gAgentID)
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] You cannot request possession of your own avatar.");
            return false;
        }

        std::string controller_name;
        LLAgentUI::buildFullname(controller_name);

        auto& transport = LocalTransport::instance();
        if (!transport.requestController(
                subject_id.asString(),
                gAgentID.asString(),
                controller_name))
        {
            FSCommon::report_to_nearby_chat(
                "[Blazing Storm] " + transport.lastStatus());
            return false;
        }

        FSCommon::report_to_nearby_chat(
            "[Blazing Storm] Possession request sent to "
            + subject_id.asString() + ".");
        return true;
    }
}

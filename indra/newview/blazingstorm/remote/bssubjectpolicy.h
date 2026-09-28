/**
 * @file bssubjectpolicy.h
 * @brief Enforcement points for restrictions placed on the possessed user.
 */

#ifndef BS_SUBJECT_POLICY_H
#define BS_SUBJECT_POLICY_H

#include <string>

namespace BlazingStorm
{
    class SubjectPolicy final
    {
    public:
        // Returns true when the attempted message was consumed locally and
        // must not be sent to Second Life as nearby chat.
        static bool handleOutgoingNearbyChat(const std::string& text, int channel);

        // Returns true when a subject-authored direct IM was consumed locally
        // and must not be sent to Second Life.
        static bool handleOutgoingInstantMessage(const std::string& text);
    };
}

#endif // BS_SUBJECT_POLICY_H

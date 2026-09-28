/**
 * @file bsrequestactions.h
 * @brief Shared entry points for starting Blazing Storm possession requests.
 */

#ifndef BS_REQUEST_ACTIONS_H
#define BS_REQUEST_ACTIONS_H

#include "lluuid.h"

namespace BlazingStorm
{
    class RequestActions final
    {
    public:
        // Sends the SL IM bootstrap and begins the local controller-side
        // connection attempt for the selected subject.
        static bool requestPossession(const LLUUID& subject_id);
    };
}

#endif // BS_REQUEST_ACTIONS_H

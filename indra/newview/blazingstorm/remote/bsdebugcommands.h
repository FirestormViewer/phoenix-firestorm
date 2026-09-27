/**
 * @file bsdebugcommands.h
 * @brief Local chat commands used to exercise Blazing Storm remote control.
 */

#ifndef BS_DEBUG_COMMANDS_H
#define BS_DEBUG_COMMANDS_H

#include <string_view>

namespace BlazingStorm
{
    bool handleDebugChatCommand(std::string_view text);
}

#endif // BS_DEBUG_COMMANDS_H

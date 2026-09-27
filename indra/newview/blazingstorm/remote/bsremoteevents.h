/**
 * @file bsremoteevents.h
 * @brief Events emitted by the possessed viewer for the remote controller.
 */

#ifndef BS_REMOTE_EVENTS_H
#define BS_REMOTE_EVENTS_H

#include <cstddef>
#include <string>
#include <vector>

namespace BlazingStorm
{
    enum class RemoteEventType
    {
        Thought,
        IncomingInstantMessage
    };

    struct RemoteEvent
    {
        RemoteEventType type = RemoteEventType::Thought;
        std::string fromId;
        std::string fromName;
        std::string sessionId;
        std::string text;
    };

    class RemoteEvents final
    {
    public:
        static RemoteEvents& instance();

        void pushThought(std::string text);
        void pushIncomingInstantMessage(std::string from_id,
                                        std::string from_name,
                                        std::string session_id,
                                        std::string text);

        std::vector<RemoteEvent> takeAll();
        std::size_t size() const;
        void clear();

    private:
        RemoteEvents() = default;

        std::vector<RemoteEvent> mPending;
    };
}

#endif // BS_REMOTE_EVENTS_H

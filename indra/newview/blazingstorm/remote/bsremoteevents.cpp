/**
 * @file bsremoteevents.cpp
 * @brief Events emitted by the possessed viewer for the remote controller.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bsremoteevents.h"

#include <utility>

namespace BlazingStorm
{
    RemoteEvents& RemoteEvents::instance()
    {
        static RemoteEvents events;
        return events;
    }

    void RemoteEvents::pushThought(std::string text)
    {
        RemoteEvent event;
        event.type = RemoteEventType::Thought;
        event.text = std::move(text);
        mPending.emplace_back(std::move(event));
    }

    void RemoteEvents::pushIncomingInstantMessage(std::string from_id,
                                                   std::string from_name,
                                                   std::string session_id,
                                                   std::string text)
    {
        RemoteEvent event;
        event.type = RemoteEventType::IncomingInstantMessage;
        event.fromId = std::move(from_id);
        event.fromName = std::move(from_name);
        event.sessionId = std::move(session_id);
        event.text = std::move(text);
        mPending.emplace_back(std::move(event));
    }

    std::vector<RemoteEvent> RemoteEvents::takeAll()
    {
        std::vector<RemoteEvent> result;
        result.swap(mPending);
        return result;
    }

    std::size_t RemoteEvents::size() const
    {
        return mPending.size();
    }

    void RemoteEvents::clear()
    {
        mPending.clear();
    }
}

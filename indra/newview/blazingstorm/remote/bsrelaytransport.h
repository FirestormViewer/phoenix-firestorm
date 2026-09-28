/**
 * @file bsrelaytransport.h
 * @brief Azure Functions + Azure Web PubSub transport for Blazing Storm.
 */

#ifndef BS_RELAY_TRANSPORT_H
#define BS_RELAY_TRANSPORT_H

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace BlazingStorm
{
    enum class RelayBrokerRole
    {
        Subject,
        Controller
    };

    enum class RelayEventType
    {
        SessionCreated,
        Negotiated,
        Connected,
        Message,
        Closed,
        Error
    };

    struct RelayEvent
    {
        RelayEventType type = RelayEventType::Error;
        std::string sessionId;
        std::string subjectTicket;
        std::string controllerTicket;
        std::string clientUrl;
        std::string receiveGroup;
        std::string sendGroup;
        std::string payload;
        std::string detail;
        std::string matchedPath;
    };

    class RelayTransport final
    {
    public:
        static constexpr std::size_t MAX_MESSAGE_BYTES = 128 * 1024;

        static RelayTransport& instance();
        ~RelayTransport();

        // Subject-only broker call. The create key never goes to the Controller
        // and is never placed in a URL.
        bool createSession(const std::string& broker_base_url,
                           const std::string& relay_create_key,
                           const std::string& controller_id,
                           const std::string& controller_name,
                           const std::string& nonce,
                           const std::string& path_override = {});

        // Exchanges a short-lived signed session ticket for a Web PubSub
        // client-access URL. Both roles use the same local broker base URL.
        bool negotiate(const std::string& broker_base_url,
                       RelayBrokerRole role,
                       const std::string& session_id,
                       const std::string& ticket,
                       const std::string& path_override = {});

        // Connects to Azure Web PubSub with json.webpubsub.azure.v1. Application
        // payloads are published as text to send_group and received from
        // receive_group.
        bool connectPubSub(const std::string& client_url,
                           const std::string& receive_group,
                           const std::string& send_group);

        bool sendApplication(const std::string& payload);
        void disconnect();

        std::vector<RelayEvent> takeEvents();
        bool isOpen() const;

    private:
        class PubSubImpl;

        RelayTransport() = default;
        RelayTransport(const RelayTransport&) = delete;
        RelayTransport& operator=(const RelayTransport&) = delete;

        using BrokerTask = std::function<void(std::uint64_t)>;

        bool startBrokerTask(BrokerTask task);
        void joinBrokerThread();
        void pushEvent(std::uint64_t generation, RelayEvent event);

        std::atomic<std::uint64_t> mGeneration{1};
        std::thread mBrokerThread;
        std::unique_ptr<PubSubImpl> mPubSub;

        mutable std::mutex mEventMutex;
        std::vector<RelayEvent> mEvents;
    };
}

#endif // BS_RELAY_TRANSPORT_H

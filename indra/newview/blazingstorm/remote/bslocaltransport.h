/**
 * @file bslocaltransport.h
 * @brief Loopback-only transport for testing two Blazing Storm viewers.
 *
 * This transport binds exclusively to 127.0.0.1. It is intentionally not
 * suitable for LAN or Internet use because this first test protocol is not
 * encrypted. A future relay/TLS transport can implement the same higher-level
 * command model without exposing Second Life credentials.
 */

#ifndef BS_LOCAL_TRANSPORT_H
#define BS_LOCAL_TRANSPORT_H

#include "blazingstorm/remote/bsremoteprotocol.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <memory>
#include <string>

namespace BlazingStorm
{
    class LocalTransport final
    {
    public:
        static constexpr std::uint16_t DEFAULT_PORT = 29876;

        static LocalTransport& instance();

        bool startHost(std::uint16_t port = DEFAULT_PORT);
        bool connectController(const std::string& pairing_code,
                               const std::string& controller_id,
                               const std::string& controller_name,
                               std::uint16_t port = DEFAULT_PORT);

        bool requestController(const std::string& subject_id,
                               const std::string& controller_id,
                               const std::string& controller_name);

        static std::uint16_t portForAvatarId(const std::string& avatar_id);

        bool acceptPending();
        void rejectPending();
        void disconnect();

        void update();

        bool sendCommand(const RemoteCommand& command);

        RemoteRole role() const { return mRole; }
        bool isListening() const { return mListening; }
        bool isConnected() const { return mConnected; }
        bool isPaired() const { return mPaired; }
        bool hasPendingPairing() const { return mPendingPairing; }

        const std::string& pairingCode() const { return mPairingCode; }
        const std::string& pendingControllerId() const { return mPendingControllerId; }
        const std::string& pendingControllerName() const { return mPendingControllerName; }
        const std::string& lastStatus() const { return mLastStatus; }
        std::uint16_t port() const { return mPort; }

    private:
        using tcp = boost::asio::ip::tcp;

        LocalTransport() = default;

        std::string generatePairingCode() const;
        void showPairingPrompt();
        void tryAccept();
        void readAvailable();
        void processLine(const std::string& line);
        void queueLine(const std::string& line);
        void flushWrites();

        void closeSocketOnly();
        void resetConnectionState(bool keep_listener);
        void handlePeerDisconnect(const std::string& reason);

        static std::string hexEncode(const std::string& value);
        static bool hexDecode(const std::string& value, std::string& decoded);
        static std::string commandName(RemoteCommandType type);
        static RemoteCommandType commandType(const std::string& name);

        boost::asio::io_context mIo;
        std::unique_ptr<tcp::acceptor> mAcceptor;
        std::unique_ptr<tcp::socket> mSocket;

        RemoteRole mRole = RemoteRole::None;
        bool mListening = false;
        bool mConnected = false;
        bool mPaired = false;
        bool mPendingPairing = false;
        bool mAutoListenerNeedsRestart = true;

        std::uint16_t mPort = DEFAULT_PORT;
        std::string mPairingCode;
        std::string mPendingControllerId;
        std::string mPendingControllerName;
        std::string mReceiveBuffer;
        std::string mWriteBuffer;
        std::string mLastStatus;
    };
}

#endif // BS_LOCAL_TRANSPORT_H

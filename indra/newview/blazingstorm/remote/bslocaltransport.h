/**
 * @file bslocaltransport.h
 * @brief Blazing Storm possession protocol over local loopback or Azure relay.
 *
 * Local mode remains loopback-only for same-machine debugging. Relay mode uses
 * an Azure Function broker and Azure Web PubSub; Subject-side permission and
 * command validation remain authoritative.
 */

#ifndef BS_LOCAL_TRANSPORT_H
#define BS_LOCAL_TRANSPORT_H

#include "blazingstorm/remote/bsremoteprotocol.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
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

        // Recognizes an explicit Blazing Storm bootstrap marker carried by a
        // normal avatar-to-avatar SL IM. Returns true only for a valid marker
        // whose embedded controller UUID matches the actual IM sender.
        bool handleBootstrapInstantMessage(const std::string& from_id,
                                           const std::string& from_name,
                                           const std::string& message,
                                           bool online);

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
        bool usingRelay() const { return mRelayMode; }

    private:
        using tcp = boost::asio::ip::tcp;

        LocalTransport() = default;

        std::string generatePairingCode() const;
        std::string generateBootstrapNonce() const;
        std::string buildBootstrapMessage(const std::string& controller_id,
                                          const std::string& nonce) const;
        bool parseBootstrapMessage(const std::string& message,
                                   std::string& controller_id,
                                   std::string& nonce) const;

        std::string buildRelayRequestMessage(const std::string& controller_id,
                                             const std::string& nonce) const;
        std::string buildRelayInviteMessage(const std::string& subject_id,
                                            const std::string& session_id,
                                            const std::string& controller_ticket,
                                            const std::string& nonce) const;
        std::string buildRelayRejectMessage(const std::string& subject_id,
                                            const std::string& nonce) const;
        bool parseRelayRequestMessage(const std::string& message,
                                      std::string& controller_id,
                                      std::string& nonce) const;
        bool parseRelayInviteMessage(const std::string& message,
                                     std::string& subject_id,
                                     std::string& session_id,
                                     std::string& controller_ticket,
                                     std::string& nonce) const;
        bool parseRelayRejectMessage(const std::string& message,
                                     std::string& subject_id,
                                     std::string& nonce) const;

        bool beginApprovedRelaySession();
        void sendRelayInvite();
        void sendRelayReject();
        void updateRelay();
        void tryBootstrapConnect();
        void announcePossessionAccepted(bool trusted_auto_accept);
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
        bool mPendingTrustedAutoAccept = false;
        bool mBootstrapPending = false;
        bool mRelayMode = false;
        bool mRelayPreapproved = false;
        bool mRelayInviteSent = false;

        std::uint16_t mPort = DEFAULT_PORT;
        std::string mPairingCode;
        std::string mPendingControllerId;
        std::string mPendingControllerName;

        std::string mBootstrapSubjectId;
        std::string mBootstrapControllerId;
        std::string mBootstrapControllerName;
        std::string mBootstrapNonce;
        std::string mExpectedBootstrapControllerId;
        std::string mExpectedBootstrapControllerName;
        std::string mExpectedBootstrapNonce;

        std::string mRelaySessionId;
        std::string mRelaySubjectTicket;
        std::string mRelayControllerTicket;
        std::string mRelayBrokerPath;
        std::string mRelayNegotiatePath;
        std::chrono::steady_clock::time_point mBootstrapDeadline{};
        std::chrono::steady_clock::time_point mNextBootstrapAttempt{};

        std::string mReceiveBuffer;
        std::string mWriteBuffer;
        std::string mLastStatus;
    };
}

#endif // BS_LOCAL_TRANSPORT_H

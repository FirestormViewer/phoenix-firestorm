/**
 * @file bslocaltransport.cpp
 * @brief Blazing Storm possession protocol over loopback or Azure relay.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bslocaltransport.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotecommanddispatcher.h"
#include "blazingstorm/remote/bsremotecontroller.h"
#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "blazingstorm/remote/bstruststore.h"
#include "blazingstorm/remote/bsworldinteraction.h"
#include "blazingstorm/remote/bsremotefeatures.h"
#include "blazingstorm/remote/bsrelaytransport.h"
#include "fscommon.h"
#include "fsnearbychathub.h"
#include "llagent.h"
#include "llimview.h"
#include "llviewermessage.h"
#include "llnotificationsutil.h"
#include "lluuid.h"
#include "llviewercontrol.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/write.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <random>
#include <sstream>
#include <system_error>
#include <vector>

namespace
{
    std::vector<std::string> splitFields(const std::string& line)
    {
        std::vector<std::string> fields;
        std::size_t begin = 0;
        while (true)
        {
            const auto separator = line.find('|', begin);
            if (separator == std::string::npos)
            {
                fields.emplace_back(line.substr(begin));
                break;
            }
            fields.emplace_back(line.substr(begin, separator - begin));
            begin = separator + 1;
        }
        return fields;
    }

    bool parseSequence(const std::string& value, std::uint64_t& sequence)
    {
        const char* first = value.data();
        const char* last = first + value.size();
        auto result = std::from_chars(first, last, sequence);
        return result.ec == std::errc() && result.ptr == last;
    }
}

namespace BlazingStorm
{
    LocalTransport& LocalTransport::instance()
    {
        static LocalTransport transport;
        return transport;
    }

    std::uint16_t LocalTransport::portForAvatarId(const std::string& avatar_id)
    {
        // Stable FNV-1a hash. Each logged-in local viewer gets a predictable
        // loopback-only listener port derived from its avatar UUID.
        std::uint32_t hash = 2166136261u;
        for (const unsigned char ch : avatar_id)
        {
            hash ^= ch;
            hash *= 16777619u;
        }

        return static_cast<std::uint16_t>(30000u + (hash % 20000u));
    }

    std::string LocalTransport::generatePairingCode() const
    {
        std::random_device random;
        const unsigned value = 100000u + (random() % 900000u);
        return std::to_string(value);
    }

    std::string LocalTransport::generateBootstrapNonce() const
    {
        static constexpr char digits[] = "0123456789abcdef";
        std::random_device random;
        std::string nonce;
        nonce.reserve(16);
        for (int i = 0; i < 16; ++i)
        {
            nonce.push_back(digits[random() & 0x0f]);
        }
        return nonce;
    }

    std::string LocalTransport::buildBootstrapMessage(
        const std::string& controller_id,
        const std::string& nonce) const
    {
        return std::string("\xF0\x9F\x9A\xAA The door has opened. Will you step through?\n")
            + "[Blazing Storm request v1 | " + controller_id + " | " + nonce + "]";
    }

    bool LocalTransport::parseBootstrapMessage(
        const std::string& message,
        std::string& controller_id,
        std::string& nonce) const
    {
        static const std::string prefix = "[Blazing Storm request v1 | ";
        const std::size_t begin = message.rfind(prefix);
        if (begin == std::string::npos || message.empty() || message.back() != ']')
        {
            return false;
        }

        const std::size_t id_begin = begin + prefix.size();
        const std::size_t separator = message.find(" | ", id_begin);
        if (separator == std::string::npos)
        {
            return false;
        }

        controller_id = message.substr(id_begin, separator - id_begin);
        nonce = message.substr(separator + 3, message.size() - (separator + 3) - 1);

        LLUUID controller_uuid(controller_id);
        if (controller_uuid.isNull() || nonce.size() != 16)
        {
            return false;
        }

        for (const char ch : nonce)
        {
            if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
            {
                return false;
            }
        }
        return true;
    }


    std::string LocalTransport::buildRelayRequestMessage(
        const std::string& controller_id,
        const std::string& nonce) const
    {
        return std::string("\xF0\x9F\x8C\x90 Blazing Storm Internet relay request.\n")
            + "[Blazing Storm relay request v2 | "
            + controller_id + " | " + nonce + "]";
    }

    std::string LocalTransport::buildRelayInviteMessage(
        const std::string& subject_id,
        const std::string& session_id,
        const std::string& controller_ticket,
        const std::string& nonce) const
    {
        return std::string("\xF0\x9F\x94\x90 Blazing Storm relay invitation.\n")
            + "[Blazing Storm relay invite v2 | "
            + subject_id + " | " + session_id + " | "
            + controller_ticket + " | " + nonce + "]";
    }

    std::string LocalTransport::buildRelayRejectMessage(
        const std::string& subject_id,
        const std::string& nonce) const
    {
        return std::string("Blazing Storm possession request declined.\n")
            + "[Blazing Storm relay reject v2 | "
            + subject_id + " | " + nonce + "]";
    }

    bool LocalTransport::parseRelayRequestMessage(
        const std::string& message,
        std::string& controller_id,
        std::string& nonce) const
    {
        static const std::string prefix =
            "[Blazing Storm relay request v2 | ";
        const auto begin = message.rfind(prefix);
        if (begin == std::string::npos || message.empty()
            || message.back() != ']') return false;

        const auto id_begin = begin + prefix.size();
        const auto separator = message.find(" | ", id_begin);
        if (separator == std::string::npos) return false;

        controller_id =
            message.substr(id_begin, separator - id_begin);
        nonce = message.substr(
            separator + 3,
            message.size() - (separator + 3) - 1);

        LLUUID controller_uuid(controller_id);
        if (controller_uuid.isNull() || nonce.size() != 16) return false;
        return std::all_of(
            nonce.begin(), nonce.end(),
            [](char ch)
            {
                return (ch >= '0' && ch <= '9')
                    || (ch >= 'a' && ch <= 'f');
            });
    }

    bool LocalTransport::parseRelayInviteMessage(
        const std::string& message,
        std::string& subject_id,
        std::string& session_id,
        std::string& controller_ticket,
        std::string& nonce) const
    {
        static const std::string prefix =
            "[Blazing Storm relay invite v2 | ";
        const auto begin = message.rfind(prefix);
        if (begin == std::string::npos || message.empty()
            || message.back() != ']') return false;

        const auto subject_begin = begin + prefix.size();
        const auto sep1 = message.find(" | ", subject_begin);
        const auto sep2 = sep1 == std::string::npos
            ? std::string::npos : message.find(" | ", sep1 + 3);
        const auto sep3 = sep2 == std::string::npos
            ? std::string::npos : message.find(" | ", sep2 + 3);
        if (sep1 == std::string::npos || sep2 == std::string::npos
            || sep3 == std::string::npos) return false;

        subject_id =
            message.substr(subject_begin, sep1 - subject_begin);
        session_id =
            message.substr(sep1 + 3, sep2 - (sep1 + 3));
        controller_ticket =
            message.substr(sep2 + 3, sep3 - (sep2 + 3));
        nonce = message.substr(
            sep3 + 3, message.size() - (sep3 + 3) - 1);

        LLUUID subject_uuid(subject_id);
        if (subject_uuid.isNull()
            || session_id.empty() || session_id.size() > 128
            || controller_ticket.empty() || controller_ticket.size() > 900
            || nonce.size() != 16)
        {
            return false;
        }

        const auto safe_ticket_char = [](char ch)
        {
            const unsigned char c = static_cast<unsigned char>(ch);
            // Tickets/session IDs travel inside a visible SL IM. Permit
            // ordinary signed-token punctuation, but never our field delimiter,
            // controls, brackets, or whitespace.
            return c >= 0x21 && c <= 0x7e
                && ch != '|' && ch != '[' && ch != ']';
        };
        return std::all_of(
                   session_id.begin(), session_id.end(), safe_ticket_char)
            && std::all_of(
                   controller_ticket.begin(),
                   controller_ticket.end(),
                   safe_ticket_char)
            && std::all_of(
                   nonce.begin(), nonce.end(),
                   [](char ch)
                   {
                       return (ch >= '0' && ch <= '9')
                           || (ch >= 'a' && ch <= 'f');
                   });
    }

    bool LocalTransport::parseRelayRejectMessage(
        const std::string& message,
        std::string& subject_id,
        std::string& nonce) const
    {
        static const std::string prefix =
            "[Blazing Storm relay reject v2 | ";
        const auto begin = message.rfind(prefix);
        if (begin == std::string::npos || message.empty()
            || message.back() != ']') return false;

        const auto subject_begin = begin + prefix.size();
        const auto separator = message.find(" | ", subject_begin);
        if (separator == std::string::npos) return false;

        subject_id =
            message.substr(subject_begin, separator - subject_begin);
        nonce = message.substr(
            separator + 3,
            message.size() - (separator + 3) - 1);

        LLUUID subject_uuid(subject_id);
        return subject_uuid.notNull() && nonce.size() == 16;
    }

    bool LocalTransport::beginApprovedRelaySession()
    {
        const std::string broker_url =
            gSavedPerAccountSettings.getString("BlazingStormRelayBrokerUrl");
        const std::string create_key =
            gSavedPerAccountSettings.getString("BlazingStormRelayCreateKey");
        const std::string create_path =
            gSavedPerAccountSettings.getString("BlazingStormRelayCreatePath");

        if (broker_url.empty())
        {
            mLastStatus =
                "Relay approval failed: no Azure Function broker URL is configured.";
            return false;
        }
        if (create_key.empty())
        {
            mLastStatus =
                "Relay approval failed: this Subject has no relay create key configured.";
            return false;
        }

        mRelayPreapproved = true;
        mPendingPairing = false;
        mBootstrapDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(45);

        if (!RelayTransport::instance().createSession(
                broker_url,
                create_key,
                mExpectedBootstrapControllerId,
                mExpectedBootstrapControllerName,
                mExpectedBootstrapNonce,
                create_path))
        {
            mLastStatus = "Could not start the relay create-session request.";
            return false;
        }

        mLastStatus =
            "Approved. Creating a private Azure relay session...";
        return true;
    }

    void LocalTransport::sendRelayInvite()
    {
        if (mRelayInviteSent
            || mRelaySessionId.empty()
            || mRelayControllerTicket.empty())
        {
            return;
        }

        LLUUID controller_uuid(mExpectedBootstrapControllerId);
        if (controller_uuid.isNull()) return;

        const LLUUID im_session_id =
            LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, controller_uuid);
        send_simple_im(
            controller_uuid,
            buildRelayInviteMessage(
                gAgentID.asString(),
                mRelaySessionId,
                mRelayControllerTicket,
                mExpectedBootstrapNonce),
            IM_NOTHING_SPECIAL,
            im_session_id);
        mRelayInviteSent = true;
    }

    void LocalTransport::sendRelayReject()
    {
        LLUUID controller_uuid(mExpectedBootstrapControllerId);
        if (controller_uuid.isNull() || mExpectedBootstrapNonce.empty()) return;

        const LLUUID im_session_id =
            LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, controller_uuid);
        send_simple_im(
            controller_uuid,
            buildRelayRejectMessage(
                gAgentID.asString(),
                mExpectedBootstrapNonce),
            IM_NOTHING_SPECIAL,
            im_session_id);
    }

    void LocalTransport::updateRelay()
    {
        for (const auto& event : RelayTransport::instance().takeEvents())
        {
            if (event.type == RelayEventType::SessionCreated)
            {
                if (mRole != RemoteRole::Host || !mRelayPreapproved)
                    continue;

                mRelaySessionId = event.sessionId;
                mRelaySubjectTicket = event.subjectTicket;
                mRelayControllerTicket = event.controllerTicket;
                mRelayBrokerPath = event.matchedPath;

                // Do not invite the Controller yet. Web PubSub does not
                // guarantee storage for an absent peer; first make sure the
                // Subject has connected and joined its inbound commands group.
                const std::string broker_url =
                    gSavedPerAccountSettings.getString(
                        "BlazingStormRelayBrokerUrl");
                const std::string path =
                    gSavedPerAccountSettings.getString(
                        "BlazingStormRelaySubjectNegotiatePath");

                if (!RelayTransport::instance().negotiate(
                        broker_url,
                        RelayBrokerRole::Subject,
                        mRelaySessionId,
                        mRelaySubjectTicket,
                        path))
                {
                    handlePeerDisconnect(
                        "Could not start Subject relay negotiation.");
                    return;
                }

                mLastStatus =
                    "Relay session created. Negotiating Subject Web PubSub access...";
                continue;
            }

            if (event.type == RelayEventType::Negotiated)
            {
                mRelayNegotiatePath = event.matchedPath;
                if (!RelayTransport::instance().connectPubSub(
                        event.clientUrl,
                        event.receiveGroup,
                        event.sendGroup))
                {
                    handlePeerDisconnect(
                        "Could not open the negotiated Web PubSub connection.");
                    return;
                }

                mLastStatus =
                    "Relay access negotiated. Connecting to Azure Web PubSub...";
                continue;
            }

            if (event.type == RelayEventType::Connected)
            {
                mConnected = true;
                mListening = false;

                if (mRole == RemoteRole::Controller)
                {
                    mBootstrapPending = false;
                    mLastStatus =
                        "Connected through Azure relay; confirming the approved possession session.";

                    queueLine(
                        "REQUEST|" + mBootstrapControllerId
                        + "|" + hexEncode(mBootstrapControllerName)
                        + "|" + mBootstrapNonce);
                    flushWrites();
                }
                else
                {
                    // Subject is now listening to its inbound commands
                    // direction, so it is safe to let the Controller join.
                    sendRelayInvite();
                    mLastStatus =
                        "Azure relay ready; Controller invitation sent through Second Life.";
                }
                continue;
            }

            if (event.type == RelayEventType::Message)
            {
                if (!event.payload.empty()
                    && event.payload.size() <= RelayTransport::MAX_MESSAGE_BYTES)
                {
                    processLine(event.payload);
                }
                continue;
            }

            if (event.type == RelayEventType::Closed)
            {
                handlePeerDisconnect(
                    event.detail.empty()
                        ? "Azure relay connection closed."
                        : event.detail);
                return;
            }

            if (event.type == RelayEventType::Error)
            {
                handlePeerDisconnect(
                    event.detail.empty()
                        ? "Azure relay connection failed."
                        : event.detail);
                return;
            }
        }

        if (mRelayMode
            && !mConnected
            && std::chrono::steady_clock::now() >= mBootstrapDeadline)
        {
            handlePeerDisconnect("Azure relay bootstrap timed out.");
        }
    }

    bool LocalTransport::startHost(std::uint16_t port)
    {
        disconnect();

        boost::system::error_code error;
        auto acceptor = std::make_unique<tcp::acceptor>(mIo);
        const tcp::endpoint endpoint(boost::asio::ip::address_v4::loopback(), port);

        acceptor->open(endpoint.protocol(), error);
        if (error)
        {
            mLastStatus = "Unable to open local possession listener: " + error.message();
            return false;
        }

        acceptor->set_option(boost::asio::socket_base::reuse_address(true), error);
        error.clear();
        acceptor->bind(endpoint, error);
        if (error)
        {
            mLastStatus = "Unable to bind 127.0.0.1:" + std::to_string(port) + ": " + error.message();
            return false;
        }

        acceptor->listen(1, error);
        if (error)
        {
            mLastStatus = "Unable to listen on local possession port: " + error.message();
            return false;
        }

        acceptor->non_blocking(true, error);
        if (error)
        {
            mLastStatus = "Unable to make local listener nonblocking: " + error.message();
            return false;
        }

        mAcceptor = std::move(acceptor);
        mRole = RemoteRole::Host;
        mListening = true;
        mPort = port;
        mPairingCode = generatePairingCode();
        mLastStatus = "Listening for possession requests on this computer.";
        return true;
    }

    bool LocalTransport::connectController(const std::string& pairing_code,
                                            const std::string& controller_id,
                                            const std::string& controller_name,
                                            std::uint16_t port)
    {
        disconnect();

        boost::system::error_code error;
        auto socket = std::make_unique<tcp::socket>(mIo);
        socket->connect(tcp::endpoint(boost::asio::ip::address_v4::loopback(), port), error);
        if (error)
        {
            mLastStatus = "Could not connect to local subject viewer: " + error.message();
            return false;
        }

        socket->non_blocking(true, error);
        if (error)
        {
            mLastStatus = "Could not make local controller socket nonblocking: " + error.message();
            return false;
        }

        mSocket = std::move(socket);
        mRole = RemoteRole::Controller;
        mConnected = true;
        mPort = port;
        mPairingCode = pairing_code;
        mLastStatus = "Connected; waiting for subject approval.";

        queueLine("HELLO|" + pairing_code + "|" + controller_id + "|" + hexEncode(controller_name));
        flushWrites();
        return mConnected;
    }

    bool LocalTransport::requestController(const std::string& subject_id,
                                            const std::string& controller_id,
                                            const std::string& controller_name)
    {
        LLUUID subject_uuid(subject_id);
        LLUUID controller_uuid(controller_id);
        if (subject_uuid.isNull() || controller_uuid.isNull())
        {
            mLastStatus = "Subject or controller avatar UUID is invalid.";
            return false;
        }

        disconnect();

        const std::string nonce = generateBootstrapNonce();
        const bool use_relay =
            !gSavedPerAccountSettings.getString(
                "BlazingStormRelayBrokerUrl").empty();
        const std::string bootstrap_message = use_relay
            ? buildRelayRequestMessage(controller_id, nonce)
            : buildBootstrapMessage(controller_id, nonce);

        const LLUUID im_session_id =
            LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, subject_uuid);
        send_simple_im(
            subject_uuid,
            bootstrap_message,
            IM_NOTHING_SPECIAL,
            im_session_id);

        mRole = RemoteRole::Controller;
        mRelayMode = use_relay;
        mBootstrapPending = true;
        mBootstrapSubjectId = subject_id;
        mBootstrapControllerId = controller_id;
        mBootstrapControllerName = controller_name;
        mBootstrapNonce = nonce;

        const auto now = std::chrono::steady_clock::now();
        mNextBootstrapAttempt = now + std::chrono::milliseconds(250);
        mBootstrapDeadline =
            now + (use_relay
                ? std::chrono::seconds(60)
                : std::chrono::seconds(15));

        mLastStatus = use_relay
            ? "Internet possession request sent through Second Life; waiting for Subject approval and relay invitation."
            : "Bootstrap IM sent through Second Life; waiting for the subject viewer to open its local listener.";
        return true;
    }

    bool LocalTransport::handleBootstrapInstantMessage(
        const std::string& from_id,
        const std::string& from_name,
        const std::string& message,
        bool online)
    {
        if (!online || gAgentID.isNull()) return false;

        // Controller receives either the approved relay invitation or a
        // rejection for its outstanding v2 request.
        if (mRole == RemoteRole::Controller
            && mRelayMode
            && mBootstrapPending)
        {
            std::string subject_id;
            std::string session_id;
            std::string controller_ticket;
            std::string nonce;

            if (parseRelayInviteMessage(
                    message,
                    subject_id,
                    session_id,
                    controller_ticket,
                    nonce))
            {
                if (from_id != mBootstrapSubjectId
                    || subject_id != mBootstrapSubjectId
                    || nonce != mBootstrapNonce)
                {
                    return false;
                }

                mRelaySessionId = session_id;
                mRelayControllerTicket = controller_ticket;
                const std::string broker_url =
                    gSavedPerAccountSettings.getString(
                        "BlazingStormRelayBrokerUrl");
                const std::string path =
                    gSavedPerAccountSettings.getString(
                        "BlazingStormRelayControllerNegotiatePath");

                if (broker_url.empty()
                    || !RelayTransport::instance().negotiate(
                        broker_url,
                        RelayBrokerRole::Controller,
                        mRelaySessionId,
                        mRelayControllerTicket,
                        path))
                {
                    mLastStatus =
                        "Could not start Controller relay negotiation.";
                }
                else
                {
                    mLastStatus =
                        "Subject approved. Negotiating Controller Web PubSub access...";
                }
                return true;
            }

            if (parseRelayRejectMessage(message, subject_id, nonce)
                && from_id == mBootstrapSubjectId
                && subject_id == mBootstrapSubjectId
                && nonce == mBootstrapNonce)
            {
                mLastStatus = "Subject declined the possession request.";
                FSCommon::report_to_nearby_chat(
                    "[Blazing Storm] Subject declined the possession request.");
                resetConnectionState(false);
                return true;
            }
        }

        if (mRole != RemoteRole::None
            || RemoteSession::instance().isActive()
            || RemoteController::instance().isActive())
        {
            return false;
        }

        std::string embedded_controller_id;
        std::string nonce;

        if (parseRelayRequestMessage(
                message, embedded_controller_id, nonce))
        {
            if (embedded_controller_id != from_id) return false;
            LLUUID from_uuid(from_id);
            if (from_uuid.isNull()) return false;

            disconnect();
            mRole = RemoteRole::Host;
            mRelayMode = true;
            mExpectedBootstrapControllerId = from_id;
            mExpectedBootstrapControllerName = from_name;
            mExpectedBootstrapNonce = nonce;
            mPendingControllerId = from_id;
            mPendingControllerName = from_name;
            mPendingPairing = true;
            mBootstrapDeadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(60);

            if (TrustStore::instance().find(mPendingControllerId))
            {
                mPendingTrustedAutoAccept = true;
                mLastStatus =
                    "Trusted Controller "
                    + (from_name.empty() ? from_id : from_name)
                    + " approved; creating Azure relay session.";
                if (!acceptPending())
                {
                    sendRelayReject();
                    resetConnectionState(false);
                }
            }
            else
            {
                mPendingTrustedAutoAccept = false;
                mLastStatus =
                    "Internet possession request from "
                    + (from_name.empty() ? from_id : from_name)
                    + " is waiting for approval.";
                showPairingPrompt();
            }
            return true;
        }

        if (!parseBootstrapMessage(message, embedded_controller_id, nonce)
            || embedded_controller_id != from_id)
        {
            return false;
        }

        LLUUID from_uuid(from_id);
        if (from_uuid.isNull()) return false;

        if (!startHost(portForAvatarId(gAgentID.asString()))) return false;

        mExpectedBootstrapControllerId = from_id;
        mExpectedBootstrapControllerName = from_name;
        mExpectedBootstrapNonce = nonce;
        mBootstrapDeadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(15);
        mLastStatus =
            "Blazing Storm bootstrap received from "
            + (from_name.empty() ? from_id : from_name)
            + "; waiting briefly for the matching local controller connection.";
        return true;
    }

    void LocalTransport::tryBootstrapConnect()
    {
        if (mRelayMode) return;

        if (mRole != RemoteRole::Controller
            || !mBootstrapPending
            || mConnected)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= mBootstrapDeadline)
        {
            mBootstrapPending = false;
            mBootstrapSubjectId.clear();
            mBootstrapControllerId.clear();
            mBootstrapControllerName.clear();
            mBootstrapNonce.clear();
            mRole = RemoteRole::None;
            mLastStatus =
                "Possession bootstrap timed out before the subject viewer opened its listener.";
            return;
        }

        if (now < mNextBootstrapAttempt)
        {
            return;
        }
        mNextBootstrapAttempt = now + std::chrono::milliseconds(250);

        boost::system::error_code error;
        auto socket = std::make_unique<tcp::socket>(mIo);
        socket->connect(
            tcp::endpoint(
                boost::asio::ip::address_v4::loopback(),
                portForAvatarId(mBootstrapSubjectId)),
            error);

        if (error)
        {
            return;
        }

        socket->non_blocking(true, error);
        if (error)
        {
            return;
        }

        mSocket = std::move(socket);
        mConnected = true;
        mPort = portForAvatarId(mBootstrapSubjectId);
        mBootstrapPending = false;
        mLastStatus =
            "Connected to subject viewer; waiting for explicit possession approval.";

        queueLine(
            "REQUEST|" + mBootstrapControllerId
            + "|" + hexEncode(mBootstrapControllerName)
            + "|" + mBootstrapNonce);
        flushWrites();
    }

    void LocalTransport::announcePossessionAccepted(bool trusted_auto_accept)
    {
        const char* setting_name = trusted_auto_accept
            ? "BlazingStormWhitelistAcceptedMessage"
            : "BlazingStormPossessionAcceptedMessage";

        const std::string message =
            gSavedPerAccountSettings.getString(setting_name);

        if (!message.empty())
        {
            // Use the lower-level nearby chat send path so this system
            // announcement is spoken by the subject avatar and is not
            // converted into a "thought" by the subject chat restriction.
            FSNearbyChat::instance().sendChatFromViewer(
                message,
                CHAT_TYPE_NORMAL,
                true);
        }
    }

    bool LocalTransport::acceptPending()
    {
        if (mRole != RemoteRole::Host || !mPendingPairing || mPaired)
        {
            mLastStatus = "There is no pending pairing request to accept.";
            return false;
        }

        if (mRelayMode && !mConnected)
        {
            if (!beginApprovedRelaySession())
            {
                const std::string failure =
                    mLastStatus.empty()
                        ? "Could not create the Azure relay session."
                        : mLastStatus;
                sendRelayReject();
                resetConnectionState(false);
                mLastStatus = failure;
                return false;
            }
            return true;
        }

        if (!mConnected)
        {
            mLastStatus = "The pending local pairing connection is not ready.";
            return false;
        }

        LLUUID session_id;
        session_id.generate();

        RemotePermissionMask initial_permissions =
              toMask(RemotePermission::Movement)
            | toMask(RemotePermission::Chat);

        if (const TrustedController* trusted =
                TrustStore::instance().find(mPendingControllerId))
        {
            initial_permissions = trusted->permissions;
        }

        const bool trusted_auto_accept = mPendingTrustedAutoAccept;

        RemoteCommandDispatcher::instance().reset();
        RemoteActions::instance().stopMovement();
        RemoteSession::instance().begin(mPendingControllerId, initial_permissions);

        mPendingPairing = false;
        mPendingTrustedAutoAccept = false;
        mRelayPreapproved = false;
        mPaired = true;
        mExpectedBootstrapControllerId.clear();
        mExpectedBootstrapControllerName.clear();
        mExpectedBootstrapNonce.clear();
        mLastStatus = "Controller accepted. Possession session is active.";

        queueLine("ACCEPT|" + session_id.asString());
        flushWrites();

        announcePossessionAccepted(trusted_auto_accept);
        return true;
    }

    void LocalTransport::rejectPending()
    {
        if (mRole != RemoteRole::Host || !mPendingPairing)
        {
            mLastStatus = "There is no pending pairing request to reject.";
            return;
        }

        if (mRelayMode && !mConnected)
        {
            sendRelayReject();
            resetConnectionState(false);
            mLastStatus = "Internet possession request declined.";
            return;
        }

        if (!mConnected)
        {
            mLastStatus = "The pending local pairing connection is not ready.";
            return;
        }

        queueLine("REJECT|" + hexEncode("Subject rejected the pairing request."));
        flushWrites();
        resetConnectionState(false);

        mPendingPairing = false;
        mPendingTrustedAutoAccept = false;
        mPendingControllerId.clear();
        mPendingControllerName.clear();
        mExpectedBootstrapControllerId.clear();
        mExpectedBootstrapControllerName.clear();
        mExpectedBootstrapNonce.clear();
        mLastStatus = "Pairing request rejected; local bootstrap listener closed.";
    }

    void LocalTransport::disconnect()
    {
        RemoteFeatures::instance().reset();
        if (mRole == RemoteRole::Host && RemoteSession::instance().isActive())
        {
            RemoteActions::instance().stopMovement();
            RemoteSession::instance().emergencyRelease();
            RemoteCommandDispatcher::instance().reset();
        }

        if (mRole == RemoteRole::Controller)
        {
            RemoteController::instance().end();
        }

        if (mRelayMode)
        {
            RelayTransport::instance().disconnect();
        }
        closeSocketOnly();

        if (mAcceptor)
        {
            boost::system::error_code error;
            mAcceptor->close(error);
            mAcceptor.reset();
        }

        mRole = RemoteRole::None;
        mListening = false;
        mConnected = false;
        mPaired = false;
        mPendingPairing = false;
        mPendingTrustedAutoAccept = false;
        mPairingCode.clear();
        mPendingControllerId.clear();
        mPendingControllerName.clear();
        mBootstrapPending = false;
        mRelayMode = false;
        mRelayPreapproved = false;
        mRelayInviteSent = false;
        mRelaySessionId.clear();
        mRelaySubjectTicket.clear();
        mRelayControllerTicket.clear();
        mRelayBrokerPath.clear();
        mRelayNegotiatePath.clear();
        mBootstrapSubjectId.clear();
        mBootstrapControllerId.clear();
        mBootstrapControllerName.clear();
        mBootstrapNonce.clear();
        mExpectedBootstrapControllerId.clear();
        mExpectedBootstrapControllerName.clear();
        mExpectedBootstrapNonce.clear();
        mReceiveBuffer.clear();
        mWriteBuffer.clear();
        WorldInteraction::instance().reset();
    }

    void LocalTransport::closeSocketOnly()
    {
        if (mSocket)
        {
            boost::system::error_code error;
            mSocket->shutdown(tcp::socket::shutdown_both, error);
            error.clear();
            mSocket->close(error);
            mSocket.reset();
        }

        mConnected = false;
        mPaired = false;
        mReceiveBuffer.clear();
        mWriteBuffer.clear();
    }

    void LocalTransport::resetConnectionState(bool keep_listener)
    {
        RemoteFeatures::instance().reset();
        if (mRelayMode)
        {
            RelayTransport::instance().disconnect();
        }
        closeSocketOnly();
        WorldInteraction::instance().reset();

        if (mRole == RemoteRole::Host)
        {
            RemoteActions::instance().stopMovement();
            if (RemoteSession::instance().isActive())
            {
                RemoteSession::instance().emergencyRelease();
            }
            RemoteCommandDispatcher::instance().reset();
        }
        else if (mRole == RemoteRole::Controller)
        {
            RemoteController::instance().end();
        }

        mPendingPairing = false;
        mPendingTrustedAutoAccept = false;
        mPendingControllerId.clear();
        mPendingControllerName.clear();

        // Dialog mirrors and remembered object/linkset context are scoped to
        // one possession connection. Never leave stale object menus alive
        // after a peer disconnect or failed handoff.
        WorldInteraction::instance().reset();

        if (!keep_listener)
        {
            mBootstrapPending = false;
            mBootstrapSubjectId.clear();
            mBootstrapControllerId.clear();
            mBootstrapControllerName.clear();
            mBootstrapNonce.clear();
            mExpectedBootstrapControllerId.clear();
            mExpectedBootstrapControllerName.clear();
            mExpectedBootstrapNonce.clear();
            mRelayMode = false;
            mRelayPreapproved = false;
            mRelayInviteSent = false;
            mRelaySessionId.clear();
            mRelaySubjectTicket.clear();
            mRelayControllerTicket.clear();
            mRelayBrokerPath.clear();
            mRelayNegotiatePath.clear();

            if (mAcceptor)
            {
                boost::system::error_code error;
                mAcceptor->close(error);
                mAcceptor.reset();
            }
            mListening = false;
            mRole = RemoteRole::None;
        }
    }

    void LocalTransport::handlePeerDisconnect(const std::string& reason)
    {
        const bool keep_listener =
            (!mRelayMode
             && mRole == RemoteRole::Host
             && mListening
             && !mPaired
             && mExpectedBootstrapControllerId.empty());
        resetConnectionState(keep_listener);
        mLastStatus = reason;
        FSCommon::report_to_nearby_chat("[Blazing Storm] " + reason);
    }

    void LocalTransport::showPairingPrompt()
    {
        if (!mPendingPairing)
        {
            return;
        }

        LLSD substitutions;
        substitutions["CONTROLLER"] =
            mPendingControllerName.empty() ? mPendingControllerId : mPendingControllerName;

        LLSD payload;
        payload["controller_id"] = mPendingControllerId;

        LLNotificationsUtil::add(
            "BlazingStormPossessionRequest",
            substitutions,
            payload,
            [this](const LLSD& notification, const LLSD& response)
            {
                if (!mPendingPairing
                    || notification["payload"]["controller_id"].asString()
                        != mPendingControllerId)
                {
                    return;
                }

                const S32 option =
                    LLNotificationsUtil::getSelectedOption(notification, response);
                if (option == 0)
                {
                    acceptPending();
                }
                else
                {
                    rejectPending();
                }
            });
    }

    void LocalTransport::tryAccept()
    {
        if (mRole != RemoteRole::Host || !mListening || mConnected || !mAcceptor)
        {
            return;
        }

        auto socket = std::make_unique<tcp::socket>(mIo);
        boost::system::error_code error;
        mAcceptor->accept(*socket, error);

        if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
        {
            return;
        }

        if (error)
        {
            mLastStatus = "Local pairing accept failed: " + error.message();
            return;
        }

        socket->non_blocking(true, error);
        if (error)
        {
            mLastStatus = "Could not make accepted socket nonblocking: " + error.message();
            return;
        }

        mSocket = std::move(socket);
        mConnected = true;
        mLastStatus = "Controller connected; waiting for pairing request.";
    }

    void LocalTransport::readAvailable()
    {
        if (!mSocket || !mConnected)
        {
            return;
        }

        std::array<char, 4096> buffer{};
        while (mSocket && mConnected)
        {
            boost::system::error_code error;
            const std::size_t count = mSocket->read_some(boost::asio::buffer(buffer), error);

            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
            {
                break;
            }

            if (error == boost::asio::error::eof)
            {
                handlePeerDisconnect("Remote viewer disconnected.");
                break;
            }

            if (error)
            {
                handlePeerDisconnect("Remote connection ended: " + error.message());
                break;
            }

            if (count == 0)
            {
                break;
            }

            mReceiveBuffer.append(buffer.data(), count);

            if (mReceiveBuffer.size() > 64 * 1024)
            {
                handlePeerDisconnect("Remote protocol buffer exceeded the safety limit.");
                break;
            }

            std::size_t newline = 0;
            while ((newline = mReceiveBuffer.find('\n')) != std::string::npos)
            {
                std::string line = mReceiveBuffer.substr(0, newline);
                mReceiveBuffer.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                if (!line.empty())
                {
                    processLine(line);
                }
                if (!mConnected)
                {
                    break;
                }
            }
        }
    }

    void LocalTransport::processLine(const std::string& line)
    {
        const auto fields = splitFields(line);
        if (fields.empty())
        {
            return;
        }

        if (mRole == RemoteRole::Host)
        {
            if (fields[0] == "HELLO" && fields.size() == 4 && !mPaired && !mPendingPairing)
            {
                if (fields[1] != mPairingCode)
                {
                    queueLine("REJECT|" + hexEncode("Incorrect pairing code."));
                    flushWrites();
                    closeSocketOnly();
                    mLastStatus = "Rejected a controller with an incorrect pairing code.";
                    return;
                }

                std::string controller_name;
                if (!hexDecode(fields[3], controller_name))
                {
                    queueLine("REJECT|" + hexEncode("Malformed controller identity."));
                    flushWrites();
                    closeSocketOnly();
                    return;
                }

                mPendingControllerId = fields[2];
                mPendingControllerName = controller_name;
                mPendingPairing = true;

                if (TrustStore::instance().find(mPendingControllerId))
                {
                    mPendingTrustedAutoAccept = true;
                    mLastStatus =
                        "Trusted controller " + controller_name + " auto-accepted.";
                    acceptPending();
                }
                else
                {
                    mPendingTrustedAutoAccept = false;
                    mLastStatus =
                        "Pairing request from " + controller_name + " is waiting for approval.";
                    queueLine("WAIT");
                    showPairingPrompt();
                }
                return;
            }

            if (fields[0] == "REQUEST" && fields.size() == 4 && !mPaired && !mPendingPairing)
            {
                std::string controller_name;
                if (!hexDecode(fields[2], controller_name)
                    || mExpectedBootstrapControllerId.empty()
                    || fields[1] != mExpectedBootstrapControllerId
                    || fields[3] != mExpectedBootstrapNonce
                    || std::chrono::steady_clock::now() >= mBootstrapDeadline)
                {
                    queueLine("REJECT|" + hexEncode("Bootstrap identity or nonce did not match the SL IM request."));
                    flushWrites();
                    resetConnectionState(false);
                    return;
                }

                mPendingControllerId = fields[1];
                mPendingControllerName = controller_name;
                mPendingPairing = true;

                if (mRelayMode && mRelayPreapproved)
                {
                    mLastStatus =
                        "Approved Controller connected through Azure relay.";
                    acceptPending();
                }
                else if (TrustStore::instance().find(mPendingControllerId))
                {
                    mPendingTrustedAutoAccept = true;
                    mLastStatus =
                        "Trusted controller " + controller_name + " auto-accepted.";
                    acceptPending();
                }
                else
                {
                    mPendingTrustedAutoAccept = false;
                    mLastStatus =
                        "Possession request from " + controller_name + " is waiting for approval.";
                    queueLine("WAIT");
                    showPairingPrompt();
                }
                return;
            }

            if (fields[0] == "CMD" && fields.size() == 5 && mPaired)
            {
                std::uint64_t sequence = 0;
                if (!parseSequence(fields[1], sequence) || sequence == 0)
                {
                    return;
                }

                RemoteCommand command;
                command.sequence = sequence;
                command.type = commandType(fields[2]);
                command.targetId = fields[3];

                if (!hexDecode(fields[4], command.text))
                {
                    return;
                }

                const bool dispatched = RemoteCommandDispatcher::instance().dispatch(command);
                if (!dispatched)
                {
                    queueLine("BLOCKED|" + std::to_string(sequence));
                }
                else if (command.type == RemoteCommandType::RestrictMovementOn
                      || command.type == RemoteCommandType::RestrictMovementOff
                      || command.type == RemoteCommandType::RestrictNearbyChatOn
                      || command.type == RemoteCommandType::RestrictNearbyChatOff
                      || command.type == RemoteCommandType::RestrictInstantMessageOn
                      || command.type == RemoteCommandType::RestrictInstantMessageOff)
                {
                    queueLine("APPLIED|" + commandName(command.type));
                }

                if (command.type == RemoteCommandType::EmergencyRelease
                    && !RemoteSession::instance().isActive())
                {
                    queueLine("ENDED");
                    flushWrites();
                    handlePeerDisconnect("Possession ended by controller.");
                }
                return;
            }
        }
        else if (mRole == RemoteRole::Controller)
        {
            if (fields[0] == "WAIT")
            {
                mLastStatus = "Pairing request sent; waiting for subject approval.";
                return;
            }

            if (fields[0] == "ACCEPT" && fields.size() == 2)
            {
                RemoteController::instance().begin(
                    fields[1],
                    mRelayMode && !mBootstrapSubjectId.empty()
                        ? mBootstrapSubjectId
                        : "local-subject");
                mPaired = true;
                mLastStatus = "Subject accepted. Remote control session is active.";
                FSCommon::report_to_nearby_chat("[Blazing Storm] Subject accepted the possession session.");
                return;
            }

            if (fields[0] == "REJECT" && fields.size() == 2)
            {
                std::string reason;
                hexDecode(fields[1], reason);
                mLastStatus = reason.empty() ? "Pairing request rejected." : reason;
                FSCommon::report_to_nearby_chat("[Blazing Storm] " + mLastStatus);
                resetConnectionState(false);
                return;
            }

            if (fields[0] == "EVT" && fields.size() == 3 && fields[1] == "FEATURES" && mPaired)
            {
                std::string data;
                if (fields[2].size() <= 56000 && hexDecode(fields[2], data))
                    RemoteFeatures::instance().receiveSnapshot(data);
                return;
            }
            if (fields[0] == "EVT" && fields.size() == 3 && fields[1] == "THOUGHT")
            {
                std::string thought;
                if (hexDecode(fields[2], thought))
                {
                    FSCommon::report_to_nearby_chat("[Subject thought] " + thought);
                }
                return;
            }

            if (fields[0] == "EVT" && fields.size() == 8 && fields[1] == "DIALOG")
            {
                ForwardedScriptDialog dialog;
                dialog.dialogId = fields[2];
                dialog.sourceObjectId = fields[3];
                dialog.rootObjectId = fields[4];

                if (!hexDecode(fields[5], dialog.objectName)
                    || !hexDecode(fields[6], dialog.message))
                {
                    return;
                }

                if (!mPaired || !RemoteController::instance().isActive()) return;
                std::size_t start = 0;
                for (;;)
                {
                    const auto end = fields[7].find(',', start);
                    const std::string encoded_button = fields[7].substr(start, end - start);
                    std::string button;
                    if (!hexDecode(encoded_button, button) || dialog.buttons.size() >= 12) return;
                    dialog.buttons.push_back(button);
                    if (end == std::string::npos) break;
                    start = end + 1;
                }

                if (!dialog.buttons.empty())
                {
                    WorldInteraction::instance().showForwardedDialog(dialog);
                }
                return;
            }

            if (fields[0] == "EVT" && fields.size() == 3 && fields[1] == "DIALOGCLOSED")
            {
                WorldInteraction::instance().closeForwardedDialog(fields[2]);
                return;
            }

            if (fields[0] == "BLOCKED" && fields.size() == 2)
            {
                FSCommon::report_to_nearby_chat(
                    "[Blazing Storm] Remote command " + fields[1] + " was blocked by subject permissions.");
                return;
            }

            if (fields[0] == "APPLIED" && fields.size() == 2)
            {
                FSCommon::report_to_nearby_chat(
                    "[Blazing Storm] Subject restriction applied: " + fields[1]);
                return;
            }

            if (fields[0] == "ENDED")
            {
                FSCommon::report_to_nearby_chat("[Blazing Storm] Possession session ended.");
                resetConnectionState(false);
                return;
            }
        }
    }

    void LocalTransport::queueLine(const std::string& line)
    {
        if (mRelayMode)
        {
            if (mConnected
                && !RelayTransport::instance().sendApplication(line))
            {
                handlePeerDisconnect(
                    "Could not queue the Azure relay message.");
            }
            return;
        }

        if (!mSocket || !mConnected) return;
        mWriteBuffer += line;
        mWriteBuffer.push_back('\n');
    }

    void LocalTransport::flushWrites()
    {
        if (mRelayMode) return;

        if (!mSocket || !mConnected)
        {
            return;
        }

        while (!mWriteBuffer.empty() && mSocket && mConnected)
        {
            boost::system::error_code error;
            const std::size_t written =
                mSocket->write_some(boost::asio::buffer(mWriteBuffer.data(), mWriteBuffer.size()), error);

            if (error == boost::asio::error::would_block || error == boost::asio::error::try_again)
            {
                return;
            }

            if (error)
            {
                handlePeerDisconnect("Remote connection write failed: " + error.message());
                return;
            }

            mWriteBuffer.erase(0, written);
        }
    }

    void LocalTransport::update()
    {
        if (mRelayMode)
        {
            updateRelay();
        }
        else
        {
            tryBootstrapConnect();
        }

        if (!mRelayMode
            && mRole == RemoteRole::Host
            && mListening
            && !mConnected
            && !mExpectedBootstrapControllerId.empty()
            && std::chrono::steady_clock::now() >= mBootstrapDeadline)
        {
            disconnect();
            mLastStatus =
                "Blazing Storm bootstrap listener expired without a matching controller connection.";
            return;
        }

        if (!mRelayMode)
        {
            tryAccept();
            flushWrites();
            readAvailable();
        }

        if (mRole == RemoteRole::Host && mPaired && mConnected)
        {
            const auto features = RemoteFeatures::instance().snapshot();
            if (!features.empty() && features.size() <= 28000)
                queueLine("EVT|FEATURES|" + hexEncode(features));
            auto events = RemoteEvents::instance().takeAll();
            for (const auto& event : events)
            {
                // Do not mirror third-party incoming IM content. Thoughts are
                // authored by the subject and are safe to send to the controller.
                if (event.type == RemoteEventType::Thought)
                {
                    queueLine("EVT|THOUGHT|" + hexEncode(event.text));
                }
            }

            for (const auto& dialog :
                 WorldInteraction::instance().takeDialogsToForward())
            {
                std::string encoded_buttons;
                for (std::size_t i = 0; i < dialog.buttons.size(); ++i)
                {
                    if (i != 0)
                    {
                        encoded_buttons.push_back(',');
                    }
                    encoded_buttons += hexEncode(dialog.buttons[i]);
                }

                queueLine("EVT|DIALOG|"
                    + dialog.dialogId + "|"
                    + dialog.sourceObjectId + "|"
                    + dialog.rootObjectId + "|"
                    + hexEncode(dialog.objectName) + "|"
                    + hexEncode(dialog.message) + "|"
                    + encoded_buttons);
            }

            for (const auto& dialog_id :
                 WorldInteraction::instance().takeDialogClosures())
            {
                queueLine("EVT|DIALOGCLOSED|" + dialog_id);
            }

            flushWrites();
        }
    }

    bool LocalTransport::sendCommand(const RemoteCommand& command)
    {
        if (mRole != RemoteRole::Controller || !mConnected || !mPaired)
        {
            mLastStatus = "No accepted controller session is active.";
            return false;
        }

        const std::string name = commandName(command.type);
        if (name.empty())
        {
            return false;
        }

        queueLine("CMD|" + std::to_string(command.sequence)
            + "|" + name
            + "|" + command.targetId
            + "|" + hexEncode(command.text));
        flushWrites();
        return mConnected;
    }

    std::string LocalTransport::hexEncode(const std::string& value)
    {
        static constexpr char digits[] = "0123456789ABCDEF";
        std::string result;
        result.reserve(value.size() * 2);

        for (const unsigned char byte : value)
        {
            result.push_back(digits[(byte >> 4) & 0x0F]);
            result.push_back(digits[byte & 0x0F]);
        }
        return result;
    }

    bool LocalTransport::hexDecode(const std::string& value, std::string& decoded)
    {
        if ((value.size() % 2) != 0)
        {
            return false;
        }

        auto nibble = [](char c) -> int
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + c - 'a';
            if (c >= 'A' && c <= 'F') return 10 + c - 'A';
            return -1;
        };

        decoded.clear();
        decoded.reserve(value.size() / 2);

        for (std::size_t i = 0; i < value.size(); i += 2)
        {
            const int high = nibble(value[i]);
            const int low = nibble(value[i + 1]);
            if (high < 0 || low < 0)
            {
                decoded.clear();
                return false;
            }
            decoded.push_back(static_cast<char>((high << 4) | low));
        }

        return true;
    }

    std::string LocalTransport::commandName(RemoteCommandType type)
    {
        switch (type)
        {
            case RemoteCommandType::InventoryBrowse: return "inventory-browse";
            case RemoteCommandType::InventoryWear: return "inventory-wear";
            case RemoteCommandType::InventoryRemove: return "inventory-remove";
            case RemoteCommandType::InventoryRez: return "inventory-rez";
            case RemoteCommandType::TeleportLocation: return "teleport-location";
            case RemoteCommandType::TeleportOffer: return "teleport-offer";
            case RemoteCommandType::TeleportRequest: return "teleport-request";
            case RemoteCommandType::TeleportAccept: return "teleport-accept";
            case RemoteCommandType::TeleportDecline: return "teleport-decline";
            case RemoteCommandType::CameraLeft: return "camera-left";
            case RemoteCommandType::CameraRight: return "camera-right";
            case RemoteCommandType::CameraUp: return "camera-up";
            case RemoteCommandType::CameraDown: return "camera-down";
            case RemoteCommandType::CameraIn: return "camera-in";
            case RemoteCommandType::CameraOut: return "camera-out";
            case RemoteCommandType::CameraReset: return "camera-reset";
            case RemoteCommandType::CameraFocus: return "camera-focus";
            case RemoteCommandType::MoveForward:       return "forward";
            case RemoteCommandType::MoveBackward:      return "back";
            case RemoteCommandType::StrafeLeft:        return "strafeleft";
            case RemoteCommandType::StrafeRight:       return "straferight";
            case RemoteCommandType::TurnLeft:          return "turnleft";
            case RemoteCommandType::TurnRight:         return "turnright";
            case RemoteCommandType::MoveUp:            return "moveup";
            case RemoteCommandType::MoveDown:          return "movedown";
            case RemoteCommandType::FlyOn:             return "fly-on";
            case RemoteCommandType::FlyOff:            return "fly-off";
            case RemoteCommandType::ToggleFly:         return "fly-toggle";
            case RemoteCommandType::Jump:              return "jump";
            case RemoteCommandType::StopForward:       return "stop-forward";
            case RemoteCommandType::StopStrafe:        return "stop-strafe";
            case RemoteCommandType::StopTurn:          return "stop-turn";
            case RemoteCommandType::StopVertical:      return "stop-vertical";
            case RemoteCommandType::Stop:              return "stop";
            case RemoteCommandType::Say:               return "say";
            case RemoteCommandType::SendInstantMessage:       return "im";
            case RemoteCommandType::SitObject:                 return "sit-object";
            case RemoteCommandType::Stand:                     return "stand";
            case RemoteCommandType::TouchObject:               return "touch-object";
            case RemoteCommandType::DialogReply:               return "dialog-reply";
            case RemoteCommandType::RestrictMovementOn:        return "restrictmovement-on";
            case RemoteCommandType::RestrictMovementOff:       return "restrictmovement-off";
            case RemoteCommandType::RestrictNearbyChatOn:      return "restrictchat-on";
            case RemoteCommandType::RestrictNearbyChatOff:     return "restrictchat-off";
            case RemoteCommandType::RestrictInstantMessageOn:  return "restrictim-on";
            case RemoteCommandType::RestrictInstantMessageOff: return "restrictim-off";
            case RemoteCommandType::EmergencyRelease:          return "release";
            default:                                   return {};
        }
    }

    RemoteCommandType LocalTransport::commandType(const std::string& name)
    {
        if (name == "inventory-browse") return RemoteCommandType::InventoryBrowse;
        if (name == "inventory-wear") return RemoteCommandType::InventoryWear;
        if (name == "inventory-remove") return RemoteCommandType::InventoryRemove;
        if (name == "inventory-rez") return RemoteCommandType::InventoryRez;
        if (name == "teleport-location") return RemoteCommandType::TeleportLocation;
        if (name == "teleport-offer") return RemoteCommandType::TeleportOffer;
        if (name == "teleport-request") return RemoteCommandType::TeleportRequest;
        if (name == "teleport-accept") return RemoteCommandType::TeleportAccept;
        if (name == "teleport-decline") return RemoteCommandType::TeleportDecline;
        if (name == "camera-left") return RemoteCommandType::CameraLeft;
        if (name == "camera-right") return RemoteCommandType::CameraRight;
        if (name == "camera-up") return RemoteCommandType::CameraUp;
        if (name == "camera-down") return RemoteCommandType::CameraDown;
        if (name == "camera-in") return RemoteCommandType::CameraIn;
        if (name == "camera-out") return RemoteCommandType::CameraOut;
        if (name == "camera-reset") return RemoteCommandType::CameraReset;
        if (name == "camera-focus") return RemoteCommandType::CameraFocus;
        if (name == "forward")     return RemoteCommandType::MoveForward;
        if (name == "back")        return RemoteCommandType::MoveBackward;
        if (name == "strafeleft")  return RemoteCommandType::StrafeLeft;
        if (name == "straferight") return RemoteCommandType::StrafeRight;
        if (name == "turnleft")     return RemoteCommandType::TurnLeft;
        if (name == "turnright")    return RemoteCommandType::TurnRight;
        if (name == "moveup")       return RemoteCommandType::MoveUp;
        if (name == "movedown")     return RemoteCommandType::MoveDown;
        if (name == "fly-on")       return RemoteCommandType::FlyOn;
        if (name == "fly-off")      return RemoteCommandType::FlyOff;
        if (name == "fly-toggle")   return RemoteCommandType::ToggleFly;
        if (name == "jump")         return RemoteCommandType::Jump;
        if (name == "stop-forward") return RemoteCommandType::StopForward;
        if (name == "stop-strafe")  return RemoteCommandType::StopStrafe;
        if (name == "stop-turn")     return RemoteCommandType::StopTurn;
        if (name == "stop-vertical") return RemoteCommandType::StopVertical;
        if (name == "stop")          return RemoteCommandType::Stop;
        if (name == "say")         return RemoteCommandType::Say;
        if (name == "im")                   return RemoteCommandType::SendInstantMessage;
        if (name == "sit-object")           return RemoteCommandType::SitObject;
        if (name == "stand")                return RemoteCommandType::Stand;
        if (name == "touch-object")         return RemoteCommandType::TouchObject;
        if (name == "dialog-reply")         return RemoteCommandType::DialogReply;
        if (name == "restrictmovement-on")   return RemoteCommandType::RestrictMovementOn;
        if (name == "restrictmovement-off")  return RemoteCommandType::RestrictMovementOff;
        if (name == "restrictchat-on")       return RemoteCommandType::RestrictNearbyChatOn;
        if (name == "restrictchat-off") return RemoteCommandType::RestrictNearbyChatOff;
        if (name == "restrictim-on")    return RemoteCommandType::RestrictInstantMessageOn;
        if (name == "restrictim-off")   return RemoteCommandType::RestrictInstantMessageOff;
        if (name == "release")          return RemoteCommandType::EmergencyRelease;
        return RemoteCommandType::None;
    }
}

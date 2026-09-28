/**
 * @file bslocaltransport.cpp
 * @brief Loopback-only transport for testing two Blazing Storm viewers.
 */

#include "llviewerprecompiledheaders.h"

#include "blazingstorm/remote/bslocaltransport.h"

#include "blazingstorm/remote/bsremoteactions.h"
#include "blazingstorm/remote/bsremotecommanddispatcher.h"
#include "blazingstorm/remote/bsremotecontroller.h"
#include "blazingstorm/remote/bsremoteevents.h"
#include "blazingstorm/remote/bsremotesession.h"
#include "blazingstorm/remote/bstruststore.h"
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

#include <array>
#include <charconv>
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
        const std::string bootstrap_message =
            buildBootstrapMessage(controller_id, nonce);
        const LLUUID im_session_id =
            LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, subject_uuid);

        send_simple_im(
            subject_uuid,
            bootstrap_message,
            IM_NOTHING_SPECIAL,
            im_session_id);

        mRole = RemoteRole::Controller;
        mBootstrapPending = true;
        mBootstrapSubjectId = subject_id;
        mBootstrapControllerId = controller_id;
        mBootstrapControllerName = controller_name;
        mBootstrapNonce = nonce;

        const auto now = std::chrono::steady_clock::now();
        mNextBootstrapAttempt = now + std::chrono::milliseconds(250);
        mBootstrapDeadline = now + std::chrono::seconds(15);

        mLastStatus =
            "Bootstrap IM sent through Second Life; waiting for the subject viewer to open its local listener.";
        return true;
    }

    bool LocalTransport::handleBootstrapInstantMessage(
        const std::string& from_id,
        const std::string& from_name,
        const std::string& message,
        bool online)
    {
        if (!online
            || gAgentID.isNull()
            || mRole != RemoteRole::None
            || RemoteSession::instance().isActive()
            || RemoteController::instance().isActive())
        {
            return false;
        }

        std::string embedded_controller_id;
        std::string nonce;
        if (!parseBootstrapMessage(message, embedded_controller_id, nonce)
            || embedded_controller_id != from_id)
        {
            return false;
        }

        LLUUID from_uuid(from_id);
        if (from_uuid.isNull())
        {
            return false;
        }

        if (!startHost(portForAvatarId(gAgentID.asString())))
        {
            return false;
        }

        mExpectedBootstrapControllerId = from_id;
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
        if (mRole != RemoteRole::Host || !mConnected || !mPendingPairing || mPaired)
        {
            mLastStatus = "There is no pending local pairing request to accept.";
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
        mPaired = true;
        mExpectedBootstrapControllerId.clear();
        mExpectedBootstrapNonce.clear();
        mLastStatus = "Controller accepted. Possession session is active.";

        queueLine("ACCEPT|" + session_id.asString());
        flushWrites();

        announcePossessionAccepted(trusted_auto_accept);
        return true;
    }

    void LocalTransport::rejectPending()
    {
        if (mRole != RemoteRole::Host || !mConnected || !mPendingPairing)
        {
            mLastStatus = "There is no pending local pairing request to reject.";
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
        mExpectedBootstrapNonce.clear();
        mLastStatus = "Pairing request rejected; local bootstrap listener closed.";
    }

    void LocalTransport::disconnect()
    {
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
        mBootstrapSubjectId.clear();
        mBootstrapControllerId.clear();
        mBootstrapControllerName.clear();
        mBootstrapNonce.clear();
        mExpectedBootstrapControllerId.clear();
        mExpectedBootstrapNonce.clear();
        mReceiveBuffer.clear();
        mWriteBuffer.clear();
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
        closeSocketOnly();

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
        mPendingControllerId.clear();
        mPendingControllerName.clear();

        if (!keep_listener)
        {
            mBootstrapPending = false;
            mBootstrapSubjectId.clear();
            mBootstrapControllerId.clear();
            mBootstrapControllerName.clear();
            mBootstrapNonce.clear();
            mExpectedBootstrapControllerId.clear();
            mExpectedBootstrapNonce.clear();

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
            (mRole == RemoteRole::Host
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

                if (TrustStore::instance().find(mPendingControllerId))
                {
                    mLastStatus =
                        "Trusted controller " + controller_name + " auto-accepted.";
                    acceptPending();
                }
                else
                {
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
                if (!parseSequence(fields[1], sequence))
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
                RemoteController::instance().begin(fields[1], "local-subject");
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

            if (fields[0] == "EVT" && fields.size() == 3 && fields[1] == "THOUGHT")
            {
                std::string thought;
                if (hexDecode(fields[2], thought))
                {
                    FSCommon::report_to_nearby_chat("[Subject thought] " + thought);
                }
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
        if (!mSocket || !mConnected)
        {
            return;
        }
        mWriteBuffer += line;
        mWriteBuffer.push_back('\n');
    }

    void LocalTransport::flushWrites()
    {
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
        tryBootstrapConnect();

        if (mRole == RemoteRole::Host
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

        tryAccept();
        flushWrites();
        readAvailable();

        if (mRole == RemoteRole::Host && mPaired && mConnected)
        {
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
        if (name == "forward")     return RemoteCommandType::MoveForward;
        if (name == "back")        return RemoteCommandType::MoveBackward;
        if (name == "strafeleft")  return RemoteCommandType::StrafeLeft;
        if (name == "straferight") return RemoteCommandType::StrafeRight;
        if (name == "turnleft")    return RemoteCommandType::TurnLeft;
        if (name == "turnright")   return RemoteCommandType::TurnRight;
        if (name == "jump")         return RemoteCommandType::Jump;
        if (name == "stop-forward") return RemoteCommandType::StopForward;
        if (name == "stop-strafe")  return RemoteCommandType::StopStrafe;
        if (name == "stop-turn")     return RemoteCommandType::StopTurn;
        if (name == "stop-vertical") return RemoteCommandType::StopVertical;
        if (name == "stop")          return RemoteCommandType::Stop;
        if (name == "say")         return RemoteCommandType::Say;
        if (name == "im")                   return RemoteCommandType::SendInstantMessage;
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

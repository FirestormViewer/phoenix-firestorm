#pragma once

#include "lluuid.h"
#include "llvksessionowner.h"
#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

class LLVKChatProtocol final
{
public:
    using Bytes=std::vector<std::uint8_t>;
    struct Context
    {
        LLVKSessionOwner::Tag tag;
        LLUUID agent;
        std::string name,regionName;
    };
    struct Resident
    {
        LLUUID id;
        std::string displayName,username;
    };
    struct Friend
    {
        LLUUID id;
        std::uint32_t rightsGiven=0,rightsHeld=0;
        bool online=false,presenceReceived=false;
    };
    struct Presence { LLUUID id; bool online=false; };
    struct Group
    {
        enum class State { Closed, Joining, Joined, Failed };
        struct Participant { bool moderator=false,textMuted=false; };
        LLUUID id;
        std::string name;
        std::uint64_t powers=0;
        State state=State::Closed;
        std::string error;
        std::map<LLUUID,Participant> participants;
        bool acceptNotices=true;
        bool moderationPending=false;
    };
    struct SearchResult
    {
        std::uint64_t query=0;
        std::vector<Resident> residents;
        std::string error;
    };
    struct Message
    {
        enum class Kind { Local, Instant };
        Kind kind=Kind::Local;
        LLUUID sender,owner,recipient,conversation,region;
        std::string name,text;
        std::array<float,3> position{};
        Bytes bucket;
        std::uint32_t timestamp=0;
        std::uint8_t sourceType=0,chatType=1,audible=1,dialog=0,offline=0;
        bool fromGroup=false;
    };
    static LLUUID directSession(const LLUUID& agent,const LLUUID& recipient);
    static std::optional<std::vector<Friend>> decodeFriends(const LLSD& list,std::string& error);
    static std::optional<std::vector<Presence>> decodePresence(std::span<const std::uint8_t> payload,bool online,std::string& error);
    static std::optional<std::vector<Group>> decodeGroups(const LLSD& body,const LLUUID& agent,std::string& error);
    static bool updateParticipants(Group& group,const LLSD& body,bool initial,std::string& error);
    static std::optional<Message> decodeInvitation(const LLSD& body,std::string& error);
    static std::optional<Bytes> local(const LLUUID& agent,const LLUUID& session,
        const std::string& text,std::uint8_t type,std::int32_t channel,std::string& error);
    static std::optional<Bytes> instant(const LLUUID& agent,const LLUUID& session,
        const Message& message,std::string& error);
    static std::optional<Message> decodeLocal(std::span<const std::uint8_t> payload,std::string& error);
    static std::optional<Message> decodeInstant(std::span<const std::uint8_t> payload,std::string& error);
    static std::optional<Message> decodeInstantEvent(const LLSD& body,std::string& error);
};
#include "linden_common.h"
#include "llvkchatprotocol.h"
#include <boost/endian/conversion.hpp>
#include <bit>
#include <cmath>

namespace
{
    using Bytes=LLVKChatProtocol::Bytes;
    bool validText(const std::string& text,std::size_t limit)
    {
        return text.size()<=limit && text.find('\0')==text.npos && wstring_to_utf8str(utf8str_to_wstring(text))==text;
    }
    void uuid(Bytes& bytes,const LLUUID& value) { bytes.insert(bytes.end(),value.mData,value.mData+16); }
    void integer(Bytes& bytes,std::uint32_t value)
    {
        const auto offset=bytes.size(); bytes.resize(offset+4);
        boost::endian::store_little_u32(bytes.data()+offset,value);
    }
    void variable(Bytes& bytes,std::span<const std::uint8_t> data,bool shortLength)
    {
        bytes.push_back(static_cast<std::uint8_t>(data.size()));
        if (!shortLength) bytes.push_back(static_cast<std::uint8_t>(data.size()>>8));
        bytes.insert(bytes.end(),data.begin(),data.end());
    }
    void text(Bytes& bytes,const std::string& value,bool shortLength)
    {
        const auto data=std::span(reinterpret_cast<const std::uint8_t*>(value.c_str()),value.size()+1);
        variable(bytes,data,shortLength);
    }
    struct Reader
    {
        std::span<const std::uint8_t> data;
        std::size_t offset=0;
        bool good=true;
        auto take(std::size_t count)
        {
            if (!good || count>data.size()-offset) { good=false; return std::span<const std::uint8_t>{}; }
            auto result=data.subspan(offset,count); offset+=count; return result;
        }
        std::uint8_t byte() { const auto value=take(1); return value.empty() ? 0 : value[0]; }
        std::uint32_t integer()
        { const auto value=take(4); return value.empty() ? 0 : boost::endian::load_little_u32(value.data()); }
        LLUUID uuid()
        {
            LLUUID result; const auto value=take(16);
            if (!value.empty()) std::copy(value.begin(),value.end(),result.mData);
            return result;
        }
        std::span<const std::uint8_t> variable(bool shortLength)
        {
            std::size_t count=byte();
            if (!shortLength) count|=std::size_t(byte())<<8;
            return take(count);
        }
        std::string text(bool shortLength,std::size_t limit)
        {
            auto value=variable(shortLength);
            if (!value.empty() && value.back()==0) value=value.first(value.size()-1);
            std::string result(value.begin(),value.end());
            if (!validText(result,limit)) good=false;
            return result;
        }
        std::array<float,3> position()
        {
            std::array<float,3> result;
            for (auto& component : result)
            { component=std::bit_cast<float>(integer()); if (!std::isfinite(component)) good=false; }
            return result;
        }
        bool done() const { return good && offset==data.size(); }
    };
}

LLUUID LLVKChatProtocol::directSession(const LLUUID& agent,const LLUUID& recipient)
{ return agent==recipient ? agent : agent^recipient; }

std::optional<std::vector<LLVKChatProtocol::Friend>> LLVKChatProtocol::decodeFriends(const LLSD& list,std::string& error)
{
    error.clear();
    std::vector<Friend> friends;
    if (list.isUndefined()) return friends;
    if (!list.isArray() || list.size()>4096) { error="Invalid native friend list"; return {}; }
    std::set<LLUUID> identities;
    for (auto iterator=list.beginArray(); iterator!=list.endArray(); ++iterator)
    {
        const auto& row=*iterator;
        const auto id=row["buddy_id"].asUUID();
        if (!row.isMap() || id.isNull() || !identities.insert(id).second ||
            (row.has("buddy_rights_given") && !row["buddy_rights_given"].isInteger()) ||
            (row.has("buddy_rights_has") && !row["buddy_rights_has"].isInteger()))
        { error="Invalid native friend identity or permissions"; return {}; }
        friends.push_back({id,static_cast<std::uint32_t>(row["buddy_rights_given"].asInteger()),
            static_cast<std::uint32_t>(row["buddy_rights_has"].asInteger())});
    }
    return friends;
}

std::optional<std::vector<LLVKChatProtocol::Presence>> LLVKChatProtocol::decodePresence(
    std::span<const std::uint8_t> payload,bool online,std::string& error)
{
    error.clear();
    if (payload.empty() || payload.size()!=1+std::size_t(payload[0])*16)
    { error="Invalid native presence notification"; return {}; }
    Reader reader{payload}; const auto count=reader.byte();
    std::vector<Presence> result;
    for (unsigned index=0; index<count; ++index)
    {
        const auto id=reader.uuid();
        if (id.isNull()) { error="Invalid native presence identity"; return {}; }
        result.push_back({id,online});
    }
    return result;
}

std::optional<std::vector<LLVKChatProtocol::Group>> LLVKChatProtocol::decodeGroups(const LLSD& input,const LLUUID& agent,std::string& error)
{
    error.clear();
    const auto& body=input.has("body") ? input["body"] : input;
    if (!body.isMap() || !body["AgentData"].isArray() || body["AgentData"].size()!=1 ||
        body["AgentData"][0]["AgentID"].asUUID()!=agent || agent.isNull() || !body["GroupData"].isArray() || body["GroupData"].size()>256)
    { error="Invalid native group membership event"; return {}; }
    std::vector<Group> groups;
    const auto& rows=body["GroupData"];
    for (auto iterator=rows.beginArray(); iterator!=rows.endArray(); ++iterator)
    {
        const auto& row=*iterator;
        const auto id=row["GroupID"].asUUID();
        if (id.isNull()) continue;
        const auto name=row["GroupName"].asString();
        const auto& powers=row["GroupPowers"].asBinary();
        if (!row.isMap() || name.empty() || !validText(name,255) || powers.size()!=8 ||
            std::any_of(groups.begin(),groups.end(),[&](const auto& group) { return group.id==id; }))
        { error="Invalid native group membership data"; return {}; }
        groups.push_back({id,name,boost::endian::load_big_u64(powers.data())});
        groups.back().acceptNotices=row["AcceptNotices"].asBoolean();
    }
    return groups;
}

std::optional<LLVKChatProtocol::Message> LLVKChatProtocol::decodeInvitation(const LLSD& body,std::string& error)
{
    error.clear();
    const auto& data=body["instantmessage"]["message_params"];
    Message message;
    message.kind=Message::Kind::Instant; message.dialog=13;
    message.sender=data["from_id"].asUUID(); message.conversation=data["id"].asUUID();
    message.recipient=data["to_id"].asUUID(); message.region=data["region_id"].asUUID();
    message.name=data["from_name"].asString(); message.text=data["message"].asString();
    const auto offline=data["offline"].asInteger();
    if (!data.isMap() || message.sender.isNull() || message.conversation.isNull() ||
        !validText(message.name,254) || !validText(message.text,65534) || offline<0 || offline>1)
    { error="Invalid native group invitation"; return {}; }
    message.offline=static_cast<std::uint8_t>(offline);
    message.timestamp=static_cast<std::uint32_t>(data["timestamp"].asInteger());
    return message;
}

bool LLVKChatProtocol::updateParticipants(Group& group,const LLSD& body,bool initial,std::string& error)
{
    error.clear();
    auto participants=initial ? decltype(group.participants){} : group.participants;
    const auto applyInfo=[&](Group::Participant& participant,const LLSD& info)
    {
        if (!info.isMap()) return false;
        if (info.has("is_moderator")) participant.moderator=info["is_moderator"].asBoolean();
        if (info.has("mutes")) participant.textMuted=info["mutes"]["text"].asBoolean();
        return true;
    };
    const auto& rows=initial ? body["agent_info"] : body["agent_updates"];
    if (rows.isMap())
    {
        if (rows.size()>4096) { error="Group participant limit exceeded"; return false; }
        for (auto iterator=rows.beginMap(); iterator!=rows.endMap(); ++iterator)
        {
            LLUUID id;
            if (!id.set(iterator->first,false) || id.isNull() || !iterator->second.isMap())
            { error="Invalid group participant identity"; return false; }
            const auto& row=iterator->second;
            if (initial)
            {
                if (!applyInfo(participants[id],row)) { error="Invalid group participant data"; return false; }
                continue;
            }
            const auto transition=row["transition"].asString();
            if (transition=="LEAVE") { participants.erase(id); continue; }
            if (transition=="ENTER") participants.try_emplace(id);
            else if (!transition.empty()) { error="Invalid group participant transition"; return false; }
            const auto found=participants.find(id);
            if (found!=participants.end() && row.has("info") && !applyInfo(found->second,row["info"]))
            { error="Invalid group participant update"; return false; }
        }
    }
    else if (initial && body["agents"].isArray())
    {
        const auto& agents=body["agents"];
        if (agents.size()>4096) { error="Group participant limit exceeded"; return false; }
        for (auto iterator=agents.beginArray(); iterator!=agents.endArray(); ++iterator)
        {
            const auto id=iterator->asUUID();
            if (id.isNull()) { error="Invalid group participant identity"; return false; }
            participants.try_emplace(id);
        }
    }
    else if (!initial && body["updates"].isMap())
    {
        const auto& updates=body["updates"];
        if (updates.size()>4096) { error="Group participant limit exceeded"; return false; }
        for (auto iterator=updates.beginMap(); iterator!=updates.endMap(); ++iterator)
        {
            LLUUID id;
            if (!id.set(iterator->first,false) || id.isNull()) { error="Invalid group participant identity"; return false; }
            const auto transition=iterator->second.asString();
            if (transition=="LEAVE") participants.erase(id);
            else if (transition=="ENTER") participants.try_emplace(id);
            else { error="Invalid group participant transition"; return false; }
        }
    }
    else if (body.has(initial ? "agent_info" : "agent_updates"))
    { error="Invalid group participant list"; return false; }
    if (participants.size()>4096) { error="Group participant limit exceeded"; return false; }
    group.participants=std::move(participants);
    return true;
}

std::optional<LLVKChatProtocol::Bytes> LLVKChatProtocol::local(const LLUUID& agent,const LLUUID& session,
    const std::string& message,std::uint8_t type,std::int32_t channel,std::string& error)
{
    error.clear();
    if (agent.isNull() || session.isNull() || !validText(message,1023) || (type!=0 && type!=1 && type!=2))
    { error="Invalid native local chat request"; return {}; }
    Bytes bytes; uuid(bytes,agent); uuid(bytes,session); text(bytes,message,false);
    bytes.push_back(type); integer(bytes,static_cast<std::uint32_t>(channel));
    return bytes;
}

std::optional<LLVKChatProtocol::Bytes> LLVKChatProtocol::instant(const LLUUID& agent,const LLUUID& session,
    const Message& message,std::string& error)
{
    error.clear();
    if (agent.isNull() || session.isNull() || message.recipient.isNull() || !validText(message.name,254) ||
        !validText(message.text,1023) || message.bucket.size()>1024 || message.offline>1 ||
        (message.dialog!=0 && message.dialog!=15 && message.dialog!=17 && message.dialog!=18 && message.dialog!=41 && message.dialog!=42))
    { error="Invalid native instant message request"; return {}; }
    Bytes bytes; uuid(bytes,agent); uuid(bytes,session); bytes.push_back(message.fromGroup);
    uuid(bytes,message.recipient); integer(bytes,0); uuid(bytes,message.region);
    for (const auto component : message.position)
    {
        if (!std::isfinite(component)) { error="Invalid native message position"; return {}; }
        integer(bytes,std::bit_cast<std::uint32_t>(component));
    }
    bytes.push_back(message.offline); bytes.push_back(message.dialog); uuid(bytes,message.conversation);
    integer(bytes,message.timestamp); text(bytes,message.name,true); text(bytes,message.text,false);
    const Bytes emptyBucket{0}; variable(bytes,message.bucket.empty() ? emptyBucket : message.bucket,false);
    integer(bytes,0); bytes.push_back(0);
    if (bytes.size()>1400) { error="Native instant message exceeds packet budget"; return {}; }
    return bytes;
}

std::optional<LLVKChatProtocol::Message> LLVKChatProtocol::decodeLocal(std::span<const std::uint8_t> payload,std::string& error)
{
    error.clear();
    Reader reader{payload}; Message result;
    result.name=reader.text(true,254); result.sender=reader.uuid(); result.owner=reader.uuid();
    result.sourceType=reader.byte(); result.chatType=reader.byte(); result.audible=reader.byte();
    result.position=reader.position(); result.text=reader.text(false,65534);
    if (!reader.done()) { error="Malformed native local chat payload"; return {}; }
    return result;
}

std::optional<LLVKChatProtocol::Message> LLVKChatProtocol::decodeInstantEvent(const LLSD& body,std::string& error)
{
    error.clear();
    if (!body.isMap() || !body["AgentData"].isArray() || body["AgentData"].size()!=1 ||
        !body["MessageBlock"].isArray() || body["MessageBlock"].size()!=1)
    { error="Malformed native instant message event blocks"; return {}; }
    const auto& agent=body["AgentData"][0];
    const auto& block=body["MessageBlock"][0];
    const auto uuidField=[](const LLSD& value)
    {
        LLUUID parsed;
        return value.isUUID() || (value.isString() && parsed.set(value.asString(),false));
    };
    if (!agent.isMap() || !block.isMap() || !uuidField(agent["AgentID"]) || !uuidField(block["ToAgentID"]) ||
        !uuidField(block["ID"]) || !uuidField(block["RegionID"]) || !block["FromGroup"].isBoolean() ||
        !block["Offline"].isInteger() || block["Offline"].asInteger()<0 || block["Offline"].asInteger()>1 ||
        !block["Dialog"].isInteger() || block["Dialog"].asInteger()<0 || block["Dialog"].asInteger()>255 ||
        !block["FromAgentName"].isString() || !validText(block["FromAgentName"].asString(),254) ||
        !block["Message"].isString() || !validText(block["Message"].asString(),65534) ||
        !block["BinaryBucket"].isBinary() || block["BinaryBucket"].asBinary().size()>65535 ||
        !block["Timestamp"].isBinary() || block["Timestamp"].asBinary().size()!=4 ||
        !block["Position"].isArray() || block["Position"].size()!=3)
    { error="Malformed native instant message event fields"; return {}; }
    Message result; result.kind=Message::Kind::Instant;
    result.sender=agent["AgentID"].asUUID(); result.recipient=block["ToAgentID"].asUUID();
    result.conversation=block["ID"].asUUID(); result.region=block["RegionID"].asUUID();
    result.fromGroup=block["FromGroup"].asBoolean();
    result.offline=static_cast<std::uint8_t>(block["Offline"].asInteger());
    result.dialog=static_cast<std::uint8_t>(block["Dialog"].asInteger());
    result.name=block["FromAgentName"].asString(); result.text=block["Message"].asString();
    result.bucket=block["BinaryBucket"].asBinary();
    result.timestamp=boost::endian::load_big_u32(block["Timestamp"].asBinary().data());
    for (int index=0; index<3; ++index)
    {
        const auto& value=block["Position"][index];
        result.position[index]=static_cast<float>(value.asReal());
        if ((!value.isReal() && !value.isInteger()) || !std::isfinite(result.position[index]))
        { error="Malformed native instant message event position"; return {}; }
    }
    return result;
}

std::optional<LLVKChatProtocol::Message> LLVKChatProtocol::decodeInstant(std::span<const std::uint8_t> payload,std::string& error)
{
    error.clear();
    Reader reader{payload}; Message result; result.kind=Message::Kind::Instant;
    result.sender=reader.uuid(); reader.uuid(); result.fromGroup=reader.byte()!=0;
    result.recipient=reader.uuid(); reader.integer(); result.region=reader.uuid(); result.position=reader.position();
    result.offline=reader.byte(); result.dialog=reader.byte(); result.conversation=reader.uuid(); result.timestamp=reader.integer();
    result.name=reader.text(true,254); result.text=reader.text(false,65534);
    const auto bucket=reader.variable(false); result.bucket.assign(bucket.begin(),bucket.end());
    if (reader.good && reader.offset<payload.size())
    {
        reader.integer();
        if (reader.offset<payload.size())
        {
            const auto count=reader.byte();
            for (unsigned index=0; index<count && reader.good; ++index) reader.variable(false);
        }
    }
    if (!reader.done() || result.offline>1) { error="Malformed native instant message payload"; return {}; }
    return result;
}
#include "llvkmutelist.h"
#include "llstring.h"
#include <algorithm>
#include <sstream>

bool LLVKMuteList::staffName(std::string name)
{
    std::replace(name.begin(),name.end(),'.',' ');
    std::istringstream input(name);
    std::string first,last;
    if (!(input>>first>>last)) return false;
    LLStringUtil::toLower(last);
    return last=="linden";
}

bool LLVKMuteList::add(Entry entry,U32 flags,const LLUUID& self,std::size_t limit,std::string& error)
{
    error.clear();
    if (flags&~All) { error="Unsupported native mute flags"; return false; }
    if (entry.type==Type::Agent && staffName(entry.name) && (!flags || (flags&Text)))
    { error="Cannot block staff text chat"; return false; }
    if (entry.type==Type::Agent && entry.id==self) { error="Cannot block yourself"; return false; }
    if (mEntries.size()+mNames.size()>=limit) { error="Mute list limit reached"; return false; }
    if (entry.type==Type::Name)
    {
        if (entry.name.empty() || entry.id.notNull()) { error="Invalid by-name mute"; return false; }
        if (!mNames.insert(entry.name).second) { error="Duplicate by-name mute"; return false; }
        entry.allowed=0;
        mChanges.push_back({entry,false,true}); ++mRevision;
        return true;
    }
    const auto existing=mEntries.find(entry.id);
    const bool newlyAdded=existing==mEntries.end();
    entry.allowed=newlyAdded ? All : existing->second.allowed;
    entry.allowed=flags ? entry.allowed&~flags : 0;
    mEntries[entry.id]=entry;
    mChanges.push_back({entry,false,newlyAdded}); ++mRevision;
    return true;
}

bool LLVKMuteList::remove(const LLUUID& id,const std::string& name,U32 flags)
{
    if (flags&~All) return false;
    const auto found=mEntries.find(id);
    if (found!=mEntries.end())
    {
        auto entry=found->second;
        entry.allowed=flags ? entry.allowed|flags : All;
        const bool removed=entry.allowed==All;
        if (removed) mEntries.erase(found); else found->second=entry;
        mChanges.push_back({entry,removed,false}); ++mRevision;
        return true;
    }
    if (!mNames.erase(name)) return false;
    mChanges.push_back({Entry{LLUUID::null,name,Type::Name,0},true,false}); ++mRevision;
    return true;
}

bool LLVKMuteList::muted(const LLUUID& resolvedId,const std::string& name,U32 flags,const LLUUID& self) const
{
    if (resolvedId==self) return false;
    const auto found=mEntries.find(resolvedId);
    if (found!=mEntries.end()) return (flags&found->second.allowed)==0;
    return !name.empty() && mNames.contains(name);
}

std::vector<LLVKMuteList::Entry> LLVKMuteList::entries() const
{
    std::vector<Entry> result;
    result.reserve(mEntries.size()+mNames.size());
    for (const auto& [id,entry] : mEntries) result.push_back(entry);
    for (const auto& name : mNames) result.push_back({LLUUID::null,name,Type::Name,0});
    return result;
}

std::vector<LLVKMuteList::Change> LLVKMuteList::takeChanges()
{
    auto changes=std::move(mChanges); mChanges.clear(); return changes;
}
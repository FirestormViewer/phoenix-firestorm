#ifndef LLVKMUTELIST_H
#define LLVKMUTELIST_H

#include "llsd.h"
#include "lluuid.h"
#include <map>
#include <set>
#include <vector>

class LLVKMuteList final
{
public:
    enum class Type { Name, Agent, Object, Group, External };
    enum : U32 { Text=1, Voice=2, Particles=4, Sounds=8, All=15 };
    struct Entry { LLUUID id; std::string name; Type type=Type::Name; U32 allowed=0; };
    struct Change { Entry entry; bool removed=false, newlyAdded=false; };
    bool add(Entry entry, U32 flags, const LLUUID& self, std::size_t limit, std::string& error);
    bool remove(const LLUUID& id, const std::string& name, U32 flags=0);
    bool muted(const LLUUID& resolvedId, const std::string& name, U32 flags, const LLUUID& self) const;
    std::vector<Entry> entries() const;
    std::vector<Change> takeChanges();
    std::uint64_t revision() const noexcept { return mRevision; }
    static bool staffName(std::string name);
private:
    std::map<LLUUID,Entry> mEntries;
    std::set<std::string> mNames;
    std::vector<Change> mChanges;
    std::uint64_t mRevision=0;
};

#endif
#include "llvkmediafilter.h"
#include "llstring.h"

std::string LLVKMediaFilter::domain(std::string url)
{
    const auto protocol=url.find("//");
    if (protocol!=std::string::npos) url.erase(0,protocol+2);
    const auto path=url.find('/');
    if (path!=std::string::npos) url.resize(path);
    const auto credentials=url.find('@');
    if (credentials!=std::string::npos) url.erase(0,credentials+1);
    const auto port=url.find(':');
    if (port!=std::string::npos) url.resize(port);
    LLStringUtil::toLower(url);
    return url;
}

bool LLVKMediaFilter::load(const LLSD& rules,std::string& error)
{
    error.clear();
    if (rules.isUndefined()) { mRules=LLSD::emptyArray(); ++mRevision; return true; }
    if (!rules.isArray() || rules.size()>10000) { error="Invalid native media filter list"; return false; }
    std::size_t bytes=0;
    for (auto rule=rules.beginArray(); rule!=rules.endArray(); ++rule)
    {
        const auto action=(*rule)["action"].asString(),name=(*rule)["domain"].asString();
        bytes+=name.size();
        if (!rule->isMap() || name.empty() || name.size()>65536 || bytes>4*1024*1024 || (action!="allow" && action!="deny"))
        { error="Invalid native media filter rule"; return false; }
    }
    mRules=rules; ++mRevision;
    return true;
}

bool LLVKMediaFilter::add(const std::string& url,Action action,std::string& error)
{
    error.clear();
    if (url.size()>65536) { error="Native media filter domain exceeds length limit"; return false; }
    const auto name=domain(url);
    if (name.empty()) { error="No media filter domain specified"; return false; }
    auto rules=mRules;
    LLSD rule; rule["domain"]=name; rule["action"]=action==Action::Allow ? "allow" : "deny";
    rules.append(rule);
    return load(rules,error);
}

bool LLVKMediaFilter::remove(const std::string& domain)
{
    for (int index=0; index<mRules.size(); ++index)
        if (mRules[index]["domain"].asString()==domain)
        { mRules.erase(index); ++mRevision; return true; }
    return false;
}

std::optional<LLVKMediaFilter::Action> LLVKMediaFilter::decide(const std::string& url) const
{
    const auto host=domain(url);
    for (auto rule=mRules.beginArray(); rule!=mRules.endArray(); ++rule)
    {
        const auto listed=(*rule)["domain"].asString();
        if (url==listed || host.ends_with(listed))
            return (*rule)["action"].asString()=="allow" ? Action::Allow : Action::Deny;
    }
    return std::nullopt;
}
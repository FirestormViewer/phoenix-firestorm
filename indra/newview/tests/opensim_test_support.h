/** Offline fixtures for the production OpenSim grid and SLURL implementations. */
#ifndef FS_OPENSIM_TEST_SUPPORT_H
#define FS_OPENSIM_TEST_SUPPORT_H

#include "../llviewernetwork.h"
#include "../llviewercontrol.h"
#include "fscorehttputil.h"
#include "lltut.h"
#include <deque>
#include <vector>

namespace opensim_test
{
struct Request
{
    std::string url;
    FSCoreHttpUtil::completionCallback_t success;
    FSCoreHttpUtil::completionCallback_t failure;
};
extern std::deque<Request> requests;
extern std::vector<std::string> notifications;

LLSD grid(const std::string& name = "grid.example.invalid:8002");
void add(LLGridManager& manager, const LLSD& data);
void reply(const std::string& xml);
void failRequest(S32 status);

struct Fixture
{
    Fixture();
    ~Fixture();
    LLGridManager& manager() { return LLGridManager::instance(); }
};
}
#endif

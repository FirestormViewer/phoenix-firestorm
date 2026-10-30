/** Viewer-service boundaries for offline OpenSim grid and SLURL tests. */
#include "llviewerprecompiledheaders.h"
#include "opensim_test_support.h"
#include "llcommandhandler.h"
#include "lllogininstance.h"
#include "lllogin.h"
#include "llstartup.h"
#include "llnotificationsutil.h"
#include "lltrans.h"
#include "fspanellogin.h"
#include "lfsimfeaturehandler.h"
#include "rlvhandler.h"

LLControlGroup gSavedSettings("opensim_test");

// Grid selection only needs the viewer's login state, not a login transport.
LLLoginInstance::LLLoginInstance() : mLoginState("offline"), mAttemptComplete(false),
    mDispatcher("opensim_test_login", "change") {}
LLLoginInstance::~LLLoginInstance() = default;
EStartupState LLStartUp::gStartupState = STATE_FIRST;
LLCommandHandler::LLCommandHandler(const char*, EUntrustedAccess) {}
LLCommandHandler::~LLCommandHandler() = default;
void FSPanelLogin::updateServer() { tut::fail("Unexpected login-panel access"); }
LFSimFeatureHandler::LFSimFeatureHandler() {}

std::string LLTrans::getString(std::string_view name, const LLStringUtil::format_map_t&, bool)
{
    return std::string(name);
}
void LLTrans::setDefaultArg(const std::string&, const std::string&) {}
LLNotificationPtr LLNotificationsUtil::add(const std::string& name, const LLSD&)
{
    opensim_test::notifications.push_back(name);
    return nullptr;
}

// Location hiding is outside this parser test; keep RLVa explicitly inactive.
RlvHandler::RlvHandler() : m_pGCTimer(nullptr)
{
    memset(m_Behaviours, 0, sizeof(m_Behaviours));
}
RlvHandler::~RlvHandler() = default;
bool RlvHandler::handleEvent(LLPointer<LLOldEvents::LLEvent>, const LLSD&) { return false; }
void RlvHandler::changed(const LLUUID&, LLGroupChange) { tut::fail("Unexpected RLVa group event"); }
RlvHandler gRlvHandler;
bool RlvUtil::isNearbyRegion(const std::string&) { return false; }
const std::string& RlvStrings::getString(const std::string_view&)
{
    static const std::string hidden("hidden");
    return hidden;
}

namespace FSCoreHttpUtil
{
// No socket is opened. Tests explicitly deliver success/error responses.
void callbackHttpGetRaw(const std::string& url, completionCallback_t success,
                       completionCallback_t failure, LLCore::HttpHeaders::ptr_t,
                       LLCore::HttpOptions::ptr_t)
{
    opensim_test::requests.push_back({url, std::move(success), std::move(failure)});
}
}

namespace opensim_test
{
std::deque<Request> requests;
std::vector<std::string> notifications;

LLSD grid(const std::string& name)
{
    LLSD data;
    data[GRID_VALUE] = name;
    data[GRID_LABEL_VALUE] = "Test Grid";
    data[GRID_NICK_VALUE] = "testgrid";
    data[GRID_LOGIN_URI_VALUE] = LLSD::emptyArray();
    data[GRID_LOGIN_URI_VALUE].append("https://" + name + "/login");
    data[GRID_HELPER_URI_VALUE] = "https://" + name + "/helpers/";
    data[GRID_LOGIN_PAGE_VALUE] = "https://" + name + "/welcome";
    data[GRID_SLURL_BASE] = "hop://" + name + "/";
    return data;
}

void add(LLGridManager& manager, const LLSD& data)
{
    auto* entry = new GridEntry;
    entry->grid = data;
    entry->set_current = false;
    manager.addGrid(entry, LLGridManager::FINISH);
}

void reply(const std::string& xml)
{
    tut::ensure("an HTTP request must be pending", !requests.empty());
    Request request = std::move(requests.front());
    requests.pop_front();
    LLSD response;
    LLSD& results = response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_SUCCESS] = true;
    results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_TYPE] = HTTP_OK;
    results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_STATUS] = 0;
    response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW] = LLSD::Binary(xml.begin(), xml.end());
    request.success(response);
}

void failRequest(S32 status)
{
    tut::ensure("an HTTP request must be pending", !requests.empty());
    Request request = std::move(requests.front());
    requests.pop_front();
    LLSD response;
    response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_TYPE] = status;
    response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_STATUS] = 0;
    response[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_MESSAGE] = "test error";
    request.failure(response);
}

Fixture::Fixture()
{
    LLGridManager::createInstance();
    for (const char* name : {"CurrentGrid", "CmdLineHelperURI", "LoginPage", "CmdLineUpdateService"})
    {
        if (!gSavedSettings.controlExists(name))
            gSavedSettings.declareString(name, "", "test setting", LLControlVariable::PERSIST_NO);
        gSavedSettings.setString(name, "");
    }
    requests.clear();
    notifications.clear();
}

Fixture::~Fixture()
{
    // Retire any deliberately uncompleted discovery requests through the real
    // failure path so their GridEntry objects are released before the manager.
    while (!requests.empty())
        failRequest(HTTP_BAD_REQUEST);
    LLGridManager::deleteSingleton();
}
}

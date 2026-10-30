/** Regression coverage for the actual OpenSim grid manager, without network I/O. */
#include "llviewerprecompiledheaders.h"
#include "opensim_test_support.h"
#include "namedtempfile.h"
#include "llsdserialize.h"

namespace tut
{
using namespace opensim_test;
struct grid_data : Fixture {};
using grid_group = test_group<grid_data>;
using grid_object = grid_group::object;
grid_group grid_tests("FSGridHandler");

template<> template<> void grid_object::test<1>()
{
    set_test_name("select a saved OpenSim grid and resolve its endpoints");
    LLSD data = grid();
    data[GRID_LOGIN_URI_VALUE].append("");
    data[GRID_LOGIN_URI_VALUE].append("https://backup.example.invalid/login");
    add(manager(), data);
    manager().setGridChoice(data[GRID_VALUE]);
    ensure("ready after selection", manager().isReadyToLogin());
    ensure("OpenSim classification", manager().isInOpenSim());
    ensure("not Second Life", !manager().isInSecondLife());
    ensure_equals("selected grid", manager().getGrid(), data[GRID_VALUE].asString());
    ensure_equals("persisted selection", gSavedSettings.getString("CurrentGrid"), manager().getGrid());
    ensure_equals("grid nick", manager().getGridId(), "testgrid");
    ensure_equals("label", manager().getGridLabel(), "Test Grid");
    std::vector<std::string> uris;
    manager().getLoginURIs(uris);
    ensure_equals("ignore empty login endpoints", uris.size(), size_t(2));
    ensure_equals("first endpoint", uris[0], data[GRID_LOGIN_URI_VALUE][0].asString());
    ensure_equals("backup endpoint", uris[1], "https://backup.example.invalid/login");
    ensure_equals("helper", manager().getHelperURI(), data[GRID_HELPER_URI_VALUE].asString());
    ensure_equals("login page", manager().getLoginPage(), data[GRID_LOGIN_PAGE_VALUE].asString());
    ensure("saved grids do not trigger HTTP", requests.empty());
}

template<> template<> void grid_object::test<2>()
{
    set_test_name("case-insensitive lookup and command-line endpoint overrides");
    add(manager(), grid());
    manager().setGridChoice(grid()[GRID_VALUE]);
    ensure_equals("lookup by label", manager().getGridByLabel("TEST GRID"), manager().getGrid());
    ensure_equals("lookup by nick", manager().getGridByGridNick("TESTGRID"), manager().getGrid());
    ensure_equals("lookup by host", manager().getGridByHostName("GRID.EXAMPLE.INVALID:8002"), manager().getGrid());
    ensure_equals("unknown grid", manager().getGridByProbing("missing.example.invalid"), "");
    for (const char* alias : {"TESTGRID", "TEST GRID"})
    {
        manager().setGridChoice(alias);
        ensure_equals("aliases select the canonical grid", manager().getGrid(), grid()[GRID_VALUE].asString());
        ensure("alias selection keeps OpenSim classification", manager().isInOpenSim());
        ensure_equals("canonical selection persisted", gSavedSettings.getString("CurrentGrid"), manager().getGrid());
    }
    gSavedSettings.setString("CmdLineHelperURI", "https://override.example.invalid/helpers");
    gSavedSettings.setString("LoginPage", "https://override.example.invalid/welcome");
    ensure_equals("helper override", manager().getHelperURI(), "https://override.example.invalid/helpers");
    ensure_equals("login-page override", manager().getLoginPage(), "https://override.example.invalid/welcome");
    ensure("lookups remain offline", requests.empty());
}

template<> template<> void grid_object::test<3>()
{
    set_test_name("saved-grid defaults and invalid entries");
    LLSD data;
    data[GRID_VALUE] = "minimal.example.invalid";
    add(manager(), data);
    LLSD stored;
    manager().getGridData("minimal.example.invalid", stored);
    ensure_equals("default label", stored[GRID_LABEL_VALUE].asString(), "minimal.example.invalid");
    ensure_equals("default nick", stored[GRID_NICK_VALUE].asString(), "minimal.example.invalid");
    ensure_equals("agent credentials", stored[GRID_LOGIN_IDENTIFIER_TYPES][0].asString(), "agent");
    ensure_equals("account credentials", stored[GRID_LOGIN_IDENTIFIER_TYPES][1].asString(), "account");
    std::vector<bool> changes;
    auto connection = manager().addGridListChangedCallback([&](bool ok) { changes.push_back(ok); });
    add(manager(), LLSD::emptyMap());
    ensure_equals("invalid entry is rejected", manager().getKnownGrids().size(), size_t(1));
    ensure("failure notification", changes.size() == 1 && !changes[0]);
    connection.disconnect();
    ensure("no HTTP for malformed saved entry", requests.empty());
}

template<> template<> void grid_object::test<4>()
{
    set_test_name("persist grid data, filter transient entries, and reload");
    struct PersistentGrid : LLGridManager
    {
        void setFile(const std::string& path) { mGridFile = path; }
    } saved;
    NamedTempFile file("fs_grids", "");
    saved.setFile(fsyspath(file.getPath()).string());
    add(saved, grid());
    LLSD temporary = grid("temporary.example.invalid");
    temporary["FLAG_TEMPORARY"] = true;
    add(saved, temporary);
    LLSD deprecated = grid("deprecated.example.invalid");
    deprecated["DEPRECATED"] = true;
    add(saved, deprecated);
    saved.saveGridList();
    manager().initGridList(fsyspath(file.getPath()).string(), LLGridManager::FINISH);
    ensure_equals("only durable entries reloaded", manager().getKnownGrids().size(), size_t(1));
    LLSD actual;
    manager().getGridData(grid()[GRID_VALUE], actual);
    ensure_equals("login endpoints survive reload", actual[GRID_LOGIN_URI_VALUE], grid()[GRID_LOGIN_URI_VALUE]);
    ensure_equals("SLURL base survives reload", actual[GRID_SLURL_BASE], grid()[GRID_SLURL_BASE]);
    manager().removeGrid(grid()[GRID_VALUE]);
    ensure("deleted grid hidden", manager().getKnownGrids().empty());
    ensure("persistence remains offline", requests.empty());
}

template<> template<> void grid_object::test<5>()
{
    set_test_name("newer grid-list entries override older definitions");
    LLSD data = grid();
    data["LastModified"] = LLDate("2024-01-01T00:00:00Z");
    add(manager(), data);
    data[GRID_LABEL_VALUE] = "Stale label";
    data["LastModified"] = LLDate("2023-01-01T00:00:00Z");
    add(manager(), data);
    ensure_equals("older entry ignored", manager().getGridLabel(grid()[GRID_VALUE]), "Test Grid");
    data[GRID_LABEL_VALUE] = "Updated label";
    data["LastModified"] = LLDate("2025-01-01T00:00:00Z");
    add(manager(), data);
    ensure_equals("newer entry used", manager().getGridLabel(grid()[GRID_VALUE]), "Updated label");
}

template<> template<> void grid_object::test<6>()
{
    set_test_name("discover and select a grid through a controlled HTTP response");
    manager().addGrid("https://discovered.example.invalid:8002");
    ensure_equals("one discovery request", requests.size(), size_t(1));
    ensure_equals("grid-info URL", requests.front().url, "https://discovered.example.invalid:8002/get_grid_info");
    reply("<gridinfo><gridname>Discovered Grid</gridname><gridnick>discovered</gridnick>"
          "<login>https://discovered.example.invalid:8002/login</login>"
          "<welcome>https://discovered.example.invalid:8002/welcome</welcome></gridinfo>");
    ensure("discovery complete", requests.empty());
    ensure("ready to login", manager().isReadyToLogin());
    ensure_equals("selected discovered grid", manager().getGrid(), "discovered.example.invalid:8002");
    ensure_equals("parsed label", manager().getGridLabel(), "Discovered Grid");
    ensure_equals("parsed nick", manager().getGridId(), "discovered");
    ensure("no error dialog", notifications.empty());
}

template<> template<> void grid_object::test<7>()
{
    set_test_name("bad discovery XML and HTTP failures do not add a grid");
    manager().addGrid("https://broken.example.invalid");
    reply("not grid-info XML");
    ensure("broken XML rejected", manager().getKnownGrids().empty());
    ensure("broken XML reports an error", !notifications.empty());
    manager().addGrid("https://unavailable.example.invalid");
    failRequest(HTTP_NOT_FOUND);
    ensure_equals("legacy fallback request", requests.size(), size_t(1));
    failRequest(HTTP_NOT_FOUND);
    ensure("failure path terminates", requests.empty());
    ensure("failed discovery does not add a grid", manager().getKnownGrids().empty());
    ensure("failure does not enable login", !manager().isReadyToLogin());
}

template<> template<> void grid_object::test<8>()
{
    set_test_name("missing and malformed saved-grid files are ignored");
    NamedTempFile malformed("fs_bad_grids", "<llsd><map><key>broken");
    manager().initGridList(fsyspath(malformed.getPath()).string(), LLGridManager::FINISH);
    ensure("malformed file adds no grids", manager().getKnownGrids().empty());
    manager().initGridList(fsyspath(malformed.getPath()).string() + ".missing", LLGridManager::FINISH);
    ensure("missing file adds no grids", manager().getKnownGrids().empty());
    ensure("bad saved files cause no HTTP requests", requests.empty());
}


template<> template<> void grid_object::test<9>()
{
    set_test_name("Hypergrid authorities and URL bases preserve the destination port");
    LLSD data = grid();
    data["gatekeeper"] = "https://gatekeeper.example.invalid:8002";
    add(manager(), data);
    manager().setGridChoice(data[GRID_VALUE]);
    ensure_equals("numeric port is preserved", manager().trimHypergrid("grid.example.invalid:8002"), "grid.example.invalid:8002");
    ensure_equals("region suffix is removed", manager().trimHypergrid("grid.example.invalid:8002:Remote Region"), "grid.example.invalid:8002");
    ensure_equals("gatekeeper metadata", manager().getGatekeeper(), "https://gatekeeper.example.invalid:8002");
    ensure_equals("location base", manager().getSLURLBase(), "hop://grid.example.invalid:8002/");
    ensure_equals("app base", manager().getAppSLURLBase(), "hop://grid.example.invalid:8002/app");
    ensure("known-grid URL bases do not fetch metadata", requests.empty());
}

template<> template<> void grid_object::test<10>()
{
    set_test_name("probe saved grid aliases through public lookup APIs");
    const LLSD data = grid();
    const std::string canonical = data[GRID_VALUE].asString();
    add(manager(), data);
    manager().setGridChoice(canonical);
    for (const char* alias : {"grid.example.invalid:8002", "GRID.EXAMPLE.INVALID:8002",
                             "testgrid", "TESTGRID", "Test Grid", "TEST GRID"})
    {
        ensure_equals(std::string("probe ") + alias,
                      manager().getGridByProbing(alias), canonical);
        ensure_equals(std::string("resolve ") + alias,
                      manager().getGrid(alias), canonical);
        ensure_equals(std::string("grid ID through ") + alias,
                      manager().getGridId(alias), "testgrid");
    }
    ensure_equals("case-sensitive nickname lookup", manager().getGridByProbing("testgrid", true), canonical);
    ensure_equals("case-sensitive label lookup", manager().getGridByProbing("Test Grid", true), canonical);
    ensure("case-sensitive nickname rejects different case", manager().getGridByProbing("TESTGRID", true).empty());
    ensure("case-sensitive label rejects different case", manager().getGridByProbing("TEST GRID", true).empty());
    ensure("unknown alias stays unresolved", manager().getGridByProbing("unknown grid").empty());
    ensure_equals("probing does not change the selected grid", manager().getGrid(), canonical);
    ensure("alias lookups do not start discovery", requests.empty());
}

}

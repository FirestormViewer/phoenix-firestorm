/** Tests of Firestorm's production SLURL parser with the real OpenSim grid manager. */
#include "llviewerprecompiledheaders.h"
#include "opensim_test_support.h"
#include "llslurl.h"

namespace tut
{
using namespace opensim_test;
struct slurl_data : Fixture
{
    slurl_data()
    {
        add(manager(), grid());
        manager().setGridChoice(grid()[GRID_VALUE]);
    }
};
using slurl_group = test_group<slurl_data>;
using slurl_object = slurl_group::object;
slurl_group slurl_tests("FSSLURL");

template<> template<> void slurl_object::test<1>()
{
    set_test_name("home, last location, and invalid protocols");
    ensure_equals("empty means last", LLSLURL("").getType(), LLSLURL::LAST_LOCATION);
    ensure_equals("home", LLSLURL("home").getLoginString(), "home");
    ensure_equals("last", LLSLURL("last").getLoginString(), "last");
    for (const char* url : {"mailto:someone@example.invalid", "ftp://grid.example.invalid/region/Test",
                            "https://unrelated.example.invalid/region/Test", "hop://grid.example.invalid:8002"})
        ensure(std::string("reject ") + url, !LLSLURL(url).isValid());
    ensure("invalid inputs cause no discovery", requests.empty());
}

template<> template<> void slurl_object::test<2>()
{
    set_test_name("hop URL preserves grid, Unicode region, and var-region coordinates");
    const std::string region = "\xE6\xB5\x8B\xE8\xAF\x95 \xD0\x9C\xD0\xB8\xD1\x80";
    const std::string url = "hop://grid.example.invalid:8002/%E6%B5%8B%E8%AF%95%20%D0%9C%D0%B8%D1%80/512/768/42";
    LLSLURL location(url);
    ensure_equals("location type", location.getType(), LLSLURL::LOCATION);
    ensure("Hypergrid flag", location.getHypergrid());
    ensure_equals("destination grid", location.getGrid(), manager().getGrid());
    ensure_equals("decoded Unicode region", location.getRegion(), region);
    ensure_equals("coordinates are not wrapped at 256", location.getPosition(), LLVector3(512, 768, 42));
    ensure_equals("canonical URL", location.getSLURLString(), url);
    ensure("round trip", location == LLSLURL(location.getSLURLString()));
    ensure("known location remains offline", requests.empty());
}

template<> template<> void slurl_object::test<3>()
{
    set_test_name("typed location defaults and optional height");
    LLSLURL centre("Test Region");
    ensure_equals("region", centre.getRegion(), "Test Region");
    ensure_equals("default coordinates", centre.getPosition(), LLVector3(128, 128, 0));
    LLSLURL planar("Test Region/600/700");
    ensure_equals("default height", planar.getPosition(), LLVector3(600, 700, 0));
    ensure_equals("leading slash", LLSLURL("/Test Region/600/700/80").getPosition(), LLVector3(600, 700, 80));
    ensure_equals("login destination", planar.getLoginString(), "uri:Test Region&amp;600&amp;700&amp;0");
}

template<> template<> void slurl_object::test<4>()
{
    set_test_name("HTTP and x-grid-location-info URLs for known grids");
    for (const char* url : {"https://grid.example.invalid:8002/region/Test%20Region/1/2/3",
                            "x-grid-location-info://grid.example.invalid:8002/region/Test%20Region/1/2/3"})
    {
        LLSLURL location(url);
        ensure_equals("location", location.getType(), LLSLURL::LOCATION);
        ensure_equals("region", location.getRegion(), "Test Region");
        ensure_equals("coordinates", location.getPosition(), LLVector3(1, 2, 3));
    }
}

template<> template<> void slurl_object::test<5>()
{
    set_test_name("app commands and query parameters");
    LLSLURL app("hop://grid.example.invalid:8002/app/teleport/Test%20Region/512/768/42?title=Hello%20World");
    ensure_equals("app type", app.getType(), LLSLURL::APP);
    ensure_equals("command", app.getAppCmd(), "teleport");
    // LLURI::pathArray deliberately keeps path components escaped for dispatch.
    ensure_equals("escaped path", app.getAppPath()[0].asString(), "Test%20Region");
    ensure_equals("query map", app.getAppQueryMap()["title"].asString(), "Hello World");
    ensure("app is not a spatial start location", !app.isSpatial());
}

template<> template<> void slurl_object::test<6>()
{
    set_test_name("location constructors preserve OpenSim var-region positions");
    LLSLURL local(manager().getGrid(), "Large Region", LLVector3(600, 700, 42));
    ensure_equals("large local coordinates", local.getPosition(), LLVector3(600, 700, 42));
    LLSLURL global(manager().getGrid(), "Large Region", LLVector3d(10000, 20000, 0), LLVector3d(10600, 20700, 42));
    ensure_equals("global-to-local conversion", global.getPosition(), LLVector3(600, 700, 42));
    ensure("constructor URL round trip", local == LLSLURL(local.getSLURLString()));
}

template<> template<> void slurl_object::test<7>()
{
    set_test_name("Second Life URLs remain usable in an OpenSim-capable build");
    LLSLURL location("https://maps.secondlife.com/secondlife/Ahern/128/64/30");
    ensure_equals("location", location.getType(), LLSLURL::LOCATION);
    ensure_equals("Second Life destination", location.getGrid(), MAINGRID);
    ensure_equals("region", location.getRegion(), "Ahern");
    ensure_equals("coordinates", location.getPosition(), LLVector3(128, 64, 30));
}

template<> template<> void slurl_object::test<8>()
{
    set_test_name("strip ASCII controls while preserving UTF-8 region bytes");
    LLSLURL location("hop://grid.example.invalid:8002/%01%E6%B5%8B%E8%AF%95%0A/600/700/42");
    ensure_equals("UTF-8 region survives control filtering", location.getRegion(), "\xE6\xB5\x8B\xE8\xAF\x95");
    ensure_equals("valid coordinates survive filtering", location.getPosition(), LLVector3(600, 700, 42));
}


template<> template<> void slurl_object::test<9>()
{
    set_test_name("unknown Hypergrid destination does not change the selected login grid");
    const std::string current = manager().getGrid();
    const std::string url = "hop://remote.example.invalid:9000/Remote%20Region/512/600/80";
    LLSLURL location(url);
    ensure("Hypergrid location is valid", location.isValid() && location.getHypergrid());
    ensure_equals("remote destination", location.getGrid(), "remote.example.invalid:9000");
    ensure("parsing does not fetch metadata", requests.empty());
    ensure_equals("URL available before discovery completes", location.getSLURLString(), url);
    ensure_equals("one asynchronous discovery", requests.size(), size_t(1));
    reply("<gridinfo><gridname>Remote Grid</gridname><gridnick>remote</gridnick>"
          "<login>https://remote.example.invalid:9000/login</login>"
          "<welcome>https://remote.example.invalid:9000/welcome</welcome></gridinfo>");
    ensure_equals("login grid is unchanged", manager().getGrid(), current);
    ensure("remote metadata remains temporary", manager().isTemporary("remote.example.invalid:9000"));
    ensure("discovery completed", requests.empty());
}

}

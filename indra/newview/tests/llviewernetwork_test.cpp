/**
 * @file llviewernetwork_test.cpp
 * @author Roxie
 * @date 2009-03-9
 * @brief Test the viewernetwork functionality
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */
#include "../llviewerprecompiledheaders.h"
#include "../llviewernetwork.h"
#include "../test/lltut.h"
#include "../../llxml/llcontrol.h"
#include "llfile.h"
// <FS:Beq> Implement the real translation interface rather than redefining it.
#include "lltrans.h"
// </FS:Beq>

namespace
{

// Should not collide with other test programs creating temp files.
static const char * const TEST_FILENAME("llviewernetwork_test.xml");

}

//
// Stub implementation for LLTrans
//
// <FS:Beq/> The class declaration comes from lltrans.h; only functions are stubbed.

std::string LLTrans::getString(std::string_view xml_desc, const LLStringUtil::format_map_t& args, bool def_string)
{
    std::string grid_label = std::string();
    if(xml_desc == "AgniGridLabel")
    {
        grid_label = "Second Life Main Grid (Agni)";
    }
    else if(xml_desc == "AditiGridLabel")
    {
        grid_label = "Second Life Beta Test Grid (Aditi)";
    }

    return grid_label;
}

//----------------------------------------------------------------------------
// Mock objects for the dependencies of the code we're testing

// <FS:Beq> Grid selection updates translation arguments through this UI boundary.
void LLTrans::setDefaultArg(const std::string&, const std::string&) {}
// </FS:Beq>

LLControlGroup::LLControlGroup(const std::string& name)
: LLInstanceTracker<LLControlGroup, std::string>(name) {}
LLControlGroup::~LLControlGroup() {}
LLControlVariable* LLControlGroup::declareString(const std::string& name,
                                   const std::string& initial_val,
                                   const std::string& comment,
                                   LLControlVariable::ePersist persist) {return NULL;}
void LLControlGroup::setString(std::string_view name, const std::string& val){}

std::string gCmdLineLoginURI;
std::string gCmdLineGridChoice;
std::string gCmdLineHelperURI;
std::string gLoginPage;
std::string gCurrentGrid;
std::string LLControlGroup::getString(std::string_view name)
{
    if (name == "CmdLineGridChoice")
        return gCmdLineGridChoice;
    else if (name == "CmdLineHelperURI")
        return gCmdLineHelperURI;
    else if (name == "LoginPage")
        return gLoginPage;
    else if (name == "CurrentGrid")
        return gCurrentGrid;
    return "";
}

LLSD LLControlGroup::getLLSD(std::string_view name)
{
    if (name == "CmdLineLoginURI")
    {
        if(!gCmdLineLoginURI.empty())
        {
            return LLSD(gCmdLineLoginURI);
        }
    }
    return LLSD();
}

LLPointer<LLControlVariable> LLControlGroup::getControl(std::string_view name)
{
    ctrl_name_table_t::iterator iter = mNameTable.find(name.data());
    return iter == mNameTable.end() ? LLPointer<LLControlVariable>() : iter->second;
}

LLControlGroup gSavedSettings("test");

const char *gSampleGridFile =
    "<?xml version=\"1.0\"?>"
    "<llsd>"
    "  <map>"
    "    <key>altgrid.long.name</key>"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>label</key><string>Alternative Grid</string>"
    "      <key>login_page</key><string>altgrid/loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>altgrid/myloginuri1</string>"
    "        <string>altgrid/myloginuri2</string>"
    "      </array>"
    "      <key>keyname</key><string>altgrid.long.name</string>"
    "      <key>credential_type</key><string>agent</string>"
    "      <key>grid_login_id</key><string>AltGrid</string>"
    "    </map>"
    "    <key>minimal.long.name</key>"
    "    <map>"
    "      <key>keyname</key><string>minimal.long.name</string>"
    "    </map>"
    "    <!-- Note that the values for agni and aditi below are deliberately"
    "         incorrect to test that they are not overwritten -->"
    "    <key>util.agni.lindenlab.com</key> <!-- conflict -->"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>grid_login_id</key><string>mylabel</string>"
    "      <key>label</key><string>mylabel</string>"
    "      <key>login_page</key><string>loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>myloginuri</string>"
    "      </array>"
    "      <key>keyname</key><string>util.agni.lindenlab.com</string> <!-- conflict -->"
    "    </map>"
    "    <key>util.foobar.lindenlab.com</key>"
    "    <map>"
    "      <key>helper_uri</key><string>https://helper1/helpers/</string>"
    "      <key>grid_login_id</key><string>Aditi</string> <!-- conflict -->"
    "      <key>label</key><string>mylabel</string>"
    "      <key>login_page</key><string>loginpage</string>"
    "      <key>login_uri</key>"
    "      <array>"
    "        <string>myloginuri</string>"
    "      </array>"
    "      <key>update_query_url_base</key><string>https://update.secondlife.com/update</string>"
    "      <key>keyname</key><string>util.foobar.lindenlab.com</string>"
    "    </map>"
    "  </map>"
    "</llsd>"
    ;
// -------------------------------------------------------------------------------------------
// TUT
// -------------------------------------------------------------------------------------------
namespace tut
{
  // Test wrapper declaration : wrapping nothing for the moment
  struct viewerNetworkTest
    {
        viewerNetworkTest()
        {
            LLGridManager::createInstance();
            LLFile::remove(TEST_FILENAME);
            gCmdLineLoginURI.clear();
            gCmdLineGridChoice.clear();
            gCmdLineHelperURI.clear();
            gLoginPage.clear();
            gCurrentGrid.clear();
        }
        ~viewerNetworkTest()
        {
            LLFile::remove(TEST_FILENAME);
            LLGridManager::deleteSingleton();
        }
    };

    // Tut templating thingamagic: test group, object and test instance
    typedef test_group<viewerNetworkTest> viewerNetworkTestFactory;
    typedef viewerNetworkTestFactory::object viewerNetworkTestObject;
    tut::viewerNetworkTestFactory tut_test("LLViewerNetwork");

    // ---------------------------------------------------------------------------------------
    // Test functions
    // ---------------------------------------------------------------------------------------
    // initialization without a grid file
    template<> template<>
    void viewerNetworkTestObject::test<1>()
    {
        LLGridManager *manager = LLGridManager::getInstance();
        // grid file doesn't exist
        manager->initialize(TEST_FILENAME);
        // validate that some of the defaults are available.
        std::map<std::string, std::string> known_grids = manager->getKnownGrids();
        ensure_equals("Known grids is a string-string map of size 2", known_grids.size(), 2);
        ensure_equals("Agni has the right name and label",
                      known_grids[std::string("util.agni.lindenlab.com")],
                      std::string("Second Life")); // <FS:Beq/> Current grid label.
        ensure_equals("Aditi has the right name and label",
                      known_grids[std::string("util.aditi.lindenlab.com")],
                      std::string("Second Life Beta")); // <FS:Beq/> Current grid label.
        ensure_equals("name for agni",
                      LLGridManager::getInstance()->getGrid("util.agni.lindenlab.com"),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("id for agni",
                      std::string("Agni"),
                      LLGridManager::getInstance()->getGridId("util.agni.lindenlab.com"));
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        ensure_equals("label for agni",
                      LLGridManager::getInstance()->getGridLabel("util.agni.lindenlab.com"),
                      std::string("Second Life")); // <FS:Beq/> Current grid label.

        std::vector<std::string> login_uris;
        LLGridManager::getInstance()->getLoginURIs(std::string("util.agni.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for agni", 1, login_uris.size());
        ensure_equals("Agni login uri",
                      login_uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Agni helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.agni.lindenlab.com"),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("Agni login page",
                      LLGridManager::getInstance()->getLoginPage("util.agni.lindenlab.com"),
                      std::string("https://phoenixviewer.com/app/loginV3/")); // <FS:Beq/> Firestorm login page.
        ensure("Agni is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.agni.lindenlab.com"));

        ensure_equals("name for aditi",
                      LLGridManager::getInstance()->getGrid("util.aditi.lindenlab.com"),
                      std::string("util.aditi.lindenlab.com"));
        ensure_equals("id for aditi",
                      LLGridManager::getInstance()->getGridId("util.aditi.lindenlab.com"),
                      std::string("Aditi"));
        ensure_equals("label for aditi",
                      LLGridManager::getInstance()->getGridLabel("util.aditi.lindenlab.com"),
                      std::string("Second Life Beta")); // <FS:Beq/> Current grid label.

        LLGridManager::getInstance()->getLoginURIs(std::string("util.aditi.lindenlab.com"), login_uris);

        ensure_equals("Number of login uris for aditi", 1, login_uris.size());
        ensure_equals("Aditi login uri",
                      login_uris[0],
                      std::string("https://login.aditi.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Aditi helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.aditi.lindenlab.com"),
                      std::string("https://secondlife.aditi.lindenlab.com/helpers/"));
        ensure_equals("Aditi login page",
                      LLGridManager::getInstance()->getLoginPage("util.aditi.lindenlab.com"),
                      std::string("https://phoenixviewer.com/app/loginV3/")); // <FS:Beq/> Firestorm login page.
        ensure("Aditi is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.aditi.lindenlab.com"));
    }

    // initialization with a grid file
    template<> template<>
    void viewerNetworkTestObject::test<2>()
    {
        llofstream gridfile(TEST_FILENAME);
        gridfile << gSampleGridFile;
        gridfile.close();

        LLGridManager::getInstance()->initialize(TEST_FILENAME);
        std::map<std::string, std::string> known_grids = LLGridManager::getInstance()->getKnownGrids();
        // <FS:Beq> SL builds deliberately ignore external grid files.
        ensure_equals("grid file cannot add grids in an SL build", known_grids.size(), 2);
        // </FS:Beq>

        // Verify that Agni and Aditi were not overwritten
        ensure_equals("Agni has the right name and label",
                      known_grids[std::string("util.agni.lindenlab.com")],
                      std::string("Second Life")); // <FS:Beq/> Current grid label.
        ensure_equals("Aditi has the right name and label",
                      known_grids[std::string("util.aditi.lindenlab.com")],
                      std::string("Second Life Beta")); // <FS:Beq/> Current grid label.
        ensure_equals("name for agni",
                      LLGridManager::getInstance()->getGrid("util.agni.lindenlab.com"),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("id for agni",
                      LLGridManager::getInstance()->getGridId("util.agni.lindenlab.com"),
                      std::string("Agni"));
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        ensure_equals("label for agni",
                      LLGridManager::getInstance()->getGridLabel("util.agni.lindenlab.com"),
                      std::string("Second Life")); // <FS:Beq/> Current grid label.
        std::vector<std::string> login_uris;
        LLGridManager::getInstance()->getLoginURIs(std::string("util.agni.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for agni", 1, login_uris.size());
        ensure_equals("Agni login uri",
                      login_uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Agni helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.agni.lindenlab.com"),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("Agni login page",
                      LLGridManager::getInstance()->getLoginPage("util.agni.lindenlab.com"),
                      std::string("https://phoenixviewer.com/app/loginV3/")); // <FS:Beq/> Firestorm login page.
        ensure("Agni is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.agni.lindenlab.com"));

        ensure_equals("name for aditi",
                      LLGridManager::getInstance()->getGrid("util.aditi.lindenlab.com"),
                      std::string("util.aditi.lindenlab.com"));
        ensure_equals("id for aditi",
                      LLGridManager::getInstance()->getGridId("util.aditi.lindenlab.com"),
                      std::string("Aditi"));
        ensure_equals("label for aditi",
                      LLGridManager::getInstance()->getGridLabel("util.aditi.lindenlab.com"),
                      std::string("Second Life Beta")); // <FS:Beq/> Current grid label.

        LLGridManager::getInstance()->getLoginURIs(std::string("util.aditi.lindenlab.com"), login_uris);
        ensure_equals("Number of login uris for aditi", 1, login_uris.size());
        ensure_equals("Aditi login uri",
                      login_uris[0],
                      std::string("https://login.aditi.lindenlab.com/cgi-bin/login.cgi"));
        ensure_equals("Aditi helper uri",
                      LLGridManager::getInstance()->getHelperURI("util.aditi.lindenlab.com"),
                      std::string("https://secondlife.aditi.lindenlab.com/helpers/"));
        ensure_equals("Aditi login page",
                      LLGridManager::getInstance()->getLoginPage("util.aditi.lindenlab.com"),
                      std::string("https://phoenixviewer.com/app/loginV3/")); // <FS:Beq/> Firestorm login page.
        ensure("Aditi is a system grid",
               LLGridManager::getInstance()->isSystemGrid("util.aditi.lindenlab.com"));

        // <FS:Beq> Custom grid loading is disabled in the SL production code.
        // Keep regression coverage that the file cannot enable another grid.
        ensure_equals("alternative grid is not loaded",
                      LLGridManager::getInstance()->getGrid("altgrid.long.name"), std::string());
        ensure_equals("minimal grid is not loaded",
                      LLGridManager::getInstance()->getGrid("minimal.long.name"), std::string());
        // </FS:Beq>

    }


    // validate grid selection
    template<> template<>
    void viewerNetworkTestObject::test<7>()
    {
        // adding a grid with simply a name will populate the values.
        llofstream gridfile(TEST_FILENAME);
        gridfile << gSampleGridFile;
        gridfile.close();

        LLGridManager::getInstance()->initialize(TEST_FILENAME);

        LLGridManager::getInstance()->setGridChoice("util.agni.lindenlab.com");
        ensure_equals("getGridLabel",
                      LLGridManager::getInstance()->getGridLabel(),
                      std::string("Second Life")); // <FS:Beq/> Current grid label.
        ensure_equals("getGridId",
                      LLGridManager::getInstance()->getGridId(),
                      std::string("Agni"));
        ensure_equals("getGrid",
                      LLGridManager::getInstance()->getGrid(),
                      std::string("util.agni.lindenlab.com"));
        ensure_equals("getHelperURI",
                      LLGridManager::getInstance()->getHelperURI(),
                      std::string("https://secondlife.com/helpers/"));
        ensure_equals("getLoginPage",
                      LLGridManager::getInstance()->getLoginPage(),
                      std::string("https://phoenixviewer.com/app/loginV3/")); // <FS:Beq/> Firestorm login page.
        ensure_equals("update url base for Agni", // relies on agni being the default
                      std::string("https://update.secondlife.com/update"),
                      LLGridManager::getInstance()->getUpdateServiceURL());
        // <FS:Beq> Exercise grid classification through its public API.
        ensure("Is Agni a production grid", LLGridManager::getInstance()->isInSLMain());
        // </FS:Beq>
        std::vector<std::string> uris;
        LLGridManager::getInstance()->getLoginURIs(uris);
        ensure_equals("getLoginURIs size", 1, uris.size());
        ensure_equals("getLoginURIs",
                      uris[0],
                      std::string("https://login.agni.lindenlab.com/cgi-bin/login.cgi"));

        // <FS:Beq> Check the supported beta grid and reject unknown choices.
        LLGridManager::getInstance()->setGridChoice("Aditi");
        ensure_equals("beta grid label", LLGridManager::getInstance()->getGridLabel(),
                      std::string("Second Life Beta"));
        ensure_equals("beta grid id", LLGridManager::getInstance()->getGridId(), std::string("Aditi"));
        ensure("beta is a system grid", LLGridManager::getInstance()->isSystemGrid());
        ensure("beta is not production", !LLGridManager::getInstance()->isInSLMain());
        LLGridManager::getInstance()->setGridChoice("altgrid.long.name");
        ensure_equals("unknown grid cannot change selection", LLGridManager::getInstance()->getGridId(),
                      std::string("Aditi"));
        // </FS:Beq>

    }

}

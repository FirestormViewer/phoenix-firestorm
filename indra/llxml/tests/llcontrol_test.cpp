/**
 * @file llcontrol_tut.cpp
 * @date   February 2008
 * @brief control group unit tests
 *
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#include "linden_common.h"
#include "llsdserialize.h"
#include "llfile.h"
#include "stringize.h"

#include "../llcontrol.h"

#include "../test/lltut.h"
#include <memory>
#include <vector>
#include <filesystem> // </FS:Beq> Validate the shipped rating-setting declarations.

namespace tut
{
    struct control_group
    {
        std::unique_ptr<LLControlGroup> mCG;
        std::string mTestConfigDir;
        std::string mTestConfigFile;
        std::vector<std::string> mCleanups;
        static bool mListenerFired;
        control_group()
        {
            mCG.reset(new LLControlGroup("foo"));
            LLUUID random;
            random.generate();
            // generate temp dir
            mTestConfigDir = STRINGIZE(LLFile::tmpdir() << "llcontrol-test-" << random << "/");
            mTestConfigFile = mTestConfigDir + "settings.xml";
            LLFile::mkdir(mTestConfigDir);
            LLSD config;
            config["TestSetting"]["Comment"] = "Dummy setting used for testing";
            config["TestSetting"]["Persist"] = 1;
            config["TestSetting"]["Type"] = "U32";
            config["TestSetting"]["Value"] = 12;
            writeSettingsFile(config);
        }
        ~control_group()
        {
            //Remove test files
            for (auto filename : mCleanups)
            {
                LLFile::remove(filename);
            }
            LLFile::remove(mTestConfigFile);
            LLFile::remove(mTestConfigDir);
        }
        void writeSettingsFile(const LLSD& config)
        {
            llofstream file(mTestConfigFile.c_str());
            if (file.is_open())
            {
                LLSDSerialize::toPrettyXML(config, file);
            }
            file.close();
        }
        static bool handleListenerTest()
        {
            control_group::mListenerFired = true;
            return true;
        }
    };

    bool control_group::mListenerFired = false;

    typedef test_group<control_group> control_group_test;
    typedef control_group_test::object control_group_t;
    control_group_test tut_control_group("control_group");

    //load settings from files - LLSD
    template<> template<>
    void control_group_t::test<1>()
    {
        int results = mCG->loadFromFile(mTestConfigFile.c_str());
        ensure("number of settings", (results == 1));
        ensure("value of setting", (mCG->getU32("TestSetting") == 12));
    }

    //save settings to files
    template<> template<>
    void control_group_t::test<2>()
    {
        int results = mCG->loadFromFile(mTestConfigFile.c_str());
        mCG->setU32("TestSetting", 13);
        ensure_equals("value of changed setting", mCG->getU32("TestSetting"), 13);
        LLControlGroup test_cg("foo2");
        std::string temp_test_file = (mTestConfigDir + "setting_llsd_temp.xml");
        mCleanups.push_back(temp_test_file);
        mCG->saveToFile(temp_test_file.c_str(), true);
        results = test_cg.loadFromFile(temp_test_file.c_str());
        ensure("number of changed settings loaded", (results == 1));
        ensure("value of changed settings loaded", (test_cg.getU32("TestSetting") == 13));
    }

    //priorities
    template<> template<>
    void control_group_t::test<3>()
    {
        // Pass default_values = true. This tells loadFromFile() we're loading
        // a default settings file that declares variables, rather than a user
        // settings file. When loadFromFile() encounters an unrecognized user
        // settings variable, it forcibly preserves it (CHOP-962).
        int results = mCG->loadFromFile(mTestConfigFile.c_str(), true);
        LLControlVariable* control = mCG->getControl("TestSetting");
        LLSD new_value = 13;
        control->setValue(new_value, false);
        ensure_equals("value of changed setting", mCG->getU32("TestSetting"), 13);
        LLControlGroup test_cg("foo3");
        std::string temp_test_file = (mTestConfigDir + "setting_llsd_persist_temp.xml");
        mCleanups.push_back(temp_test_file);
        mCG->saveToFile(temp_test_file.c_str(), true);
        results = test_cg.loadFromFile(temp_test_file.c_str());
        //If we haven't changed any settings, then we shouldn't have any settings to load
        ensure("number of non-persisted changed settings loaded", (results == 0));
    }

    //listeners
    template<> template<>
    void control_group_t::test<4>()
    {
        int results = mCG->loadFromFile(mTestConfigFile.c_str());
        ensure("number of settings", (results == 1));
        mCG->getControl("TestSetting")->getSignal()->connect(boost::bind(&this->handleListenerTest));
        mCG->setU32("TestSetting", 13);
        ensure("listener fired on changed setting", mListenerFired);
    }

    // <FS:Beq> Regress invalid control types reported by ASan during shutdown.
    template<> template<>
    void control_group_t::test<5>()
    {
        LLSD config;
        for (const auto& name : {"BadInteger", "BadSavedType", "MissingType", "Valid"})
        {
            config[name]["Comment"] = "Type validation regression";
            config[name]["Persist"] = 1;
            config[name]["Value"] = 2;
        }
        config["BadInteger"]["Type"] = "Integer";
        config["BadSavedType"]["Type"] = "NotBetween";
        config["Valid"]["Type"] = "S32";
        writeSettingsFile(config);
        ensure_equals("only valid declaration loaded", mCG->loadFromFile(mTestConfigFile, true), U32(1));
        ensure("invalid declaration absent", !mCG->getControl("BadInteger"));
        ensure("missing type absent", !mCG->getControl("MissingType"));
        mCG->setS32("Valid", 3);
        ensure_equals("valid settings still saved", mCG->saveToFile(mTestConfigFile, true), U32(1));
    }

    template<> template<>
    void control_group_t::test<6>()
    {
        mCG->declareS32("Rating", 1, "Previously malformed rating");
        LLSD config;
        config["Rating"]["Type"] = "NotBetween";
        config["Rating"]["Comment"] = "Previously malformed rating";
        config["Rating"]["Value"] = 2;
        writeSettingsFile(config);
        ensure_equals("existing control value recovered", mCG->loadFromFile(mTestConfigFile, false), U32(1));
        ensure_equals("user rating retained", mCG->getS32("Rating"), S32(2));
        ensure_equals("declared type retained", mCG->getControl("Rating")->type(), TYPE_S32);
        ensure_equals("repaired setting saved", mCG->saveToFile(mTestConfigFile, true), U32(1));
        LLSD saved;
        llifstream input(mTestConfigFile);
        LLSDSerialize::fromXML(saved, input);
        ensure_equals("saved type repaired", saved["Rating"]["Type"].asString(), std::string("S32"));
    }

    template<> template<>
    void control_group_t::test<7>()
    {
        for (const auto* invalid : {"Integer", "NotBetween", ""})
        {
            const auto type = LLControlGroup::typeStringToEnum(invalid);
            ensure_equals("unknown type uses a valid sentinel", type, TYPE_COUNT);
            ensure("unknown type cannot index the table", LLControlGroup::typeEnumToString(type).empty());
        }
        ensure("end sentinel cannot read beyond table", LLControlGroup::typeEnumToString(TYPE_COUNT).empty());
        ensure_equals("valid type still parses", LLControlGroup::typeStringToEnum("S32"), TYPE_S32);
        ensure_equals("valid enum still converts", LLControlGroup::typeEnumToString(TYPE_S32), std::string("S32"));
        ensure("invalid API declaration rejected", !mCG->declareControl("Invalid", TYPE_COUNT, LLSD(1),
            "Invalid type", SANITY_TYPE_NONE, LLSD(), "", LLControlVariable::PERSIST_ALWAYS));
        ensure("invalid API declaration not registered", !mCG->getControl("Invalid"));
    }

    template<> template<>
    void control_group_t::test<8>()
    {
        const auto source = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()
            / "newview/app_settings/settings_per_account.xml";
        LLSD shipped;
        llifstream input(source.string());
        ensure("shipped settings opened", input.is_open());
        ensure("shipped settings parsed", LLSDSerialize::fromXML(shipped, input) != LLSDParser::PARSE_FAILURE);
        LLSD config;
        for (const auto& name : {"FSPrimfeedPhotoRating", "FlickrPhotoRating"})
        {
            ensure_equals("rating uses a supported integer type", shipped[name]["Type"].asString(), std::string("S32"));
            config[name] = shipped[name];
        }
        writeSettingsFile(config);
        ensure_equals("both rating declarations loaded", mCG->loadFromFile(mTestConfigFile, true), U32(2));
        mCG->setS32("FSPrimfeedPhotoRating", 2);
        mCG->setS32("FlickrPhotoRating", 2);
        ensure_equals("both ratings survive shutdown saving", mCG->saveToFile(mTestConfigFile, true), U32(2));
    }
    // </FS:Beq>

}

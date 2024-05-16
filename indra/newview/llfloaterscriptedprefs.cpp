/**
 * @file llfloaterscriptedprefs.cpp
 * @brief Color controls for the script editor
 * @author Cinder Roxley
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"
#include "llfloaterscriptedprefs.h"

#include "llcolorswatch.h"
#include "llscripteditor.h"
#include "lldirpicker.h"
#include "llviewercontrol.h"

#include "llfloaterreg.h"
#include "llpreviewscript.h"

LLFloaterScriptEdPrefs::LLFloaterScriptEdPrefs(const LLSD& key)
:   LLFloater(key)
,   mEditor(NULL)
{
    mCommitCallbackRegistrar.add("ScriptPref.applyUIColor", boost::bind(&LLFloaterScriptEdPrefs::applyUIColor, this ,_1, _2));
    mCommitCallbackRegistrar.add("ScriptPref.getUIColor",   boost::bind(&LLFloaterScriptEdPrefs::getUIColor, this ,_1, _2));
    // <FS:Ansariel> Port old FS script prefs
    mCommitCallbackRegistrar.add("NACL.SetPreprocInclude",  boost::bind(&LLFloaterScriptEdPrefs::setPreprocInclude, this));
}

BOOL LLFloaterScriptEdPrefs::postBuild()
{
    mEditor = getChild<LLScriptEditor>("Script Preview");
    if (mEditor)
    {
        mEditor->initKeywords();
        mEditor->loadKeywords();
    }

    // <FS:Ansariel> Port old FS script prefs
    getChild<LLButton>("close_btn")->setClickedCallback(boost::bind(&LLFloaterScriptEdPrefs::closeFloater, this, false));

    return TRUE;
}

void LLFloaterScriptEdPrefs::applyUIColor(LLUICtrl* ctrl, const LLSD& param)
{
    LLUIColorTable::instance().setColor(param.asString(), LLColor4(ctrl->getValue()));
    mEditor->initKeywords();
    mEditor->loadKeywords();

    // <FS:Ansariel> FIRE-16740: Color syntax highlighting changes don't immediately appear in script window
    // This will return both LLPreviewLSL as well as LLLiveLSLEditor instances because they are grouped into "preview_script"!
    LLFloaterReg::const_instance_list_t floaters = LLFloaterReg::getFloaterList("preview_script");
    for (LLFloaterReg::const_instance_list_t::const_iterator it = floaters.begin(); it != floaters.end(); ++it)
    {
        LLScriptEdContainer* cont = dynamic_cast<LLScriptEdContainer*>(*it);
        if (cont)
        {
            cont->updateStyle();
        }
    }
    // </FS:Ansariel>
}

void LLFloaterScriptEdPrefs::getUIColor(LLUICtrl* ctrl, const LLSD& param)
{
    LLColorSwatchCtrl* color_swatch = dynamic_cast<LLColorSwatchCtrl*>(ctrl);
    color_swatch->setOriginal(LLUIColorTable::instance().getColor(param.asString()));
}

// <FS:Ansariel> Port old FS script prefs
void LLFloaterScriptEdPrefs::setPreprocInclude()
{
    std::string cur_name(gSavedSettings.getString("_NACL_PreProcHDDIncludeLocation"));
    std::string proposed_name(cur_name);

    (new LLDirPickerThread(boost::bind(&LLFloaterScriptEdPrefs::changePreprocIncludePath, this, _1, _2), proposed_name))->getFile();
}

void LLFloaterScriptEdPrefs::changePreprocIncludePath(const std::vector<std::string>& filenames, std::string proposed_name)
{
    std::string dir_name = filenames[0];
    if (!dir_name.empty() && dir_name != proposed_name)
    {
        std::string new_top_folder(gDirUtilp->getBaseFileName(dir_name));
        gSavedSettings.setString("_NACL_PreProcHDDIncludeLocation", dir_name);
    }
}
// </FS:Ansariel>

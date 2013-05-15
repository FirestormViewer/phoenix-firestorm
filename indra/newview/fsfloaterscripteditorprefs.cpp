/*
 * @file fsfloaterscripteditorprefs.h
 * @brief Script editor preferences
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Cinder Roxley <cinder.roxley@phoenixviewer.com>
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterscripteditorprefs.h"

#include "llcolorswatch.h"
#include "lldirpicker.h"
#include "llviewercontrol.h"

FSFloaterScriptEdPrefs::FSFloaterScriptEdPrefs(const LLSD& key)
:	LLFloater(key)
{
	mCommitCallbackRegistrar.add("ScriptPref.applyUIColor",	boost::bind(&FSFloaterScriptEdPrefs::applyUIColor, this ,_1, _2));
	mCommitCallbackRegistrar.add("ScriptPref.getUIColor",	boost::bind(&FSFloaterScriptEdPrefs::getUIColor, this ,_1, _2));
	mCommitCallbackRegistrar.add("NACL.SetPreprocInclude",	boost::bind(&FSFloaterScriptEdPrefs::setPreprocInclude, this));
}

FSFloaterScriptEdPrefs::~FSFloaterScriptEdPrefs()
{
}

/*BOOL FSFloaterScriptEdPrefs::postBuild()
{	
	return TRUE;
}*/

void FSFloaterScriptEdPrefs::applyUIColor(LLUICtrl* ctrl, const LLSD& param)
{
	LLUIColorTable::instance().setColor(param.asString(), LLColor4(ctrl->getValue()));
}

void FSFloaterScriptEdPrefs::getUIColor(LLUICtrl* ctrl, const LLSD& param)
{
	LLColorSwatchCtrl* color_swatch = (LLColorSwatchCtrl*) ctrl;
	color_swatch->setOriginal(LLUIColorTable::instance().getColor(param.asString()));
}

void FSFloaterScriptEdPrefs::setPreprocInclude()
{
	std::string cur_name(gSavedSettings.getString("_NACL_PreProcHDDIncludeLocation"));
	
	std::string proposed_name(cur_name);
	
	LLDirPicker& picker = LLDirPicker::instance();
	if (!picker.getDir(&proposed_name ))
	{
		return; //Canceled!
	}
	
	std::string dir_name = picker.getDirName();
	if (!dir_name.empty() && dir_name != cur_name)
	{
		std::string new_top_folder(gDirUtilp->getBaseFileName(dir_name));
		gSavedSettings.setString("_NACL_PreProcHDDIncludeLocation", dir_name);
	}
}

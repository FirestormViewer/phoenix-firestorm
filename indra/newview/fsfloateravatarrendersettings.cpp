/**
 * @file fsfloateravatarrendersettings.cpp
 * @brief Floater for showing persisted avatar render settings
 *
 * $LicenseInfo:firstyear=2016&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2016, Ansariel Hiller
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

#include "fsfloateravatarrendersettings.h"

#include "fscommon.h"
#include "llfiltereditor.h"
#include "llfloateravatarpicker.h"
#include "llnamelistctrl.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"


FSFloaterAvatarRenderSettings::FSFloaterAvatarRenderSettings(const LLSD& key)
 : LLFloater(key),
 mAvatarList(NULL),
 mRenderSettingChangedCallbackConnection(),
 mFilterSubString(LLStringUtil::null),
 mFilterSubStringOrig(LLStringUtil::null)
{
	mCommitCallbackRegistrar.add("Settings.AddNewEntry", boost::bind(&FSFloaterAvatarRenderSettings::onClickAdd, this, _2));
}

FSFloaterAvatarRenderSettings::~FSFloaterAvatarRenderSettings()
{
	if (mRenderSettingChangedCallbackConnection.connected())
	{
		mRenderSettingChangedCallbackConnection.disconnect();
	}
}


void FSFloaterAvatarRenderSettings::addElementToList(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_setting)
{
	static const std::string av_render_never = getString("av_render_never");
	static const std::string av_render_always = getString("av_render_always");
	static const std::string av_name_waiting = LLTrans::getString("AvatarNameWaiting");

	LLNameListCtrl::NameItem item_params;
	item_params.value = avatar_id;
	item_params.target = LLNameListCtrl::INDIVIDUAL;

	item_params.columns.add().column("name");
	item_params.name = av_name_waiting;

	std::string render_value = (render_setting == LLVOAvatar::AV_DO_NOT_RENDER ? av_render_never : av_render_always);
	item_params.columns.add().value(render_value).column("render_setting");

	mAvatarList->addNameItemRow(item_params);
}

BOOL FSFloaterAvatarRenderSettings::postBuild()
{
	mAvatarList = getChild<LLNameListCtrl>("avatar_list");
	mAvatarList->setContextMenu(&FSFloaterAvatarRenderPersistenceMenu::gFSAvatarRenderPersistenceMenu);
	mAvatarList->setFilterColumn(0);

	childSetAction("close_btn", boost::bind(&FSFloaterAvatarRenderSettings::onCloseBtn, this));

	getChild<LLFilterEditor>("filter_input")->setCommitCallback(boost::bind(&FSFloaterAvatarRenderSettings::onFilterEdit, this, _2));

	mRenderSettingChangedCallbackConnection = FSAvatarRenderPersistence::instance().setAvatarRenderSettingChangedCallback(boost::bind(&FSFloaterAvatarRenderSettings::onAvatarRenderSettingChanged, this, _1, _2));

	this->setVisibleCallback(boost::bind(&FSFloaterAvatarRenderSettings::removePicker, this));
	
	loadInitialList();

	return TRUE;
}

void FSFloaterAvatarRenderSettings::removePicker()
{
	if (mPicker.get())
	{
		mPicker.get()->closeFloater();
	}
}

void FSFloaterAvatarRenderSettings::onCloseBtn()
{
	closeFloater();
}

void FSFloaterAvatarRenderSettings::loadInitialList()
{
	FSAvatarRenderPersistence::avatar_render_setting_t avatar_render_map = FSAvatarRenderPersistence::instance().getAvatarRenderMap();
	for (FSAvatarRenderPersistence::avatar_render_setting_t::iterator it = avatar_render_map.begin(); it != avatar_render_map.end(); ++it)
	{
		addElementToList(it->first, it->second);
	}
}

void FSFloaterAvatarRenderSettings::onAvatarRenderSettingChanged(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_setting)
{
	mAvatarList->removeNameItem(avatar_id);
	if (render_setting != LLVOAvatar::AV_RENDER_NORMALLY)
	{
		addElementToList(avatar_id, render_setting);
	}
}

void FSFloaterAvatarRenderSettings::onFilterEdit(const std::string& search_string)
{
	mFilterSubStringOrig = search_string;
	LLStringUtil::trimHead(mFilterSubStringOrig);
	// Searches are case-insensitive
	std::string search_upper = mFilterSubStringOrig;
	LLStringUtil::toUpper(search_upper);

	if (mFilterSubString == search_upper)
	{
		return;
	}

	mFilterSubString = search_upper;

	// Apply new filter.
	mAvatarList->setFilterString(mFilterSubStringOrig);
}

BOOL FSFloaterAvatarRenderSettings::handleKeyHere(KEY key, MASK mask)
{
	if (FSCommon::isFilterEditorKeyCombo(key, mask))
	{
		getChild<LLFilterEditor>("filter_input")->setFocus(TRUE);
		return TRUE;
	}

	return LLFloater::handleKeyHere(key, mask);
}

void FSFloaterAvatarRenderSettings::onClickAdd(const LLSD& userdata)
{
	const std::string command_name = userdata.asString();
	LLVOAvatar::VisualMuteSettings render_setting = LLVOAvatar::AV_RENDER_NORMALLY;
	if ("never" == command_name)
	{
		render_setting = LLVOAvatar::AV_DO_NOT_RENDER;
	}
	else if ("always" == command_name)
	{
		render_setting = LLVOAvatar::AV_ALWAYS_RENDER;
	}

	LLView* button = findChild<LLButton>("plus_btn", TRUE);
	LLFloater* root_floater = gFloaterView->getParentFloater(this);
	LLFloaterAvatarPicker* picker = LLFloaterAvatarPicker::show(boost::bind(&FSFloaterAvatarRenderSettings::callbackAvatarPicked, this, _1, render_setting),
																	TRUE, TRUE, TRUE, root_floater->getName(), button);

	if (root_floater)
	{
		root_floater->addDependentFloater(picker);
	}

	mPicker = picker->getHandle();
}

void FSFloaterAvatarRenderSettings::callbackAvatarPicked(const uuid_vec_t& ids, LLVOAvatar::VisualMuteSettings render_setting)
{
	for (uuid_vec_t::const_iterator it = ids.begin(); it != ids.end(); ++it)
	{
		LLUUID avatar_id = *it;

		LLVOAvatar *avatarp = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(avatar_id));
		if (avatarp)
		{
			avatarp->setVisualMuteSettings(render_setting);
		}
		else
		{
			FSAvatarRenderPersistence::instance().setAvatarRenderSettings(avatar_id, render_setting);
		}
	}
}


//---------------------------------------------------------------------------
// Context menu
//---------------------------------------------------------------------------
namespace FSFloaterAvatarRenderPersistenceMenu
{

	FSAvatarRenderPersistenceMenu gFSAvatarRenderPersistenceMenu;

	LLContextMenu* FSAvatarRenderPersistenceMenu::createMenu()
	{
		// set up the callbacks for all of the avatar menu items
		LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;

		registrar.add("Avatar.ChangeRenderSetting", boost::bind(&FSAvatarRenderPersistenceMenu::changeRenderSetting, this, _2));

		// create the context menu from the XUI
		return createFromFile("menu_fs_avatar_render_setting.xml");
	}

	void FSAvatarRenderPersistenceMenu::changeRenderSetting(const LLSD& param)
	{
		LLVOAvatar::VisualMuteSettings render_setting = (LLVOAvatar::VisualMuteSettings)param.asInteger();

		for (uuid_vec_t::iterator it = mUUIDs.begin(); it != mUUIDs.end(); ++it)
		{
			LLUUID avatar_id = *it;

			LLVOAvatar* avatar = dynamic_cast<LLVOAvatar*>(gObjectList.findObject(avatar_id));
			if (avatar)
			{
				// Set setting via the LLVOAvatar instance if it's available; will also call FSAvatarRenderPersistence::setAvatarRenderSettings()
				avatar->setVisualMuteSettings(render_setting);
			}
			else
			{
				FSAvatarRenderPersistence::instance().setAvatarRenderSettings(avatar_id, render_setting);
			}
		}

		LLVOAvatar::cullAvatarsByPixelArea();
	}
}

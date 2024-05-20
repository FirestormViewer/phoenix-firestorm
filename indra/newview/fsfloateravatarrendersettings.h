/**
 * @file fsfloateravatarrendersettings.h
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

#ifndef FS_FLOATERAVATARRENDERSETTINGS_H
#define FS_FLOATERAVATARRENDERSETTINGS_H

#include "fsavatarrenderpersistence.h"
#include "llfloater.h"
#include "lllistcontextmenu.h"
#include "llvoavatar.h"

class LLNameListCtrl;

class FSFloaterAvatarRenderSettings : public LLFloater
{
    LOG_CLASS(FSFloaterAvatarRenderSettings);
public:
    FSFloaterAvatarRenderSettings(const LLSD& key);
    virtual ~FSFloaterAvatarRenderSettings();

    /*virtual*/ BOOL postBuild();
    /*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
    /*virtual*/ bool hasAccelerators() const { return true; }

private:
    void onCloseBtn();
    void onFilterEdit(const std::string& search_string);
    void onAvatarRenderSettingChanged(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_setting);
    void onClickAdd(const LLSD& userdata);

    void loadInitialList();
    void addElementToList(const LLUUID& avatar_id, LLVOAvatar::VisualMuteSettings render_setting);

    void callbackAvatarPicked(const uuid_vec_t& ids, LLVOAvatar::VisualMuteSettings render_setting);
    void removePicker();

    LLNameListCtrl* mAvatarList;
    LLHandle<LLFloater> mPicker;

    boost::signals2::connection mRenderSettingChangedCallbackConnection;

    std::string mFilterSubString;
    std::string mFilterSubStringOrig;
};



namespace FSFloaterAvatarRenderPersistenceMenu
{

class FSAvatarRenderPersistenceMenu : public LLListContextMenu
{
public:
    /*virtual*/ LLContextMenu* createMenu();
private:
    void changeRenderSetting(const LLSD& param);
};

extern FSAvatarRenderPersistenceMenu gFSAvatarRenderPersistenceMenu;

} // namespace FSFloaterAvatarRenderPersistenceMenu

#endif // FS_FLOATERAVATARRENDERSETTINGS_H

/**
 * @file fsfloaterwsassetblacklist.h
 * @brief Floater for Asset Blacklist and Derender
 *
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Wolfspirit Magic
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

#ifndef FS_FSFLOATERWSASSETBLACKLIST_H
#define FS_FSFLOATERWSASSETBLACKLIST_H

#include "llfloater.h"
#include "llassettype.h"


class LLScrollListCtrl;



class FSFloaterWSAssetBlacklist : public LLFloater
{
    LOG_CLASS(FSFloaterWSAssetBlacklist);
public:
    FSFloaterWSAssetBlacklist(const LLSD& key);
    virtual ~FSFloaterWSAssetBlacklist();

    /*virtual*/ void onOpen(const LLSD& key);

    /*virtual*/ BOOL postBuild();
	std::string TypeToString(S32 type);
	void BuildBlacklist();
	void addElementToList(LLUUID id, LLSD data);
	void removeElementFromList(LLUUID id);


protected:
    void onRemoveBtn();
    void onCancelBtn();

private:
	LLScrollListCtrl* mResultList;
};

#endif // FS_FSFLOATERPROFILE_H

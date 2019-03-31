/** 
 * @file fspanelprefs.h 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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

#include "fspanelprefs.h"

#include "fsdroptarget.h"
#include "lggbeamcolormapfloater.h"
#include "lggbeammapfloater.h"
#include "lggbeammaps.h"
#include "llagent.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "llinventorymodel.h"
#include "llstartup.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"

static LLPanelInjector<FSPanelPrefs> t_pref_fs("panel_preference_firestorm");

FSPanelPrefs::FSPanelPrefs() : LLPanelPreference()
{
	mCommitCallbackRegistrar.add("Perms.Copy",	boost::bind(&FSPanelPrefs::onCommitCopy, this));
	mCommitCallbackRegistrar.add("Perms.Trans",	boost::bind(&FSPanelPrefs::onCommitTrans, this));

	mEmbeddedItem = gSavedPerAccountSettings.getString("FSBuildPrefs_Item");
}

BOOL FSPanelPrefs::postBuild()
{
	// LGG's Color Beams
	refreshBeamLists();

	// Beam Colors
	getChild<LLUICtrl>("BeamColor_new")->setCommitCallback(boost::bind(&FSPanelPrefs::onBeamColorNew, this));
	getChild<LLUICtrl>("BeamColor_refresh")->setCommitCallback(boost::bind(&FSPanelPrefs::refreshBeamLists, this));
	getChild<LLUICtrl>("BeamColor_delete")->setCommitCallback(boost::bind(&FSPanelPrefs::onBeamColorDelete, this));

	// Beam Shapes
	getChild<LLUICtrl>("custom_beam_btn")->setCommitCallback(boost::bind(&FSPanelPrefs::onBeamNew, this));
	getChild<LLUICtrl>("refresh_beams")->setCommitCallback(boost::bind(&FSPanelPrefs::refreshBeamLists, this));
	getChild<LLUICtrl>("delete_beam")->setCommitCallback(boost::bind(&FSPanelPrefs::onBeamDelete, this));

	getChild<LLUICtrl>("reset_default_folders")->setCommitCallback(boost::bind(&FSPanelPrefs::onResetDefaultFolders, this));

	LLTextureCtrl* tex_ctrl = getChild<LLTextureCtrl>("texture control");
	tex_ctrl->setCommitCallback(boost::bind(&FSPanelPrefs::onCommitTexture, this, _2));
	tex_ctrl->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("DefaultObjectTexture")));

	mInvDropTarget = getChild<FSEmbeddedItemDropTarget>("embed_item");
	mInvDropTarget->setDADCallback(boost::bind(&FSPanelPrefs::onDADEmbeddedItem, this, _1));

	return LLPanelPreference::postBuild();
}

void FSPanelPrefs::onOpen(const LLSD& key)
{
	if (LLStartUp::getStartupState() == STATE_STARTED)
	{
		getChild<LLCheckBoxCtrl>("FSBuildPrefs_EmbedItem")->setEnabled(TRUE);
		mEmbeddedItem = gSavedPerAccountSettings.getString("FSBuildPrefs_Item");
		LLUUID item_id(mEmbeddedItem);
		if (item_id.isNull())
		{
			getChild<LLTextBox>("build_item_add_disp_rect_txt")->setTextArg("[ITEM]", getString("EmbeddedItemNotSet"));
		}
		else
		{
			LLInventoryObject* item = gInventory.getObject(item_id);
			if (item)
			{
				getChild<LLTextBox>("build_item_add_disp_rect_txt")->setTextArg("[ITEM]", item->getName());
			}
			else
			{
				getChild<LLTextBox>("build_item_add_disp_rect_txt")->setTextArg("[ITEM]", getString("EmbeddedItemNotAvailable"));
			}
		}
		getChild<LLUICtrl>("reset_default_folders")->setEnabled(TRUE);
	}
	else
	{
		getChild<LLCheckBoxCtrl>("FSBuildPrefs_EmbedItem")->setEnabled(FALSE);
		getChild<LLTextBox>("build_item_add_disp_rect_txt")->setTextArg("[ITEM]", getString("EmbeddedItemNotLoggedIn"));
		getChild<LLUICtrl>("reset_default_folders")->setEnabled(FALSE);
	}
}

void FSPanelPrefs::apply()
{
	LLPanelPreference::apply();

	if (LLStartUp::getStartupState() == STATE_STARTED)
	{
		gSavedPerAccountSettings.setString("FSBuildPrefs_Item", mEmbeddedItem);
	}
}

void FSPanelPrefs::cancel()
{
	LLPanelPreference::cancel();
}

void FSPanelPrefs::refreshBeamLists()
{
	static const std::string off_label = getString("BeamsOffLabel");

	LLComboBox* comboBox = findChild<LLComboBox>("FSBeamShape_combo");

	if (comboBox)
	{
		comboBox->removeall();
		comboBox->add(off_label, LLSD(""));

		string_vec_t names = gLggBeamMaps.getFileNames();
		for (string_vec_t::iterator it = names.begin(); it != names.end(); ++it)
		{
			comboBox->add(*it, LLSD(*it));
		}
		comboBox->setSimple(gSavedSettings.getString("FSBeamShape"));
	}

	comboBox = findChild<LLComboBox>("BeamColor_combo");
	if (comboBox)
	{
		comboBox->removeall();
		comboBox->add(off_label, LLSD(""));
		string_vec_t names = gLggBeamMaps.getColorsFileNames();
		for (string_vec_t::iterator it = names.begin(); it != names.end(); ++it)
		{
			comboBox->add(*it, LLSD(*it));
		}
		comboBox->setSimple(gSavedSettings.getString("FSBeamColorFile"));
	}
}

void FSPanelPrefs::onBeamColorNew()
{
	lggBeamColorMapFloater* colorMapFloater = LLFloaterReg::showTypedInstance<lggBeamColorMapFloater>("lgg_beamcolormap");
	colorMapFloater->setData(this);
}

void FSPanelPrefs::onBeamNew()
{
	lggBeamMapFloater* beamMapFloater = LLFloaterReg::showTypedInstance<lggBeamMapFloater>("lgg_beamshape");
	beamMapFloater->setData(this);
}

void FSPanelPrefs::onBeamColorDelete()
{
	LLComboBox* comboBox = findChild<LLComboBox>("BeamColor_combo");

	if (comboBox)
	{
		std::string filename = comboBox->getValue().asString() + ".xml";
		std::string path_name1(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beamsColors", filename));
		std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", filename));

		if (gDirUtilp->fileExists(path_name1))
		{
			LLFile::remove(path_name1);
			gSavedSettings.setString("FSBeamColorFile", "");
		}
		if (gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("FSBeamColorFile", "");
		}
	}
	refreshBeamLists();
}

void FSPanelPrefs::onBeamDelete()
{
	LLComboBox* comboBox = findChild<LLComboBox>("FSBeamShape_combo");

	if (comboBox)
	{
		std::string filename = comboBox->getValue().asString() + ".xml";
		std::string path_name1(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "beams", filename));
		std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beams", filename));
		
		if (gDirUtilp->fileExists(path_name1))
		{
			LLFile::remove(path_name1);
			gSavedSettings.setString("FSBeamShape", "");
		}
		if (gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("FSBeamShape", "");
		}
	}
	refreshBeamLists();
}

void FSPanelPrefs::onCommitTexture(const LLSD& data)
{
	LLTextureCtrl* texture_ctrl = findChild<LLTextureCtrl>("texture control");

	if (texture_ctrl && !texture_ctrl->getTentative())
	{
		// we grab the item id first, because we want to do a
		// permissions check
		LLUUID id = texture_ctrl->getImageItemID();
		if (id.isNull())
		{
			id = texture_ctrl->getImageAssetID();
		}
		
		// Texture picker defaults aren't inventory items
		// * Don't need to worry about permissions for them
		LLViewerInventoryItem* item = gInventory.getItem(id);
		if (item && !item->getPermissions().allowOperationBy(PERM_COPY, gAgentID))
		{
			// Do not have permission to copy the texture.
			return;
		}

		gSavedSettings.setString("FSDefaultObjectTexture", texture_ctrl->getImageAssetID().asString());
	}
}

void FSPanelPrefs::onCommitCopy()
{
	// Implements fair use
	BOOL copyable = gSavedSettings.getBOOL("NextOwnerCopy");
	if (!copyable)
	{
		gSavedSettings.setBOOL("NextOwnerTransfer", TRUE);
	}
	LLCheckBoxCtrl* xfer = getChild<LLCheckBoxCtrl>("next_owner_transfer");
	xfer->setEnabled(copyable);
}

void FSPanelPrefs::onCommitTrans()
{
	BOOL transferable = gSavedSettings.getBOOL("NextOwnerTransfer");
	if (!transferable)
	{
		gSavedSettings.setBOOL("NextOwnerCopy", TRUE);
	}
}

void FSPanelPrefs::onDADEmbeddedItem(const LLUUID& item_id)
{
	LLInventoryObject* item = gInventory.getObject(item_id);
	if (item)
	{
		getChild<LLTextBox>("build_item_add_disp_rect_txt")->setTextArg("[ITEM]", item->getName());
		mEmbeddedItem = item_id.asString();
	}
}

void FSPanelPrefs::onResetDefaultFolders()
{
	gSavedPerAccountSettings.getControl("ModelUploadFolder")->resetToDefault(true);
	gSavedPerAccountSettings.getControl("TextureUploadFolder")->resetToDefault(true);
	gSavedPerAccountSettings.getControl("SoundUploadFolder")->resetToDefault(true);
	gSavedPerAccountSettings.getControl("AnimationUploadFolder")->resetToDefault(true);
}

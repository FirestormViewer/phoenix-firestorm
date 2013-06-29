/*${License blank}*/
#include "llviewerprecompiledheaders.h"
#include "fspanelprefs.h"
#include "llcombobox.h"
#include "llcolorswatch.h"
#include "llcheckboxctrl.h"
#include "llviewercontrol.h"
#include "llfloaterreg.h"
#include "lggbeammaps.h"
#include "lggbeammapfloater.h"
#include "lggbeamcolormapfloater.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "llspinctrl.h"
#include "lldiriterator.h"	// <FS:CR> for populating the cloud combo
#include "lltexturectrl.h"
#include "llinventorymodel.h"

static LLRegisterPanelClassWrapper<PanelPreferenceFirestorm> t_pref_fs("panel_preference_firestorm");

PanelPreferenceFirestorm::PanelPreferenceFirestorm() : LLPanelPreference()
{
	mCommitCallbackRegistrar.add("Perms.Copy",	boost::bind(&PanelPreferenceFirestorm::onCommitCopy, this));
	mCommitCallbackRegistrar.add("Perms.Trans", boost::bind(&PanelPreferenceFirestorm::onCommitTrans, this));
}

BOOL PanelPreferenceFirestorm::postBuild()
{
	
	// LGG's Color Beams
	refreshBeamLists();

	// Beam Colors
	getChild<LLUICtrl>("BeamColor_new")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onBeamColor_new, this));
	getChild<LLUICtrl>("BeamColor_refresh")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::refreshBeamLists, this));
	getChild<LLUICtrl>("BeamColor_delete")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onBeamColorDelete, this));

	// Beam Shapes
	getChild<LLUICtrl>("custom_beam_btn")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onBeam_new, this));
	getChild<LLUICtrl>("refresh_beams")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::refreshBeamLists, this));
	getChild<LLUICtrl>("delete_beam")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onBeamDelete, this));

	populateCloudCombo();
	
	LLTextureCtrl* tex_ctrl = getChild<LLTextureCtrl>("texture control");
	tex_ctrl->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onCommitTexture, this, _2));
	tex_ctrl->setDefaultImageAssetID(LLUUID(gSavedSettings.getString("DefaultObjectTexture")));

	return LLPanelPreference::postBuild();	
}

void PanelPreferenceFirestorm::apply()
{
}


void PanelPreferenceFirestorm::cancel()
{
}


void PanelPreferenceFirestorm::refreshBeamLists()
{
	LLComboBox* comboBox = getChild<LLComboBox>("FSBeamShape_combo");

	if(comboBox != NULL) 
	{
		comboBox->removeall();
		comboBox->add("===OFF===");
		std::vector<std::string> names = gLggBeamMaps.getFileNames();
		for(int i=0; i<(int)names.size(); i++) 
		{
			comboBox->add(names[i]);
		}
		comboBox->setSimple(gSavedSettings.getString("FSBeamShape"));
	}

	comboBox = getChild<LLComboBox>("BeamColor_combo");
	if(comboBox != NULL) 
	{
		comboBox->removeall();
		comboBox->add("===OFF===");
		std::vector<std::string> names = gLggBeamMaps.getColorsFileNames();
		for(int i=0; i<(int)names.size(); i++) 
		{
			comboBox->add(names[i]);
		}
		comboBox->setSimple(gSavedSettings.getString("FSBeamColorFile"));
	}
}

void PanelPreferenceFirestorm::onBeamColor_new()
{
	lggBeamColorMapFloater* colorMapFloater = (lggBeamColorMapFloater*)LLFloaterReg::showInstance("lgg_beamcolormap");
	colorMapFloater->setData(this);
}

void PanelPreferenceFirestorm::onBeam_new()
{
	lggBeamMapFloater* beamMapFloater = (lggBeamMapFloater*)LLFloaterReg::showInstance("lgg_beamshape");
	beamMapFloater->setData(this);
}

void PanelPreferenceFirestorm::onBeamColorDelete()
{
	LLComboBox* comboBox = getChild<LLComboBox>("BeamColor_combo");

	if(comboBox != NULL) 
	{
		std::string filename = comboBox->getValue().asString()+".xml";
		std::string path_name1(gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS , "beamsColors", filename));
		std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "beamsColors", filename));

		if(gDirUtilp->fileExists(path_name1))
		{
			LLFile::remove(path_name1);
			gSavedSettings.setString("FSBeamColorFile","===OFF===");
		}
		if(gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("FSBeamColorFile","===OFF===");
		}
	}
	refreshBeamLists();
}

void PanelPreferenceFirestorm::onBeamDelete()
{
	LLComboBox* comboBox = getChild<LLComboBox>("FSBeamShape_combo");

	if(comboBox != NULL) 
	{
		std::string filename = comboBox->getValue().asString()+".xml";
		std::string path_name1(gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS , "beams", filename));
		std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "beams", filename));
		
		if(gDirUtilp->fileExists(path_name1))
		{
			LLFile::remove(path_name1);
			gSavedSettings.setString("FSBeamShape","===OFF===");
		}
		if(gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("FSBeamShape","===OFF===");
		}
	}
	refreshBeamLists();
}


void PanelPreferenceFirestorm::populateCloudCombo()
{
	LLComboBox* cloud_combo = getChild<LLComboBox>("cloud_combo");
	if(cloud_combo)
	{
		const std::string cloudDir(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight" + gDirUtilp->getDirDelimiter() + "clouds"));

		LLDirIterator dir_iter(cloudDir, "*.tga");
		while (1)
		{
			std::string file;
			if (!dir_iter.next(file))
			{
				break; // no more files
			}
			
			cloud_combo->add(file);
		}
		cloud_combo->setSimple(gSavedSettings.getString("FSCloudTexture"));
	}
}

void PanelPreferenceFirestorm::onCommitTexture(const LLSD& data)
{
	LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>("texture control");
	if(!texture_ctrl) return;
	if( !texture_ctrl->getTentative() )
	{
		// we grab the item id first, because we want to do a
		// permissions check
		LLUUID id = texture_ctrl->getImageItemID();
		if(id.isNull())
		{
			id = texture_ctrl->getImageAssetID();
		}
		
		// Texture picker defaults aren't inventory items
		// * Don't need to worry about permissions for them
		LLViewerInventoryItem* item = gInventory.getItem(id);
		if(item && !item->getPermissions().allowOperationBy(PERM_COPY, gAgent.getID()))
		{
			// Do not have permission to copy the texture.
			return;
		}

		gSavedSettings.setString("FSDefaultObjectTexture", texture_ctrl->getImageAssetID().asString());
	}
}

void PanelPreferenceFirestorm::onCommitCopy()
{
	// Implements fair use
	BOOL copyable = gSavedSettings.getBOOL("NextOwnerCopy");
	if(!copyable)
	{
		gSavedSettings.setBOOL("NextOwnerTransfer", TRUE);
	}
	LLCheckBoxCtrl* xfer = getChild<LLCheckBoxCtrl>("next_owner_transfer");
	xfer->setEnabled(copyable);
}

void PanelPreferenceFirestorm::onCommitTrans()
{
	BOOL transferable = gSavedSettings.getBOOL("NextOwnerTransfer");
	if (!transferable)
	{
		gSavedSettings.setBOOL("NextOwnerCopy", TRUE);
	}
}

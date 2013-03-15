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

static LLRegisterPanelClassWrapper<PanelPreferenceFirestorm> t_pref_fs("panel_preference_firestorm");

PanelPreferenceFirestorm::PanelPreferenceFirestorm() : LLPanelPreference()
{
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

	//WS: Set the combo_box vars and refresh/reload them
	m_UseLegacyClienttags = getChild<LLComboBox>("UseLegacyClienttags");		
	m_ColorClienttags = getChild<LLComboBox>("ColorClienttags");		
	m_ClientTagsVisibility = getChild<LLComboBox>("ClientTagsVisibility");	
	refreshTagCombos();

	populateCloudCombo();

	return LLPanelPreference::postBuild();	
}

void PanelPreferenceFirestorm::apply()
{
	//WS: Apply the combo_boxes for tags
	applyTagCombos();

}


void PanelPreferenceFirestorm::cancel()
{
	//WS: Refresh/Reload the Combo_boxes for tags to show the right setting.
	refreshTagCombos();
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

void PanelPreferenceFirestorm::refreshTagCombos()
{	

	//WS: Set the combo_boxes to the right value
	U32 usel_u = gSavedSettings.getU32("FSUseLegacyClienttags");
	U32 tagv_u = gSavedSettings.getU32("FSClientTagsVisibility2");
	U32 tagc_u = gSavedSettings.getU32("FSColorClienttags");


	std::string usel = llformat("%d",usel_u);
	std::string tagv = llformat("%d",tagv_u);
	std::string tagc = llformat("%d",tagc_u);
	
	m_UseLegacyClienttags->setCurrentByIndex(usel_u);
	m_ColorClienttags->setCurrentByIndex(tagc_u);
	m_ClientTagsVisibility->setCurrentByIndex(tagv_u);
	
	getChild<LLUICtrl>("usernamecolorswatch")->setValue(gSavedSettings.getColor4("FSColorUsernameColor").getValue());
	getChild<LLUICtrl>("FSColorUsername_toggle")->setValue(gSavedSettings.getBOOL("FSColorUsername"));
	getChild<LLUICtrl>("FSShowOwnTagColor_toggle")->setValue(gSavedSettings.getBOOL("FSShowOwnTagColor"));


	LLColor4 tag_color = gSavedPerAccountSettings.getColor4("FirestormTagColor");
	LLSD selectedColor("red"); // default case: red
	if (tag_color==LLColor4::blue) selectedColor = LLSD("blue");
	else if (tag_color==LLColor4::yellow) selectedColor = LLSD("yellow");
	else if (tag_color==LLColor4::purple) selectedColor = LLSD("purple");
	else if (tag_color==LLColor4((F32)0.99,(F32)0.56,(F32)0.65,(F32)1)) selectedColor = LLSD("pink");
	else if (tag_color==LLColor4::white) selectedColor = LLSD("white");
	else if (tag_color==LLColor4((F32)0.99,(F32)0.39,(F32)0.12,(F32)1)) selectedColor = LLSD("orange");
	else if (tag_color==LLColor4::green) selectedColor = LLSD("green");

	getChild<LLComboBox>("ClientTagColor")->setValue(selectedColor);
}


void PanelPreferenceFirestorm::applyTagCombos()
{
	//WS: If the user hits "apply" then write everything (if something changed) into the Debug Settings
	bool change=false;
	if(gSavedSettings.getU32("FSUseLegacyClienttags")!=m_UseLegacyClienttags->getCurrentIndex()
		|| gSavedSettings.getU32("FSColorClienttags")!=m_ColorClienttags->getCurrentIndex()
		|| gSavedSettings.getU32("FSClientTagsVisibility2")!=m_ClientTagsVisibility->getCurrentIndex()){

		gSavedSettings.setU32("FSUseLegacyClienttags",m_UseLegacyClienttags->getCurrentIndex());
		gSavedSettings.setU32("FSColorClienttags",m_ColorClienttags->getCurrentIndex());
		gSavedSettings.setU32("FSClientTagsVisibility2",m_ClientTagsVisibility->getCurrentIndex());
		
		//WS: Clear all nametags to make everything display properly!
		change=true;
	}

	if(LLColor4(getChild<LLUICtrl>("usernamecolorswatch")->getValue()) != gSavedSettings.getColor4("FSColorUsernameColor"))
	{
		gSavedSettings.setColor4("FSColorUsernameColor",LLColor4(getChild<LLUICtrl>("usernamecolorswatch")->getValue()));
		change=true;
	}
	if(getChild<LLUICtrl>("FSColorUsername_toggle")->getValue().asBoolean() != (LLSD::Boolean)gSavedSettings.getBOOL("FSColorUsername")){
		gSavedSettings.setBOOL("FSColorUsername",getChild<LLUICtrl>("FSColorUsername_toggle")->getValue().asBoolean());
		change=true;
	}
	if(getChild<LLUICtrl>("FSShowOwnTagColor_toggle")->getValue().asBoolean() != (LLSD::Boolean)gSavedSettings.getBOOL("FSShowOwnTagColor")){
		gSavedSettings.setBOOL("FSShowOwnTagColor",getChild<LLUICtrl>("FSShowOwnTagColor_toggle")->getValue().asBoolean());
		change=true;
	}


	LLColor4 tag_color=LLColor4::red;

	std::string selectedColor(getChild<LLComboBox>("ClientTagColor")->getValue().asString());
	if (selectedColor == "blue") tag_color = LLColor4::blue;
	else if (selectedColor == "yellow") tag_color = LLColor4::yellow;
	else if (selectedColor == "purple") tag_color = LLColor4::purple;
	else if (selectedColor == "pink") tag_color = LLColor4((F32)0.99,(F32)0.56,(F32)0.65,(F32)1);
	else if (selectedColor == "white") tag_color = LLColor4::white;
	else if (selectedColor == "orange") tag_color = LLColor4((F32)0.99,(F32)0.39,(F32)0.12,(F32)1);
	else if (selectedColor == "green") tag_color = LLColor4::green;

	if(tag_color!=gSavedPerAccountSettings.getColor4("FirestormTagColor")){
		gSavedPerAccountSettings.setColor4("FirestormTagColor",tag_color);
		if(gAgentAvatarp.notNull())	gAgentAvatarp->forceBakeAllTextures(true);
		if(gSavedSettings.getBOOL("FSShowOwnTagColor")) change=true;
	}

	if(change) LLVOAvatar::invalidateNameTags();
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

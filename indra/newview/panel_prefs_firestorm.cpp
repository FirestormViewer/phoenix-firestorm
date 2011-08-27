/*${License blank}*/
#include "llviewerprecompiledheaders.h"
#include "panel_prefs_firestorm.h"
#include "llcombobox.h"
#include "llcolorswatch.h"
#include "llcheckboxctrl.h"
#include "llviewercontrol.h"
#include "llfloaterreg.h"
#include "lggbeammaps.h"
#include "lggbeammapfloater.h"
#include "lggbeamcolormapfloater.h"
#include "lggautocorrectfloater.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"

static LLRegisterPanelClassWrapper<PanelPreferenceFirestorm> t_pref_fs("panel_preference_firestorm");

PanelPreferenceFirestorm::PanelPreferenceFirestorm() : LLPanelPreference(), m_calcLineEditor(NULL), m_acLineEditor(NULL), m_tp2LineEditor(NULL), m_clearchatLineEditor(NULL), m_musicLineEditor(NULL)
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

	//autocorrect button
	getChild<LLUICtrl>("lgg_ac_showgui")->setCommitCallback(boost::bind(&PanelPreferenceFirestorm::onAutoCorrectSettings, this));


	// m_calcLineEditor = getChild<LLLineEditor>("PhoenixCmdLineCalc");
	m_acLineEditor = getChild<LLLineEditor>("PhoenixCmdLineAutocorrect");
	m_tp2LineEditor = getChild<LLLineEditor>("PhoenixCmdLineTP2");
	m_clearchatLineEditor = getChild<LLLineEditor>("PhoenixCmdLineClearChat");
	m_musicLineEditor = getChild<LLLineEditor>("PhoenixCmdLineMusic");
	m_aoLineEditor = getChild<LLLineEditor>("PhoenixCmdLineAO");
	// if(m_calcLineEditor)
	// {
		// m_calcLineEditor->setEnabled(FALSE);
	// }
	if(m_acLineEditor)
	{
		m_acLineEditor->setEnabled(FALSE);
	}
	if(m_tp2LineEditor)
	{
		m_tp2LineEditor->setEnabled(FALSE);
	}
	if(m_clearchatLineEditor)
	{
		m_clearchatLineEditor->setEnabled(FALSE);
	}
	if(m_musicLineEditor)
	{
		m_musicLineEditor->setEnabled(FALSE);
	}
	if(m_aoLineEditor)
	{
		m_aoLineEditor->setEnabled(FALSE);
	}
	
	
	//WS: Set the combo_box vars and refresh/reload them
	m_UseLegacyClienttags = getChild<LLComboBox>("UseLegacyClienttags");		
	m_ColorClienttags = getChild<LLComboBox>("ColorClienttags");		
	m_ClientTagsVisibility = getChild<LLComboBox>("ClientTagsVisibility");	
	refreshTagCombos();


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
	LLComboBox* comboBox = getChild<LLComboBox>("PhoenixBeamShape_combo");

	if(comboBox != NULL) 
	{
		comboBox->removeall();
		comboBox->add("===OFF===");
		std::vector<std::string> names = gLggBeamMaps.getFileNames();
		for(int i=0; i<(int)names.size(); i++) 
		{
			comboBox->add(names[i]);
		}
		comboBox->setSimple(gSavedSettings.getString("PhoenixBeamShape"));
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
		comboBox->setSimple(gSavedSettings.getString("PhoenixBeamColorFile"));
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
			gSavedSettings.setString("PhoenixBeamColorFile","===OFF===");
		}
		if(gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("PhoenixBeamColorFile","===OFF===");
		}
	}
	refreshBeamLists();
}

void PanelPreferenceFirestorm::onBeamDelete()
{
	LLComboBox* comboBox = getChild<LLComboBox>("PhoenixBeamShape_combo");

	if(comboBox != NULL) 
	{
		std::string filename = comboBox->getValue().asString()+".xml";
		std::string path_name1(gDirUtilp->getExpandedFilename( LL_PATH_APP_SETTINGS , "beams", filename));
		std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "beams", filename));
		
		if(gDirUtilp->fileExists(path_name1))
		{
			LLFile::remove(path_name1);
			gSavedSettings.setString("PhoenixBeamShape","===OFF===");
		}
		if(gDirUtilp->fileExists(path_name2))
		{
			LLFile::remove(path_name2);
			gSavedSettings.setString("PhoenixBeamShape","===OFF===");
		}
	}
	refreshBeamLists();
}
void PanelPreferenceFirestorm::onAutoCorrectSettings()
{
	LGGAutoCorrectFloater::showFloater();
}



void PanelPreferenceFirestorm::refreshTagCombos()
{	

	//WS: Set the combo_boxes to the right value
	U32 usel_u = gSavedSettings.getU32("FSUseLegacyClienttags");
	U32 tagv_u = gSavedSettings.getU32("FSClientTagsVisibility");
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


	LLColor4 tag_color = gSavedSettings.getColor4("FirestormTagColor");
	if(tag_color==LLColor4::red) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Red"));
	if(tag_color==LLColor4::blue) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Blue"));
	if(tag_color==LLColor4::yellow) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Yellow"));
	if(tag_color==LLColor4::purple) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Purple"));
	if(tag_color==LLColor4((F32)0.99,(F32)0.56,(F32)0.65,(F32)1)) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Pink"));
	if(tag_color==LLColor4::white) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("White"));
	if(tag_color==LLColor4((F32)0.99,(F32)0.39,(F32)0.12,(F32)1)) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Orange"));
	if(tag_color==LLColor4::green) getChild<LLComboBox>("ClientTagColor")->setSimple(LLStringExplicit("Green"));


}


void PanelPreferenceFirestorm::applyTagCombos()
{
	//WS: If the user hits "apply" then write everything (if something changed) into the Debug Settings
	bool change=false;
	if(gSavedSettings.getU32("FSUseLegacyClienttags")!=m_UseLegacyClienttags->getCurrentIndex()
		|| gSavedSettings.getU32("FSColorClienttags")!=m_ColorClienttags->getCurrentIndex()
		|| gSavedSettings.getU32("FSClientTagsVisibility")!=m_ClientTagsVisibility->getCurrentIndex()){

		gSavedSettings.setU32("FSUseLegacyClienttags",m_UseLegacyClienttags->getCurrentIndex());
		gSavedSettings.setU32("FSColorClienttags",m_ColorClienttags->getCurrentIndex());
		gSavedSettings.setU32("FSClientTagsVisibility",m_ClientTagsVisibility->getCurrentIndex());
		
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
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Blue") tag_color=LLColor4::blue;
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Yellow") tag_color=LLColor4::yellow;
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Purple") tag_color=LLColor4::purple;
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Pink") tag_color=LLColor4((F32)0.99,(F32)0.56,(F32)0.65,(F32)1);
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="White") tag_color=LLColor4::white;
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Orange") tag_color=LLColor4((F32)0.99,(F32)0.39,(F32)0.12,(F32)1);
	if(getChild<LLComboBox>("ClientTagColor")->getSimple()=="Green") tag_color=LLColor4::green;


	if(tag_color!=gSavedSettings.getColor4("FirestormTagColor")){
		gSavedSettings.setColor4("FirestormTagColor",tag_color);
		if(gAgentAvatarp!=NULL)	gAgentAvatarp->forceBakeAllTextures(true);
		if(gSavedSettings.getBOOL("FSShowOwnTagColor")) change=true;
	}


	if(change) LLVOAvatar::invalidateNameTags();

}
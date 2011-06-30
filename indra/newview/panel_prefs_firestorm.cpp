/*${License blank}*/
#include "llviewerprecompiledheaders.h"
#include "panel_prefs_firestorm.h"
#include "llcombobox.h"
#include "llviewercontrol.h"
#include "llfloaterreg.h"
#include "lggbeammaps.h"
#include "lggbeammapfloater.h"
#include "lggbeamcolormapfloater.h"
#include "lggautocorrectfloater.h"

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
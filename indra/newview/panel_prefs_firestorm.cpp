/*${License blank}*/
#include "llviewerprecompiledheaders.h"
#include "panel_prefs_firestorm.h"


static LLRegisterPanelClassWrapper<PanelPreferenceFirestorm> t_pref_fs("panel_preference_firestorm");

PanelPreferenceFirestorm::PanelPreferenceFirestorm() : LLPanelPreference(), m_calcLineEditor(NULL), m_acLineEditor(NULL), m_tp2LineEditor(NULL), m_clearchatLineEditor(NULL), m_musicLineEditor(NULL)
{
}


BOOL PanelPreferenceFirestorm::postBuild()
{
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

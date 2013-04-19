/*${License blank}*/

#ifndef FS_PANELPREFS_H
#define FS_PANELPREFS_H

#include "llfloaterpreference.h"
#include "lllineeditor.h"

class LLLineEditor;

class PanelPreferenceFirestorm : public LLPanelPreference
{
public:
	PanelPreferenceFirestorm();
	
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void apply();
	/*virtual*/ void cancel();

	void refreshBeamLists();
	void onBeamColor_new();
	void onBeam_new();
	void onBeamColorDelete();
	void onBeamDelete();

	void refreshTagCombos();
	void applyTagCombos();
	void populateCloudCombo();
	
	void onCommitTexture(const LLSD& data);
	void onCommitCopy();
	void onCommitTrans();

protected:
	LLComboBox* m_UseLegacyClienttags;
	LLComboBox* m_ColorClienttags;
	LLComboBox* m_ClientTagsVisibility;
};

#endif // FS_PANELPREFS_H

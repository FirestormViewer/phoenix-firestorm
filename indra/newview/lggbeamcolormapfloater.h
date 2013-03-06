/** 
 * @file lggbeamcolormapfloater.h
 * @brief Floater for beam colors
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#ifndef LGG_BEAMCOLORMAPFLOATER_H
#define LGG_BEAMCOLORMAPFLOATER_H

#include "llviewerprecompiledheaders.h"

#include "llagentdata.h"
#include "llcommandhandler.h"
#include "llfloater.h"
#include "llsdutil.h"
#include "lluictrlfactory.h"
#include "llviewercontrol.h"
#include "llagent.h"
#include "llfilepicker.h"
#include "llpanel.h"
#include "lliconctrl.h"
#include "llbutton.h"
#include "llcolorswatch.h"

#include "llsdserialize.h"
#include "fspanelprefs.h"
#include "lggbeamscolors.h"
#include "llsliderctrl.h"
#include "llfocusmgr.h"


////////////////////////////////////////////////////////////////////////////
// lggBeamMapFloater
class lggBeamColorMapFloater : public LLFloater
{
public:
	lggBeamColorMapFloater(const LLSD& seed);
	virtual ~lggBeamColorMapFloater();
	
	void fixOrder();

	BOOL postBuild(void);
	BOOL handleMouseDown(S32 x,S32 y,MASK mask);
	BOOL handleRightMouseDown(S32 x,S32 y,MASK mask);
	void update();
	
	void setData(void* data);

	void draw();
	
	LLSD getMyDataSerialized();
	
	// UI Handlers
	static void onClickSlider(LLUICtrl* crtl, void* userdata);

	void onClickSave();
	void onClickLoad();
	void onClickCancel();
	
	
protected:
	F32 mContextConeOpacity;
	PanelPreferenceFirestorm * fspanel;
	lggBeamsColors myData; 
	LLSliderCtrl* mColorSlider;

};

#endif // LGG_BEAMCOLORMAPFLOATER_H
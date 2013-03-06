/** 
 * @file lggbeammapfloater.h
 * @brief Floater for beam shapes
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This code is free. It comes
 * WITHOUT ANY WARRANTY, without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://sam.zoy.org/wtfpl/COPYING for more details.
 */

#ifndef LGG_BEAMMAPFLOATER_H
#define LGG_BEAMMAPFLOATER_H

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
#include "llfocusmgr.h"


class lggPoint
{
	public:
		S32 x;
		S32 y;
		LLColor4 c;
};

////////////////////////////////////////////////////////////////////////////
// lggBeamMapFloater
class lggBeamMapFloater : public LLFloater
{
public:
	lggBeamMapFloater(const LLSD& seed);
	virtual ~lggBeamMapFloater();
	

	BOOL postBuild(void);
	BOOL handleMouseDown(S32 x,S32 y,MASK mask);
	void update();
	BOOL handleRightMouseDown(S32 x,S32 y,MASK mask);
	
	void setData(void* data);
	PanelPreferenceFirestorm* fspanel;

	void draw();
	void clearPoints();

	LLSD getMyDataSerialized();
	
	std::vector<lggPoint> dots;

	// UI Handlers
	void onClickSave();
	void onClickClear();
	void onClickLoad();

	
private:
	static void onBackgroundChange(LLUICtrl* ctrl, void* userdata);

	F32 mContextConeOpacity;
};

#endif // LGG_BEAMMAPFLOATER_H
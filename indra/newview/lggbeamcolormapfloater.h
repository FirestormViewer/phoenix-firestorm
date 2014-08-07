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

#include "llfloater.h"

#include "lggbeamscolors.h"

class FSPanelPrefs;
class LLSliderCtrl;

////////////////////////////////////////////////////////////////////////////
// lggBeamMapFloater
class lggBeamColorMapFloater : public LLFloater
{
public:
	lggBeamColorMapFloater(const LLSD& seed);
	virtual ~lggBeamColorMapFloater();

	BOOL postBuild(void);
	BOOL handleMouseDown(S32 x,S32 y,MASK mask);
	BOOL handleRightMouseDown(S32 x,S32 y,MASK mask);

	void setData(void* data);

	void draw();

protected:
	// UI Handlers
	void onClickSlider();
	void onClickSave();
	void onClickLoad();
	void onClickCancel();

	void fixOrder();
	LLSD getMyDataSerialized();

	F32				mContextConeOpacity;
	FSPanelPrefs*	mFSPanel;
	lggBeamsColors	myData;
	LLSliderCtrl*	mColorSlider;
};

#endif // LGG_BEAMCOLORMAPFLOATER_H

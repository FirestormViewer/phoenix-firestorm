/** 
 * @file lggbeamcolormapfloater.cpp
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

#include "llviewerprecompiledheaders.h"

#include "lggbeamcolormapfloater.h"

#include "fspanelprefs.h"
#include "lggbeammaps.h"
#include "llcolorswatch.h"
#include "llfilepicker.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "llviewercontrol.h"

F32 convertXToHue(S32 place)
{
	return ((place - 6) / 396.0f) * 720.0f;
}

S32 convertHueToX(F32 place)
{
	return ll_round((place / 720.0f) * 396.0f) + 6;
}


const F32 CONTEXT_CONE_IN_ALPHA = 0.0f;
const F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
const F32 CONTEXT_FADE_TIME = 0.08f;

// Correction factors needed after porting from Phoenix
const S32 CORRECTION_X = 0;
const S32 CORRECTION_Y = -40;

void lggBeamColorMapFloater::onClickSlider()
{
	fixOrder();
}

void lggBeamColorMapFloater::draw()
{
	static const std::string start_hue_label = getString("start_hue");
	static const std::string end_hue_label = getString("end_hue");

	//set the color of the preview thing
	LLColor4 bColor = LLColor4(lggBeamMaps::beamColorFromData(myData));
	mBeamColorPreview->set(bColor, TRUE);
	
	//Try draw rectangle attach beam
	LLRect swatch_rect;
	LLButton* createButton = mFSPanel->getChild<LLButton>("BeamColor_new");
	
	createButton->localRectToOtherView(createButton->getLocalRect(), &swatch_rect, this);
	LLRect local_rect = getLocalRect();
	if (gFocusMgr.childHasKeyboardFocus(this) && mFSPanel->isInVisibleChain() && mContextConeOpacity > 0.001f)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLEnable(GL_CULL_FACE);
		gGL.begin(LLRender::TRIANGLE_STRIP);
		{
			F32 r = bColor.mV[0];
			F32 g = bColor.mV[1];
			F32 b = bColor.mV[2];

			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
			gGL.color4f(r, g, b, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
			gGL.color4f(r, g, b, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
		}
		gGL.end();
	}

	static LLCachedControl<F32> opacity(gSavedSettings, "PickerContextOpacity");
	mContextConeOpacity = lerp(mContextConeOpacity, opacity(), LLCriticalDamp::getInterpolant(CONTEXT_FADE_TIME));

	//Draw Base Stuff
	LLFloater::draw();
	
	//Draw hues and pointers at color
	gGL.pushMatrix();
	F32 r, g, b;
	LLColor4 output;
	for (S32 i = 0; i <= 720; ++i)
	{
		S32 hi = i % 360;
		hslToRgb((hi / 360.0f), 1.0f, 0.5f, r, g, b);
		output.set(r, g, b);

		gl_line_2d(
			convertHueToX(i) + CORRECTION_X, 201 + CORRECTION_Y,
			convertHueToX(i) + CORRECTION_X, 277 + CORRECTION_Y, output);
	}

	S32 X1 = convertHueToX(myData.mStartHue) + CORRECTION_X;
	S32 X2 = convertHueToX(myData.mEndHue) + CORRECTION_X;
	LLFontGL* font = LLFontGL::getFontSansSerifSmall();

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y, 9.0f, 30, false);

	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y, 8.0f, 30, false);

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X1 + CORRECTION_X, 238 + CORRECTION_Y, 7.0f, 30, false);

	gl_line_2d(X1 + 1 + CORRECTION_X, 210 + CORRECTION_Y, X1 + 1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1 - 1 + CORRECTION_X, 210 + CORRECTION_Y, X1-1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1 + CORRECTION_X, 210 + CORRECTION_Y, X1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::black);

	gl_line_2d(X1 - 25 + CORRECTION_X, 238 + 1 + CORRECTION_Y, X1 + 25 + CORRECTION_X, 238 + 1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1 - 25 + CORRECTION_X, 238 - 1 + CORRECTION_Y, X1 + 25 + CORRECTION_X, 238 - 1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X1 - 25 + CORRECTION_X, 238 + CORRECTION_Y, X1 + 25 + CORRECTION_X, 238 + CORRECTION_Y, LLColor4::black);

	font->renderUTF8(
		start_hue_label, 0,
		X1 + CORRECTION_X,
		212 + CORRECTION_Y,
		LLColor4::white, LLFontGL::HCENTER,
		LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 9.0f, 30, false);

	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 8.0f, 30, false);

	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(X2 + CORRECTION_X, 238 + CORRECTION_Y, 7.0f, 30, false);

	gl_line_2d(X2 + 1 + CORRECTION_X, 210 + CORRECTION_Y, X2 + 1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2 - 1 + CORRECTION_X, 210 + CORRECTION_Y, X2 - 1 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2 + CORRECTION_X ,210 + CORRECTION_Y, X2 + CORRECTION_X, 266 + CORRECTION_Y, LLColor4::black);

	gl_line_2d(X2 - 25 + CORRECTION_X, 238 + 1 + CORRECTION_Y, X2 + 25 + CORRECTION_X, 238 + 1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2 - 25 + CORRECTION_X, 238 - 1 + CORRECTION_Y, X2 + 25 + CORRECTION_X, 238 - 1 + CORRECTION_Y, LLColor4::white);
	gl_line_2d(X2 - 25 + CORRECTION_X, 238 + CORRECTION_Y, X2 + 25 + CORRECTION_X, 238 + CORRECTION_Y, LLColor4::black);

	font->renderUTF8(
		end_hue_label, 0,
		X2 + CORRECTION_X,
		212 + CORRECTION_Y,
		LLColor4::white, LLFontGL::HCENTER,
		LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);

	gGL.popMatrix();
}

lggBeamColorMapFloater::~lggBeamColorMapFloater()
{
}

lggBeamColorMapFloater::lggBeamColorMapFloater(const LLSD& seed) : LLFloater(seed),
	mContextConeOpacity(0.0f)
{
}

BOOL lggBeamColorMapFloater::postBuild()
{
	setCanMinimize(FALSE);

	getChild<LLUICtrl>("BeamColor_Save")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickSave, this));
	getChild<LLUICtrl>("BeamColor_Load")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickLoad, this));
	getChild<LLUICtrl>("BeamColor_Cancel")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickCancel, this));

	mColorSlider = getChild<LLSliderCtrl>("BeamColor_Speed");
	mColorSlider->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickSlider, this));

	mBeamColorPreview = getChild<LLColorSwatchCtrl>("BeamColor_Preview");

	fixOrder();

	return TRUE;
}

BOOL lggBeamColorMapFloater::handleMouseDown(S32 x,S32 y,MASK mask)
{
	if (y > (201 + CORRECTION_Y) &&  y < (277 + CORRECTION_Y))
	{
		if (x < (6 + CORRECTION_X))
		{
			myData.mStartHue=0.0f;
		}
		else if (x > (402 + CORRECTION_X))
		{
			myData.mEndHue = 720.0f;
		}
		else
		{
			myData.mStartHue  = convertXToHue(x + CORRECTION_X);
		}

		fixOrder();
	}

	LL_DEBUGS() << "we got clicked at (" << x << ", " << y << " yay! " << LL_ENDL;

	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL lggBeamColorMapFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (y > (201 + CORRECTION_Y) &&  y < (277 + CORRECTION_Y))
	{
		if (x < (6 + CORRECTION_X))
		{
			myData.mStartHue = 0.0f;
		}
		else if (x > (402 + CORRECTION_X))
		{
			myData.mEndHue = 720.0f;
		}
		else
		{
			myData.mEndHue  = convertXToHue(x + CORRECTION_X);
		}

		fixOrder();
	}
	LL_DEBUGS() << "we got right clicked at (" << x << ", " << y << " yay! " << LL_ENDL;

	return LLFloater::handleRightMouseDown(x, y, mask);
}

void lggBeamColorMapFloater::fixOrder()
{
	myData.mRotateSpeed = mColorSlider->getValueF32();
	myData.mRotateSpeed /= 100.0f;

	if(myData.mEndHue < myData.mStartHue)
	{
		F32 temp = myData.mStartHue;
		myData.mStartHue = myData.mEndHue;
		myData.mEndHue = temp;
	}
}

void lggBeamColorMapFloater::setData(void* data)
{
	mFSPanel = (FSPanelPrefs*)data;
	if (mFSPanel)
	{
		gFloaterView->getParentFloater(mFSPanel)->addDependentFloater(this);
	}
}

LLSD lggBeamColorMapFloater::getMyDataSerialized()
{
	return myData.toLLSD();
}

void lggBeamColorMapFloater::onClickSave()
{
	LLFilePicker& picker = LLFilePicker::instance();

	std::string path_name2(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", ""));

	std::string filename=path_name2 + "myNewBeamColor.xml";
	if (!picker.getSaveFile(LLFilePicker::FFSAVE_BEAM, filename))
	{
		return;
	}

	filename = path_name2 + gDirUtilp->getBaseFileName(picker.getFirstFile());

	LLSD main = getMyDataSerialized();

	llofstream export_file;
	export_file.open( filename.c_str() );
	LLSDSerialize::toPrettyXML(main, export_file);
	export_file.close();

	gSavedSettings.setString("FSBeamColorFile", gDirUtilp->getBaseFileName(filename, true));

	if (mFSPanel)
	{
		mFSPanel->refreshBeamLists();
	}
	closeFloater();
}

void lggBeamColorMapFloater::onClickCancel()
{
	closeFloater();
}

void lggBeamColorMapFloater::onClickLoad()
{
	LLFilePicker& picker = LLFilePicker::instance();
	if (!picker.getOpenFile(LLFilePicker::FFLOAD_XML))
	{
		return;
	}
	LLSD minedata;
	llifstream importer( picker.getFirstFile().c_str() );
	LLSDSerialize::fromXMLDocument(minedata, importer);

	myData = lggBeamsColors::fromLLSD(minedata);
	childSetValue("BeamColor_Speed", myData.mRotateSpeed * 100);
}


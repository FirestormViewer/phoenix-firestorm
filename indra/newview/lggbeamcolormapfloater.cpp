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
#include "llviewermenufile.h"

// Correction factors needed after porting from Phoenix
const S32 CORRECTION_X = 0;
const S32 CORRECTION_Y = -40;

F32 convertXToHue(S32 place)
{
	return ((place - 6) / 396.0f) * 720.0f;
}

S32 convertHueToX(F32 place)
{
	return ll_round((place / 720.0f) * 396.0f) + 6;
}

lggBeamColorMapFloater::lggBeamColorMapFloater(const LLSD& seed) : LLFloater(seed),
	mContextConeOpacity(0.f),
	mContextConeInAlpha(0.f),
	mContextConeOutAlpha(0.f),
	mContextConeFadeTime(0.f)
{
	mContextConeInAlpha = gSavedSettings.getF32("ContextConeInAlpha");
	mContextConeOutAlpha = gSavedSettings.getF32("ContextConeOutAlpha");
	mContextConeFadeTime = gSavedSettings.getF32("ContextConeFadeTime");
}

lggBeamColorMapFloater::~lggBeamColorMapFloater()
{
}

BOOL lggBeamColorMapFloater::postBuild()
{
	getChild<LLUICtrl>("BeamColor_Save")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickSave, this));
	getChild<LLUICtrl>("BeamColor_Load")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickLoad, this));
	getChild<LLUICtrl>("BeamColor_Cancel")->setCommitCallback(boost::bind(&lggBeamColorMapFloater::closeFloater, this, false));

	mColorSlider = getChild<LLSliderCtrl>("BeamColor_Speed");
	mColorSlider->setCommitCallback(boost::bind(&lggBeamColorMapFloater::onClickSlider, this));

	mBeamColorPreview = getChild<LLColorSwatchCtrl>("BeamColor_Preview");

	fixOrder();

	return TRUE;
}

void lggBeamColorMapFloater::draw()
{
	static const std::string start_hue_label = getString("start_hue");
	static const std::string end_hue_label = getString("end_hue");

	//set the color of the preview thing
	LLColor4 bColor = LLColor4(lggBeamMaps::beamColorFromData(mData));
	mBeamColorPreview->set(bColor, TRUE);

	static LLCachedControl<F32> max_opacity(gSavedSettings, "PickerContextOpacity", 0.4f);
	drawConeToOwner(mContextConeOpacity, max_opacity, mFSPanel->getChild<LLButton>("BeamColor_new"), mContextConeFadeTime, mContextConeInAlpha, mContextConeOutAlpha);

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

	S32 X1 = convertHueToX(mData.mStartHue) + CORRECTION_X;
	S32 X2 = convertHueToX(mData.mEndHue) + CORRECTION_X;
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

BOOL lggBeamColorMapFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
	F32 hue = getHueFromLocation(x, y);
	if (hue != -1.f)
	{
		mData.mStartHue = hue;
		fixOrder();
		
		return TRUE;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

BOOL lggBeamColorMapFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	F32 hue = getHueFromLocation(x, y);
	if (hue != -1.f)
	{
		mData.mEndHue = hue;
		fixOrder();
		
		return TRUE;
	}

	return LLFloater::handleRightMouseDown(x, y, mask);
}

void lggBeamColorMapFloater::onClickSlider()
{
	fixOrder();
}

F32 lggBeamColorMapFloater::getHueFromLocation(S32 x, S32 y)
{
	if (y > (201 + CORRECTION_Y) &&  y < (277 + CORRECTION_Y))
	{
		if (x < (6 + CORRECTION_X))
		{
			return 0.0f;
		}
		else if (x > (402 + CORRECTION_X))
		{
			return 720.0f;
		}
		else
		{
			return convertXToHue(x + CORRECTION_X);
		}
	}

	return -1.f;
}

void lggBeamColorMapFloater::fixOrder()
{
	mData.mRotateSpeed = mColorSlider->getValueF32();
	mData.mRotateSpeed /= 100.0f;

	if (mData.mEndHue < mData.mStartHue)
	{
		llswap(mData.mStartHue, mData.mEndHue);
	}
}

void lggBeamColorMapFloater::setData(FSPanelPrefs* data)
{
	mFSPanel = data;
	if (mFSPanel)
	{
		gFloaterView->getParentFloater(mFSPanel)->addDependentFloater(this);
	}
}

LLSD lggBeamColorMapFloater::getDataSerialized()
{
	return mData.toLLSD();
}

void lggBeamColorMapFloater::onClickSave()
{
	std::string filename(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "beamsColors", "NewBeamColor.xml"));
	(new LLFilePickerReplyThread(boost::bind(&lggBeamColorMapFloater::onSaveCallback, this, _1), LLFilePicker::FFSAVE_BEAM, filename))->getFile();
}

void lggBeamColorMapFloater::onSaveCallback(const std::vector<std::string>& filenames)
{
	std::string filename = filenames[0];

	LLSD export_data = getDataSerialized();

	llofstream export_file;
	export_file.open(filename.c_str());
	LLSDSerialize::toPrettyXML(export_data, export_file);
	export_file.close();

	gSavedSettings.setString("FSBeamColorFile", gDirUtilp->getBaseFileName(filename, true));

	if (mFSPanel)
	{
		mFSPanel->refreshBeamLists();
	}
	closeFloater();
}

void lggBeamColorMapFloater::onClickLoad()
{
	(new LLFilePickerReplyThread(boost::bind(&lggBeamColorMapFloater::onLoadCallback, this, _1), LLFilePicker::FFLOAD_XML, false))->getFile();
}

void lggBeamColorMapFloater::onLoadCallback(const std::vector<std::string>& filenames)
{
	LLSD import_data;
	llifstream importer(filenames[0].c_str());
	LLSDSerialize::fromXMLDocument(import_data, importer);

	mData = lggBeamsColors::fromLLSD(import_data);
	childSetValue("BeamColor_Speed", mData.mRotateSpeed * 100.f);
}

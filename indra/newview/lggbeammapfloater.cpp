/** 
 * @file lggbeammapfloater.cpp
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

#include "llviewerprecompiledheaders.h"

#include "lggbeammapfloater.h"

#include "fspanelprefs.h"
#include "lggbeammaps.h"
#include "llcolorswatch.h"
#include "llfilepicker.h"
#include "llsdserialize.h"
#include "llviewercontrol.h"
#include "llviewermenufile.h"

const F32 CONTEXT_CONE_IN_ALPHA = 0.0f;
const F32 CONTEXT_CONE_OUT_ALPHA = 1.f;
const F32 CONTEXT_FADE_TIME = 0.08f;


void lggBeamMapFloater::clearPoints()
{
	mDots.clear();
}

void lggBeamMapFloater::draw()
{
	LLRect swatch_rect;
	LLButton* createButton = mFSPanel->getChild<LLButton>("custom_beam_btn");

	createButton->localRectToOtherView(createButton->getLocalRect(), &swatch_rect, this);
	LLRect local_rect = getLocalRect();
	if (gFocusMgr.childHasKeyboardFocus(this) && createButton->isInVisibleChain() && mContextConeOpacity > 0.001f)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLEnable(GL_CULL_FACE);
		gGL.begin(LLRender::TRIANGLE_STRIP);
		{
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mTop);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mTop);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mRight, swatch_rect.mBottom);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mRight, local_rect.mBottom);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mBottom);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mBottom);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_IN_ALPHA * mContextConeOpacity);
			gGL.vertex2i(swatch_rect.mLeft, swatch_rect.mTop);
			gGL.color4f(0.f, 0.f, 0.f, CONTEXT_CONE_OUT_ALPHA * mContextConeOpacity);
			gGL.vertex2i(local_rect.mLeft, local_rect.mTop);
		}
		gGL.end();
	}

	static LLCachedControl<F32> opacity(gSavedSettings, "PickerContextOpacity");
	mContextConeOpacity = lerp(mContextConeOpacity, opacity(), LLCriticalDamp::getInterpolant(CONTEXT_FADE_TIME));

	LLFloater::draw();
	LLRect rec = mBeamshapePanel->getRect();

	gGL.pushMatrix();
	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(rec.getCenterX(), rec.getCenterY(), 2.0f, 30, false);
	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(rec.getCenterX(), rec.getCenterY(), 30.0f, 30, false);
	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(rec.getCenterX(), rec.getCenterY(), 60.0f, 30, false);
	gGL.color4fv(LLColor4::black.mV);
	gl_circle_2d(rec.getCenterX(), rec.getCenterY(), 90.0f, 30, false);
	gGL.color4fv(LLColor4::white.mV);
	gl_circle_2d(rec.getCenterX(), rec.getCenterY(), 120.0f, 30, false);

	for (std::vector<lggPoint>::iterator it = mDots.begin(); it != mDots.end(); ++it)
	{
		lggPoint dot = *it;

		gGL.color4fv(LLColor4::white.mV);
		gl_circle_2d(dot.x, dot.y, 9.0f, 30, true);

		gGL.color4fv(LLColor4::black.mV);
		gl_circle_2d(dot.x, dot.y, 8.0f, 30, true);

		gGL.color4fv(dot.c.mV);
		gl_circle_2d(dot.x, dot.y, 7.0f, 30, true);
	}
	gGL.popMatrix();
}

lggBeamMapFloater::~lggBeamMapFloater()
{
}

lggBeamMapFloater::lggBeamMapFloater(const LLSD& seed) : LLFloater(seed),
	mContextConeOpacity(0.0f)
{
}

BOOL lggBeamMapFloater::postBuild()
{
	setCanMinimize(FALSE);

	getChild<LLUICtrl>("beamshape_save")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickSave, this));
	getChild<LLUICtrl>("beamshape_clear")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickClear, this));
	getChild<LLUICtrl>("beamshape_load")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickLoad, this));

	getChild<LLColorSwatchCtrl>("back_color_swatch")->setCommitCallback(boost::bind(&lggBeamMapFloater::onBackgroundChange, this));
	getChild<LLColorSwatchCtrl>("beam_color_swatch")->setColor(LLColor4::red);

	mBeamshapePanel = getChild<LLPanel>("beamshape_draw");

	return TRUE;
}

BOOL lggBeamMapFloater::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (y > 39 && x > 16 && x < 394 && y < 317)
	{
		lggPoint a;
		a.x = x;
		a.y = y;
		a.c = getChild<LLColorSwatchCtrl>("beam_color_swatch")->get();
		mDots.push_back(a);
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

void lggBeamMapFloater::setData(void* data)
{
	mFSPanel = (FSPanelPrefs*)data;
	if (mFSPanel)
	{
		gFloaterView->getParentFloater(mFSPanel)->addDependentFloater(this);
	}
}

BOOL lggBeamMapFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	std::vector<lggPoint> newDots;
	for (std::vector<lggPoint>::iterator it = mDots.begin(); it != mDots.end(); ++it)
	{
		lggPoint dot = *it;
		if (dist_vec(LLVector2(x, y), LLVector2(dot.x, dot.y)) >= 7)
		{
			newDots.push_back(dot);
		}

	}
	mDots = newDots;

	return LLFloater::handleMouseDown(x, y, mask);
}

void lggBeamMapFloater::onBackgroundChange()
{
	mBeamshapePanel->setBackgroundColor(getChild<LLColorSwatchCtrl>("back_color_swatch")->get());
}

LLSD lggBeamMapFloater::getMyDataSerialized()
{
	LLSD out;
	LLRect r  = mBeamshapePanel->getRect();
	for (S32 i = 0; i < mDots.size(); ++i)
	{
		LLSD point;
		lggPoint t = mDots[i];
		LLVector3 vec = LLVector3(0.f, (F32)t.x, (F32)t.y);
		vec -= LLVector3(0.f, (F32)r.getCenterX(), (F32)r.getCenterY());
		
		point["offset"] = vec.getValue();
		point["color"] = t.c.getValue();

		out[i] = point;
	}
	return out;
}

void lggBeamMapFloater::onClickSave()
{
	std::string filename(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beams", "NewBeam.xml"));
	(new LLFilePickerReplyThread(boost::bind(&lggBeamMapFloater::onSaveCallback, this, _1), LLFilePicker::FFSAVE_BEAM, filename))->getFile();
}

void lggBeamMapFloater::onSaveCallback(const std::vector<std::string>& filenames)
{
	std::string filename = filenames[0];

	LLSD export_data;
	export_data["scale"] = 8.0f / (mBeamshapePanel->getRect().getWidth());
	export_data["data"] = getMyDataSerialized();

	llofstream export_file;
	export_file.open(filename.c_str());
	LLSDSerialize::toPrettyXML(export_data, export_file);
	export_file.close();
	gSavedSettings.setString("FSBeamShape", gDirUtilp->getBaseFileName(filename, true));

	if (mFSPanel)
	{
		mFSPanel->refreshBeamLists();
	}
}

void lggBeamMapFloater::onClickClear()
{
	clearPoints();
}

void lggBeamMapFloater::onClickLoad()
{
	(new LLFilePickerReplyThread(boost::bind(&lggBeamMapFloater::onLoadCallback, this, _1), LLFilePicker::FFLOAD_XML, false))->getFile();
}

void lggBeamMapFloater::onLoadCallback(const std::vector<std::string>& filenames)
{
	mDots.clear();
	LLSD import_data;
	llifstream importer(filenames[0].c_str());
	LLSDSerialize::fromXMLDocument(import_data, importer);
	LLSD picture = import_data["data"];
	F32 scale = (F32)import_data["scale"].asReal();

	for (LLSD::array_iterator it = picture.beginArray(); it != picture.endArray(); ++it)
	{
		LLRect rec = mBeamshapePanel->getRect();

		LLSD beam_data = *it;
		lggPoint p;
		LLVector3 vec = LLVector3(beam_data["offset"]);
		vec *= scale / (8.0f / rec.getWidth());
		LLColor4 color = LLColor4(beam_data["color"]);
		p.c = color;
		p.x = (S32)(vec.mV[VY] + rec.getCenterX());
		p.y = (S32)(vec.mV[VZ] + rec.getCenterY());

		mDots.push_back(p);
	}
}

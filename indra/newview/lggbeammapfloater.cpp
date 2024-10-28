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

lggBeamMapFloater::lggBeamMapFloater(const LLSD& seed) : LLFloater(seed),
    mContextConeOpacity(0.f),
    mContextConeInAlpha(CONTEXT_CONE_IN_ALPHA),
    mContextConeOutAlpha(CONTEXT_CONE_OUT_ALPHA),
    mContextConeFadeTime(CONTEXT_CONE_FADE_TIME),
    mFSPanel(nullptr),
    mBeamshapePanel(nullptr)
{
}

bool lggBeamMapFloater::postBuild()
{
    getChild<LLUICtrl>("beamshape_save")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickSave, this));
    getChild<LLUICtrl>("beamshape_clear")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickClear, this));
    getChild<LLUICtrl>("beamshape_load")->setCommitCallback(boost::bind(&lggBeamMapFloater::onClickLoad, this));
    getChild<LLUICtrl>("cancel")->setCommitCallback(boost::bind(&lggBeamMapFloater::closeFloater, this, false));

    getChild<LLColorSwatchCtrl>("back_color_swatch")->setCommitCallback(boost::bind(&lggBeamMapFloater::onBackgroundChange, this));
    getChild<LLColorSwatchCtrl>("beam_color_swatch")->setColor(LLColor4::red);

    mBeamshapePanel = getChild<LLPanel>("beamshape_draw");

    return true;
}

void lggBeamMapFloater::draw()
{
    if (mFSPanel)
    {
        static LLCachedControl<F32> max_opacity(gSavedSettings, "PickerContextOpacity", 0.4f);
        drawConeToOwner(mContextConeOpacity, max_opacity, mFSPanel->getChild<LLButton>("custom_beam_btn"), mContextConeFadeTime, mContextConeInAlpha, mContextConeOutAlpha);
    }

    LLFloater::draw();
    const LLRect& rec = mBeamshapePanel->getRect();

    gGL.pushMatrix();
    gGL.color4fv(LLColor4::white.mV);
    gl_circle_2d((F32)rec.getCenterX(), (F32)rec.getCenterY(), 2.0f, 30, false);
    gGL.color4fv(LLColor4::black.mV);
    gl_circle_2d((F32)rec.getCenterX(), (F32)rec.getCenterY(), 30.0f, 30, false);
    gGL.color4fv(LLColor4::white.mV);
    gl_circle_2d((F32)rec.getCenterX(), (F32)rec.getCenterY(), 60.0f, 30, false);
    gGL.color4fv(LLColor4::black.mV);
    gl_circle_2d((F32)rec.getCenterX(), (F32)rec.getCenterY(), 90.0f, 30, false);
    gGL.color4fv(LLColor4::white.mV);
    gl_circle_2d((F32)rec.getCenterX(), (F32)rec.getCenterY(), 120.0f, 30, false);

    for (const auto& dot : mDots)
    {
        gGL.color4fv(LLColor4::white.mV);
        gl_circle_2d((F32)dot.x, (F32)dot.y, 9.0f, 30, true);

        gGL.color4fv(LLColor4::black.mV);
        gl_circle_2d((F32)dot.x, (F32)dot.y, 8.0f, 30, true);

        gGL.color4fv(dot.c.mV);
        gl_circle_2d((F32)dot.x, (F32)dot.y, 7.0f, 30, true);
    }
    gGL.popMatrix();
}

bool lggBeamMapFloater::handleMouseDown(S32 x, S32 y, MASK mask)
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

bool lggBeamMapFloater::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
    std::vector<lggPoint> newDots;
    for (const auto& dot : mDots)
    {
        if (dist_vec(LLVector2((F32)x, (F32)y), LLVector2((F32)dot.x, (F32)dot.y)) >= 7)
        {
            newDots.emplace_back(dot);
        }

    }
    mDots = newDots;

    return LLFloater::handleMouseDown(x, y, mask);
}

void lggBeamMapFloater::onBackgroundChange()
{
    mBeamshapePanel->setBackgroundColor(getChild<LLColorSwatchCtrl>("back_color_swatch")->get());
}

LLSD lggBeamMapFloater::getDataSerialized() const
{
    LLSD out;
    LLRect r  = mBeamshapePanel->getRect();
    for (S32 i = 0; i < mDots.size(); ++i)
    {
        LLSD point;
        const lggPoint& dot = mDots.at(i);
        LLVector3 vec{ 0.f, (F32)dot.x, (F32)dot.y };
        vec -= LLVector3(0.f, (F32)r.getCenterX(), (F32)r.getCenterY());

        point["offset"] = vec.getValue();
        point["color"] = dot.c.getValue();

        out[i] = point;
    }
    return out;
}

void lggBeamMapFloater::onClickSave()
{
    std::string filename(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS , "beams", "NewBeam.xml"));
    LLFilePickerReplyThread::startPicker(boost::bind(&lggBeamMapFloater::onSaveCallback, this, _1), LLFilePicker::FFSAVE_BEAM, filename);
}

void lggBeamMapFloater::onSaveCallback(const std::vector<std::string>& filenames)
{
    std::string filename = filenames[0];

    LLSD export_data;
    export_data["scale"] = 8.0f / (mBeamshapePanel->getRect().getWidth());
    export_data["data"] = getDataSerialized();

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
    LLFilePickerReplyThread::startPicker(boost::bind(&lggBeamMapFloater::onLoadCallback, this, _1), LLFilePicker::FFLOAD_XML, false);
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
        LLVector3 vec{ beam_data["offset"] };
        vec *= scale / (8.0f / rec.getWidth());
        LLColor4 color = LLColor4(beam_data["color"]);
        p.c = color;
        p.x = (S32)(vec.mV[VY] + rec.getCenterX());
        p.y = (S32)(vec.mV[VZ] + rec.getCenterY());

        mDots.push_back(p);
    }
}

void lggBeamMapFloater::setData(FSPanelPrefs* data)
{
    mFSPanel = data;
    if (mFSPanel)
    {
        gFloaterView->getParentFloater(mFSPanel)->addDependentFloater(this);
    }
}

void lggBeamMapFloater::clearPoints()
{
    mDots.clear();
}

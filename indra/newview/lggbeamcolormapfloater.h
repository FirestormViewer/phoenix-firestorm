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
class LLColorSwatchCtrl;
class LLSliderCtrl;

////////////////////////////////////////////////////////////////////////////
// lggBeamMapFloater
class lggBeamColorMapFloater : public LLFloater
{
public:
    lggBeamColorMapFloater(const LLSD& seed);

    bool postBuild() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;

    void setData(FSPanelPrefs* data);

    void draw() override;

protected:
    // UI Handlers
    void onClickSlider();
    void onClickSave();
    void onClickLoad();

    void onSaveCallback(const std::vector<std::string>& filenames);
    void onLoadCallback(const std::vector<std::string>& filenames);

    F32  getHueFromLocation(S32 x, S32 y) const;
    void fixOrder();
    LLSD getDataSerialized() const;

    F32                 mContextConeOpacity;
    F32                 mContextConeInAlpha;
    F32                 mContextConeOutAlpha;
    F32                 mContextConeFadeTime;
    FSPanelPrefs*       mFSPanel;
    lggBeamsColors      mData;
    LLSliderCtrl*       mColorSlider;
    LLColorSwatchCtrl*  mBeamColorPreview;
};

#endif // LGG_BEAMCOLORMAPFLOATER_H

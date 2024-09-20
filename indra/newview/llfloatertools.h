/**
 * @file llfloatertools.h
 * @brief The edit tools, including move, position, land, etc.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLFLOATERTOOLS_H
#define LL_LLFLOATERTOOLS_H

#include "llfloater.h"
#include "llcoord.h"
#include "llparcelselection.h"

class LLButton;
class LLComboBox;
class LLCheckBoxCtrl;
class LLComboBox;
class LLPanelPermissions;
class LLPanelObject;
class LLPanelVolume;
class LLPanelContents;
class LLPanelFace;
class LLPanelLandInfo;
class LLRadioGroup;
class LLSlider;
class LLTabContainer;
class LLTextBox;
class LLTool;
class LLParcelSelection;
class LLObjectSelection;
class LLLandImpactsObserver;

// <FS:Zi> switchable edit texture/materials panel
#include "llgltfmaterial.h"
#include "llmaterial.h"
#include "llrender.h"

class FSPanelFace;
// </FS:Zi>

typedef LLSafeHandle<LLObjectSelection> LLObjectSelectionHandle;

class LLFloaterTools
: public LLFloater
{
public:
    virtual bool    postBuild();
    static  void*   createPanelPermissions(void*    vdata);
    static  void*   createPanelObject(void* vdata);
    static  void*   createPanelVolume(void* vdata);
    static  void*   createPanelFace(void*   vdata);
    static  void*   createPanelContents(void*   vdata);
    static  void*   createPanelLandInfo(void*   vdata);

    LLFloaterTools(const LLSD& key);
    virtual ~LLFloaterTools();

    /*virtual*/ void onOpen(const LLSD& key);
    /*virtual*/ bool canClose();
    /*virtual*/ void onClose(bool app_quitting);
    /*virtual*/ void draw();
    /*virtual*/ void onFocusReceived();

    // call this once per frame to handle visibility, rect location,
    // button highlights, etc.
    void updatePopup(LLCoordGL center, MASK mask);

    // <FS:CR> Aurora Sim
    void updateToolsSizeLimits();
    // </FS:CR> Aurora Sim
    void changePrecision(S32 decimal_precision);

    // When the floater is going away, reset any options that need to be
    // cleared.
    void resetToolState();

    enum EInfoPanel
    {
        PANEL_GENERAL=0,
        PANEL_OBJECT,
        PANEL_FEATURES,
        PANEL_FACE,
        PANEL_CONTENTS,
        PANEL_COUNT
    };

    void dirty();
    void showPanel(EInfoPanel panel);

    void setStatusText(const std::string& text);
    static void setEditTool(void* data);
    void setTool(const LLSD& user_data);
    void saveLastTool();
    void updateLandImpacts();

    static void setGridMode(S32 mode);

    // <FS:Zi> switchable edit texture/materials panel
    // LLPanelFace* getPanelFace() { return mPanelFace; }
    LLRender::eTexIndex getTextureDropChannel();
    LLRender::eTexIndex getTextureChannelToEdit();
    LLGLTFMaterial::TextureInfo getPBRDropChannel();
    LLMaterialPtr createDefaultMaterial(LLMaterialPtr old_mat);
    void refreshPanelFace();
    // </FS:Zi>

    void onClickBtnCopyKeys();
    void onClickExpand();

private:
    void refresh();
    static void setObjectType( LLPCode pcode );
    void onClickGridOptions();

    // <FS:Ansariel> FIRE-7802: Grass and tree selection in build tool
    void buildTreeGrassCombo();
    void onSelectTreeGrassCombo();
    // </FS:Ansariel>

public:
    LLButton        *mBtnFocus;
    LLButton        *mBtnMove;
    LLButton        *mBtnEdit;
    LLButton        *mBtnCreate;
    LLButton        *mBtnLand;

    LLTextBox       *mTextStatus;

    // Focus buttons
    LLRadioGroup*   mRadioGroupFocus;

    // Move buttons
    LLRadioGroup*   mRadioGroupMove;

    // Edit buttons
    LLRadioGroup*   mRadioGroupEdit;

    LLCheckBoxCtrl  *mCheckSelectIndividual;
    LLButton*       mBtnLink;
    LLButton*       mBtnUnlink;

    // <FS:PP> FIRE-14493: Buttons to cycle through linkset
    LLButton*       mBtnPrevPart;
    LLButton*       mBtnNextPart;
    // </FS:PP>

    LLCheckBoxCtrl* mCheckSnapToGrid;
    LLButton*       mBtnGridOptions;
    LLComboBox*     mComboGridMode;
    LLCheckBoxCtrl* mCheckStretchUniform;
    LLCheckBoxCtrl* mCheckStretchTexture;
    LLCheckBoxCtrl* mCheckShowHighlight; //Phoenix:KC
    LLCheckBoxCtrl* mCheckActualRoot; //Phoenix:KC
    LLCheckBoxCtrl* mCheckSelectProbes; // <FS:PP>

    // Ansariel: Reverted the hack because then when clicking the label it
    //           doesn't check the checkbox anymore!
    // !HACK! Replacement of mCheckStretchUniform label because LLCheckBoxCtrl
    //  doesn't support word_wrap of its label. Need to fix truncation bug EXT-6658

    LLButton    *mBtnRotateLeft;
    LLButton    *mBtnRotateReset;
    LLButton    *mBtnRotateRight;

    LLButton    *mBtnDelete;
    LLButton    *mBtnDuplicate;
    LLButton    *mBtnDuplicateInPlace;

    LLTextBox*  mTextSelectionCount = nullptr;
    LLTextBox*  mTextSelectionEmpty = nullptr;
    LLTextBox*  mTextSelectionFaces = nullptr;
    LLSlider*   mSliderZoom = nullptr;

    // <FS:Ansariel> Performance improvements
    LLTextBox*  mTextLinkNumObjCount = nullptr;
    LLTextBox*  mTextMoreInfoLabel = nullptr;
    LLButton*   mBtnCopyKeys = nullptr;

    // <FS:Ansariel> FIRE-7802: Grass and tree selection in build tool
    LLComboBox*     mTreeGrassCombo;

    // Create buttons
    LLCheckBoxCtrl  *mCheckSticky;
    LLCheckBoxCtrl  *mCheckCopySelection;
    LLCheckBoxCtrl  *mCheckCopyCenters;
    LLCheckBoxCtrl  *mCheckCopyRotates;

    // Land buttons
    LLRadioGroup*   mRadioGroupLand = nullptr;
    LLSlider        *mSliderDozerSize = nullptr;
    LLSlider        *mSliderDozerForce = nullptr;
    LLTextBox*      mTextBulldozer = nullptr;
    LLTextBox*      mTextDozerSize = nullptr;
    LLTextBox*      mTextDozerStrength = nullptr;

    LLButton        *mBtnApplyToSelection;

    std::vector<LLButton*>  mButtons;//[ 15 ];

    LLTabContainer  *mTab;
    LLPanelPermissions      *mPanelPermissions;
    LLPanelObject           *mPanelObject;
    LLPanelVolume           *mPanelVolume;
    LLPanelContents         *mPanelContents;
//  LLPanelFace             *mPanelFace;         // <FS:Zi> switchable edit texture/materials panel
    LLPanelLandInfo         *mPanelLandInfo;

    LLViewBorder*           mCostTextBorder;

    LLTabContainer*         mTabLand;

    LLLandImpactsObserver*  mLandImpactsObserver;

    LLParcelSelectionHandle mParcelSelection;
    LLObjectSelectionHandle mObjectSelection;

private:
    bool                    mDirty;
    bool                    mHasSelection;
    bool                    mOpen; //Phoenix:KC

    //Phoenix:KC
    S32                 mCollapsedHeight;
    S32                 mExpandedHeight;
    std::map<std::string, std::string> mStatusText;

    LLPanelFace*            mPanelFace;         // <FS:Zi> switchable edit texture/materials panel
    FSPanelFace*            mFSPanelFace;       // <FS:Zi> switchable edit texture/materials panel

public:
    static bool     sShowObjectCost;
    static bool     sPreviousFocusOnAvatar;

};

extern LLFloaterTools *gFloaterTools;

#endif

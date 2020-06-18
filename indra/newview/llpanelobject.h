/** 
 * @file llpanelobject.h
 * @brief Object editing (position, scale, etc.) in the tools floater
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#ifndef LL_LLPANELOBJECT_H
#define LL_LLPANELOBJECT_H

#include "v3math.h"
#include "llpanel.h"
#include "llpointer.h"
#include "llvolume.h"

class LLSpinCtrl;
class LLCheckBoxCtrl;
class LLTextBox;
class LLUICtrl;
class LLButton;
class LLMenuButton;
class LLViewerObject;
class LLComboBox;
class LLColorSwatchCtrl;
class LLTextureCtrl;
class LLInventoryItem;
class LLUUID;

class LLPanelObject : public LLPanel
{
public:
	LLPanelObject();
	virtual ~LLPanelObject();

	virtual BOOL	postBuild();
	virtual void	draw();
	virtual void 	clearCtrls();

	void			changePrecision(S32 decimal_precision);	// <FS:CR> Adjustable decimal precision
	void 			updateLimits(BOOL attachment);// <AW: opensim-limits>
	void			refresh();

	static bool		precommitValidate(const LLSD& data);
	
	static void		onCommitLock(LLUICtrl *ctrl, void *data);
	static void 	onCommitPosition(		LLUICtrl* ctrl, void* userdata);
	static void 	onCommitScale(			LLUICtrl* ctrl, void* userdata);
	static void 	onCommitRotation(		LLUICtrl* ctrl, void* userdata);
	static void 	onCommitTemporary(		LLUICtrl* ctrl, void* userdata);
	static void 	onCommitPhantom(		LLUICtrl* ctrl, void* userdata);
	static void 	onCommitPhysics(		LLUICtrl* ctrl, void* userdata);
	//<FS:Beq> FIRE-21445 - Display specific LOD
	void			onCommitLOD();
	//</FS:Beq>

    void            onCopyPos();
    void            onPastePos();
    void            onCopySize();
    void            onPasteSize();
    void            onCopyRot();
    void            onPasteRot();
    void            onCopyParams();
    void            onPasteParams();
    // <FS> Extended copy & paste buttons
    void            onPastePosClip();
    void            onPasteSizeClip();
    void            onPasteRotClip();
    // </FS>

	static void 	onCommitParametric(LLUICtrl* ctrl, void* userdata);


	void     		onCommitSculpt(const LLSD& data);
	void     		onCancelSculpt(const LLSD& data);
	void     		onSelectSculpt(const LLSD& data);
	BOOL     		onDropSculpt(LLInventoryItem* item);
	static void     onCommitSculptType(    LLUICtrl *ctrl, void* userdata);

    // <FS> Extended copy & paste buttons
    //void            menuDoToSelected(const LLSD& userdata);
    //bool            menuEnableItem(const LLSD& userdata);
    bool            pasteCheckMenuItem(const LLSD& userdata);
    void            pasteDoMenuItem(const LLSD& userdata);
    bool            pasteEnabledMenuItem(const LLSD& userdata);
    // </FS>

protected:
	void			getState();
	//<FS:Beq> FIRE-21445 + Mesh Info in object panel
	void			deactivateStandardFields();
	void			activateMeshFields(LLViewerObject * objectp);
	void			setLODDistValues(LLTextBox * tb, F32 factor, F32 dmid, F32 dlow, F32 dlowest);
	void			deactivateMeshFields();
	//</FS:Beq>
	void			sendRotation(BOOL btn_down);
	void			sendScale(BOOL btn_down);
	void			sendPosition(BOOL btn_down);
	void			sendIsPhysical();
	void			sendIsTemporary();
	void			sendIsPhantom();

	void            sendSculpt();
	
	void 			getVolumeParams(LLVolumeParams& volume_params);
	
protected:
	S32				mComboMaterialItemCount;

	LLComboBox*		mComboMaterial;
	
	// Per-object options
	LLComboBox*		mComboBaseType;

	//LLMenuButton*	mMenuClipboardParams; // <FS> Extended copy & paste buttons

	LLComboBox*		mComboLOD;

	LLTextBox*		mLabelCut;
	LLSpinCtrl*		mSpinCutBegin;
	LLSpinCtrl*		mSpinCutEnd;
// <AW: opensim-limits>
	F32			mRegionMaxHeight;

	F32			mMinScale;
	F32			mMaxScale;

	F32			mMaxHollowSize;
// </AW: opensim-limits>
	LLTextBox*		mLabelHollow;
	LLSpinCtrl*		mSpinHollow;

	F32			mMinHoleSize;// <AW: opensim-limits>
	LLTextBox*		mLabelHoleType;
	LLComboBox*		mComboHoleType;

	LLTextBox*		mLabelTwist;
	LLSpinCtrl*		mSpinTwist;
	LLSpinCtrl*		mSpinTwistBegin;

	LLSpinCtrl*		mSpinScaleX;
	LLSpinCtrl*		mSpinScaleY;
	
	LLTextBox*		mLabelSkew;
	LLSpinCtrl*		mSpinSkew;

	LLTextBox*		mLabelShear;
	LLSpinCtrl*		mSpinShearX;
	LLSpinCtrl*		mSpinShearY;

	// Advanced Path
	LLSpinCtrl*		mCtrlPathBegin;
	LLSpinCtrl*		mCtrlPathEnd;

	LLTextBox*		mLabelTaper;
	LLSpinCtrl*		mSpinTaperX;
	LLSpinCtrl*		mSpinTaperY;

	LLTextBox*		mLabelRadiusOffset;
	LLSpinCtrl*		mSpinRadiusOffset;

	LLTextBox*		mLabelRevolutions;
	LLSpinCtrl*		mSpinRevolutions;

	//LLMenuButton*   mMenuClipboardPos; // <FS> Extended copy & paste buttons
	LLTextBox*		mLabelPosition;
	LLSpinCtrl*		mCtrlPosX;
	LLSpinCtrl*		mCtrlPosY;
	LLSpinCtrl*		mCtrlPosZ;

	//LLMenuButton*   mMenuClipboardSize; // <FS> Extended copy & paste buttons
	LLTextBox*		mLabelSize;
	LLSpinCtrl*		mCtrlScaleX;
	LLSpinCtrl*		mCtrlScaleY;
	LLSpinCtrl*		mCtrlScaleZ;
	BOOL			mSizeChanged;

	//LLMenuButton*   mMenuClipboardRot; // <FS> Extended copy & paste buttons
	LLTextBox*		mLabelRotation;
	LLSpinCtrl*		mCtrlRotX;
	LLSpinCtrl*		mCtrlRotY;
	LLSpinCtrl*		mCtrlRotZ;

    LLButton        *mBtnCopyPos;
    LLButton        *mBtnPastePos;
    LLButton        *mBtnCopySize;
    LLButton        *mBtnPasteSize;
    LLButton        *mBtnCopyRot;
    LLButton        *mBtnPasteRot;
    LLButton        *mBtnCopyParams;
    LLButton        *mBtnPasteParams;
    LLMenuButton    *mBtnPasteMenu;

    // <FS> Extended copy & paste buttons
    LLButton        *mBtnPastePosClip;
    LLButton        *mBtnPasteSizeClip;
    LLButton        *mBtnPasteRotClip;
    // </FS>

	LLCheckBoxCtrl	*mCheckLock;
	LLCheckBoxCtrl	*mCheckPhysics;
	LLCheckBoxCtrl	*mCheckTemporary;
	LLCheckBoxCtrl	*mCheckPhantom;

	LLTextureCtrl   *mCtrlSculptTexture;
	LLTextBox       *mLabelSculptType;
	LLComboBox      *mCtrlSculptType;
	LLCheckBoxCtrl  *mCtrlSculptMirror;
	LLCheckBoxCtrl  *mCtrlSculptInvert;

	LLVector3		mCurEulerDegrees;		// to avoid sending rotation when not changed
	BOOL			mIsPhysical;			// to avoid sending "physical" when not changed
	BOOL			mIsTemporary;			// to avoid sending "temporary" when not changed
	BOOL			mIsPhantom;				// to avoid sending "phantom" when not changed
	S32				mSelectedType;			// So we know what selected type we last were

	LLUUID          mSculptTextureRevert;   // so we can revert the sculpt texture on cancel
	U8              mSculptTypeRevert;      // so we can revert the sculpt type on cancel

    LLVector3       mClipboardPos;
    LLVector3           mClipboardSize;
    LLVector3       mClipboardRot;
    LLSD            mClipboardParams;

    bool            mHasClipboardPos;
    bool            mHasClipboardSize;
    bool            mHasClipboardRot;
    bool            mHasClipboardParams;
    // <FS> Extended copy & paste buttons
    bool            mPasteParametric;
    bool            mPasteFlexible;
    bool            mPastePhysics;
    bool            mPasteLight;
    // </FS>

	LLPointer<LLViewerObject> mObject;
	LLPointer<LLViewerObject> mRootObject;
};

#endif

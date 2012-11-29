/** 
 * @file llpreviewtexture.h
 * @brief LLPreviewTexture class definition
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

#ifndef LL_LLPREVIEWTEXTURE_H
#define LL_LLPREVIEWTEXTURE_H

#include "llpreview.h"
#include "llbutton.h"
#include "llframetimer.h"
#include "llviewertexture.h"

// <FS:Ansariel> Metadata reader
#include "llavatarnamecache.h"

class LLComboBox;
class LLImageRaw;

class LLPreviewTexture : public LLPreview
{
public:
	enum EFileformatType
	{
		FORMAT_TGA,
		FORMAT_PNG
	};


	LLPreviewTexture(const LLSD& key);
	~LLPreviewTexture();

	virtual void		draw();

	virtual BOOL		canSaveAs() const;
	virtual void		saveAs();
	void				saveAs(EFileformatType format);

	virtual void		loadAsset();
	virtual EAssetStatus	getAssetStatus();
	
	virtual void		reshape(S32 width, S32 height, BOOL called_from_parent = TRUE);
	virtual void 		onFocusReceived();
	
	static void			onFileLoadedForSaveTGA( 
							BOOL success,
							LLViewerFetchedTexture *src_vi,
							LLImageRaw* src, 
							LLImageRaw* aux_src,
							S32 discard_level, 
							BOOL final,
							void* userdata );
	static void			onFileLoadedForSavePNG( 
							BOOL success,
							LLViewerFetchedTexture *src_vi,
							LLImageRaw* src, 
							LLImageRaw* aux_src,
							S32 discard_level, 
							BOOL final,
							void* userdata );
	void 				openToSave();
	
	static void			onSaveAsBtn(LLUICtrl* ctrl, void* data);

	/*virtual*/ void setObjectID(const LLUUID& object_id);

	// <FS:Techwolf Lupindo> texture comment metadata reader
	void callbackLoadName(const LLUUID& agent_id, const LLAvatarName& av_name);
	void onButtonClickProfile();
	void onButtonClickUUID();
	// </FS:Techwolf Lupindo>
	
protected:
	void				init();
	/* virtual */ BOOL	postBuild();
	bool				setAspectRatio(const F32 width, const F32 height);
	static void			onAspectRatioCommit(LLUICtrl*,void* userdata);
	
private:
	void				updateImageID(); // set what image is being uploaded.
	void				updateDimensions();
	LLUUID				mImageID;
	LLPointer<LLViewerFetchedTexture>		mImage;
	S32                 mImageOldBoostLevel;
	std::string			mSaveFileName;
	LLFrameTimer		mSavedFileTimer;
	BOOL				mLoadingFullImage;
	BOOL                mShowKeepDiscard;
	BOOL                mCopyToInv;

	// Save the image once it's loaded.
	BOOL                mPreviewToSave;

	// This is stored off in a member variable, because the save-as
	// button and drag and drop functionality need to know.
	BOOL mIsCopyable;
	BOOL mUpdateDimensions;
	S32 mLastHeight;
	S32 mLastWidth;
	F32 mAspectRatio;

	bool mShowingButtons;
	bool mDisplayNameCallback;
	LLUIString mUploaderDateTime;

	LLLoadedCallbackEntry::source_callback_list_t mCallbackTextureList ; 
};
#endif  // LL_LLPREVIEWTEXTURE_H

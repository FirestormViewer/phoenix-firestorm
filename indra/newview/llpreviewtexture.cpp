/** 
 * @file llpreviewtexture.cpp
 * @brief LLPreviewTexture class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llwindow.h"

#include "llpreviewtexture.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llbutton.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llfilepicker.h"
#include "llfloaterreg.h"
#include "llimagetga.h"
#include "llimagepng.h"
#include "llinventory.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "lltrans.h"
#include "lltextbox.h"
#include "lltextureview.h"
#include "llui.h"
#include "llviewercontrol.h" // <FS:PP> For user-defined default save format for textures
#include "llviewerinventory.h"
#include "llviewertexture.h"
#include "llviewertexturelist.h"
#include "lluictrlfactory.h"
#include "llviewerwindow.h"
#include "lllineeditor.h"
#include "lltexteditor.h"

#include "llimagepng.h"

#include "fscommon.h"

const S32 CLIENT_RECT_VPAD = 4;

const F32 SECONDS_TO_SHOW_FILE_SAVED_MSG = 8.f;

const F32 PREVIEW_TEXTURE_MAX_ASPECT = 200.f;
const F32 PREVIEW_TEXTURE_MIN_ASPECT = 0.005f;

// <FS:Ansariel> FIRE-14111: File extension missing on Linux when saving a texture
std::string checkFileExtension(const std::string& filename, LLPreviewTexture::EFileformatType format)
{
	std::string tmp_name = filename;
	std::string extension = (format == LLPreviewTexture::FORMAT_TGA ? ".tga" : ".png");

	LLStringUtil::toLower(tmp_name);
	size_t result = tmp_name.rfind(extension);
	if (result == std::string::npos || result != tmp_name.length() - extension.size())
	{
		return (filename + extension);
	}

	return filename;
}
// </FS:Ansariel>

LLPreviewTexture::LLPreviewTexture(const LLSD& key)
	: LLPreview((key.has("uuid") ? key.get("uuid") : key)), // Changed for texture preview mode
	  mLoadingFullImage( FALSE ),
	  mShowKeepDiscard(FALSE),
	  mCopyToInv(FALSE),
	  mIsCopyable(FALSE),
	  mIsFullPerm(FALSE),
	  mUpdateDimensions(TRUE),
	  mLastHeight(0),
	  mLastWidth(0),
	  mAspectRatio(0.f),
	  mPreviewToSave(FALSE),
	  mImage(NULL),
	  mImageOldBoostLevel(LLGLTexture::BOOST_NONE),
	  mShowingButtons(false),
	  mDisplayNameCallback(false),
	  mAvatarNameCallbackConnection(),
	  // <FS:Ansariel> Performance improvement
	  mCurrentImageWidth(0),
	  mCurrentImageHeight(0)
	  // </FS:Ansariel>
{
	updateImageID();
	if (key.has("save_as"))
	{
		mPreviewToSave = TRUE;
	}
	// Texture preview mode
	if (key.has("preview_only"))
	{
		mShowKeepDiscard = FALSE;
		mCopyToInv = FALSE;
		mIsCopyable = FALSE;
		mPreviewToSave = FALSE;
		mIsFullPerm = FALSE;
	}
}

LLPreviewTexture::~LLPreviewTexture()
{
	// <FS:Ansariel> Handle avatar name callback
	if (mAvatarNameCallbackConnection.connected())
	{
		mAvatarNameCallbackConnection.disconnect();
	}
	// </FS:Ansariel>

	LLLoadedCallbackEntry::cleanUpCallbackList(&mCallbackTextureList) ;

	if( mLoadingFullImage )
	{
		getWindow()->decBusyCount();
	}

	// <FS:ND> mImage can be 0.
	// mImage->setBoostLevel(mImageOldBoostLevel);

	if( mImage )
		mImage->setBoostLevel(mImageOldBoostLevel);

	// <FS:ND>

	mImage = NULL;
}

// virtual
BOOL LLPreviewTexture::postBuild()
{
	if (mCopyToInv) 
	{
		getChild<LLButton>("Keep")->setLabel(getString("Copy"));
		childSetAction("Keep",LLPreview::onBtnCopyToInv,this);
		getChildView("Discard")->setVisible( false);
	}
	else if (mShowKeepDiscard)
	{
		childSetAction("Keep",onKeepBtn,this);
		childSetAction("Discard",onDiscardBtn,this);
	}
	else
	{
		getChildView("Keep")->setVisible( false);
		getChildView("Discard")->setVisible( false);
	}
	
	childSetCommitCallback("save_tex_btn", onSaveAsBtn, this);
	getChildView("save_tex_btn")->setVisible(canSaveAs()); // Ansariel: No need to show the save button if we can't save anyway
	getChildView("save_tex_btn")->setEnabled(canSaveAs());
	
	if (!mCopyToInv) 
	{
		const LLInventoryItem* item = getItem();
		
		if (item)
		{
			childSetCommitCallback("desc", LLPreview::onText, this);
			getChild<LLUICtrl>("desc")->setValue(item->getDescription());
			getChild<LLLineEditor>("desc")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
		}
	}

	// Fill in ratios list with common aspect ratio values
	mRatiosList.clear();
	mRatiosList.push_back(LLTrans::getString("Unconstrained"));
	mRatiosList.push_back("1:1");
	mRatiosList.push_back("4:3");
	mRatiosList.push_back("10:7");
	mRatiosList.push_back("3:2");
	mRatiosList.push_back("16:10");
	mRatiosList.push_back("16:9");
	mRatiosList.push_back("2:1");
	
	// Now fill combo box with provided list
	LLComboBox* combo = getChild<LLComboBox>("combo_aspect_ratio");
	combo->removeall();

	for (std::vector<std::string>::const_iterator it = mRatiosList.begin(); it != mRatiosList.end(); ++it)
	{
		combo->add(*it);
	}

	childSetCommitCallback("combo_aspect_ratio", onAspectRatioCommit, this);
	combo->setCurrentByIndex(0);

	// <FS:Techwolf Lupindo> texture comment metadata reader
	getChild<LLButton>("openprofile")->setClickedCallback(boost::bind(&LLPreviewTexture::onButtonClickProfile, this));
	getChild<LLButton>("copyuuid")->setClickedCallback(boost::bind(&LLPreviewTexture::onButtonClickUUID, this));
	mUploaderDateTime = getString("UploaderDateTime");
	// </FS:Techwolf Lupindo>

	// <FS:Ansariel> AnsaStorm skin: Need to disable line editors from
	//               code or the floater would be dragged around if
	//               trying to mark text
	if (findChild<LLLineEditor>("uploader"))
	{
		getChild<LLLineEditor>("uploader")->setEnabled(FALSE);
		getChild<LLLineEditor>("upload_time")->setEnabled(FALSE);
		getChild<LLLineEditor>("uuid")->setEnabled(FALSE);
	}
	// </FS:Ansariel>

	// <FS:Ansariel> Performance improvement
	mDimensionsCtrl = getChild<LLUICtrl>("dimensions");

	return LLPreview::postBuild();
}

// static
void LLPreviewTexture::onSaveAsBtn(LLUICtrl* ctrl, void* data)
{
	LLPreviewTexture* self = (LLPreviewTexture*)data;
	std::string value = ctrl->getValue().asString();
	if (value == "format_png")
	{
		self->saveAs(LLPreviewTexture::FORMAT_PNG);
	}
	else if (value == "format_tga")
	{
		self->saveAs(LLPreviewTexture::FORMAT_TGA);
	}
	else
	{
		// <FS:PP> Allow to use user-defined default save format for textures
		// self->saveAs(LLPreviewTexture::FORMAT_TGA);
		if (!gSavedSettings.getBOOL("FSTextureDefaultSaveAsFormat"))
		{
			self->saveAs(LLPreviewTexture::FORMAT_TGA);
		}
		else
		{
			self->saveAs(LLPreviewTexture::FORMAT_PNG);
		}
		// </FS:PP>
	}
}

void LLPreviewTexture::draw()
{
	updateDimensions();
	
	LLPreview::draw();

	if (!isMinimized())
	{
		LLGLSUIDefault gls_ui;
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		
		const LLRect& border = mClientRect;
		LLRect interior = mClientRect;
		interior.stretch( -PREVIEW_BORDER_WIDTH );

		// ...border
		gl_rect_2d( border, LLColor4(0.f, 0.f, 0.f, 1.f));
		gl_rect_2d_checkerboard( interior );

		if ( mImage.notNull() )
		{
			// Draw the texture
			gGL.diffuseColor3f( 1.f, 1.f, 1.f );
			gl_draw_scaled_image(interior.mLeft,
								interior.mBottom,
								interior.getWidth(),
								interior.getHeight(),
								mImage);

			// Pump the texture priority
			F32 pixel_area = mLoadingFullImage ? (F32)MAX_IMAGE_AREA  : (F32)(interior.getWidth() * interior.getHeight() );
			mImage->addTextureStats( pixel_area );

			// Don't bother decoding more than we can display, unless
			// we're loading the full image.
			if (!mLoadingFullImage)
			{
				S32 int_width = interior.getWidth();
				S32 int_height = interior.getHeight();
				mImage->setKnownDrawSize(int_width, int_height);
			}
			else
			{
				// Don't use this feature
				mImage->setKnownDrawSize(0, 0);
			}

			if( mLoadingFullImage )
			{
				LLFontGL::getFontSansSerif()->renderUTF8(LLTrans::getString("Receiving"), 0,
					interior.mLeft + 4, 
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::NORMAL,
					LLFontGL::DROP_SHADOW);
				
				F32 data_progress = mImage->getDownloadProgress() ;
				
				// Draw the progress bar.
				const S32 BAR_HEIGHT = 12;
				const S32 BAR_LEFT_PAD = 80;
				S32 left = interior.mLeft + 4 + BAR_LEFT_PAD;
				S32 bar_width = getRect().getWidth() - left - RESIZE_HANDLE_WIDTH - 2;
				S32 top = interior.mBottom + 4 + BAR_HEIGHT;
				S32 right = left + bar_width;
				S32 bottom = top - BAR_HEIGHT;

				LLColor4 background_color(0.f, 0.f, 0.f, 0.75f);
				LLColor4 decoded_color(0.f, 1.f, 0.f, 1.0f);
				LLColor4 downloaded_color(0.f, 0.5f, 0.f, 1.0f);

				gl_rect_2d(left, top, right, bottom, background_color);

				if (data_progress > 0.0f)
				{
					// Downloaded bytes
					right = left + llfloor(data_progress * (F32)bar_width);
					if (right > left)
					{
						gl_rect_2d(left, top, right, bottom, downloaded_color);
					}
				}
			}
			else
			if( !mSavedFileTimer.hasExpired() )
			{
				LLFontGL::getFontSansSerif()->renderUTF8(LLTrans::getString("FileSaved"), 0,
					interior.mLeft + 4,
					interior.mBottom + 4,
					LLColor4::white, LLFontGL::LEFT, LLFontGL::BOTTOM,
					LLFontGL::NORMAL,
					LLFontGL::DROP_SHADOW);
			}
		}
	} 

}


// virtual
BOOL LLPreviewTexture::canSaveAs() const
{
	return mIsFullPerm && !mLoadingFullImage && mImage.notNull() && !mImage->isMissingAsset();
}


// virtual
void LLPreviewTexture::saveAs()
{
	// <FS:PP> Allow to use user-defined default save format for textures
	// saveAs(LLPreviewTexture::FORMAT_TGA);
	if (!gSavedSettings.getBOOL("FSTextureDefaultSaveAsFormat"))
	{
		saveAs(LLPreviewTexture::FORMAT_TGA);
	}
	else
	{
		saveAs(LLPreviewTexture::FORMAT_PNG);
	}
	// </FS:PP>
	
}

void LLPreviewTexture::saveAs(EFileformatType format)
{
	if( mLoadingFullImage )
		return;

	LLFilePicker& file_picker = LLFilePicker::instance();
	const LLInventoryItem* item = getItem() ;

	loaded_callback_func callback;
	LLFilePicker::ESaveFilter saveFilter;

	switch (format)
	{
		case LLPreviewTexture::FORMAT_PNG:
			callback = LLPreviewTexture::onFileLoadedForSavePNG;
			saveFilter = LLFilePicker::FFSAVE_PNG;
			break;
		case LLPreviewTexture::FORMAT_TGA:
		default:
			callback = LLPreviewTexture::onFileLoadedForSaveTGA;
			saveFilter = LLFilePicker::FFSAVE_TGA;
			break;
	}

	// <FS:Ansariel> FIRE-14111: File extension missing on Linux when saving a texture
	//if( !file_picker.getSaveFile( saveFilter, item ? LLDir::getScrubbedFileName(item->getName()) : LLStringUtil::null) )
	if( !file_picker.getSaveFile( saveFilter, item ? checkFileExtension(LLDir::getScrubbedFileName(item->getName()), format) : LLStringUtil::null) )
	// </FS:Ansariel>
	{
		// User canceled or we failed to acquire save file.
		return;
	}
	if(mPreviewToSave)
	{
		mPreviewToSave = FALSE;
		LLFloaterReg::showTypedInstance<LLPreviewTexture>("preview_texture", item->getUUID());
	}

	// remember the user-approved/edited file name.
	// <FS:Ansariel> FIRE-14111: File extension missing on Linux when saving a texture
	//mSaveFileName = file_picker.getFirstFile();
	mSaveFileName = checkFileExtension(file_picker.getFirstFile(), format);
	// </FS:Ansariel>
	mLoadingFullImage = TRUE;
	getWindow()->incBusyCount();

	mImage->forceToSaveRawImage(0) ;//re-fetch the raw image if the old one is removed.
	mImage->setLoadedCallback( callback, 
								0, TRUE, FALSE, new LLUUID( mItemUUID ), &mCallbackTextureList );
}

// virtual
void LLPreviewTexture::reshape(S32 width, S32 height, BOOL called_from_parent)
{
	LLPreview::reshape(width, height, called_from_parent);

	LLRect dim_rect;
	// Ansariel: Need the rect of the dimensions_panel
	//LLView *pView = findChildView( "dimensions" );
	LLView *pView = findChildView( "dimensions_panel" );
	
	if( pView )
		dim_rect = pView->getRect();

	S32 horiz_pad = 2 * (LLPANEL_BORDER_WIDTH + PREVIEW_PAD) + PREVIEW_RESIZE_HANDLE_SIZE;

	// add space for dimensions and aspect ratio
	S32 info_height = dim_rect.mTop + CLIENT_RECT_VPAD;

	LLRect client_rect(horiz_pad, getRect().getHeight(), getRect().getWidth() - horiz_pad, 0);
	client_rect.mTop -= (PREVIEW_HEADER_SIZE + CLIENT_RECT_VPAD);

	// <FS:Techwolf Lupindo> texture comment metadata reader
	// 1 additional line: uploader and date time
	if (findChild<LLTextEditor>("uploader_date_time"))
	{
		if (mImage && (mImage->mComment.find("a") != mImage->mComment.end() || mImage->mComment.find("z") != mImage->mComment.end()))
		{
			client_rect.mTop -= (getChild<LLTextEditor>("uploader_date_time")->getTextBoundingRect().getHeight() + CLIENT_RECT_VPAD);
		}
	}
	else if (findChild<LLLineEditor>("uploader"))
	{
		// AnsaStorm skin
		client_rect.mTop -= 3 * (PREVIEW_LINE_HEIGHT + CLIENT_RECT_VPAD);
	}
	// </FS:Techwolf Lupindo>


	client_rect.mBottom += PREVIEW_BORDER + CLIENT_RECT_VPAD + info_height ;

	S32 client_width = client_rect.getWidth();
	S32 client_height = client_rect.getHeight();

	if (mAspectRatio > 0.f)
	{
		if(mAspectRatio > 1.f)
		{
			client_height = llceil((F32)client_width / mAspectRatio);
			if(client_height > client_rect.getHeight())
			{
				client_height = client_rect.getHeight();
				client_width = llceil((F32)client_height * mAspectRatio);
			}
		}
		else//mAspectRatio < 1.f
		{
			client_width = llceil((F32)client_height * mAspectRatio);
			if(client_width > client_rect.getWidth())
			{
				client_width = client_rect.getWidth();
				client_height = llceil((F32)client_width / mAspectRatio);
			}
		}
	}

	mClientRect.setLeftTopAndSize(client_rect.getCenterX() - (client_width / 2), client_rect.getCenterY() +  (client_height / 2), client_width, client_height);

}

// virtual
void LLPreviewTexture::onFocusReceived()
{
	LLPreview::onFocusReceived();
}

void LLPreviewTexture::openToSave()
{
	mPreviewToSave = TRUE;
}

// static
void LLPreviewTexture::onFileLoadedForSaveTGA(BOOL success, 
					   LLViewerFetchedTexture *src_vi,
					   LLImageRaw* src, 
					   LLImageRaw* aux_src, 
					   S32 discard_level,
					   BOOL final,
					   void* userdata)
{
	LLUUID* item_uuid = (LLUUID*) userdata;

	LLPreviewTexture* self = LLFloaterReg::findTypedInstance<LLPreviewTexture>("preview_texture", *item_uuid);

	if( final || !success )
	{
		delete item_uuid;

		if( self )
		{
			self->getWindow()->decBusyCount();
			self->mLoadingFullImage = FALSE;
		}
	}

	if( self && final && success )
	{
		// <FS:Ansariel> Undo MAINT-2897 and use our own texture format selection
		//const U32 ext_length = 3;
		//std::string extension = self->mSaveFileName.substr( self->mSaveFileName.length() - ext_length);

		//// We only support saving in PNG or TGA format
		//LLPointer<LLImageFormatted> image;
		//if(extension == "png")
		//{
		//	image = new LLImagePNG;
		//}
		//else if(extension == "tga")
		//{
		//	image = new LLImageTGA;
		//}

		//if( image && !image->encode( src, 0 ) )
		LLPointer<LLImageTGA> image_tga = new LLImageTGA;
		if( !image_tga->encode( src ) )
		// </FS:Ansariel>
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotificationsUtil::add("CannotEncodeFile", args);
		}
		// <FS:Ansariel> Undo MAINT-2897 and use our own texture format selection
		//else if( image && !image->save( self->mSaveFileName ) )
		else if( !image_tga->save( self->mSaveFileName ) )
		// </FS:Ansariel>
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotificationsUtil::add("CannotWriteFile", args);
		}
		else
		{
			self->mSavedFileTimer.reset();
			self->mSavedFileTimer.setTimerExpirySec( SECONDS_TO_SHOW_FILE_SAVED_MSG );
		}

		self->mSaveFileName.clear();
	}

	if( self && !success )
	{
		LLNotificationsUtil::add("CannotDownloadFile");
	}

}

// static
void LLPreviewTexture::onFileLoadedForSavePNG(BOOL success, 
											LLViewerFetchedTexture *src_vi,
											LLImageRaw* src, 
											LLImageRaw* aux_src, 
											S32 discard_level,
											BOOL final,
											void* userdata)
{
	LLUUID* item_uuid = (LLUUID*) userdata;

	LLPreviewTexture* self = LLFloaterReg::findTypedInstance<LLPreviewTexture>("preview_texture", *item_uuid);

	if( final || !success )
	{
		delete item_uuid;

		if( self )
		{
			self->getWindow()->decBusyCount();
			self->mLoadingFullImage = FALSE;
		}
	}

	if( self && final && success )
	{
		LLPointer<LLImagePNG> image_png = new LLImagePNG;
		if( !image_png->encode( src, 0.0 ) )
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotificationsUtil::add("CannotEncodeFile", args);
		}
		else if( !image_png->save( self->mSaveFileName ) )
		{
			LLSD args;
			args["FILE"] = self->mSaveFileName;
			LLNotificationsUtil::add("CannotWriteFile", args);
		}
		else
		{
			self->mSavedFileTimer.reset();
			self->mSavedFileTimer.setTimerExpirySec( SECONDS_TO_SHOW_FILE_SAVED_MSG );
		}

		self->mSaveFileName.clear();
	}

	if( self && !success )
	{
		LLNotificationsUtil::add("CannotDownloadFile");
	}
}

// It takes a while until we get height and width information.
// When we receive it, reshape the window accordingly.
void LLPreviewTexture::updateDimensions()
{
	if (!mImage)
	{
		return;
	}
	if ((mImage->getFullWidth() * mImage->getFullHeight()) == 0)
	{
		return;
	}

	if (mAssetStatus != PREVIEW_ASSET_LOADED)
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
		// Asset has been fully loaded, adjust aspect ratio
		adjustAspectRatio();
	}
	
	// Update the width/height display every time
	// <FS:Ansariel> Performance improvement
	//getChild<LLUICtrl>("dimensions")->setTextArg("[WIDTH]",  llformat("%d", mImage->getFullWidth()));
	//getChild<LLUICtrl>("dimensions")->setTextArg("[HEIGHT]", llformat("%d", mImage->getFullHeight()));
	if (mCurrentImageWidth != mImage->getFullWidth())
	{
		mDimensionsCtrl->setTextArg("[WIDTH]", llformat("%d", mImage->getFullWidth()));
		mCurrentImageWidth = mImage->getFullWidth();
	}
	if (mCurrentImageHeight != mImage->getFullHeight())
	{
		mDimensionsCtrl->setTextArg("[HEIGHT]", llformat("%d", mImage->getFullHeight()));
		mCurrentImageHeight = mImage->getFullHeight();
	}
	// </FS:Ansariel>

	// Reshape the floater only when required
	if (mUpdateDimensions)
	{
		mUpdateDimensions = FALSE;
		
		// <FS:Ansariel>: Show image at full resolution if possible
		//reshape floater
		//reshape(getRect().getWidth(), getRect().getHeight());

		//gFloaterView->adjustToFitScreen(this, FALSE);

		// Move dimensions panel into correct position depending
		// if any of the buttons is shown
		LLView* button_panel = getChildView("button_panel");
		LLView* dimensions_panel = getChildView("dimensions_panel");
		dimensions_panel->setVisible(TRUE);
		if (!getChildView("Keep")->getVisible() &&
			!getChildView("Discard")->getVisible() &&
			!getChildView("save_tex_btn")->getVisible())
		{
			button_panel->setVisible(FALSE);
			if (mShowingButtons)
			{
				dimensions_panel->translate(0, -button_panel->getRect().mTop);
				mShowingButtons = false;
			}
		}
		else
		{
			button_panel->setVisible(TRUE);
			if (!mShowingButtons)
			{
				dimensions_panel->translate(0, button_panel->getRect().mTop);
				mShowingButtons = true;
			}
		}

		// <FS:Techwolf Lupindo> texture comment metadata reader
		S32 additional_height = 0;
		if (findChild<LLTextEditor>("uploader_date_time"))
		{
			bool adjust_height = false;
			if (mImage->mComment.find("a") != mImage->mComment.end())
			{
				getChildView("uploader_date_time")->setVisible(TRUE);
				LLUUID id = LLUUID(mImage->mComment["a"]);
				std::string name;
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(id, &avatar_name))
				{
					mUploaderDateTime.setArg("[UPLOADER]", avatar_name.getCompleteName());
				}
				else
				{
					if (!mDisplayNameCallback) // prevents a possible callbackLoadName loop due to server error.
					{
						mDisplayNameCallback = true;
						mUploaderDateTime.setArg("[UPLOADER]", LLTrans::getString("AvatarNameWaiting"));
						if (mAvatarNameCallbackConnection.connected())
						{
							mAvatarNameCallbackConnection.disconnect();
						}
						mAvatarNameCallbackConnection = LLAvatarNameCache::get(id, boost::bind(&LLPreviewTexture::callbackLoadName, this, _1, _2));
					}
				}
				getChild<LLTextEditor>("uploader_date_time")->setText(mUploaderDateTime.getString());
				adjust_height = true;
			}

			if (mImage->mComment.find("z") != mImage->mComment.end())
			{
				if (!adjust_height)
				{
					getChildView("uploader_date_time")->setVisible(TRUE);
					adjust_height = true;
				}
				std::string date_time = mImage->mComment["z"];
				LLSD substitution;
				substitution["datetime"] = FSCommon::secondsSinceEpochFromString("%Y%m%d%H%M%S", date_time);
				date_time = getString("DateTime"); // reuse date_time variable
				LLStringUtil::format(date_time, substitution);
				mUploaderDateTime.setArg("[DATE_TIME]", date_time);
				getChild<LLTextEditor>("uploader_date_time")->setText(mUploaderDateTime.getString());
			}
			// add extra space for uploader and date_time
			if (adjust_height)
			{
				getChildView("openprofile")->setVisible(TRUE);
				additional_height += (getChild<LLTextEditor>("uploader_date_time")->getTextBoundingRect().getHeight());
			}
		}
		else if (findChild<LLLineEditor>("uploader"))
		{
			// AnsaStorm skin
			if (mImage->mComment.find("a") != mImage->mComment.end())
			{
				getChild<LLButton>("openprofile")->setEnabled(TRUE);
				LLUUID id = LLUUID(mImage->mComment["a"]);
				std::string name;
				LLAvatarName avatar_name;
				if (LLAvatarNameCache::get(id, &avatar_name))
				{
					childSetValue("uploader", LLSD( avatar_name.getCompleteName()) );
				}
				else
				{
					if (!mDisplayNameCallback) // prevents a possible callbackLoadName loop due to server error.
					{
						mDisplayNameCallback = true;
						getChild<LLLineEditor>("uploader")->setText(LLTrans::getString("AvatarNameWaiting"));
						if (mAvatarNameCallbackConnection.connected())
						{
							mAvatarNameCallbackConnection.disconnect();
						}
						mAvatarNameCallbackConnection = LLAvatarNameCache::get(id, boost::bind(&LLPreviewTexture::callbackLoadName, this, _1, _2));
					}
				}
			}

			if (mImage->mComment.find("z") != mImage->mComment.end())
			{
				std::string date_time = mImage->mComment["z"];
				LLSD substitution;
				substitution["datetime"] = FSCommon::secondsSinceEpochFromString("%Y%m%d%H%M%S", date_time);
				date_time = getString("DateTime"); // reuse date_time variable
				LLStringUtil::format(date_time, substitution);
				childSetValue("upload_time", LLSD( date_time ) );
			}

 			if (mIsCopyable)
 			{
				childSetValue("uuid", LLSD( mImageID.asString() ));
 			}

			LLView* uploader_view = getChildView("uploader");
			LLView* uploadtime_view = getChildView("upload_time");
			LLView* uuid_view = getChildView("uuid");
				
			uploader_view->setVisible(TRUE);
			uploadtime_view->setVisible(TRUE);
			uuid_view->setVisible(TRUE);
			getChildView("openprofile")->setVisible(TRUE);
			getChildView("copyuuid")->setVisible(TRUE);
			getChildView("uploader_label")->setVisible(TRUE);
			getChildView("upload_time_label")->setVisible(TRUE);
			getChildView("uuid_label")->setVisible(TRUE);

			additional_height = uploader_view->getRect().getHeight() + uploadtime_view->getRect().getHeight() + uuid_view->getRect().getHeight() + 3 * PREVIEW_VPAD;
		}
		// </FS:Techwolf Lupindo>

		// If this is 100% correct???
		S32 floater_target_width = mImage->getFullWidth() + 2 * (LLPANEL_BORDER_WIDTH + PREVIEW_PAD) + PREVIEW_RESIZE_HANDLE_SIZE;;
		S32 floater_target_height = mImage->getFullHeight() + 3 * CLIENT_RECT_VPAD + PREVIEW_BORDER + dimensions_panel->getRect().mTop + getChildView("desc")->getRect().getHeight() + additional_height;

		// Scale down by factor 0.5 if image would exceed viewer window
		if (gViewerWindow->getWindowWidthRaw() < floater_target_width || gViewerWindow->getWindowHeightRaw() < floater_target_height)
		{
			floater_target_width = mImage->getFullWidth() / 2 + 2 * (LLPANEL_BORDER_WIDTH + PREVIEW_PAD) + PREVIEW_RESIZE_HANDLE_SIZE;;
			floater_target_height = mImage->getFullHeight() / 2 + 3 * CLIENT_RECT_VPAD + PREVIEW_BORDER + dimensions_panel->getRect().mTop + getChildView("desc")->getRect().getHeight() + additional_height;
		}
		
		// Preserve minimum floater size
		floater_target_width = llmax(floater_target_width, getMinWidth());
		floater_target_height = llmax(floater_target_height, getMinHeight());

		// Resize floater
		LLMultiFloater* host = getHost();
		if (host)
		{
			S32 old_height = host->getRect().getHeight();
			host->reshape(getMinWidth(), getMinHeight());
			host->translate(0, old_height - getMinHeight());
			host->growToFit(floater_target_width, floater_target_height);
		}
		reshape(floater_target_width, floater_target_height);
		gFloaterView->adjustToFitScreen(this, FALSE);
		// </FS:Ansariel>: Show image at full resolution if possible

		LLRect dim_rect(getChildView("dimensions")->getRect());
		LLRect aspect_label_rect(getChildView("aspect_ratio")->getRect());
		getChildView("aspect_ratio")->setVisible( dim_rect.mRight < aspect_label_rect.mLeft);

		// <FS:Ansariel> Asset UUID
		if (mIsCopyable)
		{
			LLView* copy_uuid_btn = getChildView("copyuuid");
			copy_uuid_btn->setVisible(TRUE);
			copy_uuid_btn->setEnabled(TRUE);
		}
		// </FS:Ansariel>
	}
}


// <FS:Techwolf Lupindo> texture comment metadata reader
void LLPreviewTexture::callbackLoadName(const LLUUID& agent_id, const LLAvatarName& av_name)
{
	if (mAvatarNameCallbackConnection.connected())
	{
		mAvatarNameCallbackConnection.disconnect();
	}

	if (findChild<LLTextEditor>("uploader_date_time"))
	{
		mUploaderDateTime.setArg("[UPLOADER]", av_name.getCompleteName());
		getChild<LLTextEditor>("uploader_date_time")->setText(mUploaderDateTime.getString());
		mUpdateDimensions = TRUE;
	}
	else if (findChild<LLLineEditor>("uploader"))
	{
		// AnsaStorm skin
		childSetValue("uploader", LLSD( av_name.getCompleteName() ) );
	}
}

void LLPreviewTexture::onButtonClickProfile()
{
	if (mImage && (mImage->mComment.find("a") != mImage->mComment.end()))
	{
		LLUUID id = LLUUID(mImage->mComment["a"]);
		LLAvatarActions::showProfile(id);
	}
}

void LLPreviewTexture::onButtonClickUUID()
{
	std::string uuid = mImageID.asString();
	LLClipboard::instance().copyToClipboard(utf8str_to_wstring(uuid), 0, uuid.size());
}

/* static */
void LLPreviewTexture::onTextureLoaded(BOOL success, LLViewerFetchedTexture* src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	LLPreviewTexture* self = (LLPreviewTexture*)userdata;
	self->mUpdateDimensions = TRUE;
}
// </FS:Techwolf Lupindo> texture comment decoder

// Return true if everything went fine, false if we somewhat modified the ratio as we bumped on border values
bool LLPreviewTexture::setAspectRatio(const F32 width, const F32 height)
{
	mUpdateDimensions = TRUE;

	// We don't allow negative width or height. Also, if height is positive but too small, we reset to default
	// A default 0.f value for mAspectRatio means "unconstrained" in the rest of the code
	if ((width <= 0.f) || (height <= F_APPROXIMATELY_ZERO))
	{
		mAspectRatio = 0.f;
		return false;
	}
	
	// Compute and store the ratio
	F32 ratio = width / height;
	mAspectRatio = llclamp(ratio, PREVIEW_TEXTURE_MIN_ASPECT, PREVIEW_TEXTURE_MAX_ASPECT);
	
	// Return false if we clamped the value, true otherwise
	return (ratio == mAspectRatio);
}


void LLPreviewTexture::onAspectRatioCommit(LLUICtrl* ctrl, void* userdata)
{	
	LLPreviewTexture* self = (LLPreviewTexture*) userdata;
	
	std::string ratio(ctrl->getValue().asString());
	std::string::size_type separator(ratio.find_first_of(":/\\"));
	
	if (std::string::npos == separator) {
		// If there's no separator assume we want an unconstrained ratio
		self->setAspectRatio( 0.f, 0.f );
		return;
	}
	
	F32 width, height;
	std::istringstream numerator(ratio.substr(0, separator));
	std::istringstream denominator(ratio.substr(separator + 1));
	numerator >> width;
	denominator >> height;
	
	self->setAspectRatio( width, height );	
}

void LLPreviewTexture::loadAsset()
{
	mImage = LLViewerTextureManager::getFetchedTexture(mImageID, FTT_DEFAULT, MIPMAP_TRUE, LLGLTexture::BOOST_NONE, LLViewerTexture::LOD_TEXTURE);
	mImageOldBoostLevel = mImage->getBoostLevel();
	mImage->setBoostLevel(LLGLTexture::BOOST_PREVIEW);
	mImage->forceToSaveRawImage(0) ;
	// <FS:Techwolf Lupindo> texture comment decoder
	mImage->setLoadedCallback(LLPreviewTexture::onTextureLoaded, 0, TRUE, FALSE, this, &mCallbackTextureList);
	// </FS:Techwolf Lupindo>
	mAssetStatus = PREVIEW_ASSET_LOADING;
	mUpdateDimensions = TRUE;
	updateDimensions();
	getChildView("save_tex_btn")->setEnabled(canSaveAs());
	getChildView("save_tex_btn")->setVisible(canSaveAs());
	if (mObjectUUID.notNull())
	{
		// check that we can copy inworld items into inventory
		getChildView("Keep")->setEnabled(mIsCopyable);
	}
}

LLPreview::EAssetStatus LLPreviewTexture::getAssetStatus()
{
	if (mImage.notNull() && (mImage->getFullWidth() * mImage->getFullHeight() > 0))
	{
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
	return mAssetStatus;
}

void LLPreviewTexture::adjustAspectRatio()
{
	S32 w = mImage->getFullWidth();
    S32 h = mImage->getFullHeight();

	// Determine aspect ratio of the image
	S32 tmp;
    while (h != 0)
    {
        tmp = w % h;
        w = h;
        h = tmp;
    }
	S32 divisor = w;
	S32 num = mImage->getFullWidth() / divisor;
	S32 denom = mImage->getFullHeight() / divisor;

	if (setAspectRatio(num, denom))
	{
		// Select corresponding ratio entry in the combo list
		LLComboBox* combo = getChild<LLComboBox>("combo_aspect_ratio");
		if (combo)
		{
			std::ostringstream ratio;
			ratio << num << ":" << denom;
			std::vector<std::string>::const_iterator found = std::find(mRatiosList.begin(), mRatiosList.end(), ratio.str());
			if (found == mRatiosList.end())
			{
				combo->setCurrentByIndex(0);
			}
			else
			{
				combo->setCurrentByIndex(found - mRatiosList.begin());
			}
		}
	}

	mUpdateDimensions = TRUE;
}

void LLPreviewTexture::updateImageID()
{
	const LLViewerInventoryItem *item = static_cast<const LLViewerInventoryItem*>(getItem());
	if(item)
	{
		mImageID = item->getAssetUUID();

		// here's the old logic...
		//mShowKeepDiscard = item->getPermissions().getCreator() != gAgent.getID();
		// here's the new logic... 'cos we hate disappearing buttons.
		mShowKeepDiscard = TRUE;

		mCopyToInv = FALSE;
		LLPermissions perm(item->getPermissions());
		mIsCopyable = perm.allowCopyBy(gAgent.getID(), gAgent.getGroupID()) && perm.allowTransferTo(gAgent.getID());
		mIsFullPerm = item->checkPermissionsSet(PERM_ITEM_UNRESTRICTED);
	}
	else // not an item, assume it's an asset id
	{
		mImageID = mItemUUID;
		mShowKeepDiscard = FALSE;
		mCopyToInv = TRUE;
		mIsCopyable = TRUE;
		mIsFullPerm = TRUE;
	}

}

/* virtual */
void LLPreviewTexture::setObjectID(const LLUUID& object_id)
{
	mObjectUUID = object_id;

	const LLUUID old_image_id = mImageID;

	// Update what image we're pointing to, such as if we just specified the mObjectID
	// that this mItemID is part of.
	updateImageID();

	// If the imageID has changed, start over and reload the new image.
	if (mImageID != old_image_id)
	{
		mAssetStatus = PREVIEW_ASSET_UNLOADED;
		loadAsset();
	}
	refreshFromItem();
}

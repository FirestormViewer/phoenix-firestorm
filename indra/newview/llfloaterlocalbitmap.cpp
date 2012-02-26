/** 
 * @file llfloaterlocalbitmap.h
 * @author Vaalith Jinn
 * @brief Local Bitmap Browser source
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

/* basic headers */
#include "llviewerprecompiledheaders.h"
#include "lluictrlfactory.h"

/* boost */
#include <boost/filesystem.hpp>
/* in case of unexpected crashes - check if someone defined 'equivalent' and undef it before the above header. */

/* own class header */
#include "llfloaterlocalbitmap.h"

/* image compression headers. */
#include "llimagebmp.h"
#include "llimagetga.h"
#include "llimagejpeg.h"
#include "llimagepng.h"

/* misc headers */
#include <time.h>
#include <ctime>
#include "llfloaterreg.h"
#include "llviewertexturelist.h"
#include "llviewerobjectlist.h"
#include "llfilepicker.h"
#include "llviewermenufile.h"
#include "llfloaterimagepreview.h"

/* xui elements */
#include "lltexturectrl.h"   
#include "llscrolllistctrl.h"
#include "llviewercontrol.h"
#include "lllineeditor.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"

/* including to force rebakes when needed */
#include "llagent.h"
#include "llvoavatarself.h"

/* sculpt refresh */
#include "llvovolume.h"
#include "llface.h"
#include "llvolumemgr.h"

/*=======================================*/
/*  Instantiating manager class          */
/*  and formally declaring it's list     */
/*=======================================*/ 
LLLocalBitmapBrowser* gLocalBrowser;
LLLocalBitmapBrowserTimer* gLocalBrowserTimer;
std::vector<LLLocalBitmap*> LLLocalBitmapBrowser::sLoadedBitmaps;
bool    LLLocalBitmapBrowser::sLayerUpdated;
bool    LLLocalBitmapBrowser::sSculptUpdated;

/*=======================================*/
/*  LLLocalBitmap: unit class            */
/*=======================================*/ 
LLLocalBitmap::LLLocalBitmap(std::string fullpath)
{
	if ( gDirUtilp->fileExists(fullpath) )
	{
		/* taking care of basic properties */
		this->mId.generate();
		this->mFilename	    = fullpath;
		this->mLinkStatus   = LS_OFF;
		this->mKeepUpdating = false;
		this->mShortName    = gDirUtilp->getBaseFileName(this->mFilename, true);
		this->mBitmapType   = BT_TEXTURE;
		this->mSculptDirty  = false;
		this->mVolumeDirty  = false;
		this->mValid        = false;

		/* taking care of extension type now to avoid switch madness */
		std::string temp_exten = gDirUtilp->getExtension(this->mFilename);

		if (temp_exten == "bmp") { this->mExtension = ET_IMG_BMP; }
		else if (temp_exten == "tga") { this->mExtension = ET_IMG_TGA; }
		else if (temp_exten == "jpg" || temp_exten == "jpeg") { this->mExtension = ET_IMG_JPG; }
		else if (temp_exten == "png") { this->mExtension = ET_IMG_PNG; }
		else { return; } // no valid extension.
		
		/* getting file's last modified */

		// <FS:ND> Using boost breaks on windows if mFilename is a UTF-8 filename, use either wpath (not included in the prebuilt boost libs) or LLFile

		// const std::time_t time = boost::filesystem::last_write_time( boost::filesystem::path( this->mFilename ) );
		// this->mLastModified = asctime( localtime(&time) );
	
		llstat oStatus;
		LLFile::stat( mFilename, &oStatus );
		this->mLastModified = asctime( localtime(&oStatus.st_mtime) );

		// </FS:ND>

		/* checking if the bitmap is valid && decoding if it is */
		LLImageRaw* raw_image = new LLImageRaw();
		if ( this->decodeSelf(raw_image) )
		{
			/* creating a shell LLViewerImage and fusing raw image into it */
			LLViewerFetchedTexture* viewer_texture = new LLViewerFetchedTexture( "file://"+this->mFilename, this->mId, LOCAL_USE_MIPMAPS );
			viewer_texture->createGLTexture( LOCAL_DISCARD_LEVEL, raw_image );
			viewer_texture->setCachedRawImage( LOCAL_DISCARD_LEVEL, raw_image );

			/* making damn sure gImageList will not delete it prematurely */
			viewer_texture->ref(); 

			/* finalizing by adding LLViewerImage instance into gImageList */
			gTextureList.addImage(viewer_texture);

			/* filename is valid, bitmap is decoded and valid, we're done. */
			this->mValid = true;
		}
	}
}

LLLocalBitmap::~LLLocalBitmap()
{
}

/* [maintenence functions] */
void LLLocalBitmap::updateSelf()
{
	if ( this->mLinkStatus == LS_ON || this->mLinkStatus == LS_UPDATING )
	{
		/* making sure file still exists */
		if ( !gDirUtilp->fileExists(this->mFilename) )
			{ 
				this->mLinkStatus = LS_BROKEN;
				LLFloaterLocalBitmapBrowser::updateRightSide();
				return; 
			}

		/* exists, let's check if it's lastmod has changed */
		const std::time_t temp_time = boost::filesystem::last_write_time( boost::filesystem::path( this->mFilename ) );
		LLSD new_last_modified = asctime( localtime(&temp_time) );
		if ( this->mLastModified.asString() == new_last_modified.asString() ) { return; }

		/* here we update the image */
		LLImageRaw* new_imgraw = new LLImageRaw();
		
		if ( !decodeSelf(new_imgraw) ) { this->mLinkStatus = LS_UPDATING; return; }
		else { this->mLinkStatus = LS_ON; }

		LLViewerFetchedTexture* image = gTextureList.findImage(this->mId);
		
		// here was a check if isForSculptOnly, but it appears the function is broken.
		image->createGLTexture( LOCAL_DISCARD_LEVEL, new_imgraw );
		image->setCachedRawImage( LOCAL_DISCARD_LEVEL, new_imgraw );

		/* finalizing by updating lastmod to current */
		this->mLastModified = new_last_modified;

		/* setting unit property to reflect that it has been changed */
		switch (this->mBitmapType)
		{
			case BT_TEXTURE:
				  { break; }

			case BT_SCULPT:
				  {
					  /* sets a bool to run through all visible sculpts in one go, and update the ones necessary. */
					  this->mSculptDirty = true;
					  this->mVolumeDirty = true;
					  gLocalBrowser->setSculptUpdated( true );
					  break;
				  }

			case BT_LAYER:
				  {
					  /* sets a bool to rebake layers after the iteration is done with */
					  gLocalBrowser->setLayerUpdated( true );
					  break;
				  }

			default:
				  { break; }

		}
	}

}

bool LLLocalBitmap::decodeSelf(LLImageRaw* rawimg)
{
	switch (this->mExtension)
	{
		case ET_IMG_BMP:
			{
				LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
				if ( !bmp_image->load(mFilename) ) { break; }
				if ( !bmp_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT ); 
				return true;
			}

		case ET_IMG_TGA:
			{
				LLPointer<LLImageTGA> tga_image = new LLImageTGA;
				if ( !tga_image->load(mFilename) ) { break; }
				if ( !tga_image->decode(rawimg) ) { break; }

				if(	( tga_image->getComponents() != 3) &&
					( tga_image->getComponents() != 4) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		case ET_IMG_JPG:
			{
				LLPointer<LLImageJPEG> jpeg_image = new LLImageJPEG;
				if ( !jpeg_image->load(mFilename) ) { break; }
				if ( !jpeg_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		case ET_IMG_PNG:
			{
				LLPointer<LLImagePNG> png_image = new LLImagePNG;
				if ( !png_image->load(mFilename) ) { break; }
				if ( !png_image->decode(rawimg, 0.0f) ) { break; }

				rawimg->biasedScaleToPowerOfTwo( LLViewerFetchedTexture::MAX_IMAGE_SIZE_DEFAULT );
				return true;
			}

		default:
			break;
	}
	return false;
}

void LLLocalBitmap::setUpdateBool()
{
	if ( this->mLinkStatus != LS_BROKEN )
	{
		if ( !this->mKeepUpdating )
		{
			this->mLinkStatus = LS_ON;
			this->mKeepUpdating = true;
		}
		else
		{
			this->mLinkStatus = LS_OFF;
			this->mKeepUpdating = false;
		}
	}
	else
	{
		this->mKeepUpdating = false;
	}
}

void LLLocalBitmap::setType( S32 type )
{
	this->mBitmapType = type;
}

/* [information query functions] */
std::string LLLocalBitmap::getShortName()
{
	return this->mShortName;
}

std::string LLLocalBitmap::getFileName()
{
	return this->mFilename;
}

LLUUID LLLocalBitmap::getID()
{
	return this->mId;
}

LLSD LLLocalBitmap::getLastModified()
{
	return this->mLastModified;
}

std::string LLLocalBitmap::getLinkStatus()
{
	switch(this->mLinkStatus)
	{
		case LS_ON:
			return "On";

		case LS_OFF:
			return "Off";

		case LS_BROKEN:
			return "Broken";

		case LS_UPDATING:
			return "Updating";

		default:
			return "Unknown";
	}
}

bool LLLocalBitmap::getUpdateBool()
{
	return this->mKeepUpdating;
}

bool LLLocalBitmap::getIfValidBool()
{
	return this->mValid;
}

LLLocalBitmap* LLLocalBitmap::getThis()
{
	return this;
}

S32 LLLocalBitmap::getType()
{
	return this->mBitmapType;
}

std::vector<S32> LLLocalBitmap::getFaceUsesThis(LLDrawable* drawable)
{
	std::vector<S32> matching_faces;

	for ( S32 face_iter = 0; face_iter < drawable->getNumFaces(); face_iter++ )
	{
		LLFace* newface = drawable->getFace(face_iter);

		if (!newface) { continue; }
		if (!newface->getTexture()) { continue; }

		if ( this->mId == newface->getTexture()->getID() )
		{
			matching_faces.push_back(face_iter); 
		}
	}

	return matching_faces;
}

std::vector<LLAffectedObject> LLLocalBitmap::getUsingObjects(bool seek_by_type, bool seek_textures, bool seek_sculptmaps)
{
	std::vector<LLAffectedObject> affected_vector;
	
	for( LLDynamicArrayPtr< LLPointer<LLViewerObject>, 256 >::iterator  iter = gObjectList.mObjects.begin();
		 iter != gObjectList.mObjects.end(); iter++ )
	{
		LLViewerObject* obj = *iter;
		LLAffectedObject shell;
		shell.object = obj;
		shell.local_sculptmap = false;
		bool obj_relevant = false;

		if ( obj && obj->mDrawable )
		{
			/* looking for textures */
			if ( seek_textures || ( seek_by_type && this->mBitmapType == BT_TEXTURE ) )
			{
				std::vector<S32> affected_faces = this->getFaceUsesThis( obj->mDrawable );
				if ( !affected_faces.empty() )
				{
					shell.face_list = affected_faces;
					obj_relevant = true;
				}
			}

			/* looking for sculptmaps */
			if ( ( seek_sculptmaps || ( seek_by_type && this->mBitmapType == BT_SCULPT ) )
				   && obj->isSculpted() && obj->getVolume() 
				   && this->mId == obj->getVolume()->getParams().getSculptID() 
			   )	
			{ 
				shell.local_sculptmap = true;
				obj_relevant = true;
			}
		}

		if (obj_relevant)
		{ affected_vector.push_back(shell); }
	}

	return affected_vector;
}

void LLLocalBitmap::getDebugInfo()
{
	/* debug function: dumps everything human readable into llinfos */
	llinfos << "===[local bitmap debug]==="               << "\n"
			<< "path: "          << this->mFilename       << "\n"
			<< "name: "          << this->mShortName      << "\n"
			<< "extension: "     << this->mExtension      << "\n"
			<< "uuid: "          << this->mId             << "\n"
			<< "last modified: " << this->mLastModified   << "\n"
			<< "link status: "   << this->getLinkStatus() << "\n"
			<< "keep updated: "  << this->mKeepUpdating   << "\n"
			<< "type: "          << this->mBitmapType     << "\n"
			<< "is valid: "      << this->mValid          << "\n"
			<< "=========================="               << llendl;
	
}

/*=======================================*/
/*  LLLocalBitmapBrowser: main class     */
/*=======================================*/ 
LLLocalBitmapBrowser::LLLocalBitmapBrowser()
{
	this->sLayerUpdated = false;
	this->sSculptUpdated = false;
}

LLLocalBitmapBrowser::~LLLocalBitmapBrowser()
{

}

void LLLocalBitmapBrowser::addBitmap()
{
	LLFilePicker& picker = LLFilePicker::instance();
	if ( !picker.getMultipleOpenFiles(LLFilePicker::FFLOAD_IMAGE) ) 
	   { return; }

	bool change_happened = false;
	std::string filename = picker.getFirstFile();	
	while( !filename.empty() )
	{
		LLLocalBitmap* unit = new LLLocalBitmap( filename );

		if	( unit->getIfValidBool() )
		{ 
			sLoadedBitmaps.push_back( unit ); 
			change_happened = true;
		}
	
		filename = picker.getNextFile();
	}

	if ( change_happened )
	{ onChangeHappened(); }
}

void LLLocalBitmapBrowser::delBitmap( std::vector<LLScrollListItem*> delete_vector, S32 column )
{
	bool change_happened = false;
	for( std::vector<LLScrollListItem*>::iterator list_iter = delete_vector.begin();
		 list_iter != delete_vector.end(); list_iter++ )
	{
		LLScrollListItem* list_item = *list_iter; 
		if ( list_item )
		{
			LLUUID id = list_item->getColumn(column)->getValue().asUUID();
			for (local_list_iter iter = sLoadedBitmaps.begin();
				 iter != sLoadedBitmaps.end();)
			{
				LLLocalBitmap* unit = (*iter)->getThis();

				if ( unit->getID() == id )
				{	
					LLViewerFetchedTexture* image = gTextureList.findImage(id);
					gTextureList.deleteImage( image );
					image->unref();

					iter = sLoadedBitmaps.erase(iter);
					delete unit;
					unit = NULL;

					change_happened = true;
				}
				else
				{ iter++; }
			}
		}
	}

	if ( change_happened )
	{ onChangeHappened(); }
}

void LLLocalBitmapBrowser::onUpdateBool(LLUUID id)
{
	LLLocalBitmap* unit = getBitmapUnit( id );
	if ( unit ) 
	{ 
		unit->setUpdateBool(); 
		pingTimer();
	}
}

void LLLocalBitmapBrowser::onSetType(LLUUID id, S32 type)
{
	LLLocalBitmap* unit = getBitmapUnit( id );
	if ( unit ) 
	{ unit->setType(type); }
}

LLLocalBitmap* LLLocalBitmapBrowser::getBitmapUnit(LLUUID id)
{
	local_list_iter iter = sLoadedBitmaps.begin();
	for (; iter != sLoadedBitmaps.end(); iter++)
	{ 
		if ( (*iter)->getID() == id ) 
		{
			return (*iter)->getThis();
		} 
	}
	
	return NULL;
}

bool LLLocalBitmapBrowser::isDoingUpdates()
{
	local_list_iter iter = sLoadedBitmaps.begin();
	for (; iter != sLoadedBitmaps.end(); iter++)
	{ 
		if ( (*iter)->getUpdateBool() ) 
		{ return true; } /* if at least one unit in the list needs updates - we need a timer. */
	}

	return false;
}


/* Reaction to a change in bitmaplist, this function finds a texture picker floater's appropriate scrolllist
   and passes this scrolllist's pointer to UpdateTextureCtrlList for processing. */
void LLLocalBitmapBrowser::onChangeHappened()
{
	/* own floater update */
	LLFloaterLocalBitmapBrowser::updateBitmapScrollList();

	/* texturepicker related */
	const LLView::child_list_t* child_list = gFloaterView->getChildList();
	LLView::child_list_const_iter_t child_list_iter = child_list->begin();

	for (; child_list_iter != child_list->end(); child_list_iter++)
	{
		LLView* view = *child_list_iter;
		if ( view->getName() == LOCAL_TEXTURE_PICKER_NAME )
		{
			LLScrollListCtrl* ctrl = view->getChild<LLScrollListCtrl>
									( LOCAL_TEXTURE_PICKER_LIST_NAME, 
									  LOCAL_TEXTURE_PICKER_RECURSE );

			if ( ctrl ) { updateTextureCtrlList(ctrl); }
		}
	}

	/* poking timer to see if it's still needed/still not needed */
	pingTimer();

}

void LLLocalBitmapBrowser::pingTimer()
{
	if ( !sLoadedBitmaps.empty() && isDoingUpdates() )
	{
		if (!gLocalBrowserTimer) 
		{ gLocalBrowserTimer = new LLLocalBitmapBrowserTimer(); }
		
		if ( !gLocalBrowserTimer->isRunning() )
		{ gLocalBrowserTimer->start(); }
	}

	else
	{
		if (gLocalBrowserTimer)
		{
			if ( gLocalBrowserTimer->isRunning() ) 
			{ gLocalBrowserTimer->stop(); } 
		}
	}
}

/* This function refills the texture picker floater's scrolllist with the updated contents of bitmaplist */
void LLLocalBitmapBrowser::updateTextureCtrlList(LLScrollListCtrl* ctrl)
{
	if ( ctrl ) // checking again in case called externally for some silly reason.
	{
		ctrl->clearRows(); 
		if ( !sLoadedBitmaps.empty() )
		{
			local_list_iter iter = sLoadedBitmaps.begin();
			for ( ; iter != sLoadedBitmaps.end(); iter++ )
			{
				LLSD element;
				element["columns"][0]["column"] = "unit_name";
				element["columns"][0]["type"] = "text";
				element["columns"][0]["value"] = (*iter)->mShortName;

				element["columns"][1]["column"] = "unit_id_HIDDEN";
				element["columns"][1]["type"] = "text";
				element["columns"][1]["value"] = (*iter)->mId;

				ctrl->addElement(element);
			}
		}
	}
}

void LLLocalBitmapBrowser::performTimedActions(void)
{
	// perform checking if updates are needed && update if so.
	local_list_iter iter;
	for (iter = sLoadedBitmaps.begin(); iter != sLoadedBitmaps.end(); iter++)
	{ (*iter)->updateSelf(); }

	// one or more sculpts have been updated, refreshing them.
	if ( sSculptUpdated )
	{
		LLLocalBitmapBrowser::local_list_iter iter;
		for(iter = sLoadedBitmaps.begin(); iter != sLoadedBitmaps.end(); iter++)
		{
			if ( (*iter)->mSculptDirty )
			{
				performSculptUpdates( (*iter)->getThis() );
				(*iter)->mSculptDirty = false;
			}
		}
		sSculptUpdated = false;
	}

	// one of the layer bitmaps has been updated, we need to rebake.
	if ( sLayerUpdated )
	{
		if  ( !isAgentAvatarValid() ) { return; }
		gAgentAvatarp->forceBakeAllTextures( SLAM_FOR_DEBUG );
		
		sLayerUpdated = false;
	}
}

void LLLocalBitmapBrowser::performSculptUpdates(LLLocalBitmap* unit)
{

	/* looking for sculptmap using objects only */
	std::vector<LLAffectedObject> object_list = unit->getUsingObjects(false, false, true);
	if (object_list.empty()) { return; }

	for( std::vector<LLAffectedObject>::iterator iter = object_list.begin();
		 iter != object_list.end(); iter++ )
	{
		LLAffectedObject aobj = *iter;
		if ( aobj.object )
		{
			if ( !aobj.local_sculptmap ) { continue; } // should never get here. only in case of misuse.
			
			// update code [begin]   
			if ( unit->mVolumeDirty )
			{
				LLImageRaw* rawimage = gTextureList.findImage( unit->getID() )->getCachedRawImage();

				LLVolumeParams params = aobj.object->getVolume()->getParams();
				LLVolumeLODGroup* lodgroup = aobj.object->mDrawable->getVOVolume()->getVolumeManager()->getGroup(params);
			
				for (S32 i = 0; i < LLVolumeLODGroup::NUM_LODS; i++)
				{
					LLVolume* vol = lodgroup->getVolByLOD(i);

					if (vol)
						{ vol->sculpt(rawimage->getWidth(), rawimage->getHeight(), rawimage->getComponents(), rawimage->getData(), 0); }
				}

				// doing this again to fix the weirdness with selected-for-edit objects not updating otherwise.
				aobj.object->getVolume()->sculpt(rawimage->getWidth(), rawimage->getHeight(), 
												  rawimage->getComponents(), rawimage->getData(), 0);

				unit->mVolumeDirty = false;
			}

			aobj.object->mDrawable->getVOVolume()->setSculptChanged( true ); 
			aobj.object->mDrawable->getVOVolume()->markForUpdate( true );
			// update code [end]
		}
			
	}

}

/*==================================================*/
/*  LLFloaterLocalBitmapBrowser : floater class     */
/*==================================================*/ 
std::list<LLFloaterLocalBitmapBrowser*> LLFloaterLocalBitmapBrowser::sSelfInstances;

LLFloaterLocalBitmapBrowser::LLFloaterLocalBitmapBrowser( const LLSD& key )
:   LLFloater( key )
{
	sSelfInstances.push_back(this);
}

BOOL LLFloaterLocalBitmapBrowser::postBuild()
{
	// setting element/xui children:
	mAddBtn         = getChild<LLButton>("add_btn");
	mDelBtn         = getChild<LLButton>("del_btn");
	mUploadBtn      = getChild<LLButton>("upload_btn");

	mBitmapList     = getChild<LLScrollListCtrl>("bitmap_list");
	mTextureView    = getChild<LLTextureCtrl>("texture_view");
	mUpdateChkBox   = getChild<LLCheckBoxCtrl>("keep_updating_checkbox");

	mPathTxt            = getChild<LLLineEditor>("path_text");
	mUUIDTxt            = getChild<LLLineEditor>("uuid_text");

	mLinkTxt		    = getChild<LLTextBox>("link_text");
	mTimeTxt		    = getChild<LLTextBox>("time_text");
	mTypeComboBox       = getChild<LLComboBox>("type_combobox");

	mCaptionPathTxt     = getChild<LLTextBox>("path_caption_text");
	mCaptionUUIDTxt     = getChild<LLTextBox>("uuid_caption_text");
	mCaptionLinkTxt     = getChild<LLTextBox>("link_caption_text");
	mCaptionTimeTxt	    = getChild<LLTextBox>("time_caption_text");

	// pre-disabling line editors, they're for view only and buttons that shouldn't be on on-spawn.
	mPathTxt->setEnabled( false );
	mUUIDTxt->setEnabled( false );

	mDelBtn->setEnabled( false );
	mUploadBtn->setEnabled( false );

	// setting button callbacks:
	mAddBtn->setClickedCallback(         onClickAdd,         this);
	mDelBtn->setClickedCallback(         onClickDel,         this);
	mUploadBtn->setClickedCallback(      onClickUpload,      this);
	
	// combo callback
	mTypeComboBox->setCommitCallback(onCommitTypeCombo, this);

	// scrolllist callbacks
	mBitmapList->setCommitCallback(onChooseBitmapList, this);

	// checkbox callbacks
	mUpdateChkBox->setCommitCallback(onClickUpdateChkbox, this);

	this->updateBitmapScrollList();

	return true;
}

LLFloaterLocalBitmapBrowser::~LLFloaterLocalBitmapBrowser()
{
	sSelfInstances.remove( this );
}

void LLFloaterLocalBitmapBrowser::onClickAdd(void* userdata)
{	
	gLocalBrowser->addBitmap();
}

void LLFloaterLocalBitmapBrowser::onClickDel(void* userdata)
{
	LLFloaterLocalBitmapBrowser* self = (LLFloaterLocalBitmapBrowser*) userdata;
	gLocalBrowser->delBitmap( self->mBitmapList->getAllSelected() );
}

void LLFloaterLocalBitmapBrowser::onClickUpload(void* userdata)
{
	LLFloaterLocalBitmapBrowser* self = (LLFloaterLocalBitmapBrowser*) userdata;
	if ( self->mBitmapList->getAllSelected().empty() ) { return; }

	std::string filename = gLocalBrowser->getBitmapUnit( 
		(LLUUID)self->mBitmapList->getSelectedItemLabel(BLIST_COL_ID) )->getFileName();

	if ( !filename.empty() )
	{
		LLFloaterReg::showInstance("upload_image", LLSD(filename));
	}
}

void LLFloaterLocalBitmapBrowser::onChooseBitmapList(LLUICtrl* ctrl, void *userdata)
{
	LLFloaterLocalBitmapBrowser* self = (LLFloaterLocalBitmapBrowser*) userdata;

	bool button_status = self->mBitmapList->isEmpty();
	self->mDelBtn->setEnabled(!button_status);
	self->mUploadBtn->setEnabled(!button_status);

	self->updateRightSide();
}

void LLFloaterLocalBitmapBrowser::onClickUpdateChkbox(LLUICtrl *ctrl, void *userdata)
{
	LLFloaterLocalBitmapBrowser* self = (LLFloaterLocalBitmapBrowser*) userdata;

	std::string temp_str = self->mBitmapList->getSelectedItemLabel(BLIST_COL_ID);
	if ( !temp_str.empty() )
	{
		gLocalBrowser->onUpdateBool( (LLUUID)temp_str );
		self->updateRightSide();
	}
}

void LLFloaterLocalBitmapBrowser::onCommitTypeCombo(LLUICtrl* ctrl, void *userdata)
{
	LLFloaterLocalBitmapBrowser* self = (LLFloaterLocalBitmapBrowser*) userdata;

	std::string temp_str = self->mBitmapList->getSelectedItemLabel(BLIST_COL_ID);

	if ( !temp_str.empty() )
	{
		S32 selection = self->mTypeComboBox->getCurrentIndex();
		gLocalBrowser->onSetType( (LLUUID)temp_str, selection ); 

	}
}

void LLFloaterLocalBitmapBrowser::updateBitmapScrollList()
{

	if ( sSelfInstances.empty() ) { return; }
	
	std::list<LLFloaterLocalBitmapBrowser*>::iterator iter;
	for( iter = sSelfInstances.begin(); iter != sSelfInstances.end(); iter++ )
	{
		LLFloaterLocalBitmapBrowser* sLFInstance = *iter;

		sLFInstance->mBitmapList->clearRows();
		if (!gLocalBrowser->sLoadedBitmaps.empty())
		{
		
			LLLocalBitmapBrowser::local_list_iter iter;
			for(iter = gLocalBrowser->sLoadedBitmaps.begin(); iter != gLocalBrowser->sLoadedBitmaps.end(); iter++)
			{
				LLSD element;
				element["columns"][BLIST_COL_NAME]["column"] = "bitmap_name";
				element["columns"][BLIST_COL_NAME]["type"]   = "text";
				element["columns"][BLIST_COL_NAME]["value"]  = (*iter)->getShortName();

				element["columns"][BLIST_COL_ID]["column"] = "bitmap_uuid";
				element["columns"][BLIST_COL_ID]["type"]   = "text";
				element["columns"][BLIST_COL_ID]["value"]  = (*iter)->getID();

				sLFInstance->mBitmapList->addElement(element);
			}

		}
		LLFloaterLocalBitmapBrowser::updateRightSide();
	}
}

void LLFloaterLocalBitmapBrowser::updateRightSide()
{
	if ( sSelfInstances.empty() ) { return; }
	
	std::list<LLFloaterLocalBitmapBrowser*>::iterator iter;
	for( iter = sSelfInstances.begin(); iter != sSelfInstances.end(); iter++ )
	{
		LLFloaterLocalBitmapBrowser* sLFInstance = *iter;

		if ( !sLFInstance->mBitmapList->getAllSelected().empty() )
		{ 
			LLLocalBitmap* unit = gLocalBrowser->getBitmapUnit( LLUUID(sLFInstance->mBitmapList->getSelectedItemLabel(BLIST_COL_ID)) );
		
			if ( unit )
			{
				sLFInstance->mTextureView->setImageAssetID( unit->getID() );
				sLFInstance->mUpdateChkBox->set( unit->getUpdateBool() );
				sLFInstance->mPathTxt->setText( unit->getFileName() );
				sLFInstance->mUUIDTxt->setText( unit->getID().asString() );
				sLFInstance->mTimeTxt->setText( unit->getLastModified().asString() );
				sLFInstance->mLinkTxt->setText( unit->getLinkStatus() );
				sLFInstance->mTypeComboBox->selectNthItem( unit->getType() );

				sLFInstance->mTextureView->setEnabled(true);
				sLFInstance->mUpdateChkBox->setEnabled(true);
				sLFInstance->mTypeComboBox->setEnabled(true);
			}
		}
		else
		{
			sLFInstance->mTextureView->setImageAssetID( NO_IMAGE );
			sLFInstance->mTextureView->setEnabled( false );
			sLFInstance->mUpdateChkBox->set( false );
			sLFInstance->mUpdateChkBox->setEnabled( false );

			sLFInstance->mTypeComboBox->selectFirstItem();
			sLFInstance->mTypeComboBox->setEnabled( false );
		
			sLFInstance->mPathTxt->setText( LLStringExplicit("None") );
			sLFInstance->mUUIDTxt->setText( LLStringExplicit("None") );
			sLFInstance->mLinkTxt->setText( LLStringExplicit("None") );
			sLFInstance->mTimeTxt->setText( LLStringExplicit("None") );
		}

	}
}


/*==================================================*/
/*     LLLocalBitmapBrowserTimer: timer class       */
/*==================================================*/ 
LLLocalBitmapBrowserTimer::LLLocalBitmapBrowserTimer() : LLEventTimer( (F32)TIMER_HEARTBEAT )
{

}

LLLocalBitmapBrowserTimer::~LLLocalBitmapBrowserTimer()
{

}

BOOL LLLocalBitmapBrowserTimer::tick()
{
	gLocalBrowser->performTimedActions();
	return FALSE;
}

void LLLocalBitmapBrowserTimer::start()
{
	mEventTimer.start();
}

void LLLocalBitmapBrowserTimer::stop()
{
	mEventTimer.stop();
}

bool LLLocalBitmapBrowserTimer::isRunning()
{
	return mEventTimer.getStarted();
}


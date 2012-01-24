/** 
 * @file kvfloaterflickrupload.cpp
 * @brief Flickr upload floater
 * @copyright Copyright (c) 2011 Katharine Berry
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "llviewerprecompiledheaders.h"

#include "kvfloaterflickrupload.h"

#include "llagent.h"
#include "llfloaterreg.h"
#include "llfloatersnapshot.h"
#include "llgl.h"
#include "llimage.h"
#include "llnotificationsutil.h"
#include "llslurl.h"
#include "llui.h"
#include "lluploaddialog.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llworld.h"

#include "kvflickr.h"
#include "kvfloaterflickrauth.h"

#include <boost/bind.hpp>

KVFloaterFlickrUpload::KVFloaterFlickrUpload(const LLSD& key) : LLFloater(key),
mCompressedImage(NULL),
mViewerImage(NULL)
{
}

KVFloaterFlickrUpload::~KVFloaterFlickrUpload()
{
	mCompressedImage = NULL;
	mViewerImage = NULL;
}

// static
KVFloaterFlickrUpload* KVFloaterFlickrUpload::showFromSnapshot(LLImageFormatted *compressed, LLViewerTexture *img, const LLVector2& img_scale, const LLVector3d& pos_taken_global)
{
	// Take the images from the caller
	// It's now our job to clean them up
	KVFloaterFlickrUpload* instance = LLFloaterReg::showTypedInstance<KVFloaterFlickrUpload>("flickr_upload", LLSD(img->getID()));
	
	instance->mCompressedImage = compressed;
	instance->mViewerImage = img;
	instance->mImageScale = img_scale;
	instance->mPosTakenGlobal = pos_taken_global;
	
	return instance;
}

BOOL KVFloaterFlickrUpload::postBuild()
{
	// Set the various UI fields to their default values.
	childSetValue("rating_combo", gSavedSettings.getLLSD("KittyFlickrLastRating"));
	childSetValue("tags_form", gSavedSettings.getLLSD("KittyFlickrLastTags"));
	childSetValue("show_position_check", gSavedSettings.getLLSD("KittyFlickrShowPosition"));

	// Connect the buttons up
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("upload_btn", onClickUpload, this);

	// Check that we actually can do an upload.
	LLSD query;
	query["auth_token"] = gSavedPerAccountSettings.getLLSD("KittyFlickrToken");
	KVFlickrRequest::request("flickr.auth.checkToken", query, boost::bind(&KVFloaterFlickrUpload::confirmToken, this, _1, _2));

	return true;
}

void KVFloaterFlickrUpload::confirmToken(bool success, const LLSD &response)
{
	if(!success)
	{
		LLNotificationsUtil::add("KittyFlickrHTTPFail");
		closeFloater(false);
		return;
	}
	if(response["stat"].asString() == "ok")
	{
		// Just in case the username changed. This can happen.
		std::string username = response["auth"]["user"]["username"];
		gSavedPerAccountSettings.setString("KittyFlickrUsername", username);
		childSetValue("account_name", username);
		childSetEnabled("upload_btn", true);
	}
	else
	{
		// Uh oh.
		if(response["code"].asInteger() == 98) // Invalid auth token
		{
			// Mark the account as invalid
			childSetValue("account_name", getString("no_account"));
			// Need to authenticate.
			gSavedPerAccountSettings.setString("KittyFlickrToken", "");
			gSavedPerAccountSettings.setString("KittyFlickrUsername", "");
			gSavedPerAccountSettings.setString("KittyFlickrNSID", "");
			KVFloaterFlickrAuth *floater = KVFloaterFlickrAuth::showFloater(boost::bind(&KVFloaterFlickrUpload::authCallback, this, _1));
			// Link it to us to protect it from freeze frame mode, if need be.
			if(floater && getDependee()) // (if we're depending on something, so should it)
			{
				gFloaterView->removeChild(floater);
				gSnapshotFloaterView->addChild(floater);
				// Even though we don't really want this to depend on the snapshot view
				// being open, if we manipulate it after closing the snapshot view,
				// it will crash.
				getDependee()->addDependentFloater(floater, false);
			}
			LLNotificationsUtil::add("KittyFlickrTokenRejected");
		}
		else
		{
			LLSD args;
			args["CODE"] = response["code"];
			args["ERROR"] = response["message"];
			LLNotificationsUtil::add("KittyFlickrGenericFail", args);
		}
	}
}

void KVFloaterFlickrUpload::authCallback(bool authorised)
{
	if(authorised)
	{
		childSetValue("account_name", gSavedPerAccountSettings.getString("KittyFlickrUsername"));
		childSetEnabled("upload_btn", true);
	}
	else
	{
		LLNotificationsUtil::add("KittyFlickrUploadCancelledAuthRejected");
		closeFloater(false);
	}
}

void KVFloaterFlickrUpload::saveSettings()
{
	gSavedSettings.setS32("KittyFlickrLastRating", childGetValue("rating_combo"));
	gSavedSettings.setString("KittyFlickrLastTags", childGetValue("tags_form"));
	gSavedSettings.setBOOL("KittyFlickrShowPosition", childGetValue("show_position_check"));
}

void KVFloaterFlickrUpload::uploadSnapshot()
{
	mTitle = childGetValue("title_form").asString();
	LLSD params;
	params["title"] = childGetValue("title_form");
	params["safety_level"] = childGetValue("rating_combo");
	std::string tags = childGetValue("tags_form");
	std::string description = childGetValue("description_form");
	if(childGetValue("show_position_check").asBoolean())
	{
		// Work out where this was taken.
		LLVector3d clamped_global = LLWorld::getInstance()->clipToVisibleRegions(gAgent.getPositionGlobal(), mPosTakenGlobal);
		LLViewerRegion* region = LLWorld::getInstance()->getRegionFromPosGlobal(clamped_global);
		if(!region)
		{
			// Clamping failed? Shouldn't happen.
			// Use the agent's position instead; if the region the agent is in doesn't exist we have some serious issues,
			// and crashing is an entirely reasonable thing to do.
			region = gAgent.getRegion();
			clamped_global = gAgent.getPositionGlobal();
		}
		std::string region_name = region->getName();
		LLVector3 region_pos = region->getPosRegionFromGlobal(clamped_global);
		std::ostringstream region_tags;
		region_tags << " \"secondlife:region=" << region_name << "\"";
		region_tags << " secondlife:x=" << llround(region_pos[VX]);
		region_tags << " secondlife:y=" << llround(region_pos[VY]);
		region_tags << " secondlife:z=" << llround(region_pos[VZ]);

		// Now let's give some precise camera values.
		region_tags << " secondlife:camera_pos_x=" << (mPosTakenGlobal[VX] - region->getOriginGlobal()[VX]);
		region_tags << " secondlife:camera_pos_y=" << (mPosTakenGlobal[VY] - region->getOriginGlobal()[VY]);
		region_tags << " secondlife:camera_pos_z=" << mPosTakenGlobal[VZ];
		tags += region_tags.str();

		// Include an SLurl in the description, too (maybe).
		if(gSavedSettings.getBOOL("KittyFlickrIncludeSLURL"))
		{
			LLSLURL url(region_name, region_pos);
			std::ostringstream region_desc;
			region_desc << "<em><a href='" << url.getSLURLString() << "'>";
			region_desc << "Taken at " << region_name << " (";
			region_desc << llround(region_pos[VX]) << ", ";
			region_desc << llround(region_pos[VY]) << ", ";
			region_desc << llround(region_pos[VZ]) << ")";
			region_desc << "</a></em>";
			if(description != "")
			{
				description += "\n\n";
			}
			description += region_desc.str();
		}
	}
	params["tags"] = tags;
	params["description"] = description;
	LL_INFOS("FlickrAPI") << "Uploading snapshot with metadata: " << params << LL_ENDL;

	params["auth_token"] = gSavedPerAccountSettings.getLLSD("KittyFlickrToken");
	LLUploadDialog::modalUploadDialog(getString("uploading"));
	KVFlickrRequest::uploadPhoto(params, mCompressedImage, boost::bind(&KVFloaterFlickrUpload::imageUploaded, this, _1, _2));
}

void KVFloaterFlickrUpload::imageUploaded(bool success, const LLSD& response)
{
	LLUploadDialog::modalUploadFinished();
	LLSD args;
	args["TITLE"] = mTitle;
	if(success)
	{
		args["ID"] = response["photoid"];
		LLNotificationsUtil::add("KittyFlickrUploadComplete", args);
	}
	else if(response.has("stat"))
	{
		args["CODE"] = response["code"];
		args["ERROR"] = response["msg"];
		LLNotificationsUtil::add("KittyFlickrUploadFailed", args);
	}
	else
	{
		LLNotificationsUtil::add("KittyFlickrUploadFailedNoError");
	}

	// We're pretty much done now.
	closeFloater(false);
}

// This function stolen from LLFloaterPostcard
void KVFloaterFlickrUpload::draw()
{
	LLGLSUIDefault gls_ui;
	LLFloater::draw();
	
	if(!isMinimized() && mViewerImage.notNull() && mCompressedImage.notNull()) 
	{
		LLRect rect(getRect());
		
		// first set the max extents of our preview
		rect.translate(-rect.mLeft, -rect.mBottom);
		rect.mLeft += 280;
		rect.mRight -= 10;
		rect.mTop -= 27;
		rect.mBottom = rect.mTop - 130;
		
		// then fix the aspect ratio
		F32 ratio = (F32)mCompressedImage->getWidth() / (F32)mCompressedImage->getHeight();
		if ((F32)rect.getWidth() / (F32)rect.getHeight() >= ratio)
		{
			rect.mRight = LLRect::tCoordType((F32)rect.mLeft + ((F32)rect.getHeight() * ratio));
		}
		else
		{
			rect.mBottom = LLRect::tCoordType((F32)rect.mTop - ((F32)rect.getWidth() / ratio));
		}
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gl_rect_2d(rect, LLColor4(0.f, 0.f, 0.f, 1.f));
			rect.stretch(-1);
		}
		{
			
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.pushMatrix();
			{
				gGL.scalef(mImageScale.mV[VX], mImageScale.mV[VY], 1.f);
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gl_draw_scaled_image(rect.mLeft,
									 rect.mBottom,
									 rect.getWidth(),
									 rect.getHeight(),
									 mViewerImage.get(), 
									 LLColor4::white);
			}
			gGL.matrixMode(LLRender::MM_TEXTURE);
			gGL.popMatrix();
			gGL.matrixMode(LLRender::MM_MODELVIEW);
		}
	}
}

//static
void KVFloaterFlickrUpload::onClickCancel(void* data)
{
	if(data)
	{
		KVFloaterFlickrUpload *self = (KVFloaterFlickrUpload*)data;
		self->closeFloater(false);
	}
}

//static
void KVFloaterFlickrUpload::onClickUpload(void* data)
{
	if(!data)
		return;
	KVFloaterFlickrUpload *self = (KVFloaterFlickrUpload*)data;
	self->uploadSnapshot();
	self->saveSettings();
	self->setVisible(false);
	// Make sure that, if we were attached to anything, that we detach from it.
	// Otherwise bad things happen.
	LLFloater *dependee = self->getDependee();
	if(dependee)
		dependee->removeDependentFloater(self);
}

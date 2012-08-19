/**
 * @file exopanelsnapshotflickr.cpp
 * @brief Provides the UI for uploading a snapshot to Flickr.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Copyright (C) 2012 Katharine Berry
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llcombobox.h"
#include "llsidetraypanelcontainer.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"

#include "llfloatersnapshot.h" // FIXME: replace with a snapshot storage model
#include "llpanelsnapshot.h"
#include "llviewercontrol.h" // gSavedSettings
#include "llviewerwindow.h"
#include "llagent.h"
#include "llviewerregion.h"
#include "llslurl.h"
#include "llnotificationsutil.h"

#include "exoflickr.h"
#include "exoflickrauth.h"

class exoPanelSnapshotFlickr
:	public LLPanelSnapshot
{
	LOG_CLASS(exoPanelSnapshotFlickr);

public:
	exoPanelSnapshotFlickr();
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& key);

private:
	/*virtual*/ std::string getWidthSpinnerName() const		{ return "flickr_snapshot_width"; }
	/*virtual*/ std::string getHeightSpinnerName() const	{ return "flickr_snapshot_height"; }
	/*virtual*/ std::string getAspectRatioCBName() const	{ return "flickr_keep_aspect_check"; }
	/*virtual*/ std::string getImageSizeComboName() const	{ return "flickr_size_combo"; }
	/*virtual*/ std::string getImageSizePanelName() const	{ return "flickr_image_size_lp"; }
	/*virtual*/ LLFloaterSnapshot::ESnapshotFormat getImageFormat() const;
	// <FS:TS> Firestorm-specific changes for temp upload and parm saving fixes
	/*virtual*/ std::string getTempUploadCBName() const		{ return LLStringUtil::null; }
	/*virtual*/ std::string getImageSizeControlName() const	{ return "LastSnapshotToFlickrResolution"; }
	// </FS:TS>
	/*virtual*/ void updateControls(const LLSD& info);

	void onFormatComboCommit(LLUICtrl* ctrl);
	void onQualitySliderCommit(LLUICtrl* ctrl);
	void onRatingComboCommit(LLUICtrl *ctrl);
	void onUpload();
	void swapTab(S32 btn_idx);
	void flickrAuthResponse(bool success, const LLSD& response);
	void uploadCallback(bool success, const LLSD& response);
};

static LLRegisterPanelClassWrapper<exoPanelSnapshotFlickr> panel_class("exopanelsnapshotflickr");

void exoPanelSnapshotFlickr::onOpen(const LLSD& key)
{
	// If the snapshot type isn't JPEG or PNG we have to refresh
	LLFloaterSnapshot::ESnapshotFormat fmt = (LLFloaterSnapshot::ESnapshotFormat) gSavedSettings.getS32("SnapshotFormat");
	if(fmt != LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG && fmt != LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG)
	{
		LLFloaterSnapshot::getInstance()->notify(LLSD().with("image-format-change", true));
	}
	LLPanelSnapshot::onOpen(key);

	// Reauthorise if necessary.
	LLFloaterSnapshot::getInstance()->notify(LLSD().with("set-working", true));
	new exoFlickrAuth(boost::bind(&exoPanelSnapshotFlickr::flickrAuthResponse, this, _1, _2));
}

exoPanelSnapshotFlickr::exoPanelSnapshotFlickr()
{
	mCommitCallbackRegistrar.add("Flickr.Upload",	boost::bind(&exoPanelSnapshotFlickr::onUpload,	this));
	// <FS:TS> Removed because we use a tab container instead of buttons
	// mCommitCallbackRegistrar.add("Flickr.Cancel",	boost::bind(&exoPanelSnapshotFlickr::cancel,	this));
	// mCommitCallbackRegistrar.add("Flickr.Metadata",	boost::bind(&exoPanelSnapshotFlickr::swapTab,	this, 0));
	// mCommitCallbackRegistrar.add("Flickr.Settings",	boost::bind(&exoPanelSnapshotFlickr::swapTab,	this, 1));
	// </FS:TS>
}

void exoPanelSnapshotFlickr::flickrAuthResponse(bool success, const LLSD& response)
{
	if(!success)
	{
		// Complain about failed auth here.
		LL_WARNS("Flickr") << "Flickr authentication failed." << LL_ENDL;
		cancel();
	}
	else
	{
		LLFloaterSnapshot::getInstance()->notify(LLSD().with("set-ready", true));
	}
}

// <FS:TS> Removed because we use a tab container
//void exoPanelSnapshotFlickr::swapTab(S32 which)
//{
//	LLButton *buttons[2] = {
//		getChild<LLButton>("metadata_btn"),
//		getChild<LLButton>("settings_btn"),
//	};
//
//	LLButton* clicked_btn = buttons[which];
//	LLButton* other_btn = buttons[!which];
//	LLSideTrayPanelContainer* container =
//		getChild<LLSideTrayPanelContainer>("flickr_panel_container");
//	
//	container->selectTab(clicked_btn->getToggleState() ? which : !which);
//	other_btn->toggleState();
//}
// </FS:TS>

BOOL exoPanelSnapshotFlickr::postBuild()
{
	getChild<LLUICtrl>("title_form")->setFocus(TRUE);
	getChild<LLUICtrl>("image_quality_slider")->setCommitCallback(boost::bind(&exoPanelSnapshotFlickr::onQualitySliderCommit, this, _1));
	getChild<LLUICtrl>("flickr_format_combo")->setCommitCallback(boost::bind(&exoPanelSnapshotFlickr::onFormatComboCommit, this, _1));
	getChild<LLUICtrl>("rating_combo")->setCommitCallback(boost::bind(&exoPanelSnapshotFlickr::onRatingComboCommit, this, _1));
	// <FS:TS> Removed because we use a tab container instead of buttons
	//getChild<LLButton>("metadata_btn")->setToggleState(TRUE);
	// </FS:TS>

	return LLPanelSnapshot::postBuild();
}

// virtual
void exoPanelSnapshotFlickr::updateControls(const LLSD& info)
{
	LLFloaterSnapshot::ESnapshotFormat fmt = (LLFloaterSnapshot::ESnapshotFormat) gSavedSettings.getS32("SnapshotFormat");
	getChild<LLComboBox>("local_format_combo")->selectNthItem((S32) fmt);

	const bool show_quality_ctrls = (fmt == LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG);
	getChild<LLUICtrl>("image_quality_slider")->setVisible(show_quality_ctrls);
	getChild<LLUICtrl>("image_quality_level")->setVisible(show_quality_ctrls);

	getChild<LLUICtrl>("image_quality_slider")->setValue(gSavedSettings.getS32("SnapshotQuality"));
	updateImageQualityLevel();

	const bool have_snapshot = info.has("have-snapshot") ? info["have-snapshot"].asBoolean() : true;
	getChild<LLUICtrl>("upload_btn")->setEnabled(have_snapshot);

	getChild<LLUICtrl>("rating_combo")->setValue(gSavedSettings.getS32("ExodusFlickrLastRating"));
}

void exoPanelSnapshotFlickr::onQualitySliderCommit(LLUICtrl* ctrl)
{
	updateImageQualityLevel();

	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	S32 quality_val = llfloor((F32)slider->getValue().asReal());
	LLSD info;
	info["image-quality-change"] = quality_val;
	LLFloaterSnapshot::getInstance()->notify(info);
}

void exoPanelSnapshotFlickr::onFormatComboCommit(LLUICtrl* ctrl)
{
	LLFloaterSnapshot::getInstance()->notify(LLSD().with("image-format-change", true));
}

void exoPanelSnapshotFlickr::onRatingComboCommit(LLUICtrl* ctrl)
{
	gSavedSettings.setS32("ExodusFlickrLastRating", ctrl->getValue().asInteger());
}

void exoPanelSnapshotFlickr::onUpload()
{
	// Build metadata params
	LLSD params;
	params["title"] = getChild<LLUICtrl>("title_form")->getValue();
	params["safety_level"] = getChild<LLUICtrl>("rating_combo")->getValue();
	std::ostringstream tags;
	std::ostringstream description;
	tags << getChild<LLUICtrl>("tags_form")->getValue().asString();
	description << getChild<LLUICtrl>("description_form")->getValue().asString();

	// Deal with location inclusion, if necessary.
	if(getChild<LLUICtrl>("location_check")->getValue().asBoolean())
	{
		// Work out our position.
		LLVector3d global = gAgent.getPositionGlobal();
		LLViewerRegion* region = gAgent.getRegion();
		if(region)
		{
			LLVector3 region_pos = region->getPosRegionFromGlobal(global);
			std::string region_name = region->getName();
			tags << " \"secondlife:region=" << region_name << "\"";
			tags << " secondlife:x=" << llround(region_pos[VX]);
			tags << " secondlife:y=" << llround(region_pos[VY]);
			tags << " secondlife:z=" << llround(region_pos[VZ]);
			
			if(gSavedSettings.getBOOL("ExodusFlickrIncludeSLURL"))
			{
				LLSLURL url(region_name, region_pos);
				if((int)description.tellp() != 0)
				{
					description << "\n\n";
				}
				description << "<em><a href='" << url.getSLURLString() << "'>";
				description << "Taken at " << region_name << " (";
				description << llround(region_pos[VX]) << ", ";
				description << llround(region_pos[VY]) << ", ";
				description << llround(region_pos[VZ]) << ")";
				description << "</a></em>";
			}
		}
		else
		{
			tags << " \"secondlife:region=(unknown)\"";
		}
		tags << " secondlife:global_x=" << global[VX];
		tags << " secondlife:global_y=" << global[VY];
		tags << " secondlfie:global_z=" << global[VZ];
	}

	params["tags"] = tags.str();
	params["description"] = description.str();
	exoFlickr::uploadPhoto(params, LLFloaterSnapshot::getImageData(), boost::bind(&exoPanelSnapshotFlickr::uploadCallback, this, _1, _2));

	gViewerWindow->playSnapshotAnimAndSound();

	LLFloaterSnapshot::postSave();
}

void exoPanelSnapshotFlickr::uploadCallback(bool success, const LLSD& response)
{
	LLSD args;
	if(success && response["stat"].asString() == "ok")
	{
		args["ID"] = response["photoid"];
		LLNotificationsUtil::add("ExodusFlickrUploadComplete", args);
		LLFloaterSnapshot::getInstance()->notify(LLSD().with("set-finished", LLSD().with("ok", true).with("msg", "flickr")));
	}
	else
	{
		LLFloaterSnapshot::getInstance()->notify(LLSD().with("set-finished", LLSD().with("ok", false).with("msg", "flickr")));
	}
}

// virtual
LLFloaterSnapshot::ESnapshotFormat exoPanelSnapshotFlickr::getImageFormat() const
{
	std::string id = getChild<LLComboBox>("flickr_format_combo")->getValue().asString();
	if(id == "JPEG")
	{
		return LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG;
	}
	// Default to PNG if we get something we don't understand (or PNG).
	return LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG;
}

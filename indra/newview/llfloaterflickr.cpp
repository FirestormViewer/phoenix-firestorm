/** 
* @file llfloaterflickr.cpp
* @brief Implementation of llfloaterflickr
* @author cho@lindenlab.com
*
* $LicenseInfo:firstyear=2013&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2013, Linden Research, Inc.
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

#include "llfloaterflickr.h"

#include "llagent.h"
#include "llagentui.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llflickrconnect.h"
#include "llfloaterreg.h"
#include "lliconctrl.h"
#include "llimagefiltersmanager.h"
#include "llresmgr.h"		// LLLocale
#include "llsdserialize.h"
#include "llloadingindicator.h"
#include "llplugincookiestore.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llsnapshotlivepreview.h"
#include "llfloaterbigpreview.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "lltabcontainer.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include <boost/regex.hpp>
#include "llspinctrl.h"

#include "llviewernetwork.h"
#include "llnotificationsutil.h"
#ifdef OPENSIM
#include "exoflickr.h"
#include "exoflickrauth.h"
#include "llnotificationsutil.h"
#include "llviewernetwork.h"
#endif

static LLPanelInjector<LLFlickrPhotoPanel> t_panel_photo("llflickrphotopanel");
static LLPanelInjector<LLFlickrAccountPanel> t_panel_account("llflickraccountpanel");

const std::string DEFAULT_PHOTO_QUERY_PARAMETERS = "?sourceid=slshare_photo&utm_source=flickr&utm_medium=photo&utm_campaign=slshare";
// <FS:Ansariel> Don't assume we're always in Second Life
//const std::string DEFAULT_TAG_TEXT = "secondlife ";
//const std::string FLICKR_MACHINE_TAGS_NAMESPACE = "secondlife";
const std::string DEFAULT_TAG_TEXT = "Firestorm ";
static std::string FLICKR_MACHINE_TAGS_NAMESPACE = "secondlife";
// </FS:Ansariel>

///////////////////////////
//LLFlickrPhotoPanel///////
///////////////////////////

LLFlickrPhotoPanel::LLFlickrPhotoPanel() :
mResolutionComboBox(NULL),
mRefreshBtn(NULL),
mBtnPreview(NULL),
mWorkingLabel(NULL),
mThumbnailPlaceholder(NULL),
mTitleTextBox(NULL),
mDescriptionTextBox(NULL),
mLocationCheckbox(NULL),
mTagsTextBox(NULL),
mRatingComboBox(NULL),
mBigPreviewFloater(NULL),
mPostButton(NULL)
{
	mCommitCallbackRegistrar.add("SocialSharing.SendPhoto", boost::bind(&LLFlickrPhotoPanel::onSend, this));
	mCommitCallbackRegistrar.add("SocialSharing.RefreshPhoto", boost::bind(&LLFlickrPhotoPanel::onClickNewSnapshot, this));
	mCommitCallbackRegistrar.add("SocialSharing.BigPreview", boost::bind(&LLFlickrPhotoPanel::onClickBigPreview, this));
}

LLFlickrPhotoPanel::~LLFlickrPhotoPanel()
{
	if(mPreviewHandle.get())
	{
		mPreviewHandle.get()->die();
	}

	// <FS:Ansariel> Store settings at logout
	gSavedSettings.setS32("FSLastSnapshotToFlickrResolution", getChild<LLComboBox>("resolution_combobox")->getCurrentIndex());
	gSavedSettings.setS32("FSLastSnapshotToFlickrWidth", getChild<LLSpinCtrl>("custom_snapshot_width")->getValue().asInteger());
	gSavedSettings.setS32("FSLastSnapshotToFlickrHeight", getChild<LLSpinCtrl>("custom_snapshot_height")->getValue().asInteger());
	// </FS:Ansariel>
}

BOOL LLFlickrPhotoPanel::postBuild()
{
	setVisibleCallback(boost::bind(&LLFlickrPhotoPanel::onVisibilityChange, this, _2));
	
	mResolutionComboBox = getChild<LLUICtrl>("resolution_combobox");
	mResolutionComboBox->setCommitCallback(boost::bind(&LLFlickrPhotoPanel::updateResolution, this, TRUE));
	mFilterComboBox = getChild<LLUICtrl>("filters_combobox");
	mFilterComboBox->setCommitCallback(boost::bind(&LLFlickrPhotoPanel::updateResolution, this, TRUE));
	mRefreshBtn = getChild<LLUICtrl>("new_snapshot_btn");
	mBtnPreview = getChild<LLButton>("big_preview_btn");
    mWorkingLabel = getChild<LLUICtrl>("working_lbl");
	mThumbnailPlaceholder = getChild<LLUICtrl>("thumbnail_placeholder");
	mTitleTextBox = getChild<LLUICtrl>("photo_title");
	mDescriptionTextBox = getChild<LLUICtrl>("photo_description");
	mLocationCheckbox = getChild<LLUICtrl>("add_location_cb");
	mTagsTextBox = getChild<LLUICtrl>("photo_tags");
	// <FS:Ansariel> Don't assume we're always in Second Life
	//mTagsTextBox->setValue(DEFAULT_TAG_TEXT);
	mTagsTextBox->setValue(DEFAULT_TAG_TEXT + (LLGridManager::instance().isInSecondLife() ? "secondlife" : ("\"" + LLGridManager::instance().getGridId()) + "\"") + " ");
	// </FS:Ansariel>
	mRatingComboBox = getChild<LLUICtrl>("rating_combobox");
	mPostButton = getChild<LLUICtrl>("post_photo_btn");
	mCancelButton = getChild<LLUICtrl>("cancel_photo_btn");
	mBigPreviewFloater = dynamic_cast<LLFloaterBigPreview*>(LLFloaterReg::getInstance("big_preview"));

	// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
	getChild<LLSpinCtrl>("custom_snapshot_width")->setCommitCallback(boost::bind(&LLFlickrPhotoPanel::updateResolution, this, TRUE));
	getChild<LLSpinCtrl>("custom_snapshot_height")->setCommitCallback(boost::bind(&LLFlickrPhotoPanel::updateResolution, this, TRUE));
	getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->setCommitCallback(boost::bind(&LLFlickrPhotoPanel::updateResolution, this, TRUE));

	getChild<LLComboBox>("resolution_combobox")->setCurrentByIndex(gSavedSettings.getS32("FSLastSnapshotToFlickrResolution"));
	getChild<LLSpinCtrl>("custom_snapshot_width")->setValue(gSavedSettings.getS32("FSLastSnapshotToFlickrWidth"));
	getChild<LLSpinCtrl>("custom_snapshot_height")->setValue(gSavedSettings.getS32("FSLastSnapshotToFlickrHeight"));
	// </FS:Ansariel>

	// Update filter list
    std::vector<std::string> filter_list = LLImageFiltersManager::getInstance()->getFiltersList();
	LLComboBox* filterbox = static_cast<LLComboBox *>(mFilterComboBox);
    for (U32 i = 0; i < filter_list.size(); i++) 
	{
        filterbox->add(filter_list[i]);
    }

	return LLPanel::postBuild();
}

// virtual
S32 LLFlickrPhotoPanel::notify(const LLSD& info)
{
	if (info.has("snapshot-updating"))
	{
        // Disable the Post button and whatever else while the snapshot is not updated
        // updateControls();
		return 1;
	}
    
	if (info.has("snapshot-updated"))
	{
        // Enable the send/post/save buttons.
        updateControls();
        
		// The refresh button is initially hidden. We show it after the first update,
		// i.e. after snapshot is taken
		LLUICtrl * refresh_button = getRefreshBtn();
		if (!refresh_button->getVisible())
		{
			refresh_button->setVisible(true);
		}
		return 1;
	}
    
	return 0;
}

void LLFlickrPhotoPanel::draw()
{ 
	LLSnapshotLivePreview * previewp = static_cast<LLSnapshotLivePreview *>(mPreviewHandle.get());

    // Enable interaction only if no transaction with the service is on-going (prevent duplicated posts)
    bool no_ongoing_connection = !(LLFlickrConnect::instance().isTransactionOngoing());
// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	if (!LLGridManager::getInstance()->isInSecondLife())
	{
		LLFlickrConnect::EConnectionState connection_state = LLFlickrConnect::instance().getConnectionState();
		no_ongoing_connection &= (connection_state != LLFlickrConnect::FLICKR_CONNECTION_IN_PROGRESS &&
									connection_state != LLFlickrConnect::FLICKR_CONNECTION_FAILED &&
									connection_state != LLFlickrConnect::FLICKR_NOT_CONNECTED);
	}
#endif
// </FS:Ansariel>
    mCancelButton->setEnabled(no_ongoing_connection);
    mTitleTextBox->setEnabled(no_ongoing_connection);
    mDescriptionTextBox->setEnabled(no_ongoing_connection);
    mTagsTextBox->setEnabled(no_ongoing_connection);
    mRatingComboBox->setEnabled(no_ongoing_connection);
    mResolutionComboBox->setEnabled(no_ongoing_connection);
    mFilterComboBox->setEnabled(no_ongoing_connection);
    mRefreshBtn->setEnabled(no_ongoing_connection);
    mBtnPreview->setEnabled(no_ongoing_connection);
    mLocationCheckbox->setEnabled(no_ongoing_connection);
    
    // Reassign the preview floater if we have the focus and the preview exists
    if (hasFocus() && isPreviewVisible())
    {
        attachPreview();
    }
    
    // Toggle the button state as appropriate
    bool preview_active = (isPreviewVisible() && mBigPreviewFloater->isFloaterOwner(getParentByType<LLFloater>()));
	mBtnPreview->setToggleState(preview_active);
    
    // Display the preview if one is available
	if (previewp && previewp->getThumbnailImage())
	{
		const LLRect& thumbnail_rect = mThumbnailPlaceholder->getRect();
		const S32 thumbnail_w = previewp->getThumbnailWidth();
		const S32 thumbnail_h = previewp->getThumbnailHeight();

		// calc preview offset within the preview rect
		const S32 local_offset_x = (thumbnail_rect.getWidth()  - thumbnail_w) / 2 ;
		const S32 local_offset_y = (thumbnail_rect.getHeight() - thumbnail_h) / 2 ;
		S32 offset_x = thumbnail_rect.mLeft + local_offset_x;
		S32 offset_y = thumbnail_rect.mBottom + local_offset_y;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		// Apply floater transparency to the texture unless the floater is focused.
		F32 alpha = getTransparencyType() == TT_ACTIVE ? 1.0f : getCurrentTransparency();
		LLColor4 color = LLColor4::white;
		gl_draw_scaled_image(offset_x, offset_y, 
			thumbnail_w, thumbnail_h,
			previewp->getThumbnailImage(), color % alpha);
	}

    // Update the visibility of the working (computing preview) label
    mWorkingLabel->setVisible(!(previewp && previewp->getSnapshotUpToDate()));
    
    // Enable Post if we have a preview to send and no on going connection being processed
    mPostButton->setEnabled(no_ongoing_connection && (previewp && previewp->getSnapshotUpToDate()));
    
    // Draw the rest of the panel on top of it
	LLPanel::draw();
}

LLSnapshotLivePreview* LLFlickrPhotoPanel::getPreviewView()
{
	LLSnapshotLivePreview* previewp = (LLSnapshotLivePreview*)mPreviewHandle.get();
	return previewp;
}

void LLFlickrPhotoPanel::onVisibilityChange(BOOL visible)
{
	if (visible)
	{
		if (mPreviewHandle.get())
		{
			LLSnapshotLivePreview* preview = getPreviewView();
			if(preview)
			{
				LL_DEBUGS() << "opened, updating snapshot" << LL_ENDL;
				preview->updateSnapshot(TRUE);
			}
		}
		else
		{
			LLRect full_screen_rect = getRootView()->getRect();
			LLSnapshotLivePreview::Params p;
			p.rect(full_screen_rect);
			LLSnapshotLivePreview* previewp = new LLSnapshotLivePreview(p);
			mPreviewHandle = previewp->getHandle();

            previewp->setContainer(this);
            previewp->setSnapshotType(LLSnapshotModel::SNAPSHOT_WEB);
            previewp->setSnapshotFormat(LLSnapshotModel::SNAPSHOT_FORMAT_PNG);
            previewp->setThumbnailSubsampled(TRUE);     // We want the preview to reflect the *saved* image
            previewp->setAllowRenderUI(FALSE);          // We do not want the rendered UI in our snapshots
            previewp->setAllowFullScreenPreview(FALSE);  // No full screen preview in SL Share mode
			previewp->setThumbnailPlaceholderRect(mThumbnailPlaceholder->getRect());

			updateControls();
		}
	}
}

void LLFlickrPhotoPanel::onClickNewSnapshot()
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	if (previewp)
	{
		previewp->updateSnapshot(TRUE);
	}
}

void LLFlickrPhotoPanel::onClickBigPreview()
{
    // Toggle the preview
    if (isPreviewVisible())
    {
        LLFloaterReg::hideInstance("big_preview");
    }
    else
    {
        attachPreview();
        LLFloaterReg::showInstance("big_preview");
    }
}

bool LLFlickrPhotoPanel::isPreviewVisible()
{
    return (mBigPreviewFloater && mBigPreviewFloater->getVisible());
}

void LLFlickrPhotoPanel::attachPreview()
{
    if (mBigPreviewFloater)
    {
        LLSnapshotLivePreview* previewp = getPreviewView();
        mBigPreviewFloater->setPreview(previewp);
        mBigPreviewFloater->setFloaterOwner(getParentByType<LLFloater>());
    }
}

void LLFlickrPhotoPanel::onSend()
{
	LLEventPumps::instance().obtain("FlickrConnectState").stopListening("LLFlickrPhotoPanel"); // just in case it is already listening
	LLEventPumps::instance().obtain("FlickrConnectState").listen("LLFlickrPhotoPanel", boost::bind(&LLFlickrPhotoPanel::onFlickrConnectStateChange, this, _1));
	
// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInSecondLife())
	{
#endif
// </FS:Ansariel>
	// Connect to Flickr if necessary and then post
	if (LLFlickrConnect::instance().isConnected())
	{
		sendPhoto();
	}
	else
	{
		LLFlickrConnect::instance().checkConnectionToFlickr(true);
	}
// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	}
	else
	{
		sendPhoto();
	}
#endif
// </FS:Ansariel>
}

bool LLFlickrPhotoPanel::onFlickrConnectStateChange(const LLSD& data)
{
	switch (data.get("enum").asInteger())
	{
		case LLFlickrConnect::FLICKR_CONNECTED:
			sendPhoto();
			break;

		case LLFlickrConnect::FLICKR_POSTED:
			LLEventPumps::instance().obtain("FlickrConnectState").stopListening("LLFlickrPhotoPanel");
			// <FS:Ansariel> FIRE-15948: Don't close floater after each post and retain entered text
			//clearAndClose();
			break;
	}

	return false;
}

void LLFlickrPhotoPanel::sendPhoto()
{
	// Get the title, description, and tags
	std::string title = mTitleTextBox->getValue().asString();
	std::string description = mDescriptionTextBox->getValue().asString();
	std::string tags = mTagsTextBox->getValue().asString();

	// Add the location if required
	bool add_location = mLocationCheckbox->getValue().asBoolean();
	if (add_location)
	{
		// Get the SLURL for the location
		LLSLURL slurl;
		LLAgentUI::buildSLURL(slurl);
		std::string slurl_string = slurl.getSLURLString();

		// Add query parameters so Google Analytics can track incoming clicks!
		// <FS:Ansariel> Don't assume we're always in Second Life
		//slurl_string += DEFAULT_PHOTO_QUERY_PARAMETERS;
		if (LLGridManager::instance().isInSecondLife())
		{
			slurl_string += DEFAULT_PHOTO_QUERY_PARAMETERS;
		}
		// </FS:Ansariel>

		std::string photo_link_text = "Visit this location";// at [] in Second Life";
		std::string parcel_name = LLViewerParcelMgr::getInstance()->getAgentParcelName();
		if (!parcel_name.empty())
		{
			boost::regex pattern = boost::regex("\\S\\.[a-zA-Z]{2,}");
			boost::match_results<std::string::const_iterator> matches;
			if(!boost::regex_search(parcel_name, matches, pattern))
			{
				photo_link_text += " at " + parcel_name;
			}
		}
		// <FS:Ansariel> Don't assume we're always in Second Life
		//photo_link_text += " in Second Life";
		if (LLGridManager::instance().isInSecondLife())
		{
			photo_link_text += " in Second Life";
		}
		else
		{
			photo_link_text += " in \"" + LLGridManager::instance().getGridLabel() + "\"";
		}
		// </FS:Ansariel>

		slurl_string = "<a href=\"" + slurl_string + "\">" + photo_link_text + "</a>";

		// Add it to the description (pretty crude, but we don't have a better option with photos)
		if (description.empty())
			description = slurl_string;
		else
			description = description + "\n\n" + slurl_string;

		// Also add special "machine tags" with location metadata
		const LLVector3& agent_pos_region = gAgent.getPositionAgent();
		LLViewerRegion* region = gAgent.getRegion();
		LLParcel* parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		if (region && parcel)
		{
			S32 pos_x = S32(agent_pos_region.mV[VX]);
			S32 pos_y = S32(agent_pos_region.mV[VY]);
			S32 pos_z = S32(agent_pos_region.mV[VZ]);
			
			std::string parcel_name = LLViewerParcelMgr::getInstance()->getAgentParcelName();
			std::string region_name = region->getName();
			
			// <FS:Ansariel> Don't assume we're always in Second Life
			if (!LLGridManager::instance().isInSecondLife())
			{
				FLICKR_MACHINE_TAGS_NAMESPACE = LLGridManager::instance().getGridId();
			}
			// </FS:Ansariel>

			if (!region_name.empty())
			{
				tags += llformat(" \"%s:region=%s\"", FLICKR_MACHINE_TAGS_NAMESPACE.c_str(), region_name.c_str());
			}
			if (!parcel_name.empty())
			{
				tags += llformat(" \"%s:parcel=%s\"", FLICKR_MACHINE_TAGS_NAMESPACE.c_str(), parcel_name.c_str());
			}
			tags += llformat(" \"%s:x=%d\"", FLICKR_MACHINE_TAGS_NAMESPACE.c_str(), pos_x);
			tags += llformat(" \"%s:y=%d\"", FLICKR_MACHINE_TAGS_NAMESPACE.c_str(), pos_y);
			tags += llformat(" \"%s:z=%d\"", FLICKR_MACHINE_TAGS_NAMESPACE.c_str(), pos_z);
		}
	}

	// Get the content rating
	int content_rating = mRatingComboBox->getValue().asInteger();

	// Get the image
	LLSnapshotLivePreview* previewp = getPreviewView();
	
// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInSecondLife())
	{
#endif
// </FS:Ansariel>
	// Post to Flickr
	LLFlickrConnect::instance().uploadPhoto(previewp->getFormattedImage(), title, description, tags, content_rating);
// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	}
	else
	{
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_POSTING);
		LLSD params;
		params["title"] = title;
		params["safety_level"] = content_rating;
		params["tags"] = tags;
		params["description"] = description;
		exoFlickr::uploadPhoto(params, previewp->getFormattedImage().get(), boost::bind(&LLFlickrPhotoPanel::uploadCallback, this, _1, _2));
	}
#endif
// </FS:Ansariel>

	updateControls();
}

void LLFlickrPhotoPanel::clearAndClose()
{
	mTitleTextBox->setValue("");
	mDescriptionTextBox->setValue("");

	LLFloater* floater = getParentByType<LLFloater>();
	if (floater)
	{
		floater->closeFloater();
        if (mBigPreviewFloater)
        {
            mBigPreviewFloater->closeOnFloaterOwnerClosing(floater);
        }
	}
}

void LLFlickrPhotoPanel::updateControls()
{
	LLSnapshotLivePreview* previewp = getPreviewView();
	BOOL got_snap = previewp && previewp->getSnapshotUpToDate();

	// *TODO: Separate maximum size for Web images from postcards
	LL_DEBUGS() << "Is snapshot up-to-date? " << got_snap << LL_ENDL;

	updateResolution(FALSE);
}

void LLFlickrPhotoPanel::updateResolution(BOOL do_update)
{
	LLComboBox* combobox  = static_cast<LLComboBox *>(mResolutionComboBox);
	LLComboBox* filterbox = static_cast<LLComboBox *>(mFilterComboBox);

	std::string sdstring = combobox->getSelectedValue();
	LLSD sdres;
	std::stringstream sstream(sdstring);
	LLSDSerialize::fromNotation(sdres, sstream, sdstring.size());

	S32 width = sdres[0];
	S32 height = sdres[1];
    
    // Note : index 0 of the filter drop down is assumed to be "No filter" in whichever locale 
    std::string filter_name = (filterbox->getCurrentIndex() ? filterbox->getSimple() : "");

	LLSnapshotLivePreview * previewp = static_cast<LLSnapshotLivePreview *>(mPreviewHandle.get());
	if (previewp && combobox->getCurrentIndex() >= 0)
	{
		// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare; moved up
		checkAspectRatio(width);

		S32 original_width = 0 , original_height = 0 ;
		previewp->getSize(original_width, original_height) ;

		if (width == 0 || height == 0)
		{
			// take resolution from current window size
			LL_DEBUGS() << "Setting preview res from window: " << gViewerWindow->getWindowWidthRaw() << "x" << gViewerWindow->getWindowHeightRaw() << LL_ENDL;
			previewp->setSize(gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw());
		}
		// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
		else if (width == -1 || height == -1)
		{
			// take resolution from custom size
			LLSpinCtrl* width_spinner = getChild<LLSpinCtrl>("custom_snapshot_width");
			LLSpinCtrl* height_spinner = getChild<LLSpinCtrl>("custom_snapshot_height");
			S32 custom_width = width_spinner->getValue().asInteger();
			S32 custom_height = height_spinner->getValue().asInteger();
			if (checkImageSize(previewp, custom_width, custom_height, TRUE, previewp->getMaxImageSize()))
			{
				width_spinner->set(custom_width);
				height_spinner->set(custom_height);
			}
			LL_DEBUGS() << "Setting preview res from custom: " << custom_width << "x" << custom_height << LL_ENDL;
			previewp->setSize(custom_width, custom_height);
		}
		// </FS:Ansariel>
		else
		{
			// use the resolution from the selected pre-canned drop-down choice
			LL_DEBUGS() << "Setting preview res selected from combo: " << width << "x" << height << LL_ENDL;
			previewp->setSize(width, height);
		}

		// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare; moved up
		//checkAspectRatio(width);

		previewp->getSize(width, height);
		if ((original_width != width) || (original_height != height))
		{
			previewp->setSize(width, height);
			if (do_update)
			{
                //previewp->updateSnapshot(TRUE);
                previewp->updateSnapshot(TRUE, TRUE);
				updateControls();
			}
		}
        // Get the old filter, compare to the current one "filter_name" and set if changed
        std::string original_filter = previewp->getFilter();
		if (original_filter != filter_name)
		{
            previewp->setFilter(filter_name);
			if (do_update)
			{
                previewp->updateSnapshot(FALSE, TRUE);
				updateControls();
			}
		}
	}

	// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
	BOOL custom_resolution = static_cast<LLComboBox *>(mResolutionComboBox)->getSelectedValue().asString() == "[i-1,i-1]";
	getChild<LLSpinCtrl>("custom_snapshot_width")->setEnabled(custom_resolution);
	getChild<LLSpinCtrl>("custom_snapshot_height")->setEnabled(custom_resolution);
	getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->setEnabled(custom_resolution);
	// </FS:Ansariel>
}

void LLFlickrPhotoPanel::checkAspectRatio(S32 index)
{
	LLSnapshotLivePreview *previewp = getPreviewView() ;

	BOOL keep_aspect = FALSE;

	if (0 == index) // current window size
	{
		keep_aspect = TRUE;
	}
	// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
	else if (-1 == index)
	{
		keep_aspect = getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->get();
	}
	// </FS:Ansariel>
	else // predefined resolution
	{
		keep_aspect = FALSE;
	}

	if (previewp)
	{
		previewp->mKeepAspectRatio = keep_aspect;
	}
}

LLUICtrl* LLFlickrPhotoPanel::getRefreshBtn()
{
	return mRefreshBtn;
}

// <FS:Ansariel> Exodus' flickr upload
void LLFlickrPhotoPanel::onOpen(const LLSD& key)
{
#ifdef OPENSIM
	if (!LLGridManager::getInstance()->isInSecondLife())
	{
		// Reauthorise if necessary.
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_CONNECTION_IN_PROGRESS);
		new exoFlickrAuth(boost::bind(&LLFlickrPhotoPanel::flickrAuthResponse, this, _1, _2));
	}
#endif
}

void LLFlickrPhotoPanel::uploadCallback(bool success, const LLSD& response)
{
	LLSD args;
	if(success && response["stat"].asString() == "ok")
	{
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_POSTED);
		args["ID"] = response["photoid"];
		LLNotificationsUtil::add("ExodusFlickrUploadComplete", args);
	}
	else
	{
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_POST_FAILED);
	}
}

void LLFlickrPhotoPanel::flickrAuthResponse(bool success, const LLSD& response)
{
	if(!success)
	{
		// Complain about failed auth here.
		LL_WARNS("Flickr") << "Flickr authentication failed." << LL_ENDL;
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_CONNECTION_FAILED);
	}
	else
	{
		LLFlickrConnect::instance().setConnectionState(LLFlickrConnect::FLICKR_CONNECTED);
	}
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
BOOL LLFlickrPhotoPanel::checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, BOOL isWidthChanged, S32 max_value)
{
	S32 w = width ;
	S32 h = height ;

	if(previewp && previewp->mKeepAspectRatio)
	{
		if(gViewerWindow->getWindowWidthRaw() < 1 || gViewerWindow->getWindowHeightRaw() < 1)
		{
			return FALSE ;
		}

		//aspect ratio of the current window
		F32 aspect_ratio = (F32)gViewerWindow->getWindowWidthRaw() / gViewerWindow->getWindowHeightRaw() ;

		//change another value proportionally
		if(isWidthChanged)
		{
			height = ll_round(width / aspect_ratio) ;
		}
		else
		{
			width = ll_round(height * aspect_ratio) ;
		}

		//bound w/h by the max_value
		if(width > max_value || height > max_value)
		{
			if(width > height)
			{
				width = max_value ;
				height = (S32)(width / aspect_ratio) ;
			}
			else
			{
				height = max_value ;
				width = (S32)(height * aspect_ratio) ;
			}
		}
	}

	return (w != width || h != height) ;
}
// </FS:Ansariel>

///////////////////////////
//LLFlickrAccountPanel//////
///////////////////////////

LLFlickrAccountPanel::LLFlickrAccountPanel() : 
mAccountCaptionLabel(NULL),
mAccountNameLabel(NULL),
mPanelButtons(NULL),
mConnectButton(NULL),
mDisconnectButton(NULL)
{
	mCommitCallbackRegistrar.add("SocialSharing.Connect", boost::bind(&LLFlickrAccountPanel::onConnect, this));
	mCommitCallbackRegistrar.add("SocialSharing.Disconnect", boost::bind(&LLFlickrAccountPanel::onDisconnect, this));

	setVisibleCallback(boost::bind(&LLFlickrAccountPanel::onVisibilityChange, this, _2));
}

BOOL LLFlickrAccountPanel::postBuild()
{
	mAccountCaptionLabel = getChild<LLTextBox>("account_caption_label");
	mAccountNameLabel = getChild<LLTextBox>("account_name_label");
	mPanelButtons = getChild<LLUICtrl>("panel_buttons");
	mConnectButton = getChild<LLUICtrl>("connect_btn");
	mDisconnectButton = getChild<LLUICtrl>("disconnect_btn");

	return LLPanel::postBuild();
}

void LLFlickrAccountPanel::draw()
{
	LLFlickrConnect::EConnectionState connection_state = LLFlickrConnect::instance().getConnectionState();

	//Disable the 'disconnect' button and the 'use another account' button when disconnecting in progress
	bool disconnecting = connection_state == LLFlickrConnect::FLICKR_DISCONNECTING;
	mDisconnectButton->setEnabled(!disconnecting);

	//Disable the 'connect' button when a connection is in progress
	bool connecting = connection_state == LLFlickrConnect::FLICKR_CONNECTION_IN_PROGRESS;
	mConnectButton->setEnabled(!connecting);

	LLPanel::draw();
}

void LLFlickrAccountPanel::onVisibilityChange(BOOL visible)
{
	if(visible)
	{
		LLEventPumps::instance().obtain("FlickrConnectState").stopListening("LLFlickrAccountPanel");
		LLEventPumps::instance().obtain("FlickrConnectState").listen("LLFlickrAccountPanel", boost::bind(&LLFlickrAccountPanel::onFlickrConnectStateChange, this, _1));

		LLEventPumps::instance().obtain("FlickrConnectInfo").stopListening("LLFlickrAccountPanel");
		LLEventPumps::instance().obtain("FlickrConnectInfo").listen("LLFlickrAccountPanel", boost::bind(&LLFlickrAccountPanel::onFlickrConnectInfoChange, this));

		//Connected
		if(LLFlickrConnect::instance().isConnected())
		{
			showConnectedLayout();
		}
		//Check if connected (show disconnected layout in meantime)
		else
		{
			showDisconnectedLayout();
		}
        if ((LLFlickrConnect::instance().getConnectionState() == LLFlickrConnect::FLICKR_NOT_CONNECTED) ||
            (LLFlickrConnect::instance().getConnectionState() == LLFlickrConnect::FLICKR_CONNECTION_FAILED))
        {
            LLFlickrConnect::instance().checkConnectionToFlickr();
        }
	}
	else
	{
		LLEventPumps::instance().obtain("FlickrConnectState").stopListening("LLFlickrAccountPanel");
		LLEventPumps::instance().obtain("FlickrConnectInfo").stopListening("LLFlickrAccountPanel");
	}
}

bool LLFlickrAccountPanel::onFlickrConnectStateChange(const LLSD& data)
{
	if(LLFlickrConnect::instance().isConnected())
	{
		//In process of disconnecting so leave the layout as is
		if(data.get("enum").asInteger() != LLFlickrConnect::FLICKR_DISCONNECTING)
		{
			showConnectedLayout();
		}
	}
	else
	{
		showDisconnectedLayout();
	}

	return false;
}

bool LLFlickrAccountPanel::onFlickrConnectInfoChange()
{
	LLSD info = LLFlickrConnect::instance().getInfo();
	std::string clickable_name;

	//Strings of format [http://www.somewebsite.com Click Me] become clickable text
	if(info.has("link") && info.has("name"))
	{
		clickable_name = "[" + info["link"].asString() + " " + info["name"].asString() + "]";
	}

	mAccountNameLabel->setText(clickable_name);

	return false;
}

void LLFlickrAccountPanel::showConnectButton()
{
	if(!mConnectButton->getVisible())
	{
		mConnectButton->setVisible(TRUE);
		mDisconnectButton->setVisible(FALSE);
	}
}

void LLFlickrAccountPanel::hideConnectButton()
{
	if(mConnectButton->getVisible())
	{
		mConnectButton->setVisible(FALSE);
		mDisconnectButton->setVisible(TRUE);
	}
}

void LLFlickrAccountPanel::showDisconnectedLayout()
{
	mAccountCaptionLabel->setText(getString("flickr_disconnected"));
	mAccountNameLabel->setText(std::string(""));
	showConnectButton();
}

void LLFlickrAccountPanel::showConnectedLayout()
{
	LLFlickrConnect::instance().loadFlickrInfo();

	mAccountCaptionLabel->setText(getString("flickr_connected"));
	hideConnectButton();
}

void LLFlickrAccountPanel::onConnect()
{
	LLFlickrConnect::instance().checkConnectionToFlickr(true);

	//Clear only the flickr browser cookies so that the flickr login screen appears
	LLViewerMedia::getCookieStore()->removeCookiesByDomain(".flickr.com"); 
}

void LLFlickrAccountPanel::onDisconnect()
{
	LLFlickrConnect::instance().disconnectFromFlickr();

	LLViewerMedia::getCookieStore()->removeCookiesByDomain(".flickr.com"); 
}

////////////////////////
//LLFloaterFlickr///////
////////////////////////

LLFloaterFlickr::LLFloaterFlickr(const LLSD& key) : LLFloater(key),
    mFlickrPhotoPanel(NULL),
    mStatusErrorText(NULL),
    mStatusLoadingText(NULL),
    mStatusLoadingIndicator(NULL)
{
	mCommitCallbackRegistrar.add("SocialSharing.Cancel", boost::bind(&LLFloaterFlickr::onCancel, this));
}

void LLFloaterFlickr::onClose(bool app_quitting)
{
    LLFloaterBigPreview* big_preview_floater = dynamic_cast<LLFloaterBigPreview*>(LLFloaterReg::getInstance("big_preview"));
    if (big_preview_floater)
    {
        big_preview_floater->closeOnFloaterOwnerClosing(this);
    }
	LLFloater::onClose(app_quitting);
}

void LLFloaterFlickr::onCancel()
{
    LLFloaterBigPreview* big_preview_floater = dynamic_cast<LLFloaterBigPreview*>(LLFloaterReg::getInstance("big_preview"));
    if (big_preview_floater)
    {
        big_preview_floater->closeOnFloaterOwnerClosing(this);
    }
    closeFloater();
}

BOOL LLFloaterFlickr::postBuild()
{
    // Keep tab of the Photo Panel
	mFlickrPhotoPanel = static_cast<LLFlickrPhotoPanel*>(getChild<LLUICtrl>("panel_flickr_photo"));
    // Connection status widgets
    mStatusErrorText = getChild<LLTextBox>("connection_error_text");
    mStatusLoadingText = getChild<LLTextBox>("connection_loading_text");
    mStatusLoadingIndicator = getChild<LLUICtrl>("connection_loading_indicator");

// <FS:Ansariel> Exodus' flickr upload
#ifdef OPENSIM
	if (!LLGridManager::getInstance()->isInSecondLife())
	{
		getChild<LLTabContainer>("tabs")->removeTabPanel(getChild<LLPanel>("panel_flickr_account"));
	}
#endif
// </FS:Ansariel>

	return LLFloater::postBuild();
}

void LLFloaterFlickr::showPhotoPanel()
{
	LLTabContainer* parent = dynamic_cast<LLTabContainer*>(mFlickrPhotoPanel->getParent());
	if (!parent)
	{
		LL_WARNS() << "Cannot find panel container" << LL_ENDL;
		return;
	}

	parent->selectTabPanel(mFlickrPhotoPanel);
}

void LLFloaterFlickr::draw()
{
    if (mStatusErrorText && mStatusLoadingText && mStatusLoadingIndicator)
    {
        mStatusErrorText->setVisible(false);
        mStatusLoadingText->setVisible(false);
        mStatusLoadingIndicator->setVisible(false);
        LLFlickrConnect::EConnectionState connection_state = LLFlickrConnect::instance().getConnectionState();
        std::string status_text;
        
        switch (connection_state)
        {
        case LLFlickrConnect::FLICKR_NOT_CONNECTED:
            // No status displayed when first opening the panel and no connection done
        case LLFlickrConnect::FLICKR_CONNECTED:
            // When successfully connected, no message is displayed
        case LLFlickrConnect::FLICKR_POSTED:
            // No success message to show since we actually close the floater after successful posting completion
            break;
        case LLFlickrConnect::FLICKR_CONNECTION_IN_PROGRESS:
            // Connection loading indicator
            mStatusLoadingText->setVisible(true);
            status_text = LLTrans::getString("SocialFlickrConnecting");
            mStatusLoadingText->setValue(status_text);
            mStatusLoadingIndicator->setVisible(true);
            break;
        case LLFlickrConnect::FLICKR_POSTING:
            // Posting indicator
            mStatusLoadingText->setVisible(true);
            status_text = LLTrans::getString("SocialFlickrPosting");
            mStatusLoadingText->setValue(status_text);
            mStatusLoadingIndicator->setVisible(true);
			break;
        case LLFlickrConnect::FLICKR_CONNECTION_FAILED:
            // Error connecting to the service
            mStatusErrorText->setVisible(true);
            status_text = LLTrans::getString("SocialFlickrErrorConnecting");
            mStatusErrorText->setValue(status_text);
            break;
        case LLFlickrConnect::FLICKR_POST_FAILED:
            // Error posting to the service
            mStatusErrorText->setVisible(true);
            status_text = LLTrans::getString("SocialFlickrErrorPosting");
            mStatusErrorText->setValue(status_text);
            break;
		case LLFlickrConnect::FLICKR_DISCONNECTING:
			// Disconnecting loading indicator
			mStatusLoadingText->setVisible(true);
			status_text = LLTrans::getString("SocialFlickrDisconnecting");
			mStatusLoadingText->setValue(status_text);
			mStatusLoadingIndicator->setVisible(true);
			break;
		case LLFlickrConnect::FLICKR_DISCONNECT_FAILED:
			// Error disconnecting from the service
			mStatusErrorText->setVisible(true);
			status_text = LLTrans::getString("SocialFlickrErrorDisconnecting");
			mStatusErrorText->setValue(status_text);
			break;
        }
    }
	LLFloater::draw();
}

// <FS:Ansariel> Exodus' flickr upload
void LLFloaterFlickr::onOpen(const LLSD& key)
{
	mFlickrPhotoPanel->onOpen(key);
}
// </FS:Ansariel>

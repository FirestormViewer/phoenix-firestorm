/**
 * @file fsfloaterprimfeed.cpp
 * @brief Implementation of primfeed floater
 * @author beq@firestorm
 *
 * $LicenseInfo:firstyear=2025&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2025, Beq Janus
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterprimfeed.h"
#include "fsprimfeedconnect.h"
#include "llagent.h"
#include "llagentui.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llfloaterreg.h"
#include "lliconctrl.h"
#include "llimagefiltersmanager.h"
#include "llresmgr.h" // LLLocale
#include "llsdserialize.h"
#include "llloadingindicator.h"
#include "llslurl.h"
#include "lltrans.h"
#include "llfloatersnapshot.h"
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
#include "fsprimfeedauth.h"
#include "llviewernetwork.h"

static LLPanelInjector<FSPrimfeedPhotoPanel>   t_panel_photo("fsprimfeedphotopanel");
static LLPanelInjector<FSPrimfeedAccountPanel> t_panel_account("fsprimfeedaccountpanel");

///////////////////////////
// FSPrimfeedPhotoPanel/////
///////////////////////////

FSPrimfeedPhotoPanel::FSPrimfeedPhotoPanel() :
    mResolutionComboBox(nullptr),
    mRefreshBtn(nullptr),
    mWorkingLabel(nullptr),
    mThumbnailPlaceholder(nullptr),
    mDescriptionTextBox(nullptr),
    mLocationCheckbox(nullptr),
    mRatingComboBox(nullptr),
    mStoresComboBox(nullptr),
    mPostButton(nullptr),
    mBtnPreview(nullptr),
    mBigPreviewFloater(nullptr),
    mCancelButton(nullptr),
    mCommercialCheckbox(nullptr),
    mFilterComboBox(nullptr),
    mPublicGalleryCheckbox(nullptr)
{
    mCommitCallbackRegistrar.add("SocialSharing.SendPhoto", [this](LLUICtrl*, const LLSD&) { onSend(); });
    mCommitCallbackRegistrar.add("SocialSharing.RefreshPhoto", [this](LLUICtrl*, const LLSD&) { onClickNewSnapshot(); });
    mCommitCallbackRegistrar.add("SocialSharing.BigPreview", [this](LLUICtrl*, const LLSD&) { onClickBigPreview(); });
    mCommitCallbackRegistrar.add("Primfeed.Info",
                                 [](LLUICtrl*, const LLSD& param)
                                 {
                                     const std::string url = param.asString();
                                     LL_DEBUGS("primfeed") << "Info button clicked, opening " << url << LL_ENDL;
                                     LLWeb::loadURLExternal(url);
                                 });
    FSPrimfeedAuth::sPrimfeedAuthPump->listen("FSPrimfeedPhotoPanel",
                                              [this](const LLSD& data)
                                              {
                                                  if (data["responseType"].asString() == "primfeed_user_info")
                                                  {
                                                      this->loadPrimfeedInfo(data);
                                                      return true;
                                                  }
                                                  return false;
                                              });
}

FSPrimfeedPhotoPanel::~FSPrimfeedPhotoPanel()
{
    if (mPreviewHandle.get())
    {
        mPreviewHandle.get()->die();
    }

    gSavedSettings.setS32("FSLastSnapshotToPrimfeedResolution", getChild<LLComboBox>("resolution_combobox")->getCurrentIndex());
    gSavedSettings.setS32("FSLastSnapshotToPrimfeedWidth", getChild<LLSpinCtrl>("custom_snapshot_width")->getValue().asInteger());
    gSavedSettings.setS32("FSLastSnapshotToPrimfeedHeight", getChild<LLSpinCtrl>("custom_snapshot_height")->getValue().asInteger());
}

bool FSPrimfeedPhotoPanel::postBuild()
{
    setVisibleCallback([this](LLUICtrl*, bool visible) { onVisibilityChange(visible); });

    mResolutionComboBox = getChild<LLUICtrl>("resolution_combobox");
    mResolutionComboBox->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateResolution(true); });
    mFilterComboBox = getChild<LLUICtrl>("filters_combobox");
    mFilterComboBox->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateResolution(true); });
    mRefreshBtn            = getChild<LLUICtrl>("new_snapshot_btn");
    mBtnPreview            = getChild<LLButton>("big_preview_btn");
    mWorkingLabel          = getChild<LLUICtrl>("working_lbl");
    mThumbnailPlaceholder  = getChild<LLUICtrl>("thumbnail_placeholder");
    mDescriptionTextBox    = getChild<LLUICtrl>("photo_description");
    mLocationCheckbox      = getChild<LLUICtrl>("add_location_cb");
    mCommercialCheckbox    = getChild<LLUICtrl>("primfeed_commercial_content");
    mPublicGalleryCheckbox = getChild<LLUICtrl>("primfeed_add_to_public_gallery");
    mRatingComboBox        = getChild<LLUICtrl>("rating_combobox");
    mStoresComboBox        = getChild<LLComboBox>("stores_combobox");
    mPostButton            = getChild<LLUICtrl>("post_photo_btn");
    mCancelButton          = getChild<LLUICtrl>("cancel_photo_btn");
    mBigPreviewFloater     = dynamic_cast<LLFloaterBigPreview*>(LLFloaterReg::getInstance("big_preview"));

    // Update custom resolution controls with lambdas
    getChild<LLSpinCtrl>("custom_snapshot_width")->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateResolution(true); });
    getChild<LLSpinCtrl>("custom_snapshot_height")->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateResolution(true); });
    getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateResolution(true); });

    getChild<LLComboBox>("resolution_combobox")->setCurrentByIndex(gSavedSettings.getS32("FSLastSnapshotToPrimfeedResolution"));
    getChild<LLSpinCtrl>("custom_snapshot_width")->setValue(gSavedSettings.getS32("FSLastSnapshotToPrimfeedWidth"));
    getChild<LLSpinCtrl>("custom_snapshot_height")->setValue(gSavedSettings.getS32("FSLastSnapshotToPrimfeedHeight"));

    // Update filter list
    std::vector<std::string> filter_list = LLImageFiltersManager::getInstance()->getFiltersList();
    auto*                    filterbox   = static_cast<LLComboBox*>(mFilterComboBox);
    for (const std::string& filter : filter_list)
    {
        filterbox->add(filter);
    }

    return LLPanel::postBuild();
}

// static
void FSFloaterPrimfeed::update()
{
    if (LLFloaterReg::instanceVisible("primfeed"))
    {
        LLFloaterSnapshotBase::ImplBase::updatePreviewList(true, true);
    }
}

// virtual
S32 FSPrimfeedPhotoPanel::notify(const LLSD& info)
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

        if (LLUICtrl* refresh_button = getRefreshBtn(); !refresh_button->getVisible())
        {
            refresh_button->setVisible(true);
        }
        return 1;
    }

    return 0;
}

void FSPrimfeedPhotoPanel::draw()
{
    auto previewp = static_cast<const LLSnapshotLivePreview*>(mPreviewHandle.get());

    // Enable interaction only if no transaction with the service is on-going (prevent duplicated posts)
    auto can_post = !(FSPrimfeedConnect::instance().isTransactionOngoing()) && FSPrimfeedAuth::isAuthorized();

    mCancelButton->setEnabled(can_post);
    mDescriptionTextBox->setEnabled(can_post);
    mRatingComboBox->setEnabled(can_post);
    // If the stores combo box is present, enable it only if we have stores to select from
    mStoresComboBox->setEnabled(can_post && mStoresComboBox->getItemCount() > 1);
    mResolutionComboBox->setEnabled(can_post);
    mFilterComboBox->setEnabled(can_post);
    mRefreshBtn->setEnabled(can_post);
    mBtnPreview->setEnabled(can_post);
    mLocationCheckbox->setEnabled(can_post);
    mPublicGalleryCheckbox->setEnabled(can_post);
    mCommercialCheckbox->setEnabled(can_post);

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
        const S32     thumbnail_w    = previewp->getThumbnailWidth();
        const S32     thumbnail_h    = previewp->getThumbnailHeight();

        // calc preview offset within the preview rect
        const S32 local_offset_x = (thumbnail_rect.getWidth() - thumbnail_w) / 2;
        const S32 local_offset_y = (thumbnail_rect.getHeight() - thumbnail_h) / 2;
        S32       offset_x       = thumbnail_rect.mLeft + local_offset_x;
        S32       offset_y       = thumbnail_rect.mBottom + local_offset_y;

        gGL.matrixMode(LLRender::MM_MODELVIEW);
        // Apply floater transparency to the texture unless the floater is focused.
        F32      alpha = getTransparencyType() == TT_ACTIVE ? 1.0f : getCurrentTransparency();
        LLColor4 color = LLColor4::white;
        gl_draw_scaled_image(offset_x, offset_y, thumbnail_w, thumbnail_h, previewp->getThumbnailImage(), color % alpha);
    }

    // Update the visibility of the working (computing preview) label
    mWorkingLabel->setVisible(!(previewp && previewp->getSnapshotUpToDate()));

    // Enable Post if we have a preview to send and no on going connection being processed
    mPostButton->setEnabled(can_post && (previewp && previewp->getSnapshotUpToDate()));

    // Draw the rest of the panel on top of it
    LLPanel::draw();
}

LLSnapshotLivePreview* FSPrimfeedPhotoPanel::getPreviewView()
{
    auto previewp = (LLSnapshotLivePreview*)mPreviewHandle.get();
    return previewp;
}

void FSPrimfeedPhotoPanel::onVisibilityChange(bool visible)
{
    if (visible)
    {
        if (mPreviewHandle.get())
        {
            LLSnapshotLivePreview* preview = getPreviewView();
            if (preview)
            {
                LL_DEBUGS() << "opened, updating snapshot" << LL_ENDL;
                preview->updateSnapshot(true);
            }
        }
        else
        {
            LLRect                        full_screen_rect = getRootView()->getRect();
            LLSnapshotLivePreview::Params p;
            p.rect(full_screen_rect);
            auto previewp  = new LLSnapshotLivePreview(p);
            mPreviewHandle = previewp->getHandle();

            previewp->setContainer(this);
            previewp->setSnapshotType(LLSnapshotModel::SNAPSHOT_WEB);
            previewp->setSnapshotFormat(LLSnapshotModel::SNAPSHOT_FORMAT_PNG);
            previewp->setThumbnailSubsampled(true);     // We want the preview to reflect the *saved* image
            previewp->setAllowRenderUI(false);          // We do not want the rendered UI in our snapshots
            previewp->setAllowFullScreenPreview(false); // No full screen preview in SL Share mode
            previewp->setThumbnailPlaceholderRect(mThumbnailPlaceholder->getRect());

            updateControls();
        }
    }
}

void FSPrimfeedPhotoPanel::onClickNewSnapshot()
{
    LLSnapshotLivePreview* previewp = getPreviewView();
    if (previewp)
    {
        previewp->updateSnapshot(true);
    }
}

void FSPrimfeedPhotoPanel::onClickBigPreview()
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

bool FSPrimfeedPhotoPanel::isPreviewVisible() const
{
    return (mBigPreviewFloater && mBigPreviewFloater->getVisible());
}

void FSPrimfeedPhotoPanel::attachPreview()
{
    if (mBigPreviewFloater)
    {
        LLSnapshotLivePreview* previewp = getPreviewView();
        mBigPreviewFloater->setPreview(previewp);
        mBigPreviewFloater->setFloaterOwner(getParentByType<LLFloater>());
    }
}

void FSPrimfeedPhotoPanel::onSend()
{
    sendPhoto();
}

bool FSPrimfeedPhotoPanel::onPrimfeedConnectStateChange(const LLSD& /*data*/)
{
    if (FSPrimfeedAuth::isAuthorized())
    {
        return true;
    }

    return false;
}

void FSPrimfeedPhotoPanel::sendPhoto()
{
    auto ratingToString = [&](int rating)
    {
        static const std::array<std::string, 4> RATING_NAMES = {
            "general",   // 1
            "moderate",  // 2
            "adult",     // 3
            "adult_plus" // 4
        };

        // clamp into [1,4]
        int idx = llclamp(rating, 1, 4) - 1;
        return RATING_NAMES[idx];
    };
    // Get the description (primfeed has no title/tags etc at this point)
    std::string description = mDescriptionTextBox->getValue().asString();

    // Get the content rating
    int         content_rating         = mRatingComboBox->getValue().asInteger();
    bool        post_to_public_gallery = mPublicGalleryCheckbox->getValue().asBoolean();
    bool        commercial_content     = mCommercialCheckbox->getValue().asBoolean();
    std::string store_id               = mStoresComboBox->getValue().asString();
    // Get the image
    LLSnapshotLivePreview* previewp = getPreviewView();

    FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_POSTING);
    LLSD params;
    params["rating"]                 = ratingToString(content_rating);
    params["content"]                = description;
    params["is_commercial"]          = commercial_content;
    params["post_to_public_gallery"] = post_to_public_gallery;
    // Add the location if required

    if (bool add_location = mLocationCheckbox->getValue().asBoolean(); add_location)
    {
        // Get the SLURL for the location
        LLSLURL slurl;
        LLAgentUI::buildSLURL(slurl);
        std::string slurl_string = slurl.getSLURLString();

        params["location"] = slurl_string;
    }
    if (!store_id.empty())
    {
        // Add the store ID if we have one selected
        params["store_id"] = store_id;
    }

    FSPrimfeedConnect::instance().uploadPhoto(params, previewp->getFormattedImage().get(),
                                              [this](bool success, const std::string& url)
                                              {
                                                  if (success)
                                                  {
                                                      FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_POSTED);
                                                      static LLCachedControl<bool> open_url_on_post(gSavedPerAccountSettings,
                                                                                                    "FSPrimfeedOpenURLOnPost", true);
                                                      if (open_url_on_post)
                                                      {
                                                          LLWeb::loadURLExternal(url);
                                                      }
                                                      LLSD args;
                                                      args["PF_POSTURL"] = url;
                                                      LLNotificationsUtil::add("FSPrimfeedUploadComplete", args);
                                                  }
                                                  else
                                                  {
                                                      mWorkingLabel->setValue("Error posting to Primfeed");
                                                      mPostButton->setEnabled(true);
                                                  }
                                              });
    updateControls();
}

void FSPrimfeedPhotoPanel::clearAndClose()
{
    mDescriptionTextBox->setValue("");

    if (LLFloater* floater = getParentByType<LLFloater>())
    {
        floater->closeFloater();
        if (mBigPreviewFloater)
        {
            mBigPreviewFloater->closeOnFloaterOwnerClosing(floater);
        }
    }
}

void FSPrimfeedPhotoPanel::updateControls()
{
    // LLSnapshotLivePreview* previewp = getPreviewView();
    updateResolution(false);
}

void FSPrimfeedPhotoPanel::updateResolution(bool do_update)
{
    auto combobox  = static_cast<LLComboBox*>(mResolutionComboBox);
    auto filterbox = static_cast<LLComboBox*>(mFilterComboBox);

    std::string       sdstring = combobox->getSelectedValue();
    LLSD              sdres;
    std::stringstream sstream(sdstring);
    LLSDSerialize::fromNotation(sdres, sstream, sdstring.size());

    S32 width  = sdres[0];
    S32 height = sdres[1];

    // Note : index 0 of the filter drop down is assumed to be "No filter" in whichever locale
    std::string filter_name = (filterbox->getCurrentIndex() ? filterbox->getSimple() : "");

    if (auto previewp = static_cast<LLSnapshotLivePreview*>(mPreviewHandle.get()); previewp && combobox->getCurrentIndex() >= 0)
    {
        checkAspectRatio(width);

        S32 original_width  = 0;
        S32 original_height = 0;
        previewp->getSize(original_width, original_height);

        if (width == 0 || height == 0)
        {
            // take resolution from current window size
            LL_DEBUGS() << "Setting preview res from window: " << gViewerWindow->getWindowWidthRaw() << "x"
                        << gViewerWindow->getWindowHeightRaw() << LL_ENDL;
            previewp->setSize(gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw());
        }
        else if (width == -1 || height == -1)
        {
            // take resolution from custom size
            LLSpinCtrl* width_spinner  = getChild<LLSpinCtrl>("custom_snapshot_width");
            LLSpinCtrl* height_spinner = getChild<LLSpinCtrl>("custom_snapshot_height");
            S32         custom_width   = width_spinner->getValue().asInteger();
            S32         custom_height  = height_spinner->getValue().asInteger();
            if (checkImageSize(previewp, custom_width, custom_height, true, previewp->getMaxImageSize()))
            {
                width_spinner->set((F32)custom_width);
                height_spinner->set((F32)custom_height);
            }
            LL_DEBUGS() << "Setting preview res from custom: " << custom_width << "x" << custom_height << LL_ENDL;
            previewp->setSize(custom_width, custom_height);
        }
        else
        {
            // use the resolution from the selected pre-canned drop-down choice
            LL_DEBUGS() << "Setting preview res selected from combo: " << width << "x" << height << LL_ENDL;
            previewp->setSize(width, height);
        }

        previewp->getSize(width, height);
        if ((original_width != width) || (original_height != height))
        {
            previewp->setSize(width, height);
            if (do_update)
            {
                previewp->updateSnapshot(true, true);
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
                previewp->updateSnapshot(false, true);
                updateControls();
            }
        }
    }

    bool custom_resolution = static_cast<LLComboBox*>(mResolutionComboBox)->getSelectedValue().asString() == "[i-1,i-1]";
    getChild<LLSpinCtrl>("custom_snapshot_width")->setEnabled(custom_resolution);
    getChild<LLSpinCtrl>("custom_snapshot_height")->setEnabled(custom_resolution);
    getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->setEnabled(custom_resolution);
}

void FSPrimfeedPhotoPanel::checkAspectRatio(S32 index)
{
    LLSnapshotLivePreview* previewp = getPreviewView();

    bool keep_aspect = false;

    if (0 == index) // current window size
    {
        keep_aspect = true;
    }
    else if (-1 == index)
    {
        keep_aspect = getChild<LLCheckBoxCtrl>("keep_aspect_ratio")->get();
    }
    else // predefined resolution
    {
        keep_aspect = false;
    }

    if (previewp)
    {
        previewp->mKeepAspectRatio = keep_aspect;
    }
}

void FSPrimfeedPhotoPanel::loadPrimfeedInfo(const LLSD& data)
{
    if (!mStoresComboBox)
        return;

    // Clear any existing entries
    mStoresComboBox->clearRows();
    mStoresComboBox->add(LLTrans::getString("Personal"), LLSD(""));

    // Read the saved stores array
    const LLSD& stores = data["stores"];
    if (!stores.isArray() || stores.size() == 0)
    {
        // No stores - disable the combobox
        mStoresComboBox->setEnabled(false);
        return;
    }

    mStoresComboBox->setEnabled(true);
    for (S32 i = 0; i < stores.size(); ++i)
    {
        const LLSD& store = stores[i];
        std::string id    = store["id"].asString();
        std::string name  = store["name"].asString();
        mStoresComboBox->add(name, LLSD(id));
    }
    mStoresComboBox->setCurrentByIndex(0);
}

LLUICtrl* FSPrimfeedPhotoPanel::getRefreshBtn()
{
    return mRefreshBtn;
}

void FSPrimfeedPhotoPanel::onOpen(const LLSD& key)
{
    // Reauthorise if necessary.
    FSPrimfeedAuth::initiateAuthRequest();
    LLSD dummy;
    onPrimfeedConnectStateChange(dummy);
}

void FSPrimfeedPhotoPanel::uploadCallback(bool success, const LLSD& response)
{
    LLSD args;
    if (success && response["stat"].asString() == "ok")
    {
        FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_POSTED);
        args["PF_POSTURL"] = response["postUrl"];
        LLNotificationsUtil::add("FSPrimfeedUploadComplete", args);
    }
    else
    {
        FSPrimfeedConnect::instance().setConnectionState(FSPrimfeedConnect::PRIMFEED_POST_FAILED);
    }
}

void FSPrimfeedPhotoPanel::primfeedAuthResponse(bool success, const LLSD& response)
{
    if (!success)
    {
        if (response.has("status") && response["status"].asString() == "reset")
        {
            LL_INFOS("Primfeed") << "Primfeed authorization has been reset." << LL_ENDL;
        }
        else
        {
            // Complain about failed auth here.
            LL_WARNS("Primfeed") << "Primfeed authentication failed." << LL_ENDL;
        }
    }
    onPrimfeedConnectStateChange(response);
}

bool FSPrimfeedPhotoPanel::checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, bool isWidthChanged, S32 max_value)
{
    S32 w = width;
    S32 h = height;

    if (previewp && previewp->mKeepAspectRatio)
    {
        if (gViewerWindow->getWindowWidthRaw() < 1 || gViewerWindow->getWindowHeightRaw() < 1)
        {
            return false;
        }

        // aspect ratio of the current window
        F32 aspect_ratio = static_cast<F32>(gViewerWindow->getWindowWidthRaw()) / static_cast<F32>(gViewerWindow->getWindowHeightRaw());

        // change another value proportionally
        if (isWidthChanged)
        {
            height = ll_round(static_cast<F32>(width) / aspect_ratio);
        }
        else
        {
            width = ll_round(static_cast<F32>(height) * aspect_ratio);
        }

        // bound w/h by the max_value
        if (width > max_value || height > max_value)
        {
            if (width > height)
            {
                width  = max_value;
                height = ll_round(static_cast<F32>(width) / aspect_ratio);
            }
            else
            {
                height = max_value;
                width  = ll_round(static_cast<F32>(height) * aspect_ratio);
            }
        }
    }

    return (w != width || h != height);
}

///////////////////////////
// FSPrimfeedAccountPanel///
///////////////////////////

FSPrimfeedAccountPanel::FSPrimfeedAccountPanel() :
    mAccountConnectedAsLabel(nullptr),
    mAccountNameLink(nullptr),
    mAccountPlan(nullptr),
    mPanelButtons(nullptr),
    mConnectButton(nullptr),
    mDisconnectButton(nullptr)
{
    mCommitCallbackRegistrar.add("SocialSharing.Connect", [this](LLUICtrl*, const LLSD&) { onConnect(); });
    mCommitCallbackRegistrar.add("SocialSharing.Disconnect", [this](LLUICtrl*, const LLSD&) { onDisconnect(); });

    FSPrimfeedAuth::sPrimfeedAuthPump->listen("FSPrimfeedAccountPanel",
                                              [this](const LLSD& data)
                                              {
                                                  if (data.has("responseType"))
                                                  {
                                                      auto response_type = data["responseType"].asString();
                                                      if (response_type == "primfeed_auth_response")
                                                      {
                                                          bool success = data["success"].asBoolean();
                                                          primfeedAuthResponse(success, data);
                                                          return true;
                                                      }
                                                      if (response_type == "primfeed_auth_reset")
                                                      {
                                                          bool success = data["success"].asBoolean();
                                                          primfeedAuthResponse(success, data);
                                                          return true;
                                                      }
                                                  }
                                                  return false;
                                              });

    setVisibleCallback([this](LLUICtrl*, bool visible) { onVisibilityChange(visible); });
}

bool FSPrimfeedAccountPanel::postBuild()
{
    mAccountConnectedAsLabel = getChild<LLTextBox>("connected_as_label");
    mAccountNameLink         = getChild<LLTextBox>("primfeed_account_name");
    mAccountPlan             = getChild<LLTextBox>("primfeed_account_plan");
    mPanelButtons            = getChild<LLUICtrl>("panel_buttons");
    mConnectButton           = getChild<LLUICtrl>("connect_btn");
    mDisconnectButton        = getChild<LLUICtrl>("disconnect_btn");

    LLSD dummy;
    onPrimfeedConnectStateChange(dummy);
    return LLPanel::postBuild();
}

void FSPrimfeedAccountPanel::draw()
{
    FSPrimfeedConnect::EConnectionState        connection_state = FSPrimfeedConnect::instance().getConnectionState();
    static FSPrimfeedConnect::EConnectionState last_state       = FSPrimfeedConnect::PRIMFEED_DISCONNECTED;

    // Update the connection state if it has changed
    if (connection_state != last_state)
    {
        onPrimfeedConnectStateChange(LLSD());
        last_state = connection_state;
    }

    LLPanel::draw();
}

void FSPrimfeedAccountPanel::primfeedAuthResponse(bool success, const LLSD& response)
{
    if (!success)
    {
        LL_WARNS("Primfeed") << "Primfeed authentication failed." << LL_ENDL;
        LLWeb::loadURLExternal("https://www.primfeed.com/login");
    }
    onPrimfeedConnectStateChange(response);
}

void FSPrimfeedAccountPanel::onVisibilityChange(bool visible)
{
    if (visible)
    {
        // Connected
        if (FSPrimfeedAuth::isAuthorized())
        {
            showConnectedLayout();
        }
        else
        {
            showDisconnectedLayout();
        }
    }
}

bool FSPrimfeedAccountPanel::onPrimfeedConnectStateChange(const LLSD&)
{
    if (FSPrimfeedAuth::isAuthorized() || FSPrimfeedConnect::instance().getConnectionState() == FSPrimfeedConnect::PRIMFEED_CONNECTING)
    {
        showConnectedLayout();
    }
    else
    {
        showDisconnectedLayout();
    }
    onPrimfeedConnectInfoChange();
    return false;
}

bool FSPrimfeedAccountPanel::onPrimfeedConnectInfoChange()
{
    std::string clickable_name{ "" };

    static LLCachedControl<std::string> primfeed_username(gSavedPerAccountSettings, "FSPrimfeedUsername");
    static LLCachedControl<std::string> primfeed_profile_link(gSavedPerAccountSettings, "FSPrimfeedProfileLink");
    static LLCachedControl<std::string> primfeed_plan(gSavedPerAccountSettings, "FSPrimfeedPlan");

    // Strings of format [http://www.somewebsite.com Click Me] become clickable text
    if (!primfeed_username().empty())
    {
        clickable_name = std::string("[") + std::string(primfeed_profile_link) + " " + std::string(primfeed_username) + "]";
    }

    mAccountNameLink->setText(clickable_name);
    mAccountPlan->setText(primfeed_plan());

    return false;
}

void FSPrimfeedAccountPanel::showConnectButton()
{
    if (!mConnectButton->getVisible())
    {
        mConnectButton->setVisible(true);
        mDisconnectButton->setVisible(false);
    }
}

void FSPrimfeedAccountPanel::hideConnectButton()
{
    if (mConnectButton->getVisible())
    {
        mConnectButton->setVisible(false);
        mDisconnectButton->setVisible(true);
    }
}

void FSPrimfeedAccountPanel::showDisconnectedLayout()
{
    mAccountConnectedAsLabel->setText(getString("primfeed_disconnected"));
    mAccountNameLink->setText(std::string(""));
    mAccountPlan->setText(getString("primfeed_plan_unknown"));
    showConnectButton();
}

void FSPrimfeedAccountPanel::showConnectedLayout()
{
    mAccountConnectedAsLabel->setText(getString("primfeed_connected"));
    hideConnectButton();
}

void FSPrimfeedAccountPanel::onConnect()
{
    FSPrimfeedAuth::initiateAuthRequest();
    LLSD dummy;
    onPrimfeedConnectStateChange(dummy);
}

void FSPrimfeedAccountPanel::onDisconnect()
{
    FSPrimfeedAuth::resetAuthStatus();
    LLSD dummy;
    onPrimfeedConnectStateChange(dummy);
}

FSPrimfeedAccountPanel::~FSPrimfeedAccountPanel()
{
    // Disconnect the primfeed auth pump
    FSPrimfeedAuth::sPrimfeedAuthPump->stopListening("FSPrimfeedAccountPanel");
}

////////////////////////
// FSFloaterPrimfeed/////
////////////////////////

FSFloaterPrimfeed::FSFloaterPrimfeed(const LLSD& key) :
    LLFloater(key),
    mPrimfeedPhotoPanel(nullptr),
    mPrimfeedAccountPanel(nullptr),
    mStatusErrorText(nullptr),
    mStatusLoadingText(nullptr),
    mStatusLoadingIndicator(nullptr)
{
    mCommitCallbackRegistrar.add("SocialSharing.Cancel", [this](LLUICtrl*, const LLSD&) { onCancel(); });
}

void FSFloaterPrimfeed::onClose(bool app_quitting)
{
    if (auto big_preview_floater = LLFloaterReg::getTypedInstance<LLFloaterBigPreview>("big_preview"))
    {
        big_preview_floater->closeOnFloaterOwnerClosing(this);
    }
    LLFloater::onClose(app_quitting);
}

void FSFloaterPrimfeed::onCancel()
{
    if (auto big_preview_floater = LLFloaterReg::getTypedInstance<LLFloaterBigPreview>("big_preview"))
    {
        big_preview_floater->closeOnFloaterOwnerClosing(this);
    }
    closeFloater();
}

bool FSFloaterPrimfeed::postBuild()
{
    // Keep tab of the Photo Panel
    mPrimfeedPhotoPanel   = static_cast<FSPrimfeedPhotoPanel*>(getChild<LLUICtrl>("panel_primfeed_photo"));
    mPrimfeedAccountPanel = static_cast<FSPrimfeedAccountPanel*>(getChild<LLUICtrl>("panel_primfeed_account"));
    // Connection status widgets
    mStatusErrorText        = getChild<LLTextBox>("connection_error_text");
    mStatusLoadingText      = getChild<LLTextBox>("connection_loading_text");
    mStatusLoadingIndicator = getChild<LLUICtrl>("connection_loading_indicator");

    return LLFloater::postBuild();
}

void FSFloaterPrimfeed::showPhotoPanel()
{
    auto parent = dynamic_cast<LLTabContainer*>(mPrimfeedPhotoPanel->getParent());
    if (!parent)
    {
        LL_WARNS() << "Cannot find panel container" << LL_ENDL;
        return;
    }

    parent->selectTabPanel(mPrimfeedPhotoPanel);
}

void FSFloaterPrimfeed::draw()
{
    if (mStatusErrorText && mStatusLoadingText && mStatusLoadingIndicator)
    {
        mStatusErrorText->setVisible(false);
        mStatusLoadingText->setVisible(false);
        mStatusLoadingIndicator->setVisible(false);

        FSPrimfeedConnect::EConnectionState connection_state = FSPrimfeedConnect::instance().getConnectionState();
        std::string                         status_text;

        if (FSPrimfeedAuth::isAuthorized())
        {
            switch (connection_state)
            {
                case FSPrimfeedConnect::PRIMFEED_POSTING:
                {
                    // Posting indicator
                    mStatusLoadingText->setVisible(true);
                    status_text = LLTrans::getString("SocialPrimfeedPosting");
                    mStatusLoadingText->setValue(status_text);
                    mStatusLoadingIndicator->setVisible(true);
                    break;
                }
                case FSPrimfeedConnect::PRIMFEED_POST_FAILED:
                {
                    // Error posting to the service
                    mStatusErrorText->setVisible(true);
                    status_text = LLTrans::getString("SocialPrimfeedErrorPosting");
                    mStatusErrorText->setValue(status_text);
                    break;
                }
                default:
                {
                    // LL_WARNS("Prmfeed") << "unexpected state" << connection_state << LL_ENDL;
                    break;
                }
            }
        }
        else if (FSPrimfeedAuth::isPendingAuth())
        {
            // Show the status text when authorisation is pending
            mStatusLoadingText->setVisible(true);
            status_text = LLTrans::getString("SocialPrimfeedConnecting");
            mStatusLoadingText->setValue(status_text);
        }
        else
        {
            // Show the status text when not authorised
            mStatusErrorText->setVisible(true);
            status_text = LLTrans::getString("SocialPrimfeedNotAuthorized");
            mStatusErrorText->setValue(status_text);
        }
    }
    LLFloater::draw();
}

void FSFloaterPrimfeed::onOpen(const LLSD& key)
{
    mPrimfeedPhotoPanel->onOpen(key);
}

LLSnapshotLivePreview* FSFloaterPrimfeed::getPreviewView()
{
    if (mPrimfeedPhotoPanel)
    {
        return mPrimfeedPhotoPanel->getPreviewView();
    }
    return nullptr;
}

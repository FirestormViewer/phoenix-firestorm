/**
* @file fsfloaterprimfeed.cpp
* @brief Declaration of primfeed floater
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
#ifndef FS_FLOATERPRIMFEED_H
#define FS_FLOATERPRIMFEED_H

#include "llfloater.h"
#include "lltextbox.h"
#include "llviewertexture.h"

class LLIconCtrl;
class LLCheckBoxCtrl;
class LLSnapshotLivePreview;
class LLFloaterBigPreview;

/*
 * (TODO) Beq: Refactor this with Flickr
 * Primfeed floater is copied heavily from the LLFlaoterFlickr class and deliberately implemetns much of the underlying plumbinng into the connector class.
 * Once this is bedded in and any initial issues are addressed, it would be sensible to refactor both the flickr and primfeed classes to share a common base.
 * In particular a ref counted test for the livepreview would eliminate the need for the static update method in the app mainloop.
*/
class FSPrimfeedPhotoPanel : public LLPanel
{
public:
    FSPrimfeedPhotoPanel();
    ~FSPrimfeedPhotoPanel();

    bool postBuild();
    S32 notify(const LLSD& info);
    void draw();

    LLSnapshotLivePreview* getPreviewView();
    void onVisibilityChange(bool new_visibility);
    void onClickNewSnapshot();
    void onClickBigPreview();
    void onSend();
    bool onPrimfeedConnectStateChange(const LLSD& data);

    void sendPhoto();
    void clearAndClose();

    void updateControls();
    void updateResolution(bool do_update);
    void checkAspectRatio(S32 index);
    LLUICtrl* getRefreshBtn();

    /*virtual*/ void onOpen(const LLSD& key);
    void primfeedAuthResponse(bool success, const LLSD& response);
    void uploadCallback(bool success, const LLSD& response);

private:
    bool isPreviewVisible() const;
    void attachPreview();

    bool checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, bool isWidthChanged, S32 max_value);

    LLHandle<LLView> mPreviewHandle;

    LLUICtrl * mResolutionComboBox;
    LLUICtrl * mFilterComboBox;
    LLUICtrl * mRefreshBtn;
    LLUICtrl * mWorkingLabel;
    LLUICtrl * mThumbnailPlaceholder;
    LLUICtrl * mDescriptionTextBox;
    LLUICtrl * mLocationCheckbox;

    LLUICtrl * mCommercialCheckbox;
    LLUICtrl * mPublicGalleryCheckbox;
    LLUICtrl * mRatingComboBox;
    LLUICtrl * mPostButton;
    LLUICtrl * mCancelButton;
    LLButton * mBtnPreview;

    LLFloaterBigPreview * mBigPreviewFloater;
};

class FSPrimfeedAccountPanel : public LLPanel
{
public:
    FSPrimfeedAccountPanel();
    bool postBuild();
    void draw();

private:
    void onVisibilityChange(bool new_visibility);
    void primfeedAuthResponse(bool success, const LLSD& response);
    bool onPrimfeedConnectStateChange(const LLSD& data);
    bool onPrimfeedConnectInfoChange();
    void onConnect();
    void onUseAnotherAccount();
    void onDisconnect();

    void showConnectButton();
    void hideConnectButton();
    void showDisconnectedLayout();
    void showConnectedLayout();

    LLTextBox * mAccountConnectedAsLabel;
    LLTextBox * mAccountNameLink;
    LLTextBox * mAccountPlan;
    LLUICtrl * mPanelButtons;
    LLUICtrl * mConnectButton;
    LLUICtrl * mDisconnectButton;
};


class FSFloaterPrimfeed : public LLFloater
{
public:
    explicit FSFloaterPrimfeed(const LLSD& key);
    static void update();    
    bool postBuild();
    void draw();
    void onClose(bool app_quitting);
    void onCancel();

    void showPhotoPanel();

    void onOpen(const LLSD& key);
    LLSnapshotLivePreview* getPreviewView();

private:
    FSPrimfeedPhotoPanel* mPrimfeedPhotoPanel;
    FSPrimfeedAccountPanel* mPrimfeedAccountPanel;
    LLTextBox* mStatusErrorText;
    LLTextBox* mStatusLoadingText;
    LLUICtrl*  mStatusLoadingIndicator;
};

#endif // LL_FSFLOATERPRIMFEED_H


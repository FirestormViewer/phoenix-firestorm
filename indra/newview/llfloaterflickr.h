/**
* @file   llfloaterflickr.h
* @brief  Header file for llfloaterflickr
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
#ifndef LL_LLFLOATERFLICKR_H
#define LL_LLFLOATERFLICKR_H

#include "llfloater.h"
#include "lltextbox.h"
#include "llviewertexture.h"

class LLIconCtrl;
class LLCheckBoxCtrl;
class LLSnapshotLivePreview;
class LLFloaterBigPreview;

class LLFlickrPhotoPanel : public LLPanel
{
public:
    LLFlickrPhotoPanel();
    ~LLFlickrPhotoPanel();

    bool postBuild();
    S32 notify(const LLSD& info);
    void draw();

    LLSnapshotLivePreview* getPreviewView();
    void onVisibilityChange(bool new_visibility);
    void onClickNewSnapshot();
    void onClickBigPreview();
    void onSend();
    bool onFlickrConnectStateChange(const LLSD& data);

    void sendPhoto();
    void clearAndClose();

    void updateControls();
    void updateResolution(bool do_update);
    void checkAspectRatio(S32 index);
    LLUICtrl* getRefreshBtn();

    // <FS:Ansariel> Exodus' flickr upload
    /*virtual*/ void onOpen(const LLSD& key);
    void flickrAuthResponse(bool success, const LLSD& response);
    void uploadCallback(bool success, const LLSD& response);
    // </FS:Ansariel>

private:
    bool isPreviewVisible();
    void attachPreview();

    // <FS:Ansariel> FIRE-15112: Allow custom resolution for SLShare
    bool checkImageSize(LLSnapshotLivePreview* previewp, S32& width, S32& height, bool isWidthChanged, S32 max_value);

    LLHandle<LLView> mPreviewHandle;

    LLUICtrl * mResolutionComboBox;
    LLUICtrl * mFilterComboBox;
    LLUICtrl * mRefreshBtn;
    LLUICtrl * mWorkingLabel;
    LLUICtrl * mThumbnailPlaceholder;
    LLUICtrl * mTitleTextBox;
    LLUICtrl * mDescriptionTextBox;
    LLUICtrl * mLocationCheckbox;
    LLUICtrl * mTagsTextBox;
    LLUICtrl * mRatingComboBox;
    LLUICtrl * mPostButton;
    LLUICtrl * mCancelButton;
    LLButton * mBtnPreview;

    LLFloaterBigPreview * mBigPreviewFloater;
};

class LLFlickrAccountPanel : public LLPanel
{
public:
    LLFlickrAccountPanel();
    bool postBuild();
    void draw();

private:
    void onVisibilityChange(bool new_visibility);
    bool onFlickrConnectStateChange(const LLSD& data);
    bool onFlickrConnectInfoChange();
    void onConnect();
    void onUseAnotherAccount();
    void onDisconnect();

    void showConnectButton();
    void hideConnectButton();
    void showDisconnectedLayout();
    void showConnectedLayout();

    LLTextBox * mAccountCaptionLabel;
    LLTextBox * mAccountNameLabel;
    LLUICtrl * mPanelButtons;
    LLUICtrl * mConnectButton;
    LLUICtrl * mDisconnectButton;
};


class LLFloaterFlickr : public LLFloater
{
public:
    LLFloaterFlickr(const LLSD& key);
    bool postBuild();
    void draw();
    void onClose(bool app_quitting);
    void onCancel();

    void showPhotoPanel();

    // <FS:Ansariel> Exodus' flickr upload
    void onOpen(const LLSD& key);
    LLSnapshotLivePreview* getPreviewView(); // <FS:Beq/> Required for snapshot frame rendering

    static void update(); // <FS:Beq/> FIRE-35002 - Flickr preview not updating whne opened directly from tool tray icon
private:
    LLFlickrPhotoPanel* mFlickrPhotoPanel;
    LLTextBox* mStatusErrorText;
    LLTextBox* mStatusLoadingText;
    LLUICtrl*  mStatusLoadingIndicator;
};

#endif // LL_LLFLOATERFLICKR_H


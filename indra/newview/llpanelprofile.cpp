/**
* @file llpanelprofile.cpp
* @brief Profile panel implementation
*
* $LicenseInfo:firstyear=2022&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2022, Linden Research, Inc.
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
#include "llpanelprofile.h"

// Common
#include "llavatarnamecache.h"
#include "llsdutil.h"
#include "llslurl.h"
#include "lldateutil.h" //ageFromDate

// UI
#include "llavatariconctrl.h"
#include "llclipboard.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llloadingindicator.h"
#include "llmenubutton.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lltoggleablemenu.h"
#include "lltooldraganddrop.h"
#include "llgrouplist.h"
#include "llurlaction.h"

// Image
#include "llimagej2c.h"

// Newview
#include "llagent.h" //gAgent
#include "llagentpicksinfo.h"
#include "llavataractions.h"
#include "llavatarpropertiesprocessor.h"
#include "llcallingcard.h"
#include "llcommandhandler.h"
#include "llfloaterprofiletexture.h"
#include "llfloaterreg.h"
#include "llfloaterreporter.h"
#include "llfilepicker.h"
#include "llfirstuse.h"
#include "llgroupactions.h"
#include "lllogchat.h"
#include "llmutelist.h"
#include "llnotificationsutil.h"
#include "llpanelblockedlist.h"
#include "llpanelprofileclassifieds.h"
#include "llpanelprofilepicks.h"
#include "llthumbnailctrl.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewermenu.h" //is_agent_mappable
#include "llviewermenufile.h"
#include "llviewertexturelist.h"
#include "llvoiceclient.h"
#include "llweb.h"
#include "llviewernetwork.h" // <FS:Beq> For LLGridManager

#include "fsdata.h"
#include "fsradar.h"        // <FS:Zi> Update notes in radar when edited
#include "llviewermenu.h"

static LLPanelInjector<LLPanelProfileSecondLife> t_panel_profile_secondlife("panel_profile_secondlife");
static LLPanelInjector<LLPanelProfileWeb> t_panel_web("panel_profile_web");
static LLPanelInjector<LLPanelProfilePicks> t_panel_picks("panel_profile_picks");
static LLPanelInjector<LLPanelProfileFirstLife> t_panel_firstlife("panel_profile_firstlife");
static LLPanelInjector<LLPanelProfileNotes> t_panel_notes("panel_profile_notes");
static LLPanelInjector<LLPanelProfile>          t_panel_profile("panel_profile");

static const std::string PANEL_SECONDLIFE   = "panel_profile_secondlife";
static const std::string PANEL_WEB          = "panel_profile_web";
static const std::string PANEL_PICKS        = "panel_profile_picks";
static const std::string PANEL_CLASSIFIEDS  = "panel_profile_classifieds";
static const std::string PANEL_FIRSTLIFE    = "panel_profile_firstlife";
static const std::string PANEL_NOTES        = "panel_profile_notes";
static const std::string PANEL_PROFILE_VIEW = "panel_profile_view";

static const std::string PROFILE_PROPERTIES_CAP = "AgentProfile";
static const std::string PROFILE_IMAGE_UPLOAD_CAP = "UploadAgentProfileImage";

//////////////////////////////////////////////////////////////////////////

LLUUID post_profile_image(std::string cap_url, const LLSD &first_data, std::string path_to_image, LLHandle<LLPanel> *handle)
{
    LLCore::HttpRequest::policy_t httpPolicy(LLCore::HttpRequest::DEFAULT_POLICY_ID);
    LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
        httpAdapter(new LLCoreHttpUtil::HttpCoroutineAdapter("post_profile_image_coro", httpPolicy));
    LLCore::HttpRequest::ptr_t httpRequest(new LLCore::HttpRequest);
    LLCore::HttpHeaders::ptr_t httpHeaders;

    LLCore::HttpOptions::ptr_t httpOpts(new LLCore::HttpOptions);
    httpOpts->setFollowRedirects(true);

    LLSD result = httpAdapter->postAndSuspend(httpRequest, cap_url, first_data, httpOpts, httpHeaders);

    LLSD httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    if (!status)
    {
        // todo: notification?
        LL_WARNS("AvatarProperties") << "Failed to get uploader cap " << status.toString() << LL_ENDL;
        return LLUUID::null;
    }
    if (!result.has("uploader"))
    {
        // todo: notification?
        LL_WARNS("AvatarProperties") << "Failed to get uploader cap, response contains no data." << LL_ENDL;
        return LLUUID::null;
    }
    std::string uploader_cap = result["uploader"].asString();
    if (uploader_cap.empty())
    {
        LL_WARNS("AvatarProperties") << "Failed to get uploader cap, cap invalid." << LL_ENDL;
        return LLUUID::null;
    }

    // Upload the image
    LLCore::HttpRequest::ptr_t uploaderhttpRequest(new LLCore::HttpRequest);
    LLCore::HttpHeaders::ptr_t uploaderhttpHeaders(new LLCore::HttpHeaders);
    LLCore::HttpOptions::ptr_t uploaderhttpOpts(new LLCore::HttpOptions);
    S64 length;

    {
        llifstream instream(path_to_image.c_str(), std::iostream::binary | std::iostream::ate);
        if (!instream.is_open())
        {
            LL_WARNS("AvatarProperties") << "Failed to open file " << path_to_image << LL_ENDL;
            return LLUUID::null;
        }
        length = instream.tellg();
    }

    uploaderhttpHeaders->append(HTTP_OUT_HEADER_CONTENT_TYPE, "application/jp2"); // optional
    uploaderhttpHeaders->append(HTTP_OUT_HEADER_CONTENT_LENGTH, llformat("%d", length)); // required!
    uploaderhttpOpts->setFollowRedirects(true);

    result = httpAdapter->postFileAndSuspend(uploaderhttpRequest, uploader_cap, path_to_image, uploaderhttpOpts, uploaderhttpHeaders);

    httpResults = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
    status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

    LL_DEBUGS("AvatarProperties") << result << LL_ENDL;

    if (!status)
    {
        LL_WARNS("AvatarProperties") << "Failed to upload image " << status.toString() << LL_ENDL;
        return LLUUID::null;
    }

    if (result["state"].asString() != "complete")
    {
        if (result.has("message"))
        {
            LL_WARNS("AvatarProperties") << "Failed to upload image, state " << result["state"] << " message: " << result["message"] << LL_ENDL;
        }
        else
        {
            LL_WARNS("AvatarProperties") << "Failed to upload image " << result << LL_ENDL;
        }
        return LLUUID::null;
    }

    return result["new_asset"].asUUID();
}

enum EProfileImageType
{
    PROFILE_IMAGE_SL,
    PROFILE_IMAGE_FL,
};

void post_profile_image_coro(std::string cap_url, EProfileImageType type, std::string path_to_image, LLHandle<LLPanel> *handle)
{
    LLSD data;
    switch (type)
    {
    case PROFILE_IMAGE_SL:
        data["profile-image-asset"] = "sl_image_id";
        break;
    case PROFILE_IMAGE_FL:
        data["profile-image-asset"] = "fl_image_id";
        break;
    }

    LLUUID result = post_profile_image(cap_url, data, path_to_image, handle);

    // reset loading indicator
    if (!handle->isDead())
    {
        switch (type)
        {
        case PROFILE_IMAGE_SL:
            {
                LLPanelProfileSecondLife* panel = static_cast<LLPanelProfileSecondLife*>(handle->get());
                if (result.notNull())
                {
                    panel->setProfileImageUploaded(result);
                }
                else
                {
                    // failure, just stop progress indicator
                    panel->setProfileImageUploading(false);
                }
                break;
            }
        case PROFILE_IMAGE_FL:
            {
                LLPanelProfileFirstLife* panel = static_cast<LLPanelProfileFirstLife*>(handle->get());
                if (result.notNull())
                {
                    panel->setProfileImageUploaded(result);
                }
                else
                {
                    // failure, just stop progress indicator
                    panel->setProfileImageUploading(false);
                }
                break;
            }
        }
    }

    if (type == PROFILE_IMAGE_SL && result.notNull())
    {
        LLAvatarIconIDCache::getInstance()->add(gAgentID, result);
        // Should trigger callbacks in icon controls
        LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(gAgentID);
    }

    // Cleanup
    LLFile::remove(path_to_image);
    delete handle;
}

//////////////////////////////////////////////////////////////////////////
// LLProfileHandler

class LLProfileHandler : public LLCommandHandler
{
public:
    // requires trusted browser to trigger
    LLProfileHandler() : LLCommandHandler("profile", UNTRUSTED_THROTTLE) { }

    bool handle(const LLSD& params,
                const LLSD& query_map,
                const std::string& grid,
                LLMediaCtrl* web)
    {
        if (params.size() < 1) return false;
        std::string agent_name = params[0];
        LL_INFOS() << "Profile, agent_name " << agent_name << LL_ENDL;
        std::string url = getProfileURL(agent_name);
        LLWeb::loadURLInternal(url);

        return true;
    }
};
LLProfileHandler gProfileHandler;


//////////////////////////////////////////////////////////////////////////
// LLAgentHandler

class LLAgentHandler : public LLCommandHandler
{
public:
    // requires trusted browser to trigger
    LLAgentHandler() : LLCommandHandler("agent", UNTRUSTED_THROTTLE) { }

    virtual bool canHandleUntrusted(
        const LLSD& params,
        const LLSD& query_map,
        LLMediaCtrl* web,
        const std::string& nav_type)
    {
        if (params.size() < 2)
        {
            return true; // don't block, will fail later
        }

        if (nav_type == NAV_TYPE_CLICKED
            || nav_type == NAV_TYPE_EXTERNAL)
        {
            return true;
        }

        const std::string verb = params[1].asString();
        if (verb == "about" || verb == "inspect" || verb == "reportAbuse")
        {
            return true;
        }
        return false;
    }

    bool handle(const LLSD& params,
                const LLSD& query_map,
                const std::string& grid,
                LLMediaCtrl* web)
    {
        if (params.size() < 2) return false;
        LLUUID avatar_id;
        if (!avatar_id.set(params[0], false))
        {
            return false;
        }

        const std::string verb = params[1].asString();
        // <FS:Ansariel> FIRE-9045: Inspect links always open full profile
        //if (verb == "about")
        if (verb == "about" || (gSavedSettings.getBOOL("FSInspectAvatarSlurlOpensProfile") && verb == "inspect"))
        // </FS:Ansariel>
        {
            LLAvatarActions::showProfile(avatar_id);
            return true;
        }

        if (verb == "inspect")
        {
            LLFloaterReg::showInstance("inspect_avatar", LLSD().with("avatar_id", avatar_id));
            return true;
        }

        if (verb == "im")
        {
            LLAvatarActions::startIM(avatar_id);
            return true;
        }

        if (verb == "pay")
        {
            if (!LLUI::getInstance()->mSettingGroups["config"]->getBOOL("EnableAvatarPay"))
            {
                LLNotificationsUtil::add("NoAvatarPay", LLSD(), LLSD(), std::string("SwitchToStandardSkinAndQuit"));
                return true;
            }

            LLAvatarActions::pay(avatar_id);
            return true;
        }

        if (verb == "offerteleport")
        {
            LLAvatarActions::offerTeleport(avatar_id);
            return true;
        }

        if (verb == "requestfriend")
        {
            LLAvatarActions::requestFriendshipDialog(avatar_id);
            return true;
        }

        if (verb == "removefriend")
        {
            LLAvatarActions::removeFriendDialog(avatar_id);
            return true;
        }

        if (verb == "mute")
        {
            if (! LLAvatarActions::isBlocked(avatar_id))
            {
                LLAvatarActions::toggleBlock(avatar_id);
            }
            return true;
        }

        if (verb == "unmute")
        {
            if (LLAvatarActions::isBlocked(avatar_id))
            {
                LLAvatarActions::toggleBlock(avatar_id);
            }
            return true;
        }

        if (verb == "block")
        {
            if (params.size() > 2)
            {
                const std::string object_name = LLURI::unescape(params[2].asString());
                LLMute mute(avatar_id, object_name, LLMute::OBJECT);
                LLMuteList::getInstance()->add(mute);
                LLPanelBlockedList::showPanelAndSelect(mute.mID);
            }
            return true;
        }

        if (verb == "unblock")
        {
            if (params.size() > 2)
            {
                const std::string object_name = params[2].asString();
                LLMute mute(avatar_id, object_name, LLMute::OBJECT);
                LLMuteList::getInstance()->remove(mute);
            }
            return true;
        }

        // reportAbuse is here due to convoluted avatar handling
        // in LLScrollListCtrl and LLTextBase
        if (verb == "reportAbuse" && web == NULL)
        {
            LLAvatarName av_name;
            if (LLAvatarNameCache::get(avatar_id, &av_name))
            {
                LLFloaterReporter::showFromAvatar(avatar_id, av_name.getCompleteName());
            }
            else
            {
                LLFloaterReporter::showFromAvatar(avatar_id, "not avaliable");
            }
            return true;
        }
        return false;
    }
};
LLAgentHandler gAgentHandler;

// <FS:Ansariel> FIRE-30611: "You" in transcript is underlined
class FSAgentSelfHandler : public LLCommandHandler
{
public:
    // requires trusted browser to trigger
    FSAgentSelfHandler() : LLCommandHandler("agentself", UNTRUSTED_THROTTLE) { }

    bool handle(const LLSD& params, const LLSD& query_map, const std::string& grid, LLMediaCtrl* web)
    {
        return gAgentHandler.handle(params, query_map, grid, web);
    }
};
FSAgentSelfHandler gAgentSelfHandler;
// </FS:Ansariel>

///----------------------------------------------------------------------------
/// LLFloaterProfilePermissions
///----------------------------------------------------------------------------

class LLFloaterProfilePermissions
    : public LLFloater
    , public LLFriendObserver
{
public:
    LLFloaterProfilePermissions(LLView * owner, const LLUUID &avatar_id);
    ~LLFloaterProfilePermissions();
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void draw() override;
    void changed(U32 mask) override; // LLFriendObserver

    void onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name);
    bool hasUnsavedChanges() { return mHasUnsavedPermChanges; }

    void onApplyRights();

private:
    void fillRightsData();
    void rightsConfirmationCallback(const LLSD& notification, const LLSD& response);
    void confirmModifyRights(bool grant);
    void onCommitSeeOnlineRights();
    void onCommitEditRights();
    void onCancel();

    LLTextBase*         mDescription;
    LLCheckBoxCtrl*     mOnlineStatus;
    LLCheckBoxCtrl*     mMapRights;
    LLCheckBoxCtrl*     mEditObjectRights;
    LLButton*           mOkBtn;
    LLButton*           mCancelBtn;

    LLUUID              mAvatarID;
    F32                 mContextConeOpacity;
    bool                mHasUnsavedPermChanges;
    LLHandle<LLView>    mOwnerHandle;

    boost::signals2::connection mAvatarNameCacheConnection;
};

LLFloaterProfilePermissions::LLFloaterProfilePermissions(LLView * owner, const LLUUID &avatar_id)
    : LLFloater(LLSD())
    , mAvatarID(avatar_id)
    , mContextConeOpacity(0.0f)
    , mHasUnsavedPermChanges(false)
    , mOwnerHandle(owner->getHandle())
{
    buildFromFile("floater_profile_permissions.xml");
}

LLFloaterProfilePermissions::~LLFloaterProfilePermissions()
{
    mAvatarNameCacheConnection.disconnect();
    if (mAvatarID.notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(mAvatarID, this);
    }
}

bool LLFloaterProfilePermissions::postBuild()
{
    mDescription = getChild<LLTextBase>("perm_description");
    mOnlineStatus = getChild<LLCheckBoxCtrl>("online_check");
    mMapRights = getChild<LLCheckBoxCtrl>("map_check");
    mEditObjectRights = getChild<LLCheckBoxCtrl>("objects_check");
    mOkBtn = getChild<LLButton>("perms_btn_ok");
    mCancelBtn = getChild<LLButton>("perms_btn_cancel");

    mOnlineStatus->setCommitCallback([this](LLUICtrl*, void*) { onCommitSeeOnlineRights(); }, nullptr);
    mMapRights->setCommitCallback([this](LLUICtrl*, void*) { mHasUnsavedPermChanges = true; }, nullptr);
    mEditObjectRights->setCommitCallback([this](LLUICtrl*, void*) { onCommitEditRights(); }, nullptr);
    mOkBtn->setCommitCallback([this](LLUICtrl*, void*) { onApplyRights(); }, nullptr);
    mCancelBtn->setCommitCallback([this](LLUICtrl*, void*) { onCancel(); }, nullptr);

    return true;
}

void LLFloaterProfilePermissions::onOpen(const LLSD& key)
{
    if (LLAvatarActions::isFriend(mAvatarID))
    {
        LLAvatarTracker::instance().addParticularFriendObserver(mAvatarID, this);
        fillRightsData();
    }

    mCancelBtn->setFocus(true);

    mAvatarNameCacheConnection = LLAvatarNameCache::get(mAvatarID, boost::bind(&LLFloaterProfilePermissions::onAvatarNameCache, this, _1, _2));
}

void LLFloaterProfilePermissions::draw()
{
    // drawFrustum
    LLView *owner = mOwnerHandle.get();
    static LLCachedControl<F32> max_opacity(gSavedSettings, "PickerContextOpacity", 0.4f);
    drawConeToOwner(mContextConeOpacity, max_opacity, owner);
    LLFloater::draw();
}

void LLFloaterProfilePermissions::changed(U32 mask)
{
    if (mask != LLFriendObserver::ONLINE)
    {
        fillRightsData();
    }
}

void LLFloaterProfilePermissions::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    LLStringUtil::format_map_t args;
    // <FS:Ansariel> Fix LL UI/UX design accident
    //args["[AGENT_NAME]"] = av_name.getDisplayName();
    args["[AGENT_NAME]"] = av_name.getCompleteName();
    std::string descritpion = getString("description_string", args);
    mDescription->setValue(descritpion);
}

void LLFloaterProfilePermissions::fillRightsData()
{
    const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(mAvatarID);
    // If true - we are viewing friend's profile, enable check boxes and set values.
    if (relation)
    {
        S32 rights = relation->getRightsGrantedTo();

        bool see_online = LLRelationship::GRANT_ONLINE_STATUS & rights;
        mOnlineStatus->setValue(see_online);
        mMapRights->setEnabled(see_online);
        mMapRights->setValue(LLRelationship::GRANT_MAP_LOCATION & rights);
        mEditObjectRights->setValue(LLRelationship::GRANT_MODIFY_OBJECTS & rights);
    }
    else
    {
        closeFloater();
        LL_INFOS("ProfilePermissions") << "Floater closing since agent is no longer a friend" << LL_ENDL;
    }
}

void LLFloaterProfilePermissions::rightsConfirmationCallback(const LLSD& notification,
    const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if (option != 0) // canceled
    {
        mEditObjectRights->setValue(!mEditObjectRights->getValue().asBoolean());
    }
    else
    {
        mHasUnsavedPermChanges = true;
    }
}

void LLFloaterProfilePermissions::confirmModifyRights(bool grant)
{
    LLSD args;
    args["NAME"] = LLSLURL("agent", mAvatarID, "completename").getSLURLString();
    LLNotificationsUtil::add(grant ? "GrantModifyRights" : "RevokeModifyRights", args, LLSD(),
        boost::bind(&LLFloaterProfilePermissions::rightsConfirmationCallback, this, _1, _2));
}

void LLFloaterProfilePermissions::onCommitSeeOnlineRights()
{
    bool see_online = mOnlineStatus->getValue().asBoolean();
    mMapRights->setEnabled(see_online);
    if (see_online)
    {
        const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(mAvatarID);
        if (relation)
        {
            S32 rights = relation->getRightsGrantedTo();
            mMapRights->setValue(LLRelationship::GRANT_MAP_LOCATION & rights);
        }
        else
        {
            closeFloater();
            LL_INFOS("ProfilePermissions") << "Floater closing since agent is no longer a friend" << LL_ENDL;
        }
    }
    else
    {
        mMapRights->setValue(false);
    }
    mHasUnsavedPermChanges = true;
}

void LLFloaterProfilePermissions::onCommitEditRights()
{
    const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(mAvatarID);

    if (!buddy_relationship)
    {
        LL_WARNS("ProfilePermissions") << "Trying to modify rights for non-friend avatar. Closing floater." << LL_ENDL;
        closeFloater();
        return;
    }

    bool allow_modify_objects = mEditObjectRights->getValue().asBoolean();

    // if modify objects checkbox clicked
    if (buddy_relationship->isRightGrantedTo(
        LLRelationship::GRANT_MODIFY_OBJECTS) != allow_modify_objects)
    {
        confirmModifyRights(allow_modify_objects);
    }
}

void LLFloaterProfilePermissions::onApplyRights()
{
    const LLRelationship* buddy_relationship = LLAvatarTracker::instance().getBuddyInfo(mAvatarID);

    if (!buddy_relationship)
    {
        LL_WARNS("ProfilePermissions") << "Trying to modify rights for non-friend avatar. Skipped." << LL_ENDL;
        return;
    }

    S32 rights = 0;

    if (mOnlineStatus->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_ONLINE_STATUS;
    }
    if (mMapRights->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_MAP_LOCATION;
    }
    if (mEditObjectRights->getValue().asBoolean())
    {
        rights |= LLRelationship::GRANT_MODIFY_OBJECTS;
    }

    LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(mAvatarID, rights);

    closeFloater();
}

void LLFloaterProfilePermissions::onCancel()
{
    closeFloater();
}

//////////////////////////////////////////////////////////////////////////
// LLPanelProfileSecondLife

LLPanelProfileSecondLife::LLPanelProfileSecondLife()
    : LLPanelProfilePropertiesProcessorTab()
    , mAvatarNameCacheConnection()
    , mHasUnsavedDescriptionChanges(false)
    , mWaitingForImageUpload(false)
    , mAllowPublish(false)
    , mHideAge(false)
    , mRlvBehaviorCallbackConnection() // <FS:Ansariel> RLVa support
    , mPreview(false)                  // <AS:Chanayane> Preview button
{
}

LLPanelProfileSecondLife::~LLPanelProfileSecondLife()
{
    if (getAvatarId().notNull())
    {
        LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        // <FS:Zi> FIRE-32184: Online/Offline status not working for non-friends
        LLAvatarPropertiesProcessor::getInstance()->removeObserver(getAvatarId(), &mPropertiesObserver);
    }

    LLVoiceClient::removeObserver((LLVoiceClientStatusObserver*)this);

    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }

    // <FS:Ansariel> RLVa support
    if (mRlvBehaviorCallbackConnection.connected())
    {
        mRlvBehaviorCallbackConnection.disconnect();
    }
    // </FS:Ansariel>
}

bool LLPanelProfileSecondLife::postBuild()
{
    mGroupList              = getChild<LLGroupList>("group_list");
    // <FS:Ansariel> Fix LL UI/UX design accident
    //mShowInSearchCombo      = getChild<LLComboBox>("show_in_search");
    //mHideAgeCombo           = getChild<LLComboBox>("hide_age");
    mShowInSearchCheckbox   = getChild<LLCheckBoxCtrl>("show_in_search");
    mHideAgeCheckbox        = getChild<LLCheckBoxCtrl>("hide_sl_age");
    // </FS:Ansariel>
    // <FS:Zi> Allow proper texture swatch handling
    // mSecondLifePic          = getChild<LLThumbnailCtrl>("2nd_life_pic");
    mSecondLifePic          = getChild<LLTextureCtrl>("2nd_life_pic");
    // <FS:Zi>
    mSecondLifePicLayout    = getChild<LLPanel>("image_panel");
    mDescriptionEdit        = getChild<LLTextEditor>("sl_description_edit");
    // mAgentActionMenuButton  = getChild<LLMenuButton>("agent_actions_menu"); // <FS:Ansariel> Fix LL UI/UX design accident
    mSaveDescriptionChanges = getChild<LLButton>("save_description_changes");
    mDiscardDescriptionChanges = getChild<LLButton>("discard_description_changes");
    mCanSeeOnlineIcon       = getChild<LLIconCtrl>("can_see_online");
    mCantSeeOnlineIcon      = getChild<LLIconCtrl>("cant_see_online");
    mCanSeeOnMapIcon        = getChild<LLIconCtrl>("can_see_on_map");
    mCantSeeOnMapIcon       = getChild<LLIconCtrl>("cant_see_on_map");
    mCanEditObjectsIcon     = getChild<LLIconCtrl>("can_edit_objects");
    mCantEditObjectsIcon    = getChild<LLIconCtrl>("cant_edit_objects");

    // <FS:Ansariel> Fix LL UI/UX design accident
    mStatusText = getChild<LLTextBox>("status");
    mCopyMenuButton = getChild<LLMenuButton>("copy_btn");
    mGroupInviteButton = getChild<LLButton>("group_invite");
    mDisplayNameButton = getChild<LLButton>("set_name");
    mImageActionMenuButton = getChild<LLMenuButton>("image_action_btn");
    mTeleportButton = getChild<LLButton>("teleport");
    mShowOnMapButton = getChild<LLButton>("show_on_map_btn");
    mBlockButton = getChild<LLButton>("block");
    mUnblockButton = getChild<LLButton>("unblock");
    mAddFriendButton = getChild<LLButton>("add_friend");
    mRemoveFriendButton = getChild<LLButton>("remove_friend");    // <FS:Zi> Add "Remove Friend" button to profile
    mPayButton = getChild<LLButton>("pay");
    mIMButton = getChild<LLButton>("im");
    mOverflowButton = getChild<LLMenuButton>("overflow_btn");
    // </FS:Ansariel>
    mPreviewButton = getChild<LLButton>("btn_preview"); // <AS:Chanayane> Preview button

    // <FS:Ansariel> Fix LL UI/UX design accident
    //mShowInSearchCombo->setCommitCallback([this](LLUICtrl*, void*) { onShowInSearchCallback(); }, nullptr);
    //mHideAgeCombo->setCommitCallback([this](LLUICtrl*, void*) { onHideAgeCallback(); }, nullptr);
    mHideAgeCheckbox->setCommitCallback([this](LLUICtrl*, void*) { onHideAgeCallback(); }, nullptr);
    mShowInSearchCheckbox->setCommitCallback([this](LLUICtrl*, void*) { onShowInSearchCallback(); }, nullptr);
    mGroupInviteButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("invite_to_group")); }, nullptr);
    mDisplayNameButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("edit_display_name")); }, nullptr);
    mTeleportButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("offer_teleport")); }, nullptr);
    mShowOnMapButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("can_show_on_map")); }, nullptr);
    mBlockButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("toggle_block_agent")); }, nullptr);
    mUnblockButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("toggle_block_agent")); }, nullptr);
    mAddFriendButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("add_friend")); }, nullptr);
    mRemoveFriendButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("remove_friend")); }, nullptr);   // <FS:Zi> Add "Remove Friend" button to profile
    mPayButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("pay")); }, nullptr);
    mIMButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("im")); }, nullptr);
    // </FS:Ansariel>
    // <AS:Chanayane> Preview button
    mPreviewButton->setCommitCallback([this](LLUICtrl*, void*) { onCommitMenu(LLSD("preview")); }, nullptr);
    // </AS:Chanayane>
    mGroupList->setDoubleClickCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { LLPanelProfileSecondLife::openGroupProfile(); });
    mGroupList->setReturnCallback([this](LLUICtrl*, const LLSD&) { LLPanelProfileSecondLife::openGroupProfile(); });
    mSaveDescriptionChanges->setCommitCallback([this](LLUICtrl*, void*) { onSaveDescriptionChanges(); }, nullptr);
    mDiscardDescriptionChanges->setCommitCallback([this](LLUICtrl*, void*) { onDiscardDescriptionChanges(); }, nullptr);
    mDescriptionEdit->setKeystrokeCallback([this](LLTextEditor* caller) { onSetDescriptionDirty(); });

    mCanSeeOnlineIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    mCantSeeOnlineIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    mCanSeeOnMapIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    mCantSeeOnMapIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    mCanEditObjectsIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    mCantEditObjectsIcon->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentPermissionsDialog(); });
    // <FS:Zi> Allow proper texture swatch handling
    // mSecondLifePic->setMouseUpCallback([this](LLUICtrl*, S32 x, S32 y, MASK mask) { onShowAgentProfileTexture(); });
    mSecondLifePic->setCommitCallback(boost::bind(&LLPanelProfileSecondLife::onSecondLifePicChanged, this));
    // </FS:Zi>

    // <FS:Ansariel> RLVa support
    mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourCallback(boost::bind(&LLPanelProfileSecondLife::updateRlvRestrictions, this, _1));

    return true;
}

// <FS:Zi> FIRE-32184: Online/Offline status not working for non-friends
void LLPanelProfileSecondLife::onAvatarProperties(const LLAvatarData* data)
{
    // only update the "unknown" status if they are showing as online, otherwise
    // we still don't know their true status
    if (data->agent_id == gAgentID && data->flags & AVATAR_ONLINE)
    {
        processOnlineStatus(false, true, true);
    }
}
// </FS:Zi>

void LLPanelProfileSecondLife::onOpen(const LLSD& key)
{
    LLPanelProfilePropertiesProcessorTab::onOpen(key); // <FS:Beq/> alter ancestry to re-enable UDP
    resetData();

    LLUUID avatar_id = getAvatarId();

    bool own_profile = getSelfProfile();

    mGroupList->setShowNone(!own_profile);

    //childSetVisible("notes_panel", !own_profile); // <FS:Ansariel> Doesn't exist (anymore)
    // <FS:Ansariel> Fix LL UI/UX design accident
    //childSetVisible("settings_panel", own_profile);
    //childSetVisible("about_buttons_panel", own_profile);
    mSaveDescriptionChanges->setVisible(own_profile);
    mDiscardDescriptionChanges->setVisible(own_profile);
    mShowInSearchCheckbox->setVisible(own_profile);
    // </FS:Ansariel>
    mPreviewButton->setVisible(own_profile); // <AS:Chanayane> Preview button

    if (own_profile)
    {
        // Group list control cannot toggle ForAgent loading
        // Less than ideal, but viewing own profile via search is edge case
        mGroupList->enableForAgent(false);
    }

    // Init menu, menu needs to be created in scope of a registar to work correctly.
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar commit;
    commit.add("Profile.Commit", [this](LLUICtrl*, const LLSD& userdata) { onCommitMenu(userdata); });

    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable;
    enable.add("Profile.EnableItem", [this](LLUICtrl*, const LLSD& userdata) { return onEnableMenu(userdata); });
    enable.add("Profile.CheckItem", [this](LLUICtrl*, const LLSD& userdata) { return onCheckMenu(userdata); });

    // <FS:Ansariel> Fix LL UI/UX design accident
    //if (own_profile)
    //{
    //    mAgentActionMenuButton->setMenu("menu_profile_self.xml", LLMenuButton::MP_BOTTOM_RIGHT);
    //}
    //else
    //{
    //    // Todo: use PeopleContextMenu instead?
    //    mAgentActionMenuButton->setMenu("menu_profile_other.xml", LLMenuButton::MP_BOTTOM_RIGHT);
    //}
// <FS:Beq> Remove the menu thingy and just have click picture to change
// if (own_profile)
// only if we are in opensim and opensim doesn't have the image upload cap
#ifdef OPENSIM
    std::string cap_url = gAgent.getRegionCapability(PROFILE_IMAGE_UPLOAD_CAP);
    if( own_profile && (!LLGridManager::instance().isInOpenSim() || !cap_url.empty() ) )
#else
    if (own_profile)
#endif
// </FS:Beq>
    {
        mImageActionMenuButton->setVisible(true);
        mImageActionMenuButton->setMenu("menu_fs_profile_image_actions.xml", LLMenuButton::MP_BOTTOM_RIGHT);
    }
    else
    {
        mImageActionMenuButton->setVisible(false);
        LLToggleableMenu* profile_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_fs_profile_overflow.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
        mOverflowButton->setMenu(profile_menu, LLMenuButton::MP_TOP_RIGHT);
    }

    mCopyMenuButton->setMenu("menu_fs_profile_name_field.xml", LLMenuButton::MP_BOTTOM_RIGHT);
    // </FS:Ansariel>

    mDescriptionEdit->setParseHTML(!own_profile);

    if (!own_profile)
    {
        mVoiceStatus = LLAvatarActions::canCall() && (LLAvatarActions::isFriend(avatar_id) ? LLAvatarTracker::instance().isBuddyOnline(avatar_id) : true);
        updateOnlineStatus();
        fillRightsData();
    }

    // <FS:Ansariel> Display agent ID
    getChild<LLUICtrl>("user_key")->setValue(avatar_id.asString());

    // <FS:Zi> Allow proper texture swatch handling
    mSecondLifePic->setEnabled(own_profile);

    mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileSecondLife::onAvatarNameCache, this, _1, _2));
}


bool LLPanelProfileSecondLife::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
                                          EDragAndDropType cargo_type,
                                          void* cargo_data,
                                          EAcceptance* accept,
                                          std::string& tooltip_msg)
{
    // Try children first
    if (LLPanelProfileTab::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data, accept, tooltip_msg)
        && *accept != ACCEPT_NO)
    {
        return true;
    }

    // No point sharing with own profile
    if (getSelfProfile())
    {
        return false;
    }

    // Exclude fields that look like they are editable.
    S32 child_x = 0;
    S32 child_y = 0;
    if (localPointToOtherView(x, y, &child_x, &child_y, mDescriptionEdit)
        && mDescriptionEdit->pointInView(child_x, child_y))
    {
        return false;
    }

    if (localPointToOtherView(x, y, &child_x, &child_y, mGroupList)
        && mGroupList->pointInView(child_x, child_y))
    {
        return false;
    }

    // Share
    LLToolDragAndDrop::handleGiveDragAndDrop(getAvatarId(),
                                             LLUUID::null,
                                             drop,
                                             cargo_type,
                                             cargo_data,
                                             accept);
    return true;
}

// <FS:Beq> restore UDP profiles for opensim that does not support the cap
void LLPanelProfileSecondLife::updateData()
{
#ifdef OPENSIM
    if (LLGridManager::instance().isInOpenSim() && gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
    LLUUID avatar_id = getAvatarId();
        if (!getStarted() && avatar_id.notNull() && gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty() && !getSelfProfile())
    {
        setIsLoading();
                LLAvatarPropertiesProcessor::getInstance()->sendAvatarGroupsRequest(avatar_id);
            }
    }
            else
#endif
    {
        LLPanelProfilePropertiesProcessorTab::updateData();
    }
}
// </FS:Beq>

void LLPanelProfileSecondLife::refreshName()
{
    if (!mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileSecondLife::onAvatarNameCache, this, _1, _2));
    }
}

// <FS:Beq> Restore UDP profiles
void LLPanelProfileSecondLife::apply(LLAvatarData* data)
{
#ifdef OPENSIM
    if (LLGridManager::instance().isInOpenSim() && getIsLoaded() && getSelfProfile())
    {
        data->image_id = mImageId;
        data->about_text = mDescriptionEdit->getValue().asString();
        data->allow_publish = mShowInSearchCheckbox->getValue();

        LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(data);
    }
#endif
}
// </FS:Beq>

void LLPanelProfileSecondLife::resetData()
{
    resetLoading();

    // Set default image and 1:1 dimensions for it
    // <FS:Ansariel> Retain texture picker for profile images
    //mSecondLifePic->setValue("Generic_Person_Large");
    mSecondLifePic->setImageAssetID(LLUUID::null);
    mImageId = LLUUID::null;

    // <FS:Ansariel> Fix LL UI/UX design accident
    //LLRect imageRect = mSecondLifePicLayout->getRect();
    LLRect imageRect = mSecondLifePic->getRect();
    mSecondLifePicLayout->reshape(imageRect.getHeight(), imageRect.getHeight());

    setDescriptionText(LLStringUtil::null);
    mGroups.clear();
    mGroupList->setGroups(mGroups);

    bool own_profile = getSelfProfile();
    mCanSeeOnlineIcon->setVisible(false);
    // <FS:Ansariel> Fix LL UI/UX design accident
    //mCantSeeOnlineIcon->setVisible(!own_profile);
    mCantSeeOnlineIcon->setVisible(false);
    mCanSeeOnMapIcon->setVisible(false);
    // <FS:Ansariel> Fix LL UI/UX design accident
    //mCantSeeOnMapIcon->setVisible(!own_profile);
    mCantSeeOnMapIcon->setVisible(false);
    mCanEditObjectsIcon->setVisible(false);
    // <FS:Ansariel> Fix LL UI/UX design accident
    //mCantEditObjectsIcon->setVisible(!own_profile);
    mCantEditObjectsIcon->setVisible(false);

    mCanSeeOnlineIcon->setEnabled(false);
    mCantSeeOnlineIcon->setEnabled(false);
    mCanSeeOnMapIcon->setEnabled(false);
    mCantSeeOnMapIcon->setEnabled(false);
    mCanEditObjectsIcon->setEnabled(false);
    mCantEditObjectsIcon->setEnabled(false);

    // <FS:Ansariel> Fix LL UI/UX design accident
    //childSetVisible("partner_layout", false);
    //childSetVisible("badge_layout", false);
    //childSetVisible("partner_spacer_layout", true);
    // <FS:Zi> Always show the online status text, just set it to "offline" when a friend is hiding
    // mStatusText->setVisible(false);
    mCopyMenuButton->setVisible(false);
    mGroupInviteButton->setVisible(!own_profile);
    if (own_profile && LLAvatarName::useDisplayNames())
    {
        mDisplayNameButton->setVisible(true);
        mDisplayNameButton->setEnabled(true);
    }
    mShowOnMapButton->setVisible(!own_profile);
    mPayButton->setVisible(!own_profile);
    mTeleportButton->setVisible(!own_profile);
    mIMButton->setVisible(!own_profile);
    mAddFriendButton->setVisible(!own_profile);
    mRemoveFriendButton->setVisible(!own_profile);   // <FS:Zi> Add "Remove Friend" button to profile
    mBlockButton->setVisible(!own_profile);
    mUnblockButton->setVisible(!own_profile);
    mGroupList->setShowNone(!own_profile);
    mOverflowButton->setVisible(!own_profile);

    updateButtons();
    // </FS:Ansariel>
}

void LLPanelProfileSecondLife::processProperties(void* data, EAvatarProcessorType type)
{
    if (APT_PROPERTIES == type)
    {
        LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProfileProperties(avatar_data);
        }
    }

    // <FS:Beq> Restore UDP profiles
    // discard UDP replies for profile data if profile capability is available
    // otherwise we will truncate profile descriptions to the old UDP limits
    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        return;
    }

    if (data && APT_PROPERTIES_LEGACY == type)
    {
        const LLAvatarData avatar_data(*static_cast<const LLAvatarLegacyData*>(data));
        if (getAvatarId() == avatar_data.avatar_id)
        {
            processProfileProperties(&avatar_data);
        }
    }
    else if (APT_GROUPS == type)
    {
        LLAvatarGroups* avatar_groups = static_cast<LLAvatarGroups*>(data);
        if (avatar_groups && getAvatarId() == avatar_groups->avatar_id)
        {
            processGroupProperties(avatar_groups);
        }
    }
    // </FS:Beq>
}

void LLPanelProfileSecondLife::processProfileProperties(const LLAvatarData* avatar_data)
{
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    if ((relationship != NULL || gAgent.isGodlike()) && !getSelfProfile())
    {
        // Relies onto friend observer to get information about online status updates.
        // Once SL-17506 gets implemented, condition might need to become:
        // (gAgent.isGodlike() || isRightGrantedFrom || flags & AVATAR_ONLINE)
        processOnlineStatus(relationship != NULL,
                            gAgent.isGodlike() || relationship->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS),
                            (avatar_data->flags & AVATAR_ONLINE));
    }
    // <FS:Zi> FIRE-32184: Online/Offline status not working for non-friends
    else if (avatar_data->flags & AVATAR_ONLINE_UNDEFINED)
    {
        // being a friend who doesn't show online status and appears online can't happen
        // so this is our marker for "undefined"
        processOnlineStatus(true, false, true);
    }
    // </FS:Zi>

    fillCommonData(avatar_data);

    fillPartnerData(avatar_data);

    fillAccountStatus(avatar_data);

    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty()) { // <FS>
    LLAvatarData::group_list_t::const_iterator it = avatar_data->group_list.begin();
    const LLAvatarData::group_list_t::const_iterator it_end = avatar_data->group_list.end();

    for (; it_end != it; ++it)
    {
        LLAvatarData::LLGroupData group_data = *it;
        mGroups[group_data.group_name] = group_data.group_id;
    }

    mGroupList->setGroups(mGroups);
    } // </FS>

// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
    if (LLGridManager::instance().isInOpenSim())
    {
        LLFloater* floater_profile = LLFloaterReg::findInstance("profile", LLSD().with("id", getAvatarId()));
        if (!floater_profile)
        {
            // floater is dead, so panels are dead as well
            return;
        }
        LLPanelProfile* panel_profile = floater_profile->findChild<LLPanelProfile>(PANEL_PROFILE_VIEW, true);
        if (panel_profile)
        {
            panel_profile->setAvatarData(avatar_data);
        }
        else
        {
            LL_WARNS() << PANEL_PROFILE_VIEW << " not found" << LL_ENDL;
        }
    }
#endif
// </FS:Beq>

    setLoaded();

    // <FS:Ansariel> Fix LL UI/UX design accident
    updateButtons();
}

// <FS> OpenSim
void LLPanelProfileSecondLife::processGroupProperties(const LLAvatarGroups* avatar_groups)
{
    LLAvatarGroups::group_list_t::const_iterator it = avatar_groups->group_list.begin();
    const LLAvatarGroups::group_list_t::const_iterator it_end = avatar_groups->group_list.end();

    for (; it_end != it; ++it)
    {
        LLAvatarGroups::LLGroupData group_data = *it;
        mGroups[group_data.group_name] = group_data.group_id;
    }

    mGroupList->setGroups(mGroups);
}
// </FS>

void LLPanelProfileSecondLife::openGroupProfile()
{
    LLUUID group_id = mGroupList->getSelectedUUID();
    LLGroupActions::show(group_id);
}

void LLPanelProfileSecondLife::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();
    // <FS:Ansariel> Fix LL UI/UX design accident
    //getChild<LLUICtrl>("display_name")->setValue(av_name.getDisplayName());
    //getChild<LLUICtrl>("user_name")->setValue(av_name.getAccountName());
    getChild<LLUICtrl>("complete_name")->setValue(av_name.getCompleteName());
    mCopyMenuButton->setVisible(true);
    // </FS:Ansariel>
}

void LLPanelProfileSecondLife::setProfileImageUploading(bool loading)
{
    LLLoadingIndicator* indicator = getChild<LLLoadingIndicator>("image_upload_indicator");
    indicator->setVisible(loading);
    if (loading)
    {
        indicator->start();
    }
    else
    {
        indicator->stop();
    }
    mWaitingForImageUpload = loading;
}

void LLPanelProfileSecondLife::setProfileImageUploaded(const LLUUID &image_asset_id)
{
    mSecondLifePic->setValue(image_asset_id);
    mImageId = image_asset_id;

    LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(image_asset_id);
    if (imagep->getFullHeight())
    {
        onImageLoaded(true, imagep);
    }
    else
    {
        imagep->setLoadedCallback(onImageLoaded,
            MAX_DISCARD_LEVEL,
            false,
            false,
            new LLHandle<LLPanel>(getHandle()),
            NULL,
            false);
    }

    LLFloater *floater = mFloaterProfileTextureHandle.get();
    if (floater)
    {
        LLFloaterProfileTexture * texture_view = dynamic_cast<LLFloaterProfileTexture*>(floater);
        if (mImageId.notNull())
        {
            texture_view->loadAsset(mImageId);
        }
        else
        {
            texture_view->resetAsset();
        }
    }

    setProfileImageUploading(false);
}

bool LLPanelProfileSecondLife::hasUnsavedChanges()
{
    LLFloater *floater = mFloaterPermissionsHandle.get();
    if (floater)
    {
        LLFloaterProfilePermissions* perm = dynamic_cast<LLFloaterProfilePermissions*>(floater);
        if (perm && perm->hasUnsavedChanges())
        {
            return true;
        }
    }
    // if floater
    return mHasUnsavedDescriptionChanges;
}

void LLPanelProfileSecondLife::commitUnsavedChanges()
{
    LLFloater *floater = mFloaterPermissionsHandle.get();
    if (floater)
    {
        LLFloaterProfilePermissions* perm = dynamic_cast<LLFloaterProfilePermissions*>(floater);
        if (perm && perm->hasUnsavedChanges())
        {
            perm->onApplyRights();
        }
    }
    if (mHasUnsavedDescriptionChanges)
    {
        onSaveDescriptionChanges();
    }
}

void LLPanelProfileSecondLife::fillCommonData(const LLAvatarData* avatar_data)
{
    // Refresh avatar id in cache with new info to prevent re-requests
    // and to make sure icons in text will be up to date
    LLAvatarIconIDCache::getInstance()->add(avatar_data->avatar_id, avatar_data->image_id);

    fillAgeData(avatar_data);

    setDescriptionText(avatar_data->about_text);

    if (avatar_data->image_id.notNull())
    {
        mSecondLifePic->setValue(avatar_data->image_id);
        mImageId = avatar_data->image_id;
    }
    else
    {
        mSecondLifePic->setValue("Generic_Person_Large");
        mImageId = LLUUID::null;
    }

    // Will be loaded as a LLViewerFetchedTexture::BOOST_UI due to mSecondLifePic
    LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(avatar_data->image_id);
    if (imagep->getFullHeight())
    {
        onImageLoaded(true, imagep);
    }
    else
    {
        imagep->setLoadedCallback(onImageLoaded,
                                  MAX_DISCARD_LEVEL,
                                  false,
                                  false,
                                  new LLHandle<LLPanel>(getHandle()),
                                  NULL,
                                  false);
    }

    if (getSelfProfile())
    {
        mAllowPublish = avatar_data->flags & AVATAR_ALLOW_PUBLISH;
        // <FS:Ansariel> Fix LL UI/UX design accident
        //mShowInSearchCombo->setValue(mAllowPublish);
        mShowInSearchCheckbox->setValue(mAllowPublish);
        // </FS:Ansariel>
    }
}

void LLPanelProfileSecondLife::fillPartnerData(const LLAvatarData* avatar_data)
{
    // <FS:Ansariel> Fix LL UI/UX design accident
    //LLTextBox* partner_text_ctrl = getChild<LLTextBox>("partner_link");
    LLTextEditor* partner_text_ctrl = getChild<LLTextEditor>("partner_link");
    if (avatar_data->partner_id.notNull())
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //childSetVisible("partner_layout", true);
        //LLStringUtil::format_map_t args;
        //args["[LINK]"] = LLSLURL("agent", avatar_data->partner_id, "inspect").getSLURLString();
        //std::string partner_text = getString("partner_text", args);
        //partner_text_ctrl->setText(partner_text);
        partner_text_ctrl->setText(LLSLURL("agent", avatar_data->partner_id, "inspect").getSLURLString());
        // </FS:Ansariel>
    }
    else
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //childSetVisible("partner_layout", false);
        partner_text_ctrl->setText(getString("no_partner_text"));
    }
}

void LLPanelProfileSecondLife::fillAccountStatus(const LLAvatarData* avatar_data)
{
    LLStringUtil::format_map_t args;
    args["[ACCTTYPE]"] = LLAvatarPropertiesProcessor::accountType(avatar_data);
    args["[PAYMENTINFO]"] = LLAvatarPropertiesProcessor::paymentInfo(avatar_data);

    // <FS:Ansariel> FSData support
    args["[FIRESTORM]"] = "";
    args["[FSSUPP]"] = "";
    args["[FSDEV]"] = "";
    args["[FSQA]"] = "";
    args["[FSGW]"] = "";
    S32 flags = FSData::getInstance()->getAgentFlags(avatar_data->avatar_id);
    if (flags != -1)
    {
        bool separator = false;
        std::string text;
        if (flags & (FSData::DEVELOPER | FSData::SUPPORT | FSData::QA | FSData::GATEWAY))
        {
            args["[FIRESTORM]"] = LLTrans::getString("APP_NAME");
        }

        if (flags & FSData::DEVELOPER)
        {
            text = getString("FSDev");
            args["[FSDEV]"] = text;
            separator = true;
        }

        if (flags & FSData::SUPPORT)
        {
            text = getString("FSSupp");
            if (separator)
            {
                text = " /" + text;
            }
            args["[FSSUPP]"] = text;
            separator = true;
        }

        if (flags & FSData::QA)
        {
            text = getString("FSQualityAssurance");
            if (separator)
            {
                text = " /" + text;
            }
            args["[FSQA]"] = text;
            separator = true;
        }

        if (flags & FSData::GATEWAY)
        {
            text = getString("FSGW");
            if (separator)
            {
                text = " /" + text;
            }
            args["[FSGW]"] = text;
        }
    }
    // </FS:Ansariel>

    std::string caption_text = getString("CaptionTextAcctInfo", args);
    getChild<LLUICtrl>("account_info")->setValue(caption_text);

    constexpr S32 LINDEN_EMPLOYEE_INDEX = 3;
    LLDate sl_release;
    sl_release.fromYMDHMS(2003, 6, 23, 0, 0, 0);
    std::string customer_lower = avatar_data->customer_type;
    LLStringUtil::toLower(customer_lower);
    if (avatar_data->caption_index == LINDEN_EMPLOYEE_INDEX)
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Linden");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgeLinden"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Linden", "BadgeLinden", BadgeLocation::bottom);
    }
    else if (avatar_data->born_on < sl_release)
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Beta");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgeBeta"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Beta", "BadgeBeta", BadgeLocation::bottom);
    }
    else if (customer_lower == "beta_lifetime")
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Beta_Lifetime");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgeBetaLifetime"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Beta_Lifetime", "BadgeBetaLifetime", BadgeLocation::bottom);
    }
    else if (customer_lower == "lifetime")
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Lifetime");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgeLifetime"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Lifetime", "BadgeLifetime", BadgeLocation::bottom);
    }
    else if (customer_lower == "secondlifetime_premium")
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Premium_Lifetime");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgePremiumLifetime"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Premium_Lifetime", "BadgePremiumLifetime", BadgeLocation::bottom);
    }
    else if (customer_lower == "secondlifetime_premium_plus")
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //getChild<LLUICtrl>("badge_icon")->setValue("Profile_Badge_Pplus_Lifetime");
        //getChild<LLUICtrl>("badge_text")->setValue(getString("BadgePremiumPlusLifetime"));
        //childSetVisible("badge_layout", true);
        //childSetVisible("partner_spacer_layout", false);
        setBadge("Profile_Badge_Pplus_Lifetime", "BadgePremiumPlusLifetime", BadgeLocation::bottom);
    }
    else
    {
        childSetVisible("badge_layout", false);
        // <FS:Ansariel> Fix LL UI/UX design accident
        //childSetVisible("partner_spacer_layout", true);
    }

    // <FS:Ansariel> Add Firestorm team badge
    if (FSData::getInstance()->getAgentFlags(avatar_data->avatar_id) != -1)
    {
        setBadge("Profile_Badge_Team", "BadgeTeam", BadgeLocation::top);
    }
    // </FS:Ansariel>
}

// <FS:Ansariel> Fix LL UI/UX design accident
void LLPanelProfileSecondLife::setBadge(std::string_view icon_name, std::string_view tooltip, BadgeLocation location)
{
    auto iconctrl = getChild<LLIconCtrl>(location == BadgeLocation::top ? "top_badge_icon" : "bottom_badge_icon");
    iconctrl->setValue(icon_name.data());
    iconctrl->setToolTip(getString(tooltip.data()));
    childSetVisible(location == BadgeLocation::top ? "top_badge_layout" : "bottom_badge_layout", true);
    childSetVisible("badge_layout", true);
}
// </FS:Ansariel>

void LLPanelProfileSecondLife::fillRightsData()
{
    if (getSelfProfile())
    {
        return;
    }

    const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    // If true - we are viewing friend's profile, enable check boxes and set values.
    if (relation)
    {
        const S32 rights = relation->getRightsGrantedTo();
        bool can_see_online = LLRelationship::GRANT_ONLINE_STATUS & rights;
        bool can_see_on_map = LLRelationship::GRANT_MAP_LOCATION & rights;
        bool can_edit_objects = LLRelationship::GRANT_MODIFY_OBJECTS & rights;

        mCanSeeOnlineIcon->setVisible(can_see_online);
        mCantSeeOnlineIcon->setVisible(!can_see_online);
        mCanSeeOnMapIcon->setVisible(can_see_on_map);
        mCantSeeOnMapIcon->setVisible(!can_see_on_map);
        mCanEditObjectsIcon->setVisible(can_edit_objects);
        mCantEditObjectsIcon->setVisible(!can_edit_objects);

        mCanSeeOnlineIcon->setEnabled(true);
        mCantSeeOnlineIcon->setEnabled(true);
        mCanSeeOnMapIcon->setEnabled(true);
        mCantSeeOnMapIcon->setEnabled(true);
        mCanEditObjectsIcon->setEnabled(true);
        mCantEditObjectsIcon->setEnabled(true);
    }
    else
    {
        mCanSeeOnlineIcon->setVisible(false);
        mCantSeeOnlineIcon->setVisible(false);
        mCanSeeOnMapIcon->setVisible(false);
        mCantSeeOnMapIcon->setVisible(false);
        mCanEditObjectsIcon->setVisible(false);
        mCantEditObjectsIcon->setVisible(false);
    }
}

void LLPanelProfileSecondLife::fillAgeData(const LLAvatarData* avatar_data)
{
    // <FS:Ansariel> Fix LL UI/UX design accident
    //// Date from server comes already converted to stl timezone,
    //// so display it as an UTC + 0
    //bool hide_age = avatar_data->hide_age && !getSelfProfile();
    //std::string name_and_date = getString(hide_age ? "date_format_short" : "date_format_full");
    //LLSD args_name;
    //args_name["datetime"] = (S32)avatar_data->born_on.secondsSinceEpoch();
    //LLStringUtil::format(name_and_date, args_name);
    //getChild<LLUICtrl>("sl_birth_date")->setValue(name_and_date);

    //LLUICtrl* userAgeCtrl = getChild<LLUICtrl>("user_age");
    //if (hide_age)
    //{
    //    userAgeCtrl->setVisible(false);
    //}
    //else
    //{
    //    std::string register_date = getString("age_format");
    //    LLSD args_age;
    //    args_age["[AGE]"] = LLDateUtil::ageFromDate(avatar_data->born_on, LLDate::now());
    //    LLStringUtil::format(register_date, args_age);
    //    userAgeCtrl->setValue(register_date);
    //}

    std::string register_date = getString((!getSelfProfile() && avatar_data->hide_age) ? "age_format_short" : "age_format_full");
    LLSD args_age;
    std::string birth_date = LLTrans::getString(!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty() ? ((!getSelfProfile() && avatar_data->hide_age) ? "AvatarBirthDateFormatShort" : "AvatarBirthDateFormatFull") : "AvatarBirthDateFormat_legacy");
    LLStringUtil::format(birth_date, LLSD().with("datetime", (S32)avatar_data->born_on.secondsSinceEpoch()));
    args_age["[REG_DATE]"] = birth_date;
    args_age["[AGE]"] = LLDateUtil::ageFromDate(avatar_data->born_on, LLDate::now());
    LLStringUtil::format(register_date, args_age);
    getChild<LLUICtrl>("user_age")->setValue(register_date);
    // </FS:Ansariel>

    bool showHideAgeCombo = false;
    if (getSelfProfile())
    {
        if (LLAvatarPropertiesProcessor::getInstance()->isHideAgeSupportedByServer())
        {
            F64 birth = avatar_data->born_on.secondsSinceEpoch();
            F64 now = LLDate::now().secondsSinceEpoch();
            if (now - birth > 365 * 24 * 60 * 60)
            {
                mHideAge = avatar_data->hide_age;
                // <FS:Ansariel> Fix LL UI/UX design accident
                //mHideAgeCombo->setValue(mHideAge);
                mHideAgeCheckbox->setValue(mHideAge);
                showHideAgeCombo = true;
            }
        }
    }
    // <FS:Ansariel> Fix LL UI/UX design accident
    //mHideAgeCombo->setVisible(showHideAgeCombo);
    mHideAgeCheckbox->setVisible(showHideAgeCombo);
}

void LLPanelProfileSecondLife::onImageLoaded(bool success, LLViewerFetchedTexture *imagep)
{
    // <FS:Ansariel> Fix LL UI/UX design accident
    //LLRect imageRect = mSecondLifePicLayout->getRect();
    LLRect imageRect = mSecondLifePic->getRect();
    if (!success || imagep->getFullWidth() == imagep->getFullHeight())
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //mSecondLifePicLayout->reshape(imageRect.getWidth(), imageRect.getWidth());
        mSecondLifePicLayout->reshape(imageRect.getHeight(), imageRect.getHeight());
    }
    else
    {
        // assume 3:4, for sake of firestorm
        // <FS:Ansariel> Fix LL UI/UX design accident
        //mSecondLifePicLayout->reshape(imageRect.getWidth(), imageRect.getWidth() * 3 / 4);
        mSecondLifePicLayout->reshape(imageRect.getHeight() * 4 / 3, imageRect.getHeight());
    }
}

//static
void LLPanelProfileSecondLife::onImageLoaded(bool success,
                                             LLViewerFetchedTexture *src_vi,
                                             LLImageRaw* src,
                                             LLImageRaw* aux_src,
                                             S32 discard_level,
                                             bool final,
                                             void* userdata)
{
    if (!userdata) return;

    LLHandle<LLPanel>* handle = (LLHandle<LLPanel>*)userdata;

    if (!handle->isDead())
    {
        LLPanelProfileSecondLife* panel = static_cast<LLPanelProfileSecondLife*>(handle->get());
        if (panel)
        {
            panel->onImageLoaded(success, src_vi);
        }
    }

    if (final || !success)
    {
        delete handle;
    }
}

// virtual, called by LLAvatarTracker
void LLPanelProfileSecondLife::changed(U32 mask)
{
    updateOnlineStatus();
    if (mask != LLFriendObserver::ONLINE)
    {
        fillRightsData();
    }

    updateButtons(); // <FS:Ansariel> Fix LL UI/UX design accident
}

// virtual, called by LLVoiceClient
void LLPanelProfileSecondLife::onChange(EStatusType status, const LLSD& channelInfo, bool proximal)
{
    if(status == STATUS_JOINING || status == STATUS_LEFT_CHANNEL)
    {
        return;
    }

    mVoiceStatus = LLAvatarActions::canCall() && (LLAvatarActions::isFriend(getAvatarId()) ? LLAvatarTracker::instance().isBuddyOnline(getAvatarId()) : true);
}

void LLPanelProfileSecondLife::setAvatarId(const LLUUID& avatar_id)
{
    if (avatar_id.notNull())
    {
        if (getAvatarId().notNull())
        {
            LLAvatarTracker::instance().removeParticularFriendObserver(getAvatarId(), this);
        }

        LLPanelProfilePropertiesProcessorTab::setAvatarId(avatar_id);

        if (LLAvatarActions::isFriend(getAvatarId()))
        {
            LLAvatarTracker::instance().addParticularFriendObserver(getAvatarId(), this);
        }
    }
}

// method was disabled according to EXT-2022. Re-enabled & improved according to EXT-3880
void LLPanelProfileSecondLife::updateOnlineStatus()
{
    const LLRelationship* relationship = LLAvatarTracker::instance().getBuddyInfo(getAvatarId());
    if (relationship)
    {
        // For friend let check if he allowed me to see his status
        bool online = relationship->isOnline();
        bool perm_granted = relationship->isRightGrantedFrom(LLRelationship::GRANT_ONLINE_STATUS);
        processOnlineStatus(true, perm_granted, online);
    }
    // <FS:Ansariel> Fix LL UI/UX design accident
    //else
    //{
    //    childSetVisible("friend_layout", false);
    //    childSetVisible("online_layout", false);
    //    childSetVisible("offline_layout", false);
    //}
    // </FS:Ansariel>
}

void LLPanelProfileSecondLife::processOnlineStatus(bool is_friend, bool show_online, bool online)
{
    // <FS:Zi> FIRE-32184: Online/Offline status not working for non-friends
    // being a friend who doesn't show online status and appears online can't happen
    // so this is our marker for "undefined"
    if (is_friend && !show_online && online)
    {
        mStatusText->setValue(getString("status_unknown"));
        mStatusText->setColor(LLUIColorTable::getInstance()->getColor("StatusUserUnknown"));

        mPropertiesObserver.mPanelProfile = this;
        mPropertiesObserver.mRequester = gAgentID;
        LLAvatarPropertiesProcessor::getInstance()->addObserver(getAvatarId(), &mPropertiesObserver);
        LLAvatarPropertiesProcessor::getInstance()->sendAvatarLegacyPropertiesRequest(getAvatarId());

        return;
    }
    // </FS:Zi>
    // <FS:Ansariel> Fix LL UI/UX design accident
    //childSetVisible("friend_layout", is_friend);
    //childSetVisible("online_layout", online && show_online);
    //childSetVisible("offline_layout", !online && show_online);
    // <FS:Zi> Always show the online status text, just set it to "offline" when a friend is hiding
    // mStatusText->setVisible(show_online);

    std::string status = getString(online ? "status_online" : "status_offline");

    mStatusText->setValue(status);
    mStatusText->setColor(online ?
        LLUIColorTable::instance().getColor("StatusUserOnline") :
        LLUIColorTable::instance().getColor("StatusUserOffline"));
    // </FS:Ansariel>
}

void LLPanelProfileSecondLife::setLoaded()
{
    LLPanelProfileTab::setLoaded();

    if (getSelfProfile())
    {
        // <FS:Ansariel> Fix LL UI/UX design accident
        //mShowInSearchCombo->setEnabled(true);
        //if (mHideAgeCombo->getVisible())
        //{
        //    mHideAgeCombo->setEnabled(true);
        mShowInSearchCheckbox->setEnabled(true);
        mPreviewButton->setEnabled(true); // <AS:Chanayane> Preview button
        if (mHideAgeCheckbox->getVisible())
        {
            mHideAgeCheckbox->setEnabled(true);
        // </FS:Ansariel>
        }
        mDescriptionEdit->setEnabled(true);
    }
}

// <FS:Ansariel> Fix LL UI/UX design accident
void LLPanelProfileSecondLife::updateButtons()
{
    if (getSelfProfile())
    {
        mShowInSearchCheckbox->setVisible(true);
        mShowInSearchCheckbox->setEnabled(true);
// <AS:Chanayane> Preview button
        mPreviewButton->setVisible(true);
        mPreviewButton->setEnabled(true);
// </AS:Chanayane>
        mDescriptionEdit->setEnabled(true);
    }
    else
    {
        LLUUID av_id = getAvatarId();
        bool is_buddy_online = LLAvatarTracker::instance().isBuddyOnline(getAvatarId());

        if (LLAvatarActions::isFriend(av_id))
        {
            // <FS:Ansariel> RLVa support
            //mTeleportButton->setEnabled(is_buddy_online);
            const LLRelationship* friend_status = LLAvatarTracker::instance().getBuddyInfo(av_id);
            bool can_offer_tp = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) ||
                (gRlvHandler.isException(RLV_BHVR_TPLURE, av_id, ERlvExceptionCheck::Permissive) ||
                    friend_status->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION)));

            mTeleportButton->setEnabled(is_buddy_online && can_offer_tp);
            // </FS:Ansariel>
            //Disable "Add Friend" button for friends.
            // <FS:Zi> Add "Remove Friend" button to profile
            // mAddFriendButton->setEnabled(false);
            mAddFriendButton->setVisible(false);
            mRemoveFriendButton->setVisible(true);
            // </FS:Zi>
        }
        else
        {
            // <FS:Ansariel> RLVa support
            //mTeleportButton->setEnabled(true);
            bool can_offer_tp = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC) ||
                gRlvHandler.isException(RLV_BHVR_TPLURE, av_id, ERlvExceptionCheck::Permissive));
            mTeleportButton->setEnabled(can_offer_tp);
            // </FS:Ansariel>
            // <FS:Zi> Add "Remove Friend" button to profile
            // mAddFriendButton->setEnabled(true);
            mAddFriendButton->setVisible(true);
            mRemoveFriendButton->setVisible(false);
            // </FS:Zi>
        }

        // <FS:Ansariel> RLVa support
        //bool enable_map_btn = (is_buddy_online && is_agent_mappable(av_id)) || gAgent.isGodlike();
        bool enable_map_btn = ((is_buddy_online && is_agent_mappable(av_id)) || gAgent.isGodlike()) && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP);
        // </FS:Ansariel>
        mShowOnMapButton->setEnabled(enable_map_btn);

        bool enable_block_btn = LLAvatarActions::canBlock(av_id) && !LLAvatarActions::isBlocked(av_id);
        mBlockButton->setVisible(enable_block_btn);

        bool enable_unblock_btn = LLAvatarActions::isBlocked(av_id);
        mUnblockButton->setVisible(enable_unblock_btn);
    }
}
// </FS:Ansariel>

class LLProfileImagePicker : public LLFilePickerThread
{
public:
    LLProfileImagePicker(EProfileImageType type, LLHandle<LLPanel> *handle);
    ~LLProfileImagePicker();
    void notify(const std::vector<std::string>& filenames) override;

private:
    LLHandle<LLPanel> *mHandle;
    EProfileImageType mType;
};

LLProfileImagePicker::LLProfileImagePicker(EProfileImageType type, LLHandle<LLPanel> *handle)
    : LLFilePickerThread(LLFilePicker::FFLOAD_IMAGE),
    mHandle(handle),
    mType(type)
{
}

LLProfileImagePicker::~LLProfileImagePicker()
{
    delete mHandle;
}

void LLProfileImagePicker::notify(const std::vector<std::string>& filenames)
{
    if (mHandle->isDead())
    {
        return;
    }
    if (filenames.empty())
    {
        return;
    }
    std::string file_path = filenames[0];
    if (file_path.empty())
    {
        return;
    }

    // generate a temp texture file for coroutine
    std::string temp_file = gDirUtilp->getTempFilename();
    U32 codec = LLImageBase::getCodecFromExtension(gDirUtilp->getExtension(file_path));
    constexpr S32 MAX_DIM = 256;
    if (!LLViewerTextureList::createUploadFile(file_path, temp_file, codec, MAX_DIM))
    {
        LLSD notif_args;
        notif_args["REASON"] = LLImage::getLastThreadError().c_str();
        LLNotificationsUtil::add("CannotUploadTexture", notif_args);
        LL_WARNS("AvatarProperties") << "Failed to upload profile image of type " << (S32)mType << ", " << notif_args["REASON"].asString() << LL_ENDL;
        return;
    }

    std::string cap_url = gAgent.getRegionCapability(PROFILE_IMAGE_UPLOAD_CAP);
    if (cap_url.empty())
    {
        LLSD args;
        args["CAPABILITY"] = PROFILE_IMAGE_UPLOAD_CAP;
        LLNotificationsUtil::add("RegionCapabilityRequestError", args);
        LL_WARNS("AvatarProperties") << "Failed to upload profile image of type " << (S32)mType << ", no cap found" << LL_ENDL;
        return;
    }

    switch (mType)
    {
    case PROFILE_IMAGE_SL:
        {
            LLPanelProfileSecondLife* panel = static_cast<LLPanelProfileSecondLife*>(mHandle->get());
            panel->setProfileImageUploading(true);
        }
        break;
    case PROFILE_IMAGE_FL:
        {
            LLPanelProfileFirstLife* panel = static_cast<LLPanelProfileFirstLife*>(mHandle->get());
            panel->setProfileImageUploading(true);
        }
        break;
    }

    LLCoros::instance().launch("postAgentUserImageCoro",
        boost::bind(post_profile_image_coro, cap_url, mType, temp_file, mHandle));

    mHandle = nullptr; // transferred to post_profile_image_coro
}

void LLPanelProfileSecondLife::onCommitMenu(const LLSD& userdata)
{
    const std::string item_name = userdata.asString();
    const LLUUID agent_id = getAvatarId();
    // todo: consider moving this into LLAvatarActions::onCommit(name, id)
    // and making all other flaoters, like people menu do the same
    if (item_name == "im")
    {
        LLAvatarActions::startIM(agent_id);
    }
    else if (item_name == "offer_teleport")
    {
        LLAvatarActions::offerTeleport(agent_id);
    }
    else if (item_name == "request_teleport")
    {
        LLAvatarActions::teleportRequest(agent_id);
    }
    else if (item_name == "voice_call")
    {
        LLAvatarActions::startCall(agent_id);
    }
    else if (item_name == "chat_history")
    {
        LLAvatarActions::viewChatHistory(agent_id);
    }
    else if (item_name == "add_friend")
    {
        LLAvatarActions::requestFriendshipDialog(agent_id);
    }
    else if (item_name == "remove_friend")
    {
        LLAvatarActions::removeFriendDialog(agent_id);
    }
    else if (item_name == "invite_to_group")
    {
        LLAvatarActions::inviteToGroup(agent_id);
    }
    else if (item_name == "can_show_on_map")
    {
        LLAvatarActions::showOnMap(agent_id);
    }
    else if (item_name == "share")
    {
        LLAvatarActions::share(agent_id);
    }
    else if (item_name == "pay")
    {
        LLAvatarActions::pay(agent_id);
    }
    else if (item_name == "toggle_block_agent")
    {
        // <FS:PP> Swap block/unblock buttons properly
        // LLAvatarActions::toggleBlock(agent_id);
        bool is_blocked = LLAvatarActions::toggleBlock(agent_id);
        mBlockButton->setVisible(!is_blocked);
        mUnblockButton->setVisible(is_blocked);
        // </FS:PP>
    }
    else if (item_name == "copy_user_id")
    {
        LLWString wstr = utf8str_to_wstring(getAvatarId().asString());
        LLClipboard::instance().copyToClipboard(wstr, 0, static_cast<S32>(wstr.size()));
    }
    else if (item_name == "agent_permissions")
    {
        onShowAgentPermissionsDialog();
    }
    else if (item_name == "copy_display_name"
        || item_name == "copy_username")
    {
        LLAvatarName av_name;
        if (!LLAvatarNameCache::get(getAvatarId(), &av_name))
        {
            // shouldn't happen, option is supposed to be invisible while name is fetching
            LL_WARNS() << "Failed to get agent data" << LL_ENDL;
            return;
        }
        LLWString wstr;
        if (item_name == "copy_display_name")
        {
            wstr = utf8str_to_wstring(av_name.getDisplayName(true));
        }
        else if (item_name == "copy_username")
        {
            wstr = utf8str_to_wstring(av_name.getUserName());
        }
        LLClipboard::instance().copyToClipboard(wstr, 0, static_cast<S32>(wstr.size()));
    }
    else if (item_name == "edit_display_name")
    {
        LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileSecondLife::onAvatarNameCacheSetName, this, _1, _2));
        LLFirstUse::setDisplayName(false);
    }
    else if (item_name == "edit_partner")
    {
        std::string url = "https://[GRID]/my/account/partners.php";
        LLSD subs;
        url = LLWeb::expandURLSubstitutions(url, subs);
        LLUrlAction::openURL(url);
    }
    else if (item_name == "upload_photo")
    {
        (new LLProfileImagePicker(PROFILE_IMAGE_SL, new LLHandle<LLPanel>(getHandle())))->getFile();

        LLFloater* floaterp = mFloaterTexturePickerHandle.get();
        if (floaterp)
        {
            floaterp->closeFloater();
        }
    }
    else if (item_name == "change_photo")
    {
        onShowTexturePicker();
    }
    else if (item_name == "remove_photo")
    {
        onCommitProfileImage(LLUUID::null);

        LLFloater* floaterp = mFloaterTexturePickerHandle.get();
        if (floaterp)
        {
            floaterp->closeFloater();
        }
    }
    // <FS:Ansariel> Fix LL UI/UX design accident
    else if (item_name == "add_to_contact_set")
    {
        LLAvatarActions::addToContactSet(agent_id);
    }
    else if (item_name == "copy_uri")
    {
        LLWString wstr = utf8str_to_wstring(LLSLURL("agent", agent_id, "about").getSLURLString());
        LLClipboard::instance().copyToClipboard(wstr, 0, static_cast<S32>(wstr.size()));
    }
    else if (item_name == "kick")
    {
        LLAvatarActions::kick(agent_id);
    }
    else if (item_name == "freeze")
    {
        LLAvatarActions::freeze(agent_id);
    }
    else if (item_name == "unfreeze")
    {
        LLAvatarActions::unfreeze(agent_id);
    }
    else if (item_name == "csr")
    {
        LLAvatarName av_name;
        if (!LLAvatarNameCache::get(getAvatarId(), &av_name))
        {
            // shouldn't happen, option is supposed to be invisible while name is fetching
            LL_WARNS() << "Failed to get agent data" << LL_ENDL;
            return;
        }
        LLAvatarActions::csr(getAvatarId(), av_name.getUserName());
    }
    else if (item_name == "report")
    {
        LLAvatarActions::report(agent_id);
    }
    // </FS:Ansariel>
    // <AS:Chanayane> Preview button
    else if (item_name == "preview")
    {
        mPreview = !mPreview;

        mDescriptionEdit->setEnabled(!mPreview);
        mDescriptionEdit->setParseHTML(mPreview);

        if (mPreview) {
            mPreviewButton->setImageOverlay("Profile_Group_Visibility_Off");
            if (mHasUnsavedDescriptionChanges) {
                mSaveDescriptionChanges->setEnabled(false);
                mDiscardDescriptionChanges->setEnabled(false);
            }
            mOriginalDescriptionText = mDescriptionEdit->getValue().asString();
            reparseDescriptionText(mOriginalDescriptionText);
        } else {
            mPreviewButton->setImageOverlay("Profile_Group_Visibility_On");
            if (mHasUnsavedDescriptionChanges) {
                mSaveDescriptionChanges->setEnabled(true);
                mDiscardDescriptionChanges->setEnabled(true);
            }
            reparseDescriptionText(mOriginalDescriptionText);
        }
    }
    // </AS:Chanayane>
}

bool LLPanelProfileSecondLife::onEnableMenu(const LLSD& userdata)
{
    const std::string item_name = userdata.asString();
    const LLUUID agent_id = getAvatarId();
    if (item_name == "offer_teleport" || item_name == "request_teleport")
    {
        return LLAvatarActions::canOfferTeleport(agent_id);
    }
    else if (item_name == "voice_call")
    {
        return mVoiceStatus;
    }
    else if (item_name == "chat_history")
    {
        return LLLogChat::isTranscriptExist(agent_id);
    }
    else if (item_name == "add_friend")
    {
        return !LLAvatarActions::isFriend(agent_id);
    }
    else if (item_name == "remove_friend")
    {
        return LLAvatarActions::isFriend(agent_id);
    }
    else if (item_name == "can_show_on_map")
    {
        // <FS:Ansariel> RLVa
        //return (LLAvatarTracker::instance().isBuddyOnline(agent_id) && is_agent_mappable(agent_id))
        //|| gAgent.isGodlike();
         return ((LLAvatarTracker::instance().isBuddyOnline(agent_id) && is_agent_mappable(agent_id))
        || gAgent.isGodlike()) && !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP);
        // </FS:Ansariel>
   }
    else if (item_name == "toggle_block_agent")
    {
        return LLAvatarActions::canBlock(agent_id);
    }
    else if (item_name == "agent_permissions")
    {
        return LLAvatarActions::isFriend(agent_id);
    }
    else if (item_name == "copy_display_name"
        || item_name == "copy_username")
    {
        return !mAvatarNameCacheConnection.connected();
    }
    else if (item_name == "upload_photo"
        || item_name == "change_photo")
    {
        std::string cap_url = gAgent.getRegionCapability(PROFILE_IMAGE_UPLOAD_CAP);
        return !cap_url.empty() && !mWaitingForImageUpload && getIsLoaded();
    }
    else if (item_name == "remove_photo")
    {
        std::string cap_url = gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP);
        return mImageId.notNull() && !cap_url.empty() && !mWaitingForImageUpload && getIsLoaded();
    }
    // <FS:Ansariel> Fix LL UI/UX design accident
    else if (item_name == "kick" || item_name == "freeze" || item_name == "unfreeze" || item_name == "csr")
    {
        return gAgent.isGodlike();
    }
    // </FS:Ansariel>

    return false;
}

bool LLPanelProfileSecondLife::onCheckMenu(const LLSD& userdata)
{
    const std::string item_name = userdata.asString();
    const LLUUID agent_id = getAvatarId();
    if (item_name == "toggle_block_agent")
    {
        return LLAvatarActions::isBlocked(agent_id);
    }
    return false;
}

void LLPanelProfileSecondLife::onAvatarNameCacheSetName(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    if (av_name.getDisplayName().empty())
    {
        // something is wrong, tell user to try again later
        LLNotificationsUtil::add("SetDisplayNameFailedGeneric");
        return;
    }

    LL_INFOS("LegacyProfile") << "name-change now " << LLDate::now() << " next_update "
        << LLDate(av_name.mNextUpdate) << LL_ENDL;
    F64 now_secs = LLDate::now().secondsSinceEpoch();

    if (now_secs < av_name.mNextUpdate)
    {
        // if the update time is more than a year in the future, it means updates have been blocked
        // show a more general message
        static const S32 YEAR = 60*60*24*365;
        if (now_secs + YEAR < av_name.mNextUpdate)
        {
            LLNotificationsUtil::add("SetDisplayNameBlocked");
            return;
        }
    }

    LLFloaterReg::showInstance("display_name");
}

void LLPanelProfileSecondLife::setDescriptionText(const std::string &text)
{
    mSaveDescriptionChanges->setEnabled(false);
    mDiscardDescriptionChanges->setEnabled(false);
    mHasUnsavedDescriptionChanges = false;

    mDescriptionText = text;
    mDescriptionEdit->setValue(mDescriptionText);
}

// <AS:Chanayane> Preview button
void LLPanelProfileSecondLife::reparseDescriptionText(const std::string &text)
{
    mDescriptionEdit->reparseValue(text);
}
// </AS:Chanayane>

void LLPanelProfileSecondLife::onSetDescriptionDirty()
{
    mSaveDescriptionChanges->setEnabled(true);
    mDiscardDescriptionChanges->setEnabled(true);
    mHasUnsavedDescriptionChanges = true;
}

void LLPanelProfileSecondLife::onShowInSearchCallback()
{
    // <FS:Ansariel> Fix LL UI/UX design accident
    //bool value = mShowInSearchCombo->getValue().asInteger();
    bool value = mShowInSearchCheckbox->getValue().asInteger();
    // </FS:Ansariel>
    if (mAllowPublish == value)
        return;

    mAllowPublish = value;
    saveAgentUserInfoCoro("allow_publish", value);
}

void LLPanelProfileSecondLife::onHideAgeCallback()
{
    // <FS:Ansariel> Fix LL UI/UX design accident
    //bool value = mHideAgeCombo->getValue().asInteger();
    bool value = !mHideAgeCheckbox->getValue().asBoolean();
    // </FS:Ansariel>
    if (value == mHideAge)
        return;

    mHideAge = value;
    saveAgentUserInfoCoro("hide_age", value);
}

void LLPanelProfileSecondLife::onSaveDescriptionChanges()
{
    mDescriptionText = mDescriptionEdit->getValue().asString();
    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        saveAgentUserInfoCoro("sl_about_text", mDescriptionText);
    }
// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
    else if (LLGridManager::getInstance()->isInOpenSim())
    {
        if (getIsLoaded() && getSelfProfile())
        {
            LLFloater* floater_profile = LLFloaterReg::findInstance("profile", LLSD().with("id", gAgentID));
            if (!floater_profile)
            {
                // floater is dead, so panels are dead as well
                return;
            }
            LLPanelProfile* panel_profile = floater_profile->findChild<LLPanelProfile>(PANEL_PROFILE_VIEW, true);
            if (!panel_profile)
            {
                LL_WARNS() << PANEL_PROFILE_VIEW << " not found" << LL_ENDL;
            }
            else
            {
                auto avatar_data = panel_profile->getAvatarData();
                avatar_data.agent_id = gAgentID;
                avatar_data.avatar_id = gAgentID;
                avatar_data.image_id = mImageId;
                avatar_data.about_text = mDescriptionEdit->getValue().asString();
                avatar_data.allow_publish = mShowInSearchCheckbox->getValue();

                LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(&avatar_data);
            }
        }
    }
#endif
// </FS:Beq>

    mSaveDescriptionChanges->setEnabled(false);
    mDiscardDescriptionChanges->setEnabled(false);
    mHasUnsavedDescriptionChanges = false;
}

void LLPanelProfileSecondLife::onDiscardDescriptionChanges()
{
    setDescriptionText(mDescriptionText);
}

void LLPanelProfileSecondLife::onShowAgentPermissionsDialog()
{
    LLFloater* floater = mFloaterPermissionsHandle.get();
    if (!floater)
    {
        LLFloater* parent_floater = gFloaterView->getParentFloater(this);
        if (parent_floater)
        {
            LLFloaterProfilePermissions * perms = new LLFloaterProfilePermissions(parent_floater, getAvatarId());
            mFloaterPermissionsHandle = perms->getHandle();
            perms->openFloater();
            perms->setVisibleAndFrontmost(true);

            parent_floater->addDependentFloater(mFloaterPermissionsHandle);
        }
    }
    else // already open
    {
        floater->setMinimized(false);
        floater->setVisibleAndFrontmost(true);
    }
}

void LLPanelProfileSecondLife::onShowAgentProfileTexture()
{
    if (!getIsLoaded())
    {
        return;
    }

    LLFloater* floater = mFloaterProfileTextureHandle.get();
    if (!floater)
    {
        LLFloater* parent_floater = gFloaterView->getParentFloater(this);
        if (parent_floater)
        {
            LLFloaterProfileTexture * texture_view = new LLFloaterProfileTexture(parent_floater);
            mFloaterProfileTextureHandle = texture_view->getHandle();
            if (mImageId.notNull())
            {
                texture_view->loadAsset(mImageId);
            }
            else
            {
                texture_view->resetAsset();
            }
            texture_view->openFloater();
            texture_view->setVisibleAndFrontmost(true);

            parent_floater->addDependentFloater(mFloaterProfileTextureHandle);
        }
    }
    else // already open
    {
        LLFloaterProfileTexture * texture_view = dynamic_cast<LLFloaterProfileTexture*>(floater);
        texture_view->setMinimized(false);
        texture_view->setVisibleAndFrontmost(true);
        if (mImageId.notNull())
        {
            texture_view->loadAsset(mImageId);
        }
        else
        {
            texture_view->resetAsset();
        }
    }
}

void LLPanelProfileSecondLife::onShowTexturePicker()
{
    LLFloater* floaterp = mFloaterTexturePickerHandle.get();

    // Show the dialog
    if (!floaterp)
    {
        LLFloater* parent_floater = gFloaterView->getParentFloater(this);
        if (parent_floater)
        {
            // because inventory construction is somewhat slow
            getWindow()->setCursor(UI_CURSOR_WAIT);
            LLFloaterTexturePicker* texture_floaterp = new LLFloaterTexturePicker(
                this,
                mImageId,
                LLUUID::null,
                mImageId,
                false,
                false,
                getString("texture_picker_label"), // "SELECT PHOTO", // <FS:Ansariel> Fix LL UI/UX design accident
                PERM_NONE,
                PERM_NONE,
                false,
                NULL,
                PICK_TEXTURE);

            mFloaterTexturePickerHandle = texture_floaterp->getHandle();

            texture_floaterp->setOnFloaterCommitCallback([this](LLTextureCtrl::ETexturePickOp op, LLPickerSource source, const LLUUID& asset_id, const LLUUID&, const LLUUID&)
            {
                if (op == LLTextureCtrl::TEXTURE_SELECT)
                {
                    onCommitProfileImage(asset_id);
                }
            });
            texture_floaterp->setLocalTextureEnabled(false);
            texture_floaterp->setBakeTextureEnabled(false);
            texture_floaterp->setCanApply(false, true, false);

            parent_floater->addDependentFloater(mFloaterTexturePickerHandle);

            texture_floaterp->openFloater();
            texture_floaterp->setFocus(true);
        }
    }
    else
    {
        floaterp->setMinimized(false);
        floaterp->setVisibleAndFrontmost(true);
    }
}

// <FS:Zi> Allow proper texture swatch handling
void LLPanelProfileSecondLife::onSecondLifePicChanged()
{
    onCommitProfileImage(mSecondLifePic->getImageAssetID());
}
// </FS:Zi>

void LLPanelProfileSecondLife::onCommitProfileImage(const LLUUID& id)
{
    if (mImageId == id)
        return;

    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        std::function<void(bool)> callback = [id](bool result)
        {
            if (result)
            {
                LLAvatarIconIDCache::getInstance()->add(gAgentID, id);
                // Should trigger callbacks in icon controls
                LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(gAgentID);
            }
        };

        if (!saveAgentUserInfoCoro("sl_image_id", id, callback))
            return;

        mImageId = id;
        if (mImageId == LLUUID::null)
        {
            mSecondLifePic->setValue("Generic_Person_Large");
        }
        else
        {
            mSecondLifePic->setValue(mImageId);
        }

        // <FS:Ansariel> Fix LL UI/UX design accident
        LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(mImageId);
        if (imagep->getFullHeight())
        {
            onImageLoaded(true, imagep);
        }
        else
        {
            imagep->setLoadedCallback(onImageLoaded,
                MAX_DISCARD_LEVEL,
                false,
                false,
                new LLHandle<LLPanel>(getHandle()),
                NULL,
                false);
        }
        // </FS:Ansariel>

        LLFloater *floater = mFloaterProfileTextureHandle.get();
        if (floater)
        {
            LLFloaterProfileTexture * texture_view = dynamic_cast<LLFloaterProfileTexture*>(floater);
            if (mImageId == LLUUID::null)
            {
                texture_view->resetAsset();
            }
            else
            {
                texture_view->loadAsset(mImageId);
            }
        }
    }
// <FS:Beq> Make OpenSim profiles work again
#ifdef OPENSIM
    else
    {
        if (LLGridManager::getInstance()->isInOpenSim())
        {
            mImageId = id;
            // save immediately only if description changes are not pending.
            if(!mHasUnsavedDescriptionChanges)
            {
                onSaveDescriptionChanges();
            }
        }
    }
#endif
// </FS:Beq>
}

// <FS:Ansariel> RLVa support
void LLPanelProfileSecondLife::updateRlvRestrictions(ERlvBehaviour behavior)
{
    if (behavior == RLV_BHVR_SHOWLOC || behavior == RLV_BHVR_SHOWWORLDMAP)
    {
        updateButtons();
    }
}
// </FS:Ansariel>

//////////////////////////////////////////////////////////////////////////
// LLPanelProfileWeb

LLPanelProfileWeb::LLPanelProfileWeb()
 : LLPanelProfileTab()
 , mWebBrowser(nullptr)
 , mAvatarNameCacheConnection()
 , mFirstNavigate(false)
{
}

LLPanelProfileWeb::~LLPanelProfileWeb()
{
    if (mAvatarNameCacheConnection.connected())
    {
        mAvatarNameCacheConnection.disconnect();
    }
}

void LLPanelProfileWeb::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();

    mAvatarNameCacheConnection = LLAvatarNameCache::get(getAvatarId(), boost::bind(&LLPanelProfileWeb::onAvatarNameCache, this, _1, _2));
}

bool LLPanelProfileWeb::postBuild()
{
    mWebBrowser = getChild<LLMediaCtrl>("profile_html");
    mWebBrowser->addObserver(this);
    mWebBrowser->setHomePageUrl("about:blank");

    return true;
}

void LLPanelProfileWeb::resetData()
{
    mWebBrowser->navigateHome();
}

void LLPanelProfileWeb::updateData()
{
    LLUUID avatar_id = getAvatarId();
    if (!getStarted() && avatar_id.notNull() && !mURLWebProfile.empty())
    {
        setIsLoading();

        mWebBrowser->setVisible(true);
        mPerformanceTimer.start();
        mWebBrowser->navigateTo(mURLWebProfile, HTTP_CONTENT_TEXT_HTML);
    }
}

// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
void LLPanelProfileWeb::apply(LLAvatarData* data)
{
    data->profile_url = mURLHome;
}
#endif
// </FS:Beq>

void LLPanelProfileWeb::onAvatarNameCache(const LLUUID& agent_id, const LLAvatarName& av_name)
{
    mAvatarNameCacheConnection.disconnect();

    std::string username = av_name.getAccountName();
    if (username.empty())
    {
        username = LLCacheName::buildUsername(av_name.getDisplayName());
    }
    else
    {
        LLStringUtil::replaceChar(username, ' ', '.');
    }

    mURLWebProfile = getProfileURL(username, true);
    if (mURLWebProfile.empty())
    {
        return;
    }

    //if the tab was opened before name was resolved, load the panel now
    updateData();
}

void LLPanelProfileWeb::onCommitLoad(LLUICtrl* ctrl)
{
    if (!mURLHome.empty())
    {
        LLSD::String valstr = ctrl->getValue().asString();
        if (valstr.empty())
        {
            mWebBrowser->setVisible(true);
            mPerformanceTimer.start();
            mWebBrowser->navigateTo( mURLHome, HTTP_CONTENT_TEXT_HTML );
        }
        else if (valstr == "popout")
        {
            // open in viewer's browser, new window
            LLWeb::loadURLInternal(mURLHome);
        }
        else if (valstr == "external")
        {
            // open in external browser
            LLWeb::loadURLExternal(mURLHome);
        }
    }
}

void LLPanelProfileWeb::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
    switch(event)
    {
        case MEDIA_EVENT_STATUS_TEXT_CHANGED:
            childSetValue("status_text", LLSD( self->getStatusText() ) );
        break;

        case MEDIA_EVENT_NAVIGATE_BEGIN:
        {
            if (mFirstNavigate)
            {
                mFirstNavigate = false;
            }
            else
            {
                mPerformanceTimer.start();
            }
        }
        break;

        case MEDIA_EVENT_NAVIGATE_COMPLETE:
        {
            LLStringUtil::format_map_t args;
            args["[TIME]"] = llformat("%.2f", mPerformanceTimer.getElapsedTimeF32());
            childSetValue("status_text", LLSD( getString("LoadTime", args)) );

            setLoaded();
        }
        break;

        default:
            // Having a default case makes the compiler happy.
        break;
    }
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelProfileFirstLife::LLPanelProfileFirstLife()
 : LLPanelProfilePropertiesProcessorTab()
 , mHasUnsavedChanges(false)
 , mPreview(false) // <AS:Chanayane> Preview button
{
}

LLPanelProfileFirstLife::~LLPanelProfileFirstLife()
{
}

bool LLPanelProfileFirstLife::postBuild()
{
    mDescriptionEdit = getChild<LLTextEditor>("fl_description_edit");
    // <FS:Zi> Allow proper texture swatch handling
    // mPicture = getChild<LLThumbnailCtrl>("real_world_pic");
    mPicture = getChild<LLTextureCtrl>("real_world_pic");
    // </FS:Zi>

    mUploadPhoto = getChild<LLButton>("fl_upload_image");
    mChangePhoto = getChild<LLButton>("fl_change_image");
    mRemovePhoto = getChild<LLButton>("fl_remove_image");
    mSaveChanges = getChild<LLButton>("fl_save_changes");
    mDiscardChanges = getChild<LLButton>("fl_discard_changes");
    mPreviewButton = getChild<LLButton>("btn_preview"); // <AS:Chanayane> Preview button

    mUploadPhoto->setCommitCallback([this](LLUICtrl*, void*) { onUploadPhoto(); }, nullptr);
    mChangePhoto->setCommitCallback([this](LLUICtrl*, void*) { onChangePhoto(); }, nullptr);
    mRemovePhoto->setCommitCallback([this](LLUICtrl*, void*) { onRemovePhoto(); }, nullptr);
    mSaveChanges->setCommitCallback([this](LLUICtrl*, void*) { onSaveDescriptionChanges(); }, nullptr);
    mDiscardChanges->setCommitCallback([this](LLUICtrl*, void*) { onDiscardDescriptionChanges(); }, nullptr);
    // <AS:Chanayane> Preview button
    mPreviewButton->setCommitCallback([this](LLUICtrl*, void*) { onClickPreview(); }, nullptr);
    // </AS:Chanayane>
    mDescriptionEdit->setKeystrokeCallback([this](LLTextEditor* caller) { onSetDescriptionDirty(); });
    mPicture->setCommitCallback(boost::bind(&LLPanelProfileFirstLife::onFirstLifePicChanged, this));    // <FS:Zi> Allow proper texture swatch handling

    return true;
}

void LLPanelProfileFirstLife::onOpen(const LLSD& key)
{
    LLPanelProfilePropertiesProcessorTab::onOpen(key); // <FS:Beq/> alter ancestry to re-enable UDP

    if (!getSelfProfile())
    {
        // Otherwise as the only focusable element it will be selected
        mDescriptionEdit->setTabStop(false);
    }
    mPreviewButton->setVisible(getSelfProfile()); // <AS:Chanayane> Preview button
    mDescriptionEdit->setParseHTML(!getSelfProfile()); // <AS:Chanayane> Fix FIRE-35185 (disables link rendering while editing picks or 1st life)

    // <FS:Zi> Allow proper texture swatch handling
    mPicture->setEnabled(getSelfProfile());

    resetData();
}

void LLPanelProfileFirstLife::setProfileImageUploading(bool loading)
{
    mUploadPhoto->setEnabled(!loading);
    mChangePhoto->setEnabled(!loading);
    mRemovePhoto->setEnabled(!loading && mImageId.notNull());

    LLLoadingIndicator* indicator = getChild<LLLoadingIndicator>("image_upload_indicator");
    indicator->setVisible(loading);
    if (loading)
    {
        indicator->start();
    }
    else
    {
        indicator->stop();
    }
}

void LLPanelProfileFirstLife::setProfileImageUploaded(const LLUUID &image_asset_id)
{
    mPicture->setValue(image_asset_id);
    mImageId = image_asset_id;

    setProfileImageUploading(false);
}

void LLPanelProfileFirstLife::commitUnsavedChanges()
{
    if (mHasUnsavedChanges)
    {
        onSaveDescriptionChanges();
    }
}

void LLPanelProfileFirstLife::onUploadPhoto()
{
    (new LLProfileImagePicker(PROFILE_IMAGE_FL, new LLHandle<LLPanel>(getHandle())))->getFile();

    LLFloater* floaterp = mFloaterTexturePickerHandle.get();
    if (floaterp)
    {
        floaterp->closeFloater();
    }
}

void LLPanelProfileFirstLife::onChangePhoto()
{
    LLFloater* floaterp = mFloaterTexturePickerHandle.get();

    // Show the dialog
    if (!floaterp)
    {
        LLFloater* parent_floater = gFloaterView->getParentFloater(this);
        if (parent_floater)
        {
            // because inventory construction is somewhat slow
            getWindow()->setCursor(UI_CURSOR_WAIT);
            LLFloaterTexturePicker* texture_floaterp = new LLFloaterTexturePicker(
                this,
                mImageId,
                LLUUID::null,
                mImageId,
                false,
                false,
                getString("texture_picker_label"), // "SELECT PHOTO", // <FS:Ansariel> Fix LL UI/UX design accident
                PERM_NONE,
                PERM_NONE,
                false,
                NULL,
                PICK_TEXTURE);

            mFloaterTexturePickerHandle = texture_floaterp->getHandle();

            texture_floaterp->setOnFloaterCommitCallback([this](LLTextureCtrl::ETexturePickOp op, LLPickerSource source, const LLUUID& asset_id, const LLUUID&, const LLUUID&)
            {
                if (op == LLTextureCtrl::TEXTURE_SELECT)
                {
                    onCommitPhoto(asset_id);
                }
            });
            texture_floaterp->setLocalTextureEnabled(false);
            texture_floaterp->setCanApply(false, true, false);

            parent_floater->addDependentFloater(mFloaterTexturePickerHandle);

            texture_floaterp->openFloater();
            texture_floaterp->setFocus(true);
        }
    }
    else
    {
        floaterp->setMinimized(false);
        floaterp->setVisibleAndFrontmost(true);
    }
}

void LLPanelProfileFirstLife::onRemovePhoto()
{
    onCommitPhoto(LLUUID::null);

    LLFloater* floaterp = mFloaterTexturePickerHandle.get();
    if (floaterp)
    {
        floaterp->closeFloater();
    }
}

// <FS:Zi> Allow proper texture swatch handling
void LLPanelProfileFirstLife::onFirstLifePicChanged()
{
    onCommitPhoto(mPicture->getImageAssetID());
}
// </FS:Zi>

void LLPanelProfileFirstLife::onCommitPhoto(const LLUUID& id)
{
    if (mImageId == id)
        return;

    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        if (!saveAgentUserInfoCoro("fl_image_id", id))
            return;

        mImageId = id;
        if (mImageId.notNull())
        {
            mPicture->setValue(mImageId);
        }
        else
        {
            mPicture->setValue("Generic_Person_Large");
        }

        mRemovePhoto->setEnabled(mImageId.notNull());
    }
// <FS:Beq> Make OpenSim profiles work again
#ifdef OPENSIM
    else
    {
        LL_WARNS("AvatarProperties") << "Failed to update profile data, no cap found" << LL_ENDL;
        if (LLGridManager::getInstance()->isInOpenSim())
        {
            mImageId = id;
            mImageId = id;
            // save immediately only if description changes are not pending.
            if(!mHasUnsavedChanges)
            {
                onSaveDescriptionChanges();
            }
        }
    }
#endif
// </FS:Beq>
}

void LLPanelProfileFirstLife::setDescriptionText(const std::string &text)
{
    mSaveChanges->setEnabled(false);
    mDiscardChanges->setEnabled(false);
    mHasUnsavedChanges = false;

    mCurrentDescription = text;
    mDescriptionEdit->setValue(mCurrentDescription);
}

// <AS:Chanayane> Preview button
void LLPanelProfileFirstLife::reparseDescriptionText(const std::string &text)
{
    mDescriptionEdit->reparseValue(text);
}
// </AS:Chanayane>

void LLPanelProfileFirstLife::onSetDescriptionDirty()
{
    mSaveChanges->setEnabled(true);
    mDiscardChanges->setEnabled(true);
    mHasUnsavedChanges = true;
}

void LLPanelProfileFirstLife::onSaveDescriptionChanges()
{
    mCurrentDescription = mDescriptionEdit->getValue().asString();
    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        saveAgentUserInfoCoro("fl_about_text", mCurrentDescription);
    }
// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
    else if (LLGridManager::getInstance()->isInOpenSim())
    {
        if (getIsLoaded() && getSelfProfile())
        {
            LLFloater* floater_profile = LLFloaterReg::findInstance("profile", LLSD().with("id", gAgentID));
            if (!floater_profile)
            {
                // floater is dead, so panels are dead as well
                return;
            }
            LLPanelProfile* panel_profile = floater_profile->findChild<LLPanelProfile>(PANEL_PROFILE_VIEW, true);
            if (!panel_profile)
            {
                LL_WARNS() << PANEL_PROFILE_VIEW << " not found" << LL_ENDL;
            }
            else
            {
                auto avatar_data = panel_profile->getAvatarData();
                avatar_data.agent_id = gAgentID;
                avatar_data.avatar_id = gAgentID;
                avatar_data.fl_image_id = mImageId;
                avatar_data.fl_about_text = mDescriptionEdit->getValue().asString();

                LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesUpdate(&avatar_data);
            }
        }
    }
#endif
// </FS:Beq>

    mSaveChanges->setEnabled(false);
    mDiscardChanges->setEnabled(false);
    mHasUnsavedChanges = false;
}

void LLPanelProfileFirstLife::onDiscardDescriptionChanges()
{
    setDescriptionText(mCurrentDescription);
}

// <AS:Chanayane> Preview button
void LLPanelProfileFirstLife::onClickPreview()
{
    mPreview = !mPreview;

    mDescriptionEdit->setEnabled(!mPreview);
    mDescriptionEdit->setParseHTML(mPreview);

    if (mPreview) {
        mPreviewButton->setImageOverlay("Profile_Group_Visibility_Off");
        if (mHasUnsavedChanges) {
            mSaveChanges->setEnabled(false);
            mDiscardChanges->setEnabled(false);
        }
        mOriginalDescription = mDescriptionEdit->getValue().asString();
        reparseDescriptionText(mOriginalDescription);
    } else {
        mPreviewButton->setImageOverlay("Profile_Group_Visibility_On");
        if (mHasUnsavedChanges) {
            mSaveChanges->setEnabled(true);
            mDiscardChanges->setEnabled(true);
        }
        reparseDescriptionText(mOriginalDescription);
    }
}
// </AS:Chanayane>

void LLPanelProfileFirstLife::processProperties(void * data, EAvatarProcessorType type)
{
    if (APT_PROPERTIES == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProperties(avatar_data);
        }
    }
    // <FS:Beq> Restore UDP profiles
    else if (gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty() && APT_PROPERTIES_LEGACY == type)
    {
        const LLAvatarData* avatar_data = static_cast<const LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProperties(avatar_data);
        }
}
// </FS:Beq>
}

void LLPanelProfileFirstLife::processProperties(const LLAvatarData* avatar_data)
{
    setDescriptionText(avatar_data->fl_about_text);

    mImageId = avatar_data->fl_image_id;

    if (mImageId.notNull())
    {
        mPicture->setValue(mImageId);
    }
    else
    {
        mPicture->setValue("Generic_Person_Large");
    }

    setLoaded();
}

// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
void LLPanelProfileFirstLife::apply(LLAvatarData* data)
{
    data->fl_image_id = mImageId;
    data->fl_about_text = mDescriptionEdit->getValue().asString();
}
#endif
// </FS:Beq>

void LLPanelProfileFirstLife::resetData()
{
    setDescriptionText(std::string());
    // <FS:Ansariel> Retain texture picker for profile images
    //mPicture->setValue("Generic_Person_Large");
    mPicture->setImageAssetID(LLUUID::null);
    mImageId = LLUUID::null;

// <FS:Beq> remove the buttons and just have click image to update profile
// mUploadPhoto->setVisible(getSelfProfile());
// mChangePhoto->setVisible(getSelfProfile());
// mRemovePhoto->setVisible(getSelfProfile());
    auto show_image_buttons = getSelfProfile();
#ifdef OPENSIM
    std::string cap_url = gAgent.getRegionCapability(PROFILE_IMAGE_UPLOAD_CAP);
    if (cap_url.empty() && LLGridManager::instance().isInOpenSim())
    {
        show_image_buttons = false;
    }
#endif
    mUploadPhoto->setVisible(show_image_buttons);
    mChangePhoto->setVisible(show_image_buttons);
    mRemovePhoto->setVisible(show_image_buttons);
// </FS:Beq>
    mSaveChanges->setVisible(getSelfProfile());
    mDiscardChanges->setVisible(getSelfProfile());
    mPreviewButton->setVisible(getSelfProfile()); // <AS:Chanayane> Preview button
}

void LLPanelProfileFirstLife::setLoaded()
{
    LLPanelProfileTab::setLoaded();

    if (getSelfProfile())
    {
        mDescriptionEdit->setEnabled(true);
        mPicture->setEnabled(true);
        mRemovePhoto->setEnabled(mImageId.notNull());
        mPreviewButton->setEnabled(true);
    }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

LLPanelProfileNotes::LLPanelProfileNotes()
: LLPanelProfilePropertiesProcessorTab()
 , mHasUnsavedChanges(false)
{

}

LLPanelProfileNotes::~LLPanelProfileNotes()
{
}

// <FS:Beq> Restore UDP profiles
void LLPanelProfileNotes::updateData()
{
#ifdef OPENSIM
    if (LLGridManager::instance().isInOpenSim() && gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
    LLUUID avatar_id = getAvatarId();
        if (!getStarted() && avatar_id.notNull() && gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty() && !getSelfProfile())
    {
        setIsLoading();
            LLAvatarPropertiesProcessor::getInstance()->sendAvatarNotesRequest(avatar_id);
        }
    }
    else
#endif
    {
        LLPanelProfilePropertiesProcessorTab::updateData();
    }
}
// </FS:Beq>

void LLPanelProfileNotes::commitUnsavedChanges()
{
    if (mHasUnsavedChanges)
    {
        onSaveNotesChanges();
    }
}

bool LLPanelProfileNotes::postBuild()
{
    mNotesEditor = getChild<LLTextEditor>("notes_edit");
    mSaveChanges = getChild<LLButton>("notes_save_changes");
    mDiscardChanges = getChild<LLButton>("notes_discard_changes");

    mSaveChanges->setCommitCallback([this](LLUICtrl*, void*) { onSaveNotesChanges(); }, nullptr);
    mDiscardChanges->setCommitCallback([this](LLUICtrl*, void*) { onDiscardNotesChanges(); }, nullptr);
    mNotesEditor->setKeystrokeCallback([this](LLTextEditor* caller) { onSetNotesDirty(); });

    return true;
}

void LLPanelProfileNotes::onOpen(const LLSD& key)
{
    LLPanelProfileTab::onOpen(key);

    resetData();
}

void LLPanelProfileNotes::setNotesText(const std::string &text)
{
    // <FS:Zi> FIRE-32926 - Profile notes that are actively being edited get discarded when
    //                      the profile owner enters or leaves the region at the same time.
    if (mHasUnsavedChanges)
    {
        return;
    }
    // </FS:Zi>

    mSaveChanges->setEnabled(false);
    mDiscardChanges->setEnabled(false);
    mHasUnsavedChanges = false;

    mCurrentNotes = text;
    mNotesEditor->setValue(mCurrentNotes);
}

void LLPanelProfileNotes::onSetNotesDirty()
{
    mSaveChanges->setEnabled(true);
    mDiscardChanges->setEnabled(true);
    mHasUnsavedChanges = true;
}

void LLPanelProfileNotes::onSaveNotesChanges()
{
    mCurrentNotes = mNotesEditor->getValue().asString();
    if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
    {
        saveAgentUserInfoCoro("notes", mCurrentNotes);
    }
// <FS:Beq> Restore UDO profiles
#ifdef OPENSIM
    else if (LLGridManager::instance().isInOpenSim())
    {
        LLAvatarPropertiesProcessor::getInstance()->sendNotes(getAvatarId(), mCurrentNotes);
    }
#endif
// </FS:Beq>

    FSRadar::getInstance()->updateNotes(getAvatarId(), mCurrentNotes);     // <FS:Zi> Update notes in radar when edited

    mSaveChanges->setEnabled(false);
    mDiscardChanges->setEnabled(false);
    mHasUnsavedChanges = false;
}

void LLPanelProfileNotes::onDiscardNotesChanges()
{
    setNotesText(mCurrentNotes);
}

void LLPanelProfileNotes::processProperties(void * data, EAvatarProcessorType type)
{
    if (APT_PROPERTIES == type)
    {
        LLAvatarData* avatar_data = static_cast<LLAvatarData*>(data);
        if (avatar_data && getAvatarId() == avatar_data->avatar_id)
        {
            processProperties(avatar_data);
        }
    }

    // <FS:Beq> Restore UDP profiles
    else if (gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty() && APT_NOTES == type)
    {
        LLAvatarNotes* avatar_notes = static_cast<LLAvatarNotes*>(data);
        if (avatar_notes && getAvatarId() == avatar_notes->target_id)
        {
            LLAvatarData avatardata;
            avatardata.notes = avatar_notes->notes;
            processProperties(&avatardata);
    }
}
// </FS:Beq>
}

void LLPanelProfileNotes::processProperties(const LLAvatarData* avatar_data)
{
    setNotesText(avatar_data->notes);
    mNotesEditor->setEnabled(true);
    setLoaded();
}

void LLPanelProfileNotes::resetData()
    {
    resetLoading();
    setNotesText(std::string());
}


//////////////////////////////////////////////////////////////////////////
// LLPanelProfile

LLPanelProfile::LLPanelProfile()
 : LLPanelProfileTab()
{
}

LLPanelProfile::~LLPanelProfile()
{
}

bool LLPanelProfile::postBuild()
{
    return true;
}

void LLPanelProfile::onTabChange()
{
    LLPanelProfileTab* active_panel = dynamic_cast<LLPanelProfileTab*>(mTabContainer->getCurrentPanel());
    if (active_panel)
    {
        active_panel->updateData();
    }
}

void LLPanelProfile::onOpen(const LLSD& key)
{
    LLUUID avatar_id = key["id"].asUUID();

    // Don't reload the same profile
    if (getAvatarId() == avatar_id)
    {
        return;
    }

    LLPanelProfileTab::onOpen(avatar_id);

    mTabContainer       = getChild<LLTabContainer>("panel_profile_tabs");
    mPanelSecondlife    = findChild<LLPanelProfileSecondLife>(PANEL_SECONDLIFE);
    mPanelWeb           = findChild<LLPanelProfileWeb>(PANEL_WEB);
    mPanelPicks         = findChild<LLPanelProfilePicks>(PANEL_PICKS);
    mPanelClassifieds   = findChild<LLPanelProfileClassifieds>(PANEL_CLASSIFIEDS);
    mPanelFirstlife     = findChild<LLPanelProfileFirstLife>(PANEL_FIRSTLIFE);
    mPanelNotes         = findChild<LLPanelProfileNotes>(PANEL_NOTES);

    mPanelSecondlife->onOpen(avatar_id);
    mPanelWeb->onOpen(avatar_id);
    mPanelPicks->onOpen(avatar_id);
    mPanelClassifieds->onOpen(avatar_id);
    mPanelFirstlife->onOpen(avatar_id);
    mPanelNotes->onOpen(avatar_id);

    // Always request the base profile info
    resetLoading();
    updateData();

    // Some tabs only request data when opened
    mTabContainer->setCommitCallback(boost::bind(&LLPanelProfile::onTabChange, this));
}

void LLPanelProfile::updateData()
{
    LLUUID avatar_id = getAvatarId();
    // Todo: getIsloading functionality needs to be expanded to
    // include 'inited' or 'data_provided' state to not rerequest
    if (!getStarted() && avatar_id.notNull())
    {
// <FS:Beq> Restore UDP profiles
#ifdef OPENSIM
        if (LLGridManager::instance().isInOpenSim())
        {
            mPanelSecondlife->updateData();
            mPanelPicks->updateData();
            mPanelFirstlife->updateData();
            mPanelNotes->updateData();
        }
        else
#endif
        {
// </FS:Beq>
        setIsLoading();
        mPanelSecondlife->setIsLoading();
        mPanelPicks->setIsLoading();
        mPanelFirstlife->setIsLoading();
        mPanelNotes->setIsLoading();
        } // <FS:Beq/> restore udp profiles

// <FS:Beq> Restore UDP profiles
        //LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(getAvatarId());
        if (!gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty())
        {
            LLAvatarPropertiesProcessor::getInstance()->sendAvatarPropertiesRequest(getAvatarId());
        }
#ifdef OPENSIM
        else if (LLGridManager::instance().isInOpenSim())
        {
            LLAvatarPropertiesProcessor::getInstance()->sendAvatarLegacyPropertiesRequest(avatar_id);
        }
#endif
// </FS:Beq>
    }
}

void LLPanelProfile::refreshName()
{
    mPanelSecondlife->refreshName();
}

void LLPanelProfile::createPick(const LLPickData &data)
{
    mTabContainer->selectTabPanel(mPanelPicks);
    mPanelPicks->createPick(data);
}

void LLPanelProfile::showPick(const LLUUID& pick_id)
{
    if (pick_id.notNull())
    {
        mPanelPicks->selectPick(pick_id);
    }
    mTabContainer->selectTabPanel(mPanelPicks);
}

bool LLPanelProfile::isPickTabSelected()
{
    return (mTabContainer->getCurrentPanel() == mPanelPicks);
}

bool LLPanelProfile::isNotesTabSelected()
{
    return (mTabContainer->getCurrentPanel() == mPanelNotes);
}

bool LLPanelProfile::hasUnsavedChanges()
{
    return mPanelSecondlife->hasUnsavedChanges()
        || mPanelPicks->hasUnsavedChanges()
        || mPanelClassifieds->hasUnsavedChanges()
        || mPanelFirstlife->hasUnsavedChanges()
        || mPanelNotes->hasUnsavedChanges();
}

bool LLPanelProfile::hasUnpublishedClassifieds()
{
    return mPanelClassifieds->hasNewClassifieds();
}

void LLPanelProfile::commitUnsavedChanges()
{
    mPanelSecondlife->commitUnsavedChanges();
    mPanelPicks->commitUnsavedChanges();
    mPanelClassifieds->commitUnsavedChanges();
    mPanelFirstlife->commitUnsavedChanges();
    mPanelNotes->commitUnsavedChanges();
    // <FS:Beq> restore UDP - this is effectvely the apply() method from the previous incarnation
#ifdef OPENSIM
    if (LLGridManager::instance().isInOpenSim() && (gAgent.getRegionCapability(PROFILE_PROPERTIES_CAP).empty()) && getSelfProfile())
    {
        //KC - Avatar data is spread over 3 different panels
        // collect data from the last 2 and give to the first to save
        LLAvatarData data = mAvatarData;
        data.avatar_id = gAgentID;
        // these three collate data so need to be called in sequence.
        mPanelFirstlife->apply(&data);
        mPanelWeb->apply(&data);
        mPanelSecondlife->apply(&data);
        // These three triggered above
        // mPanelInterests->apply();
        // mPanelPicks->apply();
        // mPanelNotes->apply();
    }
#endif
    // </FS:Beq>
}
void LLPanelProfile::showClassified(const LLUUID& classified_id, bool edit)
{
    if (classified_id.notNull())
    {
        mPanelClassifieds->selectClassified(classified_id, edit);
    }
    mTabContainer->selectTabPanel(mPanelClassifieds);
}

void LLPanelProfile::createClassified()
{
    mPanelClassifieds->createClassified();
    mTabContainer->selectTabPanel(mPanelClassifieds);
}

// <FS:Zi> FIRE-32184: Online/Offline status not working for non-friends
FSPanelPropertiesObserver::FSPanelPropertiesObserver() : LLAvatarPropertiesObserver(),
    mPanelProfile(nullptr)
{
}

void FSPanelPropertiesObserver::processProperties(void* data, EAvatarProcessorType type)
{
    if (data && type == APT_PROPERTIES_LEGACY && mPanelProfile)
    {
        LLAvatarData avatardata(*static_cast<LLAvatarLegacyData*>(data));
        mPanelProfile->onAvatarProperties(&avatardata);
    }
}
// </FS:Zi>

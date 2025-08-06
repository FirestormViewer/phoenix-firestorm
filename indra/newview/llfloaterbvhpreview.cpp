/**
 * @file llfloaterbvhpreview.cpp
 * @brief LLFloaterBvhPreview class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewerlgpl$
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

#include "llfloaterbvhpreview.h"

#include "llbvhloader.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llnotificationsutil.h"
#include "llfilesystem.h"
#include "llapr.h"
#include "llstring.h"

#include "llagent.h"
#include "llagentbenefits.h"
#include "llanimationstates.h"
#include "llbbox.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llrender.h"
#include "llface.h"
#include "llfocusmgr.h"
#include "llkeyframemotion.h"
#include "lllineeditor.h"
#include "llfloaterperms.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltoolmgr.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llviewermenufile.h"   // upload_new_resource()
#include "llvoavatar.h"
#include "pipeline.h"
#include "lluictrlfactory.h"
#include "lltrans.h"
#include "llviewercontrol.h" // for gSavedSettings
#include "llvoavatarself.h"

#include "aoengine.h"   // <FS:Zi> FIRE-32315: Animation preview sometimes fails when FS AO is enabled

S32 LLFloaterBvhPreview::sOwnAvatarInstanceCount = 0; // <FS> Preview on own avatar

const S32 PREVIEW_BORDER_WIDTH = 2;
const S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) + PREVIEW_BORDER_WIDTH;
const S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
// <FS:Ansariel> Fix preview window location
//const S32 PREVIEW_VPAD = 35;
//const S32 PREF_BUTTON_HEIGHT = 16 + 35;
const S32 PREVIEW_VPAD = 70;
const S32 PREF_BUTTON_HEIGHT = 16 + PREVIEW_VPAD;
// </FS:Ansariel>
const S32 PREVIEW_TEXTURE_HEIGHT = 300;

const F32 PREVIEW_CAMERA_DISTANCE = 4.f;

const F32 MIN_CAMERA_ZOOM = 0.5f;
const F32 MAX_CAMERA_ZOOM = 10.f;

const F32 BASE_ANIM_TIME_OFFSET = 5.f;

std::string STATUS[] =
{
    "E_ST_OK",
    "E_ST_EOF",
    "E_ST_NO_CONSTRAINT",
    "E_ST_NO_FILE",
    "E_ST_NO_HIER",
    "E_ST_NO_JOINT",
    "E_ST_NO_NAME",
    "E_ST_NO_OFFSET",
    "E_ST_NO_CHANNELS",
    "E_ST_NO_ROTATION",
    "E_ST_NO_AXIS",
    "E_ST_NO_MOTION",
    "E_ST_NO_FRAMES",
    "E_ST_NO_FRAME_TIME",
    "E_ST_NO_POS",
    "E_ST_NO_ROT",
    "E_ST_NO_XLT_FILE",
    "E_ST_NO_XLT_HEADER",
    "E_ST_NO_XLT_NAME",
    "E_ST_NO_XLT_IGNORE",
    "E_ST_NO_XLT_RELATIVE",
    "E_ST_NO_XLT_OUTNAME",
    "E_ST_NO_XLT_MATRIX",
    "E_ST_NO_XLT_MERGECHILD",
    "E_ST_NO_XLT_MERGEPARENT",
    "E_ST_NO_XLT_PRIORITY",
    "E_ST_NO_XLT_LOOP",
    "E_ST_NO_XLT_EASEIN",
    "E_ST_NO_XLT_EASEOUT",
    "E_ST_NO_XLT_HAND",
    "E_ST_NO_XLT_EMOTE",
"E_ST_BAD_ROOT"
};

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::LLFloaterBvhPreview(const LLSD& args) :
    LLFloaterNameDesc(args)
{
    mLastMouseX = 0;
    mLastMouseY = 0;

    // <FS> Preview on own avatar
    mUseOwnAvatar = gSavedSettings.getBOOL("FSUploadAnimationOnOwnAvatar");
    if (mUseOwnAvatar)
    {
        sOwnAvatarInstanceCount++;

        // // Switch FS AO off during preview
        mAOEnabled = gSavedPerAccountSettings.getBOOL("UseAO");
        if (mAOEnabled)
        {
            AOEngine::getInstance()->enable(false);
        }
    }
    // </FS>

    mIDList["Standing"] = ANIM_AGENT_STAND;
    mIDList["Walking"] = ANIM_AGENT_FEMALE_WALK;
    mIDList["Sitting"] = ANIM_AGENT_SIT_FEMALE;
    mIDList["Flying"] = ANIM_AGENT_HOVER;

    mIDList["[None]"] = LLUUID::null;
    mIDList["Aaaaah"] = ANIM_AGENT_EXPRESS_OPEN_MOUTH;
    mIDList["Afraid"] = ANIM_AGENT_EXPRESS_AFRAID;
    mIDList["Angry"] = ANIM_AGENT_EXPRESS_ANGER;
    mIDList["Big Smile"] = ANIM_AGENT_EXPRESS_TOOTHSMILE;
    mIDList["Bored"] = ANIM_AGENT_EXPRESS_BORED;
    mIDList["Cry"] = ANIM_AGENT_EXPRESS_CRY;
    mIDList["Disdain"] = ANIM_AGENT_EXPRESS_DISDAIN;
    mIDList["Embarrassed"] = ANIM_AGENT_EXPRESS_EMBARRASSED;
    mIDList["Frown"] = ANIM_AGENT_EXPRESS_FROWN;
    mIDList["Kiss"] = ANIM_AGENT_EXPRESS_KISS;
    mIDList["Laugh"] = ANIM_AGENT_EXPRESS_LAUGH;
    mIDList["Plllppt"] = ANIM_AGENT_EXPRESS_TONGUE_OUT;
    mIDList["Repulsed"] = ANIM_AGENT_EXPRESS_REPULSED;
    mIDList["Sad"] = ANIM_AGENT_EXPRESS_SAD;
    mIDList["Shrug"] = ANIM_AGENT_EXPRESS_SHRUG;
    mIDList["Smile"] = ANIM_AGENT_EXPRESS_SMILE;
    mIDList["Surprise"] = ANIM_AGENT_EXPRESS_SURPRISE;
    mIDList["Wink"] = ANIM_AGENT_EXPRESS_WINK;
    mIDList["Worry"] = ANIM_AGENT_EXPRESS_WORRY;

    // <FS:Ansariel> FIRE-2083: Slider in upload animation floater doesn't work
    mTimer.stop();
}

//-----------------------------------------------------------------------------
// setAnimCallbacks()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::setAnimCallbacks()
{
    getChild<LLUICtrl>("playback_slider")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onSliderMove, this));

    getChild<LLUICtrl>("preview_base_anim")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitBaseAnim, this));
    // <FS:Sei> FIRE-17251: Use the XUI values for defaults
    //getChild<LLUICtrl>("preview_base_anim")->setValue("Standing");
    // </FS:Sei>

    getChild<LLUICtrl>("priority")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitPriority, this));
    getChild<LLUICtrl>("loop_check")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoop, this));
    getChild<LLUICtrl>("loop_in_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopIn, this));
    getChild<LLUICtrl>("loop_in_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopIn, this, _1));
    getChild<LLUICtrl>("loop_out_point")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopOut, this));
    getChild<LLUICtrl>("loop_out_point")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopOut, this, _1));
    // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
    getChild<LLUICtrl>("loop_in_frames")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopInFrames, this));
    getChild<LLUICtrl>("loop_in_frames")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopInFrames, this, _1));
    getChild<LLUICtrl>("loop_out_frames")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitLoopOutFrames, this));
    getChild<LLUICtrl>("loop_out_frames")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateLoopOutFrames, this, _1));
    // </FS:Sei>

    getChild<LLUICtrl>("hand_pose_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitHandPose, this));

    getChild<LLUICtrl>("emote_combo")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEmote, this));
    // <FS:Sei> FIRE-17251: Use the XUI values for defaults
    //getChild<LLUICtrl>("emote_combo")->setValue("[None]");
    // </FS:Sei>

    getChild<LLUICtrl>("ease_in_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseIn, this));
    getChild<LLUICtrl>("ease_in_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseIn, this, _1));
    getChild<LLUICtrl>("ease_out_time")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitEaseOut, this));
    getChild<LLUICtrl>("ease_out_time")->setValidateBeforeCommit( boost::bind(&LLFloaterBvhPreview::validateEaseOut, this, _1));
}

std::map<std::string, std::string, std::less<>> LLFloaterBvhPreview::getJointAliases()
{
    LLPointer<LLVOAvatar> av = (LLVOAvatar*)mAnimPreview->getDummyAvatar();
    return av->getJointAliases();
}

//-----------------------------------------------------------------------------
// postBuild()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::postBuild()
{
    // <FS> Reload animation from disk
    //LLKeyframeMotion* motionp = NULL;
    //LLBVHLoader* loaderp = NULL;
    // </FS>

    if (!LLFloaterNameDesc::postBuild())
    {
        return false;
    }

    getChild<LLUICtrl>("name_form")->setCommitCallback(boost::bind(&LLFloaterBvhPreview::onCommitName, this));

    // <FS> Reload animation from disk
    childSetAction("reload_btn", onBtnReload, this);
    childSetAction("ok_btn", onBtnOK, this);
    setDefaultBtn();

    // <FS> Preview on own avatar
    //mPreviewRect.set(PREVIEW_HPAD,
    //  PREVIEW_TEXTURE_HEIGHT,
    //  getRect().getWidth() - PREVIEW_HPAD,
    //  PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
    //mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);
    if (!mUseOwnAvatar)
    {
        LLRect rect = getRect();
        translate(0, PREVIEW_TEXTURE_HEIGHT-30);
        reshape(rect.getWidth(), rect.getHeight() + PREVIEW_TEXTURE_HEIGHT-30);

        mPreviewRect.set(PREVIEW_HPAD,
            PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD,
            getRect().getWidth() - PREVIEW_HPAD,
            PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
        mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);
    }
    // </FS>

    mPlayButton = getChild<LLButton>( "play_btn");
    mPlayButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnPlay, this));
    // <FS> Reload animation from disk
    //mPlayButton->setVisible(true);

    mPauseButton = getChild<LLButton>( "pause_btn");
    mPauseButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnPause, this));
    // <FS> Reload animation from disk
    //mPauseButton->setVisible(false);

    mStopButton = getChild<LLButton>( "stop_btn");
    mStopButton->setClickedCallback(boost::bind(&LLFloaterBvhPreview::onBtnStop, this));

// <FS:CR> Allow Higher priority animations to be uploaded
    LLSpinCtrl* spinner = getChild<LLSpinCtrl>("priority");
    S32 max_value = gSavedSettings.getS32("FSMaxAnimationPriority");
    if (max_value > 6) max_value = 6;
    if (max_value < 0) max_value = 0;
    spinner->setMaxValue((F32)max_value);
// </FS:CR>

// <FS> Reload animation from disk
    setAnimCallbacks();
    loadBVH();

    return true;
}

//-----------------------------------------------------------------------------
// loadBVH()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::loadBVH()
{
    LLKeyframeMotion* motionp = NULL;
    LLBVHLoader* loaderp = NULL;

    mPlayButton->setVisible(true);
    mPauseButton->setVisible(false);
// </FS>

    getChildView("bad_animation_text")->setVisible(false);

    mAnimPreview = new LLPreviewAnimation(256, 256);

    std::string exten = gDirUtilp->getExtension(mFilename);
    if (exten == "bvh")
    {
        // loading a bvh file

        // now load bvh file
        S32 file_size;

        LLAPRFile infile ;
        infile.open(mFilenameAndPath, LL_APR_RB, NULL, &file_size);

        if (!infile.getFileHandle())
        {
            LL_WARNS() << "Can't open BVH file:" << mFilename << LL_ENDL;
        }
        else
        {
            char*   file_buffer;

            file_buffer = new char[file_size + 1];

            if (file_size == infile.read(file_buffer, file_size))
            {
                file_buffer[file_size] = '\0';
                LL_INFOS() << "Loading BVH file " << mFilename << LL_ENDL;
                ELoadStatus load_status = E_ST_OK;
                S32 line_number = 0;

                auto joint_alias_map = getJointAliases();

                loaderp = new LLBVHLoader(file_buffer, load_status, line_number, joint_alias_map);
                std::string status = getString(STATUS[load_status]);

                if(load_status == E_ST_NO_XLT_FILE)
                {
                    LL_WARNS() << "NOTE: No translation table found." << LL_ENDL;
                }
                else
                {
                    LL_WARNS() << "ERROR: [line: " << line_number << "] " << status << LL_ENDL;
                }
            }

            infile.close() ;
            delete[] file_buffer;
        }
    }

    if (loaderp && loaderp->isInitialized() && loaderp->getDuration() <= MAX_ANIM_DURATION)
    {
        // generate unique id for this motion
        mTransactionID.generate();
        mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());

        // motion will be returned, but it will be in a load-pending state, as this is a new motion
        // this motion will not request an asset transfer until next update, so we have a chance to
        // load the keyframe data locally
        // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
        mNumFrames = loaderp->getNumFrames();
        getChild<LLSpinCtrl>("loop_in_frames")->setMaxValue(LLSD((F32)mNumFrames));
        getChild<LLSpinCtrl>("loop_out_frames")->setMaxValue(LLSD((F32)mNumFrames));
        // (Re)assign loop frames spinners from loop percentages.
        getChild<LLUICtrl>("loop_in_frames")->setValue(LLSD((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f * (F32)mNumFrames));
        getChild<LLUICtrl>("loop_out_frames")->setValue(LLSD((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() / 100.f * (F32)mNumFrames));

        LLUIString out_str = getString("FS_report_frames");
        out_str.setArg("[F]", llformat("%d", mNumFrames));
        out_str.setArg("[S]", llformat("%.1f", loaderp->getDuration()));
        out_str.setArg("[FPS]", llformat("%.1f", (F32)mNumFrames / loaderp->getDuration()));
        getChild<LLUICtrl>("frames_label")->setValue(LLSD(out_str));
        // </FS:Sei>
        // <FS> Preview on own avatar
        //motionp = (LLKeyframeMotion*)mAnimPreview->getDummyAvatar()->createMotion(mMotionID);
        motionp =  dynamic_cast<LLKeyframeMotion*>(mAnimPreview->getPreviewAvatar(this)->createMotion(mMotionID));
        // </FS>

        // create data buffer for keyframe initialization
        S32 buffer_size = loaderp->getOutputSize();
        U8* buffer = new(std::nothrow) U8[buffer_size];
        if (!buffer)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            LL_ERRS() << "Bad memory allocation for buffer, size: " << buffer_size << LL_ENDL;
        }

        LLDataPackerBinaryBuffer dp(buffer, buffer_size);

        // pass animation data through memory buffer
        LL_INFOS("BVH") << "Serializing loaderp" << LL_ENDL;
        loaderp->serialize(dp);
        dp.reset();
        LL_INFOS("BVH") << "Deserializing motionp" << LL_ENDL;
        bool success = motionp && motionp->deserialize(dp, mMotionID, false);
        LL_INFOS("BVH") << "Done" << LL_ENDL;

        delete []buffer;

        if (success)
        {
            // <FS> Reload animation from disk; moved to postBuild
            //setAnimCallbacks() ;

            const LLBBoxLocal &pelvis_bbox = motionp->getPelvisBBox();

            LLVector3 temp = pelvis_bbox.getCenter();
            // only consider XY?
            //temp.mV[VZ] = 0.f;
            F32 pelvis_offset = temp.magVec();

            temp = pelvis_bbox.getExtent();
            //temp.mV[VZ] = 0.f;
            F32 pelvis_max_displacement = pelvis_offset + (temp.magVec() * 0.5f) + 1.f;

            F32 camera_zoom = LLViewerCamera::getInstance()->getDefaultFOV() / (2.f * atan(pelvis_max_displacement / PREVIEW_CAMERA_DISTANCE));

            mAnimPreview->setZoom(camera_zoom);

            motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
            // <FS> Preview on own avatar
            //mAnimPreview->getDummyAvatar()->startMotion(mMotionID);
            onBtnPlay();
            // </FS>

            getChild<LLSliderCtrl>("playback_slider")->setMinValue(0.0);
            getChild<LLSliderCtrl>("playback_slider")->setMaxValue(1.0);

            //<FS:Sei> FIRE-17251: Use defaults from XUI, not from the JointMotionList constructor
            //getChild<LLUICtrl>("loop_check")->setValue(LLSD(motionp->getLoop()));
            //getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(motionp->getLoopIn() / motionp->getDuration() * 100.f));
            //getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(motionp->getLoopOut() / motionp->getDuration() * 100.f));
            //getChild<LLUICtrl>("priority")->setValue(LLSD((F32)motionp->getPriority()));
            //getChild<LLUICtrl>("hand_pose_combo")->setValue(LLHandMotion::getHandPoseName(motionp->getHandPose()));
            //getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(motionp->getEaseInDuration()));
            //getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(motionp->getEaseOutDuration()));
            motionp->setLoop(getChild<LLUICtrl>("loop_check")->getValue().asBoolean());
            motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f * motionp->getDuration());
            motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() / 100.f * motionp->getDuration());
            motionp->setPriority(getChild<LLUICtrl>("priority")->getValue().asInteger());
            motionp->setHandPose(LLHandMotion::getHandPose(getChild<LLUICtrl>("hand_pose_combo")->getValue().asString()));
            F32 ease_in = (F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal();
            F32 ease_out = (F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal();
            if (motionp->getDuration() != 0.f && ease_in + ease_out > motionp->getDuration() && !getChild<LLUICtrl>("loop_check")->getValue().asBoolean())
            {
                F32 factor = motionp->getDuration() / (ease_in + ease_out);
                ease_in *= factor;
                ease_out *= factor;
                getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(ease_in));
                getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(ease_out));
            }
            motionp->setEaseIn(ease_in);
            motionp->setEaseOut(ease_out);
            //</FS>
            setEnabled(true);
            std::string seconds_string;
            seconds_string = llformat(" - %.2f seconds", motionp->getDuration());

            setTitle(mFilename + std::string(seconds_string));
        }
        else
        {
            mAnimPreview = NULL;
            mMotionID.setNull();
            getChild<LLUICtrl>("bad_animation_text")->setValue(getString("failed_to_initialize"));
        }
    }
    else
    {
        if ( loaderp )
        {
            if (loaderp->getDuration() > MAX_ANIM_DURATION)
            {
                LLUIString out_str = getString("anim_too_long");
                out_str.setArg("[LENGTH]", llformat("%.1f", loaderp->getDuration()));
                out_str.setArg("[MAX_LENGTH]", llformat("%.1f", MAX_ANIM_DURATION));
                getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
            }
            else
            {
                LLUIString out_str = getString("failed_file_read");
                out_str.setArg("[STATUS]", getString(STATUS[loaderp->getStatus()]));
                getChild<LLUICtrl>("bad_animation_text")->setValue(out_str.getString());
            }
        }

        //setEnabled(false);
        mMotionID.setNull();
        mAnimPreview = NULL;
    }

    refresh();

    delete loaderp;

    return true;
}

// <FS> Reload animation from disk
//-----------------------------------------------------------------------------
// unloadMotion()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::unloadMotion()
{
    if (mMotionID.notNull() && mAnimPreview && mUseOwnAvatar)
    {
        resetMotion();
        // <FS> Preview on own avatar
        //mAnimPreview->getDummyAvatar()->removeMotion(mMotionID);
        mAnimPreview->getPreviewAvatar(this)->removeMotion(mMotionID);
        // </FS>
        LLKeyframeDataCache::removeKeyframeData(mMotionID);
    }

    mMotionID.setNull();
    mAnimPreview = NULL;
}
// </FS>

//-----------------------------------------------------------------------------
// LLFloaterBvhPreview()
//-----------------------------------------------------------------------------
LLFloaterBvhPreview::~LLFloaterBvhPreview()
{
    mAnimPreview = NULL;

    // <FS> Preview on own avatar
    if (mUseOwnAvatar)
    {
        sOwnAvatarInstanceCount--;

        if (mMotionID.notNull())
        {
            LLKeyframeDataCache::removeKeyframeData(mMotionID);
        }

        if (sOwnAvatarInstanceCount == 0 && isAgentAvatarValid())
        {
            gAgentAvatarp->deactivateAllMotions();
            gAgentAvatarp->startDefaultMotions();
            gAgentAvatarp->startMotion(ANIM_AGENT_STAND);
        }

        // Switch FS AO back on if it was disabled during preview
        if (mAOEnabled)
        {
            AOEngine::getInstance()->enable(true);
        }
    }
    // </FS>

    setEnabled(false);
}

//-----------------------------------------------------------------------------
// draw()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::draw()
{
    LLFloater::draw();
    LLRect r = getRect();

    refresh();

    if (mMotionID.notNull() && mAnimPreview)
    {
        // <FS> Preview on own avatar
        if (!mUseOwnAvatar)
        {
        // </FS>
        gGL.color3f(1.f, 1.f, 1.f);
        gGL.getTexUnit(0)->bind(mAnimPreview);

        gGL.begin(LLRender::TRIANGLES);
        {
            gGL.texCoord2f(0.f, 1.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
            gGL.texCoord2f(0.f, 0.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
            gGL.texCoord2f(1.f, 0.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);

            gGL.texCoord2f(0.f, 1.f);
            gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
            gGL.texCoord2f(1.f, 0.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
            gGL.texCoord2f(1.f, 1.f);
            gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT + PREVIEW_VPAD);
        }
        gGL.end();

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        }
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>
        if (!avatarp->areAnimationsPaused())
        {
            mAnimPreview->requestUpdate();
        }
        // <FS:Ansariel> FIRE-2083: Slider in upload animation floater doesn't work
        if (mTimer.getStarted() && mTimer.hasExpired())
        {
            mTimer.stop();
            mPauseRequest = avatarp->requestPause();
        }
        // </FS:Ansariel>
    }
}

//-----------------------------------------------------------------------------
// resetMotion()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::resetMotion()
{
    if (!mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    // </FS>
    bool paused = avatarp->areAnimationsPaused();

    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    if( motionp )
    {
        // Set emotion
        std::string emote = getChild<LLUICtrl>("emote_combo")->getValue().asString();
        motionp->setEmote(mIDList[emote]);
    }

    LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
    avatarp->deactivateAllMotions();
    avatarp->startMotion(mMotionID, 0.0f);
    avatarp->startMotion(base_id, BASE_ANIM_TIME_OFFSET);
    getChild<LLUICtrl>("playback_slider")->setValue(0.0f);

    // Set pose
    std::string handpose = getChild<LLUICtrl>("hand_pose_combo")->getValue().asString();
    avatarp->startMotion( ANIM_AGENT_HAND_MOTION, 0.0f );

    if( motionp )
    {
        motionp->setHandPose(LLHandMotion::getHandPose(handpose));
    }

    if (paused)
    {
        mPauseRequest = avatarp->requestPause();
    }
    else
    {
        mPauseRequest = NULL;
    }
}

//-----------------------------------------------------------------------------
// handleMouseDown()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
    // <FS> Preview on own avatar
    if (mUseOwnAvatar) return LLFloater::handleMouseDown(x, y, mask);

    if (mPreviewRect.pointInRect(x, y))
    {
        bringToFront( x, y );
        gFocusMgr.setMouseCapture(this);
        gViewerWindow->hideCursor();
        mLastMouseX = x;
        mLastMouseY = y;
        return true;
    }

    return LLFloater::handleMouseDown(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleMouseUp()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
    // <FS> Preview on own avatar
    if (mUseOwnAvatar) return LLFloater::handleMouseUp(x, y, mask);

    gFocusMgr.setMouseCapture(nullptr);
    gViewerWindow->showCursor();
    return LLFloater::handleMouseUp(x, y, mask);
}

//-----------------------------------------------------------------------------
// handleHover()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleHover(S32 x, S32 y, MASK mask)
{
    // <FS> Preview on own avatar
    if (!mUseOwnAvatar)
    {
    // </FS>
    MASK local_mask = mask & ~MASK_ALT;
    if (mAnimPreview && hasMouseCapture())
    {
        if (local_mask == MASK_PAN)
        {
            // pan here
            mAnimPreview->pan((F32)(x - mLastMouseX) * -0.005f, (F32)(y - mLastMouseY) * -0.005f);
        }
        else if (local_mask == MASK_ORBIT)
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

            mAnimPreview->rotate(yaw_radians, pitch_radians);
        }
        else
        {
            F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
            F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

            mAnimPreview->rotate(yaw_radians, 0.f);
            mAnimPreview->zoom(zoom_amt);
        }

        mAnimPreview->requestUpdate();

        LLUI::getInstance()->setMousePositionLocal(this, mLastMouseX, mLastMouseY);
    }

    if (!mPreviewRect.pointInRect(x, y) || !mAnimPreview)
    {
    return LLFloater::handleHover(x, y, mask);
    }
    else if (local_mask == MASK_ORBIT)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLCAMERA);
    }
    else if (local_mask == MASK_PAN)
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLPAN);
    }
    else
    {
        gViewerWindow->setCursor(UI_CURSOR_TOOLZOOMIN);
    }
    // <FS> Preview on own avatar
    }
    // </FS>
    return true;
}

//-----------------------------------------------------------------------------
// handleScrollWheel()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
    // <FS> Preview on own avatar
    if (mUseOwnAvatar)
        return false;
    // </FS>

    if (!mAnimPreview)
        return false;

    mAnimPreview->zoom((F32)clicks * -0.2f);
    mAnimPreview->requestUpdate();

    return true;
}

//-----------------------------------------------------------------------------
// onMouseCaptureLost()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onMouseCaptureLost()
{
    // <FS> Preview on own avatar
    //gViewerWindow->showCursor();
    if (!mUseOwnAvatar)
    {
        gViewerWindow->showCursor();
    }
    // </FS>
}

//-----------------------------------------------------------------------------
// onBtnPlay()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnPlay()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>

        if (!avatarp->isMotionActive(mMotionID))
        {
            resetMotion();
            mPauseRequest = NULL;
        }
        else if (avatarp->areAnimationsPaused())
        {
            mPauseRequest = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// onBtnPause()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnPause()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>

        if (avatarp->isMotionActive(mMotionID))
        {
            if (!avatarp->areAnimationsPaused())
            {
                mPauseRequest = avatarp->requestPause();
            }
        }
    }
}

//-----------------------------------------------------------------------------
// onBtnStop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnStop()
{
    if (!getEnabled())
        return;

    if (mMotionID.notNull() && mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>
        resetMotion();
        mPauseRequest = avatarp->requestPause();
    }
}

//-----------------------------------------------------------------------------
// onSliderMove()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onSliderMove()
{
    if (!getEnabled())
        return;

    if (mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>
        F32 slider_value = (F32)getChild<LLUICtrl>("playback_slider")->getValue().asReal();
        LLUUID base_id = mIDList[getChild<LLUICtrl>("preview_base_anim")->getValue().asString()];
        LLMotion* motionp = avatarp->findMotion(mMotionID);
        F32 duration = motionp->getDuration();// + motionp->getEaseOutDuration();
        F32 delta_time = duration * slider_value;
        avatarp->deactivateAllMotions();
        avatarp->startMotion(base_id, delta_time + BASE_ANIM_TIME_OFFSET);
        avatarp->startMotion(mMotionID, delta_time);
        // <FS:Ansariel> FIRE-2083: Slider in upload animation floater doesn't work
        //mPauseRequest = avatarp->requestPause();
        mPauseRequest = NULL;
        mTimer.resetWithExpiry(0.001f);
        mTimer.start();
        // </FS:Ansariel>
        refresh();
    }

}

//-----------------------------------------------------------------------------
// onCommitBaseAnim()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitBaseAnim()
{
    if (!getEnabled())
        return;

    if (mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>

        bool paused = avatarp->areAnimationsPaused();

        // stop all other possible base motions
        avatarp->stopMotion(mIDList["Standing"], true);
        avatarp->stopMotion(mIDList["Walking"], true);
        avatarp->stopMotion(mIDList["Sitting"], true);
        avatarp->stopMotion(mIDList["Flying"], true);

        resetMotion();

        if (!paused)
        {
            mPauseRequest = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
// onCommitLoop()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoop()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        motionp->setLoop(getChild<LLUICtrl>("loop_check")->getValue().asBoolean());
        motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() * 0.01f * motionp->getDuration());
        motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
    }
}

//-----------------------------------------------------------------------------
// onCommitLoopIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopIn()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
        //motionp->setLoopIn((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f);
        getChild<LLUICtrl>("loop_in_frames")->setValue(LLSD((F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal() / 100.f * (F32)mNumFrames));
        // </FS:Sei>
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        onCommitLoop();
    }
}

//-----------------------------------------------------------------------------
// onCommitLoopOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopOut()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
        //motionp->setLoopOut((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() * 0.01f * motionp->getDuration());
        getChild<LLUICtrl>("loop_out_frames")->setValue(LLSD((F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal() / 100.f * (F32)mNumFrames));
        // </FS:Sei>
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        onCommitLoop();
    }
}

// <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
//-----------------------------------------------------------------------------
// onCommitLoopInFrames()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopInFrames()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // Preview on own avatar
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));

    if (motionp)
    {
        getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(mNumFrames == 0 ? 0.f : 100.f * (F32)getChild<LLUICtrl>("loop_in_frames")->getValue().asReal() / (F32)mNumFrames));
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        // The values are actually set here:
        onCommitLoop();
    }
}

//-----------------------------------------------------------------------------
// onCommitLoopOutFrames()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitLoopOutFrames()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // Preview on own avatar
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));

    if (motionp)
    {
        getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(mNumFrames == 0 ? 100.f : 100.f * (F32)getChild<LLUICtrl>("loop_out_frames")->getValue().asReal() / (F32)mNumFrames));
        resetMotion();
        getChild<LLUICtrl>("loop_check")->setValue(LLSD(true));
        onCommitLoop();
    }
}
// </FS:Sei>

//-----------------------------------------------------------------------------
// onCommitName()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitName()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        motionp->setName(getChild<LLUICtrl>("name_form")->getValue().asString());
    }

    doCommit();
}

//-----------------------------------------------------------------------------
// onCommitHandPose()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitHandPose()
{
    if (!getEnabled())
        return;

    resetMotion(); // sets hand pose
}

//-----------------------------------------------------------------------------
// onCommitEmote()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEmote()
{
    if (!getEnabled())
        return;

    resetMotion(); // ssts emote
}

//-----------------------------------------------------------------------------
// onCommitPriority()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitPriority()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        motionp->setPriority(llfloor((F32)getChild<LLUICtrl>("priority")->getValue().asReal()));
    }
}

//-----------------------------------------------------------------------------
// onCommitEaseIn()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseIn()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        motionp->setEaseIn((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal());
    }
    resetMotion();
}

//-----------------------------------------------------------------------------
// onCommitEaseOut()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onCommitEaseOut()
{
    if (!getEnabled() || !mAnimPreview)
        return;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp)
    {
        motionp->setEaseOut((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal());
    }
    resetMotion();
}

//-----------------------------------------------------------------------------
// validateEaseIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseIn(const LLSD& data)
{
    if (!getEnabled() || !mAnimPreview)
        return false;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp && !motionp->getLoop())
    {
        F32 new_ease_in = llclamp((F32)getChild<LLUICtrl>("ease_in_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseOutDuration());
        getChild<LLUICtrl>("ease_in_time")->setValue(LLSD(new_ease_in));
    }

    return true;
}

//-----------------------------------------------------------------------------
// validateEaseOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateEaseOut(const LLSD& data)
{
    if (!getEnabled() || !mAnimPreview)
        return false;

    // <FS> Preview on own avatar
    //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
    //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
    LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
    LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
    // </FS>

    if (motionp && !motionp->getLoop())
    {
        F32 new_ease_out = llclamp((F32)getChild<LLUICtrl>("ease_out_time")->getValue().asReal(), 0.f, motionp->getDuration() - motionp->getEaseInDuration());
        getChild<LLUICtrl>("ease_out_time")->setValue(LLSD(new_ease_out));
    }

    return true;
}

//-----------------------------------------------------------------------------
// validateLoopIn()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopIn(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();
    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();

    if (loop_in_value < 0.f)
    {
        loop_in_value = 0.f;
    }
    else if (loop_in_value > 100.f)
    {
        loop_in_value = 100.f;
    }
    else if (loop_in_value > loop_out_value)
    {
        loop_in_value = loop_out_value;
    }

    getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(loop_in_value));
    // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
    // <FS:Beq> FIRE-21330: (additional cleanup) make loop out round to an integer
    //  getChild<LLUICtrl>("loop_in_frames")->setValue(LLSD(loop_in_value / 100.f * (F32)mNumFrames));
    getChild<LLUICtrl>("loop_in_frames")->setValue(LLSD(ll_round(loop_in_value / 100.f * (F32)mNumFrames)));
    // </FS:Beq>
    // </FS:Sei>
    return true;
}

//-----------------------------------------------------------------------------
// validateLoopOut()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopOut(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_point")->getValue().asReal();
    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_point")->getValue().asReal();

    if (loop_out_value < 0.f)
    {
        loop_out_value = 0.f;
    }
    else if (loop_out_value > 100.f)
    {
        loop_out_value = 100.f;
    }
    else if (loop_out_value < loop_in_value)
    {
        loop_out_value = loop_in_value;
    }

    getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(loop_out_value));
    // <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
    // <FS:Beq> FIRE-21330: (additional cleanup) make loop out round to an integer
//  getChild<LLUICtrl>("loop_out_frames")->setValue(LLSD(loop_out_value / 100.f * (F32)mNumFrames));
    getChild<LLUICtrl>("loop_out_frames")->setValue(LLSD(ll_round(loop_out_value / 100.f * (F32)mNumFrames)));
    // </FS:Beq>
    // </FS:Sei>
    return true;
}


// <FS:Sei> FIRE-17277: Allow entering Loop In/Loop Out as frames
//-----------------------------------------------------------------------------
// validateLoopInFrames()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopInFrames(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_frames")->getValue().asReal();
    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_frames")->getValue().asReal();

    if (loop_in_value < 0.f)
    {
        loop_in_value = 0.f;
    }
    // <FS:Beq> FIRE-21330: Max value for the ctrl is 1000 not 100.
//  else if (loop_in_value > 100.f)
    else if (loop_in_value > 1000.f)
    {
//      loop_in_value = 100.f;
        loop_in_value = 1000.f;
    }
    else if (loop_in_value > loop_out_value)
    {
        loop_in_value = loop_out_value;
    }

    getChild<LLUICtrl>("loop_in_frames")->setValue(LLSD(loop_in_value));
    getChild<LLUICtrl>("loop_in_point")->setValue(LLSD(mNumFrames == 0 ? 0.f : 100.f * loop_in_value / (F32)mNumFrames));
    return true;
}

//-----------------------------------------------------------------------------
// validateLoopOutFrames()
//-----------------------------------------------------------------------------
bool LLFloaterBvhPreview::validateLoopOutFrames(const LLSD& data)
{
    if (!getEnabled())
        return false;

    F32 loop_out_value = (F32)getChild<LLUICtrl>("loop_out_frames")->getValue().asReal();
    F32 loop_in_value = (F32)getChild<LLUICtrl>("loop_in_frames")->getValue().asReal();

    if (loop_out_value < 0.f)
    {
        loop_out_value = 0.f;
    }
    // <FS:Beq> FIRE-21330: Max value for the ctrl is 1000 not 100.
    //  else if (loop_out_value > 100.f)
    else if (loop_out_value > 1000.f)
    {
        //      loop_out_value = 100.f;
        loop_out_value = 1000.f;
    }
    else if (loop_out_value < loop_in_value)
    {
        loop_out_value = loop_in_value;
    }

    getChild<LLUICtrl>("loop_out_frames")->setValue(LLSD(loop_out_value));
    getChild<LLUICtrl>("loop_out_point")->setValue(LLSD(mNumFrames == 0 ? 100.f : 100.f * loop_out_value / (F32)mNumFrames));
    return true;
}
// </FS:Sei>


//-----------------------------------------------------------------------------
// refresh()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::refresh()
{
    // Are we showing the play button (default) or the pause button?
    bool show_play = true;
    if (!mAnimPreview)
    {
        getChildView("bad_animation_text")->setVisible(true);
        // play button visible but disabled
        mPlayButton->setEnabled(false);
        mStopButton->setEnabled(false);
        getChildView("ok_btn")->setEnabled(false);
    }
    else
    {
        getChildView("bad_animation_text")->setVisible(false);
        // re-enabled in case previous animation was bad
        mPlayButton->setEnabled(true);
        mStopButton->setEnabled(true);
        // <FS> Preview on own avatar
        //LLVOAvatar* avatarp = mAnimPreview->getDummyAvatar();
        LLVOAvatar* avatarp = mAnimPreview->getPreviewAvatar(this);
        // </FS>
        if (avatarp->isMotionActive(mMotionID))
        {
            mStopButton->setEnabled(true);
            // <FS:Ansariel> Possible memory corruption
            //LLKeyframeMotion* motionp = (LLKeyframeMotion*)avatarp->findMotion(mMotionID);
            LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(avatarp->findMotion(mMotionID));
            // </FS:Ansariel>
            if (!avatarp->areAnimationsPaused())
            {
                // animation is playing
                if (motionp)
                {
                    F32 fraction_complete = motionp->getLastUpdateTime() / motionp->getDuration();
                    getChild<LLUICtrl>("playback_slider")->setValue(fraction_complete);
                }
                show_play = false;
            }
        }
        else
        {
            // Motion just finished playing
            mPauseRequest = avatarp->requestPause();
        }
        getChildView("ok_btn")->setEnabled(true);
        mAnimPreview->requestUpdate();
    }
    mPlayButton->setVisible(show_play);
    mPauseButton->setVisible(!show_play);
}

//-----------------------------------------------------------------------------
// onBtnOK()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnOK(void* userdata)
{
    LLFloaterBvhPreview* floaterp = (LLFloaterBvhPreview*)userdata;
    if (!floaterp->getEnabled()) return;

    if (floaterp->mAnimPreview)
    {
        // <FS> Preview on own avatar
        //LLKeyframeMotion* motionp = (LLKeyframeMotion*)floaterp->mAnimPreview->getDummyAvatar()->findMotion(floaterp->mMotionID);
        LLKeyframeMotion* motionp = dynamic_cast<LLKeyframeMotion*>(floaterp->mAnimPreview->getPreviewAvatar(floaterp)->findMotion(floaterp->mMotionID));
        if (!motionp)
        {
            LL_WARNS() << "Could not find motion data." << LL_ENDL;
            LLNotificationsUtil::add("WriteAnimationFail");
            return;
        }
        // </FS>

        S32 file_size = motionp->getFileSize();
        U8* buffer = new(std::nothrow) U8[file_size];
        if (!buffer)
        {
            LLError::LLUserWarningMsg::showOutOfMemory();
            LL_ERRS() << "Bad memory allocation for buffer, size: " << file_size << LL_ENDL;
        }

        LLDataPackerBinaryBuffer dp(buffer, file_size);
        if (motionp->serialize(dp))
        {
            LLFileSystem file(motionp->getID(), LLAssetType::AT_ANIMATION, LLFileSystem::APPEND);

            S32 size = dp.getCurrentSize();
            if (file.write((U8*)buffer, size))
            {
                std::string name = floaterp->getChild<LLUICtrl>("name_form")->getValue().asString();
                std::string desc = floaterp->getChild<LLUICtrl>("description_form")->getValue().asString();
                S32 expected_upload_cost = LLAgentBenefitsMgr::current().getAnimationUploadCost();

                LLResourceUploadInfo::ptr_t assetUploadInfo(new LLResourceUploadInfo(
                    floaterp->mTransactionID, LLAssetType::AT_ANIMATION,
                    name, desc, 0,
                    LLFolderType::FT_NONE, LLInventoryType::IT_ANIMATION,
                    LLFloaterPerms::getNextOwnerPerms("Uploads"),
                    LLFloaterPerms::getGroupPerms("Uploads"),
                    LLFloaterPerms::getEveryonePerms("Uploads"),
                    expected_upload_cost,
                    floaterp->mDestinationFolderId));

                upload_new_resource(assetUploadInfo);
            }
            else
            {
                LL_WARNS() << "Failure writing animation data." << LL_ENDL;
                LLNotificationsUtil::add("WriteAnimationFail");
            }
        }

        delete [] buffer;
        // clear out cache for motion data
        // <FS> Preview on own avatar
        //floaterp->mAnimPreview->getDummyAvatar()->removeMotion(floaterp->mMotionID);
        floaterp->resetMotion();
        floaterp->mAnimPreview->getPreviewAvatar(floaterp)->removeMotion(floaterp->mMotionID);
        // </FS>
        LLKeyframeDataCache::removeKeyframeData(floaterp->mMotionID);
        // <FS> Preview on own avatar
        floaterp->mMotionID.setNull();
        // </FS>
    }

    floaterp->closeFloater(false);
}

// <FS> Reload animation from disk
//-----------------------------------------------------------------------------
// onBtnReload()
//-----------------------------------------------------------------------------
void LLFloaterBvhPreview::onBtnReload(void* userdata)
{
    LLFloaterBvhPreview* floaterp = (LLFloaterBvhPreview*)userdata;
    if (!floaterp->getEnabled()) return;

    floaterp->unloadMotion();
    floaterp->loadBVH();
    floaterp->resetMotion();
}
// </FS>

//-----------------------------------------------------------------------------
// LLPreviewAnimation
//-----------------------------------------------------------------------------
LLPreviewAnimation::LLPreviewAnimation(S32 width, S32 height) : LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, false)
{
    mNeedsUpdate = true;
    mCameraDistance = PREVIEW_CAMERA_DISTANCE;
    mCameraYaw = 0.f;
    mCameraPitch = 0.f;
    mCameraZoom = 1.f;

    mDummyAvatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, gAgent.getRegion(), LLViewerObject::CO_FLAG_UI_AVATAR);
    mDummyAvatar->mSpecialRenderMode = 1;
    mDummyAvatar->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);

    // on idle overall apperance update will set skirt to visible, so either
    // call early or account for mSpecialRenderMode in updateMeshVisibility
    mDummyAvatar->updateOverallAppearance();
    mDummyAvatar->hideHair();
    mDummyAvatar->hideSkirt();

    // stop extraneous animations
    mDummyAvatar->stopMotion( ANIM_AGENT_HEAD_ROT, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_EYE, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_BODY_NOISE, true );
    mDummyAvatar->stopMotion( ANIM_AGENT_BREATHE_ROT, true );
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation()
//-----------------------------------------------------------------------------
LLPreviewAnimation::~LLPreviewAnimation()
{
    mDummyAvatar->markDead();
}

//virtual
S8 LLPreviewAnimation::getType() const
{
    return LLViewerDynamicTexture::LL_PREVIEW_ANIMATION ;
}

//-----------------------------------------------------------------------------
// update()
//-----------------------------------------------------------------------------
bool    LLPreviewAnimation::render()
{
    mNeedsUpdate = false;
    LLVOAvatar* avatarp = mDummyAvatar;

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.pushMatrix();
    gGL.loadIdentity();
    gGL.ortho(0.0f, (F32)mFullWidth, 0.0f, (F32)mFullHeight, -1.0f, 1.0f);

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.pushMatrix();
    gGL.loadIdentity();

    gUIProgram.bind();

    LLGLSUIDefault def;
    gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
    gGL.color4f(0.15f, 0.2f, 0.3f, 1.f);

    gl_rect_2d_simple( mFullWidth, mFullHeight );

    gGL.matrixMode(LLRender::MM_PROJECTION);
    gGL.popMatrix();

    gGL.matrixMode(LLRender::MM_MODELVIEW);
    gGL.popMatrix();

    gGL.flush();

    LLVector3 target_pos = avatarp->mRoot->getWorldPosition();

    LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
        LLQuaternion(mCameraYaw, LLVector3::z_axis);

    LLViewerCamera* camera = LLViewerCamera::getInstance();

    LLQuaternion av_rot = avatarp->mRoot->getWorldRotation() * camera_rot;
    camera->setOriginAndLookAt(
        target_pos + ((LLVector3(mCameraDistance, 0.f, 0.f) + mCameraOffset) * av_rot),     // camera
        LLVector3::z_axis,                                                                  // up
        target_pos + (mCameraOffset  * av_rot) );                                           // point of interest

    camera->setViewNoBroadcast(LLViewerCamera::getInstance()->getDefaultFOV() / mCameraZoom);
    camera->setAspect((F32) mFullWidth / (F32) mFullHeight);
    camera->setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth, mFullHeight, false);

    //SJB: Animation is updated in LLVOAvatar::updateCharacter

    if (avatarp->mDrawable.notNull())
    {
        avatarp->updateLOD();

        LLVertexBuffer::unbind();
        LLGLDepthTest gls_depth(GL_TRUE);

        LLFace* face = avatarp->mDrawable->getFace(0);
        if (face)
        {
            LLDrawPoolAvatar *avatarPoolp = (LLDrawPoolAvatar *)face->getPool();
            avatarp->dirtyMesh();
            gPipeline.enableLightsPreview();
            avatarPoolp->renderAvatars(avatarp);  // renders only one avatar
        }
    }

    gGL.color4f(1,1,1,1);
    return true;
}

//-----------------------------------------------------------------------------
// requestUpdate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::requestUpdate()
{
    mNeedsUpdate = true;
}

//-----------------------------------------------------------------------------
// rotate()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::rotate(F32 yaw_radians, F32 pitch_radians)
{
    mCameraYaw = mCameraYaw + yaw_radians;

    mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

//-----------------------------------------------------------------------------
// zoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::zoom(F32 zoom_delta)
{
    setZoom(mCameraZoom + zoom_delta);
}

//-----------------------------------------------------------------------------
// setZoom()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::setZoom(F32 zoom_amt)
{
    mCameraZoom = llclamp(zoom_amt, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

//-----------------------------------------------------------------------------
// pan()
//-----------------------------------------------------------------------------
void LLPreviewAnimation::pan(F32 right, F32 up)
{
    mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom, -1.f, 1.f);
    mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom, -1.f, 1.f);
}

// <FS> Preview on own avatar
LLVOAvatar* LLPreviewAnimation::getPreviewAvatar(LLFloaterBvhPreview* floaterp)
{
    if (!floaterp)
    {
        // Don't return null - missing safety checks all around the code!
        return (LLVOAvatar*)mDummyAvatar;
    }
    return floaterp->mUseOwnAvatar ? (LLVOAvatar*)gAgentAvatarp : (LLVOAvatar*)mDummyAvatar;
}
// </FS>

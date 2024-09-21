/*
 * @file fsfloaterposestand.cpp
 * @brief It's a pose stand!
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterposestand.h"

#include "fspose.h"
#include "llagent.h"
#include "llvoavatarself.h"
#include "llsdserialize.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "rlvhandler.h"

FSFloaterPoseStand::FSFloaterPoseStand(const LLSD& key)
:   LLFloater(key),
    mComboPose(nullptr),
    mPoseStandLock(false),
    mAOPaused(false)
{
}

FSFloaterPoseStand::~FSFloaterPoseStand()
{
}

bool FSFloaterPoseStand::postBuild()
{
    mComboPose = getChild<LLComboBox>("pose_combo");
    mComboPose->setCommitCallback(boost::bind(&FSFloaterPoseStand::onCommitCombo, this));
    loadPoses();

    return true;
}

// virtual
void FSFloaterPoseStand::onOpen(const LLSD& key)
{
    if (!isAgentAvatarValid())
    {
        return;
    }

    if (gSavedPerAccountSettings.getBOOL("UseAO"))
    {
        gSavedPerAccountSettings.setBOOL("UseAO", false);
        mAOPaused = true;
    }

    if (gSavedSettings.getBOOL("FSPoseStandLock")
        && !gAgentAvatarp->isSitting()
        && !gRlvHandler.hasBehaviour(RLV_BHVR_SIT))
    {
        setLock(true);
    }
    gAgent.stopCurrentAnimations(true);
    gAgent.setCustomAnim(true);
    gFocusMgr.setKeyboardFocus(nullptr);
    gFocusMgr.setMouseCapture(nullptr);
    std::string last_pose = gSavedSettings.getString("FSPoseStandLastSelectedPose");
    if (!last_pose.empty())
    {
        mComboPose->setSelectedByValue(last_pose, true);
    }
    onCommitCombo();
}

// virtual
void FSFloaterPoseStand::onClose(bool app_quitting)
{
    if (!isAgentAvatarValid())
    {
        return;
    }

    if (mPoseStandLock && gAgentAvatarp->isSitting())
    {
        setLock(false);
        gAgent.standUp();
    }
    gAgent.setCustomAnim(false);
    FSPose::getInstance()->stopPose();
    gAgent.stopCurrentAnimations(true);
    if (mAOPaused && !gSavedPerAccountSettings.getBOOL("UseAO"))
    {
        gSavedPerAccountSettings.setBOOL("UseAO", true);
        mAOPaused = false;
    }
}

void FSFloaterPoseStand::loadPoses()
{
    const std::string pose_filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "posestand.xml");
    llifstream pose_file(pose_filename.c_str());
    LLSD poses;
    if (pose_file.is_open())
    {
        if (LLSDSerialize::fromXML(poses, pose_file) >= 1)
        {
            for (LLSD::map_iterator p_itr = poses.beginMap(); p_itr != poses.endMap(); ++p_itr)
            {
                LLUUID anim_id(p_itr->first);
                if (anim_id.notNull())
                {
                    mComboPose->add(LLTrans::getString(p_itr->second["name"].asStringRef()), anim_id);
                }
            }
        }
        pose_file.close();
    }
    mComboPose->sortByName();
}

void FSFloaterPoseStand::onCommitCombo()
{
    std::string selected_pose = mComboPose->getValue();
    gSavedSettings.setString("FSPoseStandLastSelectedPose", selected_pose);
    FSPose::getInstance()->setPose(selected_pose);
}

void FSFloaterPoseStand::setLock(bool enabled)
{
    if (enabled)
    {
        gAgent.sitDown();
    }
    else
    {
        gAgent.standUp();
    }
    mPoseStandLock = enabled;
}

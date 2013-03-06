/*
 * @file fsfloaterposestand.cpp
 * @brief It's a pose stand!
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder@cinderblocks.biz>
 * ----------------------------------------------------------------------------
 */

#include "llviewerprecompiledheaders.h"
#include "fsfloaterposestand.h"

#include "fspose.h"
#include "llsdserialize.h"
#include "lltrans.h"

FSFloaterPoseStand::FSFloaterPoseStand(const LLSD& key)
:	LLFloater(key),
	mComboPose(NULL)
{
}

FSFloaterPoseStand::~FSFloaterPoseStand()
{
}

BOOL FSFloaterPoseStand::postBuild()
{
	mComboPose = getChild<LLComboBox>("pose_combo");
	mComboPose->setCommitCallback(boost::bind(&FSFloaterPoseStand::onCommitCombo, this));
	loadPoses();
	onCommitCombo();

	return TRUE;
}

// virtual
void FSFloaterPoseStand::onOpen(const LLSD& key)
{
	onCommitCombo();
}

// virtual
void FSFloaterPoseStand::onClose(bool app_quitting)
{
	FSPose::getInstance()->stopPose();
}

void FSFloaterPoseStand::loadPoses()
{
	const std::string pose_filename = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "posestand.xml");
	llifstream pose_file(pose_filename);
	LLSD poses;
	if (pose_file.is_open())
	{
		if(LLSDSerialize::fromXML(poses, pose_file) >= 1)
		{
			for(LLSD::map_iterator p_itr = poses.beginMap(); p_itr != poses.endMap(); ++p_itr)
			{
				mComboPose->add(LLTrans::getString(p_itr->second["name"]), LLUUID(p_itr->first));
			}
		}
		pose_file.close();
	}
	mComboPose->sortByName();
}

void FSFloaterPoseStand::onCommitCombo()
{
	std::string selected_pose = mComboPose->getValue();
	FSPose::getInstance()->setPose(selected_pose);
}

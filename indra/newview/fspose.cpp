/*
 * @file fspose.cpp
 * @brief Pose stuff
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder@cinderblocks.biz>
 * ----------------------------------------------------------------------------
 */

#include "llviewerprecompiledheaders.h"
#include "fspose.h"

#include "llagent.h"
#include "llvoavatarself.h"

FSPose::FSPose()
:	mCurrentPose(LLUUID::null)
{
}

FSPose::~FSPose()
{
}

void FSPose::setPose(std::string new_pose)
{
	if (isAgentAvatarValid())
	{
		if (mCurrentPose.notNull())
			gAgent.sendAnimationRequest(mCurrentPose, ANIM_REQUEST_STOP);
		mCurrentPose.set(new_pose);
		gAgent.sendAnimationRequest(mCurrentPose, ANIM_REQUEST_START);
	}
}

void FSPose::stopPose()
{
	if (isAgentAvatarValid() && mCurrentPose != LLUUID::null)
	{
		gAgent.sendAnimationRequest(mCurrentPose, ANIM_REQUEST_STOP);
		mCurrentPose.setNull();
	}
}
/*
 * @file fspose.h
 * @brief Animation Posing Definitions
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 */

#ifndef FS_POSE_H
#define FS_POSE_H

#include "llsingleton.h"

class FSPose : public LLSingleton<FSPose>
{
    LOG_CLASS(FSPose);

    LLSINGLETON(FSPose);
    ~FSPose();

public:
    void setPose(const std::string& new_pose, bool save_state = true);
    void stopPose();

private:
    LLUUID  mCurrentPose;
};

#endif // FS_POSE_H

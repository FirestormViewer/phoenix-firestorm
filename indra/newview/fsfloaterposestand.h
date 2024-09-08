 /*
 * @file fsfloaterposestand.h
 * @brief Pose stand definitions
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder.roxley@phoenixviewer.com>
 * ----------------------------------------------------------------------------
 */

#ifndef FS_FLOATERPOSESTAND_H
#define FS_FLOATERPOSESTAND_H

#include "llfloater.h"
#include "llcombobox.h"

class FSFloaterPoseStand
:   public LLFloater
{
    LOG_CLASS(FSFloaterPoseStand);
public:
    FSFloaterPoseStand(const LLSD& key);
    bool postBuild() override;
    void setLock(bool enabled);
    void onCommitCombo();
private:
    ~FSFloaterPoseStand();
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void loadPoses();

    bool mAOPaused;
    bool mPoseStandLock;
    LLComboBox* mComboPose;
};

#endif // FS_FLOATERPOSESTAND_H

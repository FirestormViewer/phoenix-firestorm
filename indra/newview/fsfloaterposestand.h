 /*
 * @file fsfloaterposestand.h
 * @brief Pose stand definitions
 *
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * Cinder Roxley wrote this file. As long as you retain this notice you can do
 * whatever you want with this stuff. If we meet some day, and you think this
 * stuff is worth it, you can buy me a beer in return <cinder@cinderblocks.biz>
 * ----------------------------------------------------------------------------
 */

#ifndef FS_FLOATERPOSESTAND_H
#define FS_FLOATERPOSESTAND_H

#include "llfloater.h"
#include "llcombobox.h"

class FSFloaterPoseStand
:	public LLFloater
{
	LOG_CLASS(FSFloaterPoseStand);
public:
	FSFloaterPoseStand(const LLSD& key);
	BOOL postBuild();
	void setLock(bool enabled);
	void onCommitCombo();
private:
	~FSFloaterPoseStand();
	virtual void onOpen(const LLSD& key);
	virtual void onClose(bool app_quitting);
	void loadPoses();
	
	bool mPoseStandLock;
	LLComboBox* mComboPose;
};

#endif // FS_FLOATERPOSESTAND_H

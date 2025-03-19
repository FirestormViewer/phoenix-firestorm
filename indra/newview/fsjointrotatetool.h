#ifndef FS_JOINTROTATETOOL_H
#define FS_JOINTROTATETOOL_H

#include "lltool.h"
#include "llbbox.h"
#include "llvoavatar.h"

class FSJointRotateTool : public LLTool, public LLSingleton<FSJointRotateTool>
{
    LLSINGLETON(FSJointRotateTool);

public:
    void handleSelect() override;
    void handleDeselect() override;
    bool handleMouseDown(S32 x, S32 y, MASK mask) override;
    bool handleHover(S32 x, S32 y, MASK mask) override;
    void render() override;

private:
    void rotateJoint(LLVOAvatar* avatar);
    bool findSelectedManipulator(S32 x, S32 y);
    void computeManipulatorSize();
    void renderManipulators();

    LLBBox mBBox;
    F32 mManipulatorSize;
    S32 mHighlightedAxis;
    F32 mHighlightedDirection;
    bool mForce;
};

#endif // FS_JOINTROTATETOOL_H

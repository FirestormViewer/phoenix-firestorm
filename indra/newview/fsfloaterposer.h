/**
 * @file fsfloaterposer.h
 * @brief View Model for posing your (and other) avatar(s).
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2024 Angeldark Raymaker @ Second Life
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


#ifndef FS_FLOATER_POSER_H
#define FS_FLOATER_POSER_H

#include "llfloater.h"
#include "lltoolmgr.h"
#include "fsposeranimator.h"

class FSVirtualTrackpad;
class LLButton;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLScrollListCtrl;
class LLSliderCtrl;
class LLTabContainer;

/// <summary>
/// Describes how to load a pose file.
/// </summary>
typedef enum E_LoadPoseMethods
{
    ROTATIONS               = 1,
    POSITIONS               = 2,
    SCALES                  = 3,
    ROTATIONS_AND_POSITIONS = 4,
    ROTATIONS_AND_SCALES    = 5,
    POSITIONS_AND_SCALES    = 6,
    ROT_POS_AND_SCALES      = 7,
    HAND_RIGHT              = 8,
    HAND_LEFT               = 9,
    FACE_ONLY               = 10,
    SELECTIVE               = 11,
    SELECTIVE_ROT           = 12,
} E_LoadPoseMethods;

/// <summary>
/// Describes the columns of the avatars scroll-list.
/// </summary>
typedef enum E_Columns
{
    COL_ICON = 0,
    COL_NAME = 1,
    COL_UUID = 2,
    COL_SAVE = 3,
} E_Columns;

/// <summary>
/// A class containing the UI fiddling for the Poser Floater.
/// Please don't do LLJoint stuff here, fsposingmotion (the LLMotion derivative) is the class for that.
/// </summary>
class FSFloaterPoser : public LLFloater, public LLEditMenuHandler
{
    friend class LLFloaterReg;
    FSFloaterPoser(const LLSD &key);
public:
    void updatePosedBones(const std::string& jointName);
    void selectJointByName(const std::string& jointName);
    void undo() override { onUndoLastChange(); };
    bool canUndo() const override { return true; }
    void redo() override { onRedoLastChange(); };
    bool canRedo() const override { return true; }
 private:
    // Helper function to encapsualte save logic
    void doPoseSave(LLVOAvatar* avatar, const std::string& filename);
    bool postBuild() override;
    void onOpen(const LLSD& key) override;
    void onClose(bool app_quitting) override;
    void onFocusReceived() override;
    void onFocusLost() override;
    /// <summary>
    /// Refreshes the supplied pose list from the supplued subdirectory.
    /// </summary>
    void refreshPoseScroll(LLScrollListCtrl* posesScrollList, std::optional<std::string_view> subDirectory = std::nullopt);

    /// <summary>
    /// (Dis)Enables all of the posing controls; such as when you can't pose for reasons.
    /// </summary>
    /// <param name="enable">Whether to enable the pose controls.</param>
    void poseControlsEnable(bool enable);

    /// <summary>
    /// Refreshes all of the 'joint/bones/thingos' lists (lists-zah, all of them).
    /// </summary>
    void refreshJointScrollListMembers();

    /// <summary>
    /// Adds a 'header' menu item to the supplied scroll list, handy for demarking clusters of joints on the UI.
    /// </summary>
    /// <param name="jointName">
    /// The well-known name of the joint, eg: mChest
    /// This does a lookup into the poser XML for a friendly header title by joint name, if it exists.
    /// </param>
    /// <param name="bodyJointsScrollList">The scroll list to add the header-row to.</param>
    void addHeaderRowToScrollList(const std::string& jointName, LLScrollListCtrl* bodyJointsScrollList);

    /// <summary>
    /// Generates the data for a row to add to a scroll-list.
    /// The supplied joint name is looked up in the UI XML to find a friendly name.
    /// </summary>
    /// <param name="jointName">The well-known joint name of the joint to add the row for, eg: mChest.</param>
    /// <param name="isHeaderRow">Whether the joint is one which should come immediately after a header.</param>
    /// <returns>The data required to make the row.</returns>
    LLSD createRowForJoint(const std::string& jointName, bool isHeaderRow);

    /// <summary>
    /// Gets the collection of poser joints currently selected on the active bones-tab of the UI.
    /// </summary>
    /// <returns>The selected joints</returns>
    std::vector<FSPoserAnimator::FSPoserJoint*> getUiSelectedPoserJoints() const;

    /// <summary>
    /// Updates the visual with the first selected joint from the supplied collection, if any.
    /// </summary>
    /// <param name="joints">The collection of selected joints.</param>
    void updateManipWithFirstSelectedJoint(std::vector<FSPoserAnimator::FSPoserJoint*> joints) const;

    /// <summary>
    /// Gets a detectable avatar by its UUID.
    /// </summary>
    /// <param name="avatarToFind">The ID of the avatar to find.</param>
    /// <returns>The avatar, if found, otherwise nullptr.</returns>
    LLVOAvatar* getAvatarByUuid(const LLUUID& avatarToFind) const;

    /// <summary>
    /// Gets the currently selected avatar or animesh.
    /// </summary>
    /// <returns>The currently selected avatar or animesh.</returns>
    LLVOAvatar* getUiSelectedAvatar() const;

    /// <summary>
    /// Sets the UI selection for avatar or animesh.
    /// </summary>
    /// <param name="avatarToSelect">The ID of the avatar to select, if found.</param>
    void setUiSelectedAvatar(const LLUUID& avatarToSelect);

    /// <summary>
    /// Gets the current bone-deflection style: encapsulates 'anything else you want to do' while you're manipulating a joint.
    /// Such as: fiddle the opposite joint too.
    /// </summary>
    /// <returns>A E_BoneDeflectionStyles member.</returns>
    E_BoneDeflectionStyles getUiSelectedBoneDeflectionStyle() const;

    /// <summary>
    /// Gets the means by which the rotation should be applied to the supplied joint name.
    /// Such as: fiddle the opposite joint too.
    /// </summary>
    /// <param name="jointName">The well-known joint name of the joint to add the row for, eg: mChest.</param>
    /// <returns>A E_RotationStyle member.</returns>
    E_RotationStyle getUiSelectedBoneRotationStyle(const std::string& jointName) const;

    /// <summary>
    /// Gets the collection of UUIDs for nearby avatars.
    /// </summary>
    /// <returns>A the collection of UUIDs for nearby avatars.</returns>
    uuid_vec_t getNearbyAvatarsAndAnimeshes() const;

    /// <summary>
    /// Gets a collection of UUIDs for avatars currently being presented on the UI.
    /// </summary>
    /// <returns>A the collection of UUIDs.</returns>
    uuid_vec_t getCurrentlyListedAvatarsAndAnimeshes() const;

    /// <summary>
    /// Gets the scroll-list index of the supplied avatar.
    /// </summary>
    /// <param name="toFind">The avatar UUID to find on the avatars scroll list.</param>
    /// <returns>The scroll-list index for the supplied avatar, if found, otherwise -1.</returns>
    S32 getAvatarListIndexForUuid(const LLUUID& toFind) const;
    
    /// <summary>
    /// There are several control-callbacks manipulating rotations etc, they all devolve to these.
    /// In these are the appeals to the posing business layer.
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>    
    void setSelectedJointsRotation(const LLVector3& absoluteRot, const LLVector3& deltaRot);
    void setSelectedJointsPosition(F32 x, F32 y, F32 z);
    void setSelectedJointsScale(F32 x, F32 y, F32 z);
    /// <summary>
    /// Yeilds the rotation of the first selected joint (one may multi-select).
    /// </summary>
    /// <remarks>
    /// Using a set, then a get does not guarantee the value you just set.
    /// There may be +/- PI difference two axes, because harmonics.
    /// Thus keep your UI synced with less gets.
    /// </remarks>
    LLVector3 getRotationOfFirstSelectedJoint() const;
    LLVector3 getPositionOfFirstSelectedJoint() const;
    LLVector3 getScaleOfFirstSelectedJoint() const;

    LLScrollListCtrl* getScrollListForTab(LLPanel * tabPanel) const;
    // Pose load/save
    void createUserPoseDirectoryIfNeeded();
    void onToggleLoadSavePanel();
    void onClickPoseSave();
    void onMouseLeaveSavePoseBtn();
    void onPoseFileSelect();
    bool savePoseToXml(LLVOAvatar* avatar, const std::string& posePath);
    bool savePoseToBvh(LLVOAvatar* avatar, const std::string& posePath);
    void onClickBrowsePoseCache();
    void onPoseMenuAction(const LLSD& param);
    void loadPoseFromXml(LLVOAvatar* avatar, const std::string& poseFileName, E_LoadPoseMethods loadMethod);
    bool poseFileStartsFromTeePose(const std::string& poseFileName);
    void setPoseSaveFileTextBoxToUiSelectedAvatarSaveFileName();
    void setUiSelectedAvatarSaveFileName(const std::string& saveFileName);
    bool confirmFileOverwrite(std::string fileName);
    void startPosingSelf();
    void stopPosingAllAvatars();
    // visual manipulators control
    void enableVisualManipulators();
    void disableVisualManipulators();

    // UI Event Handlers
    void onAvatarsRefresh();
    void onAvatarSelect();
    void onJointTabSelect();
    void onToggleMirrorChange();
    void onToggleSympatheticChange();
    void onToggleVisualManipulators();
    void setRotationChangeButtons(bool mirror, bool sympathetic);
    void onUndoLastChange();
    void onRedoLastChange();
    void onResetJoint(const LLSD data);
    void onSetAvatarToTpose();
    void onPoseStartStop();
    void onTrackballChanged();
    void onYawPitchRollChanged(bool skipUpdateTrackpad = false);
    void onPositionSet();
    void onScaleSet();
    void onClickToggleSelectedBoneEnabled();
    void onClickRecaptureSelectedBones();
    void onClickFlipPose();
    void onClickFlipSelectedJoints();
    void onAdjustTrackpadSensitivity();
    void onClickLoadLeftHandPose();
    void onClickLoadRightHandPose();
    void onClickLoadHandPose(bool isRightHand);
    void onClickSetBaseRotZero();
    void onCommitSpinner(const LLUICtrl* spinner, const S32 ID);
    void onCommitSlider(const LLUICtrl* slider, const S32 id);
    void onClickSymmetrize(const S32 ID);

    // UI Refreshments
    void refreshRotationSlidersAndSpinners();
    void refreshPositionSlidersAndSpinners();
    void refreshScaleSlidersAndSpinners();
    void refreshTrackpadCursor();
    void enableOrDisableRedoAndUndoButton();

    /// <summary>
    /// Determines if we have permission to animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if we have permission to animate, otherwise false.</returns>
    bool havePermissionToAnimateAvatar(LLVOAvatar* avatar) const;

    /// <summary>
    /// Determines if we could animate the supplied avatar.
    /// </summary>
    /// <param name="avatar">The avatar to animate.</param>
    /// <returns>True if the avatar is non-null, not dead, in the same region as self, otherwise false.</returns>
    bool couldAnimateAvatar(LLVOAvatar* avatar) const;

    /// <summary>
    /// Our instance of the class which lets us do the business of manipulating the avatar.
    /// This separates that business from the code-behind the UI.
    /// </summary>
    FSPoserAnimator mPoserAnimator;

    /// <summary>
    /// The supplied Joint name has a quaternion describing its rotation.
    /// This gets the kind of axial transformation required for 'easy' consumption of the joint's Euler angles on our UI.
    /// This facilitates 'conceptual' conversion of Euler frame to up/down, left/right and roll and is rather subjective.
    /// Thus, many of these 'conversions' are backed by values in the XML.
    /// </summary>
    /// <param name="jointName">The well-known name of the joint, eg: mChest.</param>
    /// <returns>The axial translation so the oily angles make better sense in terms of up/down/left/right/roll.</returns>
    /// <remarks>
    /// Euler angles aren't cartesian; they're one of 12 possible orderings or something, yes, yes.
    /// No the translation isn't untangling all of that, it's not needed until it is.
    /// We're not landing on Mars with this code, just offering a user reasonable thumb-twiddlings.
    /// </remarks>
    E_BoneAxisTranslation getJointTranslation(const std::string& jointName) const;

    /// <summary>
    /// Gets the collection of E_BoneAxisNegation values for the supplied joint.
    /// </summary>
    /// <param name="jointName">The name of the joind to get the axis transformation for.</param>
    /// <returns>The kind of axis transformation to perform.</returns>
    S32 getJointNegation(const std::string& jointName) const;

    /// <summary>
    /// Gets the axial translation required for joints when saving to BVH.
    /// </summary>
    /// <param name="jointName">The name of the joint to get the transformation for.</param>
    /// <returns>The axial translation required.</returns>
    E_BoneAxisTranslation getBvhJointTranslation(const std::string& jointName) const;

    S32 getBvhJointNegation(const std::string& jointName) const;

    /// <summary>
    /// Refreshes the text on the avatars scroll list based on their state.
    /// </summary>
    void refreshTextHighlightingOnAvatarScrollList();

    /// <summary>
    /// Refreshes the text on all joints scroll lists based on their state.
    /// </summary>
    void refreshTextHighlightingOnJointScrollLists();

    /// <summary>
    /// Sets the text of the save pose button.
    /// </summary>
    /// <param name="setAsSaveDiff">Whether to indicate a diff will be saved, instead of a pose.</param>
    void setSavePosesButtonText(bool setAsSaveDiff);

    /// <summary>
    /// Applies the appropriate font-face (such as bold) to the text of the supplied list, to indicate use.
    /// </summary>
    /// <param name="listName">The name of the list to adjust text-face for.</param>
    /// <param name="avatar">The avatar to whom the list is relevant.</param>
    void addBoldToScrollList(LLScrollListCtrl* list, LLVOAvatar* avatar);

    /// <summary>
    /// Gets whether the user wishes to reset the base-rotation to zero when they start editing a joint.
    /// </summary>
    /// <remarks>
    /// If a joint has a base-rotation of zero, the rotation then appears to be the user's work and qualifies to save to a re-importable format.
    /// </remarks>
    bool getWhetherToResetBaseRotationOnEdit();

    /// <summary>
    /// Gets the name of an item from the supplied object ID.
    /// </summary>
    /// <param name="avatar">The control avatar to get the name for.</param>
    /// <returns>The name of the supplied object.</returns>
    /// <devnotes>
    /// Getting the name for an arbitrary item appears to involve sending system message and creating a
    /// callback, making for unwanted dependencies and conflict-risk; so not implemented.
    /// </devnotes>
    std::string getControlAvatarName(const LLControlAvatar* avatar);

    /// Gets whether the pose should also write a BVH file when saved.
    /// </summary>
    /// <returns>True if the user wants to additionally save a BVH file, otherwise false.</returns>
    bool getSavingToBvh();

    /// <summary>
    /// Writes the current pose in BVH-format to the supplied stream.
    /// </summary>
    /// <param name="fileStream">The stream to write the pose to.</param>
    /// <param name="avatar">The avatar whose pose should be saved.</param>
    /// <returns>True if the pose saved successfully as a BVH, otherwise false.</returns>
    /// <remarks>
    /// Only joints with a zero base-rotation should export to BVH.
    /// </remarks>
    bool writePoseAsBvh(llofstream* fileStream, LLVOAvatar* avatar);

    /// <summary>
    /// Recursively writes a fragment of a BVH file format representation of the supplied joint, then that joints BVH child(ren).
    /// None of what is written here matters a jot; it's just here so it parses on read.
    /// </summary>
    /// <param name="fileStream">The stream to write the fragment to.</param>
    /// <param name="avatar">The avatar owning the supplied joint.</param>
    /// <param name="joint">The joint whose fragment should be written, and whose child(ren) will also be written.</param>
    /// <param name="tabStops">The number of tab-stops to include for formatting purpose.</param>
    void writeBvhFragment(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserAnimator::FSPoserJoint* joint, S32 tabStops);

    /// <summary>
    /// Writes a fragment of the 'single line' representing an animation frame within the BVH file respresenting the positions and/or
    /// rotations.
    /// </summary>
    /// <param name="fileStream">The stream to write the position and/or rotation to.</param>
    /// <param name="avatar">The avatar owning the supplied joint.</param>
    /// <param name="joint">The joint whose position and/or rotation should be written.</param>
    void writeBvhMotion(llofstream* fileStream, LLVOAvatar* avatar, const FSPoserAnimator::FSPoserJoint* joint);

    /// <summary>
    /// Writes a fragment of the 'single line' representing the first animation frame within the BVH file respresenting the positions and/or
    /// rotations.
    /// </summary>
    /// <param name="fileStream">The stream to write the position and/or rotation to.</param>
    /// <param name="joint">The joint whose position and/or rotation should be written.</param>
    void writeFirstFrameOfBvhMotion(llofstream* fileStream, const FSPoserAnimator::FSPoserJoint* joint);

    /// <summary>
    /// Generates a string with the supplied number of tab-chars.
    /// </summary>
    std::string static getTabs(S32 numOfTabstops);

    /// <summary>
    /// Transforms a rotation such that llbvhloader.cpp can resolve it to something vaguely approximating the supplied angle.
    /// When I say vague, I mean, it's numbers, buuuuut.
    /// </summary>
    std::string static rotationToString(const LLVector3& val);

    /// <summary>
    /// Transforms the supplied vector into a string of three numbers, format suiting to writing into a BVH file.
    /// </summary>
    std::string static positionToString(const LLVector3& val);

    /// <summary>
    /// Performs an angle module of the supplied value to between -180 & 180 (degrees).
    /// </summary>
    /// <param name="value">The value to modulo.</param>
    /// <returns>The modulo value.</returns>
    /// <remarks>
    /// If the trackpad is in 'infinite scroll' mode, it can produce normalized-values outside the range of the spinners.
    /// This method ensures whatever value the trackpad produces, they work with the spinners.
    /// </remarks>
    static F32 clipRange(F32 value);

    LLToolset*  mLastToolset{ nullptr };
    LLTool*     mJointRotTool{ nullptr };
    
    LLVector3          mLastSliderRotation;

    FSVirtualTrackpad* mAvatarTrackball{ nullptr };

    LLSliderCtrl* mTrackpadSensitivitySlider{ nullptr };
    LLSliderCtrl* mPosXSlider{ nullptr };
    LLSliderCtrl* mPosYSlider{ nullptr };
    LLSliderCtrl* mPosZSlider{ nullptr };
    LLSliderCtrl* mAdvRotXSlider{ nullptr };
    LLSliderCtrl* mAdvRotYSlider{ nullptr };
    LLSliderCtrl* mAdvRotZSlider{ nullptr };
    LLSliderCtrl* mAdvPosXSlider{ nullptr };
    LLSliderCtrl* mAdvPosYSlider{ nullptr };
    LLSliderCtrl* mAdvPosZSlider{ nullptr };
    LLSliderCtrl* mAdvScaleXSlider{ nullptr };
    LLSliderCtrl* mAdvScaleYSlider{ nullptr };
    LLSliderCtrl* mAdvScaleZSlider{ nullptr };

    LLTabContainer* mJointsTabs{ nullptr };
    LLTabContainer* mHandsTabs{ nullptr };

    LLScrollListCtrl* mAvatarSelectionScrollList{ nullptr };
    LLScrollListCtrl* mBodyJointsScrollList{ nullptr };
    LLScrollListCtrl* mFaceJointsScrollList{ nullptr };
    LLScrollListCtrl* mHandJointsScrollList{ nullptr };
    LLScrollListCtrl* mMiscJointsScrollList{ nullptr };
    LLScrollListCtrl* mCollisionVolumesScrollList{ nullptr };
    LLScrollListCtrl* mEntireAvJointScroll{ nullptr };
    LLScrollListCtrl* mPosesScrollList{ nullptr };
    LLScrollListCtrl* mHandPresetsScrollList{ nullptr };

    LLButton* mToggleVisualManipulators{ nullptr };
    LLButton* mStartStopPosingBtn{ nullptr };
    LLButton* mToggleLoadSavePanelBtn{ nullptr };
    LLButton* mBrowserFolderBtn{ nullptr };
    LLButton* mLoadPosesBtn{ nullptr };
    LLButton* mSavePosesBtn{ nullptr };
    LLButton* mFlipPoseBtn{ nullptr };
    LLButton* mFlipJointBtn{ nullptr };
    LLButton* mRecaptureBtn{ nullptr };
    LLButton* mTogglePosingBonesBtn{ nullptr };
    LLButton* mToggleMirrorRotationBtn{ nullptr };
    LLButton* mToggleSympatheticRotationBtn{ nullptr };
    LLButton* mToggleDeltaModeBtn{ nullptr };
    LLButton* mRedoChangeBtn{ nullptr };
    LLButton* mUndoChangeBtn{ nullptr };
    LLButton* mSetToTposeButton{ nullptr };
    LLButton* mBtnJointRotate{ nullptr };

    LLLineEditor* mPoseSaveNameEditor{ nullptr };

    LLPanel* mJointsParentPnl{ nullptr };
    LLPanel* mTrackballPnl{ nullptr };
    LLPanel* mPositionRotationPnl{ nullptr };
    LLPanel* mBodyJointsPnl{ nullptr };
    LLPanel* mFaceJointsPnl{ nullptr };
    LLPanel* mHandsJointsPnl{ nullptr };
    LLPanel* mMiscJointsPnl{ nullptr };
    LLPanel* mCollisionVolumesPnl{ nullptr };
    LLPanel* mPosesLoadSavePnl{ nullptr };

    LLCheckBoxCtrl* mResetBaseRotCbx{ nullptr };
    LLCheckBoxCtrl* mAlsoSaveBvhCbx{ nullptr };

    LLUICtrl* mTrackpadSensitivitySpnr{ nullptr };
    LLUICtrl* mYawSpnr{ nullptr };
    LLUICtrl* mPitchSpnr{ nullptr };
    LLUICtrl* mRollSpnr{ nullptr };
    LLUICtrl* mUpDownSpnr{ nullptr };
    LLUICtrl* mLeftRightSpnr{ nullptr };
    LLUICtrl* mInOutSpnr{ nullptr };
    LLUICtrl* mAdvPosXSpnr{ nullptr };
    LLUICtrl* mAdvPosYSpnr{ nullptr };
    LLUICtrl* mAdvPosZSpnr{ nullptr };
    LLUICtrl* mScaleXSpnr{ nullptr };
    LLUICtrl* mScaleYSpnr{ nullptr };
    LLUICtrl* mScaleZSpnr{ nullptr };
};

#endif

/**
 * @file llpanelcontents.h
 * @brief Object contents panel in the tools floater.
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#ifndef LL_LLPANELCONTENTS_H
#define LL_LLPANELCONTENTS_H

#include "llfolderview.h"
#include "llinventory.h"
#include "llpanel.h"
#include "lluuid.h"
#include "llviewerobject.h"
#include "llvoinventorylistener.h"
#include "v3math.h"

class LLButton;
class LLPanelObjectInventory;
class LLViewerObject;
class LLCheckBoxCtrl;
class LLSpinCtrl;
// <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
class LLIconsComboBox;
class LLComboBox;
// </FS:minerjr> [FIRE-36685]zX

class LLPanelContents : public LLPanel
{
public:
    virtual bool postBuild();
    LLPanelContents();
    virtual ~LLPanelContents();

    void            refresh();
    void            clearContents();

    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    void onCommitSetItem(); // Set the new item that can be added to the objec's Content Inventory
    void onClickAddItemToObject(); // Adds the actual item to the object's Content Inventory.
    void onFinishCreateItem();
    // </FS:minerjr> [FIRE-36685]

    static void     onClickNewScript(void*);
    static void     onClickPermissions(void*);
    static void     onClickResetScripts(void*); // <FS> Script reset in edit floater
    static void     onClickRefresh(void*);

    // Key suffix for "tentative" fields
    static const char* TENTATIVE_SUFFIX;

    // These aren't fields in LLMediaEntry, so we have to define them ourselves for checkbox control
    static const char* PERMS_OWNER_INTERACT_KEY;
    static const char* PERMS_OWNER_CONTROL_KEY;
    static const char* PERMS_GROUP_INTERACT_KEY;
    static const char* PERMS_GROUP_CONTROL_KEY;
    static const char* PERMS_ANYONE_INTERACT_KEY;
    static const char* PERMS_ANYONE_CONTROL_KEY;

protected:
    void getState(LLViewerObject *object);
    void onFilterEdit();

    bool mDirtyFilter { false };

public:
    class LLFilterEditor* mFilterEditor;
    LLSaveFolderState mSavedFolderState;
    LLPanelObjectInventory* mPanelInventoryObject;
    // <FS:minerjr> [FIRE-36685] - Toolbox Window - Add dropdown of new items to Content tab
    LLComboBox* mCMBSetItem; // Used to keep track of the Set Item combo-box for selecting what new item to create.
    LLButton*   mBTNAddItemToObject; // Used to keep track of the button which will actually add the new item.
    std::string mCMBCommand; // Used to store what command is selected by the Set Item combo-box. Options are NewScript, DoCreate.
    std::string mCMBItemName; // The name of the item to be added.
    std::string mCMBLabel; // Stores the label assigned to the Set Item combo-box button. Needed for translations and selection changes.
    const static S32 CMB_SET_ITEM_TEXT_COL = 1; // Column index used for the Text of the Set Item combo-box.
    const static S32 CMB_SET_ITEM_ICON_COL = 0; // Column index used for the Icon of the Set Item combo-box.
    // </FS:minerjr> [FIRE-36685]
};

#endif // LL_LLPANELCONTENTS_H

/**
 * @file llpanelcontents.cpp
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

#include "llviewerprecompiledheaders.h"

// file include
#include "llpanelcontents.h"

// linden library includes
#include "lleconomy.h"
#include "llerror.h"
#include "llfloaterreg.h"
#include "llfontgl.h"
#include "llinventorydefines.h"
#include "llmaterialtable.h"
#include "llpermissionsflags.h"
#include "llrect.h"
#include "llstring.h"
#include "llui.h"
#include "m3math.h"
#include "material_codes.h"

// project includes
#include "llagent.h"
#include "llpanelobjectinventory.h"
#include "llpreviewscript.h"
#include "llresmgr.h"
#include "llselectmgr.h"
#include "lltool.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llviewerassettype.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]

//
// Imported globals
//


//
// Globals
//
const char* LLPanelContents::TENTATIVE_SUFFIX = "_tentative";
const char* LLPanelContents::PERMS_OWNER_INTERACT_KEY = "perms_owner_interact";
const char* LLPanelContents::PERMS_OWNER_CONTROL_KEY = "perms_owner_control";
const char* LLPanelContents::PERMS_GROUP_INTERACT_KEY = "perms_group_interact";
const char* LLPanelContents::PERMS_GROUP_CONTROL_KEY = "perms_group_control";
const char* LLPanelContents::PERMS_ANYONE_INTERACT_KEY = "perms_anyone_interact";
const char* LLPanelContents::PERMS_ANYONE_CONTROL_KEY = "perms_anyone_control";

BOOL LLPanelContents::postBuild()
{
	LLRect rect = this->getRect();

	setMouseOpaque(FALSE);

	childSetAction("button new script",&LLPanelContents::onClickNewScript, this);
	childSetAction("button permissions",&LLPanelContents::onClickPermissions, this);

	mPanelInventoryObject = getChild<LLPanelObjectInventory>("contents_inventory");

	return TRUE;
}

LLPanelContents::LLPanelContents()
	:	LLPanel(),
		mPanelInventoryObject(NULL)
{
}


LLPanelContents::~LLPanelContents()
{
	// Children all cleaned up by default view destructor.
}


void LLPanelContents::getState(LLViewerObject *objectp )
{
	if( !objectp )
	{
		getChildView("button new script")->setEnabled(FALSE);
		return;
	}

	LLUUID group_id;			// used for SL-23488
	LLSelectMgr::getInstance()->selectGetGroup(group_id);  // sets group_id as a side effect SL-23488

	// BUG? Check for all objects being editable?
	bool editable = gAgent.isGodlike()
					|| (objectp->permModify()
					       && ( objectp->permYouOwner() || ( !group_id.isNull() && gAgent.isInGroup(group_id) )));  // solves SL-23488
	BOOL all_volume = LLSelectMgr::getInstance()->selectionAllPCode( LL_PCODE_VOLUME );

// [RLVa:KB] - Checked: 2010-04-01 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
	if ( (rlv_handler_t::isEnabled()) && (editable) )
	{
		// Don't allow creation of new scripts if it's non-detachable
		if (objectp->isAttachment())
			editable = !gRlvAttachmentLocks.isLockedAttachment(objectp->getRootEdit());

		// Don't allow creation of new scripts if we're @unsit=n or @sittp=n restricted and we're sitting on the selection
		if ( (editable) && ((gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP))) )
		{
			// Only check the first (non-)root object because nothing else would result in enabling the button (see below)
			LLViewerObject* pObj = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(TRUE);

			editable = 
				(pObj) && (isAgentAvatarValid()) && ((!gAgentAvatarp->isSitting()) || (gAgentAvatarp->getRoot() != pObj->getRootEdit()));
		}
	}
// [/RLVa:KB]

	// Edit script button - ok if object is editable and there's an unambiguous destination for the object.
	getChildView("button new script")->setEnabled(
		editable &&
		all_volume &&
		((LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() == 1)
			|| (LLSelectMgr::getInstance()->getSelection()->getObjectCount() == 1)));

}

void LLPanelContents::refresh()
{
	const BOOL children_ok = TRUE;
	LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);

	getState(object);
	if (mPanelInventoryObject)
	{
		mPanelInventoryObject->refresh();
	}	
}



//
// Static functions
//

// static
void LLPanelContents::onClickNewScript(void *userdata)
{
	const BOOL children_ok = TRUE;
	LLViewerObject* object = LLSelectMgr::getInstance()->getSelection()->getFirstRootObject(children_ok);
	if(object)
	{
// [RLVa:KB] - Checked: 2010-03-31 (RLVa-1.2.0c) | Modified: RLVa-1.0.5a
		if (rlv_handler_t::isEnabled())	// Fallback code [see LLPanelContents::getState()]
		{
			if (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit()))
			{
				return;					// Disallow creating new scripts in a locked attachment
			}
			else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_SITTP)) )
			{
				if ( (isAgentAvatarValid()) && (gAgentAvatarp->isSitting()) && (gAgentAvatarp->getRoot() == object->getRootEdit()) )
					return;				// .. or in a linkset the avie is sitting on under @unsit=n/@sittp=n
			}
		}
// [/RLVa:KB]

		LLPermissions perm;
		perm.init(gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null);
		perm.initMasks(
			PERM_ALL,
			PERM_ALL,
			PERM_NONE,
			PERM_NONE,
			PERM_MOVE | PERM_TRANSFER);
		std::string desc;
		LLViewerAssetType::generateDescriptionFor(LLAssetType::AT_LSL_TEXT, desc);
		LLPointer<LLViewerInventoryItem> new_item =
			new LLViewerInventoryItem(
				LLUUID::null,
				LLUUID::null,
				perm,
				LLUUID::null,
				LLAssetType::AT_LSL_TEXT,
				LLInventoryType::IT_LSL,
				"New Script",
				desc,
				LLSaleInfo::DEFAULT,
				LLInventoryItemFlags::II_FLAGS_NONE,
				time_corrected());
		object->saveScript(new_item, TRUE, true);

		// *NOTE: In order to resolve SL-22177, we needed to create
		// the script first, and then you have to click it in
		// inventory to edit it.
		// *TODO: The script creation should round-trip back to the
		// viewer so the viewer can auto-open the script and start
		// editing ASAP.
#if 0
		LLFloaterReg::showInstance("preview_scriptedit", LLSD(inv_item->getUUID()), TAKE_FOCUS_YES);
#endif
	}
}


// static
void LLPanelContents::onClickPermissions(void *userdata)
{
	LLPanelContents* self = (LLPanelContents*)userdata;
	gFloaterView->getParentFloater(self)->addDependentFloater(LLFloaterReg::showInstance("bulk_perms"));
}

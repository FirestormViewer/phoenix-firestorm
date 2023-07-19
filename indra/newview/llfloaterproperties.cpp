/** 
 * @file llfloaterproperties.cpp
 * @brief A floater which shows an inventory item's properties.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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
#include "llfloaterproperties.h"

#include <algorithm>
#include <functional>
#include "llcachename.h"
#include "llavatarnamecache.h"
#include "lldbstrings.h"
#include "llfloaterreg.h"

#include "llagent.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llavataractions.h"
#include "llinventorydefines.h"
#include "llinventoryobserver.h"
#include "llinventorymodel.h"
#include "lllineeditor.h"
//#include "llspinctrl.h"
#include "llradiogroup.h"
#include "llresmgr.h"
#include "roles_constants.h"
#include "llselectmgr.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluiconstants.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "llgroupactions.h"

#include "lluictrlfactory.h"

// [RLVa:KB] - Checked: RLVa-2.0.1
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]
#include "llviewernetwork.h"	// <FS:CR> For OpenSim export perms
#include "llexperiencecache.h"
#include "llslurl.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPropertiesObserver
//
// helper class to watch the inventory. 
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// Ugh. This can't be a singleton because it needs to remove itself
//  from the inventory observer list when destroyed, which could
//  happen after gInventory has already been destroyed if a singleton.
// Instead, do our own ref counting and create / destroy it as needed
class LLPropertiesObserver : public LLInventoryObserver
{
public:
	LLPropertiesObserver(LLFloaterProperties* floater)
		: mFloater(floater)
	{
		gInventory.addObserver(this);
	}
	virtual ~LLPropertiesObserver()
	{
		gInventory.removeObserver(this);
	}
	virtual void changed(U32 mask);
private:
	LLFloaterProperties* mFloater; // Not a handle because LLFloaterProperties is managing LLPropertiesObserver
};

void LLPropertiesObserver::changed(U32 mask)
{
	// if there's a change we're interested in.
	if((mask & (LLInventoryObserver::LABEL | LLInventoryObserver::INTERNAL | LLInventoryObserver::REMOVE)) != 0)
	{
		mFloater->dirty();
	}
}



///----------------------------------------------------------------------------
/// Class LLFloaterProperties
///----------------------------------------------------------------------------

// Default constructor
LLFloaterProperties::LLFloaterProperties(const LLSD& key)
  : LLFloater(key),
	mDirty(TRUE)
	// <FS:Ansariel> Avatar names often not showing on first open
	,mCreatorNameCbConnection(),
	mOwnerNameCbConnection(),
	mGroupOwnerNameCbConnection()
	// </FS:Ansariel>
{
	if (key.has("item_id"))
	{
		mItemID = key["item_id"].asUUID();
		if (key.has("object_id"))
			mObjectID = key["object_id"].asUUID();
	}
	mPropertiesObserver = new LLPropertiesObserver(this);
}

// Destroys the object
LLFloaterProperties::~LLFloaterProperties()
{
	delete mPropertiesObserver;
	mPropertiesObserver = NULL;

	// <FS:Ansariel> Avatar names often not showing on first open
	if (mCreatorNameCbConnection.connected())
	{
		mCreatorNameCbConnection.disconnect();
	}
	if (mOwnerNameCbConnection.connected())
	{
		mOwnerNameCbConnection.disconnect();
	}
	if (mGroupOwnerNameCbConnection.connected())
	{
		mGroupOwnerNameCbConnection.disconnect();
	}
	// </FS:Ansariel>
}

// virtual
BOOL LLFloaterProperties::postBuild()
{
	// build the UI
	// item name & description
	getChild<LLLineEditor>("LabelItemName")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	getChild<LLUICtrl>("LabelItemName")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitName,this));
	getChild<LLLineEditor>("LabelItemDesc")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);
	getChild<LLUICtrl>("LabelItemDesc")->setCommitCallback(boost::bind(&LLFloaterProperties:: onCommitDescription, this));
	// Creator information
	getChild<LLUICtrl>("BtnCreator")->setCommitCallback(boost::bind(&LLFloaterProperties::onClickCreator,this));
	// owner information
	getChild<LLUICtrl>("BtnOwner")->setCommitCallback(boost::bind(&LLFloaterProperties::onClickOwner,this));
	// acquired date
	// owner permissions
	// Permissions debug text
	// group permissions
	getChild<LLUICtrl>("CheckShareWithGroup")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	// everyone permissions
	getChild<LLUICtrl>("CheckEveryoneCopy")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	// next owner permissions
	getChild<LLUICtrl>("CheckNextOwnerModify")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	// <FS:CR> OpenSim export permissions
	getChild<LLUICtrl>("CheckOwnerExport")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitPermissions, this));
	getChild<LLUICtrl>("CheckOwnerExport")->setVisible(!LLGridManager::getInstance()->isInSecondLife());
	// </FS:CR>
	// Mark for sale or not, and sale info
	getChild<LLUICtrl>("CheckPurchase")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleInfo, this));
	getChild<LLUICtrl>("ComboBoxSaleType")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleType, this));
	// "Price" label for edit
	getChild<LLUICtrl>("Edit Cost")->setCommitCallback(boost::bind(&LLFloaterProperties::onCommitSaleInfo, this));
	// The UI has been built, now fill in all the values
	refresh();

	return TRUE;
}

// virtual
void LLFloaterProperties::onOpen(const LLSD& key)
{
	refresh();
}

void LLFloaterProperties::refresh()
{
	LLInventoryItem* item = findItem();
	if(item)
	{
		refreshFromItem(item);
	}
	else
	{
		//RN: it is possible that the container object is in the middle of an inventory refresh
		// causing findItem() to fail, so just temporarily disable everything
		
		mDirty = TRUE;

		const char* enableNames[]={
			"LabelItemName",
			"LabelItemDesc",
			"LabelCreatorName",
			"BtnCreator",
			"LabelOwnerName",
			"BtnOwner",
			"CheckOwnerModify",
			"CheckOwnerCopy",
			"CheckOwnerTransfer",
			"CheckShareWithGroup",
			"CheckEveryoneCopy",
			"CheckNextOwnerModify",
			"CheckNextOwnerCopy",
			"CheckNextOwnerTransfer",
			"CheckOwnerExport",	// <FS:CR> OpenSim export permissions
			"CheckPurchase",
			"ComboBoxSaleType",
			"Edit Cost"
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(enableNames); ++t)
		{
			getChildView(enableNames[t])->setEnabled(false);
		}
		const char* hideNames[]={
			"BaseMaskDebug",
			"OwnerMaskDebug",
			"GroupMaskDebug",
			"EveryoneMaskDebug",
			"NextMaskDebug"
		};
		for(size_t t=0; t<LL_ARRAY_SIZE(hideNames); ++t)
		{
			getChildView(hideNames[t])->setVisible(false);
		}
	}
}

void LLFloaterProperties::draw()
{
	if (mDirty)
	{
		// RN: clear dirty first because refresh can set dirty to TRUE
		mDirty = FALSE;
		refresh();
	}

	LLFloater::draw();
}

void LLFloaterProperties::refreshFromItem(LLInventoryItem* item)
{
	////////////////////////
	// PERMISSIONS LOOKUP //
	////////////////////////

	// do not enable the UI for incomplete items.
	LLViewerInventoryItem* i = (LLViewerInventoryItem*)item;
	BOOL is_complete = i->isFinished();
	const BOOL cannot_restrict_permissions = LLInventoryType::cannotRestrictPermissions(i->getInventoryType());
	const BOOL is_calling_card = (i->getInventoryType() == LLInventoryType::IT_CALLINGCARD);
	const BOOL is_settings = (item->getInventoryType() == LLInventoryType::IT_SETTINGS);
	const LLPermissions& perm = item->getPermissions();
	const BOOL can_agent_manipulate = gAgent.allowOperation(PERM_OWNER, perm, 
															GP_OBJECT_MANIPULATE);
	const BOOL can_agent_sell = gAgent.allowOperation(PERM_OWNER, perm, 
													  GP_OBJECT_SET_SALE) &&
		!cannot_restrict_permissions;
	const BOOL is_link = i->getIsLinkType();

	// You need permission to modify the object to modify an inventory
	// item in it.
	LLViewerObject* object = NULL;
	if(!mObjectID.isNull()) object = gObjectList.findObject(mObjectID);
	BOOL is_obj_modify = TRUE;
	if(object)
	{
		is_obj_modify = object->permOwnerModify();
	}

	// <FS:Ansariel> Experience info
	if(item->getInventoryType() == LLInventoryType::IT_LSL)
	{
		getChildView("LabelItemExperienceTitle")->setVisible(TRUE);
		LLTextBox* tb = getChild<LLTextBox>("LabelItemExperience");
		tb->setText(getString("loading_experience"));
		tb->setVisible(TRUE);
		LLExperienceCache::instance().fetchAssociatedExperience(item->getParentUUID(), item->getUUID(), boost::bind(&LLFloaterProperties::setAssociatedExperience, getDerivedHandle<LLFloaterProperties>(), _1));
	}
	// </FS:Ansariel>

	//////////////////////
	// ITEM NAME & DESC //
	//////////////////////
	BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY, perm,
											   GP_OBJECT_MANIPULATE)
		&& is_obj_modify && is_complete;

	getChildView("LabelItemNameTitle")->setEnabled(TRUE);
	getChildView("LabelItemName")->setEnabled(is_modifiable && !is_calling_card); // for now, don't allow rename of calling cards
	getChild<LLUICtrl>("LabelItemName")->setValue(item->getName());
	getChildView("LabelItemDescTitle")->setEnabled(TRUE);
	getChildView("LabelItemDesc")->setEnabled(is_modifiable);
	getChildView("IconLocked")->setVisible(!is_modifiable);
	getChild<LLUICtrl>("LabelItemDesc")->setValue(item->getDescription());

	//////////////////
	// CREATOR NAME //
	//////////////////
	if(!gCacheName) return;
	if(!gAgent.getRegion()) return;

	if (item->getCreatorUUID().notNull())
	{
		// <FS:Ansariel> Avatar names often not showing on first open
		//LLAvatarName av_name;
		//LLAvatarNameCache::get(item->getCreatorUUID(), &av_name);
		//getChildView("BtnCreator")->setEnabled(TRUE); 
		// </FS:Ansariel>
		getChildView("LabelCreatorTitle")->setEnabled(TRUE);
		getChildView("LabelCreatorName")->setEnabled(TRUE);
		// <FS:Ansariel> Avatar names often not showing on first open
		//getChild<LLUICtrl>("LabelCreatorName")->setValue(av_name.getUserName());
		getChildView("BtnCreator")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(LLTrans::getString("AvatarNameWaiting"));
		mCreatorNameCbConnection = LLAvatarNameCache::get(item->getCreatorUUID(), boost::bind(&LLFloaterProperties::onCreatorNameCallback, this, _1, _2, perm));
		// </FS:Ansariel>
	}
	else
	{
		getChildView("BtnCreator")->setEnabled(FALSE);
		getChildView("LabelCreatorTitle")->setEnabled(FALSE);
		getChildView("LabelCreatorName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelCreatorName")->setValue(getString("unknown"));
	}

	////////////////
	// OWNER NAME //
	////////////////
	if(perm.isOwned())
	{
		// <FS:Ansariel> Avatar names often not showing on first open
		//std::string name;
		getChildView("BtnOwner")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(LLTrans::getString("AvatarNameWaiting"));
		// </FS:Ansariel>
		if (perm.isGroupOwned())
		{
			// <FS:Ansariel> Avatar names often not showing on first open
			//gCacheName->getGroupName(perm.getGroup(), name);
			mGroupOwnerNameCbConnection = gCacheName->getGroup(perm.getGroup(), boost::bind(&LLFloaterProperties::onGroupOwnerNameCallback, this, _2));
			// </FS:Ansariel>
		}
		else
		{
			// <FS:Ansariel> Avatar names often not showing on first open
			//LLAvatarName av_name;
			//LLAvatarNameCache::get(perm.getOwner(), &av_name);
			//name = av_name.getUserName();
			mOwnerNameCbConnection = LLAvatarNameCache::get(perm.getOwner(), boost::bind(&LLFloaterProperties::onOwnerNameCallback, this, _1, _2));
			// </FS:Ansariel>
		}
		//getChildView("BtnOwner")->setEnabled(TRUE); // <FS:Ansariel> Avatar names often not showing on first open
		getChildView("LabelOwnerTitle")->setEnabled(TRUE);
		getChildView("LabelOwnerName")->setEnabled(TRUE);
		//getChild<LLUICtrl>("LabelOwnerName")->setValue(name); // <FS:Ansariel> Avatar names often not showing on first open
	}
	else
	{
		getChildView("BtnOwner")->setEnabled(FALSE);
		getChildView("LabelOwnerTitle")->setEnabled(FALSE);
		getChildView("LabelOwnerName")->setEnabled(FALSE);
		getChild<LLUICtrl>("LabelOwnerName")->setValue(getString("public"));
	}
	
	//////////////////
	// ACQUIRE DATE //
	//////////////////
	
	time_t time_utc = item->getCreationDate();
	if (0 == time_utc)
	{
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(getString("unknown"));
	}
	else
	{
		std::string timeStr = getString("acquiredDate");
		LLSD substitution;
		substitution["datetime"] = (S32) time_utc;
		LLStringUtil::format (timeStr, substitution);
		getChild<LLUICtrl>("LabelAcquiredDate")->setValue(timeStr);
	}

	///////////////////////
	// OWNER PERMISSIONS //
	///////////////////////
	if(can_agent_manipulate)
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("you_can"));
	}
	else
	{
		getChild<LLUICtrl>("OwnerLabel")->setValue(getString("owner_can"));
	}

	U32 base_mask		= perm.getMaskBase();
	U32 owner_mask		= perm.getMaskOwner();
	U32 group_mask		= perm.getMaskGroup();
	U32 everyone_mask	= perm.getMaskEveryone();
	U32 next_owner_mask	= perm.getMaskNextOwner();

	getChildView("OwnerLabel")->setEnabled(TRUE);
	getChildView("CheckOwnerModify")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerModify")->setValue(LLSD((BOOL)(owner_mask & PERM_MODIFY)));
	getChildView("CheckOwnerCopy")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerCopy")->setValue(LLSD((BOOL)(owner_mask & PERM_COPY)));
	getChildView("CheckOwnerTransfer")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerTransfer")->setValue(LLSD((BOOL)(owner_mask & PERM_TRANSFER)));
	
	// <FS:CR> OpenSim export permissions
	getChildView("CheckOwnerExport")->setEnabled(FALSE);
	getChild<LLUICtrl>("CheckOwnerExport")->setValue(LLSD((BOOL)(owner_mask & PERM_EXPORT)));
	// </FS:CR>

	///////////////////////
	// DEBUG PERMISSIONS //
	///////////////////////

	if( gSavedSettings.getBOOL("DebugPermissions") )
	{
		BOOL slam_perm 			= FALSE;
		BOOL overwrite_group	= FALSE;
		BOOL overwrite_everyone	= FALSE;

		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = item->getFlags();
			slam_perm 			= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
			overwrite_everyone	= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
			overwrite_group		= flags & LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		
		std::string perm_string;
// <FS:Beq> remove misleading X for export when not in OpenSim		
		bool isOpenSim {false};
#ifdef OPENSIM
        if( LLGridManager::instance().isInOpenSim() )
		{
            isOpenSim = true;
        }
#endif
// </FS:Beq>
		perm_string = "B: ";
		perm_string += mask_to_string(base_mask, isOpenSim); // <FS:Beq/> remove misleading X for export when not in OpenSim
		getChild<LLUICtrl>("BaseMaskDebug")->setValue(perm_string);
		getChildView("BaseMaskDebug")->setVisible(TRUE);
		
		perm_string = "O: ";
		perm_string += mask_to_string(owner_mask, isOpenSim); // <FS:Beq/> remove misleading X for export when not in OpenSim
		getChild<LLUICtrl>("OwnerMaskDebug")->setValue(perm_string);
		getChildView("OwnerMaskDebug")->setVisible(TRUE);
		
		perm_string = "G";
		perm_string += overwrite_group ? "*: " : ": ";
		perm_string += mask_to_string(group_mask, isOpenSim); // <FS:Beq/> remove misleading X for export when not in OpenSim
		getChild<LLUICtrl>("GroupMaskDebug")->setValue(perm_string);
		getChildView("GroupMaskDebug")->setVisible(TRUE);
		
		perm_string = "E";
		perm_string += overwrite_everyone ? "*: " : ": ";
		perm_string += mask_to_string(everyone_mask, isOpenSim); // <FS:Beq/> remove misleading X for export when not in OpenSim
		getChild<LLUICtrl>("EveryoneMaskDebug")->setValue(perm_string);
		getChildView("EveryoneMaskDebug")->setVisible(TRUE);
		
		perm_string = "N";
		perm_string += slam_perm ? "*: " : ": ";
		perm_string += mask_to_string(next_owner_mask, isOpenSim); // <FS:Beq/> remove misleading X for export when not in OpenSim
		getChild<LLUICtrl>("NextMaskDebug")->setValue(perm_string);
		getChildView("NextMaskDebug")->setVisible(TRUE);
	}
	else
	{
		getChildView("BaseMaskDebug")->setVisible(FALSE);
		getChildView("OwnerMaskDebug")->setVisible(FALSE);
		getChildView("GroupMaskDebug")->setVisible(FALSE);
		getChildView("EveryoneMaskDebug")->setVisible(FALSE);
		getChildView("NextMaskDebug")->setVisible(FALSE);
	}

	/////////////
	// SHARING //
	/////////////

	// Check for ability to change values.
	if (is_link || cannot_restrict_permissions)
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}
	else if (is_obj_modify && can_agent_manipulate)
	{
		getChildView("CheckShareWithGroup")->setEnabled(TRUE);
		getChildView("CheckEveryoneCopy")->setEnabled((owner_mask & PERM_COPY) && (owner_mask & PERM_TRANSFER));
	}
	else
	{
		getChildView("CheckShareWithGroup")->setEnabled(FALSE);
		getChildView("CheckEveryoneCopy")->setEnabled(FALSE);
	}
	
	getChildView("CheckOwnerExport")->setEnabled(gAgentID == item->getCreatorUUID());	// <FS:CR> OpenSim export permissions

	// Set values.
	BOOL is_group_copy = (group_mask & PERM_COPY) ? TRUE : FALSE;
	BOOL is_group_modify = (group_mask & PERM_MODIFY) ? TRUE : FALSE;
	BOOL is_group_move = (group_mask & PERM_MOVE) ? TRUE : FALSE;

	if (is_group_copy && is_group_modify && is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD((BOOL)TRUE));

		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else if (!is_group_copy && !is_group_modify && !is_group_move)
	{
		getChild<LLUICtrl>("CheckShareWithGroup")->setValue(LLSD((BOOL)FALSE));
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(FALSE);
		}
	}
	else
	{
		LLCheckBoxCtrl* ctl = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");
		if(ctl)
		{
			ctl->setTentative(TRUE);
			ctl->set(TRUE);
		}
	}
	
	getChild<LLUICtrl>("CheckEveryoneCopy")->setValue(LLSD((BOOL)(everyone_mask & PERM_COPY)));

	///////////////
	// SALE INFO //
	///////////////

	const LLSaleInfo& sale_info = item->getSaleInfo();
	BOOL is_for_sale = sale_info.isForSale();
	LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
	LLUICtrl* edit_cost = getChild<LLUICtrl>("Edit Cost");

	// Check for ability to change values.
	if (is_obj_modify && can_agent_sell 
		&& gAgent.allowOperation(PERM_TRANSFER, perm, GP_OBJECT_MANIPULATE))
	{
		getChildView("CheckPurchase")->setEnabled(is_complete);

		getChildView("NextOwnerLabel")->setEnabled(TRUE);
		getChildView("CheckNextOwnerModify")->setEnabled((base_mask & PERM_MODIFY) && !cannot_restrict_permissions);
		getChildView("CheckNextOwnerCopy")->setEnabled((base_mask & PERM_COPY) && !cannot_restrict_permissions && !is_settings);
		getChildView("CheckNextOwnerTransfer")->setEnabled((next_owner_mask & PERM_COPY) && !cannot_restrict_permissions);

		combo_sale_type->setEnabled(is_complete && is_for_sale);
		edit_cost->setEnabled(is_complete && is_for_sale);
	}
	else
	{
		getChildView("CheckPurchase")->setEnabled(FALSE);

		getChildView("NextOwnerLabel")->setEnabled(FALSE);
		getChildView("CheckNextOwnerModify")->setEnabled(FALSE);
		getChildView("CheckNextOwnerCopy")->setEnabled(FALSE);
		getChildView("CheckNextOwnerTransfer")->setEnabled(FALSE);

		combo_sale_type->setEnabled(FALSE);
		edit_cost->setEnabled(FALSE);
	}

	// Hide any properties that are not relevant to settings
	if (is_settings)
	{
		getChild<LLUICtrl>("GroupLabel")->setEnabled(false);
		getChild<LLUICtrl>("GroupLabel")->setVisible(false);
		getChild<LLUICtrl>("CheckShareWithGroup")->setEnabled(false);
		getChild<LLUICtrl>("CheckShareWithGroup")->setVisible(false);
		getChild<LLUICtrl>("AnyoneLabel")->setEnabled(false);
		getChild<LLUICtrl>("AnyoneLabel")->setVisible(false);
		getChild<LLUICtrl>("CheckEveryoneCopy")->setEnabled(false);
		getChild<LLUICtrl>("CheckEveryoneCopy")->setVisible(false);
		getChild<LLUICtrl>("CheckPurchase")->setEnabled(false);
		getChild<LLUICtrl>("CheckPurchase")->setVisible(false);
		getChild<LLUICtrl>("ComboBoxSaleType")->setEnabled(false);
		getChild<LLUICtrl>("ComboBoxSaleType")->setVisible(false);
		getChild<LLUICtrl>("Edit Cost")->setEnabled(false);
		getChild<LLUICtrl>("Edit Cost")->setVisible(false);
		getChild<LLUICtrl>("CurrencySymbol")->setEnabled(false);
		getChild<LLUICtrl>("CurrencySymbol")->setVisible(false);
	}

	// Set values.
	getChild<LLUICtrl>("CheckPurchase")->setValue(is_for_sale);
	getChild<LLUICtrl>("CheckNextOwnerModify")->setValue(LLSD(BOOL(next_owner_mask & PERM_MODIFY)));
	getChild<LLUICtrl>("CheckNextOwnerCopy")->setValue(LLSD(BOOL(next_owner_mask & PERM_COPY)));
	getChild<LLUICtrl>("CheckNextOwnerTransfer")->setValue(LLSD(BOOL(next_owner_mask & PERM_TRANSFER)));

	if (is_for_sale)
	{
		S32 numerical_price;
		numerical_price = sale_info.getSalePrice();
		edit_cost->setValue(llformat("%d",numerical_price));
		combo_sale_type->setValue(sale_info.getSaleType());
	}
	else
	{
		edit_cost->setValue(llformat("%d",0));
		combo_sale_type->setValue(LLSaleInfo::FS_COPY);
	}
}

// <FS:Ansariel> Experience info
void LLFloaterProperties::setAssociatedExperience( LLHandle<LLFloaterProperties> hInfo, const LLSD& experience )
{
	LLFloaterProperties* info = hInfo.get();
	if(info)
	{
		LLUUID id;
		if(experience.has(LLExperienceCache::EXPERIENCE_ID))
		{
			id=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
		}
		if(id.notNull())
		{
			info->getChild<LLTextBox>("LabelItemExperience")->setText(LLSLURL("experience", id, "profile").getSLURLString());
		}
		else
		{
			info->getChild<LLTextBox>("LabelItemExperience")->setText(LLTrans::getString("ExperienceNameNull"));
		}
	}
}
// </FS:Ansariel>

void LLFloaterProperties::onClickCreator()
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	if(!item->getCreatorUUID().isNull())
	{
// [RLVa:KB] - Checked: RLVa-1.2.1
		const LLUUID& idCreator = item->getCreatorUUID();
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, idCreator)) )
		{
			const LLPermissions& perm = item->getPermissions();
			if ( ((perm.isOwned()) && (!perm.isGroupOwned()) && (perm.getOwner() == idCreator) ) || (RlvUtil::isNearbyAgent(idCreator)) )
			{
				return;
			}
		}
// [/RLVa:KB]
		LLAvatarActions::showProfile(item->getCreatorUUID());
	}
}

// static
void LLFloaterProperties::onClickOwner()
{
	LLInventoryItem* item = findItem();
	if(!item) return;
	if(item->getPermissions().isGroupOwned())
	{
		LLGroupActions::show(item->getPermissions().getGroup());
	}
	else
	{
// [RLVa:KB] - Checked: RLVa-1.0.0
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, item->getPermissions().getOwner())) )
			return;
// [/RLVa:KB]
		LLAvatarActions::showProfile(item->getPermissions().getOwner());
	}
}

// static
void LLFloaterProperties::onCommitName()
{
	//LL_INFOS() << "LLFloaterProperties::onCommitName()" << LL_ENDL;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item)
	{
		return;
	}
	LLLineEditor* labelItemName = getChild<LLLineEditor>("LabelItemName");

	if(labelItemName&&
	   (item->getName() != labelItemName->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)) )
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->rename(labelItemName->getText());
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
}

void LLFloaterProperties::onCommitDescription()
{
	//LL_INFOS() << "LLFloaterProperties::onCommitDescription()" << LL_ENDL;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;

	LLLineEditor* labelItemDesc = getChild<LLLineEditor>("LabelItemDesc");
	if(!labelItemDesc)
	{
		return;
	}
	if((item->getDescription() != labelItemDesc->getText()) && 
	   (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE)))
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		new_item->setDescription(labelItemDesc->getText());
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
}

// static
void LLFloaterProperties::onCommitPermissions()
{
	//LL_INFOS() << "LLFloaterProperties::onCommitPermissions()" << LL_ENDL;
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;
	LLPermissions perm(item->getPermissions());


	LLCheckBoxCtrl* CheckShareWithGroup = getChild<LLCheckBoxCtrl>("CheckShareWithGroup");

	if(CheckShareWithGroup)
	{
		perm.setGroupBits(gAgent.getID(), gAgent.getGroupID(),
						CheckShareWithGroup->get(),
						PERM_MODIFY | PERM_MOVE | PERM_COPY);
	}
	LLCheckBoxCtrl* CheckEveryoneCopy = getChild<LLCheckBoxCtrl>("CheckEveryoneCopy");
	if(CheckEveryoneCopy)
	{
		perm.setEveryoneBits(gAgent.getID(), gAgent.getGroupID(),
						 CheckEveryoneCopy->get(), PERM_COPY);
	}

	LLCheckBoxCtrl* CheckNextOwnerModify = getChild<LLCheckBoxCtrl>("CheckNextOwnerModify");
	if(CheckNextOwnerModify)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerModify->get(), PERM_MODIFY);
	}
	LLCheckBoxCtrl* CheckNextOwnerCopy = getChild<LLCheckBoxCtrl>("CheckNextOwnerCopy");
	if(CheckNextOwnerCopy)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerCopy->get(), PERM_COPY);
	}
	LLCheckBoxCtrl* CheckNextOwnerTransfer = getChild<LLCheckBoxCtrl>("CheckNextOwnerTransfer");
	if(CheckNextOwnerTransfer)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							CheckNextOwnerTransfer->get(), PERM_TRANSFER);
	}
	// <FS:CR> OpenSim export permissions
	LLCheckBoxCtrl* CheckOwnerExport = getChild<LLCheckBoxCtrl>("CheckOwnerExport");
	if (CheckOwnerExport)
	{
		perm.setNextOwnerBits(gAgent.getID(), gAgent.getGroupID(),
							  CheckOwnerExport->get(), PERM_EXPORT);
	}
	
	
	if(perm != item->getPermissions()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
		new_item->setPermissions(perm);
		U32 flags = new_item->getFlags();
		// If next owner permissions have changed (and this is an object)
		// then set the slam permissions flag so that they are applied on rez.
		if((perm.getMaskNextOwner()!=item->getPermissions().getMaskNextOwner())
		   && (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_PERM;
		}
		// If everyone permissions have changed (and this is an object)
		// then set the overwrite everyone permissions flag so they
		// are applied on rez.
		if ((perm.getMaskEveryone()!=item->getPermissions().getMaskEveryone())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_EVERYONE;
		}
		// If group permissions have changed (and this is an object)
		// then set the overwrite group permissions flag so they
		// are applied on rez.
		if ((perm.getMaskGroup()!=item->getPermissions().getMaskGroup())
			&& (item->getType() == LLAssetType::AT_OBJECT))
		{
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_PERM_OVERWRITE_GROUP;
		}
		new_item->setFlags(flags);
		if(mObjectID.isNull())
		{
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

// static
void LLFloaterProperties::onCommitSaleInfo()
{
	//LL_INFOS() << "LLFloaterProperties::onCommitSaleInfo()" << LL_ENDL;
	updateSaleInfo();
}

// static
void LLFloaterProperties::onCommitSaleType()
{
	//LL_INFOS() << "LLFloaterProperties::onCommitSaleType()" << LL_ENDL;
	updateSaleInfo();
}

void LLFloaterProperties::updateSaleInfo()
{
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)findItem();
	if(!item) return;
	LLSaleInfo sale_info(item->getSaleInfo());
	if(!gAgent.allowOperation(PERM_TRANSFER, item->getPermissions(), GP_OBJECT_SET_SALE))
	{
		getChild<LLUICtrl>("CheckPurchase")->setValue(LLSD((BOOL)FALSE));
	}

	if((BOOL)getChild<LLUICtrl>("CheckPurchase")->getValue())
	{
		// turn on sale info
		LLSaleInfo::EForSale sale_type = LLSaleInfo::FS_COPY;

		LLComboBox* combo_sale_type = getChild<LLComboBox>("ComboBoxSaleType");
		if (combo_sale_type)
		{
			sale_type = static_cast<LLSaleInfo::EForSale>(combo_sale_type->getValue().asInteger());
		}

		if (sale_type == LLSaleInfo::FS_COPY 
			&& !gAgent.allowOperation(PERM_COPY, item->getPermissions(), 
									  GP_OBJECT_SET_SALE))
		{
			sale_type = LLSaleInfo::FS_ORIGINAL;
		}

	     
		
		S32 price = -1;
		price =  getChild<LLUICtrl>("Edit Cost")->getValue().asInteger();;

		// Invalid data - turn off the sale
		if (price < 0)
		{
			sale_type = LLSaleInfo::FS_NOT;
			price = 0;
		}

		sale_info.setSaleType(sale_type);
		sale_info.setSalePrice(price);
	}
	else
	{
		sale_info.setSaleType(LLSaleInfo::FS_NOT);
	}
	if(sale_info != item->getSaleInfo()
		&& item->isFinished())
	{
		LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);

		// Force an update on the sale price at rez
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			U32 flags = new_item->getFlags();
			flags |= LLInventoryItemFlags::II_FLAGS_OBJECT_SLAM_SALE;
			new_item->setFlags(flags);
		}

		new_item->setSaleInfo(sale_info);
		if(mObjectID.isNull())
		{
			// This is in the agent's inventory.
			new_item->updateServer(FALSE);
			gInventory.updateItem(new_item);
			gInventory.notifyObservers();
		}
		else
		{
			// This is in an object's contents.
			LLViewerObject* object = gObjectList.findObject(mObjectID);
			if(object)
			{
				object->updateInventory(
					new_item,
					TASK_INVENTORY_ITEM_KEY,
					false);
			}
		}
	}
	else
	{
		// need to make sure we don't just follow the click
		refresh();
	}
}

LLInventoryItem* LLFloaterProperties::findItem() const
{
	LLInventoryItem* item = NULL;
	if(mObjectID.isNull())
	{
		// it is in agent inventory
		item = gInventory.getItem(mItemID);
	}
	else
	{
		LLViewerObject* object = gObjectList.findObject(mObjectID);
		if(object)
		{
			item = (LLInventoryItem*)object->getInventoryObject(mItemID);
		}
	}
	return item;
}

//static
void LLFloaterProperties::dirtyAll()
{
	LLFloaterReg::const_instance_list_t& inst_list = LLFloaterReg::getFloaterList("properties");
	for (LLFloaterReg::const_instance_list_t::const_iterator iter = inst_list.begin();
		 iter != inst_list.end(); ++iter)
	{
		LLFloaterProperties* floater = dynamic_cast<LLFloaterProperties*>(*iter);
		llassert(floater); // else cast failed - wrong type D:
		if (floater)
		{
			floater->dirty();
		}
	}
}

// <FS:Ansariel> Avatar names often not showing on first open
void LLFloaterProperties::onCreatorNameCallback(const LLUUID& av_id, const LLAvatarName& av_name, const LLPermissions& perm)
{
	bool enabled = true;
	std::string name = av_name.getUserName();
	// If the object creator matches the object owner we need to anonymize the creator field as well
	if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, av_id)) &&
		    ( ((perm.isOwned()) && (!perm.isGroupOwned()) && (perm.getOwner() == av_id) ) || (RlvUtil::isNearbyAgent(av_id)) ) )
	{
		enabled = false;
		name = RlvStrings::getAnonym(av_name);
	}
	getChild<LLUICtrl>("LabelCreatorName")->setValue(name);
	childSetEnabled("BtnCreator", enabled);
}

void LLFloaterProperties::onOwnerNameCallback(const LLUUID& av_id, const LLAvatarName& av_name)
{
	bool fRlvCanShowOwner = true;
	std::string name = av_name.getUserName();
	if (RlvActions::isRlvEnabled())
	{
		fRlvCanShowOwner = RlvActions::canShowName(RlvActions::SNC_DEFAULT, av_id);
		if (!fRlvCanShowOwner)
		{
			name = RlvStrings::getAnonym(av_name);
		}
	}

	getChild<LLUICtrl>("LabelOwnerName")->setValue(name);
	getChildView("BtnOwner")->setEnabled(fRlvCanShowOwner);
}

void LLFloaterProperties::onGroupOwnerNameCallback(const std::string& name)
{
	getChild<LLUICtrl>("LabelOwnerName")->setValue(name);
	getChildView("BtnOwner")->setEnabled(TRUE);
}
// </FS:Ansariel>

///----------------------------------------------------------------------------
/// LLMultiProperties
///----------------------------------------------------------------------------

LLMultiProperties::LLMultiProperties()
	: LLMultiFloater(LLSD())
{
	// start with a small rect in the top-left corner ; will get resized
	LLRect rect;
	rect.setLeftTopAndSize(0, gViewerWindow->getWindowHeightScaled(), 350, 350);
	setRect(rect);
	LLFloater* last_floater = LLFloaterReg::getLastFloaterInGroup("properties");
	if (last_floater)
	{
		stackWith(*last_floater);
	}
	setTitle(LLTrans::getString("MultiPropertiesTitle"));
	buildTabContainer();
	// <FS:Ansariel> Center multifloater on screen
	center();
}

///----------------------------------------------------------------------------
/// Local function definitions
///----------------------------------------------------------------------------

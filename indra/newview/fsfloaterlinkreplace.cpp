/**
 * @file fsfloaterlinkreplace.cpp
 * @brief Allows replacing link targets in inventory links
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Copyright (c) 2014 Ansariel Hiller
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "fsfloaterlinkreplace.h"

#include "fsdroptarget.h"
#include "llagent.h"
#include "llappearancemgr.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llviewercontrol.h"

FSFloaterLinkReplace::FSFloaterLinkReplace(const LLSD& key)
	: LLFloater(key),
	LLEventTimer(gSavedSettings.getF32("FSLinkReplaceBatchPauseTime")),
	mRemainingItems(0),
	mSourceUUID(LLUUID::null),
	mTargetUUID(LLUUID::null),
	mInstance(NULL),
	mBatchSize(gSavedSettings.getU32("FSLinkReplaceBatchSize"))
{
	mEventTimer.stop();
	mInstance = this;
}

FSFloaterLinkReplace::~FSFloaterLinkReplace()
{
	mInstance = NULL;
}

BOOL FSFloaterLinkReplace::postBuild()
{
	mStartBtn = getChild<LLButton>("btn_start");
	mStartBtn->setCommitCallback(boost::bind(&FSFloaterLinkReplace::onStartClicked, this));

	mRefreshBtn = getChild<LLButton>("btn_refresh");
	mRefreshBtn->setCommitCallback(boost::bind(&FSFloaterLinkReplace::checkEnableStart, this));

	mSourceEditor = getChild<FSInventoryLinkReplaceDropTarget>("source_uuid_editor");
	mTargetEditor = getChild<FSInventoryLinkReplaceDropTarget>("target_uuid_editor");

	mSourceEditor->setDADCallback(boost::bind(&FSFloaterLinkReplace::onSourceItemDrop, this, _1));
	mTargetEditor->setDADCallback(boost::bind(&FSFloaterLinkReplace::onTargetItemDrop, this, _1));

	mStatusText = getChild<LLTextBox>("status_text");

	return TRUE;
}

void FSFloaterLinkReplace::onOpen(const LLSD& key)
{
	if (key.asUUID().notNull())
	{
		LLUUID item_id = key.asUUID();
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		mSourceEditor->setItem(item);
		onSourceItemDrop(item->getLinkedUUID());
	}
	else
	{
		checkEnableStart();
	}
}

void FSFloaterLinkReplace::onSourceItemDrop(const LLUUID& source_item_id)
{
	mSourceUUID = source_item_id;
	checkEnableStart();
}

void FSFloaterLinkReplace::onTargetItemDrop(const LLUUID& target_item_id)
{
	mTargetUUID = target_item_id;
	checkEnableStart();
}

void FSFloaterLinkReplace::updateFoundLinks()
{
	LLInventoryModel::item_array_t items = gInventory.collectLinkedItems(mSourceUUID);
	mRemainingItems = (U32)items.size();

	LLStringUtil::format_map_t args;
	args["NUM"] = llformat("%d", mRemainingItems);
	mStatusText->setText(getString("ItemsFound", args));
}

void FSFloaterLinkReplace::checkEnableStart()
{
	if (mSourceUUID.notNull() && mTargetUUID.notNull() && mSourceUUID == mTargetUUID)
	{
		mStatusText->setText(getString("ItemsIdentical"));
	}
	else if (mSourceUUID.notNull())
	{
		updateFoundLinks();
	}

	mStartBtn->setEnabled(mRemainingItems > 0 && mSourceUUID.notNull() && mTargetUUID.notNull() && mSourceUUID != mTargetUUID);
}

void FSFloaterLinkReplace::onStartClicked()
{
	LL_INFOS() << "Starting inventory link replace" << LL_ENDL;

	if (mSourceUUID.isNull() || mTargetUUID.isNull())
	{
		LL_WARNS() << "Cannot replace. Either source or target UUID is null." << LL_ENDL;
		return;
	}

	if (mSourceUUID == mTargetUUID)
	{
		LL_WARNS() << "Cannot replace. Source and target are identical." << LL_ENDL;
		return;
	}

	mRemainingInventoryItems = gInventory.collectLinkedItems(mSourceUUID);
	LL_INFOS() << "Found " << mRemainingInventoryItems.size() << " inventory links that need to be replaced." << LL_ENDL;

	if (mRemainingInventoryItems.size() > 0)
	{
		LLViewerInventoryItem* target_item = gInventory.getItem(mTargetUUID);
		if (target_item)
		{
			mRemainingItems = (U32)mRemainingInventoryItems.size();

			LLStringUtil::format_map_t args;
			args["NUM"] = llformat("%d", mRemainingItems);
			mStatusText->setText(getString("ItemsRemaining", args));

			mStartBtn->setEnabled(FALSE);
			mRefreshBtn->setEnabled(FALSE);

			mEventTimer.start();
			tick();
		}
		else
		{
			mStatusText->setText(getString("TargetNotFound"));
			LL_WARNS() << "Link replace target not found." << LL_ENDL;
		}
	}

}

void FSFloaterLinkReplace::linkCreatedCallback(const LLUUID& old_item_id,
												const LLUUID& target_item_id,
												bool needs_wearable_ordering_update,
												bool needs_description_update,
												const LLUUID& outfit_folder_id)
{
	LL_DEBUGS() << "Inventory link replace:" << LL_NEWLINE
		<< " - old_item_id = " << old_item_id.asString() << LL_NEWLINE
		<< " - target_item_id = " << target_item_id.asString() << LL_NEWLINE
		<< " - order update = " << (needs_wearable_ordering_update ? "true" : "false") << LL_NEWLINE
		<< " - description update = " << (needs_description_update ? "true" : "false") << LL_NEWLINE
		<< " - outfit_folder_id = " << outfit_folder_id.asString() << LL_ENDL;

	// If we are replacing an object, bodypart or gesture link within an outfit folder,
	// we need to change the actual description of the link itself. LLAppearanceMgr *should*
	// have created COF links that will be used to save the outfit with an empty description.
	// Since link_inventory_array() will set the description of the linked item for the link
	// itself, this will lead to a dirty outfit state when the outfit with the replaced
	// link is worn. So we have to correct this.
	if (needs_description_update && outfit_folder_id.notNull())
	{
		LLInventoryModel::item_array_t items;
		LLInventoryModel::cat_array_t cats;
		LLLinkedItemIDMatches is_target_link(target_item_id);
		gInventory.collectDescendentsIf(outfit_folder_id,
										cats,
										items,
										LLInventoryModel::EXCLUDE_TRASH,
										is_target_link);

		for (LLInventoryModel::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
		{
			LLPointer<LLViewerInventoryItem> item = *it;

			if ((item->getType() == LLAssetType::AT_BODYPART ||
				item->getType() == LLAssetType::AT_OBJECT ||
				item->getType() == LLAssetType::AT_GESTURE)
				&& !item->getActualDescription().empty())
			{
				LL_DEBUGS() << "Updating description for " << item->getName() << LL_ENDL;

				LLSD updates;
				updates["desc"] = "";
				update_inventory_item(item->getUUID(), updates, LLPointer<LLInventoryCallback>(NULL));
			}
		}
	}

	LLUUID outfit_update_folder = LLUUID::null;
	if (needs_wearable_ordering_update && outfit_folder_id.notNull())
	{
		// If a wearable item was involved in the link replace operation and replaced
		// a link in an outfit folder, we need to update the clothing ordering information
		// *after* the original link has been removed. LLAppearanceMgr abuses the actual link
		// description to store the clothing ordering information it. We will have to update
		// the clothing ordering information or the outfit will be in dirty state when worn.
		outfit_update_folder = outfit_folder_id;
	}

	LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(&FSFloaterLinkReplace::itemRemovedCallback, this, outfit_update_folder));
	remove_inventory_object(old_item_id, cb);
}

void FSFloaterLinkReplace::itemRemovedCallback(const LLUUID& outfit_folder_id)
{
	if (outfit_folder_id.notNull())
	{
		LLAppearanceMgr::getInstance()->updateClothingOrderingInfo(outfit_folder_id);
	}

	if (mInstance)
	{
		decreaseOpenItemCount();
	}
}

void FSFloaterLinkReplace::decreaseOpenItemCount()
{
	mRemainingItems--;

	if (mRemainingItems == 0)
	{
		mStatusText->setText(getString("ReplaceFinished"));
		mStartBtn->setEnabled(TRUE);
		mRefreshBtn->setEnabled(TRUE);
		mEventTimer.stop();
		LL_INFOS() << "Inventory link replace finished." << LL_ENDL;
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["NUM"] = llformat("%d", mRemainingItems);
		mStatusText->setText(getString("ItemsRemaining", args));
		LL_DEBUGS() << "Inventory link replace: " << mRemainingItems << " links remaining..." << LL_ENDL;
	}
}

BOOL FSFloaterLinkReplace::tick()
{
	LL_DEBUGS() << "Calling tick - remaining items = " << mRemainingInventoryItems.size() << LL_ENDL;

	LLInventoryModel::item_array_t current_batch;

	for (U32 i = 0; i < mBatchSize; ++i)
	{
		if (!mRemainingInventoryItems.size())
		{
			mEventTimer.stop();
			break;
		}

		current_batch.push_back(mRemainingInventoryItems.back());
		mRemainingInventoryItems.pop_back();
	}
	processBatch(current_batch);

	return FALSE;
}

void FSFloaterLinkReplace::processBatch(LLInventoryModel::item_array_t items)
{
	const LLViewerInventoryItem* target_item = gInventory.getItem(mTargetUUID);
	const LLUUID cof_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT, false);
	const LLUUID outfit_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_MY_OUTFITS, false);

	for (LLInventoryModel::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		LLPointer<LLInventoryItem> source_item = *it;

		if (source_item->getParentUUID() != cof_folder_id)
		{
			bool is_outfit_folder = gInventory.isObjectDescendentOf(source_item->getParentUUID(), outfit_folder_id);
			// If either the new or old item in the COF is a wearable, we need to update wearable ordering after the link has been replaced
			bool needs_wearable_ordering_update = is_outfit_folder && source_item->getType() == LLAssetType::AT_CLOTHING || target_item->getType() == LLAssetType::AT_CLOTHING;
			// Other items in the COF need a description update (description of the actual link item must be empty)
			bool needs_description_update = is_outfit_folder && target_item->getType() != LLAssetType::AT_CLOTHING;

			LL_DEBUGS() << "is_outfit_folder = " << (is_outfit_folder ? "true" : "false") << LL_NEWLINE
				<< "needs_wearable_ordering_update = " << (needs_wearable_ordering_update ? "true" : "false") << LL_NEWLINE
				<< "needs_description_update = " << (needs_description_update ? "true" : "false") << LL_ENDL;

			LLInventoryObject::const_object_list_t obj_array;
			obj_array.push_back(LLConstPointer<LLInventoryObject>(target_item));
			LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback(boost::bind(&FSFloaterLinkReplace::linkCreatedCallback,
																											this,
																											source_item->getUUID(),
																											target_item->getUUID(),
																											needs_wearable_ordering_update,
																											needs_description_update,
																											(is_outfit_folder ? source_item->getParentUUID() : LLUUID::null) ));
			link_inventory_array(source_item->getParentUUID(), obj_array, cb);
		}
		else
		{
			decreaseOpenItemCount();
		}
	}
}


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
#include "llinventoryfunctions.h"
#include "lllineeditor.h"
#include "lltextbox.h"
#include "llviewerinventory.h"

FSFloaterLinkReplace::FSFloaterLinkReplace(const LLSD& key)
	: LLFloater(key),
	mRemainingItems(0),
	mSourceUUID(LLUUID::null),
	mTargetUUID(LLUUID::null)
{
}

FSFloaterLinkReplace::~FSFloaterLinkReplace()
{
}

BOOL FSFloaterLinkReplace::postBuild()
{
	mStartBtn = getChild<LLButton>("btn_start");
	mStartBtn->setCommitCallback(boost::bind(&FSFloaterLinkReplace::onStartClicked, this));

	getChild<LLButton>("btn_refresh")->setCommitCallback(boost::bind(&FSFloaterLinkReplace::checkEnableStart, this));

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
	LLInventoryModel::item_array_t items = gInventory.collectLinkedItems(mSourceUUID, gInventory.getRootFolderID());
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

	LLInventoryModel::item_array_t items = gInventory.collectLinkedItems(mSourceUUID, gInventory.getRootFolderID());
	LL_INFOS() << "Found " << items.size() << " inventory links that need to be replaced." << LL_ENDL;

	if (items.size() > 0)
	{
		LLViewerInventoryItem* target_item = gInventory.getItem(mTargetUUID);
		if (target_item)
		{
			mRemainingItems = (U32)items.size();

			LLStringUtil::format_map_t args;
			args["NUM"] = llformat("%d", mRemainingItems);
			mStatusText->setText(getString("ItemsRemaining", args));

			const LLUUID cof_folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT, false);
			for (LLInventoryModel::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
			{
				if ((*it)->getParentUUID() != cof_folder_id)
				{
					link_inventory_item(gAgentID,
										target_item->getUUID(),
										(*it)->getParentUUID(),
										target_item->getName(),
										target_item->getDescription(),
										LLAssetType::AT_LINK,
										new LLBoostFuncInventoryCallback(boost::bind(&FSFloaterLinkReplace::linkCreatedCallback, this, (*it)->getUUID())));
				}
				else
				{
					decreaseOpenItemCount();
				}
			}
		}
		else
		{
			mStatusText->setText(getString("TargetNotFound"));
			LL_WARNS() << "Link replace target not found." << LL_ENDL;
		}
	}
}

void FSFloaterLinkReplace::linkCreatedCallback(const LLUUID& old_item_id)
{
	LL_DEBUGS() << "Inventory link replace: old_item_id = " << old_item_id.asString() << LL_ENDL;
	gInventory.purgeObject(old_item_id);
	decreaseOpenItemCount();
}

void FSFloaterLinkReplace::decreaseOpenItemCount()
{
	mRemainingItems--;

	if (mRemainingItems == 0)
	{
		mStatusText->setText(getString("ReplaceFinished"));
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


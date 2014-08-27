/**
 * @file fsdroptarget.cpp
 * @brief Various drop targets for different purposes
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Ansariel Hiller @ Second Life
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

#include "fsdroptarget.h"

#include "llinventoryfunctions.h"
#include "lltooldraganddrop.h"
#include "llviewerinventory.h"

static LLDefaultChildRegistry::Register<FSCopyTransInventoryDropTarget> r1("fs_copytrans_inventory_drop_target");
static LLDefaultChildRegistry::Register<FSInventoryLinkReplaceDropTarget> r2("fs_inventory_link_replace_drop_target");
static LLDefaultChildRegistry::Register<FSDropTarget> r3("profile_drop_target");
static LLDefaultChildRegistry::Register<FSEmbeddedItemDropTarget> r4("fs_embedded_item_drop_target");


BOOL FSCopyTransInventoryDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
													   EDragAndDropType cargo_type,
													   void* cargo_data,
													   EAcceptance* accept,
													   std::string& tooltip_msg)
{
	LLInventoryItem* item = (LLInventoryItem*)cargo_data;

	if (cargo_type >= DAD_TEXTURE && cargo_type <= DAD_GESTURE &&
		item && item->getActualType() != LLAssetType::AT_LINK && item->getActualType() != LLAssetType::AT_LINK_FOLDER && item->getType() != LLAssetType::AT_CATEGORY &&
		item->getPermissions().getMaskOwner() & PERM_COPY && item->getPermissions().getMaskOwner() & PERM_TRANSFER)
	{
		if (drop)
		{
			if (!mDADSignal.empty())
			{
				mDADSignal(item->getUUID());
			}
		}
		else
		{
			*accept = ACCEPT_YES_SINGLE;
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	return TRUE;
}


BOOL FSInventoryLinkReplaceDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
														   EDragAndDropType cargo_type,
														   void* cargo_data,
														   EAcceptance* accept,
														   std::string& tooltip_msg)
{
	LLInventoryItem* item = (LLInventoryItem*)cargo_data;

	if (cargo_type >= DAD_TEXTURE && cargo_type <= DAD_LINK &&
		item && item->getActualType() != LLAssetType::AT_LINK_FOLDER && item->getType() != LLAssetType::AT_CATEGORY &&
		(
			LLAssetType::lookupCanLink(item->getType()) ||
			(item->getType() == LLAssetType::AT_LINK && !gInventory.getObject(item->getLinkedUUID())) // Broken Link!
		))
	{
		if (drop)
		{
			setItem(item);
			if (!mDADSignal.empty())
			{
				mDADSignal(mItemID);
			}
		}
		else
		{
			*accept = ACCEPT_YES_SINGLE;
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	return TRUE;
}

void FSInventoryLinkReplaceDropTarget::setItem(LLInventoryItem* item)
{
	if (item)
	{
		mItemID = item->getLinkedUUID();
		setText(item->getName());
	}
	else
	{
		mItemID.setNull();
		setText(LLStringExplicit(""));
	}
}


FSDropTarget::FSDropTarget(const FSDropTarget::Params& p)
	: LLView(p),
	mAgentID(p.agent_id)
{}

BOOL FSDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data,
									 EAcceptance* accept,
									 std::string& tooltip_msg)
{
	if (getParent())
	{
		LLToolDragAndDrop::handleGiveDragAndDrop(mAgentID, LLUUID::null, drop,
												 cargo_type, cargo_data, accept);

		return TRUE;
	}

	return FALSE;
}

BOOL FSEmbeddedItemDropTarget::handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
												EDragAndDropType cargo_type,
												void* cargo_data,
												EAcceptance* accept,
												std::string& tooltip_msg)
{
	LLInventoryItem* item = (LLInventoryItem*)cargo_data;

	if (cargo_type >= DAD_TEXTURE && cargo_type <= DAD_GESTURE &&
		item && item->getActualType() != LLAssetType::AT_LINK && item->getActualType() != LLAssetType::AT_LINK_FOLDER && item->getType() != LLAssetType::AT_CATEGORY &&
		item->getPermissions().getMaskOwner() & PERM_COPY && item->getPermissions().getMaskOwner() & PERM_TRANSFER)
	{
		if (drop)
		{
			if (!mDADSignal.empty())
			{
				mDADSignal(item->getUUID());
			}
		}
		else
		{
			*accept = ACCEPT_YES_SINGLE;
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	return TRUE;
}

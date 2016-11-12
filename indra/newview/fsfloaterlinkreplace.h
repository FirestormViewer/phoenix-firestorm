/**
 * @file fsfloaterlinkreplace.h
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

#ifndef FS_FLOATERLINKREPLACE_H
#define FS_FLOATERLINKREPLACE_H

#include "llfloater.h"
#include "lleventtimer.h"
#include "llinventoryfunctions.h"
#include "llviewerinventory.h"

class FSInventoryLinkReplaceDropTarget;
class LLButton;
class LLTextBox;
class LLInventoryCallback;

class FSFloaterLinkReplace : public LLFloater, LLEventTimer
{
	LOG_CLASS(FSFloaterLinkReplace);

public:
	FSFloaterLinkReplace(const LLSD& key);
	virtual ~FSFloaterLinkReplace();

	BOOL postBuild();
	virtual void onOpen(const LLSD& key);

	virtual BOOL tick();

private:
	void checkEnableStart();
	void onStartClicked();
	void decreaseOpenItemCount();
	void updateFoundLinks();
	void processBatch(LLInventoryModel::item_array_t items);

	void linkCreatedCallback(const LLUUID& old_item_id,
								const LLUUID& target_item_id,
								bool needs_wearable_ordering_update,
								bool needs_description_update,
								const LLUUID& outfit_folder_id);
	void itemRemovedCallback(const LLUUID& outfit_folder_id);

	void onSourceItemDrop(const LLUUID& source_item_id);
	void onTargetItemDrop(const LLUUID& target_item_id);

	FSInventoryLinkReplaceDropTarget*	mSourceEditor;
	FSInventoryLinkReplaceDropTarget*	mTargetEditor;
	LLButton*							mStartBtn;
	LLButton*							mRefreshBtn;
	LLTextBox*							mStatusText;

	LLUUID	mSourceUUID;
	LLUUID	mTargetUUID;
	U32		mRemainingItems;
	U32		mBatchSize;

	LLInventoryModel::item_array_t	mRemainingInventoryItems;

	FSFloaterLinkReplace* mInstance;
};

#endif // FS_FLOATERLINKREPLACE_H

/**
 * @file fsfloaterwearablefavorites.h
 * @brief Class for the favorite wearables floater
 *
 * $LicenseInfo:firstyear=2018&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2018 Ansariel Hiller @ Second Life
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

#ifndef FS_FLOATERWEARABLEFAVORITES_H
#define FS_FLOATERWEARABLEFAVORITES_H

#include "llfloater.h"
#include "llwearableitemslist.h"

class LLButton;
class LLFilterEditor;
class LLMenuButton;
class LLInventoryCategoriesObserver;

class FSWearableFavoritesItemsList : public LLWearableItemsList
{
public:
	struct Params : public LLInitParam::Block<Params, LLWearableItemsList::Params>
	{
		Params()
		{}
	};

	virtual ~FSWearableFavoritesItemsList() {}

	/* virtual */ BOOL	handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);

	typedef boost::signals2::signal<void(const LLUUID& id)> item_dad_callback_t;
	boost::signals2::connection setDADCallback(const item_dad_callback_t::slot_type& cb)
	{
		return mDADSignal.connect(cb);
	}

protected:
	friend class LLUICtrlFactory;
	FSWearableFavoritesItemsList(const Params&);

	item_dad_callback_t mDADSignal;
};

class FSFloaterWearableFavorites : public LLFloater
{
public:
	FSFloaterWearableFavorites(const LLSD& key);
	virtual ~FSFloaterWearableFavorites();

	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& info);
	/*virtual*/ void draw();
	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
	/*virtual*/ bool hasAccelerators() const { return true; }

	static void initCategory();
	static LLUUID getFavoritesFolder();

private:
	void updateList(const LLUUID& folder_id);

	void onItemDAD(const LLUUID& item_id);
	void handleRemove();
	void onFilterEdit(const std::string& search_string);
	void onDoubleClick();

	void onOptionsMenuItemClicked(const LLSD& userdata);
	bool onOptionsMenuItemChecked(const LLSD& userdata);

	bool mInitialized;

	boost::signals2::connection mDADCallbackConnection;

	LLInventoryCategoriesObserver*	mCategoriesObserver;

	FSWearableFavoritesItemsList*	mItemsList;
	LLButton*						mRemoveItemBtn;
	LLFilterEditor*					mFilterEditor;
	LLMenuButton*					mOptionsButton;
	LLHandle<LLView>				mOptionsMenuHandle;

	static LLUUID sFolderID;
};

#endif // FS_FLOATERWEARABLEFAVORITES_H

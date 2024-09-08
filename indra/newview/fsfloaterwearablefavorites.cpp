/**
 * @file fsfloaterwearablefavorites.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterwearablefavorites.h"
#include "fscommon.h"
#include "llappearancemgr.h"
#include "llbutton.h"
#include "llfiltereditor.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llmenugl.h"
#include "llmenubutton.h"
#include "lltoggleablemenu.h"
#include "llviewercontrol.h"        // for gSavedSettings
#include "llviewermenu.h"           // for gMenuHolder
#include "rlvactions.h"
#include "rlvlocks.h"

// Hello Kokua! Have fun yoinking! ;)

#define FS_WEARABLE_FAVORITES_FOLDER "#Wearable Favorites"

static LLDefaultChildRegistry::Register<FSWearableFavoritesItemsList> r("fs_wearable_favorites_items_list");

FSWearableFavoritesItemsList::FSWearableFavoritesItemsList(const Params& p)
:   LLWearableItemsList(p)
{
}

bool FSWearableFavoritesItemsList::handleDragAndDrop(S32 x, S32 y, MASK mask,
                                                  bool drop,
                                                  EDragAndDropType cargo_type,
                                                  void* cargo_data,
                                                  EAcceptance* accept,
                                                  std::string& tooltip_msg)
{
    // Scroll folder view if needed.  Never accepts a drag or drop.
    *accept = ACCEPT_NO;
    autoScroll(x, y);

    if (cargo_type == DAD_BODYPART || cargo_type == DAD_CLOTHING || cargo_type == DAD_OBJECT)
    {
        if (drop)
        {
            LLInventoryItem* item = (LLInventoryItem*)cargo_data;
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

    return true;
}

LLUUID FSFloaterWearableFavorites::sFolderID = LLUUID();

FSFloaterWearableFavorites::FSFloaterWearableFavorites(const LLSD& key)
    : LLFloater(key),
    mItemsList(nullptr),
    mInitialized(false),
    mDADCallbackConnection()
{
    mCategoriesObserver = new LLInventoryCategoriesObserver();
}

FSFloaterWearableFavorites::~FSFloaterWearableFavorites()
{
    if (gInventory.containsObserver(mCategoriesObserver))
    {
        gInventory.removeObserver(mCategoriesObserver);
    }
    delete mCategoriesObserver;

    if (mDADCallbackConnection.connected())
    {
        mDADCallbackConnection.disconnect();
    }

    if (mOptionsMenuHandle.get())
    {
        mOptionsMenuHandle.get()->die();
    }
}

//virtual
bool FSFloaterWearableFavorites::postBuild()
{
    mItemsList = getChild<FSWearableFavoritesItemsList>("favorites_list");
    mItemsList->setNoFilteredItemsMsg(getString("search_no_items"));
    mItemsList->setDoubleClickCallback(boost::bind(&FSFloaterWearableFavorites::onDoubleClick, this));

    mRemoveItemBtn = getChild<LLButton>("remove_btn");
    mRemoveItemBtn->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::handleRemove, this));

    mFilterEditor = getChild<LLFilterEditor>("wearable_filter_input");
    mFilterEditor->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::onFilterEdit, this, _2));

    // Create menus.
    LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
    LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;

    registrar.add("FavWearables.Action",                boost::bind(&FSFloaterWearableFavorites::onOptionsMenuItemClicked, this, _2));
    enable_registrar.add("FavWearables.CheckAction",    boost::bind(&FSFloaterWearableFavorites::onOptionsMenuItemChecked, this, _2));

    mOptionsButton = getChild<LLMenuButton>("options_btn");

    if (LLToggleableMenu* options_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>("menu_fs_wearable_favorites.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance()); options_menu)
    {
        mOptionsMenuHandle = options_menu->getHandle();
        mOptionsButton->setMenu(options_menu, LLMenuButton::MP_BOTTOM_LEFT);
    }

    return true;
}

//virtual
void FSFloaterWearableFavorites::onOpen(const LLSD& /*info*/)
{
    if (!mInitialized)
    {
        if (sFolderID.isNull())
        {
            initCategory(boost::bind(&FSFloaterWearableFavorites::initialize, this));
        }
        else
        {
            initialize();
        }
    }
}

void FSFloaterWearableFavorites::initialize()
{
    LLViewerInventoryCategory* category = gInventory.getCategory(sFolderID);
    if (!category)
    {
        return;
    }

    const LLUUID cof = gInventory.findCategoryUUIDForType(LLFolderType::FT_CURRENT_OUTFIT);
    LLViewerInventoryCategory* category_cof = gInventory.getCategory(cof);
    if (!category_cof)
    {
        return;
    }

    gInventory.addObserver(mCategoriesObserver);
    mCategoriesObserver->addCategory(sFolderID, boost::bind(&FSFloaterWearableFavorites::updateList, this, sFolderID));
    mCategoriesObserver->addCategory(cof, boost::bind(&FSFloaterWearableFavorites::updateList, this, sFolderID));
    category->fetch();

    mItemsList->setSortOrder((LLWearableItemsList::ESortOrder)gSavedSettings.getU32("FSWearableFavoritesSortOrder"));
    updateList(sFolderID);
    mItemsList->setDADCallback(boost::bind(&FSFloaterWearableFavorites::onItemDAD, this, _1));

    mInitialized = true;
}

//virtual
void FSFloaterWearableFavorites::draw()
{
    LLFloater::draw();

    mRemoveItemBtn->setEnabled(mItemsList->numSelected() > 0);
}

//virtual
bool FSFloaterWearableFavorites::handleKeyHere(KEY key, MASK mask)
{
    if (FSCommon::isFilterEditorKeyCombo(key, mask))
    {
        mFilterEditor->setFocus(true);
        return true;
    }

    return LLFloater::handleKeyHere(key, mask);
}

// static
std::optional<LLUUID> FSFloaterWearableFavorites::getWearableFavoritesFolderID()
{
    if (LLUUID fs_root_cat_id = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER); !fs_root_cat_id.isNull())
    {
        LLInventoryModel::item_array_t* items;
        LLInventoryModel::cat_array_t* cats;
        gInventory.getDirectDescendentsOf(fs_root_cat_id, cats, items);
        if (cats)
        {
            for (const auto& cat : *cats)
            {
                if (cat->getName() == FS_WEARABLE_FAVORITES_FOLDER)
                {
                    return cat->getUUID();
                }
            }
        }
    }

    return std::nullopt;
}

// static
void FSFloaterWearableFavorites::initCategory(inventory_func_type callback)
{
    if (!gInventory.isInventoryUsable())
    {
        LL_WARNS() << "Cannot initialize Favorite Wearables inventory folder - inventory is not usable!" << LL_ENDL;
        return;
    }

    if (auto fs_favs_id = getWearableFavoritesFolderID(); fs_favs_id.has_value())
    {
        sFolderID = fs_favs_id.value();
        callback(sFolderID);
    }
    else
    {
        LLUUID fs_root_cat_id = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);
        if (fs_root_cat_id.isNull())
        {
            gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER, [callback](const LLUUID& new_cat_id)
            {
                gInventory.createNewCategory(new_cat_id, LLFolderType::FT_NONE, FS_WEARABLE_FAVORITES_FOLDER, [callback](const LLUUID& new_cat_id)
                {
                    FSFloaterWearableFavorites::sFolderID = new_cat_id;
                    callback(new_cat_id);
                });
            });
        }
        else
        {
            gInventory.createNewCategory(fs_root_cat_id, LLFolderType::FT_NONE, FS_WEARABLE_FAVORITES_FOLDER, [callback](const LLUUID& new_cat_id)
            {
                FSFloaterWearableFavorites::sFolderID = new_cat_id;
                callback(new_cat_id);
            });
        }
    }
}

//static
LLUUID FSFloaterWearableFavorites::getFavoritesFolder()
{
    if (!sFolderID.isNull())
    {
        if (auto fs_favs_id = getWearableFavoritesFolderID(); fs_favs_id.has_value())
        {
            sFolderID = fs_favs_id.value();
        }
    }

    return sFolderID;
}

void FSFloaterWearableFavorites::updateList(const LLUUID& folder_id)
{
    mItemsList->updateList(folder_id);

    if (gInventory.isCategoryComplete(folder_id))
    {
        mItemsList->setNoItemsCommentText(getString("empty_list")); // Have to reset it here because LLWearableItemsList::updateList might override it
    }
}

void FSFloaterWearableFavorites::onItemDAD(const LLUUID& item_id)
{
    link_inventory_object(sFolderID, item_id, LLPointer<LLInventoryCallback>(nullptr));
}

void FSFloaterWearableFavorites::handleRemove()
{
    uuid_vec_t selected_item_ids;
    mItemsList->getSelectedUUIDs(selected_item_ids);

    for (const auto& id : selected_item_ids)
    {
        remove_inventory_item(id, LLPointer<LLInventoryCallback>(nullptr));
    }
}

void FSFloaterWearableFavorites::onFilterEdit(const std::string& search_string)
{
    mItemsList->setFilterSubString(search_string, true);
    mItemsList->setNoItemsCommentText(getString("empty_list"));
    mItemsList->rearrange();
}

void FSFloaterWearableFavorites::onOptionsMenuItemClicked(const LLSD& userdata)
{
    const std::string action = userdata.asString();

    if (action == "sort_by_name")
    {
        mItemsList->setSortOrder(LLWearableItemsList::E_SORT_BY_NAME);
        gSavedSettings.setU32("FSWearableFavoritesSortOrder", LLWearableItemsList::E_SORT_BY_NAME);
    }
    else if (action == "sort_by_most_recent")
    {
        mItemsList->setSortOrder(LLWearableItemsList::E_SORT_BY_MOST_RECENT);
        gSavedSettings.setU32("FSWearableFavoritesSortOrder", LLWearableItemsList::E_SORT_BY_MOST_RECENT);
    }
    else if (action == "sort_by_type_name")
    {
        mItemsList->setSortOrder(LLWearableItemsList::E_SORT_BY_TYPE_NAME);
        gSavedSettings.setU32("FSWearableFavoritesSortOrder", LLWearableItemsList::E_SORT_BY_TYPE_NAME);
    }
}

bool FSFloaterWearableFavorites::onOptionsMenuItemChecked(const LLSD& userdata)
{
    const std::string action = userdata.asString();

    if (action == "sort_by_name")
    {
        return mItemsList->getSortOrder() == LLWearableItemsList::E_SORT_BY_NAME;
    }
    else if (action == "sort_by_most_recent")
    {
        return mItemsList->getSortOrder() == LLWearableItemsList::E_SORT_BY_MOST_RECENT;
    }
    else if (action == "sort_by_type_name")
    {
        return mItemsList->getSortOrder() == LLWearableItemsList::E_SORT_BY_TYPE_NAME;
    }

    return false;
}

void FSFloaterWearableFavorites::onDoubleClick()
{
    if (LLUUID selected_item_id = mItemsList->getSelectedUUID(); selected_item_id.notNull())
    {
        uuid_vec_t ids;
        ids.push_back(selected_item_id);
        LLViewerInventoryItem* item = gInventory.getItem(selected_item_id);

        if (get_is_item_worn(selected_item_id))
        {
            if ((item->getType() == LLAssetType::AT_CLOTHING && (!RlvActions::isRlvEnabled() || gRlvWearableLocks.canRemove(item))) ||
                ((item->getType() == LLAssetType::AT_OBJECT) && (!RlvActions::isRlvEnabled() || gRlvAttachmentLocks.canDetach(item))))
            {
                LLAppearanceMgr::instance().removeItemsFromAvatar(ids);
            }
        }
        else
        {
            if (item->getType() == LLAssetType::AT_BODYPART && (!RlvActions::isRlvEnabled() || (gRlvWearableLocks.canWear(item) & RLV_WEAR_REPLACE) == RLV_WEAR_REPLACE))
            {
                wear_multiple(ids, true);
            }
            else if (item->getType() == LLAssetType::AT_CLOTHING && LLAppearanceMgr::instance().canAddWearables(ids) && (!RlvActions::isRlvEnabled() || (gRlvWearableLocks.canWear(item) & RLV_WEAR_ADD) == RLV_WEAR_ADD))
            {
                wear_multiple(ids, false);
            }
            else if (item->getType() == LLAssetType::AT_OBJECT && LLAppearanceMgr::instance().canAddWearables(ids) && (!RlvActions::isRlvEnabled() || (gRlvAttachmentLocks.canAttach(item) & RLV_WEAR_ADD) == RLV_WEAR_ADD))
            {
                wear_multiple(ids, false);
            }
        }
    }
}

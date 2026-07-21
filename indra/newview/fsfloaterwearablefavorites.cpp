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
#include "llcombobox.h"
#include "llfiltereditor.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llmenugl.h"
#include "llmenubutton.h"
#include "llnotificationsutil.h"
#include "lltoggleablemenu.h"
#include "llviewercontrol.h"        // for gSavedSettings
#include "llviewerinventory.h"
#include "llviewermenu.h"           // for gMenuHolder
#include "rlvactions.h"
#include "rlvlocks.h"
#include <set>

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
    mNewFolderBtn(nullptr),
    mRenameFolderBtn(nullptr),
    mDeleteFolderBtn(nullptr),
    mFolderCombo(nullptr),
    mSelectedFolderID(),
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

    mNewFolderBtn = getChild<LLButton>("new_folder_btn");
    mNewFolderBtn->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::onNewFolder, this));

    mRenameFolderBtn = getChild<LLButton>("rename_folder_btn");
    mRenameFolderBtn->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::onRenameFolder, this));

    mDeleteFolderBtn = getChild<LLButton>("delete_folder_btn");
    mDeleteFolderBtn->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::onDeleteFolder, this));

    mFolderCombo = getChild<LLComboBox>("folder_combo");
    mFolderCombo->setCommitCallback(boost::bind(&FSFloaterWearableFavorites::onFolderChanged, this));

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
    mCategoriesObserver->addCategory(sFolderID, boost::bind(&FSFloaterWearableFavorites::refreshFolderCombo, this));
    mCategoriesObserver->addCategory(
        cof,
        [this]()
        {
            const LLUUID folder_id = mSelectedFolderID.notNull() ? mSelectedFolderID : sFolderID;
            updateList(folder_id);
        });
    category->fetch();

    mItemsList->setSortOrder((LLWearableItemsList::ESortOrder)gSavedSettings.getU32("FSWearableFavoritesSortOrder"));
    mSelectedFolderID = sFolderID;
    refreshFolderCombo();
    mItemsList->setDADCallback(boost::bind(&FSFloaterWearableFavorites::onItemDAD, this, _1));

    mInitialized = true;
}

//virtual
void FSFloaterWearableFavorites::draw()
{
    LLFloater::draw();

    mRemoveItemBtn->setEnabled(mItemsList->numSelected() > 0);
    const bool folder_action_enabled = mSelectedFolderID.notNull() && mSelectedFolderID != sFolderID;
    if (mRenameFolderBtn)
    {
        mRenameFolderBtn->setEnabled(folder_action_enabled);
    }
    if (mDeleteFolderBtn)
    {
        mDeleteFolderBtn->setEnabled(folder_action_enabled);
    }
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
    if (folder_id.isNull())
    {
        return;
    }

    // Favorite Wearables folders are implemented as inventory links.
    // The root view collects descendants recursively, which means the same
    // wearable can appear multiple times when it is linked into several
    // folders.  Keep folder views unchanged, but de-duplicate the root
    // "All Favorites" view by the original linked item UUID.
    if (folder_id == sFolderID)
    {
        LLInventoryModel::cat_array_t cat_array;
        LLInventoryModel::item_array_t item_array;

        LLFindOutfitItems collector;
        gInventory.collectDescendentsIf(
            folder_id,
            cat_array,
            item_array,
            LLInventoryModel::EXCLUDE_TRASH,
            collector);

        LLInventoryModel::item_array_t unique_items;
        std::set<LLUUID> seen_linked_items;

        for (const auto& item : item_array)
        {
            if (!item)
            {
                continue;
            }

            LLUUID linked_uuid = item->getLinkedUUID();
            if (linked_uuid.isNull())
            {
                linked_uuid = item->getUUID();
            }

            if (seen_linked_items.insert(linked_uuid).second)
            {
                unique_items.push_back(item);
            }
        }

        if (unique_items.empty() && gInventory.isCategoryComplete(folder_id))
        {
            mItemsList->setNoItemsCommentText(getString("empty_list"));
        }

        mItemsList->refreshList(unique_items);
    }
    else
    {
        mItemsList->updateList(folder_id);
    }

    if (gInventory.isCategoryComplete(folder_id))
    {
        mItemsList->setNoItemsCommentText(getString("empty_list")); // Have to reset it here because LLWearableItemsList::updateList might override it
    }
}

void FSFloaterWearableFavorites::refreshFolderCombo()
{
    if (!mFolderCombo || sFolderID.isNull())
    {
        return;
    }

    LLUUID selected_id = mSelectedFolderID;
    if (selected_id.isNull())
    {
        selected_id = sFolderID;
    }

    mFolderCombo->clearRows();
    mFolderCombo->add(getString("root_folder_label"), LLSD(sFolderID.asString()));

    LLInventoryModel::item_array_t* items;
    LLInventoryModel::cat_array_t* cats;
    gInventory.getDirectDescendentsOf(sFolderID, cats, items);
    if (cats)
    {
        std::vector<LLViewerInventoryCategory*> sorted_cats;
        for (const auto& cat : *cats)
        {
            if (cat)
            {
                sorted_cats.push_back(cat);
            }
        }

        std::sort(sorted_cats.begin(), sorted_cats.end(), [](const LLViewerInventoryCategory* lhs, const LLViewerInventoryCategory* rhs)
        {
            return LLStringUtil::compareDict(lhs->getName(), rhs->getName()) < 0;
        });

        for (const auto& cat : sorted_cats)
        {
            mFolderCombo->add(cat->getName(), LLSD(cat->getUUID().asString()));
        }
    }

    if (!mFolderCombo->selectByValue(LLSD(selected_id.asString())))
    {
        selected_id = sFolderID;
        mFolderCombo->selectByValue(LLSD(selected_id.asString()));
    }

    LLUUID previous_selected_id = mSelectedFolderID;

    mSelectedFolderID = selected_id;

    // Favorite Wearables v2.0.0:
    // Preserve the visible folder when inventory updates arrive from Assign to Folder.
    // This prevents assigning from a subfolder from bouncing the visible list back to All Favorites.
    if (previous_selected_id != mSelectedFolderID)
    {
        updateList(mSelectedFolderID);
    }
}

void FSFloaterWearableFavorites::onItemDAD(const LLUUID& item_id)
{
    const LLUUID target_folder = mSelectedFolderID.notNull() ? mSelectedFolderID : sFolderID;
    LLHandle<LLFloater> floater_handle = getHandle();

    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([floater_handle, target_folder](const LLUUID& new_item_id)
    {
        LLFloater* floater = floater_handle.get();
        FSFloaterWearableFavorites* self = dynamic_cast<FSFloaterWearableFavorites*>(floater);
        if (self)
        {
            self->updateList(target_folder);
        }
    });

    link_inventory_object(target_folder, item_id, cb);
}

void FSFloaterWearableFavorites::handleRemove()
{
    uuid_vec_t selected_item_ids;
    mItemsList->getSelectedUUIDs(selected_item_ids);

    const LLUUID folder_id = mSelectedFolderID.notNull() ? mSelectedFolderID : sFolderID;
    LLHandle<LLFloater> floater_handle = getHandle();

    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([floater_handle, folder_id](const LLUUID& item_id)
    {
        LLFloater* floater = floater_handle.get();
        FSFloaterWearableFavorites* self = dynamic_cast<FSFloaterWearableFavorites*>(floater);
        if (self)
        {
            self->updateList(folder_id);
        }
    });

    for (const auto& id : selected_item_ids)
    {
        remove_inventory_item(id, cb);
    }

    updateList(folder_id);
}

void FSFloaterWearableFavorites::onFilterEdit(const std::string& search_string)
{
    mItemsList->setFilterSubString(search_string, true);
    mItemsList->setNoItemsCommentText(getString("empty_list"));
    mItemsList->rearrange();
}

void FSFloaterWearableFavorites::onFolderChanged()
{
    if (!mFolderCombo)
    {
        return;
    }

    LLSD selected_value = mFolderCombo->getSelectedValue();
    LLUUID selected_id(selected_value.asString());
    if (selected_id.isNull())
    {
        selected_id = sFolderID;
    }

    LLUUID previous_selected_id = mSelectedFolderID;

    mSelectedFolderID = selected_id;

    // Favorite Wearables v2.0.0:
    // Preserve the visible folder when inventory updates arrive from Assign to Folder.
    // This prevents assigning from a subfolder from bouncing the visible list back to All Favorites.
    if (previous_selected_id != mSelectedFolderID)
    {
        updateList(mSelectedFolderID);
    }
}

void FSFloaterWearableFavorites::onNewFolder()
{
    LLNotificationsUtil::add("FSWearableFavoritesNewFolder", LLSD(), LLSD(), boost::bind(&FSFloaterWearableFavorites::onNewFolderCallback, this, _1, _2));
}

bool FSFloaterWearableFavorites::onNewFolderCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }

    std::string folder_name = response["message"].asString();
    LLStringUtil::trim(folder_name);

    if (folder_name.empty())
    {
        return false;
    }

    if (folder_name.find_first_of("/\\") != std::string::npos)
    {
        LLSD args;
        args["FOLDER_NAME"] = folder_name;
        LLNotificationsUtil::add("FSWearableFavoritesFolderInvalidName", args);
        return false;
    }

    gInventory.createNewCategory(sFolderID, LLFolderType::FT_NONE, folder_name, [this](const LLUUID& new_cat_id)
    {
        if (new_cat_id.notNull())
        {
            mSelectedFolderID = new_cat_id;
            refreshFolderCombo();
            if (mFolderCombo)
            {
                mFolderCombo->selectByValue(LLSD(new_cat_id.asString()));
            }
            updateList(mSelectedFolderID);
        }
    });

    return true;
}


void FSFloaterWearableFavorites::onRenameFolder()
{
    if (mSelectedFolderID.isNull() || mSelectedFolderID == sFolderID)
    {
        return;
    }

    LLSD args;
    if (LLViewerInventoryCategory* cat = gInventory.getCategory(mSelectedFolderID))
    {
        args["FOLDER_NAME"] = cat->getName();
    }
    LLNotificationsUtil::add("FSWearableFavoritesRenameFolder", args, LLSD(), boost::bind(&FSFloaterWearableFavorites::onRenameFolderCallback, this, _1, _2));
}

bool FSFloaterWearableFavorites::onRenameFolderCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }

    if (mSelectedFolderID.isNull() || mSelectedFolderID == sFolderID)
    {
        return false;
    }

    std::string folder_name = response["message"].asString();
    LLStringUtil::trim(folder_name);

    if (folder_name.empty())
    {
        return false;
    }

    if (folder_name.find_first_of("/\\") != std::string::npos)
    {
        LLSD args;
        args["FOLDER_NAME"] = folder_name;
        LLNotificationsUtil::add("FSWearableFavoritesFolderInvalidName", args);
        return false;
    }

    const LLUUID folder_id = mSelectedFolderID;
    LLHandle<LLFloater> floater_handle = getHandle();

    LLSD updates;
    updates["name"] = folder_name;

    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([floater_handle, folder_id](const LLUUID& cat_id)
    {
        LLFloater* floater = floater_handle.get();
        FSFloaterWearableFavorites* self = dynamic_cast<FSFloaterWearableFavorites*>(floater);
        if (self)
        {
            self->mSelectedFolderID = folder_id;
            self->refreshFolderCombo();
            if (self->mFolderCombo)
            {
                self->mFolderCombo->selectByValue(LLSD(folder_id.asString()));
            }
            self->updateList(folder_id);
        }
    });

    update_inventory_category(folder_id, updates, cb);
    refreshFolderCombo();
    updateList(folder_id);

    return true;
}

void FSFloaterWearableFavorites::onDeleteFolder()
{
    if (mSelectedFolderID.isNull() || mSelectedFolderID == sFolderID)
    {
        return;
    }

    LLSD args;
    LLViewerInventoryCategory* cat = gInventory.getCategory(mSelectedFolderID);
    if (cat)
    {
        args["FOLDER_NAME"] = cat->getName();
    }
    else
    {
        args["FOLDER_NAME"] = "Selected Folder";
    }

    LLInventoryModel::item_array_t* items = nullptr;
    LLInventoryModel::cat_array_t* cats = nullptr;
    gInventory.getDirectDescendentsOf(mSelectedFolderID, cats, items);

    S32 item_count = items ? (S32)items->size() : 0;
    S32 folder_count = cats ? (S32)cats->size() : 0;
    args["ITEM_COUNT"] = item_count;
    args["FOLDER_COUNT"] = folder_count;

    LLNotificationsUtil::add("FSWearableFavoritesDeleteFolder", args, LLSD(), boost::bind(&FSFloaterWearableFavorites::onDeleteFolderCallback, this, _1, _2));
}

bool FSFloaterWearableFavorites::onDeleteFolderCallback(const LLSD& notification, const LLSD& response)
{
    if (LLNotificationsUtil::getSelectedOption(notification, response) != 0)
    {
        return false;
    }

    if (mSelectedFolderID.isNull() || mSelectedFolderID == sFolderID)
    {
        return false;
    }

    const LLUUID folder_id = mSelectedFolderID;
    LLHandle<LLFloater> floater_handle = getHandle();

    LLPointer<LLInventoryCallback> cb = new LLBoostFuncInventoryCallback([floater_handle](const LLUUID& deleted_id)
    {
        LLFloater* floater = floater_handle.get();
        FSFloaterWearableFavorites* self = dynamic_cast<FSFloaterWearableFavorites*>(floater);
        if (self)
        {
            self->mSelectedFolderID = FSFloaterWearableFavorites::sFolderID;
            self->refreshFolderCombo();
            if (self->mFolderCombo)
            {
                self->mFolderCombo->selectByValue(LLSD(FSFloaterWearableFavorites::sFolderID.asString()));
            }
            self->updateList(FSFloaterWearableFavorites::sFolderID);
        }
    });

    remove_inventory_category(folder_id, cb);

    mSelectedFolderID = sFolderID;
    refreshFolderCombo();
    updateList(sFolderID);

    return true;
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

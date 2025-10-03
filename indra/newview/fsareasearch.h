/**
 * @file fsareasearch.h
 * @brief Floater to search and list objects in view or is known to the viewer.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Techwolf Lupindo
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

#ifndef FS_AREASEARCH_H
#define FS_AREASEARCH_H

#include "llcategory.h"
#include "llfloater.h"
#include "llframetimer.h"
#include "llpermissions.h"
#include "llsaleinfo.h"
#include "llscrolllistcolumn.h"
#include "llviewerobject.h"
#include "rlvdefines.h"
#include <boost/regex.hpp>

class LLAvatarName;
class LLTextBox;
class LLViewerRegion;
class LLCheckBoxCtrl;
class LLLineEditor;
class LLTabContainer;
class LLContextMenu;
class LLSpinCtrl;
class LLComboBox;

class FSPanelAreaSearchList;
class FSPanelAreaSearchFind;
class FSPanelAreaSearchFilter;
class FSPanelAreaSearchAdvanced;
class FSPanelAreaSearchOptions;
class FSScrollListCtrl;

struct FSObjectProperties
{
    LLUUID id;
    bool listed{ false };
    std::string name;
    std::string description;
    std::string touch_name;
    std::string sit_name;
    LLUUID creator_id;
    LLUUID owner_id;
    LLUUID group_id;
    LLUUID ownership_id;
    bool group_owned{ false };
    U64 creation_date{ 0 };
    U32 base_mask{ 0 };
    U32 owner_mask{ 0 };
    U32 group_mask{ 0 };
    U32 everyone_mask{ 0 };
    U32 next_owner_mask{ 0 };
    LLSaleInfo sale_info;
    LLCategory category;
    LLUUID last_owner_id;
    LLAggregatePermissions ag_perms;
    LLAggregatePermissions ag_texture_perms;
    LLAggregatePermissions ag_texture_perms_owner;
    LLPermissions permissions;
    uuid_vec_t texture_ids;
    bool name_requested{ false };
    U32 local_id{ 0 };
    U64 region_handle{ 0 };

    typedef enum e_object_properties_request
    {
        NEED,
        SENT,
        FINISHED,
        FAILED
    } EObjectPropertiesRequest;
    EObjectPropertiesRequest request{ NEED };
};

//---------------------------------------------------------------------
// Main class for area search
// holds the search engine and main floater
// --------------------------------------------------------------------
class FSAreaSearch : public LLFloater
{
    LOG_CLASS(FSAreaSearch);
public:
    FSAreaSearch(const LLSD &);
    virtual ~FSAreaSearch();

    bool postBuild() override;
    virtual void draw() override;
    virtual void onOpen(const LLSD& key) override;

    void avatarNameCacheCallback(const LLUUID& id, const LLAvatarName& av_name);
    void callbackLoadFullName(const LLUUID& id, const std::string& full_name);
    void processObjectProperties(LLMessageSystem* msg);
    void updateObjectCosts(const LLUUID& object_id, F32 object_cost, F32 link_cost, F32 physics_cost, F32 link_physics_cost);
    static void idle(void *user_data);

    void checkRegion();
    void refreshList(bool cache_clear);
    void onCommitLine();
    void clearSearchText();
    void onButtonClickedSearch();
    void onCommitCheckboxRegex();
    bool isSearchableObject(LLViewerObject* objectp, LLViewerRegion* our_region) const;
    void setFindOwnerText(const std::string& value);

    std::map<LLUUID, FSObjectProperties> mObjectDetails;

    FSPanelAreaSearchAdvanced* getPanelAdvanced() { return mPanelAdvanced; }
    FSPanelAreaSearchList* getPanelList() { return mPanelList; }

    void setFilterForSale(bool b) { mFilterForSale = b; }
    void setFilterLocked(bool b) { mFilterLocked = b; }
    void setFilterPhysical(bool b) { mFilterPhysical = b; }
    void setFilterTemporary(bool b) { mFilterTemporary = b; }
    void setFilterPhantom(bool b) { mFilterPhantom = b; }
    void setFilterAttachment(bool b) { mFilterAttachment = b; }
    void setFilterMoaP(bool b) { mFilterMoaP = b; }
    void setFilterReflectionProbe(bool b) { mFilterReflectionProbe = b; }

    void setRegexSearch(bool b) { mRegexSearch = b; }
    void setBeacons(bool b) { mBeacons = b; }

    void setExcludeAttachment(bool b) { mExcludeAttachment = b; }
    void setExcludetemporary(bool b) { mExcludeTemporary = b; }
    void setExcludeReflectionProbe(bool b) { mExcludeReflectionProbe = b; }
    void setExcludePhysics(bool b) { mExcludePhysics = b; }
    void setExcludeChildPrims(bool b) { mExcludeChildPrims = b; }
    void setExcludeNeighborRegions(bool b) { mExcludeNeighborRegions = b; }

    void setFilterForSaleMin(S32 s) { mFilterForSaleMin = s; }
    void setFilterForSaleMax(S32 s) { mFilterForSaleMax = s; }

    void setFilterClickAction(bool b) { mFilterClickAction = b; }
    void setFilterClickActionType(U8 u) { mFilterClickActionType = u; }

    void setFilterDistance(bool b) { mFilterDistance = b; }
    void setFilterDistanceMin(S32 s) { mFilterDistanceMin = s; }
    void setFilterDistanceMax(S32 s) { mFilterDistanceMax = s; }

    void setFilterPermCopy(bool b) { mFilterPermCopy = b; }
    void setFilterPermModify(bool b) { mFilterPermModify = b; }
    void setFilterPermTransfer(bool b) { mFilterPermTransfer = b; }

    void setFilterAgentParcelOnly(bool b) { mFilterAgentParcelOnly = b; }

    bool isActive() const { return mActive; }

private:
    void requestObjectProperties(const std::vector<U32>& request_list, bool select, LLViewerRegion* regionp);
    void matchObject(FSObjectProperties& details, LLViewerObject* objectp);
    void getNameFromUUID(const LLUUID& id, std::string& name, bool group, bool& name_requested);

    void updateCounterText();
    bool regexTest(std::string_view text);
    void findObjects();
    void processRequestQueue();

    boost::signals2::connection mRlvBehaviorCallbackConnection{};
    void updateRlvRestrictions(ERlvBehaviour behavior);

    S32 mRequested{ 0 };
    bool mRefresh{ false };
    S32 mSearchableObjects{ 0 };
    bool mActive{ false };
    bool mRequestQueuePause{ false };
    bool mRequestNeedsSent{ false };
    std::map<U64,S32> mRegionRequests;

    std::string mSearchName;
    std::string mSearchDescription;
    std::string mSearchOwner;
    std::string mSearchGroup;
    std::string mSearchCreator;
    std::string mSearchLastOwner;

    bool mRegexSearch{ false };
    boost::regex mRegexSearchName;
    boost::regex mRegexSearchDescription;
    boost::regex mRegexSearchOwner;
    boost::regex mRegexSearchGroup;
    boost::regex mRegexSearchCreator;
    boost::regex mRegexSearchLastOwner;

    LLFrameTimer mLastUpdateTimer;
    LLFrameTimer mLastPropertiesReceivedTimer;

    uuid_set_t mNamesRequested;

    using name_cache_connection_map_t = std::map<LLUUID, boost::signals2::connection>;
    name_cache_connection_map_t mNameCacheConnections;

    LLViewerRegion* mLastRegion{ nullptr };

    class FSParcelChangeObserver;
    friend class FSParcelChangeObserver;
    std::unique_ptr<FSParcelChangeObserver> mParcelChangedObserver;

    LLTabContainer* mTab{ nullptr };
    FSPanelAreaSearchList* mPanelList{ nullptr };
    FSPanelAreaSearchFind* mPanelFind{ nullptr };
    FSPanelAreaSearchFilter* mPanelFilter{ nullptr };
    FSPanelAreaSearchOptions* mPanelOptions{ nullptr };
    FSPanelAreaSearchAdvanced* mPanelAdvanced{ nullptr };

    bool mBeacons{ false };

    bool mExcludeAttachment{ false };
    bool mExcludeTemporary{ false };
    bool mExcludeReflectionProbe{ false };
    bool mExcludePhysics{ false };
    bool mExcludeChildPrims{ true };
    bool mExcludeNeighborRegions{ true };

    bool mFilterLocked{ false };
    bool mFilterPhysical{ true };
    bool mFilterTemporary{ true };
    bool mFilterPhantom{ false };
    bool mFilterAttachment{ false };
    bool mFilterMoaP{ false };
    bool mFilterReflectionProbe{ false };

    bool mFilterForSale{ false };
    S32 mFilterForSaleMin{ 0 };
    S32 mFilterForSaleMax{ 999999 };

    bool mFilterDistance{ false };
    S32 mFilterDistanceMin{ 0 };
    S32 mFilterDistanceMax{ 0 };

    bool mFilterClickAction{ false };
    U8 mFilterClickActionType{ 0 };

    bool mFilterPermCopy{ false };
    bool mFilterPermModify{ false };
    bool mFilterPermTransfer{ false };

    bool mFilterAgentParcelOnly{ false };

protected:
    static void* createPanelList(void* data);
    static void* createPanelFind(void* data);
    static void* createPanelFilter(void* data);
    static void* createPanelAdvanced(void* data);
    static void* createPanelOptions(void* data);
};


//------------------------------------------------------------
// List panel
// displays the found objects
//------------------------------------------------------------
class FSPanelAreaSearchList
:   public LLPanel
{
    LOG_CLASS(FSPanelAreaSearchList);
    friend class FSAreaSearchMenu;
    friend class FSPanelAreaSearchOptions;

public:
    FSPanelAreaSearchList(FSAreaSearch* pointer);
    virtual ~FSPanelAreaSearchList();

    bool postBuild() override;

    void setCounterText();
    void setCounterText(LLStringUtil::format_map_t args);
    void updateScrollList();
    void updateName(const LLUUID& id, const std::string& name);
    static void touchObject(LLViewerObject* objectp);

    FSScrollListCtrl* getResultList() { return mResultList; }
    void updateResultListColumns();

    void setAgentLastPosition(LLVector3d d) { mAgentLastPosition = d; }
    LLVector3d getAgentLastPosition() { return mAgentLastPosition; }

private:
    void onDoubleClick();
    void onClickRefresh();
    void buyObject(FSObjectProperties& details, LLViewerObject* objectp);
    void sitOnObject(FSObjectProperties& details, LLViewerObject* objectp);
    void onCommitCheckboxBeacons();

    bool onContextMenuItemClick(const LLSD& userdata);
    bool onContextMenuItemEnable(const LLSD& userdata);
    bool onContextMenuItemVisibleRLV(const LLSD& userdata);

    void onColumnVisibilityChecked(const LLSD& userdata);
    bool onEnableColumnVisibilityChecked(const LLSD& userdata);

    LLVector3d mAgentLastPosition;

    FSAreaSearch* mFSAreaSearch{ nullptr };
    LLButton* mRefreshButton{ nullptr };
    FSScrollListCtrl* mResultList{ nullptr };
    LLCheckBoxCtrl* mCheckboxBeacons{ nullptr };
    LLTextBox* mCounterText{ nullptr };

    std::map<std::string, U32, std::less<>> mColumnBits;
    boost::signals2::connection mFSAreaSearchColumnConfigConnection{};
};


//------------------------------------------------------------
// Find panel
// UI for what objects to search for
//------------------------------------------------------------
class FSPanelAreaSearchFind
:   public LLPanel
{
    LOG_CLASS(FSPanelAreaSearchFind);
public:
    FSPanelAreaSearchFind(FSAreaSearch* pointer);
    virtual ~FSPanelAreaSearchFind() = default;

    bool postBuild() override;
    bool handleKeyHere(KEY key,MASK mask) override;

    LLLineEditor* mNameLineEditor{ nullptr };
    LLLineEditor* mDescriptionLineEditor{ nullptr };
    LLLineEditor* mOwnerLineEditor{ nullptr };
    LLLineEditor* mGroupLineEditor{ nullptr };
    LLLineEditor* mCreatorLineEditor{ nullptr };
    LLLineEditor* mLastOwnerLineEditor{ nullptr };
    LLCheckBoxCtrl* mCheckboxRegex{ nullptr };

private:
    void onButtonClickedClear();

    FSAreaSearch* mFSAreaSearch{ nullptr };

    LLButton* mSearchButton{ nullptr };
    LLButton* mClearButton{ nullptr };
};


//------------------------------------------------------------
// Filter panel
// "filter" the list to certion object types.
//------------------------------------------------------------
class FSPanelAreaSearchFilter
:   public LLPanel
{
    LOG_CLASS(FSPanelAreaSearchFilter);
public:
    FSPanelAreaSearchFilter(FSAreaSearch* pointer);
    virtual ~FSPanelAreaSearchFilter() = default;

    bool postBuild() override;

private:
    void onCommitCheckbox();
    void onCommitSpin();
    void onCommitCombo();
    void onButtonClickedSaveAsDefault();

    FSAreaSearch* mFSAreaSearch{ nullptr };
    LLCheckBoxCtrl* mCheckboxForSale{ nullptr };
    LLCheckBoxCtrl* mCheckboxPhysical{ nullptr };
    LLCheckBoxCtrl* mCheckboxTemporary{ nullptr };
    LLCheckBoxCtrl* mCheckboxLocked{ nullptr };
    LLCheckBoxCtrl* mCheckboxPhantom{ nullptr };
    LLCheckBoxCtrl* mCheckboxMoaP{ nullptr };
    LLCheckBoxCtrl* mCheckboxReflectionProbe{ nullptr };
    LLCheckBoxCtrl* mCheckboxDistance{ nullptr };
    LLSpinCtrl* mSpinDistanceMinValue{ nullptr };
    LLSpinCtrl* mSpinDistanceMaxValue{ nullptr };
    LLSpinCtrl* mSpinForSaleMinValue{ nullptr };
    LLSpinCtrl* mSpinForSaleMaxValue{ nullptr };
    LLButton* mButtonApply{ nullptr };
    LLComboBox* mComboClickAction{ nullptr };
    LLCheckBoxCtrl* mCheckboxAttachment{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludeAttachment{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludePhysics{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludetemporary{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludeReflectionProbes{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludeChildPrim{ nullptr };
    LLCheckBoxCtrl* mCheckboxExcludeNeighborRegions{ nullptr };
    LLCheckBoxCtrl* mCheckboxPermCopy{ nullptr };
    LLCheckBoxCtrl* mCheckboxPermModify{ nullptr };
    LLCheckBoxCtrl* mCheckboxPermTransfer{ nullptr };
    LLCheckBoxCtrl* mCheckboxAgentParcelOnly{ nullptr };
};


//------------------------------------------------------------
// Options panel
//------------------------------------------------------------
class FSPanelAreaSearchOptions
:   public LLPanel
{
    LOG_CLASS(FSPanelAreaSearchOptions);
public:
    FSPanelAreaSearchOptions(FSAreaSearch* pointer);
    virtual ~FSPanelAreaSearchOptions() = default;

private:
    void onCommitCheckboxDisplayColumn(const LLSD& userdata);
    bool onEnableColumnVisibilityChecked(const LLSD& userdata);

    FSAreaSearch* mFSAreaSearch{ nullptr };

    std::map<std::string, LLScrollListColumn::Params, std::less<>> mColumnParms;
};


//------------------------------------------------------------
// Advanced panel
//------------------------------------------------------------
class FSPanelAreaSearchAdvanced
:   public LLPanel
{
    LOG_CLASS(FSPanelAreaSearchAdvanced);
public:
    FSPanelAreaSearchAdvanced() = default;
    virtual ~FSPanelAreaSearchAdvanced() = default;

    bool postBuild() override;

    LLCheckBoxCtrl* mCheckboxClickTouch{ nullptr };
    LLCheckBoxCtrl* mCheckboxClickBuy{ nullptr };
    LLCheckBoxCtrl* mCheckboxClickSit{ nullptr };
};

#endif // FS_AREASEARCH_H

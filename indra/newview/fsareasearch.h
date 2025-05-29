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
    bool listed;
    std::string name;
    std::string description;
    std::string touch_name;
    std::string sit_name;
    LLUUID creator_id;
    LLUUID owner_id;
    LLUUID group_id;
    LLUUID ownership_id;
    bool group_owned;
    U64 creation_date;
    U32 base_mask, owner_mask, group_mask, everyone_mask, next_owner_mask;
    LLSaleInfo sale_info;
    LLCategory category;
    LLUUID last_owner_id;
    LLAggregatePermissions ag_perms;
    LLAggregatePermissions ag_texture_perms;
    LLAggregatePermissions ag_texture_perms_owner;
    LLPermissions permissions;
    uuid_vec_t texture_ids;
    bool name_requested;
    U32 local_id;
    U64 region_handle;

    typedef enum e_object_properties_request
    {
        NEED,
        SENT,
        FINISHED,
        FAILED
    } EObjectPropertiesRequest;
    EObjectPropertiesRequest request;

    FSObjectProperties() :
        request(NEED),
        listed(false),
        name_requested(false)
    {
    }
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

    /*virtual*/ bool postBuild();
    virtual void draw();
    virtual void onOpen(const LLSD& key);

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
    bool isSearchableObject (LLViewerObject* objectp, LLViewerRegion* our_region);
    void setFindOwnerText(std::string value);

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

    bool isActive() { return mActive; }

private:
    void requestObjectProperties(const std::vector< U32 >& request_list, bool select, LLViewerRegion* regionp);
    void matchObject(FSObjectProperties& details, LLViewerObject* objectp);
    void getNameFromUUID(const LLUUID& id, std::string& name, bool group, bool& name_requested);

    void updateCounterText();
    bool regexTest(std::string_view text);
    void findObjects();
    void processRequestQueue();

    boost::signals2::connection mRlvBehaviorCallbackConnection;
    void updateRlvRestrictions(ERlvBehaviour behavior);

    S32 mRequested;
    bool mRefresh;
    S32 mSearchableObjects;
    bool mActive;
    bool mRequestQueuePause;
    bool mRequestNeedsSent;
    std::map<U64,S32> mRegionRequests;

    std::string mSearchName;
    std::string mSearchDescription;
    std::string mSearchOwner;
    std::string mSearchGroup;
    std::string mSearchCreator;
    std::string mSearchLastOwner;

    bool mRegexSearch;
    boost::regex mRegexSearchName;
    boost::regex mRegexSearchDescription;
    boost::regex mRegexSearchOwner;
    boost::regex mRegexSearchGroup;
    boost::regex mRegexSearchCreator;
    boost::regex mRegexSearchLastOwner;

    LLFrameTimer mLastUpdateTimer;
    LLFrameTimer mLastPropertiesReceivedTimer;

    uuid_vec_t mNamesRequested;

    typedef std::map<LLUUID, boost::signals2::connection> name_cache_connection_map_t;
    name_cache_connection_map_t mNameCacheConnections;

    LLViewerRegion* mLastRegion;

    class FSParcelChangeObserver;
    friend class FSParcelChangeObserver;
    std::unique_ptr<FSParcelChangeObserver> mParcelChangedObserver;

    LLTabContainer* mTab;
    FSPanelAreaSearchList* mPanelList;
    FSPanelAreaSearchFind* mPanelFind;
    FSPanelAreaSearchFilter* mPanelFilter;
    FSPanelAreaSearchOptions* mPanelOptions;
    FSPanelAreaSearchAdvanced* mPanelAdvanced;

    bool mBeacons;

    bool mExcludeAttachment;
    bool mExcludeTemporary;
    bool mExcludeReflectionProbe;
    bool mExcludePhysics;
    bool mExcludeChildPrims;
    bool mExcludeNeighborRegions;

    bool mFilterLocked;
    bool mFilterPhysical;
    bool mFilterTemporary;
    bool mFilterPhantom;
    bool mFilterAttachment;
    bool mFilterMoaP;
    bool mFilterReflectionProbe;

    bool mFilterForSale;
    S32 mFilterForSaleMin;
    S32 mFilterForSaleMax;

    bool mFilterDistance;
    S32 mFilterDistanceMin;
    S32 mFilterDistanceMax;

    bool mFilterClickAction;
    U8 mFilterClickActionType;

    bool mFilterPermCopy;
    bool mFilterPermModify;
    bool mFilterPermTransfer;

    bool mFilterAgentParcelOnly;

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

    /*virtual*/ bool postBuild();

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

    FSAreaSearch* mFSAreaSearch;
    LLButton* mRefreshButton;
    FSScrollListCtrl* mResultList;
    LLCheckBoxCtrl* mCheckboxBeacons;
    LLTextBox* mCounterText;

    std::map<std::string, U32> mColumnBits;
    boost::signals2::connection mFSAreaSearchColumnConfigConnection;
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

    /*virtual*/ bool postBuild();
    /*virtual*/ bool handleKeyHere(KEY key,MASK mask);

    LLLineEditor* mNameLineEditor;
    LLLineEditor* mDescriptionLineEditor;
    LLLineEditor* mOwnerLineEditor;
    LLLineEditor* mGroupLineEditor;
    LLLineEditor* mCreatorLineEditor;
    LLLineEditor* mLastOwnerLineEditor;
    LLCheckBoxCtrl* mCheckboxRegex;

private:
    void onButtonClickedClear();

    FSAreaSearch* mFSAreaSearch;

    LLButton* mSearchButton;
    LLButton* mClearButton;
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

    /*virtual*/ bool postBuild();

private:
    void onCommitCheckbox();
    void onCommitSpin();
    void onCommitCombo();

    FSAreaSearch* mFSAreaSearch;
    LLCheckBoxCtrl* mCheckboxForSale;
    LLCheckBoxCtrl* mCheckboxPhysical;
    LLCheckBoxCtrl* mCheckboxTemporary;
    LLCheckBoxCtrl* mCheckboxLocked;
    LLCheckBoxCtrl* mCheckboxPhantom;
    LLCheckBoxCtrl* mCheckboxMoaP;
    LLCheckBoxCtrl* mCheckboxReflectionProbe;
    LLCheckBoxCtrl* mCheckboxDistance;
    LLSpinCtrl* mSpinDistanceMinValue;
    LLSpinCtrl* mSpinDistanceMaxValue;
    LLSpinCtrl* mSpinForSaleMinValue;
    LLSpinCtrl* mSpinForSaleMaxValue;
    LLButton* mButtonApply;
    LLComboBox* mComboClickAction;
    LLCheckBoxCtrl* mCheckboxAttachment;
    LLCheckBoxCtrl* mCheckboxExcludeAttachment;
    LLCheckBoxCtrl* mCheckboxExcludePhysics;
    LLCheckBoxCtrl* mCheckboxExcludetemporary;
    LLCheckBoxCtrl* mCheckboxExcludeReflectionProbes;
    LLCheckBoxCtrl* mCheckboxExcludeChildPrim;
    LLCheckBoxCtrl* mCheckboxExcludeNeighborRegions;
    LLCheckBoxCtrl* mCheckboxPermCopy;
    LLCheckBoxCtrl* mCheckboxPermModify;
    LLCheckBoxCtrl* mCheckboxPermTransfer;
    LLCheckBoxCtrl* mCheckboxAgentParcelOnly;
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

    FSAreaSearch* mFSAreaSearch;

    std::map<std::string, LLScrollListColumn::Params> mColumnParms;
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

    /*virtual*/ bool postBuild();

    LLCheckBoxCtrl* mCheckboxClickTouch;
    LLCheckBoxCtrl* mCheckboxClickBuy;
    LLCheckBoxCtrl* mCheckboxClickSit;
};

#endif // FS_AREASEARCH_H

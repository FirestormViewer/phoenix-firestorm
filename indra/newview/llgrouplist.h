/**
 * @file llgrouplist.h
 * @brief List of the groups the agent belongs to.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
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

#ifndef LL_LLGROUPLIST_H
#define LL_LLGROUPLIST_H

#include "llevent.h"
#include "llpointer.h"

#include "llflatlistview.h"
#include "llpanel.h"
#include "llstyle.h"
#include "lltoggleablemenu.h"

#include "llgroupmgr.h"

#include <boost/signals2.hpp> // <FS:PP> Group favorites / pinning

/**
 * Auto-updating list of agent groups.
 *
 * Can use optional group name filter.
 *
 * @see setNameFilter()
 */
class LLGroupList: public LLFlatListViewEx, public LLOldEvents::LLSimpleListener
{
    LOG_CLASS(LLGroupList);
public:
    struct Params : public LLInitParam::Block<Params, LLFlatListViewEx::Params>
    {
        Optional<bool> for_agent;
        Params();
    };

    LLGroupList(const Params& p);
    virtual ~LLGroupList();

    void enableForAgent(bool show_icons);

    virtual void draw(); // from LLView
    /*virtual*/ bool handleRightMouseDown(S32 x, S32 y, MASK mask); // from LLView
    /*virtual*/ bool handleDoubleClick(S32 x, S32 y, MASK mask); // from LLView

    void setNameFilter(const std::string& filter);
    void toggleIcons();
    bool getIconsVisible() const { return mShowIcons; }
    void setIconsVisible(bool show_icons) { mShowIcons = show_icons; }
    void setShowNone(bool show_none) { mShowNone = show_none; }
    void setGroups(const std::map< std::string,LLUUID> group_list);

    LLToggleableMenu* getContextMenu() const { return mContextMenuHandle.get(); }

    void refreshFavorites(); // <FS:PP> Group favorites / pinning

private:
    void setDirty(bool val = true)      { mDirty = val; }
    void refresh();
// <FS:PP> Group favorites / pinning
    // void addNewItem(const LLUUID& id, const std::string& name, const LLUUID& icon_id, EAddPosition pos = ADD_BOTTOM, bool visible_in_profile = true);
    void addNewItem(const LLUUID& id, const std::string& name, const LLUUID& icon_id, EAddPosition pos = ADD_BOTTOM, bool visible_in_profile = true, bool is_favorite = false);
    void addFavoritesSeparator();
    void onFavoritesChanged();
// </FS:PP>
    bool handleEvent(LLPointer<LLOldEvents::LLEvent> event, const LLSD& userdata); // called on agent group list changes

    bool onContextMenuItemClick(const LLSD& userdata);
    bool onContextMenuItemEnable(const LLSD& userdata);
    bool onContextMenuItemVisible(const LLSD& userdata); // <FS:PP> Group favorites / pinning

    LLHandle<LLToggleableMenu>  mContextMenuHandle;

    bool mShowIcons;
    bool mDirty;
    std::string mNameFilter;

    bool mForAgent;
    bool mShowNone;
    typedef std::map< std::string,LLUUID>   group_map_t;
    group_map_t             mGroups;

    boost::signals2::connection mFavoritesChangedConnection; // <FS:PP> Group favorites / pinning
};

class LLButton;
class LLGroupIconCtrl;
class LLTextBox;

// <FS:PP> Group favorites / pinning
class LLGroupListSeparator : public LLPanel
{
public:
    LLGroupListSeparator();
    /*virtual*/ bool postBuild();
    // Ignore all mouse interactions on the separator
    /*virtual*/ bool handleMouseDown(S32 x, S32 y, MASK mask) { return true; }
    /*virtual*/ bool handleMouseUp(S32 x, S32 y, MASK mask) { return true; }
    /*virtual*/ bool handleRightMouseDown(S32 x, S32 y, MASK mask) { return true; }
    /*virtual*/ bool handleRightMouseUp(S32 x, S32 y, MASK mask) { return true; }
    /*virtual*/ bool handleDoubleClick(S32 x, S32 y, MASK mask) { return true; }
};
// </FS:PP>

class LLGroupListItem : public LLPanel
    , public LLGroupMgrObserver
{
public:
    LLGroupListItem(bool for_agent, bool show_icons);
    ~LLGroupListItem();
    /*virtual*/ bool postBuild();
    /*virtual*/ void setValue(const LLSD& value);
    void onMouseEnter(S32 x, S32 y, MASK mask);
    void onMouseLeave(S32 x, S32 y, MASK mask);

    const LLUUID& getGroupID() const            { return mGroupID; }
    const std::string& getGroupName() const     { return mGroupName; }
    bool isFavorite() const                     { return mIsFavorite; } // <FS:PP> Group favorites / pinning

    void setName(const std::string& name, const std::string& highlight = LLStringUtil::null);
    void setGroupID(const LLUUID& group_id);
    void setGroupIconID(const LLUUID& group_icon_id);
    void setGroupIconVisible(bool visible);
    void setFavorite(bool favorite)             { mIsFavorite = favorite; } // <FS:PP> Group favorites / pinning

    virtual void changed(LLGroupChange gc);

    void setVisibleInProfile(bool visible);
private:
    void setBold(bool bold);
    void onInfoBtnClick();
    void onProfileBtnClick();
    void onNoticesBtnClick();
    void onVisibilityBtnClick(bool new_visibility);

    LLTextBox*  mGroupNameBox;
    LLUUID      mGroupID;
    LLGroupIconCtrl* mGroupIcon;
    LLButton*   mInfoBtn;
    LLButton*   mProfileBtn;
    LLButton*   mNoticesBtn;
    LLButton*   mVisibilityHideBtn;
    LLButton*   mVisibilityShowBtn;

    std::string mGroupName;
    bool        mForAgent;
    bool        mIsFavorite{ false }; // <FS:PP> Group favorites / pinning
    LLStyle::Params mGroupNameStyle;

    S32 mIconWidth;
};
#endif // LL_LLGROUPLIST_H

/**
 * @file fsfloatercamerabookmarks.h
 * @brief Saved camera views of the account, opened through camera links
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2026, The Phoenix Firestorm Project, Inc.
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
#ifndef FS_FLOATERCAMERABOOKMARKS_H
#define FS_FLOATERCAMERABOOKMARKS_H

#include "llfloater.h"
#include "llsingleton.h"
#include "fscameralinks.h"

#include <memory>

class LLComboBox;
class LLPanel;
class LLListContextMenu;
class LLScrollListCtrl;
class LLSpinCtrl;

// Camera bookmarks. Named camera views of the account, each kept as its camera link
// (FSCameraLinks), with camera tours made of them, in camera_bookmarks.xml of the account folder.

struct FSCameraBookmarkStop
{
    std::string             mName;
    std::string             mLink;
    F32                     mTravel = 4.f;
    F32                     mHold = 5.f;
    FSCameraLinks::TourMove mMove = FSCameraLinks::TourMove::NONE;
    F32                     mAmount = 0.f;
    FSCameraLinks::TourEase mEase = FSCameraLinks::TourEase::SMOOTH;
};

struct FSCameraBookmarkTour
{
    std::string                         mName;
    FSCameraLinks::TourOptions          mOptions;
    std::vector<FSCameraBookmarkStop>   mStops;
};

// The bookmarks and tours of an account
class FSCameraBookmarkStore : public LLSingleton<FSCameraBookmarkStore>
{
    LLSINGLETON(FSCameraBookmarkStore);
    ~FSCameraBookmarkStore();
    void initSingleton() override;

public:
    LLSD                        mBookmarks;         // array of maps: name, link, target
    std::vector<FSCameraBookmarkTour>   mTours;
    S32                         mCurrentTour = -1;  // the tour shown in the UI
    S32                         mPlayingTour = -1;  // the tour in action
    std::vector<S32>            mPlayingStops;      // the stops of the playing tour

    void save() const;
    bool hasLink(const std::string& link) const;
    S32 addBookmark(const std::string& name, const std::string& link, const std::string& target_name);
    S32 findBookmark(const std::string& name) const;
    S32 findTour(const std::string& name) const;
    bool playTour(S32 tour, S32 from_stop);
    boost::signals2::connection setChangedCallback(const std::function<void()>& cb);

private:
    void load();

    boost::signals2::signal<void()> mChanged;
    boost::signals2::connection     mTourConnection;
};

class FSFloaterCameraBookmarks : public LLFloater
{
public:
    FSFloaterCameraBookmarks(const LLSD& key);
    ~FSFloaterCameraBookmarks() override;

    bool postBuild() override;
    void onClose(bool app_quitting) override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

    // the bookmark list's right click menu
    void onBookmarkMenu(const std::string& action);
    bool onBookmarkMenuEnable(const std::string& action);

    typedef FSCameraBookmarkStop Stop;
    typedef FSCameraBookmarkTour Tour;

private:
    void save() const;
    bool hasLink(const std::string& link) const;
    void onStoreChanged();

    // Bookmarks
    void refreshList(S32 select_index = -1);    // the bookmark to select, -1 to keep the selection
    void updateButtons();
    S32 selectedIndex() const;                  // the one bookmark selected, else -1
    std::vector<S32> selectedIndices() const;   // in list order
    void onClickAdd();
    void onClickAddFollowing();
    void onClickPaste();
    void onClickGo();
    void onClickCopy();
    void onClickRename();
    void onClickUpdateView();
    void onClickDelete();
    void onClickMove(S32 offset);
    void addBookmark(std::string name, const std::string& link, const std::string& target_name = std::string());
    // Asks for the name of a new bookmark of the link, the name given offered
    void askBookmarkName(const std::string& name, const std::string& link, const std::string& target_name = std::string());
    void renameBookmark(S32 index, std::string name);
    void deleteBookmarks(const std::vector<S32>& indices);
    void updateBookmarkView(S32 index, const std::string& link, const std::string& target_name);

    // Tours
    void setToursVisible(bool visible, bool remember = true);
    void setCompact(bool compact);
    void layoutPanels();                        // the bookmarks, then the tours at their fixed width
    Tour* currentTour();
    const Tour* currentTour() const;
    Tour* currentOrNewTour();                   // the tour shown, made first when there is none
    static Stop newStop(const std::string& name, const std::string& link, const Tour& tour); // added at the end of the tour
    void refreshTours();
    void refreshStops(const std::vector<S32>& select = {});
    void refreshStopEditor();
    void refreshTourOptions();
    std::vector<S32> selectedStops() const;     // in list order
    std::string moveLabel(const Stop& stop) const;
    std::string durationText(const Tour& tour) const;
    void markPlayingStop();                     // the stop playing is shown in bold, without touching the selection
    void onSelectTour();
    void onClickNewTour();
    void onClickRenameTour();
    void onClickDeleteTour();
    void onClickAddStops();
    void onClickAddCurrentStop();
    void onClickRemoveStops();
    void onClickMoveStop(S32 offset);
    void onClickGoStop();
    void onCommitStopEditor(const std::string& field);
    void onCommitTourOptions();
    void onClickPlayTour();
    void onClickCopyTour();
    void onClickPasteTour();
    void updateTourStatus();
    void stopTourIfPlaying(S32 tour);           // the tour's stops are changing mid-play, cancel instead of doing bogus stuff.

    // the store's info.
    LLSD&               mBookmarks;
    std::vector<Tour>&  mTours;
    S32&                mCurrentTour;
    S32&                mPlayingTour;
    std::vector<S32>&   mPlayingStops;

    LLScrollListCtrl*   mList = nullptr;
    LLUICtrl*           mFilter = nullptr;
    LLPanel*            mBookmarksPanel = nullptr;
    LLPanel*            mToursPanel = nullptr;
    bool                mCompact = false;
    bool                mToursBeforeCompact = false;
    S32                 mToursWidth = 0;
    S32                 mButtonRowsHeight = 0;
    S32                 mCompactToggleGap = 0;
    S32                 mCompactToggleGapCompact = 0;
    LLComboBox*         mTourCombo = nullptr;
    LLScrollListCtrl*   mStopList = nullptr;
    LLSpinCtrl*         mStopTravel = nullptr;
    LLSpinCtrl*         mStopHold = nullptr;
    LLComboBox*         mStopEase = nullptr;
    LLComboBox*         mStopMove = nullptr;
    LLSpinCtrl*         mStopAmount = nullptr;
    LLComboBox*         mTourRepeat = nullptr;
    LLComboBox*         mTourPath = nullptr;
    LLSpinCtrl*         mTourSpeed = nullptr;
    LLUICtrl*           mTourHideUI = nullptr;
    std::unique_ptr<LLListContextMenu> mBookmarkMenu;
    boost::signals2::connection mTourConnection;
    boost::signals2::connection mStoreConnection;
};

#endif // FS_FLOATERCAMERABOOKMARKS_H

/**
 * @file fsfloatercamerabookmarks.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsfloatercamerabookmarks.h"

#include "llbutton.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "llfontgl.h"
#include "lldir.h"
#include "llfile.h"
#include "llpanel.h"
#include "lllistcontextmenu.h"
#include "llnotificationsutil.h"
#include "llscrolllistcell.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llsdserialize.h"
#include "llspinctrl.h"
#include "lltrans.h"
#include "llviewercontrol.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr size_t MAX_NAME_LENGTH = 64;
    const std::string BOOKMARKS_FILE = "camera_bookmarks.xml";

    std::string bookmarks_filename()
    {
        return gDirUtilp->getExpandedFilename(LL_PATH_PER_SL_ACCOUNT, BOOKMARKS_FILE);
    }

    std::string clean_name(std::string name)
    {
        LLStringUtil::trim(name);
        if (name.size() > MAX_NAME_LENGTH)
        {
            name = utf8str_truncate(name, (S32)MAX_NAME_LENGTH);
            LLStringUtil::trim(name);
        }
        return name;
    }

    struct PastedLink
    {
        std::string mLabel;
        std::string mLink;
    };

    // The camera links in pasted text
    // "Main stage: secondlife:///app/camera/..." or "[secondlife:///app/camera/... Main stage]"
    std::vector<PastedLink> find_links(const std::string& text)
    {
        static const std::string PREFIX = "secondlife:///app/camera/";
        static const std::string LINK_END = " \t\r\n]\"'<>";
        static const std::string LABEL_TRIM = " \t:-|=>*";

        std::vector<PastedLink> links;
        size_t previous_end = 0;
        size_t start = text.find(PREFIX);
        while (start != std::string::npos)
        {
            size_t end = text.find_first_of(LINK_END, start);
            if (end == std::string::npos)
            {
                end = text.size();
            }

            PastedLink found;
            found.mLink = text.substr(start, end - start);

            if (start > 0 && text[start - 1] == '[' && end < text.size() && text[end] != ']')
            {
                // [link label]
                const size_t label_end = text.find_first_of("]\r\n", end);
                found.mLabel = text.substr(end, (label_end == std::string::npos ? text.size() : label_end) - end);
                if (label_end != std::string::npos && text[label_end] == ']')
                {
                    end = label_end + 1;
                }
            }
            else
            {
                // the line's text before the link, after any link before it on that line
                size_t line_start = text.find_last_of("\r\n", start);
                line_start = (line_start == std::string::npos) ? 0 : line_start + 1;
                line_start = llmax(line_start, previous_end);
                size_t label_end = start;
                if (label_end > line_start && text[label_end - 1] == '[')
                {
                    --label_end;
                }
                found.mLabel = text.substr(line_start, label_end - line_start);
            }

            // trims the punctuation that leads to a link, as in "Stage view: "
            const size_t first = found.mLabel.find_first_not_of(LABEL_TRIM);
            const size_t last = found.mLabel.find_last_not_of(LABEL_TRIM);
            found.mLabel = (first == std::string::npos) ? std::string() : found.mLabel.substr(first, last - first + 1);

            links.push_back(found);
            previous_end = end;
            start = text.find(PREFIX, end);
        }
        return links;
    }

    constexpr S32 BOOKMARKS_MIN_WIDTH = 370;
    // Compact mode
    constexpr S32 LIST_BOTTOM_COMPACT = 4;
    constexpr S32 COMPACT_MIN_WIDTH = 220;
    constexpr S32 COMPACT_MIN_HEIGHT = 160;
    constexpr S32 FULL_MIN_HEIGHT = 400;
    // Full mode
    const char* const BOOKMARK_BUTTONS[] = { "add_following", "follow_turns", "add", "paste", "go", "copy", "rename",
                                             "update_view", "delete", "move_up", "move_down", "tours_toggle" };
    const std::string TOUR_HEADER = "Camera tour:";

    // Stop settings include: fly=4 hold=5 ease=linear move=orbit_left amount=30 (any order, all optional)
    void parse_stop_settings(const std::string& line, FSCameraBookmarkStop& stop)
    {
        bool has_amount = false;
        std::vector<std::string> tokens;
        LLStringUtil::getTokens(line, tokens, " \t");
        for (const std::string& token : tokens)
        {
            const size_t equals = token.find('=');
            if (equals == std::string::npos)
            {
                continue;
            }
            std::string key = token.substr(0, equals);
            LLStringUtil::toLower(key);
            const std::string value = token.substr(equals + 1);
            F32 number = 0.f;
            if (key == "fly" && FSCameraLinks::parseNumber(value, number))
            {
                stop.mTravel = llclamp(number, 0.f, FSCameraLinks::TOUR_MAX_TRAVEL);
            }
            else if (key == "hold" && FSCameraLinks::parseNumber(value, number))
            {
                stop.mHold = llclamp(number, 0.f, FSCameraLinks::TOUR_MAX_HOLD);
            }
            else if (key == "amount" && FSCameraLinks::parseNumber(value, number))
            {
                stop.mAmount = llclamp(number, 0.f, FSCameraLinks::TOUR_MAX_AMOUNT);
                has_amount = true;
            }
            else if (key == "move")
            {
                FSCameraLinks::TourMove move;
                std::string name = value;
                LLStringUtil::toLower(name);
                if (FSCameraLinks::parseTourMove(name, move))
                {
                    stop.mMove = move;
                }
            }
            else if (key == "ease")
            {
                FSCameraLinks::TourEase ease;
                std::string name = value;
                LLStringUtil::toLower(name);
                if (FSCameraLinks::parseTourEase(name, ease))
                {
                    stop.mEase = ease;
                }
            }
        }
        if (!has_amount)
        {
            stop.mAmount = FSCameraLinks::defaultTourMoveAmount(stop.mMove);
        }
    }

    // Tour options: repeat=loop path=curved speed=1.5 hide_ui=1
    void parse_tour_options(const std::string& text, FSCameraLinks::TourOptions& options)
    {
        std::vector<std::string> tokens;
        LLStringUtil::getTokens(text, tokens, " \t");
        for (const std::string& token : tokens)
        {
            const size_t equals = token.find('=');
            if (equals == std::string::npos)
            {
                continue;
            }
            std::string key = token.substr(0, equals);
            std::string value = token.substr(equals + 1);
            LLStringUtil::toLower(key);
            LLStringUtil::toLower(value);
            F32 number = 0.f;
            if (key == "repeat")
            {
                FSCameraLinks::parseTourRepeat(value, options.mRepeat);
            }
            else if (key == "path")
            {
                options.mCurved = value == "curved";
            }
            else if (key == "speed" && FSCameraLinks::parseNumber(value, number))
            {
                options.mSpeed = llclamp(number, FSCameraLinks::TOUR_MIN_SPEED, FSCameraLinks::TOUR_MAX_SPEED);
            }
            else if (key == "hide_ui")
            {
                options.mHideUI = value == "1";
            }
        }
    }

    LLSD stop_to_llsd(const FSCameraBookmarkStop& stop)
    {
        LLSD data;
        data["name"] = stop.mName;
        data["link"] = stop.mLink;
        data["fly"] = stop.mTravel;
        data["hold"] = stop.mHold;
        data["move"] = FSCameraLinks::tourMoveName(stop.mMove);
        data["amount"] = stop.mAmount;
        data["ease"] = FSCameraLinks::tourEaseName(stop.mEase);
        return data;
    }

    bool stop_from_llsd(const LLSD& data, FSCameraBookmarkStop& stop)
    {
        if (!data.isMap() || !data.has("name") || !data.has("link"))
        {
            return false;
        }
        stop.mName = data["name"].asString();
        stop.mLink = data["link"].asString();
        if (data.has("fly"))
        {
            stop.mTravel = llclamp((F32)data["fly"].asReal(), 0.f, FSCameraLinks::TOUR_MAX_TRAVEL);
        }
        if (data.has("hold"))
        {
            stop.mHold = llclamp((F32)data["hold"].asReal(), 0.f, FSCameraLinks::TOUR_MAX_HOLD);
        }
        if (!FSCameraLinks::parseTourMove(data["move"].asString(), stop.mMove))
        {
            stop.mMove = FSCameraLinks::TourMove::NONE;
        }
        stop.mAmount = data.has("amount") ? llclamp((F32)data["amount"].asReal(), 0.f, FSCameraLinks::TOUR_MAX_AMOUNT)
                                          : FSCameraLinks::defaultTourMoveAmount(stop.mMove);
        if (!FSCameraLinks::parseTourEase(data["ease"].asString(), stop.mEase))
        {
            stop.mEase = FSCameraLinks::TourEase::SMOOTH;
        }
        return true;
    }

    // The bookmark list's right click menu
    class FSCameraBookmarkMenu : public LLListContextMenu
    {
    public:
        explicit FSCameraBookmarkMenu(const LLHandle<FSFloaterCameraBookmarks>& floater) : mFloater(floater) {}

    protected:
        LLContextMenu* createMenu() override
        {
            LLUICtrl::CommitCallbackRegistry::ScopedRegistrar registrar;
            LLUICtrl::EnableCallbackRegistry::ScopedRegistrar enable_registrar;
            const LLHandle<FSFloaterCameraBookmarks> floater = mFloater;
            registrar.add("CameraBookmark.Action", [floater](LLUICtrl*, const LLSD& param)
            {
                if (FSFloaterCameraBookmarks* instance = floater.get())
                {
                    instance->onBookmarkMenu(param.asString());
                }
            });
            enable_registrar.add("CameraBookmark.Enable", [floater](LLUICtrl*, const LLSD& param)
            {
                FSFloaterCameraBookmarks* instance = floater.get();
                return instance && instance->onBookmarkMenuEnable(param.asString());
            });
            return createFromFile("menu_fs_camera_bookmarks.xml");
        }

    private:
        LLHandle<FSFloaterCameraBookmarks> mFloater;
    };
}


///////////////////////////////////////////////////////////////////////////////
// FSCameraBookmarkStore

FSCameraBookmarkStore::FSCameraBookmarkStore()
    : mBookmarks(LLSD::emptyArray())
{
}

FSCameraBookmarkStore::~FSCameraBookmarkStore()
{
    mTourConnection.disconnect();
}

void FSCameraBookmarkStore::initSingleton()
{
    load();
    mTourConnection = FSCameraLinks::setTourChangedCallback([this]()
    {
        if (!FSCameraLinks::isTourPlaying())
        {
            mPlayingTour = -1;
        }
    });
}

void FSCameraBookmarkStore::load()
{
    mBookmarks = LLSD::emptyArray();
    mTours.clear();
    mCurrentTour = -1;

    const std::string filename = bookmarks_filename();
    if (!LLFile::isfile(filename))
    {
        return;
    }
    llifstream file(filename.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "Couldn't open camera bookmarks file " << filename << LL_ENDL;
        return;
    }
    LLSD data;
    LLSDSerialize::fromXMLDocument(data, file);
    file.close();

    const LLSD bookmarks = data.get("bookmarks");
    // Drop unusable entries. (Version mismatch or manual editing error)
    for (size_t i = 0; bookmarks.isArray() && i < bookmarks.size(); ++i)
    {
        const LLSD& entry = bookmarks[i];
        if (entry.isMap() && entry.has("name") && entry.has("link"))
        {
            LLSD bookmark;
            bookmark["name"] = entry["name"].asString();
            bookmark["link"] = entry["link"].asString();
            if (entry.has("target"))
            {
                bookmark["target"] = entry["target"].asString();
            }
            mBookmarks.append(bookmark);
        }
    }

    const LLSD tours = data.get("tours");
    for (size_t i = 0; tours.isArray() && i < tours.size(); ++i)
    {
        const LLSD& entry = tours[i];
        if (!entry.isMap())
        {
            continue;
        }
        FSCameraBookmarkTour tour;
        tour.mName = clean_name(entry["name"].asString());
        if (tour.mName.empty())
        {
            tour.mName = LLTrans::getString("FSCameraTourUnnamed");
        }
        FSCameraLinks::parseTourRepeat(entry["repeat"].asString(), tour.mOptions.mRepeat);
        tour.mOptions.mCurved = entry["curved"].asBoolean();
        tour.mOptions.mSpeed = entry.has("speed") ? llclamp((F32)entry["speed"].asReal(), FSCameraLinks::TOUR_MIN_SPEED, FSCameraLinks::TOUR_MAX_SPEED) : 1.f;
        tour.mOptions.mHideUI = entry["hide_ui"].asBoolean();
        const LLSD& stops = entry["stops"];
        for (size_t j = 0; stops.isArray() && j < stops.size(); ++j)
        {
            FSCameraBookmarkStop stop;
            if (stop_from_llsd(stops[j], stop))
            {
                tour.mStops.push_back(stop);
            }
        }
        mTours.push_back(tour);
    }
    if (!mTours.empty())
    {
        mCurrentTour = llclamp((S32)data.get("current_tour").asInteger(), 0, (S32)mTours.size() - 1);
    }
}

void FSCameraBookmarkStore::save() const
{
    LLSD data;
    data["bookmarks"] = mBookmarks;
    LLSD& tours = data["tours"] = LLSD::emptyArray();
    for (const FSCameraBookmarkTour& tour : mTours)
    {
        LLSD entry;
        entry["name"] = tour.mName;
        entry["repeat"] = FSCameraLinks::tourRepeatName(tour.mOptions.mRepeat);
        entry["curved"] = tour.mOptions.mCurved;
        entry["speed"] = tour.mOptions.mSpeed;
        entry["hide_ui"] = tour.mOptions.mHideUI;
        LLSD& stops = entry["stops"] = LLSD::emptyArray();
        for (const FSCameraBookmarkStop& stop : tour.mStops)
        {
            stops.append(stop_to_llsd(stop));
        }
        tours.append(entry);
    }
    data["current_tour"] = mCurrentTour;

    const std::string filename = bookmarks_filename();
    llofstream file(filename.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "Couldn't save camera bookmarks file " << filename << LL_ENDL;
        return;
    }
    LLSDSerialize::toPrettyXML(data, file);
    file.close();
}

bool FSCameraBookmarkStore::hasLink(const std::string& link) const
{
    for (size_t i = 0; i < mBookmarks.size(); ++i)
    {
        if (mBookmarks[i]["link"].asString() == link)
        {
            return true;
        }
    }
    return false;
}

S32 FSCameraBookmarkStore::addBookmark(const std::string& name, const std::string& link, const std::string& target_name)
{
    LLSD bookmark;
    bookmark["name"] = clean_name(name);
    bookmark["link"] = link;
    if (!target_name.empty())
    {
        bookmark["target"] = target_name;
    }
    mBookmarks.append(bookmark);
    save();
    mChanged();
    return (S32)mBookmarks.size() - 1;
}

namespace
{
    template <typename NAME_AT>
    S32 find_by_name(const std::string& wanted, S32 count, NAME_AT name_at)
    {
        std::string lower_wanted = wanted;
        LLStringUtil::trim(lower_wanted);
        LLStringUtil::toLower(lower_wanted);
        if (lower_wanted.empty())
        {
            return -1;
        }
        S32 partial = -1;
        for (S32 i = 0; i < count; ++i)
        {
            std::string name = name_at(i);
            LLStringUtil::toLower(name);
            if (name == lower_wanted)
            {
                return i;
            }
            if (partial < 0 && name.find(lower_wanted) != std::string::npos)
            {
                partial = i;
            }
        }
        return partial;
    }
}

S32 FSCameraBookmarkStore::findBookmark(const std::string& name) const
{
    return find_by_name(name, (S32)mBookmarks.size(), [this](S32 i) { return mBookmarks[i]["name"].asString(); });
}

S32 FSCameraBookmarkStore::findTour(const std::string& name) const
{
    return find_by_name(name, (S32)mTours.size(), [this](S32 i) { return mTours[i].mName; });
}

bool FSCameraBookmarkStore::playTour(S32 tour_index, S32 from_stop)
{
    if (tour_index < 0 || tour_index >= (S32)mTours.size())
    {
        return false;
    }
    const FSCameraBookmarkTour& tour = mTours[tour_index];

    // In order, minus invalid links
    std::vector<FSCameraLinks::TourStop> stops;
    std::vector<S32> rows;
    size_t first = 0;
    for (S32 i = 0; i < (S32)tour.mStops.size(); ++i)
    {
        const FSCameraBookmarkStop& stop = tour.mStops[i];
        FSCameraLinks::TourStop tour_stop;
        if (!FSCameraLinks::parseLink(stop.mLink, tour_stop.mView))
        {
            continue;
        }
        tour_stop.mTravel = stop.mTravel;
        tour_stop.mHold = stop.mHold;
        tour_stop.mMove = stop.mMove;
        tour_stop.mAmount = stop.mAmount;
        tour_stop.mEase = stop.mEase;
        if (i < from_stop)
        {
            first = stops.size() + 1;
        }
        stops.push_back(tour_stop);
        rows.push_back(i);
    }
    if (stops.empty())
    {
        LLNotificationsUtil::add("FSCameraLinkInvalid");
        return false;
    }
    first = llmin(first, stops.size() - 1);

    FSCameraLinks::stopTour();
    mPlayingStops = rows;
    mPlayingTour = tour_index;
    if (!FSCameraLinks::startTour(stops, first, tour.mOptions))
    {
        mPlayingTour = -1;
        return false;
    }
    return true;
}

boost::signals2::connection FSCameraBookmarkStore::setChangedCallback(const std::function<void()>& cb)
{
    return mChanged.connect(cb);
}

///////////////////////////////////////////////////////////////////////////////
// FSFloaterCameraBookmarks

FSFloaterCameraBookmarks::FSFloaterCameraBookmarks(const LLSD& key)
    : LLFloater(key),
      mBookmarks(FSCameraBookmarkStore::instance().mBookmarks),
      mTours(FSCameraBookmarkStore::instance().mTours),
      mCurrentTour(FSCameraBookmarkStore::instance().mCurrentTour),
      mPlayingTour(FSCameraBookmarkStore::instance().mPlayingTour),
      mPlayingStops(FSCameraBookmarkStore::instance().mPlayingStops)
{
}

FSFloaterCameraBookmarks::~FSFloaterCameraBookmarks()
{
    mTourConnection.disconnect();
    mStoreConnection.disconnect();
}

bool FSFloaterCameraBookmarks::postBuild()
{
    // Bookmarks
    mList = getChild<LLScrollListCtrl>("bookmarks");
    mList->setCommitOnSelectionChange(true);
    mList->setCommitCallback([this](LLUICtrl*, const LLSD&) { updateButtons(); });
    mList->setDoubleClickCallback([this]() { onClickGo(); });
    mBookmarkMenu = std::make_unique<FSCameraBookmarkMenu>(getDerivedHandle<FSFloaterCameraBookmarks>());
    mList->setRightMouseDownCallback([this](LLUICtrl*, S32 x, S32 y, MASK)
    {
        LLScrollListItem* item = mList->hitItem(x, y);
        if (!item)
        {
            return;
        }
        if (!item->getSelected())
        {
            mList->deselectAllItems(true);
            mList->setSelectedByValue(item->getValue(), true);
            updateButtons();
        }
        mBookmarkMenu->show(mList, uuid_vec_t{ LLUUID::null }, x, y);
    });
    mFilter = getChild<LLUICtrl>("filter");
    mFilter->setCommitCallback([this](LLUICtrl*, const LLSD&) { refreshList(); });

    getChild<LLUICtrl>("add")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAdd(); });
    getChild<LLUICtrl>("add_following")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAddFollowing(); });
    getChild<LLUICtrl>("paste")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPaste(); });
    getChild<LLUICtrl>("go")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGo(); });
    getChild<LLUICtrl>("copy")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCopy(); });
    getChild<LLUICtrl>("rename")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRename(); });
    getChild<LLUICtrl>("update_view")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickUpdateView(); });
    getChild<LLUICtrl>("delete")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDelete(); });
    getChild<LLUICtrl>("move_up")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickMove(-1); });
    getChild<LLUICtrl>("move_down")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickMove(1); });
    getChild<LLUICtrl>("compact_toggle")->setCommitCallback([this](LLUICtrl*, const LLSD&) { setCompact(!mCompact); });
    getChild<LLUICtrl>("tours_toggle")->setCommitCallback([this](LLUICtrl*, const LLSD&)
    {
        setToursVisible(!mToursPanel->getVisible());
    });

    // Tours
    mBookmarksPanel = getChild<LLPanel>("bookmarks_panel");
    mToursPanel = getChild<LLPanel>("tours_panel");
    mToursWidth = mToursPanel->getRect().getWidth();
    mButtonRowsHeight = mList->getRect().mBottom - LIST_BOTTOM_COMPACT;
    const S32 panel_width = mBookmarksPanel->getRect().getWidth();
    mCompactToggleGap = panel_width - getChild<LLView>("compact_toggle")->getRect().mRight;
    mCompactToggleGapCompact = panel_width - getChild<LLView>("tours_toggle")->getRect().mRight;
    mTourCombo = getChild<LLComboBox>("tour_combo");
    mTourCombo->setCommitCallback([this](LLUICtrl*, const LLSD&) { onSelectTour(); });
    mStopList = getChild<LLScrollListCtrl>("stops");
    mStopList->setCommitOnSelectionChange(true);
    mStopList->setCommitCallback([this](LLUICtrl*, const LLSD&) { refreshStopEditor(); updateButtons(); });
    mStopList->setDoubleClickCallback([this]() { onClickGoStop(); });
    mStopTravel = getChild<LLSpinCtrl>("stop_travel");
    mStopHold = getChild<LLSpinCtrl>("stop_hold");
    mStopEase = getChild<LLComboBox>("stop_ease");
    mStopMove = getChild<LLComboBox>("stop_move");
    mStopAmount = getChild<LLSpinCtrl>("stop_amount");
    mTourRepeat = getChild<LLComboBox>("tour_repeat");
    mTourPath = getChild<LLComboBox>("tour_path");
    mTourSpeed = getChild<LLSpinCtrl>("tour_speed");
    mTourHideUI = getChild<LLUICtrl>("tour_hide_ui");
    mStopTravel->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitStopEditor("travel"); });
    mStopHold->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitStopEditor("hold"); });
    mStopEase->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitStopEditor("ease"); });
    mStopMove->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitStopEditor("move"); });
    mTourRepeat->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitTourOptions(); });
    mTourPath->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitTourOptions(); });
    mTourSpeed->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitTourOptions(); });
    mTourHideUI->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitTourOptions(); });
    mStopAmount->setCommitCallback([this](LLUICtrl*, const LLSD&) { onCommitStopEditor("amount"); });

    getChild<LLUICtrl>("new_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickNewTour(); });
    getChild<LLUICtrl>("rename_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRenameTour(); });
    getChild<LLUICtrl>("delete_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickDeleteTour(); });
    getChild<LLUICtrl>("add_stops")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAddStops(); });
    getChild<LLUICtrl>("add_current_stop")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickAddCurrentStop(); });
    getChild<LLUICtrl>("remove_stops")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickRemoveStops(); });
    getChild<LLUICtrl>("stop_up")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickMoveStop(-1); });
    getChild<LLUICtrl>("stop_down")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickMoveStop(1); });
    getChild<LLUICtrl>("go_stop")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickGoStop(); });
    getChild<LLUICtrl>("play_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPlayTour(); });
    getChild<LLUICtrl>("copy_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickCopyTour(); });
    getChild<LLUICtrl>("paste_tour")->setCommitCallback([this](LLUICtrl*, const LLSD&) { onClickPasteTour(); });
    mTourConnection = FSCameraLinks::setTourChangedCallback([this]() { updateTourStatus(); });
    // bookmarks added by the chat command
    mStoreConnection = FSCameraBookmarkStore::instance().setChangedCallback([this]() { onStoreChanged(); });

    refreshList();
    refreshTours();
    updateTourStatus();
    setToursVisible(gSavedSettings.getBOOL("FSCameraBookmarksShowTours"));
    setCompact(gSavedSettings.getBOOL("FSCameraBookmarksCompact"));
    return true;
}

void FSFloaterCameraBookmarks::onClose(bool app_quitting)
{
    // Closing the window ends a tour or a target followed, camera stays where the floater was closed.
    if (!app_quitting)
    {
        FSCameraLinks::releaseCamera();
    }
    LLFloater::onClose(app_quitting);
}

///////////////////////////////////////////////////////////////////////////////
// Saving

void FSFloaterCameraBookmarks::save() const
{
    FSCameraBookmarkStore::instance().save();
}

bool FSFloaterCameraBookmarks::hasLink(const std::string& link) const
{
    return FSCameraBookmarkStore::instance().hasLink(link);
}

void FSFloaterCameraBookmarks::onStoreChanged()
{
    refreshList();
}

///////////////////////////////////////////////////////////////////////////////
// Bookmarks

void FSFloaterCameraBookmarks::refreshList(S32 select_index)
{
    std::vector<S32> select = select_index >= 0 ? std::vector<S32>{ select_index } : selectedIndices();
    std::string filter = mFilter->getValue().asString();
    LLStringUtil::trim(filter);
    LLStringUtil::toLower(filter);

    const S32 scroll = mList->getScrollPos();
    mList->deleteAllItems();
    for (S32 i = 0; i < (S32)mBookmarks.size(); ++i)
    {
        const LLSD& bookmark = mBookmarks[i];
        FSCameraLinks::View view;
        const bool valid = FSCameraLinks::parseLink(bookmark["link"].asString(), view);
        const std::string name = bookmark["name"].asString();
        std::string region = valid ? view.mRegion : getString("invalid_link");
        if (valid && view.hasTarget())
        {
            const std::string target = bookmark["target"].asString();
            LLStringUtil::format_map_t args;
            args["NAME"] = target;
            region = getString(target.empty() ? "following_unnamed" : "following", args);
        }
        if (!filter.empty())
        {
            std::string haystack = name + "\n" + region;
            LLStringUtil::toLower(haystack);
            if (haystack.find(filter) == std::string::npos)
            {
                continue;
            }
        }

        LLSD row;
        row["value"] = i;
        LLSD& columns = row["columns"];
        columns[0]["column"] = "name";
        columns[0]["value"] = name;
        columns[1]["column"] = "region";
        columns[1]["value"] = region;
        mList->addElement(row);
    }

    mList->setScrollPos(scroll);
    for (S32 index : select)
    {
        mList->setSelectedByValue(LLSD(index), true);
    }
    if (select_index >= 0)
    {
        mList->scrollToShowSelected();
    }
    updateButtons();
}

void FSFloaterCameraBookmarks::updateButtons()
{
    // Bookmarks
    const std::vector<S32> selected = selectedIndices();
    const bool one = selected.size() == 1;
    const bool any = !selected.empty();
    std::string filter = mFilter->getValue().asString();
    LLStringUtil::trim(filter);
    const bool filtered = !filter.empty();
    const S32 count = (S32)mBookmarks.size();
    childSetEnabled("go", one);
    childSetEnabled("copy", any);
    childSetEnabled("rename", one);
    childSetEnabled("update_view", one);
    childSetEnabled("delete", any);
    childSetEnabled("move_up", one && !filtered && selected[0] > 0);
    childSetEnabled("move_down", one && !filtered && selected[0] < count - 1);

    // Tours
    const Tour* tour = currentTour();
    const bool has_stops = tour && !tour->mStops.empty();
    const std::vector<S32> stops = selectedStops();
    const S32 stop_count = tour ? (S32)tour->mStops.size() : 0;
    childSetEnabled("rename_tour", tour != nullptr);
    childSetEnabled("delete_tour", tour != nullptr);
    mTourRepeat->setEnabled(tour != nullptr);
    mTourPath->setEnabled(tour != nullptr);
    mTourSpeed->setEnabled(tour != nullptr);
    mTourHideUI->setEnabled(tour != nullptr);
    childSetEnabled("add_stops", any);
    childSetEnabled("remove_stops", !stops.empty());
    childSetEnabled("stop_up", !stops.empty() && stops.front() > 0);
    childSetEnabled("stop_down", !stops.empty() && stops.back() < stop_count - 1);
    childSetEnabled("go_stop", stops.size() == 1);
    childSetEnabled("play_tour", has_stops || FSCameraLinks::isTourPlaying());
    childSetEnabled("copy_tour", has_stops);
}

S32 FSFloaterCameraBookmarks::selectedIndex() const
{
    const std::vector<S32> selected = selectedIndices();
    return selected.size() == 1 ? selected[0] : -1;
}

std::vector<S32> FSFloaterCameraBookmarks::selectedIndices() const
{
    std::vector<S32> indices;
    if (!mList)
    {
        return indices;
    }
    for (const LLScrollListItem* item : mList->getAllSelected())
    {
        const S32 index = item->getValue().asInteger();
        if (index >= 0 && index < (S32)mBookmarks.size())
        {
            indices.push_back(index);
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void FSFloaterCameraBookmarks::onClickAdd()
{
    FSCameraLinks::View view;
    std::string link;
    if (!FSCameraLinks::getCurrentView(view) || !FSCameraLinks::buildCheckedLink(view, link))
    {
        return;
    }

    askBookmarkName(view.mRegion, link);
}

void FSFloaterCameraBookmarks::askBookmarkName(const std::string& name, const std::string& link, const std::string& target_name)
{
    LLSD args;
    args["NAME"] = clean_name(name);
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraBookmarkName", args, LLSD(),
        [handle, link, target_name](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (self && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                self->addBookmark(response["message"].asString(), link, target_name);
            }
        });
}

void FSFloaterCameraBookmarks::onClickPaste()
{
    LLWString wide_text;
    if (!LLClipboard::instance().isTextAvailable() || !LLClipboard::instance().pasteFromClipboard(wide_text))
    {
        LLNotificationsUtil::add("FSCameraBookmarkPasteNone");
        return;
    }
    const std::vector<PastedLink> links = find_links(wstring_to_utf8str(wide_text));

    // One link
    if (links.size() == 1)
    {
        FSCameraLinks::View view;
        if (!FSCameraLinks::parseLink(links[0].mLink, view))
        {
            LLNotificationsUtil::add("FSCameraBookmarkPasteNone");
            return;
        }
        // Saved in the form this viewer writes, so the same view pasted twice is seen as one
        const std::string link = FSCameraLinks::buildLink(view);
        if (hasLink(link))
        {
            LLNotificationsUtil::add("FSCameraBookmarkAlreadySaved");
            return;
        }
        askBookmarkName(links[0].mLabel.empty() ? view.mRegion : links[0].mLabel, link);
        return;
    }

    // A list of links
    S32 added = 0;
    S32 skipped = 0;
    S32 invalid = 0;
    for (const PastedLink& pasted : links)
    {
        FSCameraLinks::View view;
        if (!FSCameraLinks::parseLink(pasted.mLink, view))
        {
            ++invalid;
            continue;
        }
        const std::string link = FSCameraLinks::buildLink(view);
        if (hasLink(link))
        {
            ++skipped;
            continue;
        }
        std::string name = clean_name(pasted.mLabel);
        if (name.empty())
        {
            name = clean_name(view.mRegion);
        }
        LLSD bookmark;
        bookmark["name"] = name;
        bookmark["link"] = link;
        mBookmarks.append(bookmark);
        ++added;
    }

    if (added == 0 && skipped == 0)
    {
        LLNotificationsUtil::add("FSCameraBookmarkPasteNone");
        return;
    }
    if (added > 0)
    {
        save();
        // the new bookmarks are shown, regardless of filter state
        mFilter->setValue(LLSD(std::string()));
        refreshList((S32)mBookmarks.size() - 1);
    }
    LLSD args;
    args["ADDED"] = added;
    args["SKIPPED"] = skipped;
    args["INVALID"] = invalid;
    LLNotificationsUtil::add("FSCameraBookmarksPasted", args);
}

void FSFloaterCameraBookmarks::addBookmark(std::string name, const std::string& link, const std::string& target_name)
{
    name = clean_name(name);
    if (name.empty())
    {
        return;
    }
    const S32 index = FSCameraBookmarkStore::instance().addBookmark(name, link, target_name);
    // always show and select new bookmarks
    mFilter->setValue(LLSD(std::string()));
    refreshList(index);
}

void FSFloaterCameraBookmarks::onClickAddFollowing()
{
    FSCameraLinks::View view;
    std::string target_name;
    if (!FSCameraLinks::getCurrentFollowView(view, getChild<LLUICtrl>("follow_turns")->getValue().asBoolean(), &target_name))
    {
        return;
    }
    std::string link;
    if (!FSCameraLinks::buildCheckedLink(view, link))
    {
        return;
    }
    askBookmarkName(target_name.empty() ? view.mRegion : target_name, link, target_name);
}

void FSFloaterCameraBookmarks::onClickGo()
{
    const S32 index = selectedIndex();
    if (index < 0)
    {
        return;
    }

    FSCameraLinks::View view;
    if (!FSCameraLinks::parseLink(mBookmarks[index]["link"].asString(), view))
    {
        LLNotificationsUtil::add("FSCameraLinkInvalid");
        return;
    }
    FSCameraLinks::activate(view);
}

void FSFloaterCameraBookmarks::onClickCopy()
{
    const std::vector<S32> selected = selectedIndices();
    if (selected.empty())
    {
        return;
    }

    // One
    if (selected.size() == 1)
    {
        const std::string link = mBookmarks[selected[0]]["link"].asString();
        FSCameraLinks::View view;
        if (!FSCameraLinks::parseLink(link, view))
        {
            LLNotificationsUtil::add("FSCameraLinkInvalid");
            return;
        }
        const LLWString wide_link = utf8str_to_wstring(link);
        LLClipboard::instance().copyToClipboard(wide_link, 0, (S32)wide_link.size());

        LLSD args;
        args["REGION"] = view.mRegion;
        LLNotificationsUtil::add("FSCameraLinkCopied", args);
        return;
    }

    // Several
    std::string text;
    S32 count = 0;
    for (S32 index : selected)
    {
        const std::string link = mBookmarks[index]["link"].asString();
        FSCameraLinks::View view;
        if (FSCameraLinks::parseLink(link, view))
        {
            text += mBookmarks[index]["name"].asString() + ": " + link + "\n";
            ++count;
        }
    }
    if (count == 0)
    {
        LLNotificationsUtil::add("FSCameraLinkInvalid");
        return;
    }
    const LLWString wide_text = utf8str_to_wstring(text);
    LLClipboard::instance().copyToClipboard(wide_text, 0, (S32)wide_text.size());
    LLSD args;
    args["COUNT"] = count;
    LLNotificationsUtil::add("FSCameraLinksCopied", args);
}

void FSFloaterCameraBookmarks::onClickRename()
{
    const S32 index = selectedIndex();
    if (index < 0)
    {
        return;
    }
    LLSD args;
    args["NAME"] = mBookmarks[index]["name"].asString();
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraBookmarkName", args, LLSD(),
        [handle, index](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (self && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                self->renameBookmark(index, response["message"].asString());
            }
        });
}

void FSFloaterCameraBookmarks::renameBookmark(S32 index, std::string name)
{
    name = clean_name(name);
    if (name.empty() || index < 0 || index >= (S32)mBookmarks.size())
    {
        return;
    }
    mBookmarks[index]["name"] = name;
    save();
    refreshList();
}

void FSFloaterCameraBookmarks::onClickUpdateView()
{
    const S32 index = selectedIndex();
    if (index < 0)
    {
        return;
    }

    FSCameraLinks::View old_view;
    const bool follows = FSCameraLinks::parseLink(mBookmarks[index]["link"].asString(), old_view) && old_view.hasTarget();
    FSCameraLinks::View view;
    std::string target_name;
    if (follows ? !FSCameraLinks::getCurrentFollowView(view, old_view.mTargetTurns, &target_name)
                : !FSCameraLinks::getCurrentView(view))
    {
        return;
    }
    const std::string link = FSCameraLinks::buildLink(view);
    LLSD args;
    args["NAME"] = mBookmarks[index]["name"].asString();
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraBookmarkUpdate", args, LLSD(),
        [handle, index, link, target_name](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (self && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                self->updateBookmarkView(index, link, target_name);
            }
        });
}

void FSFloaterCameraBookmarks::updateBookmarkView(S32 index, const std::string& link, const std::string& target_name)
{
    if (index < 0 || index >= (S32)mBookmarks.size())
    {
        return;
    }
    mBookmarks[index]["link"] = link;
    if (target_name.empty())
    {
        mBookmarks[index].erase("target");
    }
    else
    {
        mBookmarks[index]["target"] = target_name;
    }
    save();
    refreshList();
}

void FSFloaterCameraBookmarks::onClickDelete()
{
    const std::vector<S32> selected = selectedIndices();
    if (selected.empty())
    {
        return;
    }
    LLSD args;
    args["NAME"] = mBookmarks[selected[0]]["name"].asString();
    args["COUNT"] = (S32)selected.size();
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add(selected.size() == 1 ? "FSCameraBookmarkDelete" : "FSCameraBookmarksDelete", args, LLSD(),
        [handle, selected](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (self && LLNotificationsUtil::getSelectedOption(notification, response) == 0)
            {
                self->deleteBookmarks(selected);
            }
        });
}

void FSFloaterCameraBookmarks::deleteBookmarks(const std::vector<S32>& indices)
{
    // Tours keep their own copy of each view, changing a bookmark doesn't change tours.
    std::vector<S32> sorted = indices;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it)
    {
        if (*it >= 0 && *it < (S32)mBookmarks.size())
        {
            if (FSCameraLinks::isFollowingLink(mBookmarks[*it]["link"].asString()))
            {
                FSCameraLinks::releaseCamera();
            }
            mBookmarks.erase(*it);
        }
    }
    save();
    mList->deselectAllItems(true);
    if (!sorted.empty() && mBookmarks.size() > 0)
    {
        refreshList(llmin(sorted.front(), (S32)mBookmarks.size() - 1));
    }
    else
    {
        refreshList();
    }
}

void FSFloaterCameraBookmarks::onClickMove(S32 offset)
{
    const S32 index = selectedIndex();
    const S32 target = index + offset;
    if (index < 0 || target < 0 || target >= (S32)mBookmarks.size())
    {
        return;
    }
    const LLSD moved = mBookmarks[index];
    mBookmarks[index] = mBookmarks[target];
    mBookmarks[target] = moved;
    save();
    refreshList(target);
}

bool FSFloaterCameraBookmarks::onBookmarkMenuEnable(const std::string& action)
{
    const size_t selected = selectedIndices().size();
    if (action == "go" || action == "rename" || action == "update")
    {
        return selected == 1;
    }
    return selected > 0;    // copy, add_to_tour, delete
}

void FSFloaterCameraBookmarks::onBookmarkMenu(const std::string& action)
{
    if (!onBookmarkMenuEnable(action))
    {
        return;
    }
    if (action == "go")
    {
        onClickGo();
    }
    else if (action == "copy")
    {
        onClickCopy();
    }
    else if (action == "rename")
    {
        onClickRename();
    }
    else if (action == "update")
    {
        onClickUpdateView();
    }
    else if (action == "add_to_tour")
    {
        setToursVisible(true);
        onClickAddStops();
    }
    else if (action == "delete")
    {
        onClickDelete();
    }
}

///////////////////////////////////////////////////////////////////////////////
// Tours

void FSFloaterCameraBookmarks::setToursVisible(bool visible, bool remember)
{
    if (visible && mCompact)
    {
        setCompact(false);
    }

    S32 min_width = 0;
    S32 min_height = 0;
    getResizeLimits(&min_width, &min_height);
    const S32 bookmarks_min = mCompact ? COMPACT_MIN_WIDTH : BOOKMARKS_MIN_WIDTH;
    const S32 new_min_width = visible ? bookmarks_min + mToursWidth : bookmarks_min;
    setResizeLimits(new_min_width, min_height);

    const LLRect rect = getRect();
    S32 width = rect.getWidth();
    if (visible != mToursPanel->getVisible())
    {
        mToursPanel->setVisible(visible);
        width += visible ? mToursWidth : -mToursWidth;
    }
    width = llmax(width, new_min_width);
    if (width != rect.getWidth())
    {
        reshape(width, rect.getHeight());
    }
    layoutPanels();
    getChild<LLButton>("tours_toggle")->setLabel(getString(visible ? "hide_tours" : "show_tours"));
    if (remember)
    {
        gSavedSettings.setBOOL("FSCameraBookmarksShowTours", visible);
    }
}

void FSFloaterCameraBookmarks::setCompact(bool compact)
{
    getChild<LLButton>("compact_toggle")->setLabel(getString(compact ? "full_view" : "compact_view"));
    gSavedSettings.setBOOL("FSCameraBookmarksCompact", compact);
    if (compact == mCompact)
    {
        return;
    }

    if (compact)
    {
        mToursBeforeCompact = mToursPanel->getVisible();
        setToursVisible(false, false);
    }
    mCompact = compact;

    for (const char* name : BOOKMARK_BUTTONS)
    {
        getChild<LLView>(name)->setVisible(!compact);
    }

    LLRect list_rect = mList->getRect();
    list_rect.mBottom = compact ? LIST_BOTTOM_COMPACT : LIST_BOTTOM_COMPACT + mButtonRowsHeight;
    const S32 delta = compact ? -mButtonRowsHeight : mButtonRowsHeight;
    const S32 min_height = compact ? COMPACT_MIN_HEIGHT : FULL_MIN_HEIGHT;
    setResizeLimits(compact ? COMPACT_MIN_WIDTH : BOOKMARKS_MIN_WIDTH, min_height);

    const LLRect rect = getRect();
    const S32 height = llmax(rect.getHeight() + delta, min_height);
    reshape(rect.getWidth(), height);
    setOrigin(rect.mLeft, rect.mTop - height);

    list_rect.mTop = mList->getRect().mTop;
    mList->setShape(list_rect);

    LLView* toggle = getChild<LLView>("compact_toggle");
    LLRect toggle_rect = toggle->getRect();
    const S32 toggle_right = mBookmarksPanel->getRect().getWidth() - (compact ? mCompactToggleGapCompact : mCompactToggleGap);
    toggle_rect.setLeftTopAndSize(toggle_right - toggle_rect.getWidth(), toggle_rect.mTop, toggle_rect.getWidth(), toggle_rect.getHeight());
    toggle->setShape(toggle_rect);
    LLRect filter_rect = mFilter->getRect();
    filter_rect.mRight = toggle_rect.mLeft - 4;
    mFilter->setShape(filter_rect);

    if (!compact)
    {
        setToursVisible(mToursBeforeCompact, false);
    }
    updateButtons();
}

void FSFloaterCameraBookmarks::reshape(S32 width, S32 height, bool called_from_parent)
{
    LLFloater::reshape(width, height, called_from_parent);
    layoutPanels();
}

void FSFloaterCameraBookmarks::layoutPanels()
{
    if (!mBookmarksPanel || !mToursPanel)
    {
        return;
    }

    const S32 width = getRect().getWidth();
    const S32 height = llmax(getRect().getHeight() - getHeaderHeight(), 0);
    const S32 tours_width = mToursPanel->getVisible() ? llmin(mToursWidth, llmax(width - BOOKMARKS_MIN_WIDTH, 0)) : 0;
    const S32 bookmarks_width = width - tours_width;
    mBookmarksPanel->reshape(bookmarks_width, height);
    mBookmarksPanel->setOrigin(0, 0);
    if (tours_width > 0)
    {
        mToursPanel->reshape(tours_width, height);
        mToursPanel->setOrigin(bookmarks_width, 0);
    }
}

FSFloaterCameraBookmarks::Tour* FSFloaterCameraBookmarks::currentTour()
{
    return (mCurrentTour >= 0 && mCurrentTour < (S32)mTours.size()) ? &mTours[mCurrentTour] : nullptr;
}

const FSFloaterCameraBookmarks::Tour* FSFloaterCameraBookmarks::currentTour() const
{
    return (mCurrentTour >= 0 && mCurrentTour < (S32)mTours.size()) ? &mTours[mCurrentTour] : nullptr;
}

FSFloaterCameraBookmarks::Tour* FSFloaterCameraBookmarks::currentOrNewTour()
{
    if (!currentTour())
    {
        Tour tour;
        tour.mName = getString("new_tour_name");
        mTours.push_back(tour);
        mCurrentTour = (S32)mTours.size() - 1;
        refreshTours();
    }
    return currentTour();
}

// static
FSFloaterCameraBookmarks::Stop FSFloaterCameraBookmarks::newStop(const std::string& name, const std::string& link, const Tour& tour)
{
    Stop stop;
    stop.mName = name;
    stop.mLink = link;

    if (!tour.mStops.empty())
    {
        stop.mTravel = tour.mStops.back().mTravel;
        stop.mHold = tour.mStops.back().mHold;
    }
    return stop;
}

void FSFloaterCameraBookmarks::refreshTours()
{
    mTourCombo->removeall();
    for (S32 i = 0; i < (S32)mTours.size(); ++i)
    {
        mTourCombo->add(mTours[i].mName, LLSD(i));
    }
    if (currentTour())
    {
        mTourCombo->setValue(LLSD(mCurrentTour));
    }
    else
    {
        mTourCombo->setLabel(getString("no_tours"));
    }
    mStopList->deselectAllItems(true);
    refreshTourOptions();
    refreshStops();
}

void FSFloaterCameraBookmarks::refreshTourOptions()
{
    const Tour* tour = currentTour();
    const FSCameraLinks::TourOptions options = tour ? tour->mOptions : FSCameraLinks::TourOptions();
    mTourRepeat->setValue(LLSD(FSCameraLinks::tourRepeatName(options.mRepeat)));
    mTourPath->setValue(LLSD(std::string(options.mCurved ? "curved" : "straight")));
    mTourSpeed->setValue(LLSD(options.mSpeed));
    mTourHideUI->setValue(LLSD(options.mHideUI));
}

void FSFloaterCameraBookmarks::onCommitTourOptions()
{
    Tour* tour = currentTour();
    if (!tour)
    {
        return;
    }
    FSCameraLinks::parseTourRepeat(mTourRepeat->getValue().asString(), tour->mOptions.mRepeat);
    tour->mOptions.mCurved = mTourPath->getValue().asString() == "curved";
    tour->mOptions.mSpeed = llclamp(mTourSpeed->getValueF32(), FSCameraLinks::TOUR_MIN_SPEED, FSCameraLinks::TOUR_MAX_SPEED);
    tour->mOptions.mHideUI = mTourHideUI->getValue().asBoolean();

    save();
    updateTourStatus();
}

std::string FSFloaterCameraBookmarks::durationText(const Tour& tour) const
{
    F32 seconds = 0.f;
    for (const Stop& stop : tour.mStops)
    {
        seconds += stop.mTravel + stop.mHold;
    }
    LLStringUtil::format_map_t args;
    args["COUNT"] = llformat("%d", (S32)tour.mStops.size());
    const S32 total = ll_round(seconds / llmax(tour.mOptions.mSpeed, FSCameraLinks::TOUR_MIN_SPEED));
    LLStringUtil::format_map_t time_args;
    time_args["MINUTES"] = llformat("%d", total / 60);
    time_args["SECONDS"] = total >= 60 ? llformat("%02d", total % 60) : llformat("%d", total);
    args["TIME"] = getString(total >= 60 ? "duration_minutes" : "duration_seconds", time_args);
    return getString("tour_duration", args);
}

std::string FSFloaterCameraBookmarks::moveLabel(const Stop& stop) const
{
    if (stop.mMove == FSCameraLinks::TourMove::NONE || stop.mAmount <= 0.f)
    {
        return getString("move_none");
    }
    LLStringUtil::format_map_t args;
    args["AMOUNT"] = llformat("%g", stop.mAmount);
    return getString("move_" + FSCameraLinks::tourMoveName(stop.mMove), args);
}

void FSFloaterCameraBookmarks::refreshStops(const std::vector<S32>& select)
{
    const std::vector<S32> keep = select.empty() ? selectedStops() : select;
    const S32 scroll = mStopList->getScrollPos();
    mStopList->deleteAllItems();
    if (const Tour* tour = currentTour())
    {
        for (S32 i = 0; i < (S32)tour->mStops.size(); ++i)
        {
            const Stop& stop = tour->mStops[i];
            FSCameraLinks::View view;
            const bool valid = FSCameraLinks::parseLink(stop.mLink, view);
            LLSD row;
            row["value"] = i;
            LLSD& columns = row["columns"];
            columns[0]["column"] = "number";
            columns[0]["value"] = llformat("%d", i + 1);
            columns[1]["column"] = "name";
            LLStringUtil::format_map_t name_args;
            name_args["NAME"] = stop.mName;
            columns[1]["value"] = valid ? stop.mName : getString("invalid_stop", name_args);
            columns[2]["column"] = "fly";
            columns[2]["value"] = llformat("%g", stop.mTravel);
            columns[3]["column"] = "hold";
            columns[3]["value"] = llformat("%g", stop.mHold);
            columns[4]["column"] = "move";
            columns[4]["value"] = moveLabel(stop);
            mStopList->addElement(row);
        }
    }
    mStopList->setScrollPos(scroll);
    for (S32 index : keep)
    {
        mStopList->setSelectedByValue(LLSD(index), true);
    }
    if (!select.empty())
    {
        mStopList->scrollToShowSelected();
    }
    refreshStopEditor();
    updateTourStatus();
}

std::vector<S32> FSFloaterCameraBookmarks::selectedStops() const
{
    std::vector<S32> indices;
    const Tour* tour = currentTour();
    if (!mStopList || !tour)
    {
        return indices;
    }
    for (const LLScrollListItem* item : mStopList->getAllSelected())
    {
        const S32 index = item->getValue().asInteger();
        if (index >= 0 && index < (S32)tour->mStops.size())
        {
            indices.push_back(index);
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

void FSFloaterCameraBookmarks::refreshStopEditor()
{
    const std::vector<S32> selected = selectedStops();
    const Tour* tour = currentTour();
    const bool any = tour && !selected.empty();
    mStopTravel->setEnabled(any);
    mStopHold->setEnabled(any);
    mStopEase->setEnabled(any);
    mStopMove->setEnabled(any);
    std::string unit;
    if (any)
    {
        const Stop& stop = tour->mStops[selected[0]];
        mStopTravel->setValue(LLSD(stop.mTravel));
        mStopHold->setValue(LLSD(stop.mHold));
        mStopEase->setValue(LLSD(FSCameraLinks::tourEaseName(stop.mEase)));
        mStopMove->setValue(LLSD(FSCameraLinks::tourMoveName(stop.mMove)));
        mStopAmount->setValue(LLSD(stop.mAmount));
        mStopAmount->setEnabled(stop.mMove != FSCameraLinks::TourMove::NONE);
        switch (stop.mMove)
        {
            case FSCameraLinks::TourMove::NONE:     break;
            case FSCameraLinks::TourMove::PUSH_IN:
            case FSCameraLinks::TourMove::PULL_OUT: unit = getString("unit_percent"); break;
            case FSCameraLinks::TourMove::RISE:
            case FSCameraLinks::TourMove::SINK:     unit = getString("unit_metres"); break;
            default:                                unit = getString("unit_degrees"); break;
        }
    }
    else
    {
        mStopAmount->setEnabled(false);
    }
    getChild<LLUICtrl>("stop_amount_unit")->setValue(unit);
}

void FSFloaterCameraBookmarks::onCommitStopEditor(const std::string& field)
{
    Tour* tour = currentTour();
    const std::vector<S32> selected = selectedStops();
    if (!tour || selected.empty())
    {
        return;
    }
    FSCameraLinks::TourMove move = FSCameraLinks::TourMove::NONE;
    if (field == "move" && !FSCameraLinks::parseTourMove(mStopMove->getValue().asString(), move))
    {
        return;
    }
    FSCameraLinks::TourEase ease = FSCameraLinks::TourEase::SMOOTH;
    if (field == "ease" && !FSCameraLinks::parseTourEase(mStopEase->getValue().asString(), ease))
    {
        return;
    }
    for (S32 index : selected)
    {
        Stop& stop = tour->mStops[index];
        if (field == "travel")
        {
            stop.mTravel = llclamp(mStopTravel->getValueF32(), 0.f, FSCameraLinks::TOUR_MAX_TRAVEL);
        }
        else if (field == "hold")
        {
            stop.mHold = llclamp(mStopHold->getValueF32(), 0.f, FSCameraLinks::TOUR_MAX_HOLD);
        }
        else if (field == "move")
        {
            if (stop.mMove != move)
            {
                stop.mMove = move;
                stop.mAmount = FSCameraLinks::defaultTourMoveAmount(move);
            }
        }
        else if (field == "amount")
        {
            stop.mAmount = llclamp(mStopAmount->getValueF32(), 0.f, FSCameraLinks::TOUR_MAX_AMOUNT);
        }
        else if (field == "ease")
        {
            stop.mEase = ease;
        }
    }

    save();
    refreshStops(selected);
}

void FSFloaterCameraBookmarks::onSelectTour()
{
    const S32 index = mTourCombo->getValue().asInteger();
    if (index < 0 || index >= (S32)mTours.size() || index == mCurrentTour)
    {
        return;
    }
    mCurrentTour = index;
    save();
    mStopList->deselectAllItems(true);
    refreshTourOptions();
    refreshStops();
}

void FSFloaterCameraBookmarks::onClickNewTour()
{
    LLSD args;
    args["NAME"] = getString("new_tour_name");
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraTourName", args, LLSD(),
        [handle](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (!self || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            const std::string name = clean_name(response["message"].asString());
            if (name.empty())
            {
                return;
            }
            Tour tour;
            tour.mName = name;
            self->mTours.push_back(tour);
            self->mCurrentTour = (S32)self->mTours.size() - 1;
            self->save();
            self->refreshTours();
        });
}

void FSFloaterCameraBookmarks::onClickRenameTour()
{
    const Tour* tour = currentTour();
    if (!tour)
    {
        return;
    }
    LLSD args;
    args["NAME"] = tour->mName;
    const S32 index = mCurrentTour;
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraTourName", args, LLSD(),
        [handle, index](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (!self || LLNotificationsUtil::getSelectedOption(notification, response) != 0 ||
                index >= (S32)self->mTours.size())
            {
                return;
            }
            const std::string name = clean_name(response["message"].asString());
            if (name.empty())
            {
                return;
            }
            self->mTours[index].mName = name;
            self->save();
            const std::vector<S32> stops = self->selectedStops();
            self->refreshTours();
            self->refreshStops(stops);
        });
}

void FSFloaterCameraBookmarks::onClickDeleteTour()
{
    const Tour* tour = currentTour();
    if (!tour)
    {
        return;
    }
    LLSD args;
    args["NAME"] = tour->mName;
    const S32 index = mCurrentTour;
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraTourDelete", args, LLSD(),
        [handle, index](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (!self || LLNotificationsUtil::getSelectedOption(notification, response) != 0 ||
                index >= (S32)self->mTours.size())
            {
                return;
            }

            if (self->mPlayingTour == index)
            {
                FSCameraLinks::stopTour();
            }
            else if (self->mPlayingTour > index)
            {
                --self->mPlayingTour;
            }
            self->mTours.erase(self->mTours.begin() + index);
            self->mCurrentTour = self->mTours.empty() ? -1 : llmin(index, (S32)self->mTours.size() - 1);
            self->save();
            self->refreshTours();
        });
}

void FSFloaterCameraBookmarks::onClickAddStops()
{
    const std::vector<S32> selected = selectedIndices();
    if (selected.empty())
    {
        return;
    }

    Tour* tour = currentOrNewTour();
    stopTourIfPlaying(mCurrentTour);
    std::vector<S32> added;
    for (S32 index : selected)
    {
        added.push_back((S32)tour->mStops.size());
        tour->mStops.push_back(newStop(mBookmarks[index]["name"].asString(), mBookmarks[index]["link"].asString(), *tour));
    }
    save();
    refreshStops(added);
}

void FSFloaterCameraBookmarks::onClickAddCurrentStop()
{
    FSCameraLinks::View view;
    if (!FSCameraLinks::getCurrentView(view))
    {
        return;
    }
    const std::string link = FSCameraLinks::buildLink(view);
    LLSD args;
    args["NAME"] = view.mRegion;
    LLHandle<FSFloaterCameraBookmarks> handle = getDerivedHandle<FSFloaterCameraBookmarks>();
    LLNotificationsUtil::add("FSCameraTourStopName", args, LLSD(),
        [handle, link](const LLSD& notification, const LLSD& response)
        {
            FSFloaterCameraBookmarks* self = handle.get();
            if (!self || LLNotificationsUtil::getSelectedOption(notification, response) != 0)
            {
                return;
            }
            const std::string name = clean_name(response["message"].asString());
            if (name.empty())
            {
                return;
            }
            Tour* tour = self->currentOrNewTour();
            self->stopTourIfPlaying(self->mCurrentTour);
            tour->mStops.push_back(newStop(name, link, *tour));
            self->save();
            self->refreshStops({ (S32)tour->mStops.size() - 1 });
        });
}

void FSFloaterCameraBookmarks::onClickRemoveStops()
{
    Tour* tour = currentTour();
    const std::vector<S32> selected = selectedStops();
    if (!tour || selected.empty())
    {
        return;
    }
    stopTourIfPlaying(mCurrentTour);
    for (auto it = selected.rbegin(); it != selected.rend(); ++it)
    {
        tour->mStops.erase(tour->mStops.begin() + *it);
    }
    save();
    mStopList->deselectAllItems(true);
    if (tour->mStops.empty())
    {
        refreshStops();
    }
    else
    {
        refreshStops({ llmin(selected.front(), (S32)tour->mStops.size() - 1) });
    }
}

void FSFloaterCameraBookmarks::onClickMoveStop(S32 offset)
{
    Tour* tour = currentTour();
    std::vector<S32> selected = selectedStops();
    if (!tour || selected.empty())
    {
        return;
    }
    const S32 count = (S32)tour->mStops.size();
    if ((offset < 0 && selected.front() == 0) || (offset > 0 && selected.back() == count - 1))
    {
        return;
    }
    stopTourIfPlaying(mCurrentTour);

    if (offset > 0)
    {
        std::reverse(selected.begin(), selected.end());
    }
    for (S32& index : selected)
    {
        std::swap(tour->mStops[index], tour->mStops[index + offset]);
        index += offset;
    }
    save();
    refreshStops(selected);
}

void FSFloaterCameraBookmarks::onClickGoStop()
{
    const Tour* tour = currentTour();
    const std::vector<S32> selected = selectedStops();
    if (!tour || selected.size() != 1)
    {
        return;
    }
    FSCameraLinks::View view;
    if (!FSCameraLinks::parseLink(tour->mStops[selected[0]].mLink, view))
    {
        LLNotificationsUtil::add("FSCameraLinkInvalid");
        return;
    }
    FSCameraLinks::activate(view);
}


void FSFloaterCameraBookmarks::stopTourIfPlaying(S32 tour)
{
    if (tour >= 0 && tour == mPlayingTour && FSCameraLinks::isTourPlaying())
    {
        FSCameraLinks::stopTour();
    }
}

void FSFloaterCameraBookmarks::onClickPlayTour()
{
    if (FSCameraLinks::isTourPlaying())
    {
        FSCameraLinks::stopTour();
        return;
    }
    if (!currentTour())
    {
        return;
    }

    const std::vector<S32> selected = selectedStops();
    FSCameraBookmarkStore::instance().playTour(mCurrentTour, selected.empty() ? 0 : selected.front());
    updateTourStatus();
}

void FSFloaterCameraBookmarks::onClickCopyTour()
{
    const Tour* tour = currentTour();
    if (!tour)
    {
        return;
    }
    const FSCameraLinks::TourOptions& options = tour->mOptions;
    std::string text = TOUR_HEADER + " " + tour->mName + " | repeat=" + FSCameraLinks::tourRepeatName(options.mRepeat) +
                       " path=" + (options.mCurved ? "curved" : "straight") + llformat(" speed=%g", options.mSpeed) +
                       (options.mHideUI ? " hide_ui=1" : "") + "\n";
    S32 count = 0;
    for (const Stop& stop : tour->mStops)
    {
        FSCameraLinks::View view;
        if (!FSCameraLinks::parseLink(stop.mLink, view))
        {
            continue;
        }
        text += stop.mName + ": " + stop.mLink + llformat(" fly=%g hold=%g", stop.mTravel, stop.mHold);
        if (stop.mEase != FSCameraLinks::TourEase::SMOOTH)
        {
            text += " ease=" + FSCameraLinks::tourEaseName(stop.mEase);
        }
        if (stop.mMove != FSCameraLinks::TourMove::NONE)
        {
            text += " move=" + FSCameraLinks::tourMoveName(stop.mMove) + llformat(" amount=%g", stop.mAmount);
        }
        text += "\n";
        ++count;
    }
    if (count == 0)
    {
        LLNotificationsUtil::add("FSCameraLinkInvalid");
        return;
    }
    const LLWString wide_text = utf8str_to_wstring(text);
    LLClipboard::instance().copyToClipboard(wide_text, 0, (S32)wide_text.size());

    LLSD args;
    args["NAME"] = tour->mName;
    args["COUNT"] = count;
    LLNotificationsUtil::add("FSCameraTourCopied", args);
}

void FSFloaterCameraBookmarks::onClickPasteTour()
{
    LLWString wide_text;
    if (!LLClipboard::instance().isTextAvailable() || !LLClipboard::instance().pasteFromClipboard(wide_text))
    {
        LLNotificationsUtil::add("FSCameraBookmarkPasteNone");
        return;
    }

    // Line by line
    Tour tour;
    std::vector<std::string> lines;
    LLStringUtil::getTokens(wstring_to_utf8str(wide_text), lines, "\n");
    for (std::string line : lines)
    {
        LLStringUtil::trim(line);
        if (line.size() >= TOUR_HEADER.size() &&
            LLStringUtil::compareInsensitive(line.substr(0, TOUR_HEADER.size()), TOUR_HEADER) == 0)
        {
            std::string name = line.substr(TOUR_HEADER.size());
            const size_t bar = name.rfind(" | ");
            if (bar != std::string::npos)
            {
                parse_tour_options(name.substr(bar + 3), tour.mOptions);
                name = name.substr(0, bar);
            }
            tour.mName = clean_name(name);
            continue;
        }
        for (const PastedLink& pasted : find_links(line))
        {
            FSCameraLinks::View view;
            if (!FSCameraLinks::parseLink(pasted.mLink, view))
            {
                continue;
            }
            std::string name = clean_name(pasted.mLabel);
            Stop stop = newStop(name.empty() ? clean_name(view.mRegion) : name, FSCameraLinks::buildLink(view), tour);
            parse_stop_settings(line, stop);
            tour.mStops.push_back(stop);
        }
    }
    if (tour.mStops.empty())
    {
        LLNotificationsUtil::add("FSCameraBookmarkPasteNone");
        return;
    }
    if (tour.mName.empty())
    {
        tour.mName = getString("pasted_tour_name");
    }
    mTours.push_back(tour);
    mCurrentTour = (S32)mTours.size() - 1;
    save();
    setToursVisible(true);
    refreshTours();

    LLSD args;
    args["NAME"] = tour.mName;
    args["COUNT"] = (S32)tour.mStops.size();
    LLNotificationsUtil::add("FSCameraTourPasted", args);
}

void FSFloaterCameraBookmarks::updateTourStatus()
{
    const bool playing = FSCameraLinks::isTourPlaying();
    getChild<LLButton>("play_tour")->setLabel(getString(playing ? "tour_stop" : "tour_play"));

    std::string status;
    const S32 index = FSCameraLinks::getTourIndex();
    if (playing && mPlayingTour >= 0 && mPlayingTour < (S32)mTours.size() && index >= 0 && index < (S32)mPlayingStops.size())
    {
        const Tour& tour = mTours[mPlayingTour];
        const S32 row = mPlayingStops[index];
        const S32 skipped = FSCameraLinks::getTourSkipped();
        LLStringUtil::format_map_t args;
        args["TOUR"] = tour.mName;
        args["NUMBER"] = llformat("%d", row + 1);
        args["COUNT"] = llformat("%d", (S32)tour.mStops.size());
        args["NAME"] = row < (S32)tour.mStops.size() ? tour.mStops[row].mName : std::string();
        args["SKIPPED"] = llformat("%d", skipped);
        status = getString(skipped > 0 ? "tour_status_skipped" : "tour_status", args);
    }
    else if (const Tour* tour = currentTour())
    {
        status = durationText(*tour);
    }
    getChild<LLUICtrl>("tour_status")->setValue(status);
    markPlayingStop();
    updateButtons();
}

void FSFloaterCameraBookmarks::markPlayingStop()
{
    const S32 index = FSCameraLinks::getTourIndex();
    const S32 row = (FSCameraLinks::isTourPlaying() && mPlayingTour == mCurrentTour && index >= 0 && index < (S32)mPlayingStops.size())
        ? mPlayingStops[index] : -1;
    for (LLScrollListItem* item : mStopList->getAllData())
    {
        const U8 style = item->getValue().asInteger() == row ? LLFontGL::BOLD : LLFontGL::NORMAL;
        for (S32 column = 0; column < item->getNumColumns(); ++column)
        {
            if (LLScrollListText* text = dynamic_cast<LLScrollListText*>(item->getColumn(column)))
            {
                text->setFontStyle(style);
            }
        }
    }
}

/**
 * @file fsfloaterstreamtitle.h
 * @brief Class for the stream title and history floater
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2022 Ansariel Hiller @ Second Life
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

#ifndef FS_FLOATERSTREAMTITLE_H
#define FS_FLOATERSTREAMTITLE_H

#include "lleventtimer.h"
#include "llfloater.h"
#include "llsingleton.h"
#include "llstreamingaudio.h"
#include "streamtitledisplay.h"

class LLButton;
class LLTextBox;
class FSScrollListCtrl;

class FSStreamTitleManager : public LLSingleton<FSStreamTitleManager>
{
    LLSINGLETON_EMPTY_CTOR(FSStreamTitleManager);

public:
    ~FSStreamTitleManager() override;

    using history_vec_t = std::vector<std::string>;

    using streamtitle_update_callback_t = boost::signals2::signal<void(std::string_view streamtitle)>;
    boost::signals2::connection setUpdateCallback(const streamtitle_update_callback_t::slot_type& cb) noexcept
    {
        return mUpdateSignal.connect(cb);
    }

    using streamtitle_history_update_callback_t = boost::signals2::signal<void(const history_vec_t& streamtitle_history)>;
    boost::signals2::connection setHistoryUpdateCallback(const streamtitle_history_update_callback_t::slot_type& cb) noexcept
    {
        return mHistoryUpdateSignal.connect(cb);
    }

    std::string getCurrentStreamTitle() const noexcept { return mCurrentStreamTitle; }
    const history_vec_t& getStreamTitleHistory() const noexcept { return mStreamTitleHistory; }

protected:
    void initSingleton() override;
    void processMetadataUpdate(const LLSD& metadata) noexcept;

    boost::signals2::connection mMetadataUpdateConnection;
    streamtitle_update_callback_t mUpdateSignal;
    streamtitle_history_update_callback_t mHistoryUpdateSignal;

    std::string mCurrentStreamTitle{};
    history_vec_t mStreamTitleHistory{};
};

class FSFloaterStreamTitleHistory : public LLFloater
{
public:
    FSFloaterStreamTitleHistory(const LLSD& key);
    ~FSFloaterStreamTitleHistory() override;

    bool postBuild() override;
    void draw() override;
    void setOwnerOrigin(LLView* owner) noexcept;

protected:
    void updateHistory(const FSStreamTitleManager::history_vec_t& history);

    FSScrollListCtrl* mHistoryCtrl{ nullptr };

    boost::signals2::connection mUpdateConnection{};

    LLHandle<LLView> mOwner{};
    F32 mContextConeOpacity{ 0.f };
};

class FSFloaterStreamTitle : public LLFloater, LLEventTimer
{
public:
    FSFloaterStreamTitle(const LLSD& key);
    ~FSFloaterStreamTitle() override;

    bool postBuild() override;
    void reshape(S32 width, S32 height, bool called_from_parent = true) override;

private:
    bool tick() override;

    void updateStreamTitle(std::string_view streamtitle) noexcept;
    void toggleHistory() noexcept;
    void closeHistory() noexcept;
    void checkTitleWidth() noexcept;

    LLButton* mHistoryBtn{ nullptr };
    LLTextBox* mTitletext{ nullptr };

    LLHandle<LLFloater> mHistory{};

    boost::signals2::connection mUpdateConnection{};

    std::string mCurrentTitle{};
    std::string mCurrentDrawnTitle{};
    bool mResetTitle{ false };

};

#endif // FS_FLOATERSTREAMTITLE_H

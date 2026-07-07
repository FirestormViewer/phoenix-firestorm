/**
 * @file omnifilterengine.h
 * @brief The core Omnifilter engine
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2025, Zi Ree @ Second Life
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
 * $/LicenseInfo$
 */

#ifndef OMNIFILTERENGINE_H
#define OMNIFILTERENGINE_H

#include "lleventtimer.h"
#include "llsingleton.h"

#include <boost/bind.hpp>
#include <boost/signals2.hpp>

class LLInventoryItem;
class LLInventoryCallback;

class OmnifilterEngine
:   public LLSingleton<OmnifilterEngine>
,   public LLEventTimer
{
    LLSINGLETON(OmnifilterEngine);
    ~OmnifilterEngine();

    public:
        enum eType
        {
            NearbyChat,
            GroupChat,
            GroupNotice,
            GroupInvite,
            InstantMessage,
            ObjectChat,
            ObjectInstantMessage,
            ScriptError,
            ScriptDialog,
            FriendshipOffer,
            InventoryOffer,
            Lure,
            TeleportRequest,
            URLRequest,
            TYPES_MAX
        };

        enum eMatchType
        {
            Exact,
            Substring,
            Regex,
            MATCH_TYPES_MAX
        };

        class Haystack
        {
            public:
                // Set all the default values
                std::string mSenderName = "";
                std::string mContent = "";
                std::string mRegionName = "";

                LLUUID mOwnerID = LLUUID::null;

                eType mType = TYPES_MAX;
        };

        class Needle
        {
            public:
                // Set all the default values
                std::string mSenderName = "";
                std::string mContent = "";
                std::string mRegionName = "";

                std::string mChatReplace = "";
                std::string mButtonReply = "";
                std::string mTextBoxReply = "";
                F32 mReplyDelay = 0.0f;

                eMatchType mSenderNameMatchType = eMatchType::Substring;
                eMatchType mContentMatchType = eMatchType::Substring;

                LLUUID mOwnerID = LLUUID::null;

                std::set<eType> mTypes;

                bool mEnabled = false;
                bool mSenderNameCaseInsensitive = false;
                bool mContentCaseInsensitive = false;
        };

        static const S32 VERSION; // Current version of Omnifilter xml format.
        static const std::string HEADER; // Header used to verify the notecard xml data is valid
        static const std::string FILTER_NAME; // Item name filter used to show only Omnifilter Notecards

        typedef std::map<std::string, OmnifilterEngine::OmnifilterEngine::Needle, std::less<>> needle_list_t;
        needle_list_t& getNeedleList();

        // Typedef for the ordered list which is a vector of strings, used to keep track of the map order, which uses strings to lookup the
        // needles.
        typedef std::vector<std::string> needle_ordered_list_t;
        needle_ordered_list_t& getOrderedNeedleList() { return mOrderedNeedles; };
        S32 getOrderedNeedleListSize() const { return static_cast<S32>(mOrderedNeedles.size()); };
        std::string_view getOrderedNeedleName(const S32 index) const;
        S32 getOrderedNeedleIndex(std::string_view);
        bool setOrderedNeedleName(const S32 needle_index, std::string_view new_name);
        const Needle* getOrderedNeedle(const S32 index);
        bool swapNeedles(const S32 index1, const S32 index2);

        Needle& newNeedle(const std::string& needle_name);
        void renameNeedle(const std::string& old_name, const std::string& new_name);
        void deleteNeedle(const std::string& needle_name);

        const Needle* match(const Haystack& haystack);
        void setDirty(bool dirty);

        void init();

        bool setCurrentRuleSet(std::string_view rule_set_name);
        S32 removeCurrentRuleSet();
        S32 addNewRuleSet(std::string_view new_name);
        S32 addClonedRuleSet(std::string_view new_name);
        void exportNotecardCreatedCallback(const LLUUID & notecard_uuid);
        S32 exportRuleSetToNotecard(std::string_view export_name, LLPointer<LLInventoryCallback> cb);
        S32 importRuleSetFromNotecard(std::string_view import_name);
        LLUUID& getExportUUID() { return mExportNotecardUUID; }
        bool validateImportNotecard(const LLUUID& notecard_uuid);
        bool assignRuleSet(const bool rule_set_to_internal = true);
        bool assignRuleSetNameFromSettings();
        std::string getCurrentSelectedRuleSet() { return mCurrentSelectedRuleSet; }
        needle_ordered_list_t& getOrderedRuleSets() { return mOrderedRuleSets; }
        std::string getImportName() { return mImportName; }
        void setImportName(std::string name) { mImportName = name; }
        std::string_view getOrderedRuleSetName(const S32 index) const;
        S32 getOrderedRuleSetIndex(std::string_view lookup_name);
        S32 getOrderedRuleSetSize() { return static_cast<S32>(mOrderedRuleSets.size()); }

        typedef boost::signals2::signal<void(time_t, const std::string&)> log_signal_t;
        log_signal_t mLogSignal;

        std::vector<std::pair<time_t, std::string>> mLog;

        typedef std::pair<needle_ordered_list_t, needle_list_t> rule_set_t;
        typedef std::map<std::string, rule_set_t> rule_sets_t;
        rule_sets_t& getRuleSets() { return mNeedleRuleSets; }

        // Create a signal for the rule set changing, so that multiple UI can be changed in sync.
        typedef boost::signals2::signal<void()> rule_set_updated_signal_t;

        boost::signals2::connection setRuleSetUpdatedCallback(const rule_set_updated_signal_t::slot_type& cb)
        {
            return mRuleSetUpdatedCallback.connect(cb);
        }
        void onRuleSetsUpdated();
        rule_set_updated_signal_t mRuleSetUpdatedCallback;

        // New delay response which is to fix issue FIRE-36590 - The Omnifilter Dialog Button Reply needs a 1 second delay
        void createDelayedResponse(LLNotificationPtr notification, LLSD response, F32 delay);
        void sendDelayedResponse(LLNotificationPtr notification, LLSD response);

    protected:
        const Needle* logMatch(const std::string& needle_name, const Needle& needle);
        bool matchStrings(std::string_view needle_string, std::string_view haystack_string, eMatchType match_type, bool case_insensitive);
        bool searchStrings(std::string_view needle_string, std::string_view haystack_string, eMatchType match_type, bool case_insensitive);

        bool importFromLLSD(const LLSD& data);
        rule_set_t importRuleSetFromLLSD(const LLSD& data);
        LLSD exportToLLSD();
        LLSD exportToLLSD(const std::string &rule_set_name);
        void loadNeedles();
        void saveNeedles();

        bool tick() override;

    protected:
        rule_sets_t mNeedleRuleSets;
        std::string mCurrentSelectedRuleSet;
        needle_ordered_list_t mOrderedRuleSets;

        needle_list_t mNeedles;
        needle_ordered_list_t mOrderedNeedles;

        std::string mNeedlesXMLPath;

        bool mDirty;
        std::string mExportName;
        std::string mImportName;
        LLPointer<LLInventoryCallback> mExportUICallback;
        LLUUID mExportNotecardUUID;
        rule_set_t mImportNotecardRuleSet;
};

#endif // OMNIFILTERENGINE_H

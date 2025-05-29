/**
 * @file llurlregistry.cpp
 * @author Martin Reddy
 * @brief Contains a set of Url types that can be matched in a string
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

#include "linden_common.h"
#include "llregex.h"
#include "llurlregistry.h"
#include "lluriparser.h"

#include <boost/algorithm/string/find.hpp> //for boost::ifind_first -KC

// default dummy callback that ignores any label updates from the server
void LLUrlRegistryNullCallback(const std::string &url, const std::string &label, const std::string& icon)
{
}

LLUrlRegistry::LLUrlRegistry()
{
//  mUrlEntry.reserve(20);
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
    mUrlEntry.reserve(31);
// [/RLVa:KB]

    // Urls are matched in the order that they were registered
    mUrlEntryNoLink = new LLUrlEntryNoLink();
    registerUrl(mUrlEntryNoLink);
    mUrlEntryIcon = new LLUrlEntryIcon();
    registerUrl(mUrlEntryIcon);
    mLLUrlEntryInvalidSLURL = new LLUrlEntryInvalidSLURL();
    registerUrl(mLLUrlEntryInvalidSLURL);
    registerUrl(new LLUrlEntrySLURL());

    // decorated links for host names like: secondlife.com and lindenlab.com
    // <FS:Ansariel> Normalize only trusted URL
    //registerUrl(new LLUrlEntrySeconlifeURL());
    mUrlEntryTrustedUrl = new LLUrlEntrySecondlifeURL();
    registerUrl(mUrlEntryTrustedUrl);
    // </FS:Ansariel>
    registerUrl(new LLUrlEntrySimpleSecondlifeURL());

    registerUrl(new LLUrlEntryHTTP());
    mUrlEntryHTTPLabel = new LLUrlEntryHTTPLabel();
    registerUrl(mUrlEntryHTTPLabel);
    registerUrl(new LLUrlEntryAgentCompleteName());
    registerUrl(new LLUrlEntryAgentLegacyName());
    registerUrl(new LLUrlEntryAgentDisplayName());
    registerUrl(new LLUrlEntryAgentUserName());
// [RLVa:KB] - Checked: 2010-11-01 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
    registerUrl(new LLUrlEntryAgentRLVAnonymizedName());
// [/RLVa:KB]
    registerUrl(new FSUrlEntryAgentSelf());// <FS:Ansariel> FIRE-30611: "You" in transcript is underlined
    // LLUrlEntryAgent*Name must appear before LLUrlEntryAgent since
    // LLUrlEntryAgent is a less specific (catchall for agent urls)
    mUrlEntryAgentMention = new LLUrlEntryAgentMention();
    registerUrl(mUrlEntryAgentMention);
    registerUrl(new LLUrlEntryAgent());
    registerUrl(new LLUrlEntryChat());
    registerUrl(new LLUrlEntryGroup());
    registerUrl(new LLUrlEntryParcel());
    registerUrl(new LLUrlEntryTeleport());
    registerUrl(new LLUrlEntryRegion());
    registerUrl(new LLUrlEntryWorldMap());
    registerUrl(new LLUrlEntryObjectIM());
    registerUrl(new LLUrlEntryPlace());
    registerUrl(new LLUrlEntryInventory());
    registerUrl(new LLUrlEntryExperienceProfile());
    mUrlEntryKeybinding = new LLUrlEntryKeybinding();
    registerUrl(mUrlEntryKeybinding);
    registerUrl(new FSHelpDebugUrlEntrySL()); // <FS:Ansariel> FS Help SLUrl
    // <FS:Ansariel> Wear folder SLUrl
    mUrlEntryWear = new FSUrlEntryWear();
    registerUrl(mUrlEntryWear);
    //LLUrlEntrySL and LLUrlEntrySLLabel have more common pattern,
    //so it should be registered in the end of list
    registerUrl(new LLUrlEntrySL());
    mUrlEntrySLLabel = new LLUrlEntrySLLabel();
    registerUrl(mUrlEntrySLLabel);
    // <FS:Ansariel> Allow URLs with no protocol again
    registerUrl(new LLUrlEntryHTTPNoProtocol());
    registerUrl(new LLUrlEntryEmail());
    registerUrl(new LLUrlEntryIPv6());
    // parse jira issue names to links -KC
    registerUrl(new LLUrlEntryJira());
}

LLUrlRegistry::~LLUrlRegistry()
{
    // free all of the LLUrlEntryBase objects we are holding
    std::vector<LLUrlEntryBase *>::iterator it;
    for (it = mUrlEntry.begin(); it != mUrlEntry.end(); ++it)
    {
        delete *it;
    }
}

void LLUrlRegistry::registerUrl(LLUrlEntryBase *url, bool force_front)
{
    if (url)
    {
        if (force_front)  // IDEVO
            mUrlEntry.insert(mUrlEntry.begin(), url);
        else
        mUrlEntry.push_back(url);
    }
}

static bool matchRegex(const char *text, boost::regex regex, U32 &start, U32 &end)
{
    boost::cmatch result;
    bool found;

    found = ll_regex_search(text, result, regex);

    if (! found)
    {
        return false;
    }

    // return the first/last character offset for the matched substring
    start = static_cast<U32>(result[0].first - text);
    end = static_cast<U32>(result[0].second - text) - 1;

    // we allow certain punctuation to terminate a Url but not match it,
    // e.g., "http://foo.com/." should just match "http://foo.com/"
    if (text[end] == '.' || text[end] == ',')
    {
        end--;
    }
    // ignore a terminating ')' when Url contains no matching '('
    // see DEV-19842 for details
    else if (text[end] == ')' && std::string(text+start, end-start).find('(') == std::string::npos)
    {
        end--;
    }

    else if (text[end] == ']' && std::string(text+start, end-start).find('[') == std::string::npos)
    {
            end--;
    }

    return true;
}

static bool stringHasUrl(const std::string &text)
{
    // fast heuristic test for a URL in a string. This is used
    // to avoid lots of costly regex calls, BUT it needs to be
    // kept in sync with the LLUrlEntry regexes we support.
    return (text.find("://") != std::string::npos ||
            // text.find("www.") != std::string::npos ||
            // text.find(".com") != std::string::npos ||
            // allow ALLCAPS urls -KC
            boost::ifind_first(text, "www.") ||
            boost::ifind_first(text, ".com") ||
            boost::ifind_first(text, ".net") ||
            boost::ifind_first(text, ".edu") ||
            boost::ifind_first(text, ".org") ||
            text.find("<nolink>") != std::string::npos ||
            text.find("<icon") != std::string::npos ||
            text.find("@") != std::string::npos);
}

static bool stringHasJira(const std::string &text)
{
    // same as above, but for jiras
    // <FS:CR> Please make sure to sync these with the items in LLUrlEntryJira::LLUrlEntryJira() if you make a change
    return (text.find("ARVD") != std::string::npos ||
            text.find("BUG") != std::string::npos ||
            text.find("CHOP") != std::string::npos ||
            text.find("CHUIBUG") != std::string::npos ||
            text.find("CTS") != std::string::npos ||
            text.find("DOC") != std::string::npos ||
            text.find("DN") != std::string::npos ||
            text.find("ECC") != std::string::npos ||
            text.find("EXP") != std::string::npos ||
            text.find("FIRE") != std::string::npos ||
            text.find("FITMESH") != std::string::npos ||
            text.find("LEAP") != std::string::npos ||
            text.find("LLSD") != std::string::npos ||
            text.find("MATBUG") != std::string::npos ||
            text.find("MISC") != std::string::npos ||
            text.find("OPEN") != std::string::npos ||
            text.find("PATHBUG") != std::string::npos ||
            text.find("PLAT") != std::string::npos ||
            text.find("PYO") != std::string::npos ||
            text.find("SCR") != std::string::npos ||
            text.find("SH") != std::string::npos ||
            text.find("SINV") != std::string::npos ||
            text.find("SLS") != std::string::npos ||
            text.find("SNOW") != std::string::npos ||
            text.find("SOCIAL") != std::string::npos ||
            text.find("STORM") != std::string::npos ||
            text.find("SUN") != std::string::npos ||
            text.find("SUP") != std::string::npos ||
            text.find("SVC") != std::string::npos ||
            text.find("TPV") != std::string::npos ||
            text.find("VWR") != std::string::npos ||
            text.find("WEB") != std::string::npos);
}

bool LLUrlRegistry::findUrl(const std::string &text, LLUrlMatch &match, const LLUrlLabelCallback &cb, bool is_content_trusted, bool skip_non_mentions)
{
    // avoid costly regexes if there is clearly no URL in the text
    if (! (stringHasUrl(text) || stringHasJira(text)))
    {
        return false;
    }

    // find the first matching regex from all url entries in the registry
    U32 match_start = 0, match_end = 0;
    LLUrlEntryBase *match_entry = NULL;

    std::vector<LLUrlEntryBase *>::iterator it;
    for (it = mUrlEntry.begin(); it != mUrlEntry.end(); ++it)
    {
        //Skip for url entry icon if content is not trusted
        if((mUrlEntryIcon == *it) && ((text.find("Hand") != std::string::npos) || !is_content_trusted))
        {
            continue;
        }

        if (skip_non_mentions && (mUrlEntryAgentMention != *it))
        {
            continue;
        }

        LLUrlEntryBase *url_entry = *it;

        U32 start = 0, end = 0;
        if (matchRegex(text.c_str(), url_entry->getPattern(), start, end))
        {
            // does this match occur in the string before any other match
            if (start < match_start || match_entry == NULL)
            {

                if (mLLUrlEntryInvalidSLURL == *it)
                {
                    if(url_entry && url_entry->isSLURLvalid(text.substr(start, end - start + 1)))
                    {
                        continue;
                    }
                }

                if((mUrlEntryHTTPLabel == *it) || (mUrlEntrySLLabel == *it))
                {
                    if(url_entry && !url_entry->isWikiLinkCorrect(text.substr(start, end - start + 1)))
                    {
                        continue;
                    }
                }

                match_start = start;
                match_end = end;
                match_entry = url_entry;

                // <FS:Ansariel> Wear folder SLUrl
                if (mUrlEntryWear == *it)
                {
                    break;
                }
                // </FS:Ansariel>
            }
        }
    }

    // did we find a match? if so, return its details in the match object
    if (match_entry)
    {
        // Skip if link is an email with an empty username (starting with @). See MAINT-5371.
        if (match_start > 0 && text.substr(match_start - 1, 1) == "@")
            return false;

        // fill in the LLUrlMatch object and return it
        std::string url = text.substr(match_start, match_end - match_start + 1);

        // <FS:Ansariel> Fix the "nolink>" fail; Fix from Alchemy viewer, courtesy of Drake Arconis
        //if (match_entry == mUrlEntryTrusted)
        //{
        //  LLUriParser up(url);
        //  if (up.normalize() == 0)
        //    {
        //        url = up.normalizedUri();
        //    }
        //}
        if (match_entry != mUrlEntryNoLink && match_entry == mUrlEntryTrustedUrl)
        {
            LLUriParser up(url);
            if (up.normalize())
            {
                url = up.normalizedUri();
            }
        }
        // </FS:Ansariel>

        match.setValues(match_start, match_end,
                        match_entry->getUrl(url),
                        match_entry->getLabel(url, cb),
                        match_entry->getQuery(url),
                        match_entry->getTooltip(url),
                        match_entry->getIcon(url),
                        match_entry->getStyle(url),
                        match_entry->getMenuName(),
                        match_entry->getLocation(url),
                        // <FS:Ansariel> Store matched text
                        text.substr(match_start, match_end - match_start + 1),
                        match_entry->getID(url),
                        match_entry->getUnderline(url),
                        match_entry->isTrusted(),
                        match_entry->getSkipProfileIcon(url));
        return true;
    }

    return false;
}

bool LLUrlRegistry::findUrl(const LLWString &text, LLUrlMatch &match, const LLUrlLabelCallback &cb)
{
    // boost::regex_search() only works on char or wchar_t
    // types, but wchar_t is only 2-bytes on Win32 (not 4).
    // So we use UTF-8 to make this work the same everywhere.
    std::string utf8_text = wstring_to_utf8str(text);
    if (findUrl(utf8_text, match, cb))
    {
        // we cannot blindly return the start/end offsets from
        // the UTF-8 string because it is a variable-length
        // character encoding, so we need to update the start
        // and end values to be correct for the wide string.
        // <FS:Ansariel> Fix for LLUrlEntryHTTPLabel and
        // LLUrlEntrySLLabel: Cannot simply replace the URL,
        //need to replace the matched text!
        //LLWString wurl = utf8str_to_wstring(match.getUrl());
        LLWString wurl = utf8str_to_wstring(match.getMatchedText());
        // </FS:Ansariel>
        size_t start = text.find(wurl);
        if (start == std::string::npos)
        {
            return false;
        }
        auto end = start + wurl.size() - 1;

        match.setValues(static_cast<U32>(start), static_cast<U32>(end), match.getUrl(),
                        match.getLabel(),
                        match.getQuery(),
                        match.getTooltip(),
                        match.getIcon(),
                        match.getStyle(),
                        match.getMenuName(),
                        match.getLocation(),
                        // <FS:Ansariel> Store matched text
                        match.getMatchedText(),
                        match.getID(),
                        match.getUnderline(),
                        false,
                        match.getSkipProfileIcon());
        return true;
    }
    return false;
}

bool LLUrlRegistry::hasUrl(const std::string &text)
{
    LLUrlMatch match;
    return findUrl(text, match);
}

bool LLUrlRegistry::hasUrl(const LLWString &text)
{
    LLUrlMatch match;
    return findUrl(text, match);
}

bool LLUrlRegistry::isUrl(const std::string &text)
{
    LLUrlMatch match;
    if (findUrl(text, match))
    {
        return (match.getStart() == 0 && match.getEnd() >= text.size()-1);
    }
    return false;
}

bool LLUrlRegistry::isUrl(const LLWString &text)
{
    LLUrlMatch match;
    if (findUrl(text, match))
    {
        return (match.getStart() == 0 && match.getEnd() >= text.size()-1);
    }
    return false;
}

void LLUrlRegistry::setKeybindingHandler(LLKeyBindingToStringHandler* handler)
{
    LLUrlEntryKeybinding *entry = (LLUrlEntryKeybinding*)mUrlEntryKeybinding;
    entry->setHandler(handler);
}

bool LLUrlRegistry::containsAgentMention(const std::string& text)
{
    // avoid costly regexes if there is clearly no URL in the text
    if (!stringHasUrl(text))
    {
        return false;
    }

    try
    {
        boost::sregex_iterator it(text.begin(), text.end(), mUrlEntryAgentMention->getPattern());
        boost::sregex_iterator end;
        for (; it != end; ++it)
        {
            if (mUrlEntryAgentMention->isAgentID(it->str()))
            {
               return true;
            }
        }
    }
    catch (boost::regex_error&)
    {
        LL_INFOS() << "Regex error for: " << text << LL_ENDL;
    }
    return false;
}

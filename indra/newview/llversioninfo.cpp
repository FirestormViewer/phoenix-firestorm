/**
 * @file llversioninfo.cpp
 * @brief Routines to access the viewer version and build information
 * @author Martin Reddy
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

#include "llviewerprecompiledheaders.h"
#include "llevents.h"
#include "lleventfilter.h"
#include "llregex.h"
#include "llversioninfo.h"
#include "stringize.h"

// <FS:TS> Use configured file instead of compile time definitions to avoid
//         rebuilding the world with every Mercurial pull
#include "fsversionvalues.h"

#if ! defined(LL_VIEWER_CHANNEL)       \
 || ! defined(LL_VIEWER_VERSION_MAJOR) \
 || ! defined(LL_VIEWER_VERSION_MINOR) \
 || ! defined(LL_VIEWER_VERSION_PATCH) \
 || ! defined(LL_VIEWER_VERSION_BUILD)
 #error "Channel or Version information is undefined"
#endif

//
// Set the version numbers in indra/VIEWER_VERSION
//

LLVersionInfo::LLVersionInfo():
    short_version(STRINGIZE(LL_VIEWER_VERSION_MAJOR << "."
                            << LL_VIEWER_VERSION_MINOR << "."
                            << LL_VIEWER_VERSION_PATCH)),
    // LL_VIEWER_CHANNEL is a macro defined on the compiler command line. The
    // macro expands to the string name of the channel, but without quotes. We
    // need to turn it into a quoted string. LL_TO_STRING() does that.
    mWorkingChannelName(LL_TO_STRING(LL_VIEWER_CHANNEL)),
    build_configuration(LLBUILD_CONFIG), // set in indra/cmake/BuildVersion.cmake
    // instantiate an LLEventMailDrop with canonical name to listen for news
    // from SLVersionChecker
    mPump{new LLEventMailDrop("relnotes")},
    // immediately listen on mPump, store arriving URL into mReleaseNotes
    mStore{new LLStoreListener<std::string>(*mPump, mReleaseNotes)}
{
    // <FS:Ansariel> Above macro hackery results in extra quotes - fix it if it happens
    if (LLStringUtil::startsWith(mWorkingChannelName, "\"") && mWorkingChannelName.size() > 2)
    {
        mWorkingChannelName = mWorkingChannelName.substr(1, mWorkingChannelName.size() - 2);
    }
    // </FS:Ansariel>
}

void LLVersionInfo::initSingleton()
{
    // We override initSingleton() not because we have dependencies on other
    // LLSingletons, but because certain initializations call other member
    // functions. We should refrain from calling methods until this object is
    // fully constructed; such calls don't really belong in the constructor.

    // cache the version string
    version = stringize(getShortVersion(), ".", getBuild());
}

LLVersionInfo::~LLVersionInfo()
{
}

S32 LLVersionInfo::getMajor() const
{
    return LL_VIEWER_VERSION_MAJOR;
}

S32 LLVersionInfo::getMinor() const
{
    return LL_VIEWER_VERSION_MINOR;
}

S32 LLVersionInfo::getPatch() const
{
    return LL_VIEWER_VERSION_PATCH;
}

U64 LLVersionInfo::getBuild() const
{
    return LL_VIEWER_VERSION_BUILD;
}

std::string LLVersionInfo::getVersion() const
{
    return version;
}

//<FS:CZ>
std::string LLVersionInfo::getBuildVersion() const
{
    static std::string build_version("");
    if (build_version.empty())
    {
        std::ostringstream stream;
        stream << LLVersionInfo::getBuild();
        // cache the version string
        build_version = stream.str();
    }
    return build_version;
}
//</FS:CZ>

std::string LLVersionInfo::getShortVersion() const
{
    return short_version;
}

std::string LLVersionInfo::getChannelAndVersion()
{
    if (mVersionChannel.empty())
    {
        // cache the version string
        mVersionChannel = getChannel() + " " + getVersion();
    }

    return mVersionChannel;
}

//<FS:TS> Get version and channel in the format needed for FSDATA.
std::string LLVersionInfo::getChannelAndVersionFS() const
{
    static std::string sVersionChannelFS;
    if (sVersionChannelFS.empty())
    {
        // cache the version string
        std::ostringstream stream;
        stream << LL_VIEWER_CHANNEL << " "
               << LL_VIEWER_VERSION_MAJOR << "."
               << LL_VIEWER_VERSION_MINOR << "."
               << LL_VIEWER_VERSION_PATCH << " ("
               << LL_VIEWER_VERSION_BUILD << ")";
        sVersionChannelFS = stream.str();
    }

    return sVersionChannelFS;
}
//</FS:TS>

std::string LLVersionInfo::getChannel() const
{
    return mWorkingChannelName;
}

void LLVersionInfo::resetChannel(const std::string& channel)
{
    mWorkingChannelName = channel;
    mVersionChannel.clear(); // Reset version and channel string til next use.
}

LLVersionInfo::ViewerMaturity LLVersionInfo::getViewerMaturity() const
{
    ViewerMaturity maturity;

    std::string channel = getChannel();

    static const boost::regex is_test_channel("\\bTest\\b");
    static const boost::regex is_beta_channel("\\b(Beta|Develop)\\b");  // Develop is an alias for Beta
    static const boost::regex is_project_channel("\\bProject\\b");
    static const boost::regex is_release_channel("\\bRelease\\b");

    if (ll_regex_search(channel, is_release_channel))
    {
        maturity = RELEASE_VIEWER;
    }
    else if (ll_regex_search(channel, is_beta_channel))
    {
        maturity = BETA_VIEWER;
    }
    else if (ll_regex_search(channel, is_project_channel))
    {
        maturity = PROJECT_VIEWER;
    }
    else if (ll_regex_search(channel, is_test_channel))
    {
        maturity = TEST_VIEWER;
    }
    else
    {
        // <FS:Ansariel> Silence this warning
        //LL_WARNS() << "Channel '" << channel
        //           << "' does not follow naming convention, assuming Test"
        //           << LL_ENDL;
        // </FS:Ansariel>
        maturity = TEST_VIEWER;
    }
    return maturity;
}

// <FS:Beq> Add FS specific maturity grading
LLVersionInfo::FSViewerMaturity LLVersionInfo::getFSViewerMaturity() const
{
    FSViewerMaturity maturity;

    std::string channel = getChannel();

    static const boost::regex is_manual_channel("\\bManual(x64)?\\b");
    static const boost::regex is_beta_channel("\\bBeta(x64)?\\b");
    static const boost::regex is_alpha_channel("\\bAlpha(x64)?\\b");
    static const boost::regex is_release_channel("\\bRelease(x64)?\\b");
    static const boost::regex is_nightly_channel("\\bNightly(x64)?\\b");
    static const boost::regex is_streaming_channel("\\bStreaming\\b");

    if (ll_regex_search(channel, is_release_channel))
    {
        maturity = FSViewerMaturity::RELEASE_VIEWER;
    }
    else if (ll_regex_search(channel, is_beta_channel))
    {
        maturity = FSViewerMaturity::BETA_VIEWER;
    }
    else if (ll_regex_search(channel, is_alpha_channel))
    {
        maturity = FSViewerMaturity::ALPHA_VIEWER;
    }
    else if (ll_regex_search(channel, is_manual_channel))
    {
        maturity = FSViewerMaturity::MANUAL_VIEWER;
    }
    else if (ll_regex_search(channel, is_nightly_channel))
    {
        maturity = FSViewerMaturity::NIGHTLY_VIEWER;
    }
    else if (ll_regex_search(channel, is_streaming_channel))
    {
        maturity = FSViewerMaturity::STREAMING_VIEWER;
    }
    else
    {
        maturity = FSViewerMaturity::UNOFFICIAL_VIEWER;
    }
    return maturity;
}
// </FS:Beq>

std::string LLVersionInfo::getReleaseNotes() const
{
    return mReleaseNotes;
}

// [SL:KB] - Patch: Viewer-CrashReporting | Checked: 2011-05-08 (Catznip-2.6.0a) | Added: Catznip-2.6.0a
const char* getBuildPlatformString()
{
#if LL_WINDOWS
    #ifndef _WIN64
            return "Win32";
    #else
            return "Win64";
    #endif // _WIN64
#elif LL_SDL
            return "Linux64";
#elif LL_DARWIN
        #if ( defined(__amd64__) || defined(__x86_64__) )
            return "Darwin64";
        #else
            return "Darwin";
        #endif
#else
            return "Unknown";
#endif
}

std::string LLVersionInfo::getBuildPlatform() const
{
    std::string strPlatform = getBuildPlatformString();
    return strPlatform;
}
// [/SL:KB]


std::string LLVersionInfo::getBuildConfig() const
{
    return build_configuration;
}

//<FS:ND> return hash of HEAD
std::string LLVersionInfo::getGitHash()
{
    return LL_VIEWER_VERSION_GITHASH;
}

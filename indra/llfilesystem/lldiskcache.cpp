/**
 * @file lldiskcache.cpp
 * @brief The disk cache implementation.
 *
 * Note: Rather than keep the top level function comments up
 * to date in both the source and header files, I elected to
 * only have explicit comments about each function and variable
 * in the header - look there for details. The same is true for
 * description of how this code is supposed to work.
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2020, Linden Research, Inc.
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
#include "llapp.h"
#include "llassettype.h"
#include "lldir.h"
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>
#include <chrono>

#include "lldiskcache.h"

// <FS:Ansariel> Optimize asset simple disk cache
static const char* subdirs = "0123456789abcdef";

LLDiskCache::LLDiskCache(const std::string cache_dir,
                         const uintmax_t max_size_bytes,
                         const bool enable_cache_debug_info) :
    mCacheDir(cache_dir),
    mMaxSizeBytes(max_size_bytes),
    mEnableCacheDebugInfo(enable_cache_debug_info)
{
    mCacheFilenamePrefix = "sl_cache";

    LLFile::mkdir(cache_dir);

    // <FS:Ansariel> Optimize asset simple disk cache
    for (S32 i = 0; i < 16; i++)
    {
        std::string dirname = cache_dir + gDirUtilp->getDirDelimiter() + subdirs[i];
        LLFile::mkdir(dirname);
    }
    // </FS:Ansariel>
    // <FS:Beq> add static assets into the new cache after clear.
    // Only missing entries are copied on init, skiplist is setup
    // For everything we populate FS specific assets to allow future updates
    prepopulateCacheWithStatic();
    // </FS:Beq>
}

// WARNING: purge() is called by LLPurgeDiskCacheThread. As such it must
// NOT touch any LLDiskCache data without introducing and locking a mutex!

// Interaction through the filesystem itself should be safe. Let’s say thread
// A is accessing the cache file for reading/writing and thread B is trimming
// the cache. Let’s also assume using llifstream to open a file and
// boost::filesystem::remove are not atomic (which will be pretty much the
// case).

// Now, A is trying to open the file using llifstream ctor. It does some
// checks if the file exists and whatever else it might be doing, but has not
// issued the call to the OS to actually open the file yet. Now B tries to
// delete the file: If the file has been already marked as in use by the OS,
// deleting the file will fail and B will continue with the next file. A can
// safely continue opening the file. If the file has not yet been marked as in
// use, B will delete the file. Now A actually wants to open it, operation
// will fail, subsequent check via llifstream.is_open will fail, asset will
// have to be re-requested. (Assuming here the viewer will actually handle
// this situation properly, that can also happen if there is a file containing
// garbage.)

// Other situation: B is trimming the cache and A wants to read a file that is
// about to get deleted. boost::filesystem::remove does whatever it is doing
// before actually deleting the file. If A opens the file before the file is
// actually gone, the OS call from B to delete the file will fail since the OS
// will prevent this. B continues with the next file. If the file is already
// gone before A finally gets to open it, this operation will fail and the
// asset will have to be re-requested.
void LLDiskCache::purge()
{
    if (mEnableCacheDebugInfo)
    {
        LL_INFOS() << "Total dir size before purge is " << dirFileSize(mCacheDir) << LL_ENDL;
    }

    boost::system::error_code ec;
    auto start_time = std::chrono::high_resolution_clock::now();

    typedef std::pair<std::time_t, std::pair<uintmax_t, std::string>> file_info_t;
    std::vector<file_info_t> file_info;

#if LL_WINDOWS
    std::wstring cache_path(utf8str_to_utf16str(mCacheDir));
#else
    std::string cache_path(mCacheDir);
#endif
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        // <FS:Ansariel> Optimize asset simple disk cache
        //for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(cache_path, ec), {}))
        for (auto& entry : boost::make_iterator_range(boost::filesystem::recursive_directory_iterator(cache_path, ec), {}))
        {
            if (!ec.failed() && (boost::filesystem::is_regular_file(entry, ec) && !ec.failed()))
            {
                if (entry.path().string().find(mCacheFilenamePrefix) != std::string::npos)
                {
                    uintmax_t file_size = boost::filesystem::file_size(entry, ec);
                    if (ec.failed())
                    {
                        continue;
                    }
                    const std::string file_path = entry.path().string();
                    const std::time_t file_time = boost::filesystem::last_write_time(entry, ec);
                    if (ec.failed())
                    {
                        continue;
                    }

                    file_info.push_back(file_info_t(file_time, { file_size, file_path }));
                }
            }
        }
    }

    std::sort(file_info.begin(), file_info.end(), [](file_info_t& x, file_info_t& y)
    {
        return x.first > y.first;
    });

    LL_INFOS() << "Purging cache to a maximum of " << mMaxSizeBytes << " bytes" << LL_ENDL;

    // <FS:Beq> Extra accounting to track the retention of static assets
    //std::vector<bool> file_removed;
    std::vector<S32> file_removed;
    int keep{0};
    int del{0};
    int skip{0};
    // </FS:Beq>
    if (mEnableCacheDebugInfo)
    {
        file_removed.reserve(file_info.size());
    }
    uintmax_t file_size_total = 0;
    for (file_info_t& entry : file_info)
    {
        file_size_total += entry.second.first;

        bool should_remove = file_size_total > mMaxSizeBytes;
        // <FS> Make sure static assets are not eliminated
        S32 action{ should_remove ? 0 : 1 };
        if (should_remove)
        {
            auto uuid_as_string = gDirUtilp->getBaseFileName(entry.second.second, true);
            uuid_as_string = uuid_as_string.substr(mCacheFilenamePrefix.size() + 1, 36);// skip "sl_cache_" and trailing "_N"
            // LL_INFOS() << "checking UUID=" <<uuid_as_string<< LL_ENDL;
            if (std::find(mSkipList.begin(), mSkipList.end(), uuid_as_string) != mSkipList.end())
            {
                // this is one of our protected items so no purging
                should_remove = false;
                action = 2;
                updateFileAccessTime(entry.second.second); // force these to the front of the list next time so that purge size works 
            }
        }
        // </FS>
        if (mEnableCacheDebugInfo)
        {
            // <FS> Static asset stuff
            //file_removed.push_back(should_remove);
            file_removed.push_back(action);
        }
        if (should_remove)
        {
            boost::filesystem::remove(entry.second.second, ec);
            if (ec.failed())
            {
                LL_WARNS() << "Failed to delete cache file " << entry.second.second << ": " << ec.message() << LL_ENDL;
            }
        }
    }

    if (mEnableCacheDebugInfo)
    {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto execute_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Log afterward so it doesn't affect the time measurement
        // Logging thousands of file results can take hundreds of milliseconds
        for (size_t i = 0; i < file_info.size(); ++i)
        {
            const file_info_t& entry = file_info[i];
            // <FS> Static asset stuff
            //const bool removed = file_removed[i];
            //const std::string action = removed ? "DELETE:" : "KEEP:";
            std::string action{};
            switch (file_removed[i])
            {
                default:
                case 0:
                    action = "KEEP";
                    keep++;
                    break;
                case 1:
                    action = "DELETE";
                    del++;
                    break;
                case 2:
                    action = "STATIC";
                    skip++;
                    break;
            }
            // </FS>

            // have to do this because of LL_INFO/LL_END weirdness
            std::ostringstream line;

            line << action << "  ";
            line << entry.first << "  ";
            line << entry.second.first << "  ";
            line << entry.second.second;
            line << " (" << file_size_total << "/" << mMaxSizeBytes << ")";
            LL_INFOS() << line.str() << LL_ENDL;
        }

        LL_INFOS() << "Total dir size after purge is " << dirFileSize(mCacheDir) << LL_ENDL;
        LL_INFOS() << "Cache purge took " << execute_time << " ms to execute for " << file_info.size() << " files" << LL_ENDL;
        LL_INFOS() << "Deleted: " << del << " Skipped: " << skip << " Kept: " << keep << LL_ENDL;    // <FS:Beq/> Extra accounting to track the retention of static assets
    }
}

const std::string LLDiskCache::assetTypeToString(LLAssetType::EType at)
{
    /**
     * Make use of the handy C++17  feature that allows
     * for inline initialization of an std::map<>
     */
    typedef std::map<LLAssetType::EType, std::string> asset_type_to_name_t;
    asset_type_to_name_t asset_type_to_name =
    {
        { LLAssetType::AT_TEXTURE, "TEXTURE" },
        { LLAssetType::AT_SOUND, "SOUND" },
        { LLAssetType::AT_CALLINGCARD, "CALLINGCARD" },
        { LLAssetType::AT_LANDMARK, "LANDMARK" },
        { LLAssetType::AT_SCRIPT, "SCRIPT" },
        { LLAssetType::AT_CLOTHING, "CLOTHING" },
        { LLAssetType::AT_OBJECT, "OBJECT" },
        { LLAssetType::AT_NOTECARD, "NOTECARD" },
        { LLAssetType::AT_CATEGORY, "CATEGORY" },
        { LLAssetType::AT_LSL_TEXT, "LSL_TEXT" },
        { LLAssetType::AT_LSL_BYTECODE, "LSL_BYTECODE" },
        { LLAssetType::AT_TEXTURE_TGA, "TEXTURE_TGA" },
        { LLAssetType::AT_BODYPART, "BODYPART" },
        { LLAssetType::AT_SOUND_WAV, "SOUND_WAV" },
        { LLAssetType::AT_IMAGE_TGA, "IMAGE_TGA" },
        { LLAssetType::AT_IMAGE_JPEG, "IMAGE_JPEG" },
        { LLAssetType::AT_ANIMATION, "ANIMATION" },
        { LLAssetType::AT_GESTURE, "GESTURE" },
        { LLAssetType::AT_SIMSTATE, "SIMSTATE" },
        { LLAssetType::AT_LINK, "LINK" },
        { LLAssetType::AT_LINK_FOLDER, "LINK_FOLDER" },
        { LLAssetType::AT_MARKETPLACE_FOLDER, "MARKETPLACE_FOLDER" },
        { LLAssetType::AT_WIDGET, "WIDGET" },
        { LLAssetType::AT_PERSON, "PERSON" },
        { LLAssetType::AT_MESH, "MESH" },
        { LLAssetType::AT_SETTINGS, "SETTINGS" },
        { LLAssetType::AT_UNKNOWN, "UNKNOWN" }
    };

    asset_type_to_name_t::iterator iter = asset_type_to_name.find(at);
    if (iter != asset_type_to_name.end())
    {
        return iter->second;
    }

    return std::string("UNKNOWN");
}

// <FS:Ansariel> Optimize asset simple disk cache
//const std::string LLDiskCache::metaDataToFilepath(const std::string id,
//        LLAssetType::EType at,
//        const std::string extra_info)
//{
//    std::ostringstream file_path;
//
//    file_path << mCacheDir;
//    file_path << gDirUtilp->getDirDelimiter();
//    file_path << mCacheFilenamePrefix;
//    file_path << "_";
//    file_path << id;
//    file_path << "_";
//    file_path << (extra_info.empty() ? "0" : extra_info);
//    //file_path << "_";
//    //file_path << assetTypeToString(at); // see  SL-14210 Prune descriptive tag from new cache filenames
//                                          // for details of why it was removed. Note that if you put it
//                                          // back or change the format of the filename, the cache files
//                                          // files will be invalidated (and perhaps, more importantly,
//                                          // never deleted unless you delete them manually).
//    file_path << ".asset";
//
//    return file_path.str();
//}
const std::string LLDiskCache::metaDataToFilepath(const std::string& id,
    LLAssetType::EType at,
    const std::string& extra_info)
{
    return llformat("%s%s%c%s%s_%s_%s.asset", mCacheDir.c_str(), gDirUtilp->getDirDelimiter().c_str(), id[0], gDirUtilp->getDirDelimiter().c_str(), mCacheFilenamePrefix.c_str(), id.c_str(), (extra_info.empty() ? "0" : extra_info).c_str());
}
// </FS:Ansariel>

// <FS:Ansariel> Optimize asset simple disk cache
//void LLDiskCache::updateFileAccessTime(const std::string file_path)
void LLDiskCache::updateFileAccessTime(const std::string& file_path)
// </FS:Ansariel>
{
    /**
     * Threshold in time_t units that is used to decide if the last access time
     * time of the file is updated or not. Added as a precaution for the concern
     * outlined in SL-14582  about frequent writes on older SSDs reducing their
     * lifespan. I think this is the right place for the threshold value - rather
     * than it being a pref - do comment on that Jira if you disagree...
     *
     * Let's start with 1 hour in time_t units and see how that unfolds
     */
    const std::time_t time_threshold = 1 * 60 * 60;

    // current time
    const std::time_t cur_time = std::time(nullptr);

    boost::system::error_code ec;
#if LL_WINDOWS
    // file last write time
    const std::time_t last_write_time = boost::filesystem::last_write_time(utf8str_to_utf16str(file_path), ec);
    if (ec.failed())
    {
        LL_WARNS() << "Failed to read last write time for cache file " << file_path << ": " << ec.message() << LL_ENDL;
        return;
    }

    // delta between cur time and last time the file was written
    const std::time_t delta_time = cur_time - last_write_time;

    // we only write the new value if the time in time_threshold has elapsed
    // before the last one
    if (delta_time > time_threshold)
    {
        boost::filesystem::last_write_time(utf8str_to_utf16str(file_path), cur_time, ec);
    }
#else
    // file last write time
    const std::time_t last_write_time = boost::filesystem::last_write_time(file_path, ec);
    if (ec.failed())
    {
        LL_WARNS() << "Failed to read last write time for cache file " << file_path << ": " << ec.message() << LL_ENDL;
        return;
    }

    // delta between cur time and last time the file was written
    const std::time_t delta_time = cur_time - last_write_time;

    // we only write the new value if the time in time_threshold has elapsed
    // before the last one
    if (delta_time > time_threshold)
    {
        boost::filesystem::last_write_time(file_path, cur_time, ec);
    }
#endif

    if (ec.failed())
    {
        LL_WARNS() << "Failed to update last write time for cache file " << file_path << ": " << ec.message() << LL_ENDL;
    }
}

const std::string LLDiskCache::getCacheInfo()
{
    std::ostringstream cache_info;

    F32 max_in_mb = (F32)mMaxSizeBytes / (1024.0 * 1024.0);
    F32 percent_used = ((F32)dirFileSize(mCacheDir) / (F32)mMaxSizeBytes) * 100.0;

    cache_info << std::fixed;
    cache_info << std::setprecision(1);
    cache_info << "Max size " << max_in_mb << " MB ";
    cache_info << "(" << percent_used << "% used)";

    return cache_info.str();
}

// <FS:Beq> Copy static items into cache and add to the skip list that prevents their purging
// Note that there is no de-duplication nor other validation of the list.
void LLDiskCache::prepopulateCacheWithStatic()
{
    mSkipList.clear();

    std::vector<std::string> from_folders;
    from_folders.emplace_back(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "fs_static_assets"));
#ifdef OPENSIM
    from_folders.emplace_back(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "static_assets"));
#endif

    for (const auto& from_folder : from_folders)
    {
        if (gDirUtilp->fileExists(from_folder))
        {
            auto assets_to_copy = gDirUtilp->getFilesInDir(from_folder);
            for (auto from_asset_file : assets_to_copy)
            {
                from_asset_file = from_folder + gDirUtilp->getDirDelimiter() + from_asset_file;
                // we store static assets as UUID.asset_type the asset_type is not used in the current simple cache format
                auto uuid_as_string{ gDirUtilp->getBaseFileName(from_asset_file, true) };
                auto to_asset_file = metaDataToFilepath(uuid_as_string, LLAssetType::AT_UNKNOWN, std::string());
                if (!gDirUtilp->fileExists(to_asset_file))
                {
                    if (mEnableCacheDebugInfo)
                    {
                        LL_INFOS("LLDiskCache") << "Copying static asset " << from_asset_file << " to cache from " << from_folder << LL_ENDL;
                    }
                    if (!LLFile::copy(from_asset_file, to_asset_file))
                    {
                        LL_WARNS("LLDiskCache") << "Failed to copy " << from_asset_file << " to " << to_asset_file << LL_ENDL;
                    }
                }
                if (std::find(mSkipList.begin(), mSkipList.end(), uuid_as_string) == mSkipList.end())
                {
                    if (mEnableCacheDebugInfo)
                    {
                        LL_INFOS("LLDiskCache") << "Adding " << uuid_as_string << " to skip list" << LL_ENDL;
                    }
                    mSkipList.emplace_back(uuid_as_string);
                }
            }
        }
    }
}
// </FS:Beq>

void LLDiskCache::clearCache()
{
    LL_INFOS() << "clearing cache " << mCacheDir << LL_ENDL;
    /**
     * See notes on performance in dirFileSize(..) - there may be
     * a quicker way to do this by operating on the parent dir vs
     * the component files but it's called infrequently so it's
     * likely just fine
     */
    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring cache_path(utf8str_to_utf16str(mCacheDir));
#else
    std::string cache_path(mCacheDir);
#endif
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
    LL_INFOS() << "is a directory: " << mCacheDir << LL_ENDL;
        // <FS:Ansariel> Optimize asset simple disk cache
        //for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(cache_path, ec), {}))
        // <FS:TS> FIRE-31070: Crash on clearing cache on macOS and Linux
        //         On Unix-like operating systems, the recursive_directory_iterator gets very unhappy if you
        //         delete a file out from under it in a for loop. This restructuring to a while loop and
        //         manually incrementing the iterator avoids the problem. Note that the iterator must be
        //         incremented *before* deleting the file.
        boost::filesystem::recursive_directory_iterator entry(cache_path, ec);
        boost::filesystem::recursive_directory_iterator cache_end;
        while (entry != cache_end)
        {
            const boost::filesystem::path& remove_entry = entry->path();
            if (boost::filesystem::is_regular_file(remove_entry, ec) && !ec.failed())
            {
                if (remove_entry.string().find(mCacheFilenamePrefix) != std::string::npos)
                {
                    const boost::filesystem::path remove_path = remove_entry;
                    ++entry;
                    boost::filesystem::remove(remove_path, ec);
                    if (ec.failed())
                    {
                        LL_WARNS() << "Failed to delete cache file " << remove_path.string() << ": " << ec.message() << LL_ENDL;
                    }
                }
                else
                {
                    ++entry;
                }
            }
            else
            {
                ++entry;
            }
            // </FS:TS> FIRE-31070
        }
        // <FS:Beq> add static assets into the new cache after clear
    LL_INFOS() << "prepopulating new cache " << LL_ENDL;
        prepopulateCacheWithStatic();
    }
    LL_INFOS() << "Cleared cache " << mCacheDir << LL_ENDL;
}

void LLDiskCache::removeOldVFSFiles()
{
    //VFS files won't be created, so consider removing this code later
    static const char CACHE_FORMAT[] = "inv.llsd";
    static const char DB_FORMAT[] = "db2.x";

    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring cache_path(utf8str_to_utf16str(gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "")));
#else
    std::string cache_path(gDirUtilp->getExpandedFilename(LL_PATH_CACHE, ""));
#endif
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(cache_path, ec), {}))
        {
            if (boost::filesystem::is_regular_file(entry, ec) && !ec.failed())
            {
                if ((entry.path().string().find(CACHE_FORMAT) != std::string::npos) ||
                    (entry.path().string().find(DB_FORMAT) != std::string::npos))
                {
                    boost::filesystem::remove(entry, ec);
                    if (ec.failed())
                    {
                        LL_WARNS() << "Failed to delete cache file " << entry << ": " << ec.message() << LL_ENDL;
                    }
                }
            }
        }
    }
}

uintmax_t LLDiskCache::dirFileSize(const std::string dir)
{
    uintmax_t total_file_size = 0;

    /**
     * There may be a better way that works directly on the folder (similar to
     * right clicking on a folder in the OS and asking for size vs right clicking
     * on all files and adding up manually) but this is very fast - less than 100ms
     * for 10,000 files in my testing so, so long as it's not called frequently,
     * it should be okay. Note that's it's only currently used for logging/debugging
     * so if performance is ever an issue, optimizing this or removing it altogether,
     * is an easy win.
     */
    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring dir_path(utf8str_to_utf16str(dir));
#else
    std::string dir_path(dir);
#endif
    if (boost::filesystem::is_directory(dir_path, ec) && !ec.failed())
    {
        // <FS:Ansariel> Optimize asset simple disk cache
        //for (auto& entry : boost::make_iterator_range(boost::filesystem::directory_iterator(dir_path, ec), {}))
        for (auto& entry : boost::make_iterator_range(boost::filesystem::recursive_directory_iterator(dir_path, ec), {}))
        {
            if (!ec.failed() && (boost::filesystem::is_regular_file(entry, ec) && !ec.failed()))
            {
                if (entry.path().string().find(mCacheFilenamePrefix) != std::string::npos)
                {
                    uintmax_t file_size = boost::filesystem::file_size(entry, ec);
                    if (!ec.failed())
                    {
                        total_file_size += file_size;
                    }
                }
            }
        }
    }

    return total_file_size;
}

LLPurgeDiskCacheThread::LLPurgeDiskCacheThread() :
    LLThread("PurgeDiskCacheThread", nullptr)
{
}

void LLPurgeDiskCacheThread::run()
{
    constexpr std::chrono::seconds CHECK_INTERVAL{60};

    while (LLApp::instance()->sleep(CHECK_INTERVAL))
    {
        LLDiskCache::instance().purge();
    }
}

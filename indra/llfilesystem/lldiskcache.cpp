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
#include <chrono>

#include "lldiskcache.h"

 /**
  * The prefix inserted at the start of a cache file filename to
  * help identify it as a cache file. It's probably not required
  * (just the presence in the cache folder is enough) but I am
  * paranoid about the cache folder being set to something bad
  * like the users' OS system dir by mistake or maliciously and
  * this will help to offset any damage if that happens.
  */
static const std::string CACHE_FILENAME_PREFIX("sl_cache");

std::string LLDiskCache::sCacheDir;

// <FS:Ansariel> Optimize asset simple disk cache
static const char* subdirs = "0123456789abcdef";

LLDiskCache::LLDiskCache(const std::string& cache_dir,
                         const uintmax_t max_size_bytes,
                         const bool enable_cache_debug_info
// <FS:Beq> Add High/Low water mark support
                         ,const F32 highwater_mark_percent
                         ,const F32 lowwater_mark_percent
// </FS:Beq>
                         ) :
    mMaxSizeBytes(max_size_bytes),
    mEnableCacheDebugInfo(enable_cache_debug_info)
{
    sCacheDir = cache_dir;
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
        LL_INFOS() << "Total dir size before purge is " << dirFileSize(sCacheDir) << LL_ENDL;
    }

    boost::system::error_code ec;
    auto start_time = std::chrono::high_resolution_clock::now();

    typedef std::pair<std::time_t, std::pair<uintmax_t, std::string>> file_info_t;
    std::vector<file_info_t> file_info;

#if LL_WINDOWS
    std::wstring cache_path(utf8str_to_utf16str(sCacheDir));
#else
    std::string cache_path(sCacheDir);
#endif
    uintmax_t file_size_total = 0; // <FS:Beq/> try to make simple cache less naive.
    
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        // <FS:Ansariel> Optimize asset simple disk cache
        //boost::filesystem::directory_iterator iter(cache_path, ec);
        //while (iter != boost::filesystem::directory_iterator() && !ec.failed())
        boost::filesystem::recursive_directory_iterator iter(cache_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
        // </FS:Ansariel>
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                if ((*iter).path().string().find(CACHE_FILENAME_PREFIX) != std::string::npos)
                {
                    uintmax_t file_size = boost::filesystem::file_size(*iter, ec);
                    if (ec.failed())
                    {
                        continue;
                    }
                    const std::string file_path = (*iter).path().string();
                    const std::time_t file_time = boost::filesystem::last_write_time(*iter, ec);
                    if (ec.failed())
                    {
                        continue;
                    }
                    file_size_total += file_size; // <FS:Beq/> try to make simple cache less naive.

                    file_info.push_back(file_info_t(file_time, { file_size, file_path }));
                }
            }
            iter.increment(ec);
        }
    }

    // <FS:Beq> add high water/low water thresholds to reduce the churn in the cache.
    LL_DEBUGS("LLDiskCache") << "Cache is " << (int)(((F32)file_size_total)/mMaxSizeBytes*100.0) << "% full" << LL_ENDL;
    if( file_size_total < mMaxSizeBytes * (mHighPercent/100) )
    {
        // Nothing to do here 
        LL_DEBUGS("LLDiskCache") << "Not exceded high water - do nothing" << LL_ENDL;
        return;
    }
    // If we reach here we are above the trigger level so we must purge until we've removed enough to take us down to the low water mark.
    // </FS:Beq>
    std::sort(file_info.begin(), file_info.end(), [](file_info_t& x, file_info_t& y)
    {
        return x.first < y.first; // <FS:Beq/> sort oldest to newest, to we can remove the oldest files first.
    });


    // <FS:Beq> add high water/low water thresholds to reduce the churn in the cache.
    auto target_size = (uintmax_t)(mMaxSizeBytes * (mLowPercent/100));
    LL_INFOS() << "Purging cache to a maximum of " << target_size << " bytes" << LL_ENDL;
    // </FS:Beq>

    // <FS:Beq> Extra accounting to track the retention of static assets
    //std::vector<bool> file_removed;
    enum class purge_action { delete_file=0, keep_file, skip_file };
    std::map<std::string,purge_action> file_removed;
    auto keep{file_info.size()};
    auto del{0};
    auto skip{0};
    // </FS:Beq>
    // <FS:Beq> revised purge logic to track amount removed not retained to shortern loop
    // uintmax_t file_size_total = 0;
    // if (mEnableCacheDebugInfo)
    // {
    //     file_removed.reserve(file_info.size());
    // }
    // uintmax_t file_size_total = 0;
    // for (file_info_t& entry : file_info)
    // {
    //     file_size_total += entry.second.first;

    //     bool should_remove = file_size_total > mMaxSizeBytes;
    //     if (mEnableCacheDebugInfo)
    //     {
    //         file_removed.push_back(should_remove);
    //     }
    uintmax_t deleted_size_total = 0;
    for (const file_info_t& entry : file_info)
    {
        // first check if we still need to delete more files
        bool should_remove = (file_size_total - deleted_size_total) > target_size;

        // <FS> Make sure static assets are not eliminated
        auto action{ should_remove ? purge_action::delete_file : purge_action::keep_file };
        if (!should_remove)
        {
            break;
        }

        auto this_file_size = entry.second.first;
        deleted_size_total += this_file_size;
        auto uuid_as_string = gDirUtilp->getBaseFileName(entry.second.second, true);
        uuid_as_string      = uuid_as_string.substr(CACHE_FILENAME_PREFIX.size() + 1, 36);  // skip "sl_cache_" and trailing "_N"
        // LL_INFOS() << "checking UUID=" <<uuid_as_string<< LL_ENDL;
        if (std::find(mSkipList.begin(), mSkipList.end(), uuid_as_string) != mSkipList.end())
        {
            // this is one of our protected items so no purging
            should_remove = false;
            action = purge_action::skip_file;
            boost::filesystem::last_write_time(entry.second.second, ec);  // force these to the front of the list next time so that purge size works 
            skip++;
        }
        else{
            del++;
        }
        keep--;
        if (mEnableCacheDebugInfo)
        {
            file_removed.emplace(entry.second.second, action);
        }
    // </FS>
        if (should_remove)
        {
            boost::filesystem::remove(entry.second.second, ec);
            if (ec.failed())
            {
                LL_WARNS() << "Failed to delete cache file " << entry.second.second << ": " << ec.message() << LL_ENDL;
            }
        }
    }
// <FS:Beq> update the debug logging to be more useful
    auto end_time = std::chrono::high_resolution_clock::now();
    auto execute_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
// </FS:Beq>
    if (mEnableCacheDebugInfo)
    {
        // <FS:Beq> update the debug logging to be more useful
        // auto end_time = std::chrono::high_resolution_clock::now();
        // auto execute_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        // </FS:Beq>

        // Log afterward so it doesn't affect the time measurement
        // Logging thousands of file results can take hundreds of milliseconds
        uintmax_t deleted_so_far{ 0 }; // <FS:Beq/> update the debug logging to be more useful
        for (size_t i = 0; i < file_info.size(); ++i)
        {
            const file_info_t& entry = file_info[i];
            // <FS> Static asset stuff
            deleted_so_far += entry.second.first; // <FS:Beq/> update the debug logging to be more useful
            //const bool removed = file_removed[i];
            //const std::string action = removed ? "DELETE:" : "KEEP:";
            std::string action{};

            // Check if the file exists in the map
            auto& filename{ entry.second.second };
            if (file_removed.find(filename) != file_removed.end()) {
            // File found in the map, retrieve the corresponding enum value
            switch (file_removed[filename])
            {
                case purge_action::delete_file:
                    action = "DELETE";
                    del++;
                break;
                case purge_action::skip_file:
                    action = "STATIC";
                    skip++;
                break;
                default:
                // Handle any unexpected enum value
                    action = "UNKNOWN";
                break;
            }
            }
            else 
            {
                action = "KEEP";
            }            
            // </FS>

            // have to do this because of LL_INFO/LL_END weirdness
            std::ostringstream line;

            line << action << "  ";
            line << entry.first << "  ";
            line << entry.second.first << "  ";
            line << entry.second.second;
            line << " (" << file_size_total - deleted_so_far << "/" << mMaxSizeBytes << ")"; // <FS:Beq/> update the debug logging to be more useful
            LL_INFOS() << line.str() << LL_ENDL;
        }
// <FS:Beq> make the summary stats more easily enabled.
    }
    // <FS:Beq> update the debug logging to be more useful
        // LL_INFOS() << "Total dir size after purge is " << dirFileSize(sCacheDir) << LL_ENDL;
        // LL_INFOS() << "Cache purge took " << execute_time << " ms to execute for " << file_info.size() << " files" << LL_ENDL;

    auto newCacheSize = updateCacheSize(file_size_total - deleted_size_total); 
    LL_INFOS("LLDiskCache") << "Total dir size after purge is " << newCacheSize << LL_ENDL; 
    LL_INFOS("LLDiskCache") << "Cache purge took " << execute_time << " ms to execute for " << file_info.size() << " files" << LL_ENDL;
// </FS:Beq>
    LL_INFOS("LLDiskCache") << "Deleted: " << del << " Skipped: " << skip << " Kept: " << keep << LL_ENDL;    // <FS:Beq/> Extra accounting to track the retention of static assets
    LL_INFOS("LLDiskCache") << "Total of " << deleted_size_total << " bytes removed." << LL_ENDL;    // <FS:Beq/> Extra accounting to track the retention of static assets
    // } <FS:Beq/> this bracket was moved up a few lines.
}

const std::string LLDiskCache::metaDataToFilepath(const LLUUID& id, LLAssetType::EType at)
{
    return llformat("%s%s%s_%s_0.asset", sCacheDir.c_str(), gDirUtilp->getDirDelimiter().c_str(), CACHE_FILENAME_PREFIX.c_str(), id.asString().c_str());
}

const std::string LLDiskCache::getCacheInfo()
{
    LL_PROFILE_ZONE_SCOPED; // <FS:Beq/> add some instrumentation
    std::ostringstream cache_info;

    F32 max_in_mb = (F32)mMaxSizeBytes / (1024.0f * 1024.0f);
    // <FS:Beq> stall prevention. We still need to make sure this initialised when called at startup.
    F32 percent_used;
    if (mStoredCacheSize > 0)
    {
        percent_used = ((F32)mStoredCacheSize / (F32)mMaxSizeBytes) * 100.0f;
    }
    else
    {
        percent_used = ((F32)dirFileSize(sCacheDir) / (F32)mMaxSizeBytes) * 100.0f; 
    }
    // </FS:Beq>
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
                LLUUID uuid{ uuid_as_string };
                auto to_asset_file = metaDataToFilepath(uuid, LLAssetType::AT_UNKNOWN);
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
    LL_INFOS() << "clearing cache " << sCacheDir << LL_ENDL;
    /**
     * See notes on performance in dirFileSize(..) - there may be
     * a quicker way to do this by operating on the parent dir vs
     * the component files but it's called infrequently so it's
     * likely just fine
     */
    boost::system::error_code ec;
#if LL_WINDOWS
    std::wstring cache_path(utf8str_to_utf16str(sCacheDir));
#else
    std::string cache_path(sCacheDir);
#endif
    if (boost::filesystem::is_directory(cache_path, ec) && !ec.failed())
    {
        // <FS:Ansariel> Optimize asset simple disk cache
        //boost::filesystem::directory_iterator iter(cache_path, ec);
        //while (iter != boost::filesystem::directory_iterator() && !ec.failed())
        boost::filesystem::recursive_directory_iterator iter(cache_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
        // </FS:Ansariel>
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                if ((*iter).path().string().find(CACHE_FILENAME_PREFIX) != std::string::npos)
                {
                    boost::filesystem::remove(*iter, ec);
                    if (ec.failed())
                    {
                        LL_WARNS() << "Failed to delete cache file " << *iter << ": " << ec.message() << LL_ENDL;
                    }
                }
            }
            iter.increment(ec);
        }
        // <FS:Beq> add static assets into the new cache after clear
    LL_INFOS() << "prepopulating new cache " << LL_ENDL;
        prepopulateCacheWithStatic();
    }
    LL_INFOS() << "Cleared cache " << sCacheDir << LL_ENDL;
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
        boost::filesystem::directory_iterator iter(cache_path, ec);
        while (iter != boost::filesystem::directory_iterator() && !ec.failed())
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                if (((*iter).path().string().find(CACHE_FORMAT) != std::string::npos) ||
                    ((*iter).path().string().find(DB_FORMAT) != std::string::npos))
                {
                    boost::filesystem::remove(*iter, ec);
                    if (ec.failed())
                    {
                        LL_WARNS() << "Failed to delete cache file " << *iter << ": " << ec.message() << LL_ENDL;
                    }
                }
            }
            iter.increment(ec);
        }
    }
}

// <FS:Beq> Lets not scan every single time if we can avoid it eh?
// uintmax_t LLDiskCache::dirFileSize(const std::string& dir)
// {
uintmax_t LLDiskCache::updateCacheSize(const uintmax_t newsize)
{
    mStoredCacheSize = newsize;
    mLastScanTime = system_clock::now();
    return mStoredCacheSize;
}

uintmax_t LLDiskCache::dirFileSize(const std::string& dir, bool force)
{
    using namespace std::chrono;
    const seconds cache_duration{ 120 };// A rather arbitrary number. it takes 5 seconds+ on a fast drive to scan 80K+ items. purge runs every minute and will update. so 120 should mean we never need a superfluous cache scan.

    const auto current_time = system_clock::now();

    const auto time_difference = duration_cast<seconds>(current_time - mLastScanTime);

    // Check if the cached result can be used
    if( !force && time_difference < cache_duration )
    {
        LL_DEBUGS("LLDiskCache") << "Using cached result: " << mStoredCacheSize << LL_ENDL;
        return mStoredCacheSize;
    }
// </FS:Beq>
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
        //boost::filesystem::directory_iterator iter(dir_path, ec);
        //while (iter != boost::filesystem::directory_iterator() && !ec.failed())
        boost::filesystem::recursive_directory_iterator iter(dir_path, ec);
        while (iter != boost::filesystem::recursive_directory_iterator() && !ec.failed())
            // </FS:Ansariel>
        {
            if (boost::filesystem::is_regular_file(*iter, ec) && !ec.failed())
            {
                if ((*iter).path().string().find(CACHE_FILENAME_PREFIX) != std::string::npos)
                {
                    uintmax_t file_size = boost::filesystem::file_size(*iter, ec);
                    if (!ec.failed())
                    {
                        total_file_size += file_size;
                    }
                }
            }
            iter.increment(ec);
        }
    }

// <FS:Beq> Lets not scan every single time if we can avoid it eh?
    // return total_file_size;
    return updateCacheSize(total_file_size);
// </FS:Beq>
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

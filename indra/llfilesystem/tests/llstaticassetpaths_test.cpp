/** Static cache population, protected assets and purge accounting in Unicode paths. */
#include "linden_common.h"
// <FS:Beq> Extended non-ASCII path test coverage.
#include "path_test_support.h"
#include "llassettype.h"
#include "lldiskcache.h"
#include "llfile.h"

namespace tut
{
    struct staticassetpaths_data
    {
        nonascii::Tree tree;
        nonascii::Dirs dirs;
        const LLApp::EAppStatus previous = nonascii::AppStatus::status();
        ~staticassetpaths_data()
        {
            LLDiskCache::deleteSingleton();
            nonascii::AppStatus::setStatus(previous);
        }
    };
    using staticassetpaths_group = test_group<staticassetpaths_data>;
    using staticassetpaths_object = staticassetpaths_group::object;
    staticassetpaths_group staticassetpaths_tests("LLStaticAssetPaths");

    template<> template<>
    void staticassetpaths_object::test<1>()
    {
        set_test_name("static assets survive clear and purge with accurate byte accounting");
        namespace fs = nonascii::fs;
        const auto profile = std::getenv("LL_TEST_ASCII_PROFILE") ? fs::path("ASCII") : nonascii::name();
        const auto install = tree.path / profile / "install";
        const auto cache = tree.path / profile / "cache";
        const auto distribution = install / "app_settings" / "fs_static_assets";
        fs::create_directories(distribution);
        const std::string id = "00000000-0000-0000-0000-000000000001";
        const auto source = distribution / (id + ".asset");
        nonascii::write(source, std::string(600, 'A'));
        dirs.configure(cache, install);
        nonascii::AppStatus::setStatus(LLApp::APP_STATUS_RUNNING);
        const bool debug = std::getenv("LL_TEST_CACHE_DEBUG") != nullptr;
        LLDiskCache::initParamSingleton(nonascii::utf8(cache), uintmax_t(1024), debug, 95.f, 70.f);
        auto* disk = LLDiskCache::getInstance();
        const auto protected_asset = cache / "0" / ("sl_cache_" + id + "_0.asset");
        ensure_equals("static asset copied through native Unicode install/cache paths",
                      nonascii::read(protected_asset), std::string(600, 'A'));
        nonascii::write(source, std::string(600, 'B'));
        disk->prepopulateCacheWithStatic();
        ensure_equals("existing cached asset not overwritten", nonascii::read(protected_asset), std::string(600, 'A'));
        const auto unrelated = cache / "0" / "keep.txt";
        nonascii::write(unrelated, "keep");
        disk->clearCache();
        ensure_equals("clear repopulates current static distribution", nonascii::read(protected_asset), std::string(600, 'B'));
        ensure_equals("clear retains unrelated data", nonascii::read(unrelated), std::string("keep"));

        const auto old_asset = cache / "1" / "sl_cache_10000000-0000-0000-0000-000000000001_0.asset";
        const auto new_asset = cache / "2" / "sl_cache_20000000-0000-0000-0000-000000000001_0.asset";
        nonascii::write(old_asset, std::string(600, 'C'));
        nonascii::write(new_asset, std::string(600, 'D'));
        const auto now = fs::file_time_type::clock::now();
        fs::last_write_time(protected_asset, now - std::chrono::hours(4));
        fs::last_write_time(old_asset, now - std::chrono::hours(3));
        fs::last_write_time(new_asset, now - std::chrono::hours(2));
        nonascii::LogCapture logs;
        disk->purge();

        // Check the independent postconditions together: one failure must not
        // conceal the other known-risk paths in this singleton's lifecycle.
        std::string failures;
        if (!fs::exists(protected_asset)) failures += "protected static asset deleted; ";
        else if (fs::last_write_time(protected_asset) < now - std::chrono::minutes(1))
            failures += "protected asset timestamp was not refreshed; ";
        uintmax_t actual_size = 0;
        for (const auto& entry : fs::recursive_directory_iterator(cache))
        {
            if (entry.is_regular_file() && nonascii::utf8(entry.path().filename()).starts_with("sl_cache_"))
                actual_size += entry.file_size();
        }
        if (actual_size > uintmax_t(1024 * 0.70))
            failures += "actual remaining asset bytes exceed low-water target: " + std::to_string(actual_size) + "; ";
        if (fs::exists(old_asset)) failures += "oldest removable asset retained; ";
        if (nonascii::read(unrelated) != "keep") failures += "unrelated data changed; ";
        ensure(failures, failures.empty());
        ensure_equals("only protected bytes remain", actual_size, uintmax_t(600));
        ensure_contains("accurate purge summary", logs.messages->text,
                        "Deleted: 2 Skipped: 1 Kept: 0 Failed: 0 Missing: 0");
        ensure_contains("actual deletion byte count", logs.messages->text, "Total of 1200 bytes removed.");
        ensure_contains("cached size matches native bytes", disk->getCacheInfo(), "58.6% used");
        if (debug)
        {
            ensure_contains("protected detail retains bytes", logs.messages->text, "(1800/1024)");
            ensure_contains("final deletion detail", logs.messages->text, "(600/1024)");
        }

        auto native_bytes = [&]()
        {
            uintmax_t bytes = 0;
            for (const auto& entry : fs::recursive_directory_iterator(cache))
                if (entry.is_regular_file() && nonascii::utf8(entry.path().filename()).starts_with("sl_cache_"))
                    bytes += entry.file_size();
            return bytes;
        };

        // A protected floor can make the target unreachable. Report retained
        // bytes honestly and terminate without deleting the protected asset.
        disk->setMaxSizeBytes(512);
        logs.messages->text.clear();
        disk->purge();
        ensure_equals("protected floor retained", native_bytes(), uintmax_t(600));
        ensure_contains("protected floor summary", logs.messages->text,
                        "Deleted: 0 Skipped: 1 Kept: 0 Failed: 0 Missing: 0");
        ensure_contains("protected floor cached size", disk->getCacheInfo(), "117.2% used");

        // No protected entries on disk: oldest-first behavior still applies.
        disk->setMaxSizeBytes(1024);
        fs::remove(protected_asset);
        nonascii::write(old_asset, std::string(600, 'C'));
        nonascii::write(new_asset, std::string(600, 'D'));
        fs::last_write_time(old_asset, now - std::chrono::hours(3));
        fs::last_write_time(new_asset, now - std::chrono::hours(2));
        logs.messages->text.clear();
        disk->purge();
        ensure("oldest unprotected asset removed", !fs::exists(old_asset));
        ensure("newer asset below low-water target retained", fs::exists(new_asset));
        ensure_equals("no-protected native bytes", native_bytes(), uintmax_t(600));
        ensure_contains("no-protected summary", logs.messages->text,
                        "Deleted: 1 Skipped: 0 Kept: 1 Failed: 0 Missing: 0");
        logs.messages->text.clear();
        disk->purge();
        ensure("below-high-water asset retained", fs::exists(new_asset));
        ensure_does_not_contain("no purge below high-water threshold", logs.messages->text, "Deleted:");

#if LL_WINDOWS
        // Sharing restrictions deterministically reject deletion, regardless
        // of the account's ACL privileges. Other eligible files must be tried.
        disk->clearCache();
        nonascii::write(old_asset, std::string(600, 'C'));
        nonascii::write(new_asset, std::string(600, 'D'));
        fs::last_write_time(protected_asset, now - std::chrono::hours(4));
        fs::last_write_time(old_asset, now - std::chrono::hours(3));
        fs::last_write_time(new_asset, now - std::chrono::hours(2));
        std::error_code lock_error;
        LLFile locked(old_asset, LLFile::in | LLFile::exclusive, lock_error);
        ensure("lock oldest asset against deletion", bool(locked) && !lock_error);
        ensure_equals("locked metadata remains readable", fs::file_size(old_asset), uintmax_t(600));
        logs.messages->text.clear();
        disk->purge();
        ensure("failed deletion retains native file", fs::exists(old_asset));
        ensure("later eligible file deleted", !fs::exists(new_asset));
        ensure_equals("failed deletion bytes retained", native_bytes(), uintmax_t(1200));
        ensure_contains("failed deletion summary", logs.messages->text,
                        "Deleted: 1 Skipped: 1 Kept: 0 Failed: 1 Missing: 0");
        ensure_contains("failed deletion warning path", logs.messages->text, nonascii::utf8(old_asset));
        ensure_contains("only actual removal counted", logs.messages->text, "Total of 600 bytes removed.");
        ensure_contains("failed deletion cached size", disk->getCacheInfo(), "117.2% used");
        locked.close();
        disk->purge();
        ensure_equals("retry after unlocking reaches target", native_bytes(), uintmax_t(600));
#endif

        // The existing scan-complete log is a deterministic observation point:
        // remove one candidate there, before the deletion loop, without sleeps
        // or a production test hook. It must be classified as already absent.
        disk->clearCache();
        nonascii::write(old_asset, std::string(600, 'C'));
        nonascii::write(new_asset, std::string(600, 'D'));
        fs::last_write_time(protected_asset, now - std::chrono::hours(4));
        fs::last_write_time(old_asset, now - std::chrono::hours(3));
        fs::last_write_time(new_asset, now - std::chrono::hours(2));
        bool observed_scan = false, removed_elsewhere = false;
        std::error_code disappearance_error;
        logs.messages->text.clear();
        logs.messages->on_message = [&](const std::string& message)
        {
            if (!observed_scan && message.find("Purging cache to a maximum of ") != std::string::npos)
            {
                observed_scan = true;
                removed_elsewhere = fs::remove(old_asset, disappearance_error);
            }
        };
        disk->purge();
        logs.messages->on_message = {};
        ensure("candidate removed after scan before purge", observed_scan && removed_elsewhere && !disappearance_error);
        ensure_equals("already-absent accounting reaches target", native_bytes(), uintmax_t(600));
        ensure_contains("already-absent summary", logs.messages->text,
                        "Deleted: 1 Skipped: 1 Kept: 0 Failed: 0 Missing: 1");
        ensure_contains("already-absent bytes separate", logs.messages->text, "Already absent files account for 600 bytes.");
        ensure_contains("already-absent not counted as deletion", logs.messages->text, "Total of 600 bytes removed.");
        ensure_contains("already-absent cached size", disk->getCacheInfo(), "58.6% used");
    }
}
// </FS:Beq>

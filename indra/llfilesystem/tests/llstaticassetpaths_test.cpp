/** Static cache population, protected assets and purge accounting in Unicode paths. */
#include "linden_common.h"
// <FS:Beq> Extended non-ASCII path test coverage.
#include "path_test_support.h"
#include "llassettype.h"
#include "lldiskcache.h"

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
        set_test_name("Unicode static assets survive clear and purge with accurate byte accounting");
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
        LLDiskCache::initParamSingleton(nonascii::utf8(cache), uintmax_t(1024), false, 95.f, 70.f);
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
    }
}
// </FS:Beq>

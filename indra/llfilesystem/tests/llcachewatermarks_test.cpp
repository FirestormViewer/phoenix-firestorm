/** Startup cache watermarks and exact purge boundaries. */
#include "linden_common.h"
// <FS:Beq> Honor configured cache watermarks from the first purge.
#include "path_test_support.h"
#include "llassettype.h"
#include "lldiskcache.h"

namespace tut
{
    struct cachewatermarks_data
    {
        nonascii::Tree tree;
        nonascii::Dirs dirs;
        const LLApp::EAppStatus previous = nonascii::AppStatus::status();
        ~cachewatermarks_data()
        {
            LLDiskCache::deleteSingleton();
            nonascii::AppStatus::setStatus(previous);
        }
    };
    using cachewatermarks_group = test_group<cachewatermarks_data>;
    using cachewatermarks_object = cachewatermarks_group::object;
    cachewatermarks_group cachewatermarks_tests("LLCacheWatermarks");

    template<> template<>
    void cachewatermarks_object::test<1>()
    {
        set_test_name("constructor watermarks govern the first purge and exact byte boundaries");
        namespace fs = nonascii::fs;
        const auto profile = tree.path / nonascii::name();
        const auto cache = profile / "cache";
        fs::create_directory(profile);
        dirs.configure(cache, profile / "install");
        nonascii::AppStatus::setStatus(LLApp::APP_STATUS_RUNNING);

        // Both values are below the default low-water mark. Applying setters
        // against the defaults in the wrong order would also break this pair.
        LLDiskCache::initParamSingleton(nonascii::utf8(cache), uintmax_t(1000), false, 60.f, 40.f);
        auto* disk = LLDiskCache::getInstance();
        const auto oldest = cache / "0" / "sl_cache_00000000-0000-0000-0000-000000000001_0.asset";
        const auto newest = cache / "1" / "sl_cache_10000000-0000-0000-0000-000000000001_0.asset";
        const auto now = fs::file_time_type::clock::now();
        nonascii::write(oldest, std::string(200, 'A'));
        nonascii::write(newest, std::string(399, 'B'));
        fs::last_write_time(oldest, now - std::chrono::hours(2));
        fs::last_write_time(newest, now - std::chrono::hours(1));
        nonascii::LogCapture logs;

        disk->purge();
        ensure("below high-water mark retains oldest asset", fs::exists(oldest));
        ensure_equals("below high-water mark retains all bytes",
                      fs::file_size(oldest) + fs::file_size(newest), uintmax_t(599));
        ensure_does_not_contain("no purge below configured high-water mark", logs.messages->text, "Deleted:");

        nonascii::write(newest, std::string(400, 'B'));
        fs::last_write_time(newest, now - std::chrono::hours(1));
        logs.messages->text.clear();
        disk->purge();
        ensure("exact configured high-water mark triggers oldest-first purge", !fs::exists(oldest));
        ensure("exact configured low-water mark stops purge", fs::exists(newest));
        ensure_equals("remaining bytes equal configured low-water target", fs::file_size(newest), uintmax_t(400));
        ensure_contains("unequal asset sizes counted accurately", logs.messages->text,
                        "Deleted: 1 Skipped: 0 Kept: 1 Failed: 0 Missing: 0");
        ensure_contains("only deleted bytes reported", logs.messages->text, "Total of 200 bytes removed.");
        ensure_contains("cached accounting matches actual remaining bytes", disk->getCacheInfo(), "40.0% used");

        logs.messages->text.clear();
        disk->purge();
        ensure_equals("repeat purge retains low-water bytes", fs::file_size(newest), uintmax_t(400));
        ensure_does_not_contain("no purge at configured low-water mark", logs.messages->text, "Deleted:");
    }
}
// </FS:Beq>

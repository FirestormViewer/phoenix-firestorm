/**
 * @file lldiskcache_test.cpp
 * @brief Regression coverage for startup cache operations in Unicode paths.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (c) 2026, Linden Research, Inc.
 * $/LicenseInfo$
 */
#include "linden_common.h"

// <FS:Beq> FIRE-37081: Use real files and the public cache API, not conversion mocks.
#include "lldir.h"
#include "lldiriterator.h"
#include "llassettype.h"
#include "lldiskcache.h"
#include "llfilesystem.h"
#include "llapp.h"
#include "lluuid.h"
#include "lltut.h"

#include <filesystem>
#include <fstream>
#include <set>

namespace
{
    namespace fs = std::filesystem;

    // Keep fixture creation and expected UTF-8 independent of fsyspath/LLFile.
    std::string utf8(const fs::path& path)
    {
        const auto bytes = path.u8string();
        return std::string(bytes.begin(), bytes.end());
    }

    struct TestDirectory
    {
        const fs::path path = fs::temp_directory_path() /
            ("FIRE-37081-" + LLUUID::generateNewID().asString());

        TestDirectory()
        {
            tut::ensure("create unique test directory", fs::create_directory(path));
        }

        ~TestDirectory()
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    };

    // Redirect all cache and distribution lookups into the fixture. Never call
    // initAppDirs(), which would create directories in the real user profile.
    class TestDirs : public LLDir
    {
    public:
        TestDirs() : mPrevious(gDirUtilp) { gDirUtilp = this; }
        ~TestDirs() override { gDirUtilp = mPrevious; }

        void configure(const fs::path& cache, const fs::path& install)
        {
            mCacheDir = utf8(cache);
            mDefaultCacheDir = mCacheDir;
            mAppRODataDir = utf8(install);
        }

        void initAppDirs(const std::string&, const std::string&) override {}
        U32 countFilesInDir(const std::string&, const std::string&) override { return 0; }
        bool getNextFileInDir(const std::string&, const std::string&, std::string&) override { return false; }
        std::string getCurPath() override { return {}; }
        bool fileExists(const std::string& name) const override
        {
            return fs::exists(fs::path(std::u8string(name.begin(), name.end())));
        }
        std::string getLLPluginLauncher() override { return {}; }
        std::string getLLPluginFilename(std::string) override { return {}; }

    private:
        LLDir* mPrevious;
    };

    // purge() exits immediately unless the application is running. Save and
    // restore the status without constructing a viewer or starting any thread.
    struct RunningApp : LLApp
    {
        static EAppStatus status() { return sStatus.get(); }
        using LLApp::setStatus;
    };
}

namespace tut
{
    struct diskcache_data
    {
        TestDirectory root;
        TestDirs dirs;
        const LLApp::EAppStatus previous_status = RunningApp::status();
        fs::path cache_parent;
        fs::path cache;

        ~diskcache_data()
        {
            LLDiskCache::deleteSingleton();
            RunningApp::setStatus(previous_status);
        }

        void initialize(const fs::path& profile)
        {
            cache_parent = root.path / profile / "AppData" / "Local" / "Firestorm";
            cache = cache_parent / "assets";
            fs::create_directories(cache_parent);
            dirs.configure(cache_parent, root.path / "install");
            RunningApp::setStatus(LLApp::APP_STATUS_RUNNING);
            LLDiskCache::initParamSingleton(utf8(cache), uintmax_t(1024), false, 95.f, 70.f);
            ensure("cache created in requested native directory", fs::is_directory(cache / "0"));
        }

        void write(const fs::path& path, std::size_t size)
        {
            std::ofstream out(path, std::ios::binary);
            out << std::string(size, 'x');
            out.close();
            ensure("create native fixture file", !out.fail());
        }

        void startup()
        {
            const auto asset = cache / "0" / "sl_cache_00000000-0000-0000-0000-000000000001_0.asset";
            const auto unrelated = cache / "0" / "keep.txt";
            const auto inventory = cache_parent / "inv.llsd.old";
            const auto database = cache_parent / "db2.x.old";
            const auto settings = cache_parent / "keep.xml";
            write(asset, 512);
            write(unrelated, 3);
            write(inventory, 4);
            write(database, 5);
            write(settings, 6);

            auto* disk = LLDiskCache::getInstance();
            // Startup cache migration must inspect full paths even when every
            // filename is ASCII. A Unicode filename alone misses FIRE-37081.
            disk->removeOldVFSFiles();
            ensure("old inventory cache removed", !fs::exists(inventory));
            ensure("old VFS database removed", !fs::exists(database));
            ensure("unrelated settings retained", fs::exists(settings));
            ensure_contains("cache size scan reads the actual asset", disk->getCacheInfo(), "50.0% used");
            disk->clearCache();
            ensure("asset removed by startup cache clear", !fs::exists(asset));
            ensure("unrelated file retained by cache clear", fs::exists(unrelated));
        }

        fs::path asset_path(const LLUUID& id) const
        {
            const auto name = id.asString();
            return cache / name.substr(0, 1) / ("sl_cache_" + name + "_0.asset");
        }

        void asset_io()
        {
            // FailedToFindSettings is an environment-asset error, not a failure
            // to read user_settings/settings.xml. Its loader sizes its buffer
            // using LLFileSystem::getSize(), as do other asset consumers.
            for (const auto type : {LLAssetType::AT_SETTINGS, LLAssetType::AT_ANIMATION,
                                    LLAssetType::AT_NOTECARD})
            {
                const std::string label = LLAssetType::lookup(type);
                const auto id = LLUUID::generateNewID();
                const auto path = asset_path(id);
                const std::string payload("asset\0bytes\x80\xff", 13);
                // Seed the native file independently: matching encoding bugs
                // in a writer and reader must not make this test pass.
                std::ofstream out(path, std::ios::binary);
                out.write(payload.data(), payload.size());
                out.close();
                ensure("seed native asset", !out.fail());

                LLFileSystem reader(id, type, LLFileSystem::READ);
                ensure_equals(label + ": nonempty asset size under Unicode profile",
                              reader.getSize(), S32(payload.size()));
                ensure(label + ": existing asset is found", LLFileSystem::getExists(id, type));
                ensure(label + ": nonempty asset does not start at EOF", !reader.eof());
                std::vector<U8> bytes(payload.size());
                ensure(label + ": asset is readable", reader.read(bytes.data(), S32(bytes.size())));
                ensure_equals(label + ": all bytes read", reader.getLastBytesRead(), S32(payload.size()));
                ensure_memory_matches((label + ": exact asset contents").c_str(), bytes.data(), U32(bytes.size()),
                                      payload.data(), U32(payload.size()));
                ensure(label + ": EOF after full read", reader.eof());

                const auto renamed_id = LLUUID::generateNewID();
                ensure(label + ": rename succeeds", reader.rename(renamed_id, type));
                ensure(label + ": old native path removed", !fs::exists(path));
                ensure(label + ": renamed native path exists", fs::exists(asset_path(renamed_id)));
                ensure_equals(label + ": reader tracks renamed path", reader.getSize(), S32(payload.size()));
                ensure(label + ": remove succeeds", reader.remove());
                ensure(label + ": renamed native asset removed", !fs::exists(asset_path(renamed_id)));

                LLFileSystem writer(id, type, LLFileSystem::WRITE);
                ensure(label + ": asset write succeeds",
                       writer.write(reinterpret_cast<const U8*>(payload.data()), S32(payload.size())));
                std::ifstream in(path, std::ios::binary);
                const std::string written((std::istreambuf_iterator<char>(in)), {});
                in.close();
                ensure_equals(label + ": writer uses exact native path and bytes", written, payload);
                ensure(label + ": written asset is found", LLFileSystem::getExists(id, type));
                ensure(label + ": written asset removed", writer.remove());
                ensure(label + ": written native asset removed", !fs::exists(path));
            }
        }
    };

    using diskcache_group = test_group<diskcache_data>;
    using diskcache_object = diskcache_group::object;
    diskcache_group diskcache_tests("LLDiskCache");

    template<> template<>
    void diskcache_object::test<1>()
    {
        set_test_name("FIRE-37081: startup and cache IO in a Unicode Windows profile");
        // Accented Latin, Cyrillic, CJK, a UTF-16 surrogate pair, spaces and
        // punctuation. Source stays ASCII; fixtures use native Unicode paths.
        // LLParamSingleton can be initialized only once per process, including
        // after deletion, so exercise the complete lifecycle in one test.
        initialize(fs::path(u8"Jos\u00e9-\u0416-\u65e5 \U0001f680 [test]"));
        asset_io();
        startup();

        // Actual deletion catches a second class of bug: scanning correctly,
        // but passing a UTF-8 std::string to a native-narrow filesystem API.
        const auto old_asset = cache / "0" / "sl_cache_00000000-0000-0000-0000-000000000001_0.asset";
        const auto new_asset = cache / "1" / "sl_cache_10000000-0000-0000-0000-000000000001_0.asset";
        const auto unrelated = cache / "0" / "keep.txt";
        write(old_asset, 600);
        write(new_asset, 600);
        write(unrelated, 7);
        const auto now = fs::file_time_type::clock::now();
        fs::last_write_time(old_asset, now - std::chrono::hours(2));
        fs::last_write_time(new_asset, now - std::chrono::hours(1));

        LLDiskCache::getInstance()->purge();
        ensure("oldest asset actually deleted", !fs::exists(old_asset));
        ensure("newer asset below low-water threshold retained", fs::exists(new_asset));
        ensure("unrelated file retained", fs::exists(unrelated));
        // Also check the opposite conversion direction: native filenames must
        // be returned to callers as exact UTF-8, without loss or substitution.
        const auto first = cache / fs::path(u8"caf\u00e9.asset");
        const auto second = cache / fs::path(u8"\u65e5\U0001f680.asset");
        write(first, 8);
        write(second, 9);
        write(cache / "ignore.txt", 10);
        const std::set<std::string> expected = {utf8(first.filename()), utf8(second.filename())};

        std::set<std::string> actual;
        LLDirIterator iterator(utf8(cache), "*.asset");
        std::string name;
        while (iterator.next(name))
        {
            ensure("iterator returns each filename once", actual.insert(name).second);
        }
        ensure("iterator preserves UTF-8 bytes", actual == expected);
        const auto files = dirs.getFilesInDir(utf8(cache));
        actual = std::set<std::string>(files.begin(), files.end());
        auto all_expected = expected;
        all_expected.insert("ignore.txt");
        ensure("file listing preserves UTF-8 bytes and excludes directories", actual == all_expected);
    }
}
// </FS:Beq>

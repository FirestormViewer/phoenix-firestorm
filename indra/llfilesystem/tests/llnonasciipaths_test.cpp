/** Directory enumeration and path-form contracts with native Unicode fixtures. */
#include "linden_common.h"
// <FS:Beq> Extended non-ASCII path test coverage.
#include "path_test_support.h"
#include "lldiriterator.h"
#include "llfile.h"
#include "llfilesystem.h"
#include <set>

namespace tut
{
    struct nonasciipaths_data
    {
        nonascii::Tree tree;
        nonascii::Dirs dirs;
        nonascii::fs::path parent = tree.path / nonascii::name();
        nonasciipaths_data() { nonascii::fs::create_directory(parent); }
    };
    using nonasciipaths_group = test_group<nonasciipaths_data>;
    using nonasciipaths_object = nonasciipaths_group::object;
    nonasciipaths_group nonasciipaths_tests("LLNonASCIIPaths");

    template<> template<>
    void nonasciipaths_object::test<1>()
    {
        set_test_name("directory enumeration preserves non-ASCII child names");
        const auto first = parent / nonascii::fs::path(u8"\u0416_old");
        const auto second = parent / nonascii::fs::path(u8"\u65e5\U0001f680_old");
        nonascii::fs::create_directory(first);
        nonascii::fs::create_directory(second);
        nonascii::write(parent / "exclude.txt", "file");
        const auto directories = dirs.getDirectoriesInDir(nonascii::utf8(parent));
        const std::set<std::string> actual(directories.begin(), directories.end());
        const std::set<std::string> expected{nonascii::utf8(first.filename()), nonascii::utf8(second.filename())};
        ensure("exact UTF-8 directory names with no files", actual == expected);
        ensure_equals("no duplicate directory names", directories.size(), expected.size());
        // CEF stale-cache purging rejoins returned names with the parent.
        for (const auto& child : directories)
        {
            ensure("returned child rejoins to native directory",
                   nonascii::fs::is_directory(nonascii::native(dirs.add(nonascii::utf8(parent), child))));
        }
    }

    template<> template<>
    void nonasciipaths_object::test<2>()
    {
        set_test_name("Unicode file listings and glob filters return exact filenames");
        const auto first = parent / nonascii::fs::path(u8"\u0416.asset");
        const auto second = parent / nonascii::fs::path(u8"\u65e5\U0001f680.asset");
        nonascii::write(first, "first");
        nonascii::write(second, "second");
        nonascii::write(parent / "ignored.txt", "ignored");
        nonascii::fs::create_directory(parent / "not-a-file");
        const auto files = dirs.getFilesInDir(nonascii::utf8(parent));
        std::set<std::string> expected{nonascii::utf8(first.filename()), nonascii::utf8(second.filename()), "ignored.txt"};
        ensure("file listing names", std::set<std::string>(files.begin(), files.end()) == expected);
        LLDirIterator iterator(nonascii::utf8(parent), "*.asset");
        std::set<std::string> selected;
        std::string name;
        while (iterator.next(name)) ensure("no duplicate glob results", selected.insert(name).second);
        expected.erase("ignored.txt");
        ensure("glob preserves Unicode bytes", selected == expected);
    }

    template<> template<>
    void nonasciipaths_object::test<3>()
    {
        set_test_name("missing Unicode directory yields no entries");
        const auto missing = nonascii::utf8(parent / nonascii::fs::path(u8"\u65e5-missing"));
        ensure("missing directory listing", dirs.getDirectoriesInDir(missing).empty());
        ensure("missing file listing", dirs.getFilesInDir(missing).empty());
        LLDirIterator iterator(missing, "*");
        std::string name = "stale";
        ensure("missing iterator has no result", !iterator.next(name));
        ensure("missing iterator clears previous name", name.empty());
    }

    template<> template<>
    void nonasciipaths_object::test<4>()
    {
        set_test_name("directory iteration accepts a trailing separator");
        const auto child = parent / "child";
        nonascii::fs::create_directory(child);
        const auto bytes = nonascii::utf8(parent) + char(nonascii::fs::path::preferred_separator);
        const auto entries = dirs.getDirectoriesInDir(bytes);
        ensure_equals("trailing-separator listing", entries.size(), std::size_t(1));
        ensure_equals("child filename", entries.front(), std::string("child"));
    }

    template<> template<>
    void nonasciipaths_object::test<5>()
    {
        set_test_name("recursive deletion uses the exact Unicode directory");
        const auto doomed = parent / nonascii::fs::path(u8"\u65e5\U0001f680-remove");
        const auto retained = parent / nonascii::fs::path(u8"\u0416-keep.txt");
        nonascii::fs::create_directories(doomed / "nested");
        nonascii::write(doomed / "nested" / "file.bin", "delete");
        nonascii::write(retained, "keep");
        dirs.deleteDirAndContents(nonascii::utf8(doomed));
        ensure("native target removed", !nonascii::fs::exists(doomed));
        ensure_equals("neighbor retained", nonascii::read(retained), std::string("keep"));
    }

    template<> template<>
    void nonasciipaths_object::test<6>()
    {
        set_test_name("asset timestamp error diagnostics tolerate a missing Unicode file");
        struct DeleteCache
        {
            ~DeleteCache() { LLDiskCache::deleteSingleton(); }
        } cleanup;
        const auto cache = parent / "cache";
        dirs.configure(cache, parent / "install");
        LLDiskCache::initParamSingleton(nonascii::utf8(cache), uintmax_t(1024), false, 95.f, 70.f);
        const LLUUID id("00000000-0000-0000-0000-000000000001");
        const auto path = cache / "0" / "sl_cache_00000000-0000-0000-0000-000000000001_0.asset";
        nonascii::write(path, "asset");
        const auto old = nonascii::fs::file_time_type::clock::now() - std::chrono::hours(4);
        nonascii::fs::last_write_time(path, old);
        LLFileSystem reader(id, LLAssetType::AT_SETTINGS, LLFileSystem::READ);
        ensure("ordinary access refreshes timestamp", nonascii::fs::last_write_time(path) > old);
        // A cache file can disappear between the existence check and timestamp
        // update. Reporting that failure must not itself throw on its path.
        nonascii::fs::remove(path);
        nonascii::LogCapture logs;
        reader.updateFileAccessTime();
        ensure_contains("timestamp warning preserves exact UTF-8 path", logs.messages->text, nonascii::utf8(path));
        ensure_contains("timestamp query error was reported", logs.messages->text, "Failed to read last write time");
        ensure("missing asset was not recreated", !nonascii::fs::exists(path));
    }
}
// </FS:Beq>

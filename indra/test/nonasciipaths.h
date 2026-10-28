/** Test fixtures for native paths and independently encoded UTF-8 expectations. */
#ifndef LL_TEST_NONASCIIPATHS_H
#define LL_TEST_NONASCIIPATHS_H

// <FS:Beq> Extended non-ASCII path test coverage.
#include "lluuid.h"
#include "lltut.h"
#include <filesystem>
#include <fstream>

namespace nonascii
{
    namespace fs = std::filesystem;

    inline std::string utf8(const fs::path& path)
    {
        const auto bytes = path.u8string();
        return {bytes.begin(), bytes.end()};
    }

    inline fs::path native(const std::string& bytes)
    {
        return fs::path(std::u8string(bytes.begin(), bytes.end()));
    }

    inline fs::path name()
    {
        return fs::path(u8"caf\u00e9-\u0416-\u65e5-\U0001f680");
    }

    inline fs::path extended(const fs::path& path)
    {
#if LL_WINDOWS
        auto wide = fs::absolute(path).native();
        if (wide.starts_with(L"\\\\?\\")) return fs::path(wide);
        if (wide.starts_with(L"\\\\")) return fs::path(L"\\\\?\\UNC\\" + wide.substr(2));
        return fs::path(L"\\\\?\\" + wide);
#else
        return path;
#endif
    }

    struct Tree
    {
        const fs::path path;
        explicit Tree(const fs::path& base = fs::temp_directory_path()) :
            path(base / ("na-" + LLUUID::generateNewID().asString()))
        {
            tut::ensure("create unique native test tree", fs::create_directory(path));
        }
        Tree(const Tree&) = delete;
        Tree& operator=(const Tree&) = delete;
        ~Tree()
        {
            std::error_code ec;
            fs::remove_all(extended(path), ec);
        }
    };

    inline void write(const fs::path& path, const std::string& contents)
    {
        std::ofstream out(path, std::ios::binary);
        out.write(contents.data(), contents.size());
        out.close();
        tut::ensure("seed native file", !out.fail());
    }

    inline std::string read(const fs::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        tut::ensure("open native file", in.is_open());
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }
}
// </FS:Beq>
#endif

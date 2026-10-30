/** Extended non-ASCII path test coverage. */
#include "linden_common.h"

// <FS:Beq> Extended non-ASCII path test coverage.
#include "llfile.h"
#include "llstring.h"
#include "llerrorcontrol.h"
#include "nonasciipaths.h"

#include <memory>

namespace
{
    using namespace nonascii;

    class Messages : public LLError::Recorder
    {
    public:
        std::string text;
        Messages() { showMultiline(true); }
        void recordMessage(LLError::ELevel, const std::string& message) override
        {
            text += message + '\n';
        }
    };

    struct Capture
    {
        LLError::SettingsStoragePtr previous = LLError::saveAndResetSettings();
        std::shared_ptr<Messages> messages = std::make_shared<Messages>();
        Capture()
        {
            LLError::setDefaultLevel(LLError::LEVEL_DEBUG);
            LLError::addRecorder(messages);
        }
        ~Capture()
        {
            LLError::removeRecorder(messages);
            LLError::restoreSettings(previous);
        }
    };
}

namespace tut
{
    struct filepaths_data
    {
        nonascii::Tree tree;
        fs::path directory = tree.path / nonascii::name();
        filepaths_data() { fs::create_directory(directory); }

        template<typename Path>
        void roundtrip(const Path& input, const fs::path& expected)
        {
            const std::string payload("binary\0\x80\xff\r\n", 11);
            std::error_code ec;
            LLFile out(input, LLFile::out | LLFile::trunc, ec);
            ensure("open supplied path", bool(out) && !ec);
            ensure_equals("write all bytes", out.write(payload.data(), payload.size(), ec), S64(payload.size()));
            ensure("write error clear", !ec);
            out.close();
            ensure_equals("independent native contents", nonascii::read(expected), payload);
            ensure("native/UTF-8 exists agree", LLFile::exists(input));
            ensure_equals("native/UTF-8 sizes agree", LLFile::size(input), S64(payload.size()));
            ensure_equals("read through supplied path", LLFile::getContents(input), payload);
            std::unique_ptr<LLFILE, int (*)(LLFILE*)> file(LLFile::fopen(input, LLFILE_MODE("rb")), &LLFile::close);
            ensure("fopen supplied path", bool(file));
            std::vector<char> bytes(payload.size());
            ensure_equals("fread all bytes", fread(bytes.data(), 1, bytes.size(), file.get()), payload.size());
            ensure_equals("fopen exact contents", std::string(bytes.begin(), bytes.end()), payload);
        }
    };
    using filepaths_group = test_group<filepaths_data>;
    using filepaths_object = filepaths_group::object;
    filepaths_group filepaths_tests("LLFilePaths");

    template<> template<>
    void filepaths_object::test<1>()
    {
        set_test_name("native, UTF-8 string and char pointer file overloads");
        const auto native_path = directory / fs::path(u8"\u65e5\U0001f680.bin");
        roundtrip(native_path, native_path);
        const auto string_path = directory / fs::path(u8"\u0416-string.bin");
        roundtrip(nonascii::utf8(string_path), string_path);
        const auto pointer_path = directory / fs::path(u8"caf\u00e9-pointer.bin");
        const auto bytes = nonascii::utf8(pointer_path);
        roundtrip(bytes.c_str(), pointer_path);
    }

    template<> template<>
    void filepaths_object::test<2>()
    {
        set_test_name("UTF-8 and native stream constructors and open methods");
        const auto path = directory / fs::path(u8"\u0416\u65e5\U0001f680.txt");
        const auto bytes = nonascii::utf8(path);
        auto check = [&](const auto& input)
        {
            { llofstream out(input, std::ios::binary); out << "constructor"; ensure("stream constructor", bool(out)); }
            ensure_equals("native constructor contents", nonascii::read(path), std::string("constructor"));
            { llifstream in(input, std::ios::binary); std::string value; in >> value;
              ensure_equals("input constructor", value, std::string("constructor")); }
            { llofstream out; out.open(input, std::ios::binary); out << "open"; ensure("stream open", bool(out)); }
            ensure_equals("native open contents", nonascii::read(path), std::string("open"));
            { llifstream in; in.open(input, std::ios::binary); std::string value; in >> value;
              ensure_equals("input open", value, std::string("open")); }
        };
        check(path);
        check(bytes);
        check(bytes.c_str());
    }

    template<> template<>
    void filepaths_object::test<3>()
    {
        set_test_name("copy and rename preserve exact native filenames");
        const auto from = directory / fs::path(u8"\u0416-source.bin");
        const auto to = directory / fs::path(u8"\u65e5-copy.bin");
        const auto renamed = directory / fs::path(u8"\U0001f680-renamed.bin");
        nonascii::write(from, "native source");
        ensure("UTF-8 copy", LLFile::copy(nonascii::utf8(from), nonascii::utf8(to)));
        ensure_equals("native copy", nonascii::read(to), std::string("native source"));
        ensure_equals("UTF-8 rename", LLFile::rename(nonascii::utf8(to), nonascii::utf8(renamed)), 0);
        ensure("old native filename removed", !fs::exists(to));
        ensure_equals("native rename", nonascii::read(renamed), std::string("native source"));
        ensure_equals("native remove", LLFile::remove(renamed), 0);
        ensure("renamed native file removed", !fs::exists(renamed));
    }

    template<> template<>
    void filepaths_object::test<4>()
    {
        set_test_name("missing Unicode files report errors without corrupting diagnostics");
        const auto missing = directory / fs::path(u8"\u65e5\U0001f680-missing.bin");
        const auto destination = directory / fs::path(u8"\u0416-target.bin");
        Capture capture;
        std::error_code ec;
        ensure("missing copy fails", !LLFile::copy(missing, destination, fs::copy_options::none, ec));
        ensure("copy supplies error code", bool(ec));
        ensure_contains("copy diagnostic source", capture.messages->text, nonascii::utf8(missing));
        ensure_contains("copy diagnostic target", capture.messages->text, nonascii::utf8(destination));
        capture.messages->text.clear();
        ensure_equals("missing rename fails", LLFile::rename(nonascii::utf8(missing), nonascii::utf8(destination)), -1);
        ensure_contains("rename diagnostic", capture.messages->text, nonascii::utf8(missing));
        capture.messages->text.clear();
        char byte;
        ensure_equals("missing read fails", LLFile::read(missing, &byte, 0, 1, ec), S64(-1));
        ensure("read supplies error code", bool(ec));
        ensure_contains("read diagnostic", capture.messages->text, nonascii::utf8(missing));
        ensure("failed operations created no target", !fs::exists(destination));
    }

    template<> template<>
    void filepaths_object::test<5>()
    {
        set_test_name("denied Unicode file access preserves diagnostic path");
#if LL_WINDOWS
        const auto path = directory / fs::path(u8"\u65e5\U0001f680-locked.bin");
        nonascii::write(path, "locked data");
        std::error_code ec;
        LLFile exclusive(path, LLFile::in | LLFile::exclusive, ec);
        ensure("exclusive native handle", bool(exclusive) && !ec);
        Capture capture;
        char byte;
        ensure_equals("read denied by exclusive handle", LLFile::read(nonascii::utf8(path), &byte, 0, 1, ec), S64(-1));
        ensure("denied read error code", bool(ec));
        ensure_contains("denied read diagnostic", capture.messages->text, nonascii::utf8(path));
        exclusive.close();
        ensure_equals("denied read retained source", nonascii::read(path), std::string("locked data"));
#else
        skip("Windows sharing-denial test; POSIX permissions depend on test account privileges");
#endif
    }

    template<> template<>
    void filepaths_object::test<6>()
    {
        set_test_name("directory status accepts trailing separators and drive roots");
        auto path = nonascii::utf8(directory) + char(fs::path::preferred_separator);
        ensure("trailing separator directory status", LLFile::isdir(path));
        llstat status;
        ensure_equals("trailing separator stat", LLFile::stat(path, &status), 0);
        ensure("stat describes a directory", (status.st_mode & S_IFDIR) != 0);
        const auto root = directory.root_path();
        ensure("native root exists", fs::is_directory(root));
        ensure("root directory status", LLFile::isdir(nonascii::utf8(root)));
    }

    template<> template<>
    void filepaths_object::test<7>()
    {
        set_test_name("fresh-process temporary directory is returned as UTF-8");
        // The Python launcher sets TEMP/TMP/TMPDIR before this executable's
        // static initialization, and runs ASCII and Unicode environments.
        const auto expected = LLStringUtil::getenv("LL_TEST_EXPECTED_TEMP");
        if (expected.empty()) skip("Run through llfilepaths_test.py to set a fresh-process temp directory");
        const auto actual = std::string(LLFile::tmpdir());
        ensure("tmpdir terminates with a separator", !actual.empty() && actual.back() == fs::path::preferred_separator);
        ensure_equals("exact UTF-8 tmpdir", actual, expected + char(fs::path::preferred_separator));
        ensure("tmpdir maps to native expected directory", fs::equivalent(nonascii::native(actual), nonascii::native(expected)));
        const auto path = nonascii::native(actual) / fs::path(u8"\u0416\u65e5\U0001f680.tmp");
        roundtrip(nonascii::utf8(path), path);
        ensure_equals("remove temp file", LLFile::remove(nonascii::utf8(path)), 0);
        ensure("native temp file removed", !fs::exists(path));
    }

    template<> template<>
    void filepaths_object::test<8>()
    {
        set_test_name("UTF-8 adapter constructors and assignments preserve path forms");
        const std::vector<fs::path> paths = {
            directory / nonascii::name(),
            fs::path(u8"C:\\Users\\\u0416\\"),
            fs::path(u8"\\\\server\\share\\\u65e5\\\U0001f680.txt"),
            fs::path(u8"\\\\?\\C:\\\u0416\\\u65e5.txt"),
            fs::path(u8"\\\\?\\UNC\\server\\share\\\u0416.txt")};
        for (const auto& expected : paths)
        {
            const auto bytes = nonascii::utf8(expected);
            auto check = [&](const fsyspath& actual)
            {
                ensure("exact native path", actual.native() == expected.native());
                ensure_equals("exact UTF-8 path", actual.string(), bytes);
            };
            check(fsyspath(bytes)); check(fsyspath(bytes.c_str())); check(fsyspath(std::string_view(bytes)));
            check(fsyspath(expected)); check(fsyspath(fs::path(expected)));
            fsyspath assigned;
            assigned = bytes; check(assigned);
            assigned = bytes.c_str(); check(assigned);
            assigned = std::string_view(bytes); check(assigned);
            assigned = expected; check(assigned);
            assigned = fs::path(expected); check(assigned);
        }
    }

    template<> template<>
    void filepaths_object::test<9>()
    {
        set_test_name("extended-length Unicode Windows paths support native file IO");
#if LL_WINDOWS
        auto longdir = nonascii::extended(directory);
        while (longdir.native().size() < 280) longdir /= std::wstring(50, L'\u0416');
        fs::create_directories(longdir);
        const auto path = longdir / fs::path(u8"\u65e5\U0001f680.bin");
        roundtrip(nonascii::utf8(path), path);
#else
        skip("Windows extended-length path test");
#endif
    }

}
// </FS:Beq>

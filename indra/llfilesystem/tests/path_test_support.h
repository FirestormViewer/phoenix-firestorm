/** Filesystem test directory redirection; never initializes a real user profile. */
#ifndef LL_PATH_TEST_SUPPORT_H
#define LL_PATH_TEST_SUPPORT_H
// <FS:Beq> Extended non-ASCII path test coverage.
#include "lldir.h"
#include "llapp.h"
#include "nonasciipaths.h"

namespace nonascii
{
    class Dirs : public LLDir
    {
    public:
        Dirs() : previous(gDirUtilp)
        {
            mDirDelimiter = std::string(1, char(fs::path::preferred_separator));
            gDirUtilp = this;
        }
        ~Dirs() override { gDirUtilp = previous; }
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
        bool fileExists(const std::string& path) const override { return fs::exists(native(path)); }
        std::string getLLPluginLauncher() override { return {}; }
        std::string getLLPluginFilename(std::string) override { return {}; }
    private:
        LLDir* previous;
    };

    struct AppStatus : LLApp
    {
        static EAppStatus status() { return sStatus.get(); }
        using LLApp::setStatus;
    };
}
// </FS:Beq>
#endif

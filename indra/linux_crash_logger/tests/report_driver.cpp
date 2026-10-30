// <FS:Beq> Local-only integration driver; never included in a viewer package.
#include "linux_crash_report.h"
#include <curl/curl.h>
#include <iostream>
int main(int argc, char** argv)
{
    try
    {
        if (argc == 5 && std::string(argv[1]) == "capture")
        {
            const auto report = FSCrashReport::capture(argv[2], argv[3], argv[4]);
            std::cout << (report.directory / "report.json").string() << '\n';
            return 0;
        }
        if (argc == 4 && std::string(argv[1]) == "upload")
        {
            auto report = FSCrashReport::load(argv[2]);
            curl_global_init(CURL_GLOBAL_DEFAULT);
            std::string error;
            const bool sent = FSCrashReport::upload(report, error, argv[3]);
            curl_global_cleanup();
            std::cout << error << '\n';
            return sent ? 0 : 1;
        }
        if (argc == 3 && std::string(argv[1]) == "pending")
        {
            for (const auto& path : FSCrashReport::pending(argv[2])) std::cout << path.string() << '\n';
            return 0;
        }
        return 2;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
// </FS:Beq>

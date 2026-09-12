#include "llvktexturecache.h"
#include "lltracethreadrecorder.h"
#include <iostream>
#include <windows.h>

int main()
{
    if (LLTrace::get_master_thread_recorder())
    {
        std::cerr << "Cold-start regression unexpectedly has a master recorder\n";
        return 1;
    }
    const auto root=std::filesystem::temp_directory_path()/
        ("native-cache-cold-start-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
    struct Cleanup
    {
        std::filesystem::path root;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(root,ignored); }
    } cleanup{root};
    LLVKTextureCache cache;
    LLVKTextureCache::Configuration configuration;
    configuration.directory=root/"cache";
    configuration.localAssets=root/"assets";
    configuration.bytes=256ull*1024*1024;
    std::string error;
    if (!cache.start(configuration,error)) { std::cerr << error << '\n'; return 1; }
    if (!LLTrace::get_master_thread_recorder())
    {
        std::cerr << "Native cache started a worker without its master recorder\n";
        return 1;
    }
    auto write=cache.write(LLUUID::generateNewID(),std::vector<std::uint8_t>(2048,42),2048,error);
    if (!write.valid() || !cache.stop(error)) { std::cerr << error << '\n'; return 1; }
    if (!write.get().success) { std::cerr << "Cold-start cache write failed\n"; return 1; }
    if (LLTrace::get_master_thread_recorder())
    { std::cerr << "Owned master recorder was not retired\n"; return 1; }
    std::cout << "Native cache cold startup, worker IO and shutdown passed\n";
    return 0;
}
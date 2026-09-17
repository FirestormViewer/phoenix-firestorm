#ifndef VULKANSTORM_DIAGNOSTIC_REPLAY_CLOCK_H
#define VULKANSTORM_DIAGNOSTIC_REPLAY_CLOCK_H

#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace diagnostic_replay
{
class Clock final
{
public:
    static Clock& instance() { static Clock clock; return clock; }
    bool controlled() const { return mControlled; }
    std::uint64_t microseconds(std::uint64_t ordinary,std::uint64_t previous)
    {
        if (!mControlled) return ordinary;
        if (!mHaveBase) { mBase=previous; mHaveBase=true; }
        if (mAge>(std::numeric_limits<std::uint64_t>::max)()-mBase)
        { fail("clock overflow"); return ordinary; }
        return mBase+mAge;
    }
    bool begin()
    {
        if (mDirectory.empty() || mFailed) return false;
        for (;;)
        {
            std::ifstream request(mDirectory/"request.txt");
            std::uint64_t sequence=0;
            std::int64_t age=0;
            if (request >> sequence >> age; request && sequence>mSequence)
            {
                if (age < -1 || (age>=0 && static_cast<std::uint64_t>(age)<mAge))
                { fail("nonmonotonic replay request"); return false; }
                mSequence=sequence;
                if (age==-1)
                {
                    mControlled=false;
                    acknowledge("released");
                    return false;
                }
                mAge=static_cast<std::uint64_t>(age);
                mControlled=true;
                return true;
            }
            request.close();
            if (!mControlled) return false;
            if (!mWake || WaitForSingleObject(mWake,60000)!=WAIT_OBJECT_0)
            { fail("replay controller timeout"); return false; }
        }
    }
    void finish() { if (mControlled) acknowledge("frame"); }
private:
    Clock()
    {
        const auto directory=std::getenv("LL_DIAGNOSTIC_REPLAY_DIR");
        if (!directory || !*directory) return;
        const std::string encoded(directory);
        mDirectory=std::filesystem::path(std::u8string(encoded.begin(),encoded.end()));
        const auto name=L"Local\\VulkanStormReplay-"+std::to_wstring(GetCurrentProcessId());
        mWake=CreateEventW(nullptr,FALSE,FALSE,name.c_str());
        if (!mWake) fail("cannot create replay event");
    }
    ~Clock() { if (mWake) CloseHandle(mWake); }
    void acknowledge(const char* state)
    {
        if (mAcknowledged==mSequence) return;
        const auto basename="ack-"+std::to_string(mSequence);
        const auto temporary=mDirectory/(basename+".tmp");
        {
            std::ofstream output(temporary,std::ios::trunc);
            output<<mSequence<<' '<<mAge<<' '<<state<<'\n';
            output.flush();
            if (!output) { fail("cannot write replay acknowledgement"); return; }
        }
        if (!MoveFileExW(temporary.c_str(),(mDirectory/(basename+".txt")).c_str(),MOVEFILE_WRITE_THROUGH))
        { fail("cannot publish replay acknowledgement"); }
        else mAcknowledged=mSequence;
    }
    void fail(const char* reason)
    {
        mFailed=true; mControlled=false;
        std::ofstream output(mDirectory/"failure.txt",std::ios::app);
        output<<reason<<'\n';
    }
    std::filesystem::path mDirectory;
    HANDLE mWake=nullptr;
    std::uint64_t mSequence=0,mAge=0,mBase=0,mAcknowledged=0;
    bool mControlled=false,mHaveBase=false,mFailed=false;
};

class Frame final
{
public:
    Frame() : mControlled(Clock::instance().begin()) {}
    ~Frame() { if (mControlled) Clock::instance().finish(); }
    Frame(const Frame&)=delete;
    Frame& operator=(const Frame&)=delete;
private:
    bool mControlled;
};
}

#endif
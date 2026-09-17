#include "diagnostic_replay_clock.h"
#include <iostream>
#include <stdexcept>

int main(int argc,char** argv)
{
    if (argc==3 && std::string(argv[1])=="--controller")
    {
        if (_putenv_s("LL_DIAGNOSTIC_REPLAY_DIR",argv[2])) return 2;
        auto& clock=diagnostic_replay::Clock::instance();
        std::cout<<"READY"<<std::endl;
        const auto eventName=L"Local\\VulkanStormReplay-"+std::to_wstring(GetCurrentProcessId());
        const auto wake=OpenEventW(SYNCHRONIZE,FALSE,eventName.c_str());
        if (!wake) return 3;
        const auto ready=WaitForSingleObject(wake,30000)==WAIT_OBJECT_0;
        CloseHandle(wake);
        if (!ready || !clock.begin() || clock.microseconds(9000,1000)!=1000) return 4;
        clock.finish();
        if (!clock.begin() || clock.microseconds(9000,1000)!=11000) return 5;
        clock.finish();
        if (clock.begin() || clock.microseconds(9000,1000)!=9000) return 6;
        return 0;
    }
    const auto directory=std::filesystem::temp_directory_path()/
        ("vulkanstorm-replay-test-"+std::to_string(GetCurrentProcessId()));
    try
    {
        if (!std::filesystem::create_directory(directory)) throw std::runtime_error("test directory exists");
        const auto encoded=directory.u8string();
        const std::string path(encoded.begin(),encoded.end());
        if (_putenv_s("LL_DIAGNOSTIC_REPLAY_DIR",path.c_str())) throw std::runtime_error("test environment");
        const auto require=[](bool value,const char* message) { if (!value) throw std::runtime_error(message); };
        const auto request=[&](unsigned sequence,std::int64_t age)
        {
            std::ofstream output(directory/"request.txt");
            output<<sequence<<' '<<age<<'\n';
            if (!output) throw std::runtime_error("write request");
        };
        auto& clock=diagnostic_replay::Clock::instance();
        require(!clock.begin(),"missing request must not gate ordinary frames");
        require(clock.microseconds(1234,1000)==1234,"ordinary clock passthrough");
        request(1,0);
        require(clock.begin(),"initial controlled frame");
        require(clock.microseconds(9999,1000)==1000,"initial controlled clock retains previous input time");
        require(clock.microseconds(19999,1000)==1000,"repeated timer reads must not advance");
        clock.finish();
        unsigned sequence=0; std::uint64_t age=0; std::string state;
        clock.finish();
        require(!std::filesystem::exists(directory/"failure.txt"),"duplicate completion remains idempotent");
        std::ifstream ack(directory/"ack-1.txt"); ack>>sequence>>age>>state;
        require(sequence==1 && age==0 && state=="frame","frame acknowledgement");
        ack.close();
        request(2,10000);
        require(clock.begin() && clock.microseconds(99999,1000)==11000,"controlled ten-millisecond step");
        clock.finish();
        request(3,-1);
        require(!clock.begin() && clock.microseconds(54321,11000)==54321,"release restores ordinary clock");
        request(4,9999);
        require(!clock.begin() && std::filesystem::exists(directory/"failure.txt"),"backward clock request rejected");
        std::filesystem::remove_all(directory);
        std::cout<<"Diagnostic replay protocol: all checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr<<error.what()<<'\n';
        std::filesystem::remove_all(directory);
        return 1;
    }
}
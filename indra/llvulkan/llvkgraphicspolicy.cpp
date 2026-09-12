#include "llvkgraphicspolicy.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

bool LLVKGraphicsPolicy::initialize(const std::filesystem::path& table,const std::filesystem::path& settings,std::string& error)
{
    error.clear();
    std::ifstream input(table,std::ios::binary|std::ios::ate);
    if (!input || input.tellg()<0 || input.tellg()>1024*1024) { error="Cannot read native graphics feature table"; return false; }
    input.seekg(0);
    std::map<std::string,std::map<std::string,Feature>> masks;
    std::string line,current;
    while (std::getline(input,line))
    {
        if (const auto comment=line.find("//"); comment!=std::string::npos) line.resize(comment);
        std::istringstream row(line); row.imbue(std::locale::classic());
        std::string name; if (!(row>>name)) continue;
        if (name=="version") { int version; if (!(row>>version) || version<1) { error="Invalid graphics table version"; return false; } continue; }
        if (name=="list") { if (!(row>>current)) { error="Invalid graphics mask name"; return false; } continue; }
        int available=0; float value=0;
        if (current.empty() || !(row>>available>>value) || (available!=0 && available!=1) || !std::isfinite(value))
        { error="Invalid graphics feature entry"; return false; }
        masks[current][name]={available!=0,value};
    }
    if (!masks.contains("all")) { error="Graphics table has no base mask"; return false; }
    if (!mSchema.loadFile(settings,true,true,true,error)) return false;
    mMasks=std::move(masks); return true;
}

int LLVKGraphicsPolicy::recommendedLevel(const Device& device)
{
    if (device.skipBenchmark) return 1;
    const float bandwidth=device.bandwidth*std::clamp(device.cpuBias,0.5f,1.f);
    int level=3;
    if (std::isfinite(bandwidth) && bandwidth>=0.f && device.classOneBandwidth>0.f)
    {
        const auto relative=bandwidth/device.classOneBandwidth;
        level=relative<=1.f ? 1 : relative<=2.f ? 2 : relative<=4.f || device.vendor==0x106b ? 3 : relative<=8.f ? 4 : 5;
    }
    if (device.systemBytes && device.systemBytes<8ull*1024*1024*1024 && level>1) --level;
    return level;
}

std::optional<std::map<std::string,LLSD>> LLVKGraphicsPolicy::settings(int level,const Device& device,bool recommended,std::string& error) const
{
    error.clear();
    const char* levels[]{"Low","LowMid","Mid","MidHigh","High","HighUltra","Ultra"};
    if (mMasks.empty()) { error="Native graphics policy is not initialized"; return {}; }
    if (recommended) level=recommendedLevel(device);
    if (level<0 || level>6) level=0;
    auto features=mMasks.at("all");
    const auto mask=[&](const std::string& name)
    {
        const auto found=mMasks.find(name); if (found==mMasks.end()) return;
        for (const auto& [name,feature] : found->second)
        {
            const auto current=features.find(name);
            if (current==features.end() || (feature.available && !current->second.available)) continue;
            current->second.available=feature.available;
            current->second.value=std::min(current->second.value,feature.value);
        }
    };
    mask("Class"+std::to_string(recommendedLevel(device)));
    if (device.vendor==0x10de) mask("NVIDIA");
    if (device.vendor==0x1002) mask("AMD");
    if (device.vendor==0x8086) mask("Intel");
    mask(device.vendor==0x106b ? "AppleGPU" : "NonAppleGPU");
    if (device.videoBytes>512ull*1024*1024) mask("VRAMGT512");
    if (device.videoBytes && device.videoBytes<2048ull*1024*1024) mask("VRAMLT2GB");
    mask(levels[level]);
    std::map<std::string,LLSD> values;
    for (const auto& [name,feature] : features)
    {
        if (name.starts_with("RenderGL") || name=="RenderVBOEnable" || name=="RenderVBOMappingDisable" || name=="RenderUseStreamVBO") continue;
        if (!recommended && (name=="RenderAnisotropic" || name=="RenderGamma" || name=="RenderFogRatio")) continue;
        auto control=mSchema.find(name); if (!control) continue;
        const float value=feature.available ? feature.value : 0.f;
        if (control->type()==TYPE_BOOLEAN) values[name]=LLSD(value!=0.f);
        else if (control->type()==TYPE_F32) values[name]=LLSD(value);
        else if (control->type()==TYPE_S32 || control->type()==TYPE_U32) values[name]=LLSD(static_cast<int>(value));
    }
    values["RenderQualityPerformance"]=level;
    if (!recommended) values["DebugQualityPerformance"]=level;
    if (recommended)
    {
        if (values.contains("Disregard96DefaultDrawDistance") && !values.at("Disregard96DefaultDrawDistance").asBoolean()) values["RenderFarClip"]=96.;
        else if (values.contains("Disregard128DefaultDrawDistance") && !values.at("Disregard128DefaultDrawDistance").asBoolean()) values["RenderFarClip"]=128.;
    }
    return values;
}
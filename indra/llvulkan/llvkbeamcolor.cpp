#include "llvkbeamcolor.h"
#include "llsdserialize.h"
#include "lluuid.h"
#include <fstream>
#if LL_WINDOWS
#include <windows.h>
#endif
#include <algorithm>
#include <cmath>

bool LLVKBeamColor::load(const LLSD& value,std::string& error)
{
    error.clear();
    if (!value.isMap()) { error="Beam color preset must be a map"; return false; }
    LLVKBeamColor next;
    for (const auto& [name,target] : {std::pair{"startHue",&next.startHue},std::pair{"endHue",&next.endHue},std::pair{"rotateSpeed",&next.rotateSpeed}})
    {
        if (!value.has(name)) continue;
        if (!value[name].isReal() && !value[name].isInteger()) { error="Beam color preset requires numeric values"; return false; }
        *target=static_cast<float>(value[name].asReal());
        if (!std::isfinite(*target)) { error="Beam color preset contains a nonfinite value"; return false; }
    }
    if (next.startHue<0.f || next.startHue>720.f || next.endHue<0.f || next.endHue>720.f || next.rotateSpeed<0.f || next.rotateSpeed>3.f)
    { error="Beam color preset exceeds editor limits"; return false; }
    *this=next;
    return true;
}

LLSD LLVKBeamColor::serialize() const
{
    LLSD result;
    result["startHue"]=startHue; result["endHue"]=endHue; result["rotateSpeed"]=rotateSpeed;
    return result;
}

bool LLVKBeamColor::saveFile(const std::filesystem::path& path,std::string& error) const
{
    return llvkSaveBeamPreset(path,serialize(),error);
}

bool llvkSaveBeamPreset(const std::filesystem::path& path,const LLSD& document,std::string& error)
{
    error.clear();
    if (!path.is_absolute() || path.filename().empty()) { error="Beam preset destination must be an absolute file path"; return false; }
    try
    {
        if (!std::filesystem::is_directory(path.parent_path()) || std::filesystem::is_symlink(path) ||
            (std::filesystem::exists(path) && !std::filesystem::is_regular_file(path)))
        { error="Invalid beam preset destination"; return false; }
        const auto staging=path.parent_path()/(".native-beam-"+LLUUID::generateNewID().asString());
        if (!std::filesystem::create_directory(staging)) { error="Cannot stage beam color preset"; return false; }
        struct Cleanup
        {
            std::filesystem::path directory;
            ~Cleanup() { std::error_code ignored; std::filesystem::remove(directory/"preset.xml",ignored); std::filesystem::remove(directory,ignored); }
        } cleanup{staging};
        const auto temporary=staging/"preset.xml";
        std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
        LLSDSerialize::toPrettyXML(document,output);
        output.close();
        if (!output) { error="Cannot write beam color preset"; return false; }
#if LL_WINDOWS
        if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        { error="Cannot replace beam color preset"; return false; }
#else
        std::filesystem::rename(temporary,path);
#endif
        return true;
    }
    catch (const std::filesystem::filesystem_error&) { error="Cannot save beam color preset"; return false; }
}

bool LLVKBeamColor::select(int horizontal,int vertical,bool end)
{
    if (vertical<=161 || vertical>=237) return false;
    const float selected=static_cast<float>(std::clamp(horizontal,6,402)-6)/396.f*720.f;
    (end ? endHue : startHue)=selected;
    if (endHue<startHue) std::swap(startHue,endHue);
    return true;
}

bool LLVKBeamColor::setSpeed(float percent)
{
    if (!std::isfinite(percent) || percent<0.f || percent>300.f) return false;
    rotateSpeed=percent/100.f;
    if (endHue<startHue) std::swap(startHue,endHue);
    return true;
}

int LLVKBeamColor::position(float degrees)
{
    return static_cast<int>(std::floor(degrees/720.f*396.f+0.5f))+6;
}

LLVKColor::Value LLVKBeamColor::hue(float degrees)
{
    const auto channel=[](float value)
    {
        value-=std::floor(value);
        if (6.f*value<1.f) return 6.f*value;
        if (2.f*value<1.f) return 1.f;
        if (3.f*value<2.f) return (2.f/3.f-value)*6.f;
        return 0.f;
    };
    const float value=degrees/360.f;
    return {channel(value+1.f/3.f),channel(value),channel(value-1.f/3.f),1.f};
}

std::optional<LLVKColor::Value> LLVKBeamColor::preview(double seconds) const
{
    if (!std::isfinite(seconds) || seconds<0 || seconds>1.e9) return {};
    const float difference=endHue-startHue;
    const float phase=difference!=0.f ? static_cast<float>(seconds)*0.3f*(rotateSpeed+0.01f)*(360.f/difference) : 0.f;
    const auto rounded=static_cast<int>(std::floor(difference+0.5f));
    const float degrees=rounded==360 || rounded==720 ? std::fmod(phase,1.f)*360.f :
        startHue+difference/2.f+std::sin(phase)*difference/2.f;
    auto result=hue(degrees);
    for (auto& channel : result) channel=std::floor(std::clamp(channel,0.f,1.f)*255.f+0.5f)/255.f;
    return result;
}
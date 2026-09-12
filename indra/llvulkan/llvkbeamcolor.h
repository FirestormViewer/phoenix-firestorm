#ifndef LLVKBEAMCOLOR_H
#define LLVKBEAMCOLOR_H

#include "llsd.h"
#include "llvkcolor.h"

bool llvkSaveBeamPreset(const std::filesystem::path& path,const LLSD& document,std::string& error);

struct LLVKBeamColor
{
    float startHue=0.f, endHue=360.f, rotateSpeed=1.f;
    bool load(const LLSD& value,std::string& error);
    LLSD serialize() const;
    bool saveFile(const std::filesystem::path& path,std::string& error) const;
    bool select(int horizontal,int vertical,bool end);
    bool setSpeed(float percent);
    std::optional<LLVKColor::Value> preview(double seconds) const;
    static LLVKColor::Value hue(float degrees);
    static int position(float degrees);
};

#endif
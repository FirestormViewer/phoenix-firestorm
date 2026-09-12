#ifndef LLVKBEAMSHAPE_H
#define LLVKBEAMSHAPE_H

#include "llvkbeamcolor.h"

struct LLVKBeamShape
{
    struct Point { int horizontal=0,vertical=0; LLVKColor::Value color{1,0,0,1}; };
    std::vector<Point> points;
    bool select(int horizontal,int vertical,bool remove,const LLVKColor::Value& color);
    LLSD serialize(int left,int bottom,int width,int height) const;
    bool load(const LLSD& document,int left,int bottom,int width,int height,std::string& error);
};

#endif
#include "llvkbeamshape.h"
#include <algorithm>
#include <cmath>

bool LLVKBeamShape::select(int horizontal,int vertical,bool remove,const LLVKColor::Value& color)
{
    if (remove)
    {
        std::erase_if(points,[&](const auto& point)
        {
            const double dx=static_cast<double>(horizontal)-point.horizontal,dy=static_cast<double>(vertical)-point.vertical;
            return dx*dx+dy*dy<49.;
        });
        return true;
    }
    if (horizontal<=16 || horizontal>=394 || vertical<=39 || vertical>=317 || points.size()>=4096 ||
        !std::all_of(color.begin(),color.end(),[](float channel) { return std::isfinite(channel); })) return false;
    points.push_back({horizontal,vertical,color});
    return true;
}

LLSD LLVKBeamShape::serialize(int left,int bottom,int width,int height) const
{
    LLSD document;
    document["scale"]=8.f/static_cast<float>(width);
    document["data"]=LLSD::emptyArray();
    for (const auto& point : points)
    {
        LLSD entry;
        entry["offset"]=LLSD::emptyArray();
        entry["offset"].append(0.);
        entry["offset"].append(static_cast<float>(point.horizontal-(left+width/2)));
        entry["offset"].append(static_cast<float>(point.vertical-(bottom+height/2)));
        entry["color"]=LLSD::emptyArray();
        for (const auto channel : point.color) entry["color"].append(channel);
        document["data"].append(entry);
    }
    return document;
}

bool LLVKBeamShape::load(const LLSD& document,int left,int bottom,int width,int height,std::string& error)
{
    error.clear();
    if (!document.isMap() || !document["data"].isArray() || document["data"].size()>4096 || width<=0 || height<=0)
    { error="Invalid beam shape preset"; return false; }
    const auto scale=document["scale"].asReal();
    if (!std::isfinite(scale) || scale<=0.) { error="Invalid beam shape scale"; return false; }
    std::vector<Point> next;
    for (auto entry=document["data"].beginArray(); entry!=document["data"].endArray(); ++entry)
    {
        const auto& offset=(*entry)["offset"]; const auto& color=(*entry)["color"];
        if (!offset.isArray() || offset.size()!=3 || !color.isArray() || color.size()!=4)
        { error="Invalid beam shape point"; return false; }
        const auto horizontal=offset[1].asReal()*scale/(8.f/width)+left+width/2;
        const auto vertical=offset[2].asReal()*scale/(8.f/width)+bottom+height/2;
        if (!std::isfinite(horizontal) || !std::isfinite(vertical) || std::abs(horizontal)>1000000. || std::abs(vertical)>1000000.)
        { error="Beam shape coordinates exceed limits"; return false; }
        Point point{static_cast<int>(horizontal),static_cast<int>(vertical)};
        for (int channel=0; channel<4; ++channel)
        {
            point.color[channel]=static_cast<float>(color[channel].asReal());
            if (!std::isfinite(point.color[channel])) { error="Nonfinite beam point color"; return false; }
        }
        next.push_back(point);
    }
    points=std::move(next); return true;
}
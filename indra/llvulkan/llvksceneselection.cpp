#include "linden_common.h"
#include "llvksceneselection.h"
#include "raytrace.h"
#include <algorithm>
#include <cmath>

namespace
{
    bool finite(const LLVector3& value)
    {
        return std::isfinite(value.mV[0]) && std::isfinite(value.mV[1]) && std::isfinite(value.mV[2]);
    }

    bool valid(const LLVKSceneSelection::Segment& segment)
    {
        const auto direction = segment.end-segment.start;
        const auto length = direction.magVecSquared();
        return finite(segment.start) && finite(segment.end) && std::isfinite(length) && length > 0.f;
    }
}

bool LLVKSceneSelection::publish(Scene scene, std::string& error)
{
    error.clear();
    if (!scene.revision || !scene.originEpoch || scene.triangles.size() > 1024*1024 ||
        (mScene && scene.revision <= mScene->revision))
    { error = "Native selection scene requires a newer revision, an origin epoch and bounded geometry"; return false; }
    for (const auto& triangle : scene.triangles)
    {
        if (triangle.object.isNull() || triangle.face < 0)
        { error = "Native selection triangle requires object and face identity"; return false; }
        for (std::size_t corner = 0; corner < 3; ++corner)
            if (!finite(triangle.positions[corner]) || !std::isfinite(triangle.uv[corner].mV[0]) || !std::isfinite(triangle.uv[corner].mV[1]))
            { error = "Native selection geometry is not finite"; return false; }
        const auto normal = (triangle.positions[1]-triangle.positions[0]) % (triangle.positions[2]-triangle.positions[0]);
        const auto area = normal.magVecSquared();
        if (!std::isfinite(area) || area <= 0.f)
        { error = "Native selection triangle is degenerate"; return false; }
    }
    mScene = std::make_shared<const Scene>(std::move(scene));
    mSelected.reset();
    return true;
}

bool LLVKSceneSelection::pick(const Request& request, std::optional<Hit>& hit, std::string& error) const
{
    error.clear();
    hit.reset();
    const auto snapshot = mScene;
    if (!snapshot || request.originEpoch != snapshot->originEpoch)
    { error = "Native selection requires geometry in the requested origin epoch"; return false; }
    if (!valid(request.world) || (request.hud && !valid(*request.hud)) || request.face < -1)
    { error = "Native selection requires finite nonzero segments and a valid face filter"; return false; }
    for (const bool hud : {true,false})
    {
        if (hud && !request.hud) continue;
        const auto& segment = hud ? *request.hud : request.world;
        const auto direction = segment.end-segment.start;
        const auto lengthSquared = direction.magVecSquared();
        for (const auto& triangle : snapshot->triangles)
        {
            if (triangle.hud != hud || (!request.object.isNull() && triangle.object != request.object) ||
                (request.face >= 0 && triangle.face != request.face) ||
                (triangle.transparent && !request.transparent) || (triangle.rigged && !request.rigged) ||
                (!triangle.selectable && !request.unselectable) || (triangle.reflectionProbe && !request.reflectionProbes)) continue;
            LLVector3 position, normal;
            if (!ray_triangle(segment.start,direction,triangle.positions[0],triangle.positions[1],triangle.positions[2],position,normal)) continue;
            const auto fraction = ((position-segment.start)*direction)/lengthSquared;
            if (!finite(position) || !finite(normal) || !std::isfinite(fraction) || fraction < 0.f || fraction > 1.f ||
                (hit && fraction >= hit->fraction)) continue;
            const auto sideFirst = triangle.positions[1]-triangle.positions[0];
            const auto sideSecond = triangle.positions[2]-triangle.positions[0];
            const auto offset = position-triangle.positions[0];
            const auto firstDot = double(sideFirst*sideFirst);
            const auto secondDot = double(sideSecond*sideSecond);
            const auto crossDot = double(sideFirst*sideSecond);
            const auto denominator = firstDot*secondDot-crossDot*crossDot;
            if (!std::isfinite(denominator) || denominator <= 0.)
            { error = "Native selection triangle cannot resolve surface coordinates"; hit.reset(); return false; }
            const auto firstWeight = float((secondDot*(offset*sideFirst)-crossDot*(offset*sideSecond))/denominator);
            const auto secondWeight = float((firstDot*(offset*sideSecond)-crossDot*(offset*sideFirst))/denominator);
            const auto uv = triangle.uv[0]*(1.f-firstWeight-secondWeight)+triangle.uv[1]*firstWeight+triangle.uv[2]*secondWeight;
            if (!std::isfinite(uv.mV[0]) || !std::isfinite(uv.mV[1]))
            { error = "Native selection surface coordinates overflowed"; hit.reset(); return false; }
            hit = Hit{triangle.object,triangle.face,snapshot->revision,snapshot->originEpoch,hud,position,normal,uv,fraction};
        }
        if (hit) return true;
    }
    return true;
}

bool LLVKSceneSelection::select(const Hit& hit, std::string& error)
{
    error.clear();
    if (!mScene || hit.revision != mScene->revision || hit.originEpoch != mScene->originEpoch ||
        !finite(hit.position) || !finite(hit.geometricNormal) || !std::isfinite(hit.fraction) || hit.fraction < 0.f || hit.fraction > 1.f ||
        !std::isfinite(hit.uv.mV[0]) || !std::isfinite(hit.uv.mV[1]) ||
        std::none_of(mScene->triangles.begin(),mScene->triangles.end(),[&](const auto& triangle)
        { return triangle.object == hit.object && triangle.face == hit.face && triangle.hud == hit.hud; }))
    { error = "Native selection result no longer belongs to the current scene"; return false; }
    mSelected = hit;
    return true;
}
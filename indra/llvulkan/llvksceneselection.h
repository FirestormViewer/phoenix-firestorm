#ifndef LLVKSCENESELECTION_H
#define LLVKSCENESELECTION_H

#include "lluuid.h"
#include "v2math.h"
#include "v3math.h"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class LLVKSceneSelection final
{
public:
    struct Triangle
    {
        LLUUID object;
        std::int32_t face = 0;
        std::array<LLVector3,3> positions;
        std::array<LLVector2,3> uv;
        bool hud = false, transparent = false, rigged = false, selectable = true, reflectionProbe = false;
    };
    struct Scene
    {
        std::uint64_t revision = 0, originEpoch = 0;
        std::vector<Triangle> triangles;
    };
    struct Segment { LLVector3 start, end; };
    struct Request
    {
        std::uint64_t originEpoch = 0;
        Segment world;
        std::optional<Segment> hud;
        LLUUID object;
        std::int32_t face = -1;
        bool transparent = false, rigged = false, unselectable = false, reflectionProbes = false;
    };
    struct Hit
    {
        LLUUID object;
        std::int32_t face = 0;
        std::uint64_t revision = 0, originEpoch = 0;
        bool hud = false;
        LLVector3 position, geometricNormal;
        LLVector2 uv;
        float fraction = 0.f;
    };
    bool publish(Scene scene, std::string& error);
    bool pick(const Request& request, std::optional<Hit>& hit, std::string& error) const;
    bool select(const Hit& hit, std::string& error);
    void clearSelection() noexcept { mSelected.reset(); }
    const std::optional<Hit>& selected() const noexcept { return mSelected; }
    std::shared_ptr<const Scene> scene() const noexcept { return mScene; }
private:
    std::shared_ptr<const Scene> mScene;
    std::optional<Hit> mSelected;
};

#endif
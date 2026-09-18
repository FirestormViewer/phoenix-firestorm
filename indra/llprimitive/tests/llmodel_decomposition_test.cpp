#include "linden_common.h"
#include "lltut.h"
#include "llmodel.h"
#include "llcontrol.h"
#include <stdexcept>

LLControlGroup gSavedSettings("model_decomposition_test");

std::string stripSuffix(std::string)
{
    throw std::logic_error("Decomposition tests must not invoke model-name processing");
}

namespace tut
{
    struct model_decomposition_data
    {
        LLSD makeData(const LLSD::Binary& counts, size_t bytes)
        {
            LLSD data;
            data["HullList"] = counts;
            data["Positions"] = LLSD::Binary(bytes, 0);
            return data;
        }
    };

    typedef test_group<model_decomposition_data> model_decomposition_group;
    typedef model_decomposition_group::object model_decomposition_object;
    model_decomposition_group model_decomposition_tests("llmodel_decomposition");

    template<> template<>
    void model_decomposition_object::test<1>()
    {
        LLSD data = makeData({4}, 24);
        LLModel::Decomposition decoded(data);
        ensure_equals("one hull", decoded.mHull.size(), size_t(1));
        ensure_equals("four vertices", decoded.mHull[0].size(), size_t(4));
        ensure_equals("default minimum", decoded.mHull[0][0].mV[0], -0.5f);
    }

    template<> template<>
    void model_decomposition_object::test<2>()
    {
        for (size_t bytes : {size_t(0), size_t(1), size_t(5), size_t(23)})
        {
            LLSD data = makeData({4}, bytes);
            LLModel::Decomposition decoded(data);
            ensure("truncated positions rejected", decoded.mHull.empty());
        }
    }

    template<> template<>
    void model_decomposition_object::test<3>()
    {
        LLSD data = makeData({4, 4}, 42);
        LLModel::Decomposition decoded(data);
        ensure("partial multi-hull result discarded", decoded.mHull.empty());
    }

    template<> template<>
    void model_decomposition_object::test<4>()
    {
        LLSD data = makeData({0}, 256 * 6);
        LLModel::Decomposition decoded(data);
        ensure_equals("one 256-vertex hull", decoded.mHull.size(), size_t(1));
        ensure_equals("zero encodes 256 vertices", decoded.mHull[0].size(), size_t(256));
        data["Positions"] = LLSD::Binary(256 * 6 - 1, 0);
        decoded.fromLLSD(data);
        ensure("truncated 256-vertex hull rejected", decoded.mHull.empty());
    }

    template<> template<>
    void model_decomposition_object::test<5>()
    {
        LLSD data = makeData({}, 0);
        LLModel::Decomposition decoded(data);
        ensure("empty decomposition accepted", decoded.mHull.empty());
    }

    template<> template<>
    void model_decomposition_object::test<6>()
    {
        LLSD data = makeData({4, 4}, 48);
        data["Min"] = LLVector3(-2.f, -4.f, -6.f).getValue();
        data["Max"] = LLVector3(2.f, 4.f, 6.f).getValue();
        const U16 coordinates[] = {0, 32768, 65535};
        LLSD::Binary positions(49, 0);
        for (size_t vertex = 0; vertex < 8; ++vertex)
        {
            memcpy(positions.data() + vertex * sizeof(coordinates), coordinates, sizeof(coordinates));
        }
        data["Positions"] = positions;
        LLModel::Decomposition decoded(data);
        ensure_equals("two valid hulls", decoded.mHull.size(), size_t(2));
        for (const auto& hull : decoded.mHull)
        {
            ensure_equals("four vertices per hull", hull.size(), size_t(4));
            for (const auto& vertex : hull)
            {
                ensure_equals("custom minimum", vertex.mV[0], -2.f);
                ensure_equals("quantized midpoint", vertex.mV[1], 32768.f / 65535.f * 8.f - 4.f);
                ensure_equals("custom maximum", vertex.mV[2], 6.f);
            }
        }
    }
}
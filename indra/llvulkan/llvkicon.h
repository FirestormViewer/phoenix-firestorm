#ifndef LLVKICON_H
#define LLVKICON_H

#include "llvkwidgetimage.h"
#include "llvkcolor.h"
#include <array>

struct LLVKIcon
{
    struct Params
    {
        std::shared_ptr<const LLVKWidgetImage> image;
        LLVKColor color{1.f,1.f,1.f,1.f};
        bool useDrawContextAlpha = true;
        bool interactable = false;
        std::int32_t minimumWidth = 0;
        std::int32_t minimumHeight = 0;
    };
    Params params;
    std::shared_ptr<const LLVKWidgetImage> image;
    std::int32_t desiredImageWidth = 0;
    std::int32_t desiredImageHeight = 0;
};

#endif
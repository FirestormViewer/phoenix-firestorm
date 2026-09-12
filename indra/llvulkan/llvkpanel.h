#ifndef LLVKPANEL_H
#define LLVKPANEL_H

#include "llvkborder.h"
#include "llvkcontrol.h"
#include "llvklabel.h"
#include "llvkwidgetimage.h"

struct LLVKPanel
{
    struct Params
    {
        bool hasBorder = false;
        LLVKBorder::Params border;
        bool backgroundVisible = false;
        bool backgroundOpaque = false;
        bool acceptsBadge = true;
        LLVKColor opaqueColor{1,1,1,1}, transparentColor{1,1,1,1};
        LLVKColor opaqueImageOverlay{1,1,1,1}, transparentImageOverlay{1,1,1,1};
        std::shared_ptr<const LLVKWidgetImage> opaqueImage, transparentImage;
        std::string label;
        std::string helpTopic;
        std::string filename;
        std::map<std::string,std::string> strings;
        LLVKControl::Callback visible;
    };
    Params params;
    std::uint64_t border = 0;
    std::uint64_t defaultButton = 0;
    LLVKLabel label;
    std::map<std::string,LLVKLabel> strings;
    LLVKControl::Callback visible;
};

#endif
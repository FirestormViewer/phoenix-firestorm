#ifndef LLVKTEXTURECTRL_H
#define LLVKTEXTURECTRL_H

#include "llvkcontrol.h"
#include "llvkplaincontrol.h"
#include "llvkborder.h"
#include "lluuid.h"

struct LLVKTextureCtrl
{
    struct Selection
    {
        LLUUID asset, item, tracking;
        bool operator==(const Selection&) const = default;
    };
    enum class Operation { Preview, Select, Cancel };
    struct Params
    {
        LLUUID initialAsset, defaultAsset;
        std::string label;
        int captionHeight = 23, labelWidth = -1;
        bool allowNone = false, applyImmediately = false, commitOnSelection = true;
        LLVKControl::Params captionControl;
        LLVKPlainControl::Params caption;
        LLVKControl::Params multipleControl;
        LLVKPlainControl::Params multiple;
        std::string multipleLabel = "Multiple";
        LLVKBorder::Params border;
        LLVKColor borderColor{1,1,1,1};
        LLVKColor enabledText{1,1,1,1}, disabledText{0.5f,0.5f,0.5f,1};
        std::shared_ptr<const LLVKWidgetImage> fallback;
        std::function<void(LLVKControl::Id,bool)> showPicker;
        LLVKControl::Callback selected, cancelled;
    };
    std::shared_ptr<const Params> params;
    Selection current, original, pending;
    std::shared_ptr<const LLVKWidgetImage> preview;
    std::uint64_t generation = 0;
    LLVKControl::Id caption = 0, border = 0, multiple = 0;
    bool picking = false, valid = true, previewHasAlpha = false;
};

#endif
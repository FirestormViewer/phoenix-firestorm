#ifndef LLVKCONTROL_H
#define LLVKCONTROL_H

#include "llsd.h"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

class LLVKFont;

struct LLVKControl
{
    using Id = std::uint64_t;
    struct Callback
    {
        std::function<void(Id,const LLSD&)> function;
        std::optional<LLSD> parameter;
    };
    struct Validation
    {
        std::function<bool(Id,const LLSD&)> function;
        std::optional<LLSD> parameter;
    };
    struct Params
    {
        std::shared_ptr<LLVKFont> font;
        std::optional<LLSD> initialValue;
        std::optional<std::string> valueSetting;
        std::optional<std::string> enabledSetting;
        bool invertEnabled = false;
        std::optional<std::string> visibleSetting;
        std::optional<std::string> invisibleSetting;
        Callback init, commit, mouseEnter, mouseLeave;
        Validation validate;
        bool tabStop = true;
        bool chrome = false;
        bool requestsFront = false;
    };
    Params params;
    LLSD value;
    bool dirty = false;
    bool tentative = false;
};

#endif
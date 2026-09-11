#ifndef LLVKSCROLL_H
#define LLVKSCROLL_H

#include <cstdint>
#include <optional>
#include <string>

class LLVKScrollLayout final
{
public:
    struct Params
    {
        std::int32_t width = 0, height = 0;
        std::int32_t documentWidth = 0, documentHeight = 0;
        std::int32_t borderWidth = 0, scrollbarSize = 0;
        bool hideScrollbars = false;
    };
    struct Visible
    {
        std::int32_t width = 0, height = 0;
        bool horizontal = false, vertical = false;
    };
    static std::optional<Visible> visible(const Params& params, std::string& error);
    struct ThumbParams
    {
        std::int32_t documentSize = 0, pageSize = 0, position = 0;
        std::int32_t length = 0, thickness = 0;
        bool vertical = true;
    };
    struct Rect
    {
        std::int32_t left = 0, bottom = 0, right = 0, top = 0;
        auto operator<=>(const Rect&) const = default;
    };
    static std::optional<Rect> thumb(const ThumbParams& params, std::string& error);
};

#endif
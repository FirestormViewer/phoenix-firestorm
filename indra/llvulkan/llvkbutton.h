#ifndef LLVKBUTTON_H
#define LLVKBUTTON_H

#include "llvkcontrol.h"
#include "llvkwidgetimage.h"
#include "llvklabel.h"
#include "llvkcolor.h"
#include <array>

struct LLVKButton
{
    using Image = std::shared_ptr<const LLVKWidgetImage>;
    enum class Align { Left, Center, Right };
    struct Images
    {
        Image unselected, selected, disabled, disabledSelected;
        Image hover, hoverSelected, pressed, pressedSelected, overlay, flash;
    };
    struct Params
    {
        std::u32string label;
        std::optional<std::u32string> selectedLabel;
        Images images;
        Images defaultImages;
        bool pressedProvided = false;
        bool pressedSelectedProvided = false;
        std::int32_t leftPad = 0;
        std::int32_t rightPad = 0;
        std::int32_t originalHorizontalPad = 0;
        std::int32_t bottomPad = 0;
        std::int32_t overlayTopPad = 0;
        std::int32_t overlayBottomPad = 0;
        std::int32_t overlayLabelSpace = 1;
        Align overlayAlign = Align::Left;
        Align labelAlign = Align::Center;
        float scaleX = 1.f;
        bool autoResize = false;
        bool toggle = false;
        bool commitOnReturn = true;
        bool commitOnCaptureLost = false;
        bool displayPressed = true;
        bool handleRightMouse = true;
        bool scaleImage = true;
        bool useEllipses = false;
        bool useFontColor = false;
        bool labelShadow = true;
        bool useDrawContextAlpha = true;
        bool drawFocusBorder = true;
        bool hoverHandCursor = false;
        bool flashEnable = false;
        std::int32_t flashCount = 0;
        float flashPeriod = 0.f;
        double heldSeconds = 0.5;
        std::uint64_t heldFrames = 0;
        std::optional<LLVKControl::Callback> click;
        LLVKControl::Callback mouseDown, mouseUp, held;
        bool menuButton = false;
        LLVKControl::Validation isToggled;
        LLVKColor labelColor{1,1,1,1};
        LLVKColor selectedLabelColor{1,1,1,1};
        LLVKColor disabledLabelColor{1,1,1,1};
        LLVKColor disabledSelectedLabelColor{1,1,1,1};
        LLVKColor imageColor{1,1,1,1}, disabledImageColor{1,1,1,1};
        LLVKColor flashColor{1,1,1,1}, alternateFlashColor{1,1,1,1};
        LLVKColor overlayColor{1,1,1,0.75f}, disabledOverlayColor{1,1,1,0.3f}, selectedOverlayColor{1,1,1,1};
        float hoverGlow = 0.f;
        std::int32_t overlayRightDelta = 0;
    };
    Params params;
    LLVKLabel labelSource, selectedLabelSource;
    std::u32string selectedLabel;
    Images images;
    std::int32_t leftPad = 0;
    std::int32_t rightPad = 0;
    bool fadeWhenDisabled = false;
    bool flashing = false;
    bool highlighted = false;
    bool forcePressed = false;
    float glow = 0.f;
    struct FlashTimer
    {
        std::uint64_t limit = 0;
        std::uint64_t ticks = 0;
        float period = 0.f;
        double resetTime = 0.0;
        bool running = false;
        bool highlighted = false;
    };
    std::optional<FlashTimer> flashTimer;
    bool forceFlashing = false;
    bool alternateFlashColor = false;
    double flashResetTime = 0.0;
    std::optional<double> mouseDownTime;
    std::uint64_t mouseDownFrame = 0;
    std::uint64_t heldCount = 0;
    std::uint64_t textGeneration = 0;
    std::uint64_t badge = 0;
    bool hasBadgeHolderParent = false;
};

#endif
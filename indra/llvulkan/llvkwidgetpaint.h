#ifndef LLVKWIDGETPAINT_H
#define LLVKWIDGETPAINT_H

#include "llvkwidgettree.h"

class LLVKWidgetPaint final
{
public:
    using Id = LLVKWidgetTree::Id;
    using Rect = LLVKWidgetTree::Rect;
    struct Input
    {
        LLVKWidgetTree::ButtonView button;
        LLVKWidgetTree::EditorView editor;
        std::map<Id,std::shared_ptr<const LLVKWidgetImage>> browsers;
        LLVKColor searchBackground{1,0,0,1},searchFont{1,0,0,1};
        std::map<Id,std::uint64_t> browserEpochs;
        double animationSeconds=0;
    };
    struct Command
    {
        Id owner = 0;
        Rect rectangle, clip;
        LLVKColor::Value color{1,1,1,1};
        std::shared_ptr<const LLVKWidgetImage> image;
        std::optional<LLVKFont::LineLayout> text;
        bool alphaMask = false, additive = false, shadow = false;
        bool streamingImage = false;
        std::optional<std::array<float,6>> triangle;
        std::uint64_t imageEpoch = 0;
        std::optional<std::array<LLVKColor::Value,3>> triangleColors;
    };
    std::vector<Command> commands;
    bool skinAnisotropy = false;
    std::vector<Id> pendingBrowsers;
    static std::optional<LLVKWidgetPaint> prepare(LLVKWidgetTree& tree, Id root, const Input& input, std::string& error);
};

#endif
#ifndef LLVKWIDGETFACTORY_H
#define LLVKWIDGETFACTORY_H

#include "llvkwidgetlayout.h"
#include <string_view>

class LLVKWidgetFactory final
{
public:
    struct Defaults
    {
        LLVKWidgetTree::Params view;
        LLVKWidgetLayout geometry;
    };
    struct IconDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKIcon::Params icon;
        IconDefaults();
    };
    struct BadgeDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKBadge::Params badge;
        BadgeDefaults();
    };
    struct ButtonDefaults
    {
        Defaults view;
        LLVKControl::Params control;
        LLVKButton::Params button;
        BadgeDefaults badge;
        std::optional<BadgeDefaults> providedBadge;
        ButtonDefaults();
    };
    struct Callbacks
    {
        std::map<std::string,std::function<void(LLVKWidgetTree::Id,const LLSD&)>> actions;
        std::map<std::string,std::function<bool(LLVKWidgetTree::Id,const LLSD&)>> predicates;
    };
    struct Resources
    {
        std::shared_ptr<LLVKColorTable> colors;
        std::map<std::string,std::shared_ptr<LLVKFont>> fonts;
    };
    explicit LLVKWidgetFactory(Defaults defaults);
    LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults);
    LLVKWidgetFactory(Defaults defaults, IconDefaults iconDefaults, ButtonDefaults buttonDefaults, Callbacks callbacks,
                       Resources resources = {});
    bool loadDefaults(const LLVKWidgetTree& tree, std::string_view xml, std::string& error);
    std::optional<LLVKWidgetTree::Id> construct(LLVKWidgetTree& tree, std::string_view xml,
                                              LLVKWidgetTree::Id parent, std::string& error) const;
    std::optional<LLVKWidgetTree::Id> construct(LLVKWidgetTree& tree,
                                              const LLVKWidgetTree::Params& params,
                                              LLVKWidgetTree::Id parent, std::string& error) const;
private:
    Defaults mDefaults;
    IconDefaults mIconDefaults;
    ButtonDefaults mButtonDefaults;
    Callbacks mCallbacks;
    Resources mResources;
};

#endif
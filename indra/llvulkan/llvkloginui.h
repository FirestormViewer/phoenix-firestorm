#ifndef LLVKLOGINUI_H
#define LLVKLOGINUI_H

#include "llvkwidgetfactory.h"
#include "llvkwidgetpaint.h"

class LLVKLoginUi final
{
public:
    struct Configuration
    {
        LLVKSkinFiles::Configuration skin;
        LLVKFontRegistry::Configuration fonts;
        std::filesystem::path fontDescription;
        std::map<std::string,LLSD> settings;
        LLVKLabel::Context labels;
    };
    static std::unique_ptr<LLVKLoginUi> create(const Configuration& configuration, std::string& error);
    struct Page
    {
        std::string url = "https://phoenixviewer.com/app/loginV3/";
        std::string language, version, channel, grid, operatingSystem, skin, theme;
        std::map<std::string,LLSD> settings;
    };
    static std::string pageUrl(const Page& page);
    LLVKWidgetTree& tree() noexcept { return mTree; }
    LLVKWidgetTree::Id root() const noexcept { return mRoot; }
    LLVKWidgetTree::Id find(std::string_view name) const;
private:
    LLVKWidgetTree mTree;
    LLVKWidgetTree::Id mRoot = 0;
    std::shared_ptr<LLVKFontRegistry> mFonts;
    std::shared_ptr<LLVKSkinFiles> mSkin;
    std::shared_ptr<LLVKColorTable> mColors;
};

#endif
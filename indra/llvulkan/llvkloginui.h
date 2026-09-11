#ifndef LLVKLOGINUI_H
#define LLVKLOGINUI_H

#include "llvkwidgetfactory.h"
#include "llvkwidgetpaint.h"
#include "llvkloginmenu.h"
#include "llvkfloater.h"

class LLVKLoginUi final
{
public:
    struct Configuration
    {
        LLVKSkinFiles::Configuration skin;
        LLVKFontRegistry::Configuration fonts;
        std::filesystem::path fontDescription;
        std::map<std::string,LLSD> settings;
        std::string appliedSettingsMode;
        LLVKLabel::Context labels;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> savePreferences;
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
    LLVKLoginMenu& menu() noexcept { return *mMenu; }
    std::optional<LLVKWidgetPaint> preparePaint(const LLVKWidgetPaint::Input& input, std::string& error);
    bool showPreferences(std::string& error);
    bool showAbout(std::string& error);
    bool applyPreferences(std::string& error);
    bool closeFloater(std::string& error);
    bool floaterPointer(const LLVKWidgetTree::PointerEvent& event,std::string& error);
    bool floaterWheel(int x,int y,int clicks,std::string& error);
    LLVKWidgetTree::Id activeFloater() const;
    bool pointOverFloater(int x,int y) const;
    void setAboutInfo(std::string info);
    bool setAboutInfo(const LLSD& info, std::string& error);
    void setDialogClipboard(std::shared_ptr<LLVKClipboard> clipboard) { mDialogClipboard = std::move(clipboard); }
    void setOpenUrl(std::function<void(const std::string&)> callback) { mOpenUrl = std::move(callback); }
    void setPointerCursor(std::function<void(bool)> callback) { mPointerCursor = std::move(callback); }
    const std::string& dialogError() const noexcept { return mDialogError; }
    std::string takeDialogError() { auto error = std::move(mDialogError); mDialogError.clear(); return error; }
private:
    bool initializeDialogs(const Configuration& configuration,std::string& error);
    bool updateAboutText(std::string& error);
    bool selectAboutPage(std::size_t page,std::string& error);
    std::unique_ptr<LLVKWidgetFactory> mDialogFactory;
    LLVKWidgetTree mTree;
    LLVKWidgetTree::Id mRoot = 0;
    std::shared_ptr<LLVKFontRegistry> mFonts;
    std::shared_ptr<LLVKSkinFiles> mSkin;
    std::shared_ptr<LLVKColorTable> mColors;
    std::unique_ptr<LLVKLoginMenu> mMenu;
    std::unique_ptr<LLVKFloater> mPreferences, mAbout;
    LLVKFloater* mActiveFloater = nullptr;
    std::map<std::string,LLSD> mPreferenceSnapshot;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSavePreferences;
    std::shared_ptr<LLVKClipboard> mDialogClipboard;
    std::function<void(const std::string&)> mOpenUrl;
    std::function<void(bool)> mPointerCursor;
    std::string mAboutInfo, mDialogError;
    std::string mAppliedSettingsMode;
    LLVKWidgetTree::Id mAboutBody = 0, mAboutScrollContainer = 0, mAboutDocument = 0;
    LLVKWidgetTree::Id mAboutIntro = 0, mAboutCopy = 0;
    std::vector<std::pair<std::string,std::string>> mAboutPages;
    std::vector<std::string> mAboutPageNames;
    std::map<std::string,std::string> mAboutStrings;
    std::vector<LLVKWidgetTree::Id> mAboutTabs;
    std::size_t mAboutPage = 0;
    bool mPreferencesAccepted = false;
};

#endif
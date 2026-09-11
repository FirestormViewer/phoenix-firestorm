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
        std::map<std::string,LLSD> settingDefaults;
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
    std::optional<LLVKWidgetTree::Id> constructPreferencePanel(const std::string& filename,
        LLVKWidgetTree::Id parent, std::string& error);
    bool showAbout(std::string& error);
    bool showColorPicker(LLVKWidgetTree::Id swatch, bool takeFocus, std::string& error);
    bool applyPreferences(std::string& error);
    bool resetPreference(const std::string& name, std::string& error);
    bool closeFloater(std::string& error);
    bool floaterPointer(const LLVKWidgetTree::PointerEvent& event,std::string& error);
    bool floaterWheel(int x,int y,int clicks,std::string& error);
    LLVKWidgetTree::Id activeFloater() const;
    bool pointOverFloater(int x,int y) const;
    void setAboutInfo(std::string info);
    bool setAboutInfo(const LLSD& info, std::string& error);
    void setDialogClipboard(std::shared_ptr<LLVKClipboard> clipboard) { mTree.setClipboard(clipboard); mDialogClipboard = std::move(clipboard); }
    void setOpenUrl(std::function<void(const std::string&)> callback) { mOpenUrl = std::move(callback); }
    void setPointerCursor(std::function<void(bool)> callback) { mPointerCursor = std::move(callback); }
    const std::string& dialogError() const noexcept { return mDialogError; }
    std::string takeDialogError() { auto error = std::move(mDialogError); mDialogError.clear(); return error; }
    struct Notice { std::string name, message; };
    std::vector<Notice> takeNotices() { auto notices=std::move(mNotices); mNotices.clear(); return notices; }
    bool advanceNotices(double time, std::string& error);
    bool noticeKey(bool returnKey, bool modified, std::string& error);
    LLVKWidgetTree::Id modalNotice() const noexcept { return mNoticePanel; }
private:
    bool initializeDialogs(const Configuration& configuration,std::string& error);
    bool updateAboutText(std::string& error);
    bool initializeStartupPreferencePanel(LLVKWidgetTree::Id panel, std::string& error);
    bool updateStartupPreferenceMaturity(LLVKWidgetTree::Id panel, std::string& error);
    std::vector<LLVKFloater*> floaters() const;
    std::unique_ptr<LLVKWidgetFactory> mDialogFactory;
    LLVKWidgetTree mTree;
    LLVKWidgetTree::Id mRoot = 0;
    std::shared_ptr<LLVKFontRegistry> mFonts;
    std::shared_ptr<LLVKSkinFiles> mSkin;
    std::shared_ptr<LLVKColorTable> mColors;
    std::unique_ptr<LLVKLoginMenu> mMenu;
    std::unique_ptr<LLVKFloater> mPreferences, mAbout;
    std::map<LLVKWidgetTree::Id,std::unique_ptr<LLVKFloater>> mColorPickers;
    LLVKFloater* mActiveFloater = nullptr;
    LLVKWidgetTree::PreferenceSnapshot mPreferenceSnapshot;
    std::map<std::string,LLSD> mSettingDefaults;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSavePreferences;
    std::shared_ptr<LLVKClipboard> mDialogClipboard;
    std::function<void(const std::string&)> mOpenUrl;
    std::function<void(bool)> mPointerCursor;
    std::string mAboutInfo, mDialogError;
    std::string mAppliedSettingsMode;
    LLVKWidgetTree::Id mAboutBody = 0;
    std::string mAboutContributors;
    std::optional<std::string> mAboutLicenses;
    std::map<std::string,std::string> mAboutStrings;
    std::vector<Notice> mNotices;
    LLVKWidgetTree::Id mNoticePanel = 0, mNoticeButton = 0, mNoticePreviousFocus = 0;
    double mNoticeOpened = 0.0, mNoticeTime = 0.0;
    bool dismissNotice(std::string& error);
    bool mPreferencesAccepted = false;
};

#endif
#ifndef LLVKLOGINUI_H
#define LLVKLOGINUI_H

#include "llvkwidgetfactory.h"
#include "llvkwidgetpaint.h"
#include "llvkloginmenu.h"
#include "llvkfloater.h"
#include "llvkstartupsettings.h"
#include "llvkspellcheck.h"

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
        std::map<std::string,LLSD> accountSettings, accountDefaults;
        std::string appliedSettingsMode;
        LLVKLabel::Context labels;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> savePreferences;
        LLSD autoReplaceLists = LLSD::emptyArray();
        std::function<bool(const LLSD&,std::string&)> saveAutoReplace;
        std::filesystem::path dictionaryDirectory;
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
    bool showAutoReplace(std::string& error);
    using XmlFileResult = std::function<void(std::optional<std::filesystem::path>,std::string)>;
    using XmlFilePicker = std::function<bool(bool,const std::string&,XmlFileResult,std::string&)>;
    void setXmlFilePicker(XmlFilePicker picker) { mXmlFilePicker=std::move(picker); }
    void setDictionaryFilePicker(XmlFilePicker picker) { mDictionaryFilePicker=std::move(picker); }
    bool showSpellCheck(std::string& error);
    bool showSpellImport(std::string& error);
    using TranslationVerifier = std::function<bool(const std::string&,const LLSD&,std::function<void(bool,int)>,std::string&)>;
    void setTranslationVerifier(TranslationVerifier verifier) { mTranslationVerifier=std::move(verifier); }
    bool showTranslation(std::string& error);
    LLVKSpellCheck& spellCheck() noexcept { return *mSpelling; }
    bool showColorPicker(LLVKWidgetTree::Id swatch, bool takeFocus, std::string& error);
    bool applyPreferences(std::string& error);
    bool resetPreference(const std::string& name, std::string& error);
    bool resetAccountPreference(const std::string& name, std::string& error);
    bool previewUiSound(const std::string& name, std::string& error);
    void setUiSoundPlayer(std::function<bool(const std::string&,std::string&)> player) { mUiSoundPlayer=std::move(player); }
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
    struct Notice
    {
        struct Button { std::string name, label; int option = 0; bool isDefault = false; };
        std::string name, message;
        std::vector<Button> buttons;
        std::string inputName;
        std::function<void(int,const LLSD&)> response;
    };
    bool queueNotice(const std::string& name, const LLSD& arguments,
        std::function<void(int,const LLSD&)> response, std::string& error);
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
    std::unique_ptr<LLVKFloater> mAutoReplace;
    LLVKAutoReplaceSettings mAutoReplaceSettings, mAutoReplaceDraft;
    std::map<std::string,LLVKWidgetTree::Id> mAutoReplaceFields;
    std::string mAutoReplaceList, mAutoReplaceKeyword;
    std::uint64_t mAutoReplaceGeneration = 0;
    std::function<bool(const LLSD&,std::string&)> mSaveAutoReplace;
    bool refreshAutoReplace(bool entries, std::string& error);
    void enableAutoReplaceEntry(bool enabled);
    void promptAutoReplaceList(LLSD list, bool conflict);
    void chooseAutoReplaceFile(bool save);
    XmlFilePicker mXmlFilePicker;
    XmlFilePicker mDictionaryFilePicker;
    std::unique_ptr<LLVKSpellCheck> mSpelling;
    std::unique_ptr<LLVKFloater> mSpellCheck, mSpellImport;
    std::map<std::string,LLVKWidgetTree::Id> mSpellFields, mSpellImportFields;
    std::map<std::string,LLSD> mSpellSnapshot;
    bool mRefreshingSpelling = false, mSpellMainChanged = false;
    std::uint64_t mSpellImportGeneration = 0;
    std::filesystem::path mSpellImportPath;
    bool updateSpelling(std::string& error);
    bool refreshSpellCheck(bool fromSettings, std::string& error);
    bool commitSpellCheck(std::string& error);
    void updateSpellRemoval();
    std::unique_ptr<LLVKFloater> mTranslation;
    std::map<std::string,LLVKWidgetTree::Id> mTranslationFields;
    std::map<std::string,bool> mTranslationVerified;
    std::map<std::string,std::uint64_t> mTranslationRequests;
    std::uint64_t mTranslationGeneration = 0;
    TranslationVerifier mTranslationVerifier;
    LLSD translationKey(const std::string& service) const;
    void updateTranslationControls();
    void invalidateTranslation(const std::string& service);
    void verifyTranslation(const std::string& service, bool alert);
    std::map<LLVKWidgetTree::Id,std::unique_ptr<LLVKFloater>> mColorPickers;
    LLVKFloater* mActiveFloater = nullptr;
    LLVKWidgetTree::PreferenceSnapshot mPreferenceSnapshot;
    std::map<std::string,LLSD> mSettingDefaults;
    std::map<std::string,LLSD> mAccountDefaults;
    std::function<bool(const std::string&,std::string&)> mUiSoundPlayer;
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
    std::map<std::string,Notice> mNoticeTemplates;
    std::optional<Notice> mActiveNotice;
    std::map<LLVKWidgetTree::Id,int> mNoticeOptions;
    LLVKWidgetTree::Id mNoticeEditor = 0;
    LLVKWidgetTree::Id mNoticePanel = 0, mNoticeButton = 0, mNoticePreviousFocus = 0;
    double mNoticeOpened = 0.0, mNoticeTime = 0.0;
    bool dismissNotice(std::string& error);
    bool respondNotice(int option, std::string& error);
    bool mPreferencesAccepted = false;
};

#endif
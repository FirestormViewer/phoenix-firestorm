#ifndef LLVKVIEWERUI_H
#define LLVKVIEWERUI_H

#include "llvkvoice.h"

#include "llvkwidgetfactory.h"
#include "llvkwidgetpaint.h"
#include "llvkmenu.h"
#include "llvkfloater.h"
#include "llvksettingsmgr.h"
#include "llvkspellcheck.h"
#include "llvkkeybindings.h"
#include "llvkjoystick.h"
#include "llvkmediafilter.h"
#include "llvkmutelist.h"
#include "llvkproxy.h"
#include "llvkbeamcolor.h"
#include "llvkbeamshape.h"
#include "llvkgraphicpresets.h"
#include "llvkgraphicspolicy.h"

class LLVKViewerUi final
{
public:
    struct BackupRequest;
    struct Configuration
    {
        LLVKSkinFiles::Configuration skin;
        LLVKFontRegistry::Configuration fonts;
        std::filesystem::path fontDescription;
        std::vector<std::filesystem::path> fontPresetDirectories;
        std::map<std::string,LLSD> settings;
        LLControlGroup* settingsGroup = nullptr;
        LLControlGroup* accountSettingsGroup = nullptr;
        LLControlGroup* warningSettingsGroup = nullptr;
        bool desktopNotificationsAvailable = false;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> saveWarningPreferences;
        std::map<std::string,LLSD> settingDefaults;
        std::map<std::string,LLSD> accountSettings, accountDefaults;
        bool accountSettingsLoaded = false;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> saveAccountPreferences;
        std::map<std::string,LLSD> crashSettings;
        bool crashSettingsRequireRestart = false;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> saveCrashPreferences;
        std::function<bool(std::string&)> scheduleSettingsReset;
        std::string appliedSettingsMode;
        LLVKLabel::Context labels;
        std::function<bool(const std::map<std::string,LLSD>&,std::string&)> savePreferences;
        std::function<bool(const LLVKKeyBindings&,std::string&)> saveKeyBindings;
        LLSD autoReplaceLists = LLSD::emptyArray();
        std::function<bool(const LLSD&,std::string&)> saveAutoReplace;
        std::filesystem::path dictionaryDirectory;
        LLSD mediaFilterRules;
        std::function<bool(const LLSD&,std::string&)> saveMediaFilterRules;
        std::function<std::optional<LLVKProxy::Credentials>(std::string&)> loadProxyCredentials;
        std::function<bool(const std::optional<LLVKProxy::Credentials>&,std::string&)> saveProxyCredentials;
        std::filesystem::path cacheDirectory, defaultCacheDirectory;
        std::filesystem::path userColorsFile;
        std::function<bool(const BackupRequest&,std::string&)> backupHandler;
        std::function<void()> clearSpamQueues;
    };
    static std::unique_ptr<LLVKViewerUi> create(const Configuration& configuration, std::string& error);
    struct Page
    {
        std::string url = "https://phoenixviewer.com/app/loginV3/";
        std::string language, version, channel, grid, operatingSystem, skin, theme;
        std::map<std::string,LLSD> settings;
    };
    static std::string pageUrl(const Page& page);
    LLVKWidgetTree& tree() noexcept { return mTree; }
    LLVKWidgetTree::Id root() const noexcept { return mRoot; }
    LLVKWidgetTree::Id find(std::string_view name,LLVKWidgetTree::Id within = 0) const;
    LLVKMenu& menu() noexcept { return *mMenu; }
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
    void setExecutableFilePicker(XmlFilePicker picker) { mExecutableFilePicker=std::move(picker); }
    using DirectoryPicker = std::function<bool(const std::filesystem::path&,XmlFileResult,std::string&)>;
    void setDirectoryPicker(DirectoryPicker picker) { mDirectoryPicker=std::move(picker); }
    void setDirectoryOpener(std::function<bool(const std::filesystem::path&,std::string&)> opener) { mDirectoryOpener=std::move(opener); }
    void setQuitRequestHandler(std::function<void()> handler) { mQuitRequest=std::move(handler); }
    void setGraphicsPreferenceHandler(std::function<bool(const std::string&,const LLSD&,std::string&)> handler)
    { mGraphicsPreferenceHandler=std::move(handler); }
    void setGraphicsDevice(const LLVKGraphicsPolicy::Device& device) { mGraphicsDevice=device; }
    void setViewerPreferenceHandler(std::function<bool(const std::string&,std::string&)> handler)
    { mViewerPreferenceHandler=std::move(handler); }
    struct BackupRequest
    {
        std::filesystem::path directory;
        bool restore = false, globalSettings = true, accountSettings = false;
        std::vector<std::string> globalFiles, accountFiles, folders;
        std::map<std::string,LLSD> recommendedGraphics;
    };
    void setBackupHandler(std::function<bool(const BackupRequest&,std::string&)> handler) { mBackupHandler=std::move(handler); }
    bool showSpellCheck(std::string& error);
    bool showSpellImport(std::string& error);
    using TranslationVerifier = std::function<bool(const std::string&,const LLSD&,std::function<void(bool,int)>,std::string&)>;
    void setTranslationVerifier(TranslationVerifier verifier) { mTranslationVerifier=std::move(verifier); }
    bool showTranslation(std::string& error);
    bool showDefaultPermissions(std::string& error);
    bool showBeamColor(LLVKWidgetTree::Id owner,std::string& error);
    bool showBeamShape(LLVKWidgetTree::Id owner,std::string& error);
    bool showGraphicPreset(LLVKWidgetTree::Id owner,const std::string& action,std::string& error);
    struct JoystickServices
    {
        std::function<std::optional<std::vector<LLVKJoystick::Device>>(std::string&)> enumerate;
        std::function<std::optional<std::string>(const LLSD&,std::string&)> select;
        std::function<std::optional<LLVKJoystick::State>(std::string&)> poll;
    };
    void setJoystickServices(JoystickServices services) { mJoystickServices=std::move(services); }
    bool showJoystick(std::string& error);
    bool showMediaLists(std::string& error);
    bool showProxy(std::string& error);
    std::optional<LLVKProxy::Endpoint> httpProxy(std::string& error) const;
    std::optional<LLVKProxy::Credentials> proxyCredentials(std::string& error) const;
    bool showBlockList(std::string& error);
    void setPrivacyActionHandler(std::function<bool(const std::string&,std::string&)> handler) { mPrivacyActionHandler=std::move(handler); }
    const LLVKMediaFilter& mediaFilter() const noexcept { return mMediaFilter; }
    LLVKMuteList& muteList() noexcept { return mMuteList; }
    using VoiceDeviceState = LLVKVoice::State;
    struct VoiceDeviceServices
    {
        std::function<bool(std::string&)> refresh;
        std::function<std::optional<VoiceDeviceState>(std::string&)> state;
        std::function<bool(bool,const std::string&,std::string&)> select;
        std::function<bool(bool,float,std::string&)> tune;
    };
    void setVoiceDeviceServices(VoiceDeviceServices services) { mVoiceDeviceServices=std::move(services); }
    bool recordPreferenceKey(KEY key, MASK mask, bool down, std::string& error);
    bool recordPreferenceMouse(const LLVKWidgetTree::PointerEvent& event, EMouseClickType click, bool down, MASK mask, std::string& error);
    LLVKWidgetTree::Id keyCaptureDialog() const noexcept;
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
    bool initializeControlsPanel(LLVKWidgetTree::Id panel, std::string& error);
    bool refreshControlsPanel(LLVKWidgetTree::Id panel, std::string& error);
    bool updateClickActions(LLVKWidgetTree::Id panel, bool fromControls, std::string& error);
    bool refreshContactColumns(LLVKWidgetTree::Id panel, std::string& error);
    bool populateFontPresets(LLVKWidgetTree::Id combo, std::string& error);
    bool initializeUiSounds(LLVKWidgetTree::Id panel, std::string& error);
    bool refreshUiSounds(LLVKWidgetTree::Id panel, bool rebuild, std::string& error);
    void commitUiSound(LLVKWidgetTree::Id panel, const std::string& action);
    bool publishUiSoundChanges(const std::map<std::string,LLSD>& changes, std::string& error);
    std::string uiSoundString(LLVKWidgetTree::Id panel, const std::string& name) const;
    struct UiSoundPanel
    {
        std::map<std::string,LLVKWidgetTree::Id> fields;
        std::vector<std::string> sounds;
        std::string selected;
    };
    std::map<LLVKWidgetTree::Id,UiSoundPanel> mUiSoundPanels;
    bool initializeSoundPreferences(LLVKWidgetTree::Id panel, std::string& error);
    bool initializePrivacyPreferences(LLVKWidgetTree::Id panel, std::string& error);
    bool loadNotificationPreferences(const std::string& xml, const Configuration& configuration, std::string& error);
    bool refreshNotificationPreferences(LLVKWidgetTree::Id panel, bool fromList, std::string& error);
    struct NotificationPreference
    {
        std::string label, control;
        bool inverted=false, sessionOnly=false, saveResponse=false;
        int defaultOption=0;
        LLSD defaultResponse;
        std::map<std::string,int> responseOptions;
    };
    std::map<std::string,NotificationPreference> mNotificationPreferences;
    std::unique_ptr<LLControlGroup> mOwnedWarningSettings;
    LLControlGroup* mWarningSettings = nullptr;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSaveWarningPreferences;
    bool mDesktopNotificationsAvailable = false;
    void showButtonMenu(LLVKWidgetTree::Id button, const std::string& filename, const std::string& position,
        const LLVKWidgetFactory::Callbacks& callbacks);
    LLVKWidgetTree::Id mMenuButton = 0;
    LLVKMuteList mMuteList;
    struct BlockListPanel
    {
        std::map<std::string,LLVKWidgetTree::Id> fields;
        std::vector<LLVKMuteList::Entry> entries;
    };
    std::map<LLVKWidgetTree::Id,BlockListPanel> mBlockListPanels;
    bool initializeBlockList(LLVKWidgetTree::Id panel, std::string& error);
    bool refreshBlockList(LLVKWidgetTree::Id panel, std::string& error);
    void blockListAction(LLVKWidgetTree::Id panel, const std::string& action);
    bool blockListPredicate(LLVKWidgetTree::Id panel, const std::string& action, const std::string& kind) const;
    std::function<bool(const std::string&,std::string&)> mPrivacyActionHandler;
    bool updateSoundPreferences(LLVKWidgetTree::Id panel, const std::string& action, std::string& error);
    VoiceDeviceServices mVoiceDeviceServices;
    struct VoiceDevicePanel
    {
        std::map<std::string,LLVKWidgetTree::Id> fields;
        std::string input, output;
        float gain = 1.f;
        bool visible = false;
        std::uint64_t generation = 0;
    };
    std::map<LLVKWidgetTree::Id,VoiceDevicePanel> mVoiceDevicePanels;
    std::optional<std::pair<LLVKWidgetTree::Id,double>> mVoiceReset;
    bool initializeVoiceDevices(LLVKWidgetTree::Id panel, std::string& error);
    bool refreshVoiceDevices(std::string& error);
    std::vector<std::filesystem::path> mFontPresetDirectories;
    bool refreshCrashPreferences(LLVKWidgetTree::Id panel, bool fromSettings, std::string& error);
    std::map<std::string,LLSD> mCrashSettings;
    std::vector<LLVKWidgetTree::Id> mCrashPanels;
    bool mCrashSettingsRequireRestart = false;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSaveCrashPreferences;
    std::function<bool(std::string&)> mScheduleSettingsReset;
    LLVKKeyBindings mBindings, mDefaultBindings;
    std::optional<LLVKKeyBindings> mBindingSnapshot;
    std::function<bool(const LLVKKeyBindings&,std::string&)> mSaveKeyBindings;
    std::map<std::string,LLVKWidgetTree::Id> mKeyCaptureFields;
    LLVKWidgetTree::Id mKeyCapturePanel = 0, mKeyCapturePreviousFocus = 0;
    std::string mKeyCaptureCommand;
    LLVKKeyBindings::Mode mKeyCaptureMode = LLVKKeyBindings::Mode::ThirdPerson;
    std::uint32_t mKeyCaptureSlot = 0;
    KEY mLastModifierKey = KEY_NONE;
    std::optional<double> mKeyClickDeadline;
    MASK mKeyClickMask = 0;
    bool mKeyMouseRecorded = false;
    KEY mCapturedReleaseKey = KEY_NONE;
    bool showKeyCapture(LLVKWidgetTree::Id panel, std::string& error);
    bool applyCapturedBinding(const LLKeyData& data, bool defaults, bool allModes, std::string& error);
    bool reservedPreferenceKey(KEY key, MASK mask) const;
    bool updateStartupPreferenceMaturity(LLVKWidgetTree::Id panel, std::string& error);
    std::vector<LLVKFloater*> floaters() const;
    std::unique_ptr<LLVKWidgetFactory> mDialogFactory;
    LLVKWidgetTree mTree;
    LLVKWidgetTree::Id mRoot = 0;
    std::shared_ptr<LLVKFontRegistry> mFonts;
    std::shared_ptr<LLVKSkinFiles> mSkin;
    std::shared_ptr<LLVKColorTable> mColors;
    std::filesystem::path mUserColorsFile;
    std::string mUserColorsSnapshot;
    XmlFilePicker mExecutableFilePicker;
    std::uint64_t mPreferenceGeneration = 0;
    std::function<void()> mClearSpamQueues;
    std::unique_ptr<LLVKMenu> mMenu;
    std::unique_ptr<LLVKFloater> mPreferences, mAbout;
    std::unique_ptr<LLVKFloater> mKeyCapture;
    std::unique_ptr<LLVKFloater> mJoystick;
    std::unique_ptr<LLVKFloater> mProxy;
    std::unique_ptr<LLVKFloater> mDefaultPermissions;
    std::unique_ptr<LLVKFloater> mBeamColor;
    std::unique_ptr<LLVKFloater> mBeamShape;
    LLVKBeamShape mBeamShapeDraft;
    LLVKWidgetTree::Id mBeamShapeOwner=0,mBeamShapeCanvas=0;
    std::uint64_t mBeamShapeGeneration=0;
    bool updateBeamShapeImage(std::string& error);
    void beamShapeFile(bool save);
    std::unique_ptr<LLVKGraphicPresets> mGraphicPresets;
    std::map<std::string,std::unique_ptr<LLVKFloater>> mGraphicPresetDialogs;
    LLVKWidgetTree::Id mGraphicPresetOwner=0;
    bool mLoadingGraphicPreset=false;
    std::optional<LLVKGraphicsPolicy::Device> mGraphicsDevice;
    std::unique_ptr<LLVKGraphicsPolicy> mGraphicsPolicy;
    std::optional<std::map<std::string,LLSD>> graphicsPolicyValues(int level,bool recommended,std::string& error);
    bool refreshGraphicPresetDialogs(std::string& error);
    bool acceptGraphicPreset(const std::string& action,std::string& error);
    LLVKBeamColor mBeamColorDraft;
    LLVKWidgetTree::Id mBeamColorOwner=0, mBeamColorStrip=0;
    std::uint64_t mBeamColorGeneration=0;
    bool updateBeamColorStrip(std::string& error);
    bool updateBeamColorPreview(std::string& error);
    void beamColorFile(bool save);
    LLVKWidgetTree::PreferenceSnapshot mDefaultPermissionsSnapshot;
    bool mDefaultPermissionsAccepted = false;
    bool acceptDefaultPermissions(std::string& error);
    std::map<std::string,LLVKWidgetTree::Id> mProxyFields;
    LLVKWidgetTree::PreferenceSnapshot mProxySnapshot;
    bool mProxyAccepted = false;
    void updateProxyControls();
    bool acceptProxy(std::string& error);
    void networkPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action);
    DirectoryPicker mDirectoryPicker;
    std::function<bool(const std::filesystem::path&,std::string&)> mDirectoryOpener;
    std::filesystem::path mCacheDirectory, mDefaultCacheDirectory, mProfileDirectory;
    std::filesystem::path mSkinBaseDirectory;
    LLSD mSkinCatalog;
    struct SkinDraft { std::string skin, theme, skinName, themeName; bool edited = false; };
    std::map<LLVKWidgetTree::Id,SkinDraft> mSkinDrafts;
    bool refreshSkinPreferences(LLVKWidgetTree::Id panel,bool fromSettings,bool skinChanged,std::string& error);
    std::function<void()> mQuitRequest;
    bool filterPreferences(std::string& error);
    bool copyPreferenceSearch(std::string& error);
    bool initializeGraphicsPreferences(LLVKWidgetTree::Id panel,std::string& error);
    void graphicsPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action,const LLSD& value);
    std::function<bool(const std::string&,const LLSD&,std::string&)> mGraphicsPreferenceHandler;
    std::function<bool(const std::string&,std::string&)> mViewerPreferenceHandler;
    bool initializeViewerPreferences(LLVKWidgetTree::Id panel,std::string& error);
    bool refreshBeamPreferences(LLVKWidgetTree::Id panel,std::string& error);
    void viewerPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action);
    std::function<bool(const BackupRequest&,std::string&)> mBackupHandler;
    bool initializeBackupPreferences(LLVKWidgetTree::Id panel,std::string& error);
    void backupPreferenceAction(LLVKWidgetTree::Id panel,const std::string& action);
    std::unique_ptr<LLVKFloater> mMediaLists;
    std::unique_ptr<LLVKFloater> mBlockObjectName;
    std::unique_ptr<LLVKFloater> mBlockList;
    LLVKWidgetTree::Id mBlockObjectPanel = 0;
    bool showBlockObjectName(LLVKWidgetTree::Id panel, std::string& error);
    LLVKMediaFilter mMediaFilter;
    std::function<bool(const LLSD&,std::string&)> mSaveMediaFilterRules;
    bool refreshMediaLists(std::string& error);
    bool publishMediaRules(LLVKMediaFilter updated, std::string& error);
    JoystickServices mJoystickServices;
    std::map<std::string,LLVKWidgetTree::Id> mJoystickFields;
    LLVKWidgetTree::PreferenceSnapshot mJoystickSnapshot;
    bool updateJoystickPreview(std::string& error);
    bool restoreJoystick(std::string& error);
    void setJoystickDefaults();
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
    std::map<std::string,LLSD> mWarningSnapshot;
    std::map<std::string,LLSD> mSettingDefaults;
    std::map<std::string,LLSD> mAccountDefaults;
    std::function<bool(const std::string&,std::string&)> mUiSoundPlayer;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSavePreferences;
    std::set<std::string> mAccountSettingNames;
    bool mAccountSettingsLoaded = false;
    std::function<bool(const std::map<std::string,LLSD>&,std::string&)> mSaveAccountPreferences;
    std::function<std::optional<LLVKProxy::Credentials>(std::string&)> mLoadProxyCredentials;
    std::function<bool(const std::optional<LLVKProxy::Credentials>&,std::string&)> mSaveProxyCredentials;
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
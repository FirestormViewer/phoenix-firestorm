/**
 * @file llpreviewscript.h
 * @brief LLPreviewScript class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#pragma once

#include "llpreview.h"
#include "lltabcontainer.h"
#include "llinventory.h"
#include "llcombobox.h"
#include "lliconctrl.h"
#include "llframetimer.h"
#include "llfloatergotoline.h"
#include "lllivefile.h"
#include "llsyntaxid.h"
#include "llscripteditor.h"
#include <boost/signals2.hpp>

class LLLiveLSLFile;
class LLMessageSystem;
class LLScriptEditor;
class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;
class LLViewerObject;
struct  LLEntryAndEdCore;
class LLMenuBarGL;
//class LLFloaterScriptSearch;
class LLKeywordToken;
class LLViewerInventoryItem;
class LLScriptEdContainer;
class LLFloaterGotoLine;
class LLFloaterExperienceProfile;
class LLScriptMovedObserver;
class LLScriptEditorWSServer;
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0)
class LLEventTimer;
// [/SL:KB]
// NaCl - LSL Preprocessor
class FSLSLPreprocessor;
class FSLSLPreProcViewer;
// NaCl End

bool is_lua_script(const std::string& code);

class LLLiveLSLFile : public LLLiveFile
{
public:
    typedef std::function<bool(const std::string& filename)> change_callback_t;

    LLLiveLSLFile(std::string file_path, change_callback_t change_cb);
    ~LLLiveLSLFile() override;

    void ignoreNextUpdate() { mIgnoreNextUpdate = true; }

protected:
    bool loadFile() override;

    change_callback_t   mOnChangeCallback;
    bool                mIgnoreNextUpdate;
};

// Inner, implementation class.  LLPreviewScript and LLLiveLSLEditor each own one of these.
class LLScriptEdCore : public LLPanel
{
    friend class LLPreviewScript;
    friend class LLPreviewLSL;
    friend class LLLiveLSLEditor;
//  friend class LLFloaterScriptSearch;
    friend class LLScriptEdContainer;
    friend class LLFloaterGotoLine;
    // NaCl - LSL Preprocessor
    friend class FSLSLPreprocessor;
    // NaCl End

public:
    typedef boost::function<void(void*)> script_ed_callback_t;
    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //typedef boost::function<void(void*, bool)> save_callback_t;
    typedef boost::function<void(void*, bool, bool)> save_callback_t;

protected:
    // Supposed to be invoked only by the container.
    LLScriptEdCore(
        LLScriptEdContainer* container,
        const std::string& sample,
        const LLHandle<LLFloater>& floater_handle,
        script_ed_callback_t load_callback,
        save_callback_t save_callback,
        script_ed_callback_t search_replace_callback,
        void* userdata,
        bool live,
        S32 bottom_pad = 0);    // pad below bottom row of buttons
public:
    ~LLScriptEdCore() override;

    void            initMenu();
    void            processKeywords();
    void            processKeywords(bool luau_language);
    LLScriptEditor* getEditor() const { return mEditor; }
    LLKeywords&     getKeywords() const { return mEditor->getKeywords(); }
    bool            isLuauLanguage() const { return mEditor->getIsLuauLanguage(); }

    void            draw() override;
    bool            postBuild() override;
    bool            canClose();
    void            setEnableEditing(bool enable);
    bool            canLoadOrSaveToFile( void* userdata );

    void            setScriptText(const std::string& text, bool is_valid);
    // NaCL - LSL Preprocessor
    std::string     getScriptText();
    void            doSaveComplete(void* userdata, bool close_after_save, bool sync);
    // NaCl End
    void            makeEditorPristine();
    bool            loadScriptText(const std::string& filename);
    bool            writeToFile(const std::string& filename, bool unprocessed);
    void            sync();

    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //void          doSave( bool close_after_save );
    void            doSave(bool close_after_save, bool sync = true);
    // </FS:Ansariel>

    bool            handleSaveChangesDialog(const LLSD& notification, const LLSD& response);
    bool            handleReloadFromServerDialog(const LLSD& notification, const LLSD& response);

    void            openInExternalEditor();

    static void     onCheckLock(LLUICtrl*, void*);
    static void     onHelpComboCommit(LLUICtrl* ctrl, void* userdata);
    static void     onClickBack(void* userdata);
    static void     onClickForward(void* userdata);
    static void     onBtnInsertSample(void*);
    static void     onBtnInsertFunction(LLUICtrl*, void*);
    static void     onBtnLoadFromFile(void*);
    static void     onBtnSaveToFile(void*);
    static void     onBtnPrefs(void*);  // <FS:CR> Advanced Script Editor

    static void     loadScriptFromFile(const std::vector<std::string>& filenames, void* data);
    static void     saveScriptToFile(const std::vector<std::string>& filenames, void* data);

    static bool     enableSaveToFileMenu(void* userdata);
    static bool     enableLoadFromFileMenu(void* userdata);

    bool            hasAccelerators() const override { return true; }
    LLUUID          getAssociatedExperience()const;
    void            setAssociatedExperience( const LLUUID& experience_id );

    void            setScriptName(const std::string& name) { mScriptName = name; }

    void            setItemRemoved(bool script_removed) { mScriptRemoved = script_removed; }

    void            setAssetID( const LLUUID& asset_id){ mAssetID = asset_id; };
    LLUUID          getAssetID() const { return mAssetID; }

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //bool            isFontSizeChecked(const LLSD &userdata);
    //void            onChangeFontSize(const LLSD &size_name);
    // </FS:Ansarie>

    bool            handleKeyHere(KEY key, MASK mask) override;
    void            selectAll() { mEditor->selectAll(); }

    void            enableSave(bool b) { mEnableSave = b; }
    bool            hasChanged() const;

    void            updateTabs(bool is_lua);      // <FS:Zi> Disable LSL preprocessor when compile target is luau

  private:
    // NaCl - LSL Preprocessor
    void        onToggleProc();
    boost::signals2::connection mTogglePreprocConnection;

    void        onPreprocTabChanged(const std::string& tab_name);
    void        performAction(const std::string& action);
    bool        enableAction(const std::string& action);
    // NaCl End
    void        onBtnHelp(); // <FS:Ansariel> Keep help links
public: // <FS:Ansariel> Show keyword help on F1
    void        onBtnDynamicHelp();
private: // <FS:Ansariel> Show keyword help on F1
    void        onBtnUndoChanges();

    void selectFirstError();

// <FS:CR> Advanced Script Editor
    void    initButtonBar();
    void    updateButtonBar();
// </FS:CR>

    void    updateIndicators(bool compiling, bool success); // <FS:Kadah> Compile indicators

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    boost::signals2::connection mFontNameChangedCallbackConnection;
    boost::signals2::connection mFontSizeChangedCallbackConnection;
    void    onFontChanged();
    // </FS:Ansariel>

protected:
    void deleteBridges();
    void setHelpPage(const std::string& help_string);
    void updateDynamicHelp(bool immediate = false);
    bool isKeyword(LLKeywordToken* token);
    void addHelpItemToHistory(const std::string& help_string);
    static void onErrorList(LLUICtrl*, void* user_data);

    bool            mLive;
    bool            mCompiling; // <FS:Kadah> Compile indicators

private:
    std::string     mSampleText;
    std::string     mScriptName;
    LLScriptEditor* mEditor;
    script_ed_callback_t mLoadCallback;
    save_callback_t      mSaveCallback;
    script_ed_callback_t mSearchReplaceCallback;
    void*           mUserdata;
    LLComboBox      *mFunctions;
    bool            mForceClose;
    LLPanel*        mCodePanel;
    LLScrollListCtrl* mErrorList;
    std::vector<LLEntryAndEdCore*> mBridges;
    LLHandle<LLFloater> mLiveHelpHandle;
    LLKeywordToken* mLastHelpToken;
    LLFrameTimer    mLiveHelpTimer;
    S32             mLiveHelpHistorySize;
    bool            mEnableSave;
    bool            mHasScriptData;
    LLUUID          mAssociatedExperience;
    bool            mScriptRemoved;
    bool            mSaveDialogShown;
    LLUUID          mAssetID;
    LLTextBox*      mLineCol = nullptr;
    LLButton*       mSaveBtn = nullptr;
    LLComboBox*     mCompileTarget = nullptr;

// <FS:CR> Advanced Script Editor
    LLButton*       mSaveBtn2;  //  // <FS:Zi> support extra save button
    LLButton*       mCutBtn;
    LLButton*       mCopyBtn;
    LLButton*       mPasteBtn;
    LLButton*       mUndoBtn;
    LLButton*       mRedoBtn;
    LLButton*       mSaveToDiskBtn;
    LLButton*       mLoadFromDiskBtn;
    LLButton*       mSearchBtn;
// </FS:CR>
    // NaCl - LSL Preprocessor
    FSLSLPreprocessor*  mLSLProc;
    FSLSLPreProcViewer* mPostEditor;
    LLScriptEditor*     mCurrentEditor;
    LLTabContainer*     mPreprocTab;
    std::string         mPostScript;
    // NaCl End

    LLScriptEdContainer* mContainer; // parent view

 public:
    boost::signals2::connection mSyntaxIDConnection;
};

class LLScriptEdContainer : public LLPreview
{
    friend class LLScriptEdCore;

public:
    LLScriptEdContainer(const LLSD& key);
    ~LLScriptEdContainer() override;
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    void refreshFromItem() override;
// [/SL:KB]

    // <FS:Ansariel> FIRE-16740: Color syntax highlighting changes don't immediately appear in script window
    void updateStyle();

    bool handleKeyHere(KEY key, MASK mask) override;

    void startWebsocketServer();
    void unsubscribeScript();
    void sendCompileResults(LLSD&);

    LLScriptEdCore* getScriptEdCore() const { return mScriptEd; }

protected:
    std::string     getTmpFileName(const std::string& script_name) const;
    std::string     getUniqueHash() const;
    std::string     getErrorLogFileName(const std::string& script_path);
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    virtual std::string getBackupFileName() const;
    bool            onBackupTimer();
// [/SL:KB]

    bool            onExternalChange(const std::string& filename);
    virtual void    saveIfNeeded(bool sync = true) = 0;
    bool            logErrorsToFile(const LLSD& compile_errors);
    bool            isOpenInExternalEditor() const { return mLiveFile != nullptr; }

    LLScriptEdCore*     mScriptEd;
    LLLiveLSLFile*      mLiveFile = nullptr;
    LLLiveLSLFile*      mLiveLogFile = nullptr;

    std::weak_ptr<LLScriptEditorWSServer> mWebSocketServer;

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    std::string         mBackupFilename;
    LLEventTimer*       mBackupTimer;
// [/SL:KB]
};

// Used to view and edit an LSL script from your inventory.
class LLPreviewLSL : public LLScriptEdContainer
{
public:
    LLPreviewLSL(const LLSD& key );
    ~LLPreviewLSL() override;

    LLUUID getScriptID() { return mItemUUID; }

    void setDirty() { mDirty = true; }

    virtual void callbackLSLCompileSucceeded();
    virtual void callbackLSLCompileFailed(const LLSD& compile_errors);

    bool postBuild() override;

// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
    LLScriptEditor* getEditor() { return (mScriptEd) ? mScriptEd->mEditor : NULL; }
// [/SL:KB]

protected:
    void draw() override;
    bool canClose() override;
    void closeIfNeeded();

    void loadAsset() override;
    void saveIfNeeded(bool sync = true) override;
    void onCompileTargetChanged();

    static void onSearchReplace(void* userdata);
    static void onLoad(void* userdata);
    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //static void onSave(void* userdata, bool close_after_save);
    static void onSave(void* userdata, bool close_after_save, bool sync);
    // </FS:Ansariel>

    static void onLoadComplete(const LLUUID& uuid,
                               LLAssetType::EType type,
                               void* user_data, S32 status, LLExtStat ext_status);

protected:
    static void* createScriptEdPanel(void* userdata);

    static void finishedLSLUpload(LLUUID itemId, LLSD response);
    static bool failedLSLUpload(LLUUID itemId, LLUUID taskId, LLSD response, std::string reason);
protected:

    // Can safely close only after both text and bytecode are uploaded
    S32 mPendingUploads;

    LLScriptMovedObserver* mItemObserver;
};

// Used to view and edit an LSL script that is attached to an object.
class LLLiveLSLEditor : public LLScriptEdContainer
{
    friend class LLLiveLSLFile;
public:
    LLLiveLSLEditor(const LLSD& key);

    static void processScriptRunningReply(LLMessageSystem* msg, void**);

    virtual void callbackLSLCompileSucceeded(const LLUUID& task_id,
                                            const LLUUID& item_id,
                                            bool is_script_running);
    virtual void callbackLSLCompileFailed(const LLSD& compile_errors);

    bool postBuild() override;

    void setIsNew() { mIsNew = true; }

// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
    LLScriptEditor* getEditor() { return (mScriptEd) ? mScriptEd->mEditor : NULL; }
// [/SL:KB]

    static void setAssociatedExperience( LLHandle<LLLiveLSLEditor> editor, const LLSD& experience );
    void onToggleExperience();
    void onViewProfile();

    void setExperienceIds(const LLSD& experience_ids);
    void buildExperienceList();
    void updateExperiencePanel();
    void requestExperiences();
    void experienceChanged();

    void setObjectName(std::string name) { mObjectName = name; }

    bool getIsModifiable() const { return mIsModifiable; } // Evaluated on load assert

private:
    bool canClose() override;
    void closeIfNeeded();
    void draw() override;

    void loadAsset() override;
    void saveIfNeeded(bool sync = true) override;

    static void onSearchReplace(void* userdata);
    static void onLoad(void* userdata);
    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //static void onSave(void* userdata, bool close_after_save);
    static void onSave(void* userdata, bool close_after_save, bool sync);
    // </FS:Ansariel>

    static void onLoadComplete(const LLUUID& asset_uuid,
                               LLAssetType::EType type,
                               void* user_data, S32 status, LLExtStat ext_status);
    void onRunningCheckboxClicked();
    void onReset();

    void loadScriptText(const LLUUID &uuid, LLAssetType::EType type);

    static void* createScriptEdPanel(void* userdata);

    void onCompileTargetChanged();

    static void finishLSLUpload(LLUUID itemId, LLUUID taskId, LLUUID newAssetId, LLSD response, bool isRunning);
    static void receiveExperienceIds(LLSD result, LLHandle<LLLiveLSLEditor> parent);

private:
    bool                mIsNew;
    //LLUUID mTransmitID;
    //LLCheckBoxCtrl*       mRunningCheckbox;
    bool                mAskedForRunningInfo;
    bool                mHaveRunningInfo;
    //LLButton*         mResetButton;
    LLPointer<LLViewerInventoryItem> mItem;
    bool                mCloseAfterSave;
    // need to save both text and script, so need to decide when done
    S32                 mPendingUploads;

    bool                mIsSaving;
    bool                mIsModifiable;

    LLButton*           mResetButton       { nullptr };
    LLCheckBoxCtrl*     mRunningCheckbox   { nullptr };
    LLComboBox*         mExperiences       { nullptr };
    LLCheckBoxCtrl*     mExperienceEnabled { nullptr };
    LLButton*           mViewProfileButton { nullptr };

    LLSD                mExperienceIds;
    LLHandle<LLFloater> mExperienceProfile;
    std::string         mObjectName;
};


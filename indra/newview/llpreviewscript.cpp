/**
 * @file llpreviewscript.cpp
 * @brief LLPreviewScript class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llpreviewscript.h"

#include "llassetstorage.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llexternaleditor.h"
#include "llfilepicker.h"
#include "llfloaterreg.h"
#include "llfloatersearchreplace.h"
#include "llinventorydefines.h"
#include "llinventorymodel.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "llmd5.h"
#include "llhelp.h"
#include "llnotificationsutil.h"
#include "llresmgr.h"
#include "llscrollbar.h"
#include "llscrollcontainer.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llscrolllistcell.h"
#include "llsdserialize.h"
#include "llslider.h"
#include "lltooldraganddrop.h"
#include "llfilesystem.h"
#include "lllogchat.h"

#include "llagent.h"
#include "llmenugl.h"
#include "roles_constants.h"
#include "llselectmgr.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewermenufile.h" // LLFilePickerReplyThread
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llscripteditor.h"
#include "lltextbox.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "lluictrlfactory.h"
#include "llmediactrl.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llappviewer.h"
#include "llfloatergotoline.h"
#include "llexperiencecache.h"
#include "llfloaterexperienceprofile.h"
#include "llviewerassetupload.h"
//#include "lltoggleablemenu.h" // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
//#include "llmenubutton.h" // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
#include "llinventoryfunctions.h"
#include "llwebsocketmgr.h"
#include "llscripteditorws.h"
#include <regex>
#include "llloadingindicator.h" // <FS:Kadah> Compile indicator
#include "lliconctrl.h" // <FS:Kadah> Compile indicator
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]
#include "fsscriptlibrary.h"

// NaCl - LSL Preprocessor
#include "fslslpreproc.h"
#include "fslslpreprocviewer.h"
// NaCl End
#ifdef OPENSIM
#include "llviewernetwork.h"    // for Grid manager
#endif // OPENSIM

const std::string HELP_LSL_PORTAL_TOPIC = "LSL_Portal";

const std::string DEFAULT_SCRIPT_NAME = "New Script"; // *TODO:Translate?
const std::string DEFAULT_SCRIPT_DESC = "(No Description)"; // *TODO:Translate?

// Description and header information
const S32 MAX_HISTORY_COUNT = 10;
const F32 LIVE_HELP_REFRESH_TIME = 1.f;

static bool have_script_upload_cap(LLUUID& object_id)
{
    LLViewerObject* object = gObjectList.findObject(object_id);
    return object && (! object->getRegion()->getCapability("UpdateScriptTask").empty());
}

static bool have_lua_enabled(const LLUUID& object_id)
{
    LLViewerRegion* region = nullptr;
    LLViewerObject* object = gObjectList.findObject(object_id);
    if (object)
    {
        region = object->getRegion();
    }
    else
    {
        region = gAgent.getRegion();
    }

    if (region && region->simulatorFeaturesReceived())
    {
        LLSD simulatorFeatures;
        region->getSimulatorFeatures(simulatorFeatures);
        return simulatorFeatures["LuaScriptsEnabled"].asBoolean();
    }

    return false;
}

// TEMPORARY: Quick check to see if the code is Lua
// since we don't have another way to determine the language yet
bool is_lua_script(const std::string& code)
{
    // Check for LSL's signature "default" state pattern
    std::regex lsl_pattern("\\s*default\\s*\\{");
    if (std::regex_search(code, lsl_pattern))
        return false;

    // "default" state not found, assuming it's Lua
    return true;
}

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
#include "lleventtimer.h"

/// ---------------------------------------------------------------------------
/// Timer helper class
/// ---------------------------------------------------------------------------
class LLCallbackTimer : public LLEventTimer
{
public:
    typedef std::function<bool()> bool_func_t;
public:
    LLCallbackTimer(F32 nPeriod, bool_func_t cb) : LLEventTimer(nPeriod), m_Callback(cb) {}
    /*virtual*/ bool tick() { return m_Callback(); }
protected:
    bool_func_t m_Callback;
};

inline LLEventTimer* setupCallbackTimer(F32 nPeriod, LLCallbackTimer::bool_func_t cb)
{
    return new LLCallbackTimer(nPeriod, cb);
}
// [/SL:KB]

/// ---------------------------------------------------------------------------
/// LLLiveLSLFile
/// ---------------------------------------------------------------------------

LLLiveLSLFile::LLLiveLSLFile(std::string file_path, change_callback_t change_cb)
:   mOnChangeCallback(change_cb)
,   mIgnoreNextUpdate(false)
,   LLLiveFile(file_path, 1.0f)
{
    llassert(mOnChangeCallback);
}

LLLiveLSLFile::~LLLiveLSLFile()
{
    LLFile::remove(filename());
}

bool LLLiveLSLFile::loadFile()
{
    if (mIgnoreNextUpdate)
    {
        mIgnoreNextUpdate = false;
        return true;
    }

    return mOnChangeCallback(filename());
}

/// ---------------------------------------------------------------------------
/// LLFloaterScriptSearch
/// ---------------------------------------------------------------------------
// <FS> Replaced by LLFloaterSearchReplace
#if 0
class LLFloaterScriptSearch : public LLFloater
{
public:
    LLFloaterScriptSearch(LLScriptEdCore* editor_core);
    ~LLFloaterScriptSearch();

    /*virtual*/ bool    postBuild();
    static void show(LLScriptEdCore* editor_core);
    static void onBtnSearch(void* userdata);
    void handleBtnSearch();

    static void onBtnReplace(void* userdata);
    void handleBtnReplace();

    static void onBtnReplaceAll(void* userdata);
    void handleBtnReplaceAll();

    LLScriptEdCore* getEditorCore() { return mEditorCore; }
    static LLFloaterScriptSearch* getInstance() { return sInstance; }

    virtual bool hasAccelerators() const;
    virtual bool handleKeyHere(KEY key, MASK mask);

private:

    LLScriptEdCore* mEditorCore;
    static LLFloaterScriptSearch*   sInstance;

protected:
    LLLineEditor*           mSearchBox;
    LLLineEditor*           mReplaceBox;
        void onSearchBoxCommit();
};

LLFloaterScriptSearch* LLFloaterScriptSearch::sInstance = NULL;

LLFloaterScriptSearch::LLFloaterScriptSearch(LLScriptEdCore* editor_core)
:   LLFloater(LLSD()),
    mSearchBox(NULL),
    mReplaceBox(NULL),
    mEditorCore(editor_core)
{
    buildFromFile("floater_script_search.xml");

    sInstance = this;

    // find floater in which script panel is embedded
    LLView* viewp = (LLView*)editor_core;
    while(viewp)
    {
        LLFloater* floaterp = dynamic_cast<LLFloater*>(viewp);
        if (floaterp)
        {
            floaterp->addDependentFloater(this);
            break;
        }
        viewp = viewp->getParent();
    }
}

bool LLFloaterScriptSearch::postBuild()
{
    mReplaceBox = getChild<LLLineEditor>("replace_text");
    mSearchBox = getChild<LLLineEditor>("search_text");
    mSearchBox->setCommitCallback(boost::bind(&LLFloaterScriptSearch::onSearchBoxCommit, this));
    mSearchBox->setCommitOnFocusLost(false);
    childSetAction("search_btn", onBtnSearch,this);
    childSetAction("replace_btn", onBtnReplace,this);
    childSetAction("replace_all_btn", onBtnReplaceAll,this);

    setDefaultBtn("search_btn");

    return true;
}

//static
void LLFloaterScriptSearch::show(LLScriptEdCore* editor_core)
{
    LLSD::String search_text;
    LLSD::String replace_text;
    if (sInstance && sInstance->mEditorCore && sInstance->mEditorCore != editor_core)
    {
        search_text=sInstance->mSearchBox->getValue().asString();
        replace_text=sInstance->mReplaceBox->getValue().asString();
        sInstance->closeFloater();
        delete sInstance;
    }

    if (!sInstance)
    {
        // sInstance will be assigned in the constructor.
        new LLFloaterScriptSearch(editor_core);
        sInstance->mSearchBox->setValue(search_text);
        sInstance->mReplaceBox->setValue(replace_text);
    }

    sInstance->openFloater();
}

LLFloaterScriptSearch::~LLFloaterScriptSearch()
{
    sInstance = NULL;
}

// static
void LLFloaterScriptSearch::onBtnSearch(void *userdata)
{
    LLFloaterScriptSearch* self = (LLFloaterScriptSearch*)userdata;
    self->handleBtnSearch();
}

void LLFloaterScriptSearch::handleBtnSearch()
{
    LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
    mEditorCore->mEditor->selectNext(mSearchBox->getValue().asString(), caseChk->get());
}

// static
void LLFloaterScriptSearch::onBtnReplace(void *userdata)
{
    LLFloaterScriptSearch* self = (LLFloaterScriptSearch*)userdata;
    self->handleBtnReplace();
}

void LLFloaterScriptSearch::handleBtnReplace()
{
    LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
    mEditorCore->mEditor->replaceText(mSearchBox->getValue().asString(), mReplaceBox->getValue().asString(), caseChk->get());
}

// static
void LLFloaterScriptSearch::onBtnReplaceAll(void *userdata)
{
    LLFloaterScriptSearch* self = (LLFloaterScriptSearch*)userdata;
    self->handleBtnReplaceAll();
}

void LLFloaterScriptSearch::handleBtnReplaceAll()
{
    LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
    mEditorCore->mEditor->replaceTextAll(mSearchBox->getValue().asString(), mReplaceBox->getValue().asString(), caseChk->get());
}

bool LLFloaterScriptSearch::hasAccelerators() const
{
    if (mEditorCore)
    {
        return mEditorCore->hasAccelerators();
    }
    return false;
}

bool LLFloaterScriptSearch::handleKeyHere(KEY key, MASK mask)
{
    if (mEditorCore)
    {
        bool handled = mEditorCore->handleKeyHere(key, mask);
        if (!handled)
        {
            LLFloater::handleKeyHere(key, mask);
        }
    }

    return false;
}

void LLFloaterScriptSearch::onSearchBoxCommit()
{
    if (mEditorCore && mEditorCore->mEditor)
    {
        LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
        mEditorCore->mEditor->selectNext(mSearchBox->getValue().asString(), caseChk->get());
    }
}
#endif
// </FS>

/// ---------------------------------------------------------------------------

class LLScriptMovedObserver : public LLInventoryObserver
{
  public:
    LLScriptMovedObserver(LLPreviewLSL *floater) : mPreview(floater) { gInventory.addObserver(this); }
    virtual ~LLScriptMovedObserver() { gInventory.removeObserver(this); }
    virtual void changed(U32 mask);

  private:
    LLPreviewLSL *mPreview;
};

void LLScriptMovedObserver::changed(U32 mask)
{
    const std::set<LLUUID> &mChangedItemIDs = gInventory.getChangedIDs();
    std::set<LLUUID>::const_iterator it;

    const LLUUID &item_id = mPreview->getScriptID();

    for (it = mChangedItemIDs.begin(); it != mChangedItemIDs.end(); it++)
    {
        if (*it == item_id)
        {
            if ((mask & (LLInventoryObserver::STRUCTURE)) != 0)
            {
                mPreview->setDirty();
            }
        }
    }
}

/// ---------------------------------------------------------------------------
/// LLScriptEdCore
/// ---------------------------------------------------------------------------

struct LLSECKeywordCompare
{
    bool operator()(const std::string& lhs, const std::string& rhs)
    {
        return (LLStringUtil::compareDictInsensitive( lhs, rhs ) < 0 );
    }
};

LLScriptEdCore::LLScriptEdCore(
    LLScriptEdContainer* container,
    const std::string& sample,
    const LLHandle<LLFloater>& floater_handle,
    script_ed_callback_t load_callback,
    save_callback_t save_callback,
    script_ed_callback_t search_replace_callback,
    void* userdata,
    bool live,
    S32 bottom_pad)
    :
    LLPanel(),
    mSampleText(sample),
    mEditor( NULL ),
    mLoadCallback( load_callback ),
    mSaveCallback( save_callback ),
    mSearchReplaceCallback( search_replace_callback ),
    mUserdata( userdata ),
    mForceClose( false ),
    mLastHelpToken(NULL),
    mLiveHelpHistorySize(0),
    mEnableSave(false),
    mLive(live),
    mContainer(container),
    // <FS:CR> FIRE-10606, patch by Sei Lisa
    mLSLProc(NULL),
    mPostEditor(NULL),
    mCurrentEditor(NULL),
    mPreprocTab(NULL),
    // </FS:CR>
    mCompiling(false), //<FS:KC> Compile indicators, recompile button
    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    mFontNameChangedCallbackConnection(),
    mFontSizeChangedCallbackConnection(),
    // </FS:Ansariel>
    mHasScriptData(false),
    mScriptRemoved(false),
    mSaveDialogShown(false)
{
    setFollowsAll();
    setBorderVisible(false);

    // NaCl - Script Preprocessor
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor"))
    {
        setXMLFilename("panel_script_ed_preproc.xml");
        mLSLProc = new FSLSLPreprocessor(this);
    }
    else
    {
        setXMLFilename("panel_script_ed.xml");
    }
    // NaCl End
    llassert_always(mContainer != NULL);
}

LLScriptEdCore::~LLScriptEdCore()
{
    deleteBridges();

    // If the search window is up for this editor, close it.
//    LLFloaterScriptSearch* script_search = LLFloaterScriptSearch::getInstance();
//    if (script_search && script_search->getEditorCore() == this)
//    {
//        script_search->closeFloater();
//        // closeFloater can delete instance since it's not reusable nor single instance
//        // so make sure instance is still there before deleting
//        script_search = LLFloaterScriptSearch::getInstance();
//        if (script_search)
//        {
//            delete script_search;
//        }
//    }
    // <FS:Sei> FIRE-16042: Warn when preproc is toggled.
    if (mTogglePreprocConnection.connected())
    {
        mTogglePreprocConnection.disconnect();
    }
    // </FS:Sei>

    if (mSyntaxIDConnection.connected())
    {
        mSyntaxIDConnection.disconnect();
    }

    // NaCl - Script Preprocessor
    delete mLSLProc;

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    if (mFontNameChangedCallbackConnection.connected())
    {
        mFontNameChangedCallbackConnection.disconnect();
    }
    if (mFontSizeChangedCallbackConnection.connected())
    {
        mFontSizeChangedCallbackConnection.disconnect();
    }
    // </FS:Ansariel>
}

// <FS:Zi> Disable LSL preprocessor when compile target is luau
void LLScriptEdCore::updateTabs(bool is_lua)
{
    if (!mPreprocTab)
    {
        return;
    }

    mPreprocTab->selectTabByName("Script");
    mPreprocTab->setTabVisibility(mPreprocTab->getPanelByIndex(mPreprocTab->getPanelIndexByTitle("Preprocessed")), !is_lua);
}
// </FS:Zi>

void LLLiveLSLEditor::experienceChanged()
{
    if(mScriptEd->getAssociatedExperience() != mExperiences->getSelectedValue().asUUID())
    {
        mScriptEd->enableSave(getIsModifiable());
        //mSaveBtn->setEnabled(true);
        mScriptEd->setAssociatedExperience(mExperiences->getSelectedValue().asUUID());
        updateExperiencePanel();
    }
}

void LLLiveLSLEditor::onViewProfile()
{
    if (mExperienceEnabled->get())
    {
        LLUUID id = mScriptEd->getAssociatedExperience();
        if (id.notNull())
        {
             LLFloaterReg::showInstance("experience_profile", id, true);
        }
    }
}

void LLLiveLSLEditor::onToggleExperience()
{
    LLUUID id;
    if (mExperienceEnabled->get())
    {
        if (mScriptEd->getAssociatedExperience().isNull())
        {
            id = mExperienceIds.beginArray()->asUUID();
        }
    }

    if (id != mScriptEd->getAssociatedExperience())
    {
        mScriptEd->enableSave(getIsModifiable());
    }

    mScriptEd->setAssociatedExperience(id);

    updateExperiencePanel();
}

bool LLScriptEdCore::postBuild()
{
    mLineCol = getChild<LLTextBox>("line_col");
// <FS:CR> Advanced Script Editor
    mSaveBtn =  getChild<LLButton>("save_btn");
    mSaveBtn2 = getChild<LLButton>("save_btn_2");   // <FS:Zi> support extra save button
    mCutBtn =   getChild<LLButton>("cut_btn");
    mCopyBtn =  getChild<LLButton>("copy_btn");
    mPasteBtn = getChild<LLButton>("paste_btn");
    mUndoBtn =  getChild<LLButton>("undo_btn");
    mRedoBtn =  getChild<LLButton>("redo_btn");
    mSaveToDiskBtn = getChild<LLButton>("save_disk_btn");
    mLoadFromDiskBtn = getChild<LLButton>("load_disk_btn");
    mSearchBtn = getChild<LLButton>("search_btn");
// </FS:CR>

    mErrorList = getChild<LLScrollListCtrl>("lsl errors");

    mFunctions = getChild<LLComboBox>("Insert...");

    childSetCommitCallback("Insert...", &LLScriptEdCore::onBtnInsertFunction, this);

    mEditor = getChild<LLScriptEditor>("Script Editor");

    // NaCl - LSL Preprocessor
    mCurrentEditor = mEditor;
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor"))
    {
        mPostEditor = getChild<FSLSLPreProcViewer>("Post Editor");
        mPostEditor->setFollowsAll();
        mPostEditor->setEnabled(true);

        mPreprocTab = getChild<LLTabContainer>("Tabset");
        mPreprocTab->setCommitCallback(boost::bind(&LLScriptEdCore::onPreprocTabChanged, this, _2));
    }
    // FIRE-16042: Warn when preproc is toggled.
    mTogglePreprocConnection = gSavedSettings.getControl("_NACL_LSLPreprocessor")->getSignal()->connect(boost::bind(&LLScriptEdCore::onToggleProc, this));
    // NaCl End

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    mFontNameChangedCallbackConnection = gSavedSettings.getControl("FSScriptingFontName")->getSignal()->connect(boost::bind(&LLScriptEdCore::onFontChanged, this));
    mFontSizeChangedCallbackConnection = gSavedSettings.getControl("FSScriptingFontSize")->getSignal()->connect(boost::bind(&LLScriptEdCore::onFontChanged, this));
    onFontChanged();
    // </FS:Ansariel>

    childSetCommitCallback("lsl errors", &LLScriptEdCore::onErrorList, this);
// <FS:CR> Advanced Script Editor
    //mSaveBtn = getChild<LLButton>("Save_btn");
    //mSaveBtn->setCommitCallback(boost::bind(&LLScriptEdCore::doSave, this, false));
    childSetAction("prefs_btn", boost::bind(&LLScriptEdCore::onBtnPrefs, this));
// </FS:CR>
    childSetAction("Edit_btn", boost::bind(&LLScriptEdCore::openInExternalEditor, this));
    childSetAction("edit_btn_2", boost::bind(&LLScriptEdCore::openInExternalEditor, this)); // <FS:Zi> support extra edit button

    initMenu();
    initButtonBar();    // <FS:CR> Advanced Script Editor

    mSyntaxIDConnection = LLSyntaxDefCache::getInstance()->addSyntaxIDCallback(boost::bind(&LLScriptEdCore::processKeywords, this));

    // Intialise keyword highlighting for the current simulator's version of LSL
    LLSyntaxDefCache::getInstance();
    processKeywords();

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //mCommitCallbackRegistrar.add("FontSize.Set", boost::bind(&LLScriptEdCore::onChangeFontSize, this, _2));
    //mEnableCallbackRegistrar.add("FontSize.Check", boost::bind(&LLScriptEdCore::isFontSizeChecked, this, _2));

    //LLToggleableMenu *context_menu = LLUICtrlFactory::getInstance()->createFromFile<LLToggleableMenu>(
    //    "menu_lsl_font_size.xml", gMenuHolder, LLViewerMenuHolderGL::child_registry_t::instance());
    //getChild<LLMenuButton>("font_btn")->setMenu(context_menu, LLMenuButton::MP_BOTTOM_LEFT, true);
    // </FS:Ansariel>

    bool lua_scripts_enabled = have_lua_enabled(LLUUID::null);
    mCompileTarget = getChild<LLComboBox>("compile_target");
    if (LLScrollListItem* luau_item = mCompileTarget->findItemByValue("luau"))
    {
        luau_item->setEnabled(lua_scripts_enabled);
    }
    if (LLScrollListItem* lsl_luau_item = mCompileTarget->findItemByValue("lsl-luau"))
    {
        lsl_luau_item->setEnabled(lua_scripts_enabled);
    }

    return true;
}

void LLScriptEdCore::processKeywords()
{
    processKeywords(mEditor->getIsLuauLanguage());
}

void LLScriptEdCore::processKeywords(bool luau_language)
{
    LL_DEBUGS("SyntaxLSL") << "Processing keywords" << LL_ENDL;
    // <FS:Ansariel> FIRE-15906: Clear combobox values before adding new
    mFunctions->clearRows();
    mEditor->clearSegments();
    mEditor->initKeywords(luau_language);
    mEditor->loadKeywords();

    // <FS:Ansariel> Re-add legacy format support
    std::vector<std::string> funcs;
    std::vector<std::string> tooltips;
    for (std::vector<LLScriptLibraryFunction>::const_iterator i = gScriptLibrary.mFunctions.begin();
    i != gScriptLibrary.mFunctions.end(); ++i)
    {
        // Make sure this isn't a god only function, or the agent is a god.
        if (!i->mGodOnly || gAgent.isGodlike())
        {
            std::string name = i->mName;
            funcs.push_back(name);

            std::string desc = i->mDesc;

            F32 sleep_time = i->mSleepTime;
            if( sleep_time )
            {
                desc += "\n";

                LLStringUtil::format_map_t args;
                args["[SLEEP_TIME]"] = llformat("%.1f", sleep_time );
                desc += LLTrans::getString("LSLTipSleepTime", args);
            }

            // A \n linefeed is not part of xml. Let's add one to keep all
            // the tips one-per-line in strings.xml
            LLStringUtil::replaceString( desc, ";", "\n" );

            LL_DEBUGS() << "Adding script library function: (" << name << ") with the desc '" << desc << "'" << LL_ENDL;

            tooltips.push_back(desc);
        }
    }

    LLColor3 color(LLUIColorTable::instance().getColor("SyntaxLslFunction"));
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor"))
    {
        mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_preproc.xml"), funcs, tooltips, color);
    }

#ifdef OPENSIM
    if (LLGridManager::getInstance()->isInOpenSim())
    {
        std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "scriptlibrary_ossl.xml");
        if (LLFile::isfile(filename))
        {
            mEditor->loadKeywords(filename, funcs, tooltips, color);
        }
        else
        {
            mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_ossl.xml"), funcs, tooltips, color);
        }
    }

    if (LLGridManager::getInstance()->isInAuroraSim())
    {
        std::string filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, "scriptlibrary_aa.xml");
        if (LLFile::isfile(filename))
        {
            mEditor->loadKeywords(filename, funcs, tooltips, color);
        }
        else
        {
            mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_aa.xml"), funcs, tooltips, color);
        }
    }
#endif // OPENSIM
    // </FS:Ansariel>

    string_vec_t primary_keywords;
    string_vec_t secondary_keywords;
    LLKeywordToken *token;
    LLKeywords::keyword_iterator_t token_it;
    for (token_it = mEditor->keywordsBegin(); token_it != mEditor->keywordsEnd(); ++token_it)
    {
        token = token_it->second;
        if (token->getType() == LLKeywordToken::TT_FUNCTION)
        {
            primary_keywords.push_back( wstring_to_utf8str(token->getToken()) );
        }
        else
        {
            secondary_keywords.push_back( wstring_to_utf8str(token->getToken()) );
        }
    }

    mFunctions->removeall();
    for (string_vec_t::const_iterator iter = primary_keywords.begin();
         iter!= primary_keywords.end(); ++iter)
    {
        mFunctions->add(*iter);
    }
    for (string_vec_t::const_iterator iter = secondary_keywords.begin();
         iter!= secondary_keywords.end(); ++iter)
    {
        mFunctions->add(*iter);
    }

    // NaCl - LSL Preprocessor
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mPostEditor)
    {
        mPostEditor->clearSegments();
        mPostEditor->initKeywords();
        mPostEditor->loadKeywords();

        mPostEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_preproc.xml"), funcs, tooltips, color);
#ifdef OPENSIM
        if (LLGridManager::getInstance()->isInOpenSim())
            mPostEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_ossl.xml"), funcs, tooltips, color);
        if (LLGridManager::getInstance()->isInAuroraSim())
            mPostEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_aa.xml"), funcs, tooltips, color);
#endif // OPENSIM
    }
    // NaCl End
}

void LLScriptEdCore::initMenu()
{
    // *TODO: Skinning - make these callbacks data driven
    LLMenuItemCallGL* menuItem;

    menuItem = getChild<LLMenuItemCallGL>("Save");
    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //menuItem->setClickCallback(boost::bind(&LLScriptEdCore::doSave, this, false));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::doSave, this, false, true));
    // </FS:Ansariel>
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::hasChanged, this));

    menuItem = getChild<LLMenuItemCallGL>("Revert All Changes");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnUndoChanges, this));
    //menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::hasChanged, this));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Revert All Changes"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Revert All Changes"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Undo");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::undo, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canUndo, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Undo"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Undo"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Redo");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::redo, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canRedo, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Redo"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Redo"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Cut");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::cut, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canCut, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Cut"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Cut"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Copy");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::copy, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canCopy, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Copy"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Copy"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Paste");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::paste, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canPaste, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Paste"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Paste"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Select All");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::selectAll, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canSelectAll, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Select All"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Select All"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Deselect");
    // <FS:Ansariel> LSL Preprocessor
    //menuItem->setClickCallback(boost::bind(&LLTextEditor::deselect, mEditor));
    //menuItem->setEnableCallback(boost::bind(&LLTextEditor::canDeselect, mEditor));
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Deselect"));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableAction, this, "Deselect"));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Search / Replace...");
//  menuItem->setClickCallback(boost::bind(&LLFloaterScriptSearch::show, this));
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::performAction, this, "Search"));
// [/SL:KB]

    menuItem = getChild<LLMenuItemCallGL>("Go to line...");
    menuItem->setClickCallback(boost::bind(&LLFloaterGotoLine::show, this));

    // <FS:Ansariel> Keep help links
    menuItem = getChild<LLMenuItemCallGL>("Help...");
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnHelp, this));
    // </FS:Ansariel>

    menuItem = getChild<LLMenuItemCallGL>("Keyword Help...");
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnDynamicHelp, this));

    menuItem = getChild<LLMenuItemCallGL>("LoadFromFile");
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnLoadFromFile, this));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableLoadFromFileMenu, this));

    menuItem = getChild<LLMenuItemCallGL>("SaveToFile");
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnSaveToFile, this));
    menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::enableSaveToFileMenu, this));

// <FS:CR> Advanced Script Editor
    menuItem = getChild<LLMenuItemCallGL>("ScriptPrefs");
    menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnPrefs, this));
}

void LLScriptEdCore::initButtonBar()
{
    mSaveBtn->setClickedCallback(boost::bind(&LLScriptEdCore::doSave, this, false, true));
    mSaveBtn2->setClickedCallback(boost::bind(&LLScriptEdCore::doSave, this, false, true)); // <FS:Zi> support extra save button
    mCutBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Cut"));
    mCopyBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Copy"));
    mPasteBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Paste"));
    mUndoBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Undo"));
    mRedoBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Redo"));
    mSaveToDiskBtn->setClickedCallback(boost::bind(&LLScriptEdCore::onBtnSaveToFile, this));
    mLoadFromDiskBtn->setClickedCallback(boost::bind(&LLScriptEdCore::onBtnLoadFromFile, this));
    mSearchBtn->setClickedCallback(boost::bind(&LLScriptEdCore::performAction, this, "Search"));
}

void LLScriptEdCore::updateButtonBar()
{
    mSaveBtn->setEnabled(hasChanged() && !mScriptRemoved);
    mCutBtn->setEnabled(mCurrentEditor->canCut());
    mCopyBtn->setEnabled(mCurrentEditor->canCopy());
    mPasteBtn->setEnabled(mCurrentEditor->canPaste());
    mUndoBtn->setEnabled(mCurrentEditor->canUndo());
    mRedoBtn->setEnabled(mCurrentEditor->canRedo());
    mSaveToDiskBtn->setEnabled(mEditor->canLoadOrSaveToFile());
    mLoadFromDiskBtn->setEnabled(mEditor->canLoadOrSaveToFile());
    //<FS:Kadah> Recompile button
    static LLCachedControl<bool> FSScriptEditorRecompileButton(gSavedSettings, "FSScriptEditorRecompileButton");
    mSaveBtn2->setEnabled((hasChanged() || (mLSLProc && FSScriptEditorRecompileButton && !mCompiling && mHasScriptData)) && !mScriptRemoved);
    mSaveBtn2->setLabel((!mLSLProc || !FSScriptEditorRecompileButton || hasChanged()) ? LLTrans::getString("save_file_verb") : LLTrans::getString("recompile_script_verb"));
    //</FS:Kadah>
}

//<FS:Kadah> Compile Indicators
void LLScriptEdCore::updateIndicators(bool compiling, bool success)
{
    mCompiling = compiling;
    LLLoadingIndicator* compile_indicator = getChild<LLLoadingIndicator>("progress_indicator");
    compile_indicator->setVisible(mCompiling);
    if (mCompiling)
    {
        compile_indicator->start();
    }
    else
    {
        compile_indicator->stop();
    }

    LLIconCtrl* status_indicator = getChild<LLIconCtrl>("status_indicator");
    status_indicator->setVisible(!mCompiling);
    status_indicator->setValue((success ? "Script_Running" : "Script_Error"));
}
//</FS:Kadah>

//static
void LLScriptEdCore::onBtnPrefs(void* userdata)
{
    LLFloaterReg::showInstance("script_colors");
}
// </FS:CR>

// NaCl - LSL Preprocessor
void LLScriptEdCore::onToggleProc()
{
    mErrorList->setCommentText(LLTrans::getString("preproc_toggle_warning"));
    mErrorList->deleteAllItems(); // Make it visible
    updateButtonBar(); // Update the save button in particular (FIRE-10173)
}

void LLScriptEdCore::onPreprocTabChanged(const std::string& tab_name)
{
    mCurrentEditor = (tab_name == "Preprocessed" ? mPostEditor : mEditor);
    LLFloaterSearchReplace* search_floater = LLFloaterSearchReplace::findInstance();
    if (search_floater && (search_floater->getEditor() == mEditor || search_floater->getEditor() == mPostEditor))
    {
        search_floater->setCanReplace(mCurrentEditor == mEditor);
    }
    childSetEnabled("Insert...", mCurrentEditor == mEditor);
}

void LLScriptEdCore::performAction(const std::string& action)
{
    if (action == "Revert All Changes")
    {
        onBtnUndoChanges();
    }
    else if (action == "Undo")
    {
        mCurrentEditor->undo();
    }
    else if (action == "Redo")
    {
        mCurrentEditor->redo();
    }
    else if (action == "Cut")
    {
        mCurrentEditor->cut();
    }
    else if (action == "Copy")
    {
        mCurrentEditor->copy();
    }
    else if (action == "Paste")
    {
        mCurrentEditor->paste();
    }
    else if (action == "Select All")
    {
        mCurrentEditor->selectAll();
    }
    else if (action == "Deselect")
    {
        mCurrentEditor->deselect();
    }
    else if (action == "Search")
    {
        LLFloaterSearchReplace* floater = LLFloaterSearchReplace::show(mCurrentEditor);
        floater->setCanReplace(mCurrentEditor == mEditor);
    }
}

bool LLScriptEdCore::enableAction(const std::string& action)
{
    if (action == "Revert All Changes")
    {
        return (mCurrentEditor == mEditor && hasChanged());
    }
    else if (action == "Undo")
    {
        return mCurrentEditor->canUndo();
    }
    else if (action == "Redo")
    {
        return mCurrentEditor->canRedo();
    }
    else if (action == "Cut")
    {
        return mCurrentEditor->canCut();
    }
    else if (action == "Copy")
    {
        return mCurrentEditor->canCopy();
    }
    else if (action == "Paste")
    {
        return mCurrentEditor->canPaste();
    }
    else if (action == "Select All")
    {
        return mCurrentEditor->canSelectAll();
    }
    else if (action == "Deselect")
    {
        return mCurrentEditor->canDeselect();
    }

    return false;
}
// NaCl End

void LLScriptEdCore::setScriptText(const std::string& text, bool is_valid)
{
    if (mEditor)
    {
        // NaCl - LSL Preprocessor
        std::string ntext = text;
        if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mLSLProc && !mEditor->getIsLuauLanguage())
        {
            if (mPostEditor)
            {
                mPostEditor->setText(ntext);
            }
            ntext = mLSLProc->decode(ntext);
        }
        LLStringUtil::replaceTabsWithSpaces(ntext, mEditor->spacesPerTab());
        // NaCl End
        mEditor->setText(ntext);
        mHasScriptData = is_valid;
    }
}

// NaCl - LSL Preprocessor
std::string LLScriptEdCore::getScriptText()
{
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mPostEditor && !mEditor->getIsLuauLanguage())
    {
        //return mPostEditor->getText();
        return mPostScript;
    }
    else if (mEditor)
    {
        return mEditor->getText();
    }
    return std::string();
}
// NaCl End

void LLScriptEdCore::makeEditorPristine()
{
    if (mEditor)
    {
        mEditor->makePristine();
    }
}

bool LLScriptEdCore::loadScriptText(const std::string& filename)
{
    if (filename.empty())
    {
        LL_WARNS() << "Empty file name" << LL_ENDL;
        return false;
    }

    LLFILE* file = LLFile::fopen(filename, LLFILE_MODE("rb"));       /*Flawfinder: ignore*/
    if (!file)
    {
        LL_WARNS() << "Error opening " << filename << LL_ENDL;
        return false;
    }

    // read in the whole file
    fseek(file, 0L, SEEK_END);
    size_t file_length = (size_t) ftell(file);
    fseek(file, 0L, SEEK_SET);
    char* buffer = new char[file_length+1];
    size_t nread = fread(buffer, 1, file_length, file);
    if (nread < file_length)
    {
        LL_WARNS() << "Short read" << LL_ENDL;
    }
    buffer[nread] = '\0';
    fclose(file);


    std::string text = std::string(buffer);
    // <FS:Zi> Optionally convert tabs to spaces
    //LLStringUtil::replaceTabsWithSpaces(text, LLTextEditor::spacesPerTab());

    if(gSavedSettings.getBOOL("ExternalEditorConvertTabsToSpaces"))
    {
        LLStringUtil::replaceTabsWithSpaces(text, LLTextEditor::spacesPerTab());
    }
    // </FS:Zi>
    mEditor->setText(text);
    delete[] buffer;

    return true;
}

bool LLScriptEdCore::writeToFile(const std::string& filename, bool unprocessed)
{
    LLFILE* fp = LLFile::fopen(filename, LLFILE_MODE("wb"));
    if (!fp)
    {
        LL_WARNS() << "Unable to write to " << filename << LL_ENDL;

        LLSD row;
        row["columns"][0]["value"] = "Error writing to local file. Is your hard drive full?";
        row["columns"][0]["font"] = "SANSSERIF_SMALL";
        mErrorList->addElement(row);
        return false;
    }

    // NaCl - LSL Preprocessor
    std::string utf8text;
    if(!unprocessed)
        utf8text = this->getScriptText();
    else
        utf8text = mEditor->getText();
    // NaCl End

    // Special case for a completely empty script - stuff in one space so it can store properly.  See SL-46889
    if (utf8text.size() == 0)
    {
        utf8text = " ";
    }

    fputs(utf8text.c_str(), fp);
    fclose(fp);
    return true;
}

void LLScriptEdCore::sync()
{
    // Sync with external editor.
    if (mContainer->mLiveFile)
    {
        std::string tmp_file = mContainer->mLiveFile->filename();
        if (LLFile::isfile(tmp_file)) // file exists
        {
            mContainer->mLiveFile->ignoreNextUpdate();
            writeToFile(tmp_file, gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && !mEditor->getIsLuauLanguage());
        }
    }
}

bool LLScriptEdCore::hasChanged() const
{
    if (!mEditor)
        return false;

    return ((!mEditor->isPristine() || mEnableSave) && mHasScriptData);
}

void LLScriptEdCore::draw()
{
// <FS:CR> Advanced Script Editor
    //bool script_changed   = hasChanged();
    //mSaveBtn->setEnabled(script_changed && !mScriptRemoved);
    updateButtonBar();
// </FS:CR>

    if( mEditor->hasFocus() )
    {
        S32 line = 0;
        S32 column = 0;
        mEditor->getCurrentLineAndColumn( &line, &column, false );  // don't include wordwrap
        line = mEditor->getIsLuauLanguage() ? (line + 1) : line;
        column = mEditor->getIsLuauLanguage() ? (column + 1) : column;
        LLStringUtil::format_map_t args;
        std::string cursor_pos;
        args["[LINE]"] = llformat ("%d", line);
        args["[COLUMN]"] = llformat ("%d", column);
        cursor_pos = LLTrans::getString("CursorPos", args);
        mLineCol->setValue(cursor_pos);
    }
    // <FS:Ansariel> LSL Preprocessor
    else if (mPostEditor && mPostEditor->hasFocus())
    {
        S32 line = 0;
        S32 column = 0;
        mPostEditor->getCurrentLineAndColumn( &line, &column, false);  // don't include wordwrap
        LLStringUtil::format_map_t args;
        std::string cursor_pos;
        args["[LINE]"] = llformat ("%d", line);
        args["[COLUMN]"] = llformat ("%d", column);
        cursor_pos = LLTrans::getString("CursorPos", args);
        mLineCol->setValue(cursor_pos);
    }
    // </FS:Ansariel>
    else
    {
        mLineCol->setValue(LLStringUtil::null);
    }

    updateDynamicHelp();

    LLPanel::draw();
}

void LLScriptEdCore::updateDynamicHelp(bool immediate)
{
    LLFloater* help_floater = mLiveHelpHandle.get();
    if (!help_floater) return;

    // update back and forward buttons
    LLButton* fwd_button = help_floater->getChild<LLButton>("fwd_btn");
    LLButton* back_button = help_floater->getChild<LLButton>("back_btn");
    LLMediaCtrl* browser = help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
    back_button->setEnabled(browser->canNavigateBack());
    fwd_button->setEnabled(browser->canNavigateForward());

    if (!immediate && !gSavedSettings.getBOOL("ScriptHelpFollowsCursor"))
    {
        return;
    }

    LLTextSegmentPtr segment = NULL;
    std::vector<LLTextSegmentPtr> selected_segments;
    mEditor->getSelectedSegments(selected_segments);
    LLKeywordToken* token;
    // try segments in selection range first
    std::vector<LLTextSegmentPtr>::iterator segment_iter;
    for (segment_iter = selected_segments.begin(); segment_iter != selected_segments.end(); ++segment_iter)
    {
        token = (*segment_iter)->getToken();
        if(token && isKeyword(token))
        {
            segment = *segment_iter;
            break;
        }
    }

    // then try previous segment in case we just typed it
    if (!segment)
    {
        const LLTextSegmentPtr test_segment = mEditor->getPreviousSegment();
        token = test_segment->getToken();
        if(token && isKeyword(token))
        {
            segment = test_segment;
        }
    }

    if (segment)
    {
        if (segment->getToken() != mLastHelpToken)
        {
            mLastHelpToken = segment->getToken();
            mLiveHelpTimer.start();
        }
        if (immediate || (mLiveHelpTimer.getStarted() && mLiveHelpTimer.getElapsedTimeF32() > LIVE_HELP_REFRESH_TIME))
        {
            // Use Wtext since segment's start/end are made for wstring and will
            // result in a shift for case of multi-byte symbols inside std::string.
            LLWString segment_text = mEditor->getWText().substr(segment->getStart(), segment->getEnd() - segment->getStart());
            std::string help_string = wstring_to_utf8str(segment_text);
            setHelpPage(help_string);
            mLiveHelpTimer.stop();
        }
    }
    else
    {
        if (immediate)
        {
            setHelpPage(LLStringUtil::null);
        }
    }
}

bool LLScriptEdCore::isKeyword(LLKeywordToken* token)
{
    switch(token->getType())
    {
        case LLKeywordToken::TT_CONSTANT:
        case LLKeywordToken::TT_CONTROL:
        case LLKeywordToken::TT_EVENT:
        case LLKeywordToken::TT_FUNCTION:
        case LLKeywordToken::TT_SECTION:
        case LLKeywordToken::TT_TYPE:
        case LLKeywordToken::TT_WORD:
            return true;

        default:
            return false;
    }
}

void LLScriptEdCore::setHelpPage(const std::string& help_string)
{
    LLFloater* help_floater = mLiveHelpHandle.get();
    if (!help_floater) return;

    LLMediaCtrl* web_browser = help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
    if (!web_browser) return;

    LLComboBox* history_combo = help_floater->getChild<LLComboBox>("history_combo");
    if (!history_combo) return;

    LLUIString url_string = gSavedSettings.getString("LSLHelpURL");

    url_string.setArg("[LSL_STRING]", help_string.empty() ? HELP_LSL_PORTAL_TOPIC : help_string);

    addHelpItemToHistory(help_string);

    web_browser->navigateTo(url_string);

}


void LLScriptEdCore::addHelpItemToHistory(const std::string& help_string)
{
    if (help_string.empty()) return;

    LLFloater* help_floater = mLiveHelpHandle.get();
    if (!help_floater) return;

    LLComboBox* history_combo = help_floater->getChild<LLComboBox>("history_combo");
    if (!history_combo) return;

    // separate history items from full item list
    if (mLiveHelpHistorySize == 0)
    {
        history_combo->addSeparator(ADD_TOP);
    }
    // delete all history items over history limit
    while(mLiveHelpHistorySize > MAX_HISTORY_COUNT - 1)
    {
        history_combo->remove(mLiveHelpHistorySize - 1);
        mLiveHelpHistorySize--;
    }

    history_combo->setSimple(help_string);
    S32 index = history_combo->getCurrentIndex();

    // if help string exists in the combo box
    if (index >= 0)
    {
        S32 cur_index = history_combo->getCurrentIndex();
        if (cur_index < mLiveHelpHistorySize)
        {
            // item found in history, bubble up to top
            history_combo->remove(history_combo->getCurrentIndex());
            mLiveHelpHistorySize--;
        }
    }
    history_combo->add(help_string, LLSD(help_string), ADD_TOP);
    history_combo->selectFirstItem();
    mLiveHelpHistorySize++;
}

bool LLScriptEdCore::canClose()
{
    if(mForceClose || !hasChanged() || mScriptRemoved)
    {
        return true;
    }
    else
    {
        if(!mSaveDialogShown)
        {
            mSaveDialogShown = true;
            // Bring up view-modal dialog: Save changes? Yes, No, Cancel
            LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLScriptEdCore::handleSaveChangesDialog, this, _1, _2));
        }
        return false;
    }
}

void LLScriptEdCore::setEnableEditing(bool enable)
{
    mEditor->setEnabled(enable);
    getChildView("Edit_btn")->setEnabled(enable);
    getChildView("edit_btn_2")->setEnabled(enable); // <FS:Zi> support extra edit button
}

bool LLScriptEdCore::handleSaveChangesDialog(const LLSD& notification, const LLSD& response )
{
    mSaveDialogShown = false;
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    switch( option )
    {
    case 0:  // "Yes"
        // close after saving
            doSave( true );
        break;

    case 1:  // "No"
        mForceClose = true;
        // This will close immediately because mForceClose is true, so we won't
        // infinite loop with these dialogs. JC
        ((LLFloater*) getParent())->closeFloater();
        break;

    case 2: // "Cancel"
    default:
        // If we were quitting, we didn't really mean it.
        LLAppViewer::instance()->abortQuit();
        break;
    }
    return false;
}

// <FS:Ansariel> Keep help links
void LLScriptEdCore::onBtnHelp()
{
    LLUI::getInstance()->mHelpImpl->showTopic(HELP_LSL_PORTAL_TOPIC);
}
// </FS:Ansariel>

void LLScriptEdCore::onBtnDynamicHelp()
{
    LLFloater* live_help_floater = mLiveHelpHandle.get();
    if (!live_help_floater)
    {
        live_help_floater = new LLFloater(LLSD());
        live_help_floater->buildFromFile("floater_lsl_guide.xml");
        LLFloater* parent = dynamic_cast<LLFloater*>(getParent());
        llassert(parent);
        if (parent)
            parent->addDependentFloater(live_help_floater, true);
        live_help_floater->childSetCommitCallback("lock_check", onCheckLock, this);
        live_help_floater->getChild<LLUICtrl>("lock_check")->setValue(gSavedSettings.getBOOL("ScriptHelpFollowsCursor"));
        live_help_floater->childSetCommitCallback("history_combo", onHelpComboCommit, this);
        live_help_floater->childSetAction("back_btn", onClickBack, this);
        live_help_floater->childSetAction("fwd_btn", onClickForward, this);

        LLMediaCtrl* browser = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
        browser->setAlwaysRefresh(true);

        LLComboBox* help_combo = live_help_floater->getChild<LLComboBox>("history_combo");
        LLKeywordToken *token;
        LLKeywords::keyword_iterator_t token_it;
        for (token_it = mEditor->keywordsBegin();
             token_it != mEditor->keywordsEnd();
             ++token_it)
        {
            token = token_it->second;
            help_combo->add(wstring_to_utf8str(token->getToken()));
        }
        help_combo->sortByName();

        // re-initialize help variables
        mLastHelpToken = NULL;
        mLiveHelpHandle = live_help_floater->getHandle();
        mLiveHelpHistorySize = 0;
    }

    bool visible = true;
    bool take_focus = true;
    live_help_floater->setVisible(visible);
    live_help_floater->setFrontmost(take_focus);

    updateDynamicHelp(true);
}

//static
void LLScriptEdCore::onClickBack(void* userdata)
{
    LLScriptEdCore* corep = (LLScriptEdCore*)userdata;
    LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
    if (live_help_floater)
    {
        LLMediaCtrl* browserp = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
        if (browserp)
        {
            browserp->navigateBack();
        }
    }
}

//static
void LLScriptEdCore::onClickForward(void* userdata)
{
    LLScriptEdCore* corep = (LLScriptEdCore*)userdata;
    LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
    if (live_help_floater)
    {
        LLMediaCtrl* browserp = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
        if (browserp)
        {
            browserp->navigateForward();
        }
    }
}

// static
void LLScriptEdCore::onCheckLock(LLUICtrl* ctrl, void* userdata)
{
    LLScriptEdCore* corep = (LLScriptEdCore*)userdata;

    // clear out token any time we lock the frame, so we will refresh web page immediately when unlocked
    gSavedSettings.setBOOL("ScriptHelpFollowsCursor", ctrl->getValue().asBoolean());

    corep->mLastHelpToken = NULL;
}

// static
void LLScriptEdCore::onBtnInsertSample(void* userdata)
{
    LLScriptEdCore* self = (LLScriptEdCore*) userdata;

    // Insert sample code
    self->mEditor->selectAll();
    self->mEditor->cut();
    self->mEditor->insertText(self->mSampleText);
}

// static
void LLScriptEdCore::onHelpComboCommit(LLUICtrl* ctrl, void* userdata)
{
    LLScriptEdCore* corep = (LLScriptEdCore*)userdata;

    LLFloater* live_help_floater = corep->mLiveHelpHandle.get();
    if (live_help_floater)
    {
        std::string help_string = ctrl->getValue().asString();

        corep->addHelpItemToHistory(help_string);

        LLMediaCtrl* web_browser = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
        LLUIString url_string = gSavedSettings.getString("LSLHelpURL");
        url_string.setArg("[LSL_STRING]", help_string);
        web_browser->navigateTo(url_string);
    }
}

// static
void LLScriptEdCore::onBtnInsertFunction(LLUICtrl *ui, void* userdata)
{
    LLScriptEdCore* self = (LLScriptEdCore*) userdata;

    // Insert sample code
    if(self->mEditor->getEnabled())
    {
        self->mEditor->insertText(self->mFunctions->getSimple());
    }
    self->mEditor->setFocus(true);
    self->setHelpPage(self->mFunctions->getSimple());
}

// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
//void LLScriptEdCore::doSave( bool close_after_save )
void LLScriptEdCore::doSave(bool close_after_save, bool sync /*= true*/)
// </FS:Ansariel>
{
    // NaCl - LSL Preprocessor
    // Clear status list *before* running the preprocessor (FIRE-10172) -Sei
    mErrorList->deleteAllItems();
    mErrorList->setCommentText(std::string());

    updateIndicators(true, false); //<FS:Kadah> Compile Indicators

    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mLSLProc && !mEditor->getIsLuauLanguage())
    {
        LL_INFOS() << "passing to preproc" << LL_ENDL;
        mLSLProc->preprocess_script(close_after_save, sync);
    }
    else
    {
        if( mSaveCallback )
        {
            mSaveCallback( mUserdata, close_after_save, sync );
        }
    }
    // NaCl End
}

// NaCl - LSL Preprocessor
void LLScriptEdCore::doSaveComplete( void* userdata, bool close_after_save, bool sync)
{
    add( LLStatViewer::LSL_SAVES,1 );

    //LLScriptEdCore* self = (LLScriptEdCore*) userdata;

    if( mSaveCallback )
    {
        mSaveCallback( mUserdata, close_after_save, sync );
    }
}
// NaCl End

void LLScriptEdCore::openInExternalEditor()
{
    //if (mContainer->mLiveFile)
    //{
    //    // If already open in an external editor, just return
    //    return;
    //}
    // Open it in external editor.
    {
        LLExternalEditor ed;
        LLExternalEditor::EErrorCode status;
        std::string msg;

        status = ed.setCommand("LL_SCRIPT_EDITOR");
        if (status != LLExternalEditor::EC_SUCCESS)
        {
            if (status == LLExternalEditor::EC_NOT_SPECIFIED) // Use custom message for this error.
            {
                msg = LLTrans::getString("ExternalEditorNotSet");
            }
            else
            {
                msg = LLExternalEditor::getErrorMessage(status);
            }

            LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", msg));
            return;
        }

        // FS:TS FIRE-6122: Script contents clobbered when Edit button
        //  clicked with preprocessor active. Fix from NaCl (code moved
        //  from above).

    // Generate a suitable filename
    std::string script_name = mScriptName;

    static const std::set<char> forbidden_chars{ '<', '>', ':', '"', '\\', '/', '|', '?', '*' };
    script_name.erase(
        std::remove_if(script_name.begin(), script_name.end(), [](char c) {
            return forbidden_chars.contains(c);
        }), script_name.end());

    std::string filename = mContainer->getTmpFileName(script_name);

    // Save the script to a temporary file.
    if (!writeToFile(filename, gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && !mEditor->getIsLuauLanguage()))
    {
        // In case some characters from script name are forbidden
        // and not accounted for, name is too long or some other issue,
        // try file that doesn't include script name
        script_name.clear();
        filename = mContainer->getTmpFileName(script_name);
        writeToFile(filename, gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && !mEditor->getIsLuauLanguage());
    }

    if (mContainer->mLiveFile && mContainer->mLiveFile->filename() != filename)
    { // The name may have changed if we changed the type of scipt being edited.
        delete mContainer->mLiveFile;
        mContainer->mLiveFile = NULL;
    }
    // Start watching file changes.
    if (!mContainer->mLiveFile)
    {
        mContainer->mLiveFile = new LLLiveLSLFile(filename, boost::bind(&LLScriptEdContainer::onExternalChange, mContainer, _1));
        mContainer->mLiveFile->addToEventTimer();
    }
    mContainer->startWebsocketServer();
        // FS:TS FIRE-6122 end

        status = ed.run(filename);
        if (status != LLExternalEditor::EC_SUCCESS)
        {
            msg = LLExternalEditor::getErrorMessage(status);
            LLNotificationsUtil::add("GenericAlert", LLSD().with("MESSAGE", msg));
        }
    }
}

void LLScriptEdCore::onBtnUndoChanges()
{
    if( !mEditor->tryToRevertToPristineState() )
    {
        LLNotificationsUtil::add("ScriptCannotUndo", LLSD(), LLSD(), boost::bind(&LLScriptEdCore::handleReloadFromServerDialog, this, _1, _2));
    }
}

// static
void LLScriptEdCore::onErrorList(LLUICtrl*, void* user_data)
{
    LLScriptEdCore* self = (LLScriptEdCore*)user_data;
    LLScrollListItem* item = self->mErrorList->getFirstSelected();
    if(item)
    {
        // *FIX: replace with boost grep
        S32 row = 0;
        S32 column = 0;
        const LLScrollListCell* cell = item->getColumn(0);
        std::string line(cell->getValue().asString());
        line.erase(0, 1);
        LLStringUtil::replaceChar(line, ',',' ');
        LLStringUtil::replaceChar(line, ')',' ');
        sscanf(line.c_str(), "%d %d", &row, &column);
        //LL_INFOS() << "LLScriptEdCore::onErrorList() - " << row << ", "
        //<< column << LL_ENDL;
        row = (self->mEditor->getIsLuauLanguage() ? row - 1 : row);
        // NaCl - LSL Preprocessor
        if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && self->mPostEditor && self->mPreprocTab && !self->mEditor->getIsLuauLanguage())
        {
            self->mPreprocTab->selectTabByName("Preprocessed");
            self->getChild<LLPanel>("Preprocessed")->setFocus(true);
            self->mPostEditor->setFocus(true);
            self->mPostEditor->setCursor(row, column);
        }
        else
        {
            self->mEditor->setCursor(row, column);
            self->mEditor->setFocus(true);
        }
        // NaCl End
    }
}

bool LLScriptEdCore::handleReloadFromServerDialog(const LLSD& notification, const LLSD& response )
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    switch( option )
    {
    case 0: // "Yes"
        if( mLoadCallback )
        {
            setScriptText(getString("loading"), false);
            mLoadCallback(mUserdata);
        }
        break;

    case 1: // "No"
        break;

    default:
        llassert(0);
        break;
    }
    return false;
}

void LLScriptEdCore::selectFirstError()
{
    // Select the first item;
    mErrorList->selectFirstItem();
    onErrorList(mErrorList, this);
}


struct LLEntryAndEdCore
{
    LLScriptEdCore* mCore;
    LLEntryAndEdCore(LLScriptEdCore* core) :
        mCore(core)
        {}
};

void LLScriptEdCore::deleteBridges()
{
    auto count = mBridges.size();
    LLEntryAndEdCore* eandc;
    for(size_t i = 0; i < count; i++)
    {
        eandc = mBridges.at(i);
        delete eandc;
        mBridges[i] = NULL;
    }
    mBridges.clear();
}

// virtual
bool LLScriptEdCore::handleKeyHere(KEY key, MASK mask)
{
    bool just_control = MASK_CONTROL == (mask & MASK_MODIFIERS);

    if(('S' == key) && just_control)
    {
        if(mSaveCallback)
        {
            // don't close after saving
            // NaCl - LSL Preprocessor
            if (!hasChanged())
            {
                LL_INFOS() << "Save Not Needed" << LL_ENDL;
                return true;
            }
            doSave(false);
            // NaCl End

            //mSaveCallback(mUserdata, false);
        }

        return true;
    }

    if(('F' == key) && just_control)
    {
        if(mSearchReplaceCallback)
        {
            mSearchReplaceCallback(mUserdata);
        }

        return true;
    }

    return false;
}

void LLScriptEdCore::onBtnLoadFromFile( void* data )
{
    LLFilePickerReplyThread::startPicker(boost::bind(&LLScriptEdCore::loadScriptFromFile, _1, data), LLFilePicker::FFLOAD_SCRIPT, false);
}

void LLScriptEdCore::loadScriptFromFile(const std::vector<std::string>& filenames, void* data)
{
    std::string filename = filenames[0];

    llifstream fin(filename.c_str());

    std::string line;
    std::string text;
    std::string linetotal;
    while (!fin.eof())
    {
        getline(fin, line);
        text += line;
        if (!fin.eof())
        {
            text += "\n";
        }
    }
    fin.close();

    // Only replace the script if there is something to replace with.
    LLScriptEdCore* self = (LLScriptEdCore*)data;
    if (self && (text.length() > 0))
    {
        self->mEditor->selectAll();
        LLWString script(utf8str_to_wstring(text));
        self->mEditor->insertText(script);
    }
}

void LLScriptEdCore::onBtnSaveToFile( void* userdata )
{
    add(LLStatViewer::LSL_SAVES, 1);

    LLScriptEdCore* self = (LLScriptEdCore*) userdata;

    if( self->mSaveCallback )
    {
        LLFilePickerReplyThread::startPicker(boost::bind(&LLScriptEdCore::saveScriptToFile, _1, userdata), LLFilePicker::FFSAVE_SCRIPT, self->mScriptName);
    }
}

void LLScriptEdCore::saveScriptToFile(const std::vector<std::string>& filenames, void* data)
{
    LLScriptEdCore* self = (LLScriptEdCore*)data;
    if (self)
    {
        std::string filename = filenames[0];
        std::string scriptText = self->mEditor->getText();
        llofstream fout(filename.c_str());
        fout << (scriptText);
        fout.close();
            // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
            //self->mSaveCallback(self->mUserdata, false);
            self->mSaveCallback(self->mUserdata, false, true);
            // </FS:Ansariel>
    }
}

bool LLScriptEdCore::canLoadOrSaveToFile( void* userdata )
{
    LLScriptEdCore* self = (LLScriptEdCore*) userdata;
    return self->mEditor->canLoadOrSaveToFile();
}

// static
bool LLScriptEdCore::enableSaveToFileMenu(void* userdata)
{
    LLScriptEdCore* self = (LLScriptEdCore*)userdata;
    if (!self || !self->mEditor) return false;
    return self->mEditor->canLoadOrSaveToFile();
}

// static
bool LLScriptEdCore::enableLoadFromFileMenu(void* userdata)
{
    LLScriptEdCore* self = (LLScriptEdCore*)userdata;
    return (self && self->mEditor) ? self->mEditor->canLoadOrSaveToFile() : false;
}

LLUUID LLScriptEdCore::getAssociatedExperience()const
{
    return mAssociatedExperience;
}

// <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
//void LLScriptEdCore::onChangeFontSize(const LLSD &userdata)
//{
//    const std::string font_name = userdata.asString();
//    gSavedSettings.setString("LSLFontSizeName", font_name);
//}
//
//bool LLScriptEdCore::isFontSizeChecked(const LLSD &userdata)
//{
//    const std::string current_size_name = LLScriptEditor::getScriptFontSize();
//    const std::string size_name = userdata.asString();
//
//    return (size_name == current_size_name);
//}

void LLScriptEdCore::onFontChanged()
{
    LLFontGL* font = LLFontGL::getFont(LLFontDescriptor(gSavedSettings.getString("FSScriptingFontName"), gSavedSettings.getString("FSScriptingFontSize"), LLFontGL::NORMAL));
    if (font)
    {
        mEditor->setFont(font);
        if (mPostEditor)
        {
            mPostEditor->setFont(font);
        }
    }
}
// </FS:Ansariel>

void LLLiveLSLEditor::setExperienceIds( const LLSD& experience_ids )
{
    mExperienceIds=experience_ids;
    updateExperiencePanel();
}


void LLLiveLSLEditor::updateExperiencePanel()
{
    if(mScriptEd->getAssociatedExperience().isNull())
    {
        mExperienceEnabled->set(false);
        mExperiences->setVisible(false);
        if(mExperienceIds.size()>0)
        {
            mExperienceEnabled->setEnabled(true);
            mExperienceEnabled->setToolTip(getString("add_experiences"));
        }
        else
        {
            mExperienceEnabled->setEnabled(false);
            mExperienceEnabled->setToolTip(getString("no_experiences"));
        }
        mViewProfileButton->setVisible(false);
    }
    else
    {
        mExperienceEnabled->setToolTip(getString("experience_enabled"));
        mExperienceEnabled->setEnabled(getIsModifiable());
        mExperienceEnabled->set(true);
        mExperiences->setVisible(true);
        mViewProfileButton->setToolTip(getString("show_experience_profile"));
        buildExperienceList();
    }
}

void LLLiveLSLEditor::buildExperienceList()
{
    mExperiences->clearRows();
    bool foundAssociated=false;
    const LLUUID& associated = mScriptEd->getAssociatedExperience();
    LLUUID last;
    LLScrollListItem* item;
    for(LLSD::array_const_iterator it = mExperienceIds.beginArray(); it != mExperienceIds.endArray(); ++it)
    {
        LLUUID id = it->asUUID();
        EAddPosition position = ADD_BOTTOM;
        if(id == associated)
        {
            foundAssociated = true;
            position = ADD_TOP;
        }

        const LLSD& experience = LLExperienceCache::instance().get(id);
        if(experience.isUndefined())
        {
            mExperiences->add(getString("loading"), id, position);
            last = id;
        }
        else
        {
            std::string experience_name_string = experience[LLExperienceCache::NAME].asString();
            if (experience_name_string.empty())
            {
                experience_name_string = LLTrans::getString("ExperienceNameUntitled");
            }
            mExperiences->add(experience_name_string, id, position);
        }
    }

    if(!foundAssociated )
    {
        const LLSD& experience = LLExperienceCache::instance().get(associated);
        if(experience.isDefined())
        {
            std::string experience_name_string = experience[LLExperienceCache::NAME].asString();
            if (experience_name_string.empty())
            {
                experience_name_string = LLTrans::getString("ExperienceNameUntitled");
            }
            item=mExperiences->add(experience_name_string, associated, ADD_TOP);
        }
        else
        {
            item=mExperiences->add(getString("loading"), associated, ADD_TOP);
            last = associated;
        }
        item->setEnabled(false);
    }

    if(last.notNull())
    {
        mExperiences->setEnabled(false);
        LLExperienceCache::instance().get(last, boost::bind(&LLLiveLSLEditor::buildExperienceList, this));
    }
    else
    {
        mExperiences->setEnabled(true);
        mExperiences->sortByName(true);
        mExperiences->setCurrentByIndex(mExperiences->getCurrentIndex());
        mViewProfileButton->setVisible(true);
    }
}


void LLScriptEdCore::setAssociatedExperience( const LLUUID& experience_id )
{
    mAssociatedExperience = experience_id;
}

void LLLiveLSLEditor::requestExperiences()
{
    if (!getIsModifiable())
    {
        return;
    }

    LLViewerRegion* region = gAgent.getRegion();
    if (region)
    {
        std::string lookup_url=region->getCapability("GetCreatorExperiences");
        if(!lookup_url.empty())
        {
            LLCoreHttpUtil::HttpCoroutineAdapter::completionCallback_t success =
                boost::bind(&LLLiveLSLEditor::receiveExperienceIds, _1, getDerivedHandle<LLLiveLSLEditor>());

            LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(lookup_url, success);
        }
    }
}

/*static*/
void LLLiveLSLEditor::receiveExperienceIds(LLSD result, LLHandle<LLLiveLSLEditor> hparent)
{
    LLLiveLSLEditor* parent = hparent.get();
    if (!parent)
        return;

    parent->setExperienceIds(result["experience_ids"]);
}


/// ---------------------------------------------------------------------------
/// LLScriptEdContainer
/// ---------------------------------------------------------------------------

LLScriptEdContainer::LLScriptEdContainer(const LLSD& key) :
    LLPreview(key)
,   mScriptEd(nullptr)
,   mLiveFile(nullptr)
,   mLiveLogFile(nullptr)
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
,   mBackupTimer(nullptr)
// [/SL:KB]
{
}

LLScriptEdContainer::~LLScriptEdContainer()
{
    delete mLiveFile;
    mLiveFile = nullptr;

    delete mLiveLogFile;
    mLiveLogFile = nullptr;

    if (!mWebSocketServer.expired())
    {
        unsubscribeScript();
    }

    // [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    // Clean up the backup file (unless we've gotten disconnected)
    if (!mBackupFilename.empty() && gAgent.getRegion())
        LLFile::remove(mBackupFilename);
    delete mBackupTimer;
    // [/SL:KB]
}

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
void LLScriptEdContainer::refreshFromItem()
{
    LLPreview::refreshFromItem();

    if (!mBackupFilename.empty())
    {
        const std::string strFilename = getBackupFileName();
        if (strFilename != mBackupFilename)
        {
            LLFile::rename(mBackupFilename, strFilename);
            mBackupFilename = strFilename;
        }
    }
}

bool LLScriptEdContainer::onBackupTimer()
{
    if (mScriptEd && mScriptEd->hasChanged())
    {
        if (mBackupFilename.empty())
            mBackupFilename = getBackupFileName();
        mScriptEd->writeToFile(mBackupFilename, true);

        LL_INFOS() << "Backing up script to " << mBackupFilename << LL_ENDL;
    }
    return false;
}

std::string LLScriptEdContainer::getBackupFileName() const
{
    // NOTE: this function is not guaranteed to return the same filename every time (i.e. the item name may have changed)
    std::string strFile = LLFile::tmpdir();

    std::string agent_id_str = gAgentID.asString();
    LLStringUtil::replaceString(agent_id_str, "-", "");
    strFile += agent_id_str + "_";

    // Find the inventory item for this script
    const LLInventoryItem* pItem = getItem();
    if (pItem)
    {
        strFile += gDirUtilp->getScrubbedFileName(pItem->getName().substr(0, 32));
        strFile += "-";
    }

    // Append a CRC of the item UUID to make the filename (hopefully) unique
    LLCRC crcUUID;
    crcUUID.update((U8*)(&mItemUUID.mData), UUID_BYTES);
    strFile += llformat("%X", crcUUID.getCRC());

    strFile += ".lslbackup";

    return strFile;
}
// [/SL:KB]

std::string LLScriptEdContainer::getTmpFileName(const std::string& script_name) const
{
    std::string script_id_hash_str(getUniqueHash());
    std::string script_extension = mScriptEd->mEditor->getIsLuauLanguage() ? ".luau" : ".lsl";

    if (script_name.empty())
    {
        return std::string(LLFile::tmpdir()) + "sl_script_" + script_id_hash_str + script_extension;
    }
    else
    {
        return std::string(LLFile::tmpdir()) + "sl_script_" + script_name + "_" + script_id_hash_str + script_extension;
    }
}

std::string LLScriptEdContainer::getUniqueHash() const
{
    // Take script inventory item id (within the object inventory)
    // to consideration so that it's possible to edit multiple scripts
    // in the same object inventory simultaneously (STORM-781).
    std::string script_id = mObjectUUID.asString() + "_" + mItemUUID.asString();

    // Use MD5 sum to make the file name shorter and not exceed maximum path length.
    char  script_id_hash_str[33]; /* Flawfinder: ignore */
    LLMD5 script_id_hash((const U8*)script_id.c_str());
    script_id_hash.hex_digest(script_id_hash_str);

    return std::string(script_id_hash_str);
}

std::string LLScriptEdContainer::getErrorLogFileName(const std::string& script_path)
{
    if (script_path.empty())
    {
        return std::string();
    }

    return script_path + ".log";
}

bool LLScriptEdContainer::logErrorsToFile(const LLSD& compile_errors)
{
    if (!isOpenInExternalEditor())
    {
        return false;
    }

    std::string script_path = getTmpFileName(mScriptEd->mScriptName);
    std::string log_path = getErrorLogFileName(script_path);

    llofstream file(log_path.c_str());
    if (!file.is_open())
    {
        LL_WARNS() << "Unable to open error log file: " << log_path << LL_ENDL;
        return false;
    }

    // Write timestamp
    std::string timestamp = LLLogChat::timestamp2LogString(0, true);
    file << "// " << timestamp << "\n\n";

    // Write errors
    for (LLSD::array_const_iterator line = compile_errors.beginArray();
         line < compile_errors.endArray();
         line++)
    {
        std::string error_message = line->asString();
        LLStringUtil::stripNonprintable(error_message);
        file << error_message << "\n";
    }

    file.close();

    // Create a log file handler if we don't already have one,
    // this is needed to delete the temporary log file properly
    if (!mLiveLogFile && !log_path.empty())
    {
        // Empty callback since we don't need to react to file changes
        mLiveLogFile = new LLLiveLSLFile(log_path, [](const std::string& filename) { return true; });
    }

    return true;
}

bool LLScriptEdContainer::onExternalChange(const std::string& filename)
{
    if (!mScriptEd->loadScriptText(filename))
    {
        return false;
    }

    // Disable sync to avoid recursive load->save->load calls.
    // <FS> LSL preprocessor
    // Ansariel: Don't call saveIfNeeded directly, as we might have to run the
    // preprocessor first. saveIfNeeded will be invoked via callback. Make sure
    // to pass sync = false - we don't need to update the external editor in this
    // case or the next save will be ignored!
    //saveIfNeeded(false);
    mScriptEd->doSave(false, false);
    // </FS>
    return true;
}

bool LLScriptEdContainer::handleKeyHere(KEY key, MASK mask)
{
    if (('A' == key) && (MASK_CONTROL == (mask & MASK_MODIFIERS)))
    {
        mScriptEd->selectAll();
        return true;
    }

    if (!LLPreview::handleKeyHere(key, mask))
    {
        return mScriptEd->handleKeyHere(key, mask);
    }
    return true;
}

void LLScriptEdContainer::startWebsocketServer()
{
    if (gSavedSettings.getBOOL("ExternalWebsocketSyncEnable"))
    {
        // Attempt to find an existing server
        LLWebsocketMgr&               wsmgr  = LLWebsocketMgr::instance();
        LLScriptEditorWSServer::ptr_t server =
            std::static_pointer_cast<LLScriptEditorWSServer>(
                wsmgr.findServerByName(LLScriptEditorWSServer::DEFAULT_SERVER_NAME));

        if (!server)
        {   // We couldn't find one, so create it
            U16 server_port = static_cast<U16>(gSavedSettings.getS32("ExternalWebsocketSyncPort"));
            bool server_localhost = gSavedSettings.getBOOL("ExternalWebsocketSyncLocal");
            server = std::make_shared<LLScriptEditorWSServer>(LLScriptEditorWSServer::DEFAULT_SERVER_NAME, server_port, server_localhost);
            wsmgr.addServer(server);
        }

        bool is_running = server->isRunning();
        if (!is_running)
        {   // Server isn't running, so start it
            is_running = wsmgr.startServer(LLScriptEditorWSServer::DEFAULT_SERVER_NAME);
        }

        if (!is_running && !server->isRunning())
        {   // Failed to start the server
            LL_WARNS() << "Failed to start script editor websocket server" << LL_ENDL;
            return;
        }

        std::string script_id_hash_str(getUniqueHash());
        server->subscribeScriptEditor(mObjectUUID, mItemUUID, mScriptEd->mScriptName, getHandle(), script_id_hash_str);
        mWebSocketServer = server;
    }
}

void LLScriptEdContainer::unsubscribeScript()
{
    auto server = mWebSocketServer.lock();
    if (server)
    {
        std::string script_id_hash_str(getUniqueHash());
        server->sendUnsubscribeScriptEditor(script_id_hash_str);
        server->unsubscribeEditor(script_id_hash_str);
    }
}

void LLScriptEdContainer::sendCompileResults(LLSD& params)
{
    auto server = mWebSocketServer.lock();
    if (server)
    {
        std::string script_id_hash_str(getUniqueHash());
        server->sendCompileResults(script_id_hash_str, params);
    }
}

// <FS:Ansariel> FIRE-16740: Color syntax highlighting changes don't immediately appear in script window
void LLScriptEdContainer::updateStyle()
{
    if (mScriptEd && mScriptEd->mEditor)
    {
        mScriptEd->mEditor->initKeywords();
        mScriptEd->mEditor->loadKeywords();
    }
}
// </FS:Ansariel>

/// ---------------------------------------------------------------------------
/// LLPreviewLSL
/// ---------------------------------------------------------------------------

struct LLScriptSaveInfo
{
    LLUUID mItemUUID;
    std::string mDescription;
    LLTransactionID mTransactionID;

    LLScriptSaveInfo(const LLUUID& uuid, const std::string& desc, LLTransactionID tid) :
        mItemUUID(uuid), mDescription(desc),  mTransactionID(tid) {}
};



//static
void* LLPreviewLSL::createScriptEdPanel(void* userdata)
{

    LLPreviewLSL *self = (LLPreviewLSL*)userdata;

    self->mScriptEd =  new LLScriptEdCore(
                                   self,
                                   std::string(),
                                   self->getHandle(),
                                   LLPreviewLSL::onLoad,
                                   LLPreviewLSL::onSave,
                                   LLPreviewLSL::onSearchReplace,
                                   self,
                                   false,
                                   0);
    return self->mScriptEd;
}


LLPreviewLSL::LLPreviewLSL(const LLSD& key )
:   LLScriptEdContainer(key),
    mPendingUploads(0)
{
    mFactoryMap["script panel"] = LLCallbackMap(LLPreviewLSL::createScriptEdPanel, this);

    mItemObserver = new LLScriptMovedObserver(this);
}

LLPreviewLSL::~LLPreviewLSL()
{
    delete mItemObserver;
    mItemObserver = NULL;
}

// virtual
bool LLPreviewLSL::postBuild()
{
    const LLInventoryItem* item = getItem();

    llassert(item);
    if (item)
    {
        getChild<LLUICtrl>("desc")->setValue(item->getDescription());

        std::string item_path = get_category_path(item->getParentUUID());
        // <FS:Ansariel> Make ugly location display better
        //getChild<LLUICtrl>("path_txt")->setValue(item_path);
        //getChild<LLUICtrl>("path_txt")->setToolTip(item_path);
        getChild<LLUICtrl>("path_txt")->setTextArg("[PATH]", LLStringExplicit(item_path));
        getChild<LLUICtrl>("path_txt")->setToolTipArg(LLStringExplicit("[PATH]"), LLStringExplicit(item_path));
        // </FS:Ansariel>
    }
    // <FS:Ansariel> Make ugly location display better
    else
    {
        getChild<LLUICtrl>("path_txt")->setTextArg("[PATH]", LLStringExplicit("-"));
        getChild<LLUICtrl>("path_txt")->setToolTipArg(LLStringExplicit("[PATH]"), LLStringExplicit("-"));
    }
    // </FS:Ansariel>
    childSetCommitCallback("desc", LLPreview::onText, this);
    getChild<LLLineEditor>("desc")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

    mScriptEd->mCompileTarget = getChild<LLComboBox>("compile_target");
    mScriptEd->mCompileTarget->setCommitCallback([&](LLUICtrl*, const LLSD&) { onCompileTargetChanged(); });

    return LLPreview::postBuild();
}

void LLPreviewLSL::draw()
{
    const LLInventoryItem* item = getItem();
    if(!item)
    {
        setTitle(LLTrans::getString("ScriptWasDeleted"));
        mScriptEd->setItemRemoved(true);
        // <FS:Ansariel> Make ugly location display better
        getChild<LLUICtrl>("path_txt")->setTextArg("[PATH]", LLStringExplicit("-"));
        getChild<LLUICtrl>("path_txt")->setToolTipArg(LLStringExplicit("[PATH]"), LLStringExplicit("-"));
        // </FS:Ansariel>
    }
    else if (mDirty)
    {
        std::string item_path = get_category_path(item->getParentUUID());
        // <FS:Ansariel> Make ugly location display better
        //getChild<LLUICtrl>("path_txt")->setValue(item_path);
        //getChild<LLUICtrl>("path_txt")->setToolTip(item_path);
        getChild<LLUICtrl>("path_txt")->setTextArg("[PATH]", LLStringExplicit(item_path));
        getChild<LLUICtrl>("path_txt")->setToolTipArg(LLStringExplicit("[PATH]"), LLStringExplicit(item_path));
        // </FS:Ansariel>
    }
    LLPreview::draw();
}
// virtual
void LLPreviewLSL::callbackLSLCompileSucceeded()
{
    LL_INFOS() << "LSL Bytecode saved" << LL_ENDL;
    // <FS> Append comment text
    //mScriptEd->mErrorList->setCommentText(LLTrans::getString("CompileSuccessful"));
    //mScriptEd->mErrorList->setCommentText(LLTrans::getString("SaveComplete"));
    mScriptEd->mErrorList->addCommentText(LLTrans::getString("CompileSuccessful"));
    mScriptEd->mErrorList->addCommentText(LLTrans::getString("SaveComplete"));
    // </FS>

    mScriptEd->updateIndicators(false, true); //<FS:Kadah> Compile Indicators

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    // Script was successfully saved so delete our backup copy if we have one and the editor is still pristine
    if ( (!mBackupFilename.empty()) && (!mScriptEd->hasChanged()) )
    {
        LLFile::remove(mBackupFilename);
        mBackupFilename.clear();
    }
// [/SL:KB]


    if (isOpenInExternalEditor())
    {
        LLSD success_msg;
        success_msg.append(LLTrans::getString("CompileSuccessful"));
        success_msg.append(LLTrans::getString("SaveComplete"));
        logErrorsToFile(success_msg);
    }

    closeIfNeeded();
}

// virtual
void LLPreviewLSL::callbackLSLCompileFailed(const LLSD& compile_errors)
{
    LL_INFOS() << "Compile failed!" << LL_ENDL;

    for(LLSD::array_const_iterator line = compile_errors.beginArray();
        line < compile_errors.endArray();
        line++)
    {
        LLSD row;
        std::string error_message = line->asString();
        LLStringUtil::stripNonprintable(error_message);
        row["columns"][0]["value"] = error_message;
        row["columns"][0]["font"] = "OCRA";
        mScriptEd->mErrorList->addElement(row);
    }

    if (isOpenInExternalEditor())
    {
        logErrorsToFile(compile_errors);
    }

    mScriptEd->selectFirstError();

    mScriptEd->updateIndicators(false, false); //<FS:Kadah> Compile Indicators

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    // Script was successfully saved so delete our backup copy if we have one and the editor is still pristine
    if ( (!mBackupFilename.empty()) && (!mScriptEd->hasChanged()) )
    {
        LLFile::remove(mBackupFilename);
        mBackupFilename.clear();
    }
// [/SL:KB]

    closeIfNeeded();
}

void LLPreviewLSL::loadAsset()
{
    // *HACK: we poke into inventory to see if it's there, and if so,
    // then it might be part of the inventory library. If it's in the
    // library, then you can see the script, but not modify it.
    const LLInventoryItem* item = gInventory.getItem(mItemUUID);
    bool is_library = item
        && !gInventory.isObjectDescendentOf(mItemUUID,
                                            gInventory.getRootFolderID());
    if(!item)
    {
        // do the more generic search.
        getItem();
    }
    if(item)
    {
        bool is_copyable = gAgent.allowOperation(PERM_COPY,
                                item->getPermissions(), GP_OBJECT_MANIPULATE);
        bool is_modifiable = gAgent.allowOperation(PERM_MODIFY,
                                item->getPermissions(), GP_OBJECT_MANIPULATE);
        if (gAgent.isGodlike() || (is_copyable && (is_modifiable || is_library)))
        {
            LLUUID* new_uuid = new LLUUID(mItemUUID);
            gAssetStorage->getInvItemAsset(LLHost(),
                                        gAgent.getID(),
                                        gAgent.getSessionID(),
                                        item->getPermissions().getOwner(),
                                        LLUUID::null,
                                        item->getUUID(),
                                        item->getAssetUUID(),
                                        item->getType(),
                                        &LLPreviewLSL::onLoadComplete,
                                        (void*)new_uuid,
                                        true);
            mAssetStatus = PREVIEW_ASSET_LOADING;
        }
        else
        {
            mScriptEd->setScriptText(mScriptEd->getString("can_not_view"), false);
            mScriptEd->mEditor->makePristine();
            mScriptEd->mFunctions->setEnabled(false);
            mAssetStatus = PREVIEW_ASSET_LOADED;
        }
        getChildView("lock")->setVisible( !is_modifiable);
        mScriptEd->getChildView("Insert...")->setEnabled(is_modifiable);
    }
}


bool LLPreviewLSL::canClose()
{
    return mScriptEd->canClose();
}

void LLPreviewLSL::closeIfNeeded()
{
    // Find our window and close it if requested.
    getWindow()->decBusyCount();
    mPendingUploads--;
    if (mPendingUploads <= 0 && mCloseAfterSave)
    {
        closeFloater();
    }
}

void LLPreviewLSL::onSearchReplace(void* userdata)
{
    LLPreviewLSL* self = (LLPreviewLSL*)userdata;
    LLScriptEdCore* sec = self->mScriptEd;
//  LLFloaterScriptSearch::show(sec);
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
    LLFloaterSearchReplace* floater = LLFloaterSearchReplace::show(sec->mCurrentEditor);
    floater->setCanReplace(sec->mCurrentEditor == sec->mEditor);
// [/SL:KB]
}

// static
void LLPreviewLSL::onLoad(void* userdata)
{
    LLPreviewLSL* self = (LLPreviewLSL*)userdata;
    self->loadAsset();
}

// static
// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
//void LLPreviewLSL::onSave(void* userdata, bool close_after_save)
void LLPreviewLSL::onSave(void* userdata, bool close_after_save, bool sync)
// </FS:Ansariel>
{
    LLPreviewLSL* self = (LLPreviewLSL*)userdata;
    self->mCloseAfterSave = close_after_save;
    // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
    //self->saveIfNeeded();
    self->saveIfNeeded(sync);
    // </FS:Ansariel>
}

/*static*/
void LLPreviewLSL::finishedLSLUpload(LLUUID itemId, LLSD response)
{
    // Find our window and close it if requested.
    LLPreviewLSL* preview = LLFloaterReg::findTypedInstance<LLPreviewLSL>("preview_script", LLSD(itemId));
    if (preview)
    {
        // Bytecode save completed
        if (response["compiled"])
        {
            preview->callbackLSLCompileSucceeded();
        }
        else
        {
            preview->callbackLSLCompileFailed(response["errors"]);
        }
        preview->sendCompileResults(response);
    }
}

bool LLPreviewLSL::failedLSLUpload(LLUUID itemId, LLUUID taskId, LLSD response, std::string reason)
{
    LLSD floater_key;
    if (taskId.notNull())
    {
        floater_key["taskid"] = taskId;
        floater_key["itemid"] = itemId;
    }
    else
    {
        floater_key = LLSD(itemId);
    }

    LLPreviewLSL* preview = LLFloaterReg::findTypedInstance<LLPreviewLSL>("preview_script", floater_key);
    if (preview)
    {
        // unfreeze floater
        LLSD errors;
        errors.append(LLTrans::getString("UploadFailed") + reason);
        preview->callbackLSLCompileFailed(errors);

        LLSD message;
        message["compiled"] = false;
        message["errors"]   = errors;
        preview->sendCompileResults(message);

        return true;
    }

    return false;
}

// Save needs to compile the text in the buffer. If the compile
// succeeds, then save both assets out to the database. If the compile
// fails, go ahead and save the text anyway.
void LLPreviewLSL::saveIfNeeded(bool sync /*= true*/)
{
//  if(!mScriptEd->hasChanged())
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2012-02-10 (Catznip-3.2.1) | Added: Catznip-3.2.1
    if ( (!mScriptEd->hasChanged()) || (!gAgent.getRegion()) )
// [/SL:KB]
    {
        return;
    }

    mPendingUploads = 0;
    // <FS> FIRE-10172: Fix LSL editor error display
    //mScriptEd->mErrorList->deleteAllItems();
    mScriptEd->mEditor->makePristine();


    if (sync)
    {
        mScriptEd->sync();
    }

    if (!gAgent.getRegion()) return;
    const LLInventoryItem *inv_item = getItem();
    // save it out to asset server
    std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");

    // NaCL - LSL Preprocessor
    mScriptEd->enableSave(false); // Clear the enable save flag (FIRE-10173)
    bool domono = gSavedSettings.getBOOL("FSSaveInventoryScriptsAsMono");
    if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && !mScriptEd->mEditor->getIsLuauLanguage())
    {
        bool mono_directive = FSLSLPreprocessor::mono_directive(mScriptEd->getScriptText(), domono);

        if (mono_directive != domono)
        {
            std::string message;
            if (mono_directive)
            {
                message = LLTrans::getString("fs_preprocessor_mono_directive_override");
            }
            else
            {
                message = LLTrans::getString("fs_preprocessor_lsl2_directive_override");
            }
            domono = mono_directive;
            mScriptEd->mErrorList->addCommentText(message);
        }
    }
    // NaCl End

    if(inv_item)
    {
        getWindow()->incBusyCount();
        mPendingUploads++;
        if (!url.empty())
        {
            std::string compile_target(mScriptEd->mCompileTarget->getValue());
            //<FS:KC> Script Preprocessor
            // std::string buffer(mScriptEd->mEditor->getText());
            std::string buffer(mScriptEd->getScriptText());
            if (compile_target == "lsl2" || compile_target == "mono")
                compile_target = domono ? "mono" : "lsl2";
            //</FS:KC> Script Preprocessor

            LLUUID old_asset_id = inv_item->getAssetUUID().isNull() ? mScriptEd->getAssetID() : inv_item->getAssetUUID();

            LLResourceUploadInfo::ptr_t uploadInfo(std::make_shared<LLScriptAssetUpload>(mItemUUID, compile_target, buffer,
                [old_asset_id](LLUUID itemId, LLUUID, LLUUID, LLSD response) {
                    LLFileSystem::removeFile(old_asset_id, LLAssetType::AT_LSL_TEXT);
                    LLPreviewLSL::finishedLSLUpload(itemId, response);
                },
            LLPreviewLSL::failedLSLUpload));

            LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);
        }
    }
}

void LLPreviewLSL::onCompileTargetChanged()
{
    bool is_lua = (mScriptEd->mCompileTarget->getValue().asString() == "luau");
    mScriptEd->mEditor->setLuauLanguage(is_lua);
    mScriptEd->processKeywords(is_lua);
    mScriptEd->updateTabs(is_lua);      // <FS:Zi> Disable LSL preprocessor when compile target is luau
}

// static
void LLPreviewLSL::onLoadComplete(const LLUUID& asset_uuid, LLAssetType::EType type,
                                  void* user_data, S32 status, LLExtStat ext_status)
{
    LL_DEBUGS() << "LLPreviewLSL::onLoadComplete: got uuid " << asset_uuid
         << LL_ENDL;
    LLUUID* item_uuid = (LLUUID*)user_data;
    LLPreviewLSL* preview = LLFloaterReg::findTypedInstance<LLPreviewLSL>("preview_script", *item_uuid);
    if( preview )
    {
        if(0 == status)
        {
            LLFileSystem file(asset_uuid, type);
            S32 file_length = file.getSize();

            std::vector<char> buffer(file_length+1);
            file.read((U8*)&buffer[0], file_length);

            // put a EOS at the end
            buffer[file_length] = 0;
            preview->mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), true);
            preview->mScriptEd->mEditor->makePristine();

            std::string script_name = DEFAULT_SCRIPT_NAME;
            LLInventoryItem* item = gInventory.getItem(*item_uuid);
            bool is_modifiable = false;
            std::string compile_target;
            bool is_lua = false;
            if (item)
            {
                if (!item->getName().empty())
                {
                    script_name = item->getName();
                }
                if (gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE))
                {
                    is_modifiable = true;
                }

                // subtype is the language that the script is written in,
                // runtime is the VM that the script is running on.
                // LSL scripts have 3 possible runtimes: "lsl2", "mono", and "luau".
                // Lua scripts have 1 runtime: "luau".
                U8          subtype = item->getInventorySubType();
                std::string runtime = item->getRuntime();

                is_lua = (subtype == SST_LUA);
                if (!is_lua && (runtime == "luau"))
                {   // an lsl script executing on the luau runtime
                    compile_target = "lsl-luau";
                }
                else
                {   // otherwise, use the runtime as the compile target
                    // (this may be blank if the script has never been compiled)
                    compile_target = runtime;
                }
            }

            if (compile_target.empty())
            {
                if (!is_lua)
                {
                    // No language info from inventory metadata; fall back to content analysis.
                    is_lua = is_lua_script(std::string(&buffer[0], file_length));
                }
                compile_target = is_lua ? "luau" : "mono";
            }

            preview->mScriptEd->setScriptName(script_name);
            preview->mScriptEd->setEnableEditing(is_modifiable);
            preview->mScriptEd->setAssetID(asset_uuid);
            preview->mAssetStatus = PREVIEW_ASSET_LOADED;

            preview->mScriptEd->mCompileTarget->setValue(compile_target);
            preview->mScriptEd->mEditor->setLuauLanguage(is_lua);
            preview->mScriptEd->processKeywords(is_lua);
            preview->mScriptEd->updateTabs(is_lua);      // <FS:Zi> Disable LSL preprocessor when compile target is luau

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
            // Start the timer which will perform regular backup saves
            preview->mBackupTimer = setupCallbackTimer(60.0f, boost::bind(&LLPreviewLSL::onBackupTimer, preview));
// [/SL:KB]
        }
        else
        {
            if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
                LL_ERR_FILE_EMPTY == status)
            {
                LLNotificationsUtil::add("ScriptMissing");
            }
            else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
            {
                LLNotificationsUtil::add("ScriptNoPermissions");
            }
            else
            {
                LLNotificationsUtil::add("UnableToLoadScript");
            }

            preview->mAssetStatus = PREVIEW_ASSET_ERROR;
            LL_WARNS() << "Problem loading script: " << status << LL_ENDL;
        }
    }
    delete item_uuid;
}


/// ---------------------------------------------------------------------------
/// LLLiveLSLEditor
/// ---------------------------------------------------------------------------


//static
void* LLLiveLSLEditor::createScriptEdPanel(void* userdata)
{
    LLLiveLSLEditor *self = (LLLiveLSLEditor*)userdata;

    self->mScriptEd =  new LLScriptEdCore(
                                   self,
                                   std::string(),
                                   self->getHandle(),
                                   &LLLiveLSLEditor::onLoad,
                                   &LLLiveLSLEditor::onSave,
                                   &LLLiveLSLEditor::onSearchReplace,
                                   self,
                                   true,
                                   0);
    return self->mScriptEd;
}


LLLiveLSLEditor::LLLiveLSLEditor(const LLSD& key) :
    LLScriptEdContainer(key),
    mAskedForRunningInfo(false),
    mHaveRunningInfo(false),
    mCloseAfterSave(false),
    mPendingUploads(0),
    mIsModifiable(false),
    mIsNew(false),
    mIsSaving(false),
    mObjectName("")
{
    mFactoryMap["script ed panel"] = LLCallbackMap(LLLiveLSLEditor::createScriptEdPanel, this);
}

bool LLLiveLSLEditor::postBuild()
{
    mResetButton = getChild<LLButton>("Reset");
    mResetButton->setClickedCallback([&](LLUICtrl*, const LLSD&) { onReset(); });

    mRunningCheckbox = getChild<LLCheckBoxCtrl>("running");
    mRunningCheckbox->setCommitCallback([&](LLUICtrl*, const LLSD&) { onRunningCheckboxClicked(); });

    mExperiences = getChild<LLComboBox>("Experiences...");
    mExperiences->setCommitCallback([&](LLUICtrl*, const LLSD&) { experienceChanged(); });

    mExperienceEnabled = getChild<LLCheckBoxCtrl>("enable_xp");
    mExperienceEnabled->setCommitCallback([&](LLUICtrl*, const LLSD&) { onToggleExperience(); });

    mViewProfileButton = getChild<LLButton>("view_profile");
    mViewProfileButton->setClickedCallback([&](LLUICtrl*, const LLSD&) { onViewProfile(); });

    mScriptEd->mEditor->makePristine();
    mScriptEd->mEditor->setFocus(true);

    mScriptEd->mCompileTarget = getChild<LLComboBox>("compile_target");
    mScriptEd->mCompileTarget->setCommitCallback([&](LLUICtrl*, const LLSD&) { onCompileTargetChanged(); });

    return LLPreview::postBuild();
}

// virtual
void LLLiveLSLEditor::callbackLSLCompileSucceeded(const LLUUID& task_id,
                                                  const LLUUID& item_id,
                                                  bool is_script_running)
{
    LL_DEBUGS() << "LSL Bytecode saved" << LL_ENDL;
    // <FS> Append comment text
    //mScriptEd->mErrorList->setCommentText(LLTrans::getString("CompileSuccessful"));
    //mScriptEd->mErrorList->setCommentText(LLTrans::getString("SaveComplete"));
    mScriptEd->mErrorList->addCommentText(LLTrans::getString("CompileSuccessful"));
    mScriptEd->mErrorList->addCommentText(LLTrans::getString("SaveComplete"));
    // </FS>

    mScriptEd->updateIndicators(false, true); //<FS:Kadah> Compile Indicators

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    // Script was successfully saved so delete our backup copy if we have one and the editor is still pristine
    if ( (!mBackupFilename.empty()) && (!mScriptEd->hasChanged()) )
    {
        LLFile::remove(mBackupFilename);
        mBackupFilename.clear();
    }
// [/SL:KB]


    if (isOpenInExternalEditor())
    {
        LLSD success_msg;
        success_msg.append(LLTrans::getString("CompileSuccessful"));
        success_msg.append(LLTrans::getString("SaveComplete"));
        logErrorsToFile(success_msg);
    }

    mRunningCheckbox->set(is_script_running);
    mIsSaving = false;
    closeIfNeeded();
}

// virtual
void LLLiveLSLEditor::callbackLSLCompileFailed(const LLSD& compile_errors)
{
    LL_DEBUGS() << "Compile failed!" << LL_ENDL;

    for(LLSD::array_const_iterator line = compile_errors.beginArray();
        line < compile_errors.endArray();
        line++)
    {
        LLSD row;
        std::string error_message = line->asString();
        LLStringUtil::stripNonprintable(error_message);
        row["columns"][0]["value"] = error_message;
        // *TODO: change to "MONOSPACE" and change llfontgl.cpp?
        row["columns"][0]["font"] = "OCRA";
        mScriptEd->mErrorList->addElement(row);
    }

    if (isOpenInExternalEditor())
    {
        logErrorsToFile(compile_errors);
    }

    mScriptEd->selectFirstError();

    mScriptEd->updateIndicators(false, false); //<FS:Kadah> Compile Indicators

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
    // Script was successfully saved so delete our backup copy if we have one and the editor is still pristine
    if ( (!mBackupFilename.empty()) && (!mScriptEd->hasChanged()) )
    {
        LLFile::remove(mBackupFilename);
        mBackupFilename.clear();
    }
// [/SL:KB]

    mIsSaving = false;
    closeIfNeeded();
}

void LLLiveLSLEditor::loadAsset()
{
    //LL_INFOS() << "LLLiveLSLEditor::loadAsset()" << LL_ENDL;
    if(!mIsNew)
    {
        LLViewerObject* object = gObjectList.findObject(mObjectUUID);
        if(object)
        {
            LLViewerInventoryItem* item = dynamic_cast<LLViewerInventoryItem*>(object->getInventoryObject(mItemUUID));

            if(item)
            {
                LLViewerRegion* region = object->getRegion();
                std::string url = std::string();
                if(region)
                {
                    url = region->getCapability("GetMetadata");
                }
                LLExperienceCache::instance().fetchAssociatedExperience(item->getParentUUID(), item->getUUID(), url,
                    boost::bind(&LLLiveLSLEditor::setAssociatedExperience, getDerivedHandle<LLLiveLSLEditor>(), _1));

                bool isGodlike = gAgent.isGodlike();
                bool copyManipulate = gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE);
                mIsModifiable = gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE);

                if(!isGodlike && (!copyManipulate || !mIsModifiable))
                {
                    mItem = new LLViewerInventoryItem(item);
                    mScriptEd->setScriptText(getString("not_allowed"), false);
                    mScriptEd->mEditor->makePristine();
                    mScriptEd->enableSave(false);
                    mAssetStatus = PREVIEW_ASSET_LOADED;
                }
                else if(copyManipulate || isGodlike)
                {
                    mItem = new LLViewerInventoryItem(item);
                    // request the text from the object
                    LLSD* user_data = new LLSD();
                    user_data->with("taskid", mObjectUUID).with("itemid", mItemUUID);
                    gAssetStorage->getInvItemAsset(
                        object->getRegion()->getHost(),
                        gAgent.getID(),
                        gAgent.getSessionID(),
                        item->getPermissions().getOwner(),
                        object->getID(),
                        item->getUUID(),
                        item->getAssetUUID(),
                        item->getType(),
                        LLLiveLSLEditor::onLoadComplete,
                        user_data,
                        true);
                    LLMessageSystem* msg = gMessageSystem;
                    msg->newMessageFast(_PREHASH_GetScriptRunning);
                    msg->nextBlockFast(_PREHASH_Script);
                    msg->addUUIDFast(_PREHASH_ObjectID, mObjectUUID);
                    msg->addUUIDFast(_PREHASH_ItemID, mItemUUID);
                    msg->sendReliable(object->getRegion()->getHost());
                    mAskedForRunningInfo = true;
                    mAssetStatus = PREVIEW_ASSET_LOADING;
                }
            }

            if(mItem.isNull())
            {
                mScriptEd->setScriptText(LLStringUtil::null, false);
                mScriptEd->mEditor->makePristine();
                mAssetStatus = PREVIEW_ASSET_LOADED;
                mIsModifiable = false;
            }

            refreshFromItem();
            // <FS:Ansariel> Make ugly location display better
            //getChild<LLUICtrl>("obj_name")->setValue(mObjectName);
            getChild<LLUICtrl>("obj_name")->setTextArg("[SOURCE_OBJECT]", LLStringExplicit(mObjectName) );
            // </FS:Ansariel>
            // This is commented out, because we don't completely
            // handle script exports yet.
            /*
            // request the exports from the object
            gMessageSystem->newMessage("GetScriptExports");
            gMessageSystem->nextBlock("ScriptBlock");
            gMessageSystem->addUUID("AgentID", gAgent.getID());
            U32 local_id = object->getLocalID();
            gMessageSystem->addData("LocalID", &local_id);
            gMessageSystem->addUUID("ItemID", mItemUUID);
            LLHost host(object->getRegion()->getIP(),
                        object->getRegion()->getPort());
            gMessageSystem->sendReliable(host);
            */
        }
    }

    requestExperiences();
}

// static
void LLLiveLSLEditor::onLoadComplete(const LLUUID& asset_id,
                                     LLAssetType::EType type,
                                     void* user_data, S32 status, LLExtStat ext_status)
{
    LL_DEBUGS() << "LLLiveLSLEditor::onLoadComplete: got uuid " << asset_id
         << LL_ENDL;
    LLSD* floater_key = (LLSD*)user_data;

    LLLiveLSLEditor* instance = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", *floater_key);

    if(instance )
    {
        if( LL_ERR_NOERR == status )
        {
            // <FS:ND> FIRE-15524 opt to not crash if the item went away ....
            if( !instance->getItem() )
            {
                LL_WARNS() << "getItem() returns 0, item went away while loading script()" << LL_ENDL;
                instance->mAssetStatus = PREVIEW_ASSET_ERROR;
                delete floater_key;
                return;
            }
            // </FS:ND>

            instance->loadScriptText(asset_id, type);
            instance->mScriptEd->setEnableEditing(true);
            instance->mAssetStatus = PREVIEW_ASSET_LOADED;
            instance->mScriptEd->setAssetID(asset_id);

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
            // Start the timer which will perform regular backup saves
            instance->mBackupTimer = setupCallbackTimer(60.0f, boost::bind(&LLLiveLSLEditor::onBackupTimer, instance));
// [/SL:KB]
        }
        else
        {
            if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
                LL_ERR_FILE_EMPTY == status)
            {
                LLNotificationsUtil::add("ScriptMissing");
            }
            else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
            {
                LLNotificationsUtil::add("ScriptNoPermissions");
            }
            else
            {
                LLNotificationsUtil::add("UnableToLoadScript");
            }
            instance->mAssetStatus = PREVIEW_ASSET_ERROR;
        }
    }

    delete floater_key;
}

void LLLiveLSLEditor::loadScriptText(const LLUUID &uuid, LLAssetType::EType type)
{
    LLFileSystem file(uuid, type);
    S32 file_length = file.getSize();
    std::vector<char> buffer(file_length + 1);
    file.read((U8*)&buffer[0], file_length);

    if (file.getLastBytesRead() != file_length ||
        file_length <= 0)
    {
        LL_WARNS() << "Error reading " << uuid << ":" << type << LL_ENDL;
    }

    buffer[file_length] = '\0';

    mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), true);
    mScriptEd->makeEditorPristine();

    std::string script_name = DEFAULT_SCRIPT_NAME;
    const LLInventoryItem* inv_item = getItem();

    if(inv_item)
    {
        script_name = inv_item->getName();
    }
    mScriptEd->setScriptName(script_name);

    // Detect language from inventory metadata, with content-based fallback.
    bool is_lua = false;
    std::string compile_target;
    if (inv_item)
    {
        U8 subtype          = inv_item->getInventorySubType();
        std::string runtime = inv_item->getRuntime();
        is_lua = (subtype == SST_LUA);
        if (!is_lua && (runtime == "luau"))
            compile_target = "lsl-luau";
        else
            compile_target = runtime;
    }
    if (compile_target.empty())
    {
        if (!is_lua)
            is_lua = is_lua_script(std::string(&buffer[0], file_length));
        compile_target = is_lua ? "luau" : "mono";
    }
    mScriptEd->mCompileTarget->setValue(compile_target);
    mScriptEd->mEditor->setLuauLanguage(is_lua);
    mScriptEd->processKeywords(is_lua);
    mScriptEd->updateTabs(is_lua);      // <FS:Zi> Disable LSL preprocessor when compile target is luau
}


void LLLiveLSLEditor::onRunningCheckboxClicked()
{
    if (LLViewerObject* object = gObjectList.findObject(mObjectUUID))
    {
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
        if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
        {
            RlvUtil::notifyBlockedGeneric();
            return;
        }
// [/RLVa:KB]

        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_SetScriptRunning);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
        msg->nextBlockFast(_PREHASH_Script);
        msg->addUUIDFast(_PREHASH_ObjectID, mObjectUUID);
        msg->addUUIDFast(_PREHASH_ItemID, mItemUUID);
        msg->addBOOLFast(_PREHASH_Running, mRunningCheckbox->get());
        msg->sendReliable(object->getRegion()->getHost());
    }
    else
    {
        mRunningCheckbox->set(!mRunningCheckbox->get());
        LLNotificationsUtil::add("CouldNotStartStopScript");
    }
}

void LLLiveLSLEditor::onReset()
{
    if (LLViewerObject* object = gObjectList.findObject(mObjectUUID))
    {
// [RLVa:KB] - Checked: 2010-09-28 (RLVa-1.2.1f) | Modified: RLVa-1.0.5a
        if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
        {
            RlvUtil::notifyBlockedGeneric();
            return;
        }
// [/RLVa:KB]

        LLMessageSystem* msg = gMessageSystem;
        msg->newMessageFast(_PREHASH_ScriptReset);
        msg->nextBlockFast(_PREHASH_AgentData);
        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
        msg->nextBlockFast(_PREHASH_Script);
        msg->addUUIDFast(_PREHASH_ObjectID, mObjectUUID);
        msg->addUUIDFast(_PREHASH_ItemID, mItemUUID);
        msg->sendReliable(object->getRegion()->getHost());
    }
    else
    {
        LLNotificationsUtil::add("CouldNotStartStopScript");
    }
}

void LLLiveLSLEditor::draw()
{
    LLViewerObject* object = gObjectList.findObject(mObjectUUID);
    if (object && mAskedForRunningInfo && mHaveRunningInfo)
    {
        if (object->permAnyOwner())
        {
            mRunningCheckbox->setLabel(getString("script_running"));
            mRunningCheckbox->setEnabled(!mIsSaving);
        }
        else
        {
            mRunningCheckbox->setLabel(getString("public_objects_can_not_run"));
            mRunningCheckbox->setEnabled(false);

            // *FIX: Set it to false so that the ui is correct for
            // a box that is released to public. It could be
            // incorrect after a release/claim cycle, but will be
            // correct after clicking on it.
            mRunningCheckbox->set(false);
            mScriptEd->mCompileTarget->clear();
        }
    }
    else if (!object)
    {
        // HACK: Display this information in the title bar.
        // Really ought to put in main window.
        setTitle(LLTrans::getString("ObjectOutOfRange"));
        mRunningCheckbox->setEnabled(false);
        mScriptEd->mCompileTarget->setEnabled(false);
        // object may have fallen out of range.
        mHaveRunningInfo = false;
    }

    LLPreview::draw();
}


void LLLiveLSLEditor::onSearchReplace(void* userdata)
{
    LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;

    LLScriptEdCore* sec = self->mScriptEd;
//  LLFloaterScriptSearch::show(sec);
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
    LLFloaterSearchReplace* floater = LLFloaterSearchReplace::show(sec->mCurrentEditor);
    floater->setCanReplace(sec->mCurrentEditor == sec->mEditor);
// [/SL:KB]
}

struct LLLiveLSLSaveData
{
    LLLiveLSLSaveData(const LLUUID& id, const LLViewerInventoryItem* item, bool active);
    LLUUID mSaveObjectID;
    LLPointer<LLViewerInventoryItem> mItem;
    bool mActive;
};

LLLiveLSLSaveData::LLLiveLSLSaveData(const LLUUID& id,
                                     const LLViewerInventoryItem* item,
                                     bool active) :
    mSaveObjectID(id),
    mActive(active)
{
    llassert(item);
    mItem = new LLViewerInventoryItem(item);
}

/*static*/
void LLLiveLSLEditor::finishLSLUpload(LLUUID itemId, LLUUID taskId, LLUUID newAssetId, LLSD response, bool isRunning)
{
    LLSD floater_key;
    floater_key["taskid"] = taskId;
    floater_key["itemid"] = itemId;

    LLLiveLSLEditor* preview = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", floater_key);
    if (preview)
    {
        preview->mItem->setAssetUUID(newAssetId);
        preview->mScriptEd->setAssetID(newAssetId);

        // Bytecode save completed
        if (response["compiled"])
        {
            preview->callbackLSLCompileSucceeded(taskId, itemId, isRunning);
        }
        else
        {
            preview->callbackLSLCompileFailed(response["errors"]);
        }
        response["is_running"] = isRunning;
        preview->sendCompileResults(response);
    }
}

// virtual
void LLLiveLSLEditor::saveIfNeeded(bool sync /*= true*/)
{
    LLViewerObject* object = gObjectList.findObject(mObjectUUID);
    if (!object)
    {
        LLNotificationsUtil::add("SaveScriptFailObjectNotFound");
        return;
    }

    if (mItem.isNull() || !mItem->isFinished())
    {
        // $NOTE: While the error message may not be exactly correct,
        // it's pretty close.
        LLNotificationsUtil::add("SaveScriptFailObjectNotFound");
        return;
    }

// [RLVa:KB] - Checked: 2010-11-25 (RLVa-1.2.2b) | Modified: RLVa-1.2.2b
    if ( (rlv_handler_t::isEnabled()) && (gRlvAttachmentLocks.isLockedAttachment(object->getRootEdit())) )
    {
        RlvUtil::notifyBlockedGeneric();
        return;
    }
// [/RLVa:KB]

    // get the latest info about it. We used to be losing the script
    // name on save, because the viewer object version of the item,
    // and the editor version would get out of synch. Here's a good
    // place to synch them back up.
    LLInventoryItem* inv_item = dynamic_cast<LLInventoryItem*>(object->getInventoryObject(mItemUUID));
    if (inv_item)
    {
        mItem->copyItem(inv_item);
    }

    // Don't need to save if we're pristine
    if (!mScriptEd->hasChanged())
    {
        return;
    }

    mPendingUploads = 0;

    // save the script
    mScriptEd->enableSave(false);
    mScriptEd->mEditor->makePristine();
    // <FS> FIRE-10172: Fix LSL editor error display
    //mScriptEd->mErrorList->deleteAllItems();
    mScriptEd->mEditor->makePristine();

    if (sync)
    {
        mScriptEd->sync();
    }

    bool is_running = mRunningCheckbox->get();
    mIsSaving = true;
    getWindow()->incBusyCount();
    mPendingUploads++;

    std::string url = object->getRegion()->getCapability("UpdateScriptTask");

    if (!url.empty())
    {
        std::string compile_target(mScriptEd->mCompileTarget->getValue());
        //<FS:KC> Script Preprocessor
        // std::string buffer(mScriptEd->mEditor->getText());
        std::string buffer(mScriptEd->getScriptText());
        //</FS:KC> Script Preprocessor
        LLUUID old_asset_id = mScriptEd->getAssetID();

        LLResourceUploadInfo::ptr_t uploadInfo(std::make_shared<LLScriptAssetUpload>(mObjectUUID, mItemUUID,
            compile_target, is_running, mScriptEd->getAssociatedExperience(), buffer,
            [is_running, old_asset_id](LLUUID item_id, LLUUID task_id, LLUUID new_asset_id, LLSD response)
            {
                LLFileSystem::removeFile(old_asset_id, LLAssetType::AT_LSL_TEXT);
                LLLiveLSLEditor::finishLSLUpload(item_id, task_id, new_asset_id, response, is_running);
            },
            nullptr)); // needs failure handling?

        LLViewerAssetUpload::EnqueueInventoryUpload(url, uploadInfo);
    }
}

bool LLLiveLSLEditor::canClose()
{
    return mScriptEd->canClose();
}

void LLLiveLSLEditor::closeIfNeeded()
{
    getWindow()->decBusyCount();
    mPendingUploads--;
    if ((mPendingUploads <= 0) && mCloseAfterSave)
    {
        closeFloater();
    }
}

// static
void LLLiveLSLEditor::onLoad(void* userdata)
{
    LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
    self->loadAsset();
}

// static
// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
//void LLLiveLSLEditor::onSave(void* userdata, bool close_after_save)
void LLLiveLSLEditor::onSave(void* userdata, bool close_after_save, bool sync)
// </FS:Ansariel>
{
    if (LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata)
    {
        self->mCloseAfterSave = close_after_save;
        // <FS:Ansariel> Commented out because we fixed errors differently
        //self->mScriptEd->mErrorList->setCommentText("");
        // <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
        //self->saveIfNeeded();
        self->saveIfNeeded(sync);
        // </FS:Ansariel>
    }
}

// static
void LLLiveLSLEditor::processScriptRunningReply(LLMessageSystem* msg, void**)
{
    LLUUID item_id;
    LLUUID object_id;
    msg->getUUIDFast(_PREHASH_Script, _PREHASH_ObjectID, object_id);
    msg->getUUIDFast(_PREHASH_Script, _PREHASH_ItemID, item_id);

    LLSD floater_key;
    floater_key["taskid"] = object_id;
    floater_key["itemid"] = item_id;
    if (LLLiveLSLEditor* instance = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", floater_key))
    {
        instance->mHaveRunningInfo = true;

        bool running;
        msg->getBOOLFast(_PREHASH_Script, _PREHASH_Running, running);
        instance->mRunningCheckbox->set(running);

        bool mono = false, luau = false, luau_language = false;
        msg->getBOOLFast(_PREHASH_Script, _PREHASH_Mono, mono);
        msg->getBOOLFast(_PREHASH_Script, _PREHASH_Luau, luau); // Luau compiler is enabled
        msg->getBOOLFast(_PREHASH_Script, _PREHASH_LuauLanguage, luau_language);

        std::string compile_target;
        if (luau)
        {
            if (luau_language)
            {
                compile_target = "luau";
            }
            else
            {
                compile_target = "lsl-luau"; // Luau compiler running in LSL compatibility mode
            }
        }
        else if (mono)
        {
            compile_target = "mono";
        }
        else
        {
            compile_target = "lsl2";
        }

        instance->mScriptEd->mCompileTarget->setValue(compile_target);
        instance->mScriptEd->processKeywords(luau && luau_language); // Use Luau syntax highlighting for Luau scripts
        instance->mScriptEd->updateTabs(luau && luau_language);      // <FS:Zi> Disable LSL preprocessor when compile target is luau

        bool lua_scripts_enabled = have_lua_enabled(object_id);
        if (LLScrollListItem* luau_item = instance->mScriptEd->mCompileTarget->findItemByValue("luau"))
        {
            luau_item->setEnabled(lua_scripts_enabled);
        }
        if (LLScrollListItem* lsl_luau_item = instance->mScriptEd->mCompileTarget->findItemByValue("lsl-luau"))
        {
            lsl_luau_item->setEnabled(lua_scripts_enabled);
        }

        instance->mScriptEd->mCompileTarget->setEnabled(instance->getIsModifiable() && have_script_upload_cap(object_id));
    }
}

void LLLiveLSLEditor::onCompileTargetChanged()
{
    mScriptEd->mCompileTarget->setEnabled(have_script_upload_cap(mObjectUUID));
    mScriptEd->enableSave(getIsModifiable());
    bool is_lua = mScriptEd->mCompileTarget->getValue().asString() == "luau";
    mScriptEd->mEditor->setLuauLanguage(is_lua);
    mScriptEd->processKeywords(is_lua);
    mScriptEd->updateTabs(is_lua);      // <FS:Zi> Disable LSL preprocessor when compile target is luau
}

void LLLiveLSLEditor::setAssociatedExperience( LLHandle<LLLiveLSLEditor> editor, const LLSD& experience )
{
    if (LLLiveLSLEditor* scriptEd = editor.get())
    {
        LLUUID id;
        if (experience.has(LLExperienceCache::EXPERIENCE_ID))
        {
            id=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
        }
        scriptEd->mScriptEd->setAssociatedExperience(id);
        scriptEd->updateExperiencePanel();
    }
}

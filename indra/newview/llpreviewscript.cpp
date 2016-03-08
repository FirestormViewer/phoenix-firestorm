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
#include "llassetuploadresponders.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llenvmanager.h"
#include "llexternaleditor.h"
#include "llfilepicker.h"
#include "llfloaterreg.h"
#include "llfloatersearchreplace.h"
#include "llinventorydefines.h"
#include "llinventorymodel.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "lllivefile.h"
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
#include "llvfile.h"

#include "llagent.h"
#include "llmenugl.h"
#include "roles_constants.h"
#include "llselectmgr.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llkeyboard.h"
#include "llscrollcontainer.h"
#include "llcheckboxctrl.h"
#include "llscripteditor.h"
#include "llselectmgr.h"
#include "lltooldraganddrop.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "llslider.h"
#include "lldir.h"
#include "llcombobox.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "lluictrlfactory.h"
#include "llmediactrl.h"
#include "lluictrlfactory.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llappviewer.h"
#include "llfloatergotoline.h"
#include "llexperiencecache.h"
#include "llfloaterexperienceprofile.h"
#include "llexperienceassociationresponder.h"
#include "llloadingindicator.h" // <FS:Kadah> Compile indicator
#include "lliconctrl.h" // <FS:Kadah> Compile indicator
// [RLVa:KB] - Checked: 2011-05-22 (RLVa-1.3.1a)
#include "rlvhandler.h"
#include "rlvlocks.h"
// [/RLVa:KB]
#include "fsscriptlibrary.h"

// NaCl - LSL Preprocessor
#include "fslslpreproc.h"
// NaCl End
#ifdef OPENSIM
#include "llviewernetwork.h"	// for Grid manager
#endif // OPENSIM

const std::string HELLO_LSL =
	"default\n"
	"{\n"
	"	state_entry()\n"
	"	{\n"
	"		llSay(0, \"Hello, Avatar!\");\n"
	"	}\n"
	"\n"
	"	touch_start(integer total_number)\n"
	"	{\n"
	"		llSay(0, \"Touched.\");\n"
	"	}\n"
	"}\n";
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


class ExperienceResponder : public LLHTTPClient::Responder
{
public:
	ExperienceResponder(const LLHandle<LLLiveLSLEditor>& parent):mParent(parent)
	{
	}

	LLHandle<LLLiveLSLEditor> mParent;

	/*virtual*/ void httpSuccess()
	{
		LLLiveLSLEditor* parent = mParent.get();
		if(!parent)
			return;

		parent->setExperienceIds(getContent()["experience_ids"]);		
	}
};

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
#include "lleventtimer.h"

/// ---------------------------------------------------------------------------
/// Timer helper class
/// ---------------------------------------------------------------------------
class LLCallbackTimer : public LLEventTimer
{
public:
	typedef boost::function<bool()> bool_func_t;
public:
	LLCallbackTimer(F32 nPeriod, bool_func_t cb) : LLEventTimer(nPeriod), m_Callback(cb) {}
	/*virtual*/ BOOL tick() { return m_Callback(); }
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
class LLLiveLSLFile : public LLLiveFile
{
public:
	typedef boost::function<bool (const std::string& filename)> change_callback_t;

	LLLiveLSLFile(std::string file_path, change_callback_t change_cb);
	~LLLiveLSLFile();

	void ignoreNextUpdate() { mIgnoreNextUpdate = true; }

protected:
	/*virtual*/ bool loadFile();

	change_callback_t	mOnChangeCallback;
	bool				mIgnoreNextUpdate;
};

LLLiveLSLFile::LLLiveLSLFile(std::string file_path, change_callback_t change_cb)
:	mOnChangeCallback(change_cb)
,	mIgnoreNextUpdate(false)
,	LLLiveFile(file_path, 1.0f)
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

	/*virtual*/	BOOL	postBuild();
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
	virtual BOOL handleKeyHere(KEY key, MASK mask);

private:

	LLScriptEdCore* mEditorCore;
	static LLFloaterScriptSearch*	sInstance;

protected:
	LLLineEditor*			mSearchBox;
	LLLineEditor*			mReplaceBox;
		void onSearchBoxCommit();
};

LLFloaterScriptSearch* LLFloaterScriptSearch::sInstance = NULL;

LLFloaterScriptSearch::LLFloaterScriptSearch(LLScriptEdCore* editor_core)
:	LLFloater(LLSD()),
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

BOOL LLFloaterScriptSearch::postBuild()
{
	mReplaceBox = getChild<LLLineEditor>("replace_text");
	mSearchBox = getChild<LLLineEditor>("search_text");
	mSearchBox->setCommitCallback(boost::bind(&LLFloaterScriptSearch::onSearchBoxCommit, this));
	mSearchBox->setCommitOnFocusLost(FALSE);
	childSetAction("search_btn", onBtnSearch,this);
	childSetAction("replace_btn", onBtnReplace,this);
	childSetAction("replace_all_btn", onBtnReplaceAll,this);

	setDefaultBtn("search_btn");

	return TRUE;
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
	return FALSE;
}

BOOL LLFloaterScriptSearch::handleKeyHere(KEY key, MASK mask)
{
	if (mEditorCore)
	{
		BOOL handled = mEditorCore->handleKeyHere(key, mask);
		if (!handled)
		{
			LLFloater::handleKeyHere(key, mask);
		}
	}

	return FALSE;
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
	void (*load_callback)(void*),
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//void (*save_callback)(void*, BOOL),
	void (*save_callback)(void*, BOOL, bool),
	// </FS:Ansariel>
	void (*search_replace_callback) (void* userdata),
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
	mForceClose( FALSE ),
	mLastHelpToken(NULL),
	mLiveHelpHistorySize(0),
	mEnableSave(FALSE),
	mLiveFile(NULL),
	mLive(live),
	mContainer(container),
	// <FS:CR> FIRE-10606, patch by Sei Lisa
	mLSLProc(NULL),
	mPostEditor(NULL),
	// </FS:CR>
	mCompiling(false), //<FS:KC> Compile indicators, recompile button
	mHasScriptData(FALSE)
{
	setFollowsAll();
	setBorderVisible(FALSE);

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
//	LLFloaterScriptSearch* script_search = LLFloaterScriptSearch::getInstance();
//	if (script_search && script_search->getEditorCore() == this)
//	{
//		script_search->closeFloater();
//		delete script_search;
//	}
	// <FS:Sei> FIRE-16042: Warn when preproc is toggled.
	if (mTogglePreprocConnection.connected())
	{
		mTogglePreprocConnection.disconnect();
	}
	// </FS:Sei>

	delete mLiveFile;
	if (mSyntaxIDConnection.connected())
	{
		mSyntaxIDConnection.disconnect();
	}

	// NaCl - Script Preprocessor
	delete mLSLProc;
}

void LLLiveLSLEditor::experienceChanged()
{
	if(mScriptEd->getAssociatedExperience() != mExperiences->getSelectedValue().asUUID())
	{
		mScriptEd->enableSave(getIsModifiable());
		//getChildView("Save_btn")->setEnabled(TRUE);
		mScriptEd->setAssociatedExperience(mExperiences->getSelectedValue().asUUID());
		updateExperiencePanel();
	}
}

void LLLiveLSLEditor::onViewProfile( LLUICtrl *ui, void* userdata )
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;

	LLUUID id;
	if(self->mExperienceEnabled->get())
	{
		id=self->mScriptEd->getAssociatedExperience();
		if(id.notNull())
		{
			 LLFloaterReg::showInstance("experience_profile", id, true);
		}
	}

}

void LLLiveLSLEditor::onToggleExperience( LLUICtrl *ui, void* userdata )
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;

	LLUUID id;
	if(self->mExperienceEnabled->get())
	{
		if(self->mScriptEd->getAssociatedExperience().isNull())
		{
			id=self->mExperienceIds.beginArray()->asUUID();
		}
	}

	if(id != self->mScriptEd->getAssociatedExperience())
	{
		self->mScriptEd->enableSave(self->getIsModifiable());
	}
	self->mScriptEd->setAssociatedExperience(id);

	self->updateExperiencePanel();
}

BOOL LLScriptEdCore::postBuild()
{
	mLineCol=getChild<LLTextBox>("line_col");
// <FS:CR> Advanced Script Editor
	//mSaveBtn=getChildView("Save_btn");
	mSaveBtn =	getChild<LLButton>("save_btn");
	mSaveBtn2 =	getChild<LLButton>("save_btn_2");	// <FS:Zi> support extra save button
	mCutBtn =	getChild<LLButton>("cut_btn");
	mCopyBtn =	getChild<LLButton>("copy_btn");
	mPasteBtn =	getChild<LLButton>("paste_btn");
	mUndoBtn =	getChild<LLButton>("undo_btn");
	mRedoBtn =	getChild<LLButton>("redo_btn");
	mSaveToDiskBtn = getChild<LLButton>("save_disk_btn");
	mLoadFromDiskBtn = getChild<LLButton>("load_disk_btn");
	mSearchBtn = getChild<LLButton>("search_btn");
// </FS:CR>

	mErrorList = getChild<LLScrollListCtrl>("lsl errors");

	mFunctions = getChild<LLComboBox>("Insert...");

	childSetCommitCallback("Insert...", &LLScriptEdCore::onBtnInsertFunction, this);

	mEditor = getChild<LLScriptEditor>("Script Editor");

	// NaCl - LSL Preprocessor
	if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor"))
	{
		mPostEditor = getChild<LLScriptEditor>("Post Editor");
		if (mPostEditor)
		{
			mPostEditor->setFollowsAll();
			mPostEditor->setEnabled(TRUE);
		}
	}
	// FIRE-16042: Warn when preproc is toggled.
	mTogglePreprocConnection = gSavedSettings.getControl("_NACL_LSLPreprocessor")->getSignal()
		->connect(boost::bind(&LLScriptEdCore::onToggleProc, this));
	// NaCl End

	childSetCommitCallback("lsl errors", &LLScriptEdCore::onErrorList, this);
// <FS:CR> Advanced Script Editor
	//childSetAction("Save_btn", boost::bind(&LLScriptEdCore::doSave,this,FALSE));
	childSetAction("prefs_btn", boost::bind(&LLScriptEdCore::onBtnPrefs, this));
// </FS:CR>
	childSetAction("Edit_btn", boost::bind(&LLScriptEdCore::openInExternalEditor, this));
	childSetAction("edit_btn_2", boost::bind(&LLScriptEdCore::openInExternalEditor, this));	// <FS:Zi> support extra edit button
	
	initMenu();
	initButtonBar();	// <FS:CR> Advanced Script Editor

	mSyntaxIDConnection = LLSyntaxIdLSL::getInstance()->addSyntaxIDCallback(boost::bind(&LLScriptEdCore::processKeywords, this));

	// Intialise keyword highlighting for the current simulator's version of LSL
	LLSyntaxIdLSL::getInstance()->initialize();
	processKeywords();

	return TRUE;
}

void LLScriptEdCore::processKeywords()
{
	LL_DEBUGS("SyntaxLSL") << "Processing keywords" << LL_ENDL;
	// <FS:Ansariel> FIRE-15906: Clear combobox values before adding new
	mFunctions->clearRows();
	mEditor->clearSegments();
	mEditor->initKeywords();
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
		mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_preproc.xml"), funcs, tooltips, color);
#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
		mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_ossl.xml"), funcs, tooltips, color);
	if (LLGridManager::getInstance()->isInAuroraSim())
		mEditor->loadKeywords(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "scriptlibrary_aa.xml"), funcs, tooltips, color);
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
	//menuItem->setClickCallback(boost::bind(&LLScriptEdCore::doSave, this, FALSE));
	menuItem->setClickCallback(boost::bind(&LLScriptEdCore::doSave, this, FALSE, true));
	// </FS:Ansariel>
	menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::hasChanged, this));
	
	menuItem = getChild<LLMenuItemCallGL>("Revert All Changes");
	menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnUndoChanges, this));
	menuItem->setEnableCallback(boost::bind(&LLScriptEdCore::hasChanged, this));

	menuItem = getChild<LLMenuItemCallGL>("Undo");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::undo, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canUndo, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Redo");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::redo, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canRedo, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Cut");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::cut, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canCut, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Copy");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::copy, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canCopy, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Paste");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::paste, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canPaste, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Select All");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::selectAll, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canSelectAll, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Deselect");
	menuItem->setClickCallback(boost::bind(&LLTextEditor::deselect, mEditor));
	menuItem->setEnableCallback(boost::bind(&LLTextEditor::canDeselect, mEditor));

	menuItem = getChild<LLMenuItemCallGL>("Search / Replace...");
//	menuItem->setClickCallback(boost::bind(&LLFloaterScriptSearch::show, this));
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	menuItem->setClickCallback(boost::bind(&LLFloaterSearchReplace::show, mEditor));
// [/SL:KB]

	menuItem = getChild<LLMenuItemCallGL>("Go to line...");
	menuItem->setClickCallback(boost::bind(&LLFloaterGotoLine::show, this));

	menuItem = getChild<LLMenuItemCallGL>("Help...");
	menuItem->setClickCallback(boost::bind(&LLScriptEdCore::onBtnHelp, this));

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
	mSaveBtn->setClickedCallback(boost::bind(&LLScriptEdCore::doSave, this, FALSE, true));
	mSaveBtn2->setClickedCallback(boost::bind(&LLScriptEdCore::doSave, this, FALSE, true));	// <FS:Zi> support extra save button
	mCutBtn->setClickedCallback(boost::bind(&LLTextEditor::cut, mEditor));
	mCopyBtn->setClickedCallback(boost::bind(&LLTextEditor::copy, mEditor));
	mPasteBtn->setClickedCallback(boost::bind(&LLTextEditor::paste, mEditor));
	mUndoBtn->setClickedCallback(boost::bind(&LLTextEditor::undo, mEditor));
	mRedoBtn->setClickedCallback(boost::bind(&LLTextEditor::redo, mEditor));
	mSaveToDiskBtn->setClickedCallback(boost::bind(&LLScriptEdCore::onBtnSaveToFile, this));
	mLoadFromDiskBtn->setClickedCallback(boost::bind(&LLScriptEdCore::onBtnLoadFromFile, this));
	mSearchBtn->setClickedCallback(boost::bind(&LLFloaterSearchReplace::show, mEditor));
}

void LLScriptEdCore::updateButtonBar()
{
	mSaveBtn->setEnabled(hasChanged());
	// mSaveBtn2->setEnabled(hasChanged());	// <FS:Zi> support extra save button
	mCutBtn->setEnabled(mEditor->canCut());
	mCopyBtn->setEnabled(mEditor->canCopy());
	mPasteBtn->setEnabled(mEditor->canPaste());
	mUndoBtn->setEnabled(mEditor->canUndo());
	mRedoBtn->setEnabled(mEditor->canRedo());
	mSaveToDiskBtn->setEnabled(mEditor->canLoadOrSaveToFile());
	mLoadFromDiskBtn->setEnabled(mEditor->canLoadOrSaveToFile());
	//<FS:Kadah> Recompile button
	static LLCachedControl<bool> FSScriptEditorRecompileButton(gSavedSettings, "FSScriptEditorRecompileButton");
	mSaveBtn2->setEnabled(hasChanged() || (mLSLProc && FSScriptEditorRecompileButton && !mCompiling));
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
// NaCl End

void LLScriptEdCore::setScriptText(const std::string& text, BOOL is_valid)
{
	if (mEditor)
	{
		// NaCl - LSL Preprocessor
		std::string ntext = text;
		if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mLSLProc)
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
	if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mPostEditor)
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

bool LLScriptEdCore::loadScriptText(const std::string& filename)
{
	if (filename.empty())
	{
		LL_WARNS() << "Empty file name" << LL_ENDL;
		return false;
	}

	LLFILE* file = LLFile::fopen(filename, "rb");		/*Flawfinder: ignore*/
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

	// <FS:Zi> Optionally convert tabs to spaces
	// mEditor->setText(LLStringExplicit(buffer));
	std::string scriptText=LLStringExplicit(buffer);
	if(gSavedSettings.getBOOL("ExternalEditorConvertTabsToSpaces"))
	{
		LLStringUtil::replaceTabsWithSpaces(scriptText,mEditor->spacesPerTab());
	}
	// </FS:Zi>
	mEditor->setText(scriptText);

	delete[] buffer;

	return true;
}

bool LLScriptEdCore::writeToFile(const std::string& filename, bool unprocessed)
{
	LLFILE* fp = LLFile::fopen(filename, "wb");
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
	std::string tmp_file = mContainer->getTmpFileName();
	llstat s;
	if (LLFile::stat(tmp_file, &s) == 0) // file exists
	{
		if (mLiveFile) mLiveFile->ignoreNextUpdate();
		writeToFile(tmp_file,gSavedSettings.getBOOL("_NACL_LSLPreprocessor"));
	}
}

bool LLScriptEdCore::hasChanged()
{
	if (!mEditor) return false;

	return ((!mEditor->isPristine() || mEnableSave) && mHasScriptData);
}

void LLScriptEdCore::draw()
{
// <FS:CR> Advanced Script Editor
	//BOOL script_changed	= hasChanged();
	//mSaveBtn->setEnabled(script_changed);
	updateButtonBar();
// </FS:CR>

	if( mEditor->hasFocus() )
	{
		S32 line = 0;
		S32 column = 0;
		mEditor->getCurrentLineAndColumn( &line, &column, FALSE );  // don't include wordwrap
		LLStringUtil::format_map_t args;
		std::string cursor_pos;
		args["[LINE]"] = llformat ("%d", line);
		args["[COLUMN]"] = llformat ("%d", column);
		cursor_pos = LLTrans::getString("CursorPos", args);
		mLineCol->setValue(cursor_pos);
	}
	else
	{
		mLineCol->setValue(LLStringUtil::null);
	}

	updateDynamicHelp();

	LLPanel::draw();
}

void LLScriptEdCore::updateDynamicHelp(BOOL immediate)
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
			std::string help_string = mEditor->getText().substr(segment->getStart(), segment->getEnd() - segment->getStart());
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

	url_string.setArg("[LSL_STRING]", help_string);

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

BOOL LLScriptEdCore::canClose()
{
	if(mForceClose || !hasChanged())
	{
		return TRUE;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		LLNotificationsUtil::add("SaveChanges", LLSD(), LLSD(), boost::bind(&LLScriptEdCore::handleSaveChangesDialog, this, _1, _2));
		return FALSE;
	}
}

void LLScriptEdCore::setEnableEditing(bool enable)
{
	mEditor->setEnabled(enable);
	getChildView("Edit_btn")->setEnabled(enable);
	getChildView("edit_btn_2")->setEnabled(enable);	// <FS:Zi> support extra edit button
}

bool LLScriptEdCore::handleSaveChangesDialog(const LLSD& notification, const LLSD& response )
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	switch( option )
	{
	case 0:  // "Yes"
		// close after saving
			doSave( TRUE );
		break;

	case 1:  // "No"
		mForceClose = TRUE;
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

void LLScriptEdCore::onBtnHelp()
{
	LLUI::sHelpImpl->showTopic(HELP_LSL_PORTAL_TOPIC);
}

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
			parent->addDependentFloater(live_help_floater, TRUE);
		live_help_floater->childSetCommitCallback("lock_check", onCheckLock, this);
		live_help_floater->getChild<LLUICtrl>("lock_check")->setValue(gSavedSettings.getBOOL("ScriptHelpFollowsCursor"));
		live_help_floater->childSetCommitCallback("history_combo", onHelpComboCommit, this);
		live_help_floater->childSetAction("back_btn", onClickBack, this);
		live_help_floater->childSetAction("fwd_btn", onClickForward, this);

		LLMediaCtrl* browser = live_help_floater->getChild<LLMediaCtrl>("lsl_guide_html");
		browser->setAlwaysRefresh(TRUE);

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

	BOOL visible = TRUE;
	BOOL take_focus = TRUE;
	live_help_floater->setVisible(visible);
	live_help_floater->setFrontmost(take_focus);

	updateDynamicHelp(TRUE);
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
	self->mEditor->setFocus(TRUE);
	self->setHelpPage(self->mFunctions->getSimple());
}

// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
//void LLScriptEdCore::doSave( BOOL close_after_save )
void LLScriptEdCore::doSave(BOOL close_after_save, bool sync /*= true*/)
// </FS:Ansariel>
{
	// NaCl - LSL Preprocessor
	// Clear status list *before* running the preprocessor (FIRE-10172) -Sei
	mErrorList->deleteAllItems();
	mErrorList->setCommentText(std::string());

	updateIndicators(true, false); //<FS:Kadah> Compile Indicators

	if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && mLSLProc)
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
void LLScriptEdCore::doSaveComplete( void* userdata, BOOL close_after_save, bool sync)
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
				msg = getString("external_editor_not_set");
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
		delete mLiveFile; // deletes file

		// Save the script to a temporary file.
		std::string filename = mContainer->getTmpFileName();
		writeToFile(filename,gSavedSettings.getBOOL("_NACL_LSLPreprocessor"));

		// Start watching file changes.
		mLiveFile = new LLLiveLSLFile(filename, boost::bind(&LLScriptEdContainer::onExternalChange, mContainer, _1));
		mLiveFile->addToEventTimer();
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
		// NaCl - LSL Preprocessor
		if (gSavedSettings.getBOOL("_NACL_LSLPreprocessor") && self->mPostEditor)
		{
			LLPanel* tab = self->getChild<LLPanel>("Preprocessed");
			LLTabContainer* tabset = self->getChild<LLTabContainer>("Tabset");

			if(tabset)
				tabset->selectTabByName("Preprocessed");

			if(tab)
				tab->setFocus(TRUE);

			if( self->mPostEditor )
			{
				self->mPostEditor->setFocus(TRUE);
				self->mPostEditor->setCursor(row, column);
			}
		}
		else
		{
		  self->mEditor->setCursor(row, column);
		  self->mEditor->setFocus(TRUE);
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
			setScriptText(getString("loading"), FALSE);
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
	S32 count = mBridges.size();
	LLEntryAndEdCore* eandc;
	for(S32 i = 0; i < count; i++)
	{
		eandc = mBridges.at(i);
		delete eandc;
		mBridges[i] = NULL;
	}
	mBridges.clear();
}

// virtual
BOOL LLScriptEdCore::handleKeyHere(KEY key, MASK mask)
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
				return TRUE;
			}
			doSave(FALSE);
			// NaCl End
				
			//mSaveCallback(mUserdata, FALSE);
		}

		return TRUE;
	}

	if(('F' == key) && just_control)
	{
		if(mSearchReplaceCallback)
		{
			mSearchReplaceCallback(mUserdata);
		}

		return TRUE;
	}

	return FALSE;
}

void LLScriptEdCore::onBtnLoadFromFile( void* data )
{
	LLScriptEdCore* self = (LLScriptEdCore*) data;

	// TODO Maybe add a dialogue warning here if the current file has unsaved changes.
	LLFilePicker& file_picker = LLFilePicker::instance();
	if( !file_picker.getOpenFile( LLFilePicker::FFLOAD_SCRIPT ) )
	{
		//File picking cancelled by user, so nothing to do.
		return;
	}

	std::string filename = file_picker.getFirstFile();

	std::ifstream fin(filename.c_str());

	std::string line;
	std::string text;
	std::string linetotal;
	while (!fin.eof())
	{ 
		getline(fin,line);
		text += line;
		if (!fin.eof())
		{
			text += "\n";
		}
	}
	fin.close();

	// Only replace the script if there is something to replace with.
	if (text.length() > 0)
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
		LLFilePicker& file_picker = LLFilePicker::instance();
		if( file_picker.getSaveFile( LLFilePicker::FFSAVE_SCRIPT, self->mScriptName ) )
		{
			std::string filename = file_picker.getFirstFile();
			std::string scriptText=self->mEditor->getText();
			std::ofstream fout(filename.c_str());
			fout<<(scriptText);
			fout.close();
			// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
			//self->mSaveCallback( self->mUserdata, FALSE );
			self->mSaveCallback( self->mUserdata, FALSE, true );
			// </FS:Ansariel>
		}
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
	if (!self || !self->mEditor) return FALSE;
	return self->mEditor->canLoadOrSaveToFile();
}

// static 
bool LLScriptEdCore::enableLoadFromFileMenu(void* userdata)
{
	LLScriptEdCore* self = (LLScriptEdCore*)userdata;
	return (self && self->mEditor) ? self->mEditor->canLoadOrSaveToFile() : FALSE;
}

LLUUID LLScriptEdCore::getAssociatedExperience()const
{
	return mAssociatedExperience;
}

void LLLiveLSLEditor::setExperienceIds( const LLSD& experience_ids )
{
	mExperienceIds=experience_ids;
	updateExperiencePanel();
}


void LLLiveLSLEditor::updateExperiencePanel()
{
	if(mScriptEd->getAssociatedExperience().isNull())
	{
		mExperienceEnabled->set(FALSE);
		mExperiences->setVisible(FALSE);
		if(mExperienceIds.size()>0)
		{
			mExperienceEnabled->setEnabled(TRUE);
			mExperienceEnabled->setToolTip(getString("add_experiences"));
		}
		else
		{
			mExperienceEnabled->setEnabled(FALSE);
			mExperienceEnabled->setToolTip(getString("no_experiences"));
		}
		getChild<LLButton>("view_profile")->setVisible(FALSE);
	}
	else
	{
		mExperienceEnabled->setToolTip(getString("experience_enabled"));
		mExperienceEnabled->setEnabled(getIsModifiable());
		mExperiences->setVisible(TRUE);
		mExperienceEnabled->set(TRUE);
		getChild<LLButton>("view_profile")->setToolTip(getString("show_experience_profile"));
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
		
		const LLSD& experience = LLExperienceCache::get(id);
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
		const LLSD& experience = LLExperienceCache::get(associated);
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
		item->setEnabled(FALSE);
	}

	if(last.notNull())
	{
		mExperiences->setEnabled(FALSE);
		LLExperienceCache::get(last, boost::bind(&LLLiveLSLEditor::buildExperienceList, this));  
	}
	else
	{
		mExperiences->setEnabled(TRUE);
		mExperiences->sortByName(TRUE);
		mExperiences->setCurrentByIndex(mExperiences->getCurrentIndex());
		getChild<LLButton>("view_profile")->setVisible(TRUE);
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
			LLHTTPClient::get(lookup_url, new ExperienceResponder(getDerivedHandle<LLLiveLSLEditor>()));
		}
	}
}



/// ---------------------------------------------------------------------------
/// LLScriptEdContainer
/// ---------------------------------------------------------------------------

LLScriptEdContainer::LLScriptEdContainer(const LLSD& key) :
	LLPreview(key)
,	mScriptEd(NULL)
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
,	mBackupTimer(NULL)
// [/SL:KB]
{
}

// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
LLScriptEdContainer::~LLScriptEdContainer()
{
	// Clean up the backup file (unless we've gotten disconnected)
	if ( (!mBackupFilename.empty()) && (gAgent.getRegion()) )
		LLFile::remove(mBackupFilename);
	delete mBackupTimer;
}

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
	if ( (mScriptEd) && (mScriptEd->hasChanged()) )
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

std::string LLScriptEdContainer::getTmpFileName()
{
	// Take script inventory item id (within the object inventory)
	// to consideration so that it's possible to edit multiple scripts
	// in the same object inventory simultaneously (STORM-781).
	std::string script_id = mObjectUUID.asString() + "_" + mItemUUID.asString();

	// Use MD5 sum to make the file name shorter and not exceed maximum path length.
	char script_id_hash_str[33];			   /* Flawfinder: ignore */
	LLMD5 script_id_hash((const U8 *)script_id.c_str());
	script_id_hash.hex_digest(script_id_hash_str);

	return std::string(LLFile::tmpdir()) + "sl_script_" + script_id_hash_str + ".lsl";
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
	mScriptEd->doSave(FALSE, false);
	// </FS>
	return true;
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
								   HELLO_LSL,
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
:	LLScriptEdContainer(key),
	mPendingUploads(0)
{
	mFactoryMap["script panel"] = LLCallbackMap(LLPreviewLSL::createScriptEdPanel, this);
}

// virtual
BOOL LLPreviewLSL::postBuild()
{
	const LLInventoryItem* item = getItem();

	llassert(item);
	if (item)
	{
		getChild<LLUICtrl>("desc")->setValue(item->getDescription());
	}
	childSetCommitCallback("desc", LLPreview::onText, this);
	getChild<LLLineEditor>("desc")->setPrevalidate(&LLTextValidate::validateASCIIPrintableNoPipe);

	return LLPreview::postBuild();
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
	BOOL is_library = item
		&& !gInventory.isObjectDescendentOf(mItemUUID,
											gInventory.getRootFolderID());
	if(!item)
	{
		// do the more generic search.
		getItem();
	}
	if(item)
	{
		BOOL is_copyable = gAgent.allowOperation(PERM_COPY, 
								item->getPermissions(), GP_OBJECT_MANIPULATE);
		BOOL is_modifiable = gAgent.allowOperation(PERM_MODIFY,
								item->getPermissions(), GP_OBJECT_MANIPULATE);
		if (gAgent.isGodlike() || (is_copyable && (is_modifiable || is_library)))
		{
			LLUUID* new_uuid = new LLUUID(mItemUUID);
			gAssetStorage->getInvItemAsset(LLHost::invalid,
										gAgent.getID(),
										gAgent.getSessionID(),
										item->getPermissions().getOwner(),
										LLUUID::null,
										item->getUUID(),
										item->getAssetUUID(),
										item->getType(),
										&LLPreviewLSL::onLoadComplete,
										(void*)new_uuid,
										TRUE);
			mAssetStatus = PREVIEW_ASSET_LOADING;
		}
		else
		{
			mScriptEd->setScriptText(mScriptEd->getString("can_not_view"), FALSE);
			mScriptEd->mEditor->makePristine();
			mScriptEd->mFunctions->setEnabled(FALSE);
			mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		getChildView("lock")->setVisible( !is_modifiable);
		mScriptEd->getChildView("Insert...")->setEnabled(is_modifiable);
	}
	else
	{
		mScriptEd->setScriptText(std::string(HELLO_LSL), TRUE);
		mScriptEd->setEnableEditing(TRUE);
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}
}


BOOL LLPreviewLSL::canClose()
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
//	LLFloaterScriptSearch::show(sec);
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLFloaterSearchReplace::show(sec->mEditor);
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
//void LLPreviewLSL::onSave(void* userdata, BOOL close_after_save)
void LLPreviewLSL::onSave(void* userdata, BOOL close_after_save, bool sync)
// </FS:Ansariel>
{
	LLPreviewLSL* self = (LLPreviewLSL*)userdata;
	self->mCloseAfterSave = close_after_save;
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//self->saveIfNeeded();
	self->saveIfNeeded(sync);
	// </FS:Ansariel>
}

// Save needs to compile the text in the buffer. If the compile
// succeeds, then save both assets out to the database. If the compile
// fails, go ahead and save the text anyway.
void LLPreviewLSL::saveIfNeeded(bool sync /*= true*/)
{
	// LL_INFOS() << "LLPreviewLSL::saveIfNeeded()" << LL_ENDL;
//	if(!mScriptEd->hasChanged())
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

	// save off asset into file
	LLTransactionID tid;
	tid.generate();
	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,asset_id.asString());
	std::string filename = filepath + ".lsl";

	mScriptEd->writeToFile(filename,false);
	
	if (sync)
	{
		mScriptEd->sync();
	}

	const LLInventoryItem *inv_item = getItem();
	// save it out to asset server
	std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");

	// NaCL - LSL Preprocessor
	mScriptEd->enableSave(FALSE); // Clear the enable save flag (FIRE-10173)
	BOOL domono = FSLSLPreprocessor::mono_directive(mScriptEd->getScriptText());
	if(domono == FALSE)
	{
		LLSD row;
		if(gSavedSettings.getBOOL("_NACL_SaveInventoryScriptsAsMono"))
		{
			row["columns"][0]["value"] = "Detected compile-as-LSL2 directive, but debug setting SaveInventoryScriptsAsMono overrided it.";
			domono = TRUE;
		}else row["columns"][0]["value"] = "Detected compile-as-LSL2 directive";
		//domono = FALSE;
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		mScriptEd->mErrorList->addElement(row);
	}
	// NaCl End
	if(inv_item)
	{
		getWindow()->incBusyCount();
		mPendingUploads++;
		if (!url.empty())
		{
			// NaCL - LSL Preprocessor
			uploadAssetViaCaps(url, filename, mItemUUID, domono);
		}
	}
}

void LLPreviewLSL::uploadAssetViaCaps(const std::string& url,
									  const std::string& filename,
									  const LLUUID& item_id,
									  bool mono)
{
	LL_INFOS() << "Update Agent Inventory via capability" << LL_ENDL;
	LLSD body;
	body["item_id"] = item_id;
	if (gSavedSettings.getBOOL("FSSaveInventoryScriptsAsMono"))
	{
		body["target"] = "mono";
	}
	else
	{
		body["target"] = "lsl2";
	}
	LLHTTPClient::post(url, body, new LLUpdateAgentInventoryResponder(body, filename, LLAssetType::AT_LSL_TEXT));
}

// static
void LLPreviewLSL::onSaveComplete(const LLUUID& iuuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	// NaCl - LSL Preprocessor
	LLUUID asset_uuid = iuuid;
	// NaCl End
	LLScriptSaveInfo* info = reinterpret_cast<LLScriptSaveInfo*>(user_data);
	if(0 == status)
	{
		if (info)
		{
			const LLViewerInventoryItem* item;
			item = (const LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if(item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				// NaCl - LSL Preprocessor
				if(asset_uuid.isNull())
					asset_uuid.generate();
				// NaCl End
				new_item->setAssetUUID(asset_uuid);
				new_item->setTransactionID(info->mTransactionID);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				LL_WARNS() << "Inventory item for script " << info->mItemUUID
					<< " is no longer in agent inventory." << LL_ENDL;
			}

			// Find our window and close it if requested.
			LLPreviewLSL* self = LLFloaterReg::findTypedInstance<LLPreviewLSL>("preview_script", info->mItemUUID);
			if (self)
			{
				getWindow()->decBusyCount();
				self->mPendingUploads--;
				if (self->mPendingUploads <= 0
					&& self->mCloseAfterSave)
				{
					self->closeFloater();
				}
			}
		}
	}
	else
	{
		LL_WARNS() << "Problem saving script: " << status << LL_ENDL;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("SaveScriptFailReason", args);
	}
	delete info;
}

// static
void LLPreviewLSL::onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
	LLUUID* instance_uuid = (LLUUID*)user_data;
	LLPreviewLSL* self = NULL;
	if(instance_uuid)
	{
		self = LLFloaterReg::findTypedInstance<LLPreviewLSL>("preview_script", *instance_uuid);
	}
	if (0 == status)
	{
		if (self)
		{
			LLSD row;
			row["columns"][0]["value"] = "Compile successful!";
			row["columns"][0]["font"] = "SANSSERIF_SMALL";
			self->mScriptEd->mErrorList->addElement(row);

			// Find our window and close it if requested.
			self->getWindow()->decBusyCount();
			self->mPendingUploads--;
			if (self->mPendingUploads <= 0
				&& self->mCloseAfterSave)
			{
				self->closeFloater();
			}
		}
	}
	else
	{
		LL_WARNS() << "Problem saving LSL Bytecode (Preview)" << LL_ENDL;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("SaveBytecodeFailReason", args);
	}
	delete instance_uuid;
}

// static
void LLPreviewLSL::onLoadComplete( LLVFS *vfs, const LLUUID& asset_uuid, LLAssetType::EType type,
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
			LLVFile file(vfs, asset_uuid, type);
			S32 file_length = file.getSize();

			std::vector<char> buffer(file_length+1);
			file.read((U8*)&buffer[0], file_length);

			// put a EOS at the end
			buffer[file_length] = 0;
			preview->mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), TRUE);
			preview->mScriptEd->mEditor->makePristine();
			LLInventoryItem* item = gInventory.getItem(*item_uuid);
			BOOL is_modifiable = FALSE;
			if(item
			   && gAgent.allowOperation(PERM_MODIFY, item->getPermissions(),
				   					GP_OBJECT_MANIPULATE))
			{
				is_modifiable = TRUE;		
			}
			preview->mScriptEd->setEnableEditing(is_modifiable);
			preview->mAssetStatus = PREVIEW_ASSET_LOADED;

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
								   HELLO_LSL,
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
	mAskedForRunningInfo(FALSE),
	mHaveRunningInfo(FALSE),
	mCloseAfterSave(FALSE),
	mPendingUploads(0),
	mIsModifiable(FALSE),
	mIsNew(false)
{
	mFactoryMap["script ed panel"] = LLCallbackMap(LLLiveLSLEditor::createScriptEdPanel, this);
}

BOOL LLLiveLSLEditor::postBuild()
{
	childSetCommitCallback("running", LLLiveLSLEditor::onRunningCheckboxClicked, this);
	getChildView("running")->setEnabled(FALSE);

	childSetAction("Reset",&LLLiveLSLEditor::onReset,this);
	getChildView("Reset")->setEnabled(TRUE);

	mMonoCheckbox =	getChild<LLCheckBoxCtrl>("mono");
	childSetCommitCallback("mono", &LLLiveLSLEditor::onMonoCheckboxClicked, this);
	getChildView("mono")->setEnabled(FALSE);

	mScriptEd->mEditor->makePristine();
	mScriptEd->mEditor->setFocus(TRUE);


	mExperiences = getChild<LLComboBox>("Experiences...");
	mExperiences->setCommitCallback(boost::bind(&LLLiveLSLEditor::experienceChanged, this));
	
	mExperienceEnabled = getChild<LLCheckBoxCtrl>("enable_xp");
	
	childSetCommitCallback("enable_xp", onToggleExperience, this);
	childSetCommitCallback("view_profile", onViewProfile, this);
	

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
				ExperienceAssociationResponder::fetchAssociatedExperience(item->getParentUUID(), item->getUUID(), boost::bind(&LLLiveLSLEditor::setAssociatedExperience, getDerivedHandle<LLLiveLSLEditor>(), _1));
				
				bool isGodlike = gAgent.isGodlike();
				bool copyManipulate = gAgent.allowOperation(PERM_COPY, item->getPermissions(), GP_OBJECT_MANIPULATE);
				mIsModifiable = gAgent.allowOperation(PERM_MODIFY, item->getPermissions(), GP_OBJECT_MANIPULATE);
				
				if(!isGodlike && (!copyManipulate || !mIsModifiable))
				{
					mItem = new LLViewerInventoryItem(item);
					mScriptEd->setScriptText(getString("not_allowed"), FALSE);
					mScriptEd->mEditor->makePristine();
					mScriptEd->enableSave(FALSE);
					mAssetStatus = PREVIEW_ASSET_LOADED;
				}
				else if(copyManipulate || isGodlike)
				{
					mItem = new LLViewerInventoryItem(item);
					// request the text from the object
				LLSD* user_data = new LLSD();
				user_data->with("taskid", mObjectUUID).with("itemid", mItemUUID);
					gAssetStorage->getInvItemAsset(object->getRegion()->getHost(),
						gAgent.getID(),
						gAgent.getSessionID(),
						item->getPermissions().getOwner(),
						object->getID(),
						item->getUUID(),
						item->getAssetUUID(),
						item->getType(),
						&LLLiveLSLEditor::onLoadComplete,
						(void*)user_data,
						TRUE);
					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_GetScriptRunning);
					msg->nextBlockFast(_PREHASH_Script);
					msg->addUUIDFast(_PREHASH_ObjectID, mObjectUUID);
					msg->addUUIDFast(_PREHASH_ItemID, mItemUUID);
					msg->sendReliable(object->getRegion()->getHost());
					mAskedForRunningInfo = TRUE;
					mAssetStatus = PREVIEW_ASSET_LOADING;
				}
			}
			
			if(mItem.isNull())
			{
				mScriptEd->setScriptText(LLStringUtil::null, FALSE);
				mScriptEd->mEditor->makePristine();
				mAssetStatus = PREVIEW_ASSET_LOADED;
				mIsModifiable = FALSE;
			}

			refreshFromItem();
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
	else
	{
		mScriptEd->setScriptText(std::string(HELLO_LSL), TRUE);
		mScriptEd->enableSave(FALSE);
		LLPermissions perm;
		perm.init(gAgent.getID(), gAgent.getID(), LLUUID::null, gAgent.getGroupID());
		perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE, PERM_MOVE | PERM_TRANSFER);
		mItem = new LLViewerInventoryItem(mItemUUID,
										  mObjectUUID,
										  perm,
										  LLUUID::null,
										  LLAssetType::AT_LSL_TEXT,
										  LLInventoryType::IT_LSL,
										  DEFAULT_SCRIPT_NAME,
										  DEFAULT_SCRIPT_DESC,
										  LLSaleInfo::DEFAULT,
										  LLInventoryItemFlags::II_FLAGS_NONE,
										  time_corrected());
		mAssetStatus = PREVIEW_ASSET_LOADED;
	}

	requestExperiences();
}

// static
void LLLiveLSLEditor::onLoadComplete(LLVFS *vfs, const LLUUID& asset_id,
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
			
			instance->loadScriptText(vfs, asset_id, type);
			instance->mScriptEd->setEnableEditing(TRUE);
			instance->mAssetStatus = PREVIEW_ASSET_LOADED;

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

void LLLiveLSLEditor::loadScriptText(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type)
{
	LLVFile file(vfs, uuid, type);
	S32 file_length = file.getSize();
	std::vector<char> buffer(file_length + 1);
	file.read((U8*)&buffer[0], file_length);

	if (file.getLastBytesRead() != file_length ||
		file_length <= 0)
	{
		LL_WARNS() << "Error reading " << uuid << ":" << type << LL_ENDL;
	}

	buffer[file_length] = '\0';

	mScriptEd->setScriptText(LLStringExplicit(&buffer[0]), TRUE);
	mScriptEd->mEditor->makePristine();
	mScriptEd->setScriptName(getItem()->getName());
}


void LLLiveLSLEditor::onRunningCheckboxClicked( LLUICtrl*, void* userdata )
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*) userdata;
	LLViewerObject* object = gObjectList.findObject( self->mObjectUUID );
	LLCheckBoxCtrl* runningCheckbox = self->getChild<LLCheckBoxCtrl>("running");
	BOOL running =  runningCheckbox->get();
	//self->mRunningCheckbox->get();
	if( object )
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
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectUUID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemUUID);
		msg->addBOOLFast(_PREHASH_Running, running);
		msg->sendReliable(object->getRegion()->getHost());
	}
	else
	{
		runningCheckbox->set(!running);
		LLNotificationsUtil::add("CouldNotStartStopScript");
	}
}

void LLLiveLSLEditor::onReset(void *userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*) userdata;

	LLViewerObject* object = gObjectList.findObject( self->mObjectUUID );
	if(object)
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
		msg->addUUIDFast(_PREHASH_ObjectID, self->mObjectUUID);
		msg->addUUIDFast(_PREHASH_ItemID, self->mItemUUID);
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
	LLCheckBoxCtrl* runningCheckbox = getChild<LLCheckBoxCtrl>( "running");
	if(object && mAskedForRunningInfo && mHaveRunningInfo)
	{
		if(object->permAnyOwner())
		{
			runningCheckbox->setLabel(getString("script_running"));
			runningCheckbox->setEnabled(TRUE);

			// <FS:Ansariel> Rev 496 LL merge error
			//if(object->permAnyOwner())
			//{
			//	runningCheckbox->setLabel(getString("script_running"));
			//	runningCheckbox->setEnabled(TRUE);
			//}
			//else
			//{
			//	runningCheckbox->setLabel(getString("public_objects_can_not_run"));
			//	runningCheckbox->setEnabled(FALSE);
			//	// *FIX: Set it to false so that the ui is correct for
			//	// a box that is released to public. It could be
			//	// incorrect after a release/claim cycle, but will be
			//	// correct after clicking on it.
			//	runningCheckbox->set(FALSE);
			//	mMonoCheckbox->set(FALSE);
			//}
			// </FS:Ansariel>
		}
		else
		{
			runningCheckbox->setLabel(getString("public_objects_can_not_run"));
			runningCheckbox->setEnabled(FALSE);

			// *FIX: Set it to false so that the ui is correct for
			// a box that is released to public. It could be
			// incorrect after a release/claim cycle, but will be
			// correct after clicking on it.
			runningCheckbox->set(FALSE);
			mMonoCheckbox->setEnabled(FALSE);
			// object may have fallen out of range.
			// <FS:Ansariel> Rev 496 LL merge error
			//mHaveRunningInfo = FALSE;
		}
	}
	else if(!object)
	{
		// HACK: Display this information in the title bar.
		// Really ought to put in main window.
		setTitle(LLTrans::getString("ObjectOutOfRange"));
		runningCheckbox->setEnabled(FALSE);
		// <FS:Ansariel> Rev 496 LL merge error
		mMonoCheckbox->setEnabled(FALSE);
		// object may have fallen out of range.
		mHaveRunningInfo = FALSE;
	}

	LLPreview::draw();
}


void LLLiveLSLEditor::onSearchReplace(void* userdata)
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;

	LLScriptEdCore* sec = self->mScriptEd; 
//	LLFloaterScriptSearch::show(sec);
// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-10-26 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLFloaterSearchReplace::show(sec->mEditor);
// [/SL:KB]
}

struct LLLiveLSLSaveData
{
	LLLiveLSLSaveData(const LLUUID& id, const LLViewerInventoryItem* item, BOOL active);
	LLUUID mSaveObjectID;
	LLPointer<LLViewerInventoryItem> mItem;
	BOOL mActive;
};

LLLiveLSLSaveData::LLLiveLSLSaveData(const LLUUID& id,
									 const LLViewerInventoryItem* item,
									 BOOL active) :
	mSaveObjectID(id),
	mActive(active)
{
	llassert(item);
	mItem = new LLViewerInventoryItem(item);
}

// virtual
void LLLiveLSLEditor::saveIfNeeded(bool sync /*= true*/)
{
	LLViewerObject* object = gObjectList.findObject(mObjectUUID);
	if(!object)
	{
		LLNotificationsUtil::add("SaveScriptFailObjectNotFound");
		return;
	}

	if(mItem.isNull() || !mItem->isFinished())
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
	if(inv_item)
	{
		mItem->copyItem(inv_item);
	}

	// Don't need to save if we're pristine
	if(!mScriptEd->hasChanged())
	{
		return;
	}

	mPendingUploads = 0;

	// save the script
	mScriptEd->enableSave(FALSE);
	mScriptEd->mEditor->makePristine();
	// <FS> FIRE-10172: Fix LSL editor error display
	//mScriptEd->mErrorList->deleteAllItems();

	// set up the save on the local machine.
	mScriptEd->mEditor->makePristine();
	LLTransactionID tid;
	tid.generate();
	LLAssetID asset_id = tid.makeAssetID(gAgent.getSecureSessionID());
	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,asset_id.asString());
	std::string filename = llformat("%s.lsl", filepath.c_str());

	mItem->setAssetUUID(asset_id);
	mItem->setTransactionID(tid);

	mScriptEd->writeToFile(filename,false);

	if (sync)
	{
		mScriptEd->sync();
	}
	
	// save it out to asset server
	std::string url = object->getRegion()->getCapability("UpdateScriptTask");
	getWindow()->incBusyCount();
	mPendingUploads++;
	BOOL is_running = getChild<LLCheckBoxCtrl>( "running")->get();
	if (!url.empty())
	{
		uploadAssetViaCaps(url, filename, mObjectUUID, mItemUUID, is_running, mScriptEd->getAssociatedExperience());
	}
}

// <FS:TT> //AO Custom Update Agent Inventory via capability
void LLLiveLSLEditor::uploadAssetViaCapsStatic(const std::string& url,
										 const std::string& filename,
										 const LLUUID& task_id,
										 const LLUUID& item_id,
										 const std::string& is_mono,
										 BOOL is_running)
{
	LLSD body;
	body["item_id"] = item_id;
	body["target"] = is_mono.c_str();
	LL_INFOS() << "Upload caps body=" << body << " url=" << url << " id= " << item_id << LL_ENDL;
	LLHTTPClient::post(url, body, 
		new LLUpdateAgentInventoryResponder(body, filename, LLAssetType::AT_LSL_TEXT));
}
// </FS:TT>

void LLLiveLSLEditor::uploadAssetViaCaps(const std::string& url,
										 const std::string& filename,
										 const LLUUID& task_id,
										 const LLUUID& item_id,
										 BOOL is_running,
										 const LLUUID& experience_public_id )
{
	LL_INFOS() << "Update Task Inventory via capability " << url << LL_ENDL;
	LLSD body;
	body["task_id"] = task_id;
	body["item_id"] = item_id;
	body["is_script_running"] = is_running;
	body["target"] = monoChecked() ? "mono" : "lsl2";
	body["experience"] = experience_public_id;
	LLHTTPClient::post(url, body,
		new LLUpdateTaskInventoryResponder(body, filename, LLAssetType::AT_LSL_TEXT));
}

void LLLiveLSLEditor::onSaveTextComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
// <FS> Remove more legacy stuff -Sei
#if 0
	LLLiveLSLSaveData* data = (LLLiveLSLSaveData*)user_data;

	if (status)
	{
		LL_WARNS() << "Unable to save text for a script." << LL_ENDL;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("CompileQueueSaveText", args);
	}
	else
	{
		LLSD floater_key;
		floater_key["taskid"] = data->mSaveObjectID;
		floater_key["itemid"] = data->mItem->getUUID();
		LLLiveLSLEditor* self = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", floater_key);
		if (self)
		{
			self->getWindow()->decBusyCount();
			self->mPendingUploads--;
			if (self->mPendingUploads <= 0
				&& self->mCloseAfterSave)
			{
				self->closeFloater();
			}
		}
	}
	delete data;
	data = NULL;
#endif
}


void LLLiveLSLEditor::onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status) // StoreAssetData callback (fixed)
{
// <FS> Remove more legacy stuff -Sei
#if 0
	LLLiveLSLSaveData* data = (LLLiveLSLSaveData*)user_data;
	if(!data)
		return;
	if(0 ==status)
	{
		LL_INFOS() << "LSL Bytecode saved" << LL_ENDL;
		LLSD floater_key;
		floater_key["taskid"] = data->mSaveObjectID;
		floater_key["itemid"] = data->mItem->getUUID();
		LLLiveLSLEditor* self = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", floater_key);
		if (self)
		{
			// Tell the user that the compile worked.
			self->mScriptEd->mErrorList->setCommentText(LLTrans::getString("SaveComplete"));
			// close the window if this completes both uploads
			self->getWindow()->decBusyCount();
			self->mPendingUploads--;
			if (self->mPendingUploads <= 0
				&& self->mCloseAfterSave)
			{
				self->closeFloater();
			}
		}
		LLViewerObject* object = gObjectList.findObject(data->mSaveObjectID);
		if(object)
		{
			object->saveScript(data->mItem, data->mActive, false);
			dialog_refresh_all();
			//LLToolDragAndDrop::dropScript(object, ids->first,
			//						  LLAssetType::AT_LSL_TEXT, FALSE);
		}
	}
	else
	{
		LL_INFOS() << "Problem saving LSL Bytecode (Live Editor)" << LL_ENDL;
		LL_WARNS() << "Unable to save a compiled script." << LL_ENDL;

		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		LLNotificationsUtil::add("CompileQueueSaveBytecode", args);
	}

	std::string filepath = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,asset_uuid.asString());
	std::string dst_filename = llformat("%s.lso", filepath.c_str());
	LLFile::remove(dst_filename);
	delete data;
#endif
}

BOOL LLLiveLSLEditor::canClose()
{
	return (mScriptEd->canClose());
}

void LLLiveLSLEditor::closeIfNeeded()
{
	getWindow()->decBusyCount();
	mPendingUploads--;
	if (mPendingUploads <= 0 && mCloseAfterSave)
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
//void LLLiveLSLEditor::onSave(void* userdata, BOOL close_after_save)
void LLLiveLSLEditor::onSave(void* userdata, BOOL close_after_save, bool sync)
// </FS:Ansariel>
{
	LLLiveLSLEditor* self = (LLLiveLSLEditor*)userdata;
	if(self)
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
	LLLiveLSLEditor* instance = LLFloaterReg::findTypedInstance<LLLiveLSLEditor>("preview_scriptedit", floater_key);
	if(instance)
	{
		instance->mHaveRunningInfo = TRUE;
		BOOL running;
		msg->getBOOLFast(_PREHASH_Script, _PREHASH_Running, running);
		LLCheckBoxCtrl* runningCheckbox = instance->getChild<LLCheckBoxCtrl>("running");
		runningCheckbox->set(running);
		BOOL mono;
		msg->getBOOLFast(_PREHASH_Script, "Mono", mono);
		LLCheckBoxCtrl* monoCheckbox = instance->getChild<LLCheckBoxCtrl>("mono");
		monoCheckbox->setEnabled(instance->getIsModifiable() && have_script_upload_cap(object_id));
		monoCheckbox->set(mono);
	}
}


void LLLiveLSLEditor::onMonoCheckboxClicked(LLUICtrl*, void* userdata)
{
	LLLiveLSLEditor* self = static_cast<LLLiveLSLEditor*>(userdata);
	self->mMonoCheckbox->setEnabled(have_script_upload_cap(self->mObjectUUID));
	self->mScriptEd->enableSave(self->getIsModifiable());
}

BOOL LLLiveLSLEditor::monoChecked() const
{
	if(NULL != mMonoCheckbox)
	{
		return mMonoCheckbox->getValue()? TRUE : FALSE;
	}
	return FALSE;
}

void LLLiveLSLEditor::setAssociatedExperience( LLHandle<LLLiveLSLEditor> editor, const LLSD& experience )
{
	LLLiveLSLEditor* scriptEd = editor.get();
	if(scriptEd)
	{
		LLUUID id;
		if(experience.has(LLExperienceCache::EXPERIENCE_ID))
		{
			id=experience[LLExperienceCache::EXPERIENCE_ID].asUUID();
		}
		scriptEd->mScriptEd->setAssociatedExperience(id);
		scriptEd->updateExperiencePanel();
	}
}

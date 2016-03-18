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

#ifndef LL_LLPREVIEWSCRIPT_H
#define LL_LLPREVIEWSCRIPT_H

#include "llpreview.h"
#include "lltabcontainer.h"
#include "llinventory.h"
#include "llcombobox.h"
#include "lliconctrl.h"
#include "llframetimer.h"
#include "llfloatergotoline.h"
#include "llsyntaxid.h"
#include <boost/signals2.hpp>

class LLLiveLSLFile;
class LLMessageSystem;
class LLScriptEditor;
class LLButton;
class LLCheckBoxCtrl;
class LLScrollListCtrl;
class LLViewerObject;
struct 	LLEntryAndEdCore;
class LLMenuBarGL;
//class LLFloaterScriptSearch;
class LLKeywordToken;
class LLVFS;
class LLViewerInventoryItem;
class LLScriptEdContainer;
class LLFloaterGotoLine;
class LLFloaterExperienceProfile;
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0)
class LLEventTimer;
// [/SL:KB]
// NaCl - LSL Preprocessor
class FSLSLPreprocessor;
// NaCl End

// Inner, implementation class.  LLPreviewScript and LLLiveLSLEditor each own one of these.
class LLScriptEdCore : public LLPanel
{
	friend class LLPreviewScript;
	friend class LLPreviewLSL;
	friend class LLLiveLSLEditor;
//	friend class LLFloaterScriptSearch;
	friend class LLScriptEdContainer;
	friend class LLFloaterGotoLine;
	// NaCl - LSL Preprocessor
	friend class FSLSLPreprocessor;
	// NaCl End

protected:
	// Supposed to be invoked only by the container.
	LLScriptEdCore(
		LLScriptEdContainer* container,
		const std::string& sample,
		const LLHandle<LLFloater>& floater_handle,
		void (*load_callback)(void* userdata),
		// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
		//void (*save_callback)(void* userdata, BOOL close_after_save),
		void (*save_callback)(void* userdata, BOOL close_after_save, bool sync),
		// </FS:Ansariel>
		void (*search_replace_callback)(void* userdata),
		void* userdata,
		bool live,
		S32 bottom_pad = 0);	// pad below bottom row of buttons
public:
	~LLScriptEdCore();
	
	void			initializeKeywords();
	void			initMenu();
	void			processKeywords();
	void			processLoaded();

	virtual void	draw();
	/*virtual*/	BOOL	postBuild();
	BOOL			canClose();
	void			setEnableEditing(bool enable);
	bool			canLoadOrSaveToFile( void* userdata );

	void            setScriptText(const std::string& text, BOOL is_valid);
	// NaCL - LSL Preprocessor
	std::string		getScriptText();
	void			doSaveComplete(void* userdata, BOOL close_after_save, bool sync);
	// NaCl End
	bool			loadScriptText(const std::string& filename);
	bool			writeToFile(const std::string& filename, bool unprocessed);
	void			sync();
	
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//void			doSave( BOOL close_after_save );
	void			doSave(BOOL close_after_save, bool sync = true);
	// </FS:Ansariel>

	bool			handleSaveChangesDialog(const LLSD& notification, const LLSD& response);
	bool			handleReloadFromServerDialog(const LLSD& notification, const LLSD& response);

	void			openInExternalEditor();

	static void		onCheckLock(LLUICtrl*, void*);
	static void		onHelpComboCommit(LLUICtrl* ctrl, void* userdata);
	static void		onClickBack(void* userdata);
	static void		onClickForward(void* userdata);
	static void		onBtnInsertSample(void*);
	static void		onBtnInsertFunction(LLUICtrl*, void*);
	static void		onBtnLoadFromFile(void*);
    static void		onBtnSaveToFile(void*);
	static void		onBtnPrefs(void*);	// <FS:CR> Advanced Script Editor

	static bool		enableSaveToFileMenu(void* userdata);
	static bool		enableLoadFromFileMenu(void* userdata);

    virtual bool	hasAccelerators() const { return true; }
	LLUUID 			getAssociatedExperience()const;
	void            setAssociatedExperience( const LLUUID& experience_id );

	void 			setScriptName(const std::string& name){mScriptName = name;};

private:
	// NaCl - LSL Preprocessor
	void		onToggleProc();
	boost::signals2::connection	mTogglePreprocConnection;
	// NaCl End
	void		onBtnHelp();
	void		onBtnDynamicHelp();
	void		onBtnUndoChanges();

	bool		hasChanged();

	void selectFirstError();

	virtual BOOL handleKeyHere(KEY key, MASK mask);
	
	void enableSave(BOOL b) {mEnableSave = b;}
	
// <FS:CR> Advanced Script Editor
	void	initButtonBar();
	void	updateButtonBar();
// </FS:CR>

	void	updateIndicators(bool compiling, bool success); // <FS:Kadah> Compile indicators

protected:
	void deleteBridges();
	void setHelpPage(const std::string& help_string);
	void updateDynamicHelp(BOOL immediate = FALSE);
	bool isKeyword(LLKeywordToken* token);
	void addHelpItemToHistory(const std::string& help_string);
	static void onErrorList(LLUICtrl*, void* user_data);

	bool			mLive;
	bool			mCompiling; // <FS:Kadah> Compile indicators

private:
	std::string		mSampleText;
	std::string		mScriptName;
	LLScriptEditor*	mEditor;
	void			(*mLoadCallback)(void* userdata);
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//void			(*mSaveCallback)(void* userdata, BOOL close_after_save);
	void			(*mSaveCallback)(void* userdata, BOOL close_after_save, bool sync);
	// </FS:Ansariel>
	void			(*mSearchReplaceCallback) (void* userdata);
    void*			mUserdata;
    LLComboBox		*mFunctions;
	BOOL			mForceClose;
	LLPanel*		mCodePanel;
	LLScrollListCtrl* mErrorList;
	std::vector<LLEntryAndEdCore*> mBridges;
	LLHandle<LLFloater>	mLiveHelpHandle;
	LLKeywordToken* mLastHelpToken;
	LLFrameTimer	mLiveHelpTimer;
	S32				mLiveHelpHistorySize;
	BOOL			mEnableSave;
	BOOL			mHasScriptData;
	LLLiveLSLFile*	mLiveFile;
	LLUUID			mAssociatedExperience;
	LLTextBox*		mLineCol;
// <FS:CR> Advanced Script Editor
	//LLView*			mSaveBtn;
	LLButton*		mSaveBtn;
	LLButton*		mSaveBtn2;	// 	// <FS:Zi> support extra save button
	LLButton*		mCutBtn;
	LLButton*		mCopyBtn;
	LLButton*		mPasteBtn;
	LLButton*		mUndoBtn;
	LLButton*		mRedoBtn;
	LLButton*		mSaveToDiskBtn;
	LLButton*		mLoadFromDiskBtn;
	LLButton*		mSearchBtn;
// </FS:CR>
	// NaCl - LSL Preprocessor
	FSLSLPreprocessor* mLSLProc;
	LLScriptEditor*	mPostEditor;
	std::string		mPostScript;
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
	LLScriptEdContainer(const LLSD& key, const bool live);
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
	/*virtual*/ ~LLScriptEdContainer();

	/*virtual*/ void refreshFromItem();
// [/SL:KB]

	// <FS:Ansariel> FIRE-16740: Color syntax highlighting changes don't immediately appear in script window
	void updateStyle();

protected:
	std::string		getTmpFileName();
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
	virtual std::string getBackupFileName() const;
	bool			onBackupTimer();
// [/SL:KB]

	bool			onExternalChange(const std::string& filename);
	virtual void	saveIfNeeded(bool sync = true) = 0;

	LLScriptEdCore*		mScriptEd;
// [SL:KB] - Patch: Build-ScriptRecover | Checked: 2011-11-23 (Catznip-3.2.0) | Added: Catznip-3.2.0
	std::string			mBackupFilename;
	LLEventTimer*		mBackupTimer;
// [/SL:KB]
};

// Used to view and edit an LSL script from your inventory.
class LLPreviewLSL : public LLScriptEdContainer
{
public:
	LLPreviewLSL(const LLSD& key );
	virtual void callbackLSLCompileSucceeded();
	virtual void callbackLSLCompileFailed(const LLSD& compile_errors);

	/*virtual*/ BOOL postBuild();

// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLScriptEditor* getEditor() { return (mScriptEd) ? mScriptEd->mEditor : NULL; }
// [/SL:KB]

protected:
	virtual BOOL canClose();
	void closeIfNeeded();

	virtual void loadAsset();
	/*virtual*/ void saveIfNeeded(bool sync = true);
	// NaCl - LSL Preprocessor
	void uploadAssetViaCaps(const std::string& url,
							const std::string& filename, 
							const LLUUID& item_id,
							bool mono);
	// NaCl End

	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//static void onSave(void* userdata, BOOL close_after_save);
	static void onSave(void* userdata, BOOL close_after_save, bool sync);
	// </FS:Ansariel>
	
	static void onLoadComplete(LLVFS *vfs, const LLUUID& uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveComplete(const LLUUID& uuid, void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
	
protected:
	static void* createScriptEdPanel(void* userdata);


protected:

	// Can safely close only after both text and bytecode are uploaded
	S32 mPendingUploads;

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

	/*virtual*/ BOOL postBuild();
	
    void setIsNew() { mIsNew = TRUE; }
// <FS:TT> Client LSL Bridge
	static void uploadAssetViaCapsStatic(const std::string& url,
							const std::string& filename, 
							const LLUUID& task_id,
							const LLUUID& item_id,
							const std::string& is_mono,
							BOOL is_running);
// </FS:TT>

// [SL:KB] - Patch: UI-FloaterSearchReplace | Checked: 2010-11-05 (Catznip-2.3.0a) | Added: Catznip-2.3.0a
	LLScriptEditor* getEditor() { return (mScriptEd) ? mScriptEd->mEditor : NULL; }
// [/SL:KB]

	static void setAssociatedExperience( LLHandle<LLLiveLSLEditor> editor, const LLSD& experience );
	static void onToggleExperience(LLUICtrl *ui, void* userdata);
	static void onViewProfile(LLUICtrl *ui, void* userdata);

	void setExperienceIds(const LLSD& experience_ids);
	void buildExperienceList();
	void updateExperiencePanel();
	void requestExperiences();
	void experienceChanged();
	void addAssociatedExperience(const LLSD& experience);
	
private:
	virtual BOOL canClose();
	void closeIfNeeded();
	virtual void draw();

	virtual void loadAsset();
	void loadAsset(BOOL is_new);
	/*virtual*/ void saveIfNeeded(bool sync = true);
	void uploadAssetViaCaps(const std::string& url,
							const std::string& filename,
							const LLUUID& task_id,
							const LLUUID& item_id,
							BOOL is_running,
							const LLUUID& experience_public_id);
	BOOL monoChecked() const;


	static void onSearchReplace(void* userdata);
	static void onLoad(void* userdata);
	// <FS:Ansariel> FIRE-7514: Script in external editor needs to be saved twice
	//static void onSave(void* userdata, BOOL close_after_save);
	static void onSave(void* userdata, BOOL close_after_save, bool sync);
	// </FS:Ansariel>

	static void onLoadComplete(LLVFS *vfs, const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveTextComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
	static void onSaveBytecodeComplete(const LLUUID& asset_uuid, void* user_data, S32 status, LLExtStat ext_status);
	static void onRunningCheckboxClicked(LLUICtrl*, void* userdata);
	static void onReset(void* userdata);

	void loadScriptText(LLVFS *vfs, const LLUUID &uuid, LLAssetType::EType type);

	static void onErrorList(LLUICtrl*, void* user_data);

	static void* createScriptEdPanel(void* userdata);

	static void	onMonoCheckboxClicked(LLUICtrl*, void* userdata);

private:
	bool				mIsNew;
	//LLUUID mTransmitID;
	LLCheckBoxCtrl*		mRunningCheckbox;
	BOOL				mAskedForRunningInfo;
	BOOL				mHaveRunningInfo;
	LLButton*			mResetButton;
	LLPointer<LLViewerInventoryItem> mItem;
	BOOL				mCloseAfterSave;
	// need to save both text and script, so need to decide when done
	S32					mPendingUploads;

	BOOL                mIsSaving;

	BOOL getIsModifiable() const { return mIsModifiable; } // Evaluated on load assert

	LLCheckBoxCtrl*	mMonoCheckbox;
	BOOL mIsModifiable;


	LLComboBox*		mExperiences;
	LLCheckBoxCtrl*	mExperienceEnabled;
	LLSD			mExperienceIds;

	LLHandle<LLFloater> mExperienceProfile;
};

#endif  // LL_LLPREVIEWSCRIPT_H

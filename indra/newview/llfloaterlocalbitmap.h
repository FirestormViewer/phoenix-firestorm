/** 
 * @file llfloaterlocalbitmap.h
 * @author Vaalith Jinn
 * @brief Local Bitmap Browser header
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#ifndef LL_LOCALBITMAP_H
#define LL_LOCALBITMAP_H

/* Local Bitmap Browser: header */

#include "llfloater.h"
#include "lleventtimer.h"

class LLViewerObject; 
class LLImageRaw;
class LLDrawable;
class LLScrollListCtrl;
class LLTextureCtrl;
class LLLineEditor;
class LLComboBox;   
class LLCheckBoxCtrl;
class LLTextBox;


/*=======================================*/
/*  Global structs / enums / defines     */
/*=======================================*/ 

#define LOCAL_USE_MIPMAPS true
#define LOCAL_DISCARD_LEVEL 0
#define NO_IMAGE LLUUID::null

#define TIMER_HEARTBEAT 3.0
#define SLAM_FOR_DEBUG true

enum EBitmapListCols
{
	BLIST_COL_NAME,
	BLIST_COL_ID
};

/* update related */
struct LLAffectedObject
{
	LLViewerObject* object;
	std::vector<S32> face_list;
	bool local_sculptmap;

};

/* for finding texture pickers */
#define LOCAL_TEXTURE_PICKER_NAME "texture picker"
#define LOCAL_TEXTURE_PICKER_LIST_NAME "local_name_list"
#define LOCAL_TEXTURE_PICKER_RECURSE true

/* texture picker uses these */
#define	LOCALLIST_COL_ID 1
#define LOCALLIST_LOCAL_TAB "local_tab"

/*=======================================*/
/*  LLLocalBitmap: unit class            */
/*=======================================*/ 
class LLLocalBitmap
{
	public:
		LLLocalBitmap(std::string filename);
		virtual ~LLLocalBitmap(void);
		friend class LLLocalBitmapBrowser;

	public: /* [enums, typedefs, etc] */
		enum ELinkStatus
		{
			LS_UNKNOWN, /* default fallback */
			LS_ON,
			LS_OFF,
			LS_BROKEN,
			LS_UPDATING
		};

		enum EExtensionType
		{
			ET_IMG_BMP,
			ET_IMG_TGA,
			ET_IMG_JPG,
			ET_IMG_PNG
		};

		enum EBitmapType
		{
			BT_TEXTURE = 0,
			BT_SCULPT  = 1,
			BT_LAYER   = 2
		};

	public: /* [information query functions] */
		std::string getShortName(void);
		std::string getFileName(void);
		LLUUID      getID(void);
		LLSD        getLastModified(void);
		std::string getLinkStatus(void);
		bool        getUpdateBool(void);
		void        setType( S32 );
		bool        getIfValidBool(void);
		S32		    getType(void);
		void		getDebugInfo(void);

	private: /* [maintenence functions] */
		void updateSelf(void);
		bool decodeSelf(LLImageRaw* rawimg);
		void setUpdateBool(void);

		LLLocalBitmap*				  getThis(void);
		std::vector<S32>  		      getFaceUsesThis(LLDrawable*);
		std::vector<LLAffectedObject> getUsingObjects(bool seek_by_type = true,
												      bool seek_textures = false, bool seek_sculptmaps = false);

	protected: /* [basic properties] */
		std::string    mShortName;
		std::string    mFilename;
		EExtensionType mExtension;
		LLUUID         mId;
		LLSD           mLastModified;
		ELinkStatus    mLinkStatus;
		bool           mKeepUpdating;
		bool           mValid;
		S32		       mBitmapType;
		bool           mSculptDirty;
		bool           mVolumeDirty;
};

/*=======================================*/
/*  LLLocalBitmapBrowser: main class     */
/*=======================================*/ 
class LLLocalBitmapBrowser
{
	public:
		LLLocalBitmapBrowser();
		virtual ~LLLocalBitmapBrowser();
		
		friend class LLFloaterLocalBitmapBrowser;
		friend class LLLocalBitmapBrowserTimer;
		friend class LLFloaterTexturePicker;
		
		static void updateTextureCtrlList(LLScrollListCtrl*);
		static void setLayerUpdated(bool toggle) { sLayerUpdated = toggle; }
		static void setSculptUpdated(bool toggle) { sSculptUpdated = toggle; }
		static void addBitmap(void);
		static void loadBitmaps(void);
		static void delBitmap( std::vector<LLScrollListItem*>, S32 column = BLIST_COL_ID );
		
	private:
		static void onChangeHappened(void);
		static void onUpdateBool(LLUUID);
		static void onSetType(LLUUID, S32);
		static LLLocalBitmap* getBitmapUnit(LLUUID);
		static bool isDoingUpdates(void);
		static void pingTimer(void);
		static void performTimedActions(void);
		static void performSculptUpdates(LLLocalBitmap*);

	protected:
		static  std::vector<LLLocalBitmap*> sLoadedBitmaps;
		typedef std::vector<LLLocalBitmap*>::iterator local_list_iter;
		static  bool    sLayerUpdated;
		static  bool    sSculptUpdated; 
};

/*==================================================*/
/*  LLFloaterLocalBitmapBrowser : floater class     */
/*==================================================*/ 
class LLFloaterLocalBitmapBrowser : public LLFloater
{
public:
    LLFloaterLocalBitmapBrowser( const LLSD& key );
    virtual ~LLFloaterLocalBitmapBrowser();
    BOOL postBuild(void);

 
private: 
    // Button callback declarations
    static void onClickAdd(void* userdata);
	static void onClickDel(void* userdata);
	static void onClickUpload(void* userdata);

	// ScrollList callback declarations
	static void onChooseBitmapList(LLUICtrl* ctrl, void* userdata);

	// Checkbox callback declarations
	static void onClickUpdateChkbox(LLUICtrl* ctrl, void* userdata);

	// Combobox type select
	static void onCommitTypeCombo(LLUICtrl* ctrl, void* userdata);

	// Widgets
	LLButton* mAddBtn;
	LLButton* mDelBtn;
	LLButton* mUploadBtn;

	LLScrollListCtrl* mBitmapList;
	LLScrollListCtrl* mUsedList;
	LLTextureCtrl*    mTextureView;
	LLCheckBoxCtrl*   mUpdateChkBox;

	LLLineEditor* mPathTxt;
	LLLineEditor* mUUIDTxt;

	LLTextBox*  mLinkTxt;
	LLTextBox*  mTimeTxt;
	LLComboBox* mTypeComboBox;

	LLTextBox* mCaptionPathTxt;
	LLTextBox* mCaptionUUIDTxt;
	LLTextBox* mCaptionLinkTxt;
	LLTextBox* mCaptionTimeTxt;

	/* to ensure no crashing happens if, for some reason, someone opens two instances of the floater
	   we're keeping a list of selves for the static functions to go through */
	static std::list<LLFloaterLocalBitmapBrowser*> sSelfInstances;

	// non-widget functions
public:
	static void updateBitmapScrollList(void);
	static void updateRightSide(void);

};

/*==================================================*/
/*     LLLocalBitmapBrowserTimer : timer class      */
/*==================================================*/ 
class LLLocalBitmapBrowserTimer : public LLEventTimer
{
	public:
		LLLocalBitmapBrowserTimer();
		~LLLocalBitmapBrowserTimer();
		virtual BOOL tick();
		void		 start();
		void		 stop();
		bool         isRunning();
};

#endif // LL_LOCALBITMAP_H


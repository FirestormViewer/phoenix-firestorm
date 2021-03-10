/**
 * @file fsfloaterexport.h
 * @brief export selected objects to an xml file in LLSD format.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Techwolf Lupindo
 * Copyright (c) 2013 Cinder Roxley <cinder.roxley@phoenixviewer.com>
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_FSEXPORT_H
#define FS_FSEXPORT_H

#include "llselectmgr.h"
#include "llsingleton.h"
#include "lltexturecache.h"
#include "llviewertexture.h"

// Floater includes
#include "llfloater.h"
#include "llscrolllistctrl.h"

class LLObjectSelection;

struct FSAssetResourceData
{
	std::string name;
	std::string description;
	void* user_data;
	LLUUID uuid;
};

class FSFloaterObjectExport : public LLFloater, public LLVOInventoryListener
{
	LOG_CLASS(FSFloaterObjectExport);
public:
	FSFloaterObjectExport(const LLSD& key);
	BOOL postBuild();
	void updateSelection();
	
	static void onImageLoaded(BOOL success,
							  LLViewerFetchedTexture *src_vi,
							  LLImageRaw* src,
							  LLImageRaw* aux_src,
							  S32 discard_level,
							  BOOL final,
							  void* userdata);
	void fetchTextureFromCache(LLViewerFetchedTexture* src_vi);
	void saveFormattedImage(LLPointer<LLImageFormatted> mFormattedImage, LLUUID id);
	void removeRequestedTexture(LLUUID texture_id);
	static void onIdle(void *user_data);
	/*virtual*/ void inventoryChanged(LLViewerObject* object,
									  LLInventoryObject::object_list_t* inventory,
									  S32 serial_num,
									  void* user_data);
	static void onLoadComplete(LLVFS *vfs, const LLUUID& asset_uuid,
							   LLAssetType::EType type,
							   void* user_data, S32 status, LLExtStat ext_status);
	
private:
	typedef enum {IDLE, INVENTORY_DOWNLOAD, ASSET_DOWNLOAD, TEXTURE_DOWNLOAD} FSExportState;
	virtual ~FSFloaterObjectExport();	
	/* virtual */ void draw();
	/* virtual */ void onOpen(const LLSD& key);
	void refresh();
	void dirty();
	bool exportSelection();
	void addSelectedObjects();
	void populateObjectList();
	void onClickExport();
	void onExportFileSelected(const std::vector<std::string>& filenames);
	void addTexturePreview();
	S32 getNumExportableTextures();
	void addObject(const LLViewerObject* prim, const std::string name);
	void updateTextureInfo();
	void updateUI();
	void updateTitleProgress(FSExportState state);

	FSExportState mExportState;
	typedef std::vector<std::pair<LLViewerObject*,std::string> > obj_info_t;
	obj_info_t mObjects;
	std::string mFilename;
	LLSafeHandle<LLObjectSelection> mObjectSelection;
	LLUUID mCurrentObjectID;
	
	S32 mTotal;
	S32 mIncluded;
	S32 mNumTextures;
	S32 mNumExportableTextures;
	
	LLScrollListCtrl* mObjectList;
	LLPanel* mTexturePanel;
	
	std::string mObjectName;
	
	LLSD getLinkSet(LLSelectNode* node);
	void addPrim(LLViewerObject* object, bool root);
	bool exportTexture(const LLUUID& texture_id);
	
	void onIdle();
	void removeRequestedAsset(LLUUID asset_uuid);
	
	LLSD mManifest;
	std::map<LLUUID, FSAssetResourceData> mRequestedTexture;
	std::map<LLUUID, bool> mTextureChecked;
	std::string mFilePath;
	LLLoadedCallbackEntry::source_callback_list_t mCallbackTextureList;
	LLFrameTimer mWaitTimer;
	S32 mLastRequest;
	bool mExported;
	bool mAborted;
	bool mDirty;
	
	typedef std::vector<LLUUID> id_list_t;
	typedef std::vector<std::string> string_list_t;
	id_list_t mTextures;
	string_list_t mTextureNames;
	
	uuid_vec_t mInventoryRequests;
	uuid_vec_t mAssetRequests;
	
	class FSExportCacheReadResponder : public LLTextureCache::ReadResponder
	{
		LOG_CLASS(FSExportCacheReadResponder);
	public:
		FSExportCacheReadResponder(const LLUUID& id, LLImageFormatted* image, FSFloaterObjectExport* parent);
		
		void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal);
		virtual void completed(bool success);
		
	private:
		LLPointer<LLImageFormatted> mFormattedImage;
		LLUUID mID;
		FSFloaterObjectExport* mParent;
	};
};

#endif // FS_FSEXPORT_H

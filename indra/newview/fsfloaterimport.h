/**
 * @file fsfloaterimport.h
 * @brief Floater to import objects from a file.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2012 Techwolf Lupindo
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

#ifndef FS_FSFLOATERIMPORT_H
#define FS_FSFLOATERIMPORT_H

#include "llfloater.h"
#include "llhttpsdhandler.h"
#include "llinventorymodel.h"
#include "llresourcedata.h"
#include "llselectmgr.h"
#include "llviewerinventory.h"
#include "llviewerobject.h"

struct FSResourceData;

class FSFloaterImport : public LLFloater
{
	LOG_CLASS(FSFloaterImport);
public:
	FSFloaterImport(const LLSD &filename);
	virtual ~FSFloaterImport();
	virtual BOOL postBuild();
	
	static void onIdle(void *user_data);
	
	
	void onClickCheckBoxUploadAsset();
	void onClickCheckBoxTempAsset();
	bool processPrimCreated(LLViewerObject* object);
	static void onAssetUploadComplete(const LLUUID& uuid, void* userdata, S32 result, LLExtStat ext_status);

	void uploadAsset(LLUUID assid_id, LLUUID inventory_item = LLUUID::null);
	
	std::map<LLUUID,LLUUID> mAssetItemMap;

	struct NextAsset
	{
		LLUUID new_uuid;
		LLUUID asset_id;
		LLAssetType::EType asset_type;
	};

	void pushNextAsset( LLUUID new_uuid, LLUUID asset_id, LLAssetType::EType asset_type );
	void popNextAsset();

	void uploadDone()
	{ --m_AssetsUploading; }
	void startUpload()
	{ ++m_AssetsUploading; }
	U32 getUploads() const
	{ return m_AssetsUploading; }

private:
	U32 m_AssetsUploading;
	std::deque< NextAsset > m_NextAssets;

	typedef enum
	{
	  IDLE,
	  INVENTORY_TRANSFER,
	  LINKING
	} FSImportState;
	FSImportState mImportState;

	void loadFile();
	void populateBackupInfo();
	void onClickBtnImport();
	void createPrim();
	void postLink();
	void onIdle();
	void setPrimPosition(U8 type, LLViewerObject* object, LLVector3 position, LLQuaternion rotation = LLQuaternion(), LLVector3 scale = LLVector3());
	void addAsset(LLUUID texture, LLAssetType::EType asset_type);
	void importPrims();
	void searchInventory(LLUUID asset_id, LLViewerObject* object, std::string prim_name);
	void processPrim(LLSD& prim);

	LLSD mManifest;
	std::string mFileFullName;
	std::string mFilename;
	std::string mFilePath;
	bool mCreatingActive;
	FSFloaterImport* mInstance;
	S32 mLinkset;
	S32 mObject;
	S32 mPrim;
	LLVector3 mRootPosition;
	LLVector3 mStartPosition;
	LLVector3 mLinksetPosition;
	LLQuaternion mRootRotation;
	std::map<LLUUID,LLUUID> mPrimObjectMap;
	S32 mLinksetSize;
	S32 mObjectSize;
	LLObjectSelectionHandle	mObjectSelection;
	uuid_vec_t mTextureQueue;
	U32 mTexturesTotal;
	uuid_vec_t mSoundQueue;
	U32 mSoundsTotal;
	uuid_vec_t mAnimQueue;
	U32 mAnimsTotal;
	uuid_vec_t mAssetQueue;
	U32 mAssetsTotal;
	std::map<LLUUID,LLUUID> mAssetMap;
	BOOL mSavedSettingShowNewInventory;
	boost::signals2::connection mObjectCreatedCallback;
	
	struct FSInventoryQueue
	{
		LLViewerInventoryItem* item;
		LLViewerObject* object;
		std::string prim_name;
	};
	std::vector<FSInventoryQueue> mInventoryQueue;
	LLFrameTimer mWaitTimer;
	F32 mThrottleTime;
};

class FSCreateItemCallback : public LLInventoryCallback
{
	LOG_CLASS(FSCreateItemCallback);
public:
	FSCreateItemCallback(FSResourceData* data);
  
	void fire(const LLUUID& inv_item);

	FSResourceData* mData;
};

#endif // FS_FSFLOATERIMPORT_H

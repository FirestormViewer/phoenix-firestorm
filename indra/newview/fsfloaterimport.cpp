/**
 * @file fsfloaterimport.cpp
 * @brief Floater to import objects from a file.
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2013 Techwolf Lupindo
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterimport.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llbuycurrencyhtml.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "lleconomy.h"
#include "llfloaterperms.h"
#include "llinventorydefines.h"
#include "llinventoryfunctions.h"
#include "llmultigesture.h"
#include "llnotificationsutil.h"
#include "llparcel.h"
#include "llprimlinkinfo.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "llsdutil.h"
#include "llstatusbar.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "lltransactiontypes.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llviewerregion.h"
#include "llvfile.h"
#include "llvfs.h"
#include "llvolumemessage.h"
#include "lltrace.h"
#include "fsexportperms.h"
#include <boost/algorithm/string_regex.hpp>
#include <boost/lexical_cast.hpp>
#include "llcoros.h"
#include "llcoproceduremanager.h"
#include "llsdutil.h"

struct FSResourceData
{
	LLUUID uuid;
	bool temporary;
	LLAssetType::EType asset_type;
	LLUUID inventory_item;
	LLWearableType::EType wearable_type;
	bool post_asset_upload;
	LLUUID post_asset_upload_id;
	FSFloaterImport *mFloater;
};

void resourceDeleter( LLResourceData *aData )
{
	FSResourceData *data = (FSResourceData*)aData->mUserData;
	delete data;
	delete aData;
}

void uploadCoroutine( LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t &a_httpAdapter, std::string aURL,  LLSD aBody, LLAssetID aAssetId, LLAssetType::EType aAssetType, std::shared_ptr< LLResourceData > aResourceData );

FSFloaterImport::FSFloaterImport(const LLSD& filename) :
	LLFloater(filename),
	mCreatingActive(false),
	mLinkset(0),
	mObject(0),
	mPrim(0),
	mImportState(IDLE),
	mFileFullName(filename),
	mObjectCreatedCallback(),
	m_AssetsUploading( 0 )
{
	mInstance = this;

	mCommitCallbackRegistrar.add("Import.UploadAsset",boost::bind(&FSFloaterImport::onClickCheckBoxUploadAsset,this));
	mCommitCallbackRegistrar.add("Import.TempAsset",boost::bind(&FSFloaterImport::onClickCheckBoxTempAsset,this));

	gIdleCallbacks.addFunction(onIdle, this);

	mSavedSettingShowNewInventory = gSavedSettings.getBOOL("ShowNewInventory");
}

FSFloaterImport::~FSFloaterImport()
{
	if (!gIdleCallbacks.deleteFunction(onIdle, this))
	{
		LL_WARNS("import") << "FSFloaterImport::~FSFloaterImport() failed to delete idle callback" << LL_ENDL;
	}
	if (mObjectCreatedCallback.connected())
	{
		mObjectCreatedCallback.disconnect();
	}

	gSavedSettings.setBOOL("ShowNewInventory", mSavedSettingShowNewInventory);
}

BOOL FSFloaterImport::postBuild()
{
	if (LLGlobalEconomy::Singleton::getInstance()->getPriceUpload() == 0 
		|| gAgent.getRegion()->getCentralBakeVersion() > 0)
	{
		getChild<LLCheckBoxCtrl>("temp_asset")->setVisible(FALSE);   
		getChild<LLCheckBoxCtrl>("temp_asset")->set(FALSE);
	}
	getChild<LLButton>("import_btn")->setCommitCallback(boost::bind(&FSFloaterImport::onClickBtnImport, this));
	loadFile();
	populateBackupInfo();
	
	return TRUE;
}

// static
void FSFloaterImport::onIdle(void* user_data)
{
	FSFloaterImport* self = (FSFloaterImport*)user_data;
	self->onIdle();
}

void FSFloaterImport::onIdle()
{
	if( getUploads() < 1 )
		popNextAsset();

	switch(mImportState)
	{
	case IDLE:
		break;
	case INVENTORY_TRANSFER:
	{
		if (mInventoryQueue.empty())
		{
			mImportState = IDLE;
			mWaitTimer.stop();

			if ((mObject + 1) >= mObjectSize)
			{
				if (mObjectSize > 1)
				{
					// should be allready selected
					LL_DEBUGS("import") << "Linking " << mObjectSize << " objects. " << LLSelectMgr::getInstance()->getSelection()->getObjectCount() << " prims are selected" << LL_ENDL;
					LLSelectMgr::getInstance()->sendLink();
					mImportState = LINKING;
				}
				else
				{
					postLink();
				}
			}
			else
			{
				mObject++;
				createPrim();
			}
			
			return;
		}
		
		if (mWaitTimer.getElapsedTimeF32() < mThrottleTime)
		{
			return;
		}

		FSInventoryQueue item_queue = mInventoryQueue.back();
		LL_DEBUGS("import") << "Dropping " << item_queue.item->getName() << " " << item_queue.item->getUUID() << " into " << item_queue.prim_name << " " << item_queue.object->getID() << LL_ENDL;
		if (item_queue.item->getType() == LLAssetType::AT_LSL_TEXT)
		{
			LLToolDragAndDrop::dropScript(item_queue.object, item_queue.item, TRUE,
						      LLToolDragAndDrop::SOURCE_AGENT,
						      gAgentID);
		}
		else
		{
			LLToolDragAndDrop::dropInventory(item_queue.object, item_queue.item,
							LLToolDragAndDrop::SOURCE_AGENT,
							gAgentID);
		}
		mInventoryQueue.pop_back();
		mWaitTimer.start();
	}
		break;
	case LINKING:
		if (LLSelectMgr::getInstance()->getSelection()->getRootObjectCount() < 2)
		{
			mImportState = IDLE;
			postLink();
		}
		break;
	default:
		break;
	}
}

void FSFloaterImport::loadFile()
{
	mFilePath = gDirUtilp->getDirName(mFilename);
	mFilename = gDirUtilp->getBaseFileName(mFileFullName);

	mManifest.clear();
	mTextureQueue.clear();
	mAnimQueue.clear();
	mSoundQueue.clear();
	mLinksetSize = 0;
	mTexturesTotal = 0;
	mAnimsTotal = 0;
	mSoundsTotal = 0;

	bool file_loaded = false;
	llifstream filestream(mFileFullName.c_str(), std::ios_base::in | std::ios_base::binary);
	if(filestream.is_open())
	{
		filestream.seekg(0, std::ios::end);
		S32 file_size = (S32)filestream.tellg();
		filestream.seekg(0, std::ios::beg);
		if (unzip_llsd(mManifest, filestream, file_size))
		{
			file_loaded = true;
		}
		else
		{
			LL_WARNS("import") << "Failed to deserialize " << mFileFullName << LL_ENDL;
		}
	}
	else
	{
		LL_WARNS("import") << "Unable to open file: " << mFileFullName << LL_ENDL;
	}
	filestream.close();

	if (file_loaded)
	{
		if (mManifest.has("format_version") && mManifest["format_version"].asInteger() <= OXP_FORMAT_VERSION)
		{
			LLSD& linksetsd = mManifest["linkset"];
			U32 prims = 0;
			for (LLSD::array_iterator linkset_iter = linksetsd.beginArray();
				linkset_iter != linksetsd.endArray();
				++linkset_iter)
			{
				LLSD& objectsd = mManifest["linkset"][mLinksetSize];
				for (LLSD::array_iterator prim_iter = objectsd.beginArray();
					prim_iter != objectsd.endArray();
					++prim_iter)
				{
					processPrim(mManifest["prim"][(*prim_iter).asString()]);
					prims++;
				}
				mLinksetSize++;
			}

			LLUIString stats = getString("file_status");
			stats.setArg("[LINKSETS]", llformat("%u", mLinksetSize));
			stats.setArg("[PRIMS]", llformat("%u", prims));
			stats.setArg("[TEXTURES]", llformat("%u", mTexturesTotal));
			stats.setArg("[SOUNDS]", llformat("%u", mSoundsTotal));
			stats.setArg("[ANIMATIONS]", llformat("%u", mAnimsTotal));
			stats.setArg("[ASSETS]", llformat("%u", mAssetsTotal));
			getChild<LLTextBox>("file_status_text")->setText(stats.getString());
		}
		else
		{
			getChild<LLTextBox>("file_status_text")->setText(getString("file_version_error"));
		}
	}
	else
	{
		getChild<LLTextBox>("file_status_text")->setText(getString("file_status_error"));
	}

	LL_DEBUGS("import") << "Linkset size is " << mLinksetSize << LL_ENDL;
	if (mLinksetSize != 0)
	{
		getChild<LLButton>("import_btn")->setEnabled(TRUE);
		getChild<LLCheckBoxCtrl>("do_not_attach")->setEnabled(TRUE);
		getChild<LLCheckBoxCtrl>("region_position")->setEnabled(TRUE);
		getChild<LLCheckBoxCtrl>("upload_asset")->setEnabled(TRUE);
	}
	else
	{
		getChild<LLButton>("import_btn")->setEnabled(FALSE);
		getChild<LLCheckBoxCtrl>("do_not_attach")->setEnabled(FALSE);
		getChild<LLCheckBoxCtrl>("region_position")->setEnabled(FALSE);
		getChild<LLCheckBoxCtrl>("upload_asset")->setEnabled(FALSE);
	}
}

void FSFloaterImport::populateBackupInfo()
{
	childSetTextArg("filename_text", "[FILENAME]", mFilename);
	childSetTextArg("client_text", "[VERSION]", mManifest["format_version"].asString());
	childSetTextArg("client_text", "[CLIENT]", (mManifest.has("client") ? mManifest["client"].asString() : LLTrans::getString("Unknown")));
	childSetTextArg("author_text", "[AUTHOR]", (mManifest.has("author") ? mManifest["author"].asString() : LLTrans::getString("Unknown")));
	childSetTextArg("author_text", "[GRID]", (mManifest.has("grid") ? "@ " + mManifest["grid"].asString() : LLTrans::getString("Unknown")));
	childSetTextArg("creation_date_text", "[DATE_STRING]", (mManifest.has("creation_date") ? mManifest["creation_date"].asString(): LLTrans::getString("Unknown")));
	
	LLUIString title = getString("floater_title");
	title.setArg("[FILENAME]", mFilename);
	setTitle(title);
}

void FSFloaterImport::processPrim(LLSD& prim)
{
	if (prim.has("texture"))
	{
		LLSD& textures = prim["texture"];
		for (LLSD::array_iterator texture_iter = textures.beginArray();
			texture_iter != textures.endArray();
			++texture_iter)
		{
			addAsset((*texture_iter)["imageid"].asUUID(), LLAssetType::AT_TEXTURE);
		}
	}

	if (prim.has("sculpt"))
	{
		addAsset(prim["sculpt"]["texture"].asUUID(), LLAssetType::AT_TEXTURE);
	}

	if (!prim.has("content"))
	{
		return;
	}

	LLSD& contentsd = prim["content"];
	for (LLSD::array_iterator content_iter = contentsd.beginArray();
	      content_iter != contentsd.endArray();
	      ++content_iter)
	{
		if (!mManifest["inventory"].has((*content_iter).asString()))
		{
			continue;
		}

		LLAssetType::EType asset_type = LLAssetType::lookup(mManifest["inventory"][(*content_iter).asString()]["type"].asString().c_str());
		LLUUID asset_id = mManifest["inventory"][(*content_iter).asString()]["asset_id"].asUUID();

		if (!mManifest["asset"].has(asset_id.asString()))
		{
			continue;
		}

		addAsset(asset_id, asset_type);
		std::vector<U8> buffer = mManifest["asset"][asset_id.asString()]["data"].asBinary();

		switch(asset_type)
		{
		case LLAssetType::AT_CLOTHING:
		case LLAssetType::AT_BODYPART:
		{
			std::string asset(buffer.begin(), buffer.end());
			S32 position = asset.rfind("textures");
			boost::regex pattern("[[:xdigit:]]{8}(-[[:xdigit:]]{4}){3}-[[:xdigit:]]{12}");
			boost::sregex_iterator m1(asset.begin() + position, asset.end(), pattern);
			boost::sregex_iterator m2;
			for( ; m1 != m2; ++m1)
			{
				LL_DEBUGS("export") << "Found wearable texture " << m1->str() << LL_ENDL;
				if(LLUUID::validate(m1->str()))
				{
					addAsset(LLUUID(m1->str()), LLAssetType::AT_TEXTURE);
				}
				else
				{
					LL_DEBUGS("export") << "Invalied uuid: " << m1->str() << LL_ENDL;
				}
			}
		}
			break;
		case LLAssetType::AT_GESTURE:
		{
			buffer.push_back('\0');
			LLMultiGesture* gesture = new LLMultiGesture();
			LLDataPackerAsciiBuffer dp((char*)&buffer[0], (S32)buffer.size());
			if (!gesture->deserialize(dp))
			{
				LL_WARNS("export") << "Unable to load gesture " << asset_id << LL_ENDL;
				delete gesture;
				gesture = NULL;
				break;
			}

			S32 i;
			S32 count = gesture->mSteps.size();
			for (i = 0; i < count; ++i)
			{
				LLGestureStep* step = gesture->mSteps[i];
				
				switch(step->getType())
				{
				case STEP_ANIMATION:
				{
					LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
					addAsset(anim_step->mAnimAssetID, LLAssetType::AT_ANIMATION);
				}
					break;
				case STEP_SOUND:
				{
					LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
					addAsset(sound_step->mSoundAssetID, LLAssetType::AT_SOUND);
				}
					break;
				default:
					break;
				}
			}
			delete gesture;
		}
			break;
		default:
			break;
		}
	}
}

void FSFloaterImport::addAsset(LLUUID asset_id, LLAssetType::EType asset_type)
{
	if (!mManifest["asset"].has(asset_id.asString()))
	{
		LL_DEBUGS("import") << "Missing "<< asset_id.asString() << " asset data." << LL_ENDL;
		return;
	}

	switch(asset_type)
	{
	case LLAssetType::AT_TEXTURE:
	{
		if (std::find(mTextureQueue.begin(), mTextureQueue.end(), asset_id) == mTextureQueue.end())  
		{
			mTextureQueue.push_back(asset_id);
			mTexturesTotal++;
		}
	}
		break;
	case LLAssetType::AT_SOUND:
	{
		if (std::find(mSoundQueue.begin(), mSoundQueue.end(), asset_id) == mSoundQueue.end())  
		{
			mSoundQueue.push_back(asset_id);
			mSoundsTotal++;
		}
	}
		break;
	case LLAssetType::AT_ANIMATION:
	{
		if (std::find(mAnimQueue.begin(), mAnimQueue.end(), asset_id) == mAnimQueue.end())  
		{
			mAnimQueue.push_back(asset_id);
			mAnimsTotal++;
		}
	}
		break;
	default:
	{
		if (std::find(mAssetQueue.begin(), mAssetQueue.end(), asset_id) == mAssetQueue.end())  
		{
			mAssetQueue.push_back(asset_id);
			mAssetsTotal++;
		}
	}
		break;
	}
}

void FSFloaterImport::onClickBtnImport()
{
	mPrimObjectMap.clear();
	mLinkset = 0;
	mObject = 0;
	mPrim = 0;
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
	LLSelectMgr::getInstance()->deselectAll();

	mStartPosition = gAgent.getPositionAgent();
	LL_DEBUGS("import") << "gAgent position is " << mStartPosition << LL_ENDL;
	LLVector3 offset;
	offset.set(gSavedSettings.getVector3("FSImportBuildOffset"));
	mStartPosition = mStartPosition + offset * gAgent.getQuat();
	LL_DEBUGS("import") << "mStartPosition is " << mStartPosition << LL_ENDL;

	// don't allow change during a long upload/import
	getChild<LLButton>("import_btn")->setEnabled(FALSE);
	getChild<LLCheckBoxCtrl>("do_not_attach")->setEnabled(FALSE);
	getChild<LLCheckBoxCtrl>("region_position")->setEnabled(FALSE);
	getChild<LLCheckBoxCtrl>("upload_asset")->setEnabled(FALSE);
	getChild<LLCheckBoxCtrl>("temp_asset")->setEnabled(FALSE);

	if (((mTexturesTotal + mSoundsTotal + mAnimsTotal + mAssetsTotal) != 0) && getChild<LLCheckBoxCtrl>("upload_asset")->get())
	{
		// do not pop up preview floaters when creating new inventory items.
		gSavedSettings.setBOOL("ShowNewInventory", false);
		
		if (!getChild<LLCheckBoxCtrl>("temp_asset")->get())
		{
			U32 expected_upload_cost = mTexturesTotal * (U32)LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
			if(!(can_afford_transaction(expected_upload_cost)))
			{
				LLStringUtil::format_map_t args;
				args["AMOUNT"] = llformat("%d", expected_upload_cost);
				LLBuyCurrencyHTML::openCurrencyFloater(LLTrans::getString("UploadingCosts", args), expected_upload_cost);

				// re-enable the controls
				getChild<LLButton>("import_btn")->setEnabled(TRUE);
				getChild<LLCheckBoxCtrl>("do_not_attach")->setEnabled(TRUE);
				getChild<LLCheckBoxCtrl>("region_position")->setEnabled(TRUE);
				getChild<LLCheckBoxCtrl>("upload_asset")->setEnabled(TRUE);
				getChild<LLCheckBoxCtrl>("temp_asset")->setEnabled(getChild<LLCheckBoxCtrl>("upload_asset")->get());
				return;
			}
		}
		if (!mTextureQueue.empty())
		{
			LLUIString status = getString("texture_uploading");
			status.setArg("[TEXTURE]", llformat("%u", mTexturesTotal - (U32)mTextureQueue.size() + 1));
			status.setArg("[TEXTURETOTAL]", llformat("%u", mTexturesTotal));
			getChild<LLTextBox>("file_status_text")->setText(status.getString());
			uploadAsset(mTextureQueue.front());
			return;
		}
		if (!mSoundQueue.empty())
		{
			LLUIString status = getString("sound_uploading");
			status.setArg("[SOUND]", llformat("%u", mSoundsTotal - (U32)mSoundQueue.size() + 1));
			status.setArg("[SOUNDTOTAL]", llformat("%u", mSoundsTotal));
			getChild<LLTextBox>("file_status_text")->setText(status.getString());
			uploadAsset(mSoundQueue.front());
			return;
		}
		if (!mAnimQueue.empty())
		{
			LLUIString status = getString("animation_uploading");
			status.setArg("[ANIMATION]", llformat("%u", mAnimsTotal - (U32)mAnimQueue.size() + 1));
			status.setArg("[ANIMATIONTOTAL]", llformat("%u", mAnimsTotal));
			getChild<LLTextBox>("file_status_text")->setText(status.getString());
			uploadAsset(mAnimQueue.front());
			return;
		}
		if (!mAssetQueue.empty())
		{
			LLUIString status = getString("asset_uploading");
			status.setArg("[ASSET]", llformat("%u", mAssetsTotal - (U32)mAssetQueue.size() + 1));
			status.setArg("[ASSETTOTAL]", llformat("%u", mAssetsTotal));
			getChild<LLTextBox>("file_status_text")->setText(status.getString());
			uploadAsset(mAssetQueue.front());
			return;
		}

		LL_WARNS("import") << "Nothing in queue, proceding to prim import." << LL_ENDL;
		// restore setting to allow preview popups.
		gSavedSettings.setBOOL("ShowNewInventory", mSavedSettingShowNewInventory);
		importPrims();
	}
	else
	{
		importPrims();
	}
}

void FSFloaterImport::onClickCheckBoxUploadAsset()
{
	if (getChild<LLCheckBoxCtrl>("upload_asset")->get())
	{
		getChild<LLCheckBoxCtrl>("temp_asset")->setEnabled(TRUE);
		LLUIString stats = getString("upload_cost");
		stats.setArg("[COST]", llformat("%u", (mTexturesTotal + mSoundsTotal + mAnimsTotal) * (U32)LLGlobalEconomy::Singleton::getInstance()->getPriceUpload()));
		getChild<LLTextBox>("file_status_text")->setText(stats.getString());
	}
	else
	{
		getChild<LLCheckBoxCtrl>("temp_asset")->set(FALSE);
		getChild<LLCheckBoxCtrl>("temp_asset")->setEnabled(FALSE);
		std::string text;
		getChild<LLTextBox>("file_status_text")->setText(text);
	}
}

void FSFloaterImport::onClickCheckBoxTempAsset()
{
	if (getChild<LLCheckBoxCtrl>("temp_asset")->get())
	{
		LLUIString stats = getString("upload_cost");
		stats.setArg("[COST]", llformat("%u", 0));
		getChild<LLTextBox>("file_status_text")->setText(stats.getString());
	}
	else
	{
		LLUIString stats = getString("upload_cost");
		stats.setArg("[COST]", llformat("%u", (mTexturesTotal + mSoundsTotal + mAnimsTotal) * (U32)LLGlobalEconomy::Singleton::getInstance()->getPriceUpload()));
		getChild<LLTextBox>("file_status_text")->setText(stats.getString());
	}
}


void FSFloaterImport::importPrims()
{
	mObjectSize = 0;
	LLSD& objectsd = mManifest["linkset"][mLinkset];
	for (LLSD::array_iterator iter = objectsd.beginArray();
		iter != objectsd.endArray();
		++iter)
	{
		mObjectSize++;
	}
	LL_DEBUGS("import") << "Object size is " << mObjectSize << LL_ENDL;
	if (mObjectSize > MAX_PRIMS_PER_OBJECT)
	{
		LL_WARNS("import") << "Object size is to large to link. " << mObjectSize << " is greater then max linking size of " << MAX_PRIMS_PER_OBJECT << LL_ENDL;
	}
	mRootPosition = mStartPosition;
	LLUUID linkset_root_prim_uuid = mManifest["linkset"][0][0].asUUID();
	LLSD& linkset_root_prim = mManifest["prim"][linkset_root_prim_uuid.asString()];
	mLinksetPosition.setValue(linkset_root_prim["position"]);
	mCreatingActive = true;
	createPrim();
}

void FSFloaterImport::createPrim()
{
	LLUIString status = getString("file_status_running");
	status.setArg("[LINKSET]", llformat("%u", mLinkset + 1));
	status.setArg("[LINKSETS]", llformat("%u", mLinksetSize));
	status.setArg("[PRIM]", llformat("%u", mObject + 1));
	status.setArg("[PRIMS]", llformat("%u", mObjectSize));
	getChild<LLTextBox>("file_status_text")->setText(status.getString());
  
	LLUUID prim_uuid = mManifest["linkset"][mLinkset][mObject].asUUID();
	LL_DEBUGS("import") << "Creating prim from " << prim_uuid.asString() << LL_ENDL;
	LLSD& prim = mManifest["prim"][prim_uuid.asString()];

	gMessageSystem->newMessageFast(_PREHASH_ObjectAdd);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());

	LLUUID group_id = gAgent.getGroupID();
	LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
	if (gSavedSettings.getBOOL("RezUnderLandGroup"))
	{
		if (gAgent.isInGroup(parcel->getGroupID()))
		{
			group_id = parcel->getGroupID();
		}
		else if (gAgent.isInGroup(parcel->getOwnerID()))
		{
			group_id = parcel->getOwnerID();
		}
	}
	gMessageSystem->addUUIDFast(_PREHASH_GroupID, group_id);

	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU8Fast(_PREHASH_Material, (U8)prim["material"].asInteger());

	U32 flags = ll_U32_from_sd(prim["flags"]);
	flags |= FLAGS_CREATE_SELECTED;
	gMessageSystem->addU32Fast(_PREHASH_AddFlags, flags);

	LLVolumeParams volume_params;
	volume_params.getPathParams().fromLLSD(prim["volume"]["path"]);
	volume_params.getProfileParams().fromLLSD(prim["volume"]["profile"]);
	LLVolumeMessage::packVolumeParams(&volume_params, gMessageSystem);

	gMessageSystem->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);

	LLVector3 scale;
	scale.setValue(prim["scale"]);
	LL_DEBUGS("import") << "Creating prim scale of " << scale << LL_ENDL;
	gMessageSystem->addVector3Fast(_PREHASH_Scale, scale );

	LLQuaternion rotation = ll_quaternion_from_sd(prim["rotation"]);
	if (mObject != 0)
	{
		rotation = rotation * mRootRotation;
	}
	else
	{
		mRootRotation = rotation;
	}
	gMessageSystem->addQuatFast(_PREHASH_Rotation, rotation);

	LLVector3 position;
	LLVector3 prim_position;
	prim_position.setValue(prim["position"]);
	if (mObject != 0)
	{
		position = (prim_position * mRootRotation) + mRootPosition;
	}
	else
	{
		position = mRootPosition;
	}
	
	if (mObjectCreatedCallback.connected())
	{
		mObjectCreatedCallback.disconnect();
	}
	mObjectCreatedCallback = gObjectList.setNewObjectCallback(boost::bind(&FSFloaterImport::processPrimCreated, this, _1));
	
	LL_DEBUGS("import") << "Creating prim at position " << position << LL_ENDL;
	gMessageSystem->addVector3Fast(_PREHASH_RayStart, position);
	gMessageSystem->addVector3Fast(_PREHASH_RayEnd, position);

	gMessageSystem->addU8Fast(_PREHASH_BypassRaycast, (U8)TRUE);
	gMessageSystem->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)FALSE);
	gMessageSystem->addU8Fast(_PREHASH_State, (U8)0);
	gMessageSystem->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
	gMessageSystem->sendReliable(gAgent.getRegion()->getHost());
	LLTrace::add(LLStatViewer::OBJECT_CREATE,1);
}

bool FSFloaterImport::processPrimCreated(LLViewerObject* object)
{
	if (!(mInstance && mCreatingActive))
	{
		return false;
	}

	LLSelectMgr::getInstance()->selectObjectAndFamily(object, TRUE);

	LLUUID prim_uuid = mManifest["linkset"][mLinkset][mObject].asUUID();
	LLSD& prim = mManifest["prim"][prim_uuid.asString()];
	LL_DEBUGS("import") << "Processing prim " << prim_uuid.asString() << " for object " << object->getID().asString() << LL_ENDL;
	mPrimObjectMap[prim_uuid] = object->getID();
	U32 object_local_id = object->getLocalID();

	// due to raycasting prim placement errors, send prim position update that is more accurite.
	LLVector3 position;
	LLVector3 prim_position(prim["position"]);
	if (mObject != 0)
	{
		position = (prim_position * mRootRotation) + mRootPosition;
	}
	else
	{
		position = mRootPosition;
	}
	LL_DEBUGS("import") << "Setting prim at position " << position << LL_ENDL;
	setPrimPosition(UPD_POSITION, object, position);

#if (0) // reserved for possiable future bugfix should rot and scale not be accurite from prim creation.

	LLQuaternion rotation = ll_quaternion_from_sd(prim["rotation"]);
	if (mObject != 0)
	{
		rotation = rotation * mRootRotation;
	}

	LLVector3 scale(prim["scale"]);
	LL_DEBUGS("import") << "Setting prim scale of " << scale << LL_ENDL;

	setPrimPosition(UPD_POSITION|UPD_ROTATION|UPD_SCALE, object, position, rotation, scale);
#endif

	S32 texture_count = (S32)object->getNumTEs();
	for(S32 face = 0; face < texture_count; face++)
	{
		
		LLTextureEntry texture_entry;
		if (texture_entry.fromLLSD(prim["texture"][face]))
		{
			LL_DEBUGS("import") << "Setting face " << face << " with texture " << prim["texture"][face]["imageid"].asUUID().asString() << LL_ENDL;
	
			if (mAssetMap[texture_entry.getID()].notNull())
			{
				texture_entry.setID(mAssetMap[texture_entry.getID()]);
				LL_DEBUGS("import") << "Replaced " << prim["texture"][face]["imageid"].asUUID().asString() << " with " << texture_entry.getID().asString() << LL_ENDL;
			}
			
			object->setTE((U8)face, texture_entry);
		}
	}
	object->sendTEUpdate();
	
	// [FS:CR] Materials
	if(prim.has("materials"))
	{
		U8 te = 0;
		LLSD materials = prim["materials"];
		for (LLSD::array_iterator m_itr = materials.beginArray() ;
			 m_itr != materials.endArray() ;
			 ++m_itr)
		{
			LL_DEBUGS("import") << "Setting materials" << LL_ENDL;
			LLMaterial* mat = new LLMaterial();
			mat->fromLLSD(*m_itr);
			object->setTEMaterialParams(te, mat);
			++te;
		}
	}

	if (prim.has("sculpt"))
	{
		LL_DEBUGS("import") << "Found sculpt for " << prim_uuid.asString() << LL_ENDL;
		LLSculptParams sculpt_params;
		sculpt_params.fromLLSD(prim["sculpt"]);
		LL_DEBUGS("import") << "Setting sculpt to " << prim["sculpt"]["texture"].asUUID().asString() << LL_ENDL;
		
		if (mAssetMap[sculpt_params.getSculptTexture()].notNull())
		{
			sculpt_params.setSculptTexture(mAssetMap[sculpt_params.getSculptTexture()], sculpt_params.getSculptType());
			LL_DEBUGS("import") << "Replaced " << prim["sculpt"]["texture"].asUUID().asString() << " with " << sculpt_params.getSculptTexture().asString() << LL_ENDL;
		}

		object->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt_params, true); // sets locally and fires off an update to the region.
	}

	if (prim.has("flexible"))
	{
		LL_DEBUGS("import") << "Found flexiable for " << prim_uuid.asString() << LL_ENDL;
		LLFlexibleObjectData attributes;
		attributes.fromLLSD(prim["flexible"]);
		object->setParameterEntry(LLNetworkData::PARAMS_FLEXIBLE, attributes, true);
	}

	if (prim.has("light"))
	{
		LL_DEBUGS("import") << "Found light for " << prim_uuid.asString() << LL_ENDL;
		LLLightParams light_param_block;
		light_param_block.fromLLSD(prim["light"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT, light_param_block, true);
	}

	if (prim.has("light_texture"))
	{
		LL_DEBUGS("import") << "Found light_texture for " << prim_uuid.asString() << LL_ENDL;
		LLLightImageParams new_light_image_param_block;
		new_light_image_param_block.fromLLSD(prim["light_texture"]);
		object->setParameterEntry(LLNetworkData::PARAMS_LIGHT_IMAGE, new_light_image_param_block, true);
	}

	if (prim.has("clickaction"))
	{
		LL_DEBUGS("import") << "Setting clickaction on " << prim_uuid.asString() << " to " << prim["clickaction"].asInteger() << LL_ENDL;
		gMessageSystem->newMessageFast(_PREHASH_ObjectClickAction);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID() );
		gMessageSystem->addU8("ClickAction", (U8)prim["clickaction"].asInteger());
		gMessageSystem->sendReliable(object->getRegion()->getHost());
	}

	std::string prim_name;
	if (prim.has("name"))
	{
		prim_name = prim["name"].asString();
		LL_DEBUGS("import") << "Setting name on " << prim_uuid.asString() << " to " << prim_name << LL_ENDL;
		gMessageSystem->newMessageFast(_PREHASH_ObjectName);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_LocalID, object_local_id);
		gMessageSystem->addStringFast(_PREHASH_Name, prim_name);
		gMessageSystem->sendReliable(object->getRegion()->getHost());
	}

	if (prim.has("description"))
	{
		LL_DEBUGS("import") << "Setting description on " << prim_uuid.asString() << " to " << prim["description"].asString() << LL_ENDL;
		gMessageSystem->newMessageFast(_PREHASH_ObjectDescription);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_LocalID, object_local_id);
		gMessageSystem->addStringFast(_PREHASH_Description, prim["description"].asString());
		gMessageSystem->sendReliable(object->getRegion()->getHost());
	}

	if (prim.has("group_mask") || prim.has("everyone_mask") || prim.has("next_owner_mask"))
	{
		// can only set one permission bit at a time.
		LL_DEBUGS("import") << "Setting permissions on " << prim_uuid.asString() << LL_ENDL;
		gMessageSystem->newMessageFast(_PREHASH_ObjectPermissions);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gMessageSystem->nextBlockFast(_PREHASH_HeaderData);
		gMessageSystem->addBOOLFast(_PREHASH_Override, (BOOL)FALSE);

		if (prim.has("group_mask"))
		{
			U32 group_mask = ll_U32_from_sd(prim["group_mask"]);
			LL_DEBUGS("import") << "Setting group mask to " << group_mask << LL_ENDL;
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_GROUP);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(group_mask & PERM_MODIFY) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_MODIFY | PERM_MOVE | PERM_COPY);
		}
		if (prim.has("everyone_mask"))
		{
			U32 everyone_mask = ll_U32_from_sd(prim["everyone_mask"]);
			LL_DEBUGS("import") << "Setting everyone mask to " << everyone_mask << LL_ENDL;
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_EVERYONE);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(everyone_mask & PERM_MOVE) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_MOVE);
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_EVERYONE);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(everyone_mask & PERM_COPY) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_COPY);
		}
		if (prim.has("next_owner_mask"))
		{
			U32 next_owner_mask = ll_U32_from_sd(prim["next_owner_mask"]);
			LL_DEBUGS("import") << "Setting next owner mask to " << next_owner_mask << LL_ENDL;
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(next_owner_mask & PERM_MODIFY) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_MODIFY);
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(next_owner_mask & PERM_COPY) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_COPY);
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object_local_id);
			gMessageSystem->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
			gMessageSystem->addBOOLFast(_PREHASH_Set, (BOOL)(next_owner_mask & PERM_TRANSFER) ? TRUE : FALSE);
			gMessageSystem->addU32Fast(_PREHASH_Mask, PERM_TRANSFER);
		}
		
		gMessageSystem->sendReliable(object->getRegion()->getHost());
	
	}

	if (prim.has("sale_info"))
	{
		LLSaleInfo sale_info;
		BOOL has_perm_mask;
		U32 perm_mask;
		sale_info.fromLLSD(prim["sale_info"], has_perm_mask, perm_mask);
		if (sale_info.isForSale())
		{
			LL_DEBUGS("import") << "Setting sale info on " << prim_uuid.asString() << LL_ENDL;
			gMessageSystem->newMessageFast(_PREHASH_ObjectSaleInfo);
			gMessageSystem->nextBlockFast(_PREHASH_AgentData);
			gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
			gMessageSystem->addU32Fast(_PREHASH_LocalID, object->getLocalID());
			sale_info.packMessage(gMessageSystem);
			gMessageSystem->sendReliable(object->getRegion()->getHost());
		}
	}

	if (prim.has("ExtraPhysics"))
	{
		LLSD sim_features;
		object->getRegion()->getSimulatorFeatures(sim_features);
		if (sim_features.has("PhysicsShapeTypes"))
		{
			LL_DEBUGS("import") << "Setting physics on " << prim_uuid.asString() << LL_ENDL;
			LLSD& extra_physics = prim["ExtraPhysics"];
			
			U8 type = (U8)extra_physics["PhysicsShapeType"].asInteger();
			F32 density = (F32)extra_physics["Density"].asReal();
			F32 friction = (F32)extra_physics["Friction"].asReal();
			F32 restitution = (F32)extra_physics["Restitution"].asReal();
			F32 gravity = (F32)extra_physics["GravityMultiplier"].asReal();

			object->setPhysicsShapeType(type);
			object->setPhysicsGravity(gravity);
			object->setPhysicsFriction(friction);
			object->setPhysicsDensity(density);
			object->setPhysicsRestitution(restitution);
			object->updateFlags(TRUE);
		}
	}

	mInventoryQueue.clear();
	if (prim.has("content"))
	{
		LLSD& contentsd = prim["content"];
		for (LLSD::array_iterator content_iter = contentsd.beginArray();
		      content_iter != contentsd.endArray();
		      ++content_iter)
		{
			if (!mManifest["inventory"].has((*content_iter).asString()))
			{
				LL_WARNS("import") << "Inventory content " << (*content_iter) << " was not found in import file." << LL_ENDL;
				continue;
			}

			LLSD& item_sd = mManifest["inventory"][(*content_iter).asString()];
			LLUUID asset_id = item_sd["asset_id"].asUUID();

			if (asset_id.isNull())
			{
				LL_WARNS("import") << "Item  " << item_sd["desc"].asString() << " " << (*content_iter) << " had NULL asset ID." << LL_ENDL;
				continue;
			}

			if (mAssetMap[asset_id].isNull())
			{
				// asset was not uploaded, search inventory using asset_id
				searchInventory(asset_id, object, prim_name);
				continue;
			}

			LLUUID new_asset_id = mAssetMap[asset_id];
			if (mAssetItemMap[new_asset_id].isNull())
			{
				// no item was created for asset, search inventory using new_asset_id
				searchInventory(new_asset_id, object, prim_name);
				continue;
			}

			LLUUID item_id = mAssetItemMap[new_asset_id];
			LLViewerInventoryItem* item = gInventory.getItem(item_id);
			if (!item)
			{
				// else item was not found, search inventory using new_asset id 
				searchInventory(new_asset_id, object, prim_name);
				continue;
			}
			
			FSInventoryQueue item_queue;
			item_queue.item = item;
			item_queue.object = object;
			item_queue.prim_name = prim_name;
			mInventoryQueue.push_back(item_queue);
		}
	}
	
	if (mInventoryQueue.size() < 20)
	{
		mThrottleTime = 0.25f;
	}
	else
	{
		mThrottleTime = 0.5f;
	}

	mImportState = INVENTORY_TRANSFER;
	mWaitTimer.start();

	return true;
}

void FSFloaterImport::searchInventory(LLUUID asset_id, LLViewerObject* object, std::string prim_name)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null,
					cats,
					items,
					LLInventoryModel::INCLUDE_TRASH,
					asset_id_matches);

	if (items.size())
	{
		LLViewerInventoryItem* item = items.at(0);
		
		FSInventoryQueue item_queue;
		item_queue.item = item;
		item_queue.object = object;
		item_queue.prim_name = prim_name;
		mInventoryQueue.push_back(item_queue);
	}
	else
	{
		LL_WARNS("import") << "Asset " << asset_id << " was not found in inventory. Asset not added to object." << LL_ENDL;
	}
}

void FSFloaterImport::postLink()
{
	if (!mCreatingActive)
	{
		LL_WARNS("import") << "Post link called without being active." << LL_ENDL;
		return;
	}
  
	LLUUID root_prim_uuid = mManifest["linkset"][mLinkset][0].asUUID();
	LLSD& root_prim = mManifest["prim"][root_prim_uuid.asString()];
	if (root_prim.has("attachment_point") && !getChild<LLCheckBoxCtrl>("do_not_attach")->get())
	{
		LL_DEBUGS("import") << "Attaching to " << root_prim["attachment_point"].asInteger() << LL_ENDL;
		LLSelectMgr::getInstance()->sendAttach((U8)root_prim["attachment_point"].asInteger(), false);
		
		LLViewerObject* root_object = gObjectList.findObject(mPrimObjectMap[root_prim_uuid]);
		
		setPrimPosition(UPD_POSITION|UPD_LINKED_SETS, root_object, LLVector3(root_prim["position"]));
	}

	if (getChild<LLCheckBoxCtrl>("region_position")->get())
	{
		if (root_prim.has("attachment_point"))
		{
			LL_DEBUGS("import") << "Object is an attachment, can not retore to region position." << LL_ENDL;
		}
		else
		{
			LLViewerObject* root_object = gObjectList.findObject(mPrimObjectMap[root_prim_uuid]);
			
			setPrimPosition(UPD_POSITION|UPD_LINKED_SETS, root_object, LLVector3(root_prim["position"]));
		}
	}

	LLSelectMgr::getInstance()->deselectAll();

	if ((mLinkset + 1) >= mLinksetSize)
	{
		// all done
		mCreatingActive = false;
		LL_DEBUGS("import") << "Finished with " << mLinkset << " linksets and " << mObject << " prims in last linkset" << LL_ENDL;
		mObjectSelection = NULL;
		getChild<LLTextBox>("file_status_text")->setText(getString("file_status_done"));
	}
	else
	{
		mObject = 0;
		mLinkset++;

		mObjectSize = 0;
		LLSD& objectsd = mManifest["linkset"][mLinkset];
		for (
		      LLSD::array_iterator iter = objectsd.beginArray();
		      iter != objectsd.endArray();
		      ++iter)
		{
			mObjectSize++;
		}
		LL_DEBUGS("import") << "Next linkset Object size is " << mObjectSize << LL_ENDL;

		LLUUID root_prim_uuid = mManifest["linkset"][mLinkset][0].asUUID();
		LLSD& root_prim = mManifest["prim"][root_prim_uuid.asString()];
		LLVector3 root_prim_location(root_prim["position"]);
		mRootPosition = mStartPosition + (root_prim_location - mLinksetPosition);
		createPrim();
	}
}

void FSFloaterImport::setPrimPosition(U8 type, LLViewerObject* object, LLVector3 position, LLQuaternion rotation, LLVector3 scale)
{
	gMessageSystem->newMessage(_PREHASH_MultipleObjectUpdate);
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
	gMessageSystem->addU8Fast(_PREHASH_Type, type);
	U8 data[256];
	S32 offset = 0;

	if (type & UPD_POSITION)
	{
		htonmemcpy(&data[offset], &(position.mV), MVT_LLVector3, 12); 
		offset += 12;
	}
	if (type & UPD_ROTATION)
	{
		LLVector3 vec = rotation.packToVector3();
		htonmemcpy(&data[offset], &(vec.mV), MVT_LLQuaternion, 12); 
		offset += 12;
	}
	if (type & UPD_SCALE)
	{
		htonmemcpy(&data[offset], &(scale.mV), MVT_LLVector3, 12); 
		offset += 12;
	}
	gMessageSystem->addBinaryDataFast(_PREHASH_Data, data, offset);
	gMessageSystem->sendReliable(object->getRegion()->getHost());
}

void FSFloaterImport::uploadAsset(LLUUID asset_id, LLUUID inventory_item)
{
	bool temporary = false;
	std::vector<U8> asset_data = mManifest["asset"][asset_id.asString()]["data"].asBinary();
	std::string name = mManifest["asset"][asset_id.asString()]["name"].asString();
	std::string description = mManifest["asset"][asset_id.asString()]["description"].asString();
	LLAssetType::EType asset_type = LLAssetType::lookup(mManifest["asset"][asset_id.asString()]["type"].asString().c_str());
	std::string url;
	LLSD body = LLSD::emptyMap();
	LLFolderType::EType folder_type = LLFolderType::assetTypeToFolderType(asset_type);
	LLUUID folder_id = gInventory.findCategoryUUIDForType(folder_type);
	LLInventoryType::EType inventory_type = LLInventoryType::defaultForAssetType(asset_type);
	bool new_file_agent_inventory = false;
	LLWearableType::EType wearable_type = NOT_WEARABLE;

	if (name.empty())
	{
		name = llformat("Uploaded %s %s", LLAssetType::lookupHumanReadable(asset_type), asset_id.asString().c_str());
	}

	switch(asset_type)
	{
	case LLAssetType::AT_TEXTURE:
	{
		temporary = getChild<LLCheckBoxCtrl>("temp_asset")->get();
		if (temporary)
		{
			url = gAgent.getRegion()->getCapability("UploadBakedTexture");
		}
		else
		{
			url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
			new_file_agent_inventory = true;
		}
		LLTrace::add(LLStatViewer::UPLOAD_TEXTURE,1);
	}
		break;
	case LLAssetType::AT_SOUND:
	{
		temporary = getChild<LLCheckBoxCtrl>("temp_asset")->get();
		if (temporary)
		{
			// skip upload due to no temp support for sound
			pushNextAsset(LLUUID::null, asset_id, asset_type);
			return;
		}
		else
		{
			url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
			new_file_agent_inventory = true;
			LLTrace::add(LLStatViewer::UPLOAD_SOUND,1);
		}
		
	}
		break;
	case LLAssetType::AT_CLOTHING:
	case LLAssetType::AT_BODYPART:
	{
		std::string asset(asset_data.begin(), asset_data.end());

		S32 position = asset.rfind("type");
		S32 end = asset.find("\n", position);
		wearable_type = (LLWearableType::EType)boost::lexical_cast<S32>(asset.substr(position + 5, (end - (position + 5))));

		if (getChild<LLCheckBoxCtrl>("temp_asset")->get())
		{
			// wearables don't support using temp textures.
			break;
		}

		position = asset.rfind("textures");
		boost::regex pattern("[[:xdigit:]]{8}(-[[:xdigit:]]{4}){3}-[[:xdigit:]]{12}");
		boost::sregex_iterator m1(asset.begin() + position, asset.end(), pattern);
		boost::sregex_iterator m2;
		bool replace = false;
		for( ; m1 != m2; ++m1)
		{
			LL_DEBUGS("export") << "Found wearable texture " << m1->str() << LL_ENDL;
			if(LLUUID::validate(m1->str()))
			{
				LLUUID texture_id = LLUUID(m1->str());
				if (mAssetMap[texture_id].notNull())
				{
					asset.replace(m1->position(), m1->length(), mAssetMap[texture_id].asString());
					replace = true;
					LL_DEBUGS("export") << "Replaced wearable texture " << m1->str() << " with " << mAssetMap[texture_id].asString() << LL_ENDL;
				}
			}
			else
			{
				LL_DEBUGS("export") << "Invalied uuid: " << m1->str() << LL_ENDL;
			}
		}
		if (replace) 
		{
			std::copy(asset.begin(), asset.end(), asset_data.begin());
		}
	}
		break;
	case LLAssetType::AT_NOTECARD:
	{
		if (inventory_item.isNull())
		{
			// create inventory item first
			FSResourceData* ci_data = new FSResourceData;
			ci_data->uuid = asset_id;
			ci_data->mFloater = this;
			ci_data->asset_type = asset_type;
			ci_data->post_asset_upload = false;
			LLPointer<LLInventoryCallback> cb = new FSCreateItemCallback(ci_data);
			create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
					      folder_id, LLTransactionID::tnull, name, description, asset_type, inventory_type,
					      NOT_WEARABLE, PERM_ALL, cb);
			return;
		}
		else
		{
			url = gAgent.getRegion()->getCapability("UpdateNotecardAgentInventory");
			body["item_id"] = inventory_item;
		}
	}
		break;
	case LLAssetType::AT_LSL_TEXT:
	{
		if (inventory_item.isNull())
		{
			// create inventory item first
			FSResourceData* ci_data = new FSResourceData;
			ci_data->uuid = asset_id;
			ci_data->mFloater = this;
			ci_data->asset_type = asset_type;
			ci_data->post_asset_upload = false;
			LLPointer<LLInventoryCallback> cb = new FSCreateItemCallback(ci_data);
			create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
					      folder_id, LLTransactionID::tnull, name, description, asset_type, inventory_type,
					      NOT_WEARABLE, PERM_MOVE | PERM_TRANSFER, cb);
			return;
		}
		else
		{
			url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
			body["item_id"] = inventory_item;
			if (gSavedSettings.getBOOL("FSSaveInventoryScriptsAsMono"))
			{
				body["target"] = "mono";
			}
			else
			{
				body["target"] = "lsl2";
			}
		}
	}
		break;
	case LLAssetType::AT_ANIMATION:
	{
		temporary = getChild<LLCheckBoxCtrl>("temp_asset")->get();
		if (temporary)
		{
			// no temp support, skip
			pushNextAsset(LLUUID::null, asset_id, asset_type);
			return;
		}
		else
		{
			url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
			new_file_agent_inventory = true;
			LLTrace::add(LLStatViewer::ANIMATION_UPLOADS,1);
		}
	}
		break;
	case LLAssetType::AT_GESTURE:
	{
		if (inventory_item.isNull())
		{
			// create inventory item first
			FSResourceData* ci_data = new FSResourceData;
			ci_data->uuid = asset_id;
			ci_data->mFloater = this;
			ci_data->asset_type = asset_type;
			ci_data->post_asset_upload = false;
			LLPointer<LLInventoryCallback> cb = new FSCreateItemCallback(ci_data);
			create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
					      folder_id, LLTransactionID::tnull, name, description, asset_type, inventory_type,
					      NOT_WEARABLE, PERM_ALL, cb);
			return;
		}
		else
		{
			url = gAgent.getRegion()->getCapability("UpdateGestureAgentInventory");
			body["item_id"] = inventory_item;
			asset_data.push_back('\0');
			LLMultiGesture* gesture = new LLMultiGesture();
			LLDataPackerAsciiBuffer dp((char*)&asset_data[0], (S32)asset_data.size());
			if (!gesture->deserialize(dp))
			{
				LL_WARNS("export") << "Unable to load gesture " << asset_id << LL_ENDL;
				delete gesture;
				break;
			}

			S32 i;
			S32 count = gesture->mSteps.size();
			bool replace = false;
			for (i = 0; i < count; ++i)
			{
				LLGestureStep* step = gesture->mSteps[i];
				
				switch(step->getType())
				{
				case STEP_ANIMATION:
				{
					LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
					if (mAssetMap[anim_step->mAnimAssetID].notNull())
					{
						LL_DEBUGS("export") << "Replaced animation " << anim_step->mAnimAssetID.asString() << " with " << mAssetMap[anim_step->mAnimAssetID].asString() << LL_ENDL;
						anim_step->mAnimAssetID = mAssetMap[anim_step->mAnimAssetID];
						replace = true;
					}
				}
					break;
				case STEP_SOUND:
				{
					LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
					if (mAssetMap[sound_step->mSoundAssetID].notNull())
					{
						LL_DEBUGS("export") << "Replaced sound " << sound_step->mSoundAssetID.asString() << " with " << mAssetMap[sound_step->mSoundAssetID].asString() << LL_ENDL;
						sound_step->mSoundAssetID = mAssetMap[sound_step->mSoundAssetID];
						replace = true;
					}
				}
					break;
				default:
					break;
				}
			}
			if (replace) 
			{
				S32 max_size = gesture->getMaxSerialSize();
				char* buffer = new char[max_size];
				LLDataPackerAsciiBuffer dp(buffer, max_size);
				if (!gesture->serialize(dp))
				{
					LL_WARNS("export") << "Unable to serialize gesture " << asset_id << LL_ENDL;
					delete[] buffer;
					delete gesture;
					break;
				}
				S32 size = dp.getCurrentSize();
				asset_data.assign(buffer, buffer + size);
				delete[] buffer;
			}
			delete gesture;
		}
	}
		break;
	default:
	{
		url = gAgent.getRegion()->getCapability("NewFileAgentInventory");
		new_file_agent_inventory = true;
	}
		break;
	}

	LLTransactionID tid;
	tid.generate();
	LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

	LLVFile::writeFile((U8*)&asset_data[0], (S32)asset_data.size(), gVFS, new_asset_id, asset_type);

	LLResourceData* data( new LLResourceData );
	data->mAssetInfo.mTransactionID = tid;
	data->mAssetInfo.mUuid = new_asset_id;
	data->mAssetInfo.mType = asset_type;
	data->mAssetInfo.mCreatorID = gAgentID;
	data->mInventoryType = inventory_type;
	data->mNextOwnerPerm = LLFloaterPerms::getNextOwnerPerms();
	data->mExpectedUploadCost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
	FSResourceData* fs_data = new FSResourceData;
	fs_data->uuid = asset_id;
	fs_data->mFloater = this;
	fs_data->temporary = temporary;
	fs_data->inventory_item = inventory_item;
	fs_data->wearable_type = wearable_type;
	fs_data->asset_type = asset_type;
	data->mUserData = fs_data;
	data->mAssetInfo.setName(name);
	data->mAssetInfo.setDescription(description);
	data->mPreferredLocation = folder_type;

	if(!url.empty())
	{
		if (new_file_agent_inventory)
		{
			body["folder_id"] = folder_id;
			body["asset_type"] = LLAssetType::lookup(asset_type);
			body["inventory_type"] = LLInventoryType::lookup(inventory_type);
			body["name"] = name;
			body["description"] = description;
			body["next_owner_mask"] = LLSD::Integer(LLFloaterPerms::getNextOwnerPerms());
			body["group_mask"] = LLSD::Integer(LLFloaterPerms::getGroupPerms());
			body["everyone_mask"] = LLSD::Integer(LLFloaterPerms::getEveryonePerms());
		}
		

		std::shared_ptr< LLResourceData> pData( data, resourceDeleter );

		LLCoprocedureManager::instance().enqueueCoprocedure( "FSImporter", "Upload asset", boost::bind( uploadCoroutine, _1, url, body, new_asset_id, asset_type, pData ) );

		LL_DEBUGS("import") << "Asset upload via capability of " << new_asset_id.asString() << " to " << url << " of " << asset_id.asString() << LL_ENDL;
	}
	else
	{
		gAssetStorage->storeAssetData(tid,asset_type,FSFloaterImport::onAssetUploadComplete, data, temporary, temporary, temporary);
		LL_DEBUGS("import") << "Asset upload via AssetStorage of " << new_asset_id.asString() << " of " << asset_id.asString() << LL_ENDL;
	}
}

// static
void FSFloaterImport::onAssetUploadComplete(const LLUUID& uuid, void* userdata, S32 result, LLExtStat ext_status)
{
	LLResourceData* data = (LLResourceData*)userdata;
	if (!data)
	{
		LL_WARNS("import")  << "Got Asset upload complete without data info" << LL_ENDL;
		return;
	}

	FSResourceData* fs_data = (FSResourceData*)data->mUserData;
	if (!fs_data)
	{
		LL_WARNS("import")  << "Missing FSResourceData, can not continue." << LL_ENDL;
		return;
	}
	FSFloaterImport* self = fs_data->mFloater;
	LLUUID asset_id = uuid;

	if (result >= 0)
	{
		const LLUUID folder_id = gInventory.findCategoryUUIDForType(data->mPreferredLocation);
		
		if (fs_data->inventory_item.isNull())
		{
			if (fs_data->temporary)
			{
				LLUUID item_id;
				item_id.generate();
				LLPermissions perm;
				perm.init(gAgentID, gAgentID, gAgentID, gAgentID);
				perm.setMaskBase(PERM_ALL);
				perm.setMaskOwner(PERM_ALL);
				perm.setMaskEveryone(PERM_ALL);
				perm.setMaskGroup(PERM_ALL);
				LLPointer<LLViewerInventoryItem> item = new LLViewerInventoryItem(item_id, folder_id, perm,
												  data->mAssetInfo.mTransactionID.makeAssetID(gAgent.getSecureSessionID()),
												  data->mAssetInfo.mType, data->mInventoryType, data->mAssetInfo.getName(),
												  "Temporary asset", LLSaleInfo::DEFAULT, LLInventoryItemFlags::II_FLAGS_NONE,
												  time_corrected());
				gInventory.updateItem(item);
				gInventory.notifyObservers();
			}
			else
			{
				if (LLAssetType::AT_SOUND == data->mAssetInfo.mType ||
				    LLAssetType::AT_TEXTURE == data->mAssetInfo.mType ||
				    LLAssetType::AT_ANIMATION == data->mAssetInfo.mType)
				{
					// Charge the user for the upload.
					S32 expected_upload_cost = data->mExpectedUploadCost;
					LLViewerRegion* region = gAgent.getRegion();
					if(region)
					{
						// Charge user for upload
						gStatusBar->debitBalance(expected_upload_cost);
						
						LLMessageSystem* msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_MoneyTransferRequest);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_MoneyData);
						msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_DestID, LLUUID::null);
						msg->addU8("Flags", 0);
						// we tell the sim how much we were expecting to pay so it
						// can respond to any discrepancy
						msg->addS32Fast(_PREHASH_Amount, expected_upload_cost);
						msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
						msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
						msg->addS32Fast(_PREHASH_TransactionType, TRANS_UPLOAD_CHARGE);
						msg->addStringFast(_PREHASH_Description, NULL);
						msg->sendReliable(region->getHost());
					}
				}

				// Actually add the upload to inventory
				LL_DEBUGS("import")  << "Adding " << asset_id << " to inventory." << LL_ENDL;

				if(folder_id.notNull())
				{
					U32 next_owner_perms = data->mNextOwnerPerm;
					if(PERM_NONE == next_owner_perms)
					{
						next_owner_perms = PERM_MOVE | PERM_TRANSFER;
					}
					fs_data->post_asset_upload = true;
					fs_data->post_asset_upload_id = asset_id;
					LLPointer<LLInventoryCallback> cb = new FSCreateItemCallback(fs_data);
					create_inventory_item(gAgent.getID(), gAgent.getSessionID(),
							      folder_id, data->mAssetInfo.mTransactionID, data->mAssetInfo.getName(),
							      data->mAssetInfo.getDescription(), data->mAssetInfo.mType,
							      data->mInventoryType, fs_data->wearable_type, next_owner_perms,
							      cb);
					// delete data; // can this be done without deleting fs_data that still is in use?
					return;
				}
				else
				{
					LL_WARNS("import")  << "Can't find a folder to put asset in" << LL_ENDL;
				}  
			}
		}
		else
		{
			// Saving into user inventory
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(fs_data->inventory_item);
			if(item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setDescription(data->mAssetInfo.getDescription());
				new_item->setTransactionID(data->mAssetInfo.mTransactionID);
				new_item->setAssetUUID(asset_id);
				new_item->updateServer(FALSE);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
				LL_DEBUGS("import") << "Asset " << asset_id << " saved into "  << "inventory item " << item->getName() << LL_ENDL;
				self->mAssetItemMap[asset_id] = fs_data->inventory_item;
			}
			else
			{
				LL_WARNS("import") << "Inventory item for asset " << asset_id << " is no longer in agent inventory. Skipping to next asset upload." << LL_ENDL;
				asset_id = LLUUID::null;
			}
		}
	}
	else
	{
		LLSD args;
		args["FILE"] = LLInventoryType::lookupHumanReadable(data->mInventoryType);
		args["REASON"] = std::string(LLAssetStorage::getErrorString(result));
		LLNotificationsUtil::add("CannotUploadReason", args);
	}

	if (self)
	{
		self->pushNextAsset(asset_id, fs_data->uuid, data->mAssetInfo.mType);
	}
	else
	{
		LL_WARNS("import")  << "Unable to upload next asset due to missing self." << LL_ENDL;
	}

	delete fs_data;
	delete data;
}

void FSFloaterImport::pushNextAsset( LLUUID new_uuid, LLUUID asset_id, LLAssetType::EType asset_type )
{
	NextAsset oAsset;
	oAsset.new_uuid = new_uuid;
	oAsset.asset_id = asset_id;
	oAsset.asset_type = asset_type;
	m_NextAssets.push_back( oAsset );
}

void FSFloaterImport::popNextAsset()
{
	if( m_NextAssets.empty() )
		return;

	NextAsset oAsset = m_NextAssets.front();
	m_NextAssets.pop_front();

	LLUUID new_uuid = oAsset.new_uuid;
	LLUUID asset_id = oAsset.asset_id;
	LLAssetType::EType asset_type = oAsset.asset_type;

	if (gDisconnected)
	{
		// on logout, all assest uploads callbacks are fired.
		// this fixes a crash on logout if there are still pending uploads.
		// Crash is due to ~distructor is called for *this before the callbacks are fired.
		return;
	}

	switch(asset_type)
	{
	case LLAssetType::AT_TEXTURE:
	{
		uuid_vec_t::iterator iter = std::find(mTextureQueue.begin(), mTextureQueue.end(), asset_id);
		if ( iter != mTextureQueue.end())  
		{
			if (new_uuid.notNull())
			{
				mAssetMap[asset_id] = new_uuid;
			}
			mTextureQueue.erase(iter);
		}
		else
		{
			LL_WARNS("import") << "Error with texture upload, got callback without texture in mTextureQueue." << LL_ENDL;
			return;
		}
	}
		break;
	case LLAssetType::AT_SOUND:
	{
		uuid_vec_t::iterator iter = std::find(mSoundQueue.begin(), mSoundQueue.end(), asset_id);
		if ( iter != mSoundQueue.end())  
		{
			if (new_uuid.notNull())
			{
				mAssetMap[asset_id] = new_uuid;
			}
			mSoundQueue.erase(iter);
		}
		else
		{
			LL_WARNS("import") << "Error with sound upload, got callback without sound in mSoundQueue." << LL_ENDL;
			return;
		}
	}
		break;
	case LLAssetType::AT_ANIMATION:
	{
		uuid_vec_t::iterator iter = std::find(mAnimQueue.begin(), mAnimQueue.end(), asset_id);
		if ( iter != mAnimQueue.end())  
		{
			if (new_uuid.notNull())
			{
				mAssetMap[asset_id] = new_uuid;
			}
			mAnimQueue.erase(iter);
		}
		else
		{
			LL_WARNS("import") << "Error with animation upload, got callback without animation in mAnimQueue." << LL_ENDL;
			return;
		}
	}
		break;
	default:
	{
		uuid_vec_t::iterator iter = std::find(mAssetQueue.begin(), mAssetQueue.end(), asset_id);
		if ( iter != mAssetQueue.end())  
		{
			if (new_uuid.notNull())
			{
				mAssetMap[asset_id] = new_uuid;
			}
			mAssetQueue.erase(iter);
		}
		else
		{
			LL_WARNS("import") << "Error with asset upload, got callback without asset in mAssetQueue." << LL_ENDL;
			return;
		}
	}
		break;
	}
	
	if (!mTextureQueue.empty())
	{
		LLUIString status = getString("texture_uploading");
		status.setArg("[TEXTURE]", llformat("%u", mTexturesTotal - (U32)mTextureQueue.size() + 1));
		status.setArg("[TEXTURETOTAL]", llformat("%u", mTexturesTotal));
		getChild<LLTextBox>("file_status_text")->setText(status.getString());
		uploadAsset(mTextureQueue.front());
		return;
	}
	if (!mSoundQueue.empty())
	{
		LLUIString status = getString("sound_uploading");
		status.setArg("[SOUND]", llformat("%u", mSoundsTotal - (U32)mSoundQueue.size() + 1));
		status.setArg("[SOUNDTOTAL]", llformat("%u", mSoundsTotal));
		getChild<LLTextBox>("file_status_text")->setText(status.getString());
		uploadAsset(mSoundQueue.front());
		return;
	}
	if (!mAnimQueue.empty())
	{
		LLUIString status = getString("animation_uploading");
		status.setArg("[ANIMATION]", llformat("%u", mAnimsTotal - (U32)mAnimQueue.size() + 1));
		status.setArg("[ANIMATIONTOTAL]", llformat("%u", mAnimsTotal));
		getChild<LLTextBox>("file_status_text")->setText(status.getString());
		uploadAsset(mAnimQueue.front());
		return;
	}
	if (!mAssetQueue.empty())
	{
		LLUIString status = getString("asset_uploading");
		status.setArg("[ASSET]", llformat("%u", mAssetsTotal - (U32)mAssetQueue.size() + 1));
		status.setArg("[ASSETTOTAL]", llformat("%u", mAssetsTotal));
		getChild<LLTextBox>("file_status_text")->setText(status.getString());
		uploadAsset(mAssetQueue.front());
		return;
	}
	
	LL_DEBUGS("import") << "Nothing left in upload queue, proceding to prim import." << LL_ENDL;
	// restore setting to allow preview popups.
	gSavedSettings.setBOOL("ShowNewInventory", mSavedSettingShowNewInventory);
	importPrims();
}

FSCreateItemCallback::FSCreateItemCallback(FSResourceData* data) :
	mData(data)
{
}

void FSCreateItemCallback::fire(const LLUUID& inv_item)
{
	FSFloaterImport* self = mData->mFloater;

	if (inv_item.isNull())
	{
		LL_WARNS( "import" ) << "Item creation for asset failed, skipping " << mData->uuid.asString() << LL_ENDL;
		if (self)
		{
			self->pushNextAsset( LLUUID::null, mData->uuid, mData->asset_type );
		}
		else
		{
			LL_WARNS("import")  << "Unable to upload next asset due to missing self." << LL_ENDL;
		}
		return;
	}

	if( mData->post_asset_upload )
	{
		self->mAssetItemMap[ mData->post_asset_upload_id ] = inv_item;
		self->pushNextAsset( mData->post_asset_upload_id, mData->uuid, mData->asset_type );
	}
	else
	{
		self->uploadAsset( mData->uuid, inv_item );
	}
}

class UploadFlag
{
	FSFloaterImport *mFloater;
public:
	UploadFlag( FSFloaterImport *aFloater )
		: mFloater( aFloater )
	{ 
		if( mFloater )
			mFloater->startUpload();
	}

	~UploadFlag()
	{
		if( mFloater )
			mFloater->uploadDone();
	}
};

void uploadCoroutine( LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t &a_httpAdapter, std::string aURL, LLSD aBody, LLAssetID aAssetId, LLAssetType::EType aAssetType, std::shared_ptr< LLResourceData > aResourceData )
{
	FSResourceData* fs_data = (FSResourceData*)aResourceData->mUserData;
	FSFloaterImport* self = fs_data->mFloater;
	UploadFlag uploadDone( self );

	LLCore::HttpRequest::ptr_t pRequest( new LLCore::HttpRequest() );
	LLSD postResult = a_httpAdapter->postAndSuspend( pRequest, aURL, aBody );

	LLSD httpResults = postResult[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ];
	LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(httpResults);

	if( !status )
	{
		LL_WARNS( "import" ) << "Error " << status.toULong() << " reason: " << status.toString() << LL_ENDL;
		LL_WARNS( "import" ) << "Skipping " << fs_data->uuid.asString() << " due to upload error." << LL_ENDL;
		if( self )
		{
			self->pushNextAsset( LLUUID::null, fs_data->uuid, aResourceData->mAssetInfo.mType );
		}
		else
		{
			LL_WARNS( "import" ) << "Unable to upload next asset due to missing self." << LL_ENDL;
		}
	}

	std::string uploader = postResult[ "uploader" ].asString();

	if( uploader.empty() )
	{
		LL_WARNS( "import" ) << "No upload url provided. Nothing uploaded." << LL_ENDL;
		self->pushNextAsset( LLUUID::null, fs_data->uuid, aResourceData->mAssetInfo.mType );
		return;
	}

	if (!gVFS->getExists(aAssetId, aAssetType))
	{
		LL_WARNS() << "Asset doesn't exist in VFS anymore. Nothing uploaded." << LL_ENDL;
		self->pushNextAsset( LLUUID::null, fs_data->uuid, aResourceData->mAssetInfo.mType );
		return;
	}

	LLSD postContentResult;
	{
		pRequest.reset( new LLCore::HttpRequest() );

		postContentResult = a_httpAdapter->postFileAndSuspend( pRequest, uploader, aAssetId, aAssetType );
		LLSD httpResults = postContentResult[ LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS ];
		LLCore::HttpStatus status = LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD( httpResults );

		std::string ulstate = postContentResult[ "state" ].asString();
		

		if( (!status) || (ulstate != "complete") )
		{
			self->pushNextAsset( LLUUID::null, fs_data->uuid, aResourceData->mAssetInfo.mType );
			LL_WARNS( "import" ) << "Asset upload failed." << ll_pretty_print_sd( postContentResult) << LL_ENDL;
			return;
		}
	}

	LLUUID item_id = aBody[ "item_id" ];
	std::string responseResult = postContentResult[ "state" ];
	LLUUID new_id = postContentResult[ "new_asset" ];

	LL_DEBUGS( "import" ) << "result: " << responseResult << " new_id: " << new_id << LL_ENDL;

	// rename the file in the VFS to the actual asset id
	gVFS->renameFile(aAssetId, aAssetType, new_id, aAssetType);

	if( item_id.isNull() )
	{
		if( fs_data->temporary )
		{
			if( responseResult == "complete" )
			{
				LLUUID folder_id( gInventory.findCategoryUUIDForType( aResourceData->mPreferredLocation ) );

				LLUUID temp_item_id;
				temp_item_id.generate();
				LLPermissions perm;
				perm.init( gAgentID, gAgentID, gAgentID, gAgentID );
				perm.setMaskBase( PERM_ALL );
				perm.setMaskOwner( PERM_ALL );
				perm.setMaskEveryone( PERM_ALL );
				perm.setMaskGroup( PERM_ALL );

				LLPointer<LLViewerInventoryItem> item = new LLViewerInventoryItem(	temp_item_id, folder_id, perm, new_id, aResourceData->mAssetInfo.mType, aResourceData->mInventoryType, aResourceData->mAssetInfo.getName(),
																					"Temporary asset", LLSaleInfo::DEFAULT, LLInventoryItemFlags::II_FLAGS_NONE, time_corrected() );
				gInventory.updateItem( item );
				gInventory.notifyObservers();
			}
			else
			{
				LL_WARNS( "import" ) << "Unable to add texture, got bad result." << LL_ENDL;
				new_id = LLUUID::null;
			}
		}
		else
		{
			LLAssetType::EType asset_type = LLAssetType::lookup( aBody[ "asset_type" ].asString() );
			LLInventoryType::EType inventory_type = LLInventoryType::lookup( aBody[ "inventory_type" ].asString() );
			S32 upload_price = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();

			const std::string inventory_type_string = aBody[ "asset_type" ].asString();
			const LLUUID& item_folder_id = aBody[ "folder_id" ].asUUID();
			const std::string& item_name = aBody[ "name" ];
			const std::string& item_description = aBody[ "description" ];

			if( upload_price > 0 )
			{
				// this upload costed us L$, update our balance
				// and display something saying that it cost L$
				LLStatusBar::sendMoneyBalanceRequest();

				// <FS:CR> FIRE-10628 - Option to supress upload cost notification
				if( gSavedSettings.getBOOL( "FSShowUploadPaymentToast" ) )
				{
					LLSD args;
					args[ "AMOUNT" ] = llformat( "%d", upload_price );
					LLNotificationsUtil::add( "UploadPayment", args );
				}
				// </FS:CR>
			}

			if( item_folder_id.notNull() )
			{
				U32 everyone_perms = PERM_NONE;
				U32 group_perms = PERM_NONE;
				U32 next_owner_perms = PERM_ALL;
				if( postContentResult.has( "new_next_owner_mask" ) )
				{
					// The server provided creation perms so use them.
					// Do not assume we got the perms we asked for in
					// since the server may not have granted them all.
					everyone_perms = postContentResult[ "new_everyone_mask" ].asInteger();
					group_perms = postContentResult[ "new_group_mask" ].asInteger();
					next_owner_perms = postContentResult[ "new_next_owner_mask" ].asInteger();
				}
				else
				{
					// The server doesn't provide creation perms
					// so use old assumption-based perms.
					if( inventory_type_string != "snapshot" )
					{
						next_owner_perms = PERM_MOVE | PERM_TRANSFER;
					}
				}

				LLPermissions new_perms;
				new_perms.init( gAgent.getID(), gAgent.getID(), LLUUID::null, LLUUID::null );

				new_perms.initMasks( PERM_ALL, PERM_ALL, everyone_perms, group_perms, next_owner_perms );

				U32 inventory_item_flags = 0;
				if( postContentResult.has( "inventory_flags" ) )
				{
					inventory_item_flags = (U32)postContentResult[ "inventory_flags" ].asInteger();
					if( inventory_item_flags != 0 )
					{
						LL_INFOS() << "inventory_item_flags " << inventory_item_flags << LL_ENDL;
					}
				}
				S32 creation_date_now = time_corrected();

				LLPointer<LLViewerInventoryItem> item = new LLViewerInventoryItem( postContentResult[ "new_inventory_item" ].asUUID(), item_folder_id, new_perms, postContentResult[ "new_asset" ].asUUID(),
																					asset_type, inventory_type, item_name, item_description, LLSaleInfo::DEFAULT, inventory_item_flags, creation_date_now );

				gInventory.updateItem( item );
				gInventory.notifyObservers();
			}
			else
			{
				LL_WARNS( "import" ) << "Can't find a folder to put the texture in" << LL_ENDL;
			}
		}
	}
	else
	{
		LLViewerInventoryItem* item = (LLViewerInventoryItem*)gInventory.getItem( item_id );
		if( !item )
		{
			LL_WARNS( "import" ) << "Inventory item for " << aAssetId << " is no longer in agent inventory. Skipping to next asset upload." << LL_ENDL;
			new_id = LLUUID::null;
		}
		else
		{
			// Update viewer inventory item
			LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem( item );
			new_item->setAssetUUID( postContentResult[ "new_asset" ].asUUID() );
			gInventory.updateItem( new_item );
			gInventory.notifyObservers();

			LL_DEBUGS( "import" ) << "Asset " << postContentResult[ "new_asset" ].asString() << " saved into " << "inventory item " << item->getName() << LL_ENDL;
			self->mAssetItemMap[ postContentResult[ "new_asset" ].asUUID() ] = item_id;
		}
	}

	if( self )
	{
		self->pushNextAsset( new_id, fs_data->uuid, aResourceData->mAssetInfo.mType );
	}
	else
	{
		LL_WARNS( "import" ) << "Unable to upload next asset due to missing self." << LL_ENDL;
	}
}

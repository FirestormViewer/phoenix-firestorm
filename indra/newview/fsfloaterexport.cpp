/**
 * @file fsfloaterexport.cpp
 * @brief export selected objects to an file using LLSD.
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2013 Techwolf Lupindo
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterexport.h"

#include "lfsimfeaturehandler.h"
#include "llagent.h"
//#include "llagentconstants.h"
#include "llagentdata.h"
#include "llavatarnamecache.h"
#include "llbufferstream.h"
#include "llcallbacklist.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llfilepicker.h"
#include "llimagej2c.h"
#include "llinventoryfunctions.h"
#include "llmultigesture.h"
#include "llnotecard.h"
#include "llnotificationsutil.h"
#include "llscrollcontainer.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "llsdutil.h"
#include "lltexturectrl.h"
#include "lltrans.h"
#include "llversioninfo.h"
#include "llvfile.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerpartsource.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"
#include "fsexportperms.h"

#include "llfloaterreg.h"
#include "llappviewer.h"
#include "fscommon.h"

#include <boost/algorithm/string_regex.hpp>

const F32 MAX_TEXTURE_WAIT_TIME = 30.0f;
const F32 MAX_INVENTORY_WAIT_TIME = 30.0f;
const F32 MAX_ASSET_WAIT_TIME = 60.0f;

// static
void FSFloaterObjectExport::onIdle(void* user_data)
{
	FSFloaterObjectExport* self = (FSFloaterObjectExport*)user_data;
	self->onIdle();
}

void FSFloaterObjectExport::onIdle()
{
	switch(mExportState)
	{
	case IDLE:
		break;
	case INVENTORY_DOWNLOAD:
		if (gDisconnected)
		{
			return;
		}
		
		if (mInventoryRequests.empty())
		{
			mLastRequest = mAssetRequests.size();
			mWaitTimer.start();
			mExportState = ASSET_DOWNLOAD;
		}
		else if (mLastRequest != mInventoryRequests.size())
		{
			mWaitTimer.start();
			mLastRequest = mInventoryRequests.size();
			updateTitleProgress(INVENTORY_DOWNLOAD);
		}
		else if (mWaitTimer.getElapsedTimeF32() > MAX_INVENTORY_WAIT_TIME)
		{
			mWaitTimer.start();
			for (uuid_vec_t::const_iterator iter = mInventoryRequests.begin(); iter != mInventoryRequests.end(); ++iter)
			{
				LLViewerObject* object = gObjectList.findObject((*iter));
				object->dirtyInventory();
				object->requestInventory();
				
				LL_DEBUGS("export") << "re-requested inventory of " << (*iter).asString() << LL_ENDL;
			}
		}
		break;
	case ASSET_DOWNLOAD:
		if (gDisconnected)
		{
			return;
		}
		
		if (mAssetRequests.empty())
		{
			mLastRequest = mRequestedTexture.size();
			mWaitTimer.start();
			mExportState = TEXTURE_DOWNLOAD;
		}
		else if (mLastRequest != mAssetRequests.size())
		{
			mWaitTimer.start();
			mLastRequest = mAssetRequests.size();
			updateTitleProgress(ASSET_DOWNLOAD);
		}
		else if (mWaitTimer.getElapsedTimeF32() > MAX_ASSET_WAIT_TIME)
		{
			//abort for now
			LL_DEBUGS("export") << "Asset timeout with " << (S32)mAssetRequests.size() << " requests left." << LL_ENDL;
			for (uuid_vec_t::iterator iter = mAssetRequests.begin(); iter != mAssetRequests.end(); ++iter)
			{
				LL_DEBUGS("export") << "Asset: " << (*iter).asString() << LL_ENDL;
			}
			mAssetRequests.clear();
		}
		break;
	case TEXTURE_DOWNLOAD:
		if (gDisconnected)
		{
			return;
		}

		if(mRequestedTexture.empty())
		{
			mExportState = IDLE;
			if (!gIdleCallbacks.deleteFunction(onIdle, this))
			{
				LL_WARNS("export") << "Failed to delete idle callback" << LL_ENDL;
			}
			mWaitTimer.stop();

			llofstream file;
			file.open(mFilename.c_str(), std::ios_base::out | std::ios_base::binary);
			std::string zip_data = zip_llsd(mManifest);
			file.write(zip_data.data(), zip_data.size());
			file.close();
			LL_DEBUGS("export") << "Export finished and written to " << mFilename << LL_ENDL;
			
			LLSD args;
			args["FILENAME"] = mFilename;
			LLNotificationsUtil::add("ExportFinished", args);
			closeFloater();
		}
		else if (mLastRequest != mRequestedTexture.size())
		{
			mWaitTimer.start();
			mLastRequest = mRequestedTexture.size();
			updateTitleProgress(TEXTURE_DOWNLOAD);
		}
		else if (mWaitTimer.getElapsedTimeF32() > MAX_TEXTURE_WAIT_TIME)
		{
			mWaitTimer.start();
			for (std::map<LLUUID, FSAssetResourceData>::iterator iter = mRequestedTexture.begin(); iter != mRequestedTexture.end(); ++iter)
			{
				LLUUID texture_id = iter->first;
				LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(texture_id, FTT_DEFAULT, MIPMAP_TRUE);
				image->setBoostLevel(LLViewerTexture::BOOST_MAX_LEVEL);
				image->forceToSaveRawImage(0);
				image->setLoadedCallback(FSFloaterObjectExport::onImageLoaded, 0, TRUE, FALSE, this, &mCallbackTextureList);

				LL_DEBUGS("export") << "re-requested texture " << texture_id.asString() << LL_ENDL;
			}
		}
		break;
	default:
		break;
	}
}

FSFloaterObjectExport::FSFloaterObjectExport(const LLSD& key)
: LLFloater(key),
  mCurrentObjectID(NULL),
  mDirty(true)
{
}

FSFloaterObjectExport::~FSFloaterObjectExport()
{
	if (gIdleCallbacks.containsFunction(FSFloaterObjectExport::onIdle, this))
	{
		gIdleCallbacks.deleteFunction(FSFloaterObjectExport::onIdle, this);
	}
}

BOOL FSFloaterObjectExport::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("selected_objects");
	mTexturePanel = getChild<LLPanel>("textures_panel");
	childSetAction("export_btn", boost::bind(&FSFloaterObjectExport::onClickExport, this));
	
	LLSelectMgr::getInstance()->mUpdateSignal.connect(boost::bind(&FSFloaterObjectExport::updateSelection, this));
	
	return TRUE;
}

void FSFloaterObjectExport::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = false;
	}
	LLFloater::draw();
}

void FSFloaterObjectExport::refresh()
{
	addSelectedObjects();
	addTexturePreview();
	populateObjectList();
	updateUI();
}

void FSFloaterObjectExport::dirty()
{
	mDirty = true;
}

void FSFloaterObjectExport::onOpen(const LLSD& key)
{
	LLObjectSelectionHandle object_selection = LLSelectMgr::getInstance()->getSelection();
	if(!(object_selection->getPrimaryObject()))
	{
		closeFloater();
		return;
	}
	mObjectSelection = LLSelectMgr::getInstance()->getEditSelection();
	refresh();
}

void FSFloaterObjectExport::updateSelection()
{
	LLObjectSelectionHandle object_selection = LLSelectMgr::getInstance()->getSelection();
	LLSelectNode* node = object_selection->getFirstRootNode();
	
	if (node && !node->mValid && node->getObject()->getID() == mCurrentObjectID)
	{
		return;
	}
		
	mObjectSelection = object_selection;
	dirty();
	refresh();
}

bool FSFloaterObjectExport::exportSelection()
{
	if (!mObjectSelection)
	{
		LL_WARNS("export") << "Nothing selected; Bailing!" << LL_ENDL;
		return false;
	}
	LLObjectSelection::valid_root_iterator iter = mObjectSelection->valid_root_begin();
	LLSelectNode* node = *iter;
	if (!node)
	{
		LL_WARNS("export") << "No node selected; Bailing!" << LL_ENDL;
		return false;
	}
	mFilePath = gDirUtilp->getDirName(mFilename);
	
	mManifest.clear();
	mRequestedTexture.clear();

	mExported = false;
	mAborted = false;
	mInventoryRequests.clear();
	mAssetRequests.clear();
	mTextureChecked.clear();
	
	std::string author = "Unknown";
	if (!gAgentUsername.empty())
		author = gAgentUsername;

	time_t rawtime;
	time(&rawtime);
	struct tm* utc_time = gmtime(&rawtime);
	std::string date = llformat("%04d-%02d-%02d", utc_time->tm_year + 1900, utc_time->tm_mon + 1, utc_time->tm_mday);
	mManifest["format_version"] = OXP_FORMAT_VERSION;
	mManifest["client"] = LLVersionInfo::getChannelAndVersion();
	mManifest["creation_date"] = date;
	mManifest["author"] = author;
	mManifest["grid"] = LLGridManager::getInstance()->getGridLabel();
	
	for ( ; iter != mObjectSelection->valid_root_end(); ++iter)
	{
		mManifest["linkset"].append(getLinkSet((*iter)));
	}

	if (mExported && !mAborted)
	{
		mWaitTimer.start();
		mLastRequest = mInventoryRequests.size();
		mExportState = INVENTORY_DOWNLOAD;
		gIdleCallbacks.addFunction(onIdle, this);
	}
	else
	{
		LL_WARNS("export") << "Nothing was exported. File not created." << LL_ENDL;
		return false;
	}
	return true;
}

LLSD FSFloaterObjectExport::getLinkSet(LLSelectNode* node)
{
	LLSD linkset;
	LLViewerObject* object = node->getObject();
	LLUUID object_id = object->getID();

	// root prim
	linkset.append(object_id);
	addPrim(object, true);

	// child prims
	LLViewerObject::const_child_list_t& child_list = object->getChildren();
	for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
		 iter != child_list.end(); ++iter)
	{
		LLViewerObject* child = *iter;
		linkset.append(child->getID());
		addPrim(child, false);
	}

	return linkset;
}

void FSFloaterObjectExport::addPrim(LLViewerObject* object, bool root)
{
	LLSD prim;
	LLUUID object_id = object->getID();
	bool default_prim = true;

	struct f : public LLSelectedNodeFunctor
	{
		LLUUID mID;
		f(const LLUUID& id) : mID(id) {}
		virtual bool apply(LLSelectNode* node)
		{
			return (node->getObject() && node->getObject()->mID == mID);
		}
	} func(object_id);
	
	LLSelectNode* node = LLSelectMgr::getInstance()->getSelection()->getFirstNode(&func);
	default_prim = (!FSExportPermsCheck::canExportNode(node, false));

	if (root)
	{
		if (object->isAttachment())
		{
			prim["attachment_point"] = ATTACHMENT_ID_FROM_STATE(object->getState());
		}
	}
	else
	{
		LLViewerObject* parent_object = (LLViewerObject*)object->getParent();
		prim["parent"] = parent_object->getID();
	}
	prim["position"] = object->getPosition().getValue();
	prim["scale"] = object->getScale().getValue();
	prim["rotation"] = ll_sd_from_quaternion(object->getRotation());

	if (default_prim)
	{
		LL_DEBUGS("export") << object_id.asString() << " failed export check. Using default prim" << LL_ENDL;
		prim["flags"] = ll_sd_from_U32((U32)0);
		prim["volume"]["path"] = LLPathParams().asLLSD();
		prim["volume"]["profile"] = LLProfileParams().asLLSD();
		prim["material"] = (S32)LL_MCODE_WOOD;
	}
	else
	{
		mExported = true;
		prim["flags"] = ll_sd_from_U32(object->getFlags());
		prim["volume"]["path"] = object->getVolume()->getParams().getPathParams().asLLSD();
		prim["volume"]["profile"] = object->getVolume()->getParams().getProfileParams().asLLSD();
		prim["material"] = (S32)object->getMaterial();
		if (object->getClickAction() != 0)
		{
			prim["clickaction"] = (S32)object->getClickAction();
		}

		LLVOVolume *volobjp = NULL;
		if (object->getPCode() == LL_PCODE_VOLUME)
		{
			volobjp = (LLVOVolume *)object;
		}
		if(volobjp)
		{
			if(volobjp->isSculpted())
			{
				const LLSculptParams *sculpt_params = (const LLSculptParams *)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
				if (sculpt_params)
				{
					if(volobjp->isMesh())
					{
						if (!mAborted)
						{
							mAborted = true;
						}
						return;
					}
					else
					{
						if (exportTexture(sculpt_params->getSculptTexture()))
						{
							prim["sculpt"] = sculpt_params->asLLSD();
						}
						else
						{
							LLSculptParams default_sculpt;
							prim["sculpt"] = default_sculpt.asLLSD();
						}
					}
				}
			}

			if(volobjp->isFlexible())
			{
				const LLFlexibleObjectData *flexible_param_block = (const LLFlexibleObjectData *)object->getParameterEntry(LLNetworkData::PARAMS_FLEXIBLE);
				if (flexible_param_block)
				{
					prim["flexible"] = flexible_param_block->asLLSD();
				}
			}

			if (volobjp->getIsLight())
			{
				const LLLightParams *light_param_block = (const LLLightParams *)object->getParameterEntry(LLNetworkData::PARAMS_LIGHT);
				if (light_param_block)
				{
					prim["light"] = light_param_block->asLLSD();
				}
			}

			if (volobjp->hasLightTexture())
			{
				const LLLightImageParams* light_image_param_block = (const LLLightImageParams*)object->getParameterEntry(LLNetworkData::PARAMS_LIGHT_IMAGE);
				if (light_image_param_block)
				{
					prim["light_texture"] = light_image_param_block->asLLSD();
				}
			}
			
		}

		if(object->isParticleSource())
		{
			LLViewerPartSourceScript* partSourceScript = object->getPartSourceScript();
			prim["particle"] = partSourceScript->mPartSysData.asLLSD();
			if (!exportTexture(partSourceScript->mPartSysData.mPartImageID))
			{
				prim["particle"]["PartImageID"] = LLUUID::null.asString();
			}
		}

		U8 texture_count = object->getNumTEs();
		for(U8 i = 0; i < texture_count; ++i)
		{
			LLTextureEntry *checkTE = object->getTE(i);
			LL_DEBUGS("export") << "Checking texture number " << (S32)i
				<< ", ID " << checkTE->getID() << LL_ENDL;
			if (FSCommon::isDefaultTexture(checkTE->getID()))	// <FS:CR> Check against default textures
			{
				LL_DEBUGS("export") << "...is a default texture." << LL_ENDL;
				prim["texture"].append(checkTE->asLLSD());
			}
			else if (exportTexture(checkTE->getID()))
			{
				LL_DEBUGS("export") << "...export check passed." << LL_ENDL;
				prim["texture"].append(checkTE->asLLSD());
			}
			else
			{
				LL_DEBUGS("export") << "...export check failed." << LL_ENDL;
				checkTE->setID(LL_DEFAULT_WOOD_UUID); // TODO: use user option of default texture.
				prim["texture"].append(checkTE->asLLSD());
			}
			
			// [FS:CR] Materials support
			if (checkTE->getMaterialParams().notNull())
			{
				LL_DEBUGS("export") << "found materials. Checking permissions..." << LL_ENDL;
				LLSD params = checkTE->getMaterialParams().get()->asLLSD();
				/// *TODO: Feeling lazy so I made it check both. This is incorrect and needs to be expanded
				/// to retain exportable textures not just failing both when one is non-exportable (or unset).
				if (exportTexture(params["NormMap"].asUUID()) &&
					exportTexture(params["SpecMap"].asUUID()))
				{
					LL_DEBUGS("export") << "...passed check." << LL_ENDL;
					prim["materials"].append(params);
				}
			}
		}

		if (!object->getPhysicsShapeUnknown())
		{
			prim["ExtraPhysics"]["PhysicsShapeType"] = (S32)object->getPhysicsShapeType();
			prim["ExtraPhysics"]["Density"] = (F64)object->getPhysicsDensity();
			prim["ExtraPhysics"]["Friction"] = (F64)object->getPhysicsFriction();
			prim["ExtraPhysics"]["Restitution"] = (F64)object->getPhysicsRestitution();
			prim["ExtraPhysics"]["GravityMultiplier"] = (F64)object->getPhysicsGravity();
		}

		prim["name"] = node->mName;
		prim["description"] = node->mDescription;
		prim["creation_date"] = ll_sd_from_U64(node->mCreationDate);

		LLAvatarName avatar_name;
		LLUUID creator_id = node->mPermissions->getCreator();
		if (creator_id.notNull())
		{
			prim["creator_id"] = creator_id;
			if (LLAvatarNameCache::get(creator_id, &avatar_name))
			{
				prim["creator_name"] = avatar_name.asLLSD();
			}
		}
		LLUUID owner_id = node->mPermissions->getOwner();
		if (owner_id.notNull())
		{
			prim["owner_id"] = owner_id;
			if (LLAvatarNameCache::get(owner_id, &avatar_name))
			{
				prim["owner_name"] = avatar_name.asLLSD();
			}
		}
		LLUUID group_id = node->mPermissions->getGroup();
		if (group_id.notNull())
		{
			prim["group_id"] = group_id;
			if (LLAvatarNameCache::get(group_id, &avatar_name))
			{
				prim["group_name"] = avatar_name.asLLSD();
			}
		}
		LLUUID last_owner_id = node->mPermissions->getLastOwner();
		if (last_owner_id.notNull())
		{
			prim["last_owner_id"] = last_owner_id;
			if (LLAvatarNameCache::get(last_owner_id, &avatar_name))
			{
				prim["last_owner_name"] = avatar_name.asLLSD();
			}
		}
		prim["base_mask"] = ll_sd_from_U32(node->mPermissions->getMaskBase());
		prim["owner_mask"] = ll_sd_from_U32(node->mPermissions->getMaskOwner());
		prim["group_mask"] = ll_sd_from_U32(node->mPermissions->getMaskGroup());
		prim["everyone_mask"] = ll_sd_from_U32(node->mPermissions->getMaskEveryone());
		prim["next_owner_mask"] = ll_sd_from_U32(node->mPermissions->getMaskNextOwner());
		
		prim["sale_info"] = node->mSaleInfo.asLLSD();
		prim["touch_name"] = node->mTouchName;
		prim["sit_name"] = node->mSitName;

		static LLCachedControl<bool> sExportContents(gSavedSettings, "FSExportContents");
		if (sExportContents)
		{
			mInventoryRequests.push_back(object_id);
			object->registerInventoryListener(this, NULL);
			object->dirtyInventory();
			object->requestInventory();
		}
	}

	mManifest["prim"][object_id.asString()] = prim;
}

bool FSFloaterObjectExport::exportTexture(const LLUUID& texture_id)
{
	if(texture_id.isNull())
	{
		LL_WARNS("export") << "Attempted to export NULL texture." << LL_ENDL;
		return false;
	}
	
	if (mTextureChecked.count(texture_id) != 0)
	{
		return mTextureChecked[texture_id];
	}
	
	if (gAssetStorage->mStaticVFS->getExists(texture_id, LLAssetType::AT_TEXTURE))
	{
		LL_DEBUGS("export") << "Texture " << texture_id.asString() << " is local static." << LL_ENDL;
		// no need to save the texture data as the viewer already has it in a local file.
		mTextureChecked[texture_id] = true;
		return true;
	}
	
	//TODO: check for local file static texture. The above will only get the static texture in the static db, not individual textures.

	LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(texture_id);
	bool texture_export = false;
	std::string name;
	std::string description;
	
	if (LLGridManager::getInstance()->isInSecondLife())
	{
		if (imagep->mComment.find("a") != imagep->mComment.end())
		{
			if (LLUUID(imagep->mComment["a"]) == gAgentID)
			{
				texture_export = true;
				LL_DEBUGS("export") << texture_id <<  " pass texture export comment check." << LL_ENDL;
			}
		}
	}

	if (texture_export)
	{
		FSExportPermsCheck::canExportAsset(texture_id, &name, &description);
	}
	else
	{
		texture_export = FSExportPermsCheck::canExportAsset(texture_id, &name, &description);
	}

	mTextureChecked[texture_id] = texture_export;
	
	if (!texture_export)
	{
		LL_DEBUGS("export") << "Texture " << texture_id << " failed export check." << LL_ENDL;
		return false;
	}

	LL_DEBUGS("export") << "Loading image texture " << texture_id << LL_ENDL;
	mRequestedTexture[texture_id].name = name;
	mRequestedTexture[texture_id].description = description;
	LLViewerFetchedTexture* image = LLViewerTextureManager::getFetchedTexture(texture_id, FTT_DEFAULT, MIPMAP_TRUE);
	image->setBoostLevel(LLViewerTexture::BOOST_MAX_LEVEL);
	image->forceToSaveRawImage(0);
	image->setLoadedCallback(FSFloaterObjectExport::onImageLoaded, 0, TRUE, FALSE, this, &mCallbackTextureList);

	return true;
}

// static
void FSFloaterObjectExport::onImageLoaded(BOOL success, LLViewerFetchedTexture* src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	if(final && success)
	{
		// *HACK ALERT: I'm lazy so I moved this to a non-static member function. <FS:CR>
		FSFloaterObjectExport* parent = (FSFloaterObjectExport *)userdata;
		parent->fetchTextureFromCache(src_vi);
	}
}

void FSFloaterObjectExport::fetchTextureFromCache(LLViewerFetchedTexture* src_vi)
{
	const LLUUID& texture_id = src_vi->getID();
	LLImageJ2C* mFormattedImage = new LLImageJ2C;
	FSFloaterObjectExport::FSExportCacheReadResponder* responder = new FSFloaterObjectExport::FSExportCacheReadResponder(texture_id, mFormattedImage, this);
	LLAppViewer::getTextureCache()->readFromCache(texture_id, LLWorkerThread::PRIORITY_HIGH, 0, 999999, responder);
	LL_DEBUGS("export") << "Fetching " << texture_id << " from the TextureCache" << LL_ENDL;
}

void FSFloaterObjectExport::removeRequestedTexture(LLUUID texture_id)
{
	if (mRequestedTexture.count(texture_id) != 0)
	{
		mRequestedTexture.erase(texture_id);
	}
}

void FSFloaterObjectExport::saveFormattedImage(LLPointer<LLImageFormatted> mFormattedImage, LLUUID id)
{
	std::stringstream texture_str;
	texture_str.write((const char*) mFormattedImage->getData(), mFormattedImage->getDataSize());
	std::string str = texture_str.str();

	mManifest["asset"][id.asString()]["name"] = mRequestedTexture[id].name;
	mManifest["asset"][id.asString()]["description"] = mRequestedTexture[id].description;
	mManifest["asset"][id.asString()]["type"] = LLAssetType::lookup(LLAssetType::AT_TEXTURE);
	mManifest["asset"][id.asString()]["data"] = LLSD::Binary(str.begin(),str.end());
	
	removeRequestedTexture(id);
}

void FSFloaterObjectExport::inventoryChanged(LLViewerObject* object, LLInventoryObject::object_list_t* inventory, S32 serial_num, void* user_data)
{
	uuid_vec_t::iterator v_iter = std::find(mInventoryRequests.begin(), mInventoryRequests.end(), object->getID());
	if (v_iter != mInventoryRequests.end())
	{
		LL_DEBUGS("export") << "Erasing inventory request " << object->getID() << LL_ENDL;
		mInventoryRequests.erase(v_iter);
	}
  
	LLSD& prim = mManifest["prim"][object->getID().asString()];
	for (LLInventoryObject::object_list_t::const_iterator iter = inventory->begin(); iter != inventory->end(); ++iter)
	{
		LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(iter->get());
		if (!item)
		{
			continue;
		}

		bool exportable = false;
		LLPermissions perms = item->getPermissions();
#ifdef OPENSIM
		if (LLGridManager::getInstance()->isInOpenSim())
		{
			switch (LFSimFeatureHandler::instance().exportPolicy())
			{
				case EXPORT_ALLOWED:
					exportable = (perms.getMaskOwner() & PERM_EXPORT) == PERM_EXPORT;
					break;
					/// TODO: Once enough grids adopt a version supporting exports, get consensus
					/// on whether we should allow full perm exports anymore.
				case EXPORT_UNDEFINED:
					exportable = (perms.getMaskBase() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED;
					break;
				case EXPORT_DENIED:
				default:
					exportable = perms.getCreator() == gAgentID;
					break;
			}
		}
#endif
		if (LLGridManager::getInstance()->isInSecondLife() && (perms.getCreator() == gAgentID))
		{
			exportable = true;
		}

		if (!exportable)
		{
			//<FS:TS> Only complain if we're trying to export a non-NULL item and fail
			if (!item->getUUID().isNull())
			{
				LL_DEBUGS("export") << "Item " << item->getName() << ", UUID " << item->getUUID() << " failed export check." << LL_ENDL;
			}
			continue;
		}

		if (item->getType() == LLAssetType::AT_NONE || item->getType() == LLAssetType::AT_OBJECT)
		{
			// currentelly not exportable
			LL_DEBUGS("export") << "Skipping " << LLAssetType::lookup(item->getType()) << " item " << item->getName() << LL_ENDL;
			continue;
		}

		prim["content"].append(item->getUUID());
		mManifest["inventory"][item->getUUID().asString()] = ll_create_sd_from_inventory_item(item);
		
		if (item->getAssetUUID().isNull() && item->getType() == LLAssetType::AT_NOTECARD)
		{
			// FIRE-9655
			// Blank Notecard item can have NULL asset ID.
			// Generate a new UUID and save as an empty asset.
			LLUUID asset_uuid = LLUUID::generateNewID();
			mManifest["inventory"][item->getUUID().asString()]["asset_id"] = asset_uuid;
			
			mManifest["asset"][asset_uuid.asString()]["name"] = item->getName();
			mManifest["asset"][asset_uuid.asString()]["description"] = item->getDescription();
			mManifest["asset"][asset_uuid.asString()]["type"] = LLAssetType::lookup(item->getType());

			LLNotecard nc(255); //don't need to allocate default size of 65536
			std::stringstream out_stream;
			nc.exportStream(out_stream);
			std::string out_string = out_stream.str();
			std::vector<U8> buffer(out_string.begin(), out_string.end());
			mManifest["asset"][asset_uuid.asString()]["data"] = buffer;
		}
		else
		{
			LL_DEBUGS("export") << "Requesting asset " << item->getAssetUUID() << " for item " << item->getUUID() << LL_ENDL;
			mAssetRequests.push_back(item->getUUID());
			FSAssetResourceData* data = new FSAssetResourceData;
			data->name = item->getName();
			data->description = item->getDescription();
			data->user_data = this;
			data->uuid = item->getUUID();

			if (item->getAssetUUID().isNull() || item->getType() == LLAssetType::AT_NOTECARD || item->getType() == LLAssetType::AT_LSL_TEXT)
			{
				gAssetStorage->getInvItemAsset(object->getRegion()->getHost(),
							      gAgent.getID(),
							      gAgent.getSessionID(),
							      item->getPermissions().getOwner(),
							      object->getID(),
							      item->getUUID(),
							      item->getAssetUUID(),
							      item->getType(),
							      onLoadComplete,
							      data,
							      TRUE);
			}
			else
			{
				gAssetStorage->getAssetData(item->getAssetUUID(),
							    item->getType(),
							    onLoadComplete,
							    data,
							    TRUE);
			}
		}
	}
	object->removeInventoryListener(this);
}

// static
void FSFloaterObjectExport::onLoadComplete(LLVFS* vfs, const LLUUID& asset_uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status)
{
	FSAssetResourceData* data = (FSAssetResourceData*)user_data;
	FSFloaterObjectExport* self = (FSFloaterObjectExport*)data->user_data;
	LLUUID item_uuid = data->uuid;
	self->removeRequestedAsset(item_uuid);
	
	if (status != 0)
	{
		LL_WARNS("export") << "Problem fetching asset: " << asset_uuid << " " << status << " " << ext_status << LL_ENDL;
		delete data;
		return;
	}

	LL_DEBUGS("export") << "Saving asset " << asset_uuid.asString() << " of item " << item_uuid.asString() << LL_ENDL;
	LLVFile file(vfs, asset_uuid, type);
	S32 file_length = file.getSize();
	std::vector<U8> buffer(file_length);
	file.read(&buffer[0], file_length);
	self->mManifest["asset"][asset_uuid.asString()]["name"] = data->name;
	self->mManifest["asset"][asset_uuid.asString()]["description"] = data->description;
	self->mManifest["asset"][asset_uuid.asString()]["type"] = LLAssetType::lookup(type);
	self->mManifest["asset"][asset_uuid.asString()]["data"] = buffer;

	if (self->mManifest["inventory"].has(item_uuid.asString()))
	{
		if (self->mManifest["inventory"][item_uuid.asString()]["asset_id"].asUUID().isNull())
		{
			self->mManifest["inventory"][item_uuid.asString()]["asset_id"] = asset_uuid;
		}
	}

	switch(type)
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
				self->exportTexture(LLUUID(m1->str()));
			}
			else
			{
				LL_DEBUGS("export") << "Invalid uuid: " << m1->str() << LL_ENDL;
			}
		}
	}
		break;
	case LLAssetType::AT_GESTURE:
	{
		buffer.push_back('\0');
		LLMultiGesture* gesture = new LLMultiGesture();
		LLDataPackerAsciiBuffer dp((char*)&buffer[0], file_length+1);
		if (!gesture->deserialize(dp))
		{
			LL_WARNS("export") << "Unable to load gesture " << asset_uuid << LL_ENDL;
			delete gesture;
			gesture = NULL;
			break;
		}
		std::string name;
		std::string description;
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
				if (!FSExportPermsCheck::canExportAsset(anim_step->mAnimAssetID, &name, &description))
				{
					LL_DEBUGS("export") << "Asset in gesture " << data->name << " failed export check." << LL_ENDL;
					break;
				}
				
				FSAssetResourceData* anim_data = new FSAssetResourceData;
				anim_data->name = anim_step->mAnimName;
				anim_data->user_data = self;
				anim_data->uuid = anim_step->mAnimAssetID;

				LL_DEBUGS("export") << "Requesting animation asset " << anim_step->mAnimAssetID.asString() << LL_ENDL;
				self->mAssetRequests.push_back(anim_step->mAnimAssetID);
				gAssetStorage->getAssetData(anim_step->mAnimAssetID,
							    LLAssetType::AT_ANIMATION,
							    onLoadComplete,
							    anim_data,
							    TRUE);
			}
				break;
			case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				if (!FSExportPermsCheck::canExportAsset(sound_step->mSoundAssetID, &name, &description))
				{
					LL_DEBUGS("export") << "Asset in gesture " << data->name << " failed export check." << LL_ENDL;
					break;
				}
				
				FSAssetResourceData* sound_data = new FSAssetResourceData;
				sound_data->name = sound_step->mSoundName;
				sound_data->user_data = self;
				sound_data->uuid = sound_step->mSoundAssetID;

				LL_DEBUGS("export") << "Requesting sound asset " << sound_step->mSoundAssetID.asString() << LL_ENDL;
				self->mAssetRequests.push_back(sound_step->mSoundAssetID);
				gAssetStorage->getAssetData(sound_step->mSoundAssetID,
							    LLAssetType::AT_SOUND,
							    onLoadComplete,
							    sound_data,
							    TRUE);
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

	delete data;
}

void FSFloaterObjectExport::removeRequestedAsset(LLUUID asset_uuid)
{
	uuid_vec_t::iterator iter = std::find(mAssetRequests.begin(), mAssetRequests.end(), asset_uuid);
	if (iter != mAssetRequests.end())
	{
		LL_DEBUGS("export") << "Erasing asset request " << asset_uuid.asString() << LL_ENDL;
		mAssetRequests.erase(iter);
	}
}

void FSFloaterObjectExport::addObject(const LLViewerObject* prim, const std::string name)
{
	mObjects.push_back(std::pair<LLViewerObject*,std::string>((LLViewerObject*)prim, name));
}

// <FS:CR> *TODO: I know it's really lame to tack this in here, maybe someday it can be integrated properly.
void FSFloaterObjectExport::updateTextureInfo()
{
	mTextures.clear();
	//mTextureNames.clear();
	
	for (obj_info_t::iterator obj_iter = mObjects.begin(); obj_iter != mObjects.end(); ++obj_iter)
	{
		LLViewerObject* obj = obj_iter->first;
		S32 num_faces = obj->getVolume()->getNumVolumeFaces();
		for (S32 face_num = 0; face_num < num_faces; ++face_num)
		{
			LLTextureEntry* te = obj->getTE(face_num);
			const LLUUID id = te->getID();
			
			if (std::find(mTextures.begin(), mTextures.end(), id) != mTextures.end()) continue;
			
			mTextures.push_back(id);
			bool exportable = false;
			LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(id);
			std::string name;
			std::string description;
			if (LLGridManager::getInstance()->isInSecondLife())
			{
				if (imagep->mComment.find("a") != imagep->mComment.end())
				{
					if (LLUUID(imagep->mComment["a"]) == gAgentID)
					{
						exportable = true;
						LL_DEBUGS("export") << id <<  " passed texture export comment check." << LL_ENDL;
					}
				}
			}
			if (exportable)
				FSExportPermsCheck::canExportAsset(id, &name, &description);
			else
				exportable = FSExportPermsCheck::canExportAsset(id, &name, &description);
			
			if (exportable)
			{
				//std::string safe_name = gDirUtilp->getScrubbedFileName(name);
				//std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
				mTextureNames.push_back(name);
			}
			else
			{
				mTextureNames.push_back(std::string());
			}
		}
	}
}

void FSFloaterObjectExport::updateTitleProgress(FSExportState state)
{
	LLUIString title;
	switch (state)
	{
		case INVENTORY_DOWNLOAD:
		{
			title = getString("title_inventory");
			break;
		}
		case ASSET_DOWNLOAD:
		{
			title = getString("title_assets");
			break;
		}
		case TEXTURE_DOWNLOAD:
		{
			title = getString("title_textures");
			break;
		}
		case IDLE:
		default:
			LL_WARNS("export") << "Unhandled case: " << state << LL_ENDL;
			return;
	}
	LLSD args;
	args["OBJECT"] = mObjectName;
	title.setArgs(args);
	setTitle(title);
}

void FSFloaterObjectExport::updateUI()
{
	childSetTextArg("NameText", "[NAME]", mObjectName);
	childSetTextArg("exportable_prims", "[COUNT]", llformat("%d", mIncluded));
	childSetTextArg("exportable_prims", "[TOTAL]", llformat("%d", mTotal));
	childSetTextArg("exportable_textures", "[COUNT]", llformat("%d", mNumExportableTextures));
	childSetTextArg("exportable_textures", "[TOTAL]", llformat("%d", mNumTextures));
	
	LLUIString title = getString("title_floater");
	title.setArg("[OBJECT]", mObjectName);
	setTitle(title);
	childSetEnabled("export_textures_check", mNumExportableTextures);
	childSetEnabled("export_btn", mIncluded);
}

void FSFloaterObjectExport::onClickExport()
{
	LLFilePicker& file_picker = LLFilePicker::instance();
	if(!file_picker.getSaveFile(LLFilePicker::FFSAVE_EXPORT, LLDir::getScrubbedFileName(mObjectName + ".oxp")))
	{
		LL_INFOS() << "User closed the filepicker, aborting export!" << LL_ENDL;
		return;
	}
	mFilename = file_picker.getFirstFile();
	
	LLUIString title = getString("title_working");
	title.setArg("[OBJECT]", mObjectName);
	setTitle(title);
	
	if (!exportSelection())
	{
		LLNotificationsUtil::add("ExportFailed");
		//closeFloater();
	}
}

void FSFloaterObjectExport::populateObjectList()
{
	
	if (mObjectList)
	{
		mObjectList->deleteAllItems();
		mObjectList->setCommentText(LLTrans::getString("LoadingData"));
		for (obj_info_t::iterator obj_iter = mObjects.begin(); obj_iter != mObjects.end(); ++obj_iter)
		{
			LLSD element;
			element["columns"][0]["column"]	= "icon";
			element["columns"][0]["type"]	= "icon";
			element["columns"][0]["value"]	= "Inv_Object";
			element["columns"][1]["column"]	= "name";
			element["columns"][1]["value"]	= obj_iter->second;
			mObjectList->addElement(element, ADD_BOTTOM);
			
		}
		if (mObjectList && !mTextureNames.empty())
		{
			for (string_list_t::iterator iter = mTextureNames.begin(); iter != mTextureNames.end(); ++iter)
			{
				LLSD element;
				element["columns"][0]["column"]	= "icon";
				element["columns"][0]["type"]	= "icon";
				element["columns"][0]["value"]	= "Inv_Texture";
				element["columns"][1]["column"]	= "name";
				element["columns"][1]["value"]	= (*iter);
				mObjectList->addElement(element, ADD_BOTTOM);
				
			}
		}
	}
}

// Copypasta from DAE Export. :o
void FSFloaterObjectExport::addSelectedObjects()
{
	mTotal = 0;
	mIncluded = 0;
	mNumTextures = 0;
	mNumExportableTextures = 0;
	mObjects.clear();
	mTextures.clear();
	mTextureNames.clear();

	if (mObjectSelection)
	{
		LLSelectNode* node = mObjectSelection->getFirstRootNode();
		if (node)
		{
			mCurrentObjectID = node->getObject()->getID();
			mObjectName = node->mName;
			for (LLObjectSelection::iterator iter = mObjectSelection->begin(); iter != mObjectSelection->end(); ++iter)
			{
				node = *iter;
				mTotal++;
				if (!node->getObject()->getVolume() || !FSExportPermsCheck::canExportNode(node, false)) continue;
				mIncluded++;
				addObject(node->getObject(), node->mName);
			}
		
			if (mObjects.empty())
			{
				LL_WARNS("export") << "Nothing selected passed permissions checks!" << LL_ENDL;
				//LLNotificationsUtil::add("ExportFailed");
				return;
			}
		
			updateTextureInfo();
			mNumTextures = mTextures.size();
			mNumExportableTextures = getNumExportableTextures();
		}
		else
		{
			mObjectName = "";
		}
	}
}

S32 FSFloaterObjectExport::getNumExportableTextures()
{
	S32 res = 0;
	for (string_list_t::const_iterator t = mTextureNames.begin(); t != mTextureNames.end(); ++t)
	{
		std::string name = *t;
		if (!name.empty())
		{
			++res;
		}
	}
	
	return res;
}

void FSFloaterObjectExport::addTexturePreview()
{
	S32 num_text = mNumExportableTextures;
	if (num_text == 0) return;
	S32 img_width = 100;
	S32 img_height = img_width + 15;
	S32 panel_height = (num_text / 2 + 1) * (img_height) + 10;
	// *TODO: It would be better to check against a list of controls
	mTexturePanel->deleteAllChildren();
	mTexturePanel->reshape(230, panel_height);
	S32 img_nr = 0;
	for (S32 i=0; i < mTextures.size(); i++)
	{
		if (mTextureNames[i].empty()) continue;		
		S32 left = 8 + (img_nr % 2) * (img_width + 13);
		S32 bottom = panel_height - (10 + (img_nr / 2 + 1) * (img_height));
		LLRect r(left, bottom + img_height, left + img_width, bottom);
		LLTextureCtrl::Params p;
		p.rect(r);
		p.layout("topleft");
		p.name(mTextureNames[i]);
		p.image_id(mTextures[i]);
		p.tool_tip(mTextureNames[i]);
		LLTextureCtrl* texture_block = LLUICtrlFactory::create<LLTextureCtrl>(p);
		mTexturePanel->addChild(texture_block);
		img_nr++;
	}
}

///////////////////////////////////////////
// FSExportCacheReadResponder

FSFloaterObjectExport::FSExportCacheReadResponder::FSExportCacheReadResponder(const LLUUID& id, LLImageFormatted* image, FSFloaterObjectExport* parent)
: mFormattedImage(image),
mID(id),
mParent(parent)
{
	setImage(image);
}

void FSFloaterObjectExport::FSExportCacheReadResponder::setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal)
{
	if (imageformat != IMG_CODEC_J2C)
	{
		LL_WARNS("export") << "Texture " << mID << " is not formatted as J2C." << LL_ENDL;
	}
	
	if (mFormattedImage.notNull())
	{
		mFormattedImage->appendData(data, datasize);
	}
	else
	{
		mFormattedImage = LLImageFormatted::createFromType(imageformat);
		mFormattedImage->setData(data, datasize);
	}
	mImageSize = imagesize;
	mImageLocal = imagelocal;
}

void FSFloaterObjectExport::FSExportCacheReadResponder::completed(bool success)
{
	if (success && mFormattedImage.notNull() && mImageSize > 0)
	{
		LL_DEBUGS("export") << "SUCCESS getting texture " << mID << LL_ENDL;
		if (mParent)
			mParent->saveFormattedImage(mFormattedImage, mID);
	}
	else
	{
		/// NOTE: we can get here due to trying to fetch a static local texture
		/// do nothing special as importing static local texture just needs an UUID only.
		if (!success)
		{
			LL_WARNS("export") << "FAILED to get texture " << mID << LL_ENDL;
		}
		if (mFormattedImage.isNull())
		{
			LL_WARNS("export") << "FAILED: NULL texture " << mID << LL_ENDL;
		}
		mParent->removeRequestedTexture(mID);
	}
}

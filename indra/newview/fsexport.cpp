/**
 * @file fsexport.cpp
 * @brief export selected objects to an file using LLSD.
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

#include "fsexport.h"

#include "llagent.h"
#include "llagentconstants.h"
#include "llagentdata.h"
#include "llappviewer.h"
#include "llavatarnamecache.h"
#include "llbufferstream.h"
#include "llcallbacklist.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llfilepicker.h"
#include "llimagej2c.h"
#include "llinventoryfunctions.h"
#include "llmeshrepository.h"
#include "llmultigesture.h"
#include "llnotecard.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "llsdutil.h"
#include "llversioninfo.h"
#include "llvfile.h"
#include "llviewerinventory.h"
#include "llviewernetwork.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"
#include "llviewerpartsource.h"
#include "llworld.h"
#include <boost/algorithm/string_regex.hpp>
#include "apr_base64.h"

const F32 MAX_TEXTURE_WAIT_TIME = 30.0f;
const F32 MAX_INVENTORY_WAIT_TIME = 30.0f;
const F32 MAX_ASSET_WAIT_TIME = 60.0f;

// static
void FSExport::onIdle(void* user_data)
{
	FSExport* self = (FSExport*)user_data;
	self->onIdle();
}

void FSExport::onIdle()
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
			file.open(mFileName, std::ios_base::out | std::ios_base::binary);
			std::string zip_data = zip_llsd(mFile);
			file.write(zip_data.data(), zip_data.size());
			file.close();
			
			LL_DEBUGS("export") << "Export finished and written to " << mFileName << LL_ENDL;
		}
		else if (mLastRequest != mRequestedTexture.size())
		{
			mWaitTimer.start();
			mLastRequest = mRequestedTexture.size();
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
				image->setLoadedCallback(FSExport::onImageLoaded, 0, TRUE, FALSE, this, &mCallbackTextureList);
				LL_DEBUGS("export") << "re-requested texture " << texture_id.asString() << LL_ENDL;
			}
		}
		break;
	default:
		break;
	}
}

void FSExport::exportSelection()
{
	LLObjectSelectionHandle selection = LLSelectMgr::instance().getSelection();
	LLObjectSelection::valid_root_iterator iter = selection->valid_root_begin();
	LLSelectNode* node = *iter;

	LLFilePicker& file_picker = LLFilePicker::instance();
	if(!file_picker.getSaveFile(LLFilePicker::FFSAVE_EXPORT, LLDir::getScrubbedFileName(node->mName + ".oxp")))
	{
		return;
	}
	mFileName = file_picker.getFirstFile();
	mFilePath = gDirUtilp->getDirName(mFileName);
	
	mFile.clear();
	mRequestedTexture.clear();

	mExported = false;
	mInventoryRequests.clear();
	mAssetRequests.clear();

	mFile["format_version"] = 1;
	mFile["client"] = LLAppViewer::instance()->getSecondLifeTitle() + LLVersionInfo::getChannel();
	mFile["client_version"] = LLVersionInfo::getVersion();
	mFile["grid"] = LLGridManager::getInstance()->getGridLabel();

	for ( ; iter != selection->valid_root_end(); ++iter)
	{
		mFile["linkset"].append(getLinkSet((*iter)));
	}

	if (mExported)
	{
		mWaitTimer.start();
		mLastRequest = mInventoryRequests.size();
		mExportState = INVENTORY_DOWNLOAD;
		gIdleCallbacks.addFunction(onIdle, this);
	}
	else
	{
		LL_WARNS("export") << "Nothing was exported. File not created." << LL_ENDL;
	}
}

LLSD FSExport::getLinkSet(LLSelectNode* node)
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

void FSExport::addPrim(LLViewerObject* object, bool root)
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
	if (node)
	{
		if ((LLGridManager::getInstance()->isInSecondLife())
			&& object->permYouOwner()
			&& (gAgentID == node->mPermissions->getCreator() || megaPrimCheck(node->mPermissions->getCreator(), object)))
		{
			default_prim = false;
		}
#if OPENSIM
		if (LLGridManager::getInstance()->isInOpenSim()
		      && object->permYouOwner()
		      && object->permModify()
		      && object->permCopy()
		      && object->permTransfer())
		{
			default_prim = false;
		}
#endif
	}
	else
	{
		LL_WARNS("export") << "LLSelect node for " << object_id.asString() << " not found. Using default prim instead." << LL_ENDL;
		default_prim = true;
	}

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
						LLSD mesh_header = gMeshRepo.getMeshHeader(sculpt_params->getSculptTexture());
						if (mesh_header["creator"].asUUID() == gAgentID)
						{
							LL_DEBUGS("export") << "Mesh creater check passed." << LL_ENDL;
							prim["sculpt"] = sculpt_params->asLLSD();
						}
						else
						{
							LL_DEBUGS("export") << "Using default mesh skulpt." << LL_ENDL;
							LLSculptParams default_sculpt;
							prim["sculpt"] = default_sculpt.asLLSD();
						}
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
			if (exportTexture(object->getTE(i)->getID()))
			{
				prim["texture"].append(object->getTE(i)->asLLSD());
			}
			else
			{
				LLTextureEntry te(LL_DEFAULT_WOOD_UUID); // TODO: use user option of default texture.
				prim["texture"].append(te.asLLSD());
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

		mInventoryRequests.push_back(object_id);
		object->registerInventoryListener(this, NULL);
		object->dirtyInventory();
		object->requestInventory();
	}

	mFile["prim"][object_id.asString()] = prim;
}

bool FSExport::exportTexture(const LLUUID& texture_id)
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
		// no need to save the texture data as the viewer allready has it in a local file.
		mTextureChecked[texture_id] = true;
		return true;
	}
	
	//TODO: check for local file static texture. The above will only get the static texture in the static db, not indevitial texture files.

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
		assetCheck(texture_id, name, description);
	}
	else
	{
		texture_export = assetCheck(texture_id, name, description);
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
	image->setLoadedCallback(FSExport::onImageLoaded, 0, TRUE, FALSE, this, &mCallbackTextureList);

	return true;
}

// static
void FSExport::onImageLoaded(BOOL success, LLViewerFetchedTexture* src_vi, LLImageRaw* src, LLImageRaw* aux_src, S32 discard_level, BOOL final, void* userdata)
{
	if(final && success)
	{
		const LLUUID& texture_id = src_vi->getID();
		LLImageJ2C* mFormattedImage = new LLImageJ2C;
		FSExportCacheReadResponder* responder = new FSExportCacheReadResponder(texture_id, mFormattedImage);
		LLAppViewer::getTextureCache()->readFromCache(texture_id, LLWorkerThread::PRIORITY_HIGH, 0, 999999, responder);
		LL_DEBUGS("export") << "Fetching " << texture_id << " from the TextureCache" << LL_ENDL;
	}
}


FSExportCacheReadResponder::FSExportCacheReadResponder(const LLUUID& id, LLImageFormatted* image) :
	mFormattedImage(image),
	mID(id)
{
	setImage(image);
}

void FSExportCacheReadResponder::setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat, BOOL imagelocal)
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

void FSExportCacheReadResponder::completed(bool success)
{
	if (success && mFormattedImage.notNull() && mImageSize > 0)
	{
		LL_DEBUGS("export") << "SUCCESS getting texture " << mID << LL_ENDL;
		FSExport::getInstance()->saveFormattedImage(mFormattedImage, mID);
	}
	else
	{
		//NOTE: we can get here due to trying to fetch a static local texture
		// do nothing spiachel as importing static local texture just needs an UUID only.
		if (!success)
		{
			LL_WARNS("export") << "FAILED to get texture " << mID << LL_ENDL;
		}
		if (mFormattedImage.isNull())
		{
			LL_WARNS("export") << "FAILED: NULL texture " << mID << LL_ENDL;
		}
		FSExport::getInstance()->removeRequestedTexture(mID);
	}
}

void FSExport::removeRequestedTexture(LLUUID texture_id)
{
	if (mRequestedTexture.count(texture_id) != 0)
	{
		mRequestedTexture.erase(texture_id);
	}
}

void FSExport::saveFormattedImage(LLPointer<LLImageFormatted> mFormattedImage, LLUUID id)
{
	std::stringstream texture_str;
	texture_str.write((const char*) mFormattedImage->getData(), mFormattedImage->getDataSize());
	std::string str = texture_str.str();

	mFile["asset"][id.asString()]["name"] = mRequestedTexture[id].name;
	mFile["asset"][id.asString()]["description"] = mRequestedTexture[id].description;
	mFile["asset"][id.asString()]["type"] = LLAssetType::lookup(LLAssetType::AT_TEXTURE);
	mFile["asset"][id.asString()]["data"] = LLSD::Binary(str.begin(),str.end());

	removeRequestedTexture(id);
}

bool FSExport::megaPrimCheck(LLUUID creator, LLViewerObject* object)
{
	F32 max_object_size = LLWorld::getInstance()->getRegionMaxPrimScale();
	LLVector3 vec = object->getScale();
	if (!(vec.mV[VX] > max_object_size || vec.mV[VY] > max_object_size || vec.mV[VZ] > max_object_size))
	{
		return false;
	}
	
	if (creator == LLUUID("7ffd02d0-12f4-48b4-9640-695708fd4ae4")) // Zwagoth Klaar
	{
		return true;
	}
	
	return false;
}

bool FSExport::assetCheck(LLUUID asset_id, std::string& name, std::string& description)
{
	bool exportable = false;
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLAssetIDMatches asset_id_matches(asset_id);
	gInventory.collectDescendentsIf(LLUUID::null,
					cats,
					items,
					LLInventoryModel::INCLUDE_TRASH,
					asset_id_matches);

	if (items.count())
	{
		// use the name of the first match
		name = items[0]->getName();
		description = items[0]->getDescription();

		for (S32 i = 0; i < items.count(); ++i)
		{
			if (!exportable)
			{
				LLPermissions perms = items[i]->getPermissions();
#if OPENSIM
				if (LLGridManager::getInstance()->isInOpenSim() && ((perms.getMaskBase() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED))
				{
					exportable = true;
				}
#endif
				if (LLGridManager::getInstance()->isInSecondLife() && (perms.getCreator() == gAgentID))
				{
					exportable = true;
				}
			}
		}   
	}

	return exportable;
}

void FSExport::inventoryChanged(LLViewerObject* object, LLInventoryObject::object_list_t* inventory, S32 serial_num, void* user_data)
{
	uuid_vec_t::iterator v_iter = std::find(mInventoryRequests.begin(), mInventoryRequests.end(), object->getID());
	if (v_iter != mInventoryRequests.end())
	{
		LL_DEBUGS("export") << "Erasing inventory request " << object->getID() << LL_ENDL;
		mInventoryRequests.erase(v_iter);
	}
  
	LLSD& prim = mFile["prim"][object->getID().asString()];
	for (LLInventoryObject::object_list_t::const_iterator iter = inventory->begin(); iter != inventory->end(); ++iter)
	{
		LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(iter->get());
		if (!item)
		{
			continue;
		}

		bool exportable = false;
		LLPermissions perms = item->getPermissions();
#if OPENSIM
		if (LLGridManager::getInstance()->isInOpenSim() && ((perms.getMaskBase() & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED))
		{
			exportable = true;
		}
#endif
		if (LLGridManager::getInstance()->isInSecondLife() && (perms.getCreator() == gAgentID))
		{
			exportable = true;
		}

		if (!exportable)
		{
			LL_DEBUGS("export") << "Item " << item->getName() << " failed export check." << LL_ENDL;
			continue;
		}

		if (item->getType() == LLAssetType::AT_NONE || item->getType() == LLAssetType::AT_OBJECT)
		{
			// currentelly not exportable
			LL_DEBUGS("export") << "Skipping " << LLAssetType::lookup(item->getType()) << " item " << item->getName() << LL_ENDL;
			continue;
		}

		prim["content"].append(item->getUUID());
		mFile["inventory"][item->getUUID().asString()] = ll_create_sd_from_inventory_item(item);
		
		if (item->getAssetUUID().isNull() && item->getType() == LLAssetType::AT_NOTECARD)
		{
			// FIRE-9655
			// Blank Notecard item can have NULL asset ID.
			// Generate a new UUID and save as an empty asset.
			LLUUID asset_uuid = LLUUID::generateNewID();
			mFile["inventory"][item->getUUID().asString()]["asset_id"] = asset_uuid;
			
			mFile["asset"][asset_uuid.asString()]["name"] = item->getName();
			mFile["asset"][asset_uuid.asString()]["description"] = item->getDescription();
			mFile["asset"][asset_uuid.asString()]["type"] = LLAssetType::lookup(item->getType());

			LLNotecard nc(255); //don't need to allocate default size of 65536
			std::stringstream out_stream;
			nc.exportStream(out_stream);
			std::string out_string = out_stream.str();
			std::vector<U8> buffer(out_string.begin(), out_string.end());
			mFile["asset"][asset_uuid.asString()]["data"] = buffer;
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
void FSExport::onLoadComplete(LLVFS* vfs, const LLUUID& asset_uuid, LLAssetType::EType type, void* user_data, S32 status, LLExtStat ext_status)
{
	FSAssetResourceData* data = (FSAssetResourceData*)user_data;
	FSExport* self = (FSExport*)data->user_data;
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
	self->mFile["asset"][asset_uuid.asString()]["name"] = data->name;
	self->mFile["asset"][asset_uuid.asString()]["description"] = data->description;
	self->mFile["asset"][asset_uuid.asString()]["type"] = LLAssetType::lookup(type);
	self->mFile["asset"][asset_uuid.asString()]["data"] = buffer;

	if (self->mFile["inventory"].has(item_uuid.asString()))
	{
		if (self->mFile["inventory"][item_uuid.asString()]["asset_id"].asUUID().isNull())
		{
			self->mFile["inventory"][item_uuid.asString()]["asset_id"] = asset_uuid;
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
				LL_DEBUGS("export") << "Invalied uuid: " << m1->str() << LL_ENDL;
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
				if (!self->assetCheck(anim_step->mAnimAssetID, name, description))
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
				if (!self->assetCheck(sound_step->mSoundAssetID, name, description))
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

void FSExport::removeRequestedAsset(LLUUID asset_uuid)
{
	uuid_vec_t::iterator iter = std::find(mAssetRequests.begin(), mAssetRequests.end(), asset_uuid);
	if (iter != mAssetRequests.end())
	{
		LL_DEBUGS("export") << "Erasing asset request " << asset_uuid.asString() << LL_ENDL;
		mAssetRequests.erase(iter);
	}
}

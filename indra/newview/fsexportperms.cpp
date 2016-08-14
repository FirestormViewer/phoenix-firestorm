/*
 * @file fsexportperms.cpp
 * @brief Export permissions check
 * @authors Cinder Biscuits
 *
 * $LicenseInfo:firstyear=2013&license=LGPLV2.1$
 * Copyright (C) 2013 Cinder Biscuits <cinder.roxley@phoenixviewer.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write a love letter 
 * to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#include "llviewerprecompiledheaders.h"

#include "fsexportperms.h"
#include "lfsimfeaturehandler.h"
#include "llagent.h"
#include "llinventoryfunctions.h"
#include "llmeshrepository.h"
#include "llviewernetwork.h"
#include "llvovolume.h"
#include "llworld.h"

#define FOLLOW_PERMS 1

bool FSExportPermsCheck::canExportNode(LLSelectNode* node, bool dae)
{
	if (!node)
	{
		LL_WARNS("export") << "No node, bailing!" << LL_ENDL;
		return false;
	}
	bool exportable = false;
	
	LLViewerObject* object = node->getObject();
	if (LLGridManager::getInstance()->isInSecondLife())
	{
		LLUUID creator(node->mPermissions->getCreator());
		exportable = (object->permYouOwner() && gAgentID == creator);
		if (!exportable)
		{
			// Megaprim check
			F32 max_object_size = LLWorld::getInstance()->getRegionMaxPrimScale();
			LLVector3 vec = object->getScale();
			if (vec.mV[VX] > max_object_size || vec.mV[VY] > max_object_size || vec.mV[VZ] > max_object_size)
				exportable = (creator == LLUUID("7ffd02d0-12f4-48b4-9640-695708fd4ae4") // Zwagoth Klaar
							  || creator == gAgentID);
		}
	}
#ifdef OPENSIM
	else if (LLGridManager::getInstance()->isInOpenSim())
	{
		switch (LFSimFeatureHandler::instance().exportPolicy())
		{
			case EXPORT_ALLOWED:
			{
				exportable = node->mPermissions->allowExportBy(gAgent.getID());
				break;
			}
			/// TODO: Once enough grids adopt a version supporting exports, get consensus
			/// on whether we should allow full perm exports anymore.
			case EXPORT_UNDEFINED:
			{
				exportable = (object->permYouOwner() && object->permModify() && object->permCopy() && object->permTransfer());
				break;
			}
			case EXPORT_DENIED:
			default:
				exportable = (object->permYouOwner() && gAgentID == node->mPermissions->getCreator());
		}
	}
#endif // OPENSIM
	// We've got perms on the object itself, let's check for sculptmaps and meshes!
	if (exportable)
	{
		LLVOVolume *volobjp = NULL;
		if (object->getPCode() == LL_PCODE_VOLUME)
		{
			volobjp = (LLVOVolume *)object;
		}
		if (volobjp && volobjp->isSculpted())
		{
			const LLSculptParams *sculpt_params = (const LLSculptParams *)object->getParameterEntry(LLNetworkData::PARAMS_SCULPT);
			if (LLGridManager::getInstance()->isInSecondLife())
			{
				if(volobjp->isMesh())
				{
					if (dae)
					{
						LLSD mesh_header = gMeshRepo.getMeshHeader(sculpt_params->getSculptTexture());
						exportable = mesh_header["creator"].asUUID() == gAgentID;
					}
					else
					{
						// can not export mesh to oxp
						LL_INFOS("export") << "Mesh can not be exported to oxp." << LL_ENDL;
						return false;
					}
				}
				else if (sculpt_params)
				{
					LLViewerFetchedTexture* imagep = LLViewerTextureManager::getFetchedTexture(sculpt_params->getSculptTexture());
					if (imagep->mComment.find("a") != imagep->mComment.end())
					{
						exportable = (LLUUID(imagep->mComment["a"]) == gAgentID);
					}
					if (!exportable)
					{
						LLUUID asset_id = sculpt_params->getSculptTexture();
						LLViewerInventoryCategory::cat_array_t cats;
						LLViewerInventoryItem::item_array_t items;
						LLAssetIDMatches asset_id_matches(asset_id);
						gInventory.collectDescendentsIf(LLUUID::null, cats, items,
														LLInventoryModel::INCLUDE_TRASH,
														asset_id_matches);
						
						for (S32 i = 0; i < items.size(); ++i)
						{
							const LLPermissions perms = items[i]->getPermissions();
							exportable = perms.getCreator() == gAgentID;
						}
					}
					if (!exportable)
						LL_INFOS("export") << "Sculpt map has failed permissions check." << LL_ENDL;
				}
			}
#ifdef OPENSIM
			else if (LLGridManager::getInstance()->isInOpenSim())
			{
				if (sculpt_params && !volobjp->isMesh())
				{
					LLUUID asset_id = sculpt_params->getSculptTexture();
					LLViewerInventoryCategory::cat_array_t cats;
					LLViewerInventoryItem::item_array_t items;
					LLAssetIDMatches asset_id_matches(asset_id);
					gInventory.collectDescendentsIf(LLUUID::null, cats, items,
													LLInventoryModel::INCLUDE_TRASH,
													asset_id_matches);
					
					for (S32 i = 0; i < items.size(); ++i)
					{
						const LLPermissions perms = items[i]->getPermissions();
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
						}
						if (!exportable)
							LL_INFOS("export") << "Sculpt map has failed permissions check." << LL_ENDL;
					}
				}
				else
				{
					exportable = true;
				}
			}
#endif // OPENSIM
		}
		else
		{
			exportable = true;
		}
	}
	return exportable;
}

#if !FOLLOW_PERMS
#error "You didn't think it would be that easy, did you? :P"
#endif

bool FSExportPermsCheck::canExportAsset(LLUUID asset_id, std::string* name, std::string* description)
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
	
	if (items.size())
	{
		// use the name of the first match
		(*name) = items[0]->getName();
		(*description) = items[0]->getDescription();
		
		for (S32 i = 0; i < items.size(); ++i)
		{
			if (!exportable)
			{
				LLPermissions perms = items[i]->getPermissions();
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
			}
		}
	}
	
	return exportable;
}


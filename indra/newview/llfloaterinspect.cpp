/** 
 * @file llfloaterinspect.cpp
 * @brief Floater for object inspection tool
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#include "llfloaterinspect.h"

#include "llfloaterreg.h"
#include "llfloatertools.h"
#include "llavataractions.h"
#include "llavatarnamecache.h"
#include "llgroupactions.h"
#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "lluictrlfactory.h"
// [RLVa:KB] - Checked: RLVa-2.0.1
#include "rlvactions.h"
#include "rlvcommon.h"
#include "rlvui.h"
// [/RLVa:KB]
// PoundLife - Improved Object Inspect
#include "lltexturectrl.h"
#include "llviewerobjectlist.h" //gObjectList
#include "llviewertexturelist.h"
// PoundLife END

//LLFloaterInspect* LLFloaterInspect::sInstance = NULL;

LLFloaterInspect::LLFloaterInspect(const LLSD& key)
  : LLFloater(key),
	mDirty(FALSE),
	mOwnerNameCacheConnection(),
	mCreatorNameCacheConnection()
{
	mCommitCallbackRegistrar.add("Inspect.OwnerProfile",	boost::bind(&LLFloaterInspect::onClickOwnerProfile, this));
	mCommitCallbackRegistrar.add("Inspect.CreatorProfile",	boost::bind(&LLFloaterInspect::onClickCreatorProfile, this));
	mCommitCallbackRegistrar.add("Inspect.SelectObject",	boost::bind(&LLFloaterInspect::onSelectObject, this));
}

BOOL LLFloaterInspect::postBuild()
{
	mObjectList = getChild<LLScrollListCtrl>("object_list");
//	childSetAction("button owner",onClickOwnerProfile, this);
//	childSetAction("button creator",onClickCreatorProfile, this);
//	childSetCommitCallback("object_list", onSelectObject, NULL);
	
	refresh();
	
	return TRUE;
}

LLFloaterInspect::~LLFloaterInspect(void)
{
	if (mOwnerNameCacheConnection.connected())
	{
		mOwnerNameCacheConnection.disconnect();
	}
	if (mCreatorNameCacheConnection.connected())
	{
		mCreatorNameCacheConnection.disconnect();
	}
	if(!LLFloaterReg::instanceVisible("build"))
	{
		if(LLToolMgr::getInstance()->getBaseTool() == LLToolCompInspect::getInstance())
		{
			LLToolMgr::getInstance()->clearTransientTool();
		}
		// Switch back to basic toolset
		LLToolMgr::getInstance()->setCurrentToolset(gBasicToolset);	
	}
	else
	{
		LLFloaterReg::showInstance("build", LLSD(), TRUE);
	}
}

void LLFloaterInspect::onOpen(const LLSD& key)
{
	BOOL forcesel = LLSelectMgr::getInstance()->setForceSelection(TRUE);
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLSelectMgr::getInstance()->setForceSelection(forcesel);	// restore previouis value
	mObjectSelection = LLSelectMgr::getInstance()->getSelection();
	refresh();
}

// [RLVa:KB] - Checked: RLVa-2.0.1
const LLSelectNode* LLFloaterInspect::getSelectedNode() /*const*/
{
	if(mObjectList->getAllSelected().size() == 0)
	{
		return NULL;
	}
	LLScrollListItem* first_selected =mObjectList->getFirstSelected();

	if (first_selected)
	{
		struct f : public LLSelectedNodeFunctor
		{
			LLUUID obj_id;
			f(const LLUUID& id) : obj_id(id) {}
			virtual bool apply(LLSelectNode* node)
			{
				return (obj_id == node->getObject()->getID());
			}
		} func(first_selected->getUUID());
		return mObjectSelection->getFirstNode(&func);
	}
	return NULL;
}

void LLFloaterInspect::onClickCreatorProfile()
{
		const LLSelectNode* node = getSelectedNode();
		if(node)
		{
			// Only anonymize the creator if they're also the owner or if they're a nearby avie
			const LLUUID& idCreator = node->mPermissions->getCreator();
			if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, idCreator)) && ((node->mPermissions->getOwner() == idCreator) || (RlvUtil::isNearbyAgent(idCreator))) )
			{
				return;
			}
			LLAvatarActions::showProfile(idCreator);
		}
}

void LLFloaterInspect::onClickOwnerProfile()
{
		const LLSelectNode* node = getSelectedNode();
		if(node)
		{
			if(node->mPermissions->isGroupOwned())
			{
				const LLUUID& idGroup = node->mPermissions->getGroup();
				LLGroupActions::show(idGroup);
			}
			else
			{
				const LLUUID& owner_id = node->mPermissions->getOwner();
				if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, owner_id))
					return;
				LLAvatarActions::showProfile(owner_id);
			}

		}
}

void LLFloaterInspect::onSelectObject()
{
	if(LLFloaterInspect::getSelectedUUID() != LLUUID::null)
	{
		if (!RlvActions::isRlvEnabled())
		{
			getChildView("button owner")->setEnabled(true);
			getChildView("button creator")->setEnabled(true);
		}
		else
		{
			const LLSelectNode* node = getSelectedNode();
			const LLUUID& idOwner = (node) ? node->mPermissions->getOwner() : LLUUID::null;
			const LLUUID& idCreator = (node) ? node->mPermissions->getCreator() : LLUUID::null;

			// See LLFloaterInspect::onClickCreatorProfile()
			getChildView("button owner")->setEnabled( (RlvActions::canShowName(RlvActions::SNC_DEFAULT, idOwner)) || ((node) && (node->mPermissions->isGroupOwned())) );
			// See LLFloaterInspect::onClickOwnerProfile()
			getChildView("button creator")->setEnabled( ((idOwner != idCreator) && (!RlvUtil::isNearbyAgent(idCreator))) || (RlvActions::canShowName(RlvActions::SNC_DEFAULT, idCreator)) );
		}
	}
}
// [/RLVa:KB]

//void LLFloaterInspect::onClickCreatorProfile()
//{
//	if(mObjectList->getAllSelected().size() == 0)
//	{
//		return;
//	}
//	LLScrollListItem* first_selected =mObjectList->getFirstSelected();
//
//	if (first_selected)
//	{
//		struct f : public LLSelectedNodeFunctor
//		{
//			LLUUID obj_id;
//			f(const LLUUID& id) : obj_id(id) {}
//			virtual bool apply(LLSelectNode* node)
//			{
//				return (obj_id == node->getObject()->getID());
//			}
//		} func(first_selected->getUUID());
//		LLSelectNode* node = mObjectSelection->getFirstNode(&func);
//		if(node)
//		{
//			LLAvatarActions::showProfile(node->mPermissions->getCreator());
//		}
//	}
//}

//void LLFloaterInspect::onClickOwnerProfile()
//{
//	if(mObjectList->getAllSelected().size() == 0) return;
//	LLScrollListItem* first_selected =mObjectList->getFirstSelected();
//
//	if (first_selected)
//	{
//		LLUUID selected_id = first_selected->getUUID();
//		struct f : public LLSelectedNodeFunctor
//		{
//			LLUUID obj_id;
//			f(const LLUUID& id) : obj_id(id) {}
//			virtual bool apply(LLSelectNode* node)
//			{
//				return (obj_id == node->getObject()->getID());
//			}
//		} func(selected_id);
//		LLSelectNode* node = mObjectSelection->getFirstNode(&func);
//		if(node)
//		{
//			if(node->mPermissions->isGroupOwned())
//			{
//				const LLUUID& idGroup = node->mPermissions->getGroup();
//				LLGroupActions::show(idGroup);
//			}
//			else
//			{
//				const LLUUID& owner_id = node->mPermissions->getOwner();
//				LLAvatarActions::showProfile(owner_id);
//			}
//
//		}
//	}
//}

//void LLFloaterInspect::onSelectObject()
//{
//	if(LLFloaterInspect::getSelectedUUID() != LLUUID::null)
//	{
//		getChildView("button owner")->setEnabled(true);
//		getChildView("button creator")->setEnabled(true);
//	}
//}

LLUUID LLFloaterInspect::getSelectedUUID()
{
	if(mObjectList->getAllSelected().size() > 0)
	{
		LLScrollListItem* first_selected =mObjectList->getFirstSelected();
		if (first_selected)
		{
			return first_selected->getUUID();
		}
		
	}
	return LLUUID::null;
}

void LLFloaterInspect::refresh()
{
	// PoundLife - Improved Object Inspect
	mlinksetstats = "Total stats:\n\n";
	getChild<LLTextBase>("linksetstats_text")->setText(mlinksetstats);
	// PoundLife - End
	LLUUID creator_id;
	std::string creator_name;
	S32 pos = mObjectList->getScrollPos();
	// PoundLife - Improved Object Inspect
	LLSelectMgr* selmgr = LLSelectMgr::getInstance();
	S32 fcount = 0;
	S32 tcount = 0;
	S32 vcount = 0;
	S32 objcount = 0;
	S32 primcount = 0;
	texturecount = 0;
	texturememory = 0;
	vTextureList.clear();
	// PoundLife - End
	getChildView("button owner")->setEnabled(false);
	getChildView("button creator")->setEnabled(false);
	LLUUID selected_uuid;
	S32 selected_index = mObjectList->getFirstSelectedIndex();
	if(selected_index > -1)
	{
		LLScrollListItem* first_selected =
			mObjectList->getFirstSelected();
		if (first_selected)
		{
			selected_uuid = first_selected->getUUID();
		}
	}
	mObjectList->operateOnAll(LLScrollListCtrl::OP_DELETE);
	//List all transient objects, then all linked objects

	for (LLObjectSelection::valid_iterator iter = mObjectSelection->valid_begin();
		 iter != mObjectSelection->valid_end(); iter++)
	{
		LLSelectNode* obj = *iter;
		LLSD row;
		std::string owner_name, creator_name;

		if (obj->mCreationDate == 0)
		{	// Don't have valid information from the server, so skip this one
			continue;
		}

		time_t timestamp = (time_t) (obj->mCreationDate/1000000);
		std::string timeStr = getString("timeStamp");
		LLSD substitution;
		substitution["datetime"] = (S32) timestamp;
		LLStringUtil::format (timeStr, substitution);

		const LLUUID& idOwner = obj->mPermissions->getOwner();
		const LLUUID& idCreator = obj->mPermissions->getCreator();
		LLAvatarName av_name;

		if(obj->mPermissions->isGroupOwned())
		{
			std::string group_name;
			const LLUUID& idGroup = obj->mPermissions->getGroup();
			if(gCacheName->getGroupName(idGroup, group_name))
			{
				// <FS:Ansariel> Make text localizable
				//owner_name = "[" + group_name + "] (group)";
				owner_name = "[" + group_name + "] " + getString("Group");
			}
			else
			{
				owner_name = LLTrans::getString("RetrievingData");
				if (mOwnerNameCacheConnection.connected())
				{
					mOwnerNameCacheConnection.disconnect();
				}
				mOwnerNameCacheConnection = gCacheName->getGroup(idGroup, boost::bind(&LLFloaterInspect::onGetOwnerNameCallback, this));
			}
		}
		else
		{
			// Only work with the names if we actually get a result
			// from the name cache. If not, defer setting the
			// actual name and set a placeholder.
			if (LLAvatarNameCache::get(idOwner, &av_name))
			{
// [RLVa:KB] - Checked: RLVa-2.0.1
				bool fRlvCanShowName = (RlvActions::canShowName(RlvActions::SNC_DEFAULT, idOwner)) || (obj->mPermissions->isGroupOwned());
				owner_name = (fRlvCanShowName) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
//				owner_name = av_name.getCompleteName();
			}
			else
			{
				owner_name = LLTrans::getString("RetrievingData");
				if (mOwnerNameCacheConnection.connected())
				{
					mOwnerNameCacheConnection.disconnect();
				}
				mOwnerNameCacheConnection = LLAvatarNameCache::get(idOwner, boost::bind(&LLFloaterInspect::onGetOwnerNameCallback, this));
			}
		}

		if (LLAvatarNameCache::get(idCreator, &av_name))
		{
// [RLVa:KB] - Checked: RLVa-2.0.1
			const LLUUID& idCreator = obj->mPermissions->getCreator();
			bool fRlvCanShowName = (RlvActions::canShowName(RlvActions::SNC_DEFAULT, idCreator)) || ( (obj->mPermissions->getOwner() != idCreator) && (!RlvUtil::isNearbyAgent(idCreator)) );
			creator_name = (fRlvCanShowName) ? av_name.getCompleteName() : RlvStrings::getAnonym(av_name);
// [/RLVa:KB]
//			creator_name = av_name.getCompleteName();
		}
		else
		{
			creator_name = LLTrans::getString("RetrievingData");
			if (mCreatorNameCacheConnection.connected())
			{
				mCreatorNameCacheConnection.disconnect();
			}
			mCreatorNameCacheConnection = LLAvatarNameCache::get(idCreator, boost::bind(&LLFloaterInspect::onGetCreatorNameCallback, this));
		}
		
		row["id"] = obj->getObject()->getID();
		row["columns"][0]["column"] = "object_name";
		row["columns"][0]["type"] = "text";
		// make sure we're either at the top of the link chain
		// or top of the editable chain, for attachments
		if(!(obj->getObject()->isRoot() || obj->getObject()->isRootEdit()))
		{
			row["columns"][0]["value"] = std::string("   ") + obj->mName;
		}
		else
		{
			row["columns"][0]["value"] = obj->mName;
		}
		row["columns"][1]["column"] = "owner_name";
		row["columns"][1]["type"] = "text";
		row["columns"][1]["value"] = owner_name;
		row["columns"][2]["column"] = "creator_name";
		row["columns"][2]["type"] = "text";
		row["columns"][2]["value"] = creator_name;
		row["columns"][3]["column"] = "creation_date";
		row["columns"][3]["type"] = "text";
		row["columns"][3]["value"] = timeStr;
		// <FS:PP> FIRE-12854: Include a Description column in the Inspect Objects floater
		row["columns"][4]["column"] = "description";
		row["columns"][4]["type"] = "text";
		row["columns"][4]["value"] = obj->mDescription;
		// </FS:PP>
		// <FS:Ansariel> Correct creation date sorting
		row["columns"][5]["column"] = "creation_date_sort";
		row["columns"][5]["type"] = "text";
		row["columns"][5]["value"] = llformat("%d", timestamp);
		// </FS:Ansariel>
		// PoundLife - Improved Object Inspect
		row["columns"][6]["column"] = "facecount";
		row["columns"][6]["type"] = "text";
		row["columns"][6]["value"] = llformat("%d", obj->getObject()->getNumFaces());

		row["columns"][7]["column"] = "vertexcount";
		row["columns"][7]["type"] = "text";
		row["columns"][7]["value"] = llformat("%d", obj->getObject()->getNumVertices());

		row["columns"][8]["column"] = "trianglecount";
		row["columns"][8]["type"] = "text";
		row["columns"][8]["value"] = llformat("%d", obj->getObject()->getNumIndices() / 3);

		// Poundlife - Get VRAM
		U32 tempMem = 0;
		U32 tempCount = 0;
		getObjectVRAM(obj->getObject(), tempMem, tempCount);
		texturememory += tempMem;
		texturecount += tempCount;
		//

		row["columns"][9]["column"] = "vramcount";
		row["columns"][9]["type"] = "text";
		row["columns"][9]["value"] = llformat("%d", tempMem / 1024);

		primcount = selmgr->getSelection()->getObjectCount();
		objcount = selmgr->getSelection()->getRootObjectCount();
		fcount += obj->getObject()->getNumFaces();
		tcount += obj->getObject()->getNumIndices() / 3;
		vcount += obj->getObject()->getNumVertices();
		// PoundLife - END
		mObjectList->addElement(row, ADD_TOP);
	}
	if(selected_index > -1 && mObjectList->getItemIndex(selected_uuid) == selected_index)
	{
		mObjectList->selectNthItem(selected_index);
	}
	else
	{
		mObjectList->selectNthItem(0);
	}
	onSelectObject();
	mObjectList->setScrollPos(pos);
	// PoundLife - Total linkset stats.
	mlinksetstats += llformat("%d objects, %d prims.\n\n", objcount, primcount);
	mlinksetstats += llformat("Faces: %d\n", fcount);
	mlinksetstats += llformat("Vertices: %d\n", vcount);
	mlinksetstats += llformat("Triangles: %d\n\n", tcount);
	mlinksetstats += llformat("Textures: %d\n", texturecount);
	mlinksetstats += llformat("VRAM Use: %d kb", texturememory / 1024);
	getChild<LLTextBase>("linksetstats_text")->setText(mlinksetstats);
	// PoundLife - End
}

// PoundLife - Improved Object Inspect
void LLFloaterInspect::getObjectVRAM(LLViewerObject* object, U32 &memory, U32 &count)
{
	if (!object) return;

	S32 height;
	S32 width;
	std::string type_str;
	LLUUID uuid;
	U8 te_count = object->getNumTEs();
	//	LLUUID uploader;

	typedef std::map< LLUUID, std::vector<U8> > map_t;
	map_t faces_per_texture;
	for (U8 j = 0; j < te_count; j++)
	{
		LLViewerTexture* img = object->getTEImage(j);
		if (!img) continue;
		uuid = img->getID();
		height = img->getFullHeight();
		width = img->getFullWidth();
		addToVRAMTexList(uuid, height, width, memory, count);

		// materials per face
		if (object->getTE(j)->getMaterialParams().notNull())
		{
			uuid = object->getTE(j)->getMaterialParams()->getNormalID();
			if (uuid.notNull())
			{
				LLViewerTexture* img = gTextureList.getImage(uuid);
				if (!img) continue;
				height = img->getFullHeight();
				width = img->getFullWidth();
				addToVRAMTexList(uuid, height, width, memory, count);
			}
			uuid = object->getTE(j)->getMaterialParams()->getSpecularID();
			if (uuid.notNull())
			{
				LLViewerTexture* img = gTextureList.getImage(uuid);
				if (!img) continue;
				height = img->getFullHeight();
				width = img->getFullWidth();
				addToVRAMTexList(uuid, height, width, memory, count);
			}
		}
	}

	// sculpt map
	if (object->isSculpted() && !object->isMesh())
	{
		LLSculptParams *sculpt_params = (LLSculptParams *)(object->getParameterEntry(LLNetworkData::PARAMS_SCULPT));
		uuid = sculpt_params->getSculptTexture();
		LLViewerTexture* img = gTextureList.getImage(uuid);
		if (img)
		{
			height = img->getFullHeight();
			width = img->getFullWidth();
			addToVRAMTexList(uuid, height, width, memory, count);
		}
	}
}

void LLFloaterInspect::addToVRAMTexList(const LLUUID& uuid, S32 height, S32 width, U32 &memory, U32 &count)
{
	if (std::find(vTextureList.begin(), vTextureList.end(), uuid) == vTextureList.end())
	{
		vTextureList.push_back(uuid);
		memory += (((height * width) * 32) / 8);
		count += 1;
	}
}
// PoundLife - End

void LLFloaterInspect::onFocusReceived()
{
	LLToolMgr::getInstance()->setTransientTool(LLToolCompInspect::getInstance());
	LLFloater::onFocusReceived();
}

void LLFloaterInspect::dirty()
{
	setDirty();
}

void LLFloaterInspect::onGetOwnerNameCallback()
{
	mOwnerNameCacheConnection.disconnect();
	setDirty();
}

void LLFloaterInspect::onGetCreatorNameCallback()
{
	mCreatorNameCacheConnection.disconnect();
	setDirty();
}

void LLFloaterInspect::draw()
{
	if (mDirty)
	{
		refresh();
		mDirty = FALSE;
	}

	LLFloater::draw();
}

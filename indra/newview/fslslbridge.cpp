/** 
 * @fslslbridge.cpp 
 * @FSLSLBridge implementation 
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, The Phoenix Viewer Project, Inc.
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
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "fslslbridge.h"
#include "fslslbridgerequest.h"
#include "llxmlnode.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "llviewerinventory.h"
#include "llagent.h"
#include "llvoavatar.h"
#include "llattachmentsmgr.h"
#include "llinventorymodel.h"
#include "llinventoryfunctions.h"
#include "llviewerassettype.h"
#include "llfloaterreg.h"
#include "llinventorybridge.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "llinventorydefines.h"
#include "llselectmgr.h"
#include "llviewerregion.h"
#include "llfoldertype.h"
#include "llhttpclient.h"
#include "llassetuploadresponders.h"
#include "llnearbychatbar.h"
#include "llnotificationmanager.h"

#define phoenix_bridge_name "#LSL<->Client Bridge v0.12"
#define phoenix_folder_name "#Phoenix"

#define LIB_ROCK_NAME "Rock - medium, round"

//#define ROOT_FIRESTORM_FOLDER "#Firestorm"	//moved to llinventoryfunctions to synch with the AO object
#define FS_BRIDGE_NAME "#Firestorm LSL Bridge v"
#define FS_BRIDGE_MAJOR_VERSION 1
#define FS_BRIDGE_MINOR_VERSION 7

const std::string UPLOAD_SCRIPT_1_7 = "EBEDD1D2-A320-43f5-88CF-DD47BBCA5DFB.lsltxt";

static const U8 BRIDGE_POINT = 127;

//
//-TT Client LSL Bridge File
//
class NameCollectFunctor : public LLInventoryCollectFunctor
{
public:
	NameCollectFunctor(std::string name)
	{
		sName = name;
	}
	virtual ~NameCollectFunctor() {}
	virtual bool operator()(LLInventoryCategory* cat,
							LLInventoryItem* item)
	{
		if(item)
		{
			return (item->getName() == sName);
		}
		return false;
	}
private:
	std::string sName;
};

//
//
// Bridge functionality
//
FSLSLBridge :: FSLSLBridge():
					mBridgeAttaching(false),
					mpBridge(NULL)
{
	std::stringstream sstr;
	
	sstr << FS_BRIDGE_NAME;
	sstr << FS_BRIDGE_MAJOR_VERSION;
	sstr << ".";
	sstr << FS_BRIDGE_MINOR_VERSION;

	mCurrentFullName = sstr.str();

	//mBridgeAttaching = false;
	//mpBridge = NULL;
}

FSLSLBridge :: ~FSLSLBridge()
{
}

bool FSLSLBridge :: lslToViewer(std::string message, LLUUID fromID, LLUUID ownerID)
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return false;

	llinfos << message << llendl;

	std::string tag = message.substr(0,11);

	if (tag == "<bridgeURL>")
	{
		if (mpBridge == NULL)
		{
			LLUUID catID = findFSCategory();
			LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);

			if (fsBridge != NULL)
				mpBridge = fsBridge;
		}

		// get the content of the message, between <tag> and </tag>
		mCurrentURL = message.substr(tag.length(), message.length() - ((tag.length() * 2) + 1));
		llinfos << "New URL is: " << mCurrentURL << llendl;
		//TODO need to check if bridge finished attaching
		return true;
	}
	return false;
}

bool FSLSLBridge :: viewerToLSL(std::string message, FSLSLBridgeRequestResponder *responder)
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return false;

	if (responder == NULL)
		responder = new FSLSLBridgeRequestResponder();
	LLHTTPClient::post(mCurrentURL, LLSD(message), responder);

	return true;
}

//
//Bridge initialization
//
void FSLSLBridge :: recreateBridge()
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return;

	LLUUID catID = findFSCategory();

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);
	if (fsBridge != NULL)
	{
		if (mpBridge != NULL)
			mpBridge = NULL; //the object itself will get cleaned up when new one is created.

		if (get_is_item_worn(fsBridge->getUUID()))
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(fsBridge->getUUID());
			//FSLSLItemDeletionObserver *delObserver = new FSLSLItemDeletionObserver();
			//delObserver->setCallback(boost::bind(&FSLSLBridge::oldBridgeDetached, fsBridge->getUUID()));
			//gInventory.addObserver(delObserver);
		}
		else
		{
			gInventory.deleteObject(fsBridge->getUUID());
			gInventory.notifyObservers();
		}
	}
	mBridgeAttaching = true;
	createNewBridge();
}

void FSLSLBridge :: initBridge()
{
	if (!gSavedSettings.getBOOL("UseLSLBridge"))
		return;

	LLUUID catID = findFSCategory();

	//check for inventory load
	FSLSLBridgeInventoryObserver *bridgeInventoryObserver = new FSLSLBridgeInventoryObserver(catID);
	gInventory.addObserver(bridgeInventoryObserver);
}


void FSLSLBridge :: startCreation()
{
	//are we already in conversation with a bridge?
	if (mpBridge != NULL)
	{
		return;
	}

	//if bridge object doesn't exist - create and attach it, update script.
	LLUUID catID = findFSCategory();

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID, LLAssetType::AT_OBJECT);
	if (fsBridge == NULL)
	{
		mBridgeAttaching = true;
		if (gSavedSettings.getBOOL("NoInventoryLibrary"))
		{
			llwarns << "Asked to create bridge, but we don't have a library" << llendl;
			reportToNearbyChat("Firestorm could not create an LSL bridge. Please enable your library and relog");
			return;
		}
		createNewBridge();
	}
	else
	{
		//TODO need versioning - see isOldBridgeVersion()
		mpBridge = fsBridge;
		if (!isItemAttached(mpBridge->getUUID()))
			LLAttachmentsMgr::instance().addAttachment(mpBridge->getUUID(), BRIDGE_POINT, FALSE, TRUE);			
	}
}

void FSLSLBridge :: createNewBridge() 
{
	//announce yourself
	reportToNearbyChat("Creating the bridge. This might take a few moments, please wait");

	//check if user has a bridge
	LLUUID catID = findFSCategory();

	//attach the Linden rock from the library (will resize as soon as attached)
	LLUUID libID = gInventory.getLibraryRootFolderID();
	LLViewerInventoryItem* libRock = findInvObject(LIB_ROCK_NAME, libID, LLAssetType::AT_OBJECT);
	//shouldn't happen but just in case
	if (libRock != NULL)
	{
		//copy the library item to inventory and put it on 
		LLPointer<LLInventoryCallback> cb = new FSLSLBridgeRezCallback();
		copy_inventory_item(gAgent.getID(),libRock->getPermissions().getOwner(),libRock->getUUID(),catID,mCurrentFullName,cb);
	}
}

void FSLSLBridge :: processAttach(LLViewerObject *object, const LLViewerJointAttachment *attachment)
{
	if ((mpBridge == NULL) || (!mBridgeAttaching) || (!gAgentAvatarp->isSelf()))
		return;
	LLViewerInventoryItem *fsObject = gInventory.getItem(object->getAttachmentItemID());
	if (fsObject->getUUID() != mpBridge->getUUID())
		return;

	mpBridge->setDescription(mCurrentFullName);

	LLProfileParams profParams(LL_PCODE_PROFILE_CIRCLE, F32(0.230), F32(0.250), F32(0.95));
	LLPathParams pathParams(LL_PCODE_PATH_CIRCLE, F32(0), F32(0.2), F32(0.01), F32(0.01), F32(0.00), F32(0.0), 
		F32(0), F32(0), F32(0), F32(0), F32(0), F32(0.01), 0);
	
	object->setVolume(LLVolumeParams(profParams, pathParams), 0);

	object->setTETexture(0, LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903")); //transparent texture
	object->setTETexture(1, LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903")); //transparent texture
	object->setTETexture(2, LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903")); //transparent texture
	object->setTETexture(3, LLUUID("8dcd4a48-2d37-4909-9f78-f7a9eb4ef903")); //transparent texture

	//object->setTETexture(0, LLUUID("29de489d-0491-fb00-7dab-f9e686d31e83")); //another test texture
	object->setScale(LLVector3(F32(0.01), F32(0.01), F32(0.01)));
	object->sendShapeUpdate();
	object->markForUpdate(TRUE);

	//object->setFlags(FLAGS_TEMPORARY_ON_REZ, true);
	object->addFlags(FLAGS_TEMPORARY_ON_REZ);
	object->updateFlags();

	gInventory.updateItem(mpBridge);
    gInventory.notifyObservers();

	//add bridge script to object
	if (object)
	{
		create_script_inner(object);
	}
}

void FSLSLBridge :: create_script_inner(LLViewerObject* object)
{
	LLUUID catID = findFSCategory();

	LLPointer<LLInventoryCallback> cb = new FSLSLBridgeScriptCallback();
	create_inventory_item(gAgent.getID(), 
							gAgent.getSessionID(),
							catID,	//LLUUID::null, 
							LLTransactionID::tnull, 
							mCurrentFullName, 
							mCurrentFullName, 
							LLAssetType::AT_LSL_TEXT, 
							LLInventoryType::IT_LSL,
							NOT_WEARABLE, 
							mpBridge->getPermissions().getMaskNextOwner(), 
							cb);

}

//
// Bridge rez callback
//
FSLSLBridgeRezCallback :: FSLSLBridgeRezCallback()
{
}
FSLSLBridgeRezCallback :: ~FSLSLBridgeRezCallback()
{
}

void FSLSLBridgeRezCallback :: fire(const LLUUID& inv_item)
{
	if ((FSLSLBridge::instance().getBridge() != NULL) || inv_item.isNull() || !FSLSLBridge::instance().bridgeAttaching())
		return;

	//detach from default and put on the right point
	LLVOAvatarSelf::detachAttachmentIntoInventory(inv_item);
	LLViewerInventoryItem *item = gInventory.getItem(inv_item);
	FSLSLBridge::instance().setBridge(item);

	LLAttachmentsMgr::instance().addAttachment(inv_item, BRIDGE_POINT, FALSE, TRUE);
}


//
// Bridge script creation callback
//
FSLSLBridgeScriptCallback :: FSLSLBridgeScriptCallback()
{
}
FSLSLBridgeScriptCallback :: ~FSLSLBridgeScriptCallback()
{
}

void FSLSLBridgeScriptCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull() || !FSLSLBridge::instance().bridgeAttaching())
		return;

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) 
	{
		return;
	}
    gInventory.updateItem(item);
    gInventory.notifyObservers();

	LLViewerObject* obj = gAgentAvatarp->getWornAttachment(FSLSLBridge::instance().getBridge()->getUUID());

	//caps import 
	std::string url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	std::string isMono = "lsl2";  //could also be "mono"
	if (!url.empty())
	{
		const std::string fName = prepUploadFile();
		LLLiveLSLEditor::uploadAssetViaCapsStatic(url, fName, 
			obj->getID(), inv_item, isMono, true);
	}
	else
	{
		//can't complete bridge creation - detach and remove object, remove script
		LLVOAvatarSelf::detachAttachmentIntoInventory(FSLSLBridge::instance().getBridge()->getUUID());

		LLViewerInventoryItem* bridgeItem = FSLSLBridge::instance().getBridge();
		gInventory.deleteObject(bridgeItem->getUUID());
		bridgeItem->removeFromServer();

		gInventory.deleteObject(item->getUUID());
		item->removeFromServer();

		gInventory.notifyObservers();
	}
	llinfos << "updating script ID for bridge" << llendl;
	FSLSLBridge::instance().mScriptItemID = inv_item;
}

std::string FSLSLBridgeScriptCallback::prepUploadFile()
{
	std::string fName = gDirUtilp->getExpandedFilename(LL_PATH_FS_RESOURCES, UPLOAD_SCRIPT_1_7);
	std::string fNew = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,UPLOAD_SCRIPT_1_7);

	//open script text file
	typedef std::istream_iterator<char> istream_iterator;
	std::ifstream file(fName.c_str());

	typedef std::ostream_iterator<char> ostream_iterator;
	std::ofstream tempFile(fNew.c_str());
	
	file >> std::noskipws;
	std::copy(istream_iterator(file), istream_iterator(), ostream_iterator(tempFile));

	return fNew;
}

void FSLSLBridge :: checkBridgeScriptName(std::string fileName)
{
	if ((fileName.length() == 0) || !mBridgeAttaching)
		return;
	//need to parse out the last length of a GUID and compare to saved possible names.
	std::string fileOnly = fileName.substr(fileName.length()-UPLOAD_SCRIPT_1_7.length(), UPLOAD_SCRIPT_1_7.length());

	if (fileOnly == UPLOAD_SCRIPT_1_7)
	{
		//this is our script upload
		LLViewerObject* obj = gAgentAvatarp->getWornAttachment(mpBridge->getUUID());
		obj->saveScript(gInventory.getItem(mScriptItemID), TRUE, false);
		gInventory.deleteObject(mScriptItemID);
		gInventory.notifyObservers();
		cleanUpBridgeFolder();
		//announce yourself
		reportToNearbyChat("Bridge created.");

		mBridgeAttaching = false;
	}
}

//
// Helper functions
///
bool FSLSLBridge :: isItemAttached(LLUUID iID)
{
	return (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(iID));
}

LLUUID FSLSLBridge :: findFSCategory()
{
	LLUUID catID;
	catID = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);
	if(catID.isNull())
	{
		catID = gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER);
	}
	return catID;
}
LLViewerInventoryItem* FSLSLBridge :: findInvObject(std::string obj_name, LLUUID catID, LLAssetType::EType type)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	//gInventory.findCategoryByName
	LLUUID itemID;
	NameCollectFunctor namefunctor(obj_name);

	gInventory.collectDescendentsIf(catID,cats,items,FALSE,namefunctor);

	for (S32 iIndex = 0; iIndex < items.count(); iIndex++)
	{
		const LLViewerInventoryItem* itemp = items.get(iIndex);
		if (!itemp->getIsLinkType()  && (itemp->getType() == LLAssetType::AT_OBJECT))
		{
			itemID = itemp->getUUID();
			break;
		}
	}

	if (itemID.notNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(itemID);
		return item;
	}
	return NULL;
}

void FSLSLBridge :: reportToNearbyChat(std::string message)
// AO small utility method for chat alerts.
{	
	LLChat chat;
    chat.mText = message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLSD args;
	args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
	LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
}

void FSLSLBridge :: cleanUpBridgeFolder()
{
	LLUUID catID = findFSCategory();
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	//find all bridge and script duplicates and delete them
	NameCollectFunctor namefunctor(mCurrentFullName);
	gInventory.collectDescendentsIf(catID,cats,items,FALSE,namefunctor);

	for (S32 iIndex = 0; iIndex < items.count(); iIndex++)
	{
		const LLViewerInventoryItem* itemp = items.get(iIndex);
		if (!itemp->getIsLinkType()  && (itemp->getUUID() != mpBridge->getUUID()))
		{
			gInventory.deleteObject(itemp->getUUID());
		}
	}
	gInventory.notifyObservers();
}

void FSLSLBridge :: oldBridgeDetached(LLUUID oldID)
{
	gInventory.deleteObject(oldID);
	gInventory.notifyObservers();
}

bool FSLSLBridge :: isOldBridgeVersion(LLInventoryItem *item)
{
//	if (!item)
//		return;
//	if (!boost::regex_match(item->getName(), BridgePattern))
//
//	std::string str = item->getName();
//
//	(item) && boost::regex_match(item->getName(), AnyBridgePattern);
//
//	std::string tmpl = FS_BRIDGE_NAME;
//
//	string::size_type found = str.find_first_of(".");
// 
//	while( found != string::npos ) {}
//
//	std::string sMajor = str.substr(strlen(tmpl.c_str)-1, dotPos);
//	std::string sMinor = str.substr(strlen(tmpl.c_str)+strlen(sMajor));
//
//	int iMajor = atoi(sMajor);
//	float fMinor = atof(sMinor);

	return false;
}


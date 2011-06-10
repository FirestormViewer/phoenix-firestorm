/** 
 * @fslslbridge.h 
 * @FSLSLBridge header 
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
#ifndef FS_LSLBRIDGE_H
#define FS_LSLBRIDGE_H

#include "llviewerprecompiledheaders.h"
#include "llchat.h"
#include "llhttpclient.h"
#include "llinventorymodel.h"
#include "fslslbridgerequest.h"
#include "llviewerinventory.h"
#include "llinventoryobserver.h"

//
//-TT Client LSL Bridge File
//

class FSLSLBridge : public LLSingleton<FSLSLBridge>, public LLHTTPClient::Responder
{
	friend class FSLSLBridgeScriptCallback;
	friend class FSLSLBridgeRezCallback;
	friend class FSLSLBridgeInventoryObserver;

public:
	FSLSLBridge();
	~FSLSLBridge();

	bool lslToViewer(std::string message, LLUUID fromID, LLUUID ownerID);
	bool viewerToLSL(std::string message, FSLSLBridgeRequestResponder *responder = NULL);
	void initBridge();
	void recreateBridge();
	void processAttach(LLViewerObject *object, const LLViewerJointAttachment *attachment);
	bool bridgeAttaching() {return mBridgeAttaching; };
	void setBridge(LLViewerInventoryItem* item) { mpBridge = item; };
	LLViewerInventoryItem* getBridge() { return mpBridge; };
	void checkBridgeScriptName(std::string fileName);
	void startCreation();

protected:

	/*virtual*/ void inventoryChanged(LLViewerObject* obj,
									 LLInventoryObject::object_list_t* inv,
									 S32 serial_num,
									 void* queue);
	LLUUID	mScriptItemID;	//internal script ID

private:
	std::string				mCurrentURL;
	int						mCurrentChannel;
	bool					mBridgeAttaching;
	LLViewerInventoryItem*	mpBridge;
	std::string				mCurrentFullName;

private:
	LLViewerInventoryItem* findInvObject(std::string obj_name, LLUUID catID, LLAssetType::EType type);
	LLUUID findFSCategory();
	bool isItemAttached(LLUUID iID);
	void createNewBridge();
	void create_script_inner(LLViewerObject* object);
	bool isOldBridgeVersion(LLInventoryItem *item);
	void reportToNearbyChat(std::string message);
};


class FSLSLBridgeRezCallback : public LLInventoryCallback
{
public:
	FSLSLBridgeRezCallback();
	void fire(const LLUUID& inv_item);

protected:
	~FSLSLBridgeRezCallback();
};

class FSLSLBridgeScriptCallback : public LLInventoryCallback
{
public:
	FSLSLBridgeScriptCallback();
	void fire(const LLUUID& inv_item);
	std::string prepUploadFile();

protected:
	~FSLSLBridgeScriptCallback();
};

class FSLSLBridgeInventoryObserver : public LLInventoryFetchDescendentsObserver
{
public:
	FSLSLBridgeInventoryObserver(const LLUUID& cat_id = LLUUID::null):LLInventoryFetchDescendentsObserver(cat_id) {}
	/*virtual*/ void done() { FSLSLBridge::instance().startCreation(); 	gInventory.removeObserver(this); }

protected:
	~FSLSLBridgeInventoryObserver() {}

};

#endif // FS_LSLBRIDGE_H

/** 
 * @file fslslbridge.cpp
 * @brief FSLSLBridge implementation
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011-2015, The Phoenix Firestorm Project, Inc.
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

#include "fscommon.h"
#include "fslslbridge.h"
#include "fslslbridgerequest.h"

#include "apr_base64.h" // For getScriptInfo()
#include "llagent.h"
#include "llattachmentsmgr.h"
#include "llappearancemgr.h"
#include "llinventoryfunctions.h"
#include "llmaniptranslate.h"
#include "llpreviewscript.h"
#include "llselectmgr.h"
#include "lltrans.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

#if OPENSIM
#include "llviewernetwork.h"
#endif

static const std::string FS_BRIDGE_FOLDER = "#LSL Bridge";
static const std::string FS_BRIDGE_CONTAINER_FOLDER = "Landscaping";
static const U32 FS_BRIDGE_MAJOR_VERSION = 2;
static const U32 FS_BRIDGE_MINOR_VERSION = 20;
static const U32 FS_MAX_MINOR_VERSION = 99;
static const std::string UPLOAD_SCRIPT_CURRENT = "EBEDD1D2-A320-43f5-88CF-DD47BBCA5DFB.lsltxt";
static const std::string FS_STATE_ATTRIBUTE = "state=";
static const std::string FS_ERROR_ATTRIBUTE = "error=";

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
		if (item)
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
FSLSLBridge::FSLSLBridge():
					mBridgeCreating(false),
					mpBridge(NULL),
					mIsFirstCallDone(false),
					mAllowDetach(false),
					mFinishCreation(false)
{
	LL_INFOS("FSLSLBridge") << "Constructing FSLSLBridge" << LL_ENDL;
	mCurrentFullName = llformat("%s%d.%d", FS_BRIDGE_NAME.c_str(), FS_BRIDGE_MAJOR_VERSION, FS_BRIDGE_MINOR_VERSION);
}

FSLSLBridge::~FSLSLBridge()
{
}

bool FSLSLBridge::lslToViewer(const std::string& message, const LLUUID& fromID, const LLUUID& ownerID)
{
	LL_DEBUGS("FSLSLBridge") << message << LL_ENDL;

	//<FS:TS> FIRE-962: Script controls for built-in AO
	if (message.empty() || (message[0]) != '<')
	{
		return false; 		// quick exit if empty or no leading <
	}
	size_t closebracket = message.find('>');
	size_t firstblank = message.find(' ');
	size_t tagend;
	if (closebracket == std::string::npos)
	{
		tagend = firstblank;
	}
	else if (firstblank == std::string::npos)
	{
		tagend = closebracket;
	}
	else
	{
		tagend = (closebracket < firstblank) ? closebracket : firstblank;
	}
	if (tagend == std::string::npos)
	{
		return false;
	}
	std::string tag = message.substr(0, tagend + 1);
	std::string ourBridge = findFSCategory().asString();
	//</FS:TS> FIRE-962
	
	bool bridgeIsEnabled = gSavedSettings.getBOOL("UseLSLBridge");
	bool status = false;
	if (tag == "<bridgeURL>")
	{

		if (!bridgeIsEnabled)
		{
			// Hide handshake message if bridge is disabled in viewer settings but the bridge itself sent it, then quit - don't reply to the handshake
			return true;
		}

		// brutish parsing
		static const std::string bridge_url_tag = "<bridgeURL>";
		static const std::string bridge_auth_tag = "<bridgeAuth>";
		static const std::string bridge_ver_tag = "<bridgeVer>";
		size_t urlStart  = message.find(bridge_url_tag) + bridge_url_tag.size();
		size_t urlEnd    = message.find("</bridgeURL>");
		size_t authStart = message.find(bridge_auth_tag) + bridge_auth_tag.size();
		size_t authEnd   = message.find("</bridgeAuth>");
		size_t verStart  = message.find(bridge_ver_tag) + bridge_ver_tag.size();
		size_t verEnd    = message.find("</bridgeVer>");
		std::string bURL = message.substr(urlStart,urlEnd - urlStart);
		std::string bAuth = message.substr(authStart,authEnd - authStart);
		std::string bVer = message.substr(verStart,verEnd - verStart);

		// Verify Authorization
		if (ourBridge != bAuth)
		{
			LL_WARNS("FSLSLBridge") << "BridgeURL message received from ("<< bAuth <<") , but not from our registered bridge ("<< ourBridge <<"). Ignoring." << LL_ENDL;
			// Failing bridge authorization automatically kicks off a bridge rebuild. This correctly handles the
			// case where a user logs in from multiple computers which cannot have the bridgeAuth ID locally 
			// synchronized.
			
			
			// If something that looks like our current bridge is attached but failed auth, detach and recreate.
			const LLUUID catID = findFSCategory();
			LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID);
			if (fsBridge && get_is_item_worn(fsBridge->getUUID()))
			{
				mAllowDetach = true;
				LLVOAvatarSelf::detachAttachmentIntoInventory(fsBridge->getUUID());
				//This may have been an unfinished bridge. If so - stop the process before recreating.
				if (getBridgeCreating())
				{
					setBridgeCreating(false);
				}
				recreateBridge();
				return true; 
			}
			
			// If something that didn't look like our current bridge failed auth, don't recreate, it might interfere with a bridge creation in progress
			// or normal bridge startup.  Bridge creation isn't threadsafe yet.
			return true;
		}

		// Verify Version
		std::string receivedBridgeVersion = llformat("%s%s", FS_BRIDGE_NAME.c_str(), bVer.c_str());
		if (receivedBridgeVersion != mCurrentFullName)
		{
			LL_WARNS("FSLSLBridge") << "BridgeVer message received from ("<< bAuth <<") was ("<< receivedBridgeVersion <<"), but it should be different ("<< mCurrentFullName <<"). Recreating." << LL_ENDL;
			recreateBridge();
			return true;
		}

		// Save the inworld UUID of this attached bridge for later checking
		mBridgeUUID = fromID;
		
		// Get URL
		mCurrentURL = bURL;
		LL_INFOS("FSLSLBridge") << "New Bridge URL is: " << mCurrentURL << LL_ENDL;
		
		if (!mpBridge)
		{
			LLUUID catID = findFSCategory();
			LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID);
			mpBridge = fsBridge;
		}

		status = viewerToLSL("URL Confirmed", new FSLSLBridgeRequestResponder());
		if (!mIsFirstCallDone)
		{
			//on first call from bridge, confirm that we are here
			//then check options use

			if (gSavedPerAccountSettings.getF32("UseLSLFlightAssist") > 0.f)
			{
				viewerToLSL(llformat("UseLSLFlightAssist|%.1f", gSavedPerAccountSettings.getF32("UseLSLFlightAssist")), new FSLSLBridgeRequestResponder());
			}

			// <FS:PP> Inform user, if movelock was enabled at login
			if (gSavedPerAccountSettings.getBOOL("UseMoveLock"))
			{
				updateBoolSettingValue("UseMoveLock");
				report_to_nearby_chat(LLTrans::getString("MovelockEnabling"));
			}
			// </FS:PP>

			updateBoolSettingValue("RelockMoveLockAfterMovement");
			updateIntegrations();
			mIsFirstCallDone = true;

		}
		// <FS:PP> FIRE-11924: Refresh movelock position after region change (crossing/teleporting), if lock was enabled
		// Not called right after logging in, and only if movelock was enabled during transition
		else if (gSavedPerAccountSettings.getBOOL("UseMoveLock"))
		{
			if (!gSavedSettings.getBOOL("RelockMoveLockAfterRegionChange"))
			{
				// Don't call for update here and only change setting to 'false', getCommitSignal()->connect->boost in llviewercontrol.cpp will send a message to Bridge anyway
				gSavedPerAccountSettings.setBOOL("UseMoveLock", false);
				report_to_nearby_chat(LLTrans::getString("MovelockDisabling"));
			}
			else
			{
				// RelockMoveLockAfterRegionChange is 'true'? Then re-lock the movelock by sending a request to Bridge for coordinates update with current 'true' from UseMoveLock
				updateBoolSettingValue("UseMoveLock");
			}
		}
		// </FS:PP>

		return true;
	}
	
	//<FS:TS> FIRE-962: Script controls for built-in AO
	if (fromID != mBridgeUUID || !bridgeIsEnabled)
	{
		return false;		// ignore if not from the bridge, or bridge is disabled
	}
	if (tag == "<clientAO ")
	{
		status = true;
		size_t valuepos = message.find(FS_STATE_ATTRIBUTE);
		if (valuepos != std::string::npos)
		{
			if (message.substr(valuepos + FS_STATE_ATTRIBUTE.size(), 2) == "on")
			{
				gSavedPerAccountSettings.setBOOL("UseAO", TRUE);
			}
			else if (message.substr(valuepos + FS_STATE_ATTRIBUTE.size(), 3) == "off")
			{
				gSavedPerAccountSettings.setBOOL("UseAO", FALSE);
			}
			else
			{
				LL_WARNS("FSLSLBridge") << "AO control - Received unknown state" << LL_ENDL;
			}
		}
	}
	//</FS:TS> FIRE-962

	// <FS:PP> Get script info response
	else if (tag == "<bridgeGetScriptInfo>")
	{
		size_t tag_size = tag.size();
		status = true;
		size_t getScriptInfoEnd = message.find("</bridgeGetScriptInfo>");
		if (getScriptInfoEnd != std::string::npos)
		{
			std::string getScriptInfoString = message.substr(tag_size, getScriptInfoEnd - tag_size);
			std::istringstream strStreamGetScriptInfo(getScriptInfoString);
			std::string scriptInfoToken;
			LLSD scriptInfoArray = LLSD::emptyArray();
			S32 scriptInfoArrayCount = 0;
			while (std::getline(strStreamGetScriptInfo, scriptInfoToken, ','))
			{
				LLStringUtil::trim(scriptInfoToken);
				if (scriptInfoArrayCount == 0)
				{
					// First value, OBJECT_NAME, should be passed from Bridge as encoded in base64
					// Encoding eliminates problems with special characters and commas for CSV
					S32 base64Length = apr_base64_decode_len(scriptInfoToken.c_str());
					if (base64Length)
					{
						std::vector<U8> base64VectorData;
						base64VectorData.resize(base64Length);
						base64Length = apr_base64_decode_binary(&base64VectorData[0], scriptInfoToken.c_str());
						base64VectorData.resize(base64Length);
						std::string base64FinalData(base64VectorData.begin(), base64VectorData.end());
						scriptInfoToken = base64FinalData;
					}
					else
					{
						LL_WARNS("FSLSLBridge") << "ScriptInfo - OBJECT_NAME cannot be decoded" << LL_ENDL;
					}
				}
				scriptInfoArray.append(scriptInfoToken);
				++scriptInfoArrayCount;
			}

			if (scriptInfoArrayCount == 6)
			{
				LLStringUtil::format_map_t args;
				args["OBJECT_NAME"] = scriptInfoArray[0].asString();
				args["OBJECT_RUNNING_SCRIPT_COUNT"] = scriptInfoArray[1].asString();
				args["OBJECT_TOTAL_SCRIPT_COUNT"] = scriptInfoArray[2].asString();
				args["OBJECT_SCRIPT_MEMORY"] = scriptInfoArray[3].asString();
				args["OBJECT_SCRIPT_TIME"] = scriptInfoArray[4].asString();
				if (scriptInfoArray[5].asString() != "0.000000")
				{
					LLStringUtil::format_map_t args2;
					args2["OBJECT_CHARACTER_TIME"] = scriptInfoArray[5].asString();
					args["PATHFINDING_TEXT"] = " " + format_string(LLTrans::getString("fsbridge_script_info_pf"), args2);
				}
				else
				{
					args["PATHFINDING_TEXT"] = "";
				}
				report_to_nearby_chat(format_string(LLTrans::getString("fsbridge_script_info"), args));
			}
			else
			{
				report_to_nearby_chat(LLTrans::getString("fsbridge_error_scriptinfonotfound"));
				LL_WARNS("FSLSLBridge") << "ScriptInfo - Object to check is invalid or out of range (warning returned by viewer, data somehow passed bridge script check)" << LL_ENDL;
			}

		}
		else
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_error_scriptinfomalformed"));
			LL_WARNS("FSLSLBridge") << "ScriptInfo - Received malformed response from bridge (missing ending tag)" << LL_ENDL;
		}
	}
	// </FS:PP>

	// <FS:PP> Movelock state response
	else if (tag == "<bridgeMovelock ")
	{
		status = true;
		size_t valuepos = message.find(FS_STATE_ATTRIBUTE);
		if (valuepos != std::string::npos)
		{
			if (message.substr(valuepos + FS_STATE_ATTRIBUTE.size(), 1) == "1")
			{
				report_to_nearby_chat(LLTrans::getString("MovelockEnabled"));
			}
			else if (message.substr(valuepos + FS_STATE_ATTRIBUTE.size(), 1) == "0")
			{
				report_to_nearby_chat(LLTrans::getString("MovelockDisabled"));
			}
			else
			{
				LL_WARNS("FSLSLBridge") << "Movelock - Received unknown state" << LL_ENDL;
			}
		}
	}
	// </FS:PP>

	// <FS:PP> Error responses handling
	else if (tag == "<bridgeError ")
	{
		status = true;
		size_t valuepos = message.find(FS_ERROR_ATTRIBUTE);
		if (valuepos != std::string::npos)
		{
			if (message.substr(valuepos + FS_ERROR_ATTRIBUTE.size(), 9) == "injection")
			{
				report_to_nearby_chat(LLTrans::getString("fsbridge_error_injection"));
				LL_WARNS("FSLSLBridge") << "Script injection detected" << LL_ENDL;
			}
			else if (message.substr(valuepos + FS_ERROR_ATTRIBUTE.size(), 18) == "scriptinfonotfound")
			{
				report_to_nearby_chat(LLTrans::getString("fsbridge_error_scriptinfonotfound"));
				LL_WARNS("FSLSLBridge") << "ScriptInfo - Object to check is invalid or out of range (warning returned by bridge)" << LL_ENDL;
			}
			else if (message.substr(valuepos + FS_ERROR_ATTRIBUTE.size(), 7) == "wrongvm")
			{
				report_to_nearby_chat(LLTrans::getString("fsbridge_error_wrongvm"));
				LL_WARNS("FSLSLBridge") << "Script is using old LSO (16 KB memory limit) instead of new Mono (64 KB memory limit) virtual machine, which creates high probability of stack-heap collision and bridge failure by running out of memory" << LL_ENDL;
			}
			else
			{
				LL_WARNS("FSLSLBridge") << "ErrorReporting - Received unknown error type" << LL_ENDL;
			}
		}
	}
	// </FS:PP>

	return status;
}

bool FSLSLBridge::canUseBridge()
{
	static LLCachedControl<bool> sUseLSLBridge(gSavedSettings, "UseLSLBridge");
	return (isBridgeValid() && sUseLSLBridge);
}

bool FSLSLBridge::viewerToLSL(const std::string& message, FSLSLBridgeRequestResponder* responder)
{
	LL_DEBUGS("FSLSLBridge") << message << LL_ENDL;

	if (!canUseBridge())
	{
		return false;
	}

	if (!responder)
	{
		responder = new FSLSLBridgeRequestResponder();
	}
	LLHTTPClient::post(mCurrentURL, LLSD(message), responder);

	return true;
}

bool FSLSLBridge::updateBoolSettingValue(const std::string& msgVal)
{
	std::string boolVal = "0";

	if (gSavedPerAccountSettings.getBOOL(msgVal))
	{
		boolVal = "1";
	}

	return viewerToLSL(msgVal + "|" + boolVal, new FSLSLBridgeRequestResponder());
}

bool FSLSLBridge::updateBoolSettingValue(const std::string& msgVal, bool contentVal)
{
	std::string boolVal = "0";

	if (contentVal)
	{
		boolVal = "1";
	}

	return viewerToLSL(msgVal + "|" + boolVal, new FSLSLBridgeRequestResponder());
}

void FSLSLBridge::updateIntegrations()
{
	viewerToLSL(llformat(
		"ExternalIntegration|%d|%d",
		gSavedPerAccountSettings.getBOOL("BridgeIntegrationOC"),
		gSavedPerAccountSettings.getBOOL("BridgeIntegrationLM")
	), new FSLSLBridgeRequestResponder());
}

//
//Bridge initialization
//
void FSLSLBridge::recreateBridge()
{
	LL_INFOS("FSLSLBridge") << "Recreating bridge start" << LL_ENDL;

	// Don't create on OpenSim. We need to fallback to another creation process there, unfortunately.
	// There is no way to ensure a rock object will ever be in a grid's Library.
#if OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		return;
	}
#endif

	if (!gSavedSettings.getBOOL("UseLSLBridge"))
	{
		//<FS:TS> FIRE-11746: Recreate should throw error if disabled
		LL_WARNS("FSLSLBridge") << "Asked to create bridge, but bridge is disabled. Aborting." << LL_ENDL;
		report_to_nearby_chat(LLTrans::getString("fsbridge_cant_create_disabled"));
		setBridgeCreating(false);
		//</FS:TS> FIRE-11746
		return;
	}

	if (gSavedSettings.getBOOL("NoInventoryLibrary"))
	{
		LL_WARNS("FSLSLBridge") << "Asked to create bridge, but we don't have a library. Aborting." << LL_ENDL;
		report_to_nearby_chat(LLTrans::getString("fsbridge_no_library"));
		setBridgeCreating(false);
		return;
	}

	if (mBridgeCreating)
	{
		LL_WARNS("FSLSLBridge") << "Bridge creation already in progress, aborting new attempt." << LL_ENDL;
		report_to_nearby_chat(LLTrans::getString("fsbridge_already_creating"));
		return;
	}

	//announce yourself
	report_to_nearby_chat(LLTrans::getString("fsbridge_creating"));

	LLUUID catID = findFSCategory();

	FSLSLBridgeInventoryPreCreationCleanupObserver* bridgeInventoryObserver = new FSLSLBridgeInventoryPreCreationCleanupObserver(catID);
	bridgeInventoryObserver->startFetch();
	if (bridgeInventoryObserver->isFinished())
	{
		bridgeInventoryObserver->done();
	}
	else
	{
		gInventory.addObserver(bridgeInventoryObserver);
	}
}

void FSLSLBridge::cleanUpPreCreation()
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	NameCollectFunctor namefunctor(mCurrentFullName);
	gInventory.collectDescendentsIf(findFSCategory(), cats, items, FALSE, namefunctor);

	mAllowedDetachables.clear();
	for (LLInventoryModel::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		LLUUID item_id= (*it)->getUUID();
		if (get_is_item_worn(item_id))
		{
			mAllowedDetachables.push_back(item_id);
			LLVOAvatarSelf::detachAttachmentIntoInventory(item_id);
		}
	}

	// Immediately start with finishCleanUpPreCreation if we don't have
	// any attachments we need to wait for until they got detached
	if (mAllowedDetachables.size() == 0)
	{
		finishCleanUpPreCreation();
	}
}

// Called either by cleanUpPreCreation directly or via FSLSLBridgeStartCreationTimer
// after all pending detachments have been processed
void FSLSLBridge::finishCleanUpPreCreation()
{
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	NameCollectFunctor namefunctor(mCurrentFullName);
	gInventory.collectDescendentsIf(findFSCategory(), cats, items, FALSE, namefunctor);

	for (LLInventoryModel::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		remove_inventory_object((*it)->getUUID(), NULL);
	}
	gInventory.notifyObservers();

	// clear the stored bridge ID - we are starting over.
	mpBridge = NULL; //the object itself will get cleaned up when new one is created.

	setBridgeCreating(true);
	mFinishCreation = false;

	createNewBridge();
}

// Called by RlvAttachmentLockWatchdog::onDetach to check if attachment
// is allowed to get detached - required if attachment spot is locked!
bool FSLSLBridge::canDetach(const LLUUID& item_id)
{
	LL_DEBUGS("FSLSLBridge") << "canDetach" << LL_ENDL;
	return ((mpBridge && item_id == mpBridge->getUUID()) || (std::find(mAllowedDetachables.begin(), mAllowedDetachables.end(), item_id) != mAllowedDetachables.end()));
}

void FSLSLBridge::initBridge()
{
	LL_INFOS("FSLSLBridge") << "Initializing FSLSLBridge" << LL_ENDL;

	if (!gSavedSettings.getBOOL("UseLSLBridge"))
	{
		return;
	}

	if (gSavedSettings.getBOOL("NoInventoryLibrary"))
	{
		LL_WARNS("FSLSLBridge") << "Asked to create bridge, but we don't have a library. Aborting." << LL_ENDL;
		report_to_nearby_chat(LLTrans::getString("fsbridge_no_library"));
		setBridgeCreating(false);
		return;
	}

	LLUUID catID = findFSCategory();
	LLUUID libCatID = findFSBridgeContainerCategory();

	//check for inventory load
	// AH: Use overloaded LLInventoryFetchDescendentsObserver to check for load of
	// bridge and bridge rock category before doing anything!
	LL_INFOS("FSLSLBridge") << "initBridge called. gInventory.isInventoryUsable = " << (gInventory.isInventoryUsable() ? "true" : "false") << LL_ENDL;
	uuid_vec_t cats;
	cats.push_back(catID);
	cats.push_back(libCatID);
	FSLSLBridgeInventoryObserver* bridgeInventoryObserver = new FSLSLBridgeInventoryObserver(cats);
	bridgeInventoryObserver->startFetch();
	if (bridgeInventoryObserver->isFinished())
	{
		bridgeInventoryObserver->done();
	}
	else
	{
		gInventory.addObserver(bridgeInventoryObserver);
	}
}


// Gets called by the Init, when inventory loaded (FSLSLBridgeInventoryObserver::done()).
void FSLSLBridge::startCreation()
{
	LL_INFOS("FSLSLBridge") << "Entering startCreation" << LL_ENDL;

	if (getBridgeCreating())
	{
		LL_INFOS("FSLSLBridge") << "startCreation called while recreating bridge - aborting" << LL_ENDL;
		return;
	}

	if (!isAgentAvatarValid())
	{
		LL_WARNS("FSLSLBridge") << "AgentAvatar is not valid" << LL_ENDL;
		return;
	}

	LL_INFOS("FSLSLBridge") << "startCreation called. gInventory.isInventoryUsable = " << (gInventory.isInventoryUsable() ? "true" : "false") << LL_ENDL;

	//if bridge object doesn't exist - create and attach it, update script.
	const LLUUID catID = findFSCategory();

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID);

	//detach everything else
	LL_INFOS("FSLSLBridge") << "Detaching other bridges..." << LL_ENDL;
	detachOtherBridges();

	if (!fsBridge)
	{
		LL_INFOS("FSLSLBridge") << "Bridge not found in inventory, creating new one..." << LL_ENDL;
		// Don't create on OpenSim. We need to fallback to another creation process there, unfortunately.
		// There is no way to ensure a rock object will ever be in a grid's Library.
#if OPENSIM
		if (LLGridManager::getInstance()->isInOpenSim())
		{
			return;
		}
#endif
		setBridgeCreating(true);
		mFinishCreation = false;
		//announce yourself
		report_to_nearby_chat(LLTrans::getString("fsbridge_creating"));

		createNewBridge();
	}
	else
	{
		//TODO need versioning - see isOldBridgeVersion()
		mpBridge = fsBridge;
		if (!isItemAttached(mpBridge->getUUID()))
		{
			if (!LLAppearanceMgr::instance().getIsInCOF(mpBridge->getUUID()))
			{
				LL_INFOS("FSLSLBridge") << "Bridge not attached but found in inventory, reattaching..." << LL_ENDL;

				//Is this a valid bridge - wear it.
				LLAttachmentsMgr::instance().addAttachmentRequest(mpBridge->getUUID(), FS_BRIDGE_POINT, TRUE, TRUE);
				//from here, the attach shoould report to ProcessAttach and make sure bridge is valid.
			}
			else
			{
				LL_INFOS("FSLSLBridge") << "Bridge not found but in CoF. Waiting for automatic attach..." << LL_ENDL;
			}
		}
	}
}

void FSLSLBridge::createNewBridge()
{
	//check if user has a bridge
	const LLUUID catID = findFSCategory();

	//attach the Linden rock from the library (will resize as soon as attached)
	const LLUUID libID = gInventory.getLibraryRootFolderID();
	LLViewerInventoryItem* libRock = findInvObject(LIB_ROCK_NAME, libID);
	//shouldn't happen but just in case
	if (libRock != NULL)
	{
		LL_INFOS("FSLSLBridge") << "Cloning a new Bridge container from the Library..." << LL_ENDL;

		//copy the library item to inventory and put it on 
		LLPointer<LLInventoryCallback> cb = new FSLSLBridgeRezCallback();
		copy_inventory_item(gAgent.getID(), libRock->getPermissions().getOwner(), libRock->getUUID(), catID, mCurrentFullName, cb);
	}
	else
	{
		LL_WARNS("FSLSLBridge") << "Bridge container not found in the Library!" << LL_ENDL;
		// AH: Set to false or we won't be able to start another bridge creation
		// process in this session!
		setBridgeCreating(false);
	}
}

void FSLSLBridge::processAttach(LLViewerObject* object, const LLViewerJointAttachment* attachment)
{
	LL_INFOS("FSLSLBridge") << "Entering processAttach, checking the bridge container - gInventory.isInventoryUsable = " << (gInventory.isInventoryUsable() ? "true" : "false") << LL_ENDL;

	if ((!gAgentAvatarp->isSelf()) || (attachment->getName() != FS_BRIDGE_ATTACHMENT_POINT_NAME))
	{
		LL_WARNS("FSLSLBridge") << "Bridge not created. Our bridge container attachment isn't named correctly." << LL_ENDL;
		if (mBridgeCreating)
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_failure_creation_bad_name"));
			setBridgeCreating(false); //in case we interrupted the creation
		}
		return;
	}

	LLViewerInventoryItem* fsObject = gInventory.getItem(object->getAttachmentItemID());
	if (!fsObject) //just in case
	{
		LL_WARNS("FSLSLBridge") << "Bridge container is still NULL in inventory. Aborting." << LL_ENDL;
		if (mBridgeCreating)
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_failure_creation_null"));
			setBridgeCreating(false); //in case we interrupted the creation
		}
		return;
	}

	if (!mpBridge) //user is attaching an existing bridge?
	{
		//is it the right version?
		if (fsObject->getName() != mCurrentFullName)
		{
			mAllowDetach = true;
			LLVOAvatarSelf::detachAttachmentIntoInventory(fsObject->getUUID());
			LL_WARNS("FSLSLBridge") << "Attempt to attach to bridge point an object other than current bridge" << LL_ENDL;
			report_to_nearby_chat(LLTrans::getString("fsbridge_failure_attach_wrong_object"));
			if (mBridgeCreating)
			{
				setBridgeCreating(false); //in case we interrupted the creation
			}
			return;
		}

		//is it in the right place?
		const LLUUID catID = findFSCategory();
		if (catID != fsObject->getParentUUID())
		{
			//the object is not where we think it is. Kick it off.
			mAllowDetach = true;
			LLVOAvatarSelf::detachAttachmentIntoInventory(fsObject->getUUID());
			LL_WARNS("FSLSLBridge") << "Bridge container isn't in the correct inventory location. Detaching it and aborting." << LL_ENDL;
			if (mBridgeCreating)
			{
				report_to_nearby_chat(LLTrans::getString("fsbridge_failure_attach_wrong_location"));
				setBridgeCreating(false); //in case we interrupted the creation
			}
			return;
		}
		mpBridge = fsObject;
	}

	LL_DEBUGS("FSLSLBridge") << "Bridge container is attached, mpBridge not NULL, avatar is self, point is bridge, all is good." << LL_ENDL;


	if (fsObject->getUUID() != mpBridge->getUUID())
	{
		//something odd just got attached to bridge?
		LL_WARNS("FSLSLBridge") << "Something unknown just got attached to bridge point, detaching and aborting." << LL_ENDL;
		if (mBridgeCreating)
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_failure_attach_point_in_use"));
			setBridgeCreating(false); //in case we interrupted the creation
		}
		LLVOAvatarSelf::detachAttachmentIntoInventory(mpBridge->getUUID());
		return;
	}
	LL_DEBUGS("FSLSLBridge") << "Bridge container found is the same bridge we saved, id matched." << LL_ENDL;

	if (!mBridgeCreating) //just an attach. See what it is
	{
		// AH: We need to request objects inventory first before we can
		// do anything with it!
		LL_INFOS("FSLSLBridge") << "Requesting bridge inventory contents..." << LL_ENDL;
		object->registerInventoryListener(this, NULL);
		object->requestInventory();
	}
	else
	{
		configureBridgePrim(object);
	}
}

void FSLSLBridge::inventoryChanged(LLViewerObject* object,
								LLInventoryObject::object_list_t* inventory,
								S32 serial_num,
								void* user_data)
{
	object->removeInventoryListener(this);

	LL_INFOS("FSLSLBridge") << "Received object inventory for existing bridge prim. Checking contents..." << LL_ENDL;

	//are we attaching the right thing? Check size and script
	LLInventoryObject::object_list_t inventory_objects;
	object->getInventoryContents(inventory_objects);

	if (object->flagInventoryEmpty())
	{
		LL_INFOS("FSLSLBridge") << "Empty bridge detected- re-enter creation process" << LL_ENDL;
		setBridgeCreating(true);
	}
	else if (inventory_objects.size() > 0)
	{
		S32 count(0);

		LLInventoryObject::object_list_t::iterator it = inventory_objects.begin();
		LLInventoryObject::object_list_t::iterator end = inventory_objects.end();
		bool isOurScript = false;
		for ( ; it != end; ++it)
		{
			LLInventoryItem* item = ((LLInventoryItem*)((LLInventoryObject*)(*it)));

			// AH: Somehow always contains a wonky object item with creator
			// UUID = NULL UUID and asset type AT_NONE - don't count it
			if (item->getType() != LLAssetType::AT_NONE)
			{
				count++;
			}

			if (item->getType() == LLAssetType::AT_LSL_TEXT)
			{
				if (item->getCreatorUUID() == gAgentID)
				{
					isOurScript = true;
				}
				else //problem, not our script
				{
					LL_WARNS("FSLSLBridge") << "The bridge inventory contains a script not created by user." << LL_ENDL;
				}
			}
		}
		if (count == 1 && isOurScript) //We attached a valid bridge. Run along.
		{
			LL_INFOS("FSLSLBridge") << "Bridge inventory is okay." << LL_ENDL;
		}
		else
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_warning_unexpected_items"));
			LL_WARNS("FSLSLBridge") << "The bridge inventory contains items other than bridge script." << LL_ENDL;
			if (!isOurScript)	//some junk but no valid script? Unlikely to happen, but lets add script anyway.
			{
				setBridgeCreating(true);
			}
		}
	}
	else
	{
		LL_WARNS("FSLSLBridge") << "Bridge not empty, but we're unable to retrieve contents." << LL_ENDL;
	}

	//modify the rock size and texture
	if (getBridgeCreating())
	{
		mFinishCreation = false;
		configureBridgePrim(object);
	}
	else if (mFinishCreation)
	{
		LL_INFOS("FSLSLBridge") << "Bridge created." << LL_ENDL;
		mFinishCreation = false;
		mAllowedDetachables.clear();
		report_to_nearby_chat(LLTrans::getString("fsbridge_created"));
	}
}

void FSLSLBridge::configureBridgePrim(LLViewerObject* object)
{
	if (!isBridgeValid())
	{
		LL_WARNS("FSLSLBridge") << "Bridge not valid" << LL_ENDL;
		return;
	}

	//modify the rock size and texture
	LL_INFOS("FSLSLBridge") << "Bridge container found after attachment, configuring prim..." << LL_ENDL;
	setupBridgePrim(object);

	//add bridge script to object
	LL_INFOS("FSLSLBridge") << "Creating bridge script..." << LL_ENDL;
	create_script_inner();
}

void FSLSLBridge::processDetach(LLViewerObject* object, const LLViewerJointAttachment* attachment)
{
	LL_INFOS("FSLSLBridge") << "Entering processDetach" << LL_ENDL;

	// Begin cleanup part
	uuid_vec_t::iterator found = std::find(mAllowedDetachables.begin(), mAllowedDetachables.end(), object->getAttachmentItemID());
	if (found != mAllowedDetachables.end())
	{
		mAllowedDetachables.erase(found);
		if (mAllowedDetachables.size() == 0)
		{
			LL_INFOS("FSLSLBridge") << "No attached objects left. Starting creation timer..." << LL_ENDL;
			// Wait a few seconds before calling finishCleanUpPreCreation
			// which will clean up the LSLBridge folder
			new FSLSLBridgeStartCreationTimer();
		}
		return;
	}
	// End cleanup part

	if (gSavedSettings.getBOOL("UseLSLBridge") && !mAllowDetach)
	{
		LL_INFOS("FSLSLBridge") << "Re-attaching bridge" << LL_ENDL;
		LLViewerInventoryItem* inv_object = gInventory.getItem(object->getAttachmentItemID());
		if (inv_object && FSLSLBridge::instance().mpBridge && FSLSLBridge::instance().mpBridge->getUUID() == inv_object->getUUID())
		{
			LLAttachmentsMgr::instance().addAttachmentRequest(inv_object->getUUID(), FS_BRIDGE_POINT, TRUE, TRUE);
		}

		return;
	}
	mAllowDetach = false;

	if (gAgentAvatarp.isNull() || (!gAgentAvatarp->isSelf()) || (attachment == NULL) || (attachment->getName() != FS_BRIDGE_ATTACHMENT_POINT_NAME))
	{
		LL_WARNS("FSLSLBridge") << "Couldn't detach bridge, object has wrong name or avatar wasn't self." << LL_ENDL;
		return;
	}

	LLViewerInventoryItem* fsObject = gInventory.getItem(object->getAttachmentItemID());
	if (fsObject == NULL) //just in case
	{
		LL_WARNS("FSLSLBridge") << "Couldn't detach bridge. inventory object was NULL." << LL_ENDL;
		return;
	}

	if (mFinishCreation)
	{
		LL_INFOS("FSLSLBridge") << "Bridge detached to save settings. Starting re-attach timer..." << LL_ENDL;
		new FSLSLBridgeReAttachTimer(object->getAttachmentItemID());
		return;
	}

	//is it in the right place?
	const LLUUID catID = findFSCategory();
	if (catID != fsObject->getParentUUID())
	{
		//that was in the wrong place. It's not ours.
		LL_WARNS("FSLSLBridge") << "Bridge seems to be the wrong inventory category. Aborting detachment." << LL_ENDL;
		return;
	}
	if (mpBridge && mpBridge->getUUID() == fsObject->getUUID()) 
	{
		mpBridge = NULL;
		report_to_nearby_chat(LLTrans::getString("fsbridge_detached"));
		mIsFirstCallDone = false;
		if (mBridgeCreating)
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_warning_not_finished"));
			setBridgeCreating(false); //in case we interrupted the creation
			mAllowedDetachables.clear();
		}
	}

	LL_INFOS("FSLSLBridge") << "processDetach Finished" << LL_ENDL;
}

void FSLSLBridge::setupBridgePrim(LLViewerObject* object)
{
	LL_DEBUGS("FSLSLBridge") << "Entering bridge container setup..." << LL_ENDL;

	LLProfileParams profParams(LL_PCODE_PROFILE_CIRCLE, 0.230f, 0.250f, 0.95f);
	LLPathParams pathParams(LL_PCODE_PATH_CIRCLE, 0.2f, 0.22f, 
		0.f, 350.f,		//scale
		0.f, 0.f,		//shear
		0.f, 0.f,		//twist
		0.f,			//offset
		0.f, 0.f,		//taper
		0.05f, 0.05f);	//revolutions, skew
	pathParams.setRevolutions(1.f);
	object->setVolume(LLVolumeParams(profParams, pathParams), 0);
	object->sendShapeUpdate();

	// Set scale and position
	object->setScale(LLVector3(0.01f, 0.01f, 0.01f));
	object->setPosition(LLVector3(2.f, 2.f, 2.f));
	LLManip::rebuild(object);
	gAgentAvatarp->clampAttachmentPositions();

	U8 data[24];
	htonmemcpy(&data[0], &(object->getPosition().mV), MVT_LLVector3, 12);
	htonmemcpy(&data[12], &(object->getScale().mV), MVT_LLVector3, 12);

	gMessageSystem->newMessage("MultipleObjectUpdate");
	gMessageSystem->nextBlockFast(_PREHASH_AgentData);
	gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgentID);
	gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
	gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
	gMessageSystem->addU8Fast(_PREHASH_Type, UPD_POSITION | UPD_SCALE | UPD_LINKED_SETS);
	gMessageSystem->addBinaryDataFast(_PREHASH_Data, data, 24);
	gMessageSystem->sendReliable(object->getRegion()->getHost());

	// Set textures
	for (U8 i = 0; i < object->getNumTEs(); ++i)
	{
		LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture( IMG_INVISIBLE );
		object->setTEImage(i, image); //transparent texture
	}
	object->sendTEUpdate();

	object->addFlags(FLAGS_TEMPORARY_ON_REZ);
	object->updateFlags();

	object->setChanged(LLXform::MOVED | LLXform::SILHOUETTE | LLXform::TEXTURE);
	object->markForUpdate(TRUE);

	LL_DEBUGS("FSLSLBridge") << "End bridge container setup." << LL_ENDL;
}

void FSLSLBridge::create_script_inner()
{
	if (!isBridgeValid())
	{
		LL_WARNS("FSLSLBridge") << "Bridge no valid" << LL_ENDL;
		return;
	}

	const LLUUID catID = findFSCategory();

	LLPointer<LLInventoryCallback> cb = new FSLSLBridgeScriptCallback();
	create_inventory_item(gAgent.getID(),
							gAgent.getSessionID(),
							catID,
							LLTransactionID::tnull,
							mCurrentFullName,
							mCurrentFullName,
							LLAssetType::AT_LSL_TEXT,
							LLInventoryType::IT_LSL,
							NOT_WEARABLE,
							PERM_ALL,
							cb);

}

//
// Bridge rez callback
//
FSLSLBridgeRezCallback::FSLSLBridgeRezCallback()
{
}
FSLSLBridgeRezCallback::~FSLSLBridgeRezCallback()
{
}

void FSLSLBridgeRezCallback::fire(const LLUUID& inv_item)
{
	// this is the first attach - librock got copied and worn on hand - but the ID is now real.
	if ((FSLSLBridge::instance().getBridge() != NULL))
	{
		LL_INFOS("FSLSLBridge") << "Ignoring bridgerezcallback, already have bridge" << LL_ENDL;
		return;
	}

	if (inv_item.isNull())
	{
		LL_INFOS("FSLSLBridge") << "Ignoring bridgerezcallback, new bridge is null" << LL_ENDL;
		return;
	}

	if (!FSLSLBridge::instance().getBridgeCreating())
	{
		LL_INFOS("FSLSLBridge") << "Ignoring bridgerezcallback, we're not flagged as creating a bridge." << LL_ENDL;
		return;
	}

	if (!isAgentAvatarValid())
	{
		LL_WARNS("FSLSLBridge") << "Agent is 0, bailing out" << LL_ENDL;
		return;
	}

	LL_INFOS("FSLSLBridge") << "Bridge rez callback fired, looking for object..." << LL_ENDL;

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);

	item->setDescription(item->getName());
	item->setComplete(TRUE);
	item->updateServer(FALSE);

	gInventory.updateItem(item);
	gInventory.notifyObservers();

	//from this point on, this is our bridge - accept no substitutes!
	FSLSLBridge::instance().setBridge(item);
	
	LL_INFOS("FSLSLBridge") << "Attaching bridge to the 'bridge' attachment point..." << LL_ENDL;
	LLAttachmentsMgr::instance().addAttachmentRequest(inv_item, FS_BRIDGE_POINT, TRUE, TRUE);
}


//
// Bridge script creation callback
//
FSLSLBridgeScriptCallback::FSLSLBridgeScriptCallback()
{
}
FSLSLBridgeScriptCallback::~FSLSLBridgeScriptCallback()
{
}

void FSLSLBridgeScriptCallback::fire(const LLUUID& inv_item)
{
	if (inv_item.isNull() || !FSLSLBridge::instance().getBridgeCreating())
	{
		LL_WARNS("FSLSLBridge") << "BridgeScriptCallback fired, but target item was null or bridge isn't marked as under creation. Ignoring." << LL_ENDL;
		return;
	}

	LLViewerInventoryItem* item = gInventory.getItem(inv_item);
	if (!item) 
	{
		LL_WARNS("FSLSLBridge") << "BridgeScriptCallback Can't find target item in inventory. Ignoring." << LL_ENDL;
		return;
	}
	
	if (!isAgentAvatarValid())
	{
		LL_WARNS("FSLSLBridge") << "Agent is 0, bailing out" << LL_ENDL;
		return;
	}

	gInventory.updateItem(item);
	gInventory.notifyObservers();

	LLViewerObject* obj(NULL);

	if (FSLSLBridge::instance().isBridgeValid())
	{
		obj = gAgentAvatarp->getWornAttachment(FSLSLBridge::instance().getBridge()->getUUID());
	}
	else
	{
		LL_WARNS("FSLSLBridge") << "Bridge non valid" << LL_ENDL;
	}

	//caps import 
	std::string url;
	
	if (gAgent.getRegion())
	{
		url = gAgent.getRegion()->getCapability("UpdateScriptAgent");
	}

	bool cleanup = false;
	std::string isMono = "mono";  //could also be "lsl2"
	if (!url.empty() && obj)
	{
		const std::string fName = prepUploadFile();
		if (!fName.empty())
		{
			LLLiveLSLEditor::uploadAssetViaCapsStatic(url, fName, obj->getID(), inv_item, isMono, true);
			LL_INFOS("FSLSLBridge") << "updating script ID for bridge" << LL_ENDL;
			FSLSLBridge::instance().mScriptItemID = inv_item;
		}
		else
		{
			report_to_nearby_chat(LLTrans::getString("fsbridge_failure_creation_create_script"));
			cleanup = true;
		}
	}
	else
	{
		cleanup = true;
	}

	if (cleanup)
	{
		//can't complete bridge creation - detach and remove object, remove script
		//try to clean up and go away. Fail.
		if (FSLSLBridge::instance().isBridgeValid())
		{
			FSLSLBridge::instance().mAllowDetach = true;
			LLVOAvatarSelf::detachAttachmentIntoInventory(FSLSLBridge::instance().getBridge()->getUUID());
		}
	
		FSLSLBridge::instance().cleanUpBridge();
		//also clean up script remains
		remove_inventory_object(item->getUUID(), NULL);
		gInventory.notifyObservers();
		LL_WARNS("FSLSLBridge") << "Can't update bridge script. Purging bridge." << LL_ENDL;
		return;
	}
}

std::string FSLSLBridgeScriptCallback::prepUploadFile()
{
	const std::string fName = gDirUtilp->getExpandedFilename(LL_PATH_FS_RESOURCES, UPLOAD_SCRIPT_CURRENT);
	const std::string fNew = gDirUtilp->getExpandedFilename(LL_PATH_CACHE,UPLOAD_SCRIPT_CURRENT);

	LLFILE* fpIn = LLFile::fopen(fName, "rt");
	if (!fpIn)
	{
		LL_WARNS("FSLSLBridge") << "Cannot open script resource file" << LL_ENDL;
		return "";
	}
	fseek(fpIn, 0, SEEK_END);
	long lSize = ftell(fpIn);
	rewind(fpIn);

	std::vector< char > vctData;
	vctData.resize(lSize + 1, 0);
	if (lSize != fread(&vctData[0], 1, lSize, fpIn))
	{
		LL_WARNS("FSLSLBridge") << "Size mismatch during read" << LL_ENDL;
	}

	LLFile::close(fpIn);

	std::string bridgeScript( (char const*)&vctData[0] );

	const std::string bridgekey = "BRIDGEKEY";
	bridgeScript.replace(bridgeScript.find(bridgekey), bridgekey.length(), FSLSLBridge::getInstance()->findFSCategory().asString());

	LLFILE *fpOut = LLFile::fopen(fNew, "wt");
	if (!fpOut)
	{
		LL_WARNS("FSLSLBridge") << "Cannot open script upload file" << LL_ENDL;
		return "";
	}

	if (bridgeScript.size() != fwrite(bridgeScript.c_str(), 1, bridgeScript.size(), fpOut))
	{
		LL_WARNS("FSLSLBridge") << "Size mismatch during write" << LL_ENDL;
	}
	LLFile::close(fpOut);

	return fNew;
}

void FSLSLBridge::checkBridgeScriptName(const std::string& fileName)
{
	if ((fileName.empty()) || !mBridgeCreating)
	{
		LL_WARNS("FSLSLBridge") << "Bridge script length was zero, or bridge was not marked as under creation. Aborting." << LL_ENDL; 
		return;
	}

	if (!isBridgeValid())
	{
		LL_WARNS("FSLSLBridge") << "Bridge not valid (anymore)" << LL_ENDL;
		cleanUpBridge();
		return;
	}

	if (!isAgentAvatarValid())
	{
		LL_WARNS("FSLSLBridge") << "No agent avatar" << LL_ENDL;
		cleanUpBridge();
		return;
	}

	//need to parse out the last length of a GUID and compare to saved possible names.
	const std::string fileOnly = fileName.substr(fileName.length() - UPLOAD_SCRIPT_CURRENT.length(), UPLOAD_SCRIPT_CURRENT.length());

	if (fileOnly == UPLOAD_SCRIPT_CURRENT)
	{
		//this is our script upload
		LLViewerObject* obj = gAgentAvatarp->getWornAttachment(mpBridge->getUUID());
		if (!obj)
		{
			//something happened to our object. Try to fail gracefully.
			LL_WARNS("FSLSLBridge") << "Couldn't find worn bridge attachment" << LL_ENDL;
			cleanUpBridge();
			return;
		}
		obj->saveScript(gInventory.getItem(mScriptItemID), TRUE, false);
		FSLSLBridgeCleanupTimer* objTimer = new FSLSLBridgeCleanupTimer(1.0f);
		objTimer->startTimer();
	}
}

BOOL FSLSLBridgeCleanupTimer::tick()
{
	FSLSLBridge::instance().finishBridge();
	stopTimer();
	return TRUE;
}

void FSLSLBridge::cleanUpBridge()
{
	//something unexpected went wrong. Try to clean up and not crash.
	LL_WARNS("FSLSLBridge") << "Bridge object not found. Can't proceed with creation, exiting." << LL_ENDL;
	report_to_nearby_chat(LLTrans::getString("fsbridge_failure_not_found"));

	if (isBridgeValid())
	{
		remove_inventory_object(mpBridge->getUUID(), NULL);
	}

	gInventory.notifyObservers();
	mpBridge = NULL;
	mAllowedDetachables.clear();
	setBridgeCreating(false);
}

void FSLSLBridge::finishBridge()
{
	LL_INFOS("FSLSLBridge") << "Finishing bridge" << LL_ENDL;

	setBridgeCreating(false);
	mIsFirstCallDone = false;
	cleanUpOldVersions();
	cleanUpBridgeFolder();

	mAllowDetach = true;
	mFinishCreation = true;
	LLVOAvatarSelf::detachAttachmentIntoInventory(getBridge()->getUUID());
}

//
// Helper functions
//
bool FSLSLBridge::isItemAttached(const LLUUID& iID)
{
	return (isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(iID));
}

LLUUID FSLSLBridge::findFSCategory()
{
	if (!mBridgeFolderID.isNull())
	{
		return mBridgeFolderID;
	}

	LLUUID fsCatID;
	LLUUID bridgeCatID;

	fsCatID = gInventory.findCategoryByName(ROOT_FIRESTORM_FOLDER);
	if (!fsCatID.isNull())
	{
		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(fsCatID, cats, items);
		if (cats)
		{
			for (LLInventoryModel::cat_array_t::iterator it = cats->begin(); it != cats->end(); ++it)
			{
				if ((*it)->getName() == FS_BRIDGE_FOLDER)
				{
					bridgeCatID = (*it)->getUUID();
					break;
				}
			}
		}
	}
	else
	{
		fsCatID = gInventory.createNewCategory(gInventory.getRootFolderID(), LLFolderType::FT_NONE, ROOT_FIRESTORM_FOLDER);
	}

	if (bridgeCatID.isNull())
	{
		bridgeCatID = gInventory.createNewCategory(fsCatID, LLFolderType::FT_NONE, FS_BRIDGE_FOLDER);
	}

	mBridgeFolderID = bridgeCatID;

	return mBridgeFolderID;
}

LLUUID FSLSLBridge::findFSBridgeContainerCategory()
{
	LL_INFOS("FSLSLBridge") << "Retrieving FSBridge container category (" << FS_BRIDGE_CONTAINER_FOLDER << ")" << LL_ENDL;
	if (mBridgeContainerFolderID.notNull())
	{
		LL_INFOS("FSLSLBridge") << "Returning FSBridge container category UUID from instance: " << mBridgeContainerFolderID << LL_ENDL;
		return mBridgeContainerFolderID;
	}

	LLUUID LibRootID = gInventory.getLibraryRootFolderID();
	if (LibRootID.notNull())
	{
		LLInventoryModel::item_array_t* items;
		LLInventoryModel::cat_array_t* cats;
		gInventory.getDirectDescendentsOf(LibRootID, cats, items);
		if (cats)
		{
			for (LLInventoryModel::cat_array_t::iterator it = cats->begin(); it != cats->end(); ++it)
			{
				if ((*it)->getName() == "Objects")
				{
					LLUUID LibObjectsCatID = (*it)->getUUID();
					if (LibObjectsCatID.notNull())
					{
						LLInventoryModel::item_array_t* objects_items;
						LLInventoryModel::cat_array_t* objects_cats;
						gInventory.getDirectDescendentsOf(LibObjectsCatID, objects_cats, objects_items);
						if (objects_cats)
						{
							for (LLInventoryModel::cat_array_t::iterator object_it = objects_cats->begin(); object_it != objects_cats->end(); ++object_it)
							{
								if ((*object_it)->getName() == FS_BRIDGE_CONTAINER_FOLDER)
								{
									mBridgeContainerFolderID = (*object_it)->getUUID();
									LL_INFOS("FSLSLBridge") << "FSBridge container category found in library. UUID: " << mBridgeContainerFolderID << LL_ENDL;
									gInventory.fetchDescendentsOf(mBridgeContainerFolderID);
									return mBridgeContainerFolderID;
								}
							}
						}
					}
				}
			}
		}
	}

	LL_WARNS("FSLSLBridge") << "FSBridge container category not found in library!" << LL_ENDL;
	return LLUUID();
}

LLViewerInventoryItem* FSLSLBridge::findInvObject(const std::string& obj_name, const LLUUID& catID)
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	LLUUID itemID;
	NameCollectFunctor namefunctor(obj_name);

	gInventory.collectDescendentsIf(catID, cats, items, FALSE, namefunctor);

	for (LLViewerInventoryItem::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem* itemp = *it;
		if (!itemp->getIsLinkType() && (itemp->getType() == LLAssetType::AT_OBJECT))
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

void FSLSLBridge::cleanUpBridgeFolder(const std::string& nameToCleanUp)
{
	//LL_INFOS("FSLSLBridge") << "Cleaning leftover scripts and bridges for folder " << nameToCleanUp << LL_ENDL;
	
	if (!isBridgeValid())
	{
		LL_WARNS("FSLSLBridge") << "Bridge no valid" << LL_ENDL;
		return;
	}

	LLUUID catID = findFSCategory();
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	//find all bridge and script duplicates and delete them
	//NameCollectFunctor namefunctor(mCurrentFullName);
	NameCollectFunctor namefunctor(nameToCleanUp);
	gInventory.collectDescendentsIf(catID, cats, items, FALSE, namefunctor);

	for (LLViewerInventoryItem::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem* itemp = *it;
		if (!itemp->getIsLinkType() && (itemp->getUUID() != mpBridge->getUUID()))
		{
			remove_inventory_object(itemp->getUUID(), NULL);
		}
	}

	gInventory.notifyObservers();
}

void FSLSLBridge::cleanUpBridgeFolder()
{
	cleanUpBridgeFolder(mCurrentFullName);
}

void FSLSLBridge::cleanUpOldVersions()
{
	std::string mProcessingName;

	for (S32 i = 1; i <= FS_BRIDGE_MAJOR_VERSION; i++)
	{
		S32 minor_tip;
		if (i < FS_BRIDGE_MAJOR_VERSION)
		{
			minor_tip = FS_MAX_MINOR_VERSION;
		}
		else
		{
			minor_tip = FS_BRIDGE_MINOR_VERSION;
		}

		for (S32 j = 0; j < minor_tip; j++)
		{
			mProcessingName = llformat("%s%d.%d", FS_BRIDGE_NAME.c_str(), i, j);
			cleanUpBridgeFolder(mProcessingName);
		}
	}
}

void FSLSLBridge::detachOtherBridges()
{
	LLUUID catID = findFSCategory();
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;

	LLViewerInventoryItem* fsBridge = findInvObject(mCurrentFullName, catID);

	//detach everything except current valid bridge - if any
	gInventory.collectDescendents(catID,cats,items,FALSE);

	for (LLViewerInventoryItem::item_array_t::iterator it = items.begin(); it != items.end(); ++it)
	{
		const LLViewerInventoryItem* itemp = *it;
		if (get_is_item_worn(itemp->getUUID()) &&
			((fsBridge == NULL) || (itemp->getUUID() != fsBridge->getUUID())))
		{
			LLVOAvatarSelf::detachAttachmentIntoInventory(itemp->getUUID());
		}
	}
}

BOOL FSLSLBridgeReAttachTimer::tick()
{
	LL_INFOS("FSLSLBridge") << "Re-attaching bridge after creation..." << LL_ENDL;
	LLViewerInventoryItem* inv_object = gInventory.getItem(mBridgeUUID);
	if (inv_object && FSLSLBridge::instance().mpBridge && FSLSLBridge::instance().mpBridge->getUUID() == inv_object->getUUID())
	{
		LLAttachmentsMgr::instance().addAttachmentRequest(inv_object->getUUID(), FS_BRIDGE_POINT, TRUE, TRUE);
	}

	return TRUE;
}


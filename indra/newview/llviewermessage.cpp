/** 
 * @file llviewermessage.cpp
 * @brief Dumping ground for viewer-side message system callbacks.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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
#include "llviewermessage.h"
#include "boost/lexical_cast.hpp"

// Linden libraries
#include "llanimationstates.h"
#include "llaudioengine.h" 
#include "llavataractions.h"
#include "llavatarnamecache.h"		// IDEVO HACK
#include "lleconomy.h"
#include "lleventtimer.h"
#include "llfloaterreg.h"
#include "llfolderview.h"
#include "llfollowcamparams.h"
#include "llinventorydefines.h"
#include "lllslconstants.h"
#include "llregionhandle.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llteleportflags.h"
#include "lltoastnotifypanel.h"
#include "lltransactionflags.h"
#include "llvfile.h"
#include "llvfs.h"
#include "llxfermanager.h"
#include "mean_collision_data.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llcallingcard.h"
#include "llbuycurrencyhtml.h"
#include "llfirstuse.h"
#include "llfloaterbump.h"
#include "llfloaterbuyland.h"
#include "llfloaterland.h"
#include "llfloaterregioninfo.h"
#include "llfloaterlandholdings.h"
#include "llfloaterpreference.h"
#include "llfloatersidepanelcontainer.h"
#include "llfloatersnapshot.h"
#include "llhudeffecttrail.h"
#include "llhudmanager.h"
#include "llimview.h"
#include "llinventoryfunctions.h"
#include "llinventoryobserver.h"
#include "llinventorypanel.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaterimnearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
#include "llmarketplacefunctions.h"
#include "llnotifications.h"
#include "llnotificationsutil.h"
#include "llpanelgrouplandmoney.h"
#include "llrecentpeople.h"
#include "llscriptfloater.h"
#include "llscriptruntimeperms.h"
#include "llselectmgr.h"
#include "llstartup.h"
#include "llsky.h"
#include "llslurl.h"
#include "llstatenums.h"
#include "llstatusbar.h"
#include "llimview.h"
#include "llspeakers.h"
#include "lltrans.h"
#include "lltranslate.h"
#include "llviewerfoldertype.h"
#include "llvoavatar.h"				// IDEVO HACK
#include "lluri.h"
#include "llviewergenericmessage.h"
#include "llviewermenu.h"
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewernetwork.h" // <FS:AW opensim currency support>
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvlmanager.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "llworldmap.h"
#include "pipeline.h"
#include "llfloaterworldmap.h"
#include "llviewerdisplay.h"
#include "llkeythrottle.h"
#include "llgroupactions.h"
#include "llagentui.h"
#include "llpanelblockedlist.h"
#include "llpanelplaceprofile.h"
#include "llviewerregion.h"
#include "llfloaterregionrestarting.h"
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0a)
#include "rlvactions.h"
#include "rlvhandler.h"
#include "rlvinventory.h"
#include "rlvui.h"
// [/RLVa:KB]

#include <boost/algorithm/string/split.hpp> //
#include <boost/regex.hpp>
#include <boost/foreach.hpp>

#include "llnotificationmanager.h" //
#include "llexperiencecache.h"

#include "llexperiencecache.h"

// Firestorm inclues
#include "animationexplorer.h"		// <FS:Zi> Animation Explorer
#include "fsareasearch.h"
#include "fsassetblacklist.h"
#include "fscommon.h"
#include "fsdata.h"
#include "fsfloaterplacedetails.h"
#include "fsradar.h"
#include "fskeywords.h" // <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground
#include "fslightshare.h" // <FS:CR> FIRE-5118 - Lightshare support
#include "fslslbridge.h"
#include "fsmoneytracker.h"
#include "llfloaterbump.h"
#include "llfloaterreg.h"
#include "llfriendcard.h"
#include "llgiveinventory.h"
#include "lllandmarkactions.h"
#include "lltexturefetch.h"
#include "sound_ids.h"
#include "tea.h" // <FS:AW opensim currency support>
#include "NACLantispam.h"
#include "chatbar_as_cmdline.h"

#if LL_MSVC
// disable boost::lexical_cast warning
#pragma warning (disable:4702)
#endif

extern void on_new_message(const LLSD& msg);

//
// Constants
//
const F32 CAMERA_POSITION_THRESHOLD_SQUARED = 0.001f * 0.001f;

// Determine how quickly residents' scripts can issue question dialogs
// Allow bursts of up to 5 dialogs in 10 seconds. 10*2=20 seconds recovery if throttle kicks in
static const U32 LLREQUEST_PERMISSION_THROTTLE_LIMIT	= 5;     // requests
static const F32 LLREQUEST_PERMISSION_THROTTLE_INTERVAL	= 10.0f; // seconds

extern BOOL gDebugClicks;
extern bool gShiftFrame;

// function prototypes
bool check_offer_throttle(const std::string& from_name, bool check_only);
bool check_asset_previewable(const LLAssetType::EType asset_type);
static void process_money_balance_reply_extended(LLMessageSystem* msg);
bool handle_trusted_experiences_notification(const LLSD&);

//inventory offer throttle globals
LLFrameTimer gThrottleTimer;
const U32 OFFER_THROTTLE_MAX_COUNT=5; //number of items per time period
const F32 OFFER_THROTTLE_TIME=10.f; //time period in seconds

// Agent Update Flags (U8)
const U8 AU_FLAGS_NONE      		= 0x00;
const U8 AU_FLAGS_HIDETITLE      	= 0x01;
const U8 AU_FLAGS_CLIENT_AUTOPILOT	= 0x02;

bool friendship_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLMessageSystem* msg = gMessageSystem;
	const LLSD& payload = notification["payload"];
	LLNotificationPtr notification_ptr = LLNotifications::instance().find(notification["id"].asUUID());

    // this will be skipped if the user offering friendship is blocked
    if (notification_ptr)
    {
	    // add friend to recent people list
	    LLRecentPeople::instance().add(payload["from_id"]);

	    switch(option)
	    {
	    case 0:
	    {
		    // accept
		    LLAvatarTracker::formFriendship(payload["from_id"]);

		    const LLUUID fid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);

		    // This will also trigger an onlinenotification if the user is online
		    msg->newMessageFast(_PREHASH_AcceptFriendship);
		    msg->nextBlockFast(_PREHASH_AgentData);
		    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		    msg->nextBlockFast(_PREHASH_TransactionBlock);
		    msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
		    msg->nextBlockFast(_PREHASH_FolderData);
		    msg->addUUIDFast(_PREHASH_FolderID, fid);
		    msg->sendReliable(LLHost(payload["sender"].asString()));

		    LLSD payload = notification["payload"];
		    LLNotificationsUtil::add("FriendshipAcceptedByMe",
				    notification["substitutions"], payload);
		    break;
	    }
	    case 1: // Decline
	    {
		    LLSD payload = notification["payload"];
		    LLNotificationsUtil::add("FriendshipDeclinedByMe",
				    notification["substitutions"], payload);
	    }
	    // fall-through
	    case 2: // Send IM - decline and start IM session
		    {
			    // decline
			    // We no longer notify other viewers, but we DO still send
			    // the rejection to the simulator to delete the pending userop.
			    msg->newMessageFast(_PREHASH_DeclineFriendship);
			    msg->nextBlockFast(_PREHASH_AgentData);
			    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
			    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
			    msg->nextBlockFast(_PREHASH_TransactionBlock);
			    msg->addUUIDFast(_PREHASH_TransactionID, payload["session_id"]);
			    msg->sendReliable(LLHost(payload["sender"].asString()));

			    // start IM session
			    if(2 == option)
			    {
				    LLAvatarActions::startIM(payload["from_id"].asUUID());
			    }
	    }
	    default:
		    // close button probably, possibly timed out
		    break;
	    }
		// TODO: this set of calls has undesirable behavior under Windows OS (CHUI-985):
		// here appears three additional toasts instead one modified
		// need investigation and fix

// <FS:Ansariel> [FS communication UI] Commenting out CHUI-112;
//               Reposting the notification form will squeeze it somewhere
//               within the IM floater and we don't need it for our comm. UI.
//	    // LLNotificationFormPtr modified_form(new LLNotificationForm(*notification_ptr->getForm()));
//	    // modified_form->setElementEnabled("Accept", false);
//	    // modified_form->setElementEnabled("Decline", false);
//	    // notification_ptr->updateForm(modified_form);
//	    // notification_ptr->repost();
//// [SL:KB] - Patch: UI-Notifications | Checked: 2013-05-09 (Catznip-3.5)
//		// Assume that any offer notification with "getCanBeStored() == true" is the result of RLVa routing it to the notifcation syswell
//		//*const*/ LLNotificationsUI::LLScreenChannel* pChannel = LLNotificationsUI::LLChannelManager::instance().getNotificationScreenChannel();
//		//*const*/ LLNotificationsUI::LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification["id"].asUUID()) : NULL;
//		//if ( (!pToast) || (!pToast->getCanBeStored()) )
//		//{
//// [/SL:KB]
//		//	notification_ptr->repost();
// </FS:Ansariel>
    }

	return false;
}
static LLNotificationFunctorRegistration friendship_offer_callback_reg("OfferFriendship", friendship_offer_callback);
static LLNotificationFunctorRegistration friendship_offer_callback_reg_nm("OfferFriendshipNoMessage", friendship_offer_callback);

// Functions
//

void give_money(const LLUUID& uuid, LLViewerRegion* region, S32 amount, BOOL is_group,
				S32 trx_type, const std::string& desc)
{
	if(0 == amount || !region) return;
	amount = abs(amount);
	LL_INFOS("Messaging") << "give_money(" << uuid << "," << amount << ")"<< LL_ENDL;
	if(can_afford_transaction(amount))
	{
//		gStatusBar->debitBalance(amount);
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessageFast(_PREHASH_MoneyTransferRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_MoneyData);
		msg->addUUIDFast(_PREHASH_SourceID, gAgent.getID() );
		msg->addUUIDFast(_PREHASH_DestID, uuid);
		msg->addU8Fast(_PREHASH_Flags, pack_transaction_flags(FALSE, is_group));
		msg->addS32Fast(_PREHASH_Amount, amount);
		msg->addU8Fast(_PREHASH_AggregatePermNextOwner, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addU8Fast(_PREHASH_AggregatePermInventory, (U8)LLAggregatePermissions::AP_EMPTY);
		msg->addS32Fast(_PREHASH_TransactionType, trx_type );
		msg->addStringFast(_PREHASH_Description, desc);
		msg->sendReliable(region->getHost());
	}
	else
	{
		LLStringUtil::format_map_t args;
		args["AMOUNT"] = llformat("%d", amount);
		LLBuyCurrencyHTML::openCurrencyFloater( LLTrans::getString("giving", args), amount );
	}
}

void send_complete_agent_movement(const LLHost& sim_host)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_CompleteAgentMovement);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);
	msg->sendReliable(sim_host);
}

void process_logout_reply(LLMessageSystem* msg, void**)
{
	// The server has told us it's ok to quit.
	LL_DEBUGS("Messaging") << "process_logout_reply" << LL_ENDL;

	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	LLUUID session_id;
	msg->getUUID("AgentData", "SessionID", session_id);
	if((agent_id != gAgent.getID()) || (session_id != gAgent.getSessionID()))
	{
		LL_WARNS("Messaging") << "Bogus Logout Reply" << LL_ENDL;
	}

	LLInventoryModel::update_map_t parents;
	S32 count = msg->getNumberOfBlocksFast( _PREHASH_InventoryData );
	for(S32 i = 0; i < count; ++i)
	{
		LLUUID item_id;
		msg->getUUIDFast(_PREHASH_InventoryData, _PREHASH_ItemID, item_id, i);

		if( (1 == count) && item_id.isNull() )
		{
			// Detect dummy item.  Indicates an empty list.
			break;
		}

		// We do not need to track the asset ids, just account for an
		// updated inventory version.
		LL_INFOS("Messaging") << "process_logout_reply itemID=" << item_id << LL_ENDL;
		LLInventoryItem* item = gInventory.getItem( item_id );
		if( item )
		{
			parents[item->getParentUUID()] = 0;
			gInventory.addChangedMask(LLInventoryObserver::INTERNAL, item_id);
		}
		else
		{
			LL_INFOS("Messaging") << "process_logout_reply item not found: " << item_id << LL_ENDL;
		}
	}
    LLAppViewer::instance()->forceQuit();
}

void process_layer_data(LLMessageSystem *mesgsys, void **user_data)
{
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegion(mesgsys->getSender());

	LL_DEBUGS_ONCE("SceneLoadTiming") << "Received layer data" << LL_ENDL;

	if(!regionp)
	{
		LL_WARNS() << "Invalid region for layer data." << LL_ENDL;
		return;
	}
	S32 size;
	S8 type;

	mesgsys->getS8Fast(_PREHASH_LayerID, _PREHASH_Type, type);
	size = mesgsys->getSizeFast(_PREHASH_LayerData, _PREHASH_Data);
	if (0 == size)
	{
		LL_WARNS("Messaging") << "Layer data has zero size." << LL_ENDL;
		return;
	}
	if (size < 0)
	{
		// getSizeFast() is probably trying to tell us about an error
		LL_WARNS("Messaging") << "getSizeFast() returned negative result: "
			<< size
			<< LL_ENDL;
		return;
	}
	U8 *datap = new U8[size];
	mesgsys->getBinaryDataFast(_PREHASH_LayerData, _PREHASH_Data, datap, size);
	LLVLData *vl_datap = new LLVLData(regionp, type, datap, size);
	if (mesgsys->getReceiveCompressedSize())
	{
		gVLManager.addLayerData(vl_datap, (S32Bytes)mesgsys->getReceiveCompressedSize());
	}
	else
	{
		gVLManager.addLayerData(vl_datap, (S32Bytes)mesgsys->getReceiveSize());
	}
}

// S32 exported_object_count = 0;
// S32 exported_image_count = 0;
// S32 current_object_count = 0;
// S32 current_image_count = 0;

// extern LLNotifyBox *gExporterNotify;
// extern LLUUID gExporterRequestID;
// extern std::string gExportDirectory;

// extern LLUploadDialog *gExportDialog;

// std::string gExportedFile;

// std::map<LLUUID, std::string> gImageChecksums;

// void export_complete()
// {
// 		LLUploadDialog::modalUploadFinished();
// 		gExporterRequestID.setNull();
// 		gExportDirectory = "";

// 		LLFILE* fXML = LLFile::fopen(gExportedFile, "rb");		/* Flawfinder: ignore */
// 		fseek(fXML, 0, SEEK_END);
// 		long length = ftell(fXML);
// 		fseek(fXML, 0, SEEK_SET);
// 		U8 *buffer = new U8[length + 1];
// 		size_t nread = fread(buffer, 1, length, fXML);
// 		if (nread < (size_t) length)
// 		{
// 			LL_WARNS("Messaging") << "Short read" << LL_ENDL;
// 		}
// 		buffer[nread] = '\0';
// 		fclose(fXML);

// 		char *pos = (char *)buffer;
// 		while ((pos = strstr(pos+1, "<sl:image ")) != 0)
// 		{
// 			char *pos_check = strstr(pos, "checksum=\"");

// 			if (pos_check)
// 			{
// 				char *pos_uuid = strstr(pos_check, "\">");

// 				if (pos_uuid)
// 				{
// 					char image_uuid_str[UUID_STR_SIZE];		/* Flawfinder: ignore */
// 					memcpy(image_uuid_str, pos_uuid+2, UUID_STR_SIZE-1);		/* Flawfinder: ignore */
// 					image_uuid_str[UUID_STR_SIZE-1] = 0;
					
// 					LLUUID image_uuid(image_uuid_str);

// 					LL_INFOS("Messaging") << "Found UUID: " << image_uuid << LL_ENDL;

// 					std::map<LLUUID, std::string>::iterator itor = gImageChecksums.find(image_uuid);
// 					if (itor != gImageChecksums.end())
// 					{
// 						LL_INFOS("Messaging") << "Replacing with checksum: " << itor->second << LL_ENDL;
// 						if (!itor->second.empty())
// 						{
// 							memcpy(&pos_check[10], itor->second.c_str(), 32);		/* Flawfinder: ignore */
// 						}
// 					}
// 				}
// 			}
// 		}

// 		LLFILE* fXMLOut = LLFile::fopen(gExportedFile, "wb");		/* Flawfinder: ignore */
// 		if (fwrite(buffer, 1, length, fXMLOut) != length)
// 		{
// 			LL_WARNS("Messaging") << "Short write" << LL_ENDL;
// 		}
// 		fclose(fXMLOut);

// 		delete [] buffer;
// }


// void exported_item_complete(const LLTSCode status, void *user_data)
// {
// 	//std::string *filename = (std::string *)user_data;

// 	if (status < LLTS_OK)
// 	{
// 		LL_WARNS("Messaging") << "Export failed!" << LL_ENDL;
// 	}
// 	else
// 	{
// 		++current_object_count;
// 		if (current_image_count == exported_image_count && current_object_count == exported_object_count)
// 		{
// 			LL_INFOS("Messaging") << "*** Export complete ***" << LL_ENDL;

// 			export_complete();
// 		}
// 		else
// 		{
// 			gExportDialog->setMessage(llformat("Exported %d/%d object files, %d/%d textures.", current_object_count, exported_object_count, current_image_count, exported_image_count));
// 		}
// 	}
// }

// struct exported_image_info
// {
// 	LLUUID image_id;
// 	std::string filename;
// 	U32 image_num;
// };

// void exported_j2c_complete(const LLTSCode status, void *user_data)
// {
// 	exported_image_info *info = (exported_image_info *)user_data;
// 	LLUUID image_id = info->image_id;
// 	U32 image_num = info->image_num;
// 	std::string filename = info->filename;
// 	delete info;

// 	if (status < LLTS_OK)
// 	{
// 		LL_WARNS("Messaging") << "Image download failed!" << LL_ENDL;
// 	}
// 	else
// 	{
// 		LLFILE* fIn = LLFile::fopen(filename, "rb");		/* Flawfinder: ignore */
// 		if (fIn) 
// 		{
// 			LLPointer<LLImageJ2C> ImageUtility = new LLImageJ2C;
// 			LLPointer<LLImageTGA> TargaUtility = new LLImageTGA;

// 			fseek(fIn, 0, SEEK_END);
// 			S32 length = ftell(fIn);
// 			fseek(fIn, 0, SEEK_SET);
// 			U8 *buffer = ImageUtility->allocateData(length);
// 			if (fread(buffer, 1, length, fIn) != length)
// 			{
// 				LL_WARNS("Messaging") << "Short read" << LL_ENDL;
// 			}
// 			fclose(fIn);
// 			LLFile::remove(filename);

// 			// Convert to TGA
// 			LLPointer<LLImageRaw> image = new LLImageRaw();

// 			ImageUtility->updateData();
// 			ImageUtility->decode(image, 100000.0f);
			
// 			TargaUtility->encode(image);
// 			U8 *data = TargaUtility->getData();
// 			S32 data_size = TargaUtility->getDataSize();

// 			std::string file_path = gDirUtilp->getDirName(filename);
			
// 			std::string output_file = llformat("%s/image-%03d.tga", file_path.c_str(), image_num);//filename;
// 			//S32 name_len = output_file.length();
// 			//strcpy(&output_file[name_len-3], "tga");
// 			LLFILE* fOut = LLFile::fopen(output_file, "wb");		/* Flawfinder: ignore */
// 			char md5_hash_string[33];		/* Flawfinder: ignore */
// 			strcpy(md5_hash_string, "00000000000000000000000000000000");		/* Flawfinder: ignore */
// 			if (fOut)
// 			{
// 				if (fwrite(data, 1, data_size, fOut) != data_size)
// 				{
// 					LL_WARNS("Messaging") << "Short write" << LL_ENDL;
// 				}
// 				fseek(fOut, 0, SEEK_SET);
// 				fclose(fOut);
// 				fOut = LLFile::fopen(output_file, "rb");		/* Flawfinder: ignore */
// 				LLMD5 my_md5_hash(fOut);
// 				my_md5_hash.hex_digest(md5_hash_string);
// 			}

// 			gImageChecksums.insert(std::pair<LLUUID, std::string>(image_id, md5_hash_string));
// 		}
// 	}

// 	++current_image_count;
// 	if (current_image_count == exported_image_count && current_object_count == exported_object_count)
// 	{
// 		LL_INFOS("Messaging") << "*** Export textures complete ***" << LL_ENDL;
// 			export_complete();
// 	}
// 	else
// 	{
// 		gExportDialog->setMessage(llformat("Exported %d/%d object files, %d/%d textures.", current_object_count, exported_object_count, current_image_count, exported_image_count));
// 	}
//}

void process_derez_ack(LLMessageSystem*, void**)
{
	if(gViewerWindow) gViewerWindow->getWindow()->decBusyCount();
}

void process_places_reply(LLMessageSystem* msg, void** data)
{
	LLUUID query_id;

	msg->getUUID("AgentData", "QueryID", query_id);
	if (query_id.isNull())
	{
		LLFloaterLandHoldings::processPlacesReply(msg, data);
	}
	else if(gAgent.isInGroup(query_id))
	{
		LLPanelGroupLandMoney::processPlacesReply(msg, data);
	}
	else
	{
		LL_WARNS("Messaging") << "Got invalid PlacesReply message" << LL_ENDL;
	}
}

void send_sound_trigger(const LLUUID& sound_id, F32 gain)
{
	if (sound_id.isNull() || gAgent.getRegion() == NULL)
	{
		// disconnected agent or zero guids don't get sent (no sound)
		return;
	}

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_SoundTrigger);
	msg->nextBlockFast(_PREHASH_SoundData);
	msg->addUUIDFast(_PREHASH_SoundID, sound_id);
	// Client untrusted, ids set on sim
	msg->addUUIDFast(_PREHASH_OwnerID, LLUUID::null );
	msg->addUUIDFast(_PREHASH_ObjectID, LLUUID::null );
	msg->addUUIDFast(_PREHASH_ParentID, LLUUID::null );

	msg->addU64Fast(_PREHASH_Handle, gAgent.getRegion()->getHandle());

	LLVector3 position = gAgent.getPositionAgent();
	msg->addVector3Fast(_PREHASH_Position, position);
	msg->addF32Fast(_PREHASH_Gain, gain);

	gAgent.sendMessage();
}

static LLSD sSavedGroupInvite;
static LLSD sSavedResponse;

bool join_group_response(const LLSD& notification, const LLSD& response)
{
//	A bit of variable saving and restoring is used to deal with the case where your group list is full and you
//	receive an invitation to another group.  The data from that invitation is stored in the sSaved
//	variables.  If you then drop a group and click on the Join button the stored data is restored and used
//	to join the group.
	LLSD notification_adjusted = notification;
	LLSD response_adjusted = response;

	std::string action = notification["name"];

//	Storing all the information by group id allows for the rare case of being at your maximum
//	group count and receiving more than one invitation.
	std::string id = notification_adjusted["payload"]["group_id"].asString();

	if ("JoinGroup" == action || "JoinGroupCanAfford" == action)
	{
		sSavedGroupInvite[id] = notification;
		sSavedResponse[id] = response;
	}
	else if ("JoinedTooManyGroupsMember" == action)
	{
		S32 opt = LLNotificationsUtil::getSelectedOption(notification, response);
		if (0 == opt) // Join button pressed
		{
			notification_adjusted = sSavedGroupInvite[id];
			response_adjusted = sSavedResponse[id];
		}
	}

	S32 option = LLNotificationsUtil::getSelectedOption(notification_adjusted, response_adjusted);
	bool accept_invite = false;

	LLUUID group_id = notification_adjusted["payload"]["group_id"].asUUID();
	LLUUID transaction_id = notification_adjusted["payload"]["transaction_id"].asUUID();
	std::string name = notification_adjusted["payload"]["name"].asString();
	std::string message = notification_adjusted["payload"]["message"].asString();
	S32 fee = notification_adjusted["payload"]["fee"].asInteger();

	if (option == 2 && !group_id.isNull())
	{
		LLGroupActions::show(group_id);
		LLSD args;
		args["MESSAGE"] = message;

		// <FS:PP> Option to block/reject all group invites
		// LLNotificationsUtil::add("JoinGroup", args, notification_adjusted["payload"]);
		if (gSavedPerAccountSettings.getBOOL("FSRejectAllGroupInvitesMode"))
		{
			LL_INFOS("Messaging") << "Group invite automatically rejected because of the user setting..." << LL_ENDL;
		}
		else
		{
			make_ui_sound("UISndGroupInvitation"); // <FS:PP> Group invitation sound
			LLNotificationsUtil::add("JoinGroup", args, notification_adjusted["payload"]);
		}
		// </FS:PP>

		return false;
	}

	if(option == 0 && !group_id.isNull())
	{
		// check for promotion or demotion.
		S32 max_groups = gMaxAgentGroups;
		if(gAgent.isInGroup(group_id)) ++max_groups;

		// [CR] FIRE-12229
		//if(gAgent.mGroups.size() < max_groups)
		if(!max_groups || gAgent.mGroups.size() < max_groups)
		// [/CR] FIRE-12229
		{
			accept_invite = true;
		}
		else
		{
			LLSD args;
			args["NAME"] = name;
			LLNotificationsUtil::add("JoinedTooManyGroupsMember", args, notification_adjusted["payload"]);
			return false;
		}
	}

	if (accept_invite)
	{
		// If there is a fee to join this group, make
		// sure the user is sure they want to join.
		if (fee > 0)
		{
			LLSD args;
			args["COST"] = llformat("%d", fee);
			// Set the fee for next time to 0, so that we don't keep
			// asking about a fee.
			LLSD next_payload = notification_adjusted["payload"];
			next_payload["fee"] = 0;
			LLNotificationsUtil::add("JoinGroupCanAfford",
									args,
									next_payload);
		}
		else
		{
			send_improved_im(group_id,
							 std::string("name"),
							 std::string("message"),
							IM_ONLINE,
							IM_GROUP_INVITATION_ACCEPT,
							transaction_id);
		}
	}
	else
	{
		send_improved_im(group_id,
						 std::string("name"),
						 std::string("message"),
						IM_ONLINE,
						IM_GROUP_INVITATION_DECLINE,
						transaction_id);
	}

	sSavedGroupInvite[id] = LLSD::emptyMap();
	sSavedResponse[id] = LLSD::emptyMap();

	return false;
}

static void highlight_inventory_objects_in_panel(const std::vector<LLUUID>& items, LLInventoryPanel *inventory_panel)
{
	if (NULL == inventory_panel) return;

	for (std::vector<LLUUID>::const_iterator item_iter = items.begin();
		item_iter != items.end();
		++item_iter)
	{
		const LLUUID& item_id = (*item_iter);
		if(!highlight_offered_object(item_id))
		{
			continue;
		}

		LLInventoryObject* item = gInventory.getObject(item_id);
		llassert(item);
		if (!item) {
			continue;
		}

		LL_DEBUGS("Inventory_Move") << "Highlighting inventory item: " << item->getName() << ", " << item_id  << LL_ENDL;
		LLFolderView* fv = inventory_panel->getRootFolder();
		if (fv)
		{
			LLFolderViewItem* fv_item = inventory_panel->getItemByID(item_id);
			if (fv_item)
			{
				LLFolderViewItem* fv_folder = fv_item->getParentFolder();
				if (fv_folder)
				{
					// Parent folders can be different in case of 2 consecutive drag and drop
					// operations when the second one is started before the first one completes.
					LL_DEBUGS("Inventory_Move") << "Open folder: " << fv_folder->getName() << LL_ENDL;
					fv_folder->setOpen(TRUE);
					if (fv_folder->isSelected())
					{
						fv->changeSelection(fv_folder, FALSE);
					}
				}
				fv->changeSelection(fv_item, TRUE);
			}
		}
	}
}

static LLNotificationFunctorRegistration jgr_1("JoinGroup", join_group_response);
static LLNotificationFunctorRegistration jgr_2("JoinedTooManyGroupsMember", join_group_response);
static LLNotificationFunctorRegistration jgr_3("JoinGroupCanAfford", join_group_response);

//-----------------------------------------------------------------------------
// Instant Message
//-----------------------------------------------------------------------------
class LLOpenAgentOffer : public LLInventoryFetchItemsObserver
{
public:
	LLOpenAgentOffer(const LLUUID& object_id,
					 const std::string& from_name) : 
		LLInventoryFetchItemsObserver(object_id),
		mFromName(from_name) {}
	/*virtual*/ void startFetch()
	{
		for (uuid_vec_t::const_iterator it = mIDs.begin(); it < mIDs.end(); ++it)
		{
			LLViewerInventoryCategory* cat = gInventory.getCategory(*it);
			if (cat)
			{
				mComplete.push_back((*it));
			}
		}
		LLInventoryFetchItemsObserver::startFetch();
	}
	/*virtual*/ void done()
	{
		// <FS:Ansariel> FIRE-3234: Don't need a check for ShowNewInventory here;
		// This only gets called if the user explicity clicks "Show" or
		// AutoAcceptNewInventory and ShowNewInventory are TRUE.
		//open_inventory_offer(mComplete, mFromName);
		open_inventory_offer(mComplete, mFromName, true);
		gInventory.removeObserver(this);
		delete this;
	}
private:
	std::string mFromName;
};

/**
 * Class to observe adding of new items moved from the world to user's inventory to select them in inventory.
 *
 * We can't create it each time items are moved because "drop" event is sent separately for each
 * element even while multi-dragging. We have to have the only instance of the observer. See EXT-4347.
 */
class LLViewerInventoryMoveFromWorldObserver : public LLInventoryAddItemByAssetObserver
{
public:
	LLViewerInventoryMoveFromWorldObserver()
		: LLInventoryAddItemByAssetObserver()
	{

	}

	void setMoveIntoFolderID(const LLUUID& into_folder_uuid) {mMoveIntoFolderID = into_folder_uuid; }

private:
	/*virtual */void onAssetAdded(const LLUUID& asset_id)
	{
		// Store active Inventory panel.
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			mActivePanel = LLInventoryPanel::getActiveInventoryPanel()->getHandle();
		}

		// Store selected items (without destination folder)
		mSelectedItems.clear();
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			std::set<LLFolderViewItem*> selection =    LLInventoryPanel::getActiveInventoryPanel()->getRootFolder()->getSelectionList();
			for (std::set<LLFolderViewItem*>::iterator it = selection.begin(),    end_it = selection.end();
				it != end_it;
				++it)
			{
				mSelectedItems.insert(static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem())->getUUID());
			}
		}
		mSelectedItems.erase(mMoveIntoFolderID);
	}

	/**
	 * Selects added inventory items watched by their Asset UUIDs if selection was not changed since
	 * all items were started to watch (dropped into a folder).
	 */
	void done()
	{
		LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

		// if selection is not changed since watch started lets hightlight new items.
		if (active_panel && !isSelectionChanged())
		{
			LL_DEBUGS("Inventory_Move") << "Selecting new items..." << LL_ENDL;
			active_panel->clearSelection();
			highlight_inventory_objects_in_panel(mAddedItems, active_panel);
		}
	}

	/**
	 * Returns true if selected inventory items were changed since moved inventory items were started to watch.
	 */
	bool isSelectionChanged()
	{	
		LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

		if (NULL == active_panel)
		{
			return true;
		}

		// get selected items (without destination folder)
		selected_items_t selected_items;
 		
 		std::set<LLFolderViewItem*> selection =    LLInventoryPanel::getActiveInventoryPanel()->getRootFolder()->getSelectionList();
		for (std::set<LLFolderViewItem*>::iterator it = selection.begin(),    end_it = selection.end();
			it != end_it;
			++it)
		{
			selected_items.insert(static_cast<LLFolderViewModelItemInventory*>((*it)->getViewModelItem())->getUUID());
		}
		selected_items.erase(mMoveIntoFolderID);

		// compare stored & current sets of selected items
		selected_items_t different_items;
		std::set_symmetric_difference(mSelectedItems.begin(), mSelectedItems.end(),
			selected_items.begin(), selected_items.end(), std::inserter(different_items, different_items.begin()));

		LL_DEBUGS("Inventory_Move") << "Selected firstly: " << mSelectedItems.size()
			<< ", now: " << selected_items.size() << ", difference: " << different_items.size() << LL_ENDL;

		return different_items.size() > 0;
	}

	LLHandle<LLPanel> mActivePanel;
	typedef std::set<LLUUID> selected_items_t;
	selected_items_t mSelectedItems;

	/**
	 * UUID of FolderViewFolder into which watched items are moved.
	 *
	 * Destination FolderViewFolder becomes selected while mouse hovering (when dragged items are dropped).
	 * 
	 * If mouse is moved out it set unselected and number of selected items is changed 
	 * even if selected items in Inventory stay the same.
	 * So, it is used to update stored selection list.
	 *
	 * @see onAssetAdded()
	 * @see isSelectionChanged()
	 */
	LLUUID mMoveIntoFolderID;
};

LLViewerInventoryMoveFromWorldObserver* gInventoryMoveObserver = NULL;

void set_dad_inventory_item(LLInventoryItem* inv_item, const LLUUID& into_folder_uuid)
{
	start_new_inventory_observer();

	gInventoryMoveObserver->setMoveIntoFolderID(into_folder_uuid);
	gInventoryMoveObserver->watchAsset(inv_item->getAssetUUID());
}


/**
 * Class to observe moving of items and to select them in inventory.
 *
 * Used currently for dragging from inbox to regular inventory folders
 */

class LLViewerInventoryMoveObserver : public LLInventoryObserver
{
public:

	LLViewerInventoryMoveObserver(const LLUUID& object_id)
		: LLInventoryObserver()
		, mObjectID(object_id)
	{
		if (LLInventoryPanel::getActiveInventoryPanel())
		{
			mActivePanel = LLInventoryPanel::getActiveInventoryPanel()->getHandle();
		}
	}

	virtual ~LLViewerInventoryMoveObserver() {}
	virtual void changed(U32 mask);
	
private:
	LLUUID mObjectID;
	LLHandle<LLPanel> mActivePanel;

};

void LLViewerInventoryMoveObserver::changed(U32 mask)
{
	LLInventoryPanel* active_panel = dynamic_cast<LLInventoryPanel*>(mActivePanel.get());

	if (NULL == active_panel)
	{
		gInventory.removeObserver(this);
		return;
	}

	if((mask & (LLInventoryObserver::STRUCTURE)) != 0)
	{
		const std::set<LLUUID>& changed_items = gInventory.getChangedIDs();

		std::set<LLUUID>::const_iterator id_it = changed_items.begin();
		std::set<LLUUID>::const_iterator id_end = changed_items.end();
		for (;id_it != id_end; ++id_it)
		{
			if ((*id_it) == mObjectID)
			{
				active_panel->clearSelection();			
				std::vector<LLUUID> items;
				items.push_back(mObjectID);
				highlight_inventory_objects_in_panel(items, active_panel);
				active_panel->getRootFolder()->scrollToShowSelection();
				
				gInventory.removeObserver(this);
				break;
			}
		}
	}
}

void set_dad_inbox_object(const LLUUID& object_id)
{
	LLViewerInventoryMoveObserver* move_observer = new LLViewerInventoryMoveObserver(object_id);
	gInventory.addObserver(move_observer);
}

//unlike the FetchObserver for AgentOffer, we only make one 
//instance of the AddedObserver for TaskOffers
//and it never dies.  We do this because we don't know the UUID of 
//task offers until they are accepted, so we don't wouldn't 
//know what to watch for, so instead we just watch for all additions.
class LLOpenTaskOffer : public LLInventoryAddedObserver
{
protected:
	/*virtual*/ void done()
	{
		uuid_vec_t added;
		for(uuid_set_t::const_iterator it = gInventory.getAddedIDs().begin(); it != gInventory.getAddedIDs().end(); ++it)
		{
			added.push_back(*it);
		}
		for (uuid_vec_t::iterator it = added.begin(); it != added.end();)
		{
			const LLUUID& item_uuid = *it;
			bool was_moved = false;
			LLInventoryObject* added_object = gInventory.getObject(item_uuid);
			if (added_object)
			{
				// cast to item to get Asset UUID
				LLInventoryItem* added_item = dynamic_cast<LLInventoryItem*>(added_object);
				if (added_item)
				{
					const LLUUID& asset_uuid = added_item->getAssetUUID();
					if (gInventoryMoveObserver->isAssetWatched(asset_uuid))
					{
						LL_DEBUGS("Inventory_Move") << "Found asset UUID: " << asset_uuid << LL_ENDL;
						was_moved = true;
					}
					// <FS:Ansariel> We might end up here if LLFriendCardsManager tries to sync the friend cards at login
					//               and that might pop up the inventory window for extra annoyance -> silence this!
					else if (added_item->getActualType() == LLAssetType::AT_CALLINGCARD)
					{
						if (LLFriendCardsManager::instance().isAvatarDataStored(added_item->getCreatorUUID()))
						{
							LL_DEBUGS("FriendCard") << "Skipping added calling card from friend cards sync: " << added_item->getCreatorUUID().asString() << LL_ENDL;
							was_moved = true;
						}
					}
					// </FS:Ansariel>
				}
			}

			if (was_moved)
			{
				it = added.erase(it);
			}
			else ++it;
		}

		// <FS:Ansariel> Moved check out of check_offer_throttle
		//open_inventory_offer(added, "");
		if (gSavedSettings.getBOOL("ShowNewInventory"))
		{
			open_inventory_offer(added, "");
		}
		else if (!added.empty() && gSavedSettings.getBOOL("ShowInInventory") && highlight_offered_object(added.back()))
		{
			LLInventoryPanel::openInventoryPanelAndSetSelection(TRUE, added.back());
		}
		// </FS:Ansariel>
	}
 };

class LLOpenTaskGroupOffer : public LLInventoryAddedObserver
{
protected:
	/*virtual*/ void done()
	{
		uuid_vec_t added;
		for(uuid_set_t::const_iterator it = gInventory.getAddedIDs().begin(); it != gInventory.getAddedIDs().end(); ++it)
		{
			added.push_back(*it);
		}

		// <FS:Ansariel> Moved check out of check_offer_throttle
		//open_inventory_offer(added, "group_offer");
		if (gSavedSettings.getBOOL("ShowNewInventory"))
		{
			open_inventory_offer(added, "group_offer");
		}
		else if (!added.empty() && gSavedSettings.getBOOL("ShowInInventory"))
		{
			LLInventoryPanel::openInventoryPanelAndSetSelection(TRUE, added.back());
		}
		// </FS:Ansariel>
		gInventory.removeObserver(this);
		delete this;
	}
};

//one global instance to bind them
LLOpenTaskOffer* gNewInventoryObserver=NULL;
class LLNewInventoryHintObserver : public LLInventoryAddedObserver
{
protected:
	/*virtual*/ void done()
	{
		LLFirstUse::newInventory();
	}
};

LLNewInventoryHintObserver* gNewInventoryHintObserver=NULL;

void start_new_inventory_observer()
{
	if (!gNewInventoryObserver) //task offer observer 
	{
		// Observer is deleted by gInventory
		gNewInventoryObserver = new LLOpenTaskOffer;
		gInventory.addObserver(gNewInventoryObserver);
	}

	if (!gInventoryMoveObserver) //inventory move from the world observer 
	{
		// Observer is deleted by gInventory
		gInventoryMoveObserver = new LLViewerInventoryMoveFromWorldObserver;
		gInventory.addObserver(gInventoryMoveObserver);
	}

	if (!gNewInventoryHintObserver)
	{
		// Observer is deleted by gInventory
		gNewInventoryHintObserver = new LLNewInventoryHintObserver();
		gInventory.addObserver(gNewInventoryHintObserver);
	}
}

class LLDiscardAgentOffer : public LLInventoryFetchItemsObserver
{
	LOG_CLASS(LLDiscardAgentOffer);

public:
	LLDiscardAgentOffer(const LLUUID& folder_id, const LLUUID& object_id) :
		LLInventoryFetchItemsObserver(object_id),
		mFolderID(folder_id),
		mObjectID(object_id) {}

	virtual void done()
	{
		LL_DEBUGS("Messaging") << "LLDiscardAgentOffer::done()" << LL_ENDL;

		// We're invoked from LLInventoryModel::notifyObservers().
		// If we now try to remove the inventory item, it will cause a nested
		// notifyObservers() call, which won't work.
		// So defer moving the item to trash until viewer gets idle (in a moment).
		// Use removeObject() rather than removeItem() because at this level,
		// the object could be either an item or a folder.
		LLAppViewer::instance()->addOnIdleCallback(boost::bind(&LLInventoryModel::removeObject, &gInventory, mObjectID));
		gInventory.removeObserver(this);
		delete this;
	}

protected:
	LLUUID mFolderID;
	LLUUID mObjectID;
};


//Returns TRUE if we are OK, FALSE if we are throttled
//Set check_only true if you want to know the throttle status 
//without registering a hit
bool check_offer_throttle(const std::string& from_name, bool check_only)
{
	static U32 throttle_count;
	static bool throttle_logged;
	LLChat chat;
	std::string log_message;

	// <FS:Ansariel> This controls if items should be opened in open_inventory_offer()??? No way!
	//if (!gSavedSettings.getBOOL("ShowNewInventory"))
	//	return false;
	// </FS:Ansariel>

	if (check_only)
	{
		return gThrottleTimer.hasExpired();
	}
	
	if(gThrottleTimer.checkExpirationAndReset(OFFER_THROTTLE_TIME))
	{
		LL_DEBUGS("Messaging") << "Throttle Expired" << LL_ENDL;
		throttle_count=1;
		throttle_logged=false;
		return true;
	}
	else //has not expired
	{
		LL_DEBUGS("Messaging") << "Throttle Not Expired, Count: " << throttle_count << LL_ENDL;
		// When downloading the initial inventory we get a lot of new items
		// coming in and can't tell that from spam.

		// Ansariel: Customizable throttle!
		static LLCachedControl<U32> fsOfferThrottleMaxCount(gSavedSettings, "FSOfferThrottleMaxCount");
		if (LLStartUp::getStartupState() >= STATE_STARTED
			//&& throttle_count >= OFFER_THROTTLE_MAX_COUNT)
			&& throttle_count >= fsOfferThrottleMaxCount)
		{
			if (!throttle_logged)
			{
				// Use the name of the last item giver, who is probably the person
				// spamming you.

				LLStringUtil::format_map_t arg;
				std::string log_msg;
				std::ostringstream time ;
				time<<OFFER_THROTTLE_TIME;

				arg["APP_NAME"] = LLAppViewer::instance()->getSecondLifeTitle();
				arg["TIME"] = time.str();


				if (!from_name.empty())
				{
					// <FS:PP> gSavedSettings to LLCachedControl
					static LLCachedControl<bool> fsNotifyIncomingObjectSpamFrom(gSavedSettings, "FSNotifyIncomingObjectSpamFrom");
					if (fsNotifyIncomingObjectSpamFrom)
					// </FS:PP>
					{
						arg["FROM_NAME"] = from_name;
						log_msg = LLTrans::getString("ItemsComingInTooFastFrom", arg);
					}
				}
				else
				{
					// <FS:PP> gSavedSettings to LLCachedControl
					static LLCachedControl<bool> fsNotifyIncomingObjectSpam(gSavedSettings, "FSNotifyIncomingObjectSpam");
					if (fsNotifyIncomingObjectSpam)
					// </FS:PP>
					{
						log_msg = LLTrans::getString("ItemsComingInTooFast", arg);
					}
				}
				
				//this is kinda important, so actually put it on screen
				if (log_msg != "")
				{
					LLSD args;
					args["MESSAGE"] = log_msg;
					LLNotificationsUtil::add("SystemMessage", args);
				}

				throttle_logged=true;
			}
			return false;
		}
		else
		{
			throttle_count++;
			return true;
		}
	}
}
 
// Return "true" if we have a preview method for that asset type, "false" otherwise
bool check_asset_previewable(const LLAssetType::EType asset_type)
{
	return	(asset_type == LLAssetType::AT_NOTECARD)  || 
			(asset_type == LLAssetType::AT_LANDMARK)  ||
			(asset_type == LLAssetType::AT_TEXTURE)   ||
			(asset_type == LLAssetType::AT_ANIMATION) ||
			(asset_type == LLAssetType::AT_SCRIPT)    ||
			(asset_type == LLAssetType::AT_SOUND);
}

// <FS:Ansariel> FIRE-15886
//void open_inventory_offer(const uuid_vec_t& objects, const std::string& from_name)
void open_inventory_offer(const uuid_vec_t& objects, const std::string& from_name, bool from_agent /* = false*/)
{
	for (uuid_vec_t::const_iterator obj_iter = objects.begin();
		 obj_iter != objects.end();
		 ++obj_iter)
	{
		const LLUUID& obj_id = (*obj_iter);
		if(!highlight_offered_object(obj_id))
		{
			continue;
		}

		const LLInventoryObject *obj = gInventory.getObject(obj_id);
		if (!obj)
		{
			LL_WARNS() << "Cannot find object [ itemID:" << obj_id << " ] to open." << LL_ENDL;
			continue;
		}

		const LLAssetType::EType asset_type = obj->getActualType();

		// Either an inventory item or a category.
		const LLInventoryItem* item = dynamic_cast<const LLInventoryItem*>(obj);
		if (item && check_asset_previewable(asset_type))
		{
			////////////////////////////////////////////////////////////////////////////////
			// Special handling for various types.
			if (check_offer_throttle(from_name, false)) // If we are throttled, don't display
			{
				LL_DEBUGS("Messaging") << "Highlighting inventory item: " << item->getUUID()  << LL_ENDL;
				// If we opened this ourselves, focus it
				const BOOL take_focus = from_name.empty() ? TAKE_FOCUS_YES : TAKE_FOCUS_NO;
				switch(asset_type)
				{
					case LLAssetType::AT_NOTECARD:
					{
						LLFloaterReg::showInstance("preview_notecard", LLSD(obj_id), take_focus);
						break;
					}
					case LLAssetType::AT_LANDMARK:
					{
						LLInventoryCategory* parent_folder = gInventory.getCategory(item->getParentUUID());
						if ("inventory_handler" == from_name)
						{
							// <FS:Ansariel> FIRE-817: Separate place details floater
							//LLFloaterSidePanelContainer::showPanel("places", LLSD().with("type", "landmark").with("id", item->getUUID()));
							FSFloaterPlaceDetails::showPlaceDetails(LLSD().with("type", "landmark").with("id", item->getUUID()));
							// </FS:Ansariel>
						}
						else if("group_offer" == from_name)
						{
							// "group_offer" is passed by LLOpenTaskGroupOffer
							// Notification about added landmark will be generated under the "from_name.empty()" called from LLOpenTaskOffer::done().
							LLSD args;
							args["type"] = "landmark";
							args["id"] = obj_id;

							// <FS:Ansariel> FIRE-817: Separate place details floater
							//LLFloaterSidePanelContainer::showPanel("places", args);
							FSFloaterPlaceDetails::showPlaceDetails(args);
							// </FS:Ansariel>

							continue;
						}
						else if(from_name.empty() && gSavedSettings.getBOOL("FSLandmarkCreatedNotification")) // Ansariel: Make notification optional
						{
							std::string folder_name;
							if (parent_folder)
							{
								// Localize folder name.
								// *TODO: share this code?
								folder_name = parent_folder->getName();
								if (LLFolderType::lookupIsProtectedType(parent_folder->getPreferredType()))
								{
									LLTrans::findString(folder_name, "InvFolder " + folder_name);
								}
							}
							else
							{
								 folder_name = LLTrans::getString("Unknown");
							}

							// we receive a message from LLOpenTaskOffer, it mean that new landmark has been added.
							LLSD args;
							args["LANDMARK_NAME"] = item->getName();
							args["FOLDER_NAME"] = folder_name;
							LLNotificationsUtil::add("LandmarkCreated", args);
						}
					}
					break;
					case LLAssetType::AT_TEXTURE:
					{
						LLFloaterReg::showInstance("preview_texture", LLSD(obj_id), take_focus);
						break;
					}
					case LLAssetType::AT_ANIMATION:
						LLFloaterReg::showInstance("preview_anim", LLSD(obj_id), take_focus);
						break;
					case LLAssetType::AT_SCRIPT:
						LLFloaterReg::showInstance("preview_script", LLSD(obj_id), take_focus);
						break;
					case LLAssetType::AT_SOUND:
						LLFloaterReg::showInstance("preview_sound", LLSD(obj_id), take_focus);
						break;
					default:
						LL_DEBUGS("Messaging") << "No preview method for previewable asset type : " << LLAssetType::lookupHumanReadable(asset_type)  << LL_ENDL;
						break;
				}
			}
		}

		////////////////////////////////////////////////////////////////////////////////
		// Highlight item
		// <FS:Ansariel> Only show if either ShowInInventory is true OR it is an inventory
		//               offer from an agent and the asset is not previewable
		const BOOL auto_open = gSavedSettings.getBOOL("ShowInInventory") || (from_agent && !check_asset_previewable(asset_type));
			//gSavedSettings.getBOOL("ShowInInventory") && // don't open if showininventory is false
			//!from_name.empty(); // don't open if it's not from anyone.
		// <FS:Ansariel> Don't mess with open inventory panels when ShowInInventory is FALSE
		if (auto_open)
		LLInventoryPanel::openInventoryPanelAndSetSelection(auto_open, obj_id);
	}
}

bool highlight_offered_object(const LLUUID& obj_id)
{
	const LLInventoryObject* obj = gInventory.getObject(obj_id);
	if(!obj)
	{
		LL_WARNS("Messaging") << "Unable to show inventory item: " << obj_id << LL_ENDL;
		return false;
	}

	////////////////////////////////////////////////////////////////////////////////
	// Don't highlight if it's in certain "quiet" folders which don't need UI
	// notification (e.g. trash, cof, lost-and-found).
	if(!gAgent.getAFK())
	{
		const LLViewerInventoryCategory *parent = gInventory.getFirstNondefaultParent(obj_id);
		if (parent)
		{
			const LLFolderType::EType parent_type = parent->getPreferredType();
			if (LLViewerFolderType::lookupIsQuietType(parent_type))
			{
				return false;
			}
		}
	}

	return true;
}

void inventory_offer_mute_callback(const LLUUID& blocked_id,
								   const std::string& full_name,
								   bool is_group)
{
	// *NOTE: blocks owner if the offer came from an object
	LLMute::EType mute_type = is_group ? LLMute::GROUP : LLMute::AGENT;

	LLMute mute(blocked_id, full_name, mute_type);
	if (LLMuteList::getInstance()->add(mute))
	{
		LLPanelBlockedList::showPanelAndSelect(blocked_id);
	}

	// purge the message queue of any previously queued inventory offers from the same source.
	class OfferMatcher : public LLNotificationsUI::LLScreenChannel::Matcher
	{
	public:
		OfferMatcher(const LLUUID& to_block) : blocked_id(to_block) {}
		bool matches(const LLNotificationPtr notification) const
		{
			if(notification->getName() == "ObjectGiveItem" 
				|| notification->getName() == "OwnObjectGiveItem"
				|| notification->getName() == "UserGiveItemLegacy" // <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
				|| notification->getName() == "UserGiveItem")
			{
				return (notification->getPayload()["from_id"].asUUID() == blocked_id);
			}
			return FALSE;
		}
	private:
		const LLUUID& blocked_id;
	};

	LLNotificationsUI::LLChannelManager::getInstance()->killToastsFromChannel(LLUUID(
			gSavedSettings.getString("NotificationChannelUUID")), OfferMatcher(blocked_id));
}

std::string LLOfferInfo::mResponderType = "offer_info";

LLOfferInfo::LLOfferInfo()
 : LLNotificationResponderInterface()
 , mFromGroup(FALSE)
 , mFromObject(FALSE)
 , mIM(IM_NOTHING_SPECIAL)
 , mType(LLAssetType::AT_NONE)
 , mPersist(false)
{
}

LLOfferInfo::LLOfferInfo(const LLSD& sd)
{
	mIM = (EInstantMessage)sd["im_type"].asInteger();
	mFromID = sd["from_id"].asUUID();
	mFromGroup = sd["from_group"].asBoolean();
	mFromObject = sd["from_object"].asBoolean();
	mTransactionID = sd["transaction_id"].asUUID();
	mFolderID = sd["folder_id"].asUUID();
	mObjectID = sd["object_id"].asUUID();
	mType = LLAssetType::lookup(sd["type"].asString().c_str());
	mFromName = sd["from_name"].asString();
	mDesc = sd["description"].asString();
	mHost = LLHost(sd["sender"].asString());
	mPersist = sd["persist"].asBoolean();
}

LLOfferInfo::LLOfferInfo(const LLOfferInfo& info)
{
	mIM = info.mIM;
	mFromID = info.mFromID;
	mFromGroup = info.mFromGroup;
	mFromObject = info.mFromObject;
	mTransactionID = info.mTransactionID;
	mFolderID = info.mFolderID;
	mObjectID = info.mObjectID;
	mType = info.mType;
	mFromName = info.mFromName;
	mDesc = info.mDesc;
	mHost = info.mHost;
	mPersist = info.mPersist;
}

LLSD LLOfferInfo::asLLSD()
{
	LLSD sd;
    sd["responder_type"] = mResponderType;
	sd["im_type"] = mIM;
	sd["from_id"] = mFromID;
	sd["from_group"] = mFromGroup;
	sd["from_object"] = mFromObject;
	sd["transaction_id"] = mTransactionID;
	sd["folder_id"] = mFolderID;
	sd["object_id"] = mObjectID;
	sd["type"] = LLAssetType::lookup(mType);
	sd["from_name"] = mFromName;
	sd["description"] = mDesc;
	sd["sender"] = mHost.getIPandPort();
	sd["persist"] = mPersist;
	return sd;
}

void LLOfferInfo::fromLLSD(const LLSD& params)
{
	*this = params;
}

void LLOfferInfo::send_auto_receive_response(void)
{	
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary
	std::string name;
	LLAgentUI::buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, ""); 
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
	
	// Auto Receive Message. The math for the dialog works, because the accept
	// for inventory_offered, task_inventory_offer or
	// group_notice_inventory is 1 greater than the offer integer value.
	// Generates IM_INVENTORY_ACCEPTED, IM_TASK_INVENTORY_ACCEPTED, 
	// or IM_GROUP_NOTICE_INVENTORY_ACCEPTED
	msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 1));
	msg->addBinaryDataFast(_PREHASH_BinaryBucket, &(mFolderID.mData),
						   sizeof(mFolderID.mData));
	// send the message
	msg->sendReliable(mHost);
	
	if(IM_INVENTORY_OFFERED == mIM)
	{
		// add buddy to recent people list
//		LLRecentPeople::instance().add(mFromID);
// [RLVa:KB] - Checked: RLVa-2.0.1
		// RELEASE-RLVa: [RLVa-2.0.1] Make sure this stays in sync with the condition in inventory_offer_handler()
		bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
			(RlvActions::canShowName(RlvActions::SNC_DEFAULT, mFromID)) || (!RlvUtil::isNearbyAgent(mFromID)) || (RlvUIEnabler::hasOpenIM(mFromID)) || (RlvUIEnabler::hasOpenProfile(mFromID));
		if (fRlvCanShowName)
			LLRecentPeople::instance().add(mFromID);
// [/RLVa:KB]
	}
}

// <FS:Ansariel> Optional V1-like inventory accept messages
void LLOfferInfo::send_decline_response(void)
{	
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary
	std::string name;
	LLAgentUI::buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, ""); 
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
	msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 2));
	msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET, EMPTY_BINARY_BUCKET_SIZE);
	msg->sendReliable(mHost);
}
// </FS:Ansariel> Optional V1-like inventory accept messages

void LLOfferInfo::handleRespond(const LLSD& notification, const LLSD& response)
{
	initRespondFunctionMap();

	const std::string name = notification["name"].asString();
	if(mRespondFunctions.find(name) == mRespondFunctions.end())
	{
		LL_WARNS() << "Unexpected notification name : " << name << LL_ENDL;
		llassert(!"Unexpected notification name");
		return;
	}

	mRespondFunctions[name](notification, response);
}

bool LLOfferInfo::inventory_offer_callback(const LLSD& notification, const LLSD& response)
{
	LLChat chat;
	std::string log_message;
	S32 button = LLNotificationsUtil::getSelectedOption(notification, response);

	LLInventoryObserver* opener = NULL;
	LLViewerInventoryCategory* catp = NULL;
	catp = (LLViewerInventoryCategory*)gInventory.getCategory(mObjectID);
	LLViewerInventoryItem* itemp = NULL;
	if(!catp)
	{
		itemp = (LLViewerInventoryItem*)gInventory.getItem(mObjectID);
	}
	 
	LLNotificationPtr notification_ptr = LLNotifications::instance().find(notification["id"].asUUID());
	
	// For muting, we need to add the mute, then decline the offer.
	// This must be done here because:
	// * callback may be called immediately,
	// * adding the mute sends a message,
	// * we can't build two messages at once.
	if (IOR_MUTE == button) // Block
	{
		if (notification_ptr != NULL)
		{
			gCacheName->get(mFromID, mFromGroup, boost::bind(&inventory_offer_mute_callback, _1, _2, _3));
		}
	}

	std::string from_string; // Used in the pop-up.
	std::string chatHistory_string;  // Used in chat history.
	
	// TODO: when task inventory offers can also be handled the new way, migrate the code that sets these strings here:
	from_string = chatHistory_string = mFromName;
	
	LLNotificationFormPtr modified_form(notification_ptr ? new LLNotificationForm(*notification_ptr->getForm()) : new LLNotificationForm());

	switch(button)
	{
	case IOR_SHOW:
	case IOR_SHOW_SILENT: // <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		// we will want to open this item when it comes back.
		LL_DEBUGS("Messaging") << "Initializing an opener for tid: " << mTransactionID
				 << LL_ENDL;
		switch (mIM)
		{
		case IM_INVENTORY_OFFERED:
			{
				// This is an offer from an agent. In this case, the back
				// end has already copied the items into your inventory,
				// so we can fetch it out of our inventory.
// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.2.0)
				if ( (rlv_handler_t::isEnabled()) && (!RlvSettings::getForbidGiveToRLV()) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 0) )
				{
					RlvGiveToRLVAgentOffer* pOfferObserver = new RlvGiveToRLVAgentOffer(mObjectID);
					pOfferObserver->startFetch();
					if (pOfferObserver->isFinished())
						pOfferObserver->done();
					else
						gInventory.addObserver(pOfferObserver);
				}
// [/RLVa:KB]

				// <FS:Ansariel> FIRE-3234: Ask if items should be previewed;
				// ShowOfferedInventory is always true anyway - instead there is
				// ShowNewInventory that is actually changable by the user!
				//if (gSavedSettings.getBOOL("ShowOfferedInventory"))
				{
					LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(mObjectID, from_string);
					open_agent_offer->startFetch();
					if(catp || (itemp && itemp->isFinished()))
					{
						open_agent_offer->done();
					}
					else
					{
						opener = open_agent_offer;
					}
				}

				// <FS:Ansariel> Optional V1-like inventory accept messages
				if (gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages") && button == IOR_SHOW)
				{
					send_auto_receive_response();
				}
				// </FS:Ansariel> Optional V1-like inventory accept messages
			}
			break;
		case IM_GROUP_NOTICE:
			opener = new LLOpenTaskGroupOffer;
			send_auto_receive_response();
			break;
		case IM_TASK_INVENTORY_OFFERED:
		case IM_GROUP_NOTICE_REQUESTED:
			// This is an offer from a task or group.
			// We don't use a new instance of an opener
			// We instead use the singular observer gOpenTaskOffer
			// Since it already exists, we don't need to actually do anything
			break;
		default:
			LL_WARNS("Messaging") << "inventory_offer_callback: unknown offer type" << LL_ENDL;
			break;
		}

		if (modified_form != NULL)
		{
			modified_form->setElementEnabled("Show", false);
		}
		break;
		// end switch (mIM)
			
	case IOR_ACCEPT:
	case IOR_ACCEPT_SILENT: // <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		//don't spam them if they are getting flooded
		if (check_offer_throttle(mFromName, true))
		{
			log_message = chatHistory_string + " " + LLTrans::getString("InvOfferGaveYou") + " " + mDesc + LLTrans::getString(".");
			LLSD args;
			args["MESSAGE"] = log_message;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}

		// <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		if (mIM == IM_GROUP_NOTICE)
		{
			opener = new LLOpenTaskGroupOffer;
			send_auto_receive_response();
		}
		else if (mIM == IM_INVENTORY_OFFERED)
		{
// [RLVa:KB] - Checked: 2010-04-18 (RLVa-1.2.0)
			if ( (rlv_handler_t::isEnabled()) && (!RlvSettings::getForbidGiveToRLV()) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 0) )
			{
				RlvGiveToRLVAgentOffer* pOfferObserver = new RlvGiveToRLVAgentOffer(mObjectID);
				pOfferObserver->startFetch();
				if (pOfferObserver->isFinished())
					pOfferObserver->done();
				else
					gInventory.addObserver(pOfferObserver);
			}
// [/RLVa:KB]

			if (gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages") && button == IOR_ACCEPT)
			{
				send_auto_receive_response();
			}
			if (gSavedSettings.getBOOL("ShowInInventory"))
			{
				LLInventoryPanel::openInventoryPanelAndSetSelection(TRUE, mObjectID);
			}
		}
		// </FS:Ansariel>

		break;

	case IOR_MUTE:
		if (modified_form != NULL)
		{
			modified_form->setElementEnabled("Mute", false);
		}
		// MUTE falls through to decline
	case IOR_DECLINE:
	case IOR_DECLINE_SILENT: // <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		{
			{
				LLStringUtil::format_map_t log_message_args;
				log_message_args["DESC"] = mDesc;
				log_message_args["NAME"] = mFromName;
				log_message = LLTrans::getString("InvOfferDecline", log_message_args);
			}
			chat.mText = log_message;
			if( LLMuteList::getInstance()->isMuted(mFromID ) && ! LLMuteList::getInstance()->isLinden(mFromName) )  // muting for SL-42269
			{
				chat.mMuted = TRUE;
			}

			// *NOTE dzaporozhan
			// Disabled logging to old chat floater to fix crash in group notices - EXT-4149
			// LLFloaterChat::addChatHistory(chat);
			
			LLDiscardAgentOffer* discard_agent_offer = new LLDiscardAgentOffer(mFolderID, mObjectID);
			discard_agent_offer->startFetch();
			if ((catp && gInventory.isCategoryComplete(mObjectID)) || (itemp && itemp->isFinished()))
			{
				discard_agent_offer->done();
			}
			else
			{
				opener = discard_agent_offer;
			}

			// <FS:Ansariel> Optional V1-like inventory accept messages
			if ((gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages") && button == IOR_DECLINE) && mIM == IM_INVENTORY_OFFERED)
			{
				send_decline_response();
			}
			// </FS:Ansariel> Optional V1-like inventory accept messages
			if (modified_form != NULL)
			{
				modified_form->setElementEnabled("Show", false);
				modified_form->setElementEnabled("Discard", false);
			}

			break;
		}
	default:
		// close button probably
		// The item has already been fetched and is in your inventory, we simply won't highlight it
		// OR delete it if the notification gets killed, since we don't want that to be a vector for 
		// losing inventory offers.
		break;
	}

	if(opener)
	{
		gInventory.addObserver(opener);
	}

	if(!mPersist)
	{
		delete this;
	}

	return false;
}

bool LLOfferInfo::inventory_task_offer_callback(const LLSD& notification, const LLSD& response)
{
	LLChat chat;
	std::string log_message;
	S32 button = LLNotification::getSelectedOption(notification, response);
	
	// For muting, we need to add the mute, then decline the offer.
	// This must be done here because:
	// * callback may be called immediately,
	// * adding the mute sends a message,
	// * we can't build two messages at once.
	if (2 == button)
	{
		LLNotificationPtr notification_ptr = LLNotifications::instance().find(notification["id"].asUUID());

		llassert(notification_ptr != NULL);
		if (notification_ptr != NULL)
		{
			gCacheName->get(mFromID, mFromGroup, boost::bind(&inventory_offer_mute_callback, _1, _2, _3));
		}
	}
	
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ImprovedInstantMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_MessageBlock);
	msg->addBOOLFast(_PREHASH_FromGroup, FALSE);
	msg->addUUIDFast(_PREHASH_ToAgentID, mFromID);
	msg->addU8Fast(_PREHASH_Offline, IM_ONLINE);
	msg->addUUIDFast(_PREHASH_ID, mTransactionID);
	msg->addU32Fast(_PREHASH_Timestamp, NO_TIMESTAMP); // no timestamp necessary
	std::string name;
	LLAgentUI::buildFullname(name);
	msg->addStringFast(_PREHASH_FromAgentName, name);
	msg->addStringFast(_PREHASH_Message, ""); 
	msg->addU32Fast(_PREHASH_ParentEstateID, 0);
	msg->addUUIDFast(_PREHASH_RegionID, LLUUID::null);
	msg->addVector3Fast(_PREHASH_Position, gAgent.getPositionAgent());
	LLInventoryObserver* opener = NULL;
	
	std::string from_string; // Used in the pop-up.
	std::string chatHistory_string;  // Used in chat history.
	if (mFromObject == TRUE)
	{
		if (mFromGroup)
		{
			std::string group_name;
			if (gCacheName->getGroupName(mFromID, group_name))
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " "+"'" 
				+ mFromName + LLTrans::getString("'") +" " + LLTrans::getString("InvOfferOwnedByGroup") 
				+ " "+ "'" + group_name + "'";
				
				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedByGroup") 
				+ " " + group_name + "'";
			}
			else
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " "+"'"
				+ mFromName +"'"+ " " + LLTrans::getString("InvOfferOwnedByUnknownGroup");
				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedByUnknownGroup");
			}
		}
		else
		{
/*
			std::string full_name;
			if (gCacheName->getFullName(mFromID, full_name))
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " "+ LLTrans::getString("'") + mFromName 
					+ LLTrans::getString("'")+" " + LLTrans::getString("InvOfferOwnedBy") + full_name;
				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedBy") + " " + full_name;
			}
			else
			{
				from_string = LLTrans::getString("InvOfferAnObjectNamed") + " "+LLTrans::getString("'") 
				+ mFromName + LLTrans::getString("'")+" " + LLTrans::getString("InvOfferOwnedByUnknownUser");
				chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedByUnknownUser");
			}
*/
// [SL:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
			std::string name_slurl = LLSLURL("agent", mFromID, "about").getSLURLString();

// [RLVa:KB] - Checked: RLVa-2.0.1
			// RELEASE-RLVa: [RLVa-2.0.1] Make sure this stays in sync with the condition in inventory_offer_handler()
			bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
				(RlvActions::canShowName(RlvActions::SNC_DEFAULT, mFromID)) || (!RlvUtil::isNearbyAgent(mFromID)) || (RlvUIEnabler::hasOpenIM(mFromID)) || (RlvUIEnabler::hasOpenProfile(mFromID));
			if (!fRlvCanShowName)
				name_slurl = LLSLURL("agent", mFromID, "rlvanonym").getSLURLString();
// [/RLVa:KB]

			from_string = LLTrans::getString("InvOfferAnObjectNamed") + " "+ LLTrans::getString("'") + mFromName 
				+ LLTrans::getString("'")+" " + LLTrans::getString("InvOfferOwnedBy") + name_slurl;
			chatHistory_string = mFromName + " " + LLTrans::getString("InvOfferOwnedBy") + " " + name_slurl;
// [/SL:KB]
		}
	}
	else
	{
		from_string = chatHistory_string = mFromName;
	}
	
	bool is_do_not_disturb = gAgent.isDoNotDisturb();
	
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1)
	bool fRlvNotifyAccepted = false;
// [/RLVa:KB]
	switch(button)
	{
		case IOR_ACCEPT:
			// ACCEPT. The math for the dialog works, because the accept
			// for inventory_offered, task_inventory_offer or
			// group_notice_inventory is 1 greater than the offer integer value.

// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1)
			// Only treat the offer as 'Give to #RLV' if:
			//   - the user has enabled the feature
			//   - the inventory offer came from a script (and specifies a folder)
			//   - the name starts with the prefix - mDesc format: '[OBJECTNAME]'  ( http://slurl.com/... )
			if ( (rlv_handler_t::isEnabled()) && (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
			{
				fRlvNotifyAccepted = true;
				if (!RlvSettings::getForbidGiveToRLV())
				{
					const LLUUID& idRlvRoot = RlvInventory::instance().getSharedRootID();
					if (idRlvRoot.notNull())
						mFolderID = idRlvRoot;

					fRlvNotifyAccepted = false;		// "accepted_in_rlv" is sent from RlvGiveToRLVTaskOffer *after* we have the folder

					RlvGiveToRLVTaskOffer* pOfferObserver = new RlvGiveToRLVTaskOffer(mTransactionID);
					gInventory.addObserver(pOfferObserver);
				}
			}
// [/RLVa:KB]

			// Generates IM_INVENTORY_ACCEPTED, IM_TASK_INVENTORY_ACCEPTED, 
			// or IM_GROUP_NOTICE_INVENTORY_ACCEPTED
			msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 1));
			msg->addBinaryDataFast(_PREHASH_BinaryBucket, &(mFolderID.mData),
								   sizeof(mFolderID.mData));
			// send the message
			msg->sendReliable(mHost);
			
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1)
			if (fRlvNotifyAccepted)
			{
				std::string::size_type idxToken = mDesc.find("'  ( http://");
				if (std::string::npos != idxToken)
					RlvBehaviourNotifyHandler::sendNotification("accepted_in_inv inv_offer " + mDesc.substr(1, idxToken - 1));
			}
// [/RLVa:KB]

			//don't spam them if they are getting flooded
			if (check_offer_throttle(mFromName, true))
			{
				log_message = chatHistory_string + " " + LLTrans::getString("InvOfferGaveYou") + " " + mDesc + LLTrans::getString(".");
				LLSD args;
				args["MESSAGE"] = log_message;
				LLNotificationsUtil::add("SystemMessageTip", args);
			}
			
			// we will want to open this item when it comes back.
			LL_DEBUGS("Messaging") << "Initializing an opener for tid: " << mTransactionID
			<< LL_ENDL;
			switch (mIM)
		{
			case IM_TASK_INVENTORY_OFFERED:
			case IM_GROUP_NOTICE:
			case IM_GROUP_NOTICE_REQUESTED:
			{
				// This is an offer from a task or group.
				// We don't use a new instance of an opener
				// We instead use the singular observer gOpenTaskOffer
				// Since it already exists, we don't need to actually do anything
			}
				break;
			default:
				LL_WARNS("Messaging") << "inventory_offer_callback: unknown offer type" << LL_ENDL;
				break;
		}	// end switch (mIM)
			break;
			
		case IOR_MUTE:
			// MUTE falls through to decline
		case IOR_DECLINE:
			// DECLINE. The math for the dialog works, because the decline
			// for inventory_offered, task_inventory_offer or
			// group_notice_inventory is 2 greater than the offer integer value.
			// Generates IM_INVENTORY_DECLINED, IM_TASK_INVENTORY_DECLINED,
			// or IM_GROUP_NOTICE_INVENTORY_DECLINED
		default:
			// close button probably (or any of the fall-throughs from above)
			msg->addU8Fast(_PREHASH_Dialog, (U8)(mIM + 2));
			msg->addBinaryDataFast(_PREHASH_BinaryBucket, EMPTY_BINARY_BUCKET, EMPTY_BINARY_BUCKET_SIZE);
			// send the message
			msg->sendReliable(mHost);
			
// [RLVa:KB] - Checked: 2010-09-23 (RLVa-1.2.1e) | Added: RLVa-1.2.1e
			if ( (rlv_handler_t::isEnabled()) && 
				 (IM_TASK_INVENTORY_OFFERED == mIM) && (LLAssetType::AT_CATEGORY == mType) && (mDesc.find(RLV_PUTINV_PREFIX) == 1) )
			{
				std::string::size_type idxToken = mDesc.find("'  ( http://");
				if (std::string::npos != idxToken)
					RlvBehaviourNotifyHandler::sendNotification("declined inv_offer " + mDesc.substr(1, idxToken - 1));
			}
// [/RLVa:KB]

			if (gSavedSettings.getBOOL("LogInventoryDecline"))
			{
				LLStringUtil::format_map_t log_message_args;
				log_message_args["DESC"] = mDesc;
				log_message_args["NAME"] = mFromName;
				log_message = LLTrans::getString("InvOfferDecline", log_message_args);


				LLSD args;
				args["MESSAGE"] = log_message;
				LLNotificationsUtil::add("SystemMessageTip", args);
			}
			
			if (is_do_not_disturb &&	(!mFromGroup && !mFromObject))
			{
				send_do_not_disturb_message(msg,mFromID);
			}
			break;
	}
	
	if(opener)
	{
		gInventory.addObserver(opener);
	}

	if(!mPersist)
	{
		delete this;
	}
	return false;
}

class LLPostponedOfferNotification: public LLPostponedNotification
{
protected:
	/* virtual */
	void modifyNotificationParams()
	{
		LLSD substitutions = mParams.substitutions;
		substitutions["NAME"] = mName;
		mParams.substitutions = substitutions;
	}
};

void LLOfferInfo::initRespondFunctionMap()
{
	if(mRespondFunctions.empty())
	{
		mRespondFunctions["ObjectGiveItem"] = boost::bind(&LLOfferInfo::inventory_task_offer_callback, this, _1, _2);
		mRespondFunctions["OwnObjectGiveItem"] = boost::bind(&LLOfferInfo::inventory_task_offer_callback, this, _1, _2);
		mRespondFunctions["UserGiveItem"] = boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2);
		// <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		mRespondFunctions["UserGiveItemLegacy"] = boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2);
	}
}

void inventory_offer_handler(LLOfferInfo* info)
{
	// If muted, don't even go through the messaging stuff.  Just curtail the offer here.
	// Passing in a null UUID handles the case of where you have muted one of your own objects by_name.
	// The solution for STORM-1297 seems to handle the cases where the object is owned by someone else.
	if (LLMuteList::getInstance()->isMuted(info->mFromID, info->mFromName) ||
		LLMuteList::getInstance()->isMuted(LLUUID::null, info->mFromName))
	{
		info->forceResponse(IOR_MUTE);
		return;
	}


	bool bAutoAccept(false);
	// Avoid the Accept/Discard dialog if the user so desires. JC
	// <FS:Ansariel> Auto-accept any kind of inventory (FIRE-4128)
	//if (gSavedSettings.getBOOL("AutoAcceptNewInventory")
	//	&& (info->mType == LLAssetType::AT_NOTECARD
	//		|| info->mType == LLAssetType::AT_LANDMARK
	//		|| info->mType == LLAssetType::AT_TEXTURE))
//	if (gSavedSettings.getBOOL("AutoAcceptNewInventory"))
	// </FS:Ansariel> Auto-accept any kind of inventory (FIRE-4128)
// [RLVa:KB]
	// Don't auto-accept give-to-RLV inventory offers
	if ( (gSavedSettings.getBOOL("AutoAcceptNewInventory")) &&
		 ( (!rlv_handler_t::isEnabled()) || (!RlvInventory::instance().isGiveToRLVOffer(*info)) ) )
// [/RLVa:KB]
	{
		// For certain types, just accept the items into the inventory,
		// and possibly open them on receipt depending upon "ShowNewInventory".
		bAutoAccept = true;
	}

	// Strip any SLURL from the message display. (DEV-2754)
	std::string msg = info->mDesc;
	int indx = msg.find(" ( http://slurl.com/secondlife/");
	if(indx == std::string::npos)
	{
		// try to find new slurl host
		indx = msg.find(" ( http://maps.secondlife.com/secondlife/");
	}
	if(indx >= 0)
	{
		LLStringUtil::truncate(msg, indx);
	}

	LLSD args;
	args["[OBJECTNAME]"] = msg;

	LLSD payload;

	// must protect against a NULL return from lookupHumanReadable()
	std::string typestr = ll_safe_string(LLAssetType::lookupHumanReadable(info->mType));
	if (!typestr.empty())
	{
		// human readable matches string name from strings.xml
		// lets get asset type localized name
		args["OBJECTTYPE"] = LLTrans::getString(typestr);
	}
	else
	{
		LL_WARNS("Messaging") << "LLAssetType::lookupHumanReadable() returned NULL - probably bad asset type: " << info->mType << LL_ENDL;
		args["OBJECTTYPE"] = "";

		// This seems safest, rather than propagating bogosity
		LL_WARNS("Messaging") << "Forcing an inventory-decline for probably-bad asset type." << LL_ENDL;
		info->forceResponse(IOR_DECLINE);
		return;
	}

	// If mObjectID is null then generate the object_id based on msg to prevent
	// multiple creation of chiclets for same object.
	LLUUID object_id = info->mObjectID;
	if (object_id.isNull())
		object_id.generate(msg);

	payload["from_id"] = info->mFromID;
	// Needed by LLScriptFloaterManager to bind original notification with 
	// faked for toast one.
	payload["object_id"] = object_id;
	// Flag indicating that this notification is faked for toast.
	payload["give_inventory_notification"] = FALSE;
	args["OBJECTFROMNAME"] = info->mFromName;
	args["NAME"] = info->mFromName;
	if (info->mFromGroup)
	{
		args["NAME_SLURL"] = LLSLURL("group", info->mFromID, "about").getSLURLString();
	}
	else
	{
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
		args["NAME_LABEL"] = LLSLURL("agent", info->mFromID, "completename").getSLURLString();
// [/SL:KB]
		args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "about").getSLURLString();
	}
	std::string verb = "select?name=" + LLURI::escape(msg);
	args["ITEM_SLURL"] = LLSLURL("inventory", info->mObjectID, verb.c_str()).getSLURLString();

	LLNotification::Params p;

	// Object -> Agent Inventory Offer
	if (info->mFromObject && !bAutoAccept)
	{
// [RLVa:KB] - Checked: RLVa-1.2.2
		// Only filter if the object owner is a nearby agent
		if ( (RlvActions::isRlvEnabled()) && (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) && (RlvUtil::isNearbyAgent(info->mFromID)) )
		{
			payload["rlv_shownames"] = TRUE;
			args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "rlvanonym").getSLURLString();
		}
// [/RLVa:KB]

		// Inventory Slurls don't currently work for non agent transfers, so only display the object name.
		args["ITEM_SLURL"] = msg;
		// Note: sets inventory_task_offer_callback as the callback
		p.substitutions(args).payload(payload).functor.responder(LLNotificationResponderPtr(info));
		info->mPersist = true;

		// Offers from your own objects need a special notification template.
		p.name = info->mFromID == gAgentID ? "OwnObjectGiveItem" : "ObjectGiveItem";

		// Pop up inv offer chiclet and let the user accept (keep), or reject (and silently delete) the inventory.
	    LLPostponedNotification::add<LLPostponedOfferNotification>(p, info->mFromID, info->mFromGroup == TRUE);
	}
	else // Agent -> Agent Inventory Offer
	{
// [RLVa:KB] - Checked: RLVa-2.0.1
		// Only filter if the offer is from a nearby agent and if there's no open IM session (doesn't necessarily have to be focused)
		bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
			(RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) || (!RlvUtil::isNearbyAgent(info->mFromID)) || (RlvUIEnabler::hasOpenIM(info->mFromID)) || (RlvUIEnabler::hasOpenProfile(info->mFromID));
		if (!fRlvCanShowName)
		{
			payload["rlv_shownames"] = TRUE;
			args["NAME"] = RlvStrings::getAnonym(info->mFromName);
			args["NAME_SLURL"] = LLSLURL("agent", info->mFromID, "rlvanonym").getSLURLString();
		}
// [/RLVa:KB]

		p.responder = info;
		// Note: sets inventory_offer_callback as the callback
		// *TODO fix memory leak
		// inventory_offer_callback() is not invoked if user received notification and 
		// closes viewer(without responding the notification)
		p.substitutions(args).payload(payload).functor.responder(LLNotificationResponderPtr(info));
		info->mPersist = true;
		// <FS:Ansariel> FIRE-3832: Silent accept/decline of inventory offers
		//p.name = "UserGiveItem";
		p.name = (gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages") ? "UserGiveItemLegacy" : "UserGiveItem");
		// </FS:Ansariel>
		p.offer_from_agent = true;
		
		// Prefetch the item into your local inventory.
		LLInventoryFetchItemsObserver* fetch_item = new LLInventoryFetchItemsObserver(info->mObjectID);
		fetch_item->startFetch();
		if(fetch_item->isFinished())
		{
			fetch_item->done();
		}
		else
		{
			gInventory.addObserver(fetch_item);
		}
		
		// In viewer 2 we're now auto receiving inventory offers and messaging as such (not sending reject messages).
		// <FS:Ansariel> Optional V1-like inventory accept messages
		//info->send_auto_receive_response();
		// Also needs to be send on auto-accept so the item gets into the inventory!
		if (bAutoAccept || !gSavedSettings.getBOOL("FSUseLegacyInventoryAcceptMessages"))
		{
			info->send_auto_receive_response();
		}
		// </FS:Ansariel> Optional V1-like inventory accept messages

        if (gAgent.isDoNotDisturb()) 
        {
            send_do_not_disturb_message(gMessageSystem, info->mFromID);
        }
        
		if( !bAutoAccept ) // if we auto accept, do not pester the user
		{
			// Inform user that there is a script floater via toast system
			payload["give_inventory_notification"] = TRUE;
			p.payload = payload;
			LLPostponedNotification::add<LLPostponedOfferNotification>(p, info->mFromID, false);
		}
		// <FS:Ansariel> FIRE-19540: Log auto-accepted inventory to nearby chat
		else if (gSavedSettings.getBOOL("FSLogAutoAcceptInventoryToChat"))
		{
			std::string message_type;
			LLStringUtil::format_map_t chat_args;
			
			chat_args["OBJECT_TYPE"] = (!typestr.empty() ? LLTrans::getString(typestr) : "");
			chat_args["DESC"] = msg;

			if (info->mFromObject)
			{
				std::string str_pos;
				std::string::size_type idx_start = info->mDesc.rfind(" ( http://");
				std::string::size_type idx_end = info->mDesc.find(" )", idx_start);
				if (idx_start != std::string::npos && idx_end != std::string::npos)
				{
					LLSLURL url_pos(info->mDesc.substr(idx_start + 3, idx_end - (idx_start + 3)));
					str_pos = "&slurl=" + LLURI::escape(url_pos.getLocationString());
				}

				chat_args["OBJECT_NAME"] = "[" + LLSLURL("objectim", info->mObjectID, "").getSLURLString() + "?name=" + LLURI::escape(info->mFromName) + "&owner=" + info->mFromID.asString() + (info->mFromGroup ? "&groupowned=true" : "") + str_pos + " " + info->mFromName + "]";
				message_type = "InvOfferAutoAcceptObject";
			}
			else
			{
				bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
					(RlvActions::canShowName(RlvActions::SNC_DEFAULT, info->mFromID)) || (!RlvUtil::isNearbyAgent(info->mFromID)) || (RlvUIEnabler::hasOpenIM(info->mFromID)) || (RlvUIEnabler::hasOpenProfile(info->mFromID));
				
				std::string name_slurl = LLSLURL("agent", info->mFromID, (fRlvCanShowName ? "inspect" : "rlvanonym")).getSLURLString();

				chat_args["USER_NAME"] = name_slurl;
				message_type = "InvOfferAutoAcceptUser";
			}

			report_to_nearby_chat(LLTrans::getString(message_type, chat_args));
		}
		// </FS:Ansariel>

		// <FS:Ansariel> Show offered inventory also if auto-accept is enabled (FIRE-5101)
		if (bAutoAccept && gSavedSettings.getBOOL("ShowNewInventory"))
		{
			LLViewerInventoryCategory* catp = NULL;
			catp = (LLViewerInventoryCategory*)gInventory.getCategory(info->mObjectID);
			LLViewerInventoryItem* itemp = NULL;
			if(!catp)
			{
				itemp = (LLViewerInventoryItem*)gInventory.getItem(info->mObjectID);
			}

			LLOpenAgentOffer* open_agent_offer = new LLOpenAgentOffer(info->mObjectID, info->mFromName);
			open_agent_offer->startFetch();
			if(catp || (itemp && itemp->isFinished()))
			{
				open_agent_offer->done();
			}
			else
			{
				gInventory.addObserver(open_agent_offer);
			}
		}
		// </FS:Ansariel> Show offered inventory also if auto-accept is enabled (FIRE-5101)
	}

	LLFirstUse::newInventory();
}

bool lure_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}
	
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID lure_id = notification["payload"]["lure_id"].asUUID();
	BOOL godlike = notification["payload"]["godlike"].asBoolean();

	switch(option)
	{
	case 0:
		{
			// accept
			gAgent.teleportViaLure(lure_id, godlike);
		}
		break;
	case 1:
	default:
		// decline
		send_simple_im(from_id,
					   LLStringUtil::null,
					   IM_LURE_DECLINED,
					   lure_id);
		break;
	}

// <FS:Ansariel> [FS communication UI] FIRE-11536: Commenting out CHUI-112;
//               Reposting the notification form will squeeze it somewhere
//               within the IM floater and we don't need it for our comm. UI.
//	LLNotificationPtr notification_ptr = LLNotifications::instance().find(notification["id"].asUUID());
//
//	if (notification_ptr)
//	{
//		LLNotificationFormPtr modified_form(new LLNotificationForm(*notification_ptr->getForm()));
//		modified_form->setElementEnabled("Teleport", false);
//		modified_form->setElementEnabled("Cancel", false);
//		notification_ptr->updateForm(modified_form);
//		//notification_ptr->repost();
//// [SL:KB] - Patch: UI-Notifications | Checked: 2013-05-09 (Catznip-3.5)
//		// Assume that any offer notification with "getCanBeStored() == true" is the result of RLVa routing it to the notifcation syswell
//		/*const*/ LLNotificationsUI::LLScreenChannel* pChannel = LLNotificationsUI::LLChannelManager::instance().getNotificationScreenChannel();
//		/*const*/ LLNotificationsUI::LLToast* pToast = (pChannel) ? pChannel->getToastByNotificationID(notification["id"].asUUID()) : NULL;
//		if ( (!pToast) || (!pToast->getCanBeStored()) )
//		{
//// [/SL:KB]
//			notification_ptr->repost();
//	}
// </FS:Ansariel>

	return false;
}
static LLNotificationFunctorRegistration lure_callback_reg("TeleportOffered", lure_callback);

bool mature_lure_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = 0;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}
	
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	LLUUID lure_id = notification["payload"]["lure_id"].asUUID();
	BOOL godlike = notification["payload"]["godlike"].asBoolean();
	U8 region_access = static_cast<U8>(notification["payload"]["region_maturity"].asInteger());

	switch(option)
	{
	case 0:
		{
			// accept
			gSavedSettings.setU32("PreferredMaturity", static_cast<U32>(region_access));
			gAgent.setMaturityRatingChangeDuringTeleport(region_access);
			gAgent.teleportViaLure(lure_id, godlike);
		}
		break;
	case 1:
	default:
		// decline
		send_simple_im(from_id,
					   LLStringUtil::null,
					   IM_LURE_DECLINED,
					   lure_id);
		break;
	}
	return false;
}
static LLNotificationFunctorRegistration mature_lure_callback_reg("TeleportOffered_MaturityExceeded", mature_lure_callback);

bool goto_url_callback(const LLSD& notification, const LLSD& response)
{
	std::string url = notification["payload"]["url"].asString();
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if(1 == option)
	{
		LLWeb::loadURL(url);
	}
	return false;
}
static LLNotificationFunctorRegistration goto_url_callback_reg("GotoURL", goto_url_callback);

bool inspect_remote_object_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	if (0 == option)
	{
		LLFloaterReg::showInstance("inspect_remote_object", notification["payload"]);
	}
	return false;
}
static LLNotificationFunctorRegistration inspect_remote_object_callback_reg("ServerObjectMessage", inspect_remote_object_callback);

class LLPostponedServerObjectNotification: public LLPostponedNotification
{
protected:
	/* virtual */
	void modifyNotificationParams()
	{
		LLSD payload = mParams.payload;
		mParams.payload = payload;
	}
};

static bool parse_lure_bucket(const std::string& bucket,
							  U64& region_handle,
							  LLVector3& pos,
							  LLVector3& look_at,
							  U8& region_access)
{
	// tokenize the bucket
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("|", "", boost::keep_empty_tokens);
	tokenizer tokens(bucket, sep);
	tokenizer::iterator iter = tokens.begin();

	S32 gx,gy,rx,ry,rz,lx,ly,lz;
	try
	{
		gx = boost::lexical_cast<S32>((*(iter)).c_str());
		gy = boost::lexical_cast<S32>((*(++iter)).c_str());
		rx = boost::lexical_cast<S32>((*(++iter)).c_str());
		ry = boost::lexical_cast<S32>((*(++iter)).c_str());
		rz = boost::lexical_cast<S32>((*(++iter)).c_str());
		lx = boost::lexical_cast<S32>((*(++iter)).c_str());
		ly = boost::lexical_cast<S32>((*(++iter)).c_str());
		lz = boost::lexical_cast<S32>((*(++iter)).c_str());
	}
	catch( boost::bad_lexical_cast& )
	{
		LL_WARNS("parse_lure_bucket")
			<< "Couldn't parse lure bucket."
			<< LL_ENDL;
		return false;
	}
	// Grab region access
	region_access = SIM_ACCESS_MIN;
	if (++iter != tokens.end())
	{
		std::string access_str((*iter).c_str());
		LLStringUtil::trim(access_str);
		if ( access_str == "A" )
		{
			region_access = SIM_ACCESS_ADULT;
		}
		else if ( access_str == "M" )
		{
			region_access = SIM_ACCESS_MATURE;
		}
		else if ( access_str == "PG" )
		{
			region_access = SIM_ACCESS_PG;
		}
	}

	pos.setVec((F32)rx, (F32)ry, (F32)rz);
	look_at.setVec((F32)lx, (F32)ly, (F32)lz);

	region_handle = to_region_handle(gx, gy);
	return true;
}

// Strip out "Resident" for display, but only if the message came from a user
// (rather than a script)
static std::string clean_name_from_im(const std::string& name, EInstantMessage type)
{
	switch(type)
	{
	case IM_NOTHING_SPECIAL:
	case IM_MESSAGEBOX:
	case IM_GROUP_INVITATION:
	case IM_INVENTORY_OFFERED:
	case IM_INVENTORY_ACCEPTED:
	case IM_INVENTORY_DECLINED:
	case IM_GROUP_VOTE:
	case IM_GROUP_MESSAGE_DEPRECATED:
	//IM_TASK_INVENTORY_OFFERED
	//IM_TASK_INVENTORY_ACCEPTED
	//IM_TASK_INVENTORY_DECLINED
	case IM_NEW_USER_DEFAULT:
	case IM_SESSION_INVITE:
	case IM_SESSION_P2P_INVITE:
	case IM_SESSION_GROUP_START:
	case IM_SESSION_CONFERENCE_START:
	case IM_SESSION_SEND:
	case IM_SESSION_LEAVE:
	//IM_FROM_TASK
	case IM_DO_NOT_DISTURB_AUTO_RESPONSE:
	case IM_CONSOLE_AND_CHAT_HISTORY:
	case IM_LURE_USER:
	case IM_LURE_ACCEPTED:
	case IM_LURE_DECLINED:
	case IM_GODLIKE_LURE_USER:
	case IM_TELEPORT_REQUEST:
	case IM_GROUP_ELECTION_DEPRECATED:
	//IM_GOTO_URL
	//IM_FROM_TASK_AS_ALERT
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_INVENTORY_ACCEPTED:
	case IM_GROUP_NOTICE_INVENTORY_DECLINED:
	case IM_GROUP_INVITATION_ACCEPT:
	case IM_GROUP_INVITATION_DECLINE:
	case IM_GROUP_NOTICE_REQUESTED:
	case IM_FRIENDSHIP_OFFERED:
	case IM_FRIENDSHIP_ACCEPTED:
	case IM_FRIENDSHIP_DECLINED_DEPRECATED:
	//IM_TYPING_START
	//IM_TYPING_STOP
		return LLCacheName::cleanFullName(name);
	default:
		return name;
	}
}

static std::string clean_name_from_task_im(const std::string& msg,
										   BOOL from_group)
{
	boost::smatch match;
	static const boost::regex returned_exp(
		"(.*been returned to your inventory lost and found folder by )(.+)( (from|near).*)");
	if (boost::regex_match(msg, match, returned_exp))
	{
		// match objects are 1-based for groups
		std::string final = match[1].str();
		std::string name = match[2].str();
		// Don't try to clean up group names
		if (!from_group)
		{
			final += LLCacheName::buildUsername(name);
		}
		final += match[3].str();
		return final;
	}
	return msg;
}

static void notification_display_name_callback(const LLUUID& id,
					  const LLAvatarName& av_name,
					  const std::string& name, 
					  LLSD& substitutions, 
					  const LLSD& payload)
{
	substitutions["NAME"] = av_name.getDisplayName();
	LLNotificationsUtil::add(name, substitutions, payload);
}

class LLPostponedIMSystemTipNotification: public LLPostponedNotification
{
protected:
	/* virtual */
	void modifyNotificationParams()
	{
		LLSD payload = mParams.payload;
		payload["SESSION_NAME"] = mName;
		mParams.payload = payload;
	}

};

// Callback for name resolution of a god/estate message
static void god_message_name_cb(const LLAvatarName& av_name, LLChat chat, std::string message)
{	
	LLSD args;
	args["NAME"] = av_name.getCompleteName();
	args["MESSAGE"] = message;
	LLNotificationsUtil::add("GodMessage", args);

	// Treat like a system message and put in chat history.
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	chat.mText = message;

	// <FS:Ansariel> [FS communication UI]
	//LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
	FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
	// </FS:Ansariel> [FS communication UI]
	if (nearby_chat)
	{
		nearby_chat->addMessage(chat);
	}
}

const std::string NOT_ONLINE_MSG("User not online - message will be stored and delivered later.");
const std::string NOT_ONLINE_INVENTORY("User not online - inventory has been saved.");
void translate_if_needed(std::string& message)
{
	if (message == NOT_ONLINE_MSG)
	{
		message = LLTrans::getString("not_online_msg");
	}
	else if (message == NOT_ONLINE_INVENTORY)
	{
		message = LLTrans::getString("not_online_inventory");
	}
}

// <FS:Ansariel> FIRE-505: Group name not shown in notification well
static void notification_group_name_cb(const std::string& group_name,
										const std::string& sender,
										const std::string& subject,
										const std::string& message,
										const LLSD& payload,
										U32 timestamp)
{
	LLAvatarName av_name;
	av_name.fromString(sender);
	LLSD args;
	args["SENDER"] = av_name.getUserNameForDisplay();
	args["GROUP"] = group_name;
	args["SUBJECT"] = subject;
	args["MESSAGE"] = message;
	LLDate notice_date = LLDate(timestamp).notNull() ? LLDate(timestamp) : LLDate::now();
	LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).time_stamp(notice_date));
	make_ui_sound("UISndGroupNotice"); // <FS:PP> Group notice sound
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-6786: Always show teleport location in teleport offer
static void teleport_region_info_cb(const std::string& slurl, LLSD args, const LLSD& payload, const LLUUID& from_id, const LLUUID& session_id, bool can_user_access_dst_region, bool does_user_require_maturity_increase)
{
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		args["POS_SLURL"] = RlvStrings::getString(RLV_STRING_HIDDEN);
	}
	else
	{
		args["POS_SLURL"] = slurl;
	}

	LLNotification::Params params;

	if (!can_user_access_dst_region)
	{
		params.name = "TeleportOffered_MaturityBlocked_SLUrl";
		send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
		send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
	}
	else if (does_user_require_maturity_increase)
	{
		params.name = "TeleportOffered_MaturityExceeded_SLUrl";
	}
	else
	{
		params.name = "TeleportOffered_SLUrl";
		params.functor.name = "TeleportOffered";
	}

	params.substitutions = args;
	params.payload = payload;
	LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);

	LLWindow* viewer_window = gViewerWindow->getWindow();
	static LLCachedControl<bool> sFlashIcon(gSavedSettings, "FSFlashOnMessage");
	if (viewer_window && sFlashIcon)
	{
		viewer_window->flashIcon(5.f);
	}
}
// </FS:Ansariel>

void process_improved_im(LLMessageSystem *msg, void **user_data)
{
	LLUUID from_id;
	BOOL from_group;
	LLUUID to_id;
	U8 offline;
	U8 d = 0;
	LLUUID session_id;
	U32 timestamp;
	std::string name;
	std::string message;
	U32 parent_estate_id = 0;
	LLUUID region_id;
	LLVector3 position;
	U8 binary_bucket[MTUBYTES];
	S32 binary_bucket_size;
	LLChat chat;
	std::string buffer;
	
	// *TODO: Translate - need to fix the full name to first/last (maybe)
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, from_id);
	msg->getBOOLFast(_PREHASH_MessageBlock, _PREHASH_FromGroup, from_group);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ToAgentID, to_id);
	msg->getU8Fast(  _PREHASH_MessageBlock, _PREHASH_Offline, offline);
	msg->getU8Fast(  _PREHASH_MessageBlock, _PREHASH_Dialog, d);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_ID, session_id);
	msg->getU32Fast( _PREHASH_MessageBlock, _PREHASH_Timestamp, timestamp);
	//msg->getData("MessageBlock", "Count",		&count);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_FromAgentName, name);
	msg->getStringFast(_PREHASH_MessageBlock, _PREHASH_Message,		message);
	msg->getU32Fast(_PREHASH_MessageBlock, _PREHASH_ParentEstateID, parent_estate_id);
	msg->getUUIDFast(_PREHASH_MessageBlock, _PREHASH_RegionID, region_id);
	msg->getVector3Fast(_PREHASH_MessageBlock, _PREHASH_Position, position);
	msg->getBinaryDataFast(  _PREHASH_MessageBlock, _PREHASH_BinaryBucket, binary_bucket, 0, 0, MTUBYTES);
	binary_bucket_size = msg->getSizeFast(_PREHASH_MessageBlock, _PREHASH_BinaryBucket);
	EInstantMessage dialog = (EInstantMessage)d;

	// NaCl - Antispam Registry
	if (dialog != IM_TYPING_START && dialog != IM_TYPING_STOP &&											// Typing notifications
		!(dialog == IM_NOTHING_SPECIAL && offline == IM_OFFLINE && from_id.notNull() && to_id.notNull()) &&	// Saved offline IMs
		!(dialog == IM_FROM_TASK && offline == IM_OFFLINE)													// Saved offline IMs from objects
		)
	{
		if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_IM, from_id, ANTISPAM_SOURCE_AGENT))
		{
			return;
		}
	}
	// NaCl End

    // make sure that we don't have an empty or all-whitespace name
	LLStringUtil::trim(name);
	if (name.empty())
	{
        name = LLTrans::getString("Unnamed");
	}

	// Preserve the unaltered name for use in group notice mute checking.
	std::string original_name = name;

	// IDEVO convert new-style "Resident" names for display
	name = clean_name_from_im(name, dialog);

	BOOL is_do_not_disturb = gAgent.isDoNotDisturb();
	BOOL is_autorespond = gAgent.getAutorespond();
	BOOL is_autorespond_nonfriends = gAgent.getAutorespondNonFriends();
	// <FS:PP> FIRE-1245: Option to block/reject teleport offers
	BOOL is_rejecting_tp_offers = gAgent.getRejectTeleportOffers();
	static LLCachedControl<bool> FSDontRejectTeleportOffersFromFriends(gSavedPerAccountSettings, "FSDontRejectTeleportOffersFromFriends");
	// </FS:PP>
	BOOL is_rejecting_group_invites = gAgent.getRejectAllGroupInvites(); // <FS:PP> Option to block/reject all group invites
	BOOL is_rejecting_friendship_requests = gAgent.getRejectFriendshipRequests(); // <FS:PP> FIRE-15233: Automatic friendship request refusal
	BOOL is_autorespond_muted = gSavedPerAccountSettings.getBOOL("FSSendMutedAvatarResponse");
	BOOL is_muted = LLMuteList::getInstance()->isMuted(from_id, name, LLMute::flagTextChat)
		// object IMs contain sender object id in session_id (STORM-1209)
		|| (dialog == IM_FROM_TASK && LLMuteList::getInstance()->isMuted(session_id));
	BOOL is_owned_by_me = FALSE;
	BOOL is_friend = (LLAvatarTracker::instance().getBuddyInfo(from_id) == NULL) ? false : true;
	static LLCachedControl<bool> accept_im_from_only_friend(gSavedSettings, "VoiceCallsFriendsOnly");
	//BOOL is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
	//		LLMuteList::getInstance()->isLinden(name); <:FS:TM> Bear compie fix - is_linden not referenced

	// <FS:PP> FIRE-10500: Autoresponse for (Away)
	static LLCachedControl<bool> FSSendAwayAvatarResponse(gSavedPerAccountSettings, "FSSendAwayAvatarResponse");
	BOOL is_afk = gAgent.getAFK();
	// </FS:PP>

	chat.mMuted = is_muted;
	chat.mFromID = from_id;
	chat.mFromName = name;
	chat.mSourceType = (from_id.isNull() || (name == std::string(SYSTEM_FROM))) ? CHAT_SOURCE_SYSTEM : CHAT_SOURCE_AGENT;
	
	if (chat.mSourceType == CHAT_SOURCE_SYSTEM)
	{ // Translate server message if required (MAINT-6109)
		translate_if_needed(message);
	}

	LLViewerObject *source = gObjectList.findObject(session_id); //Session ID is probably the wrong thing.
	if (source)
	{
		is_owned_by_me = source->permYouOwner();
	}

	// NaCl - Newline flood protection
	static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
	if (useAntiSpam && dialog != IM_GROUP_INVITATION)
	{
		bool doCheck = true;
		if (from_id.isNull() || gAgentID == from_id)
		{
			doCheck = false;
		}
		if (doCheck && is_owned_by_me)
		{
			doCheck = false;
		}
		if (doCheck && NACLAntiSpamRegistry::instance().checkNewlineFlood(ANTISPAM_QUEUE_IM, from_id, message))
		{
			return;
		}
	}
	// NaCl End

	std::string separator_string(": ");

	LLSD args;
	LLSD payload;
	LLNotification::Params params;

	switch(dialog)
	{ 
	case IM_CONSOLE_AND_CHAT_HISTORY:
		args["MESSAGE"] = message;
		payload["from_id"] = from_id;

		params.name = "IMSystemMessageTip";
		params.substitutions = args;
		params.payload = payload;
	    LLPostponedNotification::add<LLPostponedIMSystemTipNotification>(params, from_id, false);
		break;

	case IM_NOTHING_SPECIAL:	// p2p IM
		// Don't show dialog, just do IM
		if (!gAgent.isGodlike()
				&& gAgent.getRegion()->isPrelude() 
				&& to_id.isNull() )
		{
			// do nothing -- don't distract newbies in
			// Prelude with global IMs
		}
// [RLVa:KB] - Checked: RLVa-2.0.3
		else if ( (RlvActions::isRlvEnabled()) && (RlvSettings::getEnableIMQuery()) && (offline == IM_ONLINE) && ("@version" == message) && (!is_muted) && ((!accept_im_from_only_friend) || (is_friend)) )
		{
			RlvUtil::sendBusyMessage(from_id, RlvStrings::getVersion(LLUUID::null), session_id);
		}
// [/RLVa:KB]
//		else if (offline == IM_ONLINE 
//					&& is_do_not_disturb
//					&& from_id.notNull() //not a system message
//					&& to_id.notNull()) //not global message
// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
		// <FS:Ansariel> Only send the busy reponse if either the sender is not
		//               muted OR the sender is muted and we explicitely want
		//               to inform him about that fact.
		else if (offline == IM_ONLINE
					&& (!accept_im_from_only_friend || is_friend)                                    // is friend or accept IMs from friend only disabled
					&& ((is_do_not_disturb && (!is_muted || (is_muted && !is_autorespond_muted))) || // do not disturb
						(is_autorespond && !is_muted) ||                                             // autorespond everyone
						(is_autorespond_nonfriends && !is_friend && !is_muted) ||                    // autorespond friends only
						(is_afk && FSSendAwayAvatarResponse && !is_muted))                           // away
					&& from_id.notNull() //not a system message
					&& to_id.notNull() //not global message
					&& RlvActions::canReceiveIM(from_id))
// [/RLVa:KB]
		{
			// <FS:Ansariel> Log autoresponse notification after initial message
			bool has_session = true;

			// <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
			// Session id will be null if avatar answers from offline IM via email
			std::string response;
			if (!gIMMgr->hasSession(session_id) && session_id.notNull())
			{
			// </FS:Ansariel>
				// <FS:Ansariel> Log autoresponse notification after initial message
				has_session = false;
				// <FS:Ansariel> FS autoresponse feature
				std::string my_name;
				LLAgentUI::buildFullname(my_name);
				if (is_do_not_disturb)
				{
					response = gSavedPerAccountSettings.getString("DoNotDisturbModeResponse");
				}
				else if (is_autorespond_nonfriends && !is_friend)
				{
					response = gSavedPerAccountSettings.getString("FSAutorespondNonFriendsResponse");
				}
				else if (is_autorespond)
				{
					response = gSavedPerAccountSettings.getString("FSAutorespondModeResponse");
				}
				// <FS:PP> FIRE-10500: Autoresponse for (Away)
				else if (is_afk && FSSendAwayAvatarResponse)
				{
					response = gSavedPerAccountSettings.getString("FSAwayAvatarResponse");
				}
				// </FS:PP>
				else
				{
					LL_WARNS() << "Unknown auto-response mode" << LL_ENDL;
				}
				pack_instant_message(
					gMessageSystem,
					gAgent.getID(),
					FALSE,
					gAgent.getSessionID(),
					from_id,
					my_name,
					response,
					IM_ONLINE,
					IM_DO_NOT_DISTURB_AUTO_RESPONSE,
					session_id);
				gAgent.sendReliableMessage();
				// </FS:Ansariel> FS autoresponse feature
			// <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
			}
			// </FS:Ansariel>

			// <FS:Ansariel> checkfor and process reqinfo
			if (has_session)
			{
				message = FSData::getInstance()->processRequestForInfo(from_id,message,name,session_id);
			}
			// </FS:Ansariel>

			// now store incoming IM in chat history

			buffer = message;
	
			LL_DEBUGS("Messaging") << "session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
			chat.mText = buffer;
			bool keyword_alert_performed = false;
			if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
			{
				FSKeywords::notify(chat);
				keyword_alert_performed = true;
			}
			// </FS:PP>

			// add to IM panel, but do not bother the user
			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				IM_OFFLINE == offline,
				LLStringUtil::null,
				dialog,
				parent_estate_id,
				region_id,
				position,
				true,
				false,
				keyword_alert_performed);

			// <FS:Ansariel> Old "do not disturb" message behavior: only send once if session not open
			//if (!gIMMgr->isDNDMessageSend(session_id))
			//{
			//	// return a standard "do not disturb" message, but only do it to online IM
			//	// (i.e. not other auto responses and not store-and-forward IM)
			//	send_do_not_disturb_message(msg, from_id, session_id);
			//	gIMMgr->setDNDMessageSent(session_id, true);
			//}
			// </FS:Ansariel>

			if (!has_session)
			{
				// <FS:LO> Fire-5389 - "Autoresponse Sent" message added to Firestorm as was in Phoenix
				LLStringUtil::format_map_t args;
				args["MESSAGE"] = response;

				gIMMgr->addMessage(
					session_id,
					gAgentID,
					LLStringUtil::null, // Pass null value so no name gets prepended
					LLTrans::getString("IM_autoresponse_sent", args),
					false,
					name,
					IM_NOTHING_SPECIAL,
					parent_estate_id,
					region_id,
					position,
					false,
					true
					);
				// </FS:LO>

				// <FS:Ansariel> Send inventory item on autoresponse
				LLUUID item_id(gSavedPerAccountSettings.getString("FSAutoresponseItemUUID"));
				if (item_id.notNull())
				{
					LLInventoryItem* item = dynamic_cast<LLInventoryItem*>(gInventory.getItem(item_id));
					if (item)
					{
						gIMMgr->addMessage(
								session_id,
								gAgentID,
								LLStringUtil::null, // Pass null value so no name gets prepended
								LLTrans::getString("IM_autoresponse_item_sent", LLSD().with("[ITEM_NAME]", item->getName())),
								false,
								name,
								IM_NOTHING_SPECIAL,
								parent_estate_id,
								region_id,
								position,
								false,
								true);
						LLGiveInventory::doGiveInventoryItem(from_id, item, session_id);
					}
				}
				// </FS:Ansariel>
			}
		}
		else if (from_id.isNull())
		{
			LLSD args;
			args["MESSAGE"] = message;
			LLNotificationsUtil::add("SystemMessage", args);
		}
		else if (to_id.isNull())
		{
			// Message to everyone from GOD, look up the fullname since
			// server always slams name to legacy names
			LLAvatarNameCache::get(from_id, boost::bind(god_message_name_cb, _2, chat, message));
		}
		else
		{
			// standard message, not from system
			std::string saved;
			if(offline == IM_OFFLINE)
			{
				LLStringUtil::format_map_t args;
				args["[LONG_TIMESTAMP]"] = formatted_time(timestamp);
				saved = LLTrans::getString("Saved_message", args);
			}
			buffer = saved + message;

			LL_DEBUGS("Messaging") << "session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			bool mute_im = is_muted;
			if(accept_im_from_only_friend&&!is_friend)
			{
				if (!gIMMgr->isNonFriendSessionNotified(session_id))
				{
					// <FS:Ansariel> Disable this - doesn't make sense it will be skipped by LLIMMgr::addMessage() anyway
					//std::string message = LLTrans::getString("IM_unblock_only_groups_friends");
					//gIMMgr->addMessage(session_id, from_id, name, message, IM_OFFLINE == offline);
					// </FS:Ansariel>
					gIMMgr->addNotifiedNonFriendSessionID(session_id);
				}

				mute_im = true;
			}

// [RLVa:KB] - Checked: 2010-11-30 (RLVa-1.3.0)
			// Don't block offline IMs, or IMs from Lindens
			if ( (rlv_handler_t::isEnabled()) && (offline != IM_OFFLINE) && (!RlvActions::canReceiveIM(from_id)) && (!LLMuteList::getInstance()->isLinden(original_name) ))
			{
				if (!mute_im)
					RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM_REMOTE), session_id);
				message = RlvStrings::getString(RLV_STRING_BLOCKED_RECVIM);
			}
// [/RLVa:KB]

			if (!mute_im) 
			{
				// checkfor and process reqinfo
				message = FSData::getInstance()->processRequestForInfo(from_id, message, name, session_id);

				// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
				chat.mText = message;
				bool keyword_alert_performed = false;
				if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
				{
					FSKeywords::notify(chat);
					keyword_alert_performed = true;
				}
				// </FS:PP>

				buffer = saved + message;

				gIMMgr->addMessage(
					session_id,
					from_id,
					name,
					buffer,
					IM_OFFLINE == offline,
					LLStringUtil::null,
					dialog,
					parent_estate_id,
					region_id,
					position,
					true,
					false,
					keyword_alert_performed);
			}
			else
			{
				/*
				EXT-5099
				currently there is no way to store in history only...
				using  LLNotificationsUtil::add will add message to Nearby Chat

				// muted user, so don't start an IM session, just record line in chat
				// history.  Pretend the chat is from a local agent,
				// so it will go into the history but not be shown on screen.

				LLSD args;
				args["MESSAGE"] = buffer;
				LLNotificationsUtil::add("SystemMessageTip", args);
				*/
				static LLCachedControl<bool> fsSendMutedAvatarResponse(gSavedPerAccountSettings, "FSSendMutedAvatarResponse");
				if (fsSendMutedAvatarResponse && (!accept_im_from_only_friend || is_friend))
				{
					std::string my_name;
					LLAgentUI::buildFullname(my_name);
					std::string response = gSavedPerAccountSettings.getString("FSMutedAvatarResponse");
					pack_instant_message(
						gMessageSystem,
						gAgent.getID(),
						FALSE,
						gAgent.getSessionID(),
						from_id,
						my_name,
						response,
						IM_ONLINE,
						IM_DO_NOT_DISTURB_AUTO_RESPONSE,
						session_id);
					gAgent.sendReliableMessage();
				}
			}
		}
		break;

	case IM_TYPING_START:
		{
			LLPointer<LLIMInfo> im_info = new LLIMInfo(gMessageSystem);
			gIMMgr->processIMTypingStart(im_info);
		}
		break;

	case IM_TYPING_STOP:
		{
			LLPointer<LLIMInfo> im_info = new LLIMInfo(gMessageSystem);
			gIMMgr->processIMTypingStop(im_info);
		}
		break;

	case IM_MESSAGEBOX:
		{
			// This is a block, modeless dialog.
			//*TODO: Translate
			args["MESSAGE"] = message;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}
		break;
	case IM_GROUP_NOTICE:
	case IM_GROUP_NOTICE_REQUESTED:
		{
			LL_INFOS("Messaging") << "Received IM_GROUP_NOTICE message." << LL_ENDL;
			// Read the binary bucket for more information.
			struct notice_bucket_header_t
			{
				U8 has_inventory;
				U8 asset_type;
				LLUUID group_id;
			};
			struct notice_bucket_full_t
			{
				struct notice_bucket_header_t header;
				U8 item_name[DB_INV_ITEM_NAME_BUF_SIZE];
			}* notice_bin_bucket;

			// Make sure the binary bucket is big enough to hold the header 
			// and a null terminated item name.
			if ( (binary_bucket_size < (S32)((sizeof(notice_bucket_header_t) + sizeof(U8))))
				|| (binary_bucket[binary_bucket_size - 1] != '\0') )
			{
				LL_WARNS("Messaging") << "Malformed group notice binary bucket" << LL_ENDL;
				break;
			}

			// The group notice packet does not have an AgentID.  Obtain one from the name cache.
			// If last name is "Resident" strip it out so the cache name lookup works.
			size_t index = original_name.find(" Resident");
			if (index != std::string::npos)
			{
				original_name = original_name.substr(0, index);
			}
			std::string legacy_name = gCacheName->buildLegacyName(original_name);
			LLUUID agent_id;
			gCacheName->getUUID(legacy_name, agent_id);

			if (agent_id.isNull())
			{
				LL_WARNS("Messaging") << "buildLegacyName returned null while processing " << original_name << LL_ENDL;
			}
			else if (LLMuteList::getInstance()->isMuted(agent_id))
			{
				break;
			}

			notice_bin_bucket = (struct notice_bucket_full_t*) &binary_bucket[0];
			U8 has_inventory = notice_bin_bucket->header.has_inventory;
			U8 asset_type = notice_bin_bucket->header.asset_type;
			LLUUID group_id = notice_bin_bucket->header.group_id;
			std::string item_name = ll_safe_string((const char*) notice_bin_bucket->item_name);

			// If there is inventory, give the user the inventory offer.
			LLOfferInfo* info = NULL;

			if (has_inventory)
			{
				info = new LLOfferInfo();
				
				info->mIM = IM_GROUP_NOTICE;
				info->mFromID = from_id;
				info->mFromGroup = from_group;
				info->mTransactionID = session_id;
				info->mType = (LLAssetType::EType) asset_type;
				info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));
				std::string from_name;

				// <FS:Ansariel> FIRE-17714: Make group notice attachment confirmation localizable
				//from_name += "A group member named ";
				//from_name += name;
				LLStringUtil::format_map_t args;
				args["NAME"] = name;
				from_name += LLTrans::getString("InvOfferGroupNoticeName", args);
				// </FS:Ansariel>

				info->mFromName = from_name;
				info->mDesc = item_name;
				info->mHost = msg->getSender();
			}
			
			std::string str(message);

			// Tokenize the string.
			// TODO: Support escaped tokens ("||" -> "|")
			typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
			boost::char_separator<char> sep("|","",boost::keep_empty_tokens);
			tokenizer tokens(str, sep);
			tokenizer::iterator iter = tokens.begin();

			std::string subj(*iter++);
			std::string mes(*iter++);

			// Send the notification down the new path.
			// For requested notices, we don't want to send the popups.
			if (dialog != IM_GROUP_NOTICE_REQUESTED)
			{
				payload["subject"] = subj;
				payload["message"] = mes;
				payload["sender_name"] = name;
				payload["group_id"] = group_id;
				payload["inventory_name"] = item_name;
 				payload["received_time"] = LLDate::now();
				if(info && info->asLLSD())
				{
					payload["inventory_offer"] = info->asLLSD();
				}

				// <FS:Ansariel> FIRE-505: Group name not shown in notification well
				//LLSD args;
				//args["SUBJECT"] = subj;
				//LLDate notice_date = LLDate(timestamp).notNull() ? LLDate(timestamp) : LLDate::now();
				//LLNotifications::instance().add(LLNotification::Params("GroupNotice").substitutions(args).payload(payload).time_stamp(notice_date));
				//make_ui_sound("UISndGroupNotice"); // <FS:PP> Group notice sound
				if (group_id.isNull())
				{
					LL_WARNS() << "Received group notice with null id!" << LL_ENDL;
				}
				gCacheName->get(group_id, true, boost::bind(&notification_group_name_cb, _2, name, subj, mes, payload, timestamp));
				// </FS:Ansariel>
			}

			// Also send down the old path for now.
			if (IM_GROUP_NOTICE_REQUESTED == dialog)
			{
				
				LLPanelGroup::showNotice(subj,mes,group_id,has_inventory,item_name,info);
			}
			else
			{
				delete info;
			}
		}
		break;
	case IM_GROUP_INVITATION:
		{
			if (!is_muted)
			{
				// group is not blocked, but we still need to check agent that sent the invitation
				// and we have no agent's id
				// Note: server sends username "first.last".
				is_muted |= LLMuteList::getInstance()->isMuted(name);
			}
			if (is_do_not_disturb || is_muted)
			{
				send_do_not_disturb_message(msg, from_id);
			}
			
			if (!is_muted)
			{
				LL_INFOS("Messaging") << "Received IM_GROUP_INVITATION message." << LL_ENDL;
				// Read the binary bucket for more information.
				struct invite_bucket_t
				{
					S32 membership_fee;
					LLUUID role_id;
				}* invite_bucket;

				// Make sure the binary bucket is the correct size.
				if (binary_bucket_size != sizeof(invite_bucket_t))
				{
					LL_WARNS("Messaging") << "Malformed group invite binary bucket" << LL_ENDL;
					break;
				}

				invite_bucket = (struct invite_bucket_t*) &binary_bucket[0];
				S32 membership_fee = ntohl(invite_bucket->membership_fee);

				LLSD payload;
				payload["transaction_id"] = session_id;
				payload["group_id"] = from_id;
				payload["name"] = name;
				payload["message"] = message;
				payload["fee"] = membership_fee;

				LLSD args;
				args["MESSAGE"] = message;
				// we shouldn't pass callback functor since it is registered in LLFunctorRegistration

				// <FS:PP> Option to block/reject all group invites
				// LLNotificationsUtil::add("JoinGroup", args, payload);
				if (is_rejecting_group_invites)
				{
					LL_INFOS("Messaging") << "Group invite automatically rejected because of the user setting..." << LL_ENDL;
				}
				else
				{
					make_ui_sound("UISndGroupInvitation"); // <FS:PP> Group invitation sound
					LLNotificationsUtil::add("JoinGroup", args, payload);
				}
				// </FS:PP>

			}
		}
		break;

	case IM_INVENTORY_OFFERED:
	case IM_TASK_INVENTORY_OFFERED:
		// Someone has offered us some inventory.
		{
			LLOfferInfo* info = new LLOfferInfo;
			if (IM_INVENTORY_OFFERED == dialog)
			{
				struct offer_agent_bucket_t
				{
					S8		asset_type;
					LLUUID	object_id;
				}* bucketp;

				if (sizeof(offer_agent_bucket_t) != binary_bucket_size)
				{
					LL_WARNS("Messaging") << "Malformed inventory offer from agent" << LL_ENDL;
					delete info;
					break;
				}
				bucketp = (struct offer_agent_bucket_t*) &binary_bucket[0];
				info->mType = (LLAssetType::EType) bucketp->asset_type;
				info->mObjectID = bucketp->object_id;
				info->mFromObject = FALSE;
			}
			else // IM_TASK_INVENTORY_OFFERED
			{
				if (sizeof(S8) != binary_bucket_size)
				{
					LL_WARNS("Messaging") << "Malformed inventory offer from object" << LL_ENDL;
					delete info;
					break;
				}
				info->mType = (LLAssetType::EType) binary_bucket[0];
				info->mObjectID = LLUUID::null;
				info->mFromObject = TRUE;
			}

			info->mIM = dialog;
			info->mFromID = from_id;
			info->mFromGroup = from_group;
			info->mTransactionID = session_id;
			info->mFolderID = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(info->mType));

			info->mFromName = name;
			info->mDesc = message;
			info->mHost = msg->getSender();
			//if (((is_do_not_disturb && !is_owned_by_me) || is_muted))
			if (is_muted)
			{
				// Prefetch the offered item so that it can be discarded by the appropriate observer. (EXT-4331)
				LLInventoryFetchItemsObserver* fetch_item = new LLInventoryFetchItemsObserver(info->mObjectID);
				fetch_item->startFetch();
				delete fetch_item;

				// Same as closing window
				info->forceResponse(IOR_DECLINE);
			}
			// old logic: busy mode must not affect interaction with objects (STORM-565)
			// new logic: inventory offers from in-world objects should be auto-declined (CHUI-519)
			else if (is_do_not_disturb && dialog == IM_TASK_INVENTORY_OFFERED)
			{
				// Until throttling is implemented, do not disturb mode should reject inventory instead of silently
				// accepting it.  SEE SL-39554
				info->forceResponse(IOR_DECLINE);
			}
			else
			{
				inventory_offer_handler(info);
			}
		}
		break;

	case IM_INVENTORY_ACCEPTED:
	{
//		args["NAME"] = LLSLURL("agent", from_id, "completename").getSLURLString();;
// [RLVa:KB] - Checked: RLVa-1.2.2
		// Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
		bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
			(RlvActions::canShowName(RlvActions::SNC_DEFAULT, from_id)) || (!RlvUtil::isNearbyAgent(from_id)) || (RlvUIEnabler::hasOpenProfile(from_id)) || (RlvUIEnabler::hasOpenIM(from_id));
		args["NAME"] = LLSLURL("agent", from_id, (fRlvCanShowName) ? "completename" : "rlvanonym").getSLURLString();;
// [/RLVa:KB]
		LLSD payload;
		payload["from_id"] = from_id;
		// Passing the "SESSION_NAME" to use it for IM notification logging
		// in LLTipHandler::processNotification(). See STORM-941.
		payload["SESSION_NAME"] = name;
		LLNotificationsUtil::add("InventoryAccepted", args, payload);
		break;
	}
	case IM_INVENTORY_DECLINED:
	{
//		args["NAME"] = LLSLURL("agent", from_id, "completename").getSLURLString();;
// [RLVa:KB] - Checked: RLVa-1.2.2
		// Only anonymize the name if the agent is nearby, there isn't an open IM session to them and their profile isn't open
		bool fRlvCanShowName = (!RlvActions::isRlvEnabled()) ||
			(RlvActions::canShowName(RlvActions::SNC_DEFAULT, from_id)) || (!RlvUtil::isNearbyAgent(from_id)) || (RlvUIEnabler::hasOpenProfile(from_id)) || (RlvUIEnabler::hasOpenIM(from_id));
		args["NAME"] = LLSLURL("agent", from_id, (fRlvCanShowName) ? "completename" : "rlvanonym").getSLURLString();;
// [/RLVa:KB]
		LLSD payload;
		payload["from_id"] = from_id;
		LLNotificationsUtil::add("InventoryDeclined", args, payload);
		break;
	}
	// TODO: _DEPRECATED suffix as part of vote removal - DEV-24856
	case IM_GROUP_VOTE:
		{
			LL_WARNS("Messaging") << "Received IM: IM_GROUP_VOTE_DEPRECATED" << LL_ENDL;
		}
		break;

	case IM_GROUP_ELECTION_DEPRECATED:
	{
		LL_WARNS("Messaging") << "Received IM: IM_GROUP_ELECTION_DEPRECATED" << LL_ENDL;
	}
	break;
	
	case IM_FROM_TASK:
		{

			if (is_do_not_disturb && !is_owned_by_me)
			{
				return;
			}

			// <FS:PP> FIRE-6406: Feature to disable Object Return notification
			static LLCachedControl<bool> FSDisableReturnObjectNotification(gSavedSettings, "FSDisableReturnObjectNotification");
			if (FSDisableReturnObjectNotification)
			{
				if (message.find("been returned to your inventory") != -1)
				{
					return;
				}
			}
			// </FS:PP>

			// Build a link to open the object IM info window.
			std::string location = ll_safe_string((char*)binary_bucket, binary_bucket_size-1);

			if (session_id.notNull())
			{
				chat.mFromID = session_id;
			}
			else
			{
				// This message originated on a region without the updated code for task id and slurl information.
				// We just need a unique ID for this object that isn't the owner ID.
				// If it is the owner ID it will overwrite the style that contains the link to that owner's profile.
				// This isn't ideal - it will make 1 style for all objects owned by the the same person/group.
				// This works because the only thing we can really do in this case is show the owner name and link to their profile.
				chat.mFromID = from_id ^ gAgent.getSessionID();
			}

			chat.mSourceType = CHAT_SOURCE_OBJECT;
			chat.mChatType = CHAT_TYPE_IM;

			// To conclude that the source type of message is CHAT_SOURCE_SYSTEM it's not
			// enough to check only from name (i.e. fromName = "Second Life"). For example
			// source type of messages from objects called "Second Life" should not be CHAT_SOURCE_SYSTEM.
			bool chat_from_system = (SYSTEM_FROM == name) && region_id.isNull() && position.isNull();
			if(chat_from_system)
			{
				// System's UUID is NULL (fixes EXT-4766)
				chat.mFromID = LLUUID::null;
				chat.mSourceType = CHAT_SOURCE_SYSTEM;
			}

			// IDEVO Some messages have embedded resident names
			message = clean_name_from_task_im(message, from_group);

			LLSD query_string;
			query_string["owner"] = from_id;
// [RLVa:KB] - Checked: RLVa-1.2.0
			if (RlvActions::isRlvEnabled())
			{
				// NOTE: the chat message itself will be filtered in LLNearbyChatHandler::processChat()
				if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) && (!from_group) && (RlvUtil::isNearbyAgent(from_id)) )
				{
					query_string["rlv_shownames"] = TRUE;

					RlvUtil::filterNames(name);
					chat.mFromName = name;
				}
				if (!RlvActions::canShowLocation())
				{
					std::string::size_type idxPos = location.find('/');
					if ( (std::string::npos != idxPos) && (RlvUtil::isNearbyRegion(location.substr(0, idxPos))) )
						location = RlvStrings::getString(RLV_STRING_HIDDEN_REGION);
				}
			}
// [/RLVa:KB]
			query_string["slurl"] = location;
			query_string["name"] = name;
			if (from_group)
			{
				query_string["groupowned"] = "true";
			}	

//			chat.mURL = LLSLURL("objectim", session_id, "").getSLURLString();
// [SL:KB] - Checked: 2010-11-02 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
			chat.mURL = LLSLURL("objectim", session_id, LLURI::mapToQueryString(query_string)).getSLURLString();
// [/SL:KB]
			chat.mText = message;

			// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of Task IM)
			if (FSKeywords::getInstance()->chatContainsKeyword(chat, true))
			{
				FSKeywords::notify(chat);
			}
			// </FS:PP>

			// Note: lie to Nearby Chat, pretending that this is NOT an IM, because
			// IMs from obejcts don't open IM sessions.
			// <FS:Ansariel> [FS communication UI]
			//LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
			FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
			// </FS:Ansariel> [FS communication UI]
			if(!chat_from_system && nearby_chat)
			{
				chat.mOwnerID = from_id;
				LLSD args;
				args["slurl"] = location;

				// Look for IRC-style emotes here so object name formatting is correct
				// <FS:Ansariel> Consolidate IRC /me prefix checks
				//std::string prefix = message.substr(0, 4);
				//if (prefix == "/me " || prefix == "/me'")
				if (is_irc_me_prefix(message))
				// </FS:Ansariel>
				{
					chat.mChatStyle = CHAT_STYLE_IRC;
				}

				LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
			}


			//Object IMs send with from name: 'Second Life' need to be displayed also in notification toasts (EXT-1590)
			if (!chat_from_system) break;
			
			LLSD substitutions;
			substitutions["NAME"] = name;
			substitutions["MSG"] = message;

			LLSD payload;
			payload["object_id"] = session_id;
			payload["owner_id"] = from_id;
			payload["from_id"] = from_id;
			payload["slurl"] = location;
			payload["name"] = name;

			if (from_group)
			{
				payload["group_owned"] = "true";
			}

			LLNotificationsUtil::add("ServerObjectMessage", substitutions, payload);
		}
		break;

	case IM_SESSION_SEND:		// ad-hoc or group IMs

		// Only show messages if we have a session open (which
		// should happen after you get an "invitation"
// [SL:KB] - Patch: Chat-GroupSnooze | Checked: 2012-06-16 (Catznip-3.3)
		//if ( !gIMMgr->hasSession(session_id) )
		if ( (!gIMMgr->hasSession(session_id)) &&
			 ( (!gAgent.isInGroup(session_id)) || (!gIMMgr->checkSnoozeExpiration(session_id)) || LLAvatarActions::isBlocked(from_id) || (!gIMMgr->restoreSnoozedSession(session_id)) ) )
// [/SL:KB]
		{
			return;
		}

		else if (offline == IM_ONLINE && is_do_not_disturb)
		{

			// return a standard "do not disturb" message, but only do it to online IM 
			// (i.e. not other auto responses and not store-and-forward IM)
			if (!gIMMgr->hasSession(session_id))
			{
				// if there is not a panel for this conversation (i.e. it is a new IM conversation
				// initiated by the other party) then...
				send_do_not_disturb_message(msg, from_id, session_id);
			}

			// now store incoming IM in chat history

			buffer = message;
	
			LL_DEBUGS("Messaging") << "message in dnd; session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			// add to IM panel, but do not bother the user
			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				IM_OFFLINE == offline,
				ll_safe_string((char*)binary_bucket),
				IM_SESSION_INVITE,
				parent_estate_id,
				region_id,
				position,
				true);
		}
		else
		{

			// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of IM)
			chat.mText = message;
			bool keyword_alert_performed = false;
			if (FSKeywords::getInstance()->chatContainsKeyword(chat, false))
			{
				FSKeywords::notify(chat);
				keyword_alert_performed = true;
			}
			// </FS:PP>

			// standard message, not from system
			std::string saved;
			if(offline == IM_OFFLINE)
			{
				saved = llformat("(Saved %s) ", formatted_time(timestamp).c_str());
			}

			buffer = saved + message;

			LL_DEBUGS("Messaging") << "standard message session_id( " << session_id << " ), from_id( " << from_id << " )" << LL_ENDL;

			gIMMgr->addMessage(
				session_id,
				from_id,
				name,
				buffer,
				IM_OFFLINE == offline,
				ll_safe_string((char*)binary_bucket),
				IM_SESSION_INVITE,
				parent_estate_id,
				region_id,
				position,
				true,
				false,
				keyword_alert_performed);
		}
		break;

	case IM_FROM_TASK_AS_ALERT:
		if (is_do_not_disturb && !is_owned_by_me)
		{
			return;
		}
		{
			// Construct a viewer alert for this message.
			args["NAME"] = name;
			args["MESSAGE"] = message;
			LLNotificationsUtil::add("ObjectMessage", args);
		}
		break;
	case IM_DO_NOT_DISTURB_AUTO_RESPONSE:
		if (is_muted)
		{
			LL_DEBUGS("Messaging") << "Ignoring do-not-disturb response from " << from_id << LL_ENDL;
			return;
		}
		else
		{
			// <FS:Ansariel> FIRE-12908: Add busy response indicator back to busy messages
			//gIMMgr->addMessage(session_id, from_id, name, message);
			buffer = llformat("(%s): %s", LLTrans::getString("BusyResponse").c_str(), message.c_str());
			gIMMgr->addMessage(session_id, from_id, name, buffer);
			// </FS:Ansariel>
		}
		break;
		
	case IM_LURE_USER:
	case IM_TELEPORT_REQUEST:
		{
// [RLVa:KB] - Checked: RLVa-1.4.9
			// If we auto-accept the offer/request then this will override DnD status (but we'll still let the other party know later)
			bool fRlvAutoAccept = (rlv_handler_t::isEnabled()) &&
				( ((IM_LURE_USER == dialog) && (RlvActions::autoAcceptTeleportOffer(from_id))) ||
				  ((IM_TELEPORT_REQUEST == dialog) && (RlvActions::autoAcceptTeleportRequest(from_id))) );
// [/RLVa:KB]

			if (is_muted)
			{ 
				return;
			}
//			else if (is_do_not_disturb) 
// [RLVa:KB] - Checked: RLVa-1.4.9
			else if ( (is_do_not_disturb) && (!fRlvAutoAccept) )
// [/RLVa:KB]
			{
				send_do_not_disturb_message(msg, from_id);
			}
			// <FS:PP> FIRE-1245: Option to block/reject teleport offers
			//else if (gSavedSettings.getBOOL("VoiceCallsFriendsOnly") && (LLAvatarTracker::instance().getBuddyInfo(from_id) == NULL))
			//{
			//	return;
			//}
			else if ( (is_rejecting_tp_offers && (!FSDontRejectTeleportOffersFromFriends || (FSDontRejectTeleportOffersFromFriends && !is_friend))) && (!fRlvAutoAccept) )
			{
				send_rejecting_tp_offers_message(msg, from_id);
			}
			// </FS:PP>
			else
			{
				LLVector3 pos, look_at;
				U64 region_handle(0);
				U8 region_access(SIM_ACCESS_MIN);
				std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
				std::string region_access_str = LLStringUtil::null;
				std::string region_access_icn = LLStringUtil::null;
				std::string region_access_lc  = LLStringUtil::null;

				bool canUserAccessDstRegion = true;
				bool doesUserRequireMaturityIncrease = false;

				// Do not parse the (empty) lure bucket for TELEPORT_REQUEST
				if (IM_TELEPORT_REQUEST != dialog && parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
				{
					region_access_str = LLViewerRegion::accessToString(region_access);
					region_access_icn = LLViewerRegion::getAccessIcon(region_access);
					region_access_lc  = region_access_str;
					LLStringUtil::toLower(region_access_lc);

					if (!gAgent.isGodlike())
					{
						switch (region_access)
						{
						case SIM_ACCESS_MIN :
						case SIM_ACCESS_PG :
							break;
						case SIM_ACCESS_MATURE :
							if (gAgent.isTeen())
							{
								canUserAccessDstRegion = false;
							}
							else if (gAgent.prefersPG())
							{
								doesUserRequireMaturityIncrease = true;
							}
							break;
						case SIM_ACCESS_ADULT :
							if (!gAgent.isAdult())
							{
								canUserAccessDstRegion = false;
							}
							else if (!gAgent.prefersAdult())
							{
								doesUserRequireMaturityIncrease = true;
							}
							break;
						default :
							llassert(0);
							break;
						}
					}
				}

// [RLVa:KB] - Checked: RLVa-1.4.9
				if (rlv_handler_t::isEnabled())
				{
					if ( ((IM_LURE_USER == dialog) && (!RlvActions::canAcceptTpOffer(from_id))) ||
					     ((IM_TELEPORT_REQUEST == dialog) && (!RlvActions::canAcceptTpRequest(from_id))) )
					{
						RlvUtil::sendBusyMessage(from_id, RlvStrings::getString(RLV_STRING_BLOCKED_TPLUREREQ_REMOTE));
						if (is_do_not_disturb)
							send_do_not_disturb_message(msg, from_id);
						return;
					}

					// Censor message if: 1) restricted from receiving IMs from the sender, or 2) teleport offer/request and @showloc=n restricted
					if ( (!RlvActions::canReceiveIM(from_id)) || 
						 ((gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) && (IM_LURE_USER == dialog || IM_TELEPORT_REQUEST == dialog)) )
					{
						message = RlvStrings::getString(RLV_STRING_HIDDEN);
					}
				}
// [/RLVa:KB]

				LLSD args;
				// *TODO: Translate -> [FIRST] [LAST] (maybe)
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
				args["NAME_LABEL"] = LLSLURL("agent", from_id, "completename").getSLURLString();
// [/SL:KB]
				args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();
				args["MESSAGE"] = message;
				args["MATURITY_STR"] = region_access_str;
				args["MATURITY_ICON"] = region_access_icn;
				args["REGION_CONTENT_MATURITY"] = region_access_lc;
				LLSD payload;
				payload["from_id"] = from_id;
				payload["lure_id"] = session_id;
				payload["godlike"] = FALSE;
				payload["region_maturity"] = region_access;

				// <FS:Ansariel> FIRE-6786: Always show teleport location in teleport offer
				if (dialog == IM_LURE_USER && (!rlv_handler_t::isEnabled() || !fRlvAutoAccept) && LLGridManager::instance().isInSecondLife())
				{
					LLVector3d pos_global = from_region_handle(region_handle);
					pos_global += LLVector3d(pos);
					LLLandmarkActions::getSLURLfromPosGlobal(pos_global, boost::bind(&teleport_region_info_cb, _1, args, payload, from_id, session_id, canUserAccessDstRegion, doesUserRequireMaturityIncrease));
					return;
				}
				// </FS:Ansariel>

				if (!canUserAccessDstRegion)
				{
					LLNotification::Params params("TeleportOffered_MaturityBlocked");
					params.substitutions = args;
					params.payload = payload;
					LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
					send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
					send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
				}
				else if (doesUserRequireMaturityIncrease)
				{
					LLNotification::Params params("TeleportOffered_MaturityExceeded");
					params.substitutions = args;
					params.payload = payload;
					LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
				}
				else
				{
					LLNotification::Params params;
					if (IM_LURE_USER == dialog)
					{
						params.name = "TeleportOffered";
						params.functor.name = "TeleportOffered";
					}
					else if (IM_TELEPORT_REQUEST == dialog)
					{
						params.name = "TeleportRequest";
						params.functor.name = "TeleportRequest";
					}

					params.substitutions = args;
					params.payload = payload;

// [RLVa:KB] - Checked: RLVa-1.4.9
					if (fRlvAutoAccept)
					{
						if (IM_LURE_USER == dialog)
							gRlvHandler.setCanCancelTp(false);
						if (is_do_not_disturb)
							send_do_not_disturb_message(msg, from_id);
						LLNotifications::instance().forceResponse(LLNotification::Params(params.name).payload(payload), 0);
					}
					else
					{
						LLPostponedNotification::add<LLPostponedOfferNotification>(params, from_id, false);
					}
// [/RLVa:KB]
//					LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
				}
			}
		}
		break;

	case IM_GODLIKE_LURE_USER:
		{
			LLVector3 pos, look_at;
			U64 region_handle(0);
			U8 region_access(SIM_ACCESS_MIN);
			std::string region_info = ll_safe_string((char*)binary_bucket, binary_bucket_size);
			std::string region_access_str = LLStringUtil::null;
			std::string region_access_icn = LLStringUtil::null;
			std::string region_access_lc  = LLStringUtil::null;

			bool canUserAccessDstRegion = true;
			bool doesUserRequireMaturityIncrease = false;

			if (parse_lure_bucket(region_info, region_handle, pos, look_at, region_access))
			{
				region_access_str = LLViewerRegion::accessToString(region_access);
				region_access_icn = LLViewerRegion::getAccessIcon(region_access);
				region_access_lc  = region_access_str;
				LLStringUtil::toLower(region_access_lc);

				if (!gAgent.isGodlike())
				{
					switch (region_access)
					{
					case SIM_ACCESS_MIN :
					case SIM_ACCESS_PG :
						break;
					case SIM_ACCESS_MATURE :
						if (gAgent.isTeen())
						{
							canUserAccessDstRegion = false;
						}
						else if (gAgent.prefersPG())
						{
							doesUserRequireMaturityIncrease = true;
						}
						break;
					case SIM_ACCESS_ADULT :
						if (!gAgent.isAdult())
						{
							canUserAccessDstRegion = false;
						}
						else if (!gAgent.prefersAdult())
						{
							doesUserRequireMaturityIncrease = true;
						}
						break;
					default :
						llassert(0);
						break;
					}
				}
			}

			LLSD args;
			// *TODO: Translate -> [FIRST] [LAST] (maybe)
			args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();
			args["MESSAGE"] = message;
			args["MATURITY_STR"] = region_access_str;
			args["MATURITY_ICON"] = region_access_icn;
			args["REGION_CONTENT_MATURITY"] = region_access_lc;
			LLSD payload;
			payload["from_id"] = from_id;
			payload["lure_id"] = session_id;
			payload["godlike"] = TRUE;
			payload["region_maturity"] = region_access;

			if (!canUserAccessDstRegion)
			{
				LLNotification::Params params("TeleportOffered_MaturityBlocked");
				params.substitutions = args;
				params.payload = payload;
				LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
				send_simple_im(from_id, LLTrans::getString("TeleportMaturityExceeded"), IM_NOTHING_SPECIAL, session_id);
				send_simple_im(from_id, LLStringUtil::null, IM_LURE_DECLINED, session_id);
			}
			else if (doesUserRequireMaturityIncrease)
			{
				LLNotification::Params params("TeleportOffered_MaturityExceeded");
				params.substitutions = args;
				params.payload = payload;
				LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
			}
			else
			{
			// do not show a message box, because you're about to be
			// teleported.
			LLNotifications::instance().forceResponse(LLNotification::Params("TeleportOffered").payload(payload), 0);
		}
		}
		break;

	case IM_GOTO_URL:
		{
			LLSD args;
			// n.b. this is for URLs sent by the system, not for
			// URLs sent by scripts (i.e. llLoadURL)
			if (binary_bucket_size <= 0)
			{
				LL_WARNS("Messaging") << "bad binary_bucket_size: "
					<< binary_bucket_size
					<< " - aborting function." << LL_ENDL;
				return;
			}

			std::string url;
			
			url.assign((char*)binary_bucket, binary_bucket_size-1);
			args["MESSAGE"] = message;
			args["URL"] = url;
			LLSD payload;
			payload["url"] = url;
			LLNotificationsUtil::add("GotoURL", args, payload );
		}
		break;

	case IM_FRIENDSHIP_OFFERED:
		{

			// <FS:PP> FIRE-15233: Automatic friendship request refusal
			if (is_rejecting_friendship_requests)
			{
				send_rejecting_friendship_requests_message(msg, from_id);
				return;
			}
			// </FS:PP>

			LLSD payload;
			payload["from_id"] = from_id;
			payload["session_id"] = session_id;
			payload["online"] = (offline == IM_ONLINE);
			payload["sender"] = msg->getSender().getIPandPort();

			bool add_notification = true;
			for (LLToastNotifyPanel::instance_iter ti(LLToastNotifyPanel::beginInstances())
				, tend(LLToastNotifyPanel::endInstances()); ti != tend; ++ti)
			{
				LLToastNotifyPanel& panel = *ti;
				const std::string& notification_name = panel.getNotificationName();
				if (notification_name == "OfferFriendship" && panel.isControlPanelEnabled())
				{
					add_notification = false;
					break;
				}
			}

			if (is_muted && add_notification)
			{
				LLNotifications::instance().forceResponse(LLNotification::Params("OfferFriendship").payload(payload), 1);
			}
			else
			{
				if (is_do_not_disturb)
				{
					send_do_not_disturb_message(msg, from_id);
				}
// [SL:KB] - Patch: UI-Notifications | Checked: 2011-04-11 (Catznip-2.5.0a) | Added: Catznip-2.5.0a
				args["NAME_LABEL"] = LLSLURL("agent", from_id, "completename").getSLURLString();
// [/SL:KB]
				args["NAME_SLURL"] = LLSLURL("agent", from_id, "about").getSLURLString();

				if (add_notification)
				{
				if(message.empty())
				{
					//support for frienship offers from clients before July 2008
				        LLNotificationsUtil::add("OfferFriendshipNoMessage", args, payload);
				        make_ui_sound("UISndFriendshipOffer"); // <FS:PP> Friendship offer sound
				}
				else
				{
					args["[MESSAGE]"] = message;
				    LLNotification::Params params("OfferFriendship");
				    params.substitutions = args;
				    params.payload = payload;
				    LLPostponedNotification::add<LLPostponedOfferNotification>(	params, from_id, false);
				    make_ui_sound("UISndFriendshipOffer"); // <FS:PP> Friendship offer sound
				}
			}
		}
		}
		break;

	case IM_FRIENDSHIP_ACCEPTED:
		{
			// In the case of an offline IM, the formFriendship() may be extraneous
			// as the database should already include the relationship.  But it
			// doesn't hurt for dupes.
			LLAvatarTracker::formFriendship(from_id);
			
			std::vector<std::string> strings;
			strings.push_back(from_id.asString());
			send_generic_message("requestonlinenotification", strings);
			
			args["NAME"] = name;
			LLSD payload;
			payload["from_id"] = from_id;
			LLAvatarNameCache::get(from_id, boost::bind(&notification_display_name_callback,_1,_2,"FriendshipAccepted",args,payload));
		}
		break;

	case IM_FRIENDSHIP_DECLINED_DEPRECATED:
	default:
		LL_WARNS("Messaging") << "Instant message calling for unknown dialog "
				<< (S32)dialog << LL_ENDL;
		break;
	}

	LLWindow* viewer_window = gViewerWindow->getWindow();
	// <FS:CR> Make osx dashboard icon bounce when window isn't in focus
	//if (viewer_window && viewer_window->getMinimized())
	static LLCachedControl<bool> sFlashIcon(gSavedSettings, "FSFlashOnMessage");
	static LLCachedControl<bool> sFSFlashOnObjectIM(gSavedSettings, "FSFlashOnObjectIM");
	if (viewer_window && dialog != IM_TYPING_START && dialog != IM_TYPING_STOP && sFlashIcon && (sFSFlashOnObjectIM || (chat.mChatType != CHAT_TYPE_IM)))
	{
		viewer_window->flashIcon(5.f);
	}
}

void send_do_not_disturb_message (LLMessageSystem* msg, const LLUUID& from_id, const LLUUID& session_id)
{
	if (gAgent.isDoNotDisturb())
	{
		std::string my_name;
		LLAgentUI::buildFullname(my_name);
		std::string response = gSavedPerAccountSettings.getString("DoNotDisturbModeResponse");
		pack_instant_message(
			msg,
			gAgent.getID(),
			FALSE,
			gAgent.getSessionID(),
			from_id,
			my_name,
			response,
			IM_ONLINE,
			IM_DO_NOT_DISTURB_AUTO_RESPONSE,
			session_id);
		gAgent.sendReliableMessage();
	}
}

// <FS:PP> FIRE-1245: Option to block/reject teleport offers
void send_rejecting_tp_offers_message (LLMessageSystem* msg, const LLUUID& from_id, const LLUUID& session_id)
{
	std::string my_name;
	LLAgentUI::buildFullname(my_name);
	std::string response = gSavedPerAccountSettings.getString("FSRejectTeleportOffersResponse");
	pack_instant_message(
		msg,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		from_id,
		my_name,
		response,
		IM_ONLINE,
		IM_DO_NOT_DISTURB_AUTO_RESPONSE,
		session_id);
	gAgent.sendReliableMessage();
}
// </FS:PP> FIRE-1245: Option to block/reject teleport offers

// <FS:PP> FIRE-15233: Automatic friendship request refusal
void send_rejecting_friendship_requests_message (LLMessageSystem* msg, const LLUUID& from_id, const LLUUID& session_id)
{
	std::string my_name;
	LLAgentUI::buildFullname(my_name);
	std::string response = gSavedPerAccountSettings.getString("FSRejectFriendshipRequestsResponse");
	pack_instant_message(
		msg,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		from_id,
		my_name,
		response,
		IM_ONLINE,
		IM_DO_NOT_DISTURB_AUTO_RESPONSE,
		session_id);
	gAgent.sendReliableMessage();
}
// </FS:PP> FIRE-15233: Automatic friendship request refusal

bool callingcard_offer_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLUUID fid;
	LLUUID from_id;
	LLMessageSystem* msg = gMessageSystem;
	switch(option)
	{
	case 0:
		// accept
		msg->newMessageFast(_PREHASH_AcceptCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		fid = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		msg->nextBlockFast(_PREHASH_FolderData);
		msg->addUUIDFast(_PREHASH_FolderID, fid);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		break;
	case 1:
		// decline		
		msg->newMessageFast(_PREHASH_DeclineCallingCard);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->nextBlockFast(_PREHASH_TransactionBlock);
		msg->addUUIDFast(_PREHASH_TransactionID, notification["payload"]["transaction_id"].asUUID());
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
		send_do_not_disturb_message(msg, notification["payload"]["source_id"].asUUID());
		break;
	default:
		// close button probably, possibly timed out
		break;
	}

	return false;
}
static LLNotificationFunctorRegistration callingcard_offer_cb_reg("OfferCallingCard", callingcard_offer_callback);

void process_offer_callingcard(LLMessageSystem* msg, void**)
{
	// someone has offered to form a friendship
	LL_DEBUGS("Messaging") << "callingcard offer" << LL_ENDL;

	LLUUID source_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, source_id);
	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_CALLING_CARD, source_id, ANTISPAM_SOURCE_AGENT))
	{
		return;
	}
	// NaCl End
	LLUUID tid;
	msg->getUUIDFast(_PREHASH_AgentBlock, _PREHASH_TransactionID, tid);

	LLSD payload;
	payload["transaction_id"] = tid;
	payload["source_id"] = source_id;
	payload["sender"] = msg->getSender().getIPandPort();

	LLViewerObject* source = gObjectList.findObject(source_id);
	LLSD args;
	std::string source_name;
	if(source && source->isAvatar())
	{
		LLNameValue* nvfirst = source->getNVPair("FirstName");
		LLNameValue* nvlast  = source->getNVPair("LastName");
		if (nvfirst && nvlast)
		{
			source_name = LLCacheName::buildFullName(
				nvfirst->getString(), nvlast->getString());
		}
	}

	if(!source_name.empty())
	{
		if (gAgent.isDoNotDisturb() 
			|| LLMuteList::getInstance()->isMuted(source_id, source_name, LLMute::flagTextChat))
		{
			// automatically decline offer
			LLNotifications::instance().forceResponse(LLNotification::Params("OfferCallingCard").payload(payload), 1);
		}
		else
		{
			args["NAME"] = source_name;
			LLNotificationsUtil::add("OfferCallingCard", args, payload);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Calling card offer from an unknown source." << LL_ENDL;
	}
}

void process_accept_callingcard(LLMessageSystem* msg, void**)
{
	LLNotificationsUtil::add("CallingCardAccepted");
}

void process_decline_callingcard(LLMessageSystem* msg, void**)
{
	LLNotificationsUtil::add("CallingCardDeclined");
}

void translateSuccess(LLChat chat, LLSD toastArgs, std::string originalMsg, std::string expectLang, std::string translation, const std::string detected_language)
{
    // filter out non-interesting responses  
    if (!translation.empty()
        && ((detected_language.empty()) || (expectLang != detected_language))
        && (LLStringUtil::compareInsensitive(translation, originalMsg) != 0))
    {
        chat.mText += " (" + translation + ")";
    }

    LLNotificationsUI::LLNotificationManager::instance().onChat(chat, toastArgs);
}

void translateFailure(LLChat chat, LLSD toastArgs, int status, const std::string err_msg)
{
    std::string msg = LLTrans::getString("TranslationFailed", LLSD().with("[REASON]", err_msg));
    LLStringUtil::replaceString(msg, "\n", " "); // we want one-line error messages
    chat.mText += " (" + msg + ")";

    LLNotificationsUI::LLNotificationManager::instance().onChat(chat, toastArgs);
}


void process_chat_from_simulator(LLMessageSystem *msg, void **user_data)
{
	LLChat	chat;
	std::string		mesg;
	std::string		from_name;
	U8			source_temp;
	U8			type_temp;
	U8			audible_temp;
	LLColor4	color(1.0f, 1.0f, 1.0f, 1.0f);
	LLUUID		from_id;
	LLUUID		owner_id;
	LLViewerObject*	chatter;

	msg->getString("ChatData", "FromName", from_name);
	
	msg->getUUID("ChatData", "SourceID", from_id);
	chat.mFromID = from_id;
	
	// Object owner for objects
	msg->getUUID("ChatData", "OwnerID", owner_id);

	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_SourceType, source_temp);
	chat.mSourceType = (EChatSourceType)source_temp;

	msg->getU8("ChatData", "ChatType", type_temp);
	chat.mChatType = (EChatType)type_temp;

	// NaCL - Antispam Registry
	if (chat.mChatType != CHAT_TYPE_START && chat.mChatType != CHAT_TYPE_STOP)
	{
		// owner_id = from_id for agents
		if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_CHAT, owner_id, ANTISPAM_SOURCE_AGENT))
		{
			return;
		}
	}
	// NaCl End

	msg->getU8Fast(_PREHASH_ChatData, _PREHASH_Audible, audible_temp);
	chat.mAudible = (EChatAudible)audible_temp;
	
	chat.mTime = LLFrameTimer::getElapsedSeconds();
	
	// IDEVO Correct for new-style "Resident" names
	if (chat.mSourceType == CHAT_SOURCE_AGENT)
	{
		// I don't know if it's OK to change this here, if 
		// anything downstream does lookups by name, for instance
		
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(from_id, &av_name))
		{
			chat.mFromName = av_name.getDisplayName();
		}
		else
		{
			chat.mFromName = LLCacheName::cleanFullName(from_name);
		}
	}
	else
	{
		// make sure that we don't have an empty or all-whitespace name
		// <FS:KC> Objects with no name get renamed to NO_NAME_OBJECT so the object profile is still accessable
		//LLStringUtil::trim(from_name);
		//if (from_name.empty())
		//{
		//	from_name = LLTrans::getString("Unnamed");
		//}
		//chat.mFromName = from_name;
		static const boost::regex whitespace_exp("^\\s*$");
		if (chat.mSourceType == CHAT_SOURCE_OBJECT && boost::regex_search(from_name, whitespace_exp))
		{
			//[FIRE-2434 Mark Unamed Objects based on setting
			static LLCachedControl<bool> FSMarkObjects(gSavedSettings, "FSMarkObjects");
			if (FSMarkObjects)
			{
				chat.mFromName = LLTrans::getString("no_name_object");
			}
			else
			{
				chat.mFromName = "";
			}
		}
		else
		{
			chat.mFromName = from_name;
		}
		// </FS:KC>
	}

	BOOL is_do_not_disturb = gAgent.isDoNotDisturb();

	BOOL is_muted = FALSE;
	BOOL is_linden = FALSE;
	is_muted = LLMuteList::getInstance()->isMuted(
		from_id,
		from_name,
		LLMute::flagTextChat) 
		|| LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagTextChat);
	is_linden = chat.mSourceType != CHAT_SOURCE_OBJECT &&
		LLMuteList::getInstance()->isLinden(from_name);

	if (is_muted && (chat.mSourceType == CHAT_SOURCE_OBJECT))
	{
		return;
	}

	BOOL is_audible = (CHAT_AUDIBLE_FULLY == chat.mAudible);
	chatter = gObjectList.findObject(from_id);
	if (chatter)
	{
		chat.mPosAgent = chatter->getPositionAgent();
		static LLCachedControl<bool> EffectScriptChatParticles(gSavedSettings, "EffectScriptChatParticles"); // <FS:PP> gSavedSettings to LLCachedControl

		// Make swirly things only for talking objects. (not script debug messages, though)
//		if (chat.mSourceType == CHAT_SOURCE_OBJECT 
//			&& chat.mChatType != CHAT_TYPE_DEBUG_MSG
//			&& gSavedSettings.getBOOL("EffectScriptChatParticles") )
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0b) | Modified: RLVa-1.0.0g
		if ( ((chat.mSourceType == CHAT_SOURCE_OBJECT) && (chat.mChatType != CHAT_TYPE_DEBUG_MSG)) && 
			 // <FS:PP> gSavedSettings to LLCachedControl
			 // (gSavedSettings.getBOOL("EffectScriptChatParticles")) &&
			 (EffectScriptChatParticles) &&
			 // </FS:PP>
			 ((!rlv_handler_t::isEnabled()) || (CHAT_TYPE_OWNER != chat.mChatType)) )
// [/RLVa:KB]
		{
			LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
			psc->setSourceObject(chatter);
			psc->setColor(color);
			//We set the particles to be owned by the object's owner, 
			//just in case they should be muted by the mute list
			psc->setOwnerUUID(owner_id);
			LLViewerPartSim::getInstance()->addPartSource(psc);
		}

		// record last audible utterance
		if (is_audible
			&& (is_linden || (!is_muted && !is_do_not_disturb)))
		{
			if (chat.mChatType != CHAT_TYPE_START 
				&& chat.mChatType != CHAT_TYPE_STOP)
			{
				gAgent.heardChat(chat.mFromID);
			}
		}
	}

	if (is_audible)
	{
		//BOOL visible_in_chat_bubble = FALSE;
		std::string verb;

		color.setVec(1.f,1.f,1.f,1.f);
		msg->getStringFast(_PREHASH_ChatData, _PREHASH_Message, mesg);

		// NaCl - Newline flood protection
		static LLCachedControl<bool> useAntiSpam(gSavedSettings, "UseAntiSpam");
		if (useAntiSpam)
		{
			bool doCheck = true;
			if (owner_id.isNull() || gAgentID == owner_id)
			{
				doCheck = false;
			}
			if (doCheck && chatter && chatter->permYouOwner())
			{
				doCheck = false;
			}
			if (doCheck && NACLAntiSpamRegistry::instance().checkNewlineFlood(ANTISPAM_QUEUE_CHAT, owner_id, mesg))
			{
				return;
			}
		}
		// NaCl End

// [RLVa:KB] - Checked: 2010-04-23 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
		if ( (rlv_handler_t::isEnabled()) && (CHAT_TYPE_START != chat.mChatType) && (CHAT_TYPE_STOP != chat.mChatType) )
		{
			// NOTE: chatter can be NULL (may not have rezzed yet, or could be another avie's HUD attachment)
			BOOL is_attachment = (chatter) ? chatter->isAttachment() : FALSE;
			BOOL is_owned_by_me = (chatter) ? chatter->permYouOwner() : FALSE;

			// Filtering "rules":
			//   avatar  => filter all avie text (unless it's this avie or they're an exemption)
			//   objects => filter everything except attachments this avie owns (never filter llOwnerSay or llRegionSayTo chat)
			if ( ( (CHAT_SOURCE_AGENT == chat.mSourceType) && (from_id != gAgent.getID()) ) || 
				 ( (CHAT_SOURCE_OBJECT == chat.mSourceType) && ((!is_owned_by_me) || (!is_attachment)) && 
				   (CHAT_TYPE_OWNER != chat.mChatType) && (CHAT_TYPE_DIRECT != chat.mChatType) ) )
			{
				bool fIsEmote = RlvUtil::isEmote(mesg);
				static LLCachedControl<bool> RestrainedLoveShowEllipsis(gSavedSettings, "RestrainedLoveShowEllipsis"); // <FS:PP> gSavedSettings to LLCachedControl
				if ((!fIsEmote) &&
					(((gRlvHandler.hasBehaviour(RLV_BHVR_RECVCHAT)) && (!gRlvHandler.isException(RLV_BHVR_RECVCHAT, from_id))) ||
					 ((gRlvHandler.hasBehaviour(RLV_BHVR_RECVCHATFROM)) && (gRlvHandler.isException(RLV_BHVR_RECVCHATFROM, from_id))) ))
				{
					// <FS:PP> gSavedSettings to LLCachedControl
					// if ( (gRlvHandler.filterChat(mesg, false)) && (!gSavedSettings.getBOOL("RestrainedLoveShowEllipsis")) )
					if ( (gRlvHandler.filterChat(mesg, false)) && (!RestrainedLoveShowEllipsis) )
					// </FS:PP>
						return;
				}
				else if ((fIsEmote) &&
					     (((gRlvHandler.hasBehaviour(RLV_BHVR_RECVEMOTE)) && (!gRlvHandler.isException(RLV_BHVR_RECVEMOTE, from_id))) ||
					      ((gRlvHandler.hasBehaviour(RLV_BHVR_RECVEMOTEFROM)) && (gRlvHandler.isException(RLV_BHVR_RECVEMOTEFROM, from_id))) ))
 				{
					// <FS:PP> gSavedSettings to LLCachedControl
					// if (!gSavedSettings.getBOOL("RestrainedLoveShowEllipsis"))
					if (!RestrainedLoveShowEllipsis)
					// </FS:PP>
						return;
					mesg = "/me ...";
				}
			}

			// Filtering "rules":
			//   avatar => filter only their name (unless it's this avie)
			//   other  => filter everything
			if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT))
			{
				if (CHAT_SOURCE_AGENT != chat.mSourceType)
				{
					RlvUtil::filterNames(chat.mFromName);
				}
				else if (chat.mFromID != gAgent.getID())
				{
					chat.mFromName = RlvStrings::getAnonym(chat.mFromName);
					chat.mRlvNamesFiltered = TRUE;
				}
			}

			// Create an "objectim" URL for objects if we're either @shownames or @showloc restricted
			// (we need to do this now because we won't be have enough information to do it later on)
			if ( (CHAT_SOURCE_OBJECT == chat.mSourceType) &&
			     ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) || (!RlvActions::canShowLocation()) ) )
			{
				LLSD sdQuery;
				sdQuery["name"] = chat.mFromName;
				sdQuery["owner"] = owner_id;

				if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, owner_id)) && (!is_owned_by_me) )
					sdQuery["rlv_shownames"] = true;

				const LLViewerRegion* pRegion = LLWorld::getInstance()->getRegionFromPosAgent(chat.mPosAgent);
				if (pRegion)
					sdQuery["slurl"] = LLSLURL(pRegion->getName(), chat.mPosAgent).getLocationString();

				chat.mURL = LLSLURL("objectim", from_id, LLURI::mapToQueryString(sdQuery)).getSLURLString();
			}
		}
// [/RLVa:KB]

		BOOL ircstyle = FALSE;

		// Look for IRC-style emotes here so chatbubbles work
		// <FS:Ansariel> Consolidate IRC /me prefix checks
		//std::string prefix = mesg.substr(0, 4);
		//if (prefix == "/me " || prefix == "/me'")
		if (is_irc_me_prefix(mesg))
		// </FS:Ansariel>
		{
			ircstyle = TRUE;
		}
		chat.mText = mesg;

		// Look for the start of typing so we can put "..." in the bubbles.
		if (CHAT_TYPE_START == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, TRUE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->startTyping();
			}
			return;
		}
		else if (CHAT_TYPE_STOP == chat.mChatType)
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);

			// Might not have the avatar constructed yet, eg on login.
			if (chatter && chatter->isAvatar())
			{
				((LLVOAvatar*)chatter)->stopTyping();
			}
			return;
		}

		// Look for IRC-style emotes
		if (ircstyle)
		{
			// set CHAT_STYLE_IRC to avoid adding Avatar Name as author of message. See EXT-656
			chat.mChatStyle = CHAT_STYLE_IRC;

			// Do nothing, ircstyle is fixed above for chat bubbles
		}
		else
		{
			chat.mText = "";
			switch(chat.mChatType)
			{
			case CHAT_TYPE_WHISPER:
				chat.mText = LLTrans::getString("whisper") + " ";
				break;
			case CHAT_TYPE_OWNER:
				// <FS:TT> Client LSL Bridge
				{
					if (FSLSLBridge::instance().lslToViewer(mesg, from_id, owner_id))
					{
						return;
					}
				}
				// </FS:TT>
				
				// <FS:KC> cmdline packager
				if (cmdline_packager(mesg, from_id, owner_id))
				{
					return;
				}
				// </FS:KC>

// [RLVa:KB] - Checked: 2010-02-XX (RLVa-1.2.0a) | Modified: RLVa-1.1.0f
				// TODO-RLVa: [RLVa-1.2.0] consider rewriting this before a RLVa-1.2.0 release
				if ( (rlv_handler_t::isEnabled()) && (mesg.length() > 3) && (RLV_CMD_PREFIX == mesg[0]) && (CHAT_TYPE_OWNER == chat.mChatType) &&
					 ((!chatter) || (!chatter->isAttachment()) || (!chatter->isTempAttachment()) || (RlvSettings::getEnableTemporaryAttachments())) )
				{
					mesg.erase(0, 1);
					LLStringUtil::toLower(mesg);

					std::string strExecuted, strFailed, strRetained, *pstr;

					boost_tokenizer tokens(mesg, boost::char_separator<char>(",", "", boost::drop_empty_tokens));
					for (boost_tokenizer::iterator itToken = tokens.begin(); itToken != tokens.end(); ++itToken)
					{
						std::string strCmd = *itToken;

						ERlvCmdRet eRet = gRlvHandler.processCommand(from_id, strCmd, true);
						if ( (RlvSettings::getDebug()) &&
							 ( (!RlvSettings::getDebugHideUnsetDup()) || 
							   ((RLV_RET_SUCCESS_UNSET != eRet) && (RLV_RET_SUCCESS_DUPLICATE != eRet)) ) )
						{
							if ( RLV_RET_SUCCESS == (eRet & RLV_RET_SUCCESS) )	
								pstr = &strExecuted;
							else if ( RLV_RET_FAILED == (eRet & RLV_RET_FAILED) )
								pstr = &strFailed;
							else if (RLV_RET_RETAINED == eRet)
								pstr = &strRetained;
							else
							{
								RLV_ASSERT(false);
								pstr = &strFailed;
							}

							const char* pstrSuffix = RlvStrings::getStringFromReturnCode(eRet);
							if (pstrSuffix)
								strCmd.append(" (").append(pstrSuffix).append(")");

							if (!pstr->empty())
								pstr->push_back(',');
							pstr->append(strCmd);
						}
					}

					if (RlvForceWear::instanceExists())
						RlvForceWear::instance().done();

					if ( (!RlvSettings::getDebug()) || ((strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty())) )
						return;

					// Silly people want comprehensive debug messages, blah :p
					if ( (!strExecuted.empty()) && (strFailed.empty()) && (strRetained.empty()) )
					{
						chat.mText = " executes: @";
						mesg = strExecuted;
					}
					else if ( (strExecuted.empty()) && (!strFailed.empty()) && (strRetained.empty()) )
					{
						chat.mText = " failed: @";
						mesg = strFailed;
					}
					else if ( (strExecuted.empty()) && (strFailed.empty()) && (!strRetained.empty()) )
					{
						chat.mText = " retained: @";
						mesg = strRetained;
					}
					else
					{
						chat.mText = ": @";
						if (!strExecuted.empty())
							mesg += "\n    - executed: @" + strExecuted;
						if (!strFailed.empty())
							mesg += "\n    - failed: @" + strFailed;
						if (!strRetained.empty())
							mesg += "\n    - retained: @" + strRetained;
					}

					break;
				}
// [/RLVa:KB]
// [RLVa:KB] - Checked: 2010-03-09 (RLVa-1.2.0b) | Modified: RLVa-1.0.0g
				// Copy/paste from above
				if  ( (rlv_handler_t::isEnabled()) && (chatter) && (chat.mSourceType == CHAT_SOURCE_OBJECT) &&
					  (gSavedSettings.getBOOL("EffectScriptChatParticles")) )
				{
					LLPointer<LLViewerPartSourceChat> psc = new LLViewerPartSourceChat(chatter->getPositionAgent());
					psc->setSourceObject(chatter);
					psc->setColor(color);
					//We set the particles to be owned by the object's owner, 
					//just in case they should be muted by the mute list
					psc->setOwnerUUID(owner_id);
					LLViewerPartSim::getInstance()->addPartSource(psc);
				}
// [/RLVa:KB]
			case CHAT_TYPE_DEBUG_MSG:
			case CHAT_TYPE_NORMAL:
			case CHAT_TYPE_DIRECT:
				break;
			case CHAT_TYPE_SHOUT:
				chat.mText = LLTrans::getString("shout") + " ";
				break;
			case CHAT_TYPE_START:
			case CHAT_TYPE_STOP:
				LL_WARNS("Messaging") << "Got chat type start/stop in main chat processing." << LL_ENDL;
				break;
			default:
				LL_WARNS("Messaging") << "Unknown type " << chat.mChatType << " in chat!" << LL_ENDL;
				break;
			}

			chat.mText += mesg;
		}

		// We have a real utterance now, so can stop showing "..." and proceed.
		if (chatter && chatter->isAvatar())
		{
			LLLocalSpeakerMgr::getInstance()->setSpeakerTyping(from_id, FALSE);
			((LLVOAvatar*)chatter)->stopTyping();
			
			if (!is_muted && !is_do_not_disturb)
			{
				//visible_in_chat_bubble = gSavedSettings.getBOOL("UseChatBubbles");
				std::string formated_msg = "";
				LLViewerChat::formatChatMsg(chat, formated_msg);
				LLChat chat_bubble = chat;

				chat_bubble.mText = formated_msg;
				((LLVOAvatar*)chatter)->addChat(chat_bubble);
			}
		}
		
		if (chatter)
		{
			chat.mPosAgent = chatter->getPositionAgent();
		}

		// truth table:
		// LINDEN	BUSY	MUTED	OWNED_BY_YOU	TASK		DISPLAY		STORE IN HISTORY
		// F		F		F		F				*			Yes			Yes
		// F		F		F		T				*			Yes			Yes
		// F		F		T		F				*			No			No
		// F		F		T		T				*			No			No
		// F		T		F		F				*			No			Yes
		// F		T		F		T				*			Yes			Yes
		// F		T		T		F				*			No			No
		// F		T		T		T				*			No			No
		// T		*		*		*				F			Yes			Yes

		chat.mMuted = is_muted && !is_linden;

		// pass owner_id to chat so that we can display the remote
		// object inspect for an object that is chatting with you
		LLSD args;
		chat.mOwnerID = owner_id;

		// <FS:PP> FIRE-10178: Keyword Alerts in group IM do not work unless the group is in the foreground (notification on receipt of local chat)
		if (FSKeywords::getInstance()->chatContainsKeyword(chat, true))
		{
			FSKeywords::notify(chat);
		}
		// </FS:PP>

		// <FS:PP> gSavedSettings to LLCachedControl
		// if (gSavedSettings.getBOOL("TranslateChat") && chat.mSourceType != CHAT_SOURCE_SYSTEM)
		static LLCachedControl<bool> TranslateChat(gSavedSettings, "TranslateChat");
		if (TranslateChat && chat.mSourceType != CHAT_SOURCE_SYSTEM)
		// </FS:PP>
		{
			if (chat.mChatStyle == CHAT_STYLE_IRC)
			{
				mesg = mesg.substr(4, std::string::npos);
			}
			const std::string from_lang = ""; // leave empty to trigger autodetect
			const std::string to_lang = LLTranslate::getTranslateLanguage();

            LLTranslate::translateMessage(from_lang, to_lang, mesg,
                boost::bind(&translateSuccess, chat, args, mesg, from_lang, _1, _2),
                boost::bind(&translateFailure, chat, args, _1, _2));

		}
		else
		{
			LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		}

		// don't call notification for debug messages from not owned objects
		if (chat.mChatType == CHAT_TYPE_DEBUG_MSG)
		{
			// <FS:Ansariel> FIRE-15014: [OpenSim] osMessageObject(target, message) fails silently
			//if (gAgentID != chat.mOwnerID)
#ifdef OPENSIM
			if (LLGridManager::getInstance()->isInSecondLife() && gAgentID != chat.mOwnerID)
#else
			if (gAgentID != chat.mOwnerID)
#endif
			// </FS:Ansariel>
			{
				return;
			}
		}

		if (mesg != "")
		{
			LLSD msg_notify = LLSD(LLSD::emptyMap());
			msg_notify["session_id"] = LLUUID();
			msg_notify["from_id"] = chat.mFromID;
			msg_notify["source_type"] = chat.mSourceType;
			on_new_message(msg_notify);
		}

	}
}


// Simulator we're on is informing the viewer that the agent
// is starting to teleport (perhaps to another sim, perhaps to the 
// same sim). If we initiated the teleport process by sending some kind 
// of TeleportRequest, then this info is redundant, but if the sim 
// initiated the teleport (via a script call, being killed, etc.) 
// then this info is news to us.
void process_teleport_start(LLMessageSystem *msg, void**)
{
	// on teleport, don't tell them about destination guide anymore
	LLFirstUse::notUsingDestinationGuide(false);
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);

	LL_DEBUGS("Messaging") << "Got TeleportStart with TeleportFlags=" << teleport_flags << ". gTeleportDisplay: " << gTeleportDisplay << ", gAgent.mTeleportState: " << gAgent.getTeleportState() << LL_ENDL;

	// *NOTE: The server sends two StartTeleport packets when you are teleporting to a LM
	LLViewerMessage::getInstance()->mTeleportStartedSignal();

//	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Added: RLVa-0.2.0b
	if ( ((teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) && !gSavedSettings.getBOOL("FSAlwaysShowTPCancel")) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, LLTrans::getString("Cancel"));
	}

	// Freeze the UI and show progress bar
	// Note: could add data here to differentiate between normal teleport and death.

	if( gAgent.getTeleportState() == LLAgent::TELEPORT_NONE )
	{
		gTeleportDisplay = TRUE;
		gAgent.setTeleportState( LLAgent::TELEPORT_START );
		make_ui_sound("UISndTeleportOut");
		
		LL_INFOS("Messaging") << "Teleport initiated by remote TeleportStart message with TeleportFlags: " <<  teleport_flags << LL_ENDL;

		// Don't call LLFirstUse::useTeleport here because this could be
		// due to being killed, which would send you home, not to a Telehub
	}
}

boost::signals2::connection LLViewerMessage::setTeleportStartedCallback(teleport_started_callback_t cb)
{
	return mTeleportStartedSignal.connect(cb);
}

void process_teleport_progress(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if((gAgent.getID() != agent_id)
	   || (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE))
	{
		LL_WARNS("Messaging") << "Unexpected teleport progress message." << LL_ENDL;
		return;
	}
	U32 teleport_flags = 0x0;
	msg->getU32("Info", "TeleportFlags", teleport_flags);
//	if (teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL)
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Added: RLVa-0.2.0b
	if ( ((teleport_flags & TELEPORT_FLAGS_DISABLE_CANCEL) && !gSavedSettings.getBOOL("FSAlwaysShowTPCancel")) || (!gRlvHandler.getCanCancelTp()) )
// [/RLVa:KB]
	{
		gViewerWindow->setProgressCancelButtonVisible(FALSE);
	}
	else
	{
		gViewerWindow->setProgressCancelButtonVisible(TRUE, LLTrans::getString("Cancel"));
	}
	std::string buffer;
	msg->getString("Info", "Message", buffer);
	LL_DEBUGS("Messaging") << "teleport progress: " << buffer << LL_ENDL;

	//Sorta hacky...default to using simulator raw messages
	//if we don't find the coresponding mapping in our progress mappings
	std::string message = buffer;

	if (LLAgent::sTeleportProgressMessages.find(buffer) != 
		LLAgent::sTeleportProgressMessages.end() )
	{
		message = LLAgent::sTeleportProgressMessages[buffer];
	}

	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages[message]);
}

class LLFetchInWelcomeArea : public LLInventoryFetchDescendentsObserver
{
public:
	LLFetchInWelcomeArea(const uuid_vec_t &ids) :
		LLInventoryFetchDescendentsObserver(ids)
	{}
	virtual void done()
	{
		LLIsType is_landmark(LLAssetType::AT_LANDMARK);
		LLIsType is_card(LLAssetType::AT_CALLINGCARD);

		LLInventoryModel::cat_array_t	card_cats;
		LLInventoryModel::item_array_t	card_items;
		LLInventoryModel::cat_array_t	land_cats;
		LLInventoryModel::item_array_t	land_items;

		uuid_vec_t::iterator it = mComplete.begin();
		uuid_vec_t::iterator end = mComplete.end();
		for(; it != end; ++it)
		{
			gInventory.collectDescendentsIf(
				(*it),
				land_cats,
				land_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_landmark);
			gInventory.collectDescendentsIf(
				(*it),
				card_cats,
				card_items,
				LLInventoryModel::EXCLUDE_TRASH,
				is_card);
		}

		gInventory.removeObserver(this);
		delete this;
	}
};



class LLPostTeleportNotifiers : public LLEventTimer 
{
public:
	LLPostTeleportNotifiers();
	virtual ~LLPostTeleportNotifiers();

	//function to be called at the supplied frequency
	virtual BOOL tick();
};

LLPostTeleportNotifiers::LLPostTeleportNotifiers() : LLEventTimer( 2.0 )
{
};

LLPostTeleportNotifiers::~LLPostTeleportNotifiers()
{
}

BOOL LLPostTeleportNotifiers::tick()
{
	BOOL all_done = FALSE;
	if ( gAgent.getTeleportState() == LLAgent::TELEPORT_NONE )
	{
		// get callingcards and landmarks available to the user arriving.
		uuid_vec_t folders;
		const LLUUID callingcard_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_CALLINGCARD);
		if(callingcard_id.notNull()) 
			folders.push_back(callingcard_id);
		const LLUUID folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
		if(folder_id.notNull()) 
			folders.push_back(folder_id);
		if(!folders.empty())
		{
			LLFetchInWelcomeArea* fetcher = new LLFetchInWelcomeArea(folders);
			fetcher->startFetch();
			if(fetcher->isFinished())
			{
				fetcher->done();
			}
			else
			{
				gInventory.addObserver(fetcher);
			}
		}
		all_done = TRUE;
	}

	return all_done;
}



// Teleport notification from the simulator
// We're going to pretend to be a new agent
void process_teleport_finish(LLMessageSystem* msg, void**)
{
	LL_DEBUGS("Messaging") << "Got teleport location message" << LL_ENDL;
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}

    if (gAgent.getTeleportState() == LLAgent::TELEPORT_NONE)
    {
        // Server either ignored teleport cancel message or did not receive it in time.
        // This message can't be ignored since teleport is complete at server side
        gAgent.restoreCanceledTeleportRequest();
    }
	
	// Teleport is finished; it can't be cancelled now.
	gViewerWindow->setProgressCancelButtonVisible(FALSE);

	gPipeline.doResetVertexBuffers(true);

	// Do teleport effect for where you're leaving
	// VEFFECT: TeleportStart
	LLHUDEffectSpiral *effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());
	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

	U32 location_id;
	U32 sim_ip;
	U16 sim_port;
	LLVector3 pos, look_at;
	U64 region_handle;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getIPAddrFast(_PREHASH_Info, _PREHASH_SimIP, sim_ip);
	msg->getIPPortFast(_PREHASH_Info, _PREHASH_SimPort, sim_port);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	//msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU64Fast(_PREHASH_Info, _PREHASH_RegionHandle, region_handle);
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

// <FS:CR> Aurora Sim
	U32 region_size_x = 256;
	U32 region_size_y = 256;

#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeX, region_size_x);
		msg->getU32Fast(_PREHASH_Info, _PREHASH_RegionSizeY, region_size_y);

		//and a little hack for Second Life compatibility
		if (region_size_y == 0 || region_size_x == 0)
		{
			region_size_x = 256;
			region_size_y = 256;
		}
	}
#endif
// </FS:CR> Aurora Sim
	
	std::string seedCap;
	msg->getStringFast(_PREHASH_Info, _PREHASH_SeedCapability, seedCap);

	// update home location if we are teleporting out of prelude - specific to teleporting to welcome area 
	if((teleport_flags & TELEPORT_FLAGS_SET_HOME_TO_TARGET)
	   && (!gAgent.isGodlike()))
	{
		gAgent.setHomePosRegion(region_handle, pos);

		// Create a timer that will send notices when teleporting is all finished.  Since this is 
		// based on the LLEventTimer class, it will be managed by that class and not orphaned or leaked.
		new LLPostTeleportNotifiers();
	}

	LLHost sim_host(sim_ip, sim_port);

	// Viewer trusts the simulator.
	gMessageSystem->enableCircuit(sim_host, TRUE);
// <FS:CR> Aurora Sim
	//LLViewerRegion* regionp =  LLWorld::getInstance()->addRegion(region_handle, sim_host);
	LLViewerRegion* regionp =  LLWorld::getInstance()->addRegion(region_handle, sim_host, region_size_x, region_size_y);
// </FS:CR> Aurora Sim
	
	// Ansariel: Disable teleport beacon after teleport
	if (gSavedSettings.getBOOL("FSDisableBeaconAfterTeleport"))
	{
		LLTracker::stopTracking((void *)(intptr_t)TRUE);
		LLWorldMap::getInstance()->cancelTracking();
	}

	// <FS:CR> FIRE-5118 - Lightshare support
	FSLightshare::getInstance()->processLightshareReset();
	// </FS:CR>
/*
	// send camera update to new region
	gAgentCamera.updateCamera();

	// likewise make sure the camera is behind the avatar
	gAgentCamera.resetView(TRUE);
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->clearChatText();
		gAgentCamera.slamLookAt(look_at);
	}
	gAgent.setPositionAgent(pos);
	gAssetStorage->setUpstream(sim);
	gCacheName->setUpstream(sim);
*/

	// Make sure we're standing
	gAgent.standUp();

	// now, use the circuit info to tell simulator about us!
	LL_INFOS("Messaging") << "process_teleport_finish() Enabling "
			<< sim_host << " with code " << msg->mOurCircuitCode << LL_ENDL;
	msg->newMessageFast(_PREHASH_UseCircuitCode);
	msg->nextBlockFast(_PREHASH_CircuitCode);
	msg->addU32Fast(_PREHASH_Code, msg->getOurCircuitCode());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_ID, gAgent.getID());
	msg->sendReliable(sim_host);

	send_complete_agent_movement(sim_host);
	gAgent.setTeleportState( LLAgent::TELEPORT_MOVING );
	gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["contacting"]);

	LL_DEBUGS("CrossingCaps") << "Calling setSeedCapability from process_teleport_finish(). Seed cap == "
			<< seedCap << LL_ENDL;
	regionp->setSeedCapability(seedCap);

	// Don't send camera updates to the new region until we're
	// actually there...

	// <FS:Ansariel> Copied from process_teleport_local: Keep us flying if we
	//               were flying before the teleport or we always want to fly
	//               after a TP.
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING || gSavedSettings.getBOOL("FSFlyAfterTeleport"))
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}
	// </FS:Ansariel>

	// <FS:Ansariel> Stop typing after teleport (possible fix for FIRE-7273)
	gAgent.stopTyping();

	// Now do teleport effect for where you're going.
	// VEFFECT: TeleportEnd
	effectp = (LLHUDEffectSpiral *)LLHUDManager::getInstance()->createViewerEffect(LLHUDObject::LL_HUD_EFFECT_POINT, TRUE);
	effectp->setPositionGlobal(gAgent.getPositionGlobal());

	effectp->setColor(LLColor4U(gAgent.getEffectColor()));
	LLHUDManager::getInstance()->sendEffects();

//	gTeleportDisplay = TRUE;
//	gTeleportDisplayTimer.reset();
//	gViewerWindow->setShowProgress(TRUE);
}

// stuff we have to do every time we get an AvatarInitComplete from a sim
/*
void process_avatar_init_complete(LLMessageSystem* msg, void**)
{
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_AvatarData, _PREHASH_Position, agent_pos);
	agent_movement_complete(msg->getSender(), agent_pos);
}
*/

void process_agent_movement_complete(LLMessageSystem* msg, void**)
{
	gShiftFrame = true;
	gAgentMovementCompleted = true;

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_agent_movement_complete()"
				<< LL_ENDL;
		return;
	}

	LL_DEBUGS("Messaging") << "process_agent_movement_complete()" << LL_ENDL;

	// *TODO: check timestamp to make sure the movement compleation
	// makes sense.
	LLVector3 agent_pos;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_Position, agent_pos);
	LLVector3 look_at;
	msg->getVector3Fast(_PREHASH_Data, _PREHASH_LookAt, look_at);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_Data, _PREHASH_RegionHandle, region_handle);
	
	std::string version_channel;
	msg->getString("SimData", "ChannelVersion", version_channel);

	if (!isAgentAvatarValid())
	{
		// Could happen if you were immediately god-teleported away on login,
		// maybe other cases.  Continue, but warn.
		LL_WARNS("Messaging") << "agent_movement_complete() with NULL avatarp." << LL_ENDL;
	}

	F32 x, y;
	from_region_handle(region_handle, &x, &y);
	LLViewerRegion* regionp = LLWorld::getInstance()->getRegionFromHandle(region_handle);
	if (!regionp)
	{
		if (gAgent.getRegion())
		{
			LL_WARNS("Messaging") << "current region " << gAgent.getRegion()->getOriginGlobal() << LL_ENDL;
		}

		LL_WARNS("Messaging") << "Agent being sent to invalid home region: " 
			<< x << ":" << y 
			<< " current pos " << gAgent.getPositionGlobal()
			<< LL_ENDL;
		LLAppViewer::instance()->forceDisconnect(LLTrans::getString("SentToInvalidRegion"));
		return;

	}

	LL_INFOS("Messaging") << "Changing home region to " << x << ":" << y << LL_ENDL;

	// set our upstream host the new simulator and shuffle things as
	// appropriate.
	LLVector3 shift_vector = regionp->getPosRegionFromGlobal(
		gAgent.getRegion()->getOriginGlobal());
	gAgent.setRegion(regionp);
	gObjectList.shiftObjects(shift_vector);
// <FS:CR> FIRE-11593: Opensim "4096 Bug" Fix by Latif Khalifa
#ifdef OPENSIM
	// Is this a really long jump?
	if (shift_vector.length() > 2048.f * 256.f)
	{
		regionp->reInitPartitions();
		gAgent.setRegion(regionp);
		// Kill objects in the regions we left behind
		for (LLWorld::region_list_t::const_iterator r = LLWorld::getInstance()->getRegionList().begin();
			r != LLWorld::getInstance()->getRegionList().end(); ++r)
		{
			if (*r != regionp)
			{
				gObjectList.killObjects(*r);
			}
		}
	}
#endif
// </FS:CR>
	gAssetStorage->setUpstream(msg->getSender());
	gCacheName->setUpstream(msg->getSender());
	gViewerThrottle.sendToSim();
	gViewerWindow->sendShapeToSim();

	bool is_teleport = gAgent.getTeleportState() == LLAgent::TELEPORT_MOVING;

	if( is_teleport )
	{
		if (gAgent.getTeleportKeepsLookAt())
		{
			// *NOTE: the LookAt data we get from the sim here doesn't
			// seem to be useful, so get it from the camera instead
			look_at = LLViewerCamera::getInstance()->getAtAxis();
		}
		// Force the camera back onto the agent, don't animate.
		gAgentCamera.setFocusOnAvatar(TRUE, FALSE);
		gAgentCamera.slamLookAt(look_at);
		gAgentCamera.updateCamera();

		gAgent.setTeleportState( LLAgent::TELEPORT_START_ARRIVAL );

		// <FS:Ansariel> [Legacy Bake]
		// set the appearance on teleport since the new sim does not;
		// know what you look like.
		gAgent.sendAgentSetAppearance();
		// </FS:Ansariel> [Legacy Bake]

		if (isAgentAvatarValid())
		{
			// [FS:CR] Reimplement DEV-4907 (Maybe we like long distracting messages?)
			if (gSavedSettings.getBOOL("FSShowBackSLURL"))
			{
				LLSLURL slurl;
				gAgent.getTeleportSourceSLURL(slurl);
				LLSD substitution = LLSD().with("[T_SLURL]", slurl.getSLURLString());
				std::string completed_from = LLAgent::sTeleportProgressMessages["completed_from"];
				LLStringUtil::format(completed_from, substitution);
				
				LLSD args;
				args["MESSAGE"] = completed_from;
				LLNotificationsUtil::add("ChatSystemMessageTip", args);
			}
			// [/FS:CR]
			// Set the new position
			gAgentAvatarp->setPositionAgent(agent_pos);
			gAgentAvatarp->clearChat();
			gAgentAvatarp->slamPosition();
		}
	}
	else
	{
		// This is initial log-in or a region crossing
		gAgent.setTeleportState( LLAgent::TELEPORT_NONE );

		if(LLStartUp::getStartupState() < STATE_STARTED)
		{	// This is initial log-in, not a region crossing:
			// Set the camera looking ahead of the AV so send_agent_update() below 
			// will report the correct location to the server.
			LLVector3 look_at_point = look_at;
			look_at_point = agent_pos + look_at_point.rotVec(gAgent.getQuat());

			static LLVector3 up_direction(0.0f, 0.0f, 1.0f);
			LLViewerCamera::getInstance()->lookAt(agent_pos, look_at_point, up_direction);
		}
	}

	if ( LLTracker::isTracking(NULL) )
	{
		// Check distance to beacon, if < 5m, remove beacon
		LLVector3d beacon_pos = LLTracker::getTrackedPositionGlobal();
		LLVector3 beacon_dir(agent_pos.mV[VX] - (F32)fmod(beacon_pos.mdV[VX], 256.0), agent_pos.mV[VY] - (F32)fmod(beacon_pos.mdV[VY], 256.0), 0);
		if (beacon_dir.magVecSquared() < 25.f)
		{
			LLTracker::stopTracking(NULL);
		}
		else if ( is_teleport && !gAgent.getTeleportKeepsLookAt() && look_at.isExactlyZero())
		{
			//look at the beacon
			LLVector3 global_agent_pos = agent_pos;
			global_agent_pos[0] += x;
			global_agent_pos[1] += y;
			look_at = (LLVector3)beacon_pos - global_agent_pos;
			look_at.normVec();
			gAgentCamera.slamLookAt(look_at);
		}
	}

	// TODO: Put back a check for flying status! DK 12/19/05
	// Sim tells us whether the new position is off the ground
	/*
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}
	*/

	send_agent_update(TRUE, TRUE);

	if (gAgent.getRegion()->getBlockFly())
	{
		gAgent.setFlying(gAgent.canFly());
	}

	// force simulator to recognize do not disturb state
	if (gAgent.isDoNotDisturb())
	{
		gAgent.setDoNotDisturb(true);
	}
	else
	{
		gAgent.setDoNotDisturb(false);
	}

	if (isAgentAvatarValid())
	{
		gAgentAvatarp->mFootPlane.clearVec();
	}
	
	// send walk-vs-run status
//	gAgent.sendWalkRun(gAgent.getRunning() || gAgent.getAlwaysRun());
// [RLVa:KB] - Checked: 2011-05-11 (RLVa-1.3.0i) | Added: RLVa-1.3.0i
	gAgent.sendWalkRun();
// [/RLVa:KB]

	// If the server version has changed, display an info box and offer
	// to display the release notes, unless this is the initial log in.
	if (gLastVersionChannel == version_channel)
	{
		return;
	}

	// <FS:Ansariel> Bring back simulator version changed messages after TP
	if (!gLastVersionChannel.empty() && gSavedSettings.getBOOL("FSShowServerVersionChangeNotice"))
	{
		LLSD args;
		args["OLDVERSION"] = gLastVersionChannel;
		args["NEWVERSION"] = version_channel;
		LLNotificationsUtil::add("ServerVersionChanged", args);
	}
	// </FS:Ansariel>

	gLastVersionChannel = version_channel;
}

void process_crossed_region(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	LLUUID session_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_SessionID, session_id);
	if((gAgent.getID() != agent_id) || (gAgent.getSessionID() != session_id))
	{
		LL_WARNS("Messaging") << "Incorrect id in process_crossed_region()"
				<< LL_ENDL;
		return;
	}
	LL_INFOS("Messaging") << "process_crossed_region()" << LL_ENDL;
	gAgentAvatarp->resetRegionCrossingTimer();
	// <FS:Ansariel> FIRE-12004: Attachments getting lost on TP; this is apparently the place to
	//               hook in for region crossings - we get an info from the simulator that we
	//               crossed a region and then the viewer starts the handover process. We only
	//               receive this message if we can actually cross the region and aren't blocked
	//               for some reason (e.g. banned, group access...)
	gAgentAvatarp->setIsCrossingRegion(true);

	U32 sim_ip;
	msg->getIPAddrFast(_PREHASH_RegionData, _PREHASH_SimIP, sim_ip);
	U16 sim_port;
	msg->getIPPortFast(_PREHASH_RegionData, _PREHASH_SimPort, sim_port);
	LLHost sim_host(sim_ip, sim_port);
	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	
	std::string seedCap;
	msg->getStringFast(_PREHASH_RegionData, _PREHASH_SeedCapability, seedCap);

// <FS:CR> Aurora Sim
	U32 region_size_x = 256;
	U32 region_size_y = 256;

#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		msg->getU32(_PREHASH_RegionData, _PREHASH_RegionSizeX, region_size_x);
		msg->getU32(_PREHASH_RegionData, _PREHASH_RegionSizeY, region_size_y);

		//and a little hack for Second Life compatibility	
		if (region_size_y == 0 || region_size_x == 0)
		{
			region_size_x = 256;
			region_size_y = 256;
		}
	}
#endif
// </FS:CR> Aurora Sim
	send_complete_agent_movement(sim_host);

// <FS:CR> Aurora Sim
	//LLViewerRegion* regionp = LLWorld::getInstance()->addRegion(region_handle, sim_host);
	LLViewerRegion* regionp = LLWorld::getInstance()->addRegion(region_handle, sim_host, region_size_x, region_size_y);
// </FS:CR> Aurora Sim
	LL_DEBUGS("CrossingCaps") << "Calling setSeedCapability from process_crossed_region(). Seed cap == "
			<< seedCap << LL_ENDL;
	regionp->setSeedCapability(seedCap);
}



// Sends avatar and camera information to simulator.
// Sent roughly once per frame, or 20 times per second, whichever is less often

const F32 THRESHOLD_HEAD_ROT_QDOT = 0.9997f;	// ~= 2.5 degrees -- if its less than this we need to update head_rot
const F32 MAX_HEAD_ROT_QDOT = 0.99999f;			// ~= 0.5 degrees -- if its greater than this then no need to update head_rot
												// between these values we delay the updates (but no more than one second)

static LLTrace::BlockTimerStatHandle FTM_AGENT_UPDATE_SEND("Send Message");

void send_agent_update(BOOL force_send, BOOL send_reliable)
{
	if (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE)
	{
		// We don't care if they want to send an agent update, they're not allowed to until the simulator
		// that's the target is ready to receive them (after avatar_init_complete is received)
		return;
	}
	
	if(gAgent.getPhantom()) return; //Don't want to do this while phantom

	// We have already requested to log out.  Don't send agent updates.
	if(LLAppViewer::instance()->logoutRequestSent())
	{
		return;
	}

	// no region to send update to
	if(gAgent.getRegion() == NULL)
	{
		return;
	}

	const F32 TRANSLATE_THRESHOLD = 0.01f;

	// NOTA BENE: This is (intentionally?) using the small angle sine approximation to test for rotation
	//			  Plus, there is an extra 0.5 in the mix since the perpendicular between last_camera_at and getAtAxis() bisects cam_rot_change
	//			  Thus, we're actually testing against 0.2 degrees
	const F32 ROTATION_THRESHOLD = 0.1f * 2.f*F_PI/360.f;			//  Rotation thresh 0.2 deg, see note above

	const U8 DUP_MSGS = 1;				//  HACK!  number of times to repeat data on motionless agent

	//  Store data on last sent update so that if no changes, no send
	static LLVector3 last_camera_pos_agent, 
					 last_camera_at, 
					 last_camera_left,
					 last_camera_up;
	
	static LLVector3 cam_center_chg,
					 cam_rot_chg;

	static LLQuaternion last_head_rot;
	static U32 last_control_flags = 0;
	static U8 last_render_state;
	static U8 duplicate_count = 0;
	static F32 head_rot_chg = 1.0;
	static U8 last_flags;

	LLMessageSystem	*msg = gMessageSystem;
	LLVector3		camera_pos_agent;				// local to avatar's region
	U8				render_state;

	LLQuaternion body_rotation = gAgent.getFrameAgent().getQuaternion();
	LLQuaternion head_rotation = gAgent.getHeadRotation();

	camera_pos_agent = gAgentCamera.getCameraPositionAgent();

	render_state = gAgent.getRenderState();

	U32		control_flag_change = 0;
	U8		flag_change = 0;

	cam_center_chg = last_camera_pos_agent - camera_pos_agent;
	cam_rot_chg = last_camera_at - LLViewerCamera::getInstance()->getAtAxis();

	// If a modifier key is held down, turn off
	// LBUTTON and ML_LBUTTON so that using the camera (alt-key) doesn't
	// trigger a control event.
	U32 control_flags = gAgent.getControlFlags();

	MASK	key_mask = gKeyboard->currentMask(TRUE);

	if (key_mask & MASK_ALT || key_mask & MASK_CONTROL)
	{
		control_flags &= ~(	AGENT_CONTROL_LBUTTON_DOWN |
							AGENT_CONTROL_ML_LBUTTON_DOWN );
		control_flags |= 	AGENT_CONTROL_LBUTTON_UP |
							AGENT_CONTROL_ML_LBUTTON_UP ;
	}

	control_flag_change = last_control_flags ^ control_flags;

	U8 flags = AU_FLAGS_NONE;
	if (gAgent.isGroupTitleHidden())
	{
		flags |= AU_FLAGS_HIDETITLE;
	}
	if (gAgent.getAutoPilot())
	{
		flags |= AU_FLAGS_CLIENT_AUTOPILOT;
	}

	flag_change = last_flags ^ flags;

	head_rot_chg = dot(last_head_rot, head_rotation);

	//static S32 msg_number = 0;		// Used for diagnostic log messages

	if (force_send || 
		(cam_center_chg.magVec() > TRANSLATE_THRESHOLD) || 
		(head_rot_chg < THRESHOLD_HEAD_ROT_QDOT) ||	
		(last_render_state != render_state) ||
		(cam_rot_chg.magVec() > ROTATION_THRESHOLD) ||
		control_flag_change != 0 ||
		flag_change != 0)  
	{
		/* Diagnotics to show why we send the AgentUpdate message.  Also un-commment the msg_number code above and below this block
		msg_number += 1;
		if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT)
		{
			//LL_INFOS("Messaging") << "head rot " << head_rotation << LL_ENDL;
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", head_rot_chg " << head_rot_chg << LL_ENDL;
		}
		if (cam_rot_chg.magVec() > ROTATION_THRESHOLD) 
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", cam rot " <<  cam_rot_chg.magVec() << LL_ENDL;
		}
		if (cam_center_chg.magVec() > TRANSLATE_THRESHOLD)
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", cam center " << cam_center_chg.magVec() << LL_ENDL;
		}
//		if (drag_delta_chg.magVec() > TRANSLATE_THRESHOLD)
//		{
//			LL_INFOS("Messaging") << "drag delta " << drag_delta_chg.magVec() << LL_ENDL;
//		}
		if (control_flag_change)
		{
			LL_INFOS("Messaging") << "msg " << msg_number << ", frame " << LLFrameTimer::getFrameCount() << ", dcf = " << control_flag_change << LL_ENDL;
		}
*/

		duplicate_count = 0;
	}
	else
	{
		duplicate_count++;

		if (head_rot_chg < MAX_HEAD_ROT_QDOT  &&  duplicate_count < AGENT_UPDATES_PER_SECOND)
		{
			// The head_rotation is sent for updating things like attached guns.
			// We only trigger a new update when head_rotation deviates beyond
			// some threshold from the last update, however this can break fine
			// adjustments when trying to aim an attached gun, so what we do here
			// (where we would normally skip sending an update when nothing has changed)
			// is gradually reduce the threshold to allow a better update to 
			// eventually get sent... should update to within 0.5 degrees in less 
			// than a second.
			if (head_rot_chg < THRESHOLD_HEAD_ROT_QDOT + (MAX_HEAD_ROT_QDOT - THRESHOLD_HEAD_ROT_QDOT) * duplicate_count / AGENT_UPDATES_PER_SECOND)
			{
				duplicate_count = 0;
			}
			else
			{
				return;
			}
		}
		else
		{
			return;
		}
	}

	if (duplicate_count < DUP_MSGS && !gDisconnected)
	{
		/* More diagnostics to count AgentUpdate messages
		static S32 update_sec = 0;
		static S32 update_count = 0;
		static S32 max_update_count = 0;
		S32 cur_sec = lltrunc( LLTimer::getTotalSeconds() );
		update_count += 1;
		if (cur_sec != update_sec)
		{
			if (update_sec != 0)
			{
				update_sec = cur_sec;
				//msg_number = 0;
				max_update_count = llmax(max_update_count, update_count);
				LL_INFOS() << "Sent " << update_count << " AgentUpdate messages per second, max is " << max_update_count << LL_ENDL;
			}
			update_sec = cur_sec;
			update_count = 0;
		}
		*/

		LL_RECORD_BLOCK_TIME(FTM_AGENT_UPDATE_SEND);
		// Build the message
		msg->newMessageFast(_PREHASH_AgentUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		msg->addQuatFast(_PREHASH_BodyRotation, body_rotation);
		msg->addQuatFast(_PREHASH_HeadRotation, head_rotation);
		msg->addU8Fast(_PREHASH_State, render_state);
		msg->addU8Fast(_PREHASH_Flags, flags);

//		if (camera_pos_agent.mV[VY] > 255.f)
//		{
//			LL_INFOS("Messaging") << "Sending camera center " << camera_pos_agent << LL_ENDL;
//		}
		
		msg->addVector3Fast(_PREHASH_CameraCenter, camera_pos_agent);
		msg->addVector3Fast(_PREHASH_CameraAtAxis, LLViewerCamera::getInstance()->getAtAxis());
		msg->addVector3Fast(_PREHASH_CameraLeftAxis, LLViewerCamera::getInstance()->getLeftAxis());
		msg->addVector3Fast(_PREHASH_CameraUpAxis, LLViewerCamera::getInstance()->getUpAxis());
		msg->addF32Fast(_PREHASH_Far, gAgentCamera.mDrawDistance);
		
		msg->addU32Fast(_PREHASH_ControlFlags, control_flags);

		if (gDebugClicks)
		{
			if (control_flags & AGENT_CONTROL_LBUTTON_DOWN)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button down" << LL_ENDL;
			}

			if (control_flags & AGENT_CONTROL_LBUTTON_UP)
			{
				LL_INFOS("Messaging") << "AgentUpdate left button up" << LL_ENDL;
			}
		}

		gAgent.enableControlFlagReset();

		if (!send_reliable)
		{
			gAgent.sendMessage();
		}
		else
		{
			gAgent.sendReliableMessage();
		}

//		LL_DEBUGS("Messaging") << "agent " << avatar_pos_agent << " cam " << camera_pos_agent << LL_ENDL;

		// Copy the old data 
		last_head_rot = head_rotation;
		last_render_state = render_state;
		last_camera_pos_agent = camera_pos_agent;
		last_camera_at = LLViewerCamera::getInstance()->getAtAxis();
		last_camera_left = LLViewerCamera::getInstance()->getLeftAxis();
		last_camera_up = LLViewerCamera::getInstance()->getUpAxis();
		last_control_flags = control_flags;
		last_flags = flags;
	}
}



// *TODO: Remove this dependency, or figure out a better way to handle
// this hack.
extern U32Bits gObjectData;

void process_object_update(LLMessageSystem *mesgsys, void **user_data)
{	
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveCompressedSize();
	}
	else
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveSize();
	}

	// Update the object...
	gObjectList.processObjectUpdate(mesgsys, user_data, OUT_FULL);
}

void process_compressed_object_update(LLMessageSystem *mesgsys, void **user_data)
{
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveCompressedSize();
	}
	else
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveSize();
	}

	// Update the object...
	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_FULL_COMPRESSED);
}

void process_cached_object_update(LLMessageSystem *mesgsys, void **user_data)
{
	// Update the data counters
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveCompressedSize();
	}
	else
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveSize();
	}

	// Update the object...
	gObjectList.processCachedObjectUpdate(mesgsys, user_data, OUT_FULL_CACHED);
}


void process_terse_object_update_improved(LLMessageSystem *mesgsys, void **user_data)
{
	if (mesgsys->getReceiveCompressedSize())
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveCompressedSize();
	}
	else
	{
		gObjectData += (U32Bytes)mesgsys->getReceiveSize();
	}

	gObjectList.processCompressedObjectUpdate(mesgsys, user_data, OUT_TERSE_IMPROVED);
}

static LLTrace::BlockTimerStatHandle FTM_PROCESS_OBJECTS("Process Kill Objects");

void process_kill_object(LLMessageSystem *mesgsys, void **user_data)
{
	LL_RECORD_BLOCK_TIME(FTM_PROCESS_OBJECTS);

	LLUUID		id;

	U32 ip = mesgsys->getSenderIP();
	U32 port = mesgsys->getSenderPort();
	LLViewerRegion* regionp = NULL;
	{
		LLHost host(ip, port);
		regionp = LLWorld::getInstance()->getRegion(host);
	}

	bool delete_object = LLViewerRegion::sVOCacheCullingEnabled;
	S32	num_objects = mesgsys->getNumberOfBlocksFast(_PREHASH_ObjectData);
	for (S32 i = 0; i < num_objects; ++i)
	{
		U32	local_id;
		mesgsys->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);

		LLViewerObjectList::getUUIDFromLocal(id, local_id, ip, port); 
		if (id == LLUUID::null)
		{
			LL_DEBUGS("Messaging") << "Unknown kill for local " << local_id << LL_ENDL;
			continue;
		}
		else
		{
			LL_DEBUGS("Messaging") << "Kill message for local " << local_id << LL_ENDL;
		}

		if (id == gAgentID)
		{
			// never kill our avatar
			continue;
		}

			LLViewerObject *objectp = gObjectList.findObject(id);
			if (objectp)
			{
				// <FS:Ansariel> FIRE-12004: Attachments getting lost on TP
				static LLCachedControl<bool> fsExperimentalLostAttachmentsFix(gSavedSettings, "FSExperimentalLostAttachmentsFix");
				if (fsExperimentalLostAttachmentsFix &&
					isAgentAvatarValid() &&
					(gAgent.getTeleportState() != LLAgent::TELEPORT_NONE || gAgentAvatarp->isCrossingRegion()) && 
					(objectp->isAttachment() || objectp->isTempAttachment()) &&
					objectp->permYouOwner())
				{
					// Simply ignore the request and don't kill the object - this should work...
					if (gSavedSettings.getBOOL("FSExperimentalLostAttachmentsFixReport"))
					{
						report_to_nearby_chat("Sim tried to kill attachment: " + objectp->getAttachmentItemName() + " (" + (gAgent.getTeleportState() != LLAgent::TELEPORT_NONE ? "tp" : "crossing") + ")");
					}
					continue;
				}
				// </FS:Ansariel>

				// Display green bubble on kill
				if ( gShowObjectUpdates )
				{
					LLColor4 color(0.f,1.f,0.f,1.f);
					gPipeline.addDebugBlip(objectp->getPositionAgent(), color);
				}

				// Do the kill
				gObjectList.killObject(objectp);
			}

			if(delete_object)
			{
				regionp->killCacheEntry(local_id);
		}

		// We should remove the object from selection after it is marked dead by gObjectList to make LLToolGrab,
        // which is using the object, release the mouse capture correctly when the object dies.
        // See LLToolGrab::handleHoverActive() and LLToolGrab::handleHoverNonPhysical().
		LLSelectMgr::getInstance()->removeObjectFromSelections(id);
	}
}

// <FS:Techwolf Lupindo> area search
void process_object_properties(LLMessageSystem *msg, void**user_data)
{
	// Send the result to the corresponding requesters.
	LLSelectMgr::processObjectProperties(msg, user_data);
	
	FSAreaSearch* area_search_floater = LLFloaterReg::findTypedInstance<FSAreaSearch>("area_search");
	if (area_search_floater)
	{
		area_search_floater->processObjectProperties(msg);
	}

	AnimationExplorer* explorer = LLFloaterReg::findTypedInstance<AnimationExplorer>("animation_explorer");
	if (explorer)
	{
		explorer->requestNameCallback(msg);
	}
}
// </FS:Techwolf Lupindo> area search

// <FS:Ansariel> Anti spam
void process_object_properties_family(LLMessageSystem *msg, void**user_data)
{
	// Send the result to the corresponding requesters.
	LLSelectMgr::processObjectPropertiesFamily(msg, user_data);

	if (NACLAntiSpamRegistry::instanceExists())
	{
		NACLAntiSpamRegistry::instance().processObjectPropertiesFamily(msg);
	}
}
// </FS:Ansariel>

void process_time_synch(LLMessageSystem *mesgsys, void **user_data)
{
	LLVector3 sun_direction;
	LLVector3 sun_ang_velocity;
	F32 phase;
	U64	space_time_usec;

    U32 seconds_per_day;
    U32 seconds_per_year;

	// "SimulatorViewerTimeMessage"
	mesgsys->getU64Fast(_PREHASH_TimeInfo, _PREHASH_UsecSinceStart, space_time_usec);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerDay, seconds_per_day);
	mesgsys->getU32Fast(_PREHASH_TimeInfo, _PREHASH_SecPerYear, seconds_per_year);

	// This should eventually be moved to an "UpdateHeavenlyBodies" message
	mesgsys->getF32Fast(_PREHASH_TimeInfo, _PREHASH_SunPhase, phase);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunDirection, sun_direction);
	mesgsys->getVector3Fast(_PREHASH_TimeInfo, _PREHASH_SunAngVelocity, sun_ang_velocity);

	LLWorld::getInstance()->setSpaceTimeUSec(space_time_usec);

	LL_DEBUGS("Windlight Sync") << "Sun phase: " << phase << " rad = " << fmodf(phase / F_TWO_PI + 0.25, 1.f) * 24.f << " h" << LL_ENDL;

	gSky.setSunPhase(phase);
	gSky.setSunTargetDirection(sun_direction, sun_ang_velocity);
	if ( !(gSavedSettings.getBOOL("SkyOverrideSimSunPosition") || gSky.getOverrideSun()) )
	{
		gSky.setSunDirection(sun_direction, sun_ang_velocity);
	}
}

void process_sound_trigger(LLMessageSystem *msg, void **)
{
	if (!gAudiop)
	{
#if !LL_LINUX
		LL_WARNS("AudioEngine") << "LLAudioEngine instance doesn't exist!" << LL_ENDL;
#endif
		return;
	}

	U64		region_handle = 0;
	F32		gain = 0;
	LLUUID	sound_id;
	LLUUID	owner_id;
	LLUUID	object_id;
	LLUUID	parent_id;
	LLVector3	pos_local;

	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_OwnerID, owner_id);
	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ObjectID, object_id);

	// <FS:ND> Protect against corrupted sounds
	if( gAudiop->isCorruptSound( sound_id ) )
		return;
	// </FS:ND>

	// <FS> Asset blacklist
	if (FSAssetBlacklist::getInstance()->isBlacklisted(sound_id, LLAssetType::AT_SOUND))
	{
		return;
	}
	// </FS>

	// NaCl - Antispam Registry
 	static LLCachedControl<U32> _NACL_AntiSpamSoundMulti(gSavedSettings, "_NACL_AntiSpamSoundMulti");
	static LLCachedControl<bool> FSPlayCollisionSounds(gSavedSettings, "FSPlayCollisionSounds");
	if (NACLAntiSpamRegistry::instance().isCollisionSound(sound_id))
	{
		if (!FSPlayCollisionSounds)
		{
			return;
		}
	}
	else
	{
		if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND, object_id, ANTISPAM_SOURCE_OBJECT, _NACL_AntiSpamSoundMulti) ||
			NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND, owner_id, ANTISPAM_SOURCE_AGENT, _NACL_AntiSpamSoundMulti))
		{
			return;
		}
	}
	// NaCl End

	msg->getUUIDFast(_PREHASH_SoundData, _PREHASH_ParentID, parent_id);
	msg->getU64Fast(_PREHASH_SoundData, _PREHASH_Handle, region_handle);
	msg->getVector3Fast(_PREHASH_SoundData, _PREHASH_Position, pos_local);
	msg->getF32Fast(_PREHASH_SoundData, _PREHASH_Gain, gain);

	// adjust sound location to true global coords
	LLVector3d	pos_global = from_region_handle(region_handle);
	pos_global.mdV[VX] += pos_local.mV[VX];
	pos_global.mdV[VY] += pos_local.mV[VY];
	pos_global.mdV[VZ] += pos_local.mV[VZ];

	// Don't play a trigger sound if you can't hear it due
	// to parcel "local audio only" settings.
	if (!LLViewerParcelMgr::getInstance()->canHearSound(pos_global)) return;

	// Don't play sounds triggered by someone you muted.
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	// Don't play sounds from an object you muted
	if (LLMuteList::getInstance()->isMuted(object_id)) return;

	// Don't play sounds from an object whose parent you muted
	if (parent_id.notNull()
		&& LLMuteList::getInstance()->isMuted(parent_id))
	{
		return;
	}

	// Don't play sounds from a region with maturity above current agent maturity
	if( !gAgent.canAccessMaturityInRegion( region_handle ) )
	{
		return;
	}
	
	// AO: Hack for legacy radar script interface compatibility. Interpret certain
	// sound assets as a request for a full radar update to a channel
	if ((owner_id == gAgent.getID()) && (sound_id.asString() == gSavedSettings.getString("RadarLegacyChannelAlertRefreshUUID")))
	{
		FSRadar* radar = FSRadar::getInstance();
		if (radar)
		{
			radar->requestRadarChannelAlertSync();
		}
		return;
	}
		
	// Don't play sounds from gestures if they are not enabled.
	// ...TS: Unless they're your own.
	if ((!gSavedSettings.getBOOL("EnableGestureSounds")) &&
		(owner_id != gAgent.getID()) &&
		(owner_id == object_id)) return;

  // NaCl - Sound Explorer
	gAudiop->triggerSound(sound_id, owner_id, gain, LLAudioEngine::AUDIO_TYPE_SFX, pos_global, object_id);
  // NaCl End
}

void process_preload_sound(LLMessageSystem *msg, void **user_data)
{
	if (!gAudiop)
	{
#if !LL_LINUX
		LL_WARNS("AudioEngine") << "LLAudioEngine instance doesn't exist!" << LL_ENDL;
#endif
		return;
	}

	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	// <FS> Asset blacklist
	if (FSAssetBlacklist::getInstance()->isBlacklisted(sound_id, LLAssetType::AT_SOUND))
	{
		return;
	}
	// </FS>

	// NaCl - Antispam Registry
	static LLCachedControl<U32> _NACL_AntiSpamSoundPreloadMulti(gSavedSettings, "_NACL_AntiSpamSoundPreloadMulti");
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND_PRELOAD, object_id, ANTISPAM_SOURCE_OBJECT, _NACL_AntiSpamSoundPreloadMulti) ||
		NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND_PRELOAD, owner_id, ANTISPAM_SOURCE_AGENT, _NACL_AntiSpamSoundPreloadMulti))
	{
		return;
	}
	// NaCl End

	// <FS:ND> Protect against corrupted sounds
	if( gAudiop->isCorruptSound( sound_id ) )
		return;
	// </FS:ND>

	LLViewerObject *objectp = gObjectList.findObject(object_id);
	if (!objectp) return;

	if (LLMuteList::getInstance()->isMuted(object_id)) return;
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	LLAudioSource *sourcep = objectp->getAudioSource(owner_id);
	if (!sourcep) return;
	
	LLAudioData *datap = gAudiop->getAudioData(sound_id);

	// Note that I don't actually do any loading of the
	// audio data into a buffer at this point, as it won't actually
	// help us out.

	// Don't play sounds from a region with maturity above current agent maturity
	LLVector3d pos_global = objectp->getPositionGlobal();
	if (gAgent.canAccessMaturityAtGlobal(pos_global))
	{
		// Add audioData starts a transfer internally.
		sourcep->addAudioData(datap, FALSE);
	}
}

void process_attached_sound(LLMessageSystem *msg, void **user_data)
{
	F32 gain = 0;
	LLUUID sound_id;
	LLUUID object_id;
	LLUUID owner_id;
	U8 flags;

	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_SoundID, sound_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_id);
	msg->getUUIDFast(_PREHASH_DataBlock, _PREHASH_OwnerID, owner_id);

	// <FS> Asset blacklist
	if (FSAssetBlacklist::getInstance()->isBlacklisted(sound_id, LLAssetType::AT_SOUND))
	{
		return;
	}
	// </FS>

	// NaCl - Antispam Registry
	static LLCachedControl<U32> _NACL_AntiSpamSoundMulti(gSavedSettings, "_NACL_AntiSpamSoundMulti");
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND, object_id, ANTISPAM_SOURCE_OBJECT, _NACL_AntiSpamSoundMulti) ||
		NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SOUND, owner_id, ANTISPAM_SOURCE_AGENT,_NACL_AntiSpamSoundMulti))
	{
		return;
	}
	// NaCl End

	msg->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);
	msg->getU8Fast(_PREHASH_DataBlock, _PREHASH_Flags, flags);

	LLViewerObject *objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		// we don't know about this object, just bail
		return;
	}
	
	if (LLMuteList::getInstance()->isMuted(object_id)) return;
	
	if (LLMuteList::getInstance()->isMuted(owner_id, LLMute::flagObjectSounds)) return;
	
	// Don't play sounds from a region with maturity above current agent maturity
	LLVector3d pos = objectp->getPositionGlobal();
	if( !gAgent.canAccessMaturityAtGlobal(pos) )
	{
		return;
	}
	
	objectp->setAttachedSound(sound_id, owner_id, gain, flags);
}


void process_attached_sound_gain_change(LLMessageSystem *mesgsys, void **user_data)
{
	F32 gain = 0;
	LLUUID object_guid;
	LLViewerObject *objectp = NULL;

	mesgsys->getUUIDFast(_PREHASH_DataBlock, _PREHASH_ObjectID, object_guid);

	if (!((objectp = gObjectList.findObject(object_guid))))
	{
		// we don't know about this object, just bail
		return;
	}

 	mesgsys->getF32Fast(_PREHASH_DataBlock, _PREHASH_Gain, gain);

	objectp->adjustAudioGain(gain);
}


void process_health_message(LLMessageSystem *mesgsys, void **user_data)
{
	F32 health;

	mesgsys->getF32Fast(_PREHASH_HealthData, _PREHASH_Health, health);

	if (gStatusBar)
	{
		gStatusBar->setHealth((S32)health);
	}
}


void process_sim_stats(LLMessageSystem *msg, void **user_data)
{	
	S32 count = msg->getNumberOfBlocks("Stat");
	for (S32 i = 0; i < count; ++i)
	{
		U32 stat_id;
		F32 stat_value;
		msg->getU32("Stat", "StatID", stat_id, i);
		msg->getF32("Stat", "StatValue", stat_value, i);
		LLStatViewer::SimMeasurementSampler* measurementp = LLStatViewer::SimMeasurementSampler::getInstance((ESimStatID)stat_id);
		
		if (measurementp )
		{
			measurementp->sample(stat_value);

			// <FS:Ansariel> Report script count changes
			if ((ESimStatID)stat_id == LL_SIM_STAT_NUMSCRIPTSACTIVE)
			{
				static LLCachedControl<bool> fsReportTotalScriptCountChanges(gSavedSettings, "FSReportTotalScriptCountChanges");
				static LLCachedControl<U32> fsReportTotalScriptCountChangesThreshold(gSavedSettings, "FSReportTotalScriptCountChangesThreshold");
				static const std::string increase_message = LLTrans::getString("TotalScriptCountChangeIncrease");
				static const std::string decrease_message = LLTrans::getString("TotalScriptCountChangeDecrease");
				static S32 prev_total_scripts = -1;

				if (fsReportTotalScriptCountChanges)
				{
					S32 new_val = (S32)stat_value;
					S32 change_count = new_val - prev_total_scripts;
					if (llabs(change_count) >= fsReportTotalScriptCountChangesThreshold && prev_total_scripts > -1)
					{
						LLStringUtil::format_map_t args;
						args["NEW_VALUE"] = llformat("%d", new_val);
						args["OLD_VALUE"] = llformat("%d", prev_total_scripts);
						args["DIFFERENCE"] = llformat("%+d", change_count);

						if (change_count > 0)
						{
							report_to_nearby_chat(format_string(increase_message, args));
						}
						else if (change_count < 0)
						{
							report_to_nearby_chat(format_string(decrease_message, args));
						}
					}
				}
				prev_total_scripts = (S32)stat_value;
			}
			// </FS:Ansariel>
		}
		else
		{
			// <FS:Ansariel> Cut down logspam
			//LL_WARNS() << "Unknown sim stat identifier: " << stat_id << LL_ENDL;
		}
	}

	//
	// Various hacks that aren't statistics, but are being handled here.
	//
	U32 max_tasks_per_region;
	U64 region_flags;
	msg->getU32("Region", "ObjectCapacity", max_tasks_per_region);

	if (msg->has(_PREHASH_RegionInfo))
	{
		msg->getU64("RegionInfo", "RegionFlagsExtended", region_flags);
	}
	else
	{
		U32 flags = 0;
		msg->getU32("Region", "RegionFlags", flags);
		region_flags = flags;
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		BOOL was_flying = gAgent.getFlying();
		regionp->setRegionFlags(region_flags);
		regionp->setMaxTasks(max_tasks_per_region);
		// HACK: This makes agents drop from the sky if the region is 
		// set to no fly while people are still in the sim.
		if (was_flying && regionp->getBlockFly())
		{
			gAgent.setFlying(gAgent.canFly());
		}
	}
}



void process_avatar_animation(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID	animation_id;
	LLUUID	uuid;
	S32		anim_sequence_id;
	LLVOAvatar *avatarp;
	
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	//clear animation flags
	avatarp = (LLVOAvatar *)gObjectList.findObject(uuid);

	if (!avatarp)
	{
		// no agent by this ID...error?
		LL_WARNS("Messaging") << "Received animation state for unknown avatar" << uuid << LL_ENDL;
		return;
	}

	S32 num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationList);
	S32 num_source_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_AnimationSourceList);

	avatarp->mSignaledAnimations.clear();
	
	if (avatarp->isSelf())
	{
		LLUUID object_id;

		for( S32 i = 0; i < num_blocks; i++ )
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);

			LL_DEBUGS("Messaging") << "Anim sequence ID: " << anim_sequence_id << LL_ENDL;

			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;

			// *HACK: Disabling flying mode if it has been enabled shortly before the agent
			// stand up animation is signaled. In this case we don't get a signal to start
			// flying animation from server, the AGENT_CONTROL_FLY flag remains set but the
			// avatar does not play flying animation, so we switch flying mode off.
			// See LLAgent::setFlying(). This may cause "Stop Flying" button to blink.
			// See EXT-2781.
			if (animation_id == ANIM_AGENT_STANDUP && gAgent.getFlying())
			{
				gAgent.setFlying(FALSE);
			}

			if (i < num_source_blocks)
			{
				mesgsys->getUUIDFast(_PREHASH_AnimationSourceList, _PREHASH_ObjectID, object_id, i);
			
				LLViewerObject* object = gObjectList.findObject(object_id);
				if (object)
				{
					object->setFlagsWithoutUpdate(FLAGS_ANIM_SOURCE, TRUE);

					BOOL anim_found = FALSE;
					LLVOAvatar::AnimSourceIterator anim_it = avatarp->mAnimationSources.find(object_id);
					for (;anim_it != avatarp->mAnimationSources.end(); ++anim_it)
					{
						if (anim_it->second == animation_id)
						{
							anim_found = TRUE;
							break;
						}
					}

					if (!anim_found)
					{
						avatarp->mAnimationSources.insert(LLVOAvatar::AnimationSourceMap::value_type(object_id, animation_id));
						// <FS:Zi> Animation Explorer
						if (avatarp == gAgentAvatarp)
						{
							RecentAnimationList::instance().addAnimation(animation_id, object_id);
						}
						// </FS:Zi>
					}
				}
			}
		}
	}
	else
	{
		for( S32 i = 0; i < num_blocks; i++ )
		{
			mesgsys->getUUIDFast(_PREHASH_AnimationList, _PREHASH_AnimID, animation_id, i);
			mesgsys->getS32Fast(_PREHASH_AnimationList, _PREHASH_AnimSequenceID, anim_sequence_id, i);
			avatarp->mSignaledAnimations[animation_id] = anim_sequence_id;
		}
	}

	if (num_blocks)
	{
		avatarp->processAnimationStateChanges();
	}
}

void process_avatar_appearance(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID uuid;
	mesgsys->getUUIDFast(_PREHASH_Sender, _PREHASH_ID, uuid);

	LLVOAvatar* avatarp = (LLVOAvatar *)gObjectList.findObject(uuid);
	if (avatarp)
	{
		avatarp->processAvatarAppearance( mesgsys );
	}
	else
	{
		LL_WARNS("Messaging") << "avatar_appearance sent for unknown avatar " << uuid << LL_ENDL;
	}
}

void process_camera_constraint(LLMessageSystem *mesgsys, void **user_data)
{
	//Freeing up the camera movement some more -KC
	if(gSavedSettings.getBOOL("FSIgnoreSimulatorCameraConstraints"))
		return;
	LLVector4 cameraCollidePlane;
	mesgsys->getVector4Fast(_PREHASH_CameraCollidePlane, _PREHASH_Plane, cameraCollidePlane);

	gAgentCamera.setCameraCollidePlane(cameraCollidePlane);
}

void near_sit_object(BOOL success, void *data)
{
	if (success)
	{
		// Send message to sit on object
		gMessageSystem->newMessageFast(_PREHASH_AgentSit);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();
	}
}

void process_avatar_sit_response(LLMessageSystem *mesgsys, void **user_data)
{
	LLVector3 sitPosition;
	LLQuaternion sitRotation;
	LLUUID sitObjectID;
	BOOL use_autopilot;
	mesgsys->getUUIDFast(_PREHASH_SitObject, _PREHASH_ID, sitObjectID);
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_AutoPilot, use_autopilot);
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_SitPosition, sitPosition);
	mesgsys->getQuatFast(_PREHASH_SitTransform, _PREHASH_SitRotation, sitRotation);
	LLVector3 camera_eye;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraEyeOffset, camera_eye);
	LLVector3 camera_at;
	mesgsys->getVector3Fast(_PREHASH_SitTransform, _PREHASH_CameraAtOffset, camera_at);
	BOOL force_mouselook;
	mesgsys->getBOOLFast(_PREHASH_SitTransform, _PREHASH_ForceMouselook, force_mouselook);

	if (isAgentAvatarValid() && dist_vec_squared(camera_eye, camera_at) > CAMERA_POSITION_THRESHOLD_SQUARED)
	{
		gAgentCamera.setSitCamera(sitObjectID, camera_eye, camera_at);
	}
	
	gAgentCamera.setForceMouselook(force_mouselook);
	// Forcing turning off flying here to prevent flying after pressing "Stand"
	// to stand up from an object. See EXT-1655.
	gAgent.setFlying(FALSE);

	LLViewerObject* object = gObjectList.findObject(sitObjectID);
	if (object)
	{
		LLVector3 sit_spot = object->getPositionAgent() + (sitPosition * object->getRotation());
		if (!use_autopilot || (isAgentAvatarValid() && gAgentAvatarp->isSitting() && gAgentAvatarp->getRoot() == object->getRoot()))
		{
			//we're already sitting on this object, so don't autopilot
		}
		else
		{
			gAgent.startAutoPilotGlobal(gAgent.getPosGlobalFromAgent(sit_spot), "Sit", &sitRotation, near_sit_object, NULL, 0.5f);
		}
	}
	else
	{
		LL_WARNS("Messaging") << "Received sit approval for unknown object " << sitObjectID << LL_ENDL;
	}
}

void process_clear_follow_cam_properties(LLMessageSystem *mesgsys, void **user_data)
{
	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLFollowCamMgr::removeFollowCamParams(source_id);
}

void process_set_follow_cam_properties(LLMessageSystem *mesgsys, void **user_data)
{
	S32			type;
	F32			value;
	bool		settingPosition = false;
	bool		settingFocus	= false;
	bool		settingFocusOffset = false;
	LLVector3	position;
	LLVector3	focus;
	LLVector3	focus_offset;

	LLUUID		source_id;

	mesgsys->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, source_id);

	LLViewerObject* objectp = gObjectList.findObject(source_id);
	if (objectp)
	{
		objectp->setFlagsWithoutUpdate(FLAGS_CAMERA_SOURCE, TRUE);
	}

	S32 num_objects = mesgsys->getNumberOfBlocks("CameraProperty");
	for (S32 block_index = 0; block_index < num_objects; block_index++)
	{
		mesgsys->getS32("CameraProperty", "Type", type, block_index);
		mesgsys->getF32("CameraProperty", "Value", value, block_index);
		switch(type)
		{
		case FOLLOWCAM_PITCH:
			LLFollowCamMgr::setPitch(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_OFFSET_X:
			focus_offset.mV[VX] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Y:
			focus_offset.mV[VY] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_FOCUS_OFFSET_Z:
			focus_offset.mV[VZ] = value;
			settingFocusOffset = true;
			break;
		case FOLLOWCAM_POSITION_LAG:
			LLFollowCamMgr::setPositionLag(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_LAG:
			LLFollowCamMgr::setFocusLag(source_id, value);
			break;
		case FOLLOWCAM_DISTANCE:
			LLFollowCamMgr::setDistance(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_ANGLE:
			LLFollowCamMgr::setBehindnessAngle(source_id, value);
			break;
		case FOLLOWCAM_BEHINDNESS_LAG:
			LLFollowCamMgr::setBehindnessLag(source_id, value);
			break;
		case FOLLOWCAM_POSITION_THRESHOLD:
			LLFollowCamMgr::setPositionThreshold(source_id, value);
			break;
		case FOLLOWCAM_FOCUS_THRESHOLD:
			LLFollowCamMgr::setFocusThreshold(source_id, value);
			break;
		case FOLLOWCAM_ACTIVE:
			//if 1, set using followcam,. 
			LLFollowCamMgr::setCameraActive(source_id, value != 0.f);
			break;
		case FOLLOWCAM_POSITION_X:
			settingPosition = true;
			position.mV[ 0 ] = value;
			break;
		case FOLLOWCAM_POSITION_Y:
			settingPosition = true;
			position.mV[ 1 ] = value;
			break;
		case FOLLOWCAM_POSITION_Z:
			settingPosition = true;
			position.mV[ 2 ] = value;
			break;
		case FOLLOWCAM_FOCUS_X:
			settingFocus = true;
			focus.mV[ 0 ] = value;
			break;
		case FOLLOWCAM_FOCUS_Y:
			settingFocus = true;
			focus.mV[ 1 ] = value;
			break;
		case FOLLOWCAM_FOCUS_Z:
			settingFocus = true;
			focus.mV[ 2 ] = value;
			break;
		case FOLLOWCAM_POSITION_LOCKED:
			LLFollowCamMgr::setPositionLocked(source_id, value != 0.f);
			break;
		case FOLLOWCAM_FOCUS_LOCKED:
			LLFollowCamMgr::setFocusLocked(source_id, value != 0.f);
			break;

		default:
			break;
		}
	}

	if ( settingPosition )
	{
		LLFollowCamMgr::setPosition(source_id, position);
	}
	if ( settingFocus )
	{
		LLFollowCamMgr::setFocus(source_id, focus);
	}
	if ( settingFocusOffset )
	{
		LLFollowCamMgr::setFocusOffset(source_id, focus_offset);
	}
}
//end Ventrella 


// Culled from newsim lltask.cpp
void process_name_value(LLMessageSystem *mesgsys, void **user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Added to object Name Value: " << temp_str << LL_ENDL;
			object->addNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to add name value pair" << LL_ENDL;
	}
}

void process_remove_name_value(LLMessageSystem *mesgsys, void **user_data)
{
	std::string	temp_str;
	LLUUID	id;
	S32		i, num_blocks;

	mesgsys->getUUIDFast(_PREHASH_TaskData, _PREHASH_ID, id);

	LLViewerObject* object = gObjectList.findObject(id);

	if (object)
	{
		num_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_NameValueData);
		for (i = 0; i < num_blocks; i++)
		{
			mesgsys->getStringFast(_PREHASH_NameValueData, _PREHASH_NVPair, temp_str, i);
			LL_INFOS("Messaging") << "Removed from object Name Value: " << temp_str << LL_ENDL;
			object->removeNVPair(temp_str);
		}
	}
	else
	{
		LL_INFOS("Messaging") << "Can't find object " << id << " to remove name value pair" << LL_ENDL;
	}
}

void process_kick_user(LLMessageSystem *msg, void** /*user_data*/)
{
	std::string message;

	msg->getStringFast(_PREHASH_UserInfo, _PREHASH_Reason, message);

	LLAppViewer::instance()->forceDisconnect(message);
}


/*
void process_user_list_reply(LLMessageSystem *msg, void **user_data)
{
	LLUserList::processUserListReply(msg, user_data);
	return;
	char	firstname[MAX_STRING+1];
	char	lastname[MAX_STRING+1];
	U8		status;
	S32		user_count;

	user_count = msg->getNumberOfBlocks("UserBlock");

	for (S32 i = 0; i < user_count; i++)
	{
		msg->getData("UserBlock", i, "FirstName", firstname);
		msg->getData("UserBlock", i, "LastName", lastname);
		msg->getData("UserBlock", i, "Status", &status);

		if (status & 0x01)
		{
			dialog_friends_add_friend(buffer, TRUE);
		}
		else
		{
			dialog_friends_add_friend(buffer, FALSE);
		}
	}

	dialog_friends_done_adding();
}
*/

// this is not handled in processUpdateMessage
/*
void process_time_dilation(LLMessageSystem *msg, void **user_data)
{
	// get the time_dilation
	U16 foo;
	msg->getData("TimeDilation", "TimeDilation", &foo);
	F32 time_dilation = ((F32) foo) / 65535.f;

	// get the pointer to the right region
	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();
	LLViewerRegion *regionp = LLWorld::getInstance()->getRegion(ip, port);
	if (regionp)
	{
		regionp->setTimeDilation(time_dilation);
	}
}
*/


void process_money_balance_reply( LLMessageSystem* msg, void** )
{
	S32 balance = 0;
	S32 credit = 0;
	S32 committed = 0;
	std::string desc;
	LLUUID tid;

	msg->getUUID("MoneyData", "TransactionID", tid);
	msg->getS32("MoneyData", "MoneyBalance", balance);
	msg->getS32("MoneyData", "SquareMetersCredit", credit);
	msg->getS32("MoneyData", "SquareMetersCommitted", committed);
	msg->getStringFast(_PREHASH_MoneyData, _PREHASH_Description, desc);
// <FS:AW opensim currency support>
//	LL_INFOS("Messaging") << "L$, credit, committed: " << balance << " " << credit << " "
	LL_INFOS("Messaging") << Tea::wrapCurrency("L$, credit, committed: ") << balance << " " << credit << " "
// <FS:AW opensim currency support>
			<< committed << LL_ENDL;

    
	if (gStatusBar)
	{
		gStatusBar->setBalance(balance);
		gStatusBar->setLandCredit(credit);
		gStatusBar->setLandCommitted(committed);
	}

	if (desc.empty()
		|| !gSavedSettings.getBOOL("NotifyMoneyChange"))
	{
		// ...nothing to display
		return;
	}

	// Suppress duplicate messages about the same transaction
	static std::deque<LLUUID> recent;
	if (std::find(recent.rbegin(), recent.rend(), tid) != recent.rend())
	{
		return;
	}

	// Once the 'recent' container gets large enough, chop some
	// off the beginning.
	const U32 MAX_LOOKBACK = 30;
	const S32 POP_FRONT_SIZE = 12;
	if(recent.size() > MAX_LOOKBACK)
	{
		LL_DEBUGS("Messaging") << "Removing oldest transaction records" << LL_ENDL;
		recent.erase(recent.begin(), recent.begin() + POP_FRONT_SIZE);
	}
	//LL_DEBUGS("Messaging") << "Pushing back transaction " << tid << LL_ENDL;
	recent.push_back(tid);

	if (msg->has("TransactionInfo"))
	{
		// ...message has extended info for localization
		process_money_balance_reply_extended(msg);
	}
	else
	{
		// Only old dev grids will not supply the TransactionInfo block,
		// so we can just use the hard-coded English string.
		LLSD args;
		args["MESSAGE"] = desc;
		LLNotificationsUtil::add("SystemMessage", args);
	}
}

static std::string reason_from_transaction_type(S32 transaction_type,
												const std::string& item_desc)
{
	// *NOTE: The keys for the reason strings are unusual because
	// an earlier version of the code used English language strings
	// extracted from hard-coded server English descriptions.
	// Keeping them so we don't have to re-localize them.
	switch (transaction_type)
	{
		case TRANS_OBJECT_SALE:
		{
			LLStringUtil::format_map_t arg;
			arg["ITEM"] = item_desc;
			return LLTrans::getString("for item", arg);
		}
		case TRANS_LAND_SALE:
			return LLTrans::getString("for a parcel of land");
			
		case TRANS_LAND_PASS_SALE:
			return LLTrans::getString("for a land access pass");
			
		case TRANS_GROUP_LAND_DEED:
			return LLTrans::getString("for deeding land");
			
		case TRANS_GROUP_CREATE:
			return LLTrans::getString("to create a group");
			
		case TRANS_GROUP_JOIN:
			return LLTrans::getString("to join a group");
			
		case TRANS_UPLOAD_CHARGE:
			return LLTrans::getString("to upload");

		case TRANS_CLASSIFIED_CHARGE:
			return LLTrans::getString("to publish a classified ad");
			
		case TRANS_GIFT:
			// Simulator returns "Payment" if no custom description has been entered
			return (item_desc == "Payment" ? std::string() : item_desc);

		// These have no reason to display, but are expected and should not
		// generate warnings
		case TRANS_PAY_OBJECT:
		case TRANS_OBJECT_PAYS:
			return std::string();

		default:
			LL_WARNS() << "Unknown transaction type " 
				<< transaction_type << LL_ENDL;
			return std::string();
	}
}

static void money_balance_group_notify(const LLUUID& group_id,
									   const std::string& name,
									   bool is_group,
									   std::string notification,
									   LLSD args,
									   LLSD payload)
{
	static LLCachedControl<bool> balance_change_in_chat(gSavedSettings, "FSPaymentInfoInChat");

	if (balance_change_in_chat)
	{
		LLChat chat;
		chat.mText = args["SLURLMESSAGE"].asString();
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLSD chat_args;
		chat_args["money_tracker"] = true;
		chat_args["console_message"] = llformat(args["MESSAGE"].asString().c_str(), name.c_str());
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, chat_args);
	}
	else
	{
		// Message uses name SLURLs, don't actually have to substitute in
		// the name.  We're just making sure it's available.
		// Notification is either PaymentReceived or PaymentSent
		LLNotificationsUtil::add(notification, args, payload);
	}
}

static void money_balance_avatar_notify(const LLUUID& agent_id,
										const LLAvatarName& av_name,
									   	std::string notification,
									   	LLSD args,
									   	LLSD payload)
{
	static LLCachedControl<bool> balance_change_in_chat(gSavedSettings, "FSPaymentInfoInChat");

	if (balance_change_in_chat)
	{
		LLChat chat;
		chat.mText = args["SLURLMESSAGE"].asString();
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		LLSD chat_args;
		chat_args["money_tracker"] = true;
		chat_args["console_message"] = llformat(args["MESSAGE"].asString().c_str(), av_name.getCompleteName().c_str());
		LLNotificationsUI::LLNotificationManager::instance().onChat(chat, chat_args);
	}
	else
	{
		// Message uses name SLURLs, don't actually have to substitute in
		// the name.  We're just making sure it's available.
		// Notification is either PaymentReceived or PaymentSent
		LLNotificationsUtil::add(notification, args, payload);
	}
}

static void process_money_balance_reply_extended(LLMessageSystem* msg)
{
    // Added in server 1.40 and viewer 2.1, support for localization
    // and agent ids for name lookup.
    S32 transaction_type = 0;
    LLUUID source_id;
	BOOL is_source_group = FALSE;
    LLUUID dest_id;
	BOOL is_dest_group = FALSE;
    S32 amount = 0;
    std::string item_description;
	BOOL success = FALSE;
	// <FS:Ansariel> If we output to chat history and probably console,
	//               don't create an SLURL for the name or we will end
	//               up with a SLURL in the console
	static LLCachedControl<bool> balance_change_in_chat(gSavedSettings, "FSPaymentInfoInChat");

    msg->getS32("TransactionInfo", "TransactionType", transaction_type);
    msg->getUUID("TransactionInfo", "SourceID", source_id);
	msg->getBOOL("TransactionInfo", "IsSourceGroup", is_source_group);
    msg->getUUID("TransactionInfo", "DestID", dest_id);
	msg->getBOOL("TransactionInfo", "IsDestGroup", is_dest_group);
    msg->getS32("TransactionInfo", "Amount", amount);
    msg->getString("TransactionInfo", "ItemDescription", item_description);
	msg->getBOOL("MoneyData", "TransactionSuccess", success);
    LL_INFOS("Money") << "MoneyBalanceReply source " << source_id 
		<< " dest " << dest_id
		<< " type " << transaction_type
		<< " item " << item_description << LL_ENDL;

	if (source_id.isNull() && dest_id.isNull())
	{
		// this is a pure balance update, no notification required
		return;
	}

	std::string source_slurl;
	std::string source_slurl_name;
	if (is_source_group)
	{
		source_slurl =
			LLSLURL( "group", source_id, "inspect").getSLURLString();
		source_slurl_name = source_slurl;
	}
	else
	{
		source_slurl_name =LLSLURL( "agent", source_id, "completename").getSLURLString();
		source_slurl =LLSLURL( "agent", source_id, "inspect").getSLURLString();
	}

	std::string dest_slurl;
	std::string dest_slurl_name;
	if (is_dest_group)
	{
		dest_slurl =
			LLSLURL( "group", dest_id, "inspect").getSLURLString();
		dest_slurl_name = dest_slurl;
	}
	else
	{
		dest_slurl_name = LLSLURL( "agent", dest_id, "completename").getSLURLString();
		dest_slurl = LLSLURL( "agent", dest_id, "inspect").getSLURLString();
	}

	std::string reason =
		reason_from_transaction_type(transaction_type, item_description);
	
	LLStringUtil::format_map_t args;
	args["REASON"] = reason; // could be empty
	args["AMOUNT"] = llformat("%d", amount);
	
	// Need to delay until name looked up, so need to know whether or not
	// is group
	bool is_name_group = false;
	LLUUID name_id;
	std::string message;
	std::string message_notification_well;
	std::string notification;
	LLSD final_args;
	LLSD payload;
	
	bool you_paid_someone = (source_id == gAgentID);
	std::string gift_suffix = (transaction_type == TRANS_GIFT ? "_gift" : "");
	if (you_paid_someone)
	{
		if(!gSavedSettings.getBOOL("NotifyMoneySpend"))
		{
			return;
		}
		args["NAME"] = balance_change_in_chat ? "%s" : dest_slurl;
		is_name_group = is_dest_group;
		name_id = dest_id;
		if (!reason.empty())
		{
			if (dest_id.notNull())
			{
				message = success ? LLTrans::getString("you_paid_ldollars" + gift_suffix, args) :
									LLTrans::getString("you_paid_failure_ldollars" + gift_suffix, args);
			}
			else
			{
				// transaction fee to the system, eg, to create a group
				message = success ? LLTrans::getString("you_paid_ldollars_no_name", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_name", args);
			}
		}
		else
		{
			if (dest_id.notNull())
			{
				message = success ? LLTrans::getString("you_paid_ldollars_no_reason", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_reason", args);
			}
			else
			{
				// no target, no reason, you just paid money
				message = success ? LLTrans::getString("you_paid_ldollars_no_info", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_info", args);
			}
		}
		
		final_args["MESSAGE"] = message;
		payload["dest_id"] = dest_id;
		notification = success ? "PaymentSent" : "PaymentFailure";

		// <FS:AO> Additionally, always add a SLURL-enabled form.
		is_name_group = is_dest_group;
		name_id = dest_id;

		args["NAME"] = dest_slurl;
		if (!reason.empty())
		{
			if (dest_id.notNull())
			{
				message = success ? LLTrans::getString("you_paid_ldollars" + gift_suffix, args) :
									LLTrans::getString("you_paid_failure_ldollars" + gift_suffix, args);
			}
			else
			{
				// transaction fee to the system, eg, to create a group
				message = success ? LLTrans::getString("you_paid_ldollars_no_name", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_name", args);
			}
		}
		else
		{
			if (dest_id.notNull())
			{
				message = success ? LLTrans::getString("you_paid_ldollars_no_reason", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_reason", args);
			}
			else
			{
				// no target, no reason, you just paid money
				message = success ? LLTrans::getString("you_paid_ldollars_no_info", args) :
									LLTrans::getString("you_paid_failure_ldollars_no_info", args);
			}
		}
		final_args["SLURLMESSAGE"] = message;
		payload["dest_id"] = dest_id;

		args["NAME"] = dest_slurl_name;
		if (!reason.empty())
		{
			if (dest_id.notNull())
			{
				message_notification_well = success ? LLTrans::getString("you_paid_ldollars" + gift_suffix, args) :
													  LLTrans::getString("you_paid_failure_ldollars" + gift_suffix, args);
			}
			else
			{
				// transaction fee to the system, eg, to create a group
				message_notification_well = success ? LLTrans::getString("you_paid_ldollars_no_name", args) :
													  LLTrans::getString("you_paid_failure_ldollars_no_name", args);
			}
		}
		else
		{
			if (dest_id.notNull())
			{
				message_notification_well = success ? LLTrans::getString("you_paid_ldollars_no_reason", args) :
													  LLTrans::getString("you_paid_failure_ldollars_no_reason", args);
			}
			else
			{
				// no target, no reason, you just paid money
				message_notification_well = success ? LLTrans::getString("you_paid_ldollars_no_info", args) :
													  LLTrans::getString("you_paid_failure_ldollars_no_info", args);
			}
		}
	}
	else 
	{
		// ...someone paid you
		if(!gSavedSettings.getBOOL("NotifyMoneyReceived"))
		{
			return;
		}
		args["NAME"] = balance_change_in_chat ? "%s" : source_slurl;
		is_name_group = is_source_group;
		name_id = source_id;
		if (!reason.empty())
		{
			message = LLTrans::getString("paid_you_ldollars" + gift_suffix, args);
		}
		else
		{
			message = LLTrans::getString("paid_you_ldollars_no_reason", args);
		}
		final_args["MESSAGE"] = message;
		
		// make notification loggable
		payload["from_id"] = source_id;
		notification = "PaymentReceived";
		
		// <FS:AO> Additionally, always add a SLURL-enabled form.
		is_name_group = is_source_group;
		name_id = source_id;

		args["NAME"] = source_slurl;
		if (!reason.empty())
		{
			message = LLTrans::getString("paid_you_ldollars", args);
		}
		else 
		{
			message = LLTrans::getString("paid_you_ldollars_no_reason", args);
		}
		final_args["SLURLMESSAGE"] = message;

		args["NAME"] = source_slurl_name;
		if (!reason.empty())
		{
			message_notification_well = LLTrans::getString("paid_you_ldollars", args);
		}
		else 
		{
			message_notification_well = LLTrans::getString("paid_you_ldollars_no_reason", args);
		}
		// </FS:AO>
	}

	payload["payment_is_group"] = is_name_group;
	payload["payment_message"] = message_notification_well;

	// <FS:Ansariel> TipTracker Support
	FSMoneyTracker* tipTracker = LLFloaterReg::getTypedInstance<FSMoneyTracker>("money_tracker");
	if ((tipTracker->isShown() || tipTracker->isMinimized()) || gSavedSettings.getBOOL("FSAlwaysTrackPayments"))
	{
		tipTracker->addPayment(name_id, is_name_group, amount, !you_paid_someone);
	}
	// </FS:Ansariel>>

	// Despite using SLURLs, wait until the name is available before
	// showing the notification, otherwise the UI layout is strange and
	// the user sees a "Loading..." message
	if (is_name_group)
	{
		gCacheName->getGroup(name_id,
						boost::bind(&money_balance_group_notify,
									_1, _2, _3,
									notification, final_args, payload));
	}
	else 
	{
		LLAvatarNameCache::get(name_id, boost::bind(&money_balance_avatar_notify, _1, _2, notification, final_args, payload));										   
	}
}

bool handle_prompt_for_maturity_level_change_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		// set the preference to the maturity of the region we're calling
		U8 preferredMaturity = static_cast<U8>(notification["payload"]["_region_access"].asInteger());
		gSavedSettings.setU32("PreferredMaturity", static_cast<U32>(preferredMaturity));
	}

	return false;
}

bool handle_prompt_for_maturity_level_change_and_reteleport_callback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	
	if (0 == option)
	{
		// set the preference to the maturity of the region we're calling
		U8 preferredMaturity = static_cast<U8>(notification["payload"]["_region_access"].asInteger());
		gSavedSettings.setU32("PreferredMaturity", static_cast<U32>(preferredMaturity));
		gAgent.setMaturityRatingChangeDuringTeleport(preferredMaturity);
		gAgent.restartFailedTeleportRequest();
	}
	else
	{
		gAgent.clearTeleportRequest();
	}
	
	return false;
}

// some of the server notifications need special handling. This is where we do that.
bool handle_special_notification(std::string notificationID, LLSD& llsdBlock)
{
	bool returnValue = false;
	if(llsdBlock.has("_region_access"))
	{
		U8 regionAccess = static_cast<U8>(llsdBlock["_region_access"].asInteger());
		std::string regionMaturity = LLViewerRegion::accessToString(regionAccess);
		LLStringUtil::toLower(regionMaturity);
		llsdBlock["REGIONMATURITY"] = regionMaturity;
		LLNotificationPtr maturityLevelNotification;
		std::string notifySuffix = "_Notify";
		if (regionAccess == SIM_ACCESS_MATURE)
		{
			if (gAgent.isTeen())
			{
				gAgent.clearTeleportRequest();
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_AdultsOnlyContent", llsdBlock);
				returnValue = true;

				notifySuffix = "_NotifyAdultsOnly";
			}
			else if (gAgent.prefersPG())
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
				returnValue = true;
			}
			else if (LLStringUtil::compareStrings(notificationID, "RegionEntryAccessBlocked") == 0)
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_PreferencesOutOfSync", llsdBlock, llsdBlock);
				returnValue = true;
			}
		}
		else if (regionAccess == SIM_ACCESS_ADULT)
		{
			if (!gAgent.isAdult())
			{
				gAgent.clearTeleportRequest();
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_AdultsOnlyContent", llsdBlock);
				returnValue = true;

				notifySuffix = "_NotifyAdultsOnly";
			}
			else if (gAgent.prefersPG() || gAgent.prefersMature())
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
				returnValue = true;
			}
			else if (LLStringUtil::compareStrings(notificationID, "RegionEntryAccessBlocked") == 0)
			{
				maturityLevelNotification = LLNotificationsUtil::add(notificationID+"_PreferencesOutOfSync", llsdBlock, llsdBlock);
				returnValue = true;
			}
		}

		if ((maturityLevelNotification == NULL) || maturityLevelNotification->isIgnored())
		{
			// Given a simple notification if no maturityLevelNotification is set or it is ignore
			LLNotificationsUtil::add(notificationID + notifySuffix, llsdBlock);
		}
	}

	return returnValue;
}

bool handle_trusted_experiences_notification(const LLSD& llsdBlock)
{
	if(llsdBlock.has("trusted_experiences"))
	{
		std::ostringstream str;
		const LLSD& experiences = llsdBlock["trusted_experiences"];
		LLSD::array_const_iterator it = experiences.beginArray();
		for(/**/; it != experiences.endArray(); ++it)
		{
			str<<LLSLURL("experience", it->asUUID(), "profile").getSLURLString() << "\n";
		}
		std::string str_list = str.str();
		if(!str_list.empty())
		{
			LLNotificationsUtil::add("TrustedExperiencesAvailable", LLSD::emptyMap().with("EXPERIENCE_LIST", (LLSD)str_list));
			return true;
		}
	}
	return false;
}

// some of the server notifications need special handling. This is where we do that.
bool handle_teleport_access_blocked(LLSD& llsdBlock, const std::string & notificationID, const std::string & defaultMessage)
{
	bool returnValue = false;
	if(llsdBlock.has("_region_access"))
	{
		U8 regionAccess = static_cast<U8>(llsdBlock["_region_access"].asInteger());
		std::string regionMaturity = LLViewerRegion::accessToString(regionAccess);
		LLStringUtil::toLower(regionMaturity);
		llsdBlock["REGIONMATURITY"] = regionMaturity;

		LLNotificationPtr tp_failure_notification;
		std::string notifySuffix;

		if (notificationID == std::string("TeleportEntryAccessBlocked"))
		{
			notifySuffix = "_Notify";
			if (regionAccess == SIM_ACCESS_MATURE)
			{
				if (gAgent.isTeen())
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID+"_AdultsOnlyContent", llsdBlock);
					returnValue = true;

					notifySuffix = "_NotifyAdultsOnly";
				}
				else if (gAgent.prefersPG())
				{
					if (gAgent.hasRestartableFailedTeleportRequest())
					{
						tp_failure_notification = LLNotificationsUtil::add(notificationID+"_ChangeAndReTeleport", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_and_reteleport_callback);
						returnValue = true;
					}
					else
					{
						gAgent.clearTeleportRequest();
						tp_failure_notification = LLNotificationsUtil::add(notificationID+"_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
						returnValue = true;
					}
				}
				else
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID+"_PreferencesOutOfSync", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
					returnValue = true;
				}
			}
			else if (regionAccess == SIM_ACCESS_ADULT)
			{
				if (!gAgent.isAdult())
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID+"_AdultsOnlyContent", llsdBlock);
					returnValue = true;

					notifySuffix = "_NotifyAdultsOnly";
				}
				else if (gAgent.prefersPG() || gAgent.prefersMature())
				{
					if (gAgent.hasRestartableFailedTeleportRequest())
					{
						tp_failure_notification = LLNotificationsUtil::add(notificationID+"_ChangeAndReTeleport", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_and_reteleport_callback);
						returnValue = true;
					}
					else
					{
						gAgent.clearTeleportRequest();
						tp_failure_notification = LLNotificationsUtil::add(notificationID+"_Change", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
						returnValue = true;
					}
				}
				else
				{
					gAgent.clearTeleportRequest();
					tp_failure_notification = LLNotificationsUtil::add(notificationID+"_PreferencesOutOfSync", llsdBlock, llsdBlock, handle_prompt_for_maturity_level_change_callback);
					returnValue = true;
				}
			}
		}		// End of special handling for "TeleportEntryAccessBlocked"
		else
		{	// Normal case, no message munging
			gAgent.clearTeleportRequest();
			if (LLNotifications::getInstance()->templateExists(notificationID))
			{
				tp_failure_notification = LLNotificationsUtil::add(notificationID, llsdBlock, llsdBlock);
			}
			else
			{
				llsdBlock["MESSAGE"] = defaultMessage;
				tp_failure_notification = LLNotificationsUtil::add("GenericAlertOK", llsdBlock);
			}
			returnValue = true;
		}

		if ((tp_failure_notification == NULL) || tp_failure_notification->isIgnored())
		{
			// Given a simple notification if no tp_failure_notification is set or it is ignore
			LLNotificationsUtil::add(notificationID + notifySuffix, llsdBlock);
		}
	}

	handle_trusted_experiences_notification(llsdBlock);
	return returnValue;
}

bool attempt_standard_notification(LLMessageSystem* msgsystem)
{
	// if we have additional alert data
	if (msgsystem->has(_PREHASH_AlertInfo) && msgsystem->getNumberOfBlocksFast(_PREHASH_AlertInfo) > 0)
	{
		// notification was specified using the new mechanism, so we can just handle it here
		std::string notificationID;
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, notificationID);
		if (!LLNotifications::getInstance()->templateExists(notificationID))
		{
			return false;
		}

		// <FS:Ansariel> FIRE-12004: Attachments getting lost on TP; Not clear if these messages are actually
		//               used if a region crossing timeout happens and if this is the correct place to handle
		//               them, but it seems the most likely way it would probably happen and I don't have a
		//               borked region at hand for testing.
		if (notificationID == "expired_region_handoff" || notificationID == "invalid_region_handoff")
		{
			LL_WARNS("Messaging") << "Region crossing failed. Resetting region crossing state." << LL_ENDL;
			if (isAgentAvatarValid())
			{
				gAgentAvatarp->setIsCrossingRegion(false);
			}
		}
		// </FS:Ansariel>

		std::string llsdRaw;
		LLSD llsdBlock;
		// <FS:Ansariel> Remove dupe call
		//msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, notificationID);
		msgsystem->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsdRaw);
		if (llsdRaw.length())
		{
			std::istringstream llsdData(llsdRaw);
			if (!LLSDSerialize::deserialize(llsdBlock, llsdData, llsdRaw.length()))
			{
				LL_WARNS() << "attempt_standard_notification: Attempted to read notification parameter data into LLSD but failed:" << llsdRaw << LL_ENDL;
			}
		}


		handle_trusted_experiences_notification(llsdBlock);
		
		if (
			(notificationID == "RegionEntryAccessBlocked") ||
			(notificationID == "LandClaimAccessBlocked") ||
			(notificationID == "LandBuyAccessBlocked")

		   )
		{
			/*---------------------------------------------------------------------
			 (Commented so a grep will find the notification strings, since
			 we construct them on the fly; if you add additional notifications,
			 please update the comment.)
			 
			 Could throw any of the following notifications:
			 
				RegionEntryAccessBlocked
				RegionEntryAccessBlocked_Notify
				RegionEntryAccessBlocked_NotifyAdultsOnly
				RegionEntryAccessBlocked_Change
				RegionEntryAccessBlocked_AdultsOnlyContent
				RegionEntryAccessBlocked_ChangeAndReTeleport
				LandClaimAccessBlocked 
				LandClaimAccessBlocked_Notify 
				LandClaimAccessBlocked_NotifyAdultsOnly
				LandClaimAccessBlocked_Change 
				LandClaimAccessBlocked_AdultsOnlyContent 
				LandBuyAccessBlocked
				LandBuyAccessBlocked_Notify
				LandBuyAccessBlocked_NotifyAdultsOnly
				LandBuyAccessBlocked_Change
				LandBuyAccessBlocked_AdultsOnlyContent
			 
			-----------------------------------------------------------------------*/ 
			if (handle_special_notification(notificationID, llsdBlock))
			{
				return true;
			}
		}
		// HACK -- handle callbacks for specific alerts.
		if( notificationID == "HomePositionSet" )
		{
			// save the home location image to disk
			std::string snap_filename = gDirUtilp->getLindenUserDir();
			snap_filename += gDirUtilp->getDirDelimiter();
			snap_filename += SCREEN_HOME_FILENAME;
			gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(), FALSE, FALSE);
		}
		
		if (notificationID == "RegionRestartMinutes" ||
			notificationID == "RegionRestartSeconds")
		{
			S32 seconds;
			if (notificationID == "RegionRestartMinutes")
			{
				seconds = 60 * static_cast<S32>(llsdBlock["MINUTES"].asInteger());
			}
			else
			{
				seconds = static_cast<S32>(llsdBlock["SECONDS"].asInteger());
			}

			// <FS:Ansariel> Optional new region restart notification
			if (!gSavedSettings.getBOOL("FSUseNewRegionRestartNotification"))
			{
				notificationID += "Toast";
			}
			else
			{
			// </FS:Ansariel>
			LLFloaterRegionRestarting* floaterp = LLFloaterReg::findTypedInstance<LLFloaterRegionRestarting>("region_restarting");

			if (floaterp)
			{
				LLFloaterRegionRestarting::updateTime(seconds);
			}
			else
			{
				LLSD params;
				params["NAME"] = llsdBlock["NAME"];
				params["SECONDS"] = (LLSD::Integer)seconds;
				LLFloaterRegionRestarting* restarting_floater = dynamic_cast<LLFloaterRegionRestarting*>(LLFloaterReg::showInstance("region_restarting", params));
				if(restarting_floater)
				{
					restarting_floater->center();
				}
			}
			// <FS:Ansariel> Optional new region restart notification
			}
			// </FS:Ansariel>

			make_ui_sound("UISndRestart");
			report_to_nearby_chat(LLTrans::getString("FSRegionRestartInLocalChat")); // <FS:PP> FIRE-6307: Region restart notices in local chat
		}

		// <FS:Ansariel> FIRE-9858: Kill annoying "Autopilot canceled" toast
		if (notificationID == "AutopilotCanceled")
		{
			return true;
		}
		// </FS:Ansariel>
// <FS:CR> FIRE-9696 - Moved detection of HomePositionSet Alert hack to here where it's actually found now
		if (notificationID == "HomePositionSet")
		{
			// save the home location image to disk
			std::string snap_filename = gDirUtilp->getLindenUserDir();
			snap_filename += gDirUtilp->getDirDelimiter();
			snap_filename += SCREEN_HOME_FILENAME;
			if (gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(), FALSE, FALSE))
			{
				LL_INFOS() << SCREEN_HOME_FILENAME << " saved successfully." << LL_ENDL;
			}
			else
			{
				LL_WARNS() << SCREEN_HOME_FILENAME << " could not be saved." << LL_ENDL;
			}
		}
// </FS:CR>
        
        // Special Marketplace update notification
		if (notificationID == "SLM_UPDATE_FOLDER")
        {
            std::string state = llsdBlock["state"].asString();
            if (state == "deleted")
            {
                // Perform the deletion viewer side, no alert shown in this case
                LLMarketplaceData::instance().deleteListing(llsdBlock["listing_id"].asInteger());
                return true;
            }
            else
            {
                // In general, no message will be displayed, all we want is to get the listing updated in the marketplace floater
                // If getListing() fails though, the message of the alert will be shown by the caller of attempt_standard_notification()
                return LLMarketplaceData::instance().getListing(llsdBlock["listing_id"].asInteger());
            }
        }
        
		LLNotificationsUtil::add(notificationID, llsdBlock);
		return true;
	}	
	return false;
}


static void process_special_alert_messages(const std::string & message)
{
	// Do special handling for alert messages.   This is a legacy hack, and any actual displayed
	// text should be altered in the notifications.xml files.
	if ( message == "You died and have been teleported to your home location")
	{
		add(LLStatViewer::KILLED, 1);
	}
	else if( message == "Home position set." )
	{
		// save the home location image to disk
		std::string snap_filename = gDirUtilp->getLindenUserDir();
		snap_filename += gDirUtilp->getDirDelimiter();
		snap_filename += SCREEN_HOME_FILENAME;
		gViewerWindow->saveSnapshot(snap_filename, gViewerWindow->getWindowWidthRaw(), gViewerWindow->getWindowHeightRaw(), FALSE, FALSE);
	}
}



void process_agent_alert_message(LLMessageSystem* msgsystem, void** user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	
	std::string message;
	msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, message);

	process_special_alert_messages(message);

	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		msgsystem->getBOOL("AlertData", "Modal", modal);
		process_alert_core(message, modal);
	}
}

// The only difference between this routine and the previous is the fact that
// for this routine, the modal parameter is always false. Sadly, for the message
// handled by this routine, there is no "Modal" parameter on the message, and
// there's no API to tell if a message has the given parameter or not.
// So we can't handle the messages with the same handler.
void process_alert_message(LLMessageSystem *msgsystem, void **user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
		
	std::string message;
	msgsystem->getStringFast(_PREHASH_AlertData, _PREHASH_Message, message);
	process_special_alert_messages(message);

	if (!attempt_standard_notification(msgsystem))
	{
		BOOL modal = FALSE;
		process_alert_core(message, modal);
	}
}

bool handle_not_age_verified_alert(const std::string &pAlertName)
{
	LLNotificationPtr notification = LLNotificationsUtil::add(pAlertName);
	if ((notification == NULL) || notification->isIgnored())
	{
		LLNotificationsUtil::add(pAlertName + "_Notify");
	}

	return true;
}

bool handle_special_alerts(const std::string &pAlertName)
{
	bool isHandled = false;
	if (LLStringUtil::compareStrings(pAlertName, "NotAgeVerified") == 0)
	{
		
		isHandled = handle_not_age_verified_alert(pAlertName);
	}

	return isHandled;
}

void process_alert_core(const std::string& message, BOOL modal)
{
	const std::string ALERT_PREFIX("ALERT: ");
	const std::string NOTIFY_PREFIX("NOTIFY: ");
	if (message.find(ALERT_PREFIX) == 0)
	{
		// Allow the server to spawn a named alert so that server alerts can be
		// translated out of English.
		std::string alert_name(message.substr(ALERT_PREFIX.length()));
		if (!handle_special_alerts(alert_name))
		{
		LLNotificationsUtil::add(alert_name);
	}
	}
	else if (message.find(NOTIFY_PREFIX) == 0)
	{
		// Allow the server to spawn a named notification so that server notifications can be
		// translated out of English.
		std::string notify_name(message.substr(NOTIFY_PREFIX.length()));
		LLNotificationsUtil::add(notify_name);
	}
	else if (message[0] == '/')
	{
		// System message is important, show in upper-right box not tip
		std::string text(message.substr(1));
		LLSD args;

		// <FS:Ansariel> Let's hope this works for OpenSim...
		bool is_region_restart = false;
		S32 seconds = 0;
		if (text.substr(0,17) == "RESTART_X_MINUTES")
		{
			S32 mins = 0;
			LLStringUtil::convertToS32(text.substr(18), mins);
			seconds = mins * 60;
			is_region_restart = true;

			if (!gSavedSettings.getBOOL("FSUseNewRegionRestartNotification"))
			{
				LLSD args;
				args["MINUTES"] = llformat("%d", mins);
				LLNotificationsUtil::add("RegionRestartMinutesToast", args);
			}
		}
		else if (text.substr(0,17) == "RESTART_X_SECONDS")
		{
			LLStringUtil::convertToS32(text.substr(18), seconds);
			is_region_restart = true;

			if (!gSavedSettings.getBOOL("FSUseNewRegionRestartNotification"))
			{
				LLSD args;
				args["SECONDS"] = llformat("%d", seconds);
				LLNotificationsUtil::add("RegionRestartSecondsToast", args);
			}
		}
		if (is_region_restart)
		{
			if (gSavedSettings.getBOOL("FSUseNewRegionRestartNotification"))
			{
				LLFloaterRegionRestarting* floaterp = LLFloaterReg::findTypedInstance<LLFloaterRegionRestarting>("region_restarting");

				if (floaterp)
				{
					LLFloaterRegionRestarting::updateTime(seconds);
				}
				else
				{
					std::string region_name;
					if (gAgent.getRegion())
					{
						region_name = gAgent.getRegion()->getName();
					}
					else
					{
						region_name = LLTrans::getString("Unknown");
					}
					LLSD params;
					params["NAME"] = region_name;
					params["SECONDS"] = (LLSD::Integer)seconds;
					LLFloaterRegionRestarting* restarting_floater = dynamic_cast<LLFloaterRegionRestarting*>(LLFloaterReg::showInstance("region_restarting", params));
					if(restarting_floater)
					{
						restarting_floater->center();
					}
				}
			}

			make_ui_sound("UISndRestartOpenSim");
			report_to_nearby_chat(LLTrans::getString("FSRegionRestartInLocalChat")); // <FS:PP> FIRE-6307: Region restart notices in local chat
			return;
		}
		// </FS:Ansariel>

		// *NOTE: If the text from the server ever changes this line will need to be adjusted.
		std::string restart_cancelled = "Region restart cancelled.";
		if (text.substr(0, restart_cancelled.length()) == restart_cancelled)
		{
			LLFloaterRegionRestarting::close();
		}

			std::string new_msg =LLNotifications::instance().getGlobalString(text);
// [RLVa:KB] - Checked: RLVa-1.4.5
			if ( (new_msg == text) && (RlvActions::isRlvEnabled()) )
			{
				if (!RlvActions::canShowLocation())
					RlvUtil::filterLocation(new_msg);
				if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT))
					RlvUtil::filterNames(new_msg);
			}
// [/RLVa:KB]
			args["MESSAGE"] = new_msg;
			LLNotificationsUtil::add("SystemMessage", args);
		}
	else if (modal)
	{
		LLSD args;
		std::string new_msg =LLNotifications::instance().getGlobalString(message);
// [RLVa:KB] - Checked: RLVa-1.4.5
		if ( (new_msg == message) && (RlvActions::isRlvEnabled()) )
		{
			if (!RlvActions::canShowLocation())
				RlvUtil::filterLocation(new_msg);
			if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT))
				RlvUtil::filterNames(new_msg);
		}
// [/RLVa:KB]
		args["ERROR_MESSAGE"] = new_msg;
		LLNotificationsUtil::add("ErrorMessage", args);
	}
	else
	{
		// Hack fix for EXP-623 (blame fix on RN :)) to avoid a sim deploy
		const std::string AUTOPILOT_CANCELED_MSG("Autopilot canceled");
		if (message.find(AUTOPILOT_CANCELED_MSG) == std::string::npos )
		{
			LLSD args;
			std::string new_msg =LLNotifications::instance().getGlobalString(message);

			std::string localized_msg;
			bool is_message_localized = LLTrans::findString(localized_msg, new_msg);

// [RLVa:KB] - Checked: RLVa-1.4.5
			if ( (new_msg == message) && (RlvActions::isRlvEnabled()) )
			{
				if (!RlvActions::canShowLocation())
					RlvUtil::filterLocation(new_msg);
				if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT))
					RlvUtil::filterNames(new_msg);
			}
// [/RLVa:KB]

			args["MESSAGE"] = is_message_localized ? localized_msg : new_msg;
			LLNotificationsUtil::add("SystemMessageTip", args);
		}
	}
}

mean_collision_list_t				gMeanCollisionList;
time_t								gLastDisplayedTime = 0;

void handle_show_mean_events(void *)
{
	LLFloaterReg::showInstance("bumps");
	//LLFloaterBump::showInstance();
}

void mean_name_callback(const LLUUID &id, const std::string& full_name, bool is_group)
{
	static const U32 max_collision_list_size = 20;
	if (gMeanCollisionList.size() > max_collision_list_size)
	{
		mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
		for (U32 i=0; i<max_collision_list_size; i++) iter++;
		for_each(iter, gMeanCollisionList.end(), DeletePointer());
		gMeanCollisionList.erase(iter, gMeanCollisionList.end());
	}

	for (mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
		 iter != gMeanCollisionList.end(); ++iter)
	{
		LLMeanCollisionData *mcd = *iter;
		if (mcd->mPerp == id)
		{
			mcd->mFullName = full_name;
		}
	}
	// <FS:Ansariel> Instant bump list floater update
	LLFloaterBump* floater = LLFloaterReg::findTypedInstance<LLFloaterBump>("bumps");
	if (floater)
	{
		floater->setDirty();
	}
	// </FS:Ansariel>
}

void process_mean_collision_alert_message(LLMessageSystem *msgsystem, void **user_data)
{
	if (gAgent.inPrelude())
	{
		// In prelude, bumping is OK.  This dialog is rather confusing to 
		// newbies, so we don't show it.  Drop the packet on the floor.
		return;
	}

	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();

	LLUUID perp;
	U32	   time;
	U8	   u8type;
	EMeanCollisionType	   type;
	F32    mag;

	S32 i, num = msgsystem->getNumberOfBlocks(_PREHASH_MeanCollision);

	for (i = 0; i < num; i++)
	{
		msgsystem->getUUIDFast(_PREHASH_MeanCollision, _PREHASH_Perp, perp);
		msgsystem->getU32Fast(_PREHASH_MeanCollision, _PREHASH_Time, time);
		msgsystem->getF32Fast(_PREHASH_MeanCollision, _PREHASH_Mag, mag);
		msgsystem->getU8Fast(_PREHASH_MeanCollision, _PREHASH_Type, u8type);

		type = (EMeanCollisionType)u8type;

		// <FS:Ansariel> Nearby Chat Collision Messages
		if (gSavedSettings.getBOOL("FSCollisionMessagesInChat"))
		{
			std::string action;
			LLStringUtil::format_map_t args;
			args["NAME"] = llformat("secondlife:///app/agent/%s/inspect", perp.asString().c_str());

			switch (type)
			{
				case MEAN_BUMP:
					action = LLTrans::getString("Collision_Bump", args);
					break;
				case MEAN_LLPUSHOBJECT:
					action = LLTrans::getString("Collision_PushObject", args);
					break;
				case MEAN_SELECTED_OBJECT_COLLIDE:
					action = LLTrans::getString("Collision_ObjectCollide", args);
					break;
				case MEAN_SCRIPTED_OBJECT_COLLIDE:
					action = LLTrans::getString("Collision_ScriptedObject", args);
					break;
				case MEAN_PHYSICAL_OBJECT_COLLIDE:
					action = LLTrans::getString("Collision_PhysicalObject", args);
					break;
				default:
					action = LLTrans::getString("Collision_UnknownType", args);
					return;
			}
			report_to_nearby_chat(action);
		}
		// </FS:Ansariel> Nearby Chat Collision Messages
		// <FS:Ansariel> Report Collision Messages to scripts
		if (gSavedSettings.getBOOL("FSReportCollisionMessages"))
		{
			std::string collision_data = llformat("%s,%.2f,%d", perp.asString().c_str(), mag, u8type);

			LLMessageSystem* msgs = gMessageSystem;
			msgs->newMessage(_PREHASH_ScriptDialogReply);
			msgs->nextBlock(_PREHASH_AgentData);
			msgs->addUUID(_PREHASH_AgentID, gAgent.getID());
			msgs->addUUID(_PREHASH_SessionID, gAgent.getSessionID());
			msgs->nextBlock(_PREHASH_Data);
			msgs->addUUID(_PREHASH_ObjectID, gAgent.getID());
			msgs->addS32(_PREHASH_ChatChannel, gSavedSettings.getS32("FSReportCollisionMessagesChannel"));
			msgs->addS32(_PREHASH_ButtonIndex, 1);
			msgs->addString(_PREHASH_ButtonLabel, collision_data.c_str());
			gAgent.sendReliableMessage();
		}
		// </FS:Ansariel> Report Collision Messages to scripts

		BOOL b_found = FALSE;

		for (mean_collision_list_t::iterator iter = gMeanCollisionList.begin();
			 iter != gMeanCollisionList.end(); ++iter)
		{
			LLMeanCollisionData *mcd = *iter;
			if ((mcd->mPerp == perp) && (mcd->mType == type))
			{
				mcd->mTime = time;
				mcd->mMag = mag;
				b_found = TRUE;
				break;
			}
		}

		if (!b_found)
		{
			LLMeanCollisionData *mcd = new LLMeanCollisionData(gAgentID, perp, time, type, mag);
			gMeanCollisionList.push_front(mcd);
			gCacheName->get(perp, false, boost::bind(&mean_name_callback, _1, _2, _3));
		}
		// <FS:Ansariel> Instant bump list floater update
		else
		{
			LLFloaterBump* floater = LLFloaterReg::findTypedInstance<LLFloaterBump>("bumps");
			if (floater)
			{
				floater->setDirty();
			}
		}
		// </FS:Ansariel>
	}
	// <FS:Ansariel> Instant bump list floater update
	//LLFloaterBump* bumps_floater = LLFloaterBump::getInstance();
	//if(bumps_floater && bumps_floater->isInVisibleChain())
	//{
	//	bumps_floater->populateCollisionList();
	//}
	// </FS:Ansariel>
}

void process_frozen_message(LLMessageSystem *msgsystem, void **user_data)
{
	// make sure the cursor is back to the usual default since the
	// alert is probably due to some kind of error.
	gViewerWindow->getWindow()->resetBusyCount();
	BOOL b_frozen;
	
	msgsystem->getBOOL("FrozenData", "Data", b_frozen);

	// TODO: make being frozen change view
	if (b_frozen)
	{
	}
	else
	{
	}
}

// do some extra stuff once we get our economy data
void process_economy_data(LLMessageSystem *msg, void** /*user_data*/)
{
	LLGlobalEconomy::processEconomyData(msg, LLGlobalEconomy::getInstance());
	S32 upload_cost = LLGlobalEconomy::getInstance()->getPriceUpload();
// <FS:AW opensim currency support> 
// AW: from this point anything is bogus because it's all replaced by the LLUploadCostCalculator in llviewermenu
//	LL_INFOS_ONCE("Messaging") << "EconomyData message arrived; upload cost is L$" << upload_cost << LL_ENDL;
// 	gMenuHolder->getChild<LLUICtrl>("Upload Image")->setLabelArg("[COST]", llformat("%d", upload_cost));
// 	gMenuHolder->getChild<LLUICtrl>("Upload Sound")->setLabelArg("[COST]", llformat("%d", upload_cost));
// 	gMenuHolder->getChild<LLUICtrl>("Upload Animation")->setLabelArg("[COST]", llformat("%d", upload_cost));
// 	gMenuHolder->getChild<LLUICtrl>("Bulk Upload")->setLabelArg("[COST]", llformat("%d", upload_cost));
	std::string cost_str;
#ifdef OPENSIM
	if (LLGridManager::getInstance()->isInOpenSim())
	{
		cost_str = upload_cost > 0 ? llformat("%s%d", "L$", upload_cost) : LLTrans::getString("free");
	}
	else
#endif
	{
		cost_str = "L$" + (upload_cost > 0 ? llformat("%d", upload_cost) : llformat("%d", gSavedSettings.getU32("DefaultUploadCost")));
	}

	LL_INFOS_ONCE("Messaging") << Tea::wrapCurrency("EconomyData message arrived; upload cost is L$") << cost_str << LL_ENDL;

	gMenuHolder->getChild<LLUICtrl>("Upload Image")->setLabelArg("[COST]",  cost_str);
	gMenuHolder->getChild<LLUICtrl>("Upload Sound")->setLabelArg("[COST]",  cost_str);
	gMenuHolder->getChild<LLUICtrl>("Upload Animation")->setLabelArg("[COST]", cost_str);
	gMenuHolder->getChild<LLUICtrl>("Bulk Upload")->setLabelArg("[COST]", cost_str);
// <FS:AW opensim currency support>

	// update L$ substitution for "Buy and Sell L$", it was set before we knew the currency
	gMenuHolder->getChild<LLUICtrl>("Buy and Sell L$")->setLabelArg("L$", LLStringExplicit("L$"));
}

void notify_cautioned_script_question(const LLSD& notification, const LLSD& response, S32 orig_questions, BOOL granted)
{
	// only continue if at least some permissions were requested
	if (orig_questions)
	{
		// check to see if the person we are asking

		// "'[OBJECTNAME]', an object owned by '[OWNERNAME]', 
		// located in [REGIONNAME] at [REGIONPOS], 
		// has been <granted|denied> permission to: [PERMISSIONS]."

		LLUIString notice(LLTrans::getString(granted ? "ScriptQuestionCautionChatGranted" : "ScriptQuestionCautionChatDenied"));

		// always include the object name and owner name 
		notice.setArg("[OBJECTNAME]", notification["payload"]["object_name"].asString());
		notice.setArg("[OWNERNAME]", notification["payload"]["owner_name"].asString());

		// try to lookup viewerobject that corresponds to the object that
		// requested permissions (here, taskid->requesting object id)
		BOOL foundpos = FALSE;
		LLViewerObject* viewobj = gObjectList.findObject(notification["payload"]["task_id"].asUUID());
		if (viewobj)
		{
			// found the viewerobject, get it's position in its region
			LLVector3 objpos(viewobj->getPosition());
			
			// try to lookup the name of the region the object is in
			LLViewerRegion* viewregion = viewobj->getRegion();
			if (viewregion)
			{
				// got the region, so include the region and 3d coordinates of the object
				notice.setArg("[REGIONNAME]", viewregion->getName());
				std::string formatpos = llformat("%.1f, %.1f,%.1f", objpos[VX], objpos[VY], objpos[VZ]);
				notice.setArg("[REGIONPOS]", formatpos);

				foundpos = TRUE;
			}
		}

// [RLVa:KB] - Checked: 2010-04-23 (RLVa-1.2.0g) | Modified: RLVa-1.0.0a
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			notice.setArg("[REGIONNAME]", RlvStrings::getString(RLV_STRING_HIDDEN_REGION));
			notice.setArg("[REGIONPOS]", RlvStrings::getString(RLV_STRING_HIDDEN));
		}
		else if (!foundpos)
// [/RLVa:KB]
//		if (!foundpos)
		{
			// unable to determine location of the object
			// <FS:Ansariel> Made hardcoded strings localizable
			//notice.setArg("[REGIONNAME]", "(unknown region)");
			//notice.setArg("[REGIONPOS]", "(unknown position)");
			notice.setArg("[REGIONNAME]", LLTrans::getString("UnknownRegion"));
			notice.setArg("[REGIONPOS]", LLTrans::getString("UnknownPosition"));
			// </FS:Ansariel>
		}

		// check each permission that was requested, and list each 
		// permission that has been flagged as a caution permission
		BOOL caution = FALSE;
		S32 count = 0;
		std::string perms;
		BOOST_FOREACH(script_perm_t script_perm, SCRIPT_PERMISSIONS)
		{
//			if ((orig_questions & script_perm.permbit)
//				&& script_perm.caution)
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
			if ((orig_questions & script_perm.permbit)
				&& ((script_perm.caution) || (notification["payload"]["rlv_notify"].asBoolean())) )
// [/RLVa:KB]
			{
				count++;
				caution = TRUE;

				// add a comma before the permission description if it is not the first permission
				// added to the list or the last permission to check
				if (count > 1)
				{
					perms.append(", ");
				}

				perms.append(LLTrans::getString(script_perm.question));
			}
		}

		notice.setArg("[PERMISSIONS]", perms);

		// log a chat message as long as at least one requested permission
		// is a caution permission
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
		if (caution)
		{
			// <FS:Ansariel> [FS communication UI]
			//LLFloaterIMNearbyChat* nearby_chat = LLFloaterReg::getTypedInstance<LLFloaterIMNearbyChat>("nearby_chat");
			FSFloaterNearbyChat* nearby_chat = FSFloaterNearbyChat::getInstance();
			// </FS:Ansariel> [FS communication UI]
			if(nearby_chat)
			{
				LLChat chat_msg(notice.getString());
				chat_msg.mFromName = SYSTEM_FROM;
				chat_msg.mFromID = LLUUID::null;
				chat_msg.mSourceType = CHAT_SOURCE_SYSTEM;
				nearby_chat->addMessage(chat_msg);
			}
		}
// [/RLVa:KB]
//		if (caution)
//		{
//			LLChat chat(notice.getString());
//	//		LLFloaterChat::addChat(chat, FALSE, FALSE);
//		}
	}
}

void script_question_mute(const LLUUID& item_id, const std::string& object_name);

bool unknown_script_question_cb(const LLSD& notification, const LLSD& response)
{
	// Only care if they muted the object here.
	if ( response["Mute"] ) // mute
	{
		LLUUID task_id = notification["payload"]["task_id"].asUUID();
		script_question_mute(task_id,notification["payload"]["object_name"].asString());
	}
	return false;
}

void experiencePermissionBlock(LLUUID experience, LLSD result)
{
    LLSD permission;
    LLSD data;
    permission["permission"] = "Block";
    data[experience.asString()] = permission;
    data["experience"] = experience;
    LLEventPumps::instance().obtain("experience_permission").post(data);
}

bool script_question_cb(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
	LLMessageSystem *msg = gMessageSystem;
	S32 orig = notification["payload"]["questions"].asInteger();
	S32 new_questions = orig;

	if (response["Details"])
	{
		// respawn notification...
		LLNotificationsUtil::add(notification["name"], notification["substitutions"], notification["payload"]);

		// ...with description on top
		LLNotificationsUtil::add("DebitPermissionDetails");
		return false;
	}

	LLUUID experience;
	if(notification["payload"].has("experience"))
	{
		experience = notification["payload"]["experience"].asUUID();
	}

	// check whether permissions were granted or denied
	BOOL allowed = TRUE;
	// the "yes/accept" button is the first button in the template, making it button 0
	// if any other button was clicked, the permissions were denied
	if (option != 0)
	{
		new_questions = 0;
		allowed = FALSE;
	}	
	else if(experience.notNull())
	{
		LLSD permission;
		LLSD data;
		permission["permission"]="Allow";

		data[experience.asString()]=permission;
		data["experience"]=experience;
		LLEventPumps::instance().obtain("experience_permission").post(data);
	}

	LLUUID task_id = notification["payload"]["task_id"].asUUID();
	LLUUID item_id = notification["payload"]["item_id"].asUUID();

	// reply with the permissions granted or denied
	msg->newMessageFast(_PREHASH_ScriptAnswerYes);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_Data);
	msg->addUUIDFast(_PREHASH_TaskID, task_id);
	msg->addUUIDFast(_PREHASH_ItemID, item_id);
	msg->addS32Fast(_PREHASH_Questions, new_questions);
	msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));

	// only log a chat message if caution prompts are enabled
//	if (gSavedSettings.getBOOL("PermissionsCautionEnabled"))
// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
	if ( (gSavedSettings.getBOOL("PermissionsCautionEnabled")) || (notification["payload"]["rlv_notify"].asBoolean()) )
// [/RLVa:KB]
	{
		// log a chat message, if appropriate
		notify_cautioned_script_question(notification, response, orig, allowed);
	}

// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
	if ( (allowed) && (notification["payload"].has("rlv_blocked")) )
	{
		RlvUtil::notifyBlocked(notification["payload"]["rlv_blocked"], LLSD().with("OBJECT", notification["payload"]["object_name"]));
	}
// [/RLVa:KB]

	if ( response["Mute"] ) // mute
	{
		script_question_mute(task_id,notification["payload"]["object_name"].asString());
	}
	if ( response["BlockExperience"] )
	{
		if(experience.notNull())
		{
			LLViewerRegion* region = gAgent.getRegion();
			if (!region)
			    return false;

            LLExperienceCache::instance().setExperiencePermission(experience, std::string("Block"), boost::bind(&experiencePermissionBlock, experience, _1));

		}
}
	return false;
}

void script_question_mute(const LLUUID& task_id, const std::string& object_name)
{
	LLMuteList::getInstance()->add(LLMute(task_id, object_name, LLMute::OBJECT));

    // purge the message queue of any previously queued requests from the same source. DEV-4879
    class OfferMatcher : public LLNotificationsUI::LLScreenChannel::Matcher
    {
    public:
    	OfferMatcher(const LLUUID& to_block) : blocked_id(to_block) {}
      	bool matches(const LLNotificationPtr notification) const
        {
            if (notification->getName() == "ScriptQuestionCaution"
                || notification->getName() == "ScriptQuestion"
				|| notification->getName() == "UnknownScriptQuestion")
            {
                return (notification->getPayload()["task_id"].asUUID() == blocked_id);
            }
            return false;
        }
    private:
        const LLUUID& blocked_id;
    };

    LLNotificationsUI::LLChannelManager::getInstance()->killToastsFromChannel(LLUUID(
            gSavedSettings.getString("NotificationChannelUUID")), OfferMatcher(task_id));
}

static LLNotificationFunctorRegistration script_question_cb_reg_1("ScriptQuestion", script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_2("ScriptQuestionCaution", script_question_cb);
static LLNotificationFunctorRegistration script_question_cb_reg_3("ScriptQuestionExperience", script_question_cb);
static LLNotificationFunctorRegistration unknown_script_question_cb_reg("UnknownScriptQuestion", unknown_script_question_cb);

void process_script_experience_details(const LLSD& experience_details, LLSD args, LLSD payload)
{
	if(experience_details[LLExperienceCache::PROPERTIES].asInteger() & LLExperienceCache::PROPERTY_GRID)
	{
		args["GRID_WIDE"] = LLTrans::getString("Grid-Scope");
	}
	else
	{
		args["GRID_WIDE"] = LLTrans::getString("Land-Scope");
	}
	args["EXPERIENCE"] = LLSLURL("experience", experience_details[LLExperienceCache::EXPERIENCE_ID].asUUID(), "profile").getSLURLString();

	LLNotificationsUtil::add("ScriptQuestionExperience", args, payload);
	make_ui_sound("UISndQuestionExperience"); // <FS:PP> New Experience notification sound
}

void process_script_question(LLMessageSystem *msg, void **user_data)
{
	// *TODO: Translate owner name -> [FIRST] [LAST]

	LLHost sender = msg->getSender();

	LLUUID taskid;
	LLUUID itemid;
	S32		questions;
	std::string object_name;
	std::string owner_name;
	LLUUID experienceid;

	// taskid -> object key of object requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_TaskID, taskid );
	// itemid -> script asset key of script requesting permissions
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_ItemID, itemid );

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SCRIPT_DIALOG, taskid, ANTISPAM_SOURCE_OBJECT))
	{
		return;
	}
	// NaCl End

	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectName, object_name);
	msg->getStringFast(_PREHASH_Data, _PREHASH_ObjectOwner, owner_name);
	msg->getS32Fast(_PREHASH_Data, _PREHASH_Questions, questions );

	if(msg->has(_PREHASH_Experience))
	{
		msg->getUUIDFast(_PREHASH_Experience, _PREHASH_ExperienceID, experienceid);
	}

	// Special case. If the objects are owned by this agent, throttle per-object instead
	// of per-owner. It's common for residents to reset a ton of scripts that re-request
	// permissions, as with tier boxes. UUIDs can't be valid agent names and vice-versa,
	// so we'll reuse the same namespace for both throttle types.
	std::string throttle_name = owner_name;
	std::string self_name;
	LLAgentUI::buildFullname( self_name );
	if( owner_name == self_name )
	{
		throttle_name = taskid.getString();
	}
	
	// don't display permission requests if this object is muted
	if (LLMuteList::getInstance()->isMuted(taskid)) return;
	
	// throttle excessive requests from any specific user's scripts
	typedef LLKeyThrottle<std::string> LLStringThrottle;
	static LLStringThrottle question_throttle( LLREQUEST_PERMISSION_THROTTLE_LIMIT, LLREQUEST_PERMISSION_THROTTLE_INTERVAL );

	switch (question_throttle.noteAction(throttle_name))
	{
		case LLStringThrottle::THROTTLE_NEWLY_BLOCKED:
			LL_INFOS("Messaging") << "process_script_question throttled"
					<< " owner_name:" << owner_name
					<< LL_ENDL;
			// Fall through

		case LLStringThrottle::THROTTLE_BLOCKED:
			// Escape altogether until we recover
			return;

		case LLStringThrottle::THROTTLE_OK:
			break;
	}

	std::string script_question;
	if (questions)
	{
		bool caution = false;
		S32 count = 0;
		LLSD args;
		args["OBJECTNAME"] = object_name;
		args["NAME"] = LLCacheName::cleanFullName(owner_name);
		S32 known_questions = 0;
		bool has_not_only_debit = questions ^ SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_DEBIT].permbit;
		// check the received permission flags against each permission
		BOOST_FOREACH(script_perm_t script_perm, SCRIPT_PERMISSIONS)
		{
			if (questions & script_perm.permbit)
			{
				count++;
				known_questions |= script_perm.permbit;
				// check whether permission question should cause special caution dialog
				caution |= (script_perm.caution);

				if (("ScriptTakeMoney" == script_perm.question) && has_not_only_debit)
					continue;

                if (script_perm.question == "JoinAnExperience")
                { // Some experience only permissions do not have an explicit permission bit.  Add them here.
                    script_question += "    " + LLTrans::getString("ForceSitAvatar") + "\n";
                }

				script_question += "    " + LLTrans::getString(script_perm.question) + "\n";
			}
		}
	
		args["QUESTIONS"] = script_question;

		if (known_questions != questions)
		{	// This is in addition to the normal dialog.
			LLSD payload;
			payload["task_id"] = taskid;
			payload["item_id"] = itemid;
			payload["object_name"] = object_name;
			
			args["DOWNLOADURL"] = LLTrans::getString("ViewerDownloadURL");
			LLNotificationsUtil::add("UnknownScriptQuestion",args,payload);
			make_ui_sound("UISndScriptFloaterOpen"); // <FS:PP> FIRE-16958: Incoming script permission request doesn't trigger a sound
		}
		
		if (known_questions)
		{
			LLSD payload;
			payload["task_id"] = taskid;
			payload["item_id"] = itemid;
			payload["sender"] = sender.getIPandPort();
			payload["questions"] = known_questions;
			payload["object_name"] = object_name;
			payload["owner_name"] = owner_name;

// [RLVa:KB] - Checked: 2012-07-28 (RLVa-1.4.7)
			if (rlv_handler_t::isEnabled())
			{
				RlvUtil::filterScriptQuestions(questions, payload);

				if ( (questions) && (gRlvHandler.hasBehaviour(RLV_BHVR_ACCEPTPERMISSION)) )
				{
					const LLViewerObject* pObj = gObjectList.findObject(taskid);
					if (pObj)
					{
						if ( (pObj->permYouOwner()) && (!pObj->isAttachment()) )
						{
							questions &= ~(SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_TAKE_CONTROLS].permbit | 
								SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_ATTACH].permbit);
						}
						else
						{
							questions &= ~(SCRIPT_PERMISSIONS[SCRIPT_PERMISSION_TAKE_CONTROLS].permbit);
						}
						payload["rlv_notify"] = !pObj->permYouOwner();
					}
				}
			}

			if ( (!caution) && (!questions) && (experienceid.isNull()) )
			{
				LLNotifications::instance().forceResponse(
					LLNotification::Params("ScriptQuestion").substitutions(args).payload(payload), 0/*YES*/);
				return;
			}
// [/RLVa:KB]

			// check whether cautions are even enabled or not
			const char* notification = "ScriptQuestion";

			if(caution && gSavedSettings.getBOOL("PermissionsCautionEnabled"))
			{
				args["FOOTERTEXT"] = (count > 1) ? LLTrans::getString("AdditionalPermissionsRequestHeader") + "\n\n" + script_question : "";
				notification = "ScriptQuestionCaution";
			}
			else if(experienceid.notNull())
			{
				payload["experience"]=experienceid;
                LLExperienceCache::instance().get(experienceid, boost::bind(process_script_experience_details, _1, args, payload));
				return;
			}

			LLNotificationsUtil::add(notification, args, payload);
			make_ui_sound("UISndScriptFloaterOpen"); // <FS:PP> FIRE-16958: Incoming script permission request doesn't trigger a sound
		}
	}
}


void process_derez_container(LLMessageSystem *msg, void**)
{
	LL_WARNS("Messaging") << "call to deprecated process_derez_container" << LL_ENDL;
}

void container_inventory_arrived(LLViewerObject* object,
								 LLInventoryObject::object_list_t* inventory,
								 S32 serial_num,
								 void* data)
{
	LL_DEBUGS("Messaging") << "container_inventory_arrived()" << LL_ENDL;
	if( gAgentCamera.cameraMouselook() )
	{
		gAgentCamera.changeCameraToDefault();
	}

	LLInventoryPanel *active_panel = LLInventoryPanel::getActiveInventoryPanel();

	if (inventory->size() > 2)
	{
		// create a new inventory category to put this in
		LLUUID cat_id;
		cat_id = gInventory.createNewCategory(gInventory.getRootFolderID(),
											  LLFolderType::FT_NONE,
											  LLTrans::getString("AcquiredItems"));

		LLInventoryObject::object_list_t::const_iterator it = inventory->begin();
		LLInventoryObject::object_list_t::const_iterator end = inventory->end();
		for ( ; it != end; ++it)
		{
			if ((*it)->getType() != LLAssetType::AT_CATEGORY)
			{
				LLInventoryObject* obj = (LLInventoryObject*)(*it);
				LLInventoryItem* item = (LLInventoryItem*)(obj);
				LLUUID item_id;
				item_id.generate();
				time_t creation_date_utc = time_corrected();
				LLPointer<LLViewerInventoryItem> new_item
					= new LLViewerInventoryItem(item_id,
												cat_id,
												item->getPermissions(),
												item->getAssetUUID(),
												item->getType(),
												item->getInventoryType(),
												item->getName(),
												item->getDescription(),
												LLSaleInfo::DEFAULT,
												item->getFlags(),
												creation_date_utc);
				new_item->updateServer(TRUE);
				gInventory.updateItem(new_item);
			}
		}
		gInventory.notifyObservers();
		if(active_panel)
		{
			active_panel->setSelection(cat_id, TAKE_FOCUS_NO);
		}
	}
	else if (inventory->size() == 2)
	{
		// we're going to get one fake root category as well as the
		// one actual object
		LLInventoryObject::object_list_t::iterator it = inventory->begin();

		if ((*it)->getType() == LLAssetType::AT_CATEGORY)
		{
			++it;
		}

		LLInventoryItem* item = (LLInventoryItem*)((LLInventoryObject*)(*it));
		const LLUUID category = gInventory.findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(item->getType()));

		LLUUID item_id;
		item_id.generate();
		time_t creation_date_utc = time_corrected();
		LLPointer<LLViewerInventoryItem> new_item
			= new LLViewerInventoryItem(item_id, category,
										item->getPermissions(),
										item->getAssetUUID(),
										item->getType(),
										item->getInventoryType(),
										item->getName(),
										item->getDescription(),
										LLSaleInfo::DEFAULT,
										item->getFlags(),
										creation_date_utc);
		new_item->updateServer(TRUE);
		gInventory.updateItem(new_item);
		gInventory.notifyObservers();
		if(active_panel)
		{
			active_panel->setSelection(item_id, TAKE_FOCUS_NO);
		}
	}

	// we've got the inventory, now delete this object if this was a take
	BOOL delete_object = (BOOL)(intptr_t)data;
	LLViewerRegion *region = gAgent.getRegion();
	if (delete_object && region)
	{
		gMessageSystem->newMessageFast(_PREHASH_ObjectDelete);
		gMessageSystem->nextBlockFast(_PREHASH_AgentData);
		gMessageSystem->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		gMessageSystem->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		const U8 NO_FORCE = 0;
		gMessageSystem->addU8Fast(_PREHASH_Force, NO_FORCE);
		gMessageSystem->nextBlockFast(_PREHASH_ObjectData);
		gMessageSystem->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		gMessageSystem->sendReliable(region->getHost());
	}
}

// method to format the time.
std::string formatted_time(const time_t& the_time)
{
	std::string dateStr = "["+LLTrans::getString("LTimeWeek")+"] ["
						+LLTrans::getString("LTimeMonth")+"] ["
						+LLTrans::getString("LTimeDay")+"] ["
						+LLTrans::getString("LTimeHour")+"]:["
						+LLTrans::getString("LTimeMin")+"]:["
						+LLTrans::getString("LTimeSec")+"] ["
						+LLTrans::getString("LTimeYear")+"]";

	LLSD substitution;
	substitution["datetime"] = (S32) the_time;
	LLStringUtil::format (dateStr, substitution);
	return dateStr;
}


void process_teleport_failed(LLMessageSystem *msg, void**)
{
	std::string message_id;		// Tag from server, like "RegionEntryAccessBlocked"
	std::string big_reason;		// Actual message to display
	LLSD args;

	// Let the interested parties know that teleport failed.
	LLViewerParcelMgr::getInstance()->onTeleportFailed();

	// if we have additional alert data
	if (msg->has(_PREHASH_AlertInfo) && msg->getSizeFast(_PREHASH_AlertInfo, _PREHASH_Message) > 0)
	{
		// Get the message ID
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_Message, message_id);
		big_reason = LLAgent::sTeleportErrorMessages[message_id];
		if ( big_reason.size() > 0 )
		{	// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{	// Nothing found in the map - use what the server returned in the original message block
			msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, big_reason);
			args["REASON"] = big_reason;
		}

		LLSD llsd_block;
		std::string llsd_raw;
		msg->getStringFast(_PREHASH_AlertInfo, _PREHASH_ExtraParams, llsd_raw);
		if (llsd_raw.length())
		{
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(llsd_block, llsd_data, llsd_raw.length()))
			{
				LL_WARNS() << "process_teleport_failed: Attempted to read alert parameter data into LLSD but failed:" << llsd_raw << LL_ENDL;
			}
			else
			{
				// change notification name in this special case
				if (handle_teleport_access_blocked(llsd_block, message_id, args["REASON"]))
				{
					if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
					{
						gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
					}
					return;
				}
			}
		}

	}
	else
	{	// Extra message payload not found - use what the simulator sent
		msg->getStringFast(_PREHASH_Info, _PREHASH_Reason, message_id);

		big_reason = LLAgent::sTeleportErrorMessages[message_id];
		if ( big_reason.size() > 0 )
		{	// Substitute verbose reason from the local map
			args["REASON"] = big_reason;
		}
		else
		{	// Nothing found in the map - use what the server returned
			args["REASON"] = message_id;
		}
	}

	// <FS:Ansariel> Stop typing after teleport (possible fix for FIRE-7273)
	gAgent.stopTyping();

	LL_INFOS() << "Teleport error, message_id=" << message_id << LL_ENDL;
	if (!FSLSLBridge::instance().canUseBridge() ||
		(message_id != "Could not teleport closer to destination"))
	{
		LLNotificationsUtil::add("CouldNotTeleportReason", args);
	}

	if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
	{
		gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
	}
}

void process_teleport_local(LLMessageSystem *msg,void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Info, _PREHASH_AgentID, agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Got teleport notification for wrong agent!" << LL_ENDL;
		return;
	}

	U32 location_id;
	LLVector3 pos, look_at;
	U32 teleport_flags;
	msg->getU32Fast(_PREHASH_Info, _PREHASH_LocationID, location_id);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_Position, pos);
	msg->getVector3Fast(_PREHASH_Info, _PREHASH_LookAt, look_at);
	msg->getU32Fast(_PREHASH_Info, _PREHASH_TeleportFlags, teleport_flags);

	if( gAgent.getTeleportState() != LLAgent::TELEPORT_NONE )
	{
		if( gAgent.getTeleportState() == LLAgent::TELEPORT_LOCAL )
		{
			// To prevent TeleportStart messages re-activating the progress screen right
			// after tp, keep the teleport state and let progress screen clear it after a short delay
			// (progress screen is active but not visible)  *TODO: remove when SVC-5290 is fixed
			gTeleportDisplayTimer.reset();
			gTeleportDisplay = TRUE;
		}
		else
		{
			gAgent.setTeleportState( LLAgent::TELEPORT_NONE );
		}
	}

	// Sim tells us whether the new position is off the ground
	// <FS:Ansariel> Always fly after TP option
	//if (teleport_flags & TELEPORT_FLAGS_IS_FLYING)
	if (teleport_flags & TELEPORT_FLAGS_IS_FLYING || gSavedSettings.getBOOL("FSFlyAfterTeleport"))
	// </FS:Ansariel> Always fly after TP option
	{
		gAgent.setFlying(TRUE);
	}
	else
	{
		gAgent.setFlying(FALSE);
	}

	// <FS:Ansariel> Stop typing after teleport (possible fix for FIRE-7273)
	gAgent.stopTyping();

	gAgent.setPositionAgent(pos);
	gAgentCamera.slamLookAt(look_at);

	if ( !(gAgent.getTeleportKeepsLookAt() && LLViewerJoystick::getInstance()->getOverrideCamera()) && gSavedSettings.getBOOL("FSResetCameraOnTP") )
	{
		gAgentCamera.resetView(TRUE, TRUE);
	}

	// send camera update to new region
	gAgentCamera.updateCamera();

	send_agent_update(TRUE, TRUE);

	// Let the interested parties know we've teleported.
	// Vadim *HACK: Agent position seems to get reset (to render position?)
	//              on each frame, so we have to pass the new position manually.
	LLViewerParcelMgr::getInstance()->onTeleportFinished(true, gAgent.getPosGlobalFromAgent(pos));
}

void send_simple_im(const LLUUID& to_id,
					const std::string& message,
					EInstantMessage dialog,
					const LLUUID& id)
{
	std::string my_name;
	LLAgentUI::buildFullname(my_name);
	send_improved_im(to_id,
					 my_name,
					 message,
					 IM_ONLINE,
					 dialog,
					 id,
					 NO_TIMESTAMP,
					 (U8*)EMPTY_BINARY_BUCKET,
					 EMPTY_BINARY_BUCKET_SIZE);
}

void send_group_notice(const LLUUID& group_id,
					   const std::string& subject,
					   const std::string& message,
					   const LLInventoryItem* item)
{
	// Put this notice into an instant message form.
	// This will mean converting the item to a binary bucket,
	// and the subject/message into a single field.
	std::string my_name;
	LLAgentUI::buildFullname(my_name);

	// Combine subject + message into a single string.
	std::ostringstream subject_and_message;
	// TODO: turn all existing |'s into ||'s in subject and message.
	subject_and_message << subject << "|" << message;

	// Create an empty binary bucket.
	U8 bin_bucket[MAX_INVENTORY_BUFFER_SIZE];
	U8* bucket_to_send = bin_bucket;
	bin_bucket[0] = '\0';
	S32 bin_bucket_size = EMPTY_BINARY_BUCKET_SIZE;
	// If there is an item being sent, pack it into the binary bucket.
	if (item)
	{
		LLSD item_def;
		item_def["item_id"] = item->getUUID();
		item_def["owner_id"] = item->getPermissions().getOwner();
		std::ostringstream ostr;
		LLSDSerialize::serialize(item_def, ostr, LLSDSerialize::LLSD_XML);
		bin_bucket_size = ostr.str().copy(
			(char*)bin_bucket, ostr.str().size());
		bin_bucket[bin_bucket_size] = '\0';
	}
	else
	{
		bucket_to_send = (U8*) EMPTY_BINARY_BUCKET;
	}
   

	send_improved_im(
			group_id,
			my_name,
			subject_and_message.str(),
			IM_ONLINE,
			IM_GROUP_NOTICE,
			LLUUID::null,
			NO_TIMESTAMP,
			bucket_to_send,
			bin_bucket_size);
}

void send_lures(const LLSD& notification, const LLSD& response)
{
	std::string text = response["message"].asString();
	LLSLURL slurl;
	LLAgentUI::buildSLURL(slurl);
	text.append("\r\n").append(slurl.getSLURLString());

// [RLVa:KB] - Checked: RLVa-2.0.0
	// Filter the lure message if any of the recipients are IM-blocked
	const LLSD& sdRecipients = notification["payload"]["ids"];
	if ( (gRlvHandler.isEnabled()) && 
	     (std::any_of(sdRecipients.beginArray(), sdRecipients.endArray(), [](const LLSD& id) { return !RlvActions::canStartIM(id.asUUID()) || !RlvActions::canSendIM(id.asUUID()); })) )
	{
		text = RlvStrings::getString(RLV_STRING_HIDDEN);
	}
// [/RLVa:KB]

	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_StartLure);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->nextBlockFast(_PREHASH_Info);
	msg->addU8Fast(_PREHASH_LureType, (U8)0); // sim will fill this in.
	msg->addStringFast(_PREHASH_Message, text);
	for(LLSD::array_const_iterator it = notification["payload"]["ids"].beginArray();
		it != notification["payload"]["ids"].endArray();
		++it)
	{
		LLUUID target_id = it->asUUID();

		msg->nextBlockFast(_PREHASH_TargetData);
		msg->addUUIDFast(_PREHASH_TargetID, target_id);

		// Record the offer.
		{
// [RLVa:KB] - Checked: RLVa-2.0.1
			bool fRlvCanShowName = (!notification["payload"].has("rlv_shownames")) ? true : !notification["payload"]["rlv_shownames"].asBoolean();
// [/RLVa:KB]
			// <FS:Ansariel> Show complete name for TP lures
			//std::string target_name;
			//gCacheName->getFullName(target_id, target_name);  // for im log filenames
			LLSD args;
			//args["TO_NAME"] = LLSLURL("agent", target_id, "displayname").getSLURLString();;
// [RLVa:KB] - Checked: 2014-03-31 (Catznip-3.6)
			args["TO_NAME"] = LLSLURL("agent", target_id, (fRlvCanShowName) ? "completename" : "rlvanonym").getSLURLString();;
// [/RLVa:KB]
//			args["TO_NAME"] = LLSLURL("agent", target_id, "completename").getSLURLString();
			// </FS:Ansariel>
	
			LLSD payload;
				
			//*TODO please rewrite all keys to the same case, lower or upper
			payload["from_id"] = target_id;
			payload["SUPPRESS_TOAST"] = true;
			LLNotificationsUtil::add("TeleportOfferSent", args, payload);

			// Add the recepient to the recent people list.
// [RLVa:KB] - Checked: RLVa-2.0.1
			if (fRlvCanShowName)
				LLRecentPeople::instance().add(target_id);
// [/RLVa:KB]
//			LLRecentPeople::instance().add(target_id);
		}
	}
	gAgent.sendReliableMessage();
}

bool handle_lure_callback(const LLSD& notification, const LLSD& response)
{
	static const unsigned OFFER_RECIPIENT_LIMIT = 250;
	if(notification["payload"]["ids"].size() > OFFER_RECIPIENT_LIMIT) 
	{
		// More than OFFER_RECIPIENT_LIMIT targets will overload the message
		// producing an llerror.
		LLSD args;
		args["OFFERS"] = notification["payload"]["ids"].size();
		args["LIMIT"] = static_cast<int>(OFFER_RECIPIENT_LIMIT);
		LLNotificationsUtil::add("TooManyTeleportOffers", args);
		return false;
	}

	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if(0 == option)
	{
		send_lures(notification, response);
	}

	return false;
}

void handle_lure(const LLUUID& invitee)
{
	std::vector<LLUUID> ids;
	ids.push_back(invitee);
	handle_lure(ids);
}

// Prompt for a message to the invited user.
void handle_lure(const uuid_vec_t& ids)
{
	if (ids.empty()) return;

	if (!gAgent.getRegion()) return;

	LLSD edit_args;
// [RLVa:KB] - Checked: 2010-04-07 (RLVa-1.2.0d) | Modified: RLVa-1.0.0a
	edit_args["REGION"] = (!gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC)) ? gAgent.getRegion()->getName() : RlvStrings::getString(RLV_STRING_HIDDEN);
// [/RLVa:KB]
//	edit_args["REGION"] = gAgent.getRegion()->getName();

	LLSD payload;
// [RLVa:KB] - Checked: RLVa-2.0.1
	bool fRlvShouldHideNames = false;
	for (const LLUUID& idAgent : ids)
	{
		// Only allow offering teleports if everyone is a @tplure exception or able to map this avie under @showloc=n
		if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
		{
			const LLRelationship* pBuddyInfo = LLAvatarTracker::instance().getBuddyInfo(idAgent);
			if ( (!gRlvHandler.isException(RLV_BHVR_TPLURE, idAgent, RLV_CHECK_PERMISSIVE)) &&
				 ((!pBuddyInfo) || (!pBuddyInfo->isOnline()) || (!pBuddyInfo->isRightGrantedTo(LLRelationship::GRANT_MAP_LOCATION))) )
			{
				RlvUtil::notifyBlocked(RLV_STRING_BLOCKED_TELEPORT_OFFER);
				return;
			}
		}
		fRlvShouldHideNames |= !RlvActions::canShowName(RlvActions::SNC_TELEPORTOFFER, idAgent);
		payload["ids"].append(idAgent);
	}
	payload["rlv_shownames"] = fRlvShouldHideNames;
// [/RLVa:KB]
// 	for (std::vector<LLUUID>::const_iterator it = ids.begin();
// 		it != ids.end();
// 		++it)
// 	{
// 		payload["ids"].append(*it);
// 	}
	if (gAgent.isGodlike())
	{
		LLNotificationsUtil::add("OfferTeleportFromGod", edit_args, payload, handle_lure_callback);
	}
	else
	{
		LLNotificationsUtil::add("OfferTeleport", edit_args, payload, handle_lure_callback);
	}
}

bool teleport_request_callback(const LLSD& notification, const LLSD& response)
{
	LLUUID from_id = notification["payload"]["from_id"].asUUID();
	if(from_id.isNull())
	{
		LL_WARNS() << "from_id is NULL" << LL_ENDL;
		return false;
	}

	std::string from_name;
	gCacheName->getFullName(from_id, from_name);

	if(LLMuteList::getInstance()->isMuted(from_id) && !LLMuteList::getInstance()->isLinden(from_name))
	{
		return false;
	}

	S32 option = 0;
	if (response.isInteger()) 
	{
		option = response.asInteger();
	}
	else
	{
		option = LLNotificationsUtil::getSelectedOption(notification, response);
	}

	switch(option)
	{
	// Yes
	case 0:
		{
			LLSD dummy_notification;
			dummy_notification["payload"]["ids"][0] = from_id;

			LLSD dummy_response;
			dummy_response["message"] = response["message"];

			send_lures(dummy_notification, dummy_response);
		}
		break;

	// No
	case 1:
	default:
		break;
	}

	return false;
}

static LLNotificationFunctorRegistration teleport_request_callback_reg("TeleportRequest", teleport_request_callback);

void send_improved_im(const LLUUID& to_id,
							const std::string& name,
							const std::string& message,
							U8 offline,
							EInstantMessage dialog,
							const LLUUID& id,
							U32 timestamp, 
							const U8* binary_bucket,
							S32 binary_bucket_size)
{
	pack_instant_message(
		gMessageSystem,
		gAgent.getID(),
		FALSE,
		gAgent.getSessionID(),
		to_id,
		name,
		message,
		offline,
		dialog,
		id,
		0,
		LLUUID::null,
		gAgent.getPositionAgent(),
		timestamp,
		binary_bucket,
		binary_bucket_size);
	gAgent.sendReliableMessage();
}


void send_places_query(const LLUUID& query_id,
					   const LLUUID& trans_id,
					   const std::string& query_text,
					   U32 query_flags,
					   S32 category,
					   const std::string& sim_name)
{
	LLMessageSystem* msg = gMessageSystem;

	msg->newMessage("PlacesQuery");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgent.getID());
	msg->addUUID("SessionID", gAgent.getSessionID());
	msg->addUUID("QueryID", query_id);
	msg->nextBlock("TransactionData");
	msg->addUUID("TransactionID", trans_id);
	msg->nextBlock("QueryData");
	msg->addString("QueryText", query_text);
	msg->addU32("QueryFlags", query_flags);
	msg->addS8("Category", (S8)category);
	msg->addString("SimName", sim_name);
	gAgent.sendReliableMessage();
}


void process_user_info_reply(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);
	if(agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "process_user_info_reply - "
				<< "wrong agent id." << LL_ENDL;
	}
	
	BOOL im_via_email;
	msg->getBOOLFast(_PREHASH_UserData, _PREHASH_IMViaEMail, im_via_email);
	std::string email;
	msg->getStringFast(_PREHASH_UserData, _PREHASH_EMail, email);
	std::string dir_visibility;
	msg->getString( "UserData", "DirectoryVisibility", dir_visibility);

	// <FS:Ansariel> Show email address in preferences (FIRE-1071)
	//LLFloaterPreference::updateUserInfo(dir_visibility, im_via_email);
	LLFloaterPreference::updateUserInfo(dir_visibility, im_via_email, email);
	// </FS:Ansariel> Show email address in preferences (FIRE-1071)
	LLFloaterSnapshot::setAgentEmail(email);
}


//---------------------------------------------------------------------------
// Script Dialog
//---------------------------------------------------------------------------

const S32 SCRIPT_DIALOG_MAX_BUTTONS = 12;
const char* SCRIPT_DIALOG_HEADER = "Script Dialog:\n";

bool callback_script_dialog(const LLSD& notification, const LLSD& response)
{
	LLNotificationForm form(notification["form"]);

	std::string rtn_text;
	S32 button_idx;
	button_idx = LLNotification::getSelectedOption(notification, response);
	if (response[TEXTBOX_MAGIC_TOKEN].isDefined())
	{
		if (response[TEXTBOX_MAGIC_TOKEN].isString())
			rtn_text = response[TEXTBOX_MAGIC_TOKEN].asString();
		else
			rtn_text.clear(); // bool marks empty string
	}
	else
	{
		rtn_text = LLNotification::getSelectedOptionName(response);
	}

	// Button -2 = Mute
	// Button -1 = Ignore - no processing needed for this button
	// Buttons 0 and above = dialog choices

	if (-2 == button_idx)
	{
		std::string object_name = notification["payload"]["object_name"].asString();
		LLUUID object_id = notification["payload"]["object_id"].asUUID();
		LLMute mute(object_id, object_name, LLMute::OBJECT);
		if (LLMuteList::getInstance()->add(mute))
		{
			// This call opens the sidebar, displays the block list, and highlights the newly blocked
			// object in the list so the user can see that their block click has taken effect.
			LLPanelBlockedList::showPanelAndSelect(object_id);
		}
	}

	if (0 <= button_idx)
	{
		LLMessageSystem* msg = gMessageSystem;
		msg->newMessage("ScriptDialogReply");
		msg->nextBlock("AgentData");
		msg->addUUID("AgentID", gAgent.getID());
		msg->addUUID("SessionID", gAgent.getSessionID());
		msg->nextBlock("Data");
		msg->addUUID("ObjectID", notification["payload"]["object_id"].asUUID());
		msg->addS32("ChatChannel", notification["payload"]["chat_channel"].asInteger());
		msg->addS32("ButtonIndex", button_idx);
		msg->addString("ButtonLabel", rtn_text);
		msg->sendReliable(LLHost(notification["payload"]["sender"].asString()));
	}

	return false;
}
static LLNotificationFunctorRegistration callback_script_dialog_reg_1("ScriptDialog", callback_script_dialog);
static LLNotificationFunctorRegistration callback_script_dialog_reg_2("ScriptDialogGroup", callback_script_dialog);

void process_script_dialog(LLMessageSystem* msg, void**)
{
	S32 i;
	LLSD payload;

	LLUUID object_id;
	msg->getUUID("Data", "ObjectID", object_id);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SCRIPT_DIALOG, object_id, ANTISPAM_SOURCE_OBJECT))
	{
		return;
	}
	// NaCl End

//	For compability with OS grids first check for presence of extended packet before fetching data.
    LLUUID owner_id;
	if (gMessageSystem->getNumberOfBlocks("OwnerData") > 0)
	{
    msg->getUUID("OwnerData", "OwnerID", owner_id);
	}

	if (LLMuteList::getInstance()->isMuted(object_id) || LLMuteList::getInstance()->isMuted(owner_id))
	{
		return;
	}

	std::string message; 
	std::string first_name;
	std::string last_name;
	std::string object_name;

	S32 chat_channel;
	msg->getString("Data", "FirstName", first_name);
	msg->getString("Data", "LastName", last_name);
	msg->getString("Data", "ObjectName", object_name);
	msg->getString("Data", "Message", message);
	msg->getS32("Data", "ChatChannel", chat_channel);

		// unused for now
	LLUUID image_id;
	msg->getUUID("Data", "ImageID", image_id);

	payload["sender"] = msg->getSender().getIPandPort();
	payload["object_id"] = object_id;
	payload["chat_channel"] = chat_channel;
	payload["object_name"] = object_name;

	// <FS:Ansariel> FIRE-17158: Remove "block" button for script dialog of own objects
	bool own_object = false;
	std::string self_name;
	LLAgentUI::buildFullname(self_name);
	if (LLCacheName::buildFullName(first_name, last_name) == self_name)
	{
		own_object = true;
	}
	payload["own_object"] = own_object;
	// </FS:Ansariel>

	// build up custom form
	S32 button_count = msg->getNumberOfBlocks("Buttons");
	if (button_count > SCRIPT_DIALOG_MAX_BUTTONS)
	{
		LL_WARNS() << "Too many script dialog buttons - omitting some" << LL_ENDL;
		button_count = SCRIPT_DIALOG_MAX_BUTTONS;
	}

	LLNotificationForm form;
	for (i = 0; i < button_count; i++)
	{
		std::string tdesc;
		msg->getString("Buttons", "ButtonLabel", tdesc, i);
		form.addElement("button", std::string(tdesc));
	}

	LLSD args;
	args["TITLE"] = object_name;
	args["MESSAGE"] = message;
	LLNotificationPtr notification;
	if (!first_name.empty())
	{
		args["NAME"] = LLCacheName::buildFullName(first_name, last_name);
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialog").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
	else
	{
		args["GROUPNAME"] = last_name;
		notification = LLNotifications::instance().add(
			LLNotification::Params("ScriptDialogGroup").substitutions(args).payload(payload).form_elements(form.asLLSD()));
	}
}

//---------------------------------------------------------------------------


std::vector<LLSD> gLoadUrlList;

bool callback_load_url(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotificationsUtil::getSelectedOption(notification, response);

	if (0 == option)
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}

	return false;
}
static LLNotificationFunctorRegistration callback_load_url_reg("LoadWebPage", callback_load_url);


// We've got the name of the person who owns the object hurling the url.
// Display confirmation dialog.
void callback_load_url_name(const LLUUID& id, const std::string& full_name, bool is_group)
{
	std::vector<LLSD>::iterator it;
	for (it = gLoadUrlList.begin(); it != gLoadUrlList.end(); )
	{
		LLSD load_url_info = *it;
		if (load_url_info["owner_id"].asUUID() == id)
		{
			it = gLoadUrlList.erase(it);

			std::string owner_name;
			if (is_group)
			{
				owner_name = full_name + LLTrans::getString("Group");
			}
			else
			{
				owner_name = full_name;
			}

			// For legacy name-only mutes.
			if (LLMuteList::getInstance()->isMuted(LLUUID::null, owner_name))
			{
				continue;
			}
			LLSD args;
			args["URL"] = load_url_info["url"].asString();
			args["MESSAGE"] = load_url_info["message"].asString();
			args["OBJECTNAME"] = load_url_info["object_name"].asString();
			args["NAME_SLURL"] = LLSLURL(is_group ? "group" : "agent", id, "about").getSLURLString();

			LLNotificationsUtil::add("LoadWebPage", args, load_url_info);
		}
		else
		{
			++it;
		}
	}
}

void process_load_url(LLMessageSystem* msg, void**)
{
	LLUUID object_id;
	LLUUID owner_id;
	BOOL owner_is_group;
	char object_name[256];		/* Flawfinder: ignore */
	char message[256];		/* Flawfinder: ignore */
	char url[256];		/* Flawfinder: ignore */

	msg->getString("Data", "ObjectName", 256, object_name);
	msg->getUUID(  "Data", "ObjectID", object_id);
	msg->getUUID(  "Data", "OwnerID", owner_id);
	msg->getBOOL(  "Data", "OwnerIsGroup", owner_is_group);
	msg->getString("Data", "Message", 256, message);
	msg->getString("Data", "URL", 256, url);

	// NaCl - Antispam Registry
	if (NACLAntiSpamRegistry::instance().checkQueue(ANTISPAM_QUEUE_SCRIPT_DIALOG, object_id, ANTISPAM_SOURCE_OBJECT))
	{
		return;
	}
	// NaCl End

	LLSD payload;
	payload["object_id"] = object_id;
	payload["owner_id"] = owner_id;
	payload["owner_is_group"] = owner_is_group;
	payload["object_name"] = object_name;
	payload["message"] = message;
	payload["url"] = url;

	// URL is safety checked in load_url above

	// Check if object or owner is muted
	if (LLMuteList::getInstance()->isMuted(object_id, object_name) ||
	    LLMuteList::getInstance()->isMuted(owner_id))
	{
		LL_INFOS("Messaging")<<"Ignoring load_url from muted object/owner."<<LL_ENDL;
		return;
	}

	// Add to list of pending name lookups
	gLoadUrlList.push_back(payload);

	gCacheName->get(owner_id, owner_is_group,
		boost::bind(&callback_load_url_name, _1, _2, _3));
}


void callback_download_complete(void** data, S32 result, LLExtStat ext_status)
{
	std::string* filepath = (std::string*)data;
	LLSD args;
	args["DOWNLOAD_PATH"] = *filepath;
	LLNotificationsUtil::add("FinishedRawDownload", args);
	delete filepath;
}


void process_initiate_download(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);
	if (agent_id != gAgent.getID())
	{
		LL_WARNS("Messaging") << "Initiate download for wrong agent" << LL_ENDL;
		return;
	}

	std::string sim_filename;
	std::string viewer_filename;
	msg->getString("FileData", "SimFilename", sim_filename);
	msg->getString("FileData", "ViewerFilename", viewer_filename);

	if (!gXferManager->validateFileForRequest(viewer_filename))
	{
		LL_WARNS() << "SECURITY: Unauthorized download to local file " << viewer_filename << LL_ENDL;
		return;
	}
	gXferManager->requestFile(viewer_filename,
		sim_filename,
		LL_PATH_NONE,
		msg->getSender(),
		FALSE,	// don't delete remote
		callback_download_complete,
		(void**)new std::string(viewer_filename));
}


void process_script_teleport_request(LLMessageSystem* msg, void**)
{
	if (!gSavedSettings.getBOOL("ScriptsCanShowUI")) return;

	std::string object_name;
	std::string sim_name;
	LLVector3 pos;
	LLVector3 look_at;

	msg->getString("Data", "ObjectName", object_name);
	msg->getString("Data", "SimName", sim_name);
	msg->getVector3("Data", "SimPosition", pos);
	msg->getVector3("Data", "LookAt", look_at);

	LLFloaterWorldMap* instance = LLFloaterWorldMap::getInstance();
	if(instance)
	{
		LL_INFOS() << "Object named " << object_name 
			<< " is offering TP to region "
			<< sim_name << " position " << pos
			<< LL_ENDL;

		instance->trackURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]);
		LLFloaterReg::showInstance("world_map", "center");
	}
	
	// remove above two lines and replace with below line
	// to re-enable parcel browser for llMapDestination()
	// LLURLDispatcher::dispatch(LLSLURL::buildSLURL(sim_name, (S32)pos.mV[VX], (S32)pos.mV[VY], (S32)pos.mV[VZ]), FALSE);
	
}

void process_covenant_reply(LLMessageSystem* msg, void**)
{
	LLUUID covenant_id, estate_owner_id;
	std::string estate_name;
	U32 covenant_timestamp;
	msg->getUUID("Data", "CovenantID", covenant_id);
	msg->getU32("Data", "CovenantTimestamp", covenant_timestamp);
	msg->getString("Data", "EstateName", estate_name);
	msg->getUUID("Data", "EstateOwnerID", estate_owner_id);

	LLPanelEstateCovenant::updateEstateName(estate_name);
	LLPanelLandCovenant::updateEstateName(estate_name);
	LLPanelEstateInfo::updateEstateName(estate_name);
	LLFloaterBuyLand::updateEstateName(estate_name);

	std::string owner_name =
		LLSLURL("agent", estate_owner_id, "inspect").getSLURLString();
	LLPanelEstateCovenant::updateEstateOwnerName(owner_name);
	LLPanelLandCovenant::updateEstateOwnerName(owner_name);
	LLPanelEstateInfo::updateEstateOwnerName(owner_name);
	LLFloaterBuyLand::updateEstateOwnerName(owner_name);

	// <FS:Ansariel> Don't create places floater if we don't need it
	//LLPanelPlaceProfile* panel = LLFloaterSidePanelContainer::getPanel<LLPanelPlaceProfile>("places", "panel_place_profile");
	LLPanelPlaceProfile* panel = LLFloaterSidePanelContainer::findPanel<LLPanelPlaceProfile>("places", "panel_place_profile");
	// </FS:Ansariel>
	if (panel)
	{
		panel->updateEstateName(estate_name);
		panel->updateEstateOwnerName(owner_name);
	}

	// <FS:Ansariel> Standalone location profile floater
	FSFloaterPlaceDetails* fs_floater = LLFloaterReg::findTypedInstance<FSFloaterPlaceDetails>("fs_placedetails", LLSD().with("type", "agent"));
	if (fs_floater)
	{
		fs_floater->updateEstateName(estate_name);
		fs_floater->updateEstateOwnerName(owner_name);
	}
	// </FS:Ansariel>

	// standard message, not from system
	std::string last_modified;
	if (covenant_timestamp == 0)
	{
		last_modified = LLTrans::getString("covenant_last_modified")+LLTrans::getString("never_text");
	}
	else
	{
		last_modified = LLTrans::getString("covenant_last_modified")+"["
						+LLTrans::getString("LTimeWeek")+"] ["
						+LLTrans::getString("LTimeMonth")+"] ["
						+LLTrans::getString("LTimeDay")+"] ["
						+LLTrans::getString("LTimeHour")+"]:["
						+LLTrans::getString("LTimeMin")+"]:["
						+LLTrans::getString("LTimeSec")+"] ["
						+LLTrans::getString("LTimeYear")+"]";
		LLSD substitution;
		substitution["datetime"] = (S32) covenant_timestamp;
		LLStringUtil::format (last_modified, substitution);
	}

	LLPanelEstateCovenant::updateLastModified(last_modified);
	LLPanelLandCovenant::updateLastModified(last_modified);
	LLFloaterBuyLand::updateLastModified(last_modified);

	// load the actual covenant asset data
	const BOOL high_priority = TRUE;
	if (covenant_id.notNull())
	{
		gAssetStorage->getEstateAsset(gAgent.getRegionHost(),
									gAgent.getID(),
									gAgent.getSessionID(),
									covenant_id,
                                    LLAssetType::AT_NOTECARD,
									ET_Covenant,
                                    onCovenantLoadComplete,
									NULL,
									high_priority);
	}
	else
	{
		std::string covenant_text;
		if (estate_owner_id.isNull())
		{
			// mainland
			covenant_text = LLTrans::getString("RegionNoCovenant");
		}
		else
		{
			covenant_text = LLTrans::getString("RegionNoCovenantOtherOwner");
		}
		LLPanelEstateCovenant::updateCovenantText(covenant_text, covenant_id);
		LLPanelLandCovenant::updateCovenantText(covenant_text);
		LLFloaterBuyLand::updateCovenantText(covenant_text, covenant_id);
		if (panel)
		{
			panel->updateCovenantText(covenant_text);
		}
		// <FS:Ansariel> Standalone location profile floater
		if (fs_floater)
		{
			fs_floater->updateCovenantText(covenant_text);
		}
		// </FS:Ansariel>
	}
}

void onCovenantLoadComplete(LLVFS *vfs,
					const LLUUID& asset_uuid,
					LLAssetType::EType type,
					void* user_data, S32 status, LLExtStat ext_status)
{
	LL_DEBUGS("Messaging") << "onCovenantLoadComplete()" << LL_ENDL;
	std::string covenant_text;
	if(0 == status)
	{
		LLVFile file(vfs, asset_uuid, type, LLVFile::READ);
		
		S32 file_length = file.getSize();
		
		std::vector<char> buffer(file_length+1);
		file.read((U8*)&buffer[0], file_length);		
		// put a EOS at the end
		buffer[file_length] = '\0';
		
		if( (file_length > 19) && !strncmp( &buffer[0], "Linden text version", 19 ) )
		{
			LLViewerTextEditor::Params params;
			params.name("temp");
			params.max_text_length(file_length+1);
			LLViewerTextEditor * editor = LLUICtrlFactory::create<LLViewerTextEditor> (params);
			if( !editor->importBuffer( &buffer[0], file_length+1 ) )
			{
				LL_WARNS("Messaging") << "Problem importing estate covenant." << LL_ENDL;
				covenant_text = "Problem importing estate covenant.";
			}
			else
			{
				// Version 0 (just text, doesn't include version number)
				covenant_text = editor->getText();
			}
			delete editor;
		}
		else
		{
			LL_WARNS("Messaging") << "Problem importing estate covenant: Covenant file format error." << LL_ENDL;
			covenant_text = "Problem importing estate covenant: Covenant file format error.";
		}
	}
	else
	{
		if( LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
		    LL_ERR_FILE_EMPTY == status)
		{
			covenant_text = "Estate covenant notecard is missing from database.";
		}
		else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
		{
			covenant_text = "Insufficient permissions to view estate covenant.";
		}
		else
		{
			covenant_text = "Unable to load estate covenant at this time.";
		}
		
		LL_WARNS("Messaging") << "Problem loading notecard: " << status << LL_ENDL;
	}
	LLPanelEstateCovenant::updateCovenantText(covenant_text, asset_uuid);
	LLPanelLandCovenant::updateCovenantText(covenant_text);
	LLFloaterBuyLand::updateCovenantText(covenant_text, asset_uuid);

	LLPanelPlaceProfile* panel = LLFloaterSidePanelContainer::getPanel<LLPanelPlaceProfile>("places", "panel_place_profile");
	if (panel)
	{
		panel->updateCovenantText(covenant_text);
	}

	// <FS:Ansariel> Standalone location profile floater
	FSFloaterPlaceDetails* fs_floater = LLFloaterReg::findTypedInstance<FSFloaterPlaceDetails>("fs_placedetails", LLSD().with("type", "agent"));
	if (fs_floater)
	{
		fs_floater->updateCovenantText(covenant_text);
	}
	// </FS:Ansariel>
}


void process_feature_disabled_message(LLMessageSystem* msg, void**)
{
	// Handle Blacklisted feature simulator response...
	LLUUID	agentID;
	LLUUID	transactionID;
	std::string	messageText;
	msg->getStringFast(_PREHASH_FailureInfo,_PREHASH_ErrorMessage, messageText,0);
	msg->getUUIDFast(_PREHASH_FailureInfo,_PREHASH_AgentID,agentID);
	msg->getUUIDFast(_PREHASH_FailureInfo,_PREHASH_TransactionID,transactionID);
	
	LL_WARNS("Messaging") << "Blacklisted Feature Response:" << messageText << LL_ENDL;
}

// ------------------------------------------------------------
// Message system exception callbacks
// ------------------------------------------------------------

void invalid_message_callback(LLMessageSystem* msg,
								   void*,
								   EMessageException exception)
{
    LLAppViewer::instance()->badNetworkHandler();
}

// Please do not add more message handlers here. This file is huge.
// Put them in a file related to the functionality you are implementing.

void LLOfferInfo::forceResponse(InventoryOfferResponse response)
{
	// <FS:Ansariel> Now this is a hell of piece of... forceResponse() will look for the
	//               ELEMENT index, and NOT the button index. So if we want to force a
	//               response of IOR_ACCEPT, we need to pass the correct element
	//               index of the button.
	//LLNotification::Params params("UserGiveItem");
	//params.functor.function(boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2));
	//LLNotifications::instance().forceResponse(params, response);
	S32 element_index;
	switch (response)
	{
		case IOR_ACCEPT:
			element_index = 1;
			break;
		case IOR_DECLINE:
			element_index = 2;
			break;
		case IOR_MUTE:
			element_index = 3;
			break;
		default:
			element_index = -1;
			break;
	}
	LLNotification::Params params("UserGiveItem");
	params.functor.function(boost::bind(&LLOfferInfo::inventory_offer_callback, this, _1, _2));
	LLNotifications::instance().forceResponse(params, element_index);
	// </FS:Ansariel>
}


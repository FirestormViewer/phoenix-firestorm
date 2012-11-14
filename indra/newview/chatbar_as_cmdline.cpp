/* Copyright (c) 2010
 *
 * Modular Systems All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *   3. Neither the name Modular Systems nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY MODULAR SYSTEMS AND CONTRIBUTORS “AS IS”
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MODULAR SYSTEMS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "llviewerprecompiledheaders.h"

#include "chatbar_as_cmdline.h"

#include "aoengine.h"
#include "fscommon.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llavatarlist.h"
#include "llcalc.h"
#include "llfloaternearbychat.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llinventorymodel.h"
#include "llnotificationmanager.h"
#include "llpanelpeople.h"
#include "llparcel.h"
#include "lltooldraganddrop.h"
#include "lltrans.h"
#include "llurldispatcher.h"
#include "llvieweraudio.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmediaautoplay.h"
#include "llviewerparcelmgr.h"
#include "llvolumemessage.h"
#include "llworld.h"
#include "llworldmap.h"
#include "rlvhandler.h"


LLViewerInventoryItem::item_array_t findInventoryInFolder(const std::string& ifolder)
{
	LLUUID folder = gInventory.findCategoryByName(ifolder);
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	//ObjectContentNameMatches objectnamematches(ifolder);
	gInventory.collectDescendents(folder,cats,items,FALSE);//,objectnamematches);

	return items;
}

void doZdCleanup();
class JCZdrop : public LLEventTimer
{
public:
	BOOL mRunning;
	
	JCZdrop(std::stack<LLViewerInventoryItem*> stack, LLUUID dest, std::string sFolder, std::string sUUID, bool package = false) : LLEventTimer(1.0), mRunning(FALSE)
	{
		mPackage = package;
		instack = stack;
		indest = dest;
		dFolder = sFolder;
		dUUID = sUUID;
	}
	~JCZdrop()
	{
		doZdCleanup();
		if (errorCode == 1)
		{
			reportToNearbyChat(llformat("The object with the UUID of \"%s\" can no longer be found in-world.",dUUID.c_str()));
			reportToNearbyChat("This can occur if the object was returned or deleted, or if your client is no longer rendering it.");
			reportToNearbyChat(llformat("Transfer from \"%s\" to \"%s\" aborted.",dFolder.c_str(),dUUID.c_str()));
		}
		else
		{
			if(mPackage)
			{
				reportToNearbyChat("Packager finished, you may now pick up the prim that contains the objects.");
				reportToNearbyChat(llformat("Packaged what you had selected in world into the folder \"%s\" in your inventory and into the prim with the UUID of \"%s\"",dFolder.c_str(),dUUID.c_str()));
				reportToNearbyChat("Don't worry if you look at the contents of package right now, it may show as empty, it isn't, it's just a bug with Second Life itself.");
				reportToNearbyChat("If you take it into your inventory then rez it back out, all the contents will be there.");
			}
			else
			{
				reportToNearbyChat(llformat("Completed transfer from \"%s\" to \"%s\".",dFolder.c_str(),dUUID.c_str()));
			}
		}
	}
	BOOL tick()
	{
		LLViewerInventoryItem* subj = instack.top();
		instack.pop();
		LLViewerObject *objectp = gObjectList.findObject(indest);
		if(objectp)
		{
			reportToNearbyChat(std::string("transferring ")+subj->getName());
			LLToolDragAndDrop::dropInventory(objectp,subj,LLToolDragAndDrop::SOURCE_AGENT,gAgent.getID());
			if (instack.size() > 0)
			{
				return mRunning;
			}
			else
			{
				return (instack.size() == 0);
			}
		}
		else
		{
			errorCode = 1;
			return TRUE;
		}
	}


private:
	std::stack<LLViewerInventoryItem*> instack;
	LLUUID indest;
	std::string dFolder;
	std::string dUUID;
	int errorCode;
	bool mPackage;
};

JCZdrop *zdrop;

class ZdCleanup: public LLEventTimer
{
public:
	ZdCleanup() : LLEventTimer(0.2f) //Don't really need this long of a timer, but it can't be to short or tick() below may get called before the packager's tick() returns to the timer system. Things get ugly otherwise.
	{
	}
	~ZdCleanup()
	{
	}
	BOOL tick()
	{
		zdrop = NULL;
		return TRUE;
	}
};

void doZdCleanup()
{
	new ZdCleanup();
}
void doZtCleanup();
class JCZtake : public LLEventTimer
{
public:
	BOOL mRunning;

	JCZtake(const LLUUID& target, bool package = false, LLUUID destination = LLUUID::null, std::string dtarget = "") : LLEventTimer(0.66f), mTarget(target), mRunning(FALSE), mCountdown(5), mPackage(package), mPackageDest(destination)
	{
		mFolderName = dtarget;
		if(mPackage)
		{
			reportToNearbyChat("Packager started. Phase 1 (taking in-world objects into inventory) starting in: ");
		}
		else
		{
			reportToNearbyChat("Ztake activated. Taking selected in-world objects into inventory in: ");
		}
	}
	~JCZtake()
	{
		if(!mPackage)
		{
			reportToNearbyChat("Ztake deactivated.");
		}
	}
	BOOL tick()
	{
		{
			LLMessageSystem *msg = gMessageSystem;
			for(LLObjectSelection::root_iterator itr=LLSelectMgr::getInstance()->getSelection()->root_begin();
				itr!=LLSelectMgr::getInstance()->getSelection()->root_end();++itr)
			{
				LLSelectNode* node = (*itr);
				LLViewerObject* objectp = node->getObject();
				U32 localid=objectp->getLocalID();
				if(mDonePrims.find(localid) == mDonePrims.end())
				{
					mDonePrims.insert(localid);
					mToTake.push_back(localid);
				}
			}

			if(mCountdown > 0)
			{
				reportToNearbyChat(llformat("%i...", mCountdown--));
			}
			else if(mToTake.size() > 0)
			{
				msg->newMessageFast(_PREHASH_DeRezObject);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_AgentBlock);
				msg->addUUIDFast(_PREHASH_GroupID, LLUUID::null);
				msg->addU8Fast(_PREHASH_Destination, 4);
				msg->addUUIDFast(_PREHASH_DestinationID, mTarget);
				LLUUID rand;
				rand.generate();
				msg->addUUIDFast(_PREHASH_TransactionID, rand);
				msg->addU8Fast(_PREHASH_PacketCount, 1);
				msg->addU8Fast(_PREHASH_PacketNumber, 0);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, mToTake[0]);
				gAgent.sendReliableMessage();
				mToTake.erase(mToTake.begin());
				if(mToTake.size() % 10 == 0)
				{
					if(mToTake.size() == 0)
					{
						if(mPackage)
						{
							reportToNearbyChat("Phase 1 of the packager finished.");
							std::stack<LLViewerInventoryItem*> lolstack;
							LLDynamicArray<LLPointer<LLViewerInventoryItem> > lolinv = findInventoryInFolder(mFolderName);
							for(LLDynamicArray<LLPointer<LLViewerInventoryItem> >::iterator it = lolinv.begin(); it != lolinv.end(); ++it)
							{
								LLViewerInventoryItem* item = *it;
								lolstack.push(item);
							}
							if(lolstack.size())
							{
								reportToNearbyChat("Do not have the destination prim selected while transfer is running to reduce the chances of \"Inventory creation on in-world object failed.\"");
								LLUUID sdest = LLUUID(mPackageDest);
								new JCZdrop(lolstack, sdest, mFolderName.c_str(), mPackageDest.asString().c_str(), true);
							} 
							doZtCleanup();
						}
						else
						{
							reportToNearbyChat("Ztake has taken all selected objects.  Say \"ztake off\" to deactivate ztake or select more objects to continue.");
						}
					} 
					else
					{
						if(mPackage)
						{
							reportToNearbyChat(llformat("Packager: %i objects left to take.", mToTake.size()));
						}
						else
						{
							reportToNearbyChat(llformat("Ztake: %i objects left to take.", mToTake.size()));
						}
					}
				}
			}
		}
		return mRunning;
	}

private:
	std::set<U32> mDonePrims;
	std::vector<U32> mToTake;
	LLUUID mTarget;
	int mCountdown;
	bool mPackage;
	LLUUID mPackageDest;
	std::string mFolderName;
};

JCZtake *ztake;

class LOZtCleanup: public LLEventTimer
{
public:
	LOZtCleanup() : LLEventTimer(0.2f) //Don't really need this long of a timer, but it can't be to short or tick() below may get called before the packager's tick() returns to the timer system. Things get ugly otherwise.
	{
	}
	~LOZtCleanup()
	{
	}
	BOOL tick()
	{
		ztake->mRunning = TRUE;
		delete ztake;
		ztake = NULL;
		return TRUE;
	}
};

void doZtCleanup()
{
	new LOZtCleanup();
}

class TMZtake : public LLEventTimer
{
public:
	BOOL mRunning;


	TMZtake(const LLUUID& target) : LLEventTimer(0.33f), mTarget(target), mRunning(FALSE), mCountdown(5)
	{
		reportToNearbyChat("Mtake activated. Taking selected in-world objects into inventory in: ");
	}
	~TMZtake()
	{
		reportToNearbyChat("Mtake deactivated.");
	}
	BOOL tick()
	{
		{
			LLMessageSystem *msg = gMessageSystem;
			for(LLObjectSelection::iterator itr=LLSelectMgr::getInstance()->getSelection()->begin();
				itr!=LLSelectMgr::getInstance()->getSelection()->end();++itr)
			{
				LLSelectNode* node = (*itr);
				LLViewerObject* object = node->getObject();
				U32 localid=object->getLocalID();
				if(mDonePrims.find(localid) == mDonePrims.end())
				{// would like to add ability to reset prim parameters to default.
					mDonePrims.insert(localid);
					std::string primX = llformat("%f",(float)object->getScale().mV[VX]);
					std::string primY = llformat("%f",(float)object->getScale().mV[VY]);
					std::string primZ = llformat("%f",(float)object->getScale().mV[VZ]);
					zeroClearX = primX.find_last_not_of("0")+1;
					primX = primX.substr(0,zeroClearX);
					zeroClearY = primY.find_last_not_of("0")+1;
					primY = primY.substr(0,zeroClearY);
					zeroClearZ = primZ.find_last_not_of("0")+1;
					primZ = primZ.substr(0,zeroClearZ);
					zeroClearX = primX.find_last_not_of(".")+1;
					primX = primX.substr(0,zeroClearX);
					zeroClearY = primY.find_last_not_of(".")+1;
					primY = primY.substr(0,zeroClearY);
					zeroClearZ = primZ.find_last_not_of(".")+1;
					primZ = primZ.substr(0,zeroClearZ);
					std::string name = llformat("%sx%sx%s",primX.c_str(),primY.c_str(),primZ.c_str()); 
					msg->newMessageFast(_PREHASH_ObjectName);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
					msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
					msg->nextBlockFast(_PREHASH_ObjectData);
					msg->addU32Fast(_PREHASH_LocalID, localid);
					msg->addStringFast(_PREHASH_Name, name);
					gAgent.sendReliableMessage();
					mToTake.push_back(localid);
				}
			}
			if(mCountdown > 0)
			{
				reportToNearbyChat(llformat("%i...", mCountdown--));
			}
			else if(mToTake.size() > 0)
			{
				msg->newMessageFast(_PREHASH_DeRezObject);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
				msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
				msg->nextBlockFast(_PREHASH_AgentBlock);
				msg->addUUIDFast(_PREHASH_GroupID, LLUUID::null);
				msg->addU8Fast(_PREHASH_Destination, 4);
				msg->addUUIDFast(_PREHASH_DestinationID, mTarget);
				LLUUID rand;
				rand.generate();
				msg->addUUIDFast(_PREHASH_TransactionID, rand);
				msg->addU8Fast(_PREHASH_PacketCount, 1);
				msg->addU8Fast(_PREHASH_PacketNumber, 0);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, mToTake[0]);
				gAgent.sendReliableMessage();
				mToTake.erase(mToTake.begin());
				if(mToTake.size() % 10 == 0) 
				{
					if(mToTake.size() == 0) 
					{
						reportToNearbyChat("Mtake has taken all selected objects.  Say \"Mtake off\" to deactivate Mtake or select more objects to continue.");
					} 
					else
					{
						reportToNearbyChat(llformat("Mtake: %i objects left to take.", mToTake.size()));
					}
				}
			}	
		}
		return mRunning;
	}

private:
	std::set<U32> mDonePrims;
	std::vector<U32> mToTake;
	LLUUID mTarget;
	int mCountdown;
	int zeroClearX;
	int zeroClearY;
	int zeroClearZ;
};
TMZtake *mtake;

void invrepair()
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	//ObjectContentNameMatches objectnamematches(ifolder);
	gInventory.collectDescendents(gInventory.getRootFolderID(),cats,items,FALSE);//,objectnamematches);
}


#ifdef JC_PROFILE_GSAVED
std::map<std::string, int> get_gsaved_calls();
#endif

bool cmd_line_chat(std::string revised_text, EChatType type, bool from_gesture)
{
	static LLCachedControl<bool> sFSCmdLine(gSavedSettings, "FSCmdLine");
	static LLCachedControl<std::string> sFSCmdLinePos(gSavedSettings,  "FSCmdLinePos");
	static LLCachedControl<std::string> sFSCmdLineDrawDistance(gSavedSettings,  "FSCmdLineDrawDistance");
	static LLCachedControl<std::string> sFSCmdTeleportToCam(gSavedSettings,  "FSCmdTeleportToCam");
	static LLCachedControl<std::string> sFSCmdLineAO(gSavedSettings,  "FSCmdLineAO");
	static LLCachedControl<std::string> sFSCmdLineKeyToName(gSavedSettings,  "FSCmdLineKeyToName");
	static LLCachedControl<std::string> sFSCmdLineOfferTp(gSavedSettings,  "FSCmdLineOfferTp");
	static LLCachedControl<std::string> sFSCmdLineGround(gSavedSettings,  "FSCmdLineGround");
	static LLCachedControl<std::string> sFSCmdLineHeight(gSavedSettings,  "FSCmdLineHeight");
	static LLCachedControl<std::string> sFSCmdLineTeleportHome(gSavedSettings,  "FSCmdLineTeleportHome");
	static LLCachedControl<std::string> sFSCmdLineRezPlatform(gSavedSettings,  "FSCmdLineRezPlatform");
	static LLCachedControl<std::string> sFSCmdLineMapTo(gSavedSettings,  "FSCmdLineMapTo");
	static LLCachedControl<bool> sFSCmdLineMapToKeepPos(gSavedSettings, "FSCmdLineMapToKeepPos");
	static LLCachedControl<std::string> sFSCmdLineCalc(gSavedSettings,  "FSCmdLineCalc");
	static LLCachedControl<std::string> sFSCmdLineTP2(gSavedSettings,  "FSCmdLineTP2");
	static LLCachedControl<std::string> sFSCmdLineClearChat(gSavedSettings,  "FSCmdLineClearChat");
	static LLCachedControl<std::string> sFSCmdLineMedia(gSavedSettings,  "FSCmdLineMedia");
	static LLCachedControl<std::string> sFSCmdLineMusic(gSavedSettings,  "FSCmdLineMusic");
	//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
	static LLCachedControl<std::string> sFSCmdLineBandwidth(gSavedSettings,  "FSCmdLineBandWidth");
	
	if(sFSCmdLine)
	{
		std::istringstream i(revised_text);
		std::string command;
		i >> command;
		if(command != "")
		{
			if(command == std::string(sFSCmdLinePos))
			{
				F32 x,y,z;
				if (i >> x)
				{
					if (i >> y)
					{
						if (i >> z)
						{
							LLVector3 agentPos = gAgent.getPositionAgent();
							LLViewerRegion* agentRegionp = gAgent.getRegion();
							if(agentRegionp)
							{
								LLVector3 targetPos(x,y,z);
								LLVector3d pos_global = from_region_handle(agentRegionp->getHandle());
								pos_global += LLVector3d((F64)targetPos.mV[0],(F64)targetPos.mV[1],(F64)targetPos.mV[2]);
								gAgent.teleportViaLocation(pos_global);
								return false;
							}
						}
					}
				}
			}
			else if(command == std::string(sFSCmdLineDrawDistance))
			{
				if(from_gesture)
				{
					reportToNearbyChat("Due to the changes in code, it is no longer necessary to use this gesture.");
					// We don't have this debug setting
					// gSavedSettings.setBOOL("RenderFarClipStepping",TRUE);
					return false;
				}
                int drawDist;
                if(i >> drawDist)
                {
					gSavedSettings.setF32("RenderFarClip", drawDist);
					gAgentCamera.mDrawDistance = drawDist;
					reportToNearbyChat(std::string(llformat("Draw distance set to: %dm",drawDist)));
					return false;
                }
			}
			else if(command == std::string(sFSCmdTeleportToCam))
            {
				gAgent.teleportViaLocation(gAgentCamera.getCameraPositionGlobal());
				return false;
            }
			else if(command == std::string(sFSCmdLineMedia))
			{
				std::string url;
				std::string type;

				if(i >> url)
				{
					if(i >> type)
					{
						LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
						if (parcel)
						{
							parcel->setMediaURL(url);
							parcel->setMediaType(type);
							if (gSavedSettings.getBOOL("MediaEnableFilter"))
							{
								LLViewerParcelMedia::filterMediaUrl(parcel);
							}
							else
							{
								LLViewerParcelMedia::play(parcel);
							}
							LLViewerParcelMediaAutoPlay::playStarted();
							return false;
						}
					}
				}
			}
			else if(command == std::string(sFSCmdLineMusic))
			{
				std::string status;
				if(i >> status)
				{
					if (gSavedSettings.getBOOL("MediaEnableFilter"))
					{
						LLViewerParcelMedia::filterAudioUrl(status);
					}
					else
					{
						LLViewerAudio::getInstance()->startInternetStreamWithAutoFade(status);
					}
					return false;
				}
			}
			//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
			else if(command == std::string(sFSCmdLineBandwidth))
			{
				S32 band_width;
                if (i >> band_width)
                {
					band_width = llclamp(band_width, 50, 3000);
					gSavedSettings.setF32("ThrottleBandwidthKBPS", band_width);
					LLStringUtil::format_map_t args;
					std::string bw_cmd_respond;
					args["[VALUE]"] = llformat ("%d", band_width);
					bw_cmd_respond = LLTrans::getString("FSCmdLineRSP", args);
					reportToNearbyChat(bw_cmd_respond);
					return false;
                }
			}
			//</FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
			else if(command == std::string(sFSCmdLineAO))
            {
				std::string status;
                if(i >> status)
                {
					if (status == "on" )
					{
						gSavedPerAccountSettings.setBOOL("UseAO",TRUE);
					}
					else if (status == "off" )
					{
						gSavedPerAccountSettings.setBOOL("UseAO",FALSE);
					}
					else if (status == "sit" )
					{
						AOSet* tmp;
						tmp = AOEngine::instance().getSetByName(AOEngine::instance().getCurrentSetName());
						if(i >> status)
						{
							if(status == "off")
							{
								AOEngine::instance().setOverrideSits(tmp,TRUE);
							}
							else if(status == "on")
							{
								AOEngine::instance().setOverrideSits(tmp,FALSE);
							}
						}
						else
						{
							AOEngine::instance().setOverrideSits(tmp,!tmp->getSitOverride());
						}
					}
				}
				return false;
            }
			else if(command == std::string(sFSCmdLineKeyToName))
            {
                LLUUID targetKey;
                if(i >> targetKey)
                {
                    std::string object_name;
                    gCacheName->getFullName(targetKey, object_name);
                    char buffer[DB_IM_MSG_BUF_SIZE * 2];
					if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
					{
						object_name = RlvStrings::getAnonym(object_name);
					}
                    snprintf(buffer,sizeof(buffer),"%s: (%s)",targetKey.asString().c_str(), object_name.c_str());
					reportToNearbyChat(std::string(buffer));
                }
				return false;
            }
			else if(command == "/touch")
            {
                LLUUID targetKey;
                if(i >> targetKey)
                {
					LLViewerObject* myObject = gObjectList.findObject(targetKey);
					char buffer[DB_IM_MSG_BUF_SIZE * 2];

					if (!myObject)
					{
						snprintf(buffer,sizeof(buffer),"Object with key %s not found!",targetKey.asString().c_str());
						reportToNearbyChat(std::string(buffer));
						return false;
					}

					if((!rlv_handler_t::isEnabled()) || (gRlvHandler.canTouch(myObject)))
					{
						LLMessageSystem	*msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_ObjectGrab);
						msg->nextBlockFast( _PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast( _PREHASH_ObjectData);
						msg->addU32Fast(    _PREHASH_LocalID, myObject->mLocalID);
						msg->addVector3Fast(_PREHASH_GrabOffset, LLVector3::zero );
						msg->nextBlock("SurfaceInfo");
						msg->addVector3("UVCoord", LLVector3::zero);
						msg->addVector3("STCoord", LLVector3::zero);
						msg->addS32Fast(_PREHASH_FaceIndex, 0);
						msg->addVector3("Position", myObject->getPosition());
						msg->addVector3("Normal", LLVector3::zero);
						msg->addVector3("Binormal", LLVector3::zero);
						msg->sendMessage( myObject->getRegion()->getHost());

						// *NOTE: Hope the packets arrive safely and in order or else
						// there will be some problems.
						// *TODO: Just fix this bad assumption.
						msg->newMessageFast(_PREHASH_ObjectDeGrab);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_ObjectData);
						msg->addU32Fast(_PREHASH_LocalID, myObject->mLocalID);
						msg->nextBlock("SurfaceInfo");
						msg->addVector3("UVCoord", LLVector3::zero);
						msg->addVector3("STCoord", LLVector3::zero);
						msg->addS32Fast(_PREHASH_FaceIndex, 0);
						msg->addVector3("Position", myObject->getPosition());
						msg->addVector3("Normal", LLVector3::zero);
						msg->addVector3("Binormal", LLVector3::zero);
						msg->sendMessage(myObject->getRegion()->getHost());
						snprintf(buffer,sizeof(buffer),"Touched object with key %s",targetKey.asString().c_str());
						reportToNearbyChat(std::string(buffer));
					}
                }
				return false;
            }
			else if(command == "/siton")
            {
                LLUUID targetKey;
                if(i >> targetKey)
                {
					LLViewerObject* myObject = gObjectList.findObject(targetKey);
					char buffer[DB_IM_MSG_BUF_SIZE * 2];

					if (!myObject)
					{
						snprintf(buffer,sizeof(buffer),"Object with key %s not found!",targetKey.asString().c_str());
						reportToNearbyChat(std::string(buffer));
						return false;
					}
					if((!rlv_handler_t::isEnabled()) || (gRlvHandler.canSit(myObject, LLVector3::zero)))
					{
						LLMessageSystem	*msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_AgentRequestSit);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
						msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
						msg->nextBlockFast(_PREHASH_TargetObject);
						msg->addUUIDFast(_PREHASH_TargetID, targetKey);
						msg->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
						gAgent.getRegion()->sendReliableMessage();

						snprintf(buffer,sizeof(buffer),"Sat on object with key %s",targetKey.asString().c_str());
						reportToNearbyChat(std::string(buffer));
					}
                }
				return false;
            }
			else if(command == "/standup")
            {
				if((!rlv_handler_t::isEnabled()) || (gRlvHandler.canStand(	)))
				{
					gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
					reportToNearbyChat(std::string("Standing up"));
				}
				return false;
            }
			else if(command == std::string(sFSCmdLineOfferTp))
            {
                std::string avatarKey;
//				llinfos << "CMD DEBUG 0 " << command << " " << avatarName << llendl;
                if(i >> avatarKey)
                {
//				llinfos << "CMD DEBUG 0 afterif " << command << " " << avatarName << llendl;
                    LLUUID tempUUID;
                    if(LLUUID::parseUUID(avatarKey, &tempUUID))
                    {
                        char buffer[DB_IM_MSG_BUF_SIZE * 2];
                        LLDynamicArray<LLUUID> ids;
                        ids.push_back(tempUUID);
                        std::string tpMsg="Join me!";
                        LLMessageSystem* msg = gMessageSystem;
                        msg->newMessageFast(_PREHASH_StartLure);
                        msg->nextBlockFast(_PREHASH_AgentData);
                        msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
                        msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
                        msg->nextBlockFast(_PREHASH_Info);
                        msg->addU8Fast(_PREHASH_LureType, (U8)0); 

                        msg->addStringFast(_PREHASH_Message, tpMsg);
                        for(LLDynamicArray<LLUUID>::iterator itr = ids.begin(); itr != ids.end(); ++itr)
                        {
                            msg->nextBlockFast(_PREHASH_TargetData);
                            msg->addUUIDFast(_PREHASH_TargetID, *itr);
                        }
                        gAgent.sendReliableMessage();
                        snprintf(buffer,sizeof(buffer),"Offered TP to key %s",tempUUID.asString().c_str());
						reportToNearbyChat(std::string(buffer));
						return false;
                    }
                }
            }
			
			else if(command == std::string(sFSCmdLineGround))
			{
				LLVector3 agentPos = gAgent.getPositionAgent();
				U64 agentRegion = gAgent.getRegion()->getHandle();
				LLVector3 targetPos(agentPos.mV[0],agentPos.mV[1],LLWorld::getInstance()->resolveLandHeightAgent(agentPos));
				LLVector3d pos_global = from_region_handle(agentRegion);
				pos_global += LLVector3d((F64)targetPos.mV[0],(F64)targetPos.mV[1],(F64)targetPos.mV[2]);
				gAgent.teleportViaLocation(pos_global);
				return false;
			}
			else if(command == std::string(sFSCmdLineHeight))
			{
				F32 z;
				if(i >> z)
				{
					LLVector3 agentPos = gAgent.getPositionAgent();
					U64 agentRegion = gAgent.getRegion()->getHandle();
					LLVector3 targetPos(agentPos.mV[0],agentPos.mV[1],z);
					LLVector3d pos_global = from_region_handle(agentRegion);
					pos_global += LLVector3d((F64)targetPos.mV[0],(F64)targetPos.mV[1],(F64)targetPos.mV[2]);
					gAgent.teleportViaLocation(pos_global);
					return false;
				}
			}
			else if(command == std::string(sFSCmdLineTeleportHome))
			{
				gAgent.teleportHome();
				return false;
            }
			else if(command == std::string(sFSCmdLineRezPlatform))
            {
				if(!(gRlvHandler.hasBehaviour(RLV_BHVR_REZ)))
				{
					F32 width;
					if (i >> width) cmdline_rezplat(false, width);
					else cmdline_rezplat();
				}
				return false;
			}
			else if(command == std::string(sFSCmdLineMapTo))
			{
				if (revised_text.length() > command.length() + 1) //Typing this command with no argument was causing a crash. -Madgeek
				{
					LLVector3d agentPos = gAgent.getPositionGlobal();
					S32 agent_x = llround( (F32)fmod( agentPos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
					S32 agent_y = llround( (F32)fmod( agentPos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
					S32 agent_z = llround( (F32)agentPos.mdV[VZ] );
					std::string region_name = LLWeb::escapeURL(revised_text.substr(command.length()+1));
					std::string url;

					if(!sFSCmdLineMapToKeepPos)
					{
						agent_x = 128;
						agent_y = 128;
						agent_z = 0;
					}

					url = llformat("secondlife:///app/teleport/%s/%d/%d/%d",region_name.c_str(),agent_x,agent_y,agent_z);
					LLURLDispatcher::dispatch(url, "clicked", NULL, true);
				}
				return false;
			}
			else if(command == std::string(sFSCmdLineCalc))//Cryogenic Blitz
			{
				bool success;
				F32 result = 0.f;
				if(revised_text.length() > command.length() + 1)
				{

					std::string expr = revised_text.substr(command.length()+1);
					LLStringUtil::toUpper(expr);
					success = LLCalc::getInstance()->evalString(expr, result);

					std::string out;

					if (!success)
					{
						out =  "Calculation Failed";
					}
					else
					{
						// Replace the expression with the result
						std::ostringstream result_str;
						result_str << expr;
						result_str << " = ";
						result_str << result;
						out = result_str.str();
					}
					reportToNearbyChat(out);
					return false;
				}
			}
			else if(command == std::string(sFSCmdLineTP2))
			{
				if (revised_text.length() > command.length() + 1) //Typing this command with no argument was causing a crash. -Madgeek
				{
					std::string name = revised_text.substr(command.length()+1);
					cmdline_tp2name(name);
				}
				return false;
			}
			else if(revised_text == "/cs")
			{
				LLFloaterReg::showInstance("contactsets");
				reportToNearbyChat("Displaying Contact Sets Floater.");
				return false;
			}

			else if(command == std::string(sFSCmdLineClearChat))
			{
				LLFloaterNearbyChat* chat = LLFloaterReg::getTypedInstance<LLFloaterNearbyChat>("nearby_chat", LLSD());
				if(chat)
				{
					chat->clearChatHistory();
					return false;
				}
			}

			else if(command == "zdrop")
			{
				std::string setting;
				if(i >> setting)
				{
					if(setting == "on")
					{
						if(zdrop != NULL)
						{
							reportToNearbyChat("Zdrop is already active.");
						}
						else
						{
							std::string loldest;
							if(i >> loldest)
							{
								reportToNearbyChat("Beginning Zdrop.");
								reportToNearbyChat("Verifying destination prim is present inworld...");
								if(loldest.length() != 36)
								{
									reportToNearbyChat("UUID entered is of an invalid length! (Hint: use the \"copy key\" button in the build menu.)");
								}
								else if (gObjectList.findObject(LLUUID(loldest)) == false) 
								{
									reportToNearbyChat("Unable to locate object.  Please verify the object is rezzed and in view, and that the UUID is correct.");
								}
								else
								{
									std::string lolfolder;
									std::string tmp;
									while(i >> tmp)
									{
										lolfolder = lolfolder + tmp+ " ";
									}
									try
									{
										lolfolder = lolfolder.substr(0,lolfolder.length() - 1);
										LLUUID folder = gInventory.findCategoryByName(lolfolder);
										if(folder.notNull())
										{
											reportToNearbyChat("Verifying folder location...");
											std::stack<LLViewerInventoryItem*> lolstack;
											LLDynamicArray<LLPointer<LLViewerInventoryItem> > lolinv = findInventoryInFolder(lolfolder);
											for(LLDynamicArray<LLPointer<LLViewerInventoryItem> >::iterator it = lolinv.begin(); it != lolinv.end(); ++it)
											{
												LLViewerInventoryItem* item = *it;
												lolstack.push(item);
											}
											if(lolstack.size())
											{
												reportToNearbyChat(llformat("Found folder \"%s\".", lolfolder.c_str()));
												reportToNearbyChat(llformat("Found prim \"%s\".", loldest.c_str()));
												reportToNearbyChat(llformat("Transferring inventory items from \"%s\" to prim \"%s\".", lolfolder.c_str(), loldest.c_str()));
												reportToNearbyChat("WARNING: No-copy items will be moved to the destination prim!");
												reportToNearbyChat("Do not have the prim selected while transfer is running to reduce the chances of \"Inventory creation on in-world object failed.\"");
												reportToNearbyChat("Use \"Zdrop off\" to stop the transfer");
												LLUUID sdest = LLUUID(loldest);
												zdrop = new JCZdrop(lolstack, sdest, lolfolder.c_str(), loldest.c_str());
											}
										}
										else
										{
											reportToNearbyChat(llformat("\"%s\" folder not found.  Please check the spelling.", lolfolder.c_str()));
											reportToNearbyChat("Zdrop cannot work if the folder is inside another folder.");
										}
									}
									catch(std::out_of_range e)
									{
										reportToNearbyChat("The Zdrop command transfers items from your inventory to a rezzed prim without the need to wait for the contents of the prim to load.  No-copy items are moved to the prim. All other items are copied.");
										reportToNearbyChat("Valid command: Zdrop (rezzed prim UUID) (source inventory folder name)");
									}
								}
							}
							else
							{
								reportToNearbyChat("Please specify an object UUID to copy the items in this folder to.");
							}
						}
					}
					else if(setting == "off")
					{
						if(zdrop == NULL)
						{
							reportToNearbyChat("Zdrop is already deactivated.");
						}
						else
						{
							zdrop ->mRunning = TRUE;
							delete zdrop;
							zdrop = NULL;
						}
					}
					else
					{
						reportToNearbyChat(llformat("Invalid command: \"%s\".  Valid commands: Zdrop on (source inventory folder) (rezzed prim UUID) ; Zdrop off", setting.c_str()));
					}
				}
				else
				{
					reportToNearbyChat("The Zdrop command transfers items from your inventory to a rezzed prim without the need to wait for the contents of the prim to load.  No-copy items are moved to the prim. All other items are copied.");
					reportToNearbyChat("Valid commands: Zdrop on (rezzed prim UUID) (source inventory folder name) ; Zdrop off");
				}
				return false;
			}
			else if(command == "ztake")
			{
				std::string setting;
				if(i >> setting)
				{
					if(setting == "on")
					{
						if(ztake != NULL)
						{
							reportToNearbyChat("Ztake is already active.");
						}
						else
						{
							reportToNearbyChat("Beginning Ztake.");
							reportToNearbyChat("Verifying folder location...");
							std::string folder_name;
							std::string tmp;
							while(i >> tmp)
							{
								folder_name = folder_name + tmp + " ";
							}
							try
							{
								folder_name = folder_name.substr(0,folder_name.length() - 1);
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if(folder.notNull())
								{
									reportToNearbyChat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
									ztake = new JCZtake(folder);
								}
								else
								{
									reportToNearbyChat(llformat("\"%s\" folder not found.  Please check the spelling.", folder_name.c_str()));
									reportToNearbyChat("Ztake cannot work if the folder is inside another folder.");
								}
							}
							catch(std::out_of_range e)
							{
								reportToNearbyChat("Please specify a destination folder in your inventory.");
							}
						}
					}
					else if(setting == "off")
					{
						if(ztake == NULL)
						{
							reportToNearbyChat("Ztake is already deactivated.");
						}
						else
						{
							ztake->mRunning = TRUE;
							delete ztake;
							ztake = NULL;
						}
					}
					else
					{
						reportToNearbyChat(llformat("Invalid command: \"%s\".  Valid commands: Ztake on (destination inventory folder) ; Ztake off", setting.c_str()));
					}
					return false;
				}
				else
				{
					reportToNearbyChat("The Ztake command copies selected rezzed objects into the folder you specify in your inventory.");
					reportToNearbyChat("Valid commands: Ztake on (destination inventory folder name) ; Ztake off");
				}
				return false;
			}
			else if(command == "lpackage")
			{
				std::string loldest;
				if(i >> loldest)
				{
					reportToNearbyChat("Verifying destination prim is present inworld...");
					if(loldest.length() != 36)
					{
						reportToNearbyChat("UUID entered is of an invalid length! (Hint: use the \"copy key\" button in the build menu.)");
					}
					else if (gObjectList.findObject(LLUUID(loldest)) == false) 
					{
						reportToNearbyChat("Unable to locate object.  Please verify the object is rezzed, in view, and that the UUID is correct.");
					}
					else
					{
						std::string folder_name;
						std::string tmp;
						while(i >> tmp)
						{
							folder_name = folder_name + tmp + " ";
						}
						try
						{
							folder_name = folder_name.substr(0,folder_name.length() - 1);
							LLUUID folder = gInventory.findCategoryByName(folder_name);
							if(folder.notNull())
							{
								reportToNearbyChat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
								ztake = new JCZtake(folder, true, LLUUID(loldest), folder_name);
							}
							else
							{
								reportToNearbyChat(llformat("\"%s\" folder not found.  Please check the spelling.", folder_name.c_str()));
								reportToNearbyChat("The packager cannot work if the folder is inside another folder.");
							}
						}
						catch(std::out_of_range e)
						{
							reportToNearbyChat("Please specify a destination folder in your inventory.");
						}
					}
				}
				else
				{
					reportToNearbyChat(llformat("Packager usage: \"%s destination_prim_UUID inventory folder name\"",command.c_str()));
				}
				return false;
			}
			else if(command == "mtake")
			{
				std::string setting;
				if(i >> setting)
				{
					if(setting == "on")
					{
						if(mtake != NULL)
						{
							reportToNearbyChat("Mtake is already active.");
						}
						else
						{
							reportToNearbyChat("Beginning Mtake.");
							reportToNearbyChat("Verifying folder location...");
							std::string folder_name;
							std::string tmp;
							while(i >> tmp)
							{
								folder_name = folder_name + tmp + " ";
							}
							try
							{
								folder_name = folder_name.substr(0,folder_name.length() - 1);
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if(folder.notNull())
								{
									reportToNearbyChat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
									mtake = new TMZtake(folder);
								}
								else
								{
									reportToNearbyChat(llformat("\"%s\" folder not found.  Please check the spelling.", folder_name.c_str()));
									reportToNearbyChat("Mtake cannot work if the folder is inside another folder.");
								}
							}
							catch(std::out_of_range e)
							{
								reportToNearbyChat("Please specify a destination folder in your inventory.");
							}
						}
					}
					else if(setting == "off")
					{
						if(mtake == NULL)
						{
							reportToNearbyChat("Mtake is already deactivated.");
						}
						else
						{
							mtake->mRunning = TRUE;
							delete mtake;
							mtake = NULL;
						}
					}
					else
					{
						reportToNearbyChat(llformat("Invalid command: \"%s\".  Valid commands: Mtake on (destination inventory folder) ; Mtake off", setting.c_str()));
					}
					return false;
				}
				else
				{
					reportToNearbyChat("The Mtake command renames selected rezzed objects to the dimensions of the prim, then copies them into the folder you specify in your inventory.");
					reportToNearbyChat("Valid commands: Mtake on (destination inventory folder name) ; Mtake off");
				}
				return false;
			}
			else if(command == "invrepair")
			{
				invrepair();
			}
		}
	}
	return true;
}

//case insensitive search for avatar in draw distance
LLUUID cmdline_partial_name2key(std::string partial_name)
{
	std::string av_name;
	LLStringUtil::toLower(partial_name);

	LLPanel* panel_people = LLFloaterSidePanelContainer::getPanel("people", "panel_people");
	if (panel_people)
	{
		std::vector<LLPanel*> items;
		LLAvatarList* nearbyList = ((LLPanelPeople*)panel_people)->getNearbyList();
		nearbyList->getItems(items);

		for (std::vector<LLPanel*>::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
		{
			LLAvatarListItem* av = static_cast<LLAvatarListItem*>(*itItem);

			av_name = av->getUserName();

			LLStringUtil::toLower(av_name);
			if (strstr(av_name.c_str(), partial_name.c_str()))
			{
				return av->getAvatarId();
			}
		}
	}
	return LLUUID::null;
}

void cmdline_tp2name(std::string target)
{
	LLUUID avkey = cmdline_partial_name2key(target);
	LLPanel* panel_people = LLFloaterSidePanelContainer::getPanel("people", "panel_people");
	if (avkey.notNull() && panel_people)
	{
		LLAvatarListItem* avatar_list_item = ((LLPanelPeople*)panel_people)->getNearbyList()->getAvatarListItem(avkey);
		if (avatar_list_item)
		{
			LLVector3d pos = avatar_list_item->getPosition();
			pos.mdV[VZ] += 2.0;
			gAgent.teleportViaLocation(pos);
			return;
		}
	}

	reportToNearbyChat("Avatar not found.");
}

void cmdline_rezplat(bool use_saved_value, F32 visual_radius) //cmdline_rezplat() will still work... just will use the saved value
{
    LLVector3 agentPos = gAgent.getPositionAgent()+(gAgent.getVelocity()*(F32)0.333);
    LLMessageSystem* msg = gMessageSystem;
    msg->newMessageFast(_PREHASH_ObjectAdd);
    msg->nextBlockFast(_PREHASH_AgentData);
    msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
    msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
    msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
    msg->nextBlockFast(_PREHASH_ObjectData);
    msg->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);
    msg->addU8Fast(_PREHASH_Material, LL_MCODE_METAL);

    if(agentPos.mV[2] > 4096.0)msg->addU32Fast(_PREHASH_AddFlags, FLAGS_CREATE_SELECTED);
    else msg->addU32Fast(_PREHASH_AddFlags, 0);

    LLVolumeParams    volume_params;

    volume_params.setType( LL_PCODE_PROFILE_CIRCLE, 0x21 ); //TODO: make this use LL_PCODE_PATH_CIRCLE_33 again instead of the hardcoded value
    volume_params.setRatio( 2, 2 );
    volume_params.setShear( 0, 0 );
    volume_params.setTaper(2.0f,2.0f);
    volume_params.setTaperX(0.f);
    volume_params.setTaperY(0.f);

    LLVolumeMessage::packVolumeParams(&volume_params, msg);
    LLVector3 rezpos = agentPos - LLVector3(0.0f,0.0f,2.5f);
    LLQuaternion rotation;
    rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

	static LLCachedControl<F32> sFSCmdLinePlatformSize(gSavedSettings,  "FSCmdLinePlatformSize");

	if (use_saved_value) visual_radius = sFSCmdLinePlatformSize;
	F32 realsize = visual_radius / 3.0f;
	if (realsize < 0.01f) realsize = 0.01f;
	else if (realsize > 10.0f) realsize = 10.0f;

    msg->addVector3Fast(_PREHASH_Scale, LLVector3(0.01f,realsize,realsize) );
    msg->addQuatFast(_PREHASH_Rotation, rotation );
    msg->addVector3Fast(_PREHASH_RayStart, rezpos );
    msg->addVector3Fast(_PREHASH_RayEnd, rezpos );
    msg->addU8Fast(_PREHASH_BypassRaycast, (U8)1 );
    msg->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)FALSE );
    msg->addU8Fast(_PREHASH_State, 0);
    msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null );
    msg->sendReliable(gAgent.getRegionHost());
}

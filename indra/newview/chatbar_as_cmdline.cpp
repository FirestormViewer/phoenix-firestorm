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
#include "fsradar.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llavataractions.h"
#include "llcalc.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaternearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
#include "llfloaterreg.h"
#include "llinventorymodel.h"
#include "llnotificationmanager.h"
#include "llparcel.h"
#include "llslurl.h"
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
#include <boost/regex.hpp> // rand(min,max) in calc


// [RLVa:KB] - Checked by TM: 2013-11-10 (RLVa-1.4.9)
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

std::vector<U32> cmd_line_mPackagerToTake;
std::string cmd_line_mPackagerTargetFolderName;
LLUUID cmd_line_mPackagerTargetFolder;
LLUUID cmd_line_mPackagerDest;

LLViewerInventoryItem::item_array_t findInventoryInFolder(const std::string& ifolder)
{
	LLUUID folder = gInventory.findCategoryByName(ifolder);
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	gInventory.collectDescendents(folder, cats, items, FALSE);

	return items;
}

void doZdCleanup();
class JCZdrop : public LLEventTimer
{
public:
	BOOL mRunning;
	
	JCZdrop(std::stack<LLViewerInventoryItem*> stack, LLUUID dest, std::string sFolder, std::string sUUID, bool package = false) : LLEventTimer(1.f), mRunning(FALSE)
	{
		mPackage = package;
		mStack = stack;
		mDestination = dest;
		mFolderName = sFolder;
		mDropUUID = sUUID;
	}

	~JCZdrop()
	{
		doZdCleanup();
		if (mErrorCode == 1)
		{
			report_to_nearby_chat(llformat("The object with the UUID of \"%s\" can no longer be found in-world.", mDropUUID.c_str()));
			report_to_nearby_chat("This can occur if the object was returned or deleted, or if your client is no longer rendering it.");
			report_to_nearby_chat(llformat("Transfer from \"%s\" to \"%s\" aborted.", mFolderName.c_str(), mDropUUID.c_str()));
		}
		else
		{
			if (mPackage)
			{
				report_to_nearby_chat("Packager finished, you may now pick up the prim that contains the objects.");
				report_to_nearby_chat(llformat("Packaged what you had selected in world into the folder \"%s\" in your inventory and into the prim with the UUID of \"%s\"", mFolderName.c_str(), mDropUUID.c_str()));
				report_to_nearby_chat("Don't worry if you look at the contents of package right now, it may show as empty, it isn't, it's just a bug with Second Life itself.");
				report_to_nearby_chat("If you take it into your inventory then rez it back out, all the contents will be there.");
			}
			else
			{
				report_to_nearby_chat(llformat("Completed transfer from \"%s\" to \"%s\".", mFolderName.c_str(), mDropUUID.c_str()));
			}
		}
	}

	BOOL tick()
	{
		LLViewerInventoryItem* subj = mStack.top();
		mStack.pop();
		LLViewerObject* objectp = gObjectList.findObject(mDestination);
		if (objectp)
		{
			report_to_nearby_chat(std::string("transferring ") + subj->getName());
			LLToolDragAndDrop::dropInventory(objectp, subj, LLToolDragAndDrop::SOURCE_AGENT, gAgentID);
			if (mStack.size() > 0)
			{
				return mRunning;
			}
			else
			{
				return (mStack.size() == 0);
			}
		}
		else
		{
			mErrorCode = 1;
			return TRUE;
		}
	}


private:
	std::stack<LLViewerInventoryItem*> mStack;
	LLUUID mDestination;
	std::string mFolderName;
	std::string mDropUUID;
	S32 mErrorCode;
	bool mPackage;
};

JCZdrop* zdrop;

class ZdCleanup: public LLEventTimer
{
public:
	ZdCleanup() : LLEventTimer(0.2f) //Don't really need this long of a timer, but it can't be to short or tick() below may get called before the packager's tick() returns to the timer system. Things get ugly otherwise.
	{}

	~ZdCleanup() {}

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

enum EZtakeState
{
	ZTS_COUNTDOWN = 0,
	ZTS_SELECTION = 1,
	ZTS_TAKE = 2,
	ZTS_DROP = 3,
	ZTS_DONE = 4
};

void doZtCleanup();
class JCZtake : public LLEventTimer
{
public:
	BOOL mRunning;

	JCZtake(const LLUUID& target, bool package = false, LLUUID destination = LLUUID::null, std::string dtarget = "", EDeRezDestination dest = DRD_TAKE_INTO_AGENT_INVENTORY, bool use_selection = true, std::vector<U32> to_take = std::vector<U32>()) :
		LLEventTimer(0.66f),
		mTarget(target),
		mRunning(FALSE),
		mCountdown(5),
		mPackage(package),
		mPackageDest(destination),
		mDest(dest),
		mToTake(to_take),
		mPackSize(0)
	{
		if (use_selection)
		{
			mState = ZTS_COUNTDOWN;
		}
		else
		{
			mState = ZTS_TAKE;
		}
		
		mFolderName = dtarget;
		
		if (mPackage)
		{
			report_to_nearby_chat("Packager started. Phase 1 (taking in-world objects into inventory) starting in: ");
		}
		else
		{
			report_to_nearby_chat("Ztake activated. Taking selected in-world objects into inventory in: ");
		}
	}

	~JCZtake()
	{
		if (!mPackage)
		{
			report_to_nearby_chat("Ztake deactivated.");
		}
	}

	BOOL tick()
	{
		{
			switch (mState) {
				case ZTS_COUNTDOWN:
					report_to_nearby_chat(llformat("%i...", mCountdown--));
					if (mCountdown == 0) mState = ZTS_SELECTION;
					break;
					
				case ZTS_SELECTION:
					for (LLObjectSelection::root_iterator itr = LLSelectMgr::getInstance()->getSelection()->root_begin();
						itr != LLSelectMgr::getInstance()->getSelection()->root_end(); ++itr)
					{
						LLSelectNode* node = (*itr);
						LLViewerObject* objectp = node->getObject();
						U32 localid = objectp->getLocalID();
						if (mDonePrims.find(localid) == mDonePrims.end())
						{
							mDonePrims.insert(localid);
							mToTake.push_back(localid);
						}
					}
					if (mToTake.size() > 0) mState = ZTS_TAKE;
					break;
					
				case ZTS_TAKE:
					if (mToTake.size() > 0)
					{
						std::vector<LLPointer<LLViewerInventoryItem> > inventory = findInventoryInFolder(mFolderName);
						mPackSize = mToTake.size() + inventory.size();
						
						LLMessageSystem* msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_DeRezObject);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
						msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
						msg->nextBlockFast(_PREHASH_AgentBlock);
						msg->addUUIDFast(_PREHASH_GroupID, LLUUID::null);
						msg->addU8Fast(_PREHASH_Destination, mDest);
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
						
						if (mToTake.size() % 10 == 0)
						{
							if (mToTake.size() == 0)
							{
								if (mPackage)
								{
									if (mPackageDest != LLUUID::null)
									{
										mPeriod = 1.0f;
										mCountdown = 45; //reused for a basic timeout
										mState = ZTS_DROP;
									}
									else
									{
										report_to_nearby_chat("Ktake has taken all selected objects.");
										doZtCleanup();
										mState = ZTS_DONE;
									}
								}
								else
								{
									report_to_nearby_chat("Ztake has taken all selected objects. Say \"ztake off\" to deactivate ztake or select more objects to continue.");
								}
							} 
							else
							{
								if (mPackage)
								{
									report_to_nearby_chat(llformat("Packager: %i objects left to take.", mToTake.size()));
								}
								else
								{
									report_to_nearby_chat(llformat("Ztake: %i objects left to take.", mToTake.size()));
								}
							}
						}
					}
					else
					{
						if (mPackage)
						{
							report_to_nearby_chat(llformat("Packager: no objects to take."));
							doZtCleanup();
						}
						else
						{
							report_to_nearby_chat(llformat("Ztake: no objects to take."));
						}
					}
					break;
					
				case ZTS_DROP:
					{
						mCountdown --;

						std::stack<LLViewerInventoryItem*> itemstack;
						std::vector<LLPointer<LLViewerInventoryItem> > inventory = findInventoryInFolder(mFolderName);
						for (std::vector<LLPointer<LLViewerInventoryItem> >::iterator it = inventory.begin(); it != inventory.end(); ++it)
						{
							LLViewerInventoryItem* item = *it;
							itemstack.push(item);
						}

						if (itemstack.size() >= mPackSize || mCountdown == 0)
						{
							if (itemstack.size() < mPackSize) {
								report_to_nearby_chat("Phase 1 of the packager finished, but some items mave have been missed.");
							}
							else
							{
								report_to_nearby_chat("Phase 1 of the packager finished.");
							}

							report_to_nearby_chat("Do not have the destination prim selected while transfer is running to reduce the chances of \"Inventory creation on in-world object failed.\"");

							LLUUID sdest = LLUUID(mPackageDest);
							new JCZdrop(itemstack, sdest, mFolderName.c_str(), mPackageDest.asString().c_str(), true);

							doZtCleanup();
							mState = ZTS_DONE;
						}
					}
					break;

				case ZTS_DONE:
					/* nothing left to do */
					break;
			}
		}
		return mRunning;
	}

private:
	std::set<U32> mDonePrims;
	std::vector<U32> mToTake;
	LLUUID mTarget;
	S32 mCountdown;
	bool mPackage;
	LLUUID mPackageDest;
	std::string mFolderName;
	EDeRezDestination mDest;
	U32 mPackSize;
	EZtakeState mState;
};

JCZtake* ztake;

class LOZtCleanup: public LLEventTimer
{
public:
	LOZtCleanup() : LLEventTimer(0.2f) //Don't really need this long of a timer, but it can't be to short or tick() below may get called before the packager's tick() returns to the timer system. Things get ugly otherwise.
	{}

	~LOZtCleanup() {}

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
		report_to_nearby_chat("Mtake activated. Taking selected in-world objects into inventory in: ");
	}

	~TMZtake()
	{
		report_to_nearby_chat("Mtake deactivated.");
	}

	BOOL tick()
	{
		LLMessageSystem* msg = gMessageSystem;
		for (LLObjectSelection::iterator itr = LLSelectMgr::getInstance()->getSelection()->begin();
			itr != LLSelectMgr::getInstance()->getSelection()->end(); ++itr)
		{
			LLSelectNode* node = (*itr);
			LLViewerObject* object = node->getObject();
			U32 localid = object->getLocalID();
			if (mDonePrims.find(localid) == mDonePrims.end())
			{// would like to add ability to reset prim parameters to default.
				mDonePrims.insert(localid);
				std::string primX = llformat("%f", object->getScale().mV[VX]);
				std::string primY = llformat("%f", object->getScale().mV[VY]);
				std::string primZ = llformat("%f", object->getScale().mV[VZ]);
				zeroClearX = primX.find_last_not_of("0") + 1;
				primX = primX.substr(0, zeroClearX);
				zeroClearY = primY.find_last_not_of("0") + 1;
				primY = primY.substr(0, zeroClearY);
				zeroClearZ = primZ.find_last_not_of("0") + 1;
				primZ = primZ.substr(0, zeroClearZ);
				zeroClearX = primX.find_last_not_of(".") + 1;
				primX = primX.substr(0, zeroClearX);
				zeroClearY = primY.find_last_not_of(".") + 1;
				primY = primY.substr(0, zeroClearY);
				zeroClearZ = primZ.find_last_not_of(".") + 1;
				primZ = primZ.substr(0, zeroClearZ);
				std::string name = llformat("%sx%sx%s", primX.c_str(), primY.c_str(), primZ.c_str()); 
				msg->newMessageFast(_PREHASH_ObjectName);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_LocalID, localid);
				msg->addStringFast(_PREHASH_Name, name);
				gAgent.sendReliableMessage();
				mToTake.push_back(localid);
			}
		}
		if (mCountdown > 0)
		{
			report_to_nearby_chat(llformat("%i...", mCountdown--));
		}
		else if (mToTake.size() > 0)
		{
			msg->newMessageFast(_PREHASH_DeRezObject);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
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

			if (mToTake.size() % 10 == 0) 
			{
				if (mToTake.size() == 0) 
				{
					report_to_nearby_chat("Mtake has taken all selected objects. Say \"mtake off\" to deactivate Mtake or select more objects to continue.");
				} 
				else
				{
					report_to_nearby_chat(llformat("Mtake: %i objects left to take.", mToTake.size()));
				}
			}
		}

		return mRunning;
	}

private:
	std::set<U32> mDonePrims;
	std::vector<U32> mToTake;
	LLUUID mTarget;
	S32 mCountdown;
	S32 zeroClearX;
	S32 zeroClearY;
	S32 zeroClearZ;
};
TMZtake* mtake;

void invrepair()
{
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	gInventory.collectDescendents(gInventory.getRootFolderID(), cats, items, FALSE);
}

void key_to_name_callback(const LLUUID& id, const LLAvatarName& av_name)
{
	std::string name = av_name.getCompleteName();
	if (!RlvActions::canShowName(RlvActions::SNC_DEFAULT, id))
	{
		name = RlvStrings::getAnonym(name);
	}
	report_to_nearby_chat(llformat("%s: (%s)", id.asString().c_str(), name.c_str()));
}

bool cmd_line_chat(const std::string& revised_text, EChatType type, bool from_gesture)
{
	static LLCachedControl<bool> sFSCmdLine(gSavedSettings, "FSCmdLine");
	static LLCachedControl<std::string> sFSCmdLinePos(gSavedSettings, "FSCmdLinePos");
	static LLCachedControl<std::string> sFSCmdLineDrawDistance(gSavedSettings, "FSCmdLineDrawDistance");
	static LLCachedControl<std::string> sFSCmdTeleportToCam(gSavedSettings, "FSCmdTeleportToCam");
	static LLCachedControl<std::string> sFSCmdLineAO(gSavedSettings, "FSCmdLineAO");
	static LLCachedControl<std::string> sFSCmdLineKeyToName(gSavedSettings, "FSCmdLineKeyToName");
	static LLCachedControl<std::string> sFSCmdLineOfferTp(gSavedSettings, "FSCmdLineOfferTp");
	static LLCachedControl<std::string> sFSCmdLineGround(gSavedSettings, "FSCmdLineGround");
	static LLCachedControl<std::string> sFSCmdLineHeight(gSavedSettings, "FSCmdLineHeight");
	static LLCachedControl<std::string> sFSCmdLineTeleportHome(gSavedSettings, "FSCmdLineTeleportHome");
	static LLCachedControl<std::string> sFSCmdLineRezPlatform(gSavedSettings, "FSCmdLineRezPlatform");
	static LLCachedControl<std::string> sFSCmdLineMapTo(gSavedSettings, "FSCmdLineMapTo");
	static LLCachedControl<bool> sFSCmdLineMapToKeepPos(gSavedSettings, "FSCmdLineMapToKeepPos");
	static LLCachedControl<std::string> sFSCmdLineCalc(gSavedSettings, "FSCmdLineCalc");
	static LLCachedControl<std::string> sFSCmdLineTP2(gSavedSettings, "FSCmdLineTP2");
	static LLCachedControl<std::string> sFSCmdLineClearChat(gSavedSettings, "FSCmdLineClearChat");
	static LLCachedControl<std::string> sFSCmdLineMedia(gSavedSettings, "FSCmdLineMedia");
	static LLCachedControl<std::string> sFSCmdLineMusic(gSavedSettings, "FSCmdLineMusic");
	static LLCachedControl<std::string> sFSCmdLineCopyCam(gSavedSettings, "FSCmdLineCopyCam");
	static LLCachedControl<std::string> sFSCmdLineRollDice(gSavedSettings, "FSCmdLineRollDice");
	//<FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
	static LLCachedControl<std::string> sFSCmdLineBandwidth(gSavedSettings, "FSCmdLineBandWidth");
	
	if (sFSCmdLine)
	{
		std::istringstream i(revised_text);
		std::string command;
		i >> command;
		if (!command.empty())
		{
			if (command == sFSCmdLinePos())
			{
				F32 x, y, z;
				if (i >> x && i >> y)
				{
					if (!(i >> z))
					{
						z = gAgent.getPositionGlobal().mdV[VZ];
					}
					LLViewerRegion* agentRegionp = gAgent.getRegion();
					if (agentRegionp)
					{
						LLVector3 targetPos(x, y, z);
						LLVector3d pos_global = from_region_handle(agentRegionp->getHandle());
						pos_global += LLVector3d((F64)targetPos.mV[VX], (F64)targetPos.mV[VY], (F64)targetPos.mV[VZ]);
						gAgent.teleportViaLocation(pos_global);
						return false;
					}
				}
			}
			else if (command == sFSCmdLineDrawDistance())
			{
				if (from_gesture)
				{
					report_to_nearby_chat(LLTrans::getString("DrawDistanceSteppingGestureObsolete"));
					gSavedSettings.setBOOL("FSRenderFarClipStepping", TRUE);
					return false;
				}
				F32 drawDist;
				if (i >> drawDist)
				{
					gSavedSettings.setF32("RenderFarClip", drawDist);
					gAgentCamera.mDrawDistance = drawDist;
					LLStringUtil::format_map_t args;
					args["DISTANCE"] = llformat("%.0f", drawDist);
					report_to_nearby_chat(LLTrans::getString("FSCmdLineDrawDistanceSet", args));
					return false;
				}
			}
			else if (command == sFSCmdTeleportToCam())
			{
				gAgent.teleportViaLocation(gAgentCamera.getCameraPositionGlobal());
				return false;
			}
			else if (command == sFSCmdLineMedia())
			{
				std::string url;
				std::string media_type;

				if (i >> url && i >> media_type)
				{
					LLParcel *parcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
					if (parcel)
					{
						parcel->setMediaURL(url);
						parcel->setMediaType(media_type);
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
			else if (command == sFSCmdLineMusic())
			{
				std::string status;
				if (i >> status)
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
			else if (command == sFSCmdLineBandwidth())
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
					report_to_nearby_chat(bw_cmd_respond);
					return false;
				}
			}
			//</FS:HG> FIRE-6340, FIRE-6567 - Setting Bandwidth issues
			else if (command == sFSCmdLineAO())
			{
				std::string status;
				if (i >> status)
				{
					if (status == "on")
					{
						gSavedPerAccountSettings.setBOOL("UseAO", TRUE);
					}
					else if (status == "off")
					{
						gSavedPerAccountSettings.setBOOL("UseAO", FALSE);
					}
					else if (status == "sit")
					{
						AOSet* tmp;
						tmp = AOEngine::instance().getSetByName(AOEngine::instance().getCurrentSetName());
						if (i >> status)
						{
							if (status == "off")
							{
								AOEngine::instance().setOverrideSits(tmp, TRUE);
							}
							else if (status == "on")
							{
								AOEngine::instance().setOverrideSits(tmp, FALSE);
							}
						}
						else
						{
							AOEngine::instance().setOverrideSits(tmp, !tmp->getSitOverride());
						}
					}
				}
				return false;
			}
			else if (command == sFSCmdLineKeyToName())
			{
				LLUUID target_key;
				if (i >> target_key)
				{
					LLAvatarNameCache::get(target_key, boost::bind(&key_to_name_callback, _1, _2));
				}
				return false;
			}
			else if (command == "/touch")
			{
				LLUUID target_key;
				if (i >> target_key)
				{
					LLViewerObject* myObject = gObjectList.findObject(target_key);

					if (!myObject)
					{
						report_to_nearby_chat(llformat("Object with key %s not found!", target_key.asString().c_str()));
						return false;
					}

					if ( (!RlvActions::isRlvEnabled()) || (RlvActions::canTouch(myObject)) )
					{
						LLMessageSystem* msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_ObjectGrab);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
						msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
						msg->nextBlockFast(_PREHASH_ObjectData);
						msg->addU32Fast(_PREHASH_LocalID, myObject->mLocalID);
						msg->addVector3Fast(_PREHASH_GrabOffset, LLVector3::zero);
						msg->nextBlock("SurfaceInfo");
						msg->addVector3("UVCoord", LLVector3::zero);
						msg->addVector3("STCoord", LLVector3::zero);
						msg->addS32Fast(_PREHASH_FaceIndex, 0);
						msg->addVector3("Position", myObject->getPosition());
						msg->addVector3("Normal", LLVector3::zero);
						msg->addVector3("Binormal", LLVector3::zero);
						msg->sendMessage(myObject->getRegion()->getHost());

						// *NOTE: Hope the packets arrive safely and in order or else
						// there will be some problems.
						// *TODO: Just fix this bad assumption.
						msg->newMessageFast(_PREHASH_ObjectDeGrab);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
						msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
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
						report_to_nearby_chat(llformat("Touched object with key %s", target_key.asString().c_str()));
					}
				}
				return false;
			}
			else if (command == "/siton")
			{
				LLUUID target_key;
				if (i >> target_key)
				{
					LLViewerObject* myObject = gObjectList.findObject(target_key);

					if (!myObject)
					{
						report_to_nearby_chat(llformat("Object with key %s not found!", target_key.asString().c_str()));
						return false;
					}
					if ((!RlvActions::isRlvEnabled()) || (RlvActions::canSit(myObject, LLVector3::zero)))
					{
						LLMessageSystem	*msg = gMessageSystem;
						msg->newMessageFast(_PREHASH_AgentRequestSit);
						msg->nextBlockFast(_PREHASH_AgentData);
						msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
						msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
						msg->nextBlockFast(_PREHASH_TargetObject);
						msg->addUUIDFast(_PREHASH_TargetID, target_key);
						msg->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
						gAgent.getRegion()->sendReliableMessage();

						report_to_nearby_chat(llformat("Sat on object with key %s", target_key.asString().c_str()));
					}
				}
				return false;
			}
			else if (command == "/standup")
			{
				if ( (!RlvActions::isRlvEnabled()) || (RlvActions::canStand()) )
				{
					gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
					report_to_nearby_chat(std::string("Standing up"));
				}
				return false;
			}
			else if (command == sFSCmdLineOfferTp())
			{
				LLUUID target_key;
				if (i >> target_key)
				{
					const std::string tpMsg = "Join me!"; // Intentionally left English because this is the message the other avatar gets
					LLMessageSystem* msg = gMessageSystem;
					msg->newMessageFast(_PREHASH_StartLure);
					msg->nextBlockFast(_PREHASH_AgentData);
					msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
					msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
					msg->nextBlockFast(_PREHASH_Info);
					msg->addU8Fast(_PREHASH_LureType, (U8)0);
					msg->addStringFast(_PREHASH_Message, tpMsg);
					msg->nextBlockFast(_PREHASH_TargetData);
					msg->addUUIDFast(_PREHASH_TargetID, target_key);
					gAgent.sendReliableMessage();
					LLStringUtil::format_map_t args;
					args["NAME"] = LLSLURL("agent", target_key, "inspect").getSLURLString();
					report_to_nearby_chat(LLTrans::getString("FSCmdLineTpOffered", args));
				}
				return false;
			}
			
			else if (command == sFSCmdLineGround())
			{
				LLVector3 agentPos = gAgent.getPositionAgent();
				U64 agentRegion = gAgent.getRegion()->getHandle();
				LLVector3 targetPos(agentPos.mV[VX],agentPos.mV[VY], LLWorld::getInstance()->resolveLandHeightAgent(agentPos));
				LLVector3d pos_global = from_region_handle(agentRegion);
				pos_global += LLVector3d((F64)targetPos.mV[VX], (F64)targetPos.mV[VY], (F64)targetPos.mV[VZ]);
				if (RlvActions::canTeleportToLocal(pos_global))
				{
					gAgent.teleportViaLocation(pos_global);
				}
				return false;
			}
			else if (command == sFSCmdLineHeight())
			{
				F32 z;
				if (i >> z)
				{
					LLVector3 agentPos = gAgent.getPositionAgent();
					U64 agentRegion = gAgent.getRegion()->getHandle();
					LLVector3 targetPos(agentPos.mV[VX], agentPos.mV[VY], z);
					LLVector3d pos_global = from_region_handle(agentRegion);
					pos_global += LLVector3d((F64)targetPos.mV[VX], (F64)targetPos.mV[VY], (F64)targetPos.mV[VZ]);
					if (RlvActions::canTeleportToLocal(pos_global))
					{
						gAgent.teleportViaLocation(pos_global);
					}
					return false;
				}
			}
			else if (command == sFSCmdLineTeleportHome())
			{
				gAgent.teleportHome();
				return false;
			}
			else if (command == sFSCmdLineRezPlatform())
			{
				if (RlvActions::canRez())
				{
					F32 width;
					if (i >> width)
					{
						cmdline_rezplat(false, width);
					}
					else
					{
						cmdline_rezplat();
					}
				}
				return false;
			}
			else if (command == sFSCmdLineMapTo())
			{
				if (revised_text.length() > command.length() + 1) //Typing this command with no argument was causing a crash. -Madgeek
				{
					LLVector3d agentPos = gAgent.getPositionGlobal();
					S32 agent_x = ll_round( (F32)fmod( agentPos.mdV[VX], (F64)REGION_WIDTH_METERS ) );
					S32 agent_y = ll_round( (F32)fmod( agentPos.mdV[VY], (F64)REGION_WIDTH_METERS ) );
					S32 agent_z = ll_round( (F32)agentPos.mdV[VZ] );
					std::string region_name = LLWeb::escapeURL(revised_text.substr(command.length() + 1));
					std::string url;

					if (!sFSCmdLineMapToKeepPos)
					{
						agent_x = 128;
						agent_y = 128;
						agent_z = 0;
					}

					url = llformat("secondlife:///app/teleport/%s/%d/%d/%d", region_name.c_str(), agent_x, agent_y, agent_z);
					LLURLDispatcher::dispatch(url, "clicked", NULL, true);
				}
				return false;
			}
			else if (command == sFSCmdLineCalc())//Cryogenic Blitz
			{
				bool success;
				F32 result = 0.f;
				if (revised_text.length() > command.length() + 1)
				{

					std::string expr = revised_text.substr(command.length()+1);
					LLStringUtil::toUpper(expr);
					std::string original_expr = expr;

					// < rand(min,max) >
					S32 loop_attempts = 0;
					boost::cmatch random_nums;
					const boost::regex expression("RAND\\(([0-9-]+),([0-9-]+)\\)");
					while (boost::regex_search(expr.c_str(), random_nums, expression) && loop_attempts < 5) // No more than five rand() in one expression please (performance)
					{
						++loop_attempts;
						S32 random_min = atoi(random_nums[1].first);
						S32 random_max = atoi(random_nums[2].first);
						std::string look_for = "RAND(" + llformat("%d", random_min) + "," + llformat("%d", random_max) + ")";
						S32 random_string_pos = expr.find(look_for);
						if (random_string_pos != std::string::npos)
						{
							S32 random_number = 0;
							if (random_max > random_min && random_min >= -10000 && random_min <= 10000 && random_max >= -10000 && random_max <= 10000) // Generate a random number only when max > min, and when they're in rational range
							{
								S32 random_calculated = (random_max - random_min) + 1;
								if (random_calculated != 0) // Don't divide by zero
								{
									random_number = random_min + (rand() % random_calculated);
								}
							}
							else
							{
								LLStringUtil::format_map_t args;
								args["RAND"] = llformat("%s", look_for.c_str());
								report_to_nearby_chat(LLTrans::getString("FSCmdLineCalcRandError", args));
							}
							std::string random_number_text = llformat("%d", random_number);
							expr.replace(random_string_pos, look_for.length(), random_number_text);
						}
					}
					// </ rand(min,max) >

					success = LLCalc::getInstance()->evalString(expr, result);

					std::string out;

					if (!success)
					{
						out = "Calculation Failed";
					}
					else
					{
						// Replace the expression with the result
						std::ostringstream result_str;
						result_str << original_expr;
						result_str << " = ";
						result_str << result;
						out = result_str.str();
					}
					report_to_nearby_chat(out);
					return false;
				}
			}
			else if (command == sFSCmdLineTP2())
			{
				if (revised_text.length() > command.length() + 1) //Typing this command with no argument was causing a crash. -Madgeek
				{
					std::string name = revised_text.substr(command.length() + 1);
					cmdline_tp2name(name);
				}
				return false;
			}

			else if (command == sFSCmdLineClearChat())
			{
				FSFloaterNearbyChat* chat = LLFloaterReg::getTypedInstance<FSFloaterNearbyChat>("fs_nearby_chat", LLSD());
				if (chat)
				{
					chat->clearChatHistory();
					return false;
				}
			}

			else if (command == "zdrop")
			{
				std::string setting;
				if (i >> setting)
				{
					if (setting == "on")
					{
						if (zdrop)
						{
							report_to_nearby_chat("Zdrop is already active.");
						}
						else
						{
							std::string destination;
							if (i >> destination)
							{
								report_to_nearby_chat("Beginning Zdrop.");
								report_to_nearby_chat("Verifying destination prim is present inworld...");
								if (!LLUUID::validate(destination))
								{
									report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
								}
								else if (!gObjectList.findObject(LLUUID(destination)))
								{
									report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed and in view, and that the UUID is correct.");
								}
								else
								{
									std::string folder;
									std::string tmp;
									while (i >> tmp)
									{
										folder = folder + tmp + " ";
									}
									try
									{
										folder = folder.substr(0, folder.length() - 1);
										LLUUID folder_id = gInventory.findCategoryByName(folder);
										if (folder_id.notNull())
										{
											report_to_nearby_chat("Verifying folder location...");
											std::stack<LLViewerInventoryItem*> inventorystack;
											std::vector<LLPointer<LLViewerInventoryItem> > inventory = findInventoryInFolder(folder);
											for (std::vector<LLPointer<LLViewerInventoryItem> >::iterator it = inventory.begin(); it != inventory.end(); ++it)
											{
												LLViewerInventoryItem* item = *it;
												inventorystack.push(item);
											}
											if (inventorystack.size())
											{
												report_to_nearby_chat(llformat("Found folder \"%s\".", folder.c_str()));
												report_to_nearby_chat(llformat("Found prim \"%s\".", destination.c_str()));
												report_to_nearby_chat(llformat("Transferring inventory items from \"%s\" to prim \"%s\".", folder.c_str(), destination.c_str()));
												report_to_nearby_chat("WARNING: No-copy items will be moved to the destination prim!");
												report_to_nearby_chat("Do not have the prim selected while transfer is running to reduce the chances of \"Inventory creation on in-world object failed.\"");
												report_to_nearby_chat("Use \"zdrop off\" to stop the transfer");
												LLUUID sdest = LLUUID(destination);
												zdrop = new JCZdrop(inventorystack, sdest, folder.c_str(), destination.c_str());
											}
										}
										else
										{
											report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder.c_str()));
											report_to_nearby_chat("Zdrop cannot work if the folder is inside another folder.");
										}
									}
									catch (std::out_of_range)
									{
										report_to_nearby_chat("The Zdrop command transfers items from your inventory to a rezzed prim without the need to wait for the contents of the prim to load. No-copy items are moved to the prim. All other items are copied.");
										report_to_nearby_chat("Valid command: Zdrop (rezzed prim UUID) (source inventory folder name)");
									}
								}
							}
							else
							{
								report_to_nearby_chat("Please specify an object UUID to copy the items in this folder to.");
							}
						}
					}
					else if (setting == "off")
					{
						if (!zdrop)
						{
							report_to_nearby_chat("Zdrop is already deactivated.");
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
						report_to_nearby_chat(llformat("Invalid command: \"%s\". Valid commands: zdrop on (source inventory folder) (rezzed prim UUID); zdrop off", setting.c_str()));
					}
				}
				else
				{
					report_to_nearby_chat("The Zdrop command transfers items from your inventory to a rezzed prim without the need to wait for the contents of the prim to load. No-copy items are moved to the prim. All other items are copied.");
					report_to_nearby_chat("Valid commands: zdrop on (rezzed prim UUID) (source inventory folder name); zdrop off");
				}
				return false;
			}
			else if (command == "ztake")
			{
				std::string setting;
				if (i >> setting)
				{
					if (setting == "on")
					{
						if (ztake)
						{
							report_to_nearby_chat("Ztake is already active.");
						}
						else
						{
							report_to_nearby_chat("Beginning Ztake.");
							report_to_nearby_chat("Verifying folder location...");
							std::string folder_name;
							std::string tmp;
							while (i >> tmp)
							{
								folder_name = folder_name + tmp + " ";
							}
							try
							{
								folder_name = folder_name.substr(0, folder_name.length() - 1);
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if (folder.notNull())
								{
									report_to_nearby_chat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
									ztake = new JCZtake(folder);
								}
								else
								{
									report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
									report_to_nearby_chat("Ztake cannot work if the folder is inside another folder.");
								}
							}
							catch (std::out_of_range)
							{
								report_to_nearby_chat("Please specify a destination folder in your inventory.");
							}
						}
					}
					else if (setting == "off")
					{
						if (!ztake)
						{
							report_to_nearby_chat("Ztake is already deactivated.");
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
						report_to_nearby_chat(llformat("Invalid command: \"%s\". Valid commands: ztake on (destination inventory folder); ztake off", setting.c_str()));
					}
					return false;
				}
				else
				{
					report_to_nearby_chat("The Ztake command copies selected rezzed objects into the folder you specify in your inventory.");
					report_to_nearby_chat("Valid commands: ztake on (destination inventory folder name); ztake off");
				}
				return false;
			}
			else if (command == "lpackage" || command == "cpackage")
			{
				std::string destination;
				if (i >> destination)
				{
					report_to_nearby_chat("Verifying destination prim is present inworld...");
					if (!LLUUID::validate(destination))
					{
						report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
					}
					else if (!gObjectList.findObject(LLUUID(destination)))
					{
						report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct.");
					}
					else
					{
						std::string folder_name;
						std::string tmp;
						while (i >> tmp)
						{
							folder_name = folder_name + tmp + " ";
						}
						try
						{
							folder_name = folder_name.substr(0, folder_name.length() - 1);
							LLUUID folder = gInventory.findCategoryByName(folder_name);
							if (folder.notNull())
							{
								report_to_nearby_chat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
								ztake = new JCZtake(folder, true, LLUUID(destination), folder_name, (command == "cpackage") ? DRD_ACQUIRE_TO_AGENT_INVENTORY : DRD_TAKE_INTO_AGENT_INVENTORY);
							}
							else
							{
								report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
								report_to_nearby_chat("The packager cannot work if the folder is inside another folder.");
							}
						}
						catch (std::out_of_range)
						{
							report_to_nearby_chat("Please specify a destination folder in your inventory.");
						}
					}
				}
				else
				{
					report_to_nearby_chat(llformat("Packager usage: \"%s destination_prim_UUID inventory folder name\"",command.c_str()));
				}
				return false;
			}
			else if (command == "kpackage")
			{
				std::string destination;
				if (i >> destination)
				{
					report_to_nearby_chat("Verifying destination prim is present inworld...");
					if (!LLUUID::validate(destination))
					{
						report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
					}
					else if (!gObjectList.findObject(LLUUID(destination)))
					{
						report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct.");
					}
					else
					{
						std::string folder_name;
						if (i >> folder_name)
						{
							try
							{
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if (folder.notNull())
								{
									std::vector<U32> to_take;
									
									std::string take;
									while (i >> take)
									{
										
										if (!LLUUID::validate(take))
										{
											report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
											return false;
										}
										else
										{
											LLViewerObject* objectp = gObjectList.findObject(LLUUID(take));
											if(!objectp)
											{
												report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct.");
												return false;
											}
											else
											{
												U32 localid = objectp->getLocalID();
												if (std::find(to_take.begin(), to_take.end(), localid) == to_take.end())
												{
													to_take.push_back(localid);
												}
											}
										}
									}
									
									if (to_take.empty())
									{
										report_to_nearby_chat("No objects to take.");
									}
									else
									{
										report_to_nearby_chat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
										ztake = new JCZtake(folder, true, LLUUID(destination), folder_name, DRD_ACQUIRE_TO_AGENT_INVENTORY, false, to_take);
									}
								}
								else
								{
									report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
									report_to_nearby_chat("The packager cannot work if the folder is inside another folder.");
								}
							}
							catch (std::out_of_range)
							{
								report_to_nearby_chat("Please specify a destination folder in your inventory.");
							}
						}
					}
				}
				else
				{
					report_to_nearby_chat(llformat("Packager usage: \"%s destination_prim_UUID inventory folder name\"",command.c_str()));
				}
				return false;
			}
			else if (command == "kpackagerstart")
			{
				// kpackagerstart UUID_of_object_to_put_objects Some_existing_inventory_folder
				std::string destination;
				if (i >> destination)
				{
					report_to_nearby_chat("Verifying destination prim is present inworld...");
					if (!LLUUID::validate(destination))
					{
						report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
					}
					else if (!gObjectList.findObject(LLUUID(destination)))
					{
						report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct.");
					}
					else
					{
						std::string folder_name;
						if (i >> folder_name)
						{
							try
							{
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if (folder.notNull())
								{
									report_to_nearby_chat(llformat("kpackager started. Destination folder: \"%s\" Listening to object: \"%s\"", folder_name.c_str(), destination.c_str()));
									
									cmd_line_mPackagerToTake.clear();
									cmd_line_mPackagerTargetFolderName = folder_name;
									cmd_line_mPackagerTargetFolder = folder;
									cmd_line_mPackagerDest = LLUUID(destination);
								}
								else
								{
									report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
									report_to_nearby_chat("The packager cannot work if the folder is inside another folder.");
								}
							}
							catch (std::out_of_range)
							{
								report_to_nearby_chat("Please specify a destination folder in your inventory.");
							}
						}
					}
				}
				else
				{
					report_to_nearby_chat(llformat("Packager usage: \"%s destination_prim_UUID inventory folder name\"",command.c_str()));
				}
				return false;
			}
			else if (command == "kpackagerstop") {
				if (!cmd_line_mPackagerDest.isNull()) {
					cmd_line_mPackagerToTake.clear();
					cmd_line_mPackagerTargetFolderName = "";
					cmd_line_mPackagerTargetFolder.setNull();
					cmd_line_mPackagerDest.setNull();
					report_to_nearby_chat("Packager: Stopped and cleared.");
					return false;
				}
			}
			else if (command == "ktake" || command == "kcopy")
			{
				std::string folder_name;
				if (i >> folder_name)
				{
					try
					{
						LLUUID folder = gInventory.findCategoryByName(folder_name);
						if (folder.notNull())
						{
							std::vector<U32> to_take;
							
							std::string take;
							while (i >> take)
							{
								
								if (!LLUUID::validate(take))
								{
									report_to_nearby_chat("Entered UUID is invalid! (Hint: use the \"copy key\" button in the build menu.)");
									return false;
								}
								else
								{
									LLViewerObject* objectp = gObjectList.findObject(LLUUID(take));
									if(!objectp)
									{
										report_to_nearby_chat("Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct.");
										return false;
									}
									else
									{
										U32 localid = objectp->getLocalID();
										if (std::find(to_take.begin(), to_take.end(), localid) == to_take.end())
										{
											to_take.push_back(localid);
										}
									}
								}
							}
							
							if (to_take.empty())
							{
								report_to_nearby_chat("No objects to take.");
							}
							else
							{
								report_to_nearby_chat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
								ztake = new JCZtake(folder, true, LLUUID::null, folder_name, (command == "kcopy") ? DRD_ACQUIRE_TO_AGENT_INVENTORY : DRD_TAKE_INTO_AGENT_INVENTORY, false, to_take);
							}
						}
						else
						{
							report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
							report_to_nearby_chat("The packager cannot work if the folder is inside another folder.");
						}
					}
					catch (std::out_of_range)
					{
						report_to_nearby_chat("Please specify a destination folder in your inventory.");
					}
				}
				return false;
			}
			else if (command == "mtake")
			{
				std::string setting;
				if (i >> setting)
				{
					if (setting == "on")
					{
						if (mtake)
						{
							report_to_nearby_chat("Mtake is already active.");
						}
						else
						{
							report_to_nearby_chat("Beginning Mtake.");
							report_to_nearby_chat("Verifying folder location...");
							std::string folder_name;
							std::string tmp;
							while (i >> tmp)
							{
								folder_name = folder_name + tmp + " ";
							}
							try
							{
								folder_name = folder_name.substr(0, folder_name.length() - 1);
								LLUUID folder = gInventory.findCategoryByName(folder_name);
								if (folder.notNull())
								{
									report_to_nearby_chat(llformat("Found destination folder \"%s\".", folder_name.c_str()));
									mtake = new TMZtake(folder);
								}
								else
								{
									report_to_nearby_chat(llformat("\"%s\" folder not found. Please check the spelling.", folder_name.c_str()));
									report_to_nearby_chat("Mtake cannot work if the folder is inside another folder.");
								}
							}
							catch (std::out_of_range)
							{
								report_to_nearby_chat("Please specify a destination folder in your inventory.");
							}
						}
					}
					else if (setting == "off")
					{
						if (!mtake)
						{
							report_to_nearby_chat("Mtake is already deactivated.");
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
						report_to_nearby_chat(llformat("Invalid command: \"%s\". Valid commands: mtake on (destination inventory folder); mtake off", setting.c_str()));
					}
					return false;
				}
				else
				{
					report_to_nearby_chat("The Mtake command renames selected rezzed objects to the dimensions of the prim, then copies them into the folder you specify in your inventory.");
					report_to_nearby_chat("Valid commands: mtake on (destination inventory folder name); mtake off");
				}
				return false;
			}
			else if (command == "invrepair")
			{
				invrepair();
			}
			else if (command == sFSCmdLineCopyCam())
			{
				LLViewerRegion* agentRegionp = gAgent.getRegion();
				if (agentRegionp)
				{
					LLVector3 cameraPosition = gAgentCamera.getCameraPositionAgent();
					std::string cameraPositionString = llformat("<%.3f, %.3f, %.3f>",
						cameraPosition.mV[VX], cameraPosition.mV[VY], cameraPosition.mV[VZ]);
					LLUI::sWindow->copyTextToClipboard(utf8str_to_wstring(cameraPositionString));

					LLStringUtil::format_map_t args;
					args["[POS]"] = cameraPositionString;
					report_to_nearby_chat(LLTrans::getString("FSCameraPositionCopied", args));
				}
				else
				{
					report_to_nearby_chat("Could not get a valid region pointer.");
				}
				return false;
			}
			else if (command == sFSCmdLineRollDice())
			{
				S32 dice;
				S32 faces;
				S32 result = 0;
				std::string modifier_type = "";
				if (i >> dice && i >> faces)
				{
					if (dice > 0 && faces > 0 && dice < 101 && faces < 1001)
					{
						// For viewer performance - max 100 dice and 1000 faces per die at once
						S32 modifier = 0;
						S32 successes = 0;
						S32 modifier_error = 0;

						if (i >> modifier_type && i >> modifier)
						{
							// 2d20+5, 2d20-5 / 2d20>5, 2d20<5 / 2d20!>5, 2d20!<5, 2d20!5 / 2d20!p>5, 2d20!p<5, 2d20!p5 / 2d20r>5, 2d20r<5, 2d20r5
							LLStringUtil::toLower(modifier_type);
							if (modifier < -1000 || modifier > 1000 || (modifier_type != "+" && modifier_type != "-" && modifier_type != "<" && modifier_type != ">" && modifier_type != "!>" && modifier_type != "!<" && modifier_type != "!" && modifier_type != "!p" && modifier_type != "!p>" && modifier_type != "!p<" && modifier_type != "r" && modifier_type != "r>" && modifier_type != "r<"))
							{
								modifier_error = 1;
							}
						}
						else if (!modifier_type.empty())
						{
							// Modifier type invalid
							modifier_error = 1;
						}

						if (modifier_error == 1)
						{
							LLStringUtil::format_map_t args;
							args["COMMAND"] = llformat("%s", std::string(sFSCmdLineRollDice).c_str());
							report_to_nearby_chat(LLTrans::getString("FSCmdLineRollDiceModifiersInvalid", args));
							return false;
						}

						S32 result_per_die = 0;
						S32 die_iter = 1;
						S32 freeze_guard = 0;
						S32 die_penetrated = 0;
						while (die_iter <= dice)
						{

							// Each die may have a different value rolled
							result_per_die = 1 + (rand() % faces);
							if (die_penetrated == 1)
							{
								result_per_die -= 1;
								die_penetrated = 0;
								report_to_nearby_chat(llformat("#%d 1d%d-1: %d.", die_iter, faces, result_per_die));
							}
							else
							{
								report_to_nearby_chat(llformat("#%d 1d%d: %d.", die_iter, faces, result_per_die));
							}
							result += result_per_die;
							++die_iter;

							if (modifier_type == "<")
							{
								// Modifier: Successes lower than a value
								if (result_per_die <= modifier)
								{
									report_to_nearby_chat("  ^-- " + LLTrans::getString("FSCmdLineRollDiceSuccess"));
									++successes;
								}
								else
								{
									result -= result_per_die;
								}
							}
							else if (modifier_type == ">")
							{
								// Modifier: Successes greater than a value
								if (result_per_die >= modifier)
								{
									report_to_nearby_chat("  ^-- " + LLTrans::getString("FSCmdLineRollDiceSuccess"));
									++successes;
								}
								else
								{
									result -= result_per_die;
								}
							}
							else if ((modifier_type == "!" && result_per_die == modifier) || (modifier_type == "!>" && result_per_die >= modifier) || (modifier_type == "!<" && result_per_die <= modifier))
							{
								// Modifier: Exploding dice
								report_to_nearby_chat("  ^-- " + LLTrans::getString("FSCmdLineRollDiceExploded"));
								--die_iter;
							}
							else if ((modifier_type == "!p" && result_per_die == modifier) || (modifier_type == "!p>" && result_per_die >= modifier) || (modifier_type == "!p<" && result_per_die <= modifier))
							{
								// Modifier: Penetrating dice (special style of exploding dice)
								report_to_nearby_chat("  ^-- " + LLTrans::getString("FSCmdLineRollDicePenetrated"));
								die_penetrated = 1;
								--die_iter;
							}
							else if ((modifier_type == "r" && result_per_die == modifier) || (modifier_type == "r>" && result_per_die >= modifier) || (modifier_type == "r<" && result_per_die <= modifier))
							{
								// Modifier: Reroll
								result -= result_per_die;
								report_to_nearby_chat("  ^-- " + LLTrans::getString("FSCmdLineRollDiceReroll"));
								--die_iter;
							}

							++freeze_guard;
							if (freeze_guard > 1000)
							{
								// More than 1000 iterations already? We probably have an infinite loop - kill all further rolls
								// Explosions can trigger this easily, "rolld 1 6 !> 0" for example
								die_iter = 102;
								report_to_nearby_chat(LLTrans::getString("FSCmdLineRollDiceFreezeGuard"));
								return false;
							}
						}

						if (!modifier_type.empty())
						{
							if (modifier_type == "+")
							{
								// Modifier: Bonus
								result += modifier;
							}
							else if (modifier_type == "-")
							{
								// Modifier: Penalty
								result -= modifier;
							}
							else if (modifier_type == ">" || modifier_type == "<")
							{
								// Modifier: Successes
								report_to_nearby_chat(LLTrans::getString("FSCmdLineRollDiceSuccess") + ": " + llformat("%d", successes));
							}
							modifier_type = modifier_type + llformat("%d", modifier);
						}
					}
					else
					{
						report_to_nearby_chat(LLTrans::getString("FSCmdLineRollDiceLimits"));
						return false;
					}
				}
				else
				{
					// Roll a default die, if no parameters were provided
					dice = 1;
					faces = 6;
					result = 1 + (rand() % 6);
				}
				LLStringUtil::format_map_t args;
				args["DICE"] = llformat("%d", dice);
				args["FACES"] = llformat("%d", faces);
				args["RESULT"] = llformat("%d", result);
				args["MODIFIER"] = llformat("%s", modifier_type.c_str());
				report_to_nearby_chat(LLTrans::getString("FSCmdLineRollDiceTotal", args));
				return false;
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

	FSRadar* radar = FSRadar::getInstance();
	if (radar)
	{
		FSRadar::entry_map_t radar_list = radar->getRadarList();
		FSRadar::entry_map_t::iterator it_end = radar_list.end();
		for (FSRadar::entry_map_t::iterator it = radar_list.begin(); it != it_end; ++it)
		{
			FSRadarEntry* entry = it->second;
			av_name = entry->getUserName();

			LLStringUtil::toLower(av_name);
			if (strstr(av_name.c_str(), partial_name.c_str()))
			{
				return entry->getId();
			}
		}
	}
	return LLUUID::null;
}

void cmdline_tp2name(const std::string& target)
{
	LLUUID avkey = cmdline_partial_name2key(target);
	if (avkey.notNull())
	{
		LLAvatarActions::teleportTo(avkey);
	}
}

void cmdline_rezplat(bool use_saved_value, F32 visual_radius) //cmdline_rezplat() will still work... just will use the saved value
{
	LLVector3 agentPos = gAgent.getPositionAgent() + (gAgent.getVelocity() * 0.333f);
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessageFast(_PREHASH_ObjectAdd);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, FSCommon::getGroupForRezzing());
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU8Fast(_PREHASH_PCode, LL_PCODE_VOLUME);
	msg->addU8Fast(_PREHASH_Material, LL_MCODE_METAL);

	if (agentPos.mV[VZ] > 4096.0f)
	{
		msg->addU32Fast(_PREHASH_AddFlags, FLAGS_CREATE_SELECTED);
	}
	else
	{
		msg->addU32Fast(_PREHASH_AddFlags, 0);
	}

	LLVolumeParams volume_params;

	volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
	volume_params.setBeginAndEndS(0.f, 1.f);
	volume_params.setBeginAndEndT(0.f, 1.f);
	volume_params.setRatio(1.f, 1.f);
	volume_params.setShear(0.f, 0.f);

	LLVolumeMessage::packVolumeParams(&volume_params, msg);
	LLVector3 rezpos = agentPos - LLVector3(0.0f, 0.0f, 2.5f);
	LLQuaternion rotation;
	rotation.setQuat(90.f * DEG_TO_RAD, LLVector3::y_axis);

	static LLCachedControl<F32> sFSCmdLinePlatformSize(gSavedSettings, "FSCmdLinePlatformSize");

	if (use_saved_value)
	{
		visual_radius = sFSCmdLinePlatformSize;
	}
	F32 max_scale = LLWorld::getInstance()->getRegionMaxPrimScale();
	F32 min_scale = LLWorld::getInstance()->getRegionMinPrimScale();
	F32 realsize = visual_radius;
	if (realsize < min_scale)
	{
		realsize = min_scale;
	}
	else if (realsize > max_scale)
	{
		realsize = max_scale;
	}

	msg->addVector3Fast(_PREHASH_Scale, LLVector3(0.01f, realsize, realsize));
	msg->addQuatFast(_PREHASH_Rotation, rotation);
	msg->addVector3Fast(_PREHASH_RayStart, rezpos);
	msg->addVector3Fast(_PREHASH_RayEnd, rezpos);
	msg->addU8Fast(_PREHASH_BypassRaycast, (U8)1);
	msg->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)FALSE);
	msg->addU8Fast(_PREHASH_State, 0);
	msg->addUUIDFast(_PREHASH_RayTargetID, LLUUID::null);
	msg->sendReliable(gAgent.getRegionHost());
}

bool cmdline_packager(const std::string& message, const LLUUID& fromID, const LLUUID& ownerID) {
	
	if (message.empty() || cmd_line_mPackagerDest.isNull() || fromID != cmd_line_mPackagerDest)
	{
		return false;
	}
	
	std::string cmd = message.substr(0, 12);
	
	if (cmd == "kpackageradd") {
		//
		std::string csv = message.substr(13, -1);
		std::string::size_type start = 0;
		std::string::size_type comma = 0;
		do 
		{
			comma = csv.find(",", start);
			if (comma == std::string::npos)
			{
				comma = csv.length();
			}
			std::string item(csv, start, comma-start);
			LLStringUtil::trim(item);
			LLViewerObject* objectp = gObjectList.findObject(LLUUID(item));
			if(!objectp)
			{
				report_to_nearby_chat(llformat("Packager: Unable to locate object. Please verify the object is rezzed, in view, and that the UUID is correct: \"%s\"", item.c_str()));
				return false;
			}
			else
			{
				U32 localid = objectp->getLocalID();
				if (std::find(cmd_line_mPackagerToTake.begin(), cmd_line_mPackagerToTake.end(), localid) == cmd_line_mPackagerToTake.end())
				{
					cmd_line_mPackagerToTake.push_back(localid);
				}
			}
			start = comma + 1;

		}
		while(comma < csv.length());
		
		report_to_nearby_chat(llformat("Packager: adding objects: \"%s\"", csv.c_str()));
		return true;
	}
	else if (cmd == "kpackagerend") {
		
		report_to_nearby_chat("Packager: finilizing.");
		ztake = new JCZtake(cmd_line_mPackagerTargetFolder, true, cmd_line_mPackagerDest, cmd_line_mPackagerTargetFolderName, DRD_ACQUIRE_TO_AGENT_INVENTORY, false, cmd_line_mPackagerToTake);
		cmd_line_mPackagerToTake.clear();
		cmd_line_mPackagerTargetFolderName = "";
		cmd_line_mPackagerTargetFolder.setNull();
		cmd_line_mPackagerDest.setNull();
	}
	return false;
}

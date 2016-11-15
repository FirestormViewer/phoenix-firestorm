/** 
 * @file llviewercontrol.cpp
 * @brief Viewer configuration
 * @author Richard Nelson
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

#include "llviewercontrol.h"

// Library includes
#include "llwindow.h"	// getGamma()

// For Listeners
#include "llaudioengine.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llconsole.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolterrain.h"
#include "llflexibleobject.h"
#include "llfeaturemanager.h"
#include "llviewershadermgr.h"

#include "llsky.h"
#include "llvieweraudio.h"
#include "llviewermenu.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvoiceclient.h"
#include "llvosky.h"
#include "llvotree.h"
#include "llvovolume.h"
#include "llworld.h"
#include "pipeline.h"
#include "llviewerjoystick.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llparcel.h"
#include "llkeyboard.h"
#include "llerrorcontrol.h"
#include "llappviewer.h"
#include "llvosurfacepatch.h"
#include "llvowlsky.h"
#include "llrender.h"
#include "llnavigationbar.h"
#include "llfloatertools.h"
#include "llpaneloutfitsinventory.h"
// <FS:Ansariel> [FS Login Panel]
//#include "llpanellogin.h"
#include "fspanellogin.h"
// </FS:Ansariel> [FS Login Panel]
#include "llpaneltopinfobar.h"
#include "llspellcheck.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llupdaterservice.h"
// [RLVa:KB] - Checked: 2015-12-27 (RLVa-1.5.0)
#include "rlvcommon.h"
// [/RLVa:KB]

// Firestorm inclues
#include "fsfloatercontacts.h"
#include "fsfloaterim.h"
#include "fsfloaternearbychat.h"
#include "fsfloaterposestand.h"
#include "fsfloaterteleporthistory.h"
#include "fslslbridge.h"
#include "fsradar.h"
#include "llavataractions.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llhudtext.h"
#include "llnetmap.h"
#include "llnotificationsutil.h"
#include "llpanelplaces.h"
#include "llstatusbar.h"
#include "llviewerkeyboard.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "NACLantispam.h"
#include "nd/ndlogthrottle.h"

// Third party library includes
#include <boost/algorithm/string.hpp>

#ifdef TOGGLE_HACKED_GODLIKE_VIEWER
BOOL 				gHackGodmode = FALSE;
#endif

// Should you contemplate changing the name "Global", please first grep for
// that string literal. There are at least a couple other places in the C++
// code that assume the LLControlGroup named "Global" is gSavedSettings.
LLControlGroup gSavedSettings("Global");	// saved at end of session
LLControlGroup gSavedPerAccountSettings("PerAccount"); // saved at end of session
LLControlGroup gCrashSettings("CrashSettings");	// saved at end of session
LLControlGroup gWarningSettings("Warnings"); // persists ignored dialogs/warnings

std::string gLastRunVersion;

extern BOOL gResizeScreenTexture;
extern BOOL gDebugGL;

// <FS:Ansariel> FIRE-6809: Quickly moving the bandwidth slider has no effect
class BandwidthUpdater : public LLEventTimer
{
public:
	BandwidthUpdater()
		:LLEventTimer(0.5f)
	{
		mEventTimer.stop();
	}

	virtual ~BandwidthUpdater(){}

	void update(const LLSD& new_value)
	{
		mNewValue = new_value.asReal();
		mEventTimer.start();
	}

protected:
	BOOL tick()
	{
		gViewerThrottle.setMaxBandwidth(mNewValue);
		mEventTimer.stop();
	
		static LLCachedControl<bool> alreadyComplainedAboutBW(gWarningSettings, "FSBandwidthTooHigh");
		if (!alreadyComplainedAboutBW && mNewValue > 1500.f)
		{
			LLNotificationsUtil::add("FSBWTooHigh");
			gWarningSettings.setBOOL("FSBandwidthTooHigh", TRUE);
		}

		return FALSE;
	}

private:
	F32 mNewValue;
};
BandwidthUpdater sBandwidthUpdater;
// </FS:Ansariel>

////////////////////////////////////////////////////////////////////////////
// Listeners

static bool handleRenderAvatarMouselookChanged(const LLSD& newvalue)
{
	LLVOAvatar::sVisibleInFirstPerson = newvalue.asBoolean();
	return true;
}

static bool handleRenderFarClipChanged(const LLSD& newvalue)
{
	F32 draw_distance = (F32) newvalue.asReal();
	gAgentCamera.mDrawDistance = draw_distance;
	LLWorld::getInstance()->setLandFarClip(draw_distance);
	return true;
}

static bool handleTerrainDetailChanged(const LLSD& newvalue)
{
	LLDrawPoolTerrain::sDetailMode = newvalue.asInteger();
	return true;
}


// <FS:Ansariel> Expose handleSetShaderChanged()
//static bool handleSetShaderChanged(const LLSD& newvalue)
bool handleSetShaderChanged(const LLSD& newvalue)
// </FS:Ansariel>
{
	// changing shader level may invalidate existing cached bump maps, as the shader type determines the format of the bump map it expects - clear and repopulate the bump cache
	gBumpImageList.destroyGL();
	gBumpImageList.restoreGL();

	// else, leave terrain detail as is
	LLViewerShaderMgr::instance()->setShaders();
	return true;
}

static bool handleRenderPerfTestChanged(const LLSD& newvalue)
{
       bool status = !newvalue.asBoolean();
       if (!status)
       {
               gPipeline.clearRenderTypeMask(LLPipeline::RENDER_TYPE_WL_SKY,
                                                                         LLPipeline::RENDER_TYPE_GROUND,
                                                                        LLPipeline::RENDER_TYPE_TERRAIN,
                                                                         LLPipeline::RENDER_TYPE_GRASS,
                                                                         LLPipeline::RENDER_TYPE_TREE,
                                                                         LLPipeline::RENDER_TYPE_WATER,
                                                                         LLPipeline::RENDER_TYPE_PASS_GRASS,
                                                                         LLPipeline::RENDER_TYPE_HUD,
                                                                         LLPipeline::RENDER_TYPE_CLOUDS,
                                                                         LLPipeline::RENDER_TYPE_HUD_PARTICLES,
                                                                         LLPipeline::END_RENDER_TYPES); 
               gPipeline.setRenderDebugFeatureControl(LLPipeline::RENDER_DEBUG_FEATURE_UI, false);
       }
       else 
       {
               gPipeline.setRenderTypeMask(LLPipeline::RENDER_TYPE_WL_SKY,
                                                                         LLPipeline::RENDER_TYPE_GROUND,
                                                                         LLPipeline::RENDER_TYPE_TERRAIN,
                                                                         LLPipeline::RENDER_TYPE_GRASS,
                                                                         LLPipeline::RENDER_TYPE_TREE,
                                                                         LLPipeline::RENDER_TYPE_WATER,
                                                                         LLPipeline::RENDER_TYPE_PASS_GRASS,
                                                                         LLPipeline::RENDER_TYPE_HUD,
                                                                         LLPipeline::RENDER_TYPE_CLOUDS,
                                                                         LLPipeline::RENDER_TYPE_HUD_PARTICLES,
                                                                         LLPipeline::END_RENDER_TYPES);
               gPipeline.setRenderDebugFeatureControl(LLPipeline::RENDER_DEBUG_FEATURE_UI, true);
       }

       return true;
}

bool handleRenderTransparentWaterChanged(const LLSD& newvalue)
{
	LLWorld::getInstance()->updateWaterObjects();
	return true;
}

static bool handleReleaseGLBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
	}
	return true;
}

static bool handleLUTBufferChanged(const LLSD& newvalue)
{
	if (gPipeline.isInit())
	{
		gPipeline.releaseLUTBuffers();
		gPipeline.createLUTBuffers();
	}
	return true;
}

static bool handleAnisotropicChanged(const LLSD& newvalue)
{
	LLImageGL::sGlobalUseAnisotropic = newvalue.asBoolean();
	LLImageGL::dirtyTexOptions();
	return true;
}

static bool handleVolumeLODChanged(const LLSD& newvalue)
{
	LLVOVolume::sLODFactor = (F32) newvalue.asReal();
	LLVOVolume::sDistanceFactor = 1.f-LLVOVolume::sLODFactor * 0.1f;
	return true;
}

static bool handleRiggedLODChanged(const LLSD& newvalue)
{
	LLVOVolume::sRiggedLODCompensation = llmax(0.25f, (F32)newvalue.asReal());
    return true;
}

static bool handleAvatarLODChanged(const LLSD& newvalue)
{
	LLVOAvatar::sLODFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleAvatarPhysicsLODChanged(const LLSD& newvalue)
{
	LLVOAvatar::sPhysicsLODFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleTerrainLODChanged(const LLSD& newvalue)
{
		LLVOSurfacePatch::sLODFactor = (F32)newvalue.asReal();
		//sqaure lod factor to get exponential range of [0,4] and keep
		//a value of 1 in the middle of the detail slider for consistency
		//with other detail sliders (see panel_preferences_graphics1.xml)
		LLVOSurfacePatch::sLODFactor *= LLVOSurfacePatch::sLODFactor;
		return true;
}

static bool handleTreeLODChanged(const LLSD& newvalue)
{
	LLVOTree::sTreeFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleFlexLODChanged(const LLSD& newvalue)
{
	LLVolumeImplFlexible::sUpdateFactor = (F32) newvalue.asReal();
	return true;
}

static bool handleGammaChanged(const LLSD& newvalue)
{
	F32 gamma = (F32) newvalue.asReal();
	if (gamma == 0.0f)
	{
		gamma = 1.0f; // restore normal gamma
	}
	if (gViewerWindow && gViewerWindow->getWindow() && gamma != gViewerWindow->getWindow()->getGamma())
	{
		// Only save it if it's changed
		if (!gViewerWindow->getWindow()->setGamma(gamma))
		{
			LL_WARNS() << "setGamma failed!" << LL_ENDL;
		}
	}

	return true;
}

const F32 MAX_USER_FOG_RATIO = 10.f;
const F32 MIN_USER_FOG_RATIO = 0.5f;

static bool handleFogRatioChanged(const LLSD& newvalue)
{
	F32 fog_ratio = llmax(MIN_USER_FOG_RATIO, llmin((F32) newvalue.asReal(), MAX_USER_FOG_RATIO));
	gSky.setFogRatio(fog_ratio);
	return true;
}

static bool handleMaxPartCountChanged(const LLSD& newvalue)
{
	LLViewerPartSim::setMaxPartCount(newvalue.asInteger());
	return true;
}

static bool handleVideoMemoryChanged(const LLSD& newvalue)
{
	gTextureList.updateMaxResidentTexMem(S32Megabytes(newvalue.asInteger()));
	return true;
}

static bool handleChatFontSizeChanged(const LLSD& newvalue)
{
	if(gConsole)
	{
		gConsole->setFontSize(newvalue.asInteger());
	}
	return true;
}

static bool handleChatPersistTimeChanged(const LLSD& newvalue)
{
	if(gConsole)
	{
		// <FS> Changed for FIRE-805
		//gConsole->setLinePersistTime((F32) newvalue.asReal());
		gConsole->setLinePersistTime((F32) newvalue.asInteger());
	}
	return true;
}

static bool handleConsoleMaxLinesChanged(const LLSD& newvalue)
{
	if(gConsole)
	{
		gConsole->setMaxLines(newvalue.asInteger());
	}
	return true;
}

static void handleAudioVolumeChanged(const LLSD& newvalue)
{
	audio_update_volume(true);
}

static bool handleJoystickChanged(const LLSD& newvalue)
{
	LLViewerJoystick::getInstance()->setCameraNeedsUpdate(TRUE);
	return true;
}

static bool handleUseOcclusionChanged(const LLSD& newvalue)
{
	LLPipeline::sUseOcclusion = (newvalue.asBoolean() && gGLManager.mHasOcclusionQuery && LLGLSLShader::sNoFixedFunction
		&& LLFeatureManager::getInstance()->isFeatureAvailable("UseOcclusion") && !gUseWireframe) ? 2 : 0;
	return true;
}

static bool handleUploadBakedTexOldChanged(const LLSD& newvalue)
{
	LLPipeline::sForceOldBakedUpload = newvalue.asBoolean();
	return true;
}


static bool handleWLSkyDetailChanged(const LLSD&)
{
	if (gSky.mVOWLSkyp.notNull())
	{
		gSky.mVOWLSkyp->updateGeometry(gSky.mVOWLSkyp->mDrawable);
	}
	return true;
}

static bool handleResetVertexBuffersChanged(const LLSD&)
{
	if (gPipeline.isInit())
	{
		gPipeline.resetVertexBuffers();
	}
	return true;
}

static bool handleRepartition(const LLSD&)
{
	if (gPipeline.isInit())
	{
		gOctreeMaxCapacity = gSavedSettings.getU32("OctreeMaxNodeCapacity");
		gOctreeMinSize = gSavedSettings.getF32("OctreeMinimumNodeSize");
		gObjectList.repartitionObjects();
	}
	return true;
}

static bool handleRenderDynamicLODChanged(const LLSD& newvalue)
{
	LLPipeline::sDynamicLOD = newvalue.asBoolean();
	return true;
}

static bool handleRenderLocalLightsChanged(const LLSD& newvalue)
{
	gPipeline.setLightingDetail(-1);
	return true;
}

static bool handleRenderDeferredChanged(const LLSD& newvalue)
{
	LLRenderTarget::sUseFBO = newvalue.asBoolean();
	if (gPipeline.isInit())
	{
		LLPipeline::refreshCachedSettings();
		gPipeline.updateRenderDeferred();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		if (LLPipeline::sRenderDeferred == (BOOL)LLRenderTarget::sUseFBO)
		{
			LLViewerShaderMgr::instance()->setShaders();
		}
	}
	return true;
}

// This looks a great deal like handleRenderDeferredChanged because
// Advanced Lighting (Materials) implies bumps and shiny so disabling
// bumps should further disable that feature.
//
static bool handleRenderBumpChanged(const LLSD& newval)
{
	LLRenderTarget::sUseFBO = newval.asBoolean();
	if (gPipeline.isInit())
	{
		gPipeline.updateRenderBump();
		gPipeline.updateRenderDeferred();
		gPipeline.releaseGLBuffers();
		gPipeline.createGLBuffers();
		gPipeline.resetVertexBuffers();
		LLViewerShaderMgr::instance()->setShaders();
	}
	return true;
}

static bool handleRenderDebugGLChanged(const LLSD& newvalue)
{
	gDebugGL = newvalue.asBoolean() || gDebugSession;
	gGL.clearErrors();
	return true;
}

static bool handleRenderDebugPipelineChanged(const LLSD& newvalue)
{
	gDebugPipeline = newvalue.asBoolean();
	return true;
}

static bool handleRenderResolutionDivisorChanged(const LLSD&)
{
	gResizeScreenTexture = TRUE;
	return true;
}

static bool handleDebugViewsChanged(const LLSD& newvalue)
{
	LLView::sDebugRects = newvalue.asBoolean();
	return true;
}

static bool handleLogFileChanged(const LLSD& newvalue)
{
	std::string log_filename = newvalue.asString();
	LLFile::remove(log_filename);
	LLError::logToFile(log_filename);
	return true;
}

bool handleHideGroupTitleChanged(const LLSD& newvalue)
{
	gAgent.setHideGroupTitle(newvalue);
	return true;
}

bool handleEffectColorChanged(const LLSD& newvalue)
{
	gAgent.setEffectColor(LLColor4(newvalue));
	return true;
}

bool handleHighResSnapshotChanged(const LLSD& newvalue)
{
	// High Res Snapshot active, must uncheck RenderUIInSnapshot
	if (newvalue.asBoolean())
	{
		gSavedSettings.setBOOL( "RenderUIInSnapshot", FALSE );
	}
	return true;
}

bool handleVoiceClientPrefsChanged(const LLSD& newvalue)
{
	LLVoiceClient::getInstance()->updateSettings();
	return true;
}

// NaCl - Antispam Registry
bool handleNaclAntiSpamGlobalQueueChanged(const LLSD& newvalue)
{
	NACLAntiSpamRegistry::instance().setGlobalQueue(newvalue.asBoolean());
	return true;
}
bool handleNaclAntiSpamTimeChanged(const LLSD& newvalue)
{
	NACLAntiSpamRegistry::instance().setAllQueueTimes(newvalue.asInteger());
	return true;
}
bool handleNaclAntiSpamAmountChanged(const LLSD& newvalue)
{
	NACLAntiSpamRegistry::instance().setAllQueueAmounts(newvalue.asInteger());
	return true;
}
// NaCl End 

bool handleVelocityInterpolate(const LLSD& newvalue)
{
	LLMessageSystem* msg = gMessageSystem;
	if ( newvalue.asBoolean() )
	{
		msg->newMessageFast(_PREHASH_VelocityInterpolateOn);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();
		LL_INFOS() << "Velocity Interpolation On" << LL_ENDL;
	}
	else
	{
		msg->newMessageFast(_PREHASH_VelocityInterpolateOff);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
		msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
		gAgent.sendReliableMessage();
		LL_INFOS() << "Velocity Interpolation Off" << LL_ENDL;
	}
	return true;
}

// <FS:Zi> Moved Avatar Z offset from RLVa to here
bool handleAvatarZOffsetChanged(const LLSD& sdValue)
{
	if (isAgentAvatarValid())
	{
		if (gAgent.getRegion()->avatarHoverHeightEnabled())
		{
			LLVector3 avOffset(0.0f, 0.0f, llclamp<F32>(sdValue.asReal(), MIN_HOVER_Z, MAX_HOVER_Z));
			gAgentAvatarp->setHoverOffset(avOffset, true);
		}
		else if (!gAgentAvatarp->isUsingServerBakes())
		{
			gAgentAvatarp->computeBodySize();
		}
	}
	return true;
}
// </FS:Zi> Moved Avatar Z offset from RLVa to here

bool handleForceShowGrid(const LLSD& newvalue)
{
	// <FS:Ansariel> [FS Login Panel]
	//LLPanelLogin::updateServer( );
	FSPanelLogin::updateServer( );
	// </FS:Ansariel> [FS Login Panel]
	return true;
}

bool handleLoginLocationChanged()
{
	/*
	 * This connects the default preference setting to the state of the login
	 * panel if it is displayed; if you open the preferences panel before
	 * logging in, and change the default login location there, the login
	 * panel immediately changes to match your new preference.
	 */
	std::string new_login_location = gSavedSettings.getString("LoginLocation");
	LL_DEBUGS("AppInit")<<new_login_location<<LL_ENDL;
	LLStartUp::setStartSLURL(LLSLURL(new_login_location));
	return true;
}

bool handleSpellCheckChanged()
{
	if (gSavedSettings.getBOOL("SpellCheck"))
	{
		std::list<std::string> dict_list;
		std::string dict_setting = gSavedSettings.getString("SpellCheckDictionary");
		boost::split(dict_list, dict_setting, boost::is_any_of(std::string(",")));
		if (!dict_list.empty())
		{
			LLSpellChecker::setUseSpellCheck(dict_list.front());
			dict_list.pop_front();
			LLSpellChecker::instance().setSecondaryDictionaries(dict_list);
			return true;
		}
	}
	LLSpellChecker::setUseSpellCheck(LLStringUtil::null);
	return true;
}

bool toggle_agent_pause(const LLSD& newvalue)
{
	if ( newvalue.asBoolean() )
	{
		send_agent_pause();
	}
	else
	{
		send_agent_resume();
	}
	return true;
}

// <FS:Zi> Is done inside XUI now, using visibility_control
// bool toggle_show_navigation_panel(const LLSD& newvalue)
// {
	//bool value = newvalue.asBoolean();

	//LLNavigationBar::getInstance()->setVisible(value);
	//gSavedSettings.setBOOL("ShowMiniLocationPanel", !value);

	//return true;
// }

// <FS:Zi> We don't have the mini location bar
// bool toggle_show_mini_location_panel(const LLSD& newvalue)
// {
	//bool value = newvalue.asBoolean();

	//LLPanelTopInfoBar::getInstance()->setVisible(value);
	//gSavedSettings.setBOOL("ShowNavbarNavigationPanel", !value);

	//return true;
// </FS:Zi>

bool toggle_show_menubar_location_panel(const LLSD& newvalue)
{
	bool value = newvalue.asBoolean();

	if (gStatusBar)
		gStatusBar->childSetVisible("parcel_info_panel",value);

	return true;
}

bool toggle_show_object_render_cost(const LLSD& newvalue)
{
	LLFloaterTools::sShowObjectCost = newvalue.asBoolean();
	return true;
}

void toggle_updater_service_active(const LLSD& new_value)
{
    if(new_value.asInteger())
    {
		LLUpdaterService update_service;
		if(!update_service.isChecking()) update_service.startChecking();
    }
    else
    {
        LLUpdaterService().stopChecking();
    }
}

// <FS:Ansariel> Change visibility of main chatbar if autohide setting is changed
static void handleAutohideChatbarChanged(const LLSD& new_value)
{
	// Flip MainChatbarVisible when chatbar autohide setting changes. This
	// will trigger LLNearbyChat::showDefaultChatBar() being called. Since we
	// don't want to loose focus of the preferences floater when changing the
	// autohide setting, we have to use the workaround via gFloaterView.
	LLFloater* focus = gFloaterView->getFocusedFloater();
	gSavedSettings.setBOOL("MainChatbarVisible", !new_value.asBoolean());
	if (focus)
	{
		focus->setFocus(TRUE);
	}
}
// </FS:Ansariel>

// <FS:Ansariel> Synchronize tooltips throughout instances
static void handleNetMapDoubleClickActionChanged()
{
	LLNetMap::updateToolTipMsg();
}
// </FS:Ansariel> Synchronize tooltips throughout instances

// <FS:Ansariel> Clear places / teleport history search filter
static void handleUseStandaloneTeleportHistoryFloaterChanged()
{
	LLFloaterSidePanelContainer* places = LLFloaterReg::findTypedInstance<LLFloaterSidePanelContainer>("places");
	if (places)
	{
		places->findChild<LLPanelPlaces>("main_panel")->resetFilter();
	}
	FSFloaterTeleportHistory* tphistory = LLFloaterReg::findTypedInstance<FSFloaterTeleportHistory>("fs_teleporthistory");
	if (tphistory)
	{
		tphistory->resetFilter();
	}
}
// </FS:Ansariel> Clear places / teleport history search filter

// <FS:CR> Posestand Ground Lock
static void handleSetPoseStandLock(const LLSD& newvalue)
{
	FSFloaterPoseStand* pose_stand = LLFloaterReg::findTypedInstance<FSFloaterPoseStand>("fs_posestand");
	if (pose_stand)
	{
		pose_stand->setLock(newvalue);
		pose_stand->onCommitCombo();
	}
		
}
// </FS:CR> Posestand Ground Lock

// <FS:TT> Client LSL Bridge
static void handleFlightAssistOptionChanged(const LLSD& newvalue)
{
	FSLSLBridge::instance().viewerToLSL("UseLSLFlightAssist|" + newvalue.asString());
}
// </FS:TT>

// <FS:PP> Movelock for Bridge
static void handleMovelockOptionChanged(const LLSD& newvalue)
{
	FSLSLBridge::instance().updateBoolSettingValue("UseMoveLock", newvalue.asBoolean());
}
static void handleMovelockAfterMoveOptionChanged(const LLSD& newvalue)
{
	FSLSLBridge::instance().updateBoolSettingValue("RelockMoveLockAfterMovement", newvalue.asBoolean());
}
// </FS:PP>

// <FS:PP> External integrations (OC, LM etc.) for Bridge
static void handleExternalIntegrationsOptionChanged()
{
	FSLSLBridge::instance().updateIntegrations();
}
// </FS:PP>

static void handleDecimalPrecisionChanged(const LLSD& newvalue)
{
	LLFloaterTools* build_tools = LLFloaterReg::findTypedInstance<LLFloaterTools>("build");
	if (build_tools)
	{
		build_tools->changePrecision(newvalue);
	}
}

// <FS:CR> FIRE-6659: Legacy "Resident" name toggle
void handleLegacyTrimOptionChanged(const LLSD& newvalue)
{
	LLAvatarName::setTrimResidentSurname(newvalue.asBoolean());
	LLAvatarNameCache::clearCache();
	LLVOAvatar::invalidateNameTags();
	FSFloaterContacts::getInstance()->onDisplayNameChanged();
	FSRadar::getInstance()->updateNames();
}

void handleUsernameFormatOptionChanged(const LLSD& newvalue)
{
	LLAvatarName::setUseLegacyFormat(newvalue.asBoolean());
	LLAvatarNameCache::clearCache();
	LLVOAvatar::invalidateNameTags();
	FSFloaterContacts::getInstance()->onDisplayNameChanged();
	FSRadar::getInstance()->updateNames();
}
// </FS:CR>

// <FS:Ansariel> Allow instant change of keyboard layout
void handleKeyboardLayoutChanged(const LLSD& newvalue)
{
	std::string keyBindingFileName("keys.xml");
	if (newvalue.asBoolean())
	{
		keyBindingFileName = "keys_azerty.xml";
	}

	std::string key_bindings_file = gDirUtilp->findFile(keyBindingFileName,
														gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, ""),
														gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, ""));

	gViewerKeyboard.loadBindingsXML(key_bindings_file);
}
// </FS:Ansariel>

// <FS:Ansariel> Global online status toggle
void handleGlobalOnlineStatusChanged(const LLSD& newvalue)
{
	bool visible = newvalue.asBoolean();

	LLAvatarTracker::buddy_map_t all_buddies;
	LLAvatarTracker::instance().copyBuddyList(all_buddies);

	LLAvatarTracker::buddy_map_t::const_iterator buddy_it = all_buddies.begin();
	for (; buddy_it != all_buddies.end(); ++buddy_it)
	{
		LLUUID buddy_id = buddy_it->first;
		const LLRelationship* relation = LLAvatarTracker::instance().getBuddyInfo(buddy_id);
		if (relation == NULL)
		{
			// Lets have a warning log message instead of having a crash. EXT-4947.
			LL_WARNS() << "Trying to modify rights for non-friend avatar. Skipped." << LL_ENDL;
			return;
		}
		
		S32 cur_rights = relation->getRightsGrantedTo();
		S32 new_rights = 0;
		if (visible)
		{
			new_rights = LLRelationship::GRANT_ONLINE_STATUS + (cur_rights & LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
		}
		else
		{
			new_rights = (cur_rights & LLRelationship::GRANT_MAP_LOCATION) + (cur_rights & LLRelationship::GRANT_MODIFY_OBJECTS);
		}

		LLAvatarPropertiesProcessor::getInstance()->sendFriendRights(buddy_id, new_rights);
	}

	LLNotificationsUtil::add("GlobalOnlineStatusToggle");
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-14083: Search filter for contact list
void handleContactListShowSearchChanged(const LLSD& newvalue)
{
	bool visible = newvalue.asBoolean();
	if (!visible)
	{
		FSFloaterContacts* instance = FSFloaterContacts::findInstance();
		if (instance)
		{
			instance->resetFriendFilter();
		}
	}
}
// </FS:Ansariel>

// <FS:Ansariel> Debug setting to disable log throttle
void handleLogThrottleChanged(const LLSD& newvalue)
{
	nd::logging::setThrottleEnabled(newvalue.asBoolean());
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-18250: Option to disable default eye movement
void handleStaticEyesChanged()
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LLUUID anim_id(gSavedSettings.getString("FSStaticEyesUUID"));
	if (gSavedPerAccountSettings.getBOOL("FSStaticEyes"))
	{
		gAgentAvatarp->startMotion(anim_id);
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_START);
	}
	else
	{
		gAgentAvatarp->stopMotion(anim_id);
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-20288: Option to render friends only
void handleRenderFriendsOnlyChanged(const LLSD& newvalue)
{
	if (newvalue.asBoolean())
	{
		for (std::vector<LLCharacter*>::iterator iter = LLCharacter::sInstances.begin();
			iter != LLCharacter::sInstances.end(); ++iter)
		{
			LLVOAvatar* avatar = (LLVOAvatar*)*iter;

			if (avatar->getID() != gAgentID && !LLAvatarActions::isFriend(avatar->getID()))
			{
				gObjectList.killObject(avatar);
				if (LLViewerRegion::sVOCacheCullingEnabled && avatar->getRegion())
				{
					avatar->getRegion()->killCacheEntry(avatar->getLocalID());
				}
			}
		}
	}
}
// </FS:Ansariel>

////////////////////////////////////////////////////////////////////////////

void settings_setup_listeners()
{
	gSavedSettings.getControl("FirstPersonAvatarVisible")->getSignal()->connect(boost::bind(&handleRenderAvatarMouselookChanged, _2));
	gSavedSettings.getControl("RenderFarClip")->getSignal()->connect(boost::bind(&handleRenderFarClipChanged, _2));
	gSavedSettings.getControl("RenderTerrainDetail")->getSignal()->connect(boost::bind(&handleTerrainDetailChanged, _2));
	gSavedSettings.getControl("OctreeStaticObjectSizeFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeDistanceFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeMaxNodeCapacity")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeAlphaDistanceFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("OctreeAttachmentSizeFactor")->getSignal()->connect(boost::bind(&handleRepartition, _2));
	gSavedSettings.getControl("RenderMaxTextureIndex")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderUseTriStrips")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAvatarVP")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("VertexShaderEnable")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderUIBuffer")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderDepthOfField")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderFSAASamples")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderSpecularResX")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderSpecularResY")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderSpecularExponent")->getSignal()->connect(boost::bind(&handleLUTBufferChanged, _2));
	gSavedSettings.getControl("RenderAnisotropic")->getSignal()->connect(boost::bind(&handleAnisotropicChanged, _2));
	gSavedSettings.getControl("RenderShadowResolutionScale")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderGlow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderGlow")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderGlowResolutionPow")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderAvatarCloth")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderGammaFull")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderVolumeLODFactor")->getSignal()->connect(boost::bind(&handleVolumeLODChanged, _2));
	gSavedSettings.getControl("RenderRiggedLodCompensation")->getSignal()->connect(boost::bind(&handleRiggedLODChanged, _2));
	gSavedSettings.getControl("RenderAvatarLODFactor")->getSignal()->connect(boost::bind(&handleAvatarLODChanged, _2));
	gSavedSettings.getControl("RenderAvatarPhysicsLODFactor")->getSignal()->connect(boost::bind(&handleAvatarPhysicsLODChanged, _2));
	gSavedSettings.getControl("RenderTerrainLODFactor")->getSignal()->connect(boost::bind(&handleTerrainLODChanged, _2));
	gSavedSettings.getControl("RenderTreeLODFactor")->getSignal()->connect(boost::bind(&handleTreeLODChanged, _2));
	gSavedSettings.getControl("RenderFlexTimeFactor")->getSignal()->connect(boost::bind(&handleFlexLODChanged, _2));
	gSavedSettings.getControl("RenderGamma")->getSignal()->connect(boost::bind(&handleGammaChanged, _2));
	gSavedSettings.getControl("RenderFogRatio")->getSignal()->connect(boost::bind(&handleFogRatioChanged, _2));
	gSavedSettings.getControl("RenderMaxPartCount")->getSignal()->connect(boost::bind(&handleMaxPartCountChanged, _2));
	gSavedSettings.getControl("RenderDynamicLOD")->getSignal()->connect(boost::bind(&handleRenderDynamicLODChanged, _2));
	gSavedSettings.getControl("RenderLocalLights")->getSignal()->connect(boost::bind(&handleRenderLocalLightsChanged, _2));
	gSavedSettings.getControl("RenderDebugTextureBind")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAutoMaskAlphaDeferred")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderAutoMaskAlphaNonDeferred")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderObjectBump")->getSignal()->connect(boost::bind(&handleRenderBumpChanged, _2));
	gSavedSettings.getControl("RenderMaxVBOSize")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderDeferredNoise")->getSignal()->connect(boost::bind(&handleReleaseGLBufferChanged, _2));
	gSavedSettings.getControl("RenderDebugGL")->getSignal()->connect(boost::bind(&handleRenderDebugGLChanged, _2));
	gSavedSettings.getControl("RenderDebugPipeline")->getSignal()->connect(boost::bind(&handleRenderDebugPipelineChanged, _2));
	gSavedSettings.getControl("RenderResolutionDivisor")->getSignal()->connect(boost::bind(&handleRenderResolutionDivisorChanged, _2));
	gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&handleRenderDeferredChanged, _2));
	gSavedSettings.getControl("RenderShadowDetail")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderDeferredSSAO")->getSignal()->connect(boost::bind(&handleSetShaderChanged, _2));
	gSavedSettings.getControl("RenderPerformanceTest")->getSignal()->connect(boost::bind(&handleRenderPerfTestChanged, _2));
	gSavedSettings.getControl("TextureMemory")->getSignal()->connect(boost::bind(&handleVideoMemoryChanged, _2));
	gSavedSettings.getControl("ChatConsoleFontSize")->getSignal()->connect(boost::bind(&handleChatFontSizeChanged, _2));
	gSavedSettings.getControl("NearbyToastLifeTime")->getSignal()->connect(boost::bind(&handleChatPersistTimeChanged, _2));
	gSavedSettings.getControl("ConsoleMaxLines")->getSignal()->connect(boost::bind(&handleConsoleMaxLinesChanged, _2));
	gSavedSettings.getControl("UploadBakedTexOld")->getSignal()->connect(boost::bind(&handleUploadBakedTexOldChanged, _2));
	gSavedSettings.getControl("UseOcclusion")->getSignal()->connect(boost::bind(&handleUseOcclusionChanged, _2));
	gSavedSettings.getControl("AudioLevelMaster")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelSFX")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelDoppler")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelRolloff")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("AudioLevelUnderwaterRolloff")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteAudio")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteMusic")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteMedia")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteVoice")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteAmbient")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("MuteUI")->getSignal()->connect(boost::bind(&handleAudioVolumeChanged, _2));
	gSavedSettings.getControl("RenderVBOEnable")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderUseVAO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderVBOMappingDisable")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderUseStreamVBO")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("RenderPreferStreamDraw")->getSignal()->connect(boost::bind(&handleResetVertexBuffersChanged, _2));
	gSavedSettings.getControl("WLSkyDetail")->getSignal()->connect(boost::bind(&handleWLSkyDetailChanged, _2));
	gSavedSettings.getControl("JoystickAxis0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("JoystickAxis6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisScale6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("FlycamAxisDeadZone6")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("AvatarAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisScale5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone0")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone1")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone2")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone3")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone4")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("BuildAxisDeadZone5")->getSignal()->connect(boost::bind(&handleJoystickChanged, _2));
	gSavedSettings.getControl("DebugViews")->getSignal()->connect(boost::bind(&handleDebugViewsChanged, _2));
	gSavedSettings.getControl("UserLogFile")->getSignal()->connect(boost::bind(&handleLogFileChanged, _2));
	gSavedSettings.getControl("RenderHideGroupTitle")->getSignal()->connect(boost::bind(handleHideGroupTitleChanged, _2));
	gSavedSettings.getControl("HighResSnapshot")->getSignal()->connect(boost::bind(handleHighResSnapshotChanged, _2));
	gSavedSettings.getControl("EnableVoiceChat")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PTTCurrentlyEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PushToTalkButton")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("PushToTalkToggle")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceEarLocation")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceInputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("VoiceOutputAudioDevice")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("AudioLevelMic")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncEnabled")->getSignal()->connect(boost::bind(&handleVoiceClientPrefsChanged, _2));	
	gSavedSettings.getControl("VelocityInterpolate")->getSignal()->connect(boost::bind(&handleVelocityInterpolate, _2));
	gSavedSettings.getControl("QAMode")->getSignal()->connect(boost::bind(&show_debug_menus));
	gSavedSettings.getControl("UseDebugMenus")->getSignal()->connect(boost::bind(&show_debug_menus));
	gSavedSettings.getControl("AgentPause")->getSignal()->connect(boost::bind(&toggle_agent_pause, _2));
	// <FS:Zi> Is done inside XUI now, using visibility_control
	// gSavedSettings.getControl("ShowNavbarNavigationPanel")->getSignal()->connect(boost::bind(&toggle_show_navigation_panel, _2));
	// </FS:Zi>
	// <FS:Zi> We don't have the mini location bar
	// gSavedSettings.getControl("ShowMiniLocationPanel")->getSignal()->connect(boost::bind(&toggle_show_mini_location_panel, _2));
	// </FS: Zi>
	gSavedSettings.getControl("ShowMenuBarLocation")->getSignal()->connect(boost::bind(&toggle_show_menubar_location_panel, _2));
	gSavedSettings.getControl("ShowObjectRenderingCost")->getSignal()->connect(boost::bind(&toggle_show_object_render_cost, _2));
	gSavedSettings.getControl("UpdaterServiceSetting")->getSignal()->connect(boost::bind(&toggle_updater_service_active, _2));
	gSavedSettings.getControl("ForceShowGrid")->getSignal()->connect(boost::bind(&handleForceShowGrid, _2));
	gSavedSettings.getControl("RenderTransparentWater")->getSignal()->connect(boost::bind(&handleRenderTransparentWaterChanged, _2));
	gSavedSettings.getControl("SpellCheck")->getSignal()->connect(boost::bind(&handleSpellCheckChanged));
	gSavedSettings.getControl("SpellCheckDictionary")->getSignal()->connect(boost::bind(&handleSpellCheckChanged));
	gSavedSettings.getControl("LoginLocation")->getSignal()->connect(boost::bind(&handleLoginLocationChanged));
// [RLVa:KB] - Checked: 2015-12-27 (RLVa-1.5.0)
	gSavedSettings.getControl("RestrainedLove")->getSignal()->connect(boost::bind(&RlvSettings::onChangedSettingMain, _2));
// [/RLVa:KB]
	gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getSignal()->connect(boost::bind(&handleAvatarZOffsetChanged, _2)); // <FS:Zi> Moved Avatar Z offset from RLVa to here
	// <FS:Zi> Is done inside XUI now, using visibility_control
	// gSavedSettings.getControl("ShowNavbarFavoritesPanel")->getSignal()->connect(boost::bind(&toggle_show_favorites_panel, _2));
	// </FS:Zi>
	// NaCl - Antispam Registry
	gSavedSettings.getControl("_NACL_AntiSpamGlobalQueue")->getSignal()->connect(boost::bind(&handleNaclAntiSpamGlobalQueueChanged, _2));
	gSavedSettings.getControl("_NACL_AntiSpamTime")->getSignal()->connect(boost::bind(&handleNaclAntiSpamTimeChanged, _2));
	gSavedSettings.getControl("_NACL_AntiSpamAmount")->getSignal()->connect(boost::bind(&handleNaclAntiSpamAmountChanged, _2));
	// NaCl End
	gSavedSettings.getControl("AutohideChatBar")->getSignal()->connect(boost::bind(&handleAutohideChatbarChanged, _2));

	// <FS:Ansariel> Synchronize tooltips throughout instances
	gSavedSettings.getControl("FSNetMapDoubleClickAction")->getSignal()->connect(boost::bind(&handleNetMapDoubleClickActionChanged));

	// <FS:Ansariel> Clear places / teleport history search filter
	gSavedSettings.getControl("FSUseStandaloneTeleportHistoryFloater")->getSignal()->connect(boost::bind(&handleUseStandaloneTeleportHistoryFloaterChanged));
	
	// <FS:CR> Pose stand ground lock
	gSavedSettings.getControl("FSPoseStandLock")->getSignal()->connect(boost::bind(&handleSetPoseStandLock, _2));

	gSavedPerAccountSettings.getControl("UseLSLFlightAssist")->getCommitSignal()->connect(boost::bind(&handleFlightAssistOptionChanged, _2));
	gSavedPerAccountSettings.getControl("UseMoveLock")->getCommitSignal()->connect(boost::bind(&handleMovelockOptionChanged, _2));
	gSavedPerAccountSettings.getControl("RelockMoveLockAfterMovement")->getCommitSignal()->connect(boost::bind(&handleMovelockAfterMoveOptionChanged, _2));
	gSavedSettings.getControl("FSBuildToolDecimalPrecision")->getCommitSignal()->connect(boost::bind(&handleDecimalPrecisionChanged, _2));

	// <FS:PP> External integrations (OC, LM etc.) for Bridge
	gSavedPerAccountSettings.getControl("BridgeIntegrationOC")->getCommitSignal()->connect(boost::bind(&handleExternalIntegrationsOptionChanged));
	gSavedPerAccountSettings.getControl("BridgeIntegrationLM")->getCommitSignal()->connect(boost::bind(&handleExternalIntegrationsOptionChanged));

	gSavedSettings.getControl("FSNameTagShowLegacyUsernames")->getCommitSignal()->connect(boost::bind(&handleUsernameFormatOptionChanged, _2));
	gSavedSettings.getControl("FSTrimLegacyNames")->getCommitSignal()->connect(boost::bind(&handleLegacyTrimOptionChanged, _2));

	gSavedSettings.getControl("FSUseAzertyKeyboardLayout")->getCommitSignal()->connect(boost::bind(&handleKeyboardLayoutChanged, _2));

	// <FS:Ansariel> [FS communication UI]
	gSavedSettings.getControl("PlainTextChatHistory")->getSignal()->connect(boost::bind(&FSFloaterIM::processChatHistoryStyleUpdate, _2));
	gSavedSettings.getControl("PlainTextChatHistory")->getSignal()->connect(boost::bind(&FSFloaterNearbyChat::processChatHistoryStyleUpdate, _2));
	gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&FSFloaterIM::processChatHistoryStyleUpdate, _2));
	gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&FSFloaterNearbyChat::processChatHistoryStyleUpdate, _2));
	gSavedSettings.getControl("ChatFontSize")->getSignal()->connect(boost::bind(&LLViewerChat::signalChatFontChanged));
	// </FS:Ansariel> [FS communication UI]

	gSavedPerAccountSettings.getControl("GlobalOnlineStatusToggle")->getSignal()->connect(boost::bind(&handleGlobalOnlineStatusChanged, _2));

	// <FS:Ansariel> FIRE-17393: Control HUD text fading by options
	gSavedSettings.getControl("FSHudTextFadeDistance")->getSignal()->connect(boost::bind(&LLHUDText::onFadeSettingsChanged));
	gSavedSettings.getControl("FSHudTextFadeRange")->getSignal()->connect(boost::bind(&LLHUDText::onFadeSettingsChanged));

	//<FS:HG> FIRE-6340, FIRE-6567, FIRE-6809 - Setting Bandwidth issues
	gSavedSettings.getControl("ThrottleBandwidthKBPS")->getSignal()->connect(boost::bind(&BandwidthUpdater::update, sBandwidthUpdater, _2));
	gSavedSettings.getControl("FSContactListShowSearch")->getSignal()->connect(boost::bind(&handleContactListShowSearchChanged, _2));

	// <FS:Ansariel> Debug setting to disable log throttle
	gSavedSettings.getControl("FSEnableLogThrottle")->getSignal()->connect(boost::bind(&handleLogThrottleChanged, _2));

	// <FS:Ansariel> FIRE-18250: Option to disable default eye movement
	gSavedSettings.getControl("FSStaticEyesUUID")->getSignal()->connect(boost::bind(&handleStaticEyesChanged));
	gSavedPerAccountSettings.getControl("FSStaticEyes")->getSignal()->connect(boost::bind(&handleStaticEyesChanged));
	// </FS:Ansariel>

	// <FS:Ansariel> FIRE-20288: Option to render friends only
	gSavedPerAccountSettings.getControl("FSRenderFriendsOnly")->getSignal()->connect(boost::bind(&handleRenderFriendsOnlyChanged, _2));
}

#if TEST_CACHED_CONTROL

#define DECL_LLCC(T, V) static LLCachedControl<T> mySetting_##T("TestCachedControl"#T, V)
DECL_LLCC(U32, (U32)666);
DECL_LLCC(S32, (S32)-666);
DECL_LLCC(F32, (F32)-666.666);
DECL_LLCC(bool, true);
DECL_LLCC(BOOL, FALSE);
static LLCachedControl<std::string> mySetting_string("TestCachedControlstring", "Default String Value");
DECL_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
DECL_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
DECL_LLCC(LLRect, LLRect(0, 0, 100, 500));
DECL_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
DECL_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
DECL_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));

LLSD test_llsd = LLSD()["testing1"] = LLSD()["testing2"];
DECL_LLCC(LLSD, test_llsd);

static LLCachedControl<std::string> test_BrowserHomePage("BrowserHomePage", "hahahahahha", "Not the real comment");

void test_cached_control()
{
#define do { TEST_LLCC(T, V) if((T)mySetting_##T != V) LL_ERRS() << "Fail "#T << LL_ENDL; } while(0)
	TEST_LLCC(U32, 666);
	TEST_LLCC(S32, (S32)-666);
	TEST_LLCC(F32, (F32)-666.666);
	TEST_LLCC(bool, true);
	TEST_LLCC(BOOL, FALSE);
	if((std::string)mySetting_string != "Default String Value") LL_ERRS() << "Fail string" << LL_ENDL;
	TEST_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
	TEST_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
	TEST_LLCC(LLRect, LLRect(0, 0, 100, 500));
	TEST_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
	TEST_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
	TEST_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));
//There's no LLSD comparsion for LLCC yet. TEST_LLCC(LLSD, test_llsd); 

	// AO - Phoenixviewer doesn't want to send unecessary noise to secondlife.com
	//if((std::string)test_BrowserHomePage != "http://www.secondlife.com") LL_ERRS() << "Fail BrowserHomePage" << LL_ENDL;
}
#endif // TEST_CACHED_CONTROL


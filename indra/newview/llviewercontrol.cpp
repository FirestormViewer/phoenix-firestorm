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
#include "llwindow.h"   // getGamma()

// <FS:Zi> Handle IME text input getting enabled or disabled
#if LL_SDL2
#include "llwindowsdl2.h"
#endif
// </FS:Zi>

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
#include "llvotree.h"
#include "llvovolume.h"
#include "llworld.h"
#include "llvlcomposition.h"
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
#include "llnotificationsutil.h"
#include "llfloatertools.h"
#include "llpaneloutfitsinventory.h"
// <FS:Ansariel> [FS Login Panel]
//#include "llpanellogin.h"
#include "fspanellogin.h"
// </FS:Ansariel> [FS Login Panel]
// <FS:Zi> We don't use the mini location panel in Firestorm
// #include "llpaneltopinfobar.h"
#include "llspellcheck.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llperfstats.h"
// [RLVa:KB] - Checked: 2015-12-27 (RLVa-1.5.0)
#include "llvisualeffect.h"
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]

#if LL_DARWIN
#include "llwindowmacosx.h"
#endif

// Firestorm inclues
#include "fsfloatercontacts.h"
#include "fsfloaterim.h"
#include "fsfloaternearbychat.h"
#include "fsfloaterposestand.h"
#include "fsfloaterteleporthistory.h"
#include "fslslbridge.h"
#include "fsradar.h"
#include "llavataractions.h"
#include "lldiskcache.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llhudtext.h"
#include "llnotificationsutil.h"
#include "llpanelplaces.h"
#include "llstatusbar.h"
#include "llviewerinput.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "NACLantispam.h"
#include "nd/ndlogthrottle.h"
// <FS:Zi> Run Prio 0 default bento pose in the background to fix splayed hands, open mouths, etc.
#include "llanimationstates.h"

// Third party library includes
#include <boost/algorithm/string.hpp>

#ifdef TOGGLE_HACKED_GODLIKE_VIEWER
bool                gHackGodmode = false;
#endif

// Should you contemplate changing the name "Global", please first grep for
// that string literal. There are at least a couple other places in the C++
// code that assume the LLControlGroup named "Global" is gSavedSettings.
LLControlGroup gSavedSettings("Global");    // saved at end of session
LLControlGroup gSavedPerAccountSettings("PerAccount"); // saved at end of session
LLControlGroup gCrashSettings("CrashSettings"); // saved at end of session
LLControlGroup gWarningSettings("Warnings"); // persists ignored dialogs/warnings

std::string gLastRunVersion;

extern bool gResizeScreenTexture;
extern bool gResizeShadowTexture;
extern bool gDebugGL;

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
        mNewValue = (F32)new_value.asReal();
        mEventTimer.start();
    }

protected:
    bool tick()
    {
        gViewerThrottle.setMaxBandwidth(mNewValue);
        mEventTimer.stop();

        static LLCachedControl<bool> alreadyComplainedAboutBW(gWarningSettings, "FSBandwidthTooHigh");
        if (!alreadyComplainedAboutBW && mNewValue > 1500.f)
        {
            LLNotificationsUtil::add("FSBWTooHigh");
            gWarningSettings.setBOOL("FSBandwidthTooHigh", true);
        }

        return false;
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
    if (LLStartUp::getStartupState() >= STATE_STARTED)
    {
        F32 draw_distance = (F32)newvalue.asReal();
    gAgentCamera.mDrawDistance = draw_distance;
    LLWorld::getInstance()->setLandFarClip(draw_distance);
    return true;
    }
    return false;
}

static bool handleTerrainScaleChanged(const LLSD& newvalue)
{
    F64 scale = newvalue.asReal();
    if (scale != 0.0)
    {
        LLDrawPoolTerrain::sDetailScale = F32(1.0 / scale);
    }
    return true;
}

static bool handlePBRTerrainScaleChanged(const LLSD& newvalue)
{
    F64 scale = newvalue.asReal();
    if (scale != 0.0)
    {
        LLDrawPoolTerrain::sPBRDetailScale = F32(1.0 / scale);
    }
    return true;
}

static bool handleDebugAvatarJointsChanged(const LLSD& newvalue)
{
    std::string new_string = newvalue.asString();
    LLJoint::setDebugJointNames(new_string);
    return true;
}

static bool handleAvatarHoverOffsetChanged(const LLSD& newvalue)
{
    if (isAgentAvatarValid())
    {
        gAgentAvatarp->setHoverIfRegionEnabled();
    }
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

    if (gPipeline.isInit())
    {
        // ALM depends onto atmospheric shaders, state might have changed
        LLPipeline::refreshCachedSettings();
    }

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
    if (gPipeline.isInit())
    {
        gPipeline.updateRenderTransparentWater();
        gPipeline.releaseGLBuffers();
        gPipeline.createGLBuffers();
        LLViewerShaderMgr::instance()->setShaders();
    }
    LLWorld::getInstance()->updateWaterObjects();
    return true;
}


static bool handleShadowsResized(const LLSD& newvalue)
{
    gPipeline.requestResizeShadowTexture();
    return true;
}

static bool handleWindowResized(const LLSD& newvalue)
{
    gPipeline.requestResizeScreenTexture();
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

static bool handleEnableEmissiveChanged(const LLSD& newvalue)
{
    return handleReleaseGLBufferChanged(newvalue) && handleSetShaderChanged(newvalue);
}

static bool handleDisableVintageMode(const LLSD& newvalue)
{
    gSavedSettings.setBOOL("RenderEnableEmissiveBuffer", newvalue.asBoolean());
    gSavedSettings.setBOOL("RenderHDREnabled", newvalue.asBoolean());
    return true;
}

static bool handleEnableHDR(const LLSD& newvalue)
{
    gPipeline.mReflectionMapManager.reset();
    gPipeline.mHeroProbeManager.reset();
    return handleReleaseGLBufferChanged(newvalue) && handleSetShaderChanged(newvalue);
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

static bool handleVSyncChanged(const LLSD& newvalue)
{
    LLPerfStats::tunables.vsyncEnabled = newvalue.asBoolean();
    if (gViewerWindow && gViewerWindow->getWindow())
    {
        gViewerWindow->getWindow()->toggleVSync(newvalue.asBoolean());

        if (newvalue.asBoolean())
        {
            U32 current_target = gSavedSettings.getU32("TargetFPS");
            gSavedSettings.setU32("TargetFPS", std::min((U32)gViewerWindow->getWindow()->getRefreshRate(), current_target));
        }
    }

    return true;
}

static bool handleVolumeLODChanged(const LLSD& newvalue)
{
    LLVOVolume::sLODFactor = llclamp((F32) newvalue.asReal(), 0.01f, MAX_LOD_FACTOR);
    LLVOVolume::sDistanceFactor = 1.f-LLVOVolume::sLODFactor * 0.1f;

    // <FS:PP> Warning about too high LOD on LOD change
    if (LLVOVolume::sLODFactor > 4.0f)
    {
        LLNotificationsUtil::add("RenderVolumeLODFactorWarning");
    }
    // </FS:PP>

    return true;
}
// <FS:Beq> Override VRAM detection support
static bool handleOverrideVRAMDetectionChanged(const LLSD& newvalue)
{
    if (newvalue.asBoolean())
    {
        LLNotificationsUtil::add("OverrideVRAMWarning");
    }
    return true;
}
// </FS:Beq>

static bool handleAvatarLODChanged(const LLSD& newvalue)
{
    LLVOAvatar::sLODFactor = llclamp((F32) newvalue.asReal(), 0.f, MAX_AVATAR_LOD_FACTOR);
    return true;
}

static bool handleAvatarPhysicsLODChanged(const LLSD& newvalue)
{
    LLVOAvatar::sPhysicsLODFactor = llclamp((F32) newvalue.asReal(), 0.f, MAX_AVATAR_LOD_FACTOR);
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

static bool handleChatFontSizeChanged(const LLSD& newvalue)
{
    if(gConsole)
    {
        gConsole->setFontSize(newvalue.asInteger());
    }
    return true;
}

// <FS:Ansariel> Keep custom chat persist time
static bool handleChatPersistTimeChanged(const LLSD& newvalue)
{
    if(gConsole)
    {
        gConsole->setLinePersistTime((F32) newvalue.asReal());
    }
    return true;
}
// </FS:Ansariel>

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
    LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);
    return true;
}

static bool handleUseOcclusionChanged(const LLSD& newvalue)
{
    LLPipeline::sUseOcclusion = (newvalue.asBoolean()
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

// static bool handleReflectionsEnabled(const LLSD& newvalue)
// {
//  // <FS:Beq> FIRE-33659 - everything is too dark when reflections are disabled.
//  if(newvalue.asBoolean())
//  {
//      // TODO(Beq): This setting level should probably be governed by render quality settings.
//      gSavedSettings.setS32("RenderReflectionProbeLevel", 3);
//  }
//  else
//  {
//      gSavedSettings.setS32("RenderReflectionProbeLevel", 0);
//  }
//     return true;
// }

static bool handleReflectionProbeDetailChanged(const LLSD& newvalue)
{
    if (gPipeline.isInit())
    {
        LLPipeline::refreshCachedSettings();
        gPipeline.mReflectionMapManager.reset();
        gPipeline.mHeroProbeManager.reset();
        gPipeline.releaseGLBuffers();
        gPipeline.createGLBuffers();
        LLViewerShaderMgr::instance()->setShaders();
    }
    return true;
}

#if LL_DARWIN
static bool handleAppleUseMultGLChanged(const LLSD& newvalue)
{
    if (gGLManager.mInited)
    {
        LLWindowMacOSX::setUseMultGL(newvalue.asBoolean());
    }
    return true;
}
#endif

static bool handleHeroProbeResolutionChanged(const LLSD &newvalue)
{
    if (gPipeline.isInit())
    {
        LLPipeline::refreshCachedSettings();
        gPipeline.mHeroProbeManager.reset();
        gPipeline.releaseGLBuffers();
        gPipeline.createGLBuffers();
    }
    return true;
}

static bool handleRenderDebugPipelineChanged(const LLSD& newvalue)
{
    gDebugPipeline = newvalue.asBoolean();
    return true;
}

static bool handleRenderResolutionDivisorChanged(const LLSD&)
{
    gResizeScreenTexture = true;
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
    LL_INFOS() << "Logging switched to " << log_filename << LL_ENDL;
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
        gSavedSettings.setBOOL( "RenderUIInSnapshot", false);
    }
    return true;
}

bool handleVoiceClientPrefsChanged(const LLSD& newvalue)
{
    if (LLVoiceClient::instanceExists())
    {
        LLVoiceClient::getInstance()->updateSettings();
    }
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

bool handleForceShowGrid(const LLSD& newvalue)
{
    // <FS:Ansariel> [FS Login Panel]
    //LLPanelLogin::updateLocationSelectorsVisibility( );
    FSPanelLogin::updateLocationSelectorsVisibility( );
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
    //gViewerWindow->reshapeStatusBarContainer();
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
        focus->setFocus(true);
    }
}
// </FS:Ansariel>

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
    LLAvatarNameCache::getInstance()->clearCache();
    LLVOAvatar::invalidateNameTags();
    FSFloaterContacts::getInstance()->onDisplayNameChanged();
    FSRadar::getInstance()->updateNames();
}

void handleUsernameFormatOptionChanged(const LLSD& newvalue)
{
    LLAvatarName::setUseLegacyFormat(newvalue.asBoolean());
    LLAvatarNameCache::getInstance()->clearCache();
    LLVOAvatar::invalidateNameTags();
    FSFloaterContacts::getInstance()->onDisplayNameChanged();
    FSRadar::getInstance()->updateNames();
}
// </FS:CR>

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

// <FS:Ansariel> Notification not showing if hiding the UI
void handleNavbarSettingsChanged()
{
    gSavedSettings.setBOOL("FSInternalShowNavbarNavigationPanel", gSavedSettings.getBOOL("ShowNavbarNavigationPanel"));
    gSavedSettings.setBOOL("FSInternalShowNavbarFavoritesPanel", gSavedSettings.getBOOL("ShowNavbarFavoritesPanel"));
}
// </FS:Ansariel>

// <FS:Ansariel> FIRE-20288: Option to render friends only
void handleRenderFriendsOnlyChanged(const LLSD& newvalue)
{
    if (newvalue.asBoolean())
    {
        for (auto character : LLCharacter::sInstances)
        {
            LLVOAvatar* avatar = static_cast<LLVOAvatar*>(character);

            if (avatar->getID() != gAgentID && !LLAvatarActions::isFriend(avatar->getID()) && !avatar->isControlAvatar())
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

// <FS:LO> Add ability for the statistics window to not be able to receive focus
void handleFSStatisticsNoFocusChanged(const LLSD& newvalue)
{
    LLFloater* stats = LLFloaterReg::findInstance("stats");
    if (stats)
    {
        stats->setIsChrome(newvalue.asBoolean());
    }
}
// </FS:LO>

// <FS:Ansariel> Output device selection
void handleOutputDeviceChanged(const LLSD& newvalue)
{
    if (gAudiop)
    {
        gAudiop->setDevice(newvalue.asUUID());
    }
}
// </FS:Ansariel>

// <FS:TS> FIRE-24081: Disable HiDPI by default and warn if set
void handleRenderHiDPIChanged(const LLSD& newvalue)
{
    if (newvalue)
    {
        LLNotificationsUtil::add("EnableHiDPI");
    }
}
// </FS:TS> FIRE-24081

// <FS:Ansariel> Optional small camera floater
void handleSmallCameraFloaterChanged(const LLSD& newValue)
{
    std::string old_floater_name = newValue.asBoolean() ? "camera" : "fs_camera_small";
    std::string new_floater_name = newValue.asBoolean() ? "fs_camera_small" : "camera";

    if (LLFloaterReg::instanceVisible(old_floater_name))
    {
        LLFloaterReg::hideInstance(old_floater_name);
        LLFloaterReg::showInstance(new_floater_name);
    }
}
// </FS:Ansariel>

// <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
void handleStatusbarTimeformatChanged(const LLSD& newValue)
{
    const std::string format = newValue.asString();
    if (gStatusBar)
    {
        gStatusBar->onTimeFormatChanged(format);
    }
}
// </FS:Zi>

// <FS:Zi> Run Prio 0 default bento pose in the background to fix splayed hands, open mouths, etc.
void handlePlayBentoIdleAnimationChanged(const LLSD& newValue)
{
    EAnimRequest startStop = ANIM_REQUEST_STOP;

    if (newValue.asBoolean())
    {
        startStop = ANIM_REQUEST_START;
    }

    gAgent.sendAnimationRequest(ANIM_AGENT_BENTO_IDLE, startStop);
}
// </FS:Zi>

// <FS:Ansariel> Better asset cache size control
void handleDiskCacheSizeChanged(const LLSD& newValue)
{
    const unsigned int disk_cache_mb = gSavedSettings.getU32("FSDiskCacheSize");
    const U64 disk_cache_bytes = disk_cache_mb * 1024ULL * 1024ULL;
    LLDiskCache::getInstance()->setMaxSizeBytes(disk_cache_bytes);
}
// </FS:Ansariel>

// <FS:Beq> Better asset cache purge control
void handleDiskCacheHighWaterPctChanged(const LLSD& newValue)
{
    const auto new_high = (F32)newValue.asReal();
    LLDiskCache::getInstance()->setHighWaterPercentage(new_high);
}

void handleDiskCacheLowWaterPctChanged(const LLSD& newValue)
{
    const auto new_low = (F32)newValue.asReal();
    LLDiskCache::getInstance()->setLowWaterPercentage(new_low);
}
// </FS:Beq>

void handleTargetFPSChanged(const LLSD& newValue)
{
    const auto targetFPS = gSavedSettings.getU32("TargetFPS");

    U32 frame_rate_limit = gViewerWindow->getWindow()->getRefreshRate();
    if(LLPerfStats::tunables.vsyncEnabled && (targetFPS > frame_rate_limit))
    {
        gSavedSettings.setU32("TargetFPS", std::min(frame_rate_limit, targetFPS));
    }
    else
    {
        LLPerfStats::tunables.userTargetFPS = targetFPS;
    }
}

void handleAutoTuneLockChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getBOOL("AutoTuneLock");
    LLPerfStats::tunables.userAutoTuneLock = newval;

    gSavedSettings.setBOOL("AutoTuneFPS", newval);
}

void handleAutoTuneFPSChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getBOOL("AutoTuneFPS");
    LLPerfStats::tunables.userAutoTuneEnabled = newval;
    if(newval && LLPerfStats::renderAvatarMaxART_ns == 0) // If we've enabled autotune we override "unlimited" to max
    {
        gSavedSettings.setF32("RenderAvatarMaxART", (F32)log10(LLPerfStats::ART_UNLIMITED_NANOS-1000));//triggers callback to update static var
    }
}

void handleRenderAvatarMaxARTChanged(const LLSD& newValue)
{
    LLPerfStats::tunables.updateRenderCostLimitFromSettings();
}

void handleUserTargetDrawDistanceChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getF32("AutoTuneRenderFarClipTarget");
    LLPerfStats::tunables.userTargetDrawDistance = newval;
}

void handleUserMinDrawDistanceChanged(const LLSD &newValue)
{
    const auto newval = gSavedSettings.getF32("AutoTuneRenderFarClipMin");
    LLPerfStats::tunables.userMinDrawDistance = newval;
}

void handlePerformanceStatsEnabledChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getBOOL("PerfStatsCaptureEnabled");
    LLPerfStats::StatsRecorder::setEnabled(newval);
}
void handleUserImpostorByDistEnabledChanged(const LLSD& newValue)
{
    bool auto_tune_newval = false;
    S32 mode = gSavedSettings.getS32("RenderAvatarComplexityMode");
    if (mode != LLVOAvatar::AV_RENDER_ONLY_SHOW_FRIENDS)
    {
        auto_tune_newval = gSavedSettings.getBOOL("AutoTuneImpostorByDistEnabled");
    }
    LLPerfStats::tunables.userImpostorDistanceTuningEnabled = auto_tune_newval;
}
void handleUserImpostorDistanceChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getF32("AutoTuneImpostorFarAwayDistance");
    LLPerfStats::tunables.userImpostorDistance = newval;
}
void handleFPSTuningStrategyChanged(const LLSD& newValue)
{
    const auto newval = gSavedSettings.getU32("TuningFPSStrategy");
    LLPerfStats::tunables.userFPSTuningStrategy = newval;
}

void handleLocalTerrainChanged(const LLSD& newValue)
{
    for (U32 i = 0; i < LLTerrainMaterials::ASSET_COUNT; ++i)
    {
        const auto setting = gSavedSettings.getString(std::string("LocalTerrainAsset") + std::to_string(i + 1));
        const LLUUID materialID(setting);
        gLocalTerrainMaterials.setDetailAssetID(i, materialID);

        // *NOTE: The GLTF spec allows for different texture infos to have their texture transforms set independently, but as a simplification, this debug setting only updates all the transforms in-sync (i.e. only one texture transform per terrain material).
        LLGLTFMaterial::TextureTransform transform;
        const std::string prefix = std::string("LocalTerrainTransform") + std::to_string(i + 1);
        transform.mScale.mV[VX] = gSavedSettings.getF32(prefix + "ScaleU");
        transform.mScale.mV[VY] = gSavedSettings.getF32(prefix + "ScaleV");
        transform.mRotation = gSavedSettings.getF32(prefix + "Rotation") * DEG_TO_RAD;
        transform.mOffset.mV[VX] = gSavedSettings.getF32(prefix + "OffsetU");
        transform.mOffset.mV[VY] = gSavedSettings.getF32(prefix + "OffsetV");
        LLPointer<LLGLTFMaterial> mat_override = new LLGLTFMaterial();
        for (U32 info = 0; info < LLGLTFMaterial::GLTF_TEXTURE_INFO_COUNT; ++info)
        {
            mat_override->mTextureTransform[info] = transform;
        }
        if (*mat_override == LLGLTFMaterial::sDefault)
        {
            gLocalTerrainMaterials.setMaterialOverride(i, nullptr);
        }
        else
        {
            gLocalTerrainMaterials.setMaterialOverride(i, mat_override);
        }
        const bool paint_enabled = gSavedSettings.getBOOL("LocalTerrainPaintEnabled");
        gLocalTerrainMaterials.setPaintType(paint_enabled ? TERRAIN_PAINT_TYPE_PBR_PAINTMAP : TERRAIN_PAINT_TYPE_HEIGHTMAP_WITH_NOISE);
    }
}

// <FS:Ansariel> FIRE-6809: Quickly moving the bandwidth slider has no effect
void handleBandwidthChanged(const LLSD& newValue)
{
    sBandwidthUpdater.update(newValue);
}
// </FS:Ansariel>

// <FS:Zi> Handle IME text input getting enabled or disabled
#if LL_SDL2
static bool handleSDL2IMEEnabledChanged(const LLSD& newvalue)
{
    ((LLWindowSDL*)gViewerWindow->getWindow())->enableIME(newvalue.asBoolean());

    return true;
}
#endif
// </FS:Zi>

////////////////////////////////////////////////////////////////////////////

LLPointer<LLControlVariable> setting_get_control(LLControlGroup& group, const std::string& setting)
{
    LLPointer<LLControlVariable> cntrl_ptr = group.getControl(setting);
    if (cntrl_ptr.isNull())
    {
        LLError::LLUserWarningMsg::showMissingFiles();
        LL_ERRS() << "Unable to set up setting listener for " << setting
            << "." << LL_ENDL;
    }
    return cntrl_ptr;
}

void setting_setup_signal_listener(LLControlGroup& group, const std::string& setting, std::function<void(const LLSD& newvalue)> callback)
{
    setting_get_control(group, setting)->getSignal()->connect([callback](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
    {
        callback(new_val);
    });
}

void setting_setup_signal_listener(LLControlGroup& group, const std::string& setting, std::function<void()> callback)
{
    setting_get_control(group, setting)->getSignal()->connect([callback](LLControlVariable* control, const LLSD& new_val, const LLSD& old_val)
    {
        callback();
    });
}

// <FS:WW> Feature: Fullbright Toggle - Handle preference change
static bool handleFullbrightChanged(const LLSD& newvalue)
{
    BOOL enabled = gSavedSettings.getBOOL("FSRenderEnableFullbright");

    if (!enabled)
    {
        gObjectList.killAllFullbrights();
    }
    else
    {
        gObjectList.restoreAllFullbrights(); 
    }
    return true;
}
// </FS:WW>
void settings_setup_listeners()
{
       
    setting_setup_signal_listener(gSavedSettings, "FSRenderEnableFullbright", handleFullbrightChanged); // <FS:WW> Feature: Fullbright Toggle - Add listener for preference changes 
	setting_setup_signal_listener(gSavedSettings, "FirstPersonAvatarVisible", handleRenderAvatarMouselookChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderFarClip", handleRenderFarClipChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainScale", handleTerrainScaleChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainPBRScale", handlePBRTerrainScaleChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainPBRDetail", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainPBRPlanarSampleCount", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainPBRTriplanarBlendFactor", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "OctreeStaticObjectSizeFactor", handleRepartition);
    setting_setup_signal_listener(gSavedSettings, "OctreeDistanceFactor", handleRepartition);
    setting_setup_signal_listener(gSavedSettings, "OctreeMaxNodeCapacity", handleRepartition);
    setting_setup_signal_listener(gSavedSettings, "OctreeAlphaDistanceFactor", handleRepartition);
    setting_setup_signal_listener(gSavedSettings, "OctreeAttachmentSizeFactor", handleRepartition);
    setting_setup_signal_listener(gSavedSettings, "RenderMaxTextureIndex", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderUIBuffer", handleWindowResized);
    setting_setup_signal_listener(gSavedSettings, "RenderDepthOfField", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderFSAAType", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderSpecularResX", handleLUTBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderSpecularResY", handleLUTBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderSpecularExponent", handleLUTBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderAnisotropic", handleAnisotropicChanged);
    // <FS:WW> Ensure shader update on shadow resolution scale change for correct shadow rendering.
    // setting_setup_signal_listener(gSavedSettings, "RenderShadowResolutionScale", handleShadowsResized);
    setting_setup_signal_listener(gSavedSettings, "RenderShadowResolutionScale", handleSetShaderChanged);
    // </FS>
    setting_setup_signal_listener(gSavedSettings, "RenderGlow", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderGlow", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderGlowResolutionPow", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderGlowHDR", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderEnableEmissiveBuffer", handleEnableEmissiveChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderDisableVintageMode", handleDisableVintageMode);
    setting_setup_signal_listener(gSavedSettings, "RenderHDREnabled", handleEnableHDR);
    setting_setup_signal_listener(gSavedSettings, "RenderGlowNoise", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderGammaFull", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "FSOverrideVRAMDetection", handleOverrideVRAMDetectionChanged); // <FS:Beq/> Override VRAM detection support
    setting_setup_signal_listener(gSavedSettings, "RenderVolumeLODFactor", handleVolumeLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderAvatarComplexityMode", handleUserImpostorByDistEnabledChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderAvatarLODFactor", handleAvatarLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderAvatarPhysicsLODFactor", handleAvatarPhysicsLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTerrainLODFactor", handleTerrainLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderTreeLODFactor", handleTreeLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderFlexTimeFactor", handleFlexLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderGamma", handleGammaChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderFogRatio", handleFogRatioChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderMaxPartCount", handleMaxPartCountChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderDynamicLOD", handleRenderDynamicLODChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderVSyncEnable", handleVSyncChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderDeferredNoise", handleReleaseGLBufferChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderDebugPipeline", handleRenderDebugPipelineChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderResolutionDivisor", handleRenderResolutionDivisorChanged);
// [SL:KB] - Patch: Settings-RenderResolutionMultiplier | Checked: Catznip-5.4
    setting_setup_signal_listener(gSavedSettings, "RenderResolutionMultiplier", handleRenderResolutionDivisorChanged);
// [/SL:KB]
    setting_setup_signal_listener(gSavedSettings, "RenderReflectionProbeLevel", handleReflectionProbeDetailChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderReflectionProbeDetail", handleReflectionProbeDetailChanged);
    // setting_setup_signal_listener(gSavedSettings, "RenderReflectionsEnabled", handleReflectionsEnabled); // <FS:Beq/> FIRE-33659 better way to enable/disable reflections
#if LL_DARWIN
    setting_setup_signal_listener(gSavedSettings, "RenderAppleUseMultGL", handleAppleUseMultGLChanged);
#endif
    setting_setup_signal_listener(gSavedSettings, "RenderScreenSpaceReflections", handleReflectionProbeDetailChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderMirrors", handleReflectionProbeDetailChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderHeroProbeResolution", handleHeroProbeResolutionChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderShadowDetail", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderDeferredSSAO", handleSetShaderChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderPerformanceTest", handleRenderPerfTestChanged);
    setting_setup_signal_listener(gSavedSettings, "ChatConsoleFontSize", handleChatFontSizeChanged);
    setting_setup_signal_listener(gSavedSettings, "ChatPersistTime", handleChatPersistTimeChanged); // <FS:Ansariel> Keep custom chat persist time
    setting_setup_signal_listener(gSavedSettings, "ConsoleMaxLines", handleConsoleMaxLinesChanged);
    setting_setup_signal_listener(gSavedSettings, "UploadBakedTexOld", handleUploadBakedTexOldChanged);
    setting_setup_signal_listener(gSavedSettings, "UseOcclusion", handleUseOcclusionChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelMaster", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelSFX", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelUI", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelAmbient", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelMusic", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelMedia", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelVoice", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteAudio", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteMusic", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteMedia", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteVoice", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteAmbient", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "MuteUI", handleAudioVolumeChanged);
    setting_setup_signal_listener(gSavedSettings, "WLSkyDetail", handleWLSkyDetailChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "JoystickAxis6", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisScale6", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "FlycamAxisDeadZone6", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisScale5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "AvatarAxisDeadZone5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisScale5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone0", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone1", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone2", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone3", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone4", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "BuildAxisDeadZone5", handleJoystickChanged);
    setting_setup_signal_listener(gSavedSettings, "DebugViews", handleDebugViewsChanged);
    setting_setup_signal_listener(gSavedSettings, "UserLogFile", handleLogFileChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderHideGroupTitle", handleHideGroupTitleChanged);
    setting_setup_signal_listener(gSavedSettings, "HighResSnapshot", handleHighResSnapshotChanged);
    setting_setup_signal_listener(gSavedSettings, "EnableVoiceChat", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "PTTCurrentlyEnabled", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "PushToTalkButton", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "PushToTalkToggle", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceEarLocation", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceEchoCancellation", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceAutomaticGainControl", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceNoiseSuppressionLevel", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceInputAudioDevice", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VoiceOutputAudioDevice", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "AudioLevelMic", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "LipSyncEnabled", handleVoiceClientPrefsChanged);
    setting_setup_signal_listener(gSavedSettings, "VelocityInterpolate", handleVelocityInterpolate);
    setting_setup_signal_listener(gSavedSettings, "QAMode", show_debug_menus);
    setting_setup_signal_listener(gSavedSettings, "UseDebugMenus", show_debug_menus);
    setting_setup_signal_listener(gSavedSettings, "AgentPause", toggle_agent_pause);
    // <FS:Zi> Is done inside XUI now, using visibility_control
    // setting_setup_signal_listener(gSavedSettings, "ShowNavbarNavigationPanel", toggle_show_navigation_panel);
    // </FS:Zi>
    // <FS:Zi> We don't have the mini location bar
    // setting_setup_signal_listener(gSavedSettings, "ShowMiniLocationPanel", toggle_show_mini_location_panel);
    // </FS: Zi>
    setting_setup_signal_listener(gSavedSettings, "ShowMenuBarLocation", toggle_show_menubar_location_panel);
    setting_setup_signal_listener(gSavedSettings, "ShowObjectRenderingCost", toggle_show_object_render_cost);
    setting_setup_signal_listener(gSavedSettings, "ForceShowGrid", handleForceShowGrid);
    // <FS:Ansariel> Show start location setting has no effect on login
    setting_setup_signal_listener(gSavedSettings, "ShowStartLocation", handleForceShowGrid);
    setting_setup_signal_listener(gSavedSettings, "RenderTransparentWater", handleRenderTransparentWaterChanged);
    setting_setup_signal_listener(gSavedSettings, "SpellCheck", handleSpellCheckChanged);
    setting_setup_signal_listener(gSavedSettings, "SpellCheckDictionary", handleSpellCheckChanged);
    setting_setup_signal_listener(gSavedSettings, "LoginLocation", handleLoginLocationChanged);
    setting_setup_signal_listener(gSavedSettings, "DebugAvatarJoints", handleDebugAvatarJointsChanged);

    setting_setup_signal_listener(gSavedSettings, "TargetFPS", handleTargetFPSChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneFPS", handleAutoTuneFPSChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneLock", handleAutoTuneLockChanged);
    setting_setup_signal_listener(gSavedSettings, "RenderAvatarMaxART", handleRenderAvatarMaxARTChanged);
    setting_setup_signal_listener(gSavedSettings, "PerfStatsCaptureEnabled", handlePerformanceStatsEnabledChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneRenderFarClipTarget", handleUserTargetDrawDistanceChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneRenderFarClipMin", handleUserMinDrawDistanceChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneImpostorFarAwayDistance", handleUserImpostorDistanceChanged);
    setting_setup_signal_listener(gSavedSettings, "AutoTuneImpostorByDistEnabled", handleUserImpostorByDistEnabledChanged);
    setting_setup_signal_listener(gSavedSettings, "TuningFPSStrategy", handleFPSTuningStrategyChanged);
    {
        setting_setup_signal_listener(gSavedSettings, "LocalTerrainPaintEnabled", handleLocalTerrainChanged);
        const char* transform_suffixes[] = {
            "ScaleU",
            "ScaleV",
            "Rotation",
            "OffsetU",
            "OffsetV"
        };
        for (U32 i = 0; i < LLTerrainMaterials::ASSET_COUNT; ++i)
        {
            const auto asset_setting_name = std::string("LocalTerrainAsset") + std::to_string(i + 1);
            setting_setup_signal_listener(gSavedSettings, asset_setting_name, handleLocalTerrainChanged);
            for (const char* ts : transform_suffixes)
            {
                const auto transform_setting_name = std::string("LocalTerrainTransform") + std::to_string(i + 1) + ts;
                setting_setup_signal_listener(gSavedSettings, transform_setting_name, handleLocalTerrainChanged);
            }
        }
    }
    setting_setup_signal_listener(gSavedSettings, "TerrainPaintBitDepth", handleSetShaderChanged);

    setting_setup_signal_listener(gSavedPerAccountSettings, "AvatarHoverOffsetZ", handleAvatarHoverOffsetChanged);

// [RLVa:KB] - Checked: 2015-12-27 (RLVa-1.5.0)
    setting_setup_signal_listener(gSavedSettings, RlvSettingNames::Main, RlvSettings::onChangedSettingMain);
// [/RLVa:KB]
    // NaCl - Antispam Registry
    setting_setup_signal_listener(gSavedSettings, "_NACL_AntiSpamGlobalQueue", handleNaclAntiSpamGlobalQueueChanged);
    setting_setup_signal_listener(gSavedSettings, "_NACL_AntiSpamTime", handleNaclAntiSpamTimeChanged);
    setting_setup_signal_listener(gSavedSettings, "_NACL_AntiSpamAmount", handleNaclAntiSpamAmountChanged);
    // NaCl End
    setting_setup_signal_listener(gSavedSettings, "AutohideChatBar", handleAutohideChatbarChanged);

    // <FS:Ansariel> Clear places / teleport history search filter
    setting_setup_signal_listener(gSavedSettings, "FSUseStandaloneTeleportHistoryFloater", handleUseStandaloneTeleportHistoryFloaterChanged);

    // <FS:CR> Pose stand ground lock
    setting_setup_signal_listener(gSavedSettings, "FSPoseStandLock", handleSetPoseStandLock);

    setting_setup_signal_listener(gSavedPerAccountSettings, "UseLSLFlightAssist", handleFlightAssistOptionChanged);
    setting_setup_signal_listener(gSavedPerAccountSettings, "UseMoveLock", handleMovelockOptionChanged);
    setting_setup_signal_listener(gSavedPerAccountSettings, "RelockMoveLockAfterMovement", handleMovelockAfterMoveOptionChanged);
    setting_setup_signal_listener(gSavedSettings, "FSBuildToolDecimalPrecision", handleDecimalPrecisionChanged);

    // <FS:PP> External integrations (OC, LM etc.) for Bridge
    setting_setup_signal_listener(gSavedPerAccountSettings, "BridgeIntegrationOC", handleExternalIntegrationsOptionChanged);
    setting_setup_signal_listener(gSavedPerAccountSettings, "BridgeIntegrationLM", handleExternalIntegrationsOptionChanged);

    setting_setup_signal_listener(gSavedSettings, "FSNameTagShowLegacyUsernames", handleUsernameFormatOptionChanged);
    setting_setup_signal_listener(gSavedSettings, "FSTrimLegacyNames", handleLegacyTrimOptionChanged);

    // <FS:Ansariel> [FS communication UI]
    setting_setup_signal_listener(gSavedSettings, "PlainTextChatHistory", FSFloaterIM::processChatHistoryStyleUpdate);
    setting_setup_signal_listener(gSavedSettings, "PlainTextChatHistory", FSFloaterNearbyChat::processChatHistoryStyleUpdate);
    setting_setup_signal_listener(gSavedSettings, "ChatFontSize", FSFloaterIM::processChatHistoryStyleUpdate);
    setting_setup_signal_listener(gSavedSettings, "ChatFontSize", FSFloaterNearbyChat::processChatHistoryStyleUpdate);
    setting_setup_signal_listener(gSavedSettings, "ChatFontSize", LLViewerChat::signalChatFontChanged);
    // </FS:Ansariel> [FS communication UI]

    setting_setup_signal_listener(gSavedPerAccountSettings, "GlobalOnlineStatusToggle", handleGlobalOnlineStatusChanged);

    // <FS:Ansariel> FIRE-17393: Control HUD text fading by options
    setting_setup_signal_listener(gSavedSettings, "FSHudTextFadeDistance", LLHUDText::onFadeSettingsChanged);
    setting_setup_signal_listener(gSavedSettings, "FSHudTextFadeRange", LLHUDText::onFadeSettingsChanged);

    //<FS:HG> FIRE-6340, FIRE-6567, FIRE-6809 - Setting Bandwidth issues
    setting_setup_signal_listener(gSavedSettings, "ThrottleBandwidthKBPS", handleBandwidthChanged);
    setting_setup_signal_listener(gSavedSettings, "FSContactListShowSearch", handleContactListShowSearchChanged);

    // <FS:Ansariel> Debug setting to disable log throttle
    setting_setup_signal_listener(gSavedSettings, "FSEnableLogThrottle", handleLogThrottleChanged);

    // <FS:Ansariel> FIRE-18250: Option to disable default eye movement
    setting_setup_signal_listener(gSavedSettings, "FSStaticEyesUUID", handleStaticEyesChanged);
    setting_setup_signal_listener(gSavedPerAccountSettings, "FSStaticEyes", handleStaticEyesChanged);
    // </FS:Ansariel>

    // <FS:Ansariel> FIRE-20288: Option to render friends only
    setting_setup_signal_listener(gSavedPerAccountSettings, "FSRenderFriendsOnly", handleRenderFriendsOnlyChanged);

    // <FS:Ansariel> Notification not showing if hiding the UI
    setting_setup_signal_listener(gSavedSettings, "ShowNavbarFavoritesPanel", handleNavbarSettingsChanged);
    setting_setup_signal_listener(gSavedSettings, "ShowNavbarNavigationPanel", handleNavbarSettingsChanged);
    // </FS:Ansariel>

    // <FS:LO> Add ability for the statistics window to not be able to receive focus
    setting_setup_signal_listener(gSavedSettings, "FSStatisticsNoFocus", handleFSStatisticsNoFocusChanged);
    // </FS:LO>

    // <FS:Ansariel> Output device selection
    setting_setup_signal_listener(gSavedSettings, "FSOutputDeviceUUID", handleOutputDeviceChanged);

    // <FS:Ansariel> Optional small camera floater
    setting_setup_signal_listener(gSavedSettings, "FSUseSmallCameraFloater", handleSmallCameraFloaterChanged);

    // <FS:Zi> FIRE-20390, FIRE-4269 - Option for 12/24 hour clock and seconds display
    setting_setup_signal_listener(gSavedSettings, "FSStatusBarTimeFormat", handleStatusbarTimeformatChanged);

    // <FS:Zi> Run Prio 0 default bento pose in the background to fix splayed hands, open mouths, etc.
    setting_setup_signal_listener(gSavedSettings, "FSPlayDefaultBentoAnimation", handlePlayBentoIdleAnimationChanged);

    // <FS:Ansariel> Better asset cache size control
    setting_setup_signal_listener(gSavedSettings, "FSDiskCacheSize", handleDiskCacheSizeChanged);
    // <FS:Beq> Better asset cache purge control
    setting_setup_signal_listener(gSavedSettings, "FSDiskCacheHighWaterPercent", handleDiskCacheHighWaterPctChanged);
    setting_setup_signal_listener(gSavedSettings, "FSDiskCacheLowWaterPercent", handleDiskCacheLowWaterPctChanged);
    // </FS:Beq>

    // <FS:Zi> Handle IME text input getting enabled or disabled
#if LL_SDL2
    setting_setup_signal_listener(gSavedSettings, "SDL2IMEEnabled", handleSDL2IMEEnabledChanged);
#endif
    // </FS:Zi>
}

#if TEST_CACHED_CONTROL

#define DECL_LLCC(T, V) static LLCachedControl<T> mySetting_##T("TestCachedControl"#T, V)
DECL_LLCC(U32, (U32)666);
DECL_LLCC(S32, (S32)-666);
DECL_LLCC(F32, (F32)-666.666);
DECL_LLCC(bool, true);
DECL_LLCC(bool, false);
static LLCachedControl<std::string> mySetting_string("TestCachedControlstring", "Default String Value");
DECL_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
DECL_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
DECL_LLCC(LLRect, LLRect(0, 0, 100, 500));
DECL_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
DECL_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
DECL_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));

LLSD test_llsd = LLSD()["testing1"] = LLSD()["testing2"];
DECL_LLCC(LLSD, test_llsd);

void test_cached_control()
{
#define do { TEST_LLCC(T, V) if((T)mySetting_##T != V) LL_ERRS() << "Fail "#T << LL_ENDL; } while(0)
    TEST_LLCC(U32, 666);
    TEST_LLCC(S32, (S32)-666);
    TEST_LLCC(F32, (F32)-666.666);
    TEST_LLCC(bool, true);
    TEST_LLCC(bool, false);
    if((std::string)mySetting_string != "Default String Value") LL_ERRS() << "Fail string" << LL_ENDL;
    TEST_LLCC(LLVector3, LLVector3(1.0f, 2.0f, 3.0f));
    TEST_LLCC(LLVector3d, LLVector3d(6.0f, 5.0f, 4.0f));
    TEST_LLCC(LLRect, LLRect(0, 0, 100, 500));
    TEST_LLCC(LLColor4, LLColor4(0.0f, 0.5f, 1.0f));
    TEST_LLCC(LLColor3, LLColor3(1.0f, 0.f, 0.5f));
    TEST_LLCC(LLColor4U, LLColor4U(255, 200, 100, 255));
//There's no LLSD comparsion for LLCC yet. TEST_LLCC(LLSD, test_llsd);
}
#endif // TEST_CACHED_CONTROL


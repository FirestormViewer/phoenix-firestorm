/**
 * @file quickprefs.cpp
 * @brief Quick preferences access panel for bottomtray
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, WoLf Loonie @ Second Life
 * Copyright (C) 2013, Zi Ree @ Second Life
 * Copyright (C) 2013, Ansariel Hiller @ Second Life
 * Copyright (C) 2013, Cinder Biscuits @ Me too
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "quickprefs.h"

#include "fscommon.h"
#include "llagent.h"
#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llcubemap.h"
#include "llenvironment.h"
#include "llf32uictrl.h"
#include "llfeaturemanager.h"
#include "llfloaterpreference.h" // for LLAvatarComplexityControls
#include "llfloaterreg.h"
#include "llinventoryfunctions.h"
#include "lllayoutstack.h"
#include "llnotificationsutil.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltoolbarview.h"
#include "llviewercontrol.h"
#include "llviewernetwork.h" // <FS:Beq/> for LLGridManager
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "llvoavatarself.h"
#include "rlvhandler.h"

class FSSettingsCollector : public LLInventoryCollectFunctor
{
public:
    FSSettingsCollector()
    {
        mMarketplaceFolderUUID = gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS);
    }

    virtual ~FSSettingsCollector() {}

    bool operator()(LLInventoryCategory* cat, LLInventoryItem* item)
    {
        if (item && item->getType() == LLAssetType::AT_SETTINGS &&
            !gInventory.isObjectDescendentOf(item->getUUID(), mMarketplaceFolderUUID) &&
            mSeen.find(item->getAssetUUID()) == mSeen.end())
        {
            mSeen.insert(item->getAssetUUID());
            return true;
        }
        else
        {
            return false;
        }
    }

protected:
    LLUUID mMarketplaceFolderUUID;
    std::set<LLUUID> mSeen;
};


FloaterQuickPrefs::QuickPrefsXML::QuickPrefsXML()
:   entries("entries")
{}

FloaterQuickPrefs::QuickPrefsXMLEntry::QuickPrefsXMLEntry()
:   control_name("control_name"),
    label("label"),
    translation_id("translation_id"),
    control_type("control_type"),
    integer("integer"),
    min_value("min"),       // "min" is frowned upon by a braindead windows include
    max_value("max"),       // "max" see "min"
    increment("increment")
{}
// </FS:Zi>

FloaterQuickPrefs::FloaterQuickPrefs(const LLSD& key)
:   LLTransientDockableFloater(NULL, false, key),
    mAvatarZOffsetSlider(NULL),
    mRlvBehaviorCallbackConnection(),
    mEnvChangedConnection(),
    mRegionChangedSlot()
{
        // <FS:WW> // Add registration for Animation Speed Reset Button Callback (Add this line):
        mCommitCallbackRegistrar.add("Quickprefs.ResetAnimationSpeed", boost::bind(&FloaterQuickPrefs::onClickResetAnimationSpeed, this, _1, _2));
        // </FS:WW>
    mCommitCallbackRegistrar.add("Quickprefs.ShaderChanged", boost::bind(&handleSetShaderChanged, LLSD()));

    if (!getIsPhototools() && !FSCommon::isLegacySkin())
    {
        LLTransientFloaterMgr::getInstance()->addControlView(this);
    }
}

FloaterQuickPrefs::~FloaterQuickPrefs()
{
    if (mRlvBehaviorCallbackConnection.connected())
    {
        mRlvBehaviorCallbackConnection.disconnect();
    }

    if (mRegionChangedSlot.connected())
    {
        mRegionChangedSlot.disconnect();
    }

    if (mEnvChangedConnection.connected())
    {
        mEnvChangedConnection.disconnect();
    }

    if (!getIsPhototools() && !FSCommon::isLegacySkin())
    {
        LLTransientFloaterMgr::getInstance()->removeControlView(this);
    }
}

void FloaterQuickPrefs::onOpen(const LLSD& key)
{
    loadPresets();
    setSelectedEnvironment();

    // Make sure IndirectMaxNonImpostors gets set properly
    LLAvatarComplexityControls::setIndirectMaxNonImpostors();

    // bail out here if this is a reused Phototools floater
    if (getIsPhototools())
    {
        return;
    }

    // Make sure IndirectMaxComplexity gets set properly
    LLAvatarComplexityControls::setIndirectMaxArc();
    LLAvatarComplexityControls::setText(gSavedSettings.getU32("RenderAvatarMaxComplexity"), mMaxComplexityLabel);

    gSavedSettings.setBOOL("QuickPrefsEditMode", false);

    // Scan widgets and reapply control variables because some control types
    // (LLSliderCtrl for example) don't update their GUI when hidden
    control_list_t::iterator it;
    for (it = mControlsList.begin(); it != mControlsList.end(); ++it)
    {
        const ControlEntry& entry = it->second;

        LLUICtrl* current_widget = entry.widget;
        if (!current_widget)
        {
            LL_WARNS() << "missing widget for control " << it->first << LL_ENDL;
            continue;
        }

        LLControlVariable* var = current_widget->getControlVariable();
        if (var)
        {
            current_widget->setValue(var->getValue());
        }
    }

    dockToToolbarButton();
}


void FloaterQuickPrefs::initCallbacks()
{
    getChild<LLUICtrl>("WaterPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeWaterPreset, this));
    getChild<LLUICtrl>("WLPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeSkyPreset, this));
    getChild<LLUICtrl>("DCPresetsCombo")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeDayCyclePreset, this));
    getChild<LLUICtrl>("WLPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyPrev, this));
    getChild<LLUICtrl>("WLNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickSkyNext, this));
    getChild<LLUICtrl>("WWPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterPrev, this));
    getChild<LLUICtrl>("WWNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickWaterNext, this));
    getChild<LLUICtrl>("DCPrevPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCyclePrev, this));
    getChild<LLUICtrl>("DCNextPreset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickDayCycleNext, this));
    getChild<LLUICtrl>("ResetToRegionDefault")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetToRegionDefault, this));

    // Phototools additions
    if (getIsPhototools())
    {
        gSavedSettings.getControl("RenderObjectBump")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("WindLightUseAtmosShaders")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("RenderDeferred")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("RenderShadowDetail")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("FSRenderVignette")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("RenderShadowSplitExponent")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("RenderShadowGaussian")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));
        gSavedSettings.getControl("RenderSSAOEffect")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::refreshSettings, this));

        // Vignette UI controls
        getChild<LLSpinCtrl>("VignetteSpinnerX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerX, this));
        getChild<LLSlider>("VignetteSliderX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteX, this));
        getChild<LLButton>("Reset_VignetteX")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteX, this));
        getChild<LLSpinCtrl>("VignetteSpinnerY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerY, this));
        getChild<LLSlider>("VignetteSliderY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteY, this));
        getChild<LLButton>("Reset_VignetteY")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteY, this));
        getChild<LLSpinCtrl>("VignetteSpinnerZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteSpinnerZ, this));
        getChild<LLSlider>("VignetteSliderZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeVignetteZ, this));
        getChild<LLButton>("Reset_VignetteZ")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetVignetteZ, this));

        getChild<LLSlider>("SB_Shd_Clarity")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowSplitExponentSlider, this));
        getChild<LLSpinCtrl>("S_Shd_Clarity")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowSplitExponentSpinner, this));
        getChild<LLButton>("Shd_Clarity_Reset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowSplitExponentY, this));

        getChild<LLSlider>("SB_Shd_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSlider, this));
        getChild<LLSlider>("SB_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSlider, this));
        getChild<LLSpinCtrl>("S_Shd_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner, this));
        getChild<LLSpinCtrl>("S_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner, this));
        getChild<LLButton>("Shd_Soften_Reset")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowGaussianX, this));
        getChild<LLButton>("Reset_AO_Soften")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderShadowGaussianY, this));

        getChild<LLSlider>("SB_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderSSAOEffectSlider, this));
        getChild<LLSpinCtrl>("S_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onChangeRenderSSAOEffectSpinner, this));
        getChild<LLButton>("Reset_Effect")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickResetRenderSSAOEffectX, this));
    }
    else
    {
        getChild<LLButton>("Restore_Btn")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onClickRestoreDefaults, this));
        gSavedSettings.getControl("QuickPrefsEditMode")->getSignal()->connect(boost::bind(&FloaterQuickPrefs::onEditModeChanged, this));    // <FS:Zi> Dynamic Quickprefs

        mAvatarZOffsetSlider->setSliderMouseUpCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetFinalCommit, this));
        mAvatarZOffsetSlider->setSliderEditorCommitCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetFinalCommit, this));
        mAvatarZOffsetSlider->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAvatarZOffsetSliderMoved, this));

        mMaxComplexitySlider->setCommitCallback(boost::bind(&FloaterQuickPrefs::updateMaxComplexity, this));
        gSavedSettings.getControl("RenderAvatarMaxComplexity")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::updateMaxComplexityLabel, this, _2));

        syncAvatarZOffsetFromPreferenceSetting();
        // Update slider on future pref changes.
        if (gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ"))
        {
            gSavedPerAccountSettings.getControl("AvatarHoverOffsetZ")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::syncAvatarZOffsetFromPreferenceSetting, this));
        }
        else
        {
            LL_WARNS() << "Control not found for AvatarHoverOffsetZ" << LL_ENDL;
        }

        if (!mRegionChangedSlot.connected())
        {
            mRegionChangedSlot = gAgent.addRegionChangedCallback(boost::bind(&FloaterQuickPrefs::onRegionChanged, this));
        }
    }

    mRlvBehaviorCallbackConnection = gRlvHandler.setBehaviourCallback(boost::bind(&FloaterQuickPrefs::updateRlvRestrictions, this, _1, _2));
    gSavedSettings.getControl("IndirectMaxNonImpostors")->getCommitSignal()->connect(boost::bind(&FloaterQuickPrefs::updateMaxNonImpostors, this, _2));

    mEnvChangedConnection = LLEnvironment::instance().setEnvironmentChanged([this](LLEnvironment::EnvSelection_t env, S32 version){ setSelectedEnvironment(); });
}

void FloaterQuickPrefs::loadDayCyclePresets(const std::multimap<std::string, LLUUID>& daycycle_map)
{
    mDayCyclePresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mDayCyclePresetsCombo->add(LLTrans::getString("QP_WL_None"), LLSD(PRESET_NAME_NONE), EAddPosition::ADD_BOTTOM, false);
    mDayCyclePresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = daycycle_map.begin(); it != daycycle_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mDayCyclePresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle presets to QP" << LL_ENDL;
        // WL still supported
        if (!daycycle_map.empty() && !LLEnvironment::getInstance()->mLegacyDayCycles.empty())
        {
            mDayCyclePresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacyDayCycles)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy day cycle " << preset_name << LL_ENDL;
            mDayCyclePresetsCombo->add(preset_name, LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy day cycle presets to QP" << LL_ENDL;
    }
#endif
}

void FloaterQuickPrefs::loadSkyPresets(const std::multimap<std::string, LLUUID>& sky_map)
{
    mWLPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mWLPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), EAddPosition::ADD_BOTTOM, false);
    mWLPresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = sky_map.begin(); it != sky_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mWLPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy sky presets to QP" << LL_ENDL;
        // WL still supported
        if (!sky_map.empty() && !LLEnvironment::getInstance()->mLegacySkies.empty())
        {
            mWLPresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacySkies)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy sky " << preset_name << LL_ENDL;
            // append "WL" to denote legacy. Have to create a new string not update the reference.
            mWLPresetsCombo->add(preset_name+ "[WL]", LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy sky presets to QP" << LL_ENDL;
    }
#endif
}

void FloaterQuickPrefs::loadWaterPresets(const std::multimap<std::string, LLUUID>& water_map)
{
    mWaterPresetsCombo->operateOnAll(LLComboBox::OP_DELETE);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Region_Default"), LLSD(PRESET_NAME_REGION_DEFAULT), EAddPosition::ADD_BOTTOM, false);
    mWaterPresetsCombo->add(LLTrans::getString("QP_WL_Day_Cycle_Based"), LLSD(PRESET_NAME_DAY_CYCLE), EAddPosition::ADD_BOTTOM, false);
    mWaterPresetsCombo->addSeparator();

    // Add setting presets.
    for (std::multimap<std::string, LLUUID>::const_iterator it = water_map.begin(); it != water_map.end(); ++it)
    {
        const std::string& preset_name = (*it).first;
        const LLUUID& asset_id = (*it).second;

        if (!preset_name.empty())
        {
            mWaterPresetsCombo->add(preset_name, LLSD(asset_id));
        }
    }
// <FS:Beq> Opensim legacy windlight support
// Opensim may support both environment and extenvironment caps on the same region
// we also need these disabled in SL on the OpenSim build.
#ifdef OPENSIM
    if(LLGridManager::getInstance()->isInOpenSim())
    {
        LL_DEBUGS("WindlightCaps") << "Adding legacy presets to QP" << LL_ENDL;
        // WL still supported
        if (!water_map.empty() && !LLEnvironment::getInstance()->mLegacyWater.empty())
        {
            mWaterPresetsCombo->addSeparator();
        }
        for(const auto& preset_name : LLEnvironment::getInstance()->mLegacyWater)
        {
            // we add by name and only build the envp on demand
            LL_DEBUGS("WindlightCaps") << "Adding legacy water " << preset_name << LL_ENDL;
            mWaterPresetsCombo->add(preset_name, LLSD(preset_name));
        }
        LL_DEBUGS("WindlightCaps") << "Done: Adding legacy water presets to QP" << LL_ENDL;
    }
#endif
}

void FloaterQuickPrefs::loadPresets()
{
    LLInventoryModel::cat_array_t cats;
    LLInventoryModel::item_array_t items;
    FSSettingsCollector collector;
    gInventory.collectDescendentsIf(LLUUID::null,
                                    cats,
                                    items,
                                    LLInventoryModel::EXCLUDE_TRASH,
                                    collector);

    std::multimap<std::string, LLUUID> sky_map;
    std::multimap<std::string, LLUUID> water_map;
    std::multimap<std::string, LLUUID> daycycle_map;

    for (LLInventoryModel::item_array_t::const_iterator it = items.begin(); it != items.end(); ++it)
    {
        LLInventoryItem* item = *it;

        LLSettingsType::type_e type = LLSettingsType::fromInventoryFlags(item->getFlags());
        switch (type)
        {
            case LLSettingsType::ST_SKY:
                sky_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                break;
            case LLSettingsType::ST_WATER:
                water_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                break;
            case LLSettingsType::ST_DAYCYCLE:
                daycycle_map.insert(std::make_pair(item->getName(), item->getAssetUUID()));
                break;
            default:
                LL_WARNS() << "Found invalid setting: " << item->getName() << LL_ENDL;
                break;
        }
    }

    loadWaterPresets(water_map);
    loadSkyPresets(sky_map);
    loadDayCyclePresets(daycycle_map);
}

void FloaterQuickPrefs::setDefaultPresetsEnabled(bool enabled)
{
    LLScrollListItem* item{ nullptr };

    item = mWLPresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mWLPresetsCombo->getItemByValue(LLSD(PRESET_NAME_DAY_CYCLE));
    if (item) item->setEnabled(enabled);

    item = mWaterPresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mWaterPresetsCombo->getItemByValue(LLSD(PRESET_NAME_DAY_CYCLE));
    if (item) item->setEnabled(enabled);

    item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    if (item) item->setEnabled(enabled);

    item = mDayCyclePresetsCombo->getItemByValue(LLSD(PRESET_NAME_NONE));
    if (item) item->setEnabled(enabled);
}

void FloaterQuickPrefs::setSelectedEnvironment()
{
    //LL_INFOS() << "EEP: getSelectedEnvironment: " << LLEnvironment::instance().getSelectedEnvironment() << LL_ENDL;

    setDefaultPresetsEnabled(true);
    mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));

    if (LLEnvironment::instance().getSelectedEnvironment() == LLEnvironment::ENV_LOCAL)
    {
        // Day cycle, fixed sky and fixed water may all be set at the same time
        // Check and set day cycle first. Fixed sky and water both override
        // the sky and water settings in a day cycle, so check them after the
        // day cycle. If no fixed sky or fixed water is set, they are either
        // defined in the day cycle or inherited from a higher environment level.
        LLSettingsDay::ptr_t day = LLEnvironment::instance().getEnvironmentDay(LLEnvironment::ENV_LOCAL);
        if (day)
        {
            //LL_INFOS() << "EEP: day name = " << day->getName() << " - asset id = " << day->getAssetId() << LL_ENDL;
            if( day->getAssetId().notNull())
            { // EEP processing
                mDayCyclePresetsCombo->selectByValue(LLSD(day->getAssetId()));
                // Sky and Water are part of a day cycle in EEP
                mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
                mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = day->getName();
                LL_DEBUGS("WindlightCaps") << "Current Day cycle is " << preset_name << LL_ENDL;
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                mDayCyclePresetsCombo->selectByValue(preset_name);
                // Sky is part of day so treat that as day cycle
                mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_DAY_CYCLE));
                // Water is not part of legacy day so we need to hunt around
                LLSettingsWater::ptr_t water = LLEnvironment::instance().getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL);
                if (water)
                {
                    // This is going to be possible. OS will support both Legacy and EEP
                    // so having a water EEP asset with a Legacy day cycle could happen.
                    LLUUID asset_id = water->getAssetId();
                    if (asset_id.notNull())
                    {
                        mWaterPresetsCombo->selectByValue(LLSD(asset_id));
                    }
                    else
                    {
                        std::string preset_name = water->getName();
                        if (preset_name == "_default_")
                        {
                            preset_name = "Default";
                        }
                        mWaterPresetsCombo->selectByValue(preset_name);
                    }
                }
            }
#endif //OPENSIM
        }
        else
        {
            mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_NONE));
        }

        LLSettingsSky::ptr_t sky = LLEnvironment::instance().getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL);
        if (sky)
        {
            //LL_INFOS() << "EEP: sky name = " << sky->getName() << " - asset id = " << sky->getAssetId() << LL_ENDL;
            if(sky->getAssetId().notNull())
            {
                mWLPresetsCombo->selectByValue(LLSD(sky->getAssetId()));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = sky->getName();
                LL_DEBUGS("WindlightCaps") << "Current Sky is " << preset_name << LL_ENDL;
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                mWLPresetsCombo->selectByValue(preset_name);
            }
#endif
        }
        // Water is not part of legacy day so we need to hunt around
        LLSettingsWater::ptr_t water = LLEnvironment::instance().getEnvironmentFixedWater(LLEnvironment::ENV_LOCAL);
        if (water)
        {
            LLUUID asset_id = water->getAssetId();
            if (asset_id.notNull())
            {
                mWaterPresetsCombo->selectByValue(LLSD(asset_id));
            }
#ifdef OPENSIM
            else if (LLGridManager::getInstance()->isInOpenSim())
            {
                auto preset_name = water->getName();
                if (preset_name == "_default_")
                {
                    preset_name = "Default";
                }
                mWaterPresetsCombo->selectByValue(preset_name);
            }
#endif //OPENSIM
        }
    }
    else
    {
        // LLEnvironment::ENV_REGION:
        // LLEnvironment::ENV_PARCEL:
        mWLPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        mWaterPresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
        mDayCyclePresetsCombo->selectByValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    }

    setDefaultPresetsEnabled(false);
}

bool FloaterQuickPrefs::postBuild()
{
    // Phototools additions
    if (getIsPhototools())
    {
        mCtrlUseSSAO = getChild<LLCheckBoxCtrl>("UseSSAO");
        mCtrlUseDoF = getChild<LLCheckBoxCtrl>("UseDepthofField");
        mCtrlShadowDetail = getChild<LLComboBox>("ShadowDetail");

        // Vignette UI controls
        mSpinnerVignetteX = getChild<LLSpinCtrl>("VignetteSpinnerX");
        mSpinnerVignetteY = getChild<LLSpinCtrl>("VignetteSpinnerY");
        mSpinnerVignetteZ = getChild<LLSpinCtrl>("VignetteSpinnerZ");
        mSliderVignetteX = getChild<LLSlider>("VignetteSliderX");
        mSliderVignetteY = getChild<LLSlider>("VignetteSliderY");
        mSliderVignetteZ = getChild<LLSlider>("VignetteSliderZ");

        mSliderRenderShadowSplitExponentY = getChild<LLSlider>("SB_Shd_Clarity");
        mSpinnerRenderShadowSplitExponentY = getChild<LLSpinCtrl>("S_Shd_Clarity");

        mSliderRenderShadowGaussianX = getChild<LLSlider>("SB_Shd_Soften");
        mSliderRenderShadowGaussianY = getChild<LLSlider>("SB_AO_Soften");
        mSpinnerRenderShadowGaussianX = getChild<LLSpinCtrl>("S_Shd_Soften");
        mSpinnerRenderShadowGaussianY = getChild<LLSpinCtrl>("S_AO_Soften");

        mSliderRenderSSAOEffectX = getChild<LLSlider>("SB_Effect");
        mSpinnerRenderSSAOEffectX = getChild<LLSpinCtrl>("S_Effect");

		// <FS:WW> // Animation Speed UI controls 
		mAnimationSpeedSlider = getChild<LLSlider>("animationspeed_slider_name");
		if (mAnimationSpeedSlider)
		{
		    mAnimationSpeedSlider->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAnimationSpeedChanged, this, _1, _2));

		    // **Add this line to initialize slider value at startup:**
		    F32 initialSpeedFactor = LLMotionController::getCurrentTimeFactor(); 
		    mAnimationSpeedSlider->setValue(initialSpeedFactor);             
		}

		mAnimationSpeedSpinner = getChild<LLUICtrl>("animationspeed_spinner_name");
		if (mAnimationSpeedSpinner)
		{
		    mAnimationSpeedSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAnimationSpeedChanged, this, _1, _2));

		    // **Add these lines to initialize spinner value at startup:**
		    F32 initialSpeedFactor = LLMotionController::getCurrentTimeFactor(); 
		    mAnimationSpeedSpinner->setValue(initialSpeedFactor);             
		}

		// **Move refreshSettings() to the VERY END of the if block:**
		refreshSettings(); 
		// </FS:WW>

    }
    else
    {
        mBtnResetDefaults = getChild<LLButton>("Restore_Btn");

        mAvatarZOffsetSlider = getChild<LLSliderCtrl>("HoverHeightSlider");
        mAvatarZOffsetSlider->setMinValue(MIN_HOVER_Z);
        mAvatarZOffsetSlider->setMaxValue(MAX_HOVER_Z);

        mMaxComplexitySlider = getChild<LLSliderCtrl>("IndirectMaxComplexity");
        mMaxComplexityLabel = getChild<LLTextBox>("IndirectMaxComplexityText");
    }

    mWaterPresetsCombo = getChild<LLComboBox>("WaterPresetsCombo");
    mWLPresetsCombo = getChild<LLComboBox>("WLPresetsCombo");
    mDayCyclePresetsCombo = getChild<LLComboBox>("DCPresetsCombo");

    initCallbacks();

    if (gRlvHandler.isEnabled())
    {
        enableWindlightButtons(!gRlvHandler.hasBehaviour(RLV_BHVR_SETENV) && !gRlvHandler.hasBehaviour(RLV_BHVR_SETSPHERE));
    }

    // <FS:Zi> Dynamic quick prefs

    // bail out here if this is a reused Phototools floater
    if (getIsPhototools())
    {
        return LLTransientDockableFloater::postBuild();
    }

    // find the layout_stack to insert the controls into
    mOptionsStack = getChild<LLLayoutStack>("options_stack");

    // get the path to the user defined or default quick preferences settings
    loadSavedSettingsFromFile(getSettingsPath(false));

    // get edit widget pointers
    mControlLabelEdit = getChild<LLLineEditor>("label_edit");
    mControlNameCombo = getChild<LLComboBox>("control_name_combo");
    mControlTypeCombo = getChild<LLComboBox>("control_type_combo_box");
    mControlIntegerCheckbox = getChild<LLCheckBoxCtrl>("control_integer_checkbox");
    mControlMinSpinner = getChild<LLSpinCtrl>("control_min_edit");
    mControlMaxSpinner = getChild<LLSpinCtrl>("control_max_edit");
    mControlIncrementSpinner = getChild<LLSpinCtrl>("control_increment_edit");

    // wire up callbacks for changed values
    mControlLabelEdit->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlNameCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlTypeCombo->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlIntegerCheckbox->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlMinSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlMaxSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));
    mControlIncrementSpinner->setCommitCallback(boost::bind(&FloaterQuickPrefs::onValuesChanged, this));

    // wire up ordering and adding buttons
    getChild<LLButton>("move_up_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveUpClicked, this));
    getChild<LLButton>("move_down_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onMoveDownClicked, this));
    getChild<LLButton>("add_new_button")->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAddNewClicked, this));

    // functor to add debug settings to the editor dropdown
    struct f : public LLControlGroup::ApplyFunctor
    {
        LLComboBox* combo;
        f(LLComboBox* c) : combo(c) {}
        virtual void apply(const std::string& name, LLControlVariable* control)
        {
            // do not add things that are hidden in the debug settings floater
            if (!control->isHiddenFromSettingsEditor())
            {
                // don't add floater positions, sizes or visibility values
                if (name.find("floater_") != 0)
                {
                    (*combo).addSimpleElement(name);
                }
            }
        }
    } func(mControlNameCombo);

    // add global and per account settings to the dropdown
    gSavedSettings.applyToAll(&func);
    gSavedPerAccountSettings.applyToAll(&func);
    mControlNameCombo->sortByName();
    // </FS:Zi>

    updateAvatarZOffsetEditEnabled();
    onRegionChanged();

    return LLTransientDockableFloater::postBuild();
}

void FloaterQuickPrefs::loadSavedSettingsFromFile(const std::string& settings_path)
{
    QuickPrefsXML xml;
    LLXMLNodePtr root;

    if (!LLXMLNode::parseFile(settings_path, root, NULL))
    {
        LL_WARNS() << "Unable to load quick preferences from file: " << settings_path << LL_ENDL;
    }
    else if (!root->hasName("quickprefs"))
    {
        LL_WARNS() << settings_path << " is not a valid quick preferences definition file" << LL_ENDL;
    }
    else
    {
        // Parse the quick preferences settings
        LLXUIParser parser;
        parser.readXUI(root, xml, settings_path);

        if (!xml.validateBlock())
        {
            LL_WARNS() << "Unable to validate quick preferences from file: " << settings_path << LL_ENDL;
        }
        else
        {
            bool save_settings = false;

            // add the elements from the XML file to the internal list of controls
            for (const QuickPrefsXMLEntry& xml_entry : xml.entries)
            {
                // get the label
                std::string label = xml_entry.label;

                if (xml_entry.translation_id.isProvided())
                {
                    // replace label with translated version, if available
                    LLTrans::findString(label, xml_entry.translation_id());
                }

                // Convert old RenderAvatarMaxVisible setting to IndirectMaxNonImpostors
                if (xml_entry.control_name.getValue() != "RenderAvatarMaxVisible")
                {
                    U32 type = xml_entry.control_type;
                    addControl(
                        xml_entry.control_name,
                        label,
                        NULL,
                        (ControlType)type,
                        xml_entry.integer,
                        xml_entry.min_value,
                        xml_entry.max_value,
                        xml_entry.increment
                        );

                    // put it at the bottom of the ordering stack
                    mControlsOrder.push_back(xml_entry.control_name);
                }
                else
                {
                    U32 type = xml_entry.control_type;
                    addControl(
                        "IndirectMaxNonImpostors",
                        label,
                        NULL,
                        (ControlType)type,
                        xml_entry.integer,
                        1,
                        66,
                        1
                        );

                    // put it at the bottom of the ordering stack
                    mControlsOrder.push_back("IndirectMaxNonImpostors");

                    save_settings = true;
                }
            }

            if (save_settings)
            {
                // Saves settings
                onEditModeChanged();
            }
        }
    }
}


bool FloaterQuickPrefs::isValidPreset(const LLSD& preset)
{
    if (preset.isUUID())
    {
        if(!preset.asUUID().isNull()){ return true;}
    }
    else if (preset.isString())
    {
        if(!preset.asString().empty() &&
            preset.asString() != PRESET_NAME_REGION_DEFAULT &&
            preset.asString() != PRESET_NAME_DAY_CYCLE &&
            preset.asString() != PRESET_NAME_NONE)
        {
            return true;
        }
    }
    return false;
}

void FloaterQuickPrefs::stepComboBox(LLComboBox* ctrl, bool forward)
{
    S32 increment = (forward ? 1 : -1);
    S32 lastitem = ctrl->getItemCount() - 1;
    S32 curid = ctrl->getCurrentIndex();
    S32 startid = curid;

    do
    {
        curid += increment;
        if (curid < 0)
        {
            curid = lastitem;
        }
        else if (curid > lastitem)
        {
            curid = 0;
        }
        ctrl->setCurrentByIndex(curid);
    }
    while (!isValidPreset(ctrl->getSelectedValue()) && curid != startid);
}

void FloaterQuickPrefs::selectSkyPreset(const LLSD& preset)
{
// Opensim continued W/L support
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsSky::ptr_t legacy_sky = nullptr;
        LLSD messages;

        legacy_sky = LLEnvironment::createSkyFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "skies", preset.asString() + ".xml"), messages);

        if (legacy_sky)
        {
            // Need to preserve current sky manually in this case in contrast to asset-based settings
            LLSettingsWater::ptr_t current_water = LLEnvironment::instance().getCurrentWater();
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, legacy_sky, current_water);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // note the else here bridges the endif
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

void FloaterQuickPrefs::selectWaterPreset(const LLSD& preset)
{
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsWater::ptr_t legacy_water = nullptr;
        LLSD messages;
        legacy_water = LLEnvironment::createWaterFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "water", preset.asString() + ".xml"), messages);
        if (legacy_water)
        {
            // Need to preserve current sky manually in this case in contrast to asset-based settings
            LLSettingsSky::ptr_t current_sky = LLEnvironment::instance().getCurrentSky();
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, current_sky, legacy_water);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // beware the trailing else here.
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

void FloaterQuickPrefs::selectDayCyclePreset(const LLSD& preset)
{
#ifdef OPENSIM
    if(!preset.isUUID() && LLGridManager::getInstance()->isInOpenSim())
    {
        LLSettingsDay::ptr_t legacyday = nullptr;
        LLSD messages;
        legacyday = LLEnvironment::createDayCycleFromLegacyPreset(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", "days", preset.asString() + ".xml"), messages);
        if (legacyday)
        {
            LLEnvironment::instance().setEnvironment(LLEnvironment::ENV_LOCAL, legacyday);
            LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
            LLEnvironment::instance().updateEnvironment(static_cast<LLSettingsBase::Seconds>(gSavedSettings.getF32("FSEnvironmentManualTransitionTime")));
        }
        else
        {
            LL_WARNS() << "Legacy windlight conversion failed for " << preset << " existing env unchanged." << LL_ENDL;
            return;
        }
    }
    else // beware trailing else that bridges the endif
#endif
    {
        LLEnvironment::instance().setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
        LLEnvironment::instance().setManualEnvironment(LLEnvironment::ENV_LOCAL, preset.asUUID());
    }
}

void FloaterQuickPrefs::onChangeWaterPreset()
{
    if (!isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWaterPresetsCombo, true);
    }

    if (isValidPreset(mWaterPresetsCombo->getSelectedValue()))
    {
        selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void FloaterQuickPrefs::onChangeSkyPreset()
{
    if (!isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        stepComboBox(mWLPresetsCombo, true);
    }

    if (isValidPreset(mWLPresetsCombo->getSelectedValue()))
    {
        selectSkyPreset(mWLPresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void FloaterQuickPrefs::onChangeDayCyclePreset()
{
    if (!isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        stepComboBox(mDayCyclePresetsCombo, true);
    }

    if (isValidPreset(mDayCyclePresetsCombo->getSelectedValue()))
    {
        selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
    }
    else
    {
        LLNotificationsUtil::add("NoValidEnvSettingFound");
    }
}

void FloaterQuickPrefs::onClickWaterPrev()
{
    stepComboBox(mWaterPresetsCombo, false);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickWaterNext()
{
    stepComboBox(mWaterPresetsCombo, true);
    selectWaterPreset(mWaterPresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickSkyPrev()
{
    stepComboBox(mWLPresetsCombo, false);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickSkyNext()
{
    stepComboBox(mWLPresetsCombo, true);
    selectSkyPreset(mWLPresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickDayCyclePrev()
{
    stepComboBox(mDayCyclePresetsCombo, false);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickDayCycleNext()
{
    stepComboBox(mDayCyclePresetsCombo, true);
    selectDayCyclePreset(mDayCyclePresetsCombo->getSelectedValue());
}

void FloaterQuickPrefs::onClickResetToRegionDefault()
{
    mWLPresetsCombo->setValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    mWaterPresetsCombo->setValue(LLSD(PRESET_NAME_REGION_DEFAULT));
    LLEnvironment::instance().setSharedEnvironment();
}

void FloaterQuickPrefs::setSelectedSky(const std::string& preset_name)
{
    mWLPresetsCombo->setValue(LLSD(preset_name));
}

void FloaterQuickPrefs::setSelectedWater(const std::string& preset_name)
{
    mWaterPresetsCombo->setValue(LLSD(preset_name));
}

void FloaterQuickPrefs::setSelectedDayCycle(const std::string& preset_name)
{
    mDayCyclePresetsCombo->setValue(LLSD(preset_name));
    mWLPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
    mWaterPresetsCombo->setValue(LLSD(PRESET_NAME_DAY_CYCLE));
}

// Phototools additions
void FloaterQuickPrefs::refreshSettings()
{
    LLTextBox* sky_label = getChild<LLTextBox>("T_Sky_Detail");
    LLSlider* sky_slider = getChild<LLSlider>("SB_Sky_Detail");
    LLSpinCtrl* sky_spinner = getChild<LLSpinCtrl>("S_Sky_Detail");
    LLButton* sky_default_button = getChild<LLButton>("Reset_Sky_Detail");

    sky_label->setEnabled(true);
    sky_slider->setEnabled(true);
    sky_spinner->setEnabled(true);
    sky_default_button->setEnabled(true);

    bool enabled = LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO");

    mCtrlUseSSAO->setEnabled(enabled);
    mCtrlUseDoF->setEnabled(enabled);

    enabled = enabled && LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail");

    mCtrlShadowDetail->setEnabled(enabled);

    // disabled deferred SSAO
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferredSSAO"))
    {
        mCtrlUseSSAO->setEnabled(false);
        mCtrlUseSSAO->setValue(false);
    }

    // disabled deferred shadows
    if (!LLFeatureManager::getInstance()->isFeatureAvailable("RenderShadowDetail"))
    {
        mCtrlShadowDetail->setEnabled(false);
        mCtrlShadowDetail->setValue(0);
    }

    // <FS:CR> FIRE-9630 - Vignette UI controls
    if (getIsPhototools())
    {
        LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
        mSpinnerVignetteX->setValue(vignette.mV[VX]);
        mSpinnerVignetteY->setValue(vignette.mV[VY]);
        mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
        mSliderVignetteX->setValue(vignette.mV[VX]);
        mSliderVignetteY->setValue(vignette.mV[VY]);
        mSliderVignetteZ->setValue(vignette.mV[VZ]);

        LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
        mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
        mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);

        LLVector3 renderRenderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
        mSpinnerRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);
        mSpinnerRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);
        mSliderRenderShadowGaussianX->setValue(renderRenderShadowGaussian.mV[VX]);
        mSliderRenderShadowGaussianY->setValue(renderRenderShadowGaussian.mV[VY]);

        LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
        mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
        mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    }
    // </FS:CR>
}

void FloaterQuickPrefs::updateRlvRestrictions(ERlvBehaviour behavior, ERlvParamType type)
{
    if (behavior == RLV_BHVR_SETENV || behavior == RLV_BHVR_SETSPHERE)
    {
        enableWindlightButtons(type != RLV_TYPE_ADD);
    }
}

void FloaterQuickPrefs::enableWindlightButtons(bool enable)
{
    childSetEnabled("WLPresetsCombo", enable);
    childSetEnabled("WLPrevPreset", enable);
    childSetEnabled("WLNextPreset", enable);
    childSetEnabled("WaterPresetsCombo", enable);
    childSetEnabled("WWPrevPreset", enable);
    childSetEnabled("WWNextPreset", enable);
    childSetEnabled("ResetToRegionDefault", enable);
    childSetEnabled("DCPresetsCombo", enable);
    childSetEnabled("DCPrevPreset", enable);
    childSetEnabled("DCNextPreset", enable);
    childSetEnabled("btn_personal_lighting", enable);

    if (getIsPhototools())
    {
        childSetEnabled("Sunrise", enable);
        childSetEnabled("Noon", enable);
        childSetEnabled("Sunset", enable);
        childSetEnabled("Midnight", enable);
        childSetEnabled("Revert to Region Default", enable);
        childSetEnabled("new_sky_preset", enable);
        childSetEnabled("edit_sky_preset", enable);
        childSetEnabled("new_water_preset", enable);
        childSetEnabled("edit_water_preset", enable);
        childSetEnabled("PauseClouds", enable);
    }
}

// <FS:Zi> Dynamic quick prefs
std::string FloaterQuickPrefs::getSettingsPath(bool save_mode)
{
    // get the settings file name
    std::string settings_file = LLAppViewer::instance()->getSettingsFilename("Default", "QuickPreferences");
    // expand to user defined path
    std::string settings_path = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, settings_file);

    // if not in save mode, and the file was not found, use the default path
    if (!save_mode && !LLFile::isfile(settings_path))
    {
        settings_path = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, settings_file);
    }
    return settings_path;
}

void FloaterQuickPrefs::updateControl(const std::string& controlName, ControlEntry& entry)
{
    // rename the panel to contain the control's name, for identification later
    entry.panel->setName(controlName);

    // build a list of all possible control widget types
    std::map<ControlType, std::string> typeMap;
    std::map<ControlType, std::string>::iterator it;

    typeMap[ControlTypeCheckbox]    = "option_checkbox_control";
    typeMap[ControlTypeText]        = "option_text_control";
    typeMap[ControlTypeSpinner]     = "option_spinner_control";
    typeMap[ControlTypeSlider]      = "option_slider_control";
    typeMap[ControlTypeRadio]       = "option_radio_control";
    typeMap[ControlTypeColor3]      = "option_color3_control";
    typeMap[ControlTypeColor4]      = "option_color4_control";

    // hide all widget types except for the one the user wants
    LLUICtrl* widget{ nullptr };
    for (it = typeMap.begin(); it != typeMap.end(); ++it)
    {
        if (entry.type != it->first)
        {
            widget = entry.panel->getChild<LLUICtrl>(it->second);

            if (widget)
            {
                // dummy to disable old control
                widget->setControlName("QuickPrefsEditMode");
                widget->setVisible(false);
                widget->setEnabled(false);
            }
        }
    }

    // get the widget type the user wanted from the panel
    widget = entry.panel->getChild<LLUICtrl>(typeMap[entry.type]);

    // use 3 decimal places by default
    S32 decimals = 3;

    // save pointer to the widget in our internal list
    entry.widget = widget;

    // add the settings control to the widget and enable/show it
    widget->setControlName(controlName);
    widget->setVisible(true);
    widget->setEnabled(true);

    // if no increment is given, try to guess a good number
    if (entry.increment == 0.0f)
    {
        // finer grained for sliders
        if (entry.type == ControlTypeSlider)
        {
            entry.increment = (entry.max_value - entry.min_value) / 100.0f;
        }
        // a little less for spinners
        else if (entry.type == ControlTypeSpinner)
        {
            entry.increment = (entry.max_value - entry.min_value) / 20.0f;
        }
    }

    // if it's an integer entry, round the numbers
    if (entry.integer)
    {
        entry.min_value = (F32)ll_round(entry.min_value);
        entry.max_value = (F32)ll_round(entry.max_value);

        // recalculate increment
        entry.increment = (F32)ll_round(entry.increment);
        if (entry.increment == 0.f)
        {
            entry.increment = 1.f;
        }

        // no decimal places for integers
        decimals = 0;
    }

    // set up values for special case control widget types
    LLUICtrl* alpha_widget = entry.panel->getChild<LLUICtrl>("option_color_alpha_control");
    alpha_widget->setVisible(false);

    // sadly, using LLF32UICtrl does not work properly, so we have to use a branch
    // for each floating point type
    if (entry.type == ControlTypeSpinner)
    {
        LLSpinCtrl* spinner = (LLSpinCtrl*)widget;
        spinner->setPrecision(decimals);
        spinner->setMinValue(entry.min_value);
        spinner->setMaxValue(entry.max_value);
        spinner->setIncrement(entry.increment);
    }
    else if (entry.type == ControlTypeSlider)
    {
        LLSliderCtrl* slider = (LLSliderCtrl*)widget;
        slider->setPrecision(decimals);
        slider->setMinValue(entry.min_value);
        slider->setMaxValue(entry.max_value);
        slider->setIncrement(entry.increment);
    }
    else if (entry.type == ControlTypeColor4)
    {
        LLColorSwatchCtrl* color_widget = (LLColorSwatchCtrl*)widget;
        alpha_widget->setVisible(true);
        alpha_widget->setValue(color_widget->get().mV[VALPHA]);
    }

    // reuse a previously created text label if possible
    LLTextBox* label_textbox = entry.label_textbox;
    // if the text label is not known yet, this is a brand new control panel
    if (!label_textbox)
    {
        // otherwise, get the pointer to the new label
        label_textbox = entry.panel->getChild<LLTextBox>("option_label");

        // add double click and single click callbacks on the text label
        label_textbox->setDoubleClickCallback(boost::bind(&FloaterQuickPrefs::onDoubleClickLabel, this, _1, entry.panel));
        label_textbox->setMouseUpCallback(boost::bind(&FloaterQuickPrefs::onClickLabel, this, _1, entry.panel));

        // since this is a new control, wire up the remove button signal, too
        LLButton* remove_button = entry.panel->getChild<LLButton>("remove_button");
        remove_button->setCommitCallback(boost::bind(&FloaterQuickPrefs::onRemoveClicked, this, _1, entry.panel));

        // and the commit signal for the alpha value in a color4 control
        alpha_widget->setCommitCallback(boost::bind(&FloaterQuickPrefs::onAlphaChanged, this, _1, entry.panel->getChild<LLColorSwatchCtrl>("option_color4_control")));

        // save the text label pointer in the internal list
        entry.label_textbox = label_textbox;
    }
    // set the value(visible text) for the text label
    label_textbox->setValue(entry.label + ":");

    // get the named control variable from global or per account settings
    LLControlVariable* var = gSavedSettings.getControl(controlName);
    if (!var)
    {
        var = gSavedPerAccountSettings.getControl(controlName);
    }

    // if we found the control, set up the chosen widget to use it
    if (var)
    {
        widget->setValue(var->getValue());
    }
    else
    {
        LL_WARNS() << "Could not find control variable " << controlName << LL_ENDL;
    }
}

LLUICtrl* FloaterQuickPrefs::addControl(const std::string& controlName, const std::string& controlLabel, LLView* slot, ControlType type, bool integer, F32 min_value, F32 max_value, F32 increment)
{
    // create a new controls panel
    LLLayoutPanel* panel = LLUICtrlFactory::createFromFile<LLLayoutPanel>("panel_quickprefs_item.xml", NULL, LLLayoutStack::child_registry_t::instance());
    if (!panel)
    {
        LL_WARNS() << "could not add panel" << LL_ENDL;
        return NULL;
    }

    // sanity checks
    if (max_value < min_value)
    {
        max_value = min_value;
    }

    // 0.0 will make updateControl calculate the increment itself
    if (increment < 0.0f)
    {
        increment = 0.0f;
    }

    // create a new internal entry for this control
    ControlEntry newControl;
    newControl.panel = panel->getChild<LLPanel>("option_ordering_panel");
    newControl.widget = NULL;
    newControl.label_textbox = NULL;
    newControl.label = controlLabel;
    newControl.type = type;
    newControl.integer = integer;
    newControl.min_value = min_value;
    newControl.max_value = max_value;
    newControl.increment = increment;

    // update the new control
    updateControl(controlName, newControl);

    // add the control to the internal list
    mControlsList[controlName] = newControl;

    // if we have a slot already, reparent our new ordering panel and delete the old layout_panel
    if (slot)
    {
        // add the ordering panel to the slot
        slot->addChild(newControl.panel);
        // make sure the panel moves to the top left corner
        newControl.panel->setOrigin(0, 0);
        // resize it to make it fill the slot
        newControl.panel->reshape(slot->getRect().getWidth(), slot->getRect().getHeight());
        // remove the old layout panel from memory
        delete panel;
    }
    // otherwise create a new slot
    else
    {
        // add a new layout_panel to the stack
        mOptionsStack->addPanel(panel, LLLayoutStack::NO_ANIMATE);
        // add the panel to the list of ordering slots
        mOrderingSlots.push_back(panel);
        // make the floater fit the newly added control panel
        reshape(getRect().getWidth(), getRect().getHeight() + panel->getRect().getHeight());
        // show the panel
        panel->setVisible(true);
    }

    // hide the border
    newControl.panel->setBorderVisible(false);

    return newControl.widget;
}

void FloaterQuickPrefs::removeControl(const std::string& controlName, bool remove_slot)
{
    // find the control panel to remove
    const control_list_t::iterator it = mControlsList.find(controlName);
    if (it == mControlsList.end())
    {
        LL_WARNS() << "Couldn't find control entry " << controlName << LL_ENDL;
        return;
    }

    // get a pointer to the panel to remove
    LLPanel* panel = it->second.panel;
    // remember the panel's height because it will be deleted by removeChild() later
    S32 height = panel->getRect().getHeight();

    // remove the panel from the internal list
    mControlsList.erase(it);

    // get a pointer to the layout slot used
    LLLayoutPanel* slot = (LLLayoutPanel*)panel->getParent();
    // remove the panel from the slot
    slot->removeChild(panel);
    // clear the panel from memory
    delete panel;

    // remove the layout_panel if desired
    if (remove_slot)
    {
        // remove the slot from our list
        mOrderingSlots.remove(slot);
        // and delete it from the user interface stack
        mOptionsStack->removeChild(slot);

        // make the floater shrink to its new size
        reshape(getRect().getWidth(), getRect().getHeight() - height);
    }
}

void FloaterQuickPrefs::selectControl(std::string controlName)
{
    // remove previously selected marker, if any
    if (!mSelectedControl.empty() && hasControl(mSelectedControl))
    {
        mControlsList[mSelectedControl].panel->setBorderVisible(false);
    }

    // save the currently selected name in a volatile settings control to
    // enable/disable the editor widgets
    mSelectedControl = controlName;
    gSavedSettings.setString("QuickPrefsSelectedControl", controlName);

    if (mSelectedControl.size() && !hasControl(mSelectedControl))
    {
        mSelectedControl = "";
        return;
    }

    // if we are not in edit mode, we can stop here
    if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
    {
        return;
    }

    // select the topmost entry in the name dropdown, in case we don't find the name
    mControlNameCombo->selectNthItem(0);

    // assume we don't need the min/max/increment/integer widgets by default
    bool enable_floating_point = false;

    // if actually a selection is present, set up the editor widgets
    if (!mSelectedControl.empty())
    {
        // draw the new selection border
        mControlsList[mSelectedControl].panel->setBorderVisible(true);

        // set up editor values
        mControlLabelEdit->setValue(LLSD(mControlsList[mSelectedControl].label));
        mControlNameCombo->setValue(LLSD(mSelectedControl));
        mControlTypeCombo->setValue(mControlsList[mSelectedControl].type);
        mControlIntegerCheckbox->setValue(LLSD(mControlsList[mSelectedControl].integer));
        mControlMinSpinner->setValue(LLSD(mControlsList[mSelectedControl].min_value));
        mControlMaxSpinner->setValue(LLSD(mControlsList[mSelectedControl].max_value));
        mControlIncrementSpinner->setValue(LLSD(mControlsList[mSelectedControl].increment));

        // special handling to enable min/max/integer/increment widgets
        switch (mControlsList[mSelectedControl].type)
        {
            // enable floating point widgets for these types
            case ControlTypeSpinner:    // fall through
            case ControlTypeSlider:     // fall through
            {
                enable_floating_point = true;

                // assume we have floating point widgets
                mControlIncrementSpinner->setIncrement(0.1f);
                // use 3 decimal places by default
                S32 decimals = 3;
                // unless we have an integer control
                if (mControlsList[mSelectedControl].integer)
                {
                    decimals = 0;
                    mControlIncrementSpinner->setIncrement(1.0f);
                }
                // set up floating point widgets
                mControlMinSpinner->setPrecision(decimals);
                mControlMaxSpinner->setPrecision(decimals);
                mControlIncrementSpinner->setPrecision(decimals);
                break;
            }
            // the rest will not need them
            default:
            {
            }
        }
    }

    // enable/disable floating point widgets
    mControlMinSpinner->setEnabled(enable_floating_point);
    mControlMaxSpinner->setEnabled(enable_floating_point);
    mControlIntegerCheckbox->setEnabled(enable_floating_point);
    mControlIncrementSpinner->setEnabled(enable_floating_point);
}

void FloaterQuickPrefs::onClickLabel(LLUICtrl* ctrl, LLPanel* panel)
{
    // don't do anything when we are not in edit mode
    if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
    {
        return;
    }

    // select the clicked control, identified by its name
    selectControl(panel->getName());
}

void FloaterQuickPrefs::onDoubleClickLabel(LLUICtrl* ctrl, LLPanel* panel)
{
    // toggle edit mode
    bool edit_mode = !gSavedSettings.getBOOL("QuickPrefsEditMode");
    gSavedSettings.setBOOL("QuickPrefsEditMode", edit_mode);

    // select the double clicked control if we toggled edit on
    if (edit_mode)
    {
        selectControl(panel->getName());
    }
}

void FloaterQuickPrefs::onEditModeChanged()
{
    // if edit mode was enabled, stop here
    if (gSavedSettings.getBOOL("QuickPrefsEditMode"))
    {
        return;
    }

    // deselect the current control
    selectControl("");

    QuickPrefsXML xml;
    std::string settings_path = getSettingsPath(true);

    // loop through the list of controls, in the displayed order
    std::list<std::string>::iterator it;
    for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
    {
        const ControlEntry& entry = mControlsList[*it];
        QuickPrefsXMLEntry xml_entry;

        // add control values to the XML entry
        xml_entry.control_name = *it;
        xml_entry.label = entry.label;
        xml_entry.control_type = (U32)entry.type;
        xml_entry.integer = entry.integer;
        xml_entry.min_value = entry.min_value;
        xml_entry.max_value = entry.max_value;
        xml_entry.increment = entry.increment;

        // add the XML entry to the overall XML container
        xml.entries.add(xml_entry);
    }

    // Serialize the parameter tree
    LLXMLNodePtr output_node = new LLXMLNode("quickprefs", false);
    LLXUIParser parser;
    parser.writeXUI(output_node, xml);

    // Write the resulting XML to file
    if (!output_node->isNull())
    {
        LLFILE* fp = LLFile::fopen(settings_path, "w");
        if (fp)
        {
            LLXMLNode::writeHeaderToFile(fp);
            output_node->writeToFile(fp);
            fclose(fp);
        }
    }
}

void FloaterQuickPrefs::onValuesChanged()
{
    // safety, do nothing if we are not in edit mode
    if (!gSavedSettings.getBOOL("QuickPrefsEditMode"))
    {
        return;
    }

    // don't crash when we try to update values without having a control selected
    if (mSelectedControl.empty())
    {
        return;
    }

    // remember the current and possibly new control names
    std::string old_control_name = mSelectedControl;
    std::string new_control_name = mControlNameCombo->getValue().asString();

    // if we changed the control's variable, rebuild the user interface
    if (!new_control_name.empty() && old_control_name != new_control_name)
    {
        if (mControlsList.find(new_control_name) != mControlsList.end())
        {
            LL_WARNS() << "Selected control has already been added" << LL_ENDL;
            LLNotificationsUtil::add("QuickPrefsDuplicateControl");
            return;
        }

        // remember the old control parameters so we can restore them later
        ControlEntry old_parameters = mControlsList[mSelectedControl];
        // disable selection so the border doesn't cause a crash
        selectControl("");
        // rename the old ordering entry
        std::list<std::string>::iterator it;
        for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
        {
            if (*it == old_control_name)
            {
                *it = new_control_name;
                break;
            }
        }

        // remember the old slot
        LLView* slot = old_parameters.panel->getParent();
        // remove the old control name from the internal list but keep the slot available
        removeControl(old_control_name, false);
        // add new control with the old slot
        addControl(new_control_name, new_control_name, slot);
        // grab the new values and make the selection border go to the right panel
        selectControl(new_control_name);
        // restore the old UI settings
        mControlsList[mSelectedControl].label = old_parameters.label;
        // find the control variable in global or per account settings
        LLControlVariable* var = gSavedSettings.getControl(mSelectedControl);
        if (!var)
        {
            var = gSavedPerAccountSettings.getControl(mSelectedControl);
        }

        if (var && hasControl(mSelectedControl))
        {
            // choose sane defaults for floating point controls, so the control value won't be destroyed
            // start with these
            F32 min_value = 0.0f;
            F32 max_value = 1.0f;
            F32 value = (F32)var->getValue().asReal();

            // if the value was negative and smaller than the current minimum
            if (value < 0.0f)
            {
                // make the minimum even smaller
                min_value = value * 2.0f;
            }
            // if the value is above zero, set max to double of the current value
            else if (value > 0.0f)
            {
                max_value = value * 2.0f;
            }

            // do a best guess on variable types and control widgets
            ControlType type;
            switch (var->type())
            {
                // Boolean gets the full set
                case TYPE_BOOLEAN:
                {
                    // increment will be calculated below
                    min_value = 0.0f;
                    max_value = 1.0f;
                    type = ControlTypeRadio;
                    break;
                }
                // LLColor3/4 are just colors
                case TYPE_COL3:
                {
                    type = ControlTypeColor3;
                    break;
                }
                case TYPE_COL4:
                {
                    type = ControlTypeColor4;
                    break;
                }
                // U32 can never be negative
                case TYPE_U32:
                {
                    min_value = 0.0f;
                }
                // Fallthrough, S32 and U32 are integer values
                case TYPE_S32:
                // Fallthrough, S32, U32 and F32 should use sliders
                case TYPE_F32:
                {
                    type = ControlTypeSlider;
                    break;
                }
                // Everything else gets a text widget for now
                default:
                {
                    type=ControlTypeText;
                }
            }

            // choose a sane increment
            F32 increment = 0.1f;
            if (mControlsList[mSelectedControl].type == ControlTypeSlider)
            {
                // fine grained control for sliders
                increment = (max_value - min_value) / 100.0f;
            }
            else if (mControlsList[mSelectedControl].type == ControlTypeSpinner)
            {
                // not as fine grained for spinners
                increment = (max_value - min_value) / 20.0f;
            }

            // don't let values go too small
            if (increment < 0.1f)
            {
                increment = 0.1f;
            }

            // save calculated values to the edit widgets
            mControlsList[mSelectedControl].min_value = min_value;
            mControlsList[mSelectedControl].max_value = max_value;
            mControlsList[mSelectedControl].increment = increment;
            mControlsList[mSelectedControl].type = type; // old_parameters.type;
            mControlsList[mSelectedControl].widget->setValue(var->getValue());
        }

        // update our new control
        updateControl(mSelectedControl, mControlsList[mSelectedControl]);
    }
    // the control's setting variable is still the same, so just update the values
    else if (hasControl(mSelectedControl))
    {
        mControlsList[mSelectedControl].label = mControlLabelEdit->getValue().asString();
        mControlsList[mSelectedControl].type = (ControlType)mControlTypeCombo->getValue().asInteger();
        mControlsList[mSelectedControl].integer = mControlIntegerCheckbox->getValue().asBoolean();
        mControlsList[mSelectedControl].min_value = (F32)mControlMinSpinner->getValue().asReal();
        mControlsList[mSelectedControl].max_value = (F32)mControlMaxSpinner->getValue().asReal();
        mControlsList[mSelectedControl].increment = (F32)mControlIncrementSpinner->getValue().asReal();
        // and update the user interface
        updateControl(mSelectedControl, mControlsList[mSelectedControl]);
    }
    // select the control
    selectControl(mSelectedControl);
}

void FloaterQuickPrefs::onAddNewClicked()
{
    // count a number to keep control names unique
    static S32 sCount = 0;
    std::string new_control_name = "NewControl" + llformat("%d", sCount);
    // add the new control to the internal list and user interface
    addControl(new_control_name, new_control_name);
    // put it at the bottom of the ordering stack
    mControlsOrder.push_back(new_control_name);
    sCount++;
    // select the newly created control
    selectControl(new_control_name);
}

void FloaterQuickPrefs::onRemoveClicked(LLUICtrl* ctrl, LLPanel* panel)
{
    // deselect the current entry
    selectControl("");
    // first remove the control from the ordering list
    mControlsOrder.remove(panel->getName());
    // then remove it from the internal list and from memory
    removeControl(panel->getName());
    // reinstate focus in case we lost it
    setFocus(true);
}

void FloaterQuickPrefs::onAlphaChanged(LLUICtrl* ctrl, LLColorSwatchCtrl* color_swatch)
{
    // get the current color
    LLColor4 color = color_swatch->get();
    // replace the alpha value of the color with the value in the alpha spinner
    color.setAlpha((F32)ctrl->getValue().asReal());
    // save the color back into the color swatch
    color_swatch->getControlVariable()->setValue(color.getValue());
}

void FloaterQuickPrefs::swapControls(const std::string& control1, const std::string& control2)
{
    // get the control entries of both controls
    ControlEntry temp_entry_1 = mControlsList[control1];
    ControlEntry temp_entry_2 = mControlsList[control2];

    // find the respective ordering slots
    LLView* temp_slot_1 = temp_entry_1.panel->getParent();
    LLView* temp_slot_2 = temp_entry_2.panel->getParent();

    // swap the controls around
    temp_slot_1->addChild(temp_entry_2.panel);
    temp_slot_2->addChild(temp_entry_1.panel);
}

void FloaterQuickPrefs::onMoveUpClicked()
{
    // find the control in the ordering list
    std::list<std::string>::iterator it;
    for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
    {
        if (*it == mSelectedControl)
        {
            // if it's already on top of the list, do nothing
            if (it == mControlsOrder.begin())
            {
                return;
            }

            // get the iterator of the previous item
            std::list<std::string>::iterator previous = it;
            --previous;

            // copy the previous item to the one we want to move
            *it = *previous;
            // copy the moving item to previous
            *previous = mSelectedControl;
            // update the user interface
            swapControls(mSelectedControl, *it);
            return;
        }
    }
    return;
}

void FloaterQuickPrefs::onMoveDownClicked()
{
    // find the control in the ordering list
    std::list<std::string>::iterator it;
    for (it = mControlsOrder.begin(); it != mControlsOrder.end(); ++it)
    {
        if (*it == mSelectedControl)
        {
            // if it's already at the end of the list, do nothing
            if (*it == mControlsOrder.back())
            {
                return;
            }

            // get the iterator of the next item
            std::list<std::string>::iterator next = it;
            ++next;

            // copy the next item to the one we want to move
            *it = *next;
            // copy the moving item to next
            *next = mSelectedControl;
            // update the user interface
            swapControls(mSelectedControl, *it);
            return;
        }
    }
    return;
}

void FloaterQuickPrefs::onClose(bool app_quitting)
{
    // bail out here if this is a reused Phototools floater
    if (getIsPhototools())
    {
        return;
    }

    // close edit mode and save settings
    gSavedSettings.setBOOL("QuickPrefsEditMode", false);
}

// <FS:WW>
void FloaterQuickPrefs::onAnimationSpeedChanged(LLUICtrl* control, const LLSD& data)
{
    F32 newSpeedFactor = 1.0f; // Default value in case of error

    if (control == mAnimationSpeedSlider)
    {
        newSpeedFactor = mAnimationSpeedSlider->getValueF32();
		// **Add this line to update the spinner when slider changes:**
        mAnimationSpeedSpinner->setValue(newSpeedFactor); // **<-- Update spinner value from slider**
    }
    else if (control == mAnimationSpeedSpinner)
    {
        newSpeedFactor = (F32)mAnimationSpeedSpinner->getValue().asReal();
		// **Optionally, add this line to update the slider when spinner changes (if desired - see note below):**
        mAnimationSpeedSlider->setValue(newSpeedFactor); // Update slider value from spinner (optional - see note)
    }

    // Clamp the speed factor to reasonable limits (optional, adjust as needed)
    newSpeedFactor = llclamp(newSpeedFactor, 0.0f, 100.0f); // Example: 0% to 1000% speed

    // 1. Update the preference in settings.xml
    gSavedSettings.setF32("FSAnimationTimeFactor", newSpeedFactor);

    for (LLCharacter* character : LLCharacter::sInstances)
    {
        character->setAnimTimeFactor(newSpeedFactor);
    }

    // 3. Apply to running animations
    // set_all_animation_time_factors(newSpeedFactor);

    // Optionally update the UI elements to reflect the clamped value if clamping was done.
    //if (control == mAnimationSpeedSlider)
    //{
    //    mAnimationSpeedSlider->setValue(newSpeedFactor);
    //}
    //else if (control == mAnimationSpeedSpinner)
    //{
    //    mAnimationSpeedSpinner->setValue(newSpeedFactor);
    //}
}
// </FS:WW>

// <FS:WW>
void FloaterQuickPrefs::onClickResetAnimationSpeed(LLUICtrl* control, const LLSD& data)
{
    F32 defaultSpeedFactor = 1.0f; // Normal speed is 1.0

    // 1. Update the preference in settings.xml to the default value (1.0)
    gSavedSettings.setF32("FSAnimationTimeFactor", defaultSpeedFactor);
    //gSavedSettings.saveToFile(); // Or gSavedSettings.save(); if that's what works for you

    // 2. Update LLMotionController::sCurrentTimeFactor (or use character loop - choose ONE method consistently)
    // LLMotionController::sCurrentTimeFactor = defaultSpeedFactor; // Option 1: Set global time factor
    for (LLCharacter* character : LLCharacter::sInstances) // Option 2: Update each character directly (Let's use this for consistency)
    {
        character->setAnimTimeFactor(defaultSpeedFactor);
    }

    // 3. Update the UI controls to reflect the reset value (1.0)
    mAnimationSpeedSlider->setValue(defaultSpeedFactor);
    mAnimationSpeedSpinner->setValue(defaultSpeedFactor);
}
// </FS:WW>

// </FS:Zi>

// <FS:CR> FIRE-9630 - Vignette UI callbacks
void FloaterQuickPrefs::onChangeVignetteX()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VX] = mSliderVignetteX->getValueF32();
    mSpinnerVignetteX->setValue(vignette.mV[VX]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteY()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VY] = mSliderVignetteY->getValueF32();
    mSpinnerVignetteY->setValue(vignette.mV[VY]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteZ()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VZ] = mSliderVignetteZ->getValueF32();
    mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerX()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VX] = mSpinnerVignetteX->getValueF32();
    mSliderVignetteX->setValue(vignette.mV[VX]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerY()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VY] = mSpinnerVignetteY->getValueF32();
    mSliderVignetteY->setValue(vignette.mV[VY]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onChangeVignetteSpinnerZ()
{
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VZ] = mSpinnerVignetteZ->getValueF32();
    mSliderVignetteZ->setValue(vignette.mV[VZ]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteX()
{
    LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VX] = vignette_default.mV[VX];
    mSliderVignetteX->setValue(vignette.mV[VX]);
    mSpinnerVignetteX->setValue(vignette.mV[VX]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteY()
{
    LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VY] = vignette_default.mV[VY];
    mSliderVignetteY->setValue(vignette.mV[VY]);
    mSpinnerVignetteY->setValue(vignette.mV[VY]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}

void FloaterQuickPrefs::onClickResetVignetteZ()
{
    LLVector3 vignette_default = LLVector3(gSavedSettings.getControl("FSRenderVignette")->getDefault());
    LLVector3 vignette = gSavedSettings.getVector3("FSRenderVignette");
    vignette.mV[VZ] = vignette_default.mV[VZ];
    mSliderVignetteZ->setValue(vignette.mV[VZ]);
    mSpinnerVignetteZ->setValue(vignette.mV[VZ]);
    gSavedSettings.setVector3("FSRenderVignette", vignette);
}
// </FS:CR> FIRE-9630 - Vignette UI callbacks


void FloaterQuickPrefs::onChangeRenderShadowSplitExponentSlider()
{
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = mSliderRenderShadowSplitExponentY->getValueF32();
    mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onChangeRenderShadowSplitExponentSpinner()
{
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = mSpinnerRenderShadowSplitExponentY->getValueF32();
    mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onClickResetRenderShadowSplitExponentY()
{
    LLVector3 renderShadowSplitExponentDefault = LLVector3(gSavedSettings.getControl("RenderShadowSplitExponent")->getDefault());
    LLVector3 renderShadowSplitExponent = gSavedSettings.getVector3("RenderShadowSplitExponent");
    renderShadowSplitExponent.mV[VY] = renderShadowSplitExponentDefault.mV[VY];
    mSpinnerRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    mSliderRenderShadowSplitExponentY->setValue(renderShadowSplitExponent.mV[VY]);
    gSavedSettings.setVector3("RenderShadowSplitExponent", renderShadowSplitExponent);
}

void FloaterQuickPrefs::onChangeRenderShadowGaussianSlider()
{
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = mSliderRenderShadowGaussianX->getValueF32();
    renderShadowGaussian.mV[VY] = mSliderRenderShadowGaussianY->getValueF32();
    mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onChangeRenderShadowGaussianSpinner()
{
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = mSpinnerRenderShadowGaussianX->getValueF32();
    renderShadowGaussian.mV[VY] = mSpinnerRenderShadowGaussianY->getValueF32();
    mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onClickResetRenderShadowGaussianX()
{
    LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VX] = renderShadowGaussianDefault.mV[VX];
    mSpinnerRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    mSliderRenderShadowGaussianX->setValue(renderShadowGaussian.mV[VX]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onClickResetRenderShadowGaussianY()
{
    LLVector3 renderShadowGaussianDefault = LLVector3(gSavedSettings.getControl("RenderShadowGaussian")->getDefault());
    LLVector3 renderShadowGaussian = gSavedSettings.getVector3("RenderShadowGaussian");
    renderShadowGaussian.mV[VY] = renderShadowGaussianDefault.mV[VY];
    mSpinnerRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    mSliderRenderShadowGaussianY->setValue(renderShadowGaussian.mV[VY]);
    gSavedSettings.setVector3("RenderShadowGaussian", renderShadowGaussian);
}

void FloaterQuickPrefs::onChangeRenderSSAOEffectSlider()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = mSliderRenderSSAOEffectX->getValueF32();
    mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void FloaterQuickPrefs::onChangeRenderSSAOEffectSpinner()
{
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = mSpinnerRenderSSAOEffectX->getValueF32();
    mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void FloaterQuickPrefs::onClickResetRenderSSAOEffectX()
{
    LLVector3 renderSSAOEffectDefault = LLVector3(gSavedSettings.getControl("RenderSSAOEffect")->getDefault());
    LLVector3 renderSSAOEffect = gSavedSettings.getVector3("RenderSSAOEffect");
    renderSSAOEffect.mV[VX] = renderSSAOEffectDefault.mV[VX];
    mSpinnerRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    mSliderRenderSSAOEffectX->setValue(renderSSAOEffect.mV[VX]);
    gSavedSettings.setVector3("RenderSSAOEffect", renderSSAOEffect);
}

void FloaterQuickPrefs::callbackRestoreDefaults(const LLSD& notification, const LLSD& response)
{
    S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
    if ( option == 0 ) // YES
    {
        selectControl("");
        for (const auto& control : mControlsOrder)
        {
            removeControl(control);
        }
        mControlsOrder.clear();
        std::string settings_file = LLAppViewer::instance()->getSettingsFilename("Default", "QuickPreferences");
        LLFile::remove(gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS, settings_file));
        loadSavedSettingsFromFile(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, settings_file));
        gSavedSettings.setBOOL("QuickPrefsEditMode", false);
    }
    else
    {
        LL_INFOS() << "User cancelled the reset." << LL_ENDL;
    }
}

void FloaterQuickPrefs::onClickRestoreDefaults()
{
    LLNotificationsUtil::add("ConfirmRestoreQuickPrefsDefaults", LLSD(), LLSD(), boost::bind(&FloaterQuickPrefs::callbackRestoreDefaults, this, _1, _2));
}

void FloaterQuickPrefs::dockToToolbarButton()
{
    LLCommandId command_id("quickprefs");
    S32 toolbar_loc = gToolBarView->hasCommand(command_id);

    if (toolbar_loc != LLToolBarEnums::TOOLBAR_NONE && !FSCommon::isLegacySkin())
    {
        LLDockControl::DocAt doc_at = LLDockControl::TOP;
        switch (toolbar_loc)
        {
            case LLToolBarEnums::TOOLBAR_LEFT:
                doc_at = LLDockControl::RIGHT;
                break;

            case LLToolBarEnums::TOOLBAR_RIGHT:
                doc_at = LLDockControl::LEFT;
                break;
        }
        setCanDock(true);
        LLView* anchor_panel = gToolBarView->findChildView("quickprefs");
        setUseTongue(anchor_panel);
        // Garbage collected by std::auto_ptr
        // Do the actions from setDockControl here, but call setDock with pop_on_undock = false
        // or we will translate the floater 12px up on the y-axis
        mDockControl.reset(new LLDockControl(anchor_panel, this, getDockTongue(doc_at), doc_at));
        setDocked(isDocked(), false);
    }
    else
    {
        setUseTongue(false);
        setDocked(false, false);
        setCanDock(false);
        setDockControl(NULL);
    }
}

void FloaterQuickPrefs::onAvatarZOffsetSliderMoved()
{
    F32 value = mAvatarZOffsetSlider->getValueF32();
    LLVector3 offset(0.0f, 0.0f, llclamp(value, MIN_HOVER_Z, MAX_HOVER_Z));
    LL_INFOS("Avatar") << "setting hover from slider moved" << offset[VZ] << LL_ENDL;
    if (gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled())
    {
        if (mAvatarZOffsetSlider->isMouseHeldDown())
        {
            gAgentAvatarp->setHoverOffset(offset, false);
        }
        else
        {
            gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
        }
    }
    else if (!gAgentAvatarp->isUsingServerBakes())
    {
        gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ", value);
    }
}

void FloaterQuickPrefs::onAvatarZOffsetFinalCommit()
{
    F32 value = mAvatarZOffsetSlider->getValueF32();
    LLVector3 offset(0.0f, 0.0f, llclamp(value,MIN_HOVER_Z,MAX_HOVER_Z));
    gSavedPerAccountSettings.setF32("AvatarHoverOffsetZ",value);

    LL_INFOS("Avatar") << "setting hover from slider final commit " << offset[VZ] << LL_ENDL;
}

void FloaterQuickPrefs::updateAvatarZOffsetEditEnabled()
{
    bool enabled = gAgent.getRegion() && gAgent.getRegion()->avatarHoverHeightEnabled();
    if (!enabled && isAgentAvatarValid() && !gAgentAvatarp->isUsingServerBakes())
    {
        enabled = true;
    }
    mAvatarZOffsetSlider->setEnabled(enabled);
    if (enabled)
    {
        syncAvatarZOffsetFromPreferenceSetting();
    }
}

void FloaterQuickPrefs::onRegionChanged()
{
    LLViewerRegion* region = gAgent.getRegion();
    if (region && region->simulatorFeaturesReceived())
    {
        updateAvatarZOffsetEditEnabled();
    }
    else if (region)
    {
        region->setSimulatorFeaturesReceivedCallback(boost::bind(&FloaterQuickPrefs::onSimulatorFeaturesReceived, this, _1));
    }
}

void FloaterQuickPrefs::onSimulatorFeaturesReceived(const LLUUID &region_id)
{
    LLViewerRegion *region = gAgent.getRegion();
    if (region && (region->getRegionID() == region_id))
    {
        updateAvatarZOffsetEditEnabled();
    }
}

void FloaterQuickPrefs::syncAvatarZOffsetFromPreferenceSetting()
{
    F32 value = gSavedPerAccountSettings.getF32("AvatarHoverOffsetZ");
    mAvatarZOffsetSlider->setValue(value, false);
}

void FloaterQuickPrefs::updateMaxNonImpostors(const LLSD& newvalue)
{
    // Called when the IndirectMaxNonImpostors control changes
    // Responsible for fixing the setting RenderAvatarMaxNonImpostors
    U32 value = newvalue.asInteger();

    if (0 == value || LLVOAvatar::NON_IMPOSTORS_MAX_SLIDER <= value)
    {
        value=0;
    }
    gSavedSettings.setU32("RenderAvatarMaxNonImpostors", value);
    LLVOAvatar::updateImpostorRendering(value); // make it effective immediately
}

void FloaterQuickPrefs::updateMaxComplexity()
{
    // Called when the IndirectMaxComplexity control changes
    LLAvatarComplexityControls::updateMax(mMaxComplexitySlider, mMaxComplexityLabel);
}

void FloaterQuickPrefs::updateMaxComplexityLabel(const LLSD& newvalue)
{
    U32 value = newvalue.asInteger();

    LLAvatarComplexityControls::setText(value, mMaxComplexityLabel);
}

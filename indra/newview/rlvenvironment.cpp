/**
 *
 * Copyright (c) 2009-2020, Kitty Barnett
 *
 * The source code in this file is provided to you under the terms of the
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 *
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to
 * abide by those obligations.
 *
 */

#include "llviewerprecompiledheaders.h"

#include "llinventoryfunctions.h"
#include "llsettingsvo.h"
#include <boost/algorithm/string.hpp>

#include "rlvactions.h"
#include "rlvenvironment.h"
#include "rlvhelper.h"

// ================================================================================================
// Constants and helper functions
//

namespace
{
    const F32 SLIDER_SCALE_BLUE_HORIZON_DENSITY(2.0f);
    const F32 SLIDER_SCALE_DENSITY_MULTIPLIER(0.001f);
    const F32 SLIDER_SCALE_GLOW_R(20.0f);
    const F32 SLIDER_SCALE_GLOW_B(-5.0f);
    const F32 SLIDER_SCALE_SUN_AMBIENT(3.0f);

    const std::string RLV_GETENV_PREFIX = "getenv_";
    const std::string RLV_SETENV_PREFIX = "setenv_";

    U32 rlvGetColorComponentFromCharacter(char ch)
    {
        if ( ('r' == ch) || ('x' == ch) )      return VRED;
        else if ( ('g' == ch) || ('y' == ch )) return VGREEN;
        else if ( ('b' == ch) || ('d' == ch) ) return VBLUE;
        else if ('i' == ch)                    return VALPHA;
        return U32_MAX;
    }

    const LLUUID& rlvGetLibraryEnvironmentsFolder()
    {
        LLInventoryModel::cat_array_t cats;
        LLInventoryModel::item_array_t items;
        LLNameCategoryCollector f("Environments");
        gInventory.collectDescendentsIf(gInventory.getLibraryRootFolderID(), cats, items, LLInventoryModel::EXCLUDE_TRASH, f);
        return (!cats.empty()) ? cats.front()->getUUID() : LLUUID::null;
    }

    // Legacy WindLight values we need tend to be expressed as a fraction of the [0, 2PI[ domain
    F32 normalize_angle_domain(F32 angle)
    {
        while (angle < 0)
            angle += F_TWO_PI;
        while (angle > F_TWO_PI)
            angle -= F_TWO_PI;
        return angle;
    }
}

/*
 * Reasoning (Reference - https://upload.wikimedia.org/wikipedia/commons/thumb/f/f7/Azimuth-Altitude_schematic.svg/1024px-Azimuth-Altitude_schematic.svg.png)
 *
 * Given a(zimuth angle) and e(levation angle) - in the SL axis - we know that it calculates the quaternion as follows:
 *
 * | cos a  sin a   0 |   | cos e |   | cos a x cos e | = | x |
 * | sin a  cos a   0 | x |     0 | = | sin a x cos e | = | y | (normalized direction vector identifying the sun position on a unit sphere)
 * |     0      0   1 |   | sin e |   |         sin e | = | z |
 *
 * As a result we can reverse the above by: quaternion -> rotate it around X-axis
 *   x = cos a x cos e <=> cos a = x / cos e   \
 *                                              | (if we divide them we can get rid of cos e)
 *                                              | <=> sin a / cos a = y / x <=> tan a = y / x <=> a = atan2(y, x)
 *   y = sin a x cos e <=> sin a = y / cos e   /
 *   z = sin e         <=> e = asin z
 *
 * If we look at the resulting domain azimuth lies in ]-PI, PI] and elevation lies in [-PI/2, PI/2] which I actually prefer most. Going forward people should get the sun in a wind
 * direction by manipulating the azimuth and then deal with the elevation (which ends up mimicking how a camera or an observer behave in real life).
 *
 * Special cases:
 *   x = 0 => (1) cos e = 0 -> sin e = 1 so y = 0     and z = 1                                    => in (0, 0, 1) we loose all information about the azimuth since cos e = 0
 *         OR (2) cos a = 0 -> sin a = 1 so y = cos e and z = sin e => tan e = z/y (with y != 0)   => in (0, Y, Z) azimuth is PI/2 (or 3PI/2) and elevation can have an extended domain of ]-PI, PI]
 *         => When x = 0 (and y != 0) return PI/2 for azimuth and atan2(z, y) for elevation
 *   y = 0 => (1) sin a = 0 -> cos a = 1 so x = cos e and z = sin e => tan e = z/x (with x != 0)   => in (X, 0, Z) azimuth is    0 (or PI)    and elevation can have an extended domain of ]-PI, PI]
 *         OR (2) cos e = 0 -> see above
           => When y = 0 (and x != 0) return 0 for azimuth and atan2(z, x) for elevation
 *   z = 0 =>     sin e = 0 -> cos e = 1 so x = cos a and y = sin a => tan a = y / x               => in (X, Y, 0) elevation is  0 (or PI)    and azimuth has its normal domain of ]-PI, PI]
 *         => When z = 0 return 0 for elevation and a = atan2(y, x) for azimuth
 *
 * We still need to convert all that back/forth between legacy WindLight's odd choices:
 *   east angle   = SL's azimuth rotates from E (1, 0, 0) to N (0, 1, 0) to W (-1, 0, 0) to S (0, -1, O) but the legacy east angle rotates the opposite way from E to S to W to N so invert the angle
 *                  (the resulting number then needs to be positive and reported as a fraction of 2PI)
 *   sunposition  = sun elevation reported as a fraction of 2PI
 *   moonposition = the moon always has sun's azimuth but its negative elevation
 *
 * Pre-EEP both azimuth and elevation have a 2PI range which means that two different a and e value combinations could yield the same sun direction which causes us problems now since we
 * can't differentiate between the two. Pre-EEP likely favoured elevation over azimuth since people might naturally get the time of day they're thinking of and then try to adjust the
 * azimuth to get the sun in the correct wind direction; however I've already decided that we'll favour azimuth going forward (see above).
 *
 * Comparison of pre-EEP and post-EEP legacy values:
 *   east angle = 0 (aka azimuth = 0)   -> y = 0 so e = atan2(z, x) -> elevation has a range of 2PI so we correctly report pre-EEP values
 *   sunmoonpos = 0 (aka elevation = 0) -> z = 0 so a = atan2(y, x) -> azimuth has a range of 2PI so we correctly report pre-EEP values
 *   -PI/2 < sunmoonpos < PI/2          -> general case             -> post-EEP ranges match pre-EEP ranges so we correctly report pre-EEP values
 *   sunmoonpos > PI/2                  -> elevation went beyond our new maxium so the post-EEP sunmoonpos will actually be off by PI/2 (or 0.25)
 *                                         (and the resulting east angle is off by PI or 0.5 - for example smp 0.375 and ea 0.875 are equivalent with smp 0.125 and ea 0.375)
 *
 * In reverse this means that when setting values through RLVa:
 *   sunmoonpos without eastangle (=0) => always correct
 *   eastangle without sunmoonpos (=0) => always correct
 *   eastangle before sunmoonpos       => always correct
 *   sunmoonpos before eastangle       => correct   for -0.25 <= sunmoonpos <= 0.25
 *                                        incorrect for  0.75  > sunmoonpos  > 0.25
 */
F32 rlvGetAzimuthFromDirectionVector(const LLVector3& vecDir)
{
    if (is_zero(vecDir.mV[VY]))
        return 0.f;
    else if (is_zero(vecDir.mV[VX]))
        return F_PI_BY_TWO;

    F32 radAzimuth = atan2f(vecDir.mV[VY], vecDir.mV[VX]);
    return (radAzimuth >= 0.f) ? radAzimuth : radAzimuth + F_TWO_PI;
}

F32 rlvGetElevationFromDirectionVector(const LLVector3& vecDir)
{
    if (is_zero(vecDir.mV[VZ]))
        return 0.f;

    F32 radElevation;
    if ( (is_zero(vecDir.mV[VX])) && (!is_zero(vecDir.mV[VY])) )
        radElevation = atan2f(vecDir.mV[VZ], vecDir.mV[VY]);
    else if ( (!is_zero(vecDir.mV[VX])) && (is_zero(vecDir.mV[VY])) )
        radElevation = atan2f(vecDir.mV[VZ], vecDir.mV[VX]);
    else
        radElevation = asinf(vecDir.mV[VZ]);
    return (radElevation >= 0.f) ? radElevation : radElevation + F_TWO_PI;
}

// Defined in llsettingssky.cpp
LLQuaternion convert_azimuth_and_altitude_to_quat(F32 azimuth, F32 altitude);

// ================================================================================================
// RlvIsOfSettingsType - Inventory collector for settings of a specific subtype
//

class RlvIsOfSettingsType : public LLInventoryCollectFunctor
{
public:
    RlvIsOfSettingsType(LLSettingsType::type_e eSettingsType, const std::string& strNameMatch = LLStringUtil::null)
        : m_eSettingsType(eSettingsType)
        , m_strNameMatch(strNameMatch)
    {
    }

    ~RlvIsOfSettingsType() override
    {
    }

    bool operator()(LLInventoryCategory*, LLInventoryItem* pItem) override
    {
        if ( (pItem) && (LLAssetType::AT_SETTINGS == pItem->getActualType()) )
        {
            return
                (m_eSettingsType == LLSettingsType::fromInventoryFlags(pItem->getFlags())) &&
                ( (m_strNameMatch.empty()) || (boost::iequals(pItem->getName(), m_strNameMatch)) );
        }
        return false;
    }

protected:
    LLSettingsType::type_e m_eSettingsType;
    std::string            m_strNameMatch;
};

// ================================================================================================
// RlvEnvironment
//

RlvEnvironment::RlvEnvironment()
{
    //
    // Presets
    //
    registerSetEnvFn<LLUUID>("asset", [](LLEnvironment::EnvSelection_t env, const LLUUID& idAsset)
        {
            if (idAsset.isNull())
                return RLV_RET_FAILED_OPTION;

            LLEnvironment::instance().setEnvironment(env, idAsset);
            return RLV_RET_SUCCESS;
        });
    // Deprecated
    auto fnApplyLibraryPreset = [](LLEnvironment::EnvSelection_t env, const std::string& strPreset, LLSettingsType::type_e eSettingsType)
        {
            LLUUID idAsset(strPreset);
            if (idAsset.isNull())
            {
                const LLUUID& idLibraryEnv = rlvGetLibraryEnvironmentsFolder();
                LLInventoryModel::cat_array_t cats;
                LLInventoryModel::item_array_t items;
                RlvIsOfSettingsType f(eSettingsType, strPreset);
                gInventory.collectDescendentsIf(idLibraryEnv, cats, items, LLInventoryModel::EXCLUDE_TRASH, f);
                if (!items.empty())
                    idAsset = items.front()->getAssetUUID();
            }

            if (idAsset.isNull())
                return RLV_RET_FAILED_OPTION;

            LLEnvironment::instance().setEnvironment(env, idAsset);
            return RLV_RET_SUCCESS;
        };
    registerSetEnvFn<std::string>("preset",   [&fnApplyLibraryPreset](LLEnvironment::EnvSelection_t env, const std::string& strPreset) { return fnApplyLibraryPreset(env, strPreset, LLSettingsType::ST_SKY); });
    registerSetEnvFn<std::string>("daycycle", [&fnApplyLibraryPreset](LLEnvironment::EnvSelection_t env, const std::string& strPreset) { return fnApplyLibraryPreset(env, strPreset, LLSettingsType::ST_DAYCYCLE); });

    //
    // Atmosphere & Lighting tab
    //

    // SETTING_AMBIENT
    registerSkyFn<LLColor3>("ambient",      [](LLSettingsSky::ptr_t pSky) { return pSky->getAmbientColor() * (1.f / SLIDER_SCALE_SUN_AMBIENT); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setAmbientColor(clrValue * SLIDER_SCALE_SUN_AMBIENT); });
    registerLegacySkyFn<LLColor3>("ambient",[](LLSettingsSky::ptr_t pSky) { return pSky->getAmbientColor() * (1.f / SLIDER_SCALE_SUN_AMBIENT); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setAmbientColor(clrValue * SLIDER_SCALE_SUN_AMBIENT); });

    // SETTING_BLUE_DENSITY
    registerSkyFn<LLColor3>("bluedensity",  [](LLSettingsSky::ptr_t pSky) { return pSky->getBlueDensity() * (1.f / SLIDER_SCALE_BLUE_HORIZON_DENSITY); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setBlueDensity(clrValue * SLIDER_SCALE_BLUE_HORIZON_DENSITY); });
    registerLegacySkyFn<LLColor3>("bluedensity",[](LLSettingsSky::ptr_t pSky) { return pSky->getBlueDensity() * (1.f / SLIDER_SCALE_BLUE_HORIZON_DENSITY); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setBlueDensity(clrValue * SLIDER_SCALE_BLUE_HORIZON_DENSITY); });

    // SETTING_BLUE_HORIZON
    registerSkyFn<LLColor3>("bluehorizon",  [](LLSettingsSky::ptr_t pSky) { return pSky->getBlueHorizon() * (1.f / SLIDER_SCALE_BLUE_HORIZON_DENSITY); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setBlueHorizon(clrValue * SLIDER_SCALE_BLUE_HORIZON_DENSITY); });
    registerLegacySkyFn<LLColor3>("bluehorizon",[](LLSettingsSky::ptr_t pSky) { return pSky->getBlueHorizon() * (1.f / SLIDER_SCALE_BLUE_HORIZON_DENSITY); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setBlueHorizon(clrValue * SLIDER_SCALE_BLUE_HORIZON_DENSITY); });

    // SETTING_DENSITY_MULTIPLIER
    registerSkyFn<F32>("densitymultiplier", [](LLSettingsSky::ptr_t pSky) { return pSky->getDensityMultiplier() / SLIDER_SCALE_DENSITY_MULTIPLIER; },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setDensityMultiplier(nValue * SLIDER_SCALE_DENSITY_MULTIPLIER); });

    // SETTING_DISTANCE_MULTIPLIER
    registerSkyFn<F32>("distancemultiplier",[](LLSettingsSky::ptr_t pSky) { return pSky->getDistanceMultiplier(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setDistanceMultiplier(nValue); });


    // SETTING_SKY_DROPLET_RADIUS
    registerSkyFn<F32>("dropletradius",     [](LLSettingsSky::ptr_t pSky) { return pSky->getSkyDropletRadius(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setSkyDropletRadius(nValue); });

    // SETTING_HAZE_DENSITY
    registerSkyFn<F32>("hazedensity",       [](LLSettingsSky::ptr_t pSky) { return pSky->getHazeDensity(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setHazeDensity(nValue); });

    // SETTING_HAZE_HORIZON
    registerSkyFn<F32>("hazehorizon",       [](LLSettingsSky::ptr_t pSky) { return pSky->getHazeHorizon(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setHazeHorizon(nValue); });

    // SETTING_SKY_ICE_LEVEL
    registerSkyFn<F32>("icelevel",          [](LLSettingsSky::ptr_t pSky) { return pSky->getSkyIceLevel(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setSkyIceLevel(nValue); });

    // SETTING_MAX_Y
    registerSkyFn<F32>("maxaltitude",       [](LLSettingsSky::ptr_t pSky) { return pSky->getMaxY(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setMaxY(nValue); });

    // SETTING_SKY_MOISTURE_LEVEL
    registerSkyFn<F32>("moisturelevel",     [](LLSettingsSky::ptr_t pSky) { return pSky->getSkyMoistureLevel(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setSkyMoistureLevel(nValue); });

    //  SETTING_GAMMA
    registerSkyFn<F32>("scenegamma",        [](LLSettingsSky::ptr_t pSky) { return pSky->getGamma(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setGamma(nValue); });

    //
    // Clouds tab
    //

    // SETTING_CLOUD_COLOR
    registerSkyFn<LLColor3>("cloudcolor",   [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudColor(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudColor(clrValue); });
    registerLegacySkyFn<LLColor3>("cloudcolor", [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudColor(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudColor(clrValue); });

    // SETTING_CLOUD_SHADOW
    registerSkyFn<F32>("cloudcoverage",     [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudShadow(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setCloudShadow(nValue); });

    // SETTING_CLOUD_POS_DENSITY1
    registerSkyFn<LLColor3>("clouddensity", [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudPosDensity1(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudPosDensity1(clrValue); });
    registerLegacySkyFn<LLColor3>("cloud",  [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudPosDensity1(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudPosDensity1(clrValue); });

    // SETTING_CLOUD_POS_DENSITY2
    registerSkyFn<LLColor3>("clouddetail",  [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudPosDensity2(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudPosDensity2(clrValue); });
    registerLegacySkyFn<LLColor3>("clouddetail",[](LLSettingsSky::ptr_t pSky) { return pSky->getCloudPosDensity2(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setCloudPosDensity2(clrValue); });

    // SETTING_CLOUD_SCALE
    registerSkyFn<F32>("cloudscale",        [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudScale(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setCloudScale(nValue); });

    // SETTING_CLOUD_SCROLL_RATE
    registerSkyFn<LLVector2>("cloudscroll", [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudScrollRate(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLVector2& vecValue) { pSky->setCloudScrollRate(vecValue); });
    registerLegacySkyFn<LLVector2>("cloudscroll", [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudScrollRate(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLVector2& vecValue) { pSky->setCloudScrollRate(vecValue); });

    // SETTING_CLOUD_TEXTUREID
    registerSkyFn<LLUUID>("cloudtexture",   [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudNoiseTextureId();  },
                                            [](LLSettingsSky::ptr_t pSky, const LLUUID& idTexture) { pSky->setCloudNoiseTextureId(idTexture);  });

    // SETTING_CLOUD_VARIANCE
    registerSkyFn<F32>("cloudvariance",     [](LLSettingsSky::ptr_t pSky) { return pSky->getCloudVariance(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setCloudVariance(nValue); });

    //
    // Sun & Moon
    //

    // SETTING_MOON_BRIGHTNESS
    registerSkyFn<F32>("moonbrightness",    [](LLSettingsSky::ptr_t pSky) { return pSky->getMoonBrightness(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setMoonBrightness(nValue); });

    // SETTING_MOON_SCALE
    registerSkyFn<F32>("moonscale",         [](LLSettingsSky::ptr_t pSky) { return pSky->getMoonScale(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setMoonScale(nValue); });

    // SETTING_MOON_TEXTUREID
    registerSkyFn<LLUUID>("moontexture",    [](LLSettingsSky::ptr_t pSky) { return pSky->getMoonTextureId(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLUUID& idTexture) { pSky->setMoonTextureId(idTexture); });

    // SETTING_GLOW
    registerSkyFn<float>("sunglowsize",     [](LLSettingsSky::ptr_t pSky) { return 2.0 - (pSky->getGlow().mV[VRED] / SLIDER_SCALE_GLOW_R); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setGlow(LLColor3((2.0f - nValue) * SLIDER_SCALE_GLOW_R, .0f, pSky->getGlow().mV[VBLUE])); });
    registerSkyFn<float>("sunglowfocus",    [](LLSettingsSky::ptr_t pSky) { return pSky->getGlow().mV[VBLUE] / SLIDER_SCALE_GLOW_B; },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setGlow(LLColor3(pSky->getGlow().mV[VRED], .0f, nValue * SLIDER_SCALE_GLOW_B)); });

    // SETTING_SUNLIGHT_COLOR
    registerSkyFn<LLColor3>("sunlightcolor",[](LLSettingsSky::ptr_t pSky) { return pSky->getSunlightColor() * (1.f / SLIDER_SCALE_SUN_AMBIENT); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setSunlightColor(clrValue * SLIDER_SCALE_SUN_AMBIENT); });
    registerLegacySkyFn<LLColor3>("sunmooncolor", [](LLSettingsSky::ptr_t pSky) { return pSky->getSunlightColor() * (1.f / SLIDER_SCALE_SUN_AMBIENT); },
                                            [](LLSettingsSky::ptr_t pSky, const LLColor3& clrValue) { pSky->setSunlightColor(clrValue * SLIDER_SCALE_SUN_AMBIENT); });

    // SETTING_SUN_SCALE
    registerSkyFn<float>("sunscale",        [](LLSettingsSky::ptr_t pSky) { return pSky->getSunScale(); },
                                            [](LLSettingsSky::ptr_t pSky, F32 nValue) { pSky->setSunScale(nValue); });

    // SETTING_SUN_TEXTUREID
    registerSkyFn<LLUUID>("suntexture",     [](LLSettingsSky::ptr_t pSky) { return pSky->getSunTextureId(); },
                                            [](LLSettingsSky::ptr_t pSky, const LLUUID& idTexture) { pSky->setSunTextureId(idTexture); });

    // SETTING_STAR_BRIGHTNESS
    registerSkyFn<F32>("starbrightness",    [](LLSettingsSky::ptr_t pSky) { return pSky->getStarBrightness(); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue) { pSky->setStarBrightness(nValue); });

    // SETTING_SUN_ROTATION
    registerSkyFn<F32>("sunazimuth",        [](LLSettingsSky::ptr_t pSky) { return rlvGetAzimuthFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation()); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& radAzimuth) {
                                                pSky->setSunRotation(convert_azimuth_and_altitude_to_quat(radAzimuth, rlvGetElevationFromDirectionVector(LLVector3::x_axis* pSky->getSunRotation())));
                                            });
    registerSkyFn<F32>("sunelevation",      [](LLSettingsSky::ptr_t pSky) { return rlvGetElevationFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation()); },
                                            [](LLSettingsSky::ptr_t pSky, F32 radElevation) {
                                                radElevation = llclamp(radElevation, -F_PI_BY_TWO, F_PI_BY_TWO);
                                                pSky->setSunRotation(convert_azimuth_and_altitude_to_quat(rlvGetAzimuthFromDirectionVector(LLVector3::x_axis* pSky->getSunRotation()), radElevation));
                                            });

    // SETTING_MOON_ROTATION
    registerSkyFn<F32>("moonazimuth",       [](LLSettingsSky::ptr_t pSky) { return rlvGetAzimuthFromDirectionVector(LLVector3::x_axis * pSky->getMoonRotation()); },
                                            [](LLSettingsSky::ptr_t pSky, const F32& radAzimuth) {
                                                pSky->setMoonRotation(convert_azimuth_and_altitude_to_quat(radAzimuth, rlvGetElevationFromDirectionVector(LLVector3::x_axis* pSky->getMoonRotation())));
                                            });
    registerSkyFn<F32>("moonelevation",     [](LLSettingsSky::ptr_t pSky) { return rlvGetElevationFromDirectionVector(LLVector3::x_axis * pSky->getMoonRotation()); },
                                            [](LLSettingsSky::ptr_t pSky, F32 radElevation) {
                                                radElevation = llclamp(radElevation, -F_PI_BY_TWO, F_PI_BY_TWO);
                                                pSky->setMoonRotation(convert_azimuth_and_altitude_to_quat(rlvGetAzimuthFromDirectionVector(LLVector3::x_axis* pSky->getMoonRotation()), radElevation));
                                            });

    // Legacy WindLight support (see remarks at the top of this file)
    registerSkyFn<F32>("eastangle",         [](LLSettingsSky::ptr_t pSky) { return normalize_angle_domain(-rlvGetAzimuthFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation())) / F_TWO_PI; },
                                            [](LLSettingsSky::ptr_t pSky, const F32& radEastAngle)
                                            {
                                                const F32 radAzimuth = -radEastAngle * F_TWO_PI;
                                                const F32 radElevation = rlvGetElevationFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation());
                                                pSky->setSunRotation(convert_azimuth_and_altitude_to_quat(radAzimuth, radElevation));
                                                pSky->setMoonRotation(convert_azimuth_and_altitude_to_quat(radAzimuth + F_PI, -radElevation));
                                            });

    registerSkyFn<F32>("sunmoonposition",   [](LLSettingsSky::ptr_t pSky) { return rlvGetElevationFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation()) / F_TWO_PI; },
                                            [](LLSettingsSky::ptr_t pSky, const F32& nValue)
                                            {
                                                const F32 radAzimuth = rlvGetAzimuthFromDirectionVector(LLVector3::x_axis * pSky->getSunRotation());
                                                const F32 radElevation = nValue * F_TWO_PI;
                                                pSky->setSunRotation(convert_azimuth_and_altitude_to_quat(radAzimuth, radElevation));
                                                pSky->setMoonRotation(convert_azimuth_and_altitude_to_quat(radAzimuth + F_PI, -radElevation));
                                            });

    // Create a fixed sky from the nearest daycycle (local > experience > parcel > region)
    registerSetEnvFn<F32>("daytime",        [](LLEnvironment::EnvSelection_t env, const F32& nValue)
                                            {
                                                if ((nValue >= 0.f) && (nValue <= 1.0f))
                                                {
                                                    LLSettingsDay::ptr_t pDay;
                                                    if (LLEnvironment::ENV_EDIT != env)
                                                    {
                                                        LLEnvironment::EnvSelection_t envs[] = { LLEnvironment::ENV_LOCAL, LLEnvironment::ENV_PUSH, LLEnvironment::ENV_PARCEL, LLEnvironment::ENV_REGION };
                                                        for (size_t idxEnv = 0, cntEnv = sizeof(envs) / sizeof(LLEnvironment::EnvSelection_t); idxEnv < cntEnv && !pDay; idxEnv++)
                                                            pDay = LLEnvironment::instance().getEnvironmentDay(envs[idxEnv]);
                                                    }
                                                    else
                                                    {
                                                        pDay = LLEnvironment::instance().getEnvironmentDay(LLEnvironment::ENV_EDIT);
                                                    }

                                                    if (pDay)
                                                    {
                                                        auto pNewSky = LLSettingsVOSky::buildDefaultSky();
                                                        auto pSkyBlender = std::make_shared<LLTrackBlenderLoopingManual>(pNewSky, pDay, 1);
                                                        pSkyBlender->setPosition(nValue);

                                                        LLEnvironment::instance().setEnvironment(env, pNewSky);
                                                        LLEnvironment::instance().updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
                                                    }
                                                }
                                                else if (nValue == -1)
                                                {
                                                    LLEnvironment::instance().clearEnvironment(env);
                                                    LLEnvironment::instance().setSelectedEnvironment(env);
                                                    LLEnvironment::instance().updateEnvironment();
//                                                  defocusEnvFloaters();
                                                }
                                                else
                                                {
                                                    return RLV_RET_FAILED_OPTION;
                                                }

                                                return RLV_RET_SUCCESS;
                                            });
    registerGetEnvFn("daytime",             [](LLEnvironment::EnvSelection_t env)
                                            {
                                                // I forgot how much I hate this command... it literally makes no sense since time of day only has any meaning in an
                                                // actively animating day cycle (but in that case we have to return -1).
                                                if (!LLEnvironment::instance().getEnvironmentFixedSky(env)) {
                                                    return std::to_string(-1.f);
                                                }

                                                // It's invalid input for @setenv_daytime (see above) so it can be fed in without changing the current environment
                                                return std::to_string(2.f);
                                            });
}

RlvEnvironment::~RlvEnvironment()
{
}

// static
LLEnvironment::EnvSelection_t RlvEnvironment::getTargetEnvironment()
{
    return RlvActions::canChangeEnvironment() ? LLEnvironment::ENV_LOCAL : LLEnvironment::ENV_EDIT;
}

// static
LLSettingsSky::ptr_t RlvEnvironment::getTargetSky(bool forSetCmd)
{
    LLEnvironment* pEnv = LLEnvironment::getInstance();

    if (forSetCmd)
    {
        LLEnvironment::EnvSelection_t targetEnv = getTargetEnvironment();
        bool isSharedEnv = !pEnv->getEnvironmentFixedSky(targetEnv),
             hasLocalDayCycle = !isSharedEnv && pEnv->getEnvironmentDay(targetEnv),
             isLocalTransition = !hasLocalDayCycle  && pEnv->getCurrentEnvironmentInstance()->isTransition();
        if ( (isSharedEnv) || (hasLocalDayCycle) || (isLocalTransition) )
        {
            LLSettingsSky::ptr_t pSky = (isSharedEnv) ? pEnv->getEnvironmentFixedSky(LLEnvironment::ENV_PARCEL, true)->buildClone()
                                                      : (hasLocalDayCycle) ? pEnv->getEnvironmentFixedSky(targetEnv)->buildClone()
                                                                           : pEnv->getEnvironmentFixedSky(targetEnv);
            pEnv->setEnvironment(targetEnv, pSky);
            pEnv->setSelectedEnvironment(targetEnv, LLEnvironment::TRANSITION_INSTANT);
            pEnv->updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
        }
    }

    return pEnv->getCurrentSky();
}


// static
bool RlvEnvironment::onHandleCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet, const std::string& strCmdPrefix, const handler_map_t& fnLookup, const legacy_handler_map_t& legacyFnLookup)
{
    if ( (rlvCmd.getBehaviour().length() > strCmdPrefix.length() + 2) && (boost::starts_with(rlvCmd.getBehaviour(), strCmdPrefix)) )
    {
        if ( (RLV_TYPE_FORCE == rlvCmd.getParamType()) && (!RlvActions::canChangeEnvironment(rlvCmd.getObjectID())) )
        {
            cmdRet = RLV_RET_FAILED_LOCK;
            return true;
        }

        std::string strEnvCommand = rlvCmd.getBehaviour().substr(strCmdPrefix.length());

        handler_map_t::const_iterator itFnEntry = fnLookup.find(strEnvCommand);
        if (fnLookup.end() != itFnEntry)
        {
            cmdRet = itFnEntry->second((RLV_TYPE_FORCE == rlvCmd.getParamType()) ? rlvCmd.getOption() : rlvCmd.getParam());
            return true;
        }

        // Legacy handling (blargh)
        U32 idxComponent = rlvGetColorComponentFromCharacter(strEnvCommand.back());
        if (idxComponent <= VALPHA)
        {
            strEnvCommand.pop_back();

            legacy_handler_map_t::const_iterator itLegacyFnEntry = legacyFnLookup.find(strEnvCommand);
            if (legacyFnLookup.end() != itLegacyFnEntry)
            {
                cmdRet = itLegacyFnEntry->second((RLV_TYPE_FORCE == rlvCmd.getParamType()) ? rlvCmd.getOption() : rlvCmd.getParam(), idxComponent);
                return true;
            }
        }
    }

    return false;
}

bool RlvEnvironment::onReplyCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)
{
    return onHandleCommand(rlvCmd, cmdRet, RLV_GETENV_PREFIX, m_GetFnLookup, m_LegacyGetFnLookup);
}

bool RlvEnvironment::onForceCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)
{
    return onHandleCommand(rlvCmd, cmdRet, RLV_SETENV_PREFIX, m_SetFnLookup, m_LegacySetFnLookup);
}

template<>
std::string RlvEnvironment::handleGetFn<float>(const std::function<float(LLSettingsSky::ptr_t)>& fn)
{
    LLSettingsSky::ptr_t pSky = getTargetSky();
    return std::to_string(fn(pSky));
}

template<>
std::string RlvEnvironment::handleGetFn<LLUUID>(const std::function<LLUUID(LLSettingsSky::ptr_t)>& fn)
{
    LLSettingsSky::ptr_t pSky = getTargetSky();
    return fn(pSky).asString();
}

template<>
std::string RlvEnvironment::handleGetFn<LLVector2>(const std::function<LLVector2(LLSettingsSky::ptr_t)>& fn)
{
    LLSettingsSky::ptr_t pSky = getTargetSky();
    LLVector2 replyVec = fn(pSky);
    return llformat("%f/%f", replyVec.mV[VX], replyVec.mV[VY]);
}

template<>
std::string RlvEnvironment::handleGetFn<LLColor3>(const std::function<LLColor3(LLSettingsSky::ptr_t)>& fn)
{
    LLSettingsSky::ptr_t pSky = getTargetSky();
    LLColor3 replyColor = fn(pSky);
    return llformat("%f/%f/%f", replyColor.mV[VX], replyColor.mV[VY], replyColor.mV[VZ]);
}

template<typename T>
ERlvCmdRet RlvEnvironment::handleSetFn(const std::string& strRlvOption, const std::function<void(LLSettingsSky::ptr_t, const T&)>& fn)
{
    T optionValue;
    if (!RlvCommandOptionHelper::parseOption<T>(strRlvOption, optionValue))
        return RLV_RET_FAILED_PARAM;

    LLSettingsSky::ptr_t pSky = getTargetSky(true);
    fn(pSky, optionValue);
    pSky->update();
    return RLV_RET_SUCCESS;
}

template<>
std::string RlvEnvironment::handleLegacyGetFn<LLVector2>(const std::function<const LLVector2 (LLSettingsSkyPtr_t)>& getFn, U32 idxComponent)
{
    if (idxComponent >= 2)
        return LLStringUtil::null;
    return std::to_string(getFn(getTargetSky()).mV[idxComponent]);
}

template<>
std::string RlvEnvironment::handleLegacyGetFn<LLColor3>(const std::function<const LLColor3 (LLSettingsSkyPtr_t)>& getFn, U32 idxComponent)
{
    if ( (idxComponent >= VRED) && (idxComponent <= VBLUE) )
    {
        return std::to_string(getFn(getTargetSky()).mV[idxComponent]);
    }
    else if (idxComponent == VALPHA)
    {
        const LLColor3& clr = getFn(getTargetSky());
        return std::to_string(llmax(clr.mV[VRED], clr.mV[VGREEN], clr.mV[VBLUE]));
    }
    return LLStringUtil::null;
}

template<>
ERlvCmdRet RlvEnvironment::handleLegacySetFn<LLVector2>(float optionValue, LLVector2 curValue, const std::function<void(LLSettingsSkyPtr_t, const LLVector2&)>& setFn, U32 idxComponent)
{
    if (idxComponent >= 2)
        return RLV_RET_FAILED_UNKNOWN;

    LLSettingsSky::ptr_t pSky = getTargetSky(true);
    curValue.mV[idxComponent] = optionValue;
    setFn(pSky, curValue);
    pSky->update();

    return RLV_RET_SUCCESS;
}

template<>
ERlvCmdRet RlvEnvironment::handleLegacySetFn<LLColor3>(float optionValue, LLColor3 curValue, const std::function<void(LLSettingsSkyPtr_t, const LLColor3&)>& setFn, U32 idxComponent)
{
    LLSettingsSky::ptr_t pSky = getTargetSky(true);
    if ( (idxComponent >= VRED) && (idxComponent <= VBLUE) )
    {
        curValue.mV[idxComponent] = optionValue;
    }
    else if (idxComponent == VALPHA)
    {
        const F32 curMax = llmax(curValue.mV[VRED], curValue.mV[VGREEN], curValue.mV[VBLUE]);
        if ( (0.0f == optionValue) || (0.0f == curMax) )
        {
            curValue.mV[VRED] = curValue.mV[VGREEN] = curValue.mV[VBLUE] = optionValue;
        }
        else
        {
            const F32 nDelta = (optionValue - curMax) / curMax;
            curValue.mV[VRED] *= (1.0f + nDelta);
            curValue.mV[VGREEN] *= (1.0f + nDelta);
            curValue.mV[VBLUE] *= (1.0f + nDelta);
        }
    }
    else
    {
        return RLV_RET_FAILED_UNKNOWN;
    }

    setFn(pSky, curValue);
    pSky->update();

    return RLV_RET_SUCCESS;
}


template<typename T>
void RlvEnvironment::registerSkyFn(const std::string& strFnName, const std::function<T(LLSettingsSkyPtr_t)>& getFn, const std::function<void(LLSettingsSkyPtr_t, const T&)>& setFn)
{
    RLV_ASSERT(m_GetFnLookup.end() == m_GetFnLookup.find(strFnName));
    m_GetFnLookup.insert(std::make_pair(strFnName, [this, getFn](const std::string& strRlvParam)
        {
            if (RlvUtil::sendChatReply(strRlvParam, handleGetFn<T>(getFn)))
                return RLV_RET_SUCCESS;
            return RLV_RET_FAILED_PARAM;
        }));

    RLV_ASSERT(m_SetFnLookup.end() == m_SetFnLookup.find(strFnName));
    m_SetFnLookup.insert(std::make_pair(strFnName, [this, setFn](const std::string& strRlvOption)
        {
            return handleSetFn<T>(strRlvOption, setFn);
        }));
}

void RlvEnvironment::registerGetEnvFn(const std::string& strFnName, const std::function<std::string(LLEnvironment::EnvSelection_t env)>& getFn)
{
    RLV_ASSERT(m_GetFnLookup.end() == m_GetFnLookup.find(strFnName));
    m_GetFnLookup.insert(std::make_pair(strFnName, [getFn](const std::string& strRlvParam)
        {
            if (RlvUtil::sendChatReply(strRlvParam, getFn(getTargetEnvironment())))
                return RLV_RET_SUCCESS;
            return RLV_RET_FAILED_PARAM;
        }));
}

template<typename T>
void RlvEnvironment::registerSetEnvFn(const std::string& strFnName, const std::function<ERlvCmdRet(LLEnvironment::EnvSelection_t env, const T& strRlvOption)>& setFn)
{
    RLV_ASSERT(m_SetFnLookup.end() == m_SetFnLookup.find(strFnName));
    m_SetFnLookup.insert(std::make_pair(strFnName, [setFn](const std::string& strRlvOption)
        {
            T optionValue;
            if (!RlvCommandOptionHelper::parseOption<T>(strRlvOption, optionValue))
                return RLV_RET_FAILED_PARAM;
            return setFn(getTargetEnvironment(), optionValue);
        }));
}

template<typename T>
void RlvEnvironment::registerLegacySkyFn(const std::string& strFnName, const std::function<const T (LLSettingsSkyPtr_t)>& getFn, const std::function<void(LLSettingsSkyPtr_t, const T&)>& setFn)
{
    RLV_ASSERT(m_LegacyGetFnLookup.end() == m_LegacyGetFnLookup.find(strFnName));
    m_LegacyGetFnLookup.insert(std::make_pair(strFnName, [this, getFn](const std::string& strRlvParam, U32 idxComponent)
        {
            const std::string strReply = handleLegacyGetFn<T>(getFn, idxComponent);
            if (strReply.empty())
                return RLV_RET_FAILED_UNKNOWN;
            else if (RlvUtil::sendChatReply(strRlvParam, strReply))
                return RLV_RET_SUCCESS;
            return RLV_RET_FAILED_PARAM;
        }));

    RLV_ASSERT(m_LegacySetFnLookup.end() == m_LegacySetFnLookup.find(strFnName));
    m_LegacySetFnLookup.insert(std::make_pair(strFnName, [this, getFn, setFn](const std::string& strRlvOption, U32 idxComponent)
        {
            float optionValue;
            if (!RlvCommandOptionHelper::parseOption(strRlvOption, optionValue))
                return RLV_RET_FAILED_PARAM;
            return handleLegacySetFn<T>(optionValue, getFn(getTargetSky(true)), setFn, idxComponent);;
        }));
}

// ================================================================================================

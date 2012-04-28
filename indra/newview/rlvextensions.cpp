/** 
 *
 * Copyright (c) 2009-2011, Kitty Barnett
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
#include "llagent.h"
#include "llagentcamera.h"
#include "lldaycyclemanager.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"
#include "llwlparammanager.h"

#include "rlvextensions.h"
#include "rlvhandler.h"
#include "rlvhelper.h"

// ============================================================================

class RlvWindLightControl
{
public:
	enum EType			 { TYPE_COLOR, TYPE_COLOR_R, TYPE_FLOAT, TYPE_UNKNOWN };
	enum EColorComponent { COMPONENT_R, COMPONENT_G, COMPONENT_B, COMPONENT_I, COMPONENT_NONE };
public:
	RlvWindLightControl(WLColorControl* pCtrl, bool fColorR) : m_eType((!fColorR) ? TYPE_COLOR: TYPE_COLOR_R), m_pColourCtrl(pCtrl), m_pFloatCtrl(NULL) {}
	RlvWindLightControl(WLFloatControl* pCtrl) : m_eType(TYPE_FLOAT), m_pColourCtrl(NULL), m_pFloatCtrl(pCtrl) {}

	EType		getControlType() const	{ return m_eType; }
	bool		isColorType() const		{ return (TYPE_COLOR == m_eType) || (TYPE_COLOR_R == m_eType); }
	bool		isFloatType() const		{ return (TYPE_FLOAT == m_eType); }
	// TYPE_COLOR and TYPE_COLOR_R
	F32			getColorComponent(EColorComponent eComponent, bool& fError) const;
	LLVector4	getColorVector(bool& fError) const;
	bool		setColorComponent(EColorComponent eComponent, F32 nValue);
	// TYPE_FLOAT
	F32			getFloat(bool& fError) const;
	bool		setFloat(F32 nValue);

	static EColorComponent getComponentFromCharacter(char ch);
protected:
	EType			m_eType;			// Type of the WindLight control
	WLColorControl*	m_pColourCtrl;
	WLFloatControl*	m_pFloatCtrl;
};

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
static F32 get_intensity_from_color(const LLVector4& v)
{
	return llmax(v.mV[0], v.mV[1], v.mV[2]);
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
F32 RlvWindLightControl::getColorComponent(EColorComponent eComponent, bool& fError) const
{
	switch (eComponent)
	{
		case COMPONENT_R: return getColorVector(fError).mV[0];
		case COMPONENT_G: return getColorVector(fError).mV[1];
		case COMPONENT_B: return getColorVector(fError).mV[2];
		case COMPONENT_I: return get_intensity_from_color(getColorVector(fError));	// SL-2.8: Always seems to be 1.0 so get it manually
		default         : RLV_ASSERT(false); fError = true; return 0.0;
	}
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
RlvWindLightControl::EColorComponent RlvWindLightControl::getComponentFromCharacter(char ch)
{
	if (('r' == ch) || ('x' == ch))
		return COMPONENT_R;
	else if (('g' == ch) || ('y' == ch))
		return COMPONENT_G;
	else if (('b' == ch) || ('d' == ch))
		return COMPONENT_B;
	else if ('i' == ch)
		return COMPONENT_I;
	return COMPONENT_NONE;
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
LLVector4 RlvWindLightControl::getColorVector(bool& fError) const
{
	if ((fError = !isColorType()))
		return LLVector4(0, 0, 0, 0);
	F32 nMult = (m_pColourCtrl->isSunOrAmbientColor) ? 3.0f : ((m_pColourCtrl->isBlueHorizonOrDensity) ? 2.0f : 1.0f);
	return LLWLParamManager::getInstance()->mCurParams.getVector(m_pColourCtrl->mName, fError) / nMult;
}

// Checked: 2011-08-28 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
bool RlvWindLightControl::setColorComponent(EColorComponent eComponent, F32 nValue)
{
	if (isColorType())
	{
		nValue *= (m_pColourCtrl->isSunOrAmbientColor) ? 3.0f : ((m_pColourCtrl->isBlueHorizonOrDensity) ? 2.0f : 1.0f);
		if (COMPONENT_I == eComponent)								// (See: LLFloaterWindLight::onColorControlIMoved)
		{
			if (m_pColourCtrl->hasSliderName)
			{
				F32 curMax = llmax(m_pColourCtrl->r, m_pColourCtrl->g, m_pColourCtrl->b);
				if ( (0.0f == nValue) || (0.0f == curMax) )
				{
					m_pColourCtrl->r = m_pColourCtrl->g = m_pColourCtrl->b = m_pColourCtrl->i = nValue;
				}
				else
				{
					F32 nDelta = (nValue - curMax) / curMax;
					m_pColourCtrl->r *= (1.0f + nDelta);
					m_pColourCtrl->g *= (1.0f + nDelta);
					m_pColourCtrl->b *= (1.0f + nDelta);
					m_pColourCtrl->i = nValue;
				}
			}
		}
		else														// (See: LLFloaterWindLight::onColorControlRMoved)
		{
			F32* pnValue = (COMPONENT_R == eComponent) ? &m_pColourCtrl->r : (COMPONENT_G == eComponent) ? &m_pColourCtrl->g : (COMPONENT_B == eComponent) ? &m_pColourCtrl->b : NULL;
			if (pnValue)
				*pnValue = nValue;
			if (m_pColourCtrl->hasSliderName)
				m_pColourCtrl->i = llmax(m_pColourCtrl->r, m_pColourCtrl->g, m_pColourCtrl->b);
		}
		m_pColourCtrl->update(LLWLParamManager::getInstance()->mCurParams);
		LLWLParamManager::getInstance()->propagateParameters();
	}
	return isColorType();
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
F32 RlvWindLightControl::getFloat(bool& fError) const
{
	return (!(fError = (TYPE_FLOAT != m_eType))) ? LLWLParamManager::getInstance()->mCurParams.getVector(m_pFloatCtrl->mName, fError).mV[0] * m_pFloatCtrl->mult : 0.0;
}

// Checked: 2011-08-28 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
bool RlvWindLightControl::setFloat(F32 nValue)
{
	if (TYPE_FLOAT == m_eType)
	{
		m_pFloatCtrl->x = nValue / m_pFloatCtrl->mult;
		m_pFloatCtrl->update(LLWLParamManager::getInstance()->mCurParams);
		LLWLParamManager::getInstance()->propagateParameters();
	}
	return (TYPE_FLOAT == m_eType);
}

// ============================================================================

class RlvWindLight : public LLSingleton<RlvWindLight>
{
	friend class LLSingleton<RlvWindLight>;
public:
	RlvWindLight();

	std::string	getValue(const std::string& strSetting, bool& fError);
	bool		setValue(const std::string& strRlvName, const std::string& strValue);

protected:
	std::map<std::string, RlvWindLightControl> m_ControlLookupMap;
};

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
RlvWindLight::RlvWindLight()
{
	LLWLParamManager* pWLParamMgr = LLWLParamManager::getInstance();

	// TYPE_FLOAT
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("cloudcoverage",	RlvWindLightControl(&pWLParamMgr->mCloudCoverage)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("cloudscale",		RlvWindLightControl(&pWLParamMgr->mCloudScale)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("densitymultiplier", RlvWindLightControl(&pWLParamMgr->mDensityMult)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("distancemultiplier", RlvWindLightControl(&pWLParamMgr->mDistanceMult)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("maxaltitude",	RlvWindLightControl(&pWLParamMgr->mMaxAlt)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("scenegamma",		RlvWindLightControl(&pWLParamMgr->mWLGamma)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("hazedensity",	RlvWindLightControl(&pWLParamMgr->mHazeDensity)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("hazehorizon",	RlvWindLightControl(&pWLParamMgr->mHazeHorizon)));
	// TYPE_COLOR
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("ambient",		RlvWindLightControl(&pWLParamMgr->mAmbient, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("bluedensity",	RlvWindLightControl(&pWLParamMgr->mBlueDensity, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("bluehorizon",	RlvWindLightControl(&pWLParamMgr->mBlueHorizon, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("cloud",			RlvWindLightControl(&pWLParamMgr->mCloudMain, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("cloudcolor",		RlvWindLightControl(&pWLParamMgr->mCloudColor, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("clouddetail",	RlvWindLightControl(&pWLParamMgr->mCloudDetail, false)));
	m_ControlLookupMap.insert(std::pair<std::string, RlvWindLightControl>("sunmooncolor",	RlvWindLightControl(&pWLParamMgr->mSunlight, false)));
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
std::string RlvWindLight::getValue(const std::string& strSetting, bool& fError)
{
	LLWLParamManager* pWLParams = LLWLParamManager::getInstance(); 
	LLEnvManagerNew* pEnvMgr = LLEnvManagerNew::getInstance();

	fError = false;							// Assume we won't fail
	if ("preset" == strSetting)
		return (pEnvMgr->getUseFixedSky()) ? pEnvMgr->getSkyPresetName() : std::string();
	else if ("daycycle" == strSetting)
		return (pEnvMgr->getUseDayCycle()) ? pEnvMgr->getDayCycleName() : std::string();

	F32 nValue = 0.0f;
	if ("daytime" == strSetting)
	{
		nValue = (pEnvMgr->getUseFixedSky()) ? pWLParams->mCurParams.getFloat("sun_angle", fError) / F_TWO_PI : -1.0f;
	}
	else if (("sunglowfocus" == strSetting) || ("sunglowsize" == strSetting))
	{
		pWLParams->mGlow = pWLParams->mCurParams.getVector(pWLParams->mGlow.mName, fError);
		RLV_ASSERT_DBG(!fError);

		if ("sunglowfocus" == strSetting) 
			nValue = -pWLParams->mGlow.b / 5.0f;
		else
			nValue = 2 - pWLParams->mGlow.r / 20.0f;
	}
	else if ("starbrightness" == strSetting)		nValue = pWLParams->mCurParams.getStarBrightness();
	else if ("eastangle" == strSetting)				nValue = pWLParams->mCurParams.getEastAngle() / F_TWO_PI;
	else if ("sunmoonposition" == strSetting)		nValue = pWLParams->mCurParams.getSunAngle() / F_TWO_PI;
	else if ("cloudscrollx" == strSetting)			nValue = pWLParams->mCurParams.getCloudScrollX() - 10.0f;
	else if ("cloudscrolly" == strSetting)			nValue = pWLParams->mCurParams.getCloudScrollY() - 10.0f;
	else
	{
		std::map<std::string, RlvWindLightControl>::const_iterator itControl = m_ControlLookupMap.find(strSetting);
		if (m_ControlLookupMap.end() != itControl)
		{
			switch (itControl->second.getControlType())
			{
				case RlvWindLightControl::TYPE_FLOAT:
					nValue = itControl->second.getFloat(fError);
					break;
				case RlvWindLightControl::TYPE_COLOR_R:
					nValue = itControl->second.getColorComponent(RlvWindLightControl::COMPONENT_R, fError);
					break;
				default:
					fError = true;
					break;
			}
		}
		else
		{
			// Couldn't find the exact name, check for a color control name
			RlvWindLightControl::EColorComponent eComponent = RlvWindLightControl::getComponentFromCharacter(strSetting[strSetting.length() - 1]);
			if (RlvWindLightControl::COMPONENT_NONE != eComponent)
				itControl = m_ControlLookupMap.find(strSetting.substr(0, strSetting.length() - 1));
			if ( (m_ControlLookupMap.end() != itControl) && (itControl->second.isColorType()) )
				nValue = itControl->second.getColorComponent(eComponent, fError);
			else
				fError = true;
		}
	}
	return llformat("%f", nValue);
}

// Checked: 2011-08-29 (RLVa-1.4.1a) | Added: RLVa-1.4.1a
bool RlvWindLight::setValue(const std::string& strRlvName, const std::string& strValue)
{
	F32 nValue = 0.0f;
	// Sanity check - make sure strValue specifies a number for all settings except "preset" and "daycycle"
	if ( (RlvSettings::getNoSetEnv()) || 
		 ( (!LLStringUtil::convertToF32(strValue, nValue)) && (("preset" != strRlvName) && ("daycycle" != strRlvName)) ) )
 	{
		return false;
	}

	LLWLParamManager* pWLParams = LLWLParamManager::getInstance(); 
	LLEnvManagerNew* pEnvMgr = LLEnvManagerNew::getInstance();

	if ("daytime" == strRlvName)
	{
		if (0.0f <= nValue)
		{
			pWLParams->mAnimator.deactivate();
			pWLParams->mAnimator.setDayTime(nValue);
			pWLParams->mAnimator.update(pWLParams->mCurParams);
		}
		else
		{
			pEnvMgr->useRegionSettings();
		}
		return true;
	}
	else if ("preset" == strRlvName)
	{
		std::string strPresetName = pWLParams->findPreset(strValue, LLEnvKey::SCOPE_LOCAL);
		if (!strPresetName.empty())
			pEnvMgr->useSkyPreset(strPresetName);
		return !strPresetName.empty();
	}
	else if ("daycycle" == strRlvName)
	{
		std::string strPresetName = LLDayCycleManager::instance().findPreset(strValue);
		if (!strPresetName.empty())
			pEnvMgr->useDayCycle(strValue, LLEnvKey::SCOPE_LOCAL);
		return !strPresetName.empty();
	}

	bool fError = false;
	pWLParams->mAnimator.deactivate();
	if (("sunglowfocus" == strRlvName) || ("sunglowsize" == strRlvName))
	{
		pWLParams->mGlow = pWLParams->mCurParams.getVector(pWLParams->mGlow.mName, fError);
		RLV_ASSERT_DBG(!fError);

		if ("sunglowfocus" == strRlvName)
			pWLParams->mGlow.b = -nValue * 5;
		else
			pWLParams->mGlow.r = (2 - nValue) * 20;

		pWLParams->mGlow.update(pWLParams->mCurParams);
		pWLParams->propagateParameters();
		return true;
	}
	else if ("starbrightness" == strRlvName)
	{
		pWLParams->mCurParams.setStarBrightness(nValue);
		return true;
	}
	else if (("eastangle" == strRlvName) || ("sunmoonposition" == strRlvName))
	{
		if ("eastangle" == strRlvName)
			pWLParams->mCurParams.setEastAngle(F_TWO_PI * nValue);
		else
			pWLParams->mCurParams.setSunAngle(F_TWO_PI * nValue);

		// Set the sun vector
		pWLParams->mLightnorm.r = -sin(pWLParams->mCurParams.getEastAngle()) * cos(pWLParams->mCurParams.getSunAngle());
		pWLParams->mLightnorm.g = sin(pWLParams->mCurParams.getSunAngle());
		pWLParams->mLightnorm.b = cos(pWLParams->mCurParams.getEastAngle()) * cos(pWLParams->mCurParams.getSunAngle());
		pWLParams->mLightnorm.i = 1.f;

		pWLParams->propagateParameters();
		return true;
	}
	else if ("cloudscrollx" == strRlvName)
	{
		pWLParams->mCurParams.setCloudScrollX(nValue + 10.0f);
		return true;
	}
	else if ("cloudscrolly" == strRlvName)
	{
		pWLParams->mCurParams.setCloudScrollY(nValue + 10.0f);
		return true;
	}

	std::map<std::string, RlvWindLightControl>::iterator itControl = m_ControlLookupMap.find(strRlvName);
	if (m_ControlLookupMap.end() != itControl)
	{
		switch (itControl->second.getControlType())
		{
			case RlvWindLightControl::TYPE_FLOAT:
				return itControl->second.setFloat(nValue);
			case RlvWindLightControl::TYPE_COLOR_R:
				return itControl->second.setColorComponent(RlvWindLightControl::COMPONENT_R, nValue);
			default:
				RLV_ASSERT(false);
		}
	}
	else
	{
		// Couldn't find the exact name, check for a color control name
		RlvWindLightControl::EColorComponent eComponent = RlvWindLightControl::getComponentFromCharacter(strRlvName[strRlvName.length() - 1]);
		if (RlvWindLightControl::COMPONENT_NONE != eComponent)
			itControl = m_ControlLookupMap.find(strRlvName.substr(0, strRlvName.length() - 1));
		if ( (m_ControlLookupMap.end() != itControl) && (itControl->second.isColorType()) )
			return itControl->second.setColorComponent(eComponent, nValue);
	}
	return false;
}

// ============================================================================

std::map<std::string, S16> RlvExtGetSet::m_DbgAllowed;
std::map<std::string, std::string> RlvExtGetSet::m_PseudoDebug;

// Checked: 2009-06-03 (RLVa-0.2.0h) | Modified: RLVa-0.2.0h
RlvExtGetSet::RlvExtGetSet()
{
	if (!m_DbgAllowed.size())	// m_DbgAllowed is static and should only be initialized once
	{
		m_DbgAllowed.insert(std::pair<std::string, S16>("AvatarSex", DBG_READ | DBG_WRITE | DBG_PSEUDO));
		m_DbgAllowed.insert(std::pair<std::string, S16>("RenderResolutionDivisor", DBG_READ | DBG_WRITE));
		#ifdef RLV_EXTENSION_CMD_GETSETDEBUG_EX
			m_DbgAllowed.insert(std::pair<std::string, S16>(RLV_SETTING_FORBIDGIVETORLV, DBG_READ));
			m_DbgAllowed.insert(std::pair<std::string, S16>(RLV_SETTING_NOSETENV, DBG_READ));
			m_DbgAllowed.insert(std::pair<std::string, S16>("WindLightUseAtmosShaders", DBG_READ));
		#endif // RLV_EXTENSION_CMD_GETSETDEBUG_EX

		// Cache persistance of every setting
		LLControlVariable* pSetting;
		for (std::map<std::string, S16>::iterator itDbg = m_DbgAllowed.begin(); itDbg != m_DbgAllowed.end(); ++itDbg)
		{
			if ( ((pSetting = gSavedSettings.getControl(itDbg->first)) != NULL) && (pSetting->isPersisted()) )
				itDbg->second |= DBG_PERSIST;
		}
	}
}

// Checked: 2009-05-17 (RLVa-0.2.0a)
bool RlvExtGetSet::onForceCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)
{
	return processCommand(rlvCmd, cmdRet);
}

// Checked: 2009-05-17 (RLVa-0.2.0a)
bool RlvExtGetSet::onReplyCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet)
{
	return processCommand(rlvCmd, cmdRet);
}

// Checked: 2009-12-23 (RLVa-1.1.0k) | Modified: RLVa-1.1.0k
bool RlvExtGetSet::processCommand(const RlvCommand& rlvCmd, ERlvCmdRet& eRet)
{
	std::string strBehaviour = rlvCmd.getBehaviour(), strGetSet, strSetting;
	int idxSetting = strBehaviour.find('_');
	if ( (strBehaviour.length() >= 6) && (-1 != idxSetting) && ((int)strBehaviour.length() > idxSetting + 1) )
	{
		strSetting = strBehaviour.substr(idxSetting + 1);
		strBehaviour.erase(idxSetting);	// Get rid of "_<setting>"

		strGetSet = strBehaviour.substr(0, 3);
		strBehaviour.erase(0, 3);		// Get rid of get/set

		if ("debug" == strBehaviour)
		{
			if ( ("get" == strGetSet) && (RLV_TYPE_REPLY == rlvCmd.getParamType()) )
			{
				RlvUtil::sendChatReply(rlvCmd.getParam(), onGetDebug(strSetting));
				eRet = RLV_RET_SUCCESS;
				return true;
			}
			else if ( ("set" == strGetSet) && (RLV_TYPE_FORCE == rlvCmd.getParamType()) )
			{
				if (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_SETDEBUG, rlvCmd.getObjectID()))
					eRet = onSetDebug(strSetting, rlvCmd.getOption());
				else
					eRet = RLV_RET_FAILED_LOCK;
				return true;
			}
		}
		else if ("env" == strBehaviour)
		{
			bool fError = false;
			if ( ("get" == strGetSet) && (RLV_TYPE_REPLY == rlvCmd.getParamType()) )
			{
				RlvUtil::sendChatReply(rlvCmd.getParam(), RlvWindLight::instance().getValue(strSetting, fError));
				eRet = (!fError) ? RLV_RET_SUCCESS : RLV_RET_FAILED_UNKNOWN;
				return true;
			}
			else if ( ("set" == strGetSet) && (RLV_TYPE_FORCE == rlvCmd.getParamType()) )
			{
				if (!gRlvHandler.hasBehaviourExcept(RLV_BHVR_SETENV, rlvCmd.getObjectID()))
					eRet = (RlvWindLight::instance().setValue(strSetting, rlvCmd.getOption())) ? RLV_RET_SUCCESS : RLV_RET_FAILED_UNKNOWN;
				else
					eRet = RLV_RET_FAILED_LOCK;
				return true;
			}
		}
	}
	else if ("setrot" == rlvCmd.getBehaviour())
	{
		// NOTE: if <option> is invalid (or missing) altogether then RLV-1.17 will rotate to 0.0 (which is actually PI / 4)
		F32 nAngle = 0.0f;
		if (LLStringUtil::convertToF32(rlvCmd.getOption(), nAngle))
		{
			nAngle = RLV_SETROT_OFFSET - nAngle;

			gAgentCamera.startCameraAnimation();

			LLVector3 at(LLVector3::x_axis);
			at.rotVec(nAngle, LLVector3::z_axis);
			at.normalize();
			gAgent.resetAxes(at);

			eRet = RLV_RET_SUCCESS;
		}
		else
		{
			eRet = RLV_RET_FAILED_OPTION;
		}
		return true;
	}
	return false;
}

// Checked: 2009-06-03 (RLVa-0.2.0h) | Modified: RLVa-0.2.0h
bool RlvExtGetSet::findDebugSetting(std::string& strSetting, S16& flags)
{
	LLStringUtil::toLower(strSetting);	// Convenience for non-RLV calls

	std::string strTemp;
	for (std::map<std::string, S16>::const_iterator itSetting = m_DbgAllowed.begin(); itSetting != m_DbgAllowed.end(); ++itSetting)
	{
		strTemp = itSetting->first;
		LLStringUtil::toLower(strTemp);
		
		if (strSetting == strTemp)
		{
			strSetting = itSetting->first;
			flags = itSetting->second;
			return true;
		}
	}
	return false;
}

// Checked: 2009-06-03 (RLVa-0.2.0h) | Added: RLVa-0.2.0h
S16 RlvExtGetSet::getDebugSettingFlags(const std::string& strSetting)
{
	std::map<std::string, S16>::const_iterator itSetting = m_DbgAllowed.find(strSetting);
	return (itSetting != m_DbgAllowed.end()) ? itSetting->second : 0;
}

// Checked: 2009-06-03 (RLVa-0.2.0h) | Modified: RLVa-0.2.0h
std::string RlvExtGetSet::onGetDebug(std::string strSetting)
{
	S16 dbgFlags;
	if ( (findDebugSetting(strSetting, dbgFlags)) && ((dbgFlags & DBG_READ) == DBG_READ) )
	{
		if ((dbgFlags & DBG_PSEUDO) == 0)
		{
			LLControlVariable* pSetting = gSavedSettings.getControl(strSetting);
			if (pSetting)
			{
				switch (pSetting->type())
				{
					case TYPE_U32:
						return llformat("%u", gSavedSettings.getU32(strSetting));
					case TYPE_S32:
						return llformat("%d", gSavedSettings.getS32(strSetting));
					case TYPE_BOOLEAN:
						return llformat("%d", gSavedSettings.getBOOL(strSetting));
					default:
						RLV_ERRS << "Unexpected debug setting type" << LL_ENDL;
						break;
				}
			}
		}
		else
		{
			return onGetPseudoDebug(strSetting);
		}
	}
	return std::string();
}

// Checked: 2009-10-03 (RLVa-1.0.4e) | Added: RLVa-1.0.4e
std::string RlvExtGetSet::onGetPseudoDebug(const std::string& strSetting)
{
	// Skip sanity checking because it's all done in RlvExtGetSet::onGetDebug() already
	if ("AvatarSex" == strSetting)
	{
		std::map<std::string, std::string>::const_iterator itPseudo = m_PseudoDebug.find(strSetting);
		if (itPseudo != m_PseudoDebug.end())
		{
			return itPseudo->second;
		}
		else
		{
			if (isAgentAvatarValid())
				return llformat("%d", (gAgentAvatarp->getSex() == SEX_MALE)); // [See LLFloaterCustomize::LLFloaterCustomize()]
		}
	}
	return std::string();
}

// Checked: 2009-10-10 (RLVa-1.0.4e) | Modified: RLVa-1.0.4e
ERlvCmdRet RlvExtGetSet::onSetDebug(std::string strSetting, const std::string& strValue)
{
	S16 dbgFlags; ERlvCmdRet eRet = RLV_RET_FAILED_UNKNOWN;
	if ( (findDebugSetting(strSetting, dbgFlags)) && ((dbgFlags & DBG_WRITE) == DBG_WRITE) )
	{
		eRet = RLV_RET_FAILED_OPTION;
		if ((dbgFlags & DBG_PSEUDO) == 0)
		{
			LLControlVariable* pSetting = gSavedSettings.getControl(strSetting);
			if (pSetting)
			{
				U32 u32Value; S32 s32Value; BOOL fValue;
				switch (pSetting->type())
				{
					case TYPE_U32:
						if (LLStringUtil::convertToU32(strValue, u32Value))
						{
							gSavedSettings.setU32(strSetting, u32Value);
							eRet = RLV_RET_SUCCESS;
						}
						break;
					case TYPE_S32:
						if (LLStringUtil::convertToS32(strValue, s32Value))
						{
							gSavedSettings.setS32(strSetting, s32Value);
							eRet = RLV_RET_SUCCESS;
						}
						break;
					case TYPE_BOOLEAN:
						if (LLStringUtil::convertToBOOL(strValue, fValue))
						{
							gSavedSettings.setBOOL(strSetting, fValue);
							eRet = RLV_RET_SUCCESS;
						}
						break;
					default:
						RLV_ERRS << "Unexpected debug setting type" << LL_ENDL;
						eRet = RLV_RET_FAILED;
						break;
				}

				// Default settings should persist if they were marked that way, but non-default settings should never persist
				pSetting->setPersist( (pSetting->isDefault()) ? ((dbgFlags & DBG_PERSIST) == DBG_PERSIST) : false );
			}
		}
		else
		{
			eRet = onSetPseudoDebug(strSetting, strValue);
		}
	}
	return eRet;
}

// Checked: 2009-10-10 (RLVa-1.0.4e) | Modified: RLVa-1.0.4e
ERlvCmdRet RlvExtGetSet::onSetPseudoDebug(const std::string& strSetting, const std::string& strValue)
{
	ERlvCmdRet eRet = RLV_RET_FAILED_OPTION;
	if ("AvatarSex" == strSetting)
	{
		BOOL fValue;
		if (LLStringUtil::convertToBOOL(strValue, fValue))
		{
			m_PseudoDebug[strSetting] = strValue;
			eRet = RLV_RET_SUCCESS;
		}
	}
	return eRet;
}

// ============================================================================

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
#include "llvoavatarself.h"

#include "rlvextensions.h"
#include "rlvhandler.h"
#include "rlvhelper.h"

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
		m_DbgAllowed.insert(std::pair<std::string, S16>(RLV_SETTING_FORBIDGIVETORLV, DBG_READ));
		m_DbgAllowed.insert(std::pair<std::string, S16>(RLV_SETTING_NOSETENV, DBG_READ));
		m_DbgAllowed.insert(std::pair<std::string, S16>("WindLightUseAtmosShaders", DBG_READ));

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
				pSetting->setPersist( ((pSetting->isDefault()) && ((dbgFlags & DBG_PERSIST) == DBG_PERSIST)) ? LLControlVariable::PERSIST_NONDFT : LLControlVariable::PERSIST_NO );
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

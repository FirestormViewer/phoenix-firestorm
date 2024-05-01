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

#pragma once

#include "llenvironment.h"

#include "rlvcommon.h"

// ============================================================================
// RlvEnvironment - viewer-side scripted environment changes
//

class RlvEnvironment : public RlvExtCommandHandler
{
public:
    RlvEnvironment();
    ~RlvEnvironment() override;

    bool onReplyCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet) override;
    bool onForceCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet) override;
protected:
    static LLEnvironment::EnvSelection_t getTargetEnvironment();
    static LLSettingsSky::ptr_t getTargetSky(bool forSetCmd = false);
    typedef std::map<std::string, std::function<ERlvCmdRet(const std::string&)>> handler_map_t;
    typedef std::map<std::string, std::function<ERlvCmdRet(const std::string&, U32)>> legacy_handler_map_t;
    static bool onHandleCommand(const RlvCommand& rlvCmd, ERlvCmdRet& cmdRet, const std::string& strCmdPrefix, const handler_map_t& fnLookup, const legacy_handler_map_t& legacyFnLookup);

    /*
     * Command registration
     */
protected:
                         void registerGetEnvFn(const std::string& strFnName, const std::function<std::string(LLEnvironment::EnvSelection_t env)>& getFn);
    template<typename T> void registerSetEnvFn(const std::string& strFnName, const std::function<ERlvCmdRet(LLEnvironment::EnvSelection_t env, const T& strRlvOption)>& setFn);
    template<typename T> void registerSkyFn(const std::string& strFnName, const std::function<T(LLSettingsSky::ptr_t)>& getFn, const std::function<void(LLSettingsSky::ptr_t, const T&)>& setFn);
    template<typename T> void registerLegacySkyFn(const std::string& strFnName, const std::function<const T (LLSettingsSky::ptr_t)>& getFn, const std::function<void(LLSettingsSky::ptr_t, const T&)>& setFn);

    // Command handling helpers
    template<typename T> std::string handleGetFn(const std::function<T(LLSettingsSky::ptr_t)>& fn);
    template<typename T> ERlvCmdRet  handleSetFn(const std::string& strRlvOption, const std::function<void(LLSettingsSky::ptr_t, const T&)>& fn);
    template<typename T> std::string handleLegacyGetFn(const std::function<const T (LLSettingsSky::ptr_t)>& getFn, U32 idxComponent);
    template<typename T> ERlvCmdRet  handleLegacySetFn(float optionValue, T value, const std::function<void(LLSettingsSky::ptr_t, const T&)>& setFn, U32 idxComponent);

    /*
     * Member variables
     */
protected:
    handler_map_t m_GetFnLookup;
    handler_map_t m_SetFnLookup;
    legacy_handler_map_t m_LegacyGetFnLookup;
    legacy_handler_map_t m_LegacySetFnLookup;
};

// ============================================================================

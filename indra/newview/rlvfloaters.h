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

#ifndef RLV_FLOATERS_H
#define RLV_FLOATERS_H

#include "llfloater.h"

#include "rlvdefines.h"
#include "rlvcommon.h"

// ============================================================================
// Foward declarations
//
class LLComboBox;
class LLTextEditor;

// ============================================================================
// RlvFloaterLocks class declaration
//

class RlvFloaterBehaviours : public LLFloater
{
	friend class LLFloaterReg;
private:
	RlvFloaterBehaviours(const LLSD& sdKey) : LLFloater(sdKey) {}

	/*
	 * LLFloater overrides
	 */
public:
	/*virtual*/ void onOpen(const LLSD& sdKey);
	/*virtual*/ void onClose(bool fQuitting);
	/*virtual*/ BOOL postBuild();

	/*
	 * Member functions
	 */
protected:
	void onAvatarNameLookup(const LLUUID& idAgent, const LLAvatarName& avName);
	void onBtnCopyToClipboard();
	void onCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet);
	void refreshAll();

	/*
	 * Member variables
	 */
protected:
	boost::signals2::connection	m_ConnRlvCommand;
	uuid_vec_t 					m_PendingLookup;
};

// ============================================================================
// RlvFloaterLocks class declaration
//

class RlvFloaterLocks : public LLFloater
{
	friend class LLFloaterReg;
private:
	RlvFloaterLocks(const LLSD& sdKey) : LLFloater(sdKey) {}

	/*
	 * LLFloater overrides
	 */
public:
	/*virtual*/ void onOpen(const LLSD& sdKey);
	/*virtual*/ void onClose(bool fQuitting);
	/*virtual*/ BOOL postBuild();

	/*
	 * Member functions
	 */
protected:
	void onRlvCommand(const RlvCommand& rlvCmd, ERlvCmdRet eRet);
	void refreshAll();

	/*
	 * Member variables
	 */
protected:
	boost::signals2::connection m_ConnRlvCommand;
};

// ============================================================================
// RlvFloaterStrings class declaration
//

class RlvFloaterStrings : public LLFloater
{
	friend class LLFloaterReg;
private:
	RlvFloaterStrings(const LLSD& sdKey);

	// LLFloater overrides
public:
	/*virtual*/ void onClose(bool fQuitting);
	/*virtual*/ BOOL postBuild();

	// Member functions
protected:
	void onStringRevertDefault();
	void checkDirty(bool fRefresh);
	void refresh();

	// Member variables
protected:
	bool		m_fDirty;
	std::string m_strStringCurrent;
	LLComboBox*	m_pStringList;
	LLSD		m_sdStringsInfo;
};

// ============================================================================
// RlvFloaterConsole - debug console to allow command execution without the need for a script
//

class RlvFloaterConsole : public LLFloater
{
	friend class LLFloaterReg;
	template<ERlvParamType> friend struct RlvCommandHandlerBaseImpl;
	friend class RlvHandler;
private:
	RlvFloaterConsole(const LLSD& sdKey);
	~RlvFloaterConsole() override;

	/*
	 * LLFloater overrides
	 */
public:
	BOOL postBuild() override;
	void onClose(bool fQuitting) override;
	
	/*
	 * Member functions
	 */
protected:
	void addCommandReply(const std::string& strCommand, const std::string& strReply);
	void onInput(LLUICtrl* ctrl, const LLSD& param);

	/*
	 * Member variables
	 */
protected:
	LLTextEditor* m_pOutputText;
};

// ============================================================================

#endif // RLV_FLOATERS_H

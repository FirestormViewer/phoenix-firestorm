/** 
 * @file rlvfloaters.h
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

#endif // RLV_FLOATERS_H

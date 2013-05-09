/** 
 *
 * Copyright (c) 2009-2013, Kitty Barnett
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

#ifndef RLV_ACTIONS_H
#define RLV_ACTIONS_H

// ============================================================================
// RlvActions class declaration
//

class RlvActions
{
public:

	/*
	 * Returns true if the user is allowed to start a - P2P or group - conversation with the specified UUID.
	 */
	static bool canStartIM(const LLUUID& idRecipient);								// @startim and @startimto

	/*
	 * Returns true if a - P2P or group - IM session is open with the specified UUID.
	 */
	static bool hasOpenP2PSession(const LLUUID& idAgent);
	static bool hasOpenGroupSession(const LLUUID& idGroup);
};

// ============================================================================

#endif // RLV_ACTIONS_H

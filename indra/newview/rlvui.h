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

#ifndef RLV_UI_H
#define RLV_UI_H

#include "llfloaterreg.h"
#include "llsingleton.h"

#include "rlvdefines.h"

// ============================================================================
// RlvUIEnabler - self-contained class that handles disabling or reenabling certain aspects of the viewer's UI
//

class RlvUIEnabler : public LLSingleton<RlvUIEnabler>
{
protected:
	RlvUIEnabler();
	friend class LLSingleton<RlvUIEnabler>;
	friend class RlvHandler;

	/*
	 * Signal callbacks
	 */
public:
	typedef boost::function<void(bool)> behaviour_handler_t;
	void addBehaviourToggleCallback(ERlvBehaviour eBhvr, behaviour_handler_t cb);
	void onBehaviour(ERlvBehaviour eBhvr, ERlvParamType eType);			// RlvHandler::rlv_behaviour_signal_t

	/*
	 * Behaviour handlers
	 */
protected:
	void onRefreshHoverText();											// showloc, shownames, showhovertext(all|world|hud)
	void onToggleEdit();												// edit
	void onToggleFly();													// fly
	void onToggleRez();													// rez
	void onToggleSetEnv();												// setenv
	void onToggleShowInv(bool fQuitting);								// showinv
	void onToggleShowLoc();												// showloc
	void onToggleShowMinimap();											// showminimap
	void onToggleShowNames(bool fQuitting);								// shownames
	void onToggleShowWorldMap();										// showworldmap
	void onToggleTp();													// tploc and tplm
	void onToggleUnsit();												// unsit
	void onToggleViewXXX();												// viewnote, viewscript, viewtexture
	void onUpdateLoginLastLocation();									// tploc and unsit

	/*
	 * Floater validation callbacks
	 */
protected:
	void addGenericFloaterFilter(const std::string& strFloaterName);
	void removeGenericFloaterFilter(const std::string& strFloaterName);
	bool filterFloaterGeneric(const std::string&, const LLSD&);
	boost::signals2::connection m_ConnFloaterGeneric;

	bool filterFloaterShowLoc(const std::string&, const LLSD& );
	boost::signals2::connection m_ConnFloaterShowLoc;					// showloc
	bool filterFloaterViewXXX(const std::string&, const LLSD&);
	boost::signals2::connection m_ConnFloaterViewXXX;					// viewnote, viewscript, viewtexture

	/*
	 * Helper functions
	 */
public:
	static bool canViewParcelProperties();								// showloc
	static bool canViewRegionProperties();								// showloc
	static bool hasOpenIM(const LLUUID& idAgent);						// shownames
	static bool hasOpenProfile(const LLUUID& idAgent);					// shownames
	static bool isBuildEnabled();										// edit and rez

	/*
	 * Member variables
	 */
protected:
	typedef std::multimap<ERlvBehaviour, behaviour_handler_t> behaviour_handler_map_t;
	behaviour_handler_map_t m_Handlers;

	std::multiset<std::string> m_FilteredFloaters;
};

// ============================================================================
// Inlined member functions

// Checked: 2010-11-02 (RLVa-1.2.2a) | Added: RLVa-1.2.2a
inline void RlvUIEnabler::addBehaviourToggleCallback(ERlvBehaviour eBhvr, behaviour_handler_t cb)
{
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(eBhvr, cb));
}

// ============================================================================

#endif // RLV_UI_H

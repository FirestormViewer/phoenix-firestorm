/** 
 *
 * Copyright (c) 2009-2010, Kitty Barnett
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
#include "llavatarlist.h"				// Avatar list control used by the "Nearby" tab in the "People" sidebar panel
#include "llbottomtray.h"
#include "llbutton.h"
#include "llcallfloater.h"
#include "llinventorypanel.h"
#include "llimview.h"					// LLIMMgr
#include "llmoveview.h"					// Movement panel (contains "Stand" and "Stop Flying" buttons)
#include "llnavigationbar.h"			// Navigation bar
#include "llnotificationsutil.h"
#include "lloutfitslist.h"				// "My Outfits" sidebar panel
#include "llpaneloutfitsinventory.h"	// "My Appearance" sidebar panel
#include "llpanelpeople.h"				// "People" sidebar panel
#include "llpanelprofile.h"				// "Profile" sidebar panel
#include "llpanelwearing.h"				// "Current Outfit" sidebar panel
#include "llparcel.h"
#include "llsidetray.h"
#include "llsidetraypanelcontainer.h"
#include "llsidepanelappearance.h"
#include "lltabcontainer.h"
#include "llteleporthistory.h"
#include "llteleporthistorystorage.h"
#include "lltoolmgr.h"
#include "llviewerparcelmgr.h"
#include "roles_constants.h"			// Group "powers"

#include "rlvui.h"
#include "rlvhandler.h"

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
RlvUIEnabler::RlvUIEnabler()
{
	// Connect us to the behaviour toggle signal
	gRlvHandler.setBehaviourCallback(boost::bind(&RlvUIEnabler::onBehaviour, this, _1, _2));

	// onRefreshHoverText()
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMES, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTALL, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTWORLD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTHUD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));

	// onToggleViewXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWNOTE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWSCRIPT, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWTEXTURE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));

	// onToggleXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_EDIT, boost::bind(&RlvUIEnabler::onToggleEdit, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_FLY, boost::bind(&RlvUIEnabler::onToggleFly, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_REZ, boost::bind(&RlvUIEnabler::onToggleRez, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SETENV, boost::bind(&RlvUIEnabler::onToggleSetEnv, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWINV, boost::bind(&RlvUIEnabler::onToggleShowInv, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onToggleShowLoc, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWMINIMAP, boost::bind(&RlvUIEnabler::onToggleShowMinimap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWNAMES, boost::bind(&RlvUIEnabler::onToggleShowNames, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWWORLDMAP, boost::bind(&RlvUIEnabler::onToggleShowWorldMap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onToggleUnsit, this)));

	// onToggleTp
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onToggleTp, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLM, boost::bind(&RlvUIEnabler::onToggleTp, this)));

	// onUpdateLoginLastLocation
	#ifdef RLV_EXTENSION_STARTLOCATION
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this)));
	#endif // RLV_EXTENSION_STARTLOCATION
}

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onBehaviour(ERlvBehaviour eBhvr, ERlvParamType eType)
{
	// We're only interested in behaviour toggles (ie on->off or off->on)
	if ( ((RLV_TYPE_ADD == eType) && (1 == gRlvHandler.hasBehaviour(eBhvr))) ||
		 ((RLV_TYPE_REMOVE == eType) && (0 == gRlvHandler.hasBehaviour(eBhvr))) )
	{
		for (behaviour_handler_map_t::const_iterator itHandler = m_Handlers.lower_bound(eBhvr), endHandler = m_Handlers.upper_bound(eBhvr);
				itHandler != endHandler; ++itHandler)
		{
			itHandler->second();
		}
	}
}

// ============================================================================

// Checked: 2010-03-02 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onRefreshHoverText()
{
	// Refresh all hover text each time any of the monitored behaviours get set or unset
	LLHUDText::refreshAllObjectText();
}

// Checked: 2010-03-17 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleEdit()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_EDIT);

	if (!fEnable)
	{
		// Turn off "View / Highlight Transparent"
		LLDrawPoolAlpha::sShowDebugAlpha = FALSE;

		// Hide the beacons floater if it's currently visible
		if (LLFloaterReg::floaterInstanceVisible("beacons"))
			LLFloaterReg::hideInstance("beacons");

		// Hide the build floater if it's currently visible
		if (LLToolMgr::instance().inBuildMode())
			LLToolMgr::instance().toggleBuildMode();
	}

	// Enable/disable the "Build" bottom tray button (but only if edit *and* rez restricted)
	LLBottomTray::getInstance()->getChild<LLButton>("build_btn")->setEnabled(isBuildEnabled());

	// Start or stop filtering opening the beacons floater
	if (!fEnable)
		addGenericFloaterFilter("beacons");
	else
		removeGenericFloaterFilter("beacons");
}

// Checked: 2010-03-02 (RLVa-1.2.0d) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleFly()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_FLY);

	// Drop the avie out of the sky if currently flying and restricted
	if ( (!fEnable) && (gAgent.getFlying()) )
		gAgent.setFlying(FALSE);

	// Force an update since the status only updates when the current parcel changes [see LLFloaterMove::postBuild()]
	LLFloaterMove::sUpdateFlyingStatus();
}

// Checked: 2010-09-11 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
void RlvUIEnabler::onToggleRez()
{
	// Enable/disable the "Build" bottom tray button
	LLBottomTray::getInstance()->getChild<LLButton>("build_btn")->setEnabled(isBuildEnabled());
}

// Checked: 2010-03-17 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleSetEnv()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SETENV);

	const std::string strEnvFloaters[] = { "env_day_cycle", /*"env_post_process",*/ "env_settings", "env_water", "env_windlight" };
	for (int idxFloater = 0, cntFloater = sizeof(strEnvFloaters) / sizeof(std::string); idxFloater < cntFloater; idxFloater++)
	{
		if (!fEnable)
		{
			// Hide the floater if it's currently visible
			if (LLFloaterReg::floaterInstanceVisible(strEnvFloaters[idxFloater]))
				LLFloaterReg::hideInstance(strEnvFloaters[idxFloater]);

			addGenericFloaterFilter(strEnvFloaters[idxFloater]);
		}
		else
		{
			removeGenericFloaterFilter(strEnvFloaters[idxFloater]);
		}
	}
}

// Checked: 2010-09-07 (RLVa-1.2.1a) | Modified: RLVa-1.2.1a
void RlvUIEnabler::onToggleShowInv()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWINV);

	//
	// Enable/disable the "My Inventory" sidebar tab
	//
	LLSideTray* pSideTray = LLSideTray::getInstance(); 
	RLV_ASSERT(pSideTray);
	if (pSideTray)
	{
		// If the inventory sidebar tab is currently undocked we need to redock it first
		if ( (!fEnable) && (!pSideTray->isTabAttached("sidebar_inventory")) )
		{
			// NOTE: redocking will expand the sidebar and select the redocked tab so we need enough information to undo that again
			bool fCollapsed = pSideTray->getCollapsed();
			const LLPanel* pActiveTab = pSideTray->getActiveTab();

			pSideTray->toggleTabDocked("sidebar_inventory");

			if (pActiveTab)
				pSideTray->selectTabByName(pActiveTab->getName());
			if (fCollapsed)
				pSideTray->collapseSideBar();
		}

		LLButton* pInvBtn = pSideTray->getButtonFromName("sidebar_inventory");
		RLV_ASSERT(pInvBtn);
		if (pInvBtn) 
			pInvBtn->setEnabled(fEnable);

		// When disabling, switch to the "Home" sidebar tab if "My Inventory" is currently active
		// NOTE: when collapsed 'isPanelActive' will return FALSE even if the panel is currently active so we have to sidestep that
		const LLPanel* pActiveTab = pSideTray->getActiveTab();
		if ( (!fEnable) && (pActiveTab) && ("sidebar_inventory" == pActiveTab->getName()) )
		{
			pSideTray->selectTabByName("sidebar_home");	// Using selectTabByName() instead of showPanel() will prevent it from expanding
			if (pSideTray->getCollapsed())
				pSideTray->collapseSideBar();			// Fixes a button highlighting glitch when changing the active tab while collapsed
		}

		// Start or stop filtering opening the inventory sidebar tab
		RLV_ASSERT_DBG( (fEnable) || (!m_ConnSidePanelInventory.connected()) );
		if (!fEnable)
			m_ConnSidePanelInventory = pSideTray->setValidateCallback(boost::bind(&RlvUIEnabler::canOpenSidebarTab, this, RLV_BHVR_SHOWINV, "sidebar_inventory", _1, _2));
		else
			m_ConnSidePanelInventory.disconnect();
	}

	//
	// Enable/disable the "My Outfits" panel on the "My Appearance" sidebar tab
	//
	LLPanelOutfitsInventory* pAppearancePanel = LLPanelOutfitsInventory::findInstance();
	RLV_ASSERT(pAppearancePanel);
	if (pAppearancePanel)
	{
		LLTabContainer* pAppearanceTabs = pAppearancePanel->getAppearanceTabs();
		LLOutfitsList* pMyOutfitsPanel = pAppearancePanel->getMyOutfitsPanel();
		if ( (pAppearanceTabs) && (pMyOutfitsPanel) )
		{
			S32 idxTab = pAppearanceTabs->getIndexForPanel(pMyOutfitsPanel);
			RLV_ASSERT(-1 != idxTab);
			pAppearanceTabs->enableTabButton(idxTab, fEnable);

			// When disabling, switch to the COF tab if "My Outfits" is currently active
			if ( (!fEnable) && (pAppearanceTabs->getCurrentPanelIndex() == idxTab) )
				pAppearanceTabs->selectTabPanel(pAppearancePanel->getCurrentOutfitPanel());
		}

		LLSidepanelAppearance* pCOFPanel = pAppearancePanel->getAppearanceSP();
		RLV_ASSERT(pCOFPanel);
		if ( (!fEnable) && (pCOFPanel) && (pCOFPanel->isOutfitEditPanelVisible()) )
		{
			// TODO-RLVa: we should really just be collapsing the "Add more..." inventory panel (and disable the button)
			pCOFPanel->showOutfitsInventoryPanel();
		}
	}

	//
	// When disabling, hide any old style inventory floaters that may be open
	//
	if (!fEnable)
	{
		LLFloaterReg::const_instance_list_t lFloaters = LLFloaterReg::getFloaterList("inventory");
		for (LLFloaterReg::const_instance_list_t::const_iterator itFloater = lFloaters.begin(); itFloater != lFloaters.end(); ++itFloater)
			(*itFloater)->closeFloater();
	}

	// Filter out (or stop filtering) newly spawned old style inventory floaters
	if (!fEnable)
		addGenericFloaterFilter("inventory");
	else
		removeGenericFloaterFilter("inventory");
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
void RlvUIEnabler::onToggleShowLoc()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);

	// RELEASE-RLVa: [SL-2.0.1] Check that the code below still evaluates to *only* LLNavigationBar::instance().mCmbLocation->refresh()
	LLNavigationBar::instance().handleLoginComplete();

	if (!fEnable)
	{
		// Hide the "About Land" floater if it's currently visible
		if (LLFloaterReg::floaterInstanceVisible("about_land"))
			LLFloaterReg::hideInstance("about_land");
		// Hide the "Region / Estate" floater if it's currently visible
		if (LLFloaterReg::floaterInstanceVisible("region_info"))
			LLFloaterReg::hideInstance("region_info");
		// Hide the "God Tools" floater if it's currently visible
		if (LLFloaterReg::floaterInstanceVisible("god_tools"))
			LLFloaterReg::hideInstance("god_tools");

		//
		// Manipulate the teleport history
		//

		// If the last entry in the persistent teleport history matches the current teleport history entry then we should remove it
		LLTeleportHistory* pTpHistory = LLTeleportHistory::getInstance();
		LLTeleportHistoryStorage* pTpHistoryStg = LLTeleportHistoryStorage::getInstance();
		RLV_ASSERT( (pTpHistory) && (pTpHistoryStg) && (pTpHistory->getItems().size() > 0) && (pTpHistory->getCurrentItemIndex() >= 0) );
		if ( (pTpHistory) && (pTpHistory->getItems().size() > 0) && (pTpHistory->getCurrentItemIndex() >= 0) &&
			 (pTpHistoryStg) && (pTpHistory->getItems().size() > 0) )
		{
			const LLTeleportHistoryItem& tpItem = pTpHistory->getItems().back();
			const LLTeleportHistoryPersistentItem& tpItemStg = pTpHistoryStg->getItems().back();
			if (pTpHistoryStg->compareByTitleAndGlobalPos(tpItemStg, LLTeleportHistoryPersistentItem(tpItem.mTitle, tpItem.mGlobalPos)))
			{
				// TODO-RLVa: [RLVa-1.2.2] Is there a reason why LLTeleportHistoryStorage::removeItem() doesn't trigger history changed?
				pTpHistoryStg->removeItem(pTpHistoryStg->getItems().size() - 1);
				pTpHistoryStg->mHistoryChangedSignal(-1);
			}
		}

		// Clear the current location in the teleport history
		if (pTpHistory)
			pTpHistory->updateCurrentLocation(gAgent.getPositionGlobal());
	}
	else
	{
		// Reset the current location in the teleport history (also takes care of adding it to the persistent teleport history)
		LLTeleportHistory* pTpHistory = LLTeleportHistory::getInstance();
		if ( (pTpHistory) && (NULL != gAgent.getRegion()) )
			pTpHistory->updateCurrentLocation(gAgent.getPositionGlobal());
	}

	// Start or stop filtering the "About Land" and "Region / Estate" floaters
	if ( (!fEnable) && (!m_ConnFloaterShowLoc.connected()) )
		m_ConnFloaterShowLoc = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterShowLoc, this, _1, _2));
	else if ( (fEnable) && (m_ConnFloaterShowLoc.connected()) )
		m_ConnFloaterShowLoc.disconnect();
}

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowMinimap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWMINIMAP);

	// Start or stop filtering opening the mini-map
	if (!fEnable)
		addGenericFloaterFilter("mini_map");
	else
		removeGenericFloaterFilter("mini_map");

	// Hide the mini-map if it's currently visible (or restore it if it was previously visible)
	static bool fPrevVisibile = false;
	if ( (!fEnable) && ((fPrevVisibile = LLFloaterReg::floaterInstanceVisible("mini_map"))) )
		LLFloaterReg::hideFloaterInstance("mini_map");
	else if ( (fEnable) && (fPrevVisibile) )
		LLFloaterReg::showFloaterInstance("mini_map");

	// Enable/disable the "Mini-Map" bottom tray button
	const LLBottomTray* pTray = LLBottomTray::getInstance();
	LLView* pBtnView = (pTray) ? pTray->getChildView("mini_map_btn") : NULL;
	if (pBtnView)
		pBtnView->setEnabled(fEnable);
}

// Checked: 2010-06-05 (RLVa-1.2.0d) | Modified: RLVa-1.2.0d
void RlvUIEnabler::onToggleShowNames()
{
	// Refresh the nearby people list
	LLPanelPeople* pPeoplePanel = dynamic_cast<LLPanelPeople*>(LLSideTray::getInstance()->getPanel("panel_people"));
	RLV_ASSERT( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) );
	if ( (pPeoplePanel) && (pPeoplePanel->getNearbyList()) )
		pPeoplePanel->getNearbyList()->refreshNames();

	// Refresh the speaker list
	LLCallFloater* pCallFloater = LLFloaterReg::findTypedInstance<LLCallFloater>("voice_controls");
	if (pCallFloater)
		pCallFloater->getAvatarCallerList()->refreshNames();
}

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowWorldMap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP);

	// Hide the world map if it's currently visible
	if ( (!fEnable) && (LLFloaterReg::floaterInstanceVisible("world_map")) )
		LLFloaterReg::hideFloaterInstance("world_map");

	// Enable/disable the "Map" bottom tray button
	const LLBottomTray* pTray = LLBottomTray::getInstance();
	LLView* pBtnView = (pTray) ? pTray->getChildView("world_map_btn") : NULL;
	if (pBtnView)
		pBtnView->setEnabled(fEnable);

	// Start or stop filtering opening the world map
	if (!fEnable)
		addGenericFloaterFilter("world_map");
	else
		removeGenericFloaterFilter("world_map");
}

// Checked: 2010-08-22 (RLVa-1.2.1a) | Added: RLVa-1.2.1a
void RlvUIEnabler::onToggleTp()
{
	// Disable the navigation bar "Home" button if both @tplm=n *and* @tploc=n restricted
	LLButton* pNavBarHomeBtn = LLNavigationBar::getInstance()->getChild<LLButton>("home_btn");
	RLV_ASSERT(pNavBarHomeBtn);
	if (pNavBarHomeBtn)
		pNavBarHomeBtn->setEnabled(!(gRlvHandler.hasBehaviour(RLV_BHVR_TPLM) && gRlvHandler.hasBehaviour(RLV_BHVR_TPLOC)));
}

// Checked: 2010-03-01 (RLVa-1.2.0c) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleUnsit()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_UNSIT);

	LLPanelStandStopFlying* pPanelStand = LLPanelStandStopFlying::getInstance();
	RLV_ASSERT(pPanelStand);
	if (pPanelStand)
	{
		LLButton* pBtnStand = pPanelStand->getChild<LLButton>("stand_btn");
		RLV_ASSERT(pBtnStand);
		if (pBtnStand)
			pBtnStand->setEnabled(fEnable);
	}
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleViewXXX()
{
	// If any of the three are still active then we keep filtering
	bool fEnable = (!gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) && 
		(!gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) && (!gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE));

	// Start or stop filtering opening the preview floaters
	if ( (!fEnable) && (!m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterViewXXX, this, _1, _2));
	else if ( (fEnable) && (m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX.disconnect();
}

// Checked: 2010-04-01 (RLVa-1.2.0c) | Added: RLVa-1.2.0c
void RlvUIEnabler::onUpdateLoginLastLocation()
{
	RlvSettings::updateLoginLastLocation();
}

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
inline void RlvUIEnabler::addGenericFloaterFilter(const std::string& strFloaterName)
{
	m_FilteredFloaters.insert(strFloaterName);

	if (!m_ConnFloaterGeneric.connected())
		m_ConnFloaterGeneric = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterGeneric, this, _1, _2));
}

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
inline void RlvUIEnabler::removeGenericFloaterFilter(const std::string& strFloaterName)
{
	std::multiset<std::string>::iterator itFloater = m_FilteredFloaters.find(strFloaterName);
	RLV_ASSERT_DBG(itFloater != m_FilteredFloaters.end());
	m_FilteredFloaters.erase(itFloater);

	RLV_ASSERT_DBG(m_ConnFloaterGeneric.connected());
	if (m_FilteredFloaters.empty())
		m_ConnFloaterGeneric.disconnect();
}

// Checked: 2010-02-28 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterGeneric(const std::string& strName, const LLSD&)
{
	return m_FilteredFloaters.find(strName) == m_FilteredFloaters.end();
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
bool RlvUIEnabler::filterFloaterShowLoc(const std::string& strName, const LLSD&)
{
	if ("about_land" == strName)
		return canViewParcelProperties();
	else if ("region_info" == strName)
		return canViewRegionProperties();
	else if ("god_tools" == strName)
		return false;
	return true;
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterViewXXX(const std::string& strName, const LLSD&)
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) && ("preview_notecard" == strName) )
	{
		notifyBlockedViewXXX(LLAssetType::AT_NOTECARD);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) && (("preview_script" == strName) || ("preview_scriptedit" == strName)) )
	{
		notifyBlockedViewXXX(LLAssetType::AT_SCRIPT);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE)) && ("preview_texture" == strName) )
	{
		notifyBlockedViewXXX(LLAssetType::AT_TEXTURE);
		return false;
	}
	return true;
}

// ============================================================================

// Checked: 2010-03-01 (RLVa-1.2.0a) | Added: RLVa-1.2.0a
bool RlvUIEnabler::canOpenSidebarTab(ERlvBehaviour eBhvrFilter, const std::string& strNameFilter, LLUICtrl* pCtrl, const LLSD& sdParam)
{
	return (!gRlvHandler.hasBehaviour(eBhvrFilter)) || (strNameFilter != sdParam.asString());
}

// ============================================================================

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::notifyBlockedViewXXX(LLAssetType::EType assetType)
{
	LLStringUtil::format_map_t argsMsg; std::string strMsg = RlvStrings::getString(RLV_STRING_BLOCKED_VIEWXXX);
	argsMsg["[TYPE]"] = LLAssetType::lookup(assetType);
	LLStringUtil::format(strMsg, argsMsg);

	LLSD argsNotify;
	argsNotify["MESSAGE"] = strMsg;
	LLNotificationsUtil::add("SystemMessageTip", argsNotify);
}

// ============================================================================

// Checked: 2010-04-20 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
bool RlvUIEnabler::canViewParcelProperties()
{
	// We'll allow "About Land" as long as the user has the ability to return prims (through ownership or through group powers)
	bool fShow = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		// RELEASE-RLVa: [SL-2.0.0] Check that ôpening the "About Land" floater still sets focus to the current parcel is none is selected
		LLParcelSelection* pParcelSel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
		LLParcel* pParcel = (pParcelSel) ? pParcelSel->getParcel() : LLViewerParcelMgr::getInstance()->getAgentParcel();
		if ( ((pParcelSel) && (pParcelSel->hasOthersSelected())) || (!pParcel) )
			return false;

		// Ideally we could just use LLViewerParcelMgr::isParcelOwnedByAgent(), but that has that sneaky exemption for "god-like"
		// RELEASE-RLVa: [SL-2.0.0] Check that the "inlining" of "isParcelOwnedByAgent" and "LLAgent::hasPowerInGroup()" still matches
		const LLUUID& idOwner = pParcel->getOwnerID();
		if ( (idOwner != gAgent.getID()) )
		{
			// LLAgent::hasPowerInGroup() has it too so copy/paste from there
			S32 count = gAgent.mGroups.count();
			for (S32 i = 0; i < count; ++i)
			{
				if (gAgent.mGroups.get(i).mID == idOwner)
				{
					fShow |= ((gAgent.mGroups.get(i).mPowers & GP_LAND_RETURN) > 0);
					break;
				}
			}
		}
		else
		{
			fShow = true;
		}
	}
	return fShow;
}

// Checked: 2010-04-20 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
bool RlvUIEnabler::canViewRegionProperties()
{
	// We'll allow "Region / Estate" if the user is either the sim owner or an estate manager
	bool fShow = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		// [See LLRegion::canManageEstate() but without the "god-like" exception]
		const LLViewerRegion* pRegion = gAgent.getRegion();
		if (pRegion)
			fShow = (pRegion->isEstateManager()) || (pRegion->getOwner() == gAgent.getID());
	}
	return fShow;
}

// Checked: 2010-04-20 (RLVa-1.2.0f) | Added: RLVa-1.2.0f
bool RlvUIEnabler::hasOpenIM(const LLUUID& idAgent)
{
	LLUUID idSession = LLIMMgr::computeSessionID(IM_NOTHING_SPECIAL, idAgent);
	return (NULL != LLFloaterReg::findInstance("impanel", idSession));
}

// Checked: 2010-04-20 (RLVa-1.2.0h) | Added: RLVa-1.2.0f
bool RlvUIEnabler::hasOpenProfile(const LLUUID& idAgent)
{
	// NOTE: despite its name this function is used to determine whether or not we'll obfuscate names for inventory drops on nearby agents
	//   -> SL-2.1.0 added the ability to "share" inventory by dropping it on the avatar picker floater so we should check for that
	//   -> we can drag/drop inventory onto calling cards but probably noone knows about it and it would be too involved to check for that
	// TODO-RLVa: [RLVa-1.2.1] Check the avatar picker as well

	// Check if the user has the specified agent's profile open
	LLSideTray* pSideTray = LLSideTray::getInstance(); 
	RLV_ASSERT(pSideTray);
	if ( (pSideTray) && (!pSideTray->getCollapsed()) )
	{
		/*const*/ LLSideTrayPanelContainer* pPanelContainer = dynamic_cast</*const*/ LLSideTrayPanelContainer*>(pSideTray->getActivePanel());
		if (pPanelContainer)
		{
			const LLPanel* pActivePanel = pPanelContainer->getCurrentPanel();
			if ( (pActivePanel) && ("panel_profile_view" == pActivePanel->getName()) )
			{
				const LLPanelProfile* pProfilePanel = dynamic_cast<const LLPanelProfile*>(pActivePanel);
				RLV_ASSERT(pProfilePanel);
				if (pProfilePanel)
					return (idAgent == pProfilePanel->getAvatarId());
			}
		}
	}
	return false;
}

// Checked: 2010-09-11 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
bool RlvUIEnabler::isBuildEnabled()
{
	return (!gRlvHandler.hasBehaviour(RLV_BHVR_EDIT) || !gRlvHandler.hasBehaviour(RLV_BHVR_REZ)) && LLToolMgr::getInstance()->canEdit();
}

// ============================================================================

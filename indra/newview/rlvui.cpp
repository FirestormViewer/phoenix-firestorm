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
#include "llavataractions.h"			// LLAvatarActions::profileVisible()
#include "llfloatersidepanelcontainer.h"
#include "llhudtext.h"					// LLHUDText::refreshAllObjectText()
#include "llimview.h"					// LLIMMgr::computeSessionID()
#include "llmoveview.h"					// Movement panel (contains "Stand" and "Stop Flying" buttons)
#include "llnavigationbar.h"			// Navigation bar
#include "llparcel.h"
#include "llpaneltopinfobar.h"
#include "llteleporthistory.h"
#include "lltoolmgr.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvoavatar.h"
#include "roles_constants.h"			// Group "powers"

#include "rlvui.h"
#include "rlvhandler.h"
#include "rlvextensions.h"

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
RlvUIEnabler::RlvUIEnabler()
{
	// Connect us to the behaviour toggle signal
	gRlvHandler.setBehaviourToggleCallback(boost::bind(&RlvUIEnabler::onBehaviourToggle, this, _1, _2));

	// onRefreshHoverText()
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTALL, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTWORLD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWHOVERTEXTHUD, boost::bind(&RlvUIEnabler::onRefreshHoverText, this)));

	// onToggleMovement
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_FLY, boost::bind(&RlvUIEnabler::onToggleMovement, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_ALWAYSRUN, boost::bind(&RlvUIEnabler::onToggleMovement, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TEMPRUN, boost::bind(&RlvUIEnabler::onToggleMovement, this)));

	// onToggleViewXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWNOTE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWSCRIPT, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_VIEWTEXTURE, boost::bind(&RlvUIEnabler::onToggleViewXXX, this)));

	// onToggleXXX
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWLOC, boost::bind(&RlvUIEnabler::onToggleShowLoc, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWMINIMAP, boost::bind(&RlvUIEnabler::onToggleShowMinimap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_SHOWWORLDMAP, boost::bind(&RlvUIEnabler::onToggleShowWorldMap, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onToggleUnsit, this)));

	// onToggleTp
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onToggleTp, this)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLM, boost::bind(&RlvUIEnabler::onToggleTp, this)));

	// onUpdateLoginLastLocation
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_TPLOC, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this, _1)));
	m_Handlers.insert(std::pair<ERlvBehaviour, behaviour_handler_t>(RLV_BHVR_UNSIT, boost::bind(&RlvUIEnabler::onUpdateLoginLastLocation, this, _1)));
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onBehaviourToggle(ERlvBehaviour eBhvr, ERlvParamType eType)
{
	bool fQuitting = LLApp::isQuitting();
	for (behaviour_handler_map_t::const_iterator itHandler = m_Handlers.lower_bound(eBhvr), endHandler = m_Handlers.upper_bound(eBhvr);
			itHandler != endHandler; ++itHandler)
	{
		itHandler->second(fQuitting);
	}
}

// ============================================================================

// Checked: 2010-03-02 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onRefreshHoverText()
{
	// Refresh all hover text each time any of the monitored behaviours get set or unset
	LLHUDText::refreshAllObjectText();
}

// Checked: 2010-03-02 (RLVa-1.4.0a) | Modified: RLVa-1.4.0a
void RlvUIEnabler::onToggleMovement()
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_FLY)) && (gAgent.getFlying()) )
		gAgent.setFlying(FALSE);
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_ALWAYSRUN)) && (gAgent.getAlwaysRun()) )
		gAgent.clearAlwaysRun();
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_TEMPRUN)) && (gAgent.getTempRun()) )
		gAgent.clearTempRun();

	// Force an update since the status only updates when the current parcel changes [see LLFloaterMove::postBuild()]
	LLFloaterMove::sUpdateMovementStatus();
}

// Checked: 2010-04-22 (RLVa-1.2.0f) | Modified: RLVa-1.2.0f
void RlvUIEnabler::onToggleShowLoc()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);

	if (LLNavigationBar::instanceExists())
		LLNavigationBar::instance().refreshLocationCtrl();
	if (LLPanelTopInfoBar::instanceExists())
		LLPanelTopInfoBar::instance().update();

	if (!fEnable)
	{
		// Hide the "About Land" floater if it's currently visible
		if (LLFloaterReg::instanceVisible("about_land"))
			LLFloaterReg::hideInstance("about_land");
		// Hide the "Region / Estate" floater if it's currently visible
		if (LLFloaterReg::instanceVisible("region_info"))
			LLFloaterReg::hideInstance("region_info");
		// Hide the "God Tools" floater if it's currently visible
		if (LLFloaterReg::instanceVisible("god_tools"))
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
	{
		m_ConnFloaterShowLoc = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterShowLoc, this, _1, _2));
		m_ConnPanelShowLoc = LLFloaterSidePanelContainer::setValidateCallback(boost::bind(&RlvUIEnabler::filterPanelShowLoc, this, _1, _2, _3));
	}
	else if ( (fEnable) && (m_ConnFloaterShowLoc.connected()) )
	{
		m_ConnFloaterShowLoc.disconnect();
		m_ConnPanelShowLoc.disconnect();
	}
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowMinimap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWMINIMAP);

	// Start or stop filtering showing the mini-map floater
	if (!fEnable)
		addGenericFloaterFilter("mini_map");
	else
		removeGenericFloaterFilter("mini_map");

	// Hide the mini-map floater if it's currently visible (or restore it if it was previously visible)
	static bool fPrevVisibile = false;
	if ( (!fEnable) && ((fPrevVisibile = LLFloaterReg::instanceVisible("mini_map"))) )
		LLFloaterReg::hideInstance("mini_map");
	else if ( (fEnable) && (fPrevVisibile) )
		LLFloaterReg::showInstance("mini_map");

	// Break/reestablish the visibility connection for the nearby people panel embedded minimap instance
	LLPanel* pPeoplePanel = LLFloaterSidePanelContainer::getPanel("people", "panel_people");
	LLPanel* pNetMapPanel = (pPeoplePanel) ? pPeoplePanel->findChild<LLPanel>("Net Map Panel", TRUE) : NULL;
	RLV_ASSERT( (pPeoplePanel) && (pNetMapPanel) );
	if (pNetMapPanel)
	{
		pNetMapPanel->setMakeVisibleControlVariable( (fEnable) ? gSavedSettings.getControl("NearbyListShowMap").get() : NULL);
		// Reestablishing the visiblity connection will show the panel if needed so we only need to take care of hiding it when needed
		if ( (!fEnable) && (pNetMapPanel->getVisible()) )
			pNetMapPanel->setVisible(false);
	}
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleShowWorldMap()
{
	bool fEnable = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWWORLDMAP);

	// Hide the world map if it's currently visible
	if ( (!fEnable) && (LLFloaterReg::instanceVisible("world_map")) )
		LLFloaterReg::hideInstance("world_map");

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
	LLButton* pNavBarHomeBtn = LLNavigationBar::getInstance()->findChild<LLButton>("home_btn");
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
		LLButton* pBtnStand = pPanelStand->findChild<LLButton>("stand_btn");
		RLV_ASSERT(pBtnStand);
		if (pBtnStand)
			pBtnStand->setEnabled(fEnable);
	}
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
void RlvUIEnabler::onToggleViewXXX()
{
	// If any of the three are still active then we keep filtering
	bool fHasViewXXX = (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) ||
		(gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) || (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE));

	// Start or stop filtering opening the preview floaters
	if ( (fHasViewXXX) && (!m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterViewXXX, this, _1, _2));
	else if ( (!fHasViewXXX) && (m_ConnFloaterViewXXX.connected()) )
		m_ConnFloaterViewXXX.disconnect();
}

// Checked: 2010-04-01 (RLVa-1.2.0c) | Added: RLVa-1.2.0c
void RlvUIEnabler::onUpdateLoginLastLocation(bool fQuitting)
{
	if (!fQuitting)
		RlvSettings::updateLoginLastLocation();
}

// ============================================================================

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::addGenericFloaterFilter(const std::string& strFloaterName)
{
	m_FilteredFloaters.insert(strFloaterName);

	if (!m_ConnFloaterGeneric.connected())
		m_ConnFloaterGeneric = LLFloaterReg::setValidateCallback(boost::bind(&RlvUIEnabler::filterFloaterGeneric, this, _1, _2));
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
void RlvUIEnabler::removeGenericFloaterFilter(const std::string& strFloaterName)
{
	std::multiset<std::string>::iterator itFloater = m_FilteredFloaters.find(strFloaterName);
	RLV_ASSERT_DBG(itFloater != m_FilteredFloaters.end());
	m_FilteredFloaters.erase(itFloater);

	RLV_ASSERT_DBG(m_ConnFloaterGeneric.connected());
	if (m_FilteredFloaters.empty())
		m_ConnFloaterGeneric.disconnect();
}

// Checked: 2010-02-28 (RLVa-1.4.0a) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterGeneric(const std::string& strName, const LLSD&)
{
	return m_FilteredFloaters.end() == m_FilteredFloaters.find(strName);
}

// Checked: 2010-04-22 (RLVa-1.4.5) | Added: RLVa-1.2.0
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

// Checked: 2012-02-07 (RLVa-1.4.5) | Added: RLVa-1.4.5
bool RlvUIEnabler::filterPanelShowLoc(const std::string& strFloater, const std::string&, const LLSD& sdKey)
{
	if ("places" == strFloater)
	{
		const std::string strType = sdKey["type"].asString();
		if ("create_landmark" == strType)
			return false;
		else if ("agent" == strType)
			return canViewParcelProperties();
	}
	return true;
}

// Checked: 2010-03-01 (RLVa-1.2.0b) | Added: RLVa-1.2.0a
bool RlvUIEnabler::filterFloaterViewXXX(const std::string& strName, const LLSD&)
{
	if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWNOTE)) && ("preview_notecard" == strName) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_NOTECARD);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWSCRIPT)) && (("preview_script" == strName) || ("preview_scriptedit" == strName)) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_SCRIPT);
		return false;
	}
	else if ( (gRlvHandler.hasBehaviour(RLV_BHVR_VIEWTEXTURE)) && ("preview_texture" == strName) )
	{
		RlvUtil::notifyBlockedViewXXX(LLAssetType::AT_TEXTURE);
		return false;
	}
	return true;
}

// ============================================================================

// Checked: 2012-02-09 (RLVa-1.4.5) | Modified: RLVa-1.4.5
bool RlvUIEnabler::canViewParcelProperties()
{
	// We'll allow "About Land" as long as the user has the ability to return prims (through ownership or through group powers)
	bool fShow = !gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC);
	if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWLOC))
	{
		// RELEASE-RLVa: [SL-3.2] Check that opening the "About Land" floater still sets focus to the current parcel is none is selected
		const LLParcel* pParcel = NULL;
		if (LLViewerParcelMgr::getInstance()->selectionEmpty())
		{
			pParcel = LLViewerParcelMgr::getInstance()->getAgentParcel();
		}
		else
		{
			LLParcelSelection* pParcelSel = LLViewerParcelMgr::getInstance()->getFloatingParcelSelection();
			if (pParcelSel->hasOthersSelected())
				return false;
			pParcel = pParcelSel->getParcel();
		}

		// Ideally we could just use LLViewerParcelMgr::isParcelOwnedByAgent(), but that has that sneaky exemption for "god-like"
		if (pParcel)
		{
			const LLUUID& idOwner = pParcel->getOwnerID();
			if ( (idOwner != gAgent.getID()) )
			{
				S32 count = gAgent.mGroups.size();
				for (S32 i = 0; i < count; ++i)
				{
					if (gAgent.mGroups.at(i).mID == idOwner)
					{
						fShow = ((gAgent.mGroups.at(i).mPowers & GP_LAND_RETURN) > 0);
						break;
					}
				}
			}
			else
			{
				fShow = true;
			}
		}
	}
	return fShow;
}

// Checked: 2010-04-20 (RLVa-1.4.5) | Modified: RLVa-1.2.0
bool RlvUIEnabler::canViewRegionProperties()
{
	// We'll allow "Region / Estate" if the user is either the region owner or an estate manager
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

// Checked: 2011-11-04 (RLVa-1.4.4a) | Modified: RLVa-1.4.4a
bool RlvUIEnabler::hasOpenProfile(const LLUUID& idAgent)
{
	//   -> SL-2.1.0 added the ability to "share" inventory by dropping it on the avatar picker floater so we should check for that
	//   -> we can drag/drop inventory onto calling cards but probably noone knows about it and it would be too involved to check for that
	// TODO-RLVa: [RLVa-1.2.1] Check the avatar picker as well

	// Check if the user has the specified agent's profile open
	return LLAvatarActions::profileVisible(idAgent);
}

// Checked: 2010-09-11 (RLVa-1.2.1d) | Added: RLVa-1.2.1d
bool RlvUIEnabler::isBuildEnabled()
{
	return (gAgent.canEditParcel()) && ((!gRlvHandler.hasBehaviour(RLV_BHVR_EDIT)) || (!gRlvHandler.hasBehaviour(RLV_BHVR_REZ)));
}

bool RlvUIEnabler::isBuildEnabledOrActive()
{
	return LLToolMgr::instance().inEdit() || isBuildEnabled();
}

// ============================================================================

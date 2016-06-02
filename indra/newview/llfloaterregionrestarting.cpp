/** 
 * @file llfloaterregionrestarting.cpp
 * @brief Shows countdown timer during region restart
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterregionrestarting.h"

#include "llfloaterreg.h"
#include "lluictrl.h"
#include "llagent.h"
#include "llagentcamera.h"
#include "llviewercontrol.h" // <FS:PP> FIRE-12900, FIRE-12901: gSavedSettings, make screen shaking optional
#include "llviewerwindow.h"

// [SL:KB] - Patch: UI-RegionRestart | Checked: 2014-03-15 (Catznip-3.6)
#include "llcombobox.h"
#include "llinventoryfunctions.h"
#include "llinventorymodel.h"
#include "llinventorymodelbackgroundfetch.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
// [/SL:KB]
#include "llwindow.h"

static S32 sSeconds;
static U32 sShakeState;

LLFloaterRegionRestarting::LLFloaterRegionRestarting(const LLSD& key) :
	LLFloater(key),
	LLEventTimer(1.f)
{
	mName = (std::string)key["NAME"];
	sSeconds = (LLSD::Integer)key["SECONDS"];
}

LLFloaterRegionRestarting::~LLFloaterRegionRestarting()
{
	mRegionChangedConnection.disconnect();
}

BOOL LLFloaterRegionRestarting::postBuild()
{
	mRegionChangedConnection = gAgent.addRegionChangedCallback(boost::bind(&LLFloaterRegionRestarting::regionChange, this));

// [SL:KB] - Patch: UI-RegionRestart | Checked: 2014-03-15 (Catznip-3.6)
	const LLUUID idLandmarks = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
	LLInventoryModelBackgroundFetch::instance().start(idLandmarks);

	getChild<LLComboBox>("landmark combo")->setPrearrangeCallback(boost::bind(&LLFloaterRegionRestarting::refreshLandmarkList, this));
	getChild<LLUICtrl>("teleport_btn")->setCommitCallback(boost::bind(&LLFloaterRegionRestarting::onTeleportClicked, this));
// [/SL:KB]

	LLStringUtil::format_map_t args;
	std::string text;

	args["[NAME]"] = mName;
	text = getString("RegionName", args);
	LLTextBox* textbox = getChild<LLTextBox>("region_name");
	textbox->setValue(text);

	sShakeState = SHAKE_START;

	refresh();

	return TRUE;
}

void LLFloaterRegionRestarting::regionChange()
{
	close();
}

BOOL LLFloaterRegionRestarting::tick()
{
	refresh();

	return FALSE;
}

void LLFloaterRegionRestarting::refresh()
{
	LLStringUtil::format_map_t args;
	std::string text;

	args["[SECONDS]"] = llformat("%d", sSeconds);
	getChild<LLTextBox>("restart_seconds")->setValue(getString("RestartSeconds", args));

	sSeconds = sSeconds - 1.f;
	if(sSeconds < 0.0f)
	{
		sSeconds = 0.f;
	}
}

// [SL:KB] - Patch: UI-RegionRestart | Checked: 2014-03-15 (Catznip-3.6)
void LLFloaterRegionRestarting::onOpen(const LLSD& key)
{
	LLFloater::onOpen(key);

	refreshLandmarkList();

	LLWindow* viewer_window = gViewerWindow->getWindow();
	if (viewer_window)
	{
		viewer_window->flashIcon(5.f);
	}
}

void LLFloaterRegionRestarting::onTeleportClicked()
{
	LLComboBox* pCombo = findChild<LLComboBox>("landmark combo");
	if (pCombo)
	{
		const LLUUID idAsset = pCombo->getSelectedValue().asUUID();
		if (idAsset.notNull())
			gAgent.teleportViaLandmark(idAsset);
	}
}

void LLFloaterRegionRestarting::refreshLandmarkList()
{
	LLComboBox* pCombo = findChild<LLComboBox>("landmark combo");
	if (!pCombo)
		return;

	// Delete all but the placehold entry
	S32 cntItem = pCombo->getItemCount();
	if (cntItem > 1)
	{
		pCombo->selectItemRange(1, -1);
		pCombo->operateOnSelection(LLCtrlListInterface::OP_DELETE);
	}
	
	// Add landmarks from inventory (match the logic from the world map floater)
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	LLFindLandmarks is_landmark(true, false /*gSavedSettings.getBOOL("WorldMapFilterSelfLandmarks")*/);
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items, LLInventoryModel::EXCLUDE_TRASH, is_landmark);
	
	std::sort(items.begin(), items.end(), LLViewerInventoryItem::comparePointers());

	for (LLInventoryModel::item_array_t::const_iterator itItem = items.begin(); itItem != items.end(); ++itItem)
	{
		const LLViewerInventoryItem* pItem = *itItem;
		pCombo->addSimpleElement(pItem->getName(), ADD_BOTTOM, pItem->getAssetUUID());
	}

	pCombo->selectFirstItem();
}
// [/SL:KB]

void LLFloaterRegionRestarting::draw()
{
	LLFloater::draw();

	// <FS:PP> FIRE-12900, FIRE-12901: Make screen shaking optional
	static LLCachedControl<bool> fsNoScreenShakeOnRegionRestart(gSavedSettings, "FSNoScreenShakeOnRegionRestart");
	if (fsNoScreenShakeOnRegionRestart)
	{
		return;
	}
	// </FS:PP>

	double SHAKE_INTERVAL = 0.025;
	double SHAKE_TOTAL_DURATION = 1.8; // the length of the default alert tone for this
	const F32 SHAKE_INITIAL_MAGNITUDE = 1.5f;
	const F32 SHAKE_HORIZONTAL_BIAS = 0.25f;
	F32 time_shaking;
	
	if(SHAKE_START == sShakeState)
	{
			mShakeTimer.setTimerExpirySec(SHAKE_INTERVAL);
			sShakeState = SHAKE_LEFT;
			mShakeIterations = 0;
			mShakeMagnitude = SHAKE_INITIAL_MAGNITUDE;
	}

	if(SHAKE_DONE != sShakeState && mShakeTimer.hasExpired())
	{
		gAgentCamera.unlockView();

		switch(sShakeState)
		{
			case SHAKE_LEFT:
				gAgentCamera.setPanLeftKey(mShakeMagnitude * SHAKE_HORIZONTAL_BIAS);
				sShakeState = SHAKE_UP;
				break;

			case SHAKE_UP:
				gAgentCamera.setPanUpKey(mShakeMagnitude);
				sShakeState = SHAKE_RIGHT;
				break;

			case SHAKE_RIGHT:
				gAgentCamera.setPanRightKey(mShakeMagnitude * SHAKE_HORIZONTAL_BIAS);
				sShakeState = SHAKE_DOWN;
				break;

			case SHAKE_DOWN:
				gAgentCamera.setPanDownKey(mShakeMagnitude);
				mShakeIterations++;
				time_shaking = SHAKE_INTERVAL * (mShakeIterations * 4 /* left, up, right, down */);
				if(SHAKE_TOTAL_DURATION <= time_shaking)
				{
					sShakeState = SHAKE_DONE;
					mShakeMagnitude = 0.0f;
				}
				else
				{
					sShakeState = SHAKE_LEFT;
					F32 percent_done_shaking = (SHAKE_TOTAL_DURATION - time_shaking) / SHAKE_TOTAL_DURATION;
					mShakeMagnitude = SHAKE_INITIAL_MAGNITUDE * (percent_done_shaking * percent_done_shaking); // exponential decay
				}
				break;

			default:
				break;
		}
		mShakeTimer.setTimerExpirySec(SHAKE_INTERVAL);
	}
}

void LLFloaterRegionRestarting::close()
{
	LLFloaterRegionRestarting* floaterp = LLFloaterReg::findTypedInstance<LLFloaterRegionRestarting>("region_restarting");

	if (floaterp)
	{
		floaterp->closeFloater();
	}
}

void LLFloaterRegionRestarting::updateTime(S32 time)
{
	sSeconds = time;
	sShakeState = SHAKE_START;
}

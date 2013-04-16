/**
 * @file fsfloaterradar.h
 * @brief Firestorm radar floater implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Copyright (c) 2013 Ansariel Hiller @ Second Life
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
 * The Phoenix Firestorm Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_FLOATERRADAR_H
#define FS_FLOATERRADAR_H

#include "llfloater.h"

#include "fsradar.h"
#include "fsradarlistctrl.h"
#include "llcallingcard.h" // for avatar tracker
#include "llvoiceclient.h"

class LLFilterEditor;
class LLMenuButton;

class FSFloaterRadar 
	: public LLFloater
	, public LLVoiceClientStatusObserver
{
	LOG_CLASS(FSFloaterRadar);
public:
	FSFloaterRadar(const LLSD &);
	virtual ~FSFloaterRadar();

	/*virtual*/ BOOL 	postBuild();
	/*virtual*/ void	onOpen(const LLSD& key);

	// Implements LLVoiceClientStatusObserver::onChange() to enable call buttons
	// when voice is available
	/*virtual*/ void onChange(EStatusType status, const std::string &channelURI, bool proximal);

	static void	onRadarNameFmtClicked(const LLSD& userdata);
	static bool	radarNameFmtCheck(const LLSD& userdata);
	static void	onRadarReportToClicked(const LLSD& userdata);
	static bool	radarReportToCheck(const LLSD& userdata);

	void updateNearby(const std::vector<LLSD>& entries, const LLSD& stats);

private:
	void					updateButtons();
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;
	void					buttonSetEnabled(const std::string& btn_name, bool enabled);
	void					buttonSetAction(const std::string& btn_name, const commit_signal_t::slot_type& cb);

	// UI callbacks
	void					onFilterEdit(const std::string& search_string);
	void					onViewProfileButtonClicked();
	void					onAddFriendButtonClicked();
	void					onImButtonClicked();
	void					onCallButtonClicked();
	void					onTeleportButtonClicked();
	void					onShareButtonClicked();
	void					onRadarListCommitted();
	void					onRadarListDoubleClicked();
	void					onGearMenuItemClicked(const LLSD& userdata);

	LLFilterEditor*			mFilterEditor;
	FSRadarListCtrl*		mRadarList;
	LLNetMap*				mMiniMap;

	LLHandle<LLView>		mRadarGearMenuHandle;

	FSRadar::Updater*		mButtonsUpdater;

	LLMenuButton*			mRadarGearButton;

	std::string				mFilterSubString;
	std::string				mFilterSubStringOrig;

	boost::signals2::connection mUpdateSignalConnection;
};

#endif // FS_FLOATERRADAR_H

/**
 * @file fspanelradar.h
 * @brief Firestorm radar panel implementation
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

#ifndef FS_PANELRADAR_H
#define FS_PANELRADAR_H

#include "llpanel.h"

#include "fsradar.h"
#include "fsradarlistctrl.h"
#include "llcallingcard.h" // for avatar tracker

class LLButton;
class LLFilterEditor;
class LLMenuButton;
class LLNetMap;

class FSPanelRadar
	: public LLPanel
{
	LOG_CLASS(FSPanelRadar);
	friend class LLPanelPeople;

public:
	FSPanelRadar();
	virtual ~FSPanelRadar();

	/*virtual*/ BOOL 	postBuild();
	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
	/*virtual*/ bool hasAccelerators() const { return true; }

	void					requestUpdate();
	LLUUID					getCurrentItemID() const;
	void					getCurrentItemIDs(uuid_vec_t& selected_uuids) const;

	typedef boost::signals2::signal<void()> change_callback_t;
	boost::signals2::connection setChangeCallback(const change_callback_t::slot_type& cb)
	{
		return mChangeSignal.connect(cb);
	}

	typedef boost::function<bool()> visible_check_function_t;
	void					setVisibleCheckFunction(const visible_check_function_t& func)
	{
		mVisibleCheckFunction = func;
	}

private:
	void					updateButtons();
	void					updateList(const std::vector<LLSD>& entries, const LLSD& stats);

	// UI callbacks
	void					onAddFriendButtonClicked();
	void					onRadarListCommitted();
	void					onRadarListDoubleClicked();
	void					onOptionsMenuItemClicked(const LLSD& userdata);
	void					onFilterEdit(const std::string& search_string);
	void					onGearButtonClicked(LLUICtrl* btn);
	void					onColumnDisplayModeChanged();
	void					onColumnVisibilityChecked(const LLSD& userdata);
	bool					onEnableColumnVisibilityChecked(const LLSD& userdata);

	FSRadarListCtrl*		mRadarList;
	LLNetMap*				mMiniMap;
	LLButton*				mRadarGearButton;
	LLButton*				mAddFriendButton;
	LLMenuButton*			mOptionsButton;
	LLFilterEditor*			mFilterEditor;

	LLHandle<LLView>		mOptionsMenuHandle;

	FSRadar::Updater*		mButtonsUpdater;

	std::string				mFilterSubString;
	std::string				mFilterSubStringOrig;

	std::map<std::string, U32> mColumnBits;
	S32						mLastResizeDelta;

	// Slot connection for FSRadar updates
	boost::signals2::connection mUpdateSignalConnection;

	boost::signals2::connection mFSRadarColumnConfigConnection;

	// Signal for subscribers interested in updates (selection/list update)
	change_callback_t		mChangeSignal;

	// Optional function called to check if radar panel is visible
	visible_check_function_t	mVisibleCheckFunction;
};

#endif // FS_PANELRADAR_H

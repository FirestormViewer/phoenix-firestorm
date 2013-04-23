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

#include "fspanelradar.h"
#include "fsradar.h"
#include "llvoiceclient.h"

class LLButton;
class LLFilterEditor;

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

private:
	void					updateButtons();

	// UI callbacks
	void					onFilterEdit(const std::string& search_string);
	void					onViewProfileButtonClicked();
	void					onAddFriendButtonClicked();
	void					onImButtonClicked();
	void					onCallButtonClicked();
	void					onTeleportButtonClicked();
	void					onShareButtonClicked();

	FSPanelRadar*			mRadarPanel;
	LLFilterEditor*			mFilterEditor;

	LLButton*				mProfileButton;
	LLButton*				mShareButton;
	LLButton*				mIMButton;
	LLButton*				mCallButton;
	LLButton*				mTeleportButton;

	std::string				mFilterSubString;
	std::string				mFilterSubStringOrig;

	boost::signals2::connection mRadarChangeConnection;
};

#endif // FS_FLOATERRADAR_H

/** 
 * @file fsdiscordconnect.h
 * @brief Connection to Discord
 * @author liny@pinkfox.xyz
 *
 * $LicenseInfo:firstyear=2013&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2019 Liny Odell @ Second Life
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
 */

#ifndef FS_FSDISCORDCONNECT_H
#define FS_FSDISCORDCONNECT_H

#include "llsingleton.h"
#include "llcoros.h"
#include "lleventcoro.h"

class LLEventPump;

/**
 * @class LLTwitterConnect
 *
 * Manages authentication to, and interaction with, a web service allowing the
 * the viewer to post status updates and upload photos to Twitter.
 */
class FSDiscordConnect : public LLSingleton<FSDiscordConnect>
{
	LLSINGLETON(FSDiscordConnect);
	LOG_CLASS(FSDiscordConnect);
public:
    enum EConnectionState
	{
		DISCORD_NOT_CONNECTED = 0,
		DISCORD_CONNECTION_IN_PROGRESS = 1,
		DISCORD_CONNECTED = 2,
		DISCORD_CONNECTION_FAILED = 3,
		DISCORD_DISCONNECTING = 4
	};

	~FSDiscordConnect();

	void connectToDiscord();																				// Initiate the complete Discord connection. Please use checkConnectionToDiscord() in normal use.
	void disconnectFromDiscord();																			// Disconnect from the Discord service.
    void checkConnectionToDiscord(bool auto_connect = false);												// Check if connected to the Discord service. If not, call connectToDiscord().
	
	void storeInfo(const LLSD& info);
	const LLSD& getInfo() const;
	void clearInfo();
    
    void setConnectionState(EConnectionState connection_state);
	void setConnected(bool connected);
	bool isConnected() { return mConnected; }
    EConnectionState getConnectionState() { return mConnectionState; }

	void updateRichPresence();

	bool Tick(const LLSD&);

private:

    EConnectionState mConnectionState;
	BOOL mConnected;
	LLSD mInfo;
	bool mRefreshInfo;
	
	static boost::scoped_ptr<LLEventPump> sStateWatcher;
	static boost::scoped_ptr<LLEventPump> sInfoWatcher;
	static boost::scoped_ptr<LLEventPump> sContentWatcher;

    void discordConnectCoro();
    void discordDisconnectCoro();
    void discordConnectedCoro(bool autoConnect);

	bool checkMarkerFile();
	void setMarkerFile();
	void clearMarkerFile();

	std::string mMarkerFilename;
};

#endif // FS_FSDISCORDCONNECT_H

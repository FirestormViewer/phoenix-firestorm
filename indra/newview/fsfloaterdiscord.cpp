/** 
* @file fsfloaterdiscord.cpp
* @brief Implementation of fsfloaterdiscord.cpp
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

#include "llviewerprecompiledheaders.h"

#include "fsfloaterdiscord.h"

#include "llagent.h"
#include "llagentui.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "fsdiscordconnect.h"
#include "llfloaterreg.h"
#include "lltrans.h"
#include "llviewerregion.h"
#include "llviewercontrol.h"
#include "lltabcontainer.h"

#include "boost/algorithm/string/case_conv.hpp"

////////////////////////
//FSFloaterDiscord///////
////////////////////////

void FSFloaterDiscord::onVisibilityChange(BOOL visible)
{
	if(visible)
	{
		LLEventPumps::instance().obtain("DiscordConnectState").stopListening("FSDiscordAccountPanel");
		LLEventPumps::instance().obtain("DiscordConnectState").listen("FSDiscordAccountPanel", boost::bind(&FSFloaterDiscord::onDiscordConnectStateChange, this, _1));

		LLEventPumps::instance().obtain("DiscordConnectInfo").stopListening("FSDiscordAccountPanel");
		LLEventPumps::instance().obtain("DiscordConnectInfo").listen("FSDiscordAccountPanel", boost::bind(&FSFloaterDiscord::onDiscordConnectInfoChange, this));

		LLSD info = FSDiscordConnect::instance().getInfo();

		if (info.has("name"))
		{
			mAccountNameLabel->setText(info["name"].asString());
		}

		//Connected
		if(FSDiscordConnect::instance().isConnected())
		{
			showConnectedLayout();
		}
		//Check if connected (show disconnected layout in meantime)
		else
		{
			showDisconnectedLayout();
		}
		if ((FSDiscordConnect::instance().getConnectionState() == FSDiscordConnect::DISCORD_NOT_CONNECTED) ||
			(FSDiscordConnect::instance().getConnectionState() == FSDiscordConnect::DISCORD_CONNECTION_FAILED))
		{
			FSDiscordConnect::instance().checkConnectionToDiscord();
		}
	}
	else
	{
		LLEventPumps::instance().obtain("DiscordConnectState").stopListening("FSDiscordAccountPanel");
		LLEventPumps::instance().obtain("DiscordConnectInfo").stopListening("FSDiscordAccountPanel");
	}
}

void FSFloaterDiscord::onAllow()
{
	gSavedPerAccountSettings.setBOOL("FSEnableDiscordIntegration", mAllowCheckbox->get());
}

bool FSFloaterDiscord::onDiscordConnectStateChange(const LLSD& data)
{
	if(FSDiscordConnect::instance().isConnected())
	{
		mAccountCaptionLabel->setText(getString("discord_connected"));
		showConnectedLayout();
	}
	else
	{
		mAccountCaptionLabel->setText(getString("discord_disconnected"));
		showDisconnectedLayout();
	}

	return false;
}

bool FSFloaterDiscord::onDiscordConnectInfoChange()
{
	LLSD info = FSDiscordConnect::instance().getInfo();

	if(info.has("name"))
	{
		mAccountNameLabel->setText(info["name"].asString());
	}

	return false;
}

void FSFloaterDiscord::showConnectButton()
{
	if(!mConnectButton->getVisible())
	{
		mConnectButton->setVisible(TRUE);
		mDisconnectButton->setVisible(FALSE);
	}
}

void FSFloaterDiscord::hideConnectButton()
{
	if(mConnectButton->getVisible())
	{
		mConnectButton->setVisible(FALSE);
		mDisconnectButton->setVisible(TRUE);
	}
}

void FSFloaterDiscord::showDisconnectedLayout()
{
	mAccountCaptionLabel->setText(getString("discord_disconnected"));
	mAccountNameLabel->setText(std::string(""));
	showConnectButton();
}

void FSFloaterDiscord::showConnectedLayout()
{
	mAccountCaptionLabel->setText(getString("discord_connected"));
	hideConnectButton();
}

void FSFloaterDiscord::onConnect()
{
	FSDiscordConnect::instance().checkConnectionToDiscord(true);
}

void FSFloaterDiscord::onDisconnect()
{
	FSDiscordConnect::instance().disconnectFromDiscord();
}

FSFloaterDiscord::FSFloaterDiscord(const LLSD& key) : LLFloater(key),
mStatusText(nullptr)
{
	mCommitCallbackRegistrar.add("FSDiscord.Connect", boost::bind(&FSFloaterDiscord::onConnect, this));
	mCommitCallbackRegistrar.add("FSDiscord.Disconnect", boost::bind(&FSFloaterDiscord::onDisconnect, this));
	mCommitCallbackRegistrar.add("FSDiscord.Allow", boost::bind(&FSFloaterDiscord::onAllow, this));
	mCommitCallbackRegistrar.add("FSDiscord.Combo", boost::bind(&FSFloaterDiscord::onCombo, this));
	mCommitCallbackRegistrar.add("FSDiscord.Add", boost::bind(&FSFloaterDiscord::onAdd, this));
	mCommitCallbackRegistrar.add("FSDiscord.Rem", boost::bind(&FSFloaterDiscord::onRemove, this));

	setVisibleCallback(boost::bind(&FSFloaterDiscord::onVisibilityChange, this, _2));
}

void FSFloaterDiscord::onCombo()
{
	gSavedPerAccountSettings.setU32("FSMaxSharedMaturity", mMaturityCombo->getSelectedValue().asInteger());
}

void FSFloaterDiscord::onAdd()
{
	std::string name = mBlacklistEntry->getText();
	LLStringUtil::trim(name);
	if (name.empty())
	{
		return;
	}
	std::string name_lower = boost::algorithm::to_lower_copy(name);
	std::vector<LLScrollListItem*> items = mBlacklistedNames->getAllData();
	std::vector<LLScrollListItem*>::iterator itor;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		std::string tmp = (*itor)->getValue().asString();
		boost::algorithm::to_lower(tmp);
		if (tmp == name_lower)
		{
			return;
		}
	}
	mBlacklistedNames->addSimpleElement(name);
	LLSD save;
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		save.append((*itor)->getValue());
	}
	save.append(name);
	gSavedPerAccountSettings.setLLSD("FSBlacklistedRegionNames", save);
}

void FSFloaterDiscord::onRemove()
{
	std::vector<LLScrollListItem*> items = mBlacklistedNames->getAllData();
	std::vector<LLScrollListItem*>::iterator itor;
	LLSD save = LLSD::emptyArray();
	for (itor = items.begin(); itor != items.end(); ++itor)
	{
		if ((*itor)->getSelected())
		{
			continue;
		}
		save.append((*itor)->getValue());
	}
	mBlacklistedNames->deleteAllItems();
	for (LLSD::array_const_iterator iter = save.beginArray();
		iter != save.endArray();
		iter++)
	{
		mBlacklistedNames->addSimpleElement(iter->asString());
	}
	gSavedPerAccountSettings.setLLSD("FSBlacklistedRegionNames", save);
}

void FSFloaterDiscord::onClose(bool app_quitting)
{
	if (app_quitting)
	{
		std::vector<LLScrollListItem*> items = mBlacklistedNames->getAllData();
		std::vector<LLScrollListItem*>::iterator itor;
		LLSD save = LLSD::emptyArray();
		for (itor = items.begin(); itor != items.end(); ++itor)
		{
			if ((*itor)->getSelected())
			{
				continue;
			}
			save.append((*itor)->getValue());
		}
		gSavedPerAccountSettings.setLLSD("FSBlacklistedRegionNames", save);
	}
	LLFloater::onClose(app_quitting);
}

BOOL FSFloaterDiscord::postBuild()
{
	mAccountCaptionLabel = getChild<LLTextBox>("account_caption_label");
	mAccountNameLabel = getChild<LLTextBox>("account_name_label");
	mDisconnectButton = getChild<LLButton>("disconnect_btn");
	mConnectButton = getChild<LLButton>("connect_btn");
	mAllowCheckbox = getChild<LLCheckBoxCtrl>("startup_check");
	mMaturityCombo = getChild<LLComboBox>("maturity_desired_combobox");
	mBlacklistedNames = getChild<LLScrollListCtrl>("blacklisted_names");
	mBlacklistEntry = getChild<LLLineEditor>("blacklist_entry");
	mAddBlacklist = getChild<LLButton>("blacklist_entry_add");
	mRemBlacklist = getChild<LLButton>("blacklist_entry_rem");

	LLSD list = gSavedPerAccountSettings.getLLSD("FSBlacklistedRegionNames");
	for (LLSD::array_const_iterator iter = list.beginArray();
		iter != list.endArray();
		iter++)
	{
		mBlacklistedNames->addSimpleElement(iter->asString());
	}

	mMaturityCombo->selectByValue((LLSD::Integer)gSavedPerAccountSettings.getU32("FSMaxSharedMaturity"));

	mAllowCheckbox->set(gSavedPerAccountSettings.getBOOL("FSEnableDiscordIntegration"));

	// Connection status widgets
	mStatusText = getChild<LLTextBox>("connection_status_text");

	return LLFloater::postBuild();
}

void FSFloaterDiscord::draw()
{
	if (mStatusText)
	{
		mStatusText->setVisible(false);
		FSDiscordConnect::EConnectionState connection_state = FSDiscordConnect::instance().getConnectionState();
		std::string status_text;
		
		switch (connection_state)
		{
		case FSDiscordConnect::DISCORD_NOT_CONNECTED:
			// No status displayed when first opening the panel and no connection done
			break;
		case FSDiscordConnect::DISCORD_CONNECTION_IN_PROGRESS:
			// Connection loading indicator
			mStatusText->setVisible(true);
			status_text = getString("SocialDiscordConnecting");
			mStatusText->setValue(status_text);
			break;
		case FSDiscordConnect::DISCORD_CONNECTED:
			// When successfully connected, no message is displayed
			break;
		case FSDiscordConnect::DISCORD_CONNECTION_FAILED:
			// Error connecting to the service
			mStatusText->setVisible(true);
			status_text = getString("SocialDiscordErrorConnecting");
			mStatusText->setValue(status_text);
			break;
		case FSDiscordConnect::DISCORD_DISCONNECTING:
			// Disconnecting loading indicator
			mStatusText->setVisible(true);
			status_text = getString("SocialDiscordDisconnecting");
			mStatusText->setValue(status_text);
			break;
		}
	}
	LLFloater::draw();
}


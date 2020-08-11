/** 
 * @file fschatoptionsmenu.cpp
 * @brief Handler for chat options menu
 *
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2015 Ansariel Hiller
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

#include "llviewerprecompiledheaders.h"

#include "fschatoptionsmenu.h"

#include "fsfloaterim.h"
#include "fsfloaternearbychat.h"
#include "llfloaterreg.h"
#include "llfloatersidepanelcontainer.h"
#include "llviewercontrol.h"

void FSChatOptionsMenu::onMenuItemClick(const LLSD& userdata, LLUICtrl* source)
{
	const std::string option = userdata.asString();

	if (option == "blocklist")
	{
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			LLFloaterReg::toggleInstance("fs_blocklist");
		}
		else
		{
			LLPanel* panel = LLFloaterSidePanelContainer::getPanel("people", "panel_people");
			if (!panel)
			{
				return;
			}

			if (panel->isInVisibleChain())
			{
				LLFloaterReg::hideInstance("people");
			}
			else
			{
				LLFloaterSidePanelContainer::showPanel("people", "panel_people", LLSD().with("people_panel_tab_name", "blocked_panel"));
			}
		}
	}
	else if (option == "font_size_small")
	{
		gSavedSettings.setS32("ChatFontSize", 0);
	}
	else if (option == "font_size_medium")
	{
		gSavedSettings.setS32("ChatFontSize", 1);
	}
	else if (option == "font_size_large")
	{
		gSavedSettings.setS32("ChatFontSize", 2);
	}
	else if (option == "font_size_huge")
	{
		gSavedSettings.setS32("ChatFontSize", 3);
	}
	else if (option == "new_message_notification")
	{
		if (dynamic_cast<FSFloaterNearbyChat*>(source))
		{
			gSavedSettings.setBOOL("FSNotifyUnreadChatMessages", !gSavedSettings.getBOOL("FSNotifyUnreadChatMessages"));
		}
		else if (dynamic_cast<FSFloaterIM*>(source))
		{
			gSavedSettings.setBOOL("FSNotifyUnreadIMMessages", !gSavedSettings.getBOOL("FSNotifyUnreadIMMessages"));
		}
	}
}

bool FSChatOptionsMenu::onMenuItemEnable(const LLSD& userdata, LLUICtrl* source)
{
	const std::string option = userdata.asString();

	if (option == "typing_chevron")
	{
		FSFloaterIM* floater = dynamic_cast<FSFloaterIM*>(source);
		return (floater && floater->isP2PChat());
	}
	else if (option == "show_channel_selection")
	{
		return gSavedSettings.getBOOL("FSNearbyChatbar");
	}
	else if (option == "show_send_button")
	{
		return gSavedSettings.getBOOL("FSNearbyChatbar");
	}

	return false;
}

bool FSChatOptionsMenu::onMenuItemVisible(const LLSD& userdata, LLUICtrl* source)
{
	const std::string option = userdata.asString();

	if (option == "typing_chevron")
	{
		return (dynamic_cast<FSFloaterIM*>(source) != NULL);
	}
	else if (option == "show_chat_bar")
	{
		return (dynamic_cast<FSFloaterNearbyChat*>(source) != NULL);
	}
	else if (option == "show_channel_selection")
	{
		return (dynamic_cast<FSFloaterNearbyChat*>(source) != NULL);
	}
	else if (option == "show_send_button")
	{
		return (dynamic_cast<FSFloaterNearbyChat*>(source) != NULL);
	}
	else if (option == "show_im_send_button")
	{
		return (dynamic_cast<FSFloaterIM*>(source) != NULL);
	}
	else if (option == "show_mini_icons")
	{
		return !gSavedSettings.getBOOL("PlainTextChatHistory");
	}

	return false;
}

bool FSChatOptionsMenu::onMenuItemCheck(const LLSD& userdata, LLUICtrl* source)
{
	const std::string option = userdata.asString();

	if (option == "blocklist")
	{
		if (gSavedSettings.getBOOL("FSUseStandaloneBlocklistFloater"))
		{
			return LLFloaterReg::instanceVisible("fs_blocklist");
		}
	}
	else if (option == "font_size_small")
	{
		return (gSavedSettings.getS32("ChatFontSize") == 0);
	}
	else if (option == "font_size_medium")
	{
		return (gSavedSettings.getS32("ChatFontSize") == 1);
	}
	else if (option == "font_size_large")
	{
		return (gSavedSettings.getS32("ChatFontSize") == 2);
	}
	else if (option == "font_size_huge")
	{
		return (gSavedSettings.getS32("ChatFontSize") == 3);
	}
	else if (option == "new_message_notification")
	{
		if (dynamic_cast<FSFloaterNearbyChat*>(source))
		{
			return gSavedSettings.getBOOL("FSNotifyUnreadChatMessages");
		}
		else if (dynamic_cast<FSFloaterIM*>(source))
		{
			return gSavedSettings.getBOOL("FSNotifyUnreadIMMessages");
		}
	}

	return false;
}

/** 
 * @file fsnearbychatcontrol.cpp
 * @brief Nearby chat input control implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2012, Zi Ree @ Second Life
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

#include "fsnearbychatcontrol.h"
#include "fsnearbychathub.h"
// <FS:Ansariel> [FS communication UI]
//#include "llfloaternearbychat.h"
#include "fsfloaternearbychat.h"
// </FS:Ansariel> [FS communication UI]
// #include "llviewercontrol.h"
// #include "llviewerwindow.h"
// #include "llrootview.h"
//#include "llchatitemscontainerctrl.h"
// #include "lliconctrl.h"
#include "llspinctrl.h"
// #include "llfloatersidepanelcontainer.h"
// #include "llfocusmgr.h"
// #include "lllogchat.h"
// #include "llresizebar.h"
// #include "llresizehandle.h"
#include "llmenugl.h"
#include "llviewermenu.h"//for gMenuHolder

// #include "llnearbychathandler.h"
// #include "llnearbychatbar.h"	// <FS:Zi> Remove floating chat bar
// #include "llchannelmanager.h"

#include "llagent.h" 			// gAgent
#include "llagentcamera.h"		// gAgentCamera
// #include "llchathistory.h"
// #include "llstylemap.h"

// #include "llavatarnamecache.h"

// #include "lldraghandle.h"

// #include "llnearbychatbar.h"	// <FS:Zi> Remove floating chat bar
// #include "llfloaterreg.h"
// #include "lltrans.h"

// IM
// #include "llbutton.h"
// #include "lllayoutstack.h"

// #include "llimfloatercontainer.h"
// #include "llimfloater.h"
#include "lllineeditor.h"

//AO - includes for textentry
#include "rlvhandler.h"
#include "llcommandhandler.h"
#include "llkeyboard.h"
#include "llgesturemgr.h"
#include "llmultigesture.h"
#include "llautoreplace.h"

// #include "llconsole.h"
// #include "fscontactsfloater.h"

// <FS:Zi> Moved nearby chat functionality here for now
// #include "chatbar_as_cmdline.h"
// #include "llanimationstates.h"	// ANIM_AGENT_WHISPER, ANIM_AGENT_TALK, ANIM_AGENT_SHOUT
// #include "llviewerstats.h"
// </FS:Zi>
// <FS:CR> FIRE-3192 - Name Prediction
#include "llworld.h"

#define NAME_PREDICTION_MINIMUM_LENGTH 2
// </FS:CR>

static LLDefaultChildRegistry::Register<FSNearbyChatControl> r("fs_nearby_chat_control");

struct LLChatTypeTrigger {
	std::string name;
	EChatType type;
};

static LLChatTypeTrigger sChatTypeTriggers[] = {
	{ "/whisper"	, CHAT_TYPE_WHISPER},
	{ "/shout"	, CHAT_TYPE_SHOUT}
};

FSNearbyChatControl::FSNearbyChatControl(const FSNearbyChatControl::Params& p) :
	LLLineEditor(p)
{
	setAutoreplaceCallback(boost::bind(&LLAutoReplace::autoreplaceCallback, LLAutoReplace::getInstance(), _1, _2));
	setKeystrokeCallback(onKeystroke,this);
	FSNearbyChat::instance().registerChatBar(this);

	setEnableLineHistory(TRUE);
	setIgnoreArrowKeys( FALSE );
	setCommitOnFocusLost( FALSE );
	setRevertOnEsc( FALSE );
	setIgnoreTab( TRUE );
	setReplaceNewlinesWithSpaces( FALSE );
	setPassDelete( TRUE );
	setFont(LLViewerChat::getChatFont());

	// Register for font change notifications
	LLViewerChat::setFontChangedCallback(boost::bind(&FSNearbyChatControl::setFont, this, _1));
}

FSNearbyChatControl::~FSNearbyChatControl()
{
}

void FSNearbyChatControl::onKeystroke(LLLineEditor* caller,void* userdata)
{
	LLWString raw_text = caller->getWText();
	
	// Can't trim the end, because that will cause autocompletion
	// to eat trailing spaces that might be part of a gesture.
	LLWStringUtil::trimHead(raw_text);
	S32 length = raw_text.length();

	// Get the currently selected channel from the channel spinner in the nearby chat bar, if present and used.
	// NOTE: Parts of the gAgent.startTyping() code are duplicated in 3 places:
	// - llnearbychatbar.cpp
	// - llchatbar.cpp
	// - llnearbychat.cpp
	// So be sure to look in all three places if changes are needed. This needs to be addressed at some point.
	// -Zi
	S32 channel=0;
	if (gSavedSettings.getBOOL("FSNearbyChatbar") &&
		gSavedSettings.getBOOL("FSShowChatChannel"))
	{
		// <FS:Ansariel> [FS communication UI]
		//channel = (S32)(LLFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		channel = (S32)(FSFloaterNearbyChat::getInstance()->getChild<LLSpinCtrl>("ChatChannel")->get());
		// </FS:Ansariel> [FS communication UI]
	}
	// -Zi

	//	if( (length > 0) && (raw_text[0] != '/') )  // forward slash is used for escape (eg. emote) sequences
	// [RLVa:KB] - Checked: 2010-03-26 (RLVa-1.2.0b) | Modified: RLVa-1.0.0d
	if ( (length > 0) && (raw_text[0] != '/') && (!(gSavedSettings.getBOOL("AllowMUpose") && raw_text[0] == ':')) && (!gRlvHandler.hasBehaviour(RLV_BHVR_REDIRCHAT)) )
		// [/RLVa:KB]
	{
		// only start typing animation if we are chatting without / on channel 0 -Zi
		if(channel==0)
			gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}
	
	KEY key = gKeyboard->currentKey();
	
	// Ignore "special" keys, like backspace, arrows, etc.
	if (length > 1 
		&& raw_text[0] == '/'
		// <FS:Ansariel / Holy Gavenkrantz> Optional gesture autocomplete
		//&& key < KEY_SPECIAL)
		&& key < KEY_SPECIAL
		&& gSavedSettings.getBOOL("FSChatbarGestureAutoCompleteEnable"))
		// </FS:Ansariel / Holy Gavenkrantz> Optional gesture autocomplete
	{
		// we're starting a gesture, attempt to autocomplete
		
		std::string utf8_trigger = wstring_to_utf8str(raw_text);
		std::string utf8_out_str(utf8_trigger);
		
		if (LLGestureMgr::instance().matchPrefix(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			caller->setText(utf8_trigger + rest_of_match); // keep original capitalization for user-entered part
			S32 outlength = caller->getLength(); // in characters
			
			// Select to end of line, starting from the character
			// after the last one the user typed.
			caller->setSelection(length, outlength);
		}
		else if (matchChatTypeTrigger(utf8_trigger, &utf8_out_str))
		{
			std::string rest_of_match = utf8_out_str.substr(utf8_trigger.size());
			caller->setText(utf8_trigger + rest_of_match + " "); // keep original capitalization for user-entered part
			caller->setCursorToEnd();
		}
	}
// <FS:CR> FIRE-3192 - Predictive name completion, based on code by Satomi Ahn
	static LLCachedControl<bool> sNameAutocomplete(gSavedSettings, "FSChatBarNamePrediction");
	if (length > NAME_PREDICTION_MINIMUM_LENGTH && sNameAutocomplete && key < KEY_SPECIAL)
	{
		std::string text = caller->getText();
		S32 cur_pos = caller->getCursor();
		if (cur_pos && (text.at(cur_pos - 1) != ' ' || caller->hasSelection()))
		{
			// Get a list of avatars within range
			std::vector<LLUUID> avatar_ids;
			std::vector<LLVector3d> positions;
			LLWorld::getInstance()->getAvatars(&avatar_ids, &positions, gAgent.getPositionGlobal(), gSavedSettings.getF32("NearMeRange"));
			
			if (avatar_ids.empty()) return; // Nobody's in range!
			
			// Parse text for a pattern to search
			std::string prefix = text.substr(0, cur_pos); // Text before search string
			std::string suffix = text.substr(cur_pos, text.length() - cur_pos); // Text after search string
			U32 last_space = prefix.rfind(" ");
			std::string pattern = prefix.substr(last_space + 1, prefix.length() - last_space - 1); // Search pattern
			
			prefix = prefix.substr(0, last_space + 1);
			std::string match_pattern = "";
			
			if (pattern.size() < NAME_PREDICTION_MINIMUM_LENGTH) return;

			match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space -1);
			prefix = prefix.substr(0, last_space + 1);
			LLStringUtil::toLower(pattern);
			
			std::string name;
			bool found = false;
			bool full_name = false;
			std::vector<LLUUID>::iterator iter = avatar_ids.begin();
			
			if (last_space != std::string::npos && !prefix.empty())
			{
				last_space = prefix.substr(0, prefix.length() - 2).rfind(" ");
				match_pattern = prefix.substr(last_space + 1, prefix.length() - last_space -1);
				prefix = prefix.substr(0, last_space + 1);
				// prepare search pattern
				std::string full_pattern(match_pattern + pattern);
				LLStringUtil::toLower(full_pattern);
				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if ((bool)gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(full_pattern) == 0);
					}
				}
			}
			
			if (found)
			{
				full_name = true; // ignore OnlyFirstName in case we want to disambiguate
			}
			else if (!pattern.empty()) // if first search did not work, try matching with last word before cursor only
			{
				prefix += match_pattern; // first part of the pattern wasn't a pattern, so keep it in prefix
				LLStringUtil::toLower(pattern);
				iter = avatar_ids.begin();
				// Look for a match
				while (iter != avatar_ids.end() && !found)
				{
					if ((bool)gCacheName->getFullName(*iter++, name))
					{
						if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
						{
							name = RlvStrings::getAnonym(name);
						}
						LLStringUtil::toLower(name);
						found = (name.find(pattern) == 0);
					}
				}
			}
			
			// if we found something by either method, replace the pattern by the avatar name
			if (found)
			{
				std::string name;
				gCacheName->getFullName(*(iter - 1), name);
				if (gRlvHandler.hasBehaviour(RLV_BHVR_SHOWNAMES))
				{
					prefix += RlvStrings::getAnonym(name) + " ";
				}
				else
				{
					if (!full_name)
					{
						U32 space = name.find(" ");
						if (space != std::string::npos)
							name = name.substr(0, space);
					}
						
					prefix += name + " ";
				}
				caller->setText(prefix + suffix);
				caller->setSelection(prefix.length(), cur_pos);
			}
		}
	}
// </FS:CR>
}

BOOL FSNearbyChatControl::matchChatTypeTrigger(const std::string& in_str, std::string* out_str)
{
	U32 in_len = in_str.length();
	S32 cnt = sizeof(sChatTypeTriggers) / sizeof(*sChatTypeTriggers);
	
	for (S32 n = 0; n < cnt; n++)
	{
		if (in_len > sChatTypeTriggers[n].name.length())
			continue;
		
		std::string trigger_trunc = sChatTypeTriggers[n].name;
		LLStringUtil::truncate(trigger_trunc, in_len);
		
		if (!LLStringUtil::compareInsensitive(in_str, trigger_trunc))
		{
			*out_str = sChatTypeTriggers[n].name;
			return TRUE;
		}
	}
	
	return FALSE;
}

// send our focus status to the LLNearbyChat hub
void FSNearbyChatControl::onFocusReceived()
{
	FSNearbyChat::instance().setFocusedInputEditor(this,TRUE);
	LLLineEditor::onFocusReceived();
}

void FSNearbyChatControl::onFocusLost()
{
	FSNearbyChat::instance().setFocusedInputEditor(this,FALSE);
	LLLineEditor::onFocusLost();
}

void FSNearbyChatControl::setFocus(BOOL focus)
{
	FSNearbyChat::instance().setFocusedInputEditor(this,focus);
	LLLineEditor::setFocus(focus);
}

void FSNearbyChatControl::autohide()
{
	if(getName()=="default_chat_bar")
	{
		if(gSavedSettings.getBOOL("CloseChatOnReturn"))
		{
			setFocus(FALSE);
		}

		if(gAgentCamera.cameraMouselook() || gSavedSettings.getBOOL("AutohideChatBar"))
		{
			FSNearbyChat::instance().showDefaultChatBar(FALSE);
		}
	}
}

// handle ESC key here
BOOL FSNearbyChatControl::handleKeyHere(KEY key, MASK mask )
{
	BOOL handled = FALSE;
	EChatType type = CHAT_TYPE_NORMAL;

	// autohide the chat bar if escape key was pressed and we're the default chat bar
	if(key==KEY_ESCAPE && mask==MASK_NONE)
	{
		// we let ESC key go through to the rest of the UI code, so don't set handled=TRUE
		autohide();
		gAgent.stopTyping();
	}
	else if( KEY_RETURN == key )
	{
		llinfos << "Handling return key, mask=" << mask << llendl;
		if (mask == MASK_CONTROL)
		{
			// shout
			type = CHAT_TYPE_SHOUT;
			handled = TRUE;
		}
		else if (mask == MASK_SHIFT)
		{
			// whisper
			type = CHAT_TYPE_WHISPER;
			handled = TRUE;
		}
		else if (mask == MASK_ALT)
		{
			// OOC
			type = CHAT_TYPE_OOC;
			handled = TRUE;
		}
		else if (mask == MASK_NONE)
		{
			// say
			type = CHAT_TYPE_NORMAL;
			handled = TRUE;
		}
	}

	if (handled == TRUE)
	{
		// save current line in the history buffer
		LLLineEditor::onCommit();

		// send chat to nearby chat hub
		FSNearbyChat::instance().sendChat(getConvertedText(),type);

		setText(LLStringExplicit(""));
		autohide();
		return TRUE;
	}

	// let the line editor handle everything we don't handle
	return LLLineEditor::handleKeyHere(key,mask);
}

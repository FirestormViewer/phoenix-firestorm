/** 
 * @file lggautocorrectfloater.cpp
 * @brief Auto Correct List floater
 * @copyright Copyright (c) 2011 LordGregGreg Back
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "llviewerprecompiledheaders.h"

#include "lggautocorrectfloater.h"

#include "llagentdata.h"
#include "llcommandhandler.h"
#include "llfloater.h"
#include "lluictrlfactory.h"
#include "llagent.h"
#include "llpanel.h"
#include "llbutton.h"
#include "llcolorswatch.h"
#include "llcombobox.h"
#include "llview.h"
#include "llhttpclient.h"
#include "llbufferstream.h"
#include "llcheckboxctrl.h"
#include "llviewercontrol.h"

#include "llui.h"
#include "llcontrol.h"
#include "llscrollingpanellist.h"
#include "lggautocorrect.h"
#include "llfilepicker.h"
#include "llfile.h"
#include "llsdserialize.h"
#include "llchat.h"
#include "llinventorymodel.h"
#include "llhost.h"
#include "llassetstorage.h"
#include "roles_constants.h"
#include "llviewertexteditor.h"
#include <boost/tokenizer.hpp>

#include <iosfwd>
#include "llfloaterreg.h"
#include "llinspecttoast.h"
#include "llnotificationhandler.h"
#include "llnotificationmanager.h"



//JCInvDropTarget * lggAutoCorrectFloater::mNotecardDropTarget;
//lggAutoCorrectFloater* lggAutoCorrectFloater::sInstance;

//LGGAutoCorrectFloater::~LGGAutoCorrectFloater()
//{
	//delete mNotecardDropTarget;
	//mNotecardDropTarget = NULL;
//}
LGGAutoCorrectFloater::LGGAutoCorrectFloater(const LLSD& key) :
LLFloater(key)
{
}
void LGGAutoCorrectFloater::onClose(bool app_quitting)
{
	destroy(); // Die die die!
}

BOOL LGGAutoCorrectFloater::postBuild(void)
{

	namesList = getChild<LLScrollListCtrl>("lgg_ac_list_name");
	entryList = getChild<LLScrollListCtrl>("lgg_ac_list_entry");

	childSetCommitCallback("lgg_ac_enable",onBoxCommitEnabled,this);

	childSetCommitCallback("lgg_ac_list_enabled",onEntrySettingChange,this);
	childSetCommitCallback("lgg_ac_list_show",onEntrySettingChange,this);
	childSetCommitCallback("lgg_ac_list_style",onEntrySettingChange,this);
	childSetCommitCallback("lgg_ac_priority",onEntrySettingChange,this);
	

	
	updateEnabledStuff();
	updateNamesList();	


	namesList->setCommitOnSelectionChange(TRUE);
	childSetCommitCallback("lgg_ac_list_name", onSelectName, this);
	
	childSetAction("lgg_ac_deletelist",removeList,this);
	childSetAction("lgg_ac_rementry",deleteEntry,this);
	childSetAction("lgg_ac_exportlist",exportList,this);
	childSetAction("lgg_ac_addentry",addEntry,this);
	childSetAction("lgg_ac_loadlist",loadList,this);

	/*LLView *target_view = getChild<LLView>("lgg_ac_notecard_target");
	if(target_view)
	{
		if (mNotecardDropTarget)//shouldn't happen
		{
			delete mNotecardDropTarget;
		}
		mNotecardDropTarget = new JCInvDropTarget("drop target", target_view->getRect(), ResponseItemDrop);//, mAvatarID);
		addChild(mNotecardDropTarget);
	}
	*/
	return true;
}
/*
void LGGAutoCorrectFloater::ResponseItemDrop(LLViewerInventoryItem* item)
{
	if(item)
	{
		if (gAgent.allowOperation(PERM_COPY, item->getPermissions(),GP_OBJECT_MANIPULATE) || gAgent.isGodlike())
		{
			if(!item->getAssetUUID().isNull())
			gAssetStorage->getInvItemAsset(LLHost::invalid,
				gAgent.getID(),
				gAgent.getSessionID(),
				item->getPermissions().getOwner(),
				LLUUID::null,
				item->getUUID(),
				item->getAssetUUID(),
				item->getType(),
				&onNotecardLoadComplete,
				(void*)item,
				TRUE);
			gSavedSettings.setBOOL("FSEnableAutoCorrect",true);

		}
	}
}
void LGGAutoCorrectFloater::onNotecardLoadComplete(LLVFS *vfs,const LLUUID& asset_uuid,LLAssetType::EType type,void* user_data, S32 status, LLExtStat ext_status)
{
	if(status == LL_ERR_NOERR)
	{
		S32 size = vfs->getSize(asset_uuid, type);
		U8* buffer = new U8[size];
		vfs->getData(asset_uuid, type, buffer, 0, size);

		if(type == LLAssetType::AT_NOTECARD)
		{
			LLViewerTextEditor* edit = new LLViewerTextEditor("",LLRect(0,0,0,0),S32_MAX,"");
			if(edit->importBuffer((char*)buffer, (S32)size))
			{
				llinfos << "decode success" << llendl;
				std::string card = "";//edit->getText();
				//edit->die();
				LLSD info;
				std::istringstream ins; // Declare an input string stream.
				ins.str(card);        // Specify string to read.

				LLSDSerialize::fromXML(info,ins);

				LGGAutoCorrect::getInstance()->addCorrectionList(info);
				llinfos << "read success" << llendl;	
				updateEnabledStuff();
			}
			else
			{
				llinfos << "decode error" << llendl;
			}
		}
	}
	else
	{
		llinfos << "read error" << llendl;
	}
}

*/

void LGGAutoCorrectFloater::onSelectName(LLUICtrl* ctrl, void* user_data)
{
	if ( user_data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )user_data;
		if ( self )
			self->updateItemsList();
	}

}
void LGGAutoCorrectFloater::updateItemsList()
{
	entryList->deleteAllItems();
	if((namesList->getAllSelected().size())<=0)
	{

		updateListControlsEnabled(FALSE);
		return;
	}

	updateListControlsEnabled(TRUE);
	std::string listName= namesList->getFirstSelected()->getColumn(0)->getValue().asString();
	
	LLSD listData = LGGAutoCorrect::getInstance()->getAutoCorrectEntries(listName);
	childSetValue("lgg_ac_list_enabled",listData["enabled"].asBoolean());
	childSetValue("lgg_ac_list_style",listData["wordStyle"].asBoolean());
	childSetValue("lgg_ac_list_show",listData["announce"].asBoolean());
	childSetValue("lgg_ac_text_name",listName);
	childSetValue("lgg_ac_text_author",listData["author"]);
	childSetValue("lgg_ac_priority",listData["priority"]);
	static LLCachedControl<S32> countAuto(gSavedSettings, "FSAutoCorrectCount");
	childSetValue("lgg_ac_stats",(S32)countAuto);
	
	LLSD autoCorrects = listData["data"];
	LLSD::map_const_iterator loc_it = autoCorrects.beginMap();
	LLSD::map_const_iterator loc_end = autoCorrects.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const std::string& wrong = (*loc_it).first;
		const std::string& right = (*loc_it).second;

		//std::string lentry(wrong+"=>"+right);

		LLSD element;
		element["id"] = wrong;
		LLSD& s_column = element["columns"][0];
		s_column["column"] = getString("Search");
		s_column["value"] = wrong;
		s_column["font"] = "SANSSERIF";
		LLSD& r_column = element["columns"][1];
		r_column["column"] = getString("Replace");
		r_column["value"] = right;
		r_column["font"] = "SANSSERIF";

		entryList->addElement(element, ADD_BOTTOM);
	}
	
}
void LGGAutoCorrectFloater::updateNamesList()
{
	namesList->deleteAllItems();
	static LLCachedControl<bool> enabledd(gSavedSettings, "FSEnableAutoCorrect");
	if(!(enabledd))
	{
		updateItemsList();
		return;
	}
	static LLCachedControl<S32> countAuto(gSavedSettings, "FSAutoCorrectCount");
	childSetValue("lgg_ac_stats",(S32)countAuto);
	LLSD autoCorrects = LGGAutoCorrect::getInstance()->getAutoCorrects();
	LLSD::map_const_iterator loc_it = autoCorrects.beginMap();
	LLSD::map_const_iterator loc_end = autoCorrects.endMap();
	for ( ; loc_it != loc_end; ++loc_it)
	{
		const std::string& listName = (*loc_it).first;

		LLSD element;
		element["id"] = listName;
		LLSD& friend_column = element["columns"][0];
		friend_column["column"] = getString("Entries");
		friend_column["value"] = listName;
		//friend_column["font"] = "SANSSERIF";
		const LLSD& loc_map = (*loc_it).second;
		if(loc_map["enabled"].asBoolean())
			friend_column["font"] = "SANSSERIF";
			//friend_column["style"] = "BOLD";
		else
			friend_column["font"] = "SANSSERIF_SMALL";
			//friend_column["style"] = "NORMAL";
		if(namesList)
		namesList->addElement(element, ADD_BOTTOM);
	}
	updateItemsList();
}
void LGGAutoCorrectFloater::updateListControlsEnabled(BOOL selected)
{

		childSetEnabled("lgg_ac_text1",selected);
		childSetEnabled("lgg_ac_text2",selected);
		childSetEnabled("lgg_ac_text_name",selected);
		childSetEnabled("lgg_ac_text_author",selected);
		childSetEnabled("lgg_ac_list_enabled",selected);
		childSetEnabled("lgg_ac_list_show",selected);
		childSetEnabled("lgg_ac_list_style",selected);
		childSetEnabled("lgg_ac_deletelist",selected);
		childSetEnabled("lgg_ac_exportlist",selected);
		childSetEnabled("lgg_ac_addentry",selected);
		childSetEnabled("lgg_ac_rementry",selected);
		childSetEnabled("lgg_ac_priority",selected);
	
}
void LGGAutoCorrectFloater::updateEnabledStuff()
{
	static LLCachedControl<bool> enabledd(gSavedSettings, "FSEnableAutoCorrect");
	if(!(enabledd))
	{
		LLCheckBoxCtrl *enBox = getChild<LLCheckBoxCtrl>("lgg_ac_enable");
		enBox->setDisabledColor(LLColor4::red);
		getChild<LLCheckBoxCtrl>("lgg_ac_enable")->setEnabledColor(LLColor4(1.0f,0.0f,0.0f,1.0f));		
	}else
	{
		getChild<LLCheckBoxCtrl>("lgg_ac_enable")->setEnabledColor(
			LLUIColorTable::instance().getColor( "LabelTextColor" ));
	}

	childSetEnabled("lgg_ac_list_name",enabledd);
	childSetEnabled("lgg_ac_list_entry",enabledd);
	updateListControlsEnabled(enabledd);
	updateNamesList();
	LGGAutoCorrect::getInstance()->save();

}
void LGGAutoCorrectFloater::setData(void * data)
{
	//empanel = (LLPanelPhoenix*)data;
}
void LGGAutoCorrectFloater::onBoxCommitEnabled(LLUICtrl* caller, void* user_data)
{
	if ( user_data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )user_data;
		if ( self )
		{
			self->updateEnabledStuff();
		}
	}
}
void LGGAutoCorrectFloater::onEntrySettingChange(LLUICtrl* caller, void* user_data)
{
	if ( user_data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )user_data;
		if ( self )
		{
			std::string listName= self->namesList->getFirstSelected()->getColumn(0)->getValue().asString();
			LGGAutoCorrect::getInstance()->setListEnabled(listName,self->childGetValue("lgg_ac_list_enabled").asBoolean());
			LGGAutoCorrect::getInstance()->setListAnnounceeState(listName,self->childGetValue("lgg_ac_list_show").asBoolean());
			LGGAutoCorrect::getInstance()->setListStyle(listName,self->childGetValue("lgg_ac_list_style").asBoolean());
			LGGAutoCorrect::getInstance()->setListPriority(listName,self->childGetValue("lgg_ac_priority").asInteger());

			//sInstance->updateEnabledStuff();
			self->updateItemsList();
			LGGAutoCorrect::getInstance()->save();
		}
	}
}
void LGGAutoCorrectFloater::deleteEntry(void* data)
{
	if ( data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )data;
		if ( self )
		{

			std::string listName=self->namesList->getFirstSelected()->getColumn(0)->getValue().asString();

			if((self->entryList->getAllSelected().size())>0)
			{	
				std::string wrong= self->entryList->getFirstSelected()->getColumn(0)->getValue().asString();
       				LGGAutoCorrect::getInstance()->removeEntryFromList(wrong,listName);
				self->updateItemsList();
				LGGAutoCorrect::getInstance()->save();
			}
		}
	}
}
void LGGAutoCorrectFloater::loadList(void* data)
{
	LLFilePicker& picker = LLFilePicker::instance();

	if(!picker.getOpenFile( LLFilePicker::FFLOAD_XML) )
	{return;
	}	
	llifstream file;
	file.open(picker.getFirstFile().c_str());
	LLSD blankllsd;
	if (file.is_open())
	{
		LLSDSerialize::fromXMLDocument(blankllsd, file);
	}
	file.close();
	gSavedSettings.setBOOL("FSEnableAutoCorrect",true);
	LGGAutoCorrect::getInstance()->addCorrectionList(blankllsd);
	if ( data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )data;
		if ( self )
			self->updateEnabledStuff();
	}
}
void LGGAutoCorrectFloater::removeList(void* data)
{
	if ( data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )data;
		if ( self )
		{
			std::string listName= self->namesList->getFirstSelected()->getColumn(0)->getValue().asString();
			LGGAutoCorrect::getInstance()->removeCorrectionList(listName);
			self->updateEnabledStuff();
		}

	}
}
void LGGAutoCorrectFloater::exportList(void *data)
{
	if ( data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )data;
		if ( self )
		{
			std::string listName=self->namesList->getFirstSelected()->getColumn(0)->getValue().asString();

			LLFilePicker& picker = LLFilePicker::instance();

			if(!picker.getSaveFile( LLFilePicker::FFSAVE_XML) )
			{return;
			}	
			llofstream file;
			file.open(picker.getFirstFile().c_str());
			LLSDSerialize::toPrettyXML(LGGAutoCorrect::getInstance()->exportList(listName), file);
			file.close();	
		}
	
	}
}
void LGGAutoCorrectFloater::addEntry(void* data)
{
	if ( data )
	{
		LGGAutoCorrectFloater* self = ( LGGAutoCorrectFloater* )data;
		if ( self )
		{
			std::string listName= self->namesList->getFirstSelected()->getColumn(0)->getValue().asString();
			LLChat chat;
			
			LLStringUtil::format_map_t message_args;
			message_args["[COMMAND]"] = gSavedSettings.getString("FSCmdLineAutocorrect");
			message_args["[LISTNAME]"] = listName;
			chat.mText = self->getString("AddNewEntryMessage", message_args);

			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLSD args;
			args["type"] = LLNotificationsUI::NT_NEARBYCHAT;
			LLNotificationsUI::LLNotificationManager::instance().onChat(chat, args);
		}
	}
	
}
LGGAutoCorrectFloater* LGGAutoCorrectFloater::showFloater()
{
	return LLFloaterReg::showTypedInstance<LGGAutoCorrectFloater>("autocorrect");
}

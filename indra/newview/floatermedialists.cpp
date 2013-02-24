/**
 * @file floatermedialists.cpp
 * @brief Floater to edit media white/blacklists - implementation
 *
 * $LicenseInfo:firstyear=2011&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2011, Sione Lomu
 * Copyright (C) 2011, The Phoenix Firestorm Project, Inc.
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
 * $/LicenseInfo$
 */
#include "llviewerprecompiledheaders.h"

#include "floatermedialists.h"
#include "llviewerparcelmedia.h"
#include "lluictrlfactory.h" 
#include "llscrolllistctrl.h"
#include "lllineeditor.h"

bool FloaterMediaLists::sIsWhitelist;

FloaterMediaLists::FloaterMediaLists(const LLSD& key) :
	LLFloater(key)
{
}

FloaterMediaLists::~FloaterMediaLists()
{
}

BOOL FloaterMediaLists::postBuild()
{
	mWhitelistSLC = getChild<LLScrollListCtrl>("whitelist_list");
	mBlacklistSLC = getChild<LLScrollListCtrl>("blacklist_list");

	childSetAction("add_whitelist", onWhitelistAdd,this);
	childSetAction("remove_whitelist", onWhitelistRemove,this);
	childSetAction("add_blacklist", onBlacklistAdd,this);
	childSetAction("remove_blacklist", onBlacklistRemove,this);
	childSetAction("commit_domain", onCommitDomain,this);

	if (!mWhitelistSLC || !mBlacklistSLC)
	{
		return true;
	}
	
	for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
	{
		if (LLViewerParcelMedia::sMediaFilterList[i]["action"].asString() == "allow")
		{	
			LLSD element;
			element["columns"][0]["column"] = "whitelist_col";
			element["columns"][0]["value"] = LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString();
			element["columns"][0]["font"] = "SANSSERIF";
			mWhitelistSLC->addElement(element);
			mWhitelistSLC->sortByColumn("whitelist_col",TRUE);
		}
		else if (LLViewerParcelMedia::sMediaFilterList[i]["action"].asString() == "deny")
		{
			LLSD element;
			element["columns"][0]["column"] = "blacklist_col";
			element["columns"][0]["value"] = LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString();
			element["columns"][0]["font"] = "SANSSERIF";
			mBlacklistSLC->addElement(element);
			mBlacklistSLC->sortByColumn("blacklist_col",TRUE);
		}
	}

	return TRUE;
}

void FloaterMediaLists::draw()
{
	refresh();
	LLFloater::draw();
}

void FloaterMediaLists::refresh()
{
}

//static
void FloaterMediaLists::onWhitelistAdd( void* data )
{
	FloaterMediaLists* self = (FloaterMediaLists*)data;
	self->getChildView("blacklist_list")->setEnabled(false);
	self->getChildView("whitelist_list")->setEnabled(false);
	self->getChildView("remove_whitelist")->setEnabled(false);
	self->getChildView("add_whitelist")->setEnabled(false);
	self->getChildView("remove_blacklist")->setEnabled(false);
	self->getChildView("add_blacklist")->setEnabled(false);
	self->getChildView("input_domain")->setVisible(true);
	self->getChildView("commit_domain")->setVisible(true);
	self->getChild<LLUICtrl>("add_text")->
	setValue(self->getString("EnterUrlAllow"));
	self->getChildView("add_text")->setVisible(true);
	sIsWhitelist = true;
}

void FloaterMediaLists::onWhitelistRemove( void* data )
{
	FloaterMediaLists* self = (FloaterMediaLists*)data;
	LLScrollListItem* selected = self->mWhitelistSLC->getFirstSelected();

	if (selected)
	{
		std::string domain = self->mWhitelistSLC->getSelectedItemLabel();

		for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
		{
			if (LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerParcelMedia::sMediaFilterList.erase(i);
				LLViewerParcelMedia::saveDomainFilterList();
				//HACK: should really see if the URL being deleted
				//  is the same as the saved one
				LLViewerParcelMedia::sMediaLastURL = "";
				LLViewerParcelMedia::sAudioLastURL = "";
				LLViewerParcelMedia::sMediaReFilter = true;
				break;
			}
		}

		self->mWhitelistSLC->deleteSelectedItems();
	}
}

void FloaterMediaLists::onBlacklistAdd( void* data )
{
	FloaterMediaLists* self = (FloaterMediaLists*)data;
	self->getChildView("blacklist_list")->setEnabled(false);
	self->getChildView("whitelist_list")->setEnabled(false);
	self->getChildView("remove_whitelist")->setEnabled(false);
	self->getChildView("add_whitelist")->setEnabled(false);
	self->getChildView("remove_blacklist")->setEnabled(false);
	self->getChildView("add_blacklist")->setEnabled(false);
	self->getChildView("input_domain")->setVisible(true);
	self->getChildView("commit_domain")->setVisible(true);
	self->getChild<LLUICtrl>("add_text")->
	setValue(self->getString("EnterUrlDeny"));
	self->getChildView("add_text")->setVisible(true);
	self->sIsWhitelist = false;
}

void FloaterMediaLists::onBlacklistRemove( void* data )
{	
	FloaterMediaLists* self = (FloaterMediaLists*)data;
	LLScrollListItem* selected = self->mBlacklistSLC->getFirstSelected();

	if (selected)
	{
		std::string domain = self->mBlacklistSLC->getSelectedItemLabel();

		for(S32 i = 0;i<(S32)LLViewerParcelMedia::sMediaFilterList.size();i++)
		{
			if (LLViewerParcelMedia::sMediaFilterList[i]["domain"].asString() == domain)
			{
				LLViewerParcelMedia::sMediaFilterList.erase(i);
				LLViewerParcelMedia::saveDomainFilterList();
				//HACK: should really see if the URL being deleted
				//  is the same as the saved one
				LLViewerParcelMedia::sMediaLastURL = "";
				LLViewerParcelMedia::sAudioLastURL = "";
				LLViewerParcelMedia::sMediaReFilter = true;
				break;
			}
		}

		self->mBlacklistSLC->deleteSelectedItems();
	}
}	

void FloaterMediaLists::onCommitDomain( void* data )
{
	FloaterMediaLists* self = (FloaterMediaLists*)data;
	std::string domain = 
		self->getChild<LLLineEditor>("input_domain")->getText();
	domain = LLViewerParcelMedia::extractDomain(domain);

	if (sIsWhitelist)
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "allow";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		LLSD element;
		element["columns"][0]["column"] = "whitelist_col";
		element["columns"][0]["value"] = domain;
		element["columns"][0]["font"] = "SANSSERIF";
		self->mWhitelistSLC->addElement(element);
		self->mWhitelistSLC->sortByColumn("whitelist_col",TRUE);
	}
	else
	{
		LLSD newmedia;
		newmedia["domain"] = domain;
		newmedia["action"] = "deny";
		LLViewerParcelMedia::sMediaFilterList.append(newmedia);
		LLViewerParcelMedia::saveDomainFilterList();
		LLSD element;
		element["columns"][0]["column"] = "blacklist_col";
		element["columns"][0]["value"] = domain;
		element["columns"][0]["font"] = "SANSSERIF";
		self->mBlacklistSLC->addElement(element);
		self->mBlacklistSLC->sortByColumn("blacklist_col",TRUE);
	}

	self->getChildView("blacklist_list")->setEnabled(true);
	self->getChildView("whitelist_list")->setEnabled(true);
	self->getChildView("remove_whitelist")->setEnabled(true);
	self->getChildView("add_whitelist")->setEnabled(true);
	self->getChildView("remove_blacklist")->setEnabled(true);
	self->getChildView("add_blacklist")->setEnabled(true);
	self->getChildView("input_domain")->setVisible(false);
	self->getChildView("commit_domain")->setVisible(false);
	self->getChildView("add_text")->setVisible(false);
}

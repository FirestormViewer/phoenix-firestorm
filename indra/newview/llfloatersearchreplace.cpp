/** 
 * @file llfloatersearchreplace.cpp
 * @brief LLFloaterSearchReplace class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#include "llfloatersearchreplace.h"

#include "llcheckboxctrl.h"
#include "llmultifloater.h"
#include "lltexteditor.h"

LLFloaterSearchReplace::LLFloaterSearchReplace(const LLSD& sdKey)
	: LLFloater(sdKey), mEditor(NULL)
{
}

LLFloaterSearchReplace::~LLFloaterSearchReplace()
{
}

//static 
void LLFloaterSearchReplace::show(LLTextEditor* pEditor)
{
	LLFloaterSearchReplace* pSelf = LLFloaterReg::getTypedInstance<LLFloaterSearchReplace>("search_replace");
	if (!pSelf)
		return;

	pSelf->mEditor = pEditor;
	if (pEditor)
	{
		LLFloater *pDependeeNew = NULL, *pDependeeOld = pSelf->getDependee();
		LLView* pView = pEditor->getParent();
		while (pView)
		{
			pDependeeNew = dynamic_cast<LLFloater*>(pView);
			if (pDependeeNew)
			{
				if (pDependeeNew != pDependeeOld)
				{
					if (pDependeeOld)
						pDependeeOld->removeDependentFloater(pSelf);

					if (!pDependeeNew->getHost())
						pDependeeNew->addDependentFloater(pSelf);
					else
						pDependeeNew->getHost()->addDependentFloater(pSelf);
				}
				break;
			}
			pView = pView->getParent();
		}

		pSelf->getChildView("replace_text")->setEnabled(!pEditor->getReadOnly());
		pSelf->getChildView("replace_btn")->setEnabled(!pEditor->getReadOnly());
		pSelf->getChildView("replace_all_btn")->setEnabled(!pEditor->getReadOnly());

		pSelf->openFloater();
	}
}

BOOL LLFloaterSearchReplace::postBuild()
{
	childSetAction("search_btn", boost::bind(&LLFloaterSearchReplace::onBtnSearch, this));
	childSetAction("replace_btn", boost::bind(&LLFloaterSearchReplace::onBtnReplace, this));
	childSetAction("replace_all_btn", boost::bind(&LLFloaterSearchReplace::onBtnReplaceAll, this));

	setDefaultBtn("search_btn");

	return TRUE;
}

void LLFloaterSearchReplace::onBtnSearch()
{
	LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	LLCheckBoxCtrl* prevChk = getChild<LLCheckBoxCtrl>("find_previous");
	mEditor->selectNext(getChild<LLUICtrl>("search_text")->getValue().asString(), caseChk->get(), TRUE, prevChk->get());
}

void LLFloaterSearchReplace::onBtnReplace()
{
	LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	LLCheckBoxCtrl* prevChk = getChild<LLCheckBoxCtrl>("find_previous");
	mEditor->replaceText(
		getChild<LLUICtrl>("search_text")->getValue().asString(), getChild<LLUICtrl>("replace_text")->getValue().asString(), caseChk->get(), TRUE, prevChk->get());
}

void LLFloaterSearchReplace::onBtnReplaceAll()
{
	LLCheckBoxCtrl* caseChk = getChild<LLCheckBoxCtrl>("case_text");
	mEditor->replaceTextAll(getChild<LLUICtrl>("search_text")->getValue().asString(), getChild<LLUICtrl>("replace_text")->getValue().asString(), caseChk->get());
}

bool LLFloaterSearchReplace::hasAccelerators() const
{
	// Pass this on to the editor we're operating on (or any view up along its hierarchy 
	// (allows Ctrl-F to work when the floater itself has focus - see changeset 0c8947e5f433)
	const LLView* pView = (LLView*)mEditor;
	while (pView)
	{
		if (pView->hasAccelerators())
			return true;
		pView = pView->getParent();
	}
	return false;
}

BOOL LLFloaterSearchReplace::handleKeyHere(KEY key, MASK mask)
{
      // return mEditorCore->handleKeyHere(key, mask);
      // VWR-23608 Satomi Ahn
	if ( KEY_RETURN == key )
	{
		if (getChild<LLUICtrl>("search_text")->hasFocus()) { onBtnSearch(); return TRUE; }
		else if (getChild<LLUICtrl>("replace_text")->hasFocus()) { onBtnReplace(); return TRUE; }
	}
	// Pass this on to the editor we're operating on (or any view up along its hierarchy 
	// (allows Ctrl-F to work when the floater itself has focus - see changeset 0c8947e5f433)
	LLView* pView = (LLView*)mEditor;
	while (pView)
	{
		if (pView->hasAccelerators())
			return pView->handleKeyHere(key, mask);
		pView = pView->getParent();
	}
	return FALSE;
}

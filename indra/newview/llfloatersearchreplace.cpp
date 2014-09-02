/** 
 *
 * Copyright (c) 2010-2013, Kitty Barnett
 * 
 * The source code in this file is provided to you under the terms of the 
 * GNU Lesser General Public License, version 2.1, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A 
 * PARTICULAR PURPOSE. Terms of the LGPL can be found in doc/LGPL-licence.txt 
 * in this distribution, or online at http://www.gnu.org/licenses/lgpl-2.1.txt
 * 
 * By copying, modifying or distributing this software, you acknowledge that
 * you have read and understood your obligations described above, and agree to 
 * abide by those obligations.
 * 
 */

#include "llviewerprecompiledheaders.h"

#include "llcheckboxctrl.h"
#include "llfloaterreg.h"
#include "llfloatersearchreplace.h"
#include "lllineeditor.h"
#include "llmultifloater.h"
#include "lltexteditor.h"
#include "llviewermenu.h"

// ============================================================================
// LLFloaterSearchReplace class
//

LLFloaterSearchReplace::LLFloaterSearchReplace(const LLSD& sdKey)
	: LLFloater(sdKey)
	, m_pSearchEditor(NULL)
	, m_pReplaceEditor(NULL)
	, m_pCaseInsensitiveCheck(NULL)
	, m_pSearchUpCheck(NULL)
{
}

LLFloaterSearchReplace::~LLFloaterSearchReplace()
{
}

BOOL LLFloaterSearchReplace::postBuild()
{
	m_pSearchEditor = getChild<LLLineEditor>("search_text");
	m_pSearchEditor->setCommitCallback(boost::bind(&LLFloaterSearchReplace::onSearchClick, this));
	m_pSearchEditor->setCommitOnFocusLost(false);
	m_pSearchEditor->setKeystrokeCallback(boost::bind(&LLFloaterSearchReplace::refreshHighlight, this), NULL);
	m_pReplaceEditor = getChild<LLLineEditor>("replace_text");

	m_pCaseInsensitiveCheck = getChild<LLCheckBoxCtrl>("case_text");
	m_pCaseInsensitiveCheck->setCommitCallback(boost::bind(&LLFloaterSearchReplace::refreshHighlight, this));
	m_pSearchUpCheck = getChild<LLCheckBoxCtrl>("find_previous");

	LLButton* pSearchBtn = getChild<LLButton>("search_btn");
	pSearchBtn->setCommitCallback(boost::bind(&LLFloaterSearchReplace::onSearchClick, this));
	setDefaultBtn(pSearchBtn);

	LLButton* pReplaceBtn = getChild<LLButton>("replace_btn");
	pReplaceBtn->setCommitCallback(boost::bind(&LLFloaterSearchReplace::onReplaceClick, this));

	LLButton* pReplaceAllBtn = getChild<LLButton>("replace_all_btn");
	pReplaceAllBtn->setCommitCallback(boost::bind(&LLFloaterSearchReplace::onReplaceAllClick, this));

	return TRUE;
}

void LLFloaterSearchReplace::onOpen(const LLSD& sdKey)
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		// HACK-Catznip: hasSelection() is inaccessible but canCopy() is (currently) a synonym *sighs*
		if (pEditor->canCopy())
		{
			m_pSearchEditor->setText(pEditor->getSelectionString());
			m_pSearchEditor->setCursorToEnd();
		}
		pEditor->setHighlightWord(m_pSearchEditor->getText(), m_pCaseInsensitiveCheck->get());

		m_pReplaceEditor->setEnabled( (pEditor) && (!pEditor->getReadOnly()) );
		getChild<LLButton>("replace_btn")->setEnabled( (pEditor) && (!pEditor->getReadOnly()) );
		getChild<LLButton>("replace_all_btn")->setEnabled( (pEditor) && (!pEditor->getReadOnly()) );
	}
	m_pSearchEditor->setFocus(TRUE);
}

void LLFloaterSearchReplace::onClose(bool fQuiting)
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		pEditor->clearHighlights();
	}
}

bool LLFloaterSearchReplace::hasAccelerators() const
{
	const LLView* pView = dynamic_cast<LLTextEditor*>(m_EditorHandle.get());
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
	// Pass this on to the editor we're operating on (or any view up along its hierarchy) if we don't handle the key ourselves 
	// (allows Ctrl-F to work when the floater itself has focus - see changeset 0c8947e5f433)
	BOOL handled = LLFloater::handleKeyHere(key, mask);
	if (!handled)
	{
		// Check if one of our children currently has keyboard focus and if so route edit accellerators to it
		if (gFocusMgr.childHasKeyboardFocus(this))
		{
			LLView* pEditView = dynamic_cast<LLView*>(LLEditMenuHandler::gEditMenuHandler);
			if ( (pEditView) && (pEditView->hasAncestor(this)) && (gEditMenu) && (gEditMenu->handleAcceleratorKey(key, mask)) )
			{
				return TRUE;
			}
		}

		LLView* pView = m_EditorHandle.get();
		while (pView)
		{
			if ( (pView->hasAccelerators()) && (pView->handleKeyHere(key, mask)) )
				return TRUE;
			pView = pView->getParent();
		}
	}
	return handled;
}

//static 
void LLFloaterSearchReplace::show(LLTextEditor* pEditor)
{
	LLFloaterSearchReplace* pSelf = LLFloaterReg::getTypedInstance<LLFloaterSearchReplace>("search_replace");
	if ( (!pSelf) || (!pEditor) )
		return;

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
				{
					if (pSelf->getEditor())
						pSelf->getEditor()->clearHighlights();
					pDependeeOld->removeDependentFloater(pSelf);
				}

				if (!pDependeeNew->getHost())
					pDependeeNew->addDependentFloater(pSelf);
				else
					pDependeeNew->getHost()->addDependentFloater(pSelf);
			}
			break;
		}
		pView = pView->getParent();
	}

	pSelf->m_EditorHandle = pEditor->getHandle();
	pSelf->openFloater();
}

LLTextEditor* LLFloaterSearchReplace::getEditor() const
{
	return dynamic_cast<LLTextEditor*>(m_EditorHandle.get());
}

void LLFloaterSearchReplace::refreshHighlight()
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		pEditor->setHighlightWord(m_pSearchEditor->getText(), m_pCaseInsensitiveCheck->get());
	}
}

void LLFloaterSearchReplace::onSearchClick()
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		pEditor->selectNext(m_pSearchEditor->getText(), m_pCaseInsensitiveCheck->get(), TRUE, m_pSearchUpCheck->get());
	}
}

void LLFloaterSearchReplace::onReplaceClick()
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		pEditor->replaceText(m_pSearchEditor->getText(), m_pReplaceEditor->getText(), m_pCaseInsensitiveCheck->get(), TRUE, m_pSearchUpCheck->get());
	}
}

void LLFloaterSearchReplace::onReplaceAllClick()
{
	LLTextEditor* pEditor = getEditor();
	if (pEditor)
	{
		pEditor->replaceTextAll(m_pSearchEditor->getText(), m_pReplaceEditor->getText(), m_pCaseInsensitiveCheck->get());
	}
}

// ============================================================================

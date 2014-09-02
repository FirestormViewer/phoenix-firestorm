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

#ifndef LL_FLOATERSEARCHREPLACE_H
#define LL_FLOATERSEARCHREPLACE_H

#include "llfloater.h"

// ============================================================================
// Forward declarations
//

class LLCheckBoxCtrl;
class LLLineEditor;
class LLTextEditor;

// ============================================================================
// LLFloaterSearchReplace class
//

class LLFloaterSearchReplace : public LLFloater
{
public:
	LLFloaterSearchReplace(const LLSD& sdKey);
	~LLFloaterSearchReplace();

	/*
	 * LLView overrides
	 */
public:
	/*virtual*/ BOOL handleKeyHere(KEY key, MASK mask);
	/*virtual*/ bool hasAccelerators() const;
	/*virtual*/ BOOL postBuild();
	/*virtual*/ void onOpen(const LLSD& sdKey);
	/*virtual*/ void onClose(bool fQuiting);

	/*
	 * Member functions
	 */
public:
	static void   show(LLTextEditor* pEditor);
protected:
	LLTextEditor* getEditor() const;
	void          refreshHighlight();
	void          onSearchClick();
	void          onSearchKeystroke();
	void          onReplaceClick();
	void          onReplaceAllClick();

	/*
	 * Member variables
	 */
private:
	LLLineEditor*      m_pSearchEditor;
	LLLineEditor*      m_pReplaceEditor;
	LLCheckBoxCtrl*    m_pCaseInsensitiveCheck;
	LLCheckBoxCtrl*    m_pSearchUpCheck;
	LLHandle<LLUICtrl> m_EditorHandle;
};

// ============================================================================

#endif // LL_FLOATERSEARCHREPLACE_H

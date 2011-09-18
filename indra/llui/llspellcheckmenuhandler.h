/** 
 *
 * Copyright (c) 2010, Kitty Barnett
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

#ifndef LLSPELLCHECKMENUHANDLER_H
#define LLSPELLCHECKMENUHANDLER_H

// ============================================================================

class LLSpellCheckMenuHandler
{
public:
	virtual bool		useSpellCheck() const		{ return false; }

	virtual std::string	getSuggestion(U32 idxSuggestion) const { return ""; }
	virtual U32			getSuggestionCount() const	{ return 0; }
	virtual void		replaceWithSuggestion(U32 idxSuggestion) {}

	virtual void		addToDictionary()			{}
	virtual bool		canAddToDictionary() const	{ return false; }

	virtual void		addToIgnore()				{}
	virtual bool		canAddToIgnore() const		{ return false; }
};

// ============================================================================

#endif

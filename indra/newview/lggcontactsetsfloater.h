/* @file lggcontactsetsfloater.h
   Copyright (C) 2011 LordGregGreg Back (Greg Hendrickson)

   This is free software; you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as
   published by the Free Software Foundation; version 2.1 of
   the License.
 
   This is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.
 
   You should have received a copy of the GNU Lesser General Public
   License along with the viewer; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

#ifndef LGG_CONTACTSETSFLOATER_H
#define LGG_CONTACTSETSFLOATER_H

#include "llview.h"
#include "llviewerinventory.h"
#include "llfloater.h"
#include "llcallingcard.h"
#include "llcombobox.h"
#include "llcolorswatch.h"
#include "llcheckboxctrl.h"
#include "llavatarname.h"
#include "llavatarpropertiesprocessor.h"


class lggContactSetsFloater : public LLFloater, public LLFriendObserver, public LLAvatarPropertiesObserver
{
public:
	lggContactSetsFloater(const LLSD& seed);
	virtual ~lggContactSetsFloater();
	// LLAvatarPropertiesProcessor observer trigger
	virtual void processProperties(void* data, EAvatarProcessorType type);

	virtual void changed(U32 mask);
	BOOL postBuild();
	BOOL handleMouseDown(S32 x, S32 y, MASK mask);
	void update();
	BOOL handleRightMouseDown(S32 x, S32 y, MASK mask);
	BOOL handleKeyHere(KEY key, MASK mask);
	BOOL handleDoubleClick(S32 x, S32 y, MASK mask);
	BOOL handleHover(S32 x, S32 y, MASK mask);
	BOOL handleUnicodeCharHere(llwchar uni_char);
	BOOL handleScrollWheel(S32 x, S32 y, S32 clicks);
	void onMouseLeave(S32 x, S32 y, MASK mask);
	void onMouseEnter(S32 x, S32 y, MASK mask);

	BOOL generateCurrentList();
	void draw();
	void drawScrollBars();
	void drawRightClick();
	void drawFilter();
	BOOL toggleSelect(LLUUID whoToToggle);
	static BOOL compareAv(LLUUID av1, LLUUID av2);

	void onClickSettings();
	void onClickNew();
	void onClickDelete();
	void onPickAvatar(const std::vector<LLUUID>& ids, const std::vector<LLAvatarName> names);
	void onBackgroundChange();
	void onNoticesChange();
	void onCheckBoxChange();

	void hitSpaceBar();

	void updateGroupsList();
	void updateGroupGUIs();

	void onSelectGroup();

	LLComboBox* groupsList;
	LLColorSwatchCtrl * groupColorBox;
	LLCheckBoxCtrl* noticeBox;
	LLRect contextRect;

private:
	S32 mouse_x;
	S32 mouse_y;
	F32 hovered;
	F32 scrollStarted;
	S32 maxSize;
	std::string currentFilter;
	std::string currentRightClickText;
	std::vector<LLUUID> selected;
	std::vector<LLUUID> currentList;
	std::vector<std::string> allFolders;
	std::vector<std::string> openedFolders;
	std::set<LLUUID> profileImagePending;
	S32 scrollLoc;
	BOOL showRightClick;
	BOOL justClicked;
	BOOL mouseInWindow;
};

class lggContactSetsFloaterSettings : public LLFloater
{
public:
	lggContactSetsFloaterSettings(const LLSD& seed);
	BOOL postBuild(void);
	
private:
	virtual ~lggContactSetsFloaterSettings() {}
	void onDefaultBackgroundChange();
};

#endif // LGG_CONTACTSETSFLOATER_H

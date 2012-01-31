/** 
 * @file llsidetray.cpp
 * @brief SideBartray definition, broken out from llsidebar.cpp - Arrehn
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
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

#ifndef LLSIDETRAYTAB
#define LLSIDETRAYTAB

#include "llpanel.h"
#include "llbadge.h"

#include "llfloater.h"


class LLSideTray;
class LLSideTrayButton;

//////////////////////////////////////////////////////////////////////////////
// LLSideTrayTab
// Represents a single tab in the side tray, only used by LLSideTray
//////////////////////////////////////////////////////////////////////////////

class LLSideTrayTab: public LLPanel
{
	LOG_CLASS(LLSideTrayTab);
	friend class LLUICtrlFactory;
	friend class LLSideTray;
public:
	
	struct Params 
	:	public LLInitParam::Block<Params, LLPanel::Params>
	{
		// image name
		Optional<std::string>		image;
		Optional<std::string>		image_selected;
		Optional<std::string>		tab_title;
		Optional<std::string>		description;
		Optional<LLBadge::Params>	badge;
		Params()
		:	image("image"),
			image_selected("image_selected"),
			tab_title("tab_title","no title"),
			description("description","no description"),
			badge("badge")
		{};
	};
protected:
	LLSideTrayTab(const Params& params);
	
	void			dock(bool app_quitting, LLFloater* floater_tab); // LO: Fix for FIRE-2613: sidebar tabs and floaters not remembering being open/torn off
	void			undock(LLFloater* floater_tab);

	LLSideTray*		getSideTray();
	
public:
	virtual ~LLSideTrayTab();
	
    /*virtual*/ BOOL	postBuild	();
	/*virtual*/ bool	addChild	(LLView* view, S32 tab_group);
	
//-TT Toggle sidebar panels with buttons
	static void		toggleSidebarTabInstance(std::string sdname);
//-TT
	void			reshape		(S32 width, S32 height, BOOL called_from_parent = TRUE);
	static LLSideTrayTab*  createInstance	();
	const std::string& getDescription () const { return mDescription;}

	const std::string& getTabTitle() const { return mTabTitle;}

	void			onOpen		(const LLSD& key);
	void			toggleTabDocked(bool app_quitting = false, bool toggle_floater = true); // LO: Fix for FIRE-2613: sidebar tabs and floaters not remembering being open/torn off
    void			setDocked(bool dock);
    bool			isDocked() const;
	void			minimizeTab();

	BOOL            handleScrollWheel(S32 x, S32 y, S32 clicks);

	LLPanel*	getPanel();

	LLButton*	createButton(bool allowTearOff, LLUICtrl::commit_callback_t callback);

private:
	std::string mTabTitle;
	std::string mImage;
	std::string mImageSelected;
	std::string	mDescription;
	
	LLView*	mMainPanel;

	bool			mHasBadge;
	LLBadge::Params	mBadgeParams;
	LLSideTrayButton*	mSideTrayButton;
};
#endif

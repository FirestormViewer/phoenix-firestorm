/**
 * @file piemenu.cpp
 * @brief Pie menu class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2011, Zi Ree @ Second Life
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
#include "linden_common.h"

#include "piemenu.h"
#include "pieslice.h"
#include "pieseparator.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "v2math.h"

// copied from LLMenuGL - Remove these lines over there when finished
const S32 PIE_INNER_SIZE=20;		// radius of the inner pie circle
const F32 PIE_POPUP_FACTOR=(F32)1.7;		// pie menu size factor on popup
const F32 PIE_POPUP_TIME=(F32)0.25;		// time to shrink from popup size to regular size
const S32 PIE_OUTER_SIZE=96;		// radius of the outer pie circle

// register the pie menu globally as child widget
static LLDefaultChildRegistry::Register<PieMenu> r1("pie_menu");

// register all possible child widgets a pie menu can have
static PieChildRegistry::Register<PieMenu> pie_r1("pie_menu");
static PieChildRegistry::Register<PieSlice> pie_r2("pie_slice");
static PieChildRegistry::Register<PieSeparator> pie_r3("pie_separator");

#define PIE_POPUP_EFFECT 1			// debug
#define PIE_DRAW_BOUNDING_BOX 0		// debug

// pie slice label text positioning
S32 PIE_X[]={64,45, 0,-45,-63,-45,  0, 45};
S32 PIE_Y[]={ 0,44,73, 44,  0,-44,-73,-44};

PieMenu::PieMenu(const LLContextMenu::Params& p) :
	LLContextMenu(p)
{
	lldebugs << "PieMenu::PieMenu()" << llendl;
	// radius, so we need this *2
	reshape(PIE_OUTER_SIZE*2,PIE_OUTER_SIZE*2,FALSE);
	// set up the font for the menu
	mFont=LLFontGL::getFont(LLFontDescriptor("SansSerif","Pie",LLFontGL::NORMAL));
	if(!mFont)
	{
		llwarns << "Could not find font size for Pie menu, falling back to Small font." << llendl;
		mFont=LLFontGL::getFont(LLFontDescriptor("SansSerif","Small",LLFontGL::NORMAL));
	}
	// set slices pointer to our own slices
	mSlices=&mMySlices;
	// this will be the first click on the menu
	mFirstClick=TRUE;

	// clean initialisation
	mSlice=0;
}

bool PieMenu::addChild(LLView* child,S32 tab_group)
{
	// don't add invalid slices
	if(!child)
		return FALSE;

	// add a new slice to the menu
	mSlices->push_back(child);

	// tell the view that our menu has changed and reshape it back to correct size
	LLUICtrl::addChild(child);
	reshape(PIE_OUTER_SIZE*2,PIE_OUTER_SIZE*2,FALSE);
	return TRUE;
}

void PieMenu::removeChild(LLView* child)
{
	// remove a slice from the menu
	slice_list_t::iterator found_it=std::find(mSlices->begin(),mSlices->end(),child);
	if(found_it!=mSlices->end())
	{
		mSlices->erase(found_it);
	}

	// tell the view that our menu has changed and reshape it back to correct size
	LLUICtrl::removeChild(child);
	reshape(PIE_OUTER_SIZE*2,PIE_OUTER_SIZE*2,FALSE);
}

BOOL PieMenu::handleHover(S32 x,S32 y,MASK mask)
{
	// do nothing
	return TRUE;
}

void PieMenu::show(S32 x, S32 y, LLView* spawning_view)
{
	// if the menu is already there, do nothing
	if(getVisible())
		return;

	// play a sound
	make_ui_sound("UISndPieMenuAppear");

	lldebugs << "PieMenu::show(): " << x << " " << y << llendl;

	// make sure the menu is always the correct size
	reshape(PIE_OUTER_SIZE*2,PIE_OUTER_SIZE*2,FALSE);

	// remember our center point
	mCenterX=x;
	mCenterY=y;

	// get the 3D view rectangle
	LLRect screen=LLMenuGL::sMenuContainer->getMenuRect();

	// check if the pie menu is out of bounds and move it accordingly
	if(x-PIE_OUTER_SIZE<0)
		x=PIE_OUTER_SIZE;
	else if(x+PIE_OUTER_SIZE>screen.getWidth())
		x=screen.getWidth()-PIE_OUTER_SIZE;

	if(y-PIE_OUTER_SIZE<screen.mBottom)
		y=PIE_OUTER_SIZE+screen.mBottom;
	else if(y+PIE_OUTER_SIZE-screen.mBottom>screen.getHeight())
		y=screen.getHeight()-PIE_OUTER_SIZE+screen.mBottom;

	// move the mouse pointer into the center of the menu
	LLUI::setMousePositionLocal(getParent(),x,y);
	// set our drawing origin to the center of the menu, taking UI scale into account
	LLVector2 scale=gViewerWindow->getDisplayScale();
	setOrigin(x-PIE_OUTER_SIZE/scale.mV[VX],y-PIE_OUTER_SIZE/scale.mV[VY]);
	// grab mouse control
	gFocusMgr.setMouseCapture(this);

	// this was the first click for the menu
	mFirstClick=TRUE;
	// set up the slices pointer to the menu's own slices
	mSlices=&mMySlices;

	// reset enable update checks for slices
	for (slice_list_t::iterator it = mSlices->begin(); it != mSlices->end(); it++)
	{
		PieSlice* resetSlice = dynamic_cast<PieSlice*>(*it);
		if (resetSlice)
		{
			resetSlice->resetUpdateEnabledCheck();
		}
	}

	// cleanup
	mSlice=0;
	mOldSlice=0;

	// draw the menu on screen
	setVisible(TRUE);
	LLView::setVisible(TRUE);
}

void PieMenu::hide()
{
	// if the menu is already hidden, do nothing
	if(!getVisible())
		return;

	// make a sound when hiding
	make_ui_sound("UISndPieMenuHide");

	lldebugs << "Clearing selections" << llendl;

	mSlices=&mMySlices;
#if PIE_POPUP_EFFECT
	// safety in case the timer was still running
	mPopupTimer.stop();
#endif
	LLView::setVisible(FALSE);
}

void PieMenu::setVisible(BOOL visible)
{
	// hide the menu if needed
	if(!visible)
	{
		hide();
		// clear all menus and temporary selections
		sMenuContainer->hideMenus();
	}
}

void PieMenu::draw( void )
{
	// save the current OpenGL drawing matrix so we can freely modify it
	gGL.pushMatrix();

	// save the widget's rectangle for later use
	LLRect r=getRect();

	// initialize pie scale factor for popup effect
	F32 factor=1.0;

#if PIE_POPUP_EFFECT
	// set the popup size if this was the first click on the menu
	if(mFirstClick)
	{
		factor=PIE_POPUP_FACTOR;
	}
	// otherwise check if the popup timer is still running
	else if(mPopupTimer.getStarted())
	{
		// if the timer ran past the popup time, stop the timer and set the size to 1.0
		F32 elapsedTime=mPopupTimer.getElapsedTimeF32();
		if(elapsedTime>PIE_POPUP_TIME)
		{
			factor=1.0;
			mPopupTimer.stop();
		}
		// otherwise calculate the size factor to make the menu shrink over time
		else
		{
			factor=PIE_POPUP_FACTOR-(PIE_POPUP_FACTOR-1.0)*elapsedTime/PIE_POPUP_TIME;
		}
//		setRect(r);  // obsolete?
	}
#endif

#if PIE_DRAW_BOUNDING_BOX
	// draw a bounding box around the menu for debugging purposes
	gl_rect_2d(0,r.getHeight(),r.getWidth(),0,LLColor4(1,1,1,1),FALSE);
#endif

	// set up pie menu colors
	LLColor4 lineColor=LLUIColorTable::instance().getColor("PieMenuLineColor");
	LLColor4 selectedColor=LLUIColorTable::instance().getColor("PieMenuSelectedColor");
	LLColor4 textColor=LLUIColorTable::instance().getColor("PieMenuTextColor");
	LLColor4 bgColor=LLUIColorTable::instance().getColor("PieMenuBgColor");
	LLColor4 borderColor=bgColor % (F32)0.3;

	// if the user wants their own colors, apply them here
	if(gSavedSettings.getBOOL("OverridePieColors"))
	{
		bgColor=LLUIColorTable::instance().getColor("PieMenuBgColorOverride") % gSavedSettings.getF32("PieMenuOpacity");
		borderColor=bgColor % (1.f-gSavedSettings.getF32("PieMenuFade"));
		selectedColor=LLUIColorTable::instance().getColor("PieMenuSelectedColorOverride");
	}

	// on first click, make the menu fade out to indicate "borderless" operation
	if(mFirstClick)
		borderColor%=0.0;

	// initially, the current segment is marked as invalid
	S32 currentSegment=-1;

	// get the current mouse pointer position local to the pie
	S32 x,y;
	LLUI::getMousePositionLocal(this,&x,&y);
	// remember to take the UI scaling into account
	LLVector2 scale=gViewerWindow->getDisplayScale();
	// move mouse coordinates to be relative to the pie center
	LLVector2 mouseVector(x-PIE_OUTER_SIZE/scale.mV[VX],y-PIE_OUTER_SIZE/scale.mV[VY]);

	// get the distance from the center point
	F32 distance=mouseVector.length();
	// check if our mouse pointer is within the pie slice area
	if(distance>PIE_INNER_SIZE && (distance<(PIE_OUTER_SIZE*factor) || mFirstClick))
	{
		// get the angle of the mouse pointer from the center in radians
		F32 angle=acos(mouseVector.mV[VX]/distance);
		// if the mouse is below the middle of the pie, reverse the angle
		if(mouseVector.mV[VY]<0)
			angle=F_PI*2-angle;
		// rotate the angle slightly so the slices' centers are aligned correctly
		angle+=F_PI/8;

		// calculate slice number from the angle
		currentSegment=(S32) (8.0*angle/(F_PI*2.0)) % 8;
	}

	// move origin point to the center of our rectangle
	gGL.translatef(r.getWidth()/2,r.getHeight()/2,0.0);

	// draw the general pie background
	gl_washer_2d(PIE_OUTER_SIZE*factor,PIE_INNER_SIZE,32,bgColor,borderColor);

	// set up an item list iterator to point at the beginning of the item list
	slice_list_t::iterator cur_item_iter;
	cur_item_iter=mSlices->begin();

	// clear current slice pointer
	mSlice=0;
	// current slice number is 0
	S32 num=0;
	BOOL wasAutohide=FALSE;
	do
	{
		// standard item text color
		LLColor4 itemColor=textColor;

		// clear the label and set up the starting angle to draw in
		std::string label("");
		F32 segmentStart=F_PI/4.0*(F32) num-F_PI/8.0;

		// iterate through the list of slices
		if(cur_item_iter!=mSlices->end())
		{
			// get current slice item
			LLView* item=(*cur_item_iter);

			// check if this is a submenu or a normal click slice
			PieSlice* currentSlice=dynamic_cast<PieSlice*>(item);
			PieMenu* currentSubmenu=dynamic_cast<PieMenu*>(item);
			// advance internally to the next slice item
			cur_item_iter++;

			// in case it is regular click slice
			if(currentSlice)
			{
				// get the slice label and tell the slice to check if it's supposed to be visible
				label=currentSlice->getLabel();
				currentSlice->updateVisible();
				// disable it if it's not visible, pie slices never really disappear
				if(!currentSlice->getVisible())
				{
					lldebugs << label << " is not visible" << llendl;
					currentSlice->setEnabled(FALSE);
				}

				// if the current slice is the start of an autohide chain, clear out previous chains
				if(currentSlice->getStartAutohide())
					wasAutohide=FALSE;

				// check if the current slice is part of an autohide chain
				if(currentSlice->getAutohide())
				{
					// if the previous item already won the autohide, skip this item
					if(wasAutohide)
						continue;
					// look at the next item in the pie
					LLView* lookAhead=(*cur_item_iter);
					// check if this is a normal click slice
					PieSlice* lookSlice=dynamic_cast<PieSlice*>(lookAhead);
					if(lookSlice)
					{
						// if the next item is part of the current autohide chain as well ...
						if(lookSlice->getAutohide() && !lookSlice->getStartAutohide())
						{
							// ... it's visible and it's enabled, skip the current one.
							// the first visible and enabled item in autohide chains wins
							// this is useful for Sit/Stand toggles
							lookSlice->updateEnabled();
							lookSlice->updateVisible();
							if(lookSlice->getVisible() && lookSlice->getEnabled())
								continue;
							// this item won the autohide contest
							wasAutohide=TRUE;
						}
					}
				}
				else
					// reset autohide chain
					wasAutohide=FALSE;
				// check if the slice is currently enabled
				currentSlice->updateEnabled();
				if(!currentSlice->getEnabled())
				{
					lldebugs << label << " is disabled" << llendl;
					// fade the item color alpha to mark the item as disabled
					itemColor%=(F32)0.3;
				}
			}
			// if it's a submenu just get the label
			else if(currentSubmenu)
				label=currentSubmenu->getLabel();

			// if it's a slice or submenu, the mouse pointer is over the same segment as our counter and the item is enabled
			if((currentSlice || currentSubmenu) && (currentSegment==num) && item->getEnabled())
			{
				// memorize the currently highlighted slice for later
				mSlice=item;
				// if we highlighted a different slice than before, we must play a sound
				if(mOldSlice!=mSlice)
				{
					// get the appropriate UI sound and play it
					char soundNum='0'+num;
					std::string soundName="UISndPieMenuSliceHighlight";
					soundName+=soundNum;

					make_ui_sound(soundName.c_str());
					// remember which slice we highlighted last, so we only play the sound once
					mOldSlice=mSlice;
				}

				// draw the currently highlighted pie slice
				gl_washer_segment_2d(PIE_OUTER_SIZE*factor,PIE_INNER_SIZE,segmentStart+0.02,segmentStart+F_PI/4.0-0.02,4,selectedColor,borderColor);
			}
		}
		// draw the divider line for this slice
		gl_washer_segment_2d(PIE_OUTER_SIZE*factor,PIE_INNER_SIZE,segmentStart-0.02,segmentStart+0.02,4,lineColor,borderColor);

		// draw the slice labels around the center
		mFont->renderUTF8(label,
							0,
							PIE_X[num],
							PIE_Y[num],
							itemColor,
							LLFontGL::HCENTER,
							LLFontGL::VCENTER,
							LLFontGL::NORMAL,
							LLFontGL::DROP_SHADOW_SOFT);
		// next slice
		num++;
	}
	while(num<8);	// do this until the menu is full

	// draw inner and outer circle, outer only if it was not the first click
	if(!mFirstClick)
		gl_washer_2d(PIE_OUTER_SIZE*factor,PIE_OUTER_SIZE*factor-2,32,lineColor,borderColor);
	gl_washer_2d(PIE_INNER_SIZE+1,PIE_INNER_SIZE-1,16,borderColor,lineColor);

	// restore OpenGL drawing matrix
	gGL.popMatrix();

	// give control back to the UI library
	LLView::draw();
}

BOOL PieMenu::appendContextSubMenu(PieMenu* menu)
{
	lldebugs << "PieMenu::appendContextSubMenu()" << llendl;
	if(!menu)
		return FALSE;

	lldebugs << "PieMenu::appendContextSubMenu() appending " << menu->getLabel() << " to " << getLabel() << llendl;

	// add the submenu to the list of items
	mSlices->push_back(menu);
	// tell the view that our menu has changed
	LLUICtrl::addChild(menu);
	return TRUE;
}

BOOL PieMenu::handleMouseUp(S32 x,S32 y,MASK mask)
{
	// left and right mouse buttons both do the same thing currently
	return handleMouseButtonUp(x,y,mask);
}

BOOL PieMenu::handleRightMouseUp(S32 x,S32 y,MASK mask)
{
	// left and right mouse buttons both do the same thing currently
	return handleMouseButtonUp(x,y,mask);
}

// left and right mouse buttons both do the same thing currently
BOOL PieMenu::handleMouseButtonUp(S32 x,S32 y,MASK mask)
{
	// if this was the first click and no slice is highlighted (no borderless click), start the popup timer
	if(mFirstClick && !mSlice)
	{
		mFirstClick=FALSE;
#if PIE_POPUP_EFFECT
		mPopupTimer.start();
#endif
	}
	else
	{
		// default to invisible
		BOOL visible=FALSE;

		// get the current selected slice and check if this is a regular click slice
		PieSlice* currentSlice=dynamic_cast<PieSlice*>(mSlice);
		if(currentSlice)
		{
			// if so, click it and make a sound
			make_ui_sound("UISndClickRelease");
			currentSlice->onCommit();
		}
		else
		{
			// check if this was a submenu
			PieMenu* currentSubmenu=dynamic_cast<PieMenu*>(mSlice);
			if(currentSubmenu)
			{
				// if so, remember we clicked the menu already at least once
				mFirstClick=FALSE;
				// swap out the list of items for the ones in the submenu
				mSlices=&currentSubmenu->mMySlices;
				// reset enable update checks for slices
				for (slice_list_t::iterator it = mSlices->begin(); it != mSlices->end(); it++)
				{
					PieSlice* resetSlice = dynamic_cast<PieSlice*>(*it);
					if (resetSlice)
					{
						resetSlice->resetUpdateEnabledCheck();
					}
				}
				// the menu stays visible
				visible=TRUE;
#if PIE_POPUP_EFFECT
				// restart the popup timer
				mPopupTimer.reset();
				mPopupTimer.start();
#endif
				// make a sound
				make_ui_sound("UISndPieMenuAppear");
			}
		}
		// show or hide the menu, as needed
		setVisible(visible);
	}
	// release mouse capture after the first click if we still have it grabbed
	if(hasMouseCapture())
		gFocusMgr.setMouseCapture(NULL);

	// give control back to the system
	return LLView::handleMouseUp(x,y,mask);
}

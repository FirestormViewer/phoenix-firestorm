/**
 * @file pieslice.cpp
 * @brief Pie menu slice class
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
#include "pieslice.h"


// create a new slice and memorize the XUI parameters
PieSlice::PieSlice(const PieSlice::Params& p) :
	LLUICtrl(p),
	mLabel(p.label),
	mStartAutohide(p.start_autohide),
	mAutohide(p.autohide),
	mCheckEnableOnce(p.check_enable_once),
	mDoUpdateEnabled(TRUE)
{

	lldebugs << "PieSlice::PieSlice(): " << mLabel << " " << mAutohide << " " << mCheckEnableOnce << llendl;
}

// pick up parameters from the XUI definition
PieSlice::Params::Params() :
	on_click("on_click"),
	on_visible("on_visible"),
	on_enable("on_enable"),
	start_autohide("start_autohide",FALSE),
	autohide("autohide",FALSE),
	check_enable_once("check_enable_once",FALSE)
{
}

// initialize parameters
void PieSlice::initFromParams(const Params& p)
{
	// add a callback if on_click is provided
	if(p.on_click.isProvided())
	{
		setCommitCallback(initCommitCallback(p.on_click));
	}
	// add a callback if on_visible is provided
	if (p.on_visible.isProvided())
	{
		mVisibleSignal.connect(initEnableCallback(p.on_visible));
	}
	// add a callback if on_enable is provided
	if (p.on_enable.isProvided())
	{
		setEnableCallback(initEnableCallback(p.on_enable));
		// Set the enabled control variable (for backwards compatability)
		if (p.on_enable.control_name.isProvided() && !p.on_enable.control_name().empty())
		{
			LLControlVariable* control = findControl(p.on_enable.control_name());
			if (control)
			{
				setEnabledControlVariable(control);
			}
		}
	}

	LLUICtrl::initFromParams(p);
}

// call this to make the menu update its "enabled" status
void PieSlice::updateEnabled()
{
	if(mDoUpdateEnabled && mEnableSignal.num_slots()>0)
	{
		bool enabled=mEnableSignal(this,LLSD());
		if(mEnabledControlVariable)
		{
			if(!enabled)
			{
				// callback overrides control variable; this will call setEnabled()
				mEnabledControlVariable->set(false);
			}
		}
		else
		{
			setEnabled(enabled);
		}

		mDoUpdateEnabled = !mCheckEnableOnce;
	}
}

// call this to make the menu update its "visible" status
void PieSlice::updateVisible()
{
	if(mVisibleSignal.num_slots()>0)
	{
		bool visible=mVisibleSignal(this,LLSD());
		setVisible(visible);
	}
}

// accessor
LLSD PieSlice::getValue() const
{
	return getLabel();
}

// accessor
void PieSlice::setValue(const LLSD& value)
{
	setLabel(value.asString());
}

// accessor
std::string PieSlice::getLabel() const
{
	return mLabel;
}

// accessor
void PieSlice::setLabel(const std::string newLabel)
{
	mLabel=newLabel;
}

// accessor
BOOL PieSlice::getStartAutohide()
{
	return mStartAutohide;
}

// accessor
BOOL PieSlice::getAutohide()
{
	return mStartAutohide | mAutohide;
}

void PieSlice::resetUpdateEnabledCheck()
{
	mDoUpdateEnabled = TRUE;
}

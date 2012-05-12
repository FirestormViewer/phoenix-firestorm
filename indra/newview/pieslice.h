/**
 * @file pieslice.h
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

#ifndef PIESLICE_H
#define PIESLICE_H

#include <boost/signals2.hpp>

#include "llinitparam.h"
#include "lluictrl.h"

// A slice in the pie. Does nothing by itself, just stores the function and
// parameter to be execued when the user clicks on this item
class PieSlice : public LLUICtrl
{
	public:
		// parameter block for the XUI factory
		struct Params : public LLInitParam::Block<Params, LLUICtrl::Params>
		{
			// register an on_click callback we can use in the XML definition
			Optional<CommitCallbackParam > on_click;
			// register an on_enable callback we can use in the XML definition
			Optional<EnableCallbackParam > on_enable;
			// register an on_visible callback which does the same as on_enable
			Optional<EnableCallbackParam > on_visible;

			// autohide feature to hide a disabled pie slice (NOTE: <bool> is not <BOOL>)
			Optional<bool> start_autohide;
			// next item in an autohide chain
			Optional<bool> autohide;
			// only do the enable check once
			Optional<bool> check_enable_once;
			// parameter "constructor" function to pick up the parameters from the XUI system
			Params();
		};

		PieSlice(const Params& p);

		// call these before checking on enabled/visible status
		void updateEnabled();
		void updateVisible();

		// initialisation, taking care of optional parameters and setting up the callback
		void initFromParams(const Params& p);

		// accessor to expose the label to the outside (value is the same as label)
		std::string getLabel() const;
		void setLabel(const std::string newLabel);
		LLSD getValue() const;
		void setValue(const LLSD& value);

		// accessor to expose the autohide feature
		BOOL getStartAutohide();
		BOOL getAutohide();

		// callback connection for the onCommit method to launch the specified function
		boost::signals2::connection setClickCallback(const commit_signal_t::slot_type& cb)
		{
			return setCommitCallback(cb);
		}

		// callback connection for the onEnable method to enable/disable menu items
		boost::signals2::connection setEnableCallback( const enable_signal_t::slot_type& cb )
		{
			return mEnableSignal.connect(cb);
		}

		void resetUpdateEnabledCheck();

	protected:
		// accessor store
		std::string mLabel;
		BOOL mStartAutohide;
		BOOL mAutohide;
		BOOL mCheckEnableOnce;
		BOOL mDoUpdateEnabled;

	private:
		enable_signal_t mEnableSignal;
		enable_signal_t mVisibleSignal;
};

#endif // PIESLICE_H

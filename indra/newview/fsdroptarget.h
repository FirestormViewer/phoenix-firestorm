/**
 * @file fsdroptarget.h
 * @brief Various drop targets for different purposes
 *
 * $LicenseInfo:firstyear=2014&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2014, Ansariel Hiller @ Second Life
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
 * http://www.firestormviewer.org
 * $/LicenseInfo$
 */

#ifndef FS_DROPTARGET_H
#define FS_DROPTARGET_H

#include "lllineeditor.h"
#include "lltextbox.h"

class LLInventoryItem;

class FSCopyTransInventoryDropTarget : public LLLineEditor
{
public:
	struct Params : public LLInitParam::Block<Params, LLLineEditor::Params>
	{
		Params()
		{}
	};

	FSCopyTransInventoryDropTarget(const Params& p)
		: LLLineEditor(p) {}

	~FSCopyTransInventoryDropTarget() {}

	typedef boost::signals2::signal<void(const LLUUID& id)> item_dad_callback_t;
	boost::signals2::connection setDADCallback(const item_dad_callback_t::slot_type& cb)
	{
		return mDADSignal.connect(cb);
	}

	virtual BOOL postBuild()
	{
		setEnabled(FALSE);
		return LLLineEditor::postBuild();
	}

	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);

private:
	item_dad_callback_t mDADSignal;
};


class FSDropTarget : public LLView
{
public:
	struct Params : public LLInitParam::Block<Params, LLView::Params>
	{
		Optional<LLUUID> agent_id;
		Params()
			: agent_id("agent_id")
		{
			changeDefault(mouse_opaque, false);
			changeDefault(follows.flags, FOLLOWS_ALL);
		}
	};

	FSDropTarget(const Params&);
	~FSDropTarget() {}

	// LLView functionality
	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);

	void setAgentID(const LLUUID &agent_id) { mAgentID = agent_id; }

protected:
	LLUUID mAgentID;
};


class FSEmbeddedItemDropTarget : public LLTextBox
{
public:
	struct Params : public LLInitParam::Block<Params, LLTextBox::Params>
	{
		Params()
		{}
	};

	FSEmbeddedItemDropTarget(const Params& p) : LLTextBox(p) {}
	~FSEmbeddedItemDropTarget() {}

	typedef boost::signals2::signal<void(const LLUUID& id)> item_dad_callback_t;
	boost::signals2::connection setDADCallback(const item_dad_callback_t::slot_type& cb)
	{
		return mDADSignal.connect(cb);
	}

	virtual BOOL handleDragAndDrop(S32 x, S32 y, MASK mask, BOOL drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data,
								   EAcceptance* accept,
								   std::string& tooltip_msg);

protected:
	item_dad_callback_t mDADSignal;
};

#endif // FS_DROPTARGET_H

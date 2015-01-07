/**
 * $LicenseInfo:firstyear=2015&license=viewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (c) 2015 Nicky Dasmijn
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

#ifndef FS_FLOATERVRAMUSAGE_H
#define FS_FLOATERVRAMUSAGE_H

#include "llfloater.h"
#include "llselectmgr.h"

class LLScrollListCtrl;
class LLViewerObject;
class LLFace;
class LLVertexBuffer;

class FSFloaterVRAMUsage : public LLFloater, public nd::selection::PropertiesListener
{
public:
	FSFloaterVRAMUsage(const LLSD& seed);
	/*virtual*/ ~FSFloaterVRAMUsage();
	/*virtual*/ void onOpen(const LLSD& key);

	BOOL postBuild();

	virtual void onProperties( LLSelectNode const * );

	void onIdle();

private:
	void doRefresh();

	void addObjectToList( LLViewerObject*, std::string const& );
	void calcFaceSize( LLFace *aFace, S32 &aW, S32 &aH );
	S32 calcVBOEntrySize( LLVertexBuffer *aVBO );
	U32 calcTexturSize( LLViewerObject*, std::ostream * = 0 );

	struct ImplData;
	ImplData *mData;
};

#endif // FS_FLOATERBLOCKLIST_H

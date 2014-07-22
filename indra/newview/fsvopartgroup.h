/** 
 * @file fsvopartgroup.h
 * @brief fsvopartgroup base class
 *
 * $LicenseInfo:firstyear=2014&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2014, The Phoenix Firestorm Project, Inc.
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
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */

#ifndef FS_VOPARTGROUP_H
#define FS_VOPARTGROUP_H

#include "stdtypes.h"

#define sFreeIndexSize LL_MAX_PARTICLE_COUNT/32

class FSVOPartGroup
{
public:
	FSVOPartGroup();
	~FSVOPartGroup() {};

	U32 getFreeIndex(int i) {return sFreeIndex[i];}
	U32 getIndexGeneration() {return sIndexGeneration;}
	U32 getTotalParticles() {return sTotalParticles;}

	void setFreeIndex(int i, U32 val) {sFreeIndex[i] = val;}
	void setIndexGeneration(U32 val) {sIndexGeneration = val;}
	void setTotalParticles(U32 val) {sTotalParticles = val;}

	bool findAvailableVBSlots( S32 &idxStart, S32 &idxEnd, U32 amount );

private:
	U32 sFreeIndex[sFreeIndexSize];
	U32 sIndexGeneration;
	U32 sTotalParticles;

	U32 bitMasks[ 32 ];
};
#endif // FS_VOPARTGROUP_H

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

#include "llviewerprecompiledheaders.h"

#include "fsfloatervramusage.h"
#include "llscrolllistctrl.h"
#include "llviewerobjectlist.h"
#include "lldrawable.h"
#include "llviewertexture.h"
#include "lltoolpie.h"
#include "llface.h"
#include "llvertexbuffer.h"
#include "llcallbacklist.h"
#include "llvoavatarself.h"
#include "llagentcamera.h"

const F32 PROPERTIES_REQUEST_TIMEOUT = 10.0f;
const F32 PROPERY_REQUEST_INTERVAL = 2.0f;
const U32 PROPERTIES_MAX_REQUEST_COUNT = 250;

static void onIdle( void *aData )
{
	FSFloaterVRAMUsage *pFloater = reinterpret_cast<FSFloaterVRAMUsage*>(aData);
	pFloater->onIdle();
}

struct ObjectStat
{
	LLUUID mId;
	U32 mTextureSize;
};

struct FSFloaterVRAMUsage::ImplData
{
	LLScrollListCtrl *mList;
	LLObjectSelectionHandle mSelection;
	U32 mPending;
	LLFrameTimer mPropTimer;

	std::deque< ObjectStat > mObjects;
};

bool sortByTexture( ObjectStat const &lhs, ObjectStat const &rhs )
{
	return lhs.mTextureSize > rhs.mTextureSize;
}

FSFloaterVRAMUsage::FSFloaterVRAMUsage(const LLSD& seed)
	: LLFloater( seed )
{
	mData = new ImplData();
	mData->mList = 0;
	mData->mPending = 0;

 	gIdleCallbacks.addFunction( &::onIdle, this) ;
}

FSFloaterVRAMUsage::~FSFloaterVRAMUsage()
{
 	gIdleCallbacks.deleteFunction( &::onIdle, this );
	delete mData;
	LLSelectMgr::instance().removePropertyListener( this );
	LLSelectMgr::instance().enableSilhouette( TRUE );
}

void FSFloaterVRAMUsage::onOpen(const LLSD& key)
{
}

BOOL FSFloaterVRAMUsage::postBuild()
{
	LLButton *pRefresh = getChild< LLButton >( "refresh_button" );
	pRefresh->setClickedCallback( boost::bind( &FSFloaterVRAMUsage::doRefresh, this ) );

	mData->mList = getChild< LLScrollListCtrl >( "result_list" );

	LLSelectMgr::instance().registerPropertyListener( this );
	LLSelectMgr::instance().enableSilhouette( FALSE );

	return TRUE;
}

void FSFloaterVRAMUsage::onIdle()
{
	if( !mData->mPending && mData->mObjects.empty() )
	{
		LLSelectMgr::instance().deselectAll();
		return;
	}
	
	if( mData->mPending && mData->mPropTimer.getElapsedTimeF32() < PROPERTIES_REQUEST_TIMEOUT ) 
		return;

	if( !mData->mPending && mData->mPropTimer.getStarted() && mData->mPropTimer.getElapsedTimeF32() < PROPERY_REQUEST_INTERVAL ) 
		return;

	LLSelectMgr::instance().deselectAll();
	mData->mPending = 0;

	std::vector< LLViewerObject* > vctSelection;
	std::deque< ObjectStat > withoutRegion;

	while( mData->mPending < PROPERTIES_MAX_REQUEST_COUNT && !mData->mObjects.empty() )
	{
		LLViewerObject *pObj = gObjectList.findObject(  mData->mObjects[ 0 ].mId );
		if( pObj && pObj->getRegion() )
		{
			++mData->mPending;
			vctSelection.push_back( pObj );
		}
		else if( pObj )
			withoutRegion.push_back( mData->mObjects[0] );

		mData->mObjects.erase( mData->mObjects.begin() );
	}

	mData->mObjects.insert( mData->mObjects.end(), withoutRegion.begin(), withoutRegion.end() );

	mData->mPropTimer.start();
	if( vctSelection.size() )
	{
		LLSelectMgr::instance().enableBatchMode();
		mData->mSelection = LLSelectMgr::instance().selectObjectAndFamily( vctSelection );
		LLSelectMgr::instance().disableBatchMode();
	}
}

U32 FSFloaterVRAMUsage::calcTexturSize( LLViewerObject *aObject, std::ostream *aTooltip )
{
	if( !aObject || !aObject->mDrawable || aObject->mDrawable->isDead() )
		return 0;

	std::map< LLUUID, U32 > stTextures;

	F64 totalTexSize = 0;
	U8 numTEs = aObject->getNumTEs();

	if (aTooltip )
		*aTooltip << (U32)numTEs << " TEs" << std::endl;

	for( U8 j = 0; j < numTEs; ++j )
	{
		LLViewerTexture *pTex = aObject->getTEImage( j );
		if( !pTex || pTex->isMissingAsset() )
			continue;

		U32 textureId = stTextures.size();
		bool bOldTexId( false );

		if( stTextures.end() != stTextures.find( pTex->getID() ) )
		{
			textureId = stTextures[ pTex->getID() ];
			bOldTexId = true;
		}

		if (aTooltip )
			*aTooltip << "TE: " << (U32)j << " tx: " << textureId	<< " w/h/c " << pTex->getFullWidth() << "/" << pTex->getFullHeight() << "/" << (U32)pTex->getComponents() << std::endl;

		if( bOldTexId )
			continue;

		stTextures[ pTex->getID() ] = textureId;

		S32 texSize = pTex->getFullWidth() * pTex->getFullHeight() * pTex->getComponents();
		if( pTex->getUseMipMaps() )
			texSize += (texSize*33)/100;

		totalTexSize += texSize;
	}

	totalTexSize /= 1024.0;
	return static_cast< U32 >( totalTexSize );
}

void FSFloaterVRAMUsage::doRefresh()
{
	mData->mList->deleteAllItems();
	S32 numObjects = gObjectList.getNumObjects();

	mData->mPending = 0;
	mData->mObjects.clear();
	LLSelectMgr::instance().deselectAll();

	for( S32 i = 0; i < numObjects; ++i )
	{
		LLViewerObject *pObj = gObjectList.getObject( i );
		if( !pObj ) // Might be dead
			continue;

		if( !pObj->mbCanSelect ||
			pObj->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH ||
			pObj->getPCode() == LLViewerObject::LL_VO_SKY ||
			pObj->getPCode() == LLViewerObject::LL_VO_WL_SKY ||
			pObj->getPCode() == LLViewerObject::LL_VO_VOID_WATER ||
			pObj->getPCode() == LLViewerObject::LL_VO_WATER ||
			pObj->isAvatar() )
			continue;

		// Exclude everything that's not in a sphere with r=draw distance around the avatar.
		F64 distance = (pObj->getPositionGlobal() - gAgentAvatarp->getPositionGlobal()).length();
		if( distance > gAgentCamera.mDrawDistance )
			continue;

		ObjectStat oObject;
		oObject.mId = pObj->getID();
		oObject.mTextureSize = calcTexturSize( pObj );

		mData->mObjects.push_back( oObject );
	}
	std::sort( mData->mObjects.begin(), mData->mObjects.end(), sortByTexture );
}

void FSFloaterVRAMUsage::addObjectToList( LLViewerObject *aObject, std::string const &aName )
{
	LLScrollListItem::Params item;

	F64 distance = (aObject->getPositionGlobal() - gAgentAvatarp->getPositionGlobal()).length();

	item.columns.add().column("uuid").value( aObject->getID() );
	item.columns.add().column("name").value( aName );
	item.columns.add().column("distance").value( distance );
	item.columns.add().column("faces").value( aObject->getNumFaces() );
	item.columns.add().column("vertices").value( static_cast<S32>( aObject->getNumVertices() ) );
	item.columns.add().column("indices").value( static_cast<S32>( aObject->getNumIndices() ) );

	std::stringstream strTooltip;
	U32 totalTexSize = calcTexturSize( aObject, &strTooltip );

	F64 totalVboSize(0.0);

	LLPointer< LLDrawable > pDrawable = aObject->mDrawable;
	S32 numFaces = 0;
	if( pDrawable && !pDrawable->isDead() )
		numFaces = pDrawable->getNumFaces();

	strTooltip << numFaces << " faces" << std::endl;
	for (S32 j = 0; j < numFaces; j++)
	{
		LLFace* pFace = pDrawable->getFace( j );
		if( !pFace )
			continue;

		S32 cmW = 0, cmH = 0;

		calcFaceSize( pFace, cmW, cmH );

		strTooltip << "Face: " << j << " extends (cm) w/h " << cmW << "/" << cmH << std::endl;

		S32 vertexSize = calcVBOEntrySize( pFace->getVertexBuffer() ) * aObject->getNumVertices();;
		S32 indexSize = sizeof( S16 ) * aObject->getNumIndices();

		totalVboSize += vertexSize;
		totalVboSize += indexSize;
	}

	totalVboSize /= 1024.0;

	item.columns.add().column("vram_usage").value( (S32)totalTexSize );
	item.columns.add().column("vram_usage_vbo").value( (S32)totalVboSize );

	LLScrollListItem *pRow = mData->mList->addRow( item );
	if( pRow )
	{
		for( S32 j = 0; j < pRow->getNumColumns(); ++j )
			pRow->getColumn( j )->setToolTip( strTooltip.str() );
	}
}

void FSFloaterVRAMUsage::calcFaceSize( LLFace *aFace, S32 &aW, S32 &aH )
{
	aW = aH = 0;
	if( !aFace )
		return;

	LLVector4a size;
	size.setSub( aFace->mExtents[1], aFace->mExtents[0] );

	S32 cmX = static_cast<S32>( size[0]*100 );
	S32 cmY = static_cast<S32>( size[1]*100 );
	S32 cmZ = static_cast<S32>( size[2]*100 );

	aW = cmX;
	aH = cmY;

	if( 0 != cmZ )
	{
		if( 0 == aW )
			aW = cmZ;
		else
			aH = cmZ;
	}
}

S32 FSFloaterVRAMUsage::calcVBOEntrySize( LLVertexBuffer *aVBO )
{
	if( !aVBO )
		return 0;

	S32 vboEntrySize(0);

	U32 vboMask = aVBO->getTypeMask();
	for( S32 k = 0, l = 1; k < LLVertexBuffer::TYPE_MAX; ++k )
	{
		if( vboMask & l && k != LLVertexBuffer::TYPE_TEXTURE_INDEX )
			vboEntrySize += LLVertexBuffer::sTypeSize[ k ];
		l = l << 1;
	}

	return vboEntrySize;
}

void FSFloaterVRAMUsage::onProperties( LLSelectNode const *aProps )
{
	if( !aProps || !aProps->getObject() )
		return;

	LLUUID id = aProps->getObject()->getID();
	LLViewerObject *pObj = gObjectList.findObject( id );
	addObjectToList( pObj, aProps->mName );
}

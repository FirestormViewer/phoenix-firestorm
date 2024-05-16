/**
 * @file llviewertexlayer.h
 * @brief Viewer Texture layer classes. Used for avatars.
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_VIEWER_TEXLAYER_H
#define LL_VIEWER_TEXLAYER_H

#include "lldynamictexture.h"
#include "llextendedstatus.h"
#include "lltexlayer.h"

class LLVOAvatarSelf;
class LLViewerTexLayerSetBuffer;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLViewerTexLayerSet
//
// An ordered set of texture layers that gets composited into a single texture.
// Only exists for llavatarappearanceself.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLViewerTexLayerSet : public LLTexLayerSet
{
public:
    LLViewerTexLayerSet(LLAvatarAppearance* const appearance);
    virtual ~LLViewerTexLayerSet();

    /*virtual*/void             requestUpdate();
    // <FS:Ansariel> [Legacy Bake]
    void                        requestUpload();
    void                        cancelUpload();
    // </FS:Ansariel> [Legacy Bake]
    bool                        isLocalTextureDataAvailable() const;
    bool                        isLocalTextureDataFinal() const;
    void                        updateComposite();
    /*virtual*/void             createComposite();
    void                        setUpdatesEnabled(bool b);
    bool                        getUpdatesEnabled() const   { return mUpdatesEnabled; }

    LLVOAvatarSelf*             getAvatar();
    const LLVOAvatarSelf*       getAvatar() const;
    LLViewerTexLayerSetBuffer*  getViewerComposite();
    const LLViewerTexLayerSetBuffer*    getViewerComposite() const;

private:
    bool                        mUpdatesEnabled;

};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLViewerTexLayerSetBuffer
//
// The composite image that a LLViewerTexLayerSet writes to.  Each LLViewerTexLayerSet has one.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLViewerTexLayerSetBuffer : public LLTexLayerSetBuffer, public LLViewerDynamicTexture
{
    LOG_CLASS(LLViewerTexLayerSetBuffer);

public:
    LLViewerTexLayerSetBuffer(LLTexLayerSet* const owner, S32 width, S32 height);
    virtual ~LLViewerTexLayerSetBuffer();

public:
    /*virtual*/ S8          getType() const;
    bool                    isInitialized(void) const;
    static void             dumpTotalByteCount();
    const std::string       dumpTextureInfo() const;
    virtual void            restoreGLTexture();
    virtual void            destroyGLTexture();
private:
    LLViewerTexLayerSet*    getViewerTexLayerSet()
        { return dynamic_cast<LLViewerTexLayerSet*> (mTexLayerSet); }
    const LLViewerTexLayerSet*  getViewerTexLayerSet() const
        { return dynamic_cast<const LLViewerTexLayerSet*> (mTexLayerSet); }
    static S32              sGLByteCount;

    //--------------------------------------------------------------------
    // Tex Layer Render
    //--------------------------------------------------------------------
    virtual void            preRenderTexLayerSet();
    // <FS:Ansariel> [Legacy Bake]
    //virtual void          midRenderTexLayerSet(bool success);
    virtual void            midRenderTexLayerSet(bool success, LLRenderTarget* bound_target);
    // </FS:Ansariel> [Legacy Bake]
    virtual void            postRenderTexLayerSet(bool success);
    virtual S32             getCompositeOriginX() const { return getOriginX(); }
    virtual S32             getCompositeOriginY() const { return getOriginY(); }
    virtual S32             getCompositeWidth() const { return getFullWidth(); }
    virtual S32             getCompositeHeight() const { return getFullHeight(); }

    //--------------------------------------------------------------------
    // Dynamic Texture Interface
    //--------------------------------------------------------------------
public:
    /*virtual*/ bool        needsRender();
protected:
    // Pass these along for tex layer rendering.
    virtual void            preRender(bool clear_depth) { preRenderTexLayerSet(); }
    virtual void            postRender(bool success) { postRenderTexLayerSet(success); }
    virtual bool            render() { return renderTexLayerSet(mBoundTarget); }

// <FS:Ansariel> [Legacy Bake]
    //--------------------------------------------------------------------
    // Uploads
    //--------------------------------------------------------------------
public:
    void                    requestUpload();
    void                    cancelUpload();
    bool                    uploadNeeded() const;           // We need to upload a new texture
    bool                    uploadInProgress() const;       // We have started uploading a new texture and are awaiting the result
    bool                    uploadPending() const;          // We are expecting a new texture to be uploaded at some point
    static void             onTextureUploadComplete(const LLUUID& uuid,
                                                    void* userdata,
                                                    S32 result, LLExtStat ext_status);
protected:
    bool                    isReadyToUpload() const;
    void                    doUpload(LLRenderTarget* bound_target);                     // Does a read back and upload.
    void                    conditionalRestartUploadTimer();
private:
    bool                    mNeedsUpload;                   // Whether we need to send our baked textures to the server
    U32                     mNumLowresUploads;              // Number of times we've sent a lowres version of our baked textures to the server
    bool                    mUploadPending;                 // Whether we have received back the new baked textures
    LLUUID                  mUploadID;                      // The current upload process (null if none).
    LLFrameTimer            mNeedsUploadTimer;              // Tracks time since upload was requested and performed.
    S32                     mUploadFailCount;               // Number of consecutive upload failures
    LLFrameTimer            mUploadRetryTimer;              // Tracks time since last upload failure.
// </FS:Ansariel> [Legacy Bake]

    //--------------------------------------------------------------------
    // Updates
    //--------------------------------------------------------------------
public:
    void                    requestUpdate();
    bool                    requestUpdateImmediate();
protected:
    bool                    isReadyToUpdate() const;
    void                    doUpdate();
    void                    restartUpdateTimer();
private:
    bool                    mNeedsUpdate;                   // Whether we need to locally update our baked textures
    U32                     mNumLowresUpdates;              // Number of times we've locally updated with lowres version of our baked textures
    LLFrameTimer            mNeedsUpdateTimer;              // Tracks time since update was requested and performed.
};

// <FS:Ansariel> [Legacy Bake]
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLBakedUploadData
//
// Used by LLTexLayerSetBuffer for a callback.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
struct LLBakedUploadData
{
    LLBakedUploadData(const LLVOAvatarSelf* avatar,
                      LLViewerTexLayerSet* layerset,
                      const LLUUID& id,
                      bool highest_res);
    ~LLBakedUploadData() {}
    const LLUUID                mID;
    const LLVOAvatarSelf*       mAvatar; // note: backlink only; don't LLPointer
    LLViewerTexLayerSet*        mTexLayerSet;
    const U64                   mStartTime; // for measuring baked texture upload time
    const bool                  mIsHighestRes; // whether this is a "final" bake, or intermediate low res
};
// </FS:Ansariel> [Legacy Bake]

#endif  // LL_VIEWER_TEXLAYER_H


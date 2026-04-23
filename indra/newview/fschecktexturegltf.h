/**
 * @file fschecktexturegltf.h
 * @brief Save selected object mesh+textures as a single .glb without permission checks.
 */

#ifndef FSCHECKTEXTUREGLTF_H
#define FSCHECKTEXTUREGLTF_H

#include "lltexturecache.h"
#include "lltimer.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class LLViewerObject;
class LLImageFormatted;

// Exports the current selection as a binary glTF (.glb) file.
// Mesh geometry and textures (including PBR) are embedded in the single file.
// Unlike the standard DAE exporter this class skips FSExportPermsCheck so any
// object can be captured.  Coordinate system is converted from SL Z-up to
// glTF Y-up.
class FSCheckTextureGLTF
{
public:
    FSCheckTextureGLTF();
    ~FSCheckTextureGLTF();

    // Entry point: show a save-file dialog then export.
    void exportSelection();

private:
    struct PrimEntry { LLViewerObject* obj; std::string name; };

    void onFileSelected(const std::vector<std::string>& filenames);
    void startTextureFetch();
    void onTexturesFetched();
    void addSelectedObjects();
    bool buildAndSaveGLTF();

    std::vector<PrimEntry>                               mObjects;
    LLVector3                                            mOffset;
    std::string                                          mFilename;
    std::string                                          mObjectName;

    // UUID → decoded PNG bytes (populated by CacheReadResponder)
    std::map<LLUUID, std::vector<unsigned char>>         mTexturePNG;
    // UUIDs still waiting for texture download to reach discard level 0
    std::map<LLUUID, std::string>                        mTexturesToFetch;
    // UUIDs where readFromCache is in flight (completed() not yet called)
    std::set<LLUUID>                                     mTexturesPending;
    // Layer textures (bake-on-mesh, self only): UUID → bake channel dir name
    // e.g. "BAKED_HEAD".  Absent means regular texture (goes in textures/).
    std::map<LLUUID, std::string>                        mTextureLayerDir;

    LLTimer mTimer;

    class CacheReadResponder : public LLTextureCache::ReadResponder
    {
    public:
        CacheReadResponder(const LLUUID& id, LLImageFormatted* image,
                           FSCheckTextureGLTF* owner);

        void setData(U8* data, S32 datasize, S32 imagesize,
                     S32 imageformat, bool imagelocal) override;
        void completed(bool success) override;

        static void fetchWorker(void* data);

    private:
        LLPointer<LLImageFormatted> mFormattedImage;
        LLUUID                      mID;
        FSCheckTextureGLTF*         mOwner;
    };
};

#endif // FSCHECKTEXTUREGLTF_H

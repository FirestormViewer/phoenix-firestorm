/**
 * @file fschecktexture.h
 * @brief Save selected object mesh+textures as DAE+PNG without permission checks.
 */

#ifndef FSCHECKTEXTURE_H
#define FSCHECKTEXTURE_H

#include "daeexport.h"
#include "lltimer.h"

// Saves the current selection as a Collada DAE file with PNG textures.
// Unlike the standard ColladaExportFloater this class skips all
// FSExportPermsCheck calls so any object (including third-party content)
// can be captured.
class FSCheckTexture
{
public:
    FSCheckTexture();
    ~FSCheckTexture();

    // Entry point: show a save-file dialog then export.
    void exportSelection();

private:
    void onFileSelected(const std::vector<std::string>& filenames);
    void onTexturesSaved();
    void saveTextures();
    void addSelectedObjects();

    DAESaver mSaver;
    std::string mFilename;
    std::string mObjectName;

    LLTimer mTimer;
    typedef std::map<LLUUID, std::string> texture_list_t;
    texture_list_t mTexturesToSave;

    class CacheReadResponder : public LLTextureCache::ReadResponder
    {
        friend class FSCheckTexture;
    public:
        CacheReadResponder(const LLUUID& id, LLImageFormatted* image,
                           std::string name);

        void setData(U8* data, S32 datasize, S32 imagesize,
                     S32 imageformat, bool imagelocal) override;
        void completed(bool success) override;

        static void saveTexturesWorker(void* data);

    private:
        LLPointer<LLImageFormatted> mFormattedImage;
        LLUUID mID;
        std::string mName;
    };
};

#endif // FSCHECKTEXTURE_H

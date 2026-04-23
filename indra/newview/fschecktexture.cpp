/**
 * @file fschecktexture.cpp
 * @brief Save selected object mesh+textures as DAE+PNG without permission checks.
 */

#include "llviewerprecompiledheaders.h"
#include "fschecktexture.h"

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include "llimagej2c.h"
#include "llimagepng.h"

#include "llappviewer.h"
#include "llcallbacklist.h"
#include "llfilepicker.h"
#include "llviewermenufile.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "llviewertexturelist.h"

static constexpr F32 CT_TEXTURE_TIMEOUT = 60.f;

// ---------------------------------------------------------------------------
// FSCheckTexture
// ---------------------------------------------------------------------------

FSCheckTexture::FSCheckTexture()
{
}

FSCheckTexture::~FSCheckTexture()
{
    if (gIdleCallbacks.containsFunction(CacheReadResponder::saveTexturesWorker, this))
    {
        gIdleCallbacks.deleteFunction(CacheReadResponder::saveTexturesWorker, this);
    }
}

void FSCheckTexture::exportSelection()
{
    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getSelection();
    if (!sel->getPrimaryObject())
    {
        LL_WARNS() << "FSCheckTexture: no object selected" << LL_ENDL;
        return;
    }

    addSelectedObjects();
    if (mSaver.mObjects.empty())
    {
        LL_WARNS() << "FSCheckTexture: selection has no geometry" << LL_ENDL;
        return;
    }

    // Always export textures as PNG; set image format before file dialog.
    mSaver.mImageFormat = "png";

    LLFilePickerReplyThread::startPicker(
        boost::bind(&FSCheckTexture::onFileSelected, this, _1),
        LLFilePicker::FFSAVE_COLLADA,
        LLDir::getScrubbedFileName(mObjectName + ".dae"));
}

void FSCheckTexture::addSelectedObjects()
{
    mSaver.mObjects.clear();
    mSaver.mTextures.clear();
    mSaver.mTextureNames.clear();

    LLObjectSelectionHandle sel = LLSelectMgr::getInstance()->getEditSelection();
    LLSelectNode* root_node = sel->getFirstRootNode();
    if (!root_node)
        return;

    mObjectName = root_node->mName;
    mSaver.mOffset = -sel->getFirstRootObject()->getRenderPosition();

    for (LLObjectSelection::iterator it = sel->begin(); it != sel->end(); ++it)
    {
        LLSelectNode* node = *it;
        LLViewerObject* obj = node->getObject();
        if (!obj || !obj->getVolume())
            continue;
        mSaver.add(obj, node->mName);
    }

    // Collect texture UUIDs for every face of every selected object.
    // We use UUID strings as safe filenames to avoid any name collisions.
    for (auto& entry : mSaver.mObjects)
    {
        LLViewerObject* obj = entry.first;
        S32 nfaces = obj->getVolume()->getNumVolumeFaces();
        for (S32 f = 0; f < nfaces; ++f)
        {
            LLTextureEntry* te = obj->getTE(f);
            if (!te) continue;
            const LLUUID id = te->getID();
            if (id.isNull()) continue;
            if (std::find(mSaver.mTextures.begin(), mSaver.mTextures.end(), id)
                    != mSaver.mTextures.end())
                continue;

            mSaver.mTextures.push_back(id);
            // Use UUID as the filename so it is always unique and safe.
            mSaver.mTextureNames.push_back(id.asString());

            // Ensure the texture is being fetched at full resolution.
            LLViewerFetchedTexture* tex =
                LLViewerTextureManager::getFetchedTexture(id);
            if (tex)
                tex->setBoostLevel(LLGLTexture::BOOST_MAX_LEVEL);
        }
    }
}

void FSCheckTexture::onFileSelected(const std::vector<std::string>& filenames)
{
    if (filenames.empty()) return;
    mFilename = filenames[0];
    saveTextures();
}

void FSCheckTexture::saveTextures()
{
    mTexturesToSave.clear();
    for (size_t i = 0; i < mSaver.mTextures.size(); ++i)
    {
        if (mSaver.mTextureNames[i].empty()) continue;
        mTexturesToSave[mSaver.mTextures[i]] = mSaver.mTextureNames[i];
    }

    mTimer.start();
    mTimer.setTimerExpirySec(CT_TEXTURE_TIMEOUT);
    gIdleCallbacks.addFunction(CacheReadResponder::saveTexturesWorker, this);
}

void FSCheckTexture::onTexturesSaved()
{
    bool ok = mSaver.saveDAE(mFilename);
    if (ok)
        LL_INFOS() << "FSCheckTexture: DAE saved to " << mFilename << LL_ENDL;
    else
        LL_WARNS() << "FSCheckTexture: DAE save failed" << LL_ENDL;

    // Self-destruct: we were heap-allocated by the menu handler.
    delete this;
}

// ---------------------------------------------------------------------------
// FSCheckTexture::CacheReadResponder
// ---------------------------------------------------------------------------

FSCheckTexture::CacheReadResponder::CacheReadResponder(
        const LLUUID& id, LLImageFormatted* image, std::string name)
    : mFormattedImage(image), mID(id), mName(std::move(name))
{
    setImage(image);
}

void FSCheckTexture::CacheReadResponder::setData(
        U8* data, S32 datasize, S32 imagesize, S32 imageformat, bool imagelocal)
{
    if (imageformat == IMG_CODEC_TGA && mFormattedImage->getCodec() == IMG_CODEC_J2C)
    {
        LL_WARNS() << "FSCheckTexture: texture " << mID << " is TGA, skipping" << LL_ENDL;
        mFormattedImage = nullptr;
        mImageSize = 0;
        return;
    }

    if (mFormattedImage.notNull())
    {
        if (mFormattedImage->getCodec() == imageformat)
            mFormattedImage->appendData(data, datasize);
        else
        {
            LL_WARNS() << "FSCheckTexture: texture " << mID << " wrong format" << LL_ENDL;
            mFormattedImage = nullptr;
            mImageSize = 0;
            return;
        }
    }
    else
    {
        mFormattedImage = LLImageFormatted::createFromType(imageformat);
        mFormattedImage->setData(data, datasize);
    }
    mImageSize = imagesize;
    mImageLocal = imagelocal;
}

void FSCheckTexture::CacheReadResponder::completed(bool success)
{
    if (!success || mFormattedImage.isNull() || mImageSize == 0)
    {
        LL_WARNS() << "FSCheckTexture: failed to read texture " << mID << LL_ENDL;
        return;
    }

    if (!mFormattedImage->updateData() ||
        mFormattedImage->getWidth() * mFormattedImage->getHeight() *
            mFormattedImage->getComponents() == 0)
    {
        LL_WARNS() << "FSCheckTexture: invalid image data for " << mID << LL_ENDL;
        return;
    }

    mFormattedImage->setDiscardLevel(0);

    LLPointer<LLImageRaw> raw = new LLImageRaw;
    raw->resize(mFormattedImage->getWidth(),
                mFormattedImage->getHeight(),
                mFormattedImage->getComponents());

    if (!mFormattedImage->decode(raw, 0))
    {
        LL_WARNS() << "FSCheckTexture: decode failed for " << mID << LL_ENDL;
        return;
    }

    LLPointer<LLImagePNG> png = new LLImagePNG;
    if (png->encode(raw, 0))
    {
        std::string path = mName + ".png";
        if (png->save(path))
            LL_INFOS() << "FSCheckTexture: saved " << path << LL_ENDL;
        else
            LL_WARNS() << "FSCheckTexture: save failed for " << path << LL_ENDL;
    }
}

//static
void FSCheckTexture::CacheReadResponder::saveTexturesWorker(void* data)
{
    FSCheckTexture* self = static_cast<FSCheckTexture*>(data);

    if (self->mTexturesToSave.empty())
    {
        gIdleCallbacks.deleteFunction(saveTexturesWorker, self);
        self->mTimer.stop();
        self->onTexturesSaved();
        return;
    }

    LLUUID id = self->mTexturesToSave.begin()->first;
    LLViewerTexture* imagep =
        LLViewerTextureManager::findFetchedTexture(id, TEX_LIST_STANDARD);

    if (!imagep)
    {
        self->mTexturesToSave.erase(id);
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_TEXTURE_TIMEOUT);
        return;
    }

    if (imagep->getDiscardLevel() == 0)
    {
        std::string dir  = gDirUtilp->getDirName(self->mFilename);
        std::string name = dir + gDirUtilp->getDirDelimiter() +
                           self->mTexturesToSave[id];

        LLImageFormatted* img = new LLImageJ2C;
        S32 sz = LLImageJ2C::calcDataSizeJ2C(
            imagep->getFullWidth(), imagep->getFullHeight(),
            imagep->getComponents(), 0);

        CacheReadResponder* responder =
            new CacheReadResponder(id, img, name);
        LLAppViewer::getTextureCache()->readFromCache(id, 0, sz, responder);

        self->mTexturesToSave.erase(id);
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_TEXTURE_TIMEOUT);
    }
    else if (self->mTimer.hasExpired())
    {
        LL_WARNS() << "FSCheckTexture: timed out on texture " << id << LL_ENDL;
        self->mTexturesToSave.erase(id);
        self->mTimer.reset();
        self->mTimer.setTimerExpirySec(CT_TEXTURE_TIMEOUT);
    }
}

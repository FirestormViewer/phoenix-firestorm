#include "llvkimagepublication.h"

bool LLVKImagePublication::advance(std::shared_ptr<const LLVKWidgetImage> latest, std::string& error)
{
    error = mFailure;
    if (!error.empty()) return false;
    if (!latest) mCurrent = {};
    if (mUpload)
    {
        const auto status = mUpload->poll(error);
        if (status == LLVKGlyphUpload::Status::Failed) { mFailure = error; return false; }
        if (status == LLVKGlyphUpload::Status::Pending) return true;
        if (latest && latest->pixelWidth() == mUploading->pixelWidth() && latest->pixelHeight() == mUploading->pixelHeight())
            mCurrent = {mUploading,mUpload->published()};
        mUpload.reset();
        mUploading.reset();
    }
    if (!latest || latest == mCurrent.source) return true;
    mUpload = LLVKGlyphUpload::submit(mDevice,{latest->pixelWidth(),latest->pixelHeight()},latest->bottomUpRgba(),error,
        LLVKGlyphUpload::Sampling::SkinLinearClamp);
    if (!mUpload) { mFailure = error; return false; }
    mUploading = std::move(latest);
    return true;
}
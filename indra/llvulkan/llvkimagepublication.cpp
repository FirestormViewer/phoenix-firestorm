#include "llvkimagepublication.h"

void LLVKImagePublication::invalidate() noexcept
{
    mCurrent = {};
    mDiscardUpload = bool(mUpload);
}

bool LLVKImagePublication::waitPendingUpload(std::uint64_t timeout,std::string& error)
{
    error=mFailure;
    if (!error.empty()) return false;
    if (!mUpload) return true;
    const auto status=mUpload->wait(timeout,error);
    if (status==LLVKGlyphUpload::Status::Failed) mFailure=error;
    return status==LLVKGlyphUpload::Status::Ready;
}

bool LLVKImagePublication::advance(std::shared_ptr<const LLVKWidgetImage> latest, std::string& error)
{
    error = mFailure;
    if (!error.empty()) return false;
    if (!latest) invalidate();
    if (mUpload)
    {
        const auto status = mUpload->poll(error);
        if (status == LLVKGlyphUpload::Status::Failed) { mFailure = error; return false; }
        if (status == LLVKGlyphUpload::Status::Pending) return true;
        if (!mDiscardUpload && latest && latest->pixelWidth() == mUploading->pixelWidth() && latest->pixelHeight() == mUploading->pixelHeight())
            mCurrent = {mUploading,mUpload->published()};
        mUpload.reset();
        mUploading.reset();
        mDiscardUpload = false;
    }
    if (!latest || latest == mCurrent.source) return true;
    mUpload = LLVKGlyphUpload::submit(mDevice,{latest->pixelWidth(),latest->pixelHeight()},latest->bottomUpRgba(),error,
        LLVKGlyphUpload::Sampling::SkinLinearClamp);
    if (!mUpload) { mFailure = error; return false; }
    mUploading = std::move(latest);
    return true;
}
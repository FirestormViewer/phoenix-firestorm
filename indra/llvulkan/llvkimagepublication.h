#ifndef LLVKIMAGEPUBLICATION_H
#define LLVKIMAGEPUBLICATION_H

#include "llvkwidgetimage.h"
#include "llvkglyphupload.h"

class LLVKImagePublication final
{
public:
    struct Version
    {
        std::shared_ptr<const LLVKWidgetImage> source;
        std::shared_ptr<const LLVKGlyphImage> image;
    };
    explicit LLVKImagePublication(LLVKGlyphUpload::Device device) : mDevice(device) {}
    bool advance(std::shared_ptr<const LLVKWidgetImage> latest, std::string& error);
    bool waitPendingUpload(std::uint64_t timeout, std::string& error);
    const Version& current() const noexcept { return mCurrent; }
    bool pending() const noexcept { return bool(mUpload); }
    std::size_t residentBytes() const noexcept
    { return (mCurrent.source ? mCurrent.source->bottomUpRgba().size() : 0)+(mUploading ? mUploading->bottomUpRgba().size() : 0); }
private:
    LLVKGlyphUpload::Device mDevice;
    Version mCurrent;
    std::shared_ptr<const LLVKWidgetImage> mUploading;
    std::unique_ptr<LLVKGlyphUpload> mUpload;
    std::string mFailure;
};

#endif
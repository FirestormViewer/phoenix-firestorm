#include "llvkwidgetgpu.h"
#include <algorithm>
#include <cmath>
#include <fstream>

std::optional<std::vector<std::filesystem::path>> LLVKWidgetGpu::dumpFontAtlases(const std::filesystem::path& directory,std::string& error) const
{
    error.clear();
    if (!directory.is_absolute()) { error="Native font atlas dump requires an absolute directory"; return std::nullopt; }
    std::error_code status;
    std::filesystem::create_directories(directory.parent_path(),status);
    if (status || !std::filesystem::create_directory(directory,status))
    { error="Cannot create a new native font atlas dump directory: "+directory.string(); return std::nullopt; }
    std::vector<std::filesystem::path> files;
    for (const auto& [key,text] : mTexts)
    {
        if (!text.atlas) continue;
        const auto size=text.atlas->pageSize();
        std::size_t index=0;
        for (const auto& page : text.atlas->pages())
        {
            const auto encoded=LLVKWidgetImage::encodePng(size,size,page.rgba,error);
            if (!encoded) return std::nullopt;
            const auto path=directory/("native-glyph-"+std::to_string(key.first)+"-"+std::to_string(key.second)+"-"+
                std::to_string(index++)+(page.encoding==LLVKFontFace::PixelEncoding::Coverage8 ? "-coverage.png" : "-color.png"));
            std::ofstream output(path,std::ios::binary|std::ios::trunc);
            output.write(reinterpret_cast<const char*>(encoded->data()),static_cast<std::streamsize>(encoded->size()));
            output.close();
            if (!output) { error="Native font atlas write failed: "+path.string(); return std::nullopt; }
            files.push_back(path);
        }
    }
    return files;
}

bool LLVKWidgetGpu::waitPendingUploads(std::uint64_t timeout,std::string& error)
{
    error.clear();
    for (auto& [key,image] : mImages)
        if (image.upload && image.upload->wait(timeout,error)!=LLVKGlyphUpload::Status::Ready) return false;
    for (auto& [key,text] : mTexts)
        for (auto& upload : text.uploads)
            if (upload && upload->wait(timeout,error)!=LLVKGlyphUpload::Status::Ready) return false;
    for (auto& [owner,stream] : mStreams)
        if (!stream.publication->waitPendingUpload(timeout,error)) return false;
    return true;
}

LLVKWidgetGpu::Status LLVKWidgetGpu::prepare(const LLVKWidgetPaint& paint, VkExtent2D extent,
    LLVKUiPacket& packet, std::string& error)
{
    error.clear();
    const auto scale=paint.displayScale;
    if (!std::isfinite(scale) || scale<=0.f || scale>8.f)
    { error="Invalid native paint display scale"; return Status::Failed; }
    ++mFrame;
    bool pending = !paint.pendingBrowsers.empty();
    const auto poll = [&](std::unique_ptr<LLVKGlyphUpload>& upload,std::shared_ptr<const LLVKGlyphImage>& ready) -> bool
    {
        if (!upload) return true;
        const auto status = upload->poll(error);
        if (status == LLVKGlyphUpload::Status::Failed) return false;
        if (status == LLVKGlyphUpload::Status::Ready) { ready = upload->published(); upload.reset(); }
        return true;
    };
    for (auto& [key,image] : mImages) if (!poll(image.upload,image.ready)) return Status::Failed;
    for (auto& [key,text] : mTexts)
        for (std::size_t page = 0; page < text.uploads.size(); ++page)
            if (!poll(text.uploads[page],text.pages[page])) return Status::Failed;
    const auto same = [](const LLVKFont::LineLayout& first,const LLVKFont::LineLayout& second)
    {
        if (first.glyphs.size() != second.glyphs.size()) return false;
        for (std::size_t index = 0; index < first.glyphs.size(); ++index)
        {
            const auto& before = first.glyphs[index]; const auto& after = second.glyphs[index];
            if (before.glyph != after.glyph || before.left != after.left || before.right != after.right ||
                before.top != after.top || before.bottom != after.bottom) return false;
        }
        return true;
    };
    std::map<LLVKWidgetTree::Id,std::size_t> parts;
    for (const auto& command : paint.commands)
    {
        const auto key = std::pair{command.owner,parts[command.owner]++};
        if (command.image && command.streamingImage)
        {
            auto& stream = mStreams[command.owner];
            stream.used = mFrame;
            if (!stream.publication) stream.publication = std::make_unique<LLVKImagePublication>(mDevice);
            if (stream.epoch != command.imageEpoch)
            {
                stream.publication->invalidate();
                stream.epoch = command.imageEpoch;
            }
            if (!stream.publication->advance(command.image,error)) return Status::Failed;
            const auto& current = stream.publication->current();
            pending |= !current.image || current.source->pixelWidth() != command.image->pixelWidth() || current.source->pixelHeight() != command.image->pixelHeight();
        }
        else if (command.image)
        {
            const auto imageKey=std::pair{command.image.get(),paint.skinAnisotropy};
            auto& image = mImages[imageKey];
            image.used = mFrame;
            if (!image.source)
            {
                image.source = command.image;
                image.upload = LLVKGlyphUpload::submit(mDevice,{command.image->pixelWidth(),command.image->pixelHeight()},
                    command.image->bottomUpRgba(),error,paint.skinAnisotropy ? LLVKGlyphUpload::Sampling::SkinAnisotropicClamp : LLVKGlyphUpload::Sampling::SkinLinearClamp);
                if (!image.upload) { mImages.erase(imageKey); return Status::Failed; }
            }
            pending |= !image.ready;
        }
        if (command.text && !command.text->glyphs.empty())
        {
            auto deviceText=*command.text;
            if (scale!=1.f)
                for (auto& glyph : deviceText.glyphs)
                {
                    glyph.left*=scale; glyph.right*=scale;
                    glyph.bottom*=scale; glyph.top*=scale;
                }
            auto& text = mTexts[key];
            text.used = mFrame;
            if (!text.atlas || !same(text.layout,deviceText))
            {
                const bool uploading = std::any_of(text.uploads.begin(),text.uploads.end(),[](const auto& upload) { return bool(upload); });
                if (uploading) { pending = true; continue; }
                Text replacement;
                replacement.used = mFrame;
                replacement.layout = deviceText;
                replacement.atlas = LLVKGlyphAtlas::prepare(deviceText,256,16*1024*1024,error);
                if (!replacement.atlas) return Status::Failed;
                for (const auto& page : replacement.atlas->pages())
                {
                    auto upload = LLVKGlyphUpload::submit(mDevice,{256,256},page.rgba,error);
                    if (!upload) return Status::Failed;
                    replacement.uploads.push_back(std::move(upload));
                    replacement.pages.push_back({});
                }
                text = std::move(replacement);
            }
            pending |= std::any_of(text.pages.begin(),text.pages.end(),[](const auto& page) { return !page; });
        }
    }
    std::erase_if(mImages,[&](const auto& entry) { return entry.second.used != mFrame && !entry.second.upload; });
    for (auto& [owner,stream] : mStreams)
        if (stream.used != mFrame && !stream.publication->advance({},error)) return Status::Failed;
    std::erase_if(mStreams,[&](const auto& entry) { return entry.second.used != mFrame && !entry.second.publication->pending(); });
    std::erase_if(mTexts,[&](const auto& entry)
    { return entry.second.used != mFrame && std::none_of(entry.second.uploads.begin(),entry.second.uploads.end(),[](const auto& upload) { return bool(upload); }); });
    std::uint64_t resident = 0;
    for (const auto& [owner,stream] : mStreams) resident += stream.publication->residentBytes();
    for (const auto& [key,image] : mImages) resident += image.source->bottomUpRgba().size();
    for (const auto& [key,text] : mTexts) if (text.atlas) resident += text.atlas->pages().size()*256*256*4;
    if (mImages.size()+mTexts.size()+mStreams.size() > 10000 || resident > 256*1024*1024)
    { error = "Native widget GPU cache exceeds its resource budget"; return Status::Failed; }
    if (pending) return Status::Pending;
    LLVKUiPacket prepared(extent);
    parts.clear();
    for (const auto& command : paint.commands)
    {
        const auto key = std::pair{command.owner,parts[command.owner]++};
        if (command.clip.left < 0 || command.clip.bottom < 0 || command.clip.right > std::ceil(extent.width/scale) ||
            command.clip.top > std::ceil(extent.height/scale))
        { error = "Native widget paint clip is outside framebuffer"; return Status::Failed; }
        if (command.clip.right <= command.clip.left || command.clip.top <= command.clip.bottom) continue;
        const auto clipLeft=std::clamp(static_cast<int>(std::floor(command.clip.left*scale)),0,static_cast<int>(extent.width));
        const auto clipRight=std::clamp(static_cast<int>(std::floor(command.clip.left*scale)+std::ceil((command.clip.right-command.clip.left-1)*scale)+1),0,static_cast<int>(extent.width));
        const auto clipBottom=std::clamp(static_cast<int>(std::floor(command.clip.bottom*scale)),0,static_cast<int>(extent.height));
        const auto clipTop=std::clamp(static_cast<int>(std::floor(command.clip.bottom*scale)+std::ceil((command.clip.top-command.clip.bottom-1)*scale)+1),0,static_cast<int>(extent.height));
        if (clipRight<=clipLeft || clipTop<=clipBottom) continue;
        const VkRect2D clip{{clipLeft,static_cast<int>(extent.height)-clipTop},
            {static_cast<std::uint32_t>(clipRight-clipLeft),static_cast<std::uint32_t>(clipTop-clipBottom)}};
        const auto& rect = command.rectangle;
        if (command.triangle)
        {
            auto points=*command.triangle;
            for (auto& coordinate : points) coordinate*=scale;
            if (command.image || command.text) { error="Native triangle cannot contain image or text data"; return Status::Failed; }
            if (command.triangleColors)
            {
                if (!prepared.gradientTriangle(points,clip,*command.triangleColors,error)) return Status::Failed;
            }
            else if (!prepared.triangle(points,clip,command.color,error)) return Status::Failed;
        }
        else if (command.image)
        {
            const auto source = command.streamingImage ? mStreams.at(command.owner).publication->current().source : command.image;
            const auto image = command.streamingImage ? mStreams.at(command.owner).publication->current().image : mImages.at({command.image.get(),paint.skinAnisotropy}).ready;
            if (command.streamingImage)
            {
                if (!prepared.browserImage(*source,image,{rect.left*scale,rect.bottom*scale,rect.right*scale,rect.top*scale},clip,command.color,error))
                    return Status::Failed;
                continue;
            }
            if (!prepared.image(*source,image,{rect.left,rect.bottom,rect.right,rect.top},{scale,scale,0,0},clip,
                command.color,error,command.alphaMask,command.additive ? LLVKContext::Blend2D::AddWithAlpha : LLVKContext::Blend2D::Alpha)) return Status::Failed;
        }
        else if (command.text)
        {
            if (command.text->glyphs.empty()) continue;
            const auto& text = mTexts.at(key);
            LLVKTextDraw::Style style;
            for (std::size_t channel = 0; channel < 4; ++channel)
                style.color[channel] = static_cast<std::uint8_t>(std::floor(std::clamp(command.color[channel],0.f,1.f)*255.f+0.5f));
            if (command.shadow) { style.shadow = LLVKTextDraw::Shadow::Soft; style.shadowStrength = 1.f; }
            if (!prepared.text(*text.atlas,text.pages,0,0,clip,style,error)) return Status::Failed;
        }
        else if (rect.right > rect.left && rect.top > rect.bottom &&
            !prepared.solid({rect.left*scale,rect.bottom*scale,rect.right*scale,rect.top*scale},clip,command.color,error)) return Status::Failed;
    }
    packet = std::move(prepared);
    return Status::Ready;
}
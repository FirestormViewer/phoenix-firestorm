#include "llvkwidgettree.h"
#include "llstring.h"

#include <algorithm>
#include <cmath>

namespace
{
    bool requiresRichText(std::string_view text)
    {
        for (const std::string_view marker : {"ARVD","BUG","CHOP","CHUIBUG","CTS","DOC","DN","ECC","EXP",
             "FIRE","FITMESH","LEAP","LLSD","MATBUG","MISC","OPEN","PATHBUG","PLAT","PYO","SCR","SH",
             "SINV","SLS","SNOW","SOCIAL","STORM","SUN","SUP","SVC","TPV","VWR","WEB"})
            if (text.find(marker) != text.npos) return true;
        if (text.size() < 3) return false;
        for (std::size_t offset = 0; offset < text.size(); ++offset)
        {
            const auto tail = text.substr(offset);
            if (tail.front() == '@') return true;
            if (tail.size() < 4) return false;
            if (tail.starts_with("://") || tail.starts_with("www.") || tail.starts_with(".com") ||
                tail.starts_with(".net") || tail.starts_with(".org") || tail.starts_with(".edu") ||
                (tail.size() > 7 && tail.starts_with("<nolink")) ||
                (tail.size() > 4 && tail.starts_with("<icon"))) return true;
        }
        return false;
    }
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createPlainText(const Params& view,
    const LLVKControl::Params& control, const LLVKPlainControl::Params& params, Id parent, std::string& error)
{
    error.clear();
    if (params.maximumBytes > 4 * 1024 * 1024)
    { error = "Native text byte limit exceeds supported budget"; return std::nullopt; }
    for (const auto& color : {params.textColor,params.readOnlyColor})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native text color"; return std::nullopt; }
    LLVKPlainControl text;
    text.params = params;
    text.readOnly = !view.enabled;
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,std::move(text));
}

bool LLVKWidgetTree::setPlainText(Id id, std::string text, std::string& error)
{
    error.clear();
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.plainText)
    { error = "Native text target is not a plain text control"; return false; }
    auto source = found->second.plainText->source;
    source.assign(std::move(text));
    return updatePlainText(id,std::move(source),mLabelContext,error);
}

bool LLVKWidgetTree::setPlainTextClicked(Id id, std::function<void(Id)> callback)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.plainText) return false;
    found->second.plainText->params.clicked = std::move(callback);
    return true;
}

bool LLVKWidgetTree::plainTextPointer(Id id, const PointerEvent& event, std::string& error)
{
    bool handled = basePointer(id,event,error);
    if (!error.empty()) return false;
    const auto* node = get(id);
    if (!node) return true;
    const bool down = event.kind == PointerKind::LeftDown;
    const bool up = event.kind == PointerKind::LeftUp;
    if ((down || up) && (node->params.soundFlags & (down ? 1 : 2)))
    {
        const auto events = mEvents.find(id);
        const auto sound = events == mEvents.end() ? std::function<void(Id,bool)>{} : events->second.sound;
        if (sound) sound(id,up);
        node = get(id);
        if (!node) return true;
    }
    if (down)
    {
        handled = handled || bool(node->plainText->params.clicked);
        if (handled && !mMouseCapture && !setMouseCapture(id,error)) return false;
    }
    else if (up && mMouseCapture == id)
    {
        if (!setMouseCapture(0,error)) return false;
        node = get(id);
        if (!node) return true;
        const auto clicked = node->plainText->params.clicked;
        if (!handled && clicked)
        {
            clicked(id);
            return true;
        }
    }
    else if (event.kind == PointerKind::Hover && !handled &&
             node->plainText->params.clicked && node->plainText->params.showHandCursor)
    {
        cursorEffect(id,true);
        return true;
    }
    return handled;
}

bool LLVKWidgetTree::setPlainTextArgument(Id id, std::string key, std::string replacement, std::string& error)
{
    error.clear();
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.plainText)
    { error = "Native argument target is not a plain text control"; return false; }
    auto source = found->second.plainText->source;
    source.setArgument(std::move(key),std::move(replacement));
    return updatePlainText(id,std::move(source),mLabelContext,error);
}

bool LLVKWidgetTree::updatePlainText(Id id, LLVKLabel source, const LLVKLabel::Context& context, std::string& error)
{
    auto& node = mNodes.at(id);
    auto resolved = resolvePlainText(*node.plainText,std::move(source),context,error);
    if (!resolved) return false;
    LLSD value(resolved->value);
    node.plainText = std::move(resolved);
    node.control->value = std::move(value);
    node.control->dirty = true;
    return true;
}

std::optional<LLVKPlainControl> LLVKWidgetTree::resolvePlainText(const LLVKPlainControl& state,
    LLVKLabel source, const LLVKLabel::Context& context, std::string& error) const
{
    auto resolved = source.resolve(context);
    std::erase(resolved,'\r');
    if (state.params.parseUrls && requiresRichText(resolved))
    { error = "Native text requires rich URL/issue/embedded-content processing, which is not implemented"; return std::nullopt; }
    resolved = utf8str_truncate(resolved,static_cast<std::int32_t>(state.params.maximumBytes));
    const auto wide = utf8str_to_wstring(resolved);
    std::u32string text(wide.begin(),wide.end());
    if (text.find(U'\0') != std::u32string::npos)
    { error = "Native plain text does not accept embedded NUL"; return std::nullopt; }
    auto output = state;
    output.source = std::move(source);
    output.text = std::move(text);
    output.value = std::move(resolved);
    output.layout.reset();
    output.cursor = output.params.trackEnd ? output.text.size() : 0;
    output.selectionStart = output.selectionEnd = 0;
    output.selecting = false;
    ++output.textGeneration;
    return output;
}

bool LLVKWidgetTree::reflowPlainText(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->plainText || !get(node->plainText->document))
    { error = "Native plain text document does not exist"; return false; }
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width < 0 || width > INT32_MAX || height < 0 || height > INT32_MAX)
    { error = "Native plain text rectangle outside supported range"; return false; }
    auto options = node->plainText->params.layout;
    options.width = static_cast<std::int32_t>(width);
    auto document = LLVKPlainTextLayout::document(node->plainText->text,*node->control->params.font,options,
        static_cast<std::int32_t>(height),node->plainText->params.verticalPadding,node->plainText->params.vertical,error);
    if (!document) return false;
    const auto rect = document->rectangle;
    const Id documentId = node->plainText->document;
    ShapeChanges changes;
    if (!planReshape(documentId,std::int64_t(rect.right)-rect.left,std::int64_t(rect.top)-rect.bottom,
                     {rect.left,rect.bottom,rect.right,rect.top},changes,error)) return false;
    if (!completeShapes(changes,error)) return false;
    if (!get(id)) { error = "Native text owner removed during document resize"; return false; }
    mNodes.at(id).plainText->layout = std::move(document);
    return true;
}

bool LLVKWidgetTree::fitPlainText(Id id, std::string& error)
{
    if (!reflowPlainText(id,error)) return false;
    const auto& layout = *get(id)->plainText->layout;
    return reshape(id,layout.fitWidth,layout.fitHeight,error);
}
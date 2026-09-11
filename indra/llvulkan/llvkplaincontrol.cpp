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
    const auto* initial = get(id);
    if (initial && initial->plainText && initial->plainText->params.selectable)
    {
        const bool down = event.kind == PointerKind::LeftDown;
        const bool up = event.kind == PointerKind::LeftUp;
        const bool captured = mMouseCapture == id;
        if (down || (captured && initial->plainText->selecting && (up || event.kind == PointerKind::Hover)))
        {
            const auto index = plainTextIndexAt(id,event.x,event.y,error);
            if (!index) return false;
            if (down && !requestControlFocus(id,true,error)) return false;
            if (!get(id)) return true;
            auto& text = *mNodes.at(id).plainText;
            if (down)
            {
                if (!(event.modifiers&1)) text.selectionStart = *index;
                else if (text.selectionStart == text.selectionEnd) text.selectionStart = text.cursor;
                text.selecting = true;
            }
            text.cursor = text.selectionEnd = *index;
            if (text.selectionStart != text.selectionEnd) text.pressedLink.reset();
            if (up)
            {
                text.selecting = false;
                if (!text.pressedLink) return setMouseCapture(0,error);
            }
            else if (!down) return true;
        }
    }
    initial = get(id);
    if (initial && initial->plainText && !initial->plainText->links.empty())
    {
        const auto link = plainTextLinkAt(id,event.x,event.y,error);
        if (!error.empty()) return false;
        const auto* current = get(id);
        if (!current) return true;
        const auto target = link ? std::optional(current->plainText->links[*link].target) : std::nullopt;
        if (event.kind == PointerKind::LeftDown && target)
        {
            mNodes.at(id).plainText->pressedLink = target;
            return setMouseCapture(id,error);
        }
        if (event.kind == PointerKind::LeftUp && mMouseCapture == id && current->plainText->pressedLink)
        {
            const auto pressed = current->plainText->pressedLink;
            const auto callback = current->plainText->params.linkClicked;
            mNodes.at(id).plainText->pressedLink.reset();
            if (!setMouseCapture(0,error)) return false;
            if (target == pressed && callback) callback(id,*target);
            return true;
        }
        if (event.kind == PointerKind::Hover && target)
        { cursorEffect(id,true); return true; }
    }
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
        handled = handled || bool(node->plainText->params.clicked) || node->plainText->params.selectable;
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

bool LLVKWidgetTree::selectAllPlainText(Id id)
{
    const auto* node = get(id);
    if (node && node->textEditor) return selectAllPlainText(node->textEditor->body);
    if (!node || !node->plainText || !node->plainText->params.selectable) return false;
    auto& text = *mNodes.at(id).plainText;
    text.selectionStart = text.text.size();
    text.selectionEnd = text.cursor = 0;
    text.selecting = false;
    text.pressedLink.reset();
    return true;
}

bool LLVKWidgetTree::deselectPlainText(Id id)
{
    const auto* node=get(id);
    if (node && node->textEditor) return deselectPlainText(node->textEditor->body);
    if (!node || !node->plainText) return false;
    auto& text=*mNodes.at(id).plainText;
    text.selectionStart=text.selectionEnd=0;
    text.selecting=false;
    return true;
}

bool LLVKWidgetTree::copyPlainText(Id id,std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (node && node->textEditor) return copyPlainText(node->textEditor->body,error);
    if (!node || !node->plainText || !node->plainText->params.selectable || !mClipboard) return false;
    const auto& text = *node->plainText;
    const auto begin = std::min(text.selectionStart,text.selectionEnd), end = std::max(text.selectionStart,text.selectionEnd);
    if (end > text.text.size() || begin == end) return false;
    const auto selection = text.text.substr(begin,end-begin);
    const auto clipboard = mClipboard;
    return clipboard->write(selection,false,error);
}

std::optional<std::size_t> LLVKWidgetTree::plainTextIndexAt(Id id,std::int32_t x,std::int32_t y,std::string& error)
{
    error.clear();
    if (!reflowPlainText(id,error)) return std::nullopt;
    const auto* node = get(id);
    if (!node || !node->plainText || !node->plainText->layout) return std::nullopt;
    const auto& text = *node->plainText;
    const auto* document = get(text.document);
    if (!document) return std::nullopt;
    for (const auto& line : text.layout->lines)
    {
        if (y < line.bottom+document->params.rect.bottom) continue;
        const auto begin = std::min(line.begin,text.text.size());
        auto end = std::min(line.end,text.text.size());
        if (end > begin && text.text[end-1] == U'\n') --end;
        const auto offset = node->control->params.font->hitTest(text.text,begin,
            float(std::max(0,x-line.left-document->params.rect.left)),float(std::max(0,line.right-line.left)),
            end-begin+1,text.params.layout.scaleX,true,text.params.layout.tabularNumbers,error);
        return offset ? std::optional(begin+*offset) : std::nullopt;
    }
    return text.text.size();
}

std::optional<std::size_t> LLVKWidgetTree::plainTextLinkAt(Id id,std::int32_t x,std::int32_t y,std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->plainText || x < 0 || y < 0 || x >= node->params.rect.right-node->params.rect.left ||
        y >= node->params.rect.top-node->params.rect.bottom) return std::nullopt;
    const auto screen = screenRect(id,error);
    if (!screen) return std::nullopt;
    const auto screenX = std::int64_t(screen->left)+x, screenY = std::int64_t(screen->bottom)+y;
    for (Id parent = node->parent; get(parent); )
    {
        const auto ancestor = get(parent)->parent;
        if (get(parent)->scrollContainer)
        {
            const auto scroll = prepareScrollContainer(parent,1.f,error);
            if (!scroll || screenX < scroll->documentClip.left || screenX >= scroll->documentClip.right ||
                screenY < scroll->documentClip.bottom || screenY >= scroll->documentClip.top) return std::nullopt;
        }
        parent = ancestor;
    }
    if (!reflowPlainText(id,error)) return std::nullopt;
    node = get(id);
    if (!node || !node->plainText || !node->plainText->layout) return std::nullopt;
    const auto& text = *node->plainText;
    const auto* document = get(text.document);
    if (!document) return std::nullopt;
    for (const auto& line : text.layout->lines)
    {
        const auto left = line.left+document->params.rect.left;
        if (y < line.bottom+document->params.rect.bottom || y >= line.top+document->params.rect.bottom ||
            x < left || x >= line.right+document->params.rect.left) continue;
        const auto count = std::min(line.end,text.text.size())-line.begin;
        const auto offset = node->control->params.font->hitTest(text.text,line.begin,float(x-left),float(line.right-line.left),
            count+1,text.params.layout.scaleX,false,text.params.layout.tabularNumbers,error);
        if (!offset) return std::nullopt;
        const auto character = line.begin+*offset;
        for (std::size_t index = 0; index < text.links.size(); ++index)
            if (character >= text.links[index].begin && character < text.links[index].end) return index;
        return std::nullopt;
    }
    return std::nullopt;
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
    if (state.params.parseUrls && !state.params.parseWebLinks && requiresRichText(resolved))
    { error = "Native text requires rich URL/issue/embedded-content processing, which is not implemented"; return std::nullopt; }
    std::vector<LLVKWebText::Link> links;
    if (state.params.parseWebLinks)
    {
        auto parsed = LLVKWebText::parse(resolved,error);
        if (!parsed) return std::nullopt;
        resolved = wstring_to_utf8str(LLWString(parsed->text.begin(),parsed->text.end()));
        links = std::move(parsed->links);
    }
    resolved = utf8str_truncate(resolved,static_cast<std::int32_t>(state.params.maximumBytes));
    const auto wide = utf8str_to_wstring(resolved);
    std::u32string text(wide.begin(),wide.end());
    if (text.find(U'\0') != std::u32string::npos)
    { error = "Native plain text does not accept embedded NUL"; return std::nullopt; }
    auto output = state;
    output.source = std::move(source);
    output.text = std::move(text);
    for (auto& link : links) link.end = std::min(link.end,output.text.size());
    std::erase_if(links,[](const auto& link) { return link.begin >= link.end; });
    output.links = std::move(links);
    output.pressedLink.reset();
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

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createTextEditor(const Params& input,const LLVKControl::Params& control,
    const LLVKPlainControl::Params& text,const ScrollContainerParams& scroll,bool borderVisible,Id parent,std::string& error)
{
    auto view=input;
    view.enabled=true;
    auto initial=control;
    initial.init={}; initial.initialValue.reset(); initial.valueSetting.reset();
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    mNodes.at(*id).textEditor.emplace();
    const bool readOnly=text.readOnly.value_or(!input.enabled);
    mNodes.at(*id).textEditor->readOnly=readOnly;
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        Params child;
        child.name="text scroller"; child.rect={0,0,view.rect.right-view.rect.left,view.rect.top-view.rect.bottom};
        child.follows=Left|Right|Top|Bottom;
        LLVKControl::Params childControl;
        childControl.font=control.font;
        auto scrollParams=scroll;
        scrollParams.borderVisible=false;
        scrollParams.minAutoRate=200; scrollParams.maxAutoRate=800;
        const auto scroller=createScrollContainer(child,childControl,scrollParams,*id,error);
        if (!scroller) { discard(); return std::nullopt; }
        mNodes.at(*id).textEditor->scroller=*scroller;
        child.name="text document"; child.follows=0;
        const auto document=createPanel(child,childControl,{},*scroller,error);
        if (!document || !attachScrollContent(*scroller,*document,0,error)) { discard(); return std::nullopt; }
        mNodes.at(*id).textEditor->document=*document;
        child.name="text contents";
        auto bodyParams=text;
        bodyParams.selectable=true; bodyParams.readOnly=readOnly;
        const auto body=createPlainText(child,childControl,bodyParams,*document,error);
        if (!body) { discard(); return std::nullopt; }
        mNodes.at(*id).textEditor->body=*body;
        child.name="text ed border"; child.mouseOpaque=false; child.visible=borderVisible;
        child.follows=Left|Right|Top|Bottom;
        auto borderParams=scroll.border; borderParams.bevel=LLVKBorder::Bevel::In; borderParams.thickness=1;
        const auto border=createBorder(child,borderParams,*id,error);
        if (!border) { discard(); return std::nullopt; }
        mNodes.at(*id).textEditor->border=*border;
        auto value=control.initialValue.value_or(LLSD(""));
        if (control.valueSetting && mSettings.contains(*control.valueSetting)) value=mSettings.at(*control.valueSetting);
        mNodes.at(*id).control->params=control;
        if (!setTextEditorText(*id,value.asString(),error)) { discard(); return std::nullopt; }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native text editor removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::setTextEditorText(Id id,const std::string& text,std::string& error)
{
    const auto* node=get(id);
    if (!node || !node->textEditor) return false;
    if (!setPlainText(node->textEditor->body,text,error)) return false;
    mNodes.at(id).control->value=value(node->textEditor->body);
    return layoutTextEditor(id,error);
}

bool LLVKWidgetTree::layoutTextEditor(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->textEditor) return false;
    const auto state=*node->textEditor;
    const auto* body=get(state.body);
    if (!body || !body->plainText || !get(state.scroller)) return false;
    const auto width=node->params.rect.right-node->params.rect.left, height=node->params.rect.top-node->params.rect.bottom;
    if (state.width==width && state.height==height && state.laidOutGeneration==body->plainText->textGeneration) return true;
    if (width<=0 || height<=0) { error="Native text editor has invalid extent"; return false; }
    const auto generation=body->plainText->textGeneration;
    const auto scroller=*get(state.scroller)->scrollContainer;
    const auto position=get(scroller.vertical)->scrollbar->position;
    auto options=body->plainText->params.layout;
    const auto font=body->control->params.font;
    const auto text=body->plainText->text;
    const auto padding=body->plainText->params.verticalPadding;
    const auto barSize=scroller.useSizeSetting ? setting("UIScrollbarSize").value_or(LLSD(0)).asInteger() : scroller.scrollbarSize;
    options.width=width;
    auto document=LLVKPlainTextLayout::document(text,*font,options,height,padding,LLVKFont::VerticalAlign::Top,error);
    if (!document) return false;
    if (document->fitHeight>height)
    {
        options.width=std::max(1,width-barSize);
        document=LLVKPlainTextLayout::document(text,*font,options,height,padding,LLVKFont::VerticalAlign::Top,error);
        if (!document) return false;
    }
    const auto contentHeight=std::max(height,document->fitHeight);
    const auto contentWidth=options.wrap ? options.width : std::max(options.width,document->fitWidth);
    if (!setShape(state.body,{0,0,contentWidth,contentHeight},error) || !reshape(state.document,contentWidth,contentHeight,error) ||
        !reshape(state.scroller,width,height,error)) return false;
    setScrollPosition(scroller.vertical,position,true,error);
    if (!error.empty() || !updateScrollContainer(state.scroller,error) || !get(id)) return false;
    auto& current=*mNodes.at(id).textEditor;
    current.width=width; current.height=height; current.laidOutGeneration=generation;
    return true;
}

bool LLVKWidgetTree::textEditorKey(Id id,ScrollKey key,LLVKLineEditor::Modifiers modifiers,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->textEditor || !node->textEditor->readOnly) return false;
    if (!layoutTextEditor(id,error)) return false;
    const auto scroller=get(id)->textEditor->scroller;
    if (scrollContainerKey(scroller,key,modifiers,error)) return true;
    if (!error.empty() || !get(id)) return false;
    const auto body=get(id)->textEditor->body;
    if (!get(body) || !get(body)->plainText) return false;
    auto& text=*mNodes.at(body).plainText;
    const auto length=text.text.size();
    const auto previous=std::min(text.cursor,length);
    auto position=previous;
    if (key==ScrollKey::Left || key==ScrollKey::Right)
    {
        if (!modifiers.shift && !modifiers.control) return false;
        if (key==ScrollKey::Left && position)
        {
            --position;
            if (modifiers.control)
            {
                while (position && text.text[position-1]==U' ') --position;
                while (position && LLWStringUtil::isPartOfWord(static_cast<llwchar>(text.text[position-1]))) --position;
            }
        }
        else if (key==ScrollKey::Right && position<length)
        {
            ++position;
            if (modifiers.control)
            {
                while (position<length && LLWStringUtil::isPartOfWord(static_cast<llwchar>(text.text[position]))) ++position;
                while (position<length && text.text[position]==U' ') ++position;
            }
        }
    }
    else if ((key==ScrollKey::Home || key==ScrollKey::End) && modifiers.control)
        position=key==ScrollKey::Home ? 0 : length;
    else return false;
    if (modifiers.shift)
    {
        if (text.selectionStart==text.selectionEnd) text.selectionStart=previous;
        text.selectionEnd=position;
    }
    else text.selectionStart=text.selectionEnd=0;
    text.cursor=position;
    text.selecting=false;
    text.pressedLink.reset();
    return true;
}

bool LLVKWidgetTree::startTextEditorDocument(Id id,std::string& error)
{
    if (!layoutTextEditor(id,error)) return false;
    const auto scroll=get(get(id)->textEditor->scroller)->scrollContainer;
    setScrollPosition(scroll->vertical,0,true,error);
    setScrollPosition(scroll->horizontal,0,true,error);
    return error.empty() && updateScrollContainer(get(id)->textEditor->scroller,error);
}
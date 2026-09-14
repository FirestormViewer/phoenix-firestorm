#include "llvklineeditor.h"
#include "llvkwidgettree.h"
#include "llsd.h"
#include "llstring.h"

#include <algorithm>
#include <cmath>
#include <cwctype>

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createLineEditor(const Params& view,
    const LLVKControl::Params& control, const LineEditorParams& input, Id parent, std::string& error)
{
    error.clear();
    auto params = input;
    const auto width = std::int64_t(view.rect.right)-view.rect.left;
    if (width < 0 || width > INT32_MAX) { error = "Native line editor width invalid"; return std::nullopt; }
    params.text.width = static_cast<std::int32_t>(width);
    for (const auto& color : {params.cursorColor,params.backgroundColor,params.textColor,params.readOnlyColor,
        params.tentativeColor,params.highlightColor,params.preeditColor})
        for (float channel : color) if (!std::isfinite(channel))
        { error = "Nonfinite native line editor color"; return std::nullopt; }
    std::optional<std::string> initial;
    if (control.initialValue && !control.valueSetting) initial = control.initialValue->asString();
    auto text = LLVKLineEditor::create(control.font,params.text,initial,mLabelContext,error);
    if (!text) return std::nullopt;
    LineEditor editor;
    editor.params = std::move(params);
    editor.text = std::move(*text);
    editor.label.assign(input.label);
    editor.readOnly = !view.enabled;
    return createControlImpl(view,control,std::nullopt,parent,error,std::nullopt,std::nullopt,std::nullopt,
        std::nullopt,std::nullopt,std::nullopt,std::move(editor));
}

bool LLVKWidgetTree::clearLineEditor(Id id, std::string& error)
{
    error.clear();
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.lineEditor)
    { error = "Native clear target is not a line editor"; return false; }
    if (!found->second.lineEditor->text.clear(error)) return false;
    found->second.control->value = LLSD("");
    found->second.control->dirty = found->second.lineEditor->text.dirty();
    return true;
}

bool LLVKWidgetTree::setLineEditorKeystroke(Id id,LLVKControl::Callback callback)
{
    if (!get(id) || !get(id)->lineEditor) return false;
    mNodes.at(id).lineEditor->params.keystroke=std::move(callback);
    return true;
}

bool LLVKWidgetTree::setLineEditorPassword(Id id, bool password)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.lineEditor) return false;
    found->second.lineEditor->params.text.password = password;
    found->second.lineEditor->text.setPassword(password);
    lineLanguageInput(id,false);
    return get(id) != nullptr;
}

bool LLVKWidgetTree::selectLineEditorAll(Id id, std::string& error)
{
    error.clear();
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.lineEditor)
    { error = "Native select-all target is not an editor"; return false; }
    return found->second.lineEditor->text.selectAll(error);
}

std::optional<LLVKWidgetTree::EditorDraw> LLVKWidgetTree::prepareLineEditor(Id id, const EditorView& view, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor || !std::isfinite(view.drawAlpha) || !std::isfinite(view.transparency) ||
        !std::isfinite(view.secondsSinceKeystroke) || view.secondsSinceKeystroke < 0 || view.focusWidth < 0)
    { error = "Invalid native line editor preparation input"; return std::nullopt; }
    const auto width64 = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height64 = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    if (width64 < 0 || height64 < 0 || width64 > INT32_MAX-view.focusWidth || height64 > INT32_MAX-view.focusWidth)
    { error = "Native editor preparation extent overflows"; return std::nullopt; }
    const auto width = static_cast<std::int32_t>(width64), height = static_cast<std::int32_t>(height64);
    const auto& editor = *node->lineEditor;
    const auto& params = editor.params;
    const auto& text = editor.text;
    const auto display = params.text.password ? std::u32string(text.display().size(),U'\u2022') : text.display();
    const bool focused = mKeyboardFocus == id;
    auto font = node->control->params.font;
    const auto pixelPosition = [&](std::size_t position) -> std::optional<std::int32_t>
    {
        if (!params.text.password) return text.pixelPosition(position,error);
        if (position < text.scroll() || position > display.size())
        { error = "Native password pixel position is outside the visible text"; return std::nullopt; }
        const auto measured = font->measureRun(display,text.scroll(),position-text.scroll(),1,true,false,error);
        if (!measured) return std::nullopt;
        const double pixel = std::floor(double(measured->width)+0.5)+text.leftEdge();
        if (pixel < INT32_MIN || pixel > INT32_MAX)
        { error = "Native password pixel position overflows"; return std::nullopt; }
        return static_cast<std::int32_t>(pixel);
    };
    EditorDraw output;
    auto backgroundColor = params.backgroundColor.get();
    backgroundColor[3] *= view.transparency;
    if (params.useBackgroundColor) output.parts.push_back({{0,0,width,height},backgroundColor});
    else
    {
        const auto image = editor.readOnly ? params.disabledBackground : focused || params.showFocusedBackground ? params.focusedBackground : params.background;
        if (image)
        {
            if (focused && params.drawFocusBorder)
            {
                auto color = view.focusColor;
                color[3] = view.transparency;
                for (auto& channel : color) channel=static_cast<unsigned>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
                output.parts.push_back({{-view.focusWidth,-view.focusWidth,width+view.focusWidth,height+view.focusWidth},color,image,true});
            }
            output.parts.push_back({{0,0,width,height},{1,1,1,view.transparency},image});
        }
    }
    auto color = editor.readOnly ? params.readOnlyColor.get() : node->control->tentative ? params.tentativeColor.get() : params.textColor.get();
    color[3] = view.drawAlpha;
    constexpr int border = 0;
    const auto lineHeight = static_cast<std::int32_t>(std::ceil(font->metrics().ascender)+std::ceil(font->metrics().descender));
    const auto verticalPad = (height-2*border-lineHeight)/2;
    const auto cursorBottom = border+2, cursorTop = height-border-1;
    const float textBottom = float(border+verticalPad);
    const auto settingInteger = [&](const char* name)
    { const auto found = mSettings.find(name); return found == mSettings.end() ? 0 : found->second.asInteger(); };
    const auto settingReal = [&](const char* name)
    { const auto found = mSettings.find(name); return found == mSettings.end() ? 0.f : float(found->second.asReal()); };
    if (text.hasPreedit())
    {
        const auto& preedit = text.preedit();
        for (std::size_t index = 0; index < preedit.standouts.size(); ++index)
        {
            if (preedit.positions[index+1] <= text.scroll()) continue;
            const auto left = pixelPosition(std::max(preedit.positions[index],text.scroll()));
            const auto right = pixelPosition(preedit.positions[index+1]);
            if (!left || !right) return std::nullopt;
            if (*left >= width-border) break;
            const bool standout = preedit.standouts[index];
            const auto gap = settingInteger(standout ? "UIPreeditStandoutGap" : "UIPreeditMarkerGap");
            const auto position = settingInteger(standout ? "UIPreeditStandoutPosition" : "UIPreeditMarkerPosition");
            const auto thickness = settingInteger(standout ? "UIPreeditStandoutThickness" : "UIPreeditMarkerThickness");
            const auto brightness = settingReal(standout ? "UIPreeditStandoutBrightness" : "UIPreeditMarkerBrightness");
            auto markerColor = color;
            for (std::size_t channel = 0; channel < 3; ++channel)
                markerColor[channel] = color[channel]*brightness+params.preeditColor[channel]*(1.f-brightness);
            output.parts.push_back({{*left+gap,border+position-thickness,std::min(*right,width-border)-gap-1,border+position},markerColor});
        }
    }
    const auto rounded = [](float value) { return static_cast<std::int32_t>(std::floor(value+0.5f)); };
    float rightPixel = float(text.leftEdge());
    std::size_t rendered = 0;
    const auto run = [&](std::u32string_view value,std::size_t begin,std::size_t count,LLVKColor::Value tint) -> bool
    {
        LLVKFont::LineOptions options;
        options.x = rightPixel;
        options.y = textBottom;
        options.maxPixels = std::max(0,text.rightEdge()-rounded(rightPixel));
        options.vertical = LLVKFont::VerticalAlign::Bottom;
        auto line = font->layoutLine(value,begin,count,options,error);
        if (!line) return false;
        rightPixel = line->rightX;
        rendered += line->sourceCharacters;
        output.parts.push_back({{},tint,{},false,std::move(line)});
        return true;
    };
    const auto selectionLeft = std::min(text.selectionStart(),text.selectionEnd());
    const auto selectionRight = std::max(text.selectionStart(),text.selectionEnd());
    const bool selection = selectionLeft != selectionRight;
    if (focused && selection)
    {
        if (selectionLeft > text.scroll() && !run(display,text.scroll(),selectionLeft-text.scroll(),color)) return std::nullopt;
        if (rightPixel < text.rightEdge() && text.scroll()+rendered < display.size())
        {
            const auto count = selectionRight > text.scroll()+rendered ? selectionRight-text.scroll()-rendered : 0;
            const auto measured = font->measureRun(display,text.scroll()+rendered,count,1,true,false,error);
            if (!measured) return std::nullopt;
            const auto selectedWidth = std::min(rounded(measured->width),text.rightEdge()-rounded(rightPixel));
            auto highlight = params.highlightColor.get();
            highlight[3] = view.drawAlpha;
            for (auto& channel : highlight) channel=static_cast<unsigned>(std::clamp(channel,0.f,1.f)*255.f)/255.f;
            output.parts.push_back({{rounded(rightPixel),cursorBottom,rounded(rightPixel)+selectedWidth,cursorTop},highlight});
            if (!run(display,text.scroll()+rendered,count,{1-color[0],1-color[1],1-color[2],view.drawAlpha})) return std::nullopt;
        }
        if (rightPixel < text.rightEdge() && text.scroll()+rendered < display.size() &&
            !run(display,text.scroll()+rendered,display.size()-text.scroll()-rendered,color)) return std::nullopt;
    }
    else if (!run(display,text.scroll(),display.size()-text.scroll(),color)) return std::nullopt;
    if (get(editor.border)) setVisible(editor.border,false);
    if (focused && !editor.readOnly && view.applicationFocused &&
        (view.secondsSinceKeystroke < 1.0 || std::fmod(std::floor(view.secondsSinceKeystroke*2),2.0) == 1.0))
    {
        const auto pixel = pixelPosition(text.cursor());
        if (!pixel) return std::nullopt;
        const auto thickness = settingInteger("UILineEditorCursorThickness");
        auto cursorRight = *pixel-thickness/2+thickness;
        if (mOverwrite && !selection)
        {
            const auto space = font->measureRun(U" ",0,1,1,true,false,error);
            const auto character = font->measureRun(display,text.cursor(),text.cursor() < display.size() ? 1 : 0,1,true,false,error);
            if (!space || !character) return std::nullopt;
            cursorRight = *pixel-thickness/2+std::max(rounded(space->width),rounded(character->width)+1);
        }
        output.parts.push_back({{*pixel-thickness/2,cursorBottom,cursorRight,cursorTop},color});
        if (mOverwrite && !selection && text.cursor() < display.size())
        {
            rightPixel = float(*pixel);
            if (!run(display,text.cursor(),1,{1-color[0],1-color[1],1-color[2],view.drawAlpha})) return std::nullopt;
        }
        output.caretVisible = true;
        output.caretX = *pixel;
        output.imeY = height-verticalPad;
    }
    if (display.empty() && (!focused || editor.readOnly || params.showFocusedLabel))
    {
        rightPixel = float(text.leftEdge());
        auto labelColor = params.tentativeColor.get();
        labelColor[3] = view.drawAlpha;
        const auto label = editor.label.resolveWide(mLabelContext);
        if (!run(label,0,label.size(),labelColor)) return std::nullopt;
    }
    return output;
}

void LLVKWidgetTree::lineLanguageInput(Id id, bool forceOff)
{
    const auto* node = get(id);
    const auto events = mEvents.find(id);
    if (!node || !node->lineEditor || events == mEvents.end()) return;
    const auto callback = events->second.languageInput;
    const bool allowed = !forceOff && hasAncestor(mKeyboardFocus,id) && !node->lineEditor->readOnly &&
        !node->lineEditor->params.text.password && !node->lineEditor->params.prevalidator;
    if (callback) callback(id,allowed);
}

bool LLVKWidgetTree::enableLineHistory(Id id, bool enabled)
{
    const auto found = mNodes.find(id);
    if (found == mNodes.end() || !found->second.lineEditor) return false;
    found->second.lineEditor->historyEnabled = enabled;
    return true;
}

void LLVKWidgetTree::updateLineHistory(Id id)
{
    auto& editor = *mNodes.at(id).lineEditor;
    const auto value = editor.text.text();
    if (editor.historyEnabled && !value.empty())
    {
        if (!editor.history.empty() && editor.history.back().empty()) editor.history.pop_back();
        if (editor.history.empty() || editor.history.back() != value) editor.history.push_back(value);
        editor.history.emplace_back();
        editor.historyPosition = editor.history.size()-1;
    }
}

bool LLVKWidgetTree::commitLineEditor(Id id)
{
    const auto* node = get(id);
    if (!node || !node->lineEditor) return false;
    const auto value = node->lineEditor->text.text();
    updateLineHistory(id);
    writeBoundValue(id,LLSD(value));
    if (!get(id)) return true;
    dispatchControl(id,&LLVKControl::Params::commit);
    if (!get(id)) return true;
    resetDirty(id);
    const auto& current = *get(id)->lineEditor;
    if (current.params.selectOnCommit)
    {
        const auto validator = current.params.inputPrevalidator;
        const auto text = current.text.display();
        const bool accepted = !validator || validator(text);
        if (accepted && get(id))
        {
            std::string error;
            return mNodes.at(id).lineEditor->text.selectAll(error);
        }
    }
    return true;
}

std::optional<LLVKLineEditor> LLVKLineEditor::create(std::shared_ptr<LLVKFont> font,
    const Params& params, const std::optional<std::string>& initial, const LLVKLabel::Context& context, std::string& error)
{
    error.clear();
    if (!font || params.width < 0 || !params.maximumBytes || params.maximumBytes > 4*1024*1024 ||
        params.maximumCharacters > 1024*1024 || !std::isfinite(params.scaleX) || params.scaleX <= 0.f)
    { error = "Invalid native line editor font, limits or dimensions"; return std::nullopt; }
    LLVKLineEditor editor;
    editor.mParams = params;
    editor.mFont = std::move(font);
    if (!editor.resize(params.width,error) || !editor.assign(params.defaultText,true,false,context,error) ||
        (initial && !editor.assign(*initial,false,false,context,error)) || !editor.setCursor(editor.mDisplay.size(),error)) return std::nullopt;
    return editor;
}

bool LLVKLineEditor::assign(const std::string& text, bool limit, bool focused,
    const LLVKLabel::Context& context, std::string& error)
{
    error.clear();
    if (text == mText) return true;
    if (text.size() > 4*1024*1024 || text.find('\0') != std::string::npos)
    { error = "Native line editor input exceeds byte budget or contains NUL"; return false; }
    auto next = *this;
    bool all = !mDisplay.empty() && ((mSelectionStart == 0 && mSelectionEnd == mDisplay.size()) ||
                                    (mSelectionStart == mDisplay.size() && mSelectionEnd == 0));
    all = all || (mDisplay.empty() && focused && mParams.selectOnFocus);
    auto truncated = text;
    if (!mParams.allowEmoji) utf8str_remove_emojis(truncated);
    if (limit && truncated.size() > mParams.maximumBytes)
        truncated = utf8str_truncate(text,static_cast<S32>(mParams.maximumBytes));
    if (limit && mParams.maximumCharacters)
    {
        auto characters = utf8str_to_wstring(truncated);
        if (characters.size() > mParams.maximumCharacters) characters.resize(mParams.maximumCharacters);
        truncated = wstring_to_utf8str(characters);
    }
    next.mSource.assign(std::move(truncated));
    next.mText = next.mSource.resolve(context);
    if (next.mText.size() > 4*1024*1024 || next.mText.find('\0') != std::string::npos)
    { error = "Native line editor formatted text exceeds budget or contains NUL"; return false; }
    next.mDisplay = next.mSource.resolveWide(context);
    if (all)
    {
        if (!next.selectAll(error)) return false;
    }
    else next.deselect();
    if (!next.setCursor(std::min(next.mDisplay.size(),next.mCursor),error)) return false;
    next.mPrevious = next.mText;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::setCursor(std::size_t position, std::string& error)
{
    error.clear();
    const auto cursor = std::min(position,mDisplay.size());
    auto scroll = std::min(mScroll,mDisplay.size());
    auto measured = mFont->measureRun(mDisplay,scroll,cursor > scroll ? cursor-scroll : 0,
        mParams.scaleX,true,mParams.tabularNumbers,error);
    if (!measured) return false;
    const auto pixels = std::floor(measured->width+0.5f)+mLeft;
    if (pixels > mRight)
    {
        auto prefix = mFont->measureRun(mDisplay,0,scroll,mParams.scaleX,true,mParams.tabularNumbers,error);
        if (!prefix) return false;
        const auto available = float(mRight)-mLeft;
        auto last = mFont->fitCharacters(mDisplay,std::max(0.f,available+std::floor(prefix->width+0.5f)),
            mDisplay.size(),mParams.scaleX,LLVKFont::Wrap::Anywhere,mParams.tabularNumbers,error);
        if (!last) return false;
        auto minimum = mFont->firstVisible(mDisplay,cursor ? cursor-1 : 0,std::max(0.f,available),mDisplay.size(),
            mParams.scaleX,mParams.tabularNumbers,error);
        if (!minimum) return false;
        scroll = mCursor == *last ? std::min(mDisplay.size(),std::max(*minimum,scroll)) : *minimum;
    }
    else if (cursor < scroll)
        scroll = mCursor == mScroll ? std::min(cursor,scroll > 4 ? scroll-4 : 0) : cursor;
    mCursor = cursor;
    mScroll = scroll;
    return true;
}

bool LLVKLineEditor::resize(std::int32_t width, std::string& error)
{
    error.clear();
    if (width < 0) { error = "Native line editor width cannot be negative"; return false; }
    auto next = *this;
    next.mParams.width = width;
    next.mLeft = std::clamp(mParams.leftPadding,0,width);
    next.mRight = width-std::clamp(mParams.rightPadding,0,width);
    if (!next.setCursor(next.mCursor,error)) return false;
    *this = std::move(next);
    return true;
}

void LLVKLineEditor::deselect()
{
    mSelectionStart = mSelectionEnd = 0;
    mSelecting = false;
}

bool LLVKLineEditor::selectAll(std::string& error)
{
    if (!setCursor(0,error)) return false;
    mSelectionStart = mDisplay.size();
    mSelectionEnd = 0;
    mSelecting = true;
    return true;
}

bool LLVKLineEditor::setSelection(std::size_t start, std::size_t end, std::string& error)
{
    auto next = *this;
    next.mSelecting = true;
    next.mSelectionStart = std::min(end,next.mDisplay.size());
    next.mSelectionEnd = std::min(start,next.mDisplay.size());
    if (!next.setCursor(start,error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::clear(std::string& error)
{
    auto next = *this;
    next.mSource.clear();
    next.mText.clear();
    next.mDisplay.clear();
    if (!next.setCursor(0,error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::recall(const std::string& text, const LLVKLabel::Context& context, std::string& error)
{
    error.clear();
    auto next = *this;
    next.mSource.assign(text);
    next.mText = next.mSource.resolve(context);
    if (next.mText.size() > 4*1024*1024 || next.mText.find('\0') != std::string::npos)
    { error = "Native history text exceeds budget or contains NUL"; return false; }
    next.mDisplay = next.mSource.resolveWide(context);
    if (!next.setCursor(next.mDisplay.size(),error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::revert(const LLVKLabel::Context& context, std::string& error)
{
    return assign(mPrevious,true,true,context,error);
}

bool LLVKLineEditor::replace(std::size_t begin, std::size_t count, std::u32string_view replacement,
    std::size_t cursor, std::string& error)
{
    error.clear();
    if (begin > mDisplay.size() || count > mDisplay.size()-begin)
    { error = "Native line editor edit range is invalid"; return false; }
    auto next = *this;
    next.mDisplay.replace(begin,count,replacement);
    const LLWString wide(next.mDisplay.begin(),next.mDisplay.end());
    next.mText = wstring_to_utf8str(wide);
    if (next.mText.size() > 4*1024*1024)
    { error = "Native line editor edited text exceeds byte budget"; return false; }
    if (!next.setCursor(cursor,error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::eraseSelection(std::string& error)
{
    const auto first = std::min(mSelectionStart,mSelectionEnd);
    const auto last = std::max(mSelectionStart,mSelectionEnd);
    if (first == last) { error.clear(); return true; }
    if (!replace(first,last-first,{},first,error)) return false;
    deselect();
    return true;
}

bool LLVKLineEditor::resetPreedit(bool removeSelection, std::string& error)
{
    error.clear();
    auto next = *this;
    if (next.mSelectionStart != next.mSelectionEnd)
    {
        if (next.hasPreedit()) next.deselect();
        else if (removeSelection && !next.eraseSelection(error)) return false;
    }
    if (next.hasPreedit())
    {
        const auto begin = next.mPreedit.positions.front();
        const auto end = next.mPreedit.positions.back();
        if (end < begin || end > next.mDisplay.size() || begin >= end)
        { error = "Native preedit range is no longer valid"; return false; }
        if (!next.replace(begin,end-begin,next.mPreedit.overwritten,begin,error)) return false;
        next.mPreedit.text.clear();
        next.mPreedit.overwritten.clear();
        next.mPreedit.positions.clear();
    }
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::updatePreedit(std::u32string_view text, const std::vector<std::size_t>& segments,
    const std::vector<bool>& standouts, std::size_t caret, bool overwrite, std::string& error)
{
    error.clear();
    if (hasPreedit() || text.size() > 1024*1024 || segments.size() > 4096 || segments.size() != standouts.size() ||
        caret > text.size() || (text.empty() != segments.empty()) ||
        std::any_of(text.begin(),text.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Invalid native preedit text, segments or reset ordering"; return false; }
    std::size_t total = 0;
    for (const auto length : segments)
    {
        if (!length || length > text.size()-total) { error = "Native preedit segments exceed text"; return false; }
        total += length;
    }
    if (total != text.size()) { error = "Native preedit segments do not cover text"; return false; }
    auto next = *this;
    const auto begin = next.mCursor;
    next.mPreedit.text = text;
    next.mPreedit.positions.clear();
    next.mPreedit.positions.push_back(begin);
    for (const auto length : segments) next.mPreedit.positions.push_back(next.mPreedit.positions.back()+length);
    next.mPreedit.standouts = standouts;
    const auto removed = overwrite ? std::min(text.size(),next.mDisplay.size()-begin) : 0;
    next.mPreedit.overwritten = next.mDisplay.substr(begin,removed);
    if (!next.replace(begin,removed,text,begin+text.size(),error) || !next.setCursor(begin+caret,error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::markPreedit(std::size_t position, std::size_t length, bool overwrite, std::string& error)
{
    error.clear();
    if (position > mDisplay.size() || length > mDisplay.size()-position)
    { error = "Native marked preedit range is invalid"; return false; }
    auto next = *this;
    next.deselect();
    if (!next.setCursor(position,error)) return false;
    next.mPreedit.text = next.mDisplay.substr(position,length);
    next.mPreedit.positions.clear();
    next.mPreedit.standouts.clear();
    if (length)
    {
        next.mPreedit.positions = {position,position+length};
        next.mPreedit.standouts = {false};
    }
    next.mPreedit.overwritten = overwrite ? next.mPreedit.text : std::u32string();
    *this = std::move(next);
    return true;
}

bool LLVKWidgetTree::resetLinePreedit(Id id, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native preedit target is not an editor"; return false; }
    bool remove = !node->lineEditor->readOnly;
    const auto snapshot = node->lineEditor->text;
    const auto input = node->lineEditor->params.inputPrevalidator;
    if (remove && !snapshot.hasPreedit() && input && snapshot.selectionStart() != snapshot.selectionEnd())
    {
        const auto first = std::min(snapshot.selectionStart(),snapshot.selectionEnd());
        remove = input(snapshot.display().substr(first,std::max(snapshot.selectionStart(),snapshot.selectionEnd())-first));
        if (!get(id)) return true;
    }
    auto text = get(id)->lineEditor->text;
    if (!text.resetPreedit(remove,error)) return false;
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    mNodes.at(id).control->dirty = dirty(id);
    return true;
}

bool LLVKWidgetTree::updateLinePreedit(Id id, std::u32string_view composition, const std::vector<std::size_t>& segments,
    const std::vector<bool>& standouts, std::size_t caret, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native preedit target is not an editor"; return false; }
    if (node->lineEditor->readOnly) return false;
    auto text = node->lineEditor->text;
    if (!text.updatePreedit(composition,segments,standouts,caret,mOverwrite,error)) return false;
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    mNodes.at(id).control->dirty = dirty(id);
    const auto callback = get(id)->lineEditor->params.keystroke;
    if (callback.function) callback.function(id,callback.parameter.value_or(value(id)));
    return true;
}

bool LLVKWidgetTree::markLinePreedit(Id id, std::size_t position, std::size_t length, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native preedit target is not an editor"; return false; }
    return mNodes.at(id).lineEditor->text.markPreedit(position,length,mOverwrite,error);
}

std::optional<std::int32_t> LLVKLineEditor::pixelPosition(std::size_t position, std::string& error) const
{
    error.clear();
    if (position < mScroll || position > mDisplay.size())
    { error = "Native text position is outside the visible measurement range"; return std::nullopt; }
    const auto measured = mFont->measureRun(mDisplay,mScroll,position-mScroll,mParams.scaleX,true,mParams.tabularNumbers,error);
    if (!measured) return std::nullopt;
    const double pixel = std::floor(double(measured->width)+0.5)+mLeft;
    if (pixel < INT32_MIN || pixel > INT32_MAX)
    { error = "Native text pixel position overflows"; return std::nullopt; }
    return static_cast<std::int32_t>(pixel);
}

std::optional<LLVKWidgetTree::PreeditLocation> LLVKWidgetTree::linePreeditLocation(Id id, std::int32_t queryOffset,
    float scaleX, float scaleY, std::string& error) const
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor || !std::isfinite(scaleX) || !std::isfinite(scaleY) || scaleX <= 0 || scaleY <= 0)
    { error = "Invalid native preedit geometry inputs"; return std::nullopt; }
    const auto screen = screenRect(id,error);
    if (!screen) return std::nullopt;
    const auto& text = node->lineEditor->text;
    const auto first = text.hasPreedit() ? text.preedit().positions.front() : text.cursor();
    const auto last = text.hasPreedit() ? text.preedit().positions.back() : text.cursor();
    const auto query = queryOffset < 0 ? text.cursor() : first+static_cast<std::size_t>(queryOffset);
    if (last < text.scroll() || query < text.scroll() || query < first || query > last)
    { error = "Native preedit query is outside its visible range"; return std::nullopt; }
    const auto queryPixel = text.pixelPosition(query,error);
    if (!queryPixel) return std::nullopt;
    const auto leftPixel = text.pixelPosition(std::max(first,text.scroll()),error);
    if (!leftPixel) return std::nullopt;
    const auto rightPixel = text.pixelPosition(last,error);
    if (!rightPixel) return std::nullopt;
    const auto width = std::int64_t(node->params.rect.right)-node->params.rect.left;
    const auto height = std::int64_t(node->params.rect.top)-node->params.rect.bottom;
    const auto right = std::max(std::int64_t(*leftPixel),std::min(std::int64_t(*rightPixel),width));
    const auto scaled = [&](std::int64_t coordinate, float scale, std::int32_t& output)
    {
        const double rounded = std::floor(double(float(coordinate)*scale)+0.5);
        if (!std::isfinite(rounded) || rounded < INT32_MIN || rounded > INT32_MAX) return false;
        output = static_cast<std::int32_t>(rounded);
        return true;
    };
    PreeditLocation result;
    result.position = first;
    result.length = last-first;
    const auto& rect = node->params.rect;
    if (!scaled(std::int64_t(screen->left)+*queryPixel,scaleX,result.x) ||
        !scaled(std::int64_t(screen->bottom)+height/2,scaleY,result.y) ||
        !scaled(std::int64_t(screen->left)+*leftPixel,scaleX,result.bounds.left) ||
        !scaled(std::int64_t(screen->left)+right,scaleX,result.bounds.right) ||
        !scaled(screen->bottom,scaleY,result.bounds.bottom) || !scaled(screen->top,scaleY,result.bounds.top) ||
        !scaled(std::int64_t(screen->left)+rect.left,scaleX,result.control.left) ||
        !scaled(std::int64_t(screen->bottom)+rect.bottom,scaleY,result.control.bottom) ||
        !scaled(std::int64_t(screen->left)+rect.right,scaleX,result.control.right) ||
        !scaled(std::int64_t(screen->bottom)+rect.top,scaleY,result.control.top))
    { error = "Native preedit scaled geometry overflows"; return std::nullopt; }
    const auto& metrics = node->control->params.font->metrics();
    const double logicalHeight = std::ceil(metrics.ascender/scaleY)+std::ceil(metrics.descender/scaleY);
    const double fontSize = std::floor(logicalHeight*scaleY+0.5);
    if (!std::isfinite(fontSize) || fontSize < 0 || fontSize > INT32_MAX)
    { error = "Native preedit font size overflows"; return std::nullopt; }
    result.fontSize = static_cast<std::int32_t>(fontSize);
    return result;
}

bool LLVKLineEditor::paste(std::u32string_view input, bool replaceSelection, unsigned& limitSignals, std::string& error)
{
    error.clear();
    limitSignals = 0;
    if (input.size() > 1024*1024 || std::any_of(input.begin(),input.end(),[](char32_t character)
        { return !character || character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff); }))
    { error = "Native paste requires bounded Unicode scalar text without NUL"; return false; }
    auto next = *this;
    if (replaceSelection && !next.eraseSelection(error)) return false;
    if (next.mText.size() > mParams.maximumBytes ||
        (mParams.maximumCharacters && next.mDisplay.size() > mParams.maximumCharacters))
    { error = "Native paste cannot compute capacity for existing over-limit text"; return false; }
    std::u32string clean(input);
    for (auto& character : clean)
    {
        if (character == U'\t') character = U' ';
        if (character == U'\n') character = mParams.replaceNewlinesWithSpaces ? U' ' : U'\u00b6';
    }
    const auto available = mParams.maximumBytes-next.mText.size();
    std::size_t bytes = 0, characters = 0;
    for (const auto character : clean)
    {
        const std::size_t width = character <= 0x7f ? 1 : character <= 0x7ff ? 2 : character <= 0xffff ? 3 : 4;
        if (width > available-bytes) break;
        bytes += width;
        ++characters;
    }
    if (characters < clean.size()) { clean.resize(characters); ++limitSignals; }
    if (mParams.maximumCharacters)
    {
        clean.resize(std::min(clean.size(),mParams.maximumCharacters-next.mDisplay.size()));
        ++limitSignals;
    }
    if (!next.replace(next.mCursor,0,clean,next.mCursor+clean.size(),error)) return false;
    next.deselect();
    *this = std::move(next);
    return true;
}

bool LLVKWidgetTree::pasteLineEditorText(Id id, std::u32string paste, bool primary, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native paste target is not a line editor"; return false; }
    if (node->lineEditor->readOnly) return false;
    if (paste.empty()) return true;
    if (paste.size() > 1024*1024) { error = "Native paste text exceeds scalar budget"; return false; }
    if (!node->lineEditor->params.text.allowEmoji)
        std::erase_if(paste,[](char32_t character) { return character >= 0x1f000 && character < 0x20000; });
    const auto input = node->lineEditor->params.inputPrevalidator;
    const bool accepted = !input || input(paste);
    if (!get(id)) return true;
    if (!accepted) return true;
    const auto rollback = get(id)->lineEditor->text;
    bool replaceSelection = !primary && rollback.selectionStart() != rollback.selectionEnd();
    if (replaceSelection && input)
    {
        const auto begin = std::min(rollback.selectionStart(),rollback.selectionEnd());
        const auto count = std::max(rollback.selectionStart(),rollback.selectionEnd())-begin;
        replaceSelection = input(rollback.display().substr(begin,count));
        if (!get(id)) return true;
    }
    auto text = get(id)->lineEditor->text;
    unsigned limitSignals = 0;
    if (!text.paste(paste,replaceSelection,limitSignals,error)) return false;
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    for (unsigned signal = 0; signal < limitSignals; ++signal)
    {
        notify(id,&Events::badKeystroke);
        if (!get(id)) return true;
    }
    return finishLineEdit(id,rollback,false);
}

bool LLVKLineEditor::erase(std::size_t begin, std::size_t count, std::string& error)
{
    return replace(begin,count,{},begin,error);
}

void LLVKLineEditor::startSelection() noexcept
{
    mSelectionStart = mSelectionEnd = mCursor;
    mSelecting = true;
}

bool LLVKLineEditor::extendSelection(std::size_t position, std::string& error)
{
    auto next = *this;
    if (!next.mSelecting) next.startSelection();
    if (!next.setCursor(position,error)) return false;
    next.mSelectionEnd = next.mCursor;
    *this = std::move(next);
    return true;
}

std::size_t LLVKLineEditor::wordPosition(std::size_t position, bool forward) const
{
    position = std::min(position,mDisplay.size());
    const auto word = [](char32_t character)
    { return character == U'_' || (character <= WCHAR_MAX && std::iswalnum(static_cast<wint_t>(character)) != 0); };
    if (forward)
    {
        while (position < mDisplay.size() && word(mDisplay[position])) ++position;
        while (position < mDisplay.size() && mDisplay[position] == U' ') ++position;
    }
    else
    {
        while (position && mDisplay[position-1] == U' ') --position;
        while (position && word(mDisplay[position-1])) --position;
    }
    return position;
}

bool LLVKWidgetTree::lineEditorKey(Id id, LLVKLineEditor::Key key, LLVKLineEditor::Modifiers modifiers, std::string& error)
{
    error.clear();
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native key target is not a line editor"; return false; }
    if (mKeyboardFocus != id) return false;
    mNodes.at(id).lineEditor->lastKey = key;
    using Key = LLVKLineEditor::Key;
    const bool navigation = key == Key::Left || key == Key::Right || key == Key::Home || key == Key::End ||
        key == Key::PageUp || key == Key::PageDown;
    const bool selecting = modifiers.shift && navigation;
    const bool readOnly = node->lineEditor->readOnly;
    if (!selecting && readOnly) return false;
    if (!selecting && navigation)
    {
        if ((key == Key::Left || key == Key::Right) && modifiers.alt) return false;
        if (node->lineEditor->params.ignoreArrowKeys &&
            ((key != Key::Left && key != Key::Right) || (!modifiers.control && !modifiers.alt && !modifiers.shift))) return false;
    }
    const bool controlOnly = modifiers.control && !modifiers.shift && !modifiers.alt;
    if (key == Key::Delete && !controlOnly) return false;
    if (key == Key::Return)
    {
        updateLineHistory(id);
        return false;
    }
    if (key == Key::Escape)
    {
        if (node->lineEditor->params.revertOnEscape && node->lineEditor->text.dirty())
        {
            auto text = node->lineEditor->text;
            if (!text.revert(mLabelContext,error)) return false;
            auto& editor = *mNodes.at(id).lineEditor;
            editor.text = std::move(text);
            mNodes.at(id).control->value = LLSD(editor.text.text());
            mNodes.at(id).control->dirty = editor.text.dirty();
            const auto callback = editor.params.keystroke;
            if (editor.params.keystrokeOnEscape && callback.function)
                callback.function(id,callback.parameter.value_or(value(id)));
        }
        return false;
    }
    const bool historyKey = key == Key::Up || key == Key::Down;
    if (historyKey && (!node->lineEditor->historyEnabled || (node->lineEditor->params.ignoreArrowKeys && !controlOnly))) return false;
    const auto rollback = node->lineEditor->text;
    auto text = rollback;
    const auto input = node->lineEditor->params.inputPrevalidator;
    const auto length = text.display().size();
    const auto cursor = text.cursor();
    const bool hasSelection = text.selectionStart() != text.selectionEnd();
    bool bad = false;
    if (key == Key::Insert)
    {
        if (!modifiers.shift && !modifiers.control && !modifiers.alt) mOverwrite = !mOverwrite;
    }
    else if (historyKey)
    {
        auto& editor = *mNodes.at(id).lineEditor;
        auto history = editor.history;
        auto position = editor.historyPosition;
        if (key == Key::Up && position > 0 && position < history.size())
        {
            history[position] = text.text();
            --position;
        }
        else if (key == Key::Down && !history.empty() && position < history.size()-1) ++position;
        else bad = true;
        if (!bad)
        {
            if (!text.recall(history[position],mLabelContext,error)) return false;
            editor.history = std::move(history);
            editor.historyPosition = position;
        }
    }
    else if (navigation)
    {
        auto position = cursor;
        if (key == Key::Home || key == Key::PageUp) position = 0;
        else if (key == Key::End || key == Key::PageDown) position = length;
        else if (key == Key::Left)
        {
            if (!selecting && hasSelection) position = std::min({cursor ? cursor-1 : 0,text.selectionStart(),text.selectionEnd()});
            else if (cursor) position = modifiers.control ? text.wordPosition(cursor-1,false) : cursor-1;
            else bad = true;
        }
        else
        {
            if (!selecting && hasSelection) position = std::min(length,std::max({cursor+1,text.selectionStart(),text.selectionEnd()}));
            else if (cursor < length) position = modifiers.control ? text.wordPosition(cursor+1,true) : cursor+1;
            else bad = true;
        }
        const bool emptyEnd = !length && (key == Key::End || key == Key::PageDown);
        if (!bad && !emptyEnd)
        {
            if (selecting)
            {
                if (!text.selecting()) text.startSelection();
                const auto first = std::min(position,text.selectionStart());
                const auto count = std::max(position,text.selectionStart())-first;
                const bool accepted = !input || input(text.display().substr(first,count));
                if (!get(id)) return true;
                if (accepted && !text.extendSelection(position,error)) return false;
            }
            else if (!text.setCursor(position,error)) return false;
        }
    }
    else
    {
        auto begin = cursor;
        std::size_t count = 0;
        bool validateRemoval = true;
        if (key == Key::Backspace && hasSelection)
        {
            begin = std::min(text.selectionStart(),text.selectionEnd());
            count = std::max(text.selectionStart(),text.selectionEnd())-begin;
        }
        else if (controlOnly)
        {
            const bool forward = key == Key::Delete;
            bad = forward ? cursor == length : cursor == 0;
            if (!bad)
            {
                auto position = text.wordPosition(cursor,forward);
                if (position == cursor) position = text.wordPosition(forward ? cursor+1 : cursor-1,forward);
                begin = std::min(cursor,position);
                count = std::max(cursor,position)-begin;
                validateRemoval = false;
            }
        }
        else if (cursor) { begin = cursor-1; count = 1; }
        else bad = true;
        if (count)
        {
            const bool accepted = !validateRemoval || !input || input(text.display().substr(begin,count));
            if (!get(id)) return true;
            if (accepted && !text.erase(begin,count,error)) return false;
        }
    }
    if (!selecting) text.deselect();
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    if (bad) notify(id,&Events::badKeystroke);
    if (!get(id)) return true;
    return finishLineEdit(id,rollback,readOnly && get(id)->lineEditor->text.text() == rollback.text());
}

bool LLVKWidgetTree::initializeInventoryDropTarget(Id id,std::string& error)
{
    error.clear();
    if (!get(id) || (!get(id)->lineEditor && !get(id)->plainText)) { error="Native inventory target requires a text control"; return false; }
    mNodes.at(id).inventoryDropTarget=Node::InventoryDropTarget{};
    return !get(id)->lineEditor || setEnabled(id,false);
}

bool LLVKWidgetTree::setInventoryDropHandler(Id id,std::function<void(Id,const Node::InventoryDropTarget::Item&)> handler)
{
    if (!get(id) || !get(id)->inventoryDropTarget) return false;
    mNodes.at(id).inventoryDropTarget->dropped=std::move(handler);
    return true;
}

bool LLVKWidgetTree::inventoryDrop(Id id,const Node::InventoryDropTarget::Item& item,bool drop)
{
    const auto* node=get(id);
    if (!node || !node->inventoryDropTarget || item.kind==Node::InventoryDropTarget::Item::Kind::Other ||
        item.link || item.folder || !item.copy || !item.transfer || item.id.empty()) return false;
    const auto callback=node->inventoryDropTarget->dropped;
    if (drop && callback) callback(id,item);
    return true;
}

std::optional<LLVKWidgetTree::Id> LLVKWidgetTree::createSearchEditor(const Params& view,
    const LLVKControl::Params& control,const SearchEditorParams& params,Id parent,std::string& error)
{
    error.clear();
    const auto width=view.rect.right-view.rect.left, height=view.rect.top-view.rect.bottom;
    if (width<=0 || height<=0 || params.searchWidth<0 || params.searchHeight<0 || params.clearWidth<0 || params.clearHeight<0)
    { error="Invalid native search editor geometry"; return std::nullopt; }
    auto initial=control;
    initial.init={}; initial.initialValue.reset(); initial.valueSetting.reset();
    const auto id=createControl(view,initial,parent,error);
    if (!id) return std::nullopt;
    const auto discard=[&] { std::string ignored; if (get(*id)) erase(*id,ignored); };
    try
    {
        mNodes.at(*id).searchEditor=SearchEditor{0,0,0,std::make_shared<SearchEditorParams>(params)};
        Params child;
        child.name="filter edit box"; child.rect={0,0,width,height}; child.follows=Left|Right|Top|Bottom;
        auto editor=params.editor;
        editor.revertOnEscape=false; editor.passDelete=true;
        if (params.commitOnKeystroke) editor.commitOnFocusLost=false;
        if (params.searchVisible) editor.text.leftPadding+=params.searchWidth;
        if (params.clearVisible) editor.text.rightPadding=params.clearWidth+params.clearRight+params.clearLeft;
        editor.keystroke.function=[this,owner=*id](Id child,const LLSD&)
        {
            const auto* node=get(owner);
            if (!node || !node->searchEditor || !get(child)) return;
            const auto callbacks=node->searchEditor->params;
            const auto key=get(child)->lineEditor->lastKey;
            if (callbacks->keystroke.function)
                callbacks->keystroke.function(owner,callbacks->keystroke.parameter.value_or(value(owner)));
            if (!get(owner)) return;
            if (key!=LLVKLineEditor::Key::Left && key!=LLVKLineEditor::Key::Right && callbacks->textChanged.function)
                callbacks->textChanged.function(owner,callbacks->textChanged.parameter.value_or(value(owner)));
            if (get(owner) && callbacks->commitOnKeystroke) commit(child);
        };
        auto editorControl=control;
        editorControl.init={}; editorControl.valueSetting.reset();
        editorControl.commit.function=[this,owner=*id](Id,const LLSD&)
        {
            if (!get(owner)) return;
            writeBoundValue(owner,value(owner));
            if (get(owner)) dispatchControl(owner,&LLVKControl::Params::commit);
        };
        if (control.valueSetting && mSettings.contains(*control.valueSetting)) editorControl.initialValue=mSettings.at(*control.valueSetting);
        const auto body=createLineEditor(child,editorControl,editor,*id,error);
        if (!body) { discard(); return std::nullopt; }
        mNodes.at(*id).searchEditor->editor=*body;
        auto buttonControl=params.buttonControl;
        if (!buttonControl.font) buttonControl.font=control.font;
        buttonControl.tabStop=false;
        for (const bool search : {true,false})
        {
            if (!(search ? params.searchVisible : params.clearVisible)) continue;
            auto button=search ? params.searchButton : params.clearButton;
            child.name=search ? "search button" : "clear button";
            child.follows=(search ? Left : Right)|Top;
            child.rect=search ? Rect{params.searchLeft,params.searchBottom,params.searchLeft+params.searchWidth,params.searchBottom+params.searchHeight} :
                Rect{width-params.clearRight-params.clearWidth,params.clearBottom,width-params.clearRight,params.clearBottom+params.clearHeight};
            buttonControl.commit.function=[this,owner=*id,search](Id,const LLSD&)
            { if (search) commit(owner); else { std::string problem; clearSearchEditor(owner,problem); } };
            const auto buttonId=createButton(child,buttonControl,button,*body,error);
            if (!buttonId) { discard(); return std::nullopt; }
            (search ? mNodes.at(*id).searchEditor->search : mNodes.at(*id).searchEditor->clear)=*buttonId;
        }
        mNodes.at(*id).control->params=control;
        if (!refreshSearchEditor(*id,error)) { discard(); return std::nullopt; }
        if (control.init.function) control.init.function(*id,control.init.parameter.value_or(LLSD()));
        if (!get(*id)) { error="Native search editor removed during initialization"; return std::nullopt; }
        return id;
    }
    catch (...) { discard(); throw; }
}

bool LLVKWidgetTree::refreshSearchEditor(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->searchEditor || !get(node->searchEditor->editor)) return false;
    const auto state=*node->searchEditor;
    const auto nonempty=!value(id).asString().empty();
    if (state.clear) setVisible(state.clear,nonempty);
    if (!get(id) || !get(state.editor)) return false;
    auto& editor=mNodes.at(state.editor).lineEditor->params;
    if (state.params->highlight)
    {
        editor.background=nonempty ? state.params->highlightBackground : state.params->editor.background;
        editor.focusedBackground=nonempty ? state.params->highlightBackground : state.params->editor.focusedBackground;
    }
    return true;
}

bool LLVKWidgetTree::clearSearchEditor(Id id,std::string& error)
{
    error.clear();
    const auto* node=get(id);
    if (!node || !node->searchEditor) return false;
    const auto state=*node->searchEditor;
    if (!setValue(state.editor,LLSD(""))) return false;
    const auto callback=state.params->textChanged;
    if (callback.function) callback.function(id,callback.parameter.value_or(value(id)));
    return !get(id) || commit(state.editor);
}

bool LLVKWidgetTree::finishLineEdit(Id id, const LLVKLineEditor& rollback, bool forceRollback)
{
    const auto validator = get(id)->lineEditor->params.prevalidator;
    const auto proposed = get(id)->lineEditor->text.display();
    const bool accepted = !forceRollback && (!validator || validator(proposed));
    if (!get(id)) return true;
    if (!accepted)
    {
        mNodes.at(id).lineEditor->text = rollback;
        mNodes.at(id).lineEditor->text.resetDirty();
        mNodes.at(id).control->value = LLSD(rollback.text());
        mNodes.at(id).control->dirty = false;
        notify(id,&Events::badKeystroke);
        return true;
    }
    mNodes.at(id).control->dirty = dirty(id);
    const auto callback = get(id)->lineEditor->params.keystroke;
    const LLSD argument = callback.parameter.value_or(value(id));
    if (callback.function) callback.function(id,argument);
    return true;
}

bool LLVKWidgetTree::canLineEditorDelete(Id id) const
{
    const auto* node = get(id);
    if (!node || !node->lineEditor || node->lineEditor->readOnly) return false;
    const auto& editor = *node->lineEditor;
    return !editor.params.passDelete || editor.text.selectionStart() != editor.text.selectionEnd() ||
        editor.text.cursor() < editor.text.display().size();
}

bool LLVKWidgetTree::canLineEditorCopy(Id id) const
{
    const auto* node = get(id);
    return node && node->lineEditor && !node->lineEditor->params.text.password &&
        node->lineEditor->text.selectionStart() != node->lineEditor->text.selectionEnd();
}

bool LLVKWidgetTree::canLineEditorCut(Id id) const
{
    return canLineEditorCopy(id) && !get(id)->lineEditor->readOnly;
}

bool LLVKWidgetTree::canLineEditorPaste(Id id, bool primary) const
{
    const auto* node = get(id);
    const auto clipboard = mClipboard;
    return node && node->lineEditor && !node->lineEditor->readOnly && clipboard && clipboard->available(primary);
}

bool LLVKWidgetTree::copyLineEditor(Id id, bool primary, std::string& error)
{
    error.clear();
    if (!canLineEditorCopy(id)) return false;
    const auto clipboard = mClipboard;
    if (!clipboard) { error = "Native clipboard owner is unavailable"; return false; }
    const auto& text = get(id)->lineEditor->text;
    const auto begin = std::min(text.selectionStart(),text.selectionEnd());
    const auto selection = text.display().substr(begin,std::max(text.selectionStart(),text.selectionEnd())-begin);
    return clipboard->write(selection,primary,error);
}

bool LLVKWidgetTree::cutLineEditor(Id id, std::string& error)
{
    error.clear();
    if (!canLineEditorCut(id)) return false;
    const auto clipboard = mClipboard;
    if (!clipboard) { error = "Native clipboard owner is unavailable"; return false; }
    const auto input = get(id)->lineEditor->params.inputPrevalidator;
    const auto& text = get(id)->lineEditor->text;
    const auto begin = std::min(text.selectionStart(),text.selectionEnd());
    const auto selection = text.display().substr(begin,std::max(text.selectionStart(),text.selectionEnd())-begin);
    const bool accepted = !input || input(selection);
    if (!get(id)) return true;
    if (!accepted) return true;
    const auto rollback = get(id)->lineEditor->text;
    std::string clipboardError;
    const bool copied = clipboard->write(selection,false,clipboardError);
    if (!get(id)) { error = std::move(clipboardError); return copied; }
    const bool remove = !input || input(selection);
    if (!get(id)) { error = std::move(clipboardError); return copied; }
    auto next = get(id)->lineEditor->text;
    if (remove && !next.eraseSelection(error)) return false;
    mNodes.at(id).lineEditor->text = std::move(next);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    finishLineEdit(id,rollback,false);
    error = std::move(clipboardError);
    return copied;
}

bool LLVKWidgetTree::pasteLineEditor(Id id, bool primary, std::string& error)
{
    error.clear();
    const auto clipboard = mClipboard;
    const auto* node = get(id);
    if (!node || !node->lineEditor || node->lineEditor->readOnly) return false;
    if (!clipboard) { error = "Native clipboard owner is unavailable"; return false; }
    if (!clipboard->available(primary)) return false;
    const auto text = clipboard->read(primary,error);
    if (!text) return false;
    if (!get(id)) return true;
    return pasteLineEditorText(id,*text,primary,error);
}

bool LLVKWidgetTree::deleteLineEditor(Id id, std::string& error)
{
    error.clear();
    if (!canLineEditorDelete(id)) return false;
    mNodes.at(id).lineEditor->lastKey = LLVKLineEditor::Key::Delete;
    const auto rollback = get(id)->lineEditor->text;
    if (rollback.display().empty()) return true;
    auto text = rollback;
    const auto input = get(id)->lineEditor->params.inputPrevalidator;
    const bool selected = text.selectionStart() != text.selectionEnd();
    const auto begin = selected ? std::min(text.selectionStart(),text.selectionEnd()) : text.cursor();
    const auto count = selected ? std::max(text.selectionStart(),text.selectionEnd())-begin :
        (begin < text.display().size() ? 1u : 0u);
    if (count)
    {
        const auto removed = text.display().substr(begin,count);
        const bool accepted = !input || input(removed);
        if (!get(id)) return true;
        if (!accepted && !selected)
        {
            const auto callback = get(id)->lineEditor->params.keystroke;
            if (callback.function) callback.function(id,callback.parameter.value_or(value(id)));
            return true;
        }
        if (accepted)
        {
            if (selected)
            {
                if (!text.eraseSelection(error)) return false;
            }
            else
            {
                if (!text.setCursor(begin+1,error)) return false;
                const bool remove = !input || input(removed);
                if (!get(id)) return true;
                if (remove && !text.erase(begin,1,error)) return false;
            }
        }
    }
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = LLSD(mNodes.at(id).lineEditor->text.text());
    return finishLineEdit(id,rollback,false);
}

bool LLVKLineEditor::insert(char32_t character, bool overwrite, bool allowRemoval, bool& limited, std::string& error)
{
    error.clear();
    limited = false;
    if (character > 0x10ffff || (character >= 0xd800 && character <= 0xdfff))
    { error = "Native line editor requires Unicode scalar input"; return false; }
    if (!mParams.allowEmoji && character >= 0x1f000 && character < 0x20000) return true;
    auto next = *this;
    if (mSelectionStart != mSelectionEnd)
    {
        if (allowRemoval && !next.eraseSelection(error)) return false;
    }
    else if (overwrite)
    {
        if (!allowRemoval) return true;
        if (next.mCursor < next.mDisplay.size() && !next.replace(next.mCursor,1,{},next.mCursor,error)) return false;
    }
    const LLWString wide(1,static_cast<llwchar>(character));
    const auto bytes = wstring_to_utf8str(wide).size();
    limited = bytes+next.mText.size() > mParams.maximumBytes ||
        (mParams.maximumCharacters && next.mDisplay.size()+1 > mParams.maximumCharacters);
    if (!limited && !next.replace(next.mCursor,0,std::u32string_view(&character,1),next.mCursor+1,error)) return false;
    *this = std::move(next);
    return true;
}

bool LLVKWidgetTree::lineEditorUnicode(Id id, char32_t character, std::string& error)
{
    return lineEditorUnicode(id,character,mOverwrite,error);
}

bool LLVKWidgetTree::lineEditorUnicode(Id id, char32_t character, bool overwrite, std::string& error)
{
    error.clear();
    if (character < 0x20 || character == 0x7f) return false;
    const auto* node = get(id);
    if (!node || !node->lineEditor) { error = "Native Unicode target is not a line editor"; return false; }
    if (mKeyboardFocus != id || !node->params.visible || node->lineEditor->readOnly) return false;
    mNodes.at(id).lineEditor->lastKey.reset();
    const auto rollback = node->lineEditor->text;
    const auto input = node->lineEditor->params.inputPrevalidator;
    if (input && !input(std::u32string_view(&character,1))) return true;
    node = get(id);
    if (!node) return true;
    const auto& before = node->lineEditor->text;
    std::u32string removed;
    if (before.selectionStart() != before.selectionEnd())
    {
        const auto first = std::min(before.selectionStart(),before.selectionEnd());
        const auto last = std::max(before.selectionStart(),before.selectionEnd());
        if (last <= before.display().size()) removed = before.display().substr(first,last-first);
    }
    else if (overwrite && before.cursor() < before.display().size()) removed = before.display().substr(before.cursor(),1);
    const bool allowRemoval = !input || (removed.empty() && !overwrite) || input(removed);
    node = get(id);
    if (!node) return true;
    auto text = node->lineEditor->text;
    bool limited = false;
    if (!text.insert(character,overwrite,allowRemoval,limited,error)) return false;
    LLSD stored(text.text());
    mNodes.at(id).lineEditor->text = std::move(text);
    mNodes.at(id).control->value = std::move(stored);
    if (limited) notify(id,&Events::badKeystroke);
    if (!get(id)) return true;
    notify(id,&Events::hideCursor);
    if (!get(id)) return true;
    mNodes.at(id).lineEditor->text.deselect();
    return finishLineEdit(id,rollback,false);
}

std::optional<std::size_t> LLVKLineEditor::hitTest(std::int32_t localX, std::string& error) const
{
    const auto text = mParams.password ? std::u32string(mDisplay.size(),U'\u2022') : mDisplay;
    const auto position = mFont->hitTest(text,mScroll,float(localX)-mLeft,std::max(0.f,float(mRight)-mLeft+1.f),
        text.size()-mScroll+1,mParams.scaleX,true,mParams.tabularNumbers,error);
    return position ? std::optional(mScroll+*position) : std::nullopt;
}

bool LLVKLineEditor::point(std::int32_t localX, bool shift,
    const std::function<bool(std::u32string_view)>& validator, std::string& error)
{
    auto next = *this;
    const auto oldCursor = next.mCursor;
    const bool selected = next.mSelectionStart != next.mSelectionEnd;
    if (shift) next.mSelecting = true;
    const auto position = next.hitTest(localX,error);
    if (!position) return false;
    const auto first = std::min(next.mSelectionStart,*position);
    const auto count = std::max(next.mSelectionStart,*position)-first;
    if (!next.mSelecting || !validator || validator(next.mDisplay.substr(first,count)))
        if (!next.setCursor(*position,error)) return false;
    if (shift && !selected) next.mSelectionStart = oldCursor;
    if (next.mSelecting) next.mSelectionEnd = next.mCursor;
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::selectWord(std::size_t oldStart, std::size_t oldEnd,
    const std::function<bool(std::u32string_view)>& validator, std::string& error)
{
    auto next = *this;
    const auto word = [](char32_t character)
    { return character == U'_' || (character <= WCHAR_MAX && std::iswalnum(static_cast<wint_t>(character)) != 0); };
    bool all = true;
    if (next.mCursor < next.mDisplay.size() && word(next.mDisplay[next.mCursor]))
    {
        while (next.mCursor && word(next.mDisplay[next.mCursor-1])) --next.mCursor;
        next.startSelection();
        while (next.mCursor < next.mDisplay.size() && word(next.mDisplay[next.mCursor])) ++next.mCursor;
        next.mSelectionEnd = next.mCursor;
        all = oldStart == next.mSelectionStart && oldEnd == next.mSelectionEnd;
    }
    if (all && (!validator || validator(next.mDisplay)) && !next.selectAll(error)) return false;
    next.finishSelection();
    *this = std::move(next);
    return true;
}

bool LLVKLineEditor::scrollPointer(std::int32_t localX, std::size_t increment, std::string& error)
{
    error.clear();
    if (localX < mLeft && mScroll) mScroll -= std::min(mScroll,increment);
    else if (localX > mRight && mCursor < mDisplay.size())
    {
        const auto count = mCursor+1 > mScroll ? mCursor+1-mScroll : 0;
        const auto measured = mFont->measureRun(mDisplay,mScroll,count,mParams.scaleX,true,mParams.tabularNumbers,error);
        if (!measured) return false;
        if (std::floor(measured->width+0.5f)+mLeft >= mRight)
            mScroll += std::min(mDisplay.size()-mScroll,increment);
    }
    return true;
}

bool LLVKWidgetTree::lineEditorPointer(Id id, PointerEvent event, std::string& error)
{
    const auto emit = [&]
    {
        const auto found = mEvents.find(id);
        if (found == mEvents.end()) return;
        const auto callback = found->second.pointer;
        if (callback) callback(id,event);
    };
    if (event.kind == PointerKind::DoubleClick)
    {
        if (!requestControlFocus(id,true,error)) return false;
        if (!get(id)) return true;
        auto& editor = *mNodes.at(id).lineEditor;
        editor.tripleClickUntil = event.time+0.3;
        const bool all = editor.text.selectionEnd() == 0 && editor.text.selectionStart() == editor.text.display().size();
        if (all)
        {
            auto click = event;
            click.kind = PointerKind::LeftDown;
            if (!lineEditorPointer(id,click,error)) return false;
        }
        else
        {
            auto text = editor.text;
            const auto validator = editor.params.inputPrevalidator;
            if (!text.selectWord(editor.lastSelectionStart,editor.lastSelectionEnd,validator,error)) return false;
            if (!get(id)) return true;
            mNodes.at(id).lineEditor->text = std::move(text);
        }
        if (get(id)) mNodes.at(id).lineEditor->text.finishSelection();
        return true;
    }
    if (event.kind == PointerKind::LeftDown)
    {
        if (childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        const auto snapshot = *get(id)->lineEditor;
        if (!snapshot.params.text.selectOnFocus || mKeyboardFocus == id)
        {
            auto text = snapshot.text;
            const bool shift = (event.modifiers & 0x0004) != 0;
            if (shift)
            {
                if (!text.point(event.x,true,snapshot.params.inputPrevalidator,error)) return false;
            }
            else if (!snapshot.tripleClickUntil || event.time > *snapshot.tripleClickUntil)
            {
                if (!text.point(event.x,false,snapshot.params.inputPrevalidator,error)) return false;
                text.deselect();
                text.startSelection();
            }
            else
            {
                const bool allowed = !snapshot.params.inputPrevalidator || snapshot.params.inputPrevalidator(text.display());
                if (allowed && !text.selectAll(error)) return false;
                text.finishSelection();
            }
            if (!get(id)) return true;
            auto& editor = *mNodes.at(id).lineEditor;
            if (!shift && (!snapshot.tripleClickUntil || event.time > *snapshot.tripleClickUntil))
            {
                editor.lastSelectionStart = snapshot.text.selectionStart();
                editor.lastSelectionEnd = snapshot.text.selectionEnd();
            }
            editor.text = std::move(text);
            if (!setMouseCapture(id,error)) return false;
            if (!get(id)) return true;
        }
        if (!requestControlFocus(id,true,error)) return false;
        if (get(id)) emit();
        return true;
    }
    if (event.kind == PointerKind::Hover)
    {
        if (mMouseCapture != id && childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        const auto snapshot = *get(id)->lineEditor;
        if (mMouseCapture == id && snapshot.text.selecting())
        {
            auto text = snapshot.text;
            if (event.time > snapshot.scrollTime+0.05)
            {
                const auto elapsed = std::min(double(text.display().size()),std::floor((event.time-snapshot.scrollTime)/0.05+0.5));
                if (!text.scrollPointer(event.x,static_cast<std::size_t>(elapsed),error)) return false;
                mNodes.at(id).lineEditor->scrollTime = event.time;
            }
            if (!text.point(event.x,false,snapshot.params.inputPrevalidator,error)) return false;
            if (!get(id)) return true;
            mNodes.at(id).lineEditor->text = std::move(text);
        }
        notify(id,&Events::textCursor);
        return true;
    }
    if (event.kind == PointerKind::LeftUp)
    {
        bool handled = mMouseCapture == id;
        if (handled && !setMouseCapture(0,error)) return false;
        if (!get(id)) return true;
        if (!handled && childrenPointer(id,event,error)) return true;
        if (!error.empty()) return false;
        if (!get(id)) return true;
        if (get(id)->lineEditor->text.selecting())
        {
            auto text = get(id)->lineEditor->text;
            const auto validator = get(id)->lineEditor->params.inputPrevalidator;
            if (!text.point(event.x,false,validator,error)) return false;
            if (!get(id)) return true;
            mNodes.at(id).lineEditor->text = std::move(text);
            handled = true;
        }
        if (handled && canLineEditorCopy(id) && mClipboard && mClipboard->available(true))
        {
            std::string failure;
            copyLineEditor(id,true,failure);
        }
        if (get(id)) emit();
        return handled;
    }
    if (event.kind == PointerKind::MiddleDown)
    {
        if (!requestControlFocus(id,true,error)) return false;
        if (!get(id)) return true;
        if (canLineEditorPaste(id,true))
        {
            auto text = get(id)->lineEditor->text;
            const auto validator = get(id)->lineEditor->params.inputPrevalidator;
            if (!text.point(event.x,false,validator,error)) return false;
            if (!get(id)) return true;
            mNodes.at(id).lineEditor->text = std::move(text);
            return pasteLineEditor(id,true,error);
        }
        return true;
    }
    if (event.kind == PointerKind::RightDown)
    {
        if (!requestControlFocus(id,true,error)) return false;
        if (!get(id)) return true;
        basePointer(id,event,error);
        return error.empty();
    }
    return basePointer(id,event,error);
}
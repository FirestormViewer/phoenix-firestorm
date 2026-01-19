/**
 * @file llscripteditor.cpp
 * @author Cinder Roxley
 * @brief Text editor widget used for viewing and editing scripts
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"
#include "llscripteditor.h"

#include "llsyntaxid.h"
#include "lllocalcliprect.h"
#include "llviewercontrol.h"
#include "llworkerthread.h"
#include <algorithm>
#include <utility>
#include "llpreviewscript.h"

static LLWorkerThread& getSyntaxWorkerThread()
{
    static LLWorkerThread* sThread = new LLWorkerThread("SyntaxParse", true);
    return *sThread;
}

class LLScriptEditorSyntaxWorker final : public LLWorkerClass
{
public:
    struct Request
    {
        LLWString       text;
        S32             text_generation = 0;
        U32             keywords_generation = 0;
        bool            disable_highlight = false;
        LLKeywords*     keywords = nullptr;
    };

    struct Result
    {
        LLWString                   text;
        LLKeywords::segment_ops_t   ops;
        S32                         text_generation = 0;
        U32                         keywords_generation = 0;
        bool                        disable_highlight = false;
        LLKeywords*                 keywords = nullptr;
    };

    LLScriptEditorSyntaxWorker(LLWorkerThread* thread, LLScriptEditor* editor)
    :   LLWorkerClass(thread, "ScriptEditorSyntax"),
        mEditor(editor),
        mHasPending(false),
        mDropResults(false)
    {
    }

    void queueRequest(const Request& request)
    {
        bool should_add = false;
        {
            LLMutexLock lock(&mRequestMutex);
            mPendingRequest = request;
            mHasPending = true;
            should_add = (!haveWork() && !isWorking());
        }
        if (should_add)
        {
            addWork(0);
        }
    }

    void pump()
    {
        checkWork();
        bool should_add = false;
        {
            LLMutexLock lock(&mRequestMutex);
            should_add = (!haveWork() && !isWorking() && mHasPending);
        }
        if (should_add)
        {
            addWork(0);
        }
    }

    void waitForIdle(bool drop_results)
    {
        mDropResults = drop_results;
        {
            LLMutexLock lock(&mRequestMutex);
            mHasPending = false;
        }
        if (mRequestHandle != LLWorkerThread::nullHandle())
        {
            mWorkerThread->waitForResult(mRequestHandle, false);
            checkWork();
        }
        mDropResults = false;
    }

    void shutdown()
    {
        mEditor = nullptr;
        waitForIdle(true);
        scheduleDelete();
    }

private:
    void startWork(S32 param) override
    {
        LLMutexLock lock(&mRequestMutex);
        if (!mHasPending)
        {
            return;
        }
        mActiveRequest = mPendingRequest;
        mHasPending = false;
    }

    bool doWork(S32 param) override
    {
        Request request;
        {
            LLMutexLock lock(&mRequestMutex);
            request = mActiveRequest;
        }

        Result result;
        result.text = request.text;
        result.text_generation = request.text_generation;
        result.keywords_generation = request.keywords_generation;
        result.disable_highlight = request.disable_highlight;
        result.keywords = request.keywords;

        if (request.keywords)
        {
            request.keywords->collectSegmentOps(result.ops, request.text, request.disable_highlight);
        }

        {
            LLMutexLock lock(&mResultMutex);
            mResult = std::move(result);
        }
        return true;
    }

    void endWork(S32 param, bool aborted) override
    {
        if (aborted || mDropResults || !mEditor)
        {
            return;
        }

        Result result;
        {
            LLMutexLock lock(&mResultMutex);
            result = std::move(mResult);
        }

        if (mEditor->getTextGeneration() != result.text_generation)
        {
            return;
        }
        if (mEditor->getKeywordsGeneration() != result.keywords_generation)
        {
            return;
        }
        if (&mEditor->getKeywords() != result.keywords)
        {
            return;
        }

        mEditor->queueSyntaxApply(std::move(result.text),
                                  std::move(result.ops),
                                  result.text_generation,
                                  result.keywords_generation,
                                  result.disable_highlight,
                                  result.keywords);
    }

    LLScriptEditor* mEditor;
    LLMutex mRequestMutex;
    LLMutex mResultMutex;
    Request mPendingRequest;
    Request mActiveRequest;
    Result mResult;
    bool mHasPending;
    bool mDropResults;
};

// <FS:Ansariel> FIRE-23047: Increase width of line number column
//const S32 UI_TEXTEDITOR_LINE_NUMBER_MARGIN = 32;
const S32   UI_TEXTEDITOR_LINE_NUMBER_MARGIN = 40;

static LLDefaultChildRegistry::Register<LLScriptEditor> r("script_editor");

LLScriptEditor::Params::Params()
:   show_line_numbers("show_line_numbers", true),
    default_font_size("default_font_size", false)
{}


LLScriptEditor::LLScriptEditor(const Params& p)
:   LLTextEditor(p)
,   mShowLineNumbers(p.show_line_numbers),
    mUseDefaultFontSize(p.default_font_size),
    mLuauLanguage(false),
    mKeywordsGeneration(0),
    mLastQueuedTextGeneration(-1),
    mLastQueuedKeywordsGeneration(0),
    mLastQueuedDisableHighlight(false),
    mLastQueuedKeywords(nullptr),
    mSyntaxWorker(nullptr),
    mSyntaxApplyState(SyntaxApplyState::Idle),
    mPendingApplyOpIndex(0),
    mPendingApplySegmentIndex(0),
    mPendingApplyTextGeneration(-1),
    mPendingApplyKeywordsGeneration(0),
    mPendingApplyDisableHighlight(false),
    mPendingApplyKeywords(nullptr)
{
    if (mShowLineNumbers)
    {
        mHPad += UI_TEXTEDITOR_LINE_NUMBER_MARGIN;
        updateRects();
    }
}

LLScriptEditor::~LLScriptEditor()
{
    if (mSyntaxWorker)
    {
        mSyntaxWorker->shutdown();
        getSyntaxWorkerThread().update(0.f);
        mSyntaxWorker = nullptr;
    }
}

bool LLScriptEditor::postBuild()
{
    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //gSavedSettings.getControl("LSLFontSizeName")->getCommitSignal()->connect(boost::bind(&LLScriptEditor::onFontSizeChange, this));
    bool result = LLTextEditor::postBuild();
    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //setFont(getScriptFont());
    setFont(getFont());
    // </FS:Ansariel>
    return result;
}

void LLScriptEditor::draw()
{
    {
        // pad clipping rectangle so that cursor can draw at full width
        // when at left edge of mVisibleTextRect
        LLRect clip_rect(mVisibleTextRect);
        clip_rect.stretch(1);
        LLLocalClipRect clip(clip_rect);
    }

    LLTextBase::draw();
    drawLineNumbers();

    drawPreeditMarker();

    //RN: the decision was made to always show the orange border for keyboard focus but do not put an insertion caret
    // when in readonly mode
    mBorder->setKeyboardFocusHighlight( hasFocus() );// && !mReadOnly);
}

void LLScriptEditor::drawLineNumbers()
{
    LLGLSUIDefault gls_ui;
    LLRect scrolled_view_rect = getVisibleDocumentRect();
    LLRect content_rect = getVisibleTextRect();
    LLLocalClipRect clip(content_rect);
    S32 first_line = getFirstVisibleLine();
    S32 num_lines = getLineCount();
    if (first_line >= num_lines)
    {
        return;
    }

    S32 cursor_line = mLineInfoList[getLineNumFromDocIndex(mCursorPos)].mLineNum;

    if (mShowLineNumbers)
    {
        //S32 left = 0; // <FS:Ansariel> FIRE-6955: Line numbers not using correct transparency
        S32 top = getRect().getHeight();
        S32 bottom = 0;

        // <FS:Ansariel> FIRE-6955: Line numbers not using correct transparency
        //gl_rect_2d(left, top, UI_TEXTEDITOR_LINE_NUMBER_MARGIN, bottom, mReadOnlyBgColor.get() ); // line number area always read-only
        //gl_rect_2d(UI_TEXTEDITOR_LINE_NUMBER_MARGIN, top, UI_TEXTEDITOR_LINE_NUMBER_MARGIN-1, bottom, LLColor4::grey3); // separator
        gl_rect_2d(UI_TEXTEDITOR_LINE_NUMBER_MARGIN, top, UI_TEXTEDITOR_LINE_NUMBER_MARGIN-1, bottom, LLColor4::grey3 % getCurrentTransparency()); // separator
        // </FS:Ansariel>

        S32 last_line_num = -1;

        for (S32 cur_line = first_line; cur_line < num_lines; cur_line++)
        {
            line_info& line = mLineInfoList[cur_line];

            if ((line.mRect.mTop - scrolled_view_rect.mBottom) < mVisibleTextRect.mBottom)
            {
                break;
            }

            S32 line_bottom = line.mRect.mBottom - scrolled_view_rect.mBottom + mVisibleTextRect.mBottom;
            // draw the line numbers
            if(line.mLineNum != last_line_num && line.mRect.mTop <= scrolled_view_rect.mTop)
            {
                const LLWString ltext = utf8str_to_wstring(llformat("%d", mLuauLanguage ? line.mLineNum + 1 : line.mLineNum));
                bool is_cur_line = cursor_line == line.mLineNum;
                const U8 style = is_cur_line ? LLFontGL::BOLD : LLFontGL::NORMAL;
                const LLColor4& fg_color = is_cur_line ? mCursorColor : mReadOnlyFgColor;
                getFont()->render(
                                 ltext, // string to draw
                                 0, // begin offset
                                 UI_TEXTEDITOR_LINE_NUMBER_MARGIN - 2, // x
                                 (F32)line_bottom, // y
                                 fg_color,
                                 LLFontGL::RIGHT, // horizontal alignment
                                 LLFontGL::BOTTOM, // vertical alignment
                                 style,
                                 LLFontGL::NO_SHADOW,
                                 S32_MAX, // max chars
                                 UI_TEXTEDITOR_LINE_NUMBER_MARGIN - 2); // max pixels
                last_line_num = line.mLineNum;
            }
        }
    }
}

void LLScriptEditor::initKeywords(bool luau_language)
{
    mKeywordsLua.initialize(LLSyntaxLua::getInstance()->getKeywordsXML(), true);
    mKeywordsLSL.initialize(LLSyntaxIdLSL::getInstance()->getKeywordsXML(), false);

    mLuauLanguage = luau_language;

}

void LLScriptEditor::loadKeywords()
{
    LL_PROFILE_ZONE_SCOPED;
    ensureSyntaxWorker();
    mSyntaxWorker->waitForIdle(true);
    resetPendingSyntaxApply();
    getKeywords().processTokens();
    ++mKeywordsGeneration;

    LLKeywords::segment_ops_t ops;
    const LLWString& text = getWText();
    const llwchar* base = text.c_str();
    for (const llwchar* cur = base; *cur; ++cur)
    {
        if (*cur == '\n')
        {
            ops.push_back({LLKeywords::SegmentOp::OP_LINE_BREAK, (S32)(cur - base), 0, nullptr});
        }
    }
    applySyntaxSegments(text, ops);
    queueSyntaxParse();
}

// <FS:Ansariel> Re-add legacy format support
void LLScriptEditor::loadKeywords(const std::string& filename,
                                const std::vector<std::string>& funcs,
                                const std::vector<std::string>& tooltips,
                                const LLColor3& color)
{
    LL_PROFILE_ZONE_SCOPED;
    if (mKeywordsLSL.loadFromLegacyFile(filename))
    {
        auto count = llmin(funcs.size(), tooltips.size());
        for (size_t i = 0; i < count; i++)
        {
            std::string name = utf8str_trim(funcs[i]);
            mKeywordsLSL.addToken(LLKeywordToken::TT_WORD, name, LLUIColor(color), tooltips[i] );
        }
        segment_vec_t segment_list;
        LLStyleConstSP style = new LLStyle(LLStyle::Params().font(getFont()).color(mDefaultColor.get()));
        mKeywordsLSL.findSegments(&segment_list, getWText(), *this, style);

        mSegments.clear();
        segment_set_t::iterator insert_it = mSegments.begin();
        for (segment_vec_t::iterator list_it = segment_list.begin(); list_it != segment_list.end(); ++list_it)
        {
            insert_it = mSegments.insert(insert_it, *list_it);
        }
    }
}
// </FS:Ansariel>

void LLScriptEditor::updateSegments()
{
    if (getKeywords().isLoaded() && mParseOnTheFly)
    {
        LL_PROFILE_ZONE_SCOPED;
        ensureSyntaxWorker();
        mSyntaxWorker->pump();
        if (mReflowIndex < S32_MAX)
        {
            queueSyntaxParse();
        }
        processPendingSyntaxApply();
    }

    LLTextBase::updateSegments();
}

void LLScriptEditor::ensureSyntaxWorker()
{
    if (!mSyntaxWorker)
    {
        mSyntaxWorker = new LLScriptEditorSyntaxWorker(&getSyntaxWorkerThread(), this);
    }
}

void LLScriptEditor::queueSyntaxParse()
{
    static LLCachedControl<bool> sDisableSyntaxHighlighting(gSavedSettings, "ScriptEditorDisableSyntaxHighlight", false);
    const bool disable_syntax_highlighting = sDisableSyntaxHighlighting;

    LLKeywords* keywords = &getKeywords();
    const S32 text_generation = getTextGeneration();

    if (mLastQueuedTextGeneration == text_generation
        && mLastQueuedKeywordsGeneration == mKeywordsGeneration
        && mLastQueuedDisableHighlight == disable_syntax_highlighting
        && mLastQueuedKeywords == keywords)
    {
        return;
    }

    mLastQueuedTextGeneration = text_generation;
    mLastQueuedKeywordsGeneration = mKeywordsGeneration;
    mLastQueuedDisableHighlight = disable_syntax_highlighting;
    mLastQueuedKeywords = keywords;

    LLScriptEditorSyntaxWorker::Request request;
    request.text = getWText();
    request.text_generation = text_generation;
    request.keywords_generation = mKeywordsGeneration;
    request.disable_highlight = disable_syntax_highlighting;
    request.keywords = keywords;
    mSyntaxWorker->queueRequest(request);
}

void LLScriptEditor::queueSyntaxApply(LLWString text,
                                      LLKeywords::segment_ops_t ops,
                                      S32 text_generation,
                                      U32 keywords_generation,
                                      bool disable_highlight,
                                      LLKeywords* keywords)
{
    if (text_generation != getTextGeneration()
        || keywords_generation != mKeywordsGeneration
        || keywords != &getKeywords())
    {
        return;
    }

    resetPendingSyntaxApply();

    // Small updates can apply synchronously; larger ones use background slices.
    constexpr size_t kImmediateOpsThreshold = 500;
    if (ops.size() <= kImmediateOpsThreshold)
    {
        applySyntaxSegments(text, ops);
        return;
    }

    mPendingApplyText = std::move(text);
    mPendingApplyOps = std::move(ops);
    mPendingApplyTextGeneration = text_generation;
    mPendingApplyKeywordsGeneration = keywords_generation;
    mPendingApplyDisableHighlight = disable_highlight;
    mPendingApplyKeywords = keywords;
    mPendingApplyOpIndex = 0;
    mPendingApplySegmentIndex = 0;
    mPendingApplySegments.clear();
    mPendingApplySegmentSet.clear();
    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //mPendingApplyStyle = new LLStyle(LLStyle::Params().font(getScriptFont()).color(mDefaultColor.get()));
    mPendingApplyStyle = new LLStyle(LLStyle::Params().font(getFont()).color(mDefaultColor.get()));
    // </FS:Ansariel>
    mSyntaxApplyState = SyntaxApplyState::Building;
}

void LLScriptEditor::processPendingSyntaxApply()
{
    if (mSyntaxApplyState == SyntaxApplyState::Idle)
    {
        return;
    }

    static LLCachedControl<bool> sDisableSyntaxHighlighting(gSavedSettings, "ScriptEditorDisableSyntaxHighlight", false);
    if (mPendingApplyTextGeneration != getTextGeneration()
        || mPendingApplyKeywordsGeneration != mKeywordsGeneration
        || mPendingApplyKeywords != &getKeywords()
        || mPendingApplyDisableHighlight != sDisableSyntaxHighlighting)
    {
        resetPendingSyntaxApply();
        return;
    }

    // Slice sizes control UI budget while applying large highlight updates.
    constexpr size_t kOpsPerSlice = 250;
    constexpr size_t kSegmentsPerSlice = 250;

    if (mSyntaxApplyState == SyntaxApplyState::Building)
    {
        if (mPendingApplyStyle.isNull())
        {
            // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
            //mPendingApplyStyle = new LLStyle(LLStyle::Params().font(getScriptFont()).color(mDefaultColor.get()));
            mPendingApplyStyle = new LLStyle(LLStyle::Params().font(getFont()).color(mDefaultColor.get()));
            // </FS:Ansariel>
        }
        bool done = getKeywords().applySegmentOpsRange(&mPendingApplySegments,
                                                      mPendingApplyText,
                                                      mPendingApplyOps,
                                                      mPendingApplyOpIndex,
                                                      kOpsPerSlice,
                                                      *this,
                                                      mPendingApplyStyle);
        if (done)
        {
            mSyntaxApplyState = SyntaxApplyState::Inserting;
        }
        return;
    }

    size_t end_index = std::min(mPendingApplySegmentIndex + kSegmentsPerSlice, mPendingApplySegments.size());
    for (; mPendingApplySegmentIndex < end_index; ++mPendingApplySegmentIndex)
    {
        LLTextSegmentPtr segment = mPendingApplySegments[mPendingApplySegmentIndex];
        segment->linkToDocument(this);
        mPendingApplySegmentSet.insert(segment);
    }

    if (mPendingApplySegmentIndex >= mPendingApplySegments.size())
    {
        S32 saved_scroll_index = mScrollIndex;
        segment_set_t old_segments;
        old_segments.swap(mSegments);
        mSegments.swap(mPendingApplySegmentSet);
        mScrollIndex = saved_scroll_index;
        needsReflow(0);
        resetPendingSyntaxApply();
    }
}

void LLScriptEditor::resetPendingSyntaxApply()
{
    mSyntaxApplyState = SyntaxApplyState::Idle;
    mPendingApplyText.clear();
    mPendingApplyOps.clear();
    mPendingApplySegments.clear();
    mPendingApplySegmentSet.clear();
    mPendingApplyOpIndex = 0;
    mPendingApplySegmentIndex = 0;
    mPendingApplyStyle = LLStyleConstSP();
    mPendingApplyTextGeneration = -1;
    mPendingApplyKeywordsGeneration = 0;
    mPendingApplyDisableHighlight = false;
    mPendingApplyKeywords = nullptr;
}

void LLScriptEditor::applySyntaxSegments(const LLWString& text, const LLKeywords::segment_ops_t& ops)
{
    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //LLStyleConstSP style = new LLStyle(LLStyle::Params().font(getScriptFont()).color(mDefaultColor.get()));
    LLStyleConstSP style = new LLStyle(LLStyle::Params().font(getFont()).color(mDefaultColor.get()));

    segment_vec_t segment_list;
    getKeywords().applySegmentOps(&segment_list, text, ops, *this, style);

    clearSegments();
    for (segment_vec_t::iterator list_it = segment_list.begin(); list_it != segment_list.end(); ++list_it)
    {
        insertSegment(*list_it);
    }
}

void LLScriptEditor::clearSegments()
{
    if (!mSegments.empty())
    {
        mSegments.clear();
    }
}

LLKeywords::keyword_iterator_t LLScriptEditor::keywordsBegin()
{
    return getKeywords().begin();
}

LLKeywords::keyword_iterator_t LLScriptEditor::keywordsEnd()
{
    return getKeywords().end();
}

LLKeywords& LLScriptEditor::getKeywords()
{
    return mLuauLanguage ? mKeywordsLua : mKeywordsLSL;
}

// Most of this is shamelessly copied from LLTextBase
void LLScriptEditor::drawSelectionBackground()
{
    // Draw selection even if we don't have keyboard focus for search/replace
    if( hasSelection() && !mLineInfoList.empty())
    {
        std::vector<LLRect> selection_rects = getSelectionRects();

        gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
        const LLColor4& color = mReadOnly ? mReadOnlyFgColor : mFgColor;
        F32 alpha = hasFocus() ? 0.7f : 0.3f;
        alpha *= getDrawContext().mAlpha;
        // We want to shift the color to something readable but distinct
        LLColor4 selection_color((1.f + color.mV[VRED]) * 0.5f,
                                 (1.f + color.mV[VGREEN]) * 0.5f,
                                 (1.f + color.mV[VBLUE]) * 0.5f,
                                 alpha);
        LLRect content_display_rect = getVisibleDocumentRect();

        for (std::vector<LLRect>::iterator rect_it = selection_rects.begin();
             rect_it != selection_rects.end();
             ++rect_it)
        {
            LLRect selection_rect = *rect_it;
            selection_rect.translate(mVisibleTextRect.mLeft - content_display_rect.mLeft, mVisibleTextRect.mBottom - content_display_rect.mBottom);
            gl_rect_2d(selection_rect, selection_color);
        }
    }
}

// <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
//std::string LLScriptEditor::getScriptFontSize()
//{
//    static LLCachedControl<std::string> size_name(gSavedSettings, "LSLFontSizeName", "Monospace");
//    return size_name;
//}
//
//LLFontGL* LLScriptEditor::getScriptFont()
//{
//    std::string font_size_name = mUseDefaultFontSize ? "Monospace" : getScriptFontSize();
//    return LLFontGL::getFont(LLFontDescriptor("Monospace", font_size_name, 0));
//}
//
//void LLScriptEditor::onFontSizeChange()
//{
//    if (!mUseDefaultFontSize)
//    {
//        setFont(getScriptFont());
//        needsReflow();
//    }
//}
// </FS:Ansariel>

// <FS> Improved Home-key behavior
//virtual
void LLScriptEditor::startOfLine()
{
    const LLWString& text = getWText();
    const S32 line_start_pos = mCursorPos - getLineOffsetFromDocIndex(mCursorPos);

    S32 line_end_pos;
    S32 line = getLineNumFromDocIndex(mCursorPos);
    S32 num_lines = getLineCount();
    if (line + 1 >= num_lines)
    {
        line_end_pos = getLength();
    }
    else
    {
        line_end_pos = getLineStart(line + 1) - 1;
    }

    S32 trimmed_line_start_pos = line_start_pos;
    while (trimmed_line_start_pos < line_end_pos && LLStringOps::isSpace(text[trimmed_line_start_pos]))
    {
        ++trimmed_line_start_pos;
    }

    if (mCursorPos != trimmed_line_start_pos || mCursorPos == line_start_pos)
    {
        setCursorPos(trimmed_line_start_pos);
    }
    else if (mCursorPos == trimmed_line_start_pos)
    {
        setCursorPos(line_start_pos);
    }
}
// </FS>

// <FS:Ansariel> Show keyword help on F1
bool LLScriptEditor::handleKeyHere(KEY key, MASK mask)
{
    if (key == KEY_F1 && mask == MASK_NONE)
    {
        if (LLScriptEdCore* parent = getParentByType<LLScriptEdCore>(); parent != nullptr)
        {
            parent->onBtnDynamicHelp();
            return true;
        }
    }

    return LLTextEditor::handleKeyHere(key, mask);
}
// </FS:Ansariel>

/**
 * @file llscripteditor.h
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

#ifndef LL_SCRIPTEDITOR_H
#define LL_SCRIPTEDITOR_H

#include "lltexteditor.h"
#include <cstddef>

class LLScriptEditor : public LLTextEditor
{
public:

    struct Params : public LLInitParam::Block<Params, LLTextEditor::Params>
    {
        Optional<bool>      show_line_numbers;
        Optional<bool> default_font_size;
        Params();
    };

    ~LLScriptEditor() override;

    // LLView override
    void    draw() override;
    bool    postBuild() override;

    // <FS> Improved Home-key behavior
    // LLTextBase override
    void    startOfLine() override;

    void    initKeywords(bool luau_language = false);
    void    loadKeywords();
    void    clearSegments();
    LLKeywords::keyword_iterator_t keywordsBegin();
    LLKeywords::keyword_iterator_t keywordsEnd();
    LLKeywords& getKeywords();
    bool    getIsLuauLanguage() { return mLuauLanguage; }
    void    setLuauLanguage(bool luau_language) { mLuauLanguage = luau_language; }
    U32     getKeywordsGeneration() const { return mKeywordsGeneration; }
    void    applySyntaxSegments(const LLWString& text, const LLKeywords::segment_ops_t& ops);

    // <FS:Ansariel> FIRE-20818: User-selectable font and size for script editor
    //static std::string getScriptFontSize();
    //LLFontGL* getScriptFont();
    //void onFontSizeChange();
    // <FS:Ansariel>

    // <FS:Ansariel> Re-add legacy format support
    void    loadKeywords(const std::string& filename,
                         const std::vector<std::string>& funcs,
                         const std::vector<std::string>& tooltips,
                         const LLColor3& func_color);
    // </FS:Ansariel>

protected:
    friend class LLUICtrlFactory;
    friend class LLScriptEditorSyntaxWorker;
    LLScriptEditor(const Params& p);

private:
    enum class SyntaxApplyState
    {
        Idle,
        Building,
        Inserting
    };
    void    drawLineNumbers();
    void  updateSegments() override;
    void  drawSelectionBackground() override;
    void    ensureSyntaxWorker();
    void    queueSyntaxParse();
    void    queueSyntaxApply(LLWString text,
                             LLKeywords::segment_ops_t ops,
                             S32 text_generation,
                             U32 keywords_generation,
                             bool disable_highlight,
                             LLKeywords* keywords);
    void    processPendingSyntaxApply();
    void    resetPendingSyntaxApply();

    // <FS:Ansariel> Show keyword help on F1
    bool handleKeyHere(KEY key, MASK mask) override;

    LLKeywords  mKeywordsLua;
    LLKeywords  mKeywordsLSL;
    bool        mLuauLanguage;

    bool        mShowLineNumbers;
    bool mUseDefaultFontSize;
    U32         mKeywordsGeneration;
    S32         mLastQueuedTextGeneration;
    U32         mLastQueuedKeywordsGeneration;
    bool        mLastQueuedDisableHighlight;
    LLKeywords* mLastQueuedKeywords;
    class LLScriptEditorSyntaxWorker* mSyntaxWorker;
    SyntaxApplyState mSyntaxApplyState;
    LLWString   mPendingApplyText;
    LLKeywords::segment_ops_t mPendingApplyOps;
    segment_vec_t mPendingApplySegments;
    segment_set_t mPendingApplySegmentSet;
    size_t      mPendingApplyOpIndex;
    size_t      mPendingApplySegmentIndex;
    LLStyleConstSP mPendingApplyStyle;
    S32         mPendingApplyTextGeneration;
    U32         mPendingApplyKeywordsGeneration;
    bool        mPendingApplyDisableHighlight;
    LLKeywords* mPendingApplyKeywords;
};

#endif // LL_SCRIPTEDITOR_H

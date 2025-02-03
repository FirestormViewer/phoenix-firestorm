/**
 * @file llhudtext.h
 * @brief LLHUDText class definition
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
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

#ifndef LL_LLHUDTEXT_H
#define LL_LLHUDTEXT_H

#include "llpointer.h"

#include "llhudobject.h"
#include "v4color.h"
#include "v4coloru.h"
#include "v2math.h"
#include "llrect.h"
#include "llfontgl.h"
#include "llfontvertexbuffer.h"
#include <set>
#include <vector>

// Renders a 2D text billboard floating at the location specified.
class LLDrawable;
class LLHUDText;

struct lltextobject_further_away
{
    bool operator()(const LLPointer<LLHUDText>& lhs, const LLPointer<LLHUDText>& rhs) const;
};

class LLHUDText : public LLHUDObject
{
protected:
    class LLHUDTextSegment
    {
    public:
        LLHUDTextSegment(const LLWString& text, const LLFontGL::StyleFlags style, const LLColor4& color, const LLFontGL* font)
        :   mColor(color),
            mStyle(style),
            mText(text),
            mFont(font)
        // <FS:minerjr> [FIRE-35019] Add LLHUDNameTag background to floating text and hover highlights
        //{}        
        {            
            // Added a bool check to see if the current line is blank (empty, or has only a single space character stored.
            // There are issues with users using "Hello World \n \n \n \n" as strings which would create a 4 text segments, but only 1 had actual text
            // and the background would cover all 4, and would not look nice. So store a flag on each segment to see if it is blank, so we don't have to
            // do the check every frame for every text segment.
            if (text.length() == 0 || text.find_first_not_of(utf8str_to_wstring(" "), 0) == std::string::npos)
            {
                mbIsBlank = true;
            }
            else
            {
                mbIsBlank = false;
            }            
        }
        // </FS:minerjr> [FIRE-35019]
        F32 getWidth(const LLFontGL* font);
        const LLWString& getText() const { return mText; }
        void clearFontWidthMap() { mFontWidthMap.clear(); }

        // <FS:minerjr> [FIRE-35019] Add LLHUDNameTag background to floating text and hover highlights
        bool isBlank() { return mbIsBlank; } // Accessor method for checking to see if the current Text Segment is blank
        // </FS:minerjr> [FIRE-35019]
        LLColor4                mColor;
        LLFontGL::StyleFlags    mStyle;
        const LLFontGL*         mFont;
        LLFontVertexBuffer      mFontBuffer;
    private:
        LLWString               mText;
        std::map<const LLFontGL*, F32> mFontWidthMap;
        // <FS:minerjr> [FIRE-35019] Add LLHUDNameTag background to floating text and hover highlights
        bool                    mbIsBlank; // True if mText length is 0, or only contains " " characters, otherwise false
        // <FS:minerjr> [FIRE-35019]
    };

public:
    typedef enum e_text_alignment
    {
        ALIGN_TEXT_LEFT,
        ALIGN_TEXT_CENTER
    } ETextAlignment;

    typedef enum e_vert_alignment
    {
        ALIGN_VERT_TOP,
        ALIGN_VERT_CENTER
    } EVertAlignment;

public:
    // Set entire string, eliminating existing lines
    void setString(const std::string& text_utf8);

    void clearString();

    // Add text a line at a time, allowing custom formatting
    void addLine(const std::string &text_utf8, const LLColor4& color, const LLFontGL::StyleFlags style = LLFontGL::NORMAL, const LLFontGL* font = NULL);

    // Sets the default font for lines with no font specified
    void setFont(const LLFontGL* font);
    void setColor(const LLColor4 &color);
    void setAlpha(F32 alpha);
    void setZCompare(const bool zcompare);
    void setDoFade(const bool do_fade);
//  void setVisibleOffScreen(bool visible) { mVisibleOffScreen = visible; }

    // mMaxLines of -1 means unlimited lines.
    void setMaxLines(S32 max_lines) { mMaxLines = max_lines; }
    void setFadeDistance(F32 fade_distance, F32 fade_range) { mFadeDistance = fade_distance; mFadeRange = fade_range; }
    void updateVisibility();
    LLVector2 updateScreenPos(LLVector2 &offset_target);
    void updateSize();
    void setMass(F32 mass) { mMass = llmax(0.1f, mass); }
    void setTextAlignment(ETextAlignment alignment) { mTextAlignment = alignment; }
    void setVertAlignment(EVertAlignment alignment) { mVertAlignment = alignment; }
    /*virtual*/ void markDead();
    friend class LLHUDObject;
    /*virtual*/ F32 getDistance() const { return mLastDistance; }
    bool getVisible() { return mVisible; }
    bool getHidden() const { return mHidden; }
    void setHidden( bool hide ) { mHidden = hide; }
    void setOnHUDAttachment(bool on_hud) { mOnHUDAttachment = on_hud; }
    void shift(const LLVector3& offset);

    static void shiftAll(const LLVector3& offset);
    static void renderAllHUD();
    static void reshape();
    static void setDisplayText(bool flag) { sDisplayText = flag ; }

// [RLVa:KB] - Checked: RLVa-2.0.3
    const std::string& getObjectText() const                        { return mObjText; }
    void               setObjectText(const std::string &utf8string) { mObjText = utf8string; }

    enum EObjectTextFilter { OTF_NONE, OTF_HUD_ATTACHMENTS };
    static void        refreshAllObjectText(EObjectTextFilter eObjFilter = OTF_NONE);
// [/RLVa:KB]

    // <FS:Ansariel> FIRE-17393: Control HUD text fading by options
    static void onFadeSettingsChanged();

protected:
    LLHUDText(const U8 type);

    /*virtual*/ void render();
    void renderText();
    static void updateAll();
    S32 getMaxLines();

private:
    ~LLHUDText();
    bool            mOnHUDAttachment;
    bool            mDoFade;
    F32             mFadeRange;
    F32             mFadeDistance;
    F32             mLastDistance;
    bool            mZCompare;
//  bool            mVisibleOffScreen;
    bool            mOffscreen;
    LLColor4        mColor;
    LLVector3       mScale;
    F32             mWidth;
    F32             mHeight;
    LLColor4U       mPickColor;
    const LLFontGL* mFontp;
    const LLFontGL* mBoldFontp;
    LLRectf         mSoftScreenRect;
    LLVector3       mPositionAgent;
    LLVector2       mPositionOffset;
    LLVector2       mTargetPositionOffset;
    F32             mMass;
    S32             mMaxLines;
    S32             mOffsetY;
    F32             mRadius;
    std::vector<LLHUDTextSegment> mTextSegments;
    ETextAlignment  mTextAlignment;
    EVertAlignment  mVertAlignment;
    bool            mHidden;
// [RLVa:KB] - Checked: RLVa-1.0.0
    std::string     mObjText;
// [/RLVa:KB]

    // <FS:minerjr> [FIRE-35019] Add LLHUDNameTag background to floating text and hover highlights   
    LLPointer<LLUIImage> mRoundedRectImgp; // Added background rect image from LLHUDNameTag
    F32             mBackgroundHeight; // Store the actual height of the background image (calculated from the visible text segments)
    // <FS:minerjr> [FIRE-35078] llSetText(...) differences in latest Nightly Builds
    F32             mBackgroundWidth;  // Store the actual width of the background image (calculated from the visible text segments)
    // </FS:minerjr> [FIRE-35078]
    F32             mBackgroundOffsetY; // Store the offset of the top of the first visible text segment
    F32             mLuminance; // Store the luminance of the text (used to determine if the background should be white or black for higher contrast)
    // </FS:minerjr> [FIRE-35019]
    static bool    sDisplayText ;
    static std::set<LLPointer<LLHUDText> > sTextObjects;
    static std::vector<LLPointer<LLHUDText> > sVisibleTextObjects;
    static std::vector<LLPointer<LLHUDText> > sVisibleHUDTextObjects;
    typedef std::set<LLPointer<LLHUDText> >::iterator TextObjectIterator;
    typedef std::vector<LLPointer<LLHUDText> >::iterator VisibleTextObjectIterator;
};

#endif // LL_LLHUDTEXT_H

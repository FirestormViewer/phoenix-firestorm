/**
 * @file llhudtext.cpp
 * @brief Floating text above objects, set via script with llSetText()
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

#include "llviewerprecompiledheaders.h"

#include "llhudtext.h"

#include "llrender.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "lldrawable.h"
#include "llfontgl.h"
#include "llglheaders.h"
#include "llhudrender.h"
#include "llui.h"
#include "llviewercamera.h"
#include "llviewertexturelist.h"
#include "llviewerobject.h"
#include "llvovolume.h"
#include "llviewerwindow.h"
#include "llstatusbar.h"
#include "llmenugl.h"
#include "pipeline.h"
// [RLVa:KB] - Checked: RLVa-1.4.0
#include "rlvactions.h"
#include "rlvcommon.h"
// [/RLVa:KB]
#include <boost/tokenizer.hpp>
const F32 HORIZONTAL_PADDING = 15.f;
const F32 VERTICAL_PADDING = 12.f;
const F32 BUFFER_SIZE = 2.f;
const F32 HUD_TEXT_MAX_WIDTH = 190.f;
// <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
const F32 LINE_PADDING = 3.f; // aka "leading"
const S32 SHOW_BACKGROUND_ALL = 2;
const S32 SHOW_BACKGROUND_ONLY_HIGHLIGHTED = 1;
const S32 SHOW_BACKGROUND_NONE             = 0;
// </FS:minerjr>
const F32 HUD_TEXT_MAX_WIDTH_NO_BUBBLE = 1000.f;
const F32 MAX_DRAW_DISTANCE = 300.f;

std::set<LLPointer<LLHUDText> > LLHUDText::sTextObjects;
std::vector<LLPointer<LLHUDText> > LLHUDText::sVisibleTextObjects;
std::vector<LLPointer<LLHUDText> > LLHUDText::sVisibleHUDTextObjects;
bool LLHUDText::sDisplayText = true ;

bool lltextobject_further_away::operator()(const LLPointer<LLHUDText>& lhs, const LLPointer<LLHUDText>& rhs) const
{
    return lhs->getDistance() > rhs->getDistance();
}


LLHUDText::LLHUDText(const U8 type) :
            LLHUDObject(type),
            mOnHUDAttachment(false),
//          mVisibleOffScreen(false),
            mWidth(0.f),
            mHeight(0.f),
            mFontp(LLFontGL::getFontSansSerifSmall()),
            mBoldFontp(LLFontGL::getFontSansSerifBold()),
            mMass(1.f),
            mMaxLines(10),
            mOffsetY(0),
            mTextAlignment(ALIGN_TEXT_CENTER),
            mVertAlignment(ALIGN_VERT_CENTER),
//          mLOD(0),
            mHidden(false)
{
    mColor = LLColor4(1.f, 1.f, 1.f, 1.f);
    mDoFade = true;
    // <FS:Ansariel> FIRE-17393: Control HUD text fading by options
    //mFadeDistance = 8.f;
    //mFadeRange = 4.f;
    mFadeDistance = gSavedSettings.getF32("FSHudTextFadeDistance");
    mFadeRange = gSavedSettings.getF32("FSHudTextFadeRange");
    // </FS:Ansariel>
    // <FS:minerjr> //Just to get rid of a warning.
    mLastDistance = 0.0f;
    // </FS:minerjr>
    mZCompare = true;
    mOffscreen = false;
    mRadius = 0.1f;
    LLPointer<LLHUDText> ptr(this);
    sTextObjects.insert(ptr);
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    mRoundedRectImgp = LLUI::getUIImage("Rounded_Rect");
    mShowBackground    = gSavedSettings.getS32("FSHudTextShowBackground");
    mbUseHoverHighlight = gSavedSettings.getBOOL("FSHudTextUseHoverHighlight");
    mBackgroundHeight   = 0.0f;
    mBackgroundOffsetY  = 0.0f;
    mLuminance = 0.0f;
    // </FS:minerjr>
}

LLHUDText::~LLHUDText()
{
}

void LLHUDText::render()
{
    if (!mOnHUDAttachment && sDisplayText)
    {
        // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
        if (mbUseHoverHighlight && mbIsHighlighted)
        {            
            LLGLDepthTest gls_depth(GL_FALSE, GL_FALSE);
        }
        else
        {
            LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
        }
        // </FS:minerjr>
        //LLGLDisable gls_stencil(GL_STENCIL_TEST);
        renderText();
    }
}

void LLHUDText::renderText()
{
    if (!mVisible || mHidden)
    {
        return;
    }

    gGL.getTexUnit(0)->enable(LLTexUnit::TT_TEXTURE);

    LLGLState gls_blend(GL_BLEND, true);

    LLColor4 shadow_color(0.f, 0.f, 0.f, 1.f);
    F32 alpha_factor = 1.f;
    LLColor4 text_color = mColor;
    if (mDoFade)
    {
        // <FS:Ansariel> FIRE-17393: Control HUD text fading by options
        //if (mLastDistance > mFadeDistance)
        if (mLastDistance > mFadeDistance && mFadeRange > 0.f)
        // </FS:Ansariel>
        {
            alpha_factor = llmax(0.f, 1.f - (mLastDistance - mFadeDistance)/mFadeRange);
            text_color.mV[3] = text_color.mV[3]*alpha_factor;
        }
    }
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    if ((text_color.mV[3] < 0.01f && !mbUseHoverHighlight) || (text_color.mV[3] < 0.01f && mbUseHoverHighlight && !mbIsHighlighted))
    {
        return;
    }
    //If there is only 1 string and it is blank, don't render as it could add a background on a prim with no text
    if ((S32)mTextSegments.size() == 1 && (mTextSegments[0].isBlank()))
    {
        return;
    }
    // </FS:minerjr>
    shadow_color.mV[3] = text_color.mV[3];

    mOffsetY = lltrunc(mHeight * ((mVertAlignment == ALIGN_VERT_CENTER) ? 0.5f : 1.f));

    // *TODO: make this a per-text setting
    static LLCachedControl<F32> bubble_opacity(gSavedSettings, "ChatBubbleOpacity");
    static LLUIColor nametag_bg_color = LLUIColorTable::instance().getColor("ObjectBubbleColor");
    LLColor4 bg_color = nametag_bg_color;
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    if (mShowBackground)
    {
        if (mLuminance > 0.4f)
        {
            bg_color.set(0.0f, 0.0f, 0.0f); // Set to black and keep the alpha
        }
        else
        {
            bg_color.set(1.0f, 1.0f, 1.0f); // Set to white and keep the alpha
        }
    }
    if (mbUseHoverHighlight && mbIsHighlighted)
    {
        alpha_factor     = 1.0f;
        text_color.setAlpha(alpha_factor);
        //If the user has show background on only the highlighed prim, then use a transparcny, otherwise, make the background opaque to better see against the other backgrounds.
        bg_color.setAlpha(mShowBackground == SHOW_BACKGROUND_ONLY_HIGHLIGHTED ? bubble_opacity : alpha_factor);
        shadow_color.setAlpha(alpha_factor);
    }
    else
    {
        //Use the original color setting
        bg_color.setAlpha(bubble_opacity * alpha_factor);
    }
    // </FS:minerjr>
    const S32 border_height = 16;
    const S32 border_width = 16;

    // *TODO move this into helper function
    F32 border_scale = 1.f;

    if (border_height * 2 > mHeight)
    {
        border_scale = (F32)mHeight / ((F32)border_height * 2.f);
    }
    if (border_width * 2 > mWidth)
    {
        border_scale = llmin(border_scale, (F32)mWidth / ((F32)border_width * 2.f));
    }

    // scale screen size of borders down
    //RN: for now, text on hud objects is never occluded

    LLVector3 x_pixel_vec;
    LLVector3 y_pixel_vec;

    if (mOnHUDAttachment)
    {
        x_pixel_vec = LLVector3::y_axis / (F32)gViewerWindow->getWorldViewWidthRaw();
        y_pixel_vec = LLVector3::z_axis / (F32)gViewerWindow->getWorldViewHeightRaw();
    }
    else
    {
        LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);
    }

    LLVector3 width_vec = mWidth * x_pixel_vec;
    LLVector3 height_vec = mHeight * y_pixel_vec;

    mRadius = (width_vec + height_vec).magVec() * 0.5f;

    LLVector2 screen_offset;
    screen_offset = mPositionOffset;

    LLVector3 render_position = mPositionAgent
            + (x_pixel_vec * screen_offset.mV[VX])
            + (y_pixel_vec * screen_offset.mV[VY]);

    F32 y_offset = (F32)mOffsetY;

    // Render label
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    //Render text window background
    // Don't add if the parent object is attached (clothing on player avatar: as some cloths have blank text and render a background...)
    // Also, only show the background if it is show all is checked, or if on highlight, only if use hover highlight is enabled.
    if ((mShowBackground == SHOW_BACKGROUND_ALL ||
         mShowBackground == SHOW_BACKGROUND_ONLY_HIGHLIGHTED && mbIsHighlighted && mbUseHoverHighlight) &&
        mSourceObject->getAttachmentState() == 0)
    {
        LLRect screen_rect;
        screen_rect.setCenterAndSize(0, static_cast<S32>(lltrunc(-mBackgroundHeight / 2 + mOffsetY + mBackgroundOffsetY)),
                                     static_cast<S32>(lltrunc(mWidth)),
                                     static_cast<S32>(lltrunc(mBackgroundHeight)));
        mRoundedRectImgp->draw3D(render_position, x_pixel_vec, y_pixel_vec, screen_rect, bg_color);
    }
    // </FS:minerjr>
    // Render text
    {
        // -1 mMaxLines means unlimited lines.
        S32 start_segment;
        S32 max_lines = getMaxLines();

        if (max_lines < 0)
        {
            start_segment = 0;
        }
        else
        {
            start_segment = llmax((S32)0, (S32)mTextSegments.size() - max_lines);
        }

        for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin() + start_segment;
             segment_iter != mTextSegments.end(); ++segment_iter )
        {
            const LLFontGL* fontp = segment_iter->mFont;
            //y_offset -= fontp->getLineHeight() - 1; // correction factor to match legacy font metrics
            //  <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
            y_offset -= fontp->getLineHeight();//Match the same positioning as LLHUBNameTag as the windows don't line up otherwise.
            y_offset -= LINE_PADDING;
            // </FS:minerjr>
            U8 style = segment_iter->mStyle;
            LLFontGL::ShadowType shadow = LLFontGL::DROP_SHADOW;

            F32 x_offset;
            if (mTextAlignment== ALIGN_TEXT_CENTER)
            {
                x_offset = -0.5f*segment_iter->getWidth(fontp);
            }
            else // ALIGN_LEFT
            {
                x_offset = -0.5f * mWidth + (HORIZONTAL_PADDING / 2.f);
                //  <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
                // *HACK -> borrowed from LLHUBNameTag to match
                x_offset += 1;
                //  </FS:minerjr>
            }

            text_color = segment_iter->mColor;
            //  <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
            if (mbUseHoverHighlight && mbIsHighlighted)
            {
                text_color.mV[VALPHA] = alpha_factor;
            }
            else
            {
                if (mOnHUDAttachment)
                {
                    text_color = linearColor4(text_color);
                }
                text_color.mV[VALPHA] *= alpha_factor;
            }
            //  </FS:minerjr>
            hud_render_text(segment_iter->getText(), render_position, *fontp, style, shadow, x_offset, y_offset, text_color, mOnHUDAttachment);
        }
    }
    /// Reset the default color to white.  The renderer expects this to be the default.
    gGL.color4f(1.0f, 1.0f, 1.0f, 1.0f);
}

void LLHUDText::setString(const std::string &text_utf8)
{
    mTextSegments.clear();
//  addLine(text_utf8, mColor);
// [RLVa:KB] - Checked: RLVa-2.0.3
    // NOTE: setString() is called for debug and map beacons as well
    if (RlvActions::isRlvEnabled())
    {
        std::string text(text_utf8);
        if (RlvActions::canShowHoverText(mSourceObject))
        {
            if (!RlvActions::canShowLocation())
                RlvUtil::filterLocation(text);

            bool fCanShowNearby = RlvActions::canShowNearbyAgents();
            if ( (!RlvActions::canShowName(RlvActions::SNC_DEFAULT)) || (!fCanShowNearby) )
                RlvUtil::filterNames(text, true, !fCanShowNearby);
        }
        else
        {
            text = "";
        }
        addLine(text, mColor);
    }
    else
    {
        addLine(text_utf8, mColor);
    }
// [/RLVa:KB]
}

void LLHUDText::clearString()
{
    mTextSegments.clear();
}


void LLHUDText::addLine(const std::string &text_utf8,
                        const LLColor4& color,
                        const LLFontGL::StyleFlags style,
                        const LLFontGL* font)
{
    LLWString wline = utf8str_to_wstring(text_utf8);
    if (!wline.empty())
    {
        // use default font for segment if custom font not specified
        if (!font)
        {
            font = mFontp;
        }
        typedef boost::tokenizer<boost::char_separator<llwchar>, LLWString::const_iterator, LLWString > tokenizer;
        LLWString seps(utf8str_to_wstring("\r\n"));
        boost::char_separator<llwchar> sep(seps.c_str());

        tokenizer tokens(wline, sep);
        tokenizer::iterator iter = tokens.begin();

        while (iter != tokens.end())
        {
            U32 line_length = 0;
            do
            {
                F32 max_pixels = HUD_TEXT_MAX_WIDTH_NO_BUBBLE;
                S32 segment_length = font->maxDrawableChars(iter->substr(line_length).c_str(), max_pixels, static_cast<S32>(wline.length()), LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);
                LLHUDTextSegment segment(iter->substr(line_length, segment_length), style, color, font);
                mTextSegments.push_back(segment);
                line_length += segment_length;
            }
            while (line_length != iter->size());
            ++iter;
        }
    }
}

void LLHUDText::setZCompare(const bool zcompare)
{
    mZCompare = zcompare;
}

void LLHUDText::setFont(const LLFontGL* font)
{
    mFontp = font;
}


void LLHUDText::setColor(const LLColor4 &color)
{
    mColor = color;
    //  <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    // Added luminance value for the text color to determine if the background should be white or black
    // Based upon https://stackoverflow.com/questions/596216/formula-to-determine-perceived-brightness-of-rgb-color
    // used Digital ITU BT.601 (gives more weight to the R and B components):
    // Y = 0.299 R + 0.587 G + 0.114 B
    mLuminance = 0.299f * mColor.mV[0] + 0.587f * mColor.mV[1] + 0.114f * mColor.mV[2];
    //  </FS:minerjr>
    for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin();
         segment_iter != mTextSegments.end(); ++segment_iter )
    {
        segment_iter->mColor = color;
    }
}

void LLHUDText::setAlpha(F32 alpha)
{
    mColor.mV[VALPHA] = alpha;
    for (std::vector<LLHUDTextSegment>::iterator segment_iter = mTextSegments.begin();
         segment_iter != mTextSegments.end(); ++segment_iter )
    {
        segment_iter->mColor.mV[VALPHA] = alpha;
    }
}


void LLHUDText::setDoFade(const bool do_fade)
{
    mDoFade = do_fade;
}

void LLHUDText::updateVisibility()
{
    // <fs:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    // Create local doFade flag based upon member doFade flag and overide it if the object is highlighted and hover highlight is enabled.
    bool doFade = mDoFade;
    if (mbUseHoverHighlight && mbIsHighlighted)
    {
        doFade = false;
    }
    // </fs:minerjr>
    if (mSourceObject)
    {
        mSourceObject->updateText();
    }

    mPositionAgent = gAgent.getPosAgentFromGlobal(mPositionGlobal);

    if (!mSourceObject)
    {
        // Beacons
        mVisible = true;
        if (mOnHUDAttachment)
        {
            sVisibleHUDTextObjects.push_back(LLPointer<LLHUDText> (this));
        }
        else
        {
            sVisibleTextObjects.push_back(LLPointer<LLHUDText> (this));
        }
        return;
    }

    // Not visible if parent object is dead
    if (mSourceObject->isDead() || mSourceObject->mDrawable.isNull())
    {
        mVisible = false;
        return;
    }

    // for now, all text on hud objects is visible
    if (mOnHUDAttachment)
    {
        mVisible = true;
        sVisibleHUDTextObjects.push_back(LLPointer<LLHUDText> (this));
        mLastDistance = mPositionAgent.mV[VX];
        return;
    }

    // push text towards camera by radius of object, but not past camera
    LLVector3 vec_from_camera = mPositionAgent - LLViewerCamera::getInstance()->getOrigin();
    LLVector3 dir_from_camera = vec_from_camera;
    dir_from_camera.normVec();

    if (dir_from_camera * LLViewerCamera::getInstance()->getAtAxis() <= 0.f)
    { //text is behind camera, don't render
        mVisible = false;
        return;
    }

    if (vec_from_camera * LLViewerCamera::getInstance()->getAtAxis() <= LLViewerCamera::getInstance()->getNear() + 0.1f + mSourceObject->getVObjRadius())
    {
        mPositionAgent = LLViewerCamera::getInstance()->getOrigin() + vec_from_camera * ((LLViewerCamera::getInstance()->getNear() + 0.1f) / (vec_from_camera * LLViewerCamera::getInstance()->getAtAxis()));
    }
    else
    {
        mPositionAgent -= dir_from_camera * mSourceObject->getVObjRadius();
    }

    mLastDistance = (mPositionAgent - LLViewerCamera::getInstance()->getOrigin()).magVec();
    // <fs:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    // Use local do fade check to allow highlighed objects to force text to be visible
    if (!mTextSegments.size() || (doFade && (mLastDistance > mFadeDistance + mFadeRange)))
    {
        mVisible = false;
        return;
    }
    // </fs:minerjr>
    LLVector3 pos_agent_center = gAgent.getPosAgentFromGlobal(mPositionGlobal) - dir_from_camera;
    F32 last_distance_center = (pos_agent_center - LLViewerCamera::getInstance()->getOrigin()).magVec();
    static LLCachedControl<F32> prim_text_max_dist(gSavedSettings, "PrimTextMaxDrawDistance");
    F32 max_draw_distance = prim_text_max_dist;

    if(max_draw_distance < 0)
    {
        max_draw_distance = 0;
        gSavedSettings.setF32("PrimTextMaxDrawDistance", max_draw_distance);
    }
    else if(max_draw_distance > MAX_DRAW_DISTANCE)
    {
        max_draw_distance = MAX_DRAW_DISTANCE;
        gSavedSettings.setF32("PrimTextMaxDrawDistance", MAX_DRAW_DISTANCE);
    }

    if(last_distance_center > max_draw_distance)
    {
        mVisible = false;
        return;
    }


    LLVector3 x_pixel_vec;
    LLVector3 y_pixel_vec;

    LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);

    LLVector3 render_position = mPositionAgent +
            (x_pixel_vec * mPositionOffset.mV[VX]) +
            (y_pixel_vec * mPositionOffset.mV[VY]);

    mOffscreen = false;
    if (!LLViewerCamera::getInstance()->sphereInFrustum(render_position, mRadius))
    {
//      if (!mVisibleOffScreen)
//      {
            mVisible = false;
            return;
//      }
//      else
//      {
//          mOffscreen = true;
//      }
    }

    mVisible = true;
    sVisibleTextObjects.push_back(LLPointer<LLHUDText> (this));
}

LLVector2 LLHUDText::updateScreenPos(LLVector2 &offset)
{
    LLCoordGL screen_pos;
    LLVector2 screen_pos_vec;
    LLVector3 x_pixel_vec;
    LLVector3 y_pixel_vec;
    LLViewerCamera::getInstance()->getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);
//  LLVector3 world_pos = mPositionAgent + (offset.mV[VX] * x_pixel_vec) + (offset.mV[VY] * y_pixel_vec);
//  if (!LLViewerCamera::getInstance()->projectPosAgentToScreen(world_pos, screen_pos, false) && mVisibleOffScreen)
//  {
//      // bubble off-screen, so find a spot for it along screen edge
//      LLViewerCamera::getInstance()->projectPosAgentToScreenEdge(world_pos, screen_pos);
//  }

    screen_pos_vec.setVec((F32)screen_pos.mX, (F32)screen_pos.mY);

    LLRect world_rect = gViewerWindow->getWorldViewRectScaled();
    S32 bottom = world_rect.mBottom + STATUS_BAR_HEIGHT;

    LLVector2 screen_center;
    screen_center.mV[VX] = llclamp((F32)screen_pos_vec.mV[VX], (F32)world_rect.mLeft + mWidth * 0.5f, (F32)world_rect.mRight - mWidth * 0.5f);

    if(mVertAlignment == ALIGN_VERT_TOP)
    {
        screen_center.mV[VY] = llclamp((F32)screen_pos_vec.mV[VY],
            (F32)bottom,
            (F32)world_rect.mTop - mHeight - (F32)MENU_BAR_HEIGHT);
        mSoftScreenRect.setLeftTopAndSize(screen_center.mV[VX] - (mWidth + BUFFER_SIZE) * 0.5f,
            screen_center.mV[VY] + (mHeight + BUFFER_SIZE), mWidth + BUFFER_SIZE, mHeight + BUFFER_SIZE);
    }
    else
    {
        screen_center.mV[VY] = llclamp((F32)screen_pos_vec.mV[VY],
            (F32)bottom + mHeight * 0.5f,
            (F32)world_rect.mTop - mHeight * 0.5f - (F32)MENU_BAR_HEIGHT);
        mSoftScreenRect.setCenterAndSize(screen_center.mV[VX], screen_center.mV[VY], mWidth + BUFFER_SIZE, mHeight + BUFFER_SIZE);
    }

    return offset + (screen_center - LLVector2((F32)screen_pos.mX, (F32)screen_pos.mY));
}

void LLHUDText::updateSize()
{
    F32 height = 0.f;
    F32 width = 0.f;
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    // We want to create a background that fits just the visible text area only, otherwise a llsetstring('Hello, World\n\n\n') will have a text
    // box that covers 4 lines, but only the top line is visible to the user.
    // Another exaple is llsetstring('\n\n\nHello, World'), which would also have a 4 line high window, but the text be only on the last line.
    F32 backgroundFirstNoneBlankPosition = 0.0f;//Stores the position just above the first non blank line
    F32 backgroundLastNoneBlankPosition = 0.0f;//Stores the position just below the last none blank line
    bool firstNoneBlank                   = true;//Flag to determine that if the first blank line has been reached and to store the first none black position
    // </FS:minerjr>
    S32 max_lines = getMaxLines();

    S32 start_segment;
    if (max_lines < 0) start_segment = 0;
    else start_segment = llmax((S32)0, (S32)mTextSegments.size() - max_lines);

    std::vector<LLHUDTextSegment>::iterator iter = mTextSegments.begin() + start_segment;
    while (iter != mTextSegments.end())
    {
        const LLFontGL* fontp = iter->mFont;
        // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
        //If there is no blank on the current line, skip over it so that we don't make the window cover the empty space above or below a blank text box. (IE 1
        if (!iter->isBlank())
        {
            //If this is the first line without a blank, get the height above current line (0.0 for line 1, previous height if not the first line
            if (firstNoneBlank)
            {
                backgroundFirstNoneBlankPosition = height;
                firstNoneBlank = false;
            }
            //Always get the position below the non-blank line
            backgroundLastNoneBlankPosition = height + fontp->getLineHeight() + LINE_PADDING;            
        }
        // </FS:minerjr> 
        height += fontp->getLineHeight();
        height += LINE_PADDING;
        // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
        // The max width of the text is set to HUD_TEXT_MAX_WIDTH_NO_BUBBLE and not HUD_TEXT_MAX_WIDTH, so the window would be limited but the text could spill over...
        width = llmax(width, llmin(iter->getWidth(fontp), HUD_TEXT_MAX_WIDTH_NO_BUBBLE));
        // </FS:minerjr>
        ++iter;
    }

    // Don't want line spacing under the last line
    if (height > 0.f)
    {
        height -= LINE_PADDING;
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
        backgroundLastNoneBlankPosition -= LINE_PADDING;
    // </FS:minerjr> 
    }

    if (width == 0.f)
    {
        return;
    }

    width += HORIZONTAL_PADDING;
    height += VERTICAL_PADDING;
    backgroundLastNoneBlankPosition += VERTICAL_PADDING;

    // *TODO: Could do some sort of timer-based resize logic here
    //F32 u = 1.f;
    //mWidth = llmax(width, lerp(mWidth, (F32)width, u));
    //mHeight = llmax(height, lerp(mHeight, (F32)height, u));
    mWidth  = width;
    mHeight = height;
    // <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
    mBackgroundOffsetY = backgroundFirstNoneBlankPosition;
    mBackgroundHeight  = backgroundLastNoneBlankPosition - backgroundFirstNoneBlankPosition;
    // </FS:minerjr>
}

void LLHUDText::updateAll()
{
    // iterate over all text objects, calculate their restoration forces,
    // and add them to the visible set if they are on screen and close enough
    sVisibleTextObjects.clear();
    sVisibleHUDTextObjects.clear();

    TextObjectIterator text_it;
    for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
    {
        LLHUDText* textp = (*text_it);
        textp->mTargetPositionOffset.clearVec();
        textp->updateSize();
        textp->updateVisibility();
    }

    // sort back to front for rendering purposes
    std::sort(sVisibleTextObjects.begin(), sVisibleTextObjects.end(), lltextobject_further_away());
    std::sort(sVisibleHUDTextObjects.begin(), sVisibleHUDTextObjects.end(), lltextobject_further_away());
}

//void LLHUDText::setLOD(S32 lod)
//{
//  mLOD = lod;
//  //RN: uncomment this to visualize LOD levels
//  //std::string label = llformat("%d", lod);
//  //setLabel(label);
//}

S32 LLHUDText::getMaxLines()
{
    return mMaxLines;
    //switch(mLOD)
    //{
    //case 0:
    //  return mMaxLines;
    //case 1:
    //  return mMaxLines > 0 ? mMaxLines / 2 : 5;
    //case 2:
    //  return mMaxLines > 0 ? mMaxLines / 3 : 2;
    //default:
    //  // label only
    //  return 0;
    //}
}

void LLHUDText::markDead()
{
    // make sure we have at least one pointer
    // till the end of the function
    LLPointer<LLHUDText> ptr(this);
    sTextObjects.erase(ptr);
    LLHUDObject::markDead();
}

void LLHUDText::renderAllHUD()
{
    LLGLState::checkStates();

    {
        LLGLDepthTest depth(GL_FALSE, GL_FALSE);

        VisibleTextObjectIterator text_it;

        for (text_it = sVisibleHUDTextObjects.begin(); text_it != sVisibleHUDTextObjects.end(); ++text_it)
        {
            (*text_it)->renderText();
        }
    }

    LLVertexBuffer::unbind();

    LLGLState::checkStates();
}

void LLHUDText::shiftAll(const LLVector3& offset)
{
    TextObjectIterator text_it;
    for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
    {
        LLHUDText *textp = text_it->get();
        textp->shift(offset);
    }
}

void LLHUDText::shift(const LLVector3& offset)
{
    mPositionAgent += offset;
}

//static
// called when UI scale changes, to flush font width caches
void LLHUDText::reshape()
{
    TextObjectIterator text_it;
    for (text_it = sTextObjects.begin(); text_it != sTextObjects.end(); ++text_it)
    {
        LLHUDText* textp = (*text_it);
        std::vector<LLHUDTextSegment>::iterator segment_iter;
        for (segment_iter = textp->mTextSegments.begin();
             segment_iter != textp->mTextSegments.end(); ++segment_iter )
        {
            segment_iter->clearFontWidthMap();
        }
    }
}

//============================================================================

F32 LLHUDText::LLHUDTextSegment::getWidth(const LLFontGL* font)
{
    std::map<const LLFontGL*, F32>::iterator iter = mFontWidthMap.find(font);
    if (iter != mFontWidthMap.end())
    {
        return iter->second;
    }
    else
    {
        F32 width = font->getWidthF32(mText.c_str());
        mFontWidthMap[font] = width;
        return width;
    }
}

// [RLVa:KB] - Checked: RLVa-2.0.3
void LLHUDText::refreshAllObjectText(EObjectTextFilter eObjFilter)
{
    for (LLHUDText* pText : sTextObjects)
    {
        if ((pText) && (!pText->mObjText.empty()) && (pText->mSourceObject) && (LL_PCODE_VOLUME == pText->mSourceObject->getPCode()) &&
            ((OTF_NONE == eObjFilter) || ((OTF_HUD_ATTACHMENTS == eObjFilter) && (pText->mSourceObject->isHUDAttachment()))))
        {
            pText->setString(pText->mObjText);
        }
    }
}
// [/RLVa:KB]

// <FS:Ansariel> FIRE-17393: Control HUD text fading by options
// static
void LLHUDText::onFadeSettingsChanged()
{
    for (TextObjectIterator it = sTextObjects.begin(); it != sTextObjects.end(); ++it)
    {
        LLHUDText* text = *it;
        if (text)
        {
            text->mFadeDistance = gSavedSettings.getF32("FSHudTextFadeDistance");
            text->mFadeRange = gSavedSettings.getF32("FSHudTextFadeRange");
        }
    }
}
// </FS:Ansariel>


// <FS:minerjr> FIRE-35019 Add LLHUBNameTag background to floating text and hover highlights
// static
void LLHUDText::onHighlightSettingsChanged()
{    
    for (TextObjectIterator it = sTextObjects.begin(); it != sTextObjects.end(); ++it)
    {
        LLHUDText* text = *it;
        if (text)
        {
            text->mShowBackground   = gSavedSettings.getS32("FSHudTextShowBackground");
            text->mbUseHoverHighlight = gSavedSettings.getBOOL("FSHudTextUseHoverHighlight");
        }
    }
}
        
// </FS:minerjr>

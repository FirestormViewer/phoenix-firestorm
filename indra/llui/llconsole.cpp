/**
 * @file llconsole.cpp
 * @brief a scrolling console output device
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "llconsole.h"
#include "llmarkdown.h"
#include "llfontregistry.h"

// linden library includes
#include "llmath.h"
//#include "llviewercontrol.h"
#include "llcriticaldamp.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llui.h"
#include "lluiimage.h"
//#include "llviewerimage.h"
//#include "llviewerimagelist.h"
//#include "llviewerwindow.h"
#include "llsd.h"
#include "llfontgl.h"
#include "llmath.h"
#include "llurlregistry.h" // for SLURL parsing

//#include "llstartup.h"

// Used for LCD display
extern void AddNewDebugConsoleToLCD(const LLWString &newLine);

LLConsole* gConsole = NULL;  // Created and destroyed in LLViewerWindow.

const F32 FADE_DURATION = 2.f;

static LLDefaultChildRegistry::Register<LLConsole> r("console");

LLConsole::LLConsole(const LLConsole::Params& p)
:   LLUICtrl(p),
    LLFixedBuffer(p.max_lines),
    mLinePersistTime(p.persist_time), // seconds
    mFont(p.font),
    mConsoleWidth(0),
    mConsoleHeight(0),
    mParseUrls(p.parse_urls), // <FS:Ansariel> If lines should be parsed for URLs
    mSessionSupport(p.session_support) // <FS:Ansariel> Session support
{
    if (p.font_size_index.isProvided())
    {
        setFontSize(p.font_size_index);
    }
    if (mFont == nullptr)
    {
        setFontSize(0); // sans-serif
    }
    mFadeTime = mLinePersistTime - FADE_DURATION;
    setMaxLines(LLUI::getInstance()->mSettingGroups["config"]->getS32("ConsoleMaxLines"));

    // <FS:Ansariel> Configurable background for different console types
    mBackgroundImage = LLUI::getUIImage(p.background_image);
    // <VulkanStorm> retain the name for the GL-free Vulkan path
    mVkBgImageName = p.background_image.isProvided() ? p.background_image() : std::string();
    // </VulkanStorm>
}

// <VulkanStorm>
void LLConsole::prepareVkDraw()
{
    // draw()'s paragraph-expiry cleanup (only when session support is off;
    // with session support update() owns expiry).
    if (mSessionSupport)
    {
        return;
    }

    const F32 cur_time = mTimer.getElapsedTimeF32();
    const F32 skip_time = cur_time - mLinePersistTime;

    size_t num_lines{ 0 };
    paragraph_t::reverse_iterator paragraph_it = mParagraphs.rbegin();
    auto paragraph_num = mParagraphs.size();

    while (!mParagraphs.empty() && paragraph_it != mParagraphs.rend())
    {
        num_lines += (*paragraph_it).mLines.size();
        if (num_lines > mMaxLines
            || ((mLinePersistTime > (F32)0.f) && ((*paragraph_it).mAddTime - skip_time) / (mLinePersistTime - mFadeTime) <= (F32)0.f))
        {
            for (size_t i = 0; i < paragraph_num; i++)
            {
                if (!mParagraphs.empty())
                    mParagraphs.pop_front();
            }
            break;
        }
        paragraph_num--;
        paragraph_it++;
    }
}

void LLConsole::getVkDrawState(VkDrawState& out) const
{
    // Mirrors draw(): geometry/text in widget-LOCAL coordinates (the console
    // draws in its own frame; y grows upward from ~10px above the bottom).
    out = VkDrawState();

    constexpr F32 padding_horizontal = 15;
    constexpr F32 padding_vertical = 8;

    if (mParagraphs.empty())
    {
        return;
    }

    static LLUICachedControl<bool> showOnscreenConsole("FSShowOnscreenConsole");
    if (!showOnscreenConsole && this == gConsole)
    {
        return;
    }

    const F32 cur_time = mTimer.getElapsedTimeF32();
    const F32 skip_time = cur_time - mLinePersistTime;
    const F32 fade_time = cur_time - mFadeTime;

    out.visible = true;
    out.font = mFont;
    out.bg_image = mBackgroundImage.notNull() ? mBackgroundImage->getName() : mVkBgImageName;
    static LLUICachedControl<F32> consoleBackgroundOpacity("ConsoleBackgroundOpacity");
    static LLUIColor cbcolor = LLUIColorTable::instance().getColor("ConsoleBackground");
    out.bg_color = cbcolor.get();
    out.bg_color.mV[VALPHA] *= llclamp(consoleBackgroundOpacity(), 0.f, 1.f);

    const F32 line_height = (F32)mFont->getLineHeight();
    static LLUICachedControl<bool> classic_draw_mode("FSConsoleClassicDrawMode");

    F32 y_pos = 10.f;

    if (classic_draw_mode)
    {
        constexpr F32 padding_vert = 5.f;
        S32 total_width = 0;
        S32 total_height = 0;
        size_t lines_drawn = 0;

        for (auto paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); ++paragraph_it)
        {
            if (mSessionSupport)
            {
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }
            total_height += llfloor((*paragraph_it).mLines.size() * line_height + padding_vert);
            total_width = llmax(total_width, llfloor((*paragraph_it).mMaxWidth + padding_horizontal));
        }
        out.bg_rects.push_back(LLRect(-14, ll_round(y_pos + line_height / 2),
                                      -14 + total_width,
                                      ll_round(y_pos + line_height / 2) - total_height - ll_round((line_height - padding_vert) / 2.f)));

        lines_drawn = 0;
        for (auto paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); ++paragraph_it)
        {
            if (mSessionSupport)
            {
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }

            F32 y_off = 0;
            F32 alpha;
            S32 target_width = llfloor((*paragraph_it).mMaxWidth + padding_horizontal);
            y_pos += ((*paragraph_it).mLines.size()) * line_height;

            if ((mLinePersistTime > 0.f) && ((*paragraph_it).mAddTime < fade_time))
            {
                alpha = ((*paragraph_it).mAddTime - skip_time) / (mLinePersistTime - mFadeTime);
            }
            else
            {
                alpha = 1.0f;
            }

            if (alpha > 0.f)
            {
                for (auto line_it = (*paragraph_it).mLines.begin();
                     line_it != (*paragraph_it).mLines.end(); ++line_it)
                {
                    for (auto seg_it = (*line_it).mLineColorSegments.begin();
                         seg_it != (*line_it).mLineColorSegments.end(); ++seg_it)
                    {
                        VkTextRun run;
                        run.text = (*seg_it).mText;
                        run.x = (*seg_it).mXPosition - 8;
                        run.y = y_pos - y_off;
                        run.color = LLColor4((*seg_it).mColor.mV[VRED],
                                             (*seg_it).mColor.mV[VGREEN],
                                             (*seg_it).mColor.mV[VBLUE],
                                             (*seg_it).mColor.mV[VALPHA] * alpha);
                        run.style = (*line_it).mStyleFlags;
                        run.max_pixels = target_width;
                        out.runs.push_back(run);
                    }
                    y_off += line_height;
                }
            }
            y_pos += padding_vert;
        }
    }
    else
    {
        size_t lines_drawn = 0;
        for (auto paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); ++paragraph_it)
        {
            if (mSessionSupport)
            {
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }

            S32 target_height = llfloor((*paragraph_it).mLines.size() * line_height + padding_vertical);
            S32 target_width = llfloor((*paragraph_it).mMaxWidth + padding_horizontal);

            y_pos += ((*paragraph_it).mLines.size()) * line_height;
            // mBackgroundImage->drawSolid(-14, y_pos + line_height - target_height, target_width, target_height, color)
            out.bg_rects.push_back(LLRect(-14, ll_round(y_pos + line_height - target_height) + target_height,
                                          -14 + target_width,
                                          ll_round(y_pos + line_height - target_height)));

            F32 y_off = 0;
            F32 alpha;

            if ((mLinePersistTime > 0.f) && ((*paragraph_it).mAddTime < fade_time))
            {
                alpha = ((*paragraph_it).mAddTime - skip_time) / (mLinePersistTime - mFadeTime);
            }
            else
            {
                alpha = 1.0f;
            }

            if (alpha > 0.f)
            {
                for (auto line_it = (*paragraph_it).mLines.begin();
                     line_it != (*paragraph_it).mLines.end(); ++line_it)
                {
                    for (auto seg_it = (*line_it).mLineColorSegments.begin();
                         seg_it != (*line_it).mLineColorSegments.end(); ++seg_it)
                    {
                        VkTextRun run;
                        run.text = (*seg_it).mText;
                        run.x = (*seg_it).mXPosition - 8;
                        run.y = y_pos - y_off;
                        run.color = LLColor4((*seg_it).mColor.mV[VRED],
                                             (*seg_it).mColor.mV[VGREEN],
                                             (*seg_it).mColor.mV[VBLUE],
                                             (*seg_it).mColor.mV[VALPHA] * alpha);
                        run.style = (*line_it).mStyleFlags;
                        run.max_pixels = target_width;
                        out.runs.push_back(run);
                    }
                    y_off += line_height;
                }
            }
            y_pos += padding_vertical;
        }
    }
}
// </VulkanStorm>

void LLConsole::setLinePersistTime(F32 seconds)
{
    mLinePersistTime = seconds;
    mFadeTime = mLinePersistTime - FADE_DURATION;
}

void LLConsole::reshape(S32 width, S32 height, bool called_from_parent)
{
    if (mFont == nullptr)
    {
        // not initialized yet
        LL_WARNS() << "LLConsole::reshape called before font is set" << LL_ENDL;
        return;
    }

    S32 new_width = llmax(50, llmin(getRect().getWidth(), width));
    S32 new_height = llmax(mFont->getLineHeight() + 15, llmin(getRect().getHeight(), height));

    if (   mConsoleWidth == new_width
        && mConsoleHeight == new_height )
    {
        return;
    }

    mConsoleWidth = new_width;
    mConsoleHeight= new_height;

    LLUICtrl::reshape(new_width, new_height, called_from_parent);

    for(paragraph_t::iterator paragraph_it = mParagraphs.begin(); paragraph_it != mParagraphs.end(); paragraph_it++)
    {
        // <FS:Ansariel> Added styleflags parameter for style customization
        //(*paragraph_it).updateLines((F32)getRect().getWidth(), mFont, true);
        (*paragraph_it).updateLines((F32)getRect().getWidth(), mFont, (*paragraph_it).mLines.front().mStyleFlags, true);
        // </FS:Ansariel>
    }
}

void LLConsole::setFontSize(S32 size_index)
{
    if (-1 == size_index)
    {
        mFont = LLFontGL::getFontMonospace();
    }
    else if (0 == size_index)
    {
        // <FS>
        //mFont = LLFontGL::getFontSansSerif();
        mFont = LLFontGL::getFontSansSerifSmall();
        // </FS>
    }
    else if (1 == size_index)
    {
        // <FS>
        //mFont = LLFontGL::getFontSansSerifBig();
        mFont = LLFontGL::getFontSansSerif();
        // </FS>
    }
    // <FS>
    else if (2 == size_index)
    {
        mFont = LLFontGL::getFontSansSerifBig();
    }
    // </FS>
    else
    {
        mFont = LLFontGL::getFontSansSerifHuge();
    }
    // Make sure the font exists
    if (mFont == NULL)
    {
        mFont = LLFontGL::getFontDefault();
    }

    for(paragraph_t::iterator paragraph_it = mParagraphs.begin(); paragraph_it != mParagraphs.end(); paragraph_it++)
    {
        // <FSAnsariel> Added styleflags parameter for style customization
        //(*paragraph_it).updateLines((F32)getRect().getWidth(), mFont, true);
        (*paragraph_it).updateLines((F32)getRect().getWidth(), mFont, (*paragraph_it).mLines.front().mStyleFlags, true);
        // </FS:Ansariel>
    }
}

void LLConsole::draw()
{
    // Units in pixels
    // <FS>
    //static const F32 padding_horizontal = 10;
    //static const F32 padding_vertical = 3;
    constexpr F32 padding_horizontal = 15;
    constexpr F32 padding_vertical = 8;
    // </FS>
    LLGLSUIDefault gls_ui;

    // skip lines added more than mLinePersistTime ago
    F32 cur_time = mTimer.getElapsedTimeF32();

    F32 skip_time = cur_time - mLinePersistTime;
    F32 fade_time = cur_time - mFadeTime;

    if (mParagraphs.empty())    //No text to draw.
    {
        return;
    }

    // <FS:minerjr> [FIRE-35039] Add flag to show/hide the on-screen console   
    // Get the Show On-screen Console flag from the Comm menu
    static LLUICachedControl<bool> showOnscreenConsole("FSShowOnscreenConsole");
    // If the Show On-screen Console flag is disabled and the current console is the global console
    // (Not a debug console), then don't try to draw
    if (!showOnscreenConsole && this == gConsole)
    {
        return;
    }
    // </FS:minerjr> [FIRE-35039]

    // <FS:Ansariel> Session support
    if (!mSessionSupport)
    {
        // This is done in update() if session support is enabled
    // </FS:Ansariel>
    size_t num_lines{ 0 };

    paragraph_t::reverse_iterator paragraph_it;
    paragraph_it = mParagraphs.rbegin();
    auto paragraph_num=mParagraphs.size();

    while (!mParagraphs.empty() && paragraph_it != mParagraphs.rend())
    {
        num_lines += (*paragraph_it).mLines.size();
        if(num_lines > mMaxLines
            || ( (mLinePersistTime > (F32)0.f) && ((*paragraph_it).mAddTime - skip_time)/(mLinePersistTime - mFadeTime) <= (F32)0.f))
        {                           //All lines above here are done.  Lose them.
            for (size_t i = 0; i < paragraph_num; i++)
            {
                if (!mParagraphs.empty())
                    mParagraphs.pop_front();
            }
            break;
        }
        paragraph_num--;
        paragraph_it++;
    }
    // <FS:Ansariel> Session support
    }
    // </FS:Ansariel>

    if (mParagraphs.empty())
    {
        return;
    }

    // draw remaining lines
    //[FIX  FIRE-2822 SJ] Start a little bit higher with the Console not to let it blend with the stand button when Avatar is sitting
    //F32 y_pos = 0.f;
    F32 y_pos = 10.f;

    // <FS> Different draw options
    //LLUIImagePtr imagep = LLUI::getUIImage("transparent");

    //static LLCachedControl<F32> console_bg_opacity(*LLUI::getInstance()->mSettingGroups["config"], "ConsoleBackgroundOpacity", 0.7f);
    //F32 console_opacity = llclamp(console_bg_opacity(), 0.f, 1.f);

    //static LLUIColor color = LLUIColorTable::instance().getColor("ConsoleBackground");
    //color.mV[VALPHA] *= console_opacity;

    //F32 line_height = (F32)mFont->getLineHeight();

    //for(paragraph_it = mParagraphs.rbegin(); paragraph_it != mParagraphs.rend(); paragraph_it++)
    //{
    //  S32 target_height = llfloor( (*paragraph_it).mLines.size() * line_height + padding_vertical);
    //  S32 target_width =  llfloor( (*paragraph_it).mMaxWidth + padding_horizontal);

    //  y_pos += ((*paragraph_it).mLines.size()) * line_height;
    //  imagep->drawSolid(-14, (S32)(y_pos + line_height - target_height), target_width, target_height, color);

    //  F32 y_off=0;

    //  F32 alpha;

    //  if ((mLinePersistTime > 0.f) && ((*paragraph_it).mAddTime < fade_time))
    //  {
    //      alpha = ((*paragraph_it).mAddTime - skip_time)/(mLinePersistTime - mFadeTime);
    //  }
    //  else
    //  {
    //      alpha = 1.0f;
    //  }

    //  if( alpha > 0.f )
    //  {
    //      for (lines_t::iterator line_it=(*paragraph_it).mLines.begin();
    //              line_it != (*paragraph_it).mLines.end();
    //              line_it ++)
    //      {
    //          for (line_color_segments_t::iterator seg_it = (*line_it).mLineColorSegments.begin();
    //                  seg_it != (*line_it).mLineColorSegments.end();
    //                  seg_it++)
    //          {
    //              mFont->render((*seg_it).mText, 0, (*seg_it).mXPosition - 8, y_pos -  y_off,
    //                  LLColor4(
    //                      (*seg_it).mColor.mV[VRED],
    //                      (*seg_it).mColor.mV[VGREEN],
    //                      (*seg_it).mColor.mV[VBLUE],
    //                      (*seg_it).mColor.mV[VALPHA]*alpha),
    //                  LLFontGL::LEFT,
    //                  LLFontGL::BASELINE,
    //                  LLFontGL::NORMAL,
    //                  LLFontGL::DROP_SHADOW,
    //                  S32_MAX,
    //                  target_width
    //                  );
    //          }
    //          y_off += line_height;
    //      }
    //  }
    //  y_pos  += padding_vertical;
    //}

    paragraph_t::reverse_iterator paragraph_it;
    static LLUICachedControl<F32> consoleBackgroundOpacity("ConsoleBackgroundOpacity");
    static LLUIColor cbcolor = LLUIColorTable::instance().getColor("ConsoleBackground");
    LLColor4 color = cbcolor.get();
    color.mV[VALPHA] *= llclamp(consoleBackgroundOpacity(), 0.f, 1.f);

    F32 line_height = (F32)mFont->getLineHeight();
    static LLUICachedControl<bool> classic_draw_mode("FSConsoleClassicDrawMode");

    if (classic_draw_mode)
    {
        constexpr F32 padding_vert = 5.f;
        S32 total_width = 0;
        S32 total_height = 0;
        size_t lines_drawn = 0;

        paragraph_t::reverse_iterator paragraphs_end = mParagraphs.rend();
        for (paragraph_it = mParagraphs.rbegin(); paragraph_it != paragraphs_end; ++paragraph_it)
        {
            if (mSessionSupport)
            {
                // Skip paragraph if visible in floater and make sure we don't
                // exceed the maximum number of lines we want to display
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }

            total_height += llfloor( (*paragraph_it).mLines.size() * line_height + padding_vert);
            total_width = llmax(total_width, llfloor( (*paragraph_it).mMaxWidth + padding_horizontal));
        }
        mBackgroundImage->drawSolid(-14, (S32)(y_pos + line_height / 2), total_width, total_height + (S32)((line_height - padding_vert) / 2.f), color);

        lines_drawn = 0;
        for (paragraph_it = mParagraphs.rbegin(); paragraph_it != paragraphs_end; ++paragraph_it)
        {
            if (mSessionSupport)
            {
                // Skip paragraph if visible in floater and make sure we don't
                // exceed the maximum number of lines we want to display
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }

            F32 y_off=0;
            F32 alpha;
            S32 target_width =  llfloor( (*paragraph_it).mMaxWidth + padding_horizontal);
            y_pos += ((*paragraph_it).mLines.size()) * line_height;

            if ((mLinePersistTime > 0.f) && ((*paragraph_it).mAddTime < fade_time))
            {
                alpha = ((*paragraph_it).mAddTime - skip_time)/(mLinePersistTime - mFadeTime);
            }
            else
            {
                alpha = 1.0f;
            }

            if( alpha > 0.f )
            {
                LLConsole::lines_t::iterator lines_end_it = (*paragraph_it).mLines.end();
                for (lines_t::iterator line_it=(*paragraph_it).mLines.begin();
                        line_it != lines_end_it;
                        ++line_it)
                {
                    line_color_segments_t::iterator line_color_segement_end_it = (*line_it).mLineColorSegments.end();
                    for (line_color_segments_t::iterator seg_it = (*line_it).mLineColorSegments.begin();
                            seg_it != line_color_segement_end_it;
                            ++seg_it)
                    {
                        const LLFontGL* segment_font = seg_it->mFont ? seg_it->mFont : mFont;
                        segment_font->render((*seg_it).mText, 0, (*seg_it).mXPosition - 8, y_pos -  y_off,
                            LLColor4(
                                (*seg_it).mColor.mV[VRED],
                                (*seg_it).mColor.mV[VGREEN],
                                (*seg_it).mColor.mV[VBLUE],
                                (*seg_it).mColor.mV[VALPHA]*alpha),
                            LLFontGL::LEFT,
                            LLFontGL::BASELINE,
                            seg_it->mFont ? seg_it->mStyle : (*line_it).mStyleFlags,
                            LLFontGL::DROP_SHADOW,
                            S32_MAX,
                            target_width
                            );
                    }
                    y_off += line_height;
                }
            }
            y_pos  += padding_vert;
        }
    }
    else
    {
        size_t lines_drawn = 0;

        paragraph_t::reverse_iterator paragraphs_end = mParagraphs.rend();
        for(paragraph_it = mParagraphs.rbegin(); paragraph_it != paragraphs_end; paragraph_it++)
        {
            if (mSessionSupport)
            {
                // Skip paragraph if visible in floater and make sure we don't
                // exceed the maximum number of lines we want to display
                if (mCurrentSessions.find((*paragraph_it).mSessionID) != mCurrentSessions.end())
                {
                    continue;
                }
                lines_drawn += (*paragraph_it).mLines.size();
                if (lines_drawn > mMaxLines)
                {
                    break;
                }
            }

            S32 target_height = llfloor( (*paragraph_it).mLines.size() * line_height + padding_vertical);
            S32 target_width =  llfloor( (*paragraph_it).mMaxWidth + padding_horizontal);

            y_pos += ((*paragraph_it).mLines.size()) * line_height;
            mBackgroundImage->drawSolid(-14, (S32)(y_pos + line_height - target_height), target_width, target_height, color);

            F32 y_off=0;

            F32 alpha;

            if ((mLinePersistTime > 0.f) && ((*paragraph_it).mAddTime < fade_time))
            {
                alpha = ((*paragraph_it).mAddTime - skip_time)/(mLinePersistTime - mFadeTime);
            }
            else
            {
                alpha = 1.0f;
            }

            if( alpha > 0.f )
            {
                LLConsole::lines_t::iterator lines_end_it = (*paragraph_it).mLines.end();
                for (lines_t::iterator line_it=(*paragraph_it).mLines.begin();
                        line_it != lines_end_it;
                        ++line_it)
                {
                    line_color_segments_t::iterator line_color_segement_end_it = (*line_it).mLineColorSegments.end();
                    for (line_color_segments_t::iterator seg_it = (*line_it).mLineColorSegments.begin();
                            seg_it != line_color_segement_end_it;
                            ++seg_it)
                    {
                        const LLFontGL* segment_font = seg_it->mFont ? seg_it->mFont : mFont;
                        segment_font->render((*seg_it).mText, 0, (*seg_it).mXPosition - 8, y_pos -  y_off,
                            LLColor4(
                                (*seg_it).mColor.mV[VRED],
                                (*seg_it).mColor.mV[VGREEN],
                                (*seg_it).mColor.mV[VBLUE],
                                (*seg_it).mColor.mV[VALPHA]*alpha),
                            LLFontGL::LEFT,
                            LLFontGL::BASELINE,
                            seg_it->mFont ? seg_it->mStyle : (*line_it).mStyleFlags,
                            LLFontGL::DROP_SHADOW,
                            S32_MAX,
                            target_width
                            );
                    }
                    y_off += line_height;
                }
            }
            y_pos  += padding_vertical;
        }
    }
    // </FS>
}

// <FS:Ansariel> Chat console
void LLConsole::addConsoleLine(const std::string& utf8line, const LLColor4 &color, const LLUUID& session_id, LLFontGL::StyleFlags styleflags, S32 markdown_offset, bool markdown_emote)
{
    LLWString wline = utf8str_to_wstring(utf8line);
    addConsoleLine(wline, color, session_id, styleflags, markdown_offset, markdown_emote);
}

void LLConsole::addConsoleLine(const LLWString& wline, const LLColor4 &color, const LLUUID& session_id, LLFontGL::StyleFlags styleflags, S32 markdown_offset, bool markdown_emote)
{
    if (wline.empty())
    {
        return;
    }

    if (!mSessionSupport)
    {
        removeExtraLines();
    }

    mMutex.lock();
    mLines.push_back(wline);
    mLineLengths.push_back((S32)wline.length());
    mAddTimes.push_back(mTimer.getElapsedTimeF32());
    mLineColors.push_back(color);
    mLineStyle.push_back(styleflags);
    mSessionIDs.push_back(session_id);
    mMarkdownMessages.emplace_back(markdown_offset, markdown_emote);
    mMutex.unlock();
}

void LLConsole::clear()
{
    LL_INFOS() << "Clearing Console..." << LL_ENDL;
    mMutex.lock();
    mLines.clear();
    mAddTimes.clear();
    mLineLengths.clear();
    mLineColors.clear();
    mLineStyle.clear();
    mSessionIDs.clear();
    mMarkdownMessages.clear();
    mMutex.unlock();

    mTimer.reset();
}

void LLConsole::removeExtraLines()
{
    mMutex.lock();
    while ((S32)mLines.size() > llmax((S32)0, (S32)(mMaxLines - 1)))
    {
        mLines.pop_front();
        mAddTimes.pop_front();
        mLineLengths.pop_front();
        if (!mLineColors.empty())
        {
            mLineColors.pop_front();
        }
        if (!mLineStyle.empty())
        {
            mLineStyle.pop_front();
        }
        if (!mSessionIDs.empty())
        {
            mSessionIDs.pop_front();
        }
        if (!mMarkdownMessages.empty())
        {
            mMarkdownMessages.pop_front();
        }
    }
    mMutex.unlock();
}
// </FS:Ansariel> Chat console

//Generate highlight color segments for this paragraph.  Pass in default color of paragraph.
void LLConsole::Paragraph::makeParagraphColorSegments (const LLColor4 &color)
{
    mParagraphColorSegments.clear();
    mCharacterStyles.clear();
    if (mMarkdownOffset >= 0 && static_cast<size_t>(mMarkdownOffset) <= mParagraphText.size())
    {
        const std::string body = wstring_to_utf8str(mParagraphText.substr(mMarkdownOffset));
        mParagraphText.resize(mMarkdownOffset);
        mCharacterStyles.assign(mMarkdownOffset, LLFontGL::NORMAL);
        U8 emote_style = mBaseStyle;
        LLMarkdown::literal_ranges_t literal_ranges;
        std::string remaining = body;
        size_t offset = 0;
        LLUrlMatch match;
        while (LLUrlRegistry::instance().findUrl(remaining, match))
        {
            const size_t end = match.getEnd() + 1;
            if (end == 0 || end > remaining.size()) break;
            literal_ranges.emplace_back(offset + match.getStart(), offset + end);
            offset += end;
            remaining.erase(0, end);
        }
        for (const auto& entry : mUrlLabels)
        {
            if (entry.second.empty()) continue;
            size_t start = body.find(entry.second);
            while (start != std::string::npos)
            {
                literal_ranges.emplace_back(start, start + entry.second.size());
                start = body.find(entry.second, start + entry.second.size());
            }
        }
        for (const auto& span : LLMarkdown::parseEmphasis(body, mMarkdownEmote, literal_ranges))
        {
            U8 style = mMarkdownEmote ? emote_style : mBaseStyle;
            switch (span.mType)
            {
                case LLMarkdown::ESpanType::EMPHASIS_DELIM:
                case LLMarkdown::ESpanType::STRONG_DELIM:
                    continue;
                case LLMarkdown::ESpanType::EMOTE_DELIM:
                    emote_style ^= LLFontGL::ITALIC;
                    continue;
                case LLMarkdown::ESpanType::EMPHASIS:
                    style |= LLFontGL::ITALIC;
                    break;
                case LLMarkdown::ESpanType::STRONG:
                    style |= LLFontGL::BOLD;
                    break;
                case LLMarkdown::ESpanType::EMOTE_TOGGLE_ON:
                    style |= LLFontGL::ITALIC;
                    emote_style = style;
                    break;
                case LLMarkdown::ESpanType::EMOTE_TOGGLE_OFF:
                    style &= ~LLFontGL::ITALIC;
                    emote_style = style;
                    break;
                default:
                    break;
            }
            const LLWString text = utf8str_to_wstring(span.mText);
            mParagraphText += text;
            mCharacterStyles.insert(mCharacterStyles.end(), text.size(), style);
        }
    }
    LLSD paragraph_color_segments;
    paragraph_color_segments[0]["text"] =wstring_to_utf8str(mParagraphText);
    LLSD color_sd = color.getValue();
    paragraph_color_segments[0]["color"]=color_sd;

    for(LLSD::array_const_iterator color_segment_it = paragraph_color_segments.beginArray();
        color_segment_it != paragraph_color_segments.endArray();
        ++color_segment_it)
    {
        LLSD color_llsd = (*color_segment_it)["color"];
        std::string color_str  = (*color_segment_it)["text"].asString();

        ParagraphColorSegment color_segment;

        color_segment.mColor.setValue(color_llsd);
        color_segment.mNumChars = static_cast<S32>(color_str.length());

        mParagraphColorSegments.push_back(color_segment);
    }
}

//Called when a paragraph is added to the console or window is resized.
// <FS:Ansariel> Added styleflags parameter for style customization
//void LLConsole::Paragraph::updateLines(F32 screen_width, const LLFontGL* font, bool force_resize)
void LLConsole::Paragraph::updateLines(F32 screen_width, const LLFontGL* font, LLFontGL::StyleFlags styleflags, bool force_resize)
// </FS:Ansariel>
{
    if ( !force_resize )
    {
        if ( mMaxWidth >= 0.0f
         &&  mMaxWidth < screen_width)
        {
            return;                 //No resize required.
        }
    }

    screen_width = screen_width - 30;   //Margin for small windows.

    if (mParagraphText.empty())
    {
        mLines.clear();
        Line line;
        line.mStyleFlags = mBaseStyle;
        mLines.push_back(line);
        mMaxWidth = 0.f;
        return;
    }
    if (mParagraphColorSegments.empty()
        || font == NULL)
    {
        return;                 //Not enough info to complete.
    }

    mLines.clear();             //Chuck everything.
    mMaxWidth = 0.0f;

    if (!mCharacterStyles.empty())
    {
        std::map<U8, const LLFontGL*> fonts;
        auto styled_font = [&](U8 style)
        {
            auto found = fonts.find(style);
            if (found != fonts.end()) return found->second;
            LLFontDescriptor descriptor = font->getFontDesc();
            descriptor.setStyle(style);
            const LLFontGL* selected = LLFontGL::getFont(descriptor);
            if (!selected) selected = font;
            fonts.emplace(style, selected);
            return selected;
        };
        const S32 length = static_cast<S32>(mParagraphText.size());
        S32 start = 0;
        while (start < length)
        {
            S32 end = start;
            F32 width = 0.f;
            while (end < length && mParagraphText[end] != '\n')
            {
                const U8 style = mCharacterStyles[end];
                S32 run_end = end + 1;
                while (run_end < length && mParagraphText[run_end] != '\n' && mCharacterStyles[run_end] == style)
                    ++run_end;
                const LLFontGL* selected = styled_font(style);
                const S32 fit = selected->maxDrawableChars(mParagraphText.c_str() + end,
                    llmax(0.f, screen_width - width), run_end - end, LLFontGL::ANYWHERE);
                width += selected->getWidthF32(mParagraphText.c_str(), end, fit);
                end += fit;
                if (end != run_end) break;
            }
            if (end < length && mParagraphText[end] != '\n')
            {
                S32 boundary = end;
                while (boundary > start && !LLStringOps::isSpace(mParagraphText[boundary - 1])) --boundary;
                if (boundary > start) end = boundary;
                else if (end == start) ++end;
            }
            Line line;
            line.mStyleFlags = mBaseStyle;
            F32 position = 0.f;
            for (S32 offset = start; offset < end;)
            {
                const U8 style = mCharacterStyles[offset];
                S32 run_end = offset + 1;
                while (run_end < end && mCharacterStyles[run_end] == style) ++run_end;
                const LLFontGL* selected = styled_font(style);
                LineColorSegment segment(mParagraphText.substr(offset, run_end - offset),
                    mParagraphColorSegments.front().mColor, position);
                segment.mStyle = static_cast<LLFontGL::StyleFlags>(style);
                segment.mFont = selected;
                position += selected->getWidthF32(segment.mText.c_str());
                line.mLineColorSegments.push_back(segment);
                offset = run_end;
            }
            mMaxWidth = llmax(mMaxWidth, position);
            mLines.push_back(line);
            start = end + (end < length && mParagraphText[end] == '\n' ? 1 : 0);
        }
        return;
    }

    paragraph_color_segments_t::iterator current_color = mParagraphColorSegments.begin();
    U32 current_color_length = (*current_color).mNumChars;

    S32 paragraph_offset = 0;           //Offset into the paragraph text.

    // Wrap lines that are longer than the view is wide.
    while( paragraph_offset < (S32)mParagraphText.length() &&
           mParagraphText[paragraph_offset] != 0)
    {
        // <FS> FIRE-8257: Sometimes text is cut off on left side of console
        //S32 skip_chars; // skip '\n'
        // Figure out if a word-wrapped line fits here.
        LLWString::size_type line_end = mParagraphText.find_first_of(llwchar('\n'), paragraph_offset);
        // <FS> FIRE-8257: Sometimes text is cut off on left side of console
        //if (line_end != LLWString::npos)
        //{
        //  skip_chars = 1; // skip '\n'
        //}
        //else
        //{
        //  line_end = mParagraphText.size();
        //  skip_chars = 0;
        //}

        //U32 drawable = font->maxDrawableChars(mParagraphText.c_str()+paragraph_offset, screen_width, static_cast<S32>(line_end) - paragraph_offset, LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);
        if (line_end == LLWString::npos)
        {
            line_end = mParagraphText.size();
        }

        S32 skip_chars = 0; // skip '\n'
        U32 line_length = static_cast<U32>(line_end) - paragraph_offset;
        U32 drawable = font->maxDrawableChars(mParagraphText.c_str()+paragraph_offset, screen_width, line_length, LLFontGL::WORD_BOUNDARY_IF_POSSIBLE);
        // </FS>

        if (drawable != 0)
        {
            // <FS> FIRE-8257: Sometimes text is cut off on left side of console
            if (drawable >= line_length)
            {
                skip_chars = 1;
            }
            // </FS>
            F32 x_position = 0;                     //Screen X position of text.

            mMaxWidth = llmax( mMaxWidth, (F32)font->getWidth( mParagraphText.substr( paragraph_offset, drawable ).c_str() ) );
            Line line;
            line.mStyleFlags = styleflags; // <FS:Ansariel> Add styleflags to every new line

            U32 left_to_draw = drawable;
            U32 drawn = 0;

            while (left_to_draw >= current_color_length
                && current_color != mParagraphColorSegments.end() )
            {
                LLWString color_text = mParagraphText.substr( paragraph_offset + drawn, current_color_length );
                line.mLineColorSegments.push_back( LineColorSegment( color_text,            //Append segment to line.
                                                (*current_color).mColor,
                                                x_position ) );

                x_position += font->getWidth( color_text.c_str() ); //Set up next screen position.

                drawn += current_color_length;
                left_to_draw -= current_color_length;

                current_color++;                            //Goto next paragraph color record.

                if (current_color != mParagraphColorSegments.end())
                {
                    current_color_length = (*current_color).mNumChars;
                }
            }

            if (left_to_draw > 0 && current_color != mParagraphColorSegments.end() )
            {
                    LLWString color_text = mParagraphText.substr( paragraph_offset + drawn, left_to_draw );

                    line.mLineColorSegments.push_back( LineColorSegment( color_text,        //Append segment to line.
                                                    (*current_color).mColor,
                                                    x_position ) );

                    current_color_length -= left_to_draw;
            }
            mLines.push_back(line);                             //Append line to paragraph line list.
        }
        // <FS> FIRE-8257: Sometimes text is cut off on left side of console
        else
        {
            mLines.push_back(Line());
            skip_chars = 1;
        }
        // </FS>
        paragraph_offset += (drawable + skip_chars);
    }
}

//Pass in the string and the default color for this block of text.
// <FS:Ansariel> Added styleflags parameter for style customization and session support
//LLConsole::Paragraph::Paragraph (LLWString str, const LLColor4 &color, F32 add_time, const LLFontGL* font, F32 screen_width)
//: mParagraphText(str), mAddTime(add_time), mMaxWidth(-1)
LLConsole::Paragraph::Paragraph (LLWString str, const LLColor4 &color, F32 add_time, const LLFontGL* font, F32 screen_width, LLFontGL::StyleFlags styleflags, const LLUUID& session_id, bool parse_urls, LLConsole* console, S32 markdown_offset, bool markdown_emote)
:   mParagraphText(str), mAddTime(add_time), mMaxWidth(-1), mSessionID(session_id)
// </FS:Ansariel>
{
    // <FS:Ansariel> Parse SLURLs
    mSourceText = str;
    mMarkdownOffset = markdown_offset;
    mMarkdownEmote = markdown_emote;
    mBaseStyle = styleflags;
    mID.generate();

    if (parse_urls)
    {
        LLUrlMatch urlMatch;
        LLWString workLine = str;

        while (LLUrlRegistry::instance().findUrl(workLine, urlMatch, boost::bind(&LLConsole::onUrlLabelCallback, console, mID, _1, _2)) && !urlMatch.getUrl().empty())
        {
            // Special case for LLUrlEntryHTTP, LLUrlEntryHTTPNoProtocol and
            // LLUrlEntrySecondlifeURL: getLabel() only returns host part, but
            // we also want the query part
            std::string label = urlMatch.getLabel();
            std::string query = urlMatch.getQuery();
            if (!query.empty())
            {
                label += query;
            }

            // This works for LLUrlEntryHTTPLabel and LLUrlEntrySLLabel because
            // there is no callback involved for which we are storing the
            // matched Url
            LLWStringUtil::replaceString(mParagraphText, utf8str_to_wstring(urlMatch.getMatchedText()), utf8str_to_wstring(label));
            mUrlLabels[urlMatch.getUrl()] = label;

            // Remove the URL from the work line so we don't end in a loop in case of regular URLs!
            // findUrl will always return the very first URL in a string
            workLine = workLine.erase(0, urlMatch.getEnd() + 1);
        }
    }
    // </FS:Ansariel>

    makeParagraphColorSegments(color);
    // <FS:Ansariel> Added styleflags parameter for style customization
    //updateLines( screen_width, font );
    updateLines( screen_width, font, styleflags );
    // </FS:Ansariel>
}

// <FS:Ansariel> Parse SLURLs
void LLConsole::onUrlLabelCallback(const LLUUID& paragraph_id, const std::string& url, const std::string& label)
{
    for (paragraph_t::reverse_iterator it = mParagraphs.rbegin(); it != mParagraphs.rend(); ++it)
    {
        LLConsole::Paragraph& paragraph = *it;

        if (paragraph.mID == paragraph_id)
        {
            paragraph.mUrlLabels[url] = label;

            LLWString newText = paragraph.mSourceText;
            for (std::map<std::string, std::string>::iterator url_it = paragraph.mUrlLabels.begin(); url_it != paragraph.mUrlLabels.end(); ++url_it)
            {
                LLWStringUtil::replaceString(newText, utf8string_to_wstring(url_it->first), utf8string_to_wstring(url_it->second));
            }
            paragraph.mParagraphText = newText;

            const LLColor4 color = paragraph.mParagraphColorSegments.front().mColor;
            paragraph.makeParagraphColorSegments(color);
            paragraph.updateLines((F32)getRect().getWidth(), mFont, paragraph.mLines.front().mStyleFlags, true);

            break;
        }
    }
}
// </FS:Ansariel>

// called once per frame regardless of console visibility
// static
void LLConsole::updateClass()
{
    for (auto& con : instance_snapshot())
    {
        con.update();
    }
}

static LLTrace::BlockTimerStatHandle FTM_CONSOLE_UPDATE_PARAGRAPHS("Update Console Paragraphs");
void LLConsole::update()
{
    {
        LLCoros::LockType lock(mMutex);

        while (!mLines.empty())
        {
            // <FS:Ansariel> Chat console
            //mParagraphs.push_back(
            //  Paragraph(  mLines.front(),
            //              LLColor4::white,
            //              mTimer.getElapsedTimeF32(),
            //              mFont,
            //              (F32)getRect().getWidth()));
            //mLines.pop_front();

            mParagraphs.push_back(
                Paragraph(  mLines.front(),
                            (!mLineColors.empty() ? mLineColors.front() : LLColor4::white),
                            mTimer.getElapsedTimeF32(),
                            mFont,
                            (F32)getRect().getWidth(),
                            (!mLineStyle.empty() ? mLineStyle.front() : LLFontGL::NORMAL),
                            (!mSessionIDs.empty() ? mSessionIDs.front(): LLUUID::null),
                            mParseUrls,
                            this,
                            mMarkdownMessages.empty() ? -1 : mMarkdownMessages.front().first,
                            !mMarkdownMessages.empty() && mMarkdownMessages.front().second));
            mLines.pop_front();
            if (!mLineColors.empty())
                mLineColors.pop_front();
            if (!mLineStyle.empty())
                mLineStyle.pop_front();
            if (!mSessionIDs.empty())
                mSessionIDs.pop_front();
            if (!mMarkdownMessages.empty())
                mMarkdownMessages.pop_front();
            // </FS:Ansariel>
        }
    }

    // remove old paragraphs which can't possibly be visible any more.  ::draw() will do something similar but more conservative - we do this here because ::draw() isn't guaranteed to ever be called!  (i.e. the console isn't visible)
    // <FS:Ansariel> Session support
    if (!mSessionSupport)
    {
    // </FS:Ansariel>
    while ((S32)mParagraphs.size() > llmax((S32)0, (S32)(mMaxLines)))
    {
            mParagraphs.pop_front();
    }
    // <FS:Ansariel> Session support
    }
    else
    {
        LL_RECORD_BLOCK_TIME(FTM_CONSOLE_UPDATE_PARAGRAPHS);

        // skip lines added more than mLinePersistTime ago
        F32 skip_time = mTimer.getElapsedTimeF32() - mLinePersistTime;

        paragraph_t temp_para;
        std::map<LLUUID, U32> session_map;
        for (paragraph_t::reverse_iterator it = mParagraphs.rbegin(); it != mParagraphs.rend(); ++it)
        {
            Paragraph& para = *it;
            session_map[para.mSessionID] += (U32)para.mLines.size();
            if (session_map[para.mSessionID] <= mMaxLines && // max lines on a per session basis
                !((mLinePersistTime > 0.f) && (para.mAddTime - skip_time) / (mLinePersistTime - mFadeTime) <= 0.f)) // not expired yet
            {
                temp_para.push_front(para);
            }
        }
        mParagraphs.swap(temp_para);
    }
    // </FS:Ansariel>
}

// <FS:Ansariel> Session support
void LLConsole::addSession(const LLUUID& session_id)
{
    mCurrentSessions.insert(session_id);
}

void LLConsole::removeSession(const LLUUID& session_id)
{
    mCurrentSessions.erase(session_id);
}
// </FS:Ansariel>

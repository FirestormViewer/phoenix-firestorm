/** 
 * @file llkeyboardsdl.cpp
 * @brief Handler for assignable key bindings
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

#if LL_SDL

#include "linden_common.h"
#include "llkeyboardsdl.h"
#include "llwindowcallbacks.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_keycode.h"

LLKeyboardSDL::LLKeyboardSDL()
{
	// Set up key mapping for SDL - eventually can read this from a file?
	// Anything not in the key map gets dropped
	// Add default A-Z

	// Virtual key mappings from SDL_keysym.h ...

	// SDL maps the letter keys to the ASCII you'd expect, but it's lowercase...

	// <FS:ND> Those are handled by SDL2 via text input, do not map them
	
	// U16 cur_char;
	// for (cur_char = 'A'; cur_char <= 'Z'; cur_char++)
	// {
	// 	mTranslateKeyMap[cur_char] = cur_char;
	// }
	// for (cur_char = 'a'; cur_char <= 'z'; cur_char++)
	// {
	// 	mTranslateKeyMap[cur_char] = (cur_char - 'a') + 'A';
	// }
	// 
	// for (cur_char = '0'; cur_char <= '9'; cur_char++)
	// {
	// 	mTranslateKeyMap[cur_char] = cur_char;
	// }

	// </FS:ND

	// These ones are translated manually upon keydown/keyup because
	// SDL doesn't handle their numlock transition.
	//mTranslateKeyMap[SDLK_KP4] = KEY_PAD_LEFT;
	//mTranslateKeyMap[SDLK_KP6] = KEY_PAD_RIGHT;
	//mTranslateKeyMap[SDLK_KP8] = KEY_PAD_UP;
	//mTranslateKeyMap[SDLK_KP2] = KEY_PAD_DOWN;
	//mTranslateKeyMap[SDLK_KP_PERIOD] = KEY_DELETE;
	//mTranslateKeyMap[SDLK_KP7] = KEY_HOME;
	//mTranslateKeyMap[SDLK_KP1] = KEY_END;
	//mTranslateKeyMap[SDLK_KP9] = KEY_PAGE_UP;
	//mTranslateKeyMap[SDLK_KP3] = KEY_PAGE_DOWN;
	//mTranslateKeyMap[SDLK_KP0] = KEY_INSERT;

	// mTranslateKeyMap[SDLK_SPACE] = ' '; 	// <FS:ND/> Those are handled by SDL2 via text input, do not map them
	mTranslateKeyMap[SDLK_RETURN] = KEY_RETURN;
	mTranslateKeyMap[SDLK_LEFT] = KEY_LEFT;
	mTranslateKeyMap[SDLK_RIGHT] = KEY_RIGHT;
	mTranslateKeyMap[SDLK_UP] = KEY_UP;
	mTranslateKeyMap[SDLK_DOWN] = KEY_DOWN;
	mTranslateKeyMap[SDLK_ESCAPE] = KEY_ESCAPE;
	mTranslateKeyMap[SDLK_KP_ENTER] = KEY_RETURN;
	mTranslateKeyMap[SDLK_ESCAPE] = KEY_ESCAPE;
	mTranslateKeyMap[SDLK_BACKSPACE] = KEY_BACKSPACE;
	mTranslateKeyMap[SDLK_DELETE] = KEY_DELETE;
	mTranslateKeyMap[SDLK_LSHIFT] = KEY_SHIFT;
	mTranslateKeyMap[SDLK_RSHIFT] = KEY_SHIFT;
	mTranslateKeyMap[SDLK_LCTRL] = KEY_CONTROL;
	mTranslateKeyMap[SDLK_RCTRL] = KEY_CONTROL;
	mTranslateKeyMap[SDLK_LALT] = KEY_ALT;
	mTranslateKeyMap[SDLK_RALT] = KEY_ALT;
	mTranslateKeyMap[SDLK_HOME] = KEY_HOME;
	mTranslateKeyMap[SDLK_END] = KEY_END;
	mTranslateKeyMap[SDLK_PAGEUP] = KEY_PAGE_UP;
	mTranslateKeyMap[SDLK_PAGEDOWN] = KEY_PAGE_DOWN;
	mTranslateKeyMap[SDLK_MINUS] = KEY_HYPHEN;
	mTranslateKeyMap[SDLK_EQUALS] = KEY_EQUALS;
	mTranslateKeyMap[SDLK_KP_EQUALS] = KEY_EQUALS;
	mTranslateKeyMap[SDLK_INSERT] = KEY_INSERT;
	mTranslateKeyMap[SDLK_CAPSLOCK] = KEY_CAPSLOCK;
	mTranslateKeyMap[SDLK_TAB] = KEY_TAB;
	mTranslateKeyMap[SDLK_KP_PLUS] = KEY_ADD;
	mTranslateKeyMap[SDLK_KP_MINUS] = KEY_SUBTRACT;
	mTranslateKeyMap[SDLK_KP_MULTIPLY] = KEY_MULTIPLY;
	mTranslateKeyMap[SDLK_KP_DIVIDE] = KEY_PAD_DIVIDE;
	mTranslateKeyMap[SDLK_F1] = KEY_F1;
	mTranslateKeyMap[SDLK_F2] = KEY_F2;
	mTranslateKeyMap[SDLK_F3] = KEY_F3;
	mTranslateKeyMap[SDLK_F4] = KEY_F4;
	mTranslateKeyMap[SDLK_F5] = KEY_F5;
	mTranslateKeyMap[SDLK_F6] = KEY_F6;
	mTranslateKeyMap[SDLK_F7] = KEY_F7;
	mTranslateKeyMap[SDLK_F8] = KEY_F8;
	mTranslateKeyMap[SDLK_F9] = KEY_F9;
	mTranslateKeyMap[SDLK_F10] = KEY_F10;
	mTranslateKeyMap[SDLK_F11] = KEY_F11;
	mTranslateKeyMap[SDLK_F12] = KEY_F12;
	// mTranslateKeyMap[SDLK_PLUS]   = '='; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_COMMA]  = ','; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_MINUS]  = '-'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_PERIOD] = '.'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_BACKQUOTE] = '`'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_SLASH] = KEY_DIVIDE; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_SEMICOLON] = ';'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_LEFTBRACKET] = '['; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_BACKSLASH] = '\\'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_RIGHTBRACKET] = ']'; // <FS:ND/> Those are handled by SDL2 via text input, do not map them
	// mTranslateKeyMap[SDLK_QUOTE] = '\''; // <FS:ND/> Those are handled by SDL2 via text input, do not map them

	// Build inverse map
	for (auto iter = mTranslateKeyMap.begin(); iter != mTranslateKeyMap.end(); iter++)
	{
		mInvTranslateKeyMap[iter->second] = iter->first;
	}

	// numpad map
	mTranslateNumpadMap[SDLK_KP_0] = KEY_PAD_INS;
	mTranslateNumpadMap[SDLK_KP_1] = KEY_PAD_END;
	mTranslateNumpadMap[SDLK_KP_2] = KEY_PAD_DOWN;
	mTranslateNumpadMap[SDLK_KP_3] = KEY_PAD_PGDN;
	mTranslateNumpadMap[SDLK_KP_4] = KEY_PAD_LEFT;
	mTranslateNumpadMap[SDLK_KP_5] = KEY_PAD_CENTER;
	mTranslateNumpadMap[SDLK_KP_6] = KEY_PAD_RIGHT;
	mTranslateNumpadMap[SDLK_KP_7] = KEY_PAD_HOME;
	mTranslateNumpadMap[SDLK_KP_8] = KEY_PAD_UP;
	mTranslateNumpadMap[SDLK_KP_9] = KEY_PAD_PGUP;
	mTranslateNumpadMap[SDLK_KP_PERIOD] = KEY_PAD_DEL;

	// build inverse numpad map
	for (auto iter = mTranslateNumpadMap.begin();
	     iter != mTranslateNumpadMap.end();
	     iter++)
	{
		mInvTranslateNumpadMap[iter->second] = iter->first;
	}
}

void LLKeyboardSDL::resetMaskKeys()
{
	SDL_Keymod mask = SDL_GetModState();

	// MBW -- XXX -- This mirrors the operation of the Windows version of resetMaskKeys().
	//    It looks a bit suspicious, as it won't correct for keys that have been released.
	//    Is this the way it's supposed to work?

	if(mask & KMOD_SHIFT)
	{
		mKeyLevel[KEY_SHIFT] = TRUE;
	}

	if(mask & KMOD_CTRL)
	{
		mKeyLevel[KEY_CONTROL] = TRUE;
	}

	if(mask & KMOD_ALT)
	{
		mKeyLevel[KEY_ALT] = TRUE;
	}
}


MASK LLKeyboardSDL::updateModifiers(const U32 mask)
{
	// translate the mask
	MASK out_mask = MASK_NONE;

	if(mask & KMOD_SHIFT)
	{
		out_mask |= MASK_SHIFT;
	}

	if(mask & KMOD_CTRL)
	{
		out_mask |= MASK_CONTROL;
	}

	if(mask & KMOD_ALT)
	{
		out_mask |= MASK_ALT;
	}

	return out_mask;
}


static U32 adjustNativekeyFromUnhandledMask(const U32 key, const U32 mask)
{
	// SDL doesn't automatically adjust the keysym according to
	// whether NUMLOCK is engaged, so we massage the keysym manually.
	U32 rtn = key;
	if (!(mask & KMOD_NUM))
	{
		switch (key)
		{
			case SDLK_KP_PERIOD: rtn = SDLK_DELETE; break;
			case SDLK_KP_0: rtn = SDLK_INSERT; break;
			case SDLK_KP_1: rtn = SDLK_END; break;
			case SDLK_KP_2: rtn = SDLK_DOWN; break;
			case SDLK_KP_3: rtn = SDLK_PAGEDOWN; break;
			case SDLK_KP_4: rtn = SDLK_LEFT; break;
			case SDLK_KP_6: rtn = SDLK_RIGHT; break;
			case SDLK_KP_7: rtn = SDLK_HOME; break;
			case SDLK_KP_8: rtn = SDLK_UP; break;
			case SDLK_KP_9: rtn = SDLK_PAGEUP; break;
		}
	}
	return rtn;
}


BOOL LLKeyboardSDL::handleKeyDown(const U32 key, const U32 mask)
{
	U32 adjusted_nativekey;
	KEY	translated_key = 0;
	U32	translated_mask = MASK_NONE;
	BOOL handled = FALSE;

	adjusted_nativekey = adjustNativekeyFromUnhandledMask(key, mask);

	translated_mask = updateModifiers(mask);
	
	if(translateNumpadKey(adjusted_nativekey, &translated_key))
	{
		handled = handleTranslatedKeyDown(translated_key, translated_mask);
	}

	return handled;
}


BOOL LLKeyboardSDL::handleKeyUp(const U32 key, const U32 mask)
{
	U32 adjusted_nativekey;
	KEY	translated_key = 0;
	U32	translated_mask = MASK_NONE;
	BOOL handled = FALSE;

	adjusted_nativekey = adjustNativekeyFromUnhandledMask(key, mask);

	translated_mask = updateModifiers(mask);

	if(translateNumpadKey(adjusted_nativekey, &translated_key))
	{
		handled = handleTranslatedKeyUp(translated_key, translated_mask);
	}

	return handled;
}

MASK LLKeyboardSDL::currentMask(BOOL for_mouse_event)
{
	MASK result = MASK_NONE;
	SDL_Keymod mask = SDL_GetModState();

	if (mask & KMOD_SHIFT)			result |= MASK_SHIFT;
	if (mask & KMOD_CTRL)			result |= MASK_CONTROL;
	if (mask & KMOD_ALT)			result |= MASK_ALT;

	// For keyboard events, consider Meta keys equivalent to Control
	if (!for_mouse_event)
	{
		if (mask & KMOD_ALT) result |= MASK_CONTROL;
	}

	return result;
}

void LLKeyboardSDL::scanKeyboard()
{
	for (S32 key = 0; key < KEY_COUNT; key++)
	{
		// Generate callback if any event has occurred on this key this frame.
		// Can't just test mKeyLevel, because this could be a slow frame and
		// key might have gone down then up. JC
		if (mKeyLevel[key] || mKeyDown[key] || mKeyUp[key])
		{
			mCurScanKey = key;
			mCallbacks->handleScanKey(key, mKeyDown[key], mKeyUp[key], mKeyLevel[key]);
		}
	}

	// Reset edges for next frame
	for (S32 key = 0; key < KEY_COUNT; key++)
	{
		mKeyUp[key] = FALSE;
		mKeyDown[key] = FALSE;
		if (mKeyLevel[key])
		{
			mKeyLevelFrameCount[key]++;
		}
	}
}

 
BOOL LLKeyboardSDL::translateNumpadKey( const U32 os_key, KEY *translated_key)
{
	return translateKey(os_key, translated_key);	
}

U16 LLKeyboardSDL::inverseTranslateNumpadKey(const KEY translated_key)
{
	return inverseTranslateKey(translated_key);
}

// <FS:ND> Compatibility shim for SDL2 > SDL1.
// The dullahan plugin still expects SDL1 codes. Temporarily map those SDL keyes till the plugin is updated
enum class SDL1Keys: U32 {
	SDLK_UNKNOWN	= 0,
	SDLK_BACKSPACE	= 8,
	SDLK_TAB		= 9,
	SDLK_CLEAR		= 12,
	SDLK_RETURN		= 13,
	SDLK_PAUSE		= 19,
	SDLK_ESCAPE		= 27,
	SDLK_DELETE		= 127,
	SDLK_KP_PERIOD	= 266,
	SDLK_KP_DIVIDE	= 267,
	SDLK_KP_MULTIPLY= 268,
	SDLK_KP_MINUS	= 269,
	SDLK_KP_PLUS	= 270,
	SDLK_KP_ENTER	= 271,
	SDLK_KP_EQUALS	= 272,
	SDLK_UP			= 273,
	SDLK_DOWN		= 274,
	SDLK_RIGHT		= 275,
	SDLK_LEFT		= 276,
	SDLK_INSERT		= 277,
	SDLK_HOME		= 278,
	SDLK_END		= 279,
	SDLK_PAGEUP		= 280,
	SDLK_PAGEDOWN	= 281,
	SDLK_F1			= 282,
	SDLK_F2			= 283,
	SDLK_F3			= 284,
	SDLK_F4			= 285,
	SDLK_F5			= 286,
	SDLK_F6			= 287,
	SDLK_F7			= 288,
	SDLK_F8			= 289,
	SDLK_F9			= 290,
	SDLK_F10		= 291,
	SDLK_F11		= 292,
	SDLK_F12		= 293,
	SDLK_F13		= 294,
	SDLK_F14		= 295,
	SDLK_F15		= 296,
	SDLK_CAPSLOCK	= 301,
	SDLK_RSHIFT		= 303,
	SDLK_LSHIFT		= 304,
	SDLK_RCTRL		= 305,
	SDLK_LCTRL		= 306,
	SDLK_RALT		= 307,
	SDLK_LALT		= 308,
	SDLK_MODE		= 313,		/**< "Alt Gr" key */
	SDLK_HELP		= 315,
	SDLK_SYSREQ		= 317,
	SDLK_MENU		= 319,
	SDLK_POWER		= 320,		/**< Power Macintosh power key */
	SDLK_UNDO		= 322,		/**< Atari keyboard has Undo */
};

std::map< U32, U32 > mSDL2_to_SDL1;

U32 LLKeyboardSDL::mapSDL2toSDL1( U32 aSymbol )
{
	if( mSDL2_to_SDL1.empty() )
	{
		mSDL2_to_SDL1[ SDLK_UNKNOWN    ] = (U32)SDL1Keys::SDLK_UNKNOWN;
		mSDL2_to_SDL1[ SDLK_BACKSPACE  ] = (U32)SDL1Keys::SDLK_BACKSPACE;
		mSDL2_to_SDL1[ SDLK_TAB        ] = (U32)SDL1Keys::SDLK_TAB;
		mSDL2_to_SDL1[ SDLK_CLEAR      ] = (U32)SDL1Keys::SDLK_CLEAR;
		mSDL2_to_SDL1[ SDLK_RETURN     ] = (U32)SDL1Keys::SDLK_RETURN;
		mSDL2_to_SDL1[ SDLK_PAUSE      ] = (U32)SDL1Keys::SDLK_PAUSE;
		mSDL2_to_SDL1[ SDLK_ESCAPE     ] = (U32)SDL1Keys::SDLK_ESCAPE;
		mSDL2_to_SDL1[ SDLK_DELETE     ] = (U32)SDL1Keys::SDLK_DELETE;
		mSDL2_to_SDL1[ SDLK_KP_PERIOD  ] = (U32)SDL1Keys::SDLK_KP_PERIOD;
		mSDL2_to_SDL1[ SDLK_KP_DIVIDE  ] = (U32)SDL1Keys::SDLK_KP_DIVIDE;
		mSDL2_to_SDL1[ SDLK_KP_MULTIPLY] = (U32)SDL1Keys::SDLK_KP_MULTIPLY;
		mSDL2_to_SDL1[ SDLK_KP_MINUS   ] = (U32)SDL1Keys::SDLK_KP_MINUS;
		mSDL2_to_SDL1[ SDLK_KP_PLUS    ] = (U32)SDL1Keys::SDLK_KP_PLUS;
		mSDL2_to_SDL1[ SDLK_KP_ENTER   ] = (U32)SDL1Keys::SDLK_KP_ENTER;
		mSDL2_to_SDL1[ SDLK_KP_EQUALS  ] = (U32)SDL1Keys::SDLK_KP_EQUALS;
		mSDL2_to_SDL1[ SDLK_UP         ] = (U32)SDL1Keys::SDLK_UP;
		mSDL2_to_SDL1[ SDLK_DOWN       ] = (U32)SDL1Keys::SDLK_DOWN;
		mSDL2_to_SDL1[ SDLK_RIGHT      ] = (U32)SDL1Keys::SDLK_RIGHT;
		mSDL2_to_SDL1[ SDLK_LEFT       ] = (U32)SDL1Keys::SDLK_LEFT;
		mSDL2_to_SDL1[ SDLK_INSERT     ] = (U32)SDL1Keys::SDLK_INSERT;
		mSDL2_to_SDL1[ SDLK_HOME       ] = (U32)SDL1Keys::SDLK_HOME;
		mSDL2_to_SDL1[ SDLK_END        ] = (U32)SDL1Keys::SDLK_END;
		mSDL2_to_SDL1[ SDLK_PAGEUP     ] = (U32)SDL1Keys::SDLK_PAGEUP;
		mSDL2_to_SDL1[ SDLK_PAGEDOWN   ] = (U32)SDL1Keys::SDLK_PAGEDOWN;
		mSDL2_to_SDL1[ SDLK_F1         ] = (U32)SDL1Keys::SDLK_F1;
		mSDL2_to_SDL1[ SDLK_F2         ] = (U32)SDL1Keys::SDLK_F2;
		mSDL2_to_SDL1[ SDLK_F3         ] = (U32)SDL1Keys::SDLK_F3;
		mSDL2_to_SDL1[ SDLK_F4         ] = (U32)SDL1Keys::SDLK_F4;
		mSDL2_to_SDL1[ SDLK_F5         ] = (U32)SDL1Keys::SDLK_F5;
		mSDL2_to_SDL1[ SDLK_F6         ] = (U32)SDL1Keys::SDLK_F6;
		mSDL2_to_SDL1[ SDLK_F7         ] = (U32)SDL1Keys::SDLK_F7;
		mSDL2_to_SDL1[ SDLK_F8         ] = (U32)SDL1Keys::SDLK_F8;
		mSDL2_to_SDL1[ SDLK_F9         ] = (U32)SDL1Keys::SDLK_F9;
		mSDL2_to_SDL1[ SDLK_F10        ] = (U32)SDL1Keys::SDLK_F10;
		mSDL2_to_SDL1[ SDLK_F11        ] = (U32)SDL1Keys::SDLK_F11;
		mSDL2_to_SDL1[ SDLK_F12        ] = (U32)SDL1Keys::SDLK_F12;
		mSDL2_to_SDL1[ SDLK_F13        ] = (U32)SDL1Keys::SDLK_F13;
		mSDL2_to_SDL1[ SDLK_F14        ] = (U32)SDL1Keys::SDLK_F14;
		mSDL2_to_SDL1[ SDLK_F15        ] = (U32)SDL1Keys::SDLK_F15;
		mSDL2_to_SDL1[ SDLK_CAPSLOCK   ] = (U32)SDL1Keys::SDLK_CAPSLOCK;
		mSDL2_to_SDL1[ SDLK_RSHIFT     ] = (U32)SDL1Keys::SDLK_RSHIFT;
		mSDL2_to_SDL1[ SDLK_LSHIFT     ] = (U32)SDL1Keys::SDLK_LSHIFT;
		mSDL2_to_SDL1[ SDLK_RCTRL      ] = (U32)SDL1Keys::SDLK_RCTRL;
		mSDL2_to_SDL1[ SDLK_LCTRL      ] = (U32)SDL1Keys::SDLK_LCTRL;
		mSDL2_to_SDL1[ SDLK_RALT       ] = (U32)SDL1Keys::SDLK_RALT;
		mSDL2_to_SDL1[ SDLK_LALT       ] = (U32)SDL1Keys::SDLK_LALT;
		mSDL2_to_SDL1[ SDLK_MODE       ] = (U32)SDL1Keys::SDLK_MODE;
		mSDL2_to_SDL1[ SDLK_HELP       ] = (U32)SDL1Keys::SDLK_HELP;       		
		mSDL2_to_SDL1[ SDLK_SYSREQ     ] = (U32)SDL1Keys::SDLK_SYSREQ;     
		mSDL2_to_SDL1[ SDLK_MENU       ] = (U32)SDL1Keys::SDLK_MENU;       
		mSDL2_to_SDL1[ SDLK_POWER      ] = (U32)SDL1Keys::SDLK_POWER;
		mSDL2_to_SDL1[ SDLK_UNDO       ] = (U32)SDL1Keys::SDLK_UNDO;
	}
	
	auto itr = mSDL2_to_SDL1.find( aSymbol );
	if( itr != mSDL2_to_SDL1.end() )
		return itr->second;

	return aSymbol;
}

// </FS:ND> Compatibility shim for SDL2 > SDL1.

#endif


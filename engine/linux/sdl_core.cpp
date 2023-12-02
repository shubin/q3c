/*
===========================================================================
Copyright (C) 2017-2020 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// Linux main loop, event handling, etc. using SDL 2

#include "linux_local.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include "sdl_local.h"
#include <stdlib.h>


// About in_focusDelay:
// Suppose you have the game focused in windowed mode with the console down,
// open the command window (alt+F2), then press return or escape.
// On my machine, SDL will first send the X11 FocusIn event and *after that*
// the keypress event for escape or return. For oj, it's the reverse...
// In my scenario, clearing key states after reception of the FocusIn event
// won't prevent the application from receiving the undesired keypresses.

static cvar_t*	in_noGrab;
static cvar_t*	in_focusDelay;
static cvar_t*	m_relative;

static qbool 	sdl_inputActive	= qfalse;
static qbool	sdl_forceUnmute	= qfalse;	// overrides s_autoMute
static int		sdl_focusTime	= INT_MIN;	// timestamp of last X11 FocusIn event
static qbool	sdl_focused		= qtrue;	// does the X11 window have the focus?

static const cvarTableItem_t in_cvars[] = {
	{ &in_noGrab, "in_noGrab", "0", 0, CVART_BOOL, NULL, NULL, "disables input grabbing" },
	{ &in_focusDelay, "in_focusDelay", "5", CVAR_ARCHIVE, CVART_INTEGER, "0", "100", "milli-seconds keypresses are off after window focus" },
	{ &m_relative, "m_relative", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "enables SDL's relative mouse mode" }
};

static void Minimize_f();

static const cmdTableItem_t in_cmds[] = {
	{ "minimize", &Minimize_f, NULL, "minimizes the window" }
};


static qbool sdl_Version_AtLeast( int major, int minor, int patch )
{
	SDL_version v;
	SDL_GetVersion(&v);

	// has to be SDL 2
	if (v.major != major)
		return qfalse;

	if (v.minor < minor)
		return qfalse;

	if (v.minor > minor)
		return qtrue;

	return v.patch >= patch;
}


static void Minimize_f()
{
	SDL_MinimizeWindow(glimp.window);
}


static int QuakeKeyFromSDLKey( SDL_Keysym key )
{
	if (key.scancode == SDL_SCANCODE_GRAVE)
		return '`';

	const SDL_Keycode sym = key.sym;

	// these ranges map directly to ASCII chars
	if ((sym >= SDLK_a && sym <= SDLK_z) ||
		(sym >= SDLK_0 && sym <= SDLK_9))
		return (int)sym;

	// F1 to F24
	// SDL splits the values 1-12 and 13-24
	// the engine splits the values 1-15 and 16-24

	switch (sym) {
		case SDLK_F1: return K_F1;
		case SDLK_F2: return K_F2;
		case SDLK_F3: return K_F3;
		case SDLK_F4: return K_F4;
		case SDLK_F5: return K_F5;
		case SDLK_F6: return K_F6;
		case SDLK_F7: return K_F7;
		case SDLK_F8: return K_F8;
		case SDLK_F9: return K_F9;
		case SDLK_F10: return K_F10;
		case SDLK_F11: return K_F11;
		case SDLK_F12: return K_F12;
		case SDLK_F13: return K_F13;
		case SDLK_F14: return K_F14;
		case SDLK_F15: return K_F15;
		case SDLK_F16: return K_F16;
		case SDLK_F17: return K_F17;
		case SDLK_F18: return K_F18;
		case SDLK_F19: return K_F19;
		case SDLK_F20: return K_F20;
		case SDLK_F21: return K_F21;
		case SDLK_F22: return K_F22;
		case SDLK_F23: return K_F23;
		case SDLK_F24: return K_F24;
		case SDLK_UP: return K_UPARROW;
		case SDLK_DOWN: return K_DOWNARROW;
		case SDLK_LEFT: return K_LEFTARROW;
		case SDLK_RIGHT: return K_RIGHTARROW;
		case SDLK_TAB: return K_TAB;
		case SDLK_RETURN: return K_ENTER;
		case SDLK_ESCAPE: return K_ESCAPE;
		case SDLK_SPACE: return K_SPACE;
		case SDLK_BACKSPACE: return K_BACKSPACE;
		case SDLK_CAPSLOCK: return K_CAPSLOCK;
		case SDLK_LALT: return K_ALT;
		case SDLK_RALT: return K_ALT;
		case SDLK_LCTRL: return K_CTRL;
		case SDLK_RCTRL: return K_CTRL;
		case SDLK_LSHIFT: return K_SHIFT;
		case SDLK_RSHIFT: return K_SHIFT;
		case SDLK_INSERT: return K_INS;
		case SDLK_DELETE: return K_DEL;
		case SDLK_PAGEDOWN: return K_PGDN;
		case SDLK_PAGEUP: return K_PGUP;
		case SDLK_HOME: return K_HOME;
		case SDLK_END: return K_END;
		case SDLK_KP_7: return K_KP_HOME;
		case SDLK_KP_8: return K_KP_UPARROW;
		case SDLK_KP_9: return K_KP_PGUP;
		case SDLK_KP_4: return K_KP_LEFTARROW;
		case SDLK_KP_5: return K_KP_5;
		case SDLK_KP_6: return K_KP_RIGHTARROW;
		case SDLK_KP_1: return K_KP_END;
		case SDLK_KP_2: return K_KP_DOWNARROW;
		case SDLK_KP_3: return K_KP_PGDN;
		case SDLK_KP_ENTER: return K_KP_ENTER;
		case SDLK_KP_0: return K_KP_INS;
		case SDLK_KP_DECIMAL: return K_KP_DEL;
		case SDLK_KP_DIVIDE: return K_KP_SLASH;
		case SDLK_KP_MINUS: return K_KP_MINUS;
		case SDLK_KP_PLUS: return K_KP_PLUS;
		case SDLK_KP_MULTIPLY: return K_KP_STAR;
		case SDLK_BACKSLASH: return K_BACKSLASH;
		case SDLK_PAUSE: return K_PAUSE;
		case SDLK_NUMLOCKCLEAR: return K_KP_NUMLOCK;
		case SDLK_KP_EQUALS: return K_KP_EQUALS;
		case SDLK_MENU: return K_MENU;
		case SDLK_PERIOD: return '.';
		case SDLK_COMMA: return ',';
		case SDLK_EXCLAIM: return '!';
		case SDLK_HASH: return '#';
		case SDLK_PERCENT: return '%';
		case SDLK_DOLLAR: return '$';
		case SDLK_AMPERSAND: return '&';
		case SDLK_QUOTE: return '\'';
		case SDLK_LEFTPAREN: return '(';
		case SDLK_RIGHTPAREN: return ')';
		case SDLK_ASTERISK: return '*';
		case SDLK_PLUS: return '+';
		case SDLK_MINUS: return '-';
		case SDLK_SLASH: return '/';
		case SDLK_COLON: return ':';
		case SDLK_LESS: return '<';
		case SDLK_EQUALS: return '=';
		case SDLK_GREATER: return '>';
		case SDLK_QUESTION: return '?';
		case SDLK_AT: return '@';
		case SDLK_LEFTBRACKET: return '[';
		case SDLK_RIGHTBRACKET: return ']';
		case SDLK_UNDERSCORE: return '_';
		case SDLK_SEMICOLON: return ';';
		// not handled:
		// K_COMMAND (Apple)
		// K_POWER (Apple)
		// K_AUX1-16
		// K_WIN
		default: break;
	}

	if (sym >= 32 && sym <= 126)
		return (int)sym;

	return -1;
}


static void sdl_Key( const SDL_KeyboardEvent* event, qbool down )
{
	const int key = QuakeKeyFromSDLKey(event->keysym);
	if (key >= 0)
		Lin_QueEvent(Sys_Milliseconds(), SE_KEY, key, down, 0, NULL);

	if (down && key == K_BACKSPACE)
		Lin_QueEvent(Sys_Milliseconds(), SE_CHAR, 8, 0, 0, NULL);

	// ctrl+v
	if (down && key == 'v' && (event->keysym.mod & KMOD_CTRL) != 0)
		Lin_QueEvent(Sys_Milliseconds(), SE_CHAR, 22, 0, 0, NULL);
}


static void sdl_Text( const SDL_TextInputEvent* event )
{
	// text is UTF-8 encoded but we only care for
	// chars that are single-byte encoded
	const byte key = (byte)event->text[0];
	if (key >= 0 && key <= 0x7F)
		Lin_QueEvent(Sys_Milliseconds(), SE_CHAR, (int)key, 0, 0, NULL);
}


static void sdl_MouseMotion( const SDL_MouseMotionEvent* event )
{
	if (!sdl_inputActive)
		return;

	// SDL sometimes sends events with both values set to 0
	if ((event->xrel | event->yrel) == 0)
		return;

	Lin_QueEvent(Sys_Milliseconds(), SE_MOUSE, event->xrel, event->yrel, 0, NULL);
}


static void sdl_MouseButton( const SDL_MouseButtonEvent* event, qbool down )
{
	if (!sdl_inputActive && down)
		return;

	static const int mouseButtonCount = 5;
	static const int mouseButtons[mouseButtonCount][2] = {
		{ SDL_BUTTON_LEFT, K_MOUSE1 },
		{ SDL_BUTTON_RIGHT, K_MOUSE2 },
		{ SDL_BUTTON_MIDDLE, K_MOUSE3 },
		{ SDL_BUTTON_X1, K_MOUSE4 },
		{ SDL_BUTTON_X2, K_MOUSE5 }
	};

	int button = -1;
	for(int i = 0; i < mouseButtonCount; ++i) {
		if (event->button == mouseButtons[i][0]) {
			button = i;
			break;
		}
	}

	if (button < 0)
		return;

	Lin_QueEvent(Sys_Milliseconds(), SE_KEY, mouseButtons[button][1], down, 0, NULL);
}


static void sdl_MouseWheel( const SDL_MouseWheelEvent* event )
{
	if (event->y == 0)
		return;

#if SDL_VERSION_ATLEAST(2, 0, 4)
	int delta = event->y;
	if (sdl_Version_AtLeast(2, 0, 4) &&
		event->direction == SDL_MOUSEWHEEL_FLIPPED)
		delta = -delta;
#else
	const int delta = event->y;
#endif

	const int key = (delta < 0) ? K_MWHEELDOWN : K_MWHEELUP;
	Lin_QueEvent(Sys_Milliseconds(), SE_KEY, key, qtrue,  0, NULL);
	Lin_QueEvent(Sys_Milliseconds(), SE_KEY, key, qfalse, 0, NULL);
}


static void sdl_Window( const SDL_WindowEvent* event )
{
	// events of interest:
	//SDL_WINDOWEVENT_SHOWN
	//SDL_WINDOWEVENT_HIDDEN
	//SDL_WINDOWEVENT_RESIZED
	//SDL_WINDOWEVENT_SIZE_CHANGED // should prevent this from happening except on creation?
	//SDL_WINDOWEVENT_MINIMIZED
	//SDL_WINDOWEVENT_MAXIMIZED
	//SDL_WINDOWEVENT_RESTORED
	//SDL_WINDOWEVENT_ENTER // mouse focus gained
	//SDL_WINDOWEVENT_LEAVE // mouse focus lost
	//SDL_WINDOWEVENT_FOCUS_GAINED // kb focus gained
	//SDL_WINDOWEVENT_FOCUS_LOST // kb focus lost
	//SDL_WINDOWEVENT_CLOSE
	//SDL_WINDOWEVENT_MOVED

	switch (event->event) {
		case SDL_WINDOWEVENT_MAXIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
		case SDL_WINDOWEVENT_RESIZED:
		case SDL_WINDOWEVENT_SIZE_CHANGED:
		case SDL_WINDOWEVENT_MOVED:
			// if this turns out to be too expensive, track movement and
			// only call when movement stops
			sdl_UpdateMonitorIndexFromWindow();
			break;

		default:
			break;
	}

	switch (event->event) {
		case SDL_WINDOWEVENT_SHOWN:
		case SDL_WINDOWEVENT_MAXIMIZED:
		case SDL_WINDOWEVENT_RESTORED:
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			// these mean the user reacted to the alert and
			// it can now be stopped
			sdl_forceUnmute = qfalse;
			break;

		default:
			break;
	}
}


static void sdl_X11( const XEvent* event )
{
	switch (event->type) {
		case FocusIn:
			// see in_focusDelay explanation at the top
			sdl_focusTime = Sys_Milliseconds();
			sdl_focused = qtrue;
			break;

		case FocusOut:
			// set modifier keys as released to prevent
			// accidental combos such alt+enter right after
			// getting focus
			// e.g. alt gets "stuck", pressing only enter
			// does a video restart as if pressing alt+enter
			Lin_QueEvent(0, SE_KEY, K_ALT,   qfalse, 0, NULL);
			Lin_QueEvent(0, SE_KEY, K_CTRL,  qfalse, 0, NULL);
			Lin_QueEvent(0, SE_KEY, K_SHIFT, qfalse, 0, NULL);
			sdl_focused = qfalse;
			break;

		default:
			break;
	}
}


static void sdl_Event( const SDL_Event* event )
{
	// Note that CVar checks are necessary here because event polling
	// can actually start before the main loop does,
	// i.e. CVars can be uninitialized by the time we get here.

	switch (event->type) {
	case SDL_QUIT:
		Com_Quit(0);
		break;

	case SDL_KEYDOWN:
		// the CVar check means we'll ignore all keydown events until the main loop starts
		if (in_focusDelay != NULL && sdl_focused && Sys_Milliseconds() - sdl_focusTime >= in_focusDelay->integer)
			sdl_Key(&event->key, qtrue);
		break;

	case SDL_KEYUP:
		// always forward releases
		sdl_Key(&event->key, qfalse);
		break;

	case SDL_TEXTINPUT:
		if (sdl_focused)
			sdl_Text(&event->text);
		break;

	case SDL_MOUSEMOTION:
		if (sdl_focused)
			sdl_MouseMotion(&event->motion);
		break;

	case SDL_MOUSEBUTTONDOWN:
		if (sdl_focused)
			sdl_MouseButton(&event->button, qtrue);
		break;

	case SDL_MOUSEBUTTONUP:
		// always forward releases
		sdl_MouseButton(&event->button, qfalse);
		break;

	case SDL_MOUSEWHEEL:
		if (sdl_focused)
			sdl_MouseWheel(&event->wheel);
		break;

	case SDL_WINDOWEVENT:
		sdl_Window(&event->window);
		break;

	case SDL_SYSWMEVENT:
		{
			const SDL_SysWMmsg* msg = event->syswm.msg;
			if (msg->subsystem == SDL_SYSWM_X11)
				sdl_X11(&msg->msg.x11.event);
		}
		break;

	default:
		break;
	}
}


qbool sdl_Init()
{
	atexit(SDL_Quit);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
		fprintf(stderr, "Failed to initialize SDL 2: %s\n", SDL_GetError());
		return qfalse;
	}

	SDL_version version;
	SDL_GetVersion(&version);
	printf("Opened SDL %d.%d.%d\n", version.major, version.minor, version.patch);

	// @TODO: investigate/test these?
	// SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH
	// SDL_HINT_MOUSE_RELATIVE_MODE_WARP
#if SDL_VERSION_ATLEAST(2, 0, 4)
	if (sdl_Version_AtLeast(2, 0, 4))
		SDL_SetHintWithPriority(SDL_HINT_NO_SIGNAL_HANDLERS, "1", SDL_HINT_OVERRIDE);
#endif
	SDL_LogSetAllPriority(SDL_LOG_PRIORITY_CRITICAL);
	SDL_StartTextInput();	// enables SDL_TEXTINPUT events
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

	return qtrue;
}


void sdl_InitCvarsAndCmds()
{
	Cvar_RegisterArray(in_cvars, MODULE_CLIENT);
	Cmd_RegisterArray(in_cmds, MODULE_CLIENT);
}


void sdl_PollEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event))
		sdl_Event(&event);
}


static qbool sdl_IsInputActive()
{
	if (in_noGrab->integer)
		return qfalse;

	const qbool isConsoleDown = (cls.keyCatchers & KEYCATCH_CONSOLE) != 0;
	if (isConsoleDown && glimp.monitorCount >= 2)
		return qfalse;

	const qbool hasFocus = (SDL_GetWindowFlags(glimp.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
	if (!hasFocus)
		return qfalse;

	const qbool isFullScreen = Cvar_VariableIntegerValue("r_fullscreen");
	if (isConsoleDown && !isFullScreen)
		return qfalse;

	return qtrue;
}


static void S_Frame()
{
	if (sdl_forceUnmute) {
		sdl_MuteAudio(qfalse);
		return;
	}

	qbool mute = qfalse;
	if (s_autoMute->integer == AMM_UNFOCUSED) {
		const qbool hasFocus = (SDL_GetWindowFlags(glimp.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
		mute = !hasFocus;
	} else if (s_autoMute->integer == AMM_MINIMIZED) {
		const Uint32 hidingFlags = SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;
		const qbool hidden = (SDL_GetWindowFlags(glimp.window) & hidingFlags) != 0;
		mute = hidden;
	}
	sdl_MuteAudio(mute);
}


void sdl_Frame()
{
	sdl_inputActive = sdl_IsInputActive();
	sdl_PollEvents();

	SDL_SetRelativeMouseMode((sdl_inputActive && m_relative->integer) ? SDL_TRUE : SDL_FALSE);
	SDL_SetWindowGrab(glimp.window, sdl_inputActive ? SDL_TRUE : SDL_FALSE);
	SDL_ShowCursor(sdl_inputActive ? SDL_DISABLE : SDL_ENABLE);
	// @NOTE: SDL_WarpMouseInWindow generates a motion event

	S_Frame();
}


void Sys_InitInput()
{
}


void Sys_ShutdownInput()
{
}


// returns the number of bytes to skip
static int UTF8_ReadNextChar( char* c, const char* input )
{
	if (*input == '\0')
		return 0;

	const byte byte0 = (byte)input[0];
	if (byte0 <= 127) {
		*c = (char)byte0;
		return 1;
	}

	// Starts with 110?
	if ((byte0 >> 5) == 6)
		return 2;

	// Starts with 1110?
	if ((byte0 >> 4) == 14)
		return 3;

	// Starts with 11110?
	if ((byte0 >> 3) == 30)
		return 4;

	return 0;
}


char* Sys_GetClipboardData()
{
	if (SDL_HasClipboardText() == SDL_FALSE)
		return NULL;

	char* const textUTF8 = SDL_GetClipboardText();
	if (textUTF8 == NULL)
		return NULL;

	// the cleaned up string can only be
	// as long or shorter
	char* const text = (char*)Z_Malloc(strlen(textUTF8) + 1);
	if (text == NULL) {
		SDL_free(textUTF8);
		return NULL;
	}

	// clean up the text so we're sure
	// the console can display it
	char* d = text;
	const char* s = textUTF8;
	for (;;) {
		char c;
		const int bytes = UTF8_ReadNextChar(&c, s);
		if (bytes == 0) {
			*d = '\0';
			break;
		}

		if (c >= 0x20 && c <= 0x7E)
			*d++ = c;
		s += bytes;
	}

	SDL_free(textUTF8);

	return text;
}


void Sys_SetClipboardData( const char* text )
{
	SDL_SetClipboardText(text);
}


void Lin_MatchStartAlert()
{
	const int alerts = cl_matchAlerts->integer;
	const qbool unmuteBit = (alerts & MAF_UNMUTE) != 0;
	if (!unmuteBit)
		return;

	const qbool unfocusedBit = (alerts & MAF_UNFOCUSED) != 0;	
	const qbool hasFocus = (SDL_GetWindowFlags(glimp.window) & SDL_WINDOW_INPUT_FOCUS) != 0;
	const Uint32 hidingFlags = SDL_WINDOW_HIDDEN | SDL_WINDOW_MINIMIZED;
	const qbool hidden = (SDL_GetWindowFlags(glimp.window) & hidingFlags) != 0;
	if (hidden || (unfocusedBit && !hasFocus))
		sdl_forceUnmute = qtrue;
}


void Lin_MatchEndAlert()
{
	sdl_forceUnmute = qfalse;
}


void Sys_MatchAlert( sysMatchAlertEvent_t event )
{
	if (event == SMAE_MATCH_START)
		Lin_MatchStartAlert();
	else if (event == SMAE_MATCH_END)
		Lin_MatchEndAlert();
}


qbool Sys_IsMinimized()
{
	return (glimp.window != NULL) && (SDL_GetWindowFlags(glimp.window) & SDL_WINDOW_MINIMIZED) != 0;
}

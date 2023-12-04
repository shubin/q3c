/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "../client/client.h"
#include "win_local.h"


// Console variables that we need to access from this module
cvar_t		*vid_xpos;			// X coordinate of window position
cvar_t		*vid_ypos;			// Y coordinate of window position
cvar_t		*r_fullscreen;


static void WIN_AppActivate( BOOL fActive, BOOL fMinimized )
{
	const qbool active = fActive && !fMinimized;

	if ( r_fullscreen->integer )
		SetWindowPos( g_wv.hWnd, active ? HWND_TOPMOST : HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE );

	Key_ClearStates();

	// we don't want to act like we're active if we're minimized
	g_wv.activeApp = active;
}


///////////////////////////////////////////////////////////////


static byte s_scantokey[128] =
{
//  0           1       2       3       4       5       6       7 
//  8           9       A       B       C       D       E       F 
	0  ,    27,     '1',    '2',    '3',    '4',    '5',    '6', 
	'7',    '8',    '9',    '0',    '-',    '=',    K_BACKSPACE, 9, // 0 
	'q',    'w',    'e',    'r',    't',    'y',    'u',    'i', 
	'o',    'p',    '[',    ']',    13 ,    K_CTRL,'a',  's',      // 1 
	'd',    'f',    'g',    'h',    'j',    'k',    'l',    ';', 
	'\'' ,    '`',    K_SHIFT,'\\',  'z',    'x',    'c',    'v',      // 2 
	'b',    'n',    'm',    ',',    '.',    '/',    K_SHIFT,'*', 
	K_ALT,' ',   K_CAPSLOCK  ,    K_F1, K_F2, K_F3, K_F4, K_F5,   // 3 
	K_F6, K_F7, K_F8, K_F9, K_F10,  K_PAUSE,    0  , K_HOME, 
	K_UPARROW,K_PGUP,K_KP_MINUS,K_LEFTARROW,K_KP_5,K_RIGHTARROW, K_KP_PLUS,K_END, //4 
	K_DOWNARROW,K_PGDN,K_INS,K_DEL,0,0,             0,              K_F11, 
	K_F12,0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 5
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0,        // 6 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0, 
	0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0  ,    0         // 7 
};

/*
=======
MapKey

Map from windows to quake keynums
=======
*/
static int MapKey( int wParam, int lParam )
{
	// the K_F13 to K_F24 values are *not* contiguous for mod compatibility reasons
	switch ( wParam )
	{
		case VK_F13: return K_F13;
		case VK_F14: return K_F14;
		case VK_F15: return K_F15;
		case VK_F16: return K_F16;
		case VK_F17: return K_F17;
		case VK_F18: return K_F18;
		case VK_F19: return K_F19;
		case VK_F20: return K_F20;
		case VK_F21: return K_F21;
		case VK_F22: return K_F22;
		case VK_F23: return K_F23;
		case VK_F24: return K_F24;
		default: break;
	}

	const int scanCode = ( lParam >> 16 ) & 255;
	if ( scanCode > 127 )
		return 0; // why?

	const qbool isExtended = (lParam & ( 1 << 24 )) != 0;
	const int result = s_scantokey[scanCode];

	if ( !isExtended )
	{
		switch ( result )
		{
		case K_HOME:
			return K_KP_HOME;
		case K_UPARROW:
			return K_KP_UPARROW;
		case K_PGUP:
			return K_KP_PGUP;
		case K_LEFTARROW:
			return K_KP_LEFTARROW;
		case K_RIGHTARROW:
			return K_KP_RIGHTARROW;
		case K_END:
			return K_KP_END;
		case K_DOWNARROW:
			return K_KP_DOWNARROW;
		case K_PGDN:
			return K_KP_PGDN;
		case K_INS:
			return K_KP_INS;
		case K_DEL:
			return K_KP_DEL;
		case '*':
			return K_KP_STAR;
		case 0x00:
			return K_BACKSLASH;
		default:
			return result;
		}
	}
	else
	{
		switch ( result )
		{
		case K_PAUSE:
			return K_KP_NUMLOCK;
		case 0x0D:
			return K_KP_ENTER;
		case 0x2F:
			return K_KP_SLASH;
		case 0xAF:
			return K_KP_PLUS;
		}
		return result;
	}
}


/*
====================
MainWndProc

main window procedure
====================
*/

LRESULT CALLBACK MainWndProc (
    HWND    hWnd,
    UINT    uMsg,
    WPARAM  wParam,
    LPARAM  lParam)
{
	static qbool draggingWindow = qfalse;

	switch (uMsg)
	{
	case WM_CREATE:

		g_wv.hWnd = hWnd;

		vid_xpos = Cvar_Get( "vid_xpos", "3", CVAR_ARCHIVE );
		vid_ypos = Cvar_Get( "vid_ypos", "22", CVAR_ARCHIVE );
		r_fullscreen = Cvar_Get( "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH );
		Cvar_Get( "r_monitor", "0", CVAR_ARCHIVE | CVAR_LATCH ); // 1-based monitor index, 0 means primary

		WIN_RegisterLastValidHotKey();

		break;

	case WM_DESTROY:
		WIN_UnregisterHotKey();
		g_wv.hWnd = NULL;
		break;

	case WM_CLOSE:
		Cbuf_AddText( "quit\n" );
		break;

	case WM_ACTIVATE:
		WIN_AppActivate( (LOWORD(wParam) != WA_INACTIVE), !!(BOOL)HIWORD(wParam) );
		WIN_S_Mute( !g_wv.activeApp );
		break;

	case WM_MOVING:
		draggingWindow = qtrue;
		break;

	case WM_MOVE:
		{
			WIN_UpdateMonitorIndexFromMainWindow();

			if ( !r_fullscreen->integer )
			{
				RECT r;
				r.left   = 0;
				r.top    = 0;
				r.right  = 1;
				r.bottom = 1;
				AdjustWindowRect( &r, GetWindowLong( hWnd, GWL_STYLE ), FALSE );
				
				const RECT& monRect = g_wv.monitorRects[g_wv.monitor];
				const int x = (int)(short)LOWORD( lParam );
				const int y = (int)(short)HIWORD( lParam );
				Cvar_SetValue( "vid_xpos", x + r.left - monRect.left );
				Cvar_SetValue( "vid_ypos", y + r.top - monRect.top );
				vid_xpos->modified = qfalse;
				vid_ypos->modified = qfalse;
			}

			draggingWindow = qfalse;
		}
		break;

	case WM_SIZE:
		if ( wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED )
		{
			WIN_UpdateMonitorIndexFromMainWindow();

			// note that WM_SIZE can be called with no actual size change
			const int w = (int)LOWORD( lParam );
			const int h = (int)HIWORD( lParam );
			if ( g_wv.duringCreateWindow )
				WIN_UpdateResolution( w, h );
		}
		break;

	case WM_WINDOWPOSCHANGED:
		{
			const int prevMon = g_wv.monitor;
			WIN_UpdateMonitorIndexFromMainWindow();
			const int currMon = g_wv.monitor;

			if ( !g_wv.duringCreateWindow && !draggingWindow && currMon != prevMon )
				Cbuf_AddText( "vid_restart\n" );
		}
		break;

	case WM_SYSCOMMAND:
		if ( wParam == SC_SCREENSAVE )
			return 0;
		break;

	case WM_SYSKEYDOWN:
		if ( wParam == VK_RETURN )
		{
			if ( r_fullscreen )
			{
				Cvar_SetValue( "r_fullscreen", !r_fullscreen->integer );
				Cbuf_AddText( "vid_restart\n" );
			}
			return 0;
		}
		// fall through
	case WM_KEYDOWN:
		WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, MapKey( wParam, lParam ), qtrue, 0, NULL );
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, MapKey( wParam, lParam ), qfalse, 0, NULL );
		break;

	case WM_CHAR:
		{
			const char scanCode = (char)( ( lParam >> 16 ) & 0xFF );
			if ( scanCode != 0x29 ) // never send an event for the console key ('~' or '`')
				WIN_QueEvent( g_wv.sysMsgTime, SE_CHAR, wParam, 0, 0, NULL );
		}
		break;

	case WM_HOTKEY:
		if ( g_wv.minimizeHotKeyValid && (int)wParam == g_wv.minimizeHotKeyId )
		{
			if ( g_wv.activeApp && !CL_VideoRecording() )
			{
				ShowWindow( hWnd, SW_MINIMIZE );
			}
			else
			{
				ShowWindow( hWnd, SW_RESTORE );
				SetForegroundWindow( hWnd );
				SetFocus( hWnd );
			}
		}
		break;

	case WM_SETFOCUS:
		if ( g_wv.cdsDevModeValid ) // is there a valid mode to restore?
		{
			WIN_SetGameDisplaySettings();
			if ( g_wv.cdsDevModeValid ) // was the mode successfully restored?
			{
				const RECT& rect = g_wv.monitorRects[g_wv.monitor];
				const DEVMODE& dm = g_wv.cdsDevMode;
				SetWindowPos( hWnd, NULL, (int)rect.left, (int)rect.top, (int)dm.dmPelsWidth, (int)dm.dmPelsHeight, SWP_NOZORDER );
			}
		}
		g_wv.activeApp = (qbool)!IsIconic( hWnd );
		g_wv.forceUnmute = qfalse;
		break;

	case WM_KILLFOCUS:
		g_wv.activeApp = qfalse;
		if ( g_wv.cdsDevModeValid )
			WIN_SetDesktopDisplaySettings();
		break;

	default:
		// this is complicated because Win32 seems to pack multiple mouse events into
		// one update sometimes, so we always check all states and look for events
		if ( uMsg == WM_INPUT || (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST) || uMsg == WM_MOUSEWHEEL )
			if ( IN_ProcessMessage(uMsg, wParam, lParam) )
				return 0;
		break;
	}

	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}


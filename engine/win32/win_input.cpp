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
#include "win_help.h"


static cvar_t* in_noGrab;


struct Mouse {
	Mouse() : active(qfalse) {}
	virtual ~Mouse() {}

	qbool IsActive() const { return active; }

	virtual qbool Init() { return qfalse; }														// qtrue if successful
	virtual qbool Activate( qbool _active ) { return qfalse; }									// qtrue if successful
	virtual void Shutdown() {}
	virtual qbool ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam ) { return qfalse; }	// qtrue if the event was handled
	virtual void GetClipRect( RECT* clip, const RECT* client ) = 0;

protected:
	void UpdateWheel( int delta );	// queues mouse wheel events if needed

	qbool active;
};

static Mouse* mouse;
static qbool mouseSettingsSet = qfalse;


void Mouse::UpdateWheel( int delta )
{
	if (delta > 0) {
		while (delta >= WHEEL_DELTA) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MWHEELUP, qtrue,  0, NULL );
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MWHEELUP, qfalse, 0, NULL );
			delta -= WHEEL_DELTA;
		}
	} else {
		while (delta <= -WHEEL_DELTA) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MWHEELDOWN, qtrue,  0, NULL );
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MWHEELDOWN, qfalse, 0, NULL );
			delta += WHEEL_DELTA;
		}
	}
}


///////////////////////////////////////////////////////////////


struct rawmouse_t : public Mouse {
	virtual qbool Init();
	virtual qbool Activate( qbool active );
	virtual void Shutdown();
	virtual qbool ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam );
	virtual void GetClipRect( RECT* clip, const RECT* client );
};

static rawmouse_t rawmouse;


qbool rawmouse_t::Init()
{
	// Problems with the RIDEV_NOLEGACY flag:
	// - When focusing the app while pressing a mouse button, the cursor becomes visible and stays so indefinitely
	//   despite properly having done the repeated calls to ShowCursor(FALSE) until the counter is negative.
	//   You have to wait for the mouse button to be released to call ReleaseCapture() and SetCapture(hWnd) again in order to
	//   actually hide the cursor again.
	// - Unlike what the MSDN docs might let you think, some *really* important messages get dropped, not just mouse input ones.
	//   The application won't get the focus back when being clicked!
	// It's a buggy mess, so don't use it.

	RAWINPUTDEVICE rid;
	rid.usUsagePage = 1;
	rid.usUsage = 2;
	rid.dwFlags = 0;
	rid.hwndTarget = NULL;

	if (!RegisterRawInputDevices( &rid, 1, sizeof(rid)) ) {
		active = qfalse;
		return qfalse;
	}

	active = qtrue;
	return qtrue;
}


qbool rawmouse_t::Activate( qbool _active )
{
	active = _active;

	return qtrue;
}


void rawmouse_t::Shutdown()
{
	RAWINPUTDEVICE rid;
	rid.usUsagePage = 1;
	rid.usUsage = 2;
	rid.dwFlags = RIDEV_REMOVE;
	rid.hwndTarget = NULL;
	RegisterRawInputDevices( &rid, 1, sizeof(rid) );
}


// MSDN says you have to always let DefWindowProc run for WM_INPUT
// regardless of whether you process the message or not, so ALWAYS return false here

qbool rawmouse_t::ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	static const int riBtnDnFlags[5] = { RI_MOUSE_BUTTON_1_DOWN, RI_MOUSE_BUTTON_2_DOWN, RI_MOUSE_BUTTON_3_DOWN, RI_MOUSE_BUTTON_4_DOWN, RI_MOUSE_BUTTON_5_DOWN };
	static const int riBtnUpFlags[5] = { RI_MOUSE_BUTTON_1_UP, RI_MOUSE_BUTTON_2_UP, RI_MOUSE_BUTTON_3_UP, RI_MOUSE_BUTTON_4_UP, RI_MOUSE_BUTTON_5_UP };

	if (msg != WM_INPUT)
		return qfalse;

	HRAWINPUT h = (HRAWINPUT)lParam;
	UINT size;
	if (GetRawInputData( h, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER) ) == -1)
		return qfalse;

	RAWINPUT ri;
	if (GetRawInputData( h, RID_INPUT, &ri, &size, sizeof(RAWINPUTHEADER) ) != size)
		return qfalse;

	if ( (ri.header.dwType != RIM_TYPEMOUSE) || (ri.data.mouse.usFlags != MOUSE_MOVE_RELATIVE) )
		return qfalse;

	const int dx = (int)ri.data.mouse.lLastX;
	const int dy = (int)ri.data.mouse.lLastY;
	if (active && (dx != 0 || dy != 0))
		WIN_QueEvent( g_wv.sysMsgTime, SE_MOUSE, dx, dy, 0, NULL );

	if (!ri.data.mouse.usButtonFlags) // no button or wheel transitions
		return qfalse;

	if ((ri.data.mouse.usButtonFlags & RI_MOUSE_WHEEL) != 0)
		UpdateWheel( (SHORT)ri.data.mouse.usButtonData );

	for (int i = 0; i < 5; ++i) {
		// @TODO: was previously only sent when active was qtrue
		// is this still okay now?
		//if (active && (ri.data.mouse.usButtonFlags & riBtnDnFlags[i]) != 0)
		if (ri.data.mouse.usButtonFlags & riBtnDnFlags[i])
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qtrue, 0, NULL );

		// we always send the button up events to avoid
		// buttons getting "stuck" when bringing the console down
		if (ri.data.mouse.usButtonFlags & riBtnUpFlags[i])
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_MOUSE1 + i, qfalse, 0, NULL );
	}

	return qfalse;
}


void rawmouse_t::GetClipRect( RECT* clip, const RECT* client )
{
	// when passing the window's client area in desktop coordinates to ClipCursor,
	// it is *still* possible to click outside the window (on Windows 7 at least)
	POINT center = { ( client->left + client->right ) / 2, ( client->top + client->bottom ) / 2 };
	MapWindowPoints( g_wv.hWnd, HWND_DESKTOP, &center, 1 );
	*clip = { center.x - 1, center.y - 1, center.x + 1, center.y + 1 };
}


///////////////////////////////////////////////////////////////


struct winmouse_t : public Mouse {
	virtual qbool Init() { return qtrue; }
	virtual qbool Activate( qbool active );
	virtual qbool ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam );
	virtual void GetClipRect( RECT* clip, const RECT* client );

private:
	void UpdateWindowCenter();

	int window_center_x, window_center_y;
};

static winmouse_t winmouse;


void winmouse_t::UpdateWindowCenter()
{
	RECT rect;
	GetClientRect( g_wv.hWnd, &rect );
	POINT center;
	center.x = rect.right / 2;
	center.y = rect.bottom / 2;
	MapWindowPoints( g_wv.hWnd, HWND_DESKTOP, &center, 1 );
	window_center_x = (int)center.x;
	window_center_y = (int)center.y;
}


qbool winmouse_t::Activate( qbool _active )
{
	active = _active;

	if (!_active)
		return qtrue;

	UpdateWindowCenter();
	SetCursorPos( window_center_x, window_center_y );

	return qtrue;
}


qbool winmouse_t::ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	if ( !active ) {
		if ( msg == WM_MOUSEWHEEL )
			UpdateWheel( GET_WHEEL_DELTA_WPARAM(wParam) );

		return qfalse;
	}

	UpdateWindowCenter();

#define QUEUE_WM_BUTTON( qbutton, mask ) \
	WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, qbutton, (wParam & mask), 0, NULL )

	POINT p;
	GetCursorPos( &p );
	const int dx = p.x - window_center_x;
	const int dy = p.y - window_center_y;
	if (dx != 0 || dy != 0) {
		WIN_QueEvent( g_wv.sysMsgTime, SE_MOUSE, dx, dy, 0, NULL );
		SetCursorPos( window_center_x, window_center_y );
	}

	switch (msg) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
		QUEUE_WM_BUTTON( K_MOUSE1, MK_LBUTTON );
		break;
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
		QUEUE_WM_BUTTON( K_MOUSE2, MK_RBUTTON );
		break;
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
		QUEUE_WM_BUTTON( K_MOUSE3, MK_MBUTTON );
		break;
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1)
			QUEUE_WM_BUTTON( K_MOUSE4, MK_XBUTTON1 );
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2)
			QUEUE_WM_BUTTON( K_MOUSE5, MK_XBUTTON2 );
		break;
#undef QUEUE_WM_BUTTON

	case WM_MOUSEWHEEL:
		UpdateWheel( GET_WHEEL_DELTA_WPARAM(wParam) );
		break;

	default:
		return qfalse;
	}

	return qtrue;
}


void winmouse_t::GetClipRect( RECT* clip, const RECT* client )
{
	const int border = 16; // a little safety nest since Windows doesn't handle cursor clip rectangles perfectly right
	POINT endPoints[2] = { { client->left, client->top }, { client->right, client->bottom } };
	MapWindowPoints( g_wv.hWnd, HWND_DESKTOP, endPoints, 2 );
	*clip = { endPoints[0].x + border, endPoints[0].y + border, endPoints[1].x - 2 * border, endPoints[1].y - 2 * border };
}


///////////////////////////////////////////////////////////////


static void IN_StartupMouse()
{
	assert( mouse == NULL );
	mouse = NULL;
	mouseSettingsSet = qfalse;

	cvar_t* in_mouse = Cvar_Get( "in_mouse", "1", CVAR_ARCHIVE|CVAR_LATCH );
	Cvar_SetRange( "in_mouse", CVART_INTEGER, "0", "2" );
	Cvar_SetHelp( "in_mouse", help_in_mouse );
	in_mouse->modified = qfalse;

	in_noGrab = Cvar_Get( "in_noGrab", "0", 0 );
	Cvar_SetRange( "in_noGrab", CVART_BOOL, NULL, NULL );
	Cvar_SetHelp( "in_noGrab", "disables input grabbing" );

	if (!in_mouse->integer) {
		Com_Printf( "Mouse not active.\n" );
		return;
	}

	if (in_mouse->integer == 1) {
		if (rawmouse.Init()) {
			mouse = &rawmouse;
			Com_Printf( "Using raw mouse input\n" );
			return;
		}
		Com_Printf( "Raw mouse initialization failed\n" );
	}

	if (winmouse.Init()) {
		mouse = &winmouse;
		Com_Printf( "Using Win32 mouse input\n" );
	} else {
		Com_Printf( "Win32 mouse initialization failed\n" );
	}
}


qbool IN_ProcessMessage( UINT msg, WPARAM wParam, LPARAM lParam )
{
	if (!mouse)
		return qfalse;

	return mouse->ProcessMessage( msg, wParam, lParam );
}


///////////////////////////////////////////////////////////////


static void IN_StartupMIDI();
static void IN_ShutdownMIDI();

static void IN_StartupJoystick();
static void IN_JoyMove();

static void IN_StartupHotKey( qbool fullStartUp );
static void IN_ShutDownHotKey();
static void IN_UpdateHotKey();


cvar_t* in_joystick;
cvar_t* in_midi;
cvar_t* in_minimize;


static void IN_Startup()
{
	QSUBSYSTEM_INIT_START( "Input" );
	IN_StartupMouse();
	IN_StartupJoystick();
	IN_StartupMIDI();
	IN_StartupHotKey( qtrue );
	QSUBSYSTEM_INIT_DONE( "Input" );
}


void Sys_InitInput()
{
	if (g_wv.inputInitialized)
		return;

	in_midi = Cvar_Get( "in_midi", "0", CVAR_ARCHIVE );
	Cvar_SetModule( "in_midi", MODULE_INPUT );

	in_joystick = Cvar_Get( "in_joystick", "0", CVAR_ARCHIVE|CVAR_LATCH );
	Cvar_SetModule( "in_joystick", MODULE_INPUT );
	in_joystick->modified = qfalse;

	IN_Startup();

	g_wv.inputInitialized = qtrue;
}


void Sys_ShutdownInput()
{
	if (!g_wv.inputInitialized)
		return;

	g_wv.inputInitialized = qfalse;

	IN_Activate( qfalse );

	if (mouse) {
		mouse->Shutdown();
		mouse = NULL;
	}

	IN_ShutdownMIDI();
	IN_ShutDownHotKey();
}


static void IN_SetCursorSettings( qbool active )
{
	if (active) {
		while (ShowCursor(FALSE) >= 0) {}
		SetCapture( g_wv.hWnd );
		RECT clientRect;
		GetClientRect( g_wv.hWnd, &clientRect );
		RECT clipRect;
		mouse->GetClipRect( &clipRect, &clientRect );
		ClipCursor( &clipRect );
	} else {
		while (ShowCursor(TRUE) < 0) {}
		ClipCursor( NULL );
		ReleaseCapture();
	}
}


// called when the window gains or loses focus or changes in some way
// the window may have been destroyed and recreated between a deactivate and an activate

void IN_Activate( qbool active )
{
	if ( !mouse || (mouseSettingsSet && mouse->IsActive() == active) )
		return;

	if ( mouse->Activate( active ) )
		IN_SetCursorSettings( active );

	mouseSettingsSet = qtrue;
}


static qbool IN_ShouldBeActive()
{
	// @TODO: move most of this out so shared client code handles this once

	if ( in_noGrab && in_noGrab->integer )
		return qfalse;

	if ( Cvar_VariableIntegerValue("r_debugInput") )
		return qfalse;

	const qbool isConsoleDown = (cls.keyCatchers & KEYCATCH_CONSOLE) != 0;
	if ( g_wv.monitorCount >= 2 && isConsoleDown )
		return qfalse;

	if ( GetFocus() != g_wv.hWnd )
		return qfalse;

	return g_wv.activeApp && (!isConsoleDown || Cvar_VariableIntegerValue("r_fullscreen"));
}


// called every frame, even if not generating commands

void IN_Frame()
{
	// lazily initialize if needed
	if ( !com_dedicated->integer && !g_wv.inputInitialized )
		Sys_InitInput();

	IN_JoyMove();
	IN_UpdateHotKey();

	if (!mouse)
		return;

	IN_Activate( IN_ShouldBeActive() );
}


/*
=========================================================================

JOYSTICK

=========================================================================
*/


typedef struct {
	qbool	avail;
	int			id;			// joystick number
	JOYCAPS		jc;

	int			oldbuttonstate;
	int			oldpovstate;

	JOYINFOEX	ji;
} joystickInfo_t;

static	joystickInfo_t	joy;


static const cvar_t* in_joyBallScale;
static const cvar_t* in_debugJoystick;
static const cvar_t* joy_threshold;


static void IN_StartupJoystick()
{
	// assume no joystick
	joy.avail = qfalse;

	if ( !in_joystick->integer )
		return;

	// verify joystick driver is present
	int numdevs;
	if ((numdevs = joyGetNumDevs()) == 0)
	{
		Com_Printf( "joystick not found -- driver not present\n" );
		return;
	}

	// cycle through the joystick ids for the first valid one
	MMRESULT mmr = !JOYERR_NOERROR;
	for (joy.id=0 ; joy.id<numdevs ; joy.id++)
	{
		Com_Memset (&joy.ji, 0, sizeof(joy.ji));
		joy.ji.dwSize = sizeof(joy.ji);
		joy.ji.dwFlags = JOY_RETURNCENTERED;

		if ((mmr = joyGetPosEx (joy.id, &joy.ji)) == JOYERR_NOERROR)
			break;
	}

	// abort startup if we didn't find a valid joystick
	if (mmr != JOYERR_NOERROR)
	{
		Com_Printf( "joystick not found -- no valid joysticks (%x)\n", mmr );
		return;
	}

	// get the capabilities of the selected joystick
	// abort startup if command fails
	Com_Memset (&joy.jc, 0, sizeof(joy.jc));
	if ((mmr = joyGetDevCaps (joy.id, &joy.jc, sizeof(joy.jc))) != JOYERR_NOERROR)
	{
		Com_Printf( "joystick not found -- invalid joystick capabilities (%x)\n", mmr );
		return;
	}

	// activate and archive the cvars since they actually want joystick support
	in_debugJoystick	= Cvar_Get ("in_debugjoystick",			"0",		CVAR_TEMP);
	in_joyBallScale		= Cvar_Get ("in_joyBallScale",			"0.02",		CVAR_ARCHIVE);
	joy_threshold		= Cvar_Get ("joy_threshold",			"0.15",		CVAR_ARCHIVE);

	Com_Printf( "Joystick found.\n" );
	Com_Printf( "Pname: %s\n", joy.jc.szPname );
	Com_Printf( "OemVxD: %s\n", joy.jc.szOEMVxD );
	Com_Printf( "RegKey: %s\n", joy.jc.szRegKey );

	Com_Printf( "Numbuttons: %i / %i\n", joy.jc.wNumButtons, joy.jc.wMaxButtons );
	Com_Printf( "Axis: %i / %i\n", joy.jc.wNumAxes, joy.jc.wMaxAxes );
	Com_Printf( "Caps: 0x%x\n", joy.jc.wCaps );
	if ( joy.jc.wCaps & JOYCAPS_HASPOV ) {
		Com_Printf( "HASPOV\n" );
	} else {
		Com_Printf( "no POV\n" );
	}

	// old button and POV states default to no buttons pressed
	joy.oldbuttonstate = 0;
	joy.oldpovstate = 0;

	// mark the joystick as available
	joy.avail = qtrue;
}


static float JoyToF( int value )
{
	float	fValue;

	// move centerpoint to zero
	value -= 32768;

	// convert range from -32768..32767 to -1..1
	fValue = (float)value / 32768.0;

	return Com_Clamp( -1, 1, fValue );
}


static int JoyToI( int value )
{
	// move centerpoint to zero
	return (value - 32768);
}


static const int joyDirectionKeys[16] = {
	K_LEFTARROW, K_RIGHTARROW,
	K_UPARROW, K_DOWNARROW,
	K_JOY16, K_JOY17,
	K_JOY18, K_JOY19,
	K_JOY20, K_JOY21,
	K_JOY22, K_JOY23,
	K_JOY24, K_JOY25,
	K_JOY26, K_JOY27
};


static void IN_JoyMove()
{
	float	fAxisValue;
	UINT i;
	DWORD buttonstate, povstate;
	int		x, y;

	// verify joystick is available and that the user wants to use it
	if ( !joy.avail ) {
		return; 
	}

	// collect the joystick data, if possible
	Com_Memset (&joy.ji, 0, sizeof(joy.ji));
	joy.ji.dwSize = sizeof(joy.ji);
	joy.ji.dwFlags = JOY_RETURNALL;

	if ( joyGetPosEx (joy.id, &joy.ji) != JOYERR_NOERROR ) {
		// read error occurred
		// turning off the joystick seems too harsh for 1 read error,
		// but what should be done?
		// Com_Printf ("IN_ReadJoystick: no response\n");
		// joy.avail = qfalse;
		return;
	}

	if ( in_debugJoystick->integer ) {
		Com_Printf( "%8x %5i %5.2f %5.2f %5.2f %5.2f %6i %6i\n",
			joy.ji.dwButtons,
			joy.ji.dwPOV,
			JoyToF( joy.ji.dwXpos ), JoyToF( joy.ji.dwYpos ),
			JoyToF( joy.ji.dwZpos ), JoyToF( joy.ji.dwRpos ),
			JoyToI( joy.ji.dwUpos ), JoyToI( joy.ji.dwVpos ) );
	}

	// loop through the joystick buttons
	// key a joystick event or auxillary event for higher number buttons for each state change
	buttonstate = joy.ji.dwButtons;
	for ( i=0 ; i < joy.jc.wNumButtons ; i++ ) {
		if ( (buttonstate & (1<<i)) && !(joy.oldbuttonstate & (1<<i)) ) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_JOY1 + i, qtrue, 0, NULL );
		}
		if ( !(buttonstate & (1<<i)) && (joy.oldbuttonstate & (1<<i)) ) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, K_JOY1 + i, qfalse, 0, NULL );
		}
	}
	joy.oldbuttonstate = buttonstate;

	povstate = 0;

	// convert main joystick motion into 6 direction button bits
	for (i = 0; i < joy.jc.wNumAxes && i < 4 ; i++) {
		// get the floating point zero-centered, potentially-inverted data for the current axis
		fAxisValue = JoyToF( (&joy.ji.dwXpos)[i] );

		if ( fAxisValue < -joy_threshold->value ) {
			povstate |= (1<<(i*2));
		} else if ( fAxisValue > joy_threshold->value ) {
			povstate |= (1<<(i*2+1));
		}
	}

	// convert POV information from a direction into 4 button bits
	if ( joy.jc.wCaps & JOYCAPS_HASPOV ) {
		if ( joy.ji.dwPOV != JOY_POVCENTERED ) {
			if (joy.ji.dwPOV == JOY_POVFORWARD)
				povstate |= 1<<12;
			if (joy.ji.dwPOV == JOY_POVBACKWARD)
				povstate |= 1<<13;
			if (joy.ji.dwPOV == JOY_POVRIGHT)
				povstate |= 1<<14;
			if (joy.ji.dwPOV == JOY_POVLEFT)
				povstate |= 1<<15;
		}
	}

	// determine which bits have changed and key an auxillary event for each change
	for (i=0 ; i < 16 ; i++) {
		if ( (povstate & (1<<i)) && !(joy.oldpovstate & (1<<i)) ) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qtrue, 0, NULL );
		}

		if ( !(povstate & (1<<i)) && (joy.oldpovstate & (1<<i)) ) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, joyDirectionKeys[i], qfalse, 0, NULL );
		}
	}
	joy.oldpovstate = povstate;

	// if there is a trackball like interface, simulate mouse moves
	if ( joy.jc.wNumAxes >= 6 ) {
		x = JoyToI( joy.ji.dwUpos ) * in_joyBallScale->value;
		y = JoyToI( joy.ji.dwVpos ) * in_joyBallScale->value;
		if ( x || y ) {
			WIN_QueEvent( g_wv.sysMsgTime, SE_MOUSE, x, y, 0, NULL );
		}
	}
}


/*
=========================================================================

MIDI

=========================================================================
*/


#define MAX_MIDIIN_DEVICES	8

typedef struct {
	int			numDevices;
	MIDIINCAPS	caps[MAX_MIDIIN_DEVICES];
	HMIDIIN		hMidiIn;
} MidiInfo_t;

static MidiInfo_t s_midiInfo;


static const cvar_t* in_midiport;
static const cvar_t* in_midichannel;
static const cvar_t* in_mididevice;


static void MIDI_NoteOff( int note )
{
	int qkey;

	qkey = note - 60 + K_AUX1;

	if ( qkey > 255 || qkey < K_AUX1 )
		return;

	WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, qkey, qfalse, 0, NULL );
}

static void MIDI_NoteOn( int note, int velocity )
{
	int qkey;

	if ( velocity == 0 )
		MIDI_NoteOff( note );

	qkey = note - 60 + K_AUX1;

	if ( qkey > 255 || qkey < K_AUX1 )
		return;

	WIN_QueEvent( g_wv.sysMsgTime, SE_KEY, qkey, qtrue, 0, NULL );
}

static void CALLBACK MidiInProc( HMIDIIN hMidiIn, UINT uMsg, DWORD dwInstance, DWORD dwParam1, DWORD dwParam2 )
{
	int message;

	switch ( uMsg )
	{
	case MIM_OPEN:
		break;
	case MIM_CLOSE:
		break;
	case MIM_DATA:
		message = dwParam1 & 0xff;
		if ( ( message & 0xf0 ) == 0x90 )
		{
			if ( ( ( message & 0x0f ) + 1 ) == in_midichannel->integer )
				MIDI_NoteOn( ( dwParam1 & 0xff00 ) >> 8, ( dwParam1 & 0xff0000 ) >> 16 );
		}
		else if ( ( message & 0xf0 ) == 0x80 )
		{
			if ( ( ( message & 0x0f ) + 1 ) == in_midichannel->integer )
				MIDI_NoteOff( ( dwParam1 & 0xff00 ) >> 8 );
		}
		break;
	case MIM_LONGDATA:
		break;
	case MIM_ERROR:
		break;
	case MIM_LONGERROR:
		break;
	}

//	WIN_QueEvent( sys_msg_time, SE_KEY, wMsg, qtrue, 0, NULL );
}

static void MidiInfo_f( void )
{
	int i;

	const char *enableStrings[] = { "disabled", "enabled" };

	Com_Printf( "\nMIDI control:       %s\n", enableStrings[in_midi->integer != 0] );
	Com_Printf( "port:               %d\n", in_midiport->integer );
	Com_Printf( "channel:            %d\n", in_midichannel->integer );
	Com_Printf( "current device:     %d\n", in_mididevice->integer );
	Com_Printf( "number of devices:  %d\n", s_midiInfo.numDevices );

	for ( i = 0; i < s_midiInfo.numDevices; i++ )
	{
		if ( i == Cvar_VariableValue( "in_mididevice" ) )
			Com_Printf( "***" );
		else
			Com_Printf( "..." );
		Com_Printf(    "device %2d:       %s\n", i, s_midiInfo.caps[i].szPname );
		Com_Printf( "...manufacturer ID: 0x%hx\n", s_midiInfo.caps[i].wMid );
		Com_Printf( "...product ID:      0x%hx\n", s_midiInfo.caps[i].wPid );
		Com_Printf( "\n" );
	}
}

static void IN_StartupMIDI()
{
	int i;

	if ( !Cvar_VariableValue( "in_midi" ) )
		return;

	//
	// enumerate MIDI IN devices
	//
	s_midiInfo.numDevices = midiInGetNumDevs();

	for ( i = 0; i < s_midiInfo.numDevices; i++ )
	{
		midiInGetDevCaps( i, &s_midiInfo.caps[i], sizeof( s_midiInfo.caps[i] ) );
	}

	// activate and archive the cvars since they actually want midi support
	in_midiport		= Cvar_Get ("in_midiport",				"1",		CVAR_ARCHIVE);
	in_midichannel	= Cvar_Get ("in_midichannel",			"1",		CVAR_ARCHIVE);
	in_mididevice	= Cvar_Get ("in_mididevice",			"0",		CVAR_ARCHIVE);

	Cmd_AddCommand( "midiinfo", MidiInfo_f );
	Cmd_SetHelp( "midiinfo", "prints MIDI devices info" );

	//
	// open the MIDI IN port
	//
	if ( midiInOpen( &s_midiInfo.hMidiIn,
					 ( UINT ) in_mididevice->integer,
					 ( DWORD_PTR ) MidiInProc,
					 ( DWORD_PTR ) NULL,
					 CALLBACK_FUNCTION ) != MMSYSERR_NOERROR )
	{
		Com_Printf( "WARNING: could not open MIDI device %d: '%s'\n", in_mididevice->integer , s_midiInfo.caps[( int ) in_mididevice->value].szPname );
		return;
	}

	midiInStart( s_midiInfo.hMidiIn );
}

static void IN_ShutdownMIDI()
{
	if ( s_midiInfo.hMidiIn )
		midiInClose( s_midiInfo.hMidiIn );

	Com_Memset( &s_midiInfo, 0, sizeof( s_midiInfo ) );
	Cmd_RemoveCommand( "midiinfo" );
}


/*
=========================================================================

MINIMIZE HOTKEY

=========================================================================
*/


#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif


static void WIN_RegisterHotKey( UINT modifiers, UINT key )
{
	if ( g_wv.minimizeHotKeyValid )
		return;

	// the return value, when not 0, is guaranteed to be in
	// the proper range for RegisterHotKey
	const int atom = (int)GlobalAddAtom( "cnq3_minimize_hotkey" );
	if ( atom == 0 )
		return;

	if ( RegisterHotKey(g_wv.hWnd, atom, modifiers | MOD_NOREPEAT, key) == 0 )
	{
		GlobalDeleteAtom( (ATOM)atom );
		Com_Printf( "ERROR: the in_minimize hotkey registration failed\n" );
		return;
	}

	g_wv.minimizeHotKeyValid = qtrue;
	g_wv.minimizeHotKeyId = atom;
	g_wv.minimizeHotKeyKey = key;
	g_wv.minimizeHotKeyMods = modifiers;
}


void WIN_RegisterLastValidHotKey()
{
	if ( g_wv.minimizeHotKeyKey != 0 )
		WIN_RegisterHotKey( g_wv.minimizeHotKeyMods, g_wv.minimizeHotKeyKey );
}


void WIN_UnregisterHotKey()
{
	if ( !g_wv.minimizeHotKeyValid )
		return;

	UnregisterHotKey( g_wv.hWnd, g_wv.minimizeHotKeyId );
	GlobalDeleteAtom( (ATOM)g_wv.minimizeHotKeyId );
	g_wv.minimizeHotKeyValid = qfalse;
	g_wv.minimizeHotKeyId = 0;
}


typedef struct {
	const char*	name;
	UINT		key; // or flag
	qbool		reserved;
} win_keyMap_t;


#define KEY(Name, Key) { Name, Key, qfalse }
#define KEYR(Name, Key) { Name, Key, qtrue }
static const win_keyMap_t win_stdKeyMaps[] =
{
	KEYR("back",		VK_BACK),
	KEYR("backspace",	VK_BACK),
	KEY("tab",			VK_TAB),
	KEYR("return",		VK_RETURN),
	KEYR("enter",		VK_RETURN),
	KEY("pause",		VK_PAUSE),
	KEY("capital",		VK_CAPITAL),
	KEY("caps",			VK_CAPITAL),
	KEY("capslock",		VK_CAPITAL),
	KEYR("esc",			VK_ESCAPE),
	KEYR("escape",		VK_ESCAPE),
	KEYR("space",		VK_SPACE),
	KEY("pageup",		VK_PRIOR),
	KEY("pgup",			VK_PRIOR),
	KEY("pagedown",		VK_NEXT),
	KEY("pgdn",			VK_NEXT),
	KEY("end",			VK_END),
	KEY("home",			VK_HOME),
	KEY("left",			VK_LEFT),
	KEY("leftarrow",	VK_LEFT),
	KEY("up",			VK_UP),
	KEY("uparrow",		VK_UP),
	KEY("right",		VK_RIGHT),
	KEY("rightarrow",	VK_RIGHT),
	KEY("down",			VK_DOWN),
	KEY("downarrow",	VK_DOWN),
	KEY("select",		VK_SELECT),
	KEY("print",		VK_PRINT),
	KEY("snapshot",		VK_SNAPSHOT),
	KEY("printscreen",	VK_SNAPSHOT),
	KEY("prtsc",		VK_SNAPSHOT),
	KEY("insert",		VK_INSERT),
	KEY("ins",			VK_INSERT),
	KEYR("delete",		VK_DELETE),
	KEYR("del",			VK_DELETE),
	KEY("kpmultiply",	VK_MULTIPLY),
	KEY("kpmul",		VK_MULTIPLY),
	KEY("kpadd",		VK_ADD),
	KEY("kpsubtract",	VK_SUBTRACT),
	KEY("kpsub",		VK_SUBTRACT),
	KEY("kpdecimal",	VK_DECIMAL),
	KEY("kpdec",		VK_DECIMAL),
	KEY("kpdelete",		VK_DECIMAL),
	KEY("kpdel",		VK_DECIMAL),
	KEY("kpdivide",		VK_DIVIDE),
	KEY("kpdiv",		VK_DIVIDE),
	KEY("numlock",		VK_NUMLOCK),
	KEY("kpnumlock",	VK_NUMLOCK),
	KEY("kplock",		VK_NUMLOCK),
	KEY("scrolllock",	VK_SCROLL),
	KEY("scroll",		VK_SCROLL),
	KEY("oem1",			VK_OEM_1),
	KEY(";",			VK_OEM_1),
	KEY(":",			VK_OEM_1),
	KEY("oem2",			VK_OEM_2),
	KEY("/",			VK_OEM_2),
	KEY("?",			VK_OEM_2),
	KEY("oem3",			VK_OEM_3),
	KEY("backtick",		VK_OEM_3),
	KEY("tilde",		VK_OEM_3),
	KEY("oem4",			VK_OEM_4),
	KEY("[",			VK_OEM_4),
	KEY("{",			VK_OEM_4),
	KEY("oem5",			VK_OEM_5),
	KEY("\\",			VK_OEM_5),
	KEY("|",			VK_OEM_5),
	KEY("oem6",			VK_OEM_6),
	KEY("]",			VK_OEM_6),
	KEY("}",			VK_OEM_6),
	KEY("oem7",			VK_OEM_7),
	KEY("\"",			VK_OEM_7),
	KEY("'",			VK_OEM_7),
	KEY("oem8",			VK_OEM_8),
	KEY("oem102",		VK_OEM_102),
	KEY("+",			VK_OEM_PLUS),
	KEY("=",			VK_OEM_PLUS),
	KEY(",",			VK_OEM_COMMA),
	KEY("<",			VK_OEM_COMMA),
	KEY("-",			VK_OEM_MINUS),
	KEY("_",			VK_OEM_MINUS),
	KEY(".",			VK_OEM_PERIOD),
	KEY(">",			VK_OEM_PERIOD),
	KEY("!",			'1'),
	KEY("@",			'2'),
	KEY("#",			'3'),
	KEY("$",			'4'),
	KEY("%",			'5'),
	KEY("^",			'6'),
	KEY("&",			'7'),
	KEY("*",			'8'),
	KEY("(",			'9'),
	KEY(")",			'0'),
	KEY("kp0",			0x60),
	KEY("kp1",			0x61),
	KEY("kp2",			0x62),
	KEY("kp3",			0x63),
	KEY("kp4",			0x64),
	KEY("kp5",			0x65),
	KEY("kp6",			0x66),
	KEY("kp7",			0x67),
	KEY("kp8",			0x68),
	KEY("kp9",			0x69),
	KEY("f1",			0x70),
	KEY("f2",			0x71),
	KEY("f3",			0x72),
	KEY("f4",			0x73),
	KEY("f5",			0x74),
	KEY("f6",			0x75),
	KEY("f7",			0x76),
	KEY("f8",			0x77),
	KEY("f9",			0x78),
	KEY("f10",			0x79),
	KEY("f11",			0x7A),
	KEY("f12",			0x7B)
};
#undef KEY


#define MOD(Name, Flag) { Name, Flag }
static const win_keyMap_t win_modKeyMaps[] =
{
	MOD("alt",		MOD_ALT),
	MOD("ctl",		MOD_CONTROL),
	MOD("ctrl",		MOD_CONTROL),
	MOD("control",	MOD_CONTROL),
	MOD("shift",	MOD_SHIFT),
	MOD("win",		MOD_WIN),
	MOD("windows",	MOD_WIN)
};
#undef MOD


typedef struct {
	const char*	name;
	UINT		key;
	UINT		mod;
} win_hotKey_t;


#define KEY(Name, Key, Mod) { Name, Key, Mod }
static const win_hotKey_t win_allowedSingleKeys[] =
{
	KEY("alt",		VK_MENU,	MOD_ALT),
	KEY("ctl",		VK_CONTROL,	MOD_CONTROL),
	KEY("ctrl",		VK_CONTROL,	MOD_CONTROL),
	KEY("control",	VK_CONTROL,	MOD_CONTROL),
	KEY("shift",	VK_SHIFT,	MOD_SHIFT)
};
#undef KEY


#define KEY(Mod, Key, Name) { Name, Key, Mod }
static const win_hotKey_t win_reservedHotkeys[] =
{
	KEY(MOD_ALT,				VK_RETURN,	"fullscreen toggle"),
	KEY(MOD_ALT,				VK_TAB,		"task switch"),
	KEY(MOD_WIN,				'D',		"show desktop toggle"),
	KEY(MOD_WIN,				'M',		"minimize everything"),
	KEY(MOD_CONTROL | MOD_ALT,	VK_DELETE,	"log-in screen / task manager")
};
#undef KEY


static qbool WIN_ParseKey( UINT* key, qbool* reserved, const char* name, const win_keyMap_t* keyMaps, int keyCount, qbool modsOnly )
{
	if ( !modsOnly )
	{
		const int l = (int)strlen( name );

		if ( l == 1 )
		{
			const char c = name[0];

			// digits
			if ( c >= 0x30 && c <= 0x39 )
			{
				*key = (UINT)c;
				*reserved = qtrue;
				return qtrue;
			}
			// uppercase letters
			else if ( c >= 0x41 && c <= 0x5A )
			{
				*key = (UINT)c;
				*reserved = qtrue;
				return qtrue;
			}
			// lowercase letters
			else if ( c >= 0x61 && c <= 0x7A )
			{
				*key = (UINT)c - 0x20;
				*reserved = qtrue;
				return qtrue;
			}
		}
	}

	for ( int i = 0; i < keyCount; ++i )
	{
		if ( Q_stricmp( name, keyMaps[i].name ) == 0 )
		{
			*key = keyMaps[i].key;
			*reserved = keyMaps[i].reserved;
			return qtrue;
		}
	}

	return qfalse;
}


static qbool WIN_IsAllowedSingleHotKey( win_hotKey_t* hotKey, const char* name )
{
	const int keyCount = ARRAY_LEN( win_allowedSingleKeys );
	for(int i = 0; i < keyCount; ++i)
	{
		if ( Q_stricmp( name, win_allowedSingleKeys[i].name ) == 0 )
		{
			*hotKey = win_allowedSingleKeys[i];
			return qtrue;
		}
	}

	UINT key;
	qbool reserved;
	if ( WIN_ParseKey( &key, &reserved, name, win_stdKeyMaps, ARRAY_LEN(win_stdKeyMaps), qfalse ) &&
		!reserved )
	{
		hotKey->key = key;
		hotKey->mod = 0;
		hotKey->name = "";
		return qtrue;
	}

	return qfalse;
}


static qbool WIN_IsReservedHotKey( win_hotKey_t* hotKey, UINT key, UINT mods )
{
	const int keyCount = ARRAY_LEN( win_reservedHotkeys );
	for(int i = 0; i < keyCount; ++i)
	{
		if ( key == win_reservedHotkeys[i].key &&
			 mods == win_reservedHotkeys[i].mod )
		{
			*hotKey = win_reservedHotkeys[i];
			return qtrue;
		}
	}

	return qfalse;
}


static void WIN_PrintHotKeyHelp()
{
	Com_Printf( "The key names must be separated by whitespace,\n" );
	Com_Printf( "so use double quotes to enclose the key list.\n" );
	Com_Printf( "To print the key names, use the minimizekeynames command.\n" );
}


static qbool WIN_ParseHotKey( UINT* key, UINT* modifiers )
{
	Cmd_TokenizeString( in_minimize->string );
	const int count = Cmd_Argc();
	if ( count <= 0 )
		return qfalse;

	if ( count == 1 )
	{
		win_hotKey_t hotKey;
		if( !WIN_IsAllowedSingleHotKey( &hotKey, Cmd_Argv(0) ) )
		{
			Com_Printf( "ERROR: this key can't be used alone\n" );
			WIN_PrintHotKeyHelp();
			return qfalse;
		}

		Com_Printf( "the key '%s' will be registered alone as a hotkey\n", Cmd_Argv(0) );
		*key = hotKey.key;
		*modifiers = hotKey.mod;
		return qtrue;
	}

	*modifiers = 0;
	qbool keyFound = qfalse;
	for ( int i = 0; i < count; ++i )
	{
		const char* const keyName = Cmd_Argv(i);
		if ( *keyName == '\0' )
			continue;

		qbool reserved;
		if ( WIN_ParseKey( key, &reserved, keyName, win_stdKeyMaps, ARRAY_LEN(win_stdKeyMaps), qfalse ) )
		{
			if ( keyFound ) // we allow at most 1 normal key
			{
				Com_Printf( "ERROR: in_minimize specified more than 1 non-modifier key\n" );
				WIN_PrintHotKeyHelp();
				return qfalse;
			}
			keyFound = qtrue;
			continue;
		}

		UINT mod;
		if ( WIN_ParseKey( &mod, &reserved, keyName, win_modKeyMaps, ARRAY_LEN(win_modKeyMaps), qtrue ) )
		{
			*modifiers |= mod;
		}
		else
		{
			Com_Printf( "ERROR: in_minimize doesn't recognize key name '%s'\n", keyName );
			WIN_PrintHotKeyHelp();
			return qfalse;
		}
	}

	if ( !keyFound )
	{
		Com_Printf("ERROR: in_minimize didn't specify a non-modifier key\n");
		WIN_PrintHotKeyHelp();
		return qfalse;
	}

	int modCount = 0;
	UINT mods = *modifiers;
	for ( UINT i = 0; i < 32; ++i )
	{
		if ((mods & 1) != 0 )
			++modCount;
		mods >>= 1;
	}

	// we require either 1 or 2 modifier keys
	if ( modCount < 1 || modCount > 2 )
	{
		Com_Printf("ERROR: in_minimize must specify at most 2 modifier keys\n");
		WIN_PrintHotKeyHelp();
		return qfalse;
	}

	// don't use reserved key combos
	win_hotKey_t hotKey;
	if( WIN_IsReservedHotKey( &hotKey, *key, *modifiers ) )
	{
		Com_Printf( "ERROR: this combo is reserved for: %s\n", hotKey.name );
		return qfalse;
	}

	return qtrue;
}


static void WIN_PrintKeyNames( const win_keyMap_t* keys, int keyCount )
{
	const int columnCount = 4;
	const int lineCount = (keyCount + columnCount - 1) / columnCount;

	int maxColumnWidths[columnCount];
	for ( int x = 0; x < columnCount; ++x )
	{
		int maxWidth = 0;
		for ( int y = 0; y < lineCount; ++y )
		{
			const int k = x * lineCount + y;
			if ( k >= keyCount )
				break;

			const int l = (int)strlen( keys[k].name );
			maxWidth = max( maxWidth, l );
		}

		maxColumnWidths[x] = maxWidth;
	}

	for ( int y = 0; y < lineCount; ++y )
	{
		Com_Printf( "  " );

		for ( int x = 0; x < columnCount; ++x )
		{
			const int k = x * lineCount + y;
			if ( k >= keyCount )
				break;

			const char* n = keys[k].name;
			const int l = (int)strlen(n);
			Com_Printf(n);

			const int spaceCount = maxColumnWidths[x] - l + 2;
			for ( int s = 0; s < spaceCount; ++s )
				Com_Printf( " " );
		}

		Com_Printf( "\n" );
	}
}


static void WIN_PrintMinimizeKeyNames()
{
	Com_Printf( "Key Names:\n" );
	WIN_PrintKeyNames( win_stdKeyMaps, ARRAY_LEN( win_stdKeyMaps ) );
	Com_Printf( "\n" );
	Com_Printf( "Modifier Key Names:\n" );
	WIN_PrintKeyNames( win_modKeyMaps, ARRAY_LEN( win_modKeyMaps ) );
}


static void IN_StartupHotKey( qbool fullStartUp )
{
	in_minimize = Cvar_Get( "in_minimize", "", CVAR_ARCHIVE );
	Cvar_SetHelp( "in_minimize", help_in_minimize );
	Cvar_SetModule( "in_minimize", MODULE_INPUT );
	in_minimize->modified = qfalse;

	if ( fullStartUp )
	{
		Cmd_AddCommand( "minimizekeynames", &WIN_PrintMinimizeKeyNames );
		Cmd_SetHelp( "minimizekeynames", "prints all key names usable with in_minimize" );
		Cmd_SetModule( "minimizekeynames", MODULE_INPUT );
	}

	WIN_UnregisterHotKey();

	if ( in_minimize->string[0] == '\0' )
		return;

	UINT key, modifiers;
	if ( !WIN_ParseHotKey( &key, &modifiers ) )
		return;

	WIN_RegisterHotKey( modifiers, key );
}


static void IN_ShutDownHotKey()
{
	WIN_UnregisterHotKey();
}


static void IN_UpdateHotKey()
{
	if ( !in_minimize->modified )
		return;

	IN_StartupHotKey( qfalse );
}

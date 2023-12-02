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
/*
** WIN_GLIMP.C
**
** This file contains ALL Win32 specific stuff having to do with the
** OpenGL refresh. When a port is being made the following functions
** must be implemented by the port:
**
** Sys_V_EndFrame
** Sys_V_Init
** Sys_V_Shutdown
** Sys_V_IsVSynced
**
** Note that the GLW_xxx functions are Windows specific GL-subsystem
** related functions that are relevant ONLY to win_glimp.c
*/

#if _MSC_VER
#pragma warning (disable: 4996) // deprecated ZOMGOVERRUN! nannywhine
#endif

#include "../renderer/tr_local.h"
#include "../client/client.h"
#include "resource.h"
#include "win_local.h"
#include "GL/glew.h"
#include "GL/wglew.h"
#include "glw_win.h"


#define TRY_PFD_SUCCESS		0
#define TRY_PFD_FAIL_SOFT	1
#define TRY_PFD_FAIL_HARD	2

glwstate_t glw_state;
static galId_t win_galId;


static qbool WIN_UsingOpenGL()
{
	switch ( win_galId )
	{
		case GAL_GL2:
		case GAL_GL3:
			return qtrue;
		default:
			return qfalse;
	}
}


/*
** ChoosePFD
**
** Helper function that replaces ChoosePixelFormat.
*/
#define MAX_PFDS 256

static int GLW_ChoosePFD( HDC hDC, PIXELFORMATDESCRIPTOR *pPFD )
{
	PIXELFORMATDESCRIPTOR pfds[MAX_PFDS+1];
	int maxPFD = 0;
	int i;
	int bestMatch = 0;

	ri.Printf( PRINT_DEVELOPER, "...GLW_ChoosePFD( %d, %d, %d )\n", ( int ) pPFD->cColorBits, ( int ) pPFD->cDepthBits, ( int ) pPFD->cStencilBits );

	maxPFD = DescribePixelFormat( hDC, 1, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[0] );
	if ( maxPFD > MAX_PFDS )
	{
		ri.Printf( PRINT_WARNING, "...numPFDs > MAX_PFDS (%d > %d)\n", maxPFD, MAX_PFDS );
		maxPFD = MAX_PFDS;
	}

	ri.Printf( PRINT_DEVELOPER, "...%d PFDs found\n", maxPFD - 1 );

	// grab information
	for ( i = 1; i <= maxPFD; i++ )
	{
		DescribePixelFormat( hDC, i, sizeof( PIXELFORMATDESCRIPTOR ), &pfds[i] );
	}

	// look for a best match
	for ( i = 1; i <= maxPFD; i++ )
	{
		//
		// make sure this has hardware acceleration
		//
		if ( ( pfds[i].dwFlags & PFD_GENERIC_FORMAT ) != 0 ) 
		{
			continue;
		}

		// verify pixel type
		if ( pfds[i].iPixelType != PFD_TYPE_RGBA )
		{
			if ( r_verbose->integer )
			{
				ri.Printf( PRINT_DEVELOPER, "...PFD %d rejected, not RGBA\n", i );
			}
			continue;
		}

		// verify proper flags
		if ( ( ( pfds[i].dwFlags & pPFD->dwFlags ) & pPFD->dwFlags ) != pPFD->dwFlags ) 
		{
			if ( r_verbose->integer )
			{
				ri.Printf( PRINT_DEVELOPER, "...PFD %d rejected, improper flags (%x instead of %x)\n", i, pfds[i].dwFlags, pPFD->dwFlags );
			}
			continue;
		}

		// verify enough bits
		if ( pfds[i].cDepthBits < 15 )
		{
			continue;
		}
		if ( ( pfds[i].cStencilBits < 4 ) && ( pPFD->cStencilBits > 0 ) )
		{
			continue;
		}

		//
		// selection criteria (in order of priority):
		// 
		//  colorBits
		//  depthBits
		//  stencilBits
		//
		if ( bestMatch )
		{
			// check color
			if ( pfds[bestMatch].cColorBits != pPFD->cColorBits )
			{
				// prefer perfect match
				if ( pfds[i].cColorBits == pPFD->cColorBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cColorBits > pfds[bestMatch].cColorBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check depth
			if ( pfds[bestMatch].cDepthBits != pPFD->cDepthBits )
			{
				// prefer perfect match
				if ( pfds[i].cDepthBits == pPFD->cDepthBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( pfds[i].cDepthBits > pfds[bestMatch].cDepthBits )
				{
					bestMatch = i;
					continue;
				}
			}

			// check stencil
			if ( pfds[bestMatch].cStencilBits != pPFD->cStencilBits )
			{
				// prefer perfect match
				if ( pfds[i].cStencilBits == pPFD->cStencilBits )
				{
					bestMatch = i;
					continue;
				}
				// otherwise if this PFD has more bits than our best, use it
				else if ( ( pfds[i].cStencilBits > pfds[bestMatch].cStencilBits ) && 
					 ( pPFD->cStencilBits > 0 ) )
				{
					bestMatch = i;
					continue;
				}
			}
		}
		else
		{
			bestMatch = i;
		}
	}

	if ( !bestMatch )
		return 0;

	if ( pfds[bestMatch].dwFlags & (PFD_GENERIC_FORMAT | PFD_GENERIC_ACCELERATED) ) {
		ri.Printf( PRINT_ALL, "...no OpenGL ICD found\n" );
		return 0;
	}

	*pPFD = pfds[bestMatch];

	return bestMatch;
}

/*
** void GLW_CreatePFD
**
** Helper function zeros out then fills in a PFD
*/
static void GLW_CreatePFD( PIXELFORMATDESCRIPTOR *pPFD )
{
	ZeroMemory( pPFD, sizeof( *pPFD ) );
	pPFD->nSize = sizeof( *pPFD );
	pPFD->dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pPFD->iPixelType	= PFD_TYPE_RGBA;
	pPFD->iLayerType	= PFD_MAIN_PLANE;
	pPFD->cColorBits	= 32;
	pPFD->cDepthBits	= 24;
	pPFD->cStencilBits	= 8;
}


static int GLW_MakeContext( PIXELFORMATDESCRIPTOR *pPFD )
{
	int pixelformat;

	//
	// don't putz around with pixelformat if it's already set (e.g. this is a soft
	// reset of the graphics system)
	//
	if ( !glw_state.pixelFormatSet )
	{
		//
		// choose, set, and describe our desired pixel format.  If we're
		// using a minidriver then we need to bypass the GDI functions,
		// otherwise use the GDI functions.
		//
		if (glw_state.nPendingPF)
			pixelformat = glw_state.nPendingPF;
		else
		if ( ( pixelformat = GLW_ChoosePFD( glw_state.hDC, pPFD ) ) == 0 )
		{
			ri.Printf( PRINT_ALL, "...GLW_ChoosePFD failed\n");
			return TRY_PFD_FAIL_SOFT;
		}
		ri.Printf( PRINT_DEVELOPER, "...PIXELFORMAT %d selected\n", pixelformat );

		DescribePixelFormat( glw_state.hDC, pixelformat, sizeof( *pPFD ), pPFD );

		if ( SetPixelFormat( glw_state.hDC, pixelformat, pPFD ) == FALSE )
		{
			ri.Printf( PRINT_ALL, "...SetPixelFormat failed\n", glw_state.hDC );
			return TRY_PFD_FAIL_SOFT;
		}

		glw_state.pixelFormatSet = qtrue;
	}

	//
	// startup the OpenGL subsystem by creating a context and making it current
	//
	if ( !glw_state.hGLRC )
	{
		if ( ( glw_state.hGLRC = wglCreateContext( glw_state.hDC ) ) == 0 )
		{
			ri.Printf( PRINT_ALL, "...GL context creation failure\n" );
			return TRY_PFD_FAIL_HARD;
		}
		ri.Printf( PRINT_DEVELOPER, "...GL context created\n" );

		if ( !wglMakeCurrent( glw_state.hDC, glw_state.hGLRC ) )
		{
			wglDeleteContext( glw_state.hGLRC );
			glw_state.hGLRC = NULL;
			ri.Printf( PRINT_ALL, "...GL context creation currency failure\n" );
			return TRY_PFD_FAIL_HARD;
		}
		ri.Printf( PRINT_DEVELOPER, "...GL context creation made current\n" );

		if ( win_galId == GAL_GL3 )
		{
			PFNWGLCREATECONTEXTATTRIBSARBPROC const wglCreateContextAttribsARB =
				(PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress( "wglCreateContextAttribsARB" );
			if( wglCreateContextAttribsARB == NULL )
				ri.Error( ERR_FATAL, "wglCreateContextAttribsARB not found: can't ask for core 3.2 context\n" );

			const int attribs[] =
			{
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 2,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				WGL_CONTEXT_FLAGS_ARB, CL_GL_WantDebug() ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
				0
			};

			const HGLRC hGLRC = (*wglCreateContextAttribsARB)( glw_state.hDC, NULL, attribs );
			if ( hGLRC )
			{
				wglMakeCurrent( NULL, NULL );
				wglDeleteContext( glw_state.hGLRC );
				wglMakeCurrent( glw_state.hDC, hGLRC );
				glw_state.hGLRC = hGLRC;
				ri.Printf( PRINT_ALL, "OpenGL version upgraded to: %s\n", (const char*)glGetString( GL_VERSION ) );
			}
			else
			{
				ri.Printf( PRINT_WARNING, "wglCreateContextAttribsARB failed\n" );
			}
		}

		if ( glewInit() != GLEW_OK )
			ri.Error( ERR_FATAL, "glewInit failed\n" );

		CL_GL_Init();
	}

	return TRY_PFD_SUCCESS;
}


/*
** - get a DC if one doesn't exist
** - create an HGLRC if one doesn't exist
*/
static qbool GLW_InitDriver()
{
	int		tpfd;
	static PIXELFORMATDESCRIPTOR pfd;		// save between frames since 'tr' gets cleared

	ri.Printf( PRINT_DEVELOPER, "Initializing OpenGL driver\n" );

	//
	// get a DC for our window if we don't already have one allocated
	//
	if ( glw_state.hDC == NULL )
	{
		if ( ( glw_state.hDC = GetDC( g_wv.hWnd ) ) == NULL )
		{
			ri.Printf( PRINT_ALL, "...Get DC failed\n" );
			return qfalse;
		}
		ri.Printf( PRINT_DEVELOPER, "...Get DC succeeded\n" );
	}

	//
	// attempt to set the PIXELFORMAT
	//
	if ( !glw_state.pixelFormatSet )
	{
		GLW_CreatePFD( &pfd );
		if ( ( tpfd = GLW_MakeContext( &pfd ) ) != TRY_PFD_SUCCESS )
		{
			if ( tpfd == TRY_PFD_FAIL_HARD )
			{
				ri.Printf( PRINT_WARNING, "...failed hard\n" );
				return qfalse;
			}

			ReleaseDC( g_wv.hWnd, glw_state.hDC );
			glw_state.hDC = NULL;
			ri.Printf( PRINT_ALL, "...failed to find an appropriate PIXELFORMAT\n" );
			return qfalse;
		}
	}

	// store PFD specifics
	//
	glConfig.colorBits = ( int ) pfd.cColorBits;
	glConfig.depthBits = ( int ) pfd.cDepthBits;
	glConfig.stencilBits = ( int ) pfd.cStencilBits;

	return qtrue;
}


// responsible for creating the Win32 window and initializing the OpenGL driver.

static qbool GLW_CreateWindow()
{
	static qbool s_classRegistered = qfalse;

	if ( !s_classRegistered )
	{
		WNDCLASS wc;
		memset( &wc, 0, sizeof( wc ) );

		wc.style         = CS_OWNDC;
		wc.lpfnWndProc   = MainWndProc;
		wc.cbClsExtra    = 0;
		wc.cbWndExtra    = 0;
		wc.hInstance     = g_wv.hInstance;
		wc.hIcon         = LoadIcon( g_wv.hInstance, MAKEINTRESOURCE(IDI_ICON1));
		wc.hCursor       = LoadCursor (NULL,IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszMenuName  = 0;
		wc.lpszClassName = CLIENT_WINDOW_TITLE;

		if ( !RegisterClass( &wc ) )
			ri.Error( ERR_FATAL, "GLW_CreateWindow: could not register window class" );

		s_classRegistered = qtrue;
		ri.Printf( PRINT_DEVELOPER, "...registered window class\n" );
	}

	//
	// create the HWND if one does not already exist
	//
	if ( !g_wv.hWnd )
	{
		g_wv.inputInitialized = qfalse;

		RECT r;
		r.left = 0;
		r.top = 0;
		r.right  = glInfo.winWidth;
		r.bottom = glInfo.winHeight;

		int style = WS_VISIBLE | WS_CLIPCHILDREN;
		int exstyle = 0;

		if ( glInfo.winFullscreen )
		{
			style |= WS_POPUP;
			exstyle |= WS_EX_TOPMOST;
		}
		else
		{
			style |= WS_OVERLAPPED | WS_BORDER | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
			AdjustWindowRect( &r, style, FALSE );
		}

		const int w = r.right - r.left;
		const int h = r.bottom - r.top;

		const RECT& monRect = g_wv.monitorRects[g_wv.monitor];

		int dx = 0;
		int dy = 0;

		if ( !glInfo.winFullscreen )
		{
			dx = ri.Cvar_Get( "vid_xpos", "0", 0 )->integer;
			dy = ri.Cvar_Get( "vid_ypos", "0", 0 )->integer;
			dx = Com_ClampInt( 0, max( 0, monRect.right - monRect.left - w ), dx );
			dy = Com_ClampInt( 0, max( 0, monRect.bottom - monRect.top - h ), dy );
		}

		const int x = monRect.left + dx;
		const int y = monRect.top + dy;

		g_wv.duringCreateWindow = qtrue;
		g_wv.hWnd = CreateWindowEx( exstyle, CLIENT_WINDOW_TITLE, " " CLIENT_WINDOW_TITLE, style,
				x, y, w, h, NULL, NULL, g_wv.hInstance, NULL );
		g_wv.duringCreateWindow = qfalse;

		if ( !g_wv.hWnd )
			ri.Error( ERR_FATAL, "GLW_CreateWindow() - Couldn't create window" );

		ShowWindow( g_wv.hWnd, SW_SHOW );
		UpdateWindow( g_wv.hWnd );
		ri.Printf( PRINT_DEVELOPER, "...created window@%d,%d (%dx%d)\n", x, y, w, h );
	}
	else
	{
		ri.Printf( PRINT_DEVELOPER, "...window already present, CreateWindowEx skipped\n" );
	}

	if ( WIN_UsingOpenGL() && !GLW_InitDriver() )
	{
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;
		return qfalse;
	}

	SetForegroundWindow( g_wv.hWnd );
	SetFocus( g_wv.hWnd );

	return qtrue;
}


static const char* GLW_GetCurrentDisplayDeviceName()
{
	static char deviceName[CCHDEVICENAME + 1];

	const HMONITOR hMonitor = g_wv.hMonitors[g_wv.monitor];
	if ( hMonitor == NULL )
		return NULL;

	MONITORINFOEXA info;
	ZeroMemory( &info, sizeof(info) );
	info.cbSize = sizeof(info);
	if ( GetMonitorInfoA(hMonitor, &info) == 0 )
		return NULL;

	Q_strncpyz( deviceName, info.szDevice, sizeof(deviceName) );
	
	return deviceName;
}


static void GLW_UpdateMonitorRect( const char* deviceName )
{
	if ( deviceName == NULL )
		return;

	DEVMODEA dm;
	ZeroMemory( &dm, sizeof(dm) );
	dm.dmSize = sizeof(dm);
	if ( EnumDisplaySettingsExA(deviceName, ENUM_CURRENT_SETTINGS, &dm, 0) == 0 )
		return;

	if ( dm.dmPelsWidth == 0 || dm.dmPelsHeight == 0 )
		return;

	// Normally, we should check dm.dmFields for the following flags:
	// DM_POSITION DM_PELSWIDTH DM_PELSHEIGHT
	// EnumDisplaySettingsExA doesn't always set up the flags properly.

	RECT& rect = g_wv.monitorRects[g_wv.monitor];
	rect.left = dm.dmPosition.x;
	rect.top = dm.dmPosition.y;
	rect.right = dm.dmPosition.x + dm.dmPelsWidth;
	rect.bottom = dm.dmPosition.y + dm.dmPelsHeight;
}


static qbool GLW_SetDisplaySettings( DEVMODE& dm )
{
	const char* deviceName = GLW_GetCurrentDisplayDeviceName();
	const int ec = ChangeDisplaySettingsExA( deviceName, &dm, NULL, CDS_FULLSCREEN, NULL );
	if ( ec == DISP_CHANGE_SUCCESSFUL )
	{
		glw_state.cdsDevMode = dm;
		glw_state.cdsDevModeValid = qtrue;
		GLW_UpdateMonitorRect( deviceName );
		return qtrue;
	}

	glw_state.cdsDevModeValid = qfalse;

	ri.Printf( PRINT_ALL, "...CDS: %ix%i (C%i) failed: ", (int)dm.dmPelsWidth, (int)dm.dmPelsHeight, (int)dm.dmBitsPerPel );

#define CDS_ERROR(x) case x: ri.Printf( PRINT_ALL, #x##"\n" ); break;
	switch (ec) {
		default:
			ri.Printf( PRINT_ALL, "unknown error %d\n", ec );
			break;
		CDS_ERROR( DISP_CHANGE_RESTART );
		CDS_ERROR( DISP_CHANGE_BADPARAM );
		CDS_ERROR( DISP_CHANGE_BADFLAGS );
		CDS_ERROR( DISP_CHANGE_FAILED );
		CDS_ERROR( DISP_CHANGE_BADMODE );
		CDS_ERROR( DISP_CHANGE_NOTUPDATED );
	}
#undef CDS_ERROR

	return qfalse;
}


static void GLW_ResetDisplaySettings( qbool invalidate )
{
	const char* deviceName = GLW_GetCurrentDisplayDeviceName();
	ChangeDisplaySettingsEx( deviceName, NULL, NULL, 0, NULL );
	GLW_UpdateMonitorRect( deviceName );
	if ( invalidate )
		glw_state.cdsDevModeValid = qfalse;
}


void WIN_SetGameDisplaySettings()
{
	if ( glw_state.cdsDevModeValid )
		GLW_SetDisplaySettings( glw_state.cdsDevMode );
}


void WIN_SetDesktopDisplaySettings()
{
	// We don't invalidate glw_state.cdsDevModeValid so we can
	// return to the previous mode later.
	GLW_ResetDisplaySettings( qfalse );
}


static qbool GLW_SetMode()
{
	WIN_InitMonitorList();
	WIN_UpdateMonitorIndexFromCvar();

	const RECT& monRect = g_wv.monitorRects[g_wv.monitor];
	const int desktopWidth = (int)(monRect.right - monRect.left);
	const int desktopHeight = (int)(monRect.bottom - monRect.top);
	R_ConfigureVideoMode( desktopWidth, desktopHeight );

	DEVMODE dm;
	ZeroMemory( &dm, sizeof( dm ) );
	dm.dmSize = sizeof( dm );

	if (glInfo.vidFullscreen != glw_state.cdsDevModeValid) {
		if (glInfo.vidFullscreen) {
			dm.dmPelsWidth  = glConfig.vidWidth;
			dm.dmPelsHeight = glConfig.vidHeight;
			dm.dmFields     = DM_PELSWIDTH | DM_PELSHEIGHT;

			if ( r_displayRefresh->integer ) {
				dm.dmDisplayFrequency = r_displayRefresh->integer;
				dm.dmFields |= DM_DISPLAYFREQUENCY;
			}

			dm.dmBitsPerPel = 32;
			dm.dmFields |= DM_BITSPERPEL;

			dm.dmPosition.x = monRect.left;
			dm.dmPosition.y = monRect.top;
			dm.dmFields |= DM_POSITION;

			glInfo.vidFullscreen = GLW_SetDisplaySettings( dm );
		}
		else
		{
			GLW_ResetDisplaySettings( qtrue );
		}
	}

	if (!GLW_CreateWindow())
		return qfalse;

	if (EnumDisplaySettingsA( GLW_GetCurrentDisplayDeviceName(), ENUM_CURRENT_SETTINGS, &dm ))
		glInfo.displayFrequency = dm.dmDisplayFrequency;

	return qtrue;
}


static qbool WIN_LoadGL()
{
	glw_state.hinstOpenGL = LoadLibraryA( "opengl32.dll" );
	if ( glw_state.hinstOpenGL == NULL )
		return qfalse;

	return qtrue;
}


static void WIN_UnloadGL()
{
	if ( glw_state.hinstOpenGL ) {
		FreeLibrary( glw_state.hinstOpenGL );
		glw_state.hinstOpenGL = NULL;
	}
}


static qbool GLW_LoadOpenGL()
{
	// only real GL implementations are acceptable
	// load the driver and bind our function pointers to it
	if ( WIN_LoadGL() ) {
		// create the window and set up the context
		if ( GLW_SetMode() ) {
			return qtrue;
		}
	}

	WIN_UnloadGL();

	return qfalse;
}


void Sys_V_EndFrame()
{
	if ( !WIN_UsingOpenGL() )
		return;

	if ( r_swapInterval->modified ) {
		r_swapInterval->modified = qfalse;

		if ( WGLEW_EXT_swap_control ) {
			int swapInterval = r_swapInterval->integer;
			if ( swapInterval < 0 && !WGLEW_EXT_swap_control_tear )
				swapInterval = -swapInterval;
			wglSwapIntervalEXT( swapInterval );
		}
	}

	SwapBuffers( glw_state.hDC );
}


void Sys_V_Init( galId_t type )
{
	win_galId = type;

	if ( WIN_UsingOpenGL() ) {
		ri.Printf( PRINT_DEVELOPER, "Initializing OpenGL subsystem\n" );

		// load appropriate DLL and initialize subsystem
		if ( !GLW_LoadOpenGL() )
			ri.Error( ERR_FATAL, "Sys_V_Init - could not load OpenGL subsystem\n" );
	} else {
		if ( !GLW_SetMode() )
			ri.Error( ERR_FATAL, "Sys_V_Init - could not load Direct3D subsystem\n" );
	}
}


static void Sys_ShutdownOpenGL()
{
	const char* success[] = { "failed", "success" };
	int retVal;

	ri.Printf( PRINT_DEVELOPER, "Shutting down OpenGL subsystem\n" );

	retVal = wglMakeCurrent( NULL, NULL ) != 0;
	ri.Printf( PRINT_DEVELOPER, "...wglMakeCurrent( NULL, NULL ): %s\n", success[retVal] );

	// delete HGLRC
	if ( glw_state.hGLRC )
	{
		retVal = wglDeleteContext( glw_state.hGLRC ) != 0;
		ri.Printf( PRINT_DEVELOPER, "...deleting GL context: %s\n", success[retVal] );
		glw_state.hGLRC = NULL;
	}

	// release DC
	if ( glw_state.hDC )
	{
		retVal = ReleaseDC( g_wv.hWnd, glw_state.hDC ) != 0;
		ri.Printf( PRINT_DEVELOPER, "...releasing DC: %s\n", success[retVal] );
		glw_state.hDC   = NULL;
	}
}


void Sys_V_Shutdown()
{
	Sys_ShutdownInput();

	if ( WIN_UsingOpenGL() )
		Sys_ShutdownOpenGL();

	// destroy window
	if ( g_wv.hWnd )
	{
		ri.Printf( PRINT_DEVELOPER, "...destroying window\n" );
		ShowWindow( g_wv.hWnd, SW_HIDE );
		DestroyWindow( g_wv.hWnd );
		g_wv.hWnd = NULL;
		glw_state.pixelFormatSet = qfalse;
	}

	// reset display settings
	if ( glw_state.cdsDevModeValid )
	{
		ri.Printf( PRINT_DEVELOPER, "...resetting display\n" );
		GLW_ResetDisplaySettings( qtrue );
	}

	// shutdown OpenGL subsystem
	if ( WIN_UsingOpenGL() )
		WIN_UnloadGL();
}


qbool Sys_V_IsVSynced()
{
	// with Direct3D, our swap interval is (normally) respected
	if ( !WIN_UsingOpenGL() )
		return r_swapInterval->integer != 0;

	// with OpenGL, our request is often ignored (driver control panel overrides)
	// unfortunately, the value returned here might not be what we want either...
	if ( WGLEW_EXT_swap_control )
	{
		const int interval = wglGetSwapIntervalEXT();
		if ( WGLEW_EXT_swap_control_tear )
			return interval != 0;
		else
			return interval > 0;
	}

	return qfalse;
}


qbool Sys_IsMinimized()
{
	return ( g_wv.hWnd != NULL ) && !!IsIconic( g_wv.hWnd );
}


void WIN_UpdateResolution( int width, int height )
{
	glInfo.winWidth = width;
	glInfo.winHeight = height;
	if ( r_fullscreen->integer == 0 )
	{
		glConfig.vidWidth = width;
		glConfig.vidHeight = height;
	}
}


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
#ifndef _WIN32
#  error You should not be including this file on this platform
#endif

#ifndef __GLW_WIN_H__
#define __GLW_WIN_H__

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct
{
	// The main window's rendering context is the only one we can ever keep around
	// because the window class has the CS_OWNDC style set (for OpenGL).
	HDC			hDC;
	HGLRC		hGLRC;
	HINSTANCE	hinstOpenGL;
	qbool		cdsDevModeValid;
	DEVMODE		cdsDevMode;			// Custom device mode for full-screen with r_mode 1.
	qbool		pixelFormatSet;
	int			nPendingPF;
} glwstate_t;

extern glwstate_t glw_state;

#if defined(__cplusplus)
};
#endif

#endif

/*
===========================================================================
Copyright (C) 2017-2019 Gian 'myT' Schellenbaum

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
// Linux video using SDL 2

#include "linux_local.h"
#include "linux_help.h"
#include "../renderer/tr_local.h"

#include <SDL2/SDL.h>
#include "sdl_local.h"
#include "GL/glew.h"


glImp_t glimp;


cvar_t* r_fullscreen;
static cvar_t* r_monitor;

static const cvarTableItem_t glimp_cvars[] = {
	{ &r_fullscreen, "r_fullscreen", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "full-screen mode" },
	{ &r_monitor, "r_monitor", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", NULL, help_r_monitor }
};

static void sdl_MonitorList_f();

static const cmdTableItem_t glimp_cmds[] = {
	{ "monitorlist", &sdl_MonitorList_f, NULL, "refreshes and prints the monitor list" }
};


static qbool sdl_IsMonitorListValid()
{
	const int count = glimp.monitorCount;
	const int curr = glimp.monitor;

	return
		count >= 1 && count <= MAX_MONITOR_COUNT &&
		curr >= 0 && curr < count;
}


static int sdl_CompareMonitors( const void* aPtr, const void* bPtr )
{
	const SDL_Rect* const a = &((const monitor_t*)aPtr)->rect;
	const SDL_Rect* const b = &((const monitor_t*)bPtr)->rect;
	const int dy = a->y - b->y;
	if (dy != 0)
		return dy;

	return a->x - b->x;
}


static void sdl_CreateMonitorList()
{
	glimp.monitorCount = 0;

	const int count = SDL_GetNumVideoDisplays();
	if (count <= 0)
		return;

	int gi = 0;
	for (int si = 0; si < count; ++si) {
		if (gi >= MAX_MONITOR_COUNT)
			break;
		if (SDL_GetDisplayBounds(si, &glimp.monitors[gi].rect) == 0) {
			glimp.monitors[gi].sdlIndex = si;
			++gi;
		}
	}
	glimp.monitorCount = gi;

	if (sdl_IsMonitorListValid())
		qsort(glimp.monitors, (size_t)glimp.monitorCount, sizeof(glimp.monitors[0]), &sdl_CompareMonitors);
	else
		glimp.monitorCount = 0;
}


// call this before creating the window
static void sdl_UpdateMonitorIndexFromCvar()
{
	if (glimp.monitorCount <= 0 || glimp.monitorCount >= MAX_MONITOR_COUNT)
		return;

	const int monitor = Cvar_Get("r_monitor", "0", CVAR_ARCHIVE | CVAR_LATCH)->integer;
	if (monitor < 0 || monitor >= glimp.monitorCount) {
		glimp.monitor = 0;
		return;
	}
	glimp.monitor = monitor;
}


// call this after the window has been moved
void sdl_UpdateMonitorIndexFromWindow()
{
	if (glimp.monitorCount <= 0)
		return;

	// try to find the glimp index and update data accordingly
	const int sdlIndex = SDL_GetWindowDisplayIndex(glimp.window);
	for (int i = 0; i < glimp.monitorCount; ++i) {
		if (glimp.monitors[i].sdlIndex == sdlIndex) {
			glimp.monitor = i;
			Cvar_Set("r_monitor", va("%d", i));
			break;
		}
	}
}


static void sdl_GetSafeDesktopRect( SDL_Rect* rect )
{
	if (!sdl_IsMonitorListValid()) {
		rect->x = 0;
		rect->y = 0;
		rect->w = 1280;
		rect->h = 720;
	}

	*rect = glimp.monitors[glimp.monitor].rect;
}


static void sdl_PrintMonitorList()
{
	const int count = glimp.monitorCount;
	if (count <= 0) {
		Com_Printf("No monitor detected.\n");
		return;
	}

	Com_Printf("Monitors detected (left is " S_COLOR_CVAR "r_monitor ^7value):\n");
	for (int i = 0; i < count; ++i) {
		const SDL_Rect rect = glimp.monitors[i].rect;
		Com_Printf(S_COLOR_VAL "%d ^7%dx%d at %d,%d\n", i, rect.w, rect.h, rect.x, rect.y);
	}
}


static void sdl_MonitorList_f()
{
	sdl_CreateMonitorList();
	sdl_UpdateMonitorIndexFromCvar();
	sdl_PrintMonitorList();
}


void Sys_V_Init( galId_t type )
{
	if (glimp.window != NULL)
		return;

	Cvar_RegisterArray(glimp_cvars, MODULE_CLIENT);

	static qbool firstInit = qtrue;
	if (firstInit) {
		Cmd_RegisterArray(glimp_cmds, MODULE_CLIENT);
		firstInit = qfalse;
	}

	sdl_CreateMonitorList();
	sdl_UpdateMonitorIndexFromCvar();
	sdl_PrintMonitorList();

	SDL_Rect deskropRect;
	sdl_GetSafeDesktopRect(&deskropRect);
	R_ConfigureVideoMode(deskropRect.w, deskropRect.h);

	Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	if (glInfo.winFullscreen) {
		if (glInfo.vidFullscreen)
			windowFlags |= SDL_WINDOW_FULLSCREEN;
		else
			windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}

	// SDL docs: "All three attributes must be set prior to creating the first window"
	const int debugFlags = CL_GL_WantDebug() ? SDL_GL_CONTEXT_DEBUG_FLAG : 0;
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
	SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
	if (type == GAL_GL3)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, debugFlags);
	}
	else
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, debugFlags);
	}

	// @TODO: make a cvar defaulting to an empty string for this? e.g. value: "libGL.so.1"
	if (SDL_GL_LoadLibrary(NULL) < 0)
		ri.Error(ERR_FATAL, "Sys_V_Init - SDL_GL_LoadLibrary failed: %s\n", SDL_GetError());

	glimp.window = SDL_CreateWindow("CNQ3", deskropRect.x, deskropRect.y, glConfig.vidWidth, glConfig.vidHeight, windowFlags);
	if (glimp.window == NULL)
		ri.Error(ERR_FATAL, "Sys_V_Init - SDL_CreateWindow failed: %s\n", SDL_GetError());

	glimp.glContext = SDL_GL_CreateContext(glimp.window);
	if (glimp.glContext == NULL)
		ri.Error(ERR_FATAL, "Sys_V_Init - SDL_GL_CreateContext failed: %s\n", SDL_GetError());
	glConfig.colorBits = 32;
	glConfig.depthBits = 24;
	glConfig.stencilBits = 8;

	if (SDL_GL_MakeCurrent(glimp.window, glimp.glContext) < 0)
		ri.Error(ERR_FATAL, "Sys_V_Init - SDL_GL_MakeCurrent failed: %s\n", SDL_GetError());

	const GLenum glewErrorCode = glewInit();
	if (glewErrorCode != GLEW_OK)
		ri.Error(ERR_FATAL, "Sys_V_Init - glewInit failed: %s\n", glewGetErrorString(glewErrorCode));

	CL_GL_Init();
}


void Sys_V_Shutdown()
{
	if (glimp.glContext != NULL) {
		SDL_GL_DeleteContext(glimp.glContext);
		glimp.glContext = NULL;
	}

	if (glimp.window != NULL) {
		SDL_DestroyWindow(glimp.window);
		glimp.window = NULL;
	}

	SDL_GL_UnloadLibrary();
}


void Sys_V_EndFrame()
{
	if (r_swapInterval->modified) {
		r_swapInterval->modified = qfalse;
		SDL_GL_SetSwapInterval(r_swapInterval->integer);
	}

	SDL_GL_SwapWindow(glimp.window);
}


qbool Sys_V_IsVSynced()
{
	return SDL_GL_GetSwapInterval() != 0;
}

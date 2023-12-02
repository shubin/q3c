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
// linux_local.h: Linux-specific Quake3 header file


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#ifndef DEDICATED
#include "../client/client.h"
#endif


void		Lin_HardRebootHandler( int argc, char** argv );
void		Lin_TrackParentProcess();
void		Lin_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
void		Lin_ConsoleInputInit();
void		Lin_ConsoleInputShutdown();
const char*	Lin_ConsoleInput();

qbool		tty_Enabled();
void		tty_Hide();
history_t*	tty_GetHistory();

#ifndef DEDICATED
qbool	sdl_Init();
void	sdl_InitCvarsAndCmds();
void	sdl_PollEvents();
void	sdl_Frame(); // polling AND dealing with state changes (e.g. input grab)
void	sdl_UpdateMonitorIndexFromWindow();
void	sdl_MuteAudio( qbool mute );
#endif

void	SIG_InitChild();
void	SIG_InitParent();
void	SIG_Frame();

extern int		q_argc;
extern char**	q_argv;

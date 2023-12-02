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
// Linux entry point

#include "linux_local.h"


int    q_argc = 0;
char** q_argv = NULL;


int main( int argc, char** argv )
{
	q_argc = argc;
	q_argv = argv;

#ifdef DEDICATED
	Lin_HardRebootHandler(argc, argv);
#endif

	SIG_InitChild();

#ifndef DEDICATED
	if (!sdl_Init())
		return 1;
#endif

	// merge the command line: we need it in a single chunk
	int len = 1, i;
	for (i = 1; i < argc; ++i)
		len += strlen(argv[i]) + 1;
	char* cmdline = (char*)malloc(len);
	*cmdline = 0;
	for (i = 1; i < argc; ++i) {
		if (i > 1)
			strcat(cmdline, " ");
		strcat(cmdline, argv[i]);
	}
	Com_Init(cmdline);

	NET_Init();

	Com_Printf("Working directory: %s\n", Sys_Cwd());

	Lin_ConsoleInputInit();
	Lin_TrackParentProcess();

#ifndef DEDICATED	
	sdl_InitCvarsAndCmds();
#endif

	for (;;) {
		SIG_Frame();

#ifndef DEDICATED
		sdl_Frame();
#endif

#ifdef DEDICATED
		Com_Frame(qfalse);
#else
		Com_Frame(clc.demoplaying);
#endif
	}

	return 0;
}

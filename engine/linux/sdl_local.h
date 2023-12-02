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

#pragma once


#define MAX_MONITOR_COUNT 16


struct monitor_t {
	SDL_Rect		rect;
	int				sdlIndex;
};

struct glImp_t {
	SDL_Window*		window;
	SDL_GLContext	glContext;

	monitor_t		monitors[MAX_MONITOR_COUNT];
	int				monitorCount;
	int				monitor; // indexes monitors
};


extern glImp_t glimp;

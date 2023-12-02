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

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "win_local.h"


int Sys_Milliseconds()
{
	static int sys_timeBase = 0;

	if (!sys_timeBase)
		sys_timeBase = timeGetTime();

	return (timeGetTime() - sys_timeBase);
}


void Sys_Sleep( int ms )
{
	if (ms >= 1)
		Sleep(ms);
}

void Sys_MicroSleep( int us )
{
	if (us <= 0)
		return;

	LARGE_INTEGER frequency;
	LARGE_INTEGER endTime;
	QueryPerformanceCounter(&endTime);
	QueryPerformanceFrequency(&frequency);
	endTime.QuadPart += ((LONGLONG)us * frequency.QuadPart) / 1000000LL;

	LARGE_INTEGER currentTime;
	do {
		SwitchToThread();
		QueryPerformanceCounter(&currentTime);
	} while (currentTime.QuadPart < endTime.QuadPart);
}


int64_t Sys_Microseconds()
{
	static qbool initialized = qfalse;
	static LARGE_INTEGER start;
	static LARGE_INTEGER freq;

	if (!initialized) {
		initialized = qtrue;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&start);
	}

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	return ((now.QuadPart - start.QuadPart) * 1000000LL) / freq.QuadPart;
}


const char* Sys_DefaultHomePath()
{
	return NULL;
}


qbool Sys_HardReboot()
{
	return qfalse;
}


qbool Sys_HasCNQ3Parent()
{
	return qfalse;
}


int Sys_GetUptimeSeconds( qbool parent )
{
	if (parent)
		return -1;

	FILETIME startFileTime;
	FILETIME trash[3];
	if (GetProcessTimes(GetCurrentProcess(), &startFileTime, &trash[0], &trash[1], &trash[2]) == 0)
		return -1;

	SYSTEMTIME endSystemTime;
	GetSystemTime(&endSystemTime);

	FILETIME endFileTime;
	if (SystemTimeToFileTime(&endSystemTime, &endFileTime) == 0)
		return -1;

	// 1 FILETIME unit is 100-nanoseconds
	ULARGE_INTEGER start, end;
	start.LowPart = startFileTime.dwLowDateTime;
	start.HighPart = startFileTime.dwHighDateTime;
	end.LowPart = endFileTime.dwLowDateTime;
	end.HighPart = endFileTime.dwHighDateTime;
	const int seconds = (int)((end.QuadPart - start.QuadPart) / 1e7);

	return seconds;
}


void Sys_DebugPrintf( PRINTF_FORMAT_STRING const char* fmt, ... )
{
	char buffer[1024];
	va_list argptr;
	va_start(argptr, fmt);
	const int len = vsprintf(buffer, fmt, argptr);
	va_end(argptr);

	if (len < 0)
		return;
	if (len >= sizeof(buffer))
		buffer[sizeof(buffer) - 1] = '\0';

	OutputDebugStringA(buffer);
}


qbool Sys_IsDebuggerAttached()
{
	return IsDebuggerPresent();
}


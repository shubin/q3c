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
// save and print useful data for JSON crash report files

#include "q_shared.h"
#include "qcommon.h"

#include <stdio.h>


// json.cpp
void JSONW_BeginFile(FILE* file);
void JSONW_EndFile();
void JSONW_BeginObject();
void JSONW_BeginNamedObject(const char* name);
void JSONW_EndObject();
void JSONW_BeginArray();
void JSONW_BeginNamedArray(const char* name);
void JSONW_EndArray();
void JSONW_IntegerValue(const char* name, int number);
void JSONW_HexValue(const char* name, uint64_t number);
void JSONW_BooleanValue(const char* name, qbool value);
void JSONW_StringValue(const char* name, PRINTF_FORMAT_STRING const char* format, ...);
void JSONW_UnnamedHex(uint64_t number);
void JSONW_UnnamedString(PRINTF_FORMAT_STRING const char* format, ...);

// crash.cpp
void Crash_SaveQVMPointer(vmIndex_t vmIndex, vm_t* vm);
void Crash_SaveQVMChecksum(vmIndex_t vmIndex, unsigned int crc32);
void Crash_SaveQVMGitString(const char* varName, const char* varValue);
void Crash_SaveModName(const char* modName);
void Crash_SaveModVersion(const char* modVersion);
void Crash_PrintToFile(const char* engineFilePath);
#if defined(__linux__) || defined(__FreeBSD__)
void Crash_PrintVMStackTracesASS(int fd); // async-signal-safe
#endif

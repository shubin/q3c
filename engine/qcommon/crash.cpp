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

#include "crash.h"
#include "git.h"
#include "vm_local.h"
#include <sys/stat.h>


typedef struct {
	char			gitHeadHash[24]; // SHA-1 -> 160 bits -> 20 hex chars
	vm_t*			vm;
	unsigned int	crc32;
} vmCrash_t;

typedef struct {
	vmCrash_t	vm[VM_COUNT];
	char		modName[64];
	char		modVersion[64];
} crash_t;


static crash_t crash;


static qbool IsVMIndexValid(vmIndex_t vmIndex)
{
	return vmIndex >= 0 && vmIndex < VM_COUNT;
}

static const char* GetVMName(vmIndex_t vmIndex)
{
	switch (vmIndex) {
		case VM_CGAME: return "cgame";
		case VM_GAME: return "game";
		case VM_UI: return "ui";
		default: return "unknown";
	}
}

void Crash_SaveQVMPointer(vmIndex_t vmIndex, vm_t* vm)
{
	if (IsVMIndexValid(vmIndex))
		crash.vm[vmIndex].vm = vm;
}

void Crash_SaveQVMChecksum(vmIndex_t vmIndex, unsigned int crc32)
{
	if (IsVMIndexValid(vmIndex))
		crash.vm[vmIndex].crc32 = crc32;
}

void Crash_SaveQVMGitString(const char* varName, const char* varValue)
{
	if (strstr(varName, "gitHeadHash") == NULL)
		return;

	vmIndex_t index;
	if (strstr(varName, "cg_") == varName)
		index = VM_CGAME;
	else if (strstr(varName, "g_") == varName)
		index = VM_GAME;
	else if (strstr(varName, "ui_") == varName)
		index = VM_UI;
	else
		return;

	Q_strncpyz(crash.vm[index].gitHeadHash, varValue, sizeof(crash.vm[index].gitHeadHash));
}

void Crash_SaveModName(const char* modName)
{
	if (modName && *modName != '\0')
		Q_strncpyz(crash.modName, modName, sizeof(crash.modName));
}

void Crash_SaveModVersion(const char* modVersion)
{
	if (modVersion && *modVersion != '\0')
		Q_strncpyz(crash.modVersion, modVersion, sizeof(crash.modVersion));
}

static void PrintQVMInfo(vmIndex_t vmIndex)
{
	static char callStack[MAX_VM_CALL_STACK_DEPTH * 12];

	vmCrash_t* vm = &crash.vm[vmIndex];
	if (vm->crc32 == 0) {
		return;
	}

	JSONW_BeginObject();

	JSONW_IntegerValue("index", vmIndex);
	JSONW_StringValue("name", GetVMName(vmIndex));
	JSONW_BooleanValue("loaded", vm->vm != NULL);
	if (vm->crc32)
		JSONW_HexValue("crc32", vm->crc32);
	JSONW_StringValue("git_head_hash", vm->gitHeadHash);

	if (vm->vm != NULL) {
		vm_t* const vmp = vm->vm;
		JSONW_IntegerValue("call_stack_depth_current", vmp->callStackDepth);
		JSONW_IntegerValue("call_stack_depth_previous", vmp->lastCallStackDepth);

		int d = vmp->callStackDepth;
		qbool current = qtrue;
		if (d <= 0 || d > MAX_VM_CALL_STACK_DEPTH) {
			d = vmp->lastCallStackDepth;
			current = qfalse;
		}

		if (d <= 0 || d > MAX_VM_CALL_STACK_DEPTH)
			d = 0;

		if (d > 0) {
			JSONW_BooleanValue("call_stack_current", current);
			JSONW_BooleanValue("call_stack_limit_reached", d == MAX_VM_CALL_STACK_DEPTH);

			callStack[0] = '\0';
			for (int i = 0; i < d; i++) {
				Q_strcat(callStack, sizeof(callStack), Q_itohex(vmp->callStack[i], qtrue, qtrue));
				if (i - 1 < d) 
					Q_strcat(callStack, sizeof(callStack), " ");
			}
			JSONW_StringValue("call_stack", callStack);
		}
	}

	JSONW_EndObject();
}

static qbool IsAnyVMLoaded()
{
	for (int i = 0; i < VM_COUNT; i++) {
		if (crash.vm[i].crc32 != 0)
			return qtrue;
	}

	return qfalse;
}
#if !defined( QC ) // I use it to verify pak?.pk3 in win_files.cpp
static 
#endif
unsigned int CRC32_HashFile(const char* filePath)
{
	enum { BUFFER_SIZE = 16 << 10 }; // 16 KB
	static byte buffer[BUFFER_SIZE];

	struct stat st;
	if (stat(filePath, &st) != 0 || st.st_size == 0)
		return 0;

	FILE* const file = fopen(filePath, "rb");
	if (file == NULL)
		return 0;

	const unsigned int fileSize = (unsigned int)st.st_size;
	const unsigned int fullBlocks = fileSize / (unsigned int)BUFFER_SIZE;
	const unsigned int lastBlockSize = fileSize - fullBlocks * (unsigned int)BUFFER_SIZE;

	unsigned int crc32 = 0;
	CRC32_Begin(&crc32);

	for(unsigned int i = 0; i < fullBlocks; ++i) {
		if (fread(buffer, BUFFER_SIZE, 1, file) != 1) {
			fclose(file);
			return 0;
		}
		CRC32_ProcessBlock(&crc32, buffer, BUFFER_SIZE);
	}

	if(lastBlockSize > 0) {
		if (fread(buffer, lastBlockSize, 1, file) != 1) {
			fclose(file);
			return 0;
		}
		CRC32_ProcessBlock(&crc32, buffer, lastBlockSize);
	}

	CRC32_End(&crc32);

	fclose(file);

	return crc32;
}

void Crash_PrintToFile(const char* engineFilePath)
{
	JSONW_StringValue("engine_version", Q3_VERSION);
	JSONW_StringValue("engine_build_date", __DATE__);
	JSONW_StringValue("engine_arch", ARCH_STRING);
#ifdef DEDICATED
	JSONW_BooleanValue("engine_ded_server", qtrue);
#else
	JSONW_BooleanValue("engine_ded_server", qfalse);
#endif
#ifdef DEBUG
	JSONW_BooleanValue("engine_debug", qtrue);
#else
	JSONW_BooleanValue("engine_debug", qfalse);
#endif
#ifdef CNQ3_DEV
	JSONW_BooleanValue("engine_dev_build", qtrue);
#else
	JSONW_BooleanValue("engine_dev_build", qfalse);
#endif
	JSONW_StringValue("engine_git_branch", GIT_BRANCH);
	JSONW_StringValue("engine_git_commit", GIT_COMMIT);
	const unsigned int crc32 = CRC32_HashFile(engineFilePath);
	if (crc32)
		JSONW_HexValue("engine_crc32", crc32);
	JSONW_StringValue("mod_name", crash.modName);
	JSONW_StringValue("mod_version", crash.modVersion);

	if (IsAnyVMLoaded()) {
		JSONW_BeginNamedArray("vms");
		for (int i = 0; i < VM_COUNT; i++) {
			PrintQVMInfo((vmIndex_t)i);
		}
		JSONW_EndArray();
	}
}


#if defined(__linux__) || defined(__FreeBSD__)

#include <unistd.h>

static void Print(int fd, const char* msg)
{
	write(fd, msg, strlen(msg));
}

static void PrintVMStackTrace(int fd, vmIndex_t vmIndex)
{
	Print(fd, GetVMName(vmIndex));
	Print(fd, ": ");

	vmCrash_t* const vmc = &crash.vm[vmIndex];
	vm_t* const vm = vmc->vm;
	if (vm == NULL) {
		Print(fd, "not loaded\r\n");
		return;
	}

	Print(fd, "crc32 ");
	Print(fd, Q_itohex(vmc->crc32, qtrue, qtrue));
	Print(fd, " - ");

	int d = vm->callStackDepth;
	qbool current = qtrue;
	if (d <= 0 || d > MAX_VM_CALL_STACK_DEPTH) {
		d = vm->lastCallStackDepth;
		current = qfalse;
	}

	if (d <= 0 || d > MAX_VM_CALL_STACK_DEPTH)
		d = 0;

	Print(fd, current ? "current " : "old ");
	if (d > 0) {
		for (int i = 0; i < d; i++) {
			Print(fd, Q_itohex(vm->callStack[i], qtrue, qtrue));
			if (i - 1 < d) 
				Print(fd, " ");
		}		
	}
	Print(fd, "\r\n");
}

void Crash_PrintVMStackTracesASS(int fd)
{
	if (crash.modName[0] != '\0') {
		Print(fd, "mod: ");
		Print(fd, crash.modName);
		Print(fd, " ");
		Print(fd, crash.modVersion);
		Print(fd, "\r\n");	
	}

	for (int i = 0; i < VM_COUNT; i++) {
		PrintVMStackTrace(fd, (vmIndex_t)i);
	}
}

#endif

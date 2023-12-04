/*
===========================================================================
Copyright (C) 2022 Gian 'myT' Schellenbaum

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
// New Demo Player with fast seeking support


/*

GENERAL

When the client loads a demo with the new demo player enabled,
CL_NDP_PlayDemo is called with the demo file opened.
It will then proceed to generate a compressed and seekable encoding
of said demo in memory.
The input demo is read from disk exactly once in the process.

STEP BY STEP BREAKDOWN

Demo loading steps:
- Parse until a gamestate is found.
- Make gamestate config strings available to CGame.
- Create CGame VM and call CGAME_INIT.
	- CGame loads the map, the assets, etc.
	- CGame also starts demo analysis with the gamestate config strings.
- Parse commands and snapshots until there's an error, a new gamestate or there's nothing left to parse.
	Command:
	- Feed it to CGame for analysis.
	Snapshot:
	- Feed it to CGame for analysis.
	- Decide whether this snapshot shall be delta-encoded or not based on server time.
		- If full, ask CGame to generate a list of synchronization server commands.
		- If full, save indexing data to be able to reference this snapshot during playback.
	- Write the snapshot to the memory buffer.
- Close the input file and tell CGame to finish demo analysis.
- Jump to the desired time and read the first 2 snapshots from memory so we're ready for playback.

Demo playback steps:
- Always keep a decoded snapshot for the current server time.
- If not at the end, keep a "next" decoded snapshot so CGame can interpolate.
- Once CGame queries a snapshot that exists,
  generate and add all related commands to the CGame command queue.

Seeking steps:
- Find the first full snapshot before the requested server time.
- Decode 2 snapshots so that "current" and "next" are both set, ready for playback.
- Queue commands and decode more snapshots until the playback time reaches the requested time.

ERROR HANDLING

Standard (forward-only) demo playback can afford to use DROP errors
since it doesn't prevent you from seeing everything that's valid before the bad data.
However, this new demo player needs to parse the full file to generate
a new representation that can be used during playback.
A drop error would therefore completely stop the processing and cancel playback.
The new custom handler allows errors to simply stop the parsing phase earlier.
If a gamestate and at least 1 snapshot were read, then loading can be finalized
and playback can happen as if the demo file was simply truncated.

STORAGE

Why store in memory and not on disk?
- Demo conversion is fast enough that a disk cache is not needed.
  It would just waste disk space.
- Changes to either the client's code or CPMA can cause changes to
  the final generated data.
- Cached files with original names are a pain. If you ever create a new cut of a frag
  with the same file name or process the demo (e.g. LG/RG fixes),
  then the game will load the cached version of the old data.
- Cached files based on a hash of the original input file are immune to this,
  but now you don't know which files are safe to delete from the name alone.

MULTIPLE GAMESTATES

If the player has a demo with 2+ gamestates, he can trivially split it up using UDT.
There's no need to make the player more complex for such an edge case.

*/


#include "client.h"

#include <setjmp.h>
#if defined(_MSC_VER)
#pragma warning(disable: 4611) // setjmp with C++ destructors
#endif


#define FULL_SNAPSHOT_INTERVAL_MS (8 * 1000)

#define MAX_COMMANDS 256

#define VERBOSE_DEBUGGING 0

// VC++, GCC and Clang all seem to support ##__VA_ARGS__ just fine
#define Drop(Message, ...)    Com_Error(ERR_DROP, "New Demo Player: " Message "\n", ##__VA_ARGS__)
#define Error(Message, ...)   Com_Printf("ERROR: " Message "\n", ##__VA_ARGS__); longjmp(ndp.abortLoad, -1)
#define Warning(Message, ...) Com_Printf("WARNING: " Message "\n", ##__VA_ARGS__)

#if defined(_DEBUG) && VERBOSE_DEBUGGING
#define PrintCmdListBegin(Message) Sys_DebugPrintf("BEGIN " Message "\n"); cmdName = Message
#define PrintCmdListEnd()          Sys_DebugPrintf("END   %s\n", cmdName)
#define PrintCmd(Cmd)              Sys_DebugPrintf("      %8d: %s\n", demo.currSnap->serverTime, Cmd)
#else
#define PrintCmdListBegin(Message)
#define PrintCmdListEnd()
#define PrintCmd(Cmd)
#endif


struct configString_t {
	int index;
	int serverTime;
};

struct commandBuffer_t {
	char data[128 * 1024];
	int numBytes;
};

struct ndpSnapshot_t {
	// the first char of configStrings is '\0'
	// so that empty entries can all use offset 0
	commandBuffer_t serverCommands;
	commandBuffer_t synchCommands;
	char configStrings[MAX_GAMESTATE_CHARS]; // matches the size in gameState_t
	int configStringOffsets[MAX_CONFIGSTRINGS];
	int configStringTimes[MAX_CONFIGSTRINGS]; // last time it was changed
	entityState_t entities[MAX_GENTITIES];
	byte areaMask[MAX_MAP_AREA_BYTES];
	playerState_t ps;
	int numAreaMaskBytes;
	int numConfigStringBytes;
	int messageNum;
	int serverTime;
	int numEntities;
	int numServerCommands;
	int serverCommandSequence;
	int snapFlags;
	int ping;
	qbool isFullSnap;
	qbool isServerPaused;
};

struct parser_t {
	ndpSnapshot_t ndpSnapshots[2]; // current one and previous one for delta encoding
	clSnapshot_t snapshots[PACKET_BACKUP];
	entityState_t entityBaselines[MAX_GENTITIES];
	entityState_t entities[MAX_PARSE_ENTITIES];
	char bigConfigString[BIG_INFO_STRING];
	msg_t inMsg;
	msg_t outMsg;
	ndpSnapshot_t* currSnap;
	ndpSnapshot_t* prevSnap;
	int entityWriteIndex; // indexes entities directly
	int messageNum;       // number of the current msg_t data packet
	int bigConfigStringIndex;
	int prevServerTime;
	int lastMessageNum;
	int nextFullSnapshotTime;  // when server time is bigger, write a full snapshot
	int serverCommandSequence; // the command number of the latest command we decoded
	int progress;
};

struct demoIndex_t {
	int byteOffset;
	int serverTime;
	int snapshotIndex;
};

struct memoryBuffer_t {
	byte* data;
	int capacity; // total number of bytes allocated
	int position; // cursor or number of bytes read/written
	int numBytes; // total number of bytes written for read mode
	qbool isReadMode;
};

struct command_t {
	char command[MAX_STRING_CHARS]; // the size used by MSG_ReadString
};

struct demo_t {
	ndpSnapshot_t snapshots[2]; // current one and next one for CGame requests
	demoIndex_t indices[4096];
	command_t commands[MAX_COMMANDS];
	memoryBuffer_t buffer;
	ndpSnapshot_t* currSnap;
	ndpSnapshot_t* nextSnap;
	int numSnapshots;
	int numIndices; // the number of full snapshots (i.e. with no delta-encoding)
	int firstServerTime;
	int lastServerTime;
	int snapshotIndex; // the index of currSnap for CGame requests
	int numCommands;   // number of commands written, must be modulo'd to index commands
	qbool isLastSnapshot;
};

struct newDemoPlayer_t {
	jmp_buf abortLoad;
	qbool trackServerPause;
	qbool isServerPaused;
	int serverPauseDelay; // total duration in server pause since the full snapshot
};


static parser_t parser;
static demo_t demo;
static newDemoPlayer_t ndp;
static const entityState_t nullEntityState = { 0 };
static const playerState_t nullPlayerState = { 0 };
#if defined(_DEBUG) && VERBOSE_DEBUGGING
static const char* cmdName;
#endif


// smallest time value comes first to keep chronological order
static int CompareConfigStringTimes( const void* aPtr, const void* bPtr )
{
	const configString_t* const a = (const configString_t*)aPtr;
	const configString_t* const b = (const configString_t*)bPtr;
	return a->serverTime - b->serverTime;
}


static void MB_GrowTo( memoryBuffer_t* mb, int numBytes )
{
	if (mb->data && mb->capacity < numBytes) {
		byte* const oldData = mb->data;
		numBytes = max(numBytes, mb->capacity * 2);
		mb->data = (byte*)calloc(numBytes, 1);
		if (!mb->data) {
			Drop("Ran out of memory");
		}
		mb->capacity = numBytes;
		Com_Memcpy(mb->data, oldData, mb->position);
		free(oldData);
	} else if (!mb->data) {
		mb->data = (byte*)calloc(numBytes, 1);
		if (!mb->data) {
			Drop("Ran out of memory");
		}
		mb->capacity = numBytes;
	}
}


static void MB_InitWrite( memoryBuffer_t* mb )
{
	MB_GrowTo(mb, 4 << 20);
	mb->numBytes = 0;
	mb->position = 0;
	mb->isReadMode = qfalse;
}


static void MB_Write( memoryBuffer_t* mb, const void* data, int numBytes )
{
	Q_assert(!mb->isReadMode);
	MB_GrowTo(mb, mb->position + numBytes);
	Com_Memcpy(mb->data + mb->position, data, numBytes);
	mb->position += numBytes;
}


static void MB_InitRead( memoryBuffer_t* mb )
{
	Q_assert(mb->position > 0);
	mb->numBytes = mb->position;
	mb->position = 0;
	mb->isReadMode = qtrue;
}


static void MB_InitMessage( memoryBuffer_t* mb, msg_t* inMsg )
{
	const int numBytes = *(int*)(mb->data + mb->position);
	Q_assert(numBytes > 0 && numBytes <= MAX_MSGLEN);
	mb->position += 4;
	MSG_Init(inMsg, mb->data + mb->position, numBytes);
	mb->position += numBytes;
	Q_assert(demo.buffer.position > 0);
	MSG_BeginReading(inMsg);
	inMsg->cursize = numBytes;
}


static void MB_Seek( memoryBuffer_t* mb, int position )
{
	Q_assert(mb->isReadMode);
	if (position < 0 || position >= mb->numBytes) {
		Drop("Seeking out of range");
	}
	mb->position = position;
}


static void WriteNDPSnapshot( msg_t* outMsg, const ndpSnapshot_t* prevSnap, ndpSnapshot_t* currSnap, qbool isServerPaused )
{
	const qbool isFullSnap = prevSnap == NULL;

	// header
	MSG_WriteBits(outMsg, isFullSnap, 1);
	MSG_WriteLong(outMsg, currSnap->serverTime);
	MSG_WriteBits(outMsg, !!isServerPaused, 1);

	// player state
	const playerState_t* const oldPS = !prevSnap ? &nullPlayerState : &prevSnap->ps;
	playerState_t* const newPS = &currSnap->ps;
	MSG_WriteDeltaPlayerstate(outMsg, oldPS, newPS);

	// area mask
	MSG_WriteByte(outMsg, currSnap->numAreaMaskBytes);
	MSG_WriteData(outMsg, currSnap->areaMask, currSnap->numAreaMaskBytes);

	// server commands
	MSG_WriteLong(outMsg, currSnap->serverCommands.numBytes);
	MSG_WriteData(outMsg, currSnap->serverCommands.data, currSnap->serverCommands.numBytes);

	// synchronization commands
	if (isFullSnap) {
		MSG_WriteLong(outMsg, currSnap->synchCommands.numBytes);
		MSG_WriteData(outMsg, currSnap->synchCommands.data, currSnap->synchCommands.numBytes);
	}

	// config strings
	for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
		const char* const oldString = !prevSnap ? "" : &prevSnap->configStrings[prevSnap->configStringOffsets[i]];
		const char* const newString = currSnap->configStrings + currSnap->configStringOffsets[i];
		if (strcmp(oldString, newString)) {
			MSG_WriteShort(outMsg, i);
			MSG_WriteLong(outMsg, currSnap->configStringTimes[i]);
			MSG_WriteBigString(outMsg, newString);
		}
	}
	MSG_WriteShort(outMsg, MAX_CONFIGSTRINGS); // end marker

	// entities
	for (int i = 0; i < MAX_GENTITIES - 1; ++i) {
		const entityState_t* oldEntity = NULL;
		if (prevSnap && prevSnap->entities[i].number < MAX_GENTITIES - 1) {
			oldEntity = &prevSnap->entities[i];
		}

		const entityState_t* newEntity = &currSnap->entities[i];
		if (newEntity->number != i || newEntity->number >= MAX_GENTITIES - 1) {
			newEntity = NULL;
		}

		if (newEntity && !oldEntity) {
			oldEntity = &nullEntityState;
		}
		MSG_WriteDeltaEntity(outMsg, oldEntity, newEntity, qfalse);
	}
	MSG_WriteBits(outMsg, MAX_GENTITIES - 1, GENTITYNUM_BITS); // end marker
}


static void SaveConfigString( ndpSnapshot_t* snap, int index, const char* string, int serverTime )
{
	snap->configStringTimes[index] = serverTime;

	if (string[0] == '\0') {
		snap->configStringOffsets[index] = 0;
		return;
	}

	int numStringBytes = strlen(string) + 1;
	if (snap->numConfigStringBytes + numStringBytes <= sizeof(snap->configStrings)) {
		// we have enough space
		Com_Memcpy(snap->configStrings + snap->numConfigStringBytes, string, numStringBytes);
		snap->configStringOffsets[index] = snap->numConfigStringBytes;
		snap->numConfigStringBytes += numStringBytes;
		return;
	}

	// we ran out of memory, it's time to compact the buffer
	// we first create a new one locally then copy it into the snapshot
	char cs[sizeof(snap->configStrings)];
	cs[0] = '\0';
	int numTotalBytes = 1;
	for (int i = 0; i < ARRAY_LEN(snap->configStringOffsets); ++i) {
		const char* const s = (i == index) ? string : snap->configStrings + snap->configStringOffsets[i];
		if (s[0] == '\0') {
			snap->configStringOffsets[i] = 0;
			continue;
		}

		numStringBytes = strlen(s) + 1;
		if (numTotalBytes + numStringBytes > sizeof(snap->configStrings)) {
			Warning("Ran out of config string memory");
			break;
		}

		Com_Memcpy(cs + numTotalBytes, s, numStringBytes);
		snap->configStringOffsets[i] = numTotalBytes;
		numTotalBytes += numStringBytes;
	}
	Com_Memcpy(snap->configStrings, cs, numTotalBytes);
	snap->numConfigStringBytes = numTotalBytes;
}


static void SaveCommandString( commandBuffer_t* cmdBuf, const char* string )
{
	const int numBytes = strlen(string) + 1;
	if (cmdBuf->numBytes + numBytes > sizeof(cmdBuf->data)) {
		Drop("Not enough memory for command data");
	}

	Com_Memcpy(cmdBuf->data + cmdBuf->numBytes, string, numBytes);
	cmdBuf->numBytes += numBytes;
}


static void ParseServerCommand()
{
	msg_t* const inMsg = &parser.inMsg;

	const int commandNumber = MSG_ReadLong(inMsg);
	const char* const s = MSG_ReadString(inMsg);
	if (commandNumber <= parser.serverCommandSequence) {
		return;
	}
	parser.serverCommandSequence = commandNumber;

	Cmd_TokenizeString(s);

	if (!Q_stricmp(Cmd_Argv(0), "bcs0")) {
		parser.bigConfigStringIndex = atoi(Cmd_Argv(1));
		Q_strncpyz(parser.bigConfigString, Cmd_Argv(2), sizeof(parser.bigConfigString));
	} else if (!Q_stricmp(Cmd_Argv(0), "bcs1")) {
		Q_strcat(parser.bigConfigString, sizeof(parser.bigConfigString), Cmd_Argv(2));
	} else if (!Q_stricmp(Cmd_Argv(0), "bcs2")) {
		Q_strcat(parser.bigConfigString, sizeof(parser.bigConfigString), Cmd_Argv(2));
		// must tokenize again for the mod's benefit
		Cmd_TokenizeString(parser.bigConfigString);
		CL_CGNDP_AnalyzeCommand(parser.prevServerTime);
		SaveConfigString(parser.currSnap, parser.bigConfigStringIndex, parser.bigConfigString, parser.prevServerTime);
	} else if (!Q_stricmp(Cmd_Argv(0), "cs")) {
		// already tokenized, so we're good to go
		CL_CGNDP_AnalyzeCommand(parser.prevServerTime);
		SaveConfigString(parser.currSnap, atoi(Cmd_Argv(1)), Cmd_ArgsFrom(2), parser.prevServerTime);
	} else {
		// already tokenized, so we're good to go
		CL_CGNDP_AnalyzeCommand(parser.prevServerTime);
		SaveCommandString(&parser.currSnap->serverCommands, s);
	}
}


static void ParseGamestate()
{
	msg_t* const inMsg = &parser.inMsg;
	MSG_ReadLong(inMsg); // skip message sequence

	ndpSnapshot_t* const currSnap = parser.currSnap;
	for (;;) {
		const int cmd = MSG_ReadByte(inMsg);
		if (cmd == svc_EOF) {
			break;
		} else if (cmd == svc_configstring) {
			const int index = MSG_ReadShort(inMsg);
			const char* const string = MSG_ReadBigString(inMsg);
			SaveConfigString(currSnap, index, string, -10000 + index + 1);
		} else if (cmd == svc_baseline) {
			const int index = MSG_ReadBits(inMsg, GENTITYNUM_BITS);
			if (index < 0 || index >= MAX_GENTITIES) {
				Error("Invalid baseline index: %d", index);
			}
			MSG_ReadDeltaEntity(inMsg, &nullEntityState, &parser.entityBaselines[index], index);
		} else {
			Error("Invalid gamestate command byte: %d", cmd);
		}
	}

	clc.clientNum = MSG_ReadLong(inMsg);
	clc.checksumFeed = MSG_ReadLong(inMsg);

	// start up CGame now because we need it running for the command parser
	clc.demoplaying = qtrue;
	clc.newDemoPlayer = qtrue;
	clc.serverMessageSequence = 0;
	clc.lastExecutedServerCommand = 0;
	Con_Close();
	CL_ClearState();
	cls.state = CA_LOADING;
	Com_EventLoop();
	Cvar_Set("r_uiFullScreen", "0");
	CL_FlushMemory();
	Com_Memcpy(cl.gameState.stringOffsets, currSnap->configStringOffsets, sizeof(cl.gameState.stringOffsets));
	Com_Memcpy(cl.gameState.stringData, currSnap->configStrings, currSnap->numConfigStringBytes);
	cl.gameState.dataCount = currSnap->numConfigStringBytes;
	CL_InitCGame();
	if (!cls.cgameNewDemoPlayer) {
		Drop("Sorry, this mod doesn't support the new demo player");
	}
	cls.cgameStarted = qtrue;
	cls.state = CA_ACTIVE;
}


static void ParseSnapshot()
{
	msg_t* const inMsg = &parser.inMsg;

	const int byteOffset = demo.buffer.position;

	const int newSnapIndex = parser.messageNum & PACKET_MASK;
	clSnapshot_t* oldSnap = NULL;
	clSnapshot_t* newSnap = &parser.snapshots[newSnapIndex];

	const int currServerTime = MSG_ReadLong(inMsg);

	Com_Memset(newSnap, 0, sizeof(*newSnap));
	newSnap->deltaNum = MSG_ReadByte(inMsg);
	newSnap->messageNum = parser.messageNum;
	newSnap->snapFlags = MSG_ReadByte(inMsg);

	const int numAreaMaskBytes = MSG_ReadByte(inMsg);
	if (numAreaMaskBytes > sizeof(newSnap->areamask)) {
		Error("Invalid area mask size: %d", numAreaMaskBytes);
	}

	if (newSnap->deltaNum == 0) {
		// uncompressed data
		newSnap->deltaNum = -1;
		newSnap->valid = qtrue;
	} else {
		newSnap->deltaNum = newSnap->messageNum - newSnap->deltaNum;
		oldSnap = &parser.snapshots[newSnap->deltaNum & PACKET_MASK];
		if (!oldSnap->valid) {
			Error("The snapshot deltas against nothing");
		} else if (oldSnap->messageNum != newSnap->deltaNum) {
			Warning("Delta frame too old");
		} else if (parser.entityWriteIndex - oldSnap->parseEntitiesNum > MAX_PARSE_ENTITIES - 128) {
			Warning("Delta parseEntitiesNum too old");
		} else {
			newSnap->valid = qtrue;
		}
	}

	MSG_ReadData(inMsg, newSnap->areamask, numAreaMaskBytes);

	MSG_ReadDeltaPlayerstate(inMsg, oldSnap ? &oldSnap->ps : NULL, &newSnap->ps);

	newSnap->parseEntitiesNum = parser.entityWriteIndex;
	newSnap->numEntities = 0;

	entityState_t* oldstate;
	entityState_t* newstate;
	int oldnum, newnum;
	int oldindex = 0;
	newnum = MSG_ReadBits(inMsg, GENTITYNUM_BITS);
	for (;;) {
		if (oldSnap && oldindex < oldSnap->numEntities) {
			// we still haven't dealt with all entities from oldSnap
			const int index = (oldSnap->parseEntitiesNum + oldindex) & (MAX_PARSE_ENTITIES - 1);
			oldstate = &parser.entities[index];
			oldnum = oldstate->number;
		} else {
			// we're done with all entities in oldSnap
			oldstate = NULL;
			oldnum = INT_MAX;
		}

		newstate = &parser.entities[parser.entityWriteIndex];
		if (!oldstate && newnum == MAX_GENTITIES - 1) {
			// we're done with all entities in oldSnap AND got the exit signal
			break;
		} else if (oldnum < newnum) {
			// the old entity isn't present in the new list
			*newstate = *oldstate;
			oldindex++;
		} else if (oldnum == newnum) {
			// the entity changed
			MSG_ReadDeltaEntity(inMsg, oldstate, newstate, newnum);
			newnum = MSG_ReadBits(inMsg, GENTITYNUM_BITS);
			oldindex++;
		} else if (oldnum > newnum) {
			// this is a new entity that was delta'd from the baseline
			MSG_ReadDeltaEntity(inMsg, &parser.entityBaselines[newnum], newstate, newnum);
			newnum = MSG_ReadBits(inMsg, GENTITYNUM_BITS);
		}

		if (newstate->number == MAX_GENTITIES - 1) {
			continue;
		}

		parser.entityWriteIndex = (parser.entityWriteIndex + 1) & (MAX_PARSE_ENTITIES - 1);
		newSnap->numEntities++;
	}

	if (!newSnap->valid) {
		return;
	}

	// mark missing snapshots as invalid
	if (newSnap->messageNum - parser.lastMessageNum >= PACKET_BACKUP) {
		parser.lastMessageNum = newSnap->messageNum - (PACKET_BACKUP - 1);
	}
	while (parser.lastMessageNum < newSnap->messageNum) {
		parser.snapshots[parser.lastMessageNum++ & PACKET_MASK].valid = qfalse;
	}
	parser.lastMessageNum = newSnap->messageNum + 1;

	if (currServerTime <= parser.prevServerTime) {
		return;
	}

	qbool isFullSnap = qfalse;
	if (demo.numSnapshots == 0 || currServerTime > parser.nextFullSnapshotTime) {
		isFullSnap = qtrue;
		parser.nextFullSnapshotTime = currServerTime + FULL_SNAPSHOT_INTERVAL_MS;
	}

	// update our custom snapshot data structure
	ndpSnapshot_t* const currNDPSnap = parser.currSnap;
	for (int i = 0; i < MAX_GENTITIES; ++i) {
		currNDPSnap->entities[i].number = MAX_GENTITIES - 1;
	}
	for (int i = 0; i < newSnap->numEntities; ++i) {
		const int index = (newSnap->parseEntitiesNum + i) & (MAX_PARSE_ENTITIES - 1);
		const entityState_t* const ent = &parser.entities[index];
		currNDPSnap->entities[ent->number] = *ent;
	}
	currNDPSnap->isFullSnap = isFullSnap;
	currNDPSnap->ps = newSnap->ps;
	currNDPSnap->serverTime = currServerTime;
	currNDPSnap->numAreaMaskBytes = numAreaMaskBytes;
	Com_Memcpy(currNDPSnap->areaMask, newSnap->areamask, sizeof(currNDPSnap->areaMask));

	// add all synchronization commands
	if (isFullSnap) {
		const char* synchCommands;
		int numSynchCommandBytes;
		CL_CGNDP_GenerateCommands(&synchCommands, &numSynchCommandBytes);
		int synchCommandStart = 0;
		for (;;) {
			if (synchCommandStart >= numSynchCommandBytes) {
				break;
			}
			const char* const cmd = synchCommands + synchCommandStart;
			SaveCommandString(&currNDPSnap->synchCommands, cmd);
			synchCommandStart += strlen(cmd) + 1;
		}
	}

	// let CGame analyze the snapshot so it can do cool stuff
	// e.g. store events of interest for the timeline overlay
	demo.currSnap = currNDPSnap;
	const qbool isServerPaused = CL_CGNDP_AnalyzeSnapshot(parser.progress);

	// draw the current progress once in a while...
	static int lastRefreshTime = Sys_Milliseconds();
	if (Sys_Milliseconds() > lastRefreshTime + 100) {
		SCR_UpdateScreen();
		lastRefreshTime = Sys_Milliseconds();
	}

	// write it out (delta-)compressed
	byte outMsgData[MAX_MSGLEN];
	msg_t writeMsg;
	MSG_Init(&writeMsg, outMsgData, sizeof(outMsgData));
	MSG_Clear(&writeMsg);
	MSG_Bitstream(&writeMsg);
	ndpSnapshot_t* const prevNDPSnap = parser.prevSnap;
	WriteNDPSnapshot(&writeMsg, isFullSnap ? NULL : prevNDPSnap, currNDPSnap, isServerPaused);
	MB_Write(&demo.buffer, &writeMsg.cursize, 4);
	MB_Write(&demo.buffer, writeMsg.data, writeMsg.cursize);

	// prepare the next snapshot
	ndpSnapshot_t* const nextNDPSnap = prevNDPSnap;
	nextNDPSnap->isFullSnap = qfalse;
	nextNDPSnap->serverTime = 0;
	nextNDPSnap->serverCommands.numBytes = 0;
	nextNDPSnap->synchCommands.numBytes = 0;
	Com_Memcpy(nextNDPSnap->configStrings, currNDPSnap->configStrings, currNDPSnap->numConfigStringBytes);
	Com_Memcpy(nextNDPSnap->configStringOffsets, currNDPSnap->configStringOffsets, sizeof(nextNDPSnap->configStringOffsets));
	Com_Memcpy(nextNDPSnap->configStringTimes, currNDPSnap->configStringTimes, sizeof(nextNDPSnap->configStringTimes));
	nextNDPSnap->numConfigStringBytes = currNDPSnap->numConfigStringBytes;
	for (int i = 0; i < MAX_GENTITIES; ++i) {
		nextNDPSnap->entities[i].number = MAX_GENTITIES - 1;
	}

	// parser bookkeeping
	parser.prevSnap = currNDPSnap;
	parser.currSnap = nextNDPSnap;
	parser.prevServerTime = currServerTime;

	// build the playback index table
	if (isFullSnap) {
		if (demo.numIndices >= ARRAY_LEN(demo.indices)) {
			Error("Ran out of indices");
		}
		demoIndex_t* const index = &demo.indices[demo.numIndices++];
		index->byteOffset = byteOffset;
		index->snapshotIndex = demo.numSnapshots;
		index->serverTime = currServerTime;
	}
	demo.numSnapshots++;
	demo.firstServerTime = min(demo.firstServerTime, currServerTime);
	demo.lastServerTime = max(demo.lastServerTime, currServerTime);
}


static void ParseDemo()
{
	byte oldData[MAX_MSGLEN];
	msg_t* const msg = &parser.inMsg;
	const int fh = clc.demofile;
	int numGamestates = 0;

	Com_Memset(&parser, 0, sizeof(parser));
	parser.currSnap = &parser.ndpSnapshots[0];
	parser.prevSnap = &parser.ndpSnapshots[1];
	parser.currSnap->numConfigStringBytes = 1; // first char is always '\0'

	Com_Memset(&demo, 0, sizeof(demo));
	demo.firstServerTime = INT_MAX;
	demo.lastServerTime = INT_MIN;
	demo.currSnap = &demo.snapshots[0];
	demo.nextSnap = &demo.snapshots[1];

	for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
		parser.ndpSnapshots[0].configStringTimes[i] = INT_MIN;
		parser.ndpSnapshots[1].configStringTimes[i] = INT_MIN;
		demo.snapshots[0].configStringTimes[i] = INT_MIN;
		demo.snapshots[1].configStringTimes[i] = INT_MIN;
	}

	MB_InitWrite(&demo.buffer);

	// we won't have a working progress bar for demo files inside pk3 files
	const int fileLength = FS_IsZipFile(clc.demofile) ? 0 : FS_filelength(clc.demofile);

	for (;;) {
		parser.progress = fileLength > 0 ? ((100 * FS_FTell(clc.demofile)) / fileLength) : 0;

		MSG_Init(msg, oldData, sizeof(oldData));

		if (FS_Read(&parser.messageNum, 4, fh) != 4) {
			Error("File read: message number");
		}

		if (FS_Read(&msg->cursize, 4, fh) != 4) {
			Error("File read: message size");
		}
		if (msg->cursize < 0) {
			// valid EOF marker
			break;
		} else if (msg->cursize > msg->maxsize) {
			Error("Message too large: %d", msg->cursize);
		}

		if (FS_Read(msg->data, msg->cursize, fh) != msg->cursize) {
			Error("File read: message data");
		}

		MSG_BeginReading(msg);
		MSG_ReadLong(msg); // ignore sequence number

		for (;;) {
			if (msg->readcount > msg->cursize) {
				Error("Read past end of message");
			}

			const byte cmd = MSG_ReadByte(msg);
			if (cmd == svc_EOF) {
				break;
			}

			switch (cmd) {
				case svc_nop:
					break;

				case svc_gamestate:
					++numGamestates;
					if (numGamestates >= 2) {
						Warning("More than 1 gamestate found, only the first one was loaded");
						return;
					}
					ParseGamestate();
					break;

				case svc_snapshot:
					ParseSnapshot();
					break;

				case svc_serverCommand:
					ParseServerCommand();
					break;

				case svc_download:
				default:
					Error("Invalid command byte");
					break;
			}
		}
	}
}


static void ReadNextSnapshot()
{
	if (demo.buffer.position >= demo.buffer.numBytes) {
		demo.isLastSnapshot = qtrue;
		return;
	}
	demo.isLastSnapshot = qfalse;

	msg_t inMsg;
	MB_InitMessage(&demo.buffer, &inMsg);

	ndpSnapshot_t* currSnap = demo.currSnap; // destination
	ndpSnapshot_t* prevSnap = demo.nextSnap; // source
	demo.currSnap = prevSnap;
	demo.nextSnap = currSnap;

	// header
	const qbool isFullSnap = MSG_ReadBits(&inMsg, 1);
	if (isFullSnap) {
		prevSnap = NULL;
	}
	currSnap->isFullSnap = isFullSnap;
	currSnap->serverTime = MSG_ReadLong(&inMsg);
	currSnap->isServerPaused = MSG_ReadBits(&inMsg, 1);

	// player state
	MSG_ReadDeltaPlayerstate(&inMsg, prevSnap ? &prevSnap->ps : &nullPlayerState, &currSnap->ps);

	// area mask
	currSnap->numAreaMaskBytes = MSG_ReadByte(&inMsg);
	MSG_ReadData(&inMsg, currSnap->areaMask, currSnap->numAreaMaskBytes);

	// server commands
	currSnap->serverCommands.numBytes = MSG_ReadLong(&inMsg);
	MSG_ReadData(&inMsg, currSnap->serverCommands.data, currSnap->serverCommands.numBytes);

	// synchronization commands
	if (isFullSnap) {
		currSnap->synchCommands.numBytes = MSG_ReadLong(&inMsg);
		MSG_ReadData(&inMsg, currSnap->synchCommands.data, currSnap->synchCommands.numBytes);
	}

	// config strings
	currSnap->configStrings[0] = '\0';
	currSnap->numConfigStringBytes = 1;
	if (isFullSnap) {
		// if we have a full snapshot, the config strings from the message are all we need
		// we can therefore safely leave everything else as empty (i.e. offset 0)
		Com_Memset(currSnap->configStringOffsets, 0, sizeof(currSnap->configStringOffsets));
		Com_Memset(currSnap->configStringTimes, 0, sizeof(currSnap->configStringTimes));
	}
	qbool csSet[MAX_CONFIGSTRINGS];
	Com_Memset(csSet, 0, sizeof(csSet));
	for (;;) {
		const int index = MSG_ReadShort(&inMsg);
		if (index == MAX_CONFIGSTRINGS) {
			break;
		}

		const int serverTime = MSG_ReadLong(&inMsg);
		const char* const cs = MSG_ReadBigString(&inMsg);
		SaveConfigString(currSnap, index, cs, serverTime);
		csSet[index] = qtrue;
	}
	if (!isFullSnap) {
		// we need to keep the unchanged config strings
		for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
			if (!csSet[i]) {
				const char* const cs = prevSnap->configStrings + prevSnap->configStringOffsets[i];
				const int serverTime = prevSnap->configStringTimes[i];
				SaveConfigString(currSnap, i, cs, serverTime);
			}
		}
	}

	// entities
	if (isFullSnap) {
		// mark them all as invalid now
		for (int i = 0; i < MAX_GENTITIES; ++i) {
			currSnap->entities[i].number = MAX_GENTITIES - 1;
		}
	}
	qbool entSet[MAX_GENTITIES];
	Com_Memset(entSet, 0, sizeof(entSet));
	int prevIndex = -1;
	for (;;) {
		const int index = MSG_ReadBits(&inMsg, GENTITYNUM_BITS);
		Q_assert(index > prevIndex);
		prevIndex = index;
		if (index == MAX_GENTITIES - 1) {
			break;
		}
		const entityState_t* prevEnt = &nullEntityState;
		entityState_t* currEnt = &currSnap->entities[index];
		if (prevSnap && prevSnap->entities[index].number == index) {
			prevEnt = &prevSnap->entities[index];
		}
		MSG_ReadDeltaEntity(&inMsg, prevEnt, currEnt, index);
		entSet[index] = qtrue;
	}
	if (!isFullSnap) {
		// copy over old valid entities and mark missing ones as invalid
		for (int i = 0; i < MAX_GENTITIES - 1; ++i) {
			if (entSet[i]) {
				continue;
			}
			if (prevSnap->entities[i].number == i) {
				currSnap->entities[i] = prevSnap->entities[i];
			} else {
				currSnap->entities[i].number = MAX_GENTITIES - 1;
			}
		}
	}

	if (ndp.trackServerPause) {
		if (currSnap->isServerPaused && ndp.isServerPaused && prevSnap != NULL) {
			const int delta = currSnap->serverTime - prevSnap->serverTime;
			ndp.serverPauseDelay += delta;
		}
		ndp.isServerPaused = currSnap->isServerPaused;
	}

	demo.snapshotIndex++;
}


static int PeekNextSnapshotTime()
{
	const int serverTime = demo.nextSnap != NULL ? demo.nextSnap->serverTime : INT_MIN;

	return serverTime;
}


static void AddCommand( const char* string )
{
	PrintCmd(string);
	const int index = ++demo.numCommands % MAX_COMMANDS;
	Q_strncpyz(demo.commands[index].command, string, sizeof(demo.commands[index].command));
}


static void AddAllCommands( const commandBuffer_t* cmdBuf )
{
	int cmdStart = 0;
	for (;;) {
		if (cmdStart >= cmdBuf->numBytes) {
			break;
		}
		const char* const cmd = cmdBuf->data + cmdStart;
		AddCommand(cmd);
		cmdStart += strlen(cmd) + 1;
	}
}


static void SeekToIndex( int seekIndex )
{
	if (seekIndex < 0) {
		Warning("Attempted to seek out of range");
		seekIndex = 0;
	} else if (seekIndex >= demo.numIndices) {
		seekIndex = demo.numIndices - 1;
	}

	const demoIndex_t* demoIndex = &demo.indices[seekIndex];
	MB_Seek(&demo.buffer, demoIndex->byteOffset);
	ReadNextSnapshot(); // 1st in next, ??? in current
	ReadNextSnapshot(); // 2nd in next, 1st in current
	demo.snapshotIndex = demoIndex->snapshotIndex;

	ndpSnapshot_t* syncSnap = demo.currSnap;
	if (!demo.currSnap->isFullSnap) {
		// this is needed when trying to jump past the end
		syncSnap = demo.nextSnap;
	}
	Q_assert(syncSnap->isFullSnap);

	// queue special config string changes as commands
	// this will correct the red/blue scores, the game timer, etc.
	configString_t cs[MAX_CONFIGSTRINGS];
	int numCS = 0;
	for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
		if (syncSnap->configStringTimes[i] != INT_MIN &&
		    CL_CGNDP_IsConfigStringNeeded(i)) {
			cs[numCS].index = i;
			cs[numCS].serverTime = syncSnap->configStringTimes[i];
			numCS++;
		}
	}

	// sort them so they're executed in the right order
	// yes, it matters a great deal due to some old internal CPMA mechanisms
	// for network bandwidth optimization
	// this system has existed for at least 15 years before I got on board,
	// so you don't get to blame me :-)
	PrintCmdListBegin("synch CS commands");
	qsort(cs, numCS, sizeof(configString_t), &CompareConfigStringTimes);
	for (int i = 0; i < numCS; ++i) {
		const int csIndex = cs[i].index;
		const char* const csValue = syncSnap->configStrings + syncSnap->configStringOffsets[csIndex];
		AddCommand(va("cs %d \"%s\"", csIndex, csValue));
	}
	PrintCmdListEnd();

	// add special state synchronization commands
	// this will correct item timers, make sure the scoreboard can be shown, etc.
	PrintCmdListBegin("synch snap commands");
	AddAllCommands(&syncSnap->synchCommands);
	PrintCmdListEnd();
}


void CL_NDP_PlayDemo( qbool videoRestart )
{
	const int startTime = Sys_Milliseconds();

	if (setjmp(ndp.abortLoad)) {
		goto after_parse;
	}

	// read from file and write to memory
	ParseDemo();

after_parse:
	int fileSize = FS_FTell(clc.demofile);
	fileSize = max(fileSize, 1); // FS_FTell doesn't work when the demo is in a pak file
	FS_FCloseFile(clc.demofile);
	clc.demofile = 0;
	if (!clc.demoplaying || !clc.newDemoPlayer) {
		Drop("Failed to parse the gamestate message");
	}
	if (demo.numIndices <= 0 || demo.numSnapshots <= 0) {
		Warning("Failed to get anything out of the demo file");
		return;
	}

#if defined(_DEBUG)
	if (com_developer->integer) {
		FS_WriteFile(va("demos/%s.bin", clc.demoName), demo.buffer.data, demo.buffer.position);
	}
#endif

	// prepare to read from memory
	demo.currSnap = &demo.snapshots[0];
	demo.nextSnap = &demo.snapshots[1];
	MB_InitRead(&demo.buffer);

	// finalize CGame load and set any extra info needed now
	// CGame will also restore previous state when videoRestart is qtrue
	CL_CGNDP_EndAnalysis(clc.demoName, demo.firstServerTime, demo.lastServerTime, videoRestart);

	// make sure we don't execute commands from the end of the demo when starting up
	demo.commands[0].command[0] = '\0';
	demo.commands[1].command[0] = '\0';
	demo.numCommands = 1;

	const int duration = Sys_Milliseconds() - startTime;
	Com_Printf("New Demo Player: loaded demo in %d.%03d seconds\n", duration / 1000, duration % 1000);
	Com_DPrintf("New Demo Player: I-frame delay %d ms, %s -> %s (%.2fx)\n",
	            FULL_SNAPSHOT_INTERVAL_MS, Com_FormatBytes(fileSize),
	            Com_FormatBytes(demo.buffer.numBytes), (float)demo.buffer.numBytes / (float)fileSize);
}


void CL_NDP_SetCGameTime()
{
	// make sure we don't get timed out by CL_CheckTimeout
	clc.lastPacketTime = cls.realtime;
}


void CL_NDP_GetCurrentSnapshotNumber( int* snapshotNumber, int* serverTime )
{
	*snapshotNumber = demo.snapshotIndex;
	*serverTime = demo.currSnap->serverTime;
}


qbool CL_NDP_GetSnapshot( int snapshotNumber, snapshot_t* snapshot )
{
	// we don't give anything until CGame init is done
	if (!cls.cgameStarted || cls.state != CA_ACTIVE) {
		return qfalse;
	}

	// we only allow CGame to get the current snapshot and the next one
	const int playbackIndex = demo.snapshotIndex;
	if (snapshotNumber < playbackIndex ||
	    snapshotNumber > playbackIndex + 1) {
		return qfalse;
	}

	// there is no next snapshot when we're already at the end...
	if (demo.isLastSnapshot && snapshotNumber == playbackIndex + 1) {
		return qfalse;
	}

	ndpSnapshot_t* const ndpSnap = snapshotNumber == playbackIndex ? demo.currSnap : demo.nextSnap;

	Com_Memcpy(snapshot->areamask, ndpSnap->areaMask, sizeof(snapshot->areamask));
	Com_Memcpy(&snapshot->ps, &ndpSnap->ps, sizeof(snapshot->ps));
	snapshot->snapFlags = 0;
	snapshot->ping = 0;
	snapshot->numServerCommands = 0;
	snapshot->serverCommandSequence = demo.numCommands;
	snapshot->serverTime = ndpSnap->serverTime;
	snapshot->numEntities = 0;

	// copy over all valid entities
	for (int i = 0; i < MAX_GENTITIES - 1 && snapshot->numEntities < MAX_ENTITIES_IN_SNAPSHOT; ++i) {
		if (ndpSnap->entities[i].number == i) {
			snapshot->entities[snapshot->numEntities++] = ndpSnap->entities[i];
		}
	}

	const int prevNumCommands = demo.numCommands;

	// add config string changes as commands
	PrintCmdListBegin("snap config string changes as commands");
	configString_t cs[MAX_CONFIGSTRINGS];
	int numCS = 0;
	for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
		const char* const csOld = cl.gameState.stringData + cl.gameState.stringOffsets[i];
		const char* const csNew = ndpSnap->configStrings + ndpSnap->configStringOffsets[i];
		if (strcmp(csOld, csNew)) {
			cs[numCS].index = i;
			cs[numCS].serverTime = ndpSnap->configStringTimes[i];
			numCS++;
		}
	}
	qsort(cs, numCS, sizeof(configString_t), &CompareConfigStringTimes);
	for (int i = 0; i < numCS; ++i) {
		const int index = cs[i].index;
		const char* const csNew = ndpSnap->configStrings + ndpSnap->configStringOffsets[index];
		AddCommand(va("cs %d \"%s\"", index, csNew));
	}
	PrintCmdListEnd();

	// add commands
	PrintCmdListBegin("add snap commands");
	AddAllCommands(&ndpSnap->serverCommands);
	PrintCmdListEnd();

	// make sure nothing is missing
	if (demo.numCommands - prevNumCommands > MAX_COMMANDS) {
		Warning("Too many commands for a single snapshot");
	}

	return qtrue;
}


qbool CL_NDP_GetServerCommand( int serverCommandNumber )
{
	if (serverCommandNumber < demo.numCommands - MAX_COMMANDS ||
	    serverCommandNumber > demo.numCommands) {
		return qfalse;
	}

	const int index = serverCommandNumber % MAX_COMMANDS;
	const char* const cmd = demo.commands[index].command;
	Cmd_TokenizeString(cmd);

	if (!strcmp(Cmd_Argv(0), "cs")) {
		CL_ConfigstringModified();
		// reparse the string, because CL_ConfigstringModified may have done another Cmd_TokenizeString()
		Cmd_TokenizeString(cmd);
	}

	return qtrue;
}


int CL_NDP_Seek( int serverTime )
{
	const int seekStartTime = Sys_Milliseconds();

	// figure out which full snapshot we want to jump to
	int index = 0;
	if (serverTime >= demo.lastServerTime) {
		index = demo.numIndices - 1;
	} else if (serverTime <= 0) {
		index = 0;
	} else {
		index = demo.numIndices - 1;
		for (int i = 0; i < demo.numIndices; ++i) {
			if (demo.indices[i].serverTime > serverTime) {
				index = max(i - 1, 0);
				break;
			}
		}
	}

	// clear all pending commands before adding new ones
	for (int i = 0; i < ARRAY_LEN(demo.commands); ++i) {
		demo.commands[i].command[0] = '\0';
	}

	ndp.trackServerPause = qtrue;
	ndp.isServerPaused = qfalse;
	ndp.serverPauseDelay = 0;

	SeekToIndex(index);

	// read more snapshots until we're close to the target time for more precise jumps
	// we want the the closest snapshot whose time is less or equal to the target time
	int numSnapshotsRead = 2;
	while (!demo.isLastSnapshot) {
		const int nextServerTime = PeekNextSnapshotTime();
		if (nextServerTime == INT_MIN || nextServerTime > serverTime) {
			break;
		}
		// add all commands from the snapshot we're about to evict
		PrintCmdListBegin("evicted snap commands");
		AddAllCommands(&demo.currSnap->serverCommands);
		PrintCmdListEnd();
		ReadNextSnapshot();
		numSnapshotsRead++;
	}

	if (ndp.serverPauseDelay > 0) {
		AddCommand(va("server_pause_delay %d", ndp.serverPauseDelay));
	}
	ndp.trackServerPause = qfalse;

	Com_DPrintf("New Demo Player: sought and read %d snaps in %d ms\n",
	            numSnapshotsRead, Sys_Milliseconds() - seekStartTime);

	return demo.currSnap->serverTime;
}


void CL_NDP_ReadUntil( int serverTime )
{
	while (!demo.isLastSnapshot && demo.currSnap->serverTime < serverTime) {
		ReadNextSnapshot();
	}
}


void CL_NDP_HandleError()
{
	// we use our handler for read errors during load
	// read/write errors during playback aren't handled
	if (clc.newDemoPlayer && clc.demofile != 0) {
		longjmp(ndp.abortLoad, -1);
	}
}

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
// common.c -- misc functions used in client and server

#include "q_shared.h"
#include "qcommon.h"
#include "common_help.h"
#include "git.h"
#include <setjmp.h>
#include <float.h>

#ifndef INT64_MIN
#	define INT64_MIN 0x8000000000000000LL
#endif

#if (_MSC_VER >= 1400) // Visual C++ 2005 or later
#define MSVC_CPUID 1
#include <intrin.h>
#elif (__GNUC__)
#define GCC_CPUID 1
#include <cpuid.h>
#endif

#ifndef _WIN32
#include <netinet/in.h>
#include <sys/stat.h> // umask
#endif

#if defined(_MSC_VER)
#include "../win32/windows.h"
#endif


#define MIN_COMHUNKMEGS_DED      8  // for the dedicated server
#define MIN_COMHUNKMEGS         56
#define CST_COMZONEMEGS         32
#ifdef DEDICATED
#define DEF_COMHUNKMEGS         64
#define CST_SMALLZONEMEGS        1
#else
#define DEF_COMHUNKMEGS        128
#define CST_SMALLZONEMEGS        4
#endif


static jmp_buf abortframe;		// an ERR_DROP occured, exit the entire frame


static fileHandle_t	logfile = 0;
static fileHandle_t	com_journalFile = 0;		// events are written here
fileHandle_t		com_journalDataFile = 0;		// config files are written here

cvar_t	*com_viewlog = 0;
cvar_t	*com_speeds = 0;
cvar_t	*com_developer = 0;
cvar_t	*com_dedicated = 0;
cvar_t	*com_timescale = 0;
cvar_t	*com_fixedtime = 0;
cvar_t	*com_journal = 0;
cvar_t	*com_maxfps = 0;
cvar_t	*com_timedemo = 0;
cvar_t	*com_sv_running = 0;
cvar_t	*com_cl_running = 0;
cvar_t	*com_logfile = 0;		// 1 = buffer log, 2 = flush after each print
cvar_t	*com_showtrace = 0;
cvar_t	*com_version = 0;
cvar_t	*cl_paused = 0;
cvar_t	*sv_paused = 0;
cvar_t	*cl_packetdelay = 0;
cvar_t	*sv_packetdelay = 0;
#if defined(_WIN32) && defined(_DEBUG)
cvar_t	*com_noErrorInterrupt;
#endif

static cvar_t	*con_completionStyle;	// 0 = legacy, 1 = ET-style
static cvar_t	*con_history;

int		time_game; // for com_speeds

int		com_frameTime;
int		com_frameNumber;

qbool	com_errorEntered;
qbool	com_fullyInitialized;

int64_t	com_nextTargetTimeUS = INT64_MIN;

static char com_errorMessage[MAXPRINTMSG];

const char* const com_cnq3VersionWithHash = Q3_VERSION " " GIT_COMMIT_SHORT;
const char* const com_gitBranch = GIT_BRANCH;
const char* const com_gitCommit = GIT_COMMIT;

static void Com_WriteConfigToFile( const char* filename, qbool forceWrite );
static void Com_WriteConfig_f();
static void Com_CompleteWriteConfig_f( int startArg, int compArg );
static const char* Com_GetCompilerInfo();
extern void CIN_CloseAllVideos( void );

//============================================================================

static char	*rd_buffer;
static int	rd_buffersize;
static void	(*rd_flush)( char *buffer );

void Com_BeginRedirect (char *buffer, int buffersize, void (*flush)( char *) )
{
	if (!buffer || !buffersize || !flush)
		return;
	rd_buffer = buffer;
	rd_buffersize = buffersize;
	rd_flush = flush;

	*rd_buffer = 0;
}

void Com_EndRedirect (void)
{
	if ( rd_flush ) {
		rd_flush(rd_buffer);
	}

	rd_buffer = NULL;
	rd_buffersize = 0;
	rd_flush = NULL;
}


///////////////////////////////////////////////////////////////

// client and server can both use these, and will do the appropriate things


// a raw string should NEVER be passed as fmt, because of "%f" type crashers

void QDECL Com_Printf( PRINTF_FORMAT_STRING const char *fmt, ... )
{
	char msg[MAXPRINTMSG];

	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof(msg), fmt, argptr );
	va_end( argptr );

	if ( rd_buffer ) {
		if ((strlen (msg) + strlen(rd_buffer)) > (rd_buffersize - 1)) {
			rd_flush(rd_buffer);
			*rd_buffer = 0;
		}
		Q_strcat(rd_buffer, rd_buffersize, msg);
		return;
	}

#ifndef DEDICATED
	// echo to console if we're not a dedicated server
	if ( com_dedicated && !com_dedicated->integer ) {
		CL_ConsolePrint( msg );
	}
#endif

	// echo to dedicated console and early console
	Sys_Print( msg );

	if ( !com_logfile || !com_logfile->integer )
		return;

	static qbool opening_qconsole = qfalse;

	// TTimo: only open the qconsole.log if the filesystem is in an initialized state
	//   also, avoid recursing in the qconsole.log opening (i.e. if fs_debug is on)
	if ( !logfile && FS_Initialized() && !opening_qconsole) {
		opening_qconsole = qtrue;

		time_t aclock;
		time( &aclock );
		struct tm* newtime = localtime( &aclock );

		logfile = FS_FOpenFileWrite( "qconsole.log" );
		if (logfile)
		{
			Com_Printf( "logfile opened on %s\n", asctime( newtime ) );
			if ( com_logfile->integer > 1 )
			{
				// force it to not buffer so we get valid
				// data even if we are crashing
				FS_ForceFlush(logfile);
			}
		}
		else
		{
			Com_Printf("Opening qconsole.log failed!\n");
			Cvar_SetValue("logfile", 0);
		}

		opening_qconsole = qfalse;
	}

	if ( logfile && FS_Initialized() ) {
		FS_Write( msg, strlen(msg), logfile );
	}
}


// a Com_Printf that only shows up if the "developer" cvar is set

void QDECL Com_DPrintf( PRINTF_FORMAT_STRING const char *fmt, ...)
{
	// don't confuse non-developers with techie stuff...
	if ( !com_developer || !com_developer->integer )
		return;

	char msg[MAXPRINTMSG];

	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof(msg), fmt, argptr );
	Com_Printf( "%s", msg );
	va_end( argptr );
}


void QDECL Com_Error( int level, PRINTF_FORMAT_STRING const char* fmt, ... )
{
	static char msg[MAXPRINTMSG];

	va_list argptr;
	va_start( argptr, fmt );
	Q_vsnprintf( msg, sizeof(msg), fmt, argptr );
	Com_ErrorExt( level, EXT_ERRMOD_ENGINE, qtrue, "%s", msg );
	va_end(argptr);
}


void QDECL Com_ErrorExt( int code, int module, qbool realError, PRINTF_FORMAT_STRING const char *fmt, ... )
{
	static int	lastErrorTime;
	static int	errorCount;

	if ( code == ERR_DROP_NDP ) {
#if !defined(DEDICATED)
		CL_NDP_HandleError();
#endif
		code = ERR_DROP;
	}

	// make sure we can get at our local stuff
	FS_PureServerSetLoadedPaks( "" );

	// if we are getting a solid stream of ERR_DROP, do an ERR_FATAL
	int currentTime = Sys_Milliseconds();
	if ( currentTime - lastErrorTime < 100 ) {
		if ( ++errorCount > 3 ) {
			code = ERR_FATAL;
		}
	} else {
		errorCount = 0;
	}
	lastErrorTime = currentTime;

	if ( com_errorEntered ) {
		Sys_Error( "recursive error after: %s", com_errorMessage );
	}
	com_errorEntered = qtrue;

	va_list argptr;
	va_start( argptr, fmt );
	vsprintf( com_errorMessage, fmt, argptr );
	va_end( argptr );

#if defined(_MSC_VER) && defined(_DEBUG)
	if ( code != ERR_DISCONNECT && code != ERR_NEED_CD ) {
		if ( realError && (!com_noErrorInterrupt || !com_noErrorInterrupt->integer) && Sys_IsDebuggerAttached() )
			__debugbreak();
	}
#endif

	if ( code != ERR_DISCONNECT && code != ERR_NEED_CD ) {
		Cvar_Set("com_errorMessage", com_errorMessage);
	}

#ifndef DEDICATED
	CL_DisableFramerateLimiter();
#endif

	if ( code == ERR_SERVERDISCONNECT ) {
#ifndef DEDICATED
		CL_Disconnect( qtrue );
		CL_FlushMemory();
		if ( realError )
			CL_ForwardUIError( EXT_ERRLEV_SVDISC, module, com_errorMessage );
#endif
		com_errorEntered = qfalse;
		longjmp (abortframe, -1);
	} else if ( code == ERR_DROP || code == ERR_DISCONNECT ) {
		if ( realError )
			Com_Printf( "********************\nERROR: %s\n********************\n", com_errorMessage );
		else
			Com_Printf( "ERROR: %s\n", com_errorMessage );
		SV_Shutdown( va("Server crashed: %s",  com_errorMessage) );
#ifndef DEDICATED
		const qbool demo = CL_DemoPlaying();
		CL_Disconnect( qtrue );
		CL_FlushMemory(); // shuts down the VMs and starts them back up
		if ( demo && *Cvar_VariableString("nextdemo") != '\0' )
			CL_NextDemo();
		else if ( realError )
			CL_ForwardUIError( code == ERR_DROP ? EXT_ERRLEV_DROP : EXT_ERRLEV_DISC, module, com_errorMessage );
#endif
		com_errorEntered = qfalse;
		longjmp (abortframe, -1);
	} else if ( code == ERR_NEED_CD ) {
		SV_Shutdown( "Server didn't have CD" );
#ifndef DEDICATED
		if ( com_cl_running && com_cl_running->integer ) {
			CL_Disconnect( qtrue );
			CL_FlushMemory();
			com_errorEntered = qfalse;
			CL_CDDialog();
		} else {
			Com_Printf("Server didn't have CD\n" );
		}
#endif
		longjmp (abortframe, -1);
	} else {
#ifndef DEDICATED
		CL_Shutdown();
#endif
		SV_Shutdown( va("Server fatal crashed: %s", com_errorMessage) );
	}

	Com_Shutdown();

	Sys_Error( "%s", com_errorMessage );
}


void Com_Quit( int status )
{
#ifndef DEDICATED
	// note that cvar_modifiedFlags's CVAR_ARCHIVE bit is set when a bind is modified
	if ( com_frameNumber > 0 && (cvar_modifiedFlags & CVAR_ARCHIVE) != 0 )
		Com_WriteConfigToFile( "q3config.cfg", qfalse );
#endif
	Sys_SaveHistory();

	// don't try to shutdown if we are in a recursive error
	if ( !com_errorEntered ) {
		SV_Shutdown( "Server quit" );
#ifndef DEDICATED
		CL_Shutdown();
#endif
		Com_Shutdown();
		FS_Shutdown( qtrue );
	}

	Sys_Quit( status );
}


static void Com_Quit_f( void )
{
	Com_Quit( 0 );
}


///////////////////////////////////////////////////////////////


/*
COMMAND LINE FUNCTIONS

+ characters seperate the commandLine string into multiple console
command lines.

All of these are valid:

quake3 +set test blah +map test
quake3 set test blah+map test
quake3 set test blah + map test

*/

#define MAX_CONSOLE_LINES	32
static int com_numConsoleLines;
static char* com_consoleLines[MAX_CONSOLE_LINES];


// break the process args up into multiple console lines

static void Com_ParseCommandLine( char* commandLine )
{
	int inq = 0;
	com_consoleLines[0] = commandLine;
	com_numConsoleLines = 1;

	while ( *commandLine ) {
		if (*commandLine == '"') {
			inq = !inq;
		}
		// look for a + separating character
		// if commandLine came from a file, we might have real line separators
		if ( (*commandLine == '+' && !inq) || *commandLine == '\n'  || *commandLine == '\r' ) {
			if ( com_numConsoleLines == MAX_CONSOLE_LINES ) {
				return;
			}
			*commandLine++ = 0; // terminate the PREVIOUS command
			com_consoleLines[com_numConsoleLines++] = commandLine;
		}
		++commandLine;
	}
}


// check for "+safe" on the command line, which will skip loading of q3config.cfg

qbool Com_SafeMode()
{
	for (int i = 0; i < com_numConsoleLines; ++i) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( !Q_stricmp( Cmd_Argv(0), "safe" ) || !Q_stricmp( Cmd_Argv(0), "cvar_restart" ) ) {
			com_consoleLines[i][0] = 0;
			return qtrue;
		}
	}

	return qfalse;
}


/*
Searches for command line parameters that are set commands.
If match is not NULL, only that cvar will be looked for.
That is necessary because cddir and basedir need to be set
before the filesystem is started, but all other sets should
be after execing the config and default.
*/
void Com_StartupVariable( const char *match )
{
	for (int i = 0; i < com_numConsoleLines; ++i) {
		Cmd_TokenizeString( com_consoleLines[i] );
		if ( strcmp( Cmd_Argv(0), "set" ) )
			continue;

		const char* s = Cmd_Argv(1);
		if ( !match || !strcmp( s, match ) ) {
			Cvar_Set( s, Cmd_Argv(2) );
			cvar_t* cv = Cvar_Get( s, "", 0 );
			cv->flags |= CVAR_USER_CREATED | CVAR_CMDLINE_CREATED;
		}
	}
}


/*
Adds command line parameters as script statements
Commands are separated by + signs

Returns qtrue if any late commands were added
which will keep the fucking annoying cins from playing
*/
static qbool Com_AddStartupCommands()
{
	qbool added = qfalse;

	// quote every token, so args with semicolons can work
	for (int i = 0; i < com_numConsoleLines; ++i) {
		if ( !com_consoleLines[i] || !com_consoleLines[i][0] ) {
			continue;
		}

		// set commands won't override menu startup
		if ( Q_stricmpn( com_consoleLines[i], "set", 3 ) ) {
			added = qtrue;
		}

		Cbuf_AddText( com_consoleLines[i] );
		Cbuf_AddText( "\n" );
	}

	return added;
}


//============================================================================

void Info_Print( const char* string )
{
	char key[BIG_INFO_KEY];
	char value[BIG_INFO_VALUE];
	const char* const valueMissing = "missing value";

	const char* s = string;
	int width = 0;
	while (*s != '\0') {
		Info_NextPair(&s, key, value);
		if (key[0] == '\0') {
			break;
		}

		const int l = strlen(key);
		width = max(width, l);
	};
	width += 2;

	s = string;
	while (*s != '\0') {
		Info_NextPair(&s, key, value);
		if (key[0] == '\0') {
			break;
		}

		const char* const valuePtr = value[0] != '\0' ? value : valueMissing;
		Com_Printf(S_COLOR_CVAR "%-*s " S_COLOR_VAL "%s\n", width, key, valuePtr);
	};
}


static const char* Com_StringContains( const char* str1, const char* str2)
{
	int i, j;

	int len = strlen(str1) - strlen(str2);
	for (i = 0; i <= len; i++, str1++) {
		for (j = 0; str2[j]; j++) {
			if (toupper(str1[j]) != toupper(str2[j])) {
				break;
			}
		}
		if (!str2[j]) {
			return str1;
		}
	}
	return NULL;
}


int Com_Filter( const char* filter, const char* name )
{
	char buf[MAX_TOKEN_CHARS];
	const char* ptr;
	int i, found;

	while (*filter) {
		if (*filter == '*') {
			filter++;
			for (i = 0; *filter; i++) {
				if (*filter == '*' || *filter == '?') break;
				buf[i] = *filter;
				filter++;
			}
			buf[i] = '\0';
			if (buf[0]) {
				ptr = Com_StringContains(name, buf);
				if (!ptr) return qfalse;
				name = ptr + strlen(buf);
			}
		}
		else if (*filter == '?') {
			filter++;
			name++;
		}
		else if (*filter == '[' && *(filter+1) == '[') {
			filter++;
		}
		else if (*filter == '[') {
			filter++;
			found = qfalse;
			while(*filter && !found) {
				if (*filter == ']' && *(filter+1) != ']') break;
				if (*(filter+1) == '-' && *(filter+2) && (*(filter+2) != ']' || *(filter+3) == ']')) {
					if (toupper(*name) >= toupper(*filter) && toupper(*name) <= toupper(*(filter+2)))
						found = qtrue;
					filter += 3;
				}
				else {
					if (toupper(*filter) == toupper(*name))
						found = qtrue;
					filter++;
				}
			}
			if (!found) return qfalse;
			while(*filter) {
				if (*filter == ']' && *(filter+1) != ']') break;
				filter++;
			}
			filter++;
			name++;
		}
		else {
			if (toupper(*filter) != toupper(*name))
				return qfalse;
			filter++;
			name++;
		}
	}

	return qtrue;
}


int Com_FilterPath( const char* filter, const char* name )
{
	int i;
	char new_filter[MAX_QPATH];
	char new_name[MAX_QPATH];

	for (i = 0; i < MAX_QPATH-1 && filter[i]; i++) {
		if ( filter[i] == '\\' || filter[i] == ':' ) {
			new_filter[i] = '/';
		}
		else {
			new_filter[i] = filter[i];
		}
	}
	new_filter[i] = '\0';

	for (i = 0; i < MAX_QPATH-1 && name[i]; i++) {
		if ( name[i] == '\\' || name[i] == ':' ) {
			new_name[i] = '/';
		}
		else {
			new_name[i] = name[i];
		}
	}
	new_name[i] = '\0';

	return Com_Filter( new_filter, new_name );
}

/*
============
Com_HashKey
============
*/
int Com_HashKey(char *string, int maxlen) {
	int register hash, i;

	hash = 0;
	for (i = 0; i < maxlen && string[i] != '\0'; i++) {
		hash += string[i] * (119 + i);
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	return hash;
}


int Com_RealTime( qtime_t* qtime )
{
	time_t t = time(NULL);
	if (!qtime)
		return t;

	const struct tm* tms = localtime(&t);
	if (tms) {
		qtime->tm_sec = tms->tm_sec;
		qtime->tm_min = tms->tm_min;
		qtime->tm_hour = tms->tm_hour;
		qtime->tm_mday = tms->tm_mday;
		qtime->tm_mon = tms->tm_mon;
		qtime->tm_year = tms->tm_year;
		qtime->tm_wday = tms->tm_wday;
		qtime->tm_yday = tms->tm_yday;
		qtime->tm_isdst = tms->tm_isdst;
	}

	return t;
}


/*
==============================================================================

						ZONE MEMORY ALLOCATION

There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

The zone calls are pretty much only used for small strings and structures,
all big things are allocated on the hunk.
==============================================================================
*/

#define	ZONEID	0x1d4a11
#define MINFRAGMENT	64

typedef struct zonedebug_s {
	char *label;
	char *file;
	int line;
	int allocSize;
} zonedebug_t;

typedef struct memblock_s {
	int		size;           // including the header and possibly tiny fragments
	int     tag;            // a tag of 0 is a free block
	struct memblock_s       *next, *prev;
	int     id;        		// should be ZONEID
#ifdef ZONE_DEBUG
	zonedebug_t d;
#endif
} memblock_t;

typedef struct {
	int		size;			// total bytes malloced, including header
	int		used;			// total bytes used
	memblock_t	blocklist;	// start / end cap for linked list
	memblock_t	*rover;
} memzone_t;

// main zone for all "dynamic" memory allocation
static memzone_t* mainzone = NULL;
// we also have a small zone for small allocations that would only
// fragment the main zone (think of cvar and cmd strings)
static memzone_t* smallzone = NULL;


static void Z_ClearZone( memzone_t* zone, int size )
{
	memblock_t* block;

	// set the entire zone to one free block

	zone->blocklist.next = zone->blocklist.prev = block =
		(memblock_t *)( (byte *)zone + sizeof(memzone_t) );
	zone->blocklist.tag = 1;	// in use block
	zone->blocklist.id = 0;
	zone->blocklist.size = 0;
	zone->rover = block;
	zone->size = size;
	zone->used = 0;

	block->prev = block->next = &zone->blocklist;
	block->tag = 0;			// free block
	block->id = ZONEID;
	block->size = size - sizeof(memzone_t);
}


static int Z_AvailableZoneMemory( const memzone_t* zone )
{
	return zone->size - zone->used;
}


int Z_AvailableMemory( void )
{
	return Z_AvailableZoneMemory( mainzone );
}


void Z_Free( void* ptr )
{
	if (!ptr) {
		Com_Error( ERR_DROP, "Z_Free: NULL pointer" );
	}

	memblock_t* block = (memblock_t*)((byte*)ptr - sizeof(memblock_t));
	if (block->id != ZONEID) {
		Com_Error( ERR_FATAL, "Z_Free: freed a pointer without ZONEID" );
	}
	if (block->tag == 0) {
		Com_Error( ERR_FATAL, "Z_Free: freed a freed pointer" );
	}
	// if static memory
	if (block->tag == TAG_STATIC) {
		return;
	}

	// check the memory trash tester
	if ( *(int *)((byte *)block + block->size - 4 ) != ZONEID ) {
		Com_Error( ERR_FATAL, "Z_Free: memory block wrote past end" );
	}

	memzone_t* zone = (block->tag == TAG_SMALL) ? smallzone : mainzone;
	zone->used -= block->size;
	// set the block to something that should cause problems
	// if it is referenced...
	Com_Memset( ptr, 0xaa, block->size - sizeof( *block ) );

	block->tag = 0;		// mark as free

	memblock_t* other = block->prev;
	if (!other->tag) {
		// merge with previous free block
		other->size += block->size;
		other->next = block->next;
		other->next->prev = other;
		if (block == zone->rover) {
			zone->rover = other;
		}
		block = other;
	}

	zone->rover = block;

	other = block->next;
	if ( !other->tag ) {
		// merge the next free block onto the end
		block->size += other->size;
		block->next = other->next;
		block->next->prev = block;
		if (other == zone->rover) {
			zone->rover = block;
		}
	}
}


#ifdef ZONE_DEBUG

static void Z_LogZoneHeap( const memzone_t* zone, const char* name )
{
	char dump[32], *ptr;
	int  i, j;
	memblock_t	*block;
	char		buf[4096];
	int size, allocSize, numBlocks;

	if (!logfile || !FS_Initialized())
		return;
	size = allocSize = numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\n%s log\r\n================\r\n", name);
	FS_Write(buf, strlen(buf), logfile);
	for (block = zone->blocklist.next ; block->next != &zone->blocklist; block = block->next) {
		if (block->tag) {
			ptr = ((char *) block) + sizeof(memblock_t);
			j = 0;
			for (i = 0; i < 20 && i < block->d.allocSize; i++) {
				if (ptr[i] >= 32 && ptr[i] < 127) {
					dump[j++] = ptr[i];
				}
				else {
					dump[j++] = '_';
				}
			}
			dump[j] = '\0';
			Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s) [%s]\r\n", block->d.allocSize, block->d.file, block->d.line, block->d.label, dump);
			FS_Write(buf, strlen(buf), logfile);
			allocSize += block->d.allocSize;
			size += block->size;
			numBlocks++;
		}
	}

	// subtract debug memory
	size -= numBlocks * sizeof(zonedebug_t);
	allocSize = numBlocks * sizeof(memblock_t); // + 32 bit alignment

	Com_sprintf(buf, sizeof(buf), "%d %s memory in %d blocks\r\n", size, name, numBlocks);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d %s memory overhead\r\n", size - allocSize, name);
	FS_Write(buf, strlen(buf), logfile);
}


static void Z_LogHeap()
{
	Z_LogZoneHeap( mainzone, "MAIN" );
	Z_LogZoneHeap( smallzone, "SMALL" );
}

#endif


/*
================
Z_TagMalloc
================
*/
#ifdef ZONE_DEBUG
void *Z_TagMallocDebug( int size, int tag, char *label, char *file, int line ) {
#else
void *Z_TagMalloc( int size, int tag ) {
#endif
	int		extra, allocSize;
	memblock_t	*start, *rover, *base;
	memzone_t *zone;

	if (!tag) {
		Com_Error( ERR_FATAL, "Z_TagMalloc: tried to use a 0 tag" );
	}

	if ( tag == TAG_SMALL ) {
		zone = smallzone;
	}
	else {
		zone = mainzone;
	}

	allocSize = size;
	//
	// scan through the block list looking for the first free block
	// of sufficient size
	//
	size += sizeof(memblock_t);	// account for size of block header
	size += 4;					// space for memory trash tester
	size = PAD(size, sizeof(intptr_t));		// align to 32/64 bit boundary

	base = rover = zone->rover;
	start = base->prev;

	do {
		if (rover == start) {
#ifdef ZONE_DEBUG
			Z_LogHeap();
#endif
			// scaned all the way around the list
			Com_Error( ERR_FATAL, "Z_Malloc: failed on allocation of %i bytes from the %s zone",
								size, zone == smallzone ? "small" : "main");
			return NULL;
		}
		if (rover->tag) {
			base = rover = rover->next;
		} else {
			rover = rover->next;
		}
	} while (base->tag || base->size < size);

	//
	// found a block big enough
	//
	extra = base->size - size;
	if (extra > MINFRAGMENT) {
		// there will be a free fragment after the allocated block
		memblock_t* p = (memblock_t *) ((byte *)base + size );
		p->size = extra;
		p->tag = 0;			// free block
		p->prev = base;
		p->id = ZONEID;
		p->next = base->next;
		p->next->prev = p;
		base->next = p;
		base->size = size;
	}

	base->tag = tag;			// no longer a free block

	zone->rover = base->next;	// next allocation will start looking here
	zone->used += base->size;	//

	base->id = ZONEID;

#ifdef ZONE_DEBUG
	base->d.label = label;
	base->d.file = file;
	base->d.line = line;
	base->d.allocSize = allocSize;
#endif

	// marker for memory trash testing
	*(int *)((byte *)base + base->size - 4) = ZONEID;

	return (void *) ((byte *)base + sizeof(memblock_t));
}


static void Z_CheckHeap()
{
	const memblock_t* block;

	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if (block->next == &mainzone->blocklist) {
			break;			// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next)
			Com_Error( ERR_FATAL, "Z_CheckHeap: block size does not touch the next block\n" );
		if ( block->next->prev != block) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: next block doesn't have proper back link\n" );
		}
		if ( !block->tag && !block->next->tag ) {
			Com_Error( ERR_FATAL, "Z_CheckHeap: two consecutive free blocks\n" );
		}
	}
}


#ifdef ZONE_DEBUG
void *Z_MallocDebug( int size, char *label, char *file, int line )
{
	Z_CheckHeap();
	void* buf = Z_TagMallocDebug( size, TAG_GENERAL, label, file, line );
#else
void *Z_Malloc( int size )
{
	void* buf = Z_TagMalloc( size, TAG_GENERAL );
#endif
	Com_Memset( buf, 0, size );

	return buf;
}


#ifdef ZONE_DEBUG
void *S_MallocDebug( int size, char *label, char *file, int line )
{
	return Z_TagMallocDebug( size, TAG_SMALL, label, file, line );
}
#else
void *S_Malloc( int size )
{
	return Z_TagMalloc( size, TAG_SMALL );
}
#endif


// static mem blocks to reduce a lot of small zone overhead
typedef struct memstatic_s {
	memblock_t b;
	byte mem[2];
} memstatic_t;

static memstatic_t emptystring =
	{ {(sizeof(memblock_t)+2 + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'\0', '\0'} };
static memstatic_t numberstring[] = {
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'0', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'1', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'2', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'3', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'4', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'5', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'6', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'7', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'8', '\0'} },
	{ {(sizeof(memstatic_t) + 3) & ~3, TAG_STATIC, NULL, NULL, ZONEID}, {'9', '\0'} }
};

/*
========================
CopyString

 NOTE:	never write over the memory CopyString returns because
		memory from a memstatic_t might be returned
========================
*/
char* CopyString( const char *in )
{
	if (!in[0]) {
		return ((char *)&emptystring) + sizeof(memblock_t);
	}
	else if (!in[1]) {
		if (in[0] >= '0' && in[0] <= '9') {
			return ((char *)&numberstring[in[0]-'0']) + sizeof(memblock_t);
		}
	}
	char* out = (char*)S_Malloc(strlen(in)+1);
	strcpy(out, in);
	return out;
}

/*
==============================================================================

Goals:
  reproducable without history effects -- no out of memory errors on weird map to map changes
  allow restarting of the client without fragmentation
  minimize total pages in use at run time
  minimize total pages needed during load time

  Single block of memory with stack allocators coming from both ends towards the middle.

  One side is designated the temporary memory allocator.

  Temporary memory can be allocated and freed in any order.

  A highwater mark is kept of the most in use at any time.

  When there is no temporary memory allocated, the permanent and temp sides
  can be switched, allowing the already touched temp memory to be used for
  permanent storage.

  Temp memory must never be allocated on two ends at once, or fragmentation
  could occur.

  If we have any in-use temp memory, additional temp allocations must come from
  that side.

  If not, we can choose to make either side the new temp side and push future
  permanent allocations to the other side.  Permanent allocations should be
  kept on the side that has the current greatest wasted highwater mark.

==============================================================================
*/


typedef unsigned int hmagic;
static const hmagic HUNK_MAGIC_INUSE = 0x89537892;
static const hmagic HUNK_MAGIC_FREED = 0x89537893;

typedef struct {
	hmagic	magic;
	int		size;
} hunkHeader_t;

typedef struct {
	int		mark;
	int		permanent;
	int		temp;
	int		tempHighwater;
} hunkUsed_t;

static	hunkUsed_t	hunk_low = {0}, hunk_high = {0};
static	hunkUsed_t	*hunk_permanent = &hunk_low, *hunk_temp = &hunk_high;

static	byte	*s_hunkData = NULL;
static	int		s_hunkTotal = 0;

static	int		s_zoneTotal = 0;

#ifdef HUNK_DEBUG

typedef struct hunkblock_s {
	int size;
	byte printed;
	struct hunkblock_s *next;
	char *label;
	char *file;
	int line;
} hunkblock_t;

static hunkblock_t* hunkblocks = NULL;

#endif


static void Com_Meminfo_f( void )
{
	const memblock_t* block;
	int zoneBytes = 0;
	int zoneBlocks = 0;
	int botlibBytes = 0;
	int rendererBytes = 0;

	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if ( Cmd_Argc() != 1 ) {
			Com_Printf ("block:%p    size:%7i    tag:%3i\n",
				block, block->size, block->tag);
		}
		if ( block->tag ) {
			zoneBytes += block->size;
			zoneBlocks++;
			if ( block->tag == TAG_BOTLIB ) {
				botlibBytes += block->size;
			} else if ( block->tag == TAG_RENDERER ) {
				rendererBytes += block->size;
			}
		}

		if (block->next == &mainzone->blocklist) {
			break;			// all blocks have been hit
		}
		if ( (byte *)block + block->size != (byte *)block->next) {
			Com_Printf ("ERROR: block size does not touch the next block\n");
		}
		if ( block->next->prev != block) {
			Com_Printf ("ERROR: next block doesn't have proper back link\n");
		}
		if ( !block->tag && !block->next->tag ) {
			Com_Printf ("ERROR: two consecutive free blocks\n");
		}
	}

	int smallZoneBytes = 0;
	int smallZoneBlocks = 0;
	for (block = smallzone->blocklist.next ; ; block = block->next) {
		if ( block->tag ) {
			smallZoneBytes += block->size;
			smallZoneBlocks++;
		}

		if (block->next == &smallzone->blocklist) {
			break;			// all blocks have been hit
		}
	}

	Com_Printf( "%8i bytes total hunk\n", s_hunkTotal );
	Com_Printf( "%8i bytes total zone\n", s_zoneTotal );
	Com_Printf( "\n" );
	Com_Printf( "%8i low mark\n", hunk_low.mark );
	Com_Printf( "%8i low permanent\n", hunk_low.permanent );
	if ( hunk_low.temp != hunk_low.permanent ) {
		Com_Printf( "%8i low temp\n", hunk_low.temp );
	}
	Com_Printf( "%8i low tempHighwater\n", hunk_low.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i high mark\n", hunk_high.mark );
	Com_Printf( "%8i high permanent\n", hunk_high.permanent );
	if ( hunk_high.temp != hunk_high.permanent ) {
		Com_Printf( "%8i high temp\n", hunk_high.temp );
	}
	Com_Printf( "%8i high tempHighwater\n", hunk_high.tempHighwater );
	Com_Printf( "\n" );
	Com_Printf( "%8i total hunk in use\n", hunk_low.permanent + hunk_high.permanent );

	int unused = 0;
	if ( hunk_low.tempHighwater > hunk_low.permanent ) {
		unused += hunk_low.tempHighwater - hunk_low.permanent;
	}
	if ( hunk_high.tempHighwater > hunk_high.permanent ) {
		unused += hunk_high.tempHighwater - hunk_high.permanent;
	}
	Com_Printf( "%8i unused highwater\n", unused );
	Com_Printf( "\n" );

	Com_Printf( "%8i bytes in %i zone blocks\n", zoneBytes, zoneBlocks );
	Com_Printf( "   %8i bytes in dynamic botlib\n", botlibBytes );
	Com_Printf( "   %8i bytes in dynamic renderer\n", rendererBytes );
	Com_Printf( "   %8i bytes in dynamic other\n", zoneBytes - ( botlibBytes + rendererBytes ) );
	Com_Printf( "   %8i bytes in small Zone memory\n", smallZoneBytes );
}


// touch all known used data to make sure it is paged in

void Com_TouchMemory()
{
	int		i, j;
	int sum = 0;
	const memblock_t* block;

	Z_CheckHeap();

	int start = Sys_Milliseconds();

	j = hunk_low.permanent >> 2;
	for ( i = 0 ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((int *)s_hunkData)[i];
	}

	i = ( s_hunkTotal - hunk_high.permanent ) >> 2;
	j = hunk_high.permanent >> 2;
	for (  ; i < j ; i+=64 ) {			// only need to touch each page
		sum += ((int *)s_hunkData)[i];
	}

	for (block = mainzone->blocklist.next ; ; block = block->next) {
		if ( block->tag ) {
			j = block->size >> 2;
			for ( i = 0 ; i < j ; i+=64 ) {				// only need to touch each page
				sum += ((int *)block)[i];
			}
		}
		if ( block->next == &mainzone->blocklist ) {
			break;			// all blocks have been hit
		}
	}

	int end = Sys_Milliseconds();

	Com_Printf( "Com_TouchMemory: %i msec\n", end - start );
}


static void Com_InitSmallZoneMemory()
{
	const int s_smallZoneTotal = 1024 * 1024 * CST_SMALLZONEMEGS;

	smallzone = (memzone_t*)calloc( s_smallZoneTotal, 1 );
	if ( !smallzone )
		Com_Error( ERR_FATAL, "Small zone data failed to allocate %d MB", CST_SMALLZONEMEGS );

	Z_ClearZone( smallzone, s_smallZoneTotal );
}


static void Com_InitZoneMemory()
{
	//FIXME: 05/01/06 com_zoneMegs is useless right now as neither q3config.cfg nor
	// Com_StartupVariable have been executed by this point. The net result is that
	// s_zoneTotal will always be set to the default value.
	// myT: removed com_zoneMegs for now

	// allocate the random block zone
	s_zoneTotal = 1024 * 1024 * CST_COMZONEMEGS;

	mainzone = (memzone_t*)calloc( s_zoneTotal, 1 );
	if ( !mainzone )
		Com_Error( ERR_FATAL, "Zone data failed to allocate %i megs", s_zoneTotal / (1024*1024) );

	Z_ClearZone( mainzone, s_zoneTotal );
}


#ifdef HUNK_DEBUG

static void Hunk_Log( void )
{
	char buf[4096];
	const hunkblock_t* block;
	int size = 0, numBlocks = 0;

	if (!logfile || !FS_Initialized())
		return;

	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);

	for (block = hunkblocks ; block; block = block->next) {
		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", block->size, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);
		size += block->size;
		numBlocks++;
	}

	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}

void Hunk_SmallLog( void )
{
	hunkblock_t	*block, *block2;
	char		buf[4096];
	int size, locsize, numBlocks;

	if (!logfile || !FS_Initialized())
		return;
	for (block = hunkblocks ; block; block = block->next) {
		block->printed = qfalse;
	}
	size = 0;
	numBlocks = 0;
	Com_sprintf(buf, sizeof(buf), "\r\n================\r\nHunk Small log\r\n================\r\n");
	FS_Write(buf, strlen(buf), logfile);
	for (block = hunkblocks; block; block = block->next) {
		if (block->printed) {
			continue;
		}
		locsize = block->size;
		for (block2 = block->next; block2; block2 = block2->next) {
			if (block->line != block2->line) {
				continue;
			}
			if (Q_stricmp(block->file, block2->file)) {
				continue;
			}
			size += block2->size;
			locsize += block2->size;
			block2->printed = qtrue;
		}

		Com_sprintf(buf, sizeof(buf), "size = %8d: %s, line: %d (%s)\r\n", locsize, block->file, block->line, block->label);
		FS_Write(buf, strlen(buf), logfile);

		size += block->size;
		numBlocks++;
	}

	Com_sprintf(buf, sizeof(buf), "%d Hunk memory\r\n", size);
	FS_Write(buf, strlen(buf), logfile);
	Com_sprintf(buf, sizeof(buf), "%d hunk blocks\r\n", numBlocks);
	FS_Write(buf, strlen(buf), logfile);
}

#endif


static const cmdTableItem_t hunk_cmds[] =
{
	{ "meminfo", Com_Meminfo_f, NULL, "prints memory allocation info" },
#ifdef ZONE_DEBUG
	{ "zonelog", Z_LogHeap },
#endif
#ifdef HUNK_DEBUG
	{ "hunklog", Hunk_Log },
	{ "hunksmalllog", Hunk_SmallLog }
#endif
};


static void Com_InitHunkMemory()
{
	// make sure the file system has allocated and "not" freed any temp blocks
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the FS utilizing different memory systems
	if (FS_LoadStack() != 0) {
		Com_Error( ERR_FATAL, "Hunk initialization failed. File system load stack not zero" );
	}

	// allocate the stack based hunk allocator
	const cvar_t* cv = Cvar_Get( "com_hunkMegs", XSTRING(DEF_COMHUNKMEGS), CVAR_LATCH | CVAR_ARCHIVE );
	if (com_dedicated && com_dedicated->integer)
		Cvar_SetRange( "com_hunkMegs", CVART_INTEGER, XSTRING(MIN_COMHUNKMEGS_DED), NULL );
	else
		Cvar_SetRange( "com_hunkMegs", CVART_INTEGER, XSTRING(MIN_COMHUNKMEGS), NULL );

	int nMinAlloc;
	const char* s;
	if (com_dedicated && com_dedicated->integer) {
		nMinAlloc = MIN_COMHUNKMEGS_DED;
		s = "Minimum com_hunkMegs for a dedicated server is %i, allocating %i megs.\n";
	}
	else {
		nMinAlloc = MIN_COMHUNKMEGS;
		s = "Minimum com_hunkMegs is %i, allocating %i megs.\n";
	}

	if ( cv->integer < nMinAlloc ) {
		s_hunkTotal = 1024 * 1024 * nMinAlloc;
		Com_Printf( s, nMinAlloc, s_hunkTotal / (1024 * 1024) );
	} else {
		s_hunkTotal = cv->integer * 1024 * 1024;
	}
#if defined( _MSC_VER ) && defined( _DEBUG ) && defined( idx64 )
	// try to allocate at the highest possible address range to help detect errors during development
	s_hunkData = (byte*)VirtualAlloc( NULL, ( s_hunkTotal + 4095 ) & ( ~4095 ), MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE );
	Cvar_Get( "sys_hunkBaseAddress", va( "%p", s_hunkData ), CVAR_ROM );
#else
	s_hunkData = (byte*)calloc( s_hunkTotal + 63, 1 );
#endif
	if ( !s_hunkData ) {
		Com_Error( ERR_FATAL, "Hunk data failed to allocate %i megs", s_hunkTotal / (1024*1024) );
	}
	// cacheline align
	s_hunkData = (byte *) ( ( (intptr_t)s_hunkData + 63 ) & ( ~63 ) );
	Hunk_Clear();

	Cmd_RegisterArray( hunk_cmds, MODULE_COMMON );
}


int Hunk_MemoryRemaining()
{
	int low = hunk_low.permanent > hunk_low.temp ? hunk_low.permanent : hunk_low.temp;
	int high = hunk_high.permanent > hunk_high.temp ? hunk_high.permanent : hunk_high.temp;

	return s_hunkTotal - ( low + high );
}


// the server calls this after the level and game VM have been loaded

void Hunk_SetMark()
{
	hunk_low.mark = hunk_low.permanent;
	hunk_high.mark = hunk_high.permanent;
}


// the client calls this before starting a vid_restart or snd_restart

void Hunk_ClearToMark()
{
	hunk_low.permanent = hunk_low.temp = hunk_low.mark;
	hunk_high.permanent = hunk_high.temp = hunk_high.mark;
}


// the bot code uses this for no good reason FAICT

qbool Hunk_CheckMark()
{
	return ( hunk_low.mark || hunk_high.mark );
}


// the server calls this before shutting down or loading a new map

void Hunk_Clear()
{
#ifndef DEDICATED
	CL_ShutdownCGame();
	CL_ShutdownUI();
#endif

	SV_ShutdownGameProgs();

#ifndef DEDICATED
	CIN_CloseAllVideos();
#endif

	hunk_low.mark = 0;
	hunk_low.permanent = 0;
	hunk_low.temp = 0;
	hunk_low.tempHighwater = 0;

	hunk_high.mark = 0;
	hunk_high.permanent = 0;
	hunk_high.temp = 0;
	hunk_high.tempHighwater = 0;

	hunk_permanent = &hunk_low;
	hunk_temp = &hunk_high;

	VM_Clear();

#ifdef HUNK_DEBUG
	hunkblocks = NULL;
#endif
}


static void Hunk_SwapBanks()
{
	// can't swap banks if there is any temp already allocated
	if ( hunk_temp->temp != hunk_temp->permanent ) {
		return;
	}

	// if we have a larger highwater mark on this side, start making
	// our permanent allocations here and use the other side for temp
	if ( hunk_temp->tempHighwater - hunk_temp->permanent >
			hunk_permanent->tempHighwater - hunk_permanent->permanent ) {
		hunkUsed_t* swap = hunk_temp;
		hunk_temp = hunk_permanent;
		hunk_permanent = swap;
	}
}


// allocate permanent (until the hunk is cleared) memory

#ifdef HUNK_DEBUG
void *Hunk_AllocDebug( int size, ha_pref preference, char *label, char *file, int line )
#else
void *Hunk_Alloc( int size, ha_pref preference )
#endif
{
	if (!s_hunkData) {
		Com_Error( ERR_FATAL, "Hunk_Alloc: Hunk memory system not initialized" );
	}

	// can't do preference if there is any temp allocated
	if (preference == h_dontcare || hunk_temp->temp != hunk_temp->permanent) {
		Hunk_SwapBanks();
	} else {
		if (preference == h_low && hunk_permanent != &hunk_low) {
			Hunk_SwapBanks();
		} else if (preference == h_high && hunk_permanent != &hunk_high) {
			Hunk_SwapBanks();
		}
	}

#ifdef HUNK_DEBUG
	size += sizeof(hunkblock_t);
#endif

	// round to cacheline
	size = ( size + 63 ) & ( ~63 );

	if ( hunk_low.temp + hunk_high.temp + size > s_hunkTotal ) {
#ifdef HUNK_DEBUG
		Hunk_Log();
		Hunk_SmallLog();
#endif
		Com_Error( ERR_DROP, "Hunk_Alloc failed on %i", size );
	}

	void* buf;
	if ( hunk_permanent == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_permanent->permanent);
		hunk_permanent->permanent += size;
	} else {
		hunk_permanent->permanent += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_permanent->permanent );
	}

	hunk_permanent->temp = hunk_permanent->permanent;

	Com_Memset( buf, 0, size );

#ifdef HUNK_DEBUG
	{
		hunkblock_t* block = (hunkblock_t*)buf;
		block->size = size - sizeof(hunkblock_t);
		block->file = file;
		block->label = label;
		block->line = line;
		block->next = hunkblocks;
		hunkblocks = block;
		buf = ((byte *) buf) + sizeof(hunkblock_t);
	}
#endif

	return buf;
}


/*
This is used by the file loading system.
Multiple files can be loaded in temporary memory.
When the files-in-use count reaches zero, all temp memory will be deleted
*/
void* Hunk_AllocateTempMemory( int size )
{
	// return a Z_Malloc'd block if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the FS utilizing different memory systems
	if (!s_hunkData) {
		return Z_Malloc(size);
	}

	Hunk_SwapBanks();

	size = PAD(size, sizeof(intptr_t)) + sizeof( hunkHeader_t );

	if ( hunk_temp->temp + hunk_permanent->permanent + size > s_hunkTotal ) {
		Com_Error( ERR_DROP, "Hunk_AllocateTempMemory: failed on %i", size );
	}

	void* buf;
	if ( hunk_temp == &hunk_low ) {
		buf = (void *)(s_hunkData + hunk_temp->temp);
		hunk_temp->temp += size;
	} else {
		hunk_temp->temp += size;
		buf = (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp );
	}

	if ( hunk_temp->temp > hunk_temp->tempHighwater ) {
		hunk_temp->tempHighwater = hunk_temp->temp;
	}

	hunkHeader_t* hdr = (hunkHeader_t*)buf;
	buf = (void*)(hdr+1);

	hdr->magic = HUNK_MAGIC_INUSE;
	hdr->size = size;

	// don't bother clearing, because we are going to load a file over it
	return buf;
}


void Hunk_FreeTempMemory( void* buf )
{
	// free with Z_Free if the hunk has not been initialized
	// this allows the config and product id files ( journal files too ) to be loaded
	// by the file system without redundant routines in the FS utilizing different memory systems
	if (!s_hunkData) {
		Z_Free(buf);
		return;
	}

	hunkHeader_t* hdr = ( (hunkHeader_t *)buf ) - 1;
	if ( hdr->magic != HUNK_MAGIC_INUSE ) {
		Com_Error( ERR_FATAL, "Hunk_FreeTempMemory: bad magic" );
	}

	hdr->magic = HUNK_MAGIC_FREED;

	// this only works if the files are freed in stack order,
	// otherwise the memory will stay around until Hunk_ClearTempMemory
	if ( hunk_temp == &hunk_low ) {
		if ( hdr == (void *)(s_hunkData + hunk_temp->temp - hdr->size ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	} else {
		if ( hdr == (void *)(s_hunkData + s_hunkTotal - hunk_temp->temp ) ) {
			hunk_temp->temp -= hdr->size;
		} else {
			Com_Printf( "Hunk_FreeTempMemory: not the final block\n" );
		}
	}
}


/*
The temp space is no longer needed.
If we have left more touched but unused memory on this side,
have future permanent allocs use this side.
*/
void Hunk_ClearTempMemory()
{
	if ( s_hunkData ) {
		hunk_temp->temp = hunk_temp->permanent;
	}
}


/*
===================================================================

EVENTS AND JOURNALING

In addition to these events, .cfg files are also copied to the
journaled file
===================================================================
*/

// FIXME TTimo blunt upping from 256 to 1024
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=5
#define MAX_PUSHED_EVENTS	1024
static sysEvent_t com_pushedEvents[MAX_PUSHED_EVENTS];
static int com_pushedEventsHead;
static int com_pushedEventsTail;


static void Com_InitJournaling()
{
	Com_StartupVariable( "journal" );
	com_journal = Cvar_Get ("journal", "0", CVAR_INIT);
	if ( !com_journal->integer ) {
		return;
	}

	if ( com_journal->integer == 1 ) {
		Com_Printf( "Journaling events\n");
		com_journalFile = FS_FOpenFileWrite( "journal.dat" );
		com_journalDataFile = FS_FOpenFileWrite( "journaldata.dat" );
	} else if ( com_journal->integer == 2 ) {
		Com_Printf( "Replaying journaled events\n");
		FS_FOpenFileRead( "journal.dat", &com_journalFile, qtrue );
		FS_FOpenFileRead( "journaldata.dat", &com_journalDataFile, qtrue );
	}

	if ( !com_journalFile || !com_journalDataFile ) {
		Cvar_Set( "com_journal", "0" );
		com_journalFile = 0;
		com_journalDataFile = 0;
		Com_Printf( "Couldn't open journal files\n" );
	}
}


static sysEvent_t Com_GetRealEvent()
{
	int			r;
	sysEvent_t	ev;

	// get an event from either the system or the journal file
	if ( com_journal->integer == 2 ) {
		r = FS_Read( &ev, sizeof(ev), com_journalFile );
		if ( r != sizeof(ev) ) {
			Com_Error( ERR_FATAL, "Error reading from journal file" );
		}
		if ( ev.evPtrLength ) {
			ev.evPtr = Z_Malloc( ev.evPtrLength );
			r = FS_Read( ev.evPtr, ev.evPtrLength, com_journalFile );
			if ( r != ev.evPtrLength ) {
				Com_Error( ERR_FATAL, "Error reading from journal file" );
			}
		}
	} else {
		ev = Sys_GetEvent();

		// write the journal value out if needed
		if ( com_journal->integer == 1 ) {
			r = FS_Write( &ev, sizeof(ev), com_journalFile );
			if ( r != sizeof(ev) ) {
				Com_Error( ERR_FATAL, "Error writing to journal file" );
			}
			if ( ev.evPtrLength ) {
				r = FS_Write( ev.evPtr, ev.evPtrLength, com_journalFile );
				if ( r != ev.evPtrLength ) {
					Com_Error( ERR_FATAL, "Error writing to journal file" );
				}
			}
		}
	}

	return ev;
}


static void Com_PushEvent( const sysEvent_t* event )
{
	static qbool printedWarning = qfalse;

	sysEvent_t* ev = &com_pushedEvents[ com_pushedEventsHead & (MAX_PUSHED_EVENTS-1) ];

	if ( com_pushedEventsHead - com_pushedEventsTail >= MAX_PUSHED_EVENTS ) {

		// don't print the warning constantly, or it can give time for more...
		if ( !printedWarning ) {
			printedWarning = qtrue;
			Com_Printf( "WARNING: Com_PushEvent overflow\n" );
		}

		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		com_pushedEventsTail++;
	} else {
		printedWarning = qfalse;
	}

	*ev = *event;
	com_pushedEventsHead++;
}


static sysEvent_t Com_GetEvent()
{
	if ( com_pushedEventsHead > com_pushedEventsTail ) {
		com_pushedEventsTail++;
		return com_pushedEvents[ (com_pushedEventsTail-1) & (MAX_PUSHED_EVENTS-1) ];
	}
	return Com_GetRealEvent();
}


static void Com_RunAndTimeServerPacket( const netadr_t& from, msg_t* msg )
{
	int t1 = (com_speeds->integer == 3) ? Sys_Milliseconds() : 0;

	SV_PacketEvent( from, msg );

	if (com_speeds->integer == 3) {
		int ms = Sys_Milliseconds() - t1;
		Com_Printf( "SV_PacketEvent time: %i\n", ms );
	}
}


// returns last event time

int Com_EventLoop()
{
	sysEvent_t	ev;
	netadr_t	evFrom;
	byte		bufData[MAX_MSGLEN];
	msg_t		buf;

	MSG_Init( &buf, bufData, sizeof( bufData ) );

	while ( 1 ) {
		NET_FlushPacketQueue();
		ev = Com_GetEvent();

		// if no more events are available
		if ( ev.evType == SE_NONE ) {
			// manually send packet events for the loopback channel
#ifndef DEDICATED
			while ( NET_GetLoopPacket( NS_CLIENT, &evFrom, &buf ) ) {
				CL_PacketEvent( evFrom, &buf );
			}
#endif
			while ( NET_GetLoopPacket( NS_SERVER, &evFrom, &buf ) ) {
				// if the server just shut down, flush the events
				if ( com_sv_running->integer ) {
					Com_RunAndTimeServerPacket( evFrom, &buf );
				}
			}

			return ev.evTime;
		}


		switch ( ev.evType ) {
		default:
			Com_Error( ERR_FATAL, "Com_EventLoop: bad event type %i", ev.evType );
			break;
		case SE_NONE:
			break;
#ifndef DEDICATED
		case SE_KEY:
			CL_KeyEvent( ev.evValue, (ev.evValue2 != 0), ev.evTime );
			break;
		case SE_CHAR:
			CL_CharEvent( ev.evValue );
			break;
		case SE_MOUSE:
			CL_MouseEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
		case SE_JOYSTICK_AXIS:
			CL_JoystickEvent( ev.evValue, ev.evValue2, ev.evTime );
			break;
#endif
		case SE_CONSOLE:
			Cbuf_AddText( (char *)ev.evPtr );
			Cbuf_AddText( "\n" );
			break;
		case SE_PACKET:
			evFrom = *(netadr_t *)ev.evPtr;
			buf.cursize = ev.evPtrLength - sizeof( evFrom );

			// we must copy the contents of the message out, because
			// the event buffers are only large enough to hold the
			// exact payload, but channel messages need to be large
			// enough to hold fragment reassembly
			if ( (unsigned)buf.cursize > buf.maxsize ) {
				Com_Printf("Com_EventLoop: oversize packet\n");
				continue;
			}
			Com_Memcpy( buf.data, (byte *)((netadr_t *)ev.evPtr + 1), buf.cursize );
			if ( com_sv_running->integer ) {
				Com_RunAndTimeServerPacket( evFrom, &buf );
			} else {
#ifndef DEDICATED
				CL_PacketEvent( evFrom, &buf );
#endif
			}
			break;
		}

		// free any block data
		if ( ev.evPtr ) {
			Z_Free( ev.evPtr );
		}
	}

	return 0;	// never reached
}


// can be used for profiling, but will be journaled accurately

int Com_Milliseconds()
{
	sysEvent_t ev;

	// get events and push them until we get a null event with the current time
	do {
		ev = Com_GetRealEvent();
		if ( ev.evType != SE_NONE ) {
			Com_PushEvent( &ev );
		}
	} while ( ev.evType != SE_NONE );

	return ev.evTime;
}


///////////////////////////////////////////////////////////////


#if defined(DEBUG) || defined(CNQ3_DEV)


// throw a fatal error to test error shutdown procedures

static void Com_Error_f( void )
{
	if ( Cmd_Argc() > 1 ) {
		Com_Error( ERR_DROP, "Testing drop error" );
	} else {
		Com_Error( ERR_FATAL, "Testing fatal error" );
	}
}


// freeze in place for a given number of seconds to test error recovery

static void Com_Freeze_f( void )
{
	if ( Cmd_Argc() != 2 ) {
		Com_Printf( "freeze <seconds>\n" );
		return;
	}

	float s = atof( Cmd_Argv(1) );

	int start = Com_Milliseconds();
	while ( ( Com_Milliseconds() - start ) / 1000.0f < s )
		;
}


static void Com_Exit_f( void )
{
	exit( 666 );
}


static void Com_Rand_f( void )
{
	Com_Printf("%d\n", rand());
}


// force a bus error for development reasons

static void Com_Crash_f( void )
{
	*(int*)0 = 0x12345678;
}


#endif


// TTimo: centralizing the cl_cdkey stuff after I discovered a buffer overflow problem with the dedicated server version
//   not sure it's necessary to have different defaults for regular and dedicated, but I don't want to risk it
//   https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=470
#ifndef DEDICATED
char	cl_cdkey[34] = "                                ";
#else
char	cl_cdkey[34] = "123456789";
#endif

#ifndef DEDICATED
/*
=================
Com_ReadCDKey
=================
*/
qbool CL_CDKeyValidate( const char *key, const char *checksum );
void Com_ReadCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	sprintf(fbuffer, "%s/q3key", filename);

	FS_SV_FOpenFileRead( fbuffer, &f );
	if ( !f ) {
		Q_strncpyz( cl_cdkey, "                ", 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof(buffer) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if (CL_CDKeyValidate(buffer, NULL)) {
		Q_strncpyz( cl_cdkey, buffer, 17 );
	} else {
		Q_strncpyz( cl_cdkey, "                ", 17 );
	}
}


/*
=================
Com_AppendCDKey
=================
*/
void Com_AppendCDKey( const char *filename ) {
	fileHandle_t	f;
	char			buffer[33];
	char			fbuffer[MAX_OSPATH];

	sprintf(fbuffer, "%s/q3key", filename);

	FS_SV_FOpenFileRead( fbuffer, &f );
	if (!f) {
		Q_strncpyz( &cl_cdkey[16], "                ", 17 );
		return;
	}

	Com_Memset( buffer, 0, sizeof(buffer) );

	FS_Read( buffer, 16, f );
	FS_FCloseFile( f );

	if (CL_CDKeyValidate(buffer, NULL)) {
		strcat( &cl_cdkey[16], buffer );
	} else {
		Q_strncpyz( &cl_cdkey[16], "                ", 17 );
	}
}

#else

void Com_AppendCDKey( const char *filename ) {
}

void Com_ReadCDKey( const char *filename ) {
}

#endif


// 0=eax 1=ebx 2=ecx 3=edx
static qbool Com_CPUID( int function, int registers[4] ) {
#if MSVC_CPUID
	__cpuid( registers, function );
	return qtrue;
#elif GCC_CPUID
	if( __get_cpuid( (unsigned int)function, (unsigned int*)&registers[0], (unsigned int*)&registers[1],
		(unsigned int*)&registers[2], (unsigned int*)&registers[3] ) != 1 )
		return qfalse;
	return qtrue;
#else
	return qfalse;
#endif
}

static const char* Com_ProcessorName() {
	static int regs[4];

	if( !Com_CPUID( 0, regs) ) {
		return NULL;
	}

	regs[0] = regs[1];
	regs[1] = regs[3];
	regs[3] = 0;

	return (const char*)regs;
}

typedef struct {
	const char* s;
	int reg;
	int bit;
	int flag;
	qbool required;
} cpuFeatureBit_t;


static const cpuFeatureBit_t cpu_featureBits[] = {
#if id386
	{ " MMX",    3, 23, 0,         qtrue },
	{ " SSE",    3, 25, 0,         qtrue },
	{ " SSE2",   3, 26, 0,         qtrue },
#endif
	{ " SSE3",   2,  0, CPU_SSE3,  qfalse },
	{ " SSSE3",  2,  9, CPU_SSSE3, qfalse },
	{ " SSE4.1", 2, 19, CPU_SSE41, qfalse },
	{ " SSE4.2", 2, 20, CPU_SSE42, qfalse }
// we want to avoid AVX for anything that isn't really super costly
// because otherwise the power management changes will be counter-productive
//	{ " AVX", 2, 28, CPU_AVX, qfalse }
// for AVX2 and later, you'd need to call cpuid with eax=7 and ecx=0 ("extended features")
};

int cpu_features = 0;

static qbool Com_GetProcessorInfo()
{
	Cvar_Get( "sys_cpustring", "unknown", 0 );

	int regs[4];
	const char* name = Com_ProcessorName();
	if ( name == NULL || !Com_CPUID( 1, regs ) ) {
		return qfalse;
	}

	char s[256] = "";
	Q_strcat( s, sizeof(s), name );

	int features = 0;
	for (int i = 0; i < ARRAY_LEN(cpu_featureBits); i++) {
		const cpuFeatureBit_t* f = cpu_featureBits + i;
		const qbool active = ( regs[f->reg] & ( 1 << f->bit ) ) != 0;

		if ( f->required && !active ) {
			Com_Error( ERR_FATAL, "CNQ3 requires a processor with SSE2 support\n" );
		}

		if ( active ) {
			Q_strcat( s, sizeof(s), f->s );
			features |= f->flag;
		}
	}

	cpu_features = features;

	Cvar_Set( "sys_cpustring", s );

	return qtrue;
}


static const cmdTableItem_t com_cmds[] =
{
#if defined(DEBUG) || defined(CNQ3_DEV)
	{ "error", Com_Error_f },
	{ "crash", Com_Crash_f },
	{ "freeze", Com_Freeze_f },
	{ "exit", Com_Exit_f },
	{ "rand", Com_Rand_f },
#endif
	{ "quit", Com_Quit_f, NULL, "closes the application" },
	{ "writeconfig", Com_WriteConfig_f, Com_CompleteWriteConfig_f, help_writeconfig }
};


static const cvarTableItem_t com_cvars[] =
{
#if defined( QC )
	{
		&com_maxfps, "com_maxfps", "250", CVAR_ARCHIVE, CVART_INTEGER, "60", "500", help_com_maxfps,
		"Framerate cap", CVARCAT_DISPLAY, "If you see 'connection interrupted' online, lower the cap", ""
	},
#else // QC
	{
		&com_maxfps, "com_maxfps", "125", CVAR_ARCHIVE, CVART_INTEGER, "60", "250", help_com_maxfps,
		"Framerate cap", CVARCAT_DISPLAY, "If you see 'connection interrupted' online, lower the cap", ""
	},
#endif // QC

	{ &com_developer, "developer", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "enables detailed logging" },
	{ &com_logfile, "logfile", "0", CVAR_TEMP, CVART_INTEGER, "0", "2", help_com_logfile },
	{ &com_timescale, "timescale", "1", CVAR_CHEAT | CVAR_SYSTEMINFO, CVART_FLOAT, "0", "100", "game time to real time ratio" },
	{ &com_fixedtime, "fixedtime", "0", CVAR_CHEAT, CVART_INTEGER, "0", "64", "fixed number of ms per simulation tick, " S_COLOR_VAL "0" S_COLOR_HELP "=off" },
	{ &com_showtrace, "com_showtrace", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "prints trace optimization info" },
	{ &com_viewlog, "viewlog", "0", CVAR_CHEAT, CVART_INTEGER, "0", "2", help_com_viewlog },
	{ &com_speeds, "com_speeds", "0", 0, CVART_BOOL, NULL, NULL, "prints timing info" },
	{ &com_timedemo, "timedemo", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "benchmarking mode for demo playback" },
	{ &cl_paused, "cl_paused", "0", CVAR_ROM, CVART_BOOL },
	{ &sv_paused, "sv_paused", "0", CVAR_ROM, CVART_BOOL },
	{ &cl_packetdelay, "cl_packetdelay", "0", CVAR_CHEAT, CVART_INTEGER, "0", NULL },
	{ &sv_packetdelay, "sv_packetdelay", "0", CVAR_CHEAT, CVART_INTEGER, "0", NULL },
	{ &com_sv_running, "sv_running", "0", CVAR_ROM, CVART_BOOL },
	{ &com_cl_running, "cl_running", "0", CVAR_ROM, CVART_BOOL },
#if defined(_WIN32) && defined(_DEBUG)
	{ &com_noErrorInterrupt, "com_noErrorInterrupt", "0", 0, CVART_BOOL },
#endif
	{
		&con_completionStyle, "con_completionStyle", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_con_completionStyle,
		"Console completion style", CVARCAT_CONSOLE, "", "",
		CVAR_GUI_VALUE("0", "Quake 3", "Always prints all results")
		CVAR_GUI_VALUE("1", "Enemy Territory", "Prints once then cycles")
	},
	{
		&con_history, "con_history", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "writes the command history to a file on exit",
		"Save console history", CVARCAT_CONSOLE, "save the command history to a file on exit", ""
	}
};


#if defined(_MSC_VER)
#pragma warning (disable: 4611) // setjmp + destructors = bad. which it is, but...
#endif

void Com_Init( char *commandLine )
{
	Com_Printf( "%s %s %s\n", Q3_VERSION, PLATFORM_STRING, __DATE__ );

	if ( setjmp(abortframe) ) {
		Sys_Error ("Error during initialization");
	}

	srand( time( NULL ) );

	memset( com_pushedEvents, 0, sizeof(com_pushedEvents) );
	com_pushedEventsHead = 0;
	com_pushedEventsTail = 0;

	Com_InitSmallZoneMemory();
	Cvar_Init();

	// prepare enough of the subsystems to handle
	// cvar and command buffer management
	Com_ParseCommandLine( commandLine );

	Cbuf_Init();

	Com_InitZoneMemory();
	Cmd_Init();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	// get the developer cvar set as early as possible
	Com_StartupVariable( "developer" );

	// done early so bind command exists
#ifndef DEDICATED
	CL_InitKeyCommands();
#endif

	FS_InitFilesystem();

	Com_InitJournaling();

	Cbuf_AddText("exec default.cfg\n");

	// skip the q3config.cfg if "safe" is on the command line
	if ( !Com_SafeMode() )
		Cbuf_AddText("exec q3config.cfg\n");

	Cbuf_AddText("exec autoexec.cfg\n");

	Cbuf_Execute();

	// override anything from the config files with command line args
	Com_StartupVariable( NULL );

	// get dedicated here for proper hunk megs initialization
#ifdef DEDICATED
	com_dedicated = Cvar_Get("dedicated", "1", CVAR_ROM | CVAR_INIT);
#else
	com_dedicated = Cvar_Get("dedicated", "0", CVAR_LATCH);
#endif

	// allocate the stack based hunk allocator
	Com_InitHunkMemory();

	//
	// init commands and vars
	//
	Cvar_RegisterArray( com_cvars, MODULE_COMMON );

	if ( com_dedicated->integer ) {
		if ( !com_viewlog->integer ) {
			Cvar_Set( "viewlog", "1" );
		}
	}

	Cmd_RegisterArray( com_cmds, MODULE_COMMON );

#if defined( QC )
	char  s[250];
	strcpy( s, Q3_VERSION );	
#if defined( FS_DEVELOPER )
	strcat( s, " #FS_DEVELOPER" );
#endif
#if defined( PE_LOADER )
	strcat( s, " #PE_LOADER" );
#endif
	strcat( s, " " PLATFORM_STRING " " );
	strcat( s, __DATE__ );
#else
	const char* const s = Q3_VERSION " " GIT_COMMIT_SHORT " " PLATFORM_STRING " " __DATE__;
#endif
	com_version = Cvar_Get( "version", s, CVAR_ROM | CVAR_SERVERINFO );

	Cvar_Get( "sys_compiler", Com_GetCompilerInfo(), CVAR_ROM );

	// this is meaningless with cl.exe
	Cvar_Get( "sys_cplusplus", STRINGIZE(__cplusplus), CVAR_ROM );

	Cvar_Get( "sys_cpustring", "detect", 0 );
	if ( Com_GetProcessorInfo() ) {
		Com_Printf( "CPU: %s\n", Cvar_VariableString( "sys_cpustring" ) );
	}

	Sys_Init();
	Netchan_Init( Com_Milliseconds() & 0xffff );	// pick a port value that should be nice and random
	VM_Init();
	SV_Init();

	com_dedicated->modified = qfalse;
	if ( !com_dedicated->integer ) {
#ifndef DEDICATED
		CL_Init();
#endif
	}

	Sys_LoadHistory();

	// set com_frameTime so that if a map is started on the
	// command line it will still be able to count on com_frameTime
	// being random enough for a serverid
	com_frameTime = Com_Milliseconds();

	// add + commands from command line
	Com_AddStartupCommands();

	// start in full screen ui mode
	Cvar_Set( "r_uiFullScreen", "1" );

#ifndef DEDICATED
	CL_StartHunkUsers();
#endif

	// moved to fix the console window staying visible when starting the game in full-screen mode
	if ( !com_dedicated->integer )
		Sys_ShowConsole( com_viewlog->integer, qfalse );

	// make sure single player is off by default
	Cvar_Set( "sv_singlePlayer", "0" );

	Cvar_PrintDeprecationWarnings();

	com_fullyInitialized = qtrue;

	Com_Printf ("--- Common Initialization Complete ---\n");
}


//==================================================================


static void Com_WriteConfigToFile( const char* filename, qbool forceWrite )
{
	fileHandle_t f = FS_FOpenFileWrite( filename );
	if ( !f ) {
		Com_Printf ("Couldn't write %s.\n", filename );
		return;
	}

	FS_Printf(f, "// generated by quake, do not modify\n");
#ifndef DEDICATED
	Key_WriteBindings(f);
#endif
	Cvar_WriteVariables( f, forceWrite );
	FS_FCloseFile( f );
}


// write the config file to a specific name

static void Com_WriteConfig_f()
{
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: writeconfig <filename> [-f]\n" );
		return;
	}

	const qbool writeAllVars = Cmd_Argc() >= 3 && !Q_stricmp( Cmd_Argv(2), "-f" );
	char filename[MAX_QPATH];
	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );
	Com_Printf( "Writing %s.\n", filename );
	Com_WriteConfigToFile( filename, writeAllVars );
}


static void Com_CompleteWriteConfig_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteConfigName );
}


static int Com_ModifyMsec( int msec )
{
	// modify time for debugging values
	//
	if ( com_fixedtime->integer ) {
		msec = com_fixedtime->integer;
	} else if ( com_timescale->value ) {
		msec *= com_timescale->value;
	}

	// don't let it scale below 1 msec
	if ( msec < 1 && com_timescale->value) {
		msec = 1;
	}

	int clampTime;
	if ( com_dedicated->integer ) {
		// dedicated servers don't want to clamp for too long a period
		// because it would mess up all the client's views of time
		if ( msec > 500 ) {
			Com_Printf( "Hitch warning: %i msec frame time\n", msec );
		}
		clampTime = 5000;
	}
	else if ( !com_sv_running->integer ) {
		// clients of remote servers do not want to clamp time, because
		// it would skew their view of the server's time temporarily
		clampTime = 5000;
	}
	else {
		// for local single player gaming
		// we may want to clamp the time to prevent players from
		// flying off edges when something hitches.
		clampTime = 200;
	}

	if ( msec > clampTime ) {
		msec = clampTime;
	}

	return msec;
}


static void Com_FrameSleep( qbool demoPlayback )
{
	// "timedemo" playback means we run at full speed
	if ( demoPlayback && com_timedemo->integer )
		return;

	// decide how much sleep we need
	qbool preciseCap = qfalse;
	int64_t sleepUS = 0;
	if ( com_dedicated->integer ) {
		sleepUS = 1000 * SV_FrameSleepMS();
	} else {
		preciseCap = qtrue;
		sleepUS = 1000000 / com_maxfps->integer;
#ifndef DEDICATED
		if ( Sys_IsMinimized() ) {
			sleepUS = 20 * 1000;
			preciseCap = qfalse;
		} else if ( !CL_ShouldSleep() ) {
			return;
		}
#endif
	}

	// decide when we should stop sleeping
	static int64_t targetTimeUS = INT64_MIN;
	if ( Sys_Microseconds() > targetTimeUS + 3 * sleepUS )
		targetTimeUS = Sys_Microseconds() + sleepUS;
	else
		targetTimeUS += sleepUS;
	com_nextTargetTimeUS = targetTimeUS + 1000000 / com_maxfps->integer;

	// sleep if needed
	if ( com_dedicated->integer ) {
		while ( targetTimeUS - Sys_Microseconds() > 1000 ) {
			NET_Sleep( 1 );
			Com_EventLoop();
		}
	} else {
		int runEventLoop = 0;
		if ( preciseCap ) {
			for ( ;; ) {
				runEventLoop ^= 1;
				const int64_t remainingUS = targetTimeUS - Sys_Microseconds();
				if ( remainingUS > 3000 && runEventLoop )
					Com_EventLoop();
				else if ( remainingUS > 0 )
					Sys_MicroSleep( remainingUS );
				else
					break;
			}
		} else {
			while ( targetTimeUS - Sys_Microseconds() > 1000 ) {
				Sys_Sleep( 1 );
			}
		}
	}
}


void Com_Frame( qbool demoPlayback )
{
	if ( setjmp(abortframe) ) {
#ifndef DEDICATED
		CL_AbortFrame();
#endif
		return;			// an ERR_DROP was thrown
	}

	// bk001204 - init to zero.
	//  also:  might be clobbered by `longjmp' or `vfork'
	int timeBeforeFirstEvents =0;
	int timeBeforeServer =0;
	int timeBeforeEvents =0;
	int timeBeforeClient = 0;
	int timeAfter = 0;

	// if "viewlog" has been modified, show or hide the log console
	if ( com_viewlog->modified ) {
		if ( !com_dedicated->value ) {
			Sys_ShowConsole( com_viewlog->integer, qfalse );
		}
		com_viewlog->modified = qfalse;
	}

	//
	// main event loop
	//
	if ( com_speeds->integer ) {
		timeBeforeFirstEvents = Sys_Milliseconds();
	}

	Com_FrameSleep( demoPlayback );

	static int lastTime = 0;
	lastTime = com_frameTime;
	com_frameTime = Com_EventLoop();
	int msec = com_frameTime - lastTime;

	Cbuf_Execute();

	// mess with msec if needed
	msec = Com_ModifyMsec( msec );

	//
	// server side
	//
	if ( com_speeds->integer ) {
		timeBeforeServer = Sys_Milliseconds();
	}

	SV_Frame( msec );

	// if "dedicated" has been modified, start up
	// or shut down the client system.
	// Do this after the server may have started,
	// but before the client tries to auto-connect
	if ( com_dedicated->modified ) {
		// get the latched value
		Cvar_Get( "dedicated", "0", 0 );
		com_dedicated->modified = qfalse;
		if ( !com_dedicated->integer ) {
#ifndef DEDICATED
			CL_Init();
#endif
			Sys_ShowConsole( com_viewlog->integer, qfalse );
		} else {
#ifndef DEDICATED
			CL_Shutdown();
#endif
			Sys_ShowConsole( 1, qtrue );
		}
	}

#ifdef DEDICATED
	if ( com_speeds->integer ) {
		timeAfter = Sys_Milliseconds();
		timeBeforeEvents = timeAfter;
		timeBeforeClient = timeAfter;
	}
#else
	//
	// client system
	//
	if ( !com_dedicated->integer ) {
		R_WaitBeforeInputSampling();

		//
		// run event loop a second time to get server to client packets
		// without a frame of latency
		//
		if ( com_speeds->integer ) {
			timeBeforeEvents = Sys_Milliseconds();
		}
		Com_EventLoop();
		Cbuf_Execute();

		//
		// client side
		//
		if ( com_speeds->integer ) {
			timeBeforeClient = Sys_Milliseconds();
		}

		CL_Frame( msec );

		if ( com_speeds->integer ) {
			timeAfter = Sys_Milliseconds();
		}
	}
#endif

	//
	// report timing information
	//
	if ( com_speeds->integer ) {
		int all = timeAfter - timeBeforeServer;
		int sv = timeBeforeEvents - timeBeforeServer - time_game;
		int ev = timeBeforeServer - timeBeforeFirstEvents + timeBeforeClient - timeBeforeEvents;
		Com_Printf( "frame:%i all:%3i sv:%3i ev:%3i gm:%3i\n", com_frameNumber, all, sv, ev, time_game );
	}

	//
	// trace optimization tracking
	//
	if ( com_showtrace->integer ) {
		extern int c_traces, c_brush_traces, c_patch_traces, c_pointcontents;
		Com_Printf( "%4i traces  (%ib %ip) %4i points\n",
				c_traces, c_brush_traces, c_patch_traces, c_pointcontents );
		c_traces = 0;
		c_brush_traces = 0;
		c_patch_traces = 0;
		c_pointcontents = 0;
	}

	com_frameNumber++;

	// this is here because the platform layer can do more initialization
	// work after the Com_Init call and create more archived CVars,
	// meaning the config would always be written to on exit
	if ( com_frameNumber == 1 )
		cvar_modifiedFlags &= ~CVAR_ARCHIVE;
}


void Com_Shutdown()
{
	if (logfile) {
		FS_FCloseFile( logfile );
		logfile = 0;
	}

	if ( com_journalFile ) {
		FS_FCloseFile( com_journalFile );
		com_journalFile = 0;
	}
}


///////////////////////////////////////////////////////////////


/*
=====================
KHB !!!  see if this is still true - even vc might actually have a bug fixed in 8 years  :P

the msvc acos doesn't always return a value between -PI and PI:

int i;
i = 1065353246;
acos(*(float*) &i) == -1.#IND0

	This should go in q_math but it is too late to add new traps
	to game and ui
=====================
*/
float Q_acos(float c)
{
	float angle = acos(c);

	if (angle > M_PI) {
		return (float)M_PI;
	}
	if (angle < -M_PI) {
		return (float)M_PI;
	}
	return angle;
}


static unsigned int CRC32_table[256];
static qbool CRC32_tableCreated = qfalse;


void CRC32_Begin( unsigned int* crc )
{
	if ( !CRC32_tableCreated )
	{
		for ( int i = 0; i < 256; i++ )
		{
			unsigned int c = i;
			for ( int j = 0; j < 8; j++ )
				c = c & 1 ? (c >> 1) ^ 0xEDB88320UL : c >> 1;
			CRC32_table[i] = c;
		}
		CRC32_tableCreated = qtrue;
	}

	*crc = 0xFFFFFFFFUL;
}


void CRC32_ProcessBlock( unsigned int* crc, const void* buffer, unsigned int length )
{
	unsigned int hash = *crc;
	const unsigned char* buf = (const unsigned char*)buffer;
	while ( length-- )
	{
		hash = CRC32_table[(hash ^ *buf++) & 0xFF] ^ (hash >> 8);
	}
	*crc = hash;
}


void CRC32_End( unsigned int* crc )
{
	*crc ^= 0xFFFFFFFFUL;
}


/*
===========================================
command line completion
===========================================
*/


void Field_Clear( field_t* edit )
{
	memset(edit->buffer, 0, MAX_EDIT_LINE);
	edit->cursor = 0;
	edit->scroll = 0;
}

static const char*	completionString;
static char			shortestMatch[MAX_TOKEN_CHARS];
static char			fullMatch[MAX_TOKEN_CHARS];
static int			matchCount;
static int			matchIndex;
static qbool		findIndexOnly;		// for ET-style completion of command arguments
static field_t*		completionField;	// field we are working on, passed to Field_AutoComplete


static void FindMatches( const char *s )
{
	int		i;

	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}
	matchCount++;
	if ( matchCount == 1 ) {
		Q_strncpyz( shortestMatch, s, sizeof( shortestMatch ) );
		return;
	}

	// cut shortestMatch to the amount common with s
	for ( i = 0 ; shortestMatch[i] ; i++ ) {
		if ( i >= strlen( s ) ) {
			shortestMatch[i] = 0;
			break;
		}

		if ( tolower(shortestMatch[i]) != tolower(s[i]) ) {
			shortestMatch[i] = 0;
		}
	}
}


static int findMatchIndex;
static void FindIndexMatch( const char *s )
{
	if ( Q_stricmpn( s, completionString, strlen( completionString ) ) ) {
		return;
	}

	if ( findMatchIndex == matchIndex ) {
		Q_strncpyz( fullMatch, s, sizeof( fullMatch ) );
	}

	findMatchIndex++;
}


static void PrintMatches( const char *s )
{
	if ( !Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) ) {
		Com_Printf( "    %s\n", s );
	}
}


static void PrintCmdMatches( const char *s )
{
	if ( Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) )
		return;

	char msg[CONSOLE_WIDTH * 2]; // account for lots of color codes
	const char* desc;
	const char* help;
	Cmd_GetHelp( &desc, &help, s );
	const char h = help != NULL ? 'h' : ' ';

	if ( desc )
		Com_sprintf( msg, sizeof(msg), " %c  " S_COLOR_CMD "%s ^7- " S_COLOR_HELP "%s\n", h, s, desc );
	else
		Com_sprintf( msg, sizeof(msg), " %c  " S_COLOR_CMD "%s\n", h, s );

	Com_TruncatePrintString( msg, sizeof(msg), CONSOLE_WIDTH );
	Com_Printf( msg );
}


static void PrintCvarMatches( const char *s )
{
	if ( Q_stricmpn( s, shortestMatch, strlen( shortestMatch ) ) )
		return;

	char msg[CONSOLE_WIDTH * 2]; // account for lots of color codes
	const char* desc;
	const char* help;
	Cvar_GetHelp( &desc, &help, s );
	const char h = help != NULL ? 'h' : ' ';
	const char u = ( Cvar_Flags(s) & CVAR_USER_CREATED ) != 0 ? '?' : h;
	const char* const q = Cvar_Type(s) == CVART_STRING ? "\"" : "";

	if ( desc )
		Com_sprintf( msg, sizeof(msg), " %c  " S_COLOR_CVAR "%s^7 = %s" S_COLOR_VAL "%s^7%s - " S_COLOR_HELP "%s\n", u, s, q, Cvar_VariableString( s ), q, desc );
	else
		Com_sprintf( msg, sizeof(msg), " %c  " S_COLOR_CVAR "%s^7 = %s" S_COLOR_VAL "%s^7%s\n", u, s, q, Cvar_VariableString( s ), q );

	Com_TruncatePrintString( msg, sizeof(msg), CONSOLE_WIDTH );
	Com_Printf( msg );
}


static void Field_AppendArgString( field_t *field, int arg, const char *s )
{
	const qbool quoted = Cmd_ArgQuoted( arg );
	if ( quoted )
		Q_strcat( field->buffer, sizeof( field->buffer ), "\"" );
	Q_strcat( field->buffer, sizeof( field->buffer ), s );
	if ( quoted )
		Q_strcat( field->buffer, sizeof( field->buffer ), "\"" );
}


static void Field_AppendArg( field_t *field, int arg )
{
	Field_AppendArgString( field, arg, Cmd_Argv( arg ) );
}


static void Field_AppendLastArgs( field_t *field, int startArg )
{
	const int argc = Cmd_Argc();
	for ( int i = startArg; i < argc; ++i ) {
		Q_strcat( field->buffer, sizeof( field->buffer ), " " );
		Field_AppendArg( field, i );
	}
}


static void Field_AppendFirstArgs( field_t *field, int count )
{
	if ( count <= 0 )
		return;

	Field_AppendArg( field, 0 );
	for ( int i = 1; i < count; ++i ) {
		Q_strcat( field->buffer, sizeof( field->buffer ), " " );
		Field_AppendArg( field, i );
	}
}


static qbool String_HasLeadingSlash( const char *arg )
{
	return ((*arg == '\\') || (*arg == '/'));
}


// returns qtrue if the match list should be printed
static qboolean Field_CompleteShortestMatch( int startArg, int compArg )
{
	if ( matchCount == 0 )
		return qfalse;

	field_t *const field = completionField;

	// write the first part of the field
	*field->buffer = '\0';
	if ( compArg > 0 ) {
		Field_AppendFirstArgs( field, compArg );
		Q_strcat( field->buffer, sizeof( field->buffer ), " " );
	}
	Field_AppendArgString( field, compArg, shortestMatch );

	if ( matchCount == 1 ) {
		// finish the field with the only match
		if ( compArg == Cmd_Argc() - 1 )
			Q_strcat( field->buffer, sizeof( field->buffer ), " " );
		field->cursor = strlen( field->buffer );
		Field_AppendLastArgs( field, compArg + 1 );
		return qfalse;
	}

	// finish the field with the shortest match and echo the command
	field->cursor = strlen( field->buffer );
	if ( Cmd_ArgQuoted( compArg ) )
		field->cursor--;
	Field_AppendLastArgs( field, compArg + 1 );
	Com_Printf( "]%s\n", Cmd_Cmd() );

	return qtrue;
}


static void Field_AutoCompleteCmdOrVarName( int startArg, int compArg, qbool searchCmds, qbool searchVars )
{
	if ( !searchCmds && !searchVars )
		return;

	field_t* const field = completionField;

	if ( field->acOffset > 0 ) {
		if ( matchCount > 1 ) {
			// find the next match
			completionString = shortestMatch;
			findMatchIndex = 0;
			if ( searchCmds )
				Cmd_CommandCompletion( FindIndexMatch );
			if ( searchVars )
				Cvar_CommandCompletion( FindIndexMatch );
			matchIndex = ( matchIndex + 1 ) % matchCount;

			// insert it in the edit field
			if ( compArg == 0 ) {
				Q_strncpyz( field->buffer, fullMatch, sizeof(field->buffer) );
				field->cursor = strlen( field->buffer );
				Field_AppendLastArgs( field, 1 );
			} else {
				field->buffer[0] = '\0';
				Field_AppendFirstArgs( field, compArg );
				Q_strcat( field->buffer, sizeof(field->buffer), " " );
				Q_strcat( field->buffer, sizeof(field->buffer), fullMatch );
				field->cursor = strlen( field->buffer );
				Field_AppendLastArgs( field, compArg + 1 );
			}
			const int delta = String_HasLeadingSlash( field->buffer ) ? 0 : 1;
			field->acLength = field->cursor + delta - field->acOffset;
		}
		return;
	}

	*shortestMatch = '\0';
	matchCount = 0;
	matchIndex = 0;

	if ( searchCmds )
		Cmd_CommandCompletion( FindMatches );
	if ( searchVars )
		Cvar_CommandCompletion( FindMatches );

	if ( !Field_CompleteShortestMatch( startArg, compArg ) )
		return;

	// we found 2+ matches
	if ( con_completionStyle->integer ) {
		const int delta = String_HasLeadingSlash( field->buffer ) ? 0 : 1;
		field->acStartArg = startArg;
		field->acCompArg = compArg;
		field->acOffset = field->cursor + delta;
		field->acLength = 0;
	}

	if ( searchCmds )
		Cmd_CommandCompletion( PrintCmdMatches );
	if ( searchVars )
		Cvar_CommandCompletion( PrintCvarMatches );
}


static void Field_AutoCompleteCommandArgument( int startArg, int compArg )
{
	field_t* const field = completionField;
	if ( field->acOffset == 0 ) {
		*shortestMatch = '\0';
		matchCount = 0;
		matchIndex = 0;
	}

	const char* cmdName = Cmd_Argv( startArg );
	if ( String_HasLeadingSlash( cmdName ) )
		cmdName++;
	if ( *cmdName == '\0' )
		return;

	if ( field->acStartArg == startArg && field->acOffset > 0 ) {
		if ( matchCount > 1 ) {
			// find the next match
			completionString = shortestMatch;
			findMatchIndex = 0;
			findIndexOnly = qtrue;
			Cmd_AutoCompleteArgument( cmdName, startArg, compArg );
			findIndexOnly = qfalse;
			matchIndex = ( matchIndex + 1 ) % matchCount;

			// insert it in the edit field
			field->buffer[0] = '\0';
			Field_AppendFirstArgs( field, compArg );
			Q_strcat( field->buffer, sizeof(field->buffer), " " );
			Q_strcat( field->buffer, sizeof(field->buffer), fullMatch );
			field->cursor = strlen( field->buffer );
			Field_AppendLastArgs( field, compArg + 1 );
			const int delta = String_HasLeadingSlash( field->buffer ) ? 0 : 1;
			field->acLength = field->cursor + delta - field->acOffset;
		}
		return;
	}
	
	Cmd_AutoCompleteArgument( cmdName, startArg, compArg );

	// we found 2+ matches
	if ( field->acOffset == 0 && matchCount >= 2 && con_completionStyle->integer ) {
		const int delta = String_HasLeadingSlash( field->buffer ) ? 0 : 1;
		field->acStartArg = startArg;
		field->acCompArg = compArg;
		field->acOffset = field->cursor + delta;
		field->acLength = 0;
	}
}


void Field_AutoCompleteFrom( int startArg, int compArg, qbool searchCmds, qbool searchVars )
{
	// For the first argument, we always check both variables and commands.
	// For other arguments, we run a custom auto-completion handler
	// registered by the command if one was provided.
	if ( compArg == startArg ) {
		Field_AutoCompleteCmdOrVarName( startArg, compArg, searchCmds, searchVars );
	} else {
		Field_AutoCompleteCommandArgument( startArg, compArg );
	}
}


static void Field_AddLeadingSlash( field_t *field )
{
	const size_t length = strlen( field->buffer );
	if ( length + 1 < sizeof( field->buffer ) ) {
		memmove( field->buffer + 1, field->buffer, length + 1 );
		*field->buffer = '\\';
		field->cursor++;
	}
}


// runs the auto-completion but doesn't do the final leading slash and cursor position fix-ups
// returns qtrue if auto-completion was actually run on an argument
static qbool Field_AutoCompleteNoLeadingSlash( field_t *field )
{
	completionField = field;

	// first, decide which argument we're going to run completion on
	Cmd_TokenizeString( field->buffer );
	const int compArg = Cmd_Argc() == 1 ? 0 : Cmd_ArgIndexFromOffset( field->cursor );
	if ( compArg < 0 || compArg >= Cmd_Argc() )
		return qfalse;

	// now select the actual string that needs completing
	completionString = Cmd_Argv( compArg );
#ifndef DEDICATED
	if ( compArg == 0 && String_HasLeadingSlash( completionString ) )
		completionString++;
#endif
	if ( *completionString == '\0' )
		return qfalse;

	Field_AutoCompleteFrom( 0, compArg, qtrue, qtrue );

	// get rid of any superfluous space between arguments
	if ( matchCount == 0 ) {
		*field->buffer = '\0';
		Field_AppendFirstArgs( field, Cmd_Argc() );
		field->cursor = strlen( field->buffer );
	}

	return qtrue;
}


void Field_AutoComplete( field_t *field, qbool insertBackslash )
{
	const qbool ranComp = Field_AutoCompleteNoLeadingSlash( field );
	const qbool hadSlash = String_HasLeadingSlash( field->buffer );
	if ( !hadSlash && insertBackslash )
		Field_AddLeadingSlash( field );
	if ( ranComp )
		return;

	const int argc = Cmd_Argc();
	if ( argc > 0 ) {
		// keep the whitespace and clamp the cursor to 1 past the last argument
		const int offset = Cmd_ArgOffset( argc - 1 );
		const int length = strlen( Cmd_Argv( argc - 1 ) );
		const int max = offset + length + 1 + ( hadSlash ? 0 : 1 );
		if ( field->cursor > max )
			field->cursor = max;
	} else {
		// the input line is pure whitespace so we rewrite it
		Q_strncpyz ( field->buffer, insertBackslash ? "\\" : "", sizeof( field->buffer ) );
		field->cursor = strlen( field->buffer );
	}
}


void Field_AutoCompleteCustom( int startArg, int compArg, fieldCompletionHandler_t callback )
{
	if ( findIndexOnly ) {
		( *callback )( FindIndexMatch );
		return;
	}

	( *callback )( FindMatches );
	if ( Field_CompleteShortestMatch( startArg, compArg ) )
		( *callback )( PrintMatches );
}


void Field_AutoCompleteMapName( fieldCallback_t callback )
{
	FS_FilenameCompletion( "maps", "bsp", qtrue, callback, 0 );
}


void Field_AutoCompleteConfigName( fieldCallback_t callback )
{
	FS_FilenameCompletion( "", "cfg", qtrue, callback, FS_FILTER_INPAK );
}


void Field_AutoCompleteDemoNameRead( fieldCallback_t callback )
{
	FS_FilenameCompletion( "demos", "dm_66", qtrue, callback, 0 );
	FS_FilenameCompletion( "demos", "dm_67", qtrue, callback, 0 );
	FS_FilenameCompletion( "demos", "dm_68", qtrue, callback, 0 );
}


void Field_AutoCompleteDemoNameWrite( fieldCallback_t callback )
{
	FS_FilenameCompletion( "demos", "dm_68", qtrue, callback, FS_FILTER_INPAK );
}


#ifndef DEDICATED

void Field_AutoCompleteKeyName( fieldCallback_t callback )
{
	Key_KeyNameCompletion( callback );
}

#endif


void Field_InsertValue( field_t *edit, qbool defaultValue )
{
	if ( Cmd_Argc() < 1 )
		return;

	const char* name = Cmd_Argv( 0 );
	if ( name[0] == '\\' || name[0] == '/' )
		name++;

	if ( name[0] == '\0' )
		return;

	const cvar_t* const cvar = Cvar_FindVar( name );
	if ( !cvar )
		return;

	const char* const valueString = defaultValue ? cvar->resetString : cvar->string;
	const qbool quotesNeeded = valueString[0] == '\0' || strchr( valueString, ' ' ) != NULL;
	const char* const quotes = quotesNeeded ? "\"" : "";
	Com_sprintf( edit->buffer, sizeof( edit->buffer ), "\\%s %s", cvar->name, quotes );
	edit->cursor = strlen( edit->buffer );
	Q_strcat( edit->buffer, sizeof( edit->buffer ), va( "%s%s", valueString, quotes ) );
}


void History_Clear( history_t* history, int width )
{
	for ( int i = 0; i < COMMAND_HISTORY; ++i ) {
		Field_Clear( &history->commands[i] );
		history->commands[i].widthInChars = width;
	}
}


static int LengthWithoutTrailingWhitespace( const char* s )
{
	int i = (int)strlen(s);
	while ( i-- ) {
		if ( s[i] != ' ' && s[i] != '\t' )
			return i + 1;
	}

	return 0;
}


void History_SaveCommand( history_t* history, const field_t* edit )
{
	// Avoid having the same command twice in a row.
	// Unfortunately, this has to be case sensitive since case might matter for some commands.
	if ( history->next > 0 ) {
		// The real proper way to ignore whitespace is to tokenize both strings and compare the
		// argument count and then each argument with a case sensitive comparison,
		// but there's only one tokenizer data instance...
		// Instead, we only ignore the trailing whitespace.
		const int lengthCur = LengthWithoutTrailingWhitespace( edit->buffer );
		if ( lengthCur == 0 ) {
			history->display = history->next;
			return;
		}

		const int prevLine = (history->next - 1) % COMMAND_HISTORY;
		const int lengthPrev = LengthWithoutTrailingWhitespace( history->commands[prevLine].buffer );
		if ( lengthCur == lengthPrev && strncmp(edit->buffer, history->commands[prevLine].buffer, lengthCur) == 0 ) {
			history->display = history->next;
			return;
		}
	}

	// copy the line
	history->commands[history->next % COMMAND_HISTORY] = *edit;
	++history->next;
	history->display = history->next;
}


void History_GetPreviousCommand( field_t* edit, history_t* history )
{
	if ( history->next - history->display < COMMAND_HISTORY && history->display > 0 )
		--history->display;
	*edit = history->commands[history->display % COMMAND_HISTORY];
}


void History_GetNextCommand( field_t* edit, history_t* history, int width )
{
	++history->display;
	if ( history->display < history->next ) {
		*edit = history->commands[history->display % COMMAND_HISTORY];
		return;
	}
	
	history->display = history->next;
	Field_Clear( edit );
	edit->widthInChars = width;
}


// It makes no sense for both executables to share the same command history.
#if defined(DEDICATED)
#define HISTORY_PATH "cnq3svcmdhistory"
#else
#define HISTORY_PATH "cnq3cmdhistory"
#endif


void History_LoadFromFile( history_t* history )
{
	fileHandle_t f;
	FS_FOpenFileRead( HISTORY_PATH, &f, qfalse );
	if ( f == 0 )
		return;

	int count;
	if ( FS_Read( &count, sizeof(int), f ) != sizeof(int) ||
		count <= 0 ||
		count > COMMAND_HISTORY ) {
		FS_FCloseFile( f );
		return;
	}

	int lengths[COMMAND_HISTORY];
	const int lengthBytes = sizeof(int) * count;
	if ( FS_Read( lengths, lengthBytes, f ) != lengthBytes ) {
		FS_FCloseFile( f );
		return;
	}

	for ( int i = 0; i < count; ++i ) {
		const int l = lengths[i];
		if ( l <= 0 ||
			FS_Read( history->commands[i].buffer, l, f ) != l ) {
			FS_FCloseFile( f );
			return;
		}
		history->commands[i].buffer[l] = '\0';
		history->commands[i].cursor = l;
	}

	history->next = count;
	history->display = count;
	const int totalCount = ARRAY_LEN( history->commands );
	for ( int i = count; i < totalCount; ++i ) {
		history->commands[i].buffer[0] = '\0';
	}

	FS_FCloseFile( f );
}


void History_SaveToFile( const history_t* history )
{
#if defined( QC )
	if ( !con_history )
		return;
#endif
	if ( con_history->integer == 0 )
		return;

	const fileHandle_t f = FS_FOpenFileWrite( HISTORY_PATH );
	if ( f == 0 )
		return;

	int count = 0;
	int lengths[COMMAND_HISTORY];
	const int totalCount = ARRAY_LEN( history->commands );
	for ( int i = 0; i < totalCount; ++i ) {
		const char* const s = history->commands[(history->display + i) % COMMAND_HISTORY].buffer;
		if ( *s == '\0' )
			continue;

		lengths[count++] = strlen( s );
	}

	FS_Write( &count, sizeof(count), f );
	FS_Write( lengths, sizeof(int) * count, f );
	for ( int i = 0, j = 0; i < totalCount; ++i ) {
		const char* const s = history->commands[(history->display + i) % COMMAND_HISTORY].buffer;
		if ( *s == '\0' )
			continue;

		FS_Write( s, lengths[j++], f );
	}

	FS_FCloseFile( f );
}


const char* Q_itohex( uint64_t number, qbool uppercase, qbool prefix )
{
	static const char* luts[2] = { "0123456789abcdef", "0123456789ABCDEF" };
	static char buffer[19];
	const int maxLength = 16;

	const char* const lut = luts[uppercase == 0 ? 0 : 1];
	uint64_t x = number;
	int i = maxLength + 2;
	buffer[i] = '\0';
	while ( i-- ) {
		buffer[i] = lut[x & 15];
		x >>= 4;
	}

	int startOffset = 2;
	for ( i = 2; i < maxLength + 1; i++, startOffset++ ) {
		if ( buffer[i] != '0' )
			break;
	}

	if ( prefix ) {
		startOffset -= 2;
		buffer[startOffset + 0] = '0';
		buffer[startOffset + 1] = 'x';
	}

	return buffer + startOffset;
}


void Help_AllocSplitText( char** desc, char** help, const char* combined )
{
	if ( *desc != NULL || *help != NULL ) {
		// break here for some debugging fun
		return;
	}

	const char* const newLine = strchr( combined, '\n' );
	if ( !newLine ) {
		*desc = CopyString( combined );
		return;
	}

	const int srcLen = strlen( combined );
	const int descLen = newLine - combined;
	const int helpLen = srcLen - descLen - 1;
	*desc = (char*)S_Malloc( descLen + 1 );
	*help = (char*)S_Malloc( helpLen + 1 );
	memcpy( *desc, combined, descLen );
	memcpy( *help, combined + descLen + 1, helpLen );
	(*desc)[descLen] = '\0';
	(*help)[helpLen] = '\0';
}


void Com_TruncatePrintString( char* buffer, int size, int maxLength )
{
	if ( Q_PrintStrlen( buffer ) <= maxLength )
		return;

	int byteIndex = Q_PrintStroff( buffer, maxLength );
	if ( byteIndex < 0 || byteIndex >= size )
		byteIndex = size - 1;

	buffer[byteIndex - 4] = '.';
	buffer[byteIndex - 3] = '.';
	buffer[byteIndex - 2] = '.';
	buffer[byteIndex - 1] = '\n';
	buffer[byteIndex - 0] = '\0';
}


static void Com_PrintModules( module_t firstModule, int moduleMask, printf_t print )
{
#define MODULE_ITEM(Enum, Desc) Desc, 
	static const char* ModuleNames[MODULE_COUNT + 1] =
	{
		MODULE_LIST(MODULE_ITEM)
		""
	};
#undef MODULE_ITEM

	if ( firstModule == MODULE_NONE || moduleMask == 0 ) {
		print( "Module: Unknown\n" );
		return;
	}

	const int otherModules = moduleMask & (~(1 << firstModule));

	if ( otherModules )
		print( "Modules: " );
	else
		print( "Module: " );
	print( "%s", ModuleNames[firstModule] );
	
	for ( int i = 0; i < 32; ++i ) {
		if ( (otherModules >> i) & 1 )
			print( ", %s", ModuleNames[i] );
	}

	print( "\n" );
}


printHelpResult_t Com_PrintHelp( const char* name, printf_t print, qbool printNotFound, qbool printModules, qbool printFlags )
{
	qbool isCvar = qfalse;
	const char *desc;
	const char *help;
	const char* registeredName;
	module_t firstModule;
	int moduleMask;
	if ( Cvar_GetHelp( &desc, &help, name ) ) {
		isCvar = qtrue;
		Cvar_GetModuleInfo( &firstModule, &moduleMask, name );
		registeredName = Cvar_GetRegisteredName( name );
	} else if ( Cmd_GetHelp( &desc, &help, name ) ) {
		Cmd_GetModuleInfo( &firstModule, &moduleMask, name );
		registeredName = Cmd_GetRegisteredName( name );
	} else {
		if ( printNotFound )
			print( "found no cvar/command with the name '%s'\n", name );
		return PHR_NOTFOUND;
	}

	if ( registeredName )
		name = registeredName;

	if ( isCvar )
		Cvar_PrintFirstHelpLine( name, print );
	else
		print( S_COLOR_CMD "%s\n", name );

	const cvar_t* const cvar = Cvar_FindVar( name );
	if ( cvar != NULL ) {
		const char* const quote1 = cvar->type == CVART_STRING ? "\"" : "";
		const char* const quote2 = cvar->type == CVART_STRING ? "^7\"" : "^7";
		if ( cvar->latchedString != NULL ) {
			const char* const combined =
				va( "Current: %s" S_COLOR_VAL "%s%s (Latched: %s" S_COLOR_VAL "%s%s)\n",
					quote1, cvar->string, quote2, quote1, cvar->latchedString, quote2 );
			if ( strlen( combined ) < CONSOLE_WIDTH ) {
				print( combined );
			} else {
				print( "Current: %s" S_COLOR_VAL "%s%s\n", quote1, cvar->string, quote2 );
				print( "Latched: %s" S_COLOR_VAL "%s%s\n", quote1, cvar->latchedString, quote2 );
			}
		} else {
			print( "Current: %s" S_COLOR_VAL "%s" S_COLOR_HELP "%s\n", quote1, cvar->string, quote2 );
		}
	}

	if ( printModules )
		Com_PrintModules( firstModule, moduleMask, print );

	if ( isCvar && printFlags )
		Cvar_PrintFlags( name, print );

	if ( !desc ) {
		if ( printNotFound )
			print(	"no help text found for %s %s%s\n",
				isCvar ? "cvar" : "command", isCvar ? S_COLOR_CVAR : S_COLOR_CMD, name );
		return PHR_NOHELP;
	}

	const char firstLetter = toupper( *desc );
	print( S_COLOR_HELP "%c%s" S_COLOR_HELP ".\n", firstLetter, desc + 1 );

	if ( help )
		print( S_COLOR_HELP "%s\n", help );

	return PHR_HADHELP;
}


static const char* Com_GetCompilerInfo()
{
#if defined( _MSC_VER )
	typedef struct {
		int mscVer;
		const char* number;
		const char* year;
	} clVersion_t;

#define CL(MacroVersion, RealVersion, Year) { MacroVersion, RealVersion, Year }
	const clVersion_t clVersions[] = {
		CL(1200,  "6.0", ""),
		CL(1300,  "7.0", ".NET 2002"),
		CL(1310,  "7.1", ".NET 2003"),
		CL(1400,  "8.0", "2005"),
		CL(1500,  "9.0", "2005"),
		CL(1600, "10.0", "2010"),
		CL(1700, "11.0", "2012"),
		CL(1800, "12.0", "2013"),
		CL(1900, "14.0", "2015"),
		CL(1910, "15.0", "2017 RTW"),
		CL(1911, "15.3", "2017"),
		CL(1912, "15.5", "2017"),
		CL(1913, "15.6", "2017"),
		CL(1914, "15.7", "2017"),
		CL(1915, "15.8", "2017"),
		CL(1916, "15.9", "2017"),
		CL(1920, "16.0", "2019 RTW"),
		CL(1921, "16.1", "2019"),
		CL(1922, "16.2", "2019"),
		CL(1923, "16.3", "2019"),
		CL(1924, "16.4", "2019"),
		CL(1925, "16.5", "2019"),
		CL(1926, "16.6", "2019"),
		CL(1927, "16.7", "2019"),
		CL(1928, "16.8", "2019")
	};
#undef CL

	const char* info = "VS version unknown - ";
	for ( int i = 0; i < ARRAY_LEN(clVersions); ++i ) {
		if ( clVersions[i].mscVer == _MSC_VER ) {
			info = va("VS %s (%s) - ", clVersions[i].year, clVersions[i].number);
			break;
		}
	}

	// 15.00.20706.01 <-> _MSC_FULL_VER 150020706 and _MSC_BUILD 1
	const char* const fullVerStr = STRINGIZE(_MSC_FULL_VER);
	if ( strlen(fullVerStr) >= 5 ) {
		info = va( "%scl.exe %c%c.%c%c.%s.%02d",
				  info,
				  fullVerStr[0],
				  fullVerStr[1],
				  fullVerStr[2],
				  fullVerStr[3],
				  fullVerStr + 4,
				  int(_MSC_BUILD) );
	} else {
		info = va( "%scl.exe %s.%02d", info, fullVerStr, int(_MSC_BUILD) );
	}

	return info;
// the Clang check needs to be above the GCC check because Clang also defines __GNUC__
#elif defined( __clang__ )
	return va( "Clang %d.%d.%d", __clang_major__, __clang_minor__, __clang_patchlevel__ );
#elif defined( __GNUC__ )
	return va( "GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__ );
#else
	return "Unknown compiler";
#endif
}


const char* Com_FormatBytes( uint64_t numBytes )
{
	const char* units[] = { "bytes", "KB", "MB", "GB" };
	const float dividers[] = { 1.0f, float(1 << 10), float(1 << 20), float(1 << 30) };

	int unit = 0;
	for ( uint64_t vi = numBytes; vi >= 1024; vi >>= 10 ) {
		unit++;
	}

	const float vf = (float)numBytes / dividers[unit];

	return va( "%.3f %s", vf, units[unit] );
}


static int QDECL SortIntDescending( const void* a, const void* b )
{
	return *(const int*)b - *(const int*)a;
}


static int QDECL SortFloatDescending( const void* aPtr, const void* bPtr )
{
	const float a = *(const float*)aPtr;
	const float b = *(const float*)bPtr;

	return (a < b) - (a > b);
}


typedef int (QDECL* qsortCallback_t)( const void*, const void* );


template<typename T>
static void StatsFromArray( const T* samples, int numSamples, qsortCallback_t sortFunction, T* temp, stats_t* stats )
{
	memcpy( temp, samples, sizeof(T) * numSamples );
	qsort( temp, numSamples, sizeof(T), sortFunction );
	stats->median = temp[numSamples / 2];
	stats->percentile99 = temp[numSamples / 100];

	float sum = 0.0f;
	float minimum =  FLT_MAX;
	float maximum = -FLT_MAX;
	for ( int i = 0; i < numSamples; ++i ) {
		const float sample = (float)samples[i];
		sum += sample;
		minimum = min(minimum, sample);
		maximum = max(maximum, sample);
	}
	const float average = sum / (float)numSamples;
	stats->average = average;
	stats->minimum = minimum;
	stats->maximum = maximum;

	float variance = 0.0f;
	for ( int i = 0; i < numSamples; ++i ) {
		const float delta = (float)samples[i] - average;
		variance += delta * delta;
	}
	stats->variance = variance;
	stats->stdDev = sqrtf( variance );
}


void Com_StatsFromArray( const int* input, int numSamples, int* temp, stats_t* stats )
{
	StatsFromArray<int>( input, numSamples, &SortIntDescending, temp, stats );
}


void Com_StatsFromArray( const float* input, int numSamples, float* temp, stats_t* stats )
{
	StatsFromArray<float>( input, numSamples, &SortFloatDescending, temp, stats );
}


void Com_ParseHexColor( float* c, const char* text, qbool hasAlpha )
{
	c[0] = 1.0f;
	c[1] = 1.0f;
	c[2] = 1.0f;
	c[3] = 1.0f;

	unsigned int uc[4];
	if ( hasAlpha ) {
		if ( sscanf(text, "%02X%02X%02X%02X", &uc[0], &uc[1], &uc[2], &uc[3]) != 4 )
			return;
		c[0] = uc[0] / 255.0f;
		c[1] = uc[1] / 255.0f;
		c[2] = uc[2] / 255.0f;
		c[3] = uc[3] / 255.0f;
	} else {
		if ( sscanf(text, "%02X%02X%02X", &uc[0], &uc[1], &uc[2]) != 3 )
			return;
		c[0] = uc[0] / 255.0f;
		c[1] = uc[1] / 255.0f;
		c[2] = uc[2] / 255.0f;
		c[3] = 1.0f;
	}
}


static uint32_t asuint( float x )
{
	return *(uint32_t*)&x;
}


static float asfloat( uint32_t x )
{
	return *(float*)&x;
}


// IEEE-754 16-bit floating-point format (without infinity)
// "Accuracy and performance of the lattice Boltzmann method with 64-bit, 32-bit, and customized 16-bit number formats"
// x86 intrinsic: _cvtsh_ss
float f16tof32( uint16_t x )
{
	const uint32_t e = (x & 0x7C00) >> 10; // exponent
	const uint32_t m = (x & 0x03FF) << 13; // mantissa
	const uint32_t v = asuint((float)m) >> 23; // log2 bit hack to count leading zeros in denormalized format
	const float r = asfloat((x & 0x8000) << 16 | (e != 0) * ((e + 112) << 23 | m) | ((e == 0) & (m != 0)) * ((v - 37) << 23 | ((m << (150 - v)) & 0x007FE000))); // sign : normalized : denormalized

	return r;
}


// IEEE-754 16-bit floating-point format (without infinity)
// "Accuracy and performance of the lattice Boltzmann method with 64-bit, 32-bit, and customized 16-bit number formats"
// x86 intrinsic: _cvtss_sh
uint16_t f32tof16( float x )
{
	const uint32_t b = asuint(x) + 0x00001000; // round-to-nearest-even: add last bit after truncated mantissa
	const uint32_t e = (b & 0x7F800000) >> 23; // exponent
	const uint32_t m = b & 0x007FFFFF; // mantissa; in line below: 0x007FF000 = 0x00800000-0x00001000 = decimal indicator flag - initial rounding
	const uint16_t r = (b & 0x80000000) >> 16 | (e > 112) * ((((e - 112) << 10) & 0x7C00) | m >> 13) | ((e < 113) & (e > 101)) * ((((0x007FF000 + m) >> (125 - e)) + 1) >> 1) | (e > 143) * 0x7FFF; // sign : normalized : denormalized : saturate

	return r;
}

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
// qcommon.h -- definitions common between client and server, but not game or ref modules
#ifndef _QCOMMON_H_
#define _QCOMMON_H_

#include "../qcommon/cm_public.h"


//
// msg.c
//
typedef struct {
	qbool	allowoverflow;	// if qfalse, do a Com_Error
	qbool	overflowed;		// set to qtrue if the buffer size failed (with allowoverflow set)
	qbool	oob;
	byte	*data;
	int		maxsize;
	int		cursize;
	int		readcount;
	int		bit;				// for bitwise reads and writes
} msg_t;

void MSG_Init (msg_t *buf, byte *data, int length);
void MSG_InitOOB( msg_t *buf, byte *data, int length );
void MSG_Clear (msg_t *buf);
void MSG_WriteData (msg_t *buf, const void *data, int length);
void MSG_Bitstream( msg_t *buf );

// TTimo
// copy a msg_t in case we need to store it as is for a bit
// (as I needed this to keep an msg_t from a static var for later use)
// sets data buffer as MSG_Init does prior to do the copy
void MSG_Copy( msg_t* buf, byte* data, int length, const msg_t* src );

void MSG_WriteBits( msg_t *msg, int value, int bits );

void MSG_WriteByte (msg_t *sb, int c);
void MSG_WriteShort (msg_t *sb, int c);
void MSG_WriteLong (msg_t *sb, int c);
void MSG_WriteString (msg_t *sb, const char *s);
void MSG_WriteBigString (msg_t *sb, const char *s);

void	MSG_BeginReading (msg_t *sb);
void	MSG_BeginReadingOOB(msg_t *sb);

int		MSG_ReadBits( msg_t *msg, int bits );

int		MSG_ReadByte (msg_t *sb);
int		MSG_ReadShort (msg_t *sb);
int		MSG_ReadLong (msg_t *sb);
char	*MSG_ReadString (msg_t *sb);
char	*MSG_ReadBigString (msg_t *sb);
char	*MSG_ReadStringLine (msg_t *sb);
void	MSG_ReadData (msg_t *sb, void *buffer, int size);


void MSG_WriteDeltaUsercmdKey( msg_t* msg, int key, const usercmd_t* from, usercmd_t* to );
void MSG_ReadDeltaUsercmdKey( msg_t* msg, int key, const usercmd_t* from, usercmd_t* to );

void MSG_WriteDeltaEntity( msg_t* msg, const entityState_t* from, const entityState_t* to, qbool force );
void MSG_ReadDeltaEntity( msg_t* msg, const entityState_t* from, entityState_t* to, int number );

void MSG_WriteDeltaPlayerstate( msg_t* msg, const playerState_t* from, playerState_t* to );
void MSG_ReadDeltaPlayerstate( msg_t* msg, const playerState_t* from, playerState_t* to );


/*
==============================================================

NET

==============================================================
*/

#define	PACKET_BACKUP	32	// number of old messages that must be kept on client and
							// server for delta comrpession and ping estimation
#define	PACKET_MASK		(PACKET_BACKUP-1)

#define	MAX_PACKET_USERCMDS		32		// max number of usercmd_t in a packet

#define	PORT_ANY			-1

#define	MAX_RELIABLE_COMMANDS	64			// max string commands buffered for restransmit

typedef enum {
	NA_BOT,
	NA_BAD,					// an address lookup failed
	NA_LOOPBACK,
	NA_BROADCAST,
	NA_IP,
} netadrtype_t;

typedef enum {
	NS_CLIENT,
	NS_SERVER
} netsrc_t;

typedef struct {
	netadrtype_t type;
	byte ip[4];
	unsigned short port;
} netadr_t;

void NET_Init();
void NET_Shutdown();
void NET_Restart();
void NET_FlushPacketQueue();
void NET_SendPacket( netsrc_t sock, int length, const void* data, const netadr_t& to );
void QDECL NET_OutOfBandPrint( netsrc_t sock, const netadr_t& adr, PRINTF_FORMAT_STRING const char* format, ... );
void QDECL NET_OutOfBandData( netsrc_t sock, const netadr_t& adr, const byte* data, int len );

qbool NET_CompareAdr( const netadr_t& a, const netadr_t& b );
qbool NET_CompareBaseAdr( const netadr_t& a, const netadr_t& b );
qbool NET_IsLocalAddress( const netadr_t& a );
const char* NET_AdrToString( const netadr_t& a );
qbool NET_StringToAdr( const char* s, netadr_t* a );
qbool NET_GetLoopPacket( netsrc_t sock, netadr_t *net_from, msg_t *net_message );
void NET_Sleep( int msec );


#define MAX_MSGLEN 16384 // max length of a message, which may be fragmented into multiple packets


/*
Netchan handles packet fragmentation and out of order / duplicate suppression
*/

typedef struct {
	netsrc_t	sock;

	int			dropped;			// between last packet and previous

	netadr_t	remoteAddress;
	int			qport;				// qport value to write when transmitting

	// sequencing variables
	int			incomingSequence;
	int			outgoingSequence;

	// incoming fragment assembly buffer
	int			fragmentSequence;
	int			fragmentLength;	
	byte		fragmentBuffer[MAX_MSGLEN];

	// outgoing fragment buffer
	// we need to space out the sending of large fragmented messages
	qbool	unsentFragments;
	int			unsentFragmentStart;
	int			unsentLength;
	byte		unsentBuffer[MAX_MSGLEN];
} netchan_t;

void Netchan_Init( int qport );
void Netchan_Setup( netsrc_t sock, netchan_t *chan, netadr_t adr, int qport );

void Netchan_Transmit( netchan_t *chan, int length, const byte *data );
void Netchan_TransmitNextFragment( netchan_t *chan );

qbool Netchan_Process( netchan_t *chan, msg_t *msg );


/*
==============================================================

PROTOCOL

==============================================================
*/

#define	PROTOCOL_VERSION	68

#define	UPDATE_SERVER_NAME	"update.quake3arena.com"
// override on command line, config files etc.
#if defined( QC )
#define MASTER_SERVER_NAME	"164.90.203.227"
#endif
#ifndef MASTER_SERVER_NAME
#define MASTER_SERVER_NAME	"master.quake3arena.com"
#endif
#ifndef AUTHORIZE_SERVER_NAME
#define	AUTHORIZE_SERVER_NAME	"authorize.quake3arena.com"
#endif

#define	PORT_MASTER			27950
#define	PORT_UPDATE			27951
#ifndef PORT_AUTHORIZE
#define	PORT_AUTHORIZE		27952
#endif
#define	PORT_SERVER			27960
#define	NUM_SERVER_PORTS	4		// broadcast scan this many ports after
									// PORT_SERVER so a single machine can
									// run multiple servers


// the svc_strings[] array in cl_parse.c should mirror this
//
// server to client
//
enum svc_ops_e {
	svc_bad,
	svc_nop,
	svc_gamestate,
	svc_configstring,			// [short] [string] only in gamestate messages
	svc_baseline,				// only in gamestate messages
	svc_serverCommand,			// [string] to be executed by client game module
	svc_download,				// [short] size [size bytes]
	svc_snapshot,
	svc_EOF
};


//
// client to server
//
enum clc_ops_e {
	clc_bad,
	clc_nop, 		
	clc_move,				// [[usercmd_t]
	clc_moveNoDelta,		// [[usercmd_t]
	clc_clientCommand,		// [string] message
	clc_EOF
};

/*
==============================================================

VIRTUAL MACHINE

==============================================================
*/

typedef enum {
	CPU_SSE3  = (1 << 0),
	CPU_SSSE3 = (1 << 1),
	CPU_SSE41 = (1 << 2),
	CPU_SSE42 = (1 << 3)
} cpuFeatureFlags_t;

extern int cpu_features;

typedef struct vm_s vm_t;

typedef enum {
	VMI_NATIVE,
	VMI_BYTECODE,
	VMI_COMPILED
} vmInterpret_t;

typedef enum {
	TRAP_MEMSET = 100,
	TRAP_MEMCPY,
	TRAP_STRNCPY,
	TRAP_SIN,
	TRAP_COS,
	TRAP_ATAN2,
	TRAP_SQRT
	// note that ceil/floor etc have different numbers across VMs
} sharedTraps_t;

typedef enum {
	VM_BAD = -1,
	VM_GAME = 0,
	VM_CGAME,
	VM_UI,
	VM_COUNT
} vmIndex_t;

void	VM_Init();
vm_t	*VM_Create( vmIndex_t index, syscall_t systemCalls, vmInterpret_t interpret );

void	VM_Free( vm_t *vm );
void	VM_Clear(void);
void	VM_Forced_Unload_Start(void);
void	VM_Forced_Unload_Done(void);
vm_t	*VM_Restart( vm_t *vm );

intptr_t	QDECL VM_Call( vm_t *vm, int callNum, ... );

void	VM_Debug( int level );


///////////////////////////////////////////////////////////////


#define MODULE_LIST(X) \
	X(NONE, "") \
	X(COMMON, "Common") \
	X(CLIENT, "Client") \
	X(SERVER, "Server") \
	X(RENDERER, "Renderer") \
	X(SOUND, "Sound") \
	X(INPUT, "Input") \
	X(CONSOLE, "Console") \
	X(CGAME, "CGame") \
	X(GAME, "Game") \
	X(UI, "UI")

#define MODULE_ITEM(Enum, Desc) MODULE_##Enum, 
typedef enum {
	MODULE_LIST(MODULE_ITEM)
	MODULE_COUNT
} module_t;
#undef MODULE_ITEM


/*
==============================================================

CMD

Command text buffering and command execution

==============================================================
*/

/*

Any number of commands can be added in a frame, from several different sources.
Most commands come from either keybindings or console line input, but entire text
files can be execed.

*/

void Cbuf_Init();
// allocates an initial text buffer that will grow as needed

void Cbuf_AddText( const char* text );
// Adds command text at the end of the buffer, does NOT add a final \n

void Cbuf_Execute();
// Pulls off \n terminated lines of text from the command buffer and sends
// them through Cmd_ExecuteString.  Stops when the buffer is empty.
// Normally called once per frame, but may be explicitly invoked.
// Do not call inside a command function, or current args will be destroyed.


/*

Command execution takes a null terminated string, breaks it into tokens,
then searches for a command or variable that matches the first token.

*/

typedef void (*xcommand_t) (void);
typedef void (*xcommandCompletion_t) (int startArg, int compArg);

typedef struct cmdTableItem_s {
	const char*				name;
	xcommand_t				function;
	xcommandCompletion_t	completion;
	const char*				help;
} cmdTableItem_t;

void Cmd_Init();

// called by the init functions of other parts of the program to
// register commands and functions to call for them.
// The cmd_name is referenced later, so it should not be in temp memory
// if function is NULL, the command will be forwarded to the server
// as a clc_clientCommand instead of executed locally
void Cmd_AddCommand( const char* cmd_name, xcommand_t function );

void Cmd_RemoveCommand( const char* cmd_name );

void Cmd_SetHelp( const char* cmd_name, const char* cmd_help );
qbool Cmd_GetHelp( const char** desc, const char** help, const char* cmd_name );	// qtrue if the cmd was found

void Cmd_RegisterTable( const cmdTableItem_t* cmds, int count, module_t module );
void Cmd_UnregisterTable( const cmdTableItem_t* cmds, int count );
#define Cmd_RegisterArray( a, m )	Cmd_RegisterTable( a, ARRAY_LEN(a), m )
#define Cmd_UnregisterArray( a )	Cmd_UnregisterTable( a, ARRAY_LEN(a) )

void Cmd_SetModule( const char* cmd_name, module_t module );

// removes all commands that were *only* registered by the given module
void Cmd_UnregisterModule( module_t module );

void Cmd_GetModuleInfo( module_t* firstModule, int* moduleMask, const char* cmd_name );

const char* Cmd_GetRegisteredName( const char* cmd_name );

// auto-completion of command arguments
void Cmd_SetAutoCompletion( const char* cmd_name, xcommandCompletion_t complete );
void Cmd_AutoCompleteArgument( const char* cmd_name, int startArg, int compArg );

// auto-completion of the command's name
// callback with each valid string
void Cmd_CommandCompletion( void(*callback)(const char* s) );

typedef void (*search_callback_t)( const char* name, const char* desc, const char* help, const char* pattern, qbool cvar );
void Cmd_EnumHelp( search_callback_t callback, const char* pattern );

// the functions that execute commands get their parameters with these
// if arg > argc, Cmd_Argv() will return "", not NULL, so string ops are always safe
int Cmd_Argc();
const char* Cmd_Argv(int arg);
const char* Cmd_Args();
const char* Cmd_ArgsFrom( int arg );
qbool Cmd_ArgQuoted( int arg );
int Cmd_ArgOffset( int arg );				// returns the offset into the Cmd_TokenizeString argument
int Cmd_ArgIndexFromOffset( int offset );	// the argument is the offset into the Cmd_TokenizeString argument
void Cmd_ArgvBuffer( int arg, char *buffer, int bufferLength );
void Cmd_ArgsBuffer( char *buffer, int bufferLength );
const char* Cmd_Cmd(); // note: this is NOT argv[0], it's the entire cmd as a raw string

// break a NUL-terminated string up into argc+argv
void Cmd_TokenizeString( const char* text );
void Cmd_TokenizeStringIgnoreQuotes( const char* text );

void Cmd_ExecuteString( const char* text );
// parse a single line of text into arguments and execute it as if it was typed at the console


/*
==============================================================

CVAR

==============================================================
*/

/*

cvar_t variables are used to hold scalar or string variables that can be changed
or displayed at the console or prog code as well as accessed directly in C code.

The user can access cvars from the console in three ways:
r_draworder			prints the current value
r_draworder 0		sets the current value to 0
set r_draworder 0	as above, but creates the cvar if not present

Cvars are restricted from having the same names as commands to keep this
interface from being ambiguous.

They are also occasionally used to communicated information between different
modules of the program.

*/

typedef struct intValidator_s {
	int min;
	int max;
} intValidator_t;

typedef struct floatValidator_s {
	float min;
	float max;
} floatValidator_t;

typedef union {
	intValidator_t		i;
	floatValidator_s	f;
} cvarValidator_t;

struct cvarGuiValue_t {
	const char* value;
	const char* title;
	const char* desc;
	int valueLength;
};

struct cvarGui_t {
	const char* title;
	const char* desc;
	const char* help;
	cvarGuiValue_t* values;
	int categories;
	int numValues;
	int maxValueLength;
};

// nothing outside the Cvar_*() functions should modify these fields!
typedef struct cvar_s {
	char		*name;
	char		*string;
	char		*resetString;		// cvar_restart will reset to this value
	char		*latchedString;		// for CVAR_LATCH vars
	char		*desc;
	char		*help;
	int			flags;
	cvarType_t	type;
	module_t	firstModule;
	int			moduleMask;
	qboolean	modified;			// set each time the cvar is changed
	int			modificationCount;	// incremented each time the cvar is changed
	float		value;				// atof( string )
	int			integer;			// atoi( string )
	qbool		mismatchPrinted;	// have we already notified of mismatching initial values?
	cvarValidator_t	validator;
	cvarGui_t	gui;
	struct cvar_s *next;
	struct cvar_s *hashNext;
} cvar_t;

typedef struct cvarTableItem_s {
	cvar_t**		cvar;
	const char*		name;
	const char*		reset;
	int				flags;
	cvarType_t		type;
	const char*		min;
	const char*		max;
	const char*		help;
	const char*		guiName;
	int				categories;
	const char*		guiDesc;
	const char*		guiHelp;
	const char*		guiValues;
} cvarTableItem_t;

#define CVARSET_BYPASSLATCH_BIT		1

typedef void ( QDECL *printf_t )( PRINTF_FORMAT_STRING const char* fmt, ... );

cvar_t *Cvar_Get( const char *var_name, const char *value, int flags );
// creates the variable if it doesn't exist, or returns the existing one
// if it exists, the value will not be changed, but flags will be ORed in
// that allows variables to be unarchived without needing bitflags
// if value is "", the value will not override a previously set value.

cvar_t*	Cvar_FindVar( const char* var_name );

void	Cvar_PrintDeprecationWarnings();

void	Cvar_SetHelp( const char *var_name, const char *help );
qbool	Cvar_GetHelp( const char **desc, const char **help, const char* var_name );	// qtrue if the cvar was found

void	Cvar_SetRange( const char *var_name, cvarType_t type, const char *min, const char *max );

void	Cvar_SetDataType( const char* cvarName, cvarType_t type );
void	Cvar_SetMenuData( const char* cvarName, int categories, const char* title, const char* desc, const char* help, const char* values );

void	Cvar_RegisterTable( const cvarTableItem_t* cvars, int count, module_t module );
#define Cvar_RegisterArray(a, m)	Cvar_RegisterTable( a, ARRAY_LEN(a), m )

void	Cvar_SetModule( const char *var_name, module_t module );
void	Cvar_GetModuleInfo( module_t *firstModule, int *moduleMask, const char *var_name );

const char* Cvar_GetRegisteredName( const char *var_name );

void	Cvar_PrintTypeAndRange( const char *var_name, printf_t print );
void	Cvar_PrintFirstHelpLine( const char *var_name, printf_t print );
void	Cvar_PrintFlags( const char *var_name, printf_t print );

void	Cvar_Register( vmCvar_t *vmCvar, const char *varName, const char *defaultValue, int flags );
// basically a slightly modified Cvar_Get for the interpreted modules

void	Cvar_Update( vmCvar_t *vmCvar );
// updates an interpreted module's version of a cvar

void	Cvar_Set( const char *var_name, const char *value );
// will create the variable with no flags if it doesn't exist

cvar_t* Cvar_Set2( const char *var_name, const char *value, int cvarSetFlags );
// will create the variable with no flags if it doesn't exist

void	Cvar_SetValue( const char *var_name, float value );
// expands value to a string and calls Cvar_Set

float	Cvar_VariableValue( const char *var_name );
int		Cvar_VariableIntegerValue( const char *var_name );
// returns 0 if not defined or non numeric

const char* Cvar_VariableString( const char *var_name );
void	Cvar_VariableStringBuffer( const char *var_name, char *buffer, int bufsize );
// returns an empty string if not defined

int Cvar_Flags( const char *var_name );
// returns CVAR_NONEXISTENT if cvar doesn't exist or the flags of that particular CVAR.

cvarType_t Cvar_Type( const char *var_name );

void	Cvar_CommandCompletion( void(*callback)(const char *s) );
// callback with each valid string

void	Cvar_EnumHelp( search_callback_t callback, const char* pattern );

void	Cvar_Reset( const char *var_name );

void	Cvar_SetCheatState();
// reset all testing vars to a safe value

qbool Cvar_Command();
// called by Cmd_ExecuteString when Cmd_Argv(0) doesn't match a known command
// returns true if the command was a variable reference that was handled. (print or change)

void	Cvar_WriteVariables( fileHandle_t f, qbool forceWrite );
// writes lines containing "set variable value" for all variables
// with the archive flag set to qtrue.

void	Cvar_Init();

const char* Cvar_InfoString( int bit );
const char* Cvar_InfoString_Big( int bit );
// returns an info string containing all the cvars that have the given bit set
// in their flags ( CVAR_USERINFO, CVAR_SERVERINFO, CVAR_SYSTEMINFO, etc )
void	Cvar_InfoStringBuffer( int bit, char *buff, int buffsize );

cvar_t* Cvar_GetFirst();

extern	int			cvar_modifiedFlags;
// whenever a cvar is modifed, its flags will be OR'd into this, so
// a single check can determine if any CVAR_USERINFO, CVAR_SERVERINFO,
// etc, variables have been modified since the last check.  The bit
// can then be cleared to allow another change detection.
// when a bind is modified, the CVAR_ARCHIVE bit is set


///////////////////////////////////////////////////////////////


void CRC32_Begin( unsigned int* crc );
void CRC32_ProcessBlock( unsigned int* crc, const void* buffer, unsigned int length );
void CRC32_End( unsigned int* crc );


/*
==============================================================

FILESYSTEM

No stdio calls should be used by any part of the game, because
we need to deal with all sorts of directory and seperator char
issues.
==============================================================
*/

#define MAX_PAKFILES	1024

// hash a filename as a case- and OS- insensitive string with no extension
int Q_FileHash( const char* s, int tablesize );

// referenced flags
// these are in loop specific order so don't change the order
#define FS_GENERAL_REF	0x01
#define FS_UI_REF		0x02
#define FS_CGAME_REF	0x04
#define FS_QAGAME_REF	0x08
// number of id paks that will never be autodownloaded from baseq3
#define NUM_ID_PAKS		9

#define	MAX_FILE_HANDLES	64

qbool FS_Initialized();

void	FS_InitFilesystem();
void	FS_Shutdown( qbool closemfp );

void	FS_ConditionalRestart( int checksumFeed );
void	FS_Restart( int checksumFeed );
// shutdown and restart the filesystem so changes to fs_gamedir can take effect

char	**FS_ListFiles( const char *directory, const char *extension, int *numfiles );
// directory should not have either a leading or trailing /
// if extension is "/", only subdirectories will be returned
// the returned files will not include any directories or /

void	FS_FreeFileList( char **list );

qbool FS_FileExists( const char *file );						// checks in current game dir
qbool FS_FileExistsEx( const char *file, qbool curGameDir );	// if curGameDir is qfalse, checks in "baseq3"

void	FS_ReplaceSeparators( char *path );

char* FS_BuildOSPath( const char *base, const char *game, const char *qpath );

int		FS_LoadStack();

int		FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
int		FS_GetModList(  char *listbuf, int bufsize );

fileHandle_t	FS_FOpenFileWrite( const char *qpath );
// will properly create any needed paths and deal with seperater character issues

fileHandle_t	FS_OpenPipeWrite( const char* command );

fileHandle_t FS_SV_FOpenFileWrite( const char *filename );
int		FS_SV_FOpenFileRead( const char *filename, fileHandle_t *fp );
void	FS_SV_Rename( const char *from, const char *to );
int		FS_FOpenFileRead( const char *qpath, fileHandle_t *file, qbool uniqueFILE, int *pakChecksum = NULL );
// if uniqueFILE is qtrue, then a new FILE will be fopened even if the file
// is found in an already open pak file.  If uniqueFILE is qfalse, you must call
// FS_FCloseFile instead of fclose, otherwise the pak FILE would be improperly closed
// It is generally safe to always set uniqueFILE to qtrue, because the majority of
// file IO goes through FS_ReadFile, which Does The Right Thing already.

int		FS_FOpenAbsoluteRead( const char* absPath, fileHandle_t* file );

qbool	FS_FileIsInPAK( const char* filename, int* pureChecksum, int* checksum );

int		FS_Write( const void *buffer, int len, fileHandle_t f );

int		FS_Read2( void *buffer, int len, fileHandle_t f );
int		FS_Read( void *buffer, int len, fileHandle_t f );
// properly handles partial reads and reads from other dlls

void	FS_FCloseFile( fileHandle_t f );
// note: you can't just fclose from another DLL, due to MS libc issues

int		FS_ReadFile( const char *qpath, void **buffer );
int		FS_ReadFilePak( const char *qpath, void **buffer, int *pakChecksum );
// returns the length of the file
// a null buffer will just return the file length without loading
// as a quick check for existance. -1 length == not present
// A 0 byte will always be appended at the end, so string ops are safe.
// the buffer should be considered read-only, because it may be cached
// for other uses.

void	FS_ForceFlush( fileHandle_t f );
// forces flush on files we're writing to.

void	FS_FreeFile( void *buffer );
// frees the memory returned by FS_ReadFile

void	FS_WriteFile( const char *qpath, const void *buffer, int size );
// writes a complete file, creating any subdirectories needed

int		FS_filelength( fileHandle_t f );
// doesn't work for files that are opened from a pack file

int		FS_FTell( fileHandle_t f );
// where are we?

void	FS_Flush( fileHandle_t f );

void	QDECL FS_Printf( fileHandle_t f, PRINTF_FORMAT_STRING const char *fmt, ... ) __attribute__ ((format (printf, 2, 3)));
// like fprintf

int		FS_FOpenFileByMode( const char *qpath, fileHandle_t *f, fsMode_t mode );
// opens a file for reading, writing, or appending depending on the value of mode

int		FS_Seek( fileHandle_t f, long offset, int origin );
// seek on a file (doesn't work for zip files!!!!!!!!)

qbool	FS_IsZipFile( fileHandle_t f );
// tells us whether we opened a zip file

qbool FS_FilenameCompare( const char *s1, const char *s2 );

const char *FS_LoadedPakChecksums( void );
const char *FS_LoadedPakPureChecksums( void );
// Returns a space separated string containing the checksums of all loaded pk3 files.
// Servers with sv_pure set will get this string and pass it to clients.

const char *FS_ReferencedPakNames( void );
const char *FS_ReferencedPakChecksums( void );
const char *FS_ReferencedPakPureChecksums( void );
// Returns a space separated string containing the checksums of all loaded
// AND referenced pk3 files. Servers with sv_pure set will get this string
// back from clients for pure validation 

void FS_ClearPakReferences( int flags );
// clears referenced booleans on loaded pk3s

void FS_PureServerSetReferencedPaks( const char *pakSums, const char *pakNames );
void FS_PureServerSetLoadedPaks( const char *pakSums );
// If the string is empty, all data sources will be allowed.
// If not empty, only pk3 files that match one of the space
// separated checksums will be checked for files, with the
// sole exception of .cfg files.

qbool FS_CheckDirTraversal(const char *checkdir);
qbool FS_idPak( const char* pak, const char* base );
qbool FS_ComparePaks( char *neededpaks, int len, qbool dlstring );
void FS_MissingPaks( unsigned int* checksums, int* checksumCount, int maxChecksums );
qbool FS_PakExists( unsigned int checksum );

void FS_Rename( const char *from, const char *to );

void FS_Remove( const char *osPath );
void FS_HomeRemove( const char *homePath );

#define FS_FILTER_INPAK		(1 << 0)
#define FS_FILTER_NOTINPAK	(1 << 1)

void FS_FilenameCompletion( const char *dir, const char *ext, qbool stripExt,
							void (*callback)(const char *s), int filters );

qbool FS_GetPakPath( char *name, int nameSize, int pakChecksum );


/*
==============================================================

Edit fields and command line history/completion

==============================================================
*/

#define	MAX_EDIT_LINE	256
typedef struct {
	char	buffer[MAX_EDIT_LINE];
	int		cursor;
	int		scroll;
	int		widthInChars;
	int		acOffset;		// auto-completion letter index with the leading slash present
	int		acLength;		// auto-completion letter count
	int		acStartArg;		// auto-completion command token index
	int		acCompArg;		// auto-completion argument token index
} field_t;

void Field_Clear( field_t *edit );
void Field_AutoComplete( field_t *edit, qbool insertBackslash ); // should only be called by Console_Key
void Field_InsertValue( field_t *edit, qbool defaultValue );

typedef void (*fieldCallback_t)( const char* );
typedef void (*fieldCompletionHandler_t)( fieldCallback_t );

// these are the only Field_ functions you can call from a Cmd_SetAutoCompletion callback
void Field_AutoCompleteFrom( int startArg, int compArg, qbool searchCmds, qbool searchVars );
void Field_AutoCompleteCustom( int startArg, int compArg, fieldCompletionHandler_t callback );

// these should only be passed as an argument to Field_AutoCompleteCustom
void Field_AutoCompleteMapName( fieldCallback_t callback );
void Field_AutoCompleteConfigName( fieldCallback_t callback );
void Field_AutoCompleteDemoNameRead( fieldCallback_t callback );
void Field_AutoCompleteDemoNameWrite( fieldCallback_t callback );
#ifndef DEDICATED
void Field_AutoCompleteKeyName( fieldCallback_t callback );
#endif

#define COMMAND_HISTORY		32
typedef struct {
	field_t	commands[COMMAND_HISTORY];
	int		next;		// the last line in the history buffer, not masked
	int		display;	// the line being displayed from history buffer
						// will be <= nextHistoryLine
} history_t;

void History_Clear( history_t* history, int width );
void History_SaveCommand( history_t* history, const field_t* edit );
void History_GetPreviousCommand( field_t* edit, history_t* history );
void History_GetNextCommand( field_t* edit, history_t* history, int width );
void History_LoadFromFile( history_t* history );
void History_SaveToFile( const history_t* history );

/*
==============================================================

MISC

==============================================================
*/

// TTimo
// vsnprintf is ISO/IEC 9899:1999
// abstracting this to make it portable
#ifdef WIN32
#define Q_vsnprintf _vsnprintf
#else
// TODO: do we need Mac define?
#define Q_vsnprintf vsnprintf
#endif

// centralizing the declarations for cl_cdkey
// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=470
extern char cl_cdkey[34];

// TTimo
// centralized and cleaned, that's the max string you can send to a Com_Printf / Com_DPrintf (above gets truncated)
#define	MAXPRINTMSG	4096

struct stats_t
{
	float minimum;
	float maximum;
	float average;
	float median;
	float variance;
	float stdDev;
	float percentile99;
};

char*		CopyString( const char *in );
void		Info_Print( const char *s );

void		Com_BeginRedirect (char *buffer, int buffersize, void (*flush)(char *));
void		Com_EndRedirect( void );
void		QDECL Com_DPrintf( PRINTF_FORMAT_STRING const char *fmt, ... );
void		Com_Quit( int status );
int			Com_EventLoop();
int			Com_Milliseconds();	// will be journaled properly
unsigned	Com_BlockChecksum( const void *buffer, int length );
char		*Com_MD5File(const char *filename, int length);
int			Com_HashKey(char *string, int maxlen);
int			Com_Filter( const char* filter, const char* name );
int			Com_FilterPath( const char* filter, const char* name );
int			Com_RealTime(qtime_t *qtime);
qbool		Com_SafeMode();
const char	*Com_FormatBytes( uint64_t numBytes );
void		Com_StatsFromArray( const int* input, int numSamples, int* temp, stats_t* stats );
void		Com_StatsFromArray( const float* input, int numSamples, float* temp, stats_t* stats );

void		Com_StartupVariable( const char *match );
// checks for and removes command line "+set var arg" constructs
// if match is NULL, all set commands will be executed, otherwise
// only a set with the exact name.  Only used during startup.

void		Com_ParseHexColor( float* color, const char* text, qbool hasAlpha );


extern	cvar_t	*com_developer;
extern	cvar_t	*com_dedicated;
extern	cvar_t	*com_speeds;
extern	cvar_t	*com_timescale;
extern	cvar_t	*com_sv_running;
extern	cvar_t	*com_cl_running;
extern	cvar_t	*com_viewlog;			// 0 = hidden, 1 = visible, 2 = minimized
extern	cvar_t	*com_version;
extern	cvar_t	*com_journal;

// both client and server must agree to pause
extern	cvar_t	*cl_paused;
extern	cvar_t	*sv_paused;

extern	cvar_t	*cl_packetdelay;
extern	cvar_t	*sv_packetdelay;

// com_speeds times
extern	int		time_game;
extern	int		time_frontend;
extern	int		time_backend;		// renderer backend time

extern	int		com_frameTime;

extern	qbool	com_errorEntered;

extern	fileHandle_t	com_journalDataFile;

// use these variables instead of including git.h to avoid triggering rebuilds
extern	const char* const	com_cnq3VersionWithHash;
extern	const char* const	com_gitBranch;
extern	const char* const	com_gitCommit;

typedef enum {
	TAG_FREE,
	TAG_GENERAL,
	TAG_BOTLIB,
	TAG_RENDERER,
	TAG_SMALL,
	TAG_STATIC
} memtag_t;

/*

--- low memory ----
server vm
server clipmap
---mark---
renderer initialization (shaders, etc)
UI vm
cgame vm
renderer map
renderer models

---free---

temp file loading
--- high memory ---

*/

#if defined(_DEBUG) && !defined(BSPC)
	#define ZONE_DEBUG
#endif

#ifdef ZONE_DEBUG
#define Z_TagMalloc(size, tag)			Z_TagMallocDebug(size, tag, #size, __FILE__, __LINE__)
#define Z_Malloc(size)					Z_MallocDebug(size, #size, __FILE__, __LINE__)
#define S_Malloc(size)					S_MallocDebug(size, #size, __FILE__, __LINE__)
void *Z_TagMallocDebug( int size, int tag, char *label, char *file, int line );	// NOT 0 filled memory
void *Z_MallocDebug( int size, char *label, char *file, int line );			// returns 0 filled memory
void *S_MallocDebug( int size, char *label, char *file, int line );			// returns 0 filled memory
#else
void *Z_TagMalloc( int size, int tag );	// NOT 0 filled memory
void *Z_Malloc( int size );			// returns 0 filled memory
void *S_Malloc( int size );			// NOT 0 filled memory only for small allocations
#endif
void Z_Free( void *ptr );
int Z_AvailableMemory( void );
// deliberately NOT overloading global new here
//void* operator new( size_t size ) { return Z_Malloc(size); }
template <class T> T* Z_New() { return (T*)Z_Malloc(sizeof(T)); }
template <class T> T* Z_New( int c ) { return (T*)Z_Malloc(sizeof(T) * c); }

void Hunk_Clear();
void Hunk_ClearToMark();
void Hunk_SetMark();
qbool Hunk_CheckMark();
void Hunk_ClearTempMemory();
void* Hunk_AllocateTempMemory( int size );
void Hunk_FreeTempMemory( void* buf );
int Hunk_MemoryRemaining();
template <class T> T* H_New( ha_pref heap ) { return (T*)Hunk_Alloc(sizeof(T), heap); }
template <class T> T* H_New( int c, ha_pref heap ) { return static_cast<T*>(Hunk_Alloc(sizeof(T) * c, heap)); }

void Com_TouchMemory();

// commandLine should not include the executable name (argv[0])
void Com_Init( char *commandLine );
void Com_Frame( qbool demoPlayback );
void Com_Shutdown();


/*
==============================================================

CLIENT / SERVER SYSTEMS

==============================================================
*/

//
// client interface
//
void CL_InitKeyCommands();
// the keyboard binding interface must be setup before execing
// config files, but the rest of client startup will happen later

void CL_Init();
void CL_Disconnect( qbool showMainMenu );
void CL_Shutdown();
void CL_Frame( int msec );
qbool CL_ShouldSleep();
qbool CL_GameCommand();
void CL_KeyEvent (int key, qbool down, unsigned time);

void CL_CharEvent( int key );
// char events are for field typing, not game control

void CL_MouseEvent( int dx, int dy, int time );

void CL_JoystickEvent( int axis, int value, int time );

void CL_PacketEvent( netadr_t from, msg_t *msg );

void CL_ConsolePrint( const char* s );

void CL_AbortFrame();

void CL_ShutdownCGame();
void CL_ShutdownUI();

void CL_MapLoading( void );
// do a screen update before starting to load a map
// when the server is going to load a new map, the entire hunk
// will be cleared, so the client must shutdown cgame, ui, and
// the renderer

void CL_ForwardCommandToServer( const char *string );
// adds the current command line as a clc_clientCommand to the client message.
// things like godmode, noclip, etc, are commands directed to the server,
// so when they are typed in at the console, they will need to be forwarded.

void CL_CDDialog( void );
// bring up the "need a cd to play" dialog

void CL_ShutdownAll( void );
// shutdown all the client stuff

void CL_FlushMemory();
// dump all memory on an error

void CL_StartHunkUsers( void );
// start all the client stuff using the hunk

void CL_ForwardUIError( int level, int module, const char* error );
// error forwarding extension

qbool CL_DemoPlaying();
// qtrue if demo playback is in progress

void CL_NextDemo();
// invoke the "nextdemo" cvar as a command, if not empty

void CL_DisableFramerateLimiter();
// map loads might be interrupted by a drop-style error,
// which would leave the FPS limit enabled until the next successful map load
// this should therefore always be called by Com_Error

void CL_SetMenuData( qboolean typeOnly );
// sets GUI data for CVars registered by ui.qvm and cgame.qvm

void CL_NDP_HandleError();
// the new demo player's own error handler

void Key_KeyNameCompletion( void (*callback)(const char *s) );
// for /bind and /unbind auto-completion

void Key_WriteBindings( fileHandle_t f );
// for writing the config files

void S_ClearSoundBuffer( void );
// call before filesystem access

void R_WaitBeforeInputSampling();
// delays input sampling when V-Sync is enabled to reduce input latency


//
// server interface
//
void SV_Init();
void SV_Shutdown( const char* finalmsg );
void SV_Frame( int msec );
int SV_FrameSleepMS();	// the number of milli-seconds Com_Frame should sleep
void SV_PacketEvent( const netadr_t& from, msg_t* msg );
qbool SV_GameCommand();
void SV_ShutdownGameProgs();


//
// UI interface
//
qbool UI_GameCommand();


/*
==============================================================

NON-PORTABLE SYSTEM SERVICES

==============================================================
*/

typedef enum {
	AXIS_SIDE,
	AXIS_FORWARD,
	AXIS_UP,
	AXIS_ROLL,
	AXIS_YAW,
	AXIS_PITCH,
	MAX_JOYSTICK_AXIS
} joystickAxis_t;

typedef enum {
	SE_NONE,	// evTime is still valid
	SE_KEY,		// evValue is a key code, evValue2 is the down flag
	SE_CHAR,	// evValue is an ascii char
	SE_MOUSE,	// evValue and evValue2 are reletive signed x / y moves
	SE_JOYSTICK_AXIS,	// evValue is an axis number and evValue2 is the current state (-127 to 127)
	SE_CONSOLE,	// evPtr is a char*
	SE_PACKET	// evPtr is a netadr_t followed by data bytes to evPtrLength
} sysEventType_t;

typedef struct {
	int				evTime;
	sysEventType_t	evType;
	int				evValue, evValue2;
	int				evPtrLength;	// bytes of data pointed to by evPtr, for journaling
	void			*evPtr;			// this must be manually freed if not NULL
} sysEvent_t;

sysEvent_t	Sys_GetEvent();

void	Sys_Init();
void	Sys_Quit( int status ); // status is the engine's exit code

// both of these must handle duplicate calls correctly
// and are only called in a full input subsystem restart (i.e. in_restart)
void	Sys_InitInput();
void	Sys_ShutdownInput();

// general development dll loading for virtual machine testing
void* QDECL	Sys_LoadDll( const char* name, dllSyscall_t *entryPoint, dllSyscall_t systemcalls );
void		Sys_UnloadDll( void* dllHandle );

void QDECL	Sys_Error( PRINTF_FORMAT_STRING const char *error, ...);

// allowed to fail and return NULL
// if it succeeds, returns memory allocated by Z_Malloc
// note that this isn't journaled
char	*Sys_GetClipboardData( void );
void	Sys_SetClipboardData( const char* text );

// relative to window's client rectangle
void	Sys_GetCursorPosition( int* x, int* y );

void	Sys_Print( const char *msg );

// Sys_Milliseconds should only be used for profiling purposes,
// any game related timing information should come from event timestamps
int		Sys_Milliseconds();

// the system console is shown when a dedicated server is running
void	Sys_ShowConsole( int level, qbool quitOnClose );

void	Sys_SetErrorText( const char *text );

// net_ip.cpp
// system-specific but not implemented in the platform layer
qbool	Sys_GetPacket( netadr_t* net_from, msg_t* net_message );
void	Sys_SendPacket( int length, const void *data, netadr_t to );
qbool	Sys_StringToAdr( const char *s, netadr_t *a );	// does NOT parse port numbers, only base addresses
qbool	Sys_IsLANAddress( const netadr_t& adr );
void	Sys_ShowIP();

void		Sys_Mkdir( const char* path );
const char* Sys_Cwd();
const char* Sys_DefaultHomePath();

char**	Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, qbool wantsubs );
void	Sys_FreeFileList( char **list );

qbool	Sys_LowPhysicalMemory( void );

qbool	Sys_HardReboot(); // qtrue when the server can restart itself

qbool	Sys_HasCNQ3Parent();					// qtrue if a child of CNQ3
int		Sys_GetUptimeSeconds( qbool parent );	// negative if not available

void	Sys_LoadHistory();
void	Sys_SaveHistory();

void	Sys_Sleep( int ms );
void	Sys_MicroSleep( int us );
int64_t	Sys_Microseconds();

// prints text in the debugger's output window
void	Sys_DebugPrintf( PRINTF_FORMAT_STRING const char* fmt, ... );
qbool	Sys_IsDebuggerAttached();

qbool	Sys_IsAbsolutePath( const char* path );

#ifndef DEDICATED
qbool	Sys_IsMinimized();
#endif

void	Sys_Crash( const char* message, const char* file, int line, const char* function );

#define CNQ3_WINDOWS_EXCEPTION_CODE 0xDEADBEEF

#define DIE(Message) Sys_Crash(Message, __FILE__, __LINE__, __FUNCTION__)

#if defined(_MSC_VER)
#define ASSERT_OR_DIE(Condition, Message) \
	do { \
		if (!(Condition)) { \
			if (IsDebuggerPresent()) \
				__debugbreak(); \
			else \
				Sys_Crash(Message, __FILE__, __LINE__, __FUNCTION__); \
		} \
	} while (false)
#else
#define ASSERT_OR_DIE(Condition, Message) \
	do { \
		if (!(Condition)) \
			Sys_Crash(Message, __FILE__, __LINE__, __FUNCTION__); \
	} while (false)
#endif

// RenderDoc integration - the API is grabbed at start-up by the OS module
#define CNQ3_RENDERDOC_API_STRUCT  RENDERDOC_API_1_5_0
#define CNQ3_RENDERDOC_API_VERSION eRENDERDOC_API_Version_1_1_0
struct CNQ3_RENDERDOC_API_STRUCT;
extern CNQ3_RENDERDOC_API_STRUCT* renderDocAPI;

// huffman.cpp - id's original code
// used for out-of-band (OOB) datagrams with dynamically created trees
void	DynHuff_Compress( msg_t* buf, int offset );
void	DynHuff_Decompress( msg_t* buf, int offset );

// huffman_static.cpp - new CNQ3 code
// used for every other case with a predefined static tree
int		StatHuff_ReadBit( byte* buffer, int bitIndex );                 // returns the bit read
void	StatHuff_WriteBit( int bit, byte* buffer, int bitIndex );
int		StatHuff_ReadSymbol( int* symbol, byte* buffer, int bitIndex ); // returns the number of bits read
int		StatHuff_WriteSymbol( int symbol, byte* buffer, int bitIndex ); // returns the number of bits written


#define SV_ENCODE_START		4
#define CL_ENCODE_START		12
#define SV_DECODE_START		CL_ENCODE_START
#define CL_DECODE_START		SV_ENCODE_START


const char* Q_itohex( uint64_t number, qbool uppercase, qbool prefix );


void Help_AllocSplitText( char** desc, char** help, const char* combined );


#define CONSOLE_WIDTH	78


void Com_TruncatePrintString( char* buffer, int size, int maxLength );

typedef enum {
	PHR_NOTFOUND,
	PHR_NOHELP,
	PHR_HADHELP
} printHelpResult_t;

printHelpResult_t Com_PrintHelp( const char* name, printf_t print, qbool printNotFound, qbool printModules, qbool printFlags );


#if defined(_MSC_VER) && defined(_DEBUG)
#define Q_assert(Cond) do { if(!(Cond)) { if(Sys_IsDebuggerAttached()) __debugbreak(); else assert((Cond)); } } while(0)
#else
#define Q_assert(Cond)
#endif


float f16tof32( uint16_t x );
uint16_t f32tof16( float x );


// the smallest power of 2 accepted is 1
template<typename T>
static T IsPowerOfTwo( T x )
{
	return x > 0 && (x & (x - 1)) == 0;
}


// returns the original value if the alignment is already respected
// AlignUp(7, 4) -> 8
// AlignUp(8, 4) -> 8
template<typename T>
static T AlignUp( T value, T alignment )
{
	Q_assert(IsPowerOfTwo(alignment));

	const T mask = alignment - 1;

	return (value + mask) & (~mask);
}


// returns the original value if the alignment is already respected
// AlignDown(7, 4) -> 4
// AlignDown(8, 4) -> 8
template<typename T>
static T AlignDown( T value, T alignment )
{
	Q_assert(IsPowerOfTwo(alignment));

	const T mask = alignment - 1;

	return value & (~mask);
}


#endif // _QCOMMON_H_

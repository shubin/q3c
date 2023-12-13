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


#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(__linux__)
#include <sys/sysinfo.h>
#elif defined(__FreeBSD__)
#include <sys/user.h>
#include <sys/sysctl.h>
#endif
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#ifdef DEDICATED
#include <sys/wait.h>
#endif

#include "linux_local.h"

#if defined( QC )
#include "../x86_64/pe_loader.h"
#endif

#define MEM_THRESHOLD 96*1024*1024

#if defined( QC )
cvar_t *sys_peloader;
#endif

static void LIN_MicroSleep( int us )
{
	timespec req, rem;
	req.tv_sec = us / 1000000;
	req.tv_nsec = (us % 1000000) * 1000;
	while (clock_nanosleep(CLOCK_REALTIME, 0, &req, &rem) == EINTR) {
		req = rem;
	}
}


void Sys_Sleep( int ms )
{
	LIN_MicroSleep(ms * 1000);
}


void Sys_MicroSleep( int us )
{
	LIN_MicroSleep(us);
}


int64_t Sys_Microseconds()
{
	timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);

	return (int64_t)ts.tv_sec * 1000000 + (int64_t)ts.tv_nsec / 1000;
}


qboolean Sys_LowPhysicalMemory()
{
	return qfalse; // FIXME
}


void Sys_Error( const char *error, ... )
{
	va_list     argptr;
	char        string[1024];

	if (tty_Enabled())
		tty_Hide();

#ifndef DEDICATED
	CL_Shutdown();
#endif

	va_start (argptr,error);
	vsprintf (string,error,argptr);
	va_end (argptr);
	fprintf(stderr, "Sys_Error: %s\n", string);

	Lin_ConsoleInputShutdown();
	exit(1);
}


void Sys_Quit( int status )
{
	Lin_ConsoleInputShutdown();
	exit( status );
}


///////////////////////////////////////////////////////////////


const char* Sys_DllError()
{
#if defined( QC )
	int err;
	switch ( sys_peloader->integer ) {
		case 1:
			err = PE_GetLastError();
			return err == PE_OK ? NULL : PE_ErrorMessage( err );
		default:
			return dlerror();
	}
#elif
	return dlerror();
#endif
}


void Sys_UnloadDll( void* dllHandle )
{
	if ( !dllHandle )
		return;

#if defined( QC )
	switch ( sys_peloader->integer ) {
		case 1:
			PE_FreeLibrary( (PEHandle)dllHandle );
			break;
		default:
			dlclose( dllHandle );
			break;
	}
#else
	dlclose( dllHandle );
#endif
	const char* err = Sys_DllError();
	if ( err ) {
		Com_Error( ERR_FATAL, "Sys_UnloadDll failed: %s\n", err );
	}
}


static void* try_dlopen( const char* base, const char* gamedir, const char* filename )
{
	const char* fn = FS_BuildOSPath( base, gamedir, filename );

#if defined( QC )
	void *libHandle;
	switch ( sys_peloader->integer ) {
		case 1:
			libHandle = (void*)PE_LoadLibrary( fn );
			break;
		default:
			libHandle = dlopen( fn, RTLD_NOW );
			break;
	}
#else
	void* libHandle = dlopen( fn, RTLD_NOW );
#endif

	if (!libHandle) {
		Com_Printf( "Sys_LoadDll(%s) failed: %s\n", fn, Sys_DllError() );
		return NULL;
	}

	Com_Printf( "Sys_LoadDll(%s) succeeded\n", fn );
	return libHandle;
}


// used to load a development dll instead of a virtual machine
// in release builds, the load procedure matches the VFS logic (fs_homepath, then fs_basepath)
// in debug builds, the current working directory is tried first

void* QDECL Sys_LoadDll( const char* name, dllSyscall_t *entryPoint, dllSyscall_t systemcalls )
{
	char filename[MAX_QPATH];

#if defined( QC )
	switch ( sys_peloader->integer ) {
		case 1:
			Com_sprintf( filename, sizeof( filename ), "%s.dll", name );
			break;
		default:
			Com_sprintf( filename, sizeof( filename ), "%s" DLL_EXT, name );
			break;
	}
#else
	Com_sprintf( filename, sizeof( filename ), "%s" ARCH_STRING DLL_EXT, name );
#endif

	void* libHandle = 0;
	// FIXME: use fs_searchpaths from files.c
	const char* homepath = Cvar_VariableString( "fs_homepath" );
	const char* basepath = Cvar_VariableString( "fs_basepath" );
	const char* gamedir = Cvar_VariableString( "fs_game" );

#ifndef NDEBUG
	libHandle = try_dlopen( Sys_Cwd(), gamedir, filename );
#endif

	if (!libHandle && homepath)
		libHandle = try_dlopen( homepath, gamedir, filename );

	if (!libHandle && basepath)
		libHandle = try_dlopen( basepath, gamedir, filename );

	if ( !libHandle )
		return NULL;

#if defined( QC )
	dllEntry_t dllEntry;
	switch ( sys_peloader->integer ) {
		case 1:
			dllEntry = (dllEntry_t)PE_GetProcAddress( (PEHandle)libHandle, "dllEntry" );
			*entryPoint = (dllSyscall_t)PE_GetProcAddress( libHandle, "vmMain" );
			break;
		default:
			dllEntry_t dllEntry = (dllEntry_t)dlsym( libHandle, "dllEntry" );
			*entryPoint = (dllSyscall_t)dlsym( libHandle, "vmMain" );
			break;
	}
#else
	dllEntry_t dllEntry = (dllEntry_t)dlsym( libHandle, "dllEntry" );
	*entryPoint = (dllSyscall_t)dlsym( libHandle, "vmMain" );
#endif

	if ( !*entryPoint || !dllEntry ) {
		const char* err = Sys_DllError();
		Com_Printf( "Sys_LoadDll(%s) failed dlsym: %s\n", name, err );
		Sys_UnloadDll( libHandle );
		return NULL;
	}

#if defined( QC )
	switch ( sys_peloader->integer ) {
		case 1:
			dllEntry( systemcalls );
			break;
		default:
			dllEntry( systemcalls );
			break;
	}
#else
	dllEntry( systemcalls );
#endif
	return libHandle;
}

#ifdef DEDICATED

char* Sys_GetClipboardData()
{
	return NULL;
}

void Sys_SetClipboardData( const char* )
{
}

#endif

void Sys_Init()
{
#if defined( QC )
	sys_peloader = Cvar_Get( "sys_peloader", "0", CVAR_INIT );
#endif
	Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
}


qbool Sys_HardReboot()
{
#ifdef DEDICATED
	return qtrue;
#else
	return qfalse;
#endif
}


#ifdef DEDICATED


static int Lin_RunProcess( char** argv )
{
	const pid_t pid = fork();
	if (pid == 0) {
		if (execve(argv[0], argv , NULL) == -1) {
			fprintf(stderr, "failed to launch child process: %s\n", strerror(errno));
			_exit(1); // quit without calling atexit handlers
			return 0;
		}
	}

	int status;
	while (waitpid(pid, &status, WNOHANG) == 0)
		sleep(1); // in seconds

    return WEXITSTATUS(status);
}


void Lin_HardRebootHandler( int argc, char** argv )
{
	for (int i = 0; i < argc; ++i) {
		if (!Q_stricmp(argv[i], "nohardreboot")) {
			return;
		}
	}
	
	static char* args[256];
	if (argc + 2 >= sizeof(args) / sizeof(args[0])) {
		fprintf(stderr, "too many arguments: %d\n", argc);
		_exit(1); // quit without calling atexit handlers
		return;
	}

	for (int i = 0; i < argc; ++i)
		args[i] = argv[i];
	args[argc + 0] = (char*)"nohardreboot";
	args[argc + 1] = NULL;

	SIG_InitParent();

	for (;;) {
		if (Lin_RunProcess(args) == 0)
			_exit(0); // quit without calling atexit handlers
	}
}


#endif


static qbool lin_hasParent = qfalse;
static pid_t lix_parentPid;


static const char* Lin_GetExeName(const char* path)
{
	const char* lastSlash = strrchr(path, '/');
	if (lastSlash == NULL)
		return path;

	return lastSlash + 1;
}


void Lin_TrackParentProcess()
{
#if defined(__linux__)

	static char cmdLine[1024];

	char fileName[128];
	Com_sprintf(fileName, sizeof(fileName), "/proc/%d/cmdline", (int)getppid());

	const int fd = open(fileName, O_RDONLY);
	if (fd == -1)
		return;

	const qbool hasCmdLine = read(fd, cmdLine, sizeof(cmdLine)) > 0;
	close(fd);

	if (!hasCmdLine)
		return;

	cmdLine[sizeof(cmdLine) - 1] = '\0';
	lin_hasParent = strcmp(Lin_GetExeName(cmdLine), Lin_GetExeName(q_argv[0])) == 0;
	
#elif defined(__FreeBSD__)

	static char cmdLine[1024];

	int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_ARGS;
    mib[3] = getppid();
    size_t length = sizeof(cmdLine);
    if (sysctl(mib, 4, cmdLine, &length, NULL, 0) != 0)
        return;

	cmdLine[sizeof(cmdLine) - 1] = '\0';
	lin_hasParent = strcmp(Lin_GetExeName(cmdLine), Lin_GetExeName(q_argv[0])) == 0;

#endif
}


qbool Sys_HasCNQ3Parent()
{
	return lin_hasParent;
}


static int Sys_GetProcessUptime( pid_t pid )
{
#if defined(__linux__)

	// length must be in sync with the fscanf call!
	static char word[256];

	// The process start time is the 22nd column and
	// encoded as jiffies after system boot.
	const int jiffiesPerSec = sysconf(_SC_CLK_TCK);
	if (jiffiesPerSec <= 0)
		return -1;

	char fileName[128];
	Com_sprintf(fileName, sizeof(fileName), "/proc/%" PRIu64 "/stat", (uint64_t)pid);
	FILE* const file = fopen(fileName, "r");
	if (file == NULL)
		return -1;

	for (int i = 0; i < 21; ++i) {
		if (fscanf(file, "%255s", word) != 1) {
			fclose(file);
			return -1;
		}
	}

	int64_t jiffies;
	const bool success = fscanf(file, "%" PRId64, &jiffies) == 1;
	fclose(file);

	if (!success)
		return -1;

	const int64_t secondsSinceBoot = jiffies / (int64_t)jiffiesPerSec;
	struct sysinfo info;
	sysinfo(&info);
	const int64_t uptime = (int64_t)info.uptime - secondsSinceBoot;

	return (int)uptime;

#elif defined(__FreeBSD__)

	int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = pid;
    struct kinfo_proc kp;
    size_t len = sizeof(kp);
    if (sysctl(mib, 4, &kp, &len, NULL, 0) != 0) {
        return -1;
    }

	struct timeval now;
	gettimeofday(&now, NULL);

    return (int)(now.tv_sec - kp.ki_start.tv_sec);

#else

	return -1;

#endif
}


int Sys_GetUptimeSeconds( qbool parent )
{
	if (!lin_hasParent)
		return -1;

	return Sys_GetProcessUptime( parent ? getppid() : getpid() );
}


void Sys_LoadHistory()
{
#ifdef DEDICATED
	History_LoadFromFile( tty_GetHistory() );
#else
	History_LoadFromFile( &g_history );
#endif
}


void Sys_SaveHistory()
{
#ifdef DEDICATED
	History_SaveToFile( tty_GetHistory() );
#else
	History_SaveToFile( &g_history );
#endif
}


int Sys_Milliseconds()
{
	static int sys_timeBase = 0;

	struct timeval tv;
	gettimeofday( &tv, NULL );

	if (!sys_timeBase) {
		sys_timeBase = tv.tv_sec;
		return tv.tv_usec/1000;
	}

	return ((tv.tv_sec - sys_timeBase)*1000 + tv.tv_usec/1000);
}


void Sys_Mkdir( const char* path )
{
	mkdir( path, 0777 );
}


#define	MAX_FOUND_FILES	0x1000

// bk001129 - new in 1.26
static void Sys_ListFilteredFiles( const char *basedir, const char *subdirs, const char *filter, char **list, int *numfiles ) {
	char		search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char		filename[MAX_OSPATH];
	DIR			*fdir;
	struct dirent *d;
	struct stat st;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf( search, sizeof(search), "%s/%s", basedir, subdirs );
	}
	else {
		Com_sprintf( search, sizeof(search), "%s", basedir );
	}

	if ((fdir = opendir(search)) == NULL) {
		return;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(filename, sizeof(filename), "%s/%s", search, d->d_name);
		if (stat(filename, &st) == -1)
			continue;

		if (st.st_mode & S_IFDIR) {
			if (Q_stricmp(d->d_name, ".") && Q_stricmp(d->d_name, "..")) {
				if (strlen(subdirs)) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s/%s", subdirs, d->d_name);
				}
				else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", d->d_name);
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s/%s", subdirs, d->d_name );
		if (!Com_FilterPath( filter, filename ))
			continue;
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	}

	closedir(fdir);
}

char **Sys_ListFiles( const char *directory, const char *extension, const char *filter, int *numfiles, qboolean wantsubs )
{
	struct dirent *d;
	DIR		*fdir;
	qboolean dironly = wantsubs;
	char		search[MAX_OSPATH];
	int			nfiles;
	char		**listCopy;
	char		*list[MAX_FOUND_FILES];
	int			i;
	struct stat st;

	int			extLen;

	if (filter) {

		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = NULL;
		*numfiles = nfiles;

		if (!nfiles)
			return NULL;

		listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension)
		extension = "";

	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		dironly = qtrue;
	}

	extLen = strlen( extension );

	// search
	nfiles = 0;

	if ((fdir = opendir(directory)) == NULL) {
		*numfiles = 0;
		return NULL;
	}

	while ((d = readdir(fdir)) != NULL) {
		Com_sprintf(search, sizeof(search), "%s/%s", directory, d->d_name);
		if (stat(search, &st) == -1)
			continue;
		if ((dironly && !(st.st_mode & S_IFDIR)) ||
			(!dironly && (st.st_mode & S_IFDIR)))
			continue;

		if (*extension) {
			if ( strlen( d->d_name ) < strlen( extension ) ||
				Q_stricmp(
					d->d_name + strlen( d->d_name ) - strlen( extension ),
					extension ) ) {
				continue; // didn't match
			}
		}

		if ( nfiles == MAX_FOUND_FILES - 1 )
			break;
		list[ nfiles ] = CopyString( d->d_name );
		nfiles++;
	}

	list[ nfiles ] = NULL;

	closedir(fdir);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	return listCopy;
}

void	Sys_FreeFileList( char **list ) {
	int		i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


const char* Sys_Cwd()
{
	static char cwd[MAX_OSPATH];

	getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}


const char* Sys_DefaultHomePath()
{
	// Used to determine where to store user-specific files
	static char homePath[MAX_OSPATH];

	if (*homePath)
		return homePath;

	const char* const p = getenv("HOME");
	if (p != NULL) {
		Q_strncpyz(homePath, p, sizeof(homePath));
#ifdef MACOS_X
		Q_strcat(homePath, sizeof(homePath), "/Library/Application Support/Quake3");
#else
		Q_strcat(homePath, sizeof(homePath), "/.q3a");
#endif
		if (mkdir(homePath, 0777)) {
			if (errno != EEXIST)
				Sys_Error("Unable to create directory \"%s\", error is %s(%d)\n", homePath, strerror(errno), errno);
		}
		return homePath;
	}

	return ""; // assume current dir
}


void Sys_ShowConsole( int visLevel, qboolean quitOnClose )
{
}


#define	MAX_QUED_EVENTS		512
#define	MASK_QUED_EVENTS	( MAX_QUED_EVENTS - 1 )

static sysEvent_t	eventQue[MAX_QUED_EVENTS];
static int			eventHead, eventTail;


// a time of 0 will get the current time
// ptr should either be null, or point to a block of data that can be freed by the game later

void Lin_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr )
{
	sysEvent_t* ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];

	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf("Sys_QueEvent: overflow\n");
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr )
			Z_Free( ev->evPtr );
		++eventTail;
	}

	++eventHead;

	if ( time == 0 )
		time = Sys_Milliseconds();

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}


sysEvent_t Sys_GetEvent()
{
	// return if we have data
	if ( eventHead > eventTail ) {
		++eventTail;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// check for console commands
	const char* s = Lin_ConsoleInput();
	if ( s ) {
		const int slen = strlen( s );
		const int blen = slen + 1;
		char* b = (char*)Z_Malloc( blen );
		Q_strncpyz( b, s, blen );
		Lin_QueEvent( 0, SE_CONSOLE, 0, 0, slen, b );
	}

#ifndef DEDICATED
	sdl_PollEvents();
#endif

	// check for network packets
	msg_t		netmsg;
	netadr_t	adr;
	static byte sys_packetReceived[MAX_MSGLEN]; // static or it'll blow half the stack
	MSG_Init( &netmsg, sys_packetReceived, sizeof( sys_packetReceived ) );
	if ( Sys_GetPacket( &adr, &netmsg ) ) {
		// copy out to a separate buffer for queuing
		int len = sizeof( netadr_t ) + netmsg.cursize;
		netadr_t* buf = (netadr_t*)Z_Malloc( len );
		*buf = adr;
		memcpy( buf+1, netmsg.data, netmsg.cursize );
		Lin_QueEvent( 0, SE_PACKET, 0, 0, len, buf );
	}

	// return if we have data
	if ( eventHead > eventTail ) {
		++eventTail;
		return eventQue[ ( eventTail - 1 ) & MASK_QUED_EVENTS ];
	}

	// create an empty event to return
	sysEvent_t ev;
	memset( &ev, 0, sizeof( ev ) );
	ev.evTime = Sys_Milliseconds();
	return ev;
}


void Sys_DebugPrintf( const char*, ... )
{
}


qbool Sys_IsDebuggerAttached()
{
	return qfalse;
}


qbool Sys_IsAbsolutePath( const char* path )
{
	// when the mod feeds us a relative demo file path with a leading slash,
	// we then think it's an absolute file path and fail to open the file...
	// bad: return path[0] == '/';
	return qfalse;
}

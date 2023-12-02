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
// client.h -- primary header for client

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "keys.h"
#include "snd_public.h"
#include "../renderer/tr_public.h"
#include "../qcommon/cg_public.h"
#include "../qcommon/ui_public.h"


// snapshots are a view of the server at a given time
typedef struct {
	qbool		valid;			// cleared if delta parsing was invalid
	int				snapFlags;		// rate delayed and dropped commands

	int				serverTime;		// server time the message is valid for (in msec)

	int				messageNum;		// copied from netchan->incoming_sequence
	int				deltaNum;		// messageNum the delta is from
	int				ping;			// time from when cmdNum-1 was sent to time packet was reeceived
	byte			areamask[MAX_MAP_AREA_BYTES];		// portalarea visibility bits

	int				cmdNum;			// the next cmdNum the server is expecting
	playerState_t	ps;						// complete information about the current player at this time

	int				numEntities;			// all of the entities that need to be presented
	int				parseEntitiesNum;		// at the time of this snapshot

	int				serverCommandNum;		// execute all commands up to this before
											// making the snapshot current
} clSnapshot_t;


/*
=============================================================================

the clientActive_t structure is wiped completely at every
new gamestate_t, potentially several times during an established connection

=============================================================================
*/

typedef struct {
	int		p_cmdNumber;		// cl.cmdNumber when packet was sent
	int		p_serverTime;		// usercmd->serverTime when packet was sent
	int		p_realtime;			// cls.realtime when packet was sent
} outPacket_t;

// the parseEntities array must be large enough to hold PACKET_BACKUP frames of
// entities, so that when a delta compressed message arives from the server
// it can be un-deltad from the original 
#define	MAX_PARSE_ENTITIES	2048

extern int g_console_field_width;

typedef struct {
	int			timeoutcount;		// it requres several frames in a timeout condition
									// to disconnect, preventing debugging breaks from
									// causing immediate disconnects on continue
	clSnapshot_t	snap;			// latest received from server

	int			serverTime;			// may be paused during play
	int			oldServerTime;		// to prevent time from flowing bakcwards
	int			oldFrameServerTime;	// to check tournament restarts
	int			serverTimeDelta;	// cl.serverTime = cls.realtime + cl.serverTimeDelta
									// this value changes as net lag varies
	qbool	extrapolatedSnapshot;	// set if any cgame frame has been forced to extrapolate
									// cleared when CL_AdjustTimeDelta looks at it
	qbool	newSnapshots;		// set on parse of any valid packet

	gameState_t	gameState;			// configstrings
	char		mapname[MAX_QPATH];	// extracted from CS_SERVERINFO

	int			parseEntitiesNum;	// index (not anded off) into cl_parse_entities[]

	int			mouseDx[2], mouseDy[2];	// added to by mouse events
	int			mouseIndex;
	int			mouseTime;				// when the last mouse input was sampled, for cl_drawMouseLag
	int			joystickAxis[MAX_JOYSTICK_AXIS];	// set by joystick events

	// cgame communicates a few values to the client system
	int			cgameUserCmdValue;	// current weapon to add to usercmd_t
	float		cgameSensitivity;

	// cmds[cmdNumber] is the predicted command, [cmdNumber-1] is the last
	// properly generated command
	usercmd_t	cmds[CMD_BACKUP];	// each mesage will send several old cmds
	int			cmdNumber;			// incremented each frame, because multiple
									// frames may need to be packed into a single packet
	int			userCmdTime;		// when the last usercmd_t was generated, for cl_drawMouseLag

	outPacket_t	outPackets[PACKET_BACKUP];	// information about each packet we have sent out

	// the client maintains its own idea of view angles, which are
	// sent to the server each frame.  It is cleared to 0 upon entering each level.
	// the server sends a delta each frame which is added to the locally
	// tracked view angles to account for standing on rotating objects,
	// and teleport direction changes
	vec3_t		viewangles;

	int			serverId;			// included in each client message so the server
												// can tell if it is for a prior map_restart
	// big stuff at end of structure so most offsets are 15 bits or less
	clSnapshot_t	snapshots[PACKET_BACKUP];

	entityState_t	entityBaselines[MAX_GENTITIES];	// for delta compression when not in previous frame

	entityState_t	parseEntities[MAX_PARSE_ENTITIES];
} clientActive_t;

extern	clientActive_t		cl;

/*
=============================================================================

the clientConnection_t structure is wiped when disconnecting from a server,
either to go to a full screen console, play a demo, or connect to a different server

A connection can be to either a server through the network layer or a
demo through a file.

=============================================================================
*/


typedef struct {

	int			clientNum;
	int			lastPacketSentTime;			// for retransmits during connection
	int			lastPacketTime;				// for timeouts

	netadr_t	serverAddress;
	int			connectTime;				// for connection retransmits
	int			connectPacketCount;			// for display on connection dialog
	char		serverMessage[MAX_STRING_CHARS];	// for display on connection dialog

	int			challenge;					// from the server to use for connecting
	int			checksumFeed;				// from the server for checksum calculations

	// these are our reliable messages that go to the server
	int			reliableSequence;
	int			reliableAcknowledge;		// the last one the server has executed
	char		reliableCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];

	// server message (unreliable) and command (reliable) sequence
	// numbers are NOT cleared at level changes, but continue to
	// increase as long as the connection is valid

	// message sequence is used by both the network layer and the
	// delta compression layer
	int			serverMessageSequence;

	// reliable messages received from server
	int			serverCommandSequence;			// the number of the latest available command
	int			lastExecutedServerCommand;		// last server command grabbed or executed with CL_GetServerCommand
	char		serverCommands[MAX_RELIABLE_COMMANDS][MAX_STRING_CHARS];
	qbool		serverCommandsBad[MAX_RELIABLE_COMMANDS];	// non-zero means the command shouldn't be fed to cgame

	// file transfer from server
	fileHandle_t download;
	char	downloadTempName[MAX_OSPATH];
	char	downloadName[MAX_OSPATH];
	int		downloadNumber;
	int		downloadBlock;	// block we are waiting for
	int		downloadCount;	// how many bytes we got
	int		downloadSize;	// how many bytes we got
	char	downloadList[MAX_INFO_STRING]; // list of paks we need to download
	qbool	downloadRestart;	// if true, we need to do another FS_Restart because we downloaded a pak

	// demo information
	char	demoName[MAX_OSPATH];
	qbool	showAnnoyingDemoRecordMessage;
	qbool	demorecording;
	qbool	demoplaying;
	qbool	demowaiting;	// don't record until a non-delta message is received
	qbool	firstDemoFrameSkipped;
	qbool	newDemoPlayer;	// running the new player with rewind support
	fileHandle_t	demofile;

	int			timeDemoFrames;		// counter of rendered frames
	int			timeDemoStart;		// cls.realtime before first frame
	int			timeDemoBaseTime;	// each frame will be at this time + frameNum * 50

	// big stuff at end of structure so most offsets are 15 bits or less
	netchan_t	netchan;
} clientConnection_t;

extern	clientConnection_t clc;

/*
==================================================================

the clientStatic_t structure is never wiped, and is used even when
no client connection is active at all

==================================================================
*/

typedef struct {
	netadr_t	adr;
	int			start;
	int			time;
	char		info[MAX_INFO_STRING];
} ping_t;

typedef struct {
	netadr_t	adr;
	char		hostName[MAX_NAME_LENGTH];
	char		mapName[MAX_NAME_LENGTH];
	char		game[MAX_NAME_LENGTH];
	int			netType;
	int			gameType;
	int			clients;
	int			maxClients;
	int			minPing;
	int			maxPing;
	int			ping;
	qbool	visible;
	int			punkbuster;
} serverInfo_t;

typedef struct {
	byte	ip[4];
	unsigned short	port;
} serverAddress_t;

// CGame VM calls that are extensions
enum {
	CGVM_NDP_END_ANALYSIS,
	CGVM_NDP_ANALYZE_SNAPSHOT,
	CGVM_NDP_ANALYZE_COMMAND,
	CGVM_NDP_GENERATE_COMMANDS, // generate synchronization commands
	CGVM_NDP_IS_CS_NEEDED,      // does this config string need to be re-submitted?
	CGVM_COUNT
};

// UI VM calls that are extensions
enum {
	UIVM_ERROR_CALLBACK, // forward errors to UI?
	UIVM_COUNT
};

typedef struct {
	connstate_t	state;				// connection status
	int			keyCatchers;		// bit flags

	qbool	cddialog;			// bring up the cd needed dialog next frame

	char		servername[MAX_OSPATH];		// name of server from original connect (used by reconnect)
	char		proxyname[MAX_OSPATH];		// name of proxy  from original connect (used by reconnect)

	// when the server clears the hunk, all of these must be restarted
	qbool	rendererStarted;
	qbool	soundStarted;
	qbool	soundRegistered;
	qbool	uiStarted;
	qbool	cgameStarted;

	// extensions VM calls indices
	// 0 when not available
	int			cgvmCalls[CGVM_COUNT];
	int			uivmCalls[UIVM_COUNT];

	// extension: new demo player supported by the mod
	qbool		cgameNewDemoPlayer;

	// extension: forward input to cgame regardless of keycatcher state?
	int			cgameForwardInput;	// 1=mouse, 2=keys (note: we don't forward the escape key)

	int			framecount;
	int			frametime;			// msec since last frame

	int			realtime;			// ignores pause
	int			realFrametime;		// ignoring pause, so console always works

	int			numlocalservers;
	serverInfo_t	localServers[MAX_OTHER_SERVERS];

	int			numglobalservers;
	serverInfo_t  globalServers[MAX_GLOBAL_SERVERS];
	// additional global servers
	int			numGlobalServerAddresses;
	serverAddress_t		globalServerAddresses[MAX_GLOBAL_SERVERS];

	int			numfavoriteservers;
	serverInfo_t	favoriteServers[MAX_OTHER_SERVERS];

	int			nummplayerservers;
	serverInfo_t	mplayerServers[MAX_OTHER_SERVERS];

	int pingUpdateSource;		// source currently pinging or updating

	int masterNum;

	// update server info
	netadr_t	updateServer;
	char		updateChallenge[MAX_TOKEN_CHARS];
	char		updateInfoString[MAX_INFO_STRING];

	netadr_t	authorizeServer;

	// rendering info
	glconfig_t	glconfig;
	qhandle_t	charSetShader;
	qhandle_t	whiteShader;

	// frame-rate limiting, useful for scenarios like CGAME_INIT
	int			maxFPS;				// only active if > 0
	int			nextFrameTimeMS;
	int			oldSwapInterval;
} clientStatic_t;

extern	clientStatic_t		cls;

//=============================================================================

extern	vm_t			*cgvm;	// interface to cgame dll or vm
extern	vm_t			*uivm;	// interface to ui dll or vm
extern	refexport_t		re;		// interface to refresh .dll


//
// cvars
//

// match alert flags (cl_matchAlerts)
#define MAF_UNFOCUSED	1	// enable even when the window is visible but unfocused (otherwise only when minimized)
#define MAF_FLASH		2	// Windows: flash the task bar until unminimized / focused
#define MAF_BEEP		4	// Windows: play a message box beep sound once
#define MAF_UNMUTE		8	// unmute the audio
#define MAF_MAX			15	// max. bit mask value

// auto-mute modes (s_autoMute)
typedef enum {
	AMM_NEVER,
	AMM_UNFOCUSED,
	AMM_MINIMIZED
} autoMuteMode_t;

extern	cvar_t	*cl_debugMove;
extern	cvar_t	*cl_timegraph;
extern	cvar_t	*cl_maxpackets;
extern	cvar_t	*cl_packetdup;
extern	cvar_t	*cl_shownet;
extern	cvar_t	*cl_showSend;
extern	cvar_t	*cl_showTimeDelta;
extern	cvar_t	*cl_serverStatusResendTime;

extern	cvar_t	*cl_timedemo;
extern	cvar_t	*cl_aviFrameRate;
extern	cvar_t	*cl_aviMotionJpeg;

extern	cvar_t	*cl_allowDownload;	// 0=off, 1=CNQ3, -1=id
extern	cvar_t	*cl_inGameVideo;

extern	cvar_t	*cl_matchAlerts;	// bit mask, see the MAF_* constants
extern	cvar_t	*cl_demoPlayer;
extern	cvar_t	*cl_escapeAbortsDemo;

extern	cvar_t	*r_khr_debug;

extern	cvar_t	*s_autoMute;

//=================================================


// cl_main

void CL_Init();
void CL_FlushMemory();
void CL_ShutdownAll(void);
void CL_AddReliableCommand( const char *cmd );

void CL_StartHunkUsers( void );

void CL_Disconnect_f (void);
void CL_StartDemoLoop( void );
void CL_NextDemo( void );
void CL_ReadDemoMessage( void );

void CL_InitDownloads(void);
void CL_NextDownload(void);

qbool CL_CDKeyValidate( const char *key, const char *checksum );
int CL_ServerStatus( char *serverAddress, char *serverStatusString, int maxLen );

void CL_DownloadsComplete();
void CL_DemoCompleted();

qbool CL_Paused();


// cl_browser

void CL_Ping_f();
void CL_ServerStatus_f();
void CL_GetPing( int n, char *buf, int buflen, int *pingtime );
void CL_GetPingInfo( int n, char *buf, int buflen );
void CL_ClearPing( int n );
int CL_GetPingQueueCount();
void CL_ServersResponsePacket( const netadr_t& from, msg_t *msg );
void CL_ServerStatusResponse( const netadr_t& from, msg_t *msg );


//
// cl_input
//
void CL_InitInput();
void CL_ShutdownInput();
void CL_SendCmd();
void CL_ClearState();

void CL_WritePacket( void );

const char* Key_KeynumToString( int keynum );

//
// cl_parse.c
//
extern int cl_connectedToPureServer;

void CL_SystemInfoChanged();
void CL_ParseServerMessage( msg_t *msg );

//====================================================================

void	CL_ServerInfoPacket( netadr_t from, msg_t *msg );
void	CL_LocalServers_f( void );
void	CL_GlobalServers_f( void );
void	CL_FavoriteServers_f( void );
qbool	CL_UpdateVisiblePings_f( int source );


//
// console
//
void CL_ConInit();
void CL_ConShutdown();
float Con_SetConsoleVisibility( float fraction );
void Con_ToggleConsole_f();
void Con_ClearNotify();
void Con_RunConsole();
void Con_DrawConsole();
void Con_ScrollLines( int lines ); // positive means down
void Con_ScrollPages( int pages ); // positive means down
void Con_Top();
void Con_Bottom();
void Con_Close();
void Con_ContinueSearch( qbool forward );
qbool Con_HandleMarkMode( qbool ctrlDown, qbool shiftDown, int key );
qbool Con_IsInMarkMode();
const float* ConsoleColorFromChar( char ccode );


//
// cl_scrn.c
//
typedef enum {
	DSC_NONE,
	DSC_NORMAL,
	DSC_CONSOLE	// extended mode
} drawStringColors_t;

void SCR_Init();
void SCR_UpdateScreen();

void SCR_AdjustFrom640( float *x, float *y, float *w, float *h );
// SCR_ functions operate at native resolution, NOT the virtual 640x480 screen

void SCR_DrawChar( float x, float y, float cw, float ch, int c );
void SCR_DrawString( float x, float y, float cw, float ch, const char* s, qbool allowColor );
void SCR_DrawStringEx( float x, float y, float cw, float ch, const char* s, drawStringColors_t colors, qbool showColorCodes, const float* firstColor );

void SCR_DebugGraph( float value, int color );


//
// cl_cin.c
//

void SCR_DrawCinematic();
void SCR_RunCinematic();
void SCR_StopCinematic();
void CL_PlayCinematic_f( void );
int CIN_PlayCinematic( const char *arg0, int xpos, int ypos, int width, int height, int bits);
e_status CIN_StopCinematic(int handle);
e_status CIN_RunCinematic (int handle);
void CIN_DrawCinematic (int handle);
void CIN_SetExtents (int handle, int x, int y, int w, int h);
void CIN_SetLooping (int handle, qbool loop);
qbool CIN_GrabCinematic( int handle, int* w, int* h, const byte** data, int* client, qbool* dirty ); // qtrue when there's valid data
void CIN_CloseAllVideos(void);

//
// cl_cgame.c
//
void CL_InitCGame();
void CL_ShutdownCGame();
void CL_CGameRendering( stereoFrame_t stereo );
void CL_SetCGameTime();
void CL_ConfigstringModified();
void CL_CGNDP_EndAnalysis( const char* filePath, int firstServerTime, int lastServerTime, qbool videoRestart );
void CL_CGNDP_AnalyzeSnapshot( int progress );
void CL_CGNDP_AnalyzeCommand( int serverTime );
void CL_CGNDP_GenerateCommands( const char** commands, int* numCommandBytes );
qbool CL_CGNDP_IsConfigStringNeeded( int csIndex );

//
// cl_ui.c
//
void CL_InitUI();
void CL_ShutdownUI();
void CL_ForwardUIError( int level, int module, const char* error );
int Key_GetCatcher( void );
void Key_SetCatcher( int catcher );
void LAN_LoadCachedServers( void );
void LAN_SaveServersToCache( void );


//
// cl_net_chan.c
//
void CL_Netchan_Transmit( netchan_t *chan, msg_t* msg);	//int length, const byte *data );
void CL_Netchan_TransmitNextFragment( netchan_t *chan );
qbool CL_Netchan_Process( netchan_t *chan, msg_t *msg );

//
// cl_avi.c
//
qbool CL_OpenAVIForWriting( const char *fileNameNoExt, qbool reOpen );
void CL_TakeVideoFrame( void );
void CL_WriteAVIVideoFrame( const byte *imageBuffer, int size );
void CL_WriteAVIAudioFrame( const byte *pcmBuffer, int size );
qbool CL_CloseAVI( void );
qbool CL_VideoRecording( void );

//
// cl_download.cpp
//
qbool CL_MapDownload_Start( const char* mapName, qbool fromCommand );
qbool CL_MapDownload_Start_MapChecksum( const char* mapName, unsigned int mapCrc32, qbool exactMatch );
qbool CL_MapDownload_Start_PakChecksums( const char* mapName, unsigned int* pakChecksums, int pakCount, qbool exactMatch );
qbool CL_PakDownload_Start( unsigned int pakChecksum, qbool fromCommand, const char* mapName ); // mapName can be NULL
void CL_MapDownload_Continue();
void CL_MapDownload_Init();
qbool CL_MapDownload_Active();
void CL_MapDownload_Cancel();
void CL_MapDownload_DrawConsole( float cw, float ch );
void CL_MapDownload_CrashCleanUp();

//
// cl_gl.cpp
//
qbool CL_GL_WantDebug();	// do we want a debug context from the platform layer?
void CL_GL_Init();			// enables debug output if needed

//
// cl_demo.cpp
//
void CL_NDP_PlayDemo( qbool videoRestart );
void CL_NDP_SetCGameTime();
void CL_NDP_GetCurrentSnapshotNumber( int* snapshotNumber, int* serverTime );
qbool CL_NDP_GetSnapshot( int snapshotNumber, snapshot_t* snapshot );
qbool CL_NDP_GetServerCommand( int serverCommandNumber );
int CL_NDP_Seek( int serverTime );
void CL_NDP_ReadUntil( int serverTime );
void CL_NDP_HandleError();

//
// OS-specific
//
typedef enum {
	SMAE_MATCH_START,
	SMAE_MATCH_END
} sysMatchAlertEvent_t;

void Sys_MatchAlert( sysMatchAlertEvent_t event );

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

#include "server.h"
#include "../qcommon/vm_local.h"

/*
===============
SV_SetConfigstring

===============
*/
void SV_SetConfigstring (int index, const char *val) {
	int		len, i;
	int		maxChunkSize = MAX_STRING_CHARS - 24;
	client_t	*client;

	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_SetConfigstring: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	if ( strlen(val) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "SV_SetConfigstring: CS %d is too long\n", index );
	}

	// don't bother broadcasting an update if no change
	if ( !strcmp( val, sv.configstrings[ index ] ) ) {
		return;
	}

	// change the string in sv
	Z_Free( sv.configstrings[index] );
	sv.configstrings[index] = CopyString( val );

	// send it to all the clients if we aren't
	// spawning a new server
	if ( sv.state == SS_GAME || sv.restarting ) {

		// send the data to all relevent clients
		for (i = 0, client = svs.clients; i < sv_maxclients->integer ; i++, client++) {
			if ( client->state < CS_PRIMED ) {
				continue;
			}
			// do not always send server info to all clients
			if ( index == CS_SERVERINFO && client->gentity && (client->gentity->r.svFlags & SVF_NOSERVERINFO) ) {
				continue;
			}

			len = strlen( val );
			if( len >= maxChunkSize ) {
				int		sent = 0;
				int		remaining = len;
				const char* cmd;
				char	buf[MAX_STRING_CHARS];

				while (remaining > 0 ) {
					if ( sent == 0 ) {
						cmd = "bcs0";
					}
					else if( remaining < maxChunkSize ) {
						cmd = "bcs2";
					}
					else {
						cmd = "bcs1";
					}
					Q_strncpyz( buf, &val[sent], maxChunkSize );

					SV_SendServerCommand( client, "%s %i \"%s\"\n", cmd, index, buf );

					sent += (maxChunkSize - 1);
					remaining -= (maxChunkSize - 1);
				}
			} else {
				// standard cs, just send it
				SV_SendServerCommand( client, "cs %i \"%s\"\n", index, val );
			}
		}
	}
}



/*
===============
SV_GetConfigstring

===============
*/
void SV_GetConfigstring( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetConfigstring: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= MAX_CONFIGSTRINGS ) {
		Com_Error (ERR_DROP, "SV_GetConfigstring: bad index %i\n", index);
	}
	if ( !sv.configstrings[index] ) {
		buffer[0] = 0;
		return;
	}

	Q_strncpyz( buffer, sv.configstrings[index], bufferSize );
}


/*
===============
SV_SetUserinfo

===============
*/
void SV_SetUserinfo( int index, const char *val ) {
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_SetUserinfo: bad index %i\n", index);
	}

	if ( !val ) {
		val = "";
	}

	Q_strncpyz( svs.clients[index].userinfo, val, sizeof( svs.clients[ index ].userinfo ) );
	Q_strncpyz( svs.clients[index].name, Info_ValueForKey( val, "name" ), sizeof(svs.clients[index].name) );
}



/*
===============
SV_GetUserinfo

===============
*/
void SV_GetUserinfo( int index, char *buffer, int bufferSize ) {
	if ( bufferSize < 1 ) {
		Com_Error( ERR_DROP, "SV_GetUserinfo: bufferSize == %i", bufferSize );
	}
	if ( index < 0 || index >= sv_maxclients->integer ) {
		Com_Error (ERR_DROP, "SV_GetUserinfo: bad index %i\n", index);
	}
	Q_strncpyz( buffer, svs.clients[ index ].userinfo, bufferSize );
}


// entity baselines are used to compress non-delta messages to clients
// only the fields that differ from the baseline will be transmitted

static void SV_CreateBaseline()
{
	for (int entnum = 1; entnum < sv.num_entities; ++entnum) {
		sharedEntity_t* svent = SV_GentityNum(entnum);
		if (!svent->r.linked) {
			continue;
		}
		svent->s.number = entnum;
		// take current state as baseline
		sv.svEntities[entnum].baseline = svent->s;
	}
}


/*
===============
SV_BoundMaxClients

===============
*/
void SV_BoundMaxClients( int minimum ) {
	// get the current maxclients value
	Cvar_Get( "sv_maxclients", "8", 0 );

	sv_maxclients->modified = qfalse;

	if ( sv_maxclients->integer < minimum ) {
		Cvar_Set( "sv_maxclients", va("%i", minimum) );
	} else if ( sv_maxclients->integer > MAX_CLIENTS ) {
		Cvar_Set( "sv_maxclients", va("%i", MAX_CLIENTS) );
	}
}


/*
===============
SV_Startup

Called when a host starts a map when it wasn't running
one before.  Successive map or map_restart commands will
NOT cause this to be called, unless the game is exited to
the menu system first.
===============
*/
void SV_Startup( void )
{
	if ( svs.initialized )
		Com_Error( ERR_FATAL, "SV_Startup: svs.initialized" );
    
	SV_BoundMaxClients( 1 );

	svs.clients = Z_New<client_t>( sv_maxclients->integer );
	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * 64;
	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * 64;
	}
	svs.initialized = qtrue;

	Cvar_Set( "sv_running", "1" );
}


/*
==================
SV_ChangeMaxClients
==================
*/
void SV_ChangeMaxClients( void ) {
	int		oldMaxClients;
	int		i;
	int		count;

	// get the highest client number in use
	count = 0;
	for ( i = 0 ; i < sv_maxclients->integer ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			if (i > count)
				count = i;
		}
	}
	count++;

	oldMaxClients = sv_maxclients->integer;
	// never go below the highest client number in use
	SV_BoundMaxClients( count );
	// if still the same
	if ( sv_maxclients->integer == oldMaxClients ) {
		return;
	}

	client_t* oldClients = (client_t*)Hunk_AllocateTempMemory( count * sizeof(client_t) );
	// copy the clients to hunk memory
	for ( i = 0 ; i < count ; i++ ) {
		if ( svs.clients[i].state >= CS_CONNECTED ) {
			oldClients[i] = svs.clients[i];
		}
		else {
			Com_Memset(&oldClients[i], 0, sizeof(client_t));
		}
	}

	// free old clients arrays
	Z_Free( svs.clients );

	// allocate new clients
	svs.clients = Z_New<client_t>( sv_maxclients->integer );
	Com_Memset( svs.clients, 0, sv_maxclients->integer * sizeof(client_t) );

	// copy the clients over
	for ( i = 0 ; i < count ; i++ ) {
		if ( oldClients[i].state >= CS_CONNECTED ) {
			svs.clients[i] = oldClients[i];
		}
	}

	// free the old clients on the hunk
	Hunk_FreeTempMemory( oldClients );
	
	// allocate new snapshot entities
	if ( com_dedicated->integer ) {
		svs.numSnapshotEntities = sv_maxclients->integer * PACKET_BACKUP * 64;
	} else {
		// we don't need nearly as many when playing locally
		svs.numSnapshotEntities = sv_maxclients->integer * 4 * 64;
	}
}


static void SV_ClearServer()
{
	for (int i = 0; i < MAX_CONFIGSTRINGS; ++i) {
		if ( sv.configstrings[i] ) {
			Z_Free( sv.configstrings[i] );
		}
	}
	Com_Memset (&sv, 0, sizeof(sv));
}


// touch the cgame.vm so that a pure client can load it if it's in a seperate pk3

static void SV_TouchCGame()
{
	fileHandle_t f;
	FS_FOpenFileRead( "vm/cgame.qvm", &f, qfalse );
	if ( f ) {
		FS_FCloseFile( f );
	}
}


// change the server to a new map, taking all connected clients along with it.
// this is NOT called for map_restart

void SV_SpawnServer( const char* mapname )
{
	// shut down the existing game if it is running
	SV_ShutdownGameProgs();

	QSUBSYSTEM_INIT_START( "Server" );
	Com_Printf( "Map: %s\n", mapname );

#ifndef DEDICATED
	// if not running a dedicated server CL_MapLoading will connect the client to the server
	// also print some status stuff
	CL_MapLoading();

	// make sure all the client stuff is unloaded
	CL_ShutdownAll();
#endif

	// clear the whole hunk because we're (re)loading the server
	Hunk_Clear();

	// clear collision map data
	CM_ClearMap();

	// init client structures and svs.numSnapshotEntities 
	if ( !Cvar_VariableValue("sv_running") ) {
		SV_Startup();
	} else {
		// check for maxclients change
		if ( sv_maxclients->modified ) {
			SV_ChangeMaxClients();
		}
	}

	// clear all pak references
	FS_ClearPakReferences(-1);

	// allocate the snapshot entities on the hunk
	svs.snapshotEntities = (entityState_t*)Hunk_Alloc( sizeof(entityState_t)*svs.numSnapshotEntities, h_high );
	svs.nextSnapshotEntities = 0;

	// toggle the server bit so clients can detect that a
	// server has changed
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	for (int i=0 ; i<sv_maxclients->integer ; i++) {
		// save when the server started for each client already connected
        if (svs.clients[i].state >= CS_CONNECTED) {
            svs.clients[i].oldServerTime = svs.time;
        }		
	}

	// wipe the entire per-level structure
	SV_ClearServer();
	for ( int i = 0 ; i < MAX_CONFIGSTRINGS ; i++ ) {
		sv.configstrings[i] = CopyString("");
	}

	// make sure we are not paused
	Cvar_Set("cl_paused", "0");

	// get a new checksum feed and restart the file system
	srand(Com_Milliseconds());
	sv.checksumFeed = ( ((int) rand() << 16) ^ rand() ) ^ Com_Milliseconds();
	FS_Restart( sv.checksumFeed );

	unsigned checksum;
	CM_LoadMap( va("maps/%s.bsp", mapname), qfalse, &checksum );

	// set serverinfo visible name
	Cvar_Set( "mapname", mapname );

	Cvar_Set( "sv_mapChecksum", va("%d", (int)checksum) );

	int pakChecksum = 0;
	FS_FileIsInPAK( va("maps/%s.bsp", mapname), NULL, &pakChecksum );
	Cvar_Set( "sv_currentPak", va("%d", (int)pakChecksum) );

	// serverid should be different each time
	sv.serverId = com_frameTime;
	sv.restartedServerId = sv.serverId; // I suppose the init here is just to be safe
	sv.checksumFeedServerId = sv.serverId;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// clear physics interaction links
	SV_ClearWorld ();

	// media configstring setting should be done during
	// the loading stage, so connected clients don't have
	// to load during actual gameplay
	sv.state = SS_LOADING;

	// load and spawn all other entities
	SV_InitGameProgs();

	// don't allow a map_restart if game is modified
	sv_gametype->modified = qfalse;

	// run a few frames to allow everything to settle
	for (int i = 0;i < 3; i++)
	{
		VM_Call (gvm, GAME_RUN_FRAME, svs.time);
		SV_BotFrame (svs.time);
		svs.time += 100;
	}

	// create a baseline for more efficient communications
	SV_CreateBaseline ();

	for (int i=0 ; i<sv_maxclients->integer ; i++) {
		// send the new gamestate to all connected clients
		if (svs.clients[i].state >= CS_CONNECTED) {
			qbool isBot = ( svs.clients[i].netchan.remoteAddress.type == NA_BOT );
			// connect the client again
			const char* denied = (const char*)VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );	// firstTime = qfalse
			if ( denied ) {
				// this generally shouldn't happen, because the client
				// was connected before the level change
				SV_DropClient( &svs.clients[i], denied );
			} else {
				if( !isBot ) {
					// when we get the next packet from a connected client,
					// the new gamestate will be sent
					svs.clients[i].state = CS_CONNECTED;
				}
				else {
					client_t* client = &svs.clients[i];
					client->state = CS_ACTIVE;
					sharedEntity_t* ent = SV_GentityNum( i );
					ent->s.number = i;
					client->gentity = ent;

					client->deltaMessage = -1;
					client->nextSnapshotTime = svs.time;	// generate a snapshot immediately

					VM_Call( gvm, GAME_CLIENT_BEGIN, i );
				}
			}
		}
	}

	// run another frame to allow things to look at all the players
	VM_Call (gvm, GAME_RUN_FRAME, svs.time);
	SV_BotFrame (svs.time);
	svs.time += 100;

	const char *p;
	if ( sv_pure->integer ) {
		// the server sends these to the clients so they will only
		// load pk3s also loaded at the server
		p = FS_LoadedPakChecksums();
		Cvar_Set( "sv_paks", p );
		if (!p[0]) {
			Com_Printf( "WARNING: sv_pure set but no PK3 files loaded\n" );
		}

		// if a dedicated pure server we need to touch the cgame because it could be in a
		// seperate pk3 file and the client will need to load the latest cgame.qvm
		if ( com_dedicated->integer ) {
			SV_TouchCGame();
		}
	}
	else {
		Cvar_Set( "sv_paks", "" );
	}

	// the CNQ3 server doesn't set this anymore as it's of no real use
	// even for the original id download code
	Cvar_Set( "sv_pakNames", "" );

	// the server sends these to the clients so they can figure
	// out which pk3s should be auto-downloaded
	p = FS_ReferencedPakChecksums();
	Cvar_Set( "sv_referencedPaks", p );
	p = FS_ReferencedPakNames();
	Cvar_Set( "sv_referencedPakNames", p );

	// save systeminfo and serverinfo strings
	char systemInfo[16384];
	Q_strncpyz( systemInfo, Cvar_InfoString_Big( CVAR_SYSTEMINFO ), sizeof( systemInfo ) );
	cvar_modifiedFlags &= ~CVAR_SYSTEMINFO;
	SV_SetConfigstring( CS_SYSTEMINFO, systemInfo );

	SV_SetConfigstring( CS_SERVERINFO, Cvar_InfoString( CVAR_SERVERINFO ) );
	cvar_modifiedFlags &= ~CVAR_SERVERINFO;

	// any media configstring setting now should issue a warning
	// and any configstring changes should be reliably transmitted
	// to all clients
	sv.state = SS_GAME;

	// send a heartbeat now so the master will get up to date info
	SV_Heartbeat_f();

	Hunk_SetMark();

	sv.mapLoadTime = Sys_Milliseconds();

	QSUBSYSTEM_INIT_DONE( "Server" );
}


#ifdef DEDICATED
#	define SV_PURE_DEFAULT	"1"
#else
#	define SV_PURE_DEFAULT	"0"
#endif

static const cvarTableItem_t sv_cvars[] =
{
	// serverinfo vars
	{ &sv_gametype, "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH, CVART_INTEGER, NULL, NULL, "game mode" },
	{ NULL, "protocol", XSTRING(PROTOCOL_VERSION), CVAR_SERVERINFO | CVAR_ROM, CVART_INTEGER, NULL, NULL, "protocol version" },
	{ &sv_mapname, "mapname", "nomap", CVAR_SERVERINFO | CVAR_ROM, CVART_STRING, NULL, NULL, "name of the loaded map" },
	{ NULL, "sv_currentPak", "0", CVAR_SERVERINFO | CVAR_ROM, CVART_INTEGER, NULL, NULL, "checksum of the pak the current map is from" },
	{ &sv_privateClients, "sv_privateClients", "0", CVAR_SERVERINFO, CVART_INTEGER, "0", XSTRING(MAX_CLIENTS), "number of password-reserved player slots" },
	{ &sv_hostname, "sv_hostname", "noname", CVAR_SERVERINFO | CVAR_ARCHIVE, CVART_STRING, NULL, NULL, "server name" },
	{ &sv_maxclients, "sv_maxclients", "8", CVAR_SERVERINFO | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(MAX_CLIENTS), "max. player count" },
	{ &sv_minRate, "sv_minRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, CVART_INTEGER, "0", NULL, "min. rate allowed, " S_COLOR_VAL "0 " S_COLOR_HELP "means no limit" },
	{ &sv_maxRate, "sv_maxRate", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, CVART_INTEGER, "0", NULL, "max. rate allowed, " S_COLOR_VAL "0 " S_COLOR_HELP "means no limit" },
	{ &sv_minPing, "sv_minPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, CVART_INTEGER, "0", NULL, "min. ping allowed, " S_COLOR_VAL "0 " S_COLOR_HELP "means no limit" },
	{ &sv_maxPing, "sv_maxPing", "0", CVAR_ARCHIVE | CVAR_SERVERINFO, CVART_INTEGER, "0", NULL, "max. ping allowed, " S_COLOR_VAL "0 " S_COLOR_HELP "means no limit" },
	{ &sv_floodProtect, "sv_floodProtect", "1", CVAR_ARCHIVE | CVAR_SERVERINFO, CVART_BOOL, NULL, NULL, "prevents malicious users from lagging the entire server" },
	// systeminfo vars
	{ NULL, "sv_cheats", "0", CVAR_SYSTEMINFO | CVAR_ROM, CVART_BOOL, NULL, NULL, "enables cheat commands and changing cheat cvars" },
	{ &sv_serverid, "sv_serverid", "0", CVAR_SYSTEMINFO | CVAR_ROM, CVART_INTEGER, NULL, NULL, "server session identifier" },
	{ &sv_pure, "sv_pure", SV_PURE_DEFAULT, CVAR_SYSTEMINFO | CVAR_ROM | CVAR_INIT, CVART_BOOL, NULL, NULL, "disables native code and forces client pk3s to match the server's" },
	{ NULL, "sv_paks", "", CVAR_SYSTEMINFO | CVAR_ROM, CVART_STRING, NULL, NULL, "checksums of the paks you're allowed to load data from" },
	{ NULL, "sv_pakNames", "", CVAR_SYSTEMINFO | CVAR_ROM, CVART_STRING, NULL, NULL, "names of the paks you're allowed to load data from" },
	{ NULL, "sv_referencedPaks", "", CVAR_SYSTEMINFO | CVAR_ROM, CVART_STRING, NULL, NULL, "checksums of the paks you might need to download" },
	{ NULL, "sv_referencedPakNames", "", CVAR_SYSTEMINFO | CVAR_ROM, CVART_STRING, NULL, NULL, "name of the paks you might need to download" },
	// server vars
	{ &sv_rconPassword, "rconPassword", "", CVAR_TEMP, CVART_STRING, NULL, NULL, "password for /" S_COLOR_CMD "rcon " S_COLOR_HELP "access" },
	{ &sv_privatePassword, "sv_privatePassword", "", CVAR_TEMP, CVART_STRING, NULL, NULL, "password for the " S_COLOR_CVAR "sv_privateClients " S_COLOR_HELP "slots" },
	{ &sv_fps, "sv_fps", "30", CVAR_TEMP, CVART_INTEGER, "10", "40", "snapshot generation frequency" },
	{ &sv_timeout, "sv_timeout", "200", CVAR_TEMP, CVART_INTEGER, "0", NULL, "max. seconds allowed without any messages" },
	{ &sv_zombietime, "sv_zombietime", "2", CVAR_TEMP, CVART_INTEGER, "0", NULL, "seconds to sink messages after disconnect" },
	{ &sv_allowDownload, "sv_allowDownload", "0", CVAR_SERVERINFO, CVART_BOOL, NULL, NULL, "enables slow pk3 downloads directly from the server" },
	{ &sv_reconnectlimit, "sv_reconnectlimit", "3", 0, CVART_INTEGER, "0", NULL, "min. seconds between connection attempts" },
	{ &sv_padPackets, "sv_padPackets", "0", 0, CVART_BOOL, NULL, NULL, "add nop bytes to messages" },
	{ &sv_killserver, "sv_killserver", "0", 0, CVART_BOOL, NULL, NULL, "menu system can set to " S_COLOR_VAL "1 " S_COLOR_HELP "to shut server down" },
	{ NULL, "sv_mapChecksum", "", CVAR_ROM, CVART_INTEGER, NULL, NULL, ".bsp file checksum" },
	{ &sv_lanForceRate, "sv_lanForceRate", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, S_COLOR_VAL "1 " S_COLOR_HELP "means uncapped rate on LAN" },
	{ &sv_strictAuth, "sv_strictAuth", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "requires CD key authentication" },
	{ &sv_minRestartDelay, "sv_minRestartDelay", "2", 0, CVART_INTEGER, "1", "48", "min. hours to wait before restarting the server" }
};

#undef SV_PURE_DEFAULT


/*
===============
SV_Init

Only called at main exe startup, not for each game
===============
*/
void SV_BotInitBotLib(void);

void SV_Init()
{
	SV_AddOperatorCommands();

	Cvar_RegisterArray( sv_cvars, MODULE_SERVER );

	sv_master[0] = Cvar_Get ("sv_master1", MASTER_SERVER_NAME, 0 );
	for (int i = 1; i < MAX_MASTER_SERVERS; ++i)
		sv_master[i] = Cvar_Get( va("sv_master%d", i+1), "", 0 );

	// initialize bot cvars so they are listed and can be set before loading the botlib
	SV_BotInitCvars();

	// init the botlib here because we need the pre-compiler in the UI
	SV_BotInitBotLib();

	// register debugging code to evaluate network overhead
	SV_InitNetworkOverhead();
}


/*
==================
SV_FinalMessage

Used by SV_Shutdown to send a final message to all
connected clients before the server goes down.  The messages are sent immediately,
not just stuck on the outgoing message list, because the server is going
to totally exit after returning from this function.
==================
*/
void SV_FinalMessage( const char* message )
{
	int			i, j;
	client_t	*cl;

	// send it twice, ignoring rate
	for ( j = 0 ; j < 2 ; j++ ) {
		for (i=0, cl = svs.clients ; i < sv_maxclients->integer ; i++, cl++) {
			if (cl->state >= CS_CONNECTED) {
				// don't send a disconnect to a local client
				if ( cl->netchan.remoteAddress.type != NA_LOOPBACK ) {
					SV_SendServerCommand( cl, "print \"%s\n\"\n", message );
					SV_SendServerCommand( cl, "disconnect" );
				}
				// force a snapshot to be sent
				cl->nextSnapshotTime = -1;
				SV_SendClientSnapshot( cl );
			}
		}
	}
}


// called when each game quits, before Sys_Quit or Sys_Error

void SV_Shutdown( const char* finalmsg )
{
	if ( !com_sv_running || !com_sv_running->integer ) {
		return;
	}

	Com_Printf( "----- Server Shutdown (%s) -----\n", finalmsg );

	if ( svs.clients && !com_errorEntered ) {
		SV_FinalMessage( finalmsg );
	}

	SV_RemoveOperatorCommands();
	SV_MasterShutdown();
	SV_ShutdownGameProgs();

	// free current level
	SV_ClearServer();

	// free server static data
	if ( svs.clients ) {
		Z_Free( svs.clients );
	}
	Com_Memset( &svs, 0, sizeof( svs ) );

	Cvar_Set( "sv_running", "0" );
	Cvar_Set( "sv_singlePlayer", "0" );

	Com_Printf( "---------------------------\n" );

#ifndef DEDICATED
	// disconnect any local clients
	CL_Disconnect( qfalse );
#endif
}


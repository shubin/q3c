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
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/


// returns the player with player id or name from Cmd_Argv(1)

static client_t* SV_GetPlayerByHandle()
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	const char* const s = Cmd_Argv( 1 );

	// check whether this is a numeric player handle
	int i;
	for ( i = 0; s[i] >= '0' && s[i] <= '9'; i++ );

	if ( !s[i] ) {
		int plid = atoi(s);

		// Check for numeric playerid match
		if ( plid >= 0 && plid < sv_maxclients->integer ) {
			client_t* const cl = &svs.clients[plid];
			
			if ( cl->state )
				return cl;
		}
	}

	char s2[64];
	char n2[64];
	Q_strncpyz( s2, s, sizeof(s2) );
	Q_CleanStr( s2 );

	int	caseSensMatchCount = 0;
	int	caseInsMatchCount  = 0;
	int	caseSensMatchIndex = -1;
	int	caseInsMatchIndex  = -1;
	client_t* cl = svs.clients;

	// check for a name match
	for ( i = 0; i < sv_maxclients->integer; i++, cl++ ) {
		if ( !cl->state )
			continue;

		Q_strncpyz( n2, cl->name, sizeof(n2) );
		Q_CleanStr( n2 );

		if ( !strcmp( n2, s2 ) ) {
			caseSensMatchIndex = i;
			caseSensMatchCount++;
		}

		if ( !Q_stricmp( n2, s2 ) ) {
			caseInsMatchIndex = i;
			caseInsMatchCount++;
		}
	}

	if (caseSensMatchCount == 1)
		return svs.clients + caseSensMatchIndex;

	if (caseInsMatchCount == 1)
		return svs.clients + caseInsMatchIndex;

	if (caseSensMatchCount > 1 || caseInsMatchCount > 1) {
		Com_Printf( "More than 1 player with the name %s\n", s2 );
		return NULL;
	}

	Com_Printf( "Player %s is not on the server\n", s2 );
	return NULL;
}


// returns the player with idnum from Cmd_Argv(1)

static client_t* SV_GetPlayerByNum()
{
	client_t	*cl;
	int			i;
	int			idnum;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		return NULL;
	}

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "No player specified.\n" );
		return NULL;
	}

	const char* s = Cmd_Argv(1);

	for (i = 0; s[i]; i++) {
		if (s[i] < '0' || s[i] > '9') {
			Com_Printf( "Bad slot number: %s\n", s);
			return NULL;
		}
	}
	idnum = atoi( s );
	if ( idnum < 0 || idnum >= sv_maxclients->integer ) {
		Com_Printf( "Bad client slot: %i\n", idnum );
		return NULL;
	}

	cl = &svs.clients[idnum];
	if ( !cl->state ) {
		Com_Printf( "Client %i is not active\n", idnum );
		return NULL;
	}
	return cl;
}

//=========================================================


// restart the server on a different map

static void SV_ChangeMap( qbool cheats )
{
	const char* map = Cmd_Argv(1);
	if ( !map ) {
		return;
	}

	// make sure the level exists before trying to change
	// so that a typo at the server console won't end the game
	const char* mapfile = va( "maps/%s.bsp", map );
	if ( FS_ReadFile( mapfile, NULL ) == -1 ) {
		Com_Printf( "Can't find map %s\n", mapfile );
		return;
	}

	// force latched values to get set
	Cvar_Get( "g_gametype", "0", CVAR_SERVERINFO | CVAR_LATCH );

	// save the map name here cause on a map restart we reload the q3config.cfg
	// and thus nuke the arguments of the map command
	char mapname[MAX_QPATH];
	Q_strncpyz( mapname, map, sizeof(mapname) );

	// start up the map
	SV_SpawnServer( mapname );

	Cvar_Set( "sv_cheats", cheats ? "1" : "0" );
}


static void SV_Map_f( )
{
	SV_ChangeMap( qfalse );
}


static void SV_DevMap_f( )
{
	SV_ChangeMap( qtrue );
}


static void SV_CompleteMap_f( int startArg, int compArg )
{
	if ( startArg + 1 == compArg )
		Field_AutoCompleteCustom( startArg, compArg, &Field_AutoCompleteMapName );
}


/*
Completely restarts a level, but doesn't send a new gamestate to the clients.
This allows fair starts with variable load times.
*/
static void SV_MapRestart_f( )
{
	int			i;

	// make sure we aren't restarting twice in the same frame
	if ( com_frameTime == sv.serverId ) {
		return;
	}

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( sv.restartTime ) {
		return;
	}

    int	delay = Cmd_Argc() > 1 ? atoi( Cmd_Argv(1) ) : 5;
	if ( delay ) {
		sv.restartTime = svs.time + delay * 1000;
		return;
	}

	// check for changes in variables that can't just be restarted
	// dwl: If the game lasts more than two hours, then when restarting map have to restart the server
	if ( sv_maxclients->modified || sv_gametype->modified || (svs.time > 2*60*60*1000)) {
		Com_Printf( "variable change -- restarting.\n" );
		// restart the map the slow way
		char mapname[MAX_QPATH];
		Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );
		SV_SpawnServer( mapname );
		return;
	}

	// toggle the server bit so clients can detect that a
	// map_restart has happened
	svs.snapFlagServerBit ^= SNAPFLAG_SERVERCOUNT;

	// generate a new serverid
	// TTimo - don't update restartedserverId here, otherwise we won't deal correctly with multiple map_restart
	sv.serverId = com_frameTime;
	Cvar_Set( "sv_serverid", va("%i", sv.serverId ) );

	// reset all the vm data in place without changing memory allocation
	// note that we do NOT set sv.state = SS_LOADING, so configstrings that
	// had been changed from their default values will generate broadcast updates
	sv.state = SS_LOADING;
	sv.restarting = qtrue;

	SV_RestartGameProgs();

	// run a few frames to allow everything to settle
	for (i = 0; i < 3; i++)
	{
		VM_Call( gvm, GAME_RUN_FRAME, svs.time );
		svs.time += 100;
	}

	sv.state = SS_GAME;
	sv.restarting = qfalse;

	// connect and begin all the clients
	for (i = 0; i < sv_maxclients->integer; ++i) {
		client_t* client = &svs.clients[i];

		// send the new gamestate to all connected clients
		if ( client->state < CS_CONNECTED) {
			continue;
		}

		qbool isBot = ( client->netchan.remoteAddress.type == NA_BOT );

		// add the map_restart command
		SV_AddServerCommand( client, "map_restart\n" );

		// connect the client again, without the firstTime flag
		const char* denied = (const char*)VM_ExplicitArgPtr( gvm, VM_Call( gvm, GAME_CLIENT_CONNECT, i, qfalse, isBot ) );
		if ( denied ) {
			// this generally shouldn't happen, because the client
			// was connected before the level change
			SV_DropClient( client, denied );
			Com_Printf( "SV_MapRestart_f(%d): dropped client %i - denied!\n", delay, i );
			continue;
		}

        if ( client->state == CS_ACTIVE )
		    SV_ClientEnterWorld( client, &client->lastUsercmd );
	}

	// run another frame to allow things to look at all the players
	VM_Call( gvm, GAME_RUN_FRAME, svs.time );
	svs.time += 100;
}


//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_Kick_f( void ) {
	client_t	*cl;
	int			i;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kick <player name>\nkick all = kick everyone\nkick allbots = kick all bots\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		if ( !Q_stricmp(Cmd_Argv(1), "all") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
					continue;
				}
				SV_DropClient( cl, "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "allbots") ) {
			for ( i=0, cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++ ) {
				if ( !cl->state ) {
					continue;
				}
				if( cl->netchan.remoteAddress.type != NA_BOT ) {
					continue;
				}
				SV_DropClient( cl, "was kicked" );
				cl->lastPacketTime = svs.time;	// in case there is a funny zombie
			}
		}
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
==================
SV_Ban_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_Ban_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banUser <player name>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();

	if (!cl) {
		return;
	}

	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_BanNum_f

Ban a user from being able to play on this server through the auth
server
==================
*/
static void SV_BanNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: banClient <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	// look up the authorize server's IP
	if ( !svs.authorizeAddress.ip[0] && svs.authorizeAddress.type != NA_BAD ) {
		Com_Printf( "Resolving %s\n", AUTHORIZE_SERVER_NAME );
		if ( !NET_StringToAdr( AUTHORIZE_SERVER_NAME, &svs.authorizeAddress ) ) {
			Com_Printf( "Couldn't resolve address\n" );
			return;
		}
		svs.authorizeAddress.port = BigShort( PORT_AUTHORIZE );
		Com_Printf( "%s resolved to %i.%i.%i.%i:%i\n", AUTHORIZE_SERVER_NAME,
			svs.authorizeAddress.ip[0], svs.authorizeAddress.ip[1],
			svs.authorizeAddress.ip[2], svs.authorizeAddress.ip[3],
			BigShort( svs.authorizeAddress.port ) );
	}

	// otherwise send their ip to the authorize server
	if ( svs.authorizeAddress.type != NA_BAD ) {
		NET_OutOfBandPrint( NS_SERVER, svs.authorizeAddress,
			"banUser %i.%i.%i.%i", cl->netchan.remoteAddress.ip[0], cl->netchan.remoteAddress.ip[1], 
								   cl->netchan.remoteAddress.ip[2], cl->netchan.remoteAddress.ip[3] );
		Com_Printf("%s was banned from coming back\n", cl->name);
	}
}

/*
==================
SV_KickNum_f

Kick a user off of the server  FIXME: move to game
==================
*/
static void SV_KickNum_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: kicknum <client number>\n");
		return;
	}

	cl = SV_GetPlayerByNum();
	if ( !cl ) {
		return;
	}
	if( cl->netchan.remoteAddress.type == NA_LOOPBACK ) {
		SV_SendServerCommand(NULL, "print \"%s\"", "Cannot kick host player\n");
		return;
	}

	SV_DropClient( cl, "was kicked" );
	cl->lastPacketTime = svs.time;	// in case there is a funny zombie
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f( void ) {
	int			i, j, l;
	client_t	*cl;
	playerState_t	*ps;
	const char		*s;
	int			ping;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	Com_Printf ("map: %s\n", sv_mapname->string );

	Com_Printf ("num score ping name            lastmsg address               qport rate\n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ----- -----\n");
	for (i=0,cl=svs.clients ; i < sv_maxclients->integer ; i++,cl++)
	{
		if (!cl->state)
			continue;
		Com_Printf ("%3i ", i);
		ps = SV_GameClientNum( i );
		Com_Printf ("%5i ", ps->persistant[PERS_SCORE]);

		if (cl->state == CS_CONNECTED)
			Com_Printf ("CNCT ");
		else if (cl->state == CS_ZOMBIE)
			Com_Printf ("ZMBI ");
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
    // TTimo adding a ^7 to reset the color
    // NOTE: colored names in status breaks the padding (WONTFIX)
    Com_Printf ("^7");
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");

		Com_Printf ("%7i ", svs.time - cl->lastPacketTime );

		s = NET_AdrToString( cl->netchan.remoteAddress );
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
			Com_Printf (" ");
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf (" %5i", cl->rate);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
}


static void SV_ConSay_f(void)
{
	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc () < 2 ) {
		return;
	}

/*
	char text[MAX_STRING_CHARS];
	strcpy(text, "console: ");

	char* p = Cmd_Args();

	if ( *p == '"' ) {
		p++;
		p[strlen(p)-1] = 0;
	}

	strcat(text, p);

	SV_SendServerCommand(NULL, "chat \"%s\n\"", text);
*/

	SV_SendServerCommand(NULL, "chat \"console:%s\n\"", Cmd_Args());
}


/*
==================
SV_Heartbeat_f

Also called by SV_DropClient, SV_DirectConnect, and SV_SpawnServer
==================
*/
void SV_Heartbeat_f( void ) {
	svs.nextHeartbeatTime = -9999999;
}


/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f( void ) {
	Com_Printf ("Server info settings:\n");
	Info_Print ( Cvar_InfoString( CVAR_SERVERINFO ) );
}


/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f( void ) {
	Com_Printf ("System info settings:\n");
	Info_Print ( Cvar_InfoString_Big( CVAR_SYSTEMINFO ) );
}


/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f( void ) {
	client_t	*cl;

	// make sure server is running
	if ( !com_sv_running->integer ) {
		Com_Printf( "Server is not running.\n" );
		return;
	}

	if ( Cmd_Argc() != 2 ) {
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	cl = SV_GetPlayerByHandle();
	if ( !cl ) {
		return;
	}

	Com_Printf( "userinfo\n" );
	Com_Printf( "--------\n" );
	Info_Print( cl->userinfo );
}


static void SV_KillServer_f() {
	SV_Shutdown( "killserver" );
}


static void SV_ServerRestart_f() 
{
    char mapname[MAX_QPATH];

    Com_Printf( "server restarting.\n" );
    Q_strncpyz( mapname, Cvar_VariableString( "mapname" ), sizeof( mapname ) );
    SV_SpawnServer( mapname );
    return;
}


static void SV_RestartProcess_f()
{
	if ( !Sys_HasCNQ3Parent() ) {
		Com_Printf( "this server isn't controlled by a parent CNQ3 process\n" );
		return;
	}

	Com_Quit(1);
}


static const char* SV_FormatUptime( int seconds )
{
	static char result[32];

	static const int unitCount = 4;
	static const char* units[unitCount] = { "s", "m", "h", "d" };
	static const int divisors[unitCount] = { 1, 60, 60, 24 };

	if ( seconds <= 0 )
		return "nada";

	int uptime[unitCount] = { seconds, 0, 0, 0 };
	for ( int i = 1; i < unitCount; ++i ) {
		uptime[i] = uptime[i-1] / divisors[i];
		uptime[i-1] -= uptime[i] * divisors[i];
	}

	result[0] = '\0';
	for ( int i = unitCount - 1; i >= 0; --i ) {
		if ( uptime[i] <= 0 )
			continue;

		Q_strcat( result, sizeof( result ), va( "%d%s", uptime[i], units[i] ) );
		if ( i > 0 && uptime[i-1] > 0 )
			Q_strcat( result, sizeof( result ), " " );
	}

	return result;
}


static void SV_Uptime_f()
{
	if (Sys_HasCNQ3Parent()) {
		const int parentTime = Sys_GetUptimeSeconds( qtrue );
		Com_Printf( "Parent process : %s\n", parentTime >= 0 ? SV_FormatUptime(parentTime) : "unknown" );
	}

	const int childTime = Sys_GetUptimeSeconds( qfalse );
	Com_Printf( "This process   : %s\n", childTime >= 0 ? SV_FormatUptime(childTime) : "unknown" );

	int mapTime = -1;
	if ( Cvar_VariableIntegerValue( "sv_running" ) )
		mapTime =  ( Sys_Milliseconds() - sv.mapLoadTime ) / 1000;
	Com_Printf( "Current map    : %s\n", mapTime >= 0 ? SV_FormatUptime(mapTime) : "no map loaded" );
}


static const cmdTableItem_t sv_cmds[] =
{
#if defined(DEBUG) || defined(CNQ3_DEV)
	{ "net_printoverhead", SV_PrintNetworkOverhead_f, NULL, "prints network overhead stats" },
	{ "net_clearoverhead", SV_ClearNetworkOverhead_f, NULL, "clears network overhead stats" },
#endif
	{ "heartbeat", SV_Heartbeat_f, NULL, "sends a heartbeat to master servers" },
	{ "kick", SV_Kick_f, NULL, "kicks a player by name" },
	{ "banUser", SV_Ban_f, NULL, "bans a player by name" },
	{ "banClient", SV_BanNum_f, NULL, "bans a player by client number" },
	{ "clientkick", SV_KickNum_f, NULL, "kicks a player by client number" },
	{ "status", SV_Status_f, NULL, "prints the current player list" },
	{ "serverinfo", SV_Serverinfo_f, NULL, "prints all server info cvars" },
	{ "systeminfo", SV_Systeminfo_f, NULL, "prints all system info cvars" },
	{ "dumpuser", SV_DumpUser_f, NULL, "prints a user's info cvars" },
	{ "map_restart", SV_MapRestart_f, NULL, "resets the game without reloading the map" },
	{ "sectorlist", SV_SectorList_f, NULL, "prints entity count for all sectors" },
	{ "map", SV_Map_f, SV_CompleteMap_f, "loads a map" },
	{ "devmap", SV_DevMap_f, SV_CompleteMap_f, "loads a map with cheats enabled" },
	{ "killserver", SV_KillServer_f, NULL, "shuts the server down" },
	{ "sv_restart", SV_ServerRestart_f, NULL, "restarts the server" },
	{ "sv_restartProcess", SV_RestartProcess_f, NULL, "restarts the server's child process" },
	{ "uptime", SV_Uptime_f, NULL, "prints the server's uptimes" }
};


void SV_AddOperatorCommands()
{
	static qbool initialized = qfalse;

	if ( initialized )
		return;

	initialized = qtrue;

	Cmd_RegisterArray( sv_cmds, MODULE_SERVER );

	if( com_dedicated->integer ) {
		Cmd_AddCommand( "say", SV_ConSay_f );
		Cmd_SetModule( "say", MODULE_SERVER );
	}
}


void SV_RemoveOperatorCommands()
{
	// removing these won't let the server start again
	//Cmd_UnregisterModule( MODULE_SERVER );
}


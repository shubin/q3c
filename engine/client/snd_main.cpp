/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.
Copyright (C) 2005 Stuart Dalton (badcdev@gmail.com)

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
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "client.h"
#include "client_help.h"
#include "snd_codec.h"
#include "snd_local.h"
#include "snd_public.h"

cvar_t *s_volume;
cvar_t *s_musicVolume;
cvar_t *s_initsound;
cvar_t *s_autoMute;

static soundInterface_t si;


static qbool S_ValidSoundInterface()
{
	if ( !si.Shutdown ) return qfalse;
	if ( !si.StartSound ) return qfalse;
	if ( !si.StartLocalSound ) return qfalse;
	if ( !si.StartBackgroundTrack ) return qfalse;
	if ( !si.StopBackgroundTrack ) return qfalse;
	if ( !si.RawSamples ) return qfalse;
	if ( !si.StopAllSounds ) return qfalse;
	if ( !si.ClearLoopingSounds ) return qfalse;
	if ( !si.AddLoopingSound ) return qfalse;
	if ( !si.Respatialize ) return qfalse;
	if ( !si.UpdateEntityPosition ) return qfalse;
	if ( !si.Update ) return qfalse;
	if ( !si.DisableSounds ) return qfalse;
	if ( !si.BeginRegistration ) return qfalse;
	if ( !si.RegisterSound ) return qfalse;
	if ( !si.ClearSoundBuffer ) return qfalse;
	if ( !si.SoundInfo ) return qfalse;
	if ( !si.SoundList ) return qfalse;

	return qtrue;
}


void S_StartSound( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx )
{
	if (si.StartSound)
		si.StartSound( origin, entnum, entchannel, sfx );
}


void S_StartLocalSound( sfxHandle_t sfx, int channelNum )
{
	if (si.StartLocalSound)
		si.StartLocalSound( sfx, channelNum );
}


void S_StartBackgroundTrack( const char* intro, const char* loop )
{
	if (si.StartBackgroundTrack)
		si.StartBackgroundTrack( intro, loop );
}


void S_StopBackgroundTrack()
{
	if (si.StopBackgroundTrack)
		si.StopBackgroundTrack();
}


void S_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume )
{
	if (si.RawSamples)
		si.RawSamples( samples, rate, width, channels, data, volume );
}


void S_StopAllSounds()
{
	if (si.StopAllSounds)
		si.StopAllSounds();
}


void S_ClearLoopingSounds()
{
	if (si.ClearLoopingSounds)
		si.ClearLoopingSounds();
}

#if defined( QC )
void S_AddLoopingSound(int entityNum, const vec3_t origin, const vec3_t velocity, sfxHandle_t sfx)
{
	if (si.AddLoopingSound)
		si.AddLoopingSound(entityNum, origin, velocity[0], sfx);
}
#else  // QC
void S_AddLoopingSound( int entityNum, const vec3_t origin, sfxHandle_t sfx )
{
	if (si.AddLoopingSound)
		si.AddLoopingSound( entityNum, origin, sfx );
}
#endif // QC


void S_Respatialize( int entityNum, const vec3_t origin, const vec3_t axis[3], int inwater )
{
	if (si.Respatialize)
		si.Respatialize( entityNum, origin, axis, inwater );
}


void S_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
	if (si.UpdateEntityPosition)
		si.UpdateEntityPosition( entityNum, origin );
}


void S_Update()
{
	if (si.Update)
		si.Update();
}


void S_DisableSounds()
{
	if (si.DisableSounds)
		si.DisableSounds();
}


void S_BeginRegistration()
{
	if (si.BeginRegistration)
		si.BeginRegistration();
}


sfxHandle_t S_RegisterSound( const char* sample )
{
	if (si.RegisterSound)
		return si.RegisterSound( sample );
	return 0;
}


void S_ClearSoundBuffer()
{
	if (si.ClearSoundBuffer)
		si.ClearSoundBuffer();
}


static void S_SoundInfo()
{
	if (si.SoundInfo)
		si.SoundInfo();
}


static void S_SoundList()
{
	if (si.SoundList)
		si.SoundList();
}


///////////////////////////////////////////////////////////////


static void S_Play_f( void )
{
	int		i;
	sfxHandle_t	h;
	char		name[256];

	if( !si.RegisterSound || !si.StartLocalSound ) {
		return;
	}

	i = 1;
	while ( i<Cmd_Argc() ) {
		if ( !Q_strrchr(Cmd_Argv(i), '.') ) {
			Com_sprintf( name, sizeof(name), "%s.wav", Cmd_Argv(1) );
		} else {
			Q_strncpyz( name, Cmd_Argv(i), sizeof(name) );
		}
		h = si.RegisterSound( name );
		if( h ) {
			si.StartLocalSound( h, CHAN_LOCAL_SOUND );
		}
		i++;
	}
}


static void S_Music_f( void )
{
	int		c;

	if( !si.StartBackgroundTrack ) {
		return;
	}

	c = Cmd_Argc();

	if ( c == 2 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), NULL );
	} else if ( c == 3 ) {
		si.StartBackgroundTrack( Cmd_Argv(1), Cmd_Argv(2) );
	} else {
		Com_Printf ("music <musicfile> [loopfile]\n");
		return;
	}
}


///////////////////////////////////////////////////////////////


static const cvarTableItem_t cl_cvars[] =
{
	{
		&s_volume, "s_volume", "0.2", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", "global sound volume",
		"Sound volume", CVARCAT_SOUND, "", ""
	},
	{
		&s_musicVolume, "s_musicvolume", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "16", "music volume",
		"Music volume", CVARCAT_SOUND, "", ""
	},
	{ &s_initsound, "s_initsound", "1", 0, CVART_BOOL, NULL, NULL, "enables the audio system" },
	{
		&s_autoMute, "s_autoMute", "1", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", help_s_autoMute,
		"Auto-mute", CVARCAT_SOUND, "Pick scenarios where audio output should be disabled", "",
		CVAR_GUI_VALUE("0", "Never", "")
		CVAR_GUI_VALUE("1", "Window is not focused", "")
		CVAR_GUI_VALUE("2", "Window is minimized", "")
	}
};


static const cmdTableItem_t cl_cmds[] =
{
	{ "play", S_Play_f, NULL, "starts local sound playback" },
	{ "music", S_Music_f, NULL, "starts music playback" },
	{ "s_list", S_SoundList, NULL, "lists loaded sounds" },
	{ "s_stop", S_StopAllSounds, NULL, "stops all sound playbacks" },
	{ "s_info", S_SoundInfo, NULL, "prints sound system info" }
};


void S_Init()
{
	QSUBSYSTEM_INIT_START( "Sound" );

	Cvar_RegisterArray( cl_cvars, MODULE_SOUND );

	if ( !s_initsound->integer ) {
		Com_Memset( &si, 0, sizeof(si) );
		Com_Printf( "Sound disabled.\n" );
		return;
	}

	S_CodecInit();

	Cmd_RegisterArray( cl_cmds, MODULE_SOUND );

	if (!S_Base_Init( &si )) {
		Com_Printf( "Sound initialization failed.\n" );
		return;
	}

	if ( !S_ValidSoundInterface() )
		Com_Error( ERR_FATAL, "Sound interface invalid." );

	S_SoundInfo();

	QSUBSYSTEM_INIT_DONE( "Sound" );
}


void S_Shutdown()
{
	if (si.Shutdown)
		si.Shutdown();

	Com_Memset( &si, 0, sizeof(si) );

	Cmd_UnregisterModule( MODULE_SOUND );

	S_CodecShutdown();
}


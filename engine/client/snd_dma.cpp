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
//
// main control for any streaming sound output device

#include "snd_local.h"
#include "snd_codec.h"
#include "client.h"


static void S_Update_DMA();
static void S_UpdateBackgroundTrack();
static void S_Base_StopBackgroundTrack();

static snd_stream_t* s_backgroundStream = NULL;
static char s_backgroundLoop[MAX_QPATH];


static const float SOUND_MAX_DIST = 1250;

static const int MASTER_VOL = 127;

channel_t s_channels[MAX_CHANNELS];
channel_t loop_channels[MAX_CHANNELS];
int numLoopChannels;

static qbool s_soundStarted;
static qbool s_soundMuted;

static int listener_number;
static vec3_t listener_origin;
static vec3_t listener_axis[3];

dma_t dma;
int s_soundtime;
int s_paintedtime;

#define MAX_SFX 4096
static sfx_t s_knownSfx[MAX_SFX];
static int s_numSfx;

#define SFX_HASH_SIZE 128
static sfx_t* sfxHash[SFX_HASH_SIZE];

static cvar_t* s_show;
static cvar_t* s_mixahead;
static cvar_t* s_mixPreStep;
cvar_t* s_testsound;
cvar_t *s_khz;

static loopSound_t		loopSounds[MAX_GENTITIES];
static channel_t		*freelist = NULL;

int						s_rawend;
portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

#if defined( _WIN32 )
#define S_KHZ_DEFAULT "22"
#else
#define S_KHZ_DEFAULT "44"
#endif


static void S_Base_SoundInfo()
{
	if (!s_soundStarted) {
		Com_Printf( "sound system not started\n" );
		return;
	}

	Com_Printf( "DMA: %d channels, %dHz, %d bits\n", dma.channels, dma.speed, dma.samplebits );
	//Com_Printf( "%5d samples\n", dma.samples );
	//Com_Printf( "%5d submission_chunk\n", dma.submission_chunk );
	//Com_Printf( "0x%x dma buffer\n", dma.buffer );
	if ( s_backgroundStream ) {
		Com_Printf( "Music: %s\n", s_backgroundLoop );
	}
}


static void S_Base_SoundList()
{
	const sfx_t* sfx = &s_knownSfx[1];
	for ( int i = 1; i < s_numSfx; ++i, ++sfx ) {
		Com_Printf( "%6i [%s] : %s\n", sfx->soundLength, sfx->inMemory ? "MEM" : "PGD", sfx->soundName );
	}
	Com_Printf( "%d sounds loaded\n", s_numSfx - 1 );

	S_DisplayFreeMemory();
}


static void S_ChannelFree( channel_t* v )
{
	v->thesfx = NULL;
	*(channel_t **)v = freelist;
	freelist = (channel_t*)v;
}

static channel_t* S_ChannelMalloc()
{
	channel_t *v;
	if (freelist == NULL) {
		return NULL;
	}
	v = freelist;
	freelist = *(channel_t **)freelist;
	v->allocTime = Com_Milliseconds();
	return v;
}

static void S_ChannelSetup()
{
	channel_t *p, *q;

	Com_Memset( s_channels, 0, sizeof( s_channels ) );

	p = s_channels;
	q = p + MAX_CHANNELS;
	while (--q > p) {
		*(channel_t **)q = q-1;
	}

	*(channel_t **)q = NULL;
	freelist = p + MAX_CHANNELS - 1;
	Com_DPrintf("Channel memory manager started\n");
}


///////////////////////////////////////////////////////////////


// will allocate a new sfx if it isn't found

static sfx_t* S_FindName( const char *name )
{
	int i;

	if (!name) {
		Com_Error( ERR_FATAL, "S_FindName: NULL" );
	}

	if (!name[0]) {
		Com_Error( ERR_FATAL, "S_FindName: empty name" );
	}

	if (strlen(name) >= MAX_QPATH) {
		Com_Error( ERR_FATAL, "S_FindName: name too long: %s", name );
	}

	int hash = Q_FileHash( name, SFX_HASH_SIZE );
	sfx_t* sfx = sfxHash[hash];

	// see if already loaded
	while (sfx) {
		if (!Q_stricmp(sfx->soundName, name) ) {
			return sfx;
		}
		sfx = sfx->next;
	}

	// find a free sfx
	for (i = 1; i < s_numSfx; ++i) {
		if (!s_knownSfx[i].soundName[0]) {
			break;
		}
	}

	if (i == s_numSfx) {
		if (s_numSfx == MAX_SFX) {
			Com_Error( ERR_FATAL, "S_FindName: out of sfx_t" );
		}
		s_numSfx++;
	}

	sfx = &s_knownSfx[i];
	Com_Memset( sfx, 0, sizeof(*sfx) );
	strcpy( sfx->soundName, name );

	sfx->next = sfxHash[hash];
	sfxHash[hash] = sfx;

	return sfx;
}


static void S_memoryLoad( sfx_t* sfx )
{
	// Unfortunately, sound and image loads during gameplay are perfectly valid.
	// Mod code really should pre-load as much as it can during CGAME_INIT.
	// Some stuff can't really be pre-loaded, such as when loading a new player model.
	if ( cls.state == CA_ACTIVE && com_developer && com_developer->integer ) {
		enum { SoundBufferSize = 16 };
		static sfx_t* sounds[SoundBufferSize];
		static int soundIndex = 0;
		static int64_t lastLoadTime = -9000;
		static int64_t lastMsgTime = -9000;
		static int loadCount = 1;

		sounds[soundIndex++] = sfx;

		const int64_t currTime = Sys_Milliseconds();
		if ( currTime < lastLoadTime + 1000 )
			++loadCount;
		else
			loadCount = 1;
		lastLoadTime = currTime;

		if ( loadCount > 2 && currTime >= lastMsgTime + 1000 ) {
			Com_Printf( "^3WARNING: excessive sound loads\n" );
			for ( int i = 0; i < SoundBufferSize; ++i ) {
				sfx_t* const sound = sounds[ ( soundIndex + i ) % SoundBufferSize ];
				if ( sound != NULL )
					Com_Printf( "^3- %s\n", sound->soundName );
			}
			lastMsgTime = currTime;
			loadCount = 1;
		}
	}

	if ( !S_LoadSound( sfx ) )
		sfx->defaultSound = qtrue;
	sfx->inMemory = qtrue;
}


static sfxHandle_t S_Base_RegisterSound( const char* name )
{
	if (!s_soundStarted) {
		return 0;
	}

	if ( name[0] == '\0' ) {
		if ( !cls.cgameNewDemoPlayer ) {
			Com_Printf( "Sound name is empty\n" );
		}
		return 0;
	}

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Sound name exceeds MAX_QPATH\n" );
		return 0;
	}

	sfx_t* sfx = S_FindName( name );
	if ( sfx->soundData ) {
		if ( sfx->defaultSound ) {
			Com_Printf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
			return 0;
		}
		return sfx - s_knownSfx;
	}

	sfx->inMemory = qfalse;
	S_memoryLoad( sfx );

	if ( sfx->defaultSound ) {
		Com_Printf( S_COLOR_YELLOW "WARNING: could not find %s - using default\n", sfx->soundName );
		return 0;
	}

	return sfx - s_knownSfx;
}


static void S_Base_BeginRegistration()
{
	s_soundMuted = qfalse;		// we can play again

	SND_setup();

	Com_Memset( s_knownSfx, 0, sizeof( s_knownSfx ) );
	Com_Memset( sfxHash, 0, sizeof(sfx_t*) * SFX_HASH_SIZE );

	// eat sound 0's slot or the first real sound registered will be lost forever
	s_numSfx = 1;
}


///////////////////////////////////////////////////////////////


// used for spatializing s_channels (duh, really?)

#if defined( QC )
static void S_SpatializeOrigin(const vec3_t origin, int master_vol, int max_dist, int* left_vol, int* right_vol)
#else  // QC
static void S_SpatializeOrigin( const vec3_t origin, int master_vol, int* left_vol, int* right_vol )
#endif // QC
{
	vec_t		dot;
	vec_t		dist;
	vec_t		lscale, rscale;
	vec3_t		source_vec;
	vec3_t		vec;

	// calculate stereo seperation and distance attenuation
	VectorSubtract( origin, listener_origin, source_vec );
	dist = VectorNormalize( source_vec );

#if defined( QC )
	if (max_dist == 0) {
		max_dist = SOUND_MAX_DIST;
	}

	if (dist >= max_dist) {
#else
	if (dist >= SOUND_MAX_DIST) {
#endif // QC
		*left_vol = *right_vol = 0;
		return;
	}

#if defined( QC )
	dist *= (1.0f / max_dist);
#else  // QC
	dist *= (1.0f / SOUND_MAX_DIST);
#endif // QC
	vec_t scale = master_vol * (1.0f - dist);

	// attenuate correctly even if we can't spatialise
	if (dma.channels == 1) {
		*left_vol = *right_vol = scale;
		return;
	}

	VectorRotate( source_vec, listener_axis, vec );
	dot = -vec[1];

	rscale = 0.5 * (1.0 + dot);
	lscale = 0.5 * (1.0 - dot);
	if ( rscale < 0 ) {
		rscale = 0;
	}
	if ( lscale < 0 ) {
		lscale = 0;
	}

	*right_vol = scale * rscale;
	if (*right_vol < 0)
		*right_vol = 0;

	*left_vol = scale * lscale;
	if (*left_vol < 0)
		*left_vol = 0;
}


/*
validate the parms and queue the sound up
if origin is NULL, the sound will be dynamically sourced from the entity

in theory, entchannel 0 (CHAN_AUTO) will not override a playing sound
unless it has to, and other channels will automatically override

in reality, the code doesn't actually bother
and EVERYTHING except CHAN_ANNOUNCER is treated as CHAN_AUTO

for a "correct" implementation that did actually honor channel numbers:

of the 7 per-entity channels:
LOCAL is in fact NEVER used
LOCAL_SOUND and ANNOUNCER are implicitly "this client only"
the remaining 4 are common to all player entities:
VOICE is used for pain, jump, death, and very little else
BODY is used exclusively for footsteps and (incorrectly) for holdable item use
ITEM is used exclusively for PU sounds (quad, suit, and regen)
WEAPON is used exclusively for firing sounds

everything else is just thrown at CHAN_AUTO, whether it's "right" or not
that includes things like water events, weapon changes, item pickups, and so on
and auto IS right for most of them, because e.g.
you can pick up two items in (much) less time than the wavs take to play
and it's critical that they DO both play (for us, at least - baseq3 is broken as fuck anyway)
or you could "hide" an armor pickup with an ammobox pickup, etc (doom bfg style :P)

so in fact, a "correct" implementation is actually VERY UNdesirable
not just because of the amount of correction the cgame would need, but conceptually too
*/

static void S_Base_StartSound( const vec3_t origin, int entityNum, int entchannel, sfxHandle_t sfxHandle )
{
	int i;
	channel_t* ch;

	if ( !s_soundStarted || s_soundMuted || !sfxHandle ) {
		return;
	}

	if ( !origin && ( entityNum < 0 || entityNum > MAX_GENTITIES ) ) {
		Com_Error( ERR_DROP, "S_StartSound: bad entitynum %i", entityNum );
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx_t* sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad(sfx);
	}

	if ( s_show->integer == 1 ) {
		Com_Printf( "%i : %s\n", s_paintedtime, sfx->soundName );
	}

	int time = Com_Milliseconds();
	sfx->lastTimeUsed = time;

//	Com_Printf("playing %s\n", sfx->soundName);

	// a UNIQUE entity starting the same sound twice in a frame is either a bug,
	// a timedemo, or a shitmap (eg q3ctf4) giving multiple items on spawn.
	// even if you can create a case where it IS "valid", it's still pointless
	// because you implicitly can't DISTINGUISH between the sounds:
	// all that happens is the sound plays at double volume, which is just annoying

	if ( entityNum != ENTITYNUM_WORLD ) {
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; ++i, ++ch ) {
			if ( (ch->entnum == entityNum) && (ch->allocTime == time) && (ch->thesfx == sfx) ) {
				//Com_Printf("double sound start: %d %s\n", entityNum, sfx->soundName);
				return;
			}
		}
	}

	ch = S_ChannelMalloc();

	if (!ch) {
		// realistically, this will only happen in timedemos,
		// and MAYBE on ubershitty maps (3W, CA, space) with massive PG spam etc
		// so it's really not a priority or even an interesting case
		channel_t* chOldest = NULL;

		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS; ++i, ++ch ) {
			if ( (ch->entnum == entityNum) && (ch->entchannel != CHAN_ANNOUNCER) ) {
				if ( !chOldest || (ch->allocTime < chOldest->allocTime) ) {
					chOldest = ch;
				}
			}
		}

		if (!chOldest) {
			// the entity itself has no reusable channels, so use the oldest "world" one
			// which will typically be ambient noise or something equally unimportant
			// like the tail end of an mg impact or whatever from several frames ago
			ch = s_channels;
			for ( i = 0; i < MAX_CHANNELS; ++i, ++ch ) {
				if ( ch->entnum == ENTITYNUM_WORLD ) {
					if ( !chOldest || (ch->allocTime < chOldest->allocTime) ) {
						chOldest = ch;
					}
				}
			}
		}

		if (!chOldest) {
			// no channels of ANY kind free or even reusable?! something is VERY wrong
			//Com_Printf( "dropping sound %s for entity %d\n", sfx->soundName, entityNum );
			return;
		}

		ch = chOldest;
		ch->allocTime = time;
	}

	if (origin) {
		VectorCopy( origin, ch->origin );
		ch->fixed_origin = qtrue;
	} else {
		ch->fixed_origin = qfalse;
	}

	ch->master_vol = MASTER_VOL;
	ch->entnum = entityNum;
	ch->thesfx = sfx;
	ch->startSample = START_SAMPLE_IMMEDIATE;
	ch->entchannel = entchannel;
	ch->leftvol = ch->master_vol;		// these will get calced at next spatialize
	ch->rightvol = ch->master_vol;		// unless the game isn't running
}


static void S_Base_StartLocalSound( sfxHandle_t sfxHandle, int channelNum )
{
	if ( !s_soundStarted || s_soundMuted || !sfxHandle ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_StartLocalSound: handle %i out of range\n", sfxHandle );
		return;
	}

	S_Base_StartSound( NULL, listener_number, channelNum, sfxHandle );
}


// if we are about to perform file access, clear the buffer
// so sound doesn't stutter

static void S_Base_ClearSoundBuffer()
{
	if (!s_soundStarted)
		return;

	// stop looping sounds
	Com_Memset(loopSounds, 0, MAX_GENTITIES*sizeof(loopSound_t));
	Com_Memset(loop_channels, 0, MAX_CHANNELS*sizeof(channel_t));
	numLoopChannels = 0;

	S_ChannelSetup();

	s_rawend = 0;

	Sys_S_BeginPainting();
	if (dma.buffer) {
		int clear = (dma.samplebits == 8) ? 0x80 : 0x00;
		Com_Memset( dma.buffer, clear, dma.samples * dma.samplebits/8 );
	}
	Sys_S_Submit();
}


static void S_Base_StopAllSounds()
{
	if ( !s_soundStarted ) {
		return;
	}

	// stop the background music
	S_Base_StopBackgroundTrack();

	S_Base_ClearSoundBuffer();
}


// disables sounds until the next S_BeginRegistration
// this is called when the hunk is cleared and the sounds are no longer valid

static void S_Base_DisableSounds()
{
	S_Base_StopAllSounds();
	s_soundMuted = qtrue;
}


///////////////////////////////////////////////////////////////

// continuous looping sounds are added each frame by cgame


static void S_Base_ClearLoopingSounds()
{
	for (int i = 0; i < MAX_GENTITIES; ++i)
		loopSounds[i].active = qfalse;
	numLoopChannels = 0;
}


// called during entity generation for a frame

#if defined( QC )
static void S_Base_AddLoopingSound(int entityNum, const vec3_t origin, int maxDist, sfxHandle_t sfxHandle)
#else  // QC
static void S_Base_AddLoopingSound( int entityNum, const vec3_t origin, sfxHandle_t sfxHandle )
#endif // QC
{
	if ( !s_soundStarted || s_soundMuted || !sfxHandle ) {
		return;
	}

	if ( sfxHandle < 0 || sfxHandle >= s_numSfx ) {
		Com_Printf( S_COLOR_YELLOW "S_AddLoopingSound: handle %i out of range\n", sfxHandle );
		return;
	}

	sfx_t* sfx = &s_knownSfx[ sfxHandle ];

	if (sfx->inMemory == qfalse) {
		S_memoryLoad( sfx );
	}

	if ( !sfx->soundLength ) {
		Com_Error( ERR_DROP, "%s has length 0", sfx->soundName );
	}

	VectorCopy( origin, loopSounds[entityNum].origin );
	loopSounds[entityNum].sfx = sfx;
	loopSounds[entityNum].active = qtrue;
#if defined( QC )
	loopSounds[entityNum].maxDist = maxDist;
#endif // QC
}


/*
Spatialize all of the looping (ie ambient) sounds.
All sounds are on the same cycle, so any duplicates can just
sum up the channel multipliers.
*/
static void S_AddLoopSounds()
{
	int			i, j, time;
	int			left_total, right_total, left, right;
	channel_t	*ch;
	loopSound_t	*loop, *loop2;
	static int	loopFrame;

	numLoopChannels = 0;

	time = Com_Milliseconds();

	loopFrame++;
	for ( i = 0; (i < MAX_GENTITIES) && (numLoopChannels < MAX_CHANNELS); ++i) {
		loop = &loopSounds[i];
		if ( !loop->active || loop->mergeFrame == loopFrame ) {
			continue;	// already merged into an earlier sound
		}
#if defined( QC )
		S_SpatializeOrigin(loop->origin, MASTER_VOL, loop->maxDist, &left_total, &right_total);
#else  // QC
		S_SpatializeOrigin( loop->origin, MASTER_VOL, &left_total, &right_total );
#endif // QC

		loop->sfx->lastTimeUsed = time;

		for (j=(i+1); j< MAX_GENTITIES ; j++) {
			loop2 = &loopSounds[j];
			if ( !loop2->active || (loop2->sfx != loop->sfx) ) {
				continue;
			}
			loop2->mergeFrame = loopFrame;

#if defined( QC )
			S_SpatializeOrigin(loop2->origin, MASTER_VOL, loop->maxDist, &left, &right);
#else  // QC
			S_SpatializeOrigin( loop2->origin, MASTER_VOL, &left, &right );
#endif // QC

			loop2->sfx->lastTimeUsed = time;
			left_total += left;
			right_total += right;
		}
		if (left_total == 0 && right_total == 0) {
			continue;		// not audible
		}

		// allocate a channel
		ch = &loop_channels[numLoopChannels];

		if (left_total > 255) {
			left_total = 255;
		}
		if (right_total > 255) {
			right_total = 255;
		}

		ch->master_vol = MASTER_VOL;
		ch->leftvol = left_total;
		ch->rightvol = right_total;
		ch->thesfx = loop->sfx;
		numLoopChannels++;
	}
}


///////////////////////////////////////////////////////////////


// music streaming

static void S_Base_RawSamples( int samples, int rate, int width, int channels, const byte *data, float volume )
{
	int		i;
	int		src, dst;
	float	scale;
	int		intVolume;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	intVolume = 256 * volume * s_volume->value;

	if ( s_rawend < s_soundtime ) {
		Com_DPrintf( "S_RawSamples: resetting minimum: %i < %i\n", s_rawend, s_soundtime );
		s_rawend = s_soundtime;
	}

	scale = (float)rate / dma.speed;

//Com_Printf ("%i < %i < %i\n", s_soundtime, s_paintedtime, s_rawend);
	if (channels == 2 && width == 2)
	{
		if (scale == 1.0)
		{	// optimized case
			for (i=0 ; i<samples ; i++)
			{
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left = ((short *)data)[i*2] * intVolume;
				s_rawsamples[dst].right = ((short *)data)[i*2+1] * intVolume;
			}
		}
		else
		{
			for (i=0 ; ; i++)
			{
				src = i*scale;
				if (src >= samples)
					break;
				dst = s_rawend&(MAX_RAW_SAMPLES-1);
				s_rawend++;
				s_rawsamples[dst].left = ((short *)data)[src*2] * intVolume;
				s_rawsamples[dst].right = ((short *)data)[src*2+1] * intVolume;
			}
		}
	}
	else if (channels == 1 && width == 2)
	{
		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = ((short *)data)[src] * intVolume;
			s_rawsamples[dst].right = ((short *)data)[src] * intVolume;
		}
	}
	else if (channels == 2 && width == 1)
	{
		intVolume *= 256;

		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = ((char *)data)[src*2] * intVolume;
			s_rawsamples[dst].right = ((char *)data)[src*2+1] * intVolume;
		}
	}
	else if (channels == 1 && width == 1)
	{
		intVolume *= 256;

		for (i=0 ; ; i++)
		{
			src = i*scale;
			if (src >= samples)
				break;
			dst = s_rawend&(MAX_RAW_SAMPLES-1);
			s_rawend++;
			s_rawsamples[dst].left = (((byte *)data)[src]-128) * intVolume;
			s_rawsamples[dst].right = (((byte *)data)[src]-128) * intVolume;
		}
	}

	if ( s_rawend > s_soundtime + MAX_RAW_SAMPLES ) {
		Com_DPrintf( "S_RawSamples: overflowed %i > %i\n", s_rawend, s_soundtime );
	}
}


///////////////////////////////////////////////////////////////


// let the sound system know where an entity currently is

static void S_Base_UpdateEntityPosition( int entityNum, const vec3_t origin )
{
	if ( entityNum < 0 || entityNum > MAX_GENTITIES ) {
		Com_Error( ERR_DROP, "S_UpdateEntityPosition: bad entitynum %i", entityNum );
	}
	VectorCopy( origin, loopSounds[entityNum].origin );
}


// change the volumes of all the playing sounds for changes in their positions

static void S_Base_Respatialize( int entityNum, const vec3_t head, const vec3_t axis[3], int inwater )
{
	vec3_t origin;
#if defined( QC )
	int maxdist;
#endif // QC

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	listener_number = entityNum;
	VectorCopy( head, listener_origin );
	VectorCopy( axis[0], listener_axis[0] );
	VectorCopy( axis[1], listener_axis[1] );
	VectorCopy( axis[2], listener_axis[2] );

	// update spatialization for dynamic sounds
	channel_t* ch = s_channels;
	for (int i = 0; i < MAX_CHANNELS; ++i, ++ch) {
		if ( !ch->thesfx ) {
			continue;
		}
		// anything coming from the view entity will always be full volume
		if (ch->entnum == listener_number) {
			ch->leftvol = ch->master_vol;
			ch->rightvol = ch->master_vol;
		} else {
			if (ch->fixed_origin) {
				VectorCopy( ch->origin, origin );
#if defined( QC )
				maxdist = SOUND_MAX_DIST;
#endif // QC
			} else {
				VectorCopy( loopSounds[ ch->entnum ].origin, origin );
#if defined( QC )
				maxdist = loopSounds[ ch->entnum ].maxDist;
#endif // QC
			}
#if defined( QC )
			S_SpatializeOrigin(origin, ch->master_vol, maxdist, &ch->leftvol, &ch->rightvol);
#else // QC
			S_SpatializeOrigin( origin, ch->master_vol, &ch->leftvol, &ch->rightvol );
#endif // QC
		}
	}

	S_AddLoopSounds();
}


// returns true if any new sounds were started since the last mix
// and clears out any expired sounds

static qbool S_ScanChannelStarts()
{
	qbool newSamples = qfalse;

	channel_t* ch = s_channels;
	for (int i = 0; i < MAX_CHANNELS; ++i, ++ch) {
		if ( !ch->thesfx ) {
			continue;
		}

		// if this channel was just started this frame,
		// set the sample count so it begins mixing
		// into the very first sample
		if ( ch->startSample == START_SAMPLE_IMMEDIATE ) {
			ch->startSample = s_paintedtime;
			newSamples = qtrue;
			continue;
		}

		// if it is completely finished by now, clear it
		if ( ch->startSample + ch->thesfx->soundLength <= s_paintedtime ) {
			S_ChannelFree(ch);
		}
	}

	return newSamples;
}


// called once each time through the main loop

static void S_Base_Update()
{
	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	if ( s_show->integer == 2 ) {
		int total = 0;
		const channel_t* ch = s_channels;
		for (int i = 0; i < MAX_CHANNELS; ++i, ++ch) {
			if (ch->thesfx && (ch->leftvol || ch->rightvol) ) {
				Com_Printf( "%3i %3i %s\n", ch->leftvol, ch->rightvol, ch->thesfx->soundName );
				total++;
			}
		}
		//Com_Printf( "----(%i)---- painted: %i\n", total, s_paintedtime );
	}

	// add raw data from streamed samples
	S_UpdateBackgroundTrack();

	// mix some sound
	S_Update_DMA();
}


static void S_GetSoundtime()
{
	static int buffers;
	static int oldsamplepos;

	int fullsamples = dma.samples / dma.channels;

	if ( CL_VideoRecording() ) {
		s_soundtime += (int)ceil( dma.speed / cl_aviFrameRate->value );
		return;
	}

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to S_Update.  Oh well.
	int samplepos = Sys_S_GetDMAPos();
	if (samplepos < oldsamplepos) {
		buffers++;	// buffer wrapped
		if (s_paintedtime > 0x40000000) {
			// time to chop things off to avoid 32 bit limits
			buffers = 0;
			s_paintedtime = fullsamples;
			S_Base_StopAllSounds();
		}
	}
	oldsamplepos = samplepos;

	s_soundtime = buffers*fullsamples + samplepos/dma.channels;

#if 0
	// check to make sure that we haven't overshot
	if (s_paintedtime < s_soundtime) {
		Com_DPrintf( "S_GetSoundtime : overflow\n" );
		s_paintedtime = s_soundtime;
	}
#endif

	if ( dma.submission_chunk < 256 ) {
		/* NOTE: this is SO wrong - it SHOULD just be s_soundtime
		because this is essentially lying and saying it's filled a piece of the DMA
		that it absolutely hasn't, so that piece is only valid because of mixahead
		but for some insane reason, NOT doing this causes garbled sound on SOME machines
		(even though the buffer data is still valid because of the previous frame's overfill)
		so i'm at a loss to explain how this could possibly be required, other than
		"something ELSE in the sound code is fucked up in a complementary way", or it's
		a driver problem. either way, i can't repro it so we're just stuck with the hack
		*/
		s_paintedtime = s_soundtime + s_mixPreStep->value * dma.speed;
	} else {
		s_paintedtime = s_soundtime + dma.submission_chunk;
	}
}


static void S_Update_DMA()
{
	static int prevSoundtime = -1;

	if ( !s_soundStarted || s_soundMuted ) {
		return;
	}

	S_GetSoundtime();

	if (s_soundtime == prevSoundtime) {
		return;
	}
	prevSoundtime = s_soundtime;

	// clear any sound effects that end before the current time,
	// and start any new sounds
	S_ScanChannelStarts();

	// mix ahead of current position
	unsigned endtime = s_soundtime + s_mixahead->value * dma.speed;

	// mix to an even submission block size
	endtime = (endtime + dma.submission_chunk-1) & ~(dma.submission_chunk-1);

	// never mix more than the complete buffer
	int samps = dma.samples >> (dma.channels-1);
	if (endtime - s_soundtime > samps)
		endtime = s_soundtime + samps;

	Sys_S_BeginPainting();

	S_PaintChannels( endtime );

	Sys_S_Submit();
}


/*
===============================================================================

background music functions

===============================================================================
*/


static void S_Base_StopBackgroundTrack()
{
	if(!s_backgroundStream)
		return;
	S_CodecCloseStream(s_backgroundStream);
	s_backgroundStream = NULL;
	s_rawend = 0;
}


static void S_Base_StartBackgroundTrack( const char *intro, const char *loop )
{
	if ( !intro ) {
		intro = "";
	}
	if ( !loop || !loop[0] ) {
		loop = intro;
	}
	Com_DPrintf( "S_StartBackgroundTrack( %s, %s )\n", intro, loop );

	if ( !intro[0] ) {
		return;
	}

	if( !loop ) {
		s_backgroundLoop[0] = 0;
	} else {
		Q_strncpyz( s_backgroundLoop, loop, sizeof( s_backgroundLoop ) );
	}

	// close the background track, but DON'T reset s_rawend
	// if restarting the same background track
	if (s_backgroundStream) {
		S_CodecCloseStream(s_backgroundStream);
		s_backgroundStream = NULL;
	}

	// Open stream
	s_backgroundStream = S_CodecOpenStream(intro);
	if (!s_backgroundStream) {
		Com_Printf( S_COLOR_YELLOW "WARNING: couldn't open music file %s\n", intro );
		return;
	}

	if (s_backgroundStream->info.channels != 2 || s_backgroundStream->info.rate != 22050) {
		Com_Printf(S_COLOR_YELLOW "WARNING: music file %s is not 22k stereo\n", intro );
	}
}


static void S_UpdateBackgroundTrack()
{
	static float musicVolume = 0;

	int		bufferSamples;
	int		fileSamples;
	byte	raw[30000];		// just enough to fit in a mac stack frame
	int		fileBytes;
	int		r;

	if (!s_backgroundStream) {
		return;
	}

	// fade music in or out if the volume changes (mainly for to/from 0)
	musicVolume = (musicVolume + (s_musicVolume->value * 2)) / 4.0f;

	// don't bother playing anything if musicvolume is 0
	if ( musicVolume <= 0 ) {
		return;
	}

	// see how many samples should be copied into the raw buffer
	if ( s_rawend < s_soundtime ) {
		s_rawend = s_soundtime;
	}

	while ( s_rawend < s_soundtime + MAX_RAW_SAMPLES ) {
		bufferSamples = MAX_RAW_SAMPLES - (s_rawend - s_soundtime);

		// decide how much data needs to be read from the file
		fileSamples = bufferSamples * s_backgroundStream->info.rate / dma.speed;

		// fileSamples can become 0 when dma.speed > s_backgroundStream->info.rate
		if ( fileSamples <= 0 ) {
			break;
		}

		// our max buffer size
		fileBytes = fileSamples * (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		if ( fileBytes > sizeof(raw) ) {
			fileBytes = sizeof(raw);
			fileSamples = fileBytes / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		}

		r = S_CodecReadStream( s_backgroundStream, fileBytes, raw );
		if (r < fileBytes) {
			fileBytes = r;
			fileSamples = r / (s_backgroundStream->info.width * s_backgroundStream->info.channels);
		}

		if(r > 0)
		{
			// add to raw buffer
			S_Base_RawSamples( fileSamples, s_backgroundStream->info.rate,
				s_backgroundStream->info.width, s_backgroundStream->info.channels, raw, musicVolume );
		}
		else
		{
			// loop
			if(s_backgroundLoop[0])
			{
				S_CodecCloseStream(s_backgroundStream);
				s_backgroundStream = NULL;
				S_Base_StartBackgroundTrack( s_backgroundLoop, s_backgroundLoop );
				if(!s_backgroundStream)
					return;
			}
			else
			{
				S_Base_StopBackgroundTrack();
				return;
			}
		}

	}
}


qbool S_FreeOldestSound()
{
	sfx_t* sfx;
	int oldest = Com_Milliseconds();
	int slot = 0;

	for (int i = 1; i < s_numSfx; ++i) {
		sfx = &s_knownSfx[i];
		if (sfx->inMemory && sfx->lastTimeUsed < oldest) {
			slot = i;
			oldest = sfx->lastTimeUsed;
		}
	}

	if (slot == 0)
		return qfalse;

	sfx = &s_knownSfx[slot];

	Com_DPrintf( "S_FreeOldestSound: freeing sound %s\n", sfx->soundName );

	sndBuffer* buffer = sfx->soundData;
	while (buffer) {
		sndBuffer* next = buffer->next;
		SND_free(buffer);
		buffer = next;
	}

	sfx->inMemory = qfalse;
	sfx->soundData = NULL;

	return qtrue;
}


static void S_Base_Shutdown()
{
	if ( !s_soundStarted ) {
		return;
	}

	Sys_S_Shutdown();

	s_soundStarted = qfalse;

	Cmd_RemoveCommand( "s_info" );
}


static const cvarTableItem_t cl_cvars[] =
{
	{ &s_mixahead, "s_mixahead", "0.2", CVAR_ARCHIVE, CVART_FLOAT },
	{ &s_mixPreStep, "s_mixPreStep", "0.05", CVAR_ARCHIVE, CVART_FLOAT },
	{ &s_show, "s_show", "0", CVAR_CHEAT, CVART_INTEGER, "0", "2" },
	{ &s_testsound, "s_testsound", "0", CVAR_CHEAT, CVART_BOOL },
	{ &s_khz, "s_khz", S_KHZ_DEFAULT, CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "22", "44" }
};


qbool S_Base_Init( soundInterface_t *si )
{
	Com_Memset( si, 0, sizeof(*si) );

	Cvar_RegisterArray( cl_cvars, MODULE_SOUND );

	if (!Sys_S_Init())
		return qfalse;

	s_soundStarted = qtrue;
	s_soundMuted = qtrue;
	s_numSfx = 0;

	Com_Memset( sfxHash, 0, sizeof(sfx_t *)*SFX_HASH_SIZE );

	s_soundtime = 0;
	s_paintedtime = 0;

	si->Shutdown = S_Base_Shutdown;
	si->StartSound = S_Base_StartSound;
	si->StartLocalSound = S_Base_StartLocalSound;
	si->StartBackgroundTrack = S_Base_StartBackgroundTrack;
	si->StopBackgroundTrack = S_Base_StopBackgroundTrack;
	si->RawSamples = S_Base_RawSamples;
	si->StopAllSounds = S_Base_StopAllSounds;
	si->ClearLoopingSounds = S_Base_ClearLoopingSounds;
	si->AddLoopingSound = S_Base_AddLoopingSound;
	si->Respatialize = S_Base_Respatialize;
	si->UpdateEntityPosition = S_Base_UpdateEntityPosition;
	si->Update = S_Base_Update;
	si->DisableSounds = S_Base_DisableSounds;
	si->BeginRegistration = S_Base_BeginRegistration;
	si->RegisterSound = S_Base_RegisterSound;
	si->ClearSoundBuffer = S_Base_ClearSoundBuffer;
	si->SoundInfo = S_Base_SoundInfo;
	si->SoundList = S_Base_SoundList;

	S_Base_StopAllSounds();

	return qtrue;
}

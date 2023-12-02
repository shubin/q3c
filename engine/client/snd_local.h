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
// snd_local.h -- private sound definations


#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_public.h"

#define	PAINTBUFFER_SIZE		4096					// this is in samples

#define SND_CHUNK_SIZE			1024					// samples

typedef struct {
	int			left;	// the final values will be clamped to +/- 0x00ffff00 and shifted down
	int			right;
} portable_samplepair_t;

typedef	struct sndBuffer_s {
	short					sndChunk[SND_CHUNK_SIZE];
	struct sndBuffer_s		*next;
	int						size;
} sndBuffer;

typedef struct sfx_s {
	sndBuffer		*soundData;
	qbool			defaultSound;	// couldn't be loaded, so use buzz
	qbool			inMemory;
	int				soundLength;
	char			soundName[MAX_QPATH];
	int				lastTimeUsed;
	struct sfx_s	*next;
} sfx_t;

typedef struct {
	int			channels;
	int			samples;				// mono samples in buffer
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			speed;
	byte		*buffer;
} dma_t;

#define START_SAMPLE_IMMEDIATE	0x7fffffff

typedef struct {
	vec3_t		origin;
	sfx_t		*sfx;
	int			mergeFrame;
	qbool		active;
#if defined( QC )
	int			maxDist;
#endif // QC
} loopSound_t;

typedef struct
{
	int			allocTime;
	int			startSample;	// START_SAMPLE_IMMEDIATE = set immediately on next mix
	int			entnum;			// to allow overriding a specific sound
	int			entchannel;		// to allow overriding a specific sound
	int			leftvol;		// 0-255 volume after spatialization
	int			rightvol;		// 0-255 volume after spatialization
	int			master_vol;		// 0-255 volume before spatialization
	vec3_t		origin;			// only use if fixed_origin is set
	qbool		fixed_origin;	// use origin instead of fetching entnum's origin
	const sfx_t* thesfx;		// sfx structure
} channel_t;


#define	WAV_FORMAT_PCM		1


// Interface between Q3 sound "api" and the sound backend
typedef struct
{
	void (*Shutdown)();
	void (*StartSound)( const vec3_t origin, int entnum, int entchannel, sfxHandle_t sfx );
	void (*StartLocalSound)( sfxHandle_t sfx, int channelNum );
	void (*StartBackgroundTrack)( const char *intro, const char *loop );
	void (*StopBackgroundTrack)();
	void (*RawSamples)( int samples, int rate, int width, int channels, const byte *data, float volume );
	void (*StopAllSounds)();
	void (*ClearLoopingSounds)();
#if defined( QC )
	void (*AddLoopingSound)(int entityNum, const vec3_t origin, int maxDist, sfxHandle_t sfx);
#else  // QC
	void (*AddLoopingSound)( int entityNum, const vec3_t origin, sfxHandle_t sfx );
#endif // QC
	void (*Respatialize)( int entityNum, const vec3_t origin, const vec3_t axis[3], int inwater );
	void (*UpdateEntityPosition)( int entityNum, const vec3_t origin );
	void (*Update)();
	void (*DisableSounds)();
	void (*BeginRegistration)();
	sfxHandle_t (*RegisterSound)( const char* sample );
	void (*ClearSoundBuffer)();
	void (*SoundInfo)();
	void (*SoundList)();
} soundInterface_t;


/*
====================================================================

  SYSTEM SPECIFIC FUNCTIONS

====================================================================
*/

qbool	Sys_S_Init();
void	Sys_S_Shutdown();

// Returns the current sample position (in mono samples read)
// inside the DMA ring buffer so the mixing code knows
// how many sample are required to fill it up.
int		Sys_S_GetDMAPos();

// Makes sure dma.buffer is valid and accessible.
// If necessary, acquires a lock.
void	Sys_S_BeginPainting();

// Sends sound to device if dma.buffer isn't really the DMA buffer.
// If a lock was acquired by Sys_S_BeginPainting,
// Sys_S_Submit will release it.
void	Sys_S_Submit();


#define	MAX_CHANNELS			96

extern	channel_t   s_channels[MAX_CHANNELS];
extern	channel_t   loop_channels[MAX_CHANNELS];
extern	int		numLoopChannels;

extern	int		s_paintedtime;
extern	int		s_rawend;
extern	dma_t	dma;

#define	MAX_RAW_SAMPLES	16384
extern	portable_samplepair_t	s_rawsamples[MAX_RAW_SAMPLES];

extern cvar_t *s_volume;
extern cvar_t *s_musicVolume;
extern cvar_t *s_testsound;
extern cvar_t *s_khz;

qbool S_LoadSound( sfx_t* sfx );

void		SND_free(sndBuffer *v);
sndBuffer*	SND_malloc();
void		SND_setup();

void S_PaintChannels(int endtime);

qbool S_FreeOldestSound();

qbool S_Base_Init( soundInterface_t *si );

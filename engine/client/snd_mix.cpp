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
// snd_mix.c -- portable code to mix sounds for snd_dma.c

#include "client.h"
#include "snd_local.h"

static portable_samplepair_t paintbuffer[PAINTBUFFER_SIZE];
static int snd_vol;

static int* snd_p;
static int snd_linear_count;
static short* snd_out;


static void S_WriteLinearBlastStereo16()
{
	int v1, v2;

	for (int i = 0; i < snd_linear_count; i += 2)
	{
		v1 = snd_p[i] >> 8;
		if (v1 > 32767)
			v1 = 32767;
		else if (v1 < -32768)
			v1 = -32768;

		v2 = snd_p[i+1] >> 8;
		if (v2 > 32767)
			v2 = 32767;
		else if (v2 < -32768)
			v2 = -32768;

		*(uint32_t*)(&snd_out[i]) = (v2 << 16) | (v1 & 0xFFFF);
	}
}


static void S_TransferStereo16( unsigned long* pbuf, int endtime )
{
	int		lpos;
	int		ls_paintedtime;

	snd_p = (int *) paintbuffer;
	ls_paintedtime = s_paintedtime;

	while (ls_paintedtime < endtime)
	{
		// handle recirculating buffer issues
		lpos = ls_paintedtime & ((dma.samples>>1)-1);

		snd_out = (short *) pbuf + (lpos<<1);

		snd_linear_count = (dma.samples>>1) - lpos;
		if (ls_paintedtime + snd_linear_count > endtime)
			snd_linear_count = endtime - ls_paintedtime;

		snd_linear_count <<= 1;

		// write a linear blast of samples
		S_WriteLinearBlastStereo16();

		snd_p += snd_linear_count;
		ls_paintedtime += (snd_linear_count>>1);

		if ( CL_VideoRecording() ) {
			CL_WriteAVIAudioFrame( (byte *)snd_out, snd_linear_count << 1 );
		}
	}
}


static void S_TransferPaintBuffer( int endtime )
{
	int		out_idx;
	int		count;
	int		out_mask;
	int		*p;
	int		step;
	int		val;
	unsigned long *pbuf;

	pbuf = (unsigned long *)dma.buffer;


	if ( s_testsound->integer ) {
		int		i;

		// write a fixed sine wave
		count = (endtime - s_paintedtime);
		for (i=0 ; i<count ; i++)
			paintbuffer[i].left = paintbuffer[i].right = sin((s_paintedtime+i)*0.1)*20000*256;
	}


	if (dma.samplebits == 16 && dma.channels == 2)
	{	// optimized case
		S_TransferStereo16 (pbuf, endtime);
	}
	else
	{	// general case
		p = (int *) paintbuffer;
		count = (endtime - s_paintedtime) * dma.channels;
		out_mask = dma.samples - 1; 
		out_idx = s_paintedtime * dma.channels & out_mask;
		step = 3 - dma.channels;

		if (dma.samplebits == 16)
		{
			short *out = (short *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < -32768)
					val = -32768;
				out[out_idx] = val;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
		else if (dma.samplebits == 8)
		{
			unsigned char *out = (unsigned char *) pbuf;
			while (count--)
			{
				val = *p >> 8;
				p+= step;
				if (val > 0x7fff)
					val = 0x7fff;
				else if (val < -32768)
					val = -32768;
				out[out_idx] = (val>>8) + 128;
				out_idx = (out_idx + 1) & out_mask;
			}
		}
	}
}


/*
===============================================================================

CHANNEL MIXING

===============================================================================
*/


static void S_PaintChannelFrom16( channel_t* ch, const sfx_t* sfx, int count, int sampleOffset, int bufferOffset )
{
	portable_samplepair_t* samp = &paintbuffer[ bufferOffset ];
	const sndBuffer* chunk = sfx->soundData;

	while (sampleOffset >= SND_CHUNK_SIZE) {
		chunk = chunk->next;
		sampleOffset -= SND_CHUNK_SIZE;
		if (!chunk) {
			chunk = sfx->soundData;
		}
	}

	int leftvol = ch->leftvol * snd_vol;
	int rightvol = ch->rightvol * snd_vol;
	const short* samples = chunk->sndChunk;

	for (int i = 0; i < count; ++i) {
		int data = samples[sampleOffset++];
		samp[i].left += (data * leftvol) >> 8;
		samp[i].right += (data * rightvol) >> 8;

		if (sampleOffset == SND_CHUNK_SIZE) {
			chunk = chunk->next;
			samples = chunk->sndChunk;
			sampleOffset = 0;
		}
	}
}


/*
===================
S_PaintChannels
===================
*/
void S_PaintChannels( int endtime ) {
	int 	i;
	int 	end;
	channel_t *ch;
	const sfx_t* sfx;
	int		ltime, count;
	int		sampleOffset;


	snd_vol = s_volume->value*255;

//Com_Printf ("%i to %i\n", s_paintedtime, endtime);
	while ( s_paintedtime < endtime ) {
		// if paintbuffer is smaller than DMA buffer
		// we may need to fill it multiple times
		end = endtime;
		if ( endtime - s_paintedtime > PAINTBUFFER_SIZE ) {
			end = s_paintedtime + PAINTBUFFER_SIZE;
		}

		// clear the paint buffer to either music or zeros
		if ( s_rawend < s_paintedtime ) {
			if ( s_rawend ) {
				//Com_DPrintf ("background sound underrun\n");
			}
			Com_Memset(paintbuffer, 0, (end - s_paintedtime) * sizeof(portable_samplepair_t));
		} else {
			// copy from the streaming sound source
			int		s;
			int		stop;

			stop = (end < s_rawend) ? end : s_rawend;

			for ( i = s_paintedtime ; i < stop ; i++ ) {
				s = i&(MAX_RAW_SAMPLES-1);
				paintbuffer[i-s_paintedtime] = s_rawsamples[s];
			}
//		if (i != end)
//			Com_Printf ("partial stream\n");
//		else
//			Com_Printf ("full stream\n");
			for ( ; i < end ; i++ ) {
				paintbuffer[i-s_paintedtime].left =
				paintbuffer[i-s_paintedtime].right = 0;
			}
		}

		// paint in the channels.
		ch = s_channels;
		for ( i = 0; i < MAX_CHANNELS ; i++, ch++ ) {
			if ( !ch->thesfx || (!ch->leftvol && !ch->rightvol) ) {
				continue;
			}

			ltime = s_paintedtime;
			sfx = ch->thesfx;

			if (sfx->soundData==NULL || sfx->soundLength==0) {
				continue;
			}
			sampleOffset = ltime - ch->startSample;
			count = end - ltime;
			if ( sampleOffset + count > sfx->soundLength ) {
				count = sfx->soundLength - sampleOffset;
			}

			if ( count > 0 ) {
				S_PaintChannelFrom16( ch, sfx, count, sampleOffset, ltime - s_paintedtime );
			}
		}

		// paint in the looped channels.
		ch = loop_channels;
		for ( i = 0; i < numLoopChannels ; i++, ch++ ) {
			if ( !ch->thesfx || (!ch->leftvol && !ch->rightvol) ) {
				continue;
			}

			ltime = s_paintedtime;
			sfx = ch->thesfx;

			if (sfx->soundData==NULL || sfx->soundLength==0) {
				continue;
			}
			// we might have to make two passes if it
			// is a looping sound effect and the end of
			// the sample is hit
			do {
				sampleOffset = (ltime % sfx->soundLength);

				count = end - ltime;
				if ( sampleOffset + count > sfx->soundLength ) {
					count = sfx->soundLength - sampleOffset;
				}

				if ( count > 0 ) {
					S_PaintChannelFrom16( ch, sfx, count, sampleOffset, ltime - s_paintedtime );
					ltime += count;
				}
			} while ( ltime < end);
		}

		// transfer out according to DMA format
		S_TransferPaintBuffer( end );
		s_paintedtime = end;
	}
}

/*
===========================================================================
Copyright (C) 2017-2019 Gian 'myT' Schellenbaum

This file is part of Challenge Quake 3 (CNQ3).

Challenge Quake 3 is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Challenge Quake 3 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Challenge Quake 3. If not, see <https://www.gnu.org/licenses/>.
===========================================================================
*/
// Linux audio using SDL 2

#include "../qcommon/q_shared.h"
#include "../client/snd_local.h"
#include <SDL2/SDL.h>


// @TODO: cvar for samples?
// @TODO: cvar for the device name? ("alsa", "pulseaudio", etc)
static const int bits = 16;
static const int channels = 2;
static const int samples = 2048;
static const SDL_AudioFormat format = AUDIO_S16SYS;

struct audio_t {
	qbool valid;
	int q3SamplePos;
	int q3Bytes;
	SDL_AudioDeviceID device;
};

static audio_t audio;


static void FillAudioBufferCallback( void* userData, Uint8* sdlBuffer, int sdlBytesToWrite )
{
	if (sdlBuffer == NULL || sdlBytesToWrite == 0)
		return;

	if (!audio.valid) {
		memset(sdlBuffer, 0, sdlBytesToWrite);
		return;
	}

	// fix up sample offset if needed
	const int bytesPerSample = dma.samplebits / 8;
	int q3BytePos = audio.q3SamplePos * bytesPerSample;
	if (q3BytePos >= audio.q3Bytes) {
		q3BytePos = 0;
		audio.q3SamplePos = 0;
	}

	// compute the sizes for the memcpy call(s)
	int q3BytesToEnd = audio.q3Bytes - q3BytePos;
	int bytes1 = sdlBytesToWrite;
	int bytes2 = 0;
	if (bytes1 > q3BytesToEnd) {
		bytes1 = q3BytesToEnd;
		bytes2 = sdlBytesToWrite - q3BytesToEnd;
	}

	// copy the new mixed data to the device
	memcpy(sdlBuffer, dma.buffer + q3BytePos, bytes1);
	if (bytes2 > 0) {
		memcpy(sdlBuffer + bytes1, dma.buffer, bytes2);
		audio.q3SamplePos = bytes2 / bytesPerSample;
	} else {
		audio.q3SamplePos += bytes1 / bytesPerSample;
	}

	// fix up sample offset if needed
	if (audio.q3SamplePos * bytesPerSample >= audio.q3Bytes)
		audio.q3SamplePos = 0;
}


qbool Sys_S_Init()
{
	if (audio.valid)
		return qtrue;

	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		Com_Printf("SDL_Init failed: %s\n", SDL_GetError());
		return qfalse;
	}

	// open the default audio device
	SDL_AudioSpec desired;
	memset(&desired, 0, sizeof(desired));
	desired.freq = s_khz->integer == 44 ? 44100 : 22050;
	desired.format = format;
	desired.samples = samples;
	desired.channels = channels;
	desired.callback = &FillAudioBufferCallback;
	SDL_AudioSpec obtained;
	memset(&obtained, 0, sizeof(obtained));
	audio.device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, 0);
	if (audio.device == 0) {
		Com_Printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
		Sys_S_Shutdown();
		return qfalse;
	}

	// save all the data we need to
	const int q3Samples = obtained.samples * 16;
	audio.q3SamplePos = 0;
	dma.samplebits = obtained.format & 0xFF;
	dma.channels = obtained.channels;
	dma.samples = q3Samples;
	dma.submission_chunk = 1;
	dma.speed = obtained.freq;
	audio.q3Bytes = dma.samples * (dma.samplebits / 8);
	dma.buffer = (byte*)calloc(1, audio.q3Bytes);
	audio.valid = qtrue;

	// opened devices are always paused by default
	SDL_PauseAudioDevice(audio.device, 0);

	return qtrue;
}


int Sys_S_GetDMAPos()
{
	if (!audio.valid)
		return 0;

	return audio.q3SamplePos;
}


void Sys_S_Shutdown()
{
	if (audio.device != 0) {
		SDL_PauseAudioDevice(audio.device, 1);
		SDL_CloseAudioDevice(audio.device);
		audio.device = 0;
	}

	SDL_QuitSubSystem(SDL_INIT_AUDIO);
	free(dma.buffer);
	dma.buffer = NULL;
	audio.q3SamplePos = 0;
	audio.q3Bytes = 0;
	audio.valid = qfalse;
}


void Sys_S_Submit()
{
	if (!audio.valid)
		return;

	// let SDL call our registered callback function again
	SDL_UnlockAudioDevice(audio.device);
}


void Sys_S_BeginPainting()
{
	if (!audio.valid)
		return;

	// prevent SDL from calling our registered callback function
	SDL_LockAudioDevice(audio.device);
}


void sdl_MuteAudio( qbool mute )
{
	if (!audio.valid)
		return;

	const qbool playing = SDL_GetAudioDeviceStatus(audio.device) == SDL_AUDIO_PLAYING;
	if (mute && playing) {
		SDL_PauseAudioDevice(audio.device, 1);
	} else if (!mute && !playing) {
		S_ClearSoundBuffer();
		SDL_PauseAudioDevice(audio.device, 0);
	}
}

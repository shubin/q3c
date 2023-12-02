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

#include "../client/snd_local.h"
#include "win_local.h"

#include <dsound.h>
#pragma comment( lib, "dxguid" )

HRESULT (WINAPI *pDirectSoundCreate)(GUID FAR *lpGUID, LPDIRECTSOUND FAR *lplpDS, IUnknown FAR *pUnkOuter);
#define iDirectSoundCreate(a,b,c)	pDirectSoundCreate(a,b,c)

#define SECONDARY_BUFFER_SIZE	0x10000


static int		sample16;
static DWORD	gSndBufSize;
static DWORD	locksize;
static LPDIRECTSOUND pDS;
static LPDIRECTSOUNDBUFFER pDSBuf, pDSPBuf;
static HINSTANCE hInstDS;


static const char *DSoundError( int error ) {
	switch ( error ) {
	case DSERR_BUFFERLOST:
		return "DSERR_BUFFERLOST";
	case DSERR_INVALIDCALL:
		return "DSERR_INVALIDCALLS";
	case DSERR_INVALIDPARAM:
		return "DSERR_INVALIDPARAM";
	case DSERR_PRIOLEVELNEEDED:
		return "DSERR_PRIOLEVELNEEDED";
	}

	return "unknown";
}


void Sys_S_Shutdown( void ) {
	Com_DPrintf( "Shutting down sound system\n" );

	if ( pDS ) {
		Com_DPrintf( "Destroying DS buffers\n" );
		if ( pDS )
		{
			Com_DPrintf( "...setting NORMAL coop level\n" );
			pDS->SetCooperativeLevel( g_wv.hWnd, DSSCL_PRIORITY );
		}

		if ( pDSBuf )
		{
			Com_DPrintf( "...stopping and releasing sound buffer\n" );
			pDSBuf->Stop();
			pDSBuf->Release();
		}

		// only release primary buffer if it's not also the mixing buffer we just released
		if ( pDSPBuf && ( pDSBuf != pDSPBuf ) )
		{
			Com_DPrintf( "...releasing primary buffer\n" );
			pDSPBuf->Release();
		}
		pDSBuf = NULL;
		pDSPBuf = NULL;

		dma.buffer = NULL;

		Com_DPrintf( "...releasing DS object\n" );
		pDS->Release();
	}

	if ( hInstDS ) {
		Com_DPrintf( "...freeing DSOUND.DLL\n" );
		FreeLibrary( hInstDS );
		hInstDS = NULL;
	}

	pDS = NULL;
	pDSBuf = NULL;
	pDSPBuf = NULL;
	memset ((void *)&dma, 0, sizeof (dma));
	CoUninitialize( );
}


static qbool SNDDMA_InitDS()
{
	HRESULT			hresult;
	DSBUFFERDESC	dsbuf;
	DSBCAPS			dsbcaps;
	WAVEFORMATEX	format;

	Com_Printf( "Initializing DirectSound\n" );

	if (SUCCEEDED( hresult = CoCreateInstance( CLSID_DirectSound8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound8, (void **)&pDS))) {
		Com_DPrintf( "Using DS8\n" );
	}
	else if (SUCCEEDED( hresult = CoCreateInstance( CLSID_DirectSound, NULL, CLSCTX_INPROC_SERVER, IID_IDirectSound, (void **)&pDS))) {
		Com_DPrintf( "Using legacy DS\n" );
	}
	else {
		Com_Printf("failed\n");
		Sys_S_Shutdown();
		return qfalse;
	}

	hresult = pDS->Initialize( NULL );

	Com_DPrintf("...setting DSSCL_PRIORITY coop level: " );

	if ( DS_OK != pDS->SetCooperativeLevel( g_wv.hWnd, DSSCL_PRIORITY ) ) {
		Com_Printf ("failed\n");
		Sys_S_Shutdown();
		return qfalse;
	}
	Com_DPrintf("ok\n" );


	// create the secondary buffer we'll actually work with
	dma.channels = 2;
	dma.samplebits = 16;
	dma.speed = s_khz->integer == 44 ? 44100 : 22050;

	memset (&format, 0, sizeof(format));
	format.wFormatTag = WAVE_FORMAT_PCM;
	format.nChannels = dma.channels;
	format.wBitsPerSample = dma.samplebits;
	format.nSamplesPerSec = dma.speed;
	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.cbSize = 0;
	format.nAvgBytesPerSec = format.nSamplesPerSec*format.nBlockAlign;

	memset (&dsbuf, 0, sizeof(dsbuf));
	dsbuf.dwSize = sizeof(DSBUFFERDESC);

	dsbuf.dwBufferBytes = SECONDARY_BUFFER_SIZE;
	dsbuf.lpwfxFormat = &format;

	memset(&dsbcaps, 0, sizeof(dsbcaps));
	dsbcaps.dwSize = sizeof(dsbcaps);

	Com_DPrintf( "...creating secondary buffer: " );
	dsbuf.dwFlags = DSBCAPS_LOCHARDWARE | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
	if (DS_OK == pDS->CreateSoundBuffer( &dsbuf, &pDSBuf, NULL )) {
		Com_Printf( "locked hardware.  ok\n" );
	}
	else {
		// Couldn't get hardware, fallback to software.
		dsbuf.dwFlags = DSBCAPS_LOCSOFTWARE | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS | DSBCAPS_CTRLVOLUME;
		if (DS_OK != pDS->CreateSoundBuffer( &dsbuf, &pDSBuf, NULL )) {
			Com_Printf( "failed\n" );
			Sys_S_Shutdown();
			return qfalse;
		}
		Com_DPrintf( "forced to software.  ok\n" );
	}

	// Make sure mixer is active
	if ( DS_OK != pDSBuf->Play( 0, 0, DSBPLAY_LOOPING ) ) {
		Com_Printf ("*** Looped sound play failed ***\n");
		Sys_S_Shutdown ();
		return qfalse;
	}

	// get the returned buffer size
	if ( DS_OK != pDSBuf->GetCaps(&dsbcaps) ) {
		Com_Printf ("*** GetCaps failed ***\n");
		Sys_S_Shutdown ();
		return qfalse;
	}

	gSndBufSize = dsbcaps.dwBufferBytes;

	dma.channels = format.nChannels;
	dma.samplebits = format.wBitsPerSample;
	dma.speed = format.nSamplesPerSec;
	dma.samples = gSndBufSize/(dma.samplebits/8);
	dma.submission_chunk = 1;
	dma.buffer = NULL;			// must be locked first

	sample16 = (dma.samplebits/8) - 1;

	Sys_S_BeginPainting();
	if (dma.buffer)
		memset(dma.buffer, 0, dma.samples * dma.samplebits/8);
	Sys_S_Submit();

	return qtrue;
}


qbool Sys_S_Init(void)
{
	CoInitialize(NULL);

	memset ((void *)&dma, 0, sizeof (dma));
	if (!SNDDMA_InitDS()) {
		assert(!pDSBuf);
		return qfalse;
	}

	assert(pDSBuf);
	Com_DPrintf("Completed successfully\n" );

	return qtrue;
}


int Sys_S_GetDMAPos( void ) {
	MMTIME	mmtime;
	int		s;
	DWORD	dwWrite;

	mmtime.wType = TIME_SAMPLES;
	pDSBuf->GetCurrentPosition( &mmtime.u.sample, &dwWrite );

	s = mmtime.u.sample;

	s >>= sample16;

	s &= (dma.samples-1);

	return s;
}


void Sys_S_BeginPainting( void ) {
	int		reps;
	DWORD	dwSize2;
	DWORD	*pbuf, *pbuf2;
	HRESULT	hresult;
	DWORD	dwStatus;

	if ( !pDSBuf ) {
		return;
	}

	// if the buffer was lost or stopped, restore it and/or restart it
	if ( pDSBuf->GetStatus(&dwStatus) != DS_OK ) {
		Com_Printf ("Couldn't get sound buffer status\n");
	}

	if (dwStatus & DSBSTATUS_BUFFERLOST)
		pDSBuf->Restore();

	if (!(dwStatus & DSBSTATUS_PLAYING))
		pDSBuf->Play( 0, 0, DSBPLAY_LOOPING );

	// lock the dsound buffer

	reps = 0;
	dma.buffer = NULL;

	while ((hresult = pDSBuf->Lock( 0, gSndBufSize, (LPVOID*)&pbuf, &locksize, (LPVOID*)&pbuf2, &dwSize2, 0 )) != DS_OK)
	{
		if (hresult != DSERR_BUFFERLOST)
		{
			Com_Printf( "SNDDMA_BeginPainting: Lock failed with error '%s'\n", DSoundError( hresult ) );
			S_Shutdown ();
			return;
		}
		else
		{
			pDSBuf->Restore();
		}

		if (++reps > 2)
			return;
	}
	dma.buffer = (unsigned char *)pbuf;
}


void Sys_S_Submit( void )
{
	if ( pDSBuf ) {
		pDSBuf->Unlock( dma.buffer, locksize, NULL, 0 );
	}
}


void WIN_S_Mute( qbool mute )
{
	if (!pDSBuf) {
		return;
	}

	static LONG lVolume = DSBVOLUME_MIN;
	static qbool bWasMuted = qfalse;

	if (mute && !bWasMuted) {
		pDSBuf->GetVolume(&lVolume);
		pDSBuf->SetVolume(DSBVOLUME_MIN);
		bWasMuted = qtrue;
	} else if (!mute && bWasMuted && lVolume != DSBVOLUME_MIN) {
		pDSBuf->SetVolume(lVolume);
		bWasMuted = qfalse;
	}
}

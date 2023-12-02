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
// cl_scrn.c -- master for refresh, console, chat, notify, etc

#include "client.h"


static qbool scr_initialized;	// ready to draw

cvar_t		*cl_timegraph;
static cvar_t* cl_graphheight;
static cvar_t* cl_graphscale;
static cvar_t* cl_graphshift;


struct intStats_t {
	int samples[200];
	int sampleCount;
	int writeIndex;
	int median;
	int percentile99;
};


static int QDECL CompareSamples( const void* a, const void* b )
{
	return *(const int*)b - *(const int*)a;
}


static void PushSample( intStats_t* stats, int sample )
{
	stats->samples[stats->writeIndex] = sample;
	stats->sampleCount = min(stats->sampleCount + 1, (int)ARRAY_LEN(stats->samples));
	stats->writeIndex = (stats->writeIndex + 1) % ARRAY_LEN(stats->samples);
	if ( stats->sampleCount < ARRAY_LEN(stats->samples) ) {
		stats->median = 0;
		stats->percentile99 = 0;
		return;
	}

	static intStats_t temp;
	memcpy( temp.samples, stats->samples, stats->sampleCount * sizeof(int) );
	qsort( temp.samples, stats->sampleCount, sizeof(int), &CompareSamples );
	stats->median = temp.samples[stats->sampleCount / 2];
	stats->percentile99 = temp.samples[stats->sampleCount / 10];
}


// adjust for resolution and screen aspect ratio

void SCR_AdjustFrom640( float *x, float *y, float *w, float *h )
{
#if 0
	// adjust for wide screens
	if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
		*x += 0.5 * ( cls.glconfig.vidWidth - ( cls.glconfig.vidHeight * 640 / 480 ) );
	}
#endif

	// scale for screen sizes
	float xscale = (float)cls.glconfig.vidWidth / SCREEN_WIDTH;
	float yscale = (float)cls.glconfig.vidHeight / SCREEN_HEIGHT;

	if ( x ) {
		*x *= xscale;
	}
	if ( y ) {
		*y *= yscale;
	}
	if ( w ) {
		*w *= xscale;
	}
	if ( h ) {
		*h *= yscale;
	}
}


// SCR_ functions operate  at native resolution, NOT the virtual 640x480 screen


void SCR_DrawChar( float x, float y, float cw, float ch, int c )
{
	const float CELL_TCSIZE = 0.0625;

	c &= 255;

	if ( c == ' ' ) {
		return;
	}

	int xcell = c & 15;
	int ycell = c >> 4;

	float s = xcell * CELL_TCSIZE;
	float t = ycell * CELL_TCSIZE;

	re.DrawStretchPic( x, y, cw, ch, s, t, s + CELL_TCSIZE, t + CELL_TCSIZE, cls.charSetShader );
}


// draws a string with a drop shadow, optionally with colorcodes

void SCR_DrawStringEx( float x, float y, float cw, float ch, const char* string, drawStringColors_t colors, qbool showColorCodes, const float* firstColor )
{
	float xx;
	const char* s;

	// don't bother if nothing will be visible
	if ( y < -ch ) {
		return;
	}

	// draw the drop shadow
	re.SetColor( colorBlack );
	s = string;
	xx = x + 1;
	while ( *s ) {
		if ( !showColorCodes && Q_IsColorString( s ) ) {
			s += 2;
			continue;
		}
		SCR_DrawChar( xx, y+1, cw, ch, *s );
		xx += cw;
		s++;
	}

	// draw the text, possibly with colors
	s = string;
	xx = x;
	if ( firstColor == NULL )
		firstColor = colors == DSC_CONSOLE ? ConsoleColorFromChar( COLOR_WHITE ) : colorWhite;
	re.SetColor( firstColor );
	while ( *s ) {
		if ( Q_IsColorString( s ) ) {
			if ( colors == DSC_NORMAL )
				re.SetColor( ColorFromChar( s[1] ) );
			else if ( colors == DSC_CONSOLE )
				re.SetColor( ConsoleColorFromChar( s[1] ) );
			if ( !showColorCodes ) {
				s += 2;
				continue;
			}
		}
		SCR_DrawChar( xx, y, cw, ch, *s );
		xx += cw;
		s++;
	}

	re.SetColor( NULL );
}


// draws a string with a drop shadow, optionally with colorcodes

void SCR_DrawString(float x, float y, float cw, float ch, const char* s, qbool allowColor)
{
	SCR_DrawStringEx( x, y, cw, ch, s, allowColor ? DSC_NORMAL : DSC_NONE, qfalse, NULL );
}


///////////////////////////////////////////////////////////////


static void SCR_DrawDemoRecording()
{
	if (!clc.demorecording || !clc.showAnnoyingDemoRecordMessage)
		return;

	int pos = FS_FTell( clc.demofile );
	const char* s = va( "RECORDING %s: %ik", clc.demoName, pos / 1024 );

	float x = 320, y = 20, cw = 8, ch = 8;
	SCR_AdjustFrom640( &x, &y, &cw, &ch );
	SCR_DrawString( x - strlen(s) * cw * 0.5, y, cw, ch, s, qfalse );
}


///////////////////////////////////////////////////////////////


typedef struct
{
	float	value;
	int		color;
} graphsamp_t;

static	int			current;
static	graphsamp_t	values[1024];


void SCR_DebugGraph( float value, int color )
{
	values[current&1023].value = value;
	values[current&1023].color = color;
	current++;
}


static void SCR_DrawDebugGraph()
{
	int		a, x, y, w, i, h;
	float	v;
	int		color;

	w = cls.glconfig.vidWidth;
	x = 0;
	y = cls.glconfig.vidHeight;
	re.SetColor( colorBlack );
	re.DrawStretchPic( x, y - cl_graphheight->integer, w, cl_graphheight->integer, 0, 0, 0, 0, cls.whiteShader );
	re.SetColor( NULL );

	for (a=0 ; a<w ; a++)
	{
		i = (current-1-a+1024) & 1023;
		v = values[i].value;
		color = values[i].color;
		v = v * cl_graphscale->integer + cl_graphshift->integer;

		if (v < 0)
			v += cl_graphheight->integer * (1+(int)(-v / cl_graphheight->integer));
		h = (int)v % cl_graphheight->integer;
		re.DrawStretchPic( x+w-1-a, y - h, 1, h, 0, 0, 0, 0, cls.whiteShader );
	}
}


///////////////////////////////////////////////////////////////


void SCR_Init()
{
	cl_timegraph = Cvar_Get( "timegraph", "0", CVAR_CHEAT );
	cl_graphheight = Cvar_Get( "graphheight", "32", CVAR_CHEAT );
	cl_graphscale = Cvar_Get( "graphscale", "1", CVAR_CHEAT );
	cl_graphshift = Cvar_Get( "graphshift", "0", CVAR_CHEAT );

	scr_initialized = qtrue;
}


///////////////////////////////////////////////////////////////


static void SCR_DrawStats( float x, float y, float w, float h, int value )
{
	if ( value < 0 || value >= 100 )
		SCR_DrawString( x, y, w, h, "  ?", qfalse );
	else
		SCR_DrawString( x, y, w, h, va( "%3d", value ), qfalse );
}


static void SCR_DrawInputStats( int stats, float x, float y, float w, float h, const char* header )
{
	SCR_DrawString( x, y, w, h, header, qfalse );
	x += 5.0f * w;
	SCR_DrawStats( x, y, w, h, stats );
}


static void SCR_DrawMouseInputLatencies()
{
	if ( !cls.cgameStarted || cls.state != CA_ACTIVE ||
		 !Cvar_VariableIntegerValue( "cl_drawMouseLag" ) )
		return;

	float x = 10, y = 10, w = 16, h = 24;
	SCR_AdjustFrom640( &x, &y, 0, 0 );
	SCR_DrawString( x, y, w, h, "mouse lag [ms]", qfalse );
	y += 30;
	SCR_DrawInputStats( cl.userCmdTime - cl.mouseTime, x, y, w, h, "netw" );
	y += 30;
	SCR_DrawInputStats( re.GetCameraMatrixTime() - cl.mouseTime, x, y, w, h, "draw" );
}


// this will be called twice if rendering in stereo mode

static void SCR_DrawScreenField( stereoFrame_t stereoFrame )
{
	if (!cls.rendererStarted)
		return;

	re.BeginFrame( stereoFrame );

	// wide aspect ratio screens need to have the sides cleared
	// unless they are displaying game renderings
	if ( cls.state != CA_ACTIVE && cls.state != CA_CINEMATIC ) {
		if ( cls.glconfig.vidWidth * 480 > cls.glconfig.vidHeight * 640 ) {
			re.SetColor( colorBlack );
			re.DrawStretchPic( 0, 0, cls.glconfig.vidWidth, cls.glconfig.vidHeight, 0, 0, 0, 0, cls.whiteShader );
			re.SetColor( NULL );
		}
	}

	if ( !uivm ) {
		Com_DPrintf( "draw screen without UI loaded\n" );
		return;
	}

	// if the menu is going to cover the entire screen, we don't need to render anything under it
	if ( !VM_Call( uivm, UI_IS_FULLSCREEN ) ) {
		switch ( cls.state ) {
		default:
			Com_Error( ERR_FATAL, "SCR_DrawScreenField: bad cls.state" );
			break;
		case CA_CINEMATIC:
			SCR_DrawCinematic();
			break;
		case CA_DISCONNECTED:
			// force menu up
			S_StopAllSounds();
			VM_Call( uivm, UI_SET_ACTIVE_MENU, UIMENU_MAIN );
			break;
		case CA_CONNECTING:
		case CA_CHALLENGING:
		case CA_CONNECTED:
			// connecting clients will only show the connection dialog
			// refresh to update the time
			VM_Call( uivm, UI_REFRESH, cls.realtime );
			VM_Call( uivm, UI_DRAW_CONNECT_SCREEN, qfalse );
			break;
		case CA_LOADING:
		case CA_PRIMED:
			// draw the game information screen and loading progress
			CL_CGameRendering( stereoFrame );
			// also draw the connection information, so it doesn't
			// flash away too briefly on local or lan games
			// refresh to update the time
			VM_Call( uivm, UI_REFRESH, cls.realtime );
			VM_Call( uivm, UI_DRAW_CONNECT_SCREEN, qtrue );
			break;
		case CA_ACTIVE:
			CL_CGameRendering( stereoFrame );
			SCR_DrawDemoRecording();
			break;
		}
	}

	// the menu draws next
	if ( cls.keyCatchers & KEYCATCH_UI && uivm ) {
		VM_Call( uivm, UI_REFRESH, cls.realtime );
	}

	SCR_DrawMouseInputLatencies();

	// console draws next
	Con_DrawConsole();

	// debug graphs can be drawn on top of anything
	if ( cl_timegraph->integer || cl_debugMove->integer ) {
		SCR_DrawDebugGraph();
	}
}


static int pcFE[RF_STATS_MAX];
static int pc2D[RB_STATS_MAX];
static int pc3D[RB_STATS_MAX];
static intStats_t usecFE;
static intStats_t usec3D;
static intStats_t usecBS;
static intStats_t usecGPU;


static void SCR_PerformanceCounters()
{
	extern qbool Sys_V_IsVSynced();

	float x, y, cw = 8, ch = 12;
	SCR_AdjustFrom640( 0, 0, &cw, &ch );

	x = 0, y = 240;
	SCR_AdjustFrom640( &x, &y, 0, 0 );
	SCR_DrawString( x, y, cw, ch, va("FE: %.2fms (99th: %.2fms)", usecFE.median / 1000.0f, usecFE.percentile99 / 1000.0f ), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Leaf/Area:  %i %i", pcFE[RF_LEAF_CLUSTER], pcFE[RF_LEAF_AREA]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("FrusLeafs:  %i", pcFE[RF_LEAFS]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("MD3 Cull S: %i %i %i", pcFE[RF_MD3_CULL_S_IN], pcFE[RF_MD3_CULL_S_CLIP], pcFE[RF_MD3_CULL_S_OUT]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("MD3 Cull B: %i %i %i", pcFE[RF_MD3_CULL_B_IN], pcFE[RF_MD3_CULL_B_CLIP], pcFE[RF_MD3_CULL_B_OUT]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Bez Cull S: %i %i %i", pcFE[RF_BEZ_CULL_S_IN], pcFE[RF_BEZ_CULL_S_CLIP], pcFE[RF_BEZ_CULL_S_OUT]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Bez Cull B: %i %i %i", pcFE[RF_BEZ_CULL_B_IN], pcFE[RF_BEZ_CULL_B_CLIP], pcFE[RF_BEZ_CULL_B_OUT]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Light Cull: %i %i", pcFE[RF_LIGHT_CULL_IN], pcFE[RF_LIGHT_CULL_OUT]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Lit Leafs:  %i", pcFE[RF_LIT_LEAFS]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Lit Surfs:  %i %i", pcFE[RF_LIT_SURFS], pcFE[RF_LIT_CULLS]), qfalse ); y += ch;

	x = 240, y = 240;
	SCR_AdjustFrom640( &x, &y, 0, 0 );
	SCR_DrawString( x, y, cw, ch, va("3D:  %.2fms (99th: %.2fms)", usec3D.median / 1000.0f, usec3D.percentile99 / 1000.0f ), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("End: %.2fms (99th: %.2fms)", usecBS.median / 1000.0f, usecBS.percentile99 / 1000.0f ), qfalse ); y += ch;
	if ( usecGPU.sampleCount > 0 ) {
		SCR_DrawString( x, y, cw, ch, va("GPU: %.2fms (99th: %.2fms)", usecGPU.median / 1000.0f, usecGPU.percentile99 / 1000.0f ), qfalse ); y += ch;
	}	
	SCR_DrawString( x, y, cw, ch, va("Base Verts: %i", pc3D[RB_VERTICES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Base Tris:  %i", pc3D[RB_INDICES] / 3), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Base Surfs: %i", pc3D[RB_SURFACES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("B Batches:  %i", pc3D[RB_BATCHES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Shdr Chges: %i", pc3D[RB_SHADER_CHANGES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Draw Calls: %i", pc3D[RB_DRAW_CALLS]), qfalse ); y += ch;

	SCR_DrawString( x, y, cw, ch, va("Lit Verts:  %i", pc3D[RB_LIT_VERTICES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Lit Tris:   %i", pc3D[RB_LIT_INDICES] / 3), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Lit Surfs:  %i", pc3D[RB_LIT_SURFACES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("L Batches:  %i", pc3D[RB_LIT_BATCHES]), qfalse ); y += ch;

	SCR_DrawString( x, y, cw, ch, va("L LateCullT: %i", pc3D[RB_LIT_VERTICES_LATECULLTEST]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("L LateCullI: %i %i", pc3D[RB_LIT_INDICES_LATECULL_IN], pc3D[RB_LIT_INDICES_LATECULL_OUT]), qfalse ); y += ch;

	x = 480, y = 240;
	SCR_AdjustFrom640( &x, &y, 0, 0 );
	SCR_DrawString( x, y, cw, ch, "2D", qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Vertices: %i", pc2D[RB_VERTICES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Indices:  %i", pc2D[RB_INDICES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Surfaces: %i", pc2D[RB_SURFACES]), qfalse ); y += ch;
	SCR_DrawString( x, y, cw, ch, va("Batches:  %i", pc2D[RB_BATCHES]), qfalse ); y += ch;

	x = 0, y = 480 - ch;
	SCR_AdjustFrom640( &x, &y, 0, 0 );
	SCR_DrawString( x, y, cw, ch, va("Back-end: %s", Cvar_VariableString("r_backend")), qfalse ); y -= ch;
	SCR_DrawString( x, y, cw, ch, va("V-Sync  : %s", Sys_V_IsVSynced() ? "ON" : "OFF"), qfalse ); y -= ch;
}


// called every frame, and can also be called explicitly to flush text to the screen

void SCR_UpdateScreen()
{
	static int recursive = 0;

	if ( !scr_initialized )
		return;

	if ( cls.maxFPS > 0 && Sys_Milliseconds() < cls.nextFrameTimeMS )
		return;
	
	// there are several cases where this IS called twice in one frame
	// easiest example is: conn to a server, kill the server
	if ( ++recursive > 2 ) {
		Com_Error( ERR_FATAL, "SCR_UpdateScreen: recursively called" );
	}

	// Why set to 1 and 0 explicitly?
	// Because the increment/decrement approach doesn't work.
	// One of the calls below might invoke Com_Error, which will in turn
	// call longjmp and "abort" the current frame, meaning the end of this
	// function (or any function for that matter) is not always reached.
	recursive = 1;

	const qbool drawFrame = CL_VideoRecording() || !Sys_IsMinimized();
	SCR_DrawScreenField( STEREO_CENTER );

	PushSample( &usecFE, pcFE[RF_USEC] );
	PushSample( &usec3D, pc3D[RB_USEC] );
	PushSample( &usecBS, pc3D[RB_USEC_END] );
	if ( pc3D[RB_USEC_GPU] > 0 )
		PushSample( &usecGPU, pc3D[RB_USEC_GPU] );
	else
		usecGPU.sampleCount = 0;

	if ( com_speeds->integer ) {
		re.EndFrame( pcFE, pc2D, pc3D, drawFrame );
		time_frontend = pcFE[RF_USEC];
		time_backend = pc3D[RB_USEC];
	} else if ( Cvar_VariableIntegerValue("r_speeds") ) {
		// counters are actually the previous frame's, because EndFrame will clear them
		// and we need to submit the calls to show them before that
		if ( re.Registered() )
			SCR_PerformanceCounters();
		re.EndFrame( pcFE, pc2D, pc3D, drawFrame );
	} else {
		re.EndFrame( NULL, NULL, NULL, drawFrame );
	}

	if ( cls.maxFPS > 0 )
		cls.nextFrameTimeMS = Sys_Milliseconds() + 1000 / cls.maxFPS;

	recursive = 0;
}


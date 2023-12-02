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
// tr_init.c -- functions that are not called every frame

#include "tr_local.h"
#include "tr_help.h"

glconfig_t	glConfig;
glinfo_t	glInfo;

screenshotCommand_t	r_delayedScreenshot;
qbool				r_delayedScreenshotPending = qfalse;
int					r_delayedScreenshotFrame = 0;

graphicsAPILayer_t	gal;

cvar_t	*r_backend;
cvar_t	*r_frameSleep;

cvar_t	*r_verbose;

cvar_t	*r_displayRefresh;

cvar_t	*r_detailTextures;

cvar_t	*r_intensity;
cvar_t	*r_gamma;
cvar_t	*r_greyscale;

cvar_t	*r_measureOverdraw;

cvar_t	*r_fastsky;
cvar_t	*r_dynamiclight;

cvar_t	*r_lodbias;
cvar_t	*r_lodscale;

cvar_t	*r_norefresh;
cvar_t	*r_drawentities;
cvar_t	*r_drawworld;
cvar_t	*r_speeds;
cvar_t	*r_fullbright;
cvar_t	*r_lightmap;
cvar_t	*r_lightmapGreyscale;
cvar_t	*r_mapGreyscale;
cvar_t	*r_mapGreyscaleCTF;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nocurves;
cvar_t	*r_depthFade;
cvar_t	*r_gpuMipGen;
cvar_t	*r_alphaToCoverage;
cvar_t	*r_dither;
cvar_t	*r_rtColorFormat;
cvar_t	*r_depthClamp;

cvar_t	*r_mipGenFilter;
cvar_t	*r_mipGenGamma;
cvar_t	*r_ditherStrength;
cvar_t	*r_transpSort;
cvar_t	*r_gl3_geoStream;
cvar_t	*r_d3d11_syncOffsets;
cvar_t	*r_d3d11_presentMode;
cvar_t	*r_ext_max_anisotropy;
cvar_t	*r_msaa;

cvar_t	*r_ignoreGLErrors;
cvar_t	*r_ignoreShaderSortKey;

cvar_t	*r_vertexLight;
cvar_t	*r_uiFullScreen;
cvar_t	*r_mode;
cvar_t	*r_blitMode;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_showsky;
cvar_t	*r_showtris;
cvar_t	*r_shownormals;
cvar_t	*r_finish;
cvar_t	*r_clear;
cvar_t	*r_swapInterval;
cvar_t	*r_textureMode;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_alphaToCoverageMipBoost;

cvar_t	*r_width;
cvar_t	*r_height;
cvar_t	*r_customaspect;

cvar_t	*r_brightness;
cvar_t	*r_mapBrightness;

cvar_t	*r_debugSurface;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;

// these limits apply to the sum of all scenes in a frame:
// the main view, all the 3D icons, and even the console etc
#define DEFAULT_MAX_POLYS		8192
#define DEFAULT_MAX_POLYVERTS	32768
static cvar_t* r_maxpolys;
static cvar_t* r_maxpolyverts;
int max_polys;
int max_polyverts;


void R_ConfigureVideoMode( int desktopWidth, int desktopHeight )
{
	glInfo.winFullscreen = !!r_fullscreen->integer;
	glInfo.vidFullscreen = r_fullscreen->integer && r_mode->integer == VIDEOMODE_CHANGE;

	if (r_fullscreen->integer && r_mode->integer == VIDEOMODE_DESKTOPRES) {
		glConfig.vidWidth = desktopWidth;
		glConfig.vidHeight = desktopHeight;
		glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
		glInfo.winWidth = desktopWidth;
		glInfo.winHeight = desktopHeight;
		return;
	}

	if (r_fullscreen->integer && r_mode->integer == VIDEOMODE_UPSCALE) {
		glConfig.vidWidth = r_width->integer;
		glConfig.vidHeight = r_height->integer;
		glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
		glInfo.winWidth = desktopWidth;
		glInfo.winHeight = desktopHeight;
		return;
	}
		
	glConfig.vidWidth = r_width->integer;
	glConfig.vidHeight = r_height->integer;
	glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
	glInfo.winWidth = r_width->integer;
	glInfo.winHeight = r_height->integer;
}


///////////////////////////////////////////////////////////////


static void RB_TakeScreenshotTGA( int x, int y, int width, int height, const char* fileName )
{
	int c = (width * height * 3);
	RI_AutoPtr p( sizeof(TargaHeader) + c );

	TargaHeader* tga = p.Get<TargaHeader>();
	Com_Memset( tga, 0, sizeof(TargaHeader) );
	tga->image_type = 2; // uncompressed BGR
	tga->width = LittleShort( width );
	tga->height = LittleShort( height );
	tga->pixel_size = 24;
	gal.ReadPixels( x, y, width, height, 1, CS_BGR, p + sizeof(TargaHeader) );
	ri.FS_WriteFile( fileName, p, sizeof(TargaHeader) + c );
}


static void RB_TakeScreenshotJPG( int x, int y, int width, int height, const char* fileName )
{
	RI_AutoPtr p( width * height * 4 );
	gal.ReadPixels( x, y, width, height, 1, CS_RGBA, p );

	RI_AutoPtr out( width * height * 4 );
	int n = SaveJPGToBuffer( out, 95, width, height, p );
	ri.FS_WriteFile( fileName, out, n );
}


const void* RB_TakeScreenshotCmd( const screenshotCommand_t* cmd )
{
	// NOTE: the current read buffer is the last FBO color attachment texture that was written to
	// therefore, ReadPixels will get the latest data even with double/triple buffering enabled

	switch (cmd->type) {
		case screenshotCommand_t::SS_JPG:
			RB_TakeScreenshotJPG( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );
			ri.Printf( PRINT_ALL, "Wrote %s\n", cmd->fileName );
			break;
		case screenshotCommand_t::SS_TGA:
			RB_TakeScreenshotTGA( cmd->x, cmd->y, cmd->width, cmd->height, cmd->fileName );
			ri.Printf( PRINT_ALL, "Wrote %s\n", cmd->fileName );
			break;
	}

	if (cmd->conVis > 0.0f) {
		ri.SetConsoleVisibility( cmd->conVis );
		r_delayedScreenshotPending = qfalse;
		r_delayedScreenshotFrame = 0;
	}

	return (const void*)(cmd + 1);
}


// screenshot filename is YYYY_MM_DD-HH_MM_SS-TTT
// so you can find the damn things and you never run out of them for movies  :)

static void R_TakeScreenshot( const char* ext, screenshotCommand_t::ss_type type, qbool hideConsole )
{
	static char s[MAX_OSPATH];

	const float conVis = hideConsole ? ri.SetConsoleVisibility( 0.0f ) : 0.0f;
	screenshotCommand_t* cmd;
	if ( conVis > 0.0f ) {
		cmd = &r_delayedScreenshot;
		r_delayedScreenshotPending = qtrue;
		r_delayedScreenshotFrame = 0;
		cmd->delayed = qtrue;
	} else {
		if ( R_FindRenderCommand( RC_SCREENSHOT ) )
			return;
		cmd = (screenshotCommand_t*)R_GetCommandBuffer( sizeof(screenshotCommand_t), qfalse );
		if ( !cmd )
			return;
		cmd->delayed = qfalse;
	}

	if (ri.Cmd_Argc() == 2) {
		Com_sprintf( s, sizeof(s), "screenshots/%s.%s", ri.Cmd_Argv(1), ext );
	} else {
		qtime_t t;
		Com_RealTime( &t );
		int ms = min( 999, backEnd.refdef.time & 1023 );
		Com_sprintf( s, sizeof(s), "screenshots/%d_%02d_%02d-%02d_%02d_%02d-%03d.%s",
			1900+t.tm_year, 1+t.tm_mon, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, ms, ext );
	}

	cmd->commandId = RC_SCREENSHOT;
	cmd->x = 0;
	cmd->y = 0;
	cmd->width = glConfig.vidWidth;
	cmd->height = glConfig.vidHeight;
	cmd->fileName = s;
	cmd->type = type;
	cmd->conVis = conVis;
}


static void R_ScreenShotTGA_f()
{
	R_TakeScreenshot( "tga", screenshotCommand_t::SS_TGA, qfalse );
}


static void R_ScreenShotJPG_f()
{
	R_TakeScreenshot( "jpg", screenshotCommand_t::SS_JPG, qfalse );
}


static void R_ScreenShotNoConTGA_f()
{
	R_TakeScreenshot( "tga", screenshotCommand_t::SS_TGA, qtrue );
}


static void R_ScreenShotNoConJPG_f()
{
	R_TakeScreenshot( "jpg", screenshotCommand_t::SS_JPG, qtrue );
}


//============================================================================


const void *RB_TakeVideoFrameCmd( const void *data )
{
	const videoFrameCommand_t* cmd = (const videoFrameCommand_t*)data;

	if( cmd->motionJpeg )
	{
		gal.ReadPixels( 0, 0, cmd->width, cmd->height, 1, CS_RGBA, cmd->captureBuffer );
		const int frameSize = SaveJPGToBuffer( cmd->encodeBuffer, 95, cmd->width, cmd->height, cmd->captureBuffer );
		ri.CL_WriteAVIVideoFrame( cmd->encodeBuffer, frameSize );
	}
	else
	{
		gal.ReadPixels( 0, 0, cmd->width, cmd->height, 4, CS_BGR, cmd->captureBuffer );
		const int frameSize = PAD( cmd->width, 4 ) * cmd->height * 3;
		ri.CL_WriteAVIVideoFrame( cmd->captureBuffer, frameSize );
	}

	return (const void *)(cmd + 1);
}


///////////////////////////////////////////////////////////////


void GfxInfo_f( void )
{
	ri.Printf( PRINT_ALL, "Back-end: %s\n", r_backend->string );
	if ( glConfig.vendor_string[0] != '\0' )
		ri.Printf( PRINT_ALL, "Vendor: %s\n", glConfig.vendor_string );
	if ( glConfig.renderer_string[0] != '\0' )
		ri.Printf( PRINT_ALL, "Renderer: %s\n", glConfig.renderer_string );
	if ( glConfig.version_string[0] != '\0' )
		ri.Printf( PRINT_ALL, "OpenGL version: %s\n", glConfig.version_string );
	ri.Printf( PRINT_ALL, "MSAA                  : %dx\n", glInfo.msaaSampleCount );
	ri.Printf( PRINT_ALL, "MSAA alpha to coverage: %s\n", glInfo.alphaToCoverageSupport ? "ON" : "OFF" );
	ri.Printf( PRINT_ALL, "Depth fade            : %s\n", glInfo.depthFadeSupport ? "ON" : "OFF" );
	ri.Printf( PRINT_ALL, "GPU mip-map generation: %s\n", glInfo.mipGenSupport ? "ON" : "OFF" );
	gal.PrintInfo();
}


///////////////////////////////////////////////////////////////


static const cmdTableItem_t r_cmds[] =
{
	{ "gfxinfo", GfxInfo_f, NULL, "prints display mode info" },
	{ "imagelist", R_ImageList_f, NULL, "prints loaded images" },
	{ "imageinfo", R_ImageInfo_f, NULL, "prints info for a specific image" },
	{ "shaderlist", R_ShaderList_f, NULL, "prints loaded shaders" },
	{ "shaderinfo", R_ShaderInfo_f, R_CompleteShaderName_f, "prints info for a specific shader" },
	{ "shadermixeduse", R_ShaderMixedUse_f, NULL, "prints all mixed use issues" },
	{ "skinlist", R_SkinList_f, NULL, "prints loaded skins" },
	{ "modellist", R_Modellist_f, NULL, "prints loaded models" },
	{ "screenshot", R_ScreenShotTGA_f, NULL, "takes a TARGA (.tga) screenshot" },
	{ "screenshotJPEG", R_ScreenShotJPG_f, NULL, "takes a JPEG (.jpg) screenshot" },
	{ "screenshotnc", R_ScreenShotNoConTGA_f, NULL, "takes a TARGA screenshot w/o the console" },
	{ "screenshotncJPEG", R_ScreenShotNoConJPG_f, NULL, "takes a JPEG screenshot w/o the console" }
};


static const cvarTableItem_t r_cvars[] =
{
	//
	// latched and archived variables
	//
#if defined( _WIN32 )
	{ &r_backend, "r_backend", "D3D11", CVAR_ARCHIVE | CVAR_LATCH, CVART_STRING, NULL, NULL, help_r_backend },
#else
	{ &r_backend, "r_backend", "GL3", CVAR_ARCHIVE | CVAR_LATCH, CVART_STRING,  NULL, NULL, help_r_backend },
#endif
	{ &r_frameSleep, "r_frameSleep", "2", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", help_r_frameSleep },
	{ &r_mipGenFilter, "r_mipGenFilter", "L4", CVAR_ARCHIVE | CVAR_LATCH, CVART_STRING, NULL, NULL, help_r_mipGenFilter },
	{ &r_mipGenGamma, "r_mipGenGamma", "1.8", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1.0", "3.0", help_r_mipGenGamma },
	{ &r_gl3_geoStream, "r_gl3_geoStream", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(GL3MAP_MAX), help_r_gl3_geoStream },
	{ &r_d3d11_syncOffsets, "r_d3d11_syncOffsets", "2", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(D3D11SO_MAX), help_r_d3d11_syncOffsets },
	{ &r_d3d11_presentMode, "r_d3d11_presentMode", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(DXGIPM_MAX), help_r_d3d11_presentMode },
	{ &r_ext_max_anisotropy, "r_ext_max_anisotropy", "16", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", "16", help_r_ext_max_anisotropy },
	{ &r_msaa, "r_msaa", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", "32", "anti-aliasing sample count, " S_COLOR_VAL "0" S_COLOR_HELP "=off" },
	{ &r_picmip, "r_picmip", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", "16", help_r_picmip },
	{ &r_roundImagesDown, "r_roundImagesDown", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_roundImagesDown },
	{ &r_colorMipLevels, "r_colorMipLevels", "0", CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_colorMipLevels },
	{ &r_detailTextures, "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "enables detail textures shader stages" },
	{ &r_mode, "r_mode", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(VIDEOMODE_MAX), help_r_mode },
	{ &r_blitMode, "r_blitMode", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", XSTRING(BLITMODE_MAX), help_r_blitMode },
	{ &r_brightness, "r_brightness", "2", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0.25", "32", "overall brightness" },
	// should be called r_lightmapBrightness
	{ &r_mapBrightness, "r_mapBrightness", "2", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0.25", "32", "brightness of lightmap textures" },
	// should be called r_textureBrightness
	{ &r_intensity, "r_intensity", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1", NULL, "brightness of non-lightmap map textures" },
	{ &r_fullscreen, "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "full-screen mode" },
	{ &r_width, "r_width", "1280", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "320", "65535", "custom window/render width" },
	{ &r_height, "r_height", "720", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "240", "65535", "custom window/render height" },
	{ &r_customaspect, "r_customaspect", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0.1", "10", "custom pixel aspect ratio" },
	{ &r_vertexLight, "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "disables lightmap texture blending" },
	// note that r_subdivisions > 64 will create rendering artefacts because you'll see the other side of a curved surface when against it
	{ &r_subdivisions, "r_subdivisions", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1", "64", help_r_subdivisions },
	{ &r_fullbright, "r_fullbright", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_fullbright },
	{ &r_lightmap, "r_lightmap", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_lightmap },
	{ &r_lightmapGreyscale, "r_lightmapGreyscale", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0", "1", "how desaturated the lightmap looks" },
	{ &r_depthFade, "r_depthFade", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_depthFade },
	{ &r_gpuMipGen, "r_gpuMipGen", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_gpuMipGen },
	{ &r_alphaToCoverage, "r_alphaToCoverage", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_alphaToCoverage },
	{ &r_dither, "r_dither", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_dither },
	{ &r_rtColorFormat, "r_rtColorFormat", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(RTCF_MAX), help_r_rtColorFormat },
	{ &r_depthClamp, "r_depthClamp", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_depthClamp },

	//
	// latched variables that can only change over a restart
	//
	{ &r_displayRefresh, "r_displayRefresh", "0", CVAR_LATCH, CVART_INTEGER, "0", "480", S_COLOR_VAL "0 " S_COLOR_HELP "lets the driver decide" },
	{ &r_singleShader, "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH },

	//
	// archived variables that can change at any time
	//
	{ &r_lodbias, "r_lodbias", "-2", CVAR_ARCHIVE, CVART_INTEGER, "-16", "16", help_r_lodbias },
	{ &r_ignoreGLErrors, "r_ignoreGLErrors", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "if " S_COLOR_VAL "0" S_COLOR_HELP ", OpenGL errors are fatal" },
	{ &r_ignoreShaderSortKey, "r_ignoreShaderSortKey", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_ignoreShaderSortKey },
	{ &r_fastsky, "r_fastsky", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_fastsky },
	{ &r_noportals, "r_noportals", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_noportals },
	{ &r_dynamiclight, "r_dynamiclight", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "enables dynamic lights" },
	{ &r_finish, "r_finish", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "enables glFinish calls" },
	{ &r_swapInterval, "r_swapInterval", "0", CVAR_ARCHIVE, CVART_INTEGER, "-8", "8", help_r_swapInterval },
	// r_textureMode's default value can't be an empty string because otherwise, a user-created default can stick (see Cvar_Get)
	{ &r_textureMode, "r_textureMode", "best", CVAR_ARCHIVE | CVAR_LATCH, CVART_STRING, NULL, NULL, help_r_textureMode },
	{ &r_gamma, "r_gamma", "1.2", CVAR_ARCHIVE, CVART_FLOAT, "0.5", "3", help_r_gamma },
	{ &r_greyscale, "r_greyscale", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", "how desaturated the final image looks" },
	{ &r_ditherStrength, "r_ditherStrength", "1.0", CVAR_ARCHIVE, CVART_FLOAT, "0.125", "8.0", help_r_ditherStrength },
	{ &r_transpSort, "r_transpSort", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_transpSort },
	{ &r_lodCurveError, "r_lodCurveError", "2000", CVAR_ARCHIVE, CVART_FLOAT, "250", "10000", "curved surfaces LOD scale" },
	{ &r_alphaToCoverageMipBoost, "r_alphaToCoverageMipBoost", "0.125", CVAR_ARCHIVE, CVART_FLOAT, "0", "0.5", "increases the alpha value of higher mip levels" },
	{ &r_mapGreyscale, "r_mapGreyscale", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", "how desaturated the map looks" },
	{ &r_mapGreyscaleCTF, "r_mapGreyscaleCTF", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", help_r_mapGreyscaleCTF },

	//
	// temporary variables that can change at any time
	//
	{ &r_ambientScale, "r_ambientScale", "0.6", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "entity ambient light scale" },
	{ &r_directedScale, "r_directedScale", "1", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "entity directed light scale" },
	{ &r_uiFullScreen, "r_uifullscreen", "0", CVAR_TEMP, CVART_BOOL }, // keeping it around in case we enable other mods again
	{ &r_debugLight, "r_debuglight", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "prints entity light values" },
	{ &r_debugSort, "r_debugSort", "0", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "doesn't render shaders with a greater sort key" },
	{ &r_nocurves, "r_nocurves", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "doesn't render grid meshes" },
	{ &r_drawworld, "r_drawworld", "1", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables rendering of world surfaces" },
	{ &r_portalOnly, "r_portalOnly", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "only draws the mirrored plane" },
	{ &r_lodscale, "r_lodscale", "5", CVAR_CHEAT, CVART_FLOAT, "1", "20", "LOD scale for MD3 models" },
	{ &r_norefresh, "r_norefresh", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables 3D scene rendering" },
	{ &r_drawentities, "r_drawentities", "1", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "enables entity rendering" },
	{ &r_nocull, "r_nocull", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables frustum culling" },
	{ &r_novis, "r_novis", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables PVS usage" },
	{ &r_speeds, "r_speeds", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "draws rendering performance counters" },
	{ &r_verbose, "r_verbose", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "prints additional information" },
	{ &r_debugSurface, "r_debugSurface", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "draws collision models" },
	{ &r_showsky, "r_showsky", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "forces sky in front of all surfaces" },
	{ &r_showtris, "r_showtris", "0", CVAR_CHEAT, CVART_BITMASK, "0", XSTRING(SHOWTRIS_MAX), help_r_showtris },
	{ &r_shownormals, "r_shownormals", "0", CVAR_CHEAT, CVART_BITMASK, "0", XSTRING(SHOWTRIS_MAX), help_r_shownormals },
	{ &r_clear, "r_clear", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "clears to violet instead of black" },
	{ &r_lockpvs, "r_lockpvs", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "helps visualize the current PVS' limits" },
	{ &r_maxpolys, "r_maxpolys", XSTRING(DEFAULT_MAX_POLYS), 0, CVART_INTEGER, XSTRING(DEFAULT_MAX_POLYS), NULL, "maximum polygon count per frame" },
	{ &r_maxpolyverts, "r_maxpolyverts", XSTRING(DEFAULT_MAX_POLYVERTS), 0, CVART_INTEGER, XSTRING(DEFAULT_MAX_POLYVERTS), NULL, "maximum polygon vertex count per frame" }
};


static void R_Register()
{
	ri.Cvar_RegisterTable( r_cvars, ARRAY_LEN(r_cvars) );
	ri.Cmd_RegisterTable( r_cmds, ARRAY_LEN(r_cmds) );
}


static void R_InitGAL()
{
	struct gal_t {
		getGALInterface_t grabInterface;
		galId_t id;
		const char* cvarValue;
		const char* fullName;
	};

	// preferred option goes first
	const gal_t galArray[] = {
#if defined( _WIN32 )
		{ &GAL_GetD3D11, GAL_D3D11, "D3D11", "Direct3D 11" },
#endif
		{ &GAL_GetGL3, GAL_GL3, "GL3", "OpenGL 3" },
		{ &GAL_GetGL2, GAL_GL2, "GL2", "OpenGL 2" }
	};

	int galIndex = -1;
	for ( int i = 0; i < ARRAY_LEN( galArray ); ++i ) {
		if ( !Q_stricmp( r_backend->string, galArray[i].cvarValue ) ) {
			galIndex = i;
			break;
		}
	}

	if ( galIndex < 0 ) {
		galIndex = 0;
		ri.Printf( PRINT_WARNING, "Invalid r_backend value, selecting the %s back-end instead\n", galArray[galIndex].fullName );
		ri.Cvar_Set( r_backend->name, galArray[galIndex].cvarValue );
	}

	ri.Printf( PRINT_ALL, "Initializing the %s back-end...\n", galArray[galIndex].fullName );

#if defined( _DEBUG )
	// helps catch unset function pointers
	memset( &gal, 0, sizeof( gal ) );
#endif

	if ( !galArray[galIndex].grabInterface( &gal ) )
		ri.Error( ERR_FATAL, "Failed to grab the %s back-end's interface\n", galArray[galIndex].fullName );

	if ( !gal.Init() )
		ri.Error( ERR_FATAL, "Failed to initialize the %s back-end\n", galArray[galIndex].fullName );

	gal.id = galArray[galIndex].id;
	GfxInfo_f();
}


static void R_InitMipFilter()
{
	struct filter_t {
		const char* cvarName;
		const char* fullName;
		float kernel[4];
	};

	// preferred option goes first
	const filter_t filterArray[] = {
		{ "L4", "Lanczos 4", -0.0491683967f, -0.0816986337f, 0.151636884f, 0.479230106f },
		{ "L3", "Lanczos 3", 0.0f, -0.0646646321f, 0.131493837f, 0.433170795f },
		{ "BL", "Bi-linear", 0.0f, 0.0f, 0.0f, 0.5f },
		{ "MN2", "Mitchell-Netravali 2 (B = 1/3, C = 1/3)", 0.0f, 0.0f, 0.123327762f, 0.376672268f },
		{ "BH4", "3-term Blackman-Harris 4", 0.00106591149f, 0.0288433302f, 0.151539952f, 0.318550885f },
		{ "BH3", "3-term Blackman-Harris 3", 0.0f, 0.00302829780f, 0.101031370f, 0.395940334f },
		{ "BH2", "3-term Blackman-Harris 2", 0.0f, 0.0f, 0.0151469158f, 0.484853119f },
		{ "T2", "Tent 2 (1/3 2/3)", 0.0f, 0.0f, 1.0f / 6.0f, 2.0f / 6.0f },
		// The results are so similar to Lanczos...
		//{ "K4", "Kaiser 4", -0.0487834960f, -0.0811581835f, 0.151146635f, 0.478795081f },
		//{ "K3", "Kaiser 3", 0.0f, -0.0642274171f, 0.131010026f, 0.433217406f }
	};

	int index = -1;
	for ( int i = 0; i < ARRAY_LEN( filterArray ); ++i ) {
		if ( !Q_stricmp( r_mipGenFilter->string, filterArray[i].cvarName ) ) {
			index = i;
			break;
		}
	}

	if ( index < 0 ) {
		index = 0;
		ri.Printf( PRINT_WARNING, "Invalid %s value, selecting the %s filter instead\n", r_mipGenFilter->name, filterArray[index].fullName );
	}

	memcpy( tr.mipFilter, filterArray[index].kernel, sizeof( tr.mipFilter ) );
}


void R_Init()
{
	COMPILE_TIME_ASSERT( sizeof(glconfig_t) == 11332 );
	COMPILE_TIME_ASSERT( sizeof(TargaHeader) == 18 );

	QSUBSYSTEM_INIT_START( "Renderer" );

	// clear all our internal state
	Com_Memset( &tr, 0, sizeof( tr ) );
	Com_Memset( &backEnd, 0, sizeof( backEnd ) );
	Com_Memset( &tess, 0, sizeof( tess ) );

	if ((intptr_t)tess.xyz & 15)
		Com_Printf( "WARNING: tess.xyz not 16 byte aligned\n" );

	R_InitFogTable();

	R_NoiseInit();

	R_Register();

	max_polys = max( r_maxpolys->integer, DEFAULT_MAX_POLYS );
	max_polyverts = max( r_maxpolyverts->integer, DEFAULT_MAX_POLYVERTS );

	byte* ptr = (byte*)ri.Hunk_Alloc( sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys + sizeof(polyVert_t) * max_polyverts, h_low );
	backEndData = (backEndData_t*)ptr;
	backEndData->polys = (srfPoly_t*)(ptr + sizeof(backEndData_t));
	backEndData->polyVerts = (polyVert_t*)(ptr + sizeof(backEndData_t) + sizeof(srfPoly_t) * max_polys);

	R_ClearFrame();

	R_InitMipFilter();

	R_InitGAL();

	R_InitImages();

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	QSUBSYSTEM_INIT_DONE( "Renderer" );
}


static void RE_Shutdown( qbool destroyWindow )
{
	ri.Printf( PRINT_DEVELOPER, "RE_Shutdown( %i )", destroyWindow );

	// This will also force the creation of a new context when switching
	// between GL2 and GL3, which is what we want because
	// the implementations have their own resources we can't keep around.
	if ( !destroyWindow && r_backend->latchedString )
		destroyWindow = qtrue;

	ri.Printf( PRINT_DEVELOPER, " -> %i\n", destroyWindow );

	if ( tr.registered ) {
		ri.Cmd_UnregisterModule();
		gal.ShutDown( destroyWindow );
	}

	// shut down platform-specific video stuff
	if ( destroyWindow ) {
		Sys_V_Shutdown();
		memset( &glConfig, 0, sizeof( glConfig ) );
	}

	tr.registered = qfalse;
}


static void RE_BeginRegistration( glconfig_t* glconfigOut )
{
	R_Init();

	*glconfigOut = glConfig;

	tr.viewCluster = -1;		// force markleafs to regenerate
	RE_ClearScene();

	tr.registered = qtrue;
}


static void RE_EndRegistration()
{
}


static int RE_GetCameraMatrixTime()
{
	return re_cameraMatrixTime;
}


static void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qbool dirty )
{
	if ( !tr.registered )
		return;

	gal.UpdateScratch( tr.scratchImage[client], cols, rows, data, dirty );
	tr.scratchShader->stages[0]->bundle.image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, (cols - 0.5f) / cols, (rows - 0.5f) / rows, (qhandle_t)tr.scratchShader->index );
}


static qbool RE_Registered()
{
	return tr.registered;
}


static qbool RE_IsFrameSleepNeeded()
{
	if ( r_frameSleep->integer != 2 )
		return r_frameSleep->integer;

	return !Sys_V_IsVSynced();
}


static qbool RE_IsDepthClampEnabled()
{
	return r_depthClamp->integer != 0;
}


const refexport_t* GetRefAPI( const refimport_t* rimp )
{
	static refexport_t re;

	ri = *rimp;

	Com_Memset( &re, 0, sizeof( re ) );

	// the RE_ functions are Renderer Entry points

	re.Shutdown = RE_Shutdown;

	re.BeginRegistration = RE_BeginRegistration;
	re.RegisterModel = RE_RegisterModel;
	re.RegisterSkin = RE_RegisterSkin;
	re.RegisterShader = RE_RegisterShader;
	re.RegisterShaderNoMip = RE_RegisterShaderNoMip;
	re.LoadWorld = RE_LoadWorldMap;
	re.SetWorldVisData = RE_SetWorldVisData;
	re.EndRegistration = RE_EndRegistration;

	re.BeginFrame = RE_BeginFrame;
	re.EndFrame = RE_EndFrame;

	re.MarkFragments = R_MarkFragments;
#if defined( QC )
	re.ProjectDecal = R_ProjectDecal;
#endif // QC
	re.LerpTag = R_LerpTag;
	re.ModelBounds = R_ModelBounds;

	re.ClearScene = RE_ClearScene;
	re.AddRefEntityToScene = RE_AddRefEntityToScene;
	re.AddPolyToScene = RE_AddPolyToScene;
	re.LightForPoint = R_LightForPoint;
	re.AddLightToScene = RE_AddLightToScene;
	re.RenderScene = RE_RenderScene;

	re.SetColor = RE_SetColor;
	re.DrawStretchPic = RE_StretchPic;

	re.DrawTriangle = RE_DrawTriangle;
	re.DrawStretchRaw = RE_StretchRaw;

	re.GetEntityToken = R_GetEntityToken;
	re.inPVS = R_inPVS;

	re.TakeVideoFrame = RE_TakeVideoFrame;

	re.GetCameraMatrixTime = RE_GetCameraMatrixTime;

	re.Registered = RE_Registered;

	re.ShouldSleep = RE_IsFrameSleepNeeded;

	re.DepthClamp = RE_IsDepthClampEnabled;
#if defined( QC )
	re.GetAdvertisements = RE_GetAdvertisements;
#endif

	return &re;
}

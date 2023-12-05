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
cvar_t	*r_fullbright;
cvar_t	*r_lightmap;
cvar_t	*r_lightmapGreyscale;
cvar_t	*r_mapGreyscale;
cvar_t	*r_mapGreyscaleCTF;
cvar_t	*r_teleporterFlash;
cvar_t	*r_sleepThreshold;
cvar_t	*r_shadingRate;
cvar_t	*r_guiFont;
cvar_t	*r_guiFontFile;
cvar_t	*r_guiFontHeight;
cvar_t	*r_novis;
cvar_t	*r_nocull;
cvar_t	*r_nocurves;
cvar_t	*r_depthFade;
cvar_t	*r_dither;
cvar_t	*r_rtColorFormat;
cvar_t	*r_depthClamp;
cvar_t	*r_gpuPreference;

cvar_t	*r_mipGenFilter;
cvar_t	*r_mipGenGamma;
cvar_t	*r_ditherStrength;
cvar_t	*r_transpSort;
cvar_t	*r_ext_max_anisotropy;
cvar_t	*r_smaa;

cvar_t	*r_ignoreShaderSortKey;

cvar_t	*r_vertexLight;
cvar_t	*r_uiFullScreen;
cvar_t	*r_mode;
cvar_t	*r_blitMode;
cvar_t	*r_singleShader;
cvar_t	*r_roundImagesDown;
cvar_t	*r_colorMipLevels;
cvar_t	*r_picmip;
cvar_t	*r_clear;
cvar_t	*r_vsync;
cvar_t	*r_lego;
cvar_t	*r_lockpvs;
cvar_t	*r_noportals;
cvar_t	*r_portalOnly;

cvar_t	*r_subdivisions;
cvar_t	*r_lodCurveError;

cvar_t	*r_width;
cvar_t	*r_height;

cvar_t	*r_brightness;
cvar_t	*r_mapBrightness;

cvar_t	*r_debugSurface;

cvar_t	*r_ambientScale;
cvar_t	*r_directedScale;
cvar_t	*r_debugLight;
cvar_t	*r_debugSort;

cvar_t	*r_debugUI;
cvar_t	*r_debugInput;

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


static void RB_TakeScreenshotTGA( int width, int height, const char* fileName )
{
	int c = (width * height * 3);
	RI_AutoPtr p( sizeof(TargaHeader) + c );

	TargaHeader* tga = p.Get<TargaHeader>();
	Com_Memset( tga, 0, sizeof(TargaHeader) );
	tga->image_type = 2; // uncompressed BGR
	tga->width = LittleShort( width );
	tga->height = LittleShort( height );
	tga->pixel_size = 24;
	renderPipeline->ReadPixels( width, height, 1, CS_BGR, p + sizeof(TargaHeader) );
	ri.FS_WriteFile( fileName, p, sizeof(TargaHeader) + c );
}


static void RB_TakeScreenshotJPG( int width, int height, const char* fileName )
{
	RI_AutoPtr p( width * height * 4 );
	renderPipeline->ReadPixels( width, height, 1, CS_RGBA, p );

	RI_AutoPtr out( width * height * 4 );
	int n = SaveJPGToBuffer( out, 95, width, height, p );
	ri.FS_WriteFile( fileName, out, n );
}


void RB_TakeScreenshotCmd( const screenshotCommand_t* cmd )
{
	switch (cmd->type) {
		case screenshotCommand_t::SS_JPG:
			RB_TakeScreenshotJPG( cmd->width, cmd->height, cmd->fileName );
			ri.Printf( PRINT_ALL, "Wrote %s\n", cmd->fileName );
			break;
		case screenshotCommand_t::SS_TGA:
			RB_TakeScreenshotTGA( cmd->width, cmd->height, cmd->fileName );
			ri.Printf( PRINT_ALL, "Wrote %s\n", cmd->fileName );
			break;
	}

	if (cmd->conVis > 0.0f) {
		ri.SetConsoleVisibility( cmd->conVis );
		r_delayedScreenshotPending = qfalse;
		r_delayedScreenshotFrame = 0;
	}
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
	} else {
		cmd = &backEndData->readbackCommands.screenshot;
		if ( cmd->requested )
			return;
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

	cmd->requested = qtrue;
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


void RB_TakeVideoFrameCmd( const videoFrameCommand_t *cmd )
{
	if( cmd->motionJpeg )
	{
		renderPipeline->ReadPixels( cmd->width, cmd->height, 1, CS_RGBA, cmd->captureBuffer );
		const int frameSize = SaveJPGToBuffer( cmd->encodeBuffer, 95, cmd->width, cmd->height, cmd->captureBuffer );
		ri.CL_WriteAVIVideoFrame( cmd->encodeBuffer, frameSize );
	}
	else
	{
		renderPipeline->ReadPixels( cmd->width, cmd->height, 4, CS_BGR, cmd->captureBuffer );
		const int frameSize = PAD( cmd->width, 4 ) * cmd->height * 3;
		ri.CL_WriteAVIVideoFrame( cmd->captureBuffer, frameSize );
	}
}


///////////////////////////////////////////////////////////////


static void GfxInfo_f()
{
	ri.Printf(PRINT_ALL, "RHI:                    %s\n", rhiInfo.name);
	ri.Printf(PRINT_ALL, "Adapter:                %s\n", rhiInfo.adapter);
	ri.Printf(PRINT_ALL, "  Tearing support:      %s\n", rhiInfo.hasTearing ? "YES" : "NO");
	ri.Printf(PRINT_ALL, "  Base VRS support:     %s\n", rhiInfo.hasBaseVRS ? "YES" : "NO");
	ri.Printf(PRINT_ALL, "  Extended VRS support: %s\n", rhiInfo.hasExtendedVRS ? "YES" : "NO");
	ri.Printf(PRINT_ALL, "  UMA:                  %s\n", rhiInfo.isUMA ? "YES" : "NO");
	ri.Printf(PRINT_ALL, "  Cache coherent UMA:   %s\n", rhiInfo.isCacheCoherentUMA ? "YES" : "NO");
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
	{
		&r_mipGenFilter, "r_mipGenFilter", "L4", CVAR_ARCHIVE | CVAR_LATCH, CVART_STRING, NULL, NULL, "mip-map generation filter",
		"Mip-map filter", CVARCAT_GRAPHICS, "Texture sharpness in the distance", "",
		CVAR_GUI_VALUE("L4", "Lanczos 4", "Very sharp, 4-pixel radius")
		CVAR_GUI_VALUE("L3", "Lanczos 3", "Very sharp, 3-pixel radius")
		CVAR_GUI_VALUE("BL", "Bi-linear", "Blurry, 1-pixel radius")
		CVAR_GUI_VALUE("MN2", "Mitchell-Netravali 2", "Balanced, 2-pixel radius")
		CVAR_GUI_VALUE("BH4", "3-term Blackman-Harris 4", "Balanced, 4-pixel radius")
		CVAR_GUI_VALUE("BH3", "3-term Blackman-Harris 3", "Balanced, 3-pixel radius")
		CVAR_GUI_VALUE("BH2", "3-term Blackman-Harris 2", "Balanced, 2-pixel radius")
		CVAR_GUI_VALUE("T2", "Tent 2 (1/3 2/3)", "Blurry, 2-pixel radius")
	},
	{
		&r_mipGenGamma, "r_mipGenGamma", "1.8", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1.0", "3.0", help_r_mipGenGamma,
		"Mip-map gamma", CVARCAT_GRAPHICS, "Texture contrast in the distance", ""
	},
	{
		&r_ext_max_anisotropy, "r_ext_max_anisotropy", "16", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", "16", help_r_ext_max_anisotropy,
		"Texture anisotropy", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Texture sharpness at oblique angles", ""
	},
	{
		&r_roundImagesDown, "r_roundImagesDown", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_roundImagesDown,
		"Low-resolution resampling", CVARCAT_GRAPHICS, "Lowers the resolution of non-power of two textures"
	},
	{
		&r_colorMipLevels, "r_colorMipLevels", "0", CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_colorMipLevels,
		"Colorize texture mips", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_detailTextures, "r_detailtextures", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "enables detail textures shader stages",
		"Enable detail textures", CVARCAT_GRAPHICS, "It also toggles decals on some maps", ""
	},
	{
		&r_mode, "r_mode", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(VIDEOMODE_MAX), help_r_mode,
		"Full-screen video mode", CVARCAT_DISPLAY, "", "",
		CVAR_GUI_VALUE("0", "Native res", "Same resolution as on the desktop")
		CVAR_GUI_VALUE("1", "Custom res/upscale", "Custom resolution, upsampled by CNQ3")
		CVAR_GUI_VALUE("2", "Video mode change", "Custom resolution, upsampled by the system\n\n"
			"Only use this on monitors that can reach higher refresh rates at lower resolutions.\n"
			"It makes alt-tabbing slow.")
	},
	{
		&r_brightness, "r_brightness", "2", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0.25", "32", "overall brightness",
		"Screen brightness", CVARCAT_GRAPHICS, "", ""
	},
	{
		// should be called r_lightmapBrightness
		&r_mapBrightness, "r_mapBrightness", "2", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0.25", "32", "brightness of lightmap textures",
		"Lightmap brightness", CVARCAT_GRAPHICS, "Applies to lightmap textures only", ""
	},
	{
		// should be called r_textureBrightness
		&r_intensity, "r_intensity", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1", "32", "brightness of non-lightmap map textures",
		"Texture brightness", CVARCAT_GRAPHICS, "Applies to non-lightmap textures only", ""
	},
	{
		&r_fullscreen, "r_fullscreen", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "full-screen mode",
		"Fullscreen", CVARCAT_DISPLAY, "", "",
		CVAR_GUI_VALUE("0", "Windowed", "")
		CVAR_GUI_VALUE("1", "Fullscreen", "")
	},
	{
		&r_width, "r_width", "1280", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "320", "65535", "custom window/render width",
		"Window/render width", CVARCAT_DISPLAY, "Used in windowed mode and non-native full-screen", ""
	},
	{
		&r_height, "r_height", "720", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "240", "65535", "custom window/render height",
		"Window/render height", CVARCAT_DISPLAY, "Used in windowed mode and non-native full-screen", ""
	},
	{
		&r_vertexLight, "r_vertexLight", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "disables lightmap texture blending",
		"Vertex lighting", CVARCAT_GRAPHICS, "Uses per-vertex lighting data instead of lightmaps", ""
	},
	{
		// note that r_subdivisions > 64 will create rendering artefacts because you'll see the other side of a curved surface when against it
		&r_subdivisions, "r_subdivisions", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "1", "64", help_r_subdivisions,
		"Patch tessellation step size", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Lower values produce smoother curves", ""
	},
	{
		&r_fullbright, "r_fullbright", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_fullbright,
		"Fullbright lighting", CVARCAT_GRAPHICS, "Lightmap textures get replaced by white/grey images", ""
	},
	{
		&r_lightmap, "r_lightmap", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_lightmap,
		"Draw lightmaps", CVARCAT_GRAPHICS, "Draws lightmap data only when available", ""
	},
	{
		&r_lightmapGreyscale, "r_lightmapGreyscale", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_FLOAT, "0", "1", "how desaturated the lightmap looks",
		"Lightmap desaturation", CVARCAT_GRAPHICS, "Desaturates the lightmap data", ""
	},
	{
		&r_depthFade, "r_depthFade", "1", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_depthFade,
		"Depth fade", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Prevents transparent surfaces from creating sharp edges when \"cutting\" through opaque geometry", ""
	},
	{
		&r_dither, "r_dither", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_dither,
		"Dither", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Adds noise to fight color banding artifacts", ""
	},
	{
		&r_rtColorFormat, "r_rtColorFormat", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(RTCF_MAX), help_r_rtColorFormat,
		"Render target format", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Controls the number of bits per pixel for the RGBA channels", "",
		CVAR_GUI_VALUE("0", "R8G8B8A8", "High perf, standard quality")
		CVAR_GUI_VALUE("1", "R10G10B10A2", "High perf, better colors, worse alpha")
		CVAR_GUI_VALUE("2", "R16G16B16A16", "Low perf, better colors and alpha")
	},
	{
		&r_depthClamp, "r_depthClamp", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, help_r_depthClamp,
		"Depth clamping", CVARCAT_GRAPHICS, "Enable if you want a horizontal FOV larger than 130", ""
	},
	{
		&r_gpuPreference, "r_gpuPreference", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_INTEGER, "0", XSTRING(GPUPREF_MAX), help_r_gpuPreference,
		"GPU selection", CVARCAT_DISPLAY | CVARCAT_PERFORMANCE, "Choose between low-power and high-performance devices", "",
		CVAR_GUI_VALUE("0", "High performance", "")
		CVAR_GUI_VALUE("1", "Low power", "")
		CVAR_GUI_VALUE("2", "None", "")
	},
	{
		&r_vsync, "r_vsync", "0", CVAR_ARCHIVE | CVAR_LATCH, CVART_BOOL, NULL, NULL, "enables v-sync",
		"V-Sync", CVARCAT_DISPLAY | CVARCAT_PERFORMANCE, "Enabling locks the framerate to the monitor's refresh rate", ""
		CVAR_GUI_VALUE("0", "Frame cap", "The framerate is capped by CNQ3's own limiter")
		CVAR_GUI_VALUE("1", "V-Sync", "The framerate matches the monitor's refresh rate")
	},

	//
	// latched variables that can only change over a restart
	//
	{
		&r_displayRefresh, "r_displayRefresh", "0", CVAR_LATCH, CVART_INTEGER, "0", "480", S_COLOR_VAL "0 " S_COLOR_HELP "lets the driver decide",
		"Refresh rate", CVARCAT_DISPLAY, "0 to let the driver decide", "Only available in fullscreen with video mode change"
	},
	{
		&r_singleShader, "r_singleShader", "0", CVAR_CHEAT | CVAR_LATCH, CVART_BOOL, NULL, NULL, "forces the default shader on all world surfaces except the sky",
		"Force default shader", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "Forces it on all world surfaces except the sky", ""
	},

	//
	// archived variables that can change at any time
	//
	{
		&r_smaa, "r_smaa", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", "4", help_r_smaa,
		"SMAA", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Enhanced sub-pixel morphological anti-aliasing", "",
		CVAR_GUI_VALUE("0", "Disabled", "")
		CVAR_GUI_VALUE("1", "Low quality", "")
		CVAR_GUI_VALUE("2", "Medium quality", "")
		CVAR_GUI_VALUE("3", "High quality", "")
		CVAR_GUI_VALUE("4", "Ultra quality", "")
	},
	{
		&r_picmip, "r_picmip", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", "16", help_r_picmip,
		"Picmip", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Higher numbers make for blurrier textures", ""
	},
	{
		&r_blitMode, "r_blitMode", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", XSTRING(BLITMODE_MAX), help_r_blitMode,
		"Fullscreen blit mode", CVARCAT_DISPLAY, "Dictates how the image gets upsampled", "",
		CVAR_GUI_VALUE("0", "Scaled to fit", "Preserves aspect ratio -> black bars")
		CVAR_GUI_VALUE("1", "Centered", "No scaling at all")
		CVAR_GUI_VALUE("2", "Stretched", "Takes the entire screen -> no black bars")
	},
	{
		&r_lodbias, "r_lodbias", "-2", CVAR_ARCHIVE, CVART_INTEGER, "-2", "2", help_r_lodbias,
		"MD3 LoD bias", CVARCAT_GRAPHICS, "Applies to items and player models\nLower means more detail", ""
	},
	{
		&r_ignoreShaderSortKey, "r_ignoreShaderSortKey", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_ignoreShaderSortKey,
		"Ignore shader draw order", CVARCAT_GRAPHICS, "All transparent surfaces are sorted by depth", ""
	},
	{
		&r_fastsky, "r_fastsky", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_fastsky,
		"Fast sky", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Draws the sky and portal surfaces in black", ""
	},
	{
		&r_noportals, "r_noportals", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_noportals,
		"Disable portal rendering", CVARCAT_GRAPHICS, "Draws teleporter and mirror surfaces in black", ""
	},
	{
		&r_dynamiclight, "r_dynamiclight", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "enables dynamic lights",
		"Enable dynamic lights", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "For power-ups, muzzle flashes, rockets, explosions, ...", ""
	},
	{
		&r_lego, "r_lego", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "LEGO(R) texture filtering",
		"LEGO(R) textures", CVARCAT_GRAPHICS, "Makes textures look blocky", "Forces nearest neighbor texture filtering"
	},
	{
		&r_gamma, "r_gamma", "1.2", CVAR_ARCHIVE, CVART_FLOAT, "0.5", "3", help_r_gamma,
		"Screen gamma", CVARCAT_GRAPHICS, "", ""
	},
	{
		&r_greyscale, "r_greyscale", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", "how desaturated the final image looks",
		"Screen desaturation", CVARCAT_GRAPHICS, "", ""
	},
	{
		&r_ditherStrength, "r_ditherStrength", "1.0", CVAR_ARCHIVE, CVART_FLOAT, "0.125", "8.0", help_r_ditherStrength,
		"Dither strength", CVARCAT_GRAPHICS, "Amount of noise added to fight color banding", ""
	},
	{
		&r_transpSort, "r_transpSort", "0", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, help_r_transpSort,
		"Transparent surface sorting", CVARCAT_GRAPHICS, "", "",
		CVAR_GUI_VALUE("0", "Sort dynamic", "")
		CVAR_GUI_VALUE("1", "Sort all", "")
	},
	{
		&r_lodCurveError, "r_lodCurveError", "2000", CVAR_ARCHIVE, CVART_FLOAT, "250", "10000", "curved surfaces LOD scale",
		"Curve LoD scale", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Higher is more detailed", ""
	},
	{
		&r_mapGreyscale, "r_mapGreyscale", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", "how desaturated the map looks",
		"Non-CTF map desaturation", CVARCAT_GRAPHICS, "Desaturates non-CTF world surfaces", "CTF surfaces are the red/blue base banners/markers"
	},
	{
		&r_mapGreyscaleCTF, "r_mapGreyscaleCTF", "0", CVAR_ARCHIVE, CVART_FLOAT, "0", "1", help_r_mapGreyscaleCTF,
		"CTF map desaturation", CVARCAT_GRAPHICS, "Desaturates CTF world surfaces", "CTF surfaces are the red/blue base banners/markers"
	},
	{
		&r_teleporterFlash, "r_teleporterFlash", "1", CVAR_ARCHIVE, CVART_BOOL, NULL, NULL, "draws bright colors when being teleported",
		"Bright teleporter flash", CVARCAT_GRAPHICS, "Draws bright colors while being teleported", ""
	},
	{
		&r_sleepThreshold, "r_sleepThreshold", "2500", CVAR_ARCHIVE, CVART_INTEGER, "2000", "4000", help_r_sleepThreshold,
		"Frame sleep threshold", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Higher means more consistent frame times but higher CPU usage",
		"This is the time cushion (in microseconds) for frame sleep.\n"
		"It's a trade-off between frame time consistency and CPU usage.\n"
		"Set to 2000 if you have a struggling old/low-power CPU.\n"
		"2500 should be enough to deal with delayed thread wake-ups.\n"
		"Use the frame graph to confirm that higher values help on your system."
	},
	{
		&r_shadingRate, "r_shadingRate", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", "6", help_r_shadingRate,
		"Shading rate", CVARCAT_GRAPHICS | CVARCAT_PERFORMANCE, "Variable-Rate Shading (VRS) mode",
		"The numbers are the horizontal and vertical subsampling factors.\n"
		"1x1 is forced for the sky, nopicmipped sprites (e.g. simple items)\n"
		"and nopicmipped alpha tested surfaces (e.g. grates).\n"
		"If extended modes are not supported, 2x2 is used instead.\n"
		"Prefer horizontal subsampling as many maps have textures\n"
		"with thin horizontal lines, which become an aliased mess when\n"
		"vertically subsampled.",
		CVAR_GUI_VALUE("0", "Off", "1x horizontal, 1x vertical")
		CVAR_GUI_VALUE("1", "2x1", "2x horizontal, 1x vertical")
		CVAR_GUI_VALUE("2", "1x2", "1x horizontal, 2x vertical")
		CVAR_GUI_VALUE("3", "2x2", "2x horizontal, 2x vertical")
		CVAR_GUI_VALUE("4", "4x2", "4x horizontal, 2x vertical")
		CVAR_GUI_VALUE("5", "2x4", "2x horizontal, 4x vertical")
		CVAR_GUI_VALUE("6", "4x4", "4x horizontal, 4x vertical")
	},
	{
		&r_guiFont, "r_guiFont", "0", CVAR_ARCHIVE, CVART_INTEGER, "0", "2", help_r_guiFont,
		"GUI font", CVARCAT_GUI, "", "",
		CVAR_GUI_VALUE("0", "Proggy Clean (13px)", "")
		CVAR_GUI_VALUE("1", "Sweet16 Mono (16px)", "")
		CVAR_GUI_VALUE("2", "Custom Font File", "")
	},
	{
		&r_guiFontFile, "r_guiFontFile", "", CVAR_ARCHIVE, CVART_STRING, NULL, NULL, "custom font .ttf file",
		"GUI font file", CVARCAT_GUI, "Path of the custom .ttf font file", "", NULL
	},
	{
		&r_guiFontHeight, "r_guiFontHeight", "24", CVAR_ARCHIVE, CVART_INTEGER, "7", "48", "custom font height",
		"GUI font height", CVARCAT_GUI, "Height of the custom font", "", NULL
	},

	//
	// temporary variables that can change at any time
	//
	{
		&r_ambientScale, "r_ambientScale", "0.6", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "entity ambient light scale",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_directedScale, "r_directedScale", "1", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "entity directed light scale",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		// keeping it around in case we enable other mods again
		&r_uiFullScreen, "r_uifullscreen", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, NULL,
		"", 0, "", ""
	},
	{
		&r_debugLight, "r_debuglight", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "prints entity light values",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_debugSort, "r_debugSort", "0", CVAR_CHEAT, CVART_FLOAT, "0", NULL, "doesn't render shaders with a greater sort key",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_nocurves, "r_nocurves", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "doesn't render grid meshes",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_drawworld, "r_drawworld", "1", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables rendering of world surfaces",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_portalOnly, "r_portalOnly", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "only draws the mirrored plane",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_lodscale, "r_lodscale", "5", CVAR_CHEAT, CVART_FLOAT, "1", "20", "LOD scale for MD3 models",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_norefresh, "r_norefresh", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables 3D scene rendering",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_drawentities, "r_drawentities", "1", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "enables entity rendering",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_nocull, "r_nocull", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables frustum culling",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_novis, "r_novis", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "disables PVS usage",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_debugSurface, "r_debugSurface", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "draws collision models",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_clear, "r_clear", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "clears to violet instead of black",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_lockpvs, "r_lockpvs", "0", CVAR_CHEAT, CVART_BOOL, NULL, NULL, "helps visualize the current PVS' limits",
		"", CVARCAT_GRAPHICS | CVARCAT_DEBUGGING, "", ""
	},
	{
		&r_maxpolys, "r_maxpolys", XSTRING(DEFAULT_MAX_POLYS), 0, CVART_INTEGER, XSTRING(DEFAULT_MAX_POLYS), NULL, "maximum polygon count per frame",
		"Max poly count", 0, "Maximum polygon count per frame", ""
	},
	{
		&r_maxpolyverts, "r_maxpolyverts", XSTRING(DEFAULT_MAX_POLYVERTS), 0, CVART_INTEGER, XSTRING(DEFAULT_MAX_POLYVERTS), NULL, "maximum polygon vertex count per frame",
		"Max poly vertices", 0, "Maximum polygon vertex count per frame", ""
	},
	{
		&r_debugUI, "r_debugUI", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "displays the debug/profile GUI",
		"", 0, "", ""
	},
	{
		&r_debugInput, "r_debugInput", "0", CVAR_TEMP, CVART_BOOL, NULL, NULL, "routes input to the debug/profile GUI",
		"", 0, "", ""
	}
};


static void R_Register()
{
	ri.Cvar_RegisterTable( r_cvars, ARRAY_LEN(r_cvars) );
	ri.Cmd_RegisterTable( r_cmds, ARRAY_LEN(r_cmds) );
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

	renderPipeline->Init();

	R_InitImages();

	R_InitShaders();

	R_InitSkins();

	R_ModelInit();

	QSUBSYSTEM_INIT_DONE( "Renderer" );
}


static void RE_Shutdown( qbool destroyWindow )
{
	ri.Printf( PRINT_DEVELOPER, "RE_Shutdown( %i )\n", destroyWindow );

	R_ShutDownGUI();

	if ( tr.registered ) {
		ri.Cmd_UnregisterModule();
		renderPipeline->ShutDown( destroyWindow );
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


static void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, const byte *data, int client, qbool dirty )
{
	if ( !tr.registered )
		return;

	//@TODO: gal.UpdateScratch( tr.scratchImage[client], cols, rows, data, dirty );
	tr.scratchShader->stages[0]->bundle.image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, (cols - 0.5f) / cols, (rows - 0.5f) / rows, (qhandle_t)tr.scratchShader->index );
}


static qbool RE_Registered()
{
	return tr.registered;
}


static qbool RE_IsFrameSleepNeeded()
{
	return !Sys_V_IsVSynced();
}


static qbool RE_IsDepthClampEnabled()
{
	return r_depthClamp->integer != 0;
}


static void RE_ComputeCursorPosition( int* x, int* y )
{ 
	if ( r_fullscreen->integer != 1 || r_mode->integer != VIDEOMODE_UPSCALE ) {
		return;
	}

	if ( r_blitMode->integer == BLITMODE_CENTERED ) {
		*x -= (glInfo.winWidth - glConfig.vidWidth) / 2;
		*y -= (glInfo.winHeight - glConfig.vidHeight) / 2;
	} else if ( r_blitMode->integer == BLITMODE_STRETCHED ) {
		const float sx = (float)glConfig.vidWidth / (float)glInfo.winWidth;
		const float sy = (float)glConfig.vidHeight / (float)glInfo.winHeight;
		*x *= sx;
		*y *= sy;
	} else if ( r_blitMode->integer == BLITMODE_ASPECT ) {
		const float art = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
		const float arw = (float)glInfo.winWidth / (float)glInfo.winHeight;
		float wsx, wsy, s;
		if ( arw > art ) {
			wsx = art / arw;
			wsy = 1.0f;
			s = (float)glConfig.vidHeight / (float)glInfo.winHeight;
		} else {
			wsx = 1.0f;
			wsy = arw / art;
			s = (float)glConfig.vidWidth / (float)glInfo.winWidth;
		}
		const int x0 = (glInfo.winWidth - (glInfo.winWidth * wsx)) * 0.5f;
		const int y0 = (glInfo.winHeight - (glInfo.winHeight * wsy)) * 0.5f;
		*x -= x0;
		*y -= y0;
		*x *= s;
		*y *= s;
	} else {
		Q_assert( !"Invalid r_blitMode" );
	}
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

	re.Registered = RE_Registered;

	re.ShouldSleep = RE_IsFrameSleepNeeded;

	re.DepthClamp = RE_IsDepthClampEnabled;

	re.ComputeCursorPosition = RE_ComputeCursorPosition;

#if defined( QC )
	re.GetAdvertisements = RE_GetAdvertisements;
#endif

	return &re;
}

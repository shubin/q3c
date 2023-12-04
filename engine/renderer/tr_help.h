#define help_r_ext_max_anisotropy \
"max. allowed anisotropy ratio\n" \
"For anisotropic filtering to be enabled, this needs to be 2 or higher.\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "=  8- 16 tap filtering\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= 16- 32 tap filtering\n" \
S_COLOR_VAL "    8 " S_COLOR_HELP "= 32- 64 tap filtering\n" \
S_COLOR_VAL "   16 " S_COLOR_HELP "= 64-128 tap filtering"

#define help_r_picmip \
"lowest allowed mip level\n" \
"Lower number means sharper textures."

#define help_r_roundImagesDown \
"allows image downscaling before texture upload\n" \
"The maximum scale ratio is 2 on both the horizontal and vertical axis."

#define help_r_mode \
"video mode to use with " S_COLOR_CVAR "r_fullscreen " S_COLOR_VAL "1\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= No video mode change, desktop resolution\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= No video mode change, custom resolution, custom upscaling\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Video mode change, custom resolution\n" \
"Custom resolution uses " S_COLOR_CVAR "r_width " S_COLOR_HELP "and " S_COLOR_CVAR "r_height" S_COLOR_HELP ".\n" \
"Custom upscaling uses " S_COLOR_CVAR "r_blitMode" S_COLOR_HELP "."

#define help_r_blitMode \
"image upscaling mode for " S_COLOR_CVAR "r_mode " S_COLOR_VAL "1\n" \
"This will only be active with " S_COLOR_CVAR "r_fullscreen " S_COLOR_VAL "1 " S_COLOR_HELP "and " S_COLOR_CVAR "r_mode " S_COLOR_VAL "1" S_COLOR_HELP ".\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Aspect-ratio preserving upscale (black bars if necessary)\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= No scaling, centered\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Full-screen stretching (no black bars)"

#define help_r_subdivisions \
"tessellation step size for patch surfaces\n" \
"This sets the step size of a subdivision, *not* the number of subdivisions.\n" \
"In other words, lower values produce smoother curves."

#define help_r_gamma \
"gamma correction factor\n" \
S_COLOR_HELP "   <" S_COLOR_VAL "1 " S_COLOR_HELP "= Darker\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= No change\n" \
S_COLOR_HELP "   >" S_COLOR_VAL "1 " S_COLOR_HELP "= Brighter"

#define help_r_lodbias \
"MD3 models LOD bias\n" \
"For all MD3 models, including player characters.\n" \
"A higher number means a higher quality loss. " S_COLOR_VAL "0 " S_COLOR_HELP "means no loss."

#define help_r_fastsky \
"makes the sky and portals pure black\n" \
"Portal example: the bottom teleporter on q3dm7."

#define help_r_noportals \
"disables rendering of portals\n" \
"Portal example: the bottom teleporter on q3dm7."

#define help_r_lightmap \
"renders the lightmaps only\n" \
"Shaders with a lightmap stage will only draw the lightmap stage.\n" \
"This is mutually exclusive with " S_COLOR_CVAR "r_fullbright" S_COLOR_HELP "."

#define help_r_fullbright \
"renders the diffuse textures only\n" \
"Shaders with a lightmap stage will not draw the lightmap stage.\n" \
"This is mutually exclusive with " S_COLOR_CVAR "r_lightmap" S_COLOR_HELP "."

#define help_r_mipGenFilter \
"GPU mip-map filter\n" \
S_COLOR_VAL "    L4  " S_COLOR_HELP "= Lanczos 4\n" \
S_COLOR_VAL "    L3  " S_COLOR_HELP "= Lanczos 3\n" \
S_COLOR_VAL "    BL  " S_COLOR_HELP "= Bi-linear\n" \
S_COLOR_VAL "    MN2 " S_COLOR_HELP "= Mitchell-Netravali 2 (B = 1/3, C = 1/3)\n" \
S_COLOR_VAL "    BH4 " S_COLOR_HELP "= 3-term Blackman-Harris 4\n" \
S_COLOR_VAL "    BH3 " S_COLOR_HELP "= 3-term Blackman-Harris 3\n" \
S_COLOR_VAL "    BH2 " S_COLOR_HELP "= 3-term Blackman-Harris 2\n" \
S_COLOR_VAL "    T2  " S_COLOR_HELP "= Tent 2 (1/3 2/3, same as the CPU version)\n" \
"The numbers specify the 'radius' of the window filters."

#define help_r_mipGenGamma \
"GPU mip-map gamma\n" \
"Specifies the gamma space in which the textures are stored.\n" \
"This is only used by GPU-side mip-map generation (" S_COLOR_CVAR "r_gpuMipGen " S_COLOR_VAL "1" S_COLOR_HELP ").\n" \
"This should really be around " S_COLOR_VAL "2.4 " S_COLOR_HELP "(CRT screens),\n" \
"but defaults lower to not break the looks of some things\n" \
"(e.g. cpm3a's teleporter).\n" \
"While " S_COLOR_VAL "1.0 " S_COLOR_HELP "will match the game's original look,\n" \
"higher values will better preserve contrast."

#define help_r_depthFade \
"enables depth fade rendering\n" \
"With it, surfaces like blood, smoke, etc don't 'cut' through geometry sharply."

#define help_r_dither \
"enables dithering\n" \
"Introduces noise to fight color banding artifacts.\n" \
"The strength of the noise is controlled by " S_COLOR_CVAR "r_ditherStrength" S_COLOR_HELP "."

#define help_r_ditherStrength \
"dithering noise strength\n" \
"The dithering on/off switch is " S_COLOR_CVAR "r_dither" S_COLOR_HELP "."

#define help_r_transpSort \
"transparency sorting mode\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Sort dynamic transparent surfaces\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Sort all transparent surfaces\n" \
"The drawback of " S_COLOR_CVAR "r_transpSort " S_COLOR_VAL "1 " S_COLOR_HELP "is that gameplay legibility " \
"might suffer and results might look quite inconsistent based on the view angles and surface dimensions.\n" \
"Example: dropped items in the cpm18r acid with " S_COLOR_CVAR "cg_simpleItems " S_COLOR_VAL "1"

#define help_r_rtColorFormat \
"render target color format\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= R8G8B8A8\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= R10G10B10A2\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= R16G16B16A16"

#define help_r_depthClamp \
"enables depth clamping\n" \
"Enable this if you want " S_COLOR_CVAR "cg_fov " S_COLOR_HELP "to be higher than " S_COLOR_VAL "130" S_COLOR_HELP ".\n" \
"Vertices don't get clipped against the near and far clip planes.\n" \
"It turns the view frustum into a pyramid and prevents any vertex between\n" \
"the near clip plane and the camera's position from getting clipped\n" \
"(but will still clip anything behind the camera),\n" \
"preventing solid objects too close to the camera from being see-through.\n" \
"Because depth values are clamped, improper depth ordering can occur."

#define help_r_gpuPreference \
"sets the GPU selection preference\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= High performance\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Low power\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= None"

#define help_r_colorMipLevels \
"colorizes textures based on their mip level"

#define help_r_mapGreyscaleCTF \
"how desaturated CTF map surfaces look\n" \
"It applies to the red/blue color-coded surfaces of popular CTF/NTF maps."

#define help_r_sleepThreshold \
"time cushion (in us) for frame sleep\n" \
"This is a trade-off between frame time consistency and CPU usage.\n" \
"Set to " S_COLOR_VAL "2000 " S_COLOR_HELP "if you have a struggling old/low-power CPU.\n" \
S_COLOR_VAL "2500 " S_COLOR_HELP "should be enough to deal with delayed thread wake-ups.\n" \
"Use the frame graph to confirm that higher values help on your system."

#define help_r_shadingRate \
"variable-rate shading (VRS) mode\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= 1x1 (VRS off)\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= 2x1 (base mode)\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= 1x2 (base mode)\n" \
S_COLOR_VAL "    3 " S_COLOR_HELP "= 2x2 (base mode)\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= 4x2 (extended mode)\n" \
S_COLOR_VAL "    5 " S_COLOR_HELP "= 2x4 (extended mode)\n" \
S_COLOR_VAL "    6 " S_COLOR_HELP "= 4x4 (extended mode)\n" \
"The numbers are the horizontal and vertical subsampling factors.\n" \
"1x1 is forced for the sky, nopicmipped sprites (e.g. simple items)\n" \
"and nopicmipped alpha tested surfaces (e.g. grates).\n" \
"If extended modes are not supported, 2x2 is used instead.\n" \
"Prefer horizontal subsampling as many maps have textures\n" \
"with thin horizontal lines, which become an aliased mess when\n" \
"vertically subsampled."

#define help_r_guiFont \
"font for CNQ3's GUI\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Proggy Clean (13px)\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Sweet16 Mono (16px)\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Custom Font\n" \
"Custom: " S_COLOR_CVAR "r_guiFontFile" S_COLOR_HELP ", " S_COLOR_CVAR "r_guiFontHeight"

#define help_r_ignoreShaderSortKey \
"ignores the shader sort key of transparent surfaces\n" \
"Instead, it sorts by depth and original registration order only.\n" \
"You can use this as a work-around for broken maps like bones_fkd_b4."

#define help_r_showtris_bitmask \
S_COLOR_VAL "     1 " S_COLOR_HELP "= Enables the feature\n" \
S_COLOR_VAL "     2 " S_COLOR_HELP "= Only draws for visible triangles\n" \
S_COLOR_VAL "     4 " S_COLOR_HELP "= Draws for backfacing triangles\n" \
S_COLOR_VAL "     8 " S_COLOR_HELP "= Uses the original vertex color\n" \
S_COLOR_VAL "    16 " S_COLOR_HELP "= Uses the original vertex alpha as vertex color"

#define help_r_showtris \
"draws wireframe triangles\n" \
help_r_showtris_bitmask

#define help_r_shownormals \
"draws vertex normals\n" \
help_r_showtris_bitmask

#define help_r_smaa \
"enables SMAA\n" \
"SMAA is Enhanced Subpixel Morphological Antialiasing\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= No SMAA\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Low quality\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Medium quality\n" \
S_COLOR_VAL "    3 " S_COLOR_HELP "= High quality\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= Ultra quality"

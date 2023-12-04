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
// tr_image.c
#include "tr_local.h"
#include <setjmp.h>


#if defined (_MSC_VER)
#	pragma warning (disable: 4611) // setjmp and C++ destructors
#endif


// colors are pre-multiplied, alpha indicates whether blending should occur
const vec4_t r_mipBlendColors[16] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f },
	{ 0.5f, 0.0f, 0.0f, 1.0f },
	{ 0.0f, 0.5f, 0.0f, 1.0f },
	{ 0.0f, 0.0f, 0.5f, 1.0f }
};


#define IMAGE_HASH_SIZE 1024
static image_t* hashTable[IMAGE_HASH_SIZE];


void R_ImageList_f( void )
{
	const char* const match = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : NULL;

	ri.Printf( PRINT_ALL, "\nwide high MPI W format name\n" );

	int totalByteCount = 0;
	int imageCount = 0;
	for ( int i = 0; i < tr.numImages; ++i ) {
		const image_t* image = tr.images[i];

		if ( match && !Com_Filter( match, image->name ) )
			continue;

		const int byteCount = image->width * image->height * (image->format == TF_RGBA8 ? 4 : 1);
		if ( !(image->flags & IMG_NOMIPMAP) && (image->width > 1) && (image->height > 1) )
			totalByteCount += (byteCount * 4) / 3; // not exact, but good enough
		else
			totalByteCount += byteCount;
		imageCount++;

		ri.Printf( PRINT_ALL, "%4i %4i %c%c%c ",
			image->width, image->height,
			(image->flags & IMG_NOMIPMAP) ? ' ' : 'M',
			(image->flags & IMG_NOPICMIP) ? ' ' : 'P',
			(image->flags & IMG_NOIMANIP) ? ' ' : 'I'
			);

		switch ( image->wrapClampMode ) {
			case TW_REPEAT:        ri.Printf( PRINT_ALL, "R " ); break;
			case TW_CLAMP_TO_EDGE: ri.Printf( PRINT_ALL, "E " ); break;
			default:               ri.Printf( PRINT_ALL, "? " ); break;
		}

		switch ( image->format ) {
			case TF_RGBA8: ri.Printf( PRINT_ALL, "RGBA8 " );              break;
			default:       ri.Printf( PRINT_ALL, "%5i ", image->format ); break;
		}

		ri.Printf( PRINT_ALL, " %s\n", image->name );
	}

	ri.Printf( PRINT_ALL, "---------\n" );
	ri.Printf( PRINT_ALL, "%i images found\n", imageCount );
	ri.Printf( PRINT_ALL, "Estimated VRAM use: %s\n\n", Com_FormatBytes( totalByteCount ) );
}


void R_ImageInfo_f()
{
	if ( Cmd_Argc() <= 1 ) {
		ri.Printf( PRINT_ALL, "usage: %s <imagepath>\n", Cmd_Argv(0) );
		return;
	}

	const char* const name = Cmd_Argv(1);
	const image_t* image = NULL;
	int imageIndex = -1;
	for ( int i = 0; i < tr.numImages; i++ ) {
		if ( !Q_stricmp( tr.images[i]->name, name ) ) {
			image = tr.images[i];
			imageIndex = i;
			break;
		}
	}

	if ( imageIndex < 0 ) {
		ri.Printf( PRINT_ALL, "image not found\n" );
		return;
	}

	char pakName[256];
	if ( FS_GetPakPath( pakName, sizeof( pakName ), image->pakChecksum ) ) {
		ri.Printf( PRINT_ALL, "%s/%s\n", pakName, image->name );
	}

	ri.Printf( PRINT_ALL, "Used in these shaders:\n" );
	for ( int is = 0; is < ARRAY_LEN( tr.imageShaders ); ++is ) {
		const int i = tr.imageShaders[is] & 0xFFFF;
		if ( i != imageIndex ) {
			continue;
		}

		const int s = (tr.imageShaders[is] >> 16) & 0xFFFF;
		const shader_t* const shader = tr.shaders[s];
		const qbool nmmS = shader->imgflags & IMG_NOMIPMAP;
		const qbool npmS = shader->imgflags & IMG_NOPICMIP;
		ri.Printf( PRINT_ALL, "%s %s %s\n",
					nmmS ? "NMM" : "   ",
					npmS ? "NPM" : "   ",
					shader->name );

		const char* const shaderPath = R_GetShaderPath( shader );
		if ( shaderPath != NULL ) {
			ri.Printf( PRINT_ALL, "        -> %s\n", shaderPath );
		}
	}
}


///////////////////////////////////////////////////////////////


int R_ComputeMipCount( int scaled_width, int scaled_height )
{
	int mipCount = 1;
	while ( scaled_width > 1 || scaled_height > 1 ) {
		scaled_width = max( scaled_width >> 1, 1 );
		scaled_height = max( scaled_height >> 1, 1 );
		++mipCount;
	}

	return mipCount;
}


// note that the "32" here is for the image's STRIDE - it has nothing to do with the actual COMPONENTS

static void Upload32( image_t* image, unsigned int* data )
{
	// atlases we generate ourselves
	if ( image->flags & IMG_LMATLAS ) {
		image->flags |= IMG_NOMIPMAP;
		image->flags |= IMG_NOAF;
		renderPipeline->CreateTexture( image, 1, image->width, image->height );
		return;
	}

	// atlases loaded from images on disk
	if ( Q_stristr( image->name, "maps/" ) == image->name &&
		Q_stristr( image->name + 5, "/lm_" ) != NULL ) {
		image->flags |= IMG_NOPICMIP;
		image->flags |= IMG_NOMIPMAP;
		image->flags |= IMG_NOAF;
		image->flags |= IMG_EXTLMATLAS;
		if ( r_mapBrightness->value != 1.0f ) {
			const int pixelCount = image->width * image->height;
			byte* pixel = (byte*)data;
			byte* const pixelEnd = (byte*)( data + pixelCount );
			while ( pixel < pixelEnd ) {
				R_ColorShiftLightingBytes( pixel, pixel );
				pixel += 4;
			}
		}
	}

	const char* npmImageDirs[] = {
		"textures/npmenv/",
		"textures/npmpads/"
	};
	for ( int i = 0; i < ARRAY_LEN( npmImageDirs ); ++i ) {
		if ( !Q_stricmpn( image->name, npmImageDirs[i], strlen( npmImageDirs[i] ) ) ) {
			image->flags |= IMG_NOPICMIP;
			break;
		}
	}

	int scaled_width, scaled_height;

	// convert to exact power of 2 sizes
	//
	for ( scaled_width = 1; scaled_width < image->width; scaled_width <<= 1 )
		;
	for ( scaled_height = 1; scaled_height < image->height; scaled_height <<=1 )
		;
	if ( r_roundImagesDown->integer && scaled_width > image->width )
		scaled_width >>= 1;
	if ( r_roundImagesDown->integer && scaled_height > image->height )
		scaled_height >>= 1;

	if ( scaled_width != image->width || scaled_height != image->height ) {
		byte* resampled;
		R_ResampleImage( &resampled, scaled_width, scaled_height, (const byte*)data, image->width, image->height, image->wrapClampMode );
		data = (unsigned int*)resampled;
		image->width = scaled_width;
		image->height = scaled_height;
		ri.Printf( PRINT_DEVELOPER, "^3WARNING: ^7'%s' doesn't have PoT dimensions.\n", image->name );
	}

	// perform optional picmip operation
	if ( !(image->flags & IMG_NOPICMIP) ) {
		scaled_width >>= r_picmip->integer;
		scaled_height >>= r_picmip->integer;
	}

	// clamp to minimum size
	scaled_width = max( scaled_width, 1 );
	scaled_height = max( scaled_height, 1 );

	// clamp to the current upper OpenGL limit
	// scale both axis down equally so we don't have to
	// deal with a half mip resampling
	//
	while ( scaled_width > glInfo.maxTextureSize || scaled_height > glInfo.maxTextureSize ) {
		scaled_width >>= 1;
		scaled_height >>= 1;
	}

	const int w = image->width;
	const int h = image->height;
	int mipCount = R_ComputeMipCount( w, h );
	if ( image->format != TF_RGBA8 )
		image->flags |= IMG_NOMIPMAP;
	if ( image->flags & IMG_NOMIPMAP )
		mipCount = 1;
	renderPipeline->CreateTexture( image, mipCount, w, h );
	renderPipeline->UpoadTextureAndGenerateMipMaps( image, (const byte*)data );
}


// this is the only way any image_t are created
// !!! i'm pretty sure this DOESN'T work correctly for non-POT images

image_t* R_CreateImage( const char* name, byte* pic, int width, int height, textureFormat_t format, int flags, textureWrap_t glWrapClampMode )
{
	if (strlen(name) >= MAX_QPATH)
		ri.Error( ERR_DROP, "R_CreateImage: \"%s\" is too long\n", name );

	if ( tr.numImages == MAX_DRAWIMAGES )
		ri.Error( ERR_DROP, "R_CreateImage: MAX_DRAWIMAGES hit\n" );

	image_t* image = tr.images[tr.numImages] = RI_New<image_t>();

	strcpy( image->name, name );

	image->format = format;
	image->flags = flags;

	image->width = width;
	image->height = height;
	image->wrapClampMode = glWrapClampMode;

	image->index = tr.numImages;
	image->numShaders = 0;
	image->flags0 = 0;
	image->flags1 = 0;

	tr.numImages++;

	Upload32( image, (unsigned int*)pic );	

	// KHB  there are times we have no interest in naming an image at all (notably, font glyphs)
	// but atm the rest of the system is too dependent on everything being named
	//if (name) {
		int hash = Q_FileHash(name, IMAGE_HASH_SIZE);
		image->next = hashTable[hash];
		hashTable[hash] = image;
	//}

	return image;
}


static qbool ValidateImageResolution( int w, int h, const char* fileName )
{
	const int dim[2] = { w, h };
	const char* const dimNames[2] = { "width", "height" };

	for (int i = 0; i < 2; ++i) {
		if (dim[i] <= 0) {
			ri.Printf( PRINT_WARNING, "%s: %s (%d) is too small\n", fileName, dimNames[i], dim[i] );
			return qfalse;
		}

		if (dim[i] > MAX_TEXTURE_SIZE) {
			ri.Printf( PRINT_WARNING, "%s: %s (%d) is too large (max. %d)\n",
			           fileName, dimNames[i], dim[i], MAX_TEXTURE_SIZE );
			return qfalse;
		}
	}

	return qtrue;
}


static qbool LoadTGA( const char* fileName, byte* buffer, int len, byte** pic, int* w, int* h, textureFormat_t* format )
{
	*pic = NULL;

	byte* p = buffer;
	byte* const pEnd = buffer + len; // 1 byte past the end

	TargaHeader targa_header;
	targa_header.id_length = p[0];
	targa_header.colormap_type = p[1];
	targa_header.image_type = p[2];
	targa_header.width = LittleShort( *(short*)&p[12] );
	targa_header.height = LittleShort( *(short*)&p[14] );
	targa_header.pixel_size = p[16];
	targa_header.attributes = p[17];

	// skip the header and the comment, if any
	p += sizeof(TargaHeader);
	if (targa_header.id_length != 0)
		p += targa_header.id_length;

#define ERROR(Msg, ...) ri.Printf(PRINT_WARNING, Msg "\n", ##__VA_ARGS__); return qfalse

	if (targa_header.image_type != 2 && targa_header.image_type != 10 && targa_header.image_type != 3) {
		ERROR( "LoadTGA %s: Only type 2, 10 and 3 images supported", fileName );
	}

	if (targa_header.colormap_type) {
		ERROR( "LoadTGA %s: Colormaps are not supported", fileName );
	}

	if (targa_header.image_type != 3 && targa_header.pixel_size != 32 && targa_header.pixel_size != 24) {
		ERROR( "LoadTGA %s: Only 32-bit and 24-bit color images are supported", fileName );
	}

	if (targa_header.image_type == 3 && targa_header.pixel_size != 8) {
		ERROR( "LoadTGA %s: Only 8-bit greyscale images are supported", fileName );
	}

	const int bpp = targa_header.pixel_size / 8;
	const int width = targa_header.width;
	const int height = targa_header.height;
	const unsigned numBytes = width * height * 4;

	if (!ValidateImageResolution( width, height, fileName ))
		return qfalse;

	if (numBytes / (width * 4) != height) {
		ERROR( "LoadTGA %s: Invalid image size", fileName );
	}

#undef ERROR

	*pic = (byte*)ri.Malloc( numBytes );
	*w = width;
	*h = height;
	*format = TF_RGBA8;

#define UNMUNGE_PIXEL       { dst[2] = pixel[0]; dst[1] = pixel[1]; dst[0] = pixel[2]; dst[3] = pixel[3]; dst += 4; }
#define WRAP_ROW            if ((++x == width) && y--) { x = 0; dst = *pic + y*width*4; }
#define RANGE_CHECK(Bytes)  if (p + (Bytes) > pEnd) { ri.Printf( PRINT_WARNING, "LoadTGA %s: Truncated file\n", fileName ); ri.Free( *pic ); *pic = NULL; return qfalse; }

	// uncompressed luminance
	if (targa_header.image_type == 3) {
		RANGE_CHECK( width * height )
		for (int y = height-1; y >= 0; --y) {
			byte* dst = *pic + y*width * 4;
			for (int x = 0; x < width; ++x) {
				const byte l = *p;
				dst[0] = l;
				dst[1] = l;
				dst[2] = l;
				dst[3] = 255;
				dst += 4;
				p += 1;
			}
		}
	// uncompressed BGRA
	} else if (targa_header.image_type == 2 && bpp == 4) {
		RANGE_CHECK( width * height * 4 )
		for (int y = height-1; y >= 0; --y) {
			byte* dst = *pic + y*width * 4;
			for (int x = 0; x < width; ++x) {
				dst[2] = p[0];
				dst[1] = p[1];
				dst[0] = p[2];
				dst[3] = p[3];
				dst += 4;
				p += 4;
			}
		}
	// uncompressed BGR
	} else if (targa_header.image_type == 2 && bpp == 3) {
		RANGE_CHECK( width * height * 3 )
		for (int y = height-1; y >= 0; --y) {
			byte* dst = *pic + y*width * 4;
			for (int x = 0; x < width; ++x) {
				dst[2] = p[0];
				dst[1] = p[1];
				dst[0] = p[2];
				dst[3] = 255;
				dst += 4;
				p += 3;
			}
		}
	// RLE_BGRA and RLE_BGR
	} else if (targa_header.image_type == 10) {
		byte pixel[4] = { 0, 0, 0, 255 };
		int y = height-1;
		while (y >= 0) {
			byte* dst = *pic + y*width * 4;
			int x = 0;
			while (x < width) {
				RANGE_CHECK( 1 )
				const int rle = *p++;
				int n = 1 + (rle & 0x7F);
				if (rle & 0x80) {
					// RLE packet: 1 pixel repeated n times
					RANGE_CHECK( bpp )
					for (int i = 0; i < bpp; ++i)
						pixel[i] = *p++;
					while (n--) {
						UNMUNGE_PIXEL
						WRAP_ROW
					}
				} else {
					// n distinct pixels
					RANGE_CHECK( bpp * n )
					while (n--) {
						for (int i = 0; i < bpp; ++i)
							pixel[i] = *p++;
						UNMUNGE_PIXEL
						WRAP_ROW
					}
				}
			}
		}
	}

#undef WRAP_ROW
#undef UNMUNGE_PIXEL
#undef RANGE_CHECK

	if (targa_header.attributes & 0x20)
		ri.Printf( PRINT_WARNING, "LoadTGA %s: Top-down declaration ignored\n", fileName );

	return qtrue;
}


///////////////////////////////////////////////////////////////


typedef struct {
	jmp_buf		jumpBuffer;
	const char*	fileName;
	qbool		load;
} engineJPEGInfo_t;

// The only memory allocation function pointers we can override are the ones exposed in jpeg_memory_mgr.
// The problem is that it's the wrong layer for us: we want to replace malloc and free,
// not change how the pooling of allocations works.
// We are therefore re-implementing jmemnobs.c to use the engine's allocator.
extern "C"
{
	#define JPEG_INTERNALS
	#include "../libjpeg-turbo/jinclude.h"
	#include "../libjpeg-turbo/jpeglib.h"
	#include "../libjpeg-turbo/jmemsys.h"

	void* jpeg_get_small( j_common_ptr cinfo, size_t sizeofobject ) { return (void*)ri.Malloc(sizeofobject); }
	void jpeg_free_small( j_common_ptr cinfo, void* object, size_t sizeofobject ) { ri.Free(object); }
	void* jpeg_get_large( j_common_ptr cinfo, size_t sizeofobject ) { return jpeg_get_small( cinfo, sizeofobject ); }
	void jpeg_free_large( j_common_ptr cinfo, void* object, size_t sizeofobject ) { jpeg_free_small( cinfo, object, sizeofobject ); }
	size_t jpeg_mem_available( j_common_ptr cinfo, size_t min_bytes_needed, size_t max_bytes_needed, size_t already_allocated ) { return max_bytes_needed; }
	void jpeg_open_backing_store( j_common_ptr cinfo, backing_store_ptr info, long total_bytes_needed ) { ERREXIT(cinfo, JERR_NO_BACKING_STORE); }
	long jpeg_mem_init( j_common_ptr cinfo) { return 0; }
	void jpeg_mem_term( j_common_ptr cinfo) {}

	void error_exit( j_common_ptr cinfo )
	{
		char buffer[JMSG_LENGTH_MAX];
		(*cinfo->err->format_message)(cinfo, buffer);
		engineJPEGInfo_t* const extra = (engineJPEGInfo_t*)cinfo->client_data;
		ri.Printf(PRINT_WARNING, "libjpeg-turbo: couldn't %s %s: %s\n", extra->load ? "load" : "save", extra->fileName, buffer);
		jpeg_destroy(cinfo);
		longjmp(extra->jumpBuffer, -1);
	}

	void output_message( j_common_ptr cinfo )
	{
		char buffer[JMSG_LENGTH_MAX];
		(*cinfo->err->format_message)(cinfo, buffer);
		const engineJPEGInfo_t* const extra = (const engineJPEGInfo_t*)cinfo->client_data;
		ri.Printf(PRINT_ALL, "libjpeg-turbo: while %s %s: %s\n", extra->load ? "loading" : "saving", extra->fileName, buffer);
	}
};


static qbool LoadJPG( const char* fileName, byte* buffer, int len, byte** pic, int* w, int* h, textureFormat_t* format )
{
	jpeg_decompress_struct cinfo;
	jpeg_error_mgr jerr;
	engineJPEGInfo_t extra;

	*pic = NULL;

	if (setjmp( extra.jumpBuffer )) {
		jpeg_destroy_decompress( &cinfo );
		if (*pic != NULL) {
			ri.Free( *pic );
			*pic = NULL;
		}
		return qfalse;
	}

	extra.load = qtrue;
	extra.fileName = fileName;
	cinfo.err = jpeg_std_error( &jerr );
	cinfo.err->error_exit = &error_exit;
	cinfo.err->output_message = &output_message;
	cinfo.client_data = &extra;
	jpeg_create_decompress( &cinfo );

	jpeg_mem_src( &cinfo, buffer, len );

	jpeg_read_header( &cinfo, TRUE );
	jpeg_start_decompress( &cinfo );
	if (!ValidateImageResolution( cinfo.output_width, cinfo.output_height, fileName )) {
		longjmp( extra.jumpBuffer, -1 );
	}

	const unsigned numBytes = cinfo.output_width * cinfo.output_height * 4;
	*pic = (byte*)ri.Malloc(numBytes);
	*w = cinfo.output_width;
	*h = cinfo.output_height;

	// We set JCS_EXT_RGBA to instruct libjpeg-turbo to always
	// write the alpha value as 255.
	cinfo.out_color_space = JCS_EXT_RGBA;
	cinfo.output_components = 4;

	// go for speed
	cinfo.dither_mode = JDITHER_NONE;
	cinfo.dct_method = JDCT_FASTEST;
	cinfo.do_fancy_upsampling = FALSE;

	const unsigned rowStride = cinfo.output_width * 4;
	JSAMPROW rowPointer = *pic;
	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines( &cinfo, &rowPointer, 1 );
		rowPointer += rowStride;
	}

	jpeg_finish_decompress( &cinfo );
	jpeg_destroy_decompress( &cinfo );

	*format = TF_RGBA8;

	return qtrue;
}


int SaveJPGToBuffer( byte* out, int quality, int image_width, int image_height, byte* image_buffer )
{
	static const char* fileName = "memory buffer";

	jpeg_compress_struct cinfo;
	jpeg_error_mgr jerr;
	engineJPEGInfo_t extra;

	if (setjmp( extra.jumpBuffer )) {
		jpeg_destroy_compress( &cinfo );
		return qfalse;
	}

	extra.load = qfalse;
	extra.fileName = fileName;
	cinfo.err = jpeg_std_error( &jerr );
	cinfo.err->error_exit = &error_exit;
	cinfo.err->output_message = &output_message;
	cinfo.client_data = &extra;
	jpeg_create_compress( &cinfo );

	// jpeg_mem_dest only calls malloc if both outSize and outBuffer are 0
	unsigned long outSize = image_width * image_height * 4;
	unsigned char* outBuffer = out;
	jpeg_mem_dest( &cinfo, &outBuffer, &outSize );

	cinfo.image_width = image_width;
	cinfo.image_height = image_height;
	cinfo.input_components = 4;
	cinfo.in_color_space = JCS_EXT_RGBA;

	jpeg_set_defaults( &cinfo );
	jpeg_set_quality( &cinfo, quality, TRUE );

	jpeg_start_compress( &cinfo, TRUE );

	const unsigned rowStride = image_width * 4;
	JSAMPROW rowPointer = image_buffer + (cinfo.image_height - 1) * rowStride;
	while (cinfo.next_scanline < cinfo.image_height) {
		jpeg_write_scanlines( &cinfo, &rowPointer, 1 );
		rowPointer -= rowStride;
	}

	jpeg_finish_compress( &cinfo );

	const int csize = (int)(cinfo.dest->next_output_byte - outBuffer);

	jpeg_destroy_compress( &cinfo );

	return csize;
}


///////////////////////////////////////////////////////////////


extern qbool LoadSTB( const char* fileName, byte* buffer, int len, byte** pic, int* w, int* h, textureFormat_t* format );

typedef qbool (*imageLoaderFunc)( const char* fileName, byte* buffer, int len, byte** pic, int* w, int* h, textureFormat_t* format );

typedef struct {
	const char*		extension;
	imageLoaderFunc	function;
} imageLoader_t;

static const imageLoader_t imageLoaders[] = {
	{ ".jpg",	&LoadJPG },
	{ ".tga",	&LoadTGA },
	{ ".png",	&LoadSTB },
	{ ".jpeg",	&LoadJPG }
};


static void R_LoadImage( int* pakChecksum, const char* name, byte** pic, int* w, int* h, textureFormat_t* format )
{
	*pic = NULL;
	*w = 0;
	*h = 0;

	const int loaderCount = ARRAY_LEN( imageLoaders );
	char altName[MAX_QPATH];

	byte* buffer;
	int bufferSize = ri.FS_ReadFilePak( name, (void**)&buffer, pakChecksum );
	if ( buffer == NULL ) {
		const char* lastDot = strrchr( name, '.' );
		const int nameLength = lastDot != NULL ? (int)(lastDot - name) : (int)strlen( name );
		if ( nameLength >= MAX_QPATH )
			return;

		for ( int i = 0; i < loaderCount; ++i ) {
			memcpy( altName, name, nameLength );
			altName[nameLength] = '\0';
			Q_strcat( altName, sizeof(altName), imageLoaders[i].extension );
			bufferSize = ri.FS_ReadFilePak( altName, (void**)&buffer, pakChecksum );
			if ( buffer != NULL ) {
				name = altName;
				break;
			}
		}

		if ( buffer == NULL )
			return;
	}

	const int nameLength = (int)strlen( name );
	for ( int i = 0; i < loaderCount; ++i ) {
		const int extLength = (int)strlen( imageLoaders[i].extension );
		if ( extLength < nameLength &&
			 Q_stricmp(name + nameLength - extLength, imageLoaders[i].extension) == 0 ) {
			(*imageLoaders[i].function)( name, buffer, bufferSize, pic, w, h, format );
			break;
		}
	}

	ri.FS_FreeFile( buffer );
}


struct forcedLoadImage_t {
	const char* mapName;
	const char* shaderName;
	int shaderNameHash;
};

// map-specific fixes for textures that are used with different (incompatible) settings
static const forcedLoadImage_t g_forcedLoadImages[] = {
	{ "ct3ctf1", "textures/ct3ctf1/grate_02.tga", 716 }
};


// finds or loads the given image - returns NULL if it fails, not a default image

image_t* R_FindImageFile( const char* name, int flags, textureWrap_t glWrapClampMode )
{
	if ( !name )
		return NULL;
	
	qbool forcedLoad = qfalse;
	const int hash = Q_FileHash( name, IMAGE_HASH_SIZE );
	const int forcedLoadImageCount = ARRAY_LEN( g_forcedLoadImages );
	for ( int i = 0; i < forcedLoadImageCount; ++i ) {
		const forcedLoadImage_t* const fli = g_forcedLoadImages + i;
		if ( hash == fli->shaderNameHash &&
			 strcmp( R_GetMapName(), fli->mapName ) == 0 &&
			 strcmp( name, fli->shaderName ) == 0 ) {
		   forcedLoad = qtrue;
		   break;
		}
	}
	
	// see if the image is already loaded
	//
	if ( !forcedLoad ) {
		image_t* image;
		for ( image = hashTable[hash]; image; image=image->next ) {
			if ( strcmp( name, image->name ) )
				continue;

			// let's not whine about those since we rightfully ignore what the user asks for anyhow
			if ( image->flags & IMG_EXTLMATLAS )
				return image;

			if ( !strcmp( name, "*white" ) )
				return image;

			// since this WASN'T enforced as an error, half the shaders out there (including most of id's)
			// have been getting it wrong for years
			// the white image can be used with any set of parms, but other mismatches are errors
			if ( (image->flags & IMG_NOMIPMAP) != (flags & IMG_NOMIPMAP) ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed nomipmap settings\n", name );
			}
			if ( (image->flags & IMG_NOPICMIP) != (flags & IMG_NOPICMIP) ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed nopicmip settings\n", name );
			}
			if ( image->wrapClampMode != glWrapClampMode ) {
				ri.Printf( PRINT_DEVELOPER, "WARNING: reused image %s with mixed clamp settings (map vs clampMap)\n", name );
			}

			return image;
		}
	}

	// load the pic from disk
	//
	byte* pic;
	int width, height, pakChecksum;
	textureFormat_t format;
	R_LoadImage( &pakChecksum, name, &pic, &width, &height, &format );

	if ( !pic )
		return NULL;

	image_t* const image = R_CreateImage( name, pic, width, height, format, flags, glWrapClampMode );
	image->pakChecksum = pakChecksum;
	ri.Free( pic );
	return image;
}


static void R_CreateDefaultImage()
{
	const int DEFAULT_SIZE = 16;
	byte data[DEFAULT_SIZE][DEFAULT_SIZE][4];

	// the default image is a box showing increasing s and t
	Com_Memset( data, 32, sizeof( data ) );

	for ( int i = 0; i < DEFAULT_SIZE; ++i ) {
		byte b = (byte)( 64 + (128 * i / DEFAULT_SIZE) );
		data[0][i][0] = b;
		data[0][i][3] = 255;
		data[i][0][1] = b;
		data[i][0][3] = 255;
		data[i][i][0] = data[i][i][1] = b;
		data[i][i][3] = 255;
	}

	tr.defaultImage = R_CreateImage( "*default", (byte*)data, DEFAULT_SIZE, DEFAULT_SIZE, TF_RGBA8, IMG_NOPICMIP | IMG_NOAF, TW_REPEAT );
}


static void R_CreateBuiltinImages()
{
	int i;
	byte data[4];

	R_CreateDefaultImage();

	// we use a solid white image instead of disabling texturing
	Com_Memset( data, 255, 4 );
	tr.whiteImage = R_CreateImage( "*white", data, 1, 1, TF_RGBA8, IMG_NOMIPMAP | IMG_NOAF, TW_REPEAT );

	const byte mapBrightness = (byte)( min( r_mapBrightness->value, 1.0f ) * 255.0f );
	data[0] = data[1] = data[2] = mapBrightness;
	tr.fullBrightImage = R_CreateImage( "*fullBright", data, 1, 1, TF_RGBA8, IMG_NOMIPMAP | IMG_NOAF, TW_REPEAT );

	// scratchimages usually used for cinematic drawing (signal-quality effects)
	// these are just placeholders: RE_StretchRaw will regenerate them when it wants them
	for (i = 0; i < ARRAY_LEN(tr.scratchImage); ++i)
		tr.scratchImage[i] = R_CreateImage( "*scratch", data, 1, 1, TF_RGBA8, IMG_NOMIPMAP | IMG_NOPICMIP, TW_CLAMP_TO_EDGE );
}


void R_SetColorMappings()
{
	tr.identityLight = 1.0f / r_brightness->value;
	tr.identityLightByte = (int)( 255.0f * tr.identityLight );
}


void R_InitImages()
{
	Com_Memset( hashTable, 0, sizeof(hashTable) );
	R_SetColorMappings(); // build brightness translation tables
	R_CreateBuiltinImages(); // create default textures (white, fog, etc)
}


/*
============================================================================

SKINS

============================================================================
*/


// unfortunatly, skin files aren't compatible with our normal parsing rules. oops  :/

static const char* CommaParse( const char** data )
{
	static char com_token[MAX_TOKEN_CHARS];

	int c = 0;
	const char* p = *data;

	while (*p && (*p < 32))
		++p;

	while ((*p > 32) && (*p != ',') && (c < MAX_TOKEN_CHARS-1))
		com_token[c++] = *p++;

	*data = p;
	com_token[c] = 0;
	return com_token;
}


qhandle_t RE_RegisterSkin( const char* name )
{
	if (!name || !name[0] || (strlen(name) >= MAX_QPATH))
		ri.Error( ERR_DROP, "RE_RegisterSkin: invalid name [%s]\n", name ? name : "NULL" );

	skin_t* skin;
	qhandle_t hSkin;
	// see if the skin is already loaded
	for (hSkin = 1; hSkin < tr.numSkins; ++hSkin) {
		skin = tr.skins[hSkin];
		if ( !Q_stricmp( skin->name, name ) ) {
			return (skin->numSurfaces ? hSkin : 0);
		}
	}

	// allocate a new skin
	if ( tr.numSkins == MAX_SKINS ) {
		ri.Printf( PRINT_WARNING, "WARNING: RE_RegisterSkin( '%s' ) MAX_SKINS hit\n", name );
		return 0;
	}

	tr.numSkins++;
	skin = RI_New<skin_t>();
	tr.skins[hSkin] = skin;
	Q_strncpyz( skin->name, name, sizeof( skin->name ) );
	skin->numSurfaces = 0;

	// make sure the render thread is stopped
	// KHB why? we're not uploading anything...   R_SyncRenderThread();

	// if not a .skin file, load as a single shader
	if ( Q_stricmpn( name + strlen( name ) - 5, ".skin", 6 ) ) {
		skin->numSurfaces = 1;
		skin->surfaces[0] = RI_New<skinSurface_t>();
		skin->surfaces[0]->shader = R_FindShader( name, LIGHTMAP_NONE, FINDSHADER_MIPRAWIMAGE_BIT );
		return hSkin;
	}

	char* text;
	// load and parse the skin file
	ri.FS_ReadFile( name, (void **)&text );
	if (!text)
		return 0;

	const char* token;
	const char* p = text;
	char surfName[MAX_QPATH];

	while (p && *p) {
		// get surface name
		token = CommaParse( &p );
		Q_strncpyz( surfName, token, sizeof( surfName ) );

		if ( !token[0] )
			break;

		// lowercase the surface name so skin compares are faster
		Q_strlwr( surfName );

		if (*p == ',')
			++p;

		if ( strstr( token, "tag_" ) )
			continue;

		// parse the shader name
		token = CommaParse( &p );

		skinSurface_t* surf = skin->surfaces[ skin->numSurfaces ] = RI_New<skinSurface_t>();
		Q_strncpyz( surf->name, surfName, sizeof( surf->name ) );
		surf->shader = R_FindShader( token, LIGHTMAP_NONE, FINDSHADER_MIPRAWIMAGE_BIT );
		skin->numSurfaces++;
	}

	ri.FS_FreeFile( text );

	return (skin->numSurfaces ? hSkin : 0); // never let a skin have 0 shaders
}


void R_InitSkins()
{
	tr.numSkins = 1;

	// make the default skin have all default shaders
	tr.skins[0] = RI_New<skin_t>();
	tr.skins[0]->numSurfaces = 1;
	tr.skins[0]->surfaces[0] = RI_New<skinSurface_t>();
	tr.skins[0]->surfaces[0]->shader = tr.defaultShader;
	Q_strncpyz( tr.skins[0]->name, "<default skin>", sizeof( tr.skins[0]->name ) );
}


const skin_t* R_GetSkinByHandle( qhandle_t hSkin )
{
	return ((hSkin > 0) && (hSkin < tr.numSkins) ? tr.skins[hSkin] : tr.skins[0]);
}


void R_SkinList_f( void )
{
	ri.Printf( PRINT_ALL, "------------------\n" );

	const char* const match = Cmd_Argc() > 1 ? Cmd_Argv( 1 ) : NULL;

	int skinCount = 0;
	for (int i = 0; i < tr.numSkins; ++i) {
		const skin_t* skin = tr.skins[i];

		if ( match && !Com_Filter( match, skin->name ) )
			continue;

		skinCount++;
		ri.Printf( PRINT_ALL, "%3i:%s\n", i, skin->name );
		for (int j = 0; j < skin->numSurfaces; ++j) {
			ri.Printf( PRINT_ALL, "       %s = %s\n",
				skin->surfaces[j]->name, skin->surfaces[j]->shader->name );
		}
	}

	ri.Printf( PRINT_ALL, "%i skins found\n", skinCount );
	ri.Printf( PRINT_ALL, "------------------\n" );
}


void R_AddImageShader( image_t* image, shader_t* shader )
{
	assert( shader != NULL );
	if (image == NULL || shader == NULL)
		return;

	assert( tr.numImageShaders < ARRAY_LEN( tr.imageShaders ) );
	if (tr.numImageShaders >= ARRAY_LEN( tr.imageShaders ))
		return;

	const int imageShader = (shader->index << 16) | image->index;
	for (int is = 0; is < tr.numImageShaders; ++is) {
		if (tr.imageShaders[is] == imageShader)
			return;
	}

	// we consider index 0 to be invalid
	if (tr.numImageShaders == 0)
		tr.numImageShaders++;

	tr.imageShaders[tr.numImageShaders++] = imageShader;
	image->numShaders++;
	image->flags0 |= shader->imgflags ^ -1;
	image->flags1 |= shader->imgflags;
}

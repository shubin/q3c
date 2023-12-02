/*
===========================================================================
Copyright (C) 2017-2022 Gian 'myT' Schellenbaum

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
// implements LoadSTB, which is the interface between CNQ3 and stb_image

#include "tr_local.h"

#if defined(_MSC_VER)
#	pragma warning(disable: 4505) // unreferenced local function
#	pragma warning(disable: 4459) // declaration of 'ri' hides global declaration
#endif

static void*	q_malloc( size_t );
static void		q_free( void* );
static void*	q_realloc_sized( void*, size_t, size_t );

#define STBI_MALLOC				q_malloc
#define STBI_REALLOC_SIZED		q_realloc_sized
#define STBI_FREE				q_free
#define STBI_MAX_DIMENSIONS		MAX_TEXTURE_SIZE
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


static void* q_malloc( size_t bytes )
{
	return ri.Malloc((int)bytes);
}


static void q_free( void* buffer )
{
	if (buffer != NULL)
		ri.Free(buffer);
}


static void* q_realloc_sized( void* ptr, size_t oldSize, size_t newSize )
{
	if (ptr == NULL)
		return q_malloc(newSize);

	if (newSize <= oldSize)
		return ptr;

	void* ptr2 = q_malloc(newSize);
	memcpy(ptr2, ptr, oldSize);
	q_free(ptr);

	return ptr2;
}


qbool LoadSTB( const char* fileName, byte* buffer, int len, byte** pic, int* w, int* h, textureFormat_t* format )
{
	int comp;
	*pic = (byte*)stbi_load_from_memory(buffer, len, w, h, &comp, 4);
	if (*pic == NULL) {
		ri.Printf(PRINT_WARNING, "stb_image: couldn't load %s: %s\n", fileName, stbi_failure_reason());
		return qfalse;
	}
		
	*format = TF_RGBA8;

	return qtrue;
}

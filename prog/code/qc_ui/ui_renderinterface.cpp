#include "../qcommon/q_shared.h"
#include "ui_public.h"
#undef DotProduct

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/RenderInterface.h>

#include "ui_local.h"

using namespace Rml;

// see grp_nuklear.cpp
#define MAX_NUKLEAR_VERTEX_COUNT    (1024 * 1024)
#define MAX_NUKLEAR_INDEX_COUNT     ( 256 * 1024)
#define MAX_NUKLEAR_COMMAND_COUNT	(  64 * 1024)

QRenderInterface::QRenderInterface()
	: mScissorEnabled( false ), mWholeScreen{ 0 }, mScissor{ 0 }
	, mVertices( 0 ), mIndices( 0 ), mCommands( 0 ), pCurrentVertex( 0 )
	, pCurrentIndex( 0 ), pCurrentCommand( 0 )
{
}

QRenderInterface::~QRenderInterface() {
}

// Rml::Vertex field order has been changed to match the CNQ3's NuklearVertex
// see RmlUi/Core/Vertex.h
void QRenderInterface::RenderGeometry( Vertex *vertices, int num_vertices, int *indices, int num_indices, TextureHandle texture, const Vector2f &translation ) {
	uint32_t indexOffset = pCurrentVertex - mVertices;

	if ( pCurrentCommand - mCommands + 1 > MAX_NUKLEAR_COMMAND_COUNT ) {
		return;
	}

	if ( pCurrentVertex - mVertices + num_vertices > MAX_NUKLEAR_VERTEX_COUNT ) {
		return;
	}

	if ( pCurrentIndex - mIndices + num_indices > MAX_NUKLEAR_INDEX_COUNT ) {
		return;
	}

	pCurrentCommand->firstIndex = pCurrentIndex - mIndices;
	pCurrentCommand->numIndices = num_indices;
	pCurrentCommand->shader = (qhandle_t)texture;
	pCurrentCommand->scissor = mScissorEnabled ? mScissor : mWholeScreen;
	pCurrentCommand++;

	for ( int i = 0; i < num_indices; i++ ) {
		pCurrentIndex[i] = indices[i] + indexOffset;
	}
	pCurrentIndex += num_indices;

	for ( int i = 0; i < num_vertices; i++ ) {
		pCurrentVertex[i] = vertices[i];
		pCurrentVertex[i].position += translation;
	}
	pCurrentVertex += num_vertices;
}

void QRenderInterface::EnableScissorRegion( bool enable ) {
	mScissorEnabled = enable;
}

void QRenderInterface::SetScissorRegion( int x, int y, int width, int height ) {
	mScissor.x = x;
	mScissor.y = y;
	mScissor.w = width;
	mScissor.h = height;
}

bool QRenderInterface::LoadTexture( TextureHandle &texture_handle, Vector2i &texture_dimensions, const String &source ) {
	texture_handle = trap_R_RegisterShaderNoMip( source.c_str() );
	if ( texture_handle == 0 ) {
		return false;
	}
	int width, height;
	trap_R_GetShaderImageDimensions( texture_handle, 0, 0, &width, &height );
	texture_dimensions.x = width;
	texture_dimensions.y = height;
	return true;
}

bool QRenderInterface::GenerateTexture( TextureHandle &texture_handle, const byte *source, const Vector2i &source_dimensions ) {
	texture_handle = trap_R_CreateTextureFromMemory( source_dimensions.x, source_dimensions.y, (const void*)source );
	return texture_handle != 0;
}

void QRenderInterface::ReleaseTexture( TextureHandle texture ) {
}

void QRenderInterface::SetTransform( const Matrix4f *transform ) {
}

void QRenderInterface::Initialize( int width, int height ) {
	mVertices = new Vertex[MAX_NUKLEAR_VERTEX_COUNT];
	mIndices = new uint32_t[MAX_NUKLEAR_INDEX_COUNT];
	mCommands = new QRenderCmd[MAX_NUKLEAR_COMMAND_COUNT];
	pCurrentVertex = mVertices;
	pCurrentIndex = mIndices;
	pCurrentCommand = mCommands;
	mWholeScreen.w = width;
	mWholeScreen.h = height;
}

void QRenderInterface::Shutdown() {
	delete mCommands;
	delete[] mIndices;
	delete[] mVertices;
}

void QRenderInterface::Flush() {
	trap_NK_Upload( mVertices, sizeof( Vertex ) * ( pCurrentVertex - mVertices ), mIndices, sizeof( uint32_t ) * ( pCurrentIndex - mIndices ) );
	for ( QRenderCmd *cmd = mCommands; cmd < pCurrentCommand; cmd++ ) {
		trap_NK_Draw( cmd->firstIndex, cmd->numIndices, cmd->shader, (const int*)&cmd->scissor );
	}
	pCurrentVertex = mVertices;
	pCurrentIndex = mIndices;
	pCurrentCommand = mCommands;
}

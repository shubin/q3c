#include "../qcommon/q_shared.h"
#include "ui_public.h"
#undef DotProduct

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/RenderInterface.h>

#include "ui_local.h"

using namespace Rml;



struct QDrawCmd {
	int x, y, w, h; // scissor

};

QRenderInterface::QRenderInterface() {
}

QRenderInterface::~QRenderInterface() {
}

// Rml::Vertex field order has been changed to match the CNQ3's NuklearVertex
// see Core/Vertex.h
void QRenderInterface::RenderGeometry( Vertex *vertices, int num_vertices, int *indices, int num_indices, TextureHandle texture, const Vector2f &translation ) {
}

void QRenderInterface::EnableScissorRegion( bool enable ) {
}

void QRenderInterface::SetScissorRegion( int x, int y, int width, int height ) {
}

bool QRenderInterface::LoadTexture( TextureHandle &texture_handle, Vector2i &texture_dimensions, const String &source ) {
	return true;
}

bool QRenderInterface::GenerateTexture( TextureHandle &texture_handle, const byte *source, const Vector2i &source_dimensions ) {
	return true;
}

void QRenderInterface::ReleaseTexture( TextureHandle texture ) {
}

void QRenderInterface::SetTransform( const Matrix4f *transform ) {
}

#include "ui_local.h"
#undef DotProduct
#include <RmlUi/Core.h>
#include <RmlUi/Lua.h>

class Q_SystemInterface: public Rml::SystemInterface {
	virtual double GetElapsedTime() override {
		return trap_Milliseconds() / 1000.0;
	}

	virtual bool LogMessage( Rml::Log::Type type, const Rml::String &message ) override {
		static char buf[1024];
		snprintf( buf, ARRAY_LEN( buf ), S_COLOR_YELLOW "RmlUi> " S_COLOR_WHITE "%s\n", message.c_str());
		trap_Print( buf );
		return true;
	}
};

class Q_RenderInterface: public Rml::RenderInterface {
public:
	virtual void RenderGeometry( Rml::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::TextureHandle texture, const Rml::Vector2f &translation ) override {
		vec2_t tr;
		tr[0] = translation.x;
		tr[1] = translation.y;

		trap_R_RenderGeometry( (void*)vertices, num_vertices, indices, num_indices, (int)texture,  tr );
	}

	virtual Rml::CompiledGeometryHandle CompileGeometry( Rml::Vertex *vertices, int num_vertices, int *indices, int num_indices, Rml::TextureHandle texture ) override {
		return 0;
	}

	virtual void RenderCompiledGeometry( Rml::CompiledGeometryHandle geometry, const Rml::Vector2f &translation ) override {
	}

	virtual void ReleaseCompiledGeometry( Rml::CompiledGeometryHandle geometry ) {
	}

	virtual void EnableScissorRegion( bool enable ) override {
		trap_R_EnableScissor( enable ? qtrue : qfalse );
	}

	virtual void SetScissorRegion( int x, int y, int width, int height ) override {
		trap_R_SetScissor( x, y, width, height );
	}

	virtual bool LoadTexture( Rml::TextureHandle &texture_handle, Rml::Vector2i &texture_dimensions, const Rml::String &source ) override {
		texture_handle = trap_R_LoadTexture( source.c_str(), &texture_dimensions.x, &texture_dimensions.y );
		return true;
	}

	virtual bool GenerateTexture( Rml::TextureHandle &texture_handle, const byte *source, const Rml::Vector2i &source_dimensions ) override {
		texture_handle = trap_R_UploadTexture( source, source_dimensions.x, source_dimensions.y );
		return true;
	};

	virtual void ReleaseTexture( Rml::TextureHandle texture ) override {
	};

	virtual void SetTransform( const Rml::Matrix4f *transform ) override {
		trap_R_SetMatrix( transform ? transform->data() : NULL );
	}
};

class Q_FileInterface: public Rml::FileInterface {
	struct FileHandle {
		fileHandle_t f;
		int pos, size;
	};

	virtual Rml::FileHandle Open( const Rml::String &path ) override {
		FileHandle *handle = (FileHandle*)mspace_malloc( ui_mspace, sizeof( FileHandle ) );
		handle->pos = 0;
		handle->size = trap_FS_FOpenFile( path.c_str(), &handle->f, FS_READ );
		if ( handle->f == 0 ) {
			mspace_free( ui_mspace, handle );
			return 0;
		}
		return (Rml::FileHandle)handle;
	}

	virtual void Close( Rml::FileHandle file ) override {
		FileHandle *handle = (FileHandle*)file;
		trap_FS_FCloseFile( handle->f );
		mspace_free( ui_mspace, handle );
	}

	virtual size_t Read( void *buffer, size_t size, Rml::FileHandle file ) override {
		FileHandle *handle = (FileHandle*)file;
		trap_FS_Read( buffer, (int)size, handle->f );
		int oldpos = handle->pos;
		handle->pos += size;
		if ( handle->pos > handle->size ) {
			handle->pos = handle->size;
		}
		return handle->pos - oldpos;
	}

	virtual bool Seek( Rml::FileHandle file, long offset, int origin ) override {
		FileHandle *handle = (FileHandle*)file;
		int orig = 0;
		switch ( origin ) {
			case SEEK_SET: orig = FS_SEEK_SET; break;
			case SEEK_END: orig = FS_SEEK_END; break;
			case SEEK_CUR: orig = FS_SEEK_CUR; break;
		}
		handle->pos = trap_FS_Seek( handle->f, offset, origin );
		return true;
	}

	virtual size_t Tell( Rml::FileHandle file ) override {
		FileHandle *handle = (FileHandle*)file;
		return handle->pos;
	}

	virtual size_t Length( Rml::FileHandle file ) override {
		FileHandle *handle = (FileHandle*)file;
		return handle->size;
	}

	virtual bool LoadFile( const Rml::String &path, Rml::String &out_data ) override {
		fileHandle_t f;
		int len = trap_FS_FOpenFile( path.c_str(), &f, FS_READ );
		if ( f == -1 ) {
			return false;
		}
		char *buf = (char*)mspace_malloc( ui_mspace, len );
		trap_FS_Read( buf, len, f );
		out_data.assign( buf, len );
		mspace_free( ui_mspace, buf );
		return true;
	}
};

static Q_SystemInterface g_systemInterface;
static Q_RenderInterface g_renderInterface;
static Q_FileInterface g_fileInterface;

void UI_InitRML( lua_State *L ) {
	Rml::SetSystemInterface( &g_systemInterface );
	Rml::SetFileInterface( &g_fileInterface );
	Rml::SetRenderInterface( &g_renderInterface );
	Rml::Initialise();
	Rml::Lua::Initialise( L );
}

void UI_ShutdownRML( void ) {
	Rml::Shutdown();
}

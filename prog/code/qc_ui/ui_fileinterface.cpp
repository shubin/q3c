extern "C" {
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "ui_public.h"
}
#undef DotProduct

#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/FileInterface.h>

#include "ui_local.h"

namespace {

struct FileData {
	fileHandle_t	qfile;
	int				size;
	int				pos;
};

}

using namespace Rml;

QFileInterface::QFileInterface() {
}

QFileInterface::~QFileInterface() {
}

FileHandle QFileInterface::Open( const String &path ) {
	FileData *fd = new FileData();

	fd->size = trap_FS_FOpenFile( path.c_str(), &fd->qfile, FS_READ );
	if ( fd->qfile == 0 ) {
		delete fd;
		return 0;
	}
	fd->pos = 0;
	return (FileHandle)fd;
}

void QFileInterface::Close( FileHandle file ) {
	FileData *fd = (FileData*)file;
	if ( fd != NULL ) {
		trap_FS_FCloseFile( fd->qfile );
		delete fd;
	}
}

size_t QFileInterface::Read( void *buffer, size_t size, FileHandle file ) {
	FileData *fd = (FileData*)file;

	if ( fd == NULL ) {
		trap_Error( "qcui: QFileInterface::Read: wrong file handle" );
		return 0;
	}

	if ( fd->pos + size > fd->size ) {
		size -= fd->pos + size - fd->size;
	}

	if ( size > 0 ) {
		trap_FS_Read( buffer, ( int )size, fd->qfile );
		fd->pos += size;
	}
	return size;
}

bool QFileInterface::Seek( FileHandle file, long offset, int origin ) {
	trap_Error( "qcui: QFileInterface::Seek: not supported" );
	return false;
}

size_t QFileInterface::Tell( FileHandle file ) {
	FileData *fd = ( FileData * )file;

	if ( fd == NULL ) {
		trap_Error( "qcui: QFileInterface::Tell: wrong file handle" );
		return 0;
	}

	return fd->pos;
}

size_t QFileInterface::Length( FileHandle file ) {
	FileData *fd = (FileData*)file;

	if ( fd == NULL ) {
		trap_Error( "qcui: QFileInterface::Read: wrong file handle" );
		return 0;
	}

	return fd->size;
}

bool QFileInterface::LoadFile( const String &path, String &out_data ) {	
	fileHandle_t qfile;
	int size;

	size = trap_FS_FOpenFile( path.c_str(), &qfile, FS_READ );
	if ( qfile == 0 ) {
		return false;
	}
	out_data.resize( size );
	trap_FS_Read( &out_data[0], size, qfile );
	trap_FS_FCloseFile( qfile );
	return true;
}

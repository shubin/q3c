extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"

#include <string>
#include <map>

const std::map<std::string, std::string> messages;

struct reader {
	fileHandle_t handle;
	int len, bytesread;
	char buffer[256];
	char *ptr, *end;

	reader():handle( 0 ) {}

	~reader() {
		if ( handle ) {
			trap_FS_FCloseFile( handle );
		}
	}

	bool open( const char *filename ) {
		len = trap_FS_FOpenFile( filename, &handle, FS_READ );
		if ( len < 0 ) {
			handle = 0;
			return false;
		}
		bytesread = 0;
		ptr = buffer;
		end = buffer;
		return true;
	}

	void bufferize() {
		if ( ptr != end ) {
			return;
		}

		int amount = len - bytesread;
		if ( amount > sizeof( buffer ) ) {
			amount = sizeof( buffer );
		}
		trap_FS_Read( buffer, amount, handle );
		ptr = buffer;
		end = ptr + amount;
	}

	int getchar() {
		if ( bytesread == len ) {
			return -1;
		}
		bufferize();
		return *ptr++;
	}

};

void UI_LoadLocalisation( const char *path ) {
}

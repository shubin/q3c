extern "C" {
	#include "../qcommon/q_shared.h"
}
#include "ui_public.h"
#include "ui_local.h"

#include <string>
#include <unordered_map>
#include <sstream>

static std::unordered_map<std::string, std::string> messages;

bool UI_LocaliseString( std::string &translated, const std::string &input ) {
	auto it = messages.find( std::string( input ) );
	if ( it == messages.end() ) {
		return false;
	}
	translated = it->second;
	return true;
}

#define RDBUFSIZE 1024

struct reader {
	fileHandle_t handle;
	int len, bytesread;
	unsigned char buffer[RDBUFSIZE];
	unsigned char *ptr, *end;
	int line;

	reader( fileHandle_t handle, int len )
		:handle( handle ), len( len ), bytesread( 0 )
		,ptr( buffer ) ,end( buffer ), line( 0 )
	{
		bufferize();
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
		int c;
		if ( bytesread == len ) {
			return -1;
		}
		bufferize();
		c = *ptr;
		ptr++;
		bytesread++;
		if ( c == '\n' ) {
			line++;
		}
		return c;
	}
};

typedef enum {
	T_END,
	T_ERROR,
	T_ATOM,
	T_STRING,
} token_type;

struct lexer {
	enum {
		S_ERROR,
		S_IDLE,
		S_END,
		S_COMMENT,
		S_RDATOM,
		S_RDSTRING,
		S_ESCAPE,
	} state;

	reader &rd;

	lexer( reader &rd ):rd( rd ), state( S_IDLE ) {}

	token_type get( std::string &str ) {
		std::stringstream ss;
		str.clear();
		while ( true ) {
			int c;
			switch ( state ) {
			case S_ERROR:
				return T_ERROR;
			case S_IDLE:
				c = rd.getchar();
				if ( c == -1 ) {
					state = S_END;
					return T_END;
				}
				if ( isspace( c ) || c < ' ' ) {
					break;
				}
				if ( c == '#' ) {
					state = S_COMMENT;
					break;
				}
				if ( c == '\"' ) {
					state = S_RDSTRING;
					break;
				}
				if ( !isalpha( c ) ) {
					state = S_ERROR;
					return T_ERROR;
				}
				ss << ( char )c;
				state = S_RDATOM;
				break;
			case S_END:
				return T_END;
			case S_COMMENT:
				c = rd.getchar();
				if ( c == -1 ) {
					state = S_END;
					return T_END;
				}
				if ( c == '\n' ) {
					state = S_IDLE;
				}
				break;
			case S_RDATOM:
				c = rd.getchar();
				if ( c == -1 ) {
					str = ss.str();
					state = S_END;
					return T_ATOM;
				}
				if ( isspace( c ) ) {
					str = ss.str();
					state = S_IDLE;
					return T_ATOM;
				}
				if ( isalnum( c ) ) {
					ss << ( char )c;
					break;
				}
				return T_ERROR;
			case S_RDSTRING:
				c = rd.getchar();
				if ( c == -1 ) {
					state = S_ERROR;
					return T_ERROR;
				}
				if ( c == '\"' ) {
					state = S_IDLE;
					str = ss.str();
					return T_STRING;
				}
				if ( c == '\\' ) {
					state = S_ESCAPE;
					break;
				}
				if ( c < ' ' ) {
					state = S_ERROR;
					return T_ERROR;
				}
				ss << ( char )c;
				state = S_RDSTRING;
				break;
			case S_ESCAPE:
				c = rd.getchar();
				if ( c == -1 ) {
					state = S_ERROR;
					return T_ERROR;
				}
				if ( c == 'n' ) {
					ss << '\n';
					state = S_RDSTRING;
					break;
				}
				if ( c < ' ' ) {
					state = S_ERROR;
					return T_ERROR;
				}
				ss << ( char )c;
				state = S_RDSTRING;
				break;
			}
		}
	}
};

qboolean UI_LoadLocalisation( const char *path ) {
	fileHandle_t handle;
	int len;
	if ( ( len = trap_FS_FOpenFile( path, &handle, FS_READ ) ) < 0 ) {
		trap_Print( va( "^1Error: cannot open localisation file: %s\n", path ) );
		return qfalse;
	}
	reader rd( handle, len );
	lexer lx( rd );

	enum {
		S_ERROR,
		S_INIT,
		S_WAITID,
		S_RDID,
		S_WAITMSG,
		S_RDMSG,
		S_END,
	} state;

	state = S_INIT;
	token_type tok;
	std::string tokstr;

	std::string msgid;
	std::stringstream msgstr;

	std::unordered_map<std::string, std::string> messages;

	while ( state != S_END && state != S_ERROR ) {
		switch ( state ) {
		case S_INIT:
			tok = lx.get( tokstr );
			state = S_WAITID;
			break;
		case S_WAITID:
			if ( tok == T_END ) { state = S_END; break; }
			if ( tok == T_ERROR ) { state = S_ERROR; break; }
			if ( tok == T_ATOM ) {
				if ( tokstr == "msgid" ) {
					state = S_RDID;
					break;
				}
				trap_Print( va( "^1Error parsing localisation file: msgid expected, got %s\n", tokstr.c_str() ) );
				state = S_ERROR;
				break;
			}
			trap_Print( va( "^1Error parsing localisation file: msgid expected, got token %d (%s)\n", tok, tokstr.c_str() ) );
			state = S_ERROR;
			break;
		case S_RDID:
			if ( msgid.size() ) {
				messages[msgid] = msgstr.str();
				msgstr.str( "" );
				msgid.clear();
			}
			tok = lx.get( tokstr );
			if ( tok == T_END ) { state = S_END; break; }
			if ( tok == T_ERROR ) { state = S_ERROR; break; }
			if ( tok == T_STRING ) {
				msgid = tokstr;
				state = S_WAITMSG;
				break;
			}
			trap_Print( va( "^1Error parsing localisation file: msgid value expected, got token %d (%s)\n", tok, tokstr.c_str() ));
			state = S_ERROR;
			break;
		case S_WAITMSG:
			tok = lx.get( tokstr );
			if ( tok == T_END ) { state = S_END; break; }
			if ( tok == T_ERROR ) { state = S_ERROR; break; }
			if ( tok == T_ATOM ) {
				if ( tokstr == "msgstr" ) {
					state = S_RDMSG;
					break;
				}
				trap_Print( va( "^1Error parsing localisation file: msgstr expected, got %s\n", tokstr.c_str() ) );
				state = S_ERROR;
				break;
			}
			trap_Print( va( "^1Error parsing localisation file: msgstr expected, got token %d (%s)\n", tok, tokstr.c_str() ) );
			state = S_ERROR;
			break;
		case S_RDMSG:
			tok = lx.get( tokstr );
			if ( tok == T_END ) { state = S_END; break; }
			if ( tok == T_ERROR ) { state = S_ERROR; break; }
			if ( tok == T_STRING ) {
				msgstr << tokstr;
				state = S_RDMSG;
				break;
			}
			if ( tok == T_ATOM ) {
				state = S_WAITID;
				break;
			}
			trap_Print( va( "^1Error parsing localisation file: msgstr value or msgid expected, got token %d (%s)\n", tok, tokstr.c_str() ) );
			state = S_ERROR;
			break;
		}
	}
	trap_FS_FCloseFile( handle );

	if ( state == S_END ) {
		if ( msgid.size() ) {
			messages[msgid] = msgstr.str();
		}
		::messages.swap( messages );
		return qtrue;
	}
	trap_Print( "^1Error parsing localisation file\n" );
	return qfalse;
}

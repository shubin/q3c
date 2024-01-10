#include "ui_poparser.h"

#include <string>
#include <sstream>
#include <stdarg.h>

#define RDBUFSIZE 1024

static std::string format( const char *format, ... ) {
	std::string result;
	va_list ap;
	result.resize( 1024 );
	va_start( ap, format );
	vsnprintf( result.data(), result.size(), format, ap );
	va_end( ap );
	return result;
}

struct reader {
	po_read read;
	void *file;
	int len;
	
	int bytesread;
	unsigned char buffer[RDBUFSIZE];
	unsigned char *ptr, *end;
	int line;

	reader( po_read read, void *file, int len )
		:read( read ), file( file ), len( len ), bytesread( 0 )
		, ptr( buffer ), end( buffer ), line( 0 ) {
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
		read( file, buffer, amount );
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

bool po_parse( po_read read, int len, void *file, po_addmsg addmsg, void *storage, po_error error ) {
	reader rd( read, file, len );
	lexer lx( rd );

	enum {
		S_ERROR,
		S_INIT,
		S_WAITID,
		S_RDID,
		S_RDIDADD,
		S_WAITMSG,
		S_WAITMSG_NOREAD,
		S_RDMSG,
		S_RDMSGADD,
		S_END,
	} state;

	state = S_INIT;
	token_type tok;
	std::string tokstr;

	std::stringstream msgid;
	std::stringstream msgstr;

	while ( state != S_END && state != S_ERROR ) {
		switch ( state ) {
		case S_INIT:
			tok = lx.get( tokstr );
			state = S_WAITID;
			break;
		case S_WAITID:
			if ( tok == T_END ) {
				state = S_END; break;
			}
			if ( tok == T_ERROR ) {
				state = S_ERROR; break;
			}
			if ( tok == T_ATOM ) {
				if ( tokstr == "msgid" ) {
					state = S_RDID;
					break;
				}
				error( format( "^1Error parsing localisation file: msgid expected, got %s\n", tokstr.c_str() ).c_str() );
				state = S_ERROR;
				break;
			}
			error( format( "^1Error parsing localisation file: msgid expected, got token %d (%s)\n", tok, tokstr.c_str() ).c_str() );
			state = S_ERROR;
			break;
		case S_RDIDADD:
		case S_RDID:
			tok = lx.get( tokstr );
			if ( tok == T_END ) {
				state = S_END; break;
			}
			if ( tok == T_ERROR ) {
				state = S_ERROR; break;
			}
			if ( tok == T_STRING ) {
				msgid << tokstr;
				state = S_RDIDADD;
				break;
			}
			if ( tok == T_ATOM && state == S_RDIDADD ) {
				state = S_WAITMSG_NOREAD;
				break;
			}
			error( format( "^1Error parsing localisation file: msgid value expected, got token %d (%s)\n", tok, tokstr.c_str() ).c_str() );
			state = S_ERROR;
			break;
		case S_WAITMSG:
			tok = lx.get( tokstr );
			state = S_WAITMSG_NOREAD;
			break;
		case S_WAITMSG_NOREAD:
			if ( tok == T_END ) {
				state = S_END; break;
			}
			if ( tok == T_ERROR ) {
				state = S_ERROR; break;
			}
			if ( tok == T_ATOM ) {
				if ( tokstr == "msgstr" ) {
					state = S_RDMSG;
					break;
				}
				error( format( "^1Error parsing localisation file: msgstr expected, got %s\n", tokstr.c_str() ).c_str() );
				state = S_ERROR;
				break;
			}
			error( format( "^1Error parsing localisation file: msgstr expected, got token %d (%s)\n", tok, tokstr.c_str() ).c_str() );
			state = S_ERROR;
			break;
		case S_RDMSG:
		case S_RDMSGADD:
			tok = lx.get( tokstr );
			if ( tok == T_END ) {
				if ( state == S_RDMSG ) {
					error( "^1Error parsing localisation file: msgstr value expected, got end of file\n" );
					state = S_ERROR;
					break;
				}
				addmsg( storage, msgid.str().c_str(), msgstr.str().c_str() );
				msgid.str( "" );
				msgstr.str( "" );
				state = S_END;
				break;
			}
			if ( tok == T_ERROR ) {
				state = S_ERROR; break;
			}
			if ( tok == T_STRING ) {
				msgstr << tokstr;
				state = S_RDMSGADD;
				break;
			}
			if ( tok == T_ATOM ) {
				addmsg( storage, msgid.str().c_str(), msgstr.str().c_str() );
				msgid.str( "" );
				msgstr.str( "" );
				state = S_WAITID;
				break;
			}
			error( format( "^1Error parsing localisation file: msgstr value or msgid expected, got token %d (%s)\n", tok, tokstr.c_str() ).c_str() );
			state = S_ERROR;
			break;
		}
	}

	if ( state == S_ERROR ) {
		return false;
	}

	return true;
}

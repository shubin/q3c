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


#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#include "linux_local.h"


static qbool stdin_active = qtrue;

// enable/disable tty input mode
// NOTE TTimo this is used during startup, cannot be changed during run
static cvar_t *ttycon = NULL;
// general flag to tell about tty console mode
static qbool ttycon_on = qfalse;
// when printing general stuff to stdout stderr (Sys_Printf)
//   we need to disable the tty console stuff
// this increments so we can recursively disable
static int ttycon_hide = 0;
// some key codes that the terminal may be using
// TTimo NOTE: I'm not sure how relevant this is
static int tty_erase;
static int tty_eof;

static struct termios tty_tc;

static field_t tty_con;

static cvar_t *ttycon_ansicolor = NULL;

static history_t tty_history;



// =============================================================
// tty console routines
// NOTE: if the user is editing a line when something gets printed to the early console then it won't look good
//   so we provide tty_Clear and tty_Show to be called before and after a stdout or stderr output
// =============================================================

// flush stdin, I suspect some terminals are sending a LOT of shit
// FIXME TTimo relevant?
static void tty_FlushIn()
{
  char key;
  while (read(0, &key, 1)!=-1);
}

// do a backspace
// TTimo NOTE: it seems on some terminals just sending '\b' is not enough
//   so for now, in any case we send "\b \b" .. yeah well ..
//   (there may be a way to find out if '\b' alone would work though)
static void tty_Back()
{
  char key;
  key = '\b';
  write(STDOUT_FILENO, &key, 1);
  key = ' ';
  write(STDOUT_FILENO, &key, 1);
  key = '\b';
  write(STDOUT_FILENO, &key, 1);
}

// clear the display of the line currently edited
// bring cursor back to beginning of line
void tty_Hide()
{
  assert(ttycon_on);
  if (ttycon_hide)
  {
    ttycon_hide++;
    return;
  }
  const int length = strlen(tty_con.buffer);
  if (length > 0)
  {
    const int stepsForward = length - tty_con.cursor;
    // a single write call for this didn't work on my terminal
    for (int i = 0; i < stepsForward; ++i)
    {
      write(STDOUT_FILENO, "\033[1C", 4);
    }
    for (int i = 0; i < length; ++i)
    {
      tty_Back();
    }
  }
  tty_Back(); // delete the leading "]"
  ttycon_hide++;
}

// show the current line
// FIXME TTimo need to position the cursor if needed??
static void tty_Show()
{
  assert(ttycon_on);
  assert(ttycon_hide>0);
  ttycon_hide--;
  if (ttycon_hide == 0)
  {
    write(STDOUT_FILENO, "]", 1);
    const int length = strlen(tty_con.buffer);
    if (length > 0)
    {
      const int stepsBack = length - tty_con.cursor;
      write(STDOUT_FILENO, tty_con.buffer, length);
      // a single write call for this didn't work on my terminal
      for (int i = 0; i < stepsBack; ++i)
      {
		write(STDOUT_FILENO, "\033[1D", 4);
	  }
    }
  }
}


// initialize the console input (tty mode if wanted and possible)
void Lin_ConsoleInputInit()
{
  struct termios tc;

  // TTimo
  // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=390
  // ttycon 0 or 1, if the process is backgrounded (running non interactively)
  // then SIGTTIN or SIGTOU is emitted, if not catched, turns into a SIGSTP
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);

  // FIXME TTimo initialize this in Sys_Init or something?
  ttycon = Cvar_Get("ttycon", "1", 0);
  if (ttycon && ttycon->value)
  {
    if (isatty(STDIN_FILENO)!=1)
    {
      Com_Printf("stdin is not a tty, tty console mode failed: %s\n", strerror(errno));
      Cvar_Set("ttycon", "0");
      ttycon_on = qfalse;
      return;
    }
    Com_Printf("Started tty console (use +set ttycon 0 to disable)\n");
    Field_Clear(&tty_con);
    tcgetattr (STDIN_FILENO, &tty_tc);
    tty_erase = tty_tc.c_cc[VERASE];
    tty_eof = tty_tc.c_cc[VEOF];
    tc = tty_tc;
    /*
     ECHO: don't echo input characters
     ICANON: enable canonical mode.  This  enables  the  special
              characters  EOF,  EOL,  EOL2, ERASE, KILL, REPRINT,
              STATUS, and WERASE, and buffers by lines.
     ISIG: when any of the characters  INTR,  QUIT,  SUSP,  or
              DSUSP are received, generate the corresponding signal
    */
    tc.c_lflag &= ~(ECHO | ICANON);
    /*
     ISTRIP strip off bit 8
     INPCK enable input parity checking
     */
    tc.c_iflag &= ~(ISTRIP | INPCK);
    tc.c_cc[VMIN] = 1;
    tc.c_cc[VTIME] = 0;
    tcsetattr (STDIN_FILENO, TCSADRAIN, &tc);
    ttycon_on = qtrue;

    ttycon_ansicolor = Cvar_Get( "ttycon_ansicolor", "0", CVAR_ARCHIVE );
  } else
    ttycon_on = qfalse;

	// make stdin non-blocking
	fcntl( STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | FNDELAY );
}


const char* Lin_ConsoleInput()
{
  // we use this when sending back commands
  static char text[256];
  int avail;
  char key;
  field_t field;

  if (ttycon && ttycon->value)
  {
    avail = read(STDIN_FILENO, &key, 1);
    if (avail != -1)
    {
      // we have something
      
      if (key != '\t')
		tty_con.acOffset = 0;
      
      if (key == tty_erase || key == 127 || key == 8) // backspace
      {
		const int length = strlen(tty_con.buffer);
        if (length > 0)
        {
		  if (tty_con.cursor == length)
		  {
            tty_con.buffer[length - 1] = '\0';
            tty_con.cursor--;
            tty_Back();
		  }
		  else if (tty_con.cursor > 0)
		  {
			tty_Hide();
			const int toMove = length + 1 - tty_con.cursor; // with the null terminator
			memmove(tty_con.buffer + tty_con.cursor - 1, tty_con.buffer + tty_con.cursor, toMove);
			tty_con.cursor--;
			tty_Show();
		  }
        }
        return NULL;
      }

      // check if this is a control char
      if ((key) && (key) < ' ')
      {
        if (key == '\n')
        {
#ifdef DEDICATED
          History_SaveCommand(&tty_history, &tty_con);
          Q_strncpyz(text, tty_con.buffer, sizeof(text));
          Field_Clear(&tty_con);
          write(STDOUT_FILENO, "\n]", 2);
#else
          // not in a game yet and no leading slash?
          if (cls.state != CA_ACTIVE &&
              tty_con.buffer[0] != '/' && tty_con.buffer[0] != '\\')
          {
            // there's no one to chat with, so we consider this a command
            const int length = strlen(tty_con.buffer);
            if (length > 0)
            {
			  memmove(tty_con.buffer + 1, tty_con.buffer, length + 1);
			}
			else
			{
			  tty_con.buffer[1] = '\0';
			}
			tty_con.buffer[0] = '\\';
            tty_con.cursor++;
          }

          // decide what the final command will be
          const int length = strlen(tty_con.buffer);
          if (tty_con.buffer[0] == '/' || tty_con.buffer[0] == '\\')
            Q_strncpyz(text, tty_con.buffer + 1, sizeof(text));
          else if (length > 0)
            Com_sprintf(text, sizeof(text), "say %s", tty_con.buffer);
          else
            *text = '\0';
            
          History_SaveCommand(&tty_history, &tty_con);
          tty_Hide();
          Com_Printf("tty]%s\n", tty_con.buffer);
          Field_Clear(&tty_con);
          tty_Show();
#endif
          return text;
        }
        if (key == '\t')
        {
          tty_Hide();
#ifdef DEDICATED
          Field_AutoComplete( &tty_con, qfalse );
#else
          Field_AutoComplete( &tty_con, qtrue );
#endif
          tty_Show();
          return NULL;
        }
        avail = read(0, &key, 1);
        if (avail != -1)
        {
          // VT 100 keys
          if (key == '[' || key == 'O')
          {
            avail = read(0, &key, 1);
            if (avail != -1)
            {			
              switch (key)
              {
              case 'A': // up arrow (move cursor up)
                History_GetPreviousCommand(&field, &tty_history);
                if (field.buffer[0] != '\0')
                {
                  tty_Hide();
                  tty_con = field;
                  tty_Show();
                }
                tty_FlushIn();
                return NULL;
                break;
              case 'B': // down arrow (move cursor down)
                History_GetNextCommand(&field, &tty_history, 0);
                tty_Hide();
                if (tty_con.buffer[0] != '\0')
                {
                  tty_con = field;
                }
                else
                {
                  Field_Clear(&tty_con);
                }
                tty_Show();
                tty_FlushIn();
                return NULL;
                break;
              case 'C': // right arrow (move cursor right)
                if (tty_con.cursor < (int)strlen(tty_con.buffer))
                {
				  write(STDOUT_FILENO, "\033[1C", 4);
                  tty_con.cursor++;
				}
                return NULL;
              case 'D': // left arrow (move cursor left)
                if (tty_con.cursor > 0)
                {
				  write(STDOUT_FILENO, "\033[1D", 4);
                  tty_con.cursor--;
				}
                return NULL;
              case '3': // delete (might not work on all terminals)
                {
                  const int length = strlen(tty_con.buffer);
                  if (tty_con.cursor < length)
                  {
	                tty_Hide();
	                const int toMove = length - tty_con.cursor; // with terminating NULL
	                memmove(tty_con.buffer + tty_con.cursor, tty_con.buffer + tty_con.cursor + 1, toMove);
	                tty_Show();  
                  }
			    }
			    tty_FlushIn();
			    return NULL;
              case 'H': // home (move cursor to upper left corner)
                if (tty_con.cursor > 0)
                {
				  tty_Hide();
	              tty_con.cursor = 0;
	              tty_Show();  
				}
			    return NULL;
			  case 'F': // end (might not work on all terminals)
                {
			      const int length = strlen(tty_con.buffer);
                  if (tty_con.cursor < length)
                  {
	                tty_Hide();
	                tty_con.cursor = length;
	                tty_Show();  
                  }
				}
			    return NULL;
              }
            }
          }
        }
        Com_DPrintf("droping ISCTL sequence: %d, tty_erase: %d\n", key, tty_erase);
        tty_FlushIn();
        return NULL;
      }
      
      // if we get here, key is a regular character
      const int length = strlen(tty_con.buffer);
      if (tty_con.cursor == length)
      {
        write(STDOUT_FILENO, &key, 1);
        tty_con.buffer[tty_con.cursor + 0] = key;
        tty_con.buffer[tty_con.cursor + 1] = '\0';
        tty_con.cursor++;
	  }
      else
      {
        tty_Hide();
        const int toMove = length + 1 - tty_con.cursor; // need to move the null terminator too
		memmove(tty_con.buffer + tty_con.cursor + 1, tty_con.buffer + tty_con.cursor, toMove);
		tty_con.buffer[tty_con.cursor] = key;
		tty_con.cursor++;
        tty_Show();
      }
    }
    return NULL;
  } else
  {
    int     len;
    fd_set  fdset;
    struct timeval timeout;

    if (!com_dedicated || !com_dedicated->value)
      return NULL;

    if (!stdin_active)
      return NULL;

    FD_ZERO(&fdset);
    FD_SET(STDIN_FILENO, &fdset);
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    if (select (1, &fdset, NULL, NULL, &timeout) == -1 || !FD_ISSET(STDIN_FILENO, &fdset))
    {
      return NULL;
    }

    len = read (STDIN_FILENO, text, sizeof(text));
    if (len == 0)
    { // eof!
      stdin_active = qfalse;
      return NULL;
    }

    if (len < 1)
      return NULL;
    text[len-1] = 0;    // rip off the /n and terminate

    return text;
  }
}


// never exit without calling this, or your terminal will be left in a pretty bad state
void Lin_ConsoleInputShutdown()
{
	if (ttycon_on)
	{
		tty_Back(); // delete the leading "]"
		tcsetattr (STDIN_FILENO, TCSADRAIN, &tty_tc);
		ttycon_on = qfalse;
	}

	// make stdin blocking
	fcntl( STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & (~FNDELAY) );
}


static struct Q3ToAnsiColorTable_s
{
  char Q3color;
  const char* ANSIcolor;
} const tty_colorTable[] =
{
  { COLOR_BLACK,    "30" },
  { COLOR_RED,      "31" },
  { COLOR_GREEN,    "32" },
  { COLOR_YELLOW,   "33" },
  { COLOR_BLUE,     "34" },
  { COLOR_CYAN,     "36" },
  { COLOR_MAGENTA,  "35" },
  { COLOR_WHITE,    "0" }
};

static const int tty_colorTableSize = sizeof( tty_colorTable ) / sizeof( tty_colorTable[0] );


static void ANSIColorify( const char *msg, char *buffer, int bufferSize )
{
  int   msgLength;
  int   i, j;
  const char* escapeCode;
  char  tempBuffer[ 7 ];

  if( !msg || !buffer )
    return;

  msgLength = strlen( msg );
  i = 0;
  buffer[ 0 ] = '\0';

  while( i < msgLength )
  {
    if( msg[ i ] == '\n' )
    {
      Com_sprintf( tempBuffer, 7, "%c[0m\n", 0x1B );
      strncat( buffer, tempBuffer, bufferSize - 1 );
      i++;
    }
    else if( msg[ i ] == Q_COLOR_ESCAPE )
    {
      i++;

      if( i < msgLength )
      {
        escapeCode = NULL;
        for( j = 0; j < tty_colorTableSize; j++ )
        {
          if( msg[ i ] == tty_colorTable[ j ].Q3color )
          {
            escapeCode = tty_colorTable[ j ].ANSIcolor;
            break;
          }
        }

        if( escapeCode )
        {
          Com_sprintf( tempBuffer, 7, "%c[%sm", 0x1B, escapeCode );
          strncat( buffer, tempBuffer, bufferSize - 1 );
        }

        i++;
      }
    }
    else
    {
      Com_sprintf( tempBuffer, 7, "%c", msg[ i++ ] );
      strncat( buffer, tempBuffer, bufferSize - 1 );
    }
  }
}


static char* CleanTermStr( char* string )
{
	char* s = string;
	char* d = string;
	char c;

	while ((c = *s) != 0 ) {
		if ( Q_IsColorString( s ) )
			s++;
		else
			*d++ = c;
		s++;
	}
	*d = '\0';

	return string;
}


void Sys_Print( const char *msg )
{
  static char finalMsg[ MAXPRINTMSG ];
	
  if (ttycon_on)
  {
    tty_Hide();
  }

  if( ttycon_on && ttycon_ansicolor && ttycon_ansicolor->integer )
  {
    ANSIColorify( msg, finalMsg, sizeof(finalMsg) );
  }
  else
  {
    Q_strncpyz( finalMsg, msg, sizeof(finalMsg) );
    CleanTermStr( finalMsg );
  }
  fputs( finalMsg, stdout );

  if (ttycon_on)
  {
    tty_Show();
  }
}

qbool tty_Enabled()
{
	return ttycon_on;
}

history_t* tty_GetHistory()
{
	return &tty_history;
}

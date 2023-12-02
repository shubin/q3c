#define help_cl_timeNudge \
"id's crippled timenudge\n" \
"This still exists in CPMA, but should always be " S_COLOR_VAL "0 " S_COLOR_HELP ".\n" \
"All it really does now is mess up the automatic adaptive nudges."

#define help_cl_shownet \
"prints network info\n" \
S_COLOR_VAL "   -2 " S_COLOR_HELP "= Command time\n" \
S_COLOR_VAL "   -1 " S_COLOR_HELP "= Entity removed/changed events\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Disabled\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Message lengths\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Message types, command time, etc\n" \
S_COLOR_VAL "    3 " S_COLOR_HELP "= " S_COLOR_VAL "2 " S_COLOR_HELP "+ entity parsing details\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= " S_COLOR_VAL "2 " S_COLOR_HELP "+ player state details"

#define help_cl_showSend \
"prints client to server packet info\n" \
"format: (usercmd_count) packet_size\n" \
"each dot symbolizes a frame of delay"

#define help_cl_allowDownload \
"selects the download system\n" \
S_COLOR_VAL "   -1 " S_COLOR_HELP "= Id's old download system\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Downloads disabled\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= CNQ3's new download system"

#define help_con_scaleMode \
"console text scaling mode\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Text size scales with con_scale but not the resolution\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Text size scales with con_scale and the resolution\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Text size is always 8x12"

#define help_plus_minus \
"\nAbout commands starting with '+' or '-':\n" \
"- If '" S_COLOR_CMD "+cmdname" S_COLOR_HELP "' is called from a bind, the command is executed every frame until the bind key is released.\n" \
"- If '" S_COLOR_CMD "+cmdname" S_COLOR_HELP "' is not called from a bind, the command is executed every frame until '" S_COLOR_CMD "-cmdname" S_COLOR_HELP "' is called."

#define help_cl_debugMove \
"prints a graph of view angle deltas\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Disabled\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Horizontal axis\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Vertical axis"

#define help_bind_extra \
"Use " S_COLOR_CMD "bindkeylist " S_COLOR_HELP "to print the list of key names."

#define help_bind \
"binds a command to a key\n" \
help_bind_extra

#define help_unbind \
"unbinds a key\n" \
help_bind_extra

#define help_cl_matchAlerts \
"lets you know when a match is starting\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= When unfocused (otherwise only when minimized)\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Flash the task bar (Windows only)\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= Beep once (Windows only)\n" \
S_COLOR_VAL "    8 " S_COLOR_HELP "= Unmute"

#define help_s_autoMute \
"selects when the audio output should be disabled\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Never\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Window is not focused\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Window is minimized"

#define help_con_notifytime \
"seconds messages stay visible in the notify area\n" \
"If " S_COLOR_VAL "-1" S_COLOR_HELP ", CPMA will draw the notify area with the 'Console' SuperHUD element."

#define help_m_accelStyle \
S_COLOR_VAL "0" S_COLOR_HELP "=original, " S_COLOR_VAL "1" S_COLOR_HELP "=new"

#define help_m_accelOffset \
"offset for the power function\n" \
"For " S_COLOR_CVAR "m_accelStyle " S_COLOR_VAL "1 " S_COLOR_HELP "only."

#define help_m_limit \
"mouse speed cap (" S_COLOR_VAL "0" S_COLOR_HELP "=disabled)\n" \
"For " S_COLOR_CVAR "m_accelStyle " S_COLOR_VAL "0 " S_COLOR_HELP "only."

#define help_m_forward \
"forward/backwards mouse sensitivity (" S_COLOR_CMD "+strafe" S_COLOR_HELP ")"

#define help_m_side \
"left/right mouse sensitivity (" S_COLOR_CMD "+strafe" S_COLOR_HELP ")"

#define help_cl_pitchspeed \
S_COLOR_CMD "+lookup +lookdown " S_COLOR_HELP "speed"

#define help_cl_yawspeed \
S_COLOR_CMD "+right +left " S_COLOR_HELP "speed"

#define help_cl_run \
"running enabled (" S_COLOR_VAL "0" S_COLOR_HELP "=walk)"

#define help_cl_freelook \
S_COLOR_VAL "0 " S_COLOR_HELP "means you can't look up/down"

#define help_cl_showMouseRate \
"prints info when " S_COLOR_CVAR "m_accel " S_COLOR_HELP "!= " S_COLOR_VAL "0"

#define help_rconPassword \
"server password, used by /" S_COLOR_CMD "rcon"

#define help_cl_aviFrameRate \
"frame-rate for /" S_COLOR_CMD "video"

#define help_cl_aviMotionJpeg \
"/" S_COLOR_CMD "video " S_COLOR_HELP "stores frames as JPEGs"

#define help_rconAddress \
"IP address of the server to /" S_COLOR_CMD "rcon " S_COLOR_HELP "to"

#define help_con_colHL \
"RGBA color of auto-completion highlights\n" \
"This requires " S_COLOR_CVAR "con_completionStyle " S_COLOR_VAL "1" S_COLOR_HELP "."

#define help_con_drawHelp \
"draws help text below the console\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Enables the help panel below the console\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Draws the help panel even if the cvar/cmd has no help text\n" \
S_COLOR_VAL "    4 " S_COLOR_HELP "= Draws the list of modules\n" \
S_COLOR_VAL "    8 " S_COLOR_HELP "= Draws the list of attributes (cvars only)"

#define help_r_khr_debug \
"enables an OpenGL debug context\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Forced off\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Forced on\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= On in debug builds only"

#define help_net_proxy \
"proxy server address\n" \
"Set this CVar before connecting to a server to join through a proxy.\n" \
"It works with /" S_COLOR_CMD "connect" S_COLOR_HELP ", /" S_COLOR_CMD "reconnect" S_COLOR_HELP " and the server browser UI.\n" \
"Set it to an empty string to not use a proxy server."

#define help_searchconsole \
"begins a new console search\n" \
"Press ctrl-F when the console is down to bring up the command.\n" \
"Press (shift-)F3 to find the next match going up or down.\n" \
"The star character (*) will match 0, 1 or more characters.\n" \
"\n" \
"         Argument   Matches\n" \
"Example: init       lines starting with 'init'\n" \
"Example: *init      lines containing 'init'\n" \
"Example: >*net      lines starting with '>' AND containing 'net'"

#define help_cl_demoPlayer \
"1 enables demo rewind\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Uses the original demo player\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Uses the new demo player when the mod supports it"

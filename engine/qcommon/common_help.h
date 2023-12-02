#define help_pattern_matching \
"\n" \
"If no argument is passed, the list is unfiltered.\n" \
"If you pass an argument, it is used to filter the list.\n" \
"The star character (*) will match 0, 1 or more characters.\n" \
"\n"

#define help_toggle \
"toggles the boolean value of a variable\n" \
"Example: " S_COLOR_CMD "toggle " S_COLOR_CVAR "v\n" S_COLOR_HELP \
"         non-" S_COLOR_VAL "0 " S_COLOR_HELP "=> " S_COLOR_VAL "0 \n" \
"         0 " S_COLOR_HELP "=> " S_COLOR_VAL "1\n" S_COLOR_HELP \
"Example: " S_COLOR_CMD "toggle " S_COLOR_CVAR "v " S_COLOR_VAL "a b c\n" S_COLOR_HELP \
"         non-" S_COLOR_VAL "abc " S_COLOR_HELP "=> " S_COLOR_VAL "a\n" \
"         a " S_COLOR_HELP "=> " S_COLOR_VAL "b " S_COLOR_HELP "| " S_COLOR_VAL "b " S_COLOR_HELP "=> " S_COLOR_VAL "c " \
	S_COLOR_HELP "| " S_COLOR_VAL "c " S_COLOR_HELP "=> " S_COLOR_VAL "a"

#define help_cvarlist \
"lists and filters all cvars\n" \
help_pattern_matching \
"         Argument   Matches\n" \
"Example: r_         cvars starting with 'r_'\n" \
"Example: *light     cvars containing 'light'\n" \
"Example: r_*light   cvars starting with 'r_' AND containing 'light'"

#define help_cmdlist \
"lists and filters all commands\n" \
help_pattern_matching \
"         Argument   Matches\n" \
"Example: fs_        cmds starting with 'fs_'\n" \
"Example: *list      cmds containing 'list'\n" \
"Example: fs_*list   cmds starting with 'fs_' AND containing 'list'"

#define help_com_logfile \
"console logging to qconsole.log\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Disabled\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Enabled\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Enabled and flushes the file after every write"

#define help_com_viewlog \
"early console window visibility\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Hidden\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Visible\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= Minimized"

#define help_con_completionStyle \
"auto-completion style\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Legacy, always print all results\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= ET-style, print once then cycle"

#define help_qport \
"internal network port\n" \
"this allows more than one person to play from behind a NAT router by using only one IP address"

#define help_vm_load \
"\n" \
S_COLOR_VAL "    0 " S_COLOR_HELP "= Shared library (native code)\n" \
S_COLOR_VAL "    1 " S_COLOR_HELP "= Interpreted QVM\n" \
S_COLOR_VAL "    2 " S_COLOR_HELP "= JIT-compiled QVM"

#define help_com_maxfps \
"max. allowed framerate\n" \
"It's highly recommended to only use " S_COLOR_VAL "125 " S_COLOR_HELP "or " S_COLOR_VAL "250 " S_COLOR_HELP "with V-Sync disabled.\n" \
"If you see 'connection interruped' with " S_COLOR_VAL "250" S_COLOR_HELP ", set it back to " S_COLOR_VAL "125" S_COLOR_HELP "."

#define help_writeconfig \
"writes CVars and key binds to a file\n" \
"Usage: " S_COLOR_CMD "writeconfig " S_COLOR_VAL "<filename> [-f]" S_COLOR_HELP "\n" \
S_COLOR_VAL "-f " S_COLOR_HELP "will force writing all CVars,\n" \
"whether they're archived or not."

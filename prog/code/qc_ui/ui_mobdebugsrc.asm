global g_mobdebug_src_start
global g_mobdebug_src_end
section .data

g_mobdebug_src_start:	incbin "mobdebug.lua"
g_mobdebug_src_end:		db '\0'

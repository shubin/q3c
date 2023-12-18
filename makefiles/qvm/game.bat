set ProjectRoot=..\..\
set ToolDir=%ProjectRoot%tools\
set ProgDir=%ProjectRoot%mod\
set IntDir=%ProjectRoot%build\qvm\
set LCC=%ToolDir%q3lcc.exe -DQ3_VM -DQC=1 -DUNLAGGED=1 -S -Wf-target=bytecode -Wf-g -I%ProgDir%code\cgame -I%ProgDir%code\qcommon -I%ProgDir%code\game -I%ProgDir%code\hudlib
set Q3ASM=%ToolDir%q3asm.exe

%LCC% %ProgDir%code\game\ai_chat.c
%LCC% %ProgDir%code\game\ai_cmd.c
%LCC% %ProgDir%code\game\ai_dmnet.c
%LCC% %ProgDir%code\game\ai_dmq3.c
%LCC% %ProgDir%code\game\ai_main.c
%LCC% %ProgDir%code\game\ai_team.c
%LCC% %ProgDir%code\game\ai_vcmd.c
%LCC% %ProgDir%code\game\bg_lib.c
%LCC% %ProgDir%code\game\bg_misc.c
%LCC% %ProgDir%code\game\bg_pmove.c
%LCC% %ProgDir%code\game\bg_slidemove.c
%LCC% %ProgDir%code\game\g_active.c
%LCC% %ProgDir%code\game\g_arenas.c
%LCC% %ProgDir%code\game\g_bot.c
%LCC% %ProgDir%code\game\g_client.c
%LCC% %ProgDir%code\game\g_cmds.c
%LCC% %ProgDir%code\game\g_combat.c
%LCC% %ProgDir%code\game\g_items.c
%LCC% %ProgDir%code\game\g_main.c
%LCC% %ProgDir%code\game\g_mem.c
%LCC% %ProgDir%code\game\g_misc.c
%LCC% %ProgDir%code\game\g_missile.c
%LCC% %ProgDir%code\game\g_mover.c
%LCC% %ProgDir%code\game\g_session.c
%LCC% %ProgDir%code\game\g_spawn.c
%LCC% %ProgDir%code\game\g_svcmds.c
%LCC% %ProgDir%code\game\g_target.c
%LCC% %ProgDir%code\game\g_team.c
%LCC% %ProgDir%code\game\g_trigger.c
%LCC% %ProgDir%code\game\g_utils.c
%LCC% %ProgDir%code\game\g_weapon.c
%LCC% %ProgDir%code\qcommon\q_math.c
%LCC% %ProgDir%code\qcommon\q_shared.c
%LCC% %ProgDir%code\game\bg_champions.c
%LCC% %ProgDir%code\game\bg_promode.c
%LCC% %ProgDir%code\game\g_ability.c
%LCC% %ProgDir%code\game\g_acidspit.c
%LCC% %ProgDir%code\game\g_totem.c
%LCC% %ProgDir%code\game\g_unlagged.c

%Q3ASM% -f game

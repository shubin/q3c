SourceDir=$(ProjectRoot)/mod/code
ToolDir=$(ProjectRoot)/tools
IntDir=$(ProjectRoot)/build/qvm

CC=$(ToolDir)/q3lcc
ASM=$(ToolDir)/q3asm

CFLAGS=-DQ3_VM=1 -S -Wf-target=bytecode -Wf-g
#CFLAGS_QC=-DUNLAGGED=1 -DQC=1

GameVQ3Sources=					\
	game/g_main					\
	qcommon/q_math				\
	qcommon/q_shared			\
	game/ai_chat				\
	game/ai_cmd					\
	game/ai_dmnet				\
	game/ai_dmq3				\
	game/ai_main				\
	game/ai_team				\
	game/ai_vcmd 				\
	game/bg_lib					\
	game/bg_misc				\
	game/bg_pmove				\
	game/bg_slidemove			\
	game/g_active				\
	game/g_arenas				\
	game/g_bot					\
	game/g_client				\
	game/g_cmds					\
	game/g_combat				\
	game/g_items				\
	game/g_mem					\
	game/g_misc					\
	game/g_missile				\
	game/g_mover				\
	game/g_session				\
	game/g_spawn				\
	game/g_svcmds				\
	game/g_target				\
	game/g_team					\
	game/g_trigger				\
	game/g_utils				\
	game/g_weapon

GameQCSources=					\
	game/bg_champions			\
	game/bg_promode				\
	game/g_ability				\
	game/g_acidspit				\
	game/g_totem				\
	game/g_unlagged

CGameVQ3Sources=				\
	cgame/cg_main				\
	qcommon/q_math				\
	qcommon/q_shared			\
	cgame/cg_consolecmds		\
	cgame/cg_draw				\
	cgame/cg_drawtools			\
	cgame/cg_effects			\
	cgame/cg_ents				\
	cgame/cg_event				\
	cgame/cg_info				\
	cgame/cg_localents			\
	cgame/cg_marks				\
	cgame/cg_particles			\
	cgame/cg_players			\
	cgame/cg_playerstate		\
	cgame/cg_predict			\
	cgame/cg_scoreboard			\
	cgame/cg_servercmds			\
	cgame/cg_snapshot			\
	cgame/cg_view				\
	cgame/cg_weapons			\
	game/bg_lib					\
	game/bg_misc				\
	game/bg_pmove				\
	game/bg_slidemove

CGameQCSources=					\
	cgame/cg_champions			\
	cgame/cg_damageplum			\
	cgame/cg_decals				\
	cgame/cg_dmgdir				\
	cgame/cg_hud				\
	cgame/cg_unlagged			\
	cgame/hud_ability			\
	cgame/hud_ammo				\
	cgame/hud_crosshair			\
	cgame/hud_deathmessage		\
	cgame/hud_ffa_scores		\
	cgame/hud_fragmessage		\
	cgame/hud_obituary			\
	cgame/hud_pickups			\
	cgame/hud_playerstatus		\
	cgame/hud_shared			\
	cgame/hud_tdm_scores		\
	cgame/hud_timer				\
	cgame/hud_tournament_scores	\
	game/bg_champions			\
	game/bg_promode				\
	hudlib/hudlib

UIVQ3Sources=					\
	q3_ui/ui_main				\
	game/bg_misc				\
	game/bg_lib					\
	q3_ui/ui_addbots			\
	q3_ui/ui_atoms				\
	q3_ui/ui_cdkey				\
	q3_ui/ui_cinematics			\
	q3_ui/ui_confirm			\
	q3_ui/ui_connect			\
	q3_ui/ui_controls2			\
	q3_ui/ui_credits			\
	q3_ui/ui_demo2				\
	q3_ui/ui_display			\
	q3_ui/ui_gameinfo			\
	q3_ui/ui_ingame				\
	q3_ui/ui_menu				\
	q3_ui/ui_mfield				\
	q3_ui/ui_mods				\
	q3_ui/ui_network			\
	q3_ui/ui_options			\
	q3_ui/ui_playermodel		\
	q3_ui/ui_players			\
	q3_ui/ui_playersettings		\
	q3_ui/ui_preferences		\
	q3_ui/ui_qmenu				\
	q3_ui/ui_removebots			\
	q3_ui/ui_serverinfo			\
	q3_ui/ui_servers2			\
	q3_ui/ui_setup				\
	q3_ui/ui_sound				\
	q3_ui/ui_sparena			\
	q3_ui/ui_specifyserver		\
	q3_ui/ui_splevel			\
	q3_ui/ui_sppostgame			\
	q3_ui/ui_spreset			\
	q3_ui/ui_spskill			\
	q3_ui/ui_startserver		\
	q3_ui/ui_team				\
	q3_ui/ui_teamorders			\
	q3_ui/ui_video				\
	qcommon/q_math				\
	qcommon/q_shared

UIQCSources=					\
	game/bg_champions			\
	q3_ui/ui_champions			\
	q3_ui/ui_death


GameSources=$(GameVQ3Sources)
CGameSources=$(CGameVQ3Sources)
UISources=$(UIVQ3Sources)

GameAsmList=$(addprefix $(IntDir)/,$(addsuffix .asm,$(GameSources)))
CGameAsmList=$(addprefix $(IntDir)/,$(addsuffix .asm,$(CGameSources)))
UIAsmList=$(addprefix $(IntDir)/,$(addsuffix .asm,$(UISources)))

all: $(IntDir)/qagame.qvm $(IntDir)/cgame.qvm $(IntDir)/ui.qvm

$(IntDir)/qagame.qvm: $(SourceDir)/game/g_syscalls.asm $(GameAsmList)
	$(ASM) -o $@ $^

$(IntDir)/cgame.qvm: $(SourceDir)/cgame/cg_syscalls.asm $(CGameAsmList)
	$(ASM) -o $@ $^

$(IntDir)/ui.qvm: $(SourceDir)/ui/ui_syscalls.asm $(UIAsmList)
	$(ASM) -o $@ $^

$(IntDir)/game/%.asm: $(SourceDir)/game/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/cgame/%.asm: $(SourceDir)/cgame/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/q3_ui/%.asm: $(SourceDir)/q3_ui/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/ui/%.asm: $(SourceDir)/ui/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/qcommon/%.asm: $(SourceDir)/qcommon/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/hudlib/%.asm: $(SourceDir)/hudlib/%.c
	$(CC) $(CFLAGS) -o $@ $^

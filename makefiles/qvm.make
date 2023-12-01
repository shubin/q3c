SourceDir=$(ProjectRoot)/code
ToolDir=$(ProjectRoot)/tools
IntDir=$(ProjectRoot)/build/qvm

CC=$(ToolDir)/q3lcc
ASM=$(ToolDir)/q3asm

CFLAGS=-DQ3_VM=1 -S -Wf-target=bytecode -Wf-g
#CFLAGS_QC=-DUNLAGGED=1 -DQC=1

GameVQ3Sources=				\
	game/g_main				\
	qcommon/q_math			\
	qcommon/q_shared		\
	game/ai_chat			\
	game/ai_cmd				\
	game/ai_dmnet			\
	game/ai_dmq3			\
	game/ai_main			\
	game/ai_team			\
	game/ai_vcmd 			\
	game/bg_lib				\
	game/bg_misc			\
	game/bg_pmove			\
	game/bg_slidemove		\
	game/g_active			\
	game/g_arenas			\
	game/g_bot				\
	game/g_client			\
	game/g_cmds				\
	game/g_combat			\
	game/g_items			\
	game/g_mem				\
	game/g_misc				\
	game/g_missile			\
	game/g_mover			\
	game/g_session			\
	game/g_spawn			\
	game/g_svcmds			\
	game/g_target			\
	game/g_team				\
	game/g_trigger			\
	game/g_utils			\
	game/g_weapon

GameQCSources=				\
	game/bg_champions		\
	game/bg_promode			\
	game/g_ability			\
	game/g_acidspit			\
	game/g_totem			\
	game/g_unlagged

CGameVQ3Sources=			\
	cgame/cg_main			\
	qcommon/q_math			\
	qcommon/q_shared		\
	cgame/cg_consolecmds	\
	cgame/cg_draw			\
	cgame/cg_drawtools		\
	cgame/cg_effects		\
	cgame/cg_ents			\
	cgame/cg_event			\
	cgame/cg_info			\
	cgame/cg_localents		\
	cgame/cg_marks			\
	cgame/cg_particles		\
	cgame/cg_players		\
	cgame/cg_playerstate	\
	cgame/cg_predict		\
	cgame/cg_scoreboard		\
	cgame/cg_servercmds		\
	cgame/cg_snapshot		\
	cgame/cg_view			\
	cgame/cg_weapons		\
	game/bg_lib				\
	game/bg_misc			\
	game/bg_pmove			\
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

GameSources=$(GameVQ3Sources)
CGameSources=$(CGameVQ3Sources)

GameAsmList=$(addprefix $(IntDir)/,$(addsuffix .asm,$(GameSources)))
CGameAsmList=$(addprefix $(IntDir)/,$(addsuffix .asm,$(CGameSources)))

all: $(IntDir)/game.qvm $(IntDir)/cgame.qvm

$(IntDir)/game.qvm: $(SourceDir)/game/g_syscalls.asm $(GameAsmList)
	$(ASM) -o $@ $^

$(IntDir)/cgame.qvm: $(SourceDir)/cgame/cg_syscalls.asm $(CGameAsmList)
	$(ASM) -o $@ $^

$(IntDir)/game/%.asm: $(SourceDir)/game/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/cgame/%.asm: $(SourceDir)/cgame/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/qcommon/%.asm: $(SourceDir)/qcommon/%.c
	$(CC) $(CFLAGS) -o $@ $^

$(IntDir)/hudlib/%.asm: $(SourceDir)/hudlib/%.c
	$(CC) $(CFLAGS) -o $@ $^

require("enum")
require("config")
require("utils")
require("init")
require("input")
require("refresh")

require("ui.ui_player")

gCurrentMenu = UIMENU_NONE

function InitModel()
  pi = {}
  UI_PlayerInfo_SetModel(pi, "ranger/default")
  local viewangles = vec3_t.new(0, 150, 0)
  local moveangles = vec3_t.new(0, 0, 0)
  UI_PlayerInfo_SetInfo( pi, LEGS_IDLE, TORSO_STAND, viewangles, moveangles, WP_LOUSY_MACHINEGUN, false );
end

function UI_SetActiveMenu(nmenu)
  gCurrentMenu = nmenu
  if nmenu == UIMENU_NONE then
    trap_Key_SetCatcher(trap_Key_GetCatcher() & (~KEYCATCH_UI))
    trap_Key_ClearStates()
  else
    if nmenu == UIMENU_MAIN then
      ShowMenu("main")
      if not pi then
        InitModel()
      end
    end
    if nmenu == UIMENU_INGAME then
      ShowMenu("ingame")
    end
    trap_Key_SetCatcher(KEYCATCH_UI)
  end
end

function UI_IsFullscreen()
  return (gCurrentMenu ~= UIMENU_NONE) and (gCurrentMenu ~= UIMENU_INGAME) and (gCurrentMenu ~= UIMENU_DEATH)
end

function UI_ConsoleCommand()
  local argc = trap_Argc()
  if exec and (trap_Argv(0) == "lua") and (argc > 1) then
    local cmd = trap_Argv(-1):gsub("^lua%s*", "")
    exec(cmd)
    return true
  end
  return false
end

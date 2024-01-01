require("enum")
require("config")
require("utils")
require("init")
require("input")
require("refresh")

gCurrentMenu = UIMENU_NONE

function UI_SetActiveMenu(nmenu)
  gCurrentMenu = nmenu
  if nmenu == UIMENU_NONE then
    trap_Key_SetCatcher(trap_Key_GetCatcher() & (~KEYCATCH_UI))
    trap_Key_ClearStates()
  else
    if nmenu == UIMENU_MAIN then
      ShowMenu("main")
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

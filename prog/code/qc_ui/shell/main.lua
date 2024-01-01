require("shell.debug")
require("shell.enum")
require("shell.config")
require("shell.utils")
require("shell.init")
require("shell.input")
require("shell.refresh")

gCurrentMenu = UIMENU_NONE

function UI_SetActiveMenu(nmenu)
  gCurrentMenu = nmenu
  if nmenu == UIMENU_NONE then
    trap_Key_SetCatcher(trap_Key_GetCatcher() & (~KEYCATCH_UI))
    trap_Key_ClearStates()
  else
    if nmenu == UIMENU_MAIN then
      menu.main.doc:Show()
    end
    if nmenu == UIMENU_INGAME then
      menu.ingame.doc:Show()
    end
    trap_Key_SetCatcher(KEYCATCH_UI)
  end
end

function UI_IsFullscreen()
  return (gCurrentMenu ~= UIMENU_NONE) and (gCurrentMenu ~= UIMENU_INGAME) 
end

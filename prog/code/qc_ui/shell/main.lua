require("shell.enum")
require("shell.config")
require("shell.utils")
require("shell.init")
require("shell.input")
require("shell.refresh")

function UI_SetActiveMenu(menu)
  if menu == UIMENU_NONE then
    trap_Key_SetCatcher( trap_Key_GetCatcher() & (~KEYCATCH_UI) )
    trap_Key_ClearStates()
  else
    trap_Key_SetCatcher( KEYCATCH_UI )
  end
end

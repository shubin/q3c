local menu = {}
menu.doc = LoadDocument("menu/main.rml")

function menu:open(nmenu)
  self.doc:Hide()
  ShowMenu(nmenu)
end

function menu:restart()
  trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n")  
end

function menu:quit()
  trap_Cmd_ExecuteText(EXEC_APPEND, "quit\n")
end

return menu

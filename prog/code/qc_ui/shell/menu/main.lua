menu.main = {}
menu.main.doc = LoadDocument("menu/main.rml")

function menu.main:start()
  self.doc:Hide()
  menu.start.doc:Show()
end

function menu.main:restart()
  trap_Cmd_ExecuteText(EXEC_APPEND, "vid_restart\n")  
end

function menu.main:quit()
  trap_Cmd_ExecuteText(EXEC_APPEND, "quit\n")
end

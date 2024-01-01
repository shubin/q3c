local menu = {}
menu.doc = LoadDocument("menu/start.rml")

function menu:dm1()
  trap_Cmd_ExecuteText(EXEC_APPEND, "map qcdm1\n")
end

function menu:back()
  self.doc:Hide()
  ShowMenu("main")
end

return menu

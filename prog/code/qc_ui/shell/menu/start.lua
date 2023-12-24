menu.start = {}
menu.start.doc = LoadDocument("menu/start.rml")

function menu.start:dm1()
  trap_Cmd_ExecuteText(EXEC_APPEND, "map qcdm1\n")
end

function menu.start:back()
  self.doc:Hide()
  menu.main.doc:Show()
end

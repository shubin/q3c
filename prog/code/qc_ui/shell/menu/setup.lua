menu.setup = {}
menu.setup.doc = LoadDocument("menu/setup.rml")

function menu.setup:show()
  trap_Print("Setup show!\n")
  local name_elem = self.doc:GetElementById("param_name");
  local name = trap_Cvar_VariableString("name")
  trap_Print("Name: "..name.."\n")
  name_elem:SetAttribute("value", name)
end

function menu.setup:back()
  self.doc:Hide()
  menu.main.doc:Show()
end

function menu.setup:apply()
  local name_elem = self.doc:GetElementById("param_name");
  if name_elem then
    trap_Print("Can get name!\n")
  else
    trap_Print("Can't get name!\n")
  end
end

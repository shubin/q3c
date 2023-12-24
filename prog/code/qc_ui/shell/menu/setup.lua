menu.setup = {}
menu.setup.doc = LoadDocument("menu/setup.rml")

function menu.setup:show()
  local name_elem = self.doc:GetElementById("param_name");
  name_elem:SetAttribute("value", trap_Cvar_VariableString("name"))
end

function menu.setup:back()
  self.doc:Hide()
  menu.main.doc:Show()
end

function menu.setup:apply()
  local name_elem = self.doc:GetElementById("param_name");
  trap_Cvar_Set("name", name_elem:GetAttribute("value"))
end

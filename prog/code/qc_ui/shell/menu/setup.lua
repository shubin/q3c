menu.setup = {}
menu.setup.doc = LoadDocument("menu/setup.rml")
menu.setup.watch = {}

function menu.setup:show()
  print("SHOW")
  table.insert(self:watch, Cvar_Watch("name", function() self:update_param("name") end))
--  Cvar_Watch("name", self:var_changed)
--  print("UPDATE")
--  self:update_param("name")
end

function menu.setup:update_param(qname)
  local elem = self.doc:GetElementById("param_"..name);
  elem:SetAttribute("value", trap_Cvar_VariableString(qname))  
end

function menu.setup:hide()
  --Cvar_Unwatch("name", function () do self:var_changed("name") end )
end

function menu.setup:back()
  self.doc:Hide()
  menu.main.doc:Show()
end

function menu.setup:apply()
  local name_elem = self.doc:GetElementById("param_name");
  trap_Cvar_Set("name", name_elem:GetAttribute("value"))
end

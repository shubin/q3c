menu.setup = {}
menu.setup.doc = LoadDocument("menu/setup.rml")
menu.setup.watch = {}

function menu.setup:add_watch(var_name)
  table.insert(self.watch, Cvar_Watch(var_name, function() self:update_param(var_name) end))
end

function menu.setup:clear_watch()
  for _,w in pairs(self.watch) do
    Cvar_Unwatch(w)
  end
end

function menu.setup:show()
  self:add_watch("name")
  self:update_param("name")
end

function menu.setup:update_param(qname)
  local elem = self.doc:GetElementById("$"..qname);
  elem:SetAttribute("value", trap_Cvar_VariableString(qname))  
end

function menu.setup:hide()
  self:clear_watch()
end

function menu.setup:back()
  self.doc:Hide()
  menu.main.doc:Show()
end

function menu.setup:apply()
  local name_elem = self.doc:GetElementById("$name");
  trap_Cvar_Set("name", name_elem:GetAttribute("value"))
end

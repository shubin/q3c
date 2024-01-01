local menu = {}
menu.doc = LoadDocument("menu/setup.rml")
menu.watch = {}

function menu:add_watch(var_name)
  table.insert(self.watch, Cvar_Watch(var_name, function() self:update_param(var_name) end))
end

function menu:clear_watch()
  for _,w in pairs(self.watch) do
    Cvar_Unwatch(w)
  end
end

function menu:show()
  self:add_watch("name")
  self:update_param("name")
end

function menu:update_param(qname)
  local elem = self.doc:GetElementById("$"..qname);
  elem:SetAttribute("value", trap_Cvar_VariableString(qname))  
end

function menu:hide()
  self:clear_watch()
end

function menu:back()
  self.doc:Hide()
  ShowMenu("main")
end

function menu:apply()
  local name_elem = self.doc:GetElementById("$name");
  trap_Cvar_Set("name", name_elem:GetAttribute("value"))
end

return menu

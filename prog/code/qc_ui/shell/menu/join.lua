sb = require("serverbrowser")

local menu = {}

menu.doc = LoadDocument("menu/join.rml")
menu.serverlist = menu.doc:GetElementById("serverlist")

function menu:show()
end

function menu:hide()
  sb:stoprefresh()
end

function menu:back()
  self.doc:Hide()
  ShowMenu("main")
end

function menu:refresh()
  sb:refresh(function(servers) self:onupdate(servers) end)
end

function menu:getservers()
  for i = 1, self.numservers do
    table.insert(self.servers, trap_LAN_GetServerAddressString(AS_GLOBAL, i - 1))
  end
end

function menu:onupdate(servers)
  local s = ""
  for i, v in pairs(servers) do
    s = s .. string.format("server #%s: %s\n", i, v)
  end
  self.serverlist:SetAttribute("value", s)
end

return menu

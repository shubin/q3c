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

function menu:refcb(info, time)
  if info then
    print("PING:", time)
    for k,v in pairs(info) do
      print(k, v)
    end
    table.insert(self.servers, info)
  else
    print(string.format("-- refresh done, %d servers", #self.servers))
    print("ping queue: ", trap_LAN_GetPingQueueCount() )
  end
end

function menu:refresh()
  self.servers = {}
  sb:refresh(function(...) self:refcb(...)end)
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

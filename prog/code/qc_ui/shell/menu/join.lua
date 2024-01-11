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

function menu:addcells(tr, cells)
  local td
  for i = 1, #cells do
    td = tr:AppendChild(self.doc:CreateElement("td"))
    td:AppendChild(self.doc:CreateTextNode(tostring(cells[i])))
  end
end

function menu:buildlist()
  local tab = self.doc:GetElementById("serverlist")
  tab.inner_rml = ""
  local tr, td
  tr = tab:AppendChild(self.doc:CreateElement("tr"))
  tr:SetAttribute("class", "hdr")
  -- header
  self:addcells(tr, { "Address", "Hostname", "Ping" })
  -- add server info
  for i = 1, #self.servers do
    local sv = self.servers[i]
    tr = tab:AppendChild(self.doc:CreateElement("tr"))
    self:addcells(tr, { sv["/addr"], sv.hostname, sv["/time"] })
  end
end

-- this callback receives no args when the server list is complete
function menu:refresh_callback(addr, time, info)
  if info then
    info["/addr"] = addr
    info["/time"] = time
    table.insert(self.servers, info)
  else
    self:buildlist()
  end
end

function menu:refresh()
  self.servers = {}
  sb:refresh(function(...) self:refresh_callback(...)end)
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

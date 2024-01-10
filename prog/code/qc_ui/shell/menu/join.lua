local AS_LOCAL			= 0
local AS_MPLAYER		= 1
local AS_GLOBAL			= 2
local AS_FAVORITES		= 3

local S_IDLE	= 0
local S_QUERY	= 1
local S_PING	= 2

local menu = {}
menu.doc = LoadDocument("menu/join.rml")
menu.serverlist = menu.doc:GetElementById("serverlist")
menu.servers = {}
menu.numservers = 0
menu.state = S_IDLE

function menu:back()
  self.doc:Hide()
  ShowMenu("main")
end

function menu:refresh()
  local protocol
  protocol = trap_Cvar_VariableString("debug_protocol")
  if protocol == "" then
    protocol = trap_Cvar_VariableString("protocol")
  end
  local command = "globalservers 0 "..protocol.." full empty"
  trap_Cmd_ExecuteText(EXEC_APPEND, command)
  self.serverlist:SetAttribute("value", "Refreshing...")
  self.numservers = 0
  self.servers = {}
  self.state = S_QUERY
end

function menu:getservers()
  for i = 1, self.numservers do
    table.insert(self.servers, trap_LAN_GetServerAddressString(AS_GLOBAL, i - 1))
  end
end

function menu:update()
  if self.state == S_QUERY then
    self.numservers = trap_LAN_GetServerCount(AS_GLOBAL)
    if self.numservers > 0 then
      self:getservers()
      print("numservers", self.numservers)
      local s = ""
      for i, v in pairs(self.servers) do
        s = s .. string.format("server #%s: %s\n", i, v)
      end
      self.serverlist:SetAttribute("value", s)
      self.state = S_IDLE
    end
  end  
end

return menu

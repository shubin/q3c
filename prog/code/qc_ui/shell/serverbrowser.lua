local AS_LOCAL			= 0
local AS_MPLAYER		= 1
local AS_GLOBAL			= 2
local AS_FAVORITES		= 3

local S_IDLE	= 0
local S_QUERY	= 1
local S_PING	= 2

local QUERY_TIMEOUT	= 5000
local MAX_PINGREQUESTS = 32

local serverbrowser = {}

serverbrowser.state = S_IDLE

function serverbrowser:refreshhook(source, realtime)
  local numservers = trap_LAN_GetServerCount(AS_GLOBAL)
  local servers = {}
  if numservers > 0 then
    for i = 1, numservers do
      table.insert(servers, trap_LAN_GetServerAddressString(source, i - 1))
    end
    self.state = S_IDLE
    RemoveRefreshHook(self.rh)
    if self.cb then
      self.cb(servers)
    end
    return
  end
  if realtime - self.querystart > QUERY_TIMEOUT then
    if self.cb then
      self.cb({})
    end
    self:stoprefresh()
  end
end

function serverbrowser:refresh(callback)
  if self.state ~= S_IDLE then
    return
  end
  local protocol
  protocol = trap_Cvar_VariableString("debug_protocol")
  if protocol == "" then
    protocol = trap_Cvar_VariableString("protocol")
  end
  local command = "globalservers 0 "..protocol.." full empty"
  trap_Cmd_ExecuteText(EXEC_APPEND, command)
  self.state = S_QUERY
  self.querystart = uis.realtime
  self.rh = InstallRefreshHook(function(rt) self:refreshhook(AS_GLOBAL, rt) end)
  self.cb = callback
  return true
end

function serverbrowser:stoprefresh()
  if self.state == S_QUERY then
    RemoveRefreshHook(self.rh)
    self.rh = nil
    self.cb = nil
    self.state = S_IDLE
    return
  end
end

return serverbrowser

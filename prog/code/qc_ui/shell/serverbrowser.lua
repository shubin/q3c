local AS_LOCAL			= 0
local AS_MPLAYER		= 1
local AS_GLOBAL			= 2
local AS_FAVORITES		= 3

local S_IDLE	= 0
local S_QUERY	= 1
local S_PING	= 2

local QUERY_TIMEOUT	= 5000
local PING_DELAY = 50
local MAX_PINGREQUESTS = 32
local MAX_PING = 500

local serverbrowser = {}

serverbrowser.state = S_IDLE

function ParseInfoString(infostring, result)
  if result == nil then
    result = {}
  end
  for k,v in infostring:gmatch("\\([^\\]+)\\([^\\]+)") do
    result[k] = v
  end
  return result
end

local function FindEmptySlot(list)
  for k, v in pairs(list) do
    if not next(v) then
      return k, v
    end
  end
end

function serverbrowser:refreshhook(source, realtime)
  if self.state == S_QUERY then
    local numservers = trap_LAN_GetServerCount(AS_GLOBAL)
    if numservers > 0 then
      self.servers = {}  
      self.pinglist = {}
      for i = 1, numservers do
        table.insert(self.servers, trap_LAN_GetServerAddressString(source, i - 1))
        table.insert(self.pinglist, {})
      end
      self.state = S_PING
      self.nextping = realtime
      return
    end
    if realtime - self.querystart > QUERY_TIMEOUT then
      self:stoprefresh()
      return
    end
  elseif self.state == S_PING then
    -- throttle ping requests
    if realtime < self.nextping then
      return
    end
    self.nextping = realtime + PING_DELAY
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
    if self.cb then self.cb(nil) end
    RemoveRefreshHook(self.rh)
    self.rh = nil
    self.cb = nil
    self.state = S_IDLE
    return
  end
end

return serverbrowser

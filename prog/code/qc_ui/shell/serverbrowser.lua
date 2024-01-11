local AS_LOCAL			= 0
local AS_MPLAYER		= 1
local AS_GLOBAL			= 2
local AS_FAVORITES		= 3

local S_IDLE	= 0
local S_QUERY	= 1
local S_PING	= 2

local REFRESH_TIMEOUT	= 5000
local PING_DELAY		= 50
local MAX_PINGREQUESTS	= 32
local MAX_PING			= 500

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
  if self.state ~= S_IDLE and realtime > self.refreshtime then
    self:stoprefresh()
    return
  end
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
  elseif self.state == S_PING then
    -- throttle ping requests
    if realtime < self.nextping then
      return
    end
    self.nextping = realtime + PING_DELAY
    -- process ping results
    for i = 1, MAX_PINGREQUESTS do
      local addr, time = trap_LAN_GetPing( i - 1 )
      if addr:len() == 0 then
        -- ignore empty or pending pings
        goto continue        
      end
      -- find ping result in our local list
      local pr = nil
      for k, v in pairs(self.pinglist) do
        if v.addr == addr then
          pr = v
          break
        end
      end
      if pr then
        -- found it
        if time == 0 then
          time = realtime - pr.start
          if time < MAX_PING then
            -- still waiting
            goto continue
          end          
        end
        local info = {}
        if time > MAX_PING then
          time = MAX_PING
        else
          local infostring = trap_LAN_GetPingInfo(i - 1)
          info = ParseInfoString(infostring) 
        end
        self.cb(info, time)
      end
      trap_LAN_ClearPing(i - 1)
    ::continue::
    end
    -- get results of servers query
	  -- counts can increase as servers respond
    self.numqueriedservers = trap_LAN_GetServerCount(source)
    for i = 1, MAX_PINGREQUESTS do
      if self.currentping >= self.numqueriedservers then break end
      if trap_LAN_GetPingQueueCount() >= MAX_PINGREQUESTS then
        -- ping queue is full
        break
      end
      -- find empty slot
      local pr = nil
      for k, v in pairs(self.pinglist) do
        if not v.addr then
          pr = v
          break
        end
      end
      if not pr then
        -- no empty slots available yet - wait for timeout
        break
      end
      pr.addr = trap_LAN_GetServerAddressString(source, self.currentping)
      pr.start = realtime
      trap_Cmd_ExecuteText(EXEC_NOW, string.format("ping %s\n", pr.addr))
      self.currentping = self.currentping + 1
    end
    
    if trap_LAN_GetPingQueueCount() == 0 then
      self:stoprefresh()
      return
    end
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
  self.rh = InstallRefreshHook(function(rt) self:refreshhook(AS_GLOBAL, rt) end)
  self.cb = callback
  self.currentping = 0
  self.nextpingtime = 0
  self.numqueriedservers = 0
  self.refreshtime = uis.realtime + REFRESH_TIMEOUT
  return true
end

function serverbrowser:stoprefresh()
  if self.cb then self.cb(nil) end
  RemoveRefreshHook(self.rh)
  self.rh = nil
  self.cb = nil
  self.state = S_IDLE
  return
end

return serverbrowser

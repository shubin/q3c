function print(...)
  local args = table.pack(...)
  if #args == 0 then
    trap_Print("\n")
    return
  end
  local line = args[1]
  for i = 2, #args do
    line = line.." "..args[i]
  end
  line = line.."\n"
  trap_Print(line)
end

function clamp(v, low, high)
  return math.min(math.max(v, low), high)
end

function LoadDocument(name)
  local doc = gContext:LoadDocument(name)
  local titleElement = doc:GetElementById("title")
  if titleElement then
    titleElement.inner_rml = doc.title
  end
  return doc
end

function ShowMenu(nmenu)
  gMenu[nmenu].doc:Show()
end

-- Cvar change watching

local watchlist = {}

function Cvar_Watch(var_name, callback)
  local w = watchlist[var_name]
  if w == nil then
    watchlist[var_name] = {}
    w = watchlist[var_name]
    trap_Cvar_Watch(var_name, true)
  end
  w[callback] = true

  return { var_name, callback }
end

function Cvar_Unwatch(handle)
  local var_name, callback = table.unpack(handle, 1, 2)
  local w = watchlist[var_name]
  if w == nil then
    return
  end
  w[callback] = nil
  if next(w) == nil then
    trap_Cvar_Watch(var_name, false)
    watchlist[var_name] = nil
  end
end

function UI_CvarChanged(var_name)
  local w = watchlist[var_name]
  if w ~= nil then
    for func,_ in pairs(w) do
      func(var_name)
    end
  end
end

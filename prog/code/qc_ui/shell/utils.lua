function print(...)
  local args = table.pack(...)
  local line = ""
  for i = 1, args.n do
    line = line..args[i]
  end
  line = line.."\n"
  trap_Print(line)
end

function clamp(v, low, high)
  return math.min(math.max(v, low), high)
end

function LoadDocument(name)
  local doc = gContext:LoadDocument("shell/"..name)
  --doc.GetElementById("#title").inner_rml = doc.title
  return doc
end

watchlist = {}

function Cvar_Watch(var_name, callback)
  local w = watchlist[var_name]
  if w == nil then
    watchlist[var_name] = {}
    w = watchlist[var_name]
    trap_Cvar_Watch(var_name, true)
  end
  w[callback] = true
end

function Cvar_Unwatch(var_name, callback)
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

function UI_CvarChanged()
  local var_name = trap_Argv(0)
  local w = watchlist[var_name]
  if w ~= nil then
    for func,_ in pairs(w) do
      func(var_name)
    end
  end
end

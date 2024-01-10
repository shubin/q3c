rmlui:LoadFontFace("assets/NotoSansDisplay.ttf")

gGlConfig = trap_GetGlconfig()
gContext = rmlui:CreateContext("main", Vector2i.new(gGlConfig.vidWidth, gGlConfig.vidHeight))

-- load the debugger if needed
local ui_rmldebug = vmCvar_t()
trap_Cvar_Register( ui_rmldebug, "ui_rmldebug", "0", 0 )

if ui_rmldebug.integer ~= 0 then
  rmlui:InitialiseDebugger(gContext)
  rmlui:SetDebuggerVisible(true)
end

-- load all the menus
gMenu = {}
local menulist = { "main", "setup", "start", "join", "ingame" }
for _,v in pairs(menulist) do
  gMenu[v] = require("menu."..v)
end

-- execution context

ui_cookie = vmCvar_t()
trap_Cvar_Register( ui_cookie, "__ui_cookie", "", 0 )

gFreshStart = ui_cookie.string == ""
trap_Cvar_Set( "__ui_cookie", "1")

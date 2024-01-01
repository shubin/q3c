rmlui:LoadFontFace("assets/LatoLatin-Regular.ttf")
rmlui:LoadFontFace("assets/LatoLatin-Bold.ttf")
rmlui:LoadFontFace("assets/NotoEmoji-Regular.ttf", true)

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
local menulist = { "main", "setup", "start", "ingame" }
for _,v in pairs(menulist) do
  gMenu[v] = require("menu."..v)
end

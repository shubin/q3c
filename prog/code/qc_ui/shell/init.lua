rmlui:LoadFontFace("shell/assets/LatoLatin-Regular.ttf")
rmlui:LoadFontFace("shell/assets/LatoLatin-Bold.ttf")
rmlui:LoadFontFace("shell/assets/NotoEmoji-Regular.ttf", true)

gGlConfig = trap_GetGlconfig()
gContext = rmlui:CreateContext("main", Vector2i.new(gGlConfig.vidWidth, gGlConfig.vidHeight))

local ui_rmldebug = vmCvar_t()
trap_Cvar_Register( ui_rmldebug, "ui_rmldebug", "0", 0 )

if ui_rmldebug.integer ~= 0 then
  rmlui:InitialiseDebugger(gContext)
  rmlui:SetDebuggerVisible(true)
end

menu = {}
require("shell.menu.main")
require("shell.menu.start")
require("shell.menu.setup")
require("shell.menu.ingame")

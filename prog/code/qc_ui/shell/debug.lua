local ui_debug = trap_Cvar_VariableString("ui_debug")

if ui_debug and ui_debug ~= "" and socket then
  gDebuggerEnabled = true
  trap_Print("^3*** Initializing Lua debugger ***\n")
  require("shell.mobdebug").start(ui_debug)
end

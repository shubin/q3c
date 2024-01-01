local menu = {}

menu.doc = LoadDocument("menu/ingame.rml")

function menu:disconnect()
  self.doc:Hide()
  UI_SetActiveMenu( UIMENU_NONE )
  trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n")
end

function menu:resume()
  self.doc:Hide()
  UI_SetActiveMenu( UIMENU_NONE )
end

function menu:keydown(event)
  if event.parameters.key_identifier == rmlui.key_identifier.ESCAPE then
    self:resume()
    return true
  end
  return false
end

return menu

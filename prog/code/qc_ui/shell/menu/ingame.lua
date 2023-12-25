menu.ingame = {}
menu.ingame.doc = LoadDocument("menu/ingame.rml")

function menu.ingame:disconnect()
  self.doc:Hide()
  UI_SetActiveMenu( UIMENU_NONE )
  trap_Cmd_ExecuteText(EXEC_APPEND, "disconnect\n")
end

function menu.ingame:resume()
  self.doc:Hide()
  UI_SetActiveMenu( UIMENU_NONE )
end

function menu.ingame:keydown(event)
  if event.parameters.key_identifier == rmlui.key_identifier.ESCAPE then
    self:resume()
    return true
  end
  return false
end

gMousePos = Vector2i.new(gGlConfig.vidWidth/2, gGlConfig.vidHeight/2)

local modkeys = 0

local function CheckMod(in_key, in_down, q_key, rml_mod)
  if in_key == q_key then
    if in_down then
      modkeys = modkeys | rml_mod
    else
      modkeys = modkeys & ~rml_mod
    end
    return true
  end
end

local function CheckMouse(in_key, in_down, q_key, rml_button)
  if in_key == q_key then
    if in_down then
      gContext:ProcessMouseButtonDown(rml_button, modkeys)
    else
      gContext:ProcessMouseButtonUp(rml_button, modkeys)
    end
    return true
  end
end

local function CheckMouseWheel(key, down)
  if not down then
    return
  end
  if key == K_MWHEELDOWN then
    gContext:ProcessMouseWheel(1, modkeys)
    return true
  end
  if key == K_MWHEELUP then
    gContext:ProcessMouseWheel(-1, modkeys)
    return true
  end
end

local function CheckCharInput(key, down)
  if (key & K_CHAR_FLAG) ~= 0 then
    local chr = key & ~K_CHAR_FLAG
    if (chr > 0x1F and chr < 0x7F) then
      gContext:ProcessTextInput(string.char(chr))      
    end
    if chr == 13 then
      gContext:ProcessTextInput("\n")
    end
    return true
  end
end

local function CheckGeneralInput(key, down)
  local rmlkey = MapKey( key )
  if rmlkey ~= 0 then
    if down then
      gContext:ProcessKeyDown(rmlkey, modkeys)
    else
      gContext:ProcessKeyUp(rmlkey, modkeys)
    end
    return true
  end
end

function UI_KeyEvent(key, down)
  -- process char input
  if CheckCharInput(key, down) then
    return
  end
  -- process the mod keys
  if CheckMod(key, down, K_CTRL, 1)
  or CheckMod(key, down, K_SHIFT, 2)  
  or CheckMod(key, down, K_ALT, 4)
  then
    return
  end
  -- process mouse buttons
  if CheckMouse(key, down, K_MOUSE1, 0)
  or CheckMouse(key, down, K_MOUSE2, 1)
  or CheckMouse(key, down, K_MOUSE3, 2)
  then
    return
  end
  -- mouse wheel
  if CheckMouseWheel(key, down) then
    return
  end
  -- general input
  CheckGeneralInput(key, down)
end

function UI_MouseEvent(dx, dy)
  gMousePos.x = clamp(gMousePos.x + dx, 0, gGlConfig.vidWidth - 1)
  gMousePos.y = clamp(gMousePos.y + dy, 0, gGlConfig.vidHeight - 1)
  gContext:ProcessMouseMove(gMousePos.x, gMousePos.y, modkeys)
end

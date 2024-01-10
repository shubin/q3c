local pointer = trap_R_RegisterShaderNoMip("shell/assets/pointer");
local pointerSize = Vector2f.new(32, 32)
local pointerHotspot = Vector2f.new(0.3, 0)

local fadetime = nil
local fadestart = nil
local fadeshader = trap_R_RegisterShaderNoMip("white")
local fadecolor = color_t.new(0, 0, 0, 1)

function FadeIn( time )
  fadetime = time
  fadestart = nil
end

local function DoFade(realtime)
  if not fadestart then
    fadestart = realtime
  end
  if fadetime then
    fadecolor.a = clamp(1 - (realtime - fadestart) / fadetime, 0, 1)
    trap_R_SetColor( fadecolor )
    trap_R_DrawStretchPic( 0, 0, gGlConfig.vidWidth, gGlConfig.vidHeight, 0, 0, 1, 1, fadeshader )
    if fadecolor.a == 0 then
      fadetime = nil
    end
  end
end

function UI_Refresh(realtime)
  uis.frametime = realtime - uis.realtime
  uis.realtime = realtime  
  if gCurrentMenu == UIMENU_NONE then
    return
  end
  gContext:Update()
  gContext:Render()
  trap_R_DrawStretchPic(
    gMousePos.x - pointerSize.x * pointerHotspot.x,
    gMousePos.y - pointerSize.y * pointerHotspot.y,
    pointerSize.x, pointerSize.y,
    0, 0, 1, 1,
    pointer
  )
  if pi then
    UI_DrawPlayer(10, 10, 600, 600, pi, realtime/2)
  end
  DoFade(realtime)
end

function UI_SetMousePointer(nptr)
  --print("Set pointer", nptr)
end

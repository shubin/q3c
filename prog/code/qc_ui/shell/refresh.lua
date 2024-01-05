local pointer = trap_R_RegisterShaderNoMip("shell/assets/pointer");
local pointerSize = Vector2f.new(32, 32)
local pointerHotspot = Vector2f.new(0.3, 0)

function UI_Refresh(realtime)
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
    UI_DrawPlayer( 10, 10, 600, 600, pi, realtime/2 )
  end
end

function UI_SetMousePointer(nptr)
  --print("Set pointer", nptr)
end

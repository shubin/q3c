local pointer = trap_R_RegisterShaderNoMip("shell/assets/pointer");
local pointerSize = Vector2f.new(48, 48)
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
end

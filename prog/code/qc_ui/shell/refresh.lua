gPointer = trap_R_RegisterShaderNoMip("shell/assets/pointer");
gPointerSize = Vector2f.new(48, 48)
gPointerHotspot = Vector2f.new(0.3, 0)

function UI_Refresh(realtime)
  if gCurrentMenu == UIMENU_NONE then
    return
  end
  gContext:Update()
  gContext:Render()
  trap_R_DrawStretchPic(
    gMousePos.x - gPointerSize.x * gPointerHotspot.x,
    gMousePos.y - gPointerSize.y * gPointerHotspot.y,
    gPointerSize.x, gPointerSize.y,
    0, 0, 1, 1,
    gPointer
  )
end

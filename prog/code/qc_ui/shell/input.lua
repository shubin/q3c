gMousePos = Vector2i.new(gGlConfig.vidWidth/2, gGlConfig.vidHeight/2)

function UI_KeyEvent(key, down)
  if key == K_MOUSE1 then
    if down then
        gContext:ProcessMouseButtonDown(0, 0)
    else
        gContext:ProcessMouseButtonUp(0, 0)
    end
  end
end

function UI_MouseEvent(dx, dy)
  gMousePos.x = clamp(gMousePos.x + dx, 0, gGlConfig.vidWidth - 1)
  gMousePos.y = clamp(gMousePos.y + dy, 0, gGlConfig.vidHeight - 1)
  gContext:ProcessMouseMove(gMousePos.x, gMousePos.y, 0)
end

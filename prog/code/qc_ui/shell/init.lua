rmlui:LoadFontFace("shell/assets/LatoLatin-Regular.ttf")
rmlui:LoadFontFace("shell/assets/LatoLatin-Bold.ttf")
rmlui:LoadFontFace("shell/assets/NotoEmoji-Regular.ttf", true)

gGlConfig = trap_GetGlconfig()
gContext = rmlui:CreateContext("main", Vector2i.new(gGlConfig.vidWidth, gGlConfig.vidHeight))
gDoc = gContext:LoadDocument("shell/assets/demo.rml")
gDoc:Show()

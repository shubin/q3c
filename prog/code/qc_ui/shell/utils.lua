function clamp(v, low, high)
  return math.min(math.max(v, low), high)
end

function LoadDocument(name)
  local doc = gContext:LoadDocument("shell/"..name)
  --doc.GetElementById("#title").inner_rml = doc.title
  return doc
end

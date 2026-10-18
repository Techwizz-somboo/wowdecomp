-- Fallbacks for helpers the Noggit widgets call, the map runtime registers the real ones.

EditorGetCursorPosition = EditorGetCursorPosition or GetCursorPosition;
EditorGetTime = EditorGetTime or GetTime;
EditorIsMouseButtonDown = EditorIsMouseButtonDown or function(button)
	return IsMouseButtonDown(button or "LeftButton");
end;

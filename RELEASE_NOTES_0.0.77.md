# Cricket 0.0.77

## Native Haiku version
- Popup menus (right-click on a server, a channel, a nickname, the chat) have rounded corners, like SnakeTracker's own menus. This needs SnakeTracker 1.0.64 or later, with "Snake look in all programs" and rounded corners on in its settings; without it the menus are Haiku's, as before.
- Resizing the window is smoother with a long scrollback: while the edge is dragged the newest lines, the ones in view, are wrapped for the new width within a few milliseconds, and the older ones follow when the dragging pauses.
- The window can not be made smaller than 600x380 (below that the lists, the chat and the input box had no room left).

## Qt version (Haiku)
- No changes since 0.0.76.

## Packages
- `cricket-0.0.77-1-x86_64.hpkg` and `cricket-0.0.77-1-x86_gcc2.hpkg` (native Haiku)
- `cricket_qt-0.0.77-1-x86_64.hpkg` (Qt 6, 64-bit only)

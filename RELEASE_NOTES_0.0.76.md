# Cricket 0.0.76

## Native Haiku version
- Resizing the window is smoother. While the edge is dragged, a long scrollback is not wrapped again at every step and the background picture (and the rounded box) is stretched instead of scaled again; both are made properly once the dragging pauses. A short channel (150 lines or fewer) is still wrapped live.
- Chat logging: a line is now logged according to the server it belongs to. Before, the server selected in the list decided whether it was logged and which file it went to, so lines arriving on a network in the background could be missed or end up in another network's file.

## Qt version (Haiku)
- Resizing the window is smoother: the background picture is scaled quickly while the edge is dragged and smoothly once when it pauses.

## Packages
- `cricket-0.0.76-1-x86_64.hpkg` and `cricket-0.0.76-1-x86_gcc2.hpkg` (native Haiku)
- `cricket_qt-0.0.76-1-x86_64.hpkg` (Qt 6, 64-bit only)

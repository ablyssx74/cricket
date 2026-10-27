# Cricket 0.0.75

## Qt version (Haiku)
- The frames around the boxes (server list, chat, user list, topic, input) now follow the theme. With a dark theme they were drawn in a light grey that stood out; they are now a subtle line made from the window and text colors, and the same for a light theme.

## Native Haiku version (changes since 0.0.72)
- Nicknames get a color of their own, the same one every time (as in the Qt version).
- A private message opens its tab under the server of the tab in front.
- The color tags no longer leak into a tab's name.
- The rounded frames are drawn again whole when the window is resized (the old bottom corners no longer stay behind as speckles along the left edge).

## Packages
- `cricket-0.0.75-1-x86_64.hpkg` and `cricket-0.0.75-1-x86_gcc2.hpkg` (native Haiku)
- `cricket_qt-0.0.75-1-x86_64.hpkg` (Qt 6, 64-bit only)

The Qt version has been behind since 0.0.72; this package brings it up to date.

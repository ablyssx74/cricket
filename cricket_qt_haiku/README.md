# Cricket for Haiku (Qt 6)

The Qt 6 version of Cricket (the same code as `linux/`) built for Haiku. The native Haiku
version in the repository root is unchanged. This one reads the same
`~/config/settings/cricket/cricketConfig.txt` and can be installed beside it.

## Build and package (on Haiku)
```
pkgman install qt6_base_devel cmake aspell_devel
cd cricket_qt_haiku
sh make-package.sh
```
This builds with g++ (Haiku's default clang has no `ld.lld`) and makes
`build/cricket_qt-<version>-1-x86_64.hpkg`. Install it for the whole system.

## What differs from the Linux port
- `portmapper_compat.h` uses Haiku's own types and `snooze()` instead of its stand-ins.
- Links `libnetwork` for sockets.
- `haikumousefix.h`: Qt's Haiku platform plugin can report mouse moves with no button held
  while one is down, so splitters and scroll bars could be pressed but not dragged.
  Buttons are put back on those moves until release.
- `haikustyle.h`: wider splitter handle and scroll bar, which are very thin in Qt's Haiku style.
- App signature `application/x-vnd.cricket-qt`, version and icon are added as resources
  (`packaging/cricket_qt.rdef`).

Buttons drawn through a third-party control look that uses alpha overlay (SnakeControlLook before
snaketracker 1.0.32) show stripes in Qt programs; that is fixed in the control look, not here.

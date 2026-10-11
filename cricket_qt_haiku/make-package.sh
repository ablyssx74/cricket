#!/bin/sh
# Builds cricket_qt and packages it as an hpkg. Run on Haiku, in this folder.
#   needs: qt6_base_devel, cmake, aspell_devel (and g++; Haiku's default clang lacks ld.lld)
set -e
cd "$(dirname "$0")"
version=$(sed -n 's/.*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p' ../VERSION | head -1)
revision=${REVISION:-1}
arch=$(uname -m | sed 's/BePC/x86_gcc2/; s/x86_64/x86_64/')
out=build/package

CXX=g++ CC=gcc cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCRICKET_WITH_DBUS=OFF
cmake --build build -j"$(nproc)"

# app signature, version and icon go into the program's resources
rc -o build/cricket_qt.rsrc packaging/cricket_qt.rdef
cp build/cricket build/cricket_qt
xres -o build/cricket_qt build/cricket_qt.rsrc
mimeset -f build/cricket_qt

rm -rf "$out"
mkdir -p "$out/apps" "$out/bin" "$out/data/deskbar/menu/Applications" "$out/data/documentation/packages/cricket_qt"
cp build/cricket_qt "$out/apps/cricket_qt"
ln -s ../apps/cricket_qt "$out/bin/cricket_qt"
ln -s ../../../../apps/cricket_qt "$out/data/deskbar/menu/Applications/cricket_qt"
cp ../LICENSE "$out/data/documentation/packages/cricket_qt/LICENSE"
sed -e "s/@VERSION@/$version/g" -e "s/@REVISION@/$revision/g" -e "s/@YEAR@/$(date +%Y)/" packaging/cricket_qt.tpl > "$out/.PackageInfo"
rm -f "build/cricket_qt-$version-$revision-$arch.hpkg"
package create -C "$out" "build/cricket_qt-$version-$revision-$arch.hpkg"
echo "built build/cricket_qt-$version-$revision-$arch.hpkg"

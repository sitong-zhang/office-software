#!/usr/bin/env bash
# Local-only compile check: cross-compile all three executables with MinGW.
# Mirrors the exact CXXFLAGS/link flags from build-windows.sh, minus signing,
# asset generation and the engine payload. Used to validate refactors locally.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
CXXFLAGS=(-std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE
          -finput-charset=UTF-8 -fexec-charset=UTF-8
          -static -static-libgcc -static-libstdc++ -lwinpthread -I"$ROOT")
mkdir -p "$BUILD"

echo "[1/3] launcher + self-test"
"$RC" "$ROOT/app/launcher/app.rc" -O coff -o "$BUILD/app.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/app/launcher/main.cpp" \
  "$ROOT/app/third_party/pugixml/pugixml.cpp" "$BUILD/app.res.o" \
  -o "$BUILD/ChenyiOffice.exe" \
  -lwinhttp -lbcrypt -lwintrust -lcrypt32 -lshell32 -lole32 -luuid -lshlwapi -lgdi32 -luser32 -ladvapi32 -lgdiplus -ldwmapi

echo "[2/3] uninstaller"
"$RC" "$ROOT/uninstaller/app.rc" -O coff -o "$BUILD/uninstall.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/uninstaller/main.cpp" "$BUILD/uninstall.res.o" \
  -o "$BUILD/Uninstall.exe" \
  -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32 -lshlwapi -lgdiplus -ldwmapi

echo "[3/3] installer"
printf '#define MSI_PRODUCT_CODE L"{00000000-0000-0000-0000-000000000000}"\n' > "$BUILD/product_code.h"
"$RC" "$ROOT/installer/app.rc" -O coff -o "$BUILD/setup.res.o"
"$CXX" "${CXXFLAGS[@]}" -I"$BUILD" "$ROOT/installer/main.cpp" "$BUILD/setup.res.o" \
  -o "$BUILD/ChenyiOfficeSetup-base.exe" \
  -lbcrypt -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32 -lshlwapi -lgdiplus -ldwmapi

echo "OK: all three executables compiled"

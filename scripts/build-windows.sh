#!/usr/bin/env bash
# Build the Chenyi Office Windows launcher, uninstaller, and installer from Linux.
# Requirements: mingw-w64, msitools, python3 + Pillow, curl.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
CACHE="$BUILD/cache"
DIST="$ROOT/dist"
LO_VERSION="${LO_VERSION:-26.8.3}"
LO_MSI="LibreOffice_${LO_VERSION}_Win_x86-64.msi"
LO_URL="${LO_URL:-https://mirrors.tuna.tsinghua.edu.cn/libreoffice/libreoffice/stable/${LO_VERSION}/win/x86_64/${LO_MSI}}"
VERSION="0.1.1"
OUT="$DIST/ChenyiOffice-${VERSION}-x64.exe"

CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
CXXFLAGS=(-std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE
          -finput-charset=UTF-8 -fexec-charset=UTF-8
          -static -static-libgcc -static-libstdc++ -lwinpthread)

mkdir -p "$BUILD" "$CACHE" "$DIST"

echo "[1/7] Generating brand assets"
python3 "$ROOT/scripts/make-brand-assets.py"

echo "[2/7] Building launcher"
"$RC" "$ROOT/app/launcher/app.rc" -O coff -o "$BUILD/app.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/app/launcher/main.cpp" "$BUILD/app.res.o" \
  -o "$BUILD/ChenyiOffice.exe" \
  -lwinhttp -lbcrypt -lshell32 -lole32 -luuid -lshlwapi -lgdi32 -luser32 -ladvapi32

echo "[3/7] Building uninstaller"
"$RC" "$ROOT/uninstaller/app.rc" -O coff -o "$BUILD/uninstall.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/uninstaller/main.cpp" "$BUILD/uninstall.res.o" \
  -o "$BUILD/Uninstall.exe" \
  -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32

echo "[4/7] Preparing LibreOffice engine"
if [ ! -f "$CACHE/$LO_MSI" ]; then
  curl -L --fail --retry 5 --retry-delay 5 --retry-all-errors \
    -o "$CACHE/$LO_MSI.part" "$LO_URL"
  mv "$CACHE/$LO_MSI.part" "$CACHE/$LO_MSI"
fi

echo "[5/7] Reading engine product code"
PRODUCT_CODE="$(msiinfo export "$CACHE/$LO_MSI" Property | tr -d '\r' | awk -F'\t' '$1=="ProductCode"{print $2}')"
if [ -z "$PRODUCT_CODE" ]; then
  echo "Could not read ProductCode from $LO_MSI" >&2
  exit 1
fi
printf '#define MSI_PRODUCT_CODE L"%s"\n' "$PRODUCT_CODE" > "$BUILD/product_code.h"

echo "[6/7] Building installer shell"
"$RC" "$ROOT/installer/app.rc" -O coff -o "$BUILD/setup.res.o"
"$CXX" "${CXXFLAGS[@]}" -I"$BUILD" "$ROOT/installer/main.cpp" "$BUILD/setup.res.o" \
  -o "$BUILD/ChenyiOfficeSetup-base.exe" \
  -lbcrypt -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32

echo "[7/7] Appending engine payload"
python3 - "$BUILD/ChenyiOfficeSetup-base.exe" "$CACHE/$LO_MSI" "$OUT" <<'PY'
import hashlib
import pathlib
import struct
import sys

base_path, msi_path, out_path = map(pathlib.Path, sys.argv[1:4])
base = base_path.read_bytes()
msi = msi_path.read_bytes()
footer = struct.pack("<8sQQ", b"CHNYSET1", len(base), len(msi))
out = pathlib.Path(out_path)
out.write_bytes(base + msi + footer)
digest = hashlib.sha256(out.read_bytes()).hexdigest()
out.with_suffix(out.suffix + ".sha256").write_text(f"{digest}  {out.name}\n")
print(f"{out}  {out.stat().st_size} bytes")
print(f"sha256 {digest}")
PY

echo "Done: $OUT"

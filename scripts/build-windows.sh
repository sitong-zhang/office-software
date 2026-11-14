#!/usr/bin/env bash
# Build and sign the Chenyi Office Windows installer from Linux.
# Requirements: mingw-w64, msitools, python3 + Pillow, curl, osslsigncode, openssl.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$ROOT/build"
CACHE="$BUILD/cache"
DIST="$ROOT/dist"
LO_VERSION="${LO_VERSION:-26.8.3}"
LO_MSI="LibreOffice_${LO_VERSION}_Win_x86-64.msi"
LO_URL="${LO_URL:-https://mirrors.tuna.tsinghua.edu.cn/libreoffice/libreoffice/stable/${LO_VERSION}/win/x86_64/${LO_MSI}}"
VERSION="0.1.2"
OUT="$DIST/ChenyiOffice-${VERSION}-x64.exe"

PRODUCT_NAME="Chenyi Office（胡辰逸定制版）"
HOMEPAGE="https://sitong-zhang.github.io/office-software/"
SIGN_PFX="${SIGN_PFX:-$BUILD/signing/chenyi.pfx}"
SIGN_PASS="${SIGN_PFX_PASS:-chenyi-signing-key}"
SIGN_CERT="${SIGN_CERT:-$BUILD/signing/chenyi.crt}"

CXX=x86_64-w64-mingw32-g++
RC=x86_64-w64-mingw32-windres
CXXFLAGS=(-std=c++17 -O2 -municode -mwindows -DUNICODE -D_UNICODE
          -finput-charset=UTF-8 -fexec-charset=UTF-8
          -static -static-libgcc -static-libstdc++ -lwinpthread)

mkdir -p "$BUILD" "$CACHE" "$DIST"

sign_file() {
  local input="$1"
  if [ ! -f "$SIGN_PFX" ]; then
    echo "  no signing certificate at $SIGN_PFX; leaving unsigned"
    return 0
  fi
  if timeout 45 osslsigncode sign -pkcs12 "$SIGN_PFX" -pass "$SIGN_PASS" \
      -n "$PRODUCT_NAME" -i "$HOMEPAGE" -t http://timestamp.digicert.com -h sha256 \
      -in "$input" -out "$input.signed" >/dev/null 2>&1; then
    mv "$input.signed" "$input"
    echo "  signed (timestamped): $(basename "$input")"
  elif timeout 45 osslsigncode sign -pkcs12 "$SIGN_PFX" -pass "$SIGN_PASS" \
      -n "$PRODUCT_NAME" -i "$HOMEPAGE" -h sha256 \
      -in "$input" -out "$input.signed" >/dev/null 2>&1; then
    mv "$input.signed" "$input"
    echo "  signed (no timestamp): $(basename "$input")"
  else
    rm -f "$input.signed"
    echo "  signing failed: $(basename "$input")" >&2
    return 1
  fi
}

echo "[1/8] Generating brand assets"
python3 "$ROOT/scripts/make-brand-assets.py"

echo "[2/8] Building and signing launcher"
"$RC" "$ROOT/app/launcher/app.rc" -O coff -o "$BUILD/app.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/app/launcher/main.cpp" "$BUILD/app.res.o" \
  -o "$BUILD/ChenyiOffice.exe" \
  -lwinhttp -lbcrypt -lshell32 -lole32 -luuid -lshlwapi -lgdi32 -luser32 -ladvapi32
sign_file "$BUILD/ChenyiOffice.exe"

echo "[3/8] Building and signing uninstaller"
"$RC" "$ROOT/uninstaller/app.rc" -O coff -o "$BUILD/uninstall.res.o"
"$CXX" "${CXXFLAGS[@]}" "$ROOT/uninstaller/main.cpp" "$BUILD/uninstall.res.o" \
  -o "$BUILD/Uninstall.exe" \
  -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32
sign_file "$BUILD/Uninstall.exe"

echo "[4/8] Preparing LibreOffice engine"
if [ ! -f "$CACHE/$LO_MSI" ]; then
  curl -L --fail --retry 5 --retry-delay 5 --retry-all-errors \
    -o "$CACHE/$LO_MSI.part" "$LO_URL"
  mv "$CACHE/$LO_MSI.part" "$CACHE/$LO_MSI"
fi

echo "[5/8] Reading engine product code"
PRODUCT_CODE="$(msiinfo export "$CACHE/$LO_MSI" Property | tr -d '\r' | awk -F'\t' '$1=="ProductCode"{print $2}')"
if [ -z "$PRODUCT_CODE" ]; then
  echo "Could not read ProductCode from $LO_MSI" >&2
  exit 1
fi
printf '#define MSI_PRODUCT_CODE L"%s"\n' "$PRODUCT_CODE" > "$BUILD/product_code.h"

echo "[6/8] Building installer shell"
"$RC" "$ROOT/installer/app.rc" -O coff -o "$BUILD/setup.res.o"
"$CXX" "${CXXFLAGS[@]}" -I"$BUILD" "$ROOT/installer/main.cpp" "$BUILD/setup.res.o" \
  -o "$BUILD/ChenyiOfficeSetup-base.exe" \
  -lbcrypt -lshell32 -lole32 -luuid -lgdi32 -luser32 -ladvapi32

echo "[7/8] Appending engine payload"
python3 - "$BUILD/ChenyiOfficeSetup-base.exe" "$CACHE/$LO_MSI" "$OUT" <<'PY'
import pathlib
import struct
import sys

base_path, msi_path, out_path = map(pathlib.Path, sys.argv[1:4])
base = base_path.read_bytes()
msi = msi_path.read_bytes()
footer = struct.pack("<8sQQ", b"CHNYSET1", len(base), len(msi))
pathlib.Path(out_path).write_bytes(base + msi + footer)
print(f"  {out_path}  {len(base) + len(msi) + len(footer)} bytes")
PY

echo "[8/8] Signing installer and exporting certificate"
sign_file "$OUT"
if [ -f "$SIGN_CERT" ]; then
  openssl x509 -in "$SIGN_CERT" -outform DER -out "$DIST/ChenyiOffice-code-signing.cer"
fi

sha256sum "$OUT" | tee "$OUT.sha256"
if [ -f "$DIST/ChenyiOffice-code-signing.cer" ]; then
  sha256sum "$DIST/ChenyiOffice-code-signing.cer" | tee "$DIST/ChenyiOffice-code-signing.cer.sha256"
fi

if command -v osslsigncode >/dev/null 2>&1; then
  echo "Signature check:"
  osslsigncode verify -in "$OUT" 2>&1 | grep -E 'Signature verification|Number of signatures|Subject' | head -4 || true
fi

echo "Done: $OUT"

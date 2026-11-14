#!/usr/bin/env python3
"""Fail when the product version drifts between source, resources and docs."""

import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
version = None


def fail(message):
    print(f"version check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


def record(label, found):
    global version
    if not found:
        fail(f"could not read version from {label}")
    if version is None:
        version = found
    elif version != found:
        fail(f"{label} says {found}, expected {version}")


def text(path):
    return (ROOT / path).read_text(encoding="utf-8")


def match(label, path, pattern):
    found = re.search(pattern, text(path), re.MULTILINE)
    record(label, found.group(1) if found else None)


match("launcher", "app/launcher/main.cpp", r'kVersion = L"([^"]+)"')
match("installer", "installer/main.cpp", r'kVersion = L"([^"]+)"')
match("build script", "scripts/build-windows.sh", r'^VERSION="([^"]+)"')

for rc in ("app/launcher/app.rc", "installer/app.rc", "uninstaller/app.rc"):
    content = text(rc)
    file_version = re.search(r"FILEVERSION (\d+),(\d+),(\d+),0", content)
    record(f"{rc} FILEVERSION", ".".join(file_version.groups()) if file_version else None)
    match(f"{rc} ProductVersion", rc, r'VALUE "ProductVersion", "([^"]+)"')

for manifest in (
    "app/launcher/app.manifest",
    "installer/app.manifest",
    "uninstaller/app.manifest",
):
    match(manifest, manifest, r'assemblyIdentity version="(\d+\.\d+\.\d+)\.0"')

match("README badge", "README.md", r"badge/version-([0-9.]+)-")
match("README installer", "README.md", r"ChenyiOffice-([0-9.]+)-x64\.exe")

for path, needle in (
    ("docs/index.html", f"v{version}"),
    ("docs/roadmap.md", f"**v{version}**"),
    ("docs/architecture.md", f"ChenyiOffice-{version}-x64.exe"),
    ("docs/build.md", f"ChenyiOffice-{version}-x64.exe"),
    ("docs/signing.md", f"ChenyiOffice-{version}-x64.exe"),
    ("docs/release-process.md", f"v{version}"),
):
    if needle not in text(path):
        fail(f"{path} does not mention {needle}")

if f"## [{version}]" not in text("CHANGELOG.md"):
    fail(f"CHANGELOG.md has no [{version}] section")

announcements = json.loads(text("announcements.json"))
seen = set()
for entry in announcements.get("announcements", []):
    for field in ("id", "title", "body", "published_at"):
        if not entry.get(field):
            fail(f"announcement missing {field}: {entry.get('id', '<no id>')}")
    if entry["id"] in seen:
        fail(f"duplicate announcement id: {entry['id']}")
    seen.add(entry["id"])

print(f"version {version} is consistent")

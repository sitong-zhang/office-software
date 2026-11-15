#!/usr/bin/env python3
"""Load the upstream Material Design Icons glyph used by the product."""

import re
import subprocess
import tempfile
from functools import lru_cache
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
SOURCE = (
    ROOT
    / "app"
    / "branding"
    / "third-party"
    / "material-design-icons"
    / "description.svg"
)


@lru_cache(maxsize=1)
def path_data():
    """Return the SVG path of the document glyph, geometry untouched."""
    svg = SOURCE.read_text(encoding="utf-8")
    for match in re.finditer(r"<path\b([^>]*?)/?>", svg):
        attributes = match.group(1)
        if 'fill="none"' in attributes:
            continue
        data = re.search(r'd="([^"]+)"', attributes)
        if data:
            return data.group(1)
    raise RuntimeError(f"{SOURCE} does not contain a usable path")


@lru_cache(maxsize=32)
def render(size, color=(31, 92, 70), opacity=1.0):
    """Rasterize the upstream SVG and tint it with the product palette."""
    size = int(size)
    with tempfile.NamedTemporaryFile(suffix=".png", delete=False) as handle:
        raster_path = Path(handle.name)
    try:
        try:
            subprocess.run(
                [
                    "rsvg-convert",
                    "-w",
                    str(size),
                    "-h",
                    str(size),
                    "-f",
                    "png",
                    str(SOURCE),
                    "-o",
                    str(raster_path),
                ],
                check=True,
                capture_output=True,
            )
        except FileNotFoundError as error:
            raise RuntimeError(
                "rsvg-convert is required to rasterize the Material icon. "
                "Install librsvg2-bin."
            ) from error
        glyph = Image.open(raster_path).convert("RGBA")
        glyph.load()
    finally:
        raster_path.unlink(missing_ok=True)

    alpha = glyph.getchannel("A")
    if opacity < 1.0:
        alpha = alpha.point(lambda value: int(value * opacity))
    tinted = Image.new("RGBA", glyph.size, tuple(color) + (0,))
    tinted.putalpha(alpha)
    return tinted

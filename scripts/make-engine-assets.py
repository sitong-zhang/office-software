#!/usr/bin/env python3
"""Generate engine-level UI assets: splash screens and SVG brand marks."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

import material_icon

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "app" / "branding" / "engine"
OUT.mkdir(parents=True, exist_ok=True)

INK = (16, 32, 26)
ACCENT = (88, 184, 146)
CREAM = (244, 241, 232)
MUTED = (156, 170, 160)

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def first_existing(candidates, fallback):
    for path in candidates:
        if Path(path).exists():
            return path
    return fallback


FONT_CJK = first_existing(
    [
        "/usr/share/fonts/opentype/noto-cjk-otf/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Regular.otf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
    ],
    FONT_REG,
)
FONT_CJK_BOLD = first_existing(
    [
        "/usr/share/fonts/opentype/noto-cjk-otf/NotoSansCJKsc-Bold.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJKsc-Bold.otf",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Bold.ttc",
    ],
    FONT_BOLD,
)


def make_splash(width, height):
    img = Image.new("RGB", (width, height), INK)
    draw = ImageDraw.Draw(img, "RGBA")

    # subtle rings on the right
    for radius, alpha in ((height * 0.72, 26), (height * 0.50, 20), (height * 0.30, 16)):
        cx, cy = width - height * 0.55, height * 0.52
        draw.ellipse(
            (cx - radius, cy - radius, cx + radius, cy + radius),
            outline=(88, 184, 146, alpha),
            width=2,
        )

    mark_size = int(height * 0.56)
    mark = material_icon.render(mark_size, CREAM)
    mx, my = int(width * 0.045), int((height - mark_size) / 2) - int(height * 0.02)
    img.paste(mark, (mx, my), mark)

    text_x = mx + mark_size + int(width * 0.035)
    bold = ImageFont.truetype(FONT_BOLD, int(height * 0.155))
    regular = ImageFont.truetype(FONT_REG, int(height * 0.058))
    cjk = ImageFont.truetype(FONT_CJK, int(height * 0.075))

    draw.text((text_x, int(height * 0.28)), "CHENYI OFFICE", font=bold, fill=CREAM)
    draw.text((text_x, int(height * 0.50)), "胡辰逸定制版", font=cjk, fill=ACCENT)
    draw.text(
        (text_x, int(height * 0.64)),
        "Windows 10 / 11  ·  64-bit",
        font=regular,
        fill=MUTED,
    )
    draw.rounded_rectangle(
        (
            text_x,
            int(height * 0.80),
            text_x + int(width * 0.20),
            int(height * 0.80) + max(3, int(height * 0.012)),
        ),
        radius=2,
        fill=ACCENT,
    )
    return img


def svg_mark(size, x, y, color):
    # Geometry is the upstream Material Design Icons path, unchanged.
    scale = size / 24.0
    return (
        f'<g transform="translate({x},{y}) scale({scale:.6f})">'
        f'<path fill="{color}" d="{material_icon.path_data()}"/></g>'
    )


def make_logo_svg(width, height, viewbox_w, viewbox_h, mark_size, text_scale=1.0):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {viewbox_w} {viewbox_h}" preserveAspectRatio="xMidYMid">
  <style>
    .word {{ font-family: "Segoe UI Semibold", "Microsoft YaHei", Arial, sans-serif; font-weight: 600; fill: #10201a; }}
  </style>
  {svg_mark(mark_size, 0, (viewbox_h - mark_size) / 2, "#10201a")}
  <text x="{mark_size + 34}" y="{viewbox_h * 0.58}" font-size="{58 * text_scale}" class="word">CHENYI OFFICE</text>
</svg>
'''


def make_about_svg():
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="372.72406mm" height="354.72815mm" viewBox="0 0 1000 950">
  <style>
    .title {{ font-family: "Segoe UI Semibold", "Microsoft YaHei", Arial, sans-serif; font-weight: 600; fill: #10201a; }}
    .sub {{ font-family: "Microsoft YaHei", "Segoe UI", Arial, sans-serif; fill: #1f5c46; }}
    .fine {{ font-family: "Segoe UI", "Microsoft YaHei", Arial, sans-serif; fill: #5f5f52; }}
  </style>
  {svg_mark(280, 360, 120, "#1f5c46")}
  <text x="500" y="500" text-anchor="middle" font-size="76" class="title">CHENYI OFFICE</text>
  <text x="500" y="560" text-anchor="middle" font-size="34" class="sub">胡辰逸定制版</text>
  <rect x="410" y="610" width="180" height="6" rx="3" fill="#58b892"/>
  <text x="500" y="660" text-anchor="middle" font-size="22" class="fine">技术内核：LibreOffice（MPL 2.0）</text>
  <text x="500" y="700" text-anchor="middle" font-size="22" class="fine">Built on LibreOffice technology</text>
  <text x="500" y="738" text-anchor="middle" font-size="20" class="fine">Windows 10 / 11  ·  64-bit</text>
</svg>
'''


def main():
    make_splash(644, 199).save(OUT / "intro.png")
    make_splash(990, 254).save(OUT / "intro-highres.png")
    (OUT / "logo.svg").write_text(
        make_logo_svg("345.9", "66.38", 920, 177, 177, 1.0)
    )
    (OUT / "logo-sc.svg").write_text(
        make_logo_svg("348", "72", 920, 190, 190, 1.0)
    )
    (OUT / "about.svg").write_text(make_about_svg())
    print("engine assets written")


if __name__ == "__main__":
    main()

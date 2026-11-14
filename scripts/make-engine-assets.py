#!/usr/bin/env python3
"""Generate engine-level UI assets: splash screens and SVG brand marks."""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "app" / "branding" / "engine"
OUT.mkdir(parents=True, exist_ok=True)

INK = (16, 32, 26)
GREEN = (31, 92, 70)
GREEN_LIGHT = (46, 125, 91)
ACCENT = (88, 184, 146)
CREAM = (244, 241, 232)
MUTED = (156, 170, 160)

FONT_BOLD = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_REG = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_CJK = "/usr/share/fonts/opentype/noto-cjk-otf/NotoSansCJKsc-Regular.otf"
FONT_CJK_BOLD = "/usr/share/fonts/opentype/noto-cjk-otf/NotoSansCJKsc-Bold.otf"


def gradient(size, top, bottom):
    img = Image.new("RGB", size, top)
    draw = ImageDraw.Draw(img)
    for y in range(size[1]):
        t = y / max(1, size[1] - 1)
        draw.line(
            [(0, y), (size[0], y)],
            fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(3)),
        )
    return img


def draw_mark(img, x, y, size):
    panel = gradient((size, size), GREEN_LIGHT, GREEN).convert("RGBA")
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        (0, 0, size - 1, size - 1), radius=int(size * 0.22), fill=255
    )
    img.paste(panel, (x, y), mask)
    draw = ImageDraw.Draw(img)
    stroke = max(6, int(size * 0.13))
    box = (x + int(size * 0.24), y + int(size * 0.22), x + int(size * 0.76), y + int(size * 0.74))
    draw.arc(box, start=45, end=315, fill=CREAM, width=stroke)
    draw.rounded_rectangle(
        (x + int(size * 0.30), y + int(size * 0.79), x + int(size * 0.70), y + int(size * 0.84)),
        radius=int(size * 0.02),
        fill=ACCENT,
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

    mark = int(height * 0.56)
    mx, my = int(width * 0.045), int((height - mark) / 2) - int(height * 0.02)
    draw_mark(img, mx, my, mark)

    text_x = mx + mark + int(width * 0.035)
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


def svg_mark(size=150, x=0, y=0):
    return f'''<g transform="translate({x},{y})">
  <defs>
    <linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2e7d5b"/>
      <stop offset="1" stop-color="#1f5c46"/>
    </linearGradient>
  </defs>
  <rect width="{size}" height="{size}" rx="{int(size*0.22)}" fill="url(#g)"/>
  <path d="M {int(size*0.72)} {int(size*0.29)} A {int(size*0.26)} {int(size*0.26)} 0 1 0 {int(size*0.72)} {int(size*0.71)}"
        fill="none" stroke="#f4f1e8" stroke-width="{int(size*0.13)}" stroke-linecap="round"/>
  <rect x="{int(size*0.30)}" y="{int(size*0.79)}" width="{int(size*0.40)}" height="{int(size*0.05)}"
        rx="{int(size*0.02)}" fill="#58b892"/>
</g>'''


def make_logo_svg(width, height, viewbox_w, viewbox_h, mark_size, text_scale=1.0):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {viewbox_w} {viewbox_h}" preserveAspectRatio="xMidYMid">
  <style>
    .word {{ font-family: "Segoe UI Semibold", "Microsoft YaHei", Arial, sans-serif; font-weight: 600; fill: #10201a; }}
    .sub {{ font-family: "Microsoft YaHei", "Segoe UI", Arial, sans-serif; fill: #1f5c46; }}
  </style>
  {svg_mark(mark_size, 0, (viewbox_h - mark_size) / 2)}
  <text x="{mark_size + 34}" y="{viewbox_h * 0.52}" font-size="{58 * text_scale}" class="word">CHENYI OFFICE</text>
  <text x="{mark_size + 36}" y="{viewbox_h * 0.75}" font-size="{24 * text_scale}" class="sub">胡辰逸定制版</text>
  <rect x="{mark_size + 36}" y="{viewbox_h * 0.82}" width="{viewbox_w * 0.30}" height="5" rx="2.5" fill="#58b892"/>
</svg>
'''


def make_about_svg():
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="372.72406mm" height="354.72815mm" viewBox="0 0 1000 950">
  <style>
    .title {{ font-family: "Segoe UI Semibold", "Microsoft YaHei", Arial, sans-serif; font-weight: 600; fill: #10201a; }}
    .sub {{ font-family: "Microsoft YaHei", "Segoe UI", Arial, sans-serif; fill: #1f5c46; }}
    .fine {{ font-family: "Segoe UI", "Microsoft YaHei", Arial, sans-serif; fill: #5f5f52; }}
  </style>
  {svg_mark(280, 360, 120)}
  <text x="500" y="500" text-anchor="middle" font-size="76" class="title">CHENYI OFFICE</text>
  <text x="500" y="560" text-anchor="middle" font-size="34" class="sub">胡辰逸定制版</text>
  <rect x="410" y="610" width="180" height="6" rx="3" fill="#58b892"/>
  <text x="500" y="690" text-anchor="middle" font-size="24" class="fine">Built on LibreOffice technology</text>
  <text x="500" y="730" text-anchor="middle" font-size="20" class="fine">Windows 10 / 11  ·  64-bit</text>
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

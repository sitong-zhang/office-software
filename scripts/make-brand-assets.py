#!/usr/bin/env python3
"""Generate the Chenyi Office logo, icon, and installer welcome background."""

from PIL import Image, ImageDraw
from pathlib import Path

import material_icon

ROOT = Path(__file__).resolve().parent.parent
BRAND = ROOT / "app" / "branding"
INSTALLER = ROOT / "installer"
BRAND.mkdir(parents=True, exist_ok=True)

INK = (16, 32, 26)
GREEN = (31, 92, 70)
ACCENT = (88, 184, 146)
CREAM = (244, 241, 232)
PAPER = (247, 245, 239)


def make_logo(size=1024):
    # Geometry comes from Material Design Icons; only the fill color changes.
    return material_icon.render(size, GREEN)


def make_background(width=1800, height=1200):
    img = Image.new("RGB", (width, height), PAPER)
    draw = ImageDraw.Draw(img, "RGBA")

    # fine grid
    step = 48
    for x in range(0, width, step):
        draw.line([(x, 0), (x, height)], fill=(23, 23, 15, 10), width=1)
    for y in range(0, height, step):
        draw.line([(0, y), (width, y)], fill=(23, 23, 15, 10), width=1)

    # right brand panel
    panel_x = int(width * 0.69)
    draw.rectangle([panel_x, 0, width, height], fill=INK)
    draw.rectangle([panel_x, 0, panel_x + 4, height], fill=ACCENT)

    # concentric rings
    for radius, alpha in ((360, 34), (270, 26), (180, 20)):
        draw.ellipse(
            [
                width - 430 - radius,
                420 - radius,
                width - 430 + radius,
                420 + radius,
            ],
            outline=(88, 184, 146, alpha),
            width=3,
        )

    # oversized watermark: the Material Design Icons document glyph, not a drawn logo
    watermark_size = 760
    watermark = material_icon.render(watermark_size, CREAM, opacity=0.06)
    watermark_layer = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    watermark_layer.alpha_composite(
        watermark,
        (
            int(panel_x + (width - panel_x - watermark_size) / 2),
            int(560 - watermark_size / 2),
        ),
    )
    img = Image.alpha_composite(img.convert("RGBA"), watermark_layer).convert("RGB")
    draw = ImageDraw.Draw(img, "RGBA")

    # document cards, drawn rotated and placed on the dark panel
    def card(size, bg, angle, lines, center, grid=False):
        w, h = size
        cw, ch = int(w * 2.4), int(h * 2.4)
        c = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        d = ImageDraw.Draw(c)
        ox, oy = int(w * 0.7), int(h * 0.7)
        d.rounded_rectangle((ox, oy, ox + w, oy + h), radius=18, fill=bg)
        if grid:
            for i in range(1, 7):
                d.line(
                    [(ox + 22, oy + 34 * i), (ox + w - 22, oy + 34 * i)],
                    fill=(31, 92, 70, 45),
                    width=2,
                )
            for i in range(1, 5):
                d.line(
                    [(ox + 22 + i * (w - 44) / 5, oy + 26), (ox + 22 + i * (w - 44) / 5, oy + h - 22)],
                    fill=(31, 92, 70, 45),
                    width=2,
                )
        else:
            d.rounded_rectangle(
                (ox + 24, oy + 28, ox + w - 70, oy + 44), radius=6, fill=(23, 23, 15, 90)
            )
            for i in range(lines):
                y = oy + 76 + i * 34
                d.rounded_rectangle(
                    (ox + 24, y, ox + w - (34 if i % 3 else 70), y + 10),
                    radius=5,
                    fill=(95, 95, 82, 70),
                )
            d.rounded_rectangle(
                (ox + 24, oy + h - 54, ox + 120, oy + h - 34),
                radius=5,
                fill=(31, 92, 70, 110),
            )
        c = c.rotate(angle, resample=Image.BICUBIC, expand=False)
        img.paste(
            c,
            (int(center[0] - cw / 2), int(center[1] - ch / 2)),
            c,
        )

    card((280, 350), (244, 241, 232, 255), -8, 6, (1420, 720))
    card((240, 280), (227, 235, 228, 255), 7, 5, (1570, 790), grid=True)
    card((190, 230), (244, 241, 232, 240), 16, 4, (1710, 660))

    # clean footer area behind the installer action button
    draw.rectangle([panel_x, height - 190, width, height], fill=INK)
    draw.rectangle([panel_x, height - 190, panel_x + 4, height], fill=ACCENT)

    # bottom-left accent
    draw.rounded_rectangle(
        (60, height - 100, 300, height - 94), radius=3, fill=ACCENT
    )

    # vignette on the paper side
    overlay = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    od = ImageDraw.Draw(overlay)
    for i in range(120):
        alpha = int(12 * (1 - i / 120))
        od.rectangle(
            [0, height - i * 2, panel_x, height - i * 2 + 2],
            fill=(23, 23, 15, alpha),
        )
    img = Image.alpha_composite(img.convert("RGBA"), overlay).convert("RGB")
    return img


def main():
    logo = make_logo()
    logo.resize((512, 512), Image.LANCZOS).save(BRAND / "logo.png")
    material_icon.render(512, CREAM).save(BRAND / "logo-cream.png")
    (ROOT / "docs" / "assets").mkdir(parents=True, exist_ok=True)
    logo.resize((256, 256), Image.LANCZOS).save(ROOT / "docs" / "assets" / "logo.png")
    for target in (
        ROOT / "app" / "launcher" / "icon.ico",
        ROOT / "installer" / "icon.ico",
        ROOT / "uninstaller" / "icon.ico",
    ):
        logo.save(
            target,
            sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)],
        )

    background = make_background()
    background.resize((900, 600), Image.LANCZOS).save(INSTALLER / "welcome.png")
    background.resize((900, 600), Image.LANCZOS).convert("RGB").save(
        INSTALLER / "welcome.bmp"
    )

    print("assets written")


if __name__ == "__main__":
    main()

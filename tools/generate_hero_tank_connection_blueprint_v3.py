"""Render V3 with a straight, wall-backed side gallery beside the hero tank."""

from pathlib import Path
from PIL import Image, ImageDraw

from generate_hero_tank_connection_blueprint_v2 import (
    BG, FLOOR, GREEN, MUTED, RED, ROUTE, ROUTE_EDGE, TEXT, UPPER,
    UPPER_EDGE, WALL, WATER, WATER_EDGE, YELLOW,
    F12, F14, F16, F20, F28, arrow, centered, panel,
    draw_middle_floor, draw_second_floor, draw_section,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "design" / "hero_tank_connection_v3_straight_gallery.png"
W, H = 1900, 1450


def draw_first_floor(draw, bounds):
    panel(draw, bounds, "1F  ±0.0 m")
    x0, y0, x1, y1 = bounds
    cx = (x0 + x1) // 2

    # Reception and the flush six-metre gate.
    draw.rectangle((x0 + 145, y1 - 170, x1 - 145, y1 - 40),
                   fill="#26343d", outline=WALL, width=5)
    centered(draw, (cx, y1 - 108), "受付フロア", F16)
    centered(draw, (cx, y1 - 75), "17 × 8.5 m", F12, MUTED)
    draw.rectangle((cx - 70, y1 - 190, cx + 70, y1 - 165),
                   fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (cx, y1 - 201), "幅6 m／床段差なし", F12, ROUTE_EDGE)

    # Hero hall and front pane.
    hall = (x0 + 115, y0 + 435, x1 - 115, y1 - 190)
    draw.rounded_rectangle(hall, radius=16, fill=FLOOR, outline=WALL, width=5)
    centered(draw, (cx, y0 + 620), "大水槽ホール", F16)
    centered(draw, (cx, y0 + 655), "約23 × 13 m", F12, MUTED)
    tank = (x0 + 145, y0 + 195, x1 - 145, y0 + 455)
    draw.rounded_rectangle(tank, radius=20, fill=WATER, outline=WATER_EDGE, width=6)
    centered(draw, (cx, y0 + 270), "大水槽", F20)
    centered(draw, (cx, y0 + 310), "正面17 m × 高さ8 m", F12)
    centered(draw, (cx, y0 + 350), "左右岩礁／中央魚群", F12, MUTED)
    centered(draw, (cx, y0 + 400), "観覧距離8 m", F12, GREEN)
    draw.line([(cx, y0 + 455), (cx, y0 + 545)], fill=GREEN, width=4)

    # Benches and central event area.
    for bx in (x0 + 150, x1 - 230):
        for by in (y0 + 520, y0 + 585):
            draw.rounded_rectangle((bx, by, bx + 80, by + 24), radius=6,
                                   fill=UPPER, outline=UPPER_EDGE, width=2)
    centered(draw, (cx, y0 + 545), "イベント空地", F12, MUTED)

    # Straight side gallery: outside wall on the left, observation glass on
    # the right. It shares the full tank depth and never bends around itself.
    gallery_left = x0 + 45
    gallery_right = x0 + 140
    gallery_top = y0 + 195
    gallery_bottom = y0 + 535
    draw.rectangle((gallery_left, gallery_top, gallery_right, gallery_bottom),
                   fill=ROUTE, outline=ROUTE_EDGE, width=4)
    draw.line([(gallery_left, gallery_top), (gallery_left, gallery_bottom)],
              fill=WALL, width=14)
    draw.line([(gallery_right, gallery_top), (gallery_right, gallery_bottom)],
              fill=WATER_EDGE, width=8)
    centered(draw, ((gallery_left + gallery_right) // 2, y0 + 375),
             "直線ガラス回廊", F12)
    draw.text((gallery_left - 7, y0 + 490), "左：壁", font=F12, fill=MUTED)
    draw.text((gallery_right + 8, y0 + 415), "右：大水槽ガラス", font=F12, fill=WATER_EDGE)
    arrow(draw, [((gallery_left + gallery_right) // 2, gallery_bottom - 25),
                 ((gallery_left + gallery_right) // 2, gallery_top + 25)], width=5)

    # The dark vestibule starts exactly where the tank side ends. There is no
    # decorative connector or exposed water between these spaces.
    draw.rounded_rectangle((x0 + 30, y0 + 110, x0 + 155, y0 + 195),
                           radius=12, fill="#111923", outline=WALL, width=5)
    centered(draw, (x0 + 92, y0 + 142), "遮光ルーム", F12)
    centered(draw, (x0 + 92, y0 + 173), "水槽端で直結", F12, MUTED)
    draw.rounded_rectangle((x0 + 18, y0 + 58, x0 + 166, y0 + 102),
                           radius=10, fill=WATER, outline=WATER_EDGE, width=4)
    centered(draw, (x0 + 92, y0 + 80), "水中アーチ入口", F12)
    arrow(draw, [(x0 + 92, y0 + 122), (x0 + 92, y0 + 102)], width=5)

    # Right-side ramp entrance remains unchanged.
    draw.rounded_rectangle((x1 - 112, y0 + 480, x1 - 25, y0 + 655),
                           radius=12, fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (x1 - 68, y0 + 528), "スロープ", F12)
    centered(draw, (x1 - 68, y0 + 558), "入口", F12)
    arrow(draw, [(x1 - 70, y0 + 625), (x1 - 70, y0 + 580)], width=5)

    arrow(draw, [(cx, y1 - 70), (cx, y1 - 170), (cx, y0 + 655)], width=6)
    draw.text((x0 + 24, y1 - 28),
              "この階：受付・大水槽正面・直線側面回廊・遮光ルーム・水中アーチ入口",
              font=F12, fill=YELLOW)


def main():
    image = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(image)
    draw.text((55, 35), "水族館 接続設計 V3 — STRAIGHT SIDE GALLERY", font=F28, fill=TEXT)
    draw.text((58, 96),
              "1F：大水槽の左側を直進（左＝壁／右＝観察ガラス）→ 水槽端で遮光ルーム → 水中アーチ",
              font=F14, fill=MUTED)
    draw_first_floor(draw, (55, 145, 635, 1005))
    draw_middle_floor(draw, (660, 145, 1240, 1005))
    draw_second_floor(draw, (1265, 145, 1845, 1005))
    draw_section(draw)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT, optimize=True)
    print(OUTPUT)


if __name__ == "__main__":
    main()

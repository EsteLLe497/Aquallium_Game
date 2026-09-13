"""Render the hero-tank plan as three unambiguous floor layers."""

from pathlib import Path
from PIL import Image, ImageDraw

from generate_hero_tank_connection_blueprint import (
    BG, FLOOR, GREEN, GRID, MUTED, PANEL, RED, ROUTE, ROUTE_EDGE, TEXT,
    UPPER, UPPER_EDGE, WALL, WATER, WATER_EDGE, YELLOW,
    F12, F14, F16, F20, F28, arrow, centered, panel,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "design" / "hero_tank_connection_v2_three_layers.png"
W, H = 1900, 1450


def small_label(draw, xy, text, fill=MUTED):
    centered(draw, xy, text, F12, fill)


def draw_common_outline(draw, bounds, opacity_color=GRID):
    x0, y0, x1, y1 = bounds
    draw.rectangle(bounds, outline=opacity_color, width=3)
    draw.line([(x0 + 20, y0 + 110), (x1 - 20, y0 + 110)], fill=opacity_color, width=2)
    draw.line([(x0 + 20, y1 - 110), (x1 - 20, y1 - 110)], fill=opacity_color, width=2)


def draw_first_floor(draw, bounds):
    panel(draw, bounds, "1F  ±0.0 m")
    x0, y0, x1, y1 = bounds
    cx = (x0 + x1) // 2

    # Reception and flush gate.
    reception = (x0 + 145, y1 - 170, x1 - 145, y1 - 40)
    draw.rectangle(reception, fill="#26343d", outline=WALL, width=5)
    centered(draw, (cx, y1 - 105), "受付フロア", F16)
    centered(draw, (cx, y1 - 72), "17 × 8.5 m", F12, MUTED)
    draw.rectangle((cx - 70, y1 - 190, cx + 70, y1 - 165),
                   fill=ROUTE, outline=ROUTE_EDGE, width=4)
    small_label(draw, (cx, y1 - 200), "幅6 m／床段差なし", ROUTE_EDGE)

    # Dry hall and tank.
    hall = (x0 + 115, y0 + 310, x1 - 115, y1 - 190)
    draw.rounded_rectangle(hall, radius=16, fill=FLOOR, outline=WALL, width=5)
    centered(draw, (cx, y0 + 525), "大水槽ホール", F16)
    centered(draw, (cx, y0 + 558), "約23 × 13 m", F12, MUTED)
    tank = (x0 + 155, y0 + 90, x1 - 155, y0 + 330)
    draw.rounded_rectangle(tank, radius=22, fill=WATER, outline=WATER_EDGE, width=6)
    centered(draw, (cx, y0 + 165), "大水槽", F20)
    centered(draw, (cx, y0 + 205), "正面17 m × 高さ8 m", F12)
    centered(draw, (cx, y0 + 245), "左右岩礁／中央魚群", F12, MUTED)
    centered(draw, (cx, y0 + 292), "観覧距離8 m", F12, GREEN)
    draw.line([(cx, y0 + 330), (cx, y0 + 430)], fill=GREEN, width=4)

    # Benches and central event area.
    for bx in (x0 + 150, x1 - 230):
        for by in (y0 + 405, y0 + 480):
            draw.rounded_rectangle((bx, by, bx + 80, by + 25), radius=6,
                                   fill=UPPER, outline=UPPER_EDGE, width=2)
    centered(draw, (cx, y0 + 455), "イベント空地", F12, MUTED)

    # Side gallery to underwater arch, located entirely on 1F.
    draw.line([(x0 + 115, y0 + 500), (x0 + 65, y0 + 450),
               (x0 + 60, y0 + 300), (x0 + 125, y0 + 225)],
              fill=ROUTE_EDGE, width=54, joint="curve")
    draw.line([(x0 + 115, y0 + 500), (x0 + 65, y0 + 450),
               (x0 + 60, y0 + 300), (x0 + 125, y0 + 225)],
              fill=ROUTE, width=42, joint="curve")
    small_label(draw, (x0 + 88, y0 + 380), "側面ガラス通路", TEXT)
    draw.rounded_rectangle((x0 + 24, y0 + 95, x0 + 140, y0 + 190),
                           radius=16, fill="#121d29", outline=WALL, width=4)
    centered(draw, (x0 + 82, y0 + 130), "遮光前室", F12)
    centered(draw, (x0 + 82, y0 + 165), "水中アーチへ", F12, WATER_EDGE)
    arrow(draw, [(x0 + 120, y0 + 220), (x0 + 95, y0 + 190)], width=5)

    # Only the ramp entrance is shown on this level.
    draw.rounded_rectangle((x1 - 112, y0 + 390, x1 - 25, y0 + 565),
                           radius=12, fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (x1 - 68, y0 + 438), "スロープ", F12)
    centered(draw, (x1 - 68, y0 + 468), "入口", F12)
    arrow(draw, [(x1 - 70, y0 + 535), (x1 - 70, y0 + 490)], width=5)

    arrow(draw, [(cx, y1 - 70), (cx, y1 - 170), (cx, y0 + 560)], width=6)
    draw.text((x0 + 24, y1 - 28), "この階：受付・大水槽正面・側面通路・水中アーチ入口",
              font=F12, fill=YELLOW)


def draw_middle_floor(draw, bounds):
    panel(draw, bounds, "中間展示層  +2.75 m")
    x0, y0, x1, y1 = bounds
    cx = (x0 + x1) // 2

    centered(draw, (cx, y0 + 75), "ホール全面には床を作らない", F12, MUTED)

    # Ghosted 1F hall/tank footprint for orientation.
    draw.rounded_rectangle((x0 + 55, y0 + 155, x1 - 55, y1 - 150),
                           radius=14, outline=GRID, width=3)
    draw.rounded_rectangle((x0 + 115, y0 + 105, x1 - 115, y0 + 270),
                           radius=18, outline=GRID, width=3)
    centered(draw, (cx, y0 + 190), "大水槽（下階）", F14, GRID)
    centered(draw, (cx, y0 + 430), "1Fホール吹抜け", F16, GRID)

    # Actual intermediate floor sits only in the right ramp wing.
    wing = (x1 - 205, y0 + 145, x1 - 25, y1 - 95)
    draw.rounded_rectangle(wing, radius=14, fill=ROUTE, outline=ROUTE_EDGE, width=6)
    centered(draw, (x1 - 115, y0 + 205), "中間展示層", F14)
    centered(draw, (x1 - 115, y0 + 240), "右側だけ床あり", F12, MUTED)

    # Incoming and outgoing ramp flights.
    arrow(draw, [(x1 - 170, y1 - 140), (x1 - 60, y1 - 220)], width=6)
    small_label(draw, (x1 - 115, y1 - 120), "1Fから上る", YELLOW)
    draw.rounded_rectangle((x1 - 185, y0 + 330, x1 - 45, y0 + 390),
                           radius=10, fill=UPPER, outline=UPPER_EDGE, width=4)
    centered(draw, (x1 - 115, y0 + 360), "水平踊場", F12)
    arrow(draw, [(x1 - 60, y0 + 300), (x1 - 170, y0 + 225)], width=6)
    small_label(draw, (x1 - 115, y0 + 285), "2Fへ上る", YELLOW)

    # One real exhibit at the half level.
    draw.rounded_rectangle((x1 - 190, y0 + 420, x1 - 40, y0 + 515),
                           radius=12, fill=WATER, outline=WATER_EDGE, width=5)
    centered(draw, (x1 - 115, y0 + 455), "中間小水槽", F14)
    centered(draw, (x1 - 115, y0 + 487), "休憩＋視線変化", F12, MUTED)
    draw.line([(x1 - 205, y0 + 465), (cx + 20, y0 + 300)], fill=GREEN, width=4)
    small_label(draw, (cx + 5, y0 + 335), "大水槽を途中から覗く", GREEN)

    # Void warning and vertical flow.
    draw.ellipse((cx - 55, y0 + 450, cx + 55, y0 + 560), outline=RED, width=4)
    centered(draw, (cx, y0 + 492), "吹抜け", F14, RED)
    centered(draw, (cx, y0 + 527), "歩行不可", F12, RED)
    draw.text((x0 + 24, y1 - 28), "この階：折返し踊場・中間小水槽のみ（独立した全面フロアではない）",
              font=F12, fill=YELLOW)


def draw_second_floor(draw, bounds):
    panel(draw, bounds, "2F  +5.5 m")
    x0, y0, x1, y1 = bounds
    cx = (x0 + x1) // 2

    # H-shaped deck.
    draw.rectangle((x0 + 95, y0 + 155, x0 + 160, y1 - 165),
                   fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x1 - 160, y0 + 155, x1 - 95, y1 - 165),
                   fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x0 + 95, y0 + 350, x1 - 95, y0 + 420),
                   fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x0 + 95, y0 + 105, x1 - 95, y0 + 175),
                   fill=UPPER, outline=UPPER_EDGE, width=5)
    centered(draw, (cx, y0 + 140), "見下ろしバルコニー", F14)
    centered(draw, (cx, y0 + 386), "H字通路 幅3 m", F14)

    # Central void and sight direction.
    draw.rectangle((x0 + 160, y0 + 175, x1 - 160, y0 + 350),
                   fill="#091521", outline=GRID, width=2)
    centered(draw, (cx, y0 + 255), "1Fホール吹抜け", F14, MUTED)
    draw.line([(cx, y0 + 175), (cx, y0 + 285)], fill=GREEN, width=4)
    centered(draw, (cx, y0 + 310), "斜め下へ水槽内を見る", F12, GREEN)

    # Future room portals.
    rooms = [
        ((x0 + 25, y0 + 65, x0 + 175, y0 + 140), "屋外テラス"),
        ((x1 - 175, y0 + 65, x1 - 25, y0 + 140), "イルカ展示"),
        ((x0 + 25, y1 - 145, x0 + 175, y1 - 55), "管理室"),
        ((x1 - 175, y1 - 145, x1 - 25, y1 - 55), "将来展示"),
    ]
    for room, title in rooms:
        draw.rounded_rectangle(room, radius=10, fill=FLOOR, outline=WALL, width=4)
        centered(draw, ((room[0] + room[2]) / 2, (room[1] + room[3]) / 2), title, F12)

    draw.rounded_rectangle((x1 - 185, y1 - 255, x1 - 35, y1 - 195),
                           radius=10, fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (x1 - 110, y1 - 225), "中間層から到着", F12)
    arrow(draw, [(x1 - 110, y1 - 195), (x1 - 125, y1 - 165)], width=5)
    centered(draw, (cx, y0 + 690), "水面は見せない／水槽内部だけを観覧", F12, WATER_EDGE)
    draw.text((x0 + 24, y1 - 28), "この階：H字通路・見下ろしバルコニー・将来4室の接続口",
              font=F12, fill=YELLOW)


def draw_section(draw):
    bounds = (55, 1040, 1845, 1400)
    panel(draw, bounds, "高さ関係 / 1F → 中間展示層 → 2F")
    x0, y0, x1, y1 = bounds
    floor1 = y1 - 55
    middle = floor1 - 105
    floor2 = floor1 - 210
    ceiling = floor1 - 260

    draw.line([(x0 + 45, floor1), (x1 - 45, floor1)], fill=TEXT, width=4)
    draw.line([(x0 + 550, middle), (x0 + 930, middle)], fill=ROUTE_EDGE, width=7)
    draw.line([(x0 + 1080, floor2), (x1 - 160, floor2)], fill=UPPER_EDGE, width=7)
    draw.line([(x0 + 45, ceiling), (x1 - 45, ceiling)], fill=GRID, width=3)

    draw.text((x0 + 45, floor1 + 10), "1F ±0.0 m", font=F12, fill=TEXT)
    draw.text((x0 + 560, middle - 30), "中間展示層 +2.75 m", font=F12, fill=ROUTE_EDGE)
    draw.text((x0 + 1090, floor2 - 30), "2F +5.5 m", font=F12, fill=UPPER_EDGE)
    draw.text((x0 + 45, ceiling + 8), "天井 約9.5 m", font=F12, fill=MUTED)

    # Three ramp runs with clear landings.
    arrow(draw, [(x0 + 360, floor1), (x0 + 650, middle)], width=7)
    arrow(draw, [(x0 + 930, middle), (x0 + 720, floor2)], width=7)
    arrow(draw, [(x0 + 720, floor2), (x0 + 1120, floor2)], width=7)
    centered(draw, (x0 + 820, floor1 - 30), "最大勾配10%・踊場では完全に水平", F12, YELLOW)

    # Tank section and hidden water surface.
    draw.rectangle((x1 - 585, floor2 - 45, x1 - 300, floor1),
                   fill=WATER, outline=WATER_EDGE, width=5)
    draw.rectangle((x1 - 605, floor2 - 72, x1 - 280, floor2 - 43),
                   fill="#172735", outline=WALL, width=3)
    centered(draw, (x1 - 442, floor2 - 58), "不透明上部（水面を隠す）", F12)
    centered(draw, (x1 - 442, floor1 - 70), "大水槽", F16)
    draw.line([(x1 - 640, floor2 + 10), (x1 - 470, floor2 + 80)], fill=GREEN, width=4)
    small_label(draw, (x1 - 630, floor2 + 75), "2Fから見下ろす", GREEN)


def main():
    image = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(image)
    draw.text((55, 35), "水族館 接続設計 V2 — 3 LAYERS", font=F28, fill=TEXT)
    draw.text((58, 96), "それぞれ別の高さとして読む：1F / 中間展示層 / 2F",
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

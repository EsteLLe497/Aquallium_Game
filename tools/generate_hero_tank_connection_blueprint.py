"""Render the approved reception-to-hero-tank blockout as a static blueprint."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "design" / "hero_tank_connection_v1.png"

W, H = 1800, 1250
BG = "#07111d"
PANEL = "#0b1a28"
GRID = "#193449"
WALL = "#45697d"
FLOOR = "#102d3f"
WATER = "#07547a"
WATER_EDGE = "#27c8f2"
ROUTE = "#0a4650"
ROUTE_EDGE = "#38d6c5"
UPPER = "#292657"
UPPER_EDGE = "#8b7cf6"
TEXT = "#e9f5ff"
MUTED = "#92b3c5"
YELLOW = "#ffc84a"
RED = "#f45b80"
GREEN = "#69dfae"


def font(size: int, bold: bool = False):
    candidates = [
        Path("C:/Windows/Fonts/YuGothB.ttc" if bold else "C:/Windows/Fonts/YuGothR.ttc"),
        Path("C:/Windows/Fonts/meiryob.ttc" if bold else "C:/Windows/Fonts/meiryo.ttc"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


F12 = font(20)
F14 = font(24)
F16 = font(28, True)
F20 = font(36, True)
F28 = font(48, True)


def centered(draw, xy, text, use_font=F14, fill=TEXT):
    box = draw.textbbox((0, 0), text, font=use_font)
    draw.text((xy[0] - (box[2] - box[0]) / 2,
               xy[1] - (box[3] - box[1]) / 2), text, font=use_font, fill=fill)


def arrow(draw, points, fill=YELLOW, width=8):
    draw.line(points, fill=fill, width=width, joint="curve")
    x0, y0 = points[-2]
    x1, y1 = points[-1]
    dx, dy = x1 - x0, y1 - y0
    length = max((dx * dx + dy * dy) ** 0.5, 0.001)
    ux, uy = dx / length, dy / length
    px, py = -uy, ux
    size = 18
    draw.polygon([
        (x1, y1),
        (x1 - ux * size + px * size * 0.55,
         y1 - uy * size + py * size * 0.55),
        (x1 - ux * size - px * size * 0.55,
         y1 - uy * size - py * size * 0.55),
    ], fill=fill)


def dimension(draw, p0, p1, label, vertical=False):
    draw.line([p0, p1], fill=YELLOW, width=3)
    if vertical:
        draw.line([(p0[0] - 10, p0[1]), (p0[0] + 10, p0[1])], fill=YELLOW, width=3)
        draw.line([(p1[0] - 10, p1[1]), (p1[0] + 10, p1[1])], fill=YELLOW, width=3)
        centered(draw, ((p0[0] + p1[0]) / 2 - 24, (p0[1] + p1[1]) / 2), label, F12, YELLOW)
    else:
        draw.line([(p0[0], p0[1] - 10), (p0[0], p0[1] + 10)], fill=YELLOW, width=3)
        draw.line([(p1[0], p1[1] - 10), (p1[0], p1[1] + 10)], fill=YELLOW, width=3)
        centered(draw, ((p0[0] + p1[0]) / 2, (p0[1] + p1[1]) / 2 - 18), label, F12, YELLOW)


def panel(draw, bounds, title):
    draw.rounded_rectangle(bounds, radius=18, fill=PANEL, outline=WALL, width=4)
    draw.text((bounds[0] + 22, bounds[1] + 16), title, font=F16, fill=TEXT)


def draw_floor_one(draw):
    bounds = (55, 160, 1160, 880)
    panel(draw, bounds, "1F / 受付から大水槽・水中アーチへ")

    # Reference grid.
    for x in range(95, 1130, 50):
        draw.line([(x, 215), (x, 845)], fill=GRID, width=1)
    for y in range(215, 846, 50):
        draw.line([(95, y), (1130, y)], fill=GRID, width=1)

    # Hero tank and dry hall.
    hall = (315, 410, 875, 670)
    draw.rounded_rectangle(hall, radius=16, fill=FLOOR, outline=WALL, width=6)
    centered(draw, (595, 625), "大水槽ホール  約23 × 13 m", F16)
    tank = (380, 245, 810, 430)
    draw.rounded_rectangle(tank, radius=24, fill=WATER, outline=WATER_EDGE, width=7)
    centered(draw, (595, 305), "大水槽", F20)
    centered(draw, (595, 350), "正面17 m × 高さ8 m / 奥行8 m", F12, TEXT)
    centered(draw, (595, 395), "左右：高低差のある岩礁　中央：魚群の主役空間", F12, MUTED)

    # Benches and central event space.
    for x in (345, 825):
        for y in (500, 575):
            draw.rounded_rectangle((x - 45, y - 14, x + 45, y + 14), radius=7,
                                   fill=UPPER, outline=UPPER_EDGE, width=2)
    centered(draw, (595, 535), "中央イベント空地", F14, MUTED)

    # Reception, flush connection and sight line.
    reception = (390, 690, 800, 835)
    draw.rectangle(reception, fill="#26343d", outline=WALL, width=6)
    centered(draw, (595, 760), "受付フロア 17 × 8.5 m", F16)
    draw.rectangle((520, 672, 670, 704), fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (595, 687), "幅6 m・段差0", F12)
    arrow(draw, [(595, 810), (595, 705), (595, 650)], fill=YELLOW)
    draw.line([(595, 650), (595, 430)], fill=GREEN, width=4)
    centered(draw, (650, 475), "正面視距離8 m", F12, GREEN)

    # Left curved side gallery and opaque vestibule.
    draw.line([(315, 600), (245, 575), (205, 500), (215, 385), (310, 310), (378, 305)],
              fill=ROUTE_EDGE, width=58, joint="curve")
    draw.line([(315, 600), (245, 575), (205, 500), (215, 385), (310, 310), (378, 305)],
              fill=ROUTE, width=46, joint="curve")
    centered(draw, (208, 530), "側面ガラス通路", F12)
    centered(draw, (208, 555), "幅2.8 m", F12, MUTED)
    draw.rounded_rectangle((85, 235, 255, 330), radius=22, fill="#121d29", outline=WALL, width=5)
    centered(draw, (170, 270), "遮光前室", F14)
    centered(draw, (170, 302), "→ 水中アーチ", F12, WATER_EDGE)
    arrow(draw, [(360, 320), (280, 280), (250, 280)], fill=YELLOW)

    # Right switchback ramp wing.
    ramp = (880, 385, 1115, 690)
    draw.rectangle(ramp, fill=ROUTE, outline=ROUTE_EDGE, width=6)
    centered(draw, (998, 420), "壁内折返しスロープ", F14)
    centered(draw, (998, 452), "幅3 m / 3走路 / +5.5 m", F12, MUTED)
    arrow(draw, [(915, 510), (1075, 510)])
    arrow(draw, [(1075, 570), (915, 570)])
    arrow(draw, [(915, 635), (1075, 635)])
    draw.rounded_rectangle((930, 532, 1060, 550), radius=6, fill=WATER, outline=WATER_EDGE, width=2)
    centered(draw, (995, 542), "中間展示窓", F12)

    dimension(draw, (380, 220), (810, 220), "17 m")
    dimension(draw, (835, 245), (835, 430), "8 m", vertical=True)
    draw.text((92, 850), "主動線", font=F12, fill=YELLOW)
    draw.text((190, 850), "水槽・水", font=F12, fill=WATER_EDGE)
    draw.text((310, 850), "任意探索", font=F12, fill=ROUTE_EDGE)


def draw_floor_two(draw):
    bounds = (1190, 160, 1745, 880)
    panel(draw, bounds, "2F / H字通路 +5.5 m")
    x0, y0 = 1240, 235
    draw.rectangle((x0 + 70, y0 + 95, x0 + 385, y0 + 470), fill="#091521", outline=GRID, width=2)
    centered(draw, (x0 + 228, y0 + 290), "1F吹抜け", F16, MUTED)

    # H walkway and viewing balcony.
    draw.rectangle((x0 + 70, y0 + 40, x0 + 385, y0 + 105), fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x0 + 70, y0 + 100, x0 + 135, y0 + 470), fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x0 + 320, y0 + 100, x0 + 385, y0 + 470), fill=UPPER, outline=UPPER_EDGE, width=5)
    draw.rectangle((x0 + 70, y0 + 260, x0 + 385, y0 + 325), fill=UPPER, outline=UPPER_EDGE, width=5)
    centered(draw, (x0 + 228, y0 + 72), "見下ろしバルコニー", F14)
    centered(draw, (x0 + 228, y0 + 292), "H字通路 幅3 m", F14)
    draw.line([(x0 + 228, y0 + 105), (x0 + 228, y0 + 185)], fill=GREEN, width=4)
    centered(draw, (x0 + 228, y0 + 205), "斜め下へ水槽を見る", F12, GREEN)

    # Four reserved destinations.
    rooms = [
        ((x0, y0, x0 + 120, y0 + 65), "屋外テラス"),
        ((x0 + 335, y0, x0 + 455, y0 + 65), "イルカ展示"),
        ((x0, y0 + 445, x0 + 120, y0 + 535), "管理室"),
        ((x0 + 335, y0 + 445, x0 + 455, y0 + 535), "将来展示"),
    ]
    for box, title in rooms:
        draw.rounded_rectangle(box, radius=10, fill=FLOOR, outline=WALL, width=4)
        centered(draw, ((box[0] + box[2]) / 2, (box[1] + box[3]) / 2), title, F12)
    draw.rounded_rectangle((x0 + 325, y0 + 375, x0 + 455, y0 + 430), radius=8,
                           fill=ROUTE, outline=ROUTE_EDGE, width=4)
    centered(draw, (x0 + 390, y0 + 403), "スロープ出口", F12)
    centered(draw, (x0 + 228, y0 + 585), "水面は見せず、上部ガラス越しに水槽内部を観覧", F12, WATER_EDGE)
    centered(draw, (x0 + 228, y0 + 620), "将来4室は接続口のみ。謎の壁や未定空間は作らない", F12, MUTED)


def draw_section(draw):
    bounds = (55, 910, 1745, 1205)
    panel(draw, bounds, "SECTION / 高低差・水槽上部・描画ゾーン")
    floor_y = 1158
    draw.line([(95, floor_y), (1190, floor_y)], fill=TEXT, width=4)
    draw.text((95, floor_y + 8), "±0.0 m", font=F12, fill=MUTED)

    # Reception and tank section.
    draw.rectangle((145, 1053, 375, floor_y), fill="#26343d", outline=WALL, width=4)
    centered(draw, (260, 1105), "受付", F14)
    draw.rectangle((520, 975, 820, floor_y), fill=WATER, outline=WATER_EDGE, width=5)
    centered(draw, (670, 1075), "大水槽", F16)
    draw.rectangle((500, 950, 840, 978), fill="#172735", outline=WALL, width=3)
    centered(draw, (670, 964), "不透明上部：水面を完全に隠す", F12, TEXT)
    draw.rectangle((420, 1045, 545, 1065), fill=UPPER, outline=UPPER_EDGE, width=3)
    centered(draw, (482, 1028), "2F +5.5 m", F12)
    draw.line([(485, 1050), (650, 1100)], fill=GREEN, width=4)
    centered(draw, (560, 1084), "見下ろし", F12, GREEN)

    # Ramp elevation.
    arrow(draw, [(885, 1157), (1035, 1115), (885, 1115), (1035, 1073),
                 (885, 1073), (1035, 1030)], fill=YELLOW, width=6)
    centered(draw, (960, 1008), "3走路・最大勾配10%", F12)

    # Zone legend and visibility rule.
    legend_x = 1220
    zones = [
        (WALL, "Reception / HeroHall"),
        (WATER_EDGE, "HeroTank（魚群・泡・体積光）"),
        (ROUTE_EDGE, "SideGallery / Ramp"),
        (UPPER_EDGE, "UpperDeck"),
    ]
    for index, (color, name) in enumerate(zones):
        y = 978 + index * 40
        draw.rectangle((legend_x, y, legend_x + 26, y + 18), fill=color)
        draw.text((legend_x + 40, y - 4), name, font=F12, fill=TEXT)
    draw.text((legend_x, 1143), "連続マップのまま、見えるゾーンだけ描画・更新", font=F12, fill=YELLOW)


def main():
    image = Image.new("RGB", (W, H), BG)
    draw = ImageDraw.Draw(image)
    draw.text((55, 40), "水族館 接続設計 V1 — RECEPTION × HERO TANK", font=F28, fill=TEXT)
    draw.text((58, 104), "受付 → 大水槽正面 → 左：側面ガラス通路・水中アーチ / 右：折返しスロープ・2F H字通路",
              font=F14, fill=MUTED)
    draw_floor_one(draw)
    draw_floor_two(draw)
    draw_section(draw)
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT, optimize=True)
    print(OUTPUT)


if __name__ == "__main__":
    main()

"""Generate the V5 continuous promenade plan and elevation section.

This script creates an approval image only. It does not modify either GLB.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from generate_aquarium_floorplan_v2 import (
    FONT_LEGEND,
    FONT_NUMBER,
    FONT_ROOM,
    FONT_SMALL,
    FONT_SUBTITLE,
    FONT_TITLE,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "concepts" / "aquarium-floorplan-v5.png"

WIDTH = 1800
HEIGHT = 1320
BG = (7, 13, 21)
FIELD = (11, 21, 32)
TEXT = (238, 247, 252)
MUTED = (151, 187, 204)
ROUTE = (248, 194, 66)
CYAN = (68, 204, 220)
REEF = (47, 205, 190)
OCEAN = (43, 165, 220)
ARCH = (27, 142, 196)
DEEP = (62, 84, 167)
FINAL = (199, 68, 126)


def centered(draw, xy, text, font=FONT_SMALL, fill=TEXT) -> None:
    draw.multiline_text(
        xy,
        text,
        font=font,
        fill=fill,
        anchor="mm",
        align="center",
        spacing=4,
    )


def arrow(draw, points, color=ROUTE, width=7) -> None:
    draw.line(points, fill=color, width=width, joint="curve")
    p0, p1 = points[-2], points[-1]
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    length = max((dx * dx + dy * dy) ** 0.5, 1.0)
    ux, uy = dx / length, dy / length
    px, py = -uy, ux
    back = (p1[0] - ux * 18, p1[1] - uy * 18)
    draw.polygon(
        [
            p1,
            (round(back[0] + px * 9), round(back[1] + py * 9)),
            (round(back[0] - px * 9), round(back[1] - py * 9)),
        ],
        fill=color,
    )


def zone(draw, box, number, title, subtitle, fill, outline) -> None:
    draw.rounded_rectangle(box, radius=18, fill=fill, outline=outline, width=4)
    x0, y0, x1, y1 = box
    draw.ellipse(
        (x0 + 16, y0 + 16, x0 + 58, y0 + 58),
        fill=BG,
        outline=outline,
        width=3,
    )
    draw.text(
        (x0 + 37, y0 + 37),
        str(number),
        font=FONT_NUMBER,
        fill=TEXT,
        anchor="mm",
    )
    centered(draw, ((x0 + x1) // 2, (y0 + y1) // 2 - 10), title, FONT_ROOM)
    centered(draw, ((x0 + x1) // 2, y1 - 24), subtitle, FONT_SMALL, MUTED)


def build() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)

    draw.text(
        (80, 42),
        "水族館 館内図 V5 — CONTINUOUS BATHYMETRIC PROMENADE",
        font=FONT_TITLE,
        fill=TEXT,
    )
    draw.text(
        (80, 94),
        "展示室を消化する構成から、海岸→大海原→深海へ連続して下る一本の順路へ",
        font=FONT_SUBTITLE,
        fill=(121, 177, 202),
    )

    # Plan view
    plan = (80, 145, 1720, 750)
    draw.rounded_rectangle(plan, radius=20, fill=FIELD, outline=(70, 97, 114), width=4)
    draw.text((105, 172), "PLAN / 平面順路", font=FONT_ROOM, fill=TEXT)

    zone(draw, (115, 520, 315, 700), 1, "エントランス\nENTRANCE", "標高 ±0.0 m", (52, 47, 41), (191, 154, 101))
    zone(draw, (340, 470, 590, 700), 2, "クラゲシアター\nJELLYFISH", "開始 / 唯一の短い往復", (22, 35, 52), (115, 213, 233))
    zone(draw, (620, 500, 930, 700), 3, "沿岸プロムナード\nCOASTAL", "連続展示壁 / -0.5 m", (20, 52, 61), CYAN)
    zone(draw, (960, 390, 1240, 700), 4, "サンゴ礁スロープ\nREEF SLOPE", "緩いカーブ / -1.5 m", (9, 65, 72), REEF)
    zone(draw, (1270, 250, 1480, 700), 5, "大海原ホール\nGRAND OCEAN", "右壁大水槽 / -2.0 m", (5, 55, 88), OCEAN)

    # Hero tank on the right wall of the long hall.
    draw.rounded_rectangle((1482, 250, 1670, 700), radius=18, fill=(4, 76, 111), outline=(80, 220, 244), width=5)
    centered(draw, (1576, 475), "RIGHT-SIDE\nHERO TANK", FONT_ROOM)

    # Arch begins immediately after the ocean hall and bends back across the plan.
    arch_points = [(1380, 235), (1250, 205), (1030, 210), (810, 255), (625, 330)]
    draw.line(arch_points, fill=(4, 62, 91), width=118, joint="curve")
    draw.line(arch_points, fill=ARCH, width=88, joint="curve")
    draw.line(arch_points, fill=(10, 30, 46), width=54, joint="curve")
    draw.line(arch_points, fill=(119, 230, 248), width=4, joint="curve")
    centered(draw, (1015, 215), "6  降下型水中アーチ 48 m\nDESCENDING UNDERWATER ARCH", FONT_ROOM)

    zone(draw, (300, 210, 580, 390), 7, "黄昏プロムナード\nTWILIGHT", "-4.7 → -5.2 m", (17, 25, 54), DEEP)
    zone(draw, (110, 210, 275, 420), 8, "深海\nパノラマ", "最深部 -5.2 m", (48, 25, 45), FINAL)

    # Route arrows only bridge the visual zones; the route itself has no doors.
    arrows = [
        [(300, 620), (350, 620)],
        [(580, 620), (630, 620)],
        [(920, 620), (970, 620)],
        [(1230, 560), (1280, 560)],
        [(1390, 250), (1390, 220)],
        [(620, 330), (570, 300)],
        [(300, 300), (265, 300)],
    ]
    for points in arrows:
        arrow(draw, points)

    draw.text(
        (1650, 720),
        "水槽の同じ水が壁面から頭上へ回り込み、そのままアーチへ",
        font=FONT_SMALL,
        fill=(142, 214, 230),
        anchor="rs",
    )

    # Elevation section
    section = (80, 790, 1720, 1240)
    draw.rounded_rectangle(section, radius=20, fill=FIELD, outline=(70, 97, 114), width=4)
    draw.text((105, 817), "SECTION / 順路断面", font=FONT_ROOM, fill=TEXT)

    chart_left = 145
    chart_right = 1650
    chart_top = 885
    chart_bottom = 1165
    for depth in range(0, 6):
        y = chart_top + depth * 46
        draw.line((chart_left, y, chart_right, y), fill=(38, 57, 70), width=2)
        draw.text((chart_left - 18, y), f"-{depth} m", font=FONT_SMALL, fill=MUTED, anchor="rm")

    distances = [0, 30, 54, 82, 106, 154, 178, 196]
    elevations = [0.0, 0.0, -0.5, -1.5, -2.0, -4.7, -5.2, -5.2]
    labels = ["入口", "クラゲ", "沿岸", "サンゴ", "大海原", "アーチ底", "黄昏", "深海"]
    colors = [(191, 154, 101), (115, 213, 233), CYAN, REEF, OCEAN, ARCH, DEEP, FINAL]

    def sx(distance):
        return round(chart_left + distance / distances[-1] * (chart_right - chart_left))

    def sy(elevation):
        return round(chart_top + (-elevation) * 46)

    profile = [(sx(distance), sy(elevation)) for distance, elevation in zip(distances, elevations)]
    draw.line(profile, fill=ROUTE, width=8, joint="curve")
    for index, ((x, y), label_text, color) in enumerate(zip(profile, labels, colors)):
        draw.ellipse((x - 8, y - 8, x + 8, y + 8), fill=color, outline=TEXT, width=2)
        anchor = "ms" if index % 2 == 0 else "mt"
        label_y = y - 18 if anchor == "ms" else y + 18
        draw.text((x, label_y), label_text, font=FONT_SMALL, fill=TEXT, anchor=anchor)

    # Show the lighting shift as a restrained band below the elevation profile.
    band_y0 = 1190
    segment_x = [sx(value) for value in distances]
    for index in range(len(segment_x) - 1):
        draw.rectangle(
            (segment_x[index], band_y0, segment_x[index + 1], band_y0 + 22),
            fill=colors[index],
        )
    draw.text(
        (chart_left, 1222),
        "明るい浅海                                        水面光が減衰                         暗い深海",
        font=FONT_LEGEND,
        fill=MUTED,
        anchor="ls",
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()

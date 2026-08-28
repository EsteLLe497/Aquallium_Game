"""Generate the V4 linear aquarium promenade concept plan.

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
OUTPUT = ROOT / "concepts" / "aquarium-floorplan-v4.png"

WIDTH = 1800
HEIGHT = 1240
MARGIN_X = 100
MARGIN_Y = 155
METERS_WIDE = 90.0
METERS_DEEP = 38.0
SCALE = min(
    (WIDTH - MARGIN_X * 2) / METERS_WIDE,
    (HEIGHT - MARGIN_Y - 120) / METERS_DEEP,
)


def point(x: float, y: float) -> tuple[int, int]:
    return round(MARGIN_X + x * SCALE), round(MARGIN_Y + y * SCALE)


def rect_pixels(rect: tuple[float, float, float, float]) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = rect
    return (*point(x0, y0), *point(x1, y1))


def centered(
    draw: ImageDraw.ImageDraw,
    position: tuple[int, int],
    text: str,
    font,
    fill: tuple[int, int, int],
) -> None:
    draw.multiline_text(
        position,
        text,
        font=font,
        fill=fill,
        anchor="mm",
        align="center",
        spacing=5,
    )


def room(
    draw: ImageDraw.ImageDraw,
    rect: tuple[float, float, float, float],
    number: str,
    title: str,
    subtitle: str,
    fill: tuple[int, int, int],
    outline: tuple[int, int, int],
) -> None:
    pixels = rect_pixels(rect)
    draw.rounded_rectangle(
        pixels,
        radius=15,
        fill=fill,
        outline=outline,
        width=4,
    )
    x0, y0, x1, y1 = pixels
    badge = (x0 + 29, y0 + 29)
    draw.ellipse(
        (badge[0] - 20, badge[1] - 20, badge[0] + 20, badge[1] + 20),
        fill=(10, 18, 28),
        outline=outline,
        width=3,
    )
    draw.text(badge, number, font=FONT_NUMBER, fill=(242, 249, 252), anchor="mm")
    centered(
        draw,
        ((x0 + x1) // 2, (y0 + y1) // 2 - 6),
        title,
        FONT_ROOM,
        (237, 247, 251),
    )
    draw.text(
        ((x0 + x1) // 2, y1 - 22),
        subtitle,
        font=FONT_SMALL,
        fill=(162, 195, 211),
        anchor="mm",
    )


def arrow(
    draw: ImageDraw.ImageDraw,
    start: tuple[float, float],
    end: tuple[float, float],
    color: tuple[int, int, int] = (248, 194, 66),
) -> None:
    p0 = point(*start)
    p1 = point(*end)
    draw.line((*p0, *p1), fill=color, width=7)
    dx = p1[0] - p0[0]
    dy = p1[1] - p0[1]
    length = max((dx * dx + dy * dy) ** 0.5, 1.0)
    ux, uy = dx / length, dy / length
    px, py = -uy, ux
    tip = p1
    back = (p1[0] - ux * 18, p1[1] - uy * 18)
    draw.polygon(
        [
            tip,
            (round(back[0] + px * 9), round(back[1] + py * 9)),
            (round(back[0] - px * 9), round(back[1] - py * 9)),
        ],
        fill=color,
    )


def build() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), (7, 13, 21))
    draw = ImageDraw.Draw(image)

    draw.text(
        (MARGIN_X, 44),
        "水族館 館内図 V4 — LINEAR DESCENT PROMENADE",
        font=FONT_TITLE,
        fill=(238, 247, 252),
    )
    draw.text(
        (MARGIN_X, 94),
        "ほぼ一本道：クラゲの静けさ → 浅海 → 大水槽 → 水中アーチ → 深海 / 展示を増やしてもギミックは増やさない",
        font=FONT_SUBTITLE,
        fill=(121, 177, 202),
    )

    draw.rounded_rectangle(
        rect_pixels((0, 0, 90, 38)),
        radius=18,
        fill=(11, 21, 32),
        outline=(72, 99, 116),
        width=4,
    )

    room(
        draw,
        (2, 27, 14, 36),
        "1",
        "エントランス\nENTRANCE",
        "12 m × 9 m",
        (52, 47, 41),
        (191, 154, 101),
    )
    room(
        draw,
        (14, 21, 32, 36),
        "2",
        "クラゲシアター\nJELLYFISH THEATER",
        "18 m × 15 m",
        (22, 35, 52),
        (115, 213, 233),
    )
    room(
        draw,
        (32, 25, 50, 36),
        "3",
        "沿岸グラデーション\nCOASTAL GRADIENT",
        "18 m × 11 m",
        (22, 51, 60),
        (64, 195, 207),
    )
    room(
        draw,
        (50, 21, 66, 36),
        "4",
        "陽光サンゴ礁\nSUNLIT REEF",
        "16 m × 15 m",
        (11, 65, 74),
        (59, 218, 207),
    )
    room(
        draw,
        (66, 15, 77.5, 36),
        "5",
        "大海原ホール\nGRAND OCEAN\nHALL",
        "11.5 m × 21 m",
        (5, 58, 91),
        (45, 174, 224),
    )
    room(
        draw,
        (59, 3, 76, 14),
        "6",
        "発光展示室\nLUMINOUS GALLERY",
        "17 m × 11 m",
        (34, 33, 68),
        (132, 116, 236),
    )
    room(
        draw,
        (18, 2, 36, 14),
        "8",
        "深海降下区画\nDEEP SEA DESCENT",
        "18 m × 12 m",
        (15, 22, 51),
        (74, 99, 203),
    )
    room(
        draw,
        (2, 2, 18, 17),
        "9",
        "深海パノラマ\nDEEP SEA PANORAMA",
        "16 m × 15 m",
        (48, 25, 45),
        (218, 78, 140),
    )

    # The arch is a continuous linear connector, not another hub branch.
    arch_start = point(58, 8.5)
    arch_end = point(36, 8.5)
    draw.line(
        (*arch_start, *arch_end),
        fill=(4, 64, 96),
        width=120,
    )
    draw.line(
        (*arch_start, *arch_end),
        fill=(26, 181, 221),
        width=92,
    )
    draw.line(
        (*arch_start, *arch_end),
        fill=(13, 35, 51),
        width=58,
    )
    draw.line(
        (*arch_start, *arch_end),
        fill=(117, 235, 250),
        width=4,
    )
    centered(
        draw,
        point(47.0, 8.5),
        "7  水中アーチ  約34 m\nUNDERWATER ARCH / CHASE",
        FONT_ROOM,
        (226, 250, 254),
    )

    # The Grand Ocean Hall is a narrow vertical approach. Its entire right wall
    # is the hero tank, so the water dominates the player's view while walking.
    tank = rect_pixels((77.5, 15, 88, 36))
    draw.rounded_rectangle(
        tank,
        radius=20,
        fill=(4, 79, 116),
        outline=(79, 220, 245),
        width=5,
    )
    centered(
        draw,
        point(82.75, 25.5),
        "右壁全面\n大水槽\n\nRIGHT-SIDE\nHERO TANK",
        FONT_ROOM,
        (221, 249, 254),
    )

    # Short doorway connectors keep the route readable without drawing across
    # room labels. Only the start-to-entrance check is repeated.
    route = [
        ((16.5, 32.5), (12.0, 32.5)),
        ((12.0, 30.0), (16.5, 30.0)),
        ((29.5, 32.0), (34.5, 32.0)),
        ((47.5, 32.0), (52.5, 32.0)),
        ((63.5, 32.0), (68.0, 32.0)),
        ((72.0, 18.0), (72.0, 12.0)),
        ((61.5, 8.5), (57.0, 8.5)),
        ((37.5, 8.5), (34.0, 11.2)),
        ((20.0, 11.2), (16.0, 11.2)),
    ]
    for start, end in route:
        arrow(draw, start, end)

    start = point(23, 31)
    draw.ellipse(
        (start[0] - 11, start[1] - 11, start[0] + 11, start[1] + 11),
        fill=(255, 244, 166),
        outline=(255, 255, 255),
        width=3,
    )
    draw.text(
        (start[0], start[1] - 22),
        "START",
        font=FONT_SMALL,
        fill=(255, 235, 143),
        anchor="ms",
    )

    exit_point = point(2, 9)
    draw.polygon(
        [
            (exit_point[0] - 23, exit_point[1]),
            (exit_point[0] + 2, exit_point[1] - 15),
            (exit_point[0] + 2, exit_point[1] + 15),
        ],
        fill=(247, 112, 126),
    )
    draw.text(
        (exit_point[0] + 18, exit_point[1] + 38),
        "PHYSICAL EXIT",
        font=FONT_SMALL,
        fill=(247, 112, 126),
        anchor="lm",
    )

    draw.text(
        (WIDTH - MARGIN_X, 97),
        "概念上の外形 約90 m × 38 m",
        font=FONT_SMALL,
        fill=(145, 177, 193),
        anchor="rm",
    )

    legend_y = HEIGHT - 67
    legend = [
        ("観察のみ", (115, 213, 233)),
        ("パズル", (132, 116, 236)),
        ("チェイス", (26, 181, 221)),
        ("終幕", (218, 78, 140)),
    ]
    cursor_x = MARGIN_X
    for label, color in legend:
        draw.rounded_rectangle(
            (cursor_x, legend_y - 12, cursor_x + 25, legend_y + 13),
            radius=5,
            fill=color,
        )
        cursor_x += 34
        draw.text(
            (cursor_x, legend_y),
            label,
            font=FONT_LEGEND,
            fill=(181, 205, 216),
            anchor="lm",
        )
        cursor_x += round(draw.textlength(label, font=FONT_LEGEND)) + 34

    draw.text(
        (WIDTH - MARGIN_X, legend_y),
        "3 PUZZLES / 1 CHASE / NO PARALLEL-WORLD VIEW",
        font=FONT_LEGEND,
        fill=(159, 190, 205),
        anchor="rm",
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()

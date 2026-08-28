"""Generate the proposed public-only aquarium floor plan.

This script intentionally does not overwrite aquarium_greybox.glb.  It is the
layout approval artifact for the next greybox revision.
"""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "concepts" / "aquarium-floorplan-v2.png"

WIDTH = 1800
HEIGHT = 1240
MARGIN_X = 105
MARGIN_Y = 145
METERS_WIDE = 54.0
METERS_DEEP = 40.0
SCALE = min(
    (WIDTH - MARGIN_X * 2) / METERS_WIDE,
    (HEIGHT - MARGIN_Y - 110) / METERS_DEEP,
)


def load_font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    candidates = [
        Path("C:/Windows/Fonts/meiryob.ttc" if bold else "C:/Windows/Fonts/meiryo.ttc"),
        Path("C:/Windows/Fonts/YuGothB.ttc" if bold else "C:/Windows/Fonts/YuGothR.ttc"),
        Path("C:/Windows/Fonts/arialbd.ttf" if bold else "C:/Windows/Fonts/arial.ttf"),
    ]
    for candidate in candidates:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size)
    return ImageFont.load_default()


FONT_TITLE = load_font(38, True)
FONT_SUBTITLE = load_font(20)
FONT_ROOM = load_font(21, True)
FONT_SMALL = load_font(16)
FONT_NUMBER = load_font(27, True)
FONT_LEGEND = load_font(18)


def point(x: float, y: float) -> tuple[int, int]:
    """Map floor-plan meters, with (0, 0) at the north-west corner."""
    return (
        round(MARGIN_X + x * SCALE),
        round(MARGIN_Y + y * SCALE),
    )


def rect_pixels(rect: tuple[float, float, float, float]) -> tuple[int, int, int, int]:
    x0, y0, x1, y1 = rect
    return (*point(x0, y0), *point(x1, y1))


def centered_multiline(
    draw: ImageDraw.ImageDraw,
    center: tuple[int, int],
    text: str,
    font: ImageFont.FreeTypeFont,
    fill: tuple[int, int, int],
    spacing: int = 5,
) -> None:
    draw.multiline_text(
        center,
        text,
        font=font,
        fill=fill,
        anchor="mm",
        align="center",
        spacing=spacing,
    )


def draw_room(
    draw: ImageDraw.ImageDraw,
    rect: tuple[float, float, float, float],
    number: str,
    title: str,
    dimensions: str,
    fill: tuple[int, int, int],
    outline: tuple[int, int, int],
) -> None:
    pixels = rect_pixels(rect)
    draw.rounded_rectangle(
        pixels,
        radius=14,
        fill=fill,
        outline=outline,
        width=4,
    )
    x0, y0, x1, y1 = pixels
    badge_center = (x0 + 31, y0 + 31)
    draw.ellipse(
        (
            badge_center[0] - 21,
            badge_center[1] - 21,
            badge_center[0] + 21,
            badge_center[1] + 21,
        ),
        fill=(14, 23, 34),
        outline=outline,
        width=3,
    )
    draw.text(
        badge_center,
        number,
        font=FONT_NUMBER,
        fill=(244, 249, 252),
        anchor="mm",
    )
    centered_multiline(
        draw,
        ((x0 + x1) // 2, (y0 + y1) // 2 - 8),
        title,
        FONT_ROOM,
        (240, 247, 250),
    )
    draw.text(
        ((x0 + x1) // 2, y1 - 24),
        dimensions,
        font=FONT_SMALL,
        fill=(166, 197, 212),
        anchor="mm",
    )


def draw_door(
    draw: ImageDraw.ImageDraw,
    center: tuple[float, float],
    horizontal: bool,
) -> None:
    cx, cy = point(*center)
    half = round(1.15 * SCALE)
    if horizontal:
        draw.line((cx - half, cy, cx + half, cy), fill=(245, 203, 88), width=9)
    else:
        draw.line((cx, cy - half, cx, cy + half), fill=(245, 203, 88), width=9)


def build() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), (8, 14, 22))
    draw = ImageDraw.Draw(image)

    draw.text(
        (MARGIN_X, 48),
        "水族館 館内図 V2 — PUBLIC EXHIBITION PLAN",
        font=FONT_TITLE,
        fill=(238, 247, 252),
    )
    draw.text(
        (MARGIN_X, 97),
        "管理室・裏通路を廃止 / 展示室を明確化 / 長い水中アーチを主役にした一方向ルート",
        font=FONT_SUBTITLE,
        fill=(121, 177, 202),
    )

    # Whole public building envelope: 54 m x 40 m.
    draw.rounded_rectangle(
        rect_pixels((0, 0, 54, 40)),
        radius=18,
        fill=(13, 23, 34),
        outline=(72, 99, 116),
        width=4,
    )

    draw_room(
        draw,
        (20, 31, 34, 40),
        "1",
        "エントランス\nLOBBY",
        "14 m × 9 m",
        (47, 43, 39),
        (171, 139, 95),
    )
    draw_room(
        draw,
        (2, 27, 20, 40),
        "2",
        "沿岸・浅海 展示室\nCOASTAL GALLERY",
        "18 m × 13 m",
        (24, 43, 56),
        (62, 158, 198),
    )
    draw_room(
        draw,
        (34, 27, 52, 40),
        "4",
        "発光・小型水槽 展示室\nLIGHT GALLERY",
        "18 m × 13 m",
        (32, 35, 65),
        (106, 115, 222),
    )
    draw_room(
        draw,
        (3, 14, 51, 27),
        "3",
        "大水槽ホール\nMAIN TANK HALL",
        "48 m × 13 m",
        (13, 42, 58),
        (46, 180, 222),
    )

    # Main tank occupies the north wall of the hall, with a clear visitor apron.
    tank_rect = rect_pixels((8, 11, 46, 16))
    draw.rounded_rectangle(
        tank_rect,
        radius=18,
        fill=(5, 81, 118),
        outline=(57, 218, 247),
        width=5,
    )
    centered_multiline(
        draw,
        point(27, 13.4),
        "パノラマ大水槽  38 m\nPANORAMIC MAIN TANK",
        FONT_ROOM,
        (213, 249, 255),
    )

    draw_room(
        draw,
        (21, 1, 37, 10),
        "6",
        "深海 展示室\nDEEP SEA GALLERY",
        "16 m × 9 m",
        (23, 28, 59),
        (91, 116, 214),
    )
    draw_room(
        draw,
        (2, 1, 19, 12),
        "7",
        "ドーム展示・終盤イベント\nFINAL DOME",
        "17 m × 11 m",
        (43, 27, 48),
        (200, 84, 146),
    )

    # Long arched tunnel. A thick water envelope surrounds the walkable tube.
    tunnel_points_m = [
        (48.5, 21.0),
        (52.0, 18.5),
        (52.0, 7.0),
        (48.5, 3.2),
        (39.5, 3.2),
        (37.0, 5.4),
    ]
    tunnel_points = [point(*position) for position in tunnel_points_m]
    draw.line(
        tunnel_points,
        fill=(5, 70, 102),
        width=round(5.7 * SCALE),
        joint="curve",
    )
    draw.line(
        tunnel_points,
        fill=(30, 187, 222),
        width=round(4.4 * SCALE),
        joint="curve",
    )
    draw.line(
        tunnel_points,
        fill=(16, 38, 53),
        width=round(2.8 * SCALE),
        joint="curve",
    )
    draw.line(
        tunnel_points,
        fill=(101, 229, 248),
        width=4,
        joint="curve",
    )
    centered_multiline(
        draw,
        point(45.8, 8.2),
        "5  水中アーチ通路\nUNDERWATER ARCH\n約 34 m",
        FONT_ROOM,
        (226, 251, 255),
    )

    # Route is intentionally linear but rooms remain wide enough for puzzles.
    route_m = [
        (27, 40),
        (27, 35),
        (18, 34),
        (11, 32),
        (12, 26),
        (27, 21),
        (43, 22),
        (44, 33),
        (49, 32),
        (49.5, 24),
        (51, 18),
        (51, 7),
        (47, 4.2),
        (38, 4.2),
        (29, 5.5),
        (20, 5.5),
        (11, 6),
        (2, 6),
    ]
    route = [point(*position) for position in route_m]
    draw.line(route, fill=(246, 194, 68), width=7, joint="curve")
    for route_point in route[::2]:
        draw.ellipse(
            (
                route_point[0] - 5,
                route_point[1] - 5,
                route_point[0] + 5,
                route_point[1] + 5,
            ),
            fill=(255, 231, 143),
        )

    # Deliberate choke points for locks, scene streaming, and puzzle gating.
    for center, horizontal in [
        ((20, 35), False),
        ((34, 35), False),
        ((11, 27), True),
        ((43, 27), True),
        ((49.5, 27), True),
        ((51, 21), False),
        ((37, 5.5), False),
        ((20, 5.5), False),
    ]:
        draw_door(draw, center, horizontal)

    # Entry and escape markers.
    entry = point(27, 40)
    draw.polygon(
        [
            (entry[0], entry[1] + 22),
            (entry[0] - 15, entry[1] - 2),
            (entry[0] + 15, entry[1] - 2),
        ],
        fill=(117, 230, 164),
    )
    draw.text(
        (entry[0], entry[1] + 48),
        "START",
        font=FONT_SMALL,
        fill=(117, 230, 164),
        anchor="mm",
    )
    exit_point = point(2, 6)
    draw.polygon(
        [
            (exit_point[0] - 22, exit_point[1]),
            (exit_point[0] + 2, exit_point[1] - 15),
            (exit_point[0] + 2, exit_point[1] + 15),
        ],
        fill=(247, 112, 126),
    )
    draw.text(
        (exit_point[0] + 20, exit_point[1] - 35),
        "ESCAPE",
        font=FONT_SMALL,
        fill=(247, 112, 126),
        anchor="lm",
    )

    # Scale and legend.
    legend_y = HEIGHT - 74
    draw.line(
        (*point(0, 40), *point(10, 40)),
        fill=(220, 232, 238),
        width=4,
    )
    draw.line(
        (point(0, 40)[0], point(0, 40)[1] - 9, point(0, 40)[0], point(0, 40)[1] + 9),
        fill=(220, 232, 238),
        width=3,
    )
    draw.line(
        (point(10, 40)[0], point(10, 40)[1] - 9, point(10, 40)[0], point(10, 40)[1] + 9),
        fill=(220, 232, 238),
        width=3,
    )
    draw.text(
        (point(5, 40)[0], legend_y),
        "10 m",
        font=FONT_LEGEND,
        fill=(220, 232, 238),
        anchor="mm",
    )
    draw.text(
        (WIDTH - MARGIN_X, legend_y),
        "黄線：想定プレイルート　｜　黄い太線：ゲート候補　｜　全体：約 54 m × 40 m",
        font=FONT_LEGEND,
        fill=(151, 181, 196),
        anchor="rm",
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()

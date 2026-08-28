"""Generate the real-aquarium-informed V3 floor plan.

V3 keeps one hero tank as the spatial core, reframes it from three visitor
zones, and makes the lighting progression readable in the plan itself.
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
    HEIGHT,
    MARGIN_X,
    WIDTH,
    centered_multiline,
    draw_door,
    draw_room,
    point,
    rect_pixels,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "concepts" / "aquarium-floorplan-v3.png"


def route_marker(
    draw: ImageDraw.ImageDraw,
    route_point: tuple[int, int],
) -> None:
    draw.ellipse(
        (
            route_point[0] - 5,
            route_point[1] - 5,
            route_point[0] + 5,
            route_point[1] + 5,
        ),
        fill=(255, 231, 143),
    )


def draw_dashed_route(
    draw: ImageDraw.ImageDraw,
    points: list[tuple[int, int]],
    color: tuple[int, int, int],
    width: int = 5,
    dash: float = 16.0,
    gap: float = 10.0,
) -> None:
    for start, end in zip(points, points[1:]):
        dx = end[0] - start[0]
        dy = end[1] - start[1]
        length = max((dx * dx + dy * dy) ** 0.5, 0.001)
        cursor = 0.0
        while cursor < length:
            segment_end = min(cursor + dash, length)
            x0 = start[0] + dx * cursor / length
            y0 = start[1] + dy * cursor / length
            x1 = start[0] + dx * segment_end / length
            y1 = start[1] + dy * segment_end / length
            draw.line((x0, y0, x1, y1), fill=color, width=width)
            cursor += dash + gap


def build() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), (7, 13, 21))
    draw = ImageDraw.Draw(image)

    draw.text(
        (MARGIN_X, 45),
        "水族館 館内図 V3 — HERO TANK LOOP",
        font=FONT_TITLE,
        fill=(238, 247, 252),
    )
    draw.text(
        (MARGIN_X, 95),
        "実在館の構成を反映：ひとつの大水槽を3方向から再提示 / 浅海から深海へ暗くなる光の物語",
        font=FONT_SUBTITLE,
        fill=(121, 177, 202),
    )

    draw.rounded_rectangle(
        rect_pixels((0, 0, 54, 40)),
        radius=18,
        fill=(12, 22, 33),
        outline=(72, 99, 116),
        width=4,
    )

    # Public rooms remain rectangular and independently authorable.
    draw_room(
        draw,
        (21, 32, 33, 40),
        "1",
        "エントランス\nLOBBY",
        "12 m × 8 m",
        (52, 47, 41),
        (191, 154, 101),
    )
    draw_room(
        draw,
        (2, 30, 20, 40),
        "2",
        "沿岸・浅海 展示室\nCOASTAL GALLERY",
        "18 m × 10 m",
        (22, 51, 60),
        (64, 195, 207),
    )
    draw_room(
        draw,
        (3, 20, 51, 30),
        "3",
        "大水槽ホール\nMAIN TANK HALL",
        "48 m × 10 m",
        (12, 38, 56),
        (45, 174, 224),
    )
    draw_room(
        draw,
        (34, 30, 52, 40),
        "4",
        "発光・小型水槽 展示室\nLIGHT GALLERY",
        "18 m × 10 m",
        (34, 33, 68),
        (132, 116, 236),
    )
    draw_room(
        draw,
        (21, 1, 37, 9),
        "6",
        "深海 展示室\nDEEP SEA GALLERY",
        "16 m × 8 m",
        (15, 22, 51),
        (74, 99, 203),
    )
    draw_room(
        draw,
        (2, 1, 19, 14),
        "7",
        "ドーム展示・終盤イベント\nFINAL DOME",
        "17 m × 13 m",
        (48, 25, 45),
        (218, 78, 140),
    )

    # One expensive hero tank is reused by three different viewing treatments.
    tank_pixels = rect_pixels((11, 9, 43, 20))
    draw.rounded_rectangle(
        tank_pixels,
        radius=22,
        fill=(4, 73, 109),
        outline=(53, 216, 246),
        width=5,
    )
    centered_multiline(
        draw,
        point(27, 14.2),
        "共通パノラマ大水槽\nSHARED HERO TANK  32 m × 11 m",
        FONT_ROOM,
        (219, 250, 255),
    )

    # View A: wide south-facing window in the main hall.
    view_a0 = point(15, 20)
    view_a1 = point(39, 20)
    draw.line((*view_a0, *view_a1), fill=(196, 247, 255), width=11)
    draw.text(
        point(27, 19.3),
        "VIEW A  パノラマ",
        font=FONT_SMALL,
        fill=(220, 251, 255),
        anchor="ms",
    )

    # View B: small portholes from the darker deep-sea gallery.
    for x in (24.0, 27.0, 30.0, 33.0):
        center = point(x, 9)
        draw.ellipse(
            (
                center[0] - 10,
                center[1] - 10,
                center[0] + 10,
                center[1] + 10,
            ),
            fill=(120, 220, 241),
            outline=(225, 253, 255),
            width=3,
        )
    draw.text(
        point(28.5, 9.75),
        "VIEW C  深海のぞき窓",
        font=FONT_SMALL,
        fill=(191, 229, 244),
        anchor="ms",
    )

    # Long arch tunnel wraps the east side and receives a side view of the tank.
    tunnel_m = [
        (49.0, 27.0),
        (52.0, 23.0),
        (52.0, 8.0),
        (48.0, 3.5),
        (40.0, 3.5),
        (37.0, 5.0),
    ]
    tunnel = [point(*position) for position in tunnel_m]
    draw.line(
        tunnel,
        fill=(4, 64, 96),
        width=126,
        joint="curve",
    )
    draw.line(
        tunnel,
        fill=(26, 181, 221),
        width=98,
        joint="curve",
    )
    draw.line(
        tunnel,
        fill=(13, 35, 51),
        width=62,
        joint="curve",
    )
    draw.line(
        tunnel,
        fill=(117, 235, 250),
        width=4,
        joint="curve",
    )

    draw.text(
        point(46.2, 7.1),
        "5  水中アーチ 約34 m\nUNDERWATER ARCH",
        font=FONT_ROOM,
        fill=(225, 251, 255),
        anchor="mm",
        align="center",
    )

    # View B is a short angled bay from the tunnel into the same hero tank.
    side_window0 = point(43, 13)
    side_window1 = point(43, 18)
    draw.line((*side_window0, *side_window1), fill=(200, 248, 255), width=10)
    draw.line(
        (*point(43.3, 15.5), *point(49.2, 15.5)),
        fill=(104, 212, 236),
        width=3,
    )
    draw.text(
        point(47.8, 14.9),
        "VIEW B\n斜め窓",
        font=FONT_SMALL,
        fill=(201, 241, 249),
        anchor="mm",
        align="center",
    )

    # Three lighting beats inside the continuous arch.
    beat_data = [
        ((50.8, 23.0), (34, 74, 100), "A"),
        ((52.0, 14.8), (101, 231, 246), "B"),
        ((45.0, 3.5), (114, 97, 230), "C"),
    ]
    for (x, y), color, label in beat_data:
        center = point(x, y)
        draw.ellipse(
            (
                center[0] - 13,
                center[1] - 13,
                center[0] + 13,
                center[1] + 13,
            ),
            fill=color,
            outline=(230, 248, 252),
            width=2,
        )
        draw.text(
            center,
            label,
            font=FONT_NUMBER,
            fill=(240, 252, 255),
            anchor="mm",
        )
    draw.multiline_text(
        point(47.1, 10.1),
        "A  暗い入口\nB  天窓の光\nC  青紫の出口",
        font=FONT_SMALL,
        fill=(173, 211, 225),
        anchor="mm",
        align="left",
        spacing=5,
    )

    # Main route: the arch entrance is reached directly from the Main Tank Hall.
    route_m = [
        (27, 40),
        (27, 36),
        (20, 36),
        (18, 38),
        (5, 38),
        (5, 31),
        (11, 30),
        (11, 21.5),
        (39, 21.5),
        (46, 24.0),
        (49, 27),
        (51, 23),
        (51, 9),
        (47, 4.5),
        (39, 4.5),
        (36, 2.2),
        (22, 2.2),
        (19, 6),
        (17, 3.2),
        (3, 3.2),
        (2, 7),
    ]
    route = [point(*position) for position in route_m]
    draw.line(route, fill=(248, 194, 66), width=7, joint="curve")
    for route_point in route[::2]:
        route_marker(draw, route_point)

    # Required side-puzzle loop. It starts and ends in room 3, so the arch
    # still opens from the Main Tank Hall rather than from room 4.
    puzzle_route_m = [
        (40.5, 27.5),
        (43, 30),
        (43, 38),
        (49, 38),
        (49, 30),
        (47, 27.5),
    ]
    puzzle_route = [point(*position) for position in puzzle_route_m]
    draw_dashed_route(draw, puzzle_route, (183, 127, 246))
    draw.text(
        point(46, 36.7),
        "電源復旧パズル\n3へ戻る",
        font=FONT_SMALL,
        fill=(215, 179, 250),
        anchor="mm",
        align="center",
    )
    draw.text(
        point(45.6, 25.8),
        "3 → 5  MAIN ROUTE",
        font=FONT_SMALL,
        fill=(255, 222, 126),
        anchor="mm",
    )

    # Door portals double as puzzle locks and whole-room culling boundaries.
    for center, horizontal in [
        ((20, 36), False),
        ((11, 30), True),
        ((43, 30), True),
        ((49, 30), True),
        ((49, 27), False),
        ((37, 5), False),
        ((20, 6), False),
    ]:
        draw_door(draw, center, horizontal)

    # The story starts in Room 4 and initially sends the player to Room 1.
    # Keep this architectural map neutral; the chronological path lives in
    # EVENT_FLOW_V4.md.
    escape = point(2, 7)
    draw.polygon(
        [
            (escape[0] - 22, escape[1]),
            (escape[0] + 2, escape[1] - 15),
            (escape[0] + 2, escape[1] + 15),
        ],
        fill=(247, 112, 126),
    )
    draw.text(
        (escape[0] + 20, escape[1] - 35),
        "ESCAPE",
        font=FONT_SMALL,
        fill=(247, 112, 126),
        anchor="lm",
    )

    # Lighting progression legend.
    legend_y = HEIGHT - 68
    lighting_legend = [
        ("1 暖白", (191, 154, 101)),
        ("2 浅海シアン", (64, 195, 207)),
        ("3 大水槽ブルー", (45, 174, 224)),
        ("4 紫発光", (132, 116, 236)),
        ("5 水中アーチ", (26, 181, 221)),
        ("6 深海ネイビー", (74, 99, 203)),
        ("7 非常灯レッド", (218, 78, 140)),
    ]
    cursor_x = MARGIN_X
    for label, color in lighting_legend:
        draw.rounded_rectangle(
            (cursor_x, legend_y - 13, cursor_x + 25, legend_y + 12),
            radius=5,
            fill=color,
        )
        cursor_x += 34
        draw.text(
            (cursor_x, legend_y),
            label,
            font=FONT_LEGEND,
            fill=(179, 204, 216),
            anchor="lm",
        )
        cursor_x += round(draw.textlength(label, font=FONT_LEGEND)) + 28

    draw.text(
        (WIDTH - MARGIN_X, 97),
        "全体 約54 m × 40 m",
        font=FONT_SMALL,
        fill=(145, 177, 193),
        anchor="rm",
    )

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()

"""Generate a nine-panel placement sheet for the linear aquarium route."""

from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw

from generate_aquarium_floorplan_v2 import (
    FONT_LEGEND,
    FONT_ROOM,
    FONT_SMALL,
    FONT_SUBTITLE,
    FONT_TITLE,
)


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "concepts" / "aquarium-area-detail-v1.png"

WIDTH = 1800
HEIGHT = 1760
BG = (7, 13, 21)
PANEL = (12, 23, 35)
EDGE = (67, 94, 111)
ROUTE = (248, 194, 66)
WATER = (12, 100, 143)
LIGHT = (83, 219, 236)
PUZZLE = (137, 115, 237)
EVENT = (225, 82, 143)
TEXT = (238, 247, 252)
MUTED = (157, 192, 209)


def arrow(draw: ImageDraw.ImageDraw, points: list[tuple[int, int]]) -> None:
    draw.line(points, fill=ROUTE, width=6, joint="curve")
    p0, p1 = points[-2], points[-1]
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    length = max((dx * dx + dy * dy) ** 0.5, 1.0)
    ux, uy = dx / length, dy / length
    px, py = -uy, ux
    back = (p1[0] - ux * 17, p1[1] - uy * 17)
    draw.polygon(
        [
            p1,
            (round(back[0] + px * 8), round(back[1] + py * 8)),
            (round(back[0] - px * 8), round(back[1] - py * 8)),
        ],
        fill=ROUTE,
    )


def label(
    draw: ImageDraw.ImageDraw,
    xy: tuple[int, int],
    text: str,
    fill: tuple[int, int, int] = MUTED,
    anchor: str = "mm",
) -> None:
    draw.multiline_text(
        xy,
        text,
        font=FONT_SMALL,
        fill=fill,
        anchor=anchor,
        align="center",
        spacing=3,
    )


def panel(
    draw: ImageDraw.ImageDraw,
    col: int,
    row: int,
    number: str,
    title: str,
    subtitle: str,
) -> tuple[int, int, int, int]:
    x0 = 70 + col * 560
    y0 = 150 + row * 505
    x1 = x0 + 520
    y1 = y0 + 455
    draw.rounded_rectangle(
        (x0, y0, x1, y1),
        radius=18,
        fill=PANEL,
        outline=EDGE,
        width=3,
    )
    draw.ellipse(
        (x0 + 18, y0 + 18, x0 + 62, y0 + 62),
        fill=BG,
        outline=LIGHT,
        width=3,
    )
    draw.text(
        (x0 + 40, y0 + 40),
        number,
        font=FONT_ROOM,
        fill=TEXT,
        anchor="mm",
    )
    draw.text(
        (x0 + 78, y0 + 29),
        title,
        font=FONT_ROOM,
        fill=TEXT,
        anchor="la",
    )
    draw.text(
        (x0 + 78, y0 + 57),
        subtitle,
        font=FONT_SMALL,
        fill=MUTED,
        anchor="la",
    )
    plan = (x0 + 36, y0 + 92, x1 - 36, y1 - 42)
    draw.rounded_rectangle(
        plan,
        radius=13,
        fill=(9, 18, 29),
        outline=(78, 111, 129),
        width=3,
    )
    return plan


def build() -> None:
    image = Image.new("RGB", (WIDTH, HEIGHT), BG)
    draw = ImageDraw.Draw(image)
    draw.text(
        (70, 43),
        "エリア詳細配置 V1 — WALK / OBSERVE / LIGHT",
        font=FONT_TITLE,
        fill=TEXT,
    )
    draw.text(
        (70, 95),
        "歩行80%・観察15%・操作5% / 各エリアの主役演出は1つだけ",
        font=FONT_SUBTITLE,
        fill=(121, 177, 202),
    )

    # 1 Entrance
    p = panel(draw, 0, 0, "1", "エントランス", "出口を一度確認するだけ")
    x0, y0, x1, y1 = p
    draw.rectangle((x0 + 25, y0 + 35, x0 + 130, y0 + 82), fill=(70, 59, 45))
    label(draw, (x0 + 78, y0 + 58), "案内所", TEXT)
    draw.rectangle((x0 + 190, y1 - 35, x0 + 285, y1), fill=(23, 31, 39), outline=LIGHT, width=3)
    label(draw, (x0 + 237, y1 - 55), "自動ドア")
    draw.rectangle((x0 + 305, y1 - 58, x0 + 350, y1 - 18), fill=(88, 34, 43), outline=EVENT, width=2)
    label(draw, (x0 + 327, y1 - 78), "解錠盤", EVENT)
    label(draw, (x1 - 52, y0 + 45), "→ クラゲ")
    arrow(draw, [(x1 - 110, y0 + 120), (x1 - 38, y0 + 120)])

    # 2 Jellyfish
    p = panel(draw, 1, 0, "2", "クラゲシアター", "操作なし・光で東へ誘導")
    x0, y0, x1, y1 = p
    for offset in (55, 125, 195, 265):
        draw.ellipse((x0 + offset, y0 + 25, x0 + offset + 42, y0 + 67), fill=(18, 62, 83), outline=LIGHT, width=2)
    draw.ellipse((x1 - 160, y0 + 100, x1 - 45, y0 + 215), fill=WATER, outline=LIGHT, width=4)
    label(draw, (x1 - 102, y0 + 158), "5m\nメイン水槽", TEXT)
    draw.ellipse((x0 + 175, y0 + 125, x0 + 230, y0 + 205), fill=(18, 62, 83), outline=LIGHT, width=3)
    label(draw, (x0 + 202, y0 + 225), "円柱水槽")
    draw.rectangle((x0 + 50, y1 - 70, x0 + 150, y1 - 42), fill=(70, 61, 53))
    label(draw, (x0 + 100, y1 - 90), "開始ベンチ")
    arrow(draw, [(x0 + 35, y1 - 25), (x1 - 35, y1 - 25)])

    # 3 Coastal
    p = panel(draw, 2, 0, "3", "沿岸グラデーション", "3つ見て、出口で順番入力")
    x0, y0, x1, y1 = p
    names = ("河口\n波", "磯\n貝", "浅海\n魚")
    for index, name in enumerate(names):
        bx = x0 + 30 + index * 130
        draw.rounded_rectangle((bx, y0 + 25, bx + 105, y0 + 130), radius=11, fill=WATER, outline=LIGHT, width=3)
        label(draw, (bx + 52, y0 + 78), name, TEXT)
    for index in range(3):
        cx = x1 - 92 + index * 27
        draw.ellipse((cx, y1 - 70, cx + 20, y1 - 50), fill=PUZZLE)
    label(draw, (x1 - 55, y1 - 92), "入力", PUZZLE)
    arrow(draw, [(x0 + 30, y1 - 28), (x1 - 35, y1 - 28)])

    # 4 Reef
    p = panel(draw, 0, 1, "4", "陽光サンゴ礁", "S字に歩くだけ")
    x0, y0, x1, y1 = p
    draw.rounded_rectangle((x0 + 190, y0 + 35, x1 - 25, y1 - 40), radius=70, fill=WATER, outline=LIGHT, width=4)
    label(draw, (x1 - 125, (y0 + y1) // 2), "ラグーン型\n水槽", TEXT)
    for bx in (x0 + 45, x0 + 115):
        draw.rectangle((bx, y0 + 90, bx + 55, y0 + 115), fill=(74, 64, 53))
    arrow(draw, [(x0 + 25, y1 - 35), (x0 + 120, y1 - 35), (x0 + 150, y0 + 65), (x1 - 38, y0 + 65)])
    label(draw, (x0 + 85, y0 + 72), "ベンチ")

    # 5 Grand ocean
    p = panel(draw, 1, 1, "5", "大海原ホール", "縦長通路・右壁全面が大水槽")
    x0, y0, x1, y1 = p
    split = x0 + 230
    draw.rectangle((split, y0 + 18, x1 - 18, y1 - 18), fill=WATER, outline=LIGHT, width=4)
    label(draw, ((split + x1 - 18) // 2, (y0 + y1) // 2), "右壁全面\n大水槽", TEXT)
    draw.line((split - 12, y0 + 25, split - 12, y1 - 25), fill=(202, 242, 250), width=7)
    for by in (y0 + 65, y0 + 155, y0 + 245):
        draw.rectangle((x0 + 35, by, x0 + 105, by + 24), fill=(70, 61, 53))
    label(draw, (x0 + 70, y0 + 38), "左壁ベンチ")
    arrow(draw, [(x0 + 160, y1 - 28), (x0 + 160, y0 + 35)])

    # 6 Luminous
    p = panel(draw, 2, 1, "6", "発光展示室", "3色を順番に押す")
    x0, y0, x1, y1 = p
    pillar_colors = (LIGHT, (69, 215, 197), (65, 126, 225))
    for index, color in enumerate(pillar_colors):
        cx = x0 + 120 + index * 85
        draw.ellipse((cx - 26, y0 + 110, cx + 26, y0 + 162), fill=color, outline=TEXT, width=2)
        label(draw, (cx, y0 + 188), str(index + 1), color)
    draw.rectangle((x0 + 25, y0 + 35, x0 + 85, y0 + 85), fill=(44, 42, 75), outline=PUZZLE, width=3)
    label(draw, (x0 + 55, y0 + 105), "隔壁表示")
    arrow(draw, [(x1 - 35, y1 - 35), (x1 - 35, y0 + 55), (x0 + 35, y0 + 55)])

    # 7 Arch
    p = panel(draw, 0, 2, "7", "水中アーチ", "前半観察・後半チェイス")
    x0, y0, x1, y1 = p
    draw.rounded_rectangle((x0 + 20, y0 + 90, x1 - 20, y0 + 220), radius=55, fill=(7, 60, 86), outline=LIGHT, width=4)
    draw.rectangle((x0 + 95, y0 + 80, x0 + 145, y0 + 115), fill=(40, 45, 52), outline=ROUTE, width=2)
    draw.rectangle((x0 + 275, y0 + 195, x0 + 325, y0 + 230), fill=(40, 45, 52), outline=ROUTE, width=2)
    draw.line((x0 + 215, y0 + 100, x0 + 240, y0 + 200), fill=EVENT, width=7)
    label(draw, (x0 + 120, y0 + 60), "待避")
    label(draw, (x0 + 230, y0 + 65), "衝撃", EVENT)
    label(draw, (x0 + 300, y0 + 255), "漏水待避")
    arrow(draw, [(x0 + 35, y0 + 155), (x1 - 35, y0 + 155)])

    # 8 Deep sea
    p = panel(draw, 1, 2, "8", "深海降下区画", "一度パルスを押して光を追う")
    x0, y0, x1, y1 = p
    for index in range(4):
        cx = x0 + 72 + index * 92
        draw.ellipse((cx - 28, y0 + 35, cx + 28, y0 + 91), fill=(18, 39, 75), outline=(103, 139, 229), width=3)
        label(draw, (cx, y0 + 112), str(index + 1))
    draw.ellipse((x1 - 78, y1 - 75, x1 - 34, y1 - 31), fill=PUZZLE, outline=TEXT, width=2)
    label(draw, (x1 - 56, y1 - 94), "パルス", PUZZLE)
    draw.rectangle((x0 + 28, y1 - 78, x0 + 62, y1 - 30), fill=(53, 82, 61), outline=LIGHT, width=2)
    label(draw, (x0 + 45, y1 - 96), "レバー")
    arrow(draw, [(x1 - 35, y1 - 25), (x0 + 35, y1 - 25)])

    # 9 Panorama
    p = panel(draw, 2, 2, "9", "深海パノラマ", "見る・レバー・出口")
    x0, y0, x1, y1 = p
    draw.arc((x0 + 35, y0 + 25, x1 - 35, y1 + 130), 190, 350, fill=LIGHT, width=12)
    draw.ellipse((x0 + 175, y0 + 80, x0 + 280, y0 + 125), fill=(9, 25, 41), outline=EVENT, width=3)
    label(draw, (x0 + 227, y0 + 102), "巨大影", EVENT)
    draw.rectangle((x0 + 18, y1 - 110, x0 + 65, y1 - 30), fill=(74, 52, 43), outline=ROUTE, width=3)
    label(draw, (x0 + 42, y1 - 130), "非常口")
    draw.rectangle((x0 + 80, y1 - 84, x0 + 110, y1 - 40), fill=(53, 82, 61), outline=LIGHT, width=2)
    arrow(draw, [(x1 - 35, y1 - 40), (x0 + 72, y1 - 40)])

    legend_y = HEIGHT - 55
    legend = (
        ("水槽", WATER),
        ("進行方向", ROUTE),
        ("軽い操作", PUZZLE),
        ("主役演出", EVENT),
    )
    cursor = 75
    for text, color in legend:
        draw.rounded_rectangle((cursor, legend_y - 13, cursor + 26, legend_y + 13), radius=5, fill=color)
        cursor += 36
        draw.text((cursor, legend_y), text, font=FONT_LEGEND, fill=MUTED, anchor="lm")
        cursor += round(draw.textlength(text, font=FONT_LEGEND)) + 45

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    image.save(OUTPUT)
    print(OUTPUT)


if __name__ == "__main__":
    build()


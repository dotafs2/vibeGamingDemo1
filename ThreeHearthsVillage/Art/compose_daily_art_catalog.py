"""Compose the daily native UE art-catalog poster from captured PNGs."""

from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont, ImageStat


PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
CATALOG = PROJECT / "Saved/ThreeHearths/ArtCatalog/catalog.json"
WORK = REPO / ".codex-ue58-diagnostics/daily-art-20260907"
THUMBS = WORK / "thumbs"
RENDER_REPORT = WORK / "render-report.json"
OUTPUT = PROJECT / "Docs/Art_Daytime_2026-09-07.png"
OUTPUT_JSON = PROJECT / "Docs/Art_Daytime_2026-09-07.json"
REVIEW = WORK / "poster-review.png"
W, H = 3200, 2440
M = 120
CREAM = "#F4F0E6"
PAPER = "#FBF9F3"
FOREST = "#2E4C3D"
MOSS = "#6D806B"
TERRACOTTA = "#B46E50"
INK = "#35443B"
MUTED = "#7E857B"
LINE = "#D9D8CE"
REGULAR_FONT = r"C:\Windows\Fonts\msyh.ttc"
BOLD_FONT = r"C:\Windows\Fonts\msyhbd.ttc"

HOUSING_IDS = ["rowhouse", "shop_house", "courtyard_workshop", "warehouse", "inn"]
PLANT_IDS = ["oak", "birch", "orchard", "cypress", "flowering_shrub", "wildflowers"]
CASTLE_ID = "royal_keep_garden_v2"


class Poster:
    def __init__(self, catalog, render_records):
        self.catalog = {entry["id"]: entry for entry in catalog["entries"]}
        self.render_records = {record["id"]: record for record in render_records}
        self.canvas = Image.new("RGB", (W, H), CREAM)
        self.draw = ImageDraw.Draw(self.canvas)
        self.fonts = {}
        self.placements = []
        self.qa = []

    def font(self, size, bold=False):
        key = (size, bold)
        if key not in self.fonts:
            self.fonts[key] = ImageFont.truetype(BOLD_FONT if bold else REGULAR_FONT, size)
        return self.fonts[key]

    def text(self, xy, value, size=28, fill=INK, bold=False, anchor="lt"):
        self.draw.text((int(xy[0]), int(xy[1])), value, font=self.font(size, bold), fill=fill, anchor=anchor)

    def fit_text_size(self, value, max_width, size, minimum=15, bold=False):
        while self.draw.textlength(value, font=self.font(size, bold)) > max_width and size > minimum:
            size -= 1
        return size

    def rounded_card(self, box, fill=PAPER, outline=None, radius=22, width=2):
        self.draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

    def image_in_box(self, image_path, box, entry_id):
        path = Path(image_path)
        if not path.exists():
            raise FileNotFoundError("Missing native UE thumbnail for %s: %s" % (entry_id, path))
        source = Image.open(path).convert("RGB")
        if max(ImageStat.Stat(source).stddev) < 8.0:
            raise ValueError("Capture is blank or lacks visible model detail: %s" % path)
        target_w, target_h = int(box[2] - box[0]), int(box[3] - box[1])
        source.thumbnail((target_w, target_h), Image.Resampling.LANCZOS)
        x = int(box[0] + (target_w - source.width) / 2)
        y = int(box[1] + (target_h - source.height) / 2)
        self.canvas.paste(source, (x, y))
        self.qa.append({
            "id": entry_id,
            "thumbnail": str(path),
            "image_box": [int(v) for v in box],
            "source_size_px": [Image.open(path).width, Image.open(path).height],
            "full_image_fitted": True,
            "exists": True,
        })

    def render_entry(self, entry_id, box, serial, style, label_y=None):
        entry = self.catalog[entry_id]
        record = self.render_records.get(entry_id)
        if record is None:
            raise ValueError("No render report record for %s" % entry_id)
        image_box = box
        self.rounded_card((box[0] - 18, box[1] - 18, box[2] + 18, box[3] + 108), fill="#EEEAE0", radius=20)
        self.image_in_box(record["thumbnail"], image_box, entry_id)
        self.text((box[0] + 6, box[1] + 8), "%02d" % serial, 19, MUTED)
        name = entry["name_zh"]
        name_size = self.fit_text_size(name, box[2] - box[0] - 12, style[0], style[1], True)
        self.text((box[0] + 4, label_y if label_y is not None else box[3] + 18), name, name_size, FOREST, True)
        self.placements.append({
            "number": serial,
            "id": entry_id,
            "name_zh": name,
            "kind": entry["kind"],
            "thumbnail": record["thumbnail"],
            "image_box": [int(v) for v in image_box],
        })

    def build(self):
        self.text((M, 62), "THREE HEARTHS  /  NATIVE ART STUDY", 24, TERRACOTTA, True)
        self.text((M, 112), "今日美术扩展 · 2026.09.07", 76, FOREST, True)
        self.text((M, 224), "12 个原生 UE 组合条目 · 住宅、城堡方案与植物目录", 30, MUTED)
        self.text((W - M, 84), "12", 82, TERRACOTTA, True, "rt")
        self.text((W - M, 184), "NATIVE ITEMS", 20, MUTED, True, "rt")
        self.draw.line((M, 318, W - M, 318), fill=LINE, width=3)

        castle_x, castle_y, castle_w = M, 392, 1320
        self.text((castle_x, castle_y), "城堡完整方案预览", 34, FOREST, True)
        self.text((castle_x, castle_y + 50), "ASSEMBLY PREVIEW  ·  1276 模块源目录", 18, MUTED, True)
        castle_box = (castle_x + 4, castle_y + 100, castle_x + castle_w - 12, castle_y + 850)
        self.render_entry(CASTLE_ID, castle_box, 6, (33, 18), castle_box[3] + 28)
        self.text((castle_x + 4, castle_y + 958), "城堡为完整方案预览，当前游戏实建 14 / 1276。", 23, INK)

        right_x, right_y, right_w = 1540, 392, W - M - 1540
        self.text((right_x, right_y), "住宅组合", 34, FOREST, True)
        self.text((right_x, right_y + 50), "HOUSING  ·  5 COMPOSED ENTRIES", 18, MUTED, True)
        gap = 25
        cell_w = int((right_w - gap * 2) / 3)
        housing_serial = 1
        for index, entry_id in enumerate(HOUSING_IDS):
            row, col = divmod(index, 3)
            x = right_x + col * (cell_w + gap)
            y = right_y + 102 + row * 395
            self.render_entry(entry_id, (x + 4, y, x + cell_w - 4, y + 235), housing_serial, (25, 16), y + 252)
            housing_serial += 1

        plants_y = 1500
        self.draw.line((M, plants_y - 28, W - M, plants_y - 28), fill=LINE, width=3)
        self.text((M, plants_y), "植物目录", 34, FOREST, True)
        self.text((M, plants_y + 50), "PLANTS  ·  NATIVE MESH ASSETS WITH CATALOG TINT OVERRIDES", 18, MUTED, True)
        plant_gap = 22
        plant_w = int((W - 2 * M - plant_gap * 5) / 6)
        for index, entry_id in enumerate(PLANT_IDS):
            x = M + index * (plant_w + plant_gap)
            y = plants_y + 102
            self.render_entry(entry_id, (x + 4, y, x + plant_w - 4, y + 300), 7 + index, (22, 14), y + 320)

        footer_y = 2140
        self.draw.rounded_rectangle((M, footer_y, W - M, 2320), radius=28, fill="#E9E8DD")
        self.text((M + 36, footer_y + 30), "复用构件 + 程序化组合；展示当前建筑配方，植物仍为基础几何体组合。", 28, INK, True)
        self.text((M + 36, footer_y + 88), "高地、盘旋上山的道路、国王雕像尚未制作。", 26, INK)
        self.text((W - M - 36, footer_y + 139), "各条目独立缩放 · 中性棚拍光照", 21, MUTED, anchor="rt")

        self.draw.line((M, 2360, W - M, 2360), fill=LINE, width=2)
        self.text((M, 2385), "Native UE meshes  ·  Baseline 2366a8b (10:02)  ·  2026.09.07", 18, MUTED)
        return self.canvas


def main():
    if not CATALOG.exists():
        raise FileNotFoundError("Missing native catalog: %s" % CATALOG)
    if not RENDER_REPORT.exists():
        raise FileNotFoundError("Run the UE native renderer first: %s" % RENDER_REPORT)
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    render_records = json.loads(RENDER_REPORT.read_text(encoding="utf-8"))
    if catalog.get("schema") != 1 or len(catalog.get("entries", [])) != 12:
        raise ValueError("Expected schema 1 catalog with 12 entries")
    ids = {entry["id"] for entry in catalog["entries"]}
    expected = set(HOUSING_IDS + [CASTLE_ID] + PLANT_IDS)
    if ids != expected:
        raise ValueError("Poster IDs do not match the daily catalog: %s" % sorted(ids))
    if {row.get("id") for row in render_records} != expected:
        raise ValueError("Poster requires all 12 completed native UE captures")

    poster = Poster(catalog, render_records)
    canvas = poster.build()
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(OUTPUT, optimize=True, dpi=(200, 200))
    preview = canvas.copy()
    preview.thumbnail((1600, 1500), Image.Resampling.LANCZOS)
    REVIEW.parent.mkdir(parents=True, exist_ok=True)
    preview.save(REVIEW, optimize=True)

    metadata = {
        "png": str(OUTPUT),
        "review_thumbnail": str(REVIEW),
        "dimensions_px": [W, H],
        "title": "今日美术扩展 · 2026.09.07",
        "counts": {"housing": 5, "castle": 1, "plants": 6, "total": 12},
        "catalog_sha256": hashlib.sha256(CATALOG.read_bytes()).hexdigest(),
        "render_report": str(RENDER_REPORT),
        "layout": "castle large hero; five housing medium cards; six plant small cards",
        "lighting_note": "neutral studio lighting, not UE in-game lighting",
        "source_note": "native UE StaticMesh assets and asset materials; only catalog color overrides use M_VillageTint",
        "footer": [
            "复用组件与程序化组合",
            "城堡为完整方案预览，游戏实建14/1276",
            "高地与国王雕像尚未制作",
        ],
        "placements": poster.placements,
        "qa": poster.qa,
    }
    OUTPUT_JSON.write_text(json.dumps(metadata, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"png": str(OUTPUT), "json": str(OUTPUT_JSON), "review": str(REVIEW), "items": 12}, ensure_ascii=False))


if __name__ == "__main__":
    main()

"""Rebuild the delivery inventory from files on disk; no UE or paid API calls."""
from __future__ import annotations

import csv
import hashlib
import io
import json
import pathlib
import subprocess
from collections import Counter
from datetime import datetime, timezone

PROJECT = pathlib.Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
ART = PROJECT / "Art"
DOCS = PROJECT / "Docs"

# Labels identify authored/exported asset entries, not unique original geometry.
LABEL_TEXT = """
beam_timber_2m|2米木梁
bench_timber|木长凳
canopy_slateblue_2m|蓝灰瓦雨棚
canopy_terracotta_2m|红陶瓦雨棚
carry_logs|搬运原木包
carry_planks|搬运木板包
carry_plaster|搬运灰浆桶
carry_stones|搬运石料箱
carry_tiles|搬运瓦片箱
door_oak|橡木门
floor_opening_2m|楼梯井开口楼板
floor_timber_2m|木楼板
foundation_stone_2m|石地基
frame_timber_2m|木结构框架
gable_plaster_4m|灰泥山墙
gable_stone_4m|石山墙
gable_timber_4m|木山墙
porch_post_timber|门廊木柱
porch_steps_stone_2m|门廊石台阶
post_timber_2_4m|2.4米木柱
railing_timber_2m|木栏杆
roof_ridge_slateblue_2m|蓝灰瓦屋脊
roof_ridge_terracotta_2m|红陶瓦屋脊
roof_ridge_timber_2m|木屋脊
roof_slope_slateblue_2m|蓝灰瓦坡面
roof_slope_terracotta_2m|红陶瓦坡面
roof_slope_timber_2m|木屋顶坡面
shutter_sage|鼠尾草绿窗板
stairs_switchback_2x4m|折返楼梯
table_communal|公共木桌
wall_door_plaster_2m|带门洞灰泥墙
wall_door_stone_2m|带门洞石墙
wall_door_timber_2m|带门洞木墙
wall_plaster_2m|灰泥实墙
wall_stone_2m|石实墙
wall_timber_2m|木实墙
wall_window_plaster_2m|带窗灰泥墙
wall_window_stone_2m|带窗石墙
wall_window_timber_2m|带窗木墙
workbench_carpenter|木工工作台
cottage_terracotta|红陶瓦灰泥小屋组合
longhouse_slateblue|蓝灰瓦木长屋组合
townhouse_terracotta|红陶瓦双层石屋组合
castle_buttress_stone|城堡石扶壁
castle_gate_arch_4m|4米城门石拱
castle_gate_oak_pair|双扇橡木城门
castle_tower_battlement_cap_4m|塔楼垛口顶段
castle_tower_storey_4m|塔楼层段
castle_walkway_timber_2m|城墙木步道
castle_wall_battlement_2m|城墙垛口段
castle_wall_stone_2m|城堡石墙段
goods_beams_bundle|成品房梁捆
goods_bricks_tiles_crate|砖瓦混装箱
goods_paint_pails|涂料桶组
goods_planks_bundle|成品木板捆
kingdom_banner_blue|蓝色王国旗
kingdom_banner_red|红色王国旗
market_barrel_oak|市场橡木桶
market_crate_oak|市场橡木箱
market_stall_blue_2m|蓝色市集摊位
market_stall_red_2m|红色市集摊位
profession_carpenter_cap|木匠帽
profession_mason_hood|石匠兜帽
profession_smith_apron|铁匠围裙
regalia_king_crown|国王皇冠
tool_carpenter_hammer|木工锤
tool_carpenter_saw|木工锯
tool_mason_chisel|石匠凿
tool_mason_trowel|石匠抹刀
village_lantern_post|村庄灯柱
village_notice_board|村庄告示牌
workshop_blacksmith_forge|铁匠锻炉
workshop_carpenter_station|木工工位
workshop_mason_station|石工工位
workshop_tile_kiln|烧瓦窑
guild_market_yard|行会市集院落组合
kings_gate_courtyard|国王城门院落组合
apron_linen_short|亚麻短围裙
backpack_bedroll|背包与铺盖卷
bag_crossbody_leather|皮革斜挎包
beard_neat_silver|整齐银胡须
cape_royal_blue|王室蓝披风
cap_merchant_plum|梅紫商人帽
hair_braid_auburn|赤褐辫发
hair_bun_dark|深色发髻
hair_cropped_dark|深色短发
hair_swept_silver|银色侧梳发
hair_waves_chestnut|栗色波浪发
hat_straw_wide|宽檐草帽
headwrap_sage|鼠尾草绿头巾
pouch_belt_double|双腰包
scarf_red|红围巾
shawl_ochre|赭黄披肩
basket_berries|浆果篮
basket_empty|空篮
bed_double_2x2m|双人床
bed_single_1_1x2m|单人床
bench_backed_1_8m|靠背长凳
chair_oak_wide|宽橡木椅
fence_low_2m|矮木围栏
firewood_stack|柴火堆
food_tray_bread|面包餐盘
grain_chest|粮食箱
table_communal_2_6m|2.6米公共餐桌
table_dining_1_2m|1.2米家用餐桌
cabin_living_4x4m|4×4米居住角组合
common_meal_10_seats|十人公共餐区组合
goods_bricks_crate|烧制砖块箱
goods_iron_ingots_bundle|铁锭捆
goods_lime_pail|石灰桶
goods_nails_box|铁钉盒
goods_pigment_pots|颜料罐组
goods_raw_clay_basket|黏土篮
goods_tiles_slateblue_crate|蓝灰瓦商品箱
goods_tiles_terracotta_crate|红陶瓦商品箱
tool_axe|斧头
tool_hammer|铁锤
tool_hoe|锄头
tool_mallet|木槌
tool_pickaxe|镐
tool_saw|手锯
tool_shovel|铲子
tool_trowel|砌筑抹刀
public_wall_foundation_2m|公共城墙石基座
public_wall_parapet_2m|公共城墙木护栏
public_wall_stone_2m|公共城墙石墙段（复用城堡墙几何）
public_wall_walkway_2m|公共城墙木步道
town_corner_stone_2m|街区直角石墙连接件
town_gable_end_timber_2m|木屋顶端封板
town_roof_ridge_joint_2m|屋脊连接节点
town_roof_valley_joint_2m|L/T屋顶谷线节点
town_stair_timber_2m|八级木楼梯段
town_wall_gate_timber_2m|带中央通道木墙
wip_log_to_beam|原木加工房梁半成品
wip_log_to_planks|原木锯板半成品
HearthCottage|原始小屋整模
HearthCottage_SharedUV|共享UV小屋整模
HearthCottage_SharedUV_Polished|共享UV与屋顶高光改进小屋整模
"""
LABELS = dict(line.split("|", 1) for line in LABEL_TEXT.strip().splitlines())
KINDS = {"module": "独立模块", "assembly": "组合示例", "whole_model_version": "整模版本"}
GROUP_ORDER = ["VillageKit", "SocietyKit", "ResidentKit", "HomeLifeKit", "GoodsKit", "ToolKit", "PublicWallKit", "WoodProductionKit", "TownKit", "HearthCottage"]


def digest(path: pathlib.Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(part)
    return h.hexdigest()


def record(path: pathlib.Path, kind: str, group: str) -> dict:
    return {"kind": kind, "group": group, "id": path.stem,
            "name_zh": LABELS.get(path.stem, path.stem),
            "path": path.relative_to(REPO).as_posix(),
            "bytes": path.stat().st_size, "sha256": digest(path)}


def doc_link(path: str) -> str:
    return pathlib.Path(__import__("os").path.relpath(REPO / path, DOCS)).as_posix()


def main() -> None:
    records = []
    models = []
    native = sorted((PROJECT / "Content/ThreeHearths/Generated").rglob("*.uasset"))
    by_stem = {}
    for path in native:
        by_stem.setdefault(path.stem, []).append(path.relative_to(REPO).as_posix())
    for path in sorted(ART.rglob("*")):
        if not path.is_file() or path.suffix.lower() in (".pyc", ".blend1", ".log") or "__pycache__" in path.parts:
            continue
        rel = path.relative_to(ART)
        group = rel.parts[0] if len(rel.parts) > 1 else "ArtSupport"
        suffix = path.suffix.lower()
        if suffix in (".glb", ".gltf", ".fbx", ".obj"):
            kind = "module" if "modules" in rel.parts else "assembly" if "examples" in rel.parts else "whole_model_version"
        elif suffix == ".blend":
            kind = "blender_source"
        elif suffix in (".png", ".jpg", ".svg", ".webp"):
            kind = "preview_or_uv_diagram"
        elif suffix in (".json", ".py", ".md", ".html", ".ps1"):
            kind = "metadata_or_generator"
        else:
            continue
        item = record(path, kind, group)
        if kind in KINDS:
            if path.stem not in LABELS:
                raise ValueError(f"Missing label: {path}")
            item["native_name_matches"] = by_stem.get(path.stem, [])
            item["native_check"] = "发现同名UE原生包；不等于已接入玩法" if item["native_name_matches"] else "未发现同名UE包；可能未导入或使用改名路径"
            models.append(item)
        records.append(item)
    for path in native:
        records.append(record(path, "native_asset_package", path.relative_to(PROJECT / "Content/ThreeHearths/Generated").parts[0]))
    for path in sorted((PROJECT / "Docs/Validation").rglob("*")):
        if path.suffix.lower() in (".png", ".jpg", ".svg", ".webp"):
            records.append(record(path, "validation_capture", "Validation"))
    for root in (REPO / "public", REPO / "docs"):
        for path in sorted(root.rglob("*")):
            if path.is_file() and path.suffix.lower() in (".png", ".jpg", ".svg", ".webp", ".glb", ".blend"):
                records.append(record(path, "legacy_project_art", "/".join(path.relative_to(REPO).parts[:-1])))
    counts = Counter(r["kind"] for r in records)
    report = {"generated_at_utc": datetime.now(timezone.utc).isoformat(),
              "git_baseline": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip(),
              "scope": "Authored/exported ThreeHearths art, native generated packages, supporting media and separately listed legacy project art. Excludes Cropout sample assets, secrets, runtime private ledgers and caches.",
              "counting_note": "Counts are exported entries/files; examples, versions, reused geometry and derived UE materials must not be called distinct original models.",
              "counts": dict(counts), "records": records}
    DOCS.mkdir(exist_ok=True)
    (DOCS / "Art_Asset_Inventory.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with (DOCS / "Art_Asset_Inventory.csv").open("w", encoding="utf-8-sig", newline="") as out:
        writer = csv.DictWriter(out, fieldnames=["kind", "group", "id", "name_zh", "path", "bytes", "sha256", "native_name_matches", "native_check"])
        writer.writeheader()
        for item in records:
            row = dict(item)
            row["native_name_matches"] = "; ".join(row.get("native_name_matches", []))
            writer.writerow(row)
    lines = ["# 美术资产完整盘点", "", "按本地实际文件生成。每项的路径、字节数、SHA-256 和同名 UE 包记录见 [CSV](Art_Asset_Inventory.csv) 与 [JSON](Art_Asset_Inventory.json)。", "",
             f"本次中世纪项目有 **{counts['module']} 个独立模块、{counts['assembly']} 个组合示例、{counts['whole_model_version']} 个小屋整模版本，共 {len(models)} 个 GLB 导出条目；{counts['blender_source']} 份 Blender 源文件**。计数含整理和复用的模块条目，组合和版本分别计数，不代表同等数量的全新原创几何。", "",
             f"另记录 {counts['native_asset_package']} 个生成目录中的 UE 原生资产包（包含模型、材质等），{counts['preview_or_uv_diagram']} 张/份美术预览与 UV 图，以及 {counts['legacy_project_art']} 份同仓库旧二维项目素材/设计图。完整文件清单同时收录制作脚本、规格和验收媒体。", "",
             "Cropout 原有村民身体、骨骼、动画、植被、作物、岛屿和音效属于复用素材，不计入本次自制资产。ResidentKit 是 16 件原创建模配件及 10 套外观组合，未新增 10 个独立角色身体。", "",
             "## 使用状态的边界", "",
             "- VillageKit 原始 38 件已增加两件木屋顶组件，当前是 40 件。ResidentialVariants 的 40 件审计与这批相同，不能重复加算。", 
             "- 木/石住宅、公共墙和私人陶瓦屋顶已有实际施工验收；制瓦订单、运输和四个陶瓦坡面见 Tile_Workshop_Acceptance.md。其余模块是否参与玩法须逐项看运行证据。", 
             "- TownKit 六件连接件，以及 WoodProductionKit 两件半成品，在本次文件匹配中没有同名 UE 包；它们的源资产存在，不能据文件存在宣称已在游戏使用。", 
             "- PublicWallKit 石墙段复用 SocietyKit 几何，其余三件为配套组件。城堡、市集和室内组合是可编辑装配示例，不等于 NPC 已具备完整城堡建设或居住玩法。", 
             "- GoodsKit 中石灰、颜料、铁锭、铁钉、蓝灰瓦等商品模型已制作；相应生产链不因模型存在而自动完成。", "",
             "## 独立模块、组合示例和版本（逐项）", ""]
    for group in GROUP_ORDER:
        items = [r for r in models if r["group"] == group]
        lines += [f"### {group} — {len(items)} 个导出条目", "", "| 名称 | 稳定 ID | 类别 | UE 同名包 |", "| --- | --- | --- | --- |"]
        for r in items:
            status = "有" if r["native_name_matches"] else "未匹配"
            lines.append(f"| {r['name_zh']} | [{r['id']}]({doc_link(r['path'])}) | {KINDS[r['kind']]} | {status} |")
        lines.append("")
    lines += ["## Blender 源文件（逐项）", ""]
    for r in records:
        if r["kind"] == "blender_source":
            lines.append(f"- [{r['group']}/{r['id']}.blend]({doc_link(r['path'])})")
    lines += ["", "## 同仓库较早的矿场二维项目", "", "这部分包含已制作的多时代场景与透明拆件，以及用户提供的参考/原图；不全部宣称原创。来源说明见仓库 public/art/ASSET_PROVENANCE.md。按目录列出全部文件，避免遗漏旧项目成果。", ""]
    legacy = [r for r in records if r["kind"] == "legacy_project_art"]
    for group in sorted({r["group"] for r in legacy}):
        items = [r for r in legacy if r["group"] == group]
        lines += [f"### {group}（{len(items)}）", ""]
        lines += [f"- [{pathlib.Path(r['path']).name}]({doc_link(r['path'])})" for r in items]
        lines.append("")
    lines += ["## 可复现与上传核对", "", "运行 `python ThreeHearthsVillage/Art/build_asset_inventory.py` 可重新扫描。JSON 和 CSV 逐文件带哈希，可与 GitHub 当前交付分支核对。此清单记录本地存在性与包名匹配；上传完成状态须以最终远端 commit 和实际推送结果为准。", ""]
    (DOCS / "Art_Asset_Inventory.md").write_text("\n".join(lines), encoding="utf-8")
    print(json.dumps({"counts": dict(counts), "model_exports": len(models), "models_with_native_name_match": sum(bool(r["native_name_matches"]) for r in models), "files": len(records)}, ensure_ascii=False))


if __name__ == "__main__":
    main()

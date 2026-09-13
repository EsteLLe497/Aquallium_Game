"""Structural validation for the unified V3 aquarium map."""

from __future__ import annotations

import json
import math
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "artifacts" / "game_layout_v3_manifest.json"
GLB = ROOT / "asset" / "model" / "aquarium_game_layout_v3.glb"
REPORT = ROOT / "artifacts" / "game_layout_v3_validation.json"


def rect_overlap(a, b) -> float:
    if abs(a["floor_y"] - b["floor_y"]) > 0.02:
        return 0.0
    width = max(0.0, min(a["maximum_x"], b["maximum_x"]) -
                max(a["minimum_x"], b["minimum_x"]))
    depth = max(0.0, min(a["maximum_z"], b["maximum_z"]) -
                max(a["minimum_z"], b["minimum_z"]))
    return width * depth


def horizontal_distance(points) -> float:
    return sum(math.hypot(b[0] - a[0], b[2] - a[2])
               for a, b in zip(points, points[1:]))


def run() -> dict:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    checks = []

    def check(name: str, condition: bool, detail: str) -> None:
        checks.append({"name": name, "passed": bool(condition), "detail": detail})

    portal_ids = [portal["id"] for portal in data["portals"]]
    expected = {f"P{index:02d}" for index in range(2, 13)}
    check("unique_portals", len(portal_ids) == len(set(portal_ids)),
          f"{len(portal_ids)} authored portal IDs")
    check("complete_portal_chain", expected <= set(portal_ids),
          ", ".join(sorted(set(portal_ids))))
    check("portal_has_two_zones",
          all(portal.get("a") and portal.get("b") for portal in data["portals"]),
          "Every portal names both connected zones")

    # A route label is not enough: every hand-off must physically sit on two
    # walkable supports (two rooms, or one room plus a ramp/stair endpoint).
    unsupported_portals = []
    for portal in data["portals"]:
        authored_x, authored_y, authored_z = portal["position"]
        runtime = (authored_x, authored_y - 2.25, -authored_z)
        support_count = 0
        for floor in data["floors"]:
            if (floor["minimum_x"] - 0.05 <= runtime[0] <= floor["maximum_x"] + 0.05 and
                    floor["minimum_z"] - 0.05 <= runtime[2] <= floor["maximum_z"] + 0.05 and
                    abs(floor["floor_y"] - runtime[1]) <= 0.08):
                support_count += 1
        for path in data["paths"]:
            if any(math.dist(runtime, endpoint) <= 0.80
                   for endpoint in (path["points"][0], path["points"][-1])):
                support_count += 1
        if support_count < 2:
            unsupported_portals.append(
                f"{portal['id']}({support_count} supports)")
    check("portal_geometry_handoffs", not unsupported_portals,
          "all portals have two physical walkable supports"
          if not unsupported_portals else ", ".join(unsupported_portals))

    overlaps = []
    floors = data["floors"]
    def allowed_floor_overlap(first_name: str, second_name: str) -> bool:
        names = (first_name, second_name)
        return (any("CylinderRoomFloor_" in name for name in names) and
                any("CylinderWestPortalFloor" in name or
                    "CylinderEastPortalFloor" in name for name in names))
    for index, first in enumerate(floors):
        for second in floors[index + 1:]:
            area = rect_overlap(first, second)
            if area > 0.001 and not allowed_floor_overlap(
                    first["name"], second["name"]):
                overlaps.append({"a": first["name"], "b": second["name"],
                                 "area": round(area, 4)})
    check("no_unplanned_floor_overlap", not overlaps,
          "none" if not overlaps else json.dumps(overlaps))

    hero = next(volume for volume in data["water_volumes"]
                if volume["name"] == "HT01")
    rocks_outside = []
    for rock in data["rocks"]:
        for axis, axis_name in enumerate("xyz"):
            rock_min = rock["center"][axis] - rock["size"][axis] * 0.5
            rock_max = rock["center"][axis] + rock["size"][axis] * 0.5
            if rock_min < hero["min"][axis] - 0.001 or rock_max > hero["max"][axis] + 0.001:
                rocks_outside.append(f"{rock['name']}:{axis_name}")
    check("tank_props_inside_water", not rocks_outside,
          "none" if not rocks_outside else ", ".join(rocks_outside))

    path_results = []
    all_grades_valid = True
    all_widths_valid = True
    for path in data["paths"]:
        points = path["points"]
        run_length = horizontal_distance(points)
        rise = abs(points[-1][1] - points[0][1])
        grade = rise / max(run_length, 0.001)
        maximum_grade = (0.18 if "ReturnStair" in path["name"] else
                         0.13 if "Arch" in path["name"] else 0.105)
        grade_valid = grade <= maximum_grade
        width_valid = path["half_width"] * 2.0 >= 1.50
        all_grades_valid &= grade_valid
        all_widths_valid &= width_valid
        path_results.append({
            "name": path["name"],
            "horizontal_length": round(run_length, 3),
            "rise": round(rise, 3),
            "average_grade": round(grade, 4),
            "usable_width": round(path["half_width"] * 2.0, 3),
            "grade_valid": grade_valid,
            "width_valid": width_valid,
        })
    check("path_grades", all_grades_valid, json.dumps(path_results))
    check("path_widths", all_widths_valid, json.dumps(path_results))

    glb_bytes = GLB.read_bytes()
    magic, version, total_length = struct.unpack_from("<4sII", glb_bytes, 0)
    check("glb_header", magic == b"glTF" and version == 2 and total_length == len(glb_bytes),
          f"version={version}, bytes={len(glb_bytes)}")
    check("draw_batch_budget", data["mesh_statistics"]["meshes"] <= 24,
          f"{data['mesh_statistics']['meshes']} material batches")
    check("triangle_budget", data["mesh_statistics"]["triangles"] <= 12000,
          f"{data['mesh_statistics']['triangles']} triangles")

    json_length = struct.unpack_from("<I", glb_bytes, 12)[0]
    gltf = json.loads(glb_bytes[20:20 + json_length].decode("utf-8"))
    alpha_modes = {
        material["name"]: material.get("alphaMode", "OPAQUE")
        for material in gltf["materials"]
    }
    required_blend = {
        "WatatsumiWater", "WatatsumiGlass", "WatatsumiWaterSurface",
        "TankWaterArch", "TankGlassArch", "ArchWaterSurface",
        "TankWaterJellyCylinder", "TankGlassJellyCylinder",
    }
    opaque_optics = sorted(
        name for name in required_blend if alpha_modes.get(name) != "BLEND")
    check("optical_materials_are_transparent", not opaque_optics,
          "all required materials use BLEND" if not opaque_optics
          else ", ".join(opaque_optics))

    report = {
        "passed": all(item["passed"] for item in checks),
        "checks": checks,
        "path_metrics": path_results,
    }
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


if __name__ == "__main__":
    result = run()
    print(json.dumps(result, ensure_ascii=False))
    raise SystemExit(0 if result["passed"] else 1)

"""Structural checks for the key-9 reception lobby."""

from __future__ import annotations

import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "artifacts" / "reception_lobby_manifest.json"
GLB = ROOT / "asset" / "model" / "aquarium_reception_lobby.glb"
REPORT = ROOT / "artifacts" / "reception_lobby_validation.json"


def run() -> dict:
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    checks = []

    def check(name: str, condition: bool, detail: str) -> None:
        checks.append({"name": name, "passed": bool(condition), "detail": detail})

    feature = {item["type"]: item for item in data["features"]}
    check("room_dimensions", data["room"] == {"width": 17.0, "depth": 8.5,
                                               "height": 4.5}, str(data["room"]))
    check("three_ticket_desks", feature["ticket_desks"]["count"] == 3,
          "exactly three staffed-style inspection desks")
    check("one_emergency_light", feature["active_emergency_lights"]["count"] == 1,
          "exactly one active emergency fixture")
    check("staff_door_surround_is_closed",
          feature["staff_door"]["surrounding_wall_gap"] == 0.0,
          "continuous north service wall except the 1.2 m door opening")
    required_floors = {
        "Reception_Floor", "HeroHall_Floor", "HeroGallery_Floor",
        "BlackoutVestibule_Floor", "ArchConnector_Floor",
        "Upper_RightArm", "Upper_LeftArm",
        "Upper_CrossBridge"}
    floor_names = {floor["name"] for floor in data["floors"]}
    check("integrated_walkable_surfaces", required_floors <= floor_names,
          f'{len(data["floors"])} collision floors; fixed H deck present')

    names = {box["name"] for box in data["boxes"]}
    required = {"Reception_NorthServiceLeft", "Reception_NorthServiceRight",
                "Reception_NorthServiceDoorHeader", "Reception_StaffDoor",
                "HeroHall_SouthUpperWall", "HeroHall_SouthWingLeft",
                "HeroHall_SouthWingRight", "HeroGallery_EntranceUpperSeal",
                "HeroTank_RightSideWall", "HeroHall_NorthEastReturn",
                "ArchConnector_WestWall", "ArchConnector_EastWall",
                "HeroHall_WestWallFront", "HeroHall_WestWallRear",
                "HeroHall_WestGalleryClosure",
                "Blackout_GalleryWestJamb", "Blackout_GalleryEastJamb",
                "Blackout_GalleryHeaderClearance",
                "Blackout_TankOcclusionWing",
                "ArchConnector_WestReturn",
                "ArchConnector_EastReturn",
                "V4_FormerExitSealEnd", "V4_NorthFacadeHeader0",
                "V4_EastFacadeHeader0", "V4_WestFacadeHeader0"}
    check("service_wall_parts_present", required <= names,
          ", ".join(sorted(required & names)))

    # Adjacent north-wall intervals touch exactly: no mystery void beside door.
    boxes = {box["name"]: box for box in data["boxes"]}
    left = boxes["Reception_NorthServiceLeft"]["maximum"][0]
    door_min = boxes["Reception_StaffDoor"]["minimum"][0]
    door_max = boxes["Reception_StaffDoor"]["maximum"][0]
    right = boxes["Reception_NorthServiceRight"]["minimum"][0]
    check("staff_door_horizontal_seal",
          abs(left - 6.15) < 0.001 and door_min >= 6.15 and
          door_max <= 7.35 and abs(right - 7.35) < 0.001,
          f"left={left:.3f}, door=[{door_min:.3f},{door_max:.3f}], right={right:.3f}")

    raw = GLB.read_bytes()
    magic, version, total = struct.unpack_from("<4sII", raw, 0)
    check("valid_glb", magic == b"glTF" and version == 2 and total == len(raw),
          f"version={version}, bytes={len(raw)}")
    stats = data["mesh_statistics"]
    check("draw_batch_budget", stats["meshes"] <= 25,
          f'{stats["meshes"]} material batches')
    check("triangle_budget", stats["triangles"] <= 18000,
          f'{stats["triangles"]} triangles')

    features = {item["type"]: item for item in data["features"]}
    bubbles = features.get("hero_tank_bubbles", {})
    check("hero_tank_bubbles_are_one_batch",
          bubbles.get("count") == 96 and
          bubbles.get("emitters") == 3 and
          bubbles.get("draw_batches") == 1,
          str(bubbles))
    hero = features["hero_tank"]
    check("hero_pane_dimensions", hero["pane"] == [17.0, 10.2] and
          hero["depth"] == 8.0 and hero["surface_y"] == 10.2 and
          hero["surface_hidden"] is True and
          hero["surface_y"] - hero["upper_floor_y"] >= 4.5, str(hero))
    check("straight_gallery", features["straight_side_gallery"]["length"] == 8.0,
          "left wall / right tank glass / 8 m straight run")
    ramp = features["ramp"]
    check("game_ramp_spec", ramp["width"] == 4.2 and
          ramp["rise"] == 5.5 and ramp["clear_height"] >= 4.0 and
          ramp["entrance_width"] == 4.8 and
          ramp["maximum_slope"] <= 0.18 and
          ramp["layout"] == "watatsumi_wrap",
          str(ramp))
    check("upper_h_has_four_room_connections",
          features["upper_h_walkway"]["portals"] == 4 and
          features["upper_h_walkway"]["arm_width"] == 4.8 and
          features["upper_h_walkway"]["depth"] == 17.0 and
          features["upper_h_walkway"]["observation_depth"] == 6.45 and
          features["upper_h_walkway"]["observation_width"] == 28.5 and
          features["upper_h_walkway"]["tank_reveal_gap"] == 0.65,
          str(features["upper_h_walkway"]))

    # Runtime floor rectangles must touch exactly at every public hand-off.
    floors = {floor["name"]: floor for floor in data["floors"]}
    check("reception_to_hall_is_flush",
          abs(floors["Reception_Floor"]["maximum_z"] -
              floors["HeroHall_Floor"]["minimum_z"]) < 0.001 and
          abs(floors["Reception_Floor"]["floor_y"] -
              floors["HeroHall_Floor"]["floor_y"]) < 0.001,
          "same plane at runtime z=0")
    check("hall_to_gallery_is_flush",
          abs(floors["HeroHall_Floor"]["maximum_z"] -
              floors["HeroGallery_Floor"]["minimum_z"]) < 0.001,
          "same plane at runtime z=8")
    check("gallery_to_blackout_is_flush",
          abs(floors["HeroGallery_Floor"]["maximum_z"] -
              floors["BlackoutVestibule_Floor"]["minimum_z"]) < 0.001,
          "same plane at runtime z=16")

    # The generated centre line is the collision source of truth. Verify every
    # sloped segment and the 1F/2F endpoints rather than trusting the comment.
    path = data["paths"][0]
    points = path["points"]
    slopes = []
    route_length = 0.0
    for first, second in zip(points, points[1:]):
        dx = second[0] - first[0]
        dy = second[1] - first[1]
        dz = second[2] - first[2]
        horizontal = (dx * dx + dz * dz) ** 0.5
        route_length += (dx * dx + dy * dy + dz * dz) ** 0.5
        slopes.append(abs(second[1] - first[1]) / max(horizontal, 0.0001))
    check("ramp_runtime_slope", max(slopes) <= 0.18,
          f"max={max(slopes):.4f}, width={path['half_width'] * 2:.2f}")
    check("compact_ramp_length", 48.0 <= route_length <= 60.0,
          f"length={route_length:.2f} m")
    check("ramp_endpoint_elevations",
          abs(points[0][1] + 2.25) < 0.001 and
          abs(points[-1][1] - 3.25) < 0.001,
          f"start={points[0][1]:.2f}, end={points[-1][1]:.2f}")

    # The visible doorway and collision shell must agree. No outer ramp wall
    # may cover the runtime z=[1.2, 4.8] entrance band.
    ramp_shell = [boxes[name] for name in
                  ("HeroHall_EastWallFront", "HeroHall_EastWallRear")]
    doorway_clear = all(not (box["minimum"][2] < 5.399 and
                             box["maximum"][2] > 0.601)
                         for box in ramp_shell)
    check("ramp_doorway_collision_clear", doorway_clear,
          ", ".join(box["name"] for box in ramp_shell))

    # Hall and ramp use one exact doorway interval.  Any disagreement here
    # becomes the black wedges seen from the 1F viewing floor.
    hall_front = boxes["HeroHall_EastWallFront"]
    hall_rear = boxes["HeroHall_EastWallRear"]
    check("hall_ramp_shell_intervals_match",
          abs(hall_front["maximum"][2] - 0.6) < 0.001 and
          abs(hall_front["minimum"][2] - 0.0) < 0.001 and
          abs(hall_rear["maximum"][2] - 8.0) < 0.001 and
          abs(hall_rear["minimum"][2] - 5.4) < 0.001,
          "shared runtime opening z=[0.6, 5.4]")

    west_front = boxes["HeroHall_WestWallFront"]
    west_middle = boxes["V4_FormerExitSealEnd"]
    west_rear = boxes["HeroHall_WestWallRear"]
    west_closure = boxes["HeroHall_WestGalleryClosure"]
    west_gallery = boxes["HeroGallery_WestWall"]
    check("hero_hall_west_wall_has_no_vertical_slit",
          abs(west_front["maximum"][2] - west_middle["minimum"][2]) < 0.001 and
          abs(west_middle["maximum"][2] - west_rear["minimum"][2]) < 0.001 and
          abs(west_rear["maximum"][2] - west_closure["minimum"][2]) < 0.001 and
          abs(west_closure["maximum"][2] - west_gallery["minimum"][2]) < 0.001,
          "continuous west wall from runtime z=0.0 through z=16.0")

    entrance_header = boxes["HeroHall_RampDoorHeader"]
    upper_seal = boxes["V4_FormerExitSealEnd"]
    upper_header = boxes["V4_NorthFacadeHeader0"]
    check("single_low_arch_visible_from_hall",
          abs(entrance_header["minimum"][1] - 1.75) < 0.001 and
          upper_seal["minimum"][2] >= 0.899 and
          upper_seal["maximum"][1] <= 3.051 and
          upper_header["minimum"][1] >= 7.249,
          "1F east portal sealed; 2F west exit clears the rising ramp")

    # The 2F floor modules retain the H circulation but replace its narrow
    # cross-piece with a broad observation gallery in front of the acrylic.
    check("upper_h_dimensions_and_datum",
          abs(floors["Upper_RightArm"]["minimum_x"] - 9.45) < 0.001 and
          abs(floors["Upper_RightArm"]["maximum_x"] - 14.25) < 0.001 and
          abs(floors["Upper_LeftArm"]["minimum_x"] + 14.25) < 0.001 and
          abs(floors["Upper_LeftArm"]["maximum_x"] + 9.45) < 0.001 and
          abs(floors["Upper_CrossBridge"]["minimum_x"] + 9.45) < 0.001 and
          abs(floors["Upper_CrossBridge"]["maximum_x"] - 9.45) < 0.001 and
          abs(floors["Upper_CrossBridge"]["minimum_z"] - 0.90) < 0.001 and
          abs(floors["Upper_CrossBridge"]["maximum_z"] - 7.35) < 0.001 and
          len({floors[name]["floor_y"] for name in
               ("Upper_RightArm", "Upper_LeftArm",
                "Upper_CrossBridge")}) == 1,
          "continuous 4.8 m arms and 18.9 x 6.45 m cross-piece")

    check("upper_h_floor_modules_do_not_overlap",
          floors["Upper_CrossBridge"]["maximum_x"] <=
              floors["Upper_RightArm"]["minimum_x"] + 0.001 and
          floors["Upper_CrossBridge"]["minimum_x"] >=
              floors["Upper_LeftArm"]["maximum_x"] - 0.001,
          "three floor modules meet only at short side edges")

    # Visual clearance is a product requirement: no floor may occupy the tank
    # frame and the overlook guard must sit in dry space before the acrylic.
    check("upper_gallery_clears_tank_volume",
          floors["Upper_RightArm"]["minimum_x"] >= 9.40 and
          floors["Upper_LeftArm"]["maximum_x"] <= -9.40 and
          floors["Upper_CrossBridge"]["maximum_z"] <= 7.35,
          "side decks clear x=+-9.2 frame; overlook ends 0.65 m before glass")

    ramp_end = points[-1]
    landing = floors["Upper_LeftArm"]
    check("ramp_exit_lands_on_upper_deck",
          landing["minimum_x"] <= ramp_end[0] <= landing["maximum_x"] and
          landing["minimum_z"] <= ramp_end[2] <= landing["maximum_z"] and
          abs(ramp_end[1] - landing["floor_y"]) < 0.001,
          f"ramp end {ramp_end} lies inside broad 2F observation deck")

    # Benches must not enter either 2.15 m wall-side circulation lane.
    bench_boxes = [box for box in data["boxes"]
                   if box["name"].startswith("HeroHall_Bench_") and
                   box["name"].endswith("_Seat")]
    lanes_clear = all(box["maximum"][0] <= 9.20 and
                      box["minimum"][0] >= -9.20 for box in bench_boxes)
    check("hero_hall_side_lanes_clear", lanes_clear and len(bench_boxes) == 4,
          f"{len(bench_boxes)} benches; clear wall lanes x<-9.2 and x>9.2")

    check("blackout_to_arch_connector_is_flush",
          abs(floors["BlackoutVestibule_Floor"]["maximum_z"] -
              floors["ArchConnector_Floor"]["minimum_z"]) < 0.001 and
          abs(floors["BlackoutVestibule_Floor"]["floor_y"] -
              floors["ArchConnector_Floor"]["floor_y"]) < 0.001,
          "open portal and connector share runtime z=19.5 and one floor datum")

    arch_path = next(path for path in data["paths"]
                     if path["name"] == "Reception_UnderwaterArch_Walkway")
    arch_start, arch_end = arch_path["points"][0], arch_path["points"][-1]
    check("reused_arch_transform_matches_connector",
          abs(arch_start[0] + 10.05) < 0.001 and
          abs(arch_start[1] + 2.25) < 0.001 and
          abs(arch_start[2] - 25.0) < 0.001 and
          abs(arch_end[2] - 73.0) < 0.001 and
          abs(arch_end[1] + 6.95) < 0.001,
          f"start={arch_start}, end={arch_end}")

    # The right-hand service cavity is intentionally inaccessible and must be
    # sealed from both the tank side and the hall-facing return.
    right_side = boxes["HeroTank_RightSideWall"]
    right_return = boxes["HeroHall_NorthEastReturn"]
    check("hero_tank_right_service_void_sealed",
          right_side["minimum"][2] <= 8.0 and
          right_side["maximum"][2] >= 16.0 and
          right_return["minimum"][0] <= 9.20 and
          right_return["maximum"][0] >= 11.45 and
          abs(right_return["maximum"][1] -
              (floors["Upper_RightArm"]["floor_y"] - 0.18)) < 0.001,
          "opaque side wall plus return ending at the 2F slab underside")

    check("blackout_gallery_portal_visually_sealed",
          boxes["Blackout_GalleryWestJamb"]["maximum"][2] >= 16.24 and
          boxes["Blackout_GalleryEastJamb"]["maximum"][2] >= 16.24 and
          features["blackout_gallery_portal"]["visual_header_bottom"] <= 1.75 and
          boxes["Blackout_GalleryHeaderClearance"]["minimum"][1] >= 2.049 and
          boxes["Blackout_TankOcclusionWing"]["minimum"][2] <= 14.01 and
          boxes["Blackout_TankOcclusionWing"]["maximum"][2] >= 15.99,
          "portal frame plus two-metre tank-side occlusion return")
    check("arch_connector_shoulders_closed",
          boxes["ArchConnector_WestReturn"]["minimum"][0] <= -13.09 and
          boxes["ArchConnector_WestReturn"]["maximum"][0] >= -11.51 and
          boxes["ArchConnector_EastReturn"]["minimum"][0] <= -8.59 and
          boxes["ArchConnector_EastReturn"]["maximum"][0] >= -7.01,
          "both width-change shoulders span wall-to-portal")
    check("upper_blue_wayfinding_cards_removed",
          not any(name.startswith("Upper_WallLight_") or
                  name.startswith("Upper_OverlookLight_") for name in names),
          "no loose low-mounted emissive rectangles")

    check("side_glass_has_matching_water_layer",
          features["side_glass_stack"]["layers"] ==
          ["WatatsumiGlass", "WatatsumiWater"],
          "side acrylic uses the same glass-plus-water stack as the front")

    # Audit the real solid volumes, not only intended dimensions in features.
    intrusions=[]
    for name in required_floors:
        if not name.startswith("Upper_"): continue
        f=floors[name]
        for b in data["boxes"]:
            mn,mx=b["minimum"],b["maximum"]
            if (mn[1] < f["floor_y"]+2.1 and mx[1] > f["floor_y"]+.05 and
                mn[0] < f["maximum_x"]-.4 and mx[0] > f["minimum_x"]+.4 and
                mn[2] < f["maximum_z"]-.4 and mx[2] > f["minimum_z"]+.4):
                intrusions.append((name,b["name"]))
    check("upper_decks_no_interior_solid_walls",not intrusions,str(intrusions))
    check("ramp_stops_before_underwater_arch",
          max(p[2] for p in points)+path["half_width"]+.2 < arch_start[2]-2,
          "ramp shell stops before water surface leading edge z=23")
    for room in ("Management","Reef"):
        bridge=floors["V4_"+room+"Bridge"]
        under=[p for p in points if p[0]>17 and
               bridge["minimum_z"]-1 < p[2] < bridge["maximum_z"]+1]
        clearance=min(bridge["floor_y"]-.18-p[1]-4 for p in under)
        check(room+"_bridge_clears_ramp",clearance>1,
              f"minimum structural clearance {clearance:.2f} m")
    result = {"passed": all(item["passed"] for item in checks), "checks": checks}
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    return result


if __name__ == "__main__":
    result = run()
    print(json.dumps(result, ensure_ascii=False))
    raise SystemExit(0 if result["passed"] else 1)
